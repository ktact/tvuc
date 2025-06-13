#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>
#include <sys/wait.h>

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

static volatile int running = 1;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

static void handle_client(int client_sock) {
    VhostUserMsg msg, reply;
    
    printf("Client connected\n");
    
    while (running) {
        ssize_t ret = recv(client_sock, &msg, sizeof(msg), 0);
        if (ret != sizeof(msg)) {
            if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("recv");
            }
            break;
        }
        
        printf("Received request: %d (flags=0x%x, size=%d)\n", 
               msg.request, msg.flags, msg.size);
        
        // Prepare reply
        memset(&reply, 0, sizeof(reply));
        reply.request = msg.request;
        reply.flags = 1;
        reply.size = 8;
        
        switch (msg.request) {
            case VHOST_USER_GET_FEATURES:
                // Return realistic virtio-net features
                reply.payload.u64 = 0x100002000000ULL; // VIRTIO_NET_F_MRG_RXBUF | VIRTIO_F_VERSION_1
                printf("Sending GET_FEATURES reply: 0x%lx\n", reply.payload.u64);
                break;
                
            case VHOST_USER_GET_PROTOCOL_FEATURES:
                // Return supported protocol features
                reply.payload.u64 = (1ULL << VHOST_USER_PROTOCOL_F_MQ) |
                                   (1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK);
                printf("Sending GET_PROTOCOL_FEATURES reply: 0x%lx\n", reply.payload.u64);
                break;
                
            case VHOST_USER_SET_FEATURES:
                reply.size = 0;
                printf("SET_FEATURES: 0x%lx\n", msg.payload.u64);
                break;
                
            case VHOST_USER_SET_PROTOCOL_FEATURES:
                reply.size = 0;
                printf("SET_PROTOCOL_FEATURES: 0x%lx\n", msg.payload.u64);
                break;
                
            case VHOST_USER_SET_OWNER:
                reply.size = 0;
                printf("SET_OWNER\n");
                break;
                
            default:
                reply.payload.u64 = 0;
                printf("Unhandled request: %d\n", msg.request);
                break;
        }
        
        if (send(client_sock, &reply, sizeof(reply), 0) != sizeof(reply)) {
            perror("send");
            break;
        }
    }
    
    printf("Client disconnected\n");
}

int main(int argc, char *argv[]) {
    const char *socket_path = "/tmp/vhost-user-test-sock";
    int server_sock, client_sock;
    struct sockaddr_un addr;
    
    if (argc > 1) {
        socket_path = argv[1];
    }
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Remove existing socket
    unlink(socket_path);
    
    // Create server socket
    server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        return 1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_sock);
        return 1;
    }
    
    if (listen(server_sock, 5) < 0) {
        perror("listen");
        close(server_sock);
        unlink(socket_path);
        return 1;
    }
    
    printf("Simple vhost-user server listening on: %s\n", socket_path);
    printf("PID: %d\n", getpid());
    
    while (running) {
        client_sock = accept(server_sock, NULL, NULL);
        if (client_sock < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }
        
        // Fork to handle multiple clients concurrently
        pid_t child = fork();
        if (child == 0) {
            // Child process handles the client
            close(server_sock);
            handle_client(client_sock);
            close(client_sock);
            exit(0);
        } else if (child > 0) {
            // Parent process continues accepting
            close(client_sock);
        } else {
            perror("fork");
            close(client_sock);
        }
    }
    
    close(server_sock);
    unlink(socket_path);
    printf("Server shutting down\n");
    
    return 0;
}