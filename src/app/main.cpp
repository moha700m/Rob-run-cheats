// main.cpp - Entry point. Handles the command line and console attachment for
// --selftest / --help, then hands over to Application.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include "Application.h"

#include <string>

namespace {

    void writeOut(const std::string& s) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h && h != INVALID_HANDLE_VALUE && GetFileType(h) != FILE_TYPE_UNKNOWN) {
            DWORD n = 0;
            WriteFile(h, s.data(), static_cast<DWORD>(s.size()), &n, nullptr);
        }
    }

    // GUI-subsystem exe: if started from a console with no redirected stdout,
    // attach to the parent console so --help / --selftest output is visible.
    void ensureConsole() {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!h || h == INVALID_HANDLE_VALUE || GetFileType(h) == FILE_TYPE_UNKNOWN) {
            if (AttachConsole(ATTACH_PARENT_PROCESS)) {
                FILE* f = nullptr;
                freopen_s(&f, "CONOUT$", "w", stdout);
                freopen_s(&f, "CONOUT$", "w", stderr);
            }
        }
    }

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();   // physical pixels: overlay geometry matches the game window exactly

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    CliOptions opts;
    std::string err;
    const bool ok = argv && parseCommandLine(argc, argv, opts, err);
    if (argv) LocalFree(argv);

    if (!ok) {
        ensureConsole();
        writeOut("error: " + err + "\n\n" + usageText());
        MessageBoxA(nullptr, (err + "\n\n" + usageText()).c_str(), "RobRun Overlay", MB_OK | MB_ICONERROR);
        return 2;
    }
    if (opts.help) {
        ensureConsole();
        writeOut(usageText());
        return 0;
    }
    if (opts.selfTest) ensureConsole();

    Application app(opts);
    return opts.selfTest ? app.runSelfTest() : app.run();
}
