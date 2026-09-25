#include "save_transaction.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

#ifdef _WIN32
static DWORD WINAPI release_delete_blocker(void *handle)
{
    Sleep(100);
    return CloseHandle(static_cast<HANDLE>(handle)) ? 0 : 1;
}
#endif

static void put(const fs::path &path, const std::string &value)
{
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << value;
    file.close();
    assert(!file.fail());
}

static std::string get(const fs::path &path)
{
    std::ifstream file(path, std::ios::binary);
    assert(file);
    return std::string(std::istreambuf_iterator<char>(file), {});
}

int main()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("recomp-save-test-" + std::to_string(stamp));
    assert(fs::create_directory(root));
    const auto live = root / ".recomp-storage" / "partition1" / "UDATA";
    const auto journal = root / ".recomp-storage" / "save-undo-v1";
    const auto payload = live / "title" / "profile" / "payload";
    const std::string root_name = root.string();

    assert(recomp_save_initialize(root_name.c_str()));
#ifdef _WIN32
    HANDLE competing = CreateFileW((journal / "lock").c_str(),
        GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(competing == INVALID_HANDLE_VALUE);
    assert(GetLastError() == ERROR_SHARING_VIOLATION);
#endif
    assert(!recomp_save_pending());
    assert(recomp_save_begin(7));
    assert(recomp_save_active(7) && recomp_save_pending());
    put(payload, "first incomplete save");
    assert(!recomp_save_end(7, false));
    assert(!fs::exists(live));
    assert(!recomp_save_pending());

    assert(recomp_save_begin(7));
    put(payload, "first save interrupted");
    assert(recomp_save_initialize(root_name.c_str()));
    assert(!fs::exists(live));

    assert(recomp_save_begin(7));
    put(payload, "first complete save");
    fs::create_directory(live / "empty");
    assert(recomp_save_end(7, true));
    assert(get(payload) == "first complete save");
    assert(!fs::exists(journal / "undo"));

    assert(recomp_save_begin(7));
    assert(!recomp_save_begin(9));
    assert(!recomp_save_end(9, true));
    assert(recomp_save_active(7));
    assert(recomp_save_begin(7));
    put(payload, "nested partial update");
    put(live / "new-file", "must disappear");
    fs::remove(live / "empty");
    recomp_save_note_failure(7);
    assert(!recomp_save_end(7, true));
    assert(recomp_save_active(7));
    assert(!recomp_save_end(7, true));
    assert(get(payload) == "first complete save");
    assert(fs::is_directory(live / "empty"));
    assert(!fs::exists(live / "new-file"));

    assert(recomp_save_begin(7));
    put(payload, "interrupted operation");
    assert(fs::is_regular_file(journal / "undo"));
#ifdef _WIN32
    assert(!recomp_save_initialize(nullptr));
    competing = CreateFileW((journal / "lock").c_str(),
        GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(competing != INVALID_HANDLE_VALUE);
    assert(!recomp_save_initialize(root_name.c_str()));
    assert(get(payload) == "interrupted operation");
    assert(fs::is_regular_file(journal / "undo"));
    assert(CloseHandle(competing) != 0);
#endif
    /* Reinitialization models a fresh process: volatile ownership is lost. */
    assert(recomp_save_initialize(root_name.c_str()));
    assert(get(payload) == "first complete save");

    assert(recomp_save_begin(7));
    /* Interruption after recovery removed live and copied only one file. */
    fs::remove_all(live);
    put(live / "recovery-partial", "not the prior generation");
    assert(recomp_save_initialize(root_name.c_str()));
    assert(get(payload) == "first complete save");
    assert(!fs::exists(live / "recovery-partial"));

#ifdef _WIN32
    assert(recomp_save_begin(7));
    put(payload, "locked interrupted save");
    HANDLE locked = CreateFileW(payload.c_str(), GENERIC_READ, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(locked != INVALID_HANDLE_VALUE);
    assert(!recomp_save_initialize(root_name.c_str()));
    assert(fs::is_regular_file(journal / "undo"));
    assert(!recomp_save_begin(7));
    assert(CloseHandle(locked) != 0);
    assert(recomp_save_initialize(root_name.c_str()));
    assert(get(payload) == "first complete save");
#endif

    assert(recomp_save_begin(7));
    put(payload, "committed generation");
    /* Deleting the undo image is the commit point. */
    fs::remove(journal / "undo");
    assert(recomp_save_initialize(root_name.c_str()));
    assert(get(payload) == "committed generation");

    /* An image cut short never reached live data and is discarded. */
    assert(recomp_save_begin(7));
    const std::string image = get(journal / "undo");
    assert(recomp_save_end(7, true));
    put(payload, "newer than the image");
    put(journal / "undo", image.substr(0, image.size() - 1));
    assert(recomp_save_initialize(root_name.c_str()));
    assert(get(payload) == "newer than the image");
    assert(!fs::exists(journal / "undo"));

    /* An image longer than its recorded size is malformed and kept. */
    put(journal / "undo", image + "x");
    assert(!recomp_save_initialize(root_name.c_str()));
    assert(get(payload) == "newer than the image");
    assert(fs::exists(journal / "undo"));
    fs::remove(journal / "undo");

    /* A directory record from the previous journal format fails closed. */
    fs::create_directory(journal / "pending");
    assert(!recomp_save_initialize(root_name.c_str()));
    fs::remove(journal / "pending");

    put(journal / "unknown", "leave this alone");
    assert(!recomp_save_initialize(root_name.c_str()));
    assert(get(journal / "unknown") == "leave this alone");
    assert(get(payload) == "newer than the image");
    fs::remove(journal / "unknown");
    assert(recomp_save_initialize(root_name.c_str()));

    const auto outside = root / "outside";
    put(outside / "untouched", "outside data");
#ifdef _WIN32
    const std::wstring command = L"cmd /c mklink /J \"" +
        (live / "linked").wstring() + L"\" \"" + outside.wstring() + L"\" >NUL";
    assert(_wsystem(command.c_str()) == 0);
    assert((GetFileAttributesW((live / "linked").c_str()) &
        FILE_ATTRIBUTE_REPARSE_POINT) != 0);
#else
    fs::create_directory_symlink(outside, live / "linked");
#endif
    assert(!recomp_save_begin(7));
    assert(get(outside / "untouched") == "outside data");
    fs::remove(live / "linked");
    assert(recomp_save_initialize(root_name.c_str()));

    assert(recomp_save_begin(0));
    put(payload, "last generation");
    assert(recomp_save_begin(0));
    assert(recomp_save_end(0, true));
    assert(recomp_save_end(0, true));
    assert(recomp_save_initialize(root_name.c_str()));
    assert(get(payload) == "last generation");
#ifdef _WIN32
    assert(recomp_save_begin(7));
    put(payload, "delete released");
    HANDLE blocker = CreateFileW((journal / "undo").c_str(),
        GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(blocker != INVALID_HANDLE_VALUE);
    assert(!DeleteFileW((journal / "undo").c_str()));
    assert(GetLastError() == ERROR_SHARING_VIOLATION);
    HANDLE release = CreateThread(nullptr, 0, release_delete_blocker,
        blocker, 0, nullptr);
    assert(release != nullptr);
    assert(recomp_save_end(7, true));
    assert(WaitForSingleObject(release, 1000) == WAIT_OBJECT_0);
    DWORD thread_status;
    assert(GetExitCodeThread(release, &thread_status) && thread_status == 0);
    assert(CloseHandle(release));
    assert(get(payload) == "delete released");
    assert(!fs::exists(journal / "undo"));

    assert(recomp_save_begin(7));
    put(payload, "delete persistently blocked");
    blocker = CreateFileW((journal / "undo").c_str(),
        GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(blocker != INVALID_HANDLE_VALUE);
    DWORD before = GetTickCount();
    assert(!recomp_save_end(7, true));
    assert(GetTickCount() - before < 2000);
    assert(fs::exists(journal / "undo"));
    assert(!recomp_save_begin(7));
    assert(get(payload) == "delete persistently blocked");
    assert(CloseHandle(blocker));
    assert(recomp_save_initialize(root_name.c_str()));
    assert(get(payload) == "delete released");
#endif
    /* Release the lifetime lock before removing this disposable fixture. */
    assert(!recomp_save_initialize(nullptr));
    fs::remove_all(root);
    std::cout << "save transaction checks passed\n";
}
