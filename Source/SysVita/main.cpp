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
#include "UI/Menu.h"

extern "C" {

int32_t sceKernelChangeThreadVfpException(int32_t clear, int32_t set);
int _newlib_heap_size_user = 128 * 1024 * 1024;
extern uint8_t _binary_SysVita_DynaRec_MipsPayload_payload_bin_start;
extern uint8_t _binary_SysVita_DynaRec_MipsPayload_payload_bin_size;
extern uint8_t _binary_SysVita_DynaRec_MipsPayload_license_rif_start;
extern uint8_t _binary_SysVita_DynaRec_MipsPayload_license_rif_size;
	
}

extern bool run_emu;
extern bool restart_rom;

SceCompatCdram cdram;

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

static void EnableMenuButtons(bool status) {
	ImGui_ImplVitaGL_GamepadUsage(status);
	ImGui_ImplVitaGL_MouseStickUsage(status);
}

static bool Initialize()
{
	strcpy(gDaedalusExePath, DAEDALUS_VITA_PATH(""));
	strcpy(g_DaedalusConfig.mSaveDir, DAEDALUS_VITA_PATH("SaveGames/"));
	
	// Disabling all FPU exceptions traps on main thread
	sceKernelChangeThreadVfpException(0x0800009FU, 0x0);
	
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
	
	// Initializing shared mem between MIPS and ARM processors
	if (sceCompatAllocCdramWithHole(&cdram) < 0) return false;
	if (sceCompatInitEx(1) < 0) return false;
	
	// Moving our MIPS code into shared mem
	memcpy(cdram.uncached_cdram + 0x600000,
		(void *)&_binary_SysVita_DynaRec_MipsPayload_payload_bin_start,
		(int)&_binary_SysVita_DynaRec_MipsPayload_payload_bin_size);
	
	// Writing fake license file
	sceIoMkdir("ux0:pspemu", 0777);
	sceIoMkdir("ux0:pspemu/PSP", 0777);
	sceIoMkdir("ux0:pspemu/PSP/LICENSE", 0777);
	SceUID fd = sceIoOpen("ux0:pspemu/PSP/LICENSE/AA0000-AAAA00000_00-0000000000000000.rif", SCE_O_WRONLY | SCE_O_CREAT, 0777);
	if (fd > 0) {
		sceIoWrite(fd, (void *)&_binary_SysVita_DynaRec_MipsPayload_license_rif_start, (int)&_binary_SysVita_DynaRec_MipsPayload_license_rif_size);
		sceIoClose(fd);
	}
	sceCompatSetRif("AA0000-AAAA00000_00-0000000000000000");
	
	// Starting MIPS processor
	if (sceCompatStart() < 0) return false;
	if (sceCompatWaitSpecialRequest(1) < 0) return false;
	
	vglInitExtended(0x100000, SCR_WIDTH, SCR_HEIGHT, 0x1800000, SCE_GXM_MULTISAMPLE_4X);
	vglUseVram(GL_TRUE);

	System_Init();
	
	// TODO: Move this somewhere else
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui_ImplVitaGL_Init();
	ImGui_ImplVitaGL_TouchUsage(true);
	ImGui_ImplVitaGL_UseIndirectFrontTouch(true);
	ImGui::StyleColorsDark();
	SetupVFlux();
	
	return true;
}

void HandleEndOfFrame()
{
}

int main(int argc, char* argv[])
{
	if (!Initialize()) return -1;
	
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
