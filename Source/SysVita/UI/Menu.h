#define NUM_DB_CHUNKS 2

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

extern bool show_menubar;

// Config Variables
extern bool gHideMenubar;
extern int  gUseCdram;
extern int  gUseVSync;
extern int  gCpuMode;
extern int  gTexCacheMode;
extern bool gUseMp3;
extern bool gWaitRendering;
extern bool gUseExpansionPak;
extern bool gUseMipmaps;
extern int  gSortOrder;
extern int  gUiTheme;

char *DrawRomSelector();
void DrawInGameMenu();
void DrawMenuBar();
void DrawInGameMenuBar();
void DrawDownloaderScreen(int index, float downloaded_bytes, float total_bytes);

void SetupVFlux();
void setCpuMode(int cpu_mode);
void setUiTheme(int theme);
void setTexCacheMode(int mode);
void stripGameName(char *name);
