#!/bin/sh

swig -I../include/gs++/ -python cslp.i
gcc -c -fpic -I../include -I../build/absl/include -I/usr/include/python3.5 cslp_wrap.c ../src/cslp.cpp

