#pragma once
#include <glad/glad.h>

namespace MIPS::UI {

// Loads a texture from a file on disk.
// Returns the OpenGL texture handle, or 0 on failure.
// out_width and out_height receive the image dimensions.
GLuint LoadTextureFromFile(const char* path, int* out_width, int* out_height);

// Releases a previously loaded texture.
void UnloadTexture(GLuint textureID);

} // namespace MIPS::UI
