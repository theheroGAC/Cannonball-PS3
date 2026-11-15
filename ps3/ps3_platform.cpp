#include "ps3_platform.hpp"
#include "../src/main/frontend/config.hpp"
#include "../src/main/engine/ohiscore.hpp"

#include <sysutil/sysutil.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>
#include <zlib.h>

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <strings.h>
#include <unistd.h>

ps3_config_t g_ps3_cfg;
volatile int g_app_quit = 0;
volatile int g_ps3_pause_requested = 0;
char g_ps3_base_dir[512] = PS3_PKG_USRDIR;
char g_ps3_res_dir[512] = PS3_PKG_USRDIR "/res/";
char g_ps3_save_dir[512] = PS3_SAVES_DIR "/";

static FILE *g_log_fp = nullptr;
static int g_sysutil_registered = 0;

struct ps3_rom_def_t {
    const char *filename;
    const char *label;
    uint32_t expected_crc;
    uint32_t expected_size;
    int group;
};

static const ps3_rom_def_t g_rom_defs[PS3_ROM_TOTAL_COUNT] = {
    { "epr-10380b.133", "Main CPU 0 (Even Low)",  0x1f6cadadU, 0x10000U, 0 },
    { "epr-10382b.118", "Main CPU 0 (Odd Low)",   0xc4c3fa1aU, 0x10000U, 0 },
    { "epr-10381b.132", "Main CPU 0 (Even High)", 0xbe8c412bU, 0x10000U, 0 },
    { "epr-10383b.117", "Main CPU 0 (Odd High)",  0x10a2014aU, 0x10000U, 0 },
    { "epr-10327a.76",  "Sub CPU 1 (Even Low)",   0xe28a5bafU, 0x10000U, 0 },
    { "epr-10329a.58",  "Sub CPU 1 (Odd Low)",    0xda131c81U, 0x10000U, 0 },
    { "epr-10328a.75",  "Sub CPU 1 (Even High)",  0xd5ec5e5dU, 0x10000U, 0 },
    { "epr-10330a.57",  "Sub CPU 1 (Odd High)",   0xba9ec82aU, 0x10000U, 0 },
    { "opr-10268.99",   "Tile GFX Plane 0",       0x95344b04U, 0x08000U, 0 },
    { "opr-10232.102",  "Tile GFX Plane 0B",      0x776ba1ebU, 0x08000U, 0 },
    { "opr-10267.100",  "Tile GFX Plane 1",       0xa85bb823U, 0x08000U, 0 },
    { "opr-10231.103",  "Tile GFX Plane 1B",      0x8908bcbfU, 0x08000U, 0 },
    { "opr-10266.101",  "Tile GFX Plane 2",       0x9f6f1a74U, 0x08000U, 0 },
    { "opr-10230.104",  "Tile GFX Plane 2B",      0x686f5e50U, 0x08000U, 0 },
    { "opr-10185.11",   "Road GFX ROM A",         0x22794426U, 0x08000U, 0 },
    { "opr-10186.47",   "Road GFX ROM B",         0x22794426U, 0x08000U, 0 },
    { "mpr-10371.9",    "Sprites Bank 0 A",       0x7cc86208U, 0x20000U, 0 },
    { "mpr-10373.10",   "Sprites Bank 0 B",       0xb0d26ac9U, 0x20000U, 0 },
    { "mpr-10375.11",   "Sprites Bank 0 C",       0x59b60bd7U, 0x20000U, 0 },
    { "mpr-10377.12",   "Sprites Bank 0 D",       0x17a1b04aU, 0x20000U, 0 },
    { "mpr-10372.13",   "Sprites Bank 1 A",       0xb557078cU, 0x20000U, 0 },
    { "mpr-10374.14",   "Sprites Bank 1 B",       0x8051e517U, 0x20000U, 0 },
    { "mpr-10376.15",   "Sprites Bank 1 C",       0xf3b8f318U, 0x20000U, 0 },
    { "mpr-10378.16",   "Sprites Bank 1 D",       0xa1062984U, 0x20000U, 0 },
    { "epr-10187.88",   "Z80 Sound Program",      0xa10abaa9U, 0x08000U, 0 },
    { "opr-10193.66",   "Sega PCM Bank 0",        0xbcd10ddeU, 0x08000U, 0 },
    { "opr-10192.67",   "Sega PCM Bank 1",        0x770f1270U, 0x08000U, 0 },
    { "opr-10191.68",   "Sega PCM Bank 2",        0x20a284abU, 0x08000U, 0 },
    { "opr-10190.69",   "Sega PCM Bank 3",        0x7cab70e2U, 0x08000U, 0 },
    { "opr-10189.70",   "Sega PCM Bank 4",        0x01366b54U, 0x08000U, 0 },
    { "opr-10188.71",   "Sega PCM Bank 5",        0xbad30ad9U, 0x08000U, 0 },
    { "epr-10380.133",  "JAP Main CPU 0 (EL)",    0xe339e87aU, 0x10000U, 1 },
    { "epr-10382.118",  "JAP Main CPU 0 (OL)",    0x65248dd5U, 0x10000U, 1 },
    { "epr-10381.132",  "JAP Main CPU 0 (EH)",    0xbe8c412bU, 0x10000U, 1 },
    { "epr-10383.117",  "JAP Main CPU 0 (OH)",    0xdcc586e7U, 0x10000U, 1 },
    { "epr-10327.76",   "JAP Sub CPU 1 (EL)",     0xda99d855U, 0x10000U, 1 },
    { "epr-10329.58",   "JAP Sub CPU 1 (OL)",     0xfe0fa5e2U, 0x10000U, 1 },
    { "epr-10328.75",   "JAP Sub CPU 1 (EH)",     0x3c0e9a7fU, 0x10000U, 1 },
    { "epr-10330.57",   "JAP Sub CPU 1 (OH)",     0x59786e99U, 0x10000U, 1 },
    { "opr-10188.71f",  "Fixed PCM Sample Bank",  0x37598616U, 0x08000U, 2 },
    { "tilemap.bin",    "Widescreen Tilemap",     0x00000000U, 2804U,    3 },
    { "tilepatch.bin",  "Widescreen Tilepatch",   0x00000000U, 1870U,    3 }
};

