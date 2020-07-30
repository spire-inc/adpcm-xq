////////////////////////////////////////////////////////////////////////////
//                           **** ADPCM-XQ ****                           //
//                  Xtreme Quality ADPCM Encoder/Decoder                  //
//                    Copyright (c) 2015 David Bryant.                    //
//                          All Rights Reserved.                          //
//      Distributed under the BSD Software License (see license.txt)      //
////////////////////////////////////////////////////////////////////////////

/****************************************************************************
 *                              INCLUDE FILES                               *
 ****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "adpcm-lib.h"

/****************************************************************************
 *                      PRIVATE TYPES and DEFINITIONS                       *
 ****************************************************************************/

#define CLIP(data, min, max) \
    if ((data) > (max)) data = max; \
    else if ((data) < (min)) data = min;

/****************************************************************************
 *                              PRIVATE DATA                                *
 ****************************************************************************/

static const uint16_t mStepTable[] =
{
    7, 8, 9, 10, 11, 12, 13, 14,
    16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};

static const int mIndexTable[] =
{
    -1, -1, -1, -1, 2, 4, 6, 8
};

/****************************************************************************
 *                             EXTERNAL DATA                                *
 ****************************************************************************/

/****************************************************************************
 *                     PRIVATE FUNCTION DECLARATIONS                        *
 ****************************************************************************/

static uint32_t minimum_error(adpcm_context_t *ctx, int32_t csample,
                              const int16_t *sample, int depth,
                              int *bestNibble, uint32_t *minError);
static uint8_t encode_sample (adpcm_context_t *ctx, const int16_t *sample,
                              int num_samples, int lookahead, uint32_t *error);
static uint16_t decode_sample (adpcm_context_t *ctx, const uint8_t nibble);

/****************************************************************************
 *                     EXPORTED FUNCTION DEFINITIONS                        *
 ****************************************************************************/

ADPCM_STATUS_T adpcm_encode_init(adpcm_context_t *ctx, const int16_t *inBuf,
                                 int inBufCount)
{
    if (!ctx)
    {
        return ADPCM_INVALID_PARAM;
    }

    ctx->pcmData = inBuf[0];
    ctx->index = 0;

    int avg_delta = 0;
    for (int i = inBufCount - 1; i > 0; i--)
    {
        int delta = abs(inBuf[i] - inBuf[i - 1]);

        avg_delta -= avg_delta / 8;
        avg_delta += delta;
    }
    avg_delta /= 8;

    for (int i = 0; i <= 88; i++)
    {
        if (i == 88 ||
            avg_delta < ((mStepTable[i] + mStepTable[i+1]) / 2))
        {
            ctx->index = i;
            break;
        }
    }

    return ADPCM_SUCCESS;
}

ADPCM_STATUS_T adpcm_decode_init(adpcm_context_t *ctx, int16_t pcm,
                                 int8_t index)
{
    if (!ctx || index < 0 || index > 88)
    {
        return ADPCM_INVALID_PARAM;
    }

    ctx->pcmData = pcm;
    ctx->index = index;

    return ADPCM_SUCCESS;
}

ADPCM_STATUS_T adpcm_encode(adpcm_context_t *ctx, uint8_t *outBuf,
                            const int16_t *inBuf, int inBufCount, int lookahead,
                            uint32_t *error)
{
    if (!ctx || !outBuf || !inBuf || inBufCount <= 0)
    {
        return ADPCM_INVALID_PARAM;
    }

    for (int i = 0; i < inBufCount; i++)
    {
        const int16_t *pcmbuf = &inBuf[i];
        const uint8_t nibble = encode_sample(ctx, pcmbuf, inBufCount - i,
                                             lookahead, error);

        if (i % 2 == 0)
        {
            outBuf[i / 2] = nibble;
        }
        else
        {
            outBuf[i / 2] |= nibble << 4;
        }
    }

    return ADPCM_SUCCESS;
}

ADPCM_STATUS_T adpcm_decode(adpcm_context_t *ctx, int16_t *outBuf,
                            const uint8_t *inBuf, int inBufCount)
{
    if (!ctx || !inBuf || inBufCount <= 0)
    {
        return ADPCM_INVALID_PARAM;
    }

    for (int i = 0; i < inBufCount; i++)
    {
        uint8_t nibble;

        if (i % 2 == 0)
        {
            nibble = inBuf[i / 2] & 0xF;
        }
        else
        {
            nibble = (inBuf[i / 2] >> 4) & 0xF;
        }

        int16_t pcm = decode_sample(ctx, nibble);

        if (outBuf)
        {
            outBuf[i] = pcm;
        }
    }

    return ADPCM_SUCCESS;
}

