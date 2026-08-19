#pragma once

#include "fast/vita_compat.h"

#include <stdint.h>

#define TEX_FLAG_LOAD_AS_RAW (1 << 0)
#define TEX_FLAG_LOAD_AS_IMG (1 << 1)

namespace Fast {

enum class TextureType {
    Error = 0,
    RGBA32bpp = 1,
    RGBA16bpp = 2,
    Palette4bpp = 3,
    Palette8bpp = 4,
    Grayscale4bpp = 5,
    Grayscale8bpp = 6,
    GrayscaleAlpha4bpp = 7,
    GrayscaleAlpha8bpp = 8,
    GrayscaleAlpha16bpp = 9,
};

class Texture : public Ship::IResource {
public:
    uint8_t* GetPointer() { return ImageData; }
    size_t GetPointerSize() { return ImageDataSize; }

    TextureType Type = TextureType::Error;
    uint16_t Width = 0;
    uint16_t Height = 0;
    uint32_t Flags = 0;
    float HByteScale = 1.0f;
    float VPixelScale = 1.0f;
    uint32_t ImageDataSize = 0;
    uint8_t* ImageData = nullptr;
};

}
