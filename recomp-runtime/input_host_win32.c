#include "input_host_win32.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>

#include <string.h>

enum {
    XBOX_DPAD_UP = 0x0001u,
    XBOX_DPAD_DOWN = 0x0002u,
    XBOX_DPAD_LEFT = 0x0004u,
    XBOX_DPAD_RIGHT = 0x0008u,
    XBOX_START = 0x0010u,
    XBOX_BACK = 0x0020u,
};

static bool pressed(int key)
{
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

bool recomp_input_host_sample(RecompInputGamepad *gamepad)
{
    XINPUT_STATE state = {0};

    if (gamepad == NULL) {
        return false;
    }
    memset(gamepad, 0, sizeof *gamepad);
    if (XInputGetState(0u, &state) == ERROR_SUCCESS) {
        WORD digital = XINPUT_GAMEPAD_DPAD_UP |
            XINPUT_GAMEPAD_DPAD_DOWN |
            XINPUT_GAMEPAD_DPAD_LEFT |
            XINPUT_GAMEPAD_DPAD_RIGHT |
            XINPUT_GAMEPAD_START |
            XINPUT_GAMEPAD_BACK |
            XINPUT_GAMEPAD_LEFT_THUMB |
            XINPUT_GAMEPAD_RIGHT_THUMB;

        gamepad->buttons = state.Gamepad.wButtons & digital;
        gamepad->analog_buttons[0] =
            (state.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0u ? 0xffu : 0u;
        gamepad->analog_buttons[1] =
            (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0u ? 0xffu : 0u;
        gamepad->analog_buttons[2] =
            (state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0u ? 0xffu : 0u;
        gamepad->analog_buttons[3] =
            (state.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0u ? 0xffu : 0u;
        gamepad->analog_buttons[4] =
            (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0u
            ? 0xffu
            : 0u;
        gamepad->analog_buttons[5] =
            (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0u
            ? 0xffu
            : 0u;
        gamepad->analog_buttons[6] = state.Gamepad.bLeftTrigger;
        gamepad->analog_buttons[7] = state.Gamepad.bRightTrigger;
        gamepad->thumb_lx = state.Gamepad.sThumbLX;
        gamepad->thumb_ly = state.Gamepad.sThumbLY;
        gamepad->thumb_rx = state.Gamepad.sThumbRX;
        gamepad->thumb_ry = state.Gamepad.sThumbRY;
    }
    if (pressed(VK_UP)) gamepad->buttons |= XBOX_DPAD_UP;
    if (pressed(VK_DOWN)) gamepad->buttons |= XBOX_DPAD_DOWN;
    if (pressed(VK_LEFT)) gamepad->buttons |= XBOX_DPAD_LEFT;
    if (pressed(VK_RIGHT)) gamepad->buttons |= XBOX_DPAD_RIGHT;
    if (pressed(VK_RETURN)) gamepad->buttons |= XBOX_START;
    if (pressed(VK_BACK)) gamepad->buttons |= XBOX_BACK;

    if (pressed('A')) gamepad->analog_buttons[0] = 0xffu; /* X */
    if (pressed('S')) gamepad->analog_buttons[1] = 0xffu; /* Y */
    if (pressed('Z')) gamepad->analog_buttons[2] = 0xffu; /* A */
    if (pressed('X')) gamepad->analog_buttons[3] = 0xffu; /* B */
    if (pressed('Q')) gamepad->analog_buttons[4] = 0xffu; /* White */
    if (pressed('W')) gamepad->analog_buttons[5] = 0xffu; /* Black */
    if (pressed('E')) gamepad->analog_buttons[6] = 0xffu; /* Left trigger */
    if (pressed('R')) gamepad->analog_buttons[7] = 0xffu; /* Right trigger */
    return true;
}
