#include "save_transaction.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {
fs::path storage, live, journal, undo;
bool ready, failed;
uint32_t active_owner, depth;
const char version[] = "recomp-save-undo-v1\n";
/* journal/undo holds the whole UDATA tree as it was before the operation, as
   one file so a save costs a few file operations. Its header records the
   image size: an image cut short by an interruption never reached live data
   and is discarded. Deleting it commits the operation. */
const char undo_magic[8] = {'r', 's', 'u', 'n', 'd', 'o', '0', '1'};
constexpr size_t undo_header = sizeof undo_magic + sizeof(uint64_t);
#ifdef _WIN32
HANDLE journal_lock = INVALID_HANDLE_VALUE;
#endif

struct Times { uint64_t creation, access, write; };
struct Node { Times times; fs::path path; bool directory; std::string_view data; };

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
    return true;
#else
    require(!fs::is_symlink(fs::symlink_status(path)));
    return fs::exists(path);
#endif
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

void remove_file(const fs::path &path)
{
#ifdef _WIN32
    const DWORD started = GetTickCount();
    for (;;) {
        if (DeleteFileW(path.c_str())) return;
        const DWORD error = GetLastError();
        const DWORD elapsed = GetTickCount() - started;
        /* Scanners and indexers briefly open newly written files. */
        if ((error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION &&
             error != ERROR_LOCK_VIOLATION) || elapsed >= 500u) {
            std::fprintf(stderr, "save journal delete failed: win32=%lu elapsed_ms=%lu\n",
                static_cast<unsigned long>(error), static_cast<unsigned long>(elapsed));
            throw fs::filesystem_error("delete save journal", path,
                std::error_code(error, std::system_category()));
        }
        Sleep((500u - elapsed < 10u) ? 500u - elapsed : 10u);
    }
#else
    require(fs::remove(path));
#endif
}

void write_file(const fs::path &path, std::string_view data)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    stream.close();
    require(!stream.fail());
}

std::string read_file(const fs::path &path)
{
    std::string data(static_cast<size_t>(fs::file_size(path)), '\0');
    std::ifstream stream(path, std::ios::binary);
    stream.read(data.data(), static_cast<std::streamsize>(data.size()));
    require(!stream.fail());
    return data;
}

void check_marker(const fs::path &path, const std::string &expected)
{
    require(exists_plain(path) && fs::is_regular_file(path));
    require(fs::file_size(path) == expected.size());
    require(read_file(path) == expected);
}

void put(std::string &out, uint64_t value)
{
    out.append(reinterpret_cast<const char *>(&value), sizeof value);
}

void put(std::string &out, std::string_view bytes)
{
    put(out, uint64_t(bytes.size()));
    out.append(bytes);
}

