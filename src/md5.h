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

#ifndef LCOVMERGE_MD5_H_
#define LCOVMERGE_MD5_H_

#include <cstddef>
#include <cstdint>

struct MD5Hash {
    enum { Length = 16, Base64Length = 22 };
    MD5Hash() { Init(); }

    void Init();
    void Update(const char* in, size_t len);
    void Finalize(uint8_t* result);

    static bool Base64ToMD5(const char* base64, uint8_t* buf, size_t base64_len);
    static void ToBase64(const uint8_t* hash, char* output);

private:
    enum { A = 0x67452301, B = 0xefcdab89, C = 0x98badcfe, D = 0x10325476 };
    void Step(uint32_t* input);

    uint64_t size_;
    uint32_t buffer_[4];
    uint8_t input_[64];
};

#endif // LCOVMERGE_MD5_H_

