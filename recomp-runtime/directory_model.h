#ifndef DOAXBV_RECOMP_DIRECTORY_MODEL_H
#define DOAXBV_RECOMP_DIRECTORY_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    RECOMP_DIRECTORY_MAX_ENTRIES = 64u,
    RECOMP_DIRECTORY_NAME_SIZE = 260u,
};

typedef struct RecompDirectoryEntry {
    char name[RECOMP_DIRECTORY_NAME_SIZE];
    uint64_t creation_time;
    uint64_t last_access_time;
    uint64_t last_write_time;
    uint64_t change_time;
    uint64_t size;
    uint64_t allocation_size;
    uint32_t attributes;
} RecompDirectoryEntry;

typedef struct RecompDirectoryModel {
    RecompDirectoryEntry entries[RECOMP_DIRECTORY_MAX_ENTRIES];
    size_t count;
    size_t cursor;
} RecompDirectoryModel;

void recomp_directory_reset(RecompDirectoryModel *model);
bool recomp_directory_add(
    RecompDirectoryModel *model,
    const RecompDirectoryEntry *entry);
void recomp_directory_restart(RecompDirectoryModel *model);
bool recomp_directory_next(
    RecompDirectoryModel *model,
    const char *pattern,
    RecompDirectoryEntry *entry);
bool recomp_directory_serialize(
    const RecompDirectoryEntry *entry,
    void *buffer,
    size_t buffer_size,
    size_t *bytes_written);

#endif
