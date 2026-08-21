#define NOMINMAX

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>
#include <list>
#include <stack>
#include "fast/resource/type/Light.h"
#ifdef __vita__
#include "fast/vita_compat.h"
void RDRAM_MarkDirtyRange(uint32_t address, uint32_t size);
#endif

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include "fast/types.h"
#include <string>

#include "fast/interpreter.h"
#include "fast/lus_gbi.h"
#include "fast/backends/gfx_window_manager_api.h"
#include "fast/backends/gfx_rendering_api.h"




#ifdef _WIN32
#include <windows.h>
#endif

std::stack<std::string> currentDir;

#define SEG_ADDR(seg, addr) (addr | (seg << 24) | 1)
#define SUPPORT_CHECK(x) assert(x)

// SCALE_M_N: upscale/downscale M-bit integer to N-bit
#define SCALE_5_8(VAL_) (((VAL_)*0xFF) / 0x1F)
#define SCALE_8_5(VAL_) ((((VAL_) + 4) * 0x1F) / 0xFF)
#define SCALE_4_8(VAL_) ((VAL_)*0x11)
#define SCALE_8_4(VAL_) ((VAL_) / 0x11)
#define SCALE_3_8(VAL_) ((VAL_)*0x24)
#define SCALE_8_3(VAL_) ((VAL_) / 0x24)

// Based off the current set native dimensions or active framebuffer
#define HALF_SCREEN_WIDTH(activeFb) ((mFbActive ? activeFb->second.orig_width : mNativeDimensions.width) / 2)
#define HALF_SCREEN_HEIGHT(activeFb) ((mFbActive ? activeFb->second.orig_height : mNativeDimensions.height) / 2)

// Ratios for current window dimensions or active framebuffer scaled size
#define RATIO_X(activeFb, dims) \
    ((mFbActive ? activeFb->second.applied_width : dims.width) / (2.0f * HALF_SCREEN_WIDTH(activeFb)))
#define RATIO_Y(activeFb, dims) \
    ((mFbActive ? activeFb->second.applied_height : dims.height) / (2.0f * HALF_SCREEN_HEIGHT(activeFb)))

#define FAST_CLAMP(x, min, max) (x > max ? max : (x < min ? min : x))

#define TEXTURE_CACHE_MAX_SIZE 1024

#ifdef __vita__
#include <vitasdk.h>
#include "Utility/Endian.h"
extern "C" void* vglAllocFromScratch(size_t size);
#endif

namespace Fast {

static Interpreter *gInstance = nullptr;
static UcodeHandlers ucode_handler_index = ucode_f3dex2;

const static uint32_t f3dex2AttrHandler[] = {
    F3DEX2_G_MTX_PROJECTION, F3DEX2_G_MTX_LOAD,  F3DEX2_G_MTX_PUSH,  F3DEX_G_MTX_NOPUSH,
    F3DEX2_G_CULL_FRONT,     F3DEX2_G_CULL_BACK, F3DEX2_G_CULL_BOTH,
};

const static uint32_t f3dexAttrHandler[] = { F3DEX_G_MTX_PROJECTION, F3DEX_G_MTX_LOAD,   F3DEX_G_MTX_PUSH,
                                             F3DEX_G_MTX_NOPUSH,     F3DEX_G_CULL_FRONT, F3DEX_G_CULL_BACK,
                                             F3DEX_G_CULL_BOTH };

static constexpr const uint32_t* ucode_attr_handlers[] = {
    f3dexAttrHandler,
    f3dexAttrHandler,
    f3dexAttrHandler,
    f3dexAttrHandler,
    f3dex2AttrHandler,
    f3dex2AttrHandler,
    f3dexAttrHandler,
};

static uint32_t get_attr(Attribute attr) {
    return ucode_attr_handlers[ucode_handler_index][attr];
}

static std::string GetPathWithoutFileName(char* filePath) {
    size_t len = strlen(filePath);

    for (size_t i = len - 1; (long)i >= 0; i--) {
        if (filePath[i] == '/' || filePath[i] == '\\') {
            return std::string(filePath).substr(0, i);
        }
    }

    return filePath;
}

constexpr size_t MAX_TRI_BUFFER = 256;

Interpreter::Interpreter() {
    mRsp = new RSP();
    mRdp = new RDP();
#ifdef __vita__
    mBufVbo = static_cast<float*>(vglAllocFromScratch(10 * 1024 * 1024));
#else
    mBufVbo = new float[MAX_TRI_BUFFER * (32 * 3)];
#endif
}

Interpreter::~Interpreter() {
    delete mRsp;
    delete mRdp;
#ifndef __vita__
    delete[] mBufVbo;
#endif
}

static std::weak_ptr<Interpreter> mInstance;
// Set a cached pointer to the instance so we don't need to go through the window every time
void GfxSetInstance(std::shared_ptr<Interpreter> gfx) {
    mInstance = gfx;
	gInstance = gfx.get();
}

// N64 prim_depth is 15-bit (0 near, 0x7FFF far).
static constexpr float N64_PRIM_DEPTH_MAX = 32767.0f;

void Interpreter::Flush() {
    if (mBufVboNumTris > 0) {
        //mRapi->SetCurrentPrimDepth((float)mRdp->prim_depth / N64_PRIM_DEPTH_MAX);
        mRapi->DrawTriangles(mBufVbo, mBufVboLen, mBufVboNumTris);
#ifdef __vita__
        mBufVbo += mBufVboLen;
#endif
        mBufVboLen = 0;
        mBufVboNumTris = 0;
    }
}

ShaderProgram* Interpreter::LookupOrCreateShaderProgram(uint64_t id0, uint64_t id1) {
    ShaderProgram* prg = mRapi->LookupShader(id0, id1);
    if (prg == nullptr) {
        mRapi->UnloadShader(mRenderingState.mShaderProgram);
        prg = mRapi->CreateAndLoadNewShader(id0, id1);
        mRenderingState.mShaderProgram = prg;
    }
    return prg;
}

const char* Interpreter::CCMUXtoStr(uint32_t ccmux) {
    static constexpr const char* tbl[] = {
        "G_CCMUX_COMBINED",
        "G_CCMUX_TEXEL0",
        "G_CCMUX_TEXEL1",
        "G_CCMUX_PRIMITIVE",
        "G_CCMUX_SHADE",
        "G_CCMUX_ENVIRONMENT",
        "G_CCMUX_1",
        "G_CCMUX_COMBINED_ALPHA",
        "G_CCMUX_TEXEL0_ALPHA",
        "G_CCMUX_TEXEL1_ALPHA",
        "G_CCMUX_PRIMITIVE_ALPHA",
        "G_CCMUX_SHADE_ALPHA",
        "G_CCMUX_ENV_ALPHA",
        "G_CCMUX_LOD_FRACTION",
        "G_CCMUX_PRIM_LOD_FRAC",
        "G_CCMUX_K5",
    };
    if (ccmux >= sizeof(tbl) / sizeof(tbl[0])) {
        return "G_CCMUX_0";
    }
    return tbl[ccmux];
}

// Seems unused
const char* Interpreter::ACMUXtoStr(uint32_t acmux) {
    static constexpr const char* tbl[] = {
        "G_ACMUX_COMBINED or G_ACMUX_LOD_FRACTION",
        "G_ACMUX_TEXEL0",
        "G_ACMUX_TEXEL1",
        "G_ACMUX_PRIMITIVE",
        "G_ACMUX_SHADE",
        "G_ACMUX_ENVIRONMENT",
        "G_ACMUX_1 or G_ACMUX_PRIM_LOD_FRAC",
        "G_ACMUX_0",
    };
    return tbl[acmux];
}

void Interpreter::GenerateCC(ColorCombiner* comb, const ColorCombinerKey& key) {
    const bool is2Cyc = (key.options & SHADER_OPT(_2CYC)) != 0;

    uint8_t c[2][2][4];
    uint64_t shaderId0 = 0;
    uint64_t shaderId1 = key.options;
    uint8_t shaderInputMapping[2][7] = { { 0 } };
    bool usedTextures[2]{};
    for (uint32_t i = 0; i < 2 && (i == 0 || is2Cyc); i++) {
        uint32_t rgbA = (key.combine_mode >> (i * 28)) & 0xf;
        uint32_t rgbB = (key.combine_mode >> (i * 28 + 4)) & 0xf;
        uint32_t rgbC = (key.combine_mode >> (i * 28 + 8)) & 0x1f;
        uint32_t rgbD = (key.combine_mode >> (i * 28 + 13)) & 7;
        uint32_t alphaA = (key.combine_mode >> (i * 28 + 16)) & 7;
        uint32_t alphaB = (key.combine_mode >> (i * 28 + 16 + 3)) & 7;
        uint32_t alphaC = (key.combine_mode >> (i * 28 + 16 + 6)) & 7;
        uint32_t alphaD = (key.combine_mode >> (i * 28 + 16 + 9)) & 7;

        if (rgbA >= 8) {
            rgbA = G_CCMUX_0;
        }
        if (rgbB >= 8) {
            rgbB = G_CCMUX_0;
        }
        if (rgbC >= 16) {
            rgbC = G_CCMUX_0;
        }
        if (rgbD == 7) {
            rgbD = G_CCMUX_0;
        }

        if (rgbA == rgbB || rgbC == G_CCMUX_0) {
            // Normalize
            rgbA = G_CCMUX_0;
            rgbB = G_CCMUX_0;
            rgbC = G_CCMUX_0;
        }
        if (alphaA == alphaB || alphaC == G_ACMUX_0) {
            // Normalize
            alphaA = G_ACMUX_0;
            alphaB = G_ACMUX_0;
            alphaC = G_ACMUX_0;
        }
        if (i == 1) {
            if (rgbA != G_CCMUX_COMBINED && rgbB != G_CCMUX_COMBINED && rgbC != G_CCMUX_COMBINED &&
                rgbD != G_CCMUX_COMBINED) {
                // First cycle RGB not used, so clear it away
                c[0][0][0] = c[0][0][1] = c[0][0][2] = c[0][0][3] = G_CCMUX_0;
            }
            if (rgbC != G_CCMUX_COMBINED_ALPHA && alphaA != G_ACMUX_COMBINED && alphaB != G_ACMUX_COMBINED &&
                alphaD != G_ACMUX_COMBINED) {
                // First cycle ALPHA not used, so clear it away
                c[0][1][0] = c[0][1][1] = c[0][1][2] = c[0][1][3] = G_ACMUX_0;
            }
        }

        c[i][0][0] = rgbA;
        c[i][0][1] = rgbB;
        c[i][0][2] = rgbC;
        c[i][0][3] = rgbD;
        c[i][1][0] = alphaA;
        c[i][1][1] = alphaB;
        c[i][1][2] = alphaC;
        c[i][1][3] = alphaD;
    }
    if (!is2Cyc) {
        for (uint32_t i = 0; i < 2; i++) {
            for (uint32_t k = 0; k < 4; k++) {
                c[1][i][k] = i == 0 ? G_CCMUX_0 : G_ACMUX_0;
            }
        }

        // In 1-cycle mode, TEXEL1 returns the same value as TEXEL0.
        // Remap combiner inputs so the shader samples from the correct slot.
        // Ex: TEXEL1/TEXEL1_ALPHA → TEXEL0/TEXEL0_ALPHA
        for (uint32_t k = 0; k < 4; k++) {
            if (c[0][0][k] == G_CCMUX_TEXEL1)
                c[0][0][k] = G_CCMUX_TEXEL0;
            if (c[0][0][k] == G_CCMUX_TEXEL1_ALPHA)
                c[0][0][k] = G_CCMUX_TEXEL0_ALPHA;
            if (c[0][1][k] == G_ACMUX_TEXEL1)
                c[0][1][k] = G_ACMUX_TEXEL0;
        }
    }
    {
        uint8_t inputNumber[32] = { 0 };
        uint32_t nextInputNumber = SHADER_INPUT_1;
        for (uint32_t i = 0; i < 2 && (i == 0 || is2Cyc); i++) {
            for (uint32_t j = 0; j < 4; j++) {
                // Mux values 6/7/15 are overloaded by slot. Only B (value 6 = CENTER, 7 = K4)
                // and C (value 6 = SCALE, 15 = K5) carry chroma-key/convert inputs; A and D
                // reuse those values for unrelated constants.
                if (j == 1 && c[i][0][j] == G_CCMUX_CENTER) {
                    c[i][0][j] = G_CCMUX_KEY_CENTER;
                } else if (j == 1 && c[i][0][j] == G_CCMUX_K4) {
                    c[i][0][j] = G_CCMUX_CONVERT_K4;
                } else if (j == 2 && c[i][0][j] == G_CCMUX_SCALE) {
                    c[i][0][j] = G_CCMUX_KEY_SCALE;
                } else if (j == 2 && c[i][0][j] == G_CCMUX_K5) {
                    c[i][0][j] = G_CCMUX_CONVERT_K5;
                }
                uint32_t val = 0;
                switch (c[i][0][j]) {
                    case G_CCMUX_0:
                        val = SHADER_0;
                        break;
                    case G_CCMUX_1:
                        val = SHADER_1;
                        break;
                    case G_CCMUX_TEXEL0:
                        val = SHADER_TEXEL0;
                        // Set the opposite texture when reading from the second cycle color options
                        if (i == 0) {
                            usedTextures[0] = true;
                        } else {
                            usedTextures[1] = true;
                        }
                        break;
                    case G_CCMUX_TEXEL1:
                        val = SHADER_TEXEL1;
                        if (i == 0) {
                            usedTextures[1] = true;
                        } else {
                            usedTextures[0] = true;
                        }
                        break;
                    case G_CCMUX_TEXEL0_ALPHA:
                        val = SHADER_TEXEL0A;
                        if (i == 0) {
                            usedTextures[0] = true;
                        } else {
                            usedTextures[1] = true;
                        }
                        break;
                    case G_CCMUX_TEXEL1_ALPHA:
                        val = SHADER_TEXEL1A;
                        if (i == 0) {
                            usedTextures[1] = true;
                        } else {
                            usedTextures[0] = true;
                        }
                        break;
                    case G_CCMUX_NOISE:
                        val = SHADER_NOISE;
                        break;
                    case G_CCMUX_PRIMITIVE:
                    case G_CCMUX_PRIMITIVE_ALPHA:
                    case G_CCMUX_PRIM_LOD_FRAC:
                    case G_CCMUX_SHADE:
                    case G_CCMUX_ENVIRONMENT:
                    case G_CCMUX_ENV_ALPHA:
                    case G_CCMUX_LOD_FRACTION:
                    case G_CCMUX_KEY_CENTER:
                    case G_CCMUX_KEY_SCALE:
                    case G_CCMUX_CONVERT_K4:
                    case G_CCMUX_CONVERT_K5:
                        if (inputNumber[c[i][0][j]] == 0) {
                            shaderInputMapping[0][nextInputNumber - 1] = c[i][0][j];
                            inputNumber[c[i][0][j]] = nextInputNumber++;
                        }
                        val = inputNumber[c[i][0][j]];
                        break;
                    case G_CCMUX_COMBINED:
                        val = SHADER_COMBINED;
                        break;
                    default:
                        break;
                }
                shaderId0 |= (uint64_t)val << (i * 32 + j * 4);
            }
        }
    }
    {
        uint8_t inputNumber[16] = { 0 };
        uint32_t nextInputNumber = SHADER_INPUT_1;
        for (uint32_t i = 0; i < 2; i++) {
            for (uint32_t j = 0; j < 4; j++) {
                uint32_t val = 0;
                switch (c[i][1][j]) {
                    case G_ACMUX_0:
                        val = SHADER_0;
                        break;
                    case G_ACMUX_TEXEL0:
                        val = SHADER_TEXEL0;
                        // Set the opposite texture when reading from the second cycle color options
                        if (i == 0) {
                            usedTextures[0] = true;
                        } else {
                            usedTextures[1] = true;
                        }
                        break;
                    case G_ACMUX_TEXEL1:
                        val = SHADER_TEXEL1;
                        if (i == 0) {
                            usedTextures[1] = true;
                        } else {
                            usedTextures[0] = true;
                        }
                        break;
                    case G_ACMUX_LOD_FRACTION:
                        // case G_ACMUX_COMBINED: same numerical value
                        if (j != 2) {
                            val = SHADER_COMBINED;
                            break;
                        }
                        c[i][1][j] = G_CCMUX_LOD_FRACTION;
                        [[fallthrough]]; // for G_ACMUX_LOD_FRACTION
                    case G_ACMUX_1:
                        // case G_ACMUX_PRIM_LOD_FRAC: same numerical value
                        if (j != 2) {
                            val = SHADER_1;
                            break;
                        }
                        [[fallthrough]]; // for G_ACMUX_PRIM_LOD_FRAC
                    case G_ACMUX_PRIMITIVE:
                    case G_ACMUX_SHADE:
                    case G_ACMUX_ENVIRONMENT:
                        if (inputNumber[c[i][1][j]] == 0) {
                            shaderInputMapping[1][nextInputNumber - 1] = c[i][1][j];
                            inputNumber[c[i][1][j]] = nextInputNumber++;
                        }
                        val = inputNumber[c[i][1][j]];
                        break;
                }
                shaderId0 |= (uint64_t)val << (i * 32 + 16 + j * 4);
            }
        }
    }
    comb->shader_id0 = shaderId0;
    comb->shader_id1 = shaderId1;
    comb->usedTextures[0] = usedTextures[0];
    comb->usedTextures[1] = usedTextures[1];
    // comb->prg = gfx_lookup_or_create_mShaderProgram(shader_id0, shader_id1);
    memcpy(comb->shader_input_mapping, shaderInputMapping, sizeof(shaderInputMapping));
}

ColorCombiner* Interpreter::LookupOrCreateColorCombiner(const ColorCombinerKey& key) {
    if (mPrevCombiner != mColorCombinerPool.end() && mPrevCombiner->first == key) {
        return &mPrevCombiner->second;
    }
    mPrevCombiner = mColorCombinerPool.find(key);
    if (mPrevCombiner != mColorCombinerPool.end()) {
        return &mPrevCombiner->second;
    }
    Flush();
    mPrevCombiner = mColorCombinerPool.insert(std::make_pair(key, ColorCombiner())).first;
    GenerateCC(&mPrevCombiner->second, key);
    return &mPrevCombiner->second;
}

void Interpreter::TextureCacheClear() {
    for (const auto& entry : mTextureCache.map) {
        mTextureCache.free_texture_ids.push_back(entry.second.texture_id);
    }
    mTextureCache.map.clear();
    mTextureCache.lru.clear();
    // Pre-allocate buckets so the map never rehashes during normal operation.
    // Rehashing invalidates all iterators, including those stored in LRU entries.
    mTextureCache.map.reserve(TEXTURE_CACHE_MAX_SIZE);
    // Null rendering-state pointers — they pointed into map nodes that are now freed.
    std::fill(std::begin(mRenderingState.mTextures), std::end(mRenderingState.mTextures), nullptr);
}

void Interpreter::ShaderCacheClear() {
    mRapi->ClearShaderCache();
}

bool Interpreter::TextureCacheLookup(int i, const TextureCacheKey& key) {
    TextureCacheMap::iterator it = mTextureCache.map.find(key);
    TextureCacheNode** n = &mRenderingState.mTextures[i];

    if (it != mTextureCache.map.end()) {
        mRapi->SelectTexture(i, it->second.texture_id);
        *n = &*it;
        mTextureCache.lru.splice(mTextureCache.lru.end(), mTextureCache.lru,
                                 it->second.lru_location); // move to back
        return true;
    }

    if (mTextureCache.map.size() >= TEXTURE_CACHE_MAX_SIZE) {
        // Remove the texture that was least recently used
        it = mTextureCache.lru.front().it;
        mTextureCache.free_texture_ids.push_back(it->second.texture_id);
        for (int j = 0; j < SHADER_MAX_TEXTURES; j++) {
            if (mRenderingState.mTextures[j] == &*it)
                mRenderingState.mTextures[j] = nullptr;
        }
        mTextureCache.map.erase(it);
        mTextureCache.lru.pop_front();
    }

    uint32_t texture_id;
    if (!mTextureCache.free_texture_ids.empty()) {
        texture_id = mTextureCache.free_texture_ids.back();
        mTextureCache.free_texture_ids.pop_back();
    } else {
        texture_id = mRapi->NewTexture();
    }

    it = mTextureCache.map.insert(std::make_pair(key, TextureCacheValue())).first;
    TextureCacheNode* node = &*it;
    node->second.texture_id = texture_id;
    node->second.lru_location = mTextureCache.lru.insert(mTextureCache.lru.end(), { it });

    mRapi->SelectTexture(i, texture_id);
    mRapi->SetSamplerParameters(i, false, 0, 0);
    *n = node;
    return false;
}

#define GetBaseTexturePath(x) (x)

void Interpreter::TextureCacheDelete(const uint8_t* origAddr) {
    while (mTextureCache.map.bucket_count() > 0) {
        TextureCacheKey key = { origAddr, { 0 }, 0, 0, 0 }; // bucket index only depends on the address
        size_t bucket = mTextureCache.map.bucket(key);
        bool again = false;
        for (auto it = mTextureCache.map.begin(bucket); it != mTextureCache.map.end(bucket); ++it) {
            if (it->first.texture_addr == origAddr) {
                for (int j = 0; j < SHADER_MAX_TEXTURES; j++) {
                    if (mRenderingState.mTextures[j] == &*it)
                        mRenderingState.mTextures[j] = nullptr;
                }
                mTextureCache.lru.erase(it->second.lru_location);
                mTextureCache.free_texture_ids.push_back(it->second.texture_id);
                mTextureCache.map.erase(it->first);
                again = true;
                break;
            }
        }
        if (!again) {
            break;
        }
    }
}

// Pick the per-line byte width for texture decode. Prefer the DRAM stride from
// loaded_texture when it looks like real per-line info (differs from total size).
// Fall back to the TMEM tile stride when loaded sizes match total (LoadBlock with
// width=1, where line_size == full_image_line_size == size).
static uint32_t GetEffectiveLineSize(uint32_t lineSizeBytes, uint32_t fullImageLineSizeBytes, uint32_t sizeBytes,
                                     uint32_t tileLineSizeBytes) {
    if ((lineSizeBytes != sizeBytes || fullImageLineSizeBytes != sizeBytes) && lineSizeBytes > 0) {
        return lineSizeBytes;
    }
    return tileLineSizeBytes;
}

void Interpreter::ImportTextureRgba16(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t line_size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t widthBytes = GetEffectiveLineSize(line_size_bytes, fullImageLineSizeBytes, sizeBytes,
                                               mRdp->texture_tile[tile].line_size_bytes);
    uint32_t width = widthBytes / 2;
    uint32_t height = widthBytes > 0 ? sizeBytes / widthBytes : 0;

    // Clamp to the rendered region only when the loaded buffer is ~1.33x of it (mipmap
    // pyramid signature). Window-scrolling tiles have loaded ≈ rendered or loaded >> rendered;
    // skip both. CLAMP wrap mode always opts in.
    uint32_t tile_w = (uint32_t)((mRdp->texture_tile[tile].lrs - mRdp->texture_tile[tile].uls + 4) / 4);
    uint32_t tile_h = (uint32_t)((mRdp->texture_tile[tile].lrt - mRdp->texture_tile[tile].ult + 4) / 4);
    uint32_t loadedPixels = width * height;
    uint32_t renderedPixels = tile_w * tile_h;
    bool pyramidLike =
        renderedPixels > 0 && loadedPixels > renderedPixels && loadedPixels * 8 < renderedPixels * 13; // < 1.625x
    bool clampS = (mRdp->texture_tile[tile].cms & G_TX_CLAMP) != 0;
    bool clampT = (mRdp->texture_tile[tile].cmt & G_TX_CLAMP) != 0;
    if ((pyramidLike || clampS) && tile_w > 0 && tile_w < width) {
        width = tile_w;
    }
    if ((pyramidLike || clampT) && tile_h > 0 && tile_h < height) {
        height = tile_h;
    }

    // A single line of pixels should not equal the entire image (height == 1 non-withstanding)
    if (fullImageLineSizeBytes == sizeBytes) {
        fullImageLineSizeBytes = width * 2;
    }

    uint32_t i = 0;

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t clrIdx = (y * (fullImageLineSizeBytes / 2)) + (x);

            uint16_t col16 = (addr[2 * clrIdx] << 8) | addr[2 * clrIdx + 1];
            uint8_t a = col16 & 1;
            uint8_t r = col16 >> 11;
            uint8_t g = (col16 >> 6) & 0x1f;
            uint8_t b = (col16 >> 1) & 0x1f;
            mTexUploadBuffer[4 * i + 0] = SCALE_5_8(r);
            mTexUploadBuffer[4 * i + 1] = SCALE_5_8(g);
            mTexUploadBuffer[4 * i + 2] = SCALE_5_8(b);
            mTexUploadBuffer[4 * i + 3] = a ? 255 : 0;

            i++;
        }
    }

    mRapi->UploadTexture(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureRgba32(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        return;
    }

    uint32_t size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t full_image_line_size_bytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t line_size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t widthBytes = GetEffectiveLineSize(line_size_bytes, full_image_line_size_bytes, size_bytes,
                                               mRdp->texture_tile[tile].line_size_bytes * 2);
    uint32_t width = widthBytes / 4;
    uint32_t height = widthBytes > 0 ? size_bytes / widthBytes : 0;

    // Clamp to the rendered region only when the loaded buffer is ~1.33x of it (mipmap
    // pyramid signature). Window-scrolling tiles have loaded ≈ rendered or loaded >> rendered;
    // skip both. CLAMP wrap mode always opts in.
    uint32_t tile_w = (uint32_t)((mRdp->texture_tile[tile].lrs - mRdp->texture_tile[tile].uls + 4) / 4);
    uint32_t tile_h = (uint32_t)((mRdp->texture_tile[tile].lrt - mRdp->texture_tile[tile].ult + 4) / 4);
    uint32_t loadedPixels = width * height;
    uint32_t renderedPixels = tile_w * tile_h;
    bool pyramidLike = renderedPixels > 0 && loadedPixels > renderedPixels && loadedPixels * 8 < renderedPixels * 13;
    bool clampS = (mRdp->texture_tile[tile].cms & G_TX_CLAMP) != 0;
    bool clampT = (mRdp->texture_tile[tile].cmt & G_TX_CLAMP) != 0;
    if ((pyramidLike || clampS) && tile_w > 0 && tile_w < width) {
        width = tile_w;
    }
    if ((pyramidLike || clampT) && tile_h > 0 && tile_h < height) {
        height = tile_h;
    }

    if (full_image_line_size_bytes == size_bytes) {
        full_image_line_size_bytes = width * 4;
    }

    // Copy pixel by pixel, respecting full image stride (handles sub-tile loads)
    uint32_t fullImageStridePixels = full_image_line_size_bytes / 4;
    uint32_t i = 0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t srcIdx = y * fullImageStridePixels + x;
            mTexUploadBuffer[4 * i + 0] = addr[4 * srcIdx + 0];
            mTexUploadBuffer[4 * i + 1] = addr[4 * srcIdx + 1];
            mTexUploadBuffer[4 * i + 2] = addr[4 * srcIdx + 2];
            mTexUploadBuffer[4 * i + 3] = addr[4 * srcIdx + 3];
            i++;
        }
    }
    mRapi->UploadTexture(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureIA4(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t lineSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t widthBytes = GetEffectiveLineSize(lineSizeBytes, fullImageLineSizeBytes, sizeBytes,
                                               mRdp->texture_tile[tile].line_size_bytes);
    uint32_t width = widthBytes * 2;
    uint32_t height = widthBytes > 0 ? sizeBytes / widthBytes : 0;

    if (fullImageLineSizeBytes == sizeBytes) {
        fullImageLineSizeBytes = widthBytes;
    }

    uint32_t i = 0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t srcPixelIdx = y * (fullImageLineSizeBytes * 2) + x;
            uint8_t byte = addr[srcPixelIdx / 2];
            uint8_t part = (byte >> (4 - (srcPixelIdx % 2) * 4)) & 0xf;
            uint8_t intensity = part >> 1;
            uint8_t alpha = part & 1;
            mTexUploadBuffer[4 * i + 0] = SCALE_3_8(intensity);
            mTexUploadBuffer[4 * i + 1] = SCALE_3_8(intensity);
            mTexUploadBuffer[4 * i + 2] = SCALE_3_8(intensity);
            mTexUploadBuffer[4 * i + 3] = alpha ? 255 : 0;
            i++;
        }
    }

    mRapi->UploadTexture(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureIA8(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t lineSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t width = GetEffectiveLineSize(lineSizeBytes, fullImageLineSizeBytes, sizeBytes,
                                          mRdp->texture_tile[tile].line_size_bytes);
    uint32_t height = width > 0 ? sizeBytes / width : 0;

    if (fullImageLineSizeBytes == sizeBytes) {
        fullImageLineSizeBytes = width;
    }

    uint32_t i = 0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t srcIdx = y * fullImageLineSizeBytes + x;
            uint8_t intensity = addr[srcIdx] >> 4;
            uint8_t alpha = addr[srcIdx] & 0xf;
            mTexUploadBuffer[4 * i + 0] = SCALE_4_8(intensity);
            mTexUploadBuffer[4 * i + 1] = SCALE_4_8(intensity);
            mTexUploadBuffer[4 * i + 2] = SCALE_4_8(intensity);
            mTexUploadBuffer[4 * i + 3] = SCALE_4_8(alpha);
            i++;
        }
    }

    mRapi->UploadTexture(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureIA16(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        return;
    }

    uint32_t size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t full_image_line_size_bytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t line_size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t widthBytes = GetEffectiveLineSize(line_size_bytes, full_image_line_size_bytes, size_bytes,
                                               mRdp->texture_tile[tile].line_size_bytes);
    uint32_t width = widthBytes / 2;
    uint32_t height = widthBytes > 0 ? size_bytes / widthBytes : 0;

    // A single line of pixels should not equal the entire image (height == 1 non-withstanding)
    if (full_image_line_size_bytes == size_bytes) {
        full_image_line_size_bytes = width * 2;
    }

    uint32_t i = 0;

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t clrIdx = (y * (full_image_line_size_bytes / 2)) + (x);

            uint8_t intensity = addr[2 * clrIdx];
            uint8_t alpha = addr[2 * clrIdx + 1];
            uint8_t r = intensity;
            uint8_t g = intensity;
            uint8_t b = intensity;
            mTexUploadBuffer[4 * i + 0] = r;
            mTexUploadBuffer[4 * i + 1] = g;
            mTexUploadBuffer[4 * i + 2] = b;
            mTexUploadBuffer[4 * i + 3] = alpha;

            i++;
        }
    }

    mRapi->UploadTexture(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureI4(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t lineSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t widthBytes = GetEffectiveLineSize(lineSizeBytes, fullImageLineSizeBytes, sizeBytes,
                                               mRdp->texture_tile[tile].line_size_bytes);
    uint32_t width = widthBytes * 2;
    uint32_t height = widthBytes > 0 ? sizeBytes / widthBytes : 0;

    // A single line of pixels should not equal the entire image (height == 1 non-withstanding)
    if (fullImageLineSizeBytes == sizeBytes) {
        fullImageLineSizeBytes = width / 2;
    }

    uint32_t i = 0;

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t clrIdx = (y * (fullImageLineSizeBytes * 2)) + (x);

            uint8_t byte = addr[clrIdx / 2];
            uint8_t part = (byte >> (4 - (clrIdx % 2) * 4)) & 0xf;
            uint8_t intensity = part;
            uint8_t r = intensity;
            uint8_t g = intensity;
            uint8_t b = intensity;
            uint8_t a = intensity;
            mTexUploadBuffer[4 * i + 0] = SCALE_4_8(r);
            mTexUploadBuffer[4 * i + 1] = SCALE_4_8(g);
            mTexUploadBuffer[4 * i + 2] = SCALE_4_8(b);
            mTexUploadBuffer[4 * i + 3] = SCALE_4_8(a);

            i++;
        }
    }

    mRapi->UploadTexture(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureI8(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t lineSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t width = GetEffectiveLineSize(lineSizeBytes, fullImageLineSizeBytes, sizeBytes,
                                          mRdp->texture_tile[tile].line_size_bytes);
    uint32_t height = width > 0 ? sizeBytes / width : 0;

    if (fullImageLineSizeBytes == sizeBytes) {
        fullImageLineSizeBytes = width;
    }

    uint32_t i = 0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint8_t intensity = addr[y * fullImageLineSizeBytes + x];
            mTexUploadBuffer[4 * i + 0] = intensity;
            mTexUploadBuffer[4 * i + 1] = intensity;
            mTexUploadBuffer[4 * i + 2] = intensity;
            mTexUploadBuffer[4 * i + 3] = intensity;
            i++;
        }
    }

    mRapi->UploadTexture(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureCi4(int tile, bool importReplacement) {
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t lineSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;
    uint32_t palIdx = mRdp->texture_tile[tile].palette; // 0-15

    const uint8_t* palette;

    if (mRdp->palettes[palIdx / 8] == nullptr) {
        return;
    }
    palette = mRdp->palettes[palIdx / 8] + (palIdx % 8) * 16 * 2;

    uint32_t baseLineSizeBytes = GetEffectiveLineSize(lineSizeBytes, fullImageLineSizeBytes, sizeBytes,
                                                      mRdp->texture_tile[tile].line_size_bytes);
    uint32_t resultLineSizeBytes = baseLineSizeBytes;

    if (metadata->h_byte_scale != 1) {
        resultLineSizeBytes *= metadata->h_byte_scale;
    }

    // CI4: 2 pixels per byte
    uint32_t width = resultLineSizeBytes * 2;
    uint32_t height = resultLineSizeBytes > 0 ? sizeBytes / resultLineSizeBytes : 0;

    // Clamp to the rendered region only when the loaded buffer is ~1.33x of it (mipmap
    // pyramid signature). Window-scrolling tiles have loaded ≈ rendered or loaded >> rendered;
    // skip both. CLAMP wrap mode always opts in.
    uint32_t tile_w = (uint32_t)((mRdp->texture_tile[tile].lrs - mRdp->texture_tile[tile].uls + 4) / 4);
    uint32_t tile_h = (uint32_t)((mRdp->texture_tile[tile].lrt - mRdp->texture_tile[tile].ult + 4) / 4);
    uint32_t loadedPixels = width * height;
    uint32_t renderedPixels = tile_w * tile_h;
    bool pyramidLike = renderedPixels > 0 && loadedPixels > renderedPixels && loadedPixels * 8 < renderedPixels * 13;
    bool clampS = (mRdp->texture_tile[tile].cms & G_TX_CLAMP) != 0;
    bool clampT = (mRdp->texture_tile[tile].cmt & G_TX_CLAMP) != 0;
    if ((pyramidLike || clampS) && tile_w > 0 && tile_w < width) {
        width = tile_w;
    }
    if ((pyramidLike || clampT) && tile_h > 0 && tile_h < height) {
        height = tile_h;
    }

    if (fullImageLineSizeBytes == sizeBytes) {
        fullImageLineSizeBytes = resultLineSizeBytes;
    }

    uint32_t i = 0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t srcPixelIdx = y * (fullImageLineSizeBytes * 2) + x;
            uint8_t byte = addr[srcPixelIdx / 2];
            uint8_t idx = (byte >> (4 - (srcPixelIdx % 2) * 4)) & 0xf;
            uint16_t col16 = (palette[idx * 2] << 8) | palette[idx * 2 + 1]; // Big endian load
            uint8_t a = col16 & 1;
            uint8_t r = col16 >> 11;
            uint8_t g = (col16 >> 6) & 0x1f;
            uint8_t b = (col16 >> 1) & 0x1f;
            mTexUploadBuffer[4 * i + 0] = SCALE_5_8(r);
            mTexUploadBuffer[4 * i + 1] = SCALE_5_8(g);
            mTexUploadBuffer[4 * i + 2] = SCALE_5_8(b);
            mTexUploadBuffer[4 * i + 3] = a ? 255 : 0;
            i++;
        }
    }

    mRapi->UploadTexture(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureCi8(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t lineSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    if (mRdp->palettes[0] == nullptr || mRdp->palettes[1] == nullptr) {
        return;
    }

    for (uint32_t i = 0, j = 0; i < sizeBytes; j += fullImageLineSizeBytes - lineSizeBytes) {
        for (uint32_t k = 0; k < lineSizeBytes; i++, k++, j++) {
            uint8_t idx = addr[j];
            uint16_t col16 = (mRdp->palettes[idx / 128][(idx % 128) * 2] << 8) |
                             mRdp->palettes[idx / 128][(idx % 128) * 2 + 1]; // Big endian load
            uint8_t a = col16 & 1;
            uint8_t r = col16 >> 11;
            uint8_t g = (col16 >> 6) & 0x1f;
            uint8_t b = (col16 >> 1) & 0x1f;
            mTexUploadBuffer[4 * i + 0] = SCALE_5_8(r);
            mTexUploadBuffer[4 * i + 1] = SCALE_5_8(g);
            mTexUploadBuffer[4 * i + 2] = SCALE_5_8(b);
            mTexUploadBuffer[4 * i + 3] = a ? 255 : 0;
        }
    }

    uint32_t baseLineSizeBytes = GetEffectiveLineSize(lineSizeBytes, fullImageLineSizeBytes, sizeBytes,
                                                      mRdp->texture_tile[tile].line_size_bytes);
    uint32_t resultLineSizeBytes = baseLineSizeBytes;
    if (metadata->h_byte_scale != 1) {
        resultLineSizeBytes *= metadata->h_byte_scale;
    }

    uint32_t width = resultLineSizeBytes;
    uint32_t height = resultLineSizeBytes > 0 ? sizeBytes / resultLineSizeBytes : 0;

    // Clamp to the rendered region only when the loaded buffer is ~1.33x of it (mipmap
    // pyramid signature). Window-scrolling tiles have loaded ≈ rendered or loaded >> rendered;
    // skip both. CLAMP wrap mode always opts in.
    uint32_t tile_w = (uint32_t)((mRdp->texture_tile[tile].lrs - mRdp->texture_tile[tile].uls + 4) / 4);
    uint32_t tile_h = (uint32_t)((mRdp->texture_tile[tile].lrt - mRdp->texture_tile[tile].ult + 4) / 4);
    uint32_t loadedPixels = width * height;
    uint32_t renderedPixels = tile_w * tile_h;
    bool pyramidLike = renderedPixels > 0 && loadedPixels > renderedPixels && loadedPixels * 8 < renderedPixels * 13;
    bool clampS = (mRdp->texture_tile[tile].cms & G_TX_CLAMP) != 0;
    bool clampT = (mRdp->texture_tile[tile].cmt & G_TX_CLAMP) != 0;
    if ((pyramidLike || clampS) && tile_w > 0 && tile_w < width) {
        width = tile_w;
    }
    if ((pyramidLike || clampT) && tile_h > 0 && tile_h < height) {
        height = tile_h;
    }

    mRapi->UploadTexture(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureImg(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        return;
    }

    uint16_t width = metadata->width;
    uint16_t height = metadata->height;
    mRapi->UploadTexture(addr, width, height);
}

void Interpreter::ImportTextureRaw(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        return;
    }

    uint16_t width = metadata->width;
    uint16_t height = metadata->height;
    Fast::TextureType type = metadata->type;
    std::shared_ptr<Fast::Texture> resource = metadata->resource;

    // if texture type is CI4 or CI8 we need to apply tlut to it
    switch (type) {
        case Fast::TextureType::Palette4bpp:
            ImportTextureCi4(tile, importReplacement);
            return;
        case Fast::TextureType::Palette8bpp:
            ImportTextureCi8(tile, importReplacement);
            return;
        default:
            break;
    }

    uint32_t numLoadedBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t numOriginallyLoadedBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].orig_size_bytes;

    uint32_t resultOrigLineSize = mRdp->texture_tile[tile].line_size_bytes;
    switch (mRdp->texture_tile[tile].siz) {
        case G_IM_SIZ_32b:
            resultOrigLineSize *= 2;
            break;
    }
    uint32_t resultOrigHeight = numOriginallyLoadedBytes / resultOrigLineSize;
    uint32_t resultNewLineSize = resultOrigLineSize * metadata->h_byte_scale;
    uint32_t resultNewHeight = resultOrigHeight * metadata->v_pixel_scale;

    if (resultNewLineSize == 4 * width && resultNewHeight == height) {
        // Can use the texture directly since it has the correct dimensions
        mRapi->UploadTexture(addr, width, height);
        return;
    }

    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t line_size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    // Get the resource's true image size
    uint32_t resourceImageSizeBytes = resource->ImageDataSize;
    uint32_t safeFullImageLineSizeBytes = fullImageLineSizeBytes;
    uint32_t safeLineSizeBytes = line_size_bytes;
    uint32_t safeLoadedBytes = numLoadedBytes;

    // Sometimes the texture load commands will specify a size larger than the authentic texture
    // Normally the OOB info is read as garbage, but will cause a crash on some platforms
    // Restrict the bytes to a safe amount
    if (numLoadedBytes > resourceImageSizeBytes) {
        safeLoadedBytes = resourceImageSizeBytes;
        safeLineSizeBytes = resourceImageSizeBytes;
        safeFullImageLineSizeBytes = resourceImageSizeBytes;
    }

    // Safely only copy the amount of bytes the resource can allow
    for (uint32_t i = 0, j = 0; i < safeLoadedBytes; i += safeLineSizeBytes, j += safeFullImageLineSizeBytes) {
        memcpy(mTexUploadBuffer + i, addr + j, safeLineSizeBytes);
    }

    // Set the remaining bytes to load as 0
    if (numLoadedBytes > resourceImageSizeBytes) {
        memset(mTexUploadBuffer + resourceImageSizeBytes, 0, numLoadedBytes - resourceImageSizeBytes);
    }

    mRapi->UploadTexture(mTexUploadBuffer, resultNewLineSize / 4, resultNewHeight);
}

