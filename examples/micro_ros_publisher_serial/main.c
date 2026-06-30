#include <unistd.h>
#include <stdint.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/int32.h>
#include <rmw_microros/rmw_microros.h>

extern bool serial_transport_open(struct uxrCustomTransport *);
extern bool serial_transport_close(struct uxrCustomTransport *);
extern size_t serial_transport_write(struct uxrCustomTransport *,
                                     const uint8_t *, size_t, uint8_t *);
extern size_t serial_transport_read(struct uxrCustomTransport *,
                                    uint8_t *, size_t, int, uint8_t *);

int main(void)
{
    /* Adjust to your EV3 serial device — see README for common options */
    rmw_uros_set_custom_transport(
        true,
        (void *)"/dev/ttyS1",
        serial_transport_open,
        serial_transport_close,
        serial_transport_write,
        serial_transport_read
    );

    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;
    rclc_support_init(&support, 0, NULL, &allocator);

    rcl_node_t node;
    rclc_node_init_default(&node, "ev3_node", "", &support);

    rcl_publisher_t publisher;
    std_msgs__msg__Int32 msg;
    rclc_publisher_init_default(
        &publisher, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        "ev3_topic"
    );

    msg.data = 0;
    while (1) {
        (void)rcl_publish(&publisher, &msg, NULL);
        msg.data++;
        usleep(500000);
    }

    return 0;
}
