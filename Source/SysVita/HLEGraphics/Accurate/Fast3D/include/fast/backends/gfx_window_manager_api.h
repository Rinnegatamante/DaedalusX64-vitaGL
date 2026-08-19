#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "fast/vita_compat.h"
namespace Fast {
class GfxWindowBackend {
  public:
    virtual ~GfxWindowBackend() = default;
    virtual void Init(const char* gameName, const char* apiName, bool startFullScreen, uint32_t width, uint32_t height,
                      int32_t posX, int32_t posY) = 0;
    virtual void Close() = 0;
    virtual void SetKeyboardCallbacks(bool (*mOnKeyDown)(int scancode), bool (*mOnKeyUp)(int scancode),
                                      void (*mOnAllKeysUp)()) = 0;
    virtual void SetMouseCallbacks(bool (*mOnMouseButtonDown)(int btn), bool (*mOnMouseButtonUp)(int btn)) = 0;
    virtual void SetFullscreenChangedCallback(void (*mOnFullscreenChanged)(bool is_now_fullscreen)) = 0;
    virtual void SetFullscreen(bool fullscreen) = 0;
    virtual void GetActiveWindowRefreshRate(uint32_t* refreshRate) = 0;
    virtual void SetCursorVisibility(bool visability) = 0;
    virtual void SetMousePos(int32_t posX, int32_t posY) = 0;
    virtual void GetMousePos(int32_t* x, int32_t* y) = 0;
    virtual void GetMouseDelta(int32_t* x, int32_t* y) = 0;
    virtual void GetMouseWheel(float* x, float* y) = 0;
    virtual bool GetMouseState(uint32_t btn) = 0;
    virtual void SetMouseCapture(bool capture) = 0;
    virtual bool IsMouseCaptured() = 0;
    virtual void GetDimensions(uint32_t* width, uint32_t* height, int32_t* posX, int32_t* posY) = 0;
    virtual void SetDimensions(uint32_t width, uint32_t height, int32_t posX, int32_t posY) = 0;
    virtual Ship::WindowRect GetPrimaryMonitorRect() = 0;
    virtual void HandleEvents() = 0;
    virtual bool IsFrameReady() = 0;
    virtual void SwapBuffersBegin() = 0;
    virtual void SwapBuffersEnd() = 0;
    virtual double GetTime() = 0;
    virtual int GetTargetFps() = 0;
    virtual void SetTargetFps(int fps) = 0;
    virtual void SetMaxFrameLatency(int latency) = 0;
    virtual const char* GetKeyName(int scancode) = 0;
    virtual bool CanDisableVsync() = 0;
    virtual bool IsRunning() = 0;
    virtual void Destroy() = 0;
    virtual bool IsFullscreen() = 0;

  protected:
    void (*mOnFullscreenChanged)(bool isNowFullscreen);
    bool (*mOnKeyDown)(int scancode);
    bool (*mOnKeyUp)(int scancode);
    bool (*mOnMouseButtonDown)(int btn);
    bool (*mOnMouseButtonUp)(int btn);
    uint32_t mTargetFps = 60;
    bool mFullScreen;
    bool mIsRunning = true;
    bool mVsyncEnabled = true;
};
} // namespace Fast
