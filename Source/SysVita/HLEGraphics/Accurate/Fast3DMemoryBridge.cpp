#include "stdafx.h"
#include "SysVita/HLEGraphics/Accurate/Fast3DMemoryBridge.h"
#include "SysVita/HLEGraphics/Accurate/Fast3DABI.h"

#include "Core/Memory.h"
#include "OSHLE/ultra_gbi.h"

#include <malloc.h>
#include <string.h>
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif


namespace {
struct HostGfx { u32 w0; u32 w1; };
struct HostVtx {
    float x, y, z;
    u16 flag;
    s16 s, t;
    u8 c0, c1, c2, c3;
    u16 pad;
};
static_assert(sizeof(HostVtx) == 24, "Fast3D vertex ABI mismatch");
static_assert(sizeof(Fast::F3DVtx) == sizeof(HostVtx), "Fast3D vertex ABI mismatch");
struct HostViewport { s16 scale[4]; s16 trans[4]; };
}

Fast3DMemoryBridge::Fast3DMemoryBridge()
    : mInterpreter(nullptr), mArenaChunks(nullptr), mArenaChunkCount(0), mArenaChunkCapacity(0),
      mTextureStages(nullptr), mTextureStageCount(0), mTextureStageCapacity(0),
      mBuildSerial(0), mTranslationFailed(false), mSawFullSync(false), mHaveTextureImage(false),
      mTextureImageW0(0), mTextureImageResolved(0), mTextureImageCommand(nullptr),
      mTextureImageStaging(nullptr) {
    memset(mSegments, 0, sizeof(mSegments));
}

Fast3DMemoryBridge::~Fast3DMemoryBridge() {
    for (u32 i = 0; i < mArenaChunkCount; ++i) free(mArenaChunks[i].data);
    free(mArenaChunks);
    for (u32 i = 0; i < mTextureStageCount; ++i) free(mTextureStages[i].data);
    free(mTextureStages);
}

void Fast3DMemoryBridge::Reset() {
    for (u32 i = 0; i < mArenaChunkCount; ++i) mArenaChunks[i].used = 0;
}

void* Fast3DMemoryBridge::Alloc(size_t size, size_t alignment) {
    if (size == 0) size = 1;
    if (alignment < sizeof(void*)) alignment = sizeof(void*);

    for (u32 i = 0; i < mArenaChunkCount; ++i) {
        ArenaChunk& chunk = mArenaChunks[i];
        const size_t aligned = (chunk.used + alignment - 1) & ~(alignment - 1);
        if (aligned <= chunk.capacity && size <= chunk.capacity - aligned) {
            void* p = chunk.data + aligned;
            chunk.used = aligned + size;
            return p;
        }
    }

    const size_t kDefaultChunkSize = 256 * 1024;
    const size_t needed = size + alignment;
    const size_t capacity = needed > kDefaultChunkSize ? (needed + 0xFFFF) & ~((size_t)0xFFFF) : kDefaultChunkSize;
    u8* data = (u8*)memalign(64, capacity);
    if (!data) return nullptr;

    if (mArenaChunkCount == mArenaChunkCapacity) {
        const u32 next = mArenaChunkCapacity ? mArenaChunkCapacity * 2 : 8;
        ArenaChunk* chunks = (ArenaChunk*)realloc(mArenaChunks, sizeof(ArenaChunk) * next);
        if (!chunks) {
            free(data);
            return nullptr;
        }
        mArenaChunks = chunks;
        mArenaChunkCapacity = next;
    }

    ArenaChunk& chunk = mArenaChunks[mArenaChunkCount++];
    chunk.data = data;
    chunk.capacity = capacity;
    chunk.used = 0;

    const size_t aligned = (chunk.used + alignment - 1) & ~(alignment - 1);
    void* p = chunk.data + aligned;
    chunk.used = aligned + size;
    return p;
}

u32 Fast3DMemoryBridge::Resolve(u32 address) const {
    const u32 seg = address >> 24;
    if (seg && seg < 16) return (mSegments[seg] + (address & 0x00FFFFFF)) & 0x00FFFFFF;
    return address & 0x00FFFFFF;
}

