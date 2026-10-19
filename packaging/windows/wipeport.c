/* Wipeport.exe: starts the launcher of the Windows package (bin\pythonw.exe
 * launcher\wocport_launcher.py) with the package's environment: WOC_PREBUILT, the data folder
 * in %LOCALAPPDATA%\Wipeport (settings, saves, generated files), its bin\ first on PATH.
 * Arguments are passed on (--play: the game with the saved settings, without the window), and
 * the launcher's exit status is returned. */
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE /* -municode defines it */
#endif
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <wchar.h>

static void fail(const wchar_t *what) {
    wchar_t text[1024];
    swprintf(text, 1024, L"%ls (error %lu)", what, GetLastError());
    MessageBoxW(NULL, text, L"Wipeport", MB_ICONERROR | MB_OK);
    ExitProcess(1);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR arguments, int show) {
    (void)instance; (void)previous; (void)show;
    wchar_t root[MAX_PATH], path[32768], command[32768];
    if (!GetModuleFileNameW(NULL, root, MAX_PATH)) fail(L"GetModuleFileName");
    *wcsrchr(root, L'\\') = 0;

    SetEnvironmentVariableW(L"WOC_PREBUILT", L"1");
    if (!GetEnvironmentVariableW(L"WOC_DATA_DIR", path, 32768)) {
        PWSTR local = NULL;
        if (SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &local) != S_OK) fail(L"%LOCALAPPDATA%");
        swprintf(path, 32768, L"%ls\\Wipeport", local);
        CoTaskMemFree(local);
        CreateDirectoryW(path, NULL);
        SetEnvironmentVariableW(L"WOC_DATA_DIR", path);
    }
    /* The package's DLLs before any other copy of them on PATH. */
    wchar_t old_path[32768] = L"";
    GetEnvironmentVariableW(L"PATH", old_path, 32768);
    swprintf(path, 32768, L"%ls\\bin;%ls", root, old_path);
    SetEnvironmentVariableW(L"PATH", path);
    /* No user site-packages or PYTHON* variables of another Python. */
    SetEnvironmentVariableW(L"PYTHONHOME", NULL);
    SetEnvironmentVariableW(L"PYTHONPATH", NULL);
    SetEnvironmentVariableW(L"PYTHONNOUSERSITE", L"1");

    swprintf(command, 32768, L"\"%ls\\bin\\pythonw.exe\" \"%ls\\launcher\\wocport_launcher.py\" %ls", root, root,
             arguments ? arguments : L"");
    STARTUPINFOW startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION process;
    if (!CreateProcessW(NULL, command, NULL, NULL, FALSE, 0, NULL, root, &startup, &process))
        fail(L"Cannot start the launcher (bin\\pythonw.exe)");
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD status = 0;
    GetExitCodeProcess(process.hProcess, &status);
    CloseHandle(process.hProcess);
    return (int)status;
}
