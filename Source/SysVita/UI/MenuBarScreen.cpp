#include <stdlib.h>
#include <stdio.h>

#include <vitasdk.h>
#include <vitaGL.h>

#include "BuildOptions.h"
#include "Config/ConfigOptions.h"
#include "Core/Cheats.h"
#include "Core/CPU.h"
#include "Core/Dynamo.h"
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
#include "Utility/stb_image.h"
#include "SysVita/UI/Menu.h"
#include "SysVita/Input/TouchZoneMapping.h"

#define MAX_SAVESLOT 9

extern void loadConfig(const char *game);

char ver_str[64];
float ver_len = 0.0f;
bool calculate_ver_len = true;
bool pendingDownload = false;
bool is_main_menu = true;
bool fontDirty = false;

int gLanguageIndex = SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
int gUiTheme = DARK_THEME;
int gAspectRatio = RATIO_16_9;
int gTexCacheMode = TEX_CACHE_ACCURATE;
int gAntiAliasing = ANTIALIASING_MSAA_4X;
int gPostProcessing = 0;
int gOverlay = 0;
bool gTexturesDumper = false;
bool gUseHighResTextures = false;
bool gUseRearpad = false;
int gRendererType = RENDERER_LEGACY;
bool gRendererChanged = false;
bool gNetBoot = false;
bool gSRGB = false;
bool gForceLinearFilter = false;

uint8_t shader_idx = 0;

GLuint shaders[4];
GLuint program[2] = {0xDEADBEEF, 0xDEADBEEF};
GLuint cur_prog;
GLuint cur_overlay = 0xDEADBEEF;

static char custom_path_str[512];
static char net_path_str[512];
bool custom_path_str_dirty = true;
bool net_path_str_dirty = true;

static bool cached_saveslots[MAX_SAVESLOT + 1];
static bool has_cached_saveslots = false;

extern bool pause_emu;
extern bool has_rumblepak[4];
extern char cur_gfx_ucode[64];
extern char cur_gfx_ucode_str[256];
extern char cur_audio_ucode[32];
extern char rom_game_name[256];

bool gBigText = false;
bool show_menubar = true;
bool gHideMenubar = true;
bool gDisableTouchMenu = false;
bool run_emu = true;
bool restart_rom = false;

int gCpuMode = CPU_DYNAREC;
int gSortOrder = SORT_A_TO_Z;

#ifndef SCE_CTRL_RUP
#define SCE_CTRL_RUP    SCE_CTRL_HEADPHONE
#define SCE_CTRL_RDOWN  SCE_CTRL_VOLUP
#define SCE_CTRL_RLEFT  SCE_CTRL_VOLDOWN
#define SCE_CTRL_RRIGHT SCE_CTRL_POWER
#endif

bool touch_zones_window = false;
static bool vflux_window = false;
static bool vflux_enabled = false;
static bool credits_window = false;
static bool debug_window = false;
static bool post_processing_window = false;

extern bool kUpdateTexturesEveryFrame;

static float vcolors[3];

static float *colors;
float *vflux_vertices;
float *vflux_texcoords;

float gamma_val = 1.0f;

int cur_dbg_line = 0;

char *raw_net_romlist = nullptr;

PostProcessingEffect *effects_list = nullptr;
Overlay *overlays_list = nullptr;

Uniform prog_uniforms[8];
int time_unif = -1;

GLuint ff_icon = 0xDEADBEEF;
/*GLuint achievement_icon = 0xDEADBEEF;
void LoadAchievementIcon() {
	if (achievement_icon != 0xDEADBEEF) return;
	
	IO::Filename preview_filename;
	int w, h;
	IO::Path::Combine(preview_filename, DAEDALUS_VITA_PATH("Achievements/Super Mario 64/0/"), "icon.png" );
	uint8_t *icon_data = stbi_load(preview_filename, &w, &h, NULL, 4);
	if (icon_data) {
		glGenTextures(1, &achievement_icon);
		glBindTexture(GL_TEXTURE_2D, achievement_icon);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, icon_data);
		free(icon_data);
	}
}*/

void LoadFastForwardIcon() {
	if (ff_icon != 0xDEADBEEF) return;
	
	IO::Filename preview_filename;
	int w, h;
	IO::Path::Combine(preview_filename, DAEDALUS_VITA_PATH("Resources/"), "ff.png" );
	uint8_t *icon_data = stbi_load(preview_filename, &w, &h, NULL, 4);
	if (icon_data) {
		glGenTextures(1, &ff_icon);
		glBindTexture(GL_TEXTURE_2D, ff_icon);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, icon_data);
		free(icon_data);
	}
}

void loadUniformSettings(const char *path) {
	FILE *config = fopen(path, "r");
	int value, i = 0;
	while (EOF != fscanf(config, "%[^=]=%d\n", prog_uniforms[i].name, &value))
	{
		prog_uniforms[i].idx = glGetUniformLocation(cur_prog, prog_uniforms[i].name);
		prog_uniforms[i].type = (UnifType)value;
		prog_uniforms[i].value[0] = prog_uniforms[i].value[1] = prog_uniforms[i].value[2] = prog_uniforms[i].value[3] = 1.0f;
		i++;
	}

	fclose(config);
	prog_uniforms[i].idx = 0xDEADBEEF;
}

void loadCompiledShader(int idx, char *file)
{
	SceIoStat st;
	sceIoGetstat(file, &st);
	char *code = (char*)malloc(st.st_size);
	
	FILE *f = fopen(file, "rb");
	fread(code, 1, st.st_size, f);
	fclose(f);
	
	glShaderBinary(1, &shaders[idx], 0, code, st.st_size);
	
	free(code);
}

void loadShader(int idx, char *file)
{
	SceIoStat st;
	sceIoGetstat(file, &st);
	char *code = (char*)malloc(st.st_size);
	
	FILE *f = fopen(file, "rb");
	fread(code, 1, st.st_size, f);
	fclose(f);
	
	GLint len = st.st_size - 1;
	glShaderSource(shaders[idx], 1, &code, &len);
	glCompileShader(shaders[idx]);
	
	free(code);
}

void setOverlay(int idx, Overlay *p) {
	if (idx) {
		// Get the required instance if not specified
		if (!p) {
			int i = 0;
			p = overlays_list;
			while (p) {
				if (i == idx) {
					break;
				}
				p = p->next;
				i++;	
			}
		}
	
		if (cur_overlay == 0xDEADBEEF) glGenTextures(1, &cur_overlay);
		glBindTexture(GL_TEXTURE_2D, cur_overlay);
						
		char fpath[128];
		sprintf(fpath, "%s%s.png", DAEDALUS_VITA_PATH_EXT("ux0:", "Overlays/"), p->name);
		int w, h;
		uint8_t *overlay_data = stbi_load(fpath, &w, &h, NULL, 4);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, overlay_data);
		free(overlay_data);
	}
	gOverlay = idx;
}

bool setPostProcessingEffect(int idx, PostProcessingEffect *p) {
	if (gPostProcessing != idx && idx) {
		// Get the required instance if not specified
		if (!p) {
			int i = 0;
			p = effects_list;
			while (p) {
				if (i == idx) {
					break;
				}
				p = p->next;
				i++;	
			}
		}

		shader_idx = (shader_idx + 1) % 2;
		if (program[shader_idx] != 0xDEADBEEF) {
			glDeleteProgram(program[shader_idx]);
			glDeleteShader(shaders[shader_idx * 2]);
			glDeleteShader(shaders[shader_idx * 2 + 1]);
		}
		shaders[shader_idx * 2] = glCreateShader(GL_CG_VERTEX_SHADER_EXT);
		shaders[shader_idx * 2 + 1] = glCreateShader(GL_CG_FRAGMENT_SHADER_EXT);
		program[shader_idx] = glCreateProgram();
						
		char fpath[128];
		if (p->compiled) {
			sprintf(fpath, "%s%s/vert.gxp", DAEDALUS_VITA_PATH_EXT("ux0:", "Shaders/"), p->name);
			loadCompiledShader(shader_idx * 2, fpath);
			sprintf(fpath, "%s%s/frag.gxp", DAEDALUS_VITA_PATH_EXT("ux0:", "Shaders/"), p->name);
			loadCompiledShader(shader_idx * 2 + 1, fpath);
		} else {
			sprintf(fpath, "%s%s/vert.cg", DAEDALUS_VITA_PATH_EXT("ux0:", "Shaders/"), p->name);
			loadShader(shader_idx * 2, fpath);
			sprintf(fpath, "%s%s/frag.cg", DAEDALUS_VITA_PATH_EXT("ux0:", "Shaders/"), p->name);
			loadShader(shader_idx * 2 + 1, fpath);
		}
		glAttachShader(program[shader_idx], shaders[shader_idx * 2]);
		glAttachShader(program[shader_idx], shaders[shader_idx * 2 + 1]);
						
		vglBindAttribLocation(program[shader_idx], 0, "position", 3, GL_FLOAT);
		vglBindAttribLocation(program[shader_idx], 1, "texcoord", 2, GL_FLOAT);
						
		glLinkProgram(program[shader_idx]);
		cur_prog = program[shader_idx];
		glUniform1i(glGetUniformLocation(program[shader_idx], "colorMap"), 0);
		time_unif = glGetUniformLocation(cur_prog, "iTime");
		
		if (p->customizable) {
			sprintf(fpath, "%s%s/unif.txt", DAEDALUS_VITA_PATH_EXT("ux0:", "Shaders/"), p->name);
			loadUniformSettings(fpath);
		} else prog_uniforms[0].idx = 0xDEADBEEF;
	}
	gPostProcessing = idx;
	return p ? p->customizable : false;
}

