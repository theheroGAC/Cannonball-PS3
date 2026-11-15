#include "ps3_platform.hpp"
#include "../src/main/engine/audio/osoundint.hpp"
#include "../src/main/main.hpp"

#include <audio/audio.h>
#include <sys/event_queue.h>
#include <sys/thread.h>
#include <sys/mutex.h>

#include <cstring>
#include <unistd.h>

#define PS3_RING_FRAMES 32768
#define PS3_RING_MASK (PS3_RING_FRAMES - 1)
#define PS3_AUDIO_PRIME_FRAMES 3200u
#define PS3_AUDIO_TARGET_FRAMES 4096u
#define PS3_AUDIO_DEADZONE_FRAMES 800
#define PS3_AUDIO_MAX_FRAMES 12288u

static volatile int g_audio_running = 0;
static volatile int g_audio_paused = 1;
static volatile uint32_t g_ring_rd = 0;
static volatile uint32_t g_ring_wr = 0;
static volatile uint32_t g_clear_target = 0;
static volatile uint32_t g_clear_seq = 0;
static volatile uint32_t g_seen_clear_seq = 0;
static int16_t g_ring_buf[PS3_RING_FRAMES * 2];

static u32 g_audio_port = 0;
static audioPortConfig g_port_cfg;
static sys_event_queue_t g_evt_queue = 0;
static u64 g_queue_key = 0;
static sys_ppu_thread_t g_audio_thread = 0;
static int g_audio_inited = 0;

static void audio_thread_entry(void *arg)
{
    (void)arg;
    if (g_port_cfg.audioDataStart == 0) {
        sysThreadExit(0);
        return;
    }

    u32 num_blocks = static_cast<u32>(g_port_cfg.numBlocks);
    if (num_blocks == 0) num_blocks = AUDIO_BLOCK_8;
    u32 block_idx = 4u % num_blocks;
    float last_l = 0.0f;
    float last_r = 0.0f;
    uint32_t frac_fp = 0;
    int primed = 0;

    while (g_audio_running) {
        sys_event_t evt;
        if (sysEventQueueReceive(g_evt_queue, &evt, 50000) != 0) {
            usleep(1000);
            continue;
        }
        if (!g_audio_running) break;

        uint32_t cseq = g_clear_seq;
        uint32_t wr = g_ring_wr;
        __sync_synchronize();

        uint32_t rd = g_ring_rd;
        if (g_seen_clear_seq != cseq) {
            rd = g_clear_target;
            g_seen_clear_seq = cseq;
            frac_fp = 0;
            primed = 0;
            last_l = 0.0f;
            last_r = 0.0f;
        }

        float *out = reinterpret_cast<float *>(
            static_cast<uintptr_t>(g_port_cfg.audioDataStart) +
            static_cast<uintptr_t>(block_idx) * AUDIO_PORT_2CH * AUDIO_BLOCK_SAMPLES * sizeof(float)
        );

        int vol_pct = g_ps3_cfg.volume_pct;
        if (vol_pct < 0) vol_pct = 0;
        if (vol_pct > 100) vol_pct = 100;
        float vol = static_cast<float>(vol_pct) / (100.0f * 32768.0f);

        uint32_t avail = wr - rd;

        if (g_audio_paused || g_app_quit) {
            for (u32 i = 0; i < AUDIO_BLOCK_SAMPLES * 2u; i++) {
                out[i] = 0.0f;
            }
            last_l = 0.0f;
            last_r = 0.0f;
            frac_fp = 0;
            primed = 0;
        } else {
            if (!primed) {
                if (avail >= PS3_AUDIO_PRIME_FRAMES) {
                    primed = 1;
                }
            }

            if (!primed) {
                for (u32 i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
                    last_l *= 0.96f;
                    last_r *= 0.96f;
                    out[i * 2u + 0u] = last_l;
                    out[i * 2u + 1u] = last_r;
                }
            } else {
                if (avail > PS3_AUDIO_MAX_FRAMES) {
                    rd = wr - PS3_AUDIO_TARGET_FRAMES;
                    avail = PS3_AUDIO_TARGET_FRAMES;
                    frac_fp = 0;
                }

                int32_t err = static_cast<int32_t>(avail) - static_cast<int32_t>(PS3_AUDIO_TARGET_FRAMES);
                int32_t adj = 0;
                if (err < -PS3_AUDIO_DEADZONE_FRAMES) {
                    adj = (err + PS3_AUDIO_DEADZONE_FRAMES) / 8;
                    if (adj < -512) adj = -512;
                } else if (err > PS3_AUDIO_DEADZONE_FRAMES) {
                    adj = (err - PS3_AUDIO_DEADZONE_FRAMES) / 8;
                    if (adj > 512) adj = 512;
                }
                uint32_t step_fp = static_cast<uint32_t>(65536 + adj);

                for (u32 i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
                    if (avail >= 2u) {
                        uint32_t slot0 = (rd & PS3_RING_MASK) * 2u;
                        uint32_t slot1 = ((rd + 1u) & PS3_RING_MASK) * 2u;
                        float frac = static_cast<float>(frac_fp) * (1.0f / 65536.0f);

                        float l0 = static_cast<float>(g_ring_buf[slot0 + 0u]);
                        float l1 = static_cast<float>(g_ring_buf[slot1 + 0u]);
                        last_l = (l0 + (l1 - l0) * frac) * vol;

                        float r0 = static_cast<float>(g_ring_buf[slot0 + 1u]);
                        float r1 = static_cast<float>(g_ring_buf[slot1 + 1u]);
                        last_r = (r0 + (r1 - r0) * frac) * vol;

                        frac_fp += step_fp;
                        uint32_t adv = frac_fp >> 16;
                        frac_fp &= 0xFFFFu;
                        if (adv >= avail) {
                            rd += avail;
                            avail = 0;
                        } else {
                            rd += adv;
                            avail -= adv;
                        }
                    } else if (avail == 1u) {
                        uint32_t slot0 = (rd & PS3_RING_MASK) * 2u;
                        last_l = static_cast<float>(g_ring_buf[slot0 + 0u]) * vol;
                        last_r = static_cast<float>(g_ring_buf[slot0 + 1u]) * vol;
                        rd += 1u;
                        avail = 0;
                        frac_fp = 0;
                    } else {
                        last_l *= 0.990f;
                        last_r *= 0.990f;
                    }
                    out[i * 2u + 0u] = last_l;
                    out[i * 2u + 1u] = last_r;
                }
            }
        }

        __sync_synchronize();
        g_ring_rd = rd;
        block_idx = (block_idx + 1u) % num_blocks;
    }

    sysThreadExit(0);
}

