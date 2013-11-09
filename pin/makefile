##
## This is a sample makefile for building Pin tools outside
## of the Pin environment.  This makefile is suitable for
## building with the Pin kit, not a Pin source development tree.
##
## To build the tool, execute the make command:
##
##      make
## or
##      make PIN_HOME=<top-level directory where Pin was installed>
##
## After building your tool, you would invoke Pin like this:
## 
##      $PIN_HOME/pin -t MyPinTool -- /bin/ls
##
##############################################################
#
# User-specific configuration
#
##############################################################

#
# 1. Change PIN_HOME to point to the top-level directory where
#    Pin was installed. This can also be set on the command line,
#    or as an environment variable.
#
PIN_HOME ?= ../../..


##############################################################
#
# set up and include *.config files
#
##############################################################

PIN_KIT=$(PIN_HOME)
KIT=1
TESTAPP=$(OBJDIR)cp-pin.exe

TARGET_COMPILER?=gnu
ifdef OS
    ifeq (${OS},Windows_NT)
        TARGET_COMPILER=ms
    endif
endif

ifeq ($(TARGET_COMPILER),gnu)
    include $(PIN_HOME)/source/tools/makefile.gnu.config
    CXXFLAGS ?= -Wall -Werror -Wno-unknown-pragmas $(DBG) $(OPT)
    PIN=$(PIN_HOME)/pin
endif

ifeq ($(TARGET_COMPILER),ms)
    include $(PIN_HOME)/source/tools/makefile.ms.config
    DBG?=
    PIN=$(PIN_HOME)/pin.bat
endif


##############################################################
#
# Tools - you may wish to add your tool name to TOOL_ROOTS
#
##############################################################


TOOL_ROOTS = MyPinTool

TOOLS = $(TOOL_ROOTS:%=$(OBJDIR)%$(PINTOOL_SUFFIX))

INC_DIRS = -I.

##############################################################
#
# build rules
#
##############################################################

all:	packages tools

packages:	
	make -C MultiCacheSim-dist/
	make -C adt/

tools: $(OBJDIR) $(TOOLS) $(OBJDIR)cp-pin.exe
test: $(OBJDIR) $(TOOL_ROOTS:%=%.test) mytest

mytest:		test.cpp Bloom.cpp
			g++ -o $@ $^

MyPinTool.test: $(OBJDIR)cp-pin.exe
	$(MAKE) -k PIN_HOME=$(PIN_HOME)

$(OBJDIR)cp-pin.exe:
	$(CXX) $(PIN_HOME)/source/tools/Tests/cp-pin.cpp $(APP_CXXFLAGS) -o $(OBJDIR)cp-pin.exe

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)%.o : %.cpp
	$(CXX) $(INC_DIRS) -c $(CXXFLAGS) $(PIN_CXXFLAGS) ${OUTOPT}$@ $<

$(TOOLS): $(PIN_LIBNAMES)

# adt/hashmap.o:	adt/hashmap.c
# 				gcc -c -Wall -Werror -fpic $< -o $@

MCS_OBJS = MultiCacheSim-dist/CacheCore.o \
		   MultiCacheSim-dist/nanassert.o \
		   MultiCacheSim-dist/MultiCacheSim.o \
		   MultiCacheSim-dist/SMPCache.o \
		   MultiCacheSim-dist/Snippets.o

MY_OBJS = $(OBJDIR)MultiCacheSim_PinDriver.o\
		  $(OBJDIR)Bloom.o

$(TOOLS): %$(PINTOOL_SUFFIX) : %.o $(MY_OBJS)
	${PIN_LD} $(PIN_LDFLAGS) $(LINK_DEBUG) $(MCS_OBJS) ${LINK_OUT}$@ $^ ${PIN_LPATHS} $(PIN_LIBS) $(DBG) -lrt


## cleaning
clean:
	-rm -rf $(OBJDIR) *.out *.tested *.failed makefile.copy
	make -C adt clean
	make -C MultiCacheSim-dist clean
