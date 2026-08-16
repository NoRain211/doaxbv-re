#include "kernel_abi.h"
#include "runtime.h"
#include "directory_model.h"
#include "symbolic_link_model.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static const uint32_t RECOMP_STATUS_SUCCESS = 0x00000000u;
static const uint32_t RECOMP_STATUS_INVALID_HANDLE = 0xc0000008u;
static const uint32_t RECOMP_STATUS_OBJECT_NAME_NOT_FOUND = 0xc0000034u;
static const uint32_t RECOMP_STATUS_NO_MEMORY = 0xc0000017u;

enum {
    MAX_FILE_HANDLES = 256,
    MAX_PATH_LEN = 512,
    MAX_SEGMENT_LEN = 128,
};

typedef enum FileHandleKind {
    FILE_HANDLE_HOST_FILE,
    FILE_HANDLE_DIRECTORY,
    FILE_HANDLE_PSEUDO,
} FileHandleKind;

typedef struct FileHandleEntry {
    HANDLE host_handle;
    uint32_t guest_handle;
    uint64_t cursor;
    RecompDirectoryModel directory;
    FileHandleKind kind;
    int active;
} FileHandleEntry;

static FileHandleEntry file_handles[MAX_FILE_HANDLES];
static uint32_t next_guest_handle = 1u;
static RecompSymbolicLinkModel symbolic_links;
static int symbolic_links_initialized;

static void ensure_symbolic_links_initialized(void)
{
    if (!symbolic_links_initialized) {
        recomp_symbolic_link_reset(&symbolic_links);
        symbolic_links_initialized = 1;
    }
}

/* Supplied by runner.cpp from the directory containing the XBE. */
const char *recomp_disc_root_path = NULL;

static uint32_t register_file_handle(
    HANDLE host_handle,
    FileHandleKind kind,
    const char *directory_path)
{
    for (size_t i = 0; i < MAX_FILE_HANDLES; ++i) {
        if (!file_handles[i].active) {
            file_handles[i].active = 1;
            file_handles[i].host_handle = host_handle;
            file_handles[i].guest_handle = next_guest_handle;
            file_handles[i].cursor = 0u;
            recomp_directory_reset(&file_handles[i].directory);
            file_handles[i].kind = kind;
            if (kind == FILE_HANDLE_DIRECTORY && directory_path != NULL) {
                char pattern[MAX_PATH_LEN];
                WIN32_FIND_DATAA data;
                HANDLE find;

                snprintf(pattern, sizeof pattern, "%s\\*", directory_path);
                find = FindFirstFileA(pattern, &data);
                if (find != INVALID_HANDLE_VALUE) {
                    do {
                        RecompDirectoryEntry entry = {0};
                        uint64_t size;

                        if (strcmp(data.cFileName, ".") == 0 ||
                            strcmp(data.cFileName, "..") == 0) {
                            continue;
                        }
                        if (strlen(data.cFileName) >= sizeof entry.name) {
                            continue;
                        }
                        strcpy(entry.name, data.cFileName);
                        entry.creation_time =
                            (uint64_t)data.ftCreationTime.dwLowDateTime |
                            (uint64_t)data.ftCreationTime.dwHighDateTime << 32u;
                        entry.last_access_time =
                            (uint64_t)data.ftLastAccessTime.dwLowDateTime |
                            (uint64_t)data.ftLastAccessTime.dwHighDateTime << 32u;
                        entry.last_write_time =
                            (uint64_t)data.ftLastWriteTime.dwLowDateTime |
                            (uint64_t)data.ftLastWriteTime.dwHighDateTime << 32u;
                        entry.change_time = entry.last_write_time;
                        size = (uint64_t)data.nFileSizeLow |
                            (uint64_t)data.nFileSizeHigh << 32u;
                        entry.size = size;
                        entry.allocation_size = (size + 4095u) & ~4095ull;
                        entry.attributes = data.dwFileAttributes;
                        if (!recomp_directory_add(
                                &file_handles[i].directory, &entry)) {
                            break;
                        }
                    } while (FindNextFileA(find, &data));
                    FindClose(find);
                }
            }
            next_guest_handle += 4u;
            return file_handles[i].guest_handle;
        }
    }
    return 0u;
}

static int read_guest_ansi_string(uint32_t ansi_string_address, char *out, size_t out_size)
{
    if (ansi_string_address == 0u || out_size == 0u) {
        return 0;
    }
    uint16_t length = *(uint16_t *)recomp_memory_u32(ansi_string_address);
    uint32_t buffer = *recomp_memory_u32(ansi_string_address + 4u);
    if (buffer == 0u || length == 0u || length >= out_size) {
        return 0;
    }
    memcpy(out, (const void *)recomp_memory_u32(buffer), length);
    out[length] = '\0';
    return 1;
}

static int read_guest_object_name(uint32_t object_attributes, char *out, size_t out_size)
{
    if (object_attributes == 0u) {
        return 0;
    }
    uint32_t compact_name = *recomp_memory_u32(object_attributes + 4u);
    if (compact_name != 0u && read_guest_ansi_string(compact_name, out, out_size)) {
        return 1;
    }
    uint32_t nt_name = *recomp_memory_u32(object_attributes + 8u);
    if (nt_name != 0u && read_guest_ansi_string(nt_name, out, out_size)) {
        return 1;
    }
    return 0;
}

static int append_segment(char *path, size_t path_size, const char *segment)
{
    if (segment[0] == '\0' || strcmp(segment, "..") == 0 || strchr(segment, ':') != NULL) {
        return 0;
    }
    size_t len = strlen(path);
    if (len > 0 && path[len - 1] != '\\') {
        if (len + 1 >= path_size) {
            return 0;
        }
        path[len++] = '\\';
        path[len] = '\0';
    }
    size_t seg_len = strlen(segment);
    if (len + seg_len + 1 > path_size) {
        return 0;
    }
    memcpy(path + len, segment, seg_len + 1);
    return 1;
}

static int normalize_guest_path(const char *guest_path, char *body, size_t body_size)
{
    size_t i = 0;
    while (i + 1 < body_size && guest_path[i] != '\0') {
        char c = guest_path[i];
        body[i] = (c == '/') ? '\\' : (char)tolower((unsigned char)c);
        ++i;
    }
    body[i] = '\0';

    if (strncmp(body, "\\??\\", 4) == 0) {
        memmove(body, body + 4, strlen(body + 4) + 1);
    }
    if (body[0] == '\0' || body[1] != ':') {
        return 0;
    }
    char drive = (char)tolower((unsigned char)body[0]);
    if (drive != 'd' && drive != 'z') {
        return 0;
    }
    char *p = body + 2;
    while (*p == '\\') {
        ++p;
    }
    if (p != body + 2) {
        memmove(body + 2, p, strlen(p) + 1);
    }
    return 1;
}

