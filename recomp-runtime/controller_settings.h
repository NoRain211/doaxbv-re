#ifndef RECOMP_CONTROLLER_SETTINGS_H
#define RECOMP_CONTROLLER_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
bool recomp_controller_settings_load(const char *disc_root, uint8_t modes[4]);
bool recomp_controller_settings_save(const char *disc_root, const uint8_t modes[4]);
#ifdef __cplusplus
}
#endif
#endif
