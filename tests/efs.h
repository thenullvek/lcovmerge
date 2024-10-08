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
#pragma once

#include "../src/filesystem.h"
#include <map>
#include <string>

struct EmuFilesystem : public IFilesystem {

    Status ReadFile(const char* path, std::string* content, std::string* err) override;

    void PushFile(const std::string& path, const std::string& content);
    void SetIOError(int errno);

private:
    struct FileEntry {
        FileEntry(const std::string& content) : content_(content), errno_(0) {}
        std::string content_;
        int errno_;
    };

    FileEntry* current_file_ = nullptr;
    std::map<std::string, FileEntry> files_;
};