static void copy_root(char *host_path, size_t host_path_size)
{
    strncpy(host_path, recomp_disc_root_path, host_path_size - 1u);
    host_path[host_path_size - 1u] = '\0';
    size_t len = strlen(host_path);
    if (len > 0 && host_path[len - 1] == '\\') {
        host_path[len - 1] = '\0';
    }
}

static int build_host_path(const char *relative, char *host_path, size_t host_path_size)
{
    copy_root(host_path, host_path_size);

    char segment[MAX_SEGMENT_LEN];
    size_t seg_i = 0;
    for (const char *p = relative; *p != '\0'; ++p) {
        if (*p == '\\') {
            if (seg_i > 0) {
                segment[seg_i] = '\0';
                if (!append_segment(host_path, host_path_size, segment)) {
                    return 0;
                }
                seg_i = 0;
            }
        } else {
            if (seg_i >= sizeof(segment) - 1u) {
                return 0;
            }
            segment[seg_i++] = *p;
        }
    }
    if (seg_i > 0) {
        segment[seg_i] = '\0';
        if (!append_segment(host_path, host_path_size, segment)) {
            return 0;
        }
    }
    return strlen(host_path) > 0;
}

static int build_save_path(
    const char *guest_path,
    char *host_path,
    size_t host_path_size)
{
    static const char prefix[] = "\\Device\\Harddisk0\\partition1";
    const size_t prefix_len = sizeof prefix - 1u;

    if (_strnicmp(guest_path, prefix, prefix_len) != 0 ||
        (guest_path[prefix_len] != '\0' &&
         guest_path[prefix_len] != '\\' &&
         guest_path[prefix_len] != '/')) {
        return 0;
    }

    copy_root(host_path, host_path_size);
    if (!append_segment(host_path, host_path_size, ".recomp-storage") ||
        !append_segment(host_path, host_path_size, "partition1")) {
        return 0;
    }

    const char *relative = guest_path + prefix_len;
    while (*relative == '\\' || *relative == '/') {
        ++relative;
    }

    char segment[MAX_SEGMENT_LEN];
    size_t segment_length = 0u;
    for (const char *p = relative; *p != '\0'; ++p) {
        if (*p == '\\' || *p == '/') {
            if (segment_length > 0u) {
                segment[segment_length] = '\0';
                if (!append_segment(host_path, host_path_size, segment)) {
                    return 0;
                }
                segment_length = 0u;
            }
        } else {
            if (segment_length >= sizeof segment - 1u) {
                return 0;
            }
            segment[segment_length++] = *p;
        }
    }
    if (segment_length > 0u) {
        segment[segment_length] = '\0';
        if (!append_segment(host_path, host_path_size, segment)) {
            return 0;
        }
    }
    return 1;
}

static int is_save_root_path(const char *guest_path)
{
    static const char prefix[] = "\\Device\\Harddisk0\\partition1";
    const size_t prefix_len = sizeof prefix - 1u;

    if (_strnicmp(guest_path, prefix, prefix_len) != 0) {
        return 0;
    }
    const char *relative = guest_path + prefix_len;
    while (*relative == '\\' || *relative == '/') {
        ++relative;
    }
    return *relative == '\0';
}

static int build_raw_partition_path(
    const char *guest_path,
    char *host_path,
    size_t host_path_size)
{
    static const char partition0[] = "\\Device\\Harddisk0\\partition0";

    if (_stricmp(guest_path, partition0) != 0) {
        return 0;
    }
    copy_root(host_path, host_path_size);
    return append_segment(host_path, host_path_size, ".recomp-storage") &&
        append_segment(host_path, host_path_size, "partition0");
}

static int create_directory_tree(const char *path)
{
    char current[MAX_PATH_LEN];
    size_t length = strlen(path);
    if (length == 0u || length >= sizeof current) {
        return 0;
    }
    memcpy(current, path, length + 1u);

    for (char *p = current + 3; *p != '\0'; ++p) {
        if (*p != '\\' && *p != '/') {
            continue;
        }
        char separator = *p;
        *p = '\0';
        if (!CreateDirectoryA(current, NULL) &&
            GetLastError() != ERROR_ALREADY_EXISTS) {
            return 0;
        }
        *p = separator;
    }
    if (!CreateDirectoryA(current, NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        return 0;
    }
    DWORD attributes = GetFileAttributesA(current);
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static int create_parent_directories(const char *path)
{
    char parent[MAX_PATH_LEN];
    size_t length = strlen(path);
    if (length == 0u || length >= sizeof parent) {
        return 0;
    }
    memcpy(parent, path, length + 1u);
    char *last_separator = strrchr(parent, '\\');
    if (last_separator == NULL) {
        return 0;
    }
    *last_separator = '\0';
    return create_directory_tree(parent);
}

static HANDLE create_host_file(
    const char *path,
    uint32_t desired_access,
    uint32_t share_access,
    uint32_t create_disposition)
{
    DWORD host_access = 0u;
    DWORD host_share = 0u;
    DWORD host_disposition;

    if ((desired_access & 0x80000000u) != 0u) {
        host_access |= GENERIC_READ;
    }
    if ((desired_access & 0x40000000u) != 0u) {
        host_access |= GENERIC_WRITE;
    }
    if (host_access == 0u) {
        host_access = GENERIC_READ;
    }
    if ((share_access & 1u) != 0u) {
        host_share |= FILE_SHARE_READ;
    }
    if ((share_access & 2u) != 0u) {
        host_share |= FILE_SHARE_WRITE;
    }

    switch (create_disposition) {
    case 0u: host_disposition = CREATE_ALWAYS; break;
    case 1u: host_disposition = OPEN_EXISTING; break;
    case 2u: host_disposition = CREATE_NEW; break;
    case 3u: host_disposition = OPEN_ALWAYS; break;
    case 4u: host_disposition = TRUNCATE_EXISTING; break;
    case 5u: host_disposition = CREATE_ALWAYS; break;
    default: return INVALID_HANDLE_VALUE;
    }

    return CreateFileA(
        path,
        host_access,
        host_share,
        NULL,
        host_disposition,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
}

static int open_raw_partition(const char *host_path, HANDLE *out_handle)
{
    const LONGLONG MINIMUM_RAW_PARTITION_SIZE = 0xa00;

    if (!create_parent_directories(host_path)) {
        return 0;
    }
    HANDLE handle = CreateFileA(
        host_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(handle, &size)) {
        CloseHandle(handle);
        return 0;
    }
    if (size.QuadPart < MINIMUM_RAW_PARTITION_SIZE) {
        LARGE_INTEGER end;
        end.QuadPart = MINIMUM_RAW_PARTITION_SIZE;
        if (!SetFilePointerEx(handle, end, NULL, FILE_BEGIN) ||
            !SetEndOfFile(handle)) {
            CloseHandle(handle);
            return 0;
        }
    }
    *out_handle = handle;
    return 1;
}

static int find_only_child_directory(const char *root, char *child, size_t child_size)
{
    char pattern[MAX_PATH_LEN];
    snprintf(pattern, sizeof(pattern), "%s\\*", root);
    WIN32_FIND_DATAA data;
    HANDLE find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }
    int found = 0;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            strcmp(data.cFileName, ".") != 0 &&
            strcmp(data.cFileName, "..") != 0 &&
            strcmp(data.cFileName, ".recomp-storage") != 0) {
            if (found) {
                FindClose(find);
                return 0;
            }
            strncpy(child, data.cFileName, child_size - 1u);
            child[child_size - 1u] = '\0';
            found = 1;
        }
    } while (FindNextFileA(find, &data));
    FindClose(find);
    return found;
}

