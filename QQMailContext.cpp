#include "Log.h"

#include "QQMailContext.h"


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
        LOG_FMT_ERROR("%s", "has been logined");
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
