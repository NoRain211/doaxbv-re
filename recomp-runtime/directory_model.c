#include "directory_model.h"

#include <ctype.h>
#include <string.h>

static bool text_equal(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) !=
            tolower((unsigned char)*right)) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == *right;
}

static bool pattern_matches(const char *pattern, const char *name)
{
    return pattern == NULL || pattern[0] == '\0' ||
        strcmp(pattern, "*") == 0 || strcmp(pattern, "*.*") == 0 ||
        text_equal(pattern, name);
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
