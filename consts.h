#ifndef ERROR_H
#define ERROR_H

#define RECORD_MAX_SIZE 1024

typedef enum {
    SUCCESS,
    LOG_EOF,
    ERROR,
} Result;

typedef unsigned char u8;
typedef unsigned long long log_id_t;

#endif // ERROR_H