void install_data_files() {
	extract_file(TEMP_DOWNLOAD_NAME, "ux0:data/");
	sceIoRemove(TEMP_DOWNLOAD_NAME);
	resetRomList();
}

void saveCustomRomPath()
{
	sceIoMkdir(DAEDALUS_VITA_PATH("Configs/"), 0777);
	
	FILE *f = fopen(DAEDALUS_VITA_PATH("Configs/path.ini"), "w+");
	if (f)
	{
		fprintf(f, gCustomRomPath);
		fclose(f);
	}
}

void saveNetRomPath()
{
	sceIoMkdir(DAEDALUS_VITA_PATH("Configs/"), 0777);
	
	FILE *f = fopen(DAEDALUS_VITA_PATH("Configs/path2.ini"), "w+");
	if (f)
	{
		fprintf(f, gNetRomPath);
		fclose(f);
	}
}

void saveConfig(const char *game)
{
	char tmp[128];
	strcpy(tmp, game);
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
		fprintf(config, "%s=%d\n", "gForceLinearFilter", gForceLinearFilter);
		fprintf(config, "%s=%d\n", "gSRGB", (int)gSRGB);
		
		fprintf(config, "%s=%d\n", "gUseMipmaps", gUseMipmaps);
		fprintf(config, "%s=%d\n", "gUseVSync", gUseVSync);
		fprintf(config, "%s=%d\n", "gWaitRendering", gWaitRendering);
		fprintf(config, "%s=%d\n", "gAntiAliasing", gAntiAliasing);
		
		fprintf(config, "%s=%d\n", "gAudioPluginEnabled", (int)gAudioPluginEnabled);
		fprintf(config, "%s=%d\n", "gUseMp3", gUseMp3);
		
		fprintf(config, "%s=%d\n", "gUseExpansionPak", gUseExpansionPak);
		fprintf(config, "%s=%d\n", "gControllerIndex", gControllerIndex);
		
		fprintf(config, "%s=%d\n", "gTexturesDumper", (int)gTexturesDumper);
		fprintf(config, "%s=%d\n", "gUseHighResTextures", (int)gUseHighResTextures);
		fprintf(config, "%s=%d\n", "gPostProcessing", gPostProcessing);
		fprintf(config, "%s=%d\n", "gOverlay", gOverlay);
		fprintf(config, "%s=%d\n", "gRendererType", gRendererType);
		
		fprintf(config, "%s=%d\n", "gSortOrder", gSortOrder);
		fprintf(config, "%s=%d\n", "gUiTheme", gUiTheme);
		fprintf(config, "%s=%d\n", "gHideMenubar", gHideMenubar);
		fprintf(config, "%s=%d\n", "gDisableTouchMenu", (int)gDisableTouchMenu);
		fprintf(config, "%s=%d\n", "gSkipCompatListUpdate", (int)gSkipCompatListUpdate);
		fprintf(config, "%s=%d\n", "gAutoUpdate", (int)gAutoUpdate);
		fprintf(config, "%s=%d\n", "gLanguageIndex", gLanguageIndex);
		fprintf(config, "%s=%d\n", "gBigText", (int)gBigText);
		fprintf(config, "%s=%d\n", "gNetBoot", (int)gNetBoot);
		
		fprintf(config, "%s=%d\n", "gDynarecLoopsOptimisation", (int)gDynarecLoopsOptimisation);
		fprintf(config, "%s=%d\n", "gDynarecWordsOptimisation", (int)gDynarecWordsOptimisation);
		fprintf(config, "%s=%d\n", "gDynarecFastIntegerComparisons", (int)gDynarecFastIntegerComparisons);
		fprintf(config, "%s=%d\n", "gDynarecNativeFPUComparisons", (int)gDynarecNativeFPUComparisons);
		fprintf(config, "%s=%d\n", "gDynarecRelaxedFPRPairing", (int)gDynarecRelaxedFPRPairing);
		fprintf(config, "%s=%d\n", "gDynarecNativeCTC1", (int)gDynarecNativeCTC1);
		fprintf(config, "%s=%d\n", "gDynarecVolatileCodeProtection", (int)gDynarecVolatileCodeProtection);
		fclose(config);
	}
	
	if (!strcmp(game, "default")) {
		showAlert(lang_strings[STR_ALERT_GLOBAL_SETTINGS_SAVE], ALERT_MESSAGE);
	} else {
		showAlert(lang_strings[STR_ALERT_GAME_SETTINGS_SAVE], ALERT_MESSAGE);
	}
}

void save_and_restart_func() {
	saveConfig("default");
	sceAppMgrLoadExec("app0:eboot.bin", NULL, NULL);
}

void dummy_func() {}

void change_custom_rom_path() {
	getDialogTextResult(gCustomRomPath);
	uint32_t path_len = strlen(gCustomRomPath);
	if (gCustomRomPath[path_len - 1] != '/') {
		gCustomRomPath[path_len] = '/';
		gCustomRomPath[path_len + 1] = 0;
	}
	saveCustomRomPath();
	resetRomList();
	custom_path_str_dirty = true;
}

void set_net_folder() {
	if (raw_net_romlist) free(raw_net_romlist);
	raw_net_romlist = (char*)malloc(temp_download_size + 1);
	sceClibMemcpy(raw_net_romlist, rom_mem_buffer, temp_download_size);
	raw_net_romlist[temp_download_size] = 0;
	resetRomList();
}

void change_net_rom_path() {
	getDialogTextResult(gNetRomPath);
	uint32_t path_len = strlen(gNetRomPath);
	if (gNetRomPath[path_len - 1] != '/') {
		gNetRomPath[path_len] = '/';
		gNetRomPath[path_len + 1] = 0;
	}
	saveNetRomPath();
	queueDownload(lang_strings[STR_DLG_RETRIEVE_NET_PATH], gNetRomPath, 1024, set_net_folder, MEM_DOWNLOAD);
	net_path_str_dirty = true;
}

void SetupVFlux() {
	colors = (float*)malloc(sizeof(float)*4*4);
	vflux_vertices = (float*)malloc(sizeof(float)*3*4);
	vflux_texcoords = (float*)malloc(sizeof(float)*2*4);
	vflux_vertices[0] =   0.0f;
	vflux_vertices[1] =   0.0f;
	vflux_vertices[2] =   0.0f;
	vflux_vertices[3] = 960.0f;
	vflux_vertices[4] =   0.0f;
	vflux_vertices[5] =   0.0f;
	vflux_vertices[6] = 960.0f;
	vflux_vertices[7] = 544.0f;
	vflux_vertices[8] =   0.0f;
	vflux_vertices[9] =   0.0f;
	vflux_vertices[10]= 544.0f;
	vflux_vertices[11]=   0.0f;
	vflux_texcoords[0] =  0.0f;
	vflux_texcoords[1] =  0.0f;
	vflux_texcoords[2] =  1.0f;
	vflux_texcoords[3] =  0.0f;
	vflux_texcoords[4] =  1.0f;
	vflux_texcoords[5] =  1.0f;
	vflux_texcoords[6] =  0.0f;
	vflux_texcoords[7] =  1.0f;
}

