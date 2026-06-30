#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <uxr/client/transport.h>

static int serial_fd = -1;

bool serial_transport_open(struct uxrCustomTransport * transport)
{
    const char * dev = (const char *)transport->args;
    serial_fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial_fd < 0) return false;

    struct termios tty = {0};
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag = CS8 | CLOCAL | CREAD;
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cc[VTIME] = 1;
    tty.c_cc[VMIN]  = 0;
    tcflush(serial_fd, TCIFLUSH);
    tcsetattr(serial_fd, TCSANOW, &tty);
    return true;
}

bool serial_transport_close(struct uxrCustomTransport * transport)
{
    return close(serial_fd) == 0;
}

size_t serial_transport_write(struct uxrCustomTransport * transport,
                              const uint8_t * buf, size_t len, uint8_t * err)
{
    ssize_t n = write(serial_fd, buf, len);
    return (n < 0) ? 0 : (size_t)n;
}

size_t serial_transport_read(struct uxrCustomTransport * transport,
                             uint8_t * buf, size_t len, int timeout_ms, uint8_t * err)
{
    ssize_t n = read(serial_fd, buf, len);
    return (n < 0) ? 0 : (size_t)n;
}
