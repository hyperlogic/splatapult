#include "joystick.h"

#include <assert.h>
#include <SDL.h>

#include "log.h"

SDL_Joystick* s_sdlJoy = 0;
Joystick s_joy;

void JOYSTICK_Init()
{
    // Only Xbox Controller is supported
    int numJoySticks = SDL_NumJoysticks();
    if (numJoySticks > 0)
    {
        s_sdlJoy = SDL_JoystickOpen(0);

        printf("Found joystick \"%s\"\n", SDL_JoystickName(0));
        printf("  numAxes = %d\n", SDL_JoystickNumAxes(s_sdlJoy));
        printf("  numButtons = %d\n", SDL_JoystickNumButtons(s_sdlJoy));
        printf("  numBalls = %d\n", SDL_JoystickNumBalls(s_sdlJoy));
        printf("  numHats = %d\n", SDL_JoystickNumHats(s_sdlJoy));
    }
    else
    {
        printf("Joystick not found. err = %d\n", numJoySticks);
    }
}

void JOYSTICK_Shutdown()
{
    if (s_sdlJoy)
    {
        SDL_JoystickClose(s_sdlJoy);
    }
}

void JOYSTICK_ClearFlags()
{
    s_joy.buttonPressFlags = 0;
    s_joy.buttonReleaseFlags = 0;
}

// TODO: do a real joystick config.
// maps event to axes
#ifdef DARWIN
static int s_axisMap[] = {Joystick::LeftStickX, Joystick::LeftStickY, Joystick::RightStickX, Joystick::RightStickY, Joystick::LeftTrigger, Joystick::RightTrigger};
#else
//static int s_axisMap[] = {Joystick::LeftStickX, Joystick::LeftStickY, Joystick::LeftTrigger, Joystick::RightStickY, Joystick::RightStickX, Joystick::RightTrigger};
static int s_axisMap[] = {Joystick::LeftStickX, Joystick::LeftStickY, Joystick::RightStickX, Joystick::RightStickY, Joystick::LeftTrigger, Joystick::RightTrigger};
#endif

void JOYSTICK_UpdateMotion(const SDL_JoyAxisEvent* event)
{
    if (event->which != 0)
    {
        return;
    }

    const float kDeadSpot = 0.2f;
    int i = s_axisMap[event->axis];
    switch (i)
    {
        case Joystick::LeftStickX:
        case Joystick::RightStickX:
            s_joy.axes[i] = event->value / 32768.0f;
            if (fabs(s_joy.axes[i]) < kDeadSpot)
                s_joy.axes[i] = 0.0f;
            break;

        // flip y axis, not sure if this a driver thing or an SDL thing, but I want my +y to mean up.
        case Joystick::LeftStickY:
        case Joystick::RightStickY:
            s_joy.axes[i] = event->value / -32768.0f;
            if (fabs(s_joy.axes[i]) < kDeadSpot)
                s_joy.axes[i] = 0.0f;
            break;

        // I want to map the triggers from 0 to 1
        case Joystick::LeftTrigger:
        case Joystick::RightTrigger:
            s_joy.axes[i] = ((event->value / 32768.0f) * 0.5f) + 0.5f;
            if (s_joy.axes[i] < kDeadSpot)
                s_joy.axes[i] = 0.0f;
            break;
    }
}

unsigned int UpdateButtonFlag(unsigned int orig, unsigned int mask, bool newState)
{
    if (newState)
    {
        return orig | mask;     // set flag
    }
    else
    {
        return orig & ~mask;    // clear flag
    }
}

// TODO: do a real joystick config.
#ifdef DARWIN
static int s_buttonMap[] = {Joystick::DPadDown, Joystick::DPadUp, Joystick::DPadLeft, Joystick::DPadRight,
                            Joystick::Start, Joystick::Back, Joystick::LeftStick, Joystick::RightStick,
                            Joystick::LeftBumper, Joystick::RightBumper, Joystick::Xbox, Joystick::A,
                            Joystick::B, Joystick::X, Joystick::Y};
#else
static int s_buttonMap[] = {Joystick::A, Joystick::B, Joystick::X, Joystick::Y,
                            Joystick::LeftBumper, Joystick::RightBumper, Joystick::Back, Joystick::Start,
                            Joystick::LeftStick, Joystick::RightStick};
#endif

void JOYSTICK_UpdateButton(const SDL_JoyButtonEvent* event)
{
    if (event->which != 0)
    {
        return;
    }

    int i = s_buttonMap[event->button];
    s_joy.buttonStateFlags = UpdateButtonFlag(s_joy.buttonStateFlags, 1 << i, event->state);
    if (event->state)
    {
        s_joy.buttonPressFlags = UpdateButtonFlag(s_joy.buttonPressFlags, 1 << i, event->state);
    }
    else
    {
        s_joy.buttonReleaseFlags = UpdateButtonFlag(s_joy.buttonReleaseFlags, 1 << i, event->state);
    }
}

Joystick* JOYSTICK_GetJoystick()
{
    return &s_joy;
}
