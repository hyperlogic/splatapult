/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "app.h"

#ifndef __ANDROID__
#define USE_SDL
#include <GL/glew.h>
#endif

#ifdef USE_SDL
#include <SDL2/SDL.h>
#endif

#include <filesystem>
#include <thread>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedNC(NAME, COLOR)
#endif

#include "core/framebuffer.h"
#include "core/log.h"
#include "core/debugrenderer.h"
#include "core/inputbuddy.h"
#include "core/optionparser.h"
#include "core/textrenderer.h"
#include "core/texture.h"
#include "core/util.h"
#include "core/xrbuddy.h"

#include "camerasconfig.h"
#include "camerapathrenderer.h"
#include "flycam.h"
#include "gaussiancloud.h"
#include "magiccarpet.h"
#include "pointcloud.h"
#include "pointrenderer.h"
#include "splatrenderer.h"
#include "vrconfig.h"

enum optionIndex
{
    UNKNOWN,
    OPENXR,
    FULLSCREEN,
    DEBUG,
    HELP,
    FP16,
    FP32,
    NOSH,
};

const option::Descriptor usage[] =
{
    { UNKNOWN, 0, "", "", option::Arg::None, "USAGE: splatapult [options] FILE.ply\n\nOptions:" },
    { HELP, 0, "h", "help", option::Arg::None,            "  -h, --help        Print usage and exit." },
    { OPENXR, 0, "v", "openxr", option::Arg::None,        "  -v, --openxr      Launch app in vr mode, using openxr runtime." },
    { FULLSCREEN, 0, "f", "fullscren", option::Arg::None, "  -f, --fullscreen  Launch window in fullscreen." },
    { DEBUG, 0, "d", "debug", option::Arg::None,          "  -d, --debug       Enable verbose debug logging." },
    { FP16, 0, "", "fp16", option::Arg::None,             "  --fp16            Use 16-bit half-precision floating frame buffer, to reduce color banding artifacts" },
    { FP32, 0, "", "fp32", option::Arg::None,             "  --fp32            Use 32-bit floating point frame buffer, to reduce color banding even more" },
    { NOSH, 0, "", "nosh", option::Arg::None,             "  --nosh            Don't load/render full sh, this will reduce memory usage and higher performance" },
    { UNKNOWN, 0, "", "", option::Arg::None,              "\nExamples:\n  splataplut data/test.ply\n  splatapult -v data/test.ply" },
    { 0, 0, 0, 0, 0, 0}
};

const float Z_NEAR = 0.1f;
const float Z_FAR = 1000.0f;
const float FOVY = glm::radians(45.0f);

const float MOVE_SPEED = 2.5f;
const float ROT_SPEED = 1.15f;

const glm::vec4 WHITE = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
const glm::vec4 BLACK = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
const int TEXT_NUM_ROWS = 25;

#include <string>
#include <filesystem>
#include <iostream>

// searches for file named configFilename, dir that contains plyFilename, it's parent and grandparent dirs.
static std::string FindConfigFile(const std::string& plyFilename, const std::string& configFilename)
{
    std::filesystem::path plyPath(plyFilename);

    if (!std::filesystem::exists(plyPath) || !std::filesystem::is_regular_file(plyPath))
    {
        Log::E("PLY file does not exist or is not a file: \"%s\"", plyFilename.c_str());
        return "";
    }

    std::filesystem::path directory = plyPath.parent_path();

    for (int i = 0; i < 3; ++i) // Check current, parent, and grandparent directories
    {
        std::filesystem::path configPath = directory / configFilename;
        if (std::filesystem::exists(configPath) && std::filesystem::is_regular_file(configPath))
        {
            return configPath.string();
        }
        if (directory.has_parent_path())
        {
            directory = directory.parent_path();
        }
        else
        {
            break;
        }
    }

    return "";
}

static std::string GetFilenameWithoutExtension(const std::string& filepath)
{
    std::filesystem::path pathObj(filepath);

    // Check if the path has a stem (the part of the path before the extension)
    if (pathObj.has_stem())
    {
        return pathObj.stem().string();
    }

    // If there is no stem, return an empty string
    return "";
}

static std::string MakeVrConfigFilename(const std::string& plyFilename)
{
    std::filesystem::path plyPath(plyFilename);
    std::filesystem::path directory = plyPath.parent_path();
    std::string plyNoExt = GetFilenameWithoutExtension(plyFilename) + "_vr.json";
    std::filesystem::path configPath = directory / plyNoExt;
    return configPath.string();
}

