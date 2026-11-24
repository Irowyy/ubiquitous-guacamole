#include "common.h"

#include "PwfClient.h"
#include "PwfSecret.h"

#include <dpapi.h>
#include <shlobj.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>

namespace {

std::unique_ptr<PwfClient> g_client;
std::mutex                 g_mutex;
std::atomic<bool>          g_ended{false};
std::string                g_endMessage;
std::atomic<bool>          g_signedIn{false};

std::wstring session_file()
{
    PWSTR base = nullptr;
    std::wstring path;
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &base) == S_OK) {
        path = base;
        CoTaskMemFree(base);
    }
    path += L"\\RedMoon";
    saryx::make_dir(path);
    saryx::hide_folder(path);
    path += L"\\session.bin";
    return path;
}

bool store_key(const std::string& key)
{
    DATA_BLOB in{ (DWORD)key.size(),
                  reinterpret_cast<BYTE*>(const_cast<char*>(key.data())) };
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"redmoon", nullptr, nullptr, nullptr, 0, &out))
        return false;
    const bool ok = saryx::write_file(session_file(), out.pbData, out.cbData);
    LocalFree(out.pbData);
    return ok;
}

bool load_key(std::string& key)
{
    HANDLE h = CreateFileW(session_file().c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD size = GetFileSize(h, nullptr);
    std::vector<BYTE> raw(size);
    DWORD read = 0;
    const bool readOk = ReadFile(h, raw.data(), size, &read, nullptr) != FALSE;
    CloseHandle(h);
    if (!readOk || !read) return false;

    DATA_BLOB in{ read, raw.data() };
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out))
        return false;

    key.assign(reinterpret_cast<char*>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return !key.empty();
}

void erase_key() { DeleteFileW(session_file().c_str()); }

std::string days_label(const PwfResponse& r)
{
    try {
        const auto user = r.data.value("user", nlohmann::json::object());
        if (user.contains("expires_at") && user["expires_at"].is_string())
            return "expire le " + user["expires_at"].get<std::string>();
        if (user.contains("days_remaining") && user["days_remaining"].is_number())
            return std::to_string(user["days_remaining"].get<int>()) +
                   " jour(s) restant(s)";
    } catch (...) {}
    return "licence valide";
}

void report(const PwfResponse& r)
{
    const std::string code = r.ErrorCode();
    std::string msg = r.Message();
    if (msg.empty()) msg = "cle refusee (" + code + ")";

    if (code == "INVALID_KEY")       saryx::ui::bad("Cle invalide. Verifie la saisie.");
    else if (code == "EXPIRED")      saryx::ui::bad("Cle expiree.");
    else if (code == "BANNED")       saryx::ui::bad("Cle bannie.");
    else if (code == "PAUSED")       saryx::ui::bad("Cle en pause.");
    else if (code == "HWID_MISMATCH" ||
             code == "DEVICE_LIMIT") saryx::ui::warn("Cle liee a un autre PC.");
    else if (code == "MAINTENANCE")  saryx::ui::warn("Serveur en maintenance.");
    else if (code == "RATE_LIMITED") saryx::ui::warn("Trop de tentatives.");
    else                             saryx::ui::bad(msg);
}

bool do_login(const std::string& key, PwfResponse& out, bool& fatal)
{
    fatal = false;
    try {
        out = g_client->Login(key);
        return out.Success();
    } catch (const PwfHttpError& e) {
        if (e.Status() == 401) {
            saryx::ui::bad("Secret d'application refuse (401).");
            saryx::ui::info("colle le secret de 64 caracteres dans src/PwfSecret.h");
            fatal = true;
        } else {
            saryx::ui::bad("Reponse HTTP " + std::to_string(e.Status()) + ".");
        }
    } catch (const PwfNetworkError&) {
        saryx::ui::bad("Serveur injoignable (reseau, DNS, proxy, TLS).");
    } catch (const PwfSecurityError& e) {
        saryx::ui::bad(std::string("Reponse non verifiee : ") + e.what());
    } catch (const std::exception& e) {
        saryx::ui::bad(std::string("Erreur : ") + e.what());
    }
    return false;
}

bool accept(const std::string& key, const PwfResponse& r, bool persist)
{
    saryx::ui::ok("Cle acceptee - " + days_label(r));
    if (persist) store_key(key);

    std::lock_guard<std::mutex> lock(g_mutex);
    g_signedIn = true;
    g_client->SetSessionEndedCallback([](const std::string& code,
                                         const std::string& message) {
        std::lock_guard<std::mutex> guard(g_mutex);
        g_endMessage = message.empty() ? code : message;
        g_ended = true;
    });
    g_client->StartHeartbeat();
    return true;
}

} // namespace

namespace saryx {

bool license_gate()
{
    ui::frame_top();
    ui::line("", ui::Plain);
    ui::line("        ◐  L U N E   R O U G E  ◑", ui::Moon);
    ui::info("cle liee au materiel - une seule machine a la fois");
    ui::line("", ui::Plain);
    ui::frame_bottom();
    ui::line("", ui::Plain);

    if (!PwfClient::ProtocolSelfTest()) {
        ui::bad("auto-test cryptographique echoue.");
        return false;
    }

    g_client = std::make_unique<PwfClient>(PwfAppSecret);

    std::string saved;
    if (load_key(saved)) {
        ui::step("verification de la cle enregistree");
        PwfResponse r;
        bool fatal = false;
        if (do_login(saved, r, fatal) && r.Success()) {
            accept(saved, r, false);
            return true;
        }
        erase_key();
        ui::line("", ui::Plain);
    }

    for (int attempt = 1; attempt <= 5; ++attempt) {
        ui::inline_text("  cle › ", ui::Moon);
        std::string key;
        if (!std::getline(std::cin, key)) return false;

        while (!key.empty() && (key.back() == '\r' || key.back() == ' '))
            key.pop_back();
        if (key.empty()) { ui::warn("saisie vide."); continue; }

        ui::step("verification aupres du serveur");
        PwfResponse r;
        bool fatal = false;
        if (do_login(key, r, fatal)) { accept(key, r, true); return true; }
        if (fatal) return false;

        report(r);

        const std::string code = r.ErrorCode();
        if (code == "HWID_MISMATCH" || code == "DEVICE_LIMIT") {
            ui::inline_text("  deplacer la licence sur ce PC ? [o/n] › ", ui::Moon);
            std::string answer;
            std::getline(std::cin, answer);
            if (!answer.empty() && (answer[0] == 'o' || answer[0] == 'O')) {
                try {
                    const PwfResponse moved =
                        g_client->ResetHardwareId(key, "deplacement");
                    if (moved.Success()) {
                        ui::ok("licence deplacee, nouvelle tentative");
                        PwfResponse again;
                        if (do_login(key, again, fatal) && again.Success()) {
                            accept(key, again, true);
                            return true;
                        }
                        report(again);
                    } else {
                        ui::bad(moved.Message());
                    }
                } catch (const std::exception& e) {
                    ui::bad(std::string("echec du deplacement : ") + e.what());
                }
            }
        }
        ui::line("", ui::Plain);
    }

    ui::bad("trop de tentatives.");
    return false;
}

bool license_alive() { return !g_ended.load(); }

void license_logout()
{
    if (!g_client) return;
    try {
        g_client->StopHeartbeat();
        if (g_signedIn.load() && g_client->IsSignedIn()) g_client->Logout();
    } catch (...) {}
}

} // namespace saryx