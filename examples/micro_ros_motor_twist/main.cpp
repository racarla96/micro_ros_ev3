#include <cstdint>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <unistd.h>

extern "C" {
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <geometry_msgs/msg/twist.h>
#include <rmw_microros/rmw_microros.h>
#include <uxr/client/util/time.h>
#include "../../transport/config.h"
}

#include "ev3dev.h"

/* Swap left/right here if the robot drives backwards or turns the wrong way. */
#define MOTOR_LEFT_PORT   ev3dev::OUTPUT_B
#define MOTOR_RIGHT_PORT  ev3dev::OUTPUT_C
#define CONTROL_PERIOD_MS 50
#define CMD_TIMEOUT_MS    200

/* Standard LEGO EV3 wheel/track dimensions; adjust for your chassis. */
#define WHEEL_DIAMETER_MM 56.0
#define TRACK_WIDTH_MM    120.0
#define PUBLISH_RATE_HZ   10
#define PI                3.14159265358979323846

typedef struct { const char * agent_ip; uint16_t agent_port; } UDPTransportArgs;

extern "C" {
bool udp_transport_open(struct uxrCustomTransport *);
bool udp_transport_close(struct uxrCustomTransport *);
size_t udp_transport_write(struct uxrCustomTransport *,
                           const uint8_t *, size_t, uint8_t *);
size_t udp_transport_read(struct uxrCustomTransport *,
                          uint8_t *, size_t, int, uint8_t *);
}

static geometry_msgs__msg__Twist msg;

/* Constructing binds the port immediately; check .connected() before use. */
static ev3dev::large_motor g_motor_left(MOTOR_LEFT_PORT);
static ev3dev::large_motor g_motor_right(MOTOR_RIGHT_PORT);

/* count_per_rot/max_speed don't change at runtime; read once in main() to
 * avoid a sysfs read on every control-loop/odometry-timer tick. */
static int g_count_per_rot_left;
static int g_count_per_rot_right;
static int g_max_speed_left;
static int g_max_speed_right;

static rcl_publisher_t odom_publisher;
static geometry_msgs__msg__Twist odom_msg;

typedef struct {
    pthread_mutex_t lock;
    double linear_x;
    double angular_z;
    int64_t last_update_ms;
} cmd_vel_state_t;

static cmd_vel_state_t cmd_vel_state;

static void subscription_callback(const void * msgin)
{
    const geometry_msgs__msg__Twist * m = (const geometry_msgs__msg__Twist *)msgin;

    //printf("cmd_vel received: linear.x=%.2f angular.z=%.2f\n", m->linear.x, m->angular.z);
    //fflush(stdout);

    pthread_mutex_lock(&cmd_vel_state.lock);
    cmd_vel_state.linear_x       = m->linear.x;
    cmd_vel_state.angular_z      = m->angular.z;
    cmd_vel_state.last_update_ms = uxr_millis();
    pthread_mutex_unlock(&cmd_vel_state.lock);
}

static const double WHEEL_CIRCUMFERENCE_M = PI * (WHEEL_DIAMETER_MM / 1000.0);
static const double TRACK_WIDTH_M         = TRACK_WIDTH_MM / 1000.0;

/* Forward kinematics: measured wheel speed (encoder counts/sec) -> m/s.
 * Used for the odometry feedback published on wheel_twist. */
static double wheel_speed_mps(ev3dev::large_motor & motor, int count_per_rot)
{
    double rot_per_sec = (double)motor.speed() / count_per_rot;
    return rot_per_sec * WHEEL_CIRCUMFERENCE_M;
}

/* Inverse kinematics: wheel speed in m/s -> speed_sp (encoder counts/sec),
 * clamped to what the motor actually supports. */
static int mps_to_speed_sp(double mps, int count_per_rot, int max_speed)
{
    double rot_per_sec = mps / WHEEL_CIRCUMFERENCE_M;
    int counts_per_sec = (int)(rot_per_sec * count_per_rot);
    if (counts_per_sec > max_speed)  counts_per_sec = max_speed;
    if (counts_per_sec < -max_speed) counts_per_sec = -max_speed;
    return counts_per_sec;
}

/* Runs on the executor thread via a timer, not the motor control thread:
 * it only reads encoder feedback, so it doesn't compete with the motor
 * writes for the sysfs handles. */
