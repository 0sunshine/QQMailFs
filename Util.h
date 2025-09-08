#pragma once

#include <glib.h>
#include <glib/gi18n.h>
#include <string>

std::string mUtf7ToUnicode16le(const std::string& in);
std::string Unicode16leToGbk(const std::string& in);
std::string mUtf7ToGbk(const std::string& in);

char*
imap_utf7_utf8(const char* in);