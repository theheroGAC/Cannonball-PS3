#include "ps3_platform.hpp"
#include "ps3_font.hpp"

#include <SDL.h>
#include <sys/systime.h>
#include <zlib.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

static SDL_Window *g_win = nullptr;
static SDL_Renderer *g_ren = nullptr;
static SDL_Texture *g_tex = nullptr;
static SDL_Texture *g_game_tex = nullptr;
static int g_game_tex_w = 0;
static int g_game_tex_h = 0;
static uint32_t *g_fb = nullptr;
static uint32_t *g_game_argb = nullptr;
static int g_game_argb_cap = 0;
static int g_last_game_w = 0;
static int g_last_game_h = 0;
static int g_in_game_frame = 0;
static int g_out_w = PS3_SCREEN_W;
static int g_out_h = PS3_SCREEN_H;

static volatile int g_rsx_hw_linear = 0;
static int g_native_clear_frames = 0;
static int g_prev_src_w = -1;
static int g_prev_src_h = -1;
static int g_prev_aspect = -1;
static int g_prev_overscan = -1;

extern "C" void __real_rsxSetTransferScaleSurface(void *context, void *scale, void *surface);
extern "C" void __wrap_rsxSetTransferScaleSurface(void *context, void *scale, void *surface)
{
    if (scale) {
        reinterpret_cast<uint8_t *>(scale)[43] = (g_rsx_hw_linear != 0) ? 1u : 0u;
    }
    __real_rsxSetTransferScaleSurface(context, scale, surface);
}

static char g_toast_msg[128] = "";
static int g_toast_frames = 0;
static int g_cur_fps = 0;

