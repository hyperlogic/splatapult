/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#if defined(__ANDROID__)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <jni.h>
#include <android_native_app_glue.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/xf86vmode.h>  // for fullscreen video mode
#include <X11/extensions/Xrandr.h>     // for resolution changes
#include <GL/glew.h>
#include <GL/glx.h>

// Conficts with core/optionparser.h
#ifdef None
#undef None
#endif

#endif

#if defined(__ANDROID__)
    struct MainContext
    {
        EGLDisplay display;
        EGLConfig config;
        EGLContext context;
        android_app* androidApp;
    };
#elif defined(__linux__)
    struct MainContext
    {
        Display* xdisplay;
        uint32_t visualid;
        GLXFBConfig glxFBConfig;
        GLXDrawable glxDrawable;
        GLXContext glxContext;
    };

#else
    struct MainContext
    {
    };
#endif