struct scanned_rom_source_t {
    uint32_t crc;
    uint32_t size;
    bool is_zip;
    char container_path[512];
    char entry_name[128];
    uint32_t local_hdr_off;
    uint32_t comp_size;
    uint32_t uncomp_size;
    uint16_t method;
};

static scanned_rom_source_t g_rom_cache[256];
static int g_rom_cache_count = 0;

static void ensure_dir_recursive(const char *path)
{
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
        len--;
    }
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            mkdir(tmp, 0777);
            tmp[i] = '/';
        }
    }
    mkdir(tmp, 0777);
}

static bool file_exists_nonempty(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode) && st.st_size > 0;
}

static uint16_t rd_le16(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t rd_le32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

static const char *path_basename_c(const char *p)
{
    const char *s1 = strrchr(p, '/');
    const char *s2 = strrchr(p, '\\');
    const char *best = (s1 > s2) ? s1 : s2;
    return best ? (best + 1) : p;
}

static void str_to_lower(const char *in, char *out, size_t max_len)
{
    size_t i = 0;
    while (in[i] && i + 1 < max_len) {
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(in[i])));
        i++;
    }
    out[i] = '\0';
}

static bool ends_with_ci(const char *s, const char *suffix)
{
    size_t ls = strlen(s);
    size_t lf = strlen(suffix);
    if (lf > ls) return false;
    return strcasecmp(s + (ls - lf), suffix) == 0;
}

static uint32_t calc_file_crc32(const char *path, uint32_t *out_size)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    uint8_t buf[16384];
    uLong crc = crc32(0L, Z_NULL, 0);
    uint32_t total = 0;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        crc = crc32(crc, buf, static_cast<uInt>(n));
        total += static_cast<uint32_t>(n);
    }
    fclose(fp);
    if (out_size) *out_size = total;
    return static_cast<uint32_t>(crc);
}

static bool is_expected_rom_size(long sz)
{
    return sz == 0x08000L || sz == 0x10000L || sz == 0x20000L;
}

static bool is_expected_rom_size_u32(uint32_t sz)
{
    return sz == 0x08000U || sz == 0x10000U || sz == 0x20000U;
}

static bool starts_with_ci(const char *s, const char *prefix)
{
    size_t lp = strlen(prefix);
    return strncasecmp(s, prefix, lp) == 0;
}

static bool contains_ci(const char *hay, const char *needle)
{
    if (!hay || !needle || !needle[0]) return false;
    size_t ln = strlen(needle);
    for (const char *p = hay; *p; p++) {
        if (strncasecmp(p, needle, ln) == 0) return true;
    }
    return false;
}

static bool is_known_rom_crc(uint32_t crc)
{
    for (int i = 0; i < PS3_ROM_TOTAL_COUNT; i++) {
        if (g_rom_defs[i].group != 3 && g_rom_defs[i].expected_crc == crc)
            return true;
    }
    if (crc == 0xC2DE09B2U) return true;
    return false;
}

static bool is_ignored_non_rom_ext(const char *name)
{
    static const char *exts[] = {
        ".txt", ".xml", ".ini", ".cfg", ".png", ".jpg", ".jpeg", ".bmp",
        ".wav", ".mp3", ".ogg", ".log", ".pkg", ".self", ".sfo", ".prx",
        ".meta", ".bak", ".md", nullptr
    };
    for (int i = 0; exts[i]; i++) {
        if (ends_with_ci(name, exts[i])) return true;
    }
    return false;
}

static bool is_out_run_zip_name(const char *name)
{
    return contains_ci(name, "outrun") || contains_ci(name, "cannonball");
}

static void add_cache_entry(const scanned_rom_source_t& e)
{
    for (int i = 0; i < g_rom_cache_count; i++) {
        if (g_rom_cache[i].crc == e.crc &&
            strcasecmp(g_rom_cache[i].entry_name, e.entry_name) == 0) {
            return;
        }
    }
    if (g_rom_cache_count < 256) {
        g_rom_cache[g_rom_cache_count++] = e;
    }
}

