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

int gLanguageIndex = SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
int gUiTheme = DARK_THEME;
int gAspectRatio = RATIO_16_9;
int gTexCacheMode = TEX_CACHE_ACCURATE;
bool gTexturesDumper = false;
bool gUseHighResTextures = false;
bool gUseRearpad = false;

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
		fprintf(config, "%s=%d\n", "gLanguageIndex", gLanguageIndex);
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
	if (ImGui::BeginMenu(lang_strings[STR_MENU_EXTRA])){
		if (ImGui::MenuItem(lang_strings[STR_MENU_GLOBAL_SETTINGS])){
			saveConfig("default");
		}
		ImGui::Separator();
		if (ImGui::BeginMenu(lang_strings[STR_MENU_UI_THEME])){
			if (ImGui::MenuItem(lang_strings[STR_THEME_DARK], nullptr, gUiTheme == DARK_THEME)){
				setUiTheme(DARK_THEME);
			}
			if (ImGui::MenuItem(lang_strings[STR_THEME_LIGHT], nullptr, gUiTheme == LIGHT_THEME)){
				setUiTheme(LIGHT_THEME);
			}
			if (ImGui::MenuItem(lang_strings[STR_THEME_CLASSIC], nullptr, gUiTheme == CLASSIC_THEME)){
				setUiTheme(CLASSIC_THEME);
			}
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem(lang_strings[STR_MENU_MENUBAR], nullptr, gHideMenubar)){
			gHideMenubar = !gHideMenubar;
		}
		if (ImGui::MenuItem(lang_strings[STR_MENU_AUTOUPDATE], nullptr, gAutoUpdate)){
			gAutoUpdate = !gAutoUpdate;
		}
		if (ImGui::MenuItem(lang_strings[STR_MENU_COMPAT_LIST], nullptr, !gSkipCompatListUpdate)){
			gSkipCompatListUpdate = !gSkipCompatListUpdate;
		}
		ImGui::Separator();
		if (ImGui::MenuItem(lang_strings[STR_MENU_DEBUGGER], nullptr, debug_window)){
			debug_window = !debug_window;
		}
		if (ImGui::MenuItem(lang_strings[STR_MENU_LOG], nullptr, logs_window)){
			logs_window = !logs_window;
		}
		ImGui::Separator();
		if (ImGui::MenuItem(lang_strings[STR_MENU_TEX_DUMPER], nullptr, gTexturesDumper)){
			gTexturesDumper = !gTexturesDumper;
		}
		SetDescription(lang_strings[STR_DESC_TEX_DUMPER]);
		ImGui::Separator();
		if (ImGui::MenuItem(lang_strings[STR_MENU_CREDITS], nullptr, credits_window)){
			credits_window = !credits_window;
		}
		ImGui::EndMenu();
	}
}

