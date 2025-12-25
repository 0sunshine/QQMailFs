#pragma once

#include "Util.h"
#include "IOSchedulerBase.h"
#include "ImapResponseParser.h"

#include <string>
#include <filesystem>

class QQMailContext
{
public:
    QQMailContext(IOClientBase& client, const std::string& user, const std::string& passwd);

    int Login();
    int ListFolders(std::vector<std::string>& folders, const std::string& reference = "");
    int Upload(std::filesystem::path& file, const std::string& remote_folder, std::string& err);

private:
    IOClientBase& _client;
    std::string _user;
    std::string _passwd;
    bool _isLogined;

    ImapResponseParser _parser;
    ImapTagGen _tagGen;
};

