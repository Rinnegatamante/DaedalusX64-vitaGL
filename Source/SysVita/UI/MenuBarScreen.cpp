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

#define MAX_SAVESLOT 9

enum {
	DARK_THEME,
	LIGHT_THEME,
	CLASSIC_THEME
};

static uint8_t imgui_theme = DARK_THEME;

int aspect_ratio = RATIO_16_9;

static bool cached_saveslots[MAX_SAVESLOT + 1];
static bool has_cached_saveslots = false;

extern bool wait_rendering;
extern bool has_rumblepak[4];
extern char cur_ucode[256];

bool show_menubar = true;
bool hide_menubar = true;
bool run_emu = true;
bool restart_rom = false;

static bool vflux_window = false;
static bool vflux_enabled = false;
static bool credits_window = false;
static bool debug_window = false;
static bool logs_window = false;

extern EFrameskipValue gFrameskipValue;
extern bool kUpdateTexturesEveryFrame;

static float vcolors[3];

static float *colors;
static float *vertices;

char dbg_lines[MAX_DEBUG_LINES][256];
int cur_dbg_line = 0;

void SetupVFlux() {
	colors = (float*)malloc(sizeof(float)*4*4);
	vertices = (float*)malloc(sizeof(float)*3*4);
	vertices[0] =   0.0f;
	vertices[1] =   0.0f;
	vertices[2] =   0.0f;
	vertices[3] = 960.0f;
	vertices[4] =   0.0f;
	vertices[5] =   0.0f;
	vertices[6] = 960.0f;
	vertices[7] = 544.0f;
	vertices[8] =   0.0f;
	vertices[9] =   0.0f;
	vertices[10]= 544.0f;
	vertices[11]=   0.0f;
}

void SetDescription(const char *text) {
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(text);
}

