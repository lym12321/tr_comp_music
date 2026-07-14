#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace music {
    struct tone_t {
        uint16_t frequency_hz;
        uint16_t duration_ms;
    };

    bool init();

    // Both playback APIs require tones with static storage duration.
    bool play(std::span<const tone_t> tones, float duty_cycle = 0.2f);

    bool play_blocking(std::span<const tone_t> tones, float duty_cycle = 0.2f);

    bool pause();

    bool resume();

    void stop();

    bool busy();
}