static int try_open_host_file(char *host_path, size_t host_path_size, HANDLE *out_handle)
{
    DWORD attributes = GetFileAttributesA(host_path);
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        HANDLE h = CreateFileA(
            host_path,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (h != INVALID_HANDLE_VALUE) {
            *out_handle = h;
            return 1;
        }
    }

    char child[MAX_SEGMENT_LEN];
    char root[MAX_PATH_LEN];
    copy_root(root, sizeof(root));
    if (!find_only_child_directory(root, child, sizeof(child))) {
        return 0;
    }

    char nested[MAX_PATH_LEN];
    snprintf(nested, sizeof(nested), "%s\\%s", root, child);
    size_t nested_len = strlen(nested);
    size_t rel_offset = strlen(root);
    if (host_path[rel_offset] == '\\') {
        ++rel_offset;
    }
    if (nested_len + 1 + strlen(host_path + rel_offset) >= sizeof(nested)) {
        return 0;
    }
    nested[nested_len++] = '\\';
    strcpy(nested + nested_len, host_path + rel_offset);

    attributes = GetFileAttributesA(nested);
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        HANDLE h = CreateFileA(
            nested,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (h != INVALID_HANDLE_VALUE) {
            strncpy(host_path, nested, host_path_size - 1u);
            host_path[host_path_size - 1u] = '\0';
            *out_handle = h;
            return 1;
        }
    }
    return 0;
}

static int try_open_host_directory(char *host_path, size_t host_path_size)
{
    DWORD attributes = GetFileAttributesA(host_path);
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return 1;
    }

    char child[MAX_SEGMENT_LEN];
    char root[MAX_PATH_LEN];
    copy_root(root, sizeof(root));
    if (!find_only_child_directory(root, child, sizeof(child))) {
        return 0;
    }

    char nested[MAX_PATH_LEN];
    snprintf(nested, sizeof(nested), "%s\\%s", root, child);
    size_t nested_len = strlen(nested);
    size_t rel_offset = strlen(root);
    if (host_path[rel_offset] == '\\') {
        ++rel_offset;
    }
    if (nested_len + 1 + strlen(host_path + rel_offset) >= sizeof(nested)) {
        return 0;
    }
    nested[nested_len++] = '\\';
    strcpy(nested + nested_len, host_path + rel_offset);

    attributes = GetFileAttributesA(nested);
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        strncpy(host_path, nested, host_path_size - 1u);
        host_path[host_path_size - 1u] = '\0';
        return 1;
    }
    return 0;
}

/* Resolve a guest object-attributes name to a host disc path and open it the
   way the native host does: a real read-only handle for a disc file, a
   registered directory handle for a directory, or a not-found status. Returns
   the policy string for logging; on a resolved file the host handle is handed
   back through out_host_handle for the caller to register. */
static const char *resolve_and_open(
    uint32_t object_attributes,
    char *guest_path,
    char *host_path,
    HANDLE *out_host_handle,
    int *out_is_directory,
    int *out_is_writable,
    uint32_t *out_status)
{
    char normalized[MAX_PATH_LEN] = {0};
    char resolved_path[MAX_PATH_LEN] = {0};
    const char *path = guest_path;

    *out_host_handle = INVALID_HANDLE_VALUE;
    *out_is_directory = 0;
    *out_is_writable = 0;
    *out_status = RECOMP_STATUS_SUCCESS;
    host_path[0] = '\0';

    if (recomp_disc_root_path == NULL ||
        !read_guest_object_name(object_attributes, guest_path, MAX_PATH_LEN)) {
        return "pseudo-handle-open";
    }

    ensure_symbolic_links_initialized();
    if (recomp_symbolic_link_resolve_path(
            &symbolic_links, guest_path, resolved_path, sizeof resolved_path)) {
        static const char save_prefix[] =
            "\\Device\\Harddisk0\\partition1";
        const size_t save_prefix_length = sizeof save_prefix - 1u;

        if (_strnicmp(
                resolved_path, save_prefix, save_prefix_length) == 0 &&
            (resolved_path[save_prefix_length] == '\0' ||
             resolved_path[save_prefix_length] == '\\' ||
             resolved_path[save_prefix_length] == '/')) {
            path = resolved_path;
        }
    }

    if (build_raw_partition_path(path, host_path, MAX_PATH_LEN)) {
        *out_is_writable = 1;
        if (open_raw_partition(host_path, out_host_handle)) {
            return "host-raw-partition-open";
        }
        *out_status = RECOMP_STATUS_OBJECT_NAME_NOT_FOUND;
        return "host-raw-partition-open-failed";
    }

    if (build_save_path(path, host_path, MAX_PATH_LEN)) {
        *out_is_writable = 1;
        if (is_save_root_path(path)) {
            (void)create_directory_tree(host_path);
        }
        if (try_open_host_file(host_path, MAX_PATH_LEN, out_host_handle)) {
            return "host-save-file-open";
        }
        if (try_open_host_directory(host_path, MAX_PATH_LEN)) {
            *out_is_directory = 1;
            return "host-save-directory-open";
        }
        *out_status = RECOMP_STATUS_OBJECT_NAME_NOT_FOUND;
        return "host-save-path-open-failed";
    }

    if (!normalize_guest_path(path, normalized, sizeof(normalized))) {
        return "pseudo-handle-open";
    }

    const char *relative = normalized + 2;
    while (*relative == '\\') {
        ++relative;
    }
    if (!build_host_path(relative, host_path, MAX_PATH_LEN)) {
        return "pseudo-handle-open";
    }

    if (try_open_host_file(host_path, MAX_PATH_LEN, out_host_handle)) {
        return "host-disc-file-open";
    }
    if (try_open_host_directory(host_path, MAX_PATH_LEN)) {
        *out_is_directory = 1;
        return "host-disc-directory-open";
    }
    *out_status = RECOMP_STATUS_OBJECT_NAME_NOT_FOUND;
    return "host-disc-file-open-failed";
}

