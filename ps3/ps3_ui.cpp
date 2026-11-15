#include "ps3_platform.hpp"
#include "ps3_font.hpp"
#include "ps3_btn_icons.h"

#include "../src/main/frontend/config.hpp"
#include "../src/main/engine/ohiscore.hpp"
#include "../src/main/engine/omusic.hpp"
#include "../src/main/engine/audio/osoundint.hpp"
#include "../src/main/engine/audio/commands.hpp"
#include "../src/main/roms.hpp"
#include "../src/main/main.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static const uint32_t COL_BG_TOP = 0xFF060218u;
static const uint32_t COL_BG_MID = 0xFF280B46u;
static const uint32_t COL_BG_HORIZ = 0xFFD8384Cu;
static const uint32_t COL_CARD_BG = 0xE6100926u;
static const uint32_t COL_CARD_SEL = 0xF02B114Cu;
static const uint32_t COL_ACCENT_PINK = 0xFFFF2E88u;
static const uint32_t COL_ACCENT_CYAN = 0xFF38F4FFu;
static const uint32_t COL_ACCENT_GOLD = 0xFFFFD84Au;
static const uint32_t COL_TEXT_MAIN = 0xFFF4F6FFu;
static const uint32_t COL_TEXT_DIM = 0xFFA4A8C8u;
static const uint32_t COL_OK = 0xFF42F584u;
static const uint32_t COL_ERR = 0xFFFF4A64u;

