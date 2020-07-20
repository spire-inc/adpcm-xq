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

/* This module encodes and decodes 4-bit ADPCM (DVI/IMA varient). ADPCM data is divided
 * into independently decodable blocks that can be relatively small. The most common
 * configuration is to store 505 samples into a 256 byte block, although other sizes are
 * permitted as long as the number of samples is one greater than a multiple of 8.
 */

/********************************* 4-bit ADPCM encoder ********************************/

#define CLIP(data, min, max) \
if ((data) > (max)) data = max; \
else if ((data) < (min)) data = min;

/* step table */
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

/* step index tables */
static const int index_table[] = {
    /* adpcm data size is 4 */
    -1, -1, -1, -1, 2, 4, 6, 8
};

/* Initialize ADPCM encoder context. Note that even though an ADPCM encoder
 * could be set up to encode frames independently, we use a context so that
 * we can use previous data to improve quality (this encoder might not be
 * optimal for encoding independent frames).
 */

void adpcm_init_context (struct adpcm_context *pcnxt, int lookahead, int32_t initial_delta)
{
    int i;

    pcnxt->pcmdata = 0;
    pcnxt->index = 0;
    pcnxt->lookahead = lookahead;

    // given the supplied initial deltas, search for and store the closest index

    for (i = 0; i <= 88; i++)
        if (i == 88 || initial_delta < ((int32_t) step_table [i] + step_table [i+1]) / 2) {
            pcnxt->index = i;
            break;
        }
}

static double minimum_error (struct adpcm_context *pcnxt, int32_t csample, const int16_t *sample, int depth, int *best_nibble)
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

static uint8_t encode_sample (struct adpcm_context *pcnxt, const int16_t *sample, int num_samples)
{
    int32_t csample = *sample;
    int depth = num_samples - 1, nibble;
    int step = step_table[pcnxt->index];
    int trial_delta = (step >> 3);

    if (depth > pcnxt->lookahead)
        depth = pcnxt->lookahead;

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

static void encode_chunks (struct adpcm_context *pcnxt, uint8_t **outbuf, size_t *outbufsize, const int16_t **inbuf, int inbufcount)
{
    const int16_t *pcmbuf;
    int chunks, i;

    chunks = (inbufcount - 1) / 8;
    *outbufsize += (chunks * 4);

    while (chunks--)
    {
        pcmbuf = *inbuf;

        for (i = 0; i < 4; i++) {
            **outbuf = encode_sample (pcnxt, pcmbuf, chunks * 8 + (3 - i) * 2 + 2);
            pcmbuf++;
            **outbuf |= encode_sample (pcnxt, pcmbuf, chunks * 8 + (3 - i) * 2 + 1) << 4;
            pcmbuf++;
            (*outbuf)++;
        }

        *inbuf += 8;
    }
}

/* Encode a block of 16-bit PCM data into 4-bit ADPCM.
 *
 * Parameters:
 *  pcnxt           the context initialized by adpcm_init_context()
 *  outbuf          destination buffer
 *  outbufsize      pointer to variable where the number of bytes written
 *                   will be stored
 *  inbuf           source PCM samples
 *  inbufcount      number of composite PCM samples provided (note: this is
 *                   the total number of 16-bit samples)
 *
 * Returns 1 (for success as there is no error checking)
 */

int adpcm_encode_block (struct adpcm_context *pcnxt, uint8_t *outbuf, size_t *outbufsize, const int16_t *inbuf, int inbufcount)
{
    *outbufsize = 0;

    if (!inbufcount)
        return 1;

    init_pcmdata = *inbuf++;
    outbuf[0] = pcnxt->pcmdata;
    outbuf[1] = pcnxt->pcmdata >> 8;
    outbuf[2] = pcnxt->index;
    outbuf[3] = 0;

    outbuf += 4;
    *outbufsize += 4;

    encode_chunks (pcnxt, &outbuf, outbufsize, &inbuf, inbufcount);

    return 1;
}

/********************************* 4-bit ADPCM decoder ********************************/

/* Decode the block of ADPCM data into PCM. This requires no context because ADPCM blocks
 * are indeppendently decodable. This assumes that a single entire block is always decoded;
 * it must be called multiple times for multiple blocks and cannot resume in the middle of a
 * block.
 *
 * Parameters:
 *  outbuf          destination for interleaved PCM samples
 *  inbuf           source ADPCM block
 *  inbufsize       size of source ADPCM block
 *
 * Returns number of converted composite samples
 */ 

int adpcm_decode_block (int16_t *outbuf, const uint8_t *inbuf, size_t inbufsize)
{
    int samples = 1, chunks;
    int32_t pcmdata;
    int8_t index;

    if (inbufsize < 4)
        return 0;

    *outbuf++ = pcmdata = (int16_t) (inbuf [0] | (inbuf [1] << 8));
    index = inbuf [2];

    if (index < 0 || index > 88 || inbuf [3])     // sanitize the input a little...
        return 0;

    inbufsize -= 4;
    inbuf += 4;

    chunks = inbufsize / 4;
    samples += chunks * 8;

    while (chunks--) {
        int i;

        for (i = 0; i < 4; ++i) {
            int step = step_table [index], delta = step >> 3;

            if (*inbuf & 1) delta += (step >> 2);
            if (*inbuf & 2) delta += (step >> 1);
            if (*inbuf & 4) delta += step;
            if (*inbuf & 8) delta = -delta;

            pcmdata += delta;
            index += index_table [*inbuf & 0x7];
            CLIP(index, 0, 88);
            CLIP(pcmdata, -32768, 32767);
            outbuf [i * 2] = pcmdata;

            step = step_table [index], delta = step >> 3;

            if (*inbuf & 0x10) delta += (step >> 2);
            if (*inbuf & 0x20) delta += (step >> 1);
            if (*inbuf & 0x40) delta += step;
            if (*inbuf & 0x80) delta = -delta;

            pcmdata += delta;
            index += index_table [(*inbuf >> 4) & 0x7];
            CLIP(index, 0, 88);
            CLIP(pcmdata, -32768, 32767);
            outbuf [(i * 2 + 1)] = pcmdata;

            inbuf++;
        }

        outbuf += 8;
    }

    return samples;
}

