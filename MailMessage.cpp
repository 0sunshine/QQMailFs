#include "MailMessage.h"
#include "Log.h"
#include "Util.h"

/***

From: mailio library <mailio@mailserver.com>
To: mailio library <mailio@mailserver.com>
Date: Mon, 22 Dec 2025 13:50:02 +0000
MIME-Version: 1.0
Content-Type: multipart/related; boundary="012456789@mailio.dev"
Subject: smtps multipart message

--012456789@mailio.dev
Content-Type: text/html; charset=utf-8
Content-Transfer-Encoding: 8bit

<html><head></head><body><h1>i am title!</h1></body></html>

--012456789@mailio.dev
Content-Type: image/png;
  name="a0.png"
Content-Transfer-Encoding: Base64
Content-Disposition: inline;
  filename="a0.png"

MTIzNDU2

--012456789@mailio.dev--

***/



bool MailMessage::FormatTo(std::string& result)
{
    std::string strTmp;
    strTmp.reserve(1024 * 1024 * 1); //1M

    const std::string strBoundary = "012456789@mailio.dev";

    strTmp = strTmp + "From: " + from + "\r\n";
    strTmp = strTmp + "To: " + to + "\r\n";
    strTmp = strTmp + "Date: " + date + "\r\n";
    strTmp = strTmp + "MIME-Version: " + "1.0\r\n";
    strTmp = strTmp + "Content-Type: " + "multipart/related; boundary=\"" + strBoundary + "\"\r\n";
    strTmp = strTmp + "Subject: " + subject + "\r\n";

    strTmp += "\r\n";
    strTmp = strTmp + "--" + strBoundary + "\r\n";

    strTmp = strTmp + "Content-Type: text/html; charset=utf-8\r\n";
    strTmp = strTmp + "Content-Transfer-Encoding: 8bit\r\n";
    strTmp = strTmp + GbkToUtf8(gbkText) + "\r\n";

    strTmp += "\r\n";
    strTmp = strTmp + "--" + strBoundary + "\r\n";

    for (auto& file : files)
    {
        strTmp = strTmp + "Content-Type: application/octet-stream; name=\"" + GbkToUtf8(file.gbkName) + "\"\r\n";
        strTmp = strTmp + "Content-Transfer-Encoding: Base64\r\n";
        strTmp = strTmp + "Content-Disposition: inline; filename=\"" + GbkToUtf8(file.gbkName) + "\"\r\n";
        strTmp = strTmp + Base64Encode(file.binData) + "\r\n";

        strTmp += "\r\n";
        strTmp = strTmp + "--" + strBoundary + "\r\n";
    }

    result = strTmp;

    return true;
}
