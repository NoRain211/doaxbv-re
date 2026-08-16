#include "input_model.h"

#include <stddef.h>
#include <string.h>

enum {
    INPUT_HANDLE_PREFIX = 0x58490000u,
};

static uint32_t port_bit(uint32_t port)
{
    return 1u << port;
}

static uint32_t handle_for_port(uint32_t port)
{
    return INPUT_HANDLE_PREFIX | (port + 1u);
}

static bool port_for_handle(uint32_t handle, uint32_t *port)
{
    uint32_t value = handle & 0xffffu;

    if ((handle & 0xffff0000u) != INPUT_HANDLE_PREFIX ||
        value == 0u || value > RECOMP_INPUT_PORT_COUNT) {
        return false;
    }
    *port = value - 1u;
    return true;
}

static bool gamepads_equal(
    const RecompInputGamepad *left,
    const RecompInputGamepad *right)
{
    return left->buttons == right->buttons &&
        memcmp(
            left->analog_buttons,
            right->analog_buttons,
            sizeof left->analog_buttons) == 0 &&
        left->thumb_lx == right->thumb_lx &&
        left->thumb_ly == right->thumb_ly &&
        left->thumb_rx == right->thumb_rx &&
        left->thumb_ry == right->thumb_ry;
}

void recomp_input_reset(RecompInputModel *model, uint32_t connected_mask)
{
    if (model == NULL) {
        return;
    }
    *model = (RecompInputModel){0};
    model->connected_mask = connected_mask &
        ((1u << RECOMP_INPUT_PORT_COUNT) - 1u);
}

void recomp_input_set_connected(
    RecompInputModel *model,
    uint32_t port,
    bool connected)
{
    if (model == NULL || port >= RECOMP_INPUT_PORT_COUNT) {
        return;
    }
    if (connected) {
        model->connected_mask |= port_bit(port);
    } else {
        model->connected_mask &= ~port_bit(port);
        model->ports[port].open = false;
    }
}

uint32_t recomp_input_get_devices(RecompInputModel *model)
{
    if (model == NULL) {
        return 0u;
    }
    model->reported_mask = model->connected_mask;
    return model->connected_mask;
}

bool recomp_input_get_device_changes(
    RecompInputModel *model,
    uint32_t *insertions,
    uint32_t *removals)
{
    uint32_t inserted;
    uint32_t removed;

    if (model == NULL || insertions == NULL || removals == NULL) {
        return false;
    }
    inserted = model->connected_mask & ~model->reported_mask;
    removed = model->reported_mask & ~model->connected_mask;
    model->reported_mask = model->connected_mask;
    *insertions = inserted;
    *removals = removed;
    return (inserted | removed) != 0u;
}

uint32_t recomp_input_open(RecompInputModel *model, uint32_t port)
{
    if (model == NULL || port >= RECOMP_INPUT_PORT_COUNT ||
        (model->connected_mask & port_bit(port)) == 0u) {
        return 0u;
    }
    model->ports[port].open = true;
    return handle_for_port(port);
}

void recomp_input_close(RecompInputModel *model, uint32_t handle)
{
    uint32_t port;

    if (model != NULL && port_for_handle(handle, &port)) {
        model->ports[port].open = false;
    }
}

bool recomp_input_set_gamepad(
    RecompInputModel *model,
    uint32_t handle,
    const RecompInputGamepad *gamepad)
{
    uint32_t port;
    RecompInputPort *input_port;

    if (model == NULL || gamepad == NULL ||
        !port_for_handle(handle, &port) ||
        (model->connected_mask & port_bit(port)) == 0u ||
        !model->ports[port].open) {
        return false;
    }
    input_port = &model->ports[port];
    if (!gamepads_equal(&input_port->gamepad, gamepad)) {
        input_port->gamepad = *gamepad;
        ++input_port->packet_number;
    }
    return true;
}

bool recomp_input_get_state(
    const RecompInputModel *model,
    uint32_t handle,
    uint32_t *packet_number,
    RecompInputGamepad *gamepad)
{
    uint32_t port;

    if (model == NULL || packet_number == NULL || gamepad == NULL ||
        !port_for_handle(handle, &port) ||
        (model->connected_mask & port_bit(port)) == 0u ||
        !model->ports[port].open) {
        return false;
    }
    *packet_number = model->ports[port].packet_number;
    *gamepad = model->ports[port].gamepad;
    return true;
}

bool recomp_input_set_feedback(
    RecompInputModel *model,
    uint32_t handle,
    uint16_t left_motor,
    uint16_t right_motor)
{
    uint32_t port;

    if (model == NULL || !port_for_handle(handle, &port) ||
        (model->connected_mask & port_bit(port)) == 0u ||
        !model->ports[port].open) {
        return false;
    }
    model->ports[port].left_motor = left_motor;
    model->ports[port].right_motor = right_motor;
    return true;
}