static inline uint32_t lerp_color(uint32_t c0, uint32_t c1, int num, int den)
{
    if (den <= 0) return c0;
    int r0 = static_cast<int>((c0 >> 16) & 0xFFu);
    int g0 = static_cast<int>((c0 >> 8) & 0xFFu);
    int b0 = static_cast<int>(c0 & 0xFFu);
    int r1 = static_cast<int>((c1 >> 16) & 0xFFu);
    int g1 = static_cast<int>((c1 >> 8) & 0xFFu);
    int b1 = static_cast<int>(c1 & 0xFFu);
    uint32_t r = static_cast<uint32_t>(r0 + ((r1 - r0) * num) / den);
    uint32_t g = static_cast<uint32_t>(g0 + ((g1 - g0) * num) / den);
    uint32_t b = static_cast<uint32_t>(b0 + ((b1 - b0) * num) / den);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static void draw_line_clipped(int x0, int y0, int x1, int y1, uint32_t color)
{
    uint32_t *fb = ps3_video_fb();
    if (!fb) return;
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    uint32_t col = 0xFF000000u | (color & 0x00FFFFFFu);
    for (;;) {
        if (x0 >= 0 && x0 < PS3_SCREEN_W && y0 >= 0 && y0 < PS3_SCREEN_H) {
            fb[y0 * PS3_SCREEN_W + x0] = col;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_palm_silhouette(int base_x, int base_y, int scale, int flip_x)
{
    int trunk_h = 88 * scale / 100;
    int bend = 16 * scale * flip_x / 100;
    for (int i = 0; i < trunk_h; i++) {
        int px = base_x + (bend * i * i) / (trunk_h * trunk_h);
        int py = base_y - i;
        int w = 5 - (i * 3) / trunk_h;
        if (w < 2) w = 2;
        ps3_draw_fill(px - w / 2, py, w, 2, 0xFF100422u);
    }
    int tx = base_x + bend;
    int ty = base_y - trunk_h;
    static const int frond_dx[6] = { -58, -42, -16, 18, 46, 62 };
    static const int frond_dy[6] = { 12, -18, -30, -28, -14, 14 };
    for (int f = 0; f < 6; f++) {
        int ex = tx + (frond_dx[f] * scale) / 100;
        int ey = ty + (frond_dy[f] * scale) / 100;
        for (int s = 0; s < 3; s++) {
            draw_line_clipped(tx, ty + s, ex, ey + s * 3, 0xFF100422u);
            draw_line_clipped(tx, ty + s + 1, (tx + ex) / 2, (ty + ey) / 2 - 8 + s * 2, 0xFF100422u);
        }
    }
}

static uint32_t *s_bg_static = nullptr;

static void build_outrun_static_backdrop(uint32_t *fb)
{
    const int horiz_y = 372;
    ps3_draw_gradient_v(0, 0, PS3_SCREEN_W, 190, COL_BG_TOP, COL_BG_MID);
    ps3_draw_gradient_v(0, 190, PS3_SCREEN_W, horiz_y - 190, COL_BG_MID, COL_BG_HORIZ);

    const int sun_cx = PS3_SCREEN_W / 2;
    const int sun_cy = horiz_y - 96;
    const int sun_r = 132;
    for (int dy = -sun_r; dy <= sun_r; dy++) {
        int py = sun_cy + dy;
        if (py < 0 || py >= horiz_y) continue;
        if (py > sun_cy - 10) {
            int rel = py - (sun_cy - 10);
            int period = 12 + rel / 12;
            int cut = 2 + rel / 26;
            if ((rel % period) < cut) continue;
        }
        int sq = sun_r * sun_r - dy * dy;
        if (sq < 0) continue;
        int dx_max = 0;
        while ((dx_max + 1) * (dx_max + 1) <= sq) dx_max++;
        uint32_t col = lerp_color(0xFFFFEC64u, 0xFFFF1E78u, dy + sun_r, sun_r * 2);
        uint32_t *row = fb + py * PS3_SCREEN_W;
        for (int x = sun_cx - dx_max; x <= sun_cx + dx_max; x++) {
            if (x >= 0 && x < PS3_SCREEN_W) row[x] = col;
        }
    }

    for (int x = 0; x < PS3_SCREEN_W; x++) {
        int m1 = 18 + (abs(((x * 3) % 240) - 120) * 32) / 120;
        int m2 = 12 + (abs(((x * 5 + 90) % 180) - 90) * 20) / 90;
        int mh = (m1 > m2) ? m1 : m2;
        if (x > 470 && x < 810) mh = mh / 2;
        for (int py = horiz_y - mh; py < horiz_y; py++) {
            fb[py * PS3_SCREEN_W + x] = 0xFF140529u;
        }
    }

    draw_palm_silhouette(96, horiz_y, 120, 1);
    draw_palm_silhouette(166, horiz_y, 85, -1);
    draw_palm_silhouette(1180, horiz_y, 120, -1);
    draw_palm_silhouette(1108, horiz_y, 85, 1);

    ps3_draw_gradient_v(0, horiz_y, PS3_SCREEN_W, PS3_SCREEN_H - horiz_y, 0xFF14062Bu, 0xFF060212u);

    const int cx = PS3_SCREEN_W / 2;
    for (int i = -10; i <= 10; i++) {
        int top_x = cx + i * 24;
        int bot_x = cx + i * 165;
        uint32_t col = (abs(i) == 1) ? 0xFFFFB838u : ((abs(i) == 3) ? 0xFF38F4FFu : 0xFFA41E6Eu);
        draw_line_clipped(top_x, horiz_y, bot_x, PS3_SCREEN_H - 1, col);
    }

    ps3_draw_fill(0, horiz_y - 1, PS3_SCREEN_W, 3, COL_ACCENT_PINK);
}

static void draw_outrun_backdrop(uint32_t anim_tick)
{
    uint32_t *fb = ps3_video_fb();
    if (!fb) return;

    const size_t fb_bytes = static_cast<size_t>(PS3_SCREEN_W * PS3_SCREEN_H) * sizeof(uint32_t);
    if (!s_bg_static) {
        build_outrun_static_backdrop(fb);
        s_bg_static = static_cast<uint32_t *>(malloc(fb_bytes));
        if (s_bg_static) {
            memcpy(s_bg_static, fb, fb_bytes);
        }
    } else {
        memcpy(fb, s_bg_static, fb_bytes);
    }

    const int horiz_y = 372;
    uint32_t scroll = (anim_tick * 5u) % 256u;
    for (int k = 1; k <= 14; k++) {
        int z = k * 256 - static_cast<int>(scroll);
        if (z < 40) z += 14 * 256;
        int dy = (348 * 420) / (z + 180);
        int py = horiz_y + dy;
        if (py >= horiz_y + 2 && py < PS3_SCREEN_H) {
            uint32_t gcol = (py > 520) ? 0xFFE82C8Cu : 0xFF8C1C5Cu;
            uint32_t *row = fb + py * PS3_SCREEN_W;
            for (int px = 0; px < PS3_SCREEN_W; px++) row[px] = gcol;
        }
    }
}

static void draw_card(int x, int y, int w, int h, uint32_t bg, uint32_t border)
{
    ps3_draw_fill(x, y, w, h, bg);
    ps3_draw_rect(x, y, w, h, border, 2);
}

static void draw_car_preview(int x, int y, int w, int h, int pal_idx)
{
    static const uint32_t pal_body[5] = {
        0xFFE81C24u,
        0xFF2874FFu,
        0xFFF6C818u,
        0xFF22B854u,
        0xFF28DCE8u
    };
    static const char *pal_names[5] = {
        "CLASSIC RED",
        "AZURE BLUE",
        "SOLAR YELLOW",
        "EMERALD GREEN",
        "CYBER CYAN"
    };
    int idx = (pal_idx >= 0 && pal_idx < 5) ? pal_idx : 0;
    uint32_t body = pal_body[idx];

    draw_card(x, y, w, h, 0xEC0E0822u, COL_ACCENT_CYAN);
    ps3_draw_text_centered(x + w / 2, y + 12, "FERRARI COLOR PREVIEW", COL_TEXT_DIM, 0);

    int cx = x + w / 2;
    int cy = y + h / 2 + 12;

    ps3_draw_blend(cx - 105, cy + 32, 210, 14, 0xAA020208u);
    ps3_draw_fill(cx - 96, cy + 4, 192, 28, body);
    ps3_draw_fill(cx - 72, cy - 22, 144, 28, body);
    ps3_draw_fill(cx - 60, cy - 18, 120, 14, 0xFF1A2A48u);
    ps3_draw_fill(cx - 88, cy + 12, 176, 6, 0xFF201026u);
    ps3_draw_fill(cx - 86, cy + 12, 34, 10, 0xFFFF3038u);
    ps3_draw_fill(cx + 52, cy + 12, 34, 10, 0xFFFF3038u);
    ps3_draw_fill(cx - 82, cy + 28, 36, 12, 0xFF181820u);
    ps3_draw_fill(cx + 46, cy + 28, 36, 12, 0xFF181820u);
    ps3_draw_fill(cx - 28, cy + 20, 56, 10, 0xFFE6ECFFu);
    ps3_draw_rect(cx - 96, cy + 4, 192, 28, 0x80FFFFFFu, 1);

    ps3_draw_text_centered(x + w / 2, y + h - 32, pal_names[idx], COL_ACCENT_GOLD, 1);
}

static void draw_footer_btn(int *x, int y, int icon, const char *label)
{
    ps3_draw_btn_icon(icon, *x, y);
    *x += 30;
    ps3_draw_text(*x, y + 2, label, COL_TEXT_MAIN, 0);
    *x += ps3_measure_text(label, 0) + 22;
}

void ps3_ui_init(void)
{
    ps3_font_init();
}

void ps3_ui_shutdown(void)
{
    if (s_bg_static) {
        free(s_bg_static);
        s_bg_static = nullptr;
    }
    ps3_font_shutdown();
}

static void draw_busy_overlay(const char *title, const char *sub)
{
    ps3_draw_fill(300, 304, 680, 112, 0xFF140A2Bu);
    ps3_draw_rect(300, 304, 680, 112, COL_ACCENT_CYAN, 2);
    ps3_draw_text_centered(PS3_SCREEN_W / 2, 330, title, COL_ACCENT_GOLD, 1);
    if (sub && sub[0]) {
        ps3_draw_text_centered(PS3_SCREEN_W / 2, 368, sub, COL_TEXT_MAIN, 0);
    }
    ps3_video_present();
}

void ps3_ui_show_splash(void)
{
    if (!g_ps3_cfg.splash_enabled) return;

    const int total_frames = 180;
    for (int f = 0; f < total_frames && !g_app_quit; f++) {
        uint32_t t0 = ps3_ticks_ms();
        ps3_sysutil_check();
        ps3_input_poll();
        uint32_t pressed = ps3_input_pressed();
        if (f > 20 && (pressed & (PS3_ACT_CROSS | PS3_ACT_START | PS3_ACT_CIRCLE))) {
            break;
        }

        draw_outrun_backdrop(static_cast<uint32_t>(f * 2));

        draw_card(160, 124, 960, 444, 0xFF0E0622u, COL_ACCENT_PINK);
        ps3_draw_rect(170, 134, 940, 424, 0xFF28749Eu, 2);

        ps3_draw_text_centered(PS3_SCREEN_W / 2, 168, "OUTRUN ARCADE ENGINE", COL_ACCENT_CYAN, 1);
        ps3_draw_text_centered(PS3_SCREEN_W / 2, 212, "CANNONBALL", COL_ACCENT_GOLD, 3);
        ps3_draw_text_centered(PS3_SCREEN_W / 2, 286, "SONY PLAYSTATION 3 EDITION", COL_ACCENT_PINK, 2);

        ps3_draw_fill(260, 344, 760, 2, COL_ACCENT_CYAN);
        ps3_draw_text_centered(PS3_SCREEN_W / 2, 368, "PS3 PORT BY THEHEROGAC", COL_OK, 2);
        ps3_draw_text_centered(PS3_SCREEN_W / 2, 418, "ENGINE BY CHRIS WHITE (DJYT)  |  CONSOLE ADAPTATIONS: RSN8887", COL_TEXT_MAIN, 0);
        ps3_draw_text_centered(PS3_SCREEN_W / 2, 448, "48KHZ CELL AUDIO  •  SAVE PROFILES  •  720P SYNTHWAVE UI", COL_TEXT_DIM, 0);
        ps3_draw_text_centered(PS3_SCREEN_W / 2, 492, "NOTE: COPYRIGHTED OUTRUN ROMS ARE USER-SUPPLIED", COL_ACCENT_GOLD, 0);

        int bar_w = 560;
        int fill_w = (bar_w * (f + 1)) / total_frames;
        ps3_draw_fill(360, 530, bar_w, 8, 0xFF1D1038u);
        ps3_draw_fill(360, 530, fill_w, 8, COL_ACCENT_CYAN);

        ps3_draw_text_centered(PS3_SCREEN_W / 2, 610, "PRESS [X] OR [START] TO CONTINUE", COL_TEXT_DIM, 1);
        ps3_video_present();
        if (f == 0) {
            ps3_boot_mark("splash_first_frame");
        }
        uint32_t dt = ps3_ticks_ms() - t0;
        if (dt < 16u) ps3_sleep_ms(16u - dt);
    }
}

int ps3_ui_missing_data_dialog(ps3_rom_report_t *rep)
{
    int scroll = 0;
    uint32_t anim = 0;
    char status_banner[256] = "";
    uint32_t status_color = COL_ACCENT_CYAN;

    const int visible_rows = 10;
    int max_scroll = rep->count - visible_rows;
    if (max_scroll < 0) max_scroll = 0;

    while (!g_app_quit) {
        uint32_t t0 = ps3_ticks_ms();
        ps3_sysutil_check();
        ps3_input_poll();
        uint32_t pressed = ps3_input_pressed();
        uint32_t held = ps3_input_buttons();

        if (pressed & PS3_ACT_START) {
            return 0;
        }
        if ((pressed & PS3_ACT_UP) || ((held & PS3_ACT_UP) && (anim % 8 == 0))) {
            if (scroll > 0) scroll--;
        }
        if ((pressed & PS3_ACT_DOWN) || ((held & PS3_ACT_DOWN) && (anim % 8 == 0))) {
            if (scroll < max_scroll) scroll++;
        }
        if (pressed & PS3_ACT_L1) {
            scroll -= visible_rows;
            if (scroll < 0) scroll = 0;
        }
        if (pressed & PS3_ACT_R1) {
            scroll += visible_rows;
            if (scroll > max_scroll) scroll = max_scroll;
        }

        if (pressed & PS3_ACT_SQUARE) {
            draw_busy_overlay("COPYING ROMS FROM USB TO HDD...", "Checking /dev_usb000 and /dev_usb001");
            char msg[256];
            ps3_roms_import_from_usb(msg, sizeof(msg));
            ps3_roms_scan(rep);
            snprintf(status_banner, sizeof(status_banner), "%s", msg);
            status_color = rep->all_required_ok ? COL_OK : COL_ACCENT_GOLD;
            if (rep->all_required_ok) {
                ps3_video_toast("All Required OutRun ROMs Ready!", 120);
                return 1;
            }
        }

        if (pressed & PS3_ACT_CROSS) {
            draw_busy_overlay("RESCANNING STORAGE FOR OUTRUN ROMS...", "Checking HDD and connected USB folders");
            ps3_roms_scan(rep);
            if (rep->all_required_ok) {
                ps3_video_toast("ROM Verification Passed!", 120);
                return 1;
            }
            snprintf(status_banner, sizeof(status_banner),
                     "Rescan complete: %d/%d Rev B ROMs found, %d/%d Engine Res found.",
                     rep->revb_found, PS3_ROM_REVB_COUNT, rep->res_found, PS3_RES_BIN_COUNT);
            status_color = COL_ERR;
        }

        draw_outrun_backdrop(anim++);

        draw_card(54, 34, 1172, 652, 0xFF0D071Eu, COL_ERR);
        ps3_draw_fill(56, 36, 1168, 56, 0xFF420D22u);
        ps3_draw_text_centered(PS3_SCREEN_W / 2, 46, "GAME DATA MISSING - OUTRUN REVISION B ROMS REQUIRED", COL_ACCENT_GOLD, 2);

        draw_card(78, 106, 1124, 134, 0xFF160C32u, COL_ACCENT_PINK);
        ps3_draw_text(98, 116, "OutRun arcade ROMs are copyrighted and are NOT included in this package.", COL_TEXT_MAIN, 1);
        ps3_draw_text(98, 146, "Place your own OutRun (Rev. B) MAME files or 'outrun.zip' into one of these paths:", COL_TEXT_DIM, 0);
        ps3_draw_text(98, 170, "HDD: /dev_hdd0/cannonball/roms/   |   DATA: /dev_hdd0/data/cannonball/roms/", COL_ACCENT_CYAN, 0);
        ps3_draw_text(98, 194, "USB: /dev_usb000/cannonball/roms/ or /dev_usb000/outrun.zip (press SQUARE to import)", COL_ACCENT_CYAN, 0);

        char stats[160];
        snprintf(stats, sizeof(stats), "STATUS:  Rev B ROMs %d/%d   |   Engine Data %d/%d   |   Optional JAP %d/%d   |   Fixed PCM %d/1",
                 rep->revb_found, PS3_ROM_REVB_COUNT, rep->res_found, PS3_RES_BIN_COUNT, rep->jap_found, PS3_ROM_JAP_COUNT, rep->pcm_found);
        ps3_draw_text(98, 218, stats, rep->all_required_ok ? COL_OK : COL_ACCENT_GOLD, 0);

        draw_card(78, 252, 1124, 336, 0xFF0B061Au, COL_ACCENT_CYAN);
        ps3_draw_fill(80, 254, 1120, 30, 0xFF1D123Cu);
        ps3_draw_text(98, 261, "FILE NAME", COL_ACCENT_GOLD, 0);
        ps3_draw_text(310, 261, "DESCRIPTION", COL_ACCENT_GOLD, 0);
        ps3_draw_text(610, 261, "CRC32", COL_ACCENT_GOLD, 0);
        ps3_draw_text(745, 261, "STATUS / DETECTED SOURCE", COL_ACCENT_GOLD, 0);

        for (int r = 0; r < visible_rows; r++) {
            int idx = scroll + r;
            if (idx >= rep->count) break;
            const ps3_rom_entry_status_t& item = rep->items[idx];
            int ry = 288 + r * 29;
            ps3_draw_fill(84, ry, 1112, 27, (r & 1) ? 0xFF150E30u : 0xFF0C071Cu);

            ps3_draw_text(98, ry + 5, item.filename, item.found ? COL_TEXT_MAIN : COL_ERR, 0);
            ps3_draw_text(310, ry + 5, item.label, COL_TEXT_DIM, 0);

            char crcbuf[16];
            if (item.expected_crc != 0)
                snprintf(crcbuf, sizeof(crcbuf), "%08X", static_cast<unsigned int>(item.expected_crc));
            else
                snprintf(crcbuf, sizeof(crcbuf), "RES DATA");
            ps3_draw_text(610, ry + 5, crcbuf, COL_TEXT_DIM, 0);

            if (item.found) {
                char okmsg[96];
                snprintf(okmsg, sizeof(okmsg), "OK (%s)", item.source_desc);
                ps3_draw_text(745, ry + 5, okmsg, COL_OK, 0);
            } else {
                const char *tag = (item.group == 0 || item.group == 3) ? "MISSING (REQUIRED)" : "NOT PRESENT (OPTIONAL)";
                ps3_draw_text(745, ry + 5, tag, (item.group == 0 || item.group == 3) ? COL_ERR : COL_TEXT_DIM, 0);
            }
        }

        if (status_banner[0]) {
            ps3_draw_text_centered(PS3_SCREEN_W / 2, 598, status_banner, status_color, 0);
        } else {
            ps3_draw_text_centered(PS3_SCREEN_W / 2, 598, "Use D-Pad UP/DOWN or L1/R1 to scroll the complete ROM verification table.", COL_TEXT_DIM, 0);
        }

        ps3_draw_fill(60, 626, 1160, 48, 0xFF130B2Au);
        int fx = 96;
        draw_footer_btn(&fx, 638, BTN_ICON_CROSS, "Rescan Storage");
        draw_footer_btn(&fx, 638, BTN_ICON_SQUARE, "Copy ROMs from USB");
        draw_footer_btn(&fx, 638, BTN_ICON_DPAD, "Scroll List");
        draw_footer_btn(&fx, 638, BTN_ICON_START, "Exit to XMB");

        ps3_video_present();
        uint32_t dt = ps3_ticks_ms() - t0;
        if (dt < 16u) ps3_sleep_ms(16u - dt);
    }
    return 0;
}

static int jukebox_index_to_cmd(int idx)
{
    if (idx == 0) return sound::MUSIC_MAGICAL;
    if (idx == 1) return sound::MUSIC_BREEZE;
    if (idx == 2) return sound::MUSIC_SPLASH;
    if (idx == 3) return sound::MUSIC_LASTWAVE;
    return sound::MUSIC_MAGICAL;
}

static const char *jukebox_name(int idx)
{
    if (idx == 0) return "Magical Sound Shower";
    if (idx == 1) return "Passing Breeze";
    if (idx == 2) return "Splash Wave";
    if (idx == 3) return "Last Wave";
    return "Custom Track";
}

static void draw_row_option(int x, int y, int w, int selected, const char *label, const char *value)
{
    if (selected) {
        ps3_draw_fill(x, y, w, 40, COL_CARD_SEL);
        ps3_draw_rect(x, y, w, 40, COL_ACCENT_CYAN, 2);
        ps3_draw_fill(x + 4, y + 6, 6, 28, COL_ACCENT_PINK);
    } else {
        ps3_draw_fill(x, y, w, 40, 0xFF150E2Eu);
    }
    ps3_draw_text(x + 22, y + 9, label, selected ? COL_ACCENT_GOLD : COL_TEXT_MAIN, 1);
    if (value && value[0]) {
        int vw = ps3_measure_text(value, 1);
        ps3_draw_text(x + w - vw - 22, y + 9, value, selected ? COL_ACCENT_CYAN : COL_TEXT_DIM, 1);
    }
}

static void format_score_row(int rank, const score_entry& e, char *out, int out_sz)
{
    char n0 = (e.initial1 >= 0x20 && e.initial1 < 0x7F) ? static_cast<char>(e.initial1) : '.';
    char n1 = (e.initial2 >= 0x20 && e.initial2 < 0x7F) ? static_cast<char>(e.initial2) : '.';
    char n2 = (e.initial3 >= 0x20 && e.initial3 < 0x7F) ? static_cast<char>(e.initial3) : '.';
    snprintf(out, out_sz, "#%d  %c%c%c   %08X", rank + 1, n0, n1, n2, static_cast<unsigned int>(e.score));
}

int ps3_ui_main_menu(ps3_rom_report_t *rep)
{
    static const char *tabs[7] = {
        "PLAY",
        "SAVES",
        "VIDEO",
        "AUDIO",
        "ENGINE",
        "CONTROLS",
        "ABOUT"
    };
    static const int row_counts[7] = { 8, 9, 9, 7, 13, 6, 4 };

    int cur_tab = 0;
    int cur_row[7] = { 0, 0, 0, 0, 0, 0, 0 };
    int jukebox_idx = 0;
    int jukebox_playing = 0;
    int clear_confirm = 0;
    uint32_t anim = 0;
    char banner[160] = "";
    int banner_timer = 0;

    ps3_slot_info_t slot_cache[3];
    for (int sl = 1; sl <= 3; sl++) {
        ps3_saves_get_slot_info(sl, &slot_cache[sl - 1]);
    }

    if (rep->all_required_ok && roms.z80.loaded) {
        osoundint.has_booted = true;
        osoundint.init();
    }

    while (!g_app_quit) {
        uint32_t t0 = ps3_ticks_ms();
        ps3_sysutil_check();
        ps3_input_poll();
        uint32_t pressed = ps3_input_pressed();

        if (jukebox_playing && rep->all_required_ok && roms.z80.loaded && config.sound.enabled) {
            for (int t = 0; t < 4 && ps3_audio_buffered_frames() < 2400u; t++) {
                ps3_audio_tick_frame();
            }
        }

        if (pressed & PS3_ACT_L1) {
            cur_tab = (cur_tab + 6) % 7;
            clear_confirm = 0;
        }
        if (pressed & PS3_ACT_R1) {
            cur_tab = (cur_tab + 1) % 7;
            clear_confirm = 0;
        }
        if (pressed & PS3_ACT_UP) {
            cur_row[cur_tab] = (cur_row[cur_tab] + row_counts[cur_tab] - 1) % row_counts[cur_tab];
            clear_confirm = 0;
        }
        if (pressed & PS3_ACT_DOWN) {
            cur_row[cur_tab] = (cur_row[cur_tab] + 1) % row_counts[cur_tab];
            clear_confirm = 0;
        }

        int r = cur_row[cur_tab];
        int dir = 0;
        if (pressed & PS3_ACT_LEFT) dir = -1;
        if (pressed & PS3_ACT_RIGHT) dir = 1;
        bool act = (pressed & PS3_ACT_CROSS) != 0;

        if (cur_tab == 0) {
            if (r == 0 && act) {
                ps3_saves_sync_all(true);
                return PS3_LAUNCH_ENHANCED;
            } else if (r == 1 && act) {
                ps3_saves_sync_all(true);
                return PS3_LAUNCH_ORIGINAL;
            } else if (r == 2 && act) {
                ps3_saves_sync_all(true);
                return PS3_LAUNCH_CONT;
            } else if (r == 3 && (dir != 0 || act)) {
                config.cont_traffic = (config.cont_traffic + (dir < 0 ? 5 : 1)) % 6;
            } else if (r == 4 && act) {
                ps3_saves_sync_all(true);
                return PS3_LAUNCH_TTRIAL;
            } else if (r == 5 && (dir != 0 || act)) {
                config.ttrial.laps += (dir < 0 ? -1 : 1);
                if (config.ttrial.laps < 1) config.ttrial.laps = 5;
                if (config.ttrial.laps > 5) config.ttrial.laps = 1;
            } else if (r == 6 && act) {
                ps3_saves_sync_all(true);
                return PS3_LAUNCH_CLASSIC_MENU;
            } else if (r == 7 && act) {
                ps3_saves_sync_all(true);
                return PS3_LAUNCH_EXIT;
            }
        } else if (cur_tab == 1) {
            if (r == 0 && act) {
                bool ok = ps3_saves_sync_all(true);
                snprintf(banner, sizeof(banner), ok ? "All Config & High Scores Saved!" : "Error Writing Save Data!");
                banner_timer = 120;
            } else if (r == 1 && (dir != 0 || act)) {
                g_ps3_cfg.active_slot = (g_ps3_cfg.active_slot + (dir < 0 ? 3 : 1)) % 4;
                ps3_config_save();
            } else if (r >= 2 && r <= 4 && act) {
                int sl = r - 1;
                ps3_saves_sync_all(false);
                bool ok = ps3_saves_write_slot(sl);
                ps3_saves_get_slot_info(sl, &slot_cache[sl - 1]);
                snprintf(banner, sizeof(banner), ok ? "Saved Profile to Slot %d!" : "Failed Saving Slot %d!", sl);
                banner_timer = 120;
            } else if (r >= 5 && r <= 7 && act) {
                int sl = r - 4;
                bool ok = ps3_saves_load_slot(sl);
                ps3_saves_get_slot_info(sl, &slot_cache[sl - 1]);
                snprintf(banner, sizeof(banner), ok ? "Loaded Profile Slot %d!" : "Slot %d Is Empty!", sl);
                banner_timer = 120;
            } else if (r == 8 && act) {
                if (!clear_confirm) {
                    clear_confirm = 1;
                    snprintf(banner, sizeof(banner), "Press [X] again to wipe saved High Scores!");
                    banner_timer = 120;
                } else {
                    clear_confirm = 0;
                    bool ok = config.clear_scores();
                    snprintf(banner, sizeof(banner), ok ? "Saved High Scores Cleared!" : "No Score Files Found.");
                    banner_timer = 120;
                }
            }
        } else if (cur_tab == 2) {
            if (r == 0 && (dir != 0 || act)) {
                g_ps3_cfg.aspect = (g_ps3_cfg.aspect + (dir < 0 ? PS3_ASPECT_COUNT - 1 : 1)) % PS3_ASPECT_COUNT;
            } else if (r == 1 && (dir != 0 || act)) {
                config.video.widescreen ^= 1;
            } else if (r == 2 && (dir != 0 || act)) {
                config.video.hires ^= 1;
            } else if (r == 3 && (dir != 0 || act)) {
                config.video.fps = (config.video.fps + (dir < 0 ? 2 : 1)) % 3;
                config.set_fps(config.video.fps);
            } else if (r == 4 && (dir != 0 || act)) {
                g_ps3_cfg.scanlines += (dir < 0 ? -10 : 10);
                if (g_ps3_cfg.scanlines > 100) g_ps3_cfg.scanlines = 0;
                if (g_ps3_cfg.scanlines < 0) g_ps3_cfg.scanlines = 100;
                config.video.scanlines = g_ps3_cfg.scanlines;
            } else if (r == 5 && (dir != 0 || act)) {
                g_ps3_cfg.linear_filter ^= 1;
                config.video.filtering = g_ps3_cfg.linear_filter;
                ps3_video_apply_filter();
            } else if (r == 6 && (dir != 0 || act)) {
                g_ps3_cfg.overscan_pct += (dir < 0 ? -1 : 1);
                if (g_ps3_cfg.overscan_pct > 8) g_ps3_cfg.overscan_pct = 0;
                if (g_ps3_cfg.overscan_pct < 0) g_ps3_cfg.overscan_pct = 8;
            } else if (r == 7 && (dir != 0 || act)) {
                config.video.shadow ^= 1;
            } else if (r == 8 && (dir != 0 || act)) {
                g_ps3_cfg.show_fps ^= 1;
                config.video.fps_count = g_ps3_cfg.show_fps;
            }
        } else if (cur_tab == 3) {
            if (r == 0 && (dir != 0 || act)) {
                config.sound.enabled ^= 1;
                if (!config.sound.enabled) {
                    jukebox_playing = 0;
                    osoundint.queue_sound(sound::FM_RESET);
                    cannonball::audio.clear_wav();
                    cannonball::audio.stop_audio();
                }
            } else if (r == 1 && (dir != 0 || act)) {
                g_ps3_cfg.volume_pct += (dir < 0 ? -10 : 10);
                if (g_ps3_cfg.volume_pct > 100) g_ps3_cfg.volume_pct = 0;
                if (g_ps3_cfg.volume_pct < 0) g_ps3_cfg.volume_pct = 100;
            } else if (r == 2 && (dir != 0 || act)) {
                config.sound.advertise ^= 1;
            } else if (r == 3 && (dir != 0 || act)) {
                config.sound.preview ^= 1;
            } else if (r == 4 && (dir != 0 || act)) {
                if (roms.load_pcm_rom(!config.sound.fix_samples) == 0) {
                    config.sound.fix_samples ^= 1;
                } else {
                    snprintf(banner, sizeof(banner), "Fixed PCM ROM (opr-10188.71f) Not Found!");
                    banner_timer = 120;
                }
            } else if (r == 5 && (dir != 0 || act)) {
                int total_tracks = 4 + static_cast<int>(config.sound.music.size());
                jukebox_idx = (jukebox_idx + (dir < 0 ? total_tracks - 1 : 1)) % total_tracks;
            } else if (r == 6 && act) {
                if (jukebox_playing) {
                    jukebox_playing = 0;
                    osoundint.queue_sound(sound::FM_RESET);
                    cannonball::audio.clear_wav();
                    cannonball::audio.stop_audio();
                } else if (rep->all_required_ok && roms.z80.loaded && config.sound.enabled) {
                    ps3_audio_clear_ring();
                    osoundint.init();
                    cannonball::audio.init();
                    cannonball::audio.start_audio();
                    cannonball::audio.clear_wav();
                    if (jukebox_idx < 4) {
                        osoundint.queue_sound(jukebox_index_to_cmd(jukebox_idx));
                    } else {
                        int cidx = jukebox_idx - 4;
                        if (cidx >= 0 && cidx < static_cast<int>(config.sound.music.size())) {
                            omusic.play_music(cidx);
                        }
                    }
                    jukebox_playing = 1;
                }
            }
        } else if (cur_tab == 4) {
            if (r == 0 && (dir != 0 || act)) {
                if (!config.engine.jap && rep->jap_found < PS3_ROM_JAP_COUNT) {
                    snprintf(banner, sizeof(banner), "Japanese Track ROMs Not Found (%d/8)", rep->jap_found);
                    banner_timer = 120;
                } else {
                    config.engine.jap ^= 1;
                }
            } else if (r == 1 && (dir != 0 || act)) {
                config.engine.car_pal = (config.engine.car_pal + (dir < 0 ? 4 : 1)) % 5;
            } else if (r == 2 && (dir != 0 || act)) {
                config.inc_time();
            } else if (r == 3 && (dir != 0 || act)) {
                config.inc_traffic();
            } else if (r == 4 && (dir != 0 || act)) {
                config.engine.freeplay = !config.engine.freeplay;
            } else if (r == 5 && (dir != 0 || act)) {
                config.engine.fix_bugs = !config.engine.fix_bugs;
                config.engine.fix_bugs_backup = config.engine.fix_bugs;
            } else if (r == 6 && (dir != 0 || act)) {
                config.engine.level_objects ^= 1;
            } else if (r == 7 && (dir != 0 || act)) {
                config.engine.new_attract ^= 1;
            } else if (r == 8 && (dir != 0 || act)) {
                config.engine.fix_timer = !config.engine.fix_timer;
            } else if (r == 9 && (dir != 0 || act)) {
                config.engine.prototype ^= 1;
            } else if (r == 10 && (dir != 0 || act)) {
                config.engine.grippy_tyres = !config.engine.grippy_tyres;
            } else if (r == 11 && (dir != 0 || act)) {
                config.engine.offroad = !config.engine.offroad;
            } else if (r == 12 && (dir != 0 || act)) {
                config.engine.turbo = !config.engine.turbo;
                config.engine.bumper = config.engine.turbo;
            }
        } else if (cur_tab == 5) {
            if (r == 0 && (dir != 0 || act)) {
                g_ps3_cfg.control_preset = (g_ps3_cfg.control_preset + (dir < 0 ? 2 : 1)) % 3;
            } else if (r == 1 && (dir != 0 || act)) {
                config.controls.analog = (config.controls.analog + (dir < 0 ? 2 : 1)) % 3;
            } else if (r == 2 && (dir != 0 || act)) {
                config.controls.gear = (config.controls.gear + (dir < 0 ? 3 : 1)) % 4;
            } else if (r == 3 && (dir != 0 || act)) {
                config.controls.steer_speed += (dir < 0 ? -1 : 1);
                if (config.controls.steer_speed > 9) config.controls.steer_speed = 1;
                if (config.controls.steer_speed < 1) config.controls.steer_speed = 9;
            } else if (r == 4 && (dir != 0 || act)) {
                config.controls.pedal_speed += (dir < 0 ? -1 : 1);
                if (config.controls.pedal_speed > 9) config.controls.pedal_speed = 1;
                if (config.controls.pedal_speed < 1) config.controls.pedal_speed = 9;
            } else if (r == 5 && (dir != 0 || act)) {
                g_ps3_cfg.rumble_pct += (dir < 0 ? -25 : 25);
                if (g_ps3_cfg.rumble_pct > 100) g_ps3_cfg.rumble_pct = 0;
                if (g_ps3_cfg.rumble_pct < 0) g_ps3_cfg.rumble_pct = 100;
                config.controls.rumble = static_cast<float>(g_ps3_cfg.rumble_pct) / 100.0f;
                if (g_ps3_cfg.rumble_pct > 0) ps3_input_set_rumble(0.75f);
            }
        } else if (cur_tab == 6) {
            if (r == 0 && act) {
                draw_busy_overlay("RESCANNING OUTRUN ROMS...", "Checking HDD and USB ROM directories");
                ps3_roms_scan(rep);
                snprintf(banner, sizeof(banner), "Rescan: %d/%d RevB | %d/8 JAP | %d/1 PCM | %d/2 Res",
                         rep->revb_found, PS3_ROM_REVB_COUNT, rep->jap_found, rep->pcm_found, rep->res_found);
                banner_timer = 140;
            } else if (r == 1 && act) {
                draw_busy_overlay("IMPORTING ROMS FROM USB...", "Scanning /dev_usb000 and /dev_usb001");
                ps3_roms_import_from_usb(banner, sizeof(banner));
                ps3_roms_scan(rep);
                banner_timer = 150;
            } else if (r == 2 && act) {
                char spath[512];
                bool ok = ps3_video_save_screenshot(spath, sizeof(spath));
                snprintf(banner, sizeof(banner), ok ? "Screenshot saved to HDD!" : "Screenshot failed!");
                banner_timer = 120;
            } else if (r == 3 && (dir != 0 || act)) {
                g_ps3_cfg.splash_enabled ^= 1;
                ps3_config_save();
            }
        }

        if (pressed & PS3_ACT_START) {
            ps3_saves_sync_all(true);
            return PS3_LAUNCH_ENHANCED;
        }
        if (pressed & PS3_ACT_SQUARE) {
            ps3_saves_sync_all(true);
            snprintf(banner, sizeof(banner), "Settings & High Scores Saved!");
            banner_timer = 100;
        }

        draw_outrun_backdrop(anim++);

        ps3_draw_fill(0, 0, PS3_SCREEN_W, 74, 0xEC0B0520u);
        ps3_draw_fill(0, 74, PS3_SCREEN_W, 3, COL_ACCENT_PINK);
        ps3_draw_text(36, 14, "CANNONBALL PS3", COL_ACCENT_GOLD, 2);
        ps3_draw_text(38, 46, "ENHANCED OUTRUN ENGINE  •  PORT BY THEHEROGAC", COL_TEXT_DIM, 0);

        char slot_lbl[64];
        if (g_ps3_cfg.active_slot > 0)
            snprintf(slot_lbl, sizeof(slot_lbl), "PROFILE: SLOT %d", g_ps3_cfg.active_slot);
        else
            snprintf(slot_lbl, sizeof(slot_lbl), "PROFILE: AUTO");
        ps3_draw_text(980, 22, slot_lbl, COL_ACCENT_CYAN, 1);

        const int tab_y = 84;
        const int tab_w = 172;
        const int tab_gap = 4;
        const int tab_x0 = (PS3_SCREEN_W - (7 * tab_w + 6 * tab_gap)) / 2;
        for (int t = 0; t < 7; t++) {
            int tx = tab_x0 + t * (tab_w + tab_gap);
            bool sel = (t == cur_tab);
            ps3_draw_fill(tx, tab_y, tab_w, 38, sel ? COL_ACCENT_PINK : 0xD8150D30u);
            ps3_draw_rect(tx, tab_y, tab_w, 38, sel ? COL_ACCENT_GOLD : 0x6038F4FFu, sel ? 2 : 1);
            ps3_draw_text_centered(tx + tab_w / 2, tab_y + 8, tabs[t], sel ? 0xFF0A0416u : COL_TEXT_MAIN, 1);
        }

        draw_card(36, 134, 800, 520, 0xFF0D0722u, COL_ACCENT_CYAN);
        draw_card(852, 134, 392, 520, 0xFF0D0722u, COL_ACCENT_PINK);

        const int lx = 54;
        const int lw = 764;
        int ly = 152;
        char val[96];

        if (cur_tab == 0) {
            draw_row_option(lx, ly + 0 * 48, lw, r == 0, "PLAY ARCADE - ENHANCED", "60FPS / 16:9 / HIRES");
            draw_row_option(lx, ly + 1 * 48, lw, r == 1, "PLAY ARCADE - ORIGINAL", "AUTHENTIC 4:3");
            draw_row_option(lx, ly + 2 * 48, lw, r == 2, "CONTINUOUS MODE", "ALL 15 STAGES");
            snprintf(val, sizeof(val), "%d", config.cont_traffic);
            draw_row_option(lx, ly + 3 * 48, lw, r == 3, "CONTINUOUS TRAFFIC", val);
            draw_row_option(lx, ly + 4 * 48, lw, r == 4, "TIME TRIAL MODE", "STAGE RECORD");
            snprintf(val, sizeof(val), "%d LAPS", config.ttrial.laps);
            draw_row_option(lx, ly + 5 * 48, lw, r == 5, "TIME TRIAL LAPS", val);
            draw_row_option(lx, ly + 6 * 48, lw, r == 6, "CLASSIC CANNONBALL ROAD MENU", "OPEN ENGINE UI");
            draw_row_option(lx, ly + 7 * 48, lw, r == 7, "EXIT TO XMB", "SAVE & QUIT");
        } else if (cur_tab == 1) {
            draw_row_option(lx, ly + 0 * 48, lw, r == 0, "SAVE CONFIG & SCORES NOW", "WRITE TO HDD");
            if (g_ps3_cfg.active_slot > 0) snprintf(val, sizeof(val), "SLOT %d", g_ps3_cfg.active_slot);
            else snprintf(val, sizeof(val), "AUTO");
            draw_row_option(lx, ly + 1 * 48, lw, r == 1, "AUTO-MIRROR ACTIVE SLOT", val);
            for (int sl = 1; sl <= 3; sl++) {
                const ps3_slot_info_t& info = slot_cache[sl - 1];
                char lbuf[64], vbuf[64];
                snprintf(lbuf, sizeof(lbuf), "SAVE TO PROFILE SLOT %d", sl);
                if (info.exists) snprintf(vbuf, sizeof(vbuf), "%s (%s)", info.top_name, info.timestamp);
                else snprintf(vbuf, sizeof(vbuf), "EMPTY");
                draw_row_option(lx, ly + (1 + sl) * 48, lw, r == (1 + sl), lbuf, vbuf);
            }
            for (int sl = 1; sl <= 3; sl++) {
                const ps3_slot_info_t& info = slot_cache[sl - 1];
                char lbuf[64], vbuf[64];
                snprintf(lbuf, sizeof(lbuf), "LOAD PROFILE SLOT %d", sl);
                if (info.exists) snprintf(vbuf, sizeof(vbuf), "RESTORE");
                else snprintf(vbuf, sizeof(vbuf), "NONE");
                draw_row_option(lx, ly + (4 + sl) * 48, lw, r == (4 + sl), lbuf, vbuf);
            }
            draw_row_option(lx, ly + 8 * 48, lw, r == 8, "CLEAR SAVED HIGH SCORES", clear_confirm ? "CONFIRM [X]?" : "RESET");
        } else if (cur_tab == 2) {
            static const char *asp_names[PS3_ASPECT_COUNT] = { "AUTO", "16:9 WIDE", "4:3 CLASSIC", "STRETCH" };
            static const char *fps_names[3] = { "30 FPS", "ORIGINAL 30/60", "60 FPS SMOOTH" };
            draw_row_option(lx, ly + 0 * 48, lw, r == 0, "TV ASPECT RATIO", asp_names[g_ps3_cfg.aspect]);
            draw_row_option(lx, ly + 1 * 48, lw, r == 1, "WIDESCREEN GAMEPLAY", config.video.widescreen ? "ON (16:9)" : "OFF (4:3)");
            draw_row_option(lx, ly + 2 * 48, lw, r == 2, "HIGH RESOLUTION SPRITES", config.video.hires ? "ON (640x448)" : "OFF (320x224)");
            draw_row_option(lx, ly + 3 * 48, lw, r == 3, "FRAME RATE MODE", fps_names[config.video.fps % 3]);
            snprintf(val, sizeof(val), "%d%%", g_ps3_cfg.scanlines);
            draw_row_option(lx, ly + 4 * 48, lw, r == 4, "CRT SCANLINES", val);
            draw_row_option(lx, ly + 5 * 48, lw, r == 5, "TEXTURE FILTER", g_ps3_cfg.linear_filter ? "BILINEAR" : "CRISP NEAREST");
            snprintf(val, sizeof(val), "%d%%", g_ps3_cfg.overscan_pct);
            draw_row_option(lx, ly + 6 * 48, lw, r == 6, "TV OVERSCAN MARGIN", val);
            draw_row_option(lx, ly + 7 * 48, lw, r == 7, "SHADOW INTENSITY", config.video.shadow ? "LIGHT (MAME)" : "DARK (ARCADE)");
            draw_row_option(lx, ly + 8 * 48, lw, r == 8, "FPS COUNTER OVERLAY", g_ps3_cfg.show_fps ? "ON" : "OFF");
        } else if (cur_tab == 3) {
            draw_row_option(lx, ly + 0 * 48, lw, r == 0, "MASTER AUDIO", config.sound.enabled ? "ON (48KHZ)" : "MUTED");
            snprintf(val, sizeof(val), "%d%%", g_ps3_cfg.volume_pct);
            draw_row_option(lx, ly + 1 * 48, lw, r == 1, "OUTPUT VOLUME", val);
            draw_row_option(lx, ly + 2 * 48, lw, r == 2, "ATTRACT DEMO SOUND", config.sound.advertise ? "ON" : "OFF");
            draw_row_option(lx, ly + 3 * 48, lw, r == 3, "RADIO PREVIEW MUSIC", config.sound.preview ? "ON" : "OFF");
            draw_row_option(lx, ly + 4 * 48, lw, r == 4, "FIXED PCM ROM (71F)", config.sound.fix_samples ? "ENABLED" : "ORIGINAL");
            const char *tname = (jukebox_idx < 4) ? jukebox_name(jukebox_idx) : config.sound.music[static_cast<size_t>(jukebox_idx - 4)].title.c_str();
            draw_row_option(lx, ly + 5 * 48, lw, r == 5, "JUKEBOX TRACK SELECT", tname);
            draw_row_option(lx, ly + 6 * 48, lw, r == 6, "JUKEBOX PLAY / STOP", jukebox_playing ? "STOP MUSIC" : "PLAY PREVIEW");
        } else if (cur_tab == 4) {
            int win_start = (r >= 9) ? (r - 8) : 0;
            for (int k = 0; k < 10; k++) {
                int idx = win_start + k;
                if (idx >= 13) break;
                int ry = ly + k * 46;
                if (idx == 0) draw_row_option(lx, ry, lw, r == 0, "COURSE TRACK ORDER", config.engine.jap ? "JAPANESE" : "WORLD");
                else if (idx == 1) {
                    static const char *cnames[5] = { "RED", "BLUE", "YELLOW", "GREEN", "CYAN" };
                    draw_row_option(lx, ry, lw, r == 1, "FERRARI CAR COLOR", cnames[config.engine.car_pal % 5]);
                } else if (idx == 2) {
                    snprintf(val, sizeof(val), config.engine.freeze_timer ? "INFINITE" : "LEVEL %d", config.engine.dip_time + 1);
                    draw_row_option(lx, ry, lw, r == 2, "TIMER DIFFICULTY", val);
                } else if (idx == 3) {
                    snprintf(val, sizeof(val), config.engine.disable_traffic ? "NO TRAFFIC" : "LEVEL %d", config.engine.dip_traffic + 1);
                    draw_row_option(lx, ry, lw, r == 3, "TRAFFIC DENSITY", val);
                } else if (idx == 4) draw_row_option(lx, ry, lw, r == 4, "FREEPLAY COIN MODE", config.engine.freeplay ? "ON" : "OFF");
                else if (idx == 5) draw_row_option(lx, ry, lw, r == 5, "FIX ORIGINAL BUGS", config.engine.fix_bugs ? "ON" : "OFF");
                else if (idx == 6) draw_row_option(lx, ry, lw, r == 6, "ENHANCED LEVEL OBJECTS", config.engine.level_objects ? "ON" : "OFF");
                else if (idx == 7) draw_row_option(lx, ry, lw, r == 7, "NEW ATTRACT MODE", config.engine.new_attract ? "ON" : "OFF");
                else if (idx == 8) draw_row_option(lx, ry, lw, r == 8, "FIX STAGE TIMER", config.engine.fix_timer ? "ON" : "OFF");
                else if (idx == 9) draw_row_option(lx, ry, lw, r == 9, "PROTOTYPE STAGE 1", config.engine.prototype ? "ON" : "OFF");
                else if (idx == 10) draw_row_option(lx, ry, lw, r == 10, "GRIPPY TYRES CHEAT", config.engine.grippy_tyres ? "ON" : "OFF");
                else if (idx == 11) draw_row_option(lx, ry, lw, r == 11, "OFF-ROAD NO SLOWDOWN", config.engine.offroad ? "ON" : "OFF");
                else if (idx == 12) draw_row_option(lx, ry, lw, r == 12, "TURBO + ARMOR BUMPER", config.engine.turbo ? "ON" : "OFF");
            }
        } else if (cur_tab == 5) {
            static const char *pnames[3] = { "PS3 TRIGGERS + STICK", "CLASSIC FACE BTNS", "PURE DIGITAL" };
            static const char *anames[3] = { "DIGITAL", "ANALOG STICK+TRIG", "STICK + DIG PEDAL" };
            static const char *gnames[4] = { "TOGGLE BUTTON", "HOLD CABINET", "SEPARATE HI/LO", "AUTOMATIC" };
            draw_row_option(lx, ly + 0 * 48, lw, r == 0, "DS3 CONTROL PRESET", pnames[g_ps3_cfg.control_preset % 3]);
            draw_row_option(lx, ly + 1 * 48, lw, r == 1, "ANALOG INPUT MODE", anames[config.controls.analog % 3]);
            draw_row_option(lx, ly + 2 * 48, lw, r == 2, "GEARBOX MODE", gnames[config.controls.gear % 4]);
            snprintf(val, sizeof(val), "%d", config.controls.steer_speed);
            draw_row_option(lx, ly + 3 * 48, lw, r == 3, "DIGITAL STEERING SPEED", val);
            snprintf(val, sizeof(val), "%d", config.controls.pedal_speed);
            draw_row_option(lx, ly + 4 * 48, lw, r == 4, "DIGITAL PEDAL SPEED", val);
            snprintf(val, sizeof(val), "%d%%", g_ps3_cfg.rumble_pct);
            draw_row_option(lx, ly + 5 * 48, lw, r == 5, "DUALSHOCK 3 RUMBLE", val);
        } else if (cur_tab == 6) {
            snprintf(val, sizeof(val), "%d/%d OK", rep->revb_found, PS3_ROM_REVB_COUNT);
            draw_row_option(lx, ly + 0 * 48, lw, r == 0, "RESCAN OUTRUN ROMS", val);
            draw_row_option(lx, ly + 1 * 48, lw, r == 1, "IMPORT ROMS FROM USB -> HDD", "/dev_usb000");
            draw_row_option(lx, ly + 2 * 48, lw, r == 2, "SAVE MENU SCREENSHOT PNG", "/dev_hdd0/cannonball");
            draw_row_option(lx, ly + 3 * 48, lw, r == 3, "BOOT SPLASH SCREEN", g_ps3_cfg.splash_enabled ? "ENABLED" : "SKIP");

            draw_card(lx, ly + 215, lw, 265, 0xDC120A2Cu, COL_ACCENT_GOLD);
            ps3_draw_text(lx + 22, ly + 230, "CANNONBALL PS3 - PORT BY THEHEROGAC", COL_ACCENT_GOLD, 1);
            ps3_draw_text(lx + 22, ly + 266, "Original Cannonball 0.35 Engine: Chris White (djyt)", COL_TEXT_MAIN, 0);
            ps3_draw_text(lx + 22, ly + 292, "Console Control & Frontend Reference: rsn8887", COL_TEXT_MAIN, 0);
            ps3_draw_text(lx + 22, ly + 318, "Audio Drivers: MAME YM2151 & SegaPCM MultiPCM", COL_TEXT_MAIN, 0);
            ps3_draw_text(lx + 22, ly + 344, "PS3 SDK: PSL1GHT Cell PPU / SDL2 RSX / Cell Audio 48kHz", COL_TEXT_DIM, 0);
            ps3_draw_text(lx + 22, ly + 378, "ROM Paths: /dev_hdd0/cannonball/roms/ or outrun.zip", COL_ACCENT_CYAN, 0);
            ps3_draw_text(lx + 22, ly + 404, "Save Path: /dev_hdd0/cannonball/saves/ (XML + 3 Slots)", COL_ACCENT_CYAN, 0);
            ps3_draw_text(lx + 22, ly + 436, "Copyright Notice: OutRun arcade ROMs belong to SEGA & are NOT bundled.", COL_ACCENT_PINK, 0);
        }

        const int rx = 868;
        if (cur_tab == 1) {
            ps3_draw_text_centered(1048, 152, "TOP ARCADE SCORES", COL_ACCENT_GOLD, 1);
            for (int i = 0; i < 8; i++) {
                char sline[64];
                format_score_row(i, ohiscore.scores[i], sline, sizeof(sline));
                ps3_draw_text(rx + 14, 194 + i * 32, sline, (i == 0) ? COL_ACCENT_CYAN : COL_TEXT_MAIN, 0);
            }
            ps3_draw_fill(rx + 12, 468, 336, 2, 0x60FFFFFFu);
            ps3_draw_text(rx + 14, 484, "SAVES INCLUDE:", COL_ACCENT_GOLD, 0);
            ps3_draw_text(rx + 14, 512, "- XML Settings & Controls", COL_TEXT_DIM, 0);
            ps3_draw_text(rx + 14, 536, "- World & Japan High Scores", COL_TEXT_DIM, 0);
            ps3_draw_text(rx + 14, 560, "- Time Trial Stage Bests", COL_TEXT_DIM, 0);
            ps3_draw_text(rx + 14, 584, "- 3 Persistent Backup Slots", COL_TEXT_DIM, 0);
        } else if (cur_tab == 4) {
            draw_car_preview(rx + 12, 152, 336, 236, config.engine.car_pal);
            ps3_draw_text(rx + 14, 412, "TIPS & ENHANCEMENTS:", COL_ACCENT_GOLD, 0);
            ps3_draw_text(rx + 14, 444, "Turn on Hi-Res & Widescreen", COL_TEXT_MAIN, 0);
            ps3_draw_text(rx + 14, 468, "in Video for ultra-smooth", COL_TEXT_DIM, 0);
            ps3_draw_text(rx + 14, 492, "60 FPS Testarossa cruising!", COL_TEXT_DIM, 0);
        } else if (cur_tab == 5) {
            ps3_draw_text_centered(1048, 152, "DUALSHOCK 3 LAYOUT", COL_ACCENT_GOLD, 1);
            ps3_draw_text(rx + 10, 198, "L-Stick / D-Pad : Steer Car", COL_TEXT_MAIN, 0);
            ps3_draw_text(rx + 10, 228, "R2 / Cross [X]  : Accelerate", COL_TEXT_MAIN, 0);
            ps3_draw_text(rx + 10, 258, "L2 / Square     : Brake", COL_TEXT_MAIN, 0);
            ps3_draw_text(rx + 10, 288, "R1 / L1 / Tri   : Shift Gear", COL_TEXT_MAIN, 0);
            ps3_draw_text(rx + 10, 318, "Circle [O]      : Camera View", COL_TEXT_MAIN, 0);
            ps3_draw_text(rx + 10, 348, "START           : Insert Coin / Start", COL_TEXT_MAIN, 0);
            ps3_draw_text(rx + 10, 378, "SELECT / L3+R3  : PS3 Quick Menu", COL_ACCENT_CYAN, 0);
            ps3_draw_text(rx + 10, 418, "L2+R2+START     : Classic Menu", COL_ACCENT_GOLD, 0);
            ps3_draw_text(rx + 10, 448, "R3              : Screenshot PNG", COL_ACCENT_PINK, 0);
        } else {
            draw_car_preview(rx + 12, 152, 336, 220, config.engine.car_pal);
            ps3_draw_text(rx + 14, 392, "SYSTEM HEALTH:", COL_ACCENT_GOLD, 0);
            char b1[64], b2[64], b3[64], b4[64];
            snprintf(b1, sizeof(b1), "Rev B ROMs : %d / %d OK", rep->revb_found, PS3_ROM_REVB_COUNT);
            snprintf(b2, sizeof(b2), "Jap Tracks : %d / %d", rep->jap_found, PS3_ROM_JAP_COUNT);
            snprintf(b3, sizeof(b3), "Fixed PCM  : %s", rep->pcm_found ? "DETECTED" : "NOT PRESENT");
            snprintf(b4, sizeof(b4), "Engine Res : %d / %d OK", rep->res_found, PS3_RES_BIN_COUNT);
            ps3_draw_text(rx + 14, 424, b1, COL_OK, 0);
            ps3_draw_text(rx + 14, 452, b2, rep->jap_found == 8 ? COL_OK : COL_TEXT_DIM, 0);
            ps3_draw_text(rx + 14, 480, b3, rep->pcm_found ? COL_OK : COL_TEXT_DIM, 0);
            ps3_draw_text(rx + 14, 508, b4, COL_OK, 0);
            ps3_draw_text(rx + 14, 554, "IN-GAME QUICK MENU:", COL_ACCENT_CYAN, 0);
            ps3_draw_text(rx + 14, 582, "Press SELECT while driving", COL_TEXT_DIM, 0);
            ps3_draw_text(rx + 14, 608, "to save, tweak or screenshot!", COL_TEXT_DIM, 0);
        }

        ps3_draw_fill(0, 668, PS3_SCREEN_W, 52, 0xEC09041Au);
        if (banner_timer > 0 && banner[0]) {
            ps3_draw_text_centered(PS3_SCREEN_W / 2, 682, banner, COL_ACCENT_GOLD, 1);
            banner_timer--;
        } else {
            int fx = 42;
            draw_footer_btn(&fx, 682, BTN_ICON_DPAD, "Navigate");
            draw_footer_btn(&fx, 682, BTN_ICON_L1, "Prev Tab");
            draw_footer_btn(&fx, 682, BTN_ICON_R1, "Next Tab");
            draw_footer_btn(&fx, 682, BTN_ICON_CROSS, "Select / Toggle");
            draw_footer_btn(&fx, 682, BTN_ICON_SQUARE, "Quick Save");
            draw_footer_btn(&fx, 682, BTN_ICON_START, "Launch Game");
        }

        ps3_video_present();
        uint32_t dt = ps3_ticks_ms() - t0;
        if (dt < 16u) ps3_sleep_ms(16u - dt);
    }

    return PS3_LAUNCH_EXIT;
}

int ps3_ui_pause_menu(void)
{
    ps3_audio_pause(1);
    uint32_t *fb = ps3_video_fb();
    size_t fb_bytes = static_cast<size_t>(PS3_SCREEN_W * PS3_SCREEN_H) * sizeof(uint32_t);
    uint32_t *bg_snap = static_cast<uint32_t *>(malloc(fb_bytes));
    if (bg_snap && fb) {
        for (int i = 0; i < PS3_SCREEN_W * PS3_SCREEN_H; i++) {
            uint32_t p = fb[i];
            uint32_t r = ((p >> 16) & 0xFFu) / 3u;
            uint32_t g = ((p >> 8) & 0xFFu) / 3u;
            uint32_t b = (p & 0xFFu) / 3u;
            bg_snap[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }

    int cur = 0;
    const int count = 10;
    char msg[128] = "";
    int msg_timer = 0;

    while (!g_app_quit) {
        uint32_t t0 = ps3_ticks_ms();
        ps3_sysutil_check();
        ps3_input_poll();
        uint32_t pressed = ps3_input_pressed();

        if (pressed & (PS3_ACT_CIRCLE | PS3_ACT_SELECT)) {
            ps3_saves_sync_all(true);
            ps3_audio_clear_ring();
            ps3_audio_pause(config.sound.enabled ? 0 : 1);
            if (bg_snap) free(bg_snap);
            return PS3_PAUSE_RESUME;
        }
        if (pressed & PS3_ACT_UP) cur = (cur + count - 1) % count;
        if (pressed & PS3_ACT_DOWN) cur = (cur + 1) % count;

        int dir = 0;
        if (pressed & PS3_ACT_LEFT) dir = -1;
        if (pressed & PS3_ACT_RIGHT) dir = 1;
        bool act = (pressed & (PS3_ACT_CROSS | PS3_ACT_START)) != 0;

        if (cur == 0 && act) {
            ps3_saves_sync_all(true);
            ps3_audio_clear_ring();
            ps3_audio_pause(config.sound.enabled ? 0 : 1);
            if (bg_snap) free(bg_snap);
            return PS3_PAUSE_RESUME;
        } else if (cur == 1 && act) {
            bool ok = ps3_saves_sync_all(true);
            snprintf(msg, sizeof(msg), ok ? "Settings & High Scores Saved!" : "Save Failed!");
            msg_timer = 100;
        } else if (cur == 2 && (dir != 0 || act)) {
            g_ps3_cfg.aspect = (g_ps3_cfg.aspect + (dir < 0 ? PS3_ASPECT_COUNT - 1 : 1)) % PS3_ASPECT_COUNT;
        } else if (cur == 3 && (dir != 0 || act)) {
            g_ps3_cfg.scanlines += (dir < 0 ? -10 : 10);
            if (g_ps3_cfg.scanlines > 100) g_ps3_cfg.scanlines = 0;
            if (g_ps3_cfg.scanlines < 0) g_ps3_cfg.scanlines = 100;
            config.video.scanlines = g_ps3_cfg.scanlines;
        } else if (cur == 4 && (dir != 0 || act)) {
            g_ps3_cfg.linear_filter ^= 1;
            ps3_video_apply_filter();
        } else if (cur == 5 && (dir != 0 || act)) {
            g_ps3_cfg.volume_pct += (dir < 0 ? -10 : 10);
            if (g_ps3_cfg.volume_pct > 100) g_ps3_cfg.volume_pct = 0;
            if (g_ps3_cfg.volume_pct < 0) g_ps3_cfg.volume_pct = 100;
        } else if (cur == 6 && act) {
            char sp[512];
            bool ok = ps3_video_save_screenshot(sp, sizeof(sp));
            snprintf(msg, sizeof(msg), ok ? "Screenshot saved!" : "Screenshot failed!");
            msg_timer = 100;
        } else if (cur == 7 && act) {
            ps3_saves_sync_all(true);
            ps3_audio_clear_ring();
            ps3_audio_pause(config.sound.enabled ? 0 : 1);
            if (bg_snap) free(bg_snap);
            return PS3_PAUSE_CLASSIC_MENU;
        } else if (cur == 8 && act) {
            ps3_saves_sync_all(true);
            ps3_audio_clear_ring();
            ps3_audio_pause(1);
            if (bg_snap) free(bg_snap);
            return PS3_PAUSE_MAIN_UI;
        } else if (cur == 9 && act) {
            ps3_saves_sync_all(true);
            ps3_audio_pause(1);
            if (bg_snap) free(bg_snap);
            return PS3_PAUSE_EXIT;
        }

        if (bg_snap && fb) {
            memcpy(fb, bg_snap, fb_bytes);
        } else {
            ps3_video_clear(0xFF0A0518u);
        }
        draw_card(300, 82, 680, 556, 0xFF100724u, COL_ACCENT_PINK);
        ps3_draw_fill(302, 84, 676, 48, 0xFF280E4Eu);
        ps3_draw_text_centered(PS3_SCREEN_W / 2, 96, "CANNONBALL PS3 - QUICK MENU", COL_ACCENT_GOLD, 2);

        static const char *asp_names[PS3_ASPECT_COUNT] = { "AUTO", "16:9", "4:3", "STRETCH" };
        char vbuf[64];
        const int rx = 324;
        const int rw = 632;
        int ry = 148;

        draw_row_option(rx, ry + 0 * 44, rw, cur == 0, "RESUME GAME", "");
        draw_row_option(rx, ry + 1 * 44, rw, cur == 1, "SAVE SETTINGS & SCORES", "HDD + SLOT");
        draw_row_option(rx, ry + 2 * 44, rw, cur == 2, "ASPECT RATIO", asp_names[g_ps3_cfg.aspect]);
        snprintf(vbuf, sizeof(vbuf), "%d%%", g_ps3_cfg.scanlines);
        draw_row_option(rx, ry + 3 * 44, rw, cur == 3, "SCANLINES", vbuf);
        draw_row_option(rx, ry + 4 * 44, rw, cur == 4, "FILTER", g_ps3_cfg.linear_filter ? "BILINEAR" : "NEAREST");
        snprintf(vbuf, sizeof(vbuf), "%d%%", g_ps3_cfg.volume_pct);
        draw_row_option(rx, ry + 5 * 44, rw, cur == 5, "VOLUME", vbuf);
        draw_row_option(rx, ry + 6 * 44, rw, cur == 6, "SAVE SCREENSHOT PNG", "/dev_hdd0");
        draw_row_option(rx, ry + 7 * 44, rw, cur == 7, "CLASSIC CANNONBALL MENU", "");
        draw_row_option(rx, ry + 8 * 44, rw, cur == 8, "PS3 ARCADE FRONTEND", "");
        draw_row_option(rx, ry + 9 * 44, rw, cur == 9, "EXIT TO XMB", "");

        if (msg_timer > 0 && msg[0]) {
            ps3_draw_text_centered(PS3_SCREEN_W / 2, 598, msg, COL_OK, 1);
            msg_timer--;
        } else {
            ps3_draw_text_centered(PS3_SCREEN_W / 2, 600, "[CIRCLE / SELECT] Resume   •   [CROSS] Select   •   theheroGAC", COL_TEXT_DIM, 0);
        }

        ps3_video_present();
        uint32_t dt = ps3_ticks_ms() - t0;
        if (dt < 16u) ps3_sleep_ms(16u - dt);
    }

    return PS3_PAUSE_EXIT;
}
