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
#pragma once

#include <cstdint>
#include <string>

constexpr size_t Base64DecodeSize(size_t inlen) {
    return inlen * 3 / 4;
}
void Base64Encode(const uint8_t* data, size_t len, std::string* buf);
bool Base64Decode(const char* in, size_t inlen, uint8_t* out, size_t outlen);