u32 Fast3DMemoryBridge::Read32(u32 address) const { return *(u32*)(g_pu8RamBase + (address & 0x00FFFFFF)); }
s16 Fast3DMemoryBridge::ReadS16(u32 address) const { return (s16)QuickRead16Bits(g_pu8RamBase, address & 0x00FFFFFF); }
u16 Fast3DMemoryBridge::ReadU16(u32 address) const { return QuickRead16Bits(g_pu8RamBase, address & 0x00FFFFFF); }
u8 Fast3DMemoryBridge::ReadU8(u32 address) const { return g_pu8RamBase[(address & 0x00FFFFFF) ^ U8_TWIDDLE]; }

void Fast3DMemoryBridge::CopyRdramBytes(u8* dst, u32 address, u32 size) const {
    if (!dst || size == 0) return;

    u32 offset = 0;

    while (offset < size && ((address + offset) & 3)) {
        dst[offset] = ReadU8(address + offset);
        ++offset;
    }

    for (; offset + 16 <= size; offset += 16) {
        const uint8x16_t src = vld1q_u8(g_pu8RamBase + address + offset);
        const uint8x16_t swapped = vrev32q_u8(src);
        vst1q_u8(dst + offset, swapped);
    }

    for (; offset + 4 <= size; offset += 4) {
        const u32 value = *(const u32*)(g_pu8RamBase + address + offset);
        *(u32*)(dst + offset) = __builtin_bswap32(value);
    }

    for (; offset < size; ++offset) {
        dst[offset] = ReadU8(address + offset);
    }
}

void Fast3DMemoryBridge::HashRdramBytes(u32 address, u32 size, u32 hash[4]) const {
    const u8* src = g_pu8RamBase + address;
    u32 offset = 0;

    static const u32 kSeed[4] = { 2166136261u, 2246822519u, 3266489917u, 668265263u };
    static const u32 kPrime[4] = { 16777619u, 2246822519u, 3266489917u, 374761393u };

    uint32x4_t h = vld1q_u32(kSeed);
    const uint32x4_t prime = vld1q_u32(kPrime);

    for (; offset + 16 <= size; offset += 16) {
        const uint32x4_t value = vreinterpretq_u32_u8(vld1q_u8(src + offset));
        h = veorq_u32(h, value);
        h = vmulq_u32(h, prime);
        h = vaddq_u32(h, vextq_u32(value, value, 1));
    }

    vst1q_u32(hash, h);

    u32 lane = (offset >> 2) & 3u;
    for (; offset + 4 <= size; offset += 4) {
        u32 value;
        memcpy(&value, src + offset, sizeof(value));
        hash[lane] = (hash[lane] ^ value) * 16777619u;
        lane = (lane + 1) & 3u;
    }

    for (; offset < size; ++offset) {
        hash[lane] = (hash[lane] ^ src[offset]) * 16777619u;
        lane = (lane + 1) & 3u;
    }

    hash[0] ^= size * 0x9E3779B1u;
    hash[1] ^= (size << 7) | (size >> 25);
    hash[2] ^= size * 0x85EBCA77u;
    hash[3] ^= size * 0xC2B2AE3Du;
}

