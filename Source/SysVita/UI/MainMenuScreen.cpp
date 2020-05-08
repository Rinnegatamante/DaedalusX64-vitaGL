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

// FIXME: PNG loading function in CNativeTexture is wrong, using stb is just a workaround
#define STB_IMAGE_IMPLEMENTATION
#include "SysVita/UI/stb_image.h"

char selectedRom[512];

struct RomSelection {
	char name[128];
	RomSettings settings;
	RomID id;
	u32 size;
	ECicType cic;
	RomSelection *next;
};

static RomSelection *list = nullptr;
static RomSelection *old_hovered = nullptr;
static bool has_preview_icon = false;
CRefPtr<CNativeTexture> mpPreviewTexture;
GLuint preview_icon = 0;

bool LoadPreview(RomSelection *rom) {
	if (old_hovered == rom) return has_preview_icon;
	old_hovered = rom;
	
	int w, h;
	IO::Filename preview_filename;
	IO::Path::Combine(preview_filename, DAEDALUS_VITA_PATH("Resources/Preview/"), rom->settings.Preview.c_str() );
	uint8_t *icon_data = stbi_load(preview_filename, &w, &h, NULL, 4);
	if (icon_data) {
		if (!preview_icon) glGenTextures(1, &preview_icon);
		glBindTexture(GL_TEXTURE_2D, preview_icon);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, icon_data);
		free(icon_data);
		return true;
	}
	
	return false;
}

char *DrawRomSelector() {
	bool selected = false;
	
	vglStartRendering();
	DrawMenuBar();
		
	if (!list) {
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
					std::string full_path = DAEDALUS_VITA_PATH("Roms/");
					full_path += rom_filename;
					RomSelection *node = (RomSelection*)malloc(sizeof(RomSelection));
					sprintf(node->name, rom_filename);
					if (ROM_GetRomDetailsByFilename(full_path.c_str(), &node->id, &node->size, &node->cic)) {
						node->size = node->size / (1024 * 1024);
						if (!CRomSettingsDB::Get()->GetSettings(node->id, &node->settings )) {
							node->settings.Reset();
							node->settings.Comment = "Unknown";
							std::string game_name;
							if (!ROM_GetRomName(full_path.c_str(), game_name )) game_name = full_path;
							game_name = game_name.substr(0, 63);
							node->settings.GameName = game_name.c_str();
							CRomSettingsDB::Get()->SetSettings(node->id, node->settings);
						}
					} else node->settings.GameName = "Unknown";
					node->next = list;
					list = node;
				}
			}
			while(IO::FindFileNext( find_handle, find_data ));

			IO::FindFileClose( find_handle );
		}
	}
	
	ImGui::SetNextWindowPos(ImVec2(0, 19), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(SCR_WIDTH - 400, SCR_HEIGHT - 19), ImGuiSetCond_Always);
	ImGui::Begin("Selector Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
	
	RomSelection *hovered = nullptr;
	RomSelection *p = list;
	while (p) {
		if (ImGui::Button(p->name)){
			sprintf(selectedRom, p->name);
			selected = true;
		}
		if (ImGui::IsItemHovered()) hovered = p;
		p = p->next;
	}
	
	ImGui::End();
	
	ImGui::SetNextWindowPos(ImVec2(SCR_WIDTH - 400, 19), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(400, SCR_HEIGHT - 19), ImGuiSetCond_Always);
	ImGui::Begin("Info Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
	
	if (hovered) {
		if (has_preview_icon = LoadPreview(hovered)) {
			ImGui::Image((void*)preview_icon, ImVec2(387, 268));
		}
		ImGui::Text("Game Name: %s", hovered->settings.GameName.c_str());
		if (hovered->cic == CIC_UNKNOWN) ImGui::Text("Cic Type: Unknown");
		else ImGui::Text("Cic Type: %ld", (s32)hovered->cic + 6101);
		ImGui::Text("ROM Size: %lu MBs", hovered->size);
		ImGui::Text("Save Type: %s", ROM_GetSaveTypeName(hovered->settings.SaveType));
		ImGui::Text("Expansion Pak: %s", ROM_GetExpansionPakUsageName(hovered->settings.ExpansionPakUsage));
	}
	
	ImGui::End();
	
	glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
	ImGui::Render();
	ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
	vglStopRendering();
	
	if (selected) {
		p = list;
		while (p) {
			RomSelection *old = p;
			p = p->next;
			free(old);
		}
		list = nullptr;
		return selectedRom;
	}
	return nullptr;
}