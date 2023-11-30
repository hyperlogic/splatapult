#include <android/native_window_jni.h> // for native window JNI
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <jni.h>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )

#include "core/log.h"

/*
static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;
static const int NUM_MULTI_SAMPLES = 4;
*/

struct AppContext
{
    AppContext() : resumed(false), sessionActive(false) {}
    bool resumed;
    bool sessionActive;
    void Clear()
    {
        resumed = false;
        sessionActive = false;
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
