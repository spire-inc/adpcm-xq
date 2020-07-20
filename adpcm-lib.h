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

struct adpcm_context {
    int32_t pcmdata;                        // current PCM value
    int8_t index;                           // current index into step size table
    int lookahead;
};

void adpcm_init_context (struct adpcm_context *pcnxt, int lookahead, int32_t initial_delta);
int adpcm_encode_block (struct adpcm_context *pcnxt, uint8_t *outbuf, size_t *outbufsize, const int16_t *inbuf, int inbufcount);
int adpcm_decode_block (int16_t *outbuf, const uint8_t *inbuf, size_t inbufsize);

#endif /* ADPCMLIB_H_ */
