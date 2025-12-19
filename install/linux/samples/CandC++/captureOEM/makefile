TARGET_EXEC ?= captureOEM

CXX=g++

BUILD_DIR ?= ./bin
SRC_DIRS ?= ./src
INC_DIRS ?= ./inc

INCLUDES=-I$(PIXELINK_SDK_INC) -I../inc
LIBPATH=-L$(PIXELINK_SDK_LIB)  -L../lib/Release
DEFINES=-DPIXELINK_LINUX
# The below statement does not help with debug much.  Rather, Run under the debugger but with the following
# added as arguments to the command line '-prog-args --g-fatal-warnings'.  It will cause GTK assertions to abort
#DEFINES=-DPIXELINK_LINUX -DDEBUG 
LIBS=-lPxLApi -lSDL2 

SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c -or -name *.s)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(INC_DIRS) -type d) $(INCLUDES)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS=-O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP $(DEFINES) $(INCLUDES) $(INC_FLAGS) 
# When using glade, specify the -rdynamic linker option to ensure unreferenced signal handlers do not get removed
LDFLAGs=-rdynamic

CPPFLAGS += -std=c++98 `pkg-config --cflags gtk+-3.0 gmodule-2.0`
LDFLAGS += `pkg-config --libs gtk+-3.0 gmodule-2.0`

#CPPFLAGS ?= $(INC_FLAGS) -DPIXELINK_LINUX -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CXX) $(LIBPATH) $(OBJS) -o $@ $(LIBS) $(LDFLAGS)

# assembly
$(BUILD_DIR)/%.s.o: %.s
	$(MKDIR_P) $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# c++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


.PHONY: clean

clean:
#	$(RM) -r $(BUILD_DIR)
	$(RM) $(BUILD_DIR)/$(TARGET_EXEC)
	$(RM) -r $(BUILD_DIR)/src

-include $(DEPS)

MKDIR_P ?= mkdir -p
