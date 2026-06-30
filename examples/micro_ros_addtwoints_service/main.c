#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <example_interfaces/srv/add_two_ints.h>
#include <rmw_microros/rmw_microros.h>

typedef struct { const char * agent_ip; uint16_t agent_port; } UDPTransportArgs;

extern bool udp_transport_open(struct uxrCustomTransport *);
extern bool udp_transport_close(struct uxrCustomTransport *);
extern size_t udp_transport_write(struct uxrCustomTransport *,
                                  const uint8_t *, size_t, uint8_t *);
extern size_t udp_transport_read(struct uxrCustomTransport *,
                                 uint8_t *, size_t, int, uint8_t *);

static example_interfaces__srv__AddTwoInts_Request  req;
static example_interfaces__srv__AddTwoInts_Response res;

void service_callback(const void * request, void * response)
{
    const example_interfaces__srv__AddTwoInts_Request  * req_in =
        (const example_interfaces__srv__AddTwoInts_Request *)request;
    example_interfaces__srv__AddTwoInts_Response * res_in =
        (example_interfaces__srv__AddTwoInts_Response *)response;

    printf("Request: %lld + %lld\n", (long long)req_in->a, (long long)req_in->b);
    res_in->sum = req_in->a + req_in->b;
    fflush(stdout);
}

int main(void)
{
    static UDPTransportArgs udp_args = { .agent_ip = "192.168.1.100", .agent_port = 8888 };

    rmw_uros_set_custom_transport(
        false, &udp_args,
        udp_transport_open, udp_transport_close,
        udp_transport_write, udp_transport_read
    );

    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;
    rclc_support_init(&support, 0, NULL, &allocator);

    rcl_node_t node;
    rclc_node_init_default(&node, "ev3_service", "", &support);

    rcl_service_t service;
    rclc_service_init_default(
        &service, &node,
        ROSIDL_GET_SRV_TYPE_SUPPORT(example_interfaces, srv, AddTwoInts),
        "/addtwoints"
    );

    rclc_executor_t executor;
    rclc_executor_init(&executor, &support.context, 1, &allocator);
    rclc_executor_add_service(&executor, &service, &req, &res, service_callback);

    rclc_executor_spin(&executor);

    return 0;
}
