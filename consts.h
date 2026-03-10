#ifndef ERROR_H
#define ERROR_H

#define RECORD_MAX_SIZE 1024
#define CMD_KEY_MAX_SIZE 16
#define CMD_VALUE_MAX_SIZE 512
#define MAX_EVENTS 64

typedef enum {
    SUCCESS,
    LOG_EOF,
    ERROR,
} Result;

typedef unsigned char u8;
typedef unsigned long long u64;
typedef const char* (decoder) (const u8*, size_t);

#endif // ERROR_H
