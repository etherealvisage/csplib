# Makefile for csplib tests

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -g

simpletest: simpletest.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

.PHONY: clean
clean:
	-rm simpletest