static void index_zip_archive(const char *zip_path, long fsize)
{
    if (fsize < 22 || fsize > 16L * 1024L * 1024L) return;
    FILE *fp = fopen(zip_path, "rb");
    if (!fp) return;

    long scan_len = (fsize > 66000L) ? 66000L : fsize;
    uint8_t *tail = static_cast<uint8_t *>(malloc(static_cast<size_t>(scan_len)));
    if (!tail) {
        fclose(fp);
        return;
    }
    if (fseek(fp, fsize - scan_len, SEEK_SET) != 0 ||
        fread(tail, 1, static_cast<size_t>(scan_len), fp) != static_cast<size_t>(scan_len)) {
        free(tail);
        fclose(fp);
        return;
    }

    long eocd_off = -1;
    for (long i = scan_len - 22; i >= 0; i--) {
        if (tail[i] == 0x50 && tail[i + 1] == 0x4B && tail[i + 2] == 0x05 && tail[i + 3] == 0x06) {
            eocd_off = i;
            break;
        }
    }
    if (eocd_off < 0) {
        free(tail);
        fclose(fp);
        return;
    }

    uint16_t cd_entries = rd_le16(tail + eocd_off + 10);
    uint32_t cd_size = rd_le32(tail + eocd_off + 12);
    uint32_t cd_offset = rd_le32(tail + eocd_off + 16);
    free(tail);

    if (cd_entries == 0 || cd_entries > 512 || cd_size == 0 || cd_size > 262144U) {
        fclose(fp);
        return;
    }
    if (static_cast<long>(cd_offset) + static_cast<long>(cd_size) > fsize) {
        fclose(fp);
        return;
    }

    uint8_t *cd_buf = static_cast<uint8_t *>(malloc(static_cast<size_t>(cd_size)));
    if (!cd_buf) {
        fclose(fp);
        return;
    }
    if (fseek(fp, static_cast<long>(cd_offset), SEEK_SET) != 0 ||
        fread(cd_buf, 1, static_cast<size_t>(cd_size), fp) != static_cast<size_t>(cd_size)) {
        free(cd_buf);
        fclose(fp);
        return;
    }
    fclose(fp);

    uint32_t pos = 0;
    for (uint16_t idx = 0; idx < cd_entries && pos + 46u <= cd_size; idx++) {
        const uint8_t *cd = cd_buf + pos;
        if (rd_le32(cd) != 0x02014B50U) break;

        uint16_t method = rd_le16(cd + 10);
        uint32_t crc = rd_le32(cd + 16);
        uint32_t comp_sz = rd_le32(cd + 20);
        uint32_t uncomp_sz = rd_le32(cd + 24);
        uint16_t name_len = rd_le16(cd + 28);
        uint16_t extra_len = rd_le16(cd + 30);
        uint16_t comm_len = rd_le16(cd + 32);
        uint32_t lhoff = rd_le32(cd + 42);

        uint32_t next_pos = pos + 46u + static_cast<uint32_t>(name_len) + static_cast<uint32_t>(extra_len) + static_cast<uint32_t>(comm_len);
        if (next_pos > cd_size) break;

        if (is_expected_rom_size_u32(uncomp_sz) && (method == 0 || method == 8) && is_known_rom_crc(crc)) {
            char name[256];
            uint16_t to_read = (name_len < 255) ? name_len : 255;
            memcpy(name, cd + 46, to_read);
            name[to_read] = '\0';

            scanned_rom_source_t s;
            memset(&s, 0, sizeof(s));
            s.crc = crc;
            s.size = uncomp_sz;
            s.is_zip = true;
            snprintf(s.container_path, sizeof(s.container_path), "%s", zip_path);
            snprintf(s.entry_name, sizeof(s.entry_name), "%s", path_basename_c(name));
            s.local_hdr_off = lhoff;
            s.comp_size = comp_sz;
            s.uncomp_size = uncomp_sz;
            s.method = method;
            add_cache_entry(s);
        }
        pos = next_pos;
    }

    free(cd_buf);
}

static bool extract_zip_entry(const scanned_rom_source_t& src, uint8_t *out_buf, uint32_t expect_len)
{
    if (src.uncomp_size < expect_len) return false;
    FILE *fp = fopen(src.container_path, "rb");
    if (!fp) return false;
    if (fseek(fp, static_cast<long>(src.local_hdr_off), SEEK_SET) != 0) {
        fclose(fp);
        return false;
    }
    uint8_t lhdr[30];
    if (fread(lhdr, 1, 30, fp) != 30 || rd_le32(lhdr) != 0x04034B50U) {
        fclose(fp);
        return false;
    }
    uint16_t nlen = rd_le16(lhdr + 26);
    uint16_t elen = rd_le16(lhdr + 28);
    fseek(fp, static_cast<long>(nlen + elen), SEEK_CUR);

    uint8_t *tmp = static_cast<uint8_t *>(malloc(src.uncomp_size));
    if (!tmp) {
        fclose(fp);
        return false;
    }

    bool ok = false;
    if (src.method == 0) {
        ok = (fread(tmp, 1, src.uncomp_size, fp) == src.uncomp_size);
    } else if (src.method == 8) {
        uint8_t *cbuf = static_cast<uint8_t *>(malloc(src.comp_size));
        if (cbuf) {
            if (fread(cbuf, 1, src.comp_size, fp) == src.comp_size) {
                z_stream zs;
                memset(&zs, 0, sizeof(zs));
                if (inflateInit2(&zs, -MAX_WBITS) == Z_OK) {
                    zs.next_in = cbuf;
                    zs.avail_in = src.comp_size;
                    zs.next_out = tmp;
                    zs.avail_out = src.uncomp_size;
                    int ret = inflate(&zs, Z_FINISH);
                    if (ret == Z_STREAM_END || zs.total_out >= expect_len)
                        ok = true;
                    inflateEnd(&zs);
                }
            }
            free(cbuf);
        }
    }

    if (ok) {
        memcpy(out_buf, tmp, expect_len);
    }
    free(tmp);
    fclose(fp);
    return ok;
}