void SetupPostProcessingLists() {
	// Setting up overlays list
	if (overlays_list == nullptr) {
		overlays_list = (Overlay*)malloc(sizeof(Overlay));
		strcpy(overlays_list->name, lang_strings[STR_UNUSED]);
		overlays_list->next = nullptr;

		IO::FindHandleT		find_handle;
		IO::FindDataT		find_data;

		if (IO::FindFileOpen(DAEDALUS_VITA_PATH_EXT("ux0:", "Overlays/"), &find_handle, find_data )) {
			Overlay *p = overlays_list;
			do {
				const char *filename( find_data.Name );
				if (strstr(filename, ".png")) {
					p->next = (Overlay*)malloc(sizeof(Overlay));
					p = p->next;
					snprintf(p->name, strlen(filename) - 3, filename);
					p->next = nullptr;
				}
			} while(IO::FindFileNext( find_handle, find_data ));
		}
				
		sort_overlaylist(overlays_list->next);
	}
	
	// Setting up post processing effects list
	if (effects_list == nullptr) {
		effects_list = (PostProcessingEffect*)malloc(sizeof(PostProcessingEffect));
		strcpy(effects_list->name, lang_strings[STR_UNUSED]);
		effects_list->next = nullptr;
		effects_list->customizable = false;

		IO::FindHandleT		find_handle;
		IO::FindDataT		find_data;

		if (IO::FindFileOpen(DAEDALUS_VITA_PATH_EXT("ux0:", "Shaders/"), &find_handle, find_data )) {
			PostProcessingEffect *p = effects_list;
			do {
				const char *filename( find_data.Name );
				
				char fpath[256];
				sprintf(fpath, "%s%s/frag.cg", DAEDALUS_VITA_PATH_EXT("ux0:", "Shaders/"), filename);
				SceIoStat stat;
				if (sceIoGetstat(fpath, &stat) >= 0) {
					p->next = (PostProcessingEffect*)malloc(sizeof(PostProcessingEffect));
					p = p->next;
					p->compiled = false;
					strcpy(p->name, filename);
					p->next = nullptr;
				} else {
					sprintf(fpath, "%s%s/frag.gxp", DAEDALUS_VITA_PATH_EXT("ux0:", "Shaders/"), filename);
					if (sceIoGetstat(fpath, &stat) >= 0) {
						p->next = (PostProcessingEffect*)malloc(sizeof(PostProcessingEffect));
						p = p->next;
						p->compiled = true;
						strcpy(p->name, filename);
						p->next = nullptr;
					} else continue;
				}
				sprintf(fpath, "%s%s/unif.txt", DAEDALUS_VITA_PATH_EXT("ux0:", "Shaders/"), filename);
				p->customizable = sceIoGetstat(fpath, &stat) >= 0;
				sprintf(fpath, "%s%s/desc.txt", DAEDALUS_VITA_PATH_EXT("ux0:", "Shaders/"), filename);
				FILE *dh = fopen(fpath, "rb");
				int len = fread(p->desc, 1, 128, dh);
				p->desc[len] = 0;
				fclose(dh);
			} while(IO::FindFileNext( find_handle, find_data ));
		}

		sort_shaderlist(effects_list->next);
	}
}

void SetDescription(const char *text) {
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(text);
}

void DrawExtraMenu() {
	if (ImGui::BeginMenu(lang_strings[STR_MENU_EXTRA])) {
		if (ImGui::MenuItem(lang_strings[STR_MENU_GLOBAL_SETTINGS])) {
			saveConfig("default");
		}
		ImGui::Separator();
		if (ImGui::BeginMenu(lang_strings[STR_MENU_UI_THEME])) {
			if (ImGui::MenuItem(lang_strings[STR_THEME_DARK], nullptr, gUiTheme == DARK_THEME)) {
				setUiTheme(DARK_THEME);
			}
			if (ImGui::MenuItem(lang_strings[STR_THEME_LIGHT], nullptr, gUiTheme == LIGHT_THEME)) {
				setUiTheme(LIGHT_THEME);
			}
			if (ImGui::MenuItem(lang_strings[STR_THEME_CLASSIC], nullptr, gUiTheme == CLASSIC_THEME)) {
				setUiTheme(CLASSIC_THEME);
			}
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem(lang_strings[STR_MENU_MENUBAR], nullptr, gHideMenubar)) {
			gHideMenubar = !gHideMenubar;
		}
		if (ImGui::MenuItem(lang_strings[STR_MENU_DISABLE_TOUCH_MENU], nullptr, gDisableTouchMenu)) {
			gDisableTouchMenu = !gDisableTouchMenu;
		}
		if (ImGui::MenuItem(lang_strings[STR_BIG_TEXT], nullptr, gBigText, gLanguageIndex != SCE_SYSTEM_PARAM_LANG_CHINESE_S)) {
			gBigText = !gBigText;
			fontDirty = true;
		}
		ImGui::Separator();
		if (ImGui::MenuItem(lang_strings[STR_MENU_AUTOUPDATE], nullptr, gAutoUpdate)) {
			gAutoUpdate = !gAutoUpdate;
		}
		if (ImGui::MenuItem(lang_strings[STR_MENU_COMPAT_LIST], nullptr, !gSkipCompatListUpdate)) {
			gSkipCompatListUpdate = !gSkipCompatListUpdate;
		}
		ImGui::Separator();
		if (ImGui::MenuItem(lang_strings[STR_MENU_DEBUGGER], nullptr, debug_window)) {
			debug_window = !debug_window;
		}
		ImGui::Separator();
		if (ImGui::MenuItem(lang_strings[STR_MENU_TEX_DUMPER], nullptr, gTexturesDumper)) {
			gTexturesDumper = !gTexturesDumper;
		}
		SetDescription(lang_strings[STR_DESC_TEX_DUMPER]);
		ImGui::Separator();
		if (ImGui::MenuItem(lang_strings[STR_MENU_CREDITS], nullptr, credits_window)) {
			credits_window = !credits_window;
		}
		ImGui::EndMenu();
	}
}

