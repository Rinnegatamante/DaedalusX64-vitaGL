#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>

#include <iostream>
#include <string>
#include <locale>
#include <codecvt>

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
#include "minizip/unzip.h"

#define NET_INIT_SIZE 1*1024*1024
#define TEMP_DOWNLOAD_NAME "ux0:data/DaedalusX64/tmp.bin"

bool gSkipCompatListUpdate = false;
bool gStandaloneMode = true;
bool gAutoUpdate = true;
char gCustomRomPath[256] = {0};

static char fname[512], ext_fname[512], read_buffer[8192];

int console_language;

Dialog cur_dialog;
Alert cur_alert;

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
static volatile uint8_t downloader_pass = 1;
static FILE *fh;
char *bytes_string;
static SceUID net_mutex;
static bool sys_initialized = false;

int gUseCdram = GL_TRUE;
int gUseVSync = GL_TRUE;

bool pendingDialog = false;
bool pendingAlert = false;

char boot_params[1024];

void recursive_mkdir(char *dir) {
	char *p = dir;
	while (p) {
		char *p2 = strstr(p, "/");
		if (p2) {
			p2[0] = 0;
			sceIoMkdir(dir, 0777);
			p = p2 + 1;
			p2[0] = '/';
		} else break;
	}
}

void extract_file(char *file, char *dir) {
	unz_global_info global_info;
	unz_file_info file_info;
	unzFile zipfile = unzOpen(file);
	unzGetGlobalInfo(zipfile, &global_info);
	unzGoToFirstFile(zipfile);
	uint64_t total_extracted_bytes;
	uint64_t curr_extracted_bytes = 0;
	uint64_t curr_file_bytes = 0;
	int num_files = global_info.number_entry;
	for (int zip_idx = 0; zip_idx < num_files; ++zip_idx) {
		unzGetCurrentFileInfo(zipfile, &file_info, fname, 512, NULL, 0, NULL, 0);
		total_extracted_bytes += file_info.uncompressed_size;
		if ((zip_idx + 1) < num_files) unzGoToNextFile(zipfile);
	}
	unzGoToFirstFile(zipfile);
	for (int zip_idx = 0; zip_idx < num_files; ++zip_idx) {
		unzGetCurrentFileInfo(zipfile, &file_info, fname, 512, NULL, 0, NULL, 0);
		sprintf(ext_fname, "%s%s", dir, fname); 
		const size_t filename_length = strlen(ext_fname);
		if (ext_fname[filename_length - 1] != '/') {
			curr_file_bytes = 0;
			unzOpenCurrentFile(zipfile);
			recursive_mkdir(ext_fname);
			FILE *f = fopen(ext_fname, "wb");
			while (curr_file_bytes < file_info.uncompressed_size) {
				int rbytes = unzReadCurrentFile(zipfile, read_buffer, 8192);
				if (rbytes > 0) {
					fwrite(read_buffer, 1, rbytes, f);
					curr_extracted_bytes += rbytes;
					curr_file_bytes += rbytes;
				}
				DrawExtractorScreen(zip_idx + 1, curr_file_bytes, curr_extracted_bytes, file_info.uncompressed_size, total_extracted_bytes, fname, num_files);
			}
			fclose(f);
			unzCloseCurrentFile(zipfile);
		}
		if ((zip_idx + 1) < num_files) unzGoToNextFile(zipfile);
	}
	unzClose(zipfile);
}

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

