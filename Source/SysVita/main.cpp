#include <stdlib.h>
#include <stdio.h>

#include <vitasdk.h>
#include <vitaGL.h>
#include <imgui_vita.h>

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
#include "UI/MainMenuScreen.h"

extern "C" {

int32_t sceKernelChangeThreadVfpException(int32_t clear, int32_t set);
int _newlib_heap_size_user = 128 * 1024 * 1024;

}

void log2file(const char *format, ...) {
	__gnuc_va_list arg;
	int done;
	va_start(arg, format);
	char msg[512];
	done = vsprintf(msg, format, arg);
	va_end(arg);
	sprintf(msg, "%s\n", msg);
	FILE *log = fopen("ux0:/data/DaedalusX64.log", "a+");
	if (log != NULL) {
		fwrite(msg, 1, strlen(msg), log);
		fclose(log);
	}
}

static void Initialize()
{
	strcpy(gDaedalusExePath, DAEDALUS_VITA_PATH(""));
	strcpy(g_DaedalusConfig.mSaveDir, DAEDALUS_VITA_PATH("SaveGames/"));
	
	// Disabling all FPU exceptions traps on main thread
	sceKernelChangeThreadVfpException(0x0800009FU, 0x0);
	
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
	
	// FIXME: This ideally should be inside GraphicsContext initializer however we need vitaGL to draw menu
	vglInitExtended(0x100000, SCR_WIDTH, SCR_HEIGHT, 0x1800000, SCE_GXM_MULTISAMPLE_4X);
	vglUseVram(GL_TRUE);

	System_Init();
	
	// TODO: Move this something else
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui_ImplVitaGL_Init();
	ImGui_ImplVitaGL_TouchUsage(true);
    ImGui_ImplVitaGL_UseIndirectFrontTouch(true);
	ImGui_ImplVitaGL_GamepadUsage(true);
	ImGui::StyleColorsDark();
}

void HandleEndOfFrame()
{
}

int main(int argc, char* argv[])
{
	Initialize();
	
	char *rom = nullptr;
	do {
		rom = DrawRomSelector();
	} while (rom == nullptr);
	
	char fullpath[512];
	sprintf(fullpath, "%s%s", DAEDALUS_VITA_PATH("Roms/"), rom);
	System_Open(fullpath);
	CPU_Run();
	System_Close();
	System_Finalize();

	return 0;
}
