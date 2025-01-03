/*
Copyright (C) 2020 MasterFeizz

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
#include <stdio.h>
#include "stdafx.h"

#include "Math/MathUtil.h"
#include "DynaRec/AssemblyUtils.h"

extern SceUID dynarec_memblock;

namespace AssemblyUtils
{

//*****************************************************************************
//	Patch a long jump to target the specified location.
//	Return true if the patching succeeded (i.e. within range), false otherwise
//*****************************************************************************
bool	PatchJumpLong( CJumpLocation jump, CCodeLabel target )
{
	u32* p_jump_addr( reinterpret_cast< u32* >( jump.GetWritableU8P() ) );

	u32 address = target.GetTargetU32();

	s32 offset = jump.GetOffset(target) - 8;
	
	offset >>= 2;
	p_jump_addr[0] = (p_jump_addr[0] &0xFF000000)| (offset & 0xFFFFFF);
	
	// All jumps are 32 bit offsets, and so always succeed.
	return true;
}

bool	PatchJumpLongAndFlush( CJumpLocation jump, CCodeLabel target )
{
	u32* p_jump_addr( reinterpret_cast< u32* >( jump.GetWritableU8P() ) );

	u32 address = target.GetTargetU32();

	s32 offset = jump.GetOffset(target) - 8;
	
	offset >>= 2;
	p_jump_addr[0] = (p_jump_addr[0] &0xFF000000)| (offset & 0xFFFFFF);
	
#if 1
	sceKernelSyncVMDomain(dynarec_memblock, (void*)jump.GetTargetU8P(), 4);
#else
	kuKernelFlushCaches((void *)jump.GetTargetU8P(), 4);
#endif

	return true;
}

}
