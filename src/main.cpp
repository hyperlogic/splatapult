// 3d gaussian splat renderer

#include <algorithm>
#include <cassert>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <memory>
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdint.h>
#include <stdlib.h> //rand()
#include <tracy/Tracy.hpp>

#include "cameras.h"
#include "debugdraw.h"
#include "flycam.h"
#include "gaussiancloud.h"
#include "image.h"
#include "joystick.h"
#include "log.h"
#include "magiccarpet.h"
#include "pointcloud.h"
#include "pointrenderer.h"
#include "program.h"
#include "radix_sort.hpp"
#include "raymarchrenderer.h"
#include "splatrenderer.h"
#include "texture.h"
#include "util.h"
#include "vertexbuffer.h"
#include "xrbuddy.h"

static bool quitting = false;
static SDL_Window *window = NULL;
static SDL_GLContext gl_context;
static SDL_Renderer *renderer = NULL;

const float Z_NEAR = 0.1f;
const float Z_FAR = 1000.0f;
const float FOVY = glm::radians(45.0f);

//#define USE_RAY_MARCH_RENDERER
#define USE_OPENXR

void Clear(bool setViewport = true)
{
    SDL_GL_MakeCurrent(window, gl_context);

    if (setViewport)
    {
        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
    }

    // pre-multiplied alpha blending
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glm::vec4 clearColor(0.0f, 0.0f, 0.1f, 1.0f);
    //clearColor.r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // enable writes to depth buffer
    glEnable(GL_DEPTH_TEST);

    // enable alpha test
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.01f);
}

void Resize(int newWidth, int newHeight)
{
    SDL_GL_MakeCurrent(window, gl_context);

    glViewport(0, 0, newWidth, newHeight);
}

std::shared_ptr<PointCloud> LoadPointCloud()
{
    auto pointCloud = std::make_shared<PointCloud>();

    /*
    if (!pointCloud->ImportPly("data/train/input.ply"))
    {
        Log::printf("Error loading PointCloud!\n");
        return nullptr;
    }
    */

    //
    // make an example pointVec, that contain three lines one for each axis.
    //
    std::vector<PointCloud::Point>& pointVec = pointCloud->GetPointVec();
    const float AXIS_LENGTH = 1.0f;
    const int NUM_POINTS = 10;
    const float DELTA = (AXIS_LENGTH / (float)NUM_POINTS);
    pointVec.resize(NUM_POINTS * 3);
    // x axis
    for (int i = 0; i < NUM_POINTS; i++)
    {
        PointCloud::Point& p = pointVec[i];
        p.position[0] = i * DELTA;
        p.position[1] = 0.0f;
        p.position[2] = 0.0f;
        p.color[0] = 255;
        p.color[1] = 0;
        p.color[2] = 0;
    }
    // y axis
    for (int i = 0; i < NUM_POINTS; i++)
    {
        PointCloud::Point& p = pointVec[i + NUM_POINTS];
        p.position[0] = 0.0f;
        p.position[1] = i * DELTA;
        p.position[2] = 0.0f;
        p.color[0] = 0;
        p.color[1] = 255;
        p.color[2] = 0;
    }
    // z axis
    for (int i = 0; i < NUM_POINTS; i++)
    {
        PointCloud::Point& p = pointVec[i + 2 * NUM_POINTS];
        p.position[0] = 0.0f;
        p.position[1] = 0.0f;
        p.position[2] = i * DELTA;
        p.color[0] = 0;
        p.color[1] = 0;
        p.color[2] = 255;
    }

    // AJT: move splats by offset.
    glm::vec3 offset(0.0f, 0.0f, -5.0f);
    for (auto&& p : pointVec)
    {
        p.position[0] += offset.x;
        p.position[1] += offset.y;
        p.position[2] += offset.z;
    }

    return pointCloud;
}

