#pragma once

#include "IOSchedulerBase.h"

#include <string>
#include <string_view>
#include <stdint.h>
#include <vector>
#include <queue>
#include <memory>

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


// * 3 FETCH 
//(
//	BODYSTRUCTURE 
//		(
//			(
//				(
//					"TEXT" 
//					"PLAIN" 
//						(
//							"charset" "utf-8"
//						) 
//					NIL 
//					NIL 
//					"BASE64" 
//					94 
//					3 
//					NIL 
//					NIL 
//					NIL
//				)
//				(
//					"TEXT" 
//					"HTML" 
//						(
//							"charset" "utf-8"
//						) 
//					NIL 
//					NIL 
//					"BASE64" 
//					2584 
//					36 
//					NIL 
//					NIL 
//					NIL
//				) 
//				"ALTERNATIVE" 
//					(
//						"BOUNDARY" "----=_NextPart_68C6B29E_0005D1E0_36BA156E"
//					) 
//					NIL 
//					NIL
//			)
//		)
//)
struct FetchResponse : CommonResponse
{
    struct DataItem
    {
        std::string strType;
        std::vector<DataItem> arrayType;
    };


    struct Part
    {
        int32_t idx;
        std::list<DataItem> unparsedItems;
    };

    std::vector<Part> parts;
};

struct AppendResponse
{
    std::string tag;
    std::string status;
    std::string desc;

    std::vector<std::string> ignoreLines;
};

class ImapResponseParser
{
    static const int BufferSize = 1024 * 16;

public:
    ImapResponseParser(IOClientBase& client);

    int WaitLogin(LoginResponse& res);
    int WaitCapability(CapabilityResponse& res);
    int WaitList(ListResponse& res);
    int WaitSelect(SelectResponse& res);
    int WaitSearch(SearchResponse& res);
    int WaitFetch(FetchResponse& res);
    int WaitAppend(AppendResponse& res);

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

