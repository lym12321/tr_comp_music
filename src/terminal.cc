#include "music/music.h"
#include "utils/terminal.h"

namespace {
    constexpr music::tone_t test_track[] {
        { 523, 800 },
        { 587, 800 },
        { 659, 800 },
        { 698, 800 },
        { 784, 800 },
        { 880, 800 },
        { 988, 800 },
        { 1047, 800 },
    };
}

void music::register_terminal_commands() {
    terminal::register_cmd("music", [](const std::vector<std::string> &args) {
        if (args.size() == 3 && args[1] == "play" && args[2] == "test") {
            terminal::info(music::play(test_track) ? "Playing test track.\r\n" : "Play failed.\r\n");
            return;
        }
        if (args.size() == 2 && args[1] == "pause") {
            terminal::info(music::pause() ? "Paused.\r\n" : "Pause failed.\r\n");
            return;
        }
        if (args.size() == 2 && args[1] == "resume") {
            terminal::info(music::resume() ? "Resumed.\r\n" : "Resume failed.\r\n");
            return;
        }
        if (args.size() == 2 && args[1] == "stop") {
            music::stop();
            terminal::info("Stopped.\r\n");
            return;
        }
        terminal::info("Usage: music [play test | pause | resume | stop]\r\n");
    }, "Test music playback");
}
