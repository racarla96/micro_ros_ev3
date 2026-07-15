#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/int32.h>
#include <rmw_microros/rmw_microros.h>
#include <uxr/client/util/time.h>

#include "../../transport/config.h"

typedef struct { const char * agent_ip; uint16_t agent_port; } UDPTransportArgs;

extern bool udp_transport_open(struct uxrCustomTransport *);
extern bool udp_transport_close(struct uxrCustomTransport *);
extern size_t udp_transport_write(struct uxrCustomTransport *,
                                  const uint8_t *, size_t, uint8_t *);
extern size_t udp_transport_read(struct uxrCustomTransport *,
                                 uint8_t *, size_t, int, uint8_t *);

#define EXECUTE_EVERY_N_MS(MS, X) do { \
    static int64_t init = -1; \
    int64_t now = uxr_millis(); \
    if (init == -1) { init = now; } \
    if (now - init > (MS)) { X; init = now; } \
} while (0)

enum states { WAITING_AGENT, AGENT_AVAILABLE, AGENT_CONNECTED, AGENT_DISCONNECTED };

static rclc_support_t support;
static rcl_node_t node;
static rcl_timer_t timer;
static rclc_executor_t executor;
static rcl_allocator_t allocator;
static rcl_publisher_t publisher;
static std_msgs__msg__Int32 msg;
static ev3_config_t config;
static const char * g_topic;

static const char * const kConfigKeys[] = { "agent_ip", "agent_port", "topic", NULL };

static const char * state_name(enum states s)
{
    switch (s) {
    case WAITING_AGENT:      return "waiting for agent";
    case AGENT_AVAILABLE:    return "agent available";
    case AGENT_CONNECTED:    return "connected";
    case AGENT_DISCONNECTED: return "disconnected";
    default:                 return "?";
    }
}

static void timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
    (void)last_call_time;
    if (timer != NULL) {
        (void)rcl_publish(&publisher, &msg, NULL);
        msg.data++;
    }
}

/* Entity creation/destruction is repeated on every reconnect, so build
 * with RMW_UXRCE_ENTITY_CREATION_DESTROY_TIMEOUT=0 and
 * UCLIENT_MAX_SESSION_CONNECTION_ATTEMPTS=3 to keep it fast. */
static bool create_entities(void)
{
    allocator = rcl_get_default_allocator();

    if (rclc_support_init(&support, 0, NULL, &allocator) != RCL_RET_OK) return false;
    if (rclc_node_init_default(&node, "ev3_reconnection", "", &support) != RCL_RET_OK) return false;
    if (rclc_publisher_init_best_effort(
            &publisher, &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
            g_topic) != RCL_RET_OK) return false;

    const unsigned int timer_timeout = 1000;
    if (rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(timer_timeout), timer_callback) != RCL_RET_OK) return false;

    executor = rclc_executor_get_zero_initialized_executor();
    if (rclc_executor_init(&executor, &support.context, 1, &allocator) != RCL_RET_OK) return false;
    if (rclc_executor_add_timer(&executor, &timer) != RCL_RET_OK) return false;

    return true;
}

static void destroy_entities(void)
{
    rmw_context_t * rmw_context = rcl_context_get_rmw_context(&support.context);
    (void)rmw_uros_set_context_entity_destroy_session_timeout(rmw_context, 0);

    rcl_publisher_fini(&publisher, &node);
    rcl_timer_fini(&timer);
    rclc_executor_fini(&executor);
    rcl_node_fini(&node);
    rclc_support_fini(&support);
}

int main(int argc, char * argv[])
{
    ev3_config_load(argc, argv, kConfigKeys, &config);
    g_topic = ev3_config_get_string(&config, "topic", "ev3_topic");

    static UDPTransportArgs udp_args;
    udp_args.agent_ip   = ev3_config_get_string(&config, "agent_ip", "192.168.1.100");
    udp_args.agent_port = ev3_config_get_uint16(&config, "agent_port", 8888);

    rmw_uros_set_custom_transport(
        false, &udp_args,
        udp_transport_open, udp_transport_close,
        udp_transport_write, udp_transport_read
    );

    msg.data = 0;
    enum states state = WAITING_AGENT;
    enum states prev_state = AGENT_CONNECTED; /* force the first status print */

    while (1) {
        switch (state) {
        case WAITING_AGENT:
            EXECUTE_EVERY_N_MS(500,
                state = (rmw_uros_ping_agent(100, 1) == RMW_RET_OK) ? AGENT_AVAILABLE : WAITING_AGENT;
            );
            break;
        case AGENT_AVAILABLE:
            state = create_entities() ? AGENT_CONNECTED : WAITING_AGENT;
            if (state == WAITING_AGENT) {
                destroy_entities();
            }
            break;
        case AGENT_CONNECTED:
            EXECUTE_EVERY_N_MS(200,
                state = (rmw_uros_ping_agent(100, 1) == RMW_RET_OK) ? AGENT_CONNECTED : AGENT_DISCONNECTED;
            );
            if (state == AGENT_CONNECTED) {
                rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
            }
            break;
        case AGENT_DISCONNECTED:
            destroy_entities();
            state = WAITING_AGENT;
            break;
        default:
            break;
        }

        if (state != prev_state) {
            printf("State: %s\n", state_name(state));
            fflush(stdout);
            prev_state = state;
        }
    }

    return 0;
}
