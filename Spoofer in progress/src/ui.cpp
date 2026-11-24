// Theme REDMOON : croissant de lune rouge, palette sang, cadre lunaire.
#include "common.h"

#include <conio.h>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace {

HANDLE g_out = INVALID_HANDLE_VALUE;
bool   g_vt  = false;

void emit(const char* s)
{
    if (!s || !*s) return;
    DWORD written = 0;
    WriteConsoleA(g_out, s, (DWORD)std::strlen(s), &written, nullptr);
}

WORD legacy_attr(saryx::ui::Tone tone)
{
    switch (tone) {
    case saryx::ui::Accent: return FOREGROUND_RED | FOREGROUND_INTENSITY;
    case saryx::ui::Moon:   return FOREGROUND_RED | FOREGROUND_INTENSITY;
    case saryx::ui::Glow:   return FOREGROUND_RED;
    case saryx::ui::Bad:    return BACKGROUND_RED | BACKGROUND_INTENSITY |
                                   FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    case saryx::ui::Good:   return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    case saryx::ui::Warn:   return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    case saryx::ui::Dim:    return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    default:                return FOREGROUND_RED | FOREGROUND_GREEN |
                                   FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    }
}

void set_tone(saryx::ui::Tone tone)
{
    if (g_vt) {
        switch (tone) {
        case saryx::ui::Accent: emit("\x1b[1;38;5;203m"); break;
        case saryx::ui::Moon:   emit("\x1b[1;38;5;196m"); break;
        case saryx::ui::Glow:   emit("\x1b[38;5;88m");    break;
        case saryx::ui::Bad:    emit("\x1b[1;38;5;196m"); break;
        case saryx::ui::Good:   emit("\x1b[38;5;114m");   break;
        case saryx::ui::Warn:   emit("\x1b[38;5;214m");   break;
        case saryx::ui::Dim:    emit("\x1b[38;5;240m");   break;
        default:                emit("\x1b[38;5;253m");   break;
        }
    } else {
        SetConsoleTextAttribute(g_out, legacy_attr(tone));
    }
}

void reset()
{
    if (g_vt) emit("\x1b[0m");
    else SetConsoleTextAttribute(g_out, legacy_attr(saryx::ui::Plain));
}

void paint_palette()
{
    CONSOLE_SCREEN_BUFFER_INFOEX info{};
    info.cbSize = sizeof(info);
    if (!GetConsoleScreenBufferInfoEx(g_out, &info)) return;

    info.srWindow.Right  = 99;
    info.srWindow.Bottom = 36;
    info.dwSize.X        = 100;
    info.dwSize.Y        = 400;

    info.ColorTable[0]  = RGB(11, 5, 7);
    info.ColorTable[4]  = RGB(120, 22, 28);
    info.ColorTable[12] = RGB(255, 70, 70);
    info.ColorTable[7]  = RGB(198, 188, 188);
    info.ColorTable[15] = RGB(255, 240, 240);

    SetConsoleScreenBufferInfoEx(g_out, &info);
    SetConsoleWindowInfo(g_out, TRUE, &info.srWindow);
}

const char* const kMoon[] = {
    "            ▄▄████████▄▄",
    "         ▄████████████▀▀",
    "       ▄████████████▀",
    "      ▄████████████▀",
    "     ▄████████████▀",
    "    █████████████▀",
    "    ████████████▀",
    "    ████████████▀",
    "    ████████████▀",
    "    ████████████▀",
    "     ▀████████████▄",
    "      ▀████████████▄",
    "       ▀████████████▄",
    "         ▀████████████▄▄",
    "            ▀▀████████▀▀",
};
constexpr int kMoonLines = (int)(sizeof(kMoon) / sizeof(kMoon[0]));

const char* const kStars[] = {
    "                      ·        +",
    "                 ·            ·",
    "        +            ·",
    "   ·             +",
    "          ·             ·",
    "  +            ·             +",
    "       ·              ·",
    "            +            ·",
    "   ·              ·           +",
    "         +             ·",
    "  ·            ·            +",
    "        ·            +",
    "             ·            ·",
    "   +             ·            +",
    "         ·            +",
};

} // namespace

namespace saryx::ui {

bool init()
{
    if (!GetConsoleWindow()) {
        if (!AllocConsole()) return false;
        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        freopen_s(&f, "CONIN$",  "r", stdin);
    }
    g_out = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD mode = 0;
    if (GetConsoleMode(g_out, &mode) &&
        SetConsoleMode(g_out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        g_vt = true;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(kTitle);

    paint_palette();
    clear();
    return true;
}

void shutdown()
{
    reset();
    if (g_vt) emit("\x1b[0m");
}

void clear()
{
    if (g_vt) { emit("\x1b[2J\x1b[H"); return; }
    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    DWORD n = 0, cells = 0;
    if (GetConsoleScreenBufferInfo(g_out, &csbi)) {
        cells = (DWORD)csbi.dwSize.X * csbi.dwSize.Y;
        FillConsoleOutputCharacterA(g_out, ' ', cells, COORD{0, 0}, &n);
    }
    SetConsoleCursorPosition(g_out, COORD{0, 0});
}

void line(const std::string& text, Tone tone)
{
    set_tone(tone);
    emit(text.c_str());
    emit("\n");
    reset();
}

void inline_text(const std::string& text, Tone tone)
{
    set_tone(tone);
    emit(text.c_str());
    reset();
}

void rule()
{
    set_tone(Glow);
    for (int i = 0; i < 96; ++i) emit("▄");
    emit("\n");
    reset();
}

void frame_top()
{
    set_tone(Glow);
    emit(" ▄");
    for (int i = 0; i < 94; ++i) emit("▄");
    emit("▄\n");
    reset();
}

void frame_bottom()
{
    set_tone(Glow);
    emit(" ▀");
    for (int i = 0; i < 94; ++i) emit("▀");
    emit("▀\n");
    reset();
}

void banner()
{
    clear();
    frame_top();
    line("", Plain);

    for (int i = 0; i < kMoonLines; ++i) {
        inline_text("   ", Plain);
        set_tone(Glow);
        emit(kStars[i]);
        reset();
        emit("   ");
        set_tone(Moon);
        emit(kMoon[i]);
        emit("\n");
        reset();
    }

    line("", Plain);
    set_tone(Accent);
    emit("        R  E  D  M  O  O  N\n");
    reset();
    set_tone(Glow);
    emit("     ░▒▓█  lune rouge · internal  █▓▒░\n");
    reset();
    line("", Plain);
    frame_bottom();
    line("", Plain);
}

void step(const std::string& label, const std::string& detail)
{
    inline_text("  ◐ ", Glow);
    inline_text(label, Plain);
    if (!detail.empty()) inline_text("  " + detail, Dim);
    emit("\n");
}

void ok(const std::string& text)
{
    inline_text("  ● ", Moon);
    line(text, Good);
}

void warn(const std::string& text)
{
    inline_text("  ▲ ", Warn);
    line(text, Warn);
}

void bad(const std::string& text)
{
    inline_text("  ✕ ", Bad);
    line(text, Bad);
}

void info(const std::string& text) { line("    " + text, Dim); }

void phase(int index, int total)
{
    static const char* kPhases[] = { "○", "◔", "◑", "◕", "●" };
    if (total <= 0) return;
    const int step = (index * 5) / total;
    set_tone(Glow);
    emit("  ");
    emit(kPhases[step < 0 ? 0 : (step > 4 ? 4 : step)]);
    emit(" ");
    reset();
}

void progress(int percent, const std::string& label)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    const int width = 44;
    const int filled = width * percent / 100;

    emit("\r  ");
    set_tone(Glow);
    emit("▐");
    set_tone(Moon);
    for (int i = 0; i < filled; ++i) emit("█");
    set_tone(Glow);
    for (int i = filled; i < width; ++i) emit("░");
    emit("▌");
    reset();
    char pct[8];
    std::snprintf(pct, sizeof(pct), " %3d%%", percent);
    set_tone(Dim);
    emit(pct);
    reset();
    emit("  ");
    emit(label.c_str());
    emit("   ");
}

void progress_done() { emit("\n"); }

void pause()
{
    emit("\n");
    inline_text("  une touche pour fermer...", Glow);
    (void)_getch();
    emit("\n");
}

void log(const std::string& text)
{
    std::ofstream f("redmoon.log", std::ios::app);
    if (!f) return;
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char stamp[32];
    std::snprintf(stamp, sizeof(stamp), "%02d:%02d:%02d",
                  st.wHour, st.wMinute, st.wSecond);
    f << "[" << stamp << "] " << text << "\n";
}

} // namespace saryx::ui