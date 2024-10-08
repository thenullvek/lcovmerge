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

#include <alloca.h>
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base64.h"
#include "filesystem.h"
#include "md5.h"

#define MAX_NDIGITS 10
#define INVALID_UNSIGNED_INTEGER    UINT32_MAX
#define ERROR(...) fprintf(stderr, __VA_ARGS__)

// lcov format definition
struct LcovTestRecord;
struct SourceFileInfo;
struct FunctionCoverageInfo;
struct LineCoverageInfo;
struct LcovParser;

typedef enum {
    UNKNOWN = 0, TN, SF, VER, FN, FNDA, FNF, FNH, DA, BRDA, BRF, BRH, LF, LH, END_OF_RECORD,
    LAST_RECORD_TYPE
} LcovRecordType;
using LcovRecordArgList = std::vector<std::string_view>;

struct LcovTestRecord {

    LcovTestRecord(const std::string& tn, IFilesystem* fs) : tn_(tn), fs_(fs) {}
    ~LcovTestRecord();
    const std::string& GetTestName() const { return tn_; }
    int Export(FILE* fp);

private:
    SourceFileInfo* GetCurrentSourceFileInfo() const { return cursf_; }
    void SetCurrentSourceFileInfo(SourceFileInfo* sf) { cursf_ = sf; }
    IFilesystem* GetFilesystemInterface() const { return fs_; }

private:
    std::string tn_;
    // fullpath -> SourceFileInfo
    std::unordered_map<std::string, SourceFileInfo*> sfs_;
    SourceFileInfo* cursf_= nullptr;
    IFilesystem* fs_;

    friend LcovParser;
};

struct LineBranchCoverage {
    enum { NEVER_EXECUTED = UINT32_MAX - 1 };
    struct BranchExecInfo {
        uint32_t xcount_ = NEVER_EXECUTED;
        bool is_defined_ = false;
    };

    std::vector<std::vector<BranchExecInfo>> blocks_;
    bool is_defined_ = false;
};

struct SourceFileInfo {

    SourceFileInfo(std::string_view fullpath) {
        sfname_ = fullpath.substr(fullpath.find_last_of("/\\") + 1);
        fullpath_ = fullpath;
        linemap_.push_back(LINEMAP_UNKNOWN);
    }

    int Export(FILE* fp);
    const std::string& GetSourceFileName() const { return sfname_; }
    const std::string& GetSourceFilePath() const { return fullpath_; }

    bool IsLineDataAvailable() const { return linemap_[0] == LINEMAP_LOADED; }
    bool LoadLineMap(IFilesystem* fs, std::string* err);
    std::string_view ReadLineData(uint32_t lineno, bool no_newline);

    FunctionCoverageInfo* LookupFunction(std::string_view name);
    template<typename... Targs>
    std::pair<FunctionCoverageInfo*, bool> GetFunction(std::string_view name, Targs &&...args);
    LineCoverageInfo* GetLineCoverage(uint32_t lineno);

    enum { INVALID_BLOCK_ID = UINT16_MAX, INVALID_BRANCH_ID = UINT16_MAX };
    LineBranchCoverage::BranchExecInfo* GetBranchCoverage(uint32_t lineno, uint32_t blkId, uint32_t branchId);

    bool IsLineNumberInRange(uint32_t lineno) const {
        if (IsLineDataAvailable())
            return lineno > 0 && lineno <= linemap_.size() - 2;
        return lineno > 0;
    }
    bool SetVersionID(int version) {
        assert(version != VERSION_UNSET && version != VERSION_INVALID);
        if (version_ == VERSION_UNSET) {
            version_ = version;
            return true;
        }
        return false;
    }
    bool IsCompatible(int version) const {
        return version_ == VERSION_UNSET || (version_ != VERSION_UNSET && version_ == version);
    }
    enum { VERSION_UNSET = -1, VERSION_INVALID = INT_MAX };

private:
    std::string sfname_; // basename
    std::string fullpath_;
    std::string content_;
    std::unordered_map<std::string, FunctionCoverageInfo> funcs_;
    std::vector<LineCoverageInfo> das_;
    std::vector<LineBranchCoverage> branches_;
    enum { LINEMAP_UNKNOWN, LINEMAP_LOADED, LINEMAP_ON_ERROR };
    std::vector<uint32_t> linemap_;
    int version_ = -1;
};