void DrawCommonMenuBar() {
	SceCtrlPortInfo pinfo;
	sceCtrlGetControllerPortInfo(&pinfo);
	if (ImGui::BeginMenu(lang_strings[STR_MENU_EMULATION])){
		if (ImGui::BeginMenu("CPU")){
			if (ImGui::MenuItem(lang_strings[STR_MENU_UNSAFE_DYNAREC], nullptr, gCpuMode == CPU_DYNAREC_UNSAFE)){
				setCpuMode(CPU_DYNAREC_UNSAFE);
			}
			SetDescription(lang_strings[STR_DESC_UNSAFE_DYNAREC]);
			if (ImGui::MenuItem(lang_strings[STR_MENU_SAFE_DYNAREC], nullptr, gCpuMode == CPU_DYNAREC_SAFE)){
				setCpuMode(CPU_DYNAREC_SAFE);
			}
			SetDescription(lang_strings[STR_DESC_SAFE_DYNAREC]);
			if (ImGui::MenuItem(lang_strings[STR_MENU_CACHED_INTERP], nullptr, gCpuMode == CPU_CACHED_INTERPRETER)){
				setCpuMode(CPU_CACHED_INTERPRETER);
			}
			SetDescription(lang_strings[STR_DESC_CACHED_INTERP]);
			if (ImGui::MenuItem(lang_strings[STR_MENU_INTERP], nullptr, gCpuMode == CPU_INTERPRETER)){
				setCpuMode(CPU_INTERPRETER);
			}
			SetDescription(lang_strings[STR_DESC_INTERP]);
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem(lang_strings[STR_MENU_HLE], nullptr, gOSHooksEnabled)){
			gOSHooksEnabled = !gOSHooksEnabled;
		}
		SetDescription(lang_strings[STR_DESC_HLE]);
		if (ImGui::MenuItem("Expansion Pak", nullptr, gUseExpansionPak)){
				gUseExpansionPak = !gUseExpansionPak;
			}
		ImGui::Separator();
		if (ImGui::MenuItem(lang_strings[STR_MENU_FRAME_LIMIT], nullptr, gSpeedSyncEnabled)){
			gSpeedSyncEnabled = !gSpeedSyncEnabled;
		}
		SetDescription(lang_strings[STR_DESC_FRAME_LIMIT]);
		if (ImGui::MenuItem(lang_strings[STR_MENU_VIDEO_RATE], nullptr, gVideoRateMatch)){
			gVideoRateMatch = !gVideoRateMatch;
		}
		SetDescription(lang_strings[STR_DESC_VIDEO_RATE]);
		if (ImGui::MenuItem(lang_strings[STR_MENU_AUDIO_RATE], nullptr, gAudioRateMatch)){
			gAudioRateMatch = !gAudioRateMatch;
		}
		SetDescription(lang_strings[STR_DESC_AUDIO_RATE]);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu(lang_strings[STR_MENU_GRAPHICS])){
		if (ImGui::BeginMenu(lang_strings[STR_MENU_ASPECT_RATIO])){
			if (ImGui::MenuItem("16:9", nullptr, gAspectRatio == RATIO_16_9)){
				gAspectRatio = RATIO_16_9;
			}
			if (ImGui::MenuItem(lang_strings[STR_MENU_RATIO_UNSTRETCHED], nullptr, gAspectRatio == RATIO_16_9_HACK)){
				gAspectRatio = RATIO_16_9_HACK;
			}
			if (ImGui::MenuItem("4:3", nullptr, gAspectRatio == RATIO_4_3)){
				gAspectRatio = RATIO_4_3;
			}
			if (ImGui::MenuItem(lang_strings[STR_MENU_RATIO_ORIGINAL], nullptr, gAspectRatio == RATIO_ORIG)){
				gAspectRatio = RATIO_ORIG;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(lang_strings[STR_MENU_BRIGHTNESS])){
			ImGui::SliderFloat("", &gamma_val, 1.0f, 0.0f);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::BeginMenu(lang_strings[STR_MENU_TEX_CACHE])){
			if (ImGui::MenuItem(lang_strings[STR_DISABLED], nullptr, gTexCacheMode == TEX_CACHE_DISABLED)){
				setTexCacheMode(TEX_CACHE_DISABLED);
			}
			SetDescription(lang_strings[STR_DESC_CACHE_DISABLED]);
			if (ImGui::MenuItem(lang_strings[STR_ACCURATE], nullptr, gTexCacheMode == TEX_CACHE_ACCURATE)){
				setTexCacheMode(TEX_CACHE_ACCURATE);
			}
			SetDescription(lang_strings[STR_DESC_CACHE_ACCURATE]);
			if (ImGui::MenuItem(lang_strings[STR_FAST], nullptr, gTexCacheMode == TEX_CACHE_FAST)){
				setTexCacheMode(TEX_CACHE_FAST);
			}
			SetDescription(lang_strings[STR_DESC_CACHE_FAST]);
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem(lang_strings[STR_MENU_BILINEAR], nullptr, gGlobalPreferences.ForceLinearFilter)){
			gGlobalPreferences.ForceLinearFilter = !gGlobalPreferences.ForceLinearFilter;
		}
		SetDescription(lang_strings[STR_DESC_BILINEAR]);
		if (ImGui::MenuItem(lang_strings[STR_MENU_MIPMAPS], nullptr, gUseMipmaps)){
			gUseMipmaps = !gUseMipmaps;
		}
		SetDescription(lang_strings[STR_DESC_MIPMAPS]);
		if (ImGui::MenuItem(lang_strings[STR_MENU_HIRES_TEX], nullptr, gUseHighResTextures)){
			gUseHighResTextures = !gUseHighResTextures;
		}
		SetDescription(lang_strings[STR_DESC_HIRES_TEX]);
		ImGui::Separator();
		if (ImGui::MenuItem("vFlux", nullptr, vflux_window)){
			vflux_window = !vflux_window;
		}
		SetDescription(lang_strings[STR_DESC_VFLUX]);
		if (ImGui::MenuItem("V-Sync", nullptr, gUseVSync)){
			gUseVSync = gUseVSync == GL_TRUE ? GL_FALSE : GL_TRUE;
			vglWaitVblankStart(gUseVSync);
		}
		ImGui::Separator();
		if (ImGui::MenuItem(lang_strings[STR_MENU_VRAM], nullptr, gUseCdram)){
			gUseCdram = gUseCdram == GL_TRUE ? GL_FALSE : GL_TRUE;
			vglUseVram(gUseCdram);
		}
		SetDescription(lang_strings[STR_DESC_VRAM]);
		if (ImGui::MenuItem(lang_strings[STR_MENU_DEPTH_BUFFER], nullptr, gClearDepthFrameBuffer)){
			gClearDepthFrameBuffer = !gClearDepthFrameBuffer;
		}
		SetDescription(lang_strings[STR_DESC_DEPTH_BUFFER]);
		if (ImGui::MenuItem(lang_strings[STR_MENU_WAIT_REND], nullptr, gWaitRendering)){
			gWaitRendering = !gWaitRendering;
		}
		SetDescription(lang_strings[STR_DESC_WAIT_REND]);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu(lang_strings[STR_MENU_AUDIO])){
		if (ImGui::MenuItem(lang_strings[STR_DISABLED], nullptr, gAudioPluginEnabled == APM_DISABLED)){
			gAudioPluginEnabled = APM_DISABLED;
		}
		if (ImGui::MenuItem(lang_strings[STR_SYNC], nullptr, gAudioPluginEnabled == APM_ENABLED_SYNC)){
			gAudioPluginEnabled = APM_ENABLED_SYNC;
		}
		if (ImGui::MenuItem(lang_strings[STR_ASYNC], nullptr, gAudioPluginEnabled == APM_ENABLED_ASYNC)){
			gAudioPluginEnabled = APM_ENABLED_ASYNC;
		}
		ImGui::Separator();
		if (ImGui::MenuItem(lang_strings[STR_MENU_MP3_INSTR], nullptr, !gUseMp3)){
			gUseMp3 = !gUseMp3;
		}
		SetDescription(lang_strings[STR_DESC_MP3_INSTR]);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu(lang_strings[STR_MENU_INPUT])){
		if (!sceKernelIsPSVitaTV()) {
			if (ImGui::MenuItem(lang_strings[STR_MENU_REARPAD], nullptr, gUseRearpad)){
				gUseRearpad = !gUseRearpad;
			}
			SetDescription(lang_strings[STR_DESC_REARPAD]);
		}
		if (ImGui::BeginMenu(lang_strings[STR_MENU_CTRL_MAP])){
			u32 num_configs = CInputManager::Get()->GetNumConfigurations();
			for (u32 i = 0; i < num_configs; i++) {
				if (ImGui::MenuItem(CInputManager::Get()->GetConfigurationName(i), nullptr, i == gControllerIndex)){
					CInputManager::Get()->SetConfiguration(i);
				}
				SetDescription(CInputManager::Get()->GetConfigurationDescription(i));
			}
			ImGui::EndMenu();
		}
		ImGui::Separator();
		char ctrl_str[64];
		sprintf(ctrl_str, "%s 1", lang_strings[STR_CONTROLLER]);
		if (ImGui::BeginMenu(ctrl_str, pinfo.port[0] != SCE_CTRL_TYPE_UNPAIRED)){
			if (ImGui::BeginMenu(lang_strings[STR_ACCESSORY])){
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
		sprintf(ctrl_str, "%s 2", lang_strings[STR_CONTROLLER]);
		if (ImGui::BeginMenu(ctrl_str, pinfo.port[2] != SCE_CTRL_TYPE_UNPAIRED)){
			if (ImGui::BeginMenu(lang_strings[STR_ACCESSORY])){
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
		sprintf(ctrl_str, "%s 3", lang_strings[STR_CONTROLLER]);
		if (ImGui::BeginMenu(ctrl_str, pinfo.port[3] != SCE_CTRL_TYPE_UNPAIRED)){
			if (ImGui::BeginMenu(lang_strings[STR_ACCESSORY])){
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
		sprintf(ctrl_str, "%s 4", lang_strings[STR_CONTROLLER]);
		if (ImGui::BeginMenu(ctrl_str, pinfo.port[4] != SCE_CTRL_TYPE_UNPAIRED)){
			if (ImGui::BeginMenu(lang_strings[STR_ACCESSORY])){
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
	if (ImGui::BeginMenu(lang_strings[STR_MENU_LANG])){
		if (ImGui::MenuItem("Català", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_CATALAN)){
			setTranslation(SCE_SYSTEM_PARAM_LANG_CATALAN);
		}
		if (ImGui::MenuItem("Dansk", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_DANISH)){
			setTranslation(SCE_SYSTEM_PARAM_LANG_DANISH);
		}
		if (ImGui::MenuItem("Deutsch", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_GERMAN)){
			setTranslation(SCE_SYSTEM_PARAM_LANG_GERMAN);
		}
		if (ImGui::MenuItem("English", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_ENGLISH_US)){
			setTranslation(SCE_SYSTEM_PARAM_LANG_ENGLISH_US);
		}
		if (ImGui::MenuItem("Español", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_SPANISH)){
			setTranslation(SCE_SYSTEM_PARAM_LANG_SPANISH);
		}
		if (ImGui::MenuItem("Français", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_FRENCH)){
			setTranslation(SCE_SYSTEM_PARAM_LANG_FRENCH);
		}
		if (ImGui::MenuItem("Italiano", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_ITALIAN)){
			setTranslation(SCE_SYSTEM_PARAM_LANG_ITALIAN);
		}
		if (ImGui::MenuItem("Polskie", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_POLISH)){
			setTranslation(SCE_SYSTEM_PARAM_LANG_POLISH);
		}
		if (ImGui::MenuItem("Português (BR)", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR)){
			setTranslation(SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR);
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
		ImGui::Begin(lang_strings[STR_VFLUX_CONFIG], &vflux_window);
		ImGui::ColorPicker3(lang_strings[STR_VFLUX_COLOR], vcolors);
		ImGui::Checkbox(lang_strings[STR_VFLUX_ENABLE], &vflux_enabled);
		ImGui::End();
	}
	
	if (credits_window) {
		ImGui::Begin(lang_strings[STR_MENU_CREDITS], &credits_window);
		ImGui::TextColored(ImVec4(255, 255, 0, 255), "Daedalus X64 v.%s (%s)", VERSION, stringify(GIT_VERSION));
		ImGui::Text("%S: Rinnegatamante", lang_strings[STR_CREDITS_AUTHOR]);
		ImGui::Separator();
		ImGui::TextColored(ImVec4(255, 255, 0, 255), lang_strings[STR_CREDITS_PATRONERS]);
		ImGui::Text("Tain Sueiras");
		ImGui::Text("drd7of14");
		ImGui::Text("polytoad");
		ImGui::Text("The Vita3K project");
		ImGui::Text("UnrootedTiara");
		ImGui::Text("psymu");
		ImGui::Text("@Sarkies_Proxy");
		ImGui::Separator();
		ImGui::TextColored(ImVec4(255, 255, 0, 255), lang_strings[STR_CREDITS_THANKS]);
		ImGui::Text(lang_strings[STR_CREDITS_1]);
		ImGui::Text(lang_strings[STR_CREDITS_2]);
		ImGui::Text(lang_strings[STR_CREDITS_3]);
		ImGui::Text(lang_strings[STR_CREDITS_4]);
		ImGui::Text(lang_strings[STR_CREDITS_5]);
		ImGui::Text(lang_strings[STR_CREDITS_6]);
		ImGui::Text(lang_strings[STR_CREDITS_7]);
		ImGui::Text(lang_strings[STR_CREDITS_8]);
		ImGui::Separator();
		ImGui::TextColored(ImVec4(255, 255, 0, 255), lang_strings[STR_CREDITS_TRANSLATORS]);
		ImGui::Text("Rinnegatamante (ITA)");
		ImGui::Text("Samilop Cimmerian Iter (FRA)");
		ImGui::Text("f2pwn (CAT)");
		ImGui::Text("SamuEDL98 (ESP)");
		ImGui::Text("S1ngyy (GER)");
		ImGui::Text("Kamui (BRA)");
		ImGui::Text("coestergaard (DAN)");
		ImGui::Text("Kiiro Yakumo (POL)");
		ImGui::End();
	}
	
	if (debug_window) {
		ImGui::Begin(lang_strings[STR_MENU_DEBUGGER], &debug_window);
		ImGui::Text("%s: 0x%04X", lang_strings[STR_CART_ID], g_ROM.rh.CartID);
		ImGui::Text("%s: %s", lang_strings[STR_GFX_UCODE], cur_ucode);
		ImGui::Text("%s: %s", lang_strings[STR_AUDIO_UCODE], cur_audio_ucode);
		ImGui::End();
	}
	
	if (logs_window) {
		ImGui::Begin(lang_strings[STR_MENU_LOG], &logs_window);
		for (int i = 0; i < MAX_DEBUG_LINES; i++) {
			if ((i == cur_dbg_line - 1) || ((cur_dbg_line == 0) && (i == MAX_DEBUG_LINES - 1))) ImGui::TextColored({1.0f, 1.0f, 0.0f, 1.0f}, dbg_lines[i]);
			else ImGui::Text(dbg_lines[i]);
		}
		ImGui::End();
	}
	
	if (vflux_enabled) {
		memcpy_neon(&colors[0], vcolors, sizeof(float) * 3);
		memcpy_neon(&colors[4], vcolors, sizeof(float) * 3);
		memcpy_neon(&colors[8], vcolors, sizeof(float) * 3);
		memcpy_neon(&colors[12], vcolors, sizeof(float) * 3);
		
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
		if (ImGui::BeginMenu(lang_strings[STR_MENU_OPTIONS])){
			if (ImGui::BeginMenu(lang_strings[STR_MENU_SORT_ROMS])){
				if (ImGui::MenuItem(lang_strings[STR_SORT_A_TO_Z], nullptr, gSortOrder == SORT_A_TO_Z)){
					gSortOrder = SORT_A_TO_Z;
				}
				if (ImGui::MenuItem(lang_strings[STR_SORT_Z_TO_A], nullptr, gSortOrder == SORT_Z_TO_A)){
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
			if (ImGui::BeginMenu(lang_strings[STR_MENU_FILES])){
				if (ImGui::MenuItem(lang_strings[STR_MENU_GAME_SETTINGS])){
					saveConfig(g_ROM.settings.GameName.c_str());
				}
				ImGui::Separator();
				if (ImGui::BeginMenu(lang_strings[STR_MENU_SAVE_STATE])){
					for (int i = 0; i <= MAX_SAVESLOT; i++) {
						char tag[8];
						sprintf(tag, "%s %ld", lang_strings[STR_SLOT], i);
						if (ImGui::MenuItem(tag, nullptr, cached_saveslots[i])){
							ExecSaveState(i);
						}
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu(lang_strings[STR_MENU_LOAD_STATE])){
					for (int i = 0; i <= MAX_SAVESLOT; i++) {
						char tag[8];
						sprintf(tag, "%s %ld", lang_strings[STR_SLOT], i);
						if (ImGui::MenuItem(tag, nullptr, false, cached_saveslots[i])){
							LoadSaveState(i);
						}
					}
					ImGui::EndMenu();
				}
				ImGui::Separator();
				if (ImGui::MenuItem(lang_strings[STR_MENU_RESTART_ROM])){
					restart_rom = true;
					CPU_Halt(lang_strings[STR_MENU_RESTART_ROM]);
				}
				if (ImGui::MenuItem(lang_strings[STR_MENU_CLOSE_ROM])){
					has_cached_saveslots = false;
					CPU_Halt(lang_strings[STR_MENU_CLOSE_ROM]);
				}
				ImGui::EndMenu();
			}
			DrawCommonMenuBar();
			if (codegroupcount > 0) {
				if (ImGui::BeginMenu(lang_strings[STR_MENU_CHEATS])){
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