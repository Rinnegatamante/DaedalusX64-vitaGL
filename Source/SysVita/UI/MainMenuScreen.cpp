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
#include "SysVita/UI/Menu.h"

char selectedRom[512];

char *DrawRomSelector() {
	bool selected = false;
	
	vglStartRendering();
	DrawMenuBar();
		
	ImGui::SetNextWindowPos(ImVec2(0, 19), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(960, 525), ImGuiSetCond_Always);
	ImGui::Begin("", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
	
	std::string			full_path;

	IO::FindHandleT		find_handle;
	IO::FindDataT		find_data;
	
	if(IO::FindFileOpen( DAEDALUS_VITA_PATH("Roms/"), &find_handle, find_data ))
	{
		do
		{
			const char * rom_filename( find_data.Name );
			if(IsRomfilename( rom_filename ))
			{
				if (ImGui::Button(rom_filename)){
					sprintf(selectedRom, rom_filename);
					selected = true;
				}
			}
		}
		while(IO::FindFileNext( find_handle, find_data ));

		IO::FindFileClose( find_handle );
	}
	ImGui::End();
		
	glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
	ImGui::Render();
	ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
	vglStopRendering();
	
	if (selected) return selectedRom;
	return nullptr;
}