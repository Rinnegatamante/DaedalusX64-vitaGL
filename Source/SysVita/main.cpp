#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>

#include <vitasdk.h>
#include <vitaGL.h>
#include <imgui_vita.h>
#include <curl/curl.h>
#include <string.h>

#include "BuildOptions.h"
#include "Config/ConfigOptions.h"
#include "Core/Cheats.h"
#include "Core/CPU.h"
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
#include "Utility/Timer.h"
#include "UI/Menu.h"

#define NET_INIT_SIZE 1*1024*1024
#define TEMP_DOWNLOAD_NAME "ux0:data/DaedalusX64/tmp.bin"

bool gSkipCompatListUpdate = false;
bool gStandaloneMode = true;
bool gIsRomFullPath = false;

extern "C" {
	int32_t sceKernelChangeThreadVfpException(int32_t clear, int32_t set);
	int _newlib_heap_size_user = 160 * 1024 * 1024;
}

extern bool run_emu;
extern bool restart_rom;
extern bool kUpdateTexturesEveryFrame;

static CURL *curl_handle = NULL;
static volatile uint64_t total_bytes = 0xFFFFFFFF;
static volatile uint64_t downloaded_bytes = 0;
static FILE *fh;
char *bytes_string;
static SceUID net_mutex;

int gUseCdram = GL_TRUE;
int gUseVSync = GL_TRUE;

void log2file(const char *format, ...) {
	__gnuc_va_list arg;
	va_start(arg, format);
	char msg[512];
	vsprintf(msg, format, arg);
	va_end(arg);
	sprintf(msg, "%s\n", msg);
	FILE *log = fopen("ux0:/data/DaedalusX64.log", "a+");
	if (log != NULL) {
		fwrite(msg, 1, strlen(msg), log);
		fclose(log);
	}
}

void dump2file(void *ptr, uint32_t size) {
	FILE *log = fopen("ux0:/data/DaedalusX64.dmp", "a+");
	if (log != NULL) {
		fwrite(ptr, 1, size, log);
		fclose(log);
	}
}

static void EnableMenuButtons(bool status) {
	ImGui_ImplVitaGL_GamepadUsage(status);
	ImGui_ImplVitaGL_MouseStickUsage(status);
}

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *stream)
{
	downloaded_bytes += size * nmemb;
	return fwrite(ptr, size, nmemb, fh);
}

// GitHub API does not set Content-Length field
/*static size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata)
{
	if (total_bytes == 0xFFFFFFFF) {
		char *ptr = strcasestr(buffer, "Content-Length");
		if (ptr != NULL) sscanf(ptr, "Content-Length: %llu", &total_bytes);
	}
	return nitems * size;
}*/

static void startDownload(const char *url)
{
	curl_easy_reset(curl_handle);
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36");
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
	curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, bytes_string); // Dummy
	/*curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb);
	curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, bytes_string); // Dummy*/
	curl_easy_setopt(curl_handle, CURLOPT_RESUME_FROM, downloaded_bytes);
	curl_easy_setopt(curl_handle, CURLOPT_BUFFERSIZE, 524288);
	struct curl_slist *headerchunk = NULL;
	headerchunk = curl_slist_append(headerchunk, "Accept: */*");
	headerchunk = curl_slist_append(headerchunk, "Content-Type: application/json");
	headerchunk = curl_slist_append(headerchunk, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36");
	headerchunk = curl_slist_append(headerchunk, "Content-Length: 0");
	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headerchunk);
	curl_easy_perform(curl_handle);
}

static int downloadThread(unsigned int args, void* arg){
	char url[512], dbname[64];
	curl_handle = curl_easy_init();
	for (int i = 1; i <= NUM_DB_CHUNKS; i++) {
		sceKernelWaitSema(net_mutex, 1, NULL);
		sprintf(dbname, "%sdb%ld.json", DAEDALUS_VITA_MAIN_PATH, i);
		sprintf(url, "https://api.github.com/repos/Rinnegatamante/DaedalusX64-vitaGL-Compatibility/issues?state=open&page=%ld&per_page=100", i);
		fh = fopen(TEMP_DOWNLOAD_NAME, "wb");
		int resumes_count = 0;
		downloaded_bytes = 0;

		// FIXME: Workaround since GitHub Api does not set Content-Length
		SceIoStat stat;
		sceIoGetstat(dbname, &stat);
		total_bytes = stat.st_size;

		startDownload(url);

		fclose(fh);
		if (downloaded_bytes > 12 * 1024) {
			sceIoRemove(dbname);
			sceIoRename(TEMP_DOWNLOAD_NAME, dbname);
		} else sceIoRemove(TEMP_DOWNLOAD_NAME);
		downloaded_bytes = total_bytes;
	}
	curl_easy_cleanup(curl_handle);
	sceKernelExitDeleteThread(0);
	return 0;
}

