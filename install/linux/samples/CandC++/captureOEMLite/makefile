
CXX=g++
INCLUDES=-I$(PIXELINK_SDK_INC)
LIBPATH=-L$(PIXELINK_SDK_LIB) 
DEFINES=-DPIXELINK_LINUX
LIBS=-lPxLApi
CFLAGS=-O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP $(DEFINES) $(INCLUDES)
# When using glade, specify the -rdynamic linker option to ensure unreferenced signal handlers do not get removed
LDFLAGs=-rdynamic

CFLAGS += `pkg-config --cflags gtk+-3.0 gmodule-2.0`
LDFLAGS += `pkg-config --libs gtk+-3.0 gmodule-2.0`

SRCFILES=captureOEMLite.cpp camera.cpp cameraSelectCombo.cpp previewButtons.cpp exposure.cpp \
         frameRate.cpp pixelFormat.cpp pixelAddress.cpp roi.cpp gain.cpp focus.cpp \
         whiteBalance.cpp imageFlip.cpp imageCapture.cpp featurePoller.cpp slider.cpp
OBJFILES=$(SRCFILES:.cpp=.o)

all: captureOEMLite

captureOEMLite: $(OBJFILES) 
	rm -f $@
	$(CXX) $(LIBPATH) -o $@ $^ $(LIBS) $(LDFLAGS)

.cpp.o:
	$(CXX) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o
	rm -rf captureOEMLite


