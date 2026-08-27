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
#include "Graphics/GraphicsContext.h"
#include "HLEGraphics/TextureCache.h"
#include "Input/InputManager.h"
#include "Interface/RomDB.h"
#include "System/System.h"
#include "Test/BatchTest.h"
#include "Utility/IO.h"
#include "Utility/Thread.h"
#include "Utility/ROMFile.h"
#include "Utility/Timer.h"
#include "SysVita/UI/Menu.h"
#include "SysVita/Input/TouchZoneMapping.h"

extern bool touch_zones_window;

static uint64_t tmr1;
bool pause_emu = false;
ButtonSce selectBtn {false, 0, 500000, SCE_CTRL_SELECT};

int update_button(ButtonSce* btn, const SceCtrlData* pad, uint32_t ticks)
{
	if ((pad->buttons & btn->btn) && !btn->down){
		btn->down = true;
		btn->downTime = ticks;
		return BUTTON_SHORT_HOLD;
	} else if (!(pad->buttons & btn->btn) && btn->down){
		btn->down = false;
		uint32_t deltaTime = ticks - btn->downTime;
		if(deltaTime > btn->longPressTime){
			return BUTTON_LONG_RELEASED;
		}
		else {
			return BUTTON_SHORT_RELEASED;
		}
	} else if ((pad->buttons & btn->btn) && btn->down){
		uint32_t deltaTime = ticks - btn->downTime;
		if(deltaTime > btn->longPressTime){
			return BUTTON_LONG_HOLD;
		}
		else {
			return BUTTON_SHORT_HOLD;
		}
	}
	else {
		return BUTTON_NEUTRAL;
	}
}

void DrawInGameMenu() {
	// Handling menubar disappear
	SceTouchData touch;
	memset(&touch, 0, sizeof(SceTouchData));
	if (!gDisableTouchMenu) {
		sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);	
	}
	uint64_t delta_touch = sceKernelGetProcessTimeWide() - tmr1;
	
	if (gDisableTouchMenu) {
		show_menubar = !gHideMenubar;
		ImGui::GetIO().MouseDrawCursor = show_menubar || touch_zones_window || pause_emu;
	} else {
		if (touch.reportNum > 0 || pause_emu) {
			ImGui::GetIO().MouseDrawCursor = true;
			show_menubar = true;
			tmr1 = sceKernelGetProcessTimeWide();
		} else if (delta_touch > 3000000) {
			show_menubar = !gHideMenubar;
			ImGui::GetIO().MouseDrawCursor = show_menubar || touch_zones_window || pause_emu;
		}
	}

	DrawInGameMenuBar();
	DrawPendingAlert();
	if (gFastForward) DrawFastForwardIcon();
	
	ImGui::Render();
	ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
	
	// Handling select button (menu pause and fast-forward)
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(0, &pad, 1);
	int statusSelectBtn = update_button(&selectBtn, &pad, sceKernelGetProcessTimeWide());
	if (statusSelectBtn == BUTTON_SHORT_RELEASED) {
		if (gDisableTouchMenu) {
			gHideMenubar = !gHideMenubar;
			show_menubar = !gHideMenubar;
			ImGui::GetIO().MouseDrawCursor = show_menubar || touch_zones_window || pause_emu;
			EnableMenuButtons(show_menubar);
		} else {
			pause_emu = !pause_emu;
			EnableMenuButtons(pause_emu);
		}
	} else if (statusSelectBtn == BUTTON_LONG_HOLD) {
		if (!gFastForward && !pause_emu) {
			gFastForward = true;
			vglWaitVblankStart(GL_FALSE);
		}
	} else if (statusSelectBtn == BUTTON_LONG_RELEASED) {
		gFastForward = false;
		vglWaitVblankStart(gUseVSync);
	}
}
