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

static void WriteValueInvalid( u32 address, u32 value )
{
}

static void WriteValueMapped( u32 address, u32 value )
{
	bool missing;

	u32 physical_addr {TLBEntry::Translate(address, missing)};
	if (physical_addr != 0)
	{
		const u32 rdram_addr = physical_addr & 0x007FFFFF;
		*(u32*)(g_pu8RamBase + rdram_addr) = value;
		RDRAM_MarkDirtyRange(rdram_addr, sizeof(u32));
	}
	else
	{
		R4300_Exception_TLB(address, EXC_WMISS, missing ? UT_VEC : E_VEC);
	}
}

static void WriteValue_8000_807F( u32 address, u32 value )
{
	// Note: Mask is slighty different when EPAK isn't used 0x003FFFFF
	const u32 rdram_addr = address & 0x007FFFFF;
	*(u32 *)((u8 *)g_pMemoryBuffers[MEM_RD_RAM] + rdram_addr) = value;
	RDRAM_MarkDirtyRange(rdram_addr, sizeof(u32));
}

// 0x03F0 0000 to 0x03FF FFFF  RDRAM registers
static void WriteValue_83F0_83F0( u32 address, u32 value )
{
	*(u32 *)((u8 *)g_pMemoryBuffers[MEM_RD_REG0] + (address & 0xFF)) = value;
}

// 0x0400 0000 to 0x0400 FFFF  SP registers
static void WriteValue_8400_8400( u32 address, u32 value )
{
	*(u32 *)((u8 *)g_pMemoryBuffers[MEM_SP_MEM] + (address & 0x1FFF)) = value;
}

static void WriteValue_8404_8404( u32 address, u32 value )
{
	u32 offset {address & 0xFF};

	switch (offset)
	{
	case 0x0:	// SP_MEM_ADDR_REG
	case 0x4:	// SP_DRAM_ADDR_REG
		*(u32 *)((u8 *)g_pMemoryBuffers[MEM_SP_REG] + offset) = value;
		break;

	case 0x8:	//SP_RD_LEN_REG
		*(u32 *)((u8 *)g_pMemoryBuffers[MEM_SP_REG] + offset) = value;
		DMA_SP_CopyFromRDRAM();
		break;

	case 0xc:	//SP_WR_LEN_REG
		*(u32 *)((u8 *)g_pMemoryBuffers[MEM_SP_REG] + offset) = value;
		DMA_SP_CopyToRDRAM();
		break;

	case 0x10:	//SP_STATUS_REG
		MemoryUpdateSPStatus( value );
		break;
	case 0x1c:	// SP_SEMAPHORE_REG ToDO: Read should be 0 too
		*(u32 *)((u8 *)g_pMemoryBuffers[MEM_SP_REG] + offset) = 0;
		break;
/*
	case SP_DMA_FULL_REG:
	case SP_DMA_BUSY_REG:
		// Prevent writing to read-only mem
		break;
*/
	}
}

// 0x04080000 to 0x04080003  SP_PC_REG
// 0x04080004 to 0x04080007  SP_IBIST_REG
static void WriteValue_8408_8408( u32 address, u32 value )
{
	*(u32 *)((u8 *)g_pMemoryBuffers[MEM_SP_PC_REG] + (address & 0xFF)) = value;
}

// 0x0410 0000 to 0x041F FFFF DP Command Registers
static void WriteValue_8410_841F( u32 address, u32 value )
{
	u32 offset {address & 0xFF};

	switch (offset)
	{
	case 0x0:	// DPC_START_REG
		// Write value to current reg too
		Memory_DPC_SetRegister( DPC_START_REG, value );
		Memory_DPC_SetRegister( DPC_CURRENT_REG, value );
		//DBGConsole_Msg( 0, "Wrote to [WDPC_START_REG] 0x%08x", value );
		break;

	case 0x4:	//DPC_END_REG
		Memory_DPC_SetRegister( DPC_END_REG, value );
		//
		// ToDo : Implement ProcessRDPList (LLE DList)
		//
		Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_DP);
		R4300_Interrupt_UpdateCause3();
		break;

	case 0xc:	//DPC_STATUS_REG
		// Set flags etc
		MemoryUpdateDP( value );
		break;
/*
	case DPC_CURRENT_REG:// - Read Only, do we need to handle this?
	case DPC_CLOCK_REG: //- Read Only
	case DPC_BUFBUSY_REG: //- Read Only
	case DPC_PIPEBUSY_REG: //- Read Only
	case DPC_TMEM_REG: //- Read Only
		DBGConsole_Msg( 0, "Wrote to read only DPC reg" );
		break;
*/
	}
}

