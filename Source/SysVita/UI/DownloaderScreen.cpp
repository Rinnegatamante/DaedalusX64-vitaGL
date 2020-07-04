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

static char *sizes[] = {
	"B",
	"KB",
	"MB",
	"GB"
};

static float format(float len) {
	while (len > 1024) len = len / 1024.0f;
	return len;
}

static uint8_t quota(uint64_t len) {
	uint8_t ret = 0;
	while (len > 1024) {
		ret++;
		len = len / 1024;
	}
	return ret;
}

void DrawDownloaderScreen(int index, float downloaded_bytes, float total_bytes, char *text, int passes) {
	vglStartRendering();
	ImGui_ImplVitaGL_NewFrame();
	
	char msg[512];
	sprintf(msg, "%s (%ld / %ld)", text, index, passes);
	ImVec2 pos = ImGui::CalcTextSize(msg);
	
	ImGui::GetIO().MouseDrawCursor = false;
	ImGui::SetNextWindowPos(ImVec2((SCR_WIDTH / 2) - 200 * UI_SCALE, (SCR_HEIGHT / 2) - 50 * UI_SCALE), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(400 * UI_SCALE, 100 * UI_SCALE), ImGuiSetCond_Always);
	ImGui::Begin("", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
	
	ImGui::SetCursorPos(ImVec2((400 * UI_SCALE - pos.x) / 2, 20 * UI_SCALE));
	ImGui::Text(msg);
	if (total_bytes < 4000000000.0f) {
		sprintf(msg, "%.2f %s / %.2f %s", format(downloaded_bytes), sizes[quota(downloaded_bytes)], format(total_bytes), sizes[quota(total_bytes)]);
		pos = ImGui::CalcTextSize(msg);
		ImGui::SetCursorPos(ImVec2((400 * UI_SCALE - pos.x) / 2, 40 * UI_SCALE));
		ImGui::Text(msg);
		ImGui::SetCursorPos(ImVec2(100 * UI_SCALE, 60 * UI_SCALE));
		ImGui::ProgressBar(downloaded_bytes / total_bytes, ImVec2(200 * UI_SCALE, 0));
	} else {
		sprintf(msg, "%.2f %s", format(downloaded_bytes), sizes[quota(downloaded_bytes)]);
		pos = ImGui::CalcTextSize(msg);
		ImGui::SetCursorPos(ImVec2((400 * UI_SCALE - pos.x) / 2, 50 * UI_SCALE));
		ImGui::Text(msg);
	}
	
	ImGui::End();
	glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
	ImGui::Render();
	ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
	vglStopRendering();
}

void DrawDownloaderScreenCompat(float downloaded_bytes, float total_bytes, char *text) {
	vglStartRendering();
	ImGui_ImplVitaGL_NewFrame();
	
	ImVec2 pos = ImGui::CalcTextSize(text);
	
	ImGui::GetIO().MouseDrawCursor = false;
	ImGui::SetNextWindowPos(ImVec2((SCR_WIDTH / 2) - 200 * UI_SCALE, (SCR_HEIGHT / 2) - 50 * UI_SCALE), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(400 * UI_SCALE, 100 * UI_SCALE), ImGuiSetCond_Always);
	ImGui::Begin("", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
	
	ImGui::SetCursorPos(ImVec2((400 * UI_SCALE - pos.x) / 2, 20 * UI_SCALE));
	ImGui::Text(text);
	
	char msg[512];
	sprintf(msg, "%.2f %s / %.2f %s", format(downloaded_bytes), sizes[quota(downloaded_bytes)], format(total_bytes), sizes[quota(total_bytes)]);
	pos = ImGui::CalcTextSize(msg);
	ImGui::SetCursorPos(ImVec2((400 * UI_SCALE - pos.x) / 2, 40 * UI_SCALE));
	ImGui::Text(msg);
	ImGui::SetCursorPos(ImVec2(100 * UI_SCALE, 60 * UI_SCALE));
	ImGui::ProgressBar(downloaded_bytes / total_bytes, ImVec2(200 * UI_SCALE, 0));
	
	ImGui::End();
	glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
	ImGui::Render();
	ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
	vglStopRendering();
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
}

void DrawExtractorScreen(int index, float file_extracted_bytes, float extracted_bytes, float file_total_bytes, float total_bytes, char *filename, int num_files) {
	vglStartRendering();
	ImGui_ImplVitaGL_NewFrame();
	
	char msg1[256], msg2[256];
	sprintf(msg1, "%s (%ld / %ld)", lang_strings[STR_EXTRACTING], index, num_files);
	sprintf(msg2, "%s (%.2f %s / %.2f %s)", filename, format(file_extracted_bytes), sizes[quota(file_extracted_bytes)], format(file_total_bytes), sizes[quota(file_total_bytes)]);
	ImVec2 pos1 = ImGui::CalcTextSize(msg1);
	ImVec2 pos2 = ImGui::CalcTextSize(msg2);
	
	ImGui::GetIO().MouseDrawCursor = false;
	ImGui::SetNextWindowPos(ImVec2((SCR_WIDTH / 2) - 200 * UI_SCALE, (SCR_HEIGHT / 2) - 50 * UI_SCALE), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(400 * UI_SCALE, 100 * UI_SCALE), ImGuiSetCond_Always);
	ImGui::Begin("", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
	ImGui::SetCursorPos(ImVec2((400 * UI_SCALE - pos1.x) / 2, 20 * UI_SCALE));
	ImGui::Text(msg1);
	ImGui::SetCursorPos(ImVec2((400 * UI_SCALE - pos2.x) / 2, 40 * UI_SCALE));
	ImGui::Text(msg2);
	ImGui::SetCursorPos(ImVec2(100 * UI_SCALE, 60 * UI_SCALE));
	ImGui::ProgressBar(extracted_bytes / total_bytes, ImVec2(200 * UI_SCALE, 0));
	
	ImGui::End();
	glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
	ImGui::Render();
	ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
	vglStopRendering();
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
}