bool Interpreter::PrepareTextureView(int tile) {
    if (tile < 0 || tile >= 8) {
        return false;
    }

    const uint16_t requestedTmem = mRdp->texture_tile[tile].tmem;
    const uint32_t targetIndex = mRdp->texture_tile[tile].tmem_index;
    if (mRdp->loaded_texture[targetIndex].addr != nullptr &&
        mRdp->loaded_texture[targetIndex].tmem == requestedTmem &&
        !mRdp->loaded_texture[targetIndex].derived_view) {
        return true;
    }

    for (uint32_t sourceIndex = 0; sourceIndex < 2; ++sourceIndex) {
        const auto source = mRdp->loaded_texture[sourceIndex];
        if (source.addr == nullptr || source.orig_size_bytes == 0 || source.derived_view) {
            continue;
        }

        const uint32_t sourceWords = (source.orig_size_bytes + 7) >> 3;
        const uint32_t sourceEnd = static_cast<uint32_t>(source.tmem) + sourceWords;
        if (requestedTmem < source.tmem || requestedTmem >= sourceEnd) {
            continue;
        }

        const uint32_t sourceOffsetBytes = (static_cast<uint32_t>(requestedTmem) - source.tmem) << 3;
        if (sourceOffsetBytes >= source.orig_size_bytes) {
            continue;
        }

        uint32_t viewOrigSize = source.orig_size_bytes - sourceOffsetBytes;
        uint32_t viewLineSize = mRdp->texture_tile[tile].line_size_bytes;
        if (mRdp->texture_tile[tile].siz == G_IM_SIZ_32b) {
            viewLineSize *= 2;
        }

        uint32_t viewHeight = 0;
        if (mRdp->texture_tile[tile].lrt >= mRdp->texture_tile[tile].ult) {
            viewHeight = static_cast<uint32_t>(
                (mRdp->texture_tile[tile].lrt - mRdp->texture_tile[tile].ult + 4.0f) / 4.0f);
        }
        if (viewHeight <= 1 && mRdp->texture_tile[tile].maskt != 0) {
            viewHeight = 1u << mRdp->texture_tile[tile].maskt;
        }

        if (viewLineSize != 0 && viewHeight != 0) {
            const uint64_t requestedSize = static_cast<uint64_t>(viewLineSize) * viewHeight;
            if (requestedSize <= viewOrigSize) {
                viewOrigSize = static_cast<uint32_t>(requestedSize);
            }
        }

        uint32_t scaledOffsetBytes = sourceOffsetBytes;
        uint32_t viewSize = viewOrigSize;
        uint32_t scaledLineSize = viewLineSize;
        if (source.orig_size_bytes != 0 && source.size_bytes != source.orig_size_bytes) {
            scaledOffsetBytes = static_cast<uint32_t>(
                (static_cast<uint64_t>(sourceOffsetBytes) * source.size_bytes) / source.orig_size_bytes);
            viewSize = static_cast<uint32_t>(
                (static_cast<uint64_t>(viewOrigSize) * source.size_bytes) / source.orig_size_bytes);
            scaledLineSize = static_cast<uint32_t>(
                (static_cast<uint64_t>(viewLineSize) * source.size_bytes) / source.orig_size_bytes);
        }

        auto view = source;
        view.addr = source.addr + scaledOffsetBytes;
        view.orig_size_bytes = viewOrigSize;
        view.size_bytes = viewSize;
        view.tmem = requestedTmem;
        view.derived_view = true;
        if (scaledLineSize != 0) {
            view.line_size_bytes = scaledLineSize;
            view.full_image_line_size_bytes = scaledLineSize;
        }
        mRdp->loaded_texture[targetIndex] = view;

        if (mRdp->texture_tile[tile].lrs == mRdp->texture_tile[tile].uls &&
            mRdp->texture_tile[tile].masks != 0) {
            mRdp->texture_tile[tile].lrs = mRdp->texture_tile[tile].uls +
                                           static_cast<float>(((1u << mRdp->texture_tile[tile].masks) - 1u) << 2);
        }
        if (mRdp->texture_tile[tile].lrt == mRdp->texture_tile[tile].ult &&
            mRdp->texture_tile[tile].maskt != 0) {
            mRdp->texture_tile[tile].lrt = mRdp->texture_tile[tile].ult +
                                           static_cast<float>(((1u << mRdp->texture_tile[tile].maskt) - 1u) << 2);
        }

        return true;
    }

    return false;
}

void Interpreter::ImportTexture(int i, int tile, bool importReplacement) {
    if (!PrepareTextureView(tile)) {
        return;
    }

    uint8_t fmt = mRdp->texture_tile[tile].fmt;
    uint8_t siz = mRdp->texture_tile[tile].siz;
    uint32_t texFlags = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].tex_flags;
    uint32_t tmemIdex = mRdp->texture_tile[tile].tmem_index;
    uint8_t paletteIndex = mRdp->texture_tile[tile].palette;
    uint32_t origSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].orig_size_bytes;

    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* origAddr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[tmemIdex].addr;

    // Check if this texture address is a registered GPU framebuffer mirror.
    // If so, bind the GPU FB directly — full resolution, no CPU readback needed.
    if (origAddr != nullptr && !importReplacement) {
        auto fbIt = mFbTextures.find((uintptr_t)origAddr);
        if (fbIt != mFbTextures.end()) {
            Flush();
            mRapi->SelectTextureFb(fbIt->second);
            mRdp->textures_changed[i] = false;
            return;
        }
    }

    if (origAddr == nullptr) {
        // Try the other TMEM slot -- some multi-tile setups only load one slot
        // and expect both tiles to reference it.
        uint32_t otherTmem = tmemIdex ^ 1;
        origAddr = mRdp->loaded_texture[otherTmem].addr;
        if (origAddr == nullptr) {
            return;
        }
        tmemIdex = otherTmem;
        origSizeBytes = mRdp->loaded_texture[otherTmem].orig_size_bytes;
        texFlags = mRdp->loaded_texture[otherTmem].tex_flags;
    }

    // Use palette_dram_addr (the original DRAM source) instead of palettes[]
    // (which always points to the staging buffer) so the same texture drawn
    // with different palettes gets distinct cache entries.
    TextureCacheKey key;
    if (fmt == G_IM_FMT_CI) {
        if (siz == G_IM_SIZ_4b) {
            uint8_t palSlot = paletteIndex / 8;
            key = { origAddr,
                    { palSlot == 0 ? mRdp->palette_dram_addr[0] : nullptr,
                      palSlot == 1 ? mRdp->palette_dram_addr[1] : nullptr },
                    fmt,
                    siz,
                    paletteIndex,
                    origSizeBytes };
        } else {
            // CI8 uses both palette halves
            key = { origAddr,     { mRdp->palette_dram_addr[0], mRdp->palette_dram_addr[1] }, fmt, siz, paletteIndex,
                    origSizeBytes };
        }
    } else {
        key = { origAddr, {}, fmt, siz, paletteIndex, origSizeBytes };
    }

    if (TextureCacheLookup(i, key)) {
        return;
    }

    // Guard against zero-sized textures that would cause divide-by-zero
    // or GPU API errors in UploadTexture.
    if (mRdp->texture_tile[tile].line_size_bytes == 0 || mRdp->loaded_texture[tmemIdex].size_bytes == 0 ||
        origAddr == nullptr) {
        return;
    }

    if ((texFlags & TEX_FLAG_LOAD_AS_IMG) != 0) {
        ImportTextureImg(tile, importReplacement);
        return;
    }

    // if load as raw is set then we load_raw();
    if ((texFlags & TEX_FLAG_LOAD_AS_RAW) != 0) {
        ImportTextureRaw(tile, importReplacement);
        return;
    }

    switch (fmt) {
        case G_IM_FMT_RGBA:
            if (siz == G_IM_SIZ_16b) {
                ImportTextureRgba16(tile, importReplacement);
            } else if (siz == G_IM_SIZ_32b) {
                ImportTextureRgba32(tile, importReplacement);
            }
            break;
        case G_IM_FMT_IA:
            if (siz == G_IM_SIZ_4b) {
                ImportTextureIA4(tile, importReplacement);
            } else if (siz == G_IM_SIZ_8b) {
                ImportTextureIA8(tile, importReplacement);
            } else if (siz == G_IM_SIZ_16b) {
                ImportTextureIA16(tile, importReplacement);
            } else {
                ;
            }
            break;
        case G_IM_FMT_CI:
            if (siz == G_IM_SIZ_4b) {
                ImportTextureCi4(tile, importReplacement);
            } else if (siz == G_IM_SIZ_8b) {
                ImportTextureCi8(tile, importReplacement);
            } else if (siz == G_IM_SIZ_16b) {
                // CI+16b is hardware-invalid on N64. The tile's fmt is likely
                // stale from a prior draw. Decode as RGBA16 instead.
                ImportTextureRgba16(tile, importReplacement);
            } else if (siz == G_IM_SIZ_32b) {
                ImportTextureRgba32(tile, importReplacement);
            } else {
            }
            break;
        case G_IM_FMT_I:
            if (siz == G_IM_SIZ_4b) {
                ImportTextureI4(tile, importReplacement);
            } else if (siz == G_IM_SIZ_8b) {
                ImportTextureI8(tile, importReplacement);
            } else {
            }
            break;
        case G_IM_FMT_YUV:
            break;
        default:
            break;
    }
}

void Interpreter::ImportTextureMask(int i, int tile) {
    uint32_t tmemIndex = mRdp->texture_tile[tile].tmem_index;
    RawTexMetadata metadata = mRdp->loaded_texture[tmemIndex].raw_tex_metadata;

    if (metadata.resource == nullptr) {
        return;
    }

    auto maskIter = mMaskedTextures.find(GetBaseTexturePath(metadata.resource->GetInitData()->Path));
    if (maskIter == mMaskedTextures.end()) {
        return;
    }

    const uint8_t* orig_addr = maskIter->second.mask;

    if (orig_addr == nullptr) {
        return;
    }

    TextureCacheKey key = { orig_addr, {}, 0, 0, 0, 0 };

    if (TextureCacheLookup(i, key)) {
        return;
    }

    uint32_t width = mRdp->texture_tile[tile].line_size_bytes;
    uint32_t height = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].orig_size_bytes /
                      mRdp->texture_tile[tile].line_size_bytes;
    switch (mRdp->texture_tile[tile].siz) {
        case G_IM_SIZ_4b:
            width *= 2;
            break;
        case G_IM_SIZ_8b:
        default:
            break;
        case G_IM_SIZ_16b:
            width /= 2;
            break;
        case G_IM_SIZ_32b:
            width /= 4;
            break;
    }

    for (uint32_t texIndex = 0; texIndex < width * height; texIndex++) {
        uint8_t masked = orig_addr[texIndex];
        if (masked) {
            mTexUploadBuffer[4 * texIndex + 0] = 0;
            mTexUploadBuffer[4 * texIndex + 1] = 0;
            mTexUploadBuffer[4 * texIndex + 2] = 0;
            mTexUploadBuffer[4 * texIndex + 3] = 0xFF;
        } else {
            mTexUploadBuffer[4 * texIndex + 0] = 0;
            mTexUploadBuffer[4 * texIndex + 1] = 0;
            mTexUploadBuffer[4 * texIndex + 2] = 0;
            mTexUploadBuffer[4 * texIndex + 3] = 0;
        }
    }

    mRapi->UploadTexture(mTexUploadBuffer, width, height);
}

#ifdef __vita__
extern "C" {
    void normalize3_neon(float v[3], float d[3]);
    void matmul4_neon(float m0[16], float m1[16], float d[16]);
};
#endif

void Interpreter::NormalizeVector(float v[3]) {
#ifdef __vita__
	normalize3_neon(v, v);
#else
    float s = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] /= s;
    v[1] /= s;
    v[2] /= s;
#endif
}

void Interpreter::TransposedMatrixMul(float res[3], const float a[3], const float b[4][4]) {
    res[0] = a[0] * b[0][0] + a[1] * b[0][1] + a[2] * b[0][2];
    res[1] = a[0] * b[1][0] + a[1] * b[1][1] + a[2] * b[1][2];
    res[2] = a[0] * b[2][0] + a[1] * b[2][1] + a[2] * b[2][2];
}

void Interpreter::MatrixMul(float res[4][4], const float a[4][4], const float b[4][4]) {
#ifdef __vita__
	matmul4_neon((float *)b, (float *)a, (float *)res);
#else
    float tmp[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            tmp[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
        }
    }
    memcpy(res, tmp, sizeof(tmp));
#endif
}

void Interpreter::CalculateNormalDir(const F3DLight_t* light, float coeffs[3]) {
    float light_dir[3] = { light->dir[0] / 127.0f, light->dir[1] / 127.0f, light->dir[2] / 127.0f };

    Interpreter::TransposedMatrixMul(coeffs, light_dir,
                                     mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1]);
    Interpreter::NormalizeVector(coeffs);
}

void Interpreter::GfxSpMatrix(uint8_t parameters, const int32_t* addr) {
    float matrix[4][4];

    auto matrixReplacement = mCurMtxReplacements->find((Mtx*)addr);
    if (matrixReplacement != mCurMtxReplacements->end()) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                float v = matrixReplacement->second.mf[i][j];
                int as_int = (int)(v * 65536.0f);
                matrix[i][j] = as_int * (1.0f / 65536.0f);
            }
        }
    } else {
#ifndef GBI_FLOATS
        // Original GBI where fixed point matrices are used
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j += 2) {
                int32_t int_part = addr[i * 2 + j / 2];
                uint32_t frac_part = addr[8 + i * 2 + j / 2];
                matrix[i][j] = (int32_t)((int_part & 0xffff0000) | (frac_part >> 16)) / 65536.0f;
                matrix[i][j + 1] = (int32_t)((int_part << 16) | (frac_part & 0xffff)) / 65536.0f;
            }
        }
#else
        // For a modified GBI where fixed point values are replaced with floats
        memcpy(matrix, addr, sizeof(matrix));
#endif
    }

    const int8_t mtx_projection = get_attr(MTX_PROJECTION);
    const int8_t mtx_load = get_attr(MTX_LOAD);
    const int8_t mtx_push = get_attr(MTX_PUSH);

    if (parameters & mtx_projection) {
        if (parameters & mtx_load) {
            memcpy(mRsp->P_matrix, matrix, sizeof(matrix));
        } else {
            MatrixMul(mRsp->P_matrix, matrix, mRsp->P_matrix);
        }
    } else { // G_MTX_MODELVIEW
        if ((parameters & mtx_push) && mRsp->modelview_matrix_stack_size < 11) {
            ++mRsp->modelview_matrix_stack_size;
            memcpy(mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1],
                   mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 2], sizeof(matrix));
        }
        if (parameters & mtx_load) {
            if (mRsp->modelview_matrix_stack_size == 0)
                ++mRsp->modelview_matrix_stack_size;
            memcpy(mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1], matrix, sizeof(matrix));
        } else {
            MatrixMul(mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1], matrix,
                      mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1]);
        }
        mRsp->lights_changed = 1;
    }
    MatrixMul(mRsp->MP_matrix, mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1], mRsp->P_matrix);
}

void Interpreter::GfxSpPopMatrix(uint32_t count) {
    while (count--) {
        if (mRsp->modelview_matrix_stack_size > 0) {
            --mRsp->modelview_matrix_stack_size;
            if (mRsp->modelview_matrix_stack_size > 0) {
                MatrixMul(mRsp->MP_matrix, mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1],
                          mRsp->P_matrix);
            }
        }
    }
    mRsp->lights_changed = true;
}

float Interpreter::AdjXForAspectRatio(float x) const {
    // Skip widescreen adjustment for fixed-size off-screen FBs (HUD elements,
    // small capture buffers), or those which specify a fixed aspect ratio.
    if (mFbActive && mActiveFrameBuffer != mFrameBuffers.end() &&
        (!mActiveFrameBuffer->second.resize || mActiveFrameBuffer->second.forceFixedAspect)) {
        return x;
    } else {
        return x * mCurAspectRatioDeltaForX;
    }
}

// Scale the width and height value based on the ratio of the viewport to the native size
void Interpreter::AdjustWidthHeightForScale(uint32_t& width, uint32_t& height, uint32_t nativeWidth,
                                            uint32_t nativeHeight) const {
    width = round(width * (mCurDimensions.width / (2.0f * (nativeWidth / 2))));
    height = round(height * (mCurDimensions.height / (2.0f * (nativeHeight / 2))));

    if (width == 0) {
        width = 1;
    }
    if (height == 0) {
        height = 1;
    }
}