std::shared_ptr<GaussianCloud> LoadGaussianCloud()
{
    auto gaussianCloud = std::make_shared<GaussianCloud>();

    if (!gaussianCloud->ImportPly("data/train/point_cloud.ply"))
    {
        Log::printf("Error loading GaussianCloud!\n");
        return nullptr;
    }

    /*
    //
    // make an example GaussianClound, that contain red, green and blue axes.
    //
    std::vector<GaussianCloud::Gaussian>& gaussianVec = gaussianCloud->GetGaussianVec();
    const float AXIS_LENGTH = 1.0f;
    const int NUM_SPLATS = 5;
    const float DELTA = (AXIS_LENGTH / (float)NUM_SPLATS);
    const float S = logf(0.05f);
    const float SH_C0 = 0.28209479177387814f;
    const float SH_ONE = 1.0f / (2.0f * SH_C0);
    const float SH_ZERO = -1.0f / (2.0f * SH_C0);

    // x axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        GaussianCloud::Gaussian g;
        memset(&g, 0, sizeof(GaussianCloud::Gaussian));
        g.position[0] = i * DELTA + DELTA;
        g.position[1] = 0.0f;
        g.position[2] = 0.0f;
        // red
        g.f_dc[0] = SH_ONE; g.f_dc[1] = SH_ZERO; g.f_dc[2] = SH_ZERO;
        g.opacity = 100.0f;
        g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
        g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
        gaussianVec.push_back(g);
    }
    // y axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        GaussianCloud::Gaussian g;
        memset(&g, 0, sizeof(GaussianCloud::Gaussian));
        g.position[0] = 0.0f;
        g.position[1] = i * DELTA + DELTA;
        g.position[2] = 0.0f;
        // green
        g.f_dc[0] = SH_ZERO; g.f_dc[1] = SH_ONE; g.f_dc[2] = SH_ZERO;
        g.opacity = 100.0f;
        g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
        g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
        gaussianVec.push_back(g);
    }
    // z axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        GaussianCloud::Gaussian g;
        memset(&g, 0, sizeof(GaussianCloud::Gaussian));
        g.position[0] = 0.0f;
        g.position[1] = 0.0f;
        g.position[2] = i * DELTA + DELTA;
        // blue
        g.f_dc[0] = SH_ZERO; g.f_dc[1] = SH_ZERO; g.f_dc[2] = SH_ONE;
        g.opacity = 100.0f;
        g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
        g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
        gaussianVec.push_back(g);
    }
    GaussianCloud::Gaussian g;
    memset(&g, 0, sizeof(GaussianCloud::Gaussian));
    g.position[0] = 0.0f;
    g.position[1] = 0.0f;
    g.position[2] = 0.0f;
    // white
    g.f_dc[0] = SH_ONE; g.f_dc[1] = SH_ONE; g.f_dc[2] = SH_ONE;
    g.opacity = 100.0f;
    g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
    g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
    gaussianVec.push_back(g);

    // AJT: move splats by offset.
    glm::vec3 offset(0.0f, 0.0f, -5.0f);
    for (auto&& g : gaussianVec)
    {
        g.position[0] += offset.x;
        g.position[1] += offset.y;
        g.position[2] += offset.z;
    }
    */

    return gaussianCloud;
}

int SDLCALL Watch(void *userdata, SDL_Event* event)
{
    if (event->type == SDL_APP_WILLENTERBACKGROUND)
    {
        quitting = true;
    }

    return 1;
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK) != 0)
    {
        Log::printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    const int32_t WIDTH = 1024;
    const int32_t HEIGHT = 768;
    //const int32_t WIDTH = 2160;
    //const int32_t HEIGHT = 2224;

#ifdef USE_RAY_MARCH_RENDERER
    window = SDL_CreateWindow("3dgstoy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT,
                              SDL_WINDOW_RESIZABLE);

    gl_context = nullptr;

    renderer = SDL_CreateRenderer(window, -1, 0);
	if (!renderer)
    {
        Log::printf("Failed to SDL Renderer: %s\n", SDL_GetError());
		return 1;
	}

#else
    window = SDL_CreateWindow("3dgstoy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    gl_context = SDL_GL_CreateContext(window);

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer)
    {
        Log::printf("Failed to SDL Renderer: %s\n", SDL_GetError());
		return 1;
	}

    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        Log::printf("Error: %s\n", glewGetErrorString(err));
        return 1;
    }

    SDL_GL_MakeCurrent(window, gl_context);
    if (!DebugDraw_Init())
    {
        Log::printf("DebugDraw Init failed\n");
        return 1;
    }

#ifdef USE_OPENXR
    XrBuddy xrBuddy;
    if (!xrBuddy.Init())
    {
        Log::printf("OpenXR Init failed\n");
        return 1;
    }
#endif
#endif

    JOYSTICK_Init();

    SDL_AddEventWatch(Watch, NULL);

    auto cameras = std::make_shared<Cameras>();
    if (!cameras->ImportJson("data/train/cameras.json"))
    {
        Log::printf("Error loading cameras.json\n");
        return 1;
    }

    auto pointCloud = LoadPointCloud();
    if (!pointCloud)
    {
        Log::printf("Error loading PointCloud\n");
        return 1;
    }

    auto gaussianCloud = LoadGaussianCloud();

