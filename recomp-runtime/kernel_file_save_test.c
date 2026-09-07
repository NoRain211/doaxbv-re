#include "kernel_abi.h"
#include "save_transaction.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

enum {
    TEST_BASE = 0x2b000000u,
    TEST_SIZE = 0x1000u,
    TEST_STACK = TEST_BASE + 0x100u,
    TEST_HANDLE = TEST_BASE + 0x200u,
    TEST_IOSB = TEST_BASE + 0x210u,
    TEST_ATTRIBUTES = TEST_BASE + 0x300u,
    TEST_NAME = TEST_BASE + 0x320u,
    TEST_PATH = TEST_BASE + 0x400u,
    TEST_BUFFER = TEST_BASE + 0x600u,
    TEST_INFORMATION = TEST_BASE + 0x700u,
};

static int expect(const char *label, int condition)
{
    if (condition) return 1;
    fprintf(stderr, "kernel file save: %s failed\n", label);
    return 0;
}

static uint32_t invoke(uint32_t ordinal, const uint32_t *args,
    unsigned count, int *passed)
{
    RecompFunction function = recomp_kernel_file(ordinal);
    *passed &= expect("ordinal registered", function != NULL);
    if (function == NULL) return 0xc0000002u;
    *recomp_memory_u32(TEST_STACK) = 0x0010abcdu;
    for (unsigned i = 0u; i < count; ++i)
        *recomp_memory_u32(TEST_STACK + 4u * (i + 1u)) = args[i];
    recomp_runtime.registers.esp = TEST_STACK;
    recomp_runtime.registers.eax = 0xccccccccu;
    function();
    *passed &= expect("stdcall ESP", recomp_runtime.registers.esp ==
        TEST_STACK + 4u * (count + 1u));
    *passed &= expect("return address preserved",
        *recomp_memory_u32(TEST_STACK) == 0x0010abcdu);
    return recomp_runtime.registers.eax;
}

static void set_path(const char *path)
{
    uint32_t length = (uint32_t)strlen(path);
    memcpy(recomp_memory_i8(TEST_PATH), path, length + 1u);
    *recomp_memory_u32(TEST_NAME) = length | ((length + 1u) << 16u);
    *recomp_memory_u32(TEST_NAME + 4u) = TEST_PATH;
    *recomp_memory_u32(TEST_ATTRIBUTES) = 0u;
    *recomp_memory_u32(TEST_ATTRIBUTES + 4u) = TEST_NAME;
    *recomp_memory_u32(TEST_ATTRIBUTES + 8u) = 0u;
}

static uint32_t create_file(const char *path, uint32_t access,
    uint32_t disposition, uint32_t *status, int *passed)
{
    const uint32_t args[] = {TEST_HANDLE, access, TEST_ATTRIBUTES, TEST_IOSB,
        0u, 0u, 3u, disposition, 0u};
    set_path(path);
    *status = invoke(190u, args, 9u, passed);
    *passed &= expect("create IOSB status", *recomp_memory_u32(TEST_IOSB) == *status);
    return *recomp_memory_u32(TEST_HANDLE);
}

static uint32_t write_file(uint32_t handle, const char *bytes,
    uint32_t length, int *passed)
{
    const uint32_t args[] = {handle, 0u, 0u, 0u, TEST_IOSB,
        TEST_BUFFER, length, 0u};
    uint32_t status;
    memcpy(recomp_memory_i8(TEST_BUFFER), bytes, length);
    status = invoke(236u, args, 8u, passed);
    *passed &= expect("write IOSB status", *recomp_memory_u32(TEST_IOSB) == status);
    *passed &= expect("write IOSB byte count",
        *recomp_memory_u32(TEST_IOSB + 4u) == (status == 0u ? length : 0u));
    return status;
}

static uint32_t set_information(uint32_t handle, uint32_t kind,
    uint32_t value, uint32_t length, int *passed)
{
    const uint32_t args[] = {handle, TEST_IOSB, TEST_INFORMATION, length, kind};
    uint32_t status;
    *recomp_memory_u32(TEST_INFORMATION) = value;
    *recomp_memory_u32(TEST_INFORMATION + 4u) = 0u;
    status = invoke(226u, args, 5u, passed);
    *passed &= expect("set-information IOSB status",
        *recomp_memory_u32(TEST_IOSB) == status);
    return status;
}

static int close_file(uint32_t handle, int *passed)
{
    return expect("close status", invoke(187u, &handle, 1u, passed) == 0u);
}

static int file_equals(const char *path, const char *expected)
{
    char bytes[32];
    DWORD count = 0u;
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    int equal;
    if (file == INVALID_HANDLE_VALUE) return 0;
    equal = ReadFile(file, bytes, sizeof bytes, &count, NULL) &&
        count == strlen(expected) && memcmp(bytes, expected, count) == 0;
    CloseHandle(file);
    return equal;
}

