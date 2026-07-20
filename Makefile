CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11

CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17

LDFLAGS = -lcurl -lsqlite3 -lstdc++

SRC_DIR = shell
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:.c=.o)

CTX_DIR = context
CTX_SRC = $(wildcard $(CTX_DIR)/*.cpp)
CTX_OBJ = $(CTX_SRC:.cpp=.o)

SBX_DIR = sandbox
SBX_SRC = $(wildcard $(SBX_DIR)/*.c)
SBX_OBJ = $(SBX_SRC:.c=.o)

BIN = axon

.PHONY: all build clean docker run test

all: build

# Sandbox objects are built (not yet linked into the shell) so warnings surface
build: $(BIN) $(SBX_OBJ)

$(BIN): $(OBJ) $(CTX_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(CTX_DIR)/%.o: $(CTX_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(SBX_DIR)/%.o: $(SBX_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

TEST_DIR = tests
UNITY_SRC = $(TEST_DIR)/unity.c

test_parser: $(SRC_DIR)/parser.o $(TEST_DIR)/test_parser.c $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $(TEST_DIR)/$@ $(TEST_DIR)/test_parser.c $(SRC_DIR)/parser.o $(UNITY_SRC)

test_executor: $(SRC_DIR)/parser.o $(SRC_DIR)/executor.o $(TEST_DIR)/test_executor.c $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $(TEST_DIR)/$@ $(TEST_DIR)/test_executor.c $(SRC_DIR)/executor.o $(SRC_DIR)/parser.o $(UNITY_SRC)

test_builtins: $(SRC_DIR)/builtins.o $(TEST_DIR)/test_builtins.c $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $(TEST_DIR)/$@ $(TEST_DIR)/test_builtins.c $(SRC_DIR)/builtins.o $(UNITY_SRC)

test_storage: $(CTX_DIR)/storage.o $(TEST_DIR)/test_storage.cpp
	$(CXX) $(CXXFLAGS) -o $(TEST_DIR)/$@ $(TEST_DIR)/test_storage.cpp $(CTX_DIR)/storage.o -lsqlite3

test_context: $(CTX_OBJ) $(TEST_DIR)/test_context.c $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $(TEST_DIR)/$@ $(TEST_DIR)/test_context.c $(CTX_OBJ) $(UNITY_SRC) -lsqlite3 -lstdc++

test_git_context: $(CTX_DIR)/git_context.o $(TEST_DIR)/test_git_context.cpp
	$(CXX) $(CXXFLAGS) -o $(TEST_DIR)/$@ $(TEST_DIR)/test_git_context.cpp $(CTX_DIR)/git_context.o

test_sandbox: $(SBX_OBJ) $(TEST_DIR)/test_sandbox.c $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $(TEST_DIR)/$@ $(TEST_DIR)/test_sandbox.c $(SBX_OBJ) $(UNITY_SRC)

clean:
	rm -f $(SRC_DIR)/*.o $(CTX_DIR)/*.o $(SBX_DIR)/*.o $(BIN) $(TEST_DIR)/test_parser $(TEST_DIR)/test_executor $(TEST_DIR)/test_builtins $(TEST_DIR)/test_storage $(TEST_DIR)/test_context $(TEST_DIR)/test_git_context $(TEST_DIR)/test_sandbox

docker:
	docker build -t axon -f docker/Dockerfile .

run: build
	./$(BIN)

test: test_parser test_executor test_builtins test_storage test_context test_git_context test_sandbox
	@echo "=== Parser Tests ==="
	@./$(TEST_DIR)/test_parser
	@echo ""
	@echo "=== Executor Tests ==="
	@./$(TEST_DIR)/test_executor
	@echo ""
	@echo "=== Builtin Tests ==="
	@./$(TEST_DIR)/test_builtins
	@echo ""
	@echo "=== Storage Tests ==="
	@./$(TEST_DIR)/test_storage
	@echo ""
	@echo "=== Context Tests ==="
	@./$(TEST_DIR)/test_context
	@echo ""
	@echo "=== Git Context Tests ==="
	@./$(TEST_DIR)/test_git_context
	@echo ""
	@echo "=== Sandbox Tests ==="
	@./$(TEST_DIR)/test_sandbox
