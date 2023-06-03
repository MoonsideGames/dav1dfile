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

typedef struct PictureMemoryPool
{
	uint8_t **buffers;
	uint32_t bufferCount;
	uint32_t bufferCapacity;
} PictureMemoryPool;

typedef struct Context {
	Dav1dContext *dav1dContext;

	uint8_t *bitstreamData;
	uint32_t bitstreamDataSize;
	uint32_t bitstreamIndex;
	uint32_t currentOBUSize;

	Dav1dPicture currentPicture;

	int32_t width;
	int32_t height;
	PixelLayout pixelLayout;

	uint32_t yDataLength;
	uint32_t uvDataLength;
	uint32_t yStride;
	uint32_t uvStride;

	uint8_t eof;

	PictureMemoryPool pictureMemoryPool;
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

static uint8_t* acquire_buffer_from_pool(Context *context)
{
	uint8_t *result;
	uint32_t size = context->yDataLength + context->uvDataLength + context->uvDataLength;

	if (context->pictureMemoryPool.bufferCount > 0)
	{
		result = context->pictureMemoryPool.buffers[context->pictureMemoryPool.bufferCount - 1];
		context->pictureMemoryPool.bufferCount -= 1;
	}
	else
	{
		result = aligned_alloc(128, size);
	}

	memset(result, '\0', size);

	return result;
}

static void return_buffer_to_pool(Context *context, uint8_t *buffer)
{
	if (context->pictureMemoryPool.bufferCount >= context->pictureMemoryPool.bufferCapacity)
	{
		context->pictureMemoryPool.bufferCapacity *= 2;
		context->pictureMemoryPool.buffers = realloc(context->pictureMemoryPool.buffers, sizeof(uint8_t*) * context->pictureMemoryPool.bufferCapacity);
	}

	context->pictureMemoryPool.buffers[context->pictureMemoryPool.bufferCount] = buffer;
	context->pictureMemoryPool.bufferCount += 1;
}

int picture_alloc(Dav1dPicture *picture, void *opaque)
{
	Context *internalContext = (Context*) opaque;
	picture->allocator_data = acquire_buffer_from_pool(internalContext);

	picture->data[0] = picture->allocator_data;
	picture->data[1] = (uint8_t*) picture->allocator_data + internalContext->yDataLength;
	picture->data[2] = (uint8_t*) picture->allocator_data + internalContext->yDataLength + internalContext->uvDataLength;
	picture->stride[0] = internalContext->yStride;
	picture->stride[1] = internalContext->uvStride;

	return 0;
}

static void picture_free(Dav1dPicture *picture, void *opaque)
{
	Context *internalContext = (Context*) opaque;
	return_buffer_to_pool(internalContext, picture->allocator_data);
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
	uint32_t chromaWidth;
	int result;

	internalContext->bitstreamData = bytes;
	internalContext->bitstreamDataSize = size;
	internalContext->bitstreamIndex = 0;
	internalContext->currentOBUSize = 0;
	internalContext->eof = 0;
	internalContext->width = 0;
	internalContext->height = 0;
	memset(&internalContext->currentPicture, '\0', sizeof(Dav1dPicture));

	internalContext->pictureMemoryPool.bufferCapacity = 4;
	internalContext->pictureMemoryPool.bufferCount = 0;
	internalContext->pictureMemoryPool.buffers = malloc(sizeof(uint8_t *) * internalContext->pictureMemoryPool.bufferCapacity);

	dav1d_default_settings(&settings);
	settings.allocator.alloc_picture_callback = picture_alloc;
	settings.allocator.release_picture_callback = picture_free;
	settings.allocator.cookie = internalContext;
	settings.apply_grain = 0;

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
			internalContext->pixelLayout = sequenceHeader.layout;
			break;
		}
	}

	/* Did not find a valid sequence header! */
	if (internalContext->width == 0 || internalContext->height == 0)
	{
		dav1d_close(&dav1dContext);
		free(dav1dContext);
		free(internalContext);
		return 0;
	}

	/* Reset the stream index */
	internalContext->bitstreamIndex = 0;
	internalContext->currentOBUSize = 0;

	/* Set data parameters */
	internalContext->yDataLength =
		aligned(internalContext->width, 128) *
		aligned(internalContext->height, 128);

	if (internalContext->pixelLayout == I420)
	{
		chromaWidth = internalContext->width / 2;
		internalContext->uvDataLength =
			aligned(chromaWidth, 128) *
			aligned(internalContext->height / 2, 128);
	}
	else if (internalContext->pixelLayout == I422)
	{
		chromaWidth = internalContext->width / 2;
		internalContext->uvDataLength =
			aligned(chromaWidth, 128) *
			aligned(internalContext->height, 128);
	}
	else if (internalContext->pixelLayout == I444)
	{
		chromaWidth = internalContext->width;
		internalContext->uvDataLength =
			aligned(chromaWidth, 128) *
			aligned(internalContext->height, 128);
	}
	else
	{
		/* I400 format not supported!*/
		dav1d_close(&dav1dContext);
		free(dav1dContext);
		free(internalContext);
		return 0;
	}

	internalContext->yStride = aligned(internalContext->width, 128);
	internalContext->uvStride = aligned(chromaWidth, 128);

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
	PixelLayout *pixelLayout
) {
	Context *internalContext = (Context*) context;

	*width = internalContext->width;
	*height = internalContext->height;
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
	uint32_t *yDataLength,
	uint32_t *uvDataLength,
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
	*yStride = internalContext->yStride;
	*uvStride = internalContext->uvStride;
	*yDataLength = internalContext->yDataLength;
	*uvDataLength = internalContext->uvDataLength;

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
	uint32_t i;
	Context *internalContext = (Context*) context;

	dav1d_close(&internalContext->dav1dContext);

	for (i = 0; i < internalContext->pictureMemoryPool.bufferCount; i += 1)
	{
		aligned_free(internalContext->pictureMemoryPool.buffers[i]);
	}
	free(internalContext->pictureMemoryPool.buffers);

	free(internalContext);
}
