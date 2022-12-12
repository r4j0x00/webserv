CC = gcc
all: main
CFLAGS = -pthread -O3
LDFLAGS = -lz

main: webserv.c main.c sock.c hashmap.c misc.c
	$(CC) $(CFLAGS) -o bin/main $^ $(LDFLAGS) 
clean:
	rm -f bin/main
