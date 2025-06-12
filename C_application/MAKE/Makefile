# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -I$(INC_DIR)

# IEC 61850 specific includes and libraries

IEC_SRV_BASE=../
LIBIEC_HOME=$(IEC_SRV_BASE)/LIB/libiec61850-1.5.1
LIBIEC_RECIPE=$(LIBIEC_HOME)/build
LIB_IEC=$(LIBIEC_RECIPE)/libiec61850.a # Path to the static library

# Include directories for libiec61850
IEC_SRV_INC_MOD = \
    -I$(LIBIEC_HOME)/config \
    -I$(LIBIEC_HOME)/hal/inc \
    -I$(LIBIEC_HOME)/src/common/inc \
    -I$(LIBIEC_HOME)/src/mms/inc \
    -I$(LIBIEC_HOME)/src/mms/inc_private \
    -I$(LIBIEC_HOME)/src/mms/asn1 \
    -I$(LIBIEC_HOME)/src/iec61850/inc \
    -I$(LIBIEC_HOME)/src/iec61850/inc_private \
    -I$(LIBIEC_HOME)/src/goose \
    -I$(LIBIEC_HOME)/src/sampled_values \
    -I$(LIBIEC_HOME)/src/logging \
    -I$(LIBIEC_HOME)/src/tls \
    -I$(LIBIEC_HOME)/include/libiec61850

CFLAGS += $(IEC_SRV_INC_MOD) -I/usr/include/libxml2

LDFLAGS = -lm -lcjson -lrt -lpthread -lxml2 -lssl -lcrypto $(LIB_IEC)
# Directory structure
SRC_DIR = ../SRC
INC_DIR = ../INC
OBJ_DIR = ../OBJ
BIN_DIR = ../BIN

# Exclusion list
EXCLUDE_SRC = goose_publisher.c goose_receiver.c
EXCLUDE_HDR = goose_publisher.h goose_receiver.h 

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

# Main executable target
# Depends on all object files and the libiec61850 static library
$(BIN_DIR)/sv_simulator: $(OBJ) $(LIB_IEC)
	@echo "Linking $@"
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Object files rule
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HDR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Rule to build libiec61850.a if it doesn't exist
$(LIB_IEC):
	@echo "Building libiec61850..."
	cd $(LIBIEC_HOME) && $(MAKE) all

# Clean targets
clean:
	rm -rf $(OBJ_DIR)/*.o $(BIN_DIR)/sv_simulator

.PHONY: all debug release clean