static void be32_write(uint8_t *p, uint32_t v)
{
    p[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[3] = static_cast<uint8_t>(v & 0xFF);
}

static bool write_png_chunk(FILE *fp, const char type[4], const uint8_t *data, uint32_t len)
{
    uint8_t hdr[8];
    be32_write(hdr, len);
    memcpy(hdr + 4, type, 4);
    if (fwrite(hdr, 1, 8, fp) != 8) return false;
    if (len > 0 && fwrite(data, 1, len, fp) != len) return false;
    uLong c = crc32(0L, reinterpret_cast<const Bytef *>(type), 4);
    if (len > 0) c = crc32(c, data, len);
    uint8_t crc_bytes[4];
    be32_write(crc_bytes, static_cast<uint32_t>(c));
    return fwrite(crc_bytes, 1, 4, fp) == 4;
}

static bool save_argb_as_png(const char *path, const uint32_t *argb, int w, int h)
{
    size_t row_bytes = static_cast<size_t>(1 + w * 3);
    size_t raw_size = row_bytes * static_cast<size_t>(h);
    uint8_t *raw = static_cast<uint8_t *>(malloc(raw_size));
    if (!raw) return false;

    for (int y = 0; y < h; y++) {
        uint8_t *dst = raw + static_cast<size_t>(y) * row_bytes;
        dst[0] = 0;
        const uint32_t *src = argb + y * w;
        for (int x = 0; x < w; x++) {
            uint32_t px = src[x];
            dst[1 + x * 3 + 0] = static_cast<uint8_t>((px >> 16) & 0xFF);
            dst[1 + x * 3 + 1] = static_cast<uint8_t>((px >> 8) & 0xFF);
            dst[1 + x * 3 + 2] = static_cast<uint8_t>(px & 0xFF);
        }
    }

    uLongf comp_bound = compressBound(static_cast<uLong>(raw_size));
    uint8_t *comp = static_cast<uint8_t *>(malloc(comp_bound));
    if (!comp) {
        free(raw);
        return false;
    }

    int zret = compress2(comp, &comp_bound, raw, static_cast<uLong>(raw_size), Z_BEST_SPEED);
    free(raw);
    if (zret != Z_OK) {
        free(comp);
        return false;
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(comp);
        return false;
    }

    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    bool ok = fwrite(sig, 1, 8, fp) == 8;

    uint8_t ihdr[13];
    be32_write(ihdr, static_cast<uint32_t>(w));
    be32_write(ihdr + 4, static_cast<uint32_t>(h));
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;

    if (ok) ok = write_png_chunk(fp, "IHDR", ihdr, 13);
    if (ok) ok = write_png_chunk(fp, "IDAT", comp, static_cast<uint32_t>(comp_bound));
    if (ok) ok = write_png_chunk(fp, "IEND", nullptr, 0);

    fclose(fp);
    free(comp);
    return ok;
}

int ps3_video_init(void)
{
    if (g_win) return 0;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");

    ps3_boot_mark("sdl_init_begin");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        ps3_log("SDL_Init VIDEO failed: %s\n", SDL_GetError());
        return -1;
    }
    ps3_boot_mark("sdl_init_ok");

    g_fb = static_cast<uint32_t *>(calloc(PS3_SCREEN_W * PS3_SCREEN_H, sizeof(uint32_t)));
    if (!g_fb) return -1;

    SDL_DisplayMode dm;
    memset(&dm, 0, sizeof(dm));
    int win_w = 1280;
    int win_h = 720;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
        win_w = dm.w;
        win_h = dm.h;
    }

    const Uint32 wflags[2] = { 0, SDL_WINDOW_SHOWN };
    for (int wi = 0; wi < 2 && !g_win; wi++) {
        g_win = SDL_CreateWindow("Cannonball PS3", 0, 0, win_w, win_h, wflags[wi]);
    }
    if (!g_win) {
        g_win = SDL_CreateWindow("Cannonball PS3", 0, 0, 1280, 720, 0);
    }
    if (!g_win) {
        ps3_log("SDL_CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }
    ps3_boot_mark("window_ok");

    g_out_w = win_w;
    g_out_h = win_h;
    SDL_GetWindowSize(g_win, &g_out_w, &g_out_h);
    if (g_out_w <= 0 || g_out_h <= 0) {
        g_out_w = win_w;
        g_out_h = win_h;
    }

    const Uint32 rflags[4] = {
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE,
        SDL_RENDERER_ACCELERATED,
        SDL_RENDERER_SOFTWARE,
        0
    };
    for (int ri = 0; ri < 4 && !g_ren; ri++) {
        g_ren = SDL_CreateRenderer(g_win, -1, rflags[ri]);
    }
    if (!g_ren) {
        ps3_log("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return -1;
    }
    ps3_boot_mark("renderer_ok");

    SDL_SetRenderDrawColor(g_ren, 8, 12, 24, 255);
    SDL_RenderClear(g_ren);
    SDL_RenderPresent(g_ren);
    ps3_boot_mark("renderer_clear_ok");

    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, PS3_SCREEN_W, PS3_SCREEN_H);
    if (!g_tex) {
        ps3_log("SDL_CreateTexture failed: %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetTextureBlendMode(g_tex, SDL_BLENDMODE_NONE);

    ps3_video_clear(0xFF060612u);
    ps3_video_present();
    ps3_boot_mark("video_init_ok");
    return 0;
}

void ps3_video_shutdown(void)
{
    if (g_game_tex) {
        SDL_DestroyTexture(g_game_tex);
        g_game_tex = nullptr;
    }
    g_game_tex_w = 0;
    g_game_tex_h = 0;
    if (g_game_argb) {
        free(g_game_argb);
        g_game_argb = nullptr;
    }
    g_game_argb_cap = 0;
    if (g_tex) {
        SDL_DestroyTexture(g_tex);
        g_tex = nullptr;
    }
    if (g_ren) {
        SDL_DestroyRenderer(g_ren);
        g_ren = nullptr;
    }
    if (g_win) {
        SDL_DestroyWindow(g_win);
        g_win = nullptr;
    }
    if (g_fb) {
        free(g_fb);
        g_fb = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER);
}

uint32_t *ps3_video_fb(void)
{
    return g_fb;
}

void ps3_video_clear(uint32_t argb)
{
    g_in_game_frame = 0;
    g_native_clear_frames = 4;
    if (!g_fb) return;
    uint32_t c = 0xFF000000u | (argb & 0x00FFFFFFu);
    for (int i = 0; i < PS3_SCREEN_W * PS3_SCREEN_H; i++) {
        g_fb[i] = c;
    }
}

void ps3_video_apply_filter(void)
{
    g_rsx_hw_linear = (g_ps3_cfg.linear_filter != 0) ? 1 : 0;
    g_native_clear_frames = 4;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, g_ps3_cfg.linear_filter ? "1" : "0");
}

void ps3_video_toast(const char *msg, int frames)
{
    if (!msg) return;
    snprintf(g_toast_msg, sizeof(g_toast_msg), "%s", msg);
    g_toast_frames = frames;
}

void ps3_video_set_fps(int fps)
{
    g_cur_fps = fps;
}

static void draw_overlay_badges_native(uint32_t *fb, int w, int h)
{
    if (!fb || w <= 0 || h <= 0) return;
    int scale = (w >= 600) ? 2 : 1;

    if (g_ps3_cfg.show_fps && g_cur_fps > 0) {
        char fbuf[32];
        snprintf(fbuf, sizeof(fbuf), "%d FPS", g_cur_fps);
        int len = static_cast<int>(strlen(fbuf));
        int bw = len * (6 * scale) + (8 * scale);
        int bh = 11 * scale;
        int bx = w - bw - (6 * scale);
        int by = 5 * scale;
        if (bx >= 0 && by >= 0 && bx + bw <= w && by + bh <= h) {
            for (int py = by; py < by + bh; py++) {
                uint32_t *row = fb + py * w;
                for (int px = bx; px < bx + bw; px++) {
                    row[px] = 0xFF140C28u;
                }
            }
            ps3_font5x7_draw_string(fb, w, h, bx + 4 * scale, by + 2 * scale, fbuf, 0xFF38F8FFu, scale);
        }
    }

    if (g_toast_frames > 0 && g_toast_msg[0]) {
        int tw = static_cast<int>(strlen(g_toast_msg)) * (6 * scale) + (12 * scale);
        if (tw > w - 16) tw = w - 16;
        int th = 14 * scale;
        int bx = (w - tw) / 2;
        int by = h - th - (10 * scale);
        if (tw > 8 && bx >= 0 && by >= 0 && bx + tw <= w && by + th <= h) {
            for (int py = by; py < by + th; py++) {
                uint32_t *row = fb + py * w;
                for (int px = bx; px < bx + tw; px++) {
                    if (py == by || py == by + th - 1 || px == bx || px == bx + tw - 1)
                        row[px] = 0xFFFF3878u;
                    else
                        row[px] = 0xFF180D32u;
                }
            }
            ps3_font5x7_draw_string(fb, w, h, bx + 6 * scale, by + 3 * scale, g_toast_msg, 0xFFFFE880u, scale);
        }
        g_toast_frames--;
    }
}

void ps3_video_present(void)
{
    if (!g_ren || !g_tex || !g_fb) return;
    g_in_game_frame = 0;
    g_native_clear_frames = 4;
    g_rsx_hw_linear = (g_out_w != PS3_SCREEN_W || g_out_h != PS3_SCREEN_H) ? 1 : 0;
    SDL_UpdateTexture(g_tex, nullptr, g_fb, PS3_SCREEN_W * static_cast<int>(sizeof(uint32_t)));
    SDL_Rect dst = { 0, 0, g_out_w, g_out_h };
    SDL_RenderCopy(g_ren, g_tex, nullptr, &dst);
    SDL_RenderPresent(g_ren);
}

static void compute_dest_rect(int src_w, int src_h, int *out_x, int *out_y, int *out_w, int *out_h)
{
    int win_w = (g_out_w > 0) ? g_out_w : PS3_SCREEN_W;
    int win_h = (g_out_h > 0) ? g_out_h : PS3_SCREEN_H;
    int safe_w = win_w - (win_w * g_ps3_cfg.overscan_pct) / 100;
    int safe_h = win_h - (win_h * g_ps3_cfg.overscan_pct) / 100;

    int dw = safe_w;
    int dh = safe_h;

    if (g_ps3_cfg.aspect == PS3_ASPECT_4_3) {
        dw = (safe_h * 4) / 3;
        dh = safe_h;
        if (dw > safe_w) {
            dw = safe_w;
            dh = (safe_w * 3) / 4;
        }
    } else if (g_ps3_cfg.aspect == PS3_ASPECT_16_9) {
        dw = safe_w;
        dh = (safe_w * 9) / 16;
        if (dh > safe_h) {
            dh = safe_h;
            dw = (safe_h * 16) / 9;
        }
    } else if (g_ps3_cfg.aspect == PS3_ASPECT_STRETCH) {
        dw = safe_w;
        dh = safe_h;
    } else {
        if (src_w > 360 && src_w != 640) {
            dw = safe_w;
            dh = (safe_w * src_h) / src_w;
            if (dh > safe_h) {
                dh = safe_h;
                dw = (safe_h * src_w) / src_h;
            }
        } else {
            dw = (safe_h * 4) / 3;
            dh = safe_h;
            if (dw > safe_w) {
                dw = safe_w;
                dh = (safe_w * 3) / 4;
            }
        }
    }

    if (dw > safe_w) dw = safe_w;
    if (dh > safe_h) dh = safe_h;
    if (dw < 64) dw = 64;
    if (dh < 64) dh = 64;
    dw &= ~1;
    dh &= ~1;

    *out_x = ((win_w - dw) / 2) & ~1;
    *out_y = ((win_h - dh) / 2) & ~1;
    *out_w = dw;
    *out_h = dh;
}

static bool ensure_game_resources(int src_w, int src_h)
{
    int need = src_w * src_h;
    if (!g_game_argb || need > g_game_argb_cap) {
        uint32_t *new_buf = static_cast<uint32_t *>(realloc(g_game_argb, static_cast<size_t>(need) * sizeof(uint32_t)));
        if (!new_buf) return false;
        g_game_argb = new_buf;
        g_game_argb_cap = need;
    }

    if (!g_game_tex || g_game_tex_w != src_w || g_game_tex_h != src_h) {
        if (g_game_tex) {
            SDL_DestroyTexture(g_game_tex);
            g_game_tex = nullptr;
        }
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, g_ps3_cfg.linear_filter ? "1" : "0");
        g_game_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, src_w, src_h);
        if (!g_game_tex) return false;
        SDL_SetTextureBlendMode(g_game_tex, SDL_BLENDMODE_NONE);
        g_game_tex_w = src_w;
        g_game_tex_h = src_h;
        g_native_clear_frames = 4;
    }
    return true;
}