static void index_directory(const char *dir_path, bool usb_root_only)
{
    DIR *d = opendir(dir_path);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != nullptr) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        bool is_zip = ends_with_ci(de->d_name, ".zip");
        if (usb_root_only) {
            if (is_zip) {
                if (!is_out_run_zip_name(de->d_name)) continue;
            } else {
                if (!starts_with_ci(de->d_name, "epr-") &&
                    !starts_with_ci(de->d_name, "mpr-") &&
                    !starts_with_ci(de->d_name, "opr-")) {
                    continue;
                }
            }
        } else if (!is_zip && is_ignored_non_rom_ext(de->d_name)) {
            continue;
        }

        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir_path, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (!S_ISREG(st.st_mode)) continue;

        if (is_zip) {
            index_zip_archive(full, static_cast<long>(st.st_size));
            continue;
        }
        if (!is_expected_rom_size(static_cast<long>(st.st_size))) continue;

        uint32_t sz = 0;
        uint32_t crc = calc_file_crc32(full, &sz);
        if (sz > 0 && is_known_rom_crc(crc)) {
            scanned_rom_source_t s;
            memset(&s, 0, sizeof(s));
            s.crc = crc;
            s.size = sz;
            s.is_zip = false;
            snprintf(s.container_path, sizeof(s.container_path), "%s", full);
            snprintf(s.entry_name, sizeof(s.entry_name), "%s", de->d_name);
            add_cache_entry(s);
        }
    }
    closedir(d);
}

static void sysutil_callback(u64 status, u64 param, void *userdata)
{
    (void)param;
    (void)userdata;
    if (status == SYSUTIL_EXIT_GAME) {
        g_app_quit = 1;
    } else if (status == SYSUTIL_MENU_OPEN) {
        g_ps3_pause_requested = 1;
        ps3_audio_pause(1);
    }
}

void ps3_log_init(void)
{
    if (g_log_fp) return;
    ensure_dir_recursive(PS3_ROOT);
    g_log_fp = fopen(PS3_LOG_PATH, "w");
    if (g_log_fp) {
        fprintf(g_log_fp, "%s %s by %s\n", PS3_APP_NAME, PS3_APP_VERSION, PS3_AUTHOR);
        fflush(g_log_fp);
    }
}

void ps3_log(const char *fmt, ...)
{
    if (!g_log_fp) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log_fp, fmt, ap);
    va_end(ap);
    fflush(g_log_fp);
}

static void boot_trace_write(const char *path, s32 flags, const char *msg)
{
    s32 fd = -1;
    if (sysLv2FsOpen(path, flags, &fd, 0666, nullptr, 0) == 0 && fd >= 0) {
        if (flags & SYS_O_CREAT) {
            sysLv2FsChmod(path, 0666);
        }
        u64 written = 0;
        sysLv2FsWrite(fd, msg, static_cast<u64>(strlen(msg)), &written);
        sysLv2FsClose(fd);
    }
}

void ps3_boot_mark(const char *msg)
{
    if (!msg) return;
    char line[256];
    snprintf(line, sizeof(line), "[BOOT] %s\n", msg);
    boot_trace_write("/dev_hdd0/tmp/cannonball_boot.log", SYS_O_WRONLY | SYS_O_CREAT | SYS_O_APPEND, line);
    ps3_log("%s", line);
}

void ps3_paths_init(const char *argv0)
{
    ensure_dir_recursive(PS3_ROOT);
    ensure_dir_recursive(PS3_ROMS_DIR);
    ensure_dir_recursive(PS3_SAVES_DIR);
    ensure_dir_recursive(PS3_SLOTS_DIR);
    ensure_dir_recursive(PS3_MUSIC_DIR);
    ensure_dir_recursive(PS3_SHOTS_DIR);

    if (argv0 && argv0[0]) {
        const char *slash = strrchr(argv0, '/');
        if (slash && slash > argv0) {
            size_t dlen = static_cast<size_t>(slash - argv0);
            if (dlen < sizeof(g_ps3_base_dir) - 1) {
                memcpy(g_ps3_base_dir, argv0, dlen);
                g_ps3_base_dir[dlen] = '\0';
            }
        }
    }

    const char *res_candidates[] = {
        PS3_PKG_USRDIR "/res",
        "/dev_hdd0/cannonball/res",
        "./res",
        "res",
        nullptr
    };

    char dyn_res[512];
    snprintf(dyn_res, sizeof(dyn_res), "%s/res", g_ps3_base_dir);
    char check_file[512];
    snprintf(check_file, sizeof(check_file), "%s/tilemap.bin", dyn_res);
    if (file_exists_nonempty(check_file)) {
        snprintf(g_ps3_res_dir, sizeof(g_ps3_res_dir), "%s/", dyn_res);
    } else {
        for (int i = 0; res_candidates[i]; i++) {
            snprintf(check_file, sizeof(check_file), "%s/tilemap.bin", res_candidates[i]);
            if (file_exists_nonempty(check_file)) {
                snprintf(g_ps3_res_dir, sizeof(g_ps3_res_dir), "%s/", res_candidates[i]);
                break;
            }
        }
    }

    snprintf(g_ps3_save_dir, sizeof(g_ps3_save_dir), "%s/", PS3_SAVES_DIR);
    chdir(PS3_ROOT);
}

void ps3_sysutil_register(void)
{
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT1);
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_callback, nullptr);
    g_sysutil_registered = 1;
}

void ps3_sysutil_check(void)
{
    sysUtilCheckCallback();
}

void ps3_sysutil_unregister(void)
{
    if (g_sysutil_registered) {
        sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
        g_sysutil_registered = 0;
    }
    if (g_log_fp) {
        fclose(g_log_fp);
        g_log_fp = nullptr;
    }
}

