#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>
#include "consts.h"

typedef struct {
    log_id_t id;
    size_t size;
    u8 *value;
} Record;

typedef struct {
    Record* records;
    size_t capacity;
    size_t size;
} Document;

typedef struct {
    log_id_t id_sequence;
    const char* filename;
    FILE* file;
    Document* document;
    size_t cursor;
} Log;


log_id_t id(Log* log);
Record* record_new(log_id_t id, size_t value_size, u8 *value);
Document* document_new(void);
Log* log_new(const char* filename);
void print_document(const Document* document);
Result append_record(Document* document, Record* record);
Result log_record(Log* log, Record* record);
Result log_load(Log* log);
Result log_free(Log* log);

#ifdef LOG_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

log_id_t id(Log* log) {
    assert(log != NULL);
    return log->id_sequence + 1;
}

Record* record_new(log_id_t id, size_t value_size, u8 *value) {
    assert(value != NULL);
    assert(value_size <= RECORD_MAX_SIZE - 1);

    Record* record = (Record *) malloc(sizeof(Record));

    record->id = id;
    record->size = value_size;
    record->value = (u8 *) malloc((record->size + 1) * sizeof(u8));

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

    Log* log = (Log *) malloc(sizeof(Log));
    if ((log->file = fopen(filename, "ab")) == NULL) {
        fprintf(stderr, "Failed to open log file: %s\n", filename);
        free(log);
        exit(1);
    }
    log->id_sequence = 0;
    log->filename = filename;
    log->document = document_new();
    log->cursor = 0;
    return log;
}

Result append_record(Document *document, Record *record) {
    assert(document != NULL);
    assert(record != NULL);
    if (document->size >= document->capacity) {
        size_t new_capacity = (document->capacity == 0) ? 1 : document->capacity * 2;
        Record* new_records = (Record*) realloc(document->records, new_capacity * sizeof(Record));
        if (new_records == NULL) {
            fprintf(stderr, "Failed to allocate memory for records\n");
            return ERROR;
        }
        document->records = new_records;
        document->capacity = new_capacity;
    }
    document->records[document->size++] = *record;
    return SUCCESS;
}

static void print_record(const Record* record) {
    printf("[*] %llu %u %s\n", record->id, record->size, record->value);
}

void print_document(const Document* document) {
    for (size_t i = 0; i < document->size; i++) {
        print_record(&document->records[i]);
    }
}

static Result write_id(Log *log, Record *record) {
    size_t written_id = fwrite(&record->id, sizeof(log_id_t), 1, log->file);
    if (errno != 0) {
        fprintf(stderr, "Error writing record ID to log file: %s\n", strerror(errno));
        return ERROR;
    }

    if (written_id != 1) {
        fprintf(stderr, "Failed to write record ID to log file\n");
        return ERROR;
    }
    return SUCCESS;
}

static Result write_value_sz(Log *log, Record *record) {
    size_t written_value_sz = fwrite(&record->size, sizeof(size_t), 1, log->file);
    if (errno != 0) {
        fprintf(stderr, "Error writing record size to log file: %s\n", strerror(errno));
        return ERROR;
    }

    if (written_value_sz != 1) {
        fprintf(stderr, "Failed to write record size to log file\n");
        return ERROR;
    }
    return SUCCESS;
}

static Result write_value(Log *log, Record *record) {
    size_t written_value = fwrite(record->value, sizeof(u8), record->size, log->file);
    if (errno != 0) {
        fprintf(stderr, "Error writing record value to log file: %s\n", strerror(errno));
        return ERROR;
    }

    if (written_value != record->size) {
        fprintf(stderr, "Failed to write record value to log file\n");
        return ERROR;
    }
    return SUCCESS;
}

