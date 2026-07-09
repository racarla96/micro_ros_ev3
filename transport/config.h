#ifndef EV3_CONFIG_H
#define EV3_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EV3_CONFIG_IP_MAX   64
#define EV3_CONFIG_NAME_MAX 128

typedef struct {
    char     agent_ip[EV3_CONFIG_IP_MAX];
    uint16_t agent_port;
    char     topic_name[EV3_CONFIG_NAME_MAX];
} ev3_config_t;

/*
 * Resolves agent_ip / agent_port / topic_name, in increasing priority:
 *   1. default_ip / default_port / default_topic (compiled-in defaults)
 *   2. config.txt next to the running binary ("key=value" lines, keys:
 *      agent_ip, agent_port, topic)
 *   3. command-line arguments: argv[1]=agent_ip argv[2]=agent_port argv[3]=topic
 */
void ev3_config_load(int argc, char ** argv,
                     const char * default_ip, uint16_t default_port,
                     const char * default_topic, ev3_config_t * out);

#ifdef __cplusplus
}
#endif

#endif
