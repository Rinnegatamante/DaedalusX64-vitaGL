#include "stdafx.h"
#include "SysVita/HLEGraphics/Accurate/Fast3DCompat.h"
#include "Graphics/GraphicsContext.h"

#include <memory>
#include <string>

#include <vitaGL.h>
#include <psp2/kernel/processmgr.h>

extern int gPostProcessing;

namespace {

class DaedalusFastWindow final : public Fast::GfxWindowBackend {
public:
    void Init(const char*, const char*, bool fullscreen, uint32_t width, uint32_t height, int32_t x, int32_t y) override {
        mWidth = width;
        mHeight = height;
        mX = x;
        mY = y;
        mFullScreen = fullscreen;
        mIsRunning = true;
    }

    void Close() override {}
    void SetKeyboardCallbacks(bool (*down)(int), bool (*up)(int), void (*)(void)) override {
        mOnKeyDown = down;
        mOnKeyUp = up;
    }
    void SetMouseCallbacks(bool (*down)(int), bool (*up)(int)) override {
        mOnMouseButtonDown = down;
        mOnMouseButtonUp = up;
    }
    void SetFullscreenChangedCallback(void (*cb)(bool)) override { mOnFullscreenChanged = cb; }
    void SetFullscreen(bool value) override {
        mFullScreen = value;
        if (mOnFullscreenChanged) mOnFullscreenChanged(value);
    }
    void GetActiveWindowRefreshRate(uint32_t* rate) override { if (rate) *rate = 60; }
    void SetCursorVisibility(bool) override {}
    void SetMousePos(int32_t, int32_t) override {}
    void GetMousePos(int32_t* x, int32_t* y) override { if (x) *x = 0; if (y) *y = 0; }
    void GetMouseDelta(int32_t* x, int32_t* y) override { if (x) *x = 0; if (y) *y = 0; }
    void GetMouseWheel(float* x, float* y) override { if (x) *x = 0.0f; if (y) *y = 0.0f; }
    bool GetMouseState(uint32_t) override { return false; }
    void SetMouseCapture(bool) override {}
    bool IsMouseCaptured() override { return false; }
    void GetDimensions(uint32_t* width, uint32_t* height, int32_t* x, int32_t* y) override {
        if (width) *width = mWidth;
        if (height) *height = mHeight;
        if (x) *x = mX;
        if (y) *y = mY;
    }
    void SetDimensions(uint32_t width, uint32_t height, int32_t x, int32_t y) override {
        mWidth = width;
        mHeight = height;
        mX = x;
        mY = y;
    }
    Ship::WindowRect GetPrimaryMonitorRect() override { return { 0, 0, static_cast<int32_t>(mWidth), static_cast<int32_t>(mHeight) }; }
    void HandleEvents() override {}
    bool IsFrameReady() override { return true; }
    void SwapBuffersBegin() override {}
    void SwapBuffersEnd() override {}
    double GetTime() override { return static_cast<double>(sceKernelGetProcessTimeWide()) / 1000000.0; }
    int GetTargetFps() override { return static_cast<int>(mTargetFps); }
    void SetTargetFps(int fps) override { mTargetFps = fps > 0 ? static_cast<uint32_t>(fps) : 60; }
    void SetMaxFrameLatency(int) override {}
    const char* GetKeyName(int) override { return ""; }
    bool CanDisableVsync() override { return false; }
    bool IsRunning() override { return mIsRunning; }
    void Destroy() override { mIsRunning = false; }
    bool IsFullscreen() override { return mFullScreen; }

private:
    uint32_t mWidth = 960;
    uint32_t mHeight = 544;
    int32_t mX = 0;
    int32_t mY = 0;
};

DaedalusFastWindow gWindow;
Fast::GfxRenderingAPIOGL gRapi;
bool gSuppressNextTaskClear = false;

}

Fast::GfxWindowBackend* DaedalusFast3D_GetWindowBackend() {
    return &gWindow;
}