void DrawCommonMenuBar() {
	SceCtrlPortInfo pinfo;
	sceCtrlGetControllerPortInfo(&pinfo);
	if (ImGui::BeginMenu(lang_strings[STR_MENU_EMULATION])) {
		if (ImGui::BeginMenu("CPU")) {
			if (ImGui::MenuItem(lang_strings[STR_MENU_DYNAREC], nullptr, gCpuMode == CPU_DYNAREC)) {
				setCpuMode(CPU_DYNAREC);
			}
			SetDescription(lang_strings[STR_DESC_DYNAREC]);
			if (ImGui::MenuItem(lang_strings[STR_MENU_CACHED_INTERP], nullptr, gCpuMode == CPU_CACHED_INTERPRETER)) {
				setCpuMode(CPU_CACHED_INTERPRETER);
			}
			SetDescription(lang_strings[STR_DESC_CACHED_INTERP]);
			if (ImGui::MenuItem(lang_strings[STR_MENU_INTERP], nullptr, gCpuMode == CPU_INTERPRETER)) {
				setCpuMode(CPU_INTERPRETER);
			}
			SetDescription(lang_strings[STR_DESC_INTERP]);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(lang_strings[STR_MENU_DYNAREC_CONFIG], gDynarecEnabled && !gUseCachedInterpreter)) {
			if (ImGui::MenuItem(lang_strings[STR_MENU_DYNAREC_WORDS_OPT], nullptr, gDynarecWordsOptimisation)) {
				gDynarecWordsOptimisation = !gDynarecWordsOptimisation;
				CPU_ResetFragmentCache();
			}
			if (ImGui::MenuItem(lang_strings[STR_MENU_DYNAREC_LOOPS_OPT], nullptr, gDynarecLoopsOptimisation)) {
				gDynarecLoopsOptimisation = !gDynarecLoopsOptimisation;
				CPU_ResetFragmentCache();
			}
			if (ImGui::MenuItem(lang_strings[STR_MENU_DYNAREC_INTEGER_OPT], nullptr, gDynarecFastIntegerComparisons)) {
				gDynarecFastIntegerComparisons = !gDynarecFastIntegerComparisons;
				CPU_ResetFragmentCache();
			}
			if (ImGui::MenuItem(lang_strings[STR_MENU_DYNAREC_FPU_CMP_OPT], nullptr, gDynarecNativeFPUComparisons)) {
				gDynarecNativeFPUComparisons = !gDynarecNativeFPUComparisons;
				CPU_ResetFragmentCache();
			}
			if (ImGui::MenuItem(lang_strings[STR_MENU_DYNAREC_FPU_REG_OPT], nullptr, gDynarecRelaxedFPRPairing)) {
				gDynarecRelaxedFPRPairing = !gDynarecRelaxedFPRPairing;
				CPU_ResetFragmentCache();
			}
			if (ImGui::MenuItem(lang_strings[STR_MENU_DYNAREC_CTC1_OPT], nullptr, gDynarecNativeCTC1)) {
				gDynarecNativeCTC1 = !gDynarecNativeCTC1;
				CPU_ResetFragmentCache();
			}
			if (ImGui::MenuItem(lang_strings[STR_MENU_DYNAREC_SMC_OPT], nullptr, gDynarecVolatileCodeProtection)) {
				gDynarecVolatileCodeProtection = !gDynarecVolatileCodeProtection;
				Dynamo_RefreshVolatileCodeProtection();
			}
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem(lang_strings[STR_MENU_HLE], nullptr, gOSHooksEnabled)) {
			gOSHooksEnabled = !gOSHooksEnabled;
		}
		SetDescription(lang_strings[STR_DESC_HLE]);
		if (ImGui::MenuItem("Expansion Pak", nullptr, gUseExpansionPak)) {
				gUseExpansionPak = !gUseExpansionPak;
			}
		ImGui::Separator();
		if (ImGui::MenuItem(lang_strings[STR_MENU_FRAME_LIMIT], nullptr, gSpeedSyncEnabled)) {
			gSpeedSyncEnabled = !gSpeedSyncEnabled;
		}
		SetDescription(lang_strings[STR_DESC_FRAME_LIMIT]);
		if (ImGui::MenuItem(lang_strings[STR_MENU_VIDEO_RATE], nullptr, gVideoRateMatch)) {
			gVideoRateMatch = !gVideoRateMatch;
		}
		SetDescription(lang_strings[STR_DESC_VIDEO_RATE]);
		if (ImGui::MenuItem(lang_strings[STR_MENU_AUDIO_RATE], nullptr, gAudioRateMatch)) {
			gAudioRateMatch = !gAudioRateMatch;
		}
		SetDescription(lang_strings[STR_DESC_AUDIO_RATE]);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu(lang_strings[STR_MENU_GRAPHICS])) {
		if (ImGui::BeginMenu(lang_strings[STR_MENU_ASPECT_RATIO])) {
			if (ImGui::MenuItem("16:9", nullptr, gAspectRatio == RATIO_16_9)) {
				gAspectRatio = RATIO_16_9;
			}
			if (ImGui::MenuItem(lang_strings[STR_MENU_RATIO_UNSTRETCHED], nullptr, gAspectRatio == RATIO_16_9_HACK)) {
				gAspectRatio = RATIO_16_9_HACK;
			}
			if (ImGui::MenuItem("4:3", nullptr, gAspectRatio == RATIO_4_3)) {
				gAspectRatio = RATIO_4_3;
			}
			if (ImGui::MenuItem(lang_strings[STR_MENU_RATIO_ORIGINAL], nullptr, gAspectRatio == RATIO_ORIG)) {
				gAspectRatio = RATIO_ORIG;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(lang_strings[STR_MENU_BRIGHTNESS])) {
			ImGui::SliderFloat("", &gamma_val, 1.0f, 0.0f);
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem(lang_strings[STR_MENU_SRGB], nullptr, gSRGB)) {
			gSRGB = !gSRGB;
			if (!is_main_menu) {
				if (gSRGB) {
					glEnable(GL_FRAMEBUFFER_SRGB);
				} else {
					glDisable(GL_FRAMEBUFFER_SRGB);
				}
			}
		}
		ImGui::Separator();
		if (ImGui::BeginMenu("Renderer")) {
			if (ImGui::MenuItem(lang_strings[STR_MENU_LEGACY_REND], nullptr, gRendererType == RENDERER_LEGACY)) {
				if (!is_main_menu && gRendererType != RENDERER_LEGACY) gRendererChanged = true;
				gRendererType = RENDERER_LEGACY;
			}
			SetDescription(lang_strings[STR_DESC_LEGACY_REND]);
			if (ImGui::MenuItem(lang_strings[STR_MENU_MODERN_REND], nullptr, gRendererType == RENDERER_MODERN)) {
				if (!is_main_menu && gRendererType != RENDERER_MODERN) gRendererChanged = true;
				gRendererType = RENDERER_MODERN;
			}
			SetDescription(lang_strings[STR_DESC_MODERN_REND]);
			if (ImGui::MenuItem(lang_strings[STR_MENU_ACCURATE_REND], nullptr, gRendererType == RENDERER_ACCURATE)) {
				if (!is_main_menu && gRendererType != RENDERER_ACCURATE) gRendererChanged = true;
				gRendererType = RENDERER_ACCURATE;
			}
			SetDescription(lang_strings[STR_DESC_ACCURATE_REND]);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(lang_strings[STR_MENU_TEX_CACHE])) {
			if (ImGui::MenuItem(lang_strings[STR_DISABLED], nullptr, gTexCacheMode == TEX_CACHE_DISABLED)) {
				setTexCacheMode(TEX_CACHE_DISABLED);
			}
			SetDescription(lang_strings[STR_DESC_CACHE_DISABLED]);
			if (ImGui::MenuItem(lang_strings[STR_ACCURATE], nullptr, gTexCacheMode == TEX_CACHE_ACCURATE)) {
				setTexCacheMode(TEX_CACHE_ACCURATE);
			}
			SetDescription(lang_strings[STR_DESC_CACHE_ACCURATE]);
			if (ImGui::MenuItem(lang_strings[STR_FAST], nullptr, gTexCacheMode == TEX_CACHE_FAST)) {
				setTexCacheMode(TEX_CACHE_FAST);
			}
			SetDescription(lang_strings[STR_DESC_CACHE_FAST]);
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem(lang_strings[STR_MENU_BILINEAR], nullptr, gForceLinearFilter)) {
			gForceLinearFilter = !gForceLinearFilter;
		}
		SetDescription(lang_strings[STR_DESC_BILINEAR]);
		if (ImGui::BeginMenu(lang_strings[STR_ANTI_ALIASING])) {
			if (ImGui::MenuItem(lang_strings[STR_DISABLED], nullptr, gAntiAliasing == ANTIALIASING_DISABLED)) {
				if (gAntiAliasing != ANTIALIASING_DISABLED) showDialog(lang_strings[STR_REBOOT_REQ], save_and_restart_func, dummy_func, DIALOG_MESSAGE, NULL);
				gAntiAliasing = ANTIALIASING_DISABLED;
			}
			if (ImGui::MenuItem("MSAA 2x", nullptr, gAntiAliasing == ANTIALIASING_MSAA_2X)) {
				if (gAntiAliasing != ANTIALIASING_MSAA_2X) showDialog(lang_strings[STR_REBOOT_REQ], save_and_restart_func, dummy_func, DIALOG_MESSAGE, NULL);
				gAntiAliasing = ANTIALIASING_MSAA_2X;
			}
			if (ImGui::MenuItem("MSAA 4x", nullptr, gAntiAliasing == ANTIALIASING_MSAA_4X)) {
				if (gAntiAliasing != ANTIALIASING_MSAA_4X) showDialog(lang_strings[STR_REBOOT_REQ], save_and_restart_func, dummy_func, DIALOG_MESSAGE, NULL);
				gAntiAliasing = ANTIALIASING_MSAA_4X;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(lang_strings[STR_MENU_POST_PROCESSING])) {
			PostProcessingEffect *p = effects_list;
			int i = 0;
			while (p) {
				if (ImGui::MenuItem(p->name, nullptr, gPostProcessing == i)) {
					if (setPostProcessingEffect(i, p)) post_processing_window = true;
				}
				if (i) SetDescription(p->desc);
				p = p->next;
				if (!i && p) ImGui::Separator();
				i++;
				
			}
			ImGui::EndMenu();
		}
		SetDescription(lang_strings[STR_DESC_POST_PROCESSING]);
		if (ImGui::BeginMenu(lang_strings[STR_MENU_OVERLAYS])) {
			Overlay *p = overlays_list;
			int i = 0;
			while (p) {
				if (ImGui::MenuItem(p->name, nullptr, gOverlay == i)) {
					setOverlay(i, p);
				}
				p = p->next;
				if (!i && p) ImGui::Separator();
				i++;
			}
			ImGui::EndMenu();
		}
		SetDescription(lang_strings[STR_DESC_OVERLAYS]);
		if (ImGui::MenuItem(lang_strings[STR_MENU_MIPMAPS], nullptr, gUseMipmaps)) {
			gUseMipmaps = !gUseMipmaps;
		}
		SetDescription(lang_strings[STR_DESC_MIPMAPS]);
		if (ImGui::MenuItem(lang_strings[STR_MENU_HIRES_TEX], nullptr, gUseHighResTextures)) {
			gUseHighResTextures = !gUseHighResTextures;
		}
		SetDescription(lang_strings[STR_DESC_HIRES_TEX]);
		ImGui::Separator();
		if (ImGui::MenuItem("vFlux", nullptr, vflux_window)) {
			vflux_window = !vflux_window;
		}
		SetDescription(lang_strings[STR_DESC_VFLUX]);
		if (ImGui::MenuItem("V-Sync", nullptr, gUseVSync)) {
			gUseVSync = gUseVSync == GL_TRUE ? GL_FALSE : GL_TRUE;
			vglWaitVblankStart(gUseVSync);
		}
		ImGui::Separator();
		if (ImGui::MenuItem(lang_strings[STR_MENU_WAIT_REND], nullptr, gWaitRendering)) {
			gWaitRendering = !gWaitRendering;
		}
		SetDescription(lang_strings[STR_DESC_WAIT_REND]);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu(lang_strings[STR_MENU_AUDIO])) {
		if (ImGui::MenuItem(lang_strings[STR_DISABLED], nullptr, gAudioPluginEnabled == APM_DISABLED)) {
			gAudioPluginEnabled = APM_DISABLED;
		}
		if (ImGui::MenuItem(lang_strings[STR_SYNC], nullptr, gAudioPluginEnabled == APM_ENABLED_SYNC)) {
			gAudioPluginEnabled = APM_ENABLED_SYNC;
		}
		if (ImGui::MenuItem(lang_strings[STR_ASYNC], nullptr, gAudioPluginEnabled == APM_ENABLED_ASYNC)) {
			gAudioPluginEnabled = APM_ENABLED_ASYNC;
		}
		ImGui::Separator();
		if (ImGui::MenuItem(lang_strings[STR_MENU_MP3_INSTR], nullptr, !gUseMp3)) {
			gUseMp3 = !gUseMp3;
		}
		SetDescription(lang_strings[STR_DESC_MP3_INSTR]);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu(lang_strings[STR_MENU_INPUT])) {
		if (!sceKernelIsPSVitaTV()) {
			if (ImGui::MenuItem(lang_strings[STR_MENU_REARPAD], nullptr, gUseRearpad)) {
				gUseRearpad = !gUseRearpad;
			}
			SetDescription(lang_strings[STR_DESC_REARPAD]);
		}
		if (ImGui::BeginMenu(lang_strings[STR_MENU_CTRL_MAP])) {
			u32 num_configs = CInputManager::Get()->GetNumConfigurations();
			for (u32 i = 0; i < num_configs; i++) {
				if (ImGui::MenuItem(CInputManager::Get()->GetConfigurationName(i), nullptr, i == gControllerIndex)) {
					CInputManager::Get()->SetConfiguration(i);
				}
				SetDescription(CInputManager::Get()->GetConfigurationDescription(i));
			}
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem("Touch Zone Coordinate Mapping", nullptr, touch_zones_window)) {
			touch_zones_window = !touch_zones_window;
		}
		ImGui::Separator();
		char ctrl_str[64];
		sprintf(ctrl_str, "%s 1", lang_strings[STR_CONTROLLER]);
		if (ImGui::BeginMenu(ctrl_str, pinfo.port[0] != SCE_CTRL_TYPE_UNPAIRED)) {
			if (ImGui::BeginMenu(lang_strings[STR_ACCESSORY])) {
				if (ImGui::MenuItem("Rumble Pak", nullptr, has_rumblepak[0])) {
					has_rumblepak[0] = true;
				}
				if (ImGui::MenuItem("Controller Pak", nullptr, !has_rumblepak[0])) {
					has_rumblepak[0] = false;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		sprintf(ctrl_str, "%s 2", lang_strings[STR_CONTROLLER]);
		if (ImGui::BeginMenu(ctrl_str, pinfo.port[2] != SCE_CTRL_TYPE_UNPAIRED)) {
			if (ImGui::BeginMenu(lang_strings[STR_ACCESSORY])) {
				if (ImGui::MenuItem("Rumble Pak", nullptr, has_rumblepak[1])) {
					has_rumblepak[1] = true;
				}
				if (ImGui::MenuItem("Controller Pak", nullptr, !has_rumblepak[1])) {
					has_rumblepak[1] = false;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		sprintf(ctrl_str, "%s 3", lang_strings[STR_CONTROLLER]);
		if (ImGui::BeginMenu(ctrl_str, pinfo.port[3] != SCE_CTRL_TYPE_UNPAIRED)) {
			if (ImGui::BeginMenu(lang_strings[STR_ACCESSORY])) {
				if (ImGui::MenuItem("Rumble Pak", nullptr, has_rumblepak[2])) {
					has_rumblepak[2] = true;
				}
				if (ImGui::MenuItem("Controller Pak", nullptr, !has_rumblepak[2])) {
					has_rumblepak[2] = false;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		sprintf(ctrl_str, "%s 4", lang_strings[STR_CONTROLLER]);
		if (ImGui::BeginMenu(ctrl_str, pinfo.port[4] != SCE_CTRL_TYPE_UNPAIRED)) {
			if (ImGui::BeginMenu(lang_strings[STR_ACCESSORY])) {
				if (ImGui::MenuItem("Rumble Pak", nullptr, has_rumblepak[3])) {
					has_rumblepak[3] = true;
				}
				if (ImGui::MenuItem("Controller Pak", nullptr, !has_rumblepak[3])) {
					has_rumblepak[3] = false;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu(lang_strings[STR_MENU_LANG])) {
		if (ImGui::MenuItem("Català", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_CATALAN)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_CATALAN);
		}
		if (ImGui::MenuItem("Dansk", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_DANISH)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_DANISH);
		}
		if (ImGui::MenuItem("Deutsch", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_GERMAN)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_GERMAN);
		}
		if (ImGui::MenuItem("English", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_ENGLISH_US)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_ENGLISH_US);
		}
		if (ImGui::MenuItem("Español", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_SPANISH)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_SPANISH);
		}
		if (ImGui::MenuItem("Ελληνικά", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_GREEK)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_GREEK);
		}
		if (ImGui::MenuItem("Français", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_FRENCH)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_FRENCH);
		}
		if (ImGui::MenuItem("Italiano", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_ITALIAN)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_ITALIAN);
		}
		if (ImGui::MenuItem("Polskie", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_POLISH)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_POLISH);
		}
		if (ImGui::MenuItem("Português (BR)", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR);
		}
		if (ImGui::MenuItem("Română", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_ROMANIAN)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_ROMANIAN);
		}
		if (ImGui::MenuItem("Pусский", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_RUSSIAN)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_RUSSIAN);
		}
		if (ImGui::MenuItem("Українська", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_UKRAINIAN)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_UKRAINIAN);
		}
		if (ImGui::MenuItem("Türk", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_TURKISH)) {
			setTranslation(SCE_SYSTEM_PARAM_LANG_TURKISH);
		}
		if (gLanguageIndex == SCE_SYSTEM_PARAM_LANG_JAPANESE || gLanguageIndex == SCE_SYSTEM_PARAM_LANG_RYUKYUAN || gLanguageIndex == SCE_SYSTEM_PARAM_LANG_CHINESE_S) {
			if (ImGui::MenuItem("日本語", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_JAPANESE)) {
				setTranslation(SCE_SYSTEM_PARAM_LANG_JAPANESE);
			}
			if (ImGui::MenuItem("琉球", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_RYUKYUAN)) {
				setTranslation(SCE_SYSTEM_PARAM_LANG_RYUKYUAN);
			}
			if (ImGui::MenuItem("中国人", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_CHINESE_S)) {
				setTranslation(SCE_SYSTEM_PARAM_LANG_CHINESE_S);
			}
		} else {
			if (ImGui::MenuItem("Japanese", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_JAPANESE)) {
				setTranslation(SCE_SYSTEM_PARAM_LANG_JAPANESE);
			}
			if (ImGui::MenuItem("Ryukyuan", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_RYUKYUAN)) {
				setTranslation(SCE_SYSTEM_PARAM_LANG_RYUKYUAN);
			}
			if (ImGui::MenuItem("Chinese", nullptr, gLanguageIndex == SCE_SYSTEM_PARAM_LANG_CHINESE_S)) {
				setTranslation(SCE_SYSTEM_PARAM_LANG_CHINESE_S);
			}
		}
		ImGui::EndMenu();
	}
}

static int selected_zone = 0;

static const char *kButtonNames[] = {
	"N64 A",
	"N64 B",
	"N64 Z Trigger",
	"N64 L Button",
	"N64 R Button",
	"N64 Start",
	"N64 C-Up",
	"N64 C-Down",
	"N64 C-Left",
	"N64 C-Right",
	"N64 D-Pad Up",
	"N64 D-Pad Down",
	"N64 D-Pad Left",
	"N64 D-Pad Right"
};

static const uint32_t kButtonValues[] = {
	SCE_CTRL_CROSS,
	SCE_CTRL_SQUARE,
	SCE_CTRL_L1,
	SCE_CTRL_LTRIGGER,
	SCE_CTRL_R1,
	SCE_CTRL_START,
	SCE_CTRL_RUP,
	SCE_CTRL_RDOWN,
	SCE_CTRL_RLEFT,
	SCE_CTRL_RRIGHT,
	SCE_CTRL_UP,
	SCE_CTRL_DOWN,
	SCE_CTRL_LEFT,
	SCE_CTRL_RIGHT
};

static const int kNumButtons = sizeof(kButtonValues) / sizeof(kButtonValues[0]);

static const char *kShapeNames[] = {
	"Rectangle",
	"Square",
	"Circle"
};

void DrawCanvasEditOverlay() {
	if (!gCanvasEditMode) return;

	ProcessCanvasEditInput();

	if (gSelectedTouchZone < 0 || gSelectedTouchZone >= MAX_TOUCH_ZONES) {
		gSelectedTouchZone = 0;
	}

	// 1. Fully transparent full-screen viewport (960x544) over 100% clean game graphics
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(960, 544), ImGuiCond_Always);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
	                         ImGuiWindowFlags_NoResize |
	                         ImGuiWindowFlags_NoMove |
	                         ImGuiWindowFlags_NoScrollbar |
	                         ImGuiWindowFlags_NoSavedSettings |
	                         ImGuiWindowFlags_NoInputs;

	ImGui::Begin("##ZeroUICanvasEditOverlay", nullptr, flags);

	ImDrawList *draw_list = ImGui::GetWindowDrawList();

	// Render all active touch hitboxes
	for (int z = 0; z < MAX_TOUCH_ZONES; z++) {
		if (!gTouchZones[z].enabled) continue;

		uint8_t shape = gTouchZones[z].shape;
		if (shape > 2) shape = 0;

		float cx = (float)gTouchZones[z].center_x * (960.0f / 1920.0f);
		float cy = (float)gTouchZones[z].center_y * (544.0f / 1088.0f);
		float rw = (float)gTouchZones[z].radius_w * (960.0f / 1920.0f);
		float rh = (float)gTouchZones[z].radius_h * (544.0f / 1088.0f);

		bool is_sel = (z == gSelectedTouchZone);

		ImU32 col_fill = is_sel ? IM_COL32(0, 220, 255, 120) : IM_COL32(255, 180, 0, 75);
		ImU32 col_border = is_sel ? IM_COL32(0, 255, 255, 255) : IM_COL32(255, 200, 0, 220);

		if (shape == HITBOX_CIRCLE) {
			draw_list->AddCircleFilled(ImVec2(cx, cy), rw, col_fill);
			draw_list->AddCircle(ImVec2(cx, cy), rw, col_border, 48, 2.5f);
		} else if (shape == HITBOX_SQUARE) {
			draw_list->AddRectFilled(ImVec2(cx - rw, cy - rw), ImVec2(cx + rw, cy + rw), col_fill);
			draw_list->AddRect(ImVec2(cx - rw, cy - rw), ImVec2(cx + rw, cy + rw), col_border, 0.0f, 15, 2.5f);
		} else { // HITBOX_RECTANGLE
			draw_list->AddRectFilled(ImVec2(cx - rw, cy - rh), ImVec2(cx + rw, cy + rh), col_fill);
			draw_list->AddRect(ImVec2(cx - rw, cy - rh), ImVec2(cx + rw, cy + rh), col_border, 0.0f, 15, 2.5f);
		}

		// Draw center target crosshair
		draw_list->AddLine(ImVec2(cx - 10, cy), ImVec2(cx + 10, cy), IM_COL32(255, 255, 255, 255), 2.0f);
		draw_list->AddLine(ImVec2(cx, cy - 10), ImVec2(cx, cy + 10), IM_COL32(255, 255, 255, 255), 2.0f);

		// Label showing Zone & Button
		char zlabel[64];
		const char *zname = gTouchZones[z].name[0] ? gTouchZones[z].name : "Zone";
		snprintf(zlabel, sizeof(zlabel), "Z%d: %s (%s)", z + 1, zname, kShapeNames[shape]);
		draw_list->AddText(ImVec2(cx - rw + 4, cy - rh - 18), IM_COL32(255, 255, 255, 255), zlabel);
	}

	// Sleek Bottom Control Bar Legend
	draw_list->AddRectFilled(ImVec2(0, 504), ImVec2(960, 544), IM_COL32(10, 10, 15, 210));
	draw_list->AddLine(ImVec2(0, 504), ImVec2(960, 504), IM_COL32(0, 200, 255, 255), 1.5f);

	char legend_str[256];
	if (gTouchZones[gSelectedTouchZone].shape == HITBOX_RECTANGLE) {
		snprintf(legend_str, sizeof(legend_str),
			"[DPAD/L-Stick] Move  |  [TRIANGLE] Size+  [CROSS] Size-  |  [R-Stick] Width/Height (%dx%dpx)  |  [SQUARE] Shape  |  [L1/R1] Zone %d/8  |  [START] Save & Exit",
			gTouchZones[gSelectedTouchZone].radius_w, gTouchZones[gSelectedTouchZone].radius_h, gSelectedTouchZone + 1);
	} else {
		snprintf(legend_str, sizeof(legend_str),
			"[DPAD/L-Stick] Move  |  [TRIANGLE] Size+  [CROSS] Size- (%dpx)  |  [SQUARE] Shape  |  [L1/R1] Zone %d/8  |  [START] Save & Exit",
			gTouchZones[gSelectedTouchZone].radius_w, gSelectedTouchZone + 1);
	}

	draw_list->AddText(ImVec2(12, 516), IM_COL32(255, 255, 255, 255), legend_str);

	ImGui::End();
	ImGui::PopStyleColor();
}

void DrawTouchZonesWindow() {
	if (!touch_zones_window) return;

	// Support physical L1 / R1 buttons for zone switching in mapping window
	SceCtrlData win_pad;
	memset(&win_pad, 0, sizeof(SceCtrlData));
	sceCtrlPeekBufferPositive(0, &win_pad, 1);
	static uint32_t last_win_buttons = 0;
	uint32_t win_pressed = win_pad.buttons & ~last_win_buttons;
	last_win_buttons = win_pad.buttons;

	bool l1_pressed = (win_pressed & (SCE_CTRL_L1 | SCE_CTRL_LTRIGGER)) != 0;
	bool r1_pressed = (win_pressed & (SCE_CTRL_R1 | SCE_CTRL_RTRIGGER)) != 0;

	if (r1_pressed) {
		selected_zone = (selected_zone + 1) % MAX_TOUCH_ZONES;
		gSelectedTouchZone = selected_zone;
	}
	if (l1_pressed) {
		selected_zone = (selected_zone + MAX_TOUCH_ZONES - 1) % MAX_TOUCH_ZONES;
		gSelectedTouchZone = selected_zone;
	}

	ImGui::SetNextWindowSize(ImVec2(720, 540), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Touch Zone Coordinate Mapping", &touch_zones_window)) {
		ImGui::Checkbox("Enable Front Touch Zones", &gTouchZonesActive);
		ImGui::SameLine(500);
		if (ImGui::Button("Save Settings", ImVec2(180, 26))) {
			SaveTouchZonesConfig();
			showAlert("Touch Zones Saved", ALERT_MESSAGE);
		}

		ImGui::Separator();

		// Highlighted prominent action button to visually move and place touch buttons on screen
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.55f, 0.95f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.7f, 1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.45f, 0.85f, 1.0f));

		if (ImGui::Button("POSITION & MOVE BUTTONS ON SCREEN (VISUAL EDITOR)", ImVec2(-1, 36))) {
			gCanvasEditMode = true;
			touch_zones_window = false;
		}

		ImGui::PopStyleColor(3);

		ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "💡 Click this blue button to visually place, move, and resize touch zones over the game screen.");

		ImGui::Separator();

		for (int i = 0; i < MAX_TOUCH_ZONES; i++) {
			char zbtn_lbl[128];
			const char *zname = gTouchZones[i].name[0] ? gTouchZones[i].name : "Zone";
			snprintf(zbtn_lbl, sizeof(zbtn_lbl), "Z%d: %s##zbtn%d", i + 1, gTouchZones[i].enabled ? zname : "[Off]", i);
			if (i % 4 != 0) ImGui::SameLine();
			bool was_selected = (selected_zone == i);
			if (was_selected) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 1.0f, 1.0f));
			}
			if (ImGui::Button(zbtn_lbl, ImVec2(155, 28))) {
				selected_zone = i;
				gSelectedTouchZone = i;
			}
			if (was_selected) {
				ImGui::PopStyleColor();
			}
		}

		ImGui::Separator();

		if (selected_zone < 0 || selected_zone >= MAX_TOUCH_ZONES) selected_zone = 0;
		TouchZone &tz = gTouchZones[selected_zone];
		ImGui::PushID(selected_zone);

		ImGui::Text("Configuring: Zone %d - %s", selected_zone + 1, tz.enabled ? "ENABLED" : "DISABLED");

		ImGui::Checkbox("Enable this zone", &tz.enabled);

		ImGui::Text("Hitbox Shape:");
		for (int s = 0; s < 3; s++) {
			bool is_s_sel = (tz.shape == s);
			if (is_s_sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 1.0f, 1.0f));
			if (s > 0) ImGui::SameLine();
			char shape_btn_lbl[32];
			snprintf(shape_btn_lbl, sizeof(shape_btn_lbl), "%s##shapebtn%d", kShapeNames[s], s);
			if (ImGui::Button(shape_btn_lbl, ImVec2(110, 26))) {
				tz.shape = (uint8_t)s;
			}
			if (is_s_sel) ImGui::PopStyleColor();
		}

		ImGui::Separator();
		ImGui::Text("Select N64 Mapped Button:");

		for (int b = 0; b < kNumButtons; b++) {
			bool is_selected = (tz.mapped_button == kButtonValues[b]);
			if (is_selected) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 1.0f, 1.0f));
			}

			if (b % 4 != 0) ImGui::SameLine();

			char btn_lbl[64];
			snprintf(btn_lbl, sizeof(btn_lbl), "%s##mappedbtn%d", kButtonNames[b], b);
			if (ImGui::Button(btn_lbl, ImVec2(155, 28))) {
				tz.mapped_button = kButtonValues[b];
			}

			if (is_selected) {
				ImGui::PopStyleColor();
			}
		}

		ImGui::Separator();
		ImGui::Text("Center and Size Settings:");

		int cx = tz.center_x;
		int cy = tz.center_y;
		int rw = tz.radius_w;
		int rh = tz.radius_h;

		if (ImGui::SliderInt("Center X (px)", &cx, 0, 1920)) tz.center_x = (uint16_t)cx;
		if (ImGui::SliderInt("Center Y (px)", &cy, 0, 1088)) tz.center_y = (uint16_t)cy;

		if (tz.shape == HITBOX_RECTANGLE) {
			if (ImGui::SliderInt("Width / Radius X (px)", &rw, 10, 800)) tz.radius_w = (uint16_t)rw;
			if (ImGui::SliderInt("Height / Radius Y (px)", &rh, 10, 500)) tz.radius_h = (uint16_t)rh;
		} else {
			if (ImGui::SliderInt("Radius / Size (px)", &rw, 10, 800)) {
				tz.radius_w = (uint16_t)rw;
				tz.radius_h = (uint16_t)rw;
			}
		}

		ImGui::PopID();

		ImGui::End();
	}
}