static void odom_timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
    (void)last_call_time;
    if (timer == NULL) return;

    /* ev3dev-lang-cpp throws std::system_error on a failed sysfs read/write
     * (e.g. permission denied) — printed once per failure/recovery so it
     * doesn't spam the log every publish cycle. */
    static bool last_read_ok = true;
    try {
        double left_mps  = wheel_speed_mps(g_motor_left, g_count_per_rot_left);
        double right_mps = wheel_speed_mps(g_motor_right, g_count_per_rot_right);

        odom_msg.linear.x  = (left_mps + right_mps) / 2.0;
        odom_msg.angular.z = (right_mps - left_mps) / TRACK_WIDTH_M;

        (void)rcl_publish(&odom_publisher, &odom_msg, NULL);

        if (!last_read_ok) {
            //fprintf(stderr, "Reading motor speed recovered\n");
            //fflush(stderr);
            last_read_ok = true;
        }
    } catch (const std::exception & e) {
        if (last_read_ok) {
            //fprintf(stderr, "Reading motor speed failed: %s\n", e.what());
            //fflush(stderr);
            last_read_ok = false;
        }
    }
}

/* Runs on its own thread so the motors are driven (and the no-data safety
 * stop kicks in) at a steady rate regardless of executor/agent jitter. */
static void * motor_control_thread(void * arg)
{
    (void)arg;

    /* An uncaught exception here would propagate out of the thread entry
     * point and call std::terminate(), silently killing the whole process —
     * so every ev3dev-lang-cpp call (which throws std::system_error on a
     * failed sysfs write, e.g. permission denied) must be caught. */
    bool last_write_ok = true;

    /* stop() takes the motor out of "run-forever" mode, so speed_sp stops
     * having live effect. Track that so we know to re-issue run_forever()
     * once when commands resume, instead of only the first time ever at
     * startup — otherwise the motors silently stay stopped after any
     * command gap longer than CMD_TIMEOUT_MS. */
    bool motors_stopped = false;

    while (1) {
        pthread_mutex_lock(&cmd_vel_state.lock);
        double linear_x  = cmd_vel_state.linear_x;
        double angular_z = cmd_vel_state.angular_z;
        int64_t age_ms   = uxr_millis() - cmd_vel_state.last_update_ms;
        pthread_mutex_unlock(&cmd_vel_state.lock);

        try {
            if (age_ms > CMD_TIMEOUT_MS) {
                g_motor_left.stop();
                g_motor_right.stop();
                motors_stopped = true;
            } else {
                if (motors_stopped) {
                    g_motor_left.run_forever();
                    g_motor_right.run_forever();
                    motors_stopped = false;
                }

                /* Differential-drive inverse kinematics: each wheel's linear
                 * speed is the robot's forward speed plus/minus its share of
                 * the rotation at the wheel radius (track width / 2). */
                double v_left_mps  = linear_x - angular_z * (TRACK_WIDTH_M / 2.0);
                double v_right_mps = linear_x + angular_z * (TRACK_WIDTH_M / 2.0);

                g_motor_left.set_speed_sp(
                    mps_to_speed_sp(v_left_mps, g_count_per_rot_left, g_max_speed_left));
                g_motor_right.set_speed_sp(
                    mps_to_speed_sp(v_right_mps, g_count_per_rot_right, g_max_speed_right));
            }
            if (!last_write_ok) {
                //fprintf(stderr, "Writing to motors recovered\n");
                //fflush(stderr);
                last_write_ok = true;
            }
        } catch (const std::exception & e) {
            if (last_write_ok) {
                //fprintf(stderr, "Writing to motors failed: %s\n", e.what());
                //fflush(stderr);
                last_write_ok = false;
            }
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

    if (!g_motor_left.connected() || !g_motor_right.connected()) {
        fprintf(stderr, "Failed to find motors on %s / %s\n",
               ev3dev::OUTPUT_B, ev3dev::OUTPUT_C);
        return 1;
    }
    try {
        g_count_per_rot_left  = g_motor_left.count_per_rot();
        g_count_per_rot_right = g_motor_right.count_per_rot();
        g_max_speed_left      = g_motor_left.max_speed();
        g_max_speed_right     = g_motor_right.max_speed();

        g_motor_left.run_forever();
        g_motor_right.run_forever();
    } catch (const std::exception & e) {
        fprintf(stderr, "Failed to initialize motors: %s\n", e.what());
        return 1;
    }

    pthread_mutex_init(&cmd_vel_state.lock, NULL);
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
