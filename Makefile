CC = gcc
all: main
CFLAGS = -pthread -O3
LDFLAGS = -lz

main: src/webserv.c src/main.c src/sock.c src/hashmap.c src/misc.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o bin/main $^ 
clean:
	rm -f bin/main