static inline uint32_t apply_scanline_mul(uint32_t c, int mul256)
{
    uint32_t rb = (((c & 0x00FF00FFu) * static_cast<uint32_t>(mul256)) >> 8) & 0x00FF00FFu;
    uint32_t g = (((c & 0x0000FF00u) * static_cast<uint32_t>(mul256)) >> 8) & 0x0000FF00u;
    return 0xFF000000u | rb | g;
}

void ps3_video_draw_game_buffer(const uint16_t *src_pixels, const uint32_t *palette_rgb, int src_w, int src_h)
{
    if (!g_ren || !src_pixels || !palette_rgb || src_w <= 0 || src_h <= 0) return;
    if (!ensure_game_resources(src_w, src_h)) return;

    int scanlines = g_ps3_cfg.scanlines;
    int scan_mul = 256;
    if (scanlines > 0) {
        scan_mul = 256 - (scanlines * 220) / 100;
        if (scan_mul < 16) scan_mul = 16;
    }

    const uint16_t *srow = src_pixels;
    uint32_t *drow = g_game_argb;

    if (scanlines <= 0) {
        int total = src_w * src_h;
        int n = 0;
        for (; n + 3 < total; n += 4) {
            drow[n + 0] = 0xFF000000u | palette_rgb[srow[n + 0]];
            drow[n + 1] = 0xFF000000u | palette_rgb[srow[n + 1]];
            drow[n + 2] = 0xFF000000u | palette_rgb[srow[n + 2]];
            drow[n + 3] = 0xFF000000u | palette_rgb[srow[n + 3]];
        }
        for (; n < total; n++) {
            drow[n] = 0xFF000000u | palette_rgb[srow[n]];
        }
    } else {
        for (int y = 0; y < src_h; y++) {
            bool dark_line = ((y & 1) != 0);
            if (!dark_line) {
                for (int x = 0; x < src_w; x++) {
                    drow[x] = 0xFF000000u | palette_rgb[srow[x]];
                }
            } else {
                for (int x = 0; x < src_w; x++) {
                    drow[x] = apply_scanline_mul(palette_rgb[srow[x]], scan_mul);
                }
            }
            srow += src_w;
            drow += src_w;
        }
    }

    draw_overlay_badges_native(g_game_argb, src_w, src_h);

    g_last_game_w = src_w;
    g_last_game_h = src_h;
    g_in_game_frame = 1;

    if (src_w != g_prev_src_w || src_h != g_prev_src_h ||
        g_ps3_cfg.aspect != g_prev_aspect || g_ps3_cfg.overscan_pct != g_prev_overscan) {
        g_prev_src_w = src_w;
        g_prev_src_h = src_h;
        g_prev_aspect = g_ps3_cfg.aspect;
        g_prev_overscan = g_ps3_cfg.overscan_pct;
        g_native_clear_frames = 4;
    }

    int dst_x = 0, dst_y = 0, dst_w = g_out_w, dst_h = g_out_h;
    compute_dest_rect(src_w, src_h, &dst_x, &dst_y, &dst_w, &dst_h);

    g_rsx_hw_linear = (g_ps3_cfg.linear_filter != 0) ? 1 : 0;

    if (g_native_clear_frames > 0) {
        SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
        SDL_RenderClear(g_ren);
        g_native_clear_frames--;
    }

    SDL_UpdateTexture(g_game_tex, nullptr, g_game_argb, src_w * static_cast<int>(sizeof(uint32_t)));
    SDL_Rect dst = { dst_x, dst_y, dst_w, dst_h };
    SDL_RenderCopy(g_ren, g_game_tex, nullptr, &dst);
    SDL_RenderPresent(g_ren);
}

