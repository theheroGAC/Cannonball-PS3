#include <sys/process.h>
#include <sys/stat.h>
#include <sys/systime.h>
#include <cstring>
#include <cstdio>
#include <SDL.h>

#include "sdl2/timer.hpp"
#include "sdl2/input.hpp"
#include "video.hpp"
#include "romloader.hpp"
#include "trackloader.hpp"
#include "stdint.hpp"
#include "main.hpp"
#include "engine/outrun.hpp"
#include "frontend/config.hpp"
#include "frontend/menu.hpp"
#include "engine/oinputs.hpp"
#include "engine/ooutputs.hpp"
#include "engine/omusic.hpp"
#include "directx/ffeedback.hpp"
#include "../../ps3/ps3_platform.hpp"

SYS_PROCESS_PARAM(1001, 0x200000);

using namespace cannonball;

int    cannonball::state       = STATE_BOOT;
double cannonball::frame_ms    = 0;
int    cannonball::frame       = 0;
bool   cannonball::tick_frame  = true;
int    cannonball::fps_counter = 0;

Audio cannonball::audio;
Menu* menu = nullptr;
bool pause_engine = false;

static bool file_ok(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

static void shutdown_all(void)
{
    ps3_input_set_rumble(0.0f);
    audio.stop_audio();
    ps3_audio_shutdown();
    input.close_joy();
    forcefeedback::close();
    if (menu)
    {
        delete menu;
        menu = nullptr;
    }
    ps3_ui_shutdown();
    ps3_video_shutdown();
    ps3_input_shutdown();
    ps3_sysutil_unregister();
    SDL_Quit();
}

static void tick_engine_frame()
{
    frame++;

    if (config.fps == 60)
        tick_frame = (frame & 1) != 0;
    else if (config.fps == 120)
        tick_frame = (frame & 3) == 1;
    else
        tick_frame = true;

    ps3_sysutil_check();
    if (g_app_quit)
    {
        state = STATE_QUIT;
        return;
    }

    if (tick_frame)
    {
        oinputs.tick();
        oinputs.do_gear();
    }

    switch (state)
    {
        case STATE_GAME:
        {
            if (tick_frame)
            {
                if (input.has_pressed(Input::TIMER)) outrun.freeze_timer = !outrun.freeze_timer;
                if (input.has_pressed(Input::PAUSE)) pause_engine = !pause_engine;
                if (input.has_pressed(Input::MENU))  state = STATE_INIT_MENU;
            }

            if (!pause_engine || input.has_pressed(Input::STEP))
            {
                outrun.tick(tick_frame);
                if (tick_frame) input.frame_done();
                osoundint.tick();
            }
            else
            {
                if (tick_frame) input.frame_done();
            }
        }
        break;

        case STATE_INIT_GAME:
            if (config.engine.jap && !roms.load_japanese_roms())
            {
                ps3_video_toast("Japanese ROMset Not Found!", 120);
                state = STATE_INIT_MENU;
            }
            else
            {
                tick_frame = true;
                pause_engine = false;
                outrun.init();
                state = STATE_GAME;
            }
            break;

        case STATE_MENU:
            menu->tick();
            input.frame_done();
            osoundint.tick();
            break;

        case STATE_INIT_MENU:
            ps3_saves_sync_all(true);
            oinputs.init();
            outrun.outputs->init();
            menu->init();
            state = STATE_MENU;
            break;
    }

    outrun.outputs->writeDigitalToConsole();
    if (tick_frame)
    {
        bool do_rumble = outrun.outputs->is_set(OOutputs::D_MOTOR) && (g_ps3_cfg.rumble_pct > 0);
        input.set_rumble(do_rumble, static_cast<float>(g_ps3_cfg.rumble_pct) / 100.0f);
    }
}

static uint64_t ps3_us_now()
{
    u64 sec = 0;
    u64 nsec = 0;
    if (sysGetCurrentTime(&sec, &nsec) == 0) {
        return static_cast<uint64_t>(sec) * 1000000ULL + static_cast<uint64_t>(nsec / 1000ULL);
    }
    return static_cast<uint64_t>(ps3_ticks_ms()) * 1000ULL;
}

static void run_session_loop()
{
    Timer fps_count;
    int frame_cnt = 0;
    fps_count.start();

    Timer frame_time;
    double deltatime = 0;
    int deltaintegral = 0;

    uint64_t acc_tick_us = 0;
    uint64_t acc_prep_us = 0;
    uint64_t acc_rend_us = 0;
    uint64_t acc_aud_us = 0;
    uint32_t perf_frames = 0;
    Timer perf_timer;
    perf_timer.start();

    g_ps3_pause_requested = 0;
    input.frame_done();
    ps3_video_set_fps(0);

    while (state != STATE_QUIT && !g_app_quit)
    {
        frame_time.start();

        uint64_t t0 = ps3_us_now();
        tick_engine_frame();
        uint64_t t1 = ps3_us_now();
        acc_tick_us += (t1 - t0);

        if (state == STATE_QUIT || g_app_quit)
            break;

        if (g_ps3_pause_requested)
        {
            g_ps3_pause_requested = 0;
            ps3_input_set_rumble(0.0f);
            int p_act = ps3_ui_pause_menu();
            input.frame_done();
            deltatime = 0;

            if (g_app_quit || p_act == PS3_PAUSE_EXIT)
            {
                g_app_quit = 1;
                state = STATE_QUIT;
                break;
            }
            else if (p_act == PS3_PAUSE_MAIN_UI)
            {
                ps3_saves_sync_all(true);
                state = STATE_QUIT;
                break;
            }
            else if (p_act == PS3_PAUSE_CLASSIC_MENU)
            {
                state = STATE_INIT_MENU;
                continue;
            }
            else if (p_act == PS3_PAUSE_RESTART)
            {
                state = STATE_INIT_GAME;
                continue;
            }
            continue;
        }

        uint64_t t2 = ps3_us_now();
        video.prepare_frame();
        uint64_t t3 = ps3_us_now();
        video.render_frame();
        uint64_t t4 = ps3_us_now();
        audio.tick();
        uint64_t t5 = ps3_us_now();

        acc_prep_us += (t3 - t2);
        acc_rend_us += (t4 - t3);
        acc_aud_us  += (t5 - t4);

        deltatime += frame_ms;
        deltaintegral = static_cast<int>(deltatime);
        int t = frame_time.get_ticks();
        if (t < deltatime)
        {
            uint32_t wait_ms = static_cast<uint32_t>(deltatime - t);
            if (wait_ms >= 3) ps3_sleep_ms(wait_ms - 1);
            deltatime -= deltaintegral;
        }
        else
        {
            deltatime = 0;
        }

        frame_cnt++;
        perf_frames++;
        if (fps_count.get_ticks() >= 1000)
        {
            fps_counter = frame_cnt;
            ps3_video_set_fps(fps_counter);
            frame_cnt = 0;
            fps_count.start();
        }
        if (perf_timer.get_ticks() >= 2000 && perf_frames > 0)
        {
            uint32_t tick10 = static_cast<uint32_t>(acc_tick_us / (perf_frames * 100ULL));
            uint32_t prep10 = static_cast<uint32_t>(acc_prep_us / (perf_frames * 100ULL));
            uint32_t rend10 = static_cast<uint32_t>(acc_rend_us / (perf_frames * 100ULL));
            uint32_t aud10  = static_cast<uint32_t>(acc_aud_us  / (perf_frames * 100ULL));
            ps3_log("perf: fps=%d w=%d tick=%u.%u prep=%u.%u rend=%u.%u aud=%u.%u ms buf=%u\n",
                    fps_counter,
                    config.s16_width,
                    static_cast<unsigned>(tick10 / 10u), static_cast<unsigned>(tick10 % 10u),
                    static_cast<unsigned>(prep10 / 10u), static_cast<unsigned>(prep10 % 10u),
                    static_cast<unsigned>(rend10 / 10u), static_cast<unsigned>(rend10 % 10u),
                    static_cast<unsigned>(aud10 / 10u),  static_cast<unsigned>(aud10 % 10u),
                    static_cast<unsigned>(ps3_audio_buffered_frames()));
            acc_tick_us = 0;
            acc_prep_us = 0;
            acc_rend_us = 0;
            acc_aud_us = 0;
            perf_frames = 0;
            perf_timer.start();
        }
    }

    ps3_input_set_rumble(0.0f);
    ps3_video_set_fps(0);
    ps3_saves_sync_all(true);
    osoundint.queue_clear();
    cannonball::audio.clear_wav();
}

int main(int argc, char* argv[])
{
    ps3_paths_init(argc > 0 ? argv[0] : nullptr);
    ps3_log_init();
    ps3_boot_mark("main_enter");

    if (ps3_video_init() != 0)
    {
        ps3_boot_mark("video_init_fail");
        ps3_log("ps3_video_init failed\n");
        shutdown_all();
        sysProcessExit(1);
        return 1;
    }

    ps3_sysutil_register();
    ps3_boot_mark("sysutil_ok");

    ps3_input_init();
    ps3_boot_mark("input_ok");

    ps3_ui_init();
    ps3_boot_mark("ui_ok");

    ps3_config_load();
    ps3_boot_mark("cfg_ok");

    if (g_ps3_cfg.splash_enabled)
    {
        ps3_boot_mark("splash_begin");
        ps3_ui_show_splash();
        ps3_boot_mark("splash_end");
        if (g_app_quit)
        {
            shutdown_all();
            sysProcessExit(0);
            return 0;
        }
    }

    ps3_boot_mark("config_load_begin");
    config.load();
    if (!file_ok(config.data.res_path + "tilemap.bin"))
        config.data.res_path = g_ps3_res_dir;
    if (config.data.save_path.empty())
        config.data.save_path = g_ps3_save_dir;
    ps3_boot_mark("config_load_ok");

    ps3_boot_mark("roms_scan_begin");
    ps3_rom_report_t rep;
    ps3_roms_scan(&rep);
    ps3_log("roms_scan: revb=%d/%d res=%d/%d jap=%d/%d pcm=%d/%d all_req=%d path=%s\n",
            rep.revb_found, PS3_ROM_REVB_COUNT,
            rep.res_found, PS3_RES_BIN_COUNT,
            rep.jap_found, PS3_ROM_JAP_COUNT,
            rep.pcm_found, PS3_ROM_PCM_COUNT,
            rep.all_required_ok, rep.detected_rom_path);
    ps3_boot_mark("roms_scan_ok");

    while (!rep.all_required_ok)
    {
        ps3_boot_mark("missing_dlg_begin");
        int dlg = ps3_ui_missing_data_dialog(&rep);
        ps3_boot_mark("missing_dlg_end");
        if (dlg == 0 || g_app_quit)
        {
            shutdown_all();
            sysProcessExit(0);
            return 0;
        }
        ps3_roms_scan(&rep);
    }

    if (rep.detected_rom_path[0] != '\0')
        config.data.rom_path = rep.detected_rom_path;

    ps3_boot_mark("load_revb_begin");
    if (!roms.load_revb_roms(config.sound.fix_samples))
    {
        ps3_boot_mark("load_revb_fail");
        ps3_ui_missing_data_dialog(&rep);
        shutdown_all();
        sysProcessExit(1);
        return 1;
    }
    ps3_boot_mark("load_revb_ok");

    if (!omusic.load_widescreen_map(config.data.res_path))
    {
        ps3_log("load_widescreen_map failed for %s\n", config.data.res_path.c_str());
    }

    config.sound.rate = PS3_AUDIO_RATE;
    config.set_fps(config.video.fps);
    ps3_boot_mark("set_fps_ok");

    if (!video.init(&roms, &config.video))
    {
        ps3_log("video.init failed\n");
        shutdown_all();
        sysProcessExit(1);
        return 1;
    }
    ps3_boot_mark("video_core_ok");

    ps3_boot_mark("audio_ps3_begin");
    ps3_audio_init();
    ps3_boot_mark("audio_ps3_ok");

    audio.init();
    ps3_boot_mark("audio_core_ok");

    input.init(config.controls.pad_id,
               config.controls.keyconfig, config.controls.padconfig,
               config.controls.analog, config.controls.axis, config.controls.invert, config.controls.asettings);
    ps3_boot_mark("input_core_ok");

    menu = new Menu();
    menu->populate();
    ps3_boot_mark("menu_populate_ok");

    ps3_saves_sync_all(false);
    ps3_boot_mark("saves_sync_ok");

    int boot_menu_logged = 0;
    while (!g_app_quit)
    {
        ps3_sysutil_check();
        ps3_audio_pause(1);
        ps3_audio_clear_ring();
        config.sound.rate = PS3_AUDIO_RATE;
        osoundint.init();
        cannonball::audio.init();
        cannonball::audio.clear_wav();

        if (!boot_menu_logged)
        {
            ps3_boot_mark("main_menu_enter");
            boot_menu_logged = 1;
        }
        int launch = ps3_ui_main_menu(&rep);
        if (g_app_quit || launch == PS3_LAUNCH_EXIT || launch == PS3_LAUNCH_NONE)
            break;

        ps3_audio_pause(1);
        osoundint.queue_clear();
        cannonball::audio.clear_wav();
        ps3_audio_clear_ring();
        config.controls.rumble = static_cast<float>(g_ps3_cfg.rumble_pct) / 100.0f;
        config.video.scanlines = g_ps3_cfg.scanlines;
        config.video.filtering = g_ps3_cfg.linear_filter;
        config.video.fps_count = g_ps3_cfg.show_fps;

        if (launch == PS3_LAUNCH_ENHANCED)
        {
            menu->start_game(Outrun::MODE_ORIGINAL, 1);
        }
        else if (launch == PS3_LAUNCH_ORIGINAL)
        {
            menu->start_game(Outrun::MODE_ORIGINAL, 2);
        }
        else if (launch == PS3_LAUNCH_CONT)
        {
            config.sound.rate = PS3_AUDIO_RATE;
            config.set_fps(config.video.fps);
            video.disable();
            video.init(&roms, &config.video);
            outrun.custom_traffic = config.cont_traffic;
            menu->start_game(Outrun::MODE_CONT);
        }
        else if (launch == PS3_LAUNCH_TTRIAL)
        {
            config.sound.rate = PS3_AUDIO_RATE;
            config.set_fps(config.video.fps);
            video.disable();
            video.init(&roms, &config.video);
            menu->start_timetrial_ui();
        }
        else if (launch == PS3_LAUNCH_CLASSIC_MENU)
        {
            config.sound.rate = PS3_AUDIO_RATE;
            config.set_fps(config.video.fps);
            video.disable();
            video.init(&roms, &config.video);
            oinputs.init();
            outrun.outputs->init();
            menu->init(true);
            state = STATE_MENU;
        }
        else
        {
            continue;
        }

        cannonball::audio.init();
        if (config.sound.enabled)
            cannonball::audio.start_audio();
        else
            cannonball::audio.stop_audio();

        run_session_loop();
        cannonball::audio.stop_audio();
    }

    ps3_saves_sync_all(true);
    shutdown_all();
    sysProcessExit(0);
    return 0;
}