void ps3_config_defaults(void)
{
    memset(&g_ps3_cfg, 0, sizeof(g_ps3_cfg));
    g_ps3_cfg.aspect = PS3_ASPECT_AUTO;
    g_ps3_cfg.linear_filter = 0;
    g_ps3_cfg.scanlines = 0;
    g_ps3_cfg.overscan_pct = 0;
    g_ps3_cfg.show_fps = 0;
    g_ps3_cfg.rumble_pct = 100;
    g_ps3_cfg.active_slot = 0;
    g_ps3_cfg.splash_enabled = 1;
    g_ps3_cfg.volume_pct = 100;
    g_ps3_cfg.control_preset = 0;
}

void ps3_config_load(void)
{
    ps3_config_defaults();
    FILE *fp = fopen(PS3_SETTINGS_PATH, "r");
    if (!fp) return;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char k[64];
        int v = 0;
        if (sscanf(line, "%63[^=]=%d", k, &v) == 2) {
            if (strcmp(k, "aspect") == 0 && v >= 0 && v < PS3_ASPECT_COUNT) g_ps3_cfg.aspect = v;
            else if (strcmp(k, "linear_filter") == 0) g_ps3_cfg.linear_filter = v ? 1 : 0;
            else if (strcmp(k, "scanlines") == 0 && v >= 0 && v <= 100) g_ps3_cfg.scanlines = v;
            else if (strcmp(k, "overscan_pct") == 0 && v >= 0 && v <= 8) g_ps3_cfg.overscan_pct = v;
            else if (strcmp(k, "show_fps") == 0) g_ps3_cfg.show_fps = v ? 1 : 0;
            else if (strcmp(k, "rumble_pct") == 0 && v >= 0 && v <= 100) g_ps3_cfg.rumble_pct = v;
            else if (strcmp(k, "active_slot") == 0 && v >= 0 && v <= 3) g_ps3_cfg.active_slot = v;
            else if (strcmp(k, "splash_enabled") == 0) g_ps3_cfg.splash_enabled = v ? 1 : 0;
            else if (strcmp(k, "volume_pct") == 0 && v >= 0 && v <= 100) g_ps3_cfg.volume_pct = v;
            else if (strcmp(k, "control_preset") == 0 && v >= 0 && v <= 2) g_ps3_cfg.control_preset = v;
        }
    }
    fclose(fp);
}

void ps3_config_save(void)
{
    ensure_dir_recursive(PS3_SAVES_DIR);
    FILE *fp = fopen(PS3_SETTINGS_PATH, "w");
    if (!fp) return;
    fprintf(fp, "aspect=%d\n", g_ps3_cfg.aspect);
    fprintf(fp, "linear_filter=%d\n", g_ps3_cfg.linear_filter);
    fprintf(fp, "scanlines=%d\n", g_ps3_cfg.scanlines);
    fprintf(fp, "overscan_pct=%d\n", g_ps3_cfg.overscan_pct);
    fprintf(fp, "show_fps=%d\n", g_ps3_cfg.show_fps);
    fprintf(fp, "rumble_pct=%d\n", g_ps3_cfg.rumble_pct);
    fprintf(fp, "active_slot=%d\n", g_ps3_cfg.active_slot);
    fprintf(fp, "splash_enabled=%d\n", g_ps3_cfg.splash_enabled);
    fprintf(fp, "volume_pct=%d\n", g_ps3_cfg.volume_pct);
    fprintf(fp, "control_preset=%d\n", g_ps3_cfg.control_preset);
    fclose(fp);
}

struct rom_dir_candidate_t {
    char path[512];
    bool usb_root_only;
};

static void add_rom_dir_unique(rom_dir_candidate_t *list, int *count, int max_count, const char *raw_dir, bool usb_root_only)
{
    if (!raw_dir || !raw_dir[0] || *count >= max_count) return;
    char clean[512];
    snprintf(clean, sizeof(clean), "%s", raw_dir);
    size_t len = strlen(clean);
    while (len > 1 && (clean[len - 1] == '/' || clean[len - 1] == '\\')) {
        clean[len - 1] = '\0';
        len--;
    }
    if (len == 0) return;
    for (int i = 0; i < *count; i++) {
        if (strcasecmp(list[i].path, clean) == 0) return;
    }
    snprintf(list[*count].path, sizeof(list[*count].path), "%s", clean);
    list[*count].usb_root_only = usb_root_only;
    (*count)++;
}

static bool all_known_roms_cached(void)
{
    for (int i = 0; i < PS3_ROM_TOTAL_COUNT; i++) {
        if (g_rom_defs[i].group == 3) continue;
        const ps3_rom_def_t& def = g_rom_defs[i];
        bool ok = false;
        for (int c = 0; c < g_rom_cache_count; c++) {
            if (g_rom_cache[c].crc == def.expected_crc &&
                strcasecmp(g_rom_cache[c].entry_name, def.filename) == 0) {
                ok = true;
                break;
            }
        }
        if (!ok && def.group == 2 && strcmp(def.filename, "opr-10188.71f") == 0) {
            for (int c = 0; c < g_rom_cache_count; c++) {
                if (g_rom_cache[c].crc == def.expected_crc || g_rom_cache[c].crc == 0xC2DE09B2U) {
                    ok = true;
                    break;
                }
            }
        }
        if (!ok) {
            for (int c = 0; c < g_rom_cache_count; c++) {
                if (g_rom_cache[c].crc == def.expected_crc) {
                    ok = true;
                    break;
                }
            }
        }
        if (!ok) return false;
    }
    return true;
}

