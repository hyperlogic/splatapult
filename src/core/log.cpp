#include "log.h"
#include <cstdio>
#include <cstdarg>

#ifdef ANDROID
#include <android/log.h>
#endif

static Log::LogLevel level = Log::Verbose;
static std::string appName = "Core";

#ifdef ANDROID
static int log_vprintf(int prio, const char *fmt, va_list args)
{
    char buffer[4096];
    int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
    __android_log_write(prio, appName.c_str(), buffer);
    return rc;
}
#else
static int log_vprintf(const char *fmt, va_list args)
{
    char buffer[4096];
    int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
    fwrite(buffer, rc, 1, stdout);
    fflush(stdout);
    return rc;
}
#endif

int Log::printf(const char *fmt, ...)
{
#ifdef ANDROID
    return 0;
#else
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    fwrite(buffer, rc, 1, stdout);
    fflush(stdout);

    return rc;
#endif
}

void Log::SetLevel(LogLevel levelIn)
{
    level = levelIn;
}

void SetAppName(const char* appNameIn)
{
    appName = appNameIn;
}

void Log::V(const char *fmt, ...)
{
    if (level <= Log::Verbose)
    {
        Log::printf("[VERBOSE] ");
        va_list args;
        va_start(args, fmt);
#ifdef ANDROID
        log_vprintf(ANDROID_LOG_VERBOSE, fmt, args);
#else
        log_vprintf(fmt, args);
#endif
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
#ifdef ANDROID
        log_vprintf(ANDROID_LOG_DEBUG, fmt, args);
#else
        log_vprintf(fmt, args);
#endif
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
#ifdef ANDROID
        log_vprintf(ANDROID_LOG_INFO, fmt, args);
#else
        log_vprintf(fmt, args);
#endif
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
#ifdef ANDROID
        log_vprintf(ANDROID_LOG_WARNING, fmt, args);
#else
        log_vprintf(fmt, args);
#endif
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
#ifdef ANDROID
        log_vprintf(ANDROID_LOG_ERROR, fmt, args);
#else
        log_vprintf(fmt, args);
#endif
        va_end(args);
    }
}


