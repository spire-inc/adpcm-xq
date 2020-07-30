////////////////////////////////////////////////////////////////////////////
//                           **** ADPCM-XQ ****                           //
//                  Xtreme Quality ADPCM Encoder/Decoder                  //
//                    Copyright (c) 2015 David Bryant.                    //
//                          All Rights Reserved.                          //
//      Distributed under the BSD Software License (see license.txt)      //
////////////////////////////////////////////////////////////////////////////

#ifndef ADPCMLIB_H_
#define ADPCMLIB_H_

/****************************************************************************
 *                              INCLUDE FILES                               *
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 *                     EXPORTED TYPES and DEFINITIONS                       *
 ****************************************************************************/

typedef enum
{
    ADPCM_SUCCESS,
    ADPCM_INVALID_PARAM,
} ADPCM_STATUS_T;

typedef struct
{
    int32_t pcmData;  // Current PCM value
    int8_t index;     // Current index into step size table
} adpcm_context_t;

/****************************************************************************
 *                              EXPORTED DATA                               *
 ****************************************************************************/

/****************************************************************************
 *                     EXPORTED FUNCTION DECLARATIONS                       *
 ****************************************************************************/

/* Initialize ADPCM codec context for encoding.
 *
 * Parameters:
 *  ctx             context to initialize
 *  inBuf           source PCM samples for index calculation
 *  inBufCount      number of PCM samples provided
 */
ADPCM_STATUS_T adpcm_encode_init(adpcm_context_t *ctx, const int16_t *inBuf,
                                 int inBufCount);

/* Initialize ADPCM codec context for decoding.
 *
 * Parameters:
 *  ctx         context to initialize
 *  pcm         initial sample
 *  index       initial index
 */
ADPCM_STATUS_T adpcm_decode_init(adpcm_context_t *ctx, int16_t pcm,
                                 int8_t index);

/* Encode 16-bit PCM data into 4-bit ADPCM.
 *
 * Parameters:
 *  ctx             the context initialized by adpcm_init_context()
 *  outBuf          destination ADPCM buffer
 *  inBuf           source PCM samples
 *  inBufCount      number of PCM samples provided
 *  lookahead       lookahead depth
 *  error           accumulated error metric to update
 */
ADPCM_STATUS_T adpcm_encode(adpcm_context_t *ctx, uint8_t *outBuf,
                            const int16_t *inBuf, int inBufCount, int lookahead,
                            uint32_t *error);

/* Decode 4-bit ADPCM data into 16-bit PCM.
 *
 * Parameters:
 *  ctx             the context initialized by adpcm_init_context()
 *  outbuf          destination for PCM samples (can be NULL)
 *  inBuf           source ADPCM buffer
 *  inBufCount      number of ADPCM samples provided
 */
ADPCM_STATUS_T adpcm_decode(adpcm_context_t *ctx, int16_t *outbuf,
                            const uint8_t *inBuf, int inBufCount);

#endif // ADPCMLIB_H_
