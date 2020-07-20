////////////////////////////////////////////////////////////////////////////
//                           **** ADPCM-XQ ****                           //
//                  Xtreme Quality ADPCM Encoder/Decoder                  //
//                    Copyright (c) 2015 David Bryant.                    //
//                          All Rights Reserved.                          //
//      Distributed under the BSD Software License (see license.txt)      //
////////////////////////////////////////////////////////////////////////////

#ifndef ADPCMLIB_H_
#define ADPCMLIB_H_

#include <stdint.h>

typedef enum {
    ADPCM_SUCCESS,
    ADPCM_INVALID_PARAM,
} ADPCM_STATUS_T;

typedef struct {
    int32_t pcmdata;                        // Current PCM value
    int8_t index;                           // Current index into step size table
    int lookahead;                          // Ignored when decoding
} adpcm_context_t;

ADPCM_STATUS_T adpcm_init_context (adpcm_context_t *pcnxt, int16_t pcm, int lookahead, int32_t initial_delta);
ADPCM_STATUS_T adpcm_encode (adpcm_context_t *pcnxt, uint8_t *outbuf, size_t *outbufsize, const int16_t *inbuf, int inbufcount);
ADPCM_STATUS_T adpcm_decode (adpcm_context_t *pcnxt, int16_t *outbuf, size_t *outbufsize, const uint8_t *inbuf, int inbufcount);

#endif /* ADPCMLIB_H_ */
