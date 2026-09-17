#include "input_host_win32.h"
#include "input_pulse_source.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

static void trace_sample(
    const RecompInputGamepad *pad, const XINPUT_STATE *host,
    DWORD host_status, DWORD foreground_process)
{
    static bool initialized, enabled, seen;
    static unsigned lines;
    static RecompInputGamepad previous;
    static WORD previous_host_buttons;
    static DWORD previous_status, previous_foreground;
    if (!initialized) {
        initialized = true;
        enabled = getenv("RECOMP_INPUT_TRACE") != NULL;
    }
    if (!enabled || lines >= 4096u) return;
    if (seen && previous.buttons == pad->buttons &&
        memcmp(previous.analog_buttons, pad->analog_buttons, 8u) == 0 &&
        previous_host_buttons == host->Gamepad.wButtons &&
        previous_status == host_status && previous_foreground == foreground_process) return;
    fprintf(stderr,
        "[DEBUG-r501-input] tick_ms=%llu status=%lu focused=%u host=%04x"
        " digital=%04x analog=%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x"
        " axes=%d,%d,%d,%d\n",
        (unsigned long long)GetTickCount64(), (unsigned long)host_status,
        foreground_process == GetCurrentProcessId(), (unsigned)host->Gamepad.wButtons,
        (unsigned)pad->buttons, pad->analog_buttons[0], pad->analog_buttons[1],
        pad->analog_buttons[2], pad->analog_buttons[3], pad->analog_buttons[4],
        pad->analog_buttons[5], pad->analog_buttons[6], pad->analog_buttons[7],
        pad->thumb_lx, pad->thumb_ly, pad->thumb_rx, pad->thumb_ry);
    previous = *pad;
    previous_host_buttons = host->Gamepad.wButtons;
    previous_status = host_status;
    previous_foreground = foreground_process;
    seen = true;
    ++lines;
}

bool recomp_input_host_sample(RecompInputGamepad *gamepad)
{
    XINPUT_STATE state = {0};

    if (gamepad == NULL) {
        return false;
    }
    memset(gamepad, 0, sizeof *gamepad);
    DWORD host_status = XInputGetState(0u, &state);
    if (host_status == ERROR_SUCCESS) {
        WORD digital = XINPUT_GAMEPAD_DPAD_UP |
            XINPUT_GAMEPAD_DPAD_DOWN |
            XINPUT_GAMEPAD_DPAD_LEFT |
            XINPUT_GAMEPAD_DPAD_RIGHT |
            XINPUT_GAMEPAD_START |
            XINPUT_GAMEPAD_BACK |
            XINPUT_GAMEPAD_LEFT_THUMB |
            XINPUT_GAMEPAD_RIGHT_THUMB;

        gamepad->buttons = state.Gamepad.wButtons & digital;
        gamepad->analog_buttons[RECOMP_INPUT_ANALOG_X] =
            (state.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0u ? 0xffu : 0u;
        gamepad->analog_buttons[RECOMP_INPUT_ANALOG_Y] =
            (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0u ? 0xffu : 0u;
        gamepad->analog_buttons[RECOMP_INPUT_ANALOG_A] =
            (state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0u ? 0xffu : 0u;
        gamepad->analog_buttons[RECOMP_INPUT_ANALOG_B] =
            (state.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0u ? 0xffu : 0u;
        gamepad->analog_buttons[RECOMP_INPUT_ANALOG_BLACK] =
            (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0u
            ? 0xffu
            : 0u;
        gamepad->analog_buttons[RECOMP_INPUT_ANALOG_WHITE] =
            (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0u
            ? 0xffu
            : 0u;
        gamepad->analog_buttons[RECOMP_INPUT_ANALOG_LTRIG] = state.Gamepad.bLeftTrigger;
        gamepad->analog_buttons[RECOMP_INPUT_ANALOG_RTRIG] = state.Gamepad.bRightTrigger;
        gamepad->thumb_lx = state.Gamepad.sThumbLX;
        gamepad->thumb_ly = state.Gamepad.sThumbLY;
        gamepad->thumb_rx = state.Gamepad.sThumbRX;
        gamepad->thumb_ry = state.Gamepad.sThumbRY;
    }
    DWORD foreground_process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foreground_process);
    if (foreground_process != GetCurrentProcessId()) {
        trace_sample(gamepad, &state, host_status, foreground_process);
        return true;
    }

    if (pressed(VK_UP)) gamepad->buttons |= XBOX_DPAD_UP;
    if (pressed(VK_DOWN)) gamepad->buttons |= XBOX_DPAD_DOWN;
    if (pressed(VK_LEFT)) gamepad->buttons |= XBOX_DPAD_LEFT;
    if (pressed(VK_RIGHT)) gamepad->buttons |= XBOX_DPAD_RIGHT;
    if (pressed(VK_RETURN)) gamepad->buttons |= XBOX_START;
    if (pressed(VK_BACK)) gamepad->buttons |= XBOX_BACK;

    if (pressed('A')) gamepad->analog_buttons[RECOMP_INPUT_ANALOG_X] = 0xffu;
    if (pressed('S')) gamepad->analog_buttons[RECOMP_INPUT_ANALOG_Y] = 0xffu;
    if (pressed('Z')) gamepad->analog_buttons[RECOMP_INPUT_ANALOG_A] = 0xffu;
    if (pressed('X')) gamepad->analog_buttons[RECOMP_INPUT_ANALOG_B] = 0xffu;
    if (pressed('Q')) gamepad->analog_buttons[RECOMP_INPUT_ANALOG_WHITE] = 0xffu;
    if (pressed('W')) gamepad->analog_buttons[RECOMP_INPUT_ANALOG_BLACK] = 0xffu;
    if (pressed('E')) gamepad->analog_buttons[RECOMP_INPUT_ANALOG_LTRIG] = 0xffu;
    if (pressed('R')) gamepad->analog_buttons[RECOMP_INPUT_ANALOG_RTRIG] = 0xffu;
    trace_sample(gamepad, &state, host_status, foreground_process);
    return true;
}
