: # Run this before configuring CMake.
: # meson and ninja must be in your PATH.

SDKROOT=$(xcodebuild -version -sdk appletvos Path)
DEVROOT=$(dirname `dirname $SDKROOT`)
echo $SDKROOT
echo $DEVROOT

cat ext/meson/tvos_template.txt | sed "s|%SDKROOT%|${SDKROOT}|g" | sed "s|%DEVROOT%|${DEVROOT}|g" > ext/meson/tvos.txt



git clone -b `cat ext/libdav1d/VERSION` --depth 1 https://code.videolan.org/videolan/dav1d.git

cd dav1d
mkdir -p build
cd build

MACOSX_DEPLOYMENT_TARGET=11.0 meson setup --cross-file=../../ext/meson/tvos.txt --default-library=static --buildtype release ..
MACOSX_DEPLOYMENT_TARGET=11.0 ninja

cd ..
cp ./build/src/libdav1d.a ./src/libdav1d.a