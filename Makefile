CXX = g++
CPPFLAGS = -O0 -Wformat -ggdb3
PRODFLAGS = -O3 -g0

GSTCONFIG = `pkg-config --cflags --libs gstreamer-1.0`
GSTGESCONFIG = `pkg-config --cflags --libs gstreamer-1.0 gst-editing-services-1.0`
GSTGETBASECONFIG = `pkg-config --cflags --libs gstreamer-1.0 gstreamer-base-1.0`

CXXFLAGS = -Wno-psabi  
LDFLAGS = 
#INCLUDE = myplugins/NemaTode/include

#HELPERSRC = gstreamEdits.cpp gstreamBin.cpp gstreamPipeline.cpp
HELPERSRC = gstreamBin.cpp gstreamPipeline.cpp myplugins/gstmybin.c
HELPEROBJ = $(HELPERSRC:.cc=.o)
#HELPERAR = gstreamEdits.o gstreamBin.o gstreamPipeline.o
HELPERAR = gstreamBin.o gstreamPipeline.o
HELPERLIB = libgstreamHelpers.a
HELPERHEADERS = gstreamPipeline.h gstreamEdits.h gstreamBin.h

MYPLUGINSSRC = $(wildcard ./myplugins/*.c)
MYPLUGINSHEAD = $(wildcard ./myplugins/*.h)
MYPLUGINSAR = gstjsoninject.o gstjsontopango.o gstnmeasource.o nmealoop.o gstmybin.o gstjsonparse.o gstptsnormalise.o gstbarrier.o
MYPLUGINSOBJ = $(MYPLUGINSSRC:.cc=.o)
MYPLUGINSLIB = libmyplugins.a

NMEASRC = myplugins/NemaTode/src/GPSFix.cpp myplugins/NemaTode/src/GPSService.cpp myplugins/NemaTode/src/NMEACommand.cpp myplugins/NemaTode/src/NMEAParser.cpp myplugins/NemaTode/src/NumberConversion.cpp
NMEAOBJ = $(NMEASRC:.cc=.o)
NMEALIB = libnematode.a
NMEACXXFLAGS = -Wno-psabi
NMEAINCLUDE = myplugins/NemaTode/include

RPICAMLIB = myplugins/gst-rpicamsrc/src/.libs/libgstrpicamsrc.so


all: helperlib myplugins nmealib

helperlib: $(HELPERLIB)
myplugins: $(MYPLUGINSLIB)
nmealib: $(NMEALIB)

# ld only looks in ar files once, in the order they're quoted, and only uses symbols in it that are unfulfilled *at the point it opens the ar*
$(HELPERLIB): $(HELPEROBJ) $(HELPERHEADERS)
#	$(CXX) $(LDFLAGS) -o $@ $(OBJ) $(MYPLUGINSLIB) $(NMEALIB) $(CPPFLAGS) $(CXXFLAGS) $(GSTGESCONFIG) -I $(NMEAINCLUDE)
	$(CXX) $(LDFLAGS) -c $(HELPEROBJ) $(CPPFLAGS) $(CXXFLAGS) $(GSTGETBASECONFIG)
	ar rvs $(HELPERLIB) $(HELPERAR)

$(NMEALIB) : $(NMEAOBJ)
	$(CXX) $(LDFLAGS) -c $(NMEAOBJ) $(CPPFLAGS) $(NMEACXXFLAGS) -I $(NMEAINCLUDE)
	ar rvs $(NMEALIB) GPSFix.o GPSService.o NMEACommand.o NMEAParser.o NumberConversion.o

$(MYPLUGINSLIB) : $(MYPLUGINSOBJ) $(MYPLUGINSHEAD)
	$(CXX) $(LDFLAGS) -c $(MYPLUGINSOBJ) $(CPPFLAGS) $(CXXFLAGS) $(GSTCONFIG) -I $(NMEAINCLUDE)
	ar rvs $(MYPLUGINSLIB) $(MYPLUGINSAR)

$(RPICAMLIB):
	cd myplugins/gst-rpicamsrc && ./autogen.sh --prefix=/usr --libdir=/usr/lib/arm-linux-gnueabihf/ && $(MAKE) && sudo $(MAKE) install

clean:
	rm -rf *.o *.a

