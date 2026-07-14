#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace music {
    struct tone_t {
        uint16_t frequency_hz;
        uint16_t duration_ms;
    };

    [[nodiscard]] bool init();

    // Both playback APIs require tones with static storage duration.
    [[nodiscard]] bool play(std::span<const tone_t> tones, float duty_cycle = 0.2f);

    [[nodiscard]] bool play_blocking(std::span<const tone_t> tones, float duty_cycle = 0.2f);

    [[nodiscard]] bool pause();

    [[nodiscard]] bool resume();

    void stop();

    [[nodiscard]] bool busy();
}
