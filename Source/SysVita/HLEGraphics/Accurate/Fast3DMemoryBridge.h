#pragma once

#include "HLEGraphics/Microcode.h"
#include "Utility/DaedalusTypes.h"

namespace Fast { class Interpreter; }

class Fast3DMemoryBridge {
public:
    Fast3DMemoryBridge();
    ~Fast3DMemoryBridge();

    void SetInterpreter(Fast::Interpreter* interpreter) { mInterpreter = interpreter; }
    void* BuildDisplayList(u32 address, GBIVersion version);
    bool SawFullSync() const { return mSawFullSync; }

private:
    void Reset();
    void* Alloc(size_t size, size_t alignment = 16);
    u32 Resolve(u32 address) const;
    u32 Read32(u32 address) const;
    s16 ReadS16(u32 address) const;
    u16 ReadU16(u32 address) const;
    u8 ReadU8(u32 address) const;
    void* ConvertMatrix(u32 address);
    void* ConvertVertices(u32 address, u32 count);
    void* ConvertViewport(u32 address);
    void* ConvertLight(u32 address, u32 size);
    void* StageS2DEXImage(u32 address, u16 imageW, u16 imageH, u8 imageSiz);
    void* ConvertS2DEXBg(u32 address);
    void* ConvertS2DEXSprite(u32 address);
    u32 CountListCommands(u32 address, GBIVersion version) const;
    void* TranslateList(u32 address, GBIVersion& version, u32 depth);
    void CopyRdramBytes(u8* dst, u32 address, u32 size) const;
    void HashRdramBytes(u32 address, u32 size, u32 hash[4]) const;
    bool StageTextureImage(u32 requiredBytes, bool palette = false, u32 sourceOffset = 0,
                           u32 rowBytes = 0, u8 swizzleGroup = 0);

    struct ArenaChunk {
        u8* data;
        size_t capacity;
        size_t used;
    };

    struct TextureStage {
        u32 address;
        u32 size;
        u32 hash[4];
        u32 validatedBuild;
        u32 rowBytes;
        u8 swizzleGroup;
        bool dirty;
        u8* data;
    };

    u32 mSegments[16];
    Fast::Interpreter* mInterpreter;
    ArenaChunk* mArenaChunks;
    u32 mArenaChunkCount;
    u32 mArenaChunkCapacity;
    TextureStage* mTextureStages;
    u32 mTextureStageCount;
    u32 mTextureStageCapacity;
    u32 mBuildSerial;
    bool mTranslationFailed;
    bool mSawFullSync;
    bool mHaveTextureImage;
    u32 mTextureImageW0;
    u32 mTextureImageResolved;
    void* mTextureImageCommand;
    u8* mTextureImageStaging;
};
