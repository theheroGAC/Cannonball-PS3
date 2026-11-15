#include "sdl2/timer.hpp"
#include "../../ps3/ps3_platform.hpp"

Timer::Timer()
{
    startTicks = 0;
    pausedTicks = 0;
    paused = false;
    started = false;
}

void Timer::start()
{
    started = true;
    paused = false;
    startTicks = static_cast<int>(ps3_ticks_ms());
}

void Timer::stop()
{
    started = false;
    paused = false;
}

void Timer::pause()
{
    if (started && !paused) {
        paused = true;
        pausedTicks = static_cast<int>(ps3_ticks_ms()) - startTicks;
    }
}

void Timer::unpause()
{
    if (paused) {
        paused = false;
        startTicks = static_cast<int>(ps3_ticks_ms()) - pausedTicks;
        pausedTicks = 0;
    }
}

int Timer::get_ticks()
{
    if (started) {
        if (paused) return pausedTicks;
        return static_cast<int>(ps3_ticks_ms()) - startTicks;
    }
    return 0;
}

bool Timer::is_started()
{
    return started;
}

bool Timer::is_paused()
{
    return paused;
}
