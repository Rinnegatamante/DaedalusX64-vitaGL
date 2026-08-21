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
#include "Dynamo.h"

#include <stdio.h>

#include <algorithm>
#include <vector>

#include "ROM.h"
#include "CPU.h"
#include "Registers.h"					// For REG_?? defines
#include "Memory.h"
#include "Interrupt.h"
#include "R4300.h"

#include "Config/ConfigOptions.h"
#include "DynaRec/Fragment.h"
#include "DynaRec/FragmentCache.h"
#include "DynaRec/TraceRecorder.h"
#include "OSHLE/patch.h"				// GetCorrectOp
#include "OSHLE/ultra_R4300.h"
#include "Utility/IO.h"
#include "Utility/Macros.h"
#include "Utility/Synchroniser.h"

#ifdef DAEDALUS_ENABLE_DYNAREC

// These values are very sensitive to change in some games so be carefull!!! //Corn
// War God is sensitive to gHotTraceThreshold
// PD is sensitive to gMaxHotTraceMapSize
#define TRACE_SIZE 1024

static const u32					gMaxFragmentCacheSize = (8192 + 1024); //Maximum amount of fragments in the cache
static const u32					gMaxHotTraceMapSize = (2048 + TRACE_SIZE);
static const u32					gHotTraceThreshold = 10;	//How many times interpreter has to loop a trace before it becomes hot and sent to dynarec

//typedef CMemoryPoolAllocator< std::pair< const u32, u32 > > MyAllocator;
//std::map< u32, u32, std::less<u32>, MyAllocator >				gHotTraceCountMap;
//std::map< u32, u32, std::less<u32>, boost::pool_allocator<std::pair< const u32, u32 > > >				gHotTraceCountMap;
std::map< u32, u32 >				gHotTraceCountMap {};
CFragmentCache						gFragmentCache {};
static bool							gResetFragmentCache = false;
struct SPendingICacheRange
{
	u32 Address;
	u32 Length;
};

static std::vector<SPendingICacheRange> gPendingICacheRanges {};

static void							CPU_HandleDynaRecOnBranch( bool backwards, bool trace_already_enabled );
static void							CPU_UpdateTrace( u32 address, OpCode op_code, bool branch_delay_slot, bool branch_taken );
static void							CPU_CreateAndAddFragment();
static void							CPU_QueueICacheValidationRange( u32 address, u32 length );
static void							CPU_ResolvePendingICacheInvalidations();

static void CPU_QueueICacheValidationRange( u32 address, u32 length )
{
	if( length == 0 )
		return;

	const u32 physical_start = address & 0x1fffffffu;
	const u64 physical_end64 = (u64)physical_start + (u64)length;
	if( physical_end64 > 0x20000000ULL )
	{
		gPendingICacheRanges.clear();
		gResetFragmentCache = true;
		return;
	}

	u32 merged_start = physical_start;
	u32 merged_end = (u32)physical_end64;

	for( auto it = gPendingICacheRanges.begin(); it != gPendingICacheRanges.end(); )
	{
		const u32 range_start = it->Address & 0x1fffffffu;
		const u32 range_end = range_start + it->Length;
		if( merged_end < range_start || merged_start > range_end )
		{
			++it;
			continue;
		}

		merged_start = std::min( merged_start, range_start );
		merged_end = std::max( merged_end, range_end );
		it = gPendingICacheRanges.erase( it );
	}

	SPendingICacheRange merged;
	merged.Address = 0x80000000u | merged_start;
	merged.Length = merged_end - merged_start;
	gPendingICacheRanges.push_back( merged );
}

static void CPU_ResolvePendingICacheInvalidations()
{
	if( gPendingICacheRanges.empty() )
		return;

	if( gResetFragmentCache )
	{
		gPendingICacheRanges.clear();
		return;
	}

	EFragmentCacheInvalidationResult final_result = FCIR_NO_OVERLAP;

	for( const SPendingICacheRange & range : gPendingICacheRanges )
	{
		const EFragmentCacheInvalidationResult result = gFragmentCache.ValidateInvalidation(
			range.Address, range.Length, nullptr, nullptr, nullptr, nullptr );

		if( result == FCIR_CHANGED || result == FCIR_UNVERIFIABLE )
		{
			final_result = result;
			break;
		}
		if( result == FCIR_UNCHANGED )
			final_result = FCIR_UNCHANGED;
	}

	if( final_result == FCIR_CHANGED || final_result == FCIR_UNVERIFIABLE )
		gResetFragmentCache = true;

	gPendingICacheRanges.clear();
}

//*****************************************************************************
//	Indicate that the instruction cache is invalid
//	(we have to dump the dynarec contents and start over, but this is
//	better than crashing :) )
//*****************************************************************************
void R4300_CALL_TYPE CPU_InvalidateICache()
{
	gPendingICacheRanges.clear();
	CPU_ResetFragmentCache();
}