struct FunctionCoverageInfo {
    FunctionCoverageInfo() = default;

    uint32_t lineno_ = 0;
    uint32_t xcount_ = 0;
    bool is_private_ = false;
};

struct LineCoverageInfo {
    LineCoverageInfo() {
        xcount_ = 0;
        is_defined_ = false;
        has_checksum_ = false;
    }
    uint32_t xcount_;
    uint8_t checksum_[MD5Hash::Length];
    uint8_t is_defined_:1;
    uint8_t has_checksum_:1;
};

// lcov parser
struct LcovParser {

    struct Config {
        Config() : discard_checksum_(false), generate_checksum_(false) {}
        uint32_t discard_checksum_:1;
        uint32_t generate_checksum_:1;
    };

    LcovParser(Config& config) : cfg_(config) {};
    ~LcovParser() {
        for (auto tp : tests_)
            delete tp.second;
    }

    bool Parse(IFilesystem* fs, const char* fpath);
    const std::unordered_map<std::string_view,LcovTestRecord*>& GetTestRecords() const { return tests_; }

private:
    typedef bool (*RecordHandler)(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err);
    static bool HandlerSF(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err);
    static bool HandlerFN(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err);
    static bool HandlerFNDA(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err);
    static bool HandlerDA(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err);
    static bool HandlerBRDA(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err);
    static bool HandlerEOR(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err);
    static bool HandlerVER(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err);

    // NOTE: FNF, FNH, BRF, BRH, LF and LH take only one integer argument and we don't really
    // care about their values, cause those records will be recalculated anyway.
    // This special handler does nothing more than basic validation.
    static bool HandlerNFNH(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err);

    static constexpr RecordHandler kHandlers_[LcovRecordType::LAST_RECORD_TYPE] = {
        nullptr, // UNKNOWN
        nullptr, // TN
        HandlerSF,
        HandlerVER,
        HandlerFN,
        HandlerFNDA,
        HandlerNFNH, //HandlerFNF,
        HandlerNFNH, //HandlerFNH,
        HandlerDA,
        HandlerBRDA,
        HandlerNFNH, //HandlerBRF,
        HandlerNFNH, //HandlerBRH,
        HandlerNFNH, //HandlerLF,
        HandlerNFNH, //HandlerLH,
        HandlerEOR,
    };

    LcovTestRecord* current_test_ = nullptr;
    std::unordered_map<std::string_view,LcovTestRecord*> tests_;
    Config cfg_;
};

struct LineParser {

    LineParser(const char* buf, size_t len) : p_(buf), q_(buf+len) {}

    LcovRecordType ParseRecordType();
    bool ParseRecordArguments(LcovRecordArgList* args, std::string* err);

private:
    const char* p_;
    const char* q_;
};

#ifndef LCOVMERGE_DEFINITION_ONLY

// Helper functions
static uint32_t StrToUnsigned32(std::string_view str, uint32_t fallback)
{
    char* endp = nullptr;
    uint32_t res;

    if (str.size() > MAX_NDIGITS || str.at(0) == '-')
        return fallback;
    res = strtoul(str.data(), &endp, 10);

    return endp - str.data() == str.length() ? res : fallback;
}

static int FindChar(const char* p, size_t len, char ch)
{
    int res;
    for (res = 0; res < len; res++) {
        if (p[res] == ch)
            return res;
    }
    return -1;
}

template<class Tv, class... Args>
static typename std::vector<Tv>::iterator VectorExtendToPos(std::vector<Tv>* vec, size_t pos, Args&& ...args)
{
    if (pos < vec->size())
        return vec->begin() + pos;
    vec->reserve(pos + 1);
    size_t n = pos - vec->size() + 1;
    while (n--) {
        vec->push_back({args...});
    }
    return vec->end() - 1;
}

