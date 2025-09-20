#pragma once

#include "ImapResponseParser.h"


//struct QQFetchBodyStructure
//{
//    struct Paramter
//    {
//        std::string key;
//        std::string value;
//    };
//
//    struct PartInfo
//    {
//        std::string bodyType;
//        std::string bodySubType;
//        std::vector<Paramter> bodyParamerList;
//        std::string bodyID;
//        std::string bodyDesc;
//        std::string bodyEncoding;
//        std::string bodySize;
//        std::string others;
//    };
//
//    std::vector<std::vector<PartInfo>> bodys;
//};

struct QQFetchResult
{
    struct TextInfo
    {
        std::string gbkText;
    };

    struct FileInfo
    {
        std::string gbkName;
        std::string binData;
    };

    int seq = -1;
    TextInfo textInfo;
    std::vector<FileInfo> files;
};

class ImapQQParser
{
public:
    int ParseBodyStructure(FetchResponse& res, std::vector<QQFetchResult>& results);
    int ParseBodySection(FetchResponse& res, std::vector<QQFetchResult>& results);
};

