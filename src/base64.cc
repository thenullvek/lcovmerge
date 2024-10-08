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

#include "base64.h"
#include <cassert>

static constexpr const unsigned char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static constexpr int kBase64Invs[] = {
    62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58,
    59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5,
    6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28,
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51
};

void Base64Encode(const uint8_t* in, size_t len, std::string* buf)
{
    assert(len > 0 && in && buf);
    size_t esize = len * 4 / 3 + 4;
    buf->reserve(esize);
    buf->clear();

    while (len >= 3) {
        buf->append(1, kBase64Chars[*in >> 2]);
        buf->append(1, kBase64Chars[((*in & 0x03) << 4) | (in[1] >> 4)]);
        buf->append(1, kBase64Chars[((in[1] & 0x0f) << 2) | (in[2] >> 6)]);
        buf->append(1, kBase64Chars[in[2] & 0x3f]);
        in += 3;
        len -= 3;
    }

    if (len > 0) {
        buf->append(1, kBase64Chars[*in >> 2]);
        if (len == 1) {
            buf->append(1, kBase64Chars[(in[0] & 0x03) << 4]);
            buf->append(1, '=');
        } else {
            buf->append(1, kBase64Chars[((in[0] & 0x03) << 4) | (in[1] >> 4)]);
            buf->append(1, kBase64Chars[(in[1] & 0x0f) << 2]);
        }
        buf->append(1, '=');
    }
}

static inline bool IsValidBase64Char(int ch)
{
    if (ch < 43 || ch > 122 || -1 == kBase64Invs[ch])
        return false;
    return true;
}

bool Base64Decode(const unsigned char* in, size_t inlen, uint8_t* out, size_t* outlen)
{
    assert(in && inlen > 0 && out && outlen && *outlen > 0);
    size_t count = 0, olen;

    // Validate the base64 string
    for (size_t i = 0; i < inlen; i++) {
        if (!IsValidBase64Char(in[i]))
            return false;
    }
    if (inlen % 4)
        return false;

    olen = inlen / 4 * 3;
    if (*outlen < olen)
        return false;

    uint8_t* p = out, block[4];
    const unsigned char* end = in + inlen;
    int pad = 0;
    while (in < end) {
        if (*in == '=')
            pad++;
        block[count++] = kBase64Invs[*in - 43];
        if (count == 4) {
            count = 0;
            *p++ = (block[0] << 2) | (block[1] >> 4);
            *p++ = (block[1] << 4) | (block[2] >> 2);
            *p++ = (block[2] << 6) | block[3];
            if (pad) {
                switch (pad) {
                    case 2: p--;
                    case 1: p--;
                        break;
                    default:
                        // invalid padding
                        return false;
                }
            }
        }
    }
    *outlen = p - out;
    return true;
}
