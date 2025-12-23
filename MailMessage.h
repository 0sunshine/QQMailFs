#pragma once

#include <string>
#include <vector>

struct MailMessage
{
    std::string from = "save<1096693846@qq.com>";
    std::string to = "save<1096693846@qq.com>";
    std::string date = "Mon, 22 Dec 2025 13 : 50 : 02 + 0000";
    std::string subject = "no name";

    struct FileInfo
    {
        std::string gbkName;
        std::string binData;
    };

    std::string gbkText;
    std::vector<FileInfo> files;

    bool FormatTo(std::string& result);
};

