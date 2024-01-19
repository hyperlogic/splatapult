/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <string>

struct Log
{
    enum LogLevel
    {
        Verbose = 0,
        Debug = 1,
        Info = 2,
        Warning = 3,
        Error = 4
    };

    static void SetLevel(LogLevel levelIn);
    static void SetAppName(const std::string& appNameIn);

    // verbose
    static void V(const char *fmt, ...);

    // debug
    static void D(const char *fmt, ...);

    // info
    static void I(const char *fmt, ...);

    // warning
    static void W(const char *fmt, ...);

    // error
    static void E(const char *fmt, ...);

private:
    static int printf(const char *fmt, ...);
};
