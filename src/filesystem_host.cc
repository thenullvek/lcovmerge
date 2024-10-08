// Copyright 2024 Weihao Feng. All Rights Reserved.
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

#include "filesystem_host.h"
#include "filesystem.h"

#include <cstdint>
#include <sys/stat.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

HostFilesystem::Status HostFilesystem::ReadFile(const char* path, std::string* content, std::string* err)
{
    assert(path != nullptr && content != nullptr && err != nullptr);
    FILE* fp = fopen(path, "rb");

    err->clear();
    content->clear();
    if (!fp) {
        err->assign(strerror(errno));
        return errno == ENOENT ? NOT_FOUND : IO_ERROR;
    }

    struct stat sb;
    if (-1 == fstat(fileno(fp), &sb)) {
        err->assign(strerror(errno));
        fclose(fp);
        return IO_ERROR;
    }
    content->reserve(sb.st_size);

    char buffer[UINT16_MAX];
    size_t rlen;
    while (!feof(fp) && (rlen = fread(buffer, 1, sizeof(buffer), fp)))
        content->append(buffer, rlen);
    if (ferror(fp)) {
        err->assign(strerror(errno));
        fclose(fp);
        return IO_ERROR;
    }

    fclose(fp);
    return SUCCESS;
}

