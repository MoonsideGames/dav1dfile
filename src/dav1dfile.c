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

#ifdef _WIN32
#define inline __inline
#endif /* _WIN32 */

typedef struct Context {
	Dav1dContext *dav1dContext;

	uint8_t *bitstreamData;
	uint32_t bitstreamDataSize;
	uint32_t bitstreamIndex;
	uint32_t currentOBUSize;

	Dav1dPicture currentPicture;

	int32_t width;
	int32_t height;
	double fps;
	PixelLayout pixelLayout;

	uint8_t eof;
} Context;

static void allocator_no_op(const uint8_t *data, void *opaque)
{
	/* no-op */
}

static inline uint32_t aligned(
	uint32_t n,
	uint32_t alignment
) {
	return ((n + (alignment - 1)) & ~(alignment - 1));
}

/* Aligned alloc functions borrowed from SDL 3.0 */

# define SDL_SIZE_MAX ((size_t) -1)

int size_add_overflow (size_t a, size_t b, size_t *ret)
{
    if (b > SIZE_MAX - a) {
        return -1;
    }
    *ret = a + b;
    return 0;
}

void *aligned_alloc(size_t alignment, size_t size)
{
    size_t padding;
    uint8_t *retval = NULL;

    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }
    padding = (alignment - (size % alignment));

    if (size_add_overflow(size, alignment, &size) == 0 &&
        size_add_overflow(size, sizeof(void *), &size) == 0 &&
        size_add_overflow(size, padding, &size) == 0) {
        void *original = malloc(size);
        if (original) {
            /* Make sure we have enough space to store the original pointer */
            retval = (uint8_t *)original + sizeof(original);

            /* Align the pointer we're going to return */
            retval += alignment - (((size_t)retval) % alignment);

            /* Store the original pointer right before the returned value */
            memcpy(retval - sizeof(original), &original, sizeof(original));
        }
    }
    return retval;
}

void aligned_free(void *mem)
{
    if (mem) {
        void *original;
        memcpy(&original, ((uint8_t *)mem - sizeof(original)), sizeof(original));
        free(original);
    }
}

int picture_alloc_no_op(Dav1dPicture *picture, void *opaque)
{
	uint32_t yLength;
	uint32_t uvLength;
	uint32_t chromaWidth;

	yLength = aligned(picture->p.w, 128) * aligned(picture->p.h, 128);

	if (picture->p.layout == DAV1D_PIXEL_LAYOUT_I420)
	{
		chromaWidth = picture->p.w / 2;
		uvLength =
			aligned(chromaWidth, 128) *
			aligned(picture->p.h / 2, 128);
	}
	else if (picture->p.layout == DAV1D_PIXEL_LAYOUT_I422)
	{
		chromaWidth = picture->p.w / 2;
		uvLength =
			aligned(chromaWidth, 128) *
			aligned(picture->p.h, 128);
	}
	else if (picture->p.layout == DAV1D_PIXEL_LAYOUT_I444)
	{
		chromaWidth = picture->p.w;
		uvLength =
			aligned(picture->p.w, 128) *
			aligned(picture->p.h, 128);
	}
	else
	{
		/* I400 format not supported!*/
		return -1;
	}

	picture->allocator_data = aligned_alloc(128, yLength + uvLength + uvLength);
	picture->data[0] = picture->allocator_data;
	picture->data[1] = (uint8_t*) picture->allocator_data + yLength;
	picture->data[2] = (uint8_t*) picture->allocator_data + yLength + uvLength;

	memset(picture->allocator_data, '\0', aligned(yLength + uvLength + uvLength, DAV1D_PICTURE_ALIGNMENT));

	/* FIXME: are these stride values correct */
	picture->stride[0] = aligned(picture->p.w, 128);
	picture->stride[1] = aligned(chromaWidth, 128);

	return 0;
}

static void picture_free_no_op(Dav1dPicture *picture, void *opaque)
{
	aligned_free(picture->allocator_data);
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

static int df_open_from_memory(uint8_t *bytes, uint32_t size, AV1_Context **context)
{
	Context *internalContext = malloc(sizeof(Context));
	Dav1dContext *dav1dContext = malloc(sizeof(void*));
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
	settings.allocator.alloc_picture_callback = picture_alloc_no_op;
	settings.allocator.release_picture_callback = picture_free_no_op;
	settings.allocator.cookie = internalContext;

	result = dav1d_open(&dav1dContext, &settings);
	if (result < 0)
	{
		free(dav1dContext);
		free(internalContext);
		return 0;
	}

	internalContext->dav1dContext = dav1dContext;

	/* TODO: initial sequence header parse */

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
			/* TODO: verify correctness */
			internalContext->fps = ((double) sequenceHeader.time_scale / sequenceHeader.num_units_in_tick) / 2;

			internalContext->pixelLayout = sequenceHeader.layout;
			break;
		}
	}

	/* Did not find a valid sequence header! */
	if (internalContext->width == 0 || internalContext->height == 0)
	{
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
	unsigned int len, start;
	start = (unsigned int) ftell(file);
	fseek(file, 0, SEEK_END);
	len = (unsigned int) (ftell(file) - start);
	fseek(file, start, SEEK_SET);

	unsigned char *bytes = malloc(len);
	fread(bytes, 1, len, file);
	fclose(file);

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
	double *fps,
	PixelLayout *pixelLayout
) {
	Context *internalContext = (Context*) context;

	*width = internalContext->width;
	*height = internalContext->height;
	*fps = internalContext->fps;
	*pixelLayout = internalContext->pixelLayout;
}

static int read_data(Context *internalContext, Dav1dData *data)
{
	if (internalContext->eof)
	{
		return 0;
	}

	if (!INTERNAL_getNextPacket(internalContext))
	{
		internalContext->eof = 1;
		return 0;
	}

	if (dav1d_data_wrap(data, internalContext->bitstreamData + internalContext->bitstreamIndex, internalContext->currentOBUSize, allocator_no_op, NULL) < 0)
	{
		return 0;
	}

	return 1;
}

int df_readvideo(
	AV1_Context *context,
	int numFrames,
	void **yData,
	void **uData,
	void **vData,
	uint32_t *yStride,
	uint32_t *uvStride
) {
	Context *internalContext = (Context*) context;
	Dav1dData data;
	int getPictureResult;
	int sendDataResult;
	int i;

	for (i = 0; i < numFrames; i += 1)
	{
		if (read_data(internalContext, &data) == 0)
		{
			break;
		}

		do
		{
			sendDataResult = dav1d_send_data(internalContext->dav1dContext, &data);

			if (sendDataResult < 0 && sendDataResult != DAV1D_ERR(EAGAIN))
			{
				/* Something went wrong on data send! */
				return 0;
			}

			dav1d_picture_unref(&internalContext->currentPicture);
			getPictureResult = dav1d_get_picture(internalContext->dav1dContext, &internalContext->currentPicture);

			if (getPictureResult < 0)
			{
				if (getPictureResult != DAV1D_ERR(EAGAIN))
				{
					/* Something went wrong on picture get! */
					return 0;
				}
			}
			else
			{
				break;
			}
		} while (data.sz || read_data(internalContext, &data) == 1);
	}

	/* Set the picture data pointers */
	*yData = internalContext->currentPicture.data[0];
	*uData = internalContext->currentPicture.data[1];
	*vData = internalContext->currentPicture.data[2];
	*yStride = internalContext->currentPicture.stride[0];
	*uvStride = internalContext->currentPicture.stride[1];

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
