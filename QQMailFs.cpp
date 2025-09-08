#define _CRT_SECURE_NO_WARNINGS

#include "Util.h"

#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <fstream>


#include <iconv.h>

using boost::asio::ip::tcp;
using std::placeholders::_1;
using std::placeholders::_2;

std::string gUsername;
std::string gPassword;


struct ImapResponse
{
    //enum class Status 
    //{
    //    Unkowned,
    //    Ok,
    //    No,
    //    Bad,
    //    Other
    //};

    std::string tag;
    std::string status;
    std::string statusLine;
    std::vector<std::string> msgs;

    void clear()
    {
        tag.clear();
        status.clear();
        statusLine.clear();
        msgs.clear();
    }
};


class ImapClient
{
public:
    ImapClient(boost::asio::io_context& io_context,
        boost::asio::ssl::context& context,
        const tcp::resolver::results_type& endpoints)
        : _socket(io_context, context), _endpoints(endpoints)
    {
        _socket.set_verify_mode(boost::asio::ssl::verify_none);
        _socket.set_verify_callback(
            std::bind(&ImapClient::verify_certificate, this, _1, _2));
    }

    void go(boost::asio::yield_context yield);

    bool wait(ImapResponse& res, boost::asio::yield_context& yield);
    bool send(const std::string& msg, boost::asio::yield_context& yield);

private:
    bool verify_certificate(bool preverified,
        boost::asio::ssl::verify_context& ctx)
    {
        // The verify callback can be used to check whether the certificate that is
        // being presented is valid for the peer. For example, RFC 2818 describes
        // the steps involved in doing this for HTTPS. Consult the OpenSSL
        // documentation for more details. Note that the callback is called once
        // for each certificate in the certificate chain, starting from the root
        // certificate authority.

        // In this example we will simply print the certificate's subject name.
        char subject_name[256];
        X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
        X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
        std::cout << "Verifying " << subject_name << "\n";

        return preverified;
    }


    boost::asio::ssl::stream<tcp::socket> _socket;
    tcp::resolver::results_type _endpoints;
};


int main(int argc, char* argv[])
{
    std::ifstream fin("passwd.txt");
    if (!fin)
    {
        std::cout << "no passwd.txt" << std::endl;
        return -1;
    }

    std::getline(fin, gUsername);
    std::getline(fin, gPassword);

    try
    {
        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("imap.qq.com", "993");

        boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
        //ctx.load_verify_file("ca.pem");

        auto c = std::make_shared<ImapClient>(io_context, ctx, endpoints);

        boost::asio::spawn(io_context, std::bind(&ImapClient::go, c, _1), [](std::exception_ptr e)
            {
                if (e)
                    std::rethrow_exception(e);
            });

        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}

void ImapClient::go(boost::asio::yield_context yield)
{   
    boost::system::error_code ec;

    boost::asio::async_connect(_socket.lowest_layer(), _endpoints, yield[ec]);
    if (ec)
    {
        std::cout << "connect error: " << ec.message() << std::endl;
    }

    _socket.async_handshake(boost::asio::ssl::stream_base::client, yield[ec]);
    if (ec)
    {
        std::cout << "handshake error: " << ec.message() << std::endl;
    }

    ImapResponse res;
    std::string msg;
    //std::string msg = "A001 CAPABILITY\r\n";


    //Arguments: user name
    //password
    //Responses : no specific responses for this command
    //Result : OK - login completed, now in authenticated state
    //NO - login failure : user name or password rejected
    //BAD - command unknown or arguments invalid
    msg = "A001 LOGIN " + gUsername + " " + gPassword + "\r\n";
    if (!send(msg, yield))
    {
        return;
    }

    res.clear();
    if (!wait(res, yield))
    {
        return;
    }

    msg = "A002 CAPABILITY\r\n";
    if (!send(msg, yield))
    {
        return;
    }

    res.clear();
    if (!wait(res, yield))
    {
        return;
    }

    //Arguments: reference name
    //mailbox name with possible wildcards
    //Responses : untagged responses : LIST
    //Result : OK - list completed
    //NO - list failure : can’t list that reference or name
    //BAD - command unknown or arguments invalid
    msg = R"(A003 LIST "" "%")""\r\n";
    if (!send(msg, yield))
    {
        return;
    }

    res.clear();
    if (!wait(res, yield))
    {
        return;
    }

    std::cout << "end......." << std::endl;
}

bool ImapClient::wait(ImapResponse& res, boost::asio::yield_context& yield)
{
    boost::system::error_code ec;

    std::string s;
    s.resize(1024 * 64); //64k

    int64_t pos = 0; 

    while (true)
    {
        boost::asio::mutable_buffer(&s[pos], s.size() - pos);
        int bytes = _socket.async_read_some(boost::asio::buffer(s), yield[ec]);
        pos += bytes;
        
        if (ec)
        {
            std::cout << "async_read_some error: " << ec.message() << std::endl;
            return false;
        }

        while (true)
        {
            auto delimPos = s.find("\r\n");
            if (delimPos != std::string::npos)
            {
                std::string line(&s[0], delimPos);
                s.erase(0, delimPos + 2);
                pos -= (delimPos + 2);

                if (line[0] == '*'){
                    res.msgs.emplace_back(std::move(line));
                }
                else {
                    auto tagEndPos = line.find(' ');
                    if (tagEndPos == std::string::npos) {
                        std::cout << "recv error response: " << line << std::endl;
                        return false;
                    }
                    res.tag = std::string(&line[0], tagEndPos);

                    auto statusEndPos = line.find(' ', tagEndPos + 1);
                    if (statusEndPos == std::string::npos) {
                        std::cout << "recv error response: " << line << std::endl;
                        return false;
                    }
                    res.status = std::string(&line[tagEndPos + 1], statusEndPos - tagEndPos - 1);

                    res.statusLine = line;

                    return true;
                }
            }
            else
            {
                break;
            }
        }

        if (s.size() - pos <= 0)
        {
            s.resize(s.size() * 2);
        }

    }


    return false;
}

bool ImapClient::send(const std::string& msg, boost::asio::yield_context& yield)
{
    boost::system::error_code ec;

    auto bytes = boost::asio::async_write(_socket, boost::asio::buffer(msg), yield[ec]);
    if (ec)
    {
        std::cout << "async_write error: " << ec.message() << std::endl;
        return false;
    }

    if (bytes != msg.size())
    {
        std::cout << "boost::asio::async_write: bytes != msg.size()" << std::endl;
        return false;
    }


    return true;
}
