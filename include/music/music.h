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

    // tones must have static storage duration.
    [[nodiscard]] bool play(std::span<const tone_t> tones, float duty_cycle = 0.2f);

    void stop();

    [[nodiscard]] bool busy();
}
