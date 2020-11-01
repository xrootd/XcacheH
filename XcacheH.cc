/*
 * Author: Wei Yang 
 * SLAC National Accelerator Laboratory / Stanford University, 2020
 */

using namespace std;

#include <fcntl.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <openssl/md5.h>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>

#include <mutex>
#include <list>

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

#include "url2lfn.hh"
#include "cacheFileOpr.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdSys/XrdSysError.hh"

struct httpResp 
{
    char *data;
    size_t size;
};

#define MAXSTAGINWORKER 5
int currStagingWorkers = 0;
std::list<std::string> stageinList;
std::mutex stageinMutex;

void addToStageinList(std::string myPfn)
{
    int inList = 0;
    std::list<std::string>::iterator it;

    std::lock_guard<std::mutex> guard(stageinMutex);
    for (it = stageinList.begin(); it != stageinList.end(); ++it)
    {
        if (*it == myPfn)
        {
            inList = 1;
            break;
        }
    }
    if (! inList) stageinList.push_back(myPfn); 
}

void stageinWorker(std::string myPfn)
{
    std::string xrdcp = "xrdcp -s -f root://griddev06.slac.stanford.edu:1094//";
    std::string xrdcpCmd;
    xrdcpCmd = xrdcp + myPfn + " /dev/null";

    // To do: check again if the file is fully cached.
    system(xrdcpCmd.c_str());
    std::lock_guard<std::mutex> guard(stageinMutex);
    currStagingWorkers--; 
}

void stageinOpr()
{
    while (! sleep(1))
    {
        { // lock will be released when going out of the scope
            std::lock_guard<std::mutex> guard(stageinMutex);
            for (int i = currStagingWorkers; i < MAXSTAGINWORKER; i++)
            {
                if (stageinList.size() > 0)
                {
                    currStagingWorkers++;
                    std::string url = stageinList.front();
                    stageinList.pop_front();
                    std::thread newStageinWorker(stageinWorker, url);
                    newStageinWorker.detach();
                }
                else
                    break;
            }
        }
    }
}    

time_t cacheLifeTime;
std::string myX509proxyFile;
std::string CApath;

static int XcacheH_DBG = 1;

void XcacheHInit(XrdSysError* eDest,
                 const std::string myName, 
                 time_t cacheLifeT)
{
    cacheLifeTime = cacheLifeT;

    std::thread stageinThread(stageinOpr);
    stageinThread.detach();
    curl_global_init(CURL_GLOBAL_ALL);

    if (getenv("X509_USER_PROXY") != NULL)
        myX509proxyFile = getenv("X509_USER_PROXY");
    else
        myX509proxyFile = "/tmp/x509up_u" + std::to_string(geteuid());

    if (getenv("X509_CERT_DIR") != NULL)
        CApath = getenv("X509_CERT_DIR");
    else
        CApath = "/etc/grid-security/certificates";

    if (getenv("XcacheH_DBG") != NULL) XcacheH_DBG = atoi(getenv("XcacheH_DBG"));
}

// Return:
// 1: load successfully
// 0: fail to load user x509 proxy
int loadFromUserX509Proxy(char* cert, char* key)
{
    std::string myCert, myKey, line;
    int inCert, inKey;

    inCert = inKey = 0; 
    myCert = myKey = "";
    ifstream x509Proxy(myX509proxyFile.c_str());
    if (x509Proxy.is_open())
    {
        while (x509Proxy.good() && std::getline(x509Proxy, line))
        {
            cout << line;
            if (inCert || line.find("--BEGIN CERTIFICATE--") != std::string::npos)
            {
                inCert = 1;
                myCert += line;
            }
            if (line.find("--END CERTIFICATE--") != std::string::npos)
                inCert = 0;
            if (inKey || line.find("--BEGIN RSA PRIVATE KEY--") != std::string::npos)
            {
                inKey = 1;
                myKey += line;
            }
            if (line.find("--END RSA PRIVATE KEY--") != std::string::npos)
            {
                cert = strdup(myCert.c_str());
                key = strdup(myKey.c_str());
                x509Proxy.close();
                return 1;
            }
        }
        x509Proxy.close();
    }
    return 0;
     
}

