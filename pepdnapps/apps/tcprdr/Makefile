all: tcprdr
CC=gcc
CFLAGS=-W -Wall -pedantic -std=gnu99

tcprdr: tcprdr.c
	$(CC) $(CFLAGS) tcprdr.c -o tcprdr

install:
	sudo cp tcprdr /usr/bin/

clean:
	rm -f tcprdr
	sudo rm -f /usr/bin/tcprdr
