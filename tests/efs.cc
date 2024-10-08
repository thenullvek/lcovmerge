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

#include <cassert>
#include <cstring>

#include "efs.h"

EmuFilesystem::Status EmuFilesystem::ReadFile(const char* path, std::string* content, std::string* err)
{
    auto it = files_.find(path);
    err->clear();
    if (it != files_.cend()) {
        FileEntry* entry = &it->second;
        if (entry->errno_) {
            content->clear();
            err->assign(strerror(entry->errno_));
            return IO_ERROR;
        }
        content->assign(entry->content_);
        return SUCCESS;
    }
    err->assign(strerror(ENOENT));
    return NOT_FOUND;
}

void EmuFilesystem::PushFile(const std::string& path, const std::string& content)
{
    auto it = files_.find(path);
    if (it != files_.cend()) {
        it->second.content_.assign(content);
        it->second.errno_ = 0;
    } else {
        files_.emplace(path, FileEntry{content});
        it = files_.end();
        it--;
        current_file_ = &it->second;
    }
}

void EmuFilesystem::SetIOError(int errno)
{
    assert(current_file_ != nullptr);
    current_file_->errno_ = errno;
}

