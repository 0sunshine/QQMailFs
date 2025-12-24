#define _CRT_SECURE_NO_WARNINGS

#include "Util.h"
#include "Log.h"
#include "AsioIOScheduler.h"
#include "ImapResponseParser.h"
#include "ImapQQParser.h"

#include <fstream>
#include "MailMessage.h"
#include "QQMailContext.h"

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

    QQMailContext qqMailContext(client, gUsername, gPassword);
    ret = qqMailContext.Login();
    if (ret < 0)
    {
        LOG_FMT_ERROR("Login failed");
        return;
    }

    std::vector<std::string> folders;
    ret = qqMailContext.ListFolders(folders, "其他文件夹");
    if (ret < 0)
    {
        LOG_FMT_ERROR("ListFolders failed");
        return;
    }

    
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

    //FetchResponse fetchRes;
    //{
    //    nextCmdTag = tagGen.GetNextTag();
    //    fetchRes.tag = nextCmdTag;

    //    msg = nextCmdTag + R"xxx( FETCH 1,2,3 (BODY[HEADER.FIELDS (SUBJECT FROM)] BODYSTRUCTURE BODY[1] BODY[2] BODY[3]))xxx" "\r\n";

    //    ret = client.Write((uint8_t*)msg.c_str(), msg.size());
    //    if (ret < 0)
    //    {
    //        LOG_FMT_ERROR("command FETCH, Write failed, %d", ret);
    //        return;
    //    }

    //    ret = parser.WaitFetch(fetchRes);
    //    if (ret < 0)
    //    {
    //        LOG_FMT_ERROR("WaitFetch failed, %d", ret);
    //        return;
    //    }
    //}

    //ImapQQParser qqParser;
    //std::vector<QQFetchResult> qqFetchResults;

    //qqParser.ParseBodyStructure(fetchRes, qqFetchResults);

    //qqParser.ParseBodySection(fetchRes, qqFetchResults);


    //for (auto& fetch : qqFetchResults)
    //{
    //    for (auto& file : fetch.files)
    //    {
    //        std::ofstream fout("./" + file.gbkName, std::ios::binary);
    //        if (!fout) continue;
    //        fout.write(&file.binData[0], file.binData.size());
    //        fout.close();
    //    }
    //    
    //}

    MailMessage message;
    message.gbkText = "我丢";

    MailMessage::FileInfo file;
    file.gbkName = "xxx.pdf"; 
    file.binData = "123456";
    message.files.push_back(file);

    std::string s;
    message.FormatTo(s);

    {
        AppendResponse tryAppendRes;


        nextCmdTag = tagGen.GetNextTag();
        tryAppendRes.tag = nextCmdTag;
        

        std::string foldName = GbkTomUtf7("其他文件夹/再来一个测试文件夹");

        msg = nextCmdTag + " APPEND " + foldName + " {" + std::to_string(s.size()) + "}\r\n";

        ret = client.Write((uint8_t*)msg.c_str(), msg.size());
        if (ret < 0)
        {
            LOG_FMT_ERROR("command append, Write failed, %d", ret);
            return;
        }


        ret = parser.WaitAppend(tryAppendRes);
        if (ret < 0)
        {
            LOG_FMT_ERROR("WaitTryAppend failed, %d", ret);
            return;
        }

        if (tryAppendRes.status != "NO")
        {
            AppendResponse appendRes;
            appendRes.tag = tryAppendRes.tag;
            ret = client.Write((uint8_t*)s.c_str(), s.size());
            if (ret < 0)
            {
                LOG_FMT_ERROR("command append msg, Write failed, %d", ret);
                return;
            }


            ret = parser.WaitAppend(appendRes);
            if (ret < 0)
            {
                LOG_FMT_ERROR("WaitAppend failed, %d", ret);
                return;
            }
        }
    }

    LOG_FMT_DEBUG("end...");
}
