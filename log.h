#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <uuid/uuid.h>

#define RECORD_MAX_SIZE 1024
#define ID_SIZE UUID_STR_LEN

typedef struct {
    char id[ID_SIZE];
    size_t size;
    uint8_t *value;
} Record;

typedef struct {
    Record* records;
    size_t capacity;
    size_t size;
} Document;

typedef struct {
    const char* filename;
    FILE* file;
    Document* document;
    size_t cursor;
} Log;

typedef enum {
    LOG_SUCCESS,
    LOG_EOF,
    LOG_ERROR,
} Error;

char* uuid(void);
Record* record_new(const char id[ID_SIZE], size_t value_size, uint8_t *value);
Document* document_new(void);
Log* log_new(const char* filename);
void print_document(const Document* document);
Error append_record(Document* document, Record* record);
Error log_record(Log* log, Record* record);
Error log_load(Log* log);
Error log_free(Log* log);

#ifdef LOG_IMPLEMENTATION

#include <string.h>
#include <assert.h>
#include <errno.h>

char* uuid(void) {
    uuid_t binuuid;
    uuid_generate_random(binuuid);
    char *uuid_str = malloc(UUID_STR_LEN);
    uuid_unparse_lower(binuuid, uuid_str);
    return uuid_str;
}

Record* record_new(const char id[ID_SIZE], size_t value_size, uint8_t *value) {
    assert(id != NULL);
    assert(value != NULL);
    assert(value_size <= RECORD_MAX_SIZE - 1);

    Record* record = (Record*) malloc(sizeof(Record));

    memcpy(record->id, id, ID_SIZE);

    record->size = value_size;
    record->value = (uint8_t*) malloc((record->size + 1) * sizeof(uint8_t));

    memcpy(record->value, value, record->size);
    record->value[record->size] = '\0';

    return record;
}

Document* document_new(void) {
    Document* document = (Document*) malloc(sizeof(Document));
    document->records = NULL;
    document->capacity = 0;
    document->size = 0;
    return document;
}

Log* log_new(const char* filename) {
    assert(filename != NULL);

    Log* log = (Log*) malloc(sizeof(Log));
    if ((log->file = fopen(filename, "ab")) == NULL) {
        fprintf(stderr, "Failed to open log file: %s\n", filename);
        free(log);
        exit(1);
    }
    log->filename = filename;
    log->document = document_new();
    log->cursor = 0;
    return log;
}

Error append_record(Document *document, Record *record) {
    assert(document != NULL);
    assert(record != NULL);
    if (document->size >= document->capacity) {
        size_t new_capacity = (document->capacity == 0) ? 1 : document->capacity * 2;
        Record* new_records = (Record*) realloc(document->records, new_capacity * sizeof(Record));
        if (new_records == NULL) {
            fprintf(stderr, "Failed to allocate memory for records\n");
            return LOG_ERROR;
        }
        document->records = new_records;
        document->capacity = new_capacity;
    }
    document->records[document->size++] = *record;
    return LOG_SUCCESS;
}

static void print_record(const Record* record) {
    printf("[*] %s %u %s\n", record->id, record->size, record->value);
}

void print_document(const Document* document) {
    for (size_t i = 0; i < document->size; i++) {
        print_record(&document->records[i]);
    }
}

static Error write_id(Log *log, Record *record) {
    size_t written_id = fwrite(record->id, sizeof(char), ID_SIZE, log->file);
    if (errno != 0) {
        fprintf(stderr, "Error writing record ID to log file: %s\n", strerror(errno));
        return LOG_ERROR;
    }

    if (written_id != ID_SIZE) {
        fprintf(stderr, "Failed to write record ID to log file\n");
        return LOG_ERROR;
    }
    return LOG_SUCCESS;
}

static Error write_value_sz(Log *log, Record *record) {
    size_t written_value_sz = fwrite(&record->size, sizeof(size_t), 1, log->file);
    if (errno != 0) {
        fprintf(stderr, "Error writing record size to log file: %s\n", strerror(errno));
        return LOG_ERROR;
    }

    if (written_value_sz != 1) {
        fprintf(stderr, "Failed to write record size to log file\n");
        return LOG_ERROR;
    }
    return LOG_SUCCESS;
}

static Error write_value(Log *log, Record *record) {
    size_t written_value = fwrite(record->value, sizeof(uint8_t), record->size, log->file);
    if (errno != 0) {
        fprintf(stderr, "Error writing record value to log file: %s\n", strerror(errno));
        return LOG_ERROR;
    }

    if (written_value != record->size) {
        fprintf(stderr, "Failed to write record value to log file\n");
        return LOG_ERROR;
    }
    return LOG_SUCCESS;
}