void DrawCommonMenuBar() {
	SceCtrlPortInfo pinfo;
	sceCtrlGetControllerPortInfo(&pinfo);
	if (ImGui::BeginMenu("Emulation")){
		if (ImGui::MenuItem("DynaRec", nullptr, gDynarecEnabled)){
			gDynarecEnabled = !gDynarecEnabled;
		}
		SetDescription("Enables dynamic recompilation for better performances.");
		ImGui::Separator();
		if (ImGui::MenuItem("Frame Limit", nullptr, gSpeedSyncEnabled)){
			gSpeedSyncEnabled = !gSpeedSyncEnabled;
		}
		SetDescription("Limits framerate to the one running game is supposed to have.");
		/* NOTE: Due to multiple gxm scenes usage, this affects renderer causing major glitches.
		   Also, in every single game this lowers performances. Not worth to have it as an option. */
		/*if (ImGui::BeginMenu("Frameskip")){
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
		}*/
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Graphics")){
		if (ImGui::BeginMenu("Aspect Ratio")){
			if (ImGui::MenuItem("16:9", nullptr, aspect_ratio == RATIO_16_9)){
				aspect_ratio = RATIO_16_9;
			}
			if (ImGui::MenuItem("4:3", nullptr, aspect_ratio == RATIO_4_3)){
				aspect_ratio = RATIO_4_3;
			}
			if (ImGui::MenuItem("Original", nullptr, aspect_ratio == RATIO_ORIG)){
				aspect_ratio = RATIO_ORIG;
			}
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Textures Caching", nullptr, !gCheckTextureHashFrequency)){
			gCheckTextureHashFrequency = gCheckTextureHashFrequency ? 0 : 1;
		}
		SetDescription("Enables caching for stored textures.\nIncreases performances but may cause graphical glitches.");
		if (ImGui::MenuItem("Bilinear Filter", nullptr, gGlobalPreferences.ForceLinearFilter)){
			gGlobalPreferences.ForceLinearFilter = !gGlobalPreferences.ForceLinearFilter;
		}
		SetDescription("Forces bilinear filtering on every texture.");
		ImGui::Separator();
		if (ImGui::MenuItem("vFlux", nullptr, vflux_window)){
			vflux_window = !vflux_window;
		}
		SetDescription("Blends a color filter on screen depending on daytime.");
		if (ImGui::MenuItem("V-Sync", nullptr, use_vsync)){
			use_vsync = use_vsync == GL_TRUE ? GL_FALSE : GL_TRUE;
			vglWaitVblankStart(use_vsync);
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Use VRAM", nullptr, use_cdram)){
			use_cdram = use_cdram == GL_TRUE ? GL_FALSE : GL_TRUE;
			vglUseVram(use_cdram);
		}
		SetDescription("Enables VRAM usage for textures storing.");
		if (ImGui::MenuItem("Clear Depth Buffer", nullptr, gClearDepthFrameBuffer)){
			gClearDepthFrameBuffer = !gClearDepthFrameBuffer;
		}
		SetDescription("Enables depth buffer clear at every frame. May solve some glitches.");
		if (ImGui::MenuItem("Wait Rendering Done", nullptr, wait_rendering)){
			wait_rendering = !wait_rendering;
		}
		SetDescription("Makes CPU wait GPU rendering end before elaborating the next frame.\nReduces artifacting at the cost of performances.");
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Audio")){
		if (ImGui::MenuItem("Disabled", nullptr, gAudioPluginEnabled == APM_DISABLED)){
			gAudioPluginEnabled = APM_DISABLED;
		}
		if (ImGui::MenuItem("Synchronous", nullptr, gAudioPluginEnabled == APM_ENABLED_SYNC)){
			gAudioPluginEnabled = APM_ENABLED_SYNC;
		}
		if (ImGui::MenuItem("Asynchronous", nullptr, gAudioPluginEnabled == APM_ENABLED_ASYNC)){
			gAudioPluginEnabled = APM_ENABLED_ASYNC;
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Input")){
		if (ImGui::BeginMenu("Controller 1", pinfo.port[0] != SCE_CTRL_TYPE_UNPAIRED)){
			if (ImGui::BeginMenu("Accessory")){
				if (ImGui::MenuItem("Rumble Pak", nullptr, has_rumblepak[0])){
					has_rumblepak[0] = true;
				}
				if (ImGui::MenuItem("Controller Pak", nullptr, !has_rumblepak[0])){
					has_rumblepak[0] = false;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Controller 2", pinfo.port[2] != SCE_CTRL_TYPE_UNPAIRED)){
			if (ImGui::BeginMenu("Accessory")){
				if (ImGui::MenuItem("Rumble Pak", nullptr, has_rumblepak[1])){
					has_rumblepak[1] = true;
				}
				if (ImGui::MenuItem("Controller Pak", nullptr, !has_rumblepak[1])){
					has_rumblepak[1] = false;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Controller 3", pinfo.port[3] != SCE_CTRL_TYPE_UNPAIRED)){
			if (ImGui::BeginMenu("Accessory")){
				if (ImGui::MenuItem("Rumble Pak", nullptr, has_rumblepak[2])){
					has_rumblepak[2] = true;
				}
				if (ImGui::MenuItem("Controller Pak", nullptr, !has_rumblepak[2])){
					has_rumblepak[2] = false;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Controller 4", pinfo.port[4] != SCE_CTRL_TYPE_UNPAIRED)){
			if (ImGui::BeginMenu("Accessory")){
				if (ImGui::MenuItem("Rumble Pak", nullptr, has_rumblepak[3])){
					has_rumblepak[3] = true;
				}
				if (ImGui::MenuItem("Controller Pak", nullptr, !has_rumblepak[3])){
					has_rumblepak[3] = false;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Extra")){
		if (ImGui::BeginMenu("UI Theme")){
			if (ImGui::MenuItem("Dark", nullptr, imgui_theme == DARK_THEME)){
				ImGui::StyleColorsDark();
				imgui_theme = DARK_THEME;
			}
			if (ImGui::MenuItem("Light", nullptr, imgui_theme == LIGHT_THEME)){
				ImGui::StyleColorsLight();
				imgui_theme = LIGHT_THEME;
			}
			if (ImGui::MenuItem("Classic", nullptr, imgui_theme == CLASSIC_THEME)){
				ImGui::StyleColorsClassic();
				imgui_theme = CLASSIC_THEME;
			}
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem("Hide Menubar", nullptr, hide_menubar)){
			hide_menubar = !hide_menubar;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Debugger", nullptr, debug_window)){
			debug_window = !debug_window;
		}
		if (ImGui::MenuItem("Console Logs", nullptr, logs_window)){
			logs_window = !logs_window;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Credits", nullptr, credits_window)){
			credits_window = !credits_window;
		}
		ImGui::EndMenu();
	}
}

void DrawCommonWindows() {
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);
	
	if (vflux_window) {
		ImGui::Begin("vFlux Configuration", &vflux_window);
		ImGui::ColorPicker3("Filter Color", vcolors);
		ImGui::Checkbox("Enable vFlux", &vflux_enabled);
		ImGui::End();
	}
	
	if (credits_window) {
		ImGui::Begin("Credits", &credits_window);
		ImGui::TextColored(ImVec4(255, 255, 0, 255), "Daedalus X64 v.%s", VERSION);
		ImGui::Text("Port Author: Rinnegatamante");
		ImGui::Separator();
		ImGui::TextColored(ImVec4(255, 255, 0, 255), "Patreon Supporters:");
		ImGui::Text("Tain Sueiras");
		ImGui::Text("drd7of14");
		ImGui::Text("polytoad");
		ImGui::Text("The Vita3K project");
		ImGui::Text("UnrootedTiara");
		ImGui::Text("psymu");
		ImGui::Text("@Sarkies_Proxy");
		ImGui::Separator();
		ImGui::TextColored(ImVec4(255, 255, 0, 255), "Special thanks to:");
		ImGui::Text("xerpi for the initial Vita port");
		ImGui::Text("MasterFeizz for the ARM DynaRec");
		ImGui::Text("m4xw for the help sanitizing PIF code");
		ImGui::Text("frangarcj for the help with some bugfixes");
		ImGui::Text("That One Seong & TheIronUniverse for the Livearea assets");
		ImGui::Text("withLogic for the high-res preview assets");
		ImGui::End();
	}
	
	if (debug_window) {
		ImGui::Begin("Debugger", &debug_window);
		ImGui::Text("Cartridge ID: 0x%04X", g_ROM.rh.CartID);
		ImGui::Text("Installed RSP Microcode: %s", cur_ucode);
		ImGui::End();
	}
	
	if (logs_window) {
		ImGui::Begin("Console Logs", &logs_window);
		for (int i = 0; i < MAX_DEBUG_LINES; i++) {
			if ((i == cur_dbg_line - 1) || ((cur_dbg_line == 0) && (i == MAX_DEBUG_LINES - 1))) ImGui::TextColored({1.0f, 1.0f, 0.0f, 1.0f}, dbg_lines[i]);
			else ImGui::Text(dbg_lines[i]);
		}
		ImGui::End();
	}
	
	if (vflux_enabled) {
		memcpy(&colors[0], vcolors, sizeof(float) * 3);
		memcpy(&colors[4], vcolors, sizeof(float) * 3);
		memcpy(&colors[8], vcolors, sizeof(float) * 3);
		memcpy(&colors[12], vcolors, sizeof(float) * 3);
		
		float a;
		SceDateTime time;
		sceRtcGetCurrentClockLocalTime(&time);
		if (time.hour < 6)		// Night/Early Morning
			a = 0.25f;
		else if (time.hour < 10) // Morning/Early Day
			a = 0.1f;
		else if (time.hour < 15) // Mid day
			a = 0.05f;
		else if (time.hour < 19) // Late day
			a = 0.15f;
		else // Evening/Night
			a = 0.2f;
		colors[3] = colors[7] = colors[11] = colors[15] = a;
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);
		vglVertexPointerMapped(vertices);
		vglColorPointerMapped(GL_FLOAT, colors);
		vglDrawObjects(GL_TRIANGLE_FAN, 4, true);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
}

void DrawMenuBar() {
	ImGui_ImplVitaGL_NewFrame();
	
	if (ImGui::BeginMainMenuBar()){
		DrawCommonMenuBar();
		ImGui::SameLine();
		ImGui::SetCursorPosX(870);
		ImGui::Text("Daedalus X64"); 
		ImGui::EndMainMenuBar();
	}
	
	DrawCommonWindows();
}

void ExecSaveState(int slot) {
	IO::Filename full_path;
	sprintf(full_path, "%s%s.ss%ld", DAEDALUS_VITA_PATH("SaveStates/"), g_ROM.settings.GameName.c_str(), slot);
	
	CPU_RequestSaveState(full_path);
	cached_saveslots[slot] = true;
}

void LoadSaveState(int slot) {
	IO::Filename full_path;
	sprintf(full_path, "%s%s.ss%ld", DAEDALUS_VITA_PATH("SaveStates/"), g_ROM.settings.GameName.c_str(), slot);
	
	CPU_RequestLoadState(full_path);
}

void DrawInGameMenuBar() {
	if (!has_cached_saveslots) {
		IO::Directory::EnsureExists(DAEDALUS_VITA_PATH("SaveStates/"));
		for (int i = 0; i <= MAX_SAVESLOT; i++) {
			IO::Filename save_path;
			sprintf(save_path, "%s%s.ss%ld", DAEDALUS_VITA_PATH("SaveStates/"), g_ROM.settings.GameName.c_str(), i);
			cached_saveslots[i] = IO::File::Exists(save_path);
		}
		has_cached_saveslots = true;
	}
	
	ImGui_ImplVitaGL_NewFrame();
	if (show_menubar) {
		if (ImGui::BeginMainMenuBar()){
			if (ImGui::BeginMenu("Files")){
				if (ImGui::BeginMenu("Save Savestate")){
					for (int i = 0; i <= MAX_SAVESLOT; i++) {
						char tag[8];
						sprintf(tag, "Slot %ld", i);
						if (ImGui::MenuItem(tag, nullptr, cached_saveslots[i])){
							ExecSaveState(i);
						}
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Load Savestate")){
					for (int i = 0; i <= MAX_SAVESLOT; i++) {
						char tag[8];
						sprintf(tag, "Slot %ld", i);
						if (ImGui::MenuItem(tag, nullptr, false, cached_saveslots[i])){
							LoadSaveState(i);
						}
					}
					ImGui::EndMenu();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Restart Rom")){
					restart_rom = true;
					CPU_Halt("Resetting");
				}
				if (ImGui::MenuItem("Close Rom")){
					has_cached_saveslots = false;
					CPU_Halt("Resetting");
				}
				ImGui::EndMenu();
			}
			DrawCommonMenuBar();
			ImGui::SameLine();
			ImGui::SetCursorPosX(870);
			ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate); 
			ImGui::EndMainMenuBar();
		}
	}
	
	DrawCommonWindows();
}