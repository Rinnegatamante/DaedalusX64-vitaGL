#include <stdlib.h>
#include <stdio.h>

#include <vitasdk.h>
#include <vitaGL.h>

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

#include "debugScreen.h"

#define DAEDALUS_VITA_PATH(p) "ux0:/data/DaedalusX64/" p
#define LOAD_ROM              DAEDALUS_VITA_PATH("Roms/Super Mario 64 (USA).n64")
//#define LOAD_ROM              DAEDALUS_VITA_PATH("Roms/rdpdemo.z64")

extern "C" {

extern void __sinit(struct _reent *);

int _newlib_heap_size_user = 128 * 1024 * 1024;

int chdir(const char *path) {return 0;}
int mkdir(const char *path) {return sceIoMkdir(path, 0777);}
int chmod(const char *path, mode_t mode) {return 0;}
long pathconf(const char *path, int name) {return 0;}

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

char *getcwd(char *buf, size_t size)
{
	strlcpy(buf, "ux0:/data", size);
	return buf;
}

unsigned int sleep(unsigned int seconds)
{
	sceKernelDelayThreadCB(seconds * 1000 * 1000);
	return 0;
}

int usleep(useconds_t usec)
{
	sceKernelDelayThreadCB(usec);
	return 0;
}

void __assert_func(const char *filename, int line, const char *assert_func, const char *expr)
{
	log2file("assert on %s (line %ld) func: %s (%s)", filename, line, assert_func, expr);
	abort();
}

void abort(void)
{
	sceKernelExitProcess(0);
	while (1) sleep(1);
}

}

static void wait_press()
{
	SceCtrlData pad;
	memset(&pad, 0, sizeof(pad));
	while (1) {
		sceCtrlPeekBufferPositive(0, &pad, 1);

		if (pad.buttons & SCE_CTRL_CROSS)
			break;
	}
}

static void Initialize()
{
	strcpy(gDaedalusExePath, DAEDALUS_VITA_PATH(""));
	strcpy(g_DaedalusConfig.mSaveDir, DAEDALUS_VITA_PATH("SaveGames/"));

	bool ret = System_Init();
}

void HandleEndOfFrame()
{
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(0, &pad, 1);

	if (pad.buttons & SCE_CTRL_CROSS)
		CPU_Halt("Exiting");
}

int main(int argc, char* argv[])
{

	Initialize();
	System_Open(LOAD_ROM);
	CPU_Run();
	System_Close();
	System_Finalize();

	wait_press();
	return 0;
}
