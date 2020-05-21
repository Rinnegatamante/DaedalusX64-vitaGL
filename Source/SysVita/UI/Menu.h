#define NUM_DB_CHUNKS 2

enum {
	SORT_A_TO_Z,
	SORT_Z_TO_A
};

extern bool show_menubar;
extern bool hide_menubar;
extern int use_cdram;
extern int use_vsync;
extern int gCPU;
extern bool use_mp3;
extern bool wait_rendering;
extern bool use_expansion_pak;
extern bool use_mipmaps;
extern int sort_order;

char *DrawRomSelector();
void DrawInGameMenu();
void DrawMenuBar();
void DrawInGameMenuBar();
void DrawDownloaderScreen(int index, float downloaded_bytes, float total_bytes);
void SetupVFlux();