static const char* RecordType2Str(LcovRecordType type)
{
    switch (type) {
        case LcovRecordType::UNKNOWN: return "<unknown>";
        case LcovRecordType::TN: return "<TN>";
        case LcovRecordType::SF: return "<SF>";
        case LcovRecordType::VER: return "<VER>";
        case LcovRecordType::FN: return "<FN>";
        case LcovRecordType::FNDA: return "<FNDA>";
        case LcovRecordType::FNF: return "<FNF>";
        case LcovRecordType::FNH: return "<FNH>";
        case LcovRecordType::DA: return "<DA>";
        case LcovRecordType::BRDA: return "<BRDA>";
        case LcovRecordType::BRF: return "<BRF>";
        case LcovRecordType::BRH: return "<BRH>";
        case LcovRecordType::LF: return "<LF>";
        case LcovRecordType::LH: return "<LH>";
        case LcovRecordType::END_OF_RECORD: return "<end_of_record>";
        default:
            break;
    }
    return nullptr;
}

// Main implementation

LcovTestRecord::~LcovTestRecord()
{
    for (auto v : sfs_)
        delete v.second;
}


int LcovTestRecord::Export(FILE* fp)
{
    assert(fp != nullptr);
    if (!tn_.empty())
        fprintf(fp, "TN:%s\n", tn_.c_str());
    for (auto& v : sfs_) {
        fprintf(fp, "SF:%s\n", v.first.c_str());
        if (v.second->Export(fp))
            return 1;
    }
    return 0;
}

int SourceFileInfo::Export(FILE* fp)
{
    // Export function definitions
    for (const auto& rec : funcs_) {
        const auto& func = rec.second;
        if (!func.is_private_)
            fprintf(fp, "FN:%u,%s\n", func.lineno_, rec.first.c_str());
        else
            fprintf(fp, "FN:%u,%s:%s\n", func.lineno_, GetSourceFileName().c_str(), rec.first.c_str());
    }

    // Export function coverage records
    size_t fnh = 0;
    for (const auto& rec : funcs_) {
        const auto& func = rec.second;
        if (func.xcount_ != 0)
            fnh++;
        if (!func.is_private_)
            fprintf(fp, "FNDA:%u,%s\n", func.xcount_, rec.first.c_str());
        else
            fprintf(fp, "FNDA:%u,%s:%s\n", func.xcount_, GetSourceFileName().c_str(), rec.first.c_str());
    }
    fprintf(fp, "FNF:%zu\nFNH:%zu\n", funcs_.size(), fnh);

    // llvm-cov generates line coverage records after FN*s, so do we.
    uint32_t lf = 0, lh = 0, l = 0;
    for (const LineCoverageInfo& li : das_) {
        if (li.is_defined_) {
            assert(l > 0);
            fprintf(fp, "DA:%u,%u", l, li.xcount_);
            if (li.has_checksum_) {
                fputc(',', fp);
                std::string encoded_checksum;
                Base64Encode(li.checksum_, MD5Hash::Length, &encoded_checksum);
                fputs(encoded_checksum.c_str(), fp);
            }
            fputc('\n', fp);
            if (li.xcount_ > 0)
                lh++;
            lf++;
        }
        l++;
    }

    // Export branch coverage records
    l = 0;
    uint32_t brf = 0, brh = 0;
    for (const LineBranchCoverage& lbr : branches_) {
        if (lbr.is_defined_) {
            uint32_t blkno = 0;

            assert(l > 0);
            for (const auto& branches : lbr.blocks_) {
                uint32_t brno = 0;
                for (const auto& branch : branches) {
                    if (branch.is_defined_) {
                        if (branch.xcount_ == LineBranchCoverage::NEVER_EXECUTED) {
                            fprintf(fp, "BRDA:%u,%u,%u,-\n", l, blkno, brno);
                        } else {
                            fprintf(fp, "BRDA:%u,%u,%u,%u\n", l, blkno, brno, branch.xcount_);
                            if (branch.xcount_) brh++;
                        }
                        brf++;
                    }
                    brno++;
                }
                blkno++;
            }
        }
        l++;
    }
    fprintf(fp, "BRF:%u\nBRH:%u\n", brf, brh);

    // And finally the line summary info
    fprintf(fp, "LF:%u\nLH:%u\nend_of_record\n", lf, lh);

    return 0;
}