#ifdef USE_OPENXR
    MagicCarpet magicCarpet(glm::mat4(1.0f));
#else
    const float MOVE_SPEED = 2.5f;
    const float ROT_SPEED = 1.0f;
    FlyCam flyCam(glm::vec3(0.0f, 0.0f, 2.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), MOVE_SPEED, ROT_SPEED);
    //FlyCam flyCam(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), MOVE_SPEED, ROT_SPEED);

    int cameraIndex = 0;
    flyCam.SetCameraMat(cameras->GetCameraVec()[cameraIndex]);
#endif

    SDL_JoystickEventState(SDL_ENABLE);

#ifdef USE_RAY_MARCH_RENDERER
    auto rayMarchRenderer = std::make_shared<RayMarchRenderer>();
    if (!rayMarchRenderer->Init(gaussianCloud))
    {
        Log::printf("Error initializing ray march renderer!\n");
        return 1;
    }
#else
    auto pointRenderer = std::make_shared<PointRenderer>();
    if (!pointRenderer->Init(pointCloud))
    {
        Log::printf("Error initializing point renderer!\n");
        return 1;
    }

    auto splatRenderer = std::make_shared<SplatRenderer>();
    if (!splatRenderer->Init(gaussianCloud))
    {
        Log::printf("Error initializing splat renderer!\n");
        return 1;
    }

#ifdef USE_OPENXR
    xrBuddy.SetRenderCallback([&splatRenderer, &magicCarpet](const glm::mat4& projMat, const glm::mat4& eyeMat,
                                                             const glm::vec4& viewport, const glm::vec2& nearFar, int viewNum)
    {
        Clear(false);

        //PrintMat(projMat, "projMat");
        //PrintVec(viewport, "viewport");
        //PrintMat(eyeMat, "eyeMat");
        glm::mat4 fullEyeMat = magicCarpet.GetRoomMat() * eyeMat;

        /*
        if (viewNum == 0)
        {
            splatRenderer->Sort(fullEyeMat);
        }
        splatRenderer->Render(fullEyeMat, projMat, viewport, nearFar);
        */

        DebugDraw_Render(projMat * glm::inverse(fullEyeMat));
    });
#endif


#endif

    uint32_t frameCount = 1;
    uint32_t frameTicks = SDL_GetTicks();
    uint32_t lastTicks = SDL_GetTicks();
    while (!quitting)
    {
        // update dt
        uint32_t ticks = SDL_GetTicks();

#ifdef USE_OPENXR
        if (!xrBuddy.PollEvents())
        {
            Log::printf("xrBuddy PollEvents failed\n");
            return 1;
        }
#endif

        const int FPS_FRAMES = 100;
        if ((frameCount % FPS_FRAMES) == 0)
        {
            float delta = (ticks - frameTicks) / 1000.0f;
            float fps = (float)FPS_FRAMES / delta;
            frameTicks = ticks;
            Log::printf("fps = %.2f\n", fps);
        }
        float dt = (ticks - lastTicks) / 1000.0f;
        lastTicks = ticks;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                quitting = true;
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                // ESC to quit
                if (event.key.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                {
                    quitting = true;
                }
                break;
            case SDL_JOYAXISMOTION:
				JOYSTICK_UpdateMotion(&event.jaxis);
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				JOYSTICK_UpdateButton(&event.jbutton);
				break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    // The window was resized
                    int newWidth = event.window.data1;
                    int newHeight = event.window.data2;
                    // Handle the resize event, e.g., adjust your viewport, etc.
                    Resize(newWidth, newHeight);
                    SDL_RenderSetLogicalSize(renderer, newWidth, newHeight);
                }
                break;
            }
        }