void Interpreter::GfxSpVertex(size_t n_vertices, size_t dest_index, const F3DVtx* vertices) {
    float scaled_coeffs[MAX_LIGHTS][3];
	if (mRsp->geometry_mode & G_LIGHTING) {
		if (mRsp->lights_changed) {
			for (int i = 0; i < mRsp->current_num_lights - 1; i++) {
				CalculateNormalDir(&mRsp->current_lights[i].l, mRsp->current_lights_coeffs[i]);
			}
			/*static const Light_t lookat_x = {{0, 0, 0}, 0, {0, 0, 0}, 0, {127, 0, 0}, 0};
			static const Light_t lookat_y = {{0, 0, 0}, 0, {0, 0, 0}, 0, {0, 127, 0}, 0};*/
			CalculateNormalDir(&mRsp->lookat[0], mRsp->current_lookat_coeffs[0]);
			CalculateNormalDir(&mRsp->lookat[1], mRsp->current_lookat_coeffs[1]);
			mRsp->lights_changed = false;
		}
        for (int i = 0; i < mRsp->current_num_lights - 1; i++) {
            scaled_coeffs[i][0] = mRsp->current_lights_coeffs[i][0] * 0.00787401f;
            scaled_coeffs[i][1] = mRsp->current_lights_coeffs[i][1] * 0.00787401f;
            scaled_coeffs[i][2] = mRsp->current_lights_coeffs[i][2] * 0.00787401f;
        }
	}
	
	float xAdjust;
    if (mFbActive && mActiveFrameBuffer != mFrameBuffers.end() &&
        (!mActiveFrameBuffer->second.resize || mActiveFrameBuffer->second.forceFixedAspect)) {
        xAdjust = 1.f;
    } else {
        xAdjust = mCurAspectRatioDeltaForX;
    }
	
    const bool doLighting = mRsp->geometry_mode & G_LIGHTING;
#ifdef HAVE_POSITIONAL_LIGHTING
    const bool doPositionalLighting = mRsp->geometry_mode & G_LIGHTING_POSITIONAL;
#endif
    const bool doFog = mRsp->geometry_mode & G_FOG;
    const bool doTexGen = mRsp->geometry_mode & G_TEXTURE_GEN;
    const bool doTexGenLinear = mRsp->geometry_mode & G_TEXTURE_GEN_LINEAR;
	
    for (size_t i = 0; i < n_vertices; i++, dest_index++) {
        const F3DVtx_t* v = &vertices[i].v;
        if (v == nullptr) {
            return;
        }
        const F3DVtx_tn* vn = &vertices[i].n;
        struct LoadedVertex* d = &mRsp->loaded_vertices[dest_index];

        const float (*mp)[4] = mRsp->MP_matrix;
        float x = (v->ob[0] * mp[0][0] + v->ob[1] * mp[1][0] +
                  v->ob[2] * mp[2][0] + mp[3][0]) * xAdjust;
        float y = v->ob[0] * mp[0][1] + v->ob[1] * mp[1][1] +
                  v->ob[2] * mp[2][1] + mp[3][1];
        float z = v->ob[0] * mp[0][2] + v->ob[1] * mp[1][2] +
                  v->ob[2] * mp[2][2] + mp[3][2];
        float w = v->ob[0] * mp[0][3] + v->ob[1] * mp[1][3] +
                  v->ob[2] * mp[2][3] + mp[3][3];

#ifdef HAVE_POSITIONAL_LIGHTING
        float world_pos[3] = { 0.0 };
        if (doPositionalLighting) {
            float(*mtx)[4] = mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1];
            world_pos[0] = v->ob[0] * mtx[0][0] + v->ob[1] * mtx[1][0] + v->ob[2] * mtx[2][0] + mtx[3][0];
            world_pos[1] = v->ob[0] * mtx[0][1] + v->ob[1] * mtx[1][1] + v->ob[2] * mtx[2][1] + mtx[3][1];
            world_pos[2] = v->ob[0] * mtx[0][2] + v->ob[1] * mtx[1][2] + v->ob[2] * mtx[2][2] + mtx[3][2];
        }
#endif

        short U = v->tc[0] * mRsp->texture_scaling_factor.s >> 16;
        short V = v->tc[1] * mRsp->texture_scaling_factor.t >> 16;

        if (doLighting) {
            const float nx = vn->n[0], ny = vn->n[1], nz = vn->n[2];
            const auto& ambient = mRsp->current_lights[mRsp->current_num_lights - 1].l;
            int r = ambient.col[0];
            int g = ambient.col[1];
            int b = ambient.col[2];

            for (int i = 0; i < mRsp->current_num_lights - 1; i++) {
                float intensity = 0;
#ifdef HAVE_POSITIONAL_LIGHTING
                if (doPositionalLighting && (mRsp->current_lights[i].p.unk3 != 0)) {
                    // Calculate distance from the light to the vertex
                    float dist_vec[3] = { mRsp->current_lights[i].p.pos[0] - world_pos[0],
                                          mRsp->current_lights[i].p.pos[1] - world_pos[1],
                                          mRsp->current_lights[i].p.pos[2] - world_pos[2] };
                    float dist_sq =
                        dist_vec[0] * dist_vec[0] + dist_vec[1] * dist_vec[1] +
                        dist_vec[2] * dist_vec[2] * 2; // The *2 comes from GLideN64, unsure of why it does it
                    float dist = sqrt(dist_sq);

                    // Transform distance vector (which acts as a direction light vector) into model's space
                    float light_model[3];
                    TransposedMatrixMul(light_model, dist_vec,
                                        mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1]);

                    // Calculate intensity for each axis using standard formula for intensity
                    float light_intensity[3];
                    for (int light_i = 0; light_i < 3; light_i++) {
                        light_intensity[light_i] = 4.0f * light_model[light_i] / dist_sq;
                        light_intensity[light_i] = FAST_CLAMP(light_intensity[light_i], -1.0f, 1.0f);
                    }

                    // Adjust intensity based on surface normal and sum up total
                    float total_intensity =
                        light_intensity[0] * nx + light_intensity[1] * ny + light_intensity[2] * nz;
                    total_intensity = FAST_CLAMP(total_intensity, -1.0f, 1.0f);

                    // Attenuate intensity based on attenuation values.
                    // Example formula found at https://ogldev.org/www/tutorial20/tutorial20.html
                    // Specific coefficients for MM's microcode sourced from GLideN64
                    // https://github.com/gonetz/GLideN64/blob/3b43a13a80dfc2eb6357673440b335e54eaa3896/src/gSP.cpp#L636
                    float distf = floorf(dist);
                    float attenuation = (distf * mRsp->current_lights[i].p.unk7 * 2.0f +
                                         distf * distf * mRsp->current_lights[i].p.unkE * 0.125f) /
                                            (float)0xFFFF +
                                        1.0f;
                    intensity = total_intensity / attenuation;
                    if (intensity > 0.0f) {
                        r += intensity * mRsp->current_lights[i].l.col[0];
                        g += intensity * mRsp->current_lights[i].l.col[1];
                        b += intensity * mRsp->current_lights[i].l.col[2];
                    }
                } else
#endif
                {
                    intensity += nx * scaled_coeffs[i][0];
                    intensity += ny * scaled_coeffs[i][1];
                    intensity += nz * scaled_coeffs[i][2];
                    if (intensity > 0.0f) {
                        r += intensity * mRsp->current_lights[i].l.col[0];
                        g += intensity * mRsp->current_lights[i].l.col[1];
                        b += intensity * mRsp->current_lights[i].l.col[2];
                    }
                }
            }

            d->color.r = (uint8_t)std::min(r, 255);
            d->color.g = (uint8_t)std::min(g, 255);
            d->color.b = (uint8_t)std::min(b, 255);

            if (doTexGen) {
                float dotx = 0, doty = 0;
                dotx += nx * mRsp->current_lookat_coeffs[0][0];
                dotx += ny * mRsp->current_lookat_coeffs[0][1];
                dotx += nz * mRsp->current_lookat_coeffs[0][2];
                doty += nx * mRsp->current_lookat_coeffs[1][0];
                doty += ny * mRsp->current_lookat_coeffs[1][1];
                doty += nz * mRsp->current_lookat_coeffs[1][2];

                dotx *= 0.00787401f;
                doty *= 0.00787401f;

                dotx = FAST_CLAMP(dotx, -1.0f, 1.0f);
                doty = FAST_CLAMP(doty, -1.0f, 1.0f);

                if (doTexGenLinear) {
                    // Not sure exactly what formula we should use to get accurate values
                    /*dotx = (2.906921f * dotx * dotx + 1.36114f) * dotx;
                    doty = (2.906921f * doty * doty + 1.36114f) * doty;
                    dotx = (dotx + 1.0f) / 4.0f;
                    doty = (doty + 1.0f) / 4.0f;*/
                    dotx = acosf(-dotx) /* M_PI */ * 0.159155f;
                    doty = acosf(-doty) /* M_PI */ * 0.159155f;
                } else {
                    dotx = (dotx + 1.0f) * 0.25f;
                    doty = (doty + 1.0f) * 0.25f;
                }

                U = (int32_t)(dotx * mRsp->texture_scaling_factor.s);
                V = (int32_t)(doty * mRsp->texture_scaling_factor.t);
            }
        } else {
            d->color.r = v->cn[0];
            d->color.g = v->cn[1];
            d->color.b = v->cn[2];
        }

        d->u = U;
        d->v = V;

        // trivial clip rejection
        uint8_t clip = 0;
        clip |= (uint32_t)(x + w < 0.0f) << 0;  // CLIP_LEFT
        clip |= (uint32_t)(x - w > 0.0f) << 1;  // CLIP_RIGHT
        clip |= (uint32_t)(y + w < 0.0f) << 2;  // CLIP_BOTTOM
        clip |= (uint32_t)(y - w > 0.0f) << 3;  // CLIP_TOP
        clip |= (uint32_t)(z - w > 0.0f) << 5;  // CLIP_FAR
        d->clip_rej = clip;

        d->x = x;
        d->y = y;
        d->z = z;
        d->w = w;

        if (mRsp->geometry_mode & G_FOG) {
            if (fabsf(w) < 0.001f) {
                // To avoid division by zero
                w = 0.001f;
            }

            float winv = 1.0f / w;
            if (winv < 0.0f) {
                winv = std::numeric_limits<int16_t>::max();
			}

            float fog_z = z * winv * mRsp->fog_mul + mRsp->fog_offset;
            fog_z = FAST_CLAMP(fog_z, 0.0f, 255.0f);
            d->color.a = fog_z; // Use alpha variable to store fog factor
        } else {
            d->color.a = v->cn[3];
        }
    }
}

void Interpreter::GfxSpModifyVertex(uint16_t vtx_idx, uint8_t where, uint32_t val) {
    SUPPORT_CHECK(where == G_MWO_POINT_ST);

    int16_t s = (int16_t)(val >> 16);
    int16_t t = (int16_t)val;

    LoadedVertex* v = &mRsp->loaded_vertices[vtx_idx];
    v->u = s;
    v->v = t;
}

void Interpreter::GfxSpTri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {
    struct LoadedVertex* v1 = &mRsp->loaded_vertices[vtx1_idx];
    struct LoadedVertex* v2 = &mRsp->loaded_vertices[vtx2_idx];
    struct LoadedVertex* v3 = &mRsp->loaded_vertices[vtx3_idx];
    struct LoadedVertex* v_arr[3] = { v1, v2, v3 };

    // if (rand()%2) return;

    if (v1->clip_rej & v2->clip_rej & v3->clip_rej) {
        // The whole triangle lies outside the visible area
        return;
    }

    const uint32_t cull_both = get_attr(CULL_BOTH);
    const uint32_t cull_front = get_attr(CULL_FRONT);
    const uint32_t cull_back = get_attr(CULL_BACK);

    if ((mRsp->geometry_mode & cull_both) != 0) {
        float inv_w1 = 1.0f / v1->w;
        float inv_w2 = 1.0f / v2->w;
        float inv_w3 = 1.0f / v3->w;
        float dx1 = (v1->x * inv_w1) - (v2->x * inv_w2);
        float dy1 = (v1->y * inv_w1) - (v2->y * inv_w2);
        float dx2 = (v3->x * inv_w3) - (v2->x * inv_w2);
        float dy2 = (v3->y * inv_w3) - (v2->y * inv_w2);
        float cross = dx1 * dy2 - dy1 * dx2;

        if ((v1->w < 0) ^ (v2->w < 0) ^ (v3->w < 0)) {
            // If one vertex lies behind the eye, negating cross will give the correct result.
            // If all vertices lie behind the eye, the triangle will be rejected anyway.
            cross = -cross;
        }

        // G_EX_INVERT_CULLING is a LUS extension, not tied to a specific ucode,
        // so apply it regardless of the active microcode handler.
        if ((mRsp->extra_geometry_mode & G_EX_INVERT_CULLING) != 0) {
            cross = -cross;
        }

        auto cull_type = mRsp->geometry_mode & cull_both;

        if (cull_type == cull_front && cross <= 0) {
            return;
        } else if (cull_type == cull_back && cross >= 0) {
            return;
        } else if (cull_type == cull_both) {
            // Why is this even an option?
            return;
        }
    }

    // depth_test is set when the fragment has a depth value to compare (either from vertex Z via
    // RSP G_ZBUFFER, or from the prim-depth register via G_ZS_PRIM) and Z_CMP is requested.
    bool zbuffer_enabled = (mRsp->geometry_mode & G_ZBUFFER) == G_ZBUFFER;
    bool prim_depth_enabled = (mRdp->other_mode_l & G_ZS_PRIM) != 0;
    bool depth_test = (zbuffer_enabled || prim_depth_enabled) && (mRdp->other_mode_l & Z_CMP) == Z_CMP;
    bool depth_mask = (mRdp->other_mode_l & Z_UPD) == Z_UPD;
    uint8_t depth_test_and_mask = (depth_test ? 1 : 0) | (depth_mask ? 2 : 0);
    if (depth_test_and_mask != mRenderingState.depth_test_and_mask) {
        Flush();
        mRapi->SetDepthTestAndMask(depth_test, depth_mask);
        mRenderingState.depth_test_and_mask = depth_test_and_mask;
    }

    bool zmode_decal = (mRdp->other_mode_l & ZMODE_DEC) == ZMODE_DEC;
    if (zmode_decal != mRenderingState.decal_mode) {
        Flush();
        mRapi->SetZmodeDecal(zmode_decal);
        mRenderingState.decal_mode = zmode_decal;
    }

    if (mRdp->viewport_or_scissor_changed) {
        const bool viewportChanged =
            mRdp->viewport.x != mRenderingState.viewport.x ||
            mRdp->viewport.y != mRenderingState.viewport.y ||
            mRdp->viewport.width != mRenderingState.viewport.width ||
            mRdp->viewport.height != mRenderingState.viewport.height;
        const bool scissorChanged =
            mRdp->scissor.x != mRenderingState.scissor.x ||
            mRdp->scissor.y != mRenderingState.scissor.y ||
            mRdp->scissor.width != mRenderingState.scissor.width ||
            mRdp->scissor.height != mRenderingState.scissor.height;

        if (viewportChanged || scissorChanged) {
            Flush();
        }
        if (viewportChanged) {
            mRapi->SetViewport(mRdp->viewport.x, mRdp->viewport.y, mRdp->viewport.width, mRdp->viewport.height);
            mRenderingState.viewport = mRdp->viewport;
        }
        if (scissorChanged) {
            mRapi->SetScissor(mRdp->scissor.x, mRdp->scissor.y, mRdp->scissor.width, mRdp->scissor.height);
            mRenderingState.scissor = mRdp->scissor;
        }
        mRdp->viewport_or_scissor_changed = false;
    }

    uint64_t cc_id = mRdp->combine_mode;
    uint64_t cc_options = 0;
    bool use_alpha = ((mRdp->other_mode_l & (3 << 20)) == (G_BL_CLR_MEM << 20) &&
                      (mRdp->other_mode_l & (3 << 16)) == (G_BL_1MA << 16)) ||
                     ((mRdp->other_mode_l & (3 << 22)) == (G_BL_CLR_MEM << 22) &&
                      (mRdp->other_mode_l & (3 << 18)) == (G_BL_1MA << 18));
    uint8_t blend_src = mRdp->other_mode_l >> 30;
    bool use_blend_color = blend_src == G_BL_CLR_BL;
    bool use_fog = blend_src == G_BL_CLR_FOG || use_blend_color;
    bool texture_edge = (mRdp->other_mode_l & CVG_X_ALPHA) == CVG_X_ALPHA;
    bool use_noise = (mRdp->other_mode_l & (3U << G_MDSFT_ALPHACOMPARE)) == G_AC_DITHER;
    bool use_2cyc = (mRdp->other_mode_h & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE;
    bool alpha_threshold = (mRdp->other_mode_l & (3U << G_MDSFT_ALPHACOMPARE)) == G_AC_THRESHOLD;
    bool invisible =
        (mRdp->other_mode_l & (3 << 24)) == (G_BL_0 << 24) && (mRdp->other_mode_l & (3 << 20)) == (G_BL_CLR_MEM << 20);
    bool use_grayscale = mRdp->grayscale;
    bool use_prim_depth = (mRdp->other_mode_l & G_ZS_PRIM) != 0;

    if (texture_edge) {
        if (use_alpha) {
            alpha_threshold = true;
            texture_edge = false;
        }
        use_alpha = true;
    }

    if (use_alpha) {
        cc_options |= SHADER_OPT(ALPHA);
    }
    if (use_fog) {
        cc_options |= SHADER_OPT(FOG);
    }
    if (texture_edge) {
        cc_options |= SHADER_OPT(TEXTURE_EDGE);
    }
    if (use_noise) {
        cc_options |= SHADER_OPT(NOISE);
    }
    if (use_2cyc) {
        cc_options |= SHADER_OPT(_2CYC);
    }
    if (alpha_threshold) {
        cc_options |= SHADER_OPT(ALPHA_THRESHOLD);
    }
    if (invisible) {
        cc_options |= SHADER_OPT(INVISIBLE);
    }
    if (use_grayscale) {
        cc_options |= SHADER_OPT(GRAYSCALE);
    }
    if (use_prim_depth) {
        cc_options |= SHADER_OPT(PRIM_DEPTH);
    }

    if (!mShaderStack.empty()) {
        cc_options |= (mShaderStack.top() << SHADER_ID_SHIFT);
    } else {
        cc_options |= -1 << SHADER_ID_SHIFT;
    }

    if (mRdp->loaded_texture[0].masked) {
        cc_options |= SHADER_OPT(TEXEL0_MASK);
    }
    if (mRdp->loaded_texture[1].masked) {
        cc_options |= SHADER_OPT(TEXEL1_MASK);
    }
    if (mRdp->loaded_texture[0].blended) {
        cc_options |= SHADER_OPT(TEXEL0_BLEND);
    }
    if (mRdp->loaded_texture[1].blended) {
        cc_options |= SHADER_OPT(TEXEL1_BLEND);
    }

    ColorCombinerKey key;
    key.combine_mode = mRdp->combine_mode;
    key.options = cc_options;

    ColorCombiner* comb = LookupOrCreateColorCombiner(key);

    uint32_t tm = 0;
    uint32_t tex_width[2], tex_height[2], tex_width2[2], tex_height2[2];
    uint32_t effective_tile[2];

    for (int i = 0; i < 2; i++) {
        uint32_t tile = mRdp->first_tile_index + i;

        // No LOD support: force both slots to the base mip level.
        if (i == 1 && mRdp->first_tile_index >= 2) {
            tile = mRdp->first_tile_index;
        }
        effective_tile[i] = tile;

        if (comb->usedTextures[i]) {
            if (mRdp->textures_changed[i]) {
                Flush();
                ImportTexture(i, tile, false);
                if (mRdp->loaded_texture[i].masked) {
                    ImportTextureMask(SHADER_FIRST_MASK_TEXTURE + i, tile);
                }
                if (mRdp->loaded_texture[i].blended) {
                    ImportTexture(SHADER_FIRST_REPLACEMENT_TEXTURE + i, tile, true);
                }
                mRdp->textures_changed[i] = false;
            }

            uint8_t cms = mRdp->texture_tile[tile].cms;
            uint8_t cmt = mRdp->texture_tile[tile].cmt;

            uint32_t loaded_line_size = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;
            uint32_t loaded_size = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
            uint32_t loaded_full_line =
                mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
            uint32_t tex_size_bytes;
            uint32_t line_size;
            if ((loaded_line_size != loaded_size || loaded_full_line != loaded_size) && loaded_line_size > 0) {
                line_size = loaded_line_size;
                tex_size_bytes = loaded_size;
            } else {
                line_size = mRdp->texture_tile[tile].line_size_bytes;
                tex_size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].orig_size_bytes;
                // RGBA32: texture_tile stores TMEM-interleaved stride (half of actual DRAM stride).
                if (mRdp->texture_tile[tile].siz == G_IM_SIZ_32b) {
                    line_size *= 2;
                }
            }

            if (line_size == 0) {
                line_size = 1;
            }

            tex_height[i] = tex_size_bytes / line_size;
            switch (mRdp->texture_tile[tile].siz) {
                case G_IM_SIZ_4b:
                    line_size <<= 1;
                    break;
                case G_IM_SIZ_8b:
                    break;
                case G_IM_SIZ_16b:
                    line_size /= G_IM_SIZ_16b_LINE_BYTES;
                    break;
                case G_IM_SIZ_32b:
                    line_size /= 4; // RGBA32: 4 bytes per pixel (line_size is now actual DRAM stride)
                    break;
            }
            tex_width[i] = line_size;

            tex_width2[i] = (uint32_t)(int32_t)((mRdp->texture_tile[tile].lrs - mRdp->texture_tile[tile].uls + 4) / 4);
            tex_height2[i] = (uint32_t)(int32_t)((mRdp->texture_tile[tile].lrt - mRdp->texture_tile[tile].ult + 4) / 4);

            // Same pyramid-like ratio gate as ImportTexture: only clamp when loaded pixels
            // are close to rendered pixels (mipmap), not when much bigger (window scroll).
            uint32_t loadedPx = tex_width[i] * tex_height[i];
            uint32_t renderedPx = tex_width2[i] * tex_height2[i];
            bool pyrLike = renderedPx > 0 && loadedPx > renderedPx && loadedPx * 8 < renderedPx * 13;
            if ((pyrLike || (cms & G_TX_CLAMP)) && tex_width2[i] > 0 && tex_width2[i] < tex_width[i]) {
                tex_width[i] = tex_width2[i];
            }
            if ((pyrLike || (cmt & G_TX_CLAMP)) && tex_height2[i] > 0 && tex_height2[i] < tex_height[i]) {
                tex_height[i] = tex_height2[i];
            }

            uint32_t tex_width1 = tex_width[i] << (cms & G_TX_MIRROR);
            uint32_t tex_height1 = tex_height[i] << (cmt & G_TX_MIRROR);

            if ((cms & G_TX_CLAMP) && ((cms & G_TX_MIRROR) || tex_width1 != tex_width2[i])) {
                tm |= 1 << 2 * i;
                cms &= ~G_TX_CLAMP;
            }
            if ((cmt & G_TX_CLAMP) && ((cmt & G_TX_MIRROR) || tex_height1 != tex_height2[i])) {
                tm |= 1 << 2 * i + 1;
                cmt &= ~G_TX_CLAMP;
            }

            if (mRenderingState.mTextures[i] == nullptr) {
                continue;
            }

            bool linear_filter = (mRdp->other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT;
            if (linear_filter != mRenderingState.mTextures[i]->second.linear_filter ||
                cms != mRenderingState.mTextures[i]->second.cms || cmt != mRenderingState.mTextures[i]->second.cmt) {
                Flush();

                // Set the same sampler params on the blended texture. Needed for opengl.
                if (mRdp->loaded_texture[i].blended) {
                    mRapi->SetSamplerParameters(SHADER_FIRST_REPLACEMENT_TEXTURE + i, linear_filter, cms, cmt);
                }

                mRapi->SetSamplerParameters(i, linear_filter, cms, cmt);
                mRenderingState.mTextures[i]->second.linear_filter = linear_filter;
                mRenderingState.mTextures[i]->second.cms = cms;
                mRenderingState.mTextures[i]->second.cmt = cmt;
            }
        }
    }

    struct ShaderProgram* prg = comb->prg[tm];
    if (prg == NULL) {
        comb->prg[tm] = prg =
            LookupOrCreateShaderProgram(comb->shader_id0, comb->shader_id1 | tm * SHADER_OPT(TEXEL0_CLAMP_S));
    }
    if (prg != mRenderingState.mShaderProgram) {
        Flush();
        //mRapi->UnloadShader(mRenderingState.mShaderProgram);
        mRapi->LoadShader(prg);
        mRenderingState.mShaderProgram = prg;
    }
    if (use_alpha != mRenderingState.alpha_blend) {
        Flush();
        mRapi->SetUseAlpha(use_alpha);
        mRenderingState.alpha_blend = use_alpha;
    }
    uint8_t numInputs;
    bool usedTextures[2];

    mRapi->ShaderGetInfo(prg, &numInputs, usedTextures);

    bool invert_y = mRapi->GetClipParameters();
	
	float *buf_vbo_ptr = &mBufVbo[mBufVboLen];
    float u_scale[2] = {0.03125f, 0.03125f};
    float v_scale[2] = {0.03125f, 0.03125f};
    float u_offset[2] = {0.0f, 0.0f};
    float v_offset[2] = {0.0f, 0.0f};
    float inv_tex_width[2] = {1.0f, 1.0f};
    float inv_tex_height[2] = {1.0f, 1.0f};
    
    for (int t = 0; t < 2; t++) {
        if (!usedTextures[t]) {
            continue;
        }

        int shifts = mRdp->texture_tile[mRdp->first_tile_index + t].shifts;
        int shiftt = mRdp->texture_tile[mRdp->first_tile_index + t].shiftt;
        
        inv_tex_width[t] = 1.0f / (float)tex_width[t];
        inv_tex_height[t] = 1.0f / (float)tex_height[t];

        if (shifts != 0) {
            u_scale[t] *= (shifts <= 10) ? (1.0f / (1 << shifts)) : (float)(1 << (16 - shifts));
        }
        if (shiftt != 0) {
            v_scale[t] *= (shiftt <= 10) ? (1.0f / (1 << shiftt)) : (float)(1 << (16 - shiftt));
        }
        
        u_offset[t] = mRdp->texture_tile[mRdp->first_tile_index + t].uls * 0.25f;
        v_offset[t] = mRdp->texture_tile[mRdp->first_tile_index + t].ult * 0.25f;
    }

    for (int i = 0; i < 3; i++) {
        float z = v_arr[i]->z, w = v_arr[i]->w;

        *buf_vbo_ptr++ = v_arr[i]->x;
        *buf_vbo_ptr++ = invert_y ? -v_arr[i]->y : v_arr[i]->y;
        *buf_vbo_ptr++ = z;
        *buf_vbo_ptr++ = w;
		
        for (int t = 0; t < 2; t++) {
            if (!usedTextures[t]) {
                continue;
            }
			float u = (v_arr[i]->u * u_scale[t]) - u_offset[t];
            float v = (v_arr[i]->v * v_scale[t]) - v_offset[t];

            if ((mRdp->other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT) {
                // Linear filter adds 0.5f to the coordinates
                if (!is_rect) {
                    u += 0.5f;
                    v += 0.5f;
                }
            }

            *buf_vbo_ptr++ = u * inv_tex_width[t];
            *buf_vbo_ptr++ = v * inv_tex_height[t];

            bool clampS = tm & (1 << 2 * t);
            bool clampT = tm & (1 << 2 * t + 1);

            if (clampS) {
                *buf_vbo_ptr++ = (tex_width2[t] - 0.5f) * inv_tex_width[t];
            }

            if (clampT) {
                *buf_vbo_ptr++ = (tex_height2[t] - 0.5f) * inv_tex_height[t];
            }
        }

        if (use_fog) {
            *buf_vbo_ptr++ = mRdp->fog_color.r;
            *buf_vbo_ptr++ = mRdp->fog_color.g;
            *buf_vbo_ptr++ = mRdp->fog_color.b;
            *buf_vbo_ptr++ = v_arr[i]->color.a; // fog factor (not alpha)
        }

        if (use_grayscale) {
            *buf_vbo_ptr++ = mRdp->grayscale_color.r;
            *buf_vbo_ptr++ = mRdp->grayscale_color.g;
            *buf_vbo_ptr++ = mRdp->grayscale_color.b;
            *buf_vbo_ptr++ = mRdp->grayscale_color.a; // lerp interpolation factor (not alpha)
        }

        for (int j = 0; j < numInputs; j++) {
            RGBA* color;
            RGBA tmp;
            for (int k = 0; k < 1 + (use_alpha ? 1 : 0); k++) {
                switch (comb->shader_input_mapping[k][j]) {
                        // Note: CCMUX constants and ACMUX constants used here have same value, which is why this works
                        // (except LOD fraction).
                    case G_CCMUX_PRIMITIVE:
                        color = &mRdp->prim_color;
                        break;
                    case G_CCMUX_SHADE:
                        color = &v_arr[i]->color;
                        break;
                    case G_CCMUX_ENVIRONMENT:
                        color = &mRdp->env_color;
                        break;
                    case G_CCMUX_PRIMITIVE_ALPHA: {
                        tmp.r = tmp.g = tmp.b = mRdp->prim_color.a;
                        color = &tmp;
                        break;
                    }
                    case G_CCMUX_ENV_ALPHA: {
                        tmp.r = tmp.g = tmp.b = mRdp->env_color.a;
                        color = &tmp;
                        break;
                    }
                    case G_CCMUX_PRIM_LOD_FRAC: {
                        tmp.r = tmp.g = tmp.b = mRdp->prim_lod_fraction;
                        color = &tmp;
                        break;
                    }
                    case G_CCMUX_LOD_FRACTION: {
                        if (mRdp->other_mode_l & G_TL_LOD) {
                            // "Hack" that works for Bowser - Peach painting
                            float distance_frac = (v1->w - 3000.0f) / 3000.0f;
                            if (distance_frac < 0.0f) {
                                distance_frac = 0.0f;
                            }
                            if (distance_frac > 1.0f) {
                                distance_frac = 1.0f;
                            }
                            tmp.r = tmp.g = tmp.b = tmp.a = distance_frac * 255.0f;
                        } else {
                            tmp.r = tmp.g = tmp.b = tmp.a = 255.0f;
                        }
                        color = &tmp;
                        break;
                    }
                    case G_CCMUX_KEY_CENTER:
                        color = &mRdp->key_center;
                        break;
                    case G_CCMUX_KEY_SCALE:
                        color = &mRdp->key_scale;
                        break;
                    case G_CCMUX_CONVERT_K4: {
                        tmp.r = tmp.g = tmp.b = mRdp->convert_k[4];
                        color = &tmp;
                        break;
                    }
                    case G_CCMUX_CONVERT_K5: {
                        tmp.r = tmp.g = tmp.b = mRdp->convert_k[5];
                        color = &tmp;
                        break;
                    }
                    case G_ACMUX_PRIM_LOD_FRAC:
                        tmp.a = mRdp->prim_lod_fraction;
                        color = &tmp;
                        break;
                    default:
                        memset(&tmp, 0, sizeof(tmp));
                        color = &tmp;
                        break;
                }
                if (k == 0) {
                    *buf_vbo_ptr++ = color->r * (1.0f / 255.0f);
                    *buf_vbo_ptr++ = color->g * (1.0f / 255.0f);
                    *buf_vbo_ptr++ = color->b * (1.0f / 255.0f);
                } else {
                    if (use_fog && color == &v_arr[i]->color) {
                        // Shade alpha is 100% for fog
                        *buf_vbo_ptr++ = 1.0f;
                    } else {
                        *buf_vbo_ptr++ = color->a * (1.0f / 255.0f);
                    }
                }
            }
        }

        // struct RGBA *color = &v_arr[i]->color;
        // mBufVbo[mBufVboLen++] = color->r / 255.0f;
        // mBufVbo[mBufVboLen++] = color->g / 255.0f;
        // mBufVbo[mBufVboLen++] = color->b / 255.0f;
        // mBufVbo[mBufVboLen++] = color->a / 255.0f;
    }
	
	mBufVboLen = buf_vbo_ptr - mBufVbo;
#ifdef __vita__
	mBufVboNumTris++;
#else
    if (++mBufVboNumTris == MAX_TRI_BUFFER) {
        // if (++mBufVbo_num_tris == 1) {
        Flush();
    }
#endif
}

void Interpreter::GfxSpGeometryMode(uint32_t clear, uint32_t set) {
    mRsp->geometry_mode &= ~clear;
    mRsp->geometry_mode |= set;
}

void Interpreter::GfxSpExtraGeometryMode(uint32_t clear, uint32_t set) {
    mRsp->extra_geometry_mode &= ~clear;
    mRsp->extra_geometry_mode |= set;
}

void Interpreter::AdjustVIewportOrScissor(XYWidthHeight* area) {
    if (!mFbActive) {
        // Adjust the y origin based on the y-inversion for the active framebuffer
        bool invertY = mRapi->GetClipParameters();
        if (invertY) {
            area->y -= area->height;
        } else {
            area->y = mNativeDimensions.height - area->y;
        }

        area->width *= RATIO_X(mActiveFrameBuffer, mCurDimensions);
        area->height *= RATIO_Y(mActiveFrameBuffer, mCurDimensions);
        area->x *= RATIO_X(mActiveFrameBuffer, mCurDimensions);
        area->y *= RATIO_Y(mActiveFrameBuffer, mCurDimensions);

        if (!mRendersToFb) {
            area->x += mGameWindowViewport.x;
            area->y += mGfxCurrentWindowDimensions.height - (mGameWindowViewport.y + mGameWindowViewport.height);
        }
    } else {
        area->y = mActiveFrameBuffer->second.orig_height - area->y;

        if (mActiveFrameBuffer->second.resize) {
            area->width *= RATIO_X(mActiveFrameBuffer, mCurDimensions);
            area->height *= RATIO_Y(mActiveFrameBuffer, mCurDimensions);
            area->x *= RATIO_X(mActiveFrameBuffer, mCurDimensions);
            area->y *= RATIO_Y(mActiveFrameBuffer, mCurDimensions);
        }
    }
}

void Interpreter::CalcAndSetViewport(const F3DVp_t* viewport) {
    // 2 bits fraction
    float width = 2.0f * viewport->vscale[0] / 4.0f;
    float height = 2.0f * viewport->vscale[1] / 4.0f;
    float x = (viewport->vtrans[0] / 4.0f) - width / 2.0f;
    float y = ((viewport->vtrans[1] / 4.0f) + height / 2.0f);

    mRdp->viewport.x = x;
    mRdp->viewport.y = y;
    mRdp->viewport.width = width;
    mRdp->viewport.height = height;

    AdjustVIewportOrScissor(&mRdp->viewport);

    mRdp->viewport_or_scissor_changed = true;
}

void Interpreter::GfxSpMovememF3dex2(uint8_t index, uint8_t offset, const void* data) {
    switch (index) {
        case F3DEX2_G_MV_VIEWPORT:
            CalcAndSetViewport((const F3DVp_t*)data);
            break;
        case F3DEX2_G_MV_LIGHT: {
            int lightidx = offset / 24 - 2;
            if (lightidx >= 0 && lightidx <= MAX_LIGHTS) { // skip lookat
                // NOTE: reads out of bounds if it is an ambient light
                memcpy(mRsp->current_lights + lightidx, data, sizeof(F3DLight));
            } else if (lightidx < 0) {
                memcpy(mRsp->lookat + offset / 24, data, sizeof(F3DLight_t)); // TODO Light?
            }
            break;
        }
    }
}

void Interpreter::GfxSpMovememF3d(uint8_t index, uint8_t offset, const void* data) {
    switch (index) {
        case F3DEX_G_MV_VIEWPORT:
            CalcAndSetViewport((const F3DVp_t*)data);
            break;
        case F3DEX_G_MV_LOOKATY:
        case F3DEX_G_MV_LOOKATX:
            memcpy(mRsp->lookat + (index - F3DEX_G_MV_LOOKATY) / 2, data, sizeof(F3DLight_t));
            break;
        case F3DEX_G_MV_L0:
        case F3DEX_G_MV_L1:
        case F3DEX_G_MV_L2:
        case F3DEX_G_MV_L3:
        case F3DEX_G_MV_L4:
        case F3DEX_G_MV_L5:
        case F3DEX_G_MV_L6:
        case F3DEX_G_MV_L7:
            // NOTE: reads out of bounds if it is an ambient light
            memcpy(mRsp->current_lights + (index - F3DEX_G_MV_L0) / 2, data, sizeof(F3DLight_t));
            break;
    }
}

void Interpreter::GfxSpMovewordF3dex2(uint8_t index, uint16_t offset, uintptr_t data) {
    switch (index) {
        case G_MW_NUMLIGHT:
            mRsp->current_num_lights = data / 24 + 1; // add ambient light
            mRsp->lights_changed = true;
            break;
        case G_MW_FOG:
            mRsp->fog_mul = (int16_t)(data >> 16);
            mRsp->fog_offset = (int16_t)data;
            break;
        case G_MW_SEGMENT: {
            int segNumber = offset / 4;
            mSegmentPointers[segNumber] = data;
        } break;
        case G_MW_SEGMENT_INTERP: {
            int segNumber = offset % 16;
            int segIndex = offset / 16;

            if (segIndex == mInterpolationIndex)
                mSegmentPointers[segNumber] = data;
        } break;
    }
}

void Interpreter::GfxSpMovewordF3d(uint8_t index, uint16_t offset, uintptr_t data) {
    switch (index) {
        case G_MW_NUMLIGHT:
            // Ambient light is included
            // The 31st bit is a flag that lights should be recalculated
            mRsp->current_num_lights = (data - 0x80000000U) / 32;
            mRsp->lights_changed = true;
            break;
        case G_MW_FOG:
            mRsp->fog_mul = (int16_t)(data >> 16);
            mRsp->fog_offset = (int16_t)data;
            break;
        case G_MW_SEGMENT: {
            int segNumber = offset / 4;
            mSegmentPointers[segNumber] = data;
        } break;
        case G_MW_SEGMENT_INTERP: {
            int segNumber = offset % 16;
            int segIndex = offset / 16;

            if (segIndex == mInterpolationIndex)
                mSegmentPointers[segNumber] = data;
        } break;
    }
}

void Interpreter::GfxSpTexture(uint16_t sc, uint16_t tc, uint8_t level, uint8_t tile, uint8_t on) {
    mRsp->texture_scaling_factor.s = sc;
    mRsp->texture_scaling_factor.t = tc;
    if (mRdp->first_tile_index != tile) {
        mRdp->textures_changed[0] = true;
        mRdp->textures_changed[1] = true;
    }

    mRdp->first_tile_index = tile;
}

void Interpreter::GfxDpSetScissor(uint32_t mode, uint32_t ulx, uint32_t uly, uint32_t lrx, uint32_t lry) {
    mRdp->scissor_ulx_raw = (uint16_t)ulx;
    mRdp->scissor_uly_raw = (uint16_t)uly;
    mRdp->scissor_lrx_raw = (uint16_t)lrx;
    mRdp->scissor_lry_raw = (uint16_t)lry;

    float x = ulx / 4.0f;
    float y = lry / 4.0f;
    float width = (lrx - ulx) / 4.0f;
    float height = (lry - uly) / 4.0f;

    mRdp->scissor.x = x;
    mRdp->scissor.y = y;
    mRdp->scissor.width = width;
    mRdp->scissor.height = height;

    AdjustVIewportOrScissor(&mRdp->scissor);

    mRdp->viewport_or_scissor_changed = true;
}

void Interpreter::GfxDpSetTextureImage(uint32_t format, uint32_t size, uint32_t width, const char* texPath,
                                       uint32_t texFlags, RawTexMetadata rawTexMetdata, const void* addr) {
    mRdp->texture_to_load.addr = (const uint8_t*)addr;
    mRdp->texture_to_load.siz = size;
    mRdp->texture_to_load.width = width;
    mRdp->texture_to_load.tex_flags = texFlags;
    mRdp->texture_to_load.raw_tex_metadata = rawTexMetdata;
}

void Interpreter::GfxDpSetTile(uint8_t fmt, uint32_t siz, uint32_t line, uint32_t tmem, uint8_t tile, uint32_t palette,
                               uint32_t cmt, uint32_t maskt, uint32_t shiftt, uint32_t cms, uint32_t masks,
                               uint32_t shifts) {
    if (cms == G_TX_WRAP && masks == G_TX_NOMASK) {
        cms = G_TX_CLAMP;
    }
    if (cmt == G_TX_WRAP && maskt == G_TX_NOMASK) {
        cmt = G_TX_CLAMP;
    }

    mRdp->texture_tile[tile].palette = palette; // palette should set upper 4 bits of color index in 4b mode
    mRdp->texture_tile[tile].fmt = fmt;
    mRdp->texture_tile[tile].siz = siz;
    mRdp->texture_tile[tile].cms = cms;
    mRdp->texture_tile[tile].cmt = cmt;
    mRdp->texture_tile[tile].shifts = shifts;
    mRdp->texture_tile[tile].shiftt = shiftt;
    mRdp->texture_tile[tile].line_size_bytes = line * 8;

    mRdp->texture_tile[tile].tmem = tmem;
    // mRdp->texture_tile[tile].tmem_index = tmem / 256; // tmem is the 64-bit word offset, so 256 words means 2 kB

    mRdp->texture_tile[tile].tmem_index =
        tmem != 0; // assume one texture is loaded at address 0 and another texture at any other address

    mRdp->textures_changed[0] = true;
    mRdp->textures_changed[1] = true;
}

void Interpreter::GfxDpSetTileSize(uint8_t tile, uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t lrt) {
    mRdp->texture_tile[tile].uls = uls;
    mRdp->texture_tile[tile].ult = ult;
    mRdp->texture_tile[tile].lrs = lrs;
    mRdp->texture_tile[tile].lrt = lrt;
    mRdp->textures_changed[0] = true;
    mRdp->textures_changed[1] = true;
}

void Interpreter::GfxDpLoadTlut(uint8_t tile, uint32_t high_index) {
    SUPPORT_CHECK(mRdp->texture_to_load.siz == G_IM_SIZ_16b);

    uint16_t tmem = mRdp->texture_tile[tile].tmem;
    const uint8_t* src = mRdp->texture_to_load.addr;
    uint32_t entryCount = high_index + 1;
    uint32_t byteCount = entryCount * 2;

    if (tmem >= 256) {
        // N64 TMEM palette area starts at tmem word 256. Each CI4 palette = 16 entries = 16 tmem words.
        uint32_t paletteByteOffset = (tmem - 256) * 2;

        if (high_index == 255 && paletteByteOffset == 0) {
            // CI8: full 256-entry palette spanning both halves
            memcpy(mRdp->palette_staging[0], src, 256);
            memcpy(mRdp->palette_staging[1], src + 256, 256);
            mRdp->palettes[0] = mRdp->palette_staging[0];
            mRdp->palettes[1] = mRdp->palette_staging[1];
            mRdp->palette_dram_addr[0] = src;
            mRdp->palette_dram_addr[1] = src + 256;
        } else if (paletteByteOffset < 256) {
            // Palettes 0-7 range
            uint32_t copyLen = (paletteByteOffset + byteCount <= 256) ? byteCount : (256 - paletteByteOffset);
            memcpy(mRdp->palette_staging[0] + paletteByteOffset, src, copyLen);
            mRdp->palettes[0] = mRdp->palette_staging[0];
            mRdp->palette_dram_addr[0] = src;
        } else {
            // Palettes 8-15 range
            uint32_t offset = paletteByteOffset - 256;
            uint32_t copyLen = (offset + byteCount <= 256) ? byteCount : (256 - offset);
            memcpy(mRdp->palette_staging[1] + offset, src, copyLen);
            mRdp->palettes[1] = mRdp->palette_staging[1];
            mRdp->palette_dram_addr[1] = src;
        }
    } else {
        // tmem < 256: non-standard location, fall back to direct pointer
        mRdp->palettes[1] = src;
        mRdp->palette_dram_addr[1] = src;
    }
}

void Interpreter::GfxDpLoadBlock(uint8_t tile, uint32_t uls, uint32_t ult, uint32_t lrs, uint32_t dxt) {
    SUPPORT_CHECK(uls == 0);
    SUPPORT_CHECK(ult == 0);

    // The lrs field rather seems to be number of pixels to load
    uint32_t word_size_shift = 0;
    switch (mRdp->texture_to_load.siz) {
        case G_IM_SIZ_4b:
            word_size_shift = -1;
            break;
        case G_IM_SIZ_8b:
            word_size_shift = 0;
            break;
        case G_IM_SIZ_16b:
            word_size_shift = 1;
            break;
        case G_IM_SIZ_32b:
            word_size_shift = 2;
            break;
    }
    uint32_t orig_size_bytes =
        word_size_shift > 0 ? (lrs + 1) << word_size_shift : (lrs + 1) >> (-(int64_t)word_size_shift);
    uint32_t size_bytes = orig_size_bytes;
    if (mRdp->texture_to_load.raw_tex_metadata.h_byte_scale != 1 ||
        mRdp->texture_to_load.raw_tex_metadata.v_pixel_scale != 1) {
        size_bytes *= mRdp->texture_to_load.raw_tex_metadata.h_byte_scale;
        size_bytes *= mRdp->texture_to_load.raw_tex_metadata.v_pixel_scale;
    }
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].orig_size_bytes = orig_size_bytes;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes = size_bytes;
    // Compute actual per-line DRAM stride from SetTextureImage width when available.
    // The standard gDPLoadTextureBlock macro sets width=1, but manually-built DL
    // commands may set the real pixel width.
    uint32_t actual_line_bytes = size_bytes;
    if (mRdp->texture_to_load.width > 1) {
        uint32_t candidate;
        switch (mRdp->texture_to_load.siz) {
            case G_IM_SIZ_4b:
                candidate = (mRdp->texture_to_load.width + 1) / 2;
                break;
            case G_IM_SIZ_8b:
                candidate = mRdp->texture_to_load.width;
                break;
            case G_IM_SIZ_16b:
                candidate = mRdp->texture_to_load.width * 2;
                break;
            case G_IM_SIZ_32b:
                candidate = mRdp->texture_to_load.width * 4;
                break;
            default:
                candidate = mRdp->texture_to_load.width;
                break;
        }
        if (candidate > 0 && candidate < size_bytes && size_bytes % candidate == 0) {
            actual_line_bytes = candidate;
        }
    }
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes = actual_line_bytes;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes = actual_line_bytes;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].tmem = mRdp->texture_tile[tile].tmem;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].derived_view = false;
    // assert(size_bytes <= 4096 && "bug: too big texture");
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].tex_flags = mRdp->texture_to_load.tex_flags;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata = mRdp->texture_to_load.raw_tex_metadata;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr = mRdp->texture_to_load.addr;

    {
        const uint32_t loadedIndex = mRdp->texture_tile[tile].tmem_index;
        const uint32_t otherIndex = loadedIndex ^ 1u;
        const uint32_t loadBegin = mRdp->texture_tile[tile].tmem;
        const uint32_t loadEnd = loadBegin + ((orig_size_bytes + 7u) >> 3);
        const auto& other = mRdp->loaded_texture[otherIndex];
        if (other.addr != nullptr) {
            const uint32_t otherBegin = other.tmem;
            const uint32_t otherEnd = otherBegin + ((other.orig_size_bytes + 7u) >> 3);
            if (loadBegin < otherEnd && otherBegin < loadEnd) {
                mRdp->loaded_texture[otherIndex].addr = nullptr;
                mRdp->loaded_texture[otherIndex].orig_size_bytes = 0;
                mRdp->loaded_texture[otherIndex].size_bytes = 0;
            }
        }
    }
    //         mRdp->texture_to_load.siz, lrs);

    const std::string texPath =
        mRdp->texture_to_load.raw_tex_metadata.resource != nullptr
            ? GetBaseTexturePath(mRdp->texture_to_load.raw_tex_metadata.resource->GetInitData()->Path)
            : std::string();
    auto maskedTextureIter = mMaskedTextures.find(texPath);
    if (maskedTextureIter != mMaskedTextures.end()) {
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].masked = true;
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].blended =
            maskedTextureIter->second.replacementData != nullptr;
    } else {
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].masked = false;
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].blended = false;
    }

    mRdp->textures_changed[0] = true;
    mRdp->textures_changed[1] = true;
}

