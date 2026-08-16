#include "host_diagnostics.h"

#if defined(_MSC_VER) && defined(_DEBUG)

#include "runtime.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <crtdbg.h>
#include <dbghelp.h>
#include <rtcapi.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kGeneratedFramesReported = 8;

/* Runtime checks name only the offending variable, so the raising generated
   function is recovered from the host backtrace instead. */
void reportGeneratedFrames()
{
    void *frames[32];
    const USHORT captured =
        CaptureStackBackTrace(0u, 32u, frames, nullptr);
    const HANDLE process = GetCurrentProcess();
    unsigned char storage[sizeof(SYMBOL_INFO) + 256u] = {0};
    SYMBOL_INFO *symbol = reinterpret_cast<SYMBOL_INFO *>(storage);
    int reported = 0;

    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255u;
    for (USHORT i = 0u; i < captured && reported < kGeneratedFramesReported;
         ++i) {
        DWORD64 displacement = 0u;

        if (!SymFromAddr(
                process,
                reinterpret_cast<DWORD64>(frames[i]),
                &displacement,
                symbol)) {
            continue;
        }
        if (std::strncmp(symbol->Name, "report", 6) == 0 ||
            std::strncmp(symbol->Name, "_RTC", 4) == 0 ||
            std::strncmp(symbol->Name, "failwith", 8) == 0) {
            continue; /* this reporter's own frames */
        }
        std::fprintf(
            stderr,
            "%s%s+0x%llx",
            reported == 0 ? " frames=" : "<-",
            symbol->Name,
            static_cast<unsigned long long>(displacement));
        ++reported;
    }
    if (reported == 0) {
        std::fprintf(stderr, " frames=<unresolved>");
    }
}

const char *exceptionName(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "access violation";
    case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
    case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
    default: return "exception";
    }
}

LONG CALLBACK reportFatalException(EXCEPTION_POINTERS *info)
{
    const DWORD code = info->ExceptionRecord->ExceptionCode;

    if (code != EXCEPTION_ACCESS_VIOLATION &&
        code != EXCEPTION_STACK_OVERFLOW &&
        code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_INT_DIVIDE_BY_ZERO &&
        code != EXCEPTION_PRIV_INSTRUCTION) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    std::fprintf(
        stderr,
        "recomp host: %s (0x%08lx) at 0x%p",
        exceptionName(code),
        static_cast<unsigned long>(code),
        info->ExceptionRecord->ExceptionAddress);
    if (code == EXCEPTION_ACCESS_VIOLATION &&
        info->ExceptionRecord->NumberParameters >= 2u) {
        std::fprintf(
            stderr,
            " %s 0x%p",
            info->ExceptionRecord->ExceptionInformation[0] == 8u
                ? "executing"
                : (info->ExceptionRecord->ExceptionInformation[0] != 0u
                       ? "writing"
                       : "reading"),
            reinterpret_cast<void *>(
                info->ExceptionRecord->ExceptionInformation[1]));
    }
    reportGeneratedFrames();
    std::fprintf(
        stderr,
        " esp=0x%08lx ebp=0x%08lx dispatch=0x%08lx\n",
        static_cast<unsigned long>(recomp_runtime.registers.esp),
        static_cast<unsigned long>(recomp_runtime.registers.ebp),
        static_cast<unsigned long>(recomp_last_dispatch_address));
    std::fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

int __cdecl reportRuntimeCheck(
    int errorType,
    const wchar_t *filename,
    int line,
    const wchar_t *module,
    const wchar_t *format,
    ...)
{
    wchar_t message[512];
    va_list arguments;

    va_start(arguments, format);
    _vsnwprintf_s(message, _TRUNCATE, format, arguments);
    va_end(arguments);

    std::fprintf(
        stderr,
        "recomp host: runtime check %d in %ls:%d module=%ls: %ls",
        errorType,
        filename != nullptr ? filename : L"<unknown>",
        line,
        module != nullptr ? module : L"<unknown>",
        message);
    reportGeneratedFrames();
    std::fprintf(
        stderr,
        " esp=0x%08lx ebp=0x%08lx dispatch=0x%08lx\n",
        static_cast<unsigned long>(recomp_runtime.registers.esp),
        static_cast<unsigned long>(recomp_runtime.registers.ebp),
        static_cast<unsigned long>(recomp_last_dispatch_address));

    /* Report without breaking so one guarded run collects every site. */
    return 0;
}

} // namespace

void recomp_install_host_diagnostics(void)
{
    /* A hard crash discards buffered output, which loses exactly the lines
       that say how far execution got. */
    setvbuf(stderr, nullptr, _IONBF, 0);
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    const int reports[] = {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT};
    for (int report : reports) {
        _CrtSetReportMode(report, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(report, _CRTDBG_FILE_STDERR);
    }
    _RTC_SetErrorFuncW(reportRuntimeCheck);
    AddVectoredExceptionHandler(1u, reportFatalException);
}

#else

#include <cstdio>

void recomp_install_host_diagnostics(void)
{
    setvbuf(stderr, nullptr, _IONBF, 0);
}

#endif
