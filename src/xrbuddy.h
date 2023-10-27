#ifndef XR_BUDDY
#define XR_BUDDY

#include <array>
#include <functional>
#include <map>
#include <string>
#include <vector>

#define XR_USE_GRAPHICS_API_OPENGL
#if defined(WIN32)
#define XR_USE_PLATFORM_WIN32
#include <Windows.h>
#elif defined(__ANDROID__)
#define XR_USE_PLATFORM_ANDROID
#else
#define XR_USE_PLATFORM_XLIB
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <glm/glm.hpp>

class XrBuddy
{
public:
    XrBuddy();

    bool Init();
    bool PollEvents();
    bool SyncInput();

    using RenderCallback = std::function<void(const glm::mat4& projMat, const glm::mat4& eyeMat, const glm::vec4& viewport, const glm::vec2& nearFar, int32_t viewNum)>;
    void SetRenderCallback(RenderCallback renderCallbackIn)
    {
        renderCallback = renderCallbackIn;
    }
    bool IsReady() const;
    bool RenderFrame();
    bool Shutdown();

    struct SwapchainInfo
    {
        XrSwapchain handle;
        int32_t width;
        int32_t height;
    };

    struct ActionInfo
    {
        ActionInfo() {}
        ActionInfo(XrAction actionIn, XrActionType typeIn, XrSpace spaceIn) : action(actionIn), type(typeIn), space(spaceIn)
        {
            spaceLocation.type = XR_TYPE_SPACE_LOCATION;
            spaceLocation.next = NULL;
            spaceVelocity.type = XR_TYPE_SPACE_VELOCITY;
            spaceVelocity.next = NULL;
        }
        XrAction action;
        XrActionType type;
        union
        {
            XrActionStateBoolean boolState;
            XrActionStateFloat floatState;
            XrActionStateVector2f vec2State;
            XrActionStatePose poseState;
        } u;
        XrSpace space;
        XrSpaceLocation spaceLocation;
        XrSpaceVelocity spaceVelocity;
    };

    bool GetActionBool(const std::string& actionName, bool* value, bool* valid, bool* changed) const;
    bool GetActionFloat(const std::string& actionName, float* value, bool* valid, bool* changed) const;
    bool GetActionVec2(const std::string& actionName, glm::vec2* value, bool* valid, bool* changed) const;
    bool GetActionPosition(const std::string& actionName, glm::vec3* value, bool* valid, bool* tracked) const;
    bool GetActionOrientation(const std::string& actionName, glm::quat* value, bool* valid, bool* tracked) const;
    bool GetActionLinearVelocity(const std::string& actionName, glm::vec3* value, bool* valid) const;
    bool GetActionAngularVelocity(const std::string& actionName, glm::vec3* value, bool* valid) const;

    uint32_t GetColorTexture() const;

protected:
    bool LocateSpaces(XrTime predictedDisplayTime);
    bool RenderLayer(XrTime predictedDisplayTime,
                     std::vector<XrCompositionLayerProjectionView>& projectionLayerViews,
                     XrCompositionLayerProjection& layer);
    void RenderView(const XrCompositionLayerProjectionView& layerView, uint32_t frameBuffer,
                    uint32_t colorTexture, uint32_t depthTexture, int32_t viewNum);

    bool constructorSucceded = false;
    XrSessionState state = XR_SESSION_STATE_UNKNOWN;
    std::vector<XrExtensionProperties> extensionProps;
    std::vector<XrApiLayerProperties> layerProps;
    std::vector<XrViewConfigurationView> viewConfigs;

    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrActionSet actionSet = XR_NULL_HANDLE;

    std::map<std::string, ActionInfo> actionMap;

    XrSpace stageSpace = XR_NULL_HANDLE;
    XrSpace viewSpace = XR_NULL_HANDLE;
    XrSpaceLocation viewSpaceLocation;
    XrSpaceVelocity viewSpaceVelocity;

    std::vector<SwapchainInfo> swapchains;
    std::vector<std::vector<XrSwapchainImageOpenGLKHR>> swapchainImages;

    uint32_t frameBuffer = 0;
    std::map<uint32_t, uint32_t> colorToDepthMap;
    XrTime lastPredictedDisplayTime = 0;
    uint32_t prevLastColorTexture = 0;
    uint32_t lastColorTexture = 0;

    RenderCallback renderCallback;
};

#endif XR_BUDDY
