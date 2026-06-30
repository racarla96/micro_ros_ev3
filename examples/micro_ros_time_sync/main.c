#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rmw_microros/rmw_microros.h>

typedef struct { const char * agent_ip; uint16_t agent_port; } UDPTransportArgs;

extern bool udp_transport_open(struct uxrCustomTransport *);
extern bool udp_transport_close(struct uxrCustomTransport *);
extern size_t udp_transport_write(struct uxrCustomTransport *,
                                  const uint8_t *, size_t, uint8_t *);
extern size_t udp_transport_read(struct uxrCustomTransport *,
                                 uint8_t *, size_t, int, uint8_t *);

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
    rclc_node_init_default(&node, "ev3_time_sync", "", &support);

    while (1) {
        if (rmw_uros_sync_session(1000) == RMW_RET_OK) {
            int64_t time_ms = rmw_uros_epoch_millis();
            time_t  time_s  = (time_t)(time_ms / 1000);
            struct tm * t = gmtime(&time_s);
            printf("Agent time: %04d-%02d-%02d %02d:%02d:%02d.%03d UTC\n",
                   t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                   t->tm_hour, t->tm_min, t->tm_sec,
                   (int)(time_ms % 1000));
        } else {
            printf("Session sync failed\n");
        }
        fflush(stdout);
        sleep(1);
    }

    return 0;
}
