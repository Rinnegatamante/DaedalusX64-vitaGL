#include <stdlib.h>
#include <stdio.h>

#include <vitasdk.h>
#include <vitaGL.h>
#include <imgui_vita.h>

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
#include "Utility/Translate.h"
#include "Utility/ROMFile.h"
#include "Utility/Timer.h"
#include "SysVita/UI/Menu.h"

void DrawDownloaderScreen(int index, float downloaded_bytes, float total_bytes) {
	vglStartRendering();
	ImGui_ImplVitaGL_NewFrame();
	ImGui::GetIO().MouseDrawCursor = false;
	ImGui::SetNextWindowPos(ImVec2((SCR_WIDTH / 2) - 200, (SCR_HEIGHT / 2) - 50), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(400, 100), ImGuiSetCond_Always);
	ImGui::Begin("", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
	
	char msg[512];
	sprintf(msg, "Downloading compatibility list database (%ld / %ld)", index, NUM_DB_CHUNKS);
	ImVec2 pos = ImGui::CalcTextSize(msg);
	ImGui::SetCursorPos(ImVec2((400 - pos.x) / 2, 20));
	ImGui::Text(msg);
	
	if (total_bytes < 4000000000.0f) {
		sprintf(msg, "%.2f KBs / %.2f KBs", downloaded_bytes / 1024, total_bytes / 1024);
		pos = ImGui::CalcTextSize(msg);
		ImGui::SetCursorPos(ImVec2((400 - pos.x) / 2, 40));
		ImGui::Text(msg);
		ImGui::SetCursorPos(ImVec2(100, 60));
		ImGui::ProgressBar(downloaded_bytes / total_bytes, ImVec2(200, 0));
	} else {
		sprintf(msg, "%.2f KBs", downloaded_bytes / 1024);
		pos = ImGui::CalcTextSize(msg);
		ImGui::SetCursorPos(ImVec2((400 - pos.x) / 2, 50));
		ImGui::Text(msg);
	}
	
	ImGui::End();
	glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
	ImGui::Render();
	ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
	vglStopRendering();
}
