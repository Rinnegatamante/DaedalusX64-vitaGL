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

#include "stdafx.h"

#include "RSP_HLE.h"

#include "Interrupt.h"
#include "Memory.h"
#include "Core/ROM.h"
#include "Math/MathUtil.h"
#include "OSHLE/ultra_mbi.h"
#include "OSHLE/ultra_rcp.h"
#include "OSHLE/ultra_sptask.h"
#include "Plugins/AudioPlugin.h"
#include "Plugins/GraphicsPlugin.h"
#include "Test/BatchTest.h"
#include "Utility/IO.h"

/* JpegTask.cpp */
extern void jpeg_decode_PS(OSTask *task);
extern void jpeg_decode_PS0(OSTask *task);
extern void jpeg_decode_OB(OSTask *task);

/* RE2Task.cpp + HqvmTask.cpp */
extern "C" {
	extern void resize_bilinear_task(OSTask *task);
	extern void decode_video_frame_task(OSTask *task);
	extern void fill_video_double_buffer_task(OSTask *task);
	extern void hvqm2_decode_task(OSTask *task, int is32);
};

void RSP_HLE_Finished(u32 setbits)
{
	// Need to point to last instr?
	//Memory_DPC_SetRegister(DPC_CURRENT_REG, (u32)pTask->t.data_ptr);

	//
	// Set the SP flags appropriately. The RSP is not running anyway, no need to stop it
	//
	u32 status( Memory_SP_SetRegisterBits(SP_STATUS_REG, setbits) );

	//
	// We've set the SP_STATUS_BROKE flag - better check if it causes an interrupt
	//
	if( status & SP_STATUS_INTR_BREAK )
	{
		Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_SP);
		R4300_Interrupt_UpdateCause3();
	}
}


//

static EProcessResult RSP_HLE_Graphics()
{
	return gGraphicsPlugin->ProcessDList();
}


//

static EProcessResult RSP_HLE_Audio()
{
	return gAudioPlugin->ProcessAList();
}

static u32 sum_bytes(const u8 *bytes, u32 size)
{
	u32 sum = 0;
	const u8 * const bytes_end = bytes + size;

	while (bytes != bytes_end)
		sum += *bytes++;

	return sum;
}

EProcessResult RSP_HLE_Normal(OSTask * task)
{
	// most ucode_boot procedure copy 0xf80 bytes of ucode whatever the ucode_size is.
	// For practical purpose we use a ucode_size = min(0xf80, task->ucode_size)
	u32 sum = sum_bytes(g_pu8RamBase + (u32)task->t.ucode , Min<u32>(task->t.ucode_size, 0xf80) >> 1);

	//DBGConsole_Msg(0, "JPEG Task: Sum=0x%08x", sum);
	switch(sum)
	{
	case 0x2c85a: // Pokemon Stadium Jap Exclusive jpg decompression
		//printf("jpeg_decode_PS0 task\n");
		jpeg_decode_PS0(task);
		return PR_COMPLETED;
	case 0x2caa6: // Zelda OOT, Pokemon Stadium {1,2} jpg decompression
		//printf("jpeg_decode_PS task\n");
		jpeg_decode_PS(task);
		return PR_COMPLETED;
	case 0x130de: // Ogre Battle & Bottom of the 9th background decompression
	case 0x278b0:
		//printf("jpeg_decode_OB task\n");
		jpeg_decode_OB(task);
		return PR_COMPLETED;
	case 0x278: // StoreVe12: found in Zelda Ocarina of Time [misleading task->type == 4]
		//printf("StoreVe12 task\n");
		return PR_COMPLETED;
	case 0x212ee: // GFX: Twintris [misleading task->type == 0]
		//printf("GFX (Twintris) task\n");
		return RSP_HLE_Graphics();
	}
	
	// Resident Evil 2
	sum = sum_bytes(g_pu8RamBase + (u32)task->t.ucode, 256);
	switch(sum)
	{
	case 0x450f:
		//printf("resize_bilinear task\n");
		resize_bilinear_task(task);
		return PR_COMPLETED;
	case 0x3b44:
		//printf("decode_video_frame task\n");
		decode_video_frame_task(task);
		return PR_COMPLETED;
	case 0x3d84:
		//printf("fill_video_double_buffer task\n");
		fill_video_double_buffer_task(task);
		return PR_COMPLETED;
	}
	
	// HVQM
	sum = sum_bytes(g_pu8RamBase + (u32)task->t.ucode, 1488);
	switch (sum) {
	case 0x19495:
		//printf("HVQM SP1 task\n");
		hvqm2_decode_task(task, 0);
		return PR_COMPLETED;
	case 0x19728:
		//printf("HVQM SP2 task\n");
		hvqm2_decode_task(task, 1);
		break;
	}

	return PR_NOT_STARTED;
}

EProcessResult RSP_HLE_CICX105(OSTask * task)
{
	const u32 sum = sum_bytes(g_pu8SpImemBase, 44);
	
	if (sum == 0x9e2) {
		//printf("CICX105\n");
		u32 i;
		u8 * dst = g_pu8RamBase + 0x2fb1f0;
		u8 * src = g_pu8SpImemBase + 0x120;

		/* dma_read(0x1120, 0x1e8, 0x1e8) */
		sceClibMemcpy(g_pu8SpImemBase + 0x120, g_pu8RamBase + 0x1e8, 0x1f0);

		/* dma_write(0x1120, 0x2fb1f0, 0xfe817000) */
		for (i = 0; i < 24; ++i)
		{
			sceClibMemcpy(dst, src, 8);
			dst += 0xff0;
			src += 0x8;

		}
		
		return PR_COMPLETED;
	}

	return PR_NOT_STARTED;
}

void RSP_HLE_ProcessTask()
{	
	OSTask * pTask = (OSTask *)(g_pu8SpMemBase + 0x0FC0);
	
	EProcessResult result = PR_NOT_STARTED;
	
	// Non task
	if(pTask->t.ucode_boot_size > 0x1000)
	{
		result = RSP_HLE_CICX105(pTask);
		RSP_HLE_Finished(SP_STATUS_BROKE|SP_STATUS_HALT);
		return;
	}
	
	//printf("RSP Task: Type: %ld, Ptr: 0x%08X, Size: 0x%04X\n", pTask->t.type, (u32*)(pTask->t.data_ptr), pTask->t.ucode_boot_size);
	
	if (pTask->t.type == M_AUDTASK) {
		result = RSP_HLE_Audio();
	}
	
	if (result == PR_NOT_STARTED) {
		result = RSP_HLE_Normal(pTask);
		
		if (result == PR_NOT_STARTED && pTask->t.type == M_GFXTASK) {
			if(Memory_DPC_GetRegister(DPC_STATUS_REG) & DPC_STATUS_FREEZE)
				return;
			result = RSP_HLE_Graphics();
		}
	}

	// Started and completed. No need to change cores. [synchronously]
	if (result == PR_COMPLETED)
		RSP_HLE_Finished(SP_STATUS_TASKDONE|SP_STATUS_BROKE|SP_STATUS_HALT);
	//else
		//printf("Unknown ucode\n");
}