void Interpreter::GfxDpLoadTile(uint8_t tile, uint32_t uls, uint32_t ult, uint32_t lrs, uint32_t lrt) {
    SUPPORT_CHECK(tile == G_TX_LOADTILE);

    uint32_t word_size_shift = 0;
    switch (mRdp->texture_to_load.siz) {
        case G_IM_SIZ_4b:
            word_size_shift = 0;
            break;
        case G_IM_SIZ_8b:
            word_size_shift = 0;
            break;
        case G_IM_SIZ_16b:
            word_size_shift = 1;
            break;
        case G_IM_SIZ_32b:
            word_size_shift = 2;
            break;
    }

    uint32_t offset_x = uls >> G_TEXTURE_IMAGE_FRAC;
    uint32_t offset_y = ult >> G_TEXTURE_IMAGE_FRAC;
    uint32_t tile_width = ((lrs - uls) >> G_TEXTURE_IMAGE_FRAC) + 1;
    uint32_t tile_height = ((lrt - ult) >> G_TEXTURE_IMAGE_FRAC) + 1;
    uint32_t full_image_width = mRdp->texture_to_load.width;

    uint32_t offset_x_in_bytes = offset_x << word_size_shift;
    uint32_t tile_line_size_bytes = tile_width << word_size_shift;
    uint32_t full_image_line_size_bytes = full_image_width << word_size_shift;

    uint32_t orig_size_bytes = tile_line_size_bytes * tile_height;
    uint32_t size_bytes = orig_size_bytes;
    uint32_t start_offset_bytes = full_image_line_size_bytes * offset_y + offset_x_in_bytes;

    float h_byte_scale = mRdp->texture_to_load.raw_tex_metadata.h_byte_scale;
    float v_pixel_scale = mRdp->texture_to_load.raw_tex_metadata.v_pixel_scale;

    if (h_byte_scale != 1 || v_pixel_scale != 1) {
        start_offset_bytes = h_byte_scale * (v_pixel_scale * offset_y * full_image_line_size_bytes + offset_x_in_bytes);
        size_bytes *= h_byte_scale * v_pixel_scale;
        full_image_line_size_bytes *= h_byte_scale;
        tile_line_size_bytes *= h_byte_scale;
    }

    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].orig_size_bytes = orig_size_bytes;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes = size_bytes;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes = full_image_line_size_bytes;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes = tile_line_size_bytes;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].tmem = mRdp->texture_tile[tile].tmem;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].derived_view = false;

    //    assert(size_bytes <= 4096 && "bug: too big texture");
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].tex_flags = mRdp->texture_to_load.tex_flags;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata = mRdp->texture_to_load.raw_tex_metadata;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr = mRdp->texture_to_load.addr + start_offset_bytes;

    {
        const uint32_t loadedIndex = mRdp->texture_tile[tile].tmem_index;
        const uint32_t otherIndex = loadedIndex ^ 1u;
        const uint32_t loadBegin = mRdp->texture_tile[tile].tmem;
        const uint32_t loadEnd = loadBegin + ((orig_size_bytes + 7u) >> 3);
        const auto& other = mRdp->loaded_texture[otherIndex];
        if (other.addr != nullptr) {
            const uint32_t otherBegin = other.tmem;
            const uint32_t otherEnd = otherBegin + ((other.orig_size_bytes + 7u) >> 3);
            if (loadBegin < otherEnd && otherBegin < loadEnd) {
                mRdp->loaded_texture[otherIndex].addr = nullptr;
                mRdp->loaded_texture[otherIndex].orig_size_bytes = 0;
                mRdp->loaded_texture[otherIndex].size_bytes = 0;
            }
        }
    }

    const std::string texPath =
        mRdp->texture_to_load.raw_tex_metadata.resource != nullptr
            ? GetBaseTexturePath(mRdp->texture_to_load.raw_tex_metadata.resource->GetInitData()->Path)
            : std::string();
    auto maskedTextureIter = mMaskedTextures.find(texPath);
    if (maskedTextureIter != mMaskedTextures.end()) {
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].masked = true;
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].blended =
            maskedTextureIter->second.replacementData != nullptr;
    } else {
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].masked = false;
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].blended = false;
    }

    mRdp->texture_tile[tile].uls = uls;
    mRdp->texture_tile[tile].ult = ult;
    mRdp->texture_tile[tile].lrs = lrs;
    mRdp->texture_tile[tile].lrt = lrt;

    mRdp->textures_changed[0] = true;
    mRdp->textures_changed[1] = true;
}

/*static uint8_t color_comb_component(uint32_t v) {
    switch (v) {
        case G_CCMUX_TEXEL0:
            return CC_TEXEL0;
        case G_CCMUX_TEXEL1:
            return CC_TEXEL1;
        case G_CCMUX_PRIMITIVE:
            return CC_PRIM;
        case G_CCMUX_SHADE:
            return CC_SHADE;
        case G_CCMUX_ENVIRONMENT:
            return CC_ENV;
        case G_CCMUX_TEXEL0_ALPHA:
            return CC_TEXEL0A;
        case G_CCMUX_LOD_FRACTION:
            return CC_LOD;
        default:
            return CC_0;
    }
}

static inline uint32_t color_comb(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return color_comb_component(a) |
           (color_comb_component(b) << 3) |
           (color_comb_component(c) << 6) |
           (color_comb_component(d) << 9);
}

static void GfxDpSetCombineMode(uint32_t rgb, uint32_t alpha) {
    mRdp->combine_mode = rgb | (alpha << 12);
}*/

void Interpreter::GfxDpSetCombineMode(uint32_t rgb, uint32_t alpha, uint32_t rgb_cyc2, uint32_t alpha_cyc2) {
    mRdp->combine_mode = rgb | (alpha << 16) | ((uint64_t)rgb_cyc2 << 28) | ((uint64_t)alpha_cyc2 << 44);
}

static inline uint32_t color_comb(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return (a & 0xf) | ((b & 0xf) << 4) | ((c & 0x1f) << 8) | ((d & 7) << 13);
}

static inline uint32_t alpha_comb(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return (a & 7) | ((b & 7) << 3) | ((c & 7) << 6) | ((d & 7) << 9);
}

// Sign-extend a 9-bit value (used for G_SETCONVERT K0..K5).
static inline int16_t sign_extend_9(uint32_t v) {
    return (int16_t)((v & 0x100) ? (int32_t)(v | 0xFFFFFE00u) : (int32_t)v);
}

void Interpreter::GfxDpSetGrayscaleColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    mRdp->grayscale_color.r = r;
    mRdp->grayscale_color.g = g;
    mRdp->grayscale_color.b = b;
    mRdp->grayscale_color.a = a;
}

void Interpreter::GfxDpSetEnvColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    mRdp->env_color.r = r;
    mRdp->env_color.g = g;
    mRdp->env_color.b = b;
    mRdp->env_color.a = a;
}

void Interpreter::GfxDpSetPrimColor(uint8_t m, uint8_t l, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    mRdp->prim_lod_fraction = l;
    mRdp->prim_color.r = r;
    mRdp->prim_color.g = g;
    mRdp->prim_color.b = b;
    mRdp->prim_color.a = a;
}

void Interpreter::GfxDpSetFogColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    mRdp->fog_color.r = r;
    mRdp->fog_color.g = g;
    mRdp->fog_color.b = b;
    mRdp->fog_color.a = a;
}

void Interpreter::GfxDpSetBlendColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    mRdp->blend_color.r = r;
    mRdp->blend_color.g = g;
    mRdp->blend_color.b = b;
    mRdp->blend_color.a = a;
}

void Interpreter::GfxDpSetFillColor(uint32_t packed_color) {
    uint16_t col16 = (uint16_t)packed_color;
    uint32_t r = col16 >> 11;
    uint32_t g = (col16 >> 6) & 0x1f;
    uint32_t b = (col16 >> 1) & 0x1f;
    uint32_t a = col16 & 1;
    mRdp->fill_color.r = SCALE_5_8(r);
    mRdp->fill_color.g = SCALE_5_8(g);
    mRdp->fill_color.b = SCALE_5_8(b);
    mRdp->fill_color.a = a * 255;
}

void Interpreter::GfxDrawRectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
    uint32_t saved_other_mode_h = mRdp->other_mode_h;
    uint32_t cycle_type = (mRdp->other_mode_h & (3U << G_MDSFT_CYCLETYPE));

    if (cycle_type == G_CYC_COPY) {
        mRdp->other_mode_h = (mRdp->other_mode_h & ~(3U << G_MDSFT_TEXTFILT)) | G_TF_POINT;
    }

    // U10.2 coordinates
    float ulxf = ulx;
    float ulyf = uly;
    float lrxf = lrx;
    float lryf = lry;

    ulxf = ulxf / (4.0f * HALF_SCREEN_WIDTH(mActiveFrameBuffer)) - 1.0f;
    ulyf = -(ulyf / (4.0f * HALF_SCREEN_HEIGHT(mActiveFrameBuffer))) + 1.0f;
    lrxf = lrxf / (4.0f * HALF_SCREEN_WIDTH(mActiveFrameBuffer)) - 1.0f;
    lryf = -(lryf / (4.0f * HALF_SCREEN_HEIGHT(mActiveFrameBuffer))) + 1.0f;

    ulxf = AdjXForAspectRatio(ulxf);
    lrxf = AdjXForAspectRatio(lrxf);

    struct LoadedVertex* ul = &mRsp->loaded_vertices[MAX_VERTICES + 0];
    struct LoadedVertex* ll = &mRsp->loaded_vertices[MAX_VERTICES + 1];
    struct LoadedVertex* lr = &mRsp->loaded_vertices[MAX_VERTICES + 2];
    struct LoadedVertex* ur = &mRsp->loaded_vertices[MAX_VERTICES + 3];

    ul->x = ulxf;
    ul->y = ulyf;
    ul->z = -1.0f;
    ul->w = 1.0f;

    ll->x = ulxf;
    ll->y = lryf;
    ll->z = -1.0f;
    ll->w = 1.0f;

    lr->x = lrxf;
    lr->y = lryf;
    lr->z = -1.0f;
    lr->w = 1.0f;

    ur->x = lrxf;
    ur->y = ulyf;
    ur->z = -1.0f;
    ur->w = 1.0f;

    // The coordinates for texture rectangle shall bypass the viewport setting
    struct XYWidthHeight default_viewport;
    if (!mFbActive) {
        default_viewport = { 0, (int16_t)mNativeDimensions.height, mNativeDimensions.width, mNativeDimensions.height };
    } else {
        default_viewport = { 0, (int16_t)mActiveFrameBuffer->second.orig_height, mActiveFrameBuffer->second.orig_width,
                             mActiveFrameBuffer->second.orig_height };
    }

    struct XYWidthHeight viewport_saved = mRdp->viewport;
    uint32_t geometry_mode_saved = mRsp->geometry_mode;

    AdjustVIewportOrScissor(&default_viewport);

    mRdp->viewport = default_viewport;
    mRdp->viewport_or_scissor_changed = true;
    mRsp->geometry_mode = 0;

    GfxSpTri1(MAX_VERTICES + 0, MAX_VERTICES + 1, MAX_VERTICES + 3, true);
    GfxSpTri1(MAX_VERTICES + 1, MAX_VERTICES + 2, MAX_VERTICES + 3, true);

    mRsp->geometry_mode = geometry_mode_saved;
    mRdp->viewport = viewport_saved;
    mRdp->viewport_or_scissor_changed = true;

    if (cycle_type == G_CYC_COPY) {
        mRdp->other_mode_h = saved_other_mode_h;
    }
}

void Interpreter::GfxDpTextureRectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, uint8_t tile, int16_t uls,
                                        int16_t ult, int16_t dsdx, int16_t dtdy, bool flip) {
    uint64_t saved_combine_mode = mRdp->combine_mode;
    if ((mRdp->other_mode_h & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_COPY) {
        // Per RDP Command Summary Set Tile's shift s and this dsdx should be set to 4 texels
        // Divide by 4 to get 1 instead
        dsdx >>= 2;

        // Color combiner is turned off in copy mode
        GfxDpSetCombineMode(color_comb(0, 0, 0, G_CCMUX_TEXEL0), alpha_comb(0, 0, 0, G_ACMUX_TEXEL0), 0, 0);

        // Per documentation one extra pixel is added in this modes to each edge
        lrx += 1 << 2;
        lry += 1 << 2;
    }

    // uls and ult are S10.5
    // dsdx and dtdy are S5.10
    // lrx, lry, ulx, uly are U10.2
    // lrs, lrt are S10.5
    if (flip) {
        dsdx = -dsdx;
        dtdy = -dtdy;
    }
    int16_t width = !flip ? lrx - ulx : lry - uly;
    int16_t height = !flip ? lry - uly : lrx - ulx;
    float lrs = ((uls << 7) + dsdx * width) >> 7;
    float lrt = ((ult << 7) + dtdy * height) >> 7;

    LoadedVertex* ul = &mRsp->loaded_vertices[MAX_VERTICES + 0];
    LoadedVertex* ll = &mRsp->loaded_vertices[MAX_VERTICES + 1];
    LoadedVertex* lr = &mRsp->loaded_vertices[MAX_VERTICES + 2];
    LoadedVertex* ur = &mRsp->loaded_vertices[MAX_VERTICES + 3];
    ul->u = uls;
    ul->v = ult;
    lr->u = lrs;
    lr->v = lrt;
    if (!flip) {
        ll->u = uls;
        ll->v = lrt;
        ur->u = lrs;
        ur->v = ult;
    } else {
        ll->u = lrs;
        ll->v = ult;
        ur->u = uls;
        ur->v = lrt;
    }

    uint8_t saved_tile = mRdp->first_tile_index;
    if (saved_tile != tile) {
        mRdp->textures_changed[0] = true;
        mRdp->textures_changed[1] = true;
    }
    mRdp->first_tile_index = tile;

    GfxDrawRectangle(ulx, uly, lrx, lry);
    if (saved_tile != tile) {
        mRdp->textures_changed[0] = true;
        mRdp->textures_changed[1] = true;
    }
    mRdp->first_tile_index = saved_tile;
    mRdp->combine_mode = saved_combine_mode;
}

void Interpreter::GfxDpImageRectangle(int32_t tile, int32_t w, int32_t h, int32_t ulx, int32_t uly, int16_t uls,
                                      int16_t ult, int32_t lrx, int32_t lry, int16_t lrs, int16_t lrt) {

    LoadedVertex* ul = &mRsp->loaded_vertices[MAX_VERTICES + 0];
    LoadedVertex* ll = &mRsp->loaded_vertices[MAX_VERTICES + 1];
    LoadedVertex* lr = &mRsp->loaded_vertices[MAX_VERTICES + 2];
    LoadedVertex* ur = &mRsp->loaded_vertices[MAX_VERTICES + 3];
    ul->u = uls * 32;
    ul->v = ult * 32;
    lr->u = lrs * 32;
    lr->v = lrt * 32;
    ll->u = uls * 32;
    ll->v = lrt * 32;
    ur->u = lrs * 32;
    ur->v = ult * 32;

    // ensure we have the correct texture size, format and starting position
    mRdp->texture_tile[tile].siz = G_IM_SIZ_8b;
    mRdp->texture_tile[tile].fmt = G_IM_FMT_RGBA;
    mRdp->texture_tile[tile].cms = 0;
    mRdp->texture_tile[tile].cmt = 0;
    mRdp->texture_tile[tile].shifts = 0;
    mRdp->texture_tile[tile].shiftt = 0;
    mRdp->texture_tile[tile].uls = 0 * 4;
    mRdp->texture_tile[tile].ult = 0 * 4;
    mRdp->texture_tile[tile].lrs = w * 4;
    mRdp->texture_tile[tile].lrt = h * 4;
    mRdp->texture_tile[tile].line_size_bytes = w << (mRdp->texture_tile[tile].siz >> 1);

    auto& loadtex = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index];
    loadtex.full_image_line_size_bytes = loadtex.line_size_bytes = mRdp->texture_tile[tile].line_size_bytes;
    loadtex.size_bytes = loadtex.orig_size_bytes = loadtex.line_size_bytes * h;

    uint8_t saved_tile = mRdp->first_tile_index;
    if (saved_tile != tile) {
        mRdp->textures_changed[0] = true;
        mRdp->textures_changed[1] = true;
    }
    mRdp->first_tile_index = tile;

    GfxDrawRectangle(ulx, uly, lrx, lry);
    if (saved_tile != tile) {
        mRdp->textures_changed[0] = true;
        mRdp->textures_changed[1] = true;
    }
    mRdp->first_tile_index = saved_tile;
}

void Interpreter::GfxDpFillRectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
    if (mRdp->color_image_address == mRdp->z_buf_address) {
        // Fullscreen Z clears are redundant — already done by glClear at frame start.
        bool isFullScreen = (ulx <= 0 && uly <= 0 && lrx >= (int32_t)(mNativeDimensions.width - 1) * 4 &&
                             lry >= (int32_t)(mNativeDimensions.height - 1) * 4);
        if (isFullScreen) {
            return;
        }

        // Partial depth clear (e.g. HUD model regions): clear the actual depth buffer
        // via a scissored depth clear instead of drawing a colored rect to the color buffer.
        Flush();

        // Convert U10.2 coords to pixel coords and add +1 pixel for fill mode
        int32_t expanded_lrx = lrx + (1 << 2);
        int32_t expanded_lry = lry + (1 << 2);
        float x = ulx / 4.0f;
        float y = expanded_lry / 4.0f;
        float w = (expanded_lrx - ulx) / 4.0f;
        float h = (expanded_lry - uly) / 4.0f;

        struct XYWidthHeight area;
        area.x = (int16_t)x;
        area.y = (int16_t)y;
        area.width = (uint32_t)w;
        area.height = (uint32_t)h;
        AdjustVIewportOrScissor(&area);

        mRapi->ClearDepthRegion(area.x, area.y, area.width, area.height);
        return;
    }
    uint32_t mode = (mRdp->other_mode_h & (3U << G_MDSFT_CYCLETYPE));

    // Expand fullscreen fill rects to cover widescreen viewports.
    // Without this, screen clears and fades only cover the native 4:3 area.
    if (ulx == 0 && uly == 0) {
        bool isFullScreen = (lrx == ((int32_t)(mNativeDimensions.width - 1) * 4) &&
                             lry == ((int32_t)(mNativeDimensions.height - 1) * 4));
        if (isFullScreen) {
            ulx = -1024;
            uly = -1024;
            lrx = 2048;
            lry = 2048;
        }
    }

    if (mode == G_CYC_COPY || mode == G_CYC_FILL) {
        // Per documentation one extra pixel is added in this modes to each edge
        lrx += 1 << 2;
        lry += 1 << 2;
    }

    for (int i = MAX_VERTICES; i < MAX_VERTICES + 4; i++) {
        LoadedVertex* v = &mRsp->loaded_vertices[i];
        v->color = mRdp->fill_color;
    }

    uint64_t saved_combine_mode = mRdp->combine_mode;

    if (mode == G_CYC_FILL) {
        GfxDpSetCombineMode(color_comb(0, 0, 0, G_CCMUX_SHADE), alpha_comb(0, 0, 0, G_ACMUX_SHADE), 0, 0);
    }

    GfxDrawRectangle(ulx, uly, lrx, lry);
    mRdp->combine_mode = saved_combine_mode;
}

