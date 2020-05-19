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
#include <string>
#include <thread>
#include "url2lfn.hh"
#include "cacheFileOpr.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdSys/XrdSysError.hh"

struct httpResp 
{
    char *data;
    size_t size;
};

void cleaner()
{
    std::string cmd = "yes";
    while (! sleep(600))
    {
        system(cmd.c_str());
    }
}

time_t cacheLifeTime;

static int XcacheH_DBG = 1;

void XcacheHInit(XrdSysError* eDest,
                 const std::string myName, 
                 time_t cacheLifeT)
{
     cacheLifeTime = cacheLifeT;

     // std::thread cleanning(cleaner);
     // cleanning.detach();
     curl_global_init(CURL_GLOBAL_ALL);

     if (getenv("XcacheH_DBG") != NULL) XcacheH_DBG = atoi(getenv("XcacheH_DBG"));
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

// Return
// 1: yes file need to be fetched again.
// 0: no data source hasn't changed yet.
//
int NeedRefetch_HTTP(std::string myPfn, time_t mTime)
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
    res = curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);
   
    // check for errors
    int rc = 0;
    if(res == CURLE_OK)
    {
        // Two possible reponse HTTP/1.1 304 Not Modified or HTTP/1.1 200 OK (modified)
        // If http redirection happens, these two will be the last of HTTP/1.1 response 
        // in the chunk.data stream.
        if (strcasestr(chunk.data, "HTTP/1.1 200 OK") != NULL) 
            rc = 1;
        // else if (strcasestr(chunk.data, "HTTP/1.1 304 Not Modified") != NULL) 
        //     rc = 0;
    }
    free(chunk.data);
    free(rmturl);
    return rc;
}


std::string XcacheHCheckFile(XrdSysError* eDest, 
                             const std::string myName, 
                             const std::string myPfn)
{
    std::string rmtUrl, myLfn, msg;

    myLfn = url2lfn(myPfn);

    time_t mTime = cacheFileAtime(myPfn);
    time_t currTime = time(NULL);

    if ((currTime - mTime) > cacheLifeTime)
    {
        if (myPfn.find("http") == 0) // http or https protocol
            if (NeedRefetch_HTTP(myPfn, mTime) == 1)
            {
                if (cacheFilePurge(myPfn) == 0)
                    msg = myName + ": purge " + myLfn; 
                else
                    msg = myName + ": fail to purge " + myLfn;
            }
            else // no need to refetch, or data soruce is not available
                msg = myName + ": no need to refetch or data source no available" + myLfn;
        else if (myPfn.find("root") == 0) //
            msg = myName + ": dont not know what to do " + myLfn;
        if (XcacheH_DBG != 0) eDest->Say(msg.c_str()); 
    }
    else 
    {
        msg = myName + ": recently cached " + myLfn;
        if (XcacheH_DBG != 0) eDest->Say(msg.c_str());
    }

    return myLfn;  
}
