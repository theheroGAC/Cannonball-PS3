#pragma once

#include <cstdint>

void ps3_font_init(void);
void ps3_font_shutdown(void);

void ps3_font5x7_draw_string(uint32_t *fb, int fb_w, int fb_h, int x, int y, const char *s, uint32_t argb, int scale);

void ps3_draw_fill(int x, int y, int w, int h, uint32_t argb);
void ps3_draw_blend(int x, int y, int w, int h, uint32_t argb);
void ps3_draw_rect(int x, int y, int w, int h, uint32_t argb, int thickness);
void ps3_draw_gradient_v(int x, int y, int w, int h, uint32_t top_argb, uint32_t bot_argb);
void ps3_draw_text(int x, int y, const char *text, uint32_t argb, int size_tier);
void ps3_draw_text_centered(int cx, int y, const char *text, uint32_t argb, int size_tier);
int ps3_measure_text(const char *text, int size_tier);
void ps3_draw_btn_icon(int icon_id, int x, int y);
