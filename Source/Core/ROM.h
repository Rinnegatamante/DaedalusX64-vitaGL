/*
Copyright (C) 2001 StrmnNrmn

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

#pragma once

#ifndef CORE_ROM_H_
#define CORE_ROM_H_

#include "ROMImage.h"
#include "Utility/IO.h"
#include <string>

class RomID
{
	public:
		RomID()
		{
			CRC[0] = 0;
			CRC[1] = 0;
			CountryID = 0;
		}

		RomID( u32 crc1, u32 crc2, u8 country_id )
		{
			CRC[0] = crc1;
			CRC[1] = crc2;
			CountryID = country_id;
		}

		explicit RomID( const ROMHeader & header )
		{
			CRC[0] = header.CRC1;
			CRC[1] = header.CRC2;
			CountryID = header.CountryID;
		}

		bool Empty() const
		{
			return CRC[0] == 0 && CRC[1] == 0 && CountryID == 0;
		}

		bool operator==( const RomID & id ) const		{ return Compare( id ) == 0; }
		bool operator!=( const RomID & id ) const		{ return Compare( id ) != 0; }
		bool operator<( const RomID & rhs ) const		{ return Compare( rhs ) < 0; }

		s32 Compare( const RomID & rhs ) const
		{
			s32		diff;

			diff = CRC[0] - rhs.CRC[0];
			if( diff != 0 )
				return diff;

			diff = CRC[1] - rhs.CRC[1];
			if( diff != 0 )
				return diff;

			diff = CountryID - rhs.CountryID;
			if( diff != 0 )
				return diff;

			return 0;
		}

		u32		CRC[2];
		u8		CountryID;
};

#include "RomSettings.h"

struct SRomPreferences;

// Increase this everytime you add a new hack, don't forget to add it in gGameHackNames too !!!
//
//
//*****************************************************************************
//	Hacks for games etc.
//*****************************************************************************
enum EGameHacks
{
	NO_GAME_HACK = 0,
	GOLDEN_EYE,
	SUPER_BOWLING,
	ZELDA_OOT,
	ZELDA_MM,
	TARZAN,
	PMARIO,
	GEX_GECKO,
	WONDER_PROJECTJ2,
	CHAMELEON_TWIST_2,
	BODY_HARVEST,
	AIDYN_CRONICLES,
	ISS64,
	DKR,
	YOSHI,
	EXTREME_G2,
	BUCK_BUMBLE,
	WORMS_ARMAGEDDON,
	SIN_PUNISHMENT,
	DK64,
	BANJO_TOOIE,
	POKEMON_STADIUM,
	QUAKE,
	WCW_NITRO,
	MAX_HACK_NAMES	//DONT CHANGE THIS! AND SHOULD BE LAST ENTRY
};

//*****************************************************************************
//
//*****************************************************************************
struct RomInfo
{
	IO::Filename	mFileName;
	RomID			mRomID;					// The RomID (unique to this rom)

	ROMHeader		rh;						// Copy of the ROM header, correctly byteswapped
	RomSettings 	settings;				// Settings for this rom
	u32				TvType;					// OS_TV_NTSC etc
	ECicType		cic_chip;				// CIC boot chip type
	union
	{
		u64 HACKS_u64;
		struct
		{
			u16			GameHacks:16;			// Hacks for specific games
			u32			LOAD_T1_HACK:1;			//LOAD T1 texture hack
			u32			T1_HACK:1;				//T1 texture hack
			u32			ZELDA_HACK:1;			//for both MM and OOT
			u32			TLUT_HACK:1;			//Texture look up table hack for palette
			u32			ALPHA_HACK:1;			//HACK for AIDYN CHRONICLES
			u32			DISABLE_LBU_OPT:1;		//Disable memory optimation for
			u32			DISABLE_DYNA_CMP:1;		//Hack to disable Cop1 CMP operations in dynarec
			u32			VIEWPORT_HACK:1;		//Hack to force fullscreen viewport
			u32			T0_SKIP_HACK:1;			//Hack for Rayman 2 texts
			u32			SCISSOR_HACK:1;			//Hack to unbind viewport and scissor test region
			u32			SKIP_CPU_REND_HACK:1;	//Hack to disable CPU rendering at boot
			u32			SKIP_MSG_SEND_HACK:1;	//Dummies osSendMesg
			u32			KEEP_MODE_H_HACK:1;		//Hack to prevent RDP OtherMode H reset
			u32			PROJ_HACK:1;			//Hack to vertically flip projection matrix for 3D rendering
			u32			CLEAR_DEPTH_HACK:1;		//Hack to clear depth framebuffer at the beginning of a frame
			u32			CLEAR_SCENE_HACK:1;		//Hack to clear framebuffer at beginning of a frame
			u32			VIHEIGHT_HACK:1;		//Hack to "fix" viHeight scale issues
		};
	};
};

//*****************************************************************************
// Functions
//*****************************************************************************
bool ROM_ReBoot();
void ROM_Unload();
bool ROM_LoadFile();
void ROM_UnloadFile();
bool ROM_LoadFile(const RomID & rom_id, const RomSettings & settings, const SRomPreferences & preferences );

bool ROM_GetRomDetailsByFilename( const char * filename, RomID * id, u32 * rom_size, ECicType * boot_type );
bool ROM_GetRomDetailsByID( const RomID & id, u32 * rom_size, ECicType * boot_type );
bool ROM_GetRomName( const char * filename, std::string & game_name );

const char *	ROM_GetCountryNameFromID( u8 country_id )	DAEDALUS_ATTRIBUTE_PURE;
u32				ROM_GetTvTypeFromID( u8 country_id )		DAEDALUS_ATTRIBUTE_PURE;
const char *	ROM_GetCicTypeName( ECicType cic_type );

//*****************************************************************************
// Externs (urgh)
//*****************************************************************************
extern RomInfo g_ROM;

#if defined(DAEDALUS_ENABLE_DYNAREC_PROFILE) || defined(DAEDALUS_W32)
extern u32 g_dwNumFrames;
#endif

#endif // CORE_ROM_H_
