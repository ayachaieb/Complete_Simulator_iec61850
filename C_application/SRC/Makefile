# Compiler settings
CC = gcc
CFLAGS =  -Wextra -g 
# Add include directory
CFLAGS += -IINC
LDFLAGS = -lm -lcjson

# Essential source files
SRC = main.c State_Machine.c Ring_Buffer.c state_idle.c state_init.c state_running.c state_stop.c enum_to_string.c assert_handler.c

# Essential header files
HDR = State_Machine.h Ring_Buffer.h state_idle.h state_init.h state_running.h state_stop.h assert_handler.h enum_to_string.h

# Object files
OBJ = $(SRC:.c=.o)

# Default target
all: sv_simulator

# sv_simulator executable
sv_simulator: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $@

# Object files rule
%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean target
clean:
	rm -f $(OBJ) sv_simulator

# Dependencies
.PHONY: all clean