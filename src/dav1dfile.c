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

#include "dav1dfile.h"
#include "obuparse.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define inline __inline
#endif /* _WIN32 */

typedef struct Context {
	Dav1dContext *dav1dContext;

	uint8_t *bitstreamData;
	uint32_t bitstreamDataSize;
	size_t bitstreamIndex;
	size_t currentOBUSize;

	Dav1dPicture currentPicture;

	int32_t width;
	int32_t height;
	PixelLayout pixelLayout;
	uint8_t hbd;

	uint8_t eof;
} Context;

static void allocator_no_op(const uint8_t *data, void *opaque)
{
	/* no-op */
}

uint32_t df_linked_version(void)
{
	return DAV1DFILE_COMPILED_VERSION;
}

static inline int INTERNAL_getNextPacket(
	Context *context
) {
	OBPOBUType obuType;
	ptrdiff_t offset;
	int temporal_id;
	int spatial_id;
	OBPError error;
	uint8_t* bitstreamPtr;
	int result;

	error.size = 0;

	context->bitstreamIndex += context->currentOBUSize;
	bitstreamPtr = context->bitstreamData + context->bitstreamIndex;

	result = obp_get_next_obu(
		bitstreamPtr,
		context->bitstreamDataSize - context->bitstreamIndex,
		&obuType,
		&offset,
		&context->currentOBUSize,
		&temporal_id,
		&spatial_id,
		&error);

	if (result >= 0)
	{
		context->currentOBUSize += offset; /* adding header size back in */
	}

	return result >= 0;
}

// 1 = success
// 0 = end of stream
// -1 = error
static int df_INTERNAL_read_data(Context *internalContext, Dav1dData *data)
{
	if (internalContext->bitstreamIndex == internalContext->bitstreamDataSize)
	{
		return 0;
	}

	if (!INTERNAL_getNextPacket(internalContext))
	{
		return -1;
	}

	if (dav1d_data_wrap(data, internalContext->bitstreamData + internalContext->bitstreamIndex, internalContext->currentOBUSize, allocator_no_op, NULL) < 0)
	{
		return -1;
	}

	return 1;
}

int df_open_from_memory(uint8_t *bytes, uint32_t size, AV1_Context **context)
{
	Context *internalContext = malloc(sizeof(Context));
	if (!internalContext)
	{
		return 0;
	}
	Dav1dContext *dav1dContext = malloc(sizeof(void*));
	if (!dav1dContext)
	{
		free(internalContext);
		return 0;
	}
	Dav1dSettings settings;
	Dav1dSequenceHeader sequenceHeader;
	int result;

	internalContext->bitstreamData = bytes;
	internalContext->bitstreamDataSize = size;
	internalContext->bitstreamIndex = 0;
	internalContext->currentOBUSize = 0;
	internalContext->eof = 0;
	internalContext->width = 0;
	internalContext->height = 0;
	memset(&internalContext->currentPicture, '\0', sizeof(Dav1dPicture));

	dav1d_default_settings(&settings);
	settings.apply_grain = 0;

	result = dav1d_open(&dav1dContext, &settings);
	if (result < 0)
	{
		free(dav1dContext);
		free(internalContext);
		return 0;
	}

	internalContext->dav1dContext = dav1dContext;

	while (INTERNAL_getNextPacket(internalContext))
	{
		if (dav1d_parse_sequence_header(
				&sequenceHeader,
				internalContext->bitstreamData + internalContext->bitstreamIndex,
				internalContext->currentOBUSize
			) == 0)
		{
			internalContext->width = sequenceHeader.max_width;
			internalContext->height = sequenceHeader.max_height;
			internalContext->pixelLayout = (PixelLayout) sequenceHeader.layout;
			internalContext->hbd = sequenceHeader.hbd;
			break;
		}
	}

	/* Did not find a valid sequence header! */
	if (internalContext->width == 0 || internalContext->height == 0 || internalContext->pixelLayout == PIXEL_LAYOUT_I400)
	{
		dav1d_close(&dav1dContext);
		free(dav1dContext);
		free(internalContext);
		return 0;
	}

	/* Reset the stream index */
	internalContext->bitstreamIndex = 0;
	internalContext->currentOBUSize = 0;

	*context = (AV1_Context*) internalContext;

	return 1;
}

