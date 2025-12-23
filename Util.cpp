#include "Util.h"
#include "base64.h"

#include <iconv.h>
#include <iostream>
#include <mutex>

static const char* utf7_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,";

static unsigned char utf7_rank[256] = {
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x3e,0x3f,0xff,0xff,0xff,
    0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,
    0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0xff,0xff,0xff,0xff,0xff,
    0xff,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
    0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x32,0x33,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
};


//uint16_t bom = 0xFEFF;
//fout.write((char*)&bom, 2);
std::string mUtf7ToUnicode16le(const std::string& in)
{
    std::string out;
    out.reserve(in.size() * 2);

    const unsigned char* inptr = (unsigned char*)in.c_str();
    unsigned char c;
    int shifted = 0;
    uint32_t v = 0;
    uint16_t u;
    int i = 0;

    while (*inptr) {
        c = *inptr++;

        if (shifted) {
            if (c == '-') {
                /* shifted back to US-ASCII */
                shifted = 0;
            }
            else {
                /* base64 decode */
                if (utf7_rank[c] == 0xff)
                    goto exception;

                v = (v << 6) | utf7_rank[c];
                i += 6;

                if (i >= 16) {
                    u = (v >> (i - 16)) & 0xffff;

                    char cc = (char)u;
                    out.push_back(cc);

                    cc = u >> 8;
                    out.push_back(cc);

                    i -= 16;
                }
            }
        }
        else if (c == '&') {
            if (*inptr == '-') {
                out.push_back('&');
                inptr++;
            }
            else {
                /* shifted to modified UTF-7 */
                shifted = 1;
            }
        }
        else {
            out.push_back(c);
            out.push_back(0x00);
        }
    }

    if (shifted) {
    exception:
        g_warning("Invalid UTF-7 encoded string: '%s'", in);
        return "";
    }

    return out;
}

std::string Unicode16leToGbk(const std::string& in)
{
    const char* utf16_str = in.c_str();
    char* in_ptr = (char*)utf16_str;
    size_t in_len = in.size();

    std::string gbk_str;
    gbk_str.resize(in.size());

    char* out_ptr = (char*)&gbk_str[0];
    size_t out_len = gbk_str.size();

    iconv_t cd = iconv_open("GBK", "UTF-16LE");

    if ((iconv_t)-1 == cd)
    {
        std::cout << errno << std::endl;
        return "";
    }

    size_t result = iconv(cd, &in_ptr, &in_len, &out_ptr, &out_len);
    iconv_close(cd);

    gbk_str.resize(gbk_str.size() - out_len);
    //*out_ptr = '\0'; // Ìí¼Ó×Ö·û´®½áÊø·û

    return gbk_str;
}

std::string mUtf7ToGbk(const std::string& in)
{
    std::string u16 = mUtf7ToUnicode16le(in);

    return Unicode16leToGbk(u16);
}

std::string Utf8ToGbk(const std::string& in)
{
    const char* utf8_str = in.c_str();
    char* in_ptr = (char*)utf8_str;
    size_t in_len = in.size();

    std::string gbk_str;
    gbk_str.resize(in.size());

    char* out_ptr = (char*)&gbk_str[0];
    size_t out_len = gbk_str.size();

    iconv_t cd = iconv_open("gbk", "utf-8");

    if ((iconv_t)-1 == cd)
    {
        std::cout << errno << std::endl;
        return "";
    }

    size_t result = iconv(cd, &in_ptr, &in_len, &out_ptr, &out_len);
    iconv_close(cd);

    gbk_str.resize(gbk_str.size() - out_len);
    //*out_ptr = '\0'; // Ìí¼Ó×Ö·û´®½áÊø·û

    return gbk_str;
}

std::string GbkToUtf8(const std::string& in)
{
    const char* gbk_str = in.c_str();
    char* in_ptr = (char*)gbk_str;
    size_t in_len = in.size();

    std::string utf8_str;
    utf8_str.resize(in.size() * 4);

    char* out_ptr = (char*)&utf8_str[0];
    size_t out_len = utf8_str.size();

    iconv_t cd = iconv_open("gbk", "utf-8");

    if ((iconv_t)-1 == cd)
    {
        std::cout << errno << std::endl;
        return "";
    }

    size_t result = iconv(cd, &in_ptr, &in_len, &out_ptr, &out_len);
    iconv_close(cd);

    utf8_str.resize(utf8_str.size() - out_len);
    //*out_ptr = '\0'; // Ìí¼Ó×Ö·û´®½áÊø·û

    return utf8_str;
}

std::string Base64ToGbk(const std::string& in)
{
    if (in.size() < 2)
    {
        return in;
    }

    std::string_view inView(in);

    if (in.front() == '\"' && in.back() == '\"')
    {
        inView = inView.substr(1, in.size() - 2);
    }

    int isGbk = 0;

    std::string::size_type pos = std::string::npos;

    static const char* s_fmts[] = { "0=?utf-8?B?", "1=?gbk?B?", "0=?UTF-8?B?", "1=?GBK?B?"};

    for (auto c : s_fmts)
    {
        isGbk = *c == '0' ? 0 : 1;
        c += 1;
        pos = inView.find(c);
        if (pos == std::string::npos)
        {
            continue;
        }

        if (pos != 0)
        {
            return in;
        }

        pos += strlen(c);
        break;
    }

    std::string_view base64View;

    if (pos == std::string::npos) //default is utf8
    {
        pos = 0;
        isGbk = 0;
        base64View = inView;
    }
    else
    {
        if (inView.size() < pos + 2 + 1)
        {
            return in;
        }

        if (inView[inView.size() - 2] != '?' || inView[inView.size() - 1] != '=')
        {
            return in;
        }

        base64View = inView.substr(pos, inView.size() - pos - 2);
    }
        

    auto str = base64_decode(base64View);

    if (!isGbk)
    {
        return Utf8ToGbk(str);
    }

    return str;
}

std::string Base64Decode(const std::string_view& in)
{
    return base64_decode(in);
}

std::string Base64Encode(const std::string_view& in)
{
    return base64_encode(in);
}

std::string ImapTagGen::GetNextTag()
{
    ++_seq;

    int32_t seq = _seq;
    std::string tag = _baseTag;


    int n = 0;

    if (n < 10)
    {
        n = 1;
    }
    else if (n < 100)
    {
        n = 2;
    }
    else if (n < 1000)
    {
        n = 3;
    }
    else if (n < 10000)
    {
        n = 4;
    }

    while (n++ < 4)
    {
        tag.push_back('0');
    }

    return tag + std::to_string(seq);
}

ImapTagGen ImapTagGen::Create()
{
    static std::mutex s_mutex;
    static char s_base[] = "AAAA";

    std::lock_guard lg(s_mutex);

    ImapTagGen gen(s_base);

    for (int i = sizeof(s_base) - 2; i >= 0; --i)
    {
        if (s_base[i] == 'Z')
        {
            s_base[i] = 'A';
            continue;
        }

        ++s_base[i];
        break;
    }

    return gen;
}
