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

int gUiTheme = DARK_THEME;
int gAspectRatio = RATIO_16_9;
int gTexCacheMode = TEX_CACHE_ACCURATE;
bool gTexturesDumper = false;
bool gUseHighResTextures = false;

static bool cached_saveslots[MAX_SAVESLOT + 1];
static bool has_cached_saveslots = false;

extern bool has_rumblepak[4];
extern char cur_ucode[256];
extern char cur_audio_ucode[32];


bool show_menubar = true;
bool gHideMenubar = true;
bool run_emu = true;
bool restart_rom = false;

int gCpuMode = CPU_DYNAREC_UNSAFE;
int gSortOrder = SORT_A_TO_Z;

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

float gamma_val = 1.0f;

char dbg_lines[MAX_DEBUG_LINES][256];
int cur_dbg_line = 0;

void saveConfig(const char *game)
{
	char tmp[128];
	sprintf(tmp, game);
	stripGameName(tmp);
	
	char configFile[512];
	sprintf(configFile, "%s%s.ini", DAEDALUS_VITA_PATH("Configs/"), tmp);
	
	sceIoMkdir(DAEDALUS_VITA_PATH("Configs/"), 0777);
	
	FILE *config = fopen(configFile, "w+");
	if (config != NULL) {
		fprintf(config, "%s=%d\n", "gCpuMode", gCpuMode);
		fprintf(config, "%s=%d\n", "gOSHooksEnabled", gOSHooksEnabled);
		fprintf(config, "%s=%d\n", "gSpeedSyncEnabled", gSpeedSyncEnabled);
		
		fprintf(config, "%s=%d\n", "gVideoRateMatch", gVideoRateMatch);
		fprintf(config, "%s=%d\n", "gAudioRateMatch", gAudioRateMatch);
		fprintf(config, "%s=%d\n", "gAspectRatio", gAspectRatio);
		fprintf(config, "%s=%d\n", "gTexCacheMode", gTexCacheMode);
		fprintf(config, "%s=%d\n", "gForceLinearFilter", gGlobalPreferences.ForceLinearFilter);
		
		fprintf(config, "%s=%d\n", "gUseMipmaps", gUseMipmaps);
		fprintf(config, "%s=%d\n", "gUseVSync", gUseVSync);
		fprintf(config, "%s=%d\n", "gUseCdram", gUseCdram);
		fprintf(config, "%s=%d\n", "gClearDepthFrameBuffer", gClearDepthFrameBuffer);
		fprintf(config, "%s=%d\n", "gWaitRendering", gWaitRendering);
		
		fprintf(config, "%s=%d\n", "gAudioPluginEnabled", (int)gAudioPluginEnabled);
		fprintf(config, "%s=%d\n", "gUseMp3", gUseMp3);
		
		fprintf(config, "%s=%d\n", "gUseExpansionPak", gUseExpansionPak);
		fprintf(config, "%s=%d\n", "gControllerIndex", gControllerIndex);
		
		fprintf(config, "%s=%d\n", "gTexturesDumper", (int)gTexturesDumper);
		fprintf(config, "%s=%d\n", "gUseHighResTextures", (int)gUseHighResTextures);
		
		fprintf(config, "%s=%d\n", "gSortOrder", gSortOrder);
		fprintf(config, "%s=%d\n", "gUiTheme", gUiTheme);
		fprintf(config, "%s=%d\n", "gHideMenubar", gHideMenubar);
		fprintf(config, "%s=%d\n", "gSkipCompatListUpdate", (int)gSkipCompatListUpdate);
		fprintf(config, "%s=%d\n", "gAutoUpdate", (int)gAutoUpdate);
		fclose(config);
	}
}

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

void DrawExtraMenu() {
	if (ImGui::BeginMenu("Extra")){
		if (ImGui::MenuItem("Save Global Settings")){
			saveConfig("default");
		}
		ImGui::Separator();
		if (ImGui::BeginMenu("UI Theme")){
			if (ImGui::MenuItem("Dark", nullptr, gUiTheme == DARK_THEME)){
				setUiTheme(DARK_THEME);
			}
			if (ImGui::MenuItem("Light", nullptr, gUiTheme == LIGHT_THEME)){
				setUiTheme(LIGHT_THEME);
			}
			if (ImGui::MenuItem("Classic", nullptr, gUiTheme == CLASSIC_THEME)){
				setUiTheme(CLASSIC_THEME);
			}
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem("Hide Menubar", nullptr, gHideMenubar)){
			gHideMenubar = !gHideMenubar;
		}
		if (ImGui::MenuItem("Auto Update at Boot", nullptr, gAutoUpdate)){
			gAutoUpdate = !gAutoUpdate;
		}
		if (ImGui::MenuItem("Update Compat List at Boot", nullptr, !gSkipCompatListUpdate)){
			gSkipCompatListUpdate = !gSkipCompatListUpdate;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Debugger", nullptr, debug_window)){
			debug_window = !debug_window;
		}
		if (ImGui::MenuItem("Console Logs", nullptr, logs_window)){
			logs_window = !logs_window;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Textures Dumper", nullptr, gTexturesDumper)){
			gTexturesDumper = !gTexturesDumper;
		}
		SetDescription("Enables textures dumping for high-res textures pack.");
		ImGui::Separator();
		if (ImGui::MenuItem("Credits", nullptr, credits_window)){
			credits_window = !credits_window;
		}
		ImGui::EndMenu();
	}
}

