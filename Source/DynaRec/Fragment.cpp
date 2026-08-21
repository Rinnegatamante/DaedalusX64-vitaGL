/*
Copyright (C) 2006 StrmnNrmn

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
#include "Fragment.h"

#include <stdio.h>
#include <cstring>

#include <algorithm>

#include "FragmentCache.h"
#include "BranchType.h"
#include "StaticAnalysis.h"
#include "IndirectExitMap.h"

#include "Core/Registers.h"
#include "Core/CPU.h"			// Try to remove this cyclic dependency
#include "Core/R4300.h"
#include "Core/Interrupt.h"
#include "Core/Memory.h"

#include "DynaRec/CodeBufferManager.h"
#include "DynaRec/CodeGenerator.h"

#include "Utility/Macros.h"
#include "Utility/Synchroniser.h"

#include "OSHLE/ultra_R4300.h"
#include "OSHLE/patch.h"

#include "Config/ConfigOptions.h"


//#define IMMEDIATE_COUNTER_UPDATE
//#define UPDATE_COUNTER_ON_EXCEPTION

//*************************************************************************************
//
//*************************************************************************************
namespace
{

	typedef std::vector<STraceEntry> TraceBuffer;

	const u32	INVALID_IDX( u32(~0) );

	//
	//	We stuff some extra instructions at the start of each fragment, for instance
	//	to record the hitcount. This allows us to offset that from the stats.
	//
#ifdef FRAGMENT_RETAIN_ADDITIONAL_INFO
	const u32	ADDITIONAL_OUTPUT_BYTES = 5 * 4;
#else
	const u32	ADDITIONAL_OUTPUT_BYTES = 0;
#endif

}

//*************************************************************************************
//
//*************************************************************************************
CFragment::CFragment( CCodeBufferManager * p_manager,
					  u32 entry_address,
					  u32 exit_address,
					  const TraceBuffer & trace,
					  SRegisterUsageInfo &	register_usage,
					  const BranchBuffer & branch_details,
					  bool need_indirect_exit_map )
:	mEntryAddress( entry_address )
,	mGuestCodeFullyVerifiable( true )
,	mEntryPoint( nullptr )
,	mInputLength( trace.size() * sizeof( OpCode ) )
,	mOutputLength( 0 )
,	mFragmentFunctionLength( 0 )
,	mpIndirectExitMap( need_indirect_exit_map ? new CIndirectExitMap : nullptr )
#ifdef FRAGMENT_RETAIN_ADDITIONAL_INFO
,	mHitCount( 0 )
,	mTraceBuffer( trace )
,	mBranchBuffer( branch_details )
,	mExitAddress( exit_address )
#endif
#ifdef FRAGMENT_SIMULATE_EXECUTION
,	mpCache( nullptr )
#endif
{
	mCodePages.reserve( trace.size() );
	mGuestCodeWords.reserve( trace.size() );
	for( const STraceEntry & entry : trace )
	{
		mCodePages.push_back( entry.Address & ~0xfffu );

		const u32 physical_address = entry.Address & 0x1fffffffu;
		const bool direct_rdram = (entry.Address & 0xc0000000u) == 0x80000000u &&
			physical_address + sizeof(u32) <= gRamSize;
		if( !direct_rdram )
		{
			mGuestCodeFullyVerifiable = false;
		}

		SGuestCodeWord word;
		word.PhysicalAddress = physical_address;
		word.ExpectedOpCode = entry.OpCode._u32;
		mGuestCodeWords.push_back( word );
	}
	std::sort( mCodePages.begin(), mCodePages.end() );
	mCodePages.erase( std::unique( mCodePages.begin(), mCodePages.end() ), mCodePages.end() );

	std::sort( mGuestCodeWords.begin(), mGuestCodeWords.end(), []( const SGuestCodeWord & lhs, const SGuestCodeWord & rhs )
	{
		return lhs.PhysicalAddress < rhs.PhysicalAddress;
	} );

	for( size_t i = 1; i < mGuestCodeWords.size(); ++i )
	{
		if( mGuestCodeWords[i - 1].PhysicalAddress == mGuestCodeWords[i].PhysicalAddress &&
			mGuestCodeWords[i - 1].ExpectedOpCode != mGuestCodeWords[i].ExpectedOpCode )
		{
			mGuestCodeFullyVerifiable = false;
		}
	}
	mGuestCodeWords.erase( std::unique( mGuestCodeWords.begin(), mGuestCodeWords.end(),
		[]( const SGuestCodeWord & lhs, const SGuestCodeWord & rhs )
		{
			return lhs.PhysicalAddress == rhs.PhysicalAddress;
		} ), mGuestCodeWords.end() );

#ifdef FRAGMENT_RETAIN_ADDITIONAL_INFO
	mRegisterUsage = register_usage;
#endif

	Assemble( p_manager, exit_address, trace, branch_details, register_usage );
}

#ifdef DAEDALUS_ENABLE_OS_HOOKS
//*************************************************************************************
// Create a Fragement for Patch Function
//*************************************************************************************
CFragment::CFragment(CCodeBufferManager * p_manager, u32 entry_address,
						u32 function_length, void* function_Ptr)
	:	mEntryAddress( entry_address )
	,	mGuestCodeFullyVerifiable( false )
	,	mInputLength(function_length  * sizeof( OpCode ) )
	,	mOutputLength( 0 )
	,	mFragmentFunctionLength( 0 )
	,	mpIndirectExitMap( new CIndirectExitMap )
#ifdef FRAGMENT_RETAIN_ADDITIONAL_INFO
	,	mHitCount( 0 )
	,	mTraceBuffer( nullptr )
	,	mBranchBuffer( nullptr )
	,	mExitAddress( 0 )
#endif
#ifdef FRAGMENT_SIMULATE_EXECUTION
	,	mpCache( nullptr )
#endif
{
	if( mInputLength != 0 )
	{
		const u32 first_page = mEntryAddress & ~0xfffu;
		const u32 last_page = (mEntryAddress + mInputLength - 1) & ~0xfffu;
		for( u32 page = first_page; page <= last_page; page += 0x1000 )
		{
			mCodePages.push_back( page );
		}
	}

	Assemble(p_manager, CCodeLabel(function_Ptr));
}
#endif
//*************************************************************************************
//
//*************************************************************************************
EGuestCodeValidation CFragment::ValidateGuestCodeRange( u32 address, u32 length, u32 * checked_words,
	u32 * changed_address, u32 * expected_opcode, u32 * current_opcode ) const
{
	if( checked_words != nullptr )
		*checked_words = 0;

	if( length == 0 )
		return GCV_NO_OVERLAP;

	const u32 physical_start = address & 0x1fffffffu;
	const u64 physical_end64 = (u64)physical_start + (u64)length;
	if( physical_end64 > 0x20000000ULL )
		return GCV_UNVERIFIABLE;

	const u32 physical_end = (u32)physical_end64;

	if( !mGuestCodeFullyVerifiable )
	{
		for( u32 page : mCodePages )
		{
			const u32 page_start = page & 0x1fffffffu;
			const u32 page_end = page_start + 0x1000u;
			if( page_start < physical_end && page_end > physical_start )
				return GCV_UNVERIFIABLE;
		}
		return GCV_NO_OVERLAP;
	}

	SGuestCodeWord key;
	key.PhysicalAddress = physical_start > 3 ? physical_start - 3 : 0;
	key.ExpectedOpCode = 0;
	auto it = std::lower_bound( mGuestCodeWords.begin(), mGuestCodeWords.end(), key,
		[]( const SGuestCodeWord & lhs, const SGuestCodeWord & rhs )
		{
			return lhs.PhysicalAddress < rhs.PhysicalAddress;
		} );

	u32 checked = 0;
	for( ; it != mGuestCodeWords.end(); ++it )
	{
		const u32 word_start = it->PhysicalAddress;
		if( word_start >= physical_end )
			break;

		const u32 word_end = word_start + sizeof(u32);
		if( word_start < physical_end && word_end > physical_start )
		{
			++checked;
			if( word_end > gRamSize )
				return GCV_UNVERIFIABLE;

			const u32 current = *(const u32 *)(g_pu8RamBase + word_start);
			if( current != it->ExpectedOpCode )
			{
				if( checked_words != nullptr )
					*checked_words = checked;
				if( changed_address != nullptr )
					*changed_address = 0x80000000u | word_start;
				if( expected_opcode != nullptr )
					*expected_opcode = it->ExpectedOpCode;
				if( current_opcode != nullptr )
					*current_opcode = current;
				return GCV_CHANGED;
			}
		}
	}

	if( checked_words != nullptr )
		*checked_words = checked;
	return checked != 0 ? GCV_UNCHANGED : GCV_NO_OVERLAP;
}

//*************************************************************************************
//
//*************************************************************************************
CFragment::~CFragment()
{
	delete mpIndirectExitMap;
}

//*************************************************************************************
//
//*************************************************************************************
void	CFragment::SetCache( const CFragmentCache * p_cache )
{
#ifdef FRAGMENT_SIMULATE_EXECUTION
	mpCache = p_cache;
#endif

	if( mpIndirectExitMap != nullptr )
	{
		mpIndirectExitMap->SetCache( p_cache );
	}
}

//*************************************************************************************
//
//*************************************************************************************
// This is the second place where most of the CPU time is spent
//  %   cumulative   self              self     total
// time   seconds   seconds    calls   s/call   s/call  name
// 5.33     25.96     3.49   557781     0.00     0.00  CFragment::Execute()
void CFragment::Execute()
{
#ifdef FRAGMENT_SIMULATE_EXECUTION

	CFragment * p_fragment( this );

	while( p_fragment != nullptr )
	{
		CFragment * next = p_fragment->Simulate();

		p_fragment = next;
	}

#else
	const void *		p( g_pu8RamBase_8000 );
	u32					upper( 0x80000000 + gRamSize );
	_EnterDynaRec( mEntryPoint.GetTarget(), &gCPUState, p, upper );
#endif // FRAGMENT_SIMULATE_EXECUTION

	//if(gCPUState.Delay != NO_DELAY)
	//{
	//	SYNCH_POINT( DAED_SYNC_FRAGMENT_PC, gCPUState.TargetPC, "New Program Counter doesn't match on exit from fragment" );
	//}

	// We have to do this when we exit to make sure the cached read pointer is updated correctly
	CPU_SetPC( gCPUState.CurrentPC );
}
//*************************************************************************************
//
//*************************************************************************************
namespace
{
	void HandleException()
	{
		switch (gCPUState.Delay)
		{
			case DO_DELAY:
				gCPUState.CurrentPC += 4;
				gCPUState.Delay = EXEC_DELAY;
				break;
			case EXEC_DELAY:
				gCPUState.CurrentPC = gCPUState.TargetPC;
				gCPUState.Delay = NO_DELAY;
				break;
			case NO_DELAY:
				gCPUState.CurrentPC += 4;
				break;
			default:
				NODEFAULT;
		}
	}
#ifdef FRAGMENT_SIMULATE_EXECUTION
	void UpdateCountAndHandleException( u32 instructions_executed )
	{
#ifdef UPDATE_COUNTER_ON_EXCEPTION
		// If we're updating the counter on every instruction, there's no need to do this...
	#ifndef IMMEDIATE_COUNTER_UPDATE
		CPU_UpdateCounterNoInterrupt( instructions_executed );
	#endif
#endif
		HandleException();
	}
	void CheckCop1Usable()
	{
		if( (gCPUState.CPUControl[C0_SR]._u32_0 & SR_CU1) == 0 )
		{
			#ifdef DAEDALUS_DEBUG_CONSOLE
			DAEDALUS_ERROR( "Benign: Cop1 unusable fired - check logic" );
			#endif
			R4300_Exception_CopUnusuable();
		}

	}
#endif // FRAGMENT_SIMULATE_EXECUTION
}
#ifdef FRAGMENT_SIMULATE_EXECUTION
//*************************************************************************************
//
//*************************************************************************************
CFragment * CFragment::Simulate()
{
#ifdef FRAGMENT_RETAIN_ADDITIONAL_INFO
	mHitCount++;
#endif

	//
	//	Keep executing ops until we take a branch
	//
	u32			instructions_executed( 0 );

	u32			branch_idx_taken( INVALID_IDX );		// Index into mBranchBuffer of the taken branch
	u32			branch_taken_address( 0 );
	bool		checked_cop1_usable( false );

	u32			count_entry( gCPUState.CPUControl[C0_COUNT]._u32_0 );

	OpCode		last_executed_op;

	for( auto i {}; i < mTraceBuffer.size(); ++i)
	{
		const STraceEntry & ti( mTraceBuffer[ i ] );
		OpCode				op_code( ti.OpCode );
		u32					branch_idx( ti.BranchIdx );

		bool				branch_taken;

		if( ti.BranchDelaySlot )
		{
			gCPUState.Delay = EXEC_DELAY;
		}

		DAEDALUS_ASSERT( gCPUState.Delay == (ti.BranchDelaySlot ? EXEC_DELAY : NO_DELAY), "Delay doesn't match expectations" );

		// Check the cop1 usable flag. Do this only once (theoretically it could be toggled mid-fragment but this is unlikely)
		if( op_code.op == OP_COPRO1 && !checked_cop1_usable )
		{
			checked_cop1_usable = true;
			CheckCop1Usable();
			if(gCPUState.GetStuffToDo() != 0)
			{
				UpdateCountAndHandleException_Counter( instructions_executed );
				return nullptr;
			}
		}

		last_executed_op = op_code;

		CPU_ExecuteOpRaw( count_entry+instructions_executed, ti.Address, op_code, R4300_GetInstructionHandler( op_code ), &branch_taken );

#ifdef IMMEDIATE_COUNTER_UPDATE
		CPU_UpdateCounter( 1 );
#endif
		instructions_executed++;

		if(gCPUState.GetStuffToDo() != 0)
		{
			UpdateCountAndHandleException_Counter( instructions_executed );
			return nullptr;
		}

		// Break out of the loop if this is a branch instruction and it was taken
		if( branch_idx != INVALID_IDX )
		{
			const SBranchDetails &	details( mBranchBuffer[ branch_idx ] );

			// Check whether we want to invert the status of this branch
			bool	exit_trace;

			if( details.Eret )
			{
				exit_trace = true;
			}
			else if( !details.Direct )
			{
				exit_trace = (gCPUState.TargetPC != details.TargetAddress);
			}
			else
			{
				exit_trace = details.ConditionalBranchTaken ? !branch_taken : branch_taken;
			}

			if( exit_trace )
			{
				branch_taken_address = ti.Address;
				branch_idx_taken = branch_idx;
				break;
			}
		}
		else
		{
			if(ti.BranchDelaySlot)
			{
				gCPUState.Delay = NO_DELAY;
			}
		}
	}
	//
	//	Now we're leaving the fragment, handle the exit stubs
	//
	CFragment * p_target_fragment( nullptr );
	u32			exit_address {};
	u32			exit_delay {};
	if( branch_idx_taken != INVALID_IDX )
	{
		//
		//	A branch was taken - this means we have to execute it's delay op
		//

		SBranchDetails &	details( mBranchBuffer[ branch_idx_taken ] );
		bool				executed_delay_op( true );

		if( details.Likely )
		{
			if( details.ConditionalBranchTaken )
			{
				// The branch was taken in our trace, so we flipped the logic around
				// This means the likely branch WASN'T taken just now.
				// Bail out with an address of PC+8
				// TODO: Could cache this target fragment?
				exit_delay = NO_DELAY;
				exit_address = branch_taken_address + 8;

				p_target_fragment = mpCache->LookupFragmentQ( exit_address );
			}
			else
			{
				// The branch wasn't taken in our trace, so the original logic is used
				// We never saw the branch delay slot, so bail out to interpreter
				exit_delay = EXEC_DELAY;
				gCPUState.TargetPC = details.TargetAddress;
				exit_address = branch_taken_address + 4;		// i.e. execute the branch

				// XXXX This is potentially unsafe - we exit with the flag set. The target
				// fragment may not behave properly if an exception is thrown on the first instruction
				//p_target_fragment = mpCache->LookupFragmentQ( exit_address );
			}
			executed_delay_op = false;
			//return nullptr;
		}
		else if( details.Direct )
		{
			exit_address = details.TargetAddress;
			exit_delay = NO_DELAY;
			p_target_fragment = mpCache->LookupFragmentQ( details.TargetAddress );
		}
		else
		{
			if( details.Eret )
			{
				exit_address = gCPUState.CurrentPC + 4;
				p_target_fragment = mpIndirectExitMap->LookupIndirectExit( exit_address );
			}
			else
			{
				exit_address = gCPUState.TargetPC;

				p_target_fragment = mpIndirectExitMap->LookupIndirectExit( gCPUState.TargetPC );
			}
			exit_delay = NO_DELAY;
		}

		//
		//	Not all branches have delay instructions
		//

		if( executed_delay_op && details.DelaySlotTraceIndex != -1 )
		{
			OpCode		delay_op_code( mTraceBuffer[details.DelaySlotTraceIndex].OpCode );
			u32 		delay_address( mTraceBuffer[details.DelaySlotTraceIndex].Address );

			bool		dummy_branch_taken;
			gCPUState.Delay = EXEC_DELAY;

			if( delay_op_code.op == OP_COPRO1 && !checked_cop1_usable )
			{
				checked_cop1_usable = true;
				CheckCop1Usable();
				if(gCPUState.GetStuffToDo() != 0)
				{
					UpdateCountAndHandleException_Counter( instructions_executed );
					return nullptr;
				}
			}

			CPU_ExecuteOpRaw( count_entry+instructions_executed, delay_address, delay_op_code, R4300Instruction[ delay_op_code.op ], &dummy_branch_taken );

#ifdef IMMEDIATE_COUNTER_UPDATE
			CPU_UpdateCounter( 1 );
#endif
			instructions_executed++;

			if(gCPUState.GetStuffToDo() != 0)
			{
				UpdateCountAndHandleException_Counter( instructions_executed );
				return nullptr;
			}

			gCPUState.Delay = NO_DELAY;
		}
	}
	else
	{
		p_target_fragment = mpCache->LookupFragmentQ( mExitAddress );
		exit_address = mExitAddress;
		exit_delay = NO_DELAY;
	}

#ifndef IMMEDIATE_COUNTER_UPDATE
	CPU_UpdateCounter( instructions_executed );
#endif

	//
	//	Finally set up all the registers required for transferring control to the next branch
	//
	gCPUState.CurrentPC = exit_address;
	gCPUState.Delay = exit_delay;

	if( exit_address == mEntryAddress )
	{
		p_target_fragment = this;
	}

	if( gCPUState.GetStuffToDo() != 0 )
	{
		// Quit to the interpreter if there are CPU jobs to do
		p_target_fragment = nullptr;
	}

	return p_target_fragment;
}
#endif // FRAGMENT_SIMULATE_EXECUTION

//*************************************************************************************
//
//*************************************************************************************
u32	CFragment::GetMemoryUsage() const
{
	// Ignore the 'additional info' when computing this

	return sizeof( CFragment ) +
		   mPatchList.size() * sizeof( SFragmentPatchDetails );
}

//*************************************************************************************
//
//*************************************************************************************
namespace
{
	struct SBranchHandlerInfo
	{
		SBranchHandlerInfo()
			:	Index( u32( ~0 ) )
			,	Jump()
			,	RegisterSnapshot( u32( ~0 ) )
		{
		}

		u32						Index;
		CJumpLocation			Jump;
		RegisterSnapshotHandle	RegisterSnapshot;
	};
}

//*************************************************************************************
//
//*************************************************************************************
void	CFragment::AddPatch( u32 address, CJumpLocation jump_location )
{
	if( jump_location.IsSet() )
	{
		SFragmentPatchDetails	patch_details;

		patch_details.Address = address;
		patch_details.Jump = jump_location;

		mPatchList.push_back( patch_details );
	}
}


//*************************************************************************************
//
//*************************************************************************************
void CFragment::Assemble( CCodeBufferManager * p_manager,
						  u32 exit_address,
						  const std::vector< STraceEntry > & trace,
						  const std::vector< SBranchDetails > & branch_details,
						  const SRegisterUsageInfo & register_usage )
{
	const u32				NO_JUMP_ADDRESS( 0 );

	CCodeGenerator *		p_generator( p_manager->StartNewBlock() );

	mEntryPoint = p_generator->GetEntryPoint();

#ifdef FRAGMENT_RETAIN_ADDITIONAL_INFO
	p_generator->Initialise( mEntryAddress, exit_address, &mHitCount, &gCPUState, register_usage );
#else
	p_generator->Initialise( mEntryAddress, exit_address, nullptr, &gCPUState, register_usage );
#endif

	//Trace: (3 ops, 13 hits)
	//80317934:  SLT       at = (t7<a0)
	//BRANCH 0 -> 80317940
	//80317938:  BNEL      at != r0 --> 0x80317934
	//8031793c:  LW        t7 <- 0x0000(v0)

	if(trace.size() == 3)
	{
		if( trace[0].OpCode._u32 == 0x01E4082A &&
			trace[1].OpCode._u32 == 0x5420FFFE &&
			trace[2].OpCode._u32 == 0x8C4F0000)
		{
			p_generator->ExecuteNativeFunction( CCodeLabel( reinterpret_cast< const void * >( CPU_SkipToNextEvent ) ) );
		}
	}

	//
	//	Keep executing ops until we take a branch
	//
	std::vector< CJumpLocation >		exception_handler_jumps;
	std::vector< RegisterSnapshotHandle >   exception_handler_snapshots;
	std::vector< SBranchHandlerInfo >	branch_handler_info( branch_details.size() );
//	bool								checked_cop1_usable( false );

	for( u32 i = 0; i < trace.size(); ++i )
	{
		const STraceEntry & ti( trace[ i ] );
		u32	branch_idx( ti.BranchIdx );

#ifdef FRAGMENT_RETAIN_ADDITIONAL_INFO
		mInstructionStartLocations.push_back( p_generator->GetCurrentLocation().GetTargetU8P() );
#endif

		p_generator->UpdateRegisterCaching( i );

		// Check the cop1 usable flag. Do this only once (theoretically it could be toggled mid-fragment but this is unlikely)
		/*
		if( op_code.op == OP_COPRO1 && !checked_cop1_usable )
		{
			checked_cop1_usable = true;

			// TODO
			//CJumpLocation	handler( p_generator->GenerateCheckCop1Usable( CheckCop1Usable(), ti.BranchDelaySlot ) );
			//exception_handler_jumps.push_back( handler );
		}
		*/

		const SBranchDetails * p_branch( nullptr );
		if( branch_idx != INVALID_IDX )
		{
			p_branch = &branch_details[ branch_idx ];

			if(p_branch->SpeedHack == SHACK_SKIPTOEVENT)
			{
				p_generator->ExecuteNativeFunction( CCodeLabel( reinterpret_cast< const void * >( CPU_SkipToNextEvent ) ) );
			}
		}

		CJumpLocation	branch_jump( nullptr );
		CJumpLocation	exception_handler_jump;
		if (gUseCachedInterpreter) exception_handler_jump = p_generator->GenerateInterpOpCode( ti, ti.BranchDelaySlot, p_branch, &branch_jump);
		else exception_handler_jump = p_generator->GenerateOpCode( ti, ti.BranchDelaySlot, p_branch, &branch_jump);

		if( exception_handler_jump.IsSet() )
		{
			exception_handler_jumps.push_back( exception_handler_jump );
			exception_handler_snapshots.push_back(p_generator->GetRegisterSnapshot());
		}

		// Check whether we want to invert the status of this branch
		if( p_branch != nullptr )
		{
			branch_handler_info[ branch_idx ].Index = i;
			branch_handler_info[ branch_idx ].Jump = branch_jump;
			branch_handler_info[ branch_idx ].RegisterSnapshot = p_generator->GetRegisterSnapshot();
		}
	}