std::string_view SourceFileInfo::ReadLineData(uint32_t lineno, bool no_newline)
{
    assert(IsLineDataAvailable() == true);
    assert(IsLineNumberInRange(lineno) == true);
    auto offset = linemap_[lineno];
    const char* linestart = content_.c_str() + offset;
    size_t len = no_newline ? ::strcspn(linestart, "\r\n") : linemap_[lineno + 1] - offset;
    
    return std::string_view(linestart, len);
}

bool SourceFileInfo::LoadLineMap(IFilesystem* fs, std::string* err)
{
    assert(fs != nullptr);
    if (linemap_[0] != LINEMAP_UNKNOWN)
        return linemap_[0] == LINEMAP_LOADED;

    switch (fs->ReadFile(fullpath_.c_str(), &content_, err)) {
        case IFilesystem::SUCCESS:
            if (content_.empty()) {
                linemap_.push_back(0);
                linemap_[0] = LINEMAP_LOADED;
                return true;
            }
            break;
        case IFilesystem::NOT_FOUND:
        case IFilesystem::IO_ERROR:
            linemap_[0] = LINEMAP_ON_ERROR;
            return false;
    }

    const char* p = content_.c_str();
    const char* q = p + content_.size();

    linemap_.push_back(0); // the first line always begins at offset 0
    size_t absoff = 0;
    for (; p < q; ) {
        size_t reloff = ::strcspn(p, "\n");
        bool newline_found = p[reloff] == '\n';
        if (newline_found)
            reloff++;
        absoff += reloff;
        linemap_.push_back(absoff); // 1 for newline itself
        p += reloff;
    }
    linemap_[0] = LINEMAP_LOADED;
    return true;
}


FunctionCoverageInfo* SourceFileInfo::LookupFunction(std::string_view name)
{
    auto it = funcs_.find(std::string(name));
    if (it != funcs_.cend())
        return &it->second;
    return nullptr;
}

template<typename... Targs>
std::pair<FunctionCoverageInfo*, bool> SourceFileInfo::GetFunction(std::string_view name, Targs &&...args)
{
    auto v = funcs_.emplace(name, args...);
    return std::make_pair(&v.first->second, v.second);
}

LineCoverageInfo* SourceFileInfo::GetLineCoverage(uint32_t lineno)
{
    assert(lineno != 0 && "bug: invalid lineno");
    return VectorExtendToPos(&das_, lineno).base();
}

LineBranchCoverage::BranchExecInfo* SourceFileInfo::GetBranchCoverage(uint32_t lineno, uint32_t blkId, uint32_t branchId)
{
    assert(IsLineNumberInRange(lineno) && "bug: invalid line number");
    assert(blkId < INVALID_BLOCK_ID && branchId < INVALID_BRANCH_ID);
    auto lbr = VectorExtendToPos(&branches_, lineno);
    lbr->is_defined_ = true;
    auto blk = VectorExtendToPos(&lbr->blocks_, blkId);
    auto br = VectorExtendToPos(blk.base(), branchId);
    br->is_defined_ = true;
    return br.base();
}

