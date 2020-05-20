/*
 * Author: Wei Yang (SLAC National Accelerator Laboratory / Stanford University, 2019)
 */

#include <time.h>

// url is in the form or /http:/host... or /https:/host
int cacheFileStat(std::string url, struct stat *myStat);

// return 0 if file is purged, !0 if not
int cacheFilePurge(std::string url);
