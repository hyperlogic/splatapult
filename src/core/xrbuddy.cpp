/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "xrbuddy.h"

#include <cassert>
#include <string.h>
#include <vector>

#ifndef __ANDROID__
#include <GL/glew.h>
#endif

#include <glm/gtc/type_ptr.hpp>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedNC(NAME, COLOR)
#endif

#include "log.h"
#include "util.h"

static bool printAll = true;

struct PathCache
{
    PathCache(XrInstance instanceIn) : instance(instanceIn) {}
    XrPath operator[](const std::string& key)
    {
        auto iter = pathMap.find(key);
        if (iter != pathMap.end())
        {
            return iter->second;
        }
        else
        {
            XrPath path = XR_NULL_PATH;
            xrStringToPath(instance, key.c_str(), &path);
            pathMap.insert({ key, path });
            return path;
        }
    }
protected:
    XrInstance instance;
    std::map<std::string, XrPath> pathMap;
};

static bool CheckResult(XrInstance instance, XrResult result, const char* str)
{
    if (XR_SUCCEEDED(result))
    {
        return true;
    }

    if (instance != XR_NULL_HANDLE)
    {
        char resultString[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(instance, result, resultString);
        Log::E("%s [%s]\n", str, resultString);
    }
    else
    {
        Log::E("%s\n", str);
    }
    return false;
}

static bool EnumerateExtensions(std::vector<XrExtensionProperties>& extensionProps)
{
    XrResult result;

    uint32_t extensionCount = 0;
    result = xrEnumerateInstanceExtensionProperties(NULL, 0, &extensionCount, NULL);
    if (!CheckResult(NULL, result, "xrEnumerateInstanceExtensionProperties failed"))
    {
        return false;
    }

    extensionProps.resize(extensionCount);
    for (uint32_t i = 0; i < extensionCount; i++)
    {
        extensionProps[i].type = XR_TYPE_EXTENSION_PROPERTIES;
        extensionProps[i].next = NULL;
    }
    result = xrEnumerateInstanceExtensionProperties(NULL, extensionCount, &extensionCount, extensionProps.data());
    if (!CheckResult(NULL, result, "xrEnumerateInstanceExtensionProperties failed"))
    {
        return false;
    }

    bool printExtensions = false;
    if (printExtensions || printAll)
    {
        Log::D("%d extensions:\n", extensionCount);
        for (uint32_t i = 0; i < extensionCount; ++i)
        {
            Log::D("    %s\n", extensionProps[i].extensionName);
        }
    }

    return true;
}

static bool ExtensionSupported(const std::vector<XrExtensionProperties>& extensions, const char* extensionName)
{
    bool supported = false;
    for (auto& extension : extensions)
    {
        if (!strcmp(extensionName, extension.extensionName))
        {
            supported = true;
        }
    }
    return supported;
}

static bool EnumerateApiLayers(std::vector<XrApiLayerProperties>& layerProps)
{
    uint32_t layerCount;
    XrResult result = xrEnumerateApiLayerProperties(0, &layerCount, NULL);
    if (!CheckResult(NULL, result, "xrEnumerateApiLayerProperties"))
    {
        return false;
    }

    layerProps.resize(layerCount);
    for (uint32_t i = 0; i < layerCount; i++)
    {
        layerProps[i].type = XR_TYPE_API_LAYER_PROPERTIES;
        layerProps[i].next = NULL;
    }
    result = xrEnumerateApiLayerProperties(layerCount, &layerCount, layerProps.data());
    if (!CheckResult(NULL, result, "xrEnumerateApiLayerProperties"))
    {
        return false;
    }

    bool printLayers = false;
    if (printLayers || printAll)
    {
        Log::D("%d XrApiLayerProperties:\n", layerCount);
        for (uint32_t i = 0; i < layerCount; i++)
        {
            Log::D("    %s, %s\n", layerProps[i].layerName, layerProps[i].description);
        }
    }

    return true;
}

static bool CreateInstance(XrInstance& instance, const std::vector<const char*>& extensionVec)
{
    if (printAll)
    {
        Log::D("Attempting to enable the following extensions:\n");
        for (auto&& ext : extensionVec)
        {
            Log::D("    %s\n", ext);
        }
    }
    // create openxr instance
    XrResult result;
    XrInstanceCreateInfo ici = {};
    ici.type = XR_TYPE_INSTANCE_CREATE_INFO;
    ici.next = NULL;
    ici.createFlags = 0;
    ici.enabledExtensionCount = (uint32_t)extensionVec.size();
    ici.enabledExtensionNames = extensionVec.data();
    ici.enabledApiLayerCount = 0;
    ici.enabledApiLayerNames = NULL;
    StrCpy_s(ici.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "xrtoy");
    ici.applicationInfo.engineName[0] = '\0';
    ici.applicationInfo.applicationVersion = 1;
    ici.applicationInfo.engineVersion = 0;
    ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    result = xrCreateInstance(&ici, &instance);
    if (!CheckResult(NULL, result, "xrCreateInstance failed"))
    {
        return false;
    }

    bool printRuntimeInfo = false;
    if (printRuntimeInfo || printAll)
    {
        XrInstanceProperties ip = {};
        ip.type = XR_TYPE_INSTANCE_PROPERTIES;
        ip.next = NULL;

        result = xrGetInstanceProperties(instance, &ip);
        if (!CheckResult(instance, result, "xrGetInstanceProperties failed"))
        {
            return false;
        }

        Log::D("Runtime Name: %s\n", ip.runtimeName);
        Log::D("Runtime Version: %d.%d.%d\n",
               XR_VERSION_MAJOR(ip.runtimeVersion),
               XR_VERSION_MINOR(ip.runtimeVersion),
               XR_VERSION_PATCH(ip.runtimeVersion));
    }

    return true;
}

static bool GetSystemId(XrInstance instance, XrSystemId& systemId)
{
    XrResult result;
    XrSystemGetInfo sgi = {};
    sgi.type = XR_TYPE_SYSTEM_GET_INFO;
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    sgi.next = NULL;

    result = xrGetSystem(instance, &sgi, &systemId);
    if (!CheckResult(instance, result, "xrGetSystemFailed"))
    {
        return false;
    }

    return true;
}

static bool GetSystemProperties(XrInstance instance, XrSystemId systemId, XrSystemProperties& sp)
{
    XrResult result;
    sp.type = XR_TYPE_SYSTEM_PROPERTIES;
    sp.next = NULL;
    sp.graphicsProperties = {0};
    sp.trackingProperties = {0};

    result = xrGetSystemProperties(instance, systemId, &sp);
    if (!CheckResult(instance, result, "xrGetSystemProperties failed"))
    {
        return false;
    }

    bool printSystemProperties = false;
    if (printSystemProperties || printAll)
    {
        Log::D("System properties for system \"%s\":\n", sp.systemName);
        Log::D("    maxLayerCount: %d\n", sp.graphicsProperties.maxLayerCount);
        Log::D("    maxSwapChainImageHeight: %d\n", sp.graphicsProperties.maxSwapchainImageHeight);
        Log::D("    maxSwapChainImageWidth: %d\n", sp.graphicsProperties.maxSwapchainImageWidth);
        Log::D("    Orientation Tracking: %s\n", sp.trackingProperties.orientationTracking ? "true" : "false");
        Log::D("    Position Tracking: %s\n", sp.trackingProperties.positionTracking ? "true" : "false");
    }

    return true;
}

static bool SupportsVR(XrInstance instance, XrSystemId systemId)
{
    XrResult result;
    uint32_t viewConfigurationCount;
    result = xrEnumerateViewConfigurations(instance, systemId, 0, &viewConfigurationCount, NULL);
    if (!CheckResult(instance, result, "xrEnumerateViewConfigurations"))
    {
        return false;
    }

    std::vector<XrViewConfigurationType> viewConfigurations(viewConfigurationCount);
    result = xrEnumerateViewConfigurations(instance, systemId, viewConfigurationCount, &viewConfigurationCount, viewConfigurations.data());
    if (!CheckResult(instance, result, "xrEnumerateViewConfigurations"))
    {
        return false;
    }

    XrViewConfigurationType stereoViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    for (uint32_t i = 0; i < viewConfigurationCount; i++)
    {
        XrViewConfigurationProperties vcp = {};
        vcp.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
        vcp.next = NULL;

        result = xrGetViewConfigurationProperties(instance, systemId, viewConfigurations[i], &vcp);
        if (!CheckResult(instance, result, "xrGetViewConfigurationProperties"))
        {
            return false;
        }

        if (viewConfigurations[i] == stereoViewConfigType && vcp.viewConfigurationType == stereoViewConfigType)
        {
            return true;
        }
    }

    return false;
}

static bool EnumerateViewConfigs(XrInstance instance, XrSystemId systemId, std::vector<XrViewConfigurationView>& viewConfigs)
{
    XrResult result;
    uint32_t viewCount;
    XrViewConfigurationType stereoViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    result = xrEnumerateViewConfigurationViews(instance, systemId, stereoViewConfigType, 0, &viewCount, NULL);
    if (!CheckResult(instance, result, "xrEnumerateViewConfigurationViews"))
    {
        return false;
    }

    viewConfigs.resize(viewCount);
    for (uint32_t i = 0; i < viewCount; i++)
    {
        viewConfigs[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
        viewConfigs[i].next = NULL;
    }

    result = xrEnumerateViewConfigurationViews(instance, systemId, stereoViewConfigType, viewCount, &viewCount, viewConfigs.data());
    if (!CheckResult(instance, result, "xrEnumerateViewConfigurationViews"))
    {
        return false;
    }

    bool printViews = false;
    if (printViews || printAll)
    {
        Log::D("%d viewConfigs:\n", viewCount);
        for (uint32_t i = 0; i < viewCount; i++)
        {
            Log::D("    viewConfigs[%d]:\n", i);
            Log::D("        recommendedImageRectWidth: %d\n", viewConfigs[i].recommendedImageRectWidth);
            Log::D("        maxImageRectWidth: %d\n", viewConfigs[i].maxImageRectWidth);
            Log::D("        recommendedImageRectHeight: %d\n", viewConfigs[i].recommendedImageRectHeight);
            Log::D("        maxImageRectHeight: %d\n", viewConfigs[i].maxImageRectHeight);
            Log::D("        recommendedSwapchainSampleCount: %d\n", viewConfigs[i].recommendedSwapchainSampleCount);
            Log::D("        maxSwapchainSampleCount: %d\n", viewConfigs[i].maxSwapchainSampleCount);
        }
    }

    return true;
}

static bool SetColorSpace(XrInstance instance, XrSession session, XrColorSpaceFB colorSpace)
{
    XrResult result;

    PFN_xrSetColorSpaceFB xrSetColorSpaceFB = nullptr;
    result = xrGetInstanceProcAddr(instance, "xrSetColorSpaceFB", (PFN_xrVoidFunction*)&xrSetColorSpaceFB);
    if (!CheckResult(instance, result, "xrGetInstanceProcAddr(xrSetColorSpaceFB)") || !xrSetColorSpaceFB)
    {
        Log::W("get proc addr xrSetColorSpaceFB() failed\n");
        return false;
    }

    result = xrSetColorSpaceFB(session, colorSpace);
    if (!CheckResult(instance, result, "xrSetColorSpaceFB"))
    {
        Log::W("xrSetColorSpaceFB(XR_COLOR_SPACE_REC2020_FB) failed\n");
        return false;
    }

    return true;
}

static bool CreateSession(XrInstance instance, XrSystemId systemId, XrSession& session, const MainContext& mainContext)
{
    XrResult result;

    // check if opengl version is sufficient.
#ifdef XR_USE_GRAPHICS_API_OPENGL
    {
        XrGraphicsRequirementsOpenGLKHR reqs = {};
        reqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
        reqs.next = NULL;
        PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = NULL;
        result = xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction *)&pfnGetOpenGLGraphicsRequirementsKHR);
        if (!CheckResult(instance, result, "xrGetInstanceProcAddr"))
        {
            return false;
        }

        result = pfnGetOpenGLGraphicsRequirementsKHR(instance, systemId, &reqs);
        if (!CheckResult(instance, result, "GetOpenGLGraphicsRequirementsPKR"))
        {
            return false;
        }

        GLint major = 0;
        GLint minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        const XrVersion desiredApiVersion = XR_MAKE_VERSION(major, minor, 0);

        bool printVersion = false;
        if (printVersion || printAll)
        {
            Log::D("current OpenGL version: %d.%d.%d\n", XR_VERSION_MAJOR(desiredApiVersion),
                   XR_VERSION_MINOR(desiredApiVersion), XR_VERSION_PATCH(desiredApiVersion));
            Log::D("minimum OpenGL version: %d.%d.%d\n", XR_VERSION_MAJOR(reqs.minApiVersionSupported),
                   XR_VERSION_MINOR(reqs.minApiVersionSupported), XR_VERSION_PATCH(reqs.minApiVersionSupported));
        }

        if (reqs.minApiVersionSupported > desiredApiVersion)
        {
            Log::E("Runtime does not support desired Graphics API and/or version\n");
            return false;
        }
    }

#ifdef WIN32
    XrGraphicsBindingOpenGLWin32KHR glBinding = {};
    glBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
    glBinding.next = NULL;
    glBinding.hDC = wglGetCurrentDC();
    glBinding.hGLRC = wglGetCurrentContext();
#else
    XrGraphicsBindingOpenGLXlibKHR glBinding = {};
    glBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
    glBinding.next = NULL;
    glBinding.xDisplay = mainContext.xdisplay;
    glBinding.glxDrawable = mainContext.glxDrawable;
    glBinding.glxContext = mainContext.glxContext;
#endif

#elif defined (XR_USE_GRAPHICS_API_OPENGL_ES)
    XrGraphicsRequirementsOpenGLESKHR reqs = {};
    reqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR;
    reqs.next = NULL;
    PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = NULL;
    result = xrGetInstanceProcAddr(instance, "xrGetOpenGLESGraphicsRequirementsKHR", (PFN_xrVoidFunction *)&pfnGetOpenGLESGraphicsRequirementsKHR);
    if (!CheckResult(instance, result, "xrGetInstanceProcAddr"))
    {
        return false;
    }

    result = pfnGetOpenGLESGraphicsRequirementsKHR(instance, systemId, &reqs);
    if (!CheckResult(instance, result, "GetOpenGLGraphicsRequirementsPKR"))
    {
        return false;
    }

    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    const XrVersion desiredApiVersion = XR_MAKE_VERSION(major, minor, 0);

    bool printVersion = false;
    if (printVersion || printAll)
    {
        Log::D("current OpenGLES version: %d.%d.%d\n", XR_VERSION_MAJOR(desiredApiVersion),
               XR_VERSION_MINOR(desiredApiVersion), XR_VERSION_PATCH(desiredApiVersion));
        Log::D("minimum OpenGLES version: %d.%d.%d\n", XR_VERSION_MAJOR(reqs.minApiVersionSupported),
               XR_VERSION_MINOR(reqs.minApiVersionSupported), XR_VERSION_PATCH(reqs.minApiVersionSupported));
    }

    if (reqs.minApiVersionSupported > desiredApiVersion)
    {
        Log::E("Runtime does not support desired Graphics API and/or version\n");
        return false;
    }

    XrGraphicsBindingOpenGLESAndroidKHR glBinding = {};
    glBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
    glBinding.next = NULL;
    glBinding.display = mainContext.display;
    glBinding.config = mainContext.config;
    glBinding.context = mainContext.context;

#endif

    XrSessionCreateInfo sci = {};
    sci.type = XR_TYPE_SESSION_CREATE_INFO;
    sci.next = &glBinding;
    sci.createFlags = 0;
    sci.systemId = systemId;

    result = xrCreateSession(instance, &sci, &session);
    if (!CheckResult(instance, result, "xrCreateSession"))
    {
        return false;
    }

    return true;
}

