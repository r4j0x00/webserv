CC = gcc
all: main
CFLAGS = -pthread -O3
LDFLAGS = -lz

main: src/webserv.c src/main.c src/sock.c src/hashmap.c src/misc.c
	$(CC) $(CFLAGS) -o bin/main $^ $(LDFLAGS)
clean:
	rm -f bin/main