static void Clear(glm::ivec2 windowSize, bool setViewport = true)
{
    int width = windowSize.x;
    int height = windowSize.y;
    if (setViewport)
    {
        glViewport(0, 0, width, height);
    }

    // pre-multiplied alpha blending
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glm::vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // NOTE: if depth buffer has less then 24 bits, it can mess up splat rendering.
    glEnable(GL_DEPTH_TEST);
}

// Draw a textured quad over the entire screen.
static void RenderDesktop(glm::ivec2 windowSize, std::shared_ptr<Program> desktopProgram, uint32_t colorTexture, bool adjustAspect)
{
    int width = windowSize.x;
    int height = windowSize.y;

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

        glm::vec2 xyLowerLeft(0.0f, 0.0f);
        glm::vec2 xyUpperRight((float)width, (float)height);
        if (adjustAspect)
        {
            xyLowerLeft = glm::vec2(0.0f, (height - width) / 2.0f);
            xyUpperRight = glm::vec2((float)width, (height + width) / 2.0f);
        }
        glm::vec2 uvLowerLeft(0.0f, 0.0f);
        glm::vec2 uvUpperRight(1.0f, 1.0f);

        float depth = -9.0f;
        glm::vec3 positions[] = {glm::vec3(xyLowerLeft, depth), glm::vec3(xyUpperRight.x, xyLowerLeft.y, depth),
                                 glm::vec3(xyUpperRight, depth), glm::vec3(xyLowerLeft.x, xyUpperRight.y, depth)};
        desktopProgram->SetAttrib("position", positions);

        glm::vec2 uvs[] = {uvLowerLeft, glm::vec2(uvUpperRight.x, uvLowerLeft.y),
                           uvUpperRight, glm::vec2(uvLowerLeft.x, uvUpperRight.y)};
        desktopProgram->SetAttrib("uv", uvs);

        const size_t NUM_INDICES = 6;
        uint16_t indices[NUM_INDICES] = {0, 1, 2, 0, 2, 3};
        glDrawElements(GL_TRIANGLES, NUM_INDICES, GL_UNSIGNED_SHORT, indices);
    }
}

static std::shared_ptr<PointCloud> LoadPointCloud(const std::string& plyFilename, bool useLinearColors)
{
    auto pointCloud = std::make_shared<PointCloud>(useLinearColors);

    if (!pointCloud->ImportPly(plyFilename))
    {
        Log::E("Error loading PointCloud!\n");
        return nullptr;
    }
    return pointCloud;
}

static std::shared_ptr<GaussianCloud> LoadGaussianCloud(const std::string& plyFilename, const App::Options& opt)
{
    GaussianCloud::Options options = {0};
#ifdef __ANDROID__
    options.importFullSH = false;
    options.exportFullSH = false;
#else
    options.importFullSH = opt.importFullSH;
    options.exportFullSH = true;
#endif
    auto gaussianCloud = std::make_shared<GaussianCloud>(options);
    if (!gaussianCloud->ImportPly(plyFilename))
    {
        Log::E("Error loading GaussianCloud!\n");
        return nullptr;
    }

    return gaussianCloud;
}

static void PrintControls()
{
    fprintf(stdout, "\
\n\
Desktop Controls\n\
--------------------\n\
* wasd - move\n\
* arrow keys - look\n\
* right mouse button - hold down for mouse look.\n\
* gamepad - if present, right stick to rotate, left stick to move, bumpers to roll\n\
* c - toggle between initial SfM point cloud (if present) and gaussian splats.\n\
* n - jump to next camera\n\
* p - jump to previous camera\n\
\n\
VR Controls\n\
---------------\n\
* c - toggle between initial SfM point cloud (if present) and gaussian splats.\n\
* left stick - move\n\
* right stick - snap turn\n\
* f - show hide floor carpet.\n\
* single grab - translate the world.\n\
* double grab - rotate and translate the world.\n\
* triple grab - (double grab while trigger is depressed) scale, rotate and translate the world.\n\
* return - save the current position and orientation/scale of the world into a vr.json file.\n\
\n");
}

App::App(MainContext& mainContextIn):
    mainContext(mainContextIn)
{
    cameraIndex = 0;
    virtualLeftStick = glm::vec2(0.0f, 0.0f);
    virtualRightStick = glm::vec2(0.0f, 0.0f);
    mouseLookStick = glm::vec2(0.0f, 0.0f);
    mouseLook = false;
    virtualRoll = 0.0f;
    virtualUp = 0.0f;
    frameNum = 0;
}

