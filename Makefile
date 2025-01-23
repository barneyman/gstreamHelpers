CXX = g++
CPPFLAGS = -O0 -Wformat -ggdb3
PRODFLAGS = -O3 -g0
LEAKFLAGS = -O0 -Wformat -ggdb3 -fsanitize=address -fno-omit-frame-pointer

GSTCONFIG = `pkg-config --cflags --libs gstreamer-1.0`
GSTGESCONFIG = `pkg-config --cflags --libs gstreamer-1.0 gst-editing-services-1.0`
GSTGETBASECONFIG = `pkg-config --cflags --libs gstreamer-1.0 gstreamer-base-1.0`

CXXFLAGS = -Wno-psabi  
LDFLAGS = 

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

all: helperlib myplugins 

leaks: 
leaks: CPPFLAGS=$(LEAKFLAGS)
leaks: all

prod: CPPFLAGS=$(PRODFLAGS)
prod: all



helperlib: $(HELPERLIB)
myplugins: $(MYPLUGINSLIB)

# ld only looks in ar files once, in the order they're quoted, and only uses symbols in it that are unfulfilled *at the point it opens the ar*
$(HELPERLIB): $(HELPEROBJ) $(HELPERHEADERS)
	$(CXX) $(LDFLAGS) -c $(HELPEROBJ) $(CPPFLAGS) $(CXXFLAGS) $(GSTGETBASECONFIG)
	ar rvs $(HELPERLIB) $(HELPERAR)

$(MYPLUGINSLIB) : $(MYPLUGINSOBJ) $(MYPLUGINSHEAD)
	$(CXX) $(LDFLAGS) -c $(MYPLUGINSOBJ) $(CPPFLAGS) $(CXXFLAGS) $(GSTCONFIG) 
	ar rvs $(MYPLUGINSLIB) $(MYPLUGINSAR)

clean:
	rm -rf *.o *.a

