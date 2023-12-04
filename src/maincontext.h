#pragma once

#if defined(__ANDROID__)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <jni.h>
#include <android_native_app_glue.h>
#endif

#if defined(__ANDROID__)
    struct MainContext
    {
        EGLDisplay display;
        EGLConfig config;
        EGLContext context;
        android_app* androidApp;
    };
#else
    struct MainContext
    {
    };
#endif