bool Fast3DMemoryBridge::StageTextureImage(u32 requiredBytes, bool palette, u32 sourceOffset,
                                                u32 rowBytes, u8 swizzleGroup) {
    if (!mHaveTextureImage || !mTextureImageCommand) return false;
    if (requiredBytes == 0) requiredBytes = 1;

    if (rowBytes == 0 || swizzleGroup == 0 || rowBytes < swizzleGroup ||
        requiredBytes < rowBytes * 2 || (requiredBytes % rowBytes) != 0) {
        rowBytes = 0;
        swizzleGroup = 0;
    }

    const u64 source64 = (u64)mTextureImageResolved + sourceOffset;
    if (source64 >= gRamSize || requiredBytes > gRamSize - (u32)source64) return false;
    const u32 sourceAddress = (u32)source64;

    TextureStage* stage = nullptr;
    for (u32 i = 0; i < mTextureStageCount; ++i) {
        if (mTextureStages[i].address == sourceAddress &&
            mTextureStages[i].size == requiredBytes &&
            mTextureStages[i].rowBytes == rowBytes &&
            mTextureStages[i].swizzleGroup == swizzleGroup) {
            stage = &mTextureStages[i];
            break;
        }
    }

    u32 hash[4] = {};
    bool hashComputed = false;
    if ((!stage || stage->dirty) && !(stage && stage->validatedBuild == mBuildSerial)) {
        HashRdramBytes(sourceAddress, requiredBytes, hash);
        hashComputed = true;
    }

    const auto copyTexture = [&](u8* data) {
        CopyRdramBytes(data, sourceAddress, requiredBytes);
        if (rowBytes == 0 || swizzleGroup == 0) return;

        const u32 half = swizzleGroup >> 1;
        const u32 rows = requiredBytes / rowBytes;
        for (u32 row = 1; row < rows; row += 2) {
            u8* rowPtr = data + row * rowBytes;
            for (u32 offset = 0; offset + swizzleGroup <= rowBytes; offset += swizzleGroup) {
                u8 tmp[8];
                memcpy(tmp, rowPtr + offset, half);
                memcpy(rowPtr + offset, rowPtr + offset + half, half);
                memcpy(rowPtr + offset + half, tmp, half);
            }
        }
    };

    if (!stage) {
        if (mTextureStageCount == mTextureStageCapacity) {
            const u32 next = mTextureStageCapacity ? mTextureStageCapacity * 2 : 64;
            TextureStage* stages = (TextureStage*)realloc(mTextureStages, sizeof(TextureStage) * next);
            if (!stages) return false;
            mTextureStages = stages;
            mTextureStageCapacity = next;
        }

        u8* data = (u8*)memalign(16, requiredBytes);
        if (!data) return false;
        copyTexture(data);

        stage = &mTextureStages[mTextureStageCount++];
        stage->address = sourceAddress;
        stage->size = requiredBytes;
        stage->hash[0] = hash[0];
        stage->hash[1] = hash[1];
        stage->hash[2] = hash[2];
        stage->hash[3] = hash[3];
        stage->validatedBuild = mBuildSerial;
        stage->rowBytes = rowBytes;
        stage->swizzleGroup = swizzleGroup;
        stage->dirty = false;
        stage->data = data;
    } else if (hashComputed &&
               (stage->hash[0] != hash[0] || stage->hash[1] != hash[1] ||
                stage->hash[2] != hash[2] || stage->hash[3] != hash[3])) {
        if (mInterpreter) {
            if (palette) mInterpreter->TextureCacheClear();
            else mInterpreter->TextureCacheDelete(stage->data);
        }
        copyTexture(stage->data);
        stage->hash[0] = hash[0];
        stage->hash[1] = hash[1];
        stage->hash[2] = hash[2];
        stage->hash[3] = hash[3];
    }

    stage->dirty = false;
    stage->validatedBuild = mBuildSerial;
    mTextureImageStaging = stage->data;
    ((HostGfx*)mTextureImageCommand)->w1 = (u32)(uintptr_t)stage->data;
    return true;
}

void* Fast3DMemoryBridge::ConvertMatrix(u32 address) {
    float* m = (float*)Alloc(64);
    if (!m) return nullptr;
    address = Resolve(address);
    for (u32 i = 0; i < 4; ++i) {
        for (u32 j = 0; j < 4; j += 2) {
            const u32 idx = i * 2 + j / 2;
            const u32 ip = Read32(address + idx * 4);
            const u32 fp = Read32(address + (8 + idx) * 4);
            const s32 a = (s32)((ip & 0xFFFF0000u) | (fp >> 16));
            const s32 b = (s32)((ip << 16) | (fp & 0xFFFFu));
            m[i * 4 + j] = (float)a / 65536.0f;
            m[i * 4 + j + 1] = (float)b / 65536.0f;
        }
    }
    return m;
}

void* Fast3DMemoryBridge::ConvertVertices(u32 address, u32 count) {
    HostVtx* out = (HostVtx*)Alloc(sizeof(HostVtx) * count);
    if (!out) return nullptr;
    address = Resolve(address);
    for (u32 i = 0; i < count; ++i) {
        const u32 a = address + i * 16;
        out[i].x = (float)ReadS16(a + 0);
        out[i].y = (float)ReadS16(a + 2);
        out[i].z = (float)ReadS16(a + 4);
        out[i].flag = ReadU16(a + 6);
        out[i].s = ReadS16(a + 8);
        out[i].t = ReadS16(a + 10);
        out[i].c0 = ReadU8(a + 12);
        out[i].c1 = ReadU8(a + 13);
        out[i].c2 = ReadU8(a + 14);
        out[i].c3 = ReadU8(a + 15);
        out[i].pad = 0;
    }
    return out;
}