static void bridge_nt_open_file(void)
{
    uint32_t file_handle_ptr = kernel_arg(1u);
    uint32_t object_attributes = kernel_arg(3u);
    uint32_t io_status_block = kernel_arg(4u);

    uint32_t status;
    uint32_t guest_handle = 1u;
    const char *policy;
    int is_directory;
    int is_writable;
    HANDLE host_handle;

    char guest_path[MAX_PATH_LEN] = {0};
    char host_path[MAX_PATH_LEN] = {0};

    policy = resolve_and_open(
        object_attributes, guest_path, host_path,
        &host_handle, &is_directory, &is_writable, &status);
    (void)is_writable;

    if (host_handle != INVALID_HANDLE_VALUE) {
        guest_handle = register_file_handle(
            host_handle, FILE_HANDLE_HOST_FILE, NULL);
        if (guest_handle == 0u) {
            CloseHandle(host_handle);
            status = RECOMP_STATUS_NO_MEMORY;
        }
    } else if (is_directory) {
        guest_handle = register_file_handle(
            INVALID_HANDLE_VALUE, FILE_HANDLE_DIRECTORY, host_path);
        if (guest_handle == 0u) {
            status = RECOMP_STATUS_NO_MEMORY;
        }
    } else if (status != RECOMP_STATUS_SUCCESS) {
        guest_handle = 0u;
    } else {
        guest_handle = register_file_handle(
            INVALID_HANDLE_VALUE, FILE_HANDLE_PSEUDO, NULL);
        if (guest_handle == 0u) {
            status = RECOMP_STATUS_NO_MEMORY;
        }
    }

    fprintf(
        stderr,
        "recomp kernel: NtOpenFile path='%s' host='%s' policy='%s' handle=%u status=0x%08x\n",
        guest_path,
        host_path,
        policy,
        (unsigned)guest_handle,
        (unsigned)status);

    if (file_handle_ptr != 0u) {
        *recomp_memory_u32(file_handle_ptr) = guest_handle;
    }
    if (io_status_block != 0u) {
        *recomp_memory_u32(io_status_block) = status;
        *recomp_memory_u32(io_status_block + 4u) = 0u;
    }

    kernel_return(6u, status);
}

/* NtCreateFile is NtOpenFile with a create disposition and a write path.
   Partition1 paths use persistent host storage next to the disc root; disc
   paths remain read-only and use the synthetic write sink. */
static void bridge_nt_create_file(void)
{
    uint32_t file_handle_ptr = kernel_arg(1u);
    uint32_t desired_access = kernel_arg(2u);
    uint32_t object_attributes = kernel_arg(3u);
    uint32_t io_status_block = kernel_arg(4u);
    uint32_t share_access = kernel_arg(7u);
    uint32_t create_disposition = kernel_arg(8u);
    uint32_t create_options = kernel_arg(9u);

    const uint32_t FILE_OPEN_DISPOSITION = 1u;
    const uint32_t GENERIC_WRITE_ACCESS = 0x40000000u;

    uint32_t status;
    uint32_t guest_handle = 1u;
    const char *policy;
    int is_directory;
    int is_writable;
    HANDLE host_handle;

    char guest_path[MAX_PATH_LEN] = {0};
    char host_path[MAX_PATH_LEN] = {0};

    policy = resolve_and_open(
        object_attributes, guest_path, host_path,
        &host_handle, &is_directory, &is_writable, &status);

    if (is_writable && (create_options & 1u) != 0u) {
        if (host_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(host_handle);
        }
        DWORD attributes = GetFileAttributesA(host_path);
        if (create_disposition != FILE_OPEN_DISPOSITION &&
            attributes == INVALID_FILE_ATTRIBUTES &&
            create_directory_tree(host_path)) {
            attributes = GetFileAttributesA(host_path);
        }
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            status = RECOMP_STATUS_SUCCESS;
            guest_handle = register_file_handle(
                INVALID_HANDLE_VALUE, FILE_HANDLE_DIRECTORY, host_path);
            policy = "host-save-directory-open";
            if (guest_handle == 0u) {
                status = RECOMP_STATUS_NO_MEMORY;
            }
        } else {
            status = RECOMP_STATUS_OBJECT_NAME_NOT_FOUND;
            guest_handle = 0u;
            policy = "host-save-directory-open-failed";
        }
    } else if (is_writable && (desired_access & GENERIC_WRITE_ACCESS) != 0u) {
        if (host_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(host_handle);
        }
        host_handle = INVALID_HANDLE_VALUE;
        if (create_parent_directories(host_path)) {
            host_handle = create_host_file(
                host_path, desired_access, share_access, create_disposition);
        }
        if (host_handle != INVALID_HANDLE_VALUE) {
            status = RECOMP_STATUS_SUCCESS;
            guest_handle = register_file_handle(
                host_handle, FILE_HANDLE_HOST_FILE, NULL);
            policy = "host-save-file-open";
            if (guest_handle == 0u) {
                CloseHandle(host_handle);
                status = RECOMP_STATUS_NO_MEMORY;
            }
        } else {
            status = RECOMP_STATUS_OBJECT_NAME_NOT_FOUND;
            guest_handle = 0u;
            policy = "host-save-file-open-failed";
        }
    } else if (host_handle == INVALID_HANDLE_VALUE && !is_directory &&
        status != RECOMP_STATUS_SUCCESS) {
        /* Path did not resolve. The disposition decides whether that is an
           error (FILE_OPEN) or a pseudo-handle create (the rest). */
        if (create_disposition == FILE_OPEN_DISPOSITION) {
            guest_handle = 0u;
            policy = "pseudo-missing-file-open";
        } else {
            status = RECOMP_STATUS_SUCCESS;
            guest_handle = register_file_handle(
                INVALID_HANDLE_VALUE, FILE_HANDLE_PSEUDO, NULL);
            policy = "pseudo-handle-create";
            if (guest_handle == 0u) {
                status = RECOMP_STATUS_NO_MEMORY;
            }
        }
    } else if ((desired_access & GENERIC_WRITE_ACCESS) != 0u) {
        /* A resolved path opened for write becomes a synthetic sink: reads
           return nothing, writes are dropped. */
        if (host_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(host_handle);
        }
        guest_handle = register_file_handle(
            INVALID_HANDLE_VALUE, FILE_HANDLE_PSEUDO, NULL);
        if (guest_handle == 0u) {
            status = RECOMP_STATUS_NO_MEMORY;
        } else {
            policy = "synthetic-disc-write-sink";
        }
    } else if (host_handle != INVALID_HANDLE_VALUE) {
        guest_handle = register_file_handle(
            host_handle, FILE_HANDLE_HOST_FILE, NULL);
        if (guest_handle == 0u) {
            CloseHandle(host_handle);
            status = RECOMP_STATUS_NO_MEMORY;
        }
    } else if (is_directory) {
        guest_handle = register_file_handle(
            INVALID_HANDLE_VALUE, FILE_HANDLE_DIRECTORY, host_path);
        if (guest_handle == 0u) {
            status = RECOMP_STATUS_NO_MEMORY;
        }
    }

    fprintf(
        stderr,
        "recomp kernel: NtCreateFile path='%s' host='%s' policy='%s' handle=%u status=0x%08x\n",
        guest_path,
        host_path,
        policy,
        (unsigned)guest_handle,
        (unsigned)status);

    if (file_handle_ptr != 0u) {
        *recomp_memory_u32(file_handle_ptr) = guest_handle;
    }
    if (io_status_block != 0u) {
        *recomp_memory_u32(io_status_block) = status;
        *recomp_memory_u32(io_status_block + 4u) = 0u;
    }

    kernel_return(9u, status);
}