static void refresh_rom_cache(void)
{
    g_rom_cache_count = 0;
    rom_dir_candidate_t dirs[36];
    int count = 0;

    char tmp[512];
    if (!config.data.rom_path.empty()) {
        add_rom_dir_unique(dirs, &count, 36, config.data.rom_path.c_str(), false);
    }
    add_rom_dir_unique(dirs, &count, 36, PS3_ROMS_DIR, false);
    add_rom_dir_unique(dirs, &count, 36, PS3_ROMS_DIR "/outrun", false);
    add_rom_dir_unique(dirs, &count, 36, "/dev_hdd0/data/cannonball/roms", false);
    add_rom_dir_unique(dirs, &count, 36, PS3_PKG_USRDIR "/roms", false);
    add_rom_dir_unique(dirs, &count, 36, PS3_PKG_USRDIR "/roms/outrun", false);
    add_rom_dir_unique(dirs, &count, 36, PS3_PKG_USRDIR "/data/cannonball/roms", false);

    snprintf(tmp, sizeof(tmp), "%s/roms", g_ps3_base_dir);
    add_rom_dir_unique(dirs, &count, 36, tmp, false);
    snprintf(tmp, sizeof(tmp), "%s/roms/outrun", g_ps3_base_dir);
    add_rom_dir_unique(dirs, &count, 36, tmp, false);
    snprintf(tmp, sizeof(tmp), "%s/data/cannonball/roms", g_ps3_base_dir);
    add_rom_dir_unique(dirs, &count, 36, tmp, false);

    add_rom_dir_unique(dirs, &count, 36, "/dev_hdd0/ROMS/cannonball", false);
    add_rom_dir_unique(dirs, &count, 36, "/dev_hdd0/ROMS/outrun", false);
    add_rom_dir_unique(dirs, &count, 36, "/dev_hdd0/roms/cannonball", false);
    add_rom_dir_unique(dirs, &count, 36, "/dev_hdd0/roms/outrun", false);

    static const char *usb_subdirs[] = {
        "/dev_usb000/cannonball/roms",
        "/dev_usb000/data/cannonball/roms",
        "/dev_usb000/roms/outrun",
        "/dev_usb000/outrun",
        "/dev_usb000/roms",
        "/dev_usb001/cannonball/roms",
        "/dev_usb001/data/cannonball/roms",
        "/dev_usb001/roms/outrun",
        "/dev_usb001/outrun",
        "/dev_usb001/roms",
        "/dev_usb002/cannonball/roms",
        "/dev_usb002/data/cannonball/roms",
        "/dev_usb002/roms",
        nullptr
    };
    for (int i = 0; usb_subdirs[i]; i++) {
        add_rom_dir_unique(dirs, &count, 36, usb_subdirs[i], false);
    }

    add_rom_dir_unique(dirs, &count, 36, "/dev_usb000", true);
    add_rom_dir_unique(dirs, &count, 36, "/dev_usb001", true);
    add_rom_dir_unique(dirs, &count, 36, "/dev_usb002", true);

    for (int i = 0; i < count; i++) {
        index_directory(dirs[i].path, dirs[i].usb_root_only);
        if (all_known_roms_cached()) {
            break;
        }
    }
}

void ps3_roms_scan(ps3_rom_report_t *rep)
{
    memset(rep, 0, sizeof(*rep));
    rep->count = PS3_ROM_TOTAL_COUNT;
    snprintf(rep->detected_res_path, sizeof(rep->detected_res_path), "%s", g_ps3_res_dir);

    refresh_rom_cache();

    for (int i = 0; i < PS3_ROM_TOTAL_COUNT; i++) {
        const ps3_rom_def_t& def = g_rom_defs[i];
        ps3_rom_entry_status_t& st = rep->items[i];
        st.filename = def.filename;
        st.label = def.label;
        st.expected_crc = def.expected_crc;
        st.expected_size = def.expected_size;
        st.group = def.group;
        st.found = 0;
        st.source_desc[0] = '\0';

        if (def.group == 3) {
            char rpath[512];
            snprintf(rpath, sizeof(rpath), "%s%s", g_ps3_res_dir, def.filename);
            if (file_exists_nonempty(rpath)) {
                st.found = 1;
                snprintf(st.source_desc, sizeof(st.source_desc), "%s", rpath);
                rep->res_found++;
            }
            continue;
        }

        int match_idx = -1;
        for (int c = 0; c < g_rom_cache_count; c++) {
            if (g_rom_cache[c].crc == def.expected_crc &&
                strcasecmp(g_rom_cache[c].entry_name, def.filename) == 0) {
                match_idx = c;
                break;
            }
        }
        if (match_idx < 0) {
            for (int c = 0; c < g_rom_cache_count; c++) {
                if (g_rom_cache[c].crc == def.expected_crc) {
                    match_idx = c;
                    break;
                }
            }
        }
        if (match_idx < 0 && strcmp(def.filename, "opr-10188.71f") == 0) {
            for (int c = 0; c < g_rom_cache_count; c++) {
                if (g_rom_cache[c].crc == 0xC2DE09B2U) {
                    match_idx = c;
                    break;
                }
            }
        }

        if (match_idx >= 0) {
            st.found = 1;
            const scanned_rom_source_t& src = g_rom_cache[match_idx];
            if (src.is_zip) {
                snprintf(st.source_desc, sizeof(st.source_desc), "ZIP: %s (%s)",
                         path_basename_c(src.container_path), src.entry_name);
            } else {
                snprintf(st.source_desc, sizeof(st.source_desc), "%s", src.container_path);
            }
            if (!rep->detected_rom_path[0]) {
                char dir_buf[512];
                snprintf(dir_buf, sizeof(dir_buf), "%s", src.container_path);
                char *sl = strrchr(dir_buf, '/');
                if (sl && sl > dir_buf) {
                    sl[1] = '\0';
                    snprintf(rep->detected_rom_path, sizeof(rep->detected_rom_path), "%s", dir_buf);
                } else {
                    snprintf(rep->detected_rom_path, sizeof(rep->detected_rom_path), "%s/", PS3_ROMS_DIR);
                }
            }
            if (def.group == 0) rep->revb_found++;
            else if (def.group == 1) rep->jap_found++;
            else if (def.group == 2) rep->pcm_found++;
        }
    }

    rep->all_required_ok = (rep->revb_found == PS3_ROM_REVB_COUNT && rep->res_found == PS3_RES_BIN_COUNT) ? 1 : 0;
}

