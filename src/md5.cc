// Copyright 2023 Weihao Feng. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/*
 * Derived from the RSA Data Security, Inc. MD5 Message-Digest Algorithm
 * and modified slightly to be functionally identical but condensed into control structures.
 */
#include <cstdlib>
#include <cstring>

#include "md5.h"

/*
 * Bit-manipulation functions defined by the MD5 algorithm
 */
#define F(X, Y, Z) ((X & Y) | (~X & Z))
#define G(X, Y, Z) ((X & Z) | (Y & ~Z))
#define H(X, Y, Z) (X ^ Y ^ Z)
#define I(X, Y, Z) (Y ^ (X | ~Z))

static constexpr const uint32_t S[] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

static constexpr const uint32_t K[] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

/*
 * Padding used to make the size (in bits) of the input congruent to 448 mod 512
 */
static constexpr const uint8_t PADDING[] = {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static inline uint32_t rotateleft(uint32_t x, uint32_t n)
{
    return (x << n) | (x >> (32 - n));
}

void MD5Hash::Init()
{
    size_ = 0;
    buffer_[0] = A;
    buffer_[1] = B;
    buffer_[2] = C;
    buffer_[3] = D;
}

void MD5Hash::Update(const char* in, size_t len)
{
    uint32_t input[16];
    unsigned int offset = size_ % 64;
    size_ += (uint64_t)len;

    // Copy each byte in input_buffer into the next space in our context input
    for(unsigned int i = 0; i < len; ++i){
        input_[offset++] = static_cast<uint8_t>(*(in + i));

        // If we've filled our context input, copy it into our local array input
        // then reset the offset to 0 and fill in a new buffer.
        // Every time we fill out a chunk, we run it through the algorithm
        // to enable some back and forth between cpu and i/o
        if(offset % 64 == 0){
            for(unsigned int j = 0; j < 16; ++j){
                // Convert to little-endian
                // The local variable `input` our 512-bit chunk separated into 32-bit words
                // we can use in calculations
                input[j] = (uint32_t)(input_[(j * 4) + 3]) << 24 |
                           (uint32_t)(input_[(j * 4) + 2]) << 16 |
                           (uint32_t)(input_[(j * 4) + 1]) <<  8 |
                           (uint32_t)(input_[(j * 4)]);
            }
            Step(input);
            offset = 0;
        }
    }
}

void MD5Hash::Finalize(uint8_t* result)
{
    uint32_t input[16];
    unsigned int offset = size_ % 64;
    unsigned int padding_length = offset < 56 ? 56 - offset : (56 + 64) - offset;

    // Fill in the padding and undo the changes to size that resulted from the update
    Update(reinterpret_cast<const char*>(PADDING), padding_length);
    size_ -= (uint64_t)padding_length;

    // Do a final update (internal to this function)
    // Last two 32-bit words are the two halves of the size (converted from bytes to bits)
    for(unsigned int j = 0; j < 14; ++j){
        input[j] = (uint32_t)(input_[(j * 4) + 3]) << 24 |
                   (uint32_t)(input_[(j * 4) + 2]) << 16 |
                   (uint32_t)(input_[(j * 4) + 1]) <<  8 |
                   (uint32_t)(input_[(j * 4)]);
    }
    input[14] = (uint32_t)(size_ * 8);
    input[15] = (uint32_t)((size_ * 8) >> 32);

    Step(input);

    // Move the result into digest (convert from little-endian)
    for(unsigned int i = 0; i < 4; ++i){
        result[(i * 4) + 0] = static_cast<uint8_t>((buffer_[i] & 0x000000FF));
        result[(i * 4) + 1] = static_cast<uint8_t>((buffer_[i] & 0x0000FF00) >>  8);
        result[(i * 4) + 2] = static_cast<uint8_t>((buffer_[i] & 0x00FF0000) >> 16);
        result[(i * 4) + 3] = static_cast<uint8_t>((buffer_[i] & 0xFF000000) >> 24);
    }
}

bool MD5Hash::Base64ToMD5(const char *base64, uint8_t *buf, size_t base64_len)
{
    return true;
}

void MD5Hash::Step(uint32_t* input)
{
    uint32_t AA = buffer_[0];
    uint32_t BB = buffer_[1];
    uint32_t CC = buffer_[2];
    uint32_t DD = buffer_[3];

    uint32_t E;

    unsigned int j;

    for(unsigned int i = 0; i < 64; ++i){
        switch(i / 16){
            case 0:
                E = F(BB, CC, DD);
                j = i;
                break;
            case 1:
                E = G(BB, CC, DD);
                j = ((i * 5) + 1) % 16;
                break;
            case 2:
                E = H(BB, CC, DD);
                j = ((i * 3) + 5) % 16;
                break;
            default:
                E = I(BB, CC, DD);
                j = (i * 7) % 16;
                break;
        }

        uint32_t temp = DD;
        DD = CC;
        CC = BB;
        BB = BB + rotateleft(AA + E + K[i] + input[j], S[i]);
        AA = temp;
    }

    buffer_[0] += AA;
    buffer_[1] += BB;
    buffer_[2] += CC;
    buffer_[3] += DD;
}

