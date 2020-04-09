#ifndef DEBUG_SCREEN_H
#define DEBUG_SCREEN_H

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>

#include <psp2/display.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>

extern unsigned char psvDebugScreenFont[];

#define SCREEN_WIDTH    (960)
#define SCREEN_HEIGHT   (544)
#define SCREEN_FB_WIDTH (960)
#define SCREEN_FB_SIZE  (2 * 1024 * 1024)
#define SCREEN_FB_ALIGN (256 * 1024)
#define SCREEN_GLYPH_W  (8)
#define SCREEN_GLYPH_H  (8)

#define COLOR_BLACK      0xFF000000
#define COLOR_RED        0xFF0000FF
#define COLOR_BLUE       0xFF00FF00
#define COLOR_YELLOW     0xFF00FFFF
#define COLOR_GREEN      0xFFFF0000
#define COLOR_MAGENTA    0xFFFF00FF
#define COLOR_CYAN       0xFFFFFF00
#define COLOR_WHITE      0xFFFFFFFF
#define COLOR_GREY       0xFF808080
#define COLOR_DEFAULT_FG COLOR_WHITE
#define COLOR_DEFAULT_BG COLOR_BLACK

static int psvDebugScreenMutex; /*< avoid race condition when outputing strings */
static uint32_t psvDebugScreenCoordX = 0;
static uint32_t psvDebugScreenCoordY = 0;
static uint32_t psvDebugScreenColorFg = COLOR_DEFAULT_FG;
static uint32_t psvDebugScreenColorBg = COLOR_DEFAULT_BG;
static SceDisplayFrameBuf psvDebugScreenFrameBuf = {
		sizeof(SceDisplayFrameBuf), NULL, SCREEN_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT};

uint32_t psvDebugScreenSetFgColor(uint32_t color) {
	uint32_t prev_color = psvDebugScreenColorFg;
	psvDebugScreenColorFg = color;
	return prev_color;
}

uint32_t psvDebugScreenSetBgColor(uint32_t color) {
	uint32_t prev_color = psvDebugScreenColorBg;
	psvDebugScreenColorBg = color;
	return prev_color;
}

static size_t psvDebugScreenEscape(const char *str){
	int i,j, p=0, params[8]={};
	for(i=0; i<8 && str[i]!='\0'; i++){
		if(str[i] >= '0' && str[i] <= '9'){
			params[p]=(params[p]*10) + (str[i] - '0');
		}else if(str[i] == ';'){
			p++;
		}else if(str[i] == 'f' || str[i] == 'H'){
			psvDebugScreenCoordX = params[0] * SCREEN_GLYPH_W;
			psvDebugScreenCoordY = params[1] * SCREEN_GLYPH_H;
			break;
		}else if (str[i] == 'm'){
			for(j=0; j<=p; j++){
				switch(params[j]/10){/*bold,dim,underline,blink,invert,hidden => unsupported yet */
				#define BIT2BYTE(bit)    ( ((!!(bit&4))<<23) | ((!!(bit&2))<<15) | ((!!(bit&1))<<7) )
				case  0:psvDebugScreenSetFgColor(COLOR_DEFAULT_FG);psvDebugScreenSetBgColor(COLOR_DEFAULT_BG);break;
				case  3:psvDebugScreenSetFgColor(BIT2BYTE(params[j]%10));break;
				case  9:psvDebugScreenSetFgColor(BIT2BYTE(params[j]%10) | 0x7F7F7F7F);break;
				case  4:psvDebugScreenSetBgColor(BIT2BYTE(params[j]%10));break;
				case 10:psvDebugScreenSetBgColor(BIT2BYTE(params[j]%10) | 0x7F7F7F7F);break;
				#undef BIT2BYTE
				}
			}
			break;
		}
	}
	return i;
}

