#include "sdl2/rendersurface.hpp"
#include "../../ps3/ps3_platform.hpp"
#include "../frontend/config.hpp"
#include <cstring>

Render::Render()
{
    window = nullptr;
    renderer = nullptr;
    texture = nullptr;
    screen_pixels = nullptr;
}

Render::~Render()
{
}

bool Render::init(int src_width, int src_height, int scale, int video_mode, int scanlines)
{
    this->src_width  = src_width;
    this->src_height = src_height;
    this->video_mode = video_mode;
    this->scale      = scale;
    this->scanlines  = scanlines;

    Rshift = 16;
    Gshift = 8;
    Bshift = 0;
    Rmask  = 0x00FF0000u;
    Gmask  = 0x0000FF00u;
    Bmask  = 0x000000FFu;

    orig_width  = src_width;
    orig_height = src_height;

    if (config.video.scanlines > 0 && g_ps3_cfg.scanlines == 0)
        g_ps3_cfg.scanlines = config.video.scanlines;

    return true;
}

void Render::disable()
{
}

bool Render::start_frame()
{
    return !g_app_quit;
}

bool Render::finalize_frame()
{
    return true;
}

void Render::draw_frame(uint16_t* pixels)
{
    ps3_video_draw_game_buffer(pixels, rgb, src_width, src_height);
}
