#pragma once

#include <glib.h>
#include <glib/gi18n.h>
#include <string>
#include <stdint.h>

std::string mUtf7ToUnicode16le(const std::string& in);
std::string Unicode16leToGbk(const std::string& in);
std::string mUtf7ToGbk(const std::string& in);

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