#include <imgui_vita.h>
#include "stdafx.h"

#define FUNC_TO_NAME(x) #x
#define stringify(x) FUNC_TO_NAME(x)

#define NUM_DB_CHUNKS     2 // Number of pages to download for the compat list
#define NUM_UPDATE_PASSES 2 // Number of passes required to download an update

#define ALERT_TIME 5000000 // Timer for alerts to disappear in microseconds

#define UI_SCALE (gBigText ? 1.5f : 1.0f)

// Auto updater passes
enum {
	UPDATER_CHECK_UPDATES,
	UPDATER_DOWNLOAD_UPDATE
};

// Anti-aliasing modes
enum {
	ANTIALIASING_DISABLED,
	ANTIALIASING_MSAA_2X,
	ANTIALIASING_MSAA_4X
};

// Roms sorting modes
enum {
	SORT_A_TO_Z,
	SORT_Z_TO_A
};

// CPU emulation modes
enum {
	CPU_DYNAREC_UNSAFE,
	CPU_DYNAREC_SAFE,
	CPU_CACHED_INTERPRETER,
	CPU_INTERPRETER
};

// UI themes
enum {
	DARK_THEME,
	LIGHT_THEME,
	CLASSIC_THEME
};

// Texture caching modes
enum {
	TEX_CACHE_DISABLED,
	TEX_CACHE_FAST,
	TEX_CACHE_ACCURATE
};

// Translation strings
#define LANG_STRINGS_NUM 140

#define FOREACH_STR(FUNC) \
	FUNC(STR_DOWNLOADER_COMPAT_LIST) \
	FUNC(STR_DOWNLOADER_UPDATE) \
	FUNC(STR_DOWNLOADER_CHECK_UPDATE) \
	FUNC(STR_UNKNOWN) \
	FUNC(STR_UNUSED) \
	FUNC(STR_USED) \
	FUNC(STR_REQUIRED) \
	FUNC(STR_GAME_NAME) \
	FUNC(STR_REGION) \
	FUNC(STR_CIC_TYPE) \
	FUNC(STR_ROM_SIZE) \
	FUNC(STR_SAVE_TYPE) \
	FUNC(STR_TAGS) \
	FUNC(STR_GAME_PLAYABLE) \
	FUNC(STR_GAME_INGAME_PLUS) \
	FUNC(STR_GAME_INGAME_MINUS) \
	FUNC(STR_GAME_CRASH) \
	FUNC(STR_GAME_SLOW) \
	FUNC(STR_PLAYABLE_DESC) \
	FUNC(STR_INGAME_PLUS_DESC) \
	FUNC(STR_INGAME_MINUS_DESC) \
	FUNC(STR_CRASH_DESC) \
	FUNC(STR_SLOW_DESC) \
	FUNC(STR_MENU_LANG) \
	FUNC(STR_MENU_EXTRA) \
	FUNC(STR_MENU_EMULATION) \
	FUNC(STR_MENU_GRAPHICS) \
	FUNC(STR_MENU_AUDIO) \
	FUNC(STR_MENU_INPUT) \
	FUNC(STR_MENU_OPTIONS) \
	FUNC(STR_MENU_DEBUGGER) \
	FUNC(STR_MENU_CREDITS) \
	FUNC(STR_MENU_GLOBAL_SETTINGS) \
	FUNC(STR_MENU_GAME_SETTINGS) \
	FUNC(STR_MENU_UI_THEME) \
	FUNC(STR_MENU_MENUBAR) \
	FUNC(STR_MENU_AUTOUPDATE) \
	FUNC(STR_MENU_COMPAT_LIST) \
	FUNC(STR_MENU_LOG) \
	FUNC(STR_MENU_TEX_DUMPER) \
	FUNC(STR_MENU_UNSAFE_DYNAREC) \
	FUNC(STR_MENU_SAFE_DYNAREC) \
	FUNC(STR_MENU_CACHED_INTERP) \
	FUNC(STR_MENU_INTERP) \
	FUNC(STR_MENU_HLE) \
	FUNC(STR_MENU_FRAME_LIMIT) \
	FUNC(STR_MENU_VIDEO_RATE) \
	FUNC(STR_MENU_AUDIO_RATE) \
	FUNC(STR_MENU_ASPECT_RATIO) \
	FUNC(STR_MENU_RATIO_UNSTRETCHED) \
	FUNC(STR_MENU_RATIO_ORIGINAL) \
	FUNC(STR_MENU_BRIGHTNESS) \
	FUNC(STR_MENU_TEX_CACHE) \
	FUNC(STR_MENU_BILINEAR) \
	FUNC(STR_MENU_MIPMAPS) \
	FUNC(STR_MENU_HIRES_TEX) \
	FUNC(STR_MENU_VRAM) \
	FUNC(STR_MENU_DEPTH_BUFFER) \
	FUNC(STR_MENU_WAIT_REND) \
	FUNC(STR_MENU_FILES) \
	FUNC(STR_MENU_CHEATS) \
	FUNC(STR_MENU_SAVE_STATE) \
	FUNC(STR_MENU_LOAD_STATE) \
	FUNC(STR_MENU_RESTART_ROM) \
	FUNC(STR_MENU_CLOSE_ROM) \
	FUNC(STR_MENU_SORT_ROMS) \
	FUNC(STR_MENU_MP3_INSTR) \
	FUNC(STR_MENU_REARPAD) \
	FUNC(STR_MENU_CTRL_MAP) \
	FUNC(STR_SORT_A_TO_Z) \
	FUNC(STR_SORT_Z_TO_A) \
	FUNC(STR_DISABLED) \
	FUNC(STR_SLOT) \
	FUNC(STR_VFLUX_CONFIG) \
	FUNC(STR_VFLUX_COLOR) \
	FUNC(STR_VFLUX_ENABLE) \
	FUNC(STR_CREDITS_AUTHOR) \
	FUNC(STR_CREDITS_PATRONERS) \
	FUNC(STR_CREDITS_TRANSLATORS) \
	FUNC(STR_CREDITS_THANKS) \
	FUNC(STR_CREDITS_1) \
	FUNC(STR_CREDITS_2) \
	FUNC(STR_CREDITS_3) \
	FUNC(STR_CREDITS_4) \
	FUNC(STR_CREDITS_5) \
	FUNC(STR_CREDITS_6) \
	FUNC(STR_CREDITS_7) \
	FUNC(STR_CREDITS_8) \
	FUNC(STR_CREDITS_9) \
	FUNC(STR_CART_ID) \
	FUNC(STR_GFX_UCODE) \
	FUNC(STR_AUDIO_UCODE) \
	FUNC(STR_SYNC) \
	FUNC(STR_ASYNC) \
	FUNC(STR_CONTROLLER) \
	FUNC(STR_ACCESSORY) \
	FUNC(STR_ACCURATE) \
	FUNC(STR_FAST) \
	FUNC(STR_DESC_MP3_INSTR) \
	FUNC(STR_DESC_REARPAD) \
	FUNC(STR_DESC_VFLUX) \
	FUNC(STR_DESC_VRAM) \
	FUNC(STR_DESC_DEPTH_BUFFER) \
	FUNC(STR_DESC_WAIT_REND) \
	FUNC(STR_DESC_HIRES_TEX) \
	FUNC(STR_DESC_MIPMAPS) \
	FUNC(STR_DESC_BILINEAR) \
	FUNC(STR_DESC_CACHE_FAST) \
	FUNC(STR_DESC_CACHE_ACCURATE) \
	FUNC(STR_DESC_CACHE_DISABLED) \
	FUNC(STR_DESC_AUDIO_RATE) \
	FUNC(STR_DESC_VIDEO_RATE) \
	FUNC(STR_DESC_FRAME_LIMIT) \
	FUNC(STR_DESC_HLE) \
	FUNC(STR_DESC_INTERP) \
	FUNC(STR_DESC_CACHED_INTERP) \
	FUNC(STR_DESC_SAFE_DYNAREC) \
	FUNC(STR_DESC_UNSAFE_DYNAREC) \
	FUNC(STR_DESC_TEX_DUMPER) \
	FUNC(STR_THEME_DARK) \
	FUNC(STR_THEME_LIGHT) \
	FUNC(STR_THEME_CLASSIC) \
	FUNC(STR_REGION_GER) \
	FUNC(STR_REGION_USA) \
	FUNC(STR_REGION_FRA) \
	FUNC(STR_REGION_ITA) \
	FUNC(STR_REGION_JAP) \
	FUNC(STR_REGION_EUR) \
	FUNC(STR_REGION_ESP) \
	FUNC(STR_REGION_AUS) \
	FUNC(STR_ANTI_ALIASING) \
	FUNC(STR_REBOOT_REQ) \
	FUNC(STR_ALERT_GLOBAL_SETTINGS_SAVE) \
	FUNC(STR_ALERT_GAME_SETTINGS_SAVE) \
	FUNC(STR_ALERT_GLOBAL_SETTINGS_LOAD) \
	FUNC(STR_ALERT_GAME_SETTINGS_LOAD) \
	FUNC(STR_ALERT_STATE_SAVE) \
	FUNC(STR_ALERT_STATE_LOAD) \
	FUNC(STR_BIG_TEXT) \
	FUNC(STR_ROM_LAUNCH)

