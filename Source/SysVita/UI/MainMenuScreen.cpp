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

struct CompatibilityList {
	char name[128];
	bool playable;
	bool ingame_plus;
	bool ingame_low;
	bool crash;
	bool slow;
	CompatibilityList *next;
};

struct RomSelection {
	char name[128];
	RomSettings settings;
	RomID id;
	u32 size;
	ECicType cic;
	CompatibilityList *status;
	RomSelection *next;
};

static RomSelection *list = nullptr;
static CompatibilityList *comp = nullptr;
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

// TODO: Use a proper json lib for better performances and safety
void AppendCompatibilityDatabase(const char *file) {
	FILE *f = fopen(file, "rb");
	if (f) {
		fseek(f, 0, SEEK_END);
		uint64_t len = ftell(f);
		fseek(f, 0, SEEK_SET);
		char *buffer = (char*)malloc(len + 1);
		fread(buffer, 1, len, f);
		buffer[len] = 0;
		char *ptr = buffer;
		char *end;
		do {
			ptr = strstr(ptr, "\"title\":");
			if (ptr) {
				CompatibilityList *node = (CompatibilityList*)malloc(sizeof(CompatibilityList));
				
				// Extracting title
				ptr += 10;
				end = strstr(ptr, "\"");
				memcpy(node->name, ptr, end - ptr);
				node->name[end - ptr] = 0;
				
				// Extracting tags
				bool perform_slow_check = true;
				ptr += 1000; // Let's skip some data to improve performances
				ptr = strstr(ptr, "\"labels\":");
				ptr = strstr(ptr + 150, "\"name\":");
				ptr += 9;
				if (ptr[0] == 'P') {
					node->playable = true;
					node->ingame_low = false;
					node->ingame_plus = false;
					node->crash = false;
				} else if (ptr[0] == 'C') {
					node->playable = false;
					node->ingame_low = false;
					node->ingame_plus = false;
					node->slow = false;
					node->crash = true;
					perform_slow_check = false;
				} else {
					node->playable = false;
					node->crash = false;
					end = strstr(ptr, "\"");
					if ((end - ptr) == 13) {
						node->ingame_plus = true;
						node->ingame_low = false;
					}else {
						node->ingame_low = true;
						node->ingame_plus = false;
					}
				}
				ptr += 120; // Let's skip some data to improve performances
				if (perform_slow_check) {
					end = ptr;
					ptr = strstr(ptr, "]");
					if ((ptr - end) > 200) node->slow = true;
					else node->slow = false;
				}
				
				ptr += 350; // Let's skip some data to improve performances
				node->next = comp;
				comp = node;
			}
		} while (ptr);
		fclose(f);
		free(buffer);
	}
}

CompatibilityList *SearchForCompatibilityData(const char *name) {
	CompatibilityList *node = comp;
	char tmp[128], *p;
	if (p = strstr(name, " (")) {
		memcpy(tmp, name, p - name);
		tmp[p - name] = 0;
	} else sprintf(tmp, name);
	while (node) {
		if (strcasecmp(node->name, tmp) == 0) return node;
		node = node->next;
	}
	return nullptr;
}

void SetTagDescription(const char *text) {
	ImGui::SameLine();
	ImGui::Text(": %s", text);
}

char *DrawRomSelector() {
	bool selected = false;
	
	vglStartRendering();
	DrawMenuBar();
		
	if (!list) {
		if (!comp) {
			AppendCompatibilityDatabase("ux0:data/DaedalusX64/db1.json");
			AppendCompatibilityDatabase("ux0:data/DaedalusX64/db2.json");
		}
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
						node->status = SearchForCompatibilityData(node->settings.GameName.c_str());
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
		ImGui::Text("Region: %s", ROM_GetCountryNameFromID(hovered->id.CountryID));
		if (hovered->cic == CIC_UNKNOWN) ImGui::Text("Cic Type: Unknown");
		else ImGui::Text("Cic Type: %ld", (s32)hovered->cic + 6101);
		ImGui::Text("ROM Size: %lu MBs", hovered->size);
		ImGui::Text("Save Type: %s", ROM_GetSaveTypeName(hovered->settings.SaveType));
		ImGui::Text("Expansion Pak: %s", ROM_GetExpansionPakUsageName(hovered->settings.ExpansionPakUsage));
		if (hovered->status) {
			ImGui::Text(" ");
			ImGui::Text("Tags:");
			if (hovered->status->playable) {
				ImGui::TextColored(ImVec4(0, 0.75f, 0, 1.0f), "Playable");
				SetTagDescription("Games that can be played from start to\nfinish with playable performance.");
			}
			if (hovered->status->ingame_plus) {
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0, 1.0f), "Ingame +");
				SetTagDescription("Games that go far ingame but have glitches\nor have non-playable performance.");
			}
			if (hovered->status->ingame_low) {
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.25f, 1.0f), "Ingame -");
				SetTagDescription("Games that go ingame but have major issues\nthat prevents it from going further early on.");
			}
			if (hovered->status->crash) {
				ImGui::TextColored(ImVec4(1.0f, 0, 0, 1.0f), "Crash");
				SetTagDescription("Games that crash before reaching ingame.");
			}
			if (hovered->status->slow) {
				ImGui::TextColored(ImVec4(0.5f, 0, 1.0f, 1.0f), "Slow");
				SetTagDescription("Game is playable but still not fullspeed.");
			}
		}
	}
	
	ImGui::End();
	
	glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
	ImGui::Render();
	ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
	vglStopRendering();
	
	if (selected) {
		// NOTE: Uncomment this to make rom list to be re-built every time
		/*p = list;
		while (p) {
			RomSelection *old = p;
			p = p->next;
			free(old);
		}
		list = nullptr;*/
		return selectedRom;
	}
	return nullptr;
}