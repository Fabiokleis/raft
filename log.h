#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>
#include "consts.h"

typedef struct {
    u64 id;
    u64 term;
    size_t size;
    u8 *value;
} Record;

typedef struct {
    Record *records;
    size_t capacity;
    size_t size;
} Document;

typedef struct {
    u64 id_sequence;
    const char *filename;
    FILE *file;
    Document *document;
    size_t cursor;
} Log;


u64 lid(Log *log);
Record* record_new(u64 id, u64 term, size_t value_size, u8 *value);
Document* document_new(void);
Log* log_new(const char *filename);
void print_document(const Document *document, decoder *decode);
Result append_record(Document *document, Record *record);
Result log_record(Log *log, Record *record);
Result log_load(Log *log);
Result log_free(Log *log);

#ifdef LOG_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

u64 lid(Log *log) {
    assert(log != NULL);
    return log->id_sequence + 1;
}

Record* record_new(u64 id, u64 term, size_t value_size, u8 *value) {
    assert(value != NULL);
    assert(value_size <= RECORD_MAX_SIZE - 1);

    Record *record = (Record *) malloc(sizeof(Record));

    record->id = id;
    record->term = term;
    record->size = value_size;
    record->value = (u8 *) malloc((record->size + 1) * sizeof(u8));

    memcpy(record->value, value, record->size);
    record->value[record->size] = '\0';

    return record;
}

Document* document_new(void) {
    Document *document = (Document *) malloc(sizeof(Document));
    document->records = NULL;
    document->capacity = 0;
    document->size = 0;
    return document;
}

Log* log_new(const char *filename) {
    assert(filename != NULL);

    Log* log = (Log *) malloc(sizeof(Log));
    if ((log->file = fopen(filename, "ab")) == NULL) {
        fprintf(stderr, "failed to open log file: %s\n", filename);
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
        Record *new_records = (Record *) realloc(document->records, new_capacity * sizeof(Record));
        if (new_records == NULL) {
            fprintf(stderr, "failed to allocate memory for records\n");
            return ERROR;
        }
        document->records = new_records;
        document->capacity = new_capacity;
    }
    document->records[document->size++] = *record;
    return SUCCESS;
}

static void print_record(const Record *record, decoder *decode) {
    const char *decoded_value = decode(record->value, record->size);
    printf("[*]{ id: %llu, term: %llu, value: %s }\n", record->id, record->term, decoded_value);
}

void print_document(const Document* document, decoder *decode) {
    for (size_t i = 0; i < document->size; i++) {
        print_record(&document->records[i], decode);
    }
}

static Result write_id(Log *log, Record *record) {
    size_t written_id = fwrite(&record->id, sizeof(u64), 1, log->file);
    if (errno != 0) {
        fprintf(stderr, "error writing record id to log file: %s\n", strerror(errno));
        return ERROR;
    }

    if (written_id != 1) {
        fprintf(stderr, "failed to write record id to log file\n");
        return ERROR;
    }
    return SUCCESS;
}

static Result write_term(Log *log, Record *record) {
    size_t written_term = fwrite(&record->term, sizeof(u64), 1, log->file);
    if (errno != 0) {
        fprintf(stderr, "error writting record term to log file: %s\n", strerror(errno));
        return ERROR;
    }

    if (written_term != 1) {
        fprintf(stderr, "error writting record term to log file: %s\n", strerror(errno));
        return ERROR;
    }
    return SUCCESS;
}

static Result write_value_sz(Log *log, Record *record) {
    size_t written_value_sz = fwrite(&record->size, sizeof(size_t), 1, log->file);
    if (errno != 0) {
        fprintf(stderr, "error writing record size to log file: %s\n", strerror(errno));
        return ERROR;
    }

    if (written_value_sz != 1) {
        fprintf(stderr, "failed to write record size to log file\n");
        return ERROR;
    }
    return SUCCESS;
}

