/*
 * Author: Wei Yang (SLAC National Accelerator Laboratory / Stanford University, 2019)
 */

using namespace std;

#include <stdio.h>
#include <string.h>
#include <string>
#include <errno.h>
#include <iostream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "url2lfn.hh"
#include "XrdVersion.hh"
#include "XrdOuc/XrdOucCacheCM.hh"
#include "XrdPosix/XrdPosixCache.hh"

//______________________________________________________________________________


XrdPosixCache *myCache;

extern "C" {
XrdOucCacheCMInit_t XrdOucCacheCMInit(XrdPosixCache &Cache,
                                      XrdSysLogger  *Logger,
                                      const char    *Config,
                                      const char    *Parms,
                                      XrdOucEnv     *envP)
{
    myCache = &Cache;
}
};
XrdVERSIONINFO(XrdOucCacheCMInit,CacheCM-4-XcacheH);

time_t cacheFileAtime(std::string url)
{
    int rc;
    char *lfn = url2lfn(url);
    struct stat mystat; 

    rc = myCache->Stat(lfn, mystat);
    free(lfn);
     
    // if the file doesn't exist, return 0 as mtime (older than any real file)
    if (rc == 0)
        return mystat.st_atime;
    else 
        return 0;
}

int cacheFilePurge(std::string url)
{
    int rc;
    char *lfn = url2lfn(url);

    rc = myCache->Unlink(lfn); 
    free(lfn);
    return rc;
}
