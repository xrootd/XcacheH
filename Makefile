all: XrdName2NameXcacheH.so

# make sure xrootd-devel, xrootd-server-devel and xrootd-client-devel rpms 
# are installed for the needed xrootd header files.

ifeq ($(strip $(XRD_INC)),)
    XRD_INC=/usr/include/xrootd
endif

ifeq ($(strip $(XRD_LIB)),)
    XRD_LIB=/usr/lib64
endif

FLAGS=-D_REENTRANT -D_THREAD_SAFE -Wno-deprecated -std=c++0x #-I/usr/include/davix

HEADERS=cacheFileOpr.hh url2lfn.hh XcacheH.hh
SOURCES=XrdOucName2NameXcacheH.cc cacheFileOpr.cc url2lfn.cc XcacheH.cc
OBJECTS=XrdOucName2NameXcacheH.o cacheFileOpr.o url2lfn.o XcacheH.o

DEBUG=-g

XrdName2NameXcacheH.so: $(OBJECTS) Makefile
	g++ ${DEBUG} -shared -fPIC -o $@ $(OBJECTS) -L${XRD_LIB} -L${XRD_LIB}/XrdCl -ldl -lssl -lcurl -lXrdCl -lXrdPosix -lstdc++ #-ldavix

XrdOucName2NameXcacheH.o: XrdOucName2NameXcacheH.cc ${HEADERS} Makefile
	g++ ${DEBUG} ${FLAGS} -fPIC -I ${XRD_INC} -I ${XRD_LIB} -c -o $@ $<

cacheFileOpr.o: cacheFileOpr.cc ${HEADERS} Makefile
	g++ ${DEBUG} ${FLAGS} -fPIC -I ${XRD_INC} -I ${XRD_LIB} -c -o $@ $<

url2lfn.o: url2lfn.cc ${HEADERS} Makefile
	g++ ${DEBUG} ${FLAGS} -fPIC -I ${XRD_INC} -I ${XRD_LIB} -c -o $@ $<

XcacheH.o: XcacheH.cc ${HEADERS} Makefile
	g++ ${DEBUG} ${FLAGS} -fPIC -I ${XRD_INC} -I ${XRD_LIB} -c -o $@ $<

clean:
	rm -vf *.{o,so}


