/*
 * Author: Wei Yang 
 * SLAC National Accelerator Laboratory / Stanford University, 2020
 */

using namespace std;

#include <stdio.h>
#include <string>
#include <errno.h>
#include <netdb.h>
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
    string myName;
    struct cacheOptions cacheOpts;
    XrdSysError *eDest;
    bool isCmsd;
};

XrdOucName2NameXcacheH::XrdOucName2NameXcacheH(XrdSysError* erp, const char* confg, const char* parms)
{
    std::string myProg;
    std::string opts, message, key, value;
    std::string::iterator it;
    std::size_t i;
    int unit, x;
    char unitName;
    char *hostName;

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

    // the default
    cacheOpts.lifeT = 3600;
    cacheOpts.blockSize = 1048576;
    cacheOpts.xrdPort = std::stoi(getenv("XRDPORT"));

    hostName = (char*)malloc(256);
    gethostname(hostName, 256);
    struct hostent *myHostEnt = gethostbyname(hostName);
    cacheOpts.hostName = myHostEnt->h_name;
    free(hostName);

    opts = parms;
    opts += " ";
    for (it=opts.begin(); it!=opts.end(); ++it)
    {
        if (*it == '=') x = 1;
        else if (*it == ' ') 
        { 
            if (key == "cacheLife")  // unit: s/S (default), m/M, h/H, d/D, 
            {
                unit = 1;
                if (value.find_first_not_of("0123456789.") != std::string::npos)
                {
                    unitName = value.substr(value.find_first_not_of("0123456789."), 1).c_str()[0];
                    if (unitName == 's' || unitName == 'S') 
                        unit = 1;
                    else if (unitName == 'm' || unitName == 'M')
                        unit = 60;
                    else if (unitName == 'h' || unitName == 'H')
                        unit = 3600;
                    else if (unitName == 'd' || unitName == 'D')
                        unit = 86400;
                    else
                    {
                        message = myName + " Init: option cacheLife = "
                                  + value
                                  + " is invalid";
                        eDest->Say(message.c_str());
                    }

                    value.replace(value.find_first_not_of("0123456789."), 
                                  value.length() - value.find_first_not_of("0123456789."), 
                                  "");
                }

                if (value.find_first_not_of("0123456789.") == std::string::npos)
                {
                    cacheOpts.lifeT = atoi(value.c_str()) * unit;
                }
                else
                {
                    message = myName + " Init: option cacheLife = "
                                     + value
                                     + " is invalid. Using default ("
                                     + std::to_string(cacheOpts.lifeT)
                                     + " seconds)";
                    eDest->Say(message.c_str());
                }
            }
            else if (key == "cacheBlockSize") // unit: b/B (default), k/K, m/M
            {
                unit = 1;
                if (value.find_first_not_of("0123456789.") != std::string::npos)
                {
                    unitName = value.substr(value.find_first_not_of("0123456789."), 1).c_str()[0];
                    if (unitName == 'b' || unitName == 'B')
                        unit = 1;
                    else if (unitName == 'k' || unitName == 'K')
                        unit = 1024;
                    else if (unitName == 'm' || unitName == 'M')
                        unit = 1048576;
                    else
                    {
                        message = myName + " Init: option cacheBlockSize = "
                                  + value
                                  + " is invalid";
                        eDest->Say(message.c_str());
                    }

                    value.replace(value.find_first_not_of("0123456789."), 
                                  value.length() - value.find_first_not_of("0123456789."), 
                                  "");
                }

                if (value.find_first_not_of("0123456789.") == std::string::npos)
                {
                    cacheOpts.blockSize = atoi(value.c_str()) * unit;
                }
                else
                {
                    message = myName + " Init: option cacheBlockSize = " 
                                     + value
                                     + " is invalid. Using default ("
                                     + std::to_string(cacheOpts.blockSize)
                                     + " bytes)";
                    eDest->Say(message.c_str());
                }
            }
            else if (key == "xrdPort") 
            {
                if (value.find_first_not_of("0123456789.") == std::string::npos)
                {
                    cacheOpts.xrdPort = atoi(value.c_str());
                }
                else
                {
                    message = myName + " Init: option xrdPort = "
                                     + value
                                     + " is invalid";
                    eDest->Say(message.c_str());
                }
            }
            else if (key == "hostName") // my hostName to be used by sparse stagein request
            {
                cacheOpts.hostName = value;
            }
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

    message = myName + " Init: effective option cacheLife = " + std::to_string(cacheOpts.lifeT);
    eDest->Say(message.c_str());
    message = myName + " Init: effective option cacheBlockSize = " + std::to_string(cacheOpts.blockSize);
    eDest->Say(message.c_str());
    message = myName + " Init: effective option hostName:xrdPort = " + cacheOpts.hostName 
                                                                     + ":"
                                                                     + std::to_string(cacheOpts.xrdPort);
    eDest->Say(message.c_str());


    XcacheHInit(eDest, myName, &cacheOpts);
}

int XrdOucName2NameXcacheH::lfn2pfn(const char* lfn, char* buff, int blen)
{ return -EOPNOTSUPP; }

// when "pss.namelib -lfncachesrc ..." is used, pfn will look like:
// /images/junk1?src=http://u25@wt2.slac.stanford.edu/ 
// when "pss.namelib -lfncachesrc+ ..." is used, pfn will look like:
// /images/junk1?src=http://u25@wt2.slac.stanford.edu&
// /images/junk1?src=http://u25@wt2.slac.stanford.edu&mycgi=hello&his=none
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

    std::string myPath, myProt, myHostPort, myCGI;

    // it is important to use string::rfind() to search from the end. <-- why?
    myPath = myUrl.substr(0, myUrl.find("?src="));
    myUrl.replace(0, myPath.length() +5, "");
    // sometime the myPath doesn't start with a / (e.g. if the incoming is via the root protocol)
    if (myPath.c_str()[0] != '/')
        myPath = "/" + myPath;

    myProt = myUrl.substr(0, myUrl.find("://") +3);
    myUrl.replace(0, myProt.length(), "");

    // found a @ before the first '/' and '&'
    if (myUrl.find("@") != std::string::npos && 
        (myUrl.find("@") < myUrl.find("/") || myUrl.find("@") < myUrl.find("&")))
    {
        myUrl.replace(0, myUrl.find("@") +1, "");
    }
    if (myUrl.find("&") != std::string::npos) // test '&'
       myHostPort = myUrl.substr(0, myUrl.find("&")); 
    else  // no CGI
       myHostPort = myUrl;
    myUrl.replace(0, myHostPort.length(), "");
    // remove trailing "/"
    if (myHostPort.find("/") == (myHostPort.length() -1)) 
        myHostPort.replace(myHostPort.length() -1, 1, "");

    myCGI = myUrl;  // note this CGI, if not empty, starts with a "&"

    std::string stageinToken = "xcachestagein";;
    int stageinRequest = 0;

    if (myCGI.find(stageinToken + "=") != std::string::npos) // This is a stage in request
    {
        stageinRequest = 1;
        myCGI.replace(myCGI.find(stageinToken + "="), stageinToken.length() +1, "");
    }
    else if (myCGI.find(stageinToken) != std::string::npos) // This is a stage in request
    {
        stageinRequest = 1;
        myCGI.replace(myCGI.find(stageinToken), stageinToken.length(), "");
    }

    // trim the trailing ? or &
    while ( myCGI.length() != 0 &&
            ((myCGI.rfind("?") == (myCGI.length() -1)) ||
             (myCGI.rfind("&") == (myCGI.length() -1))) )
        myCGI.replace(myCGI.length() -1, 1, "");

    // trim ?& to ?
    while ( myCGI.length() != 0 && myCGI.rfind("?&") != std::string::npos )
        myCGI.replace(myCGI.rfind("?&") +1, 1, "");

    // trim ?& to &
    while ( myCGI.length() != 0 && myCGI.rfind("&&") != std::string::npos )
        myCGI.replace(myCGI.rfind("&&") +1, 1, "");

    if (myCGI.length() == 0)
        myUrl = myProt + myHostPort + myPath;
    else
        myUrl = myProt + myHostPort + myPath + myCGI.replace(0, 1, "?");

    // this scenarios should NOT happen
    if (myUrl.find("http://") != 0 && myUrl.find("https://") != 0)
    { 
        blen = 0;
        buff[0] = 0;
        return EINVAL; // see XrdOucName2Name.hh
    }

    myLfn = XcacheHCheckFile(myUrl, stageinRequest);  

    if (myLfn == "EFAULT")
        return EFAULT;
    else if (myLfn == "ENOENT") 
        return ENOENT;
    else if (myLfn == "EALREADY")
        return EALREADY;

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