void* Fast3DMemoryBridge::ConvertViewport(u32 address) {
    HostViewport* vp = (HostViewport*)Alloc(sizeof(HostViewport));
    if (!vp) return nullptr;
    address = Resolve(address);
    for (u32 i = 0; i < 4; ++i) vp->scale[i] = ReadS16(address + i * 2);
    for (u32 i = 0; i < 4; ++i) vp->trans[i] = ReadS16(address + 8 + i * 2);
    return vp;
}

void* Fast3DMemoryBridge::ConvertLight(u32 address, u32 size) {
    u8* light = (u8*)Alloc(size);
    if (!light) return nullptr;
    address = Resolve(address);
    for (u32 i = 0; i < size; ++i) light[i] = ReadU8(address + i);
    return light;
}

void* Fast3DMemoryBridge::StageS2DEXImage(u32 address, u16 imageW, u16 imageH, u8 imageSiz) {
    if (imageSiz > G_IM_SIZ_32b) return nullptr;

    const u32 width = imageW >> 2;
    const u32 height = imageH >> 2;
    if (width == 0 || height == 0) return nullptr;

    const u64 pixels = (u64)width * height;
    const u64 bytes64 = ((pixels << imageSiz) + 1) >> 1;
    if (bytes64 == 0 || bytes64 > 0xFFFFFFFFu) return nullptr;

    address = Resolve(address);
    const u32 bytes = (u32)bytes64;
    if (address >= gRamSize || bytes > gRamSize - address) return nullptr;

    u8* data = (u8*)Alloc(bytes, 16);
    if (!data) return nullptr;
    CopyRdramBytes(data, address, bytes);
    return data;
}

void* Fast3DMemoryBridge::ConvertS2DEXBg(u32 address) {
    address = Resolve(address);
    if (address >= gRamSize || 40 > gRamSize - address) return nullptr;

    Fast::F3DuObjBg* bg = (Fast::F3DuObjBg*)Alloc(sizeof(Fast::F3DuObjBg), 8);
    if (!bg) return nullptr;
    memset(bg, 0, sizeof(*bg));

    bg->b.imageX = ReadU16(address + 0);
    bg->b.imageW = ReadU16(address + 2);
    bg->b.frameX = ReadS16(address + 4);
    bg->b.frameW = ReadU16(address + 6);
    bg->b.imageY = ReadU16(address + 8);
    bg->b.imageH = ReadU16(address + 10);
    bg->b.frameY = ReadS16(address + 12);
    bg->b.frameH = ReadU16(address + 14);

    const u32 imageAddress = Read32(address + 16);
    bg->b.imageLoad = ReadU16(address + 20);
    bg->b.imageFmt = ReadU8(address + 22);
    bg->b.imageSiz = ReadU8(address + 23);
    bg->b.imagePal = ReadU16(address + 24);
    bg->b.imageFlip = ReadU16(address + 26);

    void* image = StageS2DEXImage(imageAddress, bg->b.imageW, bg->b.imageH, bg->b.imageSiz);
    if (!image) return nullptr;
    bg->b.imagePtr = (unsigned long long int*)image;
    return bg;
}

void* Fast3DMemoryBridge::ConvertS2DEXSprite(u32 address) {
    address = Resolve(address);
    if (address >= gRamSize || 24 > gRamSize - address) return nullptr;

    Fast::F3DuObjSprite* sprite = (Fast::F3DuObjSprite*)Alloc(sizeof(Fast::F3DuObjSprite), 8);
    if (!sprite) return nullptr;
    memset(sprite, 0, sizeof(*sprite));

    sprite->s.objX = ReadS16(address + 0);
    sprite->s.scaleW = ReadU16(address + 2);
    sprite->s.imageW = ReadU16(address + 4);
    sprite->s.paddingX = ReadU16(address + 6);
    sprite->s.objY = ReadS16(address + 8);
    sprite->s.scaleH = ReadU16(address + 10);
    sprite->s.imageH = ReadU16(address + 12);
    sprite->s.paddingY = ReadU16(address + 14);
    sprite->s.imageStride = ReadU16(address + 16);
    sprite->s.imageAdrs = ReadU16(address + 18);
    sprite->s.imageFmt = ReadU8(address + 20);
    sprite->s.imageSiz = ReadU8(address + 21);
    sprite->s.imagePal = ReadU8(address + 22);
    sprite->s.imageFlags = ReadU8(address + 23);
    return sprite;
}

