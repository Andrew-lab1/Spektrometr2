
CXX=gcc
INCLUDES=-I$(PIXELINK_SDK_INC)
LIBPATH=-L$(PIXELINK_SDK_LIB) 
DEFINES=-DPIXELINK_LINUX
LIBS=-lPxLApi 
CFLAGS=-O0 -g3 -Wall -c -std=c99 -fmessage-length=0 -MMD -MP $(DEFINES) $(INCLUDES)

SRCFILES=getPolarSnapshot.c main.c
OBJFILES=$(SRCFILES:.c=.o)

all: getPolarSnapshot

getPolarSnapshot : $(OBJFILES) 
	rm -f $@
	$(CXX) $(LIBPATH) -o $@ $^ $(LIBS)

.c.o:
	$(CXX) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o
	rm -rf getPolarSnapshot


