CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGET = vhost_user_client
SOURCE = vhost_user_client.c
TEST_TARGET = test_vhost_user_client
TEST_SOURCE = test_vhost_user_client.c
QEMU_TEST_TARGET = test_vhost_user_qemu
QEMU_TEST_SOURCE = test_vhost_user_qemu.c
SIMPLE_SERVER_TARGET = simple_vhost_server
SIMPLE_SERVER_SOURCE = simple_vhost_server.c

all: $(TARGET) $(TEST_TARGET) $(QEMU_TEST_TARGET) $(SIMPLE_SERVER_TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

$(TEST_TARGET): $(TEST_SOURCE)
	$(CC) $(CFLAGS) -pthread -o $(TEST_TARGET) $(TEST_SOURCE)

$(QEMU_TEST_TARGET): $(QEMU_TEST_SOURCE)
	$(CC) $(CFLAGS) -o $(QEMU_TEST_TARGET) $(QEMU_TEST_SOURCE)

$(SIMPLE_SERVER_TARGET): $(SIMPLE_SERVER_SOURCE)
	$(CC) $(CFLAGS) -o $(SIMPLE_SERVER_TARGET) $(SIMPLE_SERVER_SOURCE)

test: $(TARGET) $(TEST_TARGET)
	./$(TEST_TARGET)

qemu-test: $(TARGET) $(QEMU_TEST_TARGET) $(SIMPLE_SERVER_TARGET)
	./$(QEMU_TEST_TARGET)

test-all: test qemu-test

clean:
	rm -f $(TARGET) $(TEST_TARGET) $(QEMU_TEST_TARGET) $(SIMPLE_SERVER_TARGET)

.PHONY: clean test qemu-test test-all all