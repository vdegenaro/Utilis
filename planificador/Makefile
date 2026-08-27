##############################################
# Makefile - Planificador (EntrenadOS)
##############################################

CC = gcc
CFLAGS = -Wall -Wextra -g -pthread -std=gnu11
LDFLAGS = -lcommons -lpthread

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
TARGET = $(BIN_DIR)/planificador

.PHONY: all clean run directories

all: directories $(TARGET)

directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build OK -> $(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) planificador.log

# Ejecuta con el config y job de ejemplo. Ajustar paths según se necesite.
run: all
	./$(TARGET) config/planificador.config config/job_ejemplo.txt
