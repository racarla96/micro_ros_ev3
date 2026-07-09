#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/int32.h>
#include <rmw_microros/rmw_microros.h>

#include "../../transport/config.h"

typedef struct { const char * agent_ip; uint16_t agent_port; } UDPTransportArgs;

extern bool udp_transport_open(struct uxrCustomTransport *);
extern bool udp_transport_close(struct uxrCustomTransport *);
extern size_t udp_transport_write(struct uxrCustomTransport *,
                                  const uint8_t *, size_t, uint8_t *);
extern size_t udp_transport_read(struct uxrCustomTransport *,
                                 uint8_t *, size_t, int, uint8_t *);

static std_msgs__msg__Int32 msg;

void subscription_callback(const void * msgin)
{
    const std_msgs__msg__Int32 * m = (const std_msgs__msg__Int32 *)msgin;
    printf("Received: %d\n", m->data);
    fflush(stdout);
}

int main(int argc, char * argv[])
{
    static ev3_config_t config;
    ev3_config_load(argc, argv, "192.168.1.100", 8888, "ev3_topic", &config);

    static UDPTransportArgs udp_args;
    udp_args.agent_ip   = config.agent_ip;
    udp_args.agent_port = config.agent_port;

    rmw_uros_set_custom_transport(
        false, &udp_args,
        udp_transport_open, udp_transport_close,
        udp_transport_write, udp_transport_read
    );

    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;
    rclc_support_init(&support, 0, NULL, &allocator);

    rcl_node_t node;
    rclc_node_init_default(&node, "ev3_subscriber", "", &support);

    rcl_subscription_t subscriber;
    rclc_subscription_init_default(
        &subscriber, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        config.topic_name
    );

    rclc_executor_t executor;
    rclc_executor_init(&executor, &support.context, 1, &allocator);
    rclc_executor_add_subscription(&executor, &subscriber, &msg,
                                   &subscription_callback, ON_NEW_DATA);

    rclc_executor_spin(&executor);

    return 0;
}
