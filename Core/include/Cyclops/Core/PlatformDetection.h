#pragma once

#if defined(CYCLOPS_PLATFORM_WINDOWS)
    // Windows 64-bit strictly enforced via CMake
#elif defined(CYCLOPS_PLATFORM_MAC)
    // macOS (Apple Silicon & Intel)
#elif defined(CYCLOPS_PLATFORM_LINUX)
    // Generic Linux
#else
    #error "Unknown Platform! Ensure CYCLOPS_PLATFORM_X is defined in CMake."
#endif
