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
void adpcm_encode (struct adpcm_context *pcnxt, uint8_t *outbuf, size_t *outbufsize, const int16_t *inbuf, int inbufcount);
void adpcm_decode (struct adpcm_context *pcnxt, int16_t *outbuf, size_t *outbufsize, const uint8_t *inbuf, int inbufcount);

#endif /* ADPCMLIB_H_ */
