#include "config.h"

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Overwrites an existing key if present, otherwise appends. Silently drops
 * malformed lines or entries past EV3_CONFIG_MAX_ENTRIES. */
static void apply_line(ev3_config_t * cfg, const char * line)
{
    const char * eq = strchr(line, '=');
    if (eq == NULL) return;

    size_t key_len = (size_t)(eq - line);
    if (key_len == 0 || key_len >= EV3_CONFIG_KEY_MAX) return;

    char key[EV3_CONFIG_KEY_MAX];
    memcpy(key, line, key_len);
    key[key_len] = '\0';
    const char * value = eq + 1;

    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->keys[i], key) == 0) {
            strncpy(cfg->values[i], value, EV3_CONFIG_VALUE_MAX - 1);
            cfg->values[i][EV3_CONFIG_VALUE_MAX - 1] = '\0';
            return;
        }
    }

    if (cfg->count >= EV3_CONFIG_MAX_ENTRIES) return;

    strncpy(cfg->keys[cfg->count], key, EV3_CONFIG_KEY_MAX - 1);
    cfg->keys[cfg->count][EV3_CONFIG_KEY_MAX - 1] = '\0';
    strncpy(cfg->values[cfg->count], value, EV3_CONFIG_VALUE_MAX - 1);
    cfg->values[cfg->count][EV3_CONFIG_VALUE_MAX - 1] = '\0';
    cfg->count++;
}

/* <binary_name>.cfg lives next to the running binary, not the current
 * working directory, so it still works when launched from the EV3 screen. */
static void load_config_file(ev3_config_t * cfg)
{
    char exe_path[512];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len <= 0) return;
    exe_path[len] = '\0';

    /* dirname()/basename() may both return pointers into the same static
     * buffer depending on the libc, so give each its own copy. */
    char dir_buf[512];
    char base_buf[512];
    strncpy(dir_buf, exe_path, sizeof(dir_buf) - 1);
    dir_buf[sizeof(dir_buf) - 1] = '\0';
    strncpy(base_buf, exe_path, sizeof(base_buf) - 1);
    base_buf[sizeof(base_buf) - 1] = '\0';

    char config_path[600];
    snprintf(config_path, sizeof(config_path), "%s/%s.cfg", dirname(dir_buf), basename(base_buf));

    FILE * f = fopen(config_path, "r");
    if (f == NULL) return;

    char line[256];
    while (fgets(line, sizeof(line), f) != NULL) {
        char * newline = strpbrk(line, "\r\n");
        if (newline) *newline = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;

        apply_line(cfg, line);
    }

    fclose(f);
}

static void print_usage(const char * prog, const char * const * known_keys)
{
    fprintf(stderr, "Usage: %s [key=value ...]\n\nRecognized keys:\n", prog);
    for (int i = 0; known_keys != NULL && known_keys[i] != NULL; i++) {
        fprintf(stderr, "  %s\n", known_keys[i]);
    }
    fprintf(stderr,
        "\nValues can also be set in <binary_name>.cfg next to this binary, one\n"
        "key=value per line. Priority: command-line arguments > .cfg file > \n"
        "built-in defaults.\n");
}

void ev3_config_load(int argc, char ** argv, const char * const * known_keys, ev3_config_t * out)
{
    out->count = 0;

    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_usage(argv[0], known_keys);
        exit(0);
    }

    load_config_file(out);

    for (int i = 1; i < argc; i++) {
        apply_line(out, argv[i]);
    }
}

const char * ev3_config_get_string(const ev3_config_t * cfg, const char * key, const char * default_value)
{
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->keys[i], key) == 0) return cfg->values[i];
    }
    return default_value;
}

uint16_t ev3_config_get_uint16(const ev3_config_t * cfg, const char * key, uint16_t default_value)
{
    const char * v = ev3_config_get_string(cfg, key, NULL);
    return v ? (uint16_t)atoi(v) : default_value;
}
