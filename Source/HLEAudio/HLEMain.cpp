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
//
//     ABI ? : Unknown or unsupported UCode
//
AudioHLEInstruction ABIUnknown [0x20] = { // Unknown ABI
	SPU, SPU, SPU, SPU, SPU, SPU, SPU, SPU,
	SPU, SPU, SPU, SPU, SPU, SPU, SPU, SPU,
	SPU, SPU, SPU, SPU, SPU, SPU, SPU, SPU,
	SPU, SPU, SPU, SPU, SPU, SPU, SPU, SPU
};
//---------------------------------------------------------------------------------------------
//
//     ABI 1 : Mario64, WaveRace USA, Golden Eye 007, Quest64, SF Rush
//				 60% of all games use this.  Distributed 3rd Party ABI
//
extern AudioHLEInstruction ABI1[0x20];
//---------------------------------------------------------------------------------------------
//
//     ABI 2 : WaveRace JAP, MarioKart 64, Mario64 JAP RumbleEdition,
//				 Yoshi Story, Pokemon Games, Zelda64, Zelda MoM (miyamoto)
//				 Most NCL or NOA games (Most commands)
extern AudioHLEInstruction ABI2[0x20];
//---------------------------------------------------------------------------------------------
//
//     ABI 3 : DK64, Perfect Dark, Banjo Kazooi, Banjo Tooie
//				 All RARE games except Golden Eye 007
//
extern AudioHLEInstruction ABI3[0x20];

AudioHLEInstruction *ABI = ABIUnknown;
bool bAudioChanged = false;
extern bool isMKABI;
extern bool isZeldaABI;

//*****************************************************************************
//
//*****************************************************************************
void Audio_Reset()
{
	sprintf(cur_audio_ucode, "None");
	bAudioChanged = false;
	isMKABI		  = false;
	isZeldaABI	  = false;
	isMusyx 	  = false;
}

//*****************************************************************************
//
//*****************************************************************************
inline void Audio_Ucode_Detect(OSTask * pTask)
{
	u8* p_base = g_pu8RamBase + (u32)pTask->t.ucode_data;

	if (*(u32*)(p_base) != 0x01)
	{
		if (*(u32*)(p_base + 0x10) == 0x00000001) {
			isMusyx = true;
			sprintf(cur_audio_ucode, "Musyx v1");
		} else {
			ABI = ABI3;
			sprintf(cur_audio_ucode, "ABI3");
		}
	}
	else
	{
		if (*(u32*)(p_base + 0x30) == 0xF0000F00) {
			ABI = ABI1;
			sprintf(cur_audio_ucode, "ABI1");
		} else {
			ABI = ABI2;
			sprintf(cur_audio_ucode, "ABI2");
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
