/*
Copyright (C) 2020, Rinnegatamante

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

//
//	N.B. This source code is derived from Mupen64Plus's Audio plugin
//	and modified by Rinnegatamante to work with Daedalus X64.
//

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "stdafx.h"
#include "AudioHLEProcessor.h"
#include "audiohle.h"
#include "Math/MathUtil.h"
#include "Utility/FastMemcpy.h"
#include "OSHLE/ultra_sptask.h"
#include "Core/RDRam.h"

const u16 ResampleLUT[0x100] =
{
	0x0C39, 0x66AD, 0x0D46, 0xFFDF, 0x0B39, 0x6696, 0x0E5F, 0xFFD8,
	0x0A44, 0x6669, 0x0F83, 0xFFD0, 0x095A, 0x6626, 0x10B4, 0xFFC8,
	0x087D, 0x65CD, 0x11F0, 0xFFBF, 0x07AB, 0x655E, 0x1338, 0xFFB6,
	0x06E4, 0x64D9, 0x148C, 0xFFAC, 0x0628, 0x643F, 0x15EB, 0xFFA1,
	0x0577, 0x638F, 0x1756, 0xFF96, 0x04D1, 0x62CB, 0x18CB, 0xFF8A,
	0x0435, 0x61F3, 0x1A4C, 0xFF7E, 0x03A4, 0x6106, 0x1BD7, 0xFF71,
	0x031C, 0x6007, 0x1D6C, 0xFF64, 0x029F, 0x5EF5, 0x1F0B, 0xFF56,
	0x022A, 0x5DD0, 0x20B3, 0xFF48, 0x01BE, 0x5C9A, 0x2264, 0xFF3A,
	0x015B, 0x5B53, 0x241E, 0xFF2C, 0x0101, 0x59FC, 0x25E0, 0xFF1E,
	0x00AE, 0x5896, 0x27A9, 0xFF10, 0x0063, 0x5720, 0x297A, 0xFF02,
	0x001F, 0x559D, 0x2B50, 0xFEF4, 0xFFE2, 0x540D, 0x2D2C, 0xFEE8,
	0xFFAC, 0x5270, 0x2F0D, 0xFEDB, 0xFF7C, 0x50C7, 0x30F3, 0xFED0,
	0xFF53, 0x4F14, 0x32DC, 0xFEC6, 0xFF2E, 0x4D57, 0x34C8, 0xFEBD,
	0xFF0F, 0x4B91, 0x36B6, 0xFEB6, 0xFEF5, 0x49C2, 0x38A5, 0xFEB0,
	0xFEDF, 0x47ED, 0x3A95, 0xFEAC, 0xFECE, 0x4611, 0x3C85, 0xFEAB,
	0xFEC0, 0x4430, 0x3E74, 0xFEAC, 0xFEB6, 0x424A, 0x4060, 0xFEAF,
	0xFEAF, 0x4060, 0x424A, 0xFEB6, 0xFEAC, 0x3E74, 0x4430, 0xFEC0,
	0xFEAB, 0x3C85, 0x4611, 0xFECE, 0xFEAC, 0x3A95, 0x47ED, 0xFEDF,
	0xFEB0, 0x38A5, 0x49C2, 0xFEF5, 0xFEB6, 0x36B6, 0x4B91, 0xFF0F,
	0xFEBD, 0x34C8, 0x4D57, 0xFF2E, 0xFEC6, 0x32DC, 0x4F14, 0xFF53,
	0xFED0, 0x30F3, 0x50C7, 0xFF7C, 0xFEDB, 0x2F0D, 0x5270, 0xFFAC,
	0xFEE8, 0x2D2C, 0x540D, 0xFFE2, 0xFEF4, 0x2B50, 0x559D, 0x001F,
	0xFF02, 0x297A, 0x5720, 0x0063, 0xFF10, 0x27A9, 0x5896, 0x00AE,
	0xFF1E, 0x25E0, 0x59FC, 0x0101, 0xFF2C, 0x241E, 0x5B53, 0x015B,
	0xFF3A, 0x2264, 0x5C9A, 0x01BE, 0xFF48, 0x20B3, 0x5DD0, 0x022A,
	0xFF56, 0x1F0B, 0x5EF5, 0x029F, 0xFF64, 0x1D6C, 0x6007, 0x031C,
	0xFF71, 0x1BD7, 0x6106, 0x03A4, 0xFF7E, 0x1A4C, 0x61F3, 0x0435,
	0xFF8A, 0x18CB, 0x62CB, 0x04D1, 0xFF96, 0x1756, 0x638F, 0x0577,
	0xFFA1, 0x15EB, 0x643F, 0x0628, 0xFFAC, 0x148C, 0x64D9, 0x06E4,
	0xFFB6, 0x1338, 0x655E, 0x07AB, 0xFFBF, 0x11F0, 0x65CD, 0x087D,
	0xFFC8, 0x10B4, 0x6626, 0x095A, 0xFFD0, 0x0F83, 0x6669, 0x0A44,
	0xFFD8, 0x0E5F, 0x6696, 0x0B39, 0xFFDF, 0x0D46, 0x66AD, 0x0C39
};

static inline int align(int x, int n) {
	return (((x >> n) + 1) << n);
}

extern "C" {

/* various constants */
enum { SUBFRAME_SIZE = 192 };
enum { MAX_VOICES = 32 };

