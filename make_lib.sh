#!/bin/bash
CXX=g++
CXXFLAGS="-g -O2 -march=native -pipe -fomit-frame-pointer -Wno-deprecated -DNDEBUG"
LDFLAGS=
$CXX $CXXFLAGS $LDFLAGS $@ *.cpp network/*.cpp -c 
ar rcs libcort_proto.a *.o

