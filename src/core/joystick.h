#ifndef __JOYSTICK_H__
#define __JOYSTICK_H__

#include <string.h>

struct SDL_JoyAxisEvent;
struct SDL_JoyButtonEvent;

// based on a Xbox360 Controller
struct Joystick
{
    Joystick() { memset(this, sizeof(Joystick), 0); }

    enum Axes
    {
        LeftStickX = 0,
        LeftStickY,
        RightStickX,
        RightStickY,
        LeftTrigger,
        RightTrigger,
        NumAxes
    };

    float axes[NumAxes];

    enum ButtonFlags
    {
        DPadDown = 0,
        DPadUp,
        DPadLeft,
        DPadRight,
        Start,
        Back,
        LeftStick,
        RightStick,
        LeftBumper,
        RightBumper,
        Xbox,
        A,
        B,
        X,
        Y,
        NumButtons
    };

    unsigned int buttonStateFlags;
    unsigned int buttonPressFlags;
    unsigned int buttonReleaseFlags;
};

void JOYSTICK_Init();
void JOYSTICK_Shutdown();

void JOYSTICK_ClearFlags();
void JOYSTICK_UpdateMotion(const SDL_JoyAxisEvent* event);
void JOYSTICK_UpdateButton(const SDL_JoyButtonEvent* event);

Joystick* JOYSTICK_GetJoystick();

#endif
