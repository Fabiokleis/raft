#include <stdio.h>
#include "consts.h"

#define LOG_IMPLEMENTATION
#define LOG_PERSIST
#define FSYNC
#include "log.h"

#define COMMAND_IMPLEMENTATION
#include "cmd.h"

#define STATE_IMPLEMENTATION
#include "state.h"

#define DB "db.log"


int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    Result err;

    Command cmd = command(SET, "key1", "value1");
    // print_command(cmd);

    u8* cmd_buffer;
    size_t cmd_buffer_size;
    if ((err = serialize_command(cmd, &cmd_buffer_size, &cmd_buffer)) != SUCCESS) return 1;

    Log* log = log_new(DB);
    printf("loading %s\n", DB);
    
    if ((err = log_load(log)) != SUCCESS) return 1;

    printf("\nLOG: \n");
    print_document(log->document, decoder_command);

    if (log->document->size == 0) {
        if ((err = log_record(log, record_new(id(log), cmd_buffer_size, cmd_buffer))) != SUCCESS) return 1;
        printf("LOG: \n");
        print_document(log->document, decoder_command);
    }

    StateM* state = state_new(1024);

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

    state_free(state);
    log_free(log);

    return 0;
}
