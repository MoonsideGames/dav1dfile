: # Run this before configuring CMake.

: # meson and ninja must be in your PATH.

: # NOTE: on Windows you must run this through the Visual Studio command prompt
: # NOTE: on Windows x86 you must install NASM 2.14 or higher: https://nasm.us/

git clone -b `cat ext/libdav1d/VERSION` --depth 1 https://code.videolan.org/videolan/dav1d.git

cd dav1d
mkdir -p build
cd build

meson setup --default-library=static --buildtype release ..
ninja

cd ../..