enum { SAMPLE_BUFFER_SIZE = 0x200 };


enum {
    SFD_VOICE_COUNT     = 0x0,
    SFD_SFX_INDEX       = 0x2,
    SFD_VOICE_BITMASK   = 0x4,
    SFD_STATE_PTR       = 0x8,
    SFD_SFX_PTR         = 0xc,
    SFD_VOICES          = 0x10,

    /* v2 only */
    SFD2_10_PTR         = 0x10,
    SFD2_14_BITMASK     = 0x14,
    SFD2_15_BITMASK     = 0x15,
    SFD2_16_BITMASK     = 0x16,
    SFD2_18_PTR         = 0x18,
    SFD2_1C_PTR         = 0x1c,
    SFD2_20_PTR         = 0x20,
    SFD2_24_PTR         = 0x24,
    SFD2_VOICES         = 0x28
};

enum {
    VOICE_ENV_BEGIN         = 0x00,
    VOICE_ENV_STEP          = 0x10,
    VOICE_PITCH_Q16         = 0x20,
    VOICE_PITCH_SHIFT       = 0x22,
    VOICE_CATSRC_0          = 0x24,
    VOICE_CATSRC_1          = 0x30,
    VOICE_ADPCM_FRAMES      = 0x3c,
    VOICE_SKIP_SAMPLES      = 0x3e,

    /* for PCM16 */
    VOICE_U16_40            = 0x40,
    VOICE_U16_42            = 0x42,

    /* for ADPCM */
    VOICE_ADPCM_TABLE_PTR   = 0x40,

    VOICE_INTERLEAVED_PTR   = 0x44,
    VOICE_END_POINT         = 0x48,
    VOICE_RESTART_POINT     = 0x4a,
    VOICE_U16_4C            = 0x4c,
    VOICE_U16_4E            = 0x4e,

    VOICE_SIZE              = 0x50
};

enum {
    CATSRC_PTR1     = 0x00,
    CATSRC_PTR2     = 0x04,
    CATSRC_SIZE1    = 0x08,
    CATSRC_SIZE2    = 0x0a
};

enum {
    STATE_LAST_SAMPLE   = 0x0,
    STATE_BASE_VOL      = 0x100,
    STATE_CC0           = 0x110,
    STATE_740_LAST4_V1  = 0x290,

    STATE_740_LAST4_V2  = 0x110
};

enum {
    SFX_CBUFFER_PTR     = 0x00,
    SFX_CBUFFER_LENGTH  = 0x04,
    SFX_TAP_COUNT       = 0x08,
    SFX_FIR4_HGAIN      = 0x0a,
    SFX_TAP_DELAYS      = 0x0c,
    SFX_TAP_GAINS       = 0x2c,
    SFX_U16_3C          = 0x3c,
    SFX_U16_3E          = 0x3e,
    SFX_FIR4_HCOEFFS    = 0x40
};


/* struct definition */
typedef struct {
    /* internal subframes */
    int16_t left[SUBFRAME_SIZE];
    int16_t right[SUBFRAME_SIZE];
    int16_t cc0[SUBFRAME_SIZE];
    int16_t e50[SUBFRAME_SIZE];

    /* internal subframes base volumes */
    int32_t base_vol[4];

    /* */
    int16_t subframe_740_last4[4];
} musyx_t;

typedef void (*mix_sfx_with_main_subframes_t)(musyx_t *musyx, const int16_t *subframe,
                                              const uint16_t* gains);

