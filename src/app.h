/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <string>

#include "maincontext.h"

class CamerasConfig;
class DebugRenderer;
class FlyCam;
class GaussianCloud;
class InputBuddy;
class MagicCarpet;
class PointCloud;
class PointRenderer;
class Program;
class SplatRenderer;
class TextRenderer;
class VrConfig;
class XrBuddy;

union SDL_Event;

class App
{
public:
    App(const MainContext& mainContextIn);
    bool ParseArguments(int argc, const char* argv[]);
    bool Init();
    bool IsFullscreen() const { return opt.fullscreen; }
    void UpdateFps(float fps);
    void ProcessEvent(const SDL_Event& event);
    bool Process(float dt);
    bool Render(float dt, const glm::ivec2& windowSize);

    using VoidCallback = std::function<void()>;
    void OnQuit(const VoidCallback& cb);

    using ResizeCallback = std::function<void(int, int)>;
    void OnResize(const ResizeCallback& cb);

protected:
    struct Options
    {
        bool vrMode = false;
        bool fullscreen = false;
        bool drawCarpet = false;
        bool drawPointCloud = false;
        bool drawDebug = true;
        bool debugLogging = false;
        bool drawFps = true;
    };

    MainContext mainContext;
    Options opt;
    std::string plyFilename;
    std::shared_ptr<DebugRenderer> debugRenderer;
    std::shared_ptr<TextRenderer> textRenderer;
    std::shared_ptr<XrBuddy> xrBuddy;

    std::shared_ptr<CamerasConfig> camerasConfig;
    std::shared_ptr<VrConfig> vrConfig;

    int cameraIndex;
    std::shared_ptr<FlyCam> flyCam;
    std::shared_ptr<MagicCarpet> magicCarpet;

    std::shared_ptr<PointCloud> pointCloud;
    std::shared_ptr<GaussianCloud> gaussianCloud;
    std::shared_ptr<PointRenderer> pointRenderer;
    std::shared_ptr<SplatRenderer> splatRenderer;

    std::shared_ptr<Program> desktopProgram;
    std::shared_ptr<InputBuddy> inputBuddy;

    glm::vec2 virtualLeftStick;
    glm::vec2 virtualRightStick;
    glm::vec2 mouseLookStick;
    bool mouseLook;
    float virtualRoll;
    uint32_t fpsText;
    uint32_t frameNum;

    VoidCallback quitCallback;
    ResizeCallback resizeCallback;
};
