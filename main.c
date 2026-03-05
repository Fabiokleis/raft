#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#define LOG_IMPLEMENTATION
#define LOG_PERSIST
#include "log.h"
#define COMMAND_IMPLEMENTATION
#include "cmd.h"

#define DB "db.log"


int main(int argc, char **argv) {
    (void) argc;
    (void) argv;
    printf("hello, world!\n");

    Command cmd = command(SET, "key1", "value1");
    print_command(cmd);

    uint8_t* cmd_buffer;
    size_t cmd_buffer_size;
    serialize_command(cmd, &cmd_buffer_size, &cmd_buffer);

    Log* log = log_new(DB);
    Result err;
    // if ((err = log_record(log, record_new(uuid(), 6, (uint8_t*)"value1"))) != SUCCESS) return 1;
    // if ((err = log_record(log, record_new(uuid(), 6, (uint8_t*)"value2"))) != SUCCESS) return 1;
    if ((err = log_record(log, record_new(uuid(), cmd_buffer_size, cmd_buffer))) != SUCCESS) return 1;

    print_document(log->document);

    log_free(log);
    
    Log* log_loaded = log_new(DB);
    printf("loading %s\n", DB);
    if ((err = log_load(log_loaded)) != SUCCESS) return 1;

    print_document(log_loaded->document);

    log_free(log_loaded);

    return 0;
}
