#include "d3d_vblank.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <thread>

namespace {
using Clock = std::chrono::steady_clock;
Clock::time_point last_vblank;
HANDLE high_resolution_timer;
// ponytail: the supported progressive NTSC mode; other video modes need their rate.
constexpr auto interval = std::chrono::nanoseconds(1000000000 / 60);
constexpr auto spin_window = std::chrono::microseconds(500);
}

void recomp_d3d_vblank_reset(void)
{
    last_vblank = Clock::now();
}

void recomp_d3d_wait_vblank(void)
{
    // Late frames discard timing debt instead of running catch-up updates.
    last_vblank = std::max(last_vblank + interval, Clock::now());
    if (high_resolution_timer == nullptr) {
        high_resolution_timer = CreateWaitableTimerExW(
            nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
            TIMER_MODIFY_STATE | SYNCHRONIZE);
    }
    if (high_resolution_timer == nullptr) {
        std::this_thread::sleep_until(last_vblank);
        return;
    }

    const auto spin_deadline = last_vblank - spin_window;
    const auto timer_wait = spin_deadline - Clock::now();
    if (timer_wait > Clock::duration::zero()) {
        LARGE_INTEGER due{};
        due.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>(
            timer_wait).count() / 100;
        if (due.QuadPart < 0 &&
            SetWaitableTimer(high_resolution_timer, &due, 0, nullptr, nullptr, FALSE)) {
            WaitForSingleObject(high_resolution_timer, INFINITE);
        } else {
            std::this_thread::sleep_until(last_vblank);
            return;
        }
    }
    while (Clock::now() < last_vblank) YieldProcessor();
}
