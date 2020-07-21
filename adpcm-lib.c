////////////////////////////////////////////////////////////////////////////
//                           **** ADPCM-XQ ****                           //
//                  Xtreme Quality ADPCM Encoder/Decoder                  //
//                    Copyright (c) 2015 David Bryant.                    //
//                          All Rights Reserved.                          //
//      Distributed under the BSD Software License (see license.txt)      //
////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>

#include "adpcm-lib.h"

/* This module encodes and decodes 4-bit ADPCM (DVI/IMA variant). */

#define CLIP(data, min, max) \
if ((data) > (max)) data = max; \
else if ((data) < (min)) data = min;

/* Step table */
static const uint16_t step_table[89] = {
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

/* Step index tables */
static const int index_table[] = {
    /* adpcm data size is 4 */
    -1, -1, -1, -1, 2, 4, 6, 8
};

static double minimum_error (adpcm_context_t *pcnxt, int32_t csample, const int16_t *sample, int depth, int *best_nibble)
{
    int32_t delta = csample - pcnxt->pcmdata;
    int32_t pcmdata = pcnxt->pcmdata;
    int8_t index = pcnxt->index;
    int step = step_table[pcnxt->index];
    int trial_delta = (step >> 3);
    int nibble, nibble2;
    double min_error;

    if (delta < 0) {
        int mag = (-delta << 2) / step;
        nibble = 0x8 | (mag > 7 ? 7 : mag);
    }
    else {
        int mag = (delta << 2) / step;
        nibble = mag > 7 ? 7 : mag;
    }

    if (nibble & 1) trial_delta += (step >> 2);
    if (nibble & 2) trial_delta += (step >> 1);
    if (nibble & 4) trial_delta += step;
    if (nibble & 8) trial_delta = -trial_delta;

    pcmdata += trial_delta;
    CLIP(pcmdata, -32768, 32767);
    if (best_nibble) *best_nibble = nibble;
    min_error = (double) (pcmdata - csample) * (pcmdata - csample);

    if (depth) {
        index += index_table[nibble & 0x07];
        CLIP(index, 0, 88);
        min_error += minimum_error (pcnxt, sample [1], sample + 1, depth - 1, NULL);
    }
    else
        return min_error;

    for (nibble2 = 0; nibble2 <= 0xF; ++nibble2) {
        double error;

        if (nibble2 == nibble)
            continue;

        pcmdata = pcnxt->pcmdata;
        index = pcnxt->index;
        trial_delta = (step >> 3);

        if (nibble2 & 1) trial_delta += (step >> 2);
        if (nibble2 & 2) trial_delta += (step >> 1);
        if (nibble2 & 4) trial_delta += step;
        if (nibble2 & 8) trial_delta = -trial_delta;

        pcmdata += trial_delta;
        CLIP(pcmdata, -32768, 32767);

        error = (double) (pcmdata - csample) * (pcmdata - csample);

        if (error < min_error) {
            index += index_table[nibble2 & 0x07];
            CLIP(index, 0, 88);
            error += minimum_error (pcnxt, sample [1], sample + 1, depth - 1, NULL);

            if (error < min_error) {
                if (best_nibble) *best_nibble = nibble2;
                min_error = error;
            }
        }
    }

    return min_error;
}

static uint8_t encode_sample (adpcm_context_t *pcnxt, const int16_t *sample, int num_samples, int lookahead)
{
    int32_t csample = *sample;
    int depth = num_samples - 1, nibble;
    int step = step_table[pcnxt->index];
    int trial_delta = (step >> 3);

    if (depth > lookahead)
        depth = lookahead;

    minimum_error (pcnxt, csample, sample, depth, &nibble);

    if (nibble & 1) trial_delta += (step >> 2);
    if (nibble & 2) trial_delta += (step >> 1);
    if (nibble & 4) trial_delta += step;
    if (nibble & 8) trial_delta = -trial_delta;

    pcnxt->pcmdata += trial_delta;
    pcnxt->index += index_table[nibble & 0x07];
    CLIP(pcnxt->index, 0, 88);
    CLIP(pcnxt->pcmdata, -32768, 32767);

    return nibble;
}

static uint16_t decode_sample (adpcm_context_t *pcnxt, const uint8_t nibble)
{
    int step = step_table [pcnxt->index], delta = step >> 3;

    if (nibble & 1) delta += (step >> 2);
    if (nibble & 2) delta += (step >> 1);
    if (nibble & 4) delta += step;
    if (nibble & 8) delta = -delta;

    pcnxt->pcmdata += delta;
    pcnxt->index += index_table [nibble & 0x7];
    CLIP(pcnxt->index, 0, 88);
    CLIP(pcnxt->pcmdata, -32768, 32767);

    return pcnxt->pcmdata;
}

/* Initialize ADPCM codec context for encoding.
 *
 * Parameters:
 *  pcnxt       context to initialize
 *  pcm0        first sample used as reference
 *  pcm1        second sample used to calculate initial index
 */

ADPCM_STATUS_T adpcm_encode_init (adpcm_context_t *pcnxt, int16_t pcm0, int16_t pcm1)
{
    if (!pcnxt) {
        return ADPCM_INVALID_PARAM;
    }

    pcnxt->pcmdata = pcm0;
    pcnxt->index = 0;

    int32_t delta = pcm1 - pcm0;

    if (delta < 0) {
        delta = -delta;
    }

    for (int i = 0; i <= 88; i++) {
        if (i == 88 || delta < ((int32_t) step_table [i] + step_table [i+1]) / 2) {
            pcnxt->index = i;
            break;
        }
    }

    return ADPCM_SUCCESS;
}

/* Initialize ADPCM codec context for decoding.
 *
 * Parameters:
 *  pcnxt       context to initialize
 *  pcm         initial sample
 *  index       initial index
 */

ADPCM_STATUS_T adpcm_decode_init (adpcm_context_t *pcnxt, int16_t pcm, int8_t index)
{
    if (!pcnxt || index < 0 || index > 88) {
        return ADPCM_INVALID_PARAM;
    }

    pcnxt->pcmdata = pcm;
    pcnxt->index = index;

    return ADPCM_SUCCESS;
}

/* Encode 16-bit PCM data into 4-bit ADPCM.
 *
 * Parameters:
 *  pcnxt           the context initialized by adpcm_init_context()
 *  outbuf          destination ADPCM buffer
 *  outbufsize      pointer to variable where the number of bytes written
 *                   will be stored
 *  inbuf           source PCM samples
 *  inbufcount      number of PCM samples provided
 *  lookahead       lookahead amount
 */

ADPCM_STATUS_T adpcm_encode (adpcm_context_t *pcnxt, uint8_t *outbuf, size_t *outbufsize, const int16_t *inbuf, int inbufcount, int lookahead)
{
    if (!pcnxt || !outbuf || !outbufsize || !inbuf || inbufcount <= 0) {
        return ADPCM_INVALID_PARAM;
    }

    for (int i = 0; i < inbufcount; i++) {
        const int16_t *pcmbuf = &inbuf[i];
        const uint8_t nibble = encode_sample (pcnxt, pcmbuf, inbufcount - i, lookahead);

        if (i % 2 == 0) {
            outbuf[i / 2] = nibble;
        }
        else {
            outbuf[i / 2] |= nibble << 4;
        }
    }

    *outbufsize = (inbufcount + 1) / 2;

    return ADPCM_SUCCESS;
}

/* Decode 4-bit ADPCM data into 16-bit PCM.
 *
 * Parameters:
 *  pcnxt           the context initialized by adpcm_init_context()
 *  outbuf          destination for PCM samples
 *  outbufsize      pointer to variable where the number of bytes written
 *                   will be stored
 *  inbuf           source ADPCM buffer
 *  inbufcount      number of ADPCM samples provided
 */ 

ADPCM_STATUS_T adpcm_decode (adpcm_context_t *pcnxt, int16_t *outbuf, size_t *outbufsize, const uint8_t *inbuf, int inbufcount)
{
    if (!pcnxt || !outbuf || !outbufsize || !inbuf || inbufcount <= 0) {
        return ADPCM_INVALID_PARAM;
    }

    for (int i = 0; i < inbufcount; i++) {
        uint8_t nibble;

        if (i % 2 == 0) {
            nibble = inbuf[i / 2] & 0x7;
        }
        else {
            nibble = (inbuf[i / 2] >> 4) & 0x7;
        }

        outbuf[i] = decode_sample (pcnxt, nibble);
    }

    *outbufsize = inbufcount * 2;

    return ADPCM_SUCCESS;
}

