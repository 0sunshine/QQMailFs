#include "Log.h"
#include "Util.h"
#include "base64.h"

#include <iconv.h>
#include <iostream>
#include <mutex>

/**
 * 辅助函数：解码 Modified Base64 字符
 * 返回 0-63 的值，如果是非法字符返回 -1
 */
int decode_imap_b64(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == ',') return 63; // 注意：标准Base64是'/'，这里是','
    return -1;
}

/**
 * 将 IMAP Modified UTF-7 字符串转换为 Little Endian UTF-16 字节流
 * 输入: std::string (例如 "&bUuL1Q-")
 * 输出: std::string (存储字节流，例如 "\x4B\x6D\xD5\x8B")
 */
std::string mUtf7ToUnicode16le(const std::string& in) {
    std::string result;
    result.reserve(in.length() * 2); // UTF-16 至少是原长度的2倍（对于ASCII）

    for (size_t i = 0; i < in.length(); ++i) {
        char c = in[i];

        if (c == '&') {
            // 检查是否是转义的 '&-'
            if (i + 1 < in.length() && in[i + 1] == '-') {
                // 输出 '&' (0x0026) -> LE: 0x26, 0x00
                result.push_back(0x26);
                result.push_back(0x00);
                i++; // 跳过 '-'
            }
            else {
                // 进入 Base64 解码模式
                i++; // 跳过 '&'
                uint32_t bit_buffer = 0;
                int bit_count = 0;

                //一直读取直到遇到 '-' 或字符串结束
                while (i < in.length() && in[i] != '-') {
                    int val = decode_imap_b64(in[i]);
                    if (val != -1) {
                        bit_buffer = (bit_buffer << 6) | val;
                        bit_count += 6;

                        // 每凑够 16 位，就输出一个 UTF-16 字符
                        if (bit_count >= 16) {
                            bit_count -= 16;
                            // 提取出的 16 位是 Big Endian (因为网络序)
                            uint16_t u16 = (bit_buffer >> bit_count) & 0xFFFF;

                            // 转为 Little Endian 写入 result
                            result.push_back(static_cast<char>(u16 & 0xFF));        // Low byte
                            result.push_back(static_cast<char>((u16 >> 8) & 0xFF)); // High byte
                        }
                    }
                    i++;
                }
                // 循环结束时 input[i] 应该是 '-'，循环外会自动 i++ 跳过它
            }
        }
        else {
            // 普通 ASCII 字符 (0x00XX) -> LE: XX, 00
            result.push_back(c);
            result.push_back(0x00);
        }
    }

    return result;
}

/**
 * 将存储在 std::string 中的 Little Endian UTF-16 字节流转换为 IMAP Modified UTF-7
 * * 输入: std::string (包含原始字节，如 "a\0b\0" 代表 "ab")
 * 输出: std::string (IMAP 文件夹编码格式)
 */
std::string Unicode16leTomUtf7(const std::string& in) {
    // 检查字节数是否为偶数 (UTF-16 必须是 2 字节对齐)
    if (in.size() % 2 != 0) {
        return "";
    }

    std::string result;
    result.reserve(in.size());

    const char* b64_tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,";
    bool in_shift = false;
    uint32_t bit_buffer = 0;
    int bit_count = 0;

    // 辅助 lambda: 关闭 Base64 模式
    auto close_shift = [&](std::string& res, uint32_t& buf, int& bits, bool& shift) {
        if (shift) {
            if (bits > 0) {
                res += b64_tbl[(buf << (6 - bits)) & 0x3F];
            }
            res += '-';
            shift = false;
            buf = 0;
            bits = 0;
        }
        };

    // 步长为 2，每次处理两个字节
    for (size_t i = 0; i + 1 < in.size(); i += 2) {
        // 关键点：手动拼合 Little Endian 字节
        // 必须先转为 unsigned char 防止符号位扩展导致的计算错误
        unsigned char low = static_cast<unsigned char>(in[i]);
        unsigned char high = static_cast<unsigned char>(in[i + 1]);

        // 组合成 16 位字符值
        uint16_t c = (static_cast<uint16_t>(high) << 8) | low;

        // ---------------------------------------------------------
        // 下面是标准的 Modified UTF-7 逻辑 
        // ---------------------------------------------------------

        // 检查是否为可打印 ASCII (0x20 - 0x7E)
        if (c >= 0x20 && c <= 0x7E) {
            close_shift(result, bit_buffer, bit_count, in_shift);

            if (c == '&') {
                result += "&-";
            }
            else {
                result += static_cast<char>(c);
            }
        }
        else {
            if (!in_shift) {
                result += '&';
                in_shift = true;
            }

            // UTF-16BE 顺序推入缓冲区
            bit_buffer = (bit_buffer << 16) | c;
            bit_count += 16;

            while (bit_count >= 6) {
                bit_count -= 6;
                result += b64_tbl[(bit_buffer >> bit_count) & 0x3F];
            }
        }
    }

    close_shift(result, bit_buffer, bit_count, in_shift);
    return result;
}

std::string Unicode16leToGbk(const std::string& in)
{
    const char* utf16_str = in.c_str();
    char* in_ptr = (char*)utf16_str;
    size_t in_len = in.size();

    std::string gbk_str;
    gbk_str.resize(in.size() + 2);

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
    //*out_ptr = '\0'; // 添加字符串结束符

    return gbk_str;
}

std::string GbkToUnicode16le(const std::string& in)
{
    const char* gbk_str = in.c_str();
    char* in_ptr = (char*)gbk_str;
    size_t in_len = in.size();

    std::string utf16_str;
    utf16_str.resize(in.size() + 2);

    char* out_ptr = (char*)&utf16_str[0];
    size_t out_len = utf16_str.size();

    iconv_t cd = iconv_open("UTF-16LE", "GBK");

    if ((iconv_t)-1 == cd)
    {
        std::cout << errno << std::endl;
        return "";
    }

    size_t result = iconv(cd, &in_ptr, &in_len, &out_ptr, &out_len);
    iconv_close(cd);

    utf16_str.resize(utf16_str.size() - out_len);
    //*out_ptr = '\0'; // 添加字符串结束符

    return utf16_str;
}

std::string mUtf7ToGbk(const std::string& in)
{
    std::string u16 = mUtf7ToUnicode16le(in);

    return Unicode16leToGbk(u16);
}

std::string GbkTomUtf7(const std::string& in)
{
    std::string u16 = GbkToUnicode16le(in);

    return Unicode16leTomUtf7(u16);
}

std::string Utf8ToGbk(const std::string& in)
{
    const char* utf8_str = in.c_str();
    char* in_ptr = (char*)utf8_str;
    size_t in_len = in.size();

    std::string gbk_str;
    gbk_str.resize(in.size() + 1);

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
    //*out_ptr = '\0'; // 添加字符串结束符

    return gbk_str;
}

std::string GbkToUtf8(const std::string& in)
{
    const char* gbk_str = in.c_str();
    char* in_ptr = (char*)gbk_str;
    size_t in_len = in.size();

    std::string utf8_str;
    utf8_str.resize(in.size() * 4 + 1);

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
    //*out_ptr = '\0'; // 添加字符串结束符

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