static void bridge_nt_query_information_file(void)
{
    uint32_t guest_handle = kernel_arg(1u);
    uint32_t io_status_block = kernel_arg(2u);
    uint32_t file_information = kernel_arg(3u);
    uint32_t length = kernel_arg(4u);
    uint32_t file_information_class = kernel_arg(5u);

    const uint32_t FILE_STANDARD_INFORMATION = 5u;
    const uint32_t FILE_NETWORK_OPEN_INFORMATION = 0x22u;
    uint32_t status = RECOMP_STATUS_SUCCESS;
    uint64_t file_size = 0u;
    const char *policy = "zero-filled-pseudo-file-information";

    if (file_information != 0u && length != 0u) {
        memset((void *)recomp_memory_i8(file_information), 0, length);
    }

    for (size_t i = 0; i < MAX_FILE_HANDLES; ++i) {
        if (!file_handles[i].active ||
            file_handles[i].guest_handle != guest_handle) {
            continue;
        }

        if (file_handles[i].kind == FILE_HANDLE_PSEUDO) {
            break;
        }

        LARGE_INTEGER size;
        if (file_handles[i].kind == FILE_HANDLE_DIRECTORY ||
            file_handles[i].host_handle == INVALID_HANDLE_VALUE ||
            !GetFileSizeEx(file_handles[i].host_handle, &size)) {
            status = RECOMP_STATUS_INVALID_HANDLE;
            policy = "host-file-information-failed";
            break;
        }

        file_size = (uint64_t)size.QuadPart;
        policy = "host-file-information";
        if (file_information_class == FILE_NETWORK_OPEN_INFORMATION &&
            file_information != 0u && length >= 0x38u) {
            *recomp_memory_u32(file_information + 0x20u) =
                (uint32_t)file_size;
            *recomp_memory_u32(file_information + 0x24u) =
                (uint32_t)(file_size >> 32u);
            *recomp_memory_u32(file_information + 0x28u) =
                (uint32_t)file_size;
            *recomp_memory_u32(file_information + 0x2cu) =
                (uint32_t)(file_size >> 32u);
            *recomp_memory_u32(file_information + 0x30u) =
                FILE_ATTRIBUTE_NORMAL;
        } else if (file_information_class == FILE_STANDARD_INFORMATION &&
                   file_information != 0u && length >= 0x16u) {
            *recomp_memory_u32(file_information + 0x00u) =
                (uint32_t)file_size;
            *recomp_memory_u32(file_information + 0x04u) =
                (uint32_t)(file_size >> 32u);
            *recomp_memory_u32(file_information + 0x08u) =
                (uint32_t)file_size;
            *recomp_memory_u32(file_information + 0x0cu) =
                (uint32_t)(file_size >> 32u);
            *recomp_memory_u32(file_information + 0x10u) = 1u;
        }
        break;
    }

    if (io_status_block != 0u) {
        *recomp_memory_u32(io_status_block) = status;
        *recomp_memory_u32(io_status_block + 4u) =
            status == RECOMP_STATUS_SUCCESS ? length : 0u;
    }

    fprintf(
        stderr,
        "recomp kernel: NtQueryInformationFile handle=%u class=%u len=%u"
        " size=%llu policy='%s' status=0x%08x\n",
        (unsigned)guest_handle,
        (unsigned)file_information_class,
        (unsigned)length,
        (unsigned long long)file_size,
        policy,
        (unsigned)status);

    kernel_return(5u, status);
}

