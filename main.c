#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define LOG_IMPLEMENTATION
#define LOG_PERSIST
#include "log.h"

#define DB "db.log"

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;
    printf("hello, world!\n");

    Log* log = log_new(DB);
    Error err;
    if ((err = log_record(log, record_new(uuid(), 6, (uint8_t*)"value1"))) != LOG_SUCCESS) return 1;
    // if ((err = log_record(log, record_new(uuid(), 6, (uint8_t*)"value2"))) != LOG_SUCCESS) return 1;

    print_document(log->document);

    log_free(log);
    
    Log* log_loaded = log_new(DB);
    printf("loading %s\n", DB);
    if ((err = log_load(log_loaded)) != LOG_SUCCESS) return 1;

    print_document(log_loaded->document);

    log_free(log_loaded);

    return 0;
}