// 0x0420 0000 to 0x042F FFFF DP Span Registers
static void WriteValue_8420_842F( u32 address, u32 value )
{
}


// 0x0430 0000 to 0x043F FFFF MIPS Interface (MI) Registers
static void WriteValue_8430_843F( u32 address, u32 value )
{
	u32 offset {address & 0xFF};

	switch (offset)
	{
	case 0x0:	// MI_INIT_MODE_REG
		MemoryModeRegMI( value );
		break;
	case 0xc:	// MI_INTR_MASK_REG
		MemoryUpdateMI( value );
		break;
	// Read Only
	/*
	case MI_VERSION_REG:
	case MI_INTR_REG:
		break;
	*/
	}
}

// 0x0440 0000 to 0x044F FFFF Video Interface (VI) Registers
extern void RenderFrameBuffer(u32);
extern bool gCPURendering;
static void WriteValue_8440_844F( u32 address, u32 value )
{
	u32 offset = address & 0xFF;

	switch (offset)
	{
	case 0x0:	// VI_CONTROL_REG
		gGraphicsPlugin->ViStatusChanged();
		break;

	case 0x4:	// VI_ORIGIN_REG
		 // NB: if no display lists executed, interpret framebuffer
		if( gCPURendering )
		{
			RenderFrameBuffer(value & 0x7FFFFF);
		}
		else
		{
			// Builtin video plugin already calls UpdateScreen in DLParser_Process
			gGraphicsPlugin->UpdateScreen();
		}
		break;

	case 0x8:	// VI_WIDTH_REG
		gGraphicsPlugin->ViWidthChanged();
		break;

	case 0x10:	// VI_CURRENT_REG
		Memory_MI_ClrRegisterBits(MI_INTR_REG, MI_INTR_VI);
		R4300_Interrupt_UpdateCause3();
		return;
	}
	
	*(u32 *)((u8 *)g_pMemoryBuffers[MEM_VI_REG] + offset) = value;
}

// 0x0450 0000 to 0x045F FFFF Audio Interface (AI) Registers
static void WriteValue_8450_845F( u32 address, u32 value )
{
	u32 offset {address & 0xFF};

	switch (offset)
	{
	case 0x4:	//AI_LEN_REG
	{
		const u32 ai_length = value & 0x0003FFF8;

		if (CPU_AI_QueueDma(ai_length))
		{
			*(u32 *)((u8 *)g_pMemoryBuffers[MEM_AI_REG] + offset) = ai_length;

			if (gAudioPlugin != NULL)
			{
				gAudioPlugin->LenChanged();
			}
		}
		break;
	}

	case 0x0c:	//AI_STATUS_REG
		Memory_MI_ClrRegisterBits(MI_INTR_REG, MI_INTR_AI);
		R4300_Interrupt_UpdateCause3();
		break;

	case 0x10:	//AI_DACRATE_REG
		*(u32 *)((u8 *)g_pMemoryBuffers[MEM_AI_REG] + offset) = value;

		if (gAudioPlugin != NULL)
		{
			//When we set PAL mode for PAL games it corrects the sound pitch to the same as
			//NTSC games but it will also limit FPS 2% higher as well //Corn
			gAudioPlugin->DacrateChanged( g_ROM.TvType ? CAudioPlugin::ST_NTSC : CAudioPlugin::ST_PAL );
		}
		break;
	default:
		*(u32 *)((u8 *)g_pMemoryBuffers[MEM_AI_REG] + offset) = value;
		break;
	}
}

