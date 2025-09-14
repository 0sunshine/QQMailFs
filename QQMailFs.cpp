﻿#define _CRT_SECURE_NO_WARNINGS

#include "Util.h"
#include "Log.h"
#include "AsioIOScheduler.h"
#include "ImapResponseParser.h"

#include <fstream>

std::string gUsername;
std::string gPassword;


void go(IOClientBase& client);

int main(int argc, char* argv[])
{
    std::ifstream fin("passwd.txt");
    if (!fin)
    {
        LOG_FMT_ERROR("no passwd.txt");
        return -1;
    }

    std::getline(fin, gUsername);
    std::getline(fin, gPassword);


    AsioIOScheduler scheduler;
    scheduler.AddTask(&go);

    scheduler.Run();

    return 0;
}

void go(IOClientBase& client)
{
    int ret = 0;
    ImapResponseParser parser(client);
    ImapTagGen tagGen = ImapTagGen::Create();

    ret = client.Connect("imap.qq.com", 993);
    if (ret < 0)
    {
        LOG_FMT_ERROR("Connect failed, %d", ret);
        return;
    }

    std::string nextCmdTag;
    std::string msg;

    LoginResponse loginRes;
    {
        nextCmdTag = tagGen.GetNextTag();
        loginRes.tag = nextCmdTag;

        msg = nextCmdTag + " LOGIN " + gUsername + " " + gPassword + "\r\n";

        ret = client.Write((uint8_t*)msg.c_str(), msg.size());
        if (ret < 0)
        {
            LOG_FMT_ERROR("command LOGIN, Write failed, %d", ret);
            return;
        }

        ret = parser.WaitLogin(loginRes);
        if (ret < 0)
        {
            LOG_FMT_ERROR("WaitLogin failed, %d", ret);
            return;
        }
    }
    
    ListResponse listRes;
    {
        nextCmdTag = tagGen.GetNextTag();
        listRes.tag = nextCmdTag;

        msg = nextCmdTag + R"xxx( LIST "" "*")xxx""\r\n";

        ret = client.Write((uint8_t*)msg.c_str(), msg.size());
        if (ret < 0)
        {
            LOG_FMT_ERROR("command LIST, Write failed, %d", ret);
            return;
        }

        ret = parser.WaitList(listRes);
        if (ret < 0)
        {
            LOG_FMT_ERROR("WaitList failed, %d", ret);
            return;
        }

        LOG_FMT_DEBUG("list: ");
        for (auto& x : listRes.items)
        {
            LOG_FMT_DEBUG("%s", x.gbkName.c_str());
        }
    }

    SelectResponse selectRes;
    {
        nextCmdTag = tagGen.GetNextTag();
        selectRes.tag = nextCmdTag;

        msg = nextCmdTag + R"xxx( SELECT "&TipOultYUKg-")xxx""\r\n";

        ret = client.Write((uint8_t*)msg.c_str(), msg.size());
        if (ret < 0)
        {
            LOG_FMT_ERROR("command SELECT, Write failed, %d", ret);
            return;
        }

        ret = parser.WaitSelect(selectRes);
        if (ret < 0)
        {
            LOG_FMT_ERROR("WaitSelect failed, %d", ret);
            return;
        }
    }

    SearchResponse searchRes;
    {
        nextCmdTag = tagGen.GetNextTag();
        searchRes.tag = nextCmdTag;

        msg = nextCmdTag + R"xxx( SEARCH ALL)xxx""\r\n";

        ret = client.Write((uint8_t*)msg.c_str(), msg.size());
        if (ret < 0)
        {
            LOG_FMT_ERROR("command Search, Write failed, %d", ret);
            return;
        }

        ret = parser.WaitSearch(searchRes);
        if (ret < 0)
        {
            LOG_FMT_ERROR("WaitSearch failed, %d", ret);
            return;
        }
    }

    FetchResponse fetchRes;
    {
        nextCmdTag = tagGen.GetNextTag();
        fetchRes.tag = nextCmdTag;

        msg = nextCmdTag + R"xxx( FETCH 3 (BODYSTRUCTURE  BODY[2]))xxx" "\r\n";

        ret = client.Write((uint8_t*)msg.c_str(), msg.size());
        if (ret < 0)
        {
            LOG_FMT_ERROR("command FETCH, Write failed, %d", ret);
            return;
        }

        ret = parser.WaitFetch(fetchRes);
        if (ret < 0)
        {
            LOG_FMT_ERROR("WaitFetch failed, %d", ret);
            return;
        }
    }


    LOG_FMT_DEBUG("end...");
}
