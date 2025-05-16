# Compiler and linker options
IEC_SRV_BASE=./

LIBIEC_HOME=$(IEC_SRV_BASE)/LIB/libiec61850-1.5.1
LIBIEC_RECIPE=$(LIBIEC_HOME)/build
LIB_IEC=$(LIBIEC_RECIPE)/libiec61850.a

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


CC = gcc
CFLAGS = -I./INC $(IEC_SRV_INC_MOD) -Wall  -g -I$(INCDIR) -I/usr/include/libxml2
LDFLAGS = -lrt -lpthread -lm -lxml2 -lssl -lcrypto


# Directories
SRCDIR = SRC
BINDIR = BIN
INCDIR = INC



# Source and object files
#SRC = $(wildcard $(SRCDIR)/*.c)
#OBJ = $(SRC:$(SRCDIR)/%.c=$(BINDIR)/%.o)


# Source and object files for TARGET1 (simuStpmDualChBrutVal)
SRC1 = $(SRCDIR)/ComCalSinCos.c $(SRCDIR)/main.c $(SRCDIR)/parser.c
OBJ1 = $(BINDIR)/ComCalSinCos.o $(BINDIR)/main.o $(BINDIR)/parser.o

# Source and object files for TARGET2 (simuStpmMonoChBrutVal)
SRC2 = $(SRCDIR)/ComCalSinCos.c $(SRCDIR)/tstVdpaSVPublisherWithScenario.c $(SRCDIR)/parser.c
OBJ2 = $(BINDIR)/ComCalSinCos.o $(BINDIR)/tstVdpaSVPublisherWithScenario.o $(BINDIR)/parser.o



# Target executable
TARGET1 = $(BINDIR)/simuStpmDualChBrutVal

# Target executable
TARGET2 = $(BINDIR)/simuStpmMonoChBrutVal

# Rules
all: $(TARGET1) $(TARGET2)

$(TARGET1): $(OBJ1) $(LIB_IEC)
	$(CC) -o $@ $(OBJ1) $(LIB_IEC) $(LDFLAGS)

$(TARGET2): $(OBJ2) $(LIB_IEC)
	$(CC) -o $@ $(OBJ2) $(LIB_IEC) $(LDFLAGS)


$(BINDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_IEC): 
	cd $(LIBIEC_HOME) && make all

clean:
	rm -f $(BINDIR)/*.o $(TARGET1) $(TARGET2)

.PHONY: all clean

