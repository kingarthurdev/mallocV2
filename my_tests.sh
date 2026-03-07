#!/bin/bash

gcc -o tests main.c libtdmm/tdmm.c -lm
if [ $? -ne 0 ]; then
    echo "COMPILATION FAILED"
    exit 1
fi
./tests
if [ $? -ne 0 ]; then
    echo "TESTS FAILED (non-zero exit)"
    exit 1
fi