bool ps3_roms_load_by_crc(uint32_t expected_crc, const char *fallback_name, int length, uint8_t *dst, int interleave, int offset)
{
    if (g_rom_cache_count == 0) {
        refresh_rom_cache();
    }

    int idx = -1;
    if (fallback_name && fallback_name[0]) {
        for (int c = 0; c < g_rom_cache_count; c++) {
            if (g_rom_cache[c].crc == expected_crc &&
                strcasecmp(g_rom_cache[c].entry_name, fallback_name) == 0) {
                idx = c;
                break;
            }
        }
    }
    if (idx < 0) {
        for (int c = 0; c < g_rom_cache_count; c++) {
            if (g_rom_cache[c].crc == expected_crc) {
                idx = c;
                break;
            }
        }
    }
    if (idx < 0) return false;

    const scanned_rom_source_t& src = g_rom_cache[idx];
    uint8_t *buf = static_cast<uint8_t *>(malloc(static_cast<size_t>(length)));
    if (!buf) return false;

    bool ok = false;
    if (src.is_zip) {
        ok = extract_zip_entry(src, buf, static_cast<uint32_t>(length));
    } else {
        FILE *fp = fopen(src.container_path, "rb");
        if (fp) {
            ok = (fread(buf, 1, static_cast<size_t>(length), fp) == static_cast<size_t>(length));
            fclose(fp);
        }
    }

    if (ok) {
        for (int i = 0; i < length; i++) {
            dst[(i * interleave) + offset] = buf[i];
        }
    }
    free(buf);
    return ok;
}

bool ps3_roms_find_best_path(char *out_path, int out_sz)
{
    ps3_rom_report_t rep;
    ps3_roms_scan(&rep);
    if (rep.detected_rom_path[0]) {
        snprintf(out_path, out_sz, "%s", rep.detected_rom_path);
        return true;
    }
    snprintf(out_path, out_sz, "%s/", PS3_ROMS_DIR);
    return false;
}

bool ps3_copy_file(const char *src, const char *dst)
{
    FILE *fi = fopen(src, "rb");
    if (!fi) return false;
    FILE *fo = fopen(dst, "wb");
    if (!fo) {
        fclose(fi);
        return false;
    }
    uint8_t buf[32768];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), fi)) > 0) {
        if (fwrite(buf, 1, n, fo) != n) {
            ok = false;
            break;
        }
    }
    fclose(fo);
    fclose(fi);
    return ok;
}

int ps3_roms_import_from_usb(char *summary, int summary_sz)
{
    ensure_dir_recursive(PS3_ROMS_DIR);
    int copied = 0;
    int zips = 0;

    static const char *usb_dirs[] = {
        "/dev_usb000/cannonball/roms",
        "/dev_usb000/data/cannonball/roms",
        "/dev_usb000/roms/outrun",
        "/dev_usb000/outrun",
        "/dev_usb000/roms",
        "/dev_usb000",
        "/dev_usb001/cannonball/roms",
        "/dev_usb001/data/cannonball/roms",
        "/dev_usb001/roms/outrun",
        "/dev_usb001/outrun",
        "/dev_usb001/roms",
        "/dev_usb001",
        nullptr
    };

    for (int d = 0; usb_dirs[d]; d++) {
        bool is_root = (strcmp(usb_dirs[d], "/dev_usb000") == 0 || strcmp(usb_dirs[d], "/dev_usb001") == 0);
        DIR *dp = opendir(usb_dirs[d]);
        if (!dp) continue;
        struct dirent *de;
        while ((de = readdir(dp)) != nullptr) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;

            bool is_zip = ends_with_ci(de->d_name, ".zip");
            if (is_zip) {
                if (!is_out_run_zip_name(de->d_name)) continue;
            } else if (is_root) {
                if (!starts_with_ci(de->d_name, "epr-") &&
                    !starts_with_ci(de->d_name, "mpr-") &&
                    !starts_with_ci(de->d_name, "opr-")) {
                    continue;
                }
            } else if (is_ignored_non_rom_ext(de->d_name)) {
                continue;
            }

            char src[512];
            snprintf(src, sizeof(src), "%s/%s", usb_dirs[d], de->d_name);
            struct stat st;
            if (stat(src, &st) != 0 || !S_ISREG(st.st_mode)) continue;

            if (is_zip) {
                if (st.st_size >= 22 && st.st_size <= 16L * 1024L * 1024L) {
                    char low[128];
                    str_to_lower(de->d_name, low, sizeof(low));
                    char dst[512];
                    snprintf(dst, sizeof(dst), "%s/%s", PS3_ROMS_DIR, low);
                    if (ps3_copy_file(src, dst)) {
                        zips++;
                        copied++;
                    }
                }
                continue;
            }

            if (is_expected_rom_size(static_cast<long>(st.st_size))) {
                uint32_t sz = 0;
                uint32_t crc = calc_file_crc32(src, &sz);
                if (is_known_rom_crc(crc)) {
                    char low[128];
                    str_to_lower(de->d_name, low, sizeof(low));
                    char dst[512];
                    snprintf(dst, sizeof(dst), "%s/%s", PS3_ROMS_DIR, low);
                    if (ps3_copy_file(src, dst))
                        copied++;
                }
            }
        }
        closedir(dp);
    }

    if (summary && summary_sz > 0) {
        if (copied == 0)
            snprintf(summary, summary_sz, "No OutRun ROMs or outrun.zip found on /dev_usb000 or /dev_usb001.");
        else
            snprintf(summary, summary_sz, "Imported %d file(s) (%d zip) to /dev_hdd0/cannonball/roms/", copied, zips);
    }
    return copied;
}

