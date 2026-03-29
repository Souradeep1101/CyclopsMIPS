#pragma once
#include "Cyclops/Core/PlatformDetection.h"
#include <glad/glad.h>
#include <iostream>

namespace Cyclops {

    /**
     * @brief Initializes the OpenGL Debug Output system.
     * - Windows/Linux: Modern glDebugMessageCallback.
     * - macOS: No-op (Requires OpenGL 4.3+).
     */
    void EnableGLDebugging();

    #if defined(NDEBUG) || defined(RELEASE)
        // Strip debugging code in production for zero overhead
        #define GLCall(x) x
        inline void EnableGLDebugging() {}
    #else
        // Platform-specific Hardware Breakpoints
        #if defined(_MSC_VER)
            #define DEBUG_BREAK() __debugbreak()
        #elif defined(__APPLE__)
            // Modern traps for Apple Silicon and Clang compatibility
            #define DEBUG_BREAK() __builtin_debugtrap()
        #elif defined(__GNUC__) || defined(__clang__)
            #define DEBUG_BREAK() __builtin_trap()
        #else
            #define DEBUG_BREAK() std::abort()
        #endif

        #if defined(CYCLOPS_PLATFORM_MAC)
            // macOS Legacy Loop (OpenGL 4.1)
            inline void GLClearError() { while (glGetError() != GL_NO_ERROR); }
            inline bool GLLogCall(const char* function, const char* file, int line) {
                while (GLenum error = glGetError()) {
                    std::cout << "[OpenGL Error] (" << error << "): " << function << " " << file << ":" << line << std::endl;
                    return false;
                }
                return true;
            }
            #define GLCall(x) GLClearError(); x; if (!GLLogCall(#x, __FILE__, __LINE__)) DEBUG_BREAK();
        #else
            // Modern Pass-through (managed by EnableGLDebugging callback)
            #define GLCall(x) x
        #endif
    #endif
}
