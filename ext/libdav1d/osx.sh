: # Run this before configuring CMake.
: # meson and ninja must be in your PATH.

git clone -b `cat ext/libdav1d/VERSION` --depth 1 https://code.videolan.org/videolan/dav1d.git

cd dav1d
mkdir -p arm64
cd arm64

MACOSX_DEPLOYMENT_TARGET=11.0 meson setup --default-library=static --buildtype release ..
MACOSX_DEPLOYMENT_TARGET=11.0 ninja

cd ..
mkdir -p x64
cd x64

MACOSX_DEPLOYMENT_TARGET=11.0 meson setup --cross-file=../../ext/meson/macos_x64.txt --default-library=static --buildtype release ..
MACOSX_DEPLOYMENT_TARGET=11.0 ninja

cd ..
mkdir -p build
mkdir -p build/src
lipo -create -output build/src/libdav1d.a arm64/src/libdav1d.a x64/src/libdav1d.a
cp -r arm64/include build/include
cd ..