LcovRecordType LineParser::ParseRecordType()
{
    int sep = FindChar(p_, q_ - p_, ':');
    LcovRecordType res = LcovRecordType::UNKNOWN;

    switch (sep) {
        case 2:
            if (p_[0] == 'T' && p_[1] == 'N')
                res = LcovRecordType::TN;
            else if (p_[0] == 'S' && p_[1] == 'F')
                res = LcovRecordType::SF;
            else if (p_[0] == 'F' && p_[1] == 'N')
                res = LcovRecordType::FN;
            else if (p_[0] == 'D' && p_[1] == 'A')
                res = LcovRecordType::DA;
            else if (p_[0] == 'L' && p_[1] == 'F')
                res = LcovRecordType::LF;
            else if (p_[0] == 'L' && p_[1] == 'H')
                res = LcovRecordType::LH;
            break;

        case 3:
            if (!strncmp(p_, "FNF", 3))
                res = LcovRecordType::FNF;
            else if (!strncmp(p_, "FNH", 3))
                res = LcovRecordType::FNH;
            else if (!strncmp(p_, "BRF", 3))
                res = LcovRecordType::BRF;
            else if (!strncmp(p_, "BRH", 3))
                res = LcovRecordType::BRH;
            else if (!strncmp(p_, "VER", 3))
                res = LcovRecordType::VER;
            break;

        case 4:
            if (!strncmp(p_, "FNDA", 4))
                res = LcovRecordType::FNDA;
            else if (!strncmp(p_, "BRDA", 4))
                res = LcovRecordType::BRDA;
            break;

        default:
            break;
    }

    if (res == LcovRecordType::UNKNOWN && !strncmp(p_, "end_of_record", 13)) {
        p_ += 13;
        return LcovRecordType::END_OF_RECORD;
    }

    p_ += sep + 1;
    return res;
}

bool LineParser::ParseRecordArguments(LcovRecordArgList* args, std::string* err)
{
    args->clear();
    assert(*p_ != ':');
    *err = "";
    for (; p_ < q_; ) {
        const char* start = p_;
        size_t remlen = q_ - start;
        size_t endpos = FindChar(start, remlen, ',');

        if (!endpos) {
            *err = "trailing commas";
            return false;
        }

        size_t len = endpos == -1 ? remlen : endpos;
        args->push_back(std::string_view(start, len));
        p_ += len;
        if (endpos != -1 && start[endpos] == ',')
            p_++;

        if (args->size() > 4) {
            *err = "too many arguments (max: 4)";
            return false;
        }
    }
    return true;
}

bool LcovParser::Parse(IFilesystem* fs, const char* fpath)
{
    std::string errmsg;
    std::string content;
    LcovRecordArgList args;
    uint32_t lineno = 1;
    const char* linestart;
    const char* content_end;

    switch (fs->ReadFile(fpath, &content, &errmsg)) {
        case IFilesystem::SUCCESS:
            break;
        case IFilesystem::NOT_FOUND:
        case IFilesystem::IO_ERROR:
            ERROR("%s: %s\n", fpath, errmsg.c_str());
            return false;
    }
    if (content.empty())
        return true;
    linestart = content.c_str();
    content_end = linestart + content.size();

    args.reserve(4);
    for (; linestart < content_end; lineno++) {
        size_t len = strcspn(linestart, "\r\n");
        LineParser lp(linestart, len);

        if (linestart[0] != '#' && len) {
            LcovRecordType type = lp.ParseRecordType();

            if (type == LcovRecordType::UNKNOWN) {
                ERROR("%s:%u: unknown record type\n", fpath, lineno);
                return false;
            }

            if (!lp.ParseRecordArguments(&args, &errmsg)) {
                ERROR("%s:%u: %s\n", fpath, lineno, errmsg.c_str());
                return false;
            }

            // Handle TN and end_of_record record here.
            if (type == LcovRecordType::TN) {
                if (args.size() != 1) {
                    ERROR("%s:%u: expected one test name", fpath, lineno);
                    return false;
                }

                std::string testname(args[0]);
                auto it = tests_.find(testname);
                if (it == tests_.cend()) {
                    current_test_ = new LcovTestRecord(testname, fs);
                    tests_[current_test_->GetTestName()] = current_test_;
                } else
                    current_test_ = it->second;

            } else {
                // Note that TN record is optional, if a TN record doesn't appear before SF,
                // allocate an anonymous test record instead.
                if (type == LcovRecordType::SF && !current_test_) {
                    auto it = tests_.find("");
                    if (it == tests_.cend()) {
                        current_test_ = new LcovTestRecord("", fs);
                        tests_[""] = current_test_;
                    } else
                        current_test_ = it->second;
                }
                if (type > LcovRecordType::SF && (!current_test_ || !current_test_->GetCurrentSourceFileInfo())) {
                    ERROR("%s:%u a TN and/or SF record is missing\n", fpath, lineno);
                    return false;
                }
                errmsg.clear();
                if (!kHandlers_[type](current_test_, &args, &cfg_, &errmsg)) {
                    ERROR("%s:%u: %s %s\n", fpath, lineno, ::RecordType2Str(type), errmsg.c_str());
                    return false;
                }
            }
        }

        if (linestart[len] == '\r')
            len++;
        linestart += len + 1;
    }

    return true;
}

