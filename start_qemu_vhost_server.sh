#!/bin/bash

SOCKET_PATH="/tmp/vhost-user-test-sock"
QEMU_PID_FILE="/tmp/qemu-vhost-test.pid"
MEMORY_SIZE="512M"
LOG_FILE="/tmp/qemu-vhost-test.log"

cleanup() {
    echo "Cleaning up QEMU vhost-user server..."
    if [ -f "$QEMU_PID_FILE" ]; then
        PID=$(cat "$QEMU_PID_FILE")
        if kill -0 "$PID" 2>/dev/null; then
            kill "$PID"
            sleep 2
            if kill -0 "$PID" 2>/dev/null; then
                kill -9 "$PID"
            fi
        fi
        rm -f "$QEMU_PID_FILE"
    fi
    rm -f "$SOCKET_PATH"
    rm -f "$LOG_FILE"
}

start_qemu_server() {
    echo "Starting QEMU vhost-user server..."
    
    # Clean up any existing instances
    cleanup
    
    # Check if qemu-system-x86_64 is available
    if ! command -v qemu-system-x86_64 &> /dev/null; then
        echo "Error: qemu-system-x86_64 not found. Please install QEMU."
        echo "On Ubuntu/Debian: sudo apt-get install qemu-system-x86"
        echo "On CentOS/RHEL: sudo yum install qemu-system-x86"
        exit 1
    fi
    
    # Start QEMU with minimal configuration for vhost-user socket
    # Use the simplest possible setup that creates a vhost-user socket
    qemu-system-x86_64 \
        -machine pc,accel=tcg \
        -cpu qemu64 \
        -m 256M \
        -display none \
        -serial none \
        -monitor none \
        -daemonize \
        -pidfile "$QEMU_PID_FILE" \
        -chardev socket,id=char0,path="$SOCKET_PATH",server=on,wait=off \
        -netdev vhost-user,id=vhost-user0,chardev=char0 \
        -device virtio-net-pci,netdev=vhost-user0 \
        -drive if=none,id=drive0,format=raw,file=/dev/null \
        -device virtio-blk-pci,drive=drive0 \
        > "$LOG_FILE" 2>&1
    
    if [ $? -ne 0 ]; then
        echo "Failed to start QEMU. Trying even simpler configuration..."
        
        # Last resort: minimal QEMU with just the socket
        qemu-system-x86_64 \
            -M pc \
            -m 128M \
            -display none \
            -daemonize \
            -pidfile "$QEMU_PID_FILE" \
            -chardev socket,id=char0,path="$SOCKET_PATH",server=on,wait=off \
            > "$LOG_FILE" 2>&1
        
        if [ $? -ne 0 ]; then
            echo "Failed to start QEMU with minimal configuration."
            cat "$LOG_FILE"
            exit 1
        fi
    fi
    
    # Wait for socket to be created
    echo "Waiting for vhost-user socket to be ready..."
    for i in {1..30}; do
        if [ -S "$SOCKET_PATH" ]; then
            echo "QEMU vhost-user server started successfully!"
            echo "Socket path: $SOCKET_PATH"
            echo "PID file: $QEMU_PID_FILE"
            echo "Log file: $LOG_FILE"
            return 0
        fi
        sleep 1
    done
    
    echo "Timeout waiting for vhost-user socket"
    cat "$LOG_FILE"
    cleanup
    exit 1
}

stop_qemu_server() {
    cleanup
    echo "QEMU vhost-user server stopped."
}

case "$1" in
    start)
        start_qemu_server
        ;;
    stop)
        stop_qemu_server
        ;;
    restart)
        stop_qemu_server
        start_qemu_server
        ;;
    status)
        if [ -f "$QEMU_PID_FILE" ]; then
            PID=$(cat "$QEMU_PID_FILE")
            if kill -0 "$PID" 2>/dev/null; then
                echo "QEMU vhost-user server is running (PID: $PID)"
                echo "Socket: $SOCKET_PATH"
                ls -la "$SOCKET_PATH" 2>/dev/null || echo "Socket not found"
            else
                echo "QEMU vhost-user server is not running (stale PID file)"
            fi
        else
            echo "QEMU vhost-user server is not running"
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac