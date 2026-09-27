#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <math.h>
#include <vector>

#include <vitasdk.h>
#include <vitaGL.h>

#include "BuildOptions.h"
#include "Config/ConfigOptions.h"
#include "Core/Cheats.h"
#include "Core/CPU.h"
#include "Core/Memory.h"
#include "Core/PIF.h"
#include "Core/RomSettings.h"
#include "Core/Save.h"
#include "Graphics/GraphicsContext.h"
#include "HLEGraphics/BaseRenderer.h"
#include "HLEGraphics/TextureCache.h"
#include "Input/InputManager.h"
#include "Interface/RomDB.h"
#include "System/System.h"
#include "Test/BatchTest.h"
#include "Utility/IO.h"
#include "Utility/Thread.h"
#include "Utility/ROMFile.h"
#include "Utility/Timer.h"
#include "SysVita/UI/Menu.h"
#include "SysVita/UI/N64CartridgeMesh.h"
#include "SysVita/UI/N64BackLabelTexture.h"
#include "SysVita/UI/N64LightingShaders.h"

#define STB_IMAGE_IMPLEMENTATION
#include "Utility/stb_image.h"

#define MIN(x,y) ((x) > (y) ? (y) : (x))

#define ROMS_FOLDERS_NUM 5
#define FILTER_MODES_NUM 8

char rom_game_name[256];
char playtime_str[32];
char selectedRom[256];
char rom_name_filter[128] = {0};

GLuint bg_image = 0xDEADBEEF;

struct CompatibilityList {
	char name[128];
	bool playable;
	bool ingame_plus;
	bool ingame_low;
	bool crash;
	bool slow;
	CompatibilityList *next;
};

struct RomSelection {
	char name[128];
	char fullpath[256];
	char preview[128];
	char title[128];
	bool is_online;
	RomID id;
	u32 size;
	ESaveType save;
	ECicType cic;
	uint64_t playtime;
	CompatibilityList *status;
	RomSelection *next;
};

struct RomCoverCacheEntry {
	char preview[128];
	GLuint texture;
	int width;
	int height;
	uint32_t stamp;
	bool missing;
};

enum ERomCoverJobState {
	ROM_COVER_JOB_FREE,
	ROM_COVER_JOB_QUEUED,
	ROM_COVER_JOB_LOADING,
	ROM_COVER_JOB_READY
};

struct RomCoverLoadJob {
	ERomCoverJobState state;
	char preview[128];
	uint8_t *pixels;
	int width;
	int height;
	int priority;
	uint32_t generation;
	bool missing;
};

static RomSelection *last_launched = nullptr;
static RomSelection *list = nullptr;
static CompatibilityList *comp = nullptr;
uint64_t cur_playtime = 0;

#define ROM_COVER_CACHE_SIZE 7
#define ROM_COVER_ASYNC_JOBS 8
static RomCoverCacheEntry cover_cache[ROM_COVER_CACHE_SIZE] = {};
static uint32_t cover_cache_stamp = 1;
static RomCoverLoadJob cover_load_jobs[ROM_COVER_ASYNC_JOBS] = {};
static SceUID cover_loader_mutex = -1;
static SceUID cover_loader_thread = -1;
static volatile bool cover_loader_running = false;
static uint32_t cover_cache_generation = 1;

int oldSortOrder = -1;

int filter_idx = 0;
const char *filter_modes[] = {
	lang_strings[STR_NO_FILTER],
	lang_strings[STR_GAME_PLAYABLE],
	lang_strings[STR_GAME_INGAME_PLUS],
	lang_strings[STR_GAME_INGAME_MINUS],
	lang_strings[STR_GAME_CRASH],
	lang_strings[STR_NO_TAGS],
	lang_strings[STR_GAME_LOCAL],
	lang_strings[STR_GAME_ONLINE]
};

// Filter modes enum
enum {
	FILTER_DISABLED,
	FILTER_PLAYABLE,
	FILTER_INGAME_PLUS,
	FILTER_INGAME_MINUS,
	FILTER_CRASH,
	FILTER_NO_TAGS,
	FILTER_LOCAL,
	FILTER_ONLINE
};

void apply_rom_name_filter() {
	getDialogTextResult(rom_name_filter);
}

void resetRomList() {
	cover_cache_generation++;
	if (cover_cache_generation == 0)
		cover_cache_generation = 1;
	if (cover_loader_mutex >= 0) {
		sceKernelLockMutex(cover_loader_mutex, 1, NULL);
		for (int i = 0; i < ROM_COVER_ASYNC_JOBS; i++) {
			if (cover_load_jobs[i].state == ROM_COVER_JOB_READY && cover_load_jobs[i].pixels) {
				free(cover_load_jobs[i].pixels);
				cover_load_jobs[i].pixels = NULL;
			}
			if (cover_load_jobs[i].state != ROM_COVER_JOB_LOADING)
				cover_load_jobs[i].state = ROM_COVER_JOB_FREE;
		}
		sceKernelUnlockMutex(cover_loader_mutex, 1);
	}

	for (int i = 0; i < ROM_COVER_CACHE_SIZE; i++) {
		if (cover_cache[i].texture) {
			glDeleteTextures(1, &cover_cache[i].texture);
			cover_cache[i].texture = 0;
		}
		cover_cache[i].preview[0] = 0;
		cover_cache[i].missing = false;
		cover_cache[i].stamp = 0;
	}
	cover_cache_stamp = 1;

	RomSelection *p = list;
	while (p) {
		RomSelection *old = p;
		p = p->next;
		free(old);
	}
	list = nullptr;
}

void swap_roms(RomSelection *a, RomSelection *b) {
	RomSelection tmp;
	
	// Swapping everything except next leaf pointer
	sceClibMemcpy(&tmp, a, sizeof(RomSelection) - 4);
	sceClibMemcpy(a, b, sizeof(RomSelection) - 4);
	sceClibMemcpy(b, &tmp, sizeof(RomSelection) - 4);
}

void swap_shaders(PostProcessingEffect *a, PostProcessingEffect *b) {
	PostProcessingEffect tmp;
	
	// Swapping everything except next leaf pointer
	sceClibMemcpy(&tmp, a, sizeof(PostProcessingEffect) - 4);
	sceClibMemcpy(a, b, sizeof(PostProcessingEffect) - 4);
	sceClibMemcpy(b, &tmp, sizeof(PostProcessingEffect) - 4);
}

void swap_overlays(Overlay *a, Overlay *b) {
	Overlay tmp;
	
	// Swapping everything except next leaf pointer
	sceClibMemcpy(&tmp, a, sizeof(Overlay) - 4);
	sceClibMemcpy(a, b, sizeof(Overlay) - 4);
	sceClibMemcpy(b, &tmp, sizeof(Overlay) - 4);
}

void sort_romlist(RomSelection *start, int order) { 
	// Checking for empty list
	if (start == NULL) 
		return; 
	
	int swapped, i; 
	RomSelection *ptr1; 
	RomSelection *lptr = NULL; 
  
	do { 
		swapped = 0; 
		ptr1 = start; 
  
		while (ptr1->next != lptr && ptr1->next) {
			switch (order) {
			case SORT_Z_TO_A:
				{
					if (strcasecmp(ptr1->name,ptr1->next->name) < 0) {
						swap_roms(ptr1, ptr1->next); 
						swapped = 1; 
					}
				}
				break;
			case SORT_A_TO_Z:
				{
					if (strcasecmp(ptr1->name,ptr1->next->name) > 0) {
						swap_roms(ptr1, ptr1->next); 
						swapped = 1; 
					}
				}
				break;
			case SORT_PLAYTIME_DESC:
				{
					if (ptr1->playtime < ptr1->next->playtime) {
						swap_roms(ptr1, ptr1->next); 
						swapped = 1; 
					}
				}
				break;
			case SORT_PLAYTIME_ASC:
				{
					if (ptr1->playtime > ptr1->next->playtime) {
						swap_roms(ptr1, ptr1->next); 
						swapped = 1; 
					}
				}
				break;
			default:
				break;
			}
			ptr1 = ptr1->next; 
		} 
		lptr = ptr1; 
	} while (swapped); 
}

void sort_shaderlist(PostProcessingEffect *start) {
	int swapped, i; 
	PostProcessingEffect *ptr1; 
	PostProcessingEffect *lptr = NULL;
	
	/* Checking for empty list */
	if (start == NULL) 
		return; 
	
	do {
		swapped = 0;
		ptr1 = start;
		
		while (ptr1->next != lptr) {
			if (strcasecmp(ptr1->name,ptr1->next->name) > 0) {  
				swap_shaders(ptr1, ptr1->next); 
				swapped = 1; 
			}
			ptr1 = ptr1->next; 
		} 
		lptr = ptr1;
	} while (swapped);
}

void sort_overlaylist(Overlay *start) {
	int swapped, i; 
	Overlay *ptr1; 
	Overlay *lptr = NULL;
	
	/* Checking for empty list */
	if (start == NULL) 
		return; 
	
	do {
		swapped = 0;
		ptr1 = start;
		
		while (ptr1->next != lptr) {
			if (strcasecmp(ptr1->name,ptr1->next->name) > 0) {  
				swap_overlays(ptr1, ptr1->next); 
				swapped = 1; 
			}
			ptr1 = ptr1->next; 
		} 
		lptr = ptr1;
	} while (swapped);
}

void LoadBackground() {
	IO::Filename preview_filename;
	IO::Path::Combine(preview_filename, DAEDALUS_VITA_PATH("Resources/"), "bg.png" );
	int w, h;
	uint8_t *bg_data = stbi_load(preview_filename, &w, &h, NULL, 4);
	if (bg_data) {
		glGenTextures(1, &bg_image);
		glBindTexture(GL_TEXTURE_2D, bg_image);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, bg_data);
		free(bg_data);
	}
}

void LoadPlaytimeData(RomSelection *p) {
	if (p->is_online) {
		p->playtime = 0;
		return;
	}
	char fname[64];
	sprintf(fname, "%08x%08x-%02x.bin", p->id.CRC[0], p->id.CRC[1], p->id.CountryID);
	IO::Filename fullpath_filename;
	IO::Path::Combine(fullpath_filename, DAEDALUS_VITA_PATH("Playtimes/"), fname );
	FILE *f = fopen(fullpath_filename, "rb");
	if (f) {
		fscanf(f, "%llu", &p->playtime);
		fclose(f);
	} else p->playtime = 0;
}

char *FormatPlaytime(uint64_t playtime) {
	uint64_t seconds = playtime % 60;
	uint64_t min_raw = (playtime / 60);
	uint64_t minutes = min_raw % 60;
	uint64_t hours = min_raw / 60;
	sprintf(playtime_str, "%02llu:%02llu:%02llu", hours, minutes, seconds);
	return playtime_str;
}

float *bg_attributes = nullptr;
void DrawBackground()
{
	if (!bg_attributes) bg_attributes = (float*)malloc(sizeof(float) * 22);

	glBindTexture(GL_TEXTURE_2D, bg_image);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	bg_attributes[0] = 0.0f;
	bg_attributes[1] = 0.0f;
	bg_attributes[2] = 0.0f;
	bg_attributes[3] = SCR_WIDTH;
	bg_attributes[4] = 0.0f;
	bg_attributes[5] = 0.0f;
	bg_attributes[6] = 0.0f;
	bg_attributes[7] = SCR_HEIGHT;
	bg_attributes[8] = 0.0f;
	bg_attributes[9] = SCR_WIDTH;
	bg_attributes[10] = SCR_HEIGHT;
	bg_attributes[11] = 0.0f;
	vglVertexPointerMapped(3, bg_attributes);
	
	bg_attributes[12] = 0.0f;
	bg_attributes[13] = 0.0f;
	bg_attributes[14] = 1.0f;
	bg_attributes[15] = 0.0f;
	bg_attributes[16] = 0.0f;
	bg_attributes[17] = 1.0f;
	bg_attributes[18] = 1.0f;
	bg_attributes[19] = 1.0f;
	vglTexCoordPointerMapped(&bg_attributes[12]);
	
	uint16_t *bg_indices = (uint16_t*)&bg_attributes[20];
	bg_indices[0] = 0;
	bg_indices[1] = 1;
	bg_indices[2] = 2;
	bg_indices[3] = 3;
	vglIndexPointerMapped(bg_indices);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 960, 544, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	vglDrawObjects(GL_TRIANGLE_STRIP, 4);
}

bool filterRoms(RomSelection *p);

static uint8_t *LoadRomCoverFile(const char *path, int *width, int *height) {
	SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0)
		return NULL;

	const SceOff file_size = sceIoLseek(fd, 0, SCE_SEEK_END);
	if (file_size <= 0 || file_size > 16 * 1024 * 1024) {
		sceIoClose(fd);
		return NULL;
	}
	if (sceIoLseek(fd, 0, SCE_SEEK_SET) < 0) {
		sceIoClose(fd);
		return NULL;
	}

	uint8_t *file_data = (uint8_t*)malloc((size_t)file_size);
	if (!file_data) {
		sceIoClose(fd);
		return NULL;
	}

	SceOff total_read = 0;
	while (total_read < file_size) {
		const int read = sceIoRead(fd, file_data + total_read, (SceSize)(file_size - total_read));
		if (read <= 0) {
			free(file_data);
			sceIoClose(fd);
			return NULL;
		}
		total_read += read;
	}
	sceIoClose(fd);

	uint8_t *pixels = stbi_load_from_memory(file_data, (int)file_size, width, height, NULL, 4);
	free(file_data);
	return pixels;
}

static int RomCoverLoaderThread(unsigned int args, void *arg) {
	while (cover_loader_running) {
		int job_index = -1;
		int best_priority = 0x7fffffff;
		char preview[128] = {};
		uint32_t generation = 0;

		sceKernelLockMutex(cover_loader_mutex, 1, NULL);
		for (int i = 0; i < ROM_COVER_ASYNC_JOBS; i++) {
			if (cover_load_jobs[i].state == ROM_COVER_JOB_QUEUED &&
				cover_load_jobs[i].priority < best_priority) {
				job_index = i;
				best_priority = cover_load_jobs[i].priority;
			}
		}
		if (job_index >= 0) {
			cover_load_jobs[job_index].state = ROM_COVER_JOB_LOADING;
			strcpy(preview, cover_load_jobs[job_index].preview);
			generation = cover_load_jobs[job_index].generation;
		}
		sceKernelUnlockMutex(cover_loader_mutex, 1);

		if (job_index < 0) {
			sceKernelDelayThread(1000);
			continue;
		}

		int width = 0;
		int height = 0;
		IO::Filename cover_filename;
		IO::Path::Combine(cover_filename, DAEDALUS_VITA_PATH("Resources/Covers/"), preview);
		uint8_t *pixels = LoadRomCoverFile(cover_filename, &width, &height);

		sceKernelLockMutex(cover_loader_mutex, 1, NULL);
		RomCoverLoadJob &job = cover_load_jobs[job_index];
		if (job.state == ROM_COVER_JOB_LOADING &&
			job.generation == generation &&
			strcmp(job.preview, preview) == 0) {
			job.pixels = pixels;
			job.width = width;
			job.height = height;
			job.missing = pixels == NULL;
			job.state = ROM_COVER_JOB_READY;
			pixels = NULL;
		} else {
			job.state = ROM_COVER_JOB_FREE;
		}
		sceKernelUnlockMutex(cover_loader_mutex, 1);

		if (pixels)
			free(pixels);
	}
	return 0;
}

static void EnsureRomCoverLoader() {
	if (cover_loader_thread >= 0)
		return;

	cover_loader_mutex = sceKernelCreateMutex("ROM Cover Loader Mutex", 0, 1, NULL);
	if (cover_loader_mutex < 0)
		return;

	cover_loader_running = true;
	cover_loader_thread = sceKernelCreateThread(
		"ROM Cover Loader", &RomCoverLoaderThread, 0x10000100, 0x40000, 0, 0, NULL);
	if (cover_loader_thread < 0) {
		cover_loader_running = false;
		sceKernelDeleteMutex(cover_loader_mutex);
		cover_loader_mutex = -1;
		return;
	}
	if (sceKernelStartThread(cover_loader_thread, 0, NULL) < 0) {
		sceKernelDeleteThread(cover_loader_thread);
		cover_loader_thread = -1;
		cover_loader_running = false;
		sceKernelDeleteMutex(cover_loader_mutex);
		cover_loader_mutex = -1;
	}
}

static void QueueRomCoverDecode(const char *preview, int priority) {
	if (!preview || !preview[0])
		return;

	EnsureRomCoverLoader();
	if (cover_loader_thread < 0)
		return;

	sceKernelLockMutex(cover_loader_mutex, 1, NULL);
	for (int i = 0; i < ROM_COVER_ASYNC_JOBS; i++) {
		RomCoverLoadJob &job = cover_load_jobs[i];
		if (job.state != ROM_COVER_JOB_FREE &&
			job.generation == cover_cache_generation &&
			strcmp(job.preview, preview) == 0) {
			if (job.state == ROM_COVER_JOB_QUEUED && priority < job.priority)
				job.priority = priority;
			sceKernelUnlockMutex(cover_loader_mutex, 1);
			return;
		}
	}

	int slot = -1;
	int worst_priority = -1;
	for (int i = 0; i < ROM_COVER_ASYNC_JOBS; i++) {
		RomCoverLoadJob &job = cover_load_jobs[i];
		if (job.state == ROM_COVER_JOB_FREE) {
			slot = i;
			break;
		}
		if (job.state == ROM_COVER_JOB_QUEUED && job.priority > worst_priority) {
			slot = i;
			worst_priority = job.priority;
		}
	}

	if (slot >= 0 && (cover_load_jobs[slot].state == ROM_COVER_JOB_FREE ||
		priority < cover_load_jobs[slot].priority)) {
		RomCoverLoadJob &job = cover_load_jobs[slot];
		job.state = ROM_COVER_JOB_QUEUED;
		strncpy(job.preview, preview, sizeof(job.preview) - 1);
		job.preview[sizeof(job.preview) - 1] = 0;
		job.pixels = NULL;
		job.width = 0;
		job.height = 0;
		job.priority = priority;
		job.generation = cover_cache_generation;
		job.missing = false;
	}
	sceKernelUnlockMutex(cover_loader_mutex, 1);
}

static int FindRomCoverCacheSlot(const char *preview) {
	for (int i = 0; i < ROM_COVER_CACHE_SIZE; i++) {
		if (cover_cache[i].preview[0] && strcmp(cover_cache[i].preview, preview) == 0)
			return i;
	}
	return -1;
}

static int FindRomCoverEvictionSlot() {
	int slot = 0;
	for (int i = 1; i < ROM_COVER_CACHE_SIZE; i++) {
		if (cover_cache[i].preview[0] == 0 || cover_cache[i].stamp < cover_cache[slot].stamp)
			slot = i;
	}
	return slot;
}

static void ProcessRomCoverDecodeResults() {
	if (cover_loader_thread < 0)
		return;

	char preview[128] = {};
	uint8_t *pixels = NULL;
	int width = 0;
	int height = 0;
	bool missing = false;
	uint32_t generation = 0;
	bool found = false;

	sceKernelLockMutex(cover_loader_mutex, 1, NULL);
	for (int i = 0; i < ROM_COVER_ASYNC_JOBS; i++) {
		RomCoverLoadJob &job = cover_load_jobs[i];
		if (job.state != ROM_COVER_JOB_READY)
			continue;
		strcpy(preview, job.preview);
		pixels = job.pixels;
		width = job.width;
		height = job.height;
		missing = job.missing;
		generation = job.generation;
		job.pixels = NULL;
		job.state = ROM_COVER_JOB_FREE;
		found = true;
		break;
	}
	sceKernelUnlockMutex(cover_loader_mutex, 1);

	if (!found)
		return;
	if (generation != cover_cache_generation) {
		if (pixels)
			free(pixels);
		return;
	}

	int slot = FindRomCoverCacheSlot(preview);
	if (slot < 0)
		slot = FindRomCoverEvictionSlot();
	if (cover_cache[slot].texture) {
		glDeleteTextures(1, &cover_cache[slot].texture);
		cover_cache[slot].texture = 0;
	}

	strncpy(cover_cache[slot].preview, preview, sizeof(cover_cache[slot].preview) - 1);
	cover_cache[slot].preview[sizeof(cover_cache[slot].preview) - 1] = 0;
	cover_cache[slot].width = width;
	cover_cache[slot].height = height;
	cover_cache[slot].stamp = ++cover_cache_stamp;
	cover_cache[slot].missing = missing;

	if (pixels && !missing) {
		glGenTextures(1, &cover_cache[slot].texture);
		glBindTexture(GL_TEXTURE_2D, cover_cache[slot].texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	}
	if (pixels)
		free(pixels);
}

static GLuint n64_white_cover_texture = 0;

static GLuint GetWhiteCoverTexture() {
	if (!n64_white_cover_texture) {
		const uint32_t white_pixel = 0xFFFFFFFF;
		glGenTextures(1, &n64_white_cover_texture);
		glBindTexture(GL_TEXTURE_2D, n64_white_cover_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white_pixel);
	}
	return n64_white_cover_texture;
}

static GLuint GetRomCoverTexture(RomSelection *rom, int *width, int *height) {
	if (!rom || rom->is_online || rom->preview[0] == 0) {
		if (width) *width = 1;
		if (height) *height = 1;
		return GetWhiteCoverTexture();
	}

	cover_cache_stamp++;
	if (cover_cache_stamp == 0)
		cover_cache_stamp = 1;

	const int slot = FindRomCoverCacheSlot(rom->preview);
	if (slot >= 0) {
		cover_cache[slot].stamp = cover_cache_stamp;
		if (width) *width = cover_cache[slot].width;
		if (height) *height = cover_cache[slot].height;
		if (cover_cache[slot].missing) {
			if (width) *width = 1;
			if (height) *height = 1;
			return GetWhiteCoverTexture();
		}
		return cover_cache[slot].texture;
	}

	QueueRomCoverDecode(rom->preview, 16);
	if (width) *width = 1;
	if (height) *height = 1;
	return GetWhiteCoverTexture();
}

static float GetN64CoverTopV(float u) {
	static const float kEdgeDistance[] = {
		0.00f, 0.28f, 0.36f, 0.44f, 0.52f,
		0.60f, 0.68f, 0.76f, 0.84f, 0.92f, 1.00f
	};
	static const float kTopV[] = {
		0.000f, 0.000f, 0.004f, 0.008f, 0.012f,
		0.016f, 0.022f, 0.029f, 0.035f, 0.044f, 0.073f
	};

	float edge_distance = fabsf(u - 0.5f) * 2.0f;
	if (edge_distance <= kEdgeDistance[0])
		return kTopV[0];
	if (edge_distance >= kEdgeDistance[10])
		return kTopV[10];

	for (int i = 0; i < 10; i++) {
		if (edge_distance <= kEdgeDistance[i + 1]) {
			const float span = kEdgeDistance[i + 1] - kEdgeDistance[i];
			const float t = (edge_distance - kEdgeDistance[i]) / span;
			return kTopV[i] + (kTopV[i + 1] - kTopV[i]) * t;
		}
	}

	return 0.0f;
}

static bool RomMatchesCurrentView(RomSelection *rom) {
	if (!rom)
		return false;
	if (rom_name_filter[0] && !strcasestr(rom->name, rom_name_filter))
		return false;
	if (filter_idx > 0 && filterRoms(rom))
		return false;
	return true;
}

static void DrawCenteredClippedText(ImDrawList *draw, const ImVec2 &min, const ImVec2 &max,
	const char *text, ImU32 color) {
	if (!text || !text[0])
		return;
	ImVec2 text_size = ImGui::CalcTextSize(text);
	ImVec2 pos(
		min.x + ((max.x - min.x) - text_size.x) * 0.5f,
		min.y + ((max.y - min.y) - text_size.y) * 0.5f);
	draw->PushClipRect(min, max, true);
	draw->AddText(pos, color, text);
	draw->PopClipRect();
}

static void DrawCompatibilityTagRow(ImDrawList *draw, RomSelection *rom, float center_x, float y) {
	if (!draw || !rom || !rom->status)
		return;

	struct TagItem {
		const char *text;
		ImVec4 color;
	};

	TagItem tags[5];
	int tag_count = 0;
	if (rom->status->playable)
		tags[tag_count++] = { lang_strings[STR_GAME_PLAYABLE], ImVec4(0.0f, 0.75f, 0.0f, 1.0f) };
	if (rom->status->ingame_plus)
		tags[tag_count++] = { lang_strings[STR_GAME_INGAME_PLUS], ImVec4(1.0f, 1.0f, 0.0f, 1.0f) };
	if (rom->status->ingame_low)
		tags[tag_count++] = { lang_strings[STR_GAME_INGAME_MINUS], ImVec4(1.0f, 0.5f, 0.25f, 1.0f) };
	if (rom->status->crash)
		tags[tag_count++] = { lang_strings[STR_GAME_CRASH], ImVec4(1.0f, 0.0f, 0.0f, 1.0f) };
	if (rom->status->slow)
		tags[tag_count++] = { lang_strings[STR_GAME_SLOW], ImVec4(0.5f, 0.0f, 1.0f, 1.0f) };

	if (tag_count == 0)
		return;

	const float pad_x = 8.0f;
	const float pad_y = 3.0f;
	const float gap = 6.0f;
	float total_width = 0.0f;
	for (int i = 0; i < tag_count; i++) {
		const ImVec2 size = ImGui::CalcTextSize(tags[i].text);
		total_width += size.x + pad_x * 2.0f;
		if (i + 1 < tag_count)
			total_width += gap;
	}

	float x = center_x - total_width * 0.5f;
	for (int i = 0; i < tag_count; i++) {
		const ImVec2 text_size = ImGui::CalcTextSize(tags[i].text);
		const ImVec2 badge_min(x, y);
		const ImVec2 badge_max(x + text_size.x + pad_x * 2.0f,
			y + text_size.y + pad_y * 2.0f);
		draw->AddRectFilled(badge_min, badge_max,
			ImGui::GetColorU32(ImVec4(0.035f, 0.04f, 0.045f, 0.86f)), 5.0f);
		draw->AddText(ImVec2(badge_min.x + pad_x, badge_min.y + pad_y),
			ImGui::GetColorU32(tags[i].color), tags[i].text);
		x = badge_max.x + gap;
	}
}

static float *n64_cartridge_vertices = nullptr;
static float *n64_cartridge_normals = nullptr;
static float *n64_cartridge_colors = nullptr;
static float *n64_cartridge_selected_colors = nullptr;
static float *n64_front_label_vertices = nullptr;
static float *n64_back_label_vertices = nullptr;
static float *n64_back_label_uvs = nullptr;
static float *n64_contact_vertices = nullptr;
static float *n64_contact_normals = nullptr;
static float *n64_contact_colors = nullptr;
static float *n64_screw_vertices = nullptr;
static float *n64_screw_normals = nullptr;
static float *n64_screw_colors = nullptr;
static GLuint n64_back_label_texture = 0;

static GLuint n64_lighting_program = 0;
static GLuint n64_lighting_vs = 0;
static GLuint n64_lighting_fs = 0;
static bool n64_lighting_attempted = false;
static GLint n64_u_rot0 = -1;
static GLint n64_u_rot1 = -1;
static GLint n64_u_rot2 = -1;
static GLint n64_u_base_color = -1;
static GLint n64_u_metallic = -1;
static GLint n64_u_roughness = -1;

static bool EnsureN64LightingShader() {
	if (n64_lighting_program)
		return true;
	if (n64_lighting_attempted)
		return false;
	n64_lighting_attempted = true;

	n64_lighting_vs = glCreateShader(GL_CG_VERTEX_SHADER_EXT);
	n64_lighting_fs = glCreateShader(GL_CG_FRAGMENT_SHADER_EXT);
	if (!n64_lighting_vs || !n64_lighting_fs)
		return false;
	vglShaderGxpBinary(1, &n64_lighting_vs, kN64LightingVertexGxp, (GLsizei)kN64LightingVertexGxpSize);
	vglShaderGxpBinary(1, &n64_lighting_fs, kN64LightingFragmentGxp, (GLsizei)kN64LightingFragmentGxpSize);

	n64_lighting_program = glCreateProgram();
	if (!n64_lighting_program)
		return false;
	glAttachShader(n64_lighting_program, n64_lighting_vs);
	glAttachShader(n64_lighting_program, n64_lighting_fs);
	vglBindAttribLocation(n64_lighting_program, 0, "in_pos", 3, GL_FLOAT);
	vglBindAttribLocation(n64_lighting_program, 1, "in_normal", 3, GL_FLOAT);
	glLinkProgram(n64_lighting_program);
	GLint linked = GL_FALSE;
	glGetProgramiv(n64_lighting_program, GL_LINK_STATUS, &linked);
	if (linked != GL_TRUE) {
		glDeleteProgram(n64_lighting_program);
		n64_lighting_program = 0;
		return false;
	}

	n64_u_rot0 = glGetUniformLocation(n64_lighting_program, "uRot0");
	n64_u_rot1 = glGetUniformLocation(n64_lighting_program, "uRot1");
	n64_u_rot2 = glGetUniformLocation(n64_lighting_program, "uRot2");
	n64_u_base_color = glGetUniformLocation(n64_lighting_program, "uBaseColor");
	n64_u_metallic = glGetUniformLocation(n64_lighting_program, "uMetallic");
	n64_u_roughness = glGetUniformLocation(n64_lighting_program, "uRoughness");
	return true;
}

static void SetN64LightingRotation(float yaw, float pitch) {
	const float deg_to_rad = 0.01745329251994329577f;
	const float y = yaw * deg_to_rad;
	const float p = pitch * deg_to_rad;
	const float cy = cosf(y);
	const float sy = sinf(y);
	const float cp = cosf(p);
	const float sp = sinf(p);

	// OpenGL applies Ry first, then Rx for the current model-view setup.
	if (n64_u_rot0 >= 0) glUniform4f(n64_u_rot0, cy, 0.0f, sy, 0.0f);
	if (n64_u_rot1 >= 0) glUniform4f(n64_u_rot1, sp * sy, cp, -sp * cy, 0.0f);
	if (n64_u_rot2 >= 0) glUniform4f(n64_u_rot2, -cp * sy, sp, cp * cy, 0.0f);
}

static void SetN64LightingMaterial(float r, float g, float b, float metallic, float roughness) {
	if (n64_u_base_color >= 0) glUniform4f(n64_u_base_color, r, g, b, 1.0f);
	if (n64_u_metallic >= 0) glUniform1f(n64_u_metallic, metallic);
	if (n64_u_roughness >= 0) glUniform1f(n64_u_roughness, roughness);
}

static bool EnsureN64CartridgeMesh() {
	if (n64_cartridge_vertices)
		return true;

	const size_t body_v3 = (size_t)kN64BodyVertexCount * 3;
	const size_t body_v4 = (size_t)kN64BodyVertexCount * 4;
	const size_t front_v3 = (size_t)kN64FrontLabelVertexCount * 3;
	const size_t back_v3 = (size_t)kN64BackLabelVertexCount * 3;
	const size_t back_v2 = (size_t)kN64BackLabelVertexCount * 2;
	const size_t contact_v3 = (size_t)kN64ContactVertexCount * 3;
	const size_t contact_v4 = (size_t)kN64ContactVertexCount * 4;
	const size_t screw_v3 = (size_t)kN64ScrewVertexCount * 3;
	const size_t screw_v4 = (size_t)kN64ScrewVertexCount * 4;

	n64_cartridge_vertices = (float*)vglAlloc((uint32_t)(body_v3 * sizeof(float)), VGL_MEM_RAM);
	n64_cartridge_normals = (float*)vglAlloc((uint32_t)(body_v3 * sizeof(float)), VGL_MEM_RAM);
	n64_cartridge_colors = (float*)vglAlloc((uint32_t)(body_v4 * sizeof(float)), VGL_MEM_RAM);
	n64_cartridge_selected_colors = (float*)vglAlloc((uint32_t)(body_v4 * sizeof(float)), VGL_MEM_RAM);
	n64_front_label_vertices = (float*)vglAlloc((uint32_t)(front_v3 * sizeof(float)), VGL_MEM_RAM);
	n64_back_label_vertices = (float*)vglAlloc((uint32_t)(back_v3 * sizeof(float)), VGL_MEM_RAM);
	n64_back_label_uvs = (float*)vglAlloc((uint32_t)(back_v2 * sizeof(float)), VGL_MEM_RAM);
	n64_contact_vertices = (float*)vglAlloc((uint32_t)(contact_v3 * sizeof(float)), VGL_MEM_RAM);
	n64_contact_normals = (float*)vglAlloc((uint32_t)(contact_v3 * sizeof(float)), VGL_MEM_RAM);
	n64_contact_colors = (float*)vglAlloc((uint32_t)(contact_v4 * sizeof(float)), VGL_MEM_RAM);
	n64_screw_vertices = (float*)vglAlloc((uint32_t)(screw_v3 * sizeof(float)), VGL_MEM_RAM);
	n64_screw_normals = (float*)vglAlloc((uint32_t)(screw_v3 * sizeof(float)), VGL_MEM_RAM);
	n64_screw_colors = (float*)vglAlloc((uint32_t)(screw_v4 * sizeof(float)), VGL_MEM_RAM);
	if (!n64_cartridge_vertices || !n64_cartridge_normals || !n64_cartridge_colors || !n64_cartridge_selected_colors ||
		!n64_front_label_vertices || !n64_back_label_vertices || !n64_back_label_uvs ||
		!n64_contact_vertices || !n64_contact_normals || !n64_contact_colors ||
		!n64_screw_vertices || !n64_screw_normals || !n64_screw_colors) {
		return false;
	}

	for (int i = 0; i < kN64BodyVertexCount; i++) {
		const int p = i * 3;
		const int c = i * 4;
		n64_cartridge_vertices[p + 0] = (float)kN64BodyPositions[p + 0] / kN64CartridgePositionQuantization;
		n64_cartridge_vertices[p + 1] = (float)kN64BodyPositions[p + 1] / kN64CartridgePositionQuantization;
		n64_cartridge_vertices[p + 2] = (float)kN64BodyPositions[p + 2] / kN64CartridgePositionQuantization;
		n64_cartridge_normals[p + 0] = (float)kN64BodyNormals[p + 0] / 127.0f;
		n64_cartridge_normals[p + 1] = (float)kN64BodyNormals[p + 1] / 127.0f;
		n64_cartridge_normals[p + 2] = (float)kN64BodyNormals[p + 2] / 127.0f;

		const float nz = std::max(0.0f, n64_cartridge_normals[p + 2]);
		const float fallback = 0.40f + 0.20f * nz;
		const float selected_fallback = 0.46f + 0.22f * nz;
		n64_cartridge_colors[c + 0] = fallback * 0.96f;
		n64_cartridge_colors[c + 1] = fallback * 0.98f;
		n64_cartridge_colors[c + 2] = fallback;
		n64_cartridge_colors[c + 3] = 1.0f;
		n64_cartridge_selected_colors[c + 0] = selected_fallback * 0.96f;
		n64_cartridge_selected_colors[c + 1] = selected_fallback * 0.98f;
		n64_cartridge_selected_colors[c + 2] = selected_fallback;
		n64_cartridge_selected_colors[c + 3] = 1.0f;
	}

	for (int i = 0; i < kN64FrontLabelVertexCount; i++) {
		const int p = i * 3;
		n64_front_label_vertices[p + 0] = (float)kN64FrontLabelPositions[p + 0] / kN64CartridgePositionQuantization;
		n64_front_label_vertices[p + 1] = (float)kN64FrontLabelPositions[p + 1] / kN64CartridgePositionQuantization;
		n64_front_label_vertices[p + 2] = (float)kN64FrontLabelPositions[p + 2] / kN64CartridgePositionQuantization;
	}

	for (int i = 0; i < kN64BackLabelVertexCount; i++) {
		const int p = i * 3;
		const int t = i * 2;
		n64_back_label_vertices[p + 0] = (float)kN64BackLabelPositions[p + 0] / kN64CartridgePositionQuantization;
		n64_back_label_vertices[p + 1] = (float)kN64BackLabelPositions[p + 1] / kN64CartridgePositionQuantization;
		n64_back_label_vertices[p + 2] = (float)kN64BackLabelPositions[p + 2] / kN64CartridgePositionQuantization;
		n64_back_label_uvs[t + 0] = (float)kN64BackLabelUV[t + 0] / kN64CartridgeUVQuantization;
		n64_back_label_uvs[t + 1] = 1.0f - (float)kN64BackLabelUV[t + 1] / kN64CartridgeUVQuantization;
	}

	for (int i = 0; i < kN64ContactVertexCount; i++) {
		const int p = i * 3;
		const int c = i * 4;
		n64_contact_vertices[p + 0] = (float)kN64ContactPositions[p + 0] / kN64CartridgePositionQuantization;
		n64_contact_vertices[p + 1] = (float)kN64ContactPositions[p + 1] / kN64CartridgePositionQuantization + 0.21f;
		n64_contact_vertices[p + 2] = (float)kN64ContactPositions[p + 2] / kN64CartridgePositionQuantization + 0.22f;
		n64_contact_normals[p + 0] = (float)kN64ContactNormals[p + 0] / 127.0f;
		n64_contact_normals[p + 1] = (float)kN64ContactNormals[p + 1] / 127.0f;
		n64_contact_normals[p + 2] = (float)kN64ContactNormals[p + 2] / 127.0f;
		n64_contact_colors[c + 0] = 0.82f;
		n64_contact_colors[c + 1] = 0.56f;
		n64_contact_colors[c + 2] = 0.12f;
		n64_contact_colors[c + 3] = 1.0f;
	}

	for (int i = 0; i < kN64ScrewVertexCount; i++) {
		const int p = i * 3;
		const int c = i * 4;
		n64_screw_vertices[p + 0] = (float)kN64ScrewPositions[p + 0] / kN64CartridgePositionQuantization;
		n64_screw_vertices[p + 1] = (float)kN64ScrewPositions[p + 1] / kN64CartridgePositionQuantization;
		n64_screw_vertices[p + 2] = (float)kN64ScrewPositions[p + 2] / kN64CartridgePositionQuantization;
		n64_screw_normals[p + 0] = (float)kN64ScrewNormals[p + 0] / 127.0f;
		n64_screw_normals[p + 1] = (float)kN64ScrewNormals[p + 1] / 127.0f;
		n64_screw_normals[p + 2] = (float)kN64ScrewNormals[p + 2] / 127.0f;
		n64_screw_colors[c + 0] = 0.34f;
		n64_screw_colors[c + 1] = 0.28f;
		n64_screw_colors[c + 2] = 0.12f;
		n64_screw_colors[c + 3] = 1.0f;
	}

	int back_w = 0;
	int back_h = 0;
	uint8_t *back_pixels = stbi_load_from_memory(kN64BackLabelPng, (int)kN64BackLabelPngSize, &back_w, &back_h, NULL, 4);
	if (back_pixels) {
		glGenTextures(1, &n64_back_label_texture);
		glBindTexture(GL_TEXTURE_2D, n64_back_label_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, back_w, back_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, back_pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		stbi_image_free(back_pixels);
	}

	return true;
}

static void DrawN64Cartridge3D(RomSelection *rom, float cx, float cy, float z,
	float width, float yaw, float pitch, bool selected) {
	if (!EnsureN64CartridgeMesh())
		return;

	const float model_scale = width / kN64CartridgeWidth;
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(cx, cy, z);
	glRotatef(pitch, 1.0f, 0.0f, 0.0f);
	glRotatef(yaw, 0.0f, 1.0f, 0.0f);
	glScalef(model_scale, model_scale, model_scale);

	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);

	if (EnsureN64LightingShader()) {
		glDisableClientState(GL_COLOR_ARRAY);
		glUseProgram(n64_lighting_program);
		SetN64LightingRotation(yaw, pitch);

		vglVertexAttribPointerMapped(0, n64_cartridge_vertices);
		vglVertexAttribPointerMapped(1, n64_cartridge_normals);
		SetN64LightingMaterial(selected ? 0.59f : 0.52f, selected ? 0.60f : 0.53f, selected ? 0.63f : 0.56f, 0.56f, 0.22f);
		vglDrawObjects(GL_TRIANGLES, kN64BodyVertexCount);

		vglVertexAttribPointerMapped(0, n64_contact_vertices);
		vglVertexAttribPointerMapped(1, n64_contact_normals);
		SetN64LightingMaterial(0.88f, 0.58f, 0.10f, 0.94f, 0.16f);
		vglDrawObjects(GL_TRIANGLES, kN64ContactVertexCount);

		vglVertexAttribPointerMapped(0, n64_screw_vertices);
		vglVertexAttribPointerMapped(1, n64_screw_normals);
		SetN64LightingMaterial(0.40f, 0.31f, 0.12f, 0.92f, 0.17f);
		vglDrawObjects(GL_TRIANGLES, kN64ScrewVertexCount);

		glUseProgram(0);
	} else {
		// Conservative legacy fallback if runtime shader compilation is unavailable.
		glEnableClientState(GL_COLOR_ARRAY);
		vglVertexPointerMapped(3, n64_cartridge_vertices);
		vglColorPointerMapped(GL_FLOAT, selected ? n64_cartridge_selected_colors : n64_cartridge_colors);
		vglDrawObjects(GL_TRIANGLES, kN64BodyVertexCount);
		vglVertexPointerMapped(3, n64_contact_vertices);
		vglColorPointerMapped(GL_FLOAT, n64_contact_colors);
		vglDrawObjects(GL_TRIANGLES, kN64ContactVertexCount);
		vglVertexPointerMapped(3, n64_screw_vertices);
		vglColorPointerMapped(GL_FLOAT, n64_screw_colors);
		vglDrawObjects(GL_TRIANGLES, kN64ScrewVertexCount);
	}

	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_TEXTURE_2D);

	if (n64_back_label_texture) {
		glBindTexture(GL_TEXTURE_2D, n64_back_label_texture);
		vglVertexPointerMapped(3, n64_back_label_vertices);
		vglTexCoordPointerMapped(n64_back_label_uvs);
		vglDrawObjects(GL_TRIANGLES, kN64BackLabelVertexCount);
	}

	int cover_width = 0;
	int cover_height = 0;
	GLuint cover = GetRomCoverTexture(rom, &cover_width, &cover_height);
	if (!cover || cover_width <= 0 || cover_height <= 0)
		return;

	float *label_uvs = (float*)vglAllocFromScratch(sizeof(float) * 2 * kN64FrontLabelVertexCount);
	if (!label_uvs)
		return;

	for (int i = 0; i < kN64FrontLabelVertexCount; i++) {
		const float base_u = (float)kN64FrontLabelUV[i * 2 + 0] / kN64CartridgeUVQuantization;
		const float base_v = (float)kN64FrontLabelUV[i * 2 + 1] / kN64CartridgeUVQuantization;
		const float cover_top_v = GetN64CoverTopV(base_u);
		label_uvs[i * 2 + 0] = base_u;
		label_uvs[i * 2 + 1] = cover_top_v + (1.0f - base_v) * (1.0f - cover_top_v);
	}

	glBindTexture(GL_TEXTURE_2D, cover);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	vglVertexPointerMapped(3, n64_front_label_vertices);
	vglTexCoordPointerMapped(label_uvs);
	vglDrawObjects(GL_TRIANGLES, kN64FrontLabelVertexCount);
}

static void RestoreMenuGLState() {
	glUseProgram(0);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0f, SCR_WIDTH, SCR_HEIGHT, 0.0f, -1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

static void DrawN64CartridgeCarousel3D(const std::vector<RomSelection*> &visible_roms,
	int selected_index, float carousel_position, float details_anim, float idle_time,
	float manual_yaw, float manual_pitch) {
	if (visible_roms.empty() || !EnsureN64CartridgeMesh())
		return;

	ProcessRomCoverDecodeResults();
	static const int prefetch_offsets[] = { 0, -1, 1, -2, 2, -3, 3 };
	for (int priority = 0; priority < (int)(sizeof(prefetch_offsets) / sizeof(prefetch_offsets[0])); priority++) {
		const int index = selected_index + prefetch_offsets[priority];
		if (index < 0 || index >= (int)visible_roms.size())
			continue;
		RomSelection *rom = visible_roms[index];
		if (!rom->is_online && rom->preview[0] && FindRomCoverCacheSlot(rom->preview) < 0)
			QueueRomCoverDecode(rom->preview, priority);
	}

	glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0f, SCR_WIDTH, SCR_HEIGHT, 0.0f, -1000.0f, 1000.0f);

	const float top = 19.0f * UI_SCALE;
	const float center_x = SCR_WIDTH * 0.5f - details_anim * 135.0f;
	const float center_y = top + 256.0f;
	const int begin = selected_index > 3 ? selected_index - 3 : 0;
	const int end = MIN((int)visible_roms.size() - 1, selected_index + 3);
	auto draw_cartridge = [&](int index, bool is_selected) {
		const float distance = (float)index - carousel_position;
		const float abs_distance = distance < 0.0f ? -distance : distance;
		if (abs_distance > 3.1f)
			return;
		float scale = 1.0f - abs_distance * 0.19f;
		if (scale < 0.50f)
			scale = 0.50f;
		const float idle_phase = idle_time * 1.45f + (float)index * 0.72f;
		const float bob = sinf(idle_phase) * (is_selected ? 5.0f : 2.5f);
		const float idle_yaw = sinf(idle_phase * 0.73f) * (is_selected ? 2.2f : 1.1f);
		const float idle_pitch = cosf(idle_phase * 0.91f) * (is_selected ? 1.2f : 0.7f);
		const float x = center_x + distance * 230.0f;
		const float y = center_y + abs_distance * 14.0f + bob + (is_selected ? 9.0f : 4.0f);
		const float z = 80.0f - abs_distance * 28.0f + cosf(idle_phase * 0.8f) * 2.0f;
		const float base_yaw = is_selected ? -7.5f : -5.0f;
		const float base_pitch = is_selected ? 27.0f : 19.0f;
		const float yaw = -distance * 18.0f + base_yaw + idle_yaw + (is_selected ? manual_yaw : 0.0f);
		const float pitch = base_pitch + idle_pitch + (is_selected ? manual_pitch : 0.0f);
		DrawN64Cartridge3D(visible_roms[index], x, y, z, 286.0f * scale, yaw, pitch, is_selected);
	};

	for (int index = begin; index <= end; index++) {
		if (index == selected_index)
			continue;
		draw_cartridge(index, false);
	}

	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	draw_cartridge(selected_index, true);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	RestoreMenuGLState();
}

// TODO: Use a proper json lib for more safety
void AppendCompatibilityDatabase(const char *file) {
	FILE *f = fopen(file, "rb");
	if (f) {
		fseek(f, 0, SEEK_END);
		uint64_t len = ftell(f);
		fseek(f, 0, SEEK_SET);
		char *buffer = (char*)malloc(len + 1);
		fread(buffer, 1, len, f);
		buffer[len] = 0;
		char *ptr = buffer;
		char *end;
		do {
			ptr = strstr(ptr, "\"title\":");
			if (ptr) {
				CompatibilityList *node = (CompatibilityList*)malloc(sizeof(CompatibilityList));
				
				// Extracting title
				ptr += 10;
				end = strstr(ptr, "\"");
				sceClibMemcpy(node->name, ptr, end - ptr);
				node->name[end - ptr] = 0;
				
				// Extracting tags
				bool perform_slow_check = true;
				ptr += 1000; // Let's skip some data to improve performances
				ptr = strstr(ptr, "\"labels\":");
				ptr = strstr(ptr + 150, "\"name\":");
				ptr += 9;
				if (ptr[0] == 'P') {
					node->playable = true;
					node->ingame_low = false;
					node->ingame_plus = false;
					node->crash = false;
				} else if (ptr[0] == 'C') {
					node->playable = false;
					node->ingame_low = false;
					node->ingame_plus = false;
					node->slow = false;
					node->crash = true;
					perform_slow_check = false;
				} else {
					node->playable = false;
					node->crash = false;
					end = strstr(ptr, "\"");
					if ((end - ptr) == 13) {
						node->ingame_plus = true;
						node->ingame_low = false;
					}else {
						node->ingame_low = true;
						node->ingame_plus = false;
					}
				}
				ptr += 120; // Let's skip some data to improve performances
				if (perform_slow_check) {
					end = ptr;
					ptr = strstr(ptr, "]");
					if ((ptr - end) > 200) node->slow = true;
					else node->slow = false;
				}
				
				ptr += 350; // Let's skip some data to improve performances
				node->next = comp;
				comp = node;
			}
		} while (ptr);
		fclose(f);
		free(buffer);
	}
}

CompatibilityList *SearchForCompatibilityData(const char *name) {
	CompatibilityList *node = comp;
	char tmp[128];
	sprintf(tmp, name);
	stripGameName(tmp);
	while (node) {
		if (strcasecmp(node->name, tmp) == 0) return node;
		node = node->next;
	}
	return nullptr;
}

void SetTagDescription(const char *text) {
	ImGui::SameLine();
	ImGui::TextWrapped(": %s", text);
}

bool filterRoms(RomSelection *p) {
	if (filter_idx < FILTER_LOCAL) {
		if (!p->status) return filter_idx != FILTER_NO_TAGS;
		else {
			if (filter_idx == FILTER_NO_TAGS) return true;
			else if ((!p->status->playable && filter_idx == FILTER_PLAYABLE) ||
				(!p->status->ingame_plus && filter_idx == FILTER_INGAME_PLUS) ||
				(!p->status->ingame_low && filter_idx == FILTER_INGAME_MINUS) ||
				(!p->status->crash && filter_idx == FILTER_CRASH)) {
				return true;
			}
		}
	} else {
		if (!p->is_online && filter_idx == FILTER_ONLINE) return true;
		else if (p->is_online && filter_idx == FILTER_LOCAL) return true;
	}
	return false;
}

static void DrawRomInfoPanel(RomSelection *rom, float anim) {
	if (!rom || anim <= 0.01f)
		return;

	const float panel_width = 350.0f;
	const float top = 19.0f * UI_SCALE;
	const float x = SCR_WIDTH - panel_width * anim;

	ImGui::SetNextWindowPos(ImVec2(x, top), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panel_width, SCR_HEIGHT - top), ImGuiSetCond_Always);
	ImGui::SetNextWindowBgAlpha(0.94f * anim);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::Begin("ROM Details", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);

	ImGui::TextWrapped("%s", rom->title[0] ? rom->title : rom->name);
	ImGui::Separator();

	if (rom->is_online) {
		ImGui::TextWrapped("%s", lang_strings[STR_GAME_NET]);
	} else {
		ImGui::Text("%s: %s", lang_strings[STR_REGION], ROM_GetCountryNameFromID(rom->id.CountryID));
		ImGui::Text("%s: %s", lang_strings[STR_PLAYTIME], FormatPlaytime(rom->playtime));
		ImGui::Text("CRC: %08x%08x-%02x", rom->id.CRC[0], rom->id.CRC[1], rom->id.CountryID);
		ImGui::Text("%s: %s", lang_strings[STR_CIC_TYPE], ROM_GetCicName(rom->cic));
		ImGui::Text("%s: %lu MBs", lang_strings[STR_ROM_SIZE], rom->size);
		ImGui::Text("%s: %s", lang_strings[STR_SAVE_TYPE], ROM_GetSaveTypeName(rom->save));

			if (rom->status) {
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
				ImGui::Text("%s:", lang_strings[STR_TAGS]);
				if (rom->status->playable) {
					ImGui::TextColored(ImVec4(0.0f, 0.75f, 0.0f, 1.0f), "%s", lang_strings[STR_GAME_PLAYABLE]);
					SetTagDescription(lang_strings[STR_PLAYABLE_DESC]);
				}
				if (rom->status->ingame_plus) {
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", lang_strings[STR_GAME_INGAME_PLUS]);
					SetTagDescription(lang_strings[STR_INGAME_PLUS_DESC]);
				}
				if (rom->status->ingame_low) {
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.25f, 1.0f), "%s", lang_strings[STR_GAME_INGAME_MINUS]);
					SetTagDescription(lang_strings[STR_INGAME_MINUS_DESC]);
				}
				if (rom->status->crash) {
					ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", lang_strings[STR_GAME_CRASH]);
					SetTagDescription(lang_strings[STR_CRASH_DESC]);
				}
				if (rom->status->slow) {
					ImGui::TextColored(ImVec4(0.5f, 0.0f, 1.0f, 1.0f), "%s", lang_strings[STR_GAME_SLOW]);
					SetTagDescription(lang_strings[STR_SLOW_DESC]);
				}
			}
	}

	ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 38.0f);
	ImGui::Separator();
	ImGui::Text("%s", lang_strings[STR_ROM_DETAILS_CLOSE]);

	ImGui::End();
	ImGui::PopStyleVar();
}

