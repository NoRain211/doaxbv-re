#include "d3d_vblank.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace {
using Clock = std::chrono::steady_clock;
Clock::time_point last_vblank;
// ponytail: the supported progressive NTSC mode; other video modes need their rate.
constexpr auto interval = std::chrono::nanoseconds(1000000000 / 60);
}

void recomp_d3d_vblank_reset(void)
{
    last_vblank = Clock::now();
}

void recomp_d3d_wait_vblank(void)
{
    // Late frames discard timing debt instead of running catch-up updates.
    last_vblank = std::max(last_vblank + interval, Clock::now());
    std::this_thread::sleep_until(last_vblank);
}