/* helper functions prototypes */
static void load_base_vol(OSTask *hle, int32_t *base_vol, uint32_t address);
static void save_base_vol(OSTask *hle, const int32_t *base_vol, uint32_t address);
static void update_base_vol(OSTask *hle, int32_t *base_vol,
                            uint32_t voice_mask, uint32_t last_sample_ptr,
                            uint8_t mask_15, uint32_t ptr_24);

static void init_subframes_v1(musyx_t *musyx);
static void init_subframes_v2(musyx_t *musyx);

static uint32_t voice_stage(OSTask *hle, musyx_t *musyx,
                            uint32_t voice_ptr, uint32_t last_sample_ptr);

static void dma_cat8(OSTask *hle, uint8_t *dst, uint32_t catsrc_ptr);
static void dma_cat16(OSTask *hle, uint16_t *dst, uint32_t catsrc_ptr);

static void load_samples_PCM16(OSTask *hle, uint32_t voice_ptr, int16_t *samples,
                               unsigned *segbase, unsigned *offset);
static void load_samples_ADPCM(OSTask *hle, uint32_t voice_ptr, int16_t *samples,
                               unsigned *segbase, unsigned *offset);

static void adpcm_decode_frames(OSTask *hle,
                                int16_t *dst, const uint8_t *src,
                                const int16_t *table, uint8_t count,
                                uint8_t skip_samples);

static void adpcm_predict_frame(int16_t *dst, const uint8_t *src,
                                const uint8_t *nibbles,
                                unsigned int rshift);

static void mix_voice_samples(OSTask *hle, musyx_t *musyx,
                              uint32_t voice_ptr, const int16_t *samples,
                              unsigned segbase, unsigned offset, uint32_t last_sample_ptr);

static void sfx_stage(OSTask *hle,
                      mix_sfx_with_main_subframes_t mix_sfx_with_main_subframes,
                      musyx_t *musyx, uint32_t sfx_ptr, uint16_t idx);

static void mix_sfx_with_main_subframes_v1(musyx_t *musyx, const int16_t *subframe,
                                           const uint16_t* gains);
static void mix_sfx_with_main_subframes_v2(musyx_t *musyx, const int16_t *subframe,
                                           const uint16_t* gains);

static void mix_samples(int16_t *y, int16_t x, int16_t hgain);
static void mix_subframes(int16_t *y, const int16_t *x, int16_t hgain);
static void mix_fir4(int16_t *y, const int16_t *x, int16_t hgain, const int16_t *hcoeffs);


static void interleave_stage_v1(OSTask *hle, musyx_t *musyx,
                                uint32_t output_ptr);

static void interleave_stage_v2(OSTask *hle, musyx_t *musyx,
                                uint16_t mask_16, uint32_t ptr_18,
                                uint32_t ptr_1c, uint32_t output_ptr);

static int32_t dot4(const int16_t *x, const int16_t *y)
{
    size_t i;
    int32_t accu = 0;

    for (i = 0; i < 4; ++i)
        accu = clamp_s16(accu + (((int32_t)x[i] * (int32_t)y[i]) >> 15));

    return accu;
}

int32_t rdot(size_t n, const int16_t *x, const int16_t *y)
{
    int32_t accu = 0;

    y += n;

    while (n != 0) {
        accu += *(x++) * *(--y);
        --n;
    }

    return accu;
}

void adpcm_compute_residuals(int16_t* dst, const int16_t* src,
        const int16_t* cb_entry, const int16_t* last_samples, size_t count)
{
    const int16_t* const book1 = cb_entry;
    const int16_t* const book2 = cb_entry + 8;

    const int16_t l1 = last_samples[0];
    const int16_t l2 = last_samples[1];

    size_t i;

    for(i = 0; i < count; ++i) {
        int32_t accu = (int32_t)src[i] << 11;
        accu += book1[i]*l1 + book2[i]*l2 + rdot(i, book2, src);
        dst[i] = clamp_s16(accu >> 11);
   }
}

static inline int16_t adpcm_predict_sample(uint8_t byte, uint8_t mask,
        unsigned lshift, unsigned rshift)
{
    int16_t sample = (uint16_t)(byte & mask) << lshift;
    sample >>= rshift; /* signed */
    return sample;
}

/**************************************************************************
 * MusyX v1 audio ucode
 **************************************************************************/
void musyx_v1_task(OSTask *hle)
{
    uint32_t sfd_ptr   = (u32)hle->t.data_ptr;
    uint32_t sfd_count = (u32)hle->t.data_size;
    uint32_t state_ptr;
    musyx_t musyx;

    state_ptr = *(u32*)(g_pu8RamBase + sfd_ptr + SFD_STATE_PTR);

    /* load initial state */
    load_base_vol(hle, musyx.base_vol, state_ptr + STATE_BASE_VOL);
    rdram_read_many_u16((uint16_t *)musyx.cc0, state_ptr + STATE_CC0, SUBFRAME_SIZE);
    rdram_read_many_u16((uint16_t *)musyx.subframe_740_last4, state_ptr + STATE_740_LAST4_V1, 4);

    for (;;) {
        /* parse SFD structure */
        uint16_t sfx_index   = *(u16*)(g_pu8RamBase + sfd_ptr + SFD_SFX_INDEX);
        uint32_t voice_mask  = *(u16*)(g_pu8RamBase + sfd_ptr + SFD_VOICE_BITMASK);
        uint32_t sfx_ptr     = *(u32*)(g_pu8RamBase + sfd_ptr + SFD_SFX_PTR);
        uint32_t voice_ptr       = sfd_ptr + SFD_VOICES;
        uint32_t last_sample_ptr = state_ptr + STATE_LAST_SAMPLE;
        uint32_t output_ptr;

        /* initialize internal subframes using updated base volumes */
        update_base_vol(hle, musyx.base_vol, voice_mask, last_sample_ptr, 0, 0);
        init_subframes_v1(&musyx);

        /* active voices get mixed into L,R,cc0,e50 subframes (optional) */
        output_ptr = voice_stage(hle, &musyx, voice_ptr, last_sample_ptr);

        /* apply delay-based effects (optional) */
        sfx_stage(hle, mix_sfx_with_main_subframes_v1,
                  &musyx, sfx_ptr, sfx_index);

        /* emit interleaved L,R subframes */
        interleave_stage_v1(hle, &musyx, output_ptr);

        --sfd_count;
        if (sfd_count == 0)
            break;

        sfd_ptr += SFD_VOICES + MAX_VOICES * VOICE_SIZE;
        state_ptr = *(u32*)(g_pu8RamBase + sfd_ptr + SFD_STATE_PTR);
    }

    /* writeback updated state */
    save_base_vol(hle, musyx.base_vol, state_ptr + STATE_BASE_VOL);
    rdram_write_many_u16((uint16_t *)musyx.cc0, state_ptr + STATE_CC0, SUBFRAME_SIZE);
    rdram_write_many_u16((uint16_t *)musyx.subframe_740_last4, state_ptr + STATE_740_LAST4_V1, 4);
}

static void load_base_vol(OSTask *hle, int32_t *base_vol, uint32_t address)
{
    base_vol[0] = ((uint32_t)(*(u16*)(g_pu8RamBase + address)) << 16) | (*(u16*)(g_pu8RamBase + address +  8));
    base_vol[1] = ((uint32_t)(*(u16*)(g_pu8RamBase + address + 2)) << 16) | (*(u16*)(g_pu8RamBase + address + 10));
    base_vol[2] = ((uint32_t)(*(u16*)(g_pu8RamBase + address + 4)) << 16) | (*(u16*)(g_pu8RamBase + address + 12));
    base_vol[3] = ((uint32_t)(*(u16*)(g_pu8RamBase + address + 6)) << 16) | (*(u16*)(g_pu8RamBase + address + 14));
}

static void save_base_vol(OSTask *hle, const int32_t *base_vol, uint32_t address)
{
    unsigned k;

    for (k = 0; k < 4; ++k) {
        *(u16*)(g_pu8RamBase + address) = (uint16_t)(base_vol[k] >> 16);
        address += 2;
    }

    for (k = 0; k < 4; ++k) {
        *(u16*)(g_pu8RamBase + address) = (uint16_t)(base_vol[k]);
        address += 2;
    }
}

static void update_base_vol(OSTask *hle, int32_t *base_vol,
                            uint32_t voice_mask, uint32_t last_sample_ptr,
                            uint8_t mask_15, uint32_t ptr_24)
{
    unsigned i, k;
    uint32_t mask;

    /* optim: skip voices contributions entirely if voice_mask is empty */
    if (voice_mask != 0) {
        for (i = 0, mask = 1; i < MAX_VOICES;
             ++i, mask <<= 1, last_sample_ptr += 8) {
            if ((voice_mask & mask) == 0)
                continue;

            for (k = 0; k < 4; ++k)
                base_vol[k] += (int16_t)*(u16*)(g_pu8RamBase + last_sample_ptr + k * 2);
        }
    }

    /* optim: skip contributions entirely if mask_15 is empty */
    if (mask_15 != 0) {
        for(i = 0, mask = 1; i < 4;
                ++i, mask <<= 1, ptr_24 += 8) {
            if ((mask_15 & mask) == 0)
                continue;

            for(k = 0; k < 4; ++k)
                base_vol[k] += (int16_t)*(u16*)(g_pu8RamBase + ptr_24 + k * 2);
        }
    }

    /* apply 3% decay */
    for (k = 0; k < 4; ++k)
        base_vol[k] = (base_vol[k] * 0x0000f850) >> 16;
}

static void init_subframes_v1(musyx_t *musyx)
{
    unsigned i;

    int16_t base_cc0 = clamp_s16(musyx->base_vol[2]);
    int16_t base_e50 = clamp_s16(musyx->base_vol[3]);

    int16_t *left  = musyx->left;
    int16_t *right = musyx->right;
    int16_t *cc0   = musyx->cc0;
    int16_t *e50   = musyx->e50;

    for (i = 0; i < SUBFRAME_SIZE; ++i) {
        *(e50++)    = base_e50;
        *(left++)   = clamp_s16(*cc0 + base_cc0);
        *(right++)  = clamp_s16(-*cc0 - base_cc0);
        *(cc0++)    = 0;
    }
}

/* Process voices, and returns interleaved subframe destination address */
static uint32_t voice_stage(OSTask *hle, musyx_t *musyx,
                            uint32_t voice_ptr, uint32_t last_sample_ptr)
{
    uint32_t output_ptr;
    int i = 0;

    /* voice stage can be skipped if first voice has no samples */
    if (*(u16*)(g_pu8RamBase + voice_ptr + VOICE_CATSRC_0 + CATSRC_SIZE1) == 0) {
        output_ptr = *(u32*)(g_pu8RamBase + voice_ptr + VOICE_INTERLEAVED_PTR);
    } else {
        /* otherwise process voices until a non null output_ptr is encountered */
        for (;;) {
            /* load voice samples (PCM16 or APDCM) */
            int16_t samples[SAMPLE_BUFFER_SIZE];
            unsigned segbase;
            unsigned offset;

            if (*(u8*)(g_pu8RamBase + voice_ptr + VOICE_ADPCM_FRAMES) == 0)
                load_samples_PCM16(hle, voice_ptr, samples, &segbase, &offset);
            else
                load_samples_ADPCM(hle, voice_ptr, samples, &segbase, &offset);

            /* mix them with each internal subframes */
            mix_voice_samples(hle, musyx, voice_ptr, samples, segbase, offset,
                              last_sample_ptr + i * 8);

            /* check break condition */
            output_ptr = *(u32*)(g_pu8RamBase + voice_ptr + VOICE_INTERLEAVED_PTR);
            if (output_ptr != 0)
                break;

            /* next voice */
            ++i;
            voice_ptr += VOICE_SIZE;
        }
    }

    return output_ptr;
}

static void dma_cat8(OSTask *hle, uint8_t *dst, uint32_t catsrc_ptr)
{
    uint32_t ptr1  = *(u32*)(g_pu8RamBase + catsrc_ptr + CATSRC_PTR1);
    uint32_t ptr2  = *(u32*)(g_pu8RamBase + catsrc_ptr + CATSRC_PTR2);
    uint16_t size1 = *(u16*)(g_pu8RamBase + catsrc_ptr + CATSRC_SIZE1);
    uint16_t size2 = *(u16*)(g_pu8RamBase + catsrc_ptr + CATSRC_SIZE2);

    size_t count1 = size1;
    size_t count2 = size2;

    rdram_read_many_u8(dst, ptr1, count1);

    if (size2 == 0)
        return;

    rdram_read_many_u8(dst + count1, ptr2, count2);
}

static void dma_cat16(OSTask *hle, uint16_t *dst, uint32_t catsrc_ptr)
{
    uint32_t ptr1  = *(u32*)(g_pu8RamBase + catsrc_ptr + CATSRC_PTR1);
    uint32_t ptr2  = *(u32*)(g_pu8RamBase + catsrc_ptr + CATSRC_PTR2);
    uint16_t size1 = *(u16*)(g_pu8RamBase + catsrc_ptr + CATSRC_SIZE1);
    uint16_t size2 = *(u16*)(g_pu8RamBase + catsrc_ptr + CATSRC_SIZE2);

    size_t count1 = size1 >> 1;
    size_t count2 = size2 >> 1;

    rdram_read_many_u16(dst, ptr1, count1);

    if (size2 == 0)
        return;

    rdram_read_many_u16(dst + count1, ptr2, count2);
}

