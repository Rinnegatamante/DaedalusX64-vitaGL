/*
Copyright (C) 2003 Azimer
Copyright (C) 2001,2006 StrmnNrmn

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
//	N.B. This source code is derived from Azimer's Audio plugin (v0.55?)
//	and modified by StrmnNrmn to work with Daedalus PSP. Thanks Azimer!
//	Drop me a line if you get chance :)
//
//

#include <stdio.h>
#include "stdafx.h"
#include "audiohle.h"
#include "AudioHLEProcessor.h"

#include "OSHLE/ultra_sptask.h"

#include "Utility/Profiler.h"

extern "C" {
	extern void musyx_v1_task(OSTask *hle);
};

static bool isMusyx = false;

char cur_audio_ucode[32];

// Audio UCode lists
// Dummy UCode Handler
//
static void SPU( AudioHLECommand command ){}

extern AudioHLEInstruction ABI_Common[0x20];
extern AudioHLEInstruction ABI_GE[0x20];
extern AudioHLEInstruction NAudio[0x20];
extern AudioHLEInstruction NAudio_MP3[0x20];
extern AudioHLEInstruction NAudio_DK[0x20];
extern AudioHLEInstruction NAudio_BK[0x20];
extern AudioHLEInstruction NEAD[0x20];
extern AudioHLEInstruction NEAD_MK[0x20];
extern AudioHLEInstruction NEAD_FZ[0x20];
extern AudioHLEInstruction NEAD_SF[0x20];
extern AudioHLEInstruction NEAD_SFJ[0x20];
extern AudioHLEInstruction NEAD_WRJB[0x20];
extern AudioHLEInstruction NEAD_OOT[0x20];
extern AudioHLEInstruction NEAD_MM[0x20];
extern AudioHLEInstruction NEAD_MMB[0x20];
extern AudioHLEInstruction NEAD_1080[0x20];
extern AudioHLEInstruction NEAD_AC[0x20];
extern AudioHLEInstruction NEAD_YS[0x20];

AudioHLEInstruction *ABI;
bool bAudioChanged = false;
bool gUseMp3 = true;
extern bool isZeldaABI;

//*****************************************************************************
//
//*****************************************************************************
void Audio_Reset()
{
	sprintf(cur_audio_ucode, "None");
	bAudioChanged = false;
	isZeldaABI	  = false;
	isMusyx 	  = false;
}

//*****************************************************************************
//
//*****************************************************************************
inline void Audio_Ucode_Detect(OSTask * pTask)
{
	u8* p_base = g_pu8RamBase + (u32)pTask->t.ucode_data;
	
	u32 v;
	
	if (*(u32*)(p_base) != 0x00000001)
	{
		v = *(u32*)(p_base + 0x10);
		switch (v) {
		case 0x00000001: /* MusyX v1
			RogueSquadron, ResidentEvil2, PolarisSnoCross,
            TheWorldIsNotEnough, RugratsInParis, NBAShowTime,
            HydroThunder, Tarzan, GauntletLegend, Rush2049 */
			isMusyx = true;
			sprintf(cur_audio_ucode, "MusyX v1");
			break;
		case 0x1c58126c: /* DonkeyKong */
			ABI = NAudio_DK;
			sprintf(cur_audio_ucode, "NAudio (DK)");
			break;
		case 0x00001280: /* BanjoKazooie */
			ABI = NAudio_BK;
			sprintf(cur_audio_ucode, "NAudio (BK)");
			break;
		case 0x1ab0140c: /* Conker's Bad Fur Day */
		case 0x1ae8143c: /* NAudio MP3 BanjoTooie, JetForceGemini, MickeySpeedWayUSA, PerfectDark */
			if (gUseMp3) {
				ABI = NAudio_MP3;
				sprintf(cur_audio_ucode, "NAudio MP3");
				break;
			}
		default: /* NAudio */
			ABI = NAudio;
			sprintf(cur_audio_ucode, "NAudio");
			break;
		}
	}
	else
	{
		if (*(u32*)(p_base + 0x30) == 0xf0000f00) {
			v = *(u32*)(p_base + 0x28);
			switch (v) {
				case 0x1dc8138c: /* GoldenEye */
				case 0x1e3c1390: /* BlastCorp, DiddyKongRacing */
					ABI = ABI_GE;
					sprintf(cur_audio_ucode, "ABI (GE)");
					break;
				default: /* Audio ABI */
					ABI = ABI_Common;
					sprintf(cur_audio_ucode, "ABI");
					break;
			}
		} else {
			v = *(u32*)(p_base + 0x10);
			switch (v) {
				case 0x1f4c1230: /* F-Zero X Expansion */
				case 0x1cd01250: /* F-Zero X */
					ABI = NEAD_FZ;
					sprintf(cur_audio_ucode, "NEAD (FZ)");
					break;
				case 0x11181350: /* MarioKart, WaveRace (E) */
					ABI = NEAD_MK;
					sprintf(cur_audio_ucode, "NEAD (MK)");
					break;
				case 0x110412ac: /* WaveRace (J RevB) */
					ABI = NEAD_WRJB;
					sprintf(cur_audio_ucode, "NEAD (WRJB)");
					break;
				case 0x111812e0: /* StarFox (J) */
					ABI = NEAD_SFJ;
					sprintf(cur_audio_ucode, "NEAD (SFJ)");
					break;
				case 0x110412cc: /* StarFox/LylatWars (except J) */
					ABI = NEAD_SF;
					sprintf(cur_audio_ucode, "NEAD (SF)");
					break;
				case 0x1f08122c: /* Yoshi's Story */
					ABI = NEAD_YS;
					sprintf(cur_audio_ucode, "NEAD (YS)");
					break;
				case 0x1f38122c: /* 1080° Snowboarding */
					ABI = NEAD_1080;
					sprintf(cur_audio_ucode, "NEAD (1080)");
					break;
				case 0x1f681230: /* Zelda OoT / Zelda MM (J, J RevA) */
					ABI = NEAD_OOT;
					sprintf(cur_audio_ucode, "NEAD (OOT)");
					break;
				case 0x1f801250: /* Zelda MM (except J, J RevA, E Beta), Pokemon Stadium 2 */
					ABI = NEAD_MM;
					sprintf(cur_audio_ucode, "NEAD (MM)");
					break;
				case 0x109411f8: /* Zelda MM (E Beta) */
					ABI = NEAD_MMB;
					sprintf(cur_audio_ucode, "NEAD (MMB)");
					break;
				case 0x1eac11b8: /* Animal Crossing */
					ABI = NEAD_AC;
					sprintf(cur_audio_ucode, "NEAD (AC)");
					break;
				case 0x00010010: /* MusyX v2 (Indiana Jones, Battle For Naboo) */
				case 0x1f701238: /* Mario Artist Talent Studio */
				default: /* NEAD */
					ABI = NEAD;
					sprintf(cur_audio_ucode, "NEAD");
					break;
			}
		}
	}
}

//*****************************************************************************
//
//*****************************************************************************
void Audio_Ucode()
{
#ifdef DAEDALUS_PROFILE
	DAEDALUS_PROFILE( "HLEMain::Audio_Ucode" );
#endif
	OSTask * pTask = (OSTask *)(g_pu8SpMemBase + 0x0FC0);

	// Only detect ABI once per game
	if ( !bAudioChanged )
	{
		bAudioChanged = true;
		Audio_Ucode_Detect( pTask );
	}

	gAudioHLEState.LoopVal = 0;
	
	if (isMusyx) {
		musyx_v1_task(pTask);
	} else {
		u32 * p_alist = (u32 *)(g_pu8RamBase + (u32)pTask->t.data_ptr);
		u32 ucode_size = (pTask->t.data_size >> 3);

		while( ucode_size )
		{
			AudioHLECommand command;
			command.cmd0 = *p_alist++;
			command.cmd1 = *p_alist++;

			ABI[command.cmd](command);

			--ucode_size;
		}
	}
}
