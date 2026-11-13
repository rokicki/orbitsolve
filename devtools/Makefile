# Development tools for gullicube: data generators, tests and benchmarks.
# The solver itself (source of truth) lives in ../dist/gullicube; these tools
# build against its headers and its copy of brobdicube's sources.
DIST     = ../dist/gullicube
CXX     ?= clang++
CXXFLAGS = -std=c++20 -O3 -g -Wall -I$(DIST)/src/brob -I$(DIST)/src/full -I.
BROBSRC  = $(DIST)/src/brob/xcube.cpp $(DIST)/src/brob/cornersolver.cpp $(DIST)/src/brob/wingsolver.cpp $(DIST)/src/brob/util.cpp

all: mine minewing enumwing mkclasses mkpools mkcycles mkbin pairdata pairdeep wingbench brobcheck

brob.o: $(DIST)/src/full/brobglue.cpp $(BROBSRC)
	$(CXX) $(CXXFLAGS) -w -c -o brob_xcube.o $(DIST)/src/brob/xcube.cpp
	$(CXX) $(CXXFLAGS) -w -c -o brob_corner.o $(DIST)/src/brob/cornersolver.cpp
	$(CXX) $(CXXFLAGS) -w -c -o brob_wing.o $(DIST)/src/brob/wingsolver.cpp
	$(CXX) $(CXXFLAGS) -w -c -o brob_util.o $(DIST)/src/brob/util.cpp
	$(CXX) $(CXXFLAGS) -c -o brobglue.o $(DIST)/src/full/brobglue.cpp
	ld -r -o brob.o brob_xcube.o brob_corner.o brob_wing.o brob_util.o brobglue.o

%: %.cpp brob.o $(wildcard $(DIST)/src/full/*.h)
	$(CXX) $(CXXFLAGS) -o $@ $< brob.o -lpthread

.PHONY: all
