/*
 * Author: Wei Yang 
 * SLAC National Accelerator Laboratory / Stanford University, 2020
 */

#include "XrdSys/XrdSysError.hh"

struct cacheOptions
{
    time_t lifeT;
    size_t blockSize; 
    int    xrdPort;
    std::string hostName;
};

void XcacheHInit(XrdSysError* eDest, const std::string myName, struct cacheOptions *cacheOpt);
std::string XcacheHCheckFile(const std::string myPfn, int stageinRequest);
