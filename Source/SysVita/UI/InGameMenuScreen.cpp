#include <stdlib.h>
#include <stdio.h>

#include <vitasdk.h>
#include <vitaGL.h>
#include <imgui_vita.h>

#include "BuildOptions.h"
#include "Config/ConfigOptions.h"
#include "Core/Cheats.h"
#include "Core/CPU.h"
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
#include "Utility/Translate.h"
#include "Utility/ROMFile.h"
#include "Utility/Timer.h"

extern EFrameskipValue			gFrameskipValue;

static bool show_menubar = true;
static uint64_t tmr1;
bool hide_menubar = true;

void DrawInGameMenu() {
	ImGui_ImplVitaGL_NewFrame();
	
	if (show_menubar) {
		if (ImGui::BeginMainMenuBar()){
			if (ImGui::BeginMenu("Emulation")){
				if (ImGui::BeginMenu("Frameskip")){
					if (ImGui::MenuItem("Disabled", nullptr, gFrameskipValue == FV_DISABLED)){
						gFrameskipValue = FV_DISABLED;
					}
					if (ImGui::MenuItem("Auto 1", nullptr, gFrameskipValue == FV_AUTO1)){
						gFrameskipValue = FV_AUTO1;
					}
					if (ImGui::MenuItem("Auto 2", nullptr, gFrameskipValue == FV_AUTO2)){
						gFrameskipValue = FV_AUTO2;
					}
					if (ImGui::MenuItem("One Frame", nullptr, gFrameskipValue == FV_1)){
						gFrameskipValue = FV_1;
					}
					if (ImGui::MenuItem("Two Frames", nullptr, gFrameskipValue == FV_2)){
						gFrameskipValue = FV_2;
					}
					if (ImGui::MenuItem("Three Frames", nullptr, gFrameskipValue == FV_3)){
						gFrameskipValue = FV_3;
					}
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Graphics")){
				if (ImGui::MenuItem("Bilinear Filter", nullptr, gGlobalPreferences.ForceLinearFilter)){
					gGlobalPreferences.ForceLinearFilter = !gGlobalPreferences.ForceLinearFilter;
				}
				ImGui::EndMenu();
			}
			ImGui::SameLine();
			ImGui::SetCursorPosX(870);
		
			ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate); 
			ImGui::EndMainMenuBar();
		}
	}
	
	ImGui::Render();
	ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
	
	SceTouchData touch;
	sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
	uint64_t delta_touch = sceKernelGetProcessTimeWide() - tmr1;
	if (touch.reportNum > 0){
		ImGui::GetIO().MouseDrawCursor = true;
		show_menubar = true;
		tmr1 = sceKernelGetProcessTimeWide();
	}else if (delta_touch > 3000000){
		ImGui::GetIO().MouseDrawCursor = false;
		show_menubar = !hide_menubar;
	}
}