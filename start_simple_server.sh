#!/bin/bash

SOCKET_PATH="/tmp/vhost-user-test-sock"
PID_FILE="/tmp/simple-vhost-server.pid"
LOG_FILE="/tmp/simple-vhost-server.log"

cleanup() {
    echo "Cleaning up simple vhost-user server..."
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if kill -0 "$PID" 2>/dev/null; then
            kill "$PID"
            sleep 1
            if kill -0 "$PID" 2>/dev/null; then
                kill -9 "$PID"
            fi
        fi
        rm -f "$PID_FILE"
    fi
    rm -f "$SOCKET_PATH"
    rm -f "$LOG_FILE"
}

start_server() {
    echo "Starting simple vhost-user server..."
    
    cleanup
    
    # Build the server if it doesn't exist
    if [ ! -f "simple_vhost_server" ]; then
        gcc -Wall -Wextra -std=c99 -O2 -o simple_vhost_server simple_vhost_server.c
    fi
    
    # Start the server in background
    ./simple_vhost_server "$SOCKET_PATH" > "$LOG_FILE" 2>&1 &
    SERVER_PID=$!
    
    echo $SERVER_PID > "$PID_FILE"
    
    # Wait for socket to be created
    echo "Waiting for vhost-user socket to be ready..."
    for i in {1..10}; do
        if [ -S "$SOCKET_PATH" ]; then
            echo "Simple vhost-user server started successfully!"
            echo "Socket path: $SOCKET_PATH"
            echo "PID: $SERVER_PID"
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

stop_server() {
    cleanup
    echo "Simple vhost-user server stopped."
}

case "$1" in
    start)
        start_server
        ;;
    stop)
        stop_server
        ;;
    restart)
        stop_server
        start_server
        ;;
    status)
        if [ -f "$PID_FILE" ]; then
            PID=$(cat "$PID_FILE")
            if kill -0 "$PID" 2>/dev/null; then
                echo "Simple vhost-user server is running (PID: $PID)"
                echo "Socket: $SOCKET_PATH"
                ls -la "$SOCKET_PATH" 2>/dev/null || echo "Socket not found"
            else
                echo "Simple vhost-user server is not running (stale PID file)"
            fi
        else
            echo "Simple vhost-user server is not running"
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac