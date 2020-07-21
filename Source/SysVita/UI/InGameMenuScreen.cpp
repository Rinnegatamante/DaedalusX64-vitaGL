#include <stdlib.h>
#include <stdio.h>

#include <vitasdk.h>
#include <vitaGL.h>

#include "BuildOptions.h"
#include "Config/ConfigOptions.h"
#include "Core/Cheats.h"
#include "Core/CPU.h"
#include "Core/Memory.h"
#include "Core/PIF.h"
#include "Core/RomSettings.h"
#include "Core/Save.h"
#include "Debug/DBGConsole.h"
#include "Debug/DebugLog.h"
#include "Graphics/GraphicsContext.h"
#include "HLEGraphics/TextureCache.h"
#include "Input/InputManager.h"
#include "Interface/RomDB.h"
#include "System/Paths.h"
#include "System/System.h"
#include "Test/BatchTest.h"
#include "Utility/IO.h"
#include "Utility/Preferences.h"
#include "Utility/Profiler.h"
#include "Utility/Thread.h"
#include "Utility/ROMFile.h"
#include "Utility/Timer.h"
#include "SysVita/UI/Menu.h"

#define MENU_TIMEOUT 3000000

static uint64_t tmr1;
static uint32_t oldpad;
bool pause_emu = false;

void DrawInGameMenu() {
	DrawInGameMenuBar();
	DrawPendingAlert();
	
	ImGui::Render();
	ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
	
	// Handling menubar disappear
	uint64_t delta_pause = sceKernelGetProcessTimeWide() - tmr1;
	if (pause_emu){
		ImGui::GetIO().MouseDrawCursor = true;
		show_menubar = true;
		tmr1 = sceKernelGetProcessTimeWide();
	}else if (delta_pause > MENU_TIMEOUT){
		ImGui::GetIO().MouseDrawCursor = false;
		show_menubar = !gHideMenubar;
	}
	
	// Handling emulation pause
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(0, &pad, 1);
	if ((pad.buttons & SCE_CTRL_SELECT) && (!(oldpad & SCE_CTRL_SELECT))) {
		pause_emu = !pause_emu;
		EnableMenuButtons(pause_emu);
	}
	oldpad = pad.buttons;
	if (!pause_emu) vglStopRenderingInit();
}
