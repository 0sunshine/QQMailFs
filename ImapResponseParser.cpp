#include "ImapResponseParser.h"

#include "Util.h"
#include "Log.h"

#include <string.h>
#include <iostream>
#include <stack>

ImapResponseParser::ImapResponseParser(IOClientBase& client)
    :_client(client)
{
}

int ImapResponseParser::WaitLogin(LoginResponse& res)
{
    std::string line;
    int ret = 0;

    while ((ret = GetLineMsg(line)) > 0) 
    {
        std::string_view token;
        int offset = 0;

        offset = GetNextToken(token, line);
        if (offset < 0 || token.empty() || token[0] == '*')
        {
            res.ignoreLines.push_back(line);
            continue;
        }

        if (res.tag != token)
        {
            res.ignoreLines.push_back(line);
            continue;
        }
        
        res.desc = line;

        offset = GetNextToken(token, line, offset);
        if (offset < 0 || token.empty())
        {
            LOG_FMT_ERROR("wait login status error, line: %s", line.c_str());
            return -1;
        }

        res.status = token;

        return 0;
    }

    return ret;
}

int ImapResponseParser::WaitCapability(CapabilityResponse& res)
{
    return 0;
}

int ImapResponseParser::WaitList(ListResponse& res)
{
    std::string line;
    int ret = 0;

    while ((ret = GetLineMsg(line)) > 0)
    {
        std::string_view token;
        int offset = 0;

        offset = GetNextToken(token, line);
        if (offset < 0 || token.empty())
        {
            res.ignoreLines.push_back(line);
            continue;
        }

        if (token == "*")
        {
            offset = GetNextToken(token, line, offset);
            if (offset >= 0 && token == "LIST")
            {
                ListResponse::Item item;

                offset = GetNextToken(token, line, offset);
                if (offset < 0)
                {
                    LOG_FMT_ERROR("list response error, not a complete line, line: %s", line.c_str());
                    return -1;
                }
                item.attribute = token;
                
                offset = GetNextToken(token, line, offset);
                if (offset < 0)
                {
                    LOG_FMT_ERROR("list response error, not a complete line, line: %s", line.c_str());
                    return -1;
                }
                item.delim = token;

                item.name = line.substr(offset);
                item.gbkName = mUtf7ToGbk(item.name);

                res.items.push_back(item);
                continue;
            }
        }

        if (res.tag != token)
        {
            res.ignoreLines.push_back(line);
            continue;
        }

        res.desc = line;

        offset = GetNextToken(token, line, offset);
        if (offset < 0 || token.empty())
        {
            LOG_FMT_ERROR("wait list status error, line: %s", line.c_str());
            return -1;
        }

        res.status = token;

        return 0;
    }

    return ret;
}

int ImapResponseParser::WaitSelect(SelectResponse& res)
{
    std::string line;
    int ret = 0;

    while ((ret = GetLineMsg(line)) > 0)
    {
        std::string_view token;
        int offset = 0;

        offset = GetNextToken(token, line);
        if (offset < 0 || token.empty())
        {
            res.ignoreLines.push_back(line);
            continue;
        }

        if (token == "*")
        {
            offset = GetNextToken(token, line, offset);
            if (offset < 0 || token.empty())
            {
                res.ignoreLines.push_back(line);
                continue;
            }

            std::string thirdtoken = line.substr(offset);
            if (thirdtoken == "EXISTS")
            {
                res.exists = token;
            }
            else
            {
                res.ignoreLines.push_back(line);
            }

            continue;
        }

        if (res.tag != token)
        {
            res.ignoreLines.push_back(line);
            continue;
        }

        res.desc = line;

        offset = GetNextToken(token, line, offset);
        if (offset < 0 || token.empty())
        {
            LOG_FMT_ERROR("wait select status error, line: %s", line.c_str());
            return -1;
        }

        res.status = token;

        return 0;
    }

    return ret;
}

int ImapResponseParser::WaitSearch(SearchResponse& res)
{
    std::string line;
    int ret = 0;

    while ((ret = GetLineMsg(line)) > 0)
    {
        std::string_view token;
        int offset = 0;

        offset = GetNextToken(token, line);
        if (offset < 0 || token.empty())
        {
            res.ignoreLines.push_back(line);
            continue;
        }

        if (token == "*")
        {
            offset = GetNextToken(token, line, offset);
            if (offset < 0 || token.empty())
            {
                res.ignoreLines.push_back(line);
                continue;
            }

            if (token != "SEARCH")
            {
                res.ignoreLines.push_back(line);
                continue;
            }

            int lastOffset = offset;
            while (offset >= 0)
            {
                offset = GetNextToken(token, line, offset);
                if (offset >= 0)
                {
                    if (token.empty())
                    {
                        LOG_FMT_ERROR("wait search result error, line: %s", line.c_str());
                        return -1;
                    }

                    res.numbers.emplace_back(token);
                    lastOffset = offset;
                }
            }

            std::string lastNumber = line.substr(lastOffset);
            if (lastNumber.empty())
            {
                LOG_FMT_ERROR("wait search result error, line: %s", line.c_str());
                return -1;
            }

            res.numbers.emplace_back(lastNumber);

            continue;
        }

        if (res.tag != token)
        {
            res.ignoreLines.push_back(line);
            continue;
        }

        res.desc = line;

        offset = GetNextToken(token, line, offset);
        if (offset < 0 || token.empty())
        {
            LOG_FMT_ERROR("wait select status error, line: %s", line.c_str());
            return -1;
        }

        res.status = token;

        return 0;
    }

    return ret;
}