static const char *g_save_files[] = {
    "config.xml",
    "ps3_settings.ini",
    "hiscores.xml",
    "hiscores_jap.xml",
    "hiscores_timetrial.xml",
    "hiscores_timetrial_jap.xml",
    "hiscores_continuous.xml",
    "hiscores_continuous_jap.xml",
    nullptr
};

void ps3_saves_get_slot_info(int slot, ps3_slot_info_t *info)
{
    memset(info, 0, sizeof(*info));
    if (slot < 1 || slot > 3) return;
    char meta[512];
    snprintf(meta, sizeof(meta), "%s/slot%d/slot.meta", PS3_SLOTS_DIR, slot);
    FILE *fp = fopen(meta, "r");
    if (!fp) return;
    info->exists = 1;
    snprintf(info->top_name, sizeof(info->top_name), "OUT");
    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        if (strncmp(line, "time=", 5) == 0) {
            snprintf(info->timestamp, sizeof(info->timestamp), "%s", line + 5);
        } else if (strncmp(line, "top=", 4) == 0) {
            info->top_score = static_cast<uint32_t>(strtoul(line + 4, nullptr, 10));
        } else if (strncmp(line, "name=", 5) == 0) {
            snprintf(info->top_name, sizeof(info->top_name), "%s", line + 5);
        }
    }
    fclose(fp);
}

bool ps3_saves_write_slot(int slot)
{
    if (slot < 1 || slot > 3) return false;
    char sdir[512];
    snprintf(sdir, sizeof(sdir), "%s/slot%d", PS3_SLOTS_DIR, slot);
    ensure_dir_recursive(sdir);

    for (int i = 0; g_save_files[i]; i++) {
        char src[512], dst[512];
        snprintf(src, sizeof(src), "%s/%s", PS3_SAVES_DIR, g_save_files[i]);
        snprintf(dst, sizeof(dst), "%s/%s", sdir, g_save_files[i]);
        if (file_exists_nonempty(src)) {
            ps3_copy_file(src, dst);
        }
    }

    char meta[512];
    snprintf(meta, sizeof(meta), "%s/slot.meta", sdir);
    FILE *fp = fopen(meta, "w");
    if (!fp) return false;

    time_t now = time(nullptr);
    struct tm *tmv = localtime(&now);
    char tbuf[32] = "SAVED";
    if (tmv) {
        snprintf(tbuf, sizeof(tbuf), "%04d-%02d-%02d %02d:%02d",
                 tmv->tm_year + 1900, tmv->tm_mon + 1, tmv->tm_mday,
                 tmv->tm_hour, tmv->tm_min);
    }

    char nbuf[4] = {
        static_cast<char>(ohiscore.scores[0].initial1 >= 0x20 ? ohiscore.scores[0].initial1 : '.'),
        static_cast<char>(ohiscore.scores[0].initial2 >= 0x20 ? ohiscore.scores[0].initial2 : '.'),
        static_cast<char>(ohiscore.scores[0].initial3 >= 0x20 ? ohiscore.scores[0].initial3 : '.'),
        '\0'
    };

    fprintf(fp, "time=%s\n", tbuf);
    fprintf(fp, "top=%u\n", static_cast<unsigned int>(ohiscore.scores[0].score));
    fprintf(fp, "name=%s\n", nbuf);
    fclose(fp);
    return true;
}

bool ps3_saves_load_slot(int slot)
{
    if (slot < 1 || slot > 3) return false;
    char sdir[512];
    snprintf(sdir, sizeof(sdir), "%s/slot%d", PS3_SLOTS_DIR, slot);
    char meta[512];
    snprintf(meta, sizeof(meta), "%s/slot.meta", sdir);
    if (!file_exists_nonempty(meta)) return false;

    for (int i = 0; g_save_files[i]; i++) {
        char src[512], dst[512];
        snprintf(src, sizeof(src), "%s/%s", sdir, g_save_files[i]);
        snprintf(dst, sizeof(dst), "%s/%s", PS3_SAVES_DIR, g_save_files[i]);
        if (file_exists_nonempty(src)) {
            ps3_copy_file(src, dst);
        }
    }

    ps3_config_load();
    g_ps3_cfg.active_slot = slot;
    ps3_config_save();

    config.load();
    config.load_scores(true);
    config.load_tiletrial_scores();
    return true;
}

bool ps3_saves_sync_all(bool auto_mirror_slot)
{
    ps3_config_save();
    bool ok = config.save();
    config.save_scores(true);
    config.save_tiletrial_scores();
    if (auto_mirror_slot && g_ps3_cfg.active_slot >= 1 && g_ps3_cfg.active_slot <= 3) {
        ps3_saves_write_slot(g_ps3_cfg.active_slot);
    }
    return ok;
}
