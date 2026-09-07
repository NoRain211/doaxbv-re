#include "directory_model.h"

#include <ctype.h>
#include <string.h>

static bool pattern_matches(const char *pattern, const char *name)
{
    const char *after_star = NULL;
    const char *star_name = NULL;

    if (pattern == NULL || pattern[0] == '\0' ||
        strcmp(pattern, "*.*") == 0) {
        return true;
    }
    while (*name != '\0') {
        if (*pattern == '*') {
            after_star = ++pattern;
            star_name = name;
        } else if (*pattern != '\0' &&
            tolower((unsigned char)*pattern) == tolower((unsigned char)*name)) {
            ++pattern;
            ++name;
        } else if (after_star != NULL) {
            pattern = after_star;
            name = ++star_name;
        } else {
            return false;
        }
    }
    while (*pattern == '*') {
        ++pattern;
    }
    return *pattern == '\0';
}

void recomp_directory_reset(RecompDirectoryModel *model)
{
    if (model != NULL) {
        *model = (RecompDirectoryModel){0};
    }
}

bool recomp_directory_add(
    RecompDirectoryModel *model,
    const RecompDirectoryEntry *entry)
{
    if (model == NULL || entry == NULL || entry->name[0] == '\0' ||
        model->count >= RECOMP_DIRECTORY_MAX_ENTRIES ||
        memchr(entry->name, '\0', sizeof entry->name) == NULL) {
        return false;
    }
    model->entries[model->count++] = *entry;
    return true;
}

void recomp_directory_restart(RecompDirectoryModel *model)
{
    if (model != NULL) {
        model->cursor = 0u;
    }
}

bool recomp_directory_next(
    RecompDirectoryModel *model,
    const char *pattern,
    RecompDirectoryEntry *entry)
{
    if (model == NULL || entry == NULL) {
        return false;
    }
    while (model->cursor < model->count) {
        const RecompDirectoryEntry *candidate =
            &model->entries[model->cursor++];

        if (pattern_matches(pattern, candidate->name)) {
            *entry = *candidate;
            return true;
        }
    }
    return false;
}

static void write_u32(unsigned char *buffer, size_t offset, uint32_t value)
{
    memcpy(buffer + offset, &value, sizeof value);
}

static void write_u64(unsigned char *buffer, size_t offset, uint64_t value)
{
    memcpy(buffer + offset, &value, sizeof value);
}

bool recomp_directory_serialize(
    const RecompDirectoryEntry *entry,
    void *buffer,
    size_t buffer_size,
    size_t *bytes_written)
{
    const size_t header_size = 0x40u;
    size_t name_length;
    unsigned char *bytes = buffer;

    if (bytes_written != NULL) {
        *bytes_written = 0u;
    }
    if (entry == NULL || buffer == NULL ||
        memchr(entry->name, '\0', sizeof entry->name) == NULL) {
        return false;
    }
    name_length = strlen(entry->name);
    if (header_size + name_length + 1u > buffer_size) {
        return false;
    }

    memset(bytes, 0, buffer_size);
    write_u64(bytes, 0x08u, entry->creation_time);
    write_u64(bytes, 0x10u, entry->last_access_time);
    write_u64(bytes, 0x18u, entry->last_write_time);
    write_u64(bytes, 0x20u, entry->change_time);
    write_u64(bytes, 0x28u, entry->size);
    write_u64(bytes, 0x30u, entry->allocation_size);
    write_u32(bytes, 0x38u, entry->attributes);
    write_u32(bytes, 0x3cu, (uint32_t)name_length);
    memcpy(bytes + 0x40u, entry->name, name_length + 1u);
    if (bytes_written != NULL) {
        *bytes_written = header_size + name_length + 1u;
    }
    return true;
}
