#include "save_transaction.h"

#include <filesystem>
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {
fs::path storage, live, journal;
bool ready, failed;
uint32_t active_owner, depth;
const char version[] = "recomp-save-undo-v1\n";
#ifdef _WIN32
HANDLE journal_lock = INVALID_HANDLE_VALUE;
#endif

void require(bool condition)
{
    if (!condition) throw std::runtime_error("invalid save journal or path");
}

bool exists_plain(const fs::path &path)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        require(error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND);
        return false;
    }
    require((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0);
#else
    require(!fs::is_symlink(fs::symlink_status(path)));
#endif
    return fs::exists(path);
}

void check_parents(const fs::path &path)
{
    fs::path part;
    for (const auto &component : path) {
        part /= component;
        if (exists_plain(part)) require(fs::is_directory(part));
    }
}

void check_tree(const fs::path &path)
{
    if (!exists_plain(path)) return;
    require(fs::is_directory(path) || fs::is_regular_file(path));
    if (fs::is_directory(path)) {
        for (const auto &entry : fs::directory_iterator(path))
            check_tree(entry.path());
    }
}

void remove_tree(const fs::path &path)
{
    check_tree(path);
    fs::remove_all(path);
}

void rename_record(const fs::path &source, const fs::path &destination)
{
#ifdef _WIN32
    const DWORD started = GetTickCount();
    for (;;) {
        if (MoveFileExW(source.c_str(), destination.c_str(), 0)) return;
        const DWORD error = GetLastError();
        const DWORD elapsed = GetTickCount() - started;
        /* A briefly opened child can deny renaming its parent directory. */
        if ((error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION &&
             error != ERROR_LOCK_VIOLATION) || elapsed >= 500u) {
            std::fprintf(stderr, "save journal rename failed: win32=%lu elapsed_ms=%lu\n",
                static_cast<unsigned long>(error), static_cast<unsigned long>(elapsed));
            throw fs::filesystem_error("rename save journal", source, destination,
                std::error_code(error, std::system_category()));
        }
        Sleep((500u - elapsed < 10u) ? 500u - elapsed : 10u);
    }
#else
    fs::rename(source, destination);
#endif
}

void copy_tree(const fs::path &source, const fs::path &destination)
{
    require(exists_plain(source));
    if (fs::is_directory(source)) {
        require(fs::create_directory(destination));
        for (const auto &entry : fs::directory_iterator(source))
            copy_tree(entry.path(), destination / entry.path().filename());
    } else {
        require(fs::is_regular_file(source));
        require(fs::copy_file(source, destination));
    }
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    require(GetFileAttributesExW(source.c_str(), GetFileExInfoStandard, &attributes) != 0);
    HANDLE handle = CreateFileW(destination.c_str(), FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    require(handle != INVALID_HANDLE_VALUE);
    bool copied = SetFileTime(handle, &attributes.ftCreationTime,
        &attributes.ftLastAccessTime, &attributes.ftLastWriteTime) != 0;
    bool closed = CloseHandle(handle) != 0;
    require(copied && closed);
#else
    fs::last_write_time(destination, fs::last_write_time(source));
#endif
}

void write_marker(const fs::path &path, const char *text)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
    stream.close();
    require(!stream.fail());
}

void check_marker(const fs::path &path, const std::string &expected)
{
    require(exists_plain(path) && fs::is_regular_file(path));
    require(fs::file_size(path) == expected.size());
    std::ifstream stream(path, std::ios::binary);
    std::string actual(expected.size(), '\0');
    if (!actual.empty()) stream.read(&actual[0], actual.size());
    require(!stream.fail() && actual == expected);
}

void check_record(const fs::path &record, bool complete)
{
    require(exists_plain(record) && fs::is_directory(record));
    for (const auto &entry : fs::directory_iterator(record)) {
        const auto name = entry.path().filename();
        require(name == "present" || name == "absent" || name == "backup");
        check_tree(entry.path());
        if (name == "backup") require(fs::is_directory(entry.path()));
        else check_marker(entry.path(), "");
    }
    bool present = exists_plain(record / "present");
    bool absent = exists_plain(record / "absent");
    require(!(present && absent));
    if (complete) {
        require(present || absent);
        require(exists_plain(record / "backup") == present);
    }
}

