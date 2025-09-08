#pragma once

#include <string>
#include <stdint.h>
#include <functional>

class IOClientBase
{
public:
    virtual ~IOClientBase() {}

    virtual int Connect(const std::string& host, uint16_t port, int32_t msTimeout = -1) = 0;
    virtual int Read(uint8_t *buffer, int len) = 0;
    virtual int Write(uint8_t const *buffer, int len) = 0;
};


class IOScheduler
{
public:
    virtual ~IOScheduler() {}

    virtual int Run() = 0;
    virtual int AddTask(std::function<void(IOClientBase& client)> task) = 0;
};
