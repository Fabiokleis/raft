CC = clang
CFLAGS = -Wall -Wextra -Werror -Wpedantic -Wshadow -fsanitize=address -g
LDFLAGS = -lm

all: raft
.PHONY: all

run: raft
	./raft

main.o: main.c consts.h log.h cmd.h state.h raft.h
	$(CC) $(CFLAGS) -c main.c 

raft: main.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o raft main.o

clean:
	rm -f raft *.o db.*