void Interpreter::GfxDpSetZImage(void* zBufAddr) {
    mRdp->z_buf_address = zBufAddr;
}

void Interpreter::GfxDpSetColorImage(uint32_t format, uint32_t size, uint32_t width, void* address) {
    mRdp->color_image_address = address;
    mRdp->color_image_width = width;
    mRdp->color_image_format = (uint8_t)format;
    mRdp->color_image_size = (uint8_t)size;

}

void Interpreter::GfxSpSetOtherMode(uint32_t shift, uint32_t num_bits, uint64_t mode) {
    uint64_t mask = (((uint64_t)1 << num_bits) - 1) << shift;
    uint64_t om = mRdp->other_mode_l | ((uint64_t)mRdp->other_mode_h << 32);
    om = (om & ~mask) | mode;
    mRdp->other_mode_l = (uint32_t)om;
    mRdp->other_mode_h = (uint32_t)(om >> 32);
}

void Interpreter::GfxDpSetOtherMode(uint32_t h, uint32_t l) {
    mRdp->other_mode_h = h;
    mRdp->other_mode_l = l;
}

void Interpreter::Gfxs2dexBgCopy(F3DuObjBg* bg) {
    /*
    bg->b.imageX = 0;
    bg->b.imageW = width * 4;
    bg->b.frameX = frameX * 4;
    bg->b.imageY = 0;
    bg->b.imageH = height * 4;
    bg->b.frameY = frameY * 4;
    bg->b.imagePtr = source;
    bg->b.imageLoad = G_BGLT_LOADTILE;
    bg->b.imageFmt = fmt;
    bg->b.imageSiz = siz;
    bg->b.imagePal = 0;
    bg->b.imageFlip = 0;
    */

    uintptr_t data = (uintptr_t)bg->b.imagePtr;

    uint32_t texFlags = 0;
    RawTexMetadata rawTexMetadata = {};

    s16 dsdx = 4 << 10;
    s16 uls = bg->b.imageX << 3;
    // Flip flag only flips horizontally
    if (bg->b.imageFlip == G_BG_FLAG_FLIPS) {
        dsdx = -dsdx;
        uls = (bg->b.imageW - bg->b.imageX) << 3;
    }

    SUPPORT_CHECK(bg->b.imageSiz == G_IM_SIZ_16b);
    GfxDpSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, nullptr, texFlags, rawTexMetadata, (void*)data);
    GfxDpSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    GfxDpLoadBlock(G_TX_LOADTILE, 0, 0, (bg->b.imageW * bg->b.imageH >> 4) - 1, 0);
    GfxDpSetTile(bg->b.imageFmt, G_IM_SIZ_16b, bg->b.imageW >> 4, 0, G_TX_RENDERTILE, bg->b.imagePal, 0, 0, 0, 0, 0, 0);
    GfxDpSetTileSize(G_TX_RENDERTILE, 0, 0, bg->b.imageW, bg->b.imageH);
    GfxDpTextureRectangle(bg->b.frameX, bg->b.frameY, bg->b.frameX + bg->b.imageW - 4, bg->b.frameY + bg->b.imageH - 4,
                          G_TX_RENDERTILE, uls, bg->b.imageY << 3, dsdx, 1 << 10, false);
}

void Interpreter::Gfxs2dexBg1cyc(F3DuObjBg* bg) {
    if (bg == nullptr || bg->s.imagePtr == nullptr) return;

    const uint32_t imagePtr = [&]() {
        uint32_t address = 0;
        memcpy(&address, bg->s.padding, sizeof(address));
        return address & 0x00FFFFFFu;
    }();
    const uint32_t imageWidth = bg->s.imageW >> 2;
    const uint32_t imageHeight = bg->s.imageH >> 2;
    if (imageWidth == 0 || imageHeight == 0 || bg->s.scaleW == 0 || bg->s.scaleH == 0) return;

    const uint8_t* stagedImage = reinterpret_cast<const uint8_t*>(bg->s.imagePtr);
    uint32_t stagedImageBytes = 0;
    switch (bg->s.imageSiz) {
        case G_IM_SIZ_4b: stagedImageBytes = ((imageWidth * imageHeight) + 1u) >> 1; break;
        case G_IM_SIZ_8b: stagedImageBytes = imageWidth * imageHeight; break;
        case G_IM_SIZ_16b: stagedImageBytes = imageWidth * imageHeight * 2u; break;
        case G_IM_SIZ_32b: stagedImageBytes = imageWidth * imageHeight * 4u; break;
        default: return;
    }

    struct BgTileState {
        uint8_t fmt = 0;
        uint8_t siz = 0;
        uint16_t line = 0;
        uint16_t tmem = 0;
        uint8_t palette = 0;
        uint8_t cmt = 0;
        uint8_t maskt = 0;
        uint8_t shiftt = 0;
        uint8_t cms = 0;
        uint8_t masks = 0;
        uint8_t shifts = 0;
        uint16_t uls = 0;
        uint16_t ult = 0;
        uint16_t lrs = 0;
        uint16_t lrt = 0;
    };
    struct BgTextureImageState {
        uint8_t fmt = 0;
        uint8_t siz = 0;
        uint16_t width = 0;
        uint32_t address = 0;
        bool valid = false;
    };

    BgTileState bgTiles[8] = {};
    BgTextureImageState bgTimg = {};
    uint8_t shadowTmem[4096] = {};

    auto readRdramByte = [&](uint32_t address, uint8_t& value) -> bool {
        address &= 0x00FFFFFFu;
        if (mRdramBase != nullptr && address < mRdramSize) {
            value = mRdramBase[address ^ 3u];
            return true;
        }
        if (address >= imagePtr) {
            const uint32_t offset = address - imagePtr;
            if (offset < stagedImageBytes) {
                value = stagedImage[offset];
                return true;
            }
        }
        return false;
    };

    auto setTileCommand = [&](uint32_t w0, uint32_t w1) {
        const uint32_t tile = (w1 >> 24) & 7u;
        BgTileState& t = bgTiles[tile];
        t.fmt = (uint8_t)((w0 >> 21) & 7u);
        t.siz = (uint8_t)((w0 >> 19) & 3u);
        t.line = (uint16_t)((w0 >> 9) & 0x1FFu);
        t.tmem = (uint16_t)(w0 & 0x1FFu);
        t.palette = (uint8_t)((w1 >> 20) & 0xFu);
        t.cmt = (uint8_t)((w1 >> 18) & 3u);
        t.maskt = (uint8_t)((w1 >> 14) & 0xFu);
        t.shiftt = (uint8_t)((w1 >> 10) & 0xFu);
        t.cms = (uint8_t)((w1 >> 8) & 3u);
        t.masks = (uint8_t)((w1 >> 4) & 0xFu);
        t.shifts = (uint8_t)(w1 & 0xFu);
    };

    auto setTileSizeCommand = [&](uint32_t w0, uint32_t w1) {
        const uint32_t tile = (w1 >> 24) & 7u;
        BgTileState& t = bgTiles[tile];
        t.uls = (uint16_t)((w0 >> 12) & 0xFFFu);
        t.ult = (uint16_t)(w0 & 0xFFFu);
        t.lrs = (uint16_t)((w1 >> 12) & 0xFFFu);
        t.lrt = (uint16_t)(w1 & 0xFFFu);
    };

    auto setTextureImageCommand = [&](uint32_t w0, uint32_t w1) {
        bgTimg.fmt = (uint8_t)((w0 >> 21) & 7u);
        bgTimg.siz = (uint8_t)((w0 >> 19) & 3u);
        bgTimg.width = (uint16_t)((w0 & 0xFFFu) + 1u);
        bgTimg.address = w1 & 0x00FFFFFFu;
        bgTimg.valid = true;
    };

    auto loadTileCommand = [&](uint32_t w0, uint32_t w1) -> bool {
        if (!bgTimg.valid) return false;
        const uint32_t tileIndex = (w1 >> 24) & 7u;
        const BgTileState& tile = bgTiles[tileIndex];
        const uint32_t uls = (w0 >> 12) & 0xFFFu;
        const uint32_t ult = w0 & 0xFFFu;
        const uint32_t lrs = (w1 >> 12) & 0xFFFu;
        const uint32_t lrt = w1 & 0xFFFu;
        if (lrs < uls || lrt < ult) return false;

        uint32_t bytesPerTexel = 0;
        switch (bgTimg.siz) {
            case G_IM_SIZ_8b: bytesPerTexel = 1; break;
            case G_IM_SIZ_16b: bytesPerTexel = 2; break;
            case G_IM_SIZ_32b: bytesPerTexel = 4; break;
            default: return false;
        }

        const uint32_t srcX = uls >> 2;
        const uint32_t srcY = ult >> 2;
        const uint32_t widthTexels = ((lrs - uls) >> 2) + 1u;
        const uint32_t heightTexels = ((lrt - ult) >> 2) + 1u;
        const uint32_t srcLineBytes = (uint32_t)bgTimg.width * bytesPerTexel;
        const uint32_t copyBytes = widthTexels * bytesPerTexel;
        const uint32_t dstLineBytes = tile.line != 0 ? (uint32_t)tile.line * 8u : copyBytes;
        const uint32_t dstBase = (uint32_t)tile.tmem * 8u;
        if (copyBytes == 0 || dstLineBytes == 0 || dstBase >= sizeof(shadowTmem)) return false;

        for (uint32_t y = 0; y < heightTexels; ++y) {
            const uint64_t srcRow = (uint64_t)bgTimg.address + (uint64_t)(srcY + y) * srcLineBytes +
                                    (uint64_t)srcX * bytesPerTexel;
            const uint64_t dstRow = (uint64_t)dstBase + (uint64_t)y * dstLineBytes;
            if (dstRow >= sizeof(shadowTmem)) break;
            const uint32_t rowCopy = (uint32_t)std::min<uint64_t>(copyBytes, sizeof(shadowTmem) - dstRow);
            for (uint32_t x = 0; x < rowCopy; ++x) {
                uint8_t value = 0;
                if (!readRdramByte((uint32_t)(srcRow + x), value)) return false;
                shadowTmem[dstRow + x] = value;
            }
        }
        return true;
    };

    RawTexMetadata rawTexMetadata = {};
    rawTexMetadata.h_byte_scale = 1;
    rawTexMetadata.v_pixel_scale = 1;

    auto drawTextureRectangleFromShadow = [&](uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3) {
        const int32_t lrx = (int32_t)((w0 >> 12) & 0xFFFu);
        const int32_t lry = (int32_t)(w0 & 0xFFFu);
        const uint32_t tileIndex = (w1 >> 24) & 7u;
        const int32_t ulx = (int32_t)((w1 >> 12) & 0xFFFu);
        const int32_t uly = (int32_t)(w1 & 0xFFFu);
        const int16_t uls = (int16_t)(w2 >> 16);
        const int16_t ult = (int16_t)(w2 & 0xFFFFu);
        const int16_t dsdx = (int16_t)(w3 >> 16);
        const int16_t dtdy = (int16_t)(w3 & 0xFFFFu);
        const BgTileState& tile = bgTiles[tileIndex];

        const uint32_t lineBytes = (uint32_t)tile.line * 8u;
        const uint32_t tmemStart = (uint32_t)tile.tmem * 8u;
        const uint32_t tmemLimit = tile.fmt == G_IM_FMT_CI ? 2048u : 4096u;
        if (lineBytes == 0 || tmemStart >= tmemLimit) return;
        const uint32_t availableBytes = tmemLimit - tmemStart;
        const uint32_t textureHeight = availableBytes / lineBytes;
        if (textureHeight == 0) return;

        uint32_t textureWidth = 0;
        switch (tile.siz) {
            case G_IM_SIZ_4b: textureWidth = lineBytes * 2u; break;
            case G_IM_SIZ_8b: textureWidth = lineBytes; break;
            case G_IM_SIZ_16b: textureWidth = lineBytes / 2u; break;
            case G_IM_SIZ_32b: textureWidth = lineBytes / 4u; break;
            default: return;
        }
        if (textureWidth == 0) return;

        const uint32_t uploadBytes = lineBytes * textureHeight;
        if (uploadBytes == 0 || uploadBytes > sizeof(mS2dexBgStripStaging)) return;

        Flush();
        TextureCacheDelete(mS2dexBgStripStaging);
        memcpy(mS2dexBgStripStaging, shadowTmem + tmemStart, uploadBytes);

        GfxDpSetTextureImage(tile.fmt, tile.siz, textureWidth, nullptr, 0, rawTexMetadata, mS2dexBgStripStaging);
        GfxDpSetTile(tile.fmt, tile.siz, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
        GfxDpLoadBlock(G_TX_LOADTILE, 0, 0, textureWidth * textureHeight - 1u, 0);
        GfxDpSetTile(tile.fmt, tile.siz, tile.line, 0, (uint8_t)tileIndex, tile.palette,
                     tile.cmt, tile.maskt, tile.shiftt, tile.cms, tile.masks, tile.shifts);
        GfxDpSetTileSize((uint8_t)tileIndex, 0, 0, (uint16_t)((textureWidth - 1u) << 2),
                         (uint16_t)((textureHeight - 1u) << 2));
        GfxDpTextureRectangle(ulx, uly, lrx, lry, (uint8_t)tileIndex, uls, ult, dsdx, dtdy, false);

    };

    mRdp->other_mode_h = (mRdp->other_mode_h & ~(3u << G_MDSFT_CYCLETYPE)) | G_CYC_1CYCLE;

    const int32_t scissorXh = mRdp->scissor_lrx_raw != 0 ? (int32_t)mRdp->scissor_ulx_raw : 0;
    const int32_t scissorYh = mRdp->scissor_lry_raw != 0 ? (int32_t)mRdp->scissor_uly_raw : 0;
    const int32_t scissorXl = mRdp->scissor_lrx_raw != 0 ? (int32_t)mRdp->scissor_lrx_raw : (int32_t)mNativeDimensions.width * 4;
    const int32_t scissorYl = mRdp->scissor_lry_raw != 0 ? (int32_t)mRdp->scissor_lry_raw : (int32_t)mNativeDimensions.height * 4;

    int32_t E2_1 = 0;
    uint16_t F1_1 = 0;
    uint16_t P = 0;
    int16_t H2 = 0;

    int16_t Aw = (int16_t)((int32_t)bg->s.frameW - ((((uint32_t)bg->s.imageW << 10) / bg->s.scaleW - 1u) & 0xFFFCu));
    if (Aw < 0) Aw = 0;

    int16_t frameX = bg->s.frameX;
    int16_t frameY = bg->s.frameY;
    int32_t imageYorig = bg->s.imageYorig;
    if ((bg->s.imageFlip & G_BG_FLAG_FLIPS) != 0) frameX = (int16_t)(frameX + Aw);

    int16_t Bw = (int16_t)std::max<int32_t>(0, scissorXh - frameX);
    int16_t Cw = (int16_t)std::max<int32_t>(0, frameX + (int32_t)bg->s.frameW - scissorXl - Aw);
    if ((int16_t)bg->s.frameW - Aw - Bw - Cw <= 0) return;
    const int16_t Dw = (int16_t)(frameX + Bw);
    const int16_t Ew = (int16_t)(frameX + bg->s.frameW - Aw - Cw);

    int16_t Ah = (int16_t)((int32_t)bg->s.frameH - ((((uint32_t)bg->s.imageH << 10) / bg->s.scaleH - 1u) & 0xFFFCu));
    if (Ah < 0) Ah = 0;
    const int16_t Bh = (int16_t)std::max<int32_t>(0, scissorYh - frameY);
    const int16_t Ch = (int16_t)std::max<int32_t>(0, frameY + (int32_t)bg->s.frameH - scissorYl - Ah);
    if ((int16_t)bg->s.frameH - Ah - Bh - Ch <= 0) return;
    const int16_t Dh = (int16_t)(((int32_t)(frameY + Bh) * 0x4000) >> 16);
    const int16_t Eh = (int16_t)(((int32_t)(bg->s.frameH - Ah - Bh - Ch) * 0x4000) >> 16);

    const uint16_t Fw = (uint16_t)(bg->s.imageW << 3);
    if ((bg->s.imageFlip & G_BG_FLAG_FLIPS) != 0) Bw = Cw;
    int16_t Gw = (int16_t)((((uint32_t)bg->s.scaleW * (uint16_t)Bw * 0x0200u) >> 16) + bg->s.imageX);
    int32_t Hw = (int32_t)Gw - Fw;
    const uint16_t Fh = (uint16_t)(bg->s.imageH << 3);
    int16_t Gh = (int16_t)((((uint32_t)bg->s.scaleH * (uint16_t)Bh * 0x0200u) >> 16) + bg->s.imageY);
    while (Hw >= 0) {
        Gw = (int16_t)(Gw - Fw);
        Gh = (int16_t)(Gh + 0x20);
        imageYorig += 0x20;
        Hw = (int32_t)Gw - Fw;
    }
    int32_t Hh = (int32_t)Gh - Fh;
    while (Hh >= 0) {
        Gh = (int16_t)(Gh - Fh);
        imageYorig -= Fh;
        Hh = (int32_t)Gh - Fh;
    }

    const int32_t I = ((int32_t)Gh - imageYorig) << 5;
    const int16_t J = (int16_t)(((uint32_t)bg->s.scaleW * (uint16_t)(bg->s.frameW - Aw - Bw - Cw)) >> 7);
    const uint8_t J_2 = (J + Gw + 0x0B < Fw) ? 0u : 1u;
    const uint8_t K = (uint8_t)((mS2dexObjRenderMode & 0x08u) >> 3);

    static const uint32_t aSize[] = { 0x01FF0080u, 0x00FF0100u, 0x007F0200u, 0x003F0400u };
    const uint32_t M = aSize[bg->s.imageSiz];
    const uint16_t MHi = (uint16_t)(M >> 16);
    const uint16_t MLo = (uint16_t)M;
    uint16_t L = 0x0400u;
    if (bg->s.imageFmt == G_IM_FMT_CI) L = 0x0200u;
    const uint16_t N = (uint16_t)(((uint32_t)bg->s.frameW * bg->s.scaleW) >> 7) + (uint16_t)(K << 5);
    const uint16_t O = std::min<uint16_t>(N, Fw);
    P = (uint16_t)((((uint32_t)O + MHi) * MLo >> 16) + 1u);
    if (P == 0) return;

    const uint16_t Q = (uint16_t)(L / ((uint32_t)P * 2u) + (K != 0 ? 0xFFFFu : 0u));
    if (Q == 0) return;
    const int32_t R = (int32_t)((0x100000u * (uint32_t)Q) / bg->s.scaleH);
    if (R <= 0) return;

    const int32_t S = (int32_t)((((int64_t)I * 0x4000000ll / bg->s.scaleH) >> 16) & 0xFFFFFC00ll);
    const int16_t T = (int16_t)(((int64_t)(0xFFFFFFFFu / (uint32_t)R) * (int64_t)S) >> 32);
    const int32_t U = R * ((int32_t)T + 1);
    int16_t V = T;
    if (U <= S) ++V;
    const int32_t W = R * V;
    const int32_t Z = R - S + W;

    const uint32_t A1 = (uint32_t)(S - (W & 0xFFFFFC00));
    const uint32_t B1 = (A1 * 0x0040u) >> 16;
    const uint32_t C1 = B1 * bg->s.scaleH;
    const uint16_t D1 = (uint16_t)(((C1 * 0x0040u) >> 16) & 0xFFFFu);
    const uint16_t E1 = (uint16_t)(Q - D1);
    const uint16_t F1 = (uint16_t)(C1 & 0xFFFFu);
    F1_1 = (uint16_t)((F1 >> 5) & 0x001Fu);

    int16_t A2 = (int16_t)(((uint32_t)Q * (uint32_t)(uint16_t)V + (uint32_t)D1 +
                            (uint32_t)((imageYorig << 11) >> 16)) & 0xFFFFu);
    const uint16_t B2 = (uint16_t)((bg->s.imageH << 14) >> 16);
    int16_t A2_1 = A2 >= 0 ? A2 : (int16_t)(A2 + B2);
    if ((int32_t)A2 - B2 >= 0) A2_1 = (int16_t)(A2_1 - B2);
    const int16_t C2 = (int16_t)((((int32_t)Gw * MLo) >> 16) << 3);
    const int16_t D2 = (int16_t)((((int32_t)Fw * MLo) >> 16) << 3);
    const uint16_t C2Bits = (uint16_t)C2;
    const uint16_t D2Bits = (uint16_t)D2;
    E2_1 = (int32_t)A2_1 * D2 + C2 + (int32_t)imagePtr;
    const int32_t F2 = (int32_t)E1 * D2;
    const int32_t G2 = (int32_t)Q * D2;
    H2 = (int16_t)(Gw & MHi);
    if ((bg->s.imageFlip & G_BG_FLAG_FLIPS) != 0) H2 = (int16_t)((H2 + J) * 0xFFFF);

    const uint32_t I2 = 0xFD100000u | ((uint32_t)(D2 >> 1) - 1u);
    const uint32_t J2 = 0xF5100000u | ((uint32_t)P << 9);
    const uint32_t J2_1 = (J2 & 0xFF00FFFFu) | ((((uint32_t)bg->s.imageFmt << 5) | ((uint32_t)bg->s.imageSiz << 3)) << 16);
    const uint32_t K2 = ((uint32_t)bg->s.imagePal << 20) | 0x0007C1F0u;
    const uint16_t L2 = (uint16_t)(bg->s.imageH >> 2);

    setTileCommand(J2, 0x27000000u);
    setTileCommand(J2_1, K2);
    setTileSizeCommand(0xF2000000u, 0);


    int16_t AA = (int16_t)((uint16_t)L2 - (uint16_t)A2_1);
    int32_t CC = (int32_t)(uint16_t)E1;
    uint32_t DD = (uint32_t)Z;
    const uint32_t EE = (uint32_t)R;
    int32_t FF = (int32_t)(uint16_t)Eh;
    uint16_t JJ = (uint16_t)Dh;
    uint32_t GG = (uint32_t)F2;
    const uint32_t HH = (uint32_t)(uint16_t)Dw << 12;
    const uint32_t II = (uint32_t)(uint16_t)Ew << 12;
    uint32_t step = 2;
    int32_t KK = 0;
    int16_t LL = 0;
    int16_t MM = 0;
    int16_t NN = 0;
    int16_t RR = 0;
    int16_t AAA = 0;
    uint32_t SS = 0;

    auto setTexture = [&]() {
        setTileCommand(J2 | (uint32_t)(int32_t)AAA, 0x27000000u);
        setTextureImageCommand(I2, SS);
        const uint32_t loadW1 = (((uint32_t)P + 0x6FFu) << 16) | (((uint32_t)(uint16_t)RR << 2) - 1u);
        loadTileCommand(0xF4000000u, loadW1);
    };

    bool stop = false;
    while (!stop) {
        switch (step) {
            case 2:
                KK = DD >> 10;
                step = KK > 0 ? 5u : 3u;
                break;
            case 3:
                AA = (int16_t)(AA - CC);
                if (AA > 0) {
                    E2_1 += GG;
                } else {
                    E2_1 = (int32_t)((uint32_t)imagePtr + (uint32_t)C2Bits +
                                          (uint32_t)D2Bits * (uint32_t)(-(int32_t)AA));
                    AA = (int16_t)(AA + L2);
                }
                step = 4;
                break;
            case 4:
                DD += EE;
                CC = Q;
                GG = G2;
                F1_1 = 0;
                step = 2;
                break;
            case 5:
                FF -= KK;
                DD &= 0x03FF;
                if (FF < 0) {
                    CC += (((int32_t)bg->s.scaleH * FF) >> 10) + 1;
                    KK += FF;
                    if (CC - (int32_t)Q > 0) CC = Q;
                }
                step = 6;
                break;
            case 6:
                LL = (int16_t)(JJ + KK);
                P = (uint16_t)((((uint32_t)O + MHi) * MLo >> 16) + 1u);
                MM = (int16_t)(CC + K);
                NN = (int16_t)(AA - J_2);
                step = (NN - MM < 0) ? 7u : 77u;
                break;
            case 7:
                RR = (int16_t)(MM - AA);
                AAA = AA;
                if (RR > 0) {
                    SS = imagePtr + (uint32_t)C2Bits;
                    if ((AAA & 1) != 0) {
                        --AAA;
                        ++RR;
                        SS -= (uint32_t)D2Bits;
                    }
                    AAA = (int16_t)(AAA * (int32_t)P);
                    setTexture();
                }
                step = 8;
                break;
            case 77: {
                RR = MM;
                SS = (uint32_t)E2_1;
                setTextureImageCommand(I2, SS);
                const uint32_t loadW1 = (((uint32_t)P + 0x6FFu) << 16) | (((uint32_t)(uint16_t)RR << 2) - 1u);
                loadTileCommand(0xF4000000u, loadW1);
                AA = (int16_t)(AA - CC);
                E2_1 += GG;
                step = 11;
            } break;
            case 8:
                if (J_2 != 0) {
                    SS = imagePtr;
                    int16_t BBB = NN;
                    RR = (int16_t)(NN & 1);
                    if (RR != 0) {
                        --BBB;
                        SS -= (uint32_t)D2Bits;
                    }
                    const uint32_t CCC = (uint32_t)E2_1 +
                                         (uint32_t)((int32_t)BBB * (int32_t)(uint32_t)D2Bits);
                    ++RR;
                    const uint16_t DDD = (uint16_t)((((uint32_t)D2Bits - (uint32_t)C2Bits) * 0x2000u) >> 16);
                    const uint32_t ZZZ = (uint32_t)((int32_t)BBB * (int32_t)P);
                    P = (uint16_t)(P - DDD);
                    AAA = (int16_t)(ZZZ + DDD);
                    setTexture();
                    SS = CCC;
                    AAA = (int16_t)ZZZ;
                    P = DDD;
                    setTexture();
                }
                step = 9;
                break;
            case 9:
                AA = (int16_t)(AA - CC);
                if (NN <= 0) {
                    setTileCommand(J2, 0x27000000u);
                } else {
                    P = (uint16_t)((((uint32_t)O + MHi) * MLo >> 16) + 1u);
                    SS = (uint32_t)E2_1;
                    RR = NN;
                    AAA = 0;
                    setTexture();
                }
                step = 10;
                break;
            case 10:
                if (AA > 0) {
                    E2_1 += GG;
                } else {
                    E2_1 = (int32_t)((uint32_t)imagePtr + (uint32_t)C2Bits +
                                          (uint32_t)D2Bits * (uint32_t)(-(int32_t)AA));
                    AA = (int16_t)(AA + L2);
                }
                step = 11;
                break;
            case 11: {
                const uint32_t w0 = 0xE4000000u | ((uint32_t)(uint16_t)LL << 2) | II;
                const uint32_t w1 = ((uint32_t)(uint16_t)JJ << 2) | HH;
                const uint32_t w2 = ((uint32_t)(uint16_t)H2 << 16) | F1_1;
                const uint32_t w3 = ((uint32_t)bg->s.scaleW << 16) | bg->s.scaleH;
                drawTextureRectangleFromShadow(w0, w1, w2, w3);
                if (FF <= 0) {
                    stop = true;
                } else {
                    JJ = LL;
                    DD += EE;
                    CC = Q;
                    GG = G2;
                    F1_1 = 0;
                    step = 2;
                }
            } break;
            default:
                stop = true;
                break;
        }
    }
}

void Interpreter::SetRdramMemory(uint8_t* rdramBase, uint32_t rdramSize) {
    mRdramBase = rdramBase;
    mRdramSize = rdramSize;
}

void Interpreter::RegisterStagedTextureSource(const uint8_t* staged, uint32_t size, uint32_t rdramAddress) {
    if (staged == nullptr || size == 0) return;
    for (StagedTextureSource& source : mStagedTextureSources) {
        if (source.staged == staged) {
            source.size = size;
            source.rdramAddress = rdramAddress;
            return;
        }
    }
    StagedTextureSource source;
    source.staged = staged;
    source.size = size;
    source.rdramAddress = rdramAddress;
    mStagedTextureSources.push_back(source);
}

bool Interpreter::ResolveStagedTextureSource(const uint8_t* stagedAddress, uint32_t* rdramAddress) const {
    if (stagedAddress == nullptr || rdramAddress == nullptr) return false;
    const uintptr_t address = reinterpret_cast<uintptr_t>(stagedAddress);
    for (const StagedTextureSource& source : mStagedTextureSources) {
        const uintptr_t begin = reinterpret_cast<uintptr_t>(source.staged);
        const uintptr_t end = begin + source.size;
        if (address >= begin && address < end) {
            *rdramAddress = source.rdramAddress + static_cast<uint32_t>(address - begin);
            return true;
        }
    }
    return false;
}

void Interpreter::Gfxs2dexMemRect(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint8_t tile, int16_t s, int16_t t) {
    if (tile >= 8 || y1 <= y0 || mRdp->color_image_address == nullptr) return;
    if (!PrepareTextureView(tile)) return;

    const uint32_t tmemIndex = mRdp->texture_tile[tile].tmem_index;
    const auto& loaded = mRdp->loaded_texture[tmemIndex];
    if (loaded.addr == nullptr || loaded.orig_size_bytes == 0) return;

    uint32_t texWidth = mRdp->texture_tile[tile].line_size_bytes;
    if (texWidth == 0) texWidth = loaded.line_size_bytes;
    if (texWidth == 0) return;

    const int64_t sTexel = ((int32_t)s) >> 5;
    const int64_t tTexel = ((int32_t)t) >> 5;
    const int64_t unalignedOffset = sTexel * (int64_t)texWidth + tTexel;

    if (mRdramBase == nullptr || mRdramSize == 0) return;

    uint32_t liveRdramBase = 0;
    const bool haveLegacySource = ResolveStagedTextureSource(loaded.addr, &liveRdramBase);

    int64_t sourceOffset = unalignedOffset;
    uint32_t liveSource = 0;
    if (haveLegacySource) {
        const int64_t absolute = (int64_t)liveRdramBase + unalignedOffset;
        if (absolute < 0 || absolute >= (int64_t)mRdramSize) return;
        liveSource = (uint32_t)((absolute + 3) & ~int64_t(3));
        sourceOffset = (int64_t)liveSource - (int64_t)liveRdramBase;
    } else {
        sourceOffset = (sourceOffset + 3) & ~int64_t(3);
    }
    if (sourceOffset < 0 || (uint64_t)sourceOffset >= loaded.orig_size_bytes) return;

    const uintptr_t rdramBase = (uintptr_t)mRdramBase;
    const uintptr_t rawColor = (uintptr_t)mRdp->color_image_address;
    uint32_t colorOffset;
    if (rawColor < mRdramSize) {
        colorOffset = (uint32_t)rawColor;
    } else if (rawColor >= rdramBase && rawColor - rdramBase < mRdramSize) {
        colorOffset = (uint32_t)(rawColor - rdramBase);
    } else {
        return;
    }

    uint32_t bottom = y1;
    const float scissorBottomF = mRdp->scissor.y + mRdp->scissor.height;
    if (scissorBottomF > 0.0f) {
        const uint32_t scissorBottom = (uint32_t)(scissorBottomF + 0.5f);
        if (bottom > scissorBottom) bottom = scissorBottom;
    }
    if (bottom <= y0 || mRdp->color_image_width == 0) return;

    constexpr uint32_t copyBytes = 16;
    uint32_t copiedRows = 0;
    for (uint32_t y = y0; y < bottom; ++y) {
        const uint64_t stagedOff = (uint64_t)sourceOffset + (uint64_t)(y - y0) * texWidth;
        const uint64_t dstOff = (uint64_t)colorOffset + (uint64_t)y * mRdp->color_image_width + x0;
        if (stagedOff + copyBytes > loaded.orig_size_bytes) break;
        if (dstOff + copyBytes > mRdramSize) break;

        if (haveLegacySource) {
            const uint64_t srcOff = (uint64_t)liveSource + (uint64_t)(y - y0) * texWidth;
            if (srcOff + copyBytes > mRdramSize) break;

            const uint32_t* src = reinterpret_cast<const uint32_t*>(mRdramBase + srcOff);
            uint32_t* dst = reinterpret_cast<uint32_t*>(mRdramBase + dstOff);
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        } else {
            const uint8_t* src = loaded.addr + stagedOff;
            for (uint32_t i = 0; i < copyBytes; ++i) {
                mRdramBase[((uint32_t)dstOff + i) ^ U8_TWIDDLE] = src[i];
            }
        }
        ++copiedRows;
    }

    if (copiedRows != 0) {
        const uint64_t dirtyStart = (uint64_t)colorOffset + (uint64_t)y0 * mRdp->color_image_width + x0;
        const uint64_t dirtyEnd = (uint64_t)colorOffset + (uint64_t)(y0 + copiedRows - 1) * mRdp->color_image_width + x0 + copyBytes;
        if (dirtyStart < mRdramSize && dirtyEnd > dirtyStart) {
            const uint32_t dirtySize = (uint32_t)std::min<uint64_t>(dirtyEnd - dirtyStart, mRdramSize - dirtyStart);
            RDRAM_MarkDirtyRange((uint32_t)dirtyStart, dirtySize);
        }
    }
}

void Interpreter::Gfxs2dexResetObjMatrix() {
    mS2dexObjMtx.A = 1 << 16;
    mS2dexObjMtx.B = 0;
    mS2dexObjMtx.C = 0;
    mS2dexObjMtx.D = 1 << 16;
    mS2dexObjMtx.X = 0;
    mS2dexObjMtx.Y = 0;
    mS2dexObjMtx.BaseScaleX = 1 << 10;
    mS2dexObjMtx.BaseScaleY = 1 << 10;
}

void Interpreter::Gfxs2dexObjMoveMem(uint16_t index, const void* data) {
    if (!data) return;

    switch (index) {
        case 0: {
            const F3DuObjMtx* mtx = (const F3DuObjMtx*)data;
            mS2dexObjMtx = mtx->m;
            break;
        }
        case 2: {
            const F3DuObjSubMtx* mtx = (const F3DuObjSubMtx*)data;
            mS2dexObjMtx.X = mtx->m.X;
            mS2dexObjMtx.Y = mtx->m.Y;
            mS2dexObjMtx.BaseScaleX = mtx->m.BaseScaleX;
            mS2dexObjMtx.BaseScaleY = mtx->m.BaseScaleY;
            break;
        }
        case 8:
            CalcAndSetViewport((const F3DVp_t*)data);
            break;
    }
}

void Interpreter::Gfxs2dexObjRenderMode(uint32_t mode) {
    mS2dexObjRenderMode = mode;
}

void Interpreter::Gfxs2dexObjMoveWord(uint8_t index, uint16_t offset, uint32_t data) {
    if (index == 0x08) {
        if (offset <= 12 && (offset & 3) == 0) {
            mS2dexStatus[offset >> 2] = data;
        }
        return;
    }
    GfxSpMovewordF3d(index, offset, data);
}

void Interpreter::Gfxs2dexObjLoadTxtr(const S2DEXObjTxtrData* txtr) {
    if (!txtr || !txtr->image || txtr->sid > 12 || (txtr->sid & 3) != 0) return;

    const uint32_t statusIndex = txtr->sid >> 2;
    uint32_t& status = mS2dexStatus[statusIndex];
    if (txtr->imageBytes == 0) return;

    std::vector<uint8_t>* snapshot = nullptr;
    if (txtr->type == G_OBJLT_TLUT) {
        snapshot = &mS2dexPaletteSnapshot;
        if (!snapshot->empty()) TextureCacheClear();
    } else {
        snapshot = &mS2dexTextureSnapshots[txtr->p0 != 0];
        if (!snapshot->empty()) TextureCacheDelete(snapshot->data());
    }
    snapshot->assign(txtr->image, txtr->image + txtr->imageBytes);
    const uint8_t* image = snapshot->data();

    RawTexMetadata rawTexMetadata = {};
    switch (txtr->type) {
        case G_OBJLT_TXTRBLOCK:
            GfxDpSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, txtr->p1 + 1, nullptr, 0, rawTexMetadata, image);
            GfxDpSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, txtr->p0, G_TX_LOADTILE, 0,
                         G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD, G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
            GfxDpLoadBlock(G_TX_LOADTILE, 0, 0, txtr->p1 << 2, txtr->p2);
            break;
        case G_OBJLT_TXTRTILE:
            GfxDpSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, txtr->p1 + 1, nullptr, 0, rawTexMetadata, image);
            GfxDpSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, (txtr->p1 + 1) >> 2, txtr->p0, G_TX_LOADTILE, 0,
                         G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD, G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
            GfxDpLoadTile(G_TX_LOADTILE, 0, 0, txtr->p1 << 2, txtr->p2);
            break;
        case G_OBJLT_TLUT:
            GfxDpSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, nullptr, 0, rawTexMetadata, image);
            GfxDpSetTile(G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, txtr->p0, G_TX_LOADTILE, 0,
                         G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD, G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
            GfxDpLoadTlut(G_TX_LOADTILE, txtr->p1);
            break;
        default:
            return;
    }

    status = (status & ~txtr->mask) | (txtr->flag & txtr->mask);
}

