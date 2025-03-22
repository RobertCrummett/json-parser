CC       := cc
CPPFLAGS := -MMD -MP
CFLAGS   := -Wall -Wextra -Wpedantic -ggdb
LDFLAGS  :=
LDLIBS   := -lm

BIN_DIR := build
EXE     := $(BIN_DIR)/MyApp

SRC := main.c json.c
OBJ := main.o json.o

.PHONY: all clean

all: $(EXE)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC) | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	@$(RM) -rv $(EXE) *.o *.d

-include $(OBJ:.o=.d)
