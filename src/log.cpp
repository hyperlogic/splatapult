#include "log.h"
#include <cstdio>
#include <cstdarg>

int Log::printf(const char *fmt, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    fwrite(buffer, rc, 1, stdout);

    return rc;
}

int Log::printf_ansi(AnsiColor color, const char *fmt, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    fwrite(buffer, rc, 1, stdout);

    return rc;
}
