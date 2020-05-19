/*
 * Author: Wei Yang 
 * SLAC National Accelerator Laboratory / Stanford University, 2020
 */

#include "XrdSys/XrdSysError.hh"

void XcacheHInit(XrdSysError* eDest, const std::string myName, time_t cacheLifeT);
std::string XcacheHCheckFile(XrdSysError* eDest,
                             const std::string myName,
                             const std::string myPfn);
