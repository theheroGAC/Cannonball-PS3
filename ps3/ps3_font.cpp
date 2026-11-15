#include "ps3_platform.hpp"
#include "ps3_font.hpp"
#include "ps3_btn_icons.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const uint8_t g_font5x7[96][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04},
    {0x0A,0x0A,0x0A,0x00,0x00,0x00,0x00},
    {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00},
    {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04},
    {0x18,0x19,0x02,0x04,0x08,0x13,0x03},
    {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D},
    {0x0C,0x04,0x08,0x00,0x00,0x00,0x00},
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02},
    {0x08,0x04,0x02,0x02,0x02,0x04,0x08},
    {0x04,0x15,0x0E,0x1F,0x0E,0x15,0x04},
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
    {0x00,0x00,0x00,0x00,0x0C,0x04,0x08},
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},
    {0x01,0x02,0x02,0x04,0x08,0x08,0x10},
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},
    {0x1F,0x02,0x04,0x06,0x01,0x11,0x0E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00},
    {0x00,0x0C,0x0C,0x00,0x0C,0x04,0x08},
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02},
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08},
    {0x0E,0x11,0x02,0x04,0x04,0x00,0x04},
    {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E},
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
    {0x07,0x02,0x02,0x02,0x12,0x12,0x0C},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E},
    {0x10,0x08,0x08,0x04,0x02,0x02,0x01},
    {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E},
    {0x04,0x0A,0x11,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x1F},
    {0x08,0x04,0x02,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F},
    {0x10,0x10,0x16,0x19,0x11,0x11,0x1E},
    {0x00,0x00,0x0E,0x11,0x10,0x11,0x0E},
    {0x01,0x01,0x0D,0x13,0x11,0x11,0x0F},
    {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E},
    {0x06,0x09,0x08,0x1C,0x08,0x08,0x08},
    {0x00,0x00,0x0F,0x11,0x0F,0x01,0x0E},
    {0x10,0x10,0x16,0x19,0x11,0x11,0x11},
    {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E},
    {0x02,0x00,0x06,0x02,0x02,0x12,0x0C},
    {0x10,0x10,0x12,0x14,0x18,0x14,0x12},
    {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E},
    {0x00,0x00,0x1A,0x15,0x15,0x11,0x11},
    {0x00,0x00,0x16,0x19,0x11,0x11,0x11},
    {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E},
    {0x00,0x00,0x1E,0x11,0x1E,0x10,0x10},
    {0x00,0x00,0x0F,0x11,0x0F,0x01,0x01},
    {0x00,0x00,0x16,0x19,0x10,0x10,0x10},
    {0x00,0x00,0x0E,0x10,0x0E,0x01,0x1E},
    {0x08,0x08,0x1C,0x08,0x08,0x09,0x06},
    {0x00,0x00,0x11,0x11,0x11,0x13,0x0D},
    {0x00,0x00,0x11,0x11,0x11,0x0A,0x04},
    {0x00,0x00,0x11,0x11,0x15,0x15,0x0A},
    {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11},
    {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E},
    {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F},
    {0x06,0x08,0x08,0x10,0x08,0x08,0x06},
    {0x04,0x04,0x04,0x00,0x04,0x04,0x04},
    {0x0C,0x04,0x04,0x02,0x04,0x04,0x0C},
    {0x08,0x15,0x02,0x00,0x00,0x00,0x00},
    {0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}
};

#define TTF_SLOTS 4
#define TTF_CACHE_MAX 256

static const int g_ttf_sizes[TTF_SLOTS] = { 15, 20, 26, 40 };
static TTF_Font *g_fonts[TTF_SLOTS] = { nullptr, nullptr, nullptr, nullptr };
static int g_ttf_ready = 0;

struct ttf_cache_entry_t {
    uint32_t hash;
    int slot;
    int w;
    int h;
    uint32_t last_use;
    uint8_t *alpha;
    char text[160];
};

static ttf_cache_entry_t g_cache[TTF_CACHE_MAX];
static uint32_t g_cache_tick = 1;

static uint32_t fnv1a_text(const char *s, int slot)
{
    uint32_t h = 2166136261u ^ static_cast<uint32_t>(slot * 16777619u);
    while (*s) {
        h ^= static_cast<uint8_t>(*s++);
        h *= 16777619u;
    }
    return h;
}

static void clear_ttf_cache(void)
{
    for (int i = 0; i < TTF_CACHE_MAX; i++) {
        if (g_cache[i].alpha) {
            free(g_cache[i].alpha);
            g_cache[i].alpha = nullptr;
        }
        g_cache[i].text[0] = '\0';
        g_cache[i].last_use = 0;
    }
}