int ImapResponseParser::WaitFetch(FetchResponse& res)
{
    std::string line;
    int ret = 0;

    std::stack<FetchResponse::DataItem*> stack;
    std::string currWord;

    while ((ret = GetLineMsg(line)) > 0)
    {
        std::string_view token;
        int offset = 0;

        if (stack.empty())
        {
            offset = GetNextToken(token, line);
            if (offset < 0 || token.empty())
            {
                res.ignoreLines.push_back(line);
                continue;
            }

            if (res.tag == token)
            {
                res.desc = line;

                offset = GetNextToken(token, line, offset);
                if (offset < 0 || token.empty())
                {
                    LOG_FMT_ERROR("wait fetch status error, line: %s", line.c_str());
                    return -1;
                }

                res.status = token;

                return 0;
            }

            if (token != "*")
            {
                res.ignoreLines.push_back(line);
                continue;
            }


            offset = GetNextToken(token, line, offset);
            if (offset < 0 || token.empty())
            {
                res.ignoreLines.push_back(line);
                continue;
            }

            std::string_view thirdtoken;
            offset = GetNextToken(thirdtoken, line, offset);
            if (offset < 0 || thirdtoken.empty() || thirdtoken != "FETCH")
            {
                res.ignoreLines.push_back(line);
                continue;
            }

            while (line[offset++] != '(')
            {
                if (offset > line.size())
                {
                    LOG_FMT_ERROR("fetch no (, line: %s", line.c_str());
                    return -1;
                }
            }

            auto& part = res.parts.emplace_back();
            part.idx = res.parts.size() + 1;
            part.unparsedItems.emplace_back();

            auto& item = part.unparsedItems.back();

            stack.push(&item);
        }

        if (stack.empty())
        {
            continue;
        }

        if (line.empty())
        {
            auto& topItem = stack.top();
            if (!currWord.empty())
            {
                topItem->arrayType.emplace_back();
                topItem->arrayType.back().strType = currWord;
                currWord.clear();
            }

            topItem->arrayType.emplace_back();
            topItem->arrayType.back().strType = "\r\n";
        }

        //遇到(创建新对象压栈，字符串赋值self, )出栈
        for(int i = offset; i < line.size(); ++i)
        {
            if (stack.empty())
            {
                LOG_FMT_ERROR("fetch stack.empty(), but still has unparse string, line: %s", line.c_str());
                return -1;
            }

            auto& topItem = stack.top();
            auto c = line[i];

            if (c != '(' && c != ')' && c != ' ')
            {
                currWord.push_back(c);
                continue;
            }

            if (c == ' ')
            {
                if (!currWord.empty())
                {
                    topItem->arrayType.emplace_back();
                    topItem->arrayType.back().strType = currWord;
                    currWord.clear();
                }

                continue;
            }
            else if (c == '(')
            {
                if (!currWord.empty())
                {
                    topItem->arrayType.emplace_back();
                    topItem->arrayType.back().strType = currWord;
                    currWord.clear();
                }

                topItem->arrayType.emplace_back();
                auto& item = topItem->arrayType.back();
                stack.push(&item);
                continue;
            }
            else if (c == ')')
            {
                if (!currWord.empty())
                {
                    topItem->arrayType.emplace_back();
                    topItem->arrayType.back().strType = currWord;
                    currWord.clear();
                }
                stack.pop();
                continue;
            }
        }

    }

    return ret;
}

int ImapResponseParser::TryFromCache(std::string& line)
{
    int32_t remainLenth = _cacheEnd - _cacheCurr;

    int32_t pos = FindCRLFPos(_cache + _cacheCurr, remainLenth);
    if (-1 != pos)
    {
        int32_t oldLen = line.size();
        int32_t len = pos;

        line.resize(oldLen + len);

        if (len > 0)
        {
            memcpy(&line[oldLen], _cache + _cacheCurr, len);
        }

        _cacheCurr += (len + 2);

        return 1;
    }
    else if (_cacheEnd == BufferSize)
    {
        int32_t oldLen = line.size();
        int32_t len = BufferSize - _cacheCurr;
        if (_cache[BufferSize - 1] == '\r')
        {
            --len;
        }

        line.resize(oldLen + len);

        if (len > 0)
        {
            memcpy(&line[oldLen], _cache + _cacheCurr, len);
            _cacheCurr += len;
        }

        len = BufferSize - _cacheCurr;
        if (len > 0)
        {
            memcpy(_cache, _cache + _cacheCurr, len);
        }

        _cacheCurr = 0;
        _cacheEnd = len;
    }

    return 0;
}

int ImapResponseParser::GetLineMsg(std::string& line)
{
    line.clear();

    while (true)
    {
        int cacheRet = TryFromCache(line);
        if (cacheRet > 0) {
            return cacheRet;
        }
        else if (cacheRet < 0) {
            //todo log
            return cacheRet;
        }

        //fill cache
        int32_t remainLenth = BufferSize - _cacheEnd;
        int bytes = _client.Read(_cache + _cacheEnd, remainLenth);
        if (bytes < 0) {
            return bytes;
        }
        _cacheEnd += bytes;
    }

    return -1;
}

int ImapResponseParser::GetNextToken(std::string_view& token, const std::string_view& line, int offset)
{
    static char delim[] = " ";

    auto tokenEnd = line.find(delim, offset);
    if (tokenEnd == std::string::npos)
    {
        return -1;
    }

    token = line.substr(offset, tokenEnd - offset);

    return tokenEnd + (sizeof(delim) - 1);
}

int32_t ImapResponseParser::FindCRLFPos(uint8_t* src, const int len)
{
    for (int32_t i = 0; (i + 1) < len; ++i)
    {
        if (src[i] == '\r' && src[i + 1] == '\n')
        {
            return i;
        }
    }

    return -1;
}
