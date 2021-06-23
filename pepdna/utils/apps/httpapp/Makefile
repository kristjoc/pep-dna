CC=gcc
CFLAGS=-g -Wall

all:
	$(CC) $(CFLAGS) main.c httpapp.c -o httpapp -lpthread
install:
	sudo cp httpapp /usr/bin/
	sudo mkdir -p /var/www/web
clean:
	rm -f httpapp
	rm -rf httpapp.dSYM
	rm -f /usr/bin/httpapp
