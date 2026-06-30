#include <time.h>
#include <stdint.h>

/* glibc 2.34+ routes clock_gettime to __clock_gettime64 on 32-bit targets
 * (Y2038 compat layer). ev3dev Buster has glibc 2.28 which lacks this symbol.
 * This shim provides it by delegating to the standard clock_gettime. */
struct timespec64_compat {
    int64_t tv_sec;
    int32_t tv_nsec;
    int32_t _pad;
};

int __clock_gettime64(clockid_t clkid, struct timespec64_compat *tp)
{
    struct timespec ts;
    int ret = clock_gettime(clkid, &ts);
    if (ret == 0) {
        tp->tv_sec  = (int64_t)ts.tv_sec;
        tp->tv_nsec = (int32_t)ts.tv_nsec;
        tp->_pad    = 0;
    }
    return ret;
}
