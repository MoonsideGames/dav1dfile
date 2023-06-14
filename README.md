This is dav1dfile, an AV1 video decoder system for game development.

About dav1dfile
----------------
dav1dfile is inspired by the FNA project's Theorafile.

dav1dfile decodes video data from a single raw .obu (open bitstream unit) file containing encoded AV1 data.

It does not provide any mechanism for decoding an associated audio stream.

A C# interop project is included.

Dependencies
------------
dav1dfile depends on VideoLAN's libdav1d. A helper script for downloading and building libdav1d is included in ext/dav1d.cmd.

To build libdav1d, you will need to have meson, ninja, and nasm installed on your system.

Building dav1dfile
-------------------
Once a libdav1d build is available, dav1dfile can be built easily with CMake:

	$ mkdir build/
	$ cd build
	$ cmake ../
	$ make

License
-------
dav1dfile is licensed under the zlib license. See LICENSE for details.