Fast::GfxRenderingAPI* DaedalusFast3D_GetRenderingAPI() {
    return &gRapi;
}

extern "C" bool DaedalusFast3D_ShouldSuppressFramebufferClear(bool color, bool depth) {
    if (color && depth && gSuppressNextTaskClear) {
        gSuppressNextTaskClear = false;
        return true;
    }
    if (color && depth) gSuppressNextTaskClear = false;
    return false;
}

extern "C" uint32_t DaedalusFast3D_GetFramebufferOverride(int fbId) {
    if (fbId != 0 || !gPostProcessing) return 0;
    return GraphicsContextVita_GetPostProcessFramebuffer();
}

void DaedalusFast3D_InvalidateExternalGLState(Fast::Interpreter* interpreter) {
    if (!interpreter) return;
    interpreter->mRenderingState.mShaderProgram = nullptr;
    for (int i = 0; i < SHADER_MAX_TEXTURES; ++i) interpreter->mRenderingState.mTextures[i] = nullptr;
    gRapi.InvalidateExternalState();
}

void DaedalusFast3D_PrepareTask(bool firstTaskInFrame) {
    gSuppressNextTaskClear = !firstTaskInFrame;
}

void DaedalusFast3D_RestoreGLState() {
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glClientActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    for (GLuint i = 0; i < 8; ++i) glDisableVertexAttribArray(i);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glViewport(0, 0, 960, 544);
    glScissor(0, 0, 960, 544);
    gRapi.InvalidateExternalState();
}

namespace Ship {

static std::shared_ptr<Context> sContext = std::make_shared<Context>();
static std::shared_ptr<ConsoleVariable> sCvars = std::make_shared<ConsoleVariable>();
static std::shared_ptr<ResourceManager> sResourceManager = std::make_shared<ResourceManager>();
static std::shared_ptr<ArchiveManager> sArchiveManager = std::make_shared<ArchiveManager>();

std::shared_ptr<ResourceInitData> IResource::GetInitData() { return mInitData; }
const std::string* ArchiveManager::HashToString(uint64_t) const { return nullptr; }
std::shared_ptr<ArchiveManager> ResourceManager::GetArchiveManager() { return sArchiveManager; }
void* ResourceManager::GetResourceRawPointer(const char*) { return nullptr; }
void* ResourceManager::GetResourceRawPointer(uint64_t) { return nullptr; }
std::shared_ptr<IResource> ResourceManager::LoadResourceProcess(const std::string&, bool, std::shared_ptr<ResourceInitData>, uint64_t) { return {}; }
std::shared_ptr<IResource> ResourceManager::LoadResourceProcessFast(const char*) { return {}; }
bool ResourceManager::OtrSignatureCheck(const char*) { return false; }
float ConsoleVariable::GetFloat(const char*, float fallback) { return fallback; }
int ConsoleVariable::GetInteger(const char*, int fallback) { return fallback; }
std::shared_ptr<Context> Context::GetInstance() { return sContext; }
std::shared_ptr<ConsoleVariable> Context::GetConsoleVariables() const { return sCvars; }
std::shared_ptr<ResourceManager> Context::GetResourceManager() const { return sResourceManager; }

}

void DaedalusFast3D_SetDisplayConfiguration(Fast::Interpreter* interpreter, int aspectRatio) {
    u32 width = SCR_WIDTH;
    u32 height = SCR_HEIGHT;
    CGraphicsContext::Get()->GetScreenSize(&width, &height);
    const int32_t x = static_cast<int32_t>((SCR_WIDTH - width) / 2);
    const int32_t y = static_cast<int32_t>((SCR_HEIGHT - height) / 2);
    const float displayAspect = static_cast<float>(width) / static_cast<float>(height);
    const float aspectDelta = aspectRatio == RATIO_16_9 ? 1.0f : (4.0f / 3.0f) / displayAspect;
    interpreter->SetDisplayConfiguration(width, height, x, y, aspectDelta);
    gWindow.SetDimensions(SCR_WIDTH, SCR_HEIGHT, 0, 0);
}