bool LcovParser::HandlerSF(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err)
{
    if (args->size() != 1) {
        *err = "expected 1 argument";
        return false;
    } else if (tr->GetCurrentSourceFileInfo()) {
        *err = "expected end_of_record";
        return false;
    }
    std::string sfpath{args->at(0)};
    auto it = tr->sfs_.find(sfpath);
    if (it != tr->sfs_.cend())
        tr->SetCurrentSourceFileInfo(it->second);
    else {
        tr->cursf_ = new SourceFileInfo(sfpath);
        tr->sfs_[sfpath] = tr->cursf_;
    }
    if ((!config->discard_checksum_ || config->generate_checksum_) &&
        !tr->GetCurrentSourceFileInfo()->LoadLineMap(tr->GetFilesystemInterface(), err)) {
        // *err = "failed to load linemap";
        return false;
    }
    return true;
}

bool LcovParser::HandlerEOR(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err)
{
    if (tr->GetCurrentSourceFileInfo()) {
        tr->SetCurrentSourceFileInfo(nullptr);
        return true;
    }
    *err = "no matching SF record";
    return false;
}

bool LcovParser::HandlerFN(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err)
{
    if (args->size() != 2) {
        *err = "expected 2 arguments";
        return false;
    }
    uint32_t lineno = StrToUnsigned32(args->at(0), 0);
    auto funcpath = args->at(1);
    bool is_private_ = false;

    // If a source file name is mentioned in the full fucntion name, verify that
    // it has the same basename as the current source file record.
    size_t pos = funcpath.find_first_of(':');
    if (pos != std::string_view::npos) {
        std::string_view srcfile = funcpath.substr(0, pos);
        if (srcfile.compare(tr->cursf_->GetSourceFileName())) {
            *err = "the orgin of the function doesn't match with current source file";
            return false;
        }
        // Keep the function name part only.
        funcpath = funcpath.substr(pos + 1);
        is_private_ = true;
    }

    auto* sf = tr->GetCurrentSourceFileInfo();
    if (!sf->IsLineNumberInRange(lineno)) {
        *err = "invalid line number";
        return false;
    }
    auto rp = sf->GetFunction(funcpath, FunctionCoverageInfo{lineno, 0, is_private_});
    if (rp.second == false) {
        auto* func = rp.first;
        if (func->lineno_ != lineno || func->is_private_ != is_private_) {
            *err = "conflicting function definitions";
            return false;
        }
    }

    return true;
}

bool LcovParser::HandlerFNDA(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err)
{
    if (args->size() != 2) {
        *err = "expected 2 arguments";
        return false;
    }
    uint32_t xcount = StrToUnsigned32(args->at(0), INVALID_UNSIGNED_INTEGER);
    auto funcpath = args->at(1);

    // If a source file name is mentioned in the full fucntion name, verify that
    // it has the same basename as the current source file record.
    size_t pos = funcpath.find_first_of(':');
    if (pos != std::string_view::npos) {
        std::string_view srcfile = funcpath.substr(0, pos);
        if (srcfile.compare(tr->cursf_->GetSourceFileName())) {
            *err = "orgin of the function doesn't match with the current source file";
            return false;
        }
        // Keep the function name part only.
        funcpath = funcpath.substr(pos + 1);
    }

    auto* func = tr->GetCurrentSourceFileInfo()->LookupFunction(funcpath);
    if (!func) {
        *err = "function coverage info references to an undefined function";
        return false;
    } else if (xcount == INVALID_UNSIGNED_INTEGER) {
        *err = "invalid execution count";
        return false;
    }
    func->xcount_ += xcount;

    return true;
}

