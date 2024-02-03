/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <map>
#include <stdint.h>
#include <string.h>

union SDL_Event;
struct SDL_JoyAxisEvent;
struct SDL_JoyHatEvent;
struct SDL_JoyButtonEvent;

class InputBuddy
{
public:
    using Keycode = int32_t;

    InputBuddy();

    using VoidCallback = std::function<void()>;
    using KeyCallback = std::function<void(bool, uint16_t)>;
    using ResizeCallback = std::function<void(int, int)>;
    using MouseButtonCallback = std::function<void(uint8_t, bool, glm::ivec2)>; // button 1 = LEFT, 2 = MIDDLE, 3 = RIGHT
    using MouseMotionCallback = std::function<void(glm::ivec2, glm::ivec2)>;

    void ProcessEvent(const SDL_Event& event);

    void OnKey(Keycode key, const KeyCallback& cb);
    void OnQuit(const VoidCallback& cb);
    void OnResize(const ResizeCallback& cb);
    void OnMouseButton(const MouseButtonCallback& cb);
    void OnMouseMotion(const MouseMotionCallback& cb);

    void SetRelativeMouseMode(bool val);

    // based on an xbox controler
    struct Joypad
    {
        Joypad()
        {
            memset(this, 0, sizeof(Joypad));
        }

        glm::vec2 leftStick;
        glm::vec2 rightStick;
        float leftTrigger;
        float rightTrigger;
        bool down:1;
        bool up:1;
        bool left:1;
        bool right:1;
        bool view:1;
        bool menu:1;
        bool rs:1;
        bool ls:1;
        bool lb:1;
        bool rb:1;
        bool a:1;
        bool b:1;
        bool x:1;
        bool y:1;
    };

    const Joypad& GetJoypad() const { return joypad; }

protected:
    void UpdateJoypadAxis(const SDL_JoyAxisEvent& event);
    void UpdateJoypadHat(const SDL_JoyHatEvent& event);
    void UpdateJoypadButton(const SDL_JoyButtonEvent& event);

    std::map<Keycode, KeyCallback> keyCallbackMap;
    VoidCallback quitCallback;
    ResizeCallback resizeCallback;
    MouseButtonCallback mouseButtonCallback;
    MouseMotionCallback mouseMotionCallback;
    Joypad joypad;
};