u32 Fast3DMemoryBridge::CountListCommands(u32 address, GBIVersion version) const {
    if (address >= gRamSize) return 0;

    const u32 remainingCommands = (gRamSize - address) / 8;
    const u32 maxCommands = remainingCommands < 65536 ? remainingCommands : 65536;

    for (u32 n = 0; n < maxCommands; ++n) {
        const u32 pc = address + n * 8;
        if (pc > gRamSize - 8) return 0;
        const u32 w0 = Read32(pc);
        const u8 op = (u8)(w0 >> 24);

        if (version == GBI_2 || version == GBI_2_S2DEX) {
            if (op == 0xDF || (op == 0xDE && (((w0 >> 16) & 0xFF) == G_DL_NOPUSH))) {
                return n + 1;
            }
        } else {
            if (op == 0xB8 || (op == 0x06 && (((w0 >> 16) & 0xFF) == G_DL_NOPUSH))) {
                return n + 1;
            }
        }
    }
    return 0;
}

void* Fast3DMemoryBridge::TranslateList(u32 address, GBIVersion& version, u32 depth) {
    if (depth > 24) return nullptr;

    address = Resolve(address);
    if (address >= gRamSize) return nullptr;

    const u32 commandCount = CountListCommands(address, version);
    if (commandCount == 0) return nullptr;

    const u32 outputMultiplier = version == GBI_GE ? 5 : 2;
    HostGfx* out = (HostGfx*)Alloc(sizeof(HostGfx) * commandCount * outputMultiplier, 8);
    if (!out) return nullptr;
    u32 outIndex = 0;

    for (u32 n = 0; n < commandCount; ++n) {
        const u32 pc = address + n * 8;
        u32 w0 = Read32(pc);
        u32 w1 = Read32(pc + 4);
        const u8 op = (u8)(w0 >> 24);
        bool emitTextureSetImg = false;
        const auto fail = [&]() -> void* {
            mTranslationFailed = true;
            return nullptr;
        };

        if ((version == GBI_2 || version == GBI_2_S2DEX) && op == 0xE1 && n + 1 < commandCount) {
            const u32 nextW0 = Read32(pc + 8);
            if ((u8)(nextW0 >> 24) == 0xDD) continue;
        }

        if (op == G_RDP_RDPFULLSYNC) mSawFullSync = true;

        if (op == G_RDP_SETTIMG) {
            mHaveTextureImage = true;
            mTextureImageW0 = w0;
            mTextureImageResolved = Resolve(w1);
            mTextureImageCommand = nullptr;
            mTextureImageStaging = nullptr;
        } else if (op == G_RDP_LOADTLUT) {
            const u32 count = ((w1 >> 14) & 0x3FF) + 1;

            if (mHaveTextureImage && (((mTextureImageW0 >> 19) & 0x3) != G_IM_SIZ_16b)) return fail();
            if (!StageTextureImage(count * 2, true)) return fail();
            emitTextureSetImg = true;
        } else if (op == G_RDP_LOADBLOCK) {
            if (!mHaveTextureImage) return fail();
            const u32 siz = (mTextureImageW0 >> 19) & 0x3;
            const u32 width = (mTextureImageW0 & 0xFFF) + 1;
            const u32 uls = (w0 >> 12) & 0xFFF;
            const u32 ult = w0 & 0xFFF;
            const u32 lrs = (w1 >> 12) & 0xFFF;
            const u32 dxt = w1 & 0xFFF;
            const u32 pixels = lrs + 1;

            u32 bytes = 0;
            switch (siz) {
                case G_IM_SIZ_4b:  bytes = (pixels + 1) >> 1; break;
                case G_IM_SIZ_8b:  bytes = pixels; break;
                case G_IM_SIZ_16b: bytes = pixels * 2; break;
                case G_IM_SIZ_32b: bytes = pixels * 4; break;
                default: return fail();
            }

            const u64 pitch = ((u64)width << siz) >> 1;
            const u64 xOffset = ((u64)uls << siz) >> 1;
            const u64 sourceOffset64 = (u64)ult * pitch + xOffset;
            if (sourceOffset64 > 0xFFFFFFFFu) return fail();
            const u32 sourceOffset = (u32)sourceOffset64;

            u32 rowBytes = 0;
            u8 swizzleGroup = 0;

            if (dxt == 0 && uls == 0 && ult == 0) {
                const u32 lookaheadEnd = (n + 8 < commandCount) ? n + 8 : commandCount;
                for (u32 lookahead = n + 1; lookahead < lookaheadEnd; ++lookahead) {
                    const u32 nextW0 = Read32(address + lookahead * 8);
                    const u8 nextOp = (u8)(nextW0 >> 24);

                    if (nextOp == G_RDP_SETTIMG || nextOp == G_RDP_LOADBLOCK || nextOp == G_RDP_LOADTILE) break;

                    if (nextOp == G_RDP_SETTILE) {
                        const u32 line = (nextW0 >> 9) & 0x1FF;
                        if (line == 0) continue;

                        const u32 renderSiz = (nextW0 >> 19) & 0x3;
                        u32 candidateRowBytes = line * 8;
                        if (renderSiz == G_IM_SIZ_32b) candidateRowBytes *= 2;

                        const u8 candidateGroup = renderSiz == G_IM_SIZ_32b ? 16 : 8;
                        if (candidateRowBytes >= candidateGroup && bytes >= candidateRowBytes * 2 &&
                            (bytes % candidateRowBytes) == 0) {
                            rowBytes = candidateRowBytes;
                            swizzleGroup = candidateGroup;
                        }
                        break;
                    }
                }
            }

            if (!StageTextureImage(bytes, false, sourceOffset, rowBytes, swizzleGroup)) return fail();
            emitTextureSetImg = true;

            if (uls != 0 || ult != 0) w0 &= 0xFF000000u;
        } else if (op == G_RDP_LOADTILE) {
            if (!mHaveTextureImage) return fail();
            const u32 siz = (mTextureImageW0 >> 19) & 0x3;
            const u32 width = (mTextureImageW0 & 0xFFF) + 1;
            const u32 uls = (w0 >> 12) & 0xFFF;
            const u32 ult = w0 & 0xFFF;
            const u32 lrs = (w1 >> 12) & 0xFFF;
            const u32 lrt = w1 & 0xFFF;
            if (lrs < uls || lrt < ult) return fail();

            u32 byteShift = 0;
            if (siz == G_IM_SIZ_16b) byteShift = 1;
            else if (siz == G_IM_SIZ_32b) byteShift = 2;

            const u32 offsetX = uls >> 2;
            const u32 offsetY = ult >> 2;
            const u32 tileWidth = ((lrs - uls) >> 2) + 1;
            const u32 tileHeight = ((lrt - ult) >> 2) + 1;
            const u32 fullLineBytes = width << byteShift;
            const u32 tileLineBytes = tileWidth << byteShift;
            const u32 startOffset = fullLineBytes * offsetY + (offsetX << byteShift);
            const u64 required = (u64)startOffset +
                (tileHeight > 0 ? (u64)(tileHeight - 1) * fullLineBytes + tileLineBytes : 0);
            if (required == 0 || required > 0xFFFFFFFFu || !StageTextureImage((u32)required)) return fail();
            emitTextureSetImg = true;
        }

        if (op >= 0x20 && op <= 0x49) return fail();

        if (emitTextureSetImg) {
            out[outIndex].w0 = mTextureImageW0;
            out[outIndex].w1 = (u32)(uintptr_t)mTextureImageStaging;
            ++outIndex;
        }

        if (version == GBI_GE && op == 0xB1) {
            u32 triW0 = w0;
            u32 triW1 = w1;
            u32 triangleCount = 0;
            while (triW1 != 0) {
                const u32 v0 = triW1 & 0xF;
                triW1 >>= 4;
                const u32 v1 = triW1 & 0xF;
                triW1 >>= 4;
                const u32 v2 = triW0 & 0xF;
                triW0 >>= 4;

                HostGfx* tri = &out[outIndex++];
                tri->w0 = 0xBF000000u;
                tri->w1 = ((v0 * 10) << 16) | ((v1 * 10) << 8) | (v2 * 10);
                ++triangleCount;
            }
            if (triangleCount == 0) return fail();
            continue;
        }

        HostGfx* dst = &out[outIndex++];
        dst->w0 = w0;
        dst->w1 = w1;

        if (op == G_RDP_SETTIMG) {
            if (mTextureImageResolved >= gRamSize) return fail();
            mTextureImageCommand = dst;

            dst->w1 = (u32)(uintptr_t)(g_pu8RamBase + mTextureImageResolved);
        } else if (op == G_RDP_SETZIMG || op == G_RDP_SETCIMG) {
            dst->w1 = Resolve(w1);
        }

        if (version == GBI_2 || version == GBI_2_S2DEX) {
            if (op == 0xDD) {
                if (n == 0) return fail();

                const u32 previousPC = pc - 8;
                const u32 previousW0 = Read32(previousPC);
                const u32 previousW1 = Read32(previousPC + 4);
                if ((u8)(previousW0 >> 24) != 0xE1) return fail();

                const u32 codeBase = Resolve(w1);
                const u32 dataBase = Resolve(previousW1);
                const u32 dataSize = (w0 & 0xFFFF) + 1;
                if (codeBase >= gRamSize || dataBase >= gRamSize || dataSize > gRamSize - dataBase) return fail();

                const UcodeInfo loadedUcode = GBIMicrocode_DetectVersion(codeBase, 0, dataBase, dataSize);
                u32 fastUcode = 0;
                if (loadedUcode.version == GBI_2) {
                    fastUcode = (u32)ucode_f3dex2;
                } else if (loadedUcode.version == GBI_2_S2DEX) {
                    fastUcode = (u32)ucode_s2dex;
                } else {
                    return fail();
                }

                dst->w0 = (0xDDu << 24) | fastUcode;
                dst->w1 = 0;
                version = loadedUcode.version;
            } else if (op == 0xDB && ((w0 >> 16) & 0xFF) == G_MW_SEGMENT) {
                const u32 offset = w0 & 0xFFFF;
                const u32 seg = (offset >> 2) & 0xF;
                mSegments[seg] = w1 & 0x00FFFFFF;
                dst->w0 = 0;
                dst->w1 = 0;
            } else if (op == 0xDE) {
                const u32 param = (w0 >> 16) & 0xFF;
                void* ptr = TranslateList(w1, version, depth + 1);
                if (!ptr) {
                    if (!mTranslationFailed) return fail();
                    return nullptr;
                }
                dst->w1 = (u32)(uintptr_t)ptr;
                if (param == G_DL_NOPUSH) break;
            } else if (version == GBI_2) {
                if (op == 0xDA) {
                    void* ptr = ConvertMatrix(w1);
                    if (!ptr) return fail();
                    dst->w1 = (u32)(uintptr_t)ptr;
                } else if (op == 0x01) {
                    const u32 count = (w0 >> 12) & 0xFF;
                    if (count == 0 || count > 32) return fail();
                    void* ptr = ConvertVertices(w1, count);
                    if (!ptr) return fail();
                    dst->w1 = (u32)(uintptr_t)ptr;
                } else if (op == 0xDC) {
                    const u32 type = w0 & 0xFE;
                    void* ptr = nullptr;
                    if (type == G_GBI2_MV_VIEWPORT) ptr = ConvertViewport(w1);
                    else if (type == G_GBI2_MV_LIGHT) ptr = ConvertLight(w1, 24);
                    else return fail();
                    if (!ptr) return fail();
                    dst->w1 = (u32)(uintptr_t)ptr;
                }
            } else if (version == GBI_2_S2DEX) {
                if (op == 0x01 || op == 0xDA) {
                    void* ptr = ConvertS2DEXSprite(w1);
                    if (!ptr) return fail();
                    dst->w1 = (u32)(uintptr_t)ptr;
                } else if (op == 0x09 || op == 0x0A) {
                    void* ptr = ConvertS2DEXBg(w1);
                    if (!ptr) return fail();
                    dst->w1 = (u32)(uintptr_t)ptr;
                }
            }
            if (op == 0xDF) break;
        } else {
            if (version == GBI_GE && op == 0xBD) {
                if ((w0 & 0xFF) == G_MW_SEGMENT) {
                    const u32 offset = (w0 >> 8) & 0xFFFF;
                    const u32 seg = (offset >> 2) & 0xF;
                    mSegments[seg] = w1 & 0x00FFFFFF;
                    dst->w0 = 0;
                    dst->w1 = 0;
                } else {
                    dst->w0 = (0xBCu << 24) | (w0 & 0x00FFFFFFu);
                }
            } else if (op == 0xAF) {
                return fail();
            } else if (op == 0xBC && (w0 & 0xFF) == G_MW_SEGMENT) {
                const u32 offset = (w0 >> 8) & 0xFFFF;
                const u32 seg = (offset >> 2) & 0xF;
                mSegments[seg] = w1 & 0x00FFFFFF;
                dst->w0 = 0;
                dst->w1 = 0;
            } else if (op == 0x01) {
                void* ptr = ConvertMatrix(w1);
                if (!ptr) return fail();
                dst->w1 = (u32)(uintptr_t)ptr;
            } else if (op == 0x04) {
                const bool gbi0Layout = version == GBI_0 || version == GBI_GE;
                const u32 count = gbi0Layout ? ((w0 >> 20) & 0xF) + 1 : (w0 >> 10) & 0x3F;
                if (count == 0 || count > 32) return fail();
                if (gbi0Layout) dst->w0 = (w0 & 0xFFFF0000u) | (count * sizeof(HostVtx));
                void* ptr = ConvertVertices(w1, count);
                if (!ptr) return fail();
                dst->w1 = (u32)(uintptr_t)ptr;
            } else if (op == 0x03) {
                const u32 type = (w0 >> 16) & 0xFF;
                void* ptr = nullptr;
                if (type == G_MV_VIEWPORT) {
                    ptr = ConvertViewport(w1);
                } else if (type == G_MV_LOOKATY || type == G_MV_LOOKATX) {
                    ptr = ConvertLight(w1, 12);
                } else if (type >= G_MV_L0 && type <= G_MV_L7 && ((type - G_MV_L0) & 1) == 0) {
                    ptr = ConvertLight(w1, 12);
                } else {
                    return fail();
                }
                if (!ptr) return fail();
                dst->w1 = (u32)(uintptr_t)ptr;
            } else if (op == 0x06) {
                void* ptr = TranslateList(w1, version, depth + 1);
                if (!ptr) {
                    if (!mTranslationFailed) return fail();
                    return nullptr;
                }
                dst->w1 = (u32)(uintptr_t)ptr;
                if (((w0 >> 16) & 0xFF) == G_DL_NOPUSH) break;
            }
            if (op == 0xB8) break;
        }
    }
    return out;
}

