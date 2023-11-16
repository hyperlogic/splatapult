// 3d gaussian splat renderer

#include <algorithm>
#include <cassert>
#include <chrono>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <limits>
#include <memory>
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <thread>
#include <tracy/Tracy.hpp>

#include "core/debugdraw.h"
#include "core/image.h"
#include "core/joystick.h"
#include "core/log.h"
#include "core/program.h"
#include "core/texture.h"
#include "core/util.h"
#include "core/vertexbuffer.h"
#include "core/xrbuddy.h"

#include "cameras.h"
#include "flycam.h"
#include "gaussiancloud.h"
#include "magiccarpet.h"
#include "pointcloud.h"
#include "pointrenderer.h"
#include "radix_sort.hpp"
#include "softwarerenderer.h"
#include "splatrenderer.h"
#include "raymarchrenderer.h"
#include "shaderdebugrenderer.h"

//#define SOFTWARE_SPLATS

struct GlobalContext
{
    bool quitting = false;
    SDL_Window* window = NULL;
    SDL_GLContext gl_context;
    SDL_Renderer* renderer = NULL;
};

GlobalContext ctx;

struct Options
{
    bool vrMode = false;
    bool fullscreen = false;
    bool drawCarpet = true;
    bool drawPointCloud = false;
    bool drawDebug = true;
};

const float Z_NEAR = 0.1f;
const float Z_FAR = 100.0f;
const float FOVY = glm::radians(45.0f);

void Clear(bool setViewport = true)
{
#ifdef SOFTWARE_SPLATS
    SDL_SetRenderDrawColor(ctx.renderer, 0, 0, 0, 0);
    SDL_RenderClear(ctx.renderer);
#else
    SDL_GL_MakeCurrent(ctx.window, ctx.gl_context);

    if (setViewport)
    {
        int width, height;
        SDL_GetWindowSize(ctx.window, &width, &height);
        glViewport(0, 0, width, height);
    }

    // pre-multiplied alpha blending
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glm::vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    // enable alpha test
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 1.0f / 256.0f);
#endif
}

void Resize(int newWidth, int newHeight)
{
#ifndef SOFTWARE_SPLATS
    SDL_GL_MakeCurrent(ctx.window, ctx.gl_context);
    glViewport(0, 0, newWidth, newHeight);
#endif
    SDL_RenderSetLogicalSize(ctx.renderer, newWidth, newHeight);
}

// Draw a textured quad over the entire screen.
void RenderDesktop(std::shared_ptr<Program> desktopProgram, uint32_t colorTexture)
{
    SDL_GL_MakeCurrent(ctx.window, ctx.gl_context);

    int width, height;
    SDL_GetWindowSize(ctx.window, &width, &height);

    glViewport(0, 0, width, height);
    glm::vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glm::mat4 projMat = glm::ortho(0.0f, (float)width, 0.0f, (float)height, -10.0f, 10.0f);

    if (colorTexture > 0)
    {
        desktopProgram->Bind();
        desktopProgram->SetUniform("modelViewProjMat", projMat);
        desktopProgram->SetUniform("color", glm::vec4(1.0f));

        // use texture unit 0 for colorTexture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTexture);
        desktopProgram->SetUniform("colorTexture", 0);

        glm::vec2 xyLowerLeft(0.0f, (height - width) / 2.0f);
        glm::vec2 xyUpperRight((float)width, (height + width) / 2.0f);
        glm::vec2 uvLowerLeft(0.0f, 0.0f);
        glm::vec2 uvUpperRight(1.0f, 1.0f);

        glm::vec3 positions[] = {glm::vec3(xyLowerLeft, 0.0f), glm::vec3(xyUpperRight.x, xyLowerLeft.y, 0.0f),
                                 glm::vec3(xyUpperRight, 0.0f), glm::vec3(xyLowerLeft.x, xyUpperRight.y, 0.0f)};
        desktopProgram->SetAttrib("position", positions);

        glm::vec2 uvs[] = {uvLowerLeft, glm::vec2(uvUpperRight.x, uvLowerLeft.y),
                           uvUpperRight, glm::vec2(uvLowerLeft.x, uvUpperRight.y)};
        desktopProgram->SetAttrib("uv", uvs);

        const size_t NUM_INDICES = 6;
        uint16_t indices[NUM_INDICES] = {0, 1, 2, 0, 2, 3};
        glDrawElements(GL_TRIANGLES, NUM_INDICES, GL_UNSIGNED_SHORT, indices);
    }
}