static inline uint32_t blend_argb_px(uint32_t dst, uint32_t src, uint32_t a)
{
    if (a == 0) return dst;
    if (a >= 255) return 0xFF000000u | (src & 0x00FFFFFFu);
    uint32_t ia = 255u - a;
    uint32_t rb = (((src & 0x00FF00FFu) * a) + ((dst & 0x00FF00FFu) * ia)) >> 8;
    uint32_t g = (((src & 0x0000FF00u) * a) + ((dst & 0x0000FF00u) * ia)) >> 8;
    return 0xFF000000u | (rb & 0x00FF00FFu) | (g & 0x0000FF00u);
}

void ps3_font5x7_draw_string(uint32_t *fb, int fb_w, int fb_h, int x, int y, const char *s, uint32_t argb, int scale)
{
    if (!fb || !s || scale < 1) return;
    uint32_t col = 0xFF000000u | (argb & 0x00FFFFFFu);
    int cx = x;
    while (*s) {
        uint8_t ch = static_cast<uint8_t>(*s++);
        if (ch < 32 || ch > 127) ch = '?';
        const uint8_t *rows = g_font5x7[ch - 32];
        for (int r = 0; r < 7; r++) {
            uint8_t bits = rows[r];
            for (int c = 0; c < 5; c++) {
                if (bits & (0x10 >> c)) {
                    int px0 = cx + c * scale;
                    int py0 = y + r * scale;
                    for (int sy = 0; sy < scale; sy++) {
                        int py = py0 + sy;
                        if (py < 0 || py >= fb_h) continue;
                        uint32_t *dst_row = fb + py * fb_w;
                        for (int sx = 0; sx < scale; sx++) {
                            int px = px0 + sx;
                            if (px >= 0 && px < fb_w) dst_row[px] = col;
                        }
                    }
                }
            }
        }
        cx += 6 * scale;
    }
}

void ps3_font_init(void)
{
    if (g_ttf_ready) return;
    if (TTF_Init() != 0) return;

    char res_font[512];
    snprintf(res_font, sizeof(res_font), "%sfont.ttf", g_ps3_res_dir);

    const char *candidates[] = {
        res_font,
        PS3_PKG_USRDIR "/res/font.ttf",
        PS3_PKG_USRDIR "/font.ttf",
        PS3_ROOT "/font.ttf",
        "./res/font.ttf",
        "./font.ttf",
        nullptr
    };

    const char *found = nullptr;
    for (int i = 0; candidates[i]; i++) {
        FILE *fp = fopen(candidates[i], "rb");
        if (!fp) continue;
        fclose(fp);
        TTF_Font *probe = TTF_OpenFont(candidates[i], g_ttf_sizes[1]);
        if (probe) {
            TTF_CloseFont(probe);
            found = candidates[i];
            break;
        }
    }

    if (!found) {
        TTF_Quit();
        return;
    }

    for (int i = 0; i < TTF_SLOTS; i++) {
        g_fonts[i] = TTF_OpenFont(found, g_ttf_sizes[i]);
    }
    memset(g_cache, 0, sizeof(g_cache));
    g_cache_tick = 1;
    g_ttf_ready = (g_fonts[1] != nullptr) ? 1 : 0;
    if (!g_ttf_ready) {
        for (int i = 0; i < TTF_SLOTS; i++) {
            if (g_fonts[i]) {
                TTF_CloseFont(g_fonts[i]);
                g_fonts[i] = nullptr;
            }
        }
        TTF_Quit();
    }
}

void ps3_font_shutdown(void)
{
    clear_ttf_cache();
    for (int i = 0; i < TTF_SLOTS; i++) {
        if (g_fonts[i]) {
            TTF_CloseFont(g_fonts[i]);
            g_fonts[i] = nullptr;
        }
    }
    if (g_ttf_ready) {
        TTF_Quit();
        g_ttf_ready = 0;
    }
}

