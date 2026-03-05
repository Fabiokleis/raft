CC = clang
CFLAGS = -Wall -Wextra -Werror -Wpedantic -g
LDFLAGS = -luuid

all: raft
.PHONY: all

main.o: main.c log.h
	$(CC) $(CFLAGS) -c main.c

raft: main.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o raft main.o
	
clean:
	rm -f raft *.o db.*
