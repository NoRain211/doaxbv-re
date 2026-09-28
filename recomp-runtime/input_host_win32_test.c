#include "input_host_win32.h"
#include "input_pulse_source.h"

#include <SDL3/SDL.h>

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); return 1; } } while (0)

static uint32_t rumbled[2];

static bool SDLCALL record_rumble(void *userdata, Uint16 low, Uint16 high)
{
    rumbled[(uintptr_t)userdata] = (uint32_t)low << 16 | high;
    return true;
}

static SDL_Joystick *attach(uintptr_t index, SDL_JoystickID *id)
{
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.userdata = (void *)index;
    desc.Rumble = record_rumble;
    *id = SDL_AttachVirtualJoystick(&desc);
    return *id != 0 ? SDL_OpenJoystick(*id) : NULL;
}

int main(void)
{
    /* Only the virtual gamepads below may be visible. */
    const char *drivers[] = {SDL_HINT_JOYSTICK_HIDAPI, SDL_HINT_JOYSTICK_RAWINPUT,
        SDL_HINT_XINPUT_ENABLED, SDL_HINT_JOYSTICK_WGI, SDL_HINT_JOYSTICK_DIRECTINPUT,
        SDL_HINT_JOYSTICK_GAMEINPUT};
    for (size_t i = 0; i < sizeof drivers / sizeof drivers[0]; ++i) SDL_SetHint(drivers[i], "0");
    CHECK(SDL_Init(SDL_INIT_GAMEPAD));

    SDL_JoystickID first_id, second_id;
    SDL_Joystick *first = attach(0u, &first_id);
    CHECK(first != NULL);

    RecompInputGamepad pad;
    /* SDL discards the first jump of an axis that starts at an extreme (triggers). */
    CHECK(recomp_input_host_sample(0u, &pad));
    CHECK(!recomp_input_host_sample(1u, &pad));
    SDL_SetJoystickVirtualButton(first, SDL_GAMEPAD_BUTTON_SOUTH, true);
    SDL_SetJoystickVirtualButton(first, SDL_GAMEPAD_BUTTON_NORTH, true);
    SDL_SetJoystickVirtualButton(first, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, true);
    SDL_SetJoystickVirtualButton(first, SDL_GAMEPAD_BUTTON_START, true);
    SDL_SetJoystickVirtualButton(first, SDL_GAMEPAD_BUTTON_DPAD_LEFT, true);
    SDL_SetJoystickVirtualAxis(first, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MAX);
    SDL_SetJoystickVirtualAxis(first, SDL_GAMEPAD_AXIS_LEFTY, SDL_JOYSTICK_AXIS_MIN);
    SDL_SetJoystickVirtualAxis(first, SDL_GAMEPAD_AXIS_RIGHTX, 1000);

    CHECK(recomp_input_host_sample(0u, &pad));
    CHECK(pad.buttons == 0x0014u); /* START | DPAD_LEFT */
    CHECK(pad.analog_buttons[RECOMP_INPUT_ANALOG_A] == 0xffu);
    CHECK(pad.analog_buttons[RECOMP_INPUT_ANALOG_Y] == 0xffu);
    CHECK(pad.analog_buttons[RECOMP_INPUT_ANALOG_B] == 0u);
    CHECK(pad.analog_buttons[RECOMP_INPUT_ANALOG_X] == 0u);
    CHECK(pad.analog_buttons[RECOMP_INPUT_ANALOG_BLACK] == 0xffu);
    CHECK(pad.analog_buttons[RECOMP_INPUT_ANALOG_WHITE] == 0u);
    CHECK(pad.analog_buttons[RECOMP_INPUT_ANALOG_RTRIG] == 0xffu);
    CHECK(pad.analog_buttons[RECOMP_INPUT_ANALOG_LTRIG] == 0u);
    CHECK(pad.thumb_ly == 32767); /* stick up is positive, as in XInput */
    CHECK(pad.thumb_rx == 1000);
    CHECK(pad.thumb_ry == 0);

    /* A second pad takes port 1 and gets its own input and rumble. */
    SDL_Joystick *second = attach(1u, &second_id);
    CHECK(second != NULL);
    SDL_SetJoystickVirtualButton(second, SDL_GAMEPAD_BUTTON_BACK, true);
    CHECK(recomp_input_host_sample(1u, &pad));
    CHECK(recomp_input_host_sample(1u, &pad));
    CHECK(pad.buttons == 0x0020u);
    recomp_input_host_set_vibration(1u, 0x1234u, 0x5678u);
    CHECK(rumbled[1] == 0x12345678u && rumbled[0] == 0u);
    recomp_input_host_stop_vibration();
    CHECK(rumbled[1] == 0u);

    /* Removing port 0's pad leaves port 1 in place. */
    SDL_CloseJoystick(first);
    CHECK(SDL_DetachVirtualJoystick(first_id));
    CHECK(recomp_input_host_sample(0u, &pad));
    CHECK(pad.buttons == 0u && pad.analog_buttons[RECOMP_INPUT_ANALOG_A] == 0u);
    CHECK(recomp_input_host_sample(1u, &pad) && pad.buttons == 0x0020u);

    SDL_CloseJoystick(second);
    CHECK(SDL_DetachVirtualJoystick(second_id));
    CHECK(!recomp_input_host_sample(1u, &pad));
    puts("input host SDL mapping: ok");
    return 0;
}
