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

#include "stdafx.h"

#include <string.h>

#include "audiohle.h"
#include "AudioHLEProcessor.h"

#include "Math/MathUtil.h"

#include "Debug/DBGConsole.h"

bool isZeldaABI = false;

static u32 gEnv_t3, gEnv_s5, gEnv_s6;
static u16 env[8];



AudioHLEInstruction NEAD[0x20] =
{
    SPNOOP  , ADPCM2       , CLEARBUFF2, UNKNOWN   , ADDMIXER , RESAMPLE2  , UNKNOWN  , SEGMENT2,
    SETBUFF2, DUPLICATE2   , DMEMMOVE2 , LOADADPCM2, MIXER2   , INTERLEAVE2, HILOGAIN , SETLOOP2,
    SPNOOP  , DEINTERLEAVE2, ENVSETUP1 , ENVMIXER2 , LOADBUFF2, SAVEBUFF2  , ENVSETUP2, SPNOOP  ,
    HILOGAIN, SPNOOP       , DUPLICATE2, UNKNOWN   , SPNOOP   , SPNOOP     , SPNOOP   , SPNOOP
};

AudioHLEInstruction NEAD_FZ[0x20] =
{
    SPNOOP  , ADPCM2       , CLEARBUFF2, SPNOOP    , ADDMIXER , RESAMPLE2  , SPNOOP   , SPNOOP  ,
    SETBUFF2, SPNOOP       , DMEMMOVE2 , LOADADPCM2, MIXER2   , INTERLEAVE2, SPNOOP   , SETLOOP2,
    NEAD_16 , DEINTERLEAVE2, ENVSETUP1 , ENVMIXER2 , LOADBUFF2, SAVEBUFF2  , ENVSETUP2, UNKNOWN ,
    SPNOOP  , UNKNOWN      , DUPLICATE2, SPNOOP    , SPNOOP   , SPNOOP     , SPNOOP   , SPNOOP
};


AudioHLEInstruction NEAD_MK[0x20] =
{
    SPNOOP  , ADPCM2       , CLEARBUFF2   , SPNOOP      , SPNOOP   , RESAMPLE2    , SPNOOP   , SEGMENT2,
    SETBUFF2, SPNOOP       , DMEMMOVE2    , LOADADPCM2  , MIXER2   , INTERLEAVE_MK, POLEF    , SETLOOP2,
    NEAD_16 , DEINTERLEAVE2, ENVSETUP_MK  , ENVMIXER_MK , LOADBUFF2, SAVEBUFF2    , ENVSETUP2, SPNOOP  ,
    SPNOOP  , SPNOOP       , SPNOOP       , SPNOOP      , SPNOOP   , SPNOOP       , SPNOOP   , SPNOOP
};

/* NOTES:

  FILTER/SEGMENT - Still needs to be finished up... add FILTER?
  UNKNOWWN #27	 - Is this worth doing?  Looks like a pain in the ass just for WaveRace64
*/
