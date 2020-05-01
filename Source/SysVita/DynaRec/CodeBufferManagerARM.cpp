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

#include "stdafx.h"
#include <stdio.h>
#include <malloc.h>
#include <vitasdk.h>

#include "Math/MathUtil.h"

#include "DynaRec/CodeBufferManager.h"
#include "Debug/DBGConsole.h"
#include "CodeGeneratorARM.h"

#define CODE_BUFFER_SIZE (16 * 1024 * 1024)

static SceUID dynarec_memblock;

void _DaedalusICacheInvalidate(const void * address, u32 length)
{
	if(length > 0)
		sceKernelSyncVMDomain(dynarec_memblock, address, length);
}

class CCodeBufferManagerARM : public CCodeBufferManager
{
public:
	CCodeBufferManagerARM()
		:	mpBuffer( NULL )
		,	mBufferPtr( 0 )
		,	mBufferSize( 0 )
		,	mpSecondBuffer( NULL )
		,	mSecondBufferPtr( 0 )
		,	mSecondBufferSize( 0 )
	{
	}

	virtual bool			Initialise();
	virtual void			Reset();
	virtual void			Finalise();

	virtual CCodeGenerator *StartNewBlock();
	virtual u32				FinaliseCurrentBlock();

private:

	u8	*					mpBuffer;
	u32						mBufferPtr;
	u32						mBufferSize;

	u8 *					mpSecondBuffer;
	u32						mSecondBufferPtr;
	u32						mSecondBufferSize;

private:
	CAssemblyBuffer			mPrimaryBuffer;
	CAssemblyBuffer			mSecondaryBuffer;
};

//*****************************************************************************
//
//*****************************************************************************
CCodeBufferManager *	CCodeBufferManager::Create()
{
	return new CCodeBufferManagerARM;
}

//*****************************************************************************
//
//*****************************************************************************
bool	CCodeBufferManagerARM::Initialise()
{
	// Initializing memblock for dynarec
	dynarec_memblock = sceKernelAllocMemBlockForVM("code", CODE_BUFFER_SIZE);

	sceKernelGetMemBlockBase(dynarec_memblock, &mpBuffer);

	if (mpBuffer == NULL)
		return false;

	mBufferPtr = 0;
	mBufferSize = 0;

	mSecondBufferPtr = 0;
	mSecondBufferSize = 0;

	sceKernelOpenVMDomain();

	return true;
}

//*****************************************************************************
//
//*****************************************************************************
void	CCodeBufferManagerARM::Reset()
{
	mBufferPtr = 0;
	mSecondBufferPtr = 0;
}

//*****************************************************************************
//
//*****************************************************************************
void	CCodeBufferManagerARM::Finalise()
{
	if (mpBuffer != NULL)
	{
		sceKernelFreeMemBlock(dynarec_memblock);
		
		mpBuffer = NULL;
		mpSecondBuffer = NULL;
	}
}

//*****************************************************************************
//
//*****************************************************************************
CCodeGenerator * CCodeBufferManagerARM::StartNewBlock()
{
	// Round up to 16 byte boundry
	u32 aligned_ptr( (mBufferPtr + 15) & (~15) );

	mBufferPtr = aligned_ptr;

	mPrimaryBuffer.SetBuffer( mpBuffer + mBufferPtr );
	mSecondaryBuffer.SetBuffer( mpSecondBuffer + mSecondBufferPtr );

	return new CCodeGeneratorARM( &mPrimaryBuffer, &mSecondaryBuffer );
}

//*****************************************************************************
//
//*****************************************************************************
u32 CCodeBufferManagerARM::FinaliseCurrentBlock()
{
	const u32 main_block_size( mPrimaryBuffer.GetSize() );
	const u8* p_base( mPrimaryBuffer.GetStartAddress().GetTargetU8P() );

	mBufferPtr += main_block_size;

	#if 0
	mSecondBufferPtr += mSecondaryBuffer.GetSize();
	mSecondBufferPtr = ((mSecondBufferPtr - 1) & 0xfffffff0) + 0x10; // align to 16-byte boundary
	#endif

	_DaedalusICacheInvalidate(p_base, main_block_size);

	return main_block_size;
}
