#include "common.h"

#include <shlobj.h>
#include <cstdio>
#include <fstream>
#include <iostream>

namespace saryx {

std::string w2u(const std::wstring& w)
{
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                      nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                        s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring u2w(const std::string& s)
{
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(),
                                      nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

bool elevated()
{
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    PSID admins = nullptr;
    BOOL member = FALSE;
    if (!AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                  &admins))
        return false;
    CheckTokenMembership(nullptr, admins, &member);
    FreeSid(admins);
    return member != FALSE;
}

bool write_file(const std::wstring& path, const void* data, size_t size)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    const auto* p = static_cast<const unsigned char*>(data);
    size_t left = size;
    bool ok = true;
    while (left) {
        DWORD chunk = (DWORD)(left > 0x100000 ? 0x100000 : left);
        DWORD written = 0;
        if (!WriteFile(h, p, chunk, &written, nullptr) || written == 0) {
            ok = false;
            break;
        }
        p += written;
        left -= written;
    }
    CloseHandle(h);
    return ok;
}

bool make_dir(const std::wstring& dir)
{
    if (GetFileAttributesW(dir.c_str()) != INVALID_FILE_ATTRIBUTES) return true;
    return SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr) == ERROR_SUCCESS;
}

void hide_folder(const std::wstring& dir)
{
    if (GetFileAttributesW(dir.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    SetFileAttributesW(dir.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
}

void run_hidden(const std::wstring& cmdline)
{
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    std::wstring mutable_cmd = cmdline;
    if (CreateProcessW(nullptr, mutable_cmd.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

void taskkill(const std::wstring& image)
{
    run_hidden(L"taskkill /f /im \"" + image + L"\" /t");
}

void reg_create(const std::wstring& subkey)
{
    HKEY hk = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, subkey.c_str(), 0, nullptr, 0,
                        KEY_WRITE | KEY_WOW64_64KEY, nullptr, &hk,
                        nullptr) == ERROR_SUCCESS)
        RegCloseKey(hk);
}

void reg_set_string(const std::wstring& subkey, const std::wstring& value,
                    const std::wstring& data)
{
    HKEY hk = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, subkey.c_str(), 0, nullptr, 0,
                        KEY_WRITE | KEY_WOW64_64KEY, nullptr, &hk,
                        nullptr) != ERROR_SUCCESS)
        return;
    RegSetValueExW(hk, value.empty() ? nullptr : value.c_str(), 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(data.c_str()),
                   (DWORD)((data.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hk);
}

void reg_set_dword(const std::wstring& subkey, const std::wstring& value,
                   DWORD data)
{
    HKEY hk = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, subkey.c_str(), 0, nullptr, 0,
                        KEY_WRITE | KEY_WOW64_64KEY, nullptr, &hk,
                        nullptr) != ERROR_SUCCESS)
        return;
    RegSetValueExW(hk, value.c_str(), 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&data), sizeof(data));
    RegCloseKey(hk);
}

std::wstring reg_get_string(const std::wstring& subkey, const std::wstring& value)
{
    HKEY hk = nullptr;
    std::wstring out;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey.c_str(), 0,
                      KEY_READ | KEY_WOW64_64KEY, &hk) != ERROR_SUCCESS)
        return out;

    wchar_t buf[512]{};
    DWORD size = sizeof(buf) - sizeof(wchar_t);
    DWORD type = 0;
    if (RegQueryValueExW(hk, value.c_str(), nullptr, &type,
                         reinterpret_cast<BYTE*>(buf), &size) == ERROR_SUCCESS &&
        type == REG_SZ)
        out = buf;
    RegCloseKey(hk);
    return out;
}

void reg_delete_tree(const std::wstring& subkey)
{
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, subkey.c_str());
}

} // namespace saryx