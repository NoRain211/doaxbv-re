#include "symbolic_link_model.h"

#include <ctype.h>
#include <string.h>

enum {
    SYMBOLIC_LINK_HANDLE_BASE = 0x53590001u,
};

static const char *without_nt_prefix(const char *name)
{
    return name != NULL && strncmp(name, "\\??\\", 4u) == 0
        ? name + 4
        : name;
}

static bool names_equal(const char *left, const char *right)
{
    left = without_nt_prefix(left);
    right = without_nt_prefix(right);
    if (left == NULL || right == NULL) {
        return false;
    }
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

static bool copy_text(char *out, size_t size, const char *text)
{
    size_t length;

    if (out == NULL || text == NULL || size == 0u) {
        return false;
    }
    length = strlen(text);
    if (length >= size) {
        return false;
    }
    memcpy(out, text, length + 1u);
    return true;
}

void recomp_symbolic_link_reset(RecompSymbolicLinkModel *model)
{
    if (model != NULL) {
        *model = (RecompSymbolicLinkModel){
            .next_handle = SYMBOLIC_LINK_HANDLE_BASE,
        };
    }
}

bool recomp_symbolic_link_create(
    RecompSymbolicLinkModel *model,
    const char *name,
    const char *target)
{
    RecompSymbolicLink *free_link = NULL;

    if (model == NULL || name == NULL || target == NULL) {
        return false;
    }
    for (size_t i = 0u; i < RECOMP_SYMBOLIC_LINK_MAX_COUNT; ++i) {
        RecompSymbolicLink *link = &model->links[i];

        if (link->active && names_equal(link->name, name)) {
            return false;
        }
        if (!link->active && free_link == NULL) {
            free_link = link;
        }
    }
    if (free_link == NULL ||
        !copy_text(free_link->name, sizeof free_link->name, name) ||
        !copy_text(free_link->target, sizeof free_link->target, target)) {
        return false;
    }
    free_link->active = true;
    return true;
}

bool recomp_symbolic_link_delete(
    RecompSymbolicLinkModel *model,
    const char *name)
{
    if (model == NULL || name == NULL) {
        return false;
    }
    for (size_t i = 0u; i < RECOMP_SYMBOLIC_LINK_MAX_COUNT; ++i) {
        RecompSymbolicLink *link = &model->links[i];

        if (link->active && names_equal(link->name, name)) {
            for (size_t j = 0u;
                 j < RECOMP_SYMBOLIC_LINK_MAX_OPEN_HANDLES;
                 ++j) {
                if (model->open_handles[j].active &&
                    model->open_handles[j].link_index == i) {
                    model->open_handles[j] =
                        (RecompSymbolicLinkHandle){0};
                }
            }
            *link = (RecompSymbolicLink){0};
            return true;
        }
    }
    return false;
}

bool recomp_symbolic_link_open(
    RecompSymbolicLinkModel *model,
    const char *name,
    uint32_t *handle)
{
    if (model == NULL || name == NULL || handle == NULL) {
        return false;
    }
    for (size_t i = 0u; i < RECOMP_SYMBOLIC_LINK_MAX_COUNT; ++i) {
        RecompSymbolicLink *link = &model->links[i];

        if (link->active && names_equal(link->name, name)) {
            for (size_t j = 0u;
                 j < RECOMP_SYMBOLIC_LINK_MAX_OPEN_HANDLES;
                 ++j) {
                RecompSymbolicLinkHandle *open = &model->open_handles[j];

                if (!open->active) {
                    *open = (RecompSymbolicLinkHandle){
                        .active = true,
                        .handle = model->next_handle,
                        .link_index = i,
                    };
                    model->next_handle += 4u;
                    *handle = open->handle;
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

bool recomp_symbolic_link_query(
    const RecompSymbolicLinkModel *model,
    uint32_t handle,
    char *target,
    size_t target_size)
{
    if (model == NULL || handle == 0u) {
        return false;
    }
    for (size_t i = 0u; i < RECOMP_SYMBOLIC_LINK_MAX_OPEN_HANDLES; ++i) {
        const RecompSymbolicLinkHandle *open = &model->open_handles[i];

        if (open->active && open->handle == handle &&
            open->link_index < RECOMP_SYMBOLIC_LINK_MAX_COUNT &&
            model->links[open->link_index].active) {
            return copy_text(
                target,
                target_size,
                model->links[open->link_index].target);
        }
    }
    return false;
}

static bool prefix_equal(const char *text, const char *prefix, size_t length)
{
    for (size_t i = 0u; i < length; ++i) {
        if (text[i] == '\0' ||
            tolower((unsigned char)text[i]) !=
                tolower((unsigned char)prefix[i])) {
            return false;
        }
    }
    return true;
}

bool recomp_symbolic_link_resolve_path(
    const RecompSymbolicLinkModel *model,
    const char *path,
    char *target,
    size_t target_size)
{
    const char *body = without_nt_prefix(path);

    if (model == NULL || body == NULL || target == NULL || target_size == 0u) {
        return false;
    }
    for (size_t i = 0u; i < RECOMP_SYMBOLIC_LINK_MAX_COUNT; ++i) {
        const RecompSymbolicLink *link = &model->links[i];
        const char *name;
        size_t name_length;
        size_t target_length;
        const char *suffix;

        if (!link->active) {
            continue;
        }
        name = without_nt_prefix(link->name);
        name_length = strlen(name);
        if (!prefix_equal(body, name, name_length) ||
            (body[name_length] != '\0' && body[name_length] != '\\' &&
             body[name_length] != '/')) {
            continue;
        }
        suffix = body + name_length;
        target_length = strlen(link->target);
        if (target_length + strlen(suffix) + 1u > target_size) {
            return false;
        }
        memcpy(target, link->target, target_length);
        strcpy(target + target_length, suffix);
        return true;
    }
    return false;
}

bool recomp_symbolic_link_close(
    RecompSymbolicLinkModel *model,
    uint32_t handle)
{
    if (model == NULL || handle == 0u) {
        return false;
    }
    for (size_t i = 0u; i < RECOMP_SYMBOLIC_LINK_MAX_OPEN_HANDLES; ++i) {
        RecompSymbolicLinkHandle *open = &model->open_handles[i];

        if (open->active && open->handle == handle) {
            *open = (RecompSymbolicLinkHandle){0};
            return true;
        }
    }
    return false;
}
