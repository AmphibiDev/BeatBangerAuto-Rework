#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cstddef>

namespace Constants {
    constexpr size_t MEMORY_CHUNK_SIZE = 8 * 1024 * 1024;
    constexpr int NUM_SEARCH_THREADS = 4;
    constexpr int AUTOPLAY_CHECK_INTERVAL = 50;

    constexpr const char* APP_VERSION = "0.6beta";
    constexpr const char* GAME_PROCESS_NAME = "beatbanger.exe";

    constexpr const char* CONFIG_FILENAME = "config.json";
    constexpr const char* GITHUB_CONFIG_URL = "https://raw.githubusercontent.com/AmphibiDev/BeatBangerAuto-Rework/dev/config.json";
    constexpr const char* GITHUB_RELEASES_URL = "https://github.com/AmphibiDev/BeatBangerAuto-Rework/releases/latest";
    constexpr int MAX_REASONABLE_OFFSET = 1024 * 1024;

    constexpr int NETWORK_REQUEST_TIMEOUT = 10000;

    constexpr int THREAD_QUIT_TIMEOUT = 2000;
    constexpr int THREAD_TERMINATE_TIMEOUT = 1000;

    constexpr int BUTTON_COOLDOWN_MS = 500;
}

#endif // CONSTANTS_H
