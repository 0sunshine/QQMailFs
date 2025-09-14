#pragma once

#include <stdio.h>

#define LOG_FMT_DEBUG(fmt, ...) \
    printf("debug: " fmt "\n", __VA_ARGS__);


#define LOG_FMT_INFO(fmt, ...) \
    printf("info: " fmt "\n", __VA_ARGS__);

#define LOG_FMT_WARN(fmt, ...) \
    printf("warn: " fmt "\n", __VA_ARGS__);

#define LOG_FMT_ERROR(fmt, ...) \
    printf("error: " fmt "\n", __VA_ARGS__);