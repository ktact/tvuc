#!/bin/bash

# QEMU Vhost-User Test Runner
# This script provides convenient commands to run various test scenarios

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_dependencies() {
    log_info "Checking dependencies..."
    
    if ! command -v qemu-system-x86_64 &> /dev/null; then
        log_error "qemu-system-x86_64 not found"
        echo "Please install QEMU:"
        echo "  Ubuntu/Debian: sudo apt-get install qemu-system-x86"
        echo "  CentOS/RHEL: sudo yum install qemu-system-x86"
        echo "  Fedora: sudo dnf install qemu-system-x86"
        exit 1
    fi
    
    if ! command -v gcc &> /dev/null; then
        log_error "gcc not found. Please install build-essential or gcc"
        exit 1
    fi
    
    log_info "All dependencies found"
}

build_tests() {
    log_info "Building test executables..."
    
    make clean
    make all
    
    if [ ! -f "test_vhost_user_qemu" ]; then
        gcc -Wall -Wextra -std=c99 -O2 -o test_vhost_user_qemu test_vhost_user_qemu.c
    fi
    
    log_info "Build completed"
}

run_basic_tests() {
    log_info "Running basic unit tests..."
    ./test_vhost_user_client
}

run_qemu_tests() {
    log_info "Running QEMU integration tests..."
    
    # Ensure we have proper permissions
    if [ ! -x "./start_qemu_vhost_server.sh" ]; then
        chmod +x "./start_qemu_vhost_server.sh"
    fi
    
    ./test_vhost_user_qemu
}

run_performance_tests() {
    log_info "Running performance tests with QEMU..."
    
    # Start QEMU server
    ./start_qemu_vhost_server.sh start
    
    sleep 3
    
    log_info "Testing connection latency..."
    time ./vhost_user_client /tmp/vhost-user-test-sock
    
    log_info "Testing multiple rapid connections..."
    for i in {1..5}; do
        echo "Connection attempt $i:"
        time ./vhost_user_client /tmp/vhost-user-test-sock
        sleep 1
    done
    
    # Stop QEMU server
    ./start_qemu_vhost_server.sh stop
}

run_stress_tests() {
    log_info "Running stress tests with QEMU..."
    
    # Start QEMU server
    ./start_qemu_vhost_server.sh start
    
    sleep 3
    
    log_info "Running 10 concurrent client connections..."
    for i in {1..10}; do
        ./vhost_user_client /tmp/vhost-user-test-sock &
    done
    
    wait
    
    # Stop QEMU server
    ./start_qemu_vhost_server.sh stop
}

cleanup_all() {
    log_info "Cleaning up..."
    
    # Stop any running QEMU instances
    ./start_qemu_vhost_server.sh stop 2>/dev/null || true
    
    # Clean build artifacts
    make clean
    
    # Remove test files
    rm -f /tmp/qemu-vhost-test.pid
    rm -f /tmp/qemu-vhost-test.log
    rm -f /tmp/vhost-user-test-sock
    
    log_info "Cleanup completed"
}

show_usage() {
    echo "QEMU Vhost-User Test Runner"
    echo ""
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  check      - Check system dependencies"
    echo "  build      - Build all test executables"
    echo "  basic      - Run basic unit tests (no QEMU required)"
    echo "  qemu       - Run QEMU integration tests"
    echo "  perf       - Run performance tests with QEMU"
    echo "  stress     - Run stress tests with QEMU"
    echo "  all        - Run all tests (basic + qemu)"
    echo "  clean      - Clean up all test artifacts"
    echo "  help       - Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 check          # Check dependencies"
    echo "  $0 build          # Build tests"
    echo "  $0 basic          # Run unit tests"
    echo "  $0 qemu           # Run QEMU tests"
    echo "  $0 all            # Run all tests"
}

# Main execution
case "${1:-help}" in
    check)
        check_dependencies
        ;;
    build)
        check_dependencies
        build_tests
        ;;
    basic)
        check_dependencies
        build_tests
        run_basic_tests
        ;;
    qemu)
        check_dependencies
        build_tests
        run_qemu_tests
        ;;
    perf)
        check_dependencies
        build_tests
        run_performance_tests
        ;;
    stress)
        check_dependencies
        build_tests
        run_stress_tests
        ;;
    all)
        check_dependencies
        build_tests
        run_basic_tests
        echo ""
        run_qemu_tests
        ;;
    clean)
        cleanup_all
        ;;
    help|*)
        show_usage
        ;;
esac