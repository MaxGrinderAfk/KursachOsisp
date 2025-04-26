
CC = g++

CFLAGS_DEBUG = -Wall -Wextra -pedantic -g -MMD -MP -std=c++20
CFLAGS_RELEASE = -Wall -Wextra -pedantic -O2 -MMD -MP -std=c++20

CFLAGS = $(CFLAGS_RELEASE)

SRC_DIR = src
OBJ_DIR = test

SRC = $(wildcard $(SRC_DIR)/*.cpp)
OBJ = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRC))

TARGET = bTreeAlloc

all: $(TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJ) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

DEP = $(OBJ:.o=.d)
-include $(DEP)

clean:
	rm -rf $(OBJ_DIR) Debug $(SRC_DIR)/Debug $(TARGET)

memcheck: debug
	valgrind --leak-check=full \
	--show-leak-kinds=all \
	--track-origins=yes \
	./$(TARGET)

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: clean $(TARGET)

release: CFLAGS = $(CFLAGS_RELEASE)
release: clean $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean debug release run