Error log_record(Log *log, Record *record) {
    assert(log != NULL);
    if (append_record(log->document, record) != LOG_SUCCESS) return LOG_ERROR;

#ifdef LOG_PERSIST
    Error err;
    if ((err = write_id(log, record)) != LOG_SUCCESS) return LOG_ERROR;
    if ((err = write_value_sz(log, record)) != LOG_SUCCESS) return LOG_ERROR;
    if ((err = write_value(log, record)) != LOG_SUCCESS) return LOG_ERROR;

    log->cursor++;
#endif
    return LOG_SUCCESS;
}

static Error reads_id(Log *log, char* id_buff) {
    size_t read_id = fread(id_buff, sizeof(char), ID_SIZE, log->file);

    if (read_id == 0 && feof(log->file)) return LOG_EOF;    

    if (errno != 0) {
        fprintf(stderr, "Error reading record ID from log file: %s\n", strerror(errno));
        return LOG_ERROR;
    }
    
    if (read_id != ID_SIZE) {
        fprintf(stderr, "Error reading record ID from log file: %s\n", strerror(errno));
        return LOG_ERROR;
    }

    id_buff[ID_SIZE - 1] = '\0';
    return LOG_SUCCESS;
}

static Error read_value_size(Log *log, size_t* value_size) {
    size_t read_value_sz = fread(value_size, sizeof(size_t), 1, log->file);
    if (errno != 0) {
        fprintf(stderr, "Error reading record value size from log file: %s\n", strerror(errno));
        return LOG_ERROR;
    }

    if (read_value_sz != 1) {
        fprintf(stderr, "Error reading record value size from log file: %s\n", strerror(errno));
        return LOG_ERROR;
    }
    return LOG_SUCCESS;
}

static Error read_value(Log *log, uint8_t* value_buff, size_t value_size) {
    size_t read_value = fread(value_buff, sizeof(uint8_t), value_size, log->file);
    if (errno != 0) {
        fprintf(stderr, "Error reading record value from log file: %s\n", strerror(errno));
        return LOG_ERROR;
    }

    if (read_value != value_size) {
        fprintf(stderr, "Error reading record value from log file: %s\n", strerror(errno));
        return LOG_ERROR;
    }
    value_buff[value_size] = '\0';
    return LOG_SUCCESS;
}

static Error read_record(Log *log) {
    char id_buff[ID_SIZE];
    uint8_t* value_buff;
    size_t value_size;

    switch (reads_id(log, id_buff)) {
        case LOG_SUCCESS:
            break;
        case LOG_EOF:
            return LOG_EOF;
        case LOG_ERROR:
            return LOG_ERROR;
    }
    
    if (read_value_size(log, &value_size) != LOG_SUCCESS) return LOG_ERROR;

    value_buff = (uint8_t*) malloc((value_size + 1) * sizeof(uint8_t));
    if (read_value(log, value_buff, value_size) != LOG_SUCCESS) {
        free(value_buff);
        return LOG_ERROR;
    }    
    Record* record = record_new(id_buff, value_size, value_buff);
    free(value_buff);
    if (append_record(log->document, record) != LOG_SUCCESS) {
        free(record);
        return LOG_ERROR;
    }
    return LOG_SUCCESS;
}

Error log_load(Log* log) {
    assert(log != NULL);
    fclose(log->file);
    if ((log->file = fopen(log->filename, "rb")) == NULL) {
        fprintf(stderr, "Failed to open log file for reading: %s\n", log->filename);
        return LOG_ERROR;
    }
    for (;;) {
        Error err = read_record(log);
        if (err == LOG_EOF) break;
        if (err != LOG_SUCCESS) return LOG_ERROR;
    }
    return LOG_SUCCESS;
}

static Error document_free(Document* document) {
    assert(document != NULL);
    for (size_t i = 0; i < document->size; i++) {
        free(document->records[i].value);
    }
    free(document->records);
    return LOG_SUCCESS;
}

Error log_free(Log* log) {
    assert(log != NULL);
    if (log->file != NULL) {
        fclose(log->file);
        log->file = NULL;
    }
    if (log->document != NULL) {
        document_free(log->document);
        log->document = NULL;
    }
    free(log);
    return LOG_SUCCESS;
}

#endif // LOG_IMPLEMENTATION

#endif // LOG_H
