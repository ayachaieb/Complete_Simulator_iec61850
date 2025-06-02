# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -I$(INC_DIR)
LDFLAGS = -lm -lcjson

# Directory structure
SRC_DIR = ../SRC
INC_DIR = ../INC
OBJ_DIR = ../OBJ
BIN_DIR = ../BIN

# Exclusion list
EXCLUDE_SRC = goose_publisher.c goose_receiver.c parser.c sv_publisher.c main.c
EXCLUDE_HDR = goose_publisher.h goose_receiver.h parser.h sv_publisher.h

# Source files
SRC = $(filter-out $(addprefix $(SRC_DIR)/,$(EXCLUDE_SRC)), $(wildcard $(SRC_DIR)/*.c))
HDR = $(filter-out $(addprefix $(INC_DIR)/,$(EXCLUDE_HDR)), $(wildcard $(INC_DIR)/*.h))

# Object files
OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))

# Ensure directories exist
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR))

# Default target (release build)
all: release

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: $(BIN_DIR)/sv_simulator

# Release build
release: CFLAGS += -O2 -DNDEBUG
release: $(BIN_DIR)/sv_simulator

# Main executable
$(BIN_DIR)/sv_simulator: $(OBJ)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Object files rule
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HDR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean targets
clean:
	rm -rf $(OBJ_DIR)/*.o $(BIN_DIR)/sv_simulator

.PHONY: all debug release clean