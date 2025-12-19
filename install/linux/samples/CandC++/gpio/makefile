
CXX=g++
INCLUDES=-I$(PIXELINK_SDK_INC)
LIBPATH=-L$(PIXELINK_SDK_LIB) 
DEFINES=-DPIXELINK_LINUX
LIBS=-lPxLApi -lpthread
CFLAGS=-O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP $(DEFINES) $(INCLUDES)

SRCFILES=gpio.cpp LinuxUtil.cpp
OBJFILES=$(SRCFILES:.cpp=.o)

all: gpio

gpio: $(OBJFILES) 
	rm -f $@
	$(CXX) $(LIBPATH) -o $@ $^ $(LIBS)

.cpp.o:
	$(CXX) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o
	rm -rf gpio