void DrawCommonMenuBar() {
	SceCtrlPortInfo pinfo;
	sceCtrlGetControllerPortInfo(&pinfo);
	if (ImGui::BeginMenu("Emulation")){
		if (ImGui::BeginMenu("CPU")){
			if (ImGui::MenuItem("DynaRec (Unsafe)", nullptr, gCpuMode == CPU_DYNAREC_UNSAFE)){
				setCpuMode(CPU_DYNAREC_UNSAFE);
			}
			SetDescription("Enables full dynamic recompilation for best performances.");
			if (ImGui::MenuItem("DynaRec (Safe)", nullptr, gCpuMode == CPU_DYNAREC_SAFE)){
				setCpuMode(CPU_DYNAREC_SAFE);
			}
			SetDescription("Enables safe dynamic recompilation for good performances and better compatibility.");
			if (ImGui::MenuItem("Cached Interpreter", nullptr, gCpuMode == CPU_CACHED_INTERPRETER)){
				setCpuMode(CPU_CACHED_INTERPRETER);
			}
			SetDescription("Enables cached interpreter for decent performances and better compatibility.");
			if (ImGui::MenuItem("Interpreter", nullptr, gCpuMode == CPU_INTERPRETER)){
				setCpuMode(CPU_INTERPRETER);
			}
			SetDescription("Enables interpreter for best compatibility.");
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem("High Level Emulation", nullptr, gOSHooksEnabled)){
			gOSHooksEnabled = !gOSHooksEnabled;
		}
		SetDescription("Enables high level emulation of OS functions for better performance.\nMay cause instability on some games.");
		if (ImGui::MenuItem("Expansion Pak", nullptr, gUseExpansionPak)){
				gUseExpansionPak = !gUseExpansionPak;
			}
		ImGui::Separator();
		if (ImGui::MenuItem("Frame Limit", nullptr, gSpeedSyncEnabled)){
			gSpeedSyncEnabled = !gSpeedSyncEnabled;
		}
		SetDescription("Limits framerate to the one running game is supposed to have.");
		if (ImGui::MenuItem("Sync Video Rate", nullptr, gVideoRateMatch)){
			gVideoRateMatch = !gVideoRateMatch;
		}
		SetDescription("Speed up video logic to match framerate.");
		if (ImGui::MenuItem("Sync Audio Rate", nullptr, gAudioRateMatch)){
			gAudioRateMatch = !gAudioRateMatch;
		}
		SetDescription("Speed up audio logic to match framerate.");
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Graphics")){
		if (ImGui::BeginMenu("Aspect Ratio")){
			if (ImGui::MenuItem("16:9", nullptr, gAspectRatio == RATIO_16_9)){
				gAspectRatio = RATIO_16_9;
			}
			if (ImGui::MenuItem("16:9 Unstretched", nullptr, gAspectRatio == RATIO_16_9_HACK)){
				gAspectRatio = RATIO_16_9_HACK;
			}
			if (ImGui::MenuItem("4:3", nullptr, gAspectRatio == RATIO_4_3)){
				gAspectRatio = RATIO_4_3;
			}
			if (ImGui::MenuItem("Original", nullptr, gAspectRatio == RATIO_ORIG)){
				gAspectRatio = RATIO_ORIG;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Brightness")){
			ImGui::SliderFloat("", &gamma_val, 1.0f, 0.0f);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::BeginMenu("Textures Caching")){
			if (ImGui::MenuItem("Disabled", nullptr, gTexCacheMode == TEX_CACHE_DISABLED)){
				setTexCacheMode(TEX_CACHE_DISABLED);
			}
			SetDescription("Disables caching for stored textures.\nReduces graphical glitches at the cost of performances.");
			if (ImGui::MenuItem("Accurate", nullptr, gTexCacheMode == TEX_CACHE_ACCURATE)){
				setTexCacheMode(TEX_CACHE_ACCURATE);
			}
			SetDescription("Caches stored textures at each frame.\nImproves performances but may cause glitches.");
			if (ImGui::MenuItem("Fast", nullptr, gTexCacheMode == TEX_CACHE_FAST)){
				setTexCacheMode(TEX_CACHE_FAST);
			}
			SetDescription("Caches permanently stored textures.\nImproves greatly performances but may cause severe glitches.");
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem("Bilinear Filter", nullptr, gGlobalPreferences.ForceLinearFilter)){
			gGlobalPreferences.ForceLinearFilter = !gGlobalPreferences.ForceLinearFilter;
		}
		SetDescription("Forces bilinear filtering on every texture.");
		if (ImGui::MenuItem("Mipmaps", nullptr, gUseMipmaps)){
			gUseMipmaps = !gUseMipmaps;
		}
		SetDescription("Forces mipmaps generation for 3D rendering.");
		if (ImGui::MenuItem("High-Res Textures", nullptr, gUseHighResTextures)){
			gUseHighResTextures = !gUseHighResTextures;
		}
		SetDescription("Enables external high-res textures packs usage.");
		ImGui::Separator();
		if (ImGui::MenuItem("vFlux", nullptr, vflux_window)){
			vflux_window = !vflux_window;
		}
		SetDescription("Blends a color filter on screen depending on daytime.");
		if (ImGui::MenuItem("V-Sync", nullptr, gUseVSync)){
			gUseVSync = gUseVSync == GL_TRUE ? GL_FALSE : GL_TRUE;
			vglWaitVblankStart(gUseVSync);
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Use VRAM", nullptr, gUseCdram)){
			gUseCdram = gUseCdram == GL_TRUE ? GL_FALSE : GL_TRUE;
			vglUseVram(gUseCdram);
		}
		SetDescription("Enables VRAM usage for textures storing.");
		if (ImGui::MenuItem("Clear Depth Buffer", nullptr, gClearDepthFrameBuffer)){
			gClearDepthFrameBuffer = !gClearDepthFrameBuffer;
		}
		SetDescription("Enables depth buffer clear at every frame. May solve some glitches.");
		if (ImGui::MenuItem("Wait Rendering Done", nullptr, gWaitRendering)){
			gWaitRendering = !gWaitRendering;
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
		ImGui::Separator();
		if (ImGui::MenuItem("Disable MP3 instructions", nullptr, !gUseMp3)){
			gUseMp3 = !gUseMp3;
		}
		SetDescription("Disables MP3 instructions for better performances.");
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Input")){
		if (ImGui::BeginMenu("Controls Mapping")){
			u32 num_configs = CInputManager::Get()->GetNumConfigurations();
			for (u32 i = 0; i < num_configs; i++) {
				if (ImGui::MenuItem(CInputManager::Get()->GetConfigurationName(i), nullptr, i == gControllerIndex)){
					CInputManager::Get()->SetConfiguration(i);
				}
				SetDescription(CInputManager::Get()->GetConfigurationDescription(i));
			}
			ImGui::EndMenu();
		}
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
		ImGui::TextColored(ImVec4(255, 255, 0, 255), "Daedalus X64 v.%s (%s)", VERSION, stringify(GIT_VERSION));
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
		ImGui::Text("TheFloW for his contributions to the DynaRec code");
		ImGui::Text("m4xw for the help sanitizing PIF code");
		ImGui::Text("Salvy & frangarcj for the help with some bugfixes");
		ImGui::Text("Inssame for some additions to the UI code");
		ImGui::Text("That One Seong & TheIronUniverse for the Livearea assets");
		ImGui::Text("withLogic for the high-res preview assets");
		ImGui::End();
	}
	
	if (debug_window) {
		ImGui::Begin("Debugger", &debug_window);
		ImGui::Text("Cartridge ID: 0x%04X", g_ROM.rh.CartID);
		ImGui::Text("Installed GFX Microcode: %s", cur_ucode);
		ImGui::Text("Installed Audio Microcode: %s", cur_audio_ucode);
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
		if (ImGui::BeginMenu("Options")){
			if (ImGui::BeginMenu("Sort Roms")){
				if (ImGui::MenuItem("A to Z", nullptr, gSortOrder == SORT_A_TO_Z)){
					gSortOrder = SORT_A_TO_Z;
				}
				if (ImGui::MenuItem("Z to A", nullptr, gSortOrder == SORT_Z_TO_A)){
					gSortOrder = SORT_Z_TO_A;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		DrawCommonMenuBar();
		DrawExtraMenu();
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
				if (ImGui::MenuItem("Save Game Settings")){
					saveConfig(g_ROM.settings.GameName.c_str());
				}
				ImGui::Separator();
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
					CPU_Halt("Restarting Rom");
				}
				if (ImGui::MenuItem("Close Rom")){
					has_cached_saveslots = false;
					CPU_Halt("Closing Rom");
				}
				ImGui::EndMenu();
			}
			DrawCommonMenuBar();
			if (codegroupcount > 0) {
				if (ImGui::BeginMenu("Cheats")){
					for (u32 i = 0; i < codegroupcount; i++) {
						if (ImGui::MenuItem(codegrouplist[i].name, nullptr, codegrouplist[i].enable)){
							codegrouplist[i].enable = !codegrouplist[i].enable;
							CheatCodes_Apply(i, IN_GAME);
						}
						if (strlen(codegrouplist[i].note) > 0) SetDescription(codegrouplist[i].note);
					}
					ImGui::EndMenu();
				}
			}
			DrawExtraMenu();
			ImGui::SameLine();
			ImGui::SetCursorPosX(870);
			ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate); 
			ImGui::EndMainMenuBar();
		}
	}
	
	DrawCommonWindows();
}