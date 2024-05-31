/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

// 3d gaussian splat renderer

#include <chrono>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_syswm.h>
#include <stdint.h>
#include <thread>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define FrameMark do {} while(0)
#endif

#include "core/log.h"
#include "core/util.h"

#include "app.h"

//#define SOFTWARE_SPLATS

struct GlobalContext
{
    bool quitting = false;
    SDL_Window* window = NULL;
    SDL_GLContext gl_context;
};

GlobalContext ctx;

int SDLCALL Watch(void *userdata, SDL_Event* event)
{
    if (event->type == SDL_APP_WILLENTERBACKGROUND)
    {
        ctx.quitting = true;
    }

    return 1;
}

int main(int argc, char *argv[])
{
    Log::SetAppName("splataplut");
    MainContext mainContext;
    App app(mainContext);
    App::ParseResult parseResult = app.ParseArguments(argc, (const char**)argv);
    switch (parseResult)
    {
    case App::SUCCESS_RESULT:
        break;
    case App::ERROR_RESULT:
        Log::E("App::ParseArguments failed!\n");
        return 1;
    case App::QUIT_RESULT:
        return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK) != 0)
    {
        Log::E("Failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    const int32_t WIDTH = 1024;
    const int32_t HEIGHT = 768;

    // Allow us to use automatic linear->sRGB conversion.
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    // increase depth buffer size
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    uint32_t windowFlags = SDL_WINDOW_OPENGL;
    if (app.IsFullscreen())
    {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    else
    {
        windowFlags |= SDL_WINDOW_RESIZABLE;
    }
    ctx.window = SDL_CreateWindow("splatapult", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, windowFlags);

    if (!ctx.window)
    {
        Log::E("Failed to create window: %s\n", SDL_GetError());
        return 1;
    }

    ctx.gl_context = SDL_GL_CreateContext(ctx.window);

    SDL_GL_MakeCurrent(ctx.window, ctx.gl_context);

#ifdef __linux__
    // Initialize context from the SDL window
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version)
    auto ret = SDL_GetWindowWMInfo(ctx.window, &info);
    if (ret != SDL_TRUE)
    {
        Log::W("Failed to retrieve SDL window info: %s\n", SDL_GetError());
    }
    else
    {
        mainContext.xdisplay = info.info.x11.display;
        mainContext.glxDrawable = (GLXWindow)info.info.x11.window;
        mainContext.glxContext = (GLXContext)ctx.gl_context;
    }
#endif

    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        Log::E("Error: %s\n", glewGetErrorString(err));
        return 1;
    }

    // AJT: TODO REMOVE disable vsync for benchmarks
    SDL_GL_SetSwapInterval(0);

    SDL_AddEventWatch(Watch, NULL);

    if (!app.Init())
    {
        Log::E("App::Init failed\n");
        return 1;
    }

    bool shouldQuit = false;
    app.OnQuit([&shouldQuit]()
    {
        shouldQuit = true;
    });

    app.OnResize([](int newWidth, int newHeight)
    {
        //SDL_RenderSetLogicalSize(ctx.renderer, newWidth, newHeight);
        // AJT: TODO resize texture?
    });

    uint32_t frameCount = 1;
    uint32_t frameTicks = SDL_GetTicks();
    uint32_t lastTicks = SDL_GetTicks();
    while (!ctx.quitting && !shouldQuit)
    {
        // update dt
        uint32_t ticks = SDL_GetTicks();

        const int FPS_FRAMES = 100;
        if ((frameCount % FPS_FRAMES) == 0)
        {
            float delta = (ticks - frameTicks) / 1000.0f;
            float fps = (float)FPS_FRAMES / delta;
            frameTicks = ticks;
            app.UpdateFps(fps);
        }
        float dt = (ticks - lastTicks) / 1000.0f;
        lastTicks = ticks;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            app.ProcessEvent(event);
        }

        if (!app.Process(dt))
        {
            Log::E("App::Process failed!\n");
            return 1;
        }

        SDL_GL_MakeCurrent(ctx.window, ctx.gl_context);

        int width, height;
        SDL_GetWindowSize(ctx.window, &width, &height);
        if (!app.Render(dt, glm::ivec2(width, height)))
        {
            Log::E("App::Render failed!\n");
            return 1;
        }

        SDL_GL_SwapWindow(ctx.window);

        frameCount++;

        FrameMark;
    }

    SDL_DelEventWatch(Watch, NULL);
    SDL_GL_DeleteContext(ctx.gl_context);

    SDL_DestroyWindow(ctx.window);
    SDL_Quit();

    return 0;
}
