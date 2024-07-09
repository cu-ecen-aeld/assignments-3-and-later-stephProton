CC=$(CROSS_COMPILE)gcc

.PHONY: clean

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) -g -o '$@' '$<'

clean:
	rm -rf aesdsocket