void check_journal()
{
    check_parents(storage);
    check_parents(live.parent_path());
    require(exists_plain(journal) && fs::is_directory(journal));
    check_marker(journal / "version", version);
    unsigned records = 0;
    for (const auto &entry : fs::directory_iterator(journal)) {
        const auto name = entry.path().filename();
        if (name == "version") continue;
        if (name == "lock") {
            require(exists_plain(entry.path()) && fs::is_regular_file(entry.path()));
            continue;
        }
        require(name == "staging" || name == "pending" || name == "committed");
        check_record(entry.path(), name == "pending");
        ++records;
    }
    require(records <= 1);
}

void recover()
{
    check_journal();
    const auto pending = journal / "pending";
    if (exists_plain(pending)) {
        /* Keep the complete undo tree until restoration finishes. A restart
           during removal/copy repeats this operation from the same backup. */
        check_tree(live);
        remove_tree(live);
        if (exists_plain(pending / "present")) {
            fs::create_directories(live.parent_path());
            copy_tree(pending / "backup", live);
        }
        rename_record(pending, journal / "committed");
    }
    remove_tree(journal / "committed");
    remove_tree(journal / "staging");
}
}

extern "C" bool recomp_save_initialize(const char *disc_root)
{
#ifdef _WIN32
    if (journal_lock != INVALID_HANDLE_VALUE) {
        CloseHandle(journal_lock);
        journal_lock = INVALID_HANDLE_VALUE;
    }
#endif
    ready = false;
    depth = 0;
    failed = false;
    try {
        require(disc_root != nullptr && *disc_root != '\0');
        fs::path root = fs::absolute(disc_root).lexically_normal();
        check_parents(root);
        require(exists_plain(root) && fs::is_directory(root));
        storage = root / ".recomp-storage";
        live = storage / "partition1" / "UDATA";
        journal = storage / "save-undo-v1";
        check_parents(storage);
        fs::create_directories(storage);
        bool created = false;
        if (!exists_plain(journal)) created = fs::create_directory(journal);
        require(exists_plain(journal) && fs::is_directory(journal));
#ifdef _WIN32
        const auto lock_path = journal / "lock";
        if (exists_plain(lock_path)) require(fs::is_regular_file(lock_path));
        journal_lock = CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_ALWAYS, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        require(journal_lock != INVALID_HANDLE_VALUE);
        BY_HANDLE_FILE_INFORMATION lock_info;
        require(GetFileInformationByHandle(journal_lock, &lock_info) != 0);
        require((lock_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0);
        require(lock_info.nFileSizeHigh == 0 && lock_info.nFileSizeLow == 0);
#endif
        if (created) {
            write_marker(journal / "version", version);
        }
        recover();
        check_tree(live);
        ready = true;
        return true;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "save recovery failed: %s\n", error.what());
#ifdef _WIN32
        if (journal_lock != INVALID_HANDLE_VALUE) {
            CloseHandle(journal_lock);
            journal_lock = INVALID_HANDLE_VALUE;
        }
#endif
        return false;
    }
}

extern "C" bool recomp_save_begin(uint32_t owner)
{
    if (!ready) return false;
    if (depth != 0) {
        if (owner != active_owner || depth == (std::numeric_limits<uint32_t>::max)())
            return false;
        ++depth;
        return true;
    }
    try {
        recover();
        check_tree(live);
        require(!exists_plain(live) || fs::is_directory(live));
        const auto staging = journal / "staging";
        require(fs::create_directory(staging));
        bool present = exists_plain(live);
        write_marker(staging / (present ? "present" : "absent"), "");
        if (present) copy_tree(live, staging / "backup");
        check_record(staging, true);
        rename_record(staging, journal / "pending");
        active_owner = owner;
        depth = 1;
        failed = false;
        return true;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "save begin failed: %s\n", error.what());
        ready = false;
        return false;
    }
}

extern "C" bool recomp_save_end(uint32_t owner, bool success)
{
    if (!ready || depth == 0 || owner != active_owner) return false;
    failed = failed || !success;
    if (--depth != 0) return !failed;
    try {
        check_journal();
        require(exists_plain(journal / "pending"));
        if (!failed) {
            check_tree(live);
            rename_record(journal / "pending", journal / "committed");
        }
        recover();
        return !failed;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "save end failed: %s\n", error.what());
        ready = false;
        return false;
    }
}

extern "C" void recomp_save_note_failure(uint32_t owner)
{
    if (depth != 0 && owner == active_owner) failed = true;
}

extern "C" bool recomp_save_active(uint32_t owner)
{
    return ready && depth != 0 && owner == active_owner;
}

extern "C" bool recomp_save_pending(void)
{
    return depth != 0;
}
