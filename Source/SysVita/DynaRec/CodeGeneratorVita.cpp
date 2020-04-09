/*
Copyright (C) 2005 StrmnNrmn

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
#include "CodeGeneratorVita.h"

#include <limits.h>
#include <stdio.h>

#include <algorithm>

//#include "N64RegisterCachePSP.h"

#include "Config/ConfigOptions.h"
#include "Core/CPU.h"
#include "Core/Memory.h"
#include "Core/R4300.h"
#include "Core/Registers.h"
#include "Core/ROM.h"
#include "Debug/DBGConsole.h"
#include "DynaRec/AssemblyUtils.h"
#include "DynaRec/Trace.h"
#include "Math/MathUtil.h"
#include "OSHLE/ultra_R4300.h"
#include "Utility/Macros.h"
#include "Utility/PrintOpCode.h"
#include "Utility/Profiler.h"

using namespace AssemblyUtils;

//Enable unaligned load/store(used in CBFD, OOT, Rayman2 and PD) //Corn
#define ENABLE_LWR_LWL
//#define ENABLE_SWR_SWL

//Enable to load/store floats directly to/from FPU //Corn
#define ENABLE_LWC1
#define ENABLE_SWC1
//#define ENABLE_LDC1
//#define ENABLE_SDC1

//Define to handle full 64bit checks for SLT,SLTU,SLTI & SLTIU //Corn
//#define ENABLE_64BIT

//Define to check for DIV / 0 //Corn
//#define DIVZEROCHK

//Define to enable exceptions for interpreter calls from DYNAREC
//#define ALLOW_INTERPRETER_EXCEPTIONS



#define NOT_IMPLEMENTED( x )	DAEDALUS_ERROR( x )

extern "C" { const void * g_MemoryLookupTableReadForDynarec = g_MemoryLookupTableRead; }	//Important pointer for Dynarec see DynaRecStubs.s

extern "C" { void _DDIV( s64 Num, s32 Div ); }	//signed 64bit division  //Corn
extern "C" { void _DDIVU( u64 Num, u32 Div ); }	//unsigned 64bit division  //Corn
extern "C" { void _DMULTU( u64 A, u64 B ); }	//unsigned 64bit multiply  //Corn
extern "C" { void _DMULT( s64 A, s64 B ); }	//signed 64bit multiply (result is 64bit not 128bit!)  //Corn

extern "C" { u64 _FloatToDouble( u32 _float); }	//Uses CPU to pass f64/32 thats why its maskerading as u64/32 //Corn
extern "C" { u32 _DoubleToFloat( u64 _double); }	//Uses CPU to pass f64/32 thats why its maskerading as u64/32 //Corn

extern "C" { void _ReturnFromDynaRec(); }
extern "C" { void _DirectExitCheckNoDelay( u32 instructions_executed, u32 exit_pc ); }
extern "C" { void _DirectExitCheckDelay( u32 instructions_executed, u32 exit_pc, u32 target_pc ); }
extern "C" { void _IndirectExitCheck( u32 instructions_executed, CIndirectExitMap * p_map, u32 target_pc ); }

extern "C"
{
	u32 _ReadBitsDirect_u8( u32 address, u32 current_pc );
	u32 _ReadBitsDirect_s8( u32 address, u32 current_pc );
	u32 _ReadBitsDirect_u16( u32 address, u32 current_pc );
	u32 _ReadBitsDirect_s16( u32 address, u32 current_pc );
	u32 _ReadBitsDirect_u32( u32 address, u32 current_pc );

	u32 _ReadBitsDirectBD_u8( u32 address, u32 current_pc );
	u32 _ReadBitsDirectBD_s8( u32 address, u32 current_pc );
	u32 _ReadBitsDirectBD_u16( u32 address, u32 current_pc );
	u32 _ReadBitsDirectBD_s16( u32 address, u32 current_pc );
	u32 _ReadBitsDirectBD_u32( u32 address, u32 current_pc );

	// Dynarec calls this for simplicity
	void Write32BitsForDynaRec( u32 address, u32 value )
	{
		Write32Bits_NoSwizzle( address, value );
	}
	void Write16BitsForDynaRec( u32 address, u16 value )
	{
		Write16Bits_NoSwizzle( address, value );
	}
	void Write8BitsForDynaRec( u32 address, u8 value )
	{
		Write8Bits_NoSwizzle( address, value );
	}

	void _WriteBitsDirect_u32( u32 address, u32 value, u32 current_pc );
	void _WriteBitsDirect_u16( u32 address, u32 value, u32 current_pc );		// Value in low 16 bits
	void _WriteBitsDirect_u8( u32 address, u32 value, u32 current_pc );			// Value in low 8 bits

	void _WriteBitsDirectBD_u32( u32 address, u32 value, u32 current_pc );
	void _WriteBitsDirectBD_u16( u32 address, u32 value, u32 current_pc );		// Value in low 16 bits
	void _WriteBitsDirectBD_u8( u32 address, u32 value, u32 current_pc );			// Value in low 8 bits
}

#define ReadBitsDirect_u8 _ReadBitsDirect_u8
#define ReadBitsDirect_s8 _ReadBitsDirect_s8
#define ReadBitsDirect_u16 _ReadBitsDirect_u16
#define ReadBitsDirect_s16 _ReadBitsDirect_s16
#define ReadBitsDirect_u32 _ReadBitsDirect_u32

#define ReadBitsDirectBD_u8 _ReadBitsDirectBD_u8
#define ReadBitsDirectBD_s8 _ReadBitsDirectBD_s8
#define ReadBitsDirectBD_u16 _ReadBitsDirectBD_u16
#define ReadBitsDirectBD_s16 _ReadBitsDirectBD_s16
#define ReadBitsDirectBD_u32 _ReadBitsDirectBD_u32


#define WriteBitsDirect_u32 _WriteBitsDirect_u32
#define WriteBitsDirect_u16 _WriteBitsDirect_u16
#define WriteBitsDirect_u8 _WriteBitsDirect_u8

#define WriteBitsDirectBD_u32 _WriteBitsDirectBD_u32
#define WriteBitsDirectBD_u16 _WriteBitsDirectBD_u16
#define WriteBitsDirectBD_u8 _WriteBitsDirectBD_u8


bool			gHaveSavedPatchedOps = false;

#define URO_HI_SIGN_EXTEND 0	// Sign extend from src
#define URO_HI_CLEAR	   1	// Clear hi bits

void Dynarec_ClearedCPUStuffToDo()
{

}

void Dynarec_SetCPUStuffToDo()
{

}