/****************************************************************************
 *                     PRIVATE FUNCTION DEFINITIONS                         *
 ****************************************************************************/

static uint32_t minimum_error(adpcm_context_t *ctx, int32_t csample,
                              const int16_t *sample, int depth,
                              int *bestNibble, uint32_t *minError)
{
    int32_t delta = csample - ctx->pcmData;
    adpcm_context_t localCtx = *ctx;
    int step = mStepTable[ctx->index];
    int trialDelta = (step >> 3);
    int nibble, nibble2;
    uint32_t minTotalError;

    if (delta < 0)
    {
        int mag = (-delta << 2) / step;
        nibble = 0x8 | (mag > 7 ? 7 : mag);
    }
    else
    {
        int mag = (delta << 2) / step;
        nibble = mag > 7 ? 7 : mag;
    }

    if (nibble & 1) trialDelta += (step >> 2);
    if (nibble & 2) trialDelta += (step >> 1);
    if (nibble & 4) trialDelta += step;
    if (nibble & 8) trialDelta = -trialDelta;

    localCtx.pcmData += trialDelta;
    CLIP(localCtx.pcmData, INT16_MIN, INT16_MAX);
    if (bestNibble) *bestNibble = nibble;
    minTotalError = abs(localCtx.pcmData - csample);
    if (minError) *minError = minTotalError;

    if (depth == 0)
    {
        return minTotalError;
    }

    localCtx.index += mIndexTable[nibble & 0x7];
    CLIP(localCtx.index, 0, 88);
    minTotalError += minimum_error(&localCtx, sample[1], sample + 1,
                                   depth - 1, NULL, NULL);

    for (nibble2 = 0; nibble2 <= 0xF; ++nibble2)
    {
        uint32_t error;

        if (nibble2 == nibble)
        {
            continue;
        }

        localCtx = *ctx;
        trialDelta = (step >> 3);

        if (nibble2 & 1) trialDelta += (step >> 2);
        if (nibble2 & 2) trialDelta += (step >> 1);
        if (nibble2 & 4) trialDelta += step;
        if (nibble2 & 8) trialDelta = -trialDelta;

        localCtx.pcmData += trialDelta;
        CLIP(localCtx.pcmData, INT16_MIN, INT16_MAX);

        error = abs(localCtx.pcmData - csample);

        if (error < minTotalError)
        {
            uint32_t totalError;

            localCtx.index += mIndexTable[nibble2 & 0x7];
            CLIP(localCtx.index, 0, 88);
            totalError = error + minimum_error(&localCtx, sample[1], sample + 1,
                                               depth - 1, NULL, NULL);

            if (totalError < minTotalError)
            {
                if (bestNibble) *bestNibble = nibble2;
                minTotalError = totalError;
                if (minError) *minError = error;
            }
        }
    }

    return minTotalError;
}

static uint8_t encode_sample(adpcm_context_t *ctx, const int16_t *sample,
                             int num_samples, int lookahead, uint32_t *error)
{
    int32_t csample = *sample;
    int depth = num_samples - 1, nibble;
    int step = mStepTable[ctx->index];
    int trialDelta = (step >> 3);
    uint32_t sampleError;

    if (depth > lookahead)
    {
        depth = lookahead;
    }

    minimum_error(ctx, csample, sample, depth, &nibble, &sampleError);
    *error += sampleError;

    if (nibble & 1) trialDelta += (step >> 2);
    if (nibble & 2) trialDelta += (step >> 1);
    if (nibble & 4) trialDelta += step;
    if (nibble & 8) trialDelta = -trialDelta;

    ctx->pcmData += trialDelta;
    ctx->index += mIndexTable[nibble & 0x7];
    CLIP(ctx->index, 0, 88);
    CLIP(ctx->pcmData, INT16_MIN, INT16_MAX);

    return nibble;
}

static uint16_t decode_sample(adpcm_context_t *ctx, const uint8_t nibble)
{
    int step = mStepTable[ctx->index];
    int delta = step >> 3;

    if (nibble & 1) delta += (step >> 2);
    if (nibble & 2) delta += (step >> 1);
    if (nibble & 4) delta += step;
    if (nibble & 8) delta = -delta;

    ctx->pcmData += delta;
    ctx->index += mIndexTable[nibble & 0x7];
    CLIP(ctx->index, 0, 88);
    CLIP(ctx->pcmData, INT16_MIN, INT16_MAX);

    return ctx->pcmData;
}
