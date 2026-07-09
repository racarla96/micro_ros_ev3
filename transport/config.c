#include "config.h"

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void apply_field(ev3_config_t * cfg, const char * key, const char * value)
{
    if (strcmp(key, "agent_ip") == 0) {
        strncpy(cfg->agent_ip, value, sizeof(cfg->agent_ip) - 1);
        cfg->agent_ip[sizeof(cfg->agent_ip) - 1] = '\0';
    } else if (strcmp(key, "agent_port") == 0) {
        cfg->agent_port = (uint16_t)atoi(value);
    } else if (strcmp(key, "topic") == 0) {
        strncpy(cfg->topic_name, value, sizeof(cfg->topic_name) - 1);
        cfg->topic_name[sizeof(cfg->topic_name) - 1] = '\0';
    }
}

/* config.txt lives next to the running binary, not the current working
 * directory, so it still works when launched from the EV3 screen. */
static void load_config_file(ev3_config_t * cfg)
{
    char exe_path[512];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len <= 0) return;
    exe_path[len] = '\0';

    char dir_buf[512];
    strncpy(dir_buf, exe_path, sizeof(dir_buf) - 1);
    dir_buf[sizeof(dir_buf) - 1] = '\0';

    char config_path[600];
    snprintf(config_path, sizeof(config_path), "%s/config.txt", dirname(dir_buf));

    FILE * f = fopen(config_path, "r");
    if (f == NULL) return;

    char line[256];
    while (fgets(line, sizeof(line), f) != NULL) {
        char * newline = strpbrk(line, "\r\n");
        if (newline) *newline = '\0';

        char * eq = strchr(line, '=');
        if (eq == NULL) continue;
        *eq = '\0';

        apply_field(cfg, line, eq + 1);
    }

    fclose(f);
}

static void print_usage(const char * prog, const char * default_ip,
                       uint16_t default_port, const char * default_topic)
{
    fprintf(stderr, "Usage: %s [agent_ip] [agent_port]", prog);
    if (default_topic[0] != '\0') {
        fprintf(stderr, " [topic]");
    }
    fprintf(stderr,
        "\n"
        "  agent_ip    micro-ROS agent IP address (default: %s)\n"
        "  agent_port  micro-ROS agent UDP port    (default: %u)\n",
        default_ip, (unsigned)default_port);
    if (default_topic[0] != '\0') {
        fprintf(stderr, "  topic       topic/service name        (default: %s)\n", default_topic);
    }
    fprintf(stderr,
        "\n"
        "Values can also be set in config.txt next to this binary\n"
        "(key=value lines: agent_ip, agent_port, topic). Priority:\n"
        "command-line arguments > config.txt > built-in defaults.\n");
}

void ev3_config_load(int argc, char ** argv,
                     const char * default_ip, uint16_t default_port,
                     const char * default_topic, ev3_config_t * out)
{
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_usage(argv[0], default_ip, default_port, default_topic);
        exit(0);
    }

    strncpy(out->agent_ip, default_ip, sizeof(out->agent_ip) - 1);
    out->agent_ip[sizeof(out->agent_ip) - 1] = '\0';
    out->agent_port = default_port;
    strncpy(out->topic_name, default_topic, sizeof(out->topic_name) - 1);
    out->topic_name[sizeof(out->topic_name) - 1] = '\0';

    load_config_file(out);

    if (argc > 1) {
        strncpy(out->agent_ip, argv[1], sizeof(out->agent_ip) - 1);
        out->agent_ip[sizeof(out->agent_ip) - 1] = '\0';
    }
    if (argc > 2) {
        out->agent_port = (uint16_t)atoi(argv[2]);
    }
    if (argc > 3) {
        strncpy(out->topic_name, argv[3], sizeof(out->topic_name) - 1);
        out->topic_name[sizeof(out->topic_name) - 1] = '\0';
    }
}
