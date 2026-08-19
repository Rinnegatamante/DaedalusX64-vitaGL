#pragma once

#include <stdint.h>

namespace Fast {

struct LightN64 {
    uint8_t Color[3];
    int8_t Pad1;
    uint8_t ColorCopy[3];
    int8_t Pad2;
    uint8_t Direction[3];
    int8_t Pad3;
};

struct PointLightN64 {
    uint8_t Color[3];
    uint8_t Unk0;
    uint8_t ColorCopy[3];
    uint8_t Unk1;
    int16_t Position[3];
    uint8_t Unk2;
};

union LightData {
    LightN64 Light;
    PointLightN64 PointLight;
    long long ForceAlignment[2];
};

union AmbientData {
    uint8_t Color[3];
    int8_t Pad1;
    uint8_t ColorCopy[3];
    int8_t Pad2;
};

struct LightEntry {
    AmbientData Ambient;
    LightData Light;
};

}
