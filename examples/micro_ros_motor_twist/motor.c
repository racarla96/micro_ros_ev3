#include "motor.h"

#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TACHO_MOTOR_CLASS_DIR "/sys/class/tacho-motor"

static bool read_attr(const char * base_path, const char * attr, char * out, size_t out_len)
{
    char path[EV3_MOTOR_PATH_MAX + 32];
    snprintf(path, sizeof(path), "%s/%s", base_path, attr);

    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;

    ssize_t n = read(fd, out, out_len - 1);
    close(fd);
    if (n <= 0) return false;

    out[n] = '\0';
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) out[--n] = '\0';
    return true;
}

static bool write_attr(const char * base_path, const char * attr, const char * value)
{
    char path[EV3_MOTOR_PATH_MAX + 32];
    snprintf(path, sizeof(path), "%s/%s", base_path, attr);

    int fd = open(path, O_WRONLY);
    if (fd < 0) return false;

    ssize_t len = (ssize_t)strlen(value);
    bool ok = write(fd, value, len) == len;
    close(fd);
    return ok;
}

/* Motor ports don't map to a fixed motorN directory, so find the one whose
 * "address" attribute matches the requested port (e.g. "outB"). */
bool ev3_motor_open(const char * port, ev3_motor_t * motor)
{
    DIR * dir = opendir(TACHO_MOTOR_CLASS_DIR);
    if (dir == NULL) return false;

    bool found = false;
    struct dirent * entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char candidate[EV3_MOTOR_PATH_MAX];
        snprintf(candidate, sizeof(candidate), "%s/%s", TACHO_MOTOR_CLASS_DIR, entry->d_name);

        char address[16];
        if (read_attr(candidate, "address", address, sizeof(address)) &&
            strcmp(address, port) == 0) {
            strncpy(motor->base_path, candidate, sizeof(motor->base_path) - 1);
            found = true;
            break;
        }
    }
    closedir(dir);

    if (!found) return false;

    char count_per_rot[16];
    motor->count_per_rot = read_attr(motor->base_path, "count_per_rot", count_per_rot, sizeof(count_per_rot))
                                ? atoi(count_per_rot) : 360;

    return write_attr(motor->base_path, "command", "run-direct");
}

int ev3_motor_get_speed(const ev3_motor_t * motor)
{
    char value[16];
    return read_attr(motor->base_path, "speed", value, sizeof(value)) ? atoi(value) : 0;
}

void ev3_motor_set_duty_cycle(const ev3_motor_t * motor, int percent)
{
    if (percent > 100) percent = 100;
    if (percent < -100) percent = -100;

    char value[8];
    snprintf(value, sizeof(value), "%d", percent);
    write_attr(motor->base_path, "duty_cycle_sp", value);
}

void ev3_motor_stop(const ev3_motor_t * motor)
{
    write_attr(motor->base_path, "command", "stop");
}
