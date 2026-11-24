#include "common.h"

#include "PwfClient.h"
#include "PwfSecret.h"

#include <conio.h>
#include <cstring>
#include <iostream>

using namespace saryx;

namespace {

bool process_name_is_vm()
{
    static const wchar_t* kProcs[] = {
        L"vmtoolsd.exe", L"vm3dservice.exe", L"VBoxService.exe",
        L"VBoxTray.exe", L"qemu-ga.exe", L"xenservice.exe",
    };

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            for (const wchar_t* name : kProcs) {
                if (_wcsicmp(pe.szExeFile, name) == 0) { found = true; break; }
            }
        } while (!found && Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

int run_check()
{
    ui::banner();
    ui::step("auto-test cryptographique");
    if (!PwfClient::ProtocolSelfTest()) {
        ui::bad("chiffrement HS");
        ui::pause();
        return 1;
    }
    ui::ok("chiffrement OK");

    ui::step("contact du serveur avec une cle bidon");
    try {
        PwfClient probe(PwfAppSecret);
        const PwfResponse r = probe.Login("PWF-0000-0000-0000-0000");
        ui::ok("secret accepte, reponse verifiee");
        ui::info("code serveur : " + r.ErrorCode() + " (attendu INVALID_KEY)");
        ui::pause();
        return 0;
    } catch (const PwfHttpError& e) {
        if (e.Status() == 401) {
            ui::bad("secret refuse (401)");
            ui::info("regenere-le dans le panel, puis src/PwfSecret.h");
        } else {
            ui::bad("HTTP " + std::to_string(e.Status()));
        }
    } catch (const PwfSecurityError& e) {
        ui::bad(std::string("signature RSA refusee : ") + e.what());
    } catch (const std::exception& e) {
        ui::bad(e.what());
    }
    ui::pause();
    return 1;
}

int run_map()
{
    ui::banner();
    ui::step("mapping direct de C:\\redmoon.dll");
    std::string error;
    if (manual_map_file(kPayloadPath, error)) {
        ui::ok("mappe sans erreur");
        ui::info("charge dans redmoon.exe, pas dans le jeu");
    } else {
        ui::bad(error);
    }
    ui::pause();
    return 0;
}

void hold_until_exit()
{
    ui::line("", ui::Plain);
    ui::inline_text("  une touche pour fermer...", ui::Glow);

    for (;;) {
        if (_kbhit()) { (void)_getch(); return; }
        if (!license_alive()) {
            ui::line("", ui::Plain);
            ui::bad("session fermee par le serveur.");
            ui::inline_text("  une touche pour fermer...", ui::Glow);
            (void)_getch();
            return;
        }
        Sleep(80);
    }
}

} // namespace

int main(int argc, char** argv)
{
    if (!ui::init()) return 1;

    if (argc > 1 && std::strcmp(argv[1], "--check") == 0) {
        const int rc = run_check();
        ui::shutdown();
        return rc;
    }

    if (argc > 1 && std::strcmp(argv[1], "--map") == 0) {
        const int rc = run_map();
        ui::shutdown();
        return rc;
    }

    ui::banner();
    ui::step("verification de l'environnement");

    if (!elevated()) {
        ui::bad("droits administrateur requis.");
        ui::info("clic droit > executer en tant qu'administrateur");
        ui::pause();
        ui::shutdown();
        return 1;
    }
    ui::ok("administrateur");

    if (detect_vm() || process_name_is_vm()) {
        ui::bad("machine virtuelle detectee.");
        ui::info("refus sous VMware / VirtualBox / Hyper-V / QEMU");
        ui::pause();
        ui::shutdown();
        return 1;
    }
    ui::ok("machine physique");

    ui::line("", ui::Plain);
    if (!license_gate()) {
        ui::line("", ui::Plain);
        ui::bad("licence refusee.");
        ui::pause();
        ui::shutdown();
        return 1;
    }

    ui::line("", ui::Plain);
    ui::step("arret des overlays et du jeu");
    kill_overlays();
    ui::ok("processus liberes");

    ui::step("enregistrement du provider AMSI");
    install_amsi_hijack();
    ui::ok("classes COM et provider en place");

    ui::step("preparation du dossier Kaspersky");
    if (!make_dir(kKasDir)) {
        ui::bad("dossier Kaspersky impossible a creer.");
        ui::pause();
        ui::shutdown();
        return 1;
    }
    acl_takeover();
    hide_folder(kKasRoot);
    ui::ok("ACL reprises");

    ui::step("depot du loader");
    if (!install_modules(kKasDir)) { ui::pause(); ui::shutdown(); return 1; }
    ui::ok("office_antivirus.dll ecrit");

    ui::step("depot du payload");
    if (!write_payload()) { ui::pause(); ui::shutdown(); return 1; }
    if (kEmbeddedPayloadSize)
        ui::ok("C:\\redmoon.dll ecrit (" +
               std::to_string(kEmbeddedPayloadSize) + " octets)");

    ui::step("armement du trigger");
    reg_set_dword(kAmsiProvider, L"Trigger", 1);
    reg_set_string(kMyCheatKey, L"Trigger", L"1");
    ui::log("armement : triggers poses");
    ui::ok("pret");

    ui::line("", ui::Plain);
    ui::frame_top();
    ui::line("", ui::Plain);
    ui::line("        ◐  INJECTION TERMINEE  ◑", ui::Moon);
    ui::info("tu peux lancer Fortnite");
    ui::info("cheat by anakae");
    ui::line("", ui::Plain);
    ui::frame_bottom();

    hold_until_exit();
    license_logout();
    ui::shutdown();
    return 0;
}