#ifdef _WIN32
uint64_t ticks(FILETIME time)
{
    return (uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

FILETIME filetime(uint64_t ticks)
{
    return {static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32)};
}
#endif

void save_node(std::string &out, const fs::path &path, const fs::path &relative)
{
    Times times{};
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    require(GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes) != 0);
    require((attributes.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0);
    const bool directory = (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    times = {ticks(attributes.ftCreationTime), ticks(attributes.ftLastAccessTime),
        ticks(attributes.ftLastWriteTime)};
#else
    const auto status = fs::symlink_status(path);
    require(fs::is_directory(status) || fs::is_regular_file(status));
    const bool directory = fs::is_directory(status);
    times.write = static_cast<uint64_t>(fs::last_write_time(path).time_since_epoch().count());
#endif
    out += directory ? 'D' : 'F';
    put(out, times.creation);
    put(out, times.access);
    put(out, times.write);
    const auto &name = relative.native();
    put(out, std::string_view(reinterpret_cast<const char *>(name.data()),
        name.size() * sizeof(fs::path::value_type)));
    if (!directory) {
        put(out, read_file(path));
        return;
    }
    for (const auto &entry : fs::directory_iterator(path))
        save_node(out, entry.path(), relative / entry.path().filename());
}

std::string snapshot_live()
{
    std::string out(undo_magic, sizeof undo_magic);
    put(out, uint64_t(0));
    if (exists_plain(live)) {
        require(fs::is_directory(live));
        save_node(out, live, fs::path());
    }
    const uint64_t size = out.size();
    std::memcpy(&out[sizeof undo_magic], &size, sizeof size);
    return out;
}

struct Reader {
    std::string_view data;
    size_t at = undo_header;

    std::string_view take(uint64_t size)
    {
        require(size <= data.size() - at);
        const auto bytes = data.substr(at, static_cast<size_t>(size));
        at += static_cast<size_t>(size);
        return bytes;
    }
    uint64_t number()
    {
        uint64_t value;
        std::memcpy(&value, take(sizeof value).data(), sizeof value);
        return value;
    }
};

void set_times(const Node &node)
{
#ifdef _WIN32
    HANDLE handle = CreateFileW(node.path.c_str(), FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    require(handle != INVALID_HANDLE_VALUE);
    const FILETIME creation = filetime(node.times.creation);
    const FILETIME access = filetime(node.times.access);
    const FILETIME write = filetime(node.times.write);
    bool copied = SetFileTime(handle, &creation, &access, &write) != 0;
    bool closed = CloseHandle(handle) != 0;
    require(copied && closed);
#else
    fs::last_write_time(node.path, fs::file_time_type(
        fs::file_time_type::duration(static_cast<int64_t>(node.times.write))));
#endif
}

/* A name as the filesystem compares it: Windows ignores case and separator style. */
fs::path::string_type name_key(const fs::path &path)
{
    auto key = path.lexically_normal().native();
#ifdef _WIN32
    if (!key.empty()) CharUpperBuffW(key.data(), static_cast<DWORD>(key.size()));
#endif
    return key;
}

/* Parse the whole image before touching live data. A restart during removal
   or rewriting repeats this from the same image. */
void restore(std::string_view image)
{
    Reader in{image};
    std::vector<Node> nodes;
    std::set<fs::path::string_type> seen, directories;
    while (in.at != image.size()) {
        Node node{};
        const char kind = in.take(1)[0];
        require(kind == 'D' || kind == 'F');
        node.directory = kind == 'D';
        node.times.creation = in.number();
        node.times.access = in.number();
        node.times.write = in.number();
        const auto name = in.take(in.number());
        require(name.size() % sizeof(fs::path::value_type) == 0);
        fs::path::string_type native(name.size() / sizeof(fs::path::value_type), 0);
        std::memcpy(native.data(), name.data(), name.size());
        const fs::path relative(native);
        require(nodes.empty() ? relative.empty() && node.directory
                              : !relative.empty() && !relative.has_root_path());
        for (const auto &part : relative) require(part != ".." && part != ".");
        /* Each name once, after its parent directory, so rebuilding cannot fail midway. */
        const auto key = name_key(relative);
        require(nodes.empty() || directories.count(name_key(relative.parent_path())) != 0);
        require(seen.insert(key).second);
        if (node.directory) directories.insert(key);
        node.path = nodes.empty() ? live : live / relative;
        if (!node.directory) node.data = in.take(in.number());
        nodes.push_back(node);
    }
    remove_tree(live);
    if (nodes.empty()) return;
    fs::create_directories(live.parent_path());
    for (const auto &node : nodes) {
        if (node.directory) {
            require(fs::create_directory(node.path));
        } else {
            write_file(node.path, node.data);
            set_times(node);
        }
    }
    /* Creating children updates directory times, so restore them last. */
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        if (it->directory) set_times(*it);
    }
}

void recover()
{
    if (!exists_plain(undo)) return;
    require(fs::is_regular_file(undo));
    const std::string image = read_file(undo);
    if (image.size() >= sizeof undo_magic)
        require(std::memcmp(image.data(), undo_magic, sizeof undo_magic) == 0);
    uint64_t size = 0;
    if (image.size() >= undo_header) std::memcpy(&size, image.data() + sizeof undo_magic, sizeof size);
    /* Only an image shorter than its recorded size is discardable; anything
       else malformed stops startup with the undo file kept. */
    if (image.size() >= undo_header && image.size() >= size) {
        require(image.size() == size);
        restore(image);
    }
    remove_file(undo);
}

void check_journal()
{
    require(exists_plain(journal) && fs::is_directory(journal));
    check_marker(journal / "version", version);
    for (const auto &entry : fs::directory_iterator(journal)) {
        const auto name = entry.path().filename();
        if (name == "staging" || name == "pending" || name == "committed") {
            throw std::runtime_error(
                "save journal from an older build; run that build once to recover it");
        }
        require(name == "version" || name == "lock" || name == "undo");
        require(exists_plain(entry.path()) && fs::is_regular_file(entry.path()));
    }
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
        undo = journal / "undo";
        check_parents(storage);
        fs::create_directories(storage);
        check_parents(live.parent_path());
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
            write_file(journal / "version", version);
        }
        check_journal();
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
        require(!exists_plain(undo));
        write_file(undo, snapshot_live());
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
        require(exists_plain(undo));
        if (failed) {
            recover();
        } else {
            check_tree(live);
            remove_file(undo);
        }
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
