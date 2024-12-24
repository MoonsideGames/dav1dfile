/* dav1dfile - AV1 Video Decoder Library
 *
 * Copyright (c) 2023 Evan Hemsley
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Evan "cosmonaut" Hemsley <evan@moonside.games>
 *
 */

#ifndef DAV1DFILE_H
#define DAV1DFILE_H

#include "dav1d/dav1d.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifndef DECLSPEC
#if defined(_WIN32)
#define DECLSPEC __declspec(dllexport)
#else
#define DECLSPEC
#endif
#endif

#define DAV1DFILE_MAJOR_VERSION 1
#define DAV1DFILE_MINOR_VERSION 1
#define DAV1DFILE_PATCH_VERSION 1

#define DAV1DFILE_COMPILED_VERSION ( \
	(DAV1DFILE_MAJOR_VERSION * 100 * 100) + \
	(DAV1DFILE_MINOR_VERSION * 100) + \
	(DAV1DFILE_PATCH_VERSION) \
)

DECLSPEC uint32_t df_linked_version(void);

typedef struct AV1_Context AV1_Context;

typedef enum PixelLayout
{
	PIXEL_LAYOUT_I400,
	PIXEL_LAYOUT_I420,
	PIXEL_LAYOUT_I422,
	PIXEL_LAYOUT_I444
} PixelLayout;

DECLSPEC int df_open_from_memory(uint8_t* bytes, uint32_t size, AV1_Context** context);
DECLSPEC int df_fopen(const char *fname, AV1_Context **context);
DECLSPEC void df_close(AV1_Context *context);

DECLSPEC void df_videoinfo(
	AV1_Context *context,
	int *width,
	int *height,
	PixelLayout *pixelLayout);

DECLSPEC void df_videoinfo2(
	AV1_Context *context,
	int *width,
	int *height,
	PixelLayout *pixelLayout,
	uint8_t *hbd);

DECLSPEC int df_eos(AV1_Context *context);
DECLSPEC void df_reset(AV1_Context *context);

DECLSPEC int df_readvideo(
	AV1_Context *context,
	int numFrames,
	void **yData,
	void **uData,
	void **vData,
	uint32_t *yDataLength,
	uint32_t *uvDataLength,
	uint32_t *yStride,
	uint32_t *uvStride);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DAV1DFILE_H */
