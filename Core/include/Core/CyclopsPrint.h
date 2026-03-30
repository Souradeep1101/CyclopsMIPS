#pragma once

/**
 * @file CyclopsPrint.h
 * @brief Cross-platform C++23 print/println polyfill.
 * 
 * GCC 13 (Ubuntu 24.04 CI) lacks the <print> header. This shim provides a 
 * feature-tested implementation that falls back to std::vformat/std::cout 
 * when native support is unavailable.
 */

#if __has_include(<print>)
#include <print>
namespace Cyclops {
    using std::print;
    using std::println;
}
#else
#include <iostream>
#include <format>
#include <string_view>

namespace Cyclops {
    /**
     * @brief Type-safe print implementation using std::vformat fallback.
     */
    template<typename... Args>
    void print(std::string_view fmt, Args&&... args) {
        std::cout << std::vformat(fmt, std::make_format_args(args...));
    }

    /**
     * @brief Type-safe println implementation (appends \n) using std::vformat fallback.
     */
    template<typename... Args>
    void println(std::string_view fmt, Args&&... args) {
        std::cout << std::vformat(fmt, std::make_format_args(args...)) << "\n";
    }

    /**
     * @brief No-argument newline overload.
     */
    inline void println() {
        std::cout << "\n";
    }
}
#endif
