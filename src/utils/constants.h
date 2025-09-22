#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cstddef>

namespace Constants {

// Memory scanning constants
constexpr size_t MEMORY_CHUNK_SIZE = 8 * 1024 * 1024;   // 8MB chunks
constexpr int NUM_SEARCH_THREADS = 4;                   // Number of parallel search threads
constexpr int AUTOPLAY_CHECK_INTERVAL = 50;             // 50ms between autoplay checks

// Game detection constants
constexpr const char* GAME_PROCESS_NAME = "beatbanger.exe";
constexpr int MAX_VERSION_SCAN_RETRIES = 3;
constexpr size_t VERSION_BUFFER_SIZE = 256;

// Configuration constants
constexpr const char* CONFIG_FILENAME = "versions.json";
constexpr int MAX_REASONABLE_OFFSET = 1024 * 1024;      // 1MB max offset

// Network constants
constexpr int NETWORK_REQUEST_TIMEOUT = 10000;          // 10 seconds network timeout

// Thread timeouts
constexpr int THREAD_QUIT_TIMEOUT = 2000;               // 2 seconds
constexpr int THREAD_TERMINATE_TIMEOUT = 1000;          // 1 second

// UI constants
constexpr int BUTTON_COOLDOWN_MS = 500;                 // Button press cooldown

}

#endif // CONSTANTS_H
