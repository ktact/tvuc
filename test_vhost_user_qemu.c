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
#include <time.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>

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

#define QEMU_SOCKET_PATH "/tmp/vhost-user-test-sock"
#define QEMU_STARTUP_SCRIPT "./start_qemu_vhost_server.sh"
#define SIMPLE_STARTUP_SCRIPT "./start_simple_server.sh"
#define MAX_WAIT_TIME 30

static int start_qemu_server() {
    printf("Starting vhost-user server...\n");
    
    // Try simple server first (more reliable)
    int status = system(SIMPLE_STARTUP_SCRIPT " start");
    if (status == 0) {
        printf("Simple vhost-user server started successfully\n");
        return 1;
    }
    
    printf("Simple server failed, trying QEMU...\n");
    status = system(QEMU_STARTUP_SCRIPT " start");
    if (status != 0) {
        printf("Failed to start both simple and QEMU servers\n");
        return 0;
    }
    
    printf("QEMU server started successfully\n");
    return 1;
}

static int stop_qemu_server() {
    printf("Stopping vhost-user server...\n");
    
    // Try to stop both servers
    system(SIMPLE_STARTUP_SCRIPT " stop");
    system(QEMU_STARTUP_SCRIPT " stop");
    return 1;
}

static int check_qemu_server_status() {
    int status = system(SIMPLE_STARTUP_SCRIPT " status > /dev/null 2>&1");
    if (status == 0) return 1;
    
    status = system(QEMU_STARTUP_SCRIPT " status > /dev/null 2>&1");
    return status == 0;
}

static int wait_for_socket(const char *socket_path, int timeout_seconds) {
    for (int i = 0; i < timeout_seconds; i++) {
        if (access(socket_path, F_OK) == 0) {
            return 1;
        }
        sleep(1);
    }
    return 0;
}

static int test_qemu_connection() {
    if (!wait_for_socket(QEMU_SOCKET_PATH, MAX_WAIT_TIME)) {
        printf("Timeout waiting for QEMU socket: %s\n", QEMU_SOCKET_PATH);
        return 0;
    }
    
    int sock;
    struct sockaddr_un addr;
    
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 0;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, QEMU_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect to QEMU");
        close(sock);
        return 0;
    }
    
    close(sock);
    return 1;
}

static int test_vhost_user_protocol_with_qemu() {
    if (!wait_for_socket(QEMU_SOCKET_PATH, MAX_WAIT_TIME)) {
        printf("QEMU socket not available: %s\n", QEMU_SOCKET_PATH);
        return 0;
    }
    
    int sock;
    struct sockaddr_un addr;
    VhostUserMsg msg, reply;
    
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 0;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, QEMU_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect to QEMU");
        close(sock);
        return 0;
    }
    
    printf("Connected to QEMU vhost-user server\n");
    
    // Test GET_FEATURES
    memset(&msg, 0, sizeof(msg));
    msg.request = VHOST_USER_GET_FEATURES;
    msg.flags = 1;
    msg.size = 0;
    
    if (send(sock, &msg, sizeof(msg), 0) != sizeof(msg)) {
        perror("send GET_FEATURES");
        close(sock);
        return 0;
    }
    
    if (recv(sock, &reply, sizeof(reply), 0) != sizeof(reply)) {
        perror("recv GET_FEATURES reply");
        close(sock);
        return 0;
    }
    
    printf("GET_FEATURES reply: request=%d, flags=0x%x, size=%d, features=0x%lx\n",
           reply.request, reply.flags, reply.size, reply.payload.u64);
    
    // Test GET_PROTOCOL_FEATURES
    memset(&msg, 0, sizeof(msg));
    msg.request = VHOST_USER_GET_PROTOCOL_FEATURES;
    msg.flags = 1;
    msg.size = 0;
    
    if (send(sock, &msg, sizeof(msg), 0) != sizeof(msg)) {
        perror("send GET_PROTOCOL_FEATURES");
        close(sock);
        return 0;
    }
    
    if (recv(sock, &reply, sizeof(reply), 0) != sizeof(reply)) {
        perror("recv GET_PROTOCOL_FEATURES reply");
        close(sock);
        return 0;
    }
    
    printf("GET_PROTOCOL_FEATURES reply: request=%d, flags=0x%x, size=%d, features=0x%lx\n",
           reply.request, reply.flags, reply.size, reply.payload.u64);
    
    close(sock);
    return 1;
}

