#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <cstddef>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace saryx {

constexpr wchar_t kTitle[]       = L"REDMOON";
constexpr wchar_t kGameExe[]     = L"FortniteClient-Win64-Shipping.exe";
constexpr wchar_t kGameBeExe[]   = L"FortniteClient-Win64-Shipping_BE.exe";

constexpr wchar_t kKasRoot[]     = L"C:\\Program Files (x86)\\Kaspersky Lab";
constexpr wchar_t kKasDir[]      = L"C:\\Program Files (x86)\\Kaspersky Lab\\Kaspersky Antivirus 21.3";
constexpr wchar_t kLoaderPath[]  = L"C:\\Program Files (x86)\\Kaspersky Lab\\Kaspersky Antivirus 21.3\\office_antivirus.dll";
constexpr wchar_t kLoaderName[]  = L"office_antivirus.dll";
constexpr wchar_t kPayloadPath[] = L"C:\\redmoon.dll";

constexpr wchar_t kClsidOa[]    = L"{57C3D834-F47B-46BA-9924-83C2873B0686}";
constexpr wchar_t kClsidAmsi[]  = L"{4B770032-31CA-43B1-AB0D-32C5FE2F82FA}";
constexpr wchar_t kAmsiProvider[] =
    L"SOFTWARE\\Microsoft\\AMSI\\Providers\\{4B770032-31CA-43B1-AB0D-32C5FE2F82FA}";
constexpr wchar_t kMyCheatKey[] = L"SOFTWARE\\RedMoon";

extern const unsigned char kEmbeddedLoader[];
extern const size_t        kEmbeddedLoaderSize;
extern const unsigned char kEmbeddedPayload[];
extern const size_t        kEmbeddedPayloadSize;

namespace ui {
enum Tone { Accent, Moon, Glow, Dim, Good, Warn, Bad, Plain };

bool init();
void shutdown();
void clear();
void banner();
void frame_top();
void frame_bottom();
void rule();
void line(const std::string& text, Tone tone = Plain);
void inline_text(const std::string& text, Tone tone = Plain);
void step(const std::string& label, const std::string& detail = {});
void ok(const std::string& text);
void warn(const std::string& text);
void bad(const std::string& text);
void info(const std::string& text);
void phase(int index, int total);
void progress(int percent, const std::string& label);
void progress_done();
void pause();
void log(const std::string& text);
} // namespace ui

std::string  w2u(const std::wstring& w);
std::wstring u2w(const std::string& s);
bool         elevated();
bool         write_file(const std::wstring& path, const void* data, size_t size);
bool         make_dir(const std::wstring& dir);
void         hide_folder(const std::wstring& dir);
void         run_hidden(const std::wstring& cmdline);
void         taskkill(const std::wstring& image);
void         reg_create(const std::wstring& subkey);
void         reg_set_string(const std::wstring& subkey, const std::wstring& value,
                            const std::wstring& data);
void         reg_set_dword(const std::wstring& subkey, const std::wstring& value, DWORD data);
std::wstring reg_get_string(const std::wstring& subkey, const std::wstring& value);
void         reg_delete_tree(const std::wstring& subkey);

bool detect_vm();
void enable_privilege(const wchar_t* name);
void install_amsi_hijack();
void acl_takeover();
void kill_overlays();

bool license_gate();
void license_logout();
bool license_alive();

bool install_modules(const std::wstring& kas_dir);
bool write_payload();
bool manual_map_file(const std::wstring& path, std::string& error);

} // namespace saryx