App::ParseResult App::ParseArguments(int argc, const char* argv[])
{
    // skip program name
    if (argc > 0)
    {
        argc--;
        argv++;
    }
    option::Stats stats(usage, argc, argv);
    std::vector<option::Option> options(stats.options_max);
    std::vector<option::Option> buffer(stats.buffer_max);
    option::Parser parse(usage, argc, argv, options.data(), buffer.data());

    if (parse.error())
    {
        return ERROR_RESULT;
    }

    if (options[HELP] || argc == 0)
    {
        option::printUsage(std::cout, usage);
        PrintControls();
        return QUIT_RESULT;
    }

    if (options[OPENXR])
    {
        opt.vrMode = true;
    }

    if (options[FULLSCREEN])
    {
        opt.fullscreen = true;
    }

    if (options[DEBUG])
    {
        opt.debugLogging = true;
    }

    if (options[FP32])
    {
        opt.frameBuffer = Options::FrameBuffer::Float;
    }
    else if (options[FP16])
    {
        opt.frameBuffer = Options::FrameBuffer::HalfFloat;
    }

    opt.importFullSH = options[NOSH] ? false : true;

    bool unknownOptionFound = false;
    for (option::Option* opt = options[UNKNOWN]; opt; opt = opt->next())
    {
        unknownOptionFound = true;
        std::cout << "Unknown option: " << std::string(opt->name,opt->namelen) << "\n";
    }
    if (unknownOptionFound)
    {
        return ERROR_RESULT;
    }

    if (parse.nonOptionsCount() == 0)
    {
        std::cout << "Expected filename argument\n";
        return ERROR_RESULT;
    }
    else
    {
        plyFilename = parse.nonOption(0);
    }

    Log::SetLevel(opt.debugLogging ? Log::Debug : Log::Warning);

    std::filesystem::path plyPath(plyFilename);
    if (!std::filesystem::exists(plyPath) || !std::filesystem::is_regular_file(plyPath))
    {
        Log::E("Invalid file \"%s\"\n", plyFilename.c_str());
        return ERROR_RESULT;
    }

    return SUCCESS_RESULT;
}

bool App::Init()
{
    bool isFramebufferSRGBEnabled = opt.vrMode;

#ifndef __ANDROID__
    // AJT: ANDROID: TODO: make sure colors are accurate on android.
    if (isFramebufferSRGBEnabled)
    {
        // necessary for proper color conversion
        glEnable(GL_FRAMEBUFFER_SRGB);
    }
    else
    {
        glDisable(GL_FRAMEBUFFER_SRGB);
    }

    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        Log::E("Error: %s\n", glewGetErrorString(err));
        return false;
    }
