#pragma once

#include "IOSchedulerBase.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <vector>

class AsioIOScheduler:public IOScheduler
{
public:
    using TaskType = std::function<void(IOClientBase& client)>;
    AsioIOScheduler();
    int Run() override;
    int AddTask(TaskType task) override;

private:
    boost::asio::io_context _ioContext;
    boost::asio::ssl::context _sslContext;

    std::vector<TaskType> _tasks;
};