std::shared_ptr<PointCloud> LoadPointCloud(const std::string& dataDir)
{
    auto pointCloud = std::make_shared<PointCloud>();

    if (!pointCloud->ImportPly(dataDir + "input.ply"))
    {
        Log::printf("Error loading PointCloud!\n");
        return nullptr;
    }
    return pointCloud;
}

std::shared_ptr<GaussianCloud> LoadGaussianCloud(const std::string& dataDir)
{
    auto gaussianCloud = std::make_shared<GaussianCloud>();

    // AJT: TODO: find highest iteration_# dir.
    std::string iterationDir = dataDir + "point_cloud/iteration_30000/";
    if (!gaussianCloud->ImportPly(iterationDir + "point_cloud.ply"))
    {
        Log::printf("Error loading GaussianCloud!\n");
        return nullptr;
    }

    return gaussianCloud;
}

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
    Options opt;

    // parse arguments
    std::string dataDir;
    if (argc < 2)
    {
        Log::printf("Missing FILENAME argument\n");
        // AJT: TODO print usage
        return 1;
    }
    else
    {
        for (int i = 1; i < (argc - 1); i++)
        {
            if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--openxr"))
            {
                opt.vrMode = true;
            }
            else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--fullscreen"))
            {
                opt.fullscreen = true;
            }
        }
        dataDir = argv[argc - 1];

        // make sure data dir ends with /
        if (!(dataDir.back() == '/' || dataDir.back() == '\\'))
        {
            dataDir += "/";
        }
    }

    // verify dataDir exists
    struct stat sb;
    if (stat(dataDir.c_str(), &sb))
    {
        Log::printf("Invalid directory \"%s\"\n", dataDir.c_str());
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK) != 0)
    {
        Log::printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    const int32_t WIDTH = 1024;
    const int32_t HEIGHT = 768;

    // We render in linear space and use opengl to convert linear back into sRGB.
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    uint32_t windowFlags = SDL_WINDOW_OPENGL;
    if (opt.fullscreen)
    {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    else
    {
        windowFlags |= SDL_WINDOW_RESIZABLE;
    }
    ctx.window = SDL_CreateWindow("3dgstoy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, windowFlags);

    ctx.gl_context = SDL_GL_CreateContext(ctx.window);

    SDL_GL_MakeCurrent(ctx.window, ctx.gl_context);
    glEnable(GL_FRAMEBUFFER_SRGB);

#ifdef SOFTWARE_SPLATS
	ctx.renderer = SDL_CreateRenderer(ctx.window, -1, 0);
#else
    ctx.renderer = SDL_CreateRenderer(ctx.window, -1, SDL_RENDERER_ACCELERATED);
#endif
	if (!ctx.renderer)
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

    if (!DebugDraw_Init())
    {
        Log::printf("DebugDraw Init failed\n");
        return 1;
    }

    std::shared_ptr<XrBuddy> xrBuddy;
    if (opt.vrMode)
    {
        xrBuddy = std::make_shared<XrBuddy>(glm::vec2(Z_NEAR, Z_FAR));
        if (!xrBuddy->Init())
        {
            Log::printf("OpenXR Init failed\n");
            return 1;
        }
    }

    JOYSTICK_Init();

    SDL_AddEventWatch(Watch, NULL);

    auto cameras = std::make_shared<Cameras>();
    if (!cameras->ImportJson(dataDir + "cameras.json"))
    {
        Log::printf("Error loading cameras.json\n");
        return 1;
    }

    auto pointCloud = LoadPointCloud(dataDir);
    if (!pointCloud)
    {
        Log::printf("Error loading PointCloud\n");
        return 1;
    }

    auto gaussianCloud = LoadGaussianCloud(dataDir);
    if (!gaussianCloud)
    {
        Log::printf("Error loading GaussianCloud\n");
        return 1;
    }

    const float MOVE_SPEED = 2.5f;
    const float ROT_SPEED = 1.0f;
    FlyCam flyCam(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), MOVE_SPEED, ROT_SPEED);
    int cameraIndex = 0;
    flyCam.SetCameraMat(cameras->GetCameraVec()[cameraIndex]);

    // initialize magicCarpet from first camera and estimated floor position.
    glm::mat4 estimatedFloorMat(1.0f);
    if (cameras->GetNumCameras() > 0)
    {
        glm::vec3 floorNormal, floorPos;
        cameras->EstimateFloorPlane(floorNormal, floorPos);
        glm::vec3 floorZ = cameras->GetCameraVec()[0][2];
        glm::vec3 floorY = floorNormal;
        glm::vec3 floorX = glm::cross(floorY, floorZ);
        floorZ = glm::cross(floorX, floorY);

        estimatedFloorMat = glm::mat4(glm::vec4(floorX, 0.0f),
                                      glm::vec4(floorY, 0.0f),
                                      glm::vec4(floorZ, 0.0f),
                                      glm::vec4(floorPos, 1.0f));
    }

    MagicCarpet magicCarpet(estimatedFloorMat, MOVE_SPEED);
    if (opt.vrMode)
    {
        if (!magicCarpet.Init())
        {
            Log::printf("Error initalizing MagicCarpet\n");
        }
    }

    SDL_JoystickEventState(SDL_ENABLE);

    auto pointRenderer = std::make_shared<PointRenderer>();
    if (!pointRenderer->Init(pointCloud))
    {
        Log::printf("Error initializing point renderer!\n");
        return 1;
    }

#ifdef SOFTWARE_SPLATS
    auto splatRenderer = std::make_shared<ShaderDebugRenderer>(ctx.renderer);
#else
    auto splatRenderer = std::make_shared<SplatRenderer>();
#endif

    if (!splatRenderer->Init(gaussianCloud))
    {
        Log::printf("Error initializing splat renderer!\n");
        return 1;
    }

    auto desktopProgram = std::make_shared<Program>();
    if (opt.vrMode)
    {
        if (!desktopProgram->LoadVertFrag("shader/desktop_vert.glsl", "shader/desktop_frag.glsl"))
        {
            Log::printf("Error loading desktop shader!\n");
            return 1;
        }

        xrBuddy->SetRenderCallback([&pointRenderer, &splatRenderer, &magicCarpet, &opt](
            const glm::mat4& projMat, const glm::mat4& eyeMat,
            const glm::vec4& viewport, const glm::vec2& nearFar, int viewNum)
        {
            Clear(false);

            glm::mat4 fullEyeMat = magicCarpet.GetCarpetMat() * eyeMat;

            if (opt.drawDebug)
            {
                DebugDraw_Render(projMat * glm::inverse(fullEyeMat));
            }

            if (opt.drawCarpet)
            {
                magicCarpet.Render(fullEyeMat, projMat, viewport, nearFar);
            }

            if (opt.drawPointCloud)
            {
                pointRenderer->Render(fullEyeMat, projMat, viewport, nearFar);
            }
            else
            {
                if (viewNum == 0)
                {
                    splatRenderer->Sort(fullEyeMat, projMat, viewport, nearFar);
                }
                splatRenderer->Render(fullEyeMat, projMat, viewport, nearFar);
            }
        });
    }

    glm::vec2 virtualLeftStick(0.0f, 0.0f);
    glm::vec2 virtualRightStick(0.0f, 0.0f);

    uint32_t frameCount = 1;
    uint32_t frameTicks = SDL_GetTicks();
    uint32_t lastTicks = SDL_GetTicks();
    while (!ctx.quitting)
    {
        // update dt
        uint32_t ticks = SDL_GetTicks();

        if (opt.vrMode)
        {
            if (!xrBuddy->PollEvents())
            {
                Log::printf("xrBuddy PollEvents failed\n");
                return 1;
            }
        }

        const int FPS_FRAMES = 100;
        if ((frameCount % FPS_FRAMES) == 0)
        {
            float delta = (ticks - frameTicks) / 1000.0f;
            float fps = (float)FPS_FRAMES / delta;
            frameTicks = ticks;
            //Log::printf("fps = %.2f\n", fps);
        }
        float dt = (ticks - lastTicks) / 1000.0f;
        lastTicks = ticks;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                ctx.quitting = true;
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                // ESC to quit
                if (event.key.type == SDL_KEYDOWN)
                {
                    switch (event.key.keysym.sym)
                    {
                    case SDLK_ESCAPE:
                        ctx.quitting = true;
                        break;
                    case SDLK_c:
                        opt.drawPointCloud = !opt.drawPointCloud;
                        break;
                    case SDLK_n:
                        cameraIndex = (cameraIndex + 1) % cameras->GetNumCameras();
                        flyCam.SetCameraMat(cameras->GetCameraVec()[cameraIndex]);
                        break;
                    case SDLK_p:
                        cameraIndex = (cameraIndex - 1) % cameras->GetNumCameras();
                        flyCam.SetCameraMat(cameras->GetCameraVec()[cameraIndex]);
                        break;
                    case SDLK_f:
                        opt.drawCarpet = !opt.drawCarpet;
                        break;
                    case SDLK_a:
                        virtualLeftStick.x -= 1.0f;
                        break;
                    case SDLK_d:
                        virtualLeftStick.x += 1.0f;
                        break;
                    case SDLK_w:
                        virtualLeftStick.y += 1.0f;
                        break;
                    case SDLK_s:
                        virtualLeftStick.y -= 1.0f;
                        break;
                    case SDLK_LEFT:
                        virtualRightStick.x -= 1.0f;
                        break;
                    case SDLK_RIGHT:
                        virtualRightStick.x += 1.0f;
                        break;
                    case SDLK_UP:
                        virtualRightStick.y += 1.0f;
                        break;
                    case SDLK_DOWN:
                        virtualRightStick.y -= 1.0f;
                        break;
                    }
                }
                else // SDL_KEYUP
                {
                    switch (event.key.keysym.sym)
                    {
                    case SDLK_a:
                        virtualLeftStick.x += 1.0f;
                        break;
                    case SDLK_d:
                        virtualLeftStick.x -= 1.0f;
                        break;
                    case SDLK_w:
                        virtualLeftStick.y -= 1.0f;
                        break;
                    case SDLK_s:
                        virtualLeftStick.y += 1.0f;
                        break;
                    case SDLK_LEFT:
                        virtualRightStick.x += 1.0f;
                        break;
                    case SDLK_RIGHT:
                        virtualRightStick.x -= 1.0f;
                        break;
                    case SDLK_UP:
                        virtualRightStick.y -= 1.0f;
                        break;
                    case SDLK_DOWN:
                        virtualRightStick.y += 1.0f;
                        break;
                    }
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
                }
                break;
            }
        }

        if (opt.vrMode)
        {
            if (!xrBuddy->SyncInput())
            {
                Log::printf("xrBuddy SyncInput failed\n");
                return 1;
            }

            // copy vr input into MagicCarpet
            MagicCarpet::Pose headPose, rightPose, leftPose;
            if (!xrBuddy->GetActionPosition("head_pose", &headPose.pos, &headPose.posValid, &headPose.posTracked))
            {
                Log::printf("xrBuddy GetActionPosition(head_pose) failed\n");
            }
            if (!xrBuddy->GetActionOrientation("head_pose", &headPose.rot, &headPose.rotValid, &headPose.rotTracked))
            {
                Log::printf("xrBuddy GetActionOrientation(head_pose) failed\n");
            }
            xrBuddy->GetActionPosition("l_aim_pose", &leftPose.pos, &leftPose.posValid, &leftPose.posTracked);
            xrBuddy->GetActionOrientation("l_aim_pose", &leftPose.rot, &leftPose.rotValid, &leftPose.rotTracked);
            xrBuddy->GetActionPosition("r_aim_pose", &rightPose.pos, &rightPose.posValid, &rightPose.posTracked);
            xrBuddy->GetActionOrientation("r_aim_pose", &rightPose.rot, &rightPose.rotValid, &rightPose.rotTracked);
            glm::vec2 leftStick, rightStick;
            bool valid, changed;
            xrBuddy->GetActionVec2("l_stick", &leftStick, &valid, &changed);
            xrBuddy->GetActionVec2("r_stick", &rightStick, &valid, &changed);
            MagicCarpet::ButtonState buttonState;
            xrBuddy->GetActionBool("l_select_click", &buttonState.leftTrigger, &valid, &changed);
            xrBuddy->GetActionBool("r_select_click", &buttonState.rightTrigger, &valid, &changed);
            xrBuddy->GetActionBool("l_squeeze_click", &buttonState.leftGrip, &valid, &changed);
            xrBuddy->GetActionBool("r_squeeze_click", &buttonState.rightGrip, &valid, &changed);
            magicCarpet.Process(headPose, leftPose, rightPose, leftStick, rightStick, buttonState, dt);
        }

        Joystick* joystick = JOYSTICK_GetJoystick();
        if (joystick)
        {
            float roll = 0.0f;
            roll -= (joystick->buttonStateFlags & (1 << Joystick::LeftBumper)) ? 1.0f : 0.0f;
            roll += (joystick->buttonStateFlags & (1 << Joystick::RightBumper)) ? 1.0f : 0.0f;
            glm::vec2 leftStick(joystick->axes[Joystick::LeftStickX], joystick->axes[Joystick::LeftStickY]);
            glm::vec2 rightStick(joystick->axes[Joystick::RightStickX], joystick->axes[Joystick::RightStickY]);
            flyCam.Process(leftStick + virtualLeftStick,
                           rightStick + virtualRightStick, roll, dt);
        }

        // Debug draw the estimated floor
        DebugDraw_Transform(estimatedFloorMat, 1.0f);
        // Draw the origin
        DebugDraw_Transform(glm::mat4(1.0f), 1.0f);

        if (opt.vrMode)
        {
            if (xrBuddy->SessionReady())
            {
                if (!xrBuddy->RenderFrame())
                {
                    Log::printf("xrBuddy RenderFrame failed\n");
                    return 1;
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // render desktop.
            Clear(true);
            RenderDesktop(desktopProgram, xrBuddy->GetColorTexture());
            SDL_GL_SwapWindow(ctx.window);
        }
        else
        {
            Clear(true);

            int width, height;
            SDL_GetWindowSize(ctx.window, &width, &height);
            glm::mat4 cameraMat = flyCam.GetCameraMat();
            glm::vec4 viewport(0.0f, 0.0f, (float)width, (float)height);
            glm::vec2 nearFar(Z_NEAR, Z_FAR);
            glm::mat4 projMat = glm::perspective(FOVY, (float)width / (float)height, Z_NEAR, Z_FAR);

            if (opt.drawDebug)
            {
                DebugDraw_Render(projMat * glm::inverse(cameraMat));
            }

            if (opt.drawPointCloud)
            {
                pointRenderer->Render(cameraMat, projMat, viewport, nearFar);
            }
            else
            {
                splatRenderer->Sort(cameraMat, projMat, viewport, nearFar);
                splatRenderer->Render(cameraMat, projMat, viewport, nearFar);
            }

#ifdef SOFTWARE_SPLATS
            SDL_RenderPresent(ctx.renderer);
#else
            SDL_GL_SwapWindow(ctx.window);
#endif
        }

        DebugDraw_Clear();

        frameCount++;

        FrameMark;
    }

    DebugDraw_Shutdown();
    JOYSTICK_Shutdown();
    SDL_DelEventWatch(Watch, NULL);
    SDL_GL_DeleteContext(ctx.gl_context);

    SDL_DestroyWindow(ctx.window);
    SDL_Quit();

    return 0;
}