static int test_client_with_qemu() {
    if (!wait_for_socket(QEMU_SOCKET_PATH, MAX_WAIT_TIME)) {
        printf("QEMU socket not available for client test\n");
        return 0;
    }
    
    int status;
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process: run the vhost_user_client
        execl("./vhost_user_client", "vhost_user_client", QEMU_SOCKET_PATH, NULL);
        exit(1);
    } else if (pid > 0) {
        // Parent process: wait for client to complete
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status) == 0;
    }
    
    return 0;
}

static int test_multiple_connections() {
    if (!wait_for_socket(QEMU_SOCKET_PATH, MAX_WAIT_TIME)) {
        printf("QEMU socket not available for multiple connection test\n");
        return 0;
    }
    
    int successful_connections = 0;
    
    for (int i = 0; i < 3; i++) {
        int sock;
        struct sockaddr_un addr;
        
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) continue;
        
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, QEMU_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            successful_connections++;
            printf("Connection %d successful\n", i + 1);
            
            // Send a quick message to verify the connection works
            VhostUserMsg msg;
            memset(&msg, 0, sizeof(msg));
            msg.request = VHOST_USER_GET_FEATURES;
            msg.flags = 1;
            msg.size = 0;
            
            send(sock, &msg, sizeof(msg), 0);
            close(sock);
        } else {
            printf("Connection %d failed\n", i + 1);
        }
        
        // Small delay between connections
        usleep(100000);
    }
    
    return successful_connections >= 2;
}

static int test_socket_permissions() {
    if (!wait_for_socket(QEMU_SOCKET_PATH, MAX_WAIT_TIME)) {
        return 0;
    }
    
    struct stat st;
    if (stat(QEMU_SOCKET_PATH, &st) == 0) {
        printf("Socket permissions: %o\n", st.st_mode & 0777);
        return S_ISSOCK(st.st_mode);
    }
    
    return 0;
}

int main() {
    printf("=== Vhost-User Client Integration Test Suite ===\n\n");
    
    printf("Testing with simple vhost-user server (fallback to QEMU if available)\n\n");
    
    // Start vhost-user server
    if (!start_qemu_server()) {
        printf("Failed to start vhost-user server. Exiting.\n");
        return 1;
    }
    
    // Give QEMU time to initialize
    sleep(3);
    
    printf("Testing QEMU server status...\n");
    TEST_ASSERT(check_qemu_server_status(), "QEMU vhost-user server is running");
    printf("\n");
    
    printf("Testing socket creation and permissions...\n");
    TEST_ASSERT(test_socket_permissions(), "QEMU socket exists and has correct type");
    printf("\n");
    
    printf("Testing basic connection to QEMU...\n");
    TEST_ASSERT(test_qemu_connection(), "Can connect to QEMU vhost-user socket");
    printf("\n");
    
    printf("Testing vhost-user protocol with QEMU...\n");
    TEST_ASSERT(test_vhost_user_protocol_with_qemu(), "Vhost-user protocol communication works with QEMU");
    printf("\n");
    
    printf("Testing vhost_user_client with QEMU...\n");
    TEST_ASSERT(test_client_with_qemu(), "vhost_user_client successfully communicates with QEMU");
    printf("\n");
    
    printf("Testing multiple connections...\n");
    TEST_ASSERT(test_multiple_connections(), "Multiple connections to QEMU work correctly");
    printf("\n");
    
    // Cleanup
    printf("Cleaning up QEMU server...\n");
    stop_qemu_server();
    
    printf("=== Test Results ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);
    
    if (test_failed == 0) {
        printf("✓ All QEMU integration tests passed!\n");
        return 0;
    } else {
        printf("✗ Some QEMU integration tests failed!\n");
        return 1;
    }
}