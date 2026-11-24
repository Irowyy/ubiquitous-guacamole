#include "../common.h"

namespace {

volatile LONG g_loaded = 0;

bool in_game()
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (wcsstr(path, saryx::kGameExe)) return true;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    const DWORD self = GetCurrentProcessId();

    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == self) {
                found = _wcsicmp(pe.szExeFile, saryx::kGameExe) == 0;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

void debug(const char* text)
{
    OutputDebugStringA(text);
    OutputDebugStringA("\n");
}

DWORD WINAPI watchdog(LPVOID)
{
    for (;;) {
        Sleep(500);
        const std::wstring trigger =
            saryx::reg_get_string(saryx::kMyCheatKey, L"Trigger");
        if (trigger != L"1" || g_loaded) continue;

        std::wstring target = saryx::reg_get_string(saryx::kMyCheatKey, L"DllPath");
        if (target.empty()) target = saryx::kPayloadPath;

        std::string error;
        if (saryx::manual_map_file(target, error)) {
            debug("[sideloaded] payload mappe");
            InterlockedExchange(&g_loaded, 1);
        } else {
            debug(("[sideloaded] echec : " + error).c_str());
        }
        saryx::reg_set_string(saryx::kMyCheatKey, L"Trigger", L"0");
    }
}

} // namespace

extern "C" __declspec(dllexport) void Load()
{
    debug("[sideloaded] Load() appele");
    std::wstring target = saryx::reg_get_string(saryx::kMyCheatKey, L"DllPath");
    if (target.empty()) target = saryx::kPayloadPath;

    std::string error;
    if (!saryx::manual_map_file(target, error))
        debug(("[sideloaded] " + error).c_str());
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        debug("[sideloaded] attache");
        if (in_game())
            CreateThread(nullptr, 0, watchdog, nullptr, 0, nullptr);
    }
    return TRUE;
}