#ifdef FRAGMENT_RETAIN_ADDITIONAL_INFO
		mInstructionStartLocations.push_back( p_generator->GetCurrentLocation().GetTargetU8P() );
#endif

	CCodeLabel		no_next_fragment( nullptr );
	CJumpLocation	exit_jump( p_generator->GenerateExitCode( exit_address, NO_JUMP_ADDRESS, trace.size(), no_next_fragment ) );

	AddPatch( exit_address, exit_jump );

	//
	//	Generate handlers for each exit branch
	//
	for( u32 i {}; i < branch_details.size(); ++i )
	{
		const SBranchDetails &	details( branch_details[ i ] );
		u32						instruction_idx( branch_handler_info[ i ].Index );

		// If we didn't generate a branch instruction, then it didn't need handling
		if(!branch_handler_info[ i ].Jump.IsSet())
		{
			continue;
		}

		u32					branch_instruction_address( trace[ instruction_idx ].Address );
		u32					num_instructions_executed( instruction_idx + 1 );

		p_generator->GenerateBranchHandler( branch_handler_info[ i ].Jump, branch_handler_info[ i ].RegisterSnapshot );

		//
		//	Not all branches have delay instructions
		//
		if( !details.Likely && details.DelaySlotTraceIndex != -1 )
		{
			const STraceEntry & ti( trace[ details.DelaySlotTraceIndex ] );
#ifdef FRAGMENT_SIMULATE_EXECUTION
			u32			delay_address( ti.Address );
#endif
			/*
#ifdef DAEDALUS_DEBUG_CONSOLE
			OpCode		delay_op_code( ti.OpCode );
#endif
			if( delay_op_code.op == OP_COPRO1 && !checked_cop1_usable )
			{
				checked_cop1_usable = true;

				//CJumpLocation	handler( p_generator->GenerateCheckCop1Usable( CheckCop1Usable(), true ) );
				//exception_handler_jumps.push_back( handler );
			}
			*/
			CJumpLocation	exception_handler_jump;
			if (gUseCachedInterpreter) exception_handler_jump = p_generator->GenerateInterpOpCode( ti, true, nullptr, nullptr);
			else exception_handler_jump = p_generator->GenerateOpCode( ti, true, nullptr, nullptr);

			if( exception_handler_jump.IsSet() )
			{
				exception_handler_jumps.push_back(exception_handler_jump);
				exception_handler_snapshots.push_back(p_generator->GetRegisterSnapshot());
			}
			num_instructions_executed++;
		}


		if( details.Likely )
		{
			u32				exit_address {};
			CJumpLocation	jump_location {};

			if( details.ConditionalBranchTaken )
			{
				exit_address = branch_instruction_address + 8;
				jump_location = p_generator->GenerateExitCode( exit_address, NO_JUMP_ADDRESS, num_instructions_executed, no_next_fragment );
			}
			else
			{
				exit_address = branch_instruction_address + 4;
				jump_location = p_generator->GenerateExitCode( exit_address, details.TargetAddress, num_instructions_executed, no_next_fragment );
			}

			AddPatch( exit_address, jump_location );
		}
		else if( details.Direct )
		{
			u32				exit_address( details.TargetAddress );
			CJumpLocation	jump_location( p_generator->GenerateExitCode( details.TargetAddress, NO_JUMP_ADDRESS, num_instructions_executed, no_next_fragment ) );

			AddPatch( exit_address, jump_location );
		}
		else
		{
			if( details.Eret )
			{
				p_generator->GenerateEretExitCode( num_instructions_executed, mpIndirectExitMap );

			}
			else
			{
				p_generator->GenerateIndirectExitCode( num_instructions_executed, mpIndirectExitMap );
			}
		}
	}

	p_generator->Finalise( HandleException, exception_handler_jumps, exception_handler_snapshots );

	mFragmentFunctionLength = p_manager->FinaliseCurrentBlock();
	mOutputLength = mFragmentFunctionLength - ADDITIONAL_OUTPUT_BYTES;

	delete p_generator;
}

