/*
Copyright (C) 2009 StrmnNrmn

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
#include "stdafx.h"
#include "Microcode.h"

#include "Core/ROM.h"
#include "Core/Memory.h"

#include "Debug/DBGConsole.h"
#include "Utility/AuxFunc.h"

// Limit cache ucode entries to 6
// In theory we should never reach this max
#define MAX_UCODE_CACHE_ENTRIES 6

char cur_ucode[256] = "";

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//                    uCode Config                      //
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

// NoN			No Near clipping
// Rej			Reject polys with one or more points outside screenspace

//F3DEX: Extended fast3d. Vertex cache is 32, up to 18 DL links
//F3DLX: Compatible with F3DEX, GBI, but not sub-pixel accurate. Clipping can be explicitly enabled/disabled
//F3DLX.Rej: No clipping, rejection instead. Vertex cache is 64
//F3FLP.Rej: Like F3DLX.Rej. Vertex cache is 80
//L3DEX: Line processing, Vertex cache is 32.


//
// Used to keep track of used ucode entries
//
struct UcodeInfo
{
	u32 code_base;
	u32 data_base;
	u32 ucode_version;
	bool set;
};

static UcodeInfo gUcodeInfo[ MAX_UCODE_CACHE_ENTRIES ];

static bool	GBIMicrocode_DetectVersionString( u32 data_base, u32 data_size, char * str, u32 str_len )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( data_base < (MAX_RAM_ADDRESS + data_size),"Microcode data its out of bounds %08X", data_base );
	#endif
	const s8 * ram( g_ps8RamBase );

	for ( u32 i {}; i+2 < data_size; i++ )
	{
		if ( ram[ (data_base + i+0) ^ U8_TWIDDLE ] == 'R' &&
			 ram[ (data_base + i+1) ^ U8_TWIDDLE ] == 'S' &&
			 ram[ (data_base + i+2) ^ U8_TWIDDLE ] == 'P' )
		{
			char * p {str};
			char * e {str+str_len};

			// Loop while we haven't filled our buffer, and there's space for our terminator
			while (p+1 < e)
			{
				char c( ram[ (data_base + i)  ^ U8_TWIDDLE ] );
				if( c < ' ')
					break;

				*p++ = c;
				++i;
			}
			*p++ = 0;
			return true;
		}
	}
	return false;
}

static u32 GBIMicrocode_MicrocodeHash(u32 code_base, u32 code_size)
{
	// Needed for Conker's Bad Fur Day
	if( code_size == 0 ) code_size = 0x1000;

	const u8 * ram( g_pu8RamBase );

	u32 hash {};
	for (u32 i = 0; i < code_size; ++i)
	{
		hash = (hash << 4) + hash + ram[ (code_base+i) ^ U8_TWIDDLE ];   // Best hash ever!
	}
	return hash;
}

void GBIMicrocode_Reset()
{
	memset(&gUcodeInfo, 0, sizeof(gUcodeInfo));
}

//*****************************************************************************
//
//*****************************************************************************
struct MicrocodeData
{
	u32	ucode;
	u32 offset;
	u32	hash;
};

static const MicrocodeData gMicrocodeData[] =
{
	//
	//	The only games that need defining are custom ucodes
	//
	{ GBI_CONKER,	GBI_2,	0x60256efc	},	//"RSP Gfx ucode F3DEXBG.NoN fifo 2.08  Yoshitaka Yasumoto 1999 Nintendo.", "Conker's Bad Fur Day"},
	{ GBI_LL,		GBI_1,	0x6d8bec3e	},	//"", "Dark Rift"},
	{ GBI_DKR,		GBI_0,	0x0c10181a	},	//"", "Diddy Kong Racing (v1.0)"},
	{ GBI_DKR,		GBI_0,	0x713311dc	},	//"", "Diddy Kong Racing (v1.1)"},
	{ GBI_GE,		GBI_0,	0x23f92542	},	//"RSP SW Version: 2.0G, 09-30-96", "GoldenEye 007"},
	{ GBI_DKR,		GBI_0,	0x169dcc9d	},	//"", "Jet Force Gemini"},
	{ GBI_LL,		GBI_1,	0x26da8a4c	},	//"", "Last Legion UX"},
	{ GBI_PD,		GBI_0,	0xcac47dc4	},	//"", "Perfect Dark (v1.1)"},
	{ GBI_SE,		GBI_0,	0x6cbb521d	},	//"RSP SW Version: 2.0D, 04-01-96", "Star Wars - Shadows of the Empire (v1.0)"},
	{ GBI_LL,		GBI_1,	0xdd560323	},	//"", "Toukon Road - Brave Spirits"},
	{ GBI_WR,		GBI_0,	0x64cc729d	},	//"RSP SW Version: 2.0D, 04-01-96", "Wave Race 64"},
};

void GBIMicrocode_Cache(u32 index, u32 code_base, u32 data_base, u32 ucode_version)
{
	//
	// If the max of ucode entries is reached, spread it randomly
	// Otherwise we'll keep overriding the last entry
	// 
	if (index >= MAX_UCODE_CACHE_ENTRIES)
	{
		DBGConsole_Msg(0, "Reached max of ucode entries, spreading entry..");
		index = FastRand() % MAX_UCODE_CACHE_ENTRIES;
	}

	UcodeInfo& used(gUcodeInfo[index]);
	used.ucode_version = ucode_version;
	used.code_base = code_base;
	used.data_base = data_base;
	used.set = true;
}

u32	GBIMicrocode_DetectVersion( u32 code_base, u32 code_size, u32 data_base, u32 data_size, CustomMicrocodeCallback custom_callback )
{
	// Cheap way to cache ucodes, don't check for strings (too slow!) but check last used ucode entries which is alot faster than string comparison.
	// This only needed for GBI1/2/SDEX ucodes that use LoadUcode, else we only check when code_base changes, which usually never happens
	//
	u32 i;
	for( i = 0; i < MAX_UCODE_CACHE_ENTRIES; i++ )
	{
		const UcodeInfo &used( gUcodeInfo[ i ] );

		// If this returns false, it means this entry is free to use
		if( used.set == false )
			break;

		if( used.data_base == data_base && used.code_base == code_base)
			return used.ucode_version;
	}

	//
	//	Try to find the version string in the microcode data. This is faster than calculating a crc of the code
	//
	
	GBIMicrocode_DetectVersionString( data_base, data_size, cur_ucode, 256 );

	// It wasn't the same as the last time around, we'll hash it and check if is a custom ucode.
	//
	u32 code_hash = GBIMicrocode_MicrocodeHash( code_base, code_size );
	u32 ucode_version = GBI_0;
	u32 ucode_offset = ~0;

	for ( u32 i = 0; i < ARRAYSIZE(gMicrocodeData); i++ )
	{
		if ( code_hash == gMicrocodeData[i].hash )
		{
			//DBGConsole_Msg(0, "Ucode has been Detected in Array :[M\"%s\", Ucode %d]", cur_ucode, gMicrocodeData[ i ].ucode);
			ucode_version = gMicrocodeData[ i ].ucode;
			ucode_offset = gMicrocodeData[ i ].offset;
		}
	}

	if( ucode_version != GBI_0 )
	{
		// If this a custom ucode, let's build an array based from ucode_offset
		custom_callback( ucode_version, ucode_offset );
	}
	else
	{
		//
		// If it wasn't a custom ucode
		// See if we can identify it by string, if no match was found set default for Fast3D ucode
		//
		const char  *ucodes[] { "F3", "L3", "S2DEX" };
		char 		*match {};

		for(u32 j {}; j<3;j++)
		{
			if( (match = strstr(cur_ucode, ucodes[j])) )
				break;
		}

		if( match )
		{
			if( strstr(match, "fifo") || strstr(match, "xbus") )
			{
				if( !strncmp(match, "S2DEX", 5) )
					ucode_version = GBI_2_S2DEX;
				else
					ucode_version = GBI_2;
			}
			else
			{
				if( !strncmp(match, "S2DEX", 5) )
					ucode_version = GBI_1_S2DEX;
				else
					ucode_version = GBI_1;
			}
		}
	}

	//
	// Retain used ucode info which will be cached
	//
	GBIMicrocode_Cache(i, code_base, data_base, ucode_version);

#ifdef DAEDALUS_DEBUG_CONSOLE
	DBGConsole_Msg(0,"Detected %s Ucode is: [M Ucode %d, 0x%08x, \"%s\", \"%s\"]",ucode_offset == u32(~0) ? "" :"Custom", ucode_version, code_hash, cur_ucode, g_ROM.settings.GameName.c_str() );
#endif

	return ucode_version;
}
