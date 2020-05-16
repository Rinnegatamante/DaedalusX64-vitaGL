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
extern AudioHLEInstruction NEAD[0x20];
extern AudioHLEInstruction NEAD_MK[0x20];

AudioHLEInstruction *ABI;
bool bAudioChanged = false;
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
	
	if (*(u32*)(p_base) != 0x01)
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
		case 0x1AE8143C: /* NAudio MP3
			BanjoTooie, JetForceGemini, MickeySpeedWayUSA, PerfectDark */
			ABI = NAudio_MP3;
			sprintf(cur_audio_ucode, "NAudio MP3");
			break;
		default: /* NAudio */
			ABI = NAudio;
			sprintf(cur_audio_ucode, "NAudio");
			break;
		}
	}
	else
	{
		if (*(u32*)(p_base + 0x30) == 0xF0000F00) {
			v = *(u32*)(p_base + 0x28);
			switch (v) {
				case 0x1DC8138C: /* GoldenEye */
				case 0x1E3C1390: /* BlastCorp, DiddyKongRacing */
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
				case 0x11181350: /* MarioKart, WaveRace (E) */
					ABI = NEAD_MK;
					sprintf(cur_audio_ucode, "NEAD (MK)");
					break;
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
