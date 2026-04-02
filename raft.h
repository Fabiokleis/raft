#ifndef RAFT_H
#define RAFT_H 

typedef enum {
    FOLLOWER,
    CANDIDATE,
    LEADER,
    MAX_ROLES
} NodeRole;

static const char *ROLES[] = {
    [FOLLOWER] = "follower",
    [CANDIDATE] = "candidate",
    [LEADER] = "leader",
    [MAX_ROLES] = NULL,
};

typedef struct {
    u64 id;
    NodeRole role;
    u64 current_term;
    u64 voted_for;
    u64 commit_index;
    u64 last_applied;
} RaftNode;

typedef enum {
    REQUEST_VOTE,
    REQUEST_VOTE_REPLY,
    APPEND_ENTRIES,
    APPEND_ENTRIES_REPLY,
    MAX_HEADERS,
} RaftPacketType;

static const char *HEADERS[] = {
    [REQUEST_VOTE] = "request vote",
    [REQUEST_VOTE_REPLY] = "request vote reply",
    [APPEND_ENTRIES] = "append entries",
    [APPEND_ENTRIES_REPLY] = "append entries reply",
    [MAX_HEADERS] = NULL,
};

typedef struct {
    RaftPacketType type;
    size_t payload_size;
} RaftPacketHeader;

RaftNode* raft_node_new(u64 id, NodeRole role, u64 current_term);
void raft_node_free(RaftNode *node);
void raft_node_print(RaftNode *node);
Result loop(RaftNode *node, Log *log, int port);

#ifdef RAFT_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

RaftNode* raft_node_new(u64 id, NodeRole role, u64 current_term) {
    RaftNode *node = (RaftNode *) malloc(sizeof(RaftNode));
    node->id = id;
    node->role = role;
    node->current_term = current_term;
    return node;
}

void raft_node_print(RaftNode *node) {
    assert(node != NULL);
    printf("[*] { id: %llu, term: %llu, role: %s }\n", node->id, node->current_term, ROLES[node->role]);
}

void raft_node_free(RaftNode *node) {
    assert(node != NULL);
    free(node);
}

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int init_server(int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port)
    };

    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    set_nonblocking(listen_fd);
    listen(listen_fd, SOMAXCONN);

    return listen_fd;
}

static void handle_append_entries(int client_fd, RaftPacketHeader *header, RaftNode *node, Log *log) {
    (void) client_fd;
    (void) header;
    (void) node;
    (void) log;
    assert("TODO");
}

static void handle_append_entries_reply(int client_fd, RaftPacketHeader *header, RaftNode *node, Log *log) {
    (void) client_fd;
    (void) header;
    (void) node;
    (void) log;
    assert("TODO");
}

static void handle_request_vote(int client_fd, RaftPacketHeader *header, RaftNode *node, Log *log) {
    (void) client_fd;
    (void) header;
    (void) node;
    (void) log;
    assert("TODO");
}

static void handle_request_vote_reply(int client_fd, RaftPacketHeader *header, RaftNode *node, Log *log) {
    (void) client_fd;
    (void) header;
    (void) node;
    (void) log;
    assert("TODO");
}

static void handle_raft_packet(int client_fd, RaftNode *node, Log *log) {
    RaftPacketHeader header;
    
    ssize_t bytes_read = recv(client_fd, &header, sizeof(RaftPacketHeader), 0);
    
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            printf("[*] node %d disconnected.\n", client_fd);
        } else if (errno != EAGAIN) {
            fprintf(stderr, "failed to recv, no data available %s\n", strerror(errno));
        }
        close(client_fd);
        return;
    }

    if (header.type >= 0 && header.type <= MAX_HEADERS) {
        printf("[*] read packet header: %s\n", HEADERS[header.type]);
    }

    switch (header.type) {
        case APPEND_ENTRIES: {
            handle_append_entries(client_fd, &header, node, log);
        } break;
        case APPEND_ENTRIES_REPLY: {
            handle_append_entries_reply(client_fd, &header, node, log);
        } break;
        case REQUEST_VOTE: {
            handle_request_vote(client_fd, &header, node, log);
        } break;
        case REQUEST_VOTE_REPLY: {
            handle_request_vote_reply(client_fd, &header, node, log);
        } break;
        case MAX_HEADERS:
        default: {
            printf("[*] read packet with invalid header: %u\n", header.type);
            break;
        }
    }
}

Result loop(RaftNode *node, Log *log, int port) {
    assert(node != NULL);
    assert(log != NULL);

    int listen_fd = init_server(port);
    int epoll_fd = epoll_create1(0);

    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    printf("[*] database node started, listening %d ....\n", port);

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == listen_fd) {
                struct sockaddr_in client_addr;
                socklen_t addrlen = sizeof(client_addr);
                int conn_sock = accept(listen_fd, (struct sockaddr*)&client_addr, &addrlen);
                
                set_nonblocking(conn_sock);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_sock;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock, &ev);
                
            } else {
                handle_raft_packet(events[n].data.fd, node, log);
            }
        }
    }
    return SUCCESS;
}


#endif

#endif // RAFT_H