static void load_samples_PCM16(OSTask *hle, uint32_t voice_ptr, int16_t *samples,
                               unsigned *segbase, unsigned *offset)
{

    uint8_t  u8_3e  = *(u8*)(g_pu8RamBase + voice_ptr + VOICE_SKIP_SAMPLES);
    uint16_t u16_40 = *(u16*)(g_pu8RamBase + voice_ptr + VOICE_U16_40);
    uint16_t u16_42 = *(u16*)(g_pu8RamBase + voice_ptr + VOICE_U16_42);

    unsigned count = align(u16_40 + u8_3e, 4);

    *segbase = SAMPLE_BUFFER_SIZE - count;
    *offset  = u8_3e;

    dma_cat16(hle, (uint16_t *)samples + *segbase, voice_ptr + VOICE_CATSRC_0);

    if (u16_42 != 0)
        dma_cat16(hle, (uint16_t *)samples, voice_ptr + VOICE_CATSRC_1);
}

static void load_samples_ADPCM(OSTask *hle, uint32_t voice_ptr, int16_t *samples,
                               unsigned *segbase, unsigned *offset)
{
    /* decompressed samples cannot exceed 0x400 bytes;
     * ADPCM has a compression ratio of 5/16 */
    uint8_t buffer[SAMPLE_BUFFER_SIZE * 2 * 5 / 16];
    int16_t adpcm_table[128];

    uint8_t u8_3c = *(u8*)(g_pu8RamBase + voice_ptr + VOICE_ADPCM_FRAMES    );
    uint8_t u8_3d = u8_3c + 1;
    uint8_t u8_3e = *(u8*)(g_pu8RamBase + voice_ptr + VOICE_SKIP_SAMPLES    );
    uint8_t u8_3f = u8_3e + 1;
    uint32_t adpcm_table_ptr = *(u32*)(g_pu8RamBase + voice_ptr + VOICE_ADPCM_TABLE_PTR);
    unsigned count;

    count = u8_3c << 5;

    *segbase = SAMPLE_BUFFER_SIZE - count;
    *offset  = u8_3e & 0x1f;

    dma_cat8(hle, buffer, voice_ptr + VOICE_CATSRC_0);
    adpcm_decode_frames(hle, samples + *segbase, buffer, adpcm_table, u8_3c, u8_3e);

    if (u8_3d != 0) {
        dma_cat8(hle, buffer, voice_ptr + VOICE_CATSRC_1);
        adpcm_decode_frames(hle, samples, buffer, adpcm_table, u8_3d, u8_3f);
    }
}

static void adpcm_decode_frames(OSTask *hle,
                                int16_t *dst, const uint8_t *src,
                                const int16_t *table, uint8_t count,
                                uint8_t skip_samples)
{
    int16_t frame[32];
    const uint8_t *nibbles = src + 8;
    unsigned i;
    bool jump_gap = false;

    if (skip_samples >= 32) {
        jump_gap = true;
        nibbles += 16;
        src += 4;
    }

    for (i = 0; i < count; ++i) {
        uint8_t c2 = nibbles[0];

        const int16_t *book = (c2 & 0xf0) + table;
        unsigned int rshift = (c2 & 0x0f);

        adpcm_predict_frame(frame, src, nibbles, rshift);

        memcpy(dst, frame, 2 * sizeof(frame[0]));
        adpcm_compute_residuals(dst +  2, frame +  2, book, dst     , 6);
        adpcm_compute_residuals(dst +  8, frame +  8, book, dst +  6, 8);
        adpcm_compute_residuals(dst + 16, frame + 16, book, dst + 14, 8);
        adpcm_compute_residuals(dst + 24, frame + 24, book, dst + 22, 8);

        if (jump_gap) {
            nibbles += 8;
            src += 32;
        }

        jump_gap = !jump_gap;
        nibbles += 16;
        src += 4;
        dst += 32;
    }
}

static void adpcm_predict_frame(int16_t *dst, const uint8_t *src,
                                const uint8_t *nibbles,
                                unsigned int rshift)
{
    unsigned int i;

    *(dst++) = (src[0] << 8) | src[1];
    *(dst++) = (src[2] << 8) | src[3];

    for (i = 1; i < 16; ++i) {
        uint8_t byte = nibbles[i];

        *(dst++) = adpcm_predict_sample(byte, 0xf0,  8, rshift);
        *(dst++) = adpcm_predict_sample(byte, 0x0f, 12, rshift);
    }
}