#define GET_VALUE(x) x,
#define GET_STRING(x) #x,

enum {
	FOREACH_STR(GET_VALUE)
};

extern char *lang_identifiers[];

extern char lang_strings[][256];
extern bool show_menubar;

// Dialog types
enum {
	DIALOG_MESSAGE
};

// Alert types
enum {
	ALERT_MESSAGE
};

struct Dialog {
	void (*yes_func)();
	void (*no_func)();
	uint8_t type;
};

struct Alert {
	char msg[256];
	uint64_t tick;
	uint8_t type;
};

extern Dialog cur_dialog;
extern Alert cur_alert;

// Language identifiers for languages not supported by PSVITA nativeely
#define SCE_SYSTEM_PARAM_LANG_CATALAN 20

// Config Variables
extern bool gHideMenubar;
extern int  gLanguageIndex;
extern int  gUseCdram;
extern int  gUseVSync;
extern int  gCpuMode;
extern int  gTexCacheMode;
extern bool gUseMp3;
extern bool gWaitRendering;
extern bool gUseExpansionPak;
extern bool gUseHighResTextures;
extern bool gTexturesDumper;
extern bool gUseMipmaps;
extern bool gSkipCompatListUpdate;
extern bool gAutoUpdate;
extern bool gUseRearpad;
extern int  gSortOrder;
extern int  gUiTheme;
extern int  gAntiAliasing;
extern bool gBigText;

extern bool pendingDialog;
extern bool pendingAlert;

char *DrawRomSelector();
void DrawInGameMenu();
void DrawMenuBar();
void DrawInGameMenuBar();
void DrawDownloaderScreen(int index, float downloaded_bytes, float total_bytes, char *text, int passes);
void DrawPendingDialog();
void DrawPendingAlert();

void EnableMenuButtons(bool status);
void SetupVFlux();
void setCpuMode(int cpu_mode);
void setUiTheme(int theme);
void setTranslation(int idx);
void setTexCacheMode(int mode);
void stripGameName(char *name);
void showDialog(char *text, void (*yes_func)(), void (*no_func)());
void showAlert(char *text, int type);