bool LcovParser::HandlerNFNH(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err)
{
    if (args->size() != 1) {
        *err = "bad argument";
        return false;
    } else if (INVALID_UNSIGNED_INTEGER == StrToUnsigned32(args->at(0), INVALID_UNSIGNED_INTEGER)) {
        *err = "invalid integer";
        return false;
    }
    return true;
}

bool LcovParser::HandlerDA(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err)
{
    if (args->size() != 2 && args->size() != 3) {
        *err = "expected two arguments";
        return false;
    }
    uint32_t lineno = ::StrToUnsigned32(args->at(0), 0);
    uint32_t xcount = ::StrToUnsigned32(args->at(1), INVALID_UNSIGNED_INTEGER);
    bool checksum_specified = args->size() == 3 && !config->discard_checksum_;

    if (!tr->GetCurrentSourceFileInfo()->IsLineNumberInRange(lineno)) {
        *err = "invalid line number";
        return false;
    } else if (INVALID_UNSIGNED_INTEGER == xcount) {
        *err = "invalid execution count";
        return false;
    } else if (checksum_specified && args->at(2).size() != 24) {
        *err = "invalid checksum";
        return false;
    }

    auto* da = tr->GetCurrentSourceFileInfo()->GetLineCoverage(lineno);
    if (!da->has_checksum_) {
        MD5Hash md5hash;
        uint8_t* line_checksum = nullptr;

        if (config->generate_checksum_ || checksum_specified) {
            line_checksum = reinterpret_cast<uint8_t*>(::alloca(MD5Hash::Length));
            // Calculate the line checksum first.
            {
                auto* srcfile = tr->GetCurrentSourceFileInfo();
                auto linedata = srcfile->ReadLineData(lineno, /* no_newline? */false);
                md5hash.Update(linedata.data(), linedata.size());
                md5hash.Finalize(line_checksum);
            }
            if (checksum_specified) {
                auto specified_checksum_base64 = args->at(2);
                std::string checksum_base64;

                Base64Encode(line_checksum, MD5Hash::Length, &checksum_base64);
                assert(checksum_base64.size() == 24);

                if (checksum_base64.compare(specified_checksum_base64)) {
                    *err = "checksum mismatch";
                    return false;
                }
            }
            (void)memcpy(da->checksum_, line_checksum, MD5Hash::Length);
            da->has_checksum_ = true;
        }
    } else {
        // Otherwise we just need to check if the given checksum string matches with the existing one.
        if (checksum_specified) {
            auto specified_checksum_base64 = args->at(2);
            std::string checksum_base64;
            Base64Encode(da->checksum_, MD5Hash::Length, &checksum_base64);
            assert(checksum_base64.size() == 24);
            if (checksum_base64.compare(specified_checksum_base64)) {
                *err = "conflicting checksum";
                return false;
            }
        }
    }
    da->xcount_ += xcount;
    da->is_defined_ = true;

    return true;
}

bool LcovParser::HandlerBRDA(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err)
{
    if (args->size() != 4) {
        *err = "expected 4 arguments";
        return false;
    }
    uint32_t lineno = StrToUnsigned32(args->at(0), 0);
    uint32_t blkId = StrToUnsigned32(args->at(1), SourceFileInfo::INVALID_BLOCK_ID);
    uint32_t branchId = StrToUnsigned32(args->at(2), SourceFileInfo::INVALID_BRANCH_ID);
    std::string_view xcount_str = args->at(3);
    uint32_t xcount;

    if (xcount_str.size() == 1 && xcount_str[0] == '-') {
        xcount = LineBranchCoverage::NEVER_EXECUTED;
    } else {
        xcount = StrToUnsigned32(xcount_str, INVALID_UNSIGNED_INTEGER);
        if (xcount == INVALID_UNSIGNED_INTEGER) {
            *err = "invalid execution count";
            return false;
        }
    }
    if (!tr->GetCurrentSourceFileInfo()->IsLineNumberInRange(lineno) ||
        blkId == SourceFileInfo::INVALID_BLOCK_ID || branchId == SourceFileInfo::INVALID_BRANCH_ID) {
        *err = "arguments contains non-integers";
        return false;
    }

    auto* br = tr->GetCurrentSourceFileInfo()->GetBranchCoverage(lineno, blkId, branchId);
    if (br->xcount_ != LineBranchCoverage::NEVER_EXECUTED) {
        if (br->xcount_ != LineBranchCoverage::NEVER_EXECUTED)
            br->xcount_ += xcount;
    } else
        br->xcount_ = xcount;

    return true;
}

