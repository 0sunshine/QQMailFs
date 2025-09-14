#pragma once

#include "IOSchedulerBase.h"

#include <string>
#include <string_view>
#include <stdint.h>
#include <vector>
#include <queue>

struct CommonResponse
{
    std::string tag;
    std::string status;
    std::string desc;

    std::vector<std::string> ignoreLines;
};

using LoginResponse = CommonResponse;
using CapabilityResponse = CommonResponse;

struct ListResponse: CommonResponse
{
    struct Item 
    {
        std::string attribute;
        std::string delim;
        std::string name;
        std::string gbkName;
    };

    std::list<Item> items;
};

struct SelectResponse : CommonResponse
{
    std::string exists;
};

struct SearchResponse : CommonResponse
{
    std::list<std::string> numbers;
};

struct FetchResponse : CommonResponse
{

};


class ImapResponseParser
{
    static const int BufferSize = 1024 * 64;

public:
    ImapResponseParser(IOClientBase& client);

    int WaitLogin(LoginResponse& res);
    int WaitCapability(CapabilityResponse& res);
    int WaitList(ListResponse& res);
    int WaitSelect(SelectResponse& res);
    int WaitSearch(SearchResponse& res);
    int WaitFetch(FetchResponse& res);

private:
    int TryFromCache(std::string& line);
    int GetLineMsg(std::string& line);
    int GetNextToken(std::string_view& token, const std::string_view& line, int offset = 0);

private:
    static int32_t FindCRLFPos(uint8_t* src, const int len);

private:
    IOClientBase& _client;


    uint8_t _cache[BufferSize];
    int32_t _cacheCurr = 0;
    int32_t _cacheEnd = 0;
};

