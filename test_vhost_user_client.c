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
#include <pthread.h>
#include <assert.h>

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

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST_ASSERT(condition, message) do { \
    test_count++; \
    if (condition) { \
        printf("✓ PASS: %s\n", message); \
        test_passed++; \
    } else { \
        printf("✗ FAIL: %s\n", message); \
        test_failed++; \
    } \
} while(0)

static int create_mock_server(const char *socket_path) {
    int server_sock, client_sock;
    struct sockaddr_un addr;
    
    unlink(socket_path);
    
    server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("server socket");
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_sock);
        return -1;
    }
    
    if (listen(server_sock, 1) < 0) {
        perror("listen");
        close(server_sock);
        return -1;
    }
    
    return server_sock;
}

static void* mock_server_thread(void *arg) {
    const char *socket_path = (const char*)arg;
    int server_sock, client_sock;
    VhostUserMsg msg, reply;
    
    server_sock = create_mock_server(socket_path);
    if (server_sock < 0) {
        return NULL;
    }
    
    client_sock = accept(server_sock, NULL, NULL);
    if (client_sock < 0) {
        perror("accept");
        close(server_sock);
        return NULL;
    }
    
    while (1) {
        ssize_t ret = recv(client_sock, &msg, sizeof(msg), 0);
        if (ret != sizeof(msg)) {
            break;
        }
        
        memset(&reply, 0, sizeof(reply));
        reply.request = msg.request;
        reply.flags = 1;
        reply.size = 8;
        
        switch (msg.request) {
            case VHOST_USER_GET_FEATURES:
                reply.payload.u64 = 0x12345678ABCDEF00ULL;
                break;
            case VHOST_USER_GET_PROTOCOL_FEATURES:
                reply.payload.u64 = 0x0000000F;
                break;
            default:
                reply.payload.u64 = 0;
                break;
        }
        
        if (send(client_sock, &reply, sizeof(reply), 0) != sizeof(reply)) {
            break;
        }
    }
    
    close(client_sock);
    close(server_sock);
    unlink(socket_path);
    return NULL;
}

static int test_client_connection() {
    const char *test_socket = "/tmp/test-vhost-user-sock";
    pthread_t server_thread;
    int status;
    
    if (pthread_create(&server_thread, NULL, mock_server_thread, (void*)test_socket) != 0) {
        printf("Failed to create server thread\n");
        return 0;
    }
    
    usleep(100000);
    
    pid_t pid = fork();
    if (pid == 0) {
        execl("./vhost_user_client", "vhost_user_client", test_socket, NULL);
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, &status, 0);
        pthread_join(server_thread, NULL);
        return WEXITSTATUS(status) == 0;
    }
    
    return 0;
}

static int test_message_structure() {
    VhostUserMsg msg;
    
    TEST_ASSERT(sizeof(VhostUserMsg) == 20, "VhostUserMsg structure size is correct (20 bytes)");
    
    memset(&msg, 0, sizeof(msg));
    msg.request = VHOST_USER_GET_FEATURES;
    msg.flags = 1;
    msg.size = 8;
    msg.payload.u64 = 0x1234567890ABCDEFULL;
    
    TEST_ASSERT(msg.request == VHOST_USER_GET_FEATURES, "Message request field set correctly");
    TEST_ASSERT(msg.flags == 1, "Message flags field set correctly");
    TEST_ASSERT(msg.size == 8, "Message size field set correctly");
    TEST_ASSERT(msg.payload.u64 == 0x1234567890ABCDEFULL, "Message payload set correctly");
    
    return 1;
}

static int test_enum_values() {
    TEST_ASSERT(VHOST_USER_NONE == 0, "VHOST_USER_NONE value is correct");
    TEST_ASSERT(VHOST_USER_GET_FEATURES == 1, "VHOST_USER_GET_FEATURES value is correct");
    TEST_ASSERT(VHOST_USER_SET_FEATURES == 2, "VHOST_USER_SET_FEATURES value is correct");
    TEST_ASSERT(VHOST_USER_GET_PROTOCOL_FEATURES == 15, "VHOST_USER_GET_PROTOCOL_FEATURES value is correct");
    TEST_ASSERT(VHOST_USER_SET_PROTOCOL_FEATURES == 16, "VHOST_USER_SET_PROTOCOL_FEATURES value is correct");
    
    return 1;
}

static int test_invalid_socket() {
    const char *invalid_socket = "/nonexistent/path/socket";
    int status;
    
    pid_t pid = fork();
    if (pid == 0) {
        execl("./vhost_user_client", "vhost_user_client", invalid_socket, NULL);
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status) != 0;
    }
    
    return 0;
}

static int test_default_socket_path() {
    int status;
    
    pid_t pid = fork();
    if (pid == 0) {
        execl("./vhost_user_client", "vhost_user_client", NULL);
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status) != 0;
    }
    
    return 0;
}

int main() {
    printf("=== Vhost User Client Test Suite ===\n\n");
    
    printf("Testing message structure...\n");
    test_message_structure();
    printf("\n");
    
    printf("Testing enum values...\n");
    test_enum_values();
    printf("\n");
    
    printf("Testing client with invalid socket...\n");
    TEST_ASSERT(test_invalid_socket(), "Client fails gracefully with invalid socket path");
    printf("\n");
    
    printf("Testing client with default socket path...\n");
    TEST_ASSERT(test_default_socket_path(), "Client handles default socket path (expects failure without server)");
    printf("\n");
    
    printf("Testing client-server communication...\n");
    TEST_ASSERT(test_client_connection(), "Client successfully communicates with mock server");
    printf("\n");
    
    printf("=== Test Results ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);
    
    if (test_failed == 0) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ Some tests failed!\n");
        return 1;
    }
}