static const ttf_cache_entry_t *get_cached_glyph_bitmap(const char *text, int slot)
{
    if (!g_ttf_ready || !g_fonts[slot] || !text || !text[0]) return nullptr;
    char short_txt[160];
    snprintf(short_txt, sizeof(short_txt), "%s", text);
    uint32_t h = fnv1a_text(short_txt, slot);

    int lru = 0;
    uint32_t oldest = 0xFFFFFFFFu;
    for (int i = 0; i < TTF_CACHE_MAX; i++) {
        if (g_cache[i].alpha && g_cache[i].hash == h && g_cache[i].slot == slot && strcmp(g_cache[i].text, short_txt) == 0) {
            g_cache[i].last_use = ++g_cache_tick;
            return &g_cache[i];
        }
        if (g_cache[i].last_use < oldest) {
            oldest = g_cache[i].last_use;
            lru = i;
        }
    }

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *raw = TTF_RenderUTF8_Blended(g_fonts[slot], short_txt, white);
    if (!raw) return nullptr;
    SDL_Surface *surf = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(raw);
    if (!surf) return nullptr;

    int w = surf->w;
    int h_px = surf->h;
    uint8_t *alpha = static_cast<uint8_t *>(malloc(static_cast<size_t>(w * h_px)));
    if (!alpha) {
        SDL_FreeSurface(surf);
        return nullptr;
    }

    const uint8_t *pixels = static_cast<const uint8_t *>(surf->pixels);
    for (int y = 0; y < h_px; y++) {
        const uint32_t *row = reinterpret_cast<const uint32_t *>(pixels + y * surf->pitch);
        for (int x = 0; x < w; x++) {
            alpha[y * w + x] = static_cast<uint8_t>((row[x] >> 24) & 0xFFu);
        }
    }
    SDL_FreeSurface(surf);

    if (g_cache[lru].alpha) free(g_cache[lru].alpha);
    g_cache[lru].hash = h;
    g_cache[lru].slot = slot;
    g_cache[lru].w = w;
    g_cache[lru].h = h_px;
    g_cache[lru].alpha = alpha;
    g_cache[lru].last_use = ++g_cache_tick;
    snprintf(g_cache[lru].text, sizeof(g_cache[lru].text), "%s", short_txt);
    return &g_cache[lru];
}

void ps3_draw_fill(int x, int y, int w, int h, uint32_t argb)
{
    uint32_t *fb = ps3_video_fb();
    if (!fb || w <= 0 || h <= 0) return;
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = (x + w > PS3_SCREEN_W) ? PS3_SCREEN_W : (x + w);
    int y1 = (y + h > PS3_SCREEN_H) ? PS3_SCREEN_H : (y + h);
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t c = 0xFF000000u | (argb & 0x00FFFFFFu);
    int span = x1 - x0;
    uint32_t *row = fb + y0 * PS3_SCREEN_W + x0;
    for (int py = y0; py < y1; py++) {
        for (int px = 0; px < span; px++) row[px] = c;
        row += PS3_SCREEN_W;
    }
}

void ps3_draw_blend(int x, int y, int w, int h, uint32_t argb)
{
    uint32_t *fb = ps3_video_fb();
    if (!fb || w <= 0 || h <= 0) return;
    uint32_t a = (argb >> 24) & 0xFFu;
    if (a == 0) return;
    if (a == 255) {
        ps3_draw_fill(x, y, w, h, argb);
        return;
    }
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = (x + w > PS3_SCREEN_W) ? PS3_SCREEN_W : (x + w);
    int y1 = (y + h > PS3_SCREEN_H) ? PS3_SCREEN_H : (y + h);
    if (x0 >= x1 || y0 >= y1) return;

    uint32_t ia = 256u - a;
    uint32_t src_rb_a = (argb & 0x00FF00FFu) * a;
    uint32_t src_g_a = (argb & 0x0000FF00u) * a;
    int span = x1 - x0;
    uint32_t *row = fb + y0 * PS3_SCREEN_W + x0;

    for (int py = y0; py < y1; py++) {
        for (int px = 0; px < span; px++) {
            uint32_t dst = row[px];
            uint32_t rb = (src_rb_a + ((dst & 0x00FF00FFu) * ia)) >> 8;
            uint32_t g = (src_g_a + ((dst & 0x0000FF00u) * ia)) >> 8;
            row[px] = 0xFF000000u | (rb & 0x00FF00FFu) | (g & 0x0000FF00u);
        }
        row += PS3_SCREEN_W;
    }
}

void ps3_draw_rect(int x, int y, int w, int h, uint32_t argb, int thickness)
{
    if (thickness <= 0) thickness = 1;
    if ((argb >> 24) >= 0xFEu) {
        ps3_draw_fill(x, y, w, thickness, argb);
        ps3_draw_fill(x, y + h - thickness, w, thickness, argb);
        ps3_draw_fill(x, y, thickness, h, argb);
        ps3_draw_fill(x + w - thickness, y, thickness, h, argb);
    } else {
        ps3_draw_blend(x, y, w, thickness, argb);
        ps3_draw_blend(x, y + h - thickness, w, thickness, argb);
        ps3_draw_blend(x, y, thickness, h, argb);
        ps3_draw_blend(x + w - thickness, y, thickness, h, argb);
    }
}

