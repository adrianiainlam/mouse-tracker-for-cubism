#!/bin/sh -e

mkdir -p demo_build
cp -r CubismSdkForNative-5-r.4.1/Samples/OpenGL/Demo/proj.linux.cmake/* ./demo_build/
patch -d demo_build -p2 < demo.patch
./demo_build/scripts/make_gcc
