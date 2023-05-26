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

typedef struct AV1_Context
{
	Dav1dContext *context;
} AV1_Context;

DECLSPEC int df_fopen(const char *fname, AV1_Context *context);
DECLSPEC void df_close(AV1_Context *context);

DECLSPEC void df_videoinfo(
	AV1_Context *context,
	int *width,
	int *height,
	double *fps);

DECLSPEC int df_eos(AV1_Context *context);
DECLSPEC void df_reset(AV1_Context *context);

DECLSPEC int df_readvideo(AV1_Context *context, char *buffer, int numFrames);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DAV1DFILE_H */
