#ifndef DOAXBV_RECOMP_SYMBOLIC_LINK_MODEL_H
#define DOAXBV_RECOMP_SYMBOLIC_LINK_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    RECOMP_SYMBOLIC_LINK_MAX_COUNT = 16u,
    RECOMP_SYMBOLIC_LINK_MAX_OPEN_HANDLES = 32u,
    RECOMP_SYMBOLIC_LINK_NAME_SIZE = 128u,
};

typedef struct RecompSymbolicLink {
    bool active;
    char name[RECOMP_SYMBOLIC_LINK_NAME_SIZE];
    char target[RECOMP_SYMBOLIC_LINK_NAME_SIZE];
} RecompSymbolicLink;

typedef struct RecompSymbolicLinkHandle {
    bool active;
    uint32_t handle;
    size_t link_index;
} RecompSymbolicLinkHandle;

typedef struct RecompSymbolicLinkModel {
    RecompSymbolicLink links[RECOMP_SYMBOLIC_LINK_MAX_COUNT];
    RecompSymbolicLinkHandle
        open_handles[RECOMP_SYMBOLIC_LINK_MAX_OPEN_HANDLES];
    uint32_t next_handle;
} RecompSymbolicLinkModel;

void recomp_symbolic_link_reset(RecompSymbolicLinkModel *model);
bool recomp_symbolic_link_create(
    RecompSymbolicLinkModel *model,
    const char *name,
    const char *target);
bool recomp_symbolic_link_delete(
    RecompSymbolicLinkModel *model,
    const char *name);
bool recomp_symbolic_link_open(
    RecompSymbolicLinkModel *model,
    const char *name,
    uint32_t *handle);
bool recomp_symbolic_link_query(
    const RecompSymbolicLinkModel *model,
    uint32_t handle,
    char *target,
    size_t target_size);
bool recomp_symbolic_link_resolve_path(
    const RecompSymbolicLinkModel *model,
    const char *path,
    char *target,
    size_t target_size);
bool recomp_symbolic_link_close(
    RecompSymbolicLinkModel *model,
    uint32_t handle);

#endif