static void bridge_nt_query_directory_file(void)
{
    const uint32_t RECOMP_STATUS_INVALID_PARAMETER = 0xc000000du;
    const uint32_t RECOMP_STATUS_NO_MORE_FILES = 0x80000006u;
    const uint32_t FILE_DIRECTORY_INFORMATION = 1u;
    const uint32_t DIRECTORY_HEADER_SIZE = 0x40u;
    uint32_t guest_handle = kernel_arg(1u);
    uint32_t io_status_block = kernel_arg(5u);
    uint32_t file_information = kernel_arg(6u);
    uint32_t length = kernel_arg(7u);
    uint32_t file_information_class = kernel_arg(8u);
    uint32_t file_name = kernel_arg(9u);
    uint32_t restart_scan = kernel_arg(10u);
    uint32_t status = RECOMP_STATUS_INVALID_HANDLE;
    uint32_t bytes_written = 0u;
    size_t cursor = 0u;
    const char *policy = "invalid-directory-handle";
    char pattern[RECOMP_DIRECTORY_NAME_SIZE] = {0};

    if (file_information != 0u && length != 0u) {
        memset(recomp_memory_i8(file_information), 0, length);
    }
    if (file_name != 0u) {
        uint16_t pattern_length = *recomp_memory_u16(file_name);
        uint32_t pattern_buffer = *recomp_memory_u32(file_name + 4u);

        if (pattern_length >= sizeof pattern ||
            (pattern_length != 0u && pattern_buffer == 0u)) {
            status = RECOMP_STATUS_INVALID_PARAMETER;
            policy = "invalid-directory-pattern";
            goto finish;
        }
        if (pattern_length != 0u) {
            memcpy(pattern, recomp_memory_i8(pattern_buffer), pattern_length);
            pattern[pattern_length] = '\0';
        }
    }
    if (file_information == 0u || length < DIRECTORY_HEADER_SIZE + 1u ||
        file_information_class != FILE_DIRECTORY_INFORMATION) {
        status = RECOMP_STATUS_INVALID_PARAMETER;
        policy = "unsupported-directory-query";
        goto finish;
    }

    for (size_t i = 0u; i < MAX_FILE_HANDLES; ++i) {
        RecompDirectoryEntry entry;
        size_t name_length;

        if (!file_handles[i].active ||
            file_handles[i].guest_handle != guest_handle) {
            continue;
        }
        if (file_handles[i].kind != FILE_HANDLE_DIRECTORY) {
            break;
        }
        if (restart_scan != 0u) {
            recomp_directory_restart(&file_handles[i].directory);
        }
        if (!recomp_directory_next(
                &file_handles[i].directory, pattern, &entry)) {
            status = RECOMP_STATUS_NO_MORE_FILES;
            policy = "host-directory-exhausted";
            cursor = file_handles[i].directory.cursor;
            break;
        }
        name_length = strlen(entry.name);
        if (name_length + DIRECTORY_HEADER_SIZE + 1u > length) {
            status = RECOMP_STATUS_INVALID_PARAMETER;
            policy = "directory-entry-buffer-too-small";
            cursor = file_handles[i].directory.cursor;
            break;
        }

        {
            size_t serialized_size = 0u;

            if (!recomp_directory_serialize(
                    &entry,
                    recomp_memory_i8(file_information),
                    length,
                    &serialized_size)) {
                status = RECOMP_STATUS_INVALID_PARAMETER;
                policy = "directory-entry-serialize-failed";
                cursor = file_handles[i].directory.cursor;
                break;
            }
            bytes_written = (uint32_t)serialized_size;
        }
        status = RECOMP_STATUS_SUCCESS;
        policy = "host-directory-entry";
        cursor = file_handles[i].directory.cursor;
        break;
    }

finish:
    if (io_status_block != 0u) {
        *recomp_memory_u32(io_status_block) = status;
        *recomp_memory_u32(io_status_block + 4u) = bytes_written;
    }
    fprintf(
        stderr,
        "recomp kernel: NtQueryDirectoryFile handle=%u class=%u len=%u"
        " restart=%u cursor=%zu policy='%s' status=0x%08x\n",
        (unsigned)guest_handle,
        (unsigned)file_information_class,
        (unsigned)length,
        (unsigned)restart_scan,
        cursor,
        policy,
        (unsigned)status);
    kernel_return(10u, status);
}

static void bridge_nt_write_file(void)
{
    uint32_t guest_handle = kernel_arg(1u);
    uint32_t io_status_block = kernel_arg(5u);
    uint32_t buffer = kernel_arg(6u);
    uint32_t length = kernel_arg(7u);
    uint32_t byte_offset = kernel_arg(8u);

    const uint32_t RECOMP_STATUS_UNSUCCESSFUL = 0xc0000001u;
    uint32_t status = RECOMP_STATUS_INVALID_HANDLE;
    uint32_t bytes_written = 0u;
    uint64_t write_offset = 0u;
    const char *policy = "invalid-file-handle";
    int tracked_handle_seen = 0;

    for (size_t i = 0; i < MAX_FILE_HANDLES; ++i) {
        if (!file_handles[i].active ||
            file_handles[i].guest_handle != guest_handle) {
            continue;
        }

        tracked_handle_seen = 1;
        write_offset = file_handles[i].cursor;
        if (byte_offset != 0u) {
            write_offset = *recomp_memory_u32(byte_offset);
            write_offset |=
                (uint64_t)*recomp_memory_u32(byte_offset + 4u) << 32u;
        }

        if (file_handles[i].kind == FILE_HANDLE_PSEUDO) {
            bytes_written = length;
            file_handles[i].cursor = write_offset + bytes_written;
            status = RECOMP_STATUS_SUCCESS;
            policy = "pseudo-file-write-sink";
            break;
        }
        if (file_handles[i].kind == FILE_HANDLE_DIRECTORY ||
            file_handles[i].host_handle == INVALID_HANDLE_VALUE ||
            (buffer == 0u && length != 0u)) {
            break;
        }

        LARGE_INTEGER distance;
        distance.QuadPart = (LONGLONG)write_offset;
        if (!SetFilePointerEx(
                file_handles[i].host_handle, distance, NULL, FILE_BEGIN)) {
            status = RECOMP_STATUS_UNSUCCESSFUL;
            policy = "host-file-seek-failed";
            break;
        }

        const void *host_buffer = length == 0u
            ? NULL
            : (const void *)recomp_memory_i8(buffer);
        DWORD host_bytes_written = 0u;
        if (!WriteFile(
                file_handles[i].host_handle,
                host_buffer,
                length,
                &host_bytes_written,
                NULL)) {
            status = RECOMP_STATUS_UNSUCCESSFUL;
            policy = "host-file-write-failed";
            break;
        }

        bytes_written = host_bytes_written;
        file_handles[i].cursor = write_offset + bytes_written;
        status = RECOMP_STATUS_SUCCESS;
        policy = "host-file-write";
        break;
    }

    if (!tracked_handle_seen) {
        policy = "untracked-file-handle";
    }

    if (io_status_block != 0u) {
        *recomp_memory_u32(io_status_block) = status;
        *recomp_memory_u32(io_status_block + 4u) = bytes_written;
    }

    fprintf(
        stderr,
        "recomp kernel: NtWriteFile handle=%u buffer=0x%08x len=%u"
        " offset=%llu written=%u policy='%s' status=0x%08x\n",
        (unsigned)guest_handle,
        (unsigned)buffer,
        (unsigned)length,
        (unsigned long long)write_offset,
        (unsigned)bytes_written,
        policy,
        (unsigned)status);

    kernel_return(8u, status);
}

