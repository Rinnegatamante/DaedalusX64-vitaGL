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

//*****************************************************************************
//
//*****************************************************************************
static void GBIMicrocode_SetCustomArray( u32 ucode_version, u32 ucode_offset );

static MicroCodeInstruction gCustomInstruction[256];
#define SetCommand( cmd, func ) gCustomInstruction[ cmd ] = func;

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
struct UcodeUsage
{
	bool ucode_set;
	
	u32 code_base;
	u32 data_base;
	
	UcodeInfo info;
};

static UcodeUsage gUcodeUsage[MAX_UCODE_CACHE_ENTRIES];
extern void log2file(const char *format, ...);
static bool	GBIMicrocode_DetectVersionString( u32 data_base, u32 data_size, char * str, u32 str_len )
{
	const s8 * ram( g_ps8RamBase );

	for ( u32 i = 0; i + 2 < data_size; i++ )
	{
		if ( ram[ (data_base + i+0) ^ U8_TWIDDLE ] == 'R' &&
			 ram[ (data_base + i+1) ^ U8_TWIDDLE ] == 'S' &&
			 ram[ (data_base + i+2) ^ U8_TWIDDLE ] == 'P' )
		{
			char * p = str;
			char * e = str+str_len;

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

	u32 hash = 0;
	for (u32 i = 0; i < code_size; ++i)
	{
		hash = (hash << 4) + hash + ram[ (code_base+i) ^ U8_TWIDDLE ];   // Best hash ever!
	}
	return hash;
}

void GBIMicrocode_Reset()
{
	// Unset any previously cached ucode
	for (u32 i = 0; i < MAX_UCODE_CACHE_ENTRIES; i++)
 		gUcodeUsage[i].ucode_set = false;
}

//*****************************************************************************
//
//*****************************************************************************
struct MicrocodeData
{
	u32	ucode;
	u32 offset;
	u32 stride;
	u32	hash;
	bool beta_persp;
	char name[16];
};

// NOTE: GBI_Acclaim not yet populated due to games using it not properly booting
static const MicrocodeData gMicrocodeData[] =
{
	//
	//   The only games that need defining are custom ucodes  or ucodes that lack a version string in the microcode data
	//
	{ GBI_CONKER,   GBI_2,  2, 0x60256efc, false, "GBI_CONKER"    },   // Conker's Bad Fur Day
	{ GBI_LL,       GBI_1,  2, 0x6d8bec3e, true,  "GBI_LL"        },   // Dark Rift
	{ GBI_DKR,      GBI_0, 10, 0x0c10181a, true,  "GBI_DKR"       },   // Diddy Kong Racing (v1.0)
	{ GBI_DKR,      GBI_0, 10, 0x713311dc, true,  "GBI_DKR"       },   // Diddy Kong Racing (v1.1)
	{ GBI_GE,       GBI_0, 10, 0x23f92542, false, "GBI_GE"        },   // GoldenEye 007
	{ GBI_DKR,      GBI_0, 10, 0x169dcc9d, true,  "GBI_JFG"       },   // Jet Force Gemini
	{ GBI_LL,       GBI_1,  2, 0x26da8a4c, false, "GBI_LL"        },   // Last Legion UX
	{ GBI_PD,       GBI_0, 10, 0xcac47dc4, false, "GBI_PD"        },   // Perfect Dark
	{ GBI_BETA,     GBI_0,  5, 0x6cbb521d, true,  "GBI_BETA"      },   // Star Wars - Shadows of the Empire
	{ GBI_LL,       GBI_1,  2, 0xdd560323, false, "GBI_LL"        },   // Toukon Road - Brave Spirits
	{ GBI_BETA,     GBI_0,  5, 0x64cc729d, true,  "GBI_BETA"      },   // Wave Race 64 (v.1.1)
	{ GBI_1,        GBI_1,  2, 0x9fb58257, true,  "GBI_MK (F3DEX)"},   // Mario Kart 64
	{ GBI_0,        GBI_0, 10, 0xf4c3491b, true,  "GBI_SM (F3DEX)"},   // Super Mario 64
	{ GBI_0,        GBI_0, 10, 0xe908848d, true,  "GBI_CU (F3DEX)"},   // Cruise'n USA
};

UcodeInfo GBIMicrocode_SetCache(u32 index, u32 code_base, u32 data_base, u32 ucode_stride, u32 ucode_version, const MicroCodeInstruction * ucode_function)
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

	UcodeUsage& used(gUcodeUsage[index]);
	used.ucode_set = true;
	used.code_base = code_base;
	used.data_base = data_base;
	
	used.info.func = ucode_function;
	used.info.stride = ucode_stride;
	return used.info;
}

UcodeInfo GBIMicrocode_DetectVersion( u32 code_base, u32 code_size, u32 data_base, u32 data_size )
{
	// Cheap way to cache ucodes, don't check for strings (too slow!) but check last used ucode entries which is alot faster than string comparison.
	// This only needed for GBI1/2/SDEX ucodes that use LoadUcode, else we only check when code_base changes, which usually never happens
	//
	u32 i;
	for( i = 0; i < MAX_UCODE_CACHE_ENTRIES; i++ )
	{
		const UcodeUsage &used(gUcodeUsage[i]);

		// If this returns false, it means this entry is free to use
		if( used.ucode_set == false )
			break;

		if( used.data_base == data_base && used.code_base == code_base)
			return used.info; // Found a match!
	}
	
	// It wasn't the same as the last time around, we'll hash it and check if is a custom ucode.
	//
	u32 code_hash = GBIMicrocode_MicrocodeHash( code_base, code_size );
	bool is_custom_ucode = false;
	
	// Select Fast3D ucode in case there's no match or if the version string its missing
	u32 ucode_version = GBI_0;
	u32 ucode_offset = GBI_0;
	u32 ucode_stride = 10;
	bool ucode_beta_persp = false;
	
	for ( u32 index = 0; index < ARRAYSIZE(gMicrocodeData); index++ )
	{
		if ( code_hash == gMicrocodeData[index].hash )
		{
			is_custom_ucode = true;
			sprintf(cur_ucode, "%s [Hash: 0x%08x]", gMicrocodeData[index].name, code_hash);
			
			ucode_version = gMicrocodeData[index].ucode;
			ucode_stride = gMicrocodeData[index].stride;
			ucode_offset = gMicrocodeData[index].offset;
			ucode_beta_persp = gMicrocodeData[index].beta_persp;
			
			break;
		}
	}
	
	//
	// If it wasn't a custom ucode. Try to detect by checking the version string in the microcode data.
	// This is faster than calculating a CRC of the code
	//
	if (!is_custom_ucode) {
		if (!GBIMicrocode_DetectVersionString(data_base, data_size, cur_ucode, 256)) {
			sprintf(cur_ucode, "Unknown [Hash: 0x%08x]", code_hash);
			DBGConsole_Msg(0, "Unknown GFX microcode, falling back to F3D");
		} else {
			const char  *ucodes[] { "F3D", "L3D", "S2D" };
			char 		*match;
			u32 match_idx = 3;
		
			for(u32 j = 0; j < match_idx; j++)
			{
				if( (match = strstr(cur_ucode, ucodes[j])) ) {
					match_idx = j;
					break;
				}
			}
		
			ucode_stride = 2;
		
			switch (match_idx) {
			case 0: // F3D
				{
					if (!strncmp(match, "F3DAM", 5)) {
						ucode_version = GBI_AM;
						ucode_offset = GBI_2;
						is_custom_ucode = true;
						sprintf(cur_ucode, "GBI_2 (F3DAM) [Hash: 0x%08x]", code_hash);
					} else if (!strncmp(match, "F3DFLX", 6)) {
						ucode_version = ucode_offset = GBI_2;
						sprintf(cur_ucode, "GBI_2 (F3DFLX) [Hash: 0x%08x]", code_hash);
					} else if (!strncmp(match, "F3DZEX", 6)) {
						ucode_version = ucode_offset = GBI_2;
						sprintf(cur_ucode, "GBI_2 (F3DZEX2) [Hash: 0x%08x]", code_hash);
					} else if( strstr(match, "fifo") || strstr(match, "xbus") ) {
						ucode_version = ucode_offset = GBI_2;
						sprintf(cur_ucode, "GBI_2 (F3DEX2) [Hash: 0x%08x]", code_hash);
					} else {
						ucode_version = ucode_offset = GBI_1;
						sprintf(cur_ucode, "GBI_1 (F3DEX) [Hash: 0x%08x]", code_hash);
					}
				}
				break;
			break;
			case 1: // L3D
				{
					if ( strstr(match, "fifo") || strstr(match, "xbus") ) {
						ucode_version = ucode_offset = GBI_2;
						sprintf(cur_ucode, "GBI_2 (L3DEX2) [Hash: 0x%08x]", code_hash);
					} else {
						ucode_version = ucode_offset = GBI_1;
						sprintf(cur_ucode, "GBI_2 (L3DEX) [Hash: 0x%08x]", code_hash);
					}
				}
				break;
			case 2: // S2DEX
				{
					if( strstr(match, "fifo") || strstr(match, "xbus") ) {
						ucode_version = ucode_offset = GBI_2_S2DEX;
						sprintf(cur_ucode, "GBI_2_S2DEX (S2DEX2) [Hash: 0x%08x]", code_hash);
					} else {
						ucode_version = ucode_offset = GBI_1_S2DEX;
						sprintf(cur_ucode, "GBI_1_S2DEX (S2DEX) [Hash: 0x%08x]", code_hash);
					}
				}
				break;
			default:
				{
					ucode_stride = 10;
					sprintf(cur_ucode, "F3D [Hash: 0x%08x]", code_hash);
				}
				break;
			}
		}
	}
	
	if (is_custom_ucode) {
		GBIMicrocode_SetCustomArray(ucode_version, ucode_offset);
		if (ucode_beta_persp) {
			SetCommand(0xb2, DLParser_GBI1_RDPHalf_1);
			SetCommand(0xb3, DLParser_GBI1_RDPHalf_2);
			SetCommand(0xb4, DLParser_GBI0_PerspNorm_Beta);
		}
		return GBIMicrocode_SetCache(i, code_base, data_base, ucode_stride, ucode_version, gCustomInstruction);
	}
	
	return GBIMicrocode_SetCache(i, code_base, data_base, ucode_stride, ucode_version, gNormalInstruction[ucode_version]);
}

//****************************************************'*********************************
// This is called after a custom ucode has been detected. This function gets cached and its only called once per custom ucode set
// Main resaon for this function is to save memory since custom ucodes share a common table
// USAGE:
//		ucode:			custom ucode: (ucode>= 5), defined in GBIVersion enum
//		offset:			offset to a normal ucode which this custom ucode is based of ex GBI0
//*************************************************************************************
static void GBIMicrocode_SetCustomArray( u32 ucode_version, u32 ucode_offset )
{
	for (u32 i = 0; i < 256; i++) {
		gCustomInstruction[i] = gNormalInstruction[ucode_offset][i];
	}
	
	// Start patching to create our custom ucode table
	switch( ucode_version )
	{
		case GBI_GE:
			SetCommand( 0xb4, DLParser_RDPHalf1_GoldenEye);
			SetCommand( 0xbd, DLParser_GBI1_MoveWord);
			break;
		case GBI_BETA:
			SetCommand( 0x04, DLParser_GBI0_Vtx_Beta);
			SetCommand( 0xbf, DLParser_GBI0_Tri1_Beta);
			SetCommand( 0xb1, DLParser_GBI0_Tri2_Beta);
			SetCommand( 0xb5, DLParser_GBI0_Line3D_Beta);
			break;
		case GBI_LL:
			SetCommand( 0x80, DLParser_Last_Legion_0x80);
			SetCommand( 0x00, DLParser_Last_Legion_0x00);
			SetCommand( 0xe4, DLParser_TexRect_Last_Legion);
			break;
		case GBI_PD:
			SetCommand( 0x04, DLParser_Vtx_PD);
			SetCommand( 0x07, DLParser_Set_Vtx_CI_PD);
			SetCommand( 0xb4, DLParser_RDPHalf1_GoldenEye);
			break;
		case GBI_DKR:
			SetCommand( 0x01, DLParser_Mtx_DKR);
			SetCommand( 0x04, DLParser_GBI0_Vtx_DKR);
			SetCommand( 0x05, DLParser_DMA_Tri_DKR);
			SetCommand( 0x07, DLParser_DLInMem);
			SetCommand( 0xbc, DLParser_MoveWord_DKR);
			SetCommand( 0xbf, DLParser_Set_Addr_DKR);
			SetCommand( 0xbb, DLParser_GBI1_Texture_DKR);
			break;
		case GBI_CONKER:
			SetCommand( 0x01, DLParser_Vtx_Conker);
			SetCommand( 0x05, DLParser_Tri1_Conker);
			SetCommand( 0x06, DLParser_Tri2_Conker);
			SetCommand( 0x10, DLParser_Tri4_Conker);
			SetCommand( 0x11, DLParser_Tri4_Conker);
			SetCommand( 0x12, DLParser_Tri4_Conker);
			SetCommand( 0x13, DLParser_Tri4_Conker);
			SetCommand( 0x14, DLParser_Tri4_Conker);
			SetCommand( 0x15, DLParser_Tri4_Conker);
			SetCommand( 0x16, DLParser_Tri4_Conker);
			SetCommand( 0x17, DLParser_Tri4_Conker);
			SetCommand( 0x18, DLParser_Tri4_Conker);
			SetCommand( 0x19, DLParser_Tri4_Conker);
			SetCommand( 0x1a, DLParser_Tri4_Conker);
			SetCommand( 0x1b, DLParser_Tri4_Conker);
			SetCommand( 0x1c, DLParser_Tri4_Conker);
			SetCommand( 0x1d, DLParser_Tri4_Conker);
			SetCommand( 0x1e, DLParser_Tri4_Conker);
			SetCommand( 0x1f, DLParser_Tri4_Conker);
			SetCommand( 0xdb, DLParser_MoveWord_Conker);
			SetCommand( 0xdc, DLParser_MoveMem_Conker);
			break;
		case GBI_ACCLAIM:
			SetCommand( 0xdc, DLParser_MoveMem_Acclaim);
			break;
		case GBI_AM:
			SetCommand( 0x01, DLParser_GBI2_Vtx_AM);
			SetCommand( 0xd7, DLParser_GBI2_Texture_AM);
			SetCommand( 0xdb, DLParser_GBI2_MoveWord_AM);
			break;
		default:
			break;
	}
}