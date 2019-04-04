#!/bin/sh

sudo mkdir build && cd build

export CCC_CC=clang
export CCC_CXX=clang++
scan-build cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DENABLE_CUDA=Off ..
scan-build make