void ps3_draw_gradient_v(int x, int y, int w, int h, uint32_t top_argb, uint32_t bot_argb)
{
    uint32_t *fb = ps3_video_fb();
    if (!fb || w <= 0 || h <= 0) return;
    int x0 = (x < 0) ? 0 : x;
    int x1 = (x + w > PS3_SCREEN_W) ? PS3_SCREEN_W : (x + w);
    int r0 = static_cast<int>((top_argb >> 16) & 0xFFu);
    int g0 = static_cast<int>((top_argb >> 8) & 0xFFu);
    int b0 = static_cast<int>(top_argb & 0xFFu);
    int r1 = static_cast<int>((bot_argb >> 16) & 0xFFu);
    int g1 = static_cast<int>((bot_argb >> 8) & 0xFFu);
    int b1 = static_cast<int>(bot_argb & 0xFFu);
    int den = (h > 1) ? (h - 1) : 1;

    for (int dy = 0; dy < h; dy++) {
        int py = y + dy;
        if (py < 0 || py >= PS3_SCREEN_H) continue;
        uint32_t r = static_cast<uint32_t>(r0 + ((r1 - r0) * dy) / den);
        uint32_t g = static_cast<uint32_t>(g0 + ((g1 - g0) * dy) / den);
        uint32_t b = static_cast<uint32_t>(b0 + ((b1 - b0) * dy) / den);
        uint32_t c = 0xFF000000u | (r << 16) | (g << 8) | b;
        uint32_t *row = fb + py * PS3_SCREEN_W;
        for (int px = x0; px < x1; px++) row[px] = c;
    }
}

void ps3_draw_text(int x, int y, const char *text, uint32_t argb, int size_tier)
{
    uint32_t *fb = ps3_video_fb();
    if (!fb || !text || !text[0]) return;
    if (size_tier < 0) size_tier = 0;
    if (size_tier >= TTF_SLOTS) size_tier = TTF_SLOTS - 1;

    const ttf_cache_entry_t *ent = get_cached_glyph_bitmap(text, size_tier);
    if (!ent) {
        int sc = (size_tier == 0) ? 2 : (size_tier == 1 ? 2 : (size_tier == 2 ? 3 : 4));
        ps3_font5x7_draw_string(fb, PS3_SCREEN_W, PS3_SCREEN_H, x, y, text, argb, sc);
        return;
    }

    uint32_t global_a = (argb >> 24) & 0xFFu;
    if (global_a == 0) global_a = 255u;

    for (int py = 0; py < ent->h; py++) {
        int dy = y + py;
        if (dy < 0 || dy >= PS3_SCREEN_H) continue;
        uint32_t *dst = fb + dy * PS3_SCREEN_W;
        const uint8_t *arow = ent->alpha + py * ent->w;
        for (int px = 0; px < ent->w; px++) {
            int dx = x + px;
            if (dx < 0 || dx >= PS3_SCREEN_W) continue;
            uint32_t a = (static_cast<uint32_t>(arow[px]) * global_a) / 255u;
            if (a > 4u) {
                dst[dx] = blend_argb_px(dst[dx], argb, a);
            }
        }
    }
}

int ps3_measure_text(const char *text, int size_tier)
{
    if (!text || !text[0]) return 0;
    if (size_tier < 0) size_tier = 0;
    if (size_tier >= TTF_SLOTS) size_tier = TTF_SLOTS - 1;
    const ttf_cache_entry_t *ent = get_cached_glyph_bitmap(text, size_tier);
    if (ent) return ent->w;
    int sc = (size_tier == 0) ? 2 : (size_tier == 1 ? 2 : (size_tier == 2 ? 3 : 4));
    return static_cast<int>(strlen(text)) * 6 * sc;
}

void ps3_draw_text_centered(int cx, int y, const char *text, uint32_t argb, int size_tier)
{
    int w = ps3_measure_text(text, size_tier);
    ps3_draw_text(cx - w / 2, y, text, argb, size_tier);
}

void ps3_draw_btn_icon(int icon_id, int x, int y)
{
    uint32_t *fb = ps3_video_fb();
    if (!fb || icon_id < 0 || icon_id >= BTN_ICON_COUNT) return;
    const uint32_t *src = ps3_btn_icons_data[icon_id];
    for (int py = 0; py < PS3_BTN_ICON_SIZE; py++) {
        int dy = y + py;
        if (dy < 0 || dy >= PS3_SCREEN_H) continue;
        uint32_t *dst = fb + dy * PS3_SCREEN_W;
        for (int px = 0; px < PS3_BTN_ICON_SIZE; px++) {
            int dx = x + px;
            if (dx < 0 || dx >= PS3_SCREEN_W) continue;
            uint32_t sp = src[py * PS3_BTN_ICON_SIZE + px];
            uint32_t a = (sp >> 24) & 0xFFu;
            if (a > 0) dst[dx] = blend_argb_px(dst[dx], sp, a);
        }
    }
}
