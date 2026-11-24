#include "common.h"

namespace saryx {

namespace {

bool apply_relocations(unsigned char* image, const IMAGE_NT_HEADERS64* nt,
                       ULONGLONG delta)
{
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (!dir.VirtualAddress || !dir.Size) return true;

    auto* block = reinterpret_cast<IMAGE_BASE_RELOCATION*>(image + dir.VirtualAddress);
    auto* end = reinterpret_cast<unsigned char*>(block) + dir.Size;

    while (reinterpret_cast<unsigned char*>(block) < end && block->SizeOfBlock) {
        const DWORD count =
            (block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        auto* entries = reinterpret_cast<WORD*>(block + 1);
        for (DWORD i = 0; i < count; ++i) {
            if ((entries[i] >> 12) == IMAGE_REL_BASED_DIR64) {
                auto* slot = reinterpret_cast<ULONGLONG*>(
                    image + block->VirtualAddress + (entries[i] & 0x0FFF));
                *slot += delta;
            }
        }
        block = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
            reinterpret_cast<unsigned char*>(block) + block->SizeOfBlock);
    }
    return true;
}

bool resolve_imports(unsigned char* image, const IMAGE_NT_HEADERS64* nt)
{
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return true;

    auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image + dir.VirtualAddress);
    for (; desc->Name; ++desc) {
        HMODULE mod = LoadLibraryA(reinterpret_cast<LPCSTR>(image + desc->Name));
        if (!mod) return false;

        auto* thunk = reinterpret_cast<ULONGLONG*>(
            image + (desc->OriginalFirstThunk ? desc->OriginalFirstThunk
                                              : desc->FirstThunk));
        auto* iat = reinterpret_cast<ULONGLONG*>(image + desc->FirstThunk);

        for (; *thunk; ++thunk, ++iat) {
            FARPROC fn = nullptr;
            if (*thunk & 0x8000000000000000ULL)
                fn = GetProcAddress(mod, reinterpret_cast<LPCSTR>(*thunk & 0xFFFF));
            else
                fn = GetProcAddress(mod, reinterpret_cast<LPCSTR>(image + *thunk + 2));
            if (!fn) return false;
            *iat = reinterpret_cast<ULONGLONG>(fn);
        }
    }
    return true;
}

void protect_sections(unsigned char* image, const IMAGE_NT_HEADERS64* nt)
{
    auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        if (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) continue;
        DWORD protect = PAGE_READONLY;
        if (section->Characteristics & IMAGE_SCN_MEM_WRITE) protect = PAGE_READWRITE;
        DWORD old = 0;
        VirtualProtect(image + section->VirtualAddress, section->Misc.VirtualSize,
                       protect, &old);
    }
}

} // namespace

bool manual_map_file(const std::wstring& path, std::string& error)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) { error = "fichier introuvable"; return false; }

    const DWORD size = GetFileSize(file, nullptr);
    std::vector<unsigned char> raw(size);
    DWORD read = 0;
    const bool readOk = ReadFile(file, raw.data(), size, &read, nullptr) != FALSE;
    CloseHandle(file);
    if (!readOk || read != size) { error = "lecture impossible"; return false; }

    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(raw.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) { error = "en-tete DOS invalide"; return false; }
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(raw.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) { error = "en-tete PE invalide"; return false; }

    const SIZE_T imageSize = nt->OptionalHeader.SizeOfImage;
    auto* image = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, imageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!image) { error = "VirtualAlloc refuse"; return false; }

    memcpy(image, raw.data(), nt->OptionalHeader.SizeOfHeaders);

    auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        if (!section->SizeOfRawData) continue;
        memcpy(image + section->VirtualAddress,
               raw.data() + section->PointerToRawData,
               section->SizeOfRawData);
    }

    const ULONGLONG delta =
        reinterpret_cast<ULONGLONG>(image) - nt->OptionalHeader.ImageBase;
    if (delta && !apply_relocations(image, nt, delta)) {
        error = "relocations echouees";
        VirtualFree(image, 0, MEM_RELEASE);
        return false;
    }

    if (!resolve_imports(image, nt)) {
        error = "imports non resolus";
        VirtualFree(image, 0, MEM_RELEASE);
        return false;
    }

    if (nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress) {
        using TlsCallback = void(WINAPI*)(PVOID, DWORD, PVOID);
        const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
        auto* tls = reinterpret_cast<IMAGE_TLS_DIRECTORY64*>(image + dir.VirtualAddress);
        auto* callback = reinterpret_cast<TlsCallback*>(tls->AddressOfCallBacks);
        if (callback) {
            while (*callback) {
                (*callback)(image, DLL_PROCESS_ATTACH, nullptr);
                ++callback;
            }
        }
    }

    DWORD old = 0;
    VirtualProtect(image, imageSize, PAGE_EXECUTE_READWRITE, &old);
    protect_sections(image, nt);
    VirtualProtect(image, nt->OptionalHeader.SizeOfHeaders, PAGE_READONLY, &old);

    using Entry = BOOL(WINAPI*)(HINSTANCE, DWORD, LPVOID);
    auto entry = reinterpret_cast<Entry>(image + nt->OptionalHeader.AddressOfEntryPoint);
    if (!entry(reinterpret_cast<HINSTANCE>(image), DLL_PROCESS_ATTACH, nullptr)) {
        error = "DllMain a echoue";
        return false;
    }
    return true;
}

} // namespace saryx