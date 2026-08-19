#include "stdafx.h"

#include "SysVita/HLEGraphics/Accurate/RendererAccurate.h"
#include "SysVita/HLEGraphics/Accurate/Fast3DABI.h"
#include "SysVita/HLEGraphics/Accurate/Fast3DCompat.h"
#include "SysVita/HLEGraphics/Accurate/Fast3DMemoryBridge.h"

#include "Core/CPU.h"
#include "Core/Memory.h"
#include "HLEGraphics/Microcode.h"
#include "Graphics/GraphicsContext.h"
#include "OSHLE/ultra_rcp.h"
#include "OSHLE/ultra_sptask.h"
#include "Plugins/GraphicsPlugin.h"

#include <memory>
#include <new>
#include <psp2/io/fcntl.h>

extern u32 gRDPFrame;
extern bool gCPURendering;

namespace {

class AccurateRenderer {
public:
    bool Initialise() {
        if (mInitialised) return true;

        sceIoMkdir("ux0:data/DaedalusX64/ShaderCache_F3D", 0777);

        mInterpreter = std::make_shared<Fast::Interpreter>();
        Fast::GfxSetInstance(mInterpreter);

        GraphicsContextVita_GetPostProcessFramebuffer();

        mInterpreter->Init(
            DaedalusFast3D_GetWindowBackend(),
            DaedalusFast3D_GetRenderingAPI(),
            "DaedalusX64 Accurate",
            false,
            960,
            544,
            0,
            0);

        DaedalusFast3D_SetDisplayConfiguration(mInterpreter.get(), gAspectRatio);
        mLastAspectRatio = gAspectRatio;
        mBridge.SetInterpreter(mInterpreter.get());
        mInitialised = true;
        return true;
    }

    void Destroy() {
        if (!mInitialised) return;

        mFrameStarted = false;
        mInterpreter->Destroy();
        DaedalusFast3D_RestoreGLState();
        Fast::GfxSetInstance(std::shared_ptr<Fast::Interpreter>());
        mInterpreter.reset();
        mInitialised = false;
    }

    bool ProcessTask() {
        if (!mInitialised || !mInterpreter) return false;

        OSTask* task = reinterpret_cast<OSTask*>(g_pu8SpMemBase + 0x0FC0);
        const u32 codeBase = static_cast<u32>(reinterpret_cast<uintptr_t>(task->t.ucode)) & 0x1FFFFFFF;
        const u32 codeSize = task->t.ucode_size;
        const u32 dataBase = static_cast<u32>(reinterpret_cast<uintptr_t>(task->t.ucode_data)) & 0x1FFFFFFF;
        const u32 dataSize = task->t.ucode_data_size;

        const UcodeInfo ucode = GBIMicrocode_DetectVersion(codeBase, codeSize, dataBase, dataSize);
        const int fastUcode = MapUcode(ucode.version);
        if (fastUcode < 0) return false;

        if (gGraphicsPlugin) gGraphicsPlugin->UpdateScreen();
        const bool firstTaskInFrame = !mFrameStarted;

        const u32 dlist = static_cast<u32>(reinterpret_cast<uintptr_t>(task->t.data_ptr));
        void* translated = mBridge.BuildDisplayList(dlist, ucode.version);
        if (!translated) return false;

        Fast::gfx_set_target_ucode(static_cast<UcodeHandlers>(fastUcode));

        if (mLastAspectRatio != gAspectRatio) {
            DaedalusFast3D_SetDisplayConfiguration(mInterpreter.get(), gAspectRatio);
            mLastAspectRatio = gAspectRatio;
        }

        if (firstTaskInFrame) {
            mInterpreter->StartFrame();
            mFrameStarted = true;
        }

        DaedalusFast3D_PrepareTask(firstTaskInFrame);
        mInterpreter->Run(reinterpret_cast<Gfx*>(translated), mDummyMatrixReplacements);

        if (mBridge.SawFullSync()) {
            Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_DP);
            gCPUState.AddJob(CPU_CHECK_INTERRUPTS);
        }

        ++gRDPFrame;
        gCPURendering = false;
        return true;
    }

    void FinishFrame() {
        if (!mInitialised || !mInterpreter || !mFrameStarted) return;
        mInterpreter->EndFrame();
        mFrameStarted = false;
    }

    void InvalidateExternalGLState() {
        if (mInitialised && mInterpreter) {
            DaedalusFast3D_InvalidateExternalGLState(mInterpreter.get());
        }
    }

private:
    static int MapUcode(GBIVersion version) {
        switch (version) {
        case GBI_0: return 1;
        case GBI_1: return 2;
        case GBI_2: return 4;
        case GBI_2_S2DEX: return 5;
        default: return -1;
        }
    }

    bool mInitialised = false;
    bool mFrameStarted = false;
    int mLastAspectRatio = -1;
    std::shared_ptr<Fast::Interpreter> mInterpreter;
    Fast3DMemoryBridge mBridge;
    robin_hood::unordered_map<Mtx*, MtxF> mDummyMatrixReplacements;
};

AccurateRenderer* gAccurateRenderer = nullptr;

}

bool CreateRendererAccurate() {
    if (gAccurateRenderer) return true;
    gAccurateRenderer = new (std::nothrow) AccurateRenderer();
    if (!gAccurateRenderer) return false;
    if (!gAccurateRenderer->Initialise()) {
        delete gAccurateRenderer;
        gAccurateRenderer = nullptr;
        return false;
    }
    return true;
}

void DestroyRendererAccurate() {
    if (!gAccurateRenderer) return;
    gAccurateRenderer->Destroy();
    delete gAccurateRenderer;
    gAccurateRenderer = nullptr;
}

bool ProcessDListAccurate() {
    return gAccurateRenderer && gAccurateRenderer->ProcessTask();
}

void InvalidateRendererAccurateExternalState() {
    if (gAccurateRenderer) gAccurateRenderer->InvalidateExternalGLState();
}

void FinishRendererAccurateFrame() {
    if (gAccurateRenderer) gAccurateRenderer->FinishFrame();
}