int psvDebugScreenInit() {
	psvDebugScreenMutex = sceKernelCreateMutex("log_mutex", 0, 0, NULL);
	SceUID displayblock = sceKernelAllocMemBlock("display", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, SCREEN_FB_SIZE, NULL);
	sceKernelGetMemBlockBase(displayblock, (void**)&psvDebugScreenFrameBuf.base);

	SceDisplayFrameBuf framebuf = {
		.size = sizeof(framebuf),
		.base = psvDebugScreenFrameBuf.base,
		.pitch = SCREEN_WIDTH,
		.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,
		.width = SCREEN_WIDTH,
		.height = SCREEN_HEIGHT,
	};

	return sceDisplaySetFrameBuf(&framebuf, SCE_DISPLAY_SETBUF_NEXTFRAME);
}

void psvDebugScreenClear(int bg_color){
	psvDebugScreenCoordX = psvDebugScreenCoordY = 0;
	int i;
	for(i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
		((uint32_t*)psvDebugScreenFrameBuf.base)[i] = bg_color;
	}
}

#define PSV_DEBUG_SCALE 2

int psvDebugScreenPuts(const char * text){
	int c, i, j, l, x, y;
	uint8_t *font;
	uint32_t *vram_ptr;
	uint32_t *vram;

	sceKernelLockMutex(psvDebugScreenMutex, 1, NULL);

	for (c = 0; text[c] != '\0' ; c++) {
		if (psvDebugScreenCoordX + 8 > SCREEN_WIDTH) {
			psvDebugScreenCoordY += SCREEN_GLYPH_H * PSV_DEBUG_SCALE;
			psvDebugScreenCoordX = 0;
		}
		if (psvDebugScreenCoordY + 8 > SCREEN_HEIGHT) {
			psvDebugScreenClear(psvDebugScreenColorBg);
		}
		if (text[c] == '\n') {
			psvDebugScreenCoordX = 0;
			psvDebugScreenCoordY += SCREEN_GLYPH_H * PSV_DEBUG_SCALE;
			continue;
		} else if (text[c] == '\r') {
			psvDebugScreenCoordX = 0;
			continue;
		} else if ((text[c] == '\e') && (text[c+1] == '[')) { /* escape code (change color, position ...) */
			c+=psvDebugScreenEscape(text+2)+2;
			continue;
		}

		vram = (uint32_t*)psvDebugScreenFrameBuf.base;

		font = &psvDebugScreenFont[ (int)text[c] * 8];
		for (i = l = 0; i < SCREEN_GLYPH_W; i++, l += SCREEN_GLYPH_W, font++) {
			for (j = 0; j < SCREEN_GLYPH_W; j++) {
				for (y = 0; y < PSV_DEBUG_SCALE; y++) {
					for (x = 0; x < PSV_DEBUG_SCALE; x++) {
						vram_ptr = &vram[(psvDebugScreenCoordX + x + j * PSV_DEBUG_SCALE) +
							(psvDebugScreenCoordY + y + i * PSV_DEBUG_SCALE) * SCREEN_FB_WIDTH];
						if ((*font & (128 >> j)))
							*vram_ptr = psvDebugScreenColorFg;
						else
							*vram_ptr = psvDebugScreenColorBg;
					}
				}
			}
		}
		psvDebugScreenCoordX += SCREEN_GLYPH_W * PSV_DEBUG_SCALE;
	}

	sceKernelUnlockMutex(psvDebugScreenMutex, 1);
	return c;
}

int psvDebugScreenVprintf(const char *format, va_list ap) {
	char buf[512];
	int ret = vsnprintf(buf, sizeof(buf), format, ap);
	psvDebugScreenPuts(buf);
	return ret;
}

int psvDebugScreenPrintf(const char *format, ...) {
	char buf[512];

	va_list opt;
	va_start(opt, format);
	int ret = vsnprintf(buf, sizeof(buf), format, opt);
	psvDebugScreenPuts(buf);
	va_end(opt);

	return ret;
}

#endif
