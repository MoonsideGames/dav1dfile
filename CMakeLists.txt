# CMake Project for dav1dfile
# Written by @NeroBurner
cmake_minimum_required(VERSION 2.8.12)
project(dav1dfile C)

# Options
option(BUILD_SHARED_LIBS "Build shared library" ON)

# Version
SET(LIB_MAJOR_VERSION "1")
SET(LIB_MINOR_VERSION "1")
SET(LIB_REVISION "0")
SET(LIB_VERSION "${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION}.${LIB_REVISION}")

# Build Type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	# By default, we use Release
	message(STATUS "Setting build type to 'Release' as none was specified.")
	set(CMAKE_BUILD_TYPE "Release" CACHE
		STRING "Choose the type of build." FORCE
	)

	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
		STRINGS "Debug" "Release" "RelWithDebInfo"
	)
endif()

# Platform Flags
if(APPLE)
	set(CMAKE_MACOSX_RPATH ON)
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9)
	set(LOBJC "objc")
elseif(WIN32)
	# "dav1dfile.dll", not "libdav1dfile.dll"
	set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()

if(BUILD_SHARED_LIBS)
    set(LIBRARY_PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}")
    set(LIBRARY_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
else()
    set(LIBRARY_PREFIX "${CMAKE_STATIC_LIBRARY_PREFIX}")
    set(LIBRARY_SUFFIX "${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

# Defines

# Source lists
add_library(dav1dfile
	# Public Headers
	include/dav1dfile.h
	# Source Files
	src/dav1dfile.c
)

set(DAV1D_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dav1d")
set(LIB_FILENAME "${DAV1D_BUILD_DIR}/src/libdav1d${LIBRARY_SUFFIX}")
if(NOT EXISTS "${LIB_FILENAME}")
	if("${LIBRARY_SUFFIX}" STREQUAL ".a")
		message(FATAL_ERROR "dav1dfile: ${LIB_FILENAME} is missing, bailing out")
	else()
		# On windows, meson will produce a libdav1d.a instead of the expected libdav1d.dll/.lib.
		# See https://github.com/mesonbuild/meson/issues/8153.
		set(LIB_FILENAME "${CMAKE_CURRENT_SOURCE_DIR}/dav1d/build/src/libdav1d.a")
		if(NOT EXISTS "${LIB_FILENAME}")
			message(FATAL_ERROR "dav1dfile: ${LIB_FILENAME} (or libdav1d${LIBRARY_SUFFIX}) is missing, bailing out")
		endif()
	endif()
endif()

set(CODEC_INCLUDES ${CODEC_INCLUDES} "${DAV1D_BUILD_DIR}" "${DAV1D_BUILD_DIR}/include"
						"${DAV1D_BUILD_DIR}/include/dav1d" "${CMAKE_CURRENT_SOURCE_DIR}/dav1d/build/include/dav1d"
)
set(CODEC_LIBRARIES ${CODEC_LIBRARIES} ${LIB_FILENAME})

target_link_libraries(dav1dfile PRIVATE ${CODEC_LIBRARIES})
target_include_directories(
    dav1dfile PUBLIC $<BUILD_INTERFACE:${SOURCE_DIR}/include> PRIVATE ${CODEC_INCLUDES}
)

# Build flags
if(NOT MSVC)
	set_property(TARGET dav1dfile PROPERTY COMPILE_FLAGS "-std=gnu99 -Wall -Wno-strict-aliasing -pedantic")
endif()

# dav1dfile folders as includes, for other targets to consume
target_include_directories(dav1dfile PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# MinGW builds should statically link libgcc
if(MINGW)
	target_link_libraries(dav1dfile PRIVATE -static-libgcc)
endif()

# Soname
set_target_properties(dav1dfile PROPERTIES OUTPUT_NAME "dav1dfile"
	VERSION ${LIB_VERSION}
	SOVERSION ${LIB_MAJOR_VERSION}
)
