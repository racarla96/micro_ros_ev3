#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <geometry_msgs/msg/twist.h>
#include <rmw_microros/rmw_microros.h>

#include "../../transport/config.h"

typedef struct { const char * agent_ip; uint16_t agent_port; } UDPTransportArgs;

extern bool udp_transport_open(struct uxrCustomTransport *);
extern bool udp_transport_close(struct uxrCustomTransport *);
extern size_t udp_transport_write(struct uxrCustomTransport *,
                                  const uint8_t *, size_t, uint8_t *);
extern size_t udp_transport_read(struct uxrCustomTransport *,
                                 uint8_t *, size_t, int, uint8_t *);

static geometry_msgs__msg__Twist msg;

void subscription_callback(const void * msgin)
{
    const geometry_msgs__msg__Twist * m = (const geometry_msgs__msg__Twist *)msgin;
    printf("linear.x=%.2f angular.z=%.2f -> %s\n",
           m->linear.x, m->angular.z, (m->linear.x == 0.0) ? "stopped" : "moving");
    fflush(stdout);
}

static const char * const kConfigKeys[] = { "agent_ip", "agent_port", "topic", NULL };

int main(int argc, char * argv[])
{
    static ev3_config_t config;
    ev3_config_load(argc, argv, kConfigKeys, &config);

    static UDPTransportArgs udp_args;
    udp_args.agent_ip   = ev3_config_get_string(&config, "agent_ip", "192.168.1.100");
    udp_args.agent_port = ev3_config_get_uint16(&config, "agent_port", 8888);
    const char * topic  = ev3_config_get_string(&config, "topic", "cmd_vel");

    rmw_uros_set_custom_transport(
        false, &udp_args,
        udp_transport_open, udp_transport_close,
        udp_transport_write, udp_transport_read
    );

    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;
    rclc_support_init(&support, 0, NULL, &allocator);

    rcl_node_t node;
    rclc_node_init_default(&node, "ev3_subscriber_twist", "", &support);

    rcl_subscription_t subscriber;
    rclc_subscription_init_default(
        &subscriber, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
        topic
    );

    rclc_executor_t executor;
    rclc_executor_init(&executor, &support.context, 1, &allocator);
    rclc_executor_add_subscription(&executor, &subscriber, &msg,
                                   &subscription_callback, ON_NEW_DATA);

    rclc_executor_spin(&executor);

    return 0;
}
