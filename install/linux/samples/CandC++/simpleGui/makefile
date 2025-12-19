
CXX=g++
INCLUDES=-I$(PIXELINK_SDK_INC)
LIBPATH=-L$(PIXELINK_SDK_LIB) 
DEFINES=-DPIXELINK_LINUX
LIBS=-lPxLApi
CFLAGS=-O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP $(DEFINES) $(INCLUDES)
LDFLAGs=

CFLAGS += `pkg-config --cflags gtk+-3.0 gmodule-2.0`
LDFLAGS += `pkg-config --libs gtk+-3.0 gmodule-2.0`

SRCFILES=simpleGui.cpp camera.cpp cameraSelectCombo.cpp previewButtons.cpp
OBJFILES=$(SRCFILES:.cpp=.o)

all: simpleGui

simpleGui: $(OBJFILES) 
	rm -f $@
	$(CXX) $(LIBPATH) -o $@ $^ $(LIBS) $(LDFLAGS)

.cpp.o:
	$(CXX) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o
	rm -rf simpleGui


