/*
 * Author: Wei Yang (SLAC National Accelerator Laboratory / Stanford University, 2019)
 */

#include <time.h>

// url is in the form or /http:/host... or /https:/host
time_t cacheFileAtime(std::string url);

// return 0 if file is purged, !0 if not
int cacheFilePurge(std::string url);