void DrawCommonWindows() {
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);
	
	DrawTouchZonesWindow();
	DrawCanvasEditOverlay();
	
	if (vflux_window) {
		ImGui::Begin(lang_strings[STR_VFLUX_CONFIG], &vflux_window);
		ImGui::ColorPicker3(lang_strings[STR_VFLUX_COLOR], vcolors);
		ImGui::Checkbox(lang_strings[STR_VFLUX_ENABLE], &vflux_enabled);
		ImGui::End();
	}
	
	if (credits_window) {
		ImGui::Begin(lang_strings[STR_MENU_CREDITS], &credits_window);
		ImGui::TextColored(ImVec4(255, 255, 0, 255), "Daedalus X64 (#%s)", stringify(GIT_VERSION));
		ImGui::Text("%s: Rinnegatamante", lang_strings[STR_CREDITS_AUTHOR]);
		ImGui::Separator();
		ImGui::TextColored(ImVec4(255, 255, 0, 255), lang_strings[STR_CREDITS_PATRONERS]);
		ImGui::Text("mmtechnodrone");
		ImGui::Text("Pspguru");
		ImGui::Text("Shin Megami");
		ImGui::Text("sputnik");
		ImGui::Text("@Sarkies_Proxy");
		ImGui::Text("Badmanwazzy37");
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
		ImGui::Text(lang_strings[STR_CREDITS_9]);
		ImGui::Separator();
		ImGui::TextColored(ImVec4(255, 255, 0, 255), lang_strings[STR_CREDITS_TRANSLATORS]);
		ImGui::Text("Rinnegatamante (ITA)");
		ImGui::Text("Samilop Cimmerian Iter (FRA)");
		ImGui::Text("f2pwn (CAT)");
		ImGui::Text("SamuEDL (ESP)");
		ImGui::Text("S1ngyy (GER)");
		ImGui::Text("Leonardo Lugarinho (BRA)");
		ImGui::Text("coestergaard (DAN)");
		ImGui::Text("Kiiro Yakumo (POL)");
		ImGui::Text("ΧΡΗΣΤΟΣ ΜΑΝΟΥΣΗΣ (GRE)");
		ImGui::Text("nekita13 (RUS)");
		ImGui::Text("rewold20 (TUR)");
		ImGui::Text("CHN-Gen (CHN)");
		ImGui::Text("noru212 (ROM)");
		ImGui::Text("kuragehime (JAP, RYU)");
		ImGui::Text("BARABASDRISTATEL (UKR)");
		ImGui::End();
	}
	
	if (post_processing_window) {
		ImGui::Begin(lang_strings[STR_MENU_POST_PROCESSING], &post_processing_window);
		int i = 0;
		while (prog_uniforms[i].idx != 0xDEADBEEF) {
			switch (prog_uniforms[i].type) {
			case UNIF_FLOAT:
				ImGui::SliderFloat(prog_uniforms[i].name, &prog_uniforms[i].value[0], 0.0f, 2.0f);
				break;
			case UNIF_COLOR:
				ImGui::ColorPicker3(prog_uniforms[i].name, prog_uniforms[i].value);
				break;
			default:
				break;
			}
			i++;
		}
		
		ImGui::End();
	}
	
	if (debug_window) {
		ImGui::Begin(lang_strings[STR_MENU_DEBUGGER], &debug_window);
		ImGui::Text("%s: 0x%04X", lang_strings[STR_CART_ID], g_ROM.rh.CartID);
		ImGui::Text("%s: %s (%s)", lang_strings[STR_GFX_UCODE], cur_gfx_ucode, strlen(cur_gfx_ucode_str) > 0 ? cur_gfx_ucode_str : lang_strings[STR_UNUSED]);
		ImGui::Text("%s: %s", lang_strings[STR_AUDIO_UCODE], cur_audio_ucode);
		ImGui::End();
	}
	
	/*if (achievement_window) {
		bool dummy = true;
		ImGui::SetNextWindowPos(ImVec2(565, 40));
		ImGui::Begin("##achievement", &dummy, ImVec2(350, 70), -1.0f, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
		LoadAchievementIcon();
		ImGui::SetCursorPos(ImVec2(3, 3));
		ImGui::Image((void*)achievement_icon, ImVec2(64, 64));
		ImGui::SetCursorPos(ImVec2(80, 19));
		ImGui::TextColored({1.0f, 1.0f, 0.0f, 1.0f}, "The Journey Begins");
		ImGui::SetCursorPos(ImVec2(80, 35));
		ImGui::Text("Collected your first Star.");
		ImGui::End();
	}*/
	
	if (vflux_enabled) {
		sceClibMemcpy(&colors[0], vcolors, sizeof(float) * 3);
		sceClibMemcpy(&colors[4], vcolors, sizeof(float) * 3);
		sceClibMemcpy(&colors[8], vcolors, sizeof(float) * 3);
		sceClibMemcpy(&colors[12], vcolors, sizeof(float) * 3);
		
		float a;
		SceDateTime time;
		sceRtcGetCurrentClockLocalTime(&time);
		if (time.hour < 6) // Night/Early Morning
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
		vglVertexPointerMapped(3, vflux_vertices);
		vglColorPointerMapped(GL_FLOAT, colors);
		vglDrawObjects(GL_TRIANGLE_FAN, 4);
		glDisableClientState(GL_COLOR_ARRAY);
	}
}

