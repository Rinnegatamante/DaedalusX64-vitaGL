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

extern "C" {

int32_t sceKernelChangeThreadVfpException(int32_t clear, int32_t set);
int _newlib_heap_size_user = 128 * 1024 * 1024;

}

extern bool run_emu;
extern bool restart_rom;

static CURL *curl_handle = NULL;
static uint64_t total_bytes = 0xFFFFFFFF;
static uint64_t downloaded_bytes = 0;
static FILE *fh;
char *bytes_string;

int use_cdram = GL_TRUE;
int use_vsync = GL_TRUE;

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

static size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata)
{
	if (total_bytes == 0xFFFFFFFF) {
		char *ptr = strcasestr(buffer, "Content-Length");
		if (ptr != NULL) sscanf(ptr, "Content-Length: %llu", &total_bytes);
	}
	return nitems * size;
}

static void resumeDownload(const char *url)
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
	curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb);
	curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, bytes_string); // Dummy
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

static void startDownload(const char *filename, const char *url){
	curl_handle = curl_easy_init();
	fh = fopen(TEMP_DOWNLOAD_NAME, "wb");
	total_bytes = 0xFFFFFFFF;
	downloaded_bytes = 0;
	while (downloaded_bytes < total_bytes) {
		resumeDownload(url);
	}
	
	if (downloaded_bytes > 12 * 1024) {
		sceIoRemove(filename);
		sceIoRename(TEMP_DOWNLOAD_NAME, filename);
	} else sceIoRemove(TEMP_DOWNLOAD_NAME);
	
	fclose(fh);
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
	
	// Initializing net
	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	int ret = sceNetShowNetstat();
	void *net_memory = nullptr;
	SceNetInitParam initparam;
	if (ret == SCE_NET_ERROR_ENOTINIT) {
		net_memory = malloc(NET_INIT_SIZE);
		initparam.memory = net_memory;
		initparam.size = NET_INIT_SIZE;
		initparam.flags = 0;
		sceNetInit(&initparam);
	}
	sceNetCtlInit();
	
	// Downloading compatibility databases
	startDownload("ux0:data/DaedalusX64/db1.json", "https://api.github.com/repos/Rinnegatamante/DaedalusX64-vitaGL-Compatibility/issues?state=open&page=1&per_page=100");
	startDownload("ux0:data/DaedalusX64/db2.json", "https://api.github.com/repos/Rinnegatamante/DaedalusX64-vitaGL-Compatibility/issues?state=open&page=2&per_page=100");
	
	// Closing net
	curl_easy_cleanup(curl_handle);
	sceNetCtlTerm();
	sceNetTerm();
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
	free(net_memory);
	
	// Disabling all FPU exceptions traps on main thread
	sceKernelChangeThreadVfpException(0x0800009FU, 0x0);
	
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
	
	vglInitExtended(0x100000, SCR_WIDTH, SCR_HEIGHT, 0x1800000, SCE_GXM_MULTISAMPLE_4X);
	vglUseVram(use_cdram);
	vglWaitVblankStart(use_vsync);
	
	System_Init();
	
	// TODO: Move this somewhere else
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui_ImplVitaGL_Init();
	ImGui_ImplVitaGL_TouchUsage(true);
	ImGui_ImplVitaGL_UseIndirectFrontTouch(true);
	ImGui::StyleColorsDark();
	SetupVFlux();
}

int main(int argc, char* argv[])
{
	Initialize();
	
	char *rom;
	
	while (run_emu) {
		EnableMenuButtons(true);
		
		if (restart_rom) restart_rom = false;
		else {
			rom = nullptr;
			do {
				rom = DrawRomSelector();
			} while (rom == nullptr);
		}
		
		char fullpath[512];
		sprintf(fullpath, "%s%s", DAEDALUS_VITA_PATH("Roms/"), rom);
		EnableMenuButtons(false);
		System_Open(fullpath);
		CPU_Run();
		System_Close();
	}
	
	System_Finalize();

	return 0;
}