static void Initialize()
{
	strcpy(gDaedalusExePath, DAEDALUS_VITA_PATH(""));
	strcpy(g_DaedalusConfig.mSaveDir, DAEDALUS_VITA_PATH("SaveGames/"));

	// Setting userland maximum clocks
	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);

	// Disabling all FPU exceptions traps on main thread
	sceKernelChangeThreadVfpException(0x0800009FU, 0x0);

	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);

	// Initializing net
	void *net_memory = nullptr;
	SceUID thd;
	if (!gSkipCompatListUpdate) {
		net_mutex = sceKernelCreateSema("Net Mutex", 0, 0, 1, NULL);
		sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
		int ret = sceNetShowNetstat();
		SceNetInitParam initparam;
		if (ret == SCE_NET_ERROR_ENOTINIT) {
			net_memory = malloc(NET_INIT_SIZE);
			initparam.memory = net_memory;
			initparam.size = NET_INIT_SIZE;
			initparam.flags = 0;
			sceNetInit(&initparam);
		}
		sceNetCtlInit();
		thd = sceKernelCreateThread("Net Downloader Thread", &downloadThread, 0x10000100, 0x100000, 0, 0, NULL);
	}
	
	// Initializing vitaGL
	vglInitExtended(0x100000, SCR_WIDTH, SCR_HEIGHT, 0x1800000, SCE_GXM_MULTISAMPLE_4X);
	vglUseVram(gUseCdram);
	vglWaitVblankStart(gUseVSync);

	System_Init();

	// Initializing dear ImGui
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui_ImplVitaGL_Init();
	ImGui_ImplVitaGL_TouchUsage(true);
	ImGui_ImplVitaGL_UseIndirectFrontTouch(true);
	ImGui::StyleColorsDark();
	SetupVFlux();

	// Downloading compatibility databases
	if (!gSkipCompatListUpdate) {
		sceKernelStartThread(thd, 0, NULL);
		for (int i = 1; i <= NUM_DB_CHUNKS; i++) {
			sceKernelSignalSema(net_mutex, 1);
			while (downloaded_bytes < total_bytes) {
				DrawDownloaderScreen(i, downloaded_bytes, total_bytes);
			}
			total_bytes++;
		}
	
		ImGui::GetIO().MouseDrawCursor = true;

		// Closing net
		sceKernelWaitThreadEnd(thd, NULL, NULL);
		sceNetCtlTerm();
		sceNetTerm();
		sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
		free(net_memory);
	}
}

void setCpuMode(int cpu_mode)
{
	switch (cpu_mode)
	{
	case CPU_DYNAREC_UNSAFE:
		gDynarecEnabled = true;
		gUnsafeDynarecOptimisations = true;
		gUseCachedInterpreter = false;
		break;
	case CPU_DYNAREC_SAFE:
		gDynarecEnabled = true;
		gUnsafeDynarecOptimisations = false;
		gUseCachedInterpreter = false;
		break;
	case CPU_CACHED_INTERPRETER:
		gUseCachedInterpreter = true;
		gDynarecEnabled = true;
		break;
	case CPU_INTERPRETER:
		gUseCachedInterpreter = false;
		gDynarecEnabled = false;
		break;
	default:
		break;
	}
	gCpuMode = cpu_mode;
}

void setUiTheme(int theme)
{
	switch (theme) {
	case DARK_THEME:
		ImGui::StyleColorsDark();
		break;
	case LIGHT_THEME:
		ImGui::StyleColorsLight();
		break;
	case CLASSIC_THEME:
		ImGui::StyleColorsClassic();
		break;
	}
	gUiTheme = theme;
}

void setTexCacheMode(int mode)
{
	switch (mode) {
	case TEX_CACHE_DISABLED:
		kUpdateTexturesEveryFrame = true;
		break;
	case TEX_CACHE_ACCURATE:
		kUpdateTexturesEveryFrame = false;
		gCheckTextureHashFrequency = 1;
		break;
	case TEX_CACHE_FAST:
		kUpdateTexturesEveryFrame = false;
		gCheckTextureHashFrequency = 0;
		break;
	}
	gTexCacheMode = mode;
}

void stripGameName(char *name) {
	char *p = nullptr;
	if (p = strstr(name, " (")) {
		p[0] = 0;
	}
}

