#include "log.h"
#include <cstdio>
#include <cstdarg>

static Log::LogLevel level = Log::Warning;

static int log_vprintf(const char *fmt, va_list args)
{
    char buffer[4096];
    int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
    fwrite(buffer, rc, 1, stdout);
    fflush(stdout);

    return rc;
}

int Log::printf(const char *fmt, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    fwrite(buffer, rc, 1, stdout);
    fflush(stdout);

    return rc;
}

void Log::SetLevel(LogLevel levelIn)
{
    level = levelIn;
}

void Log::V(const char *fmt, ...)
{
    if (level <= Log::Verbose)
    {
        Log::printf("[VERBOSE] ");
        va_list args;
        va_start(args, fmt);
        int rc = log_vprintf(fmt, args);
        va_end(args);
    }
}

void Log::D(const char *fmt, ...)
{
    if (level <= Log::Debug)
    {
        Log::printf("[DEBUG] ");
        va_list args;
        va_start(args, fmt);
        log_vprintf(fmt, args);
        va_end(args);
    }
}

void Log::I(const char *fmt, ...)
{
    if (level <= Log::Info)
    {
        Log::printf("[INFO] ");
        va_list args;
        va_start(args, fmt);
        log_vprintf(fmt, args);
        va_end(args);
    }
}

void Log::W(const char *fmt, ...)
{
    if (level <= Log::Warning)
    {
        Log::printf("[WARNING] ");
        va_list args;
        va_start(args, fmt);
        log_vprintf(fmt, args);
        va_end(args);
    }
}

void Log::E(const char *fmt, ...)
{
    if (level <= Log::Error)
    {
        Log::printf("[ERROR] ");
        va_list args;
        va_start(args, fmt);
        log_vprintf(fmt, args);
        va_end(args);
    }
}


