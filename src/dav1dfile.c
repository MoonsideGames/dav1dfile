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

#include <SDL.h>

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

	uint8_t eof;
} Context;

int df_fopen(const char *fname, AV1_Context **context)
{
	Context *internalContext = SDL_malloc(sizeof(Context));
	Dav1dContext *dav1dContext = SDL_malloc(sizeof(void*));
	Dav1dSettings settings;
	int result;

	dav1d_default_settings(&settings);
	settings.allocator.cookie = dav1dContext;
	settings.allocator.alloc_picture_callback = SDL_malloc;
	settings.allocator.release_picture_callback = SDL_free;

	result = dav1d_open(&dav1dContext, &settings);
	if (result < 0)
	{
		return -1;
	}

	/* TODO: copy bitstream buffer and init context fields */

	internalContext->dav1dContext = dav1dContext;

	*context = internalContext;
}

void df_videoinfo(
	AV1_Context *context,
	int *width,
	int *height,
	double *fps
) {
	/* TODO */
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

int df_readvideo(AV1_Context *context, void **yData, void **uData, void **vData, int numFrames)
{
	Context *internalContext = (Context*) context;
	Dav1dData data;
	int getPictureResult;
	int result;
	int i;

	for (i = 0; i < numFrames; i += 1)
	{
		getPictureResult = dav1d_get_picture(internalContext->dav1dContext, &internalContext->currentPicture);

		while (getPictureResult == DAV1D_ERR(EAGAIN))
		{
			if (!INTERNAL_getNextPacket(internalContext))
			{
				/* ... unless there's nothing left for us to read. */
				internalContext->eof = 1;
				break;
			}

			data.data = internalContext->bitstreamData + internalContext->bitstreamIndex;
			data.sz = internalContext->currentOBUSize;
			data.ref = NULL;

			result = dav1d_send_data(internalContext->dav1dContext, &data);

			if (result != 0)
			{
				/* Something went wrong on data send! */
				return -1;
			}

			getPictureResult = dav1d_get_picture(internalContext->dav1dContext, &internalContext->currentPicture);
		}

		if (getPictureResult != 0)
		{
			/* Something went wrong on picture decode! */
			return -1;
		}
	}

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
	SDL_free(internalContext);
}
