#ifndef STATE_H
#define STATE_H

#include <stdbool.h>
#include "consts.h"
#include "cmd.h"

typedef struct {
    char key[CMD_KEY_MAX_SIZE];
    char value[CMD_VALUE_MAX_SIZE];
    bool active;
} Entry;

typedef struct {
    Entry* entries;
    size_t size;
    size_t capacity;
} StateM;

StateM* state_new(u64 capacity);
Result state_load(StateM* state, const Log* log);
Result state_apply(StateM* state, const Command* cmd);
Result state_debug(StateM* state);
Result state_free(StateM* state);

#ifdef STATE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

StateM* state_new(u64 capacity) {
    StateM* state = (StateM *) malloc(sizeof(StateM));
    state->entries = (Entry *) malloc(capacity * sizeof(Entry));
    state->size = 0;
    state->capacity = capacity;
    return state;
}

static Entry* state_find_entry(StateM* state, const char* key) {
    assert(state != NULL);
    assert(key != NULL);
    for (size_t i = 0; i < state->size; i++) {
        if (strcmp(state->entries[i].key, key) == 0) {
            return &state->entries[i];
        }
    }
    return NULL;
}

static Result state_realloc(StateM* state) {
    if (state->size >= state->capacity) {
        size_t new_capacity = (state->capacity == 0) ? 1 : state->capacity * 2;
        Entry* new_entries = (Entry *) realloc(state->entries, new_capacity * sizeof(Entry));
        if (new_entries == NULL) {
            fprintf(stderr, "Failed to allocate memory for state entries\n");
            return ERROR;
        }
        state->entries = new_entries;
        state->capacity = new_capacity;
    }
    return SUCCESS;
}

static Result state_set(StateM* state, const char* key, const char* value) {
    if (state_realloc(state) != SUCCESS) return ERROR;

    strncpy(state->entries[state->size].key, key, CMD_KEY_MAX_SIZE - 1);
    state->entries[state->size].key[CMD_KEY_MAX_SIZE - 1] = '\0';

    strncpy(state->entries[state->size].value, value, CMD_VALUE_MAX_SIZE - 1);
    state->entries[state->size].value[CMD_VALUE_MAX_SIZE - 1] = '\0';
    state->entries[state->size].active = true;
    state->size++;
    
    return SUCCESS;
}

Result state_load(StateM* state, const Log* log) {
    assert(state != NULL);
    assert(log != NULL);

    for (size_t i = 0; i < log->document->size; i++) {
        Record* record = &log->document->records[i];
        Command cmd;
        if (deserialize_command(record->value, record->size, &cmd) != SUCCESS) {
            fprintf(stderr, "Failed to deserialize command from log record\n");
            return ERROR;
        }
        if (state_apply(state, &cmd) != SUCCESS) {
            fprintf(stderr, "Failed to apply command to state\n");
            return ERROR;
        }
    }
    return SUCCESS;
}

Result state_apply(StateM* state, const Command* cmd) {
    assert(state != NULL);
    assert(cmd != NULL);

    Entry *e = state_find_entry(state, cmd->key);
    if (NULL != e) {
        strncpy(e->value, cmd->value, CMD_VALUE_MAX_SIZE - 1);
        e->value[CMD_VALUE_MAX_SIZE - 1] = '\0';
        e->active = true;
        return SUCCESS;
    } 
    return state_set(state, cmd->key, cmd->value);
}

Result state_debug(StateM* state) {
    assert(state != NULL);
    for (size_t i = 0; i < state->size; i++) {
        if (state->entries[i].active) {
            printf("%s: %s\n", state->entries[i].key, state->entries[i].value);
        }
    }
    return SUCCESS;
}

Result state_free(StateM* state) {
    assert(state != NULL);
    free(state->entries);
    free(state);
    return SUCCESS;
}


#endif // STATE_IMPLEMENTATION

#endif // STATE_H
