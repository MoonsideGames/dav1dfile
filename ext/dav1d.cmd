: # Run this before configuring CMake.

: # meson and ninja must be in your PATH.

: # NOTE: on Windows you must run this through the Visual Studio command prompt
: # NOTE: on Windows x86 you must install NASM 2.14 or higher: https://nasm.us/

git clone -b 1.5.3 --depth 1 https://code.videolan.org/videolan/dav1d.git

cd dav1d
mkdir build

: # Build with asan: -Db_sanitize=address
: # Build with ubsan: -Db_sanitize=undefined
meson setup --vsenv --default-library=static --buildtype release ./build
meson compile -C ./build
cd ..
