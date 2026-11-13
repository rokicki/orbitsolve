UNAME := $(shell uname)

CXX      := g++
CXXFLAGS := -march=native -Wall -Wsign-compare -g -std=c++20 -O3 -Isrc

ifeq ($(UNAME), Linux)
  LDFLAGS := -lpthread
else
  LDFLAGS :=
endif

DEPDIR := .deps
$(shell mkdir -p $(DEPDIR))

# -MMD -MP: auto-generate .d dep files alongside compilation.
# -MF names each dep file after the binary target it belongs to.
DEPFLAGS = -MMD -MP -MF $(DEPDIR)/$@.d

.PHONY: all clean

all: brobdicube pareto findpermnet algutil test_planmask

BROBD_SRCS := src/brobdicube.cpp src/xcube.cpp src/cornersolver.cpp \
              src/wingsolver.cpp src/netgen.cpp src/util.cpp src/fpng.cpp

H_FILES := src/cornersolver.h src/fpng.h src/mv.h src/netgen.h src/planmask.h \
           src/threecube.h src/util.h src/wingsolver.h src/xcube.h src/xorbit.h

brobdicube: $(BROBD_SRCS) $(H_FILES)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -o $@ \
	    src/brobdicube.cpp src/xcube.cpp src/cornersolver.cpp src/wingsolver.cpp \
	    src/netgen.cpp src/util.cpp -w src/fpng.cpp $(LDFLAGS)

pareto: src/pareto.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -o $@ $< $(LDFLAGS)

findpermnet: src/findpermnet.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -o $@ $< $(LDFLAGS)

algutil: src/algutil.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -o $@ $< $(LDFLAGS)

test_planmask: src/test_planmask.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -o $@ $< $(LDFLAGS)

-include $(DEPDIR)/brobdicube.d $(DEPDIR)/pareto.d $(DEPDIR)/findpermnet.d \
         $(DEPDIR)/algutil.d $(DEPDIR)/test_planmask.d

clean:
	rm -f brobdicube pareto findpermnet algutil test_planmask
	rm -rf test $(DEPDIR)
ifeq ($(UNAME), Darwin)
	rm -rf brobdicube.dSYM pareto.dSYM findpermnet.dSYM algutil.dSYM
endif
