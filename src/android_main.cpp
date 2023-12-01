#include <android/native_window_jni.h> // for native window JNI
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <jni.h>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )

#include "core/log.h"
#include "core/xrbuddy.h"

/*
static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;
static const int NUM_MULTI_SAMPLES = 4;
*/
static const char* EglErrorString(const EGLint error) {
    switch (error) {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "unknown";
    }
}

struct AppContext
{
    AppContext() : resumed(false), sessionActive(false) {}
    bool resumed;
    bool sessionActive;

    struct EGLInfo
    {
        EGLInfo() : majorVersion(0), minorVersion(0), display(0), config(0), context(EGL_NO_CONTEXT) {}
        EGLint majorVersion;
        EGLint minorVersion;
        EGLDisplay display;
        EGLConfig config;
        EGLContext context;
    };

    EGLInfo egl;

    void Clear()
    {
        resumed = false;
        sessionActive = false;
        egl.majorVersion = 0;
        egl.minorVersion = 0;
        egl.display = 0;
        egl.config = 0;
        egl.context = EGL_NO_CONTEXT;
    }
};

/**
 * Process the next main command.
 */
static void app_handle_cmd(struct android_app* androidApp, int32_t cmd)
{
    AppContext& ctx = *(AppContext*)androidApp->userData;

    switch (cmd)
    {
    // There is no APP_CMD_CREATE. The ANativeActivity creates the
    // application thread from onCreate(). The application thread
    // then calls android_main().
    case APP_CMD_START:
        Log::D("onStart()\n");
        Log::D("    APP_CMD_START\n");
        break;
    case APP_CMD_RESUME:
        Log::D("onResume()\n");
        Log::D("    APP_CMD_RESUME\n");
        ctx.resumed = true;
        break;
    case APP_CMD_PAUSE:
        Log::D("onPause()\n");
        Log::D("    APP_CMD_PAUSE\n");
        ctx.resumed = false;
        break;
    case APP_CMD_STOP:
        Log::D("onStop()\n");
        Log::D("    APP_CMD_STOP\n");
        break;
    case APP_CMD_DESTROY:
        Log::D("onDestroy()\n");
        Log::D("    APP_CMD_DESTROY\n");
        ctx.Clear();
        break;
    case APP_CMD_INIT_WINDOW:
        Log::D("surfaceCreated()\n");
        Log::D("    APP_CMD_INIT_WINDOW\n");
        break;
    case APP_CMD_TERM_WINDOW:
        Log::D("surfaceDestroyed()\n");
        Log::D("    APP_CMD_TERM_WINDOW\n");
        break;
    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* androidApp)
{
    Log::SetAppName("splataplut");

    Log::D("----------------------------------------------------------------\n");
    Log::D("android_app_entry()\n");
    Log::D("    android_main()\n");

    JNIEnv* Env;
    (*androidApp->activity->vm).AttachCurrentThread(&Env, nullptr);

    // Note that AttachCurrentThread will reset the thread name.
    prctl(PR_SET_NAME, (long)"android_main", 0, 0, 0);

    AppContext ctx;
    androidApp->userData = &ctx;
    androidApp->onAppCmd = app_handle_cmd;

    // create the egl context
    ctx.egl.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(ctx.egl.display, &ctx.egl.majorVersion, &ctx.egl.minorVersion);
    Log::D("OpenGLES majorVersion = %d, minorVersion = %d\n", ctx.egl.majorVersion, ctx.egl.minorVersion);
    const int MAX_CONFIGS = 1024;
    EGLConfig configs[MAX_CONFIGS];
    EGLint numConfigs = 0;
    if (!eglGetConfigs(ctx.egl.display, configs, MAX_CONFIGS, &numConfigs))
    {
        Log::E("eglGetConfigs failed: %s\n", EglErrorString(eglGetError()));
        return;
    }
    const EGLint configAttribs[] = {EGL_RED_SIZE, 8,
                                    EGL_GREEN_SIZE, 8,
                                    EGL_BLUE_SIZE, 8,
                                    EGL_ALPHA_SIZE, 8, // need alpha for the multi-pass timewarp compositor
                                    EGL_DEPTH_SIZE, 0,
                                    EGL_STENCIL_SIZE, 0,
                                    EGL_SAMPLES, 0,
                                    EGL_NONE};
    ctx.egl.config = 0;
    for (int i = 0; i < numConfigs; i++)
    {
        EGLint value = 0;
        eglGetConfigAttrib(ctx.egl.display, configs[i], EGL_RENDERABLE_TYPE, &value);
        if ((value & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR) {
            continue;
        }
        // The pbuffer config also needs to be compatible with normal window rendering
        // so it can share textures with the window context.
        eglGetConfigAttrib(ctx.egl.display, configs[i], EGL_SURFACE_TYPE, &value);
        if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) != (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) {
            continue;
        }

        int j = 0;
        for (; configAttribs[j] != EGL_NONE; j += 2) {
            eglGetConfigAttrib(ctx.egl.display, configs[i], configAttribs[j], &value);
            if (value != configAttribs[j + 1]) {
                break;
            }
        }
        if (configAttribs[j] == EGL_NONE) {
            ctx.egl.config = configs[i];
            break;
        }
    }
    if (ctx.egl.config == 0)
    {
        Log::E("eglChooseConfig() failed: %s\n", EglErrorString(eglGetError()));
        return;
    }
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    ctx.egl.context = eglCreateContext(ctx.egl.display, ctx.egl.config, EGL_NO_CONTEXT, contextAttribs);
    if (ctx.egl.context == EGL_NO_CONTEXT)
    {
        Log::E("eglCreateContext() failed: %s", EglErrorString(eglGetError()));
        return;
    }

    while (androidApp->destroyRequested == 0)
    {
        // Read all pending events.
        for (;;) {
            int events;
            struct android_poll_source* source;
            // If the timeout is zero, returns immediately without blocking.
            // If the timeout is negative, waits indefinitely until an event appears.
            const int timeoutMilliseconds = (ctx.resumed == false && ctx.sessionActive == false &&
                                             androidApp->destroyRequested == 0)
                ? -1
                : 0;
            if (ALooper_pollAll(timeoutMilliseconds, NULL, &events, (void**)&source) < 0) {
                break;
            }

            // Process this event.
            if (source != NULL) {
                source->process(androidApp, source);
            }
        }

        // xr stuff goes here...
    }

    Log::D("Finished!\n");

    (*androidApp->activity->vm).DetachCurrentThread();
}
