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

#include <gtest/gtest.h>
#include <string>

#include "efs.h"
#define LCOVMERGE_DEFINITION_ONLY
#include "../src/lcovmerge.cc"

static void SetupAndVerifyFile(EmuFilesystem* efs, const char* testname,
                               const std::string& fname, const char** linedata, size_t lines)
{
    SourceFileInfo sf(fname);
    std::string err;
    std::string content;

    for (size_t i = 0; i < lines; i++)
        content += linedata[i];
    efs->PushFile(fname, content);

    EXPECT_TRUE(sf.LoadLineMap(efs, &err)) << testname << ": Linemap should load without error";
    EXPECT_TRUE(err.empty()) << testname << ": error message must be empty";
    EXPECT_TRUE(sf.IsLineDataAvailable());
    EXPECT_TRUE(sf.IsLineNumberInRange(lines));
    EXPECT_FALSE(sf.IsLineNumberInRange(lines + 1));
    EXPECT_FALSE(sf.IsLineNumberInRange(0));

    for (int i = 1; i <= lines; i++) {
        auto line = sf.ReadLineData(i, false);
        EXPECT_EQ(line.compare(linedata[i - 1]), 0) << testname << ": line " << i << " mismatch";
    }
}

TEST(LineMapTest, EmptyLines)
{
    EmuFilesystem efs;
    const char* lines[] = {
        "\n",
        "\n",
        "\r\n",
        "\n",
    };
    SetupAndVerifyFile(&efs, "EmptyLines", "/test.c", lines, sizeof(lines)/sizeof(*lines));
}

TEST(LineMapTest, AbnormalNewlines)
{
    EmuFilesystem efs;
    const char* lines[] = { "#include abcdefg\n",
                             "\r\v\t\n",
                             "tttuuu sshhd\n",
                             "388477" };
    SetupAndVerifyFile(&efs, "AbnormalNewlines", "/test1.c", lines, 4);
}

TEST(LineMapTest, SingleLineFile)
{
    EmuFilesystem efs;
    const char* lines[] = {
        "helloworld, helloworld"
    };
    SetupAndVerifyFile(&efs, "SingleLineFile", "/test2.c", lines, 1);
}