void Interpreter::Gfxs2dexObjSprite(const F3DuObjSprite* spr) {
    if (!spr) return;

    const uint32_t w = std::max<uint32_t>(spr->s.imageW >> 5, 1);
    const uint32_t h = std::max<uint32_t>(spr->s.imageH >> 5, 1);
    GfxDpSetTile(spr->s.imageFmt, spr->s.imageSiz, spr->s.imageStride, spr->s.imageAdrs, G_TX_RENDERTILE,
                 spr->s.imagePal, G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD, G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD);
    GfxDpSetTileSize(G_TX_RENDERTILE, 0, 0, (w - 1) << 2, (h - 1) << 2);
    GfxSpTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, 1);

    static const uint32_t correctorsA01[] = {
        0x00000000, 0x00100020, 0x00200040, 0x00300060,
        0x0000FFF4, 0x00100014, 0x00200034, 0x00300054
    };
    static const uint32_t correctorsA23[] = {
        0x0001FFFE, 0xFFFEFFFE, 0x00010000, 0x00000000
    };
    static const uint32_t correctorsB03[] = {
        0xFFFC0000, 0x00000001, 0xFFFF0003, 0xFFF00000
    };

    const int16_t* a01 = reinterpret_cast<const int16_t*>(correctorsA01);
    const int16_t* a23 = reinterpret_cast<const int16_t*>(correctorsA23);
    const int16_t* b03 = reinterpret_cast<const int16_t*>(correctorsB03);
    const uint32_t o1 = (mS2dexObjRenderMode & (0x10u | 0x20u | 0x40u)) >> 3;
    const uint32_t o2 = (mS2dexObjRenderMode & (0x10u | 0x08u)) >> 2;
    const uint32_t o3 = (mS2dexObjRenderMode & 0x08u) >> 1;
    const int16_t A1 = a01[(1 + o1) ^ 1];
    const int16_t A3 = a23[(1 + o2) ^ 1];
    const int16_t B0 = b03[(0 + o3) ^ 1];
    const int16_t B3 = b03[(3 + o3) ^ 1];

    const int16_t x0 = (mS2dexObjMtx.X + B3) & B0;
    const int16_t y0 = (mS2dexObjMtx.Y + B3) & B0;
    const int16_t ulx = spr->s.objX + A3;
    const int16_t uly = spr->s.objY + A3;
    const uint32_t scaleW = std::max<uint32_t>(spr->s.scaleW, 1);
    const uint32_t scaleH = std::max<uint32_t>(spr->s.scaleH, 1);
    const int16_t lrx = (int16_t)(((((uint64_t)spr->s.imageW - A1) << 8) *
                                   (0x80007FFFu / scaleW)) >> 32) + ulx;
    const int16_t lry = (int16_t)(((((uint64_t)spr->s.imageH - A1) << 8) *
                                   (0x80007FFFu / scaleH)) >> 32) + uly;

    auto calcX = [&](int16_t x, int16_t y) -> float {
        const int16_t v = x0 + (int16_t)(((int64_t)x * mS2dexObjMtx.A) >> 16) +
                              (int16_t)(((int64_t)y * mS2dexObjMtx.B) >> 16);
        return v / 4.0f;
    };
    auto calcY = [&](int16_t x, int16_t y) -> float {
        const int16_t v = y0 + (int16_t)(((int64_t)x * mS2dexObjMtx.C) >> 16) +
                              (int16_t)(((int64_t)y * mS2dexObjMtx.D) >> 16);
        return v / 4.0f;
    };

    float uls = 0.0f;
    float lrs = spr->s.imageW / 32.0f - 1.0f;
    float ult = 0.0f;
    float lrt = spr->s.imageH / 32.0f - 1.0f;
    if (spr->s.imageFlags & G_OBJ_FLAG_FLIPS) std::swap(uls, lrs);
    if (spr->s.imageFlags & G_OBJ_FLAG_FLIPT) std::swap(ult, lrt);

    struct ObjVtx { float x, y, s, t; };
    ObjVtx ov[4] = {
        { calcX(ulx, uly), calcY(ulx, uly), uls, ult },
        { calcX(lrx, uly), calcY(lrx, uly), lrs, ult },
        { calcX(ulx, lry), calcY(ulx, lry), uls, lrt },
        { calcX(lrx, lry), calcY(lrx, lry), lrs, lrt },
    };

    LoadedVertex* out[4] = {
        &mRsp->loaded_vertices[MAX_VERTICES + 0],
        &mRsp->loaded_vertices[MAX_VERTICES + 1],
        &mRsp->loaded_vertices[MAX_VERTICES + 2],
        &mRsp->loaded_vertices[MAX_VERTICES + 3],
    };
    for (uint32_t i = 0; i < 4; ++i) {
        float x = ov[i].x / HALF_SCREEN_WIDTH(mActiveFrameBuffer) - 1.0f;
        float y = -ov[i].y / HALF_SCREEN_HEIGHT(mActiveFrameBuffer) + 1.0f;
        out[i]->x = AdjXForAspectRatio(x);
        out[i]->y = y;
        out[i]->z = -1.0f;
        out[i]->w = 1.0f;
        out[i]->u = ov[i].s * 32.0f;
        out[i]->v = ov[i].t * 32.0f;
        out[i]->color = { 255, 255, 255, 255 };
        out[i]->clip_rej = 0;
    }

    XYWidthHeight defaultViewport;
    if (!mFbActive) {
        defaultViewport = { 0, (int16_t)mNativeDimensions.height, mNativeDimensions.width, mNativeDimensions.height };
    } else {
        defaultViewport = { 0, (int16_t)mActiveFrameBuffer->second.orig_height, mActiveFrameBuffer->second.orig_width,
                            mActiveFrameBuffer->second.orig_height };
    }
    XYWidthHeight savedViewport = mRdp->viewport;
    uint32_t savedGeometryMode = mRsp->geometry_mode;
    AdjustVIewportOrScissor(&defaultViewport);
    mRdp->viewport = defaultViewport;
    mRdp->viewport_or_scissor_changed = true;
    mRsp->geometry_mode = 0;

    GfxSpTri1(MAX_VERTICES + 0, MAX_VERTICES + 2, MAX_VERTICES + 1, true);
    GfxSpTri1(MAX_VERTICES + 2, MAX_VERTICES + 3, MAX_VERTICES + 1, true);

    mRsp->geometry_mode = savedGeometryMode;
    mRdp->viewport = savedViewport;
    mRdp->viewport_or_scissor_changed = true;
}

void Interpreter::Gfxs2dexObjRectangle(const F3DuObjSprite* spr) {
    if (!spr) return;

    const uint32_t w = std::max<uint32_t>(spr->s.imageW >> 5, 1);
    const uint32_t h = std::max<uint32_t>(spr->s.imageH >> 5, 1);
    GfxDpSetTile(spr->s.imageFmt, spr->s.imageSiz, spr->s.imageStride, spr->s.imageAdrs, G_TX_RENDERTILE,
                 spr->s.imagePal, G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD, G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD);
    GfxDpSetTileSize(G_TX_RENDERTILE, 0, 0, (w - 1) << 2, (h - 1) << 2);
    GfxSpTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, 1);

    static const uint32_t correctorsA01[] = {
        0x00000000, 0x00100020, 0x00200040, 0x00300060,
        0x0000FFF4, 0x00100014, 0x00200034, 0x00300054
    };
    static const uint32_t correctorsA23[] = {
        0x0001FFFE, 0xFFFEFFFE, 0x00010000, 0x00000000
    };
    static const uint32_t correctorsB03[] = {
        0xFFFC0000, 0x00000001, 0xFFFF0003, 0xFFF00000
    };

    const int16_t* a01 = reinterpret_cast<const int16_t*>(correctorsA01);
    const int16_t* a23 = reinterpret_cast<const int16_t*>(correctorsA23);
    const int16_t* b03 = reinterpret_cast<const int16_t*>(correctorsB03);
    const uint32_t o1 = (mS2dexObjRenderMode & (0x10u | 0x20u | 0x40u)) >> 3;
    const uint32_t o2 = (mS2dexObjRenderMode & (0x10u | 0x08u)) >> 2;
    const uint32_t o3 = (mS2dexObjRenderMode & 0x08u) >> 1;
    const int16_t A0 = a01[(0 + o1) ^ 1];
    const int16_t A1 = a01[(1 + o1) ^ 1];
    const int16_t A2 = a23[(0 + o2) ^ 1];
    const int16_t B0 = b03[(0 + o3) ^ 1];
    const int16_t B2 = b03[(2 + o3) ^ 1];

    const uint32_t scaleW = std::max<uint32_t>(spr->s.scaleW, 1);
    const uint32_t scaleH = std::max<uint32_t>(spr->s.scaleH, 1);

    const int16_t xh = (int16_t)((spr->s.objX + A2) & B0);
    const int16_t xl = (int16_t)((((((uint64_t)spr->s.imageW - A1) << 24) *
                                   (0x80007FFFu / scaleW)) >> 48) + xh);
    const int16_t yh = (int16_t)((spr->s.objY + A2) & B0);
    const int16_t yl = (int16_t)((((((uint64_t)spr->s.imageH - A1) << 24) *
                                   (0x80007FFFu / scaleH)) >> 48) + yh);

    const int16_t sh = (int16_t)(A0 + B2);
    const int16_t sl = (int16_t)(sh + spr->s.imageW + A0 - A1 - 1);
    const int16_t th = (int16_t)(sh - ((((int32_t)yh & 3) * 0x0200 * scaleH) >> 16));
    const int16_t tl = (int16_t)(th + spr->s.imageH + A0 - A1 - 1);

    float ulx = xh / 4.0f;
    float lrx = xl / 4.0f;
    float uly = yh / 4.0f;
    float lry = yl / 4.0f;
    float uls = sh / 32.0f;
    float lrs = sl / 32.0f;
    float ult = th / 32.0f;
    float lrt = tl / 32.0f;

    if (spr->s.imageFlags & G_OBJ_FLAG_FLIPS) std::swap(uls, lrs);
    if (spr->s.imageFlags & G_OBJ_FLAG_FLIPT) std::swap(ult, lrt);

    struct ObjVtx { float x, y, s, t; };
    ObjVtx ov[4] = {
        { ulx, uly, uls, ult },
        { lrx, uly, lrs, ult },
        { ulx, lry, uls, lrt },
        { lrx, lry, lrs, lrt },
    };

    LoadedVertex* out[4] = {
        &mRsp->loaded_vertices[MAX_VERTICES + 0],
        &mRsp->loaded_vertices[MAX_VERTICES + 1],
        &mRsp->loaded_vertices[MAX_VERTICES + 2],
        &mRsp->loaded_vertices[MAX_VERTICES + 3],
    };
    for (uint32_t i = 0; i < 4; ++i) {
        float x = ov[i].x / HALF_SCREEN_WIDTH(mActiveFrameBuffer) - 1.0f;
        float y = -ov[i].y / HALF_SCREEN_HEIGHT(mActiveFrameBuffer) + 1.0f;
        out[i]->x = AdjXForAspectRatio(x);
        out[i]->y = y;
        out[i]->z = -1.0f;
        out[i]->w = 1.0f;
        out[i]->u = ov[i].s * 32.0f;
        out[i]->v = ov[i].t * 32.0f;
        out[i]->color = { 255, 255, 255, 255 };
        out[i]->clip_rej = 0;
    }

    XYWidthHeight defaultViewport;
    if (!mFbActive) {
        defaultViewport = { 0, (int16_t)mNativeDimensions.height, mNativeDimensions.width, mNativeDimensions.height };
    } else {
        defaultViewport = { 0, (int16_t)mActiveFrameBuffer->second.orig_height, mActiveFrameBuffer->second.orig_width,
                            mActiveFrameBuffer->second.orig_height };
    }
    XYWidthHeight savedViewport = mRdp->viewport;
    uint32_t savedGeometryMode = mRsp->geometry_mode;
    AdjustVIewportOrScissor(&defaultViewport);
    mRdp->viewport = defaultViewport;
    mRdp->viewport_or_scissor_changed = true;
    mRsp->geometry_mode = 0;

    GfxSpTri1(MAX_VERTICES + 0, MAX_VERTICES + 2, MAX_VERTICES + 1, true);
    GfxSpTri1(MAX_VERTICES + 2, MAX_VERTICES + 3, MAX_VERTICES + 1, true);

    mRsp->geometry_mode = savedGeometryMode;
    mRdp->viewport = savedViewport;
    mRdp->viewport_or_scissor_changed = true;
}

void Interpreter::Gfxs2dexObjRectangleR(const F3DuObjSprite* spr) {
    if (!spr) return;

    const uint32_t w = std::max<uint32_t>(spr->s.imageW >> 5, 1);
    const uint32_t h = std::max<uint32_t>(spr->s.imageH >> 5, 1);
    GfxDpSetTile(spr->s.imageFmt, spr->s.imageSiz, spr->s.imageStride, spr->s.imageAdrs, G_TX_RENDERTILE,
                 spr->s.imagePal, G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD, G_TX_CLAMP, G_TX_NOMASK, G_TX_NOLOD);
    GfxDpSetTileSize(G_TX_RENDERTILE, 0, 0, (w - 1) << 2, (h - 1) << 2);
    GfxSpTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, 1);

    static const uint32_t correctorsA01[] = {
        0x00000000, 0x00100020, 0x00200040, 0x00300060,
        0x0000FFF4, 0x00100014, 0x00200034, 0x00300054
    };
    static const uint32_t correctorsA23[] = {
        0x0001FFFE, 0xFFFEFFFE, 0x00010000, 0x00000000
    };
    static const uint32_t correctorsB03[] = {
        0xFFFC0000, 0x00000001, 0xFFFF0003, 0xFFF00000
    };

    const int16_t* a01 = reinterpret_cast<const int16_t*>(correctorsA01);
    const int16_t* a23 = reinterpret_cast<const int16_t*>(correctorsA23);
    const int16_t* b03 = reinterpret_cast<const int16_t*>(correctorsB03);
    const uint32_t o1 = (mS2dexObjRenderMode & (0x10u | 0x20u | 0x40u)) >> 3;
    const uint32_t o2 = (mS2dexObjRenderMode & (0x10u | 0x08u)) >> 2;
    const uint32_t o3 = (mS2dexObjRenderMode & 0x08u) >> 1;
    const int16_t A0 = a01[(0 + o1) ^ 1];
    const int16_t A1 = a01[(1 + o1) ^ 1];
    const int16_t A2 = a23[(0 + o2) ^ 1];
    const int16_t B0 = b03[(0 + o3) ^ 1];
    const int16_t B2 = b03[(2 + o3) ^ 1];

    const uint32_t objScaleW = std::max<uint32_t>(spr->s.scaleW, 1);
    const uint32_t objScaleH = std::max<uint32_t>(spr->s.scaleH, 1);
    const uint32_t baseScaleX = std::max<uint32_t>(mS2dexObjMtx.BaseScaleX, 1);
    const uint32_t baseScaleY = std::max<uint32_t>(mS2dexObjMtx.BaseScaleY, 1);
    const uint32_t scaleW = std::max<uint32_t>((baseScaleX * 0x40u * objScaleW) >> 16, 1);
    const uint32_t scaleH = std::max<uint32_t>((baseScaleY * 0x40u * objScaleH) >> 16, 1);

    const int64_t objXFixed = (int64_t)spr->s.objX * 65536ll;
    const int16_t xBaseHi = (int16_t)((mS2dexObjMtx.X + A2) & B0);
    const int32_t xBase = (int32_t)xBaseHi * 65536;
    const int32_t xhp = (int32_t)(((objXFixed * 0x0800ll *
                                   (0x80007FFFu / baseScaleX)) >> 32) + xBase);
    const int16_t xh = (int16_t)(xhp >> 16);
    const int32_t xlp = xhp + (int32_t)((((uint64_t)spr->s.imageW - A1) << 24) *
                                        (0x80007FFFu / scaleW) >> 32);
    const int16_t xl = (int16_t)(xlp >> 16);

    const int64_t objYFixed = (int64_t)spr->s.objY * 65536ll;
    const int16_t yBaseHi = (int16_t)((mS2dexObjMtx.Y + A2) & B0);
    const int32_t yBase = (int32_t)yBaseHi * 65536;
    const int32_t yhp = (int32_t)(((objYFixed * 0x0800ll *
                                   (0x80007FFFu / baseScaleY)) >> 32) + yBase);
    const int16_t yh = (int16_t)(yhp >> 16);
    const int32_t ylp = yhp + (int32_t)((((uint64_t)spr->s.imageH - A1) << 24) *
                                        (0x80007FFFu / scaleH) >> 32);
    const int16_t yl = (int16_t)(ylp >> 16);

    const int16_t sh = (int16_t)(A0 + B2);
    const int16_t sl = (int16_t)(sh + spr->s.imageW + A0 - A1 - 1);
    const int16_t th = (int16_t)(sh - ((((int32_t)yh & 3) * 0x0200 * scaleH) >> 16));
    const int16_t tl = (int16_t)(th + spr->s.imageH + A0 - A1 - 1);

    float ulx = xh / 4.0f;
    float lrx = xl / 4.0f;
    float uly = yh / 4.0f;
    float lry = yl / 4.0f;
    float uls = sh / 32.0f;
    float lrs = sl / 32.0f;
    float ult = th / 32.0f;
    float lrt = tl / 32.0f;

    if (spr->s.imageFlags & G_OBJ_FLAG_FLIPS) std::swap(uls, lrs);
    if (spr->s.imageFlags & G_OBJ_FLAG_FLIPT) std::swap(ult, lrt);

    struct ObjVtx { float x, y, s, t; };
    ObjVtx ov[4] = {
        { ulx, uly, uls, ult },
        { lrx, uly, lrs, ult },
        { ulx, lry, uls, lrt },
        { lrx, lry, lrs, lrt },
    };

    LoadedVertex* out[4] = {
        &mRsp->loaded_vertices[MAX_VERTICES + 0],
        &mRsp->loaded_vertices[MAX_VERTICES + 1],
        &mRsp->loaded_vertices[MAX_VERTICES + 2],
        &mRsp->loaded_vertices[MAX_VERTICES + 3],
    };
    for (uint32_t i = 0; i < 4; ++i) {
        float x = ov[i].x / HALF_SCREEN_WIDTH(mActiveFrameBuffer) - 1.0f;
        float y = -ov[i].y / HALF_SCREEN_HEIGHT(mActiveFrameBuffer) + 1.0f;
        out[i]->x = AdjXForAspectRatio(x);
        out[i]->y = y;
        out[i]->z = -1.0f;
        out[i]->w = 1.0f;
        out[i]->u = ov[i].s * 32.0f;
        out[i]->v = ov[i].t * 32.0f;
        out[i]->color = { 255, 255, 255, 255 };
        out[i]->clip_rej = 0;
    }

    XYWidthHeight defaultViewport;
    if (!mFbActive) {
        defaultViewport = { 0, (int16_t)mNativeDimensions.height, mNativeDimensions.width, mNativeDimensions.height };
    } else {
        defaultViewport = { 0, (int16_t)mActiveFrameBuffer->second.orig_height, mActiveFrameBuffer->second.orig_width,
                            mActiveFrameBuffer->second.orig_height };
    }
    XYWidthHeight savedViewport = mRdp->viewport;
    uint32_t savedGeometryMode = mRsp->geometry_mode;
    AdjustVIewportOrScissor(&defaultViewport);
    mRdp->viewport = defaultViewport;
    mRdp->viewport_or_scissor_changed = true;
    mRsp->geometry_mode = 0;

    GfxSpTri1(MAX_VERTICES + 0, MAX_VERTICES + 2, MAX_VERTICES + 1, true);
    GfxSpTri1(MAX_VERTICES + 2, MAX_VERTICES + 3, MAX_VERTICES + 1, true);

    mRsp->geometry_mode = savedGeometryMode;
    mRdp->viewport = savedViewport;
    mRdp->viewport_or_scissor_changed = true;
}

void Interpreter::Gfxs2dexObjLoadTxSprite(const S2DEXObjTxSpriteData* txsp) {
    if (!txsp) return;
    Gfxs2dexObjLoadTxtr(&txsp->txtr);
    Gfxs2dexObjSprite(&txsp->sprite);
}

void Interpreter::Gfxs2dexObjLoadTxRectR(const S2DEXObjTxSpriteData* txsp) {
    if (!txsp) return;
    Gfxs2dexObjLoadTxtr(&txsp->txtr);
    Gfxs2dexObjRectangleR(&txsp->sprite);
}

void Interpreter::Gfxs2dexRecyCopy(F3DuObjSprite* spr) {
    s16 dsdx = 4 << 10;
    s16 uls = spr->s.objX << 3;
    // Flip flag only flips horizontally
    if (spr->s.imageFlags == G_BG_FLAG_FLIPS) {
        dsdx = -dsdx;
        uls = (spr->s.imageW - spr->s.objX) << 3;
    }

    int realX = spr->s.objX >> 2;
    int realY = spr->s.objY >> 2;
    int realW = (((spr->s.imageW)) >> 5);
    int realH = (((spr->s.imageH)) >> 5);
    float realSW = spr->s.scaleW / 1024.0f;
    float realSH = spr->s.scaleH / 1024.0f;

    int testX = (realX + (realW / realSW));
    int testY = (realY + (realH / realSH));

    GfxDpTextureRectangle(realX << 2, realY << 2, testX << 2, testY << 2, G_TX_RENDERTILE,
                          (s32)mRdp->texture_tile[0].uls << 3, (s32)mRdp->texture_tile[0].ult << 3,
                          (float)(1 << 10) * realSW, (float)(1 << 10) * realSH, false);
}

void* Interpreter::SegAddr(uintptr_t w1) {
    // Segmented?
    if (w1 & 1) {
        uint32_t segNum = (uint32_t)(w1 >> 24);

        uint32_t offset = w1 & 0x00FFFFFE;

        if (mSegmentPointers[segNum] != 0) {
            return (void*)(mSegmentPointers[segNum] + offset);
        } else {
            return (void*)w1;
        }
    } else {
        return (void*)w1;
    }
}

#define C0(pos, width) ((cmd->words.w0 >> (pos)) & ((1U << width) - 1))
#define C1(pos, width) ((cmd->words.w1 >> (pos)) & ((1U << width) - 1))

void GfxExecStack::start(F3DGfx* dlist) {
    while (!cmd_stack.empty())
        cmd_stack.pop();
    cmd_stack.push(dlist);
    disp_stack.clear();
}

void GfxExecStack::stop() {
    while (!cmd_stack.empty())
        cmd_stack.pop();
}

F3DGfx*& GfxExecStack::currCmd() {
    return cmd_stack.top();
}

void GfxExecStack::openDisp(const char* file, int line) {
    disp_stack.push_back({ file, line });
}
void GfxExecStack::closeDisp() {
    disp_stack.pop_back();
}
const std::vector<GfxExecStack::CodeDisp>& GfxExecStack::getDisp() const {
    return disp_stack;
}

void GfxExecStack::branch(F3DGfx* caller) {
    F3DGfx* old = cmd_stack.top();
    cmd_stack.pop();
    cmd_stack.push(nullptr);
    cmd_stack.push(old);

}

void GfxExecStack::call(F3DGfx* caller, F3DGfx* callee) {
    cmd_stack.push(callee);
}

F3DGfx* GfxExecStack::ret() {
    F3DGfx* cmd = cmd_stack.top();

    cmd_stack.pop();
    while (cmd_stack.size() > 0 && cmd_stack.top() == nullptr) {
        cmd_stack.pop();
    }
    return cmd;
}

void gfx_set_framebuffer(int fb, float noise_scale);
void gfx_reset_framebuffer();
void gfx_copy_framebuffer(int fb_dst_id, int fb_src_id, bool copyOnce, bool* hasCopiedPtr);

// The main type of the handler function. These function will take a pointer to a pointer to a Gfx. It needs to be a
// double pointer because we sometimes need to increment and decrement the underlying pointer Returns false if the
// current opcode should be incremented after the handler ends.
typedef bool (*GfxOpcodeHandlerFunc)(F3DGfx** gfx);

bool gfx_load_ucode_handler_f3dex2(F3DGfx** cmd) {
    Interpreter* gfx = gInstance;
    gfx->mRsp->fog_mul = 0;
    gfx->mRsp->fog_offset = 0;
    return false;
}

bool gfx_cull_dl_handler_f3dex2(F3DGfx** cmd) {
    // TODO:
    return false;
}

