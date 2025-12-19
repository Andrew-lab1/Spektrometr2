
CXX=g++

INCLUDES=-I$(PIXELINK_SDK_INC)
LIBPATH=-L$(PIXELINK_SDK_LIB) 

DEFINES=-DPIXELINK_LINUX

LIBS=-lPxLApi 
DEPENDANT_LIBS = -lusb-1.0 -lSDL2 -lpthread -lpng

CFLAGS=-O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP $(DEFINES) $(INCLUDES)

SRCFILES=preview.cpp LinuxUtil.cpp
OBJFILES=$(SRCFILES:.cpp=.o)

all: static_preview

static_preview : $(OBJFILES) 
	rm -f $@
	$(CXX) $(LIBPATH) -o $@ $^ -Bstatic $(LIBS) -Bdynamic $(DEPENDANT_LIBS)

.cpp.o:
	$(CXX) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o
	rm -rf static_preview