//*****************************************************************************
//
//*****************************************************************************
void CPU_DynarecEnable()
{
	gDynarecEnabled = true;
	gCPUState.AddJob(CPU_CHANGE_CORE);
}

//*****************************************************************************
// If fragments overlap dynarec has to start all over which is very costly
//*****************************************************************************
void R4300_CALL_TYPE CPU_InvalidateICacheRange( u32 address, u32 length )
{
	if( gFragmentCache.ShouldInvalidateOnWrite( address, length ) )
	{
		CPU_QueueICacheValidationRange( address, length );
	}
}


//*****************************************************************************
//	Execute a single MIPS op. The conditionals for the templated arguments
//	are completely optimised away by the compiler.
//
//	DynaRec:		Run this function with dynarec enabled
//	TranslateOp:	Use this to translate breakpoints/patches to original op
//					before execution.
//*****************************************************************************
template< bool TraceEnabled > DAEDALUS_FORCEINLINE void CPU_EXECUTE_OP()
{

	u8 * p_Instruction {};
	CPU_FETCH_INSTRUCTION( p_Instruction, gCPUState.CurrentPC );
	OpCode op_code = *(OpCode*)p_Instruction;

	// Cache instruction base pointer (used for SpeedHack() @ R4300.0)
	gLastAddress = p_Instruction;

#ifdef DAEDALUS_BREAKPOINTS_ENABLED
	op_code = GetCorrectOp( op_code );
#endif

	if( TraceEnabled )
	{
		u32		pc( gCPUState.CurrentPC ) ;
		bool	branch_delay_slot( gCPUState.Delay == EXEC_DELAY );

		R4300_ExecuteInstruction(op_code);
		gGPR[0]._u64 = 0;	//Ensure r0 is zero

		bool	branch_taken( gCPUState.Delay == DO_DELAY );

		CPU_UpdateTrace( pc, op_code, branch_delay_slot, branch_taken );
	}
	else
	{
		R4300_ExecuteInstruction(op_code);
		gGPR[0]._u64 = 0;	//Ensure r0 is zero
	}
	// Increment count register
	gCPUState.CPUControl[C0_COUNT]._u32 = gCPUState.CPUControl[C0_COUNT]._u32 + g_ROM.settings.CountPerOp;

	if (CPU_ProcessEventCycles( g_ROM.settings.CountPerOp ) )
	{
		CPU_HANDLE_COUNT_INTERRUPT();
	}

	switch (gCPUState.Delay)
	{
	case DO_DELAY:
		// We've got a delayed instruction to execute. Increment
		// PC as normal, so that subsequent instruction is executed
		INCREMENT_PC();
		gCPUState.Delay = EXEC_DELAY;

		break;
	case EXEC_DELAY:
		{
			bool	backwards( gCPUState.TargetPC <= gCPUState.CurrentPC );

			// We've just executed the delayed instr. Now carry out jump as stored in gCPUState.TargetPC;
			CPU_SetPC(gCPUState.TargetPC);
			gCPUState.Delay = NO_DELAY;

			CPU_HandleDynaRecOnBranch( backwards, TraceEnabled );
		}
		break;
	case NO_DELAY:
		// Normal operation - just increment the PC
		INCREMENT_PC();
		break;
	default:
		NODEFAULT;
	}
}


//*****************************************************************************
//
//*****************************************************************************
void	CPU_ResetFragmentCache()
{
	// Need to make sure this happens at a safe point, so we use a flag
	gPendingICacheRanges.clear();
	gResetFragmentCache	= true;
}

//*****************************************************************************
// Keep executing instructions until there are other tasks to do (i.e. gCPUState.GetStuffToDo() is set)
// Process these tasks and loop
//*****************************************************************************
template < bool DynaRec, bool TraceEnabled > void CPU_Go()
{
	while (CPU_KeepRunning())
	{
		//
		// Keep executing ops as long as there's nothing to do
		//
		u32	stuff_to_do( gCPUState.GetStuffToDo() );
		while(stuff_to_do == 0)
		{
			CPU_EXECUTE_OP< TraceEnabled >();

			stuff_to_do = gCPUState.GetStuffToDo();
		}

		if( TraceEnabled && (stuff_to_do != CPU_CHANGE_CORE) )
		{
			if(gTraceRecorder.IsTraceActive())
			{

#ifdef ALLOW_TRACES_WHICH_EXCEPT
				if(stuff_to_do == CPU_CHECK_INTERRUPTS && gCPUState.Delay == NO_DELAY )		// Note checking for exactly equal, not just that it's set
				{
					//DBGConsole_Msg( 0, "Adding chunk at %08x after interrupt\n", gTraceRecorder.GetStartTraceAddress() );
					gTraceRecorder.StopTrace( gCPUState.CurrentPC );
					CPU_CreateAndAddFragment();
				}
#endif

				gTraceRecorder.AbortTrace();		// Abort any traces that were terminated through an interrupt etc
			}
			CPU_SelectCore();
		}

		if (CPU_CheckStuffToDo())
			break;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void CPU_CreateAndAddFragment()
{
	CFragment * p_fragment( gTraceRecorder.CreateFragment( gFragmentCache.GetCodeBufferManager() ) );

	if( p_fragment != nullptr )
	{
		gHotTraceCountMap.erase( p_fragment->GetEntryAddress() );
		gFragmentCache.InsertFragment( p_fragment );
	}
}

//*****************************************************************************
//
//*****************************************************************************
void CPU_UpdateTrace( u32 address, OpCode op_code, bool branch_delay_slot, bool branch_taken )
{
	CFragment * p_address_fragment( gFragmentCache.LookupFragmentQ( address ) );

	if( gTraceRecorder.UpdateTrace( address, branch_delay_slot, branch_taken, op_code, p_address_fragment ) == CTraceRecorder::UTS_CREATE_FRAGMENT )
	{
		CPU_CreateAndAddFragment();
		CPU_SelectCore();
	}
}

//*****************************************************************************
//
//*****************************************************************************
void CPU_HandleDynaRecOnBranch( bool backwards, bool trace_already_enabled )
{
	bool	start_of_trace( false );

	if( backwards )
	{
		start_of_trace = true;
	}

	bool	change_core( false );

	while( gCPUState.GetStuffToDo() == 0 && gCPUState.Delay == NO_DELAY )
	{
		u32			entry_address( gCPUState.CurrentPC );
		CFragment * p_fragment( gFragmentCache.LookupFragmentQ( entry_address ) );

		if( p_fragment != nullptr )
		{

		// Check if another trace is active and we're about to enter
			if( gTraceRecorder.IsTraceActive() )
			{
				gTraceRecorder.StopTrace( gCPUState.CurrentPC );
				CPU_CreateAndAddFragment();

				// We need to change the core when exiting
				change_core = true;
			}

			p_fragment->Execute();

			start_of_trace = true;
		}
		else
		{
			if( start_of_trace )
			{
				start_of_trace = false;

				if( !gTraceRecorder.IsTraceActive() )
				{
					CPU_ResolvePendingICacheInvalidations();

					if (gResetFragmentCache)
					{
#ifdef DAEDALUS_ENABLE_OS_HOOKS
						//Don't reset the cache if there is no fragment except OSHLE function stubs
						if (gFragmentCache.GetCacheSize() >= gNumOfOSFunctions)
#else
						if(true)
#endif
						{
							gFragmentCache.Clear();
							gHotTraceCountMap.clear();		// Makes sense to clear this now, to get accurate usage stats
							gPendingICacheRanges.clear();
#ifdef DAEDALUS_ENABLE_OS_HOOKS
							Patch_PatchAll();
#endif
						}
						gResetFragmentCache = false;
					}

					if( gFragmentCache.GetCacheSize() > gMaxFragmentCacheSize)
					{
						gFragmentCache.Clear();
						gHotTraceCountMap.clear();		// Makes sense to clear this now, to get accurate usage stats
						gPendingICacheRanges.clear();
#ifdef DAEDALUS_ENABLE_OS_HOOKS
						Patch_PatchAll();
#endif
					}

					// If there is no fragment for this target, start tracing
					u32 trace_count( ++gHotTraceCountMap[ gCPUState.CurrentPC ] );
					if( gHotTraceCountMap.size() >= gMaxHotTraceMapSize )
					{
						gHotTraceCountMap.clear();
					}
					else if( trace_count == gHotTraceThreshold )
					{
						//DBGConsole_Msg( 0, "Identified hot trace at [R%08x]! (size is %d)", gCPUState.CurrentPC, gHotTraceCountMap.size() );
						gTraceRecorder.StartTrace( gCPUState.CurrentPC );

						if(!trace_already_enabled)
						{
							change_core = true;
						}
					}
				}
			}
			break;
		}
	}

	if(change_core)
	{
		CPU_SelectCore();
	}
}

void Dynamo_Reset()
{
	gHotTraceCountMap.clear();
	gPendingICacheRanges.clear();
	gFragmentCache.Clear();
	gResetFragmentCache = false;
	gTraceRecorder.AbortTrace();
}

void Dynamo_SelectCore()
{
	bool trace_enabled = gTraceRecorder.IsTraceActive();

	if (trace_enabled)
	{
		g_pCPUCore = CPU_Go< true, true >;
	}
	else
	{
		g_pCPUCore = CPU_Go< true, false >;
	}
}

#else

void CPU_ResetFragmentCache() {}
void Dynamo_Reset() {}
void R4300_CALL_TYPE CPU_InvalidateICacheRange( u32 address, u32 length ) {}

#endif //DAEDALUS_ENABLE_DYNAREC
