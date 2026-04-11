#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "TextureLoader.h"
#include <cstdio>

namespace MIPS::UI {

GLuint LoadTextureFromFile(const char* path, int* out_width, int* out_height) {
    int width = 0, height = 0, channels = 0;
    unsigned char* data = stbi_load(path, &width, &height, &channels, 4); // Force RGBA
    if (!data) {
        fprintf(stderr, "[TextureLoader] Failed to load image: %s\n", path);
        return 0;
    }

    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Filtering: bilinear for clean scaling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);

    if (out_width)  *out_width  = width;
    if (out_height) *out_height = height;

    return textureID;
}

void UnloadTexture(GLuint textureID) {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

} // namespace MIPS::UI
