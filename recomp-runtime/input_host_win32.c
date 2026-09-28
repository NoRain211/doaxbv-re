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

static XINPUT_VIBRATION sent_vibration[XUSER_MAX_COUNT];

void recomp_input_host_set_vibration(
    uint32_t port,
    uint16_t left_motor,
    uint16_t right_motor)
{
    XINPUT_VIBRATION vibration = {left_motor, right_motor};
    DWORD status;

    /* The game resends every port's motors each frame; forward changes. */
    if (port >= XUSER_MAX_COUNT ||
        (sent_vibration[port].wLeftMotorSpeed == left_motor &&
         sent_vibration[port].wRightMotorSpeed == right_motor)) {
        return;
    }
    status = XInputSetState(port, &vibration);
    if (status == ERROR_SUCCESS) {
        sent_vibration[port] = vibration;
    }
    if (getenv("RECOMP_INPUT_TRACE") != NULL) {
        fprintf(stderr,
            "recomp input: vibration port=%u left=%u right=%u status=%lu\n",
            (unsigned)port, (unsigned)left_motor, (unsigned)right_motor,
            (unsigned long)status);
    }
}

void recomp_input_host_stop_vibration(void)
{
    for (uint32_t port = 0u; port < XUSER_MAX_COUNT; ++port) {
        recomp_input_host_set_vibration(port, 0u, 0u);
    }
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

bool recomp_input_host_sample(uint32_t port, RecompInputGamepad *gamepad)
{
    static ULONGLONG retry_empty_at[XUSER_MAX_COUNT];
    XINPUT_STATE state = {0};
    DWORD host_status = ERROR_DEVICE_NOT_CONNECTED;
    ULONGLONG now = GetTickCount64();

    if (gamepad == NULL || port >= XUSER_MAX_COUNT) {
        return false;
    }
    memset(gamepad, 0, sizeof *gamepad);
    /* Probing an empty slot is slow, so recheck one every two seconds. */
    if (now >= retry_empty_at[port]) {
        host_status = XInputGetState(port, &state);
        retry_empty_at[port] =
            host_status == ERROR_SUCCESS ? 0u : now + 2000u;
    }
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
    } else {
        /* A replugged pad starts with its motors off. */
        memset(&sent_vibration[port], 0, sizeof sent_vibration[port]);
    }
    if (port != 0u) {
        /* The keyboard drives port 0 only. */
        return host_status == ERROR_SUCCESS;
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
