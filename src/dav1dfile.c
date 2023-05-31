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

	uint8_t eof;
} Context;

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

	dav1d_default_settings(&settings);

	result = dav1d_open(&dav1dContext, &settings);
	if (result < 0)
	{
		free(dav1dContext);
		free(internalContext);
		return -1;
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

			break;
		}
	}

	/* Did not find a valid sequence header! */
	if (internalContext->width == 0 || internalContext->height == 0)
	{
		free(dav1dContext);
		free(internalContext);
		return -1;
	}

	*context = (AV1_Context*) internalContext;

	return 0;
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

	return -1;
}

void df_videoinfo(
	AV1_Context *context,
	int *width,
	int *height,
	double *fps
) {
	Context *internalContext = (Context*) context;

	*width = internalContext->width;
	*height = internalContext->height;
	*fps = internalContext->fps;
}

int df_readvideo(AV1_Context *context,int numFrames, void **yData, void **uData, void **vData)
{
	Context *internalContext = (Context*) context;
	Dav1dData data;
	int getPictureResult;
	int sendDataResult;
	int i;

	for (i = 0; i < numFrames; i += 1)
	{
		/* Get a packet... */
		if (!INTERNAL_getNextPacket(internalContext))
		{
			/* ... unless there's nothing left for us to read. */
			internalContext->eof = 1;
			break;
		}

		/* Let's send some data to the decoder... */
		data.data = internalContext->bitstreamData + internalContext->bitstreamIndex;
		data.sz = internalContext->currentOBUSize;
		data.ref = NULL;

		sendDataResult = dav1d_send_data(internalContext->dav1dContext, &data);

		while (sendDataResult == DAV1D_ERR(EAGAIN))
		{
			sendDataResult = dav1d_send_data(internalContext->dav1dContext, &data);
		}

		if (sendDataResult != 0)
		{
			/* Something went wrong on data send! */
			return -1;
		}

		/* Time to get a picture... */
		getPictureResult = dav1d_get_picture(internalContext->dav1dContext, &internalContext->currentPicture);

		if (getPictureResult != 0)
		{
			/* Something went wrong on picture get! */
			return -1;
		}
	}

	/* Set the picture data pointers */
	*yData = internalContext->currentPicture.data[0];
	*uData = internalContext->currentPicture.data[1];
	*vData = internalContext->currentPicture.data[2];

	return 0;
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
