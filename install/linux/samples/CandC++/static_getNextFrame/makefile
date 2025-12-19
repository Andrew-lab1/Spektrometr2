
CXX=g++

INCLUDES=-I$(PIXELINK_SDK_INC)
LIBPATH=-L$(PIXELINK_SDK_LIB) 

DEFINES=-DPIXELINK_LINUX

# The full and Lite variants of the API have different dependant libraries, uncomment
# as appropriate to choose between the two
LIBS=-lPxLApi 
DEPENDANT_LIBS = -lusb-1.0 -lSDL2 -lpthread -lpng
#LIBS=-lPxLApiLite 
#DEPENDANT_LIBS = -lusb-1.0 -lpthread

CFLAGS=-O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP $(DEFINES) $(INCLUDES)

SRCFILES=getNextFrame.cpp
OBJFILES=$(SRCFILES:.cpp=.o)

all: static_getNextFrame

static_getNextFrame : $(OBJFILES) 
	rm -f $@
	$(CXX) $(LIBPATH) -o $@ $^ -Bstatic $(LIBS) -Bdynamic $(DEPENDANT_LIBS)

.cpp.o:
	$(CXX) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o
	rm -rf static_getNextFrame


