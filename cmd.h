#ifndef CMD_H
#define CMD_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "error.h"

#define CMD_KEY_MAX_SIZE 16
#define CMD_VALUE_MAX_SIZE 512

typedef enum {
    SET,
    MAX_COMMANDS,
} CommandType;

typedef struct {
    CommandType type;
    char key[CMD_KEY_MAX_SIZE];
    char value[CMD_VALUE_MAX_SIZE];
} Command;

static const char *COMMANDS[] = {
    [SET] = "SET",
    NULL,
};

Command command_(CommandType type, const char* key, const char* value);
Result serialize_command(const Command cmd, size_t* out_size, uint8_t** out_buffer);
Result deserialize_command(const uint8_t* buffer, size_t size, Command* out_cmd);
void print_command(const Command cmd);

#ifdef COMMAND_IMPLEMENTATION

Command command(CommandType type, const char* key, const char* value) {
    assert(type < MAX_COMMANDS);
    assert(key != NULL);
    assert(value != NULL);

    Command cmd;
    cmd.type = type;
    strncpy(cmd.key, key, sizeof(cmd.key) - 1);
    cmd.key[sizeof(cmd.key) - 1] = '\0';
    strncpy(cmd.value, value, sizeof(cmd.value) - 1);
    cmd.value[sizeof(cmd.value) - 1] = '\0';
    return cmd;
}

Result serialize_command(const Command cmd, size_t* out_size, uint8_t** out_buffer) {
    size_t key_len = strlen(cmd.key);
    size_t value_len = strlen(cmd.value);
    *out_size = sizeof(CommandType) + sizeof(size_t) + key_len + sizeof(size_t) + value_len;

    uint8_t* buffer = (uint8_t*) malloc(*out_size);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to allocate memory for command serialization\n");
        return ERROR;
    }

    size_t offset = 0;
    memcpy(buffer + offset, &cmd.type, sizeof(CommandType));
    offset += sizeof(CommandType);

    memcpy(buffer + offset, &key_len, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(buffer + offset, cmd.key, key_len);
    offset += key_len;

    memcpy(buffer + offset, &value_len, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(buffer + offset, cmd.value, value_len);

    *out_buffer = buffer;
    return SUCCESS;
}

Result deserialize_command(const uint8_t* buffer, size_t size, Command* out_cmd) {
    if (size < sizeof(CommandType) + 2 * sizeof(size_t)) {
        fprintf(stderr, "Buffer too small for command deserialization\n");
        return ERROR;
    }

    size_t offset = 0;
    memcpy(&out_cmd->type, buffer + offset, sizeof(CommandType));
    offset += sizeof(CommandType);

    size_t key_len;
    memcpy(&key_len, buffer + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (offset + key_len > size) {
        fprintf(stderr, "Buffer too small for key deserialization\n");
        return ERROR;
    }
    memcpy(out_cmd->key, buffer + offset, key_len);
    out_cmd->key[key_len] = '\0';
    offset += key_len;

    size_t value_len;
    memcpy(&value_len, buffer + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (offset + value_len > size) {
        fprintf(stderr, "Buffer too small for value deserialization\n");
        return ERROR;
    }
    memcpy(out_cmd->value, buffer + offset, value_len);
    out_cmd->value[value_len] = '\0';

    return SUCCESS;
}

void print_command(const Command cmd) {
    printf("Command Type: %s\n", COMMANDS[cmd.type]);
    printf("Key: %s\n", cmd.key);
    printf("Value: %s\n", cmd.value);
}

#endif // COMMAND_IMPLEMENTATION

#endif // CMD_H
