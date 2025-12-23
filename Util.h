#pragma once

#include <glib.h>
#include <glib/gi18n.h>
#include <string>
#include <stdint.h>
#include <string_view>

std::string mUtf7ToUnicode16le(const std::string& in);
std::string Unicode16leTomUtf7(const std::string& in);
std::string Unicode16leToGbk(const std::string& in);
std::string GbkToUnicode16le(const std::string& in);
std::string mUtf7ToGbk(const std::string& in);
std::string GbkTomUtf7(const std::string& in);
std::string Utf8ToGbk(const std::string& in);
std::string GbkToUtf8(const std::string& in);
std::string Base64ToGbk(const std::string& in);
std::string Base64Decode(const std::string_view& in);
std::string Base64Encode(const std::string_view& in);

class ImapTagGen
{
public:

    //ImapTagGen(const ImapTagGen&) = default;

    std::string GetNextTag();

    static ImapTagGen Create();

private:
    ImapTagGen(const std::string& baseTag):_baseTag(baseTag){};

    std::string _baseTag;
    int32_t _seq = 0;
};