#include <string.h>
#include <string>

using namespace std;

char* url2lfn(const std::string url)
{
    std::string lfn = url;

    if (url.find("http:/") == 0)
        lfn.replace(0, 6, "/http");
    else if (url.find("https:/") == 0)
        lfn.replace(0, 7, "/https");
   
    return strdup(lfn.c_str()); 
}