#endif

    debugRenderer = std::make_shared<DebugRenderer>();
    if (!debugRenderer->Init())
    {
        Log::E("DebugRenderer Init failed\n");
        return false;
    }

    textRenderer = std::make_shared<TextRenderer>();
    if (!textRenderer->Init("font/JetBrainsMono-Medium.json", "font/JetBrainsMono-Medium.png"))
    {
        Log::E("TextRenderer Init failed\n");
        return false;
    }

    if (opt.vrMode)
    {
        xrBuddy = std::make_shared<XrBuddy>(mainContext, glm::vec2(Z_NEAR, Z_FAR));
        if (!xrBuddy->Init())
        {
            Log::E("OpenXR Init failed\n");
            return false;
        }
    }

    std::string camerasConfigFilename = FindConfigFile(plyFilename, "cameras.json");
    if (!camerasConfigFilename.empty())
    {
        camerasConfig = std::make_shared<CamerasConfig>();
        if (!camerasConfig->ImportJson(camerasConfigFilename))
        {
            Log::W("Error loading cameras.json\n");
            camerasConfig.reset();
        }
    }
    else
    {
        Log::D("Could not find cameras.json\n");
    }

    if (camerasConfig)
    {
        cameraPathRenderer = std::make_shared<CameraPathRenderer>();
        if (!cameraPathRenderer->Init(camerasConfig->GetCameraVec()))
        {
            Log::E("CameraPathRenderer Init failed\n");
            return false;
        }
    }

    // search for vr config file
    // for example: if plyFilename is "input.ply", then search for "input_vr.json"
    std::string vrConfigBaseFilename = GetFilenameWithoutExtension(plyFilename) + "_vr.json";
    std::string vrConfigFilename = FindConfigFile(plyFilename, vrConfigBaseFilename);
    if (!vrConfigFilename.empty())
    {
        vrConfig = std::make_shared<VrConfig>();
        if (!vrConfig->ImportJson(vrConfigFilename))
        {
            Log::I("Could not load vr.json\n");
            vrConfig.reset();
        }
    }
    else
    {
        Log::D("Could not find %s\n", vrConfigFilename.c_str());
        // Where we'd like the vr config file to exist.
        vrConfigFilename = MakeVrConfigFilename(plyFilename);
    }

    glm::mat4 flyCamMat(1.0f);
    glm::mat4 floorMat(1.0f);
    if (camerasConfig)
    {
        flyCamMat = camerasConfig->GetCameraVec()[cameraIndex].mat;

        // initialize magicCarpet from first camera and estimated floor position.
        if (camerasConfig->GetNumCameras() > 0)
        {
            glm::vec3 floorNormal, floorPos;
            camerasConfig->EstimateFloorPlane(floorNormal, floorPos);
            glm::vec3 floorZ = camerasConfig->GetCameraVec()[0].mat[2];
            glm::vec3 floorY = floorNormal;
            glm::vec3 floorX = glm::cross(floorY, floorZ);
            floorZ = glm::cross(floorX, floorY);

            floorMat = glm::mat4(glm::vec4(floorX, 0.0f),
                                 glm::vec4(floorY, 0.0f),
                                 glm::vec4(floorZ, 0.0f),
                                 glm::vec4(floorPos, 1.0f));
        }
    }

    if (vrConfig)
    {
        floorMat = vrConfig->GetFloorMat();

        if (!camerasConfig)
        {
            glm::vec3 pos = floorMat[3];
            pos += glm::mat3(floorMat) * glm::vec3(0.0f, 1.5f, 0.0f);
            glm::mat4 adjustedFloorMat = floorMat;
            adjustedFloorMat[3] = glm::vec4(pos, 1.0f);
            flyCamMat = adjustedFloorMat;
        }
    }

    glm::vec3 flyCamPos, flyCamScale, floorMatUp;
    glm::quat flyCamRot;
    floorMatUp = glm::vec3(floorMat[1]);
    Decompose(flyCamMat, &flyCamScale, &flyCamRot, &flyCamPos);
    flyCam = std::make_shared<FlyCam>(floorMatUp, flyCamPos, flyCamRot, MOVE_SPEED, ROT_SPEED);

    magicCarpet = std::make_shared<MagicCarpet>(floorMat, MOVE_SPEED);
    if (!magicCarpet->Init(isFramebufferSRGBEnabled))
    {
        Log::E("Error initalizing MagicCarpet\n");
        return false;
    }

    std::string pointCloudFilename = FindConfigFile(plyFilename, "input.ply");
    if (!pointCloudFilename.empty())
    {
        pointCloud = LoadPointCloud(pointCloudFilename, isFramebufferSRGBEnabled);
        if (!pointCloud)
        {
            Log::E("Error loading PointCloud\n");
            return false;
        }

        pointRenderer = std::make_shared<PointRenderer>();
        if (!pointRenderer->Init(pointCloud, isFramebufferSRGBEnabled))
        {
            Log::E("Error initializing point renderer!\n");
            return false;
        }
    }
    else
    {
        Log::D("Could not find input.ply\n");
    }

    gaussianCloud = LoadGaussianCloud(plyFilename, opt);
    if (!gaussianCloud)
    {
        Log::E("Error loading GaussianCloud\n");
        return false;
    }

#if 0
    const uint32_t SPLAT_COUNT = 25000;
    glm::vec3 focalPoint = flyCam->GetCameraMat()[3];
    //gaussianCloud->PruneSplats(glm::vec3(flyCam->GetCameraMat()[3]), SPLAT_COUNT);
    gaussianCloud->PruneSplats(focalPoint, SPLAT_COUNT);
#endif

    splatRenderer = std::make_shared<SplatRenderer>();
#if __ANDROID__
    bool useRgcSortOverride = true;
#else
    bool useRgcSortOverride = false;