Result log_record(Log *log, Record *record) {
    assert(log != NULL);
    if (append_record(log->document, record) != SUCCESS) return ERROR;

#ifdef LOG_PERSIST
    Result err;
    if ((err = write_id(log, record)) != SUCCESS) return ERROR;
    if ((err = write_value_sz(log, record)) != SUCCESS) return ERROR;
    if ((err = write_value(log, record)) != SUCCESS) return ERROR;

#ifdef FSYNC

    if (fflush(log->file) != 0) {
        fprintf(stderr, "Error flushing log file: %s\n", strerror(errno));
        return ERROR;
    }

    if (fsync(fileno(log->file)) != 0) {
        fprintf(stderr, "Error syncing log file: %s\n", strerror(errno));
        return ERROR;
    }
#endif

    log->id_sequence = record->id;
    log->cursor++;
#endif
    return SUCCESS;
}

static Result reads_id(Log *log, log_id_t* id_buff) {
    size_t read_id = fread(id_buff, sizeof(log_id_t), 1, log->file);

    if (read_id == 0 && feof(log->file)) return LOG_EOF;    

    if (errno != 0) {
        fprintf(stderr, "Error reading record ID from log file: %s\n", strerror(errno));
        return ERROR;
    }
    
    if (read_id != 1) {
        fprintf(stderr, "Error reading record ID from log file: %s\n", strerror(errno));
        return ERROR;
    }
    return SUCCESS;
}

static Result read_value_size(Log *log, size_t* value_size) {
    size_t read_value_sz = fread(value_size, sizeof(size_t), 1, log->file);
    if (errno != 0) {
        fprintf(stderr, "Error reading record value size from log file: %s\n", strerror(errno));
        return ERROR;
    }

    if (read_value_sz != 1) {
        fprintf(stderr, "Error reading record value size from log file: %s\n", strerror(errno));
        return ERROR;
    }
    return SUCCESS;
}

static Result read_value(Log *log, u8* value_buff, size_t value_size) {
    size_t read_value = fread(value_buff, sizeof(u8), value_size, log->file);
    if (errno != 0) {
        fprintf(stderr, "Error reading record value from log file: %s\n", strerror(errno));
        return ERROR;
    }

    if (read_value != value_size) {
        fprintf(stderr, "Error reading record value from log file: %s\n", strerror(errno));
        return ERROR;
    }
    value_buff[value_size] = '\0';
    return SUCCESS;
}

static Result read_record(Log *log) {
    log_id_t id_buff;
    u8* value_buff;
    size_t value_size;

    switch (reads_id(log, &id_buff)) {
        case SUCCESS:
            break;
        case LOG_EOF:
            return LOG_EOF;
        case ERROR:
            return ERROR;
    }
    
    if (read_value_size(log, &value_size) != SUCCESS) return ERROR;

    value_buff = (u8 *) malloc((value_size + 1) * sizeof(u8));
    if (read_value(log, value_buff, value_size) != SUCCESS) {
        free(value_buff);
        return ERROR;
    }    
    Record* record = record_new(id_buff, value_size, value_buff);
    free(value_buff);
    if (append_record(log->document, record) != SUCCESS) {
        free(record);
        return ERROR;
    }
    return SUCCESS;
}

Result log_load(Log* log) {
    assert(log != NULL);
    fclose(log->file);
    if ((log->file = fopen(log->filename, "rb")) == NULL) {
        fprintf(stderr, "Failed to open log file for reading: %s\n", log->filename);
        return ERROR;
    }
    for (;;) {
        Result err = read_record(log);
        if (err == LOG_EOF) break;
        if (err != SUCCESS) return ERROR;
    }
    if (log->document->size > 0) {
        log->id_sequence = log->document->records[log->document->size - 1].id;
    }
    return SUCCESS;
}

static Result document_free(Document* document) {
    assert(document != NULL);
    for (size_t i = 0; i < document->size; i++) {
        free(document->records[i].value);
    }
    free(document->records);
    return SUCCESS;
}

Result log_free(Log* log) {
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
    return SUCCESS;
}

#endif // LOG_IMPLEMENTATION

#endif // LOG_H