bool LcovParser::HandlerVER(LcovTestRecord* tr, LcovRecordArgList* args, Config* config, std::string* err)
{
    if (args->size() != 1) {
        *err = "bad argument";
        return false;
    }
    int ver = StrToUnsigned32(args->at(0), SourceFileInfo::VERSION_INVALID);
    if (SourceFileInfo::VERSION_INVALID == ver) {
        *err = "invalid version ID";
        return false;
    }
    auto* sf = tr->GetCurrentSourceFileInfo();
    if (!sf->SetVersionID(ver) && !sf->IsCompatible(ver)) {
        *err = "the given version ID is conflicting with the existing one";
        return false;
    }
    return true;
}

#ifndef TESTMODE

#include "filesystem_host.h"
#include "getopt.h"

[[noreturn]] static void usage(const char* program, int exitcode)
{
    fprintf(stderr, "Usage: %s [OPTIONS] <inputfile1>[inputfileN...]\n\n", program);
    fprintf(stderr, "Options:\n"
                    "   -h,--help               Print this help message and exit.\n"
                    "   -d,--discard-checksum   Discard and ignore line checksums,\n"
                    "                           checksums will not longer be validated.\n"
                    "   -g,--generate-checksum  Generate checksum for each line record.\n"
                    "                           If -d is specified, then the existing\n"
                    "                           checksums from files will be ignored and\n"
                    "                           replaced by new generated checksums.\n"
                    "   -o,--output-file=FILE   ");
    exit(exitcode);
}

int main(int argc, char** argv)
{
    LcovParser::Config config;
    const option kLongOptions[] = {
        { "help", no_argument, NULL, 'h' },
        { "discard-checksum", no_argument, NULL, 'd' },
        { "generate-checksum", no_argument, NULL, 'g'},
        { "output-file", required_argument, NULL, 'o'},
        { NULL, 0, NULL, 0 },
    };
    int opt, exitcode = EXIT_SUCCESS;
    const char* ofile = nullptr;
    const char* program = argv[0];
    HostFilesystem fs;
    FILE* fpout;

    while (-1 != (opt = getopt_long(argc, argv, "dgho:", kLongOptions, NULL))) {
        switch (opt) {
            case 'h':
                usage(program, EXIT_SUCCESS);
                /*UNREACHABLE*/
            case 'd':
                config.discard_checksum_ = true;
                break;
            case 'g':
                config.generate_checksum_ = true;
                break;
            case 'o':
                ofile = optarg;
                break;
            default:
                usage(program, EXIT_FAILURE);
                /*UNREACHABLE*/
        }
    }
    argc -= optind;
    argv += optind;

    if (!argc) {
        fprintf(stderr, "%s: no input files\n", program);
        return EXIT_FAILURE;
    }

    fpout = ofile ? fopen(ofile, "w") : stdout;
    if (!fpout) {
        fprintf(stderr, "%s: failed open file stream\n", program);
        return EXIT_FAILURE;
    }
    rewind(fpout);

    LcovParser parser(config);
    for (int i = 0; i < argc; i++) {
        if (!parser.Parse(&fs, argv[i])) {
            exitcode = EXIT_FAILURE;
            goto finished;
        }
    }
    {
        auto& tests = parser.GetTestRecords();
        for (const auto& tr : tests) {
            if (tr.second->Export(fpout)) {
                ERROR("E: failed to export test record\n");
                exitcode = EXIT_FAILURE;
                break;
            }
        }
    }
finished:
    fclose(fpout);
    if (exitcode == EXIT_FAILURE && ofile)
        std::remove(ofile);

    return exitcode;
}
#endif // TESTMODE
#endif // LCOVMERGE_DEFINITION_ONLY

