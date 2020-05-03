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

#pragma once

#include "DynaRec/CodeGenerator.h"
#include "AssemblyWriterARM.h"
#include "DynarecTargetARM.h"
#include "DynaRec/TraceRecorder.h"

// XXXX For GenerateCompare_S/D
#define FLAG_SWAP			0x100
#define FLAG_C_LT		(0x41)					// jne-   le
#define FLAG_C_LE		(FLAG_SWAP|0x41)		// je -   ! gt
#define FLAG_C_EQ		(FLAG_SWAP|0x40)		// je -   ! eq

typedef u32 (*ReadMemoryFunction)( u32 address );

class CCodeGeneratorARM : public CCodeGenerator, public CAssemblyWriterARM
{
	public:
		CCodeGeneratorARM( CAssemblyBuffer * p_primary, CAssemblyBuffer * p_secondary );

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

		virtual CJumpLocation		ExecuteNativeFunction( CCodeLabel speed_hack, bool check_return );

	private:
				void				SetVar( const u32 * p_var, u32 value );

				EArmReg				GetRegisterNoLoad( EN64Reg n64_reg, u32 lo_hi_idx, EArmReg scratch_reg );
				EArmReg				GetRegisterNoLoadLo( EN64Reg n64_reg, EArmReg scratch_reg )		{ return GetRegisterNoLoad( n64_reg, 0, scratch_reg ); }
				EArmReg				GetRegisterNoLoadHi( EN64Reg n64_reg, EArmReg scratch_reg )		{ return GetRegisterNoLoad( n64_reg, 1, scratch_reg ); }

				EArmReg				GetRegisterAndLoad( EN64Reg n64_reg, u32 lo_hi_idx, EArmReg scratch_reg );
				EArmReg				GetRegisterAndLoadLo( EN64Reg n64_reg, EArmReg scratch_reg )	{ return GetRegisterAndLoad( n64_reg, 0, scratch_reg ); }
				EArmReg				GetRegisterAndLoadHi( EN64Reg n64_reg, EArmReg scratch_reg )	{ return GetRegisterAndLoad( n64_reg, 1, scratch_reg ); }

				CJumpLocation		GenerateBranchAlways( CCodeLabel target );
				CJumpLocation		GenerateBranchIfSet( const u32 * p_var, CCodeLabel target );
				CJumpLocation		GenerateBranchIfNotSet( const u32 * p_var, CCodeLabel target );
				CJumpLocation		GenerateBranchIfEqual( const u32 * p_var, u32 value, CCodeLabel target );
				CJumpLocation		GenerateBranchIfNotEqual( const u32 * p_var, u32 value, CCodeLabel target );
				CJumpLocation		GenerateBranchIfNotEqual( EArmReg reg_a, u32 value, CCodeLabel target );

				void				GenerateGenericR4300( OpCode op_code, CPU_Instruction p_instruction );

				void				GenerateExceptionHander( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps );

				bool				mSpCachedInESI;		// Is sp cached in ESI?
				u32					mSetSpPostUpdate;	// Set Sp base counter after this update

				CAssemblyBuffer *	mpPrimary;
				CAssemblyBuffer *	mpSecondary;

	private:
				bool	GenerateCACHE( EN64Reg base, s16 offset, u32 cache_op );

				void	GenerateStore( EN64Reg base, s16 offset, u8 twiddle, u8 bits, void* p_write_memory );
				bool	GenerateSW(EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateSWC1( u32 ft, EN64Reg base, s16 offset );
				bool	GenerateSH( EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateSB( EN64Reg rt, EN64Reg base, s16 offset );

				void	GenerateLoad( EN64Reg base, s16 offset, u8 twiddle, u8 bits, bool is_signed, void* p_read_memory );
				bool	GenerateLW(EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLB(EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLBU(EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLH(EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLHU(EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLWC1(u32 ft, EN64Reg base, s16 offset );
				void	GenerateLUI( EN64Reg rt, s16 immediate );

				void	GenerateADDIU( EN64Reg rt, EN64Reg rs, s16 immediate );
				void	GenerateANDI( EN64Reg rt, EN64Reg rs, u16 immediate );
				void	GenerateORI( EN64Reg rt, EN64Reg rs, u16 immediate );
				void	GenerateXORI( EN64Reg rt, EN64Reg rs, u16 immediate );
				void	GenerateSLTI( EN64Reg rt, EN64Reg rs, s16 immediate, bool is_unsigned );

				void	GenerateJAL( u32 address );

				//Special Op
				void	GenerateMFC1( EN64Reg rt, u32 fs );
				void	GenerateMTC1( u32 fs, EN64Reg rt );
				void	GenerateCFC1( EN64Reg rt, u32 fs );
				void	GenerateCTC1( u32 fs, EN64Reg rt );

				void	GenerateSLL( EN64Reg rd, EN64Reg rt, u32 sa );
				void	GenerateSRL( EN64Reg rd, EN64Reg rt, u32 sa );
				void	GenerateSRA( EN64Reg rd, EN64Reg rt, u32 sa );

				void	GenerateOR( EN64Reg rd, EN64Reg rs, EN64Reg rt );
				void	GenerateAND( EN64Reg rd, EN64Reg rs, EN64Reg rt );
				void	GenerateXOR( EN64Reg rd, EN64Reg rs, EN64Reg rt );

				void	GenerateJR( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump );

				void	GenerateADDU( EN64Reg rd, EN64Reg rs, EN64Reg rt );
				void	GenerateSUBU( EN64Reg rd, EN64Reg rs, EN64Reg rt );

				void	GenerateMULT( EN64Reg rs, EN64Reg rt, bool is_unsigned );

				void	GenerateDIV( EN64Reg rs, EN64Reg rt );
				void	GenerateDIVU( EN64Reg rs, EN64Reg rt );

				void	GenerateMFLO( EN64Reg rd );
				void	GenerateMFHI( EN64Reg rd );
				void	GenerateMTLO( EN64Reg rs );
				void	GenerateMTHI( EN64Reg rs );

				void	GenerateSLT( EN64Reg rd, EN64Reg rs, EN64Reg rt , bool is_unsigned );

				//Branch
				void	GenerateBEQ( EN64Reg rs, EN64Reg rt, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump );
				void	GenerateBNE( EN64Reg rs, EN64Reg rt, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump );
				void 	GenerateBLTZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump );
				void 	GenerateBGEZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump );

				// CoPro1
				void	GenerateADD_S( u32 fd, u32 fs, u32 ft );
				void	GenerateSUB_S( u32 fd, u32 fs, u32 ft );
				void	GenerateMUL_S( u32 fd, u32 fs, u32 ft );
				void	GenerateDIV_S( u32 fd, u32 fs, u32 ft );
				void	GenerateSQRT_S( u32 fd, u32 fs );
				void	GenerateCMP_S( u32 fs, u32 ft, EArmCond cond );
};