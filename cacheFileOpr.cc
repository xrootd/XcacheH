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

int cacheFileStat(std::string url, struct stat* myStat)
{
    int rc;
    char *lfn = url2lfn(url);

    rc = myCache->Stat(lfn, *myStat);
    free(lfn);
    return rc;
}

int cacheFilePurge(std::string url)
{
    int rc;
    char *lfn = url2lfn(url);

    rc = myCache->Unlink(lfn); 
    free(lfn);
    return rc;
}

int cacheFileQuery(std::string url)
{
    int rc;
    char *lfn = url2lfn(url);
 
    rc = myCache->CacheQuery(lfn, true);
    free(lfn);
    return rc;
}