bool gfx_invalidate_tex_cache_handler_f3dex2(F3DGfx** cmd) {
    Interpreter* gfx = gInstance;
    const uintptr_t texAddr = (*cmd)->words.w1;

    if (texAddr == 0) {
        gfx->TextureCacheClear();
    } else {
        gfx->TextureCacheDelete((const uint8_t*)texAddr);
    }
    return false;
}

bool gfx_noop_handler_f3dex2(F3DGfx** cmd0) {
    F3DGfx* cmd = *cmd0;
    const char* filename = (const char*)(cmd)->words.w1;
    uint32_t p = C0(16, 8);
    uint32_t l = C0(0, 16);
    if (p == 7) {
        g_exec_stack.openDisp(filename, l);
    } else if (p == 8) {
        if (g_exec_stack.disp_stack.size() == 0) {
        } else {
            g_exec_stack.closeDisp();
        }
    }
    return false;
}

bool gfx_mtx_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;
    uintptr_t mtxAddr = cmd->words.w1;

    gfx->GfxSpMatrix(C0(0, 8) ^ F3DEX2_G_MTX_PUSH, (const int32_t*)gfx->SegAddr(mtxAddr));
    return false;
}
// Seems to be the same for all other non F3DEX2 microcodes...
bool gfx_mtx_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;
    uintptr_t mtxAddr = cmd->words.w1;

    gfx->GfxSpMatrix(C0(16, 8), (const int32_t*)gfx->SegAddr(cmd->words.w1));
    return false;
}

bool gfx_pop_mtx_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpPopMatrix((uint32_t)(cmd->words.w1 / 64));

    return false;
}

bool gfx_pop_mtx_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpPopMatrix(1);

    return false;
}

bool gfx_movemem_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpMovememF3dex2(C0(0, 8), C0(8, 8) * 8, gfx->SegAddr(cmd->words.w1));

    return false;
}

bool gfx_movemem_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpMovememF3d(C0(16, 8), 0, gfx->SegAddr(cmd->words.w1));

    return false;
}

bool gfx_push_shader(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;
    const char* path = (const char*)gfx->SegAddr(cmd->words.w1);

    return false;
}

bool gfx_pop_shader(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->mShaderStack.pop();

    return false;
}

const char* gfx_get_shader(int16_t id) {
    Interpreter* gfx = gInstance;

    for (const std::pair<size_t, const char*>& shader : gfx->mShaders) {
        if (shader.first == id) {
            return shader.second;
        }
    }

    return nullptr; // Use no shader
}

bool gfx_moveword_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpMovewordF3dex2(C0(16, 8), C0(0, 16), cmd->words.w1);

    return false;
}

bool gfx_moveword_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpMovewordF3d(C0(0, 8), C0(8, 16), cmd->words.w1);

    return false;
}

bool gfx_texture_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTexture(C1(16, 16), C1(0, 16), C0(11, 3), C0(8, 3), C0(1, 7));

    return false;
}

// Seems to be the same for all other non F3DEX2 microcodes...
bool gfx_texture_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTexture(C1(16, 16), C1(0, 16), C0(11, 3), C0(8, 3), C0(0, 8));

    return false;
}

// Almost all versions of the microcode have their own version of this opcode
bool gfx_vtx_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpVertex(C0(12, 8), C0(1, 7) - C0(12, 8), (const F3DVtx*)gfx->SegAddr(cmd->words.w1));

    return false;
}

bool gfx_vtx_handler_f3dex(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;
    gfx->GfxSpVertex(C0(10, 6), C0(17, 7), (const F3DVtx*)gfx->SegAddr(cmd->words.w1));

    return false;
}

bool gfx_vtx_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpVertex((C0(0, 16)) / sizeof(F3DVtx), C0(16, 4), (const F3DVtx*)gfx->SegAddr(cmd->words.w1));

    return false;
}

// The original F3D microcode doesn't seem to have this opcode. Glide handles it as part of moveword
bool gfx_modify_vtx_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;
    gfx->GfxSpModifyVertex(C0(1, 15), C0(16, 8), (uint32_t)cmd->words.w1);
    return false;
}

// F3D, F3DEX, and F3DEX2 do the same thing but F3DEX2 has its own opcode number
bool gfx_dl_handler_common(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;
    F3DGfx* subGFX = (F3DGfx*)gfx->SegAddr(cmd->words.w1);
    if (C0(16, 1) == 0) {
        // Push return address
        if (subGFX != nullptr) {
            g_exec_stack.call(*cmd0, subGFX);
        }
    } else {
        (*cmd0) = subGFX;
        g_exec_stack.branch(cmd);
        return true; // shortcut cmd increment
    }
    return false;
}

bool gfx_dl_index_handler(F3DGfx** cmd0) {
    // Compute seg addr by converting an index value to a offset value
    // handling 32 vs 64 bit size differences for Gfx
    // adding 1 to trigger the segaddr flow
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = (*cmd0);
    uint8_t segNum = (uint8_t)(cmd->words.w1 >> 24);
    uint32_t index = (uint32_t)(cmd->words.w1 & 0x00FFFFFF);
    uintptr_t segAddr = (segNum << 24) | (index * sizeof(F3DGfx)) + 1;

    F3DGfx* subGFX = (F3DGfx*)gfx->SegAddr(segAddr);
    if (C0(16, 1) == 0) {
        // Push return address
        if (subGFX != nullptr) {
            g_exec_stack.call((*cmd0), subGFX);
        }
    } else {
        (*cmd0) = subGFX;
        g_exec_stack.branch(cmd);
        return true; // shortcut cmd increment
    }
    return false;
}

// F3D, F3DEX, and F3DEX2 do the same thing but F3DEX2 has its own opcode number
bool gfx_end_dl_handler_common(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    gfx->mMarkerOn = false;
    g_exec_stack.ret();
    return true;
}

bool gfx_set_prim_depth_handler_rdp(F3DGfx** cmd) {
    Interpreter* gfx = gInstance;
    uint32_t w1 = (*cmd)->words.w1;
    gfx->mRdp->prim_depth = (uint16_t)((w1 >> 16) & 0x7FFF); // Mask to 15 bits
    return false;
}

// Only on F3DEX2
bool gfx_geometry_mode_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpGeometryMode(~C0(0, 24), (uint32_t)cmd->words.w1);
    return false;
}

// Only on F3DEX and older
bool gfx_set_geometry_mode_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpGeometryMode(0, (uint32_t)cmd->words.w1);
    return false;
}

// Only on F3DEX and older
bool gfx_clear_geometry_mode_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpGeometryMode((uint32_t)cmd->words.w1, 0);
    return false;
}

bool gfx_tri1_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTri1(C0(16, 8) / 2, C0(8, 8) / 2, C0(0, 8) / 2, false);

    return false;
}

bool gfx_tri1_handler_f3dex(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTri1(C1(17, 7), C1(9, 7), C1(1, 7), false);

    return false;
}

bool gfx_tri1_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTri1(C1(16, 8) / 10, C1(8, 8) / 10, C1(0, 8) / 10, false);

    return false;
}

// F3DEX, and F3DEX2 share a tri2 function, however F3DEX has a different quad function.
bool gfx_tri2_handler_f3dex(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTri1(C0(17, 7), C0(9, 7), C0(1, 7), false);
    gfx->GfxSpTri1(C1(17, 7), C1(9, 7), C1(1, 7), false);
    return false;
}

bool gfx_quad_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTri1(C0(16, 8) / 2, C0(8, 8) / 2, C0(0, 8) / 2, false);
    gfx->GfxSpTri1(C1(16, 8) / 2, C1(8, 8) / 2, C1(0, 8) / 2, false);
    return false;
}

bool gfx_quad_handler_f3dex(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTri1(C1(16, 8) / 2, C1(8, 8) / 2, C1(0, 8) / 2, false);
    gfx->GfxSpTri1(C1(16, 8) / 2, C1(0, 8) / 2, C1(24, 8) / 2, false);
    return false;
}

bool gfx_othermode_l_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpSetOtherMode(31 - C0(8, 8) - C0(0, 8), C0(0, 8) + 1, cmd->words.w1);

    return false;
}

bool gfx_othermode_l_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpSetOtherMode(C0(8, 8), C0(0, 8), cmd->words.w1);

    return false;
}

bool gfx_othermode_h_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpSetOtherMode(63 - C0(8, 8) - C0(0, 8), C0(0, 8) + 1, (uint64_t)cmd->words.w1 << 32);

    return false;
}

// Only on F3DEX and older
bool gfx_set_geometry_mode_handler_f3dex(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpGeometryMode(0, (uint32_t)cmd->words.w1);
    return false;
}

// Only on F3DEX and older
bool gfx_clear_geometry_mode_handler_f3dex(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpGeometryMode((uint32_t)cmd->words.w1, 0);
    return false;
}

bool gfx_othermode_h_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpSetOtherMode(C0(8, 8) + 32, C0(0, 8), (uint64_t)cmd->words.w1 << 32);

    return false;
}

bool gfx_set_timg_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;
    uintptr_t i = (uintptr_t)gfx->SegAddr(cmd->words.w1);

    char* imgData = (char*)i;
    uint32_t texFlags = 0;
    RawTexMetadata rawTexMetdata = {};
    // Default scale factors to 1 for raw N64 textures.
    rawTexMetdata.h_byte_scale = 1;
    rawTexMetdata.v_pixel_scale = 1;

    // If the resolved address is still in the N64 segmented range, SegAddr
    // failed to resolve it (segment not set up). Skip to avoid dereferencing
    // invalid memory.
    if (i <= 0x0FFFFFFF) {
        return false;
    }

    gfx->GfxDpSetTextureImage(C0(21, 3), C0(19, 2), C0(0, 12) + 1, imgData, texFlags, rawTexMetdata, (void*)i);

    return false;
}

bool gfx_load_block_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpLoadBlock(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
    return false;
}

bool gfx_load_block_wide_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    uint32_t tile = cmd->words.w0 & 0x7;
    uint32_t lrs = cmd->words.w1;

    (*cmd0)++;
    cmd = *cmd0;

    uint32_t uls = (cmd->words.w0 >> 16) & 0xFFFF;
    uint32_t ult = (cmd->words.w0 >> 0) & 0xFFFF;
    uint32_t dxt = (cmd->words.w1 >> 0) & 0xFFF;

    gfx->GfxDpLoadBlock(tile, uls, ult, lrs, dxt);
    return false;
}

bool gfx_load_tile_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpLoadTile(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
    return false;
}

bool gfx_set_tile_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetTile(C0(21, 3), C0(19, 2), C0(9, 9), C0(0, 9), C1(24, 3), C1(20, 4), C1(18, 2), C1(14, 4), C1(10, 4),
                      C1(8, 2), C1(4, 4), C1(0, 4));
    return false;
}

bool gfx_set_tile_size_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetTileSize(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
    return false;
}

bool gfx_set_tile_size_interp_handler_rdp(F3DGfx** cmd0) {
    F3DGfx* cmd = *cmd0;
    Interpreter* gfx = gInstance;

    if (gfx->mInterpolationIndex == gfx->mInterpolationIndexTarget) {
        int tile = C1(24, 3);
        gfx->GfxDpSetTileSize(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
        ++(*cmd0);
        memcpy(&gfx->mRdp->texture_tile[tile].uls, &(*cmd0)->words.w0, sizeof(float));
        memcpy(&gfx->mRdp->texture_tile[tile].ult, &(*cmd0)->words.w1, sizeof(float));
        ++(*cmd0);
        memcpy(&gfx->mRdp->texture_tile[tile].lrs, &(*cmd0)->words.w0, sizeof(float));
        memcpy(&gfx->mRdp->texture_tile[tile].lrt, &(*cmd0)->words.w1, sizeof(float));
    } else {
        ++(*cmd0);
        ++(*cmd0);
    }

    return false;
}

bool gfx_set_interpolation_index_target(F3DGfx** cmd0) {
    F3DGfx* cmd = *cmd0;
    Interpreter* gfx = gInstance;

    gfx->mInterpolationIndexTarget = cmd->words.w1;
    return false;
}

bool gfx_load_tlut_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpLoadTlut(C1(24, 3), C1(14, 10));
    return false;
}

bool gfx_set_env_color_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetEnvColor(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
    return false;
}

bool gfx_set_prim_color_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetPrimColor(C0(8, 8), C0(0, 8), C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
    return false;
}

bool gfx_set_fog_color_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetFogColor(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
    return false;
}

// CENTER/SCALE and K4/K5 are wired as combiner inputs, so the standard (A-B)*C+D
// shader path covers their common uses. TODO: chroma-key width/threshold
// gating from G_SETKEYR/GB (wR/wG/wB ignored) and the YUV->RGB matrix K0..K3
// applied during texture sampling.
// G_SETKEYR: w1 = [wR:12 | cR:8 | sR:8]
bool gfx_set_key_r_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->mRdp->key_center.r = C1(8, 8);
    gfx->mRdp->key_scale.r = C1(0, 8);
    return false;
}

// G_SETKEYGB: w0 = [op:8 | wG:12 | _:4 | wB:12], w1 = [cG:8 | sG:8 | cB:8 | sB:8]
bool gfx_set_key_gb_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->mRdp->key_center.g = C1(24, 8);
    gfx->mRdp->key_scale.g = C1(16, 8);
    gfx->mRdp->key_center.b = C1(8, 8);
    gfx->mRdp->key_scale.b = C1(0, 8);
    return false;
}

// G_SETCONVERT: w0 = [op:8 | k0:9 | k1:9 | k2_hi:4], w1 = [k2_lo:5 | k3:9 | k4:9 | k5:9]
// K0..K5 are signed 9-bit values; sign-extend after decoding.
bool gfx_set_convert_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->mRdp->convert_k[0] = sign_extend_9(C0(13, 9));
    gfx->mRdp->convert_k[1] = sign_extend_9(C0(4, 9));
    // k2 is split across w0 and w1
    gfx->mRdp->convert_k[2] = sign_extend_9((C0(0, 4) << 5) | C1(27, 5));
    gfx->mRdp->convert_k[3] = sign_extend_9(C1(18, 9));
    gfx->mRdp->convert_k[4] = sign_extend_9(C1(9, 9));
    gfx->mRdp->convert_k[5] = sign_extend_9(C1(0, 9));
    return false;
}

bool gfx_set_blend_color_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetBlendColor(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
    return false;
}

bool gfx_set_fill_color_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetFillColor((uint32_t)cmd->words.w1);
    return false;
}

bool gfx_set_combine_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetCombineMode(
        color_comb(C0(20, 4), C1(28, 4), C0(15, 5), C1(15, 3)), alpha_comb(C0(12, 3), C1(12, 3), C0(9, 3), C1(9, 3)),
        color_comb(C0(5, 4), C1(24, 4), C0(0, 5), C1(6, 3)), alpha_comb(C1(21, 3), C1(3, 3), C1(18, 3), C1(0, 3)));
    return false;
}

bool gfx_tex_rect_and_flip_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;
    int8_t opcode = (int8_t)(cmd->words.w0 >> 24);
    int32_t lrx, lry, tile, ulx, uly;
    uint32_t uls, ult, dsdx, dtdy;

    lrx = C0(12, 12);
    lry = C0(0, 12);
    tile = C1(24, 3);
    ulx = C1(12, 12);
    uly = C1(0, 12);
    // TODO make sure I don't need to increment cmd0
    ++(*cmd0);
    cmd = *cmd0;
    uls = C1(16, 16);
    ult = C1(0, 16);
    ++(*cmd0);
    cmd = *cmd0;
    dsdx = C1(16, 16);
    dtdy = C1(0, 16);

    gfx->GfxDpTextureRectangle(ulx, uly, lrx, lry, tile, uls, ult, dsdx, dtdy, opcode == RDP_G_TEXRECTFLIP);
    return false;
}

bool gfx_fill_rect_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *(cmd0);

    gfx->GfxDpFillRectangle(C1(12, 12), C1(0, 12), C0(12, 12), C0(0, 12));
    return false;
}

bool gfx_SetScissor_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *(cmd0);

    gfx->GfxDpSetScissor(C1(24, 2), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
    return false;
}

bool gfx_set_z_img_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *(cmd0);

    gfx->GfxDpSetZImage(gfx->SegAddr(cmd->words.w1));
    return false;
}

bool gfx_set_c_img_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *(cmd0);

    gfx->GfxDpSetColorImage(C0(21, 3), C0(19, 2), C0(0, 12) + 1, gfx->SegAddr(cmd->words.w1));
    return false;
}

bool gfx_rdp_set_other_mode_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *(cmd0);

    gfx->GfxDpSetOtherMode(C0(0, 24), (uint32_t)cmd->words.w1);
    return false;
}

bool gfx_bg_copy_handler_s2dex(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *(cmd0);

    if (!gfx->mMarkerOn) {
        gfx->Gfxs2dexBgCopy((F3DuObjBg*)cmd->words.w1); // not gfx->SegAddr here it seems
    }
    return false;
}

bool gfx_bg_1cyc_handler_s2dex(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *(cmd0);

    gfx->Gfxs2dexBg1cyc((F3DuObjBg*)cmd->words.w1);
    return false;
}

bool gfx_obj_rectangle_handler_s2dex(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *(cmd0);

    if (!gfx->mMarkerOn) {
        gfx->Gfxs2dexObjRectangle((const F3DuObjSprite*)cmd->words.w1);
    }
    return false;
}

bool gfx_obj_movemem_handler_s2dex(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *(cmd0);

    gfx->Gfxs2dexObjMoveMem(C0(0, 16), (const void*)cmd->words.w1);
    return false;
}

bool gfx_obj_rendermode_handler_s2dex(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *(cmd0);

    gfx->Gfxs2dexObjRenderMode((uint32_t)cmd->words.w1);
    return false;
}

bool gfx_moveword_handler_s2dex1(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;
    gfx->Gfxs2dexObjMoveWord(C0(0, 8), C0(8, 16), cmd->words.w1);
    return false;
}

bool gfx_obj_loadtxtr_handler_s2dex1(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;
    gfx->Gfxs2dexObjLoadTxtr((const S2DEXObjTxtrData*)cmd->words.w1);
    return false;
}

bool gfx_obj_ldtx_sprite_handler_s2dex1(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;
    gfx->Gfxs2dexObjLoadTxSprite((const S2DEXObjTxSpriteData*)cmd->words.w1);
    return false;
}

bool gfx_obj_ldtx_rect_r_handler_s2dex1(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;
    gfx->Gfxs2dexObjLoadTxRectR((const S2DEXObjTxSpriteData*)cmd->words.w1);
    return false;
}

bool gfx_rdphalf_0_handler_s2dex1(F3DGfx** cmd0) {
    Interpreter* gfx = gInstance;
    F3DGfx* cmd = *cmd0;

    const uint32_t x1 = (cmd->words.w0 >> 14) & 0x3FF;
    const uint32_t y1 = (cmd->words.w0 >> 2) & 0x3FF;
    const uint8_t tile = (cmd->words.w1 >> 24) & 0x7;
    const uint32_t x0 = (cmd->words.w1 >> 14) & 0x3FF;
    const uint32_t y0 = (cmd->words.w1 >> 2) & 0x3FF;

    ++(*cmd0);
    cmd = *cmd0;
    const int16_t s = (int16_t)(cmd->words.w1 >> 16);
    const int16_t t = (int16_t)(cmd->words.w1 & 0xFFFF);

    ++(*cmd0);
    gfx->Gfxs2dexMemRect(x0, y0, x1, y1, tile, s, t);
    return false;
}

bool gfx_stubbed_command_handler(F3DGfx** cmd0) {
    return false;
}

bool gfx_spnoop_command_handler_f3dex2(F3DGfx** cmd0) {
    return false;
}

class UcodeHandler {
  public:
    inline UcodeHandler(
        std::initializer_list<std::pair<int8_t, std::pair<const char*, GfxOpcodeHandlerFunc>>> initializer) {
        std::fill(std::begin(mHandlers), std::end(mHandlers),
                  std::pair<const char*, GfxOpcodeHandlerFunc>(nullptr, nullptr));

        for (const auto& entry : initializer) {
            mHandlers[static_cast<uint8_t>(entry.first)] = entry.second;
        }
    }

    inline bool contains(int8_t opcode) const {
        return mHandlers[static_cast<uint8_t>(opcode)].first != nullptr;
    }

    inline std::pair<const char*, GfxOpcodeHandlerFunc> at(int8_t opcode) const {
        return mHandlers[static_cast<uint8_t>(opcode)];
    }

  private:
    std::pair<const char*, GfxOpcodeHandlerFunc> mHandlers[std::numeric_limits<uint8_t>::max() + 1];
};

static const UcodeHandler rdpHandlers = {
    { RDP_G_SETTARGETINTERPINDEX,
      { "G_SETTARGETINTERPINDEX", gfx_set_interpolation_index_target } }, // G_SETTARGETINTERPINDEX
    { RDP_G_SETTILESIZE_INTERP,
      { "G_SETTILESIZE_INTERP", gfx_set_tile_size_interp_handler_rdp } },            // G_SETTILESIZE_INTERP
    { RDP_G_TEXRECT, { "G_TEXRECT", gfx_tex_rect_and_flip_handler_rdp } },           // G_TEXRECT (-28)
    { RDP_G_TEXRECTFLIP, { "G_TEXRECTFLIP", gfx_tex_rect_and_flip_handler_rdp } },   // G_TEXRECTFLIP (-27)
    { RDP_G_RDPLOADSYNC, { "mRdpLOADSYNC", gfx_stubbed_command_handler } },          // mRdpLOADSYNC (-26)
    { RDP_G_RDPPIPESYNC, { "mRdpPIPESYNC", gfx_stubbed_command_handler } },          // mRdpPIPESYNC (-25)
    { RDP_G_RDPTILESYNC, { "mRdpTILESYNC", gfx_stubbed_command_handler } },          // mRdpPIPESYNC (-24)
    { RDP_G_RDPFULLSYNC, { "mRdpFULLSYNC", gfx_stubbed_command_handler } },          // mRdpFULLSYNC (-23)
    { RDP_G_SETKEYGB, { "G_SETKEYGB", gfx_set_key_gb_handler_rdp } },                // G_SETKEYGB (-22)
    { RDP_G_SETKEYR, { "G_SETKEYR", gfx_set_key_r_handler_rdp } },                   // G_SETKEYR (-21)
    { RDP_G_SETCONVERT, { "G_SETCONVERT", gfx_set_convert_handler_rdp } },           // G_SETCONVERT (-20)
    { RDP_G_SETSCISSOR, { "G_SETSCISSOR", gfx_SetScissor_handler_rdp } },            // G_SETSCISSOR (-19)
    { RDP_G_SETPRIMDEPTH, { "G_SETPRIMDEPTH", gfx_set_prim_depth_handler_rdp } },    // G_SETPRIMDEPTH (-18)
    { RDP_G_RDPSETOTHERMODE, { "mRdpSETOTHERMODE", gfx_rdp_set_other_mode_rdp } },   // mRdpSETOTHERMODE (-17)
    { RDP_G_LOADTLUT, { "G_LOADTLUT", gfx_load_tlut_handler_rdp } },                 // G_LOADTLUT (-16)
    { RDP_G_SETTILESIZE, { "G_SETTILESIZE", gfx_set_tile_size_handler_rdp } },       // G_SETTILESIZE (-14)
    { RDP_G_LOADBLOCK, { "G_LOADBLOCK", gfx_load_block_handler_rdp } },              // G_LOADBLOCK (-13)
    { RDP_G_LOADTILE, { "G_LOADTILE", gfx_load_tile_handler_rdp } },                 // G_LOADTILE (-12)
    { RDP_G_SETTILE, { "G_SETTILE", gfx_set_tile_handler_rdp } },                    // G_SETTILE (-11)
    { RDP_G_FILLRECT, { "G_FILLRECT", gfx_fill_rect_handler_rdp } },                 // G_FILLRECT (-10)
    { RDP_G_SETFILLCOLOR, { "G_SETFILLCOLOR", gfx_set_fill_color_handler_rdp } },    // G_SETFILLCOLOR (-9)
    { RDP_G_SETFOGCOLOR, { "G_SETFOGCOLOR", gfx_set_fog_color_handler_rdp } },       // G_SETFOGCOLOR (-8)
    { RDP_G_SETBLENDCOLOR, { "G_SETBLENDCOLOR", gfx_set_blend_color_handler_rdp } }, // G_SETBLENDCOLOR (-7)
    { RDP_G_SETPRIMCOLOR, { "G_SETPRIMCOLOR", gfx_set_prim_color_handler_rdp } },    // G_SETPRIMCOLOR (-6)
    { RDP_G_SETENVCOLOR, { "G_SETENVCOLOR", gfx_set_env_color_handler_rdp } },       // G_SETENVCOLOR (-5)
    { RDP_G_SETCOMBINE, { "G_SETCOMBINE", gfx_set_combine_handler_rdp } },           // G_SETCOMBINE (-4)
    { RDP_G_SETTIMG, { "G_SETTIMG", gfx_set_timg_handler_rdp } },                    // G_SETTIMG (-3)
    { RDP_G_SETZIMG, { "G_SETZIMG", gfx_set_z_img_handler_rdp } },                   // G_SETZIMG (-2)
    { RDP_G_SETCIMG, { "G_SETCIMG", gfx_set_c_img_handler_rdp } },                   // G_SETCIMG (-1)
};

static const UcodeHandler f3dex2Handlers = {
    { F3DEX2_G_NOOP, { "G_NOOP", gfx_noop_handler_f3dex2 } },
    { F3DEX2_G_SPNOOP, { "G_SPNOOP", gfx_noop_handler_f3dex2 } },
    { F3DEX2_G_CULLDL, { "G_CULLDL", gfx_cull_dl_handler_f3dex2 } },
    { F3DEX2_G_MTX, { "G_MTX", gfx_mtx_handler_f3dex2 } },
    { F3DEX2_G_POPMTX, { "G_POPMTX", gfx_pop_mtx_handler_f3dex2 } },
    { F3DEX2_G_MOVEMEM, { "G_MOVEMEM", gfx_movemem_handler_f3dex2 } },
    { F3DEX2_G_MOVEWORD, { "G_MOVEWORD", gfx_moveword_handler_f3dex2 } },
    { F3DEX2_G_TEXTURE, { "G_TEXTURE", gfx_texture_handler_f3dex2 } },
    { F3DEX2_G_VTX, { "G_VTX", gfx_vtx_handler_f3dex2 } },
    { F3DEX2_G_MODIFYVTX, { "G_MODIFYVTX", gfx_modify_vtx_handler_f3dex2 } },
    { F3DEX2_G_DL, { "G_DL", gfx_dl_handler_common } },
    { F3DEX2_G_ENDDL, { "G_ENDDL", gfx_end_dl_handler_common } },
    { F3DEX2_G_GEOMETRYMODE, { "G_GEOMETRYMODE", gfx_geometry_mode_handler_f3dex2 } },
    { F3DEX2_G_TRI1, { "G_TRI1", gfx_tri1_handler_f3dex2 } },
    { F3DEX2_G_TRI2, { "G_TRI2", gfx_tri2_handler_f3dex } },
    { F3DEX2_G_QUAD, { "G_QUAD", gfx_quad_handler_f3dex2 } },
    { F3DEX2_G_SETOTHERMODE_L, { "G_SETOTHERMODE_L", gfx_othermode_l_handler_f3dex2 } },
    { F3DEX2_G_SETOTHERMODE_H, { "G_SETOTHERMODE_H", gfx_othermode_h_handler_f3dex2 } },
};

static const UcodeHandler f3dexHandlers = {
    { F3DEX_G_NOOP, { "G_NOOP", gfx_noop_handler_f3dex2 } },
    { F3DEX_G_CULLDL, { "G_CULLDL", gfx_cull_dl_handler_f3dex2 } },
    { F3DEX_G_MTX, { "G_MTX", gfx_mtx_handler_f3d } },
    { F3DEX_G_POPMTX, { "G_POPMTX", gfx_pop_mtx_handler_f3d } },
    { F3DEX_G_MOVEMEM, { "G_POPMEM", gfx_movemem_handler_f3d } },
    { F3DEX_G_MOVEWORD, { "G_MOVEWORD", gfx_moveword_handler_f3d } },
    { F3DEX_G_TEXTURE, { "G_TEXTURE", gfx_texture_handler_f3d } },
    { F3DEX_G_SETOTHERMODE_L, { "G_SETOTHERMODE_L", gfx_othermode_l_handler_f3d } },
    { F3DEX_G_SETOTHERMODE_H, { "G_SETOTHERMODE_H", gfx_othermode_h_handler_f3d } },
    { F3DEX_G_SETGEOMETRYMODE, { "G_SETGEOMETRYMODE", gfx_set_geometry_mode_handler_f3dex } },
    { F3DEX_G_CLEARGEOMETRYMODE, { "G_CLEARGEOMETRYMODE", gfx_clear_geometry_mode_handler_f3dex } },
    { F3DEX_G_VTX, { "G_VTX", gfx_vtx_handler_f3dex } },
    { F3DEX_G_TRI1, { "G_TRI1", gfx_tri1_handler_f3dex } },
    { F3DEX_G_MODIFYVTX, { "G_MODIFYVTX", gfx_modify_vtx_handler_f3dex2 } },
    { F3DEX_G_DL, { "G_DL", gfx_dl_handler_common } },
    { F3DEX_G_ENDDL, { "G_ENDDL", gfx_end_dl_handler_common } },
    { F3DEX_G_TRI2, { "G_TRI2", gfx_tri2_handler_f3dex } },
    { F3DEX_G_SPNOOP, { "G_SPNOOP", gfx_spnoop_command_handler_f3dex2 } },
    { F3DEX_G_RDPHALF_1, { "mRdpHALF_1", gfx_stubbed_command_handler } },
    { F3DEX_G_QUAD, { "G_QUAD", gfx_quad_handler_f3dex } },
};

static const UcodeHandler f3dHandlers = {
    { F3DEX_G_NOOP, { "G_NOOP", gfx_noop_handler_f3dex2 } },
    { F3DEX_G_CULLDL, { "G_CULLDL", gfx_cull_dl_handler_f3dex2 } },
    { F3DEX_G_MTX, { "G_MTX", gfx_mtx_handler_f3d } },
    { F3DEX_G_POPMTX, { "G_POPMTX", gfx_pop_mtx_handler_f3d } },
    { F3DEX_G_MOVEMEM, { "G_POPMEM", gfx_movemem_handler_f3d } },
    { F3DEX_G_MOVEWORD, { "G_MOVEWORD", gfx_moveword_handler_f3d } },
    { F3DEX_G_TEXTURE, { "G_TEXTURE", gfx_texture_handler_f3d } },
    { F3DEX_G_SETOTHERMODE_L, { "G_SETOTHERMODE_L", gfx_othermode_l_handler_f3d } },
    { F3DEX_G_SETOTHERMODE_H, { "G_SETOTHERMODE_H", gfx_othermode_h_handler_f3d } },
    { F3DEX_G_SETGEOMETRYMODE, { "G_SETGEOMETRYMODE", gfx_set_geometry_mode_handler_f3dex } },
    { F3DEX_G_CLEARGEOMETRYMODE, { "G_CLEARGEOMETRYMODE", gfx_clear_geometry_mode_handler_f3dex } },
    { F3DEX_G_VTX, { "G_VTX", gfx_vtx_handler_f3d } },
    { F3DEX_G_TRI1, { "G_TRI1", gfx_tri1_handler_f3d } },
    { F3DEX_G_MODIFYVTX, { "G_MODIFYVTX", gfx_modify_vtx_handler_f3dex2 } },
    { F3DEX_G_DL, { "G_DL", gfx_dl_handler_common } },
    { F3DEX_G_ENDDL, { "G_ENDDL", gfx_end_dl_handler_common } },
    { F3DEX_G_TRI2, { "G_TRI2", gfx_tri2_handler_f3dex } },
    { F3DEX_G_SPNOOP, { "G_SPNOOP", gfx_spnoop_command_handler_f3dex2 } },
    { F3DEX_G_RDPHALF_1, { "mRdpHALF_1", gfx_stubbed_command_handler } },
};

