/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "inputbuddy.h"

#include <SDL2/SDL.h>

#include "log.h"

InputBuddy::InputBuddy()
{
    int numJoySticks = SDL_NumJoysticks();
    if (numJoySticks > 0)
    {
        SDL_Joystick* joystick = SDL_JoystickOpen(0);
        Log::I("Found joystick \"%s\"\n", SDL_JoystickName(joystick));
    }
    else
    {
        Log::I("No joystick found\n");
    }
}

void InputBuddy::ProcessEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_QUIT:
        quitCallback();
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        {
            SDL_Keycode keycode = event.key.keysym.sym;
            uint16_t mod = event.key.keysym.mod;
            bool down = (event.key.type == SDL_KEYDOWN);
            auto iter = keyCallbackMap.find(keycode);
            if (iter != keyCallbackMap.end() && !event.key.repeat)
            {
                iter->second(down, mod);
            }
        }
        break;
    case SDL_JOYAXISMOTION:
        UpdateJoypadAxis(event.jaxis);
        break;
    case SDL_JOYHATMOTION:
        UpdateJoypadHat(event.jhat);
        break;
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        UpdateJoypadButton(event.jbutton);
        break;

    case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_RESIZED)
        {
            resizeCallback(event.window.data1, event.window.data2);
        }
        break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        if (event.button.clicks == 1)
        {
            mouseButtonCallback(event.button.button, event.button.state == SDL_PRESSED,
                                glm::ivec2(event.button.x, event.button.y));
        }
        break;
    case SDL_MOUSEMOTION:
        mouseMotionCallback(glm::ivec2(event.motion.x, event.motion.y), glm::ivec2(event.motion.xrel, event.motion.yrel));
        break;
    }
}

void InputBuddy::OnKey(Keycode key, const KeyCallback& cb)
{
    keyCallbackMap.insert(std::pair<Keycode, KeyCallback>(key, cb));
}

void InputBuddy::OnQuit(const VoidCallback& cb)
{
    quitCallback = cb;
}

void InputBuddy::OnResize(const ResizeCallback& cb)
{
    resizeCallback = cb;
}

void InputBuddy::OnMouseButton(const MouseButtonCallback& cb)
{
    mouseButtonCallback = cb;
}

void InputBuddy::OnMouseMotion(const MouseMotionCallback& cb)
{
    mouseMotionCallback = cb;
}

void InputBuddy::SetRelativeMouseMode(bool val)
{
    SDL_SetRelativeMouseMode(val ? SDL_TRUE : SDL_FALSE);
}

const uint8_t LEFT_STICK_X_AXIS = 0;
const uint8_t LEFT_STICK_Y_AXIS = 1;
const uint8_t RIGHT_STICK_X_AXIS = 2;
const uint8_t RIGHT_STICK_Y_AXIS = 3;
const uint8_t LEFT_TRIGGER_AXIS = 4;
const uint8_t RIGHT_TRIGGER_AXIS = 5;

static float Deadspot(float v)
{
    const float DEADSPOT = 0.15f;
    return fabs(v) > DEADSPOT ? v : 0.0f;
}

void InputBuddy::UpdateJoypadAxis(const SDL_JoyAxisEvent& event)
{
    // only support one joypad
    if (event.which != 0)
    {
        return;
    }

    const float AXIS_MAX = (float)SDL_JOYSTICK_AXIS_MAX;
    switch (event.axis)
    {
    case LEFT_STICK_X_AXIS:
        joypad.leftStick.x = Deadspot(event.value / AXIS_MAX);
        break;
    case LEFT_STICK_Y_AXIS:
        joypad.leftStick.y = Deadspot(-event.value / AXIS_MAX);
        break;
    case RIGHT_STICK_X_AXIS:
        joypad.rightStick.x = Deadspot(event.value / AXIS_MAX);
        break;
    case RIGHT_STICK_Y_AXIS:
        joypad.rightStick.y = Deadspot(-event.value / AXIS_MAX);
        break;
    case LEFT_TRIGGER_AXIS:
        joypad.leftTrigger = ((event.value / AXIS_MAX) * 0.5f) + 0.5f;  // (0, 1)
        break;
    case RIGHT_TRIGGER_AXIS:
        joypad.rightTrigger = ((event.value / AXIS_MAX) * 0.5f) + 0.5f;  // (0, 1)
        break;
    }
}

const uint8_t DPAD_HAT = 0;

void InputBuddy::UpdateJoypadHat(const SDL_JoyHatEvent& event)
{
    // only support one joypad
    if (event.which != 0)
    {
        return;
    }

    switch (event.hat)
    {
    case DPAD_HAT:
        joypad.up = (event.value & SDL_HAT_UP) ? true : false;
        joypad.right = (event.value & SDL_HAT_RIGHT) ? true : false;
        joypad.down = (event.value & SDL_HAT_DOWN) ? true : false;
        joypad.left = (event.value & SDL_HAT_LEFT) ? true : false;
        break;
    }
}

const uint8_t A_BUTTON = 0;
const uint8_t B_BUTTON = 1;
const uint8_t X_BUTTON = 2;
const uint8_t Y_BUTTON = 3;
const uint8_t LEFT_BUMPER_BUTTON = 4;
const uint8_t RIGHT_BUMPER_BUTTON = 5;
const uint8_t MENU_BUTTON = 6;
const uint8_t VIEW_BUTTON = 7;
const uint8_t LEFT_STICK_BUTTON = 8;
const uint8_t RIGHT_STICK_BUTTON = 9;

void InputBuddy::UpdateJoypadButton(const SDL_JoyButtonEvent& event)
{
    // only support one joypad
    if (event.which != 0)
    {
        return;
    }

    switch (event.button)
    {
    case A_BUTTON:
        joypad.a = event.state ? true : false;
        break;
    case B_BUTTON:
        joypad.b = event.state ? true : false;
        break;
    case X_BUTTON:
        joypad.x = event.state ? true : false;
        break;
    case Y_BUTTON:
        joypad.y = event.state ? true : false;
        break;
    case LEFT_BUMPER_BUTTON:
        joypad.lb = event.state ? true : false;
        break;
    case RIGHT_BUMPER_BUTTON:
        joypad.rb = event.state ? true : false;
        break;
    case MENU_BUTTON:
        joypad.menu = event.state ? true : false;
        break;
    case VIEW_BUTTON:
        joypad.view = event.state ? true : false;
        break;
    case LEFT_STICK_BUTTON:
        joypad.ls = event.state ? true : false;
        break;
    case RIGHT_STICK_BUTTON:
        joypad.rs = event.state ? true : false;
        break;
    }
}
