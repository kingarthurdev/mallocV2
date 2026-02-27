#!/usr/bin/env bash

rm -rf build
cmake -S . -B build
make -C build
cp README.md ./build/README.md
cp README.md ../README.md
cp README.md ../../README.md