static void bridge_nt_read_file(void)
{
    uint32_t guest_handle = kernel_arg(1u);
    uint32_t io_status_block = kernel_arg(5u);
    uint32_t buffer = kernel_arg(6u);
    uint32_t length = kernel_arg(7u);
    uint32_t byte_offset = kernel_arg(8u);

    const uint32_t RECOMP_STATUS_UNSUCCESSFUL = 0xc0000001u;
    const uint32_t RECOMP_STATUS_END_OF_FILE = 0xc0000011u;
    uint32_t status = RECOMP_STATUS_INVALID_HANDLE;
    uint32_t bytes_read = 0u;
    uint64_t read_offset = 0u;
    const char *policy = "invalid-file-handle";
    int tracked_handle_seen = 0;

    for (size_t i = 0; i < MAX_FILE_HANDLES; ++i) {
        if (!file_handles[i].active ||
            file_handles[i].guest_handle != guest_handle) {
            continue;
        }

        tracked_handle_seen = 1;
        read_offset = file_handles[i].cursor;
        if (byte_offset != 0u) {
            read_offset = *recomp_memory_u32(byte_offset);
            read_offset |=
                (uint64_t)*recomp_memory_u32(byte_offset + 4u) << 32u;
        }

        if (file_handles[i].kind == FILE_HANDLE_PSEUDO) {
            status = RECOMP_STATUS_END_OF_FILE;
            policy = "pseudo-empty-file-eof";
            break;
        }
        if (file_handles[i].kind == FILE_HANDLE_DIRECTORY ||
            file_handles[i].host_handle == INVALID_HANDLE_VALUE ||
            (buffer == 0u && length != 0u)) {
            break;
        }

        LARGE_INTEGER distance;
        distance.QuadPart = (LONGLONG)read_offset;
        if (!SetFilePointerEx(
                file_handles[i].host_handle, distance, NULL, FILE_BEGIN)) {
            status = RECOMP_STATUS_UNSUCCESSFUL;
            policy = "host-file-seek-failed";
            break;
        }

        void *host_buffer = length == 0u
            ? NULL
            : (void *)recomp_memory_i8(buffer);
        DWORD host_bytes_read = 0u;
        if (!ReadFile(
                file_handles[i].host_handle,
                host_buffer,
                length,
                &host_bytes_read,
                NULL)) {
            status = RECOMP_STATUS_UNSUCCESSFUL;
            policy = "host-file-read-failed";
            break;
        }

        bytes_read = host_bytes_read;
        file_handles[i].cursor = read_offset + bytes_read;
        status = bytes_read == 0u && length != 0u
            ? RECOMP_STATUS_END_OF_FILE
            : RECOMP_STATUS_SUCCESS;
        policy = "host-file-read";
        break;
    }

    if (!tracked_handle_seen) {
        policy = "untracked-file-handle";
    }

    if (io_status_block != 0u) {
        *recomp_memory_u32(io_status_block) = status;
        *recomp_memory_u32(io_status_block + 4u) = bytes_read;
    }

    fprintf(
        stderr,
        "recomp kernel: NtReadFile handle=%u buffer=0x%08x len=%u"
        " offset=%llu read=%u policy='%s' status=0x%08x\n",
        (unsigned)guest_handle,
        (unsigned)buffer,
        (unsigned)length,
        (unsigned long long)read_offset,
        (unsigned)bytes_read,
        policy,
        (unsigned)status);

    kernel_return(8u, status);
}

/* A synchronous device query succeeds with an empty status block. The one
   IOCTL this XBE issues on the CD-ROM device is a query whose input the
   native host confirms by setting three bytes past the caller's buffer; that
   confirmation is what the guest reads back, so we reproduce it even though
   it lands beyond the declared input length. */
static void bridge_nt_device_io_control_file(void)
{
    uint32_t io_status_block = kernel_arg(5u);
    uint32_t io_control_code = kernel_arg(6u);
    uint32_t input_buffer = kernel_arg(7u);
    uint32_t input_buffer_length = kernel_arg(8u);

    uint32_t status = RECOMP_STATUS_SUCCESS;

    if (io_status_block != 0u) {
        *recomp_memory_u32(io_status_block) = status;
        *recomp_memory_u32(io_status_block + 4u) = 0u;
    }
    /* The 0x4d014 query is the CRT's real-time-clock read. The guest reads
       three response bytes back from the query struct at offsets 0x36..0x38
       and treats all-zero as an invalid clock, which sends it to the
       dashboard. The struct is declared 0x2c bytes but the response occupies
       those higher offsets, so set them for this code regardless of the
       input-length argument. The native host's >=0x39 gate was wrong. */
    if (io_control_code == 0x0004d014u && input_buffer != 0u) {
        *recomp_memory_i8(input_buffer + 0x36u) = 1;
        *recomp_memory_i8(input_buffer + 0x37u) = 1;
        *recomp_memory_i8(input_buffer + 0x38u) = 1;
    }

    fprintf(
        stderr,
        "recomp kernel: NtDeviceIoControlFile code=0x%08x input=0x%08x len=%u status=0x%08x\n",
        (unsigned)io_control_code,
        (unsigned)input_buffer,
        (unsigned)input_buffer_length,
        (unsigned)status);

    kernel_return(10u, status);
}

/* Volume geometry for the mounted partition. The native host reports a
   plausible fixed size so a caller sizing a cache or free-space check gets a
   consistent answer; the values are not tied to the real host filesystem. */
static void bridge_nt_query_volume_information_file(void)
{
    uint32_t io_status_block = kernel_arg(2u);
    uint32_t fs_information = kernel_arg(3u);
    uint32_t length = kernel_arg(4u);
    uint32_t fs_information_class = kernel_arg(5u);

    const uint32_t FILE_FS_SIZE_INFORMATION = 3u;
    const uint32_t FS_SIZE_INFORMATION_LENGTH = 0x18u;
    uint32_t status = RECOMP_STATUS_SUCCESS;

    if (io_status_block != 0u) {
        *recomp_memory_u32(io_status_block) = status;
        *recomp_memory_u32(io_status_block + 4u) =
            fs_information_class == FILE_FS_SIZE_INFORMATION
                ? (length < FS_SIZE_INFORMATION_LENGTH
                       ? length
                       : FS_SIZE_INFORMATION_LENGTH)
                : 0u;
    }
    if (fs_information_class == FILE_FS_SIZE_INFORMATION &&
        fs_information != 0u && length >= FS_SIZE_INFORMATION_LENGTH) {
        *recomp_memory_u32(fs_information + 0u) = 0x00100000u;
        *recomp_memory_u32(fs_information + 4u) = 0u;
        *recomp_memory_u32(fs_information + 8u) = 0x00080000u;
        *recomp_memory_u32(fs_information + 12u) = 0u;
        *recomp_memory_u32(fs_information + 16u) = 0x20u;
        *recomp_memory_u32(fs_information + 20u) = 0x200u;
    }

    fprintf(
        stderr,
        "recomp kernel: NtQueryVolumeInformationFile class=%u len=%u status=0x%08x\n",
        (unsigned)fs_information_class,
        (unsigned)length,
        (unsigned)status);

    kernel_return(5u, status);
}