static int df_open_from_file(FILE *file, AV1_Context **context)
{
	unsigned int len, start, result;

	start = (unsigned int) ftell(file);
	fseek(file, 0, SEEK_END);
	len = (unsigned int) (ftell(file) - start);
	fseek(file, start, SEEK_SET);

	unsigned char *bytes = malloc(len);
	if (!bytes)
	{
		fclose(file);
		return 0;
	}

	result = (unsigned int) fread(bytes, 1, len, file);
	fclose(file);

	if (result != len)
	{
		return 0;
	}

	return df_open_from_memory(bytes, len, context);
}

int df_fopen(const char *fname, AV1_Context **context)
{
	FILE *f = fopen(fname, "rb");

	if (f)
	{
		return df_open_from_file(f, context);
	}

	return 0;
}

void df_videoinfo(
	AV1_Context *context,
	int *width,
	int *height,
	PixelLayout *pixelLayout
) {
	Context *internalContext = (Context*) context;

	*width = internalContext->width;
	*height = internalContext->height;
	*pixelLayout = internalContext->pixelLayout;
}

void df_videoinfo2(
	AV1_Context *context,
	int *width,
	int *height,
	PixelLayout *pixelLayout,
	uint8_t *hbd
) {
	Context *internalContext = (Context*) context;

	*width = internalContext->width;
	*height = internalContext->height;
	*pixelLayout = internalContext->pixelLayout;
	*hbd = internalContext->hbd;
}

int df_readvideo(
	AV1_Context *context,
	int numFrames,
	void **yData,
	void **uData,
	void **vData,
	uint32_t *yDataLength,
	uint32_t *uvDataLength,
	uint32_t *yStride,
	uint32_t *uvStride
) {
	Context *internalContext = (Context*) context;
	Dav1dData data = {0};
	int res;
	int got_picture = 0;
	//int i;

	for (int i = 0; i < numFrames; i += 1)
	{
		dav1d_picture_unref(&internalContext->currentPicture);

		if (df_INTERNAL_read_data(internalContext, &data) == 1)
		{
			do
			{
				res = dav1d_send_data(internalContext->dav1dContext, &data);
				// Keep going even if the function can't consume the current data
				//   packet. It eventually will after one or more frames have been
				//   returned in this loop.
				if (res < 0 && res != DAV1D_ERR(EAGAIN)) {
					return 0;
				}
				res = dav1d_get_picture(internalContext->dav1dContext, &internalContext->currentPicture);
				if (res < 0)
				{
					if (res != DAV1D_ERR(EAGAIN)) {
						return 0;
					}
				}
				else
				{
					got_picture = 1;
					break;
				}
				// Stay in the loop as long as there's data to consume.
			} while (data.sz || df_INTERNAL_read_data(internalContext, &data) == 1);
		}

		if (!got_picture)
		{
			// end of bitstream, keep decoding
			res = dav1d_get_picture(internalContext->dav1dContext, &internalContext->currentPicture);
			if (res < 0)
			{
				internalContext->eof = 1;
				return 0;
			}
		}
	}

	/* Set the picture data pointers */
	*yData = internalContext->currentPicture.data[0];
	*uData = internalContext->currentPicture.data[1];
	*vData = internalContext->currentPicture.data[2];
	*yStride = internalContext->currentPicture.stride[0];
	*uvStride = internalContext->currentPicture.stride[1];

	const int ss_ver = internalContext->currentPicture.p.layout == DAV1D_PIXEL_LAYOUT_I420;
	const int aligned_h = (internalContext->currentPicture.p.h + 127) & ~127;
    *yDataLength = *yStride * aligned_h;
    *uvDataLength = *uvStride * (aligned_h >> ss_ver);

	return 1;
}

int df_eos(AV1_Context *context)
{
	return ((Context *) context)->eof;
}

void df_reset(AV1_Context *context)
{
	Context *internalContext = (Context*) context;
	dav1d_flush(internalContext->dav1dContext);
	internalContext->bitstreamIndex = 0;
	internalContext->currentOBUSize = 0;
	internalContext->eof = 0;
}

void df_close(AV1_Context *context)
{
	Context *internalContext = (Context*) context;

	dav1d_close(&internalContext->dav1dContext);

	free(internalContext);
}