char *DrawRomSelector(bool skip_reloads) {
	bool selected = false;
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (bg_image != 0xDEADBEEF) DrawBackground();
	DrawMenuBar();
	
	if (last_launched && !skip_reloads) {
		LoadPlaytimeData(last_launched);
		last_launched = nullptr;
	}
		
	if (!list) {
		oldSortOrder = -1;
		
		if (!comp) {
			LoadBackground();
			for (int i = 1; i <= NUM_DB_CHUNKS; i++) {
				char dbname[64];
				sprintf(dbname, "%sdb%ld.json", DAEDALUS_VITA_MAIN_PATH, i);
				AppendCompatibilityDatabase(dbname);
			}
		}
		std::string			full_path;

		IO::FindHandleT		find_handle;
		IO::FindDataT		find_data;
		
		const char *rom_folders[ROMS_FOLDERS_NUM] = {
			DAEDALUS_VITA_PATH_EXT("ux0:" , "Roms/"),
			DAEDALUS_VITA_PATH_EXT("uma0:", "Roms/"),
			DAEDALUS_PSP_PATH_EXT("ux0:" , "Roms/"),
			DAEDALUS_PSP_PATH_EXT("uma0:", "Roms/"),
			gCustomRomPath
		};
		
		for (int i = 0; i < ROMS_FOLDERS_NUM; i++) {
			if(IO::FindFileOpen( rom_folders[i], &find_handle, find_data ))
			{
					do
					{
						const char * rom_filename( find_data.Name );
						if(IsRomfilename( rom_filename ))
						{
							std::string full_path = rom_folders[i];
							full_path += rom_filename;
							RomSelection *node = (RomSelection*)malloc(sizeof(RomSelection));
							memset(node, 0, sizeof(RomSelection));
							node->is_online = false;
							strcpy(node->name, rom_filename);
						strcpy(node->fullpath, full_path.c_str());
						if (ROM_GetRomDetailsByFilename(full_path.c_str(), &node->id, &node->size, &node->cic)) {
							node->size = node->size / (1024 * 1024);
							RomSettings tmpsettings;
							if (!CRomSettingsDB::Get()->GetSettings(node->id, &tmpsettings )) {
								tmpsettings.Reset();
								std::string game_name;
								if (!ROM_GetRomName(full_path.c_str(), game_name )) game_name = full_path;
								game_name = game_name.substr(0, 63);
								tmpsettings.GameName = game_name.c_str();
								CRomSettingsDB::Get()->SetSettings(node->id, tmpsettings);
							}
							strcpy(node->title, tmpsettings.GameName.c_str());
							strcpy(node->preview, tmpsettings.Preview.c_str());
							node->save = tmpsettings.SaveType;
							node->status = SearchForCompatibilityData(node->title);
						}
						LoadPlaytimeData(node);
						node->next = list;
						list = node;
					}
				}
				while(IO::FindFileNext( find_handle, find_data ));

				IO::FindFileClose( find_handle );
			}
		}
		
		if (raw_net_romlist) {
			char *p = (char*)raw_net_romlist;
			while (p) {
				char *r = strcasestr(p, "a href");
				if (r) {
					char name[128], tmp[128];
					r = strstr(r, "\">");
					char *r2 = strcasestr(r, "</a");
					sceClibMemcpy(name, r + 2, (r2 - (r + 2)));
					name[(r2 - (r + 2))] = 0;
						if (name[0] == ' ') {
							int len = strlen(&name[1]);
							memmove(name, &name[1], len);
							name[len] = 0;
						}
						if (IsRomfilename(name) && (!strstr(name, ".zip"))) {
							RomSelection *node = (RomSelection*)malloc(sizeof(RomSelection));
							memset(node, 0, sizeof(RomSelection));
							node->is_online = true;
							strcpy(node->name, name);
						node->next = list;
						node->status = nullptr;
						list = node;
					}
					p = r + 2;
				} else break;
			}
		}
	}
	
	if (oldSortOrder != gSortOrder) {
		oldSortOrder = gSortOrder;
		sort_romlist(list, gSortOrder);
	}
	
	static RomSelection *selected_rom = nullptr;
	static int selected_index = 0;
	static float carousel_position = 0.0f;
	static bool carousel_initialised = false;
	static bool details_open = false;
	static float details_anim = 0.0f;
	static float cartridge_idle_time = 0.0f;
	static float cartridge_manual_yaw = 0.0f;
	static float cartridge_manual_pitch = 0.0f;
	static uint32_t oldpad = 0;
	static bool suppress_carousel_input_until_release = false;
	static int carousel_repeat_direction = 0;
	static float carousel_repeat_hold = 0.0f;
	static float carousel_repeat_accumulator = 0.0f;

	std::vector<RomSelection*> visible_roms;
	for (RomSelection *p = list; p; p = p->next) {
		if (RomMatchesCurrentView(p))
			visible_roms.push_back(p);
	}

	if (visible_roms.empty()) {
		selected_rom = nullptr;
		selected_index = 0;
		carousel_position = 0.0f;
		carousel_initialised = false;
		details_open = false;
	} else {
		int found_index = -1;
		for (int i = 0; i < (int)visible_roms.size(); i++) {
			if (visible_roms[i] == selected_rom) {
				found_index = i;
				break;
			}
		}
		if (found_index < 0) {
			selected_index = 0;
			selected_rom = visible_roms[0];
			carousel_position = 0.0f;
			carousel_initialised = true;
		} else {
			selected_index = found_index;
		}
	}

	SceCtrlData pad = {};
	sceCtrlPeekBufferPositive(0, &pad, 1);
	const float dt = ImGui::GetIO().DeltaTime > 0.0f ? ImGui::GetIO().DeltaTime : (1.0f / 60.0f);
	const uint32_t pressed = pad.buttons & ~oldpad;
	const bool imgui_nav_capture = ImGui::GetIO().NavActive || ImGui::GetIO().WantCaptureKeyboard;
	const bool modal_capture = pendingDialog;

	if (gFrontendMenuActive) {
		if ((pressed & SCE_CTRL_START) && !modal_capture) {
			gFrontendMenuCloseRequest = true;
			gFrontendMenuActive = false;
			suppress_carousel_input_until_release = true;
		}
	} else if ((pressed & SCE_CTRL_START) && !skip_reloads && !modal_capture &&
		!suppress_carousel_input_until_release) {
		gFrontendMenuActive = true;
		gFrontendMenuCloseRequest = false;
		gFrontendMenuFocusRequest = true;
		suppress_carousel_input_until_release = true;
	}

	if (gFrontendMenuActive || imgui_nav_capture || modal_capture)
		suppress_carousel_input_until_release = true;
	if (suppress_carousel_input_until_release && !gFrontendMenuActive &&
		!imgui_nav_capture && !modal_capture && pad.buttons == 0)
		suppress_carousel_input_until_release = false;

	const bool frontend_shortcuts_allowed = !skip_reloads &&
		!gFrontendMenuActive && !imgui_nav_capture && !modal_capture &&
		!suppress_carousel_input_until_release;
	if (frontend_shortcuts_allowed) {
		if (pressed & SCE_CTRL_SQUARE) {
			showDialog(lang_strings[STR_DLG_SEARCH_ROM], apply_rom_name_filter, dummy_func,
				DIALOG_KEYBOARD, rom_name_filter);
			suppress_carousel_input_until_release = true;
		}
		if (pressed & SCE_CTRL_L1) {
			filter_idx--;
			if (filter_idx < 0)
				filter_idx = FILTER_MODES_NUM - 1;
		}
		if (pressed & SCE_CTRL_R1) {
			filter_idx++;
			if (filter_idx >= FILTER_MODES_NUM)
				filter_idx = 0;
		}
	}

	const bool carousel_input_allowed = frontend_shortcuts_allowed && selected_rom;
	if (carousel_input_allowed) {
		auto move_selection = [&](int direction) {
			const int next_index = selected_index + direction;
			if (next_index < 0 || next_index >= (int)visible_roms.size())
				return false;
			selected_index = next_index;
			selected_rom = visible_roms[selected_index];
			return true;
		};

		int held_direction = 0;
		if ((pad.buttons & SCE_CTRL_LEFT) && !(pad.buttons & SCE_CTRL_RIGHT))
			held_direction = -1;
		else if ((pad.buttons & SCE_CTRL_RIGHT) && !(pad.buttons & SCE_CTRL_LEFT))
			held_direction = 1;

		const bool new_left = (pressed & SCE_CTRL_LEFT) != 0;
		const bool new_right = (pressed & SCE_CTRL_RIGHT) != 0;
		if (new_left || new_right) {
			carousel_repeat_direction = new_left ? -1 : 1;
			carousel_repeat_hold = 0.0f;
			carousel_repeat_accumulator = 0.0f;
			move_selection(carousel_repeat_direction);
		} else if (held_direction != 0) {
			if (carousel_repeat_direction != held_direction) {
				carousel_repeat_direction = held_direction;
				carousel_repeat_hold = 0.0f;
				carousel_repeat_accumulator = 0.0f;
			} else {
				carousel_repeat_hold += dt;
				if (carousel_repeat_hold >= 0.28f) {
					const float repeat_interval = carousel_repeat_hold >= 1.10f ? 0.045f : 0.075f;
					carousel_repeat_accumulator += dt;
					int repeats = 0;
					while (carousel_repeat_accumulator >= repeat_interval && repeats < 4) {
						carousel_repeat_accumulator -= repeat_interval;
						if (!move_selection(carousel_repeat_direction))
							break;
						repeats++;
					}
				}
			}
		} else {
			carousel_repeat_direction = 0;
			carousel_repeat_hold = 0.0f;
			carousel_repeat_accumulator = 0.0f;
		}

		if (pressed & SCE_CTRL_TRIANGLE) {
			details_open = !details_open;
		}

		const int right_x = (int)pad.rx - 128;
		const int right_y = (int)pad.ry - 128;
		const int stick_deadzone = 18;
		const float rotate_speed = 135.0f;
		if (right_x > stick_deadzone || right_x < -stick_deadzone)
			cartridge_manual_yaw += ((float)right_x / 127.0f) * rotate_speed * dt;
		if (right_y > stick_deadzone || right_y < -stick_deadzone)
			cartridge_manual_pitch += ((float)right_y / 127.0f) * rotate_speed * dt;
		if (cartridge_manual_yaw > 360.0f) cartridge_manual_yaw -= 360.0f;
		if (cartridge_manual_yaw < -360.0f) cartridge_manual_yaw += 360.0f;
		if (cartridge_manual_pitch > 360.0f) cartridge_manual_pitch -= 360.0f;
		if (cartridge_manual_pitch < -360.0f) cartridge_manual_pitch += 360.0f;

		if (pressed & SCE_CTRL_CROSS) {
			if (selected_rom->is_online) {
				char url[512];
				sprintf(url, "%s%s", gNetRomPath, selected_rom->name);
				queueDownload(lang_strings[STR_DLG_ROM_LAUNCH], url, 8 * 1024 * 1024, dummy_func, MEM_DOWNLOAD);
				sprintf(selectedRom, "/%s.net", selected_rom->name);
			} else {
				strcpy(selectedRom, selected_rom->fullpath);
			}
			selected = true;
		}
	} else {
		carousel_repeat_direction = 0;
		carousel_repeat_hold = 0.0f;
		carousel_repeat_accumulator = 0.0f;
	}
	oldpad = pad.buttons;
	cartridge_idle_time += dt;
	if (cartridge_idle_time > 4096.0f)
		cartridge_idle_time = 0.0f;

	const float carousel_blend = MIN(dt * 12.0f, 1.0f);
	const float panel_blend = MIN(dt * 14.0f, 1.0f);
	if (!carousel_initialised) {
		carousel_position = (float)selected_index;
		carousel_initialised = true;
	} else {
		carousel_position += ((float)selected_index - carousel_position) * carousel_blend;
	}
	details_anim += ((details_open ? 1.0f : 0.0f) - details_anim) * panel_blend;
	DrawN64CartridgeCarousel3D(visible_roms, selected_index, carousel_position, details_anim,
		cartridge_idle_time, cartridge_manual_yaw, cartridge_manual_pitch);

	const float top = 19.0f * UI_SCALE;
	ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::SetNextWindowPos(ImVec2(0.0f, top), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(SCR_WIDTH, SCR_HEIGHT - top), ImGuiSetCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::Begin("ROM Carousel", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav);

	ImDrawList *draw = ImGui::GetWindowDrawList();
	const ImVec2 canvas_min = ImGui::GetWindowPos();
	const ImVec2 canvas_max(canvas_min.x + ImGui::GetWindowWidth(), canvas_min.y + ImGui::GetWindowHeight());

		const float details_panel_width = 350.0f;
		const float carousel_clip_right = SCR_WIDTH - details_panel_width * details_anim;
		draw->PushClipRect(canvas_min, ImVec2(carousel_clip_right, canvas_max.y), true);

		char count_text[64];
		sprintf(count_text, lang_strings[STR_ROM_SELECTOR_GAMES], (unsigned long)visible_roms.size());
	ImVec2 count_size = ImGui::CalcTextSize(count_text);
	draw->AddText(ImVec2(SCR_WIDTH - count_size.x - 18.0f, canvas_min.y + 14.0f),
		ImGui::GetColorU32(ImVec4(0.75f, 0.77f, 0.80f, 0.75f)), count_text);

	if (!visible_roms.empty()) {
		const char *selected_name = selected_rom->title[0] ? selected_rom->title : selected_rom->name;
		ImVec2 title_size = ImGui::CalcTextSize(selected_name);
		float title_width = title_size.x + 34.0f;
		if (title_width > 520.0f)
			title_width = 520.0f;
		const float title_center_x = SCR_WIDTH * 0.5f - details_anim * 135.0f;
		ImVec2 title_min(title_center_x - title_width * 0.5f, canvas_min.y + 388.0f);
		ImVec2 title_max(title_center_x + title_width * 0.5f, canvas_min.y + 419.0f);
		draw->AddRectFilled(title_min, title_max,
			ImGui::GetColorU32(ImVec4(0.04f, 0.045f, 0.05f, 0.82f)), 6.0f);
			DrawCenteredClippedText(draw, ImVec2(title_min.x + 10.0f, title_min.y),
				ImVec2(title_max.x - 10.0f, title_max.y), selected_name,
				ImGui::GetColorU32(ImVec4(0.96f, 0.96f, 0.97f, 1.0f)));
			DrawCompatibilityTagRow(draw, selected_rom, title_center_x, title_max.y + 6.0f);

			const char *hints = lang_strings[STR_ROM_SELECTOR_HINTS];
		ImVec2 hints_size = ImGui::CalcTextSize(hints);
		draw->AddRectFilled(
			ImVec2((SCR_WIDTH - hints_size.x) * 0.5f - 15.0f, canvas_max.y - 36.0f),
			ImVec2((SCR_WIDTH + hints_size.x) * 0.5f + 15.0f, canvas_max.y - 8.0f),
			ImGui::GetColorU32(ImVec4(0.04f, 0.045f, 0.05f, 0.72f)), 5.0f);
		draw->AddText(ImVec2((SCR_WIDTH - hints_size.x) * 0.5f, canvas_max.y - 30.0f),
			ImGui::GetColorU32(ImVec4(0.86f, 0.87f, 0.89f, 0.92f)), hints);
		} else {
				const char *empty_text = lang_strings[STR_ROM_SELECTOR_EMPTY];
		ImVec2 empty_size = ImGui::CalcTextSize(empty_text);
		draw->AddText(ImVec2((SCR_WIDTH - empty_size.x) * 0.5f, canvas_min.y + 245.0f),
				ImGui::GetColorU32(ImVec4(0.86f, 0.87f, 0.89f, 0.92f)), empty_text);
		}
		draw->PopClipRect();

	ImGui::End();
	ImGui::PopStyleVar();

	ImGui::SetNextWindowPos(ImVec2(18.0f, top + 18.0f), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(350.0f, 76.0f), ImGuiSetCond_Always);
	ImGui::SetNextWindowBgAlpha(0.72f);
	ImGui::Begin("ROM Library Controls", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s", lang_strings[STR_SEARCH]);
	ImGui::SameLine();
	ImGui::PushItemWidth(-1.0f);
	const char *search_label = rom_name_filter[0] ? rom_name_filter : "...";
	if (ImGui::Button(search_label, ImVec2(-1.0f, 0.0f)))
		showDialog(lang_strings[STR_DLG_SEARCH_ROM], apply_rom_name_filter, dummy_func, DIALOG_KEYBOARD, rom_name_filter);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s", lang_strings[STR_FILTER_BY]);
	ImGui::SameLine();
	if (ImGui::BeginCombo("##rom_filter", filter_modes[filter_idx])) {
		for (int n = 0; n < FILTER_MODES_NUM; n++) {
			const bool is_selected = filter_idx == n;
			if (ImGui::Selectable(filter_modes[n], is_selected))
				filter_idx = n;
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();
	ImGui::End();

	DrawRomInfoPanel(selected_rom, details_anim);
	DrawPendingAlert();
	
	glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
	ImGui::Render();
	ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
	DrawPendingDialog();
	vglSwapBuffers(GL_FALSE);
	
	if (pendingDownload) {
		switch (cur_download.type) {
		case FILE_DOWNLOAD:
			{
				if (download_file(cur_download.url, TEMP_DOWNLOAD_NAME, cur_download.msg, cur_download.size, true) >= 0)
					cur_download.post_func();
			}
			break;
		case MEM_DOWNLOAD:
			{
				if (download_file(cur_download.url, TEMP_DOWNLOAD_NAME, cur_download.msg, cur_download.size, false) >= 0)
					cur_download.post_func();
			}
			break;
		}
		pendingDownload = false;
	}
	
	if (selected && !skip_reloads && selected_rom) {
		cur_playtime = selected_rom->playtime;
		last_launched = selected_rom;
		if (!selected_rom->is_online)
			CheatCodes_Read(selected_rom->title, "Daedalus.cht", selected_rom->id.CountryID);
		return selectedRom;
	}
	return nullptr;
}
