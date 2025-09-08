#include "AsioIOScheduler.h"

#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include <functional>

using std::placeholders::_1;
using std::placeholders::_2;
using boost::asio::ip::tcp;

class AsioIOClient : public IOClientBase
{
public:
    AsioIOClient(boost::asio::io_context& ioContext, boost::asio::ssl::context& sslContext, boost::asio::yield_context& yield);

    int Connect(const std::string& host, uint16_t port, int32_t msTimeout = -1) override;
    int Read(uint8_t* buffer, int len) override;
    int Write(uint8_t const* buffer, int len) override;

    //void SetYield(boost::asio::yield_context& yield) { _yield = yield; }

private:
    bool verify_certificate(bool preverified,boost::asio::ssl::verify_context& ctx);

private:
    boost::asio::io_context& _ioContext;
    boost::asio::ssl::stream<tcp::socket> _socket;
    boost::asio::yield_context _yield;

};


AsioIOScheduler::AsioIOScheduler()
    :_sslContext(boost::asio::ssl::context::sslv23)
{

}

int AsioIOScheduler::Run()
{
    for (auto& task : _tasks)
    {
        boost::asio::spawn(_ioContext, [this, task](boost::asio::yield_context yield) {
            AsioIOClient client{ _ioContext, _sslContext, yield };
            task(client);
        },
        [](std::exception_ptr e)
        {
            if (e)
                std::rethrow_exception(e);
        });
    }

    _tasks.clear();
    _tasks.shrink_to_fit();


    _ioContext.run();

    return 0;
}

int AsioIOScheduler::AddTask(TaskType task)
{
    _tasks.emplace_back(std::move(task));
    return 0;
}

AsioIOClient::AsioIOClient(boost::asio::io_context& ioContext, boost::asio::ssl::context& sslContext, boost::asio::yield_context& yield)
    :_ioContext(ioContext), _socket(ioContext, sslContext),_yield(yield)
{
}

int AsioIOClient::Connect(const std::string& host, uint16_t port, int32_t msTimeout)
{
    _socket.set_verify_mode(boost::asio::ssl::verify_none);
    _socket.set_verify_callback(
        std::bind(&AsioIOClient::verify_certificate, this, _1, _2));

    boost::system::error_code ec;

    tcp::resolver resolver(_ioContext);
    auto endpoints = resolver.resolve(host, std::to_string(port));
    boost::asio::async_connect(_socket.lowest_layer(), endpoints, _yield[ec]);
    if (ec)
    {
        std::cout << "connect error: " << ec.message() << std::endl;
        return -1;
    }

    _socket.async_handshake(boost::asio::ssl::stream_base::client, _yield[ec]);
    if (ec)
    {
        std::cout << "handshake error: " << ec.message() << std::endl;
        return -1;
    }

    return 0;
}

int AsioIOClient::Read(uint8_t* buffer, int len)
{
    boost::system::error_code ec;
    int bytes = _socket.async_read_some(boost::asio::buffer(buffer, len), _yield[ec]);

    if (ec)
    {
        std::cout << "async_read_some error: " << ec.message() << std::endl;
        return -1;
    }

    return bytes;
}

int AsioIOClient::Write(uint8_t const* buffer, int len)
{
    boost::system::error_code ec;
    auto bytes = boost::asio::async_write(_socket, boost::asio::buffer(buffer, len), _yield[ec]);

    if (ec)
    {
        std::cout << "async_write error: " << ec.message() << std::endl;
        return -1;
    }

    if (bytes != len)
    {
        std::cout << "boost::asio::async_write: bytes != len" << std::endl;
        return -1;
    }

    return bytes;
}

bool AsioIOClient::verify_certificate(bool preverified, boost::asio::ssl::verify_context& ctx)
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
