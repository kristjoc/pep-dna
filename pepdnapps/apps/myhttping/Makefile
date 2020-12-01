CC=gcc
CFLAGS=-g -Wall

all:
	$(CC) $(CFLAGS) myhttping.c -o myhttping -lm -lpthread
install:
	sudo cp myhttping /usr/bin/
	sudo mkdir /var/www/web
clean:
	sudo rm -f myhttping
	sudo rm -rf myhttping.dSYM
	sudo rm -f /usr/bin/myhttping
