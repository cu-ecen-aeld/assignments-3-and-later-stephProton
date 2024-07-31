CC ?= $(CROSS_COMPILE)gcc
LDFLAGS ?= -lpthread -lrt

.PHONY: clean

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) -g -o '$@' '$<' $(LDFLAGS)

clean:
	rm -rf aesdsocket

