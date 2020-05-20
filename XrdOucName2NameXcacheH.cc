/*
 * Author: Wei Yang 
 * SLAC National Accelerator Laboratory / Stanford University, 2020
 */

using namespace std;

#include <stdio.h>
#include <string>
#include <errno.h>
#include <openssl/md5.h>
#include "XrdVersion.hh"
XrdVERSIONINFO(XrdOucgetName2Name, "N2N-XcacheH");

#include "XcacheH.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysError.hh"

class XrdOucName2NameXcacheH : public XrdOucName2Name
{
public:
    virtual int lfn2pfn(const char* lfn, char* buff, int blen);
    virtual int lfn2rfn(const char* lfn, char* buff, int blen);
    virtual int pfn2lfn(const char* lfn, char* buff, int blen);

    XrdOucName2NameXcacheH(XrdSysError *erp, const char* confg, const char* parms);
    virtual ~XrdOucName2NameXcacheH() {};

    friend XrdOucName2Name *XrdOucgetName2Name(XrdOucgetName2NameArgs);
private:
    string myName, optCacheLife = ""; // unit: seconds
    time_t cacheLifeT;
    XrdSysError *eDest;
    bool isCmsd;
};

XrdOucName2NameXcacheH::XrdOucName2NameXcacheH(XrdSysError* erp, const char* confg, const char* parms)
{
    std::string myProg;
    std::string opts, message, key, value;
    std::string::iterator it;
    std::size_t i;
    int x;

    myName = "XcacheH";
    eDest = erp;
    
    isCmsd = false;
    if (getenv("XRDPROG")) 
    {
        myProg = getenv("XRDPROG");
        if (myProg == "cmsd") isCmsd = true;
    } 

    setenv("XRD_METALINKPROCESSING", "1", 0);
    setenv("XRD_LOCALMETALINKFILE", "1", 0);

    x = 0;
    key = "";
    value = "";

    opts = parms;
    opts += " ";
    for (it=opts.begin(); it!=opts.end(); ++it)
    {
        if (*it == '=') x = 1;
        else if (*it == ' ') 
        { 
            if (key == "cacheLife")  // unit: seconds
                optCacheLife = value;
            key = "";
            value = "";  
            x = 0;
        }
        else
        {
            if (x == 0) key += *it;
            if (x == 1) value += *it;
        }
    }


    if (optCacheLife.find_first_not_of("0123456789.") == std::string::npos)
    {
        cacheLifeT = atoi(optCacheLife.c_str());
        message = myName + " Init: cacheLife = " + optCacheLife;
    }
    else
    {
        cacheLifeT = 3600;
        message = myName + " Init: cacheLife = " + optCacheLife + " is invalid or not set. Set it to 1 hour";
    }
    eDest->Say(message.c_str());

    XcacheHInit(eDest, myName, cacheLifeT);
}

int XrdOucName2NameXcacheH::lfn2pfn(const char* lfn, char* buff, int blen)
{ return -EOPNOTSUPP; }

// when "pss.namelib -lfncachesrc ..." is used, pfn will look 
// like /images/junk1?src=http://u25@wt2.slac.stanford.edu/ 
int XrdOucName2NameXcacheH::pfn2lfn(const char* pfn, char* buff, int blen) 
{
    std::string myLfn, myPfn, myUrl;

    myPfn = myUrl = pfn;
    if (isCmsd) // cmsd shouldn't do pfn2lfn()
    {
        blen = myPfn.find("?src="); 
        blen = strlen(pfn);
        strncpy(buff, pfn, blen);
        return 0;
    }

    // it is import to use string::rfind() to search from the end. 
    myUrl.replace(0, myUrl.rfind("?src=") +5, "");
    // myUrl.replace(myUrl.length() -1, 1, ""); // remove the tailing "/"

    // remove u25@ from the URL (see above)
    if (myUrl.find("http://") == 0)
        if (myUrl.find("@") != std::string::npos) myUrl.replace(0, myUrl.find("@") +1, "http://");
    else if (myUrl.find("https://") == 0)
        if (myUrl.find("@") != std::string::npos) myUrl.replace(0, myUrl.find("@") +1, "https://");
    else // this scenarios should NOT happen
    {
        blen = 0;
        buff[0] = 0;
        return EINVAL; // see XrdOucName2Name.hh
    }

    myUrl += myPfn.substr(0, myPfn.rfind("?src=")); 

    myLfn = XcacheHCheckFile(eDest, myName, myUrl);  

    if (myLfn == "EFAULT")
        return EFAULT;
    else if (myLfn == "ENOENT") 
        return ENOENT;

    blen = myLfn.length();
    strncpy(buff, myLfn.c_str(), blen);
    buff[blen] = 0;

    return 0;
}

int XrdOucName2NameXcacheH::lfn2rfn(const char* lfn, char* buff, int blen) 
{ return -EOPNOTSUPP; }

XrdOucName2Name *XrdOucgetName2Name(XrdOucgetName2NameArgs)
{
    static XrdOucName2NameXcacheH *inst = NULL;

    if (inst) return (XrdOucName2Name *)inst;

    inst = new XrdOucName2NameXcacheH(eDest, confg, parms);
    if (!inst) return NULL;

    return (XrdOucName2Name *)inst;
}