#endif
    if (!splatRenderer->Init(gaussianCloud, isFramebufferSRGBEnabled, useRgcSortOverride))
    {
        Log::E("Error initializing splat renderer!\n");
        return false;
    }

    if (opt.vrMode)
    {
        desktopProgram = std::make_shared<Program>();
        std::string defines = "#define USE_SUPERSAMPLING\n";
        desktopProgram->AddMacro("DEFINES", defines);
        if (!desktopProgram->LoadVertFrag("shader/desktop_vert.glsl", "shader/desktop_frag.glsl"))
        {
            Log::E("Error loading desktop shader!\n");
            return 1;
        }

        xrBuddy->SetRenderCallback([this](
            const glm::mat4& projMat, const glm::mat4& eyeMat,
            const glm::vec4& viewport, const glm::vec2& nearFar, int viewNum)
        {
            Clear(glm::ivec2(0, 0), false);

            glm::mat4 fullEyeMat = magicCarpet->GetCarpetMat() * eyeMat;

            if (opt.drawDebug)
            {
                debugRenderer->Render(fullEyeMat, projMat, viewport, nearFar);
            }

            if (cameraPathRenderer)
            {
                cameraPathRenderer->SetShowCameras(opt.drawCameraFrustums);
                cameraPathRenderer->SetShowPath(opt.drawCameraPath);
                cameraPathRenderer->Render(fullEyeMat, projMat, viewport, nearFar);
            }

            if (opt.drawCarpet)
            {
                magicCarpet->Render(fullEyeMat, projMat, viewport, nearFar);
            }

            if (opt.drawPointCloud && pointRenderer)
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

    if (!opt.vrMode && opt.frameBuffer != Options::FrameBuffer::Default)
    {
        desktopProgram = std::make_shared<Program>();
        if (!desktopProgram->LoadVertFrag("shader/desktop_vert.glsl", "shader/desktop_frag.glsl"))
        {
            Log::E("Error loading desktop shader!\n");
            return 1;
        }
    }

#ifdef USE_SDL
    inputBuddy = std::make_shared<InputBuddy>();

    inputBuddy->OnQuit([this]()
    {
        // forward this back to main
        quitCallback();
    });

    inputBuddy->OnResize([this](int newWidth, int newHeight)
    {
        glViewport(0, 0, newWidth, newHeight);
        resizeCallback(newWidth, newHeight);
    });

    inputBuddy->OnKey(SDLK_ESCAPE, [this](bool down, uint16_t mod)
    {
        quitCallback();
    });

    inputBuddy->OnKey(SDLK_c, [this](bool down, uint16_t mod)
    {
        if (down)
        {
            opt.drawPointCloud = !opt.drawPointCloud;
        }
    });

    inputBuddy->OnKey(SDLK_n, [this](bool down, uint16_t mod)
    {
        if (down && camerasConfig)
        {
            cameraIndex++;
            if (cameraIndex >= (int)camerasConfig->GetNumCameras())
            {
                cameraIndex -= (int)camerasConfig->GetNumCameras();
            }
            flyCam->SetCameraMat(camerasConfig->GetCameraVec()[cameraIndex].mat);
        }
    });

    inputBuddy->OnKey(SDLK_p, [this](bool down, uint16_t mod)
    {
        if (down && camerasConfig)
        {
            cameraIndex--;
            if (cameraIndex < 0)
            {
                cameraIndex += (int)camerasConfig->GetNumCameras();
            }
            flyCam->SetCameraMat(camerasConfig->GetCameraVec()[cameraIndex].mat);
        }
    });

    inputBuddy->OnKey(SDLK_f, [this](bool down, uint16_t mod)
    {
        if (down)
        {
            opt.drawCarpet = !opt.drawCarpet;
        }
    });

    inputBuddy->OnKey(SDLK_y, [this](bool down, uint16_t mod)
    {
        if (down)
        {
            opt.drawCameraFrustums = !opt.drawCameraFrustums;
        }
    });

    inputBuddy->OnKey(SDLK_h, [this](bool down, uint16_t mod)
    {
        if (down)
        {
            opt.drawCameraPath = !opt.drawCameraPath;
        }
    });

    inputBuddy->OnKey(SDLK_RETURN, [this, vrConfigFilename](bool down, uint16_t mod)
    {
        if (down)
        {
            if (!vrConfig)
            {
                vrConfig = std::make_shared<VrConfig>();
            }

            if (opt.vrMode)
            {
                vrConfig->SetFloorMat(magicCarpet->GetCarpetMat());
            }
            else
            {
                glm::mat4 headMat = flyCam->GetCameraMat();
                glm::vec3 pos = headMat[3];
                pos -= glm::mat3(headMat) * glm::vec3(0.0f, 1.5f, 0.0f);
                glm::mat4 floorMat = headMat;
                floorMat[3] = glm::vec4(pos, 1.0f);
                vrConfig->SetFloorMat(floorMat);
            }

            if (vrConfig->ExportJson(vrConfigFilename))
            {
                Log::I("Wrote \"%s\"\n", vrConfigFilename.c_str());
            }
            else
            {
                Log::E("Writing \"%s\" failed\n", vrConfigFilename.c_str());
            }
        }
    });

    inputBuddy->OnKey(SDLK_F1, [this](bool down, uint16_t mod)
    {
        if (down)
        {
            opt.drawFps = !opt.drawFps;
        }
    });

    inputBuddy->OnKey(SDLK_a, [this](bool down, uint16_t mod)
    {
        virtualLeftStick.x += down ? -1.0f : 1.0f;
    });

    inputBuddy->OnKey(SDLK_d, [this](bool down, uint16_t mod)
    {
        virtualLeftStick.x += down ? 1.0f : -1.0f;
    });

    inputBuddy->OnKey(SDLK_w, [this](bool down, uint16_t mod)
    {
        virtualLeftStick.y += down ? 1.0f : -1.0f;
    });

    inputBuddy->OnKey(SDLK_s, [this](bool down, uint16_t mod)
    {
        virtualLeftStick.y += down ? -1.0f : 1.0f;
    });

    inputBuddy->OnKey(SDLK_LEFT, [this](bool down, uint16_t mod)
    {
        virtualRightStick.x += down ? -1.0f : 1.0f;
    });

    inputBuddy->OnKey(SDLK_RIGHT, [this](bool down, uint16_t mod)
    {
        virtualRightStick.x += down ? 1.0f : -1.0f;
    });

    inputBuddy->OnKey(SDLK_UP, [this](bool down, uint16_t mod)
    {
        virtualRightStick.y += down ? 1.0f : -1.0f;
    });

    inputBuddy->OnKey(SDLK_DOWN, [this](bool down, uint16_t mod)
    {
        virtualRightStick.y += down ? -1.0f : 1.0f;
    });

    inputBuddy->OnKey(SDLK_q, [this](bool down, uint16_t mod)
    {
        virtualRoll += down ? -1.0f : 1.0f;
    });

    inputBuddy->OnKey(SDLK_e, [this](bool down, uint16_t mod)
    {
        virtualRoll += down ? 1.0f : -1.0f;
    });

    inputBuddy->OnKey(SDLK_t, [this](bool down, uint16_t mod)
    {
        virtualUp += down ? 1.0f : -1.0f;
    });

    inputBuddy->OnKey(SDLK_g, [this](bool down, uint16_t mod)
    {
        virtualUp += down ? -1.0f : 1.0f;
    });

    inputBuddy->OnMouseButton([this](uint8_t button, bool down, glm::ivec2 pos)
    {
        if (button == 3) // right button
        {
            if (mouseLook != down)
            {
                inputBuddy->SetRelativeMouseMode(down);
            }
            mouseLook = down;
        }
    });

    inputBuddy->OnMouseMotion([this](glm::ivec2 pos, glm::ivec2 rel)
    {
        if (mouseLook)
        {
            const float MOUSE_SENSITIVITY = 0.001f;
            mouseLookStick.x += rel.x * MOUSE_SENSITIVITY;
            mouseLookStick.y -= rel.y * MOUSE_SENSITIVITY;
        }
    });
#endif // USE_SDL

    fpsText = textRenderer->AddScreenTextWithDropShadow(glm::ivec2(0, 0), (int)TEXT_NUM_ROWS, WHITE, BLACK, "fps:");

    return true;
}

void App::ProcessEvent(const SDL_Event& event)
{
#ifdef USE_SDL
    inputBuddy->ProcessEvent(event);
#endif
}

void App::UpdateFps(float fps)
{
    std::string text = "fps: " + std::to_string((int)fps);
    textRenderer->RemoveText(fpsText);
    fpsText = textRenderer->AddScreenTextWithDropShadow(glm::ivec2(0, 0), TEXT_NUM_ROWS, WHITE, BLACK, text);

//#define FIND_BEST_NUM_BLOCKS_PER_WORKGROUP
#ifdef FIND_BEST_NUM_BLOCKS_PER_WORKGROUP
    Log::E("%s\n", text.c_str());
    fpsVec.push_back(fps);
    const uint32_t STEP_SIZE = 64;
    if (fpsVec.size() == 2)
    {
        float dFps = fpsVec[1] - fpsVec[0];

        uint32_t x = splatRenderer->numBlocksPerWorkgroup - STEP_SIZE;
        Log::E("    (%.3f -> %.3f) dFps = %.3f, x = %u\n", fpsVec[0], fpsVec[1], dFps, x);
        if (dFps < 0)
        {
            Log::E("    FPS DOWN, new x = %u\n", x - STEP_SIZE);
            if (splatRenderer->numBlocksPerWorkgroup > STEP_SIZE)
            {
                splatRenderer->numBlocksPerWorkgroup = x - STEP_SIZE;
            }
        }
        else
        {
            Log::E("    FPS UP, new x = %u\n", x + STEP_SIZE);
            splatRenderer->numBlocksPerWorkgroup = x + STEP_SIZE;
        }

        fpsVec.clear();
    }
    else
    {
        splatRenderer->numBlocksPerWorkgroup = splatRenderer->numBlocksPerWorkgroup + STEP_SIZE;
    }
#endif
}

bool App::Process(float dt)
{
    if (opt.vrMode)
    {
        if (!xrBuddy->PollEvents())
        {
            Log::E("xrBuddy PollEvents failed\n");
            return false;
        }

        if (!xrBuddy->SyncInput())
        {
            Log::E("xrBuddy SyncInput failed\n");
            return false;
        }

        // copy vr input into MagicCarpet
        MagicCarpet::Pose headPose, rightPose, leftPose;
        if (!xrBuddy->GetActionPosition("head_pose", &headPose.pos, &headPose.posValid, &headPose.posTracked))
        {
            Log::W("xrBuddy GetActionPosition(head_pose) failed\n");
        }
        if (!xrBuddy->GetActionOrientation("head_pose", &headPose.rot, &headPose.rotValid, &headPose.rotTracked))
        {
            Log::W("xrBuddy GetActionOrientation(head_pose) failed\n");
        }
        xrBuddy->GetActionPosition("l_aim_pose", &leftPose.pos, &leftPose.posValid, &leftPose.posTracked);
        xrBuddy->GetActionOrientation("l_aim_pose", &leftPose.rot, &leftPose.rotValid, &leftPose.rotTracked);
        xrBuddy->GetActionPosition("r_aim_pose", &rightPose.pos, &rightPose.posValid, &rightPose.posTracked);
        xrBuddy->GetActionOrientation("r_aim_pose", &rightPose.rot, &rightPose.rotValid, &rightPose.rotTracked);

        glm::vec2 leftStick(0.0f, 0.0f);
        glm::vec2 rightStick(0.0f, 0.0f);
        bool valid = false;
        bool changed = false;
        xrBuddy->GetActionVec2("l_stick", &leftStick, &valid, &changed);
        xrBuddy->GetActionVec2("r_stick", &rightStick, &valid, &changed);

        // Convert trackpad into a "stick", for HTC Vive controllers
        glm::vec2 leftTrackpadStick(0.0f, 0.0f);
        bool leftTrackpadClick = false;
        xrBuddy->GetActionBool("l_trackpad_click", &leftTrackpadClick, &valid, &changed);
        if (leftTrackpadClick && valid)
        {
            xrBuddy->GetActionFloat("l_trackpad_x", &leftTrackpadStick.x, &valid, &changed);
            xrBuddy->GetActionFloat("l_trackpad_y", &leftTrackpadStick.y, &valid, &changed);
        }
        else
        {
            leftTrackpadStick = glm::vec2(0.0f, 0.0f);
        }

        glm::vec2 rightTrackpadStick(0.0f, 0.0f);
        bool rightTrackpadClick = false;
        xrBuddy->GetActionBool("r_trackpad_click", &rightTrackpadClick, &valid, &changed);
        if (rightTrackpadClick && valid)
        {
            xrBuddy->GetActionFloat("r_trackpad_x", &rightTrackpadStick.x, &valid, &changed);
            xrBuddy->GetActionFloat("r_trackpad_y", &rightTrackpadStick.y, &valid, &changed);
        }
        else
        {
            rightTrackpadStick = glm::vec2(0.0f, 0.0f);
        }

        MagicCarpet::ButtonState buttonState;
        xrBuddy->GetActionBool("l_select_click", &buttonState.leftTrigger, &valid, &changed);
        xrBuddy->GetActionBool("r_select_click", &buttonState.rightTrigger, &valid, &changed);
        xrBuddy->GetActionBool("l_squeeze_click", &buttonState.leftGrip, &valid, &changed);
        xrBuddy->GetActionBool("r_squeeze_click", &buttonState.rightGrip, &valid, &changed);
        magicCarpet->Process(headPose, leftPose, rightPose, leftStick + leftTrackpadStick,
                             rightStick + rightTrackpadStick, buttonState, dt);
    }
#ifdef USE_SDL

    InputBuddy::Joypad joypad = inputBuddy->GetJoypad();
    float roll = 0.0f;
    roll -= joypad.lb ? 1.0f : 0.0f;
    roll += joypad.rb ? 1.0f : 0.0f;
    flyCam->Process(glm::clamp(joypad.leftStick + virtualLeftStick, -1.0f, 1.0f),
                    glm::clamp(joypad.rightStick + virtualRightStick, -1.0f, 1.0f) + mouseLookStick / (dt > 0.0f ? dt : 1.0f),
                    glm::clamp(roll + virtualRoll, -1.0f, 1.0f), glm::clamp(virtualUp, -1.0f, 1.0f), dt);
    mouseLookStick = glm::vec2(0.0f, 0.0f);

#endif
    return true;
}

bool App::Render(float dt, const glm::ivec2& windowSize)
{
    int width = windowSize.x;
    int height = windowSize.y;

    if (opt.vrMode)
    {
        if (xrBuddy->SessionReady())
        {
            if (!xrBuddy->RenderFrame())
            {
                Log::E("xrBuddy RenderFrame failed\n");
                return false;
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
#ifndef __ANDROID__
        // render desktop.
        Clear(windowSize, true);
        RenderDesktop(windowSize, desktopProgram, xrBuddy->GetColorTexture(), true);

        if (opt.drawFps)
        {
            glm::vec4 viewport(0.0f, 0.0f, (float)width, (float)height);
            glm::vec2 nearFar(Z_NEAR, Z_FAR);
            glm::mat4 projMat = glm::perspective(FOVY, (float)width / (float)height, Z_NEAR, Z_FAR);
            textRenderer->Render(glm::mat4(1.0f), projMat, viewport, nearFar);
        }
#endif
    }
    else
    {
        // lazy init of fbo, fbo is only used for HalfFloat, Float option.
        if (opt.frameBuffer != Options::FrameBuffer::Default && fboSize != windowSize)
        {
            fbo = std::make_shared<FrameBuffer>();

            Texture::Params texParams;
            texParams.minFilter = FilterType::Nearest;
            texParams.magFilter = FilterType::Nearest;
            texParams.sWrap = WrapType::ClampToEdge;
            texParams.tWrap = WrapType::ClampToEdge;
            if (opt.frameBuffer == Options::FrameBuffer::HalfFloat)
            {
                fboColorTex = std::make_shared<Texture>(windowSize.x, windowSize.y,
                                                        GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT,
                                                        texParams);
            }
            else if (opt.frameBuffer == Options::FrameBuffer::Float)
            {
                fboColorTex = std::make_shared<Texture>(windowSize.x, windowSize.y,
                                                        GL_RGBA32F, GL_RGBA, GL_FLOAT,
                                                        texParams);
            }
            else
            {
                Log::E("BAD opt.frameBuffer type!\n");
            }

            fbo->AttachColor(fboColorTex);

            fboSize = windowSize;
        }

        if (opt.frameBuffer != Options::FrameBuffer::Default && fbo)
        {
            fbo->Bind();
        }

        Clear(windowSize, true);

        glm::mat4 cameraMat = flyCam->GetCameraMat();
        glm::vec4 viewport(0.0f, 0.0f, (float)width, (float)height);
        glm::vec2 nearFar(Z_NEAR, Z_FAR);
        glm::mat4 projMat = glm::perspective(FOVY, (float)width / (float)height, Z_NEAR, Z_FAR);

        if (opt.drawDebug)
        {
            debugRenderer->Render(cameraMat, projMat, viewport, nearFar);
        }

        if (cameraPathRenderer)
        {
            cameraPathRenderer->SetShowCameras(opt.drawCameraFrustums);
            cameraPathRenderer->SetShowPath(opt.drawCameraPath);
            cameraPathRenderer->Render(cameraMat, projMat, viewport, nearFar);
        }

        if (opt.drawCarpet)
        {
            magicCarpet->Render(cameraMat, projMat, viewport, nearFar);
        }

        if (opt.drawPointCloud && pointRenderer)
        {
            pointRenderer->Render(cameraMat, projMat, viewport, nearFar);
        }
        else
        {
            splatRenderer->Sort(cameraMat, projMat, viewport, nearFar);
            splatRenderer->Render(cameraMat, projMat, viewport, nearFar);
        }

        if (opt.drawFps)
        {
            textRenderer->Render(cameraMat, projMat, viewport, nearFar);
        }

        if (opt.frameBuffer != Options::FrameBuffer::Default && fbo)
        {
            // render fbo colorTexture as a full screen quad to the default fbo
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            Clear(windowSize, true);
            RenderDesktop(windowSize, desktopProgram, fbo->GetColorTexture()->texture, false);
        }
    }

    debugRenderer->EndFrame();

    frameNum++;

    return true;
}

void App::OnQuit(const VoidCallback& cb)
{
    quitCallback = cb;
}

void App::OnResize(const ResizeCallback& cb)
{
    resizeCallback = cb;
}
