
CC = gcc
CC_OPTS = -std=gnu99 -Wall

BIN = bin
SRC = src

$(BIN):
	mkdir $(BIN)

$(BIN)/%: $(SRC)/%.c $(BIN)
	$(CC) $(CC_OPTS) -o $@ $<
