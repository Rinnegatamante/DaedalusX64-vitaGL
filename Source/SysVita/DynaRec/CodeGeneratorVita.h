/*
Copyright (C) 2001,2005 StrmnNrmn

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
//Define to use jumptable dynarec
//#define NEW_DYNAREC_COMPILER

#pragma once

#ifndef SYSVITA_DYNAREC_CODEGENERATORVITA_H_
#define SYSVITA_DYNAREC_CODEGENERATORVITA_H_

#include "DynaRec/CodeGenerator.h"
//#include "AssemblyWriterPSP.h"
//#include "N64RegisterCachePSP.h"
#include <stack>

class CAssemblyBuffer;


class CCodeGeneratorVita : public CCodeGenerator
{
	public:
		CCodeGeneratorVita( CAssemblyBuffer * p_buffer_a, CAssemblyBuffer * p_buffer_b );

		virtual void				Initialise( u32 entry_address, u32 exit_address, u32 * hit_counter, const void * p_base, const SRegisterUsageInfo & register_usage );
		virtual void				Finalise( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps );

		virtual void				UpdateRegisterCaching( u32 instruction_idx );

		virtual RegisterSnapshotHandle	GetRegisterSnapshot();

		virtual CCodeLabel			GetEntryPoint() const;
		virtual CCodeLabel			GetCurrentLocation() const;
		virtual u32					GetCompiledCodeSize() const;

		virtual	CJumpLocation		GenerateExitCode( u32 exit_address, u32 jump_address, u32 num_instructions, CCodeLabel next_fragment );
		virtual void				GenerateEretExitCode( u32 num_instructions, CIndirectExitMap * p_map );
		virtual void				GenerateIndirectExitCode( u32 num_instructions, CIndirectExitMap * p_map );

		virtual void				GenerateBranchHandler( CJumpLocation branch_handler_jump, RegisterSnapshotHandle snapshot );

		virtual CJumpLocation		GenerateOpCode( const STraceEntry& ti, bool branch_delay_slot, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump);

		virtual CJumpLocation		ExecuteNativeFunction( CCodeLabel speed_hack, bool check_return = false );
};


#endif // SYSVITA_DYNAREC_CODEGENERATORVITA_H_