void loadConfig(const char *game)
{
	char tmp[128];
	sprintf(tmp, game);
	stripGameName(tmp);
	
	char configFile[512];
	char buffer[30];
	int value;
	
	sprintf(configFile, "%s%s.ini", DAEDALUS_VITA_PATH("Configs/"), tmp);
	FILE *config = fopen(configFile, "r");

	if (config)
	{
		while (EOF != fscanf(config, "%[^=]=%d\n", buffer, &value))
		{
			if (strcmp("gCpuMode", buffer) == 0) gCpuMode = value;
			else if (strcmp("gOSHooksEnabled", buffer) == 0) gOSHooksEnabled = value;
			else if (strcmp("gSpeedSyncEnabled", buffer) == 0) gSpeedSyncEnabled = value;
			
			else if (strcmp("gVideoRateMatch", buffer) == 0) gVideoRateMatch = value;
			else if (strcmp("gAudioRateMatch", buffer) == 0) gAudioRateMatch = value;
			else if (strcmp("gAspectRatio", buffer) == 0) gAspectRatio = value;
			else if (strcmp("gTexCacheMode", buffer) == 0) gTexCacheMode = value;
			else if (strcmp("gForceLinearFilter", buffer) == 0) gGlobalPreferences.ForceLinearFilter = value;
			
			else if (strcmp("gUseMipmaps", buffer) == 0) gUseMipmaps = value;
			else if (strcmp("gUseVSync", buffer) == 0) gUseVSync = value;
			else if (strcmp("gUseCdram", buffer) == 0) gUseCdram = value;
			else if (strcmp("gClearDepthFrameBuffer", buffer) == 0) gClearDepthFrameBuffer = value;
			else if (strcmp("gWaitRendering", buffer) == 0) gWaitRendering = value;
			
			else if (strcmp("gAudioPluginEnabled", buffer) == 0) gAudioPluginEnabled = (EAudioPluginMode)value;
			else if (strcmp("gUseMp3", buffer) == 0) gUseMp3 = value;
			
			else if (strcmp("gUseExpansionPak", buffer) == 0) gUseExpansionPak = value;
			else if (strcmp("gControllerIndex", buffer) == 0) gControllerIndex = value;
			
			else if (strcmp("gTexturesDumper", buffer) == 0) gTexturesDumper = (bool)value;
			else if (strcmp("gUseHighResTextures", buffer) == 0) gUseHighResTextures = (bool)value;
			
			else if (strcmp("gSortOrder", buffer) == 0) gSortOrder = value;
			else if (strcmp("gUiTheme", buffer) == 0) gUiTheme = value;
			else if (strcmp("gHideMenubar", buffer) == 0) gHideMenubar = value;
			else if (strcmp("gSkipCompatListUpdate", buffer) == 0) gSkipCompatListUpdate = (bool)value;
		}
		fclose(config);
		
		setUiTheme(gUiTheme);
		setCpuMode(gCpuMode);
		setTexCacheMode(gTexCacheMode);
		vglUseVram(gUseCdram);
		vglWaitVblankStart(gUseVSync);
		CInputManager::Get()->SetConfiguration(gControllerIndex);
	}
}

char boot_params[1024];

int main(int argc, char* argv[])
{
	char *rom;
		
	// We need this to override the compat list update skip option
	loadConfig("default");
		
	// Check if Daedalus X64 has been launched with a custom bubble
	sceAppMgrGetAppParam(boot_params);
	if (strstr(boot_params,"psgm:play")) {
		gSkipCompatListUpdate = true;
		gStandaloneMode = false;
		rom = strstr(boot_params, "&param=") + 7;
		if ((strstr(boot_params, "ux0:") != nullptr) || (strstr(boot_params, "uma0:") != nullptr)) gIsRomFullPath = true;
	}
	
	Initialize();

	while (run_emu) {
		loadConfig("default");
		EnableMenuButtons(true);

		if (restart_rom) restart_rom = false;
		else if (gStandaloneMode) {
			rom = nullptr;
			do {
				rom = DrawRomSelector();
			} while (rom == nullptr);
		}
		
		EnableMenuButtons(false);
		if (!gIsRomFullPath) {
			char fullpath[512];
			sprintf(fullpath, "%s%s", DAEDALUS_VITA_PATH("Roms/"), rom);
			System_Open(fullpath);
		}else System_Open(rom);
		
		loadConfig(g_ROM.settings.GameName.c_str());
		CPU_Run();
		System_Close();
		
		if (!gStandaloneMode && !restart_rom) break;
	}

	System_Finalize();

	return 0;
}