static Result write_value(Log *log, Record *record) {
    size_t written_value = fwrite(record->value, sizeof(u8), record->size, log->file);
    if (errno != 0) {
        fprintf(stderr, "error writing record value to log file: %s\n", strerror(errno));
        return ERROR;
    }

    if (written_value != record->size) {
        fprintf(stderr, "failed to write record value to log file\n");
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
    if ((err = write_term(log, record)) != SUCCESS) return ERROR;
    if ((err = write_value_sz(log, record)) != SUCCESS) return ERROR;
    if ((err = write_value(log, record)) != SUCCESS) return ERROR;

#ifdef FSYNC
    if (fflush(log->file) != 0) {
        fprintf(stderr, "error flushing log file: %s\n", strerror(errno));
        return ERROR;
    }

    if (fsync(fileno(log->file)) != 0) {
        fprintf(stderr, "error syncing log file: %s\n", strerror(errno));
        return ERROR;
    }
#endif

    log->id_sequence = record->id;
    log->cursor++;
#endif
    return SUCCESS;
}

static Result read_id(Log *log, u64 *id_buff) {
    size_t read_id = fread(id_buff, sizeof(u64), 1, log->file);

    if (read_id == 0 && feof(log->file)) return LOG_EOF;    

    if (errno != 0) {
        fprintf(stderr, "error reading record id from log file: %s\n", strerror(errno));
        return ERROR;
    }
    
    if (read_id != 1) {
        fprintf(stderr, "error reading record id from log file: %s\n", strerror(errno));
        return ERROR;
    }
    return SUCCESS;
}

static Result read_term(Log *log, u64 *term_buff) {
    size_t read_term = fread(term_buff, sizeof(u64), 1, log->file);
    if (errno != 0) {
        fprintf(stderr, "error reading record term from log file: %s\n", strerror(errno));
        return ERROR;
    }
    
    if (read_term != 1) {
        fprintf(stderr, "error reading record term from log file: %s\n", strerror(errno));
        return ERROR;
    }
    return SUCCESS;
}

static Result read_value_size(Log *log, size_t *value_size) {
    size_t read_value_sz = fread(value_size, sizeof(size_t), 1, log->file);
    if (errno != 0) {
        fprintf(stderr, "error reading record value size from log file: %s\n", strerror(errno));
        return ERROR;
    }

    if (read_value_sz != 1) {
        fprintf(stderr, "error reading record value size from log file: %s\n", strerror(errno));
        return ERROR;
    }
    return SUCCESS;
}

static Result read_value(Log *log, u8 *value_buff, size_t value_size) {
    size_t read_value = fread(value_buff, sizeof(u8), value_size, log->file);
    if (errno != 0) {
        fprintf(stderr, "error reading record value from log file: %s\n", strerror(errno));
        return ERROR;
    }

    if (read_value != value_size) {
        fprintf(stderr, "error reading record value from log file: %s\n", strerror(errno));
        return ERROR;
    }
    value_buff[value_size] = '\0';
    return SUCCESS;
}

static Result read_record(Log *log) {
    u64 id_buff;
    u64 term_buff;
    u8 *value_buff;
    size_t value_size;

    switch (read_id(log, &id_buff)) {
        case SUCCESS:
            break;
        case LOG_EOF:
            return LOG_EOF;
        case ERROR:
            return ERROR;
    }

    if (read_term(log, &term_buff) != SUCCESS) return ERROR;
    if (read_value_size(log, &value_size) != SUCCESS) return ERROR;

    value_buff = (u8 *) malloc((value_size + 1) * sizeof(u8));
    if (read_value(log, value_buff, value_size) != SUCCESS) {
        free(value_buff);
        return ERROR;
    }    
    Record *record = record_new(id_buff, term_buff, value_size, value_buff);
    free(value_buff);
    if (append_record(log->document, record) != SUCCESS) {
        free(record);
        return ERROR;
    }
    return SUCCESS;
}

Result log_load(Log *log) {
    assert(log != NULL);
    fclose(log->file);
    if ((log->file = fopen(log->filename, "rb")) == NULL) {
        fprintf(stderr, "failed to open log file for reading: %s %s\n", log->filename, strerror(errno));
        return ERROR;
    }
    for (;;) {
        Result err = read_record(log);
        if (err == LOG_EOF) break;
        if (err != SUCCESS) return ERROR;
    }

    printf("log id sequence after loading: %llu\n", log->id_sequence);
    fclose(log->file);
    log->file = fopen(log->filename, "ab");
    if (log->file == NULL) {
        fprintf(stderr, "failed to open log file for appending: %s %s\n", log->filename, strerror(errno));
        return ERROR;
    }
    if (log->document->size > 0) {
        log->id_sequence = log->document->records[log->document->size - 1].id;
    }

    return SUCCESS;
}

static Result document_free(Document *document) {
    assert(document != NULL);
    for (size_t i = 0; i < document->size; i++) {
        free(document->records[i].value);
    }
    free(document->records);
    return SUCCESS;
}

Result log_free(Log *log) {
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
