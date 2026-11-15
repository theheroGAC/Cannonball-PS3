#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#define PS3_APP_NAME "Cannonball PS3"
#define PS3_APP_VERSION "0.35-PS3"
#define PS3_TITLE_ID "CNBLPS300"
#define PS3_AUTHOR "theheroGAC"

#define PS3_ROOT "/dev_hdd0/cannonball"
#define PS3_ROMS_DIR PS3_ROOT "/roms"
#define PS3_SAVES_DIR PS3_ROOT "/saves"
#define PS3_SLOTS_DIR PS3_SAVES_DIR "/slots"
#define PS3_MUSIC_DIR PS3_ROOT "/music"
#define PS3_SHOTS_DIR PS3_ROOT "/screenshots"
#define PS3_LOG_PATH PS3_ROOT "/cannonball_ps3.log"
#define PS3_SETTINGS_PATH PS3_SAVES_DIR "/ps3_settings.ini"
#define PS3_PKG_USRDIR "/dev_hdd0/game/" PS3_TITLE_ID "/USRDIR"

#define PS3_SCREEN_W 1280
#define PS3_SCREEN_H 720
#define PS3_AUDIO_RATE 48000

#define PS3_ACT_UP (1u << 0)
#define PS3_ACT_DOWN (1u << 1)
#define PS3_ACT_LEFT (1u << 2)
#define PS3_ACT_RIGHT (1u << 3)
#define PS3_ACT_CROSS (1u << 4)
#define PS3_ACT_CIRCLE (1u << 5)
#define PS3_ACT_SQUARE (1u << 6)
#define PS3_ACT_TRIANGLE (1u << 7)
#define PS3_ACT_L1 (1u << 8)
#define PS3_ACT_R1 (1u << 9)
#define PS3_ACT_L2 (1u << 10)
#define PS3_ACT_R2 (1u << 11)
#define PS3_ACT_START (1u << 12)
#define PS3_ACT_SELECT (1u << 13)
#define PS3_ACT_L3 (1u << 14)
#define PS3_ACT_R3 (1u << 15)

enum {
    PS3_ASPECT_AUTO = 0,
    PS3_ASPECT_16_9,
    PS3_ASPECT_4_3,
    PS3_ASPECT_STRETCH,
    PS3_ASPECT_COUNT
};

enum {
    PS3_LAUNCH_NONE = 0,
    PS3_LAUNCH_ENHANCED,
    PS3_LAUNCH_ORIGINAL,
    PS3_LAUNCH_CONT,
    PS3_LAUNCH_TTRIAL,
    PS3_LAUNCH_CLASSIC_MENU,
    PS3_LAUNCH_EXIT
};

enum {
    PS3_PAUSE_RESUME = 0,
    PS3_PAUSE_RESTART,
    PS3_PAUSE_MAIN_UI,
    PS3_PAUSE_CLASSIC_MENU,
    PS3_PAUSE_EXIT
};

struct ps3_config_t {
    int aspect;
    int linear_filter;
    int scanlines;
    int overscan_pct;
    int show_fps;
    int rumble_pct;
    int active_slot;
    int splash_enabled;
    int volume_pct;
    int control_preset;
};

#define PS3_ROM_REVB_COUNT 31
#define PS3_ROM_JAP_COUNT 8
#define PS3_ROM_PCM_COUNT 1
#define PS3_RES_BIN_COUNT 2
#define PS3_ROM_TOTAL_COUNT (PS3_ROM_REVB_COUNT + PS3_ROM_JAP_COUNT + PS3_ROM_PCM_COUNT + PS3_RES_BIN_COUNT)

struct ps3_rom_entry_status_t {
    const char *filename;
    const char *label;
    uint32_t expected_crc;
    uint32_t expected_size;
    int group;
    int found;
    char source_desc[160];
};

struct ps3_rom_report_t {
    ps3_rom_entry_status_t items[PS3_ROM_TOTAL_COUNT];
    int count;
    int revb_found;
    int jap_found;
    int pcm_found;
    int res_found;
    int all_required_ok;
    char detected_rom_path[512];
    char detected_res_path[512];
};

struct ps3_slot_info_t {
    int exists;
    char timestamp[32];
    uint32_t top_score;
    char top_name[8];
    int mode_tag;
};

extern ps3_config_t g_ps3_cfg;
extern volatile int g_app_quit;
extern volatile int g_ps3_pause_requested;
extern char g_ps3_base_dir[512];
extern char g_ps3_res_dir[512];
extern char g_ps3_save_dir[512];

void ps3_log_init(void);
void ps3_log(const char *fmt, ...);
void ps3_boot_mark(const char *msg);
void ps3_paths_init(const char *argv0);
void ps3_sysutil_register(void);
void ps3_sysutil_check(void);
void ps3_sysutil_unregister(void);

void ps3_config_defaults(void);
void ps3_config_load(void);
void ps3_config_save(void);

void ps3_roms_scan(ps3_rom_report_t *rep);
bool ps3_roms_load_by_crc(uint32_t expected_crc, const char *fallback_name, int length, uint8_t *dst, int interleave, int offset);
bool ps3_roms_find_best_path(char *out_path, int out_sz);
int ps3_roms_import_from_usb(char *summary, int summary_sz);
bool ps3_copy_file(const char *src, const char *dst);

void ps3_saves_get_slot_info(int slot, ps3_slot_info_t *info);
bool ps3_saves_write_slot(int slot);
bool ps3_saves_load_slot(int slot);
bool ps3_saves_sync_all(bool auto_mirror_slot);

int ps3_video_init(void);
void ps3_video_shutdown(void);
void ps3_video_present(void);
uint32_t *ps3_video_fb(void);
void ps3_video_clear(uint32_t argb);
void ps3_video_apply_filter(void);
void ps3_video_draw_game_buffer(const uint16_t *src_pixels, const uint32_t *palette_rgb, int src_w, int src_h);
void ps3_video_toast(const char *msg, int frames);
void ps3_video_set_fps(int fps);
bool ps3_video_save_screenshot(char *out_path, int out_sz);
void ps3_video_vsync_wait(void);
uint32_t ps3_ticks_ms(void);
void ps3_sleep_ms(uint32_t ms);

int ps3_audio_init(void);
void ps3_audio_shutdown(void);
void ps3_audio_pause(int paused);
void ps3_audio_tick_frame(void);
void ps3_audio_push_frames(const int16_t *stereo_frames, uint32_t frame_count);
uint32_t ps3_audio_buffered_frames(void);
void ps3_audio_clear_ring(void);

void ps3_input_init(void);
void ps3_input_shutdown(void);
void ps3_input_poll(void);
uint32_t ps3_input_buttons(void);
uint32_t ps3_input_pressed(void);
void ps3_input_get_analog(int *lx, int *ly, int *rx, int *ry, int *l2, int *r2);
void ps3_input_set_rumble(float strength);

void ps3_ui_init(void);
void ps3_ui_shutdown(void);
void ps3_ui_show_splash(void);
int ps3_ui_missing_data_dialog(ps3_rom_report_t *rep);
int ps3_ui_main_menu(ps3_rom_report_t *rep);
int ps3_ui_pause_menu(void);
