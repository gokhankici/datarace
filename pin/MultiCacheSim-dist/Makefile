PROG = CacheTestDriver
PINTOOL = MultiCacheSim_PinDriver.so
MARKDOWN = /usr/bin/markdown

SRCS = SMPCache.cpp MultiCacheSim.cpp CacheCore.cpp Snippets.cpp nanassert.cpp
FACSRCS = MSI_SMPCache.cpp MESI_SMPCache.cpp

#BLDTYPE=
BLDTYPE=pin

ifeq ($(BLDTYPE),pin)
PIN_KIT=$(PIN_HOME)
KIT=1
include $(PIN_HOME)/source/tools/makefile.gnu.config
CXXFLAGS += -DPIN
SRCS += MultiCacheSim_PinDriver.cpp
TARG = $(PINTOOL)
else
CXXFLAGS += -UPIN
LDFLAGS = -lpthread -ldl
SRCS += CacheTestDriver.cpp
TARG = $(PROG)
endif


CXXFLAGS += -I. -g -O0 -Wno-deprecated

OBJS = $(SRCS:%.cpp=%.o)
FACOBJS = $(FACSRCS:%.cpp=%.so)


all: $(TARG)

test: $(PROG)
	./CacheTestDriver ./MSI_SMPCache.so

## build rules
%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(PIN_CXXFLAGS) -o $@ $<

%.so : %.cpp
	$(CXX) -fPIC -shared $(CXXFLAGS) $(PIN_CXXFLAGS) -o $@ $< SMPCache.cpp Snippets.cpp nanassert.cpp CacheCore.cpp

$(PROG): $(OBJS) 
	$(CXX) -o $@ $+ $(DBG) $(LDFLAGS)

$(PINTOOL): $(OBJS)
	$(CXX) -o $@ $+ $(PIN_LIBS) $(DBG) $(PIN_LDFLAGS) $(LDFLAGS)

doc: README $(MARKDOWN) 
	echo "<html><head><title>MultiCacheSim Documentation</title></head><body>" >README.html
	$(MARKDOWN) README >> README.html
	echo "</body></html>" >> README.html

## cleaning
clean:
	-rm -f *.o $(PROG) $(PINTOOL)

-include *.d
