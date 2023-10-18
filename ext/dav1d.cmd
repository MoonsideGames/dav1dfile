: # Run this before configuring CMake.

: # meson and ninja must be in your PATH.

: # NOTE: on Windows you must run this through the Visual Studio command prompt
: # NOTE: on Windows x86 you must install NASM 2.14 or higher: https://nasm.us/

git clone -b 1.2.0 --depth 1 https://code.videolan.org/videolan/dav1d.git

cd dav1d
mkdir build
cd build

: # macOS might require: -Dc_args=-fno-stack-check
: # Build with asan: -Db_sanitize=address
: # Build with ubsan: -Db_sanitize=undefined
MACOSX_DEPLOYMENT_TARGET=10.9 meson setup --default-library=static --buildtype release ..
MACOSX_DEPLOYMENT_TARGET=10.9 ninja
cd ../..
