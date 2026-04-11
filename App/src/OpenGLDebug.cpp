#include "Cyclops/Platform/OpenGLDebug.h"
#include <glad/glad.h>
#include <iostream>

#if !defined(NDEBUG) && !defined(RELEASE)

namespace Cyclops {
    void EnableGLDebugging() {
        #if !defined(CYCLOPS_PLATFORM_MAC)
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(
                [](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
                    // Filter out non-crucial NVIDIA performance warnings
                    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

                    std::cout << "---------------------" << std::endl;
                    std::cout << "[OpenGL Debug Message] (" << id << "): " << message << std::endl;

                    if (type == GL_DEBUG_TYPE_ERROR || severity == GL_DEBUG_SEVERITY_HIGH) {
                        std::cout << "(Critical Error) Breaking into debugger..." << std::endl;
                        DEBUG_BREAK();
                    }
                }, 
                nullptr
            );
        #endif
    }
}

#endif
