#include "Log.h"

#include "QQMailContext.h"

#include <fstream>
#include <sstream>
#include "MailMessage.h"

QQMailContext::QQMailContext(IOClientBase& client, const std::string& user, const std::string& passwd)
    :
    _client(client), 
    _user(user), 
    _passwd(passwd), 
    _isLogined(false), 
    _parser(_client), 
    _tagGen(ImapTagGen::Create())
{


}

int QQMailContext::Login()
{
    if (_isLogined) 
    {
        LOG_FMT_ERROR("%s", "has been logined");
        return -1;
    }

    int ret = _client.Connect("imap.qq.com", 993);
    if (ret < 0)
    {
        LOG_FMT_ERROR("Connect failed, %d", ret);
        return -1;
    }

    LoginResponse loginRes;
    loginRes.tag = _tagGen.GetNextTag();

    std::string msg = loginRes.tag + " LOGIN " + _user + " " + _passwd + "\r\n";
    ret = _client.Write((uint8_t*)msg.c_str(), msg.size());
    if (ret < 0)
    {
        LOG_FMT_ERROR("command LOGIN, Write failed, %d", ret);
        return -1;
    }

    ret = _parser.WaitLogin(loginRes);
    if (ret < 0)
    {
        LOG_FMT_ERROR("WaitLogin failed, %d", ret);
        return -1;
    }

    if (loginRes.status != ImapOK)
    {
        LOG_FMT_ERROR("Login status: %s", loginRes.status.c_str());
        return -1;
    }

    _isLogined = true;

    return 0;
}

int QQMailContext::ListFolders(std::vector<std::string>& folders, const std::string& reference)
{
    if (!_isLogined)
    {
        LOG_FMT_ERROR("%s", "not login");
        return -1;
    }


    ListResponse listRes;
    listRes.tag = _tagGen.GetNextTag();

    std::string utf7Reference = GbkTomUtf7(reference);

    std::string msg = listRes.tag + " LIST \"" + utf7Reference + "\" *\r\n";
    int ret = _client.Write((uint8_t*)msg.c_str(), msg.size());
    if (ret < 0)
    {
        LOG_FMT_ERROR("command LIST, Write failed, %d", ret);
        return -1;
    }

    ret = _parser.WaitList(listRes);
    if (ret < 0)
    {
        LOG_FMT_ERROR("WaitList failed, %d", ret);
        return -1;
    }

    if (listRes.status != ImapOK)
    {
        LOG_FMT_ERROR("LIST status: %s", listRes.status.c_str());
        return -1;
    }

    folders.clear();
    for (auto& x : listRes.items)
    {
        folders.emplace_back(std::move(x.gbkName));
    }

    return 0;
}

int QQMailContext::Upload(std::filesystem::path& file, const std::string& remoteFolder, std::string& errMsg)
{
    if (!_isLogined)
    {
        errMsg = "not login";
        LOG_FMT_ERROR("%s", errMsg.c_str());
        return -1;
    }

    std::error_code err;

    if (!std::filesystem::is_regular_file(file, err))
    {
        errMsg = file.string() + " not exist";
        LOG_FMT_ERROR("%s", errMsg.c_str());
        return -1;
    }

    if (err)
    {
        errMsg = "error: " + err.message();
        LOG_FMT_ERROR("%s", errMsg.c_str());
        return -1;
    }



    MailMessage message;

    message.files.emplace_back();
    MailMessage::FileInfo& f = message.files.back();

    f.gbkName = file.filename().string();

    const uintmax_t fileSize = std::filesystem::file_size(file, err);
    if (err)
    {
        errMsg = "error: " + err.message();
        LOG_FMT_ERROR("%s", errMsg.c_str());
        return -1;
    }

    f.binData.reserve(fileSize);

    {
        const int tmpBufferSize = 1024 * 512;
        std::unique_ptr<char[]> tmpBuffer{ new char[tmpBufferSize] };

        std::ifstream fin(file.string(), std::ios::in | std::ios::binary);
        if (!fin)
        {
            errMsg = file.string() + " open failed";
            LOG_FMT_ERROR("%s", errMsg.c_str());
            return -1;
        }

        while (fin)
        {
            fin.read(tmpBuffer.get(), tmpBufferSize);
            std::streamsize bytes = fin.gcount();

            f.binData.append(tmpBuffer.get(), bytes);
        }

        fin.close();
    }

    
    if (f.binData.size() != fileSize)
    {
        errMsg = "error: read size not equal file size";
        LOG_FMT_ERROR("%s", errMsg.c_str());
        return -1;
    }

    std::string s;
    message.FormatTo(s);

    AppendResponse tryAppendRes;
    tryAppendRes.tag = _tagGen.GetNextTag();

    std::string mUtf7FolderName = GbkTomUtf7(remoteFolder);
    std::string cmdMsg = tryAppendRes.tag + " APPEND " + mUtf7FolderName + " {" + std::to_string(s.size()) + "}\r\n";

    int ret = _client.Write((uint8_t*)cmdMsg.c_str(), cmdMsg.size());
    if (ret < 0)
    {
        errMsg = "command append, Write failed, " + std::to_string(ret);
        LOG_FMT_ERROR("%s", errMsg.c_str());
        return -1;
    }

    ret = _parser.WaitAppend(tryAppendRes);
    if (ret < 0)
    {
        errMsg = "WaitTryAppend failed, " + std::to_string(ret);
        LOG_FMT_ERROR("%s", errMsg.c_str());
        return -1;
    }

    if (tryAppendRes.status == ImapNO || tryAppendRes.status == ImapBAD)
    {
        errMsg = "error: tryAppendRes.status " + tryAppendRes.status;
        LOG_FMT_ERROR("%s", errMsg.c_str());
        return -1;
    }

    AppendResponse appendRes;
    appendRes.tag = tryAppendRes.tag;
    
    std::streamsize sendBytes = 0;


    const int eachSendFixedSize = 1024 * 1024 * 1;
    while (sendBytes < s.size())
    {
        int nextSendSize = (sendBytes + eachSendFixedSize) > s.size() ? (s.size() - sendBytes) : eachSendFixedSize;

        ret = _client.Write((uint8_t*)(s.c_str() + sendBytes), nextSendSize);
        if (ret < 0)
        {
            errMsg = "error: appendRes write";
            LOG_FMT_ERROR("%s", errMsg.c_str());
            return -1;
        }

        sendBytes += nextSendSize;

        LOG_FMT_INFO("->file (%s) upload progress: %d/%d, %0.2lf%%", file.string().c_str(), (int)sendBytes, (int)s.size(), (double)sendBytes / (int)s.size() * 100);
    }

    ret = _parser.WaitAppend(appendRes);
    if (ret < 0)
    {
        errMsg = "WaitAppend failed, " + std::to_string(ret);
        LOG_FMT_ERROR("%s", errMsg.c_str());
        return -1;
    }

    return 0;
}