std::mutex x509proxyLock;
char *userX509cert, *userX509key;
CURLcode sslCtxCallBack(CURL *curl, void *sslctx, void *parm)
{
    static time_t tLastReadX509Proxy = -1;
    X509 *cert = NULL;
    BIO *bio = NULL;
    BIO *kbio = NULL;
    RSA *rsa = NULL;
    char *mycert, *mykey;
    int ret;

    { // this is the scope where mylock will work
        const std::lock_guard<std::mutex> mylock(x509proxyLock); 
        if ((time(NULL) - tLastReadX509Proxy) > 60) {
            tLastReadX509Proxy = time(NULL);
            if (userX509cert != NULL) 
                free(userX509cert);
            if (userX509key != NULL)
                free(userX509key);
            if (! loadFromUserX509Proxy(userX509cert, userX509key))
                return CURLE_SSL_CERTPROBLEM;
            cout << userX509cert;
        }
        mycert = strdup(userX509cert);
        mykey = strdup(userX509key);
    } // x509proxyLock is automatically released when lock goes out of scope
 
    (void)curl; // avoid warnings
    (void)parm; // avoid warnings
 
    // get a BIO
    bio = BIO_new_mem_buf((char *)mycert, -1);
    if (bio == NULL) 
        return CURLE_SSL_CERTPROBLEM; 
 
    // use it to read the PEM formatted certificate from memory into an X509
    // structure that SSL can use
    cert = PEM_read_bio_X509(bio, NULL, 0, NULL);
    if (cert == NULL)
        return CURLE_SSL_CERTPROBLEM; 
 
    // tell SSL to use the X509 certificate
    ret = SSL_CTX_use_certificate((SSL_CTX*)sslctx, cert);
    if (ret != 1)
        return CURLE_SSL_CERTPROBLEM; 
 
    // create a bio for the RSA key
    kbio = BIO_new_mem_buf((char *)mykey, -1);
    if (kbio == NULL)
        return CURLE_SSL_CERTPROBLEM; 
 
    // read the key bio into an RSA object
    rsa = PEM_read_bio_RSAPrivateKey(kbio, NULL, 0, NULL);
    if (rsa == NULL)
        return CURLE_SSL_CERTPROBLEM; 
 
    //*tell SSL to use the RSA key from memory 
    ret = SSL_CTX_use_RSAPrivateKey((SSL_CTX*)sslctx, rsa);
    if (ret != 1)
        return CURLE_SSL_CERTPROBLEM; 
 
    // free resources that have been allocated by openssl functions
    if (bio)
        BIO_free(bio);
    if (kbio)
        BIO_free(kbio);
    if (rsa)
        RSA_free(rsa);
    if (cert)
        X509_free(cert);
    if (mycert)
        free(mycert);
    if (mykey)
        free(mykey);
 
    // all set to go
    return CURLE_OK;
}

static size_t XcacheHRemoteStatCallback(void *contents, 
                                        size_t size, 
                                        size_t nmemb, 
                                        void *userp)
{
    size_t realsize = size * nmemb;
    struct httpResp *mem = (struct httpResp *)userp;
 
    mem->data = (char*)realloc(mem->data, mem->size + realsize + 1);
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    return realsize;
}

#define NeedRefetch_HTTP NeedRefetch_HTTP_curl

// Return
// 0: data source hasn't changed yet.
// 1: yes file need to be fetched again.
// 2: checking was not successful.
//
int NeedRefetch_HTTP_curl(std::string myPfn, time_t mTime)
{
    char* rmturl = strdup(myPfn.c_str());

    struct httpResp chunk;
    CURL *curl_handle;
    CURLcode res;

    chunk.data = (char*)malloc(1);  // will be grown as needed by the realloc above 
    chunk.size = 0;    // no data at this point  
       
    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1); // to make it thread-safe?
    curl_easy_setopt(curl_handle, CURLOPT_URL, rmturl);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);  // the curl -k option
    curl_easy_setopt(curl_handle, CURLOPT_HEADER, 1);  // http header
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, XcacheHRemoteStatCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 180L);

    // If-Mod-Since
    curl_easy_setopt(curl_handle, CURLOPT_TIMEVALUE, (long)mTime);
    curl_easy_setopt(curl_handle, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);

    // Header only, ask the server not to send body data
    curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1L);

    // Follow redirection
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5L);
       
    // some servers don't like requests that are made without a user-agent
    // field, so we provide one 
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // try without X509
    res = curl_easy_perform(curl_handle);
 
    // check for errors, set rc = 2: can not check
    int rc = 2;
    if (res == CURLE_OK)
    {
        if (strcasestr(chunk.data, "HTTP/1.1 403 Forbidden") != NULL)
        { // try with X509
            free(chunk.data);
            chunk.data = (char*)malloc(1);
            chunk.size = 0; 
            // this only work with libcurl/OpenSSL. CentOS 7 default is libcurl/NSS
            //             // Untested:
            //curl_easy_setopt(curl_handle, CURLOPT_SSL_CTX_FUNCTION, sslCtxCallBack);    

            curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 1L);

            curl_easy_setopt(curl_handle, CURLOPT_SSLCERTTYPE, "PEM");
            curl_easy_setopt(curl_handle, CURLOPT_SSLKEYTYPE, "PEM");

            curl_easy_setopt(curl_handle, CURLOPT_SSLCERT, myX509proxyFile.c_str());
            curl_easy_setopt(curl_handle, CURLOPT_SSLKEY, myX509proxyFile.c_str());
            curl_easy_setopt(curl_handle, CURLOPT_CAINFO, myX509proxyFile.c_str());
            curl_easy_setopt(curl_handle, CURLOPT_CAPATH, CApath.c_str());

            res = curl_easy_perform(curl_handle);
        }

        if (res == CURLE_OK) 
        {
            // Two possible reponse HTTP/1.1 304 Not Modified or HTTP/1.1 200 OK (modified)
            // If http redirection happens, these two will be the last of HTTP/1.1 response 
            // in the chunk.data stream.
            char *c = strcasestr(chunk.data, "HTTP/1.1 200 OK");
            if (c != NULL) 
            {
                rc = 1;
                /* 
                 * Can't do the following - http://cvmfs.sdcc.bnl.gov:8000/cvmfs/atlas.sdcc.bnl.gov/.cvmfspublished
                 * will always return "HTTP/1.1 200 OK" with an "Expires: in the future, regardless of whether the
                 * file was changed after "If-Modified-Since:". It also doesn't provide a "Last-Modified:" field.
                 *
                // search for something like "Expires: Fri, 05 Jun 2020 04:09:49 GMT\r\n..."
                // the expiration date could be a future date
                c = strcasestr(c, "Expires: ");
                if (c != NULL) 
                {
                    c += 9;
                    // according to https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Date
                    // http-date is always in GMT
                    c = strndup(c, 29);
                    struct tm expireTm;
                    strptime(c, "%a, %d %b %Y %T", &expireTm);
                    time_t expireT = mktime(&expireTm) + expireTm.tm_gmtoff;
                    rc = ((expireT > mTime)? 0 : 1); 
                    free(c);
                }
                */
            }
            else if (strcasestr(chunk.data, "HTTP/1.1 304 Not Modified") != NULL) 
                rc = 0;
        }
    }

    curl_easy_cleanup(curl_handle);

    free(chunk.data);
    free(rmturl);
    return rc;
}

