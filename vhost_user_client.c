#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdint.h>

#define VHOST_USER_PROTOCOL_F_MQ            0
#define VHOST_USER_PROTOCOL_F_LOG_SHMFD     1
#define VHOST_USER_PROTOCOL_F_RARP          2
#define VHOST_USER_PROTOCOL_F_REPLY_ACK     3

typedef enum VhostUserRequest {
    VHOST_USER_NONE = 0,
    VHOST_USER_GET_FEATURES = 1,
    VHOST_USER_SET_FEATURES = 2,
    VHOST_USER_SET_OWNER = 3,
    VHOST_USER_RESET_OWNER = 4,
    VHOST_USER_SET_MEM_TABLE = 5,
    VHOST_USER_SET_LOG_BASE = 6,
    VHOST_USER_SET_LOG_FD = 7,
    VHOST_USER_SET_VRING_NUM = 8,
    VHOST_USER_SET_VRING_ADDR = 9,
    VHOST_USER_SET_VRING_BASE = 10,
    VHOST_USER_GET_VRING_BASE = 11,
    VHOST_USER_SET_VRING_KICK = 12,
    VHOST_USER_SET_VRING_CALL = 13,
    VHOST_USER_SET_VRING_ERR = 14,
    VHOST_USER_GET_PROTOCOL_FEATURES = 15,
    VHOST_USER_SET_PROTOCOL_FEATURES = 16,
    VHOST_USER_MAX
} VhostUserRequest;

typedef struct VhostUserMsg {
    VhostUserRequest request;
    uint32_t flags;
    uint32_t size;
    union {
        uint64_t u64;
        struct {
            uint32_t index;
            uint32_t num;
        } state;
    } payload;
} __attribute__((packed)) VhostUserMsg;

static int connect_to_server(const char *socket_path) {
    int sock;
    struct sockaddr_un addr;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    return sock;
}

static int send_message(int sock, VhostUserMsg *msg) {
    ssize_t ret = send(sock, msg, sizeof(*msg), 0);
    if (ret != sizeof(*msg)) {
        perror("send");
        return -1;
    }
    return 0;
}

static int recv_message(int sock, VhostUserMsg *msg) {
    ssize_t ret = recv(sock, msg, sizeof(*msg), 0);
    if (ret != sizeof(*msg)) {
        if (ret < 0) {
            perror("recv");
        } else {
            printf("Partial message received: %zd bytes\n", ret);
        }
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    const char *socket_path = "/tmp/vhost-user-sock";
    int sock;
    VhostUserMsg msg, reply;

    if (argc > 1) {
        socket_path = argv[1];
    }

    printf("Connecting to vhost-user server at: %s\n", socket_path);

    sock = connect_to_server(socket_path);
    if (sock < 0) {
        printf("Failed to connect to server\n");
        return 1;
    }

    printf("Connected successfully\n");

    memset(&msg, 0, sizeof(msg));
    msg.request = VHOST_USER_GET_FEATURES;
    msg.flags = 1;
    msg.size = 0;

    printf("Sending GET_FEATURES request...\n");
    if (send_message(sock, &msg) < 0) {
        close(sock);
        return 1;
    }

    printf("Waiting for reply...\n");
    if (recv_message(sock, &reply) < 0) {
        close(sock);
        return 1;
    }

    printf("Received reply: request=%d, flags=0x%x, size=%d, features=0x%lx\n",
           reply.request, reply.flags, reply.size, reply.payload.u64);

    memset(&msg, 0, sizeof(msg));
    msg.request = VHOST_USER_GET_PROTOCOL_FEATURES;
    msg.flags = 1;
    msg.size = 0;

    printf("Sending GET_PROTOCOL_FEATURES request...\n");
    if (send_message(sock, &msg) < 0) {
        close(sock);
        return 1;
    }

    printf("Waiting for protocol features reply...\n");
    if (recv_message(sock, &reply) < 0) {
        close(sock);
        return 1;
    }

    printf("Received protocol features: request=%d, flags=0x%x, size=%d, features=0x%lx\n",
           reply.request, reply.flags, reply.size, reply.payload.u64);

    close(sock);
    printf("Client completed successfully\n");
    return 0;
}