CC=$(CROSS_COMPILE)gcc

.PHONY: clean

all: aesdsocket client

default: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) -g -o '$@' '$<'

client: client.c
	$(CC) -g -o '$@' '$<'

clean:
	rm -rf aesdsocket client 