static bool CreateActions(XrInstance instance, XrSystemId systemId, XrSession session, XrActionSet& actionSet,
                          std::map<std::string, XrBuddy::ActionInfo>& actionMap)
{
    XrResult result;

    // create action set
    XrActionSetCreateInfo asci = {};
    asci.type = XR_TYPE_ACTION_SET_CREATE_INFO;
    asci.next = NULL;
    StrCpy_s(asci.actionSetName, XR_MAX_ACTION_NAME_SIZE, "default");
    StrCpy_s(asci.localizedActionSetName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE, "Default");
    asci.priority = 0;
    result = xrCreateActionSet(instance, &asci, &actionSet);
    if (!CheckResult(instance, result, "xrCreateActionSet"))
    {
        return false;
    }

    std::vector<std::pair<std::string, XrActionType>> actionPairVec = {
        {"l_select_click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"r_select_click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"l_menu_click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"r_menu_click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"l_squeeze_click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"r_squeeze_click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"l_trackpad_click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"r_trackpad_click", XR_ACTION_TYPE_BOOLEAN_INPUT},
        {"l_trackpad_x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"r_trackpad_x", XR_ACTION_TYPE_FLOAT_INPUT},
        {"l_trackpad_y", XR_ACTION_TYPE_FLOAT_INPUT},
        {"r_trackpad_y", XR_ACTION_TYPE_FLOAT_INPUT},

        {"l_grip_pose", XR_ACTION_TYPE_POSE_INPUT},
        {"r_grip_pose", XR_ACTION_TYPE_POSE_INPUT},
        {"l_aim_pose", XR_ACTION_TYPE_POSE_INPUT},
        {"r_aim_pose", XR_ACTION_TYPE_POSE_INPUT},

        {"l_haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT},
        {"r_haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT},

        {"l_stick", XR_ACTION_TYPE_VECTOR2F_INPUT},
        {"r_stick", XR_ACTION_TYPE_VECTOR2F_INPUT}
    };

    for (auto& actionPair : actionPairVec)
    {
        // selectAction
        XrAction action = XR_NULL_HANDLE;
        XrActionCreateInfo aci = {};
        aci.type = XR_TYPE_ACTION_CREATE_INFO;
        aci.next = NULL;
        aci.actionType = actionPair.second;
        StrCpy_s(aci.actionName, XR_MAX_ACTION_NAME_SIZE, actionPair.first.c_str());
        StrCpy_s(aci.localizedActionName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE, actionPair.first.c_str());
        aci.countSubactionPaths = 0;
        aci.subactionPaths = NULL;
        result = xrCreateAction(actionSet, &aci, &action);
        if (!CheckResult(instance, result, "xrCreateAction"))
        {
            return false;
        }

        XrSpace space = XR_NULL_HANDLE;
        if (actionPair.second == XR_ACTION_TYPE_POSE_INPUT)
        {
            XrActionSpaceCreateInfo aspci = {};
            aspci.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
            aspci.next = NULL;
            aspci.action = action;
            XrPosef identity;
            identity.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
            identity.position = {0.0f, 0.0f, 0.0f};
            aspci.poseInActionSpace = identity;
            aspci.subactionPath = 0;
            result = xrCreateActionSpace(session, &aspci, &space);
            if (!CheckResult(instance, result, "xrCreateActionSpace"))
            {
                return false;
            }
        }
        actionMap[actionPair.first] = XrBuddy::ActionInfo(action, actionPair.second, space);
    }

    PathCache pathCache(instance);
    if (!CheckResult(instance, result, "xrStringToPath"))
    {
        return false;
    }

    // KHR Simple
    {
        XrPath interactionProfilePath = XR_NULL_PATH;
        xrStringToPath(instance, "/interaction_profiles/khr/simple_controller", &interactionProfilePath);
        std::vector<XrActionSuggestedBinding> bindings = {
            {actionMap["l_select_click"].action, pathCache["/user/hand/left/input/select/click"]},
            {actionMap["r_select_click"].action, pathCache["/user/hand/right/input/select/click"]},
            {actionMap["l_menu_click"].action, pathCache["/user/hand/left/input/menu/click"]},
            {actionMap["r_menu_click"].action, pathCache["/user/hand/right/input/menu/click"]},
            {actionMap["l_grip_pose"].action, pathCache["/user/hand/left/input/grip/pose"]},
            {actionMap["r_grip_pose"].action, pathCache["/user/hand/right/input/grip/pose"]},
            {actionMap["l_aim_pose"].action, pathCache["/user/hand/left/input/aim/pose"]},
            {actionMap["r_aim_pose"].action, pathCache["/user/hand/right/input/aim/pose"]},
            {actionMap["l_haptic"].action, pathCache["/user/hand/left/output/haptic"]},
            {actionMap["r_haptic"].action, pathCache["/user/hand/right/output/haptic"]}
        };

        XrInteractionProfileSuggestedBinding suggestedBindings = {};
        suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        suggestedBindings.next = NULL;
        suggestedBindings.interactionProfile = interactionProfilePath;
        suggestedBindings.suggestedBindings = bindings.data();
        suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
        result = xrSuggestInteractionProfileBindings(instance, &suggestedBindings);
        if (!CheckResult(instance, result, "xrSuggestInteractionProfileBindings"))
        {
            return false;
        }
    }

    // oculus touch
    {
        XrPath interactionProfilePath = XR_NULL_PATH;
        xrStringToPath(instance, "/interaction_profiles/oculus/touch_controller", &interactionProfilePath);
        std::vector<XrActionSuggestedBinding> bindings = {
            {actionMap["l_select_click"].action, pathCache["/user/hand/left/input/trigger/value"]},
            {actionMap["r_select_click"].action, pathCache["/user/hand/right/input/trigger/value"]},
            {actionMap["l_menu_click"].action, pathCache["/user/hand/left/input/menu/click"]},
            //{actionMap["r_menu_click"].action, pathCache["/user/hand/right/input/menu/click"]},  // right controller has no menu button
            {actionMap["l_squeeze_click"].action, pathCache["/user/hand/left/input/squeeze/value"]},
            {actionMap["r_squeeze_click"].action, pathCache["/user/hand/right/input/squeeze/value"]},

            {actionMap["l_grip_pose"].action, pathCache["/user/hand/left/input/grip/pose"]},
            {actionMap["r_grip_pose"].action, pathCache["/user/hand/right/input/grip/pose"]},
            {actionMap["l_aim_pose"].action, pathCache["/user/hand/left/input/aim/pose"]},
            {actionMap["r_aim_pose"].action, pathCache["/user/hand/right/input/aim/pose"]},
            {actionMap["l_haptic"].action, pathCache["/user/hand/left/output/haptic"]},
            {actionMap["r_haptic"].action, pathCache["/user/hand/right/output/haptic"]},

            {actionMap["l_stick"].action, pathCache["/user/hand/left/input/thumbstick"]},
            {actionMap["r_stick"].action, pathCache["/user/hand/right/input/thumbstick"]}
        };

        XrInteractionProfileSuggestedBinding suggestedBindings = {};
        suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        suggestedBindings.next = NULL;
        suggestedBindings.interactionProfile = interactionProfilePath;
        suggestedBindings.suggestedBindings = bindings.data();
        suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
        result = xrSuggestInteractionProfileBindings(instance, &suggestedBindings);
        if (!CheckResult(instance, result, "xrSuggestInteractionProfileBindings (oculus)"))
        {
            return false;
        }
    }

    // vive
    {
        XrPath interactionProfilePath = XR_NULL_PATH;
        xrStringToPath(instance, "/interaction_profiles/htc/vive_controller", &interactionProfilePath);
        std::vector<XrActionSuggestedBinding> bindings = {
            {actionMap["l_menu_click"].action, pathCache["/user/hand/left/input/menu/click"]},
            {actionMap["r_menu_click"].action, pathCache["/user/hand/right/input/menu/click"]},

            {actionMap["l_select_click"].action, pathCache["/user/hand/left/input/trigger/click"]},
            {actionMap["r_select_click"].action, pathCache["/user/hand/right/input/trigger/click"]},

            {actionMap["l_squeeze_click"].action, pathCache["/user/hand/left/input/squeeze/click"]},
            {actionMap["r_squeeze_click"].action, pathCache["/user/hand/right/input/squeeze/click"]},

            {actionMap["l_trackpad_click"].action, pathCache["/user/hand/left/input/trackpad/click"]},
            {actionMap["r_trackpad_click"].action, pathCache["/user/hand/right/input/trackpad/click"]},

            {actionMap["l_trackpad_x"].action, pathCache["/user/hand/left/input/trackpad/x"]},
            {actionMap["r_trackpad_x"].action, pathCache["/user/hand/right/input/trackpad/x"]},
            {actionMap["l_trackpad_y"].action, pathCache["/user/hand/left/input/trackpad/y"]},
            {actionMap["r_trackpad_y"].action, pathCache["/user/hand/right/input/trackpad/y"]},

            {actionMap["l_grip_pose"].action, pathCache["/user/hand/left/input/grip/pose"]},
            {actionMap["r_grip_pose"].action, pathCache["/user/hand/right/input/grip/pose"]},

            {actionMap["l_aim_pose"].action, pathCache["/user/hand/left/input/aim/pose"]},
            {actionMap["r_aim_pose"].action, pathCache["/user/hand/right/input/aim/pose"]},

            {actionMap["l_haptic"].action, pathCache["/user/hand/left/output/haptic"]},
            {actionMap["r_haptic"].action, pathCache["/user/hand/right/output/haptic"]}
        };

        XrInteractionProfileSuggestedBinding suggestedBindings = {};
        suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        suggestedBindings.next = NULL;
        suggestedBindings.interactionProfile = interactionProfilePath;
        suggestedBindings.suggestedBindings = bindings.data();
        suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
        result = xrSuggestInteractionProfileBindings(instance, &suggestedBindings);
        if (!CheckResult(instance, result, "xrSuggestInteractionProfileBindings (vive)"))
        {
            return false;
        }
    }

    // TODO
#if 0
    // microsoft mixed reality
    {
        XrPath interactionProfilePath = XR_NULL_PATH;
        xrStringToPath(instance, "/interaction_profiles/microsoft/motion_controller", &interactionProfilePath);
        std::vector<XrActionSuggestedBinding> bindings = {
            {selectAction, squeezeClickPath[0]},
            {selectAction, squeezeClickPath[1]},
            {aimAction, aimPath[0]},
            {aimAction, aimPath[1]},
            {menuAction, menuClickPath[0]},
            {menuAction, menuClickPath[1]},
            {vibrateAction, hapticPath[0]},
            {vibrateAction, hapticPath[1]}
        };

        XrInteractionProfileSuggestedBinding suggestedBindings = {};
        suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        suggestedBindings.next = NULL;
        suggestedBindings.interactionProfile = interactionProfilePath;
        suggestedBindings.suggestedBindings = bindings.data();
        suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
        result = xrSuggestInteractionProfileBindings(instance, &suggestedBindings);
        if (!CheckResult(instance, result, "xrSuggestInteractionProfileBindings"))
        {
            return false;
        }
    }
#endif

    XrSessionActionSetsAttachInfo sasai = {};
    sasai.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
    sasai.next = NULL;
    sasai.countActionSets = 1;
    sasai.actionSets = &actionSet;
    result = xrAttachSessionActionSets(session, &sasai);
    if (!CheckResult(instance, result, "xrSessionActionSetsAttachInfo"))
    {
        return false;
    }

    return true;
}

static bool CreateSpaces(XrInstance instance, XrSystemId systemId, XrSession session, XrSpace& stageSpace, XrSpace& viewSpace, XrSpaceLocation& viewSpaceLocation, XrSpaceVelocity& viewSpaceVelocity)
{
    XrResult result;
    bool printReferenceSpaces = true;
    if (printReferenceSpaces || printAll)
    {
        uint32_t referenceSpacesCount;
        result = xrEnumerateReferenceSpaces(session, 0, &referenceSpacesCount, NULL);
        if (!CheckResult(instance, result, "xrEnumerateReferenceSpaces"))
        {
            return false;
        }

        std::vector<XrReferenceSpaceType> referenceSpaces(referenceSpacesCount, XR_REFERENCE_SPACE_TYPE_VIEW);
        result = xrEnumerateReferenceSpaces(session, referenceSpacesCount, &referenceSpacesCount, referenceSpaces.data());
        if (!CheckResult(instance, result, "xrEnumerateReferenceSpaces"))
        {
            return false;
        }

        Log::D("referenceSpaces:\n");
        for (uint32_t i = 0; i < referenceSpacesCount; i++)
        {
            switch (referenceSpaces[i])
            {
            case XR_REFERENCE_SPACE_TYPE_VIEW:
                Log::D("    XR_REFERENCE_SPACE_TYPE_VIEW\n");
                break;
            case XR_REFERENCE_SPACE_TYPE_LOCAL:
                Log::D("    XR_REFERENCE_SPACE_TYPE_LOCAL\n");
                break;
            case XR_REFERENCE_SPACE_TYPE_STAGE:
                Log::D("    XR_REFERENCE_SPACE_TYPE_STAGE\n");
                break;
            default:
                Log::D("    XR_REFERENCE_SPACE_TYPE_%d\n", referenceSpaces[i]);
                break;
            }
        }
    }

    XrPosef identityPose;
    identityPose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
    identityPose.position = {0.0f, 0.0f, 0.0f};

    XrReferenceSpaceCreateInfo rsci = {};
    rsci.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
    rsci.next = NULL;
    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    rsci.poseInReferenceSpace = identityPose;

    result = xrCreateReferenceSpace(session, &rsci, &stageSpace);
    if (!CheckResult(instance, result, "xrCreateReferenceSpace"))
    {
        return false;
    }

    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    result = xrCreateReferenceSpace(session, &rsci, &viewSpace);
    if (!CheckResult(instance, result, "xrCreateReferenceSpace"))
    {
        return false;
    }

    viewSpaceLocation.type = XR_TYPE_SPACE_LOCATION;
    viewSpaceLocation.next = &viewSpaceVelocity;
    viewSpaceVelocity.type = XR_TYPE_SPACE_VELOCITY;
    viewSpaceVelocity.next = NULL;

    return true;
}

static bool BeginSession(XrInstance instance, XrSystemId systemId, XrSession session)
{
    XrResult result;
    XrViewConfigurationType stereoViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    XrSessionBeginInfo sbi = {};
    sbi.type = XR_TYPE_SESSION_BEGIN_INFO;
    sbi.next = NULL;
    sbi.primaryViewConfigurationType = stereoViewConfigType;

    result = xrBeginSession(session, &sbi);
    if (!CheckResult(instance, result, "xrBeginSession"))
    {
        return false;
    }

    return true;
}

static bool EndSession(XrInstance instance, XrSystemId systemId, XrSession session)
{
    XrResult result;
    result = xrEndSession(session);
    if (!CheckResult(instance, result, "xrEndSession"))
    {
        return false;
    }

    return true;
}

static bool CreateFrameBuffer(GLuint& frameBuffer)
{
    glGenFramebuffers(1, &frameBuffer);
    return true;
}

static bool CreateSwapchains(XrInstance instance, XrSession session,
                             const std::vector<XrViewConfigurationView>& viewConfigs,
                             std::vector<XrBuddy::SwapchainInfo>& swapchains,
                             std::vector<std::vector<XrBuddy::SwapchainImage>>& swapchainImages)
{
    XrResult result;
    uint32_t swapchainFormatCount;
    result = xrEnumerateSwapchainFormats(session, 0, &swapchainFormatCount, NULL);
    if (!CheckResult(instance, result, "xrEnumerateSwapchainFormats"))
    {
        return false;
    }

    std::vector<int64_t> swapchainFormats(swapchainFormatCount);
    result = xrEnumerateSwapchainFormats(session, swapchainFormatCount, &swapchainFormatCount, swapchainFormats.data());
    if (!CheckResult(instance, result, "xrEnumerateSwapchainFormats"))
    {
        return false;
    }

    if (printAll)
    {
        Log::D("xrEnumerateSwapchainFormats, count = %d\n", swapchainFormatCount);
        for (uint32_t i = 0; i < swapchainFormatCount; i++)
        {
            Log::D("    format[%d] = 0x%x\n", i, swapchainFormats[i]);
        }
    }

    std::vector<uint64_t> formats = {GL_R11F_G11F_B10F_EXT, GL_RGB16F_EXT, GL_RGBA};
    uint32_t foundFormatIndex = swapchainFormatCount;
    for (auto&& desiredFormat : formats)
    {
        foundFormatIndex = swapchainFormatCount;
        for (uint32_t i = 0; i < swapchainFormatCount; i++)
        {
            if (swapchainFormats[i] == desiredFormat)
            {
                if (printAll)
                {
                    Log::D("found desired framebuffer format 0x%x!\n", desiredFormat);
                }
                foundFormatIndex = i;
                break;
            }
        }
        if (foundFormatIndex != swapchainFormatCount)
        {
            break;
        }
    }

    int64_t format;
    if (foundFormatIndex == swapchainFormatCount)
    {
        Log::W("could not find any desired swapchain format!\n");
        format = swapchainFormats[0];
    }
    else
    {
        format = swapchainFormats[foundFormatIndex];
    }

    std::vector<uint32_t> swapchainLengths(viewConfigs.size());

    swapchains.resize(viewConfigs.size());

    for (uint32_t i = 0; i < viewConfigs.size(); i++)
    {
        XrSwapchainCreateInfo sci = {};
        sci.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        sci.next = NULL;
        sci.createFlags = 0;
        sci.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        sci.format = format;
        sci.sampleCount = 1;
        sci.width = viewConfigs[i].recommendedImageRectWidth;
        sci.height = viewConfigs[i].recommendedImageRectHeight;
        sci.faceCount = 1;
        sci.arraySize = 1;
        sci.mipCount = 1;

        XrSwapchain swapchainHandle = XR_NULL_HANDLE;
        result = xrCreateSwapchain(session, &sci, &swapchainHandle);
        if (!CheckResult(instance, result, "xrCreateSwapchain"))
        {
            return false;
        }

        swapchains[i].handle = swapchainHandle;
        swapchains[i].width = sci.width;
        swapchains[i].height = sci.height;

        uint32_t length;
        result = xrEnumerateSwapchainImages(swapchains[i].handle, 0, &length, nullptr);
        if (!CheckResult(instance, result, "xrEnumerateSwapchainImages"))
        {
            return false;
        }
        swapchainLengths[i] = length;
    }

    swapchainImages.resize(viewConfigs.size());
    for (uint32_t i = 0; i < viewConfigs.size(); i++)
    {
        swapchainImages[i].resize(swapchainLengths[i]);
        for (uint32_t j = 0; j < swapchainLengths[i]; j++)
        {
#if defined(XR_USE_GRAPHICS_API_OPENGL)
            swapchainImages[i][j].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
#elif defined(XR_USE_GRAPHICS_API_OPENGL_ES)
            swapchainImages[i][j].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
#endif
            swapchainImages[i][j].next = NULL;
        }

        result = xrEnumerateSwapchainImages(swapchains[i].handle, swapchainLengths[i], &swapchainLengths[i],
                                            (XrSwapchainImageBaseHeader*)(swapchainImages[i].data()));
        if (!CheckResult(instance, result, "xrEnumerateSwapchainImages"))
        {
            return false;
        }
    }

    return true;
}

static GLuint CreateDepthTexture(GLuint colorTexture, GLint width, GLint height)
{
    glBindTexture(GL_TEXTURE_2D, colorTexture);

    uint32_t depthTexture;
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    return depthTexture;
}

XrBuddy::XrBuddy(MainContext& mainContextIn, const glm::vec2& nearFarIn):
    mainContext(mainContextIn)
{
    nearFar = nearFarIn;

#ifdef XR_USE_GRAPHICS_API_OPENGL
    std::vector<const char*> requiredExtensionVec = {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
#elif defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    std::vector<const char*> requiredExtensionVec = {XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME};
#endif
    std::vector<const char*> optionalExtensionVec = {XR_FB_COLOR_SPACE_EXTENSION_NAME};
    std::vector<const char*> extensionVec;

#ifdef __ANDROID__
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
    xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&xrInitializeLoaderKHR);
    if (xrInitializeLoaderKHR != NULL)
    {
        XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid = {};
        loaderInitializeInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
        loaderInitializeInfoAndroid.applicationVM = mainContext.androidApp->activity->vm;
        loaderInitializeInfoAndroid.applicationContext = mainContext.androidApp->activity->clazz;
        xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfoAndroid);
    }
#endif

    if (!EnumerateExtensions(extensionProps))
    {
        return;
    }

    for (auto&& ext : requiredExtensionVec)
    {
        if (!ExtensionSupported(extensionProps, ext))
        {
            Log::W("required extension \"%s\" not supported!\n", ext);
            return;
        }
        extensionVec.push_back(ext);
    }

    for (auto&& ext : optionalExtensionVec)
    {
        if (ExtensionSupported(extensionProps, ext))
        {
            extensionVec.push_back(ext);
        }
    }

    if (!EnumerateApiLayers(layerProps))
    {
        return;
    }

    if (!CreateInstance(instance, extensionVec))
    {
        return;
    }

    if (!GetSystemId(instance, systemId))
    {
        return;
    }

    if (!GetSystemProperties(instance, systemId, systemProperties))
    {
        return;
    }

    if (!SupportsVR(instance, systemId))
    {
        Log::E("System doesn't support VR\n");
        return;
    }

    if (!EnumerateViewConfigs(instance, systemId, viewConfigs))
    {
        return;
    }

    constructorSucceded = true;
}

bool XrBuddy::Init()
{
    if (!constructorSucceded)
    {
        return false;
    }

    if (!CreateSession(instance, systemId, session, mainContext))
    {
        return false;
    }

    if (ExtensionSupported(extensionProps, XR_FB_COLOR_SPACE_EXTENSION_NAME))
    {
        SetColorSpace(instance, session, XR_COLOR_SPACE_REC709_FB);
    }

    if (!CreateActions(instance, systemId, session, actionSet, actionMap))
    {
        return false;
    }

    if (!CreateSpaces(instance, systemId, session, stageSpace, viewSpace, viewSpaceLocation, viewSpaceVelocity))
    {
        return false;
    }

    if (!CreateFrameBuffer(frameBuffer))
    {
        return false;
    }

    if (!CreateSwapchains(instance, session, viewConfigs, swapchains, swapchainImages))
    {
        return false;
    }

    return true;
}

bool XrBuddy::PollEvents()
{
    ZoneScoped;

    XrEventDataBuffer xrEvent = {};
    xrEvent.type = XR_TYPE_EVENT_DATA_BUFFER;
    xrEvent.next = NULL;

    while (xrPollEvent(instance, &xrEvent) == XR_SUCCESS)
    {
        switch (xrEvent.type)
        {
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            // Receiving the XrEventDataInstanceLossPending event structure indicates that the application is about to lose the indicated XrInstance at the indicated lossTime in the future.
            // The application should call xrDestroyInstance and relinquish any instance-specific resources.
            // This typically occurs to make way for a replacement of the underlying runtime, such as via a software update.
            Log::D("xrEvent: XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING\n");
            break;
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
        {
            // Receiving the XrEventDataSessionStateChanged event structure indicates that the application has changed lifecycle stat.e
            Log::D("xrEvent: XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED -> ");
            XrEventDataSessionStateChanged* ssc = (XrEventDataSessionStateChanged*)&xrEvent;
            state = ssc->state;
            switch (state)
            {
            case XR_SESSION_STATE_IDLE:
                // The initial state after calling xrCreateSession or returned to after calling xrEndSession.
                Log::D("XR_SESSION_STATE_IDLE\n");
                break;
            case XR_SESSION_STATE_READY:
                // The application is ready to call xrBeginSession and sync its frame loop with the runtime.
                Log::D("XR_SESSION_STATE_READY\n");
                if (!BeginSession(instance, systemId, session))
                {
                    return false;
                }
                sessionReady = true;
                break;
            case XR_SESSION_STATE_SYNCHRONIZED:
                // The application has synced its frame loop with the runtime but is not visible to the user.
                Log::D("XR_SESSION_STATE_SYNCHRONIZED\n");
                break;
            case XR_SESSION_STATE_VISIBLE:
                // The application has synced its frame loop with the runtime and is visible to the user but cannot receive XR input.
                Log::D("XR_SESSION_STATE_VISIBLE\n");
                break;
            case XR_SESSION_STATE_FOCUSED:
                // The application has synced its frame loop with the runtime, is visible to the user and can receive XR input.
                Log::D("XR_SESSION_STATE_FOCUSED\n");
                break;
            case XR_SESSION_STATE_STOPPING:
                Log::D("XR_SESSION_STATE_STOPPING\n");
                // The application should exit its frame loop and call xrEndSession.
                if (!EndSession(instance, systemId, session))
                {
                    return false;
                }
                sessionReady = false;
                break;
            case XR_SESSION_STATE_LOSS_PENDING:
                Log::D("XR_SESSION_STATE_LOSS_PENDING\n");
                // The session is in the process of being lost. The application should destroy the current session and can optionally recreate it.
                break;
            case XR_SESSION_STATE_EXITING:
                Log::D("XR_SESSION_STATE_EXITING\n");
                // The application should end its XR experience and not automatically restart it.
                break;
            default:
                Log::D("XR_SESSION_STATE_??? %d\n", (int)state);
                break;
            }
            break;
        }
        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
            // The XrEventDataReferenceSpaceChangePending event is sent to the application to notify it that the origin (and perhaps the bounds) of a reference space is changing.
            Log::D("XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING\n");
            break;
        case XR_TYPE_EVENT_DATA_EVENTS_LOST:
            // Receiving the XrEventDataEventsLost event structure indicates that the event queue overflowed and some events were removed at the position within the queue at which this event was found.
            Log::D("xrEvent: XR_TYPE_EVENT_DATA_EVENTS_LOST\n");
            break;
        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
            // The XrEventDataInteractionProfileChanged event is sent to the application to notify it that the active input form factor for one or more top level user paths has changed.:
            Log::D("XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED\n");
            break;
        default:
            Log::D("Unhandled event type %d\n", xrEvent.type);
            break;
        }

        xrEvent.type = XR_TYPE_EVENT_DATA_BUFFER;
        xrEvent.next = NULL;
    }

    return true;
}

bool XrBuddy::SyncInput()
{
    ZoneScoped;

    if (state == XR_SESSION_STATE_FOCUSED)
    {
        XrResult result;

        XrActiveActionSet aas = {};
        aas.actionSet = actionSet;
        aas.subactionPath = XR_NULL_PATH;
        XrActionsSyncInfo asi = {};
        asi.type = XR_TYPE_ACTIONS_SYNC_INFO;
        asi.next = NULL;
        asi.countActiveActionSets = 1;
        asi.activeActionSets = &aas;
        result = xrSyncActions(session, &asi);
        if (!CheckResult(instance, result, "xrSyncActions"))
        {
            return false;
        }

        bool printActions = false;

        for (auto& iter : actionMap)
        {
            ActionInfo& actionInfo = iter.second;
            XrActionStateGetInfo getInfo = {};
            getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
            getInfo.next = NULL;
            getInfo.action = actionInfo.action;
            getInfo.subactionPath = 0;
            iter.second.u.boolState.next = NULL;
            switch (iter.second.type)
            {
            case XR_ACTION_TYPE_BOOLEAN_INPUT:
                iter.second.u.boolState.type = XR_TYPE_ACTION_STATE_BOOLEAN;
                result = xrGetActionStateBoolean(session, &getInfo, &iter.second.u.boolState);
                if (!CheckResult(instance, result, "xrGetActionStateBoolean"))
                {
                    return false;
                }
                if (printActions && iter.second.u.boolState.changedSinceLastSync)
                {
                    Log::D("action %s:\n", iter.first.c_str());
                    Log::D("    currentState: %s\n", iter.second.u.boolState.currentState ? "true" : "false");
                    Log::D("    changedSinceLastSync: %s\n", iter.second.u.boolState.changedSinceLastSync ? "true" : "false");
                    Log::D("    lastChangeTime: %lld\n", iter.second.u.boolState.lastChangeTime);
                    Log::D("    isActive: %s\n", iter.second.u.boolState.isActive ? "true" : "false");
                }
                break;
            case XR_ACTION_TYPE_FLOAT_INPUT:
                iter.second.u.floatState.type = XR_TYPE_ACTION_STATE_FLOAT;
                result = xrGetActionStateFloat(session, &getInfo, &iter.second.u.floatState);
                if (!CheckResult(instance, result, "xrGetActionStateFloat"))
                {
                    return false;
                }
                if (printActions && iter.second.u.floatState.changedSinceLastSync)
                {
                    Log::D("action %s:\n", iter.first.c_str());
                    Log::D("    currentState: %.5f\n", iter.second.u.floatState.currentState);
                    Log::D("    changedSinceLastSync: %s\n", iter.second.u.floatState.changedSinceLastSync ? "true" : "false");
                    Log::D("    lastChangeTime: %lld\n", iter.second.u.floatState.lastChangeTime);
                    Log::D("    isActive: %s\n", iter.second.u.floatState.isActive ? "true" : "false");
                }
                break;
            case XR_ACTION_TYPE_VECTOR2F_INPUT:
                iter.second.u.vec2State.type = XR_TYPE_ACTION_STATE_VECTOR2F;
                result = xrGetActionStateVector2f(session, &getInfo, &iter.second.u.vec2State);
                if (!CheckResult(instance, result, "xrGetActionStateVector2f"))
                {
                    return false;
                }
                if (printActions && iter.second.u.vec2State.changedSinceLastSync)
                {
                    Log::D("action %s:\n", iter.first.c_str());
                    Log::D("    currentState: (%.5f, %.5f)\n", iter.second.u.vec2State.currentState.x, iter.second.u.vec2State.currentState.y);
                    Log::D("    changedSinceLastSync: %s\n", iter.second.u.vec2State.changedSinceLastSync ? "true" : "false");
                    Log::D("    lastChangeTime: %lld\n", iter.second.u.vec2State.lastChangeTime);
                    Log::D("    isActive: %s\n", iter.second.u.vec2State.isActive ? "true" : "false");
                }
                break;
            case XR_ACTION_TYPE_POSE_INPUT:
                iter.second.u.poseState.type = XR_TYPE_ACTION_STATE_POSE;
                result = xrGetActionStatePose(session, &getInfo, &iter.second.u.poseState);
                if (!CheckResult(instance, result, "xrGetActionStatePose"))
                {
                    return false;
                }
                break;
            default:
                break;
            }
        }
    }

    return true;
}

bool XrBuddy::GetActionBool(const std::string& actionName, bool* value, bool* valid, bool* changed) const
{
    auto iter = actionMap.find(actionName);
    if (iter == actionMap.end() || iter->second.type != XR_ACTION_TYPE_BOOLEAN_INPUT)
    {
        return false;
    }

    if (state != XR_SESSION_STATE_FOCUSED)
    {
        *value = false;
        *valid = false;
        *changed = false;
        return true;
    }

    *value = iter->second.u.boolState.currentState;
    *valid = iter->second.u.boolState.isActive;
    *changed = iter->second.u.boolState.changedSinceLastSync;
    return true;
}

bool XrBuddy::GetActionFloat(const std::string& actionName, float* value, bool* valid, bool* changed) const
{
    auto iter = actionMap.find(actionName);
    if (iter == actionMap.end() || iter->second.type != XR_ACTION_TYPE_FLOAT_INPUT)
    {
        return false;
    }

    if (state != XR_SESSION_STATE_FOCUSED)
    {
        *value = 0.0f;
        *valid = false;
        *changed = false;
        return true;
    }

    *value = iter->second.u.floatState.currentState;
    *valid = iter->second.u.floatState.isActive;
    *changed = iter->second.u.floatState.changedSinceLastSync;
    return true;
}

bool XrBuddy::GetActionVec2(const std::string& actionName, glm::vec2* value, bool* valid, bool* changed) const
{
    auto iter = actionMap.find(actionName);
    if (iter == actionMap.end() || iter->second.type != XR_ACTION_TYPE_VECTOR2F_INPUT)
    {
        return false;
    }

    if (state != XR_SESSION_STATE_FOCUSED)
    {
        *value = glm::vec2(0.0f);
        *valid = false;
        *changed = false;
        return true;
    }

    *value = glm::vec2(iter->second.u.vec2State.currentState.x, iter->second.u.vec2State.currentState.y);
    *valid = iter->second.u.vec2State.isActive;
    *changed = iter->second.u.vec2State.changedSinceLastSync;
    return true;
}

bool XrBuddy::GetActionPosition(const std::string& actionName, glm::vec3* value, bool* valid, bool* tracked) const
{
    // special case for "head_pose"
    if (actionName == "head_pose")
    {
        *value = glm::vec3(viewSpaceLocation.pose.position.x, viewSpaceLocation.pose.position.y, viewSpaceLocation.pose.position.z);
        *valid = viewSpaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT;
        *tracked = viewSpaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
        return true;
    }

    auto iter = actionMap.find(actionName);
    if (iter == actionMap.end() || iter->second.type != XR_ACTION_TYPE_POSE_INPUT)
    {
        return false;
    }

    if (state != XR_SESSION_STATE_FOCUSED || !iter->second.u.poseState.isActive)
    {
        *value = glm::vec3(0.0f);
        *valid = false;
        *tracked = false;
        return true;
    }

    *value = glm::vec3(iter->second.spaceLocation.pose.position.x, iter->second.spaceLocation.pose.position.y, iter->second.spaceLocation.pose.position.z);
    *valid = iter->second.spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT;
    *tracked = iter->second.spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    return true;
}

bool XrBuddy::GetActionOrientation(const std::string& actionName, glm::quat* value, bool* valid, bool* tracked) const
{
    // special case for "head_pose"
    if (actionName == "head_pose")
    {
        *value = glm::quat(viewSpaceLocation.pose.orientation.w, viewSpaceLocation.pose.orientation.x, viewSpaceLocation.pose.orientation.y, viewSpaceLocation.pose.orientation.z);
        *valid = viewSpaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        *tracked = viewSpaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
        return true;
    }

    auto iter = actionMap.find(actionName);
    if (iter == actionMap.end() || iter->second.type != XR_ACTION_TYPE_POSE_INPUT)
    {
        return false;
    }

    if (state != XR_SESSION_STATE_FOCUSED || !iter->second.u.poseState.isActive)
    {
        *value = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        *valid = false;
        *tracked = false;
        return true;
    }

    *value = glm::quat(iter->second.spaceLocation.pose.orientation.w, iter->second.spaceLocation.pose.orientation.x, iter->second.spaceLocation.pose.orientation.y, iter->second.spaceLocation.pose.orientation.z);
    *valid = iter->second.spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    *tracked = iter->second.spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
    return true;
}

bool XrBuddy::GetActionLinearVelocity(const std::string& actionName, glm::vec3* value, bool* valid) const
{
    // special case for "head_pose"
    if (actionName == "head_pose")
    {
        *value = glm::vec3(viewSpaceVelocity.linearVelocity.x, viewSpaceVelocity.linearVelocity.y, viewSpaceVelocity.linearVelocity.z);
        *valid = viewSpaceVelocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT;
        return true;
    }

    auto iter = actionMap.find(actionName);
    if (iter == actionMap.end() || iter->second.type != XR_ACTION_TYPE_POSE_INPUT)
    {
        return false;
    }

    if (state != XR_SESSION_STATE_FOCUSED || !iter->second.u.poseState.isActive)
    {
        *value = glm::vec3(0.0f);
        *valid = false;
        return true;
    }

    *value = glm::vec3(iter->second.spaceVelocity.linearVelocity.x, iter->second.spaceVelocity.linearVelocity.y, iter->second.spaceVelocity.linearVelocity.z);
    *valid = iter->second.spaceVelocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT;
    return true;
}

bool XrBuddy::GetActionAngularVelocity(const std::string& actionName, glm::vec3* value, bool* valid) const
{

    // special case for "head_pose"
    if (actionName == "head_pose")
    {
        *value = glm::vec3(viewSpaceVelocity.angularVelocity.x, viewSpaceVelocity.angularVelocity.y, viewSpaceVelocity.angularVelocity.z);
        *valid = viewSpaceVelocity.velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT;
        return true;
    }

    auto iter = actionMap.find(actionName);
    if (iter == actionMap.end() || iter->second.type != XR_ACTION_TYPE_POSE_INPUT)
    {
        return false;
    }

    if (state != XR_SESSION_STATE_FOCUSED || !iter->second.u.poseState.isActive)
    {
        *value = glm::vec3(0.0f);
        *valid = false;
        return true;
    }

    *value = glm::vec3(iter->second.spaceVelocity.angularVelocity.x, iter->second.spaceVelocity.angularVelocity.y, iter->second.spaceVelocity.angularVelocity.z);
    *valid = iter->second.spaceVelocity.velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT;
    return true;
}

uint32_t XrBuddy::GetColorTexture() const
{
    return prevLastColorTexture;
}

void XrBuddy::CycleColorSpace()
{
    static int i = 0;
    Log::D("SETTING COLOR SPACE -> %d\n", i);
    SetColorSpace(instance, session, (XrColorSpaceFB)i);
    i = (i + 1) % (XR_COLOR_SPACE_ADOBE_RGB_FB + 1);
}

bool XrBuddy::LocateSpaces(XrTime predictedDisplayTime)
{
    ZoneScoped;

    viewSpaceLocation = {};
    viewSpaceVelocity = {};
    viewSpaceLocation.type = XR_TYPE_SPACE_LOCATION;
    viewSpaceLocation.next = &viewSpaceVelocity;
    viewSpaceVelocity.type = XR_TYPE_SPACE_VELOCITY;
    viewSpaceVelocity.next = NULL;

    XrResult result;
    if (state == XR_SESSION_STATE_FOCUSED)
    {
        for (auto& iter : actionMap)
        {
            if (iter.second.type == XR_ACTION_TYPE_POSE_INPUT && iter.second.u.poseState.isActive)
            {
                // link location -> velocity
                iter.second.spaceLocation.next = &iter.second.spaceVelocity;
                result = xrLocateSpace(iter.second.space, stageSpace, predictedDisplayTime, &iter.second.spaceLocation);
                if (!CheckResult(instance, result, "xrLocateSpace"))
                {
                    return false;
                }
            }
        }

        result = xrLocateSpace(viewSpace, stageSpace, predictedDisplayTime, &viewSpaceLocation);
        if (!CheckResult(instance, result, "xrLocateSpace"))
        {
            return false;
        }
    }
    return true;
}

bool XrBuddy::SessionReady() const
{
    return sessionReady;
}

bool XrBuddy::RenderFrame()
{
    ZoneScoped;

    if (state == XR_SESSION_STATE_READY ||
        state == XR_SESSION_STATE_SYNCHRONIZED ||
        state == XR_SESSION_STATE_VISIBLE ||
        state == XR_SESSION_STATE_FOCUSED)
    {
        XrFrameState fs = {};
        fs.type = XR_TYPE_FRAME_STATE;
        fs.next = NULL;

        XrFrameWaitInfo fwi = {};
        fwi.type = XR_TYPE_FRAME_WAIT_INFO;
        fwi.next = NULL;

        XrResult result;
        {
            ZoneScopedNC("xrWaitFrame", tracy::Color::Red4);
            result = xrWaitFrame(session, &fwi, &fs);
            if (!CheckResult(instance, result, "xrWaitFrame"))
            {
                return false;
            }
        }
        lastPredictedDisplayTime = fs.predictedDisplayTime;

        XrFrameBeginInfo fbi = {};
        fbi.type = XR_TYPE_FRAME_BEGIN_INFO;
        fbi.next = NULL;
        {
            ZoneScopedNC("xrBeginFrame", tracy::Color::DarkGreen);
            result = xrBeginFrame(session, &fbi);
            if (!CheckResult(instance, result, "xrBeginFrame"))
            {
                return false;
            }
        }

        std::vector<XrCompositionLayerBaseHeader*> layers;
        XrCompositionLayerProjection layer = {};
        layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
        layer.next = NULL;

        std::vector<XrCompositionLayerProjectionView> projectionLayerViews;
        if (fs.shouldRender == XR_TRUE)
        {
            if (!LocateSpaces(fs.predictedDisplayTime))
            {
                return false;
            }
            if (RenderLayer(fs.predictedDisplayTime, projectionLayerViews, layer))
            {
                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));
            }
        }

        XrFrameEndInfo fei = {};
        fei.type = XR_TYPE_FRAME_END_INFO;
        fei.next = NULL;
        fei.displayTime = fs.predictedDisplayTime;
        fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        fei.layerCount = (uint32_t)layers.size();
        fei.layers = layers.data();

        {
            ZoneScopedNC("xrEndFrame", tracy::Color::Red4);

            result = xrEndFrame(session, &fei);
            if (!CheckResult(instance, result, "xrEndFrame"))
            {
                return false;
            }
        }
    }

    return true;
}

bool XrBuddy::Shutdown()
{
    for (auto& swapchain : swapchains)
    {
        xrDestroySwapchain(swapchain.handle);
    }
    swapchains.clear();
    swapchainImages.clear();

    for (auto& iter : colorToDepthMap)
    {
        glDeleteTextures(1, &iter.second);
    }
    colorToDepthMap.clear();

    if (frameBuffer)
    {
        glDeleteFramebuffers(1, &frameBuffer);
        frameBuffer = 0;
    }

    if (stageSpace != XR_NULL_HANDLE)
    {
        xrDestroySpace(stageSpace);
        stageSpace = XR_NULL_HANDLE;
    }

    if (viewSpace != XR_NULL_HANDLE)
    {
        xrDestroySpace(viewSpace);
        viewSpace = XR_NULL_HANDLE;
    }

    for (auto iter : actionMap)
    {
        if (iter.second.space != XR_NULL_HANDLE)
        {
            xrDestroySpace(iter.second.space);
        }
        if (iter.second.action != XR_NULL_HANDLE)
        {
            xrDestroyAction(iter.second.action);
        }
    }
    actionMap.clear();


    if (actionSet != XR_NULL_HANDLE)
    {
        xrDestroyActionSet(actionSet);
        actionSet = XR_NULL_HANDLE;
    }

    if (session != XR_NULL_HANDLE)
    {
        xrDestroySession(session);
        session = XR_NULL_HANDLE;
    }

    // on oculus runtime destroying the instance causes a crash on shutdown... so don't...
    /*
    if (instance != XR_NULL_HANDLE)
    {
        xrDestroyInstance(instance);
        instance = XR_NULL_HANDLE;
    }
    */

    return true;
}

bool XrBuddy::RenderLayer(XrTime predictedDisplayTime,
                          std::vector<XrCompositionLayerProjectionView>& projectionLayerViews,
                          XrCompositionLayerProjection& layer)
{
    ZoneScoped;

    XrViewState viewState = {};
    viewState.type = XR_TYPE_VIEW_STATE;
    viewState.next = NULL;

    uint32_t viewCapacityInput = (uint32_t)viewConfigs.size();
    uint32_t viewCountOutput;

    std::vector<XrView> views(viewConfigs.size());
    for (size_t i = 0; i < viewConfigs.size(); i++)
    {
        views[i] = {};
        views[i].type = XR_TYPE_VIEW;
        views[i].next = NULL;
    }

    XrViewLocateInfo vli = {};
    vli.type = XR_TYPE_VIEW_LOCATE_INFO;
    vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    vli.displayTime = predictedDisplayTime;
    vli.space = stageSpace;
    vli.next = NULL;
    XrResult result = xrLocateViews(session, &vli, &viewState, viewCapacityInput, &viewCountOutput, views.data());
    if (!CheckResult(instance, result, "xrLocateViews"))
    {
        return false;
    }

    if (XR_UNQUALIFIED_SUCCESS(result))
    {
        assert(viewCountOutput == viewCapacityInput);
        assert(viewCountOutput == viewConfigs.size());
        assert(viewCountOutput == swapchains.size());

        projectionLayerViews.resize(viewCountOutput);

        // Render view to the appropriate part of the swapchain image.
        for (uint32_t i = 0; i < viewCountOutput; i++)
        {
            // Each view has a separate swapchain which is acquired, rendered to, and released.
            const SwapchainInfo& viewSwapchain = swapchains[i];

            XrSwapchainImageAcquireInfo ai = {};
            ai.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
            ai.next = NULL;

            uint32_t swapchainImageIndex;
            result = xrAcquireSwapchainImage(viewSwapchain.handle, &ai, &swapchainImageIndex);
            if (!CheckResult(instance, result, "xrAcquireSwapchainImage"))
            {
                return false;
            }

            XrSwapchainImageWaitInfo wi = {};
            wi.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
            wi.next = NULL;
            wi.timeout = XR_INFINITE_DURATION;
            result = xrWaitSwapchainImage(viewSwapchain.handle, &wi);
            if (!CheckResult(instance, result, "xrWaitSwapchainImage"))
            {
                return false;
            }

            projectionLayerViews[i] = {};
            projectionLayerViews[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
            projectionLayerViews[i].pose = views[i].pose;
            projectionLayerViews[i].fov = views[i].fov;
            projectionLayerViews[i].subImage.swapchain = viewSwapchain.handle;
            projectionLayerViews[i].subImage.imageRect.offset = {0, 0};
            projectionLayerViews[i].subImage.imageRect.extent = {viewSwapchain.width, viewSwapchain.height};

            const SwapchainImage& swapchainImage = swapchainImages[i][swapchainImageIndex];

            // find or create the depthTexture associated with this colorTexture
            const uint32_t colorTexture = swapchainImage.image;
            if (i == 0)
            {
                prevLastColorTexture = lastColorTexture;
                lastColorTexture = colorTexture;  // save for rendering onto desktop.
            }

            auto iter = colorToDepthMap.find(colorTexture);
            if (iter == colorToDepthMap.end())
            {
                const uint32_t depthTexture = CreateDepthTexture(colorTexture, viewSwapchain.width, viewSwapchain.height);
                iter = colorToDepthMap.insert(std::make_pair(colorTexture, depthTexture)).first;
            }

            RenderView(projectionLayerViews[i], frameBuffer, iter->first, iter->second, i);

            XrSwapchainImageReleaseInfo ri = {};
            ri.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
            ri.next = NULL;
            result = xrReleaseSwapchainImage(viewSwapchain.handle, &ri);
            if (!CheckResult(instance, result, "xrReleaseSwapchainImage"))
            {
                return false;
            }

            layer.space = stageSpace;
            layer.viewCount = (uint32_t)projectionLayerViews.size();
            layer.views = projectionLayerViews.data();
        }
    }

    return true;
}

void XrBuddy::RenderView(const XrCompositionLayerProjectionView& layerView, uint32_t frameBuffer,
                         uint32_t colorTexture, uint32_t depthTexture, int32_t viewNum)
{
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

    glViewport(static_cast<GLint>(layerView.subImage.imageRect.offset.x),
               static_cast<GLint>(layerView.subImage.imageRect.offset.y),
               static_cast<GLsizei>(layerView.subImage.imageRect.extent.width),
               static_cast<GLsizei>(layerView.subImage.imageRect.extent.height));

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    const float tanLeft = tanf(layerView.fov.angleLeft);
    const float tanRight = tanf(layerView.fov.angleRight);
    const float tanDown = tanf(layerView.fov.angleDown);
    const float tanUp = tanf(layerView.fov.angleUp);

    glm::mat4 projMat;
    CreateProjection(glm::value_ptr(projMat), GRAPHICS_OPENGL, tanLeft, tanRight, tanUp, tanDown, nearFar.x, nearFar.y);

    const auto& pose = layerView.pose;
    glm::quat eyeRot(pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z);
    glm::vec3 eyePos(pose.position.x, pose.position.y, pose.position.z);
    glm::mat4 eyeMat = MakeMat4(eyeRot, eyePos);
    glm::vec4 viewport(layerView.subImage.imageRect.offset.x, layerView.subImage.imageRect.offset.y,
                       layerView.subImage.imageRect.extent.width, layerView.subImage.imageRect.extent.height);
    renderCallback(projMat, eyeMat, viewport, nearFar, viewNum);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