/* Mounting a drive letter as a symbolic link succeeds by registering the
   mapping; there is no real device underneath in the bring-up model. The
   names are logged so the mount sequence is visible. */
static void copy_guest_object_string(uint32_t address, char *out, size_t size)
{
    if (address == 0u || read_guest_ansi_string(address, out, size) == 0) {
        snprintf(out, size, "(null)");
    }
}

static void bridge_io_create_symbolic_link(void)
{
    uint32_t link_name = kernel_arg(1u);
    uint32_t device_name = kernel_arg(2u);
    char link[256];
    char device[256];

    copy_guest_object_string(link_name, link, sizeof link);
    copy_guest_object_string(device_name, device, sizeof device);
    ensure_symbolic_links_initialized();
    fprintf(
        stderr,
        "recomp kernel: IoCreateSymbolicLink '%s' -> '%s'\n",
        link,
        device);
    kernel_return(
        2u,
        recomp_symbolic_link_create(&symbolic_links, link, device)
            ? RECOMP_STATUS_SUCCESS
            : RECOMP_STATUS_NO_MEMORY);
}

static void bridge_io_delete_symbolic_link(void)
{
    uint32_t link_name = kernel_arg(1u);
    char link[256];

    copy_guest_object_string(link_name, link, sizeof link);
    ensure_symbolic_links_initialized();
    kernel_return(
        1u,
        recomp_symbolic_link_delete(&symbolic_links, link)
            ? RECOMP_STATUS_SUCCESS
            : RECOMP_STATUS_OBJECT_NAME_NOT_FOUND);
}

static void bridge_nt_open_symbolic_link_object(void)
{
    const uint32_t RECOMP_STATUS_INVALID_PARAMETER = 0xc000000du;
    uint32_t link_handle = kernel_arg(1u);
    uint32_t object_attributes = kernel_arg(2u);
    uint32_t guest_handle = 0u;
    uint32_t status = RECOMP_STATUS_OBJECT_NAME_NOT_FOUND;
    char link[256] = {0};

    ensure_symbolic_links_initialized();
    if (link_handle == 0u ||
        !read_guest_object_name(object_attributes, link, sizeof link)) {
        status = RECOMP_STATUS_INVALID_PARAMETER;
    } else if (recomp_symbolic_link_open(
                   &symbolic_links, link, &guest_handle)) {
        *recomp_memory_u32(link_handle) = guest_handle;
        status = RECOMP_STATUS_SUCCESS;
    }
    fprintf(
        stderr,
        "recomp kernel: NtOpenSymbolicLinkObject path='%s' handle=%u"
        " status=0x%08x\n",
        link,
        (unsigned)guest_handle,
        (unsigned)status);
    kernel_return(2u, status);
}

static void bridge_nt_query_symbolic_link_object(void)
{
    const uint32_t RECOMP_STATUS_INVALID_PARAMETER = 0xc000000du;
    uint32_t guest_handle = kernel_arg(1u);
    uint32_t target_string = kernel_arg(2u);
    uint32_t returned_length = kernel_arg(3u);
    uint32_t status = RECOMP_STATUS_INVALID_HANDLE;
    uint16_t maximum_length = 0u;
    uint32_t buffer = 0u;
    char target[RECOMP_SYMBOLIC_LINK_NAME_SIZE] = {0};
    size_t length = 0u;

    ensure_symbolic_links_initialized();
    if (recomp_symbolic_link_query(
            &symbolic_links, guest_handle, target, sizeof target)) {
        length = strlen(target);
        if (target_string == 0u) {
            status = RECOMP_STATUS_INVALID_PARAMETER;
        } else {
            maximum_length = *recomp_memory_u16(target_string + 2u);
            buffer = *recomp_memory_u32(target_string + 4u);
            if (buffer == 0u || maximum_length == 0u ||
                length >= maximum_length) {
                status = RECOMP_STATUS_INVALID_PARAMETER;
            } else {
                memcpy(recomp_memory_i8(buffer), target, length + 1u);
                *recomp_memory_u16(target_string) = (uint16_t)length;
                if (returned_length != 0u) {
                    *recomp_memory_u32(returned_length) = (uint32_t)length;
                }
                status = RECOMP_STATUS_SUCCESS;
            }
        }
    }
    fprintf(
        stderr,
        "recomp kernel: NtQuerySymbolicLinkObject handle=%u target='%s'"
        " length=%u status=0x%08x\n",
        (unsigned)guest_handle,
        target,
        (unsigned)length,
        (unsigned)status);
    kernel_return(3u, status);
}

static void bridge_nt_close(void)
{
    uint32_t guest_handle = kernel_arg(1u);

    ensure_symbolic_links_initialized();
    (void)recomp_symbolic_link_close(&symbolic_links, guest_handle);
    for (size_t i = 0; i < MAX_FILE_HANDLES; ++i) {
        if (file_handles[i].active && file_handles[i].guest_handle == guest_handle) {
            if (file_handles[i].host_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(file_handles[i].host_handle);
            }
            file_handles[i].active = 0;
            break;
        }
    }
    kernel_return(1u, RECOMP_STATUS_SUCCESS);
}

RecompFunction recomp_kernel_file(uint32_t ordinal)
{
    switch (ordinal) {
    case 67u: return bridge_io_create_symbolic_link;
    case 68u: return bridge_io_delete_symbolic_link;
    case 187u: return bridge_nt_close;
    case 190u: return bridge_nt_create_file;
    case 196u: return bridge_nt_device_io_control_file;
    case 202u: return bridge_nt_open_file;
    case 203u: return bridge_nt_open_symbolic_link_object;
    case 207u: return bridge_nt_query_directory_file;
    case 211u: return bridge_nt_query_information_file;
    case 215u: return bridge_nt_query_symbolic_link_object;
    case 218u: return bridge_nt_query_volume_information_file;
    case 219u: return bridge_nt_read_file;
    case 236u: return bridge_nt_write_file;
    default: return NULL;
    }
}