int ps3_audio_init(void)
{
    if (g_audio_inited) return 0;

    if (audioInit() != 0) {
        ps3_log("audioInit failed\n");
        return -1;
    }

    audioPortParam params;
    memset(&params, 0, sizeof(params));
    params.numChannels = AUDIO_PORT_2CH;
    params.numBlocks = AUDIO_BLOCK_8;
    params.attrib = AUDIO_PORT_INITLEVEL;
    params.level = 1.0f;

    if (audioPortOpen(&params, &g_audio_port) != 0) {
        ps3_log("audioPortOpen failed\n");
        audioQuit();
        return -1;
    }

    memset(&g_port_cfg, 0, sizeof(g_port_cfg));
    if (audioGetPortConfig(g_audio_port, &g_port_cfg) != 0 || g_port_cfg.audioDataStart == 0) {
        ps3_log("audioGetPortConfig failed\n");
        audioPortClose(g_audio_port);
        audioQuit();
        return -1;
    }

    if (audioCreateNotifyEventQueue(&g_evt_queue, &g_queue_key) != 0) {
        ps3_log("audioCreateNotifyEventQueue failed\n");
        audioPortClose(g_audio_port);
        audioQuit();
        return -1;
    }

    if (audioSetNotifyEventQueue(g_queue_key) != 0) {
        ps3_log("audioSetNotifyEventQueue failed\n");
        sysEventQueueDestroy(g_evt_queue, 0);
        audioPortClose(g_audio_port);
        audioQuit();
        return -1;
    }

    u32 num_blocks = static_cast<u32>(g_port_cfg.numBlocks);
    if (num_blocks == 0) num_blocks = AUDIO_BLOCK_8;
    size_t dma_bytes = static_cast<size_t>(num_blocks) * AUDIO_PORT_2CH * AUDIO_BLOCK_SAMPLES * sizeof(float);
    memset(reinterpret_cast<void *>(static_cast<uintptr_t>(g_port_cfg.audioDataStart)), 0, dma_bytes);

    sysEventQueueDrain(g_evt_queue);
    memset(g_ring_buf, 0, sizeof(g_ring_buf));
    g_ring_rd = 0;
    g_ring_wr = 0;
    g_clear_target = 0;
    g_clear_seq = 0;
    g_seen_clear_seq = 0;
    g_audio_paused = 1;
    __sync_synchronize();
    g_audio_running = 1;

    char tname[] = "cannonball_aud";
    if (sysThreadCreate(&g_audio_thread, audio_thread_entry, nullptr, 1000, 0x10000, THREAD_JOINABLE, tname) != 0) {
        ps3_log("sysThreadCreate audio failed\n");
        g_audio_running = 0;
        audioRemoveNotifyEventQueue(g_queue_key);
        audioPortClose(g_audio_port);
        sysEventQueueDestroy(g_evt_queue, 0);
        audioQuit();
        return -1;
    }

    if (audioPortStart(g_audio_port) != 0) {
        ps3_log("audioPortStart failed\n");
        g_audio_running = 0;
        u64 retval = 0;
        sysThreadJoin(g_audio_thread, &retval);
        audioRemoveNotifyEventQueue(g_queue_key);
        audioPortClose(g_audio_port);
        sysEventQueueDestroy(g_evt_queue, 0);
        audioQuit();
        return -1;
    }

    ps3_log("ps3_audio_init ok: port=%u blocks=%u addr=0x%08x\n",
            static_cast<unsigned>(g_audio_port),
            static_cast<unsigned>(num_blocks),
            static_cast<unsigned>(g_port_cfg.audioDataStart));
    g_audio_inited = 1;
    return 0;
}

