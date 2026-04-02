#!/bin/sh -e

mkdir -p demo_build
cp -r CubismSdkForNative-5-r.5/Samples/OpenGL/Demo/proj.linux.cmake/* ./demo_build/
cd demo_build
git init
git add .
git commit -m "Original example from CubismSdkForNative"
git apply ../demo.patch
./scripts/make_gcc