// LUSTODO: These S2DEX commands have different opcode numbers on F3DEX2 vs other ucodes. More research needs to be done
// to see if the implementations are different.
static const UcodeHandler s2dexHandlers = {
    { F3DEX2_G_BG_COPY, { "G_BG_COPY", gfx_bg_copy_handler_s2dex } },
    { F3DEX2_G_BG_1CYC, { "G_BG_1CYC", gfx_bg_1cyc_handler_s2dex } },
    { F3DEX2_G_OBJ_RENDERMODE, { "G_OBJ_RENDERMODE", gfx_stubbed_command_handler } },
    { F3DEX2_G_OBJ_RECTANGLE_R, { "G_OBJ_RECTANGLE_R", gfx_stubbed_command_handler } },
    { F3DEX2_G_OBJ_RECTANGLE, { "G_OBJ_RECTANGLE", gfx_obj_rectangle_handler_s2dex } },
    { F3DEX2_G_DL, { "G_DL", gfx_dl_handler_common } },
    { F3DEX2_G_ENDDL, { "G_ENDDL", gfx_end_dl_handler_common } },
};

static const UcodeHandler s2dex1Handlers = {
    { F3DEX_G_SPNOOP, { "G_SPNOOP", gfx_spnoop_command_handler_f3dex2 } },
    { F3DEX_G_BG_1CYC, { "G_BG_1CYC", gfx_bg_1cyc_handler_s2dex } },
    { F3DEX_G_BG_COPY, { "G_BG_COPY", gfx_bg_copy_handler_s2dex } },
    { F3DEX_G_OBJ_RECTANGLE, { "G_OBJ_RECTANGLE", gfx_obj_rectangle_handler_s2dex } },
    { F3DEX_G_OBJ_MOVEMEM, { "G_OBJ_MOVEMEM", gfx_obj_movemem_handler_s2dex } },
    { F3DEX_G_OBJ_RENDERMODE, { "G_OBJ_RENDERMODE", gfx_obj_rendermode_handler_s2dex } },
    { F3DEX_G_OBJ_LOADTXTR, { "G_OBJ_LOADTXTR", gfx_obj_loadtxtr_handler_s2dex1 } },
    { F3DEX_G_OBJ_LDTX_SPRITE, { "G_OBJ_LDTX_SPRITE", gfx_obj_ldtx_sprite_handler_s2dex1 } },
    { F3DEX_G_OBJ_LDTX_RECT_R, { "G_OBJ_LDTX_RECT_R", gfx_obj_ldtx_rect_r_handler_s2dex1 } },
    { F3DEX_G_RDPHALF_0, { "G_RDPHALF_0", gfx_rdphalf_0_handler_s2dex1 } },
    { F3DEX_G_DL, { "G_DL", gfx_dl_handler_common } },
    { F3DEX_G_MOVEWORD, { "G_MOVEWORD", gfx_moveword_handler_s2dex1 } },
    { F3DEX_G_SETOTHERMODE_H, { "G_SETOTHERMODE_H", gfx_othermode_h_handler_f3d } },
    { F3DEX_G_SETOTHERMODE_L, { "G_SETOTHERMODE_L", gfx_othermode_l_handler_f3d } },
    { F3DEX_G_ENDDL, { "G_ENDDL", gfx_end_dl_handler_common } },
    { F3DEX_G_RDPHALF_1, { "G_RDPHALF_1", gfx_stubbed_command_handler } },
    { F3DEX_G_RDPHALF_2, { "G_RDPHALF_2", gfx_stubbed_command_handler } },
};

static const UcodeHandler* ucode_handlers[] = {
    &f3dHandlers,    // ucode_f3db
    &f3dHandlers,    // ucode_f3d
    &f3dexHandlers,  // ucode_f3dex
    &f3dexHandlers,  // ucode_f3dexb
    &f3dex2Handlers, // ucode_f3dex2
    &s2dexHandlers,  // ucode_s2dex
    &s2dex1Handlers, // ucode_s2dex1
};

const char* GfxGetOpcodeName(int8_t opcode) {
    if (ucode_handler_index == ucode_s2dex1 && opcode == F3DEX_G_RDPHALF_0) {
        return s2dex1Handlers.at(opcode).first;
    }
    if (rdpHandlers.contains(opcode)) {
        return rdpHandlers.at(opcode).first;
    }

    if (static_cast<size_t>(ucode_handler_index) < sizeof(ucode_handlers) / sizeof(ucode_handlers[0])) {
        if (ucode_handlers[ucode_handler_index]->contains(opcode)) {
            return ucode_handlers[ucode_handler_index]->at(opcode).first;
        } else {
        }
    } else {
    }

    return nullptr;
}

// TODO, implement a system where we can get the current opcode handler by writing to the GWords. If the powers that be
// are OK with that...
static void gfx_set_ucode_handler(UcodeHandlers ucode) {
    // Loaded ucode must be in range of the supported ucode_handlers
    assert(ucode < ucode_max);
    Interpreter* gfx = gInstance;
    ucode_handler_index = ucode;

    // Reset some RSP state values upon ucode load to deal with hardware quirks discovered by emulators
    switch (ucode) {
        case ucode_f3d:
        case ucode_f3db:
        case ucode_f3dex:
        case ucode_f3dexb:
        case ucode_f3dex2:
            gfx->mRsp->fog_mul = 0;
            gfx->mRsp->fog_offset = 0;
            break;
        case ucode_s2dex:
        case ucode_s2dex1:
            gfx->Gfxs2dexResetObjMatrix();
            break;
        default:
            break;
    }
}

static void gfx_step() {
    auto& cmd = g_exec_stack.currCmd();
    auto cmd0 = cmd;
    int8_t opcode = (int8_t)(cmd->words.w0 >> 24);

    if (opcode == F3DEX2_G_LOAD_UCODE) {
        gfx_set_ucode_handler((UcodeHandlers)(cmd->words.w0 & 0xFFFFFF));
        ++cmd;
        return;
        // Instead of having a handler for each ucode for switching ucode, just check for it early and return.
    }

    if (ucode_handler_index == ucode_s2dex1 && opcode == F3DEX_G_RDPHALF_0) {
        if (s2dex1Handlers.at(opcode).second(&cmd)) {
            return;
        }
    } else if (rdpHandlers.contains(opcode)) {
        if (rdpHandlers.at(opcode).second(&cmd)) {
            return;
        }
    } else if (static_cast<size_t>(ucode_handler_index) < sizeof(ucode_handlers) / sizeof(ucode_handlers[0])) {
        if (ucode_handlers[ucode_handler_index]->contains(opcode)) {
            if (ucode_handlers[ucode_handler_index]->at(opcode).second(&cmd)) {
                return;
            }
        } else {
        }
    } else {
    }

    ++cmd;
}

void Interpreter::SpReset() {
    while (!mShaderStack.empty()) {
        mShaderStack.pop();
    }
    mRsp->modelview_matrix_stack_size = 1;
    mRsp->current_num_lights = 2;
    mRsp->lights_changed = true;
    mRsp->lookat[0].dir[0] = 0;
    mRsp->lookat[0].dir[1] = 127;
    mRsp->lookat[0].dir[2] = 0;
    mRsp->lookat[1].dir[0] = 127;
    mRsp->lookat[1].dir[1] = 0;
    mRsp->lookat[1].dir[2] = 0;
    CalculateNormalDir(&mRsp->lookat[0], mRsp->current_lookat_coeffs[0]);
    CalculateNormalDir(&mRsp->lookat[1], mRsp->current_lookat_coeffs[1]);
}

void Interpreter::RegisterFbTexture(const void* cpuAddr, int fbId) {
    mFbTextures[(uintptr_t)cpuAddr] = fbId;
}

void Interpreter::UnregisterFbTexture(const void* cpuAddr) {
    mFbTextures.erase((uintptr_t)cpuAddr);
}

bool Interpreter::SyncColorImageToRdram(uint8_t* rdramBase, uint32_t rdramSize) {
    if (rdramBase == nullptr || rdramSize == 0 || mRdp->color_image_address == nullptr) {
        return false;
    }
    if (mRdp->color_image_format != G_IM_FMT_RGBA || mRdp->color_image_size != G_IM_SIZ_16b) {
        return false;
    }

    const uintptr_t base = (uintptr_t)rdramBase;
    const uintptr_t rawAddress = (uintptr_t)mRdp->color_image_address;
    uint32_t rdramOffset;
    if (rawAddress < rdramSize) {
        rdramOffset = (uint32_t)rawAddress;
    } else if (rawAddress >= base && rawAddress - base < rdramSize) {
        rdramOffset = (uint32_t)(rawAddress - base);
    } else {
        return false;
    }

    const uint32_t width = mRdp->color_image_width;
    uint32_t height = mNativeDimensions.height > 0.0f ? (uint32_t)(mNativeDimensions.height + 0.5f) : SCREEN_HEIGHT;
    if (width == 0 || width > 1024 || height == 0 || height > 1024) {
        return false;
    }

    const uint64_t pixelCount = (uint64_t)width * height;
    const uint64_t byteCount = pixelCount * sizeof(uint16_t);
    if (byteCount > rdramSize || rdramOffset > rdramSize - byteCount) {
        return false;
    }

    if (mRdramReadbackFb < 0) {
        mRdramReadbackFb = CreateFrameBuffer(width, height, width, height, 0, true);
        mRdramReadbackWidth = width;
        mRdramReadbackHeight = height;
    } else if (mRdramReadbackWidth != width || mRdramReadbackHeight != height) {
        mRapi->UpdateFramebufferParameters(mRdramReadbackFb, width, height, true, true, false, false);
        auto fbIt = mFrameBuffers.find(mRdramReadbackFb);
        if (fbIt != mFrameBuffers.end()) {
            fbIt->second.orig_width = width;
            fbIt->second.orig_height = height;
            fbIt->second.applied_width = width;
            fbIt->second.applied_height = height;
            fbIt->second.native_width = width;
            fbIt->second.native_height = height;
        }
        mRdramReadbackWidth = width;
        mRdramReadbackHeight = height;
    }

    const int srcFb = mRendersToFb ? mGameFb : 0;
    int srcX0 = 0;
    int srcY0 = 0;
    int srcX1 = static_cast<int>(mCurDimensions.width > 0 ? mCurDimensions.width : width);
    int srcY1 = static_cast<int>(mCurDimensions.height > 0 ? mCurDimensions.height : height);

    if (srcFb == 0) {
        srcX0 = mGameWindowViewport.x;
        srcY0 = mGameWindowViewport.y;
        srcX1 = mGameWindowViewport.x + static_cast<int>(mGameWindowViewport.width);
        srcY1 = mGameWindowViewport.y + static_cast<int>(mGameWindowViewport.height);
    }

    mRapi->CopyFramebuffer(mRdramReadbackFb, srcFb,
                           srcX0, srcY0, srcX1, srcY1,
                           0, 0, width, height);

    mRdramReadbackBuffer.resize((size_t)pixelCount);
    mRapi->ReadFramebufferToCPU(mRdramReadbackFb, width, height, mRdramReadbackBuffer.data());

    for (uint32_t y = 0; y < height; ++y) {
        const uint16_t* srcRow = mRdramReadbackBuffer.data() + (size_t)y * width;
        const uint32_t rowOffset = rdramOffset + y * width * 2u;
        for (uint32_t x = 0; x < width; ++x) {
            *(uint16_t*)((uintptr_t)(rdramBase + rowOffset + x * 2u) ^ U16_TWIDDLE) = srcRow[x];
        }
    }
    RDRAM_MarkDirtyRange(rdramOffset, (uint32_t)byteCount);

    return true;
}

void Interpreter::GetDimensions(uint32_t* width, uint32_t* height, int32_t* posX, int32_t* posY) {
    mWapi->GetDimensions(width, height, posX, posY);
}

void Interpreter::Init(class GfxWindowBackend* wapi, class GfxRenderingAPI* rapi, const char* game_name,
                       bool start_in_fullscreen, uint32_t width, uint32_t height, uint32_t posX, uint32_t posY) {
    mWapi = wapi;
    mRapi = rapi;
    mWapi->Init(game_name, rapi->GetName(), start_in_fullscreen, width, height, posX, posY);
    mRapi->Init();
    mRapi->UpdateFramebufferParameters(0, width, height, false, true, true, true);

    mCurDimensions.width = width;
    mCurDimensions.height = height;
	mCurAspectRatioDeltaForX = (4.0f / 3.0f) / ((float)width / (float)height);

    mGameFb = mRapi->CreateFramebuffer();

    mNativeDimensions.width = SCREEN_WIDTH;
    mNativeDimensions.height = SCREEN_HEIGHT;

    for (int i = 0; i < MAX_SEGMENT_POINTERS; i++) {
        mSegmentPointers[i] = 0;
    }

    if (mTexUploadBuffer == nullptr) {
        // We cap texture max to 8k, because why would you need more?
        int max_tex_size = std::min(8192, mRapi->GetMaxTextureSize());
        mTexUploadBuffer = (uint8_t*)malloc(max_tex_size * max_tex_size * 4);
    }

    ucode_handler_index = UcodeHandlers::ucode_f3dex2;

    // Pre-allocate texture cache buckets to prevent rehash-induced iterator invalidation.
    mTextureCache.map.reserve(TEXTURE_CACHE_MAX_SIZE);
}

void Interpreter::Destroy() {
    // TODO: should also destroy rapi, and any other resources acquired in fast3d
    free(mTexUploadBuffer);
    mWapi->Destroy();

    // Texture cache and loaded textures store references to Resources which need to be unreferenced.
    TextureCacheClear();
    mRdp->texture_to_load.raw_tex_metadata.resource = nullptr;
    mRdp->loaded_texture[0].raw_tex_metadata.resource = nullptr;
    mRdp->loaded_texture[1].raw_tex_metadata.resource = nullptr;
}

GfxRenderingAPI* Interpreter::GetCurrentRenderingAPI() {
    return mRapi;
}

void Interpreter::HandleWindowEvents() {
    mWapi->HandleEvents();
}

bool Interpreter::IsFrameReady() {
    return mWapi->IsFrameReady();
}

bool Interpreter::ViewportMatchesRendererResolution() {
    if (mCurDimensions.width == mGameWindowViewport.width && mCurDimensions.height == mGameWindowViewport.height) {
        return true;
    }
    return false;
}

void Interpreter::StartFrame() {
    mWapi->GetDimensions(&mGfxCurrentWindowDimensions.width, &mGfxCurrentWindowDimensions.height, &mCurWindowPosX,
                         &mCurWindowPosY);
    if (mCurDimensions.height == 0) {
        // Avoid division by zero
        mCurDimensions.height = 1;
    }
    mCurDimensions.aspect_ratio = (float)mCurDimensions.width / (float)mCurDimensions.height;

    // Update the framebuffer sizes when the viewport or native dimension changes
    if (mCurDimensions.width != mPrvDimensions.width || mCurDimensions.height != mPrvDimensions.height ||
        mNativeDimensions.width != mPrevNativeDimensions.width ||
        mNativeDimensions.height != mPrevNativeDimensions.height) {

        for (auto& fb : mFrameBuffers) {
            uint32_t width = fb.second.orig_width, height = fb.second.orig_height;
            if (fb.second.resize) {
                AdjustWidthHeightForScale(width, height, fb.second.native_width, fb.second.native_height);
            }
            if (width != fb.second.applied_width || height != fb.second.applied_height) {
                mRapi->UpdateFramebufferParameters(fb.first, width, height, true, true, true, true);
                fb.second.applied_width = width;
                fb.second.applied_height = height;
            }
        }
    }

    mPrvDimensions = mCurDimensions;
    mPrevNativeDimensions = mNativeDimensions;
    if (!ViewportMatchesRendererResolution()) {
        mRendersToFb = true;
        if (!ViewportMatchesRendererResolution()) {
            mRapi->UpdateFramebufferParameters(mGameFb, mCurDimensions.width, mCurDimensions.height, true,
                                               true, true, true);
        } else {
            // MSAA framebuffer needs to be resolved to an equally sized target when complete, which must therefore
            // match the window size
            mRapi->UpdateFramebufferParameters(mGameFb, mGfxCurrentWindowDimensions.width,
                                               mGfxCurrentWindowDimensions.height, false, true, true, true);
        }
    } else {
        mRendersToFb = false;
    }

    mFbActive = false;
}

GfxExecStack g_exec_stack = {};

void Interpreter::RunGuiOnly() {
    SpReset();

    mGetPixelDepthPending.clear();
    mGetPixelDepthCached.clear();

    mRapi->UpdateFramebufferParameters(0, mGfxCurrentWindowDimensions.width, mGfxCurrentWindowDimensions.height,
                                       false, true, true, !mRendersToFb);
    mRapi->StartFrame();
    mRapi->StartDrawToFramebuffer(mRendersToFb ? mGameFb : 0, (float)mCurDimensions.height / mNativeDimensions.height);
    mRapi->ClearFramebuffer(true, true);
    mRdp->viewport_or_scissor_changed = true;
    mRenderingState.viewport = {};
    mRenderingState.scissor = {};

    Flush();
    mGfxFrameBuffer = 0;

    if (mRendersToFb) {
        mRapi->StartDrawToFramebuffer(0, 1);
        mRapi->ClearFramebuffer(true, true);
        mGfxFrameBuffer = (uintptr_t)mRapi->GetFramebufferTextureId(mGameFb);
    } else if (mFbActive) {
        // Failsafe reset to main framebuffer to prevent softlocking the renderer
        mFbActive = 0;
        mRapi->StartDrawToFramebuffer(0, 1);

        assert(0 && "active framebuffer was never reset back to original");
    }
}

void Interpreter::Run(Gfx* commands, const robin_hood::unordered_map<Mtx*, MtxF>& mtx_replacements) {
    SpReset();

    mGetPixelDepthPending.clear();
    mGetPixelDepthCached.clear();

    mCurMtxReplacements = &mtx_replacements;

    mRapi->UpdateFramebufferParameters(0, mGfxCurrentWindowDimensions.width, mGfxCurrentWindowDimensions.height,
                                       false, true, true, !mRendersToFb);
    mRapi->StartFrame();
    mRapi->StartDrawToFramebuffer(mRendersToFb ? mGameFb : 0, (float)mCurDimensions.height / mNativeDimensions.height);
    mRapi->ClearFramebuffer(true, true);
    mRdp->viewport_or_scissor_changed = true;
    mRenderingState.viewport = {};
    mRenderingState.scissor = {};

    g_exec_stack.start((F3DGfx*)commands);
    while (!g_exec_stack.cmd_stack.empty()) {
        auto cmd = g_exec_stack.cmd_stack.top();

        gfx_step();
    }

    Flush();
    mGfxFrameBuffer = 0;
    currentDir = std::stack<std::string>();

    if (mRendersToFb) {
        mRapi->StartDrawToFramebuffer(0, 1);
        mRapi->ClearFramebuffer(true, true);
        mGfxFrameBuffer = (uintptr_t)mRapi->GetFramebufferTextureId(mGameFb);
    } else if (mFbActive) {
        // Failsafe reset to main framebuffer to prevent softlocking the renderer
        mFbActive = 0;
        mRapi->StartDrawToFramebuffer(0, 1);

        assert(0 && "active framebuffer was never reset back to original");
    }
}

void Interpreter::EndFrame() {
    mRapi->EndFrame();
    mWapi->SwapBuffersBegin();
    mRapi->FinishRender();
    mWapi->SwapBuffersEnd();
	mBufVbo = (float*)vglAllocFromScratch(10 * 1024 * 1024);
}

void gfx_set_target_ucode(UcodeHandlers ucode) {
    ucode_handler_index = ucode;
}

int Interpreter::GetTargetFps() {
    return mWapi->GetTargetFps();
}

void Interpreter::SetTargetFps(int fps) {
    mWapi->SetTargetFps(fps);
}

void Interpreter::SetMaxFrameLatency(int latency) {
    mWapi->SetMaxFrameLatency(latency);
}

int Interpreter::CreateFrameBuffer(uint32_t width, uint32_t height, uint32_t native_width, uint32_t native_height,
                                   uint8_t resize, bool forceFixedAspect) {
    uint32_t orig_width = width, orig_height = height;
    if (resize) {
        AdjustWidthHeightForScale(width, height, native_width, native_height);
    }

    int fb = mRapi->CreateFramebuffer();
    mRapi->UpdateFramebufferParameters(fb, width, height, true, true, true, true);

    mFrameBuffers[fb] = {
        orig_width, orig_height, width, height, native_width, native_height, static_cast<bool>(resize), forceFixedAspect
    };
    return fb;
}

void Interpreter::SetFrameBuffer(int fb, float noiseScale) {
    mRapi->StartDrawToFramebuffer(fb, noiseScale);
    mRapi->ClearFramebuffer(false, true);
}

void Interpreter::CopyFrameBuffer(int fb_dst_id, int fb_src_id, bool copyOnce, bool* hasCopiedPtr) {
    // Do not copy again if we have already copied before
    if (copyOnce && hasCopiedPtr != nullptr && *hasCopiedPtr) {
        return;
    }

    if (fb_src_id == 0 && mRendersToFb) {
        // read from the framebuffer we've been rendering to
        fb_src_id = mGameFb;
    }

    int srcX0, srcY0, srcX1, srcY1;
    int dstX0, dstY0, dstX1, dstY1;

    // When rendering to the main window buffer, the source coordinates must account for any docked ImGui elements
    if (fb_src_id == 0) {
        srcX0 = mGameWindowViewport.x;
        srcY0 = mGameWindowViewport.y;
        srcX1 = mGameWindowViewport.x + mGameWindowViewport.width;
        srcY1 = mGameWindowViewport.y + mGameWindowViewport.height;
    } else {
        srcX0 = 0;
        srcY0 = 0;
        srcX1 = mCurDimensions.width;
        srcY1 = mCurDimensions.height;
    }

    dstX0 = 0;
    dstY0 = 0;
    dstX1 = mCurDimensions.width;
    dstY1 = mCurDimensions.height;

    mRapi->CopyFramebuffer(fb_dst_id, fb_src_id, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1);

    // Set the copied pointer if we have one
    if (hasCopiedPtr != nullptr) {
        *hasCopiedPtr = true;
    }
}

void Interpreter::ResetFrameBuffer() {
    mRapi->StartDrawToFramebuffer(0, (float)mCurDimensions.height / mNativeDimensions.height);
}

void Interpreter::AdjustPixelDepthCoordinates(float& x, float& y) {
    x = x * RATIO_X(mActiveFrameBuffer, mCurDimensions) -
        (mNativeDimensions.width * RATIO_X(mActiveFrameBuffer, mCurDimensions) - mCurDimensions.width) / 2;
    y *= RATIO_Y(mActiveFrameBuffer, mCurDimensions);
    if (!mRendersToFb) {
        x += mGameWindowViewport.x;
        y += mGfxCurrentWindowDimensions.height - (mGameWindowViewport.y + mGameWindowViewport.height);
    }
}

void Interpreter::GetPixelDepthPrepare(float x, float y) {
    AdjustPixelDepthCoordinates(x, y);
    mGetPixelDepthPending.emplace(x, y);
}

uint16_t Interpreter::GetPixelDepth(float x, float y) {
    AdjustPixelDepthCoordinates(x, y);

    auto depthIt = mGetPixelDepthCached.find(std::make_pair(x, y));
    if (depthIt != mGetPixelDepthCached.end()) {
        return depthIt->second;
    }

    mGetPixelDepthPending.emplace(x, y);

    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff> res =
        mRapi->GetPixelDepth(mRendersToFb ? mGameFb : 0, mGetPixelDepthPending);
    mGetPixelDepthCached.insert(res.begin(), res.end());
    mGetPixelDepthPending.clear();

    return mGetPixelDepthCached.find(std::make_pair(x, y))->second;
}

void Interpreter::RegisterBlendedTexture(const char* name, uint8_t* mask, uint8_t* replacement) {
    mMaskedTextures[name] = MaskedTextureEntry{ mask, replacement };
}

void Interpreter::UnregisterBlendedTexture(const char* name) {
    mMaskedTextures.erase(name);
}

// New getters and setters
void Interpreter::SetNativeDimensions(float width, float height) {
    mNativeDimensions.width = width;
    mNativeDimensions.height = height;
}

void Interpreter::SetDisplayConfiguration(uint32_t width, uint32_t height, int32_t x, int32_t y, float aspectDelta) {
    mCurDimensions.width = width;
    mCurDimensions.height = height;
    mCurDimensions.aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
    mGameWindowViewport.x = static_cast<int16_t>(x);
    mGameWindowViewport.y = static_cast<int16_t>(y);
    mGameWindowViewport.width = width;
    mGameWindowViewport.height = height;
    mCurAspectRatioDeltaForX = aspectDelta;
}

void Interpreter::GetCurDimensions(uint32_t* width, uint32_t* height) {
    *width = mCurDimensions.width;
    *height = mCurDimensions.height;
}

} // namespace Fast

void gfx_cc_get_features(uint64_t shader_id0, uint64_t shader_id1, struct CCFeatures* cc_features) {
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 4; k++) {
                cc_features->c[i][j][k] = shader_id0 >> i * 32 + j * 16 + k * 4 & 0xf;
            }
        }
    }

    cc_features->opt_alpha = (shader_id1 & SHADER_OPT(ALPHA)) != 0;
    cc_features->opt_fog = (shader_id1 & SHADER_OPT(FOG)) != 0;
    cc_features->opt_texture_edge = (shader_id1 & SHADER_OPT(TEXTURE_EDGE)) != 0;
    cc_features->opt_noise = (shader_id1 & SHADER_OPT(NOISE)) != 0;
    cc_features->opt_2cyc = (shader_id1 & SHADER_OPT(_2CYC)) != 0;
    cc_features->opt_alpha_threshold = (shader_id1 & SHADER_OPT(ALPHA_THRESHOLD)) != 0;
    cc_features->opt_invisible = (shader_id1 & SHADER_OPT(INVISIBLE)) != 0;
    cc_features->opt_grayscale = (shader_id1 & SHADER_OPT(GRAYSCALE)) != 0;
    cc_features->opt_prim_depth = (shader_id1 & SHADER_OPT(PRIM_DEPTH)) != 0;

    cc_features->clamp[0][0] = shader_id1 & SHADER_OPT(TEXEL0_CLAMP_S);
    cc_features->clamp[0][1] = shader_id1 & SHADER_OPT(TEXEL0_CLAMP_T);
    cc_features->clamp[1][0] = shader_id1 & SHADER_OPT(TEXEL1_CLAMP_S);
    cc_features->clamp[1][1] = shader_id1 & SHADER_OPT(TEXEL1_CLAMP_T);

    cc_features->usedTextures[0] = false;
    cc_features->usedTextures[1] = false;
    cc_features->used_masks[0] = false;
    cc_features->used_masks[1] = false;
    cc_features->used_blend[0] = false;
    cc_features->used_blend[1] = false;
    cc_features->numInputs = 0;

    for (int c = 0; c < 2; c++) {
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 4; j++) {
                if (cc_features->c[c][i][j] >= SHADER_INPUT_1 && cc_features->c[c][i][j] <= SHADER_INPUT_7) {
                    if (cc_features->c[c][i][j] > cc_features->numInputs) {
                        cc_features->numInputs = cc_features->c[c][i][j];
                    }
                }
                if (cc_features->c[c][i][j] == SHADER_TEXEL0 || cc_features->c[c][i][j] == SHADER_TEXEL0A) {
                    cc_features->usedTextures[0] = true;
                    if (cc_features->opt_2cyc) {
                        cc_features->usedTextures[1] = true;
                    }
                }
                if (cc_features->c[c][i][j] == SHADER_TEXEL1 || cc_features->c[c][i][j] == SHADER_TEXEL1A) {
                    cc_features->usedTextures[1] = true;
                    if (cc_features->opt_2cyc) {
                        cc_features->usedTextures[0] = true;
                    }
                }
            }
        }
    }

    for (int c = 0; c < 2; c++) {
        cc_features->do_single[c][0] = cc_features->c[c][0][2] == SHADER_0;
        cc_features->do_single[c][1] = cc_features->c[c][1][2] == SHADER_0;
        cc_features->do_multiply[c][0] = cc_features->c[c][0][1] == SHADER_0 && cc_features->c[c][0][3] == SHADER_0;
        cc_features->do_multiply[c][1] = cc_features->c[c][1][1] == SHADER_0 && cc_features->c[c][1][3] == SHADER_0;
        cc_features->do_mix[c][0] = cc_features->c[c][0][1] == cc_features->c[c][0][3];
        cc_features->do_mix[c][1] = cc_features->c[c][1][1] == cc_features->c[c][1][3];
        cc_features->color_alpha_same[c] = (shader_id0 >> c * 32 & 0xffff) == (shader_id0 >> c * 32 + 16 & 0xffff);
    }

    if (cc_features->usedTextures[0] && shader_id1 & SHADER_OPT(TEXEL0_MASK)) {
        cc_features->used_masks[0] = true;
    }
    if (cc_features->usedTextures[1] && shader_id1 & SHADER_OPT(TEXEL1_MASK)) {
        cc_features->used_masks[1] = true;
    }

    if (cc_features->usedTextures[0] && shader_id1 & SHADER_OPT(TEXEL0_BLEND)) {
        cc_features->used_blend[0] = true;
    }
    if (cc_features->usedTextures[1] && shader_id1 & SHADER_OPT(TEXEL1_BLEND)) {
        cc_features->used_blend[1] = true;
    }

    cc_features->shader_id = Fast::ShaderIdUnmask(shader_id1);
}

extern "C" int gfx_create_framebuffer(uint32_t width, uint32_t height, uint32_t native_width, uint32_t native_height,
                                      uint8_t resize, bool forceFixedAspect) {
    return Fast::mInstance.lock().get()->CreateFrameBuffer(width, height, native_width, native_height, resize,
                                                           forceFixedAspect);
}

extern "C" void gfx_texture_cache_clear() {
    Fast::mInstance.lock().get()->TextureCacheClear();
}

extern "C" void gfx_shader_cache_clear() {
    auto instance = Fast::gInstance;
    instance->mColorCombinerPool.clear();
    instance->mPrevCombiner = Fast::mInstance.lock().get()->mColorCombinerPool.end();
    instance->mRenderingState.mShaderProgram = nullptr;
    instance->mRapi->ClearShaderCache();
}

extern "C" void gfx_register_fb_texture(const void* cpuAddr, int fbId) {
    Fast::mInstance.lock().get()->RegisterFbTexture(cpuAddr, fbId);
}

extern "C" void gfx_unregister_fb_texture(const void* cpuAddr) {
    Fast::mInstance.lock().get()->UnregisterFbTexture(cpuAddr);
}