// 0x0460 0000 to 0x046F FFFF Peripheral Interface (PI) Registers
static void WriteValue_8460_846F( u32 address, u32 value )
{
	u32 offset {address & 0xFF};
	switch (offset)
	{
/*
	case 0x0: // PI_DRAM_ADDR_REG
	case 0x4: // PI_CART_ADDR_REG
		*(u32 *)((u8 *)g_pMemoryBuffers[MEM_PI_REG] + offset) = value;
		break;
*/
	case 0x8:	// PI_RD_LEN_REG
		*(u32 *)((u8 *)g_pMemoryBuffers[MEM_PI_REG] + offset) = value;
		DMA_PI_CopyFromRDRAM();
		break;

	case 0xc:	// PI_WR_LEN_REG
		*(u32 *)((u8 *)g_pMemoryBuffers[MEM_PI_REG] + offset) = value;
		// Do memory transfer
		DMA_PI_CopyToRDRAM();
		break;

	case 0x10:	// PI_STATUS_REG
		MemoryUpdatePI( value );
		break;

	default:
		*(u32 *)((u8 *)g_pMemoryBuffers[MEM_PI_REG] + offset) = value;
		break;
	}
}

// 0x0470 0000 to 0x047F FFFF RDRAM Interface (RI) Registers
static void WriteValue_8470_847F( u32 address, u32 value )
{
	*(u32 *)((u8 *)g_pMemoryBuffers[MEM_RI_REG] + (address & 0xFF)) = value;
}

// 0x0480 0000 to 0x048F FFFF Serial Interface (SI) Registers
static void WriteValue_8480_848F( u32 address, u32 value )
{
	u32 offset {address & 0xFF};
	switch (offset)
	{
	case 0x0:	//SI_DRAM_ADDR_REG
		*(u32 *)((u8 *)g_pMemoryBuffers[MEM_SI_REG] + offset) = value;
		break;

	case 0x4:	// SI_PIF_ADDR_RD64B_REG
		// Trigger DRAM -> PIF DMA
		*(u32 *)((u8 *)g_pMemoryBuffers[MEM_SI_REG] + offset) = value;
		DMA_SI_CopyToDRAM();
		break;

		// Reserved Registers here!
	case 0x10:	//SI_PIF_ADDR_WR64B_REG
		// Trigger DRAM -> PIF DMA
		*(u32 *)((u8 *)g_pMemoryBuffers[MEM_SI_REG] + offset) = value;
		DMA_SI_CopyFromDRAM();
		break;

	case 0x18:	//SI_STATUS_REG
		// Any write to this reg clears interrupts
		Memory_SI_ClrRegisterBits(SI_STATUS_REG, SI_STATUS_INTERRUPT);
		Memory_MI_ClrRegisterBits(MI_INTR_REG, MI_INTR_SI);
		R4300_Interrupt_UpdateCause3();
		break;
	}
}

// 0x1FC0 0000 to 0x1FC0 07BF PIF Boot ROM (Ignored)
// 0x1FC0 07C0 to 0x1FC0 07FF PIF RAM
static void WriteValue_9FC0_9FCF( u32 address, u32 value )
{
	u32 offset = address & 0x0FFF;
	u32 pif_ram_offset = address & 0x3F;

	*(u32 *)((u8 *)g_pMemoryBuffers[MEM_PIF_RAM] + pif_ram_offset) = value;
	if (pif_ram_offset == 0x3C)
	{
		MemoryUpdatePIF();
	}
}

static void WriteValue_FlashRam( u32 address, u32 value )
{
	if (g_ROM.settings.SaveType == SAVE_TYPE_FLASH)
	{
		if ((address&0x1FFFFFFF) == FLASHRAM_WRITE_ADDR)
		{
			Flash_DoCommand( value );
			return;
		}
	}
}

static void WriteValue_ROM( u32 address, u32 value )
{
	// Write to ROM support
	// A Bug's Life and Toy Story 2 write to ROM, add support by storing written value which is used when reading from Rom.
	// TODO: Make this more robust.. // Salvy
	g_pWriteRom = value;
	g_RomWritten = true;
}
