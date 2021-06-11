/*
ABOUT:
    Adaptive run length encoding of a byte buffer

VERSION:
    1.00 - (2021-02-06) Initial commit

LICENSE:

    The MIT License (MIT)

    Copyright (c) 2021 Mathias Westerdahl

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.


DISCLAIMER:

    This software is supplied "AS IS" without any warranties and support

*/


/* Examples:

// ask for the required size for the output buffer
uint32_t encodedsize = jc_rle_encode(inputsize, input, 0);

uint8_t* encoded = (uint8_t*)malloc(encodedsize);
jc_rle_encode(inputsize, input, encoded);



// ask for the required size for the output buffer
uint32_t decodedsize = jc_rle_decode(inputsize, input, 0);

uint8_t* encoded = (uint8_t*)malloc(encodedsize);
jc_rle_encode(inputsize, input, encoded);
*/

#ifndef JC_RLE_H
#define JC_RLE_H

#include <stdint.h>

/**
* @name jc_rle_encode
* @param size   Size of the input buffer
* @param in     The input data to be encoded
* @param out    Receives the encoded data. May be null
*/
extern uint32_t jc_rle_encode(uint32_t size, const uint8_t* in, uint8_t* out);

/**
* @name jc_rle_decode
* @param size   Size of the input buffer
* @param in     The input data to be encoded
* @param out    Receives the encoded data. May be null
*/
extern uint32_t jc_rle_decode(uint32_t size, const uint8_t* in, uint8_t* out);

/**
* Outputs the encoded data in tuples of "(length,payload) ..." for easier debugging
* @name jc_rle_debug_print
* @param size       Size of the input buffer
* @param encoded    The encoded data
*/
extern void jc_rle_debug_print(uint32_t size, const uint8_t* encoded);


#endif // JC_RLE_H
/* ******************************************************************************************************** */

#ifdef JC_RLE_IMPLEMENTATION
#undef JC_RLE_IMPLEMENTATION

#ifndef JC_RLE_PRINTF
    #include <stdio.h>
    #define JC_RLE_PRINTF printf
#endif

uint32_t jc_rle_encode(uint32_t size, const uint8_t* in, uint8_t* out)
{
    uint32_t encoded_bytes = 0;

    const uint8_t* p = in;
    const uint8_t* in_end = in+size;
    while (p < in_end) {
        const uint8_t* _p = p;
        uint8_t start = *p;

        // fast forward the pointer and count the length of the run
        // we can store maximum run length of 127, since we also store negative values
        uint8_t length = 0;
        while (*p == start && p < in_end && length < 127) {
            ++p;
            ++length;
        }

        if (length >= 2) {
            // At this point we have a run of either rle data
            if (out) {
                *out++ = length;
                *out++ = start;
            }
            encoded_bytes += 2;
            continue;
        }

        p = _p; // reset pointer

        uint8_t* _out = out; // We need to write the length afterwards, so we store the pointer for easy access
        if (out) {
            ++out;
        }

        // fast forward the pointer and count the length of the run
        // we can store maximum run length of 127, since we also store negative values
        // since this is a "raw output" we output directly to the output buffer
        length = 0;
        while (p < in_end && length < 127) {
            // check if there's a new run starting
            if (p+1 < in_end) {
                if (*p == *(p+1))
                    break;
            }

            if (out) {
                *out++ = *p;
            }
            ++p;
            ++length;
        }
        if (out) {
            *_out = length + 127; // make it over flow into a negative number
        }
        encoded_bytes += 1 + length;
    }

    return encoded_bytes;
}


uint32_t jc_rle_decode(uint32_t size, const uint8_t* in, uint8_t* out)
{
    uint32_t nwritten = 0;
    const uint8_t* in_end = in + size;
    while (in < in_end)
    {
        uint8_t c = *in++;
        uint32_t length = c > 127 ? c-127 : c;

        if (c > 127) { // the data should be copied as-is
            if (out) {
                for (uint32_t i = 0; i < length; ++i) {
                    *out++ = in[i];
                }
            }
            in += length;
        }
        else { // we have rle data
            if (out) {
                for (uint32_t i = 0; i < length; ++i) {
                    *out++ = *in;
                }
            }
            in++;
        }
        nwritten += length;
    }
    return nwritten;
}


void jc_rle_debug_print(uint32_t bufferlen, const uint8_t* buffer)
{
#if defined(JC_RLE_DEBUG)
    JC_RLE_PRINTF("  raw: ");
    for (uint32_t i = 0; i < bufferlen; ++i)
    {
        JC_RLE_PRINTF("%02x", buffer[i]);
    }
    JC_RLE_PRINTF("  size: %u\n", bufferlen);
#endif

    JC_RLE_PRINTF("  encoded: ");


    for (int32_t i = 0; i < (int32_t)bufferlen;)
    {
        int32_t length = (int32_t)(buffer[i] > 127 ? buffer[i]-127 : buffer[i]);
        int32_t signedlength = buffer[i] > 127 ? -length : length;

        JC_RLE_PRINTF("(%d, ", buffer[i]>127?-length:length);

        if (signedlength < 0) {
            for (int j = 0, k = i+1; j < length && k < (int32_t)bufferlen; ++j, ++k)
            {
                uint8_t c = buffer[k];
                if (c < 127)
                    JC_RLE_PRINTF("%c", c);
                else
                    JC_RLE_PRINTF("\\x%02x", c);
            }
        } else {
            uint8_t c = buffer[i+1];
            if (c < 127)
                JC_RLE_PRINTF("%c", c);
            else
                JC_RLE_PRINTF("\\x%02x", c);
        }

        if (signedlength < 0)
            i += 1 + length;
        else
            i += 1 + 1;

        JC_RLE_PRINTF(") ");
    }
    JC_RLE_PRINTF("  size: %u\n", bufferlen);
}


#endif // JC_RLE_IMPLEMENTATION