bool ps3_video_save_screenshot(char *out_path, int out_sz)
{
    const uint32_t *src = nullptr;
    int sw = 0;
    int sh = 0;
    if (g_in_game_frame && g_game_argb && g_last_game_w > 0 && g_last_game_h > 0) {
        src = g_game_argb;
        sw = g_last_game_w;
        sh = g_last_game_h;
    } else if (g_fb) {
        src = g_fb;
        sw = PS3_SCREEN_W;
        sh = PS3_SCREEN_H;
    } else {
        return false;
    }

    time_t now = time(nullptr);
    struct tm *tmv = localtime(&now);
    char path[512];
    if (tmv) {
        snprintf(path, sizeof(path), "%s/cannonball_%04d%02d%02d_%02d%02d%02d.png",
                 PS3_SHOTS_DIR,
                 tmv->tm_year + 1900, tmv->tm_mon + 1, tmv->tm_mday,
                 tmv->tm_hour, tmv->tm_min, tmv->tm_sec);
    } else {
        static int seq = 1;
        snprintf(path, sizeof(path), "%s/cannonball_%04d.png", PS3_SHOTS_DIR, seq++);
    }
    bool ok = save_argb_as_png(path, src, sw, sh);
    if (ok && out_path && out_sz > 0) {
        snprintf(out_path, out_sz, "%s", path);
    }
    return ok;
}

void ps3_video_vsync_wait(void)
{
    usleep(1000);
}

static u64 g_ticks_base_ms = 0;

uint32_t ps3_ticks_ms(void)
{
    u64 sec = 0;
    u64 nsec = 0;
    if (sysGetCurrentTime(&sec, &nsec) == 0) {
        u64 now_ms = sec * 1000ULL + (nsec / 1000000ULL);
        if (g_ticks_base_ms == 0) {
            g_ticks_base_ms = now_ms;
        }
        return static_cast<uint32_t>(now_ms - g_ticks_base_ms);
    }
    return SDL_GetTicks();
}

void ps3_sleep_ms(uint32_t ms)
{
    if (ms > 0) usleep(ms * 1000);
}