static void mix_voice_samples(OSTask *hle, musyx_t *musyx,
                              uint32_t voice_ptr, const int16_t *samples,
                              unsigned segbase, unsigned offset, uint32_t last_sample_ptr)
{
    int i, k;

    /* parse VOICE structure */
    const uint16_t pitch_q16   = *(u16*)(g_pu8RamBase + voice_ptr + VOICE_PITCH_Q16);
    const uint16_t pitch_shift = *(u16*)(g_pu8RamBase + voice_ptr + VOICE_PITCH_SHIFT); /* Q4.12 */

    const uint16_t end_point     = *(u16*)(g_pu8RamBase + voice_ptr + VOICE_END_POINT);
    const uint16_t restart_point = *(u16*)(g_pu8RamBase + voice_ptr + VOICE_RESTART_POINT);

    const uint16_t u16_4e = *(u16*)(g_pu8RamBase + voice_ptr + VOICE_U16_4E);

    /* init values and pointers */
    const int16_t       *sample         = samples + segbase + offset + u16_4e;
    const int16_t *const sample_end     = samples + segbase + end_point;
    const int16_t *const sample_restart = samples + (restart_point & 0x7fff) +
                                          (((restart_point & 0x8000) != 0) ? 0x000 : segbase);


    uint32_t pitch_accu = pitch_q16;
    uint32_t pitch_step = pitch_shift << 4;

    int32_t  v4_env[4];
    int32_t  v4_env_step[4];
    int16_t *v4_dst[4];
    int16_t  v4[4];

    rdram_read_many_u32((uint32_t *)v4_env,      voice_ptr + VOICE_ENV_BEGIN, 4);
    rdram_read_many_u32((uint32_t *)v4_env_step, voice_ptr + VOICE_ENV_STEP,  4);

    v4_dst[0] = musyx->left;
    v4_dst[1] = musyx->right;
    v4_dst[2] = musyx->cc0;
    v4_dst[3] = musyx->e50;

    for (i = 0; i < SUBFRAME_SIZE; ++i) {
        /* update sample and lut pointers and then pitch_accu */
        const int16_t *lut = ((int16_t*)ResampleLUT + ((pitch_accu & 0xfc00) >> 8));
        int dist;
        int16_t v;

        sample += (pitch_accu >> 16);
        pitch_accu &= 0xffff;
        pitch_accu += pitch_step;

        /* handle end/restart points */
        dist = sample - sample_end;
        if (dist >= 0)
            sample = sample_restart + dist;

        /* apply resample filter */
        v = clamp_s16(dot4(sample, lut));

        for (k = 0; k < 4; ++k) {
            /* envmix */
            int32_t accu = (v * (v4_env[k] >> 16)) >> 15;
            v4[k] = clamp_s16(accu);
            *(v4_dst[k]) = clamp_s16(accu + *(v4_dst[k]));

            /* update envelopes and dst pointers */
            ++(v4_dst[k]);
            v4_env[k] += v4_env_step[k];
        }
    }

    /* save last resampled sample */
    rdram_write_many_u16((uint16_t *)v4, last_sample_ptr, 4);
}