// to be implemented
int NeedRefetch_ROOT(std::string myPfn, time_t mTime)  {}

// Return
// 0: data source hasn't changed yet.
// 1: yes file need to be fetched again.
//
/*
 * All works except fileInfo.mtime (or .st_mtime) is not filled
 * Should check with the Davix team.
 *
#include <davix/davix.hpp>
#include <davix/file/davfile.hpp>

int NeedRefetch_HTTP_Davix(std::string myPfn, time_t mTime)
{
    Davix::Uri uri(myPfn);
    Davix::Context ctx;   
    Davix::DavFile file(ctx, uri);
    Davix::RequestParams reqParams;
    reqParams.setUserAgent("libdavix/0.7.6 neon/0.0.29"); 

    //Davix::StatInfo fileInfo;
    //file.statInfo(&reqParams, fileInfo);
    //if (mTime < fileInfo.mtime)
    //    return 0;
    //else
    //    return 1;

    Davix::DavixError* err = NULL;
    struct stat fileInfo;
    file.stat(&reqParams, &fileInfo, &err);
    if (mTime < fileInfo.st_mtime)
        return 0;
    else
        return 1;
}
*/

std::string XcacheHCheckFile(XrdSysError* eDest, 
                             const std::string myName, 
                             const std::string myPfn,
                             int stageinRequest)
{
    std::string rmtUrl, myLfn, msg;
    struct stat myStat;
    int rc;

    myLfn = url2lfn(myPfn);

    rc = cacheFileQuery(myPfn);

    if (rc <= 0 && stageinRequest == 1)
    {
        addToStageinList(myPfn);
    }
    else 
    {
        if (rc < 0) return myLfn; // new file, nothing to check
    }

    myStat.st_mtime = myStat.st_atime = 0;
    rc = cacheFileStat(myPfn, &myStat);

    time_t currTime = time(NULL);

    // this cache entry isn't in use or used recently. 60 is too short, for testing only!
    if ((currTime - myStat.st_atime) > cacheLifeTime)
    {
        if (myPfn.find("http") == 0) // http or https protocol
        {
            rc = NeedRefetch_HTTP(myPfn, myStat.st_mtime);
            if (rc == 0) 
                msg = "no need to refetch!";
            else if (rc == 1)
            {
                rc = cacheFilePurge(myPfn);
                if (rc == 0)
                    msg = "purge"; 
                else if (rc == -EBUSY)  // see XrdPosixCache.hh (check ::Unlink())
                    msg = "not purge, in use!";
                else if (rc == -EAGAIN)
                    msg = "not purge, file subject to internal processing!";
                else if (rc == -errno)
                    msg = "fail to purge";
            }
            else // rc = 2
                msg = "data source no available!";
        }
        else if (myPfn.find("root") == 0)
        {
            rc = NeedRefetch_ROOT(myPfn, myStat.st_mtime);
            msg = "checking and purging are not implemented!";
        }
    }
    else 
        msg = "not purge - likely in use!";

    msg = myName + ": " + msg + " " + myLfn;
    if (XcacheH_DBG != 0) eDest->Say(msg.c_str()); 

    return myLfn;  
}
