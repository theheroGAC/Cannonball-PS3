#include <cstdint>

extern "C" {

void ps3_sdl_render_cp(const char *tag)
{
    (void)tag;
}

void ps3_sdl_flip_timeout(unsigned int ms)
{
    (void)ms;
}

void ps3_sdl_fifo_blocked(unsigned int ms)
{
    (void)ms;
}

}
