#pragma once

#include "SysVita/HLEGraphics/Accurate/Fast3DABI.h"

Fast::GfxWindowBackend* DaedalusFast3D_GetWindowBackend();
Fast::GfxRenderingAPI* DaedalusFast3D_GetRenderingAPI();
void DaedalusFast3D_SetDisplayConfiguration(Fast::Interpreter* interpreter, int aspectRatio);
void DaedalusFast3D_RestoreGLState();
void DaedalusFast3D_InvalidateExternalGLState(Fast::Interpreter* interpreter);
void DaedalusFast3D_PrepareTask(bool firstTaskInFrame);
