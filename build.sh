#!/usr/bin/env bash

rm -rf build
cmake -S . -B build
make -C build
cp README ./build/README
cp README ../README
cp README ../../README
