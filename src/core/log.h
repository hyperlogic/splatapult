#pragma once

struct Log
{
    enum AnsiColor
    {
        BLACK = 0,
        RED = 1,
        GREEN = 2,
        YELLOW = 3,
        BLUE = 4,
        MAGENTA = 5,
        CYAN = 6,
        WHITE = 7
    };

    static int printf(const char *fmt, ...);
    static int printf_ansi(AnsiColor color, const char *fmt, ...);
};
