#ifndef EV3_CONFIG_H
#define EV3_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EV3_CONFIG_MAX_ENTRIES 16
#define EV3_CONFIG_KEY_MAX     32
#define EV3_CONFIG_VALUE_MAX   128

typedef struct {
    char keys[EV3_CONFIG_MAX_ENTRIES][EV3_CONFIG_KEY_MAX];
    char values[EV3_CONFIG_MAX_ENTRIES][EV3_CONFIG_VALUE_MAX];
    int  count;
} ev3_config_t;

/*
 * Loads "key=value" entries into out, in increasing priority:
 *   1. <binary_name>.cfg next to the running binary (not the current
 *      working directory -- this also works launched from the EV3 screen)
 *   2. command-line arguments, each "key=value" (e.g. agent_ip=192.168.1.50)
 *
 * known_keys is a NULL-terminated array used only to print usage (via -h/
 * --help, which exits immediately); each example defines its own keys.
 * Use ev3_config_get_string()/ev3_config_get_uint16() to read values back,
 * falling back to a caller-supplied default for anything not set.
 */
void ev3_config_load(int argc, char ** argv, const char * const * known_keys, ev3_config_t * out);

const char * ev3_config_get_string(const ev3_config_t * cfg, const char * key, const char * default_value);
uint16_t ev3_config_get_uint16(const ev3_config_t * cfg, const char * key, uint16_t default_value);

#ifdef __cplusplus
}
#endif

#endif