void ps3_audio_shutdown(void)
{
    if (!g_audio_inited) return;
    g_audio_running = 0;
    u64 retval = 0;
    sysThreadJoin(g_audio_thread, &retval);
    audioPortStop(g_audio_port);
    audioRemoveNotifyEventQueue(g_queue_key);
    audioPortClose(g_audio_port);
    sysEventQueueDestroy(g_evt_queue, 0);
    audioQuit();
    g_audio_inited = 0;
}

void ps3_audio_pause(int paused)
{
    g_audio_paused = paused ? 1 : 0;
    __sync_synchronize();
}

void ps3_audio_push_frames(const int16_t *stereo_frames, uint32_t frame_count)
{
    if (!g_audio_inited || !stereo_frames || frame_count == 0) return;
    uint32_t wr = g_ring_wr;
    uint32_t rd = (g_seen_clear_seq != g_clear_seq) ? g_clear_target : g_ring_rd;
    __sync_synchronize();
    uint32_t used = wr - rd;
    if (used >= PS3_RING_FRAMES - 1u) return;
    uint32_t free_slots = (PS3_RING_FRAMES - 1u) - used;
    uint32_t to_write = (frame_count <= free_slots) ? frame_count : free_slots;
    for (uint32_t i = 0; i < to_write; i++) {
        uint32_t slot = (wr & PS3_RING_MASK) * 2u;
        g_ring_buf[slot + 0u] = stereo_frames[i * 2u + 0u];
        g_ring_buf[slot + 1u] = stereo_frames[i * 2u + 1u];
        wr++;
    }
    __sync_synchronize();
    g_ring_wr = wr;
}

uint32_t ps3_audio_buffered_frames(void)
{
    uint32_t wr = g_ring_wr;
    uint32_t rd = (g_seen_clear_seq != g_clear_seq) ? g_clear_target : g_ring_rd;
    return wr - rd;
}

void ps3_audio_clear_ring(void)
{
    g_clear_target = g_ring_wr;
    __sync_synchronize();
    g_clear_seq++;
}

void ps3_audio_tick_frame(void)
{
    osoundint.tick();
    cannonball::audio.tick();
}
