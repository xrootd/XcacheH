#include <string.h>
#include <string>
#include <openssl/md5.h>

using namespace std;

void md5hash(const char *in, char out[MD5_DIGEST_LENGTH*2 +1])
{
    unsigned char hash[MD5_DIGEST_LENGTH];
    char *intmp = strdup(in);
    MD5_CTX md5;
    MD5_Init(&md5);
    MD5_Update(&md5, intmp, strlen(in));
    MD5_Final(hash, &md5);
    int i = 0;
    for(i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        sprintf(out + (i * 2), "%02x", hash[i]);
    }
    out[MD5_DIGEST_LENGTH*2] = 0;
    free(intmp);
}

char* url2lfn(const std::string url)
{
    std::string lfn, cgi;
    char cgiHash[MD5_DIGEST_LENGTH*2 +1]; 

    lfn = url;

    if (url.find("http:/") == 0)
        lfn.replace(0, 6, "/http");
    else if (url.find("https:/") == 0)
        lfn.replace(0, 7, "/https");
    else if (url.find("root:/") == 0)
        lfn.replace(0, 6, "/root"); 
    else if (url.find("xroot:/") == 0)
        lfn.replace(0, 7, "/root"); 
    else if (url.find("roots:/") == 0)
        lfn.replace(0, 7, "/roots"); 
    else if (url.find("xroots:/") == 0)
        lfn.replace(0, 8, "/roots"); 
    
    int i = lfn.find("?");
    if (i != std::string::npos)  // there is a CGI
    {
//        cgi = lfn;
//        lfn.replace(i, lfn.length()-i , "");
//        cgi = cgi.replace(0, i, "");
//        md5hash(cgi.c_str(), cgiHash);
//        lfn = lfn + "#" + cgiHash;
        lfn.replace(i, lfn.length(), "");
    }
    return strdup(lfn.c_str()); 
}
