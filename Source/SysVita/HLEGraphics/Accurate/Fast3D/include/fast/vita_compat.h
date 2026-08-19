#pragma once

#include <stdint.h>
#include <memory>
#include <string>

typedef int16_t s16;
typedef int32_t s32;
#define BE16SWAP(x) static_cast<uint16_t>((static_cast<uint16_t>(x) >> 8) | (static_cast<uint16_t>(x) << 8))

namespace Ship {

struct WindowRect {
    int32_t Left;
    int32_t Top;
    int32_t Right;
    int32_t Bottom;
};

struct ResourceInitData {
    std::string Path;
};

class IResource {
public:
    virtual ~IResource() = default;
    static const std::string gAltAssetPrefix;
    std::shared_ptr<ResourceInitData> GetInitData();

protected:
    std::shared_ptr<ResourceInitData> mInitData;
};

class ArchiveManager {
public:
    const std::string* HashToString(uint64_t hash) const;
};

class ResourceManager {
public:
    std::shared_ptr<ArchiveManager> GetArchiveManager();
    void* GetResourceRawPointer(const char* name);
    void* GetResourceRawPointer(uint64_t hash);
    std::shared_ptr<IResource> LoadResourceProcess(const std::string& path, bool loadExact = false,
                                                   std::shared_ptr<ResourceInitData> initData = nullptr,
                                                   uint64_t hash = 0);
    std::shared_ptr<IResource> LoadResourceProcessFast(const char* path);
    bool OtrSignatureCheck(const char* path);
};

class ConsoleVariable {
public:
    float GetFloat(const char* name, float fallback);
    int GetInteger(const char* name, int fallback);
};

class Context {
public:
    static std::shared_ptr<Context> GetInstance();
    std::shared_ptr<ConsoleVariable> GetConsoleVariables() const;
    std::shared_ptr<ResourceManager> GetResourceManager() const;
};

}


