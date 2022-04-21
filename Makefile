CC = gcc
CFLAGS = -pthread -lcrypto

default: all

all: firmware

firmware: plc.c
	$(CC) -o firmware.bin plc.c $(CFLAGS) 

clean:
	rm -f firmare.bin