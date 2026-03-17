#include "consts.h"

#include <stdio.h>
#define LOG_IMPLEMENTATION
#define LOG_PERSIST
#define FSYNC
#include "log.h"

#define COMMAND_IMPLEMENTATION
#include "cmd.h"

#define STATE_IMPLEMENTATION
#include "state.h"

#define RAFT_IMPLEMENTATION
#include "raft.h"

#define DB "db.log"
#define TERM 1
#define PORT 4120

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    Result err;

    RaftNode *node = raft_node_new(1, FOLLOWER, TERM);
    raft_node_print(node);
    Command cmd = command(SET, "key1", "value1");
    // print_command(cmd);

    u8 *cmd_buffer;
    size_t cmd_buffer_size;
    if ((err = serialize_command(cmd, &cmd_buffer_size, &cmd_buffer)) != SUCCESS) return 1;

    Log *log = log_new(DB);
    printf("loading %s\n", DB);
    
    if ((err = log_load(log)) != SUCCESS) return 1;

    printf("\nlog: \n");
    print_document(log->document, decoder_command);

    if (log->document->size == 0) {
        if ((err = log_record(log, record_new(lid(log), TERM, cmd_buffer_size, cmd_buffer))) != SUCCESS) return 1;
        printf("log: \n");
        print_document(log->document, decoder_command);
    }

    StateM *state = state_new(1024);

    if ((err = state_load(state, log)) != SUCCESS) return 1;
    printf("\n\nMEMORY STATE: \n");
    state_debug(state);

    Command cmd2 = command(SET, "chaves", "hugo");
    if (state_apply(state, &cmd2) != SUCCESS) {
        return 1;
    }

    Command cmd3 = command(SET, "key2", "6969");
    
     if (state_apply(state, &cmd3) != SUCCESS) {
        return 1;
    }

    Command cmd4 = command(SET, "chaves", "4i20");
    if (state_apply(state, &cmd4) != SUCCESS) {
        return 1;
    }

    printf("\n\nMEMORY STATE AFTER APPLYING COMMANDS: \n");
    state_debug(state);

    if ((err =loop(node, log, PORT)) != SUCCESS) return 1;

    raft_node_free(node);
    state_free(state);
    log_free(log);

    return 0;
}