void EnableMenuButtons(bool status) {
	ImGui_ImplVitaGL_GamepadUsage(status);
	ImGui_ImplVitaGL_MouseStickUsage(status);
}

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *stream)
{
	downloaded_bytes += size * nmemb;
	if (total_bytes < downloaded_bytes) total_bytes = downloaded_bytes;
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

static int compatListThread(unsigned int args, void* arg){
	char url[512], dbname[64];
	curl_handle = curl_easy_init();
	for (int i = 1; i <= NUM_DB_CHUNKS; i++) {
		downloader_pass = i;
		sceKernelWaitSema(net_mutex, 1, NULL);
		sprintf(dbname, "%sdb%ld.json", DAEDALUS_VITA_MAIN_PATH, i);
		sprintf(url, "https://api.github.com/repos/Rinnegatamante/DaedalusX64-vitaGL-Compatibility/issues?state=open&page=%ld&per_page=100", i);
		fh = fopen(TEMP_DOWNLOAD_NAME, "wb");
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

static int updaterThread(unsigned int args, void* arg){
	uint8_t update_detected = 0;
	char url[512];
	curl_handle = curl_easy_init();
	for (int i = UPDATER_CHECK_UPDATES; i < NUM_UPDATE_PASSES; i++) {
		downloader_pass = i;
		sceKernelWaitSema(net_mutex, 1, NULL);
		if (i == UPDATER_CHECK_UPDATES) sprintf(url, "https://api.github.com/repos/Rinnegatamante/DaedalusX64-vitaGL/releases/latest");
		else if (!update_detected) {
			downloaded_bytes = total_bytes;
			break;
		}
		fh = fopen(TEMP_DOWNLOAD_NAME, "wb");
		downloaded_bytes = 0;

		// FIXME: Workaround since GitHub Api does not set Content-Length
		total_bytes = i == UPDATER_CHECK_UPDATES ? 20 * 1024 : 2 * 1024 * 1024; /* 20 KB / 2 MB */

		startDownload(url);

		fclose(fh);
		if (downloaded_bytes > 12 * 1024) {
			if (i == UPDATER_CHECK_UPDATES) {
				fh = fopen(TEMP_DOWNLOAD_NAME, "rb");
				fseek(fh, 0, SEEK_END);
				uint32_t size = ftell(fh);
				fseek(fh, 0, SEEK_SET);
				char *buffer = (char*)malloc(size);
				fread(buffer, 1, size, fh);
				fclose(fh);
				sceIoRemove(TEMP_DOWNLOAD_NAME);
				if (strncmp(strstr(buffer, "target_commitish") + 20, stringify(GIT_VERSION), 6)) {
					sprintf(url, "https://github.com/Rinnegatamante/DaedalusX64-vitaGL/releases/download/Nightly/DaedalusX64.vpk");
					update_detected = 1;
				}
			}
		} else sceIoRemove(TEMP_DOWNLOAD_NAME);
		downloaded_bytes = total_bytes;
	}
	curl_easy_cleanup(curl_handle);
	sceKernelExitDeleteThread(0);
	return 0;
}

void reloadFont() {
	static const ImWchar ranges[] = {
		0x0020, 0x00FF, // Basic Latin + Latin Supplement
		0x0100, 0x024F, // Latin Extended
		0x0370, 0x03FF, // Greek
		0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
		0x1E00, 0x1EFF, // Latin Extended Additional
		0x2DE0, 0x2DFF, // Cyrillic Extended-A
		0xA640, 0xA69F, // Cyrillic Extended-B
		0,
	};
	
	ImGui::GetIO().Fonts->AddFontFromFileTTF("app0:/Roboto.ttf", 16.0f * UI_SCALE, NULL, ranges);
}

static void Initialize()
{
	sys_initialized = true;
	
	strcpy(gDaedalusExePath, DAEDALUS_VITA_PATH(""));
	strcpy(g_DaedalusConfig.mSaveDir, DAEDALUS_VITA_PATH("SaveGames/"));

	// Setting userland maximum clocks
	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);
	
	// Initializing sceCommonDialog
	SceCommonDialogConfigParam cmnDlgCfgParam;
	sceCommonDialogConfigParamInit(&cmnDlgCfgParam);
	cmnDlgCfgParam.language = (SceSystemParamLang)console_language;
	sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_ENTER_BUTTON, (int *)&cmnDlgCfgParam.enterButtonAssign);
	sceCommonDialogSetConfigParam(&cmnDlgCfgParam);

	// Disabling all FPU exceptions traps on main thread
	sceKernelChangeThreadVfpException(0x0800009FU, 0x0);

	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
	sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);
	sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
	
	// Turn off auto updater if build is marked as dirty
	uint8_t gSkipAutoUpdate = strstr(stringify(GIT_VERSION), "dirty") != nullptr;
	
	// Initializing net
	void *net_memory = nullptr;
	if ((gAutoUpdate && !gSkipAutoUpdate) || !gSkipCompatListUpdate) {
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
	}
	
	// Initializing vitaGL
	vglInitExtended(0x100000, SCR_WIDTH, SCR_HEIGHT, 0x1800000, (SceGxmMultisampleMode)gAntiAliasing);
	vglUseVram(gUseCdram);
	vglWaitVblankStart(gUseVSync);

	System_Init();

	// Initializing dear ImGui
	ImGui::CreateContext();
	reloadFont();
	ImGui_ImplVitaGL_Init();
	ImGui_ImplVitaGL_TouchUsage(true);
	ImGui_ImplVitaGL_UseIndirectFrontTouch(true);
	ImGui::StyleColorsDark();
	SetupVFlux();
	
	SceKernelThreadInfo info;
	info.size = sizeof(SceKernelThreadInfo);
	int res = 0;
	
	// Checking for updates
	if (gAutoUpdate && !gSkipAutoUpdate) {
		SceUID thd = sceKernelCreateThread("Auto Updater", &updaterThread, 0x10000100, 0x100000, 0, 0, NULL);
		sceKernelStartThread(thd, 0, NULL);
		do {
			sceKernelSignalSema(net_mutex, 1);
			if (downloader_pass == UPDATER_CHECK_UPDATES) DrawDownloaderScreen(downloader_pass + 1, downloaded_bytes, total_bytes, lang_strings[STR_DOWNLOADER_CHECK_UPDATE], NUM_UPDATE_PASSES);
			else DrawDownloaderScreen(downloader_pass + 1, downloaded_bytes, total_bytes, lang_strings[STR_DOWNLOADER_UPDATE], NUM_UPDATE_PASSES);
			res = sceKernelGetThreadInfo(thd, &info);
		} while (info.status <= SCE_THREAD_STOPPED && res >= 0);
		total_bytes = 0xFFFFFFFF;
		downloaded_bytes = 0;
		downloader_pass = 1;
		
		// Found an update, extracting and installing it
		FILE *f = fopen(TEMP_DOWNLOAD_NAME, "r");
		if (f) {
			sceAppMgrUmount("app0:");
			fclose(f);
			extract_file(TEMP_DOWNLOAD_NAME, "ux0:app/DEDALOX64/");
			sceIoRemove(TEMP_DOWNLOAD_NAME);
			sceAppMgrLoadExec("app0:eboot.bin", NULL, NULL);
		}
	}

	// Downloading compatibility databases
	if (!gSkipCompatListUpdate) {
		SceUID thd = sceKernelCreateThread("Compat List Updater", &compatListThread, 0x10000100, 0x100000, 0, 0, NULL);
		sceKernelStartThread(thd, 0, NULL);
		do {
			sceKernelSignalSema(net_mutex, 1);
			DrawDownloaderScreen(downloader_pass, downloaded_bytes, total_bytes, lang_strings[STR_DOWNLOADER_COMPAT_LIST], NUM_DB_CHUNKS);
			res = sceKernelGetThreadInfo(thd, &info);
		} while (info.status <= SCE_THREAD_STOPPED && res >= 0);
	}
	
	if ((gAutoUpdate && !gSkipAutoUpdate) || !gSkipCompatListUpdate) {
		ImGui::GetIO().MouseDrawCursor = true;

		// Closing net
		sceNetCtlTerm();
		sceNetTerm();
		sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
		free(net_memory);
		sceKernelDeleteSema(net_mutex);
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

void showAlert(char *text, int type) {
	cur_alert.type = type;
	sprintf(cur_alert.msg, text);
	cur_alert.tick = sceKernelGetProcessTimeWide();

	pendingAlert = true;
}

static uint16_t dialog_res_text[SCE_IME_DIALOG_MAX_TEXT_LENGTH];
void getDialogTextResult(char *text) {
	// Converting text from UTF16 to UTF8
	std::u16string utf16_str = (char16_t*)dialog_res_text;
	std::string utf8_str = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.to_bytes(utf16_str.data());
	sprintf(text, utf8_str.c_str());
}

void showDialog(char *text, void (*yes_func)(), void (*no_func)(), int type) {
	if (pendingDialog) return;
	
	cur_dialog.type = type;
	cur_dialog.yes_func = yes_func;
	cur_dialog.no_func = no_func;
	
	switch (cur_dialog.type) {
	case DIALOG_MESSAGE:
		{
			SceMsgDialogParam params;
			SceMsgDialogUserMessageParam msg_params;
	
			sceMsgDialogParamInit(&params);
			params.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
			params.userMsgParam = &msg_params;
	
			memset(&msg_params, 0, sizeof(SceMsgDialogUserMessageParam));
			msg_params.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_YESNO;
			msg_params.msg = (const SceChar8*)text;
	
			sceMsgDialogInit(&params);
		}
	case DIALOG_KEYBOARD:	
		{
			SceImeDialogParam params;
	
			sceImeDialogParamInit(&params);
			params.type = SCE_IME_TYPE_BASIC_LATIN;
			
			// Converting text from UTF8 to UTF16
			std::string utf8_str = text;
			std::u16string utf16_str = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(utf8_str.data());
			params.title = (const SceWChar16*)utf16_str.c_str();
			
			memset(dialog_res_text, 0, sizeof(dialog_res_text));
			params.initialText = dialog_res_text;
			params.inputTextBuffer = dialog_res_text;
			
			params.maxTextLength = SCE_IME_DIALOG_MAX_TEXT_LENGTH;
			
			sceImeDialogInit(&params);
		}
	default:
		break;
	}
	
	pendingDialog = true;
}

void setTranslation(int idx) {
	char langFile[512];
	char identifier[64], buffer[128];
	
	switch (idx) {
	case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR: // Brazilian Portuguese
		sprintf(langFile, "%sPortugueseBR.ini", DAEDALUS_VITA_PATH("Languages/"));
		break;
	case SCE_SYSTEM_PARAM_LANG_CATALAN: // Catalan
		sprintf(langFile, "%sCatalà.ini", DAEDALUS_VITA_PATH("Languages/"));
		break;
	case SCE_SYSTEM_PARAM_LANG_DANISH: // Danish
		sprintf(langFile, "%sDanish.ini", DAEDALUS_VITA_PATH("Languages/"));
		break;
	case SCE_SYSTEM_PARAM_LANG_FRENCH: // French
		sprintf(langFile, "%sFrench.ini", DAEDALUS_VITA_PATH("Languages/"));
		break;
	case SCE_SYSTEM_PARAM_LANG_GERMAN: // German
		sprintf(langFile, "%sDeutsch.ini", DAEDALUS_VITA_PATH("Languages/"));
		break;
	case SCE_SYSTEM_PARAM_LANG_ITALIAN: // Italiano
		sprintf(langFile, "%sItaliano.ini", DAEDALUS_VITA_PATH("Languages/"));
		break;
	case SCE_SYSTEM_PARAM_LANG_SPANISH: // Spanish
		sprintf(langFile, "%sSpanish.ini", DAEDALUS_VITA_PATH("Languages/"));
		break;
	case SCE_SYSTEM_PARAM_LANG_POLISH: // Polish
		sprintf(langFile, "%sPolish.ini", DAEDALUS_VITA_PATH("Languages/"));
		break;
	case SCE_SYSTEM_PARAM_LANG_GREEK: // Greek
		sprintf(langFile, "%sGreek.ini", DAEDALUS_VITA_PATH("Languages/"));
		break;
	default: // English
		sprintf(langFile, "%sEnglish.ini", DAEDALUS_VITA_PATH("Languages/"));
		break;
	}
	
	FILE *config = fopen(langFile, "r");
	if (config)
	{
		while (EOF != fscanf(config, "%[^=]=%[^\n]\n", identifier, buffer))
		{
			for (int i = 0; i < LANG_STRINGS_NUM; i++) {
				if (strcmp(lang_identifiers[i], identifier) == 0) {
					char *newline = nullptr, *p = buffer;
					while (newline = strstr(p, "\\n")) {
						newline[0] = '\n';
						sprintf(&newline[1], &newline[2]);
						p++;
					}
					sprintf(lang_strings[i], buffer);
				}
			}
		}
		fclose(config);
		gLanguageIndex = idx;
	} else if (sys_initialized) DBGConsole_Msg(0, "Cannot find language file.");
	
	custom_path_str_dirty = true;
}

void preloadConfig()
{
	char configFile[512];
	char buffer[30];
	int value;
	
	sprintf(configFile, "%sdefault.ini", DAEDALUS_VITA_PATH("Configs/"));
	FILE *config = fopen(configFile, "r");
	
	if (config)
	{
		while (EOF != fscanf(config, "%[^=]=%d\n", buffer, &value))
		{
			if (strcmp("gSkipCompatListUpdate", buffer) == 0) gSkipCompatListUpdate = (bool)value;
			else if (strcmp("gAutoUpdate", buffer) == 0) gAutoUpdate = (bool)value;
			else if (strcmp("gLanguageIndex", buffer) == 0) gLanguageIndex = value;
			else if (strcmp("gAntiAliasing", buffer) == 0) gAntiAliasing = value;
			else if (strcmp("gBigText", buffer) == 0) gBigText = (bool)value;
		}
		fclose(config);
		
		setTranslation(gLanguageIndex);
	} else setTranslation(console_language);
}

void loadCustomRomPath()
{
	FILE *f = fopen(DAEDALUS_VITA_PATH("Configs/path.ini"), "r");
	if (f)
	{
		fread(gCustomRomPath,1, 256, f);
		fclose(f);
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
			else if (strcmp("gAntiAliasing", buffer) == 0) gAntiAliasing = value;
			
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
			else if (strcmp("gAutoUpdate", buffer) == 0) gAutoUpdate = (bool)value;
			else if (strcmp("gLanguageIndex", buffer) == 0) gLanguageIndex = value;
			else if (strcmp("gBigText", buffer) == 0) gBigText = (bool)value;
		}
		fclose(config);
		
		setUiTheme(gUiTheme);
		setCpuMode(gCpuMode);
		setTexCacheMode(gTexCacheMode);
		vglUseVram(gUseCdram);
		vglWaitVblankStart(gUseVSync);
		CInputManager::Get()->SetConfiguration(gControllerIndex);
		
		if (!strcmp(game, "default")) showAlert(lang_strings[STR_ALERT_GLOBAL_SETTINGS_LOAD], ALERT_MESSAGE);
		else showAlert(lang_strings[STR_ALERT_GAME_SETTINGS_LOAD], ALERT_MESSAGE);
	}
}

void extractSubstrings(char *src, char *tag, char* dst1, char *dst2) {
	char *tag_start = strstr(src, tag);
	if (tag_start != src) memcpy_neon(dst1, src, tag_start - src);
	dst1[tag_start - src] = 0;
	sprintf(dst2, &tag_start[strlen(tag)]);
}

int main(int argc, char* argv[])
{
	char *rom;
	
	// Initializing sceAppUtil
	SceAppUtilInitParam appUtilParam;
	SceAppUtilBootParam appUtilBootParam;
	memset(&appUtilParam, 0, sizeof(SceAppUtilInitParam));
	memset(&appUtilBootParam, 0, sizeof(SceAppUtilBootParam));
	sceAppUtilInit(&appUtilParam, &appUtilBootParam);
	sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &console_language);
	
	// We need this to override the compat list update skip option
	preloadConfig();
		
	// Check if Daedalus X64 has been launched with a custom bubble
	sceAppMgrGetAppParam(boot_params);
	if (strstr(boot_params,"psgm:play")) {
		gSkipCompatListUpdate = true;
		gStandaloneMode = false;
		rom = strstr(boot_params, "&param=") + 7;
	} else loadCustomRomPath();
	
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
			
			char pre_launch[32], post_launch[32];
			extractSubstrings(lang_strings[STR_ROM_LAUNCH], "?ROMNAME?", pre_launch, post_launch);
			sprintf(boot_params, "%s%s%s", pre_launch, rom, post_launch); // Re-using boot_params to save some memory
			showAlert(boot_params, ALERT_MESSAGE);
		
			// We re-draw last frame two times in order to make the launching alert to show up
			for (int i = 0; i < 2; i++) { DrawRomSelector(); } 
		}
		
		EnableMenuButtons(false);
		System_Open(rom);
		
		loadConfig(g_ROM.settings.GameName.c_str());
		CPU_Run();
		System_Close();
		
		if (!gStandaloneMode && !restart_rom) break;
	}

	System_Finalize();

	return 0;
}