void* Fast3DMemoryBridge::BuildDisplayList(u32 address, GBIVersion version) {
    Reset();

    ++mBuildSerial;
    if (mBuildSerial == 0) ++mBuildSerial;

    for (u32 i = 0; i < mTextureStageCount; ++i) {
        TextureStage& stage = mTextureStages[i];
        if (stage.dirty || stage.size == 0) continue;

        const u32 firstPage = stage.address >> RDRAM_DIRTY_PAGE_SHIFT;
        const u32 lastPage = (stage.address + stage.size - 1) >> RDRAM_DIRTY_PAGE_SHIFT;
        for (u32 page = firstPage; page <= lastPage && page < RDRAM_DIRTY_PAGE_COUNT; ++page) {
            if (g_RDRAMDirtyPages[page]) {
                stage.dirty = true;
                break;
            }
        }
    }
    RDRAM_ClearDirtyPages();

    mTranslationFailed = false;
    mSawFullSync = false;
    mHaveTextureImage = false;
    mTextureImageW0 = 0;
    mTextureImageResolved = 0;
    mTextureImageCommand = nullptr;
    mTextureImageStaging = nullptr;

    GBIVersion activeVersion = version;
    void* list = TranslateList(address, activeVersion, 0);
    return mTranslationFailed ? nullptr : list;
}