void DrawPendingAlert() {
	if (pendingAlert) {
		uint64_t cur_tick = sceKernelGetProcessTimeWide();
		uint64_t delta = cur_tick - cur_alert.tick;
		if (cur_tick - cur_alert.tick > ALERT_TIME) pendingAlert = false;
		else {
			ImGuiStyle& style = ImGui::GetStyle();
			style.WindowBorderSize = 0.0f;
			ImGui::SetNextWindowPos(ImVec2(5 * UI_SCALE, 544 - 30 * UI_SCALE), ImGuiSetCond_Always);
			ImGui::SetNextWindowSize(ImVec2(SCR_WIDTH, 45), ImGuiSetCond_Always);
			ImGui::SetNextWindowBgAlpha(0.0f);
			ImGui::Begin("Alert Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f - (float)delta / (float)ALERT_TIME), cur_alert.msg);
			ImGui::End();
			style.WindowBorderSize = 1.0f;
		}
	}
}

void DrawFastForwardIcon() {
	LoadFastForwardIcon();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowBorderSize = 0.0f;
	ImGui::SetNextWindowPos(ImVec2(800, 16), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(150, 150), ImGuiSetCond_Always);
	ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::Begin("FastForward Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav);
	ImGui::Image((void*)ff_icon, ImVec2(128, 128));
	ImGui::End();
	style.WindowBorderSize = 1.0f;
}

void DrawPendingDialog() {
	if (pendingDialog) {
		switch(cur_dialog.type) {
		case DIALOG_MESSAGE:
			{
				while (sceMsgDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED) {
					vglSwapBuffers(GL_TRUE);
				}
				SceMsgDialogResult res;
				memset(&res, 0, sizeof(SceMsgDialogResult));
				sceMsgDialogGetResult(&res);
				if (res.buttonId == SCE_MSG_DIALOG_BUTTON_ID_NO) cur_dialog.no_func();
				else if (res.buttonId == SCE_MSG_DIALOG_BUTTON_ID_YES) cur_dialog.yes_func();
				sceMsgDialogTerm();
			}
		case DIALOG_KEYBOARD:
			{
				while (sceImeDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED) {
					vglSwapBuffers(GL_TRUE);
				}
				SceImeDialogResult res;
				memset(&res, 0, sizeof(SceImeDialogResult));
				sceImeDialogGetResult(&res);
				if (res.button == SCE_IME_DIALOG_BUTTON_ENTER) cur_dialog.yes_func();
				else cur_dialog.no_func();
				sceImeDialogTerm();
			}
		default:
			break;
		}
		
		// Common Dialog sets back mode to SCE_CTRL_MODE_DIGITAL apparently
		sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);
	
		pendingDialog = false;
	}
}

void DrawMenuBar() {
	// Checking if a UI scale change is performed
	if (fontDirty) {
		ImGui::GetIO().Fonts->Clear();
		ImGui_ImplVitaGL_InvalidateDeviceObjects();
		reloadFont();
		fontDirty = false;
	}
	
	if (custom_path_str_dirty) {
		sprintf(custom_path_str, "%s: %s", lang_strings[STR_CUSTOM_PATH], strlen(gCustomRomPath) > 1 ? gCustomRomPath : lang_strings[STR_UNUSED]); 
		custom_path_str_dirty = false;
	}
	
	if (net_path_str_dirty) {
		sprintf(net_path_str, "%s: %s", lang_strings[STR_NET_PATH], strlen(gNetRomPath) > 1 ? gNetRomPath : lang_strings[STR_UNUSED]); 
		net_path_str_dirty = false;
	}
	
	ImGui_ImplVitaGL_NewFrame();
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu(lang_strings[STR_MENU_OPTIONS])) {
			if (ImGui::MenuItem(lang_strings[STR_DOWNLOAD_DATA])) {
				queueDownload(lang_strings[STR_DLG_DOWNLOAD_DATA], "https://github.com/Rinnegatamante/DaedalusX64-vitaGL/releases/download/Nightly/DaedalusX64.zip", 26 * 1024 * 1024, install_data_files, FILE_DOWNLOAD);
			}
			ImGui::Separator();
			if (ImGui::MenuItem(custom_path_str)) {
				showDialog(lang_strings[STR_DLG_CUSTOM_PATH], change_custom_rom_path, dummy_func, DIALOG_KEYBOARD, gCustomRomPath);
			}
			if (ImGui::MenuItem(net_path_str)) {
				showDialog(lang_strings[STR_DLG_NET_PATH], change_net_rom_path, dummy_func, DIALOG_KEYBOARD, gNetRomPath);
			}
			if (ImGui::MenuItem(lang_strings[STR_DLG_NET_BOOT], nullptr, gNetBoot)) {
				gNetBoot = !gNetBoot;
			}
			ImGui::Separator();
			if (ImGui::MenuItem(lang_strings[STR_MENU_GAME_SETTINGS], nullptr, false, rom_game_name[0])) {
				saveConfig(rom_game_name);
			}
			if (ImGui::MenuItem(lang_strings[STR_MENU_UNLOAD_GAME_SETTINGS], nullptr, false, rom_game_name[0])) {
				rom_game_name[0] = 0;
				loadConfig("default");
			}
			if (ImGui::BeginMenu(lang_strings[STR_MENU_SORT_ROMS])) {
				if (ImGui::MenuItem(lang_strings[STR_SORT_A_TO_Z], nullptr, gSortOrder == SORT_A_TO_Z)) {
					gSortOrder = SORT_A_TO_Z;
				}
				if (ImGui::MenuItem(lang_strings[STR_SORT_Z_TO_A], nullptr, gSortOrder == SORT_Z_TO_A)) {
					gSortOrder = SORT_Z_TO_A;
				}
				if (ImGui::MenuItem(lang_strings[STR_PLAYTIME_DESC], nullptr, gSortOrder == SORT_PLAYTIME_DESC)) {
					gSortOrder = SORT_PLAYTIME_DESC;
				}
				if (ImGui::MenuItem(lang_strings[STR_PLAYTIME_ASC], nullptr, gSortOrder == SORT_PLAYTIME_ASC)) {
					gSortOrder = SORT_PLAYTIME_ASC;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		DrawCommonMenuBar();
		DrawExtraMenu();
		ImGui::SameLine();
		
		if (calculate_ver_len) {
			calculate_ver_len = false;
			sprintf(ver_str, "#%s", stringify(GIT_VERSION));
			ImVec2 ver_sizes = ImGui::CalcTextSize(ver_str);
			ver_len = ver_sizes.x;
		}
		ImGui::SetCursorPosX(950 - ver_len * UI_SCALE);
		ImGui::Text(ver_str); 
		ImGui::EndMainMenuBar();
	}
	DrawCommonWindows();
}

enum {
	SWAP_STARTED,
	SWAP_REBOOTED,
	SWAP_RELOADED
};

int gSwapState = SWAP_STARTED;
int gSwapRendererType;
int gSwapCounter = 0;

void SwapRenderer() {
	if (gRendererChanged) {
		SceIoStat st;
		if (sceIoGetstat("ux0:data/swap.d64", &st)) {
			// Executing temp savestate
			CPU_RequestSaveState("ux0:data/swap.d64");
			gSwapRendererType = gRendererType;
		} else {
			if (gSwapState == SWAP_STARTED) {
				// Restarting rom
				pause_emu = false;
				restart_rom = true;
				CPU_Halt("");
				gSwapState = SWAP_REBOOTED;
			} else if (gSwapState == SWAP_REBOOTED) {
				// Loading temp savestate and deleting it
				CPU_RequestLoadState("ux0:data/swap.d64");
				gSwapState = SWAP_RELOADED;
			} else {
				gSwapCounter++;
				if (gSwapCounter > 30) {
					sceIoRemove("ux0:data/swap.d64");
					gRendererChanged = false;
					gSwapState = SWAP_STARTED;
					gSwapCounter = 0;
				}
			}
		}
	}
}

void ExecSaveState(int slot) {
	IO::Filename full_path;
	sprintf(full_path, "%s%08x%08x-%02x.ss%ld", DAEDALUS_VITA_PATH("SaveStates/"), g_ROM.mRomID.CRC[0], g_ROM.mRomID.CRC[1], g_ROM.mRomID.CountryID, slot);
	
	CPU_RequestSaveState(full_path);
	cached_saveslots[slot] = true;
	showAlert(lang_strings[STR_ALERT_STATE_SAVE], ALERT_MESSAGE);
}

void LoadSaveState(int slot) {
	IO::Filename full_path;
	sprintf(full_path, "%s%08x%08x-%02x.ss%ld", DAEDALUS_VITA_PATH("SaveStates/"), g_ROM.mRomID.CRC[0], g_ROM.mRomID.CRC[1], g_ROM.mRomID.CountryID, slot);
	
	CPU_RequestLoadState(full_path);
	showAlert(lang_strings[STR_ALERT_STATE_LOAD], ALERT_MESSAGE);
}

void DrawInGameMenuBar() {
	if (!has_cached_saveslots) {
		IO::Directory::EnsureExists(DAEDALUS_VITA_PATH("SaveStates/"));
		for (int i = 0; i <= MAX_SAVESLOT; i++) {
			IO::Filename save_path;
			
			// Translating old savestates names to newer one (Introduced on 28/06/2021)
			IO::Filename old_save_path;
			sprintf(old_save_path, "%s%s.ss%ld", DAEDALUS_VITA_PATH("SaveStates/"), g_ROM.settings.GameName.c_str(), i);
			sprintf(save_path, "%s%08x%08x-%02x.ss%ld", DAEDALUS_VITA_PATH("SaveStates/"), g_ROM.mRomID.CRC[0], g_ROM.mRomID.CRC[1], g_ROM.mRomID.CountryID, i);
			if (IO::File::Exists(old_save_path)) {
				sceIoRename(old_save_path, save_path);
				cached_saveslots[i] = true;
			} else
				cached_saveslots[i] = IO::File::Exists(save_path);
		}
		has_cached_saveslots = true;
	}
	
	// Checking if a UI scale change is performed
	if (fontDirty) {
		ImGui::GetIO().Fonts->Clear();
		ImGui_ImplVitaGL_InvalidateDeviceObjects();
		reloadFont();
		fontDirty = false;
	}
	
	ImGui_ImplVitaGL_NewFrame();
	if (show_menubar) {
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu(lang_strings[STR_MENU_FILES])) {
				if (ImGui::MenuItem(lang_strings[STR_MENU_GAME_SETTINGS])) {
					saveConfig(g_ROM.settings.GameName.c_str());
				}
				ImGui::Separator();
				if (ImGui::BeginMenu(lang_strings[STR_MENU_SAVE_STATE])) {
					for (int i = 0; i <= MAX_SAVESLOT; i++) {
						char tag[8];
						sprintf(tag, "%s %ld", lang_strings[STR_SLOT], i);
						if (ImGui::MenuItem(tag, nullptr, cached_saveslots[i])) {
							ExecSaveState(i);
						}
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu(lang_strings[STR_MENU_LOAD_STATE])) {
					for (int i = 0; i <= MAX_SAVESLOT; i++) {
						char tag[8];
						sprintf(tag, "%s %ld", lang_strings[STR_SLOT], i);
						if (ImGui::MenuItem(tag, nullptr, false, cached_saveslots[i])) {
							LoadSaveState(i);
						}
					}
					ImGui::EndMenu();
				}
				ImGui::Separator();
				if (ImGui::MenuItem(lang_strings[STR_MENU_RESTART_ROM])) {
					pause_emu = false;
					restart_rom = true;
					CPU_Halt(lang_strings[STR_MENU_RESTART_ROM]);
				}
				if (ImGui::MenuItem(lang_strings[STR_MENU_CLOSE_ROM])) {
					pause_emu = false;
					has_cached_saveslots = false;
					CPU_Halt(lang_strings[STR_MENU_CLOSE_ROM]);
				}
				ImGui::EndMenu();
			}
			DrawCommonMenuBar();
			if (codegroupcount > 0) {
				if (ImGui::BeginMenu(lang_strings[STR_MENU_CHEATS])) {
					for (u32 i = 0; i < codegroupcount; i++) {
						if (ImGui::MenuItem(codegrouplist[i].name, nullptr, codegrouplist[i].enable)) {
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
			ImGui::SetCursorPosX(960 - 90 * UI_SCALE);
			ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate); 
			ImGui::EndMainMenuBar();
		}
	}
	
	DrawCommonWindows();
	
	SwapRenderer();
}