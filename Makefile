CC = clang
CFLAGS = -Wall -Wextra -Werror -Wpedantic -g
LDFLAGS = -lm

all: raft
.PHONY: all

main.o: main.c consts.h log.h cmd.h
	$(CC) $(CFLAGS) -c main.c 

raft: main.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o raft main.o
	
clean:
	rm -f raft *.o db.*