#ifdef USE_OPENXR
        if (!xrBuddy.SyncInput())
        {
            Log::printf("xrBuddy SyncInput failed\n");
            return 1;
        }

        MagicCarpet::Pose headPose, rightPose, leftPose;
        if (!xrBuddy.GetActionPosition("head_pose", &headPose.pos, &headPose.posValid, &headPose.posTracked))
        {
            Log::printf("xrBuddy GetActionPosition(head_pose) failed\n");
        }
        if (!xrBuddy.GetActionOrientation("head_pose", &headPose.rot, &headPose.rotValid, &headPose.rotTracked))
        {
            Log::printf("xrBuddy GetActionOrientation(head_pose) failed\n");
        }
        xrBuddy.GetActionPosition("l_aim_pose", &leftPose.pos, &leftPose.posValid, &leftPose.posTracked);
        xrBuddy.GetActionOrientation("l_aim_pose", &leftPose.rot, &leftPose.rotValid, &leftPose.rotTracked);
        xrBuddy.GetActionPosition("r_aim_pose", &rightPose.pos, &rightPose.posValid, &rightPose.posTracked);
        xrBuddy.GetActionOrientation("r_aim_pose", &rightPose.rot, &rightPose.rotValid, &rightPose.rotTracked);
        glm::vec2 leftStick, rightStick;
        bool valid, changed;
        xrBuddy.GetActionVec2("l_stick", &leftStick, &valid, &changed);
        xrBuddy.GetActionVec2("r_stick", &rightStick, &valid, &changed);
        MagicCarpet::ButtonState buttonState;
        xrBuddy.GetActionBool("l_select_click", &buttonState.leftTrigger, &valid, &changed);
        xrBuddy.GetActionBool("r_select_click", &buttonState.rightTrigger, &valid, &changed);
        xrBuddy.GetActionBool("l_squeeze_click", &buttonState.leftGrip, &valid, &changed);
        xrBuddy.GetActionBool("r_squeeze_click", &buttonState.rightGrip, &valid, &changed);
        magicCarpet.Process(headPose, leftPose, rightPose, leftStick, rightStick, buttonState, dt);
#else
        Joystick* joystick = JOYSTICK_GetJoystick();
        if (joystick)
        {
            float roll = 0.0f;
            roll -= (joystick->buttonStateFlags & (1 << Joystick::LeftBumper)) ? 1.0f : 0.0f;
            roll += (joystick->buttonStateFlags & (1 << Joystick::RightBumper)) ? 1.0f : 0.0f;
            flyCam.Process(glm::vec2(joystick->axes[Joystick::LeftStickX], joystick->axes[Joystick::LeftStickY]),
                           glm::vec2(joystick->axes[Joystick::RightStickX], joystick->axes[Joystick::RightStickY]), roll, dt);
        }
#endif

#ifndef USE_RAY_MARCH_RENDERER
        Clear();
#endif

        // DEBUG: draw some transforms
        glm::mat4 m(glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
                    glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
                    glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
                    glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        DebugDraw_Transform(m, 10.0f);
        m[3][0] = 5.0f;
        DebugDraw_Transform(m, 10.0f);
        m[3][0] = -5.0f;
        DebugDraw_Transform(m, 10.0f);
        m[3][0] = 0.0f;
        m[3][2] = 5.0f;
        DebugDraw_Transform(m, 10.0f);
        m[3][2] = -5.0f;
        DebugDraw_Transform(m, 10.0f);

#ifndef USE_OPENXR
#ifdef USE_RAY_MARCH_RENDERER
        rayMarchRenderer->Render(cameraMat, viewport, nearFar, FOVY, renderer);
#else
        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        glm::mat4 cameraMat = flyCam.GetCameraMat();
        glm::vec4 viewport(0.0f, 0.0f, (float)width, (float)height);
        glm::vec2 nearFar(Z_NEAR, Z_FAR);
        glm::mat4 projMat = glm::perspective(FOVY, (float)width / (float)height, Z_NEAR, Z_FAR);

        //pointRenderer->Render(cameraMat, projmat, viewport, nearFar);
        splatRenderer->Sort(cameraMat);
        splatRenderer->Render(cameraMat, projMat, viewport, nearFar);
#endif
#else
        if (!xrBuddy.RenderFrame())
        {
            Log::printf("xrBuddy RenderFrame failed\n");
            return 1;
        }
#endif

        frameCount++;

#ifndef USE_OPENXR
#ifdef USE_RAY_MARCH_RENDERER
        SDL_RenderPresent(renderer);
#else
        SDL_GL_SwapWindow(window);
#endif
#endif

        // cycle camera mat
        /*
        cameraIndex = (cameraIndex + 1) % cameras->GetCameraVec().size();
        flyCam.SetCameraMat(cameras->GetCameraVec()[cameraIndex]);
        */

        DebugDraw_Clear();

        FrameMark;
    }

    DebugDraw_Shutdown();
    JOYSTICK_Shutdown();
    SDL_DelEventWatch(Watch, NULL);
    SDL_GL_DeleteContext(gl_context);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