#ifdef DAEDALUS_ENABLE_OS_HOOKS
//*************************************************************************************
//
//*************************************************************************************
void CFragment::Assemble( CCodeBufferManager * p_manager, CCodeLabel function_ptr)
{
	std::vector< CJumpLocation >		exception_handler_jumps;
	std::vector< RegisterSnapshotHandle> exception_handler_snapshots;
	SRegisterUsageInfo register_usage;

	CCodeGenerator *p_generator = p_manager->StartNewBlock();
	mEntryPoint = p_generator->GetEntryPoint();


#ifdef FRAGMENT_RETAIN_ADDITIONAL_INFO
		p_generator->Initialise( mEntryAddress, 0, &mHitCount, &gCPUState,  register_usage);
#else
		p_generator->Initialise( mEntryAddress, 0, nullptr, &gCPUState, register_usage );
#endif

	CJumpLocation jump = p_generator->ExecuteNativeFunction(function_ptr, true);
	p_generator->GenerateIndirectExitCode(100, mpIndirectExitMap);
	AssemblyUtils::PatchJumpLong(jump, p_generator->GetCurrentLocation());
	p_generator->GenerateEretExitCode(100, mpIndirectExitMap);

	p_generator->Finalise( HandleException, exception_handler_jumps, exception_handler_snapshots );
	mFragmentFunctionLength = p_manager->FinaliseCurrentBlock();
	mOutputLength = mFragmentFunctionLength - ADDITIONAL_OUTPUT_BYTES;

	delete p_generator;
}
#endif
