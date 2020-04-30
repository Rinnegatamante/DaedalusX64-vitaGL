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

#ifdef DAEDALUS_CTR
#include "SysCTR/Utility/MemoryCTR.h"
#elif defined(DAEDALUS_VITA)
#include <vitasdk.h>
#endif

#include "DynaRec/CodeBufferManager.h"
#include "Debug/DBGConsole.h"
#include "CodeGeneratorARM.h"

#define ALIGN(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define CODE_BUFFER_SIZE (16 * 1024 * 1024)
void *dynarec_buffer = nullptr;
SceUID dynarec_memblock;
uint32_t unflushed_ptr = 0;
uint32_t flushed_ptr = 0;

void _InvalidateAndFlushCaches() {
	sceKernelSyncVMDomain(dynarec_memblock, dynarec_buffer, CODE_BUFFER_SIZE);
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
	// mpSecondBuffer is currently unused
	
	// Initializing memblock for dynarec
	dynarec_memblock = sceKernelAllocMemBlockForVM("code", ALIGN(ALIGN(CODE_BUFFER_SIZE, 4 * 1024), 1 * 1024 * 1024));
	sceKernelGetMemBlockBase(dynarec_memblock, &dynarec_buffer);
	mpBuffer = (uint8_t*)dynarec_buffer;

	if (mpBuffer == NULL)
		return false;

	#ifdef DAEDALUS_CTR
	_SetMemoryPermission((unsigned int*)mpBuffer, CODE_BUFFER_SIZE, 7);
	#elif defined(DAEDALUS_VITA)
	sceKernelOpenVMDomain();
	#endif

	mBufferPtr = 0;
	mBufferSize = 0;

	mSecondBufferPtr = 0;
	mSecondBufferSize = 0;

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
		#ifdef DAEDALUS_CTR
		_SetMemoryPermission((unsigned int*)mpBuffer, CODE_BUFFER_SIZE / 4096, 3);
		_SetMemoryPermission((unsigned int*)mpSecondBuffer, CODE_BUFFER_SIZE / 4096, 3);
		
		
		free(mpBuffer);
		free(mpSecondBuffer);
		#elif defined(DAEDALUS_VITA)
		sceKernelFreeMemBlock(dynarec_memblock);
		#endif
		
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
	u32		main_block_size( mPrimaryBuffer.GetSize() );

	mBufferPtr += main_block_size;

	mSecondBufferPtr += mSecondaryBuffer.GetSize();
	mSecondBufferPtr = ((mSecondBufferPtr - 1) & 0xfffffff0) + 0x10; // align to 16-byte boundary

	return main_block_size;
}
