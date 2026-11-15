#include "ps3_platform.hpp"
#include <io/pad.h>
#include <cstring>

static int g_pad_inited = 0;
static padData g_last_pad[7];
static uint32_t g_btn_curr = 0;
static uint32_t g_btn_prev = 0;
static int g_ana_lx = 128;
static int g_ana_ly = 128;
static int g_ana_rx = 128;
static int g_ana_ry = 128;
static int g_ana_l2 = 0;
static int g_ana_r2 = 0;
static int g_active_port = 0;
static uint8_t g_last_small = 0;
static uint8_t g_last_large = 0;

void ps3_input_init(void)
{
    if (g_pad_inited) return;
    ioPadInit(7);
    memset(g_last_pad, 0, sizeof(g_last_pad));
    for (int i = 0; i < 7; i++) {
        g_last_pad[i].ANA_L_H = 128;
        g_last_pad[i].ANA_L_V = 128;
        g_last_pad[i].ANA_R_H = 128;
        g_last_pad[i].ANA_R_V = 128;
    }
    g_pad_inited = 1;
}

void ps3_input_shutdown(void)
{
    if (!g_pad_inited) return;
    ps3_input_set_rumble(0.0f);
    ioPadEnd();
    g_pad_inited = 0;
}

void ps3_input_poll(void)
{
    if (!g_pad_inited) ps3_input_init();

    padInfo info;
    memset(&info, 0, sizeof(info));
    ioPadGetInfo(&info);

    int picked = -1;
    for (int p = 0; p < 7; p++) {
        if (info.status[p]) {
            if (picked < 0) picked = p;
            padData pd;
            memset(&pd, 0, sizeof(pd));
            ioPadGetData(static_cast<u32>(p), &pd);
            if (pd.len > 0) {
                g_last_pad[p] = pd;
            }
        }
    }

    if (picked < 0) picked = 0;
    g_active_port = picked;
    const padData& pad = g_last_pad[picked];

    g_btn_prev = g_btn_curr;
    uint32_t m = 0;
    if (pad.BTN_UP)       m |= PS3_ACT_UP;
    if (pad.BTN_DOWN)     m |= PS3_ACT_DOWN;
    if (pad.BTN_LEFT)     m |= PS3_ACT_LEFT;
    if (pad.BTN_RIGHT)    m |= PS3_ACT_RIGHT;
    if (pad.BTN_CROSS)    m |= PS3_ACT_CROSS;
    if (pad.BTN_CIRCLE)   m |= PS3_ACT_CIRCLE;
    if (pad.BTN_SQUARE)   m |= PS3_ACT_SQUARE;
    if (pad.BTN_TRIANGLE) m |= PS3_ACT_TRIANGLE;
    if (pad.BTN_L1)       m |= PS3_ACT_L1;
    if (pad.BTN_R1)       m |= PS3_ACT_R1;
    if (pad.BTN_L2)       m |= PS3_ACT_L2;
    if (pad.BTN_R2)       m |= PS3_ACT_R2;
    if (pad.BTN_START)    m |= PS3_ACT_START;
    if (pad.BTN_SELECT)   m |= PS3_ACT_SELECT;
    if (pad.BTN_L3)       m |= PS3_ACT_L3;
    if (pad.BTN_R3)       m |= PS3_ACT_R3;

    g_ana_lx = static_cast<int>(pad.ANA_L_H & 0xFF);
    g_ana_ly = static_cast<int>(pad.ANA_L_V & 0xFF);
    g_ana_rx = static_cast<int>(pad.ANA_R_H & 0xFF);
    g_ana_ry = static_cast<int>(pad.ANA_R_V & 0xFF);
    g_ana_l2 = static_cast<int>(pad.PRE_L2 & 0xFF);
    g_ana_r2 = static_cast<int>(pad.PRE_R2 & 0xFF);

    if (g_ana_l2 > 32) m |= PS3_ACT_L2;
    if (g_ana_r2 > 32) m |= PS3_ACT_R2;
    if (g_ana_lx < 56) m |= PS3_ACT_LEFT;
    if (g_ana_lx > 200) m |= PS3_ACT_RIGHT;
    if (g_ana_ly < 56) m |= PS3_ACT_UP;
    if (g_ana_ly > 200) m |= PS3_ACT_DOWN;

    g_btn_curr = m;
}

uint32_t ps3_input_buttons(void)
{
    return g_btn_curr;
}

uint32_t ps3_input_pressed(void)
{
    return g_btn_curr & ~g_btn_prev;
}

void ps3_input_get_analog(int *lx, int *ly, int *rx, int *ry, int *l2, int *r2)
{
    if (lx) *lx = g_ana_lx;
    if (ly) *ly = g_ana_ly;
    if (rx) *rx = g_ana_rx;
    if (ry) *ry = g_ana_ry;
    if (l2) *l2 = g_ana_l2;
    if (r2) *r2 = g_ana_r2;
}

void ps3_input_set_rumble(float strength)
{
    if (!g_pad_inited) return;
    float scaled = strength * (static_cast<float>(g_ps3_cfg.rumble_pct) / 100.0f);
    if (scaled < 0.0f) scaled = 0.0f;
    if (scaled > 1.0f) scaled = 1.0f;

    uint8_t small_m = (scaled >= 0.25f) ? 1 : 0;
    uint8_t large_m = static_cast<uint8_t>(scaled * 255.0f);
    if (small_m == g_last_small && large_m == g_last_large) return;

    g_last_small = small_m;
    g_last_large = large_m;

    padActParam act;
    memset(&act, 0, sizeof(act));
    act.small_motor = small_m;
    act.large_motor = large_m;
    ioPadSetActDirect(static_cast<u32>(g_active_port), &act);
}
