CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11
LDFLAGS = -lcurl

SRC_DIR = shell
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:.c=.o)
BIN = axon

.PHONY: all build clean docker run test

all: build

build: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

TEST_DIR = tests
UNITY_SRC = $(TEST_DIR)/unity.c

test_parser: $(SRC_DIR)/parser.o $(TEST_DIR)/test_parser.c $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $(TEST_DIR)/$@ $(TEST_DIR)/test_parser.c $(SRC_DIR)/parser.o $(UNITY_SRC)

test_executor: $(SRC_DIR)/parser.o $(SRC_DIR)/executor.o $(TEST_DIR)/test_executor.c $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $(TEST_DIR)/$@ $(TEST_DIR)/test_executor.c $(SRC_DIR)/executor.o $(SRC_DIR)/parser.o $(UNITY_SRC)

test_builtins: $(SRC_DIR)/builtins.o $(TEST_DIR)/test_builtins.c $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $(TEST_DIR)/$@ $(TEST_DIR)/test_builtins.c $(SRC_DIR)/builtins.o $(UNITY_SRC)

clean:
	rm -f $(SRC_DIR)/*.o $(BIN) $(TEST_DIR)/test_parser $(TEST_DIR)/test_executor $(TEST_DIR)/test_builtins

docker:
	docker build -t axon -f docker/Dockerfile .

run: build
	./$(BIN)

test: test_parser test_executor test_builtins
	@echo "=== Parser Tests ==="
	@./$(TEST_DIR)/test_parser
	@echo ""
	@echo "=== Executor Tests ==="
	@./$(TEST_DIR)/test_executor
	@echo ""
	@echo "=== Builtin Tests ==="
	@./$(TEST_DIR)/test_builtins
