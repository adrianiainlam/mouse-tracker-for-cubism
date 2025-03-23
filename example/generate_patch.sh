#!/bin/sh

mkdir -p demo_clean
cp -p -r CubismSdkForNative-5-r.3/Samples/OpenGL/Demo/proj.linux.cmake/* ./demo_clean/
diff -pruN --exclude build ./demo_clean ./demo_dev > ./demo.patch
