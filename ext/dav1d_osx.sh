: # Run this before configuring CMake.

: # meson and ninja must be in your PATH.

: # NOTE: on Windows you must run this through the Visual Studio command prompt
: # NOTE: on Windows x86 you must install NASM 2.14 or higher: https://nasm.us/

git clone -b 1.4.0 --depth 1 https://code.videolan.org/videolan/dav1d.git

cd dav1d
mkdir -p arm64
cd arm64

: # macOS might require: -Dc_args=-fno-stack-check
: # Build with asan: -Db_sanitize=address
: # Build with ubsan: -Db_sanitize=undefined

MACOSX_DEPLOYMENT_TARGET=11.0 meson setup --default-library=static --buildtype release ..
MACOSX_DEPLOYMENT_TARGET=11.0 ninja

cd ..
mkdir -p x64
cd x64
MACOSX_DEPLOYMENT_TARGET=11.0 meson setup --cross-file=../../macos_x64.txt --default-library=static --buildtype release ..
MACOSX_DEPLOYMENT_TARGET=11.0 ninja

cd ..
lipo -create -output universal.a arm64/libdav1dfile.a x64/libdav1dfile.a
cd ..
