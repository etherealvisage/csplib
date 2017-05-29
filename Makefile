# Makefile for csplib tests

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -g

.PHONY: all
all: simpletest

simpletest.o: simpletest.cpp csplib.hpp
simpletest: simpletest.o
	$(CXX) $(CXXFLAGS) $^ -o $@

.PHONY: clean
clean:
	-rm simpletest simpletest.o