int recomp_kernel_file_save_test(void)
{
    static uint8_t memory[TEST_SIZE];
    const RecompMemoryRegion region = {
        .address = TEST_BASE, .size = sizeof memory, .data = memory,
    };
    const char *old_root = recomp_disc_root_path;
    const char guest_file[] = "\\Device\\Harddisk0\\partition1\\UDATA\\profile.dat";
    char temporary[MAX_PATH], root[MAX_PATH], live[MAX_PATH], path[MAX_PATH];
    uint32_t status, handle;
    int passed = 1;

    if (GetTempPathA(sizeof temporary, temporary) == 0u ||
        GetTempFileNameA(temporary, "rsv", 0u, root) == 0u ||
        strlen(root) > MAX_PATH - 100u || !DeleteFileA(root) ||
        !CreateDirectoryA(root, NULL)) {
        return expect("temporary storage directory", 0);
    }
    snprintf(live, sizeof live, "%s\\.recomp-storage\\partition1\\UDATA", root);
    snprintf(path, sizeof path, "%s\\profile.dat", live);
    memset(memory, 0, sizeof memory);
    recomp_runtime_init(&region, 1u, NULL, 0u, NULL, 0u);
    recomp_disc_root_path = root;
    if (!recomp_save_initialize(root)) {
        passed = expect("initialize", 0);
        goto cleanup;
    }

    passed &= expect("begin complete write", recomp_save_begin(0u));
    passed &= expect("owner active", recomp_save_active(0u) && recomp_save_pending());
    handle = create_file(guest_file, GENERIC_READ | GENERIC_WRITE, 3u, &status, &passed);
    passed &= expect("create writable file", status == 0u && handle != 0u);
    passed &= expect("open protected handle blocks commit", !recomp_kernel_save_handles_closed(0u));
    passed &= expect("complete write", write_file(handle, "abcdef", 6u, &passed) == 0u);
    passed &= expect("set position", set_information(handle, 14u, 2u, 8u, &passed) == 0u);
    passed &= expect("write at guest cursor", write_file(handle, "XY", 2u, &passed) == 0u);
    passed &= expect("set EOF", set_information(handle, 20u, 4u, 8u, &passed) == 0u);
    passed &= close_file(handle, &passed);
    passed &= expect("closed handles allow commit", recomp_kernel_save_handles_closed(0u));
    passed &= expect("commit complete writes", recomp_save_end(0u, true));
    passed &= expect("committed bytes and EOF", file_equals(path, "abXY"));
    passed &= expect("commit clears operation", !recomp_save_pending());

    passed &= expect("begin optional lookup", recomp_save_begin(0u));
    set_path("\\Device\\Harddisk0\\partition1\\UDATA\\missing.dat");
    {
        const uint32_t args[] = {TEST_HANDLE, GENERIC_READ, TEST_ATTRIBUTES, TEST_IOSB, 3u, 0u};
        status = invoke(202u, args, 6u, &passed);
    }
    passed &= expect("optional read missing", status != 0u && *recomp_memory_u32(TEST_HANDLE) == 0u);
    passed &= expect("optional missing read does not poison", recomp_save_end(0u, true));
    passed &= expect("optional read preserves payload", file_equals(path, "abXY"));

    passed &= expect("begin optional read seek", recomp_save_begin(0u));
    set_path(guest_file);
    {
        const uint32_t args[] = {TEST_HANDLE, GENERIC_READ, TEST_ATTRIBUTES, TEST_IOSB, 3u, 0u};
        status = invoke(202u, args, 6u, &passed);
    }
    handle = *recomp_memory_u32(TEST_HANDLE);
    passed &= expect("optional read open", status == 0u && handle != 0u);
    passed &= expect("optional read position", set_information(handle, 14u, 2u, 8u, &passed) == 0u);
    {
        const uint32_t args[] = {handle, 0u, 0u, 0u, TEST_IOSB, TEST_BUFFER, 2u, 0u};
        status = invoke(219u, args, 8u, &passed);
    }
    passed &= expect("read uses optional handle cursor", status == 0u &&
        *recomp_memory_u32(TEST_IOSB) == 0u && *recomp_memory_u32(TEST_IOSB + 4u) == 2u &&
        memcmp(recomp_memory_i8(TEST_BUFFER), "XY", 2u) == 0);
    passed &= close_file(handle, &passed);
    passed &= expect("optional read seek does not poison", recomp_save_end(0u, true));
    passed &= expect("optional read seek preserves payload", file_equals(path, "abXY"));

    passed &= expect("begin failed required open", recomp_save_begin(0u));
    handle = create_file("\\Device\\Harddisk0\\partition1\\UDATA\\profile.dat\\child.dat",
        GENERIC_WRITE, 3u, &status, &passed);
    passed &= expect("parent-file conflict fails open", status != 0u && handle == 0u);
    passed &= expect("required open failure aborts", !recomp_save_end(0u, true));
    passed &= expect("failed open preserves payload", file_equals(path, "abXY"));

    passed &= expect("begin failed required NtOpenFile", recomp_save_begin(0u));
    handle = create_file(guest_file, GENERIC_READ | GENERIC_WRITE, 1u, &status, &passed);
    passed &= expect("open before required NtOpenFile failure", status == 0u && handle != 0u);
    passed &= expect("write before required NtOpenFile failure", write_file(handle, "zz", 2u, &passed) == 0u);
    passed &= close_file(handle, &passed);
    set_path("\\Device\\Harddisk0\\partition1\\UDATA\\missing.dat");
    {
        const uint32_t args[] = {TEST_HANDLE, GENERIC_WRITE, TEST_ATTRIBUTES, TEST_IOSB, 3u, 0u};
        status = invoke(202u, args, 6u, &passed);
    }
    passed &= expect("required NtOpenFile fails", status != 0u &&
        *recomp_memory_u32(TEST_HANDLE) == 0u && *recomp_memory_u32(TEST_IOSB) == status);
    passed &= expect("required NtOpenFile failure aborts success end", !recomp_save_end(0u, true));
    passed &= expect("required NtOpenFile failure rolls back earlier write", file_equals(path, "abXY"));

    passed &= expect("begin failed write", recomp_save_begin(0u));
    handle = create_file(guest_file, GENERIC_READ | GENERIC_WRITE, 1u, &status, &passed);
    passed &= expect("open for locked write", status == 0u && handle != 0u);
    {
        HANDLE blocker = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        int locked = blocker != INVALID_HANDLE_VALUE && LockFile(blocker, 0u, 0u, 4u, 0u);
        passed &= expect("real Win32 byte-range lock", locked);
        if (locked) {
            passed &= expect("locked write fails", write_file(handle, "NOPE", 4u, &passed) != 0u);
            UnlockFile(blocker, 0u, 0u, 4u, 0u);
        } else {
            recomp_save_note_failure(0u);
        }
        if (blocker != INVALID_HANDLE_VALUE) CloseHandle(blocker);
    }
    passed &= close_file(handle, &passed);
    passed &= expect("failed write aborts", !recomp_save_end(0u, true));
    passed &= expect("failed write restores payload", file_equals(path, "abXY"));

    passed &= expect("begin failed SetInfo", recomp_save_begin(0u));
    handle = create_file(guest_file, GENERIC_READ | GENERIC_WRITE, 1u, &status, &passed);
    passed &= expect("open for SetInfo", status == 0u && handle != 0u);
    passed &= expect("write before failed SetInfo", write_file(handle, "zz", 2u, &passed) == 0u);
    passed &= expect("short position structure fails", set_information(handle, 14u, 0u, 4u, &passed) != 0u);
    passed &= close_file(handle, &passed);
    passed &= expect("SetInfo failure aborts", !recomp_save_end(0u, true));
    passed &= expect("SetInfo failure rolls back earlier write", file_equals(path, "abXY"));

    /* An old synthetic handle must not acknowledge a protected write. */
    handle = create_file("D:\\metadata.xbx", GENERIC_WRITE, 3u, &status, &passed);
    passed &= expect("create legacy pseudo handle", status == 0u && handle != 0u);
    passed &= expect("begin pseudo-write rejection", recomp_save_begin(0u));
    passed &= expect("active pseudo write fails", write_file(handle, "lost", 4u, &passed) != 0u);
    passed &= close_file(handle, &passed);
    passed &= expect("pseudo write failure aborts", !recomp_save_end(0u, true));
    passed &= expect("pseudo failure preserves payload", file_equals(path, "abXY"));

cleanup:
    /* Reinitialization releases the documented lifetime journal lock. A null
       root leaves the backend disabled after this fixture. */
    passed &= expect("release backend ownership", !recomp_save_initialize(NULL));
    recomp_disc_root_path = old_root;
    DeleteFileA(path);
    RemoveDirectoryA(live);
    snprintf(path, sizeof path, "%s\\.recomp-storage\\partition1", root);
    RemoveDirectoryA(path);
    snprintf(path, sizeof path, "%s\\.recomp-storage\\save-undo-v1\\version", root);
    DeleteFileA(path);
    snprintf(path, sizeof path, "%s\\.recomp-storage\\save-undo-v1\\lock", root);
    DeleteFileA(path);
    snprintf(path, sizeof path, "%s\\.recomp-storage\\save-undo-v1", root);
    RemoveDirectoryA(path);
    snprintf(path, sizeof path, "%s\\.recomp-storage", root);
    RemoveDirectoryA(path);
    passed &= expect("temporary storage removed", RemoveDirectoryA(root));
    return passed;
}
