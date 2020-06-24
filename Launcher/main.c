#include <vitasdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, const char *argv[]) {
	char buffer[256];
	memset(buffer, 0, 256);
	FILE *f = fopen("app0:args.txt", "rb");
	if (f) {
		fread(buffer, 1, 256, f);
		fclose(f);
		char uri[512];
		sprintf(uri, "psgm:play?titleid=%s&param=%s", "DEDALOX64", buffer);

		sceAppMgrLaunchAppByUri(0xFFFFF, uri);
		for (;;){}
	}
	
	sceKernelExitProcess(0);
  
	return 0;
}