static void sfx_stage(OSTask *hle, mix_sfx_with_main_subframes_t mix_sfx_with_main_subframes,
                      musyx_t *musyx, uint32_t sfx_ptr, uint16_t idx)
{
    unsigned int i;

    int16_t buffer[SUBFRAME_SIZE + 4];
    int16_t *subframe = buffer + 4;

    uint32_t tap_delays[8];
    int16_t tap_gains[8];
    int16_t fir4_hcoeffs[4];

    int16_t delayed[SUBFRAME_SIZE];
    int dpos, dlength;

    const uint32_t pos = idx * SUBFRAME_SIZE;

    uint32_t cbuffer_ptr;
    uint32_t cbuffer_length;
    uint16_t tap_count;
    int16_t fir4_hgain;
    uint16_t sfx_gains[2];

    if (sfx_ptr == 0)
        return;

    /* load sfx  parameters */
    cbuffer_ptr    = *(u32*)(g_pu8RamBase + sfx_ptr + SFX_CBUFFER_PTR);
    cbuffer_length = *(u32*)(g_pu8RamBase + sfx_ptr + SFX_CBUFFER_LENGTH);

    tap_count      = *(u16*)(g_pu8RamBase + sfx_ptr + SFX_TAP_COUNT);

    rdram_read_many_u32(tap_delays, sfx_ptr + SFX_TAP_DELAYS, 8);
    rdram_read_many_u16((uint16_t *)tap_gains,  sfx_ptr + SFX_TAP_GAINS,  8);

    fir4_hgain     = *(u16*)(g_pu8RamBase + sfx_ptr + SFX_FIR4_HGAIN);
    rdram_read_many_u16((uint16_t *)fir4_hcoeffs, sfx_ptr + SFX_FIR4_HCOEFFS, 4);

    sfx_gains[0]   = *(u16*)(g_pu8RamBase + sfx_ptr + SFX_U16_3C);
    sfx_gains[1]   = *(u16*)(g_pu8RamBase + sfx_ptr + SFX_U16_3E);

    /* mix up to 8 delayed subframes */
    memset(subframe, 0, SUBFRAME_SIZE * sizeof(subframe[0]));
    for (i = 0; i < tap_count; ++i) {

        dpos = pos - tap_delays[i];
        if (dpos <= 0)
            dpos += cbuffer_length;
        dlength = SUBFRAME_SIZE;

        if ((uint32_t)(dpos + SUBFRAME_SIZE) > cbuffer_length) {
            dlength = cbuffer_length - dpos;
            rdram_read_many_u16((uint16_t *)delayed + dlength, cbuffer_ptr, SUBFRAME_SIZE - dlength);
        }

        rdram_read_many_u16((uint16_t *)delayed, cbuffer_ptr + dpos * 2, dlength);

        mix_subframes(subframe, delayed, tap_gains[i]);
    }

    /* add resulting subframe to main subframes */
    mix_sfx_with_main_subframes(musyx, subframe, sfx_gains);

    /* apply FIR4 filter and writeback filtered result */
    memcpy(buffer, musyx->subframe_740_last4, 4 * sizeof(int16_t));
    memcpy(musyx->subframe_740_last4, subframe + SUBFRAME_SIZE - 4, 4 * sizeof(int16_t));
    mix_fir4(musyx->e50, buffer + 1, fir4_hgain, fir4_hcoeffs);
    rdram_write_many_u16((uint16_t *)musyx->e50, cbuffer_ptr + pos * 2, SUBFRAME_SIZE);
}

static void mix_sfx_with_main_subframes_v1(musyx_t *musyx, const int16_t *subframe, const uint16_t* gains)
{
    unsigned i;

    for (i = 0; i < SUBFRAME_SIZE; ++i) {
        int16_t v = subframe[i];
        musyx->left[i]  = clamp_s16(musyx->left[i]  + v);
        musyx->right[i] = clamp_s16(musyx->right[i] + v);
    }
}

static void mix_samples(int16_t *y, int16_t x, int16_t hgain)
{
    *y = clamp_s16(*y + ((x * hgain + 0x4000) >> 15));
}

static void mix_subframes(int16_t *y, const int16_t *x, int16_t hgain)
{
    unsigned int i;

    for (i = 0; i < SUBFRAME_SIZE; ++i)
        mix_samples(&y[i], x[i], hgain);
}

static void mix_fir4(int16_t *y, const int16_t *x, int16_t hgain, const int16_t *hcoeffs)
{
    unsigned int i;
    int32_t h[4];

    h[0] = (hgain * hcoeffs[0]) >> 15;
    h[1] = (hgain * hcoeffs[1]) >> 15;
    h[2] = (hgain * hcoeffs[2]) >> 15;
    h[3] = (hgain * hcoeffs[3]) >> 15;

    for (i = 0; i < SUBFRAME_SIZE; ++i) {
        int32_t v = (h[0] * x[i] + h[1] * x[i + 1] + h[2] * x[i + 2] + h[3] * x[i + 3]) >> 15;
        y[i] = clamp_s16(y[i] + v);
    }
}

static void interleave_stage_v1(OSTask *hle, musyx_t *musyx, uint32_t output_ptr)
{
    size_t i;

    int16_t base_left;
    int16_t base_right;

    int16_t *left;
    int16_t *right;
    uint32_t *dst;

    base_left  = clamp_s16(musyx->base_vol[0]);
    base_right = clamp_s16(musyx->base_vol[1]);

    left  = musyx->left;
    right = musyx->right;
    dst  = (u32*)(g_pu8RamBase + output_ptr);

    for (i = 0; i < SUBFRAME_SIZE; ++i) {
        uint16_t l = clamp_s16(*(left++)  + base_left);
        uint16_t r = clamp_s16(*(right++) + base_right);

        *(dst++) = (l << 16) | r;
    }
}

};