#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <uxr/client/transport.h>

typedef struct {
    const char * agent_ip;
    uint16_t     agent_port;
} UDPTransportArgs;

static int sock_fd = -1;
static struct sockaddr_in agent_addr;

bool udp_transport_open(struct uxrCustomTransport * transport)
{
    UDPTransportArgs * args = (UDPTransportArgs *)transport->args;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) return false;

    struct sockaddr_in local_addr = {0};
    local_addr.sin_family      = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port        = 0;
    if (bind(sock_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        close(sock_fd);
        return false;
    }

    memset(&agent_addr, 0, sizeof(agent_addr));
    agent_addr.sin_family = AF_INET;
    agent_addr.sin_port   = htons(args->agent_port);
    inet_pton(AF_INET, args->agent_ip, &agent_addr.sin_addr);

    return true;
}

bool udp_transport_close(struct uxrCustomTransport * transport)
{
    return close(sock_fd) == 0;
}

size_t udp_transport_write(struct uxrCustomTransport * transport,
                           const uint8_t * buf, size_t len, uint8_t * err)
{
    ssize_t n = sendto(sock_fd, buf, len, 0,
                       (struct sockaddr *)&agent_addr, sizeof(agent_addr));
    return (n < 0) ? 0 : (size_t)n;
}

size_t udp_transport_read(struct uxrCustomTransport * transport,
                          uint8_t * buf, size_t len, int timeout_ms, uint8_t * err)
{
    struct timeval tv = {
        .tv_sec  = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000
    };
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    ssize_t n = recv(sock_fd, buf, len, 0);
    return (n < 0) ? 0 : (size_t)n;
}
