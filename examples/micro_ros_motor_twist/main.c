#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <geometry_msgs/msg/twist.h>
#include <rmw_microros/rmw_microros.h>
#include <uxr/client/util/time.h>

#include "motor.h"
#include "../../transport/config.h"

/* Swap left/right here if the robot drives backwards or turns the wrong way. */
#define MOTOR_LEFT_PORT   "outB"
#define MOTOR_RIGHT_PORT  "outC"
#define CONTROL_PERIOD_MS 50
#define CMD_TIMEOUT_MS    500
#define MAX_DUTY_CYCLE    100

/* Standard LEGO EV3 wheel/track dimensions; adjust for your chassis. */
#define WHEEL_DIAMETER_MM 56.0
#define TRACK_WIDTH_MM    120.0
#define PUBLISH_RATE_HZ   10
#define PI                3.14159265358979323846

typedef struct { const char * agent_ip; uint16_t agent_port; } UDPTransportArgs;

extern bool udp_transport_open(struct uxrCustomTransport *);
extern bool udp_transport_close(struct uxrCustomTransport *);
extern size_t udp_transport_write(struct uxrCustomTransport *,
                                  const uint8_t *, size_t, uint8_t *);
extern size_t udp_transport_read(struct uxrCustomTransport *,
                                 uint8_t *, size_t, int, uint8_t *);

static geometry_msgs__msg__Twist msg;
static ev3_motor_t g_motors[2];

static rcl_publisher_t odom_publisher;
static geometry_msgs__msg__Twist odom_msg;

typedef struct {
    pthread_mutex_t lock;
    double linear_x;
    double angular_z;
    int64_t last_update_ms;
} cmd_vel_state_t;

static cmd_vel_state_t cmd_vel_state = { .lock = PTHREAD_MUTEX_INITIALIZER };

static void subscription_callback(const void * msgin)
{
    const geometry_msgs__msg__Twist * m = (const geometry_msgs__msg__Twist *)msgin;

    pthread_mutex_lock(&cmd_vel_state.lock);
    cmd_vel_state.linear_x     = m->linear.x;
    cmd_vel_state.angular_z    = m->angular.z;
    cmd_vel_state.last_update_ms = uxr_millis();
    pthread_mutex_unlock(&cmd_vel_state.lock);
}

static int clamp_duty(double value)
{
    int duty = (int)(value * MAX_DUTY_CYCLE);
    if (duty > MAX_DUTY_CYCLE) duty = MAX_DUTY_CYCLE;
    if (duty < -MAX_DUTY_CYCLE) duty = -MAX_DUTY_CYCLE;
    return duty;
}

static double wheel_speed_mps(const ev3_motor_t * motor)
{
    double rot_per_sec = (double)ev3_motor_get_speed(motor) / motor->count_per_rot;
    return rot_per_sec * (PI * (WHEEL_DIAMETER_MM / 1000.0));
}

/* Runs on the executor thread via a timer, not the motor control thread:
 * it only reads encoder feedback, so it doesn't compete with the motor
 * writes for the sysfs handles. */
static void odom_timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
    (void)last_call_time;
    if (timer == NULL) return;

    double left_mps  = wheel_speed_mps(&g_motors[0]);
    double right_mps = wheel_speed_mps(&g_motors[1]);

    odom_msg.linear.x  = (left_mps + right_mps) / 2.0;
    odom_msg.angular.z = (right_mps - left_mps) / (TRACK_WIDTH_MM / 1000.0);

    (void)rcl_publish(&odom_publisher, &odom_msg, NULL);
}

/* Runs on its own thread so the motors are driven (and the no-data safety
 * stop kicks in) at a steady rate regardless of executor/agent jitter. */
static void * motor_control_thread(void * arg)
{
    (void)arg;
    ev3_motor_t * left  = &g_motors[0];
    ev3_motor_t * right = &g_motors[1];

    while (1) {
        pthread_mutex_lock(&cmd_vel_state.lock);
        double linear_x  = cmd_vel_state.linear_x;
        double angular_z = cmd_vel_state.angular_z;
        int64_t age_ms   = uxr_millis() - cmd_vel_state.last_update_ms;
        pthread_mutex_unlock(&cmd_vel_state.lock);

        if (age_ms > CMD_TIMEOUT_MS) {
            ev3_motor_stop(left);
            ev3_motor_stop(right);
        } else {
            ev3_motor_set_duty_cycle(left,  clamp_duty(linear_x - angular_z));
            ev3_motor_set_duty_cycle(right, clamp_duty(linear_x + angular_z));
        }

        usleep(CONTROL_PERIOD_MS * 1000);
    }

    return NULL;
}

int main(int argc, char * argv[])
{
    static ev3_config_t config;
    ev3_config_load(argc, argv, "192.168.1.100", 8888, "cmd_vel", &config);

    static UDPTransportArgs udp_args;
    udp_args.agent_ip   = config.agent_ip;
    udp_args.agent_port = config.agent_port;

    rmw_uros_set_custom_transport(
        false, &udp_args,
        udp_transport_open, udp_transport_close,
        udp_transport_write, udp_transport_read
    );

    if (!ev3_motor_open(MOTOR_LEFT_PORT, &g_motors[0]) ||
        !ev3_motor_open(MOTOR_RIGHT_PORT, &g_motors[1])) {
        fprintf(stderr, "Failed to open motors on %s / %s\n", MOTOR_LEFT_PORT, MOTOR_RIGHT_PORT);
        return 1;
    }

    cmd_vel_state.last_update_ms = uxr_millis();

    pthread_t motor_thread;
    pthread_create(&motor_thread, NULL, motor_control_thread, NULL);

    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;
    rclc_support_init(&support, 0, NULL, &allocator);

    rcl_node_t node;
    rclc_node_init_default(&node, "ev3_motor_twist", "", &support);

    rcl_subscription_t subscriber;
    rclc_subscription_init_default(
        &subscriber, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
        config.topic_name
    );

    rclc_publisher_init_best_effort(
        &odom_publisher, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
        "wheel_twist"
    );

    rcl_timer_t odom_timer;
    rclc_timer_init_default(&odom_timer, &support,
                            RCL_MS_TO_NS(1000 / PUBLISH_RATE_HZ), odom_timer_callback);

    rclc_executor_t executor;
    rclc_executor_init(&executor, &support.context, 2, &allocator);
    rclc_executor_add_subscription(&executor, &subscriber, &msg,
                                   &subscription_callback, ON_NEW_DATA);
    rclc_executor_add_timer(&executor, &odom_timer);

    rclc_executor_spin(&executor);

    return 0;
}
