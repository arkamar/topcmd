CFLAGS ?= -O2 -pipe
CFLAGS += -Wall -pedantic

BIN = topcmd

.PHONY: all
all: $(BIN)

.PHONY: all
clean:
	$(RM) $(BIN)
