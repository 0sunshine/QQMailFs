#include "ImapQQParser.h"

#include "Util.h"
#include "Log.h"

#include <string.h>
#include <iostream>
#include <stack>

int ImapQQParser::ParseBodyStructure(FetchResponse& res, std::vector<QQFetchResult>& results)
{
    for (int partIdx = 0; partIdx < res.parts.size(); ++partIdx)
    {
        if (res.parts[partIdx].unparsedItems.empty())
        {
            LOG_FMT_ERROR("unparsedItems is empty");
            return -1;
        }

        if (res.parts[partIdx].unparsedItems.front().arrayType.empty())
        {
            LOG_FMT_ERROR("unparsedItems format error");
            return -1;
        }

        if (results.size() < (partIdx + 1))
        {
            results.emplace_back();
        }

        auto& r = results[partIdx];

        r.seq = res.parts[partIdx].idx;
        auto& rootItem = *res.parts[partIdx].unparsedItems.begin();

        FetchResponse::DataItem* pBodystructureItem = nullptr;
        
        for (auto it = rootItem.arrayType.begin(); it != rootItem.arrayType.end(); ++it)
        {
            if (it->strType != "BODYSTRUCTURE")
            {
                continue;
            }

            ++it;
            if (it != rootItem.arrayType.end())
            {
                pBodystructureItem = &(*it);
            }

            break;
        }

        if (nullptr == pBodystructureItem)
        {
            LOG_FMT_ERROR("can't not find BODYSTRUCTURE, partIdx: %d", partIdx);
            return -1;
        }

        std::vector<FetchResponse::DataItem*> bodys;

        for (auto& x : pBodystructureItem->arrayType)
        {
            if (x.arrayType.empty())
            {
                continue;
            }
            
            FetchResponse::DataItem* curr = &x;

            while (!curr->arrayType[0].arrayType.empty())
            {
                curr = &(curr->arrayType[0]);
            }

            if (curr->arrayType.size() < 10)
            {
                continue;
            }

            bodys.push_back(curr);
        }
        
        for (auto& x : bodys)
        {
            auto& arr = x->arrayType;
            if (arr[0].strType == "\"APPLICATION\"")
            {
                if (arr[2].arrayType.size() < 4)
                {
                    LOG_FMT_ERROR("invaild APPLICATION, partIdx: %d", partIdx);
                    return -1;
                }
                
                r.files.emplace_back();
                r.files.back().gbkName = Base64ToGbk(arr[2].arrayType[3].strType);
            }
        }
    }

    return 0;
}

int ImapQQParser::ParseBodySection(FetchResponse& res, std::vector<QQFetchResult>& results)
{

    for (int partIdx = 0; partIdx < res.parts.size(); ++partIdx)
    {
        if (res.parts[partIdx].unparsedItems.empty())
        {
            LOG_FMT_ERROR("unparsedItems is empty");
            return -1;
        }

        if (res.parts[partIdx].unparsedItems.front().arrayType.empty())
        {
            LOG_FMT_ERROR("unparsedItems format error");
            return -1;
        }

        if (results.size() < (partIdx + 1))
        {
            LOG_FMT_ERROR("ParseBodySection error, idx(%d) more than length of results", partIdx);
            return -1;
        }

        auto& topArrayType = res.parts[partIdx].unparsedItems.front().arrayType;
        auto it = topArrayType.begin();

        auto body1 = std::find_if(it, topArrayType.end(), [](const FetchResponse::DataItem& item) {
            return item.strType == "BODY[1]";
        });

        if (body1 == topArrayType.end())
        {
            LOG_FMT_ERROR("BODY[1] not fount");
            return -1;
        }

        int body1StartFound = 0;
        int body1EndFound = 0;
        for (it = body1; it != topArrayType.end(); ++it)
        {
            if (body1StartFound)
            {
                if (it->strType == "\r\n")
                {
                    body1EndFound = 1;
                    ++it;
                    break;
                }

                results[partIdx].textInfo.gbkText += Base64ToGbk(it->strType);

                
            }

            if (it->strType == "\r\n")
            {
                body1StartFound = 1;
            }
        }

        if ((body1StartFound & body1EndFound) == 0)
        {
            LOG_FMT_ERROR("BODY[1] format error");
            return -1;
        }

        for (int i = 0; i < results[partIdx].files.size(); ++i)
        {
            auto& file = results[partIdx].files[i];

            std::string bodyTag = "BODY[";
            bodyTag += std::to_string(i+2);
            bodyTag += "]";

            auto bodyn = std::find_if(it, topArrayType.end(), [&bodyTag](const FetchResponse::DataItem& item) {
                return item.strType == bodyTag;
            });

            if (bodyn == topArrayType.end())
            {
                LOG_FMT_ERROR("%s not fount", bodyTag.c_str());
                return -1;
            }

            ++bodyn;
            if (bodyn == topArrayType.end())
            {
                continue;
            }

            auto contextPos = bodyn->strType.find("}");
            if (contextPos == std::string::npos)
            {
                LOG_FMT_ERROR("not find }, start is %s ...", bodyn->strType.substr(0, 15).c_str());
                return -1;
            }

            contextPos++;

            std::string_view view(bodyn->strType);
            file.binData = Base64Decode(view.substr(contextPos));
        }
    }
    return 0;
}
