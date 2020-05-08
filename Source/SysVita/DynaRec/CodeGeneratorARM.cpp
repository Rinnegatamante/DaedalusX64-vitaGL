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
#include "CodeGeneratorARM.h"

#include <cstdio>

#include "Config/ConfigOptions.h"
#include "Core/CPU.h"
#include "Core/R4300.h"
#include "Core/Registers.h"
#include "Debug/DBGConsole.h"
#include "Debug/DebugLog.h"
#include "DynaRec/AssemblyUtils.h"
#include "DynaRec/IndirectExitMap.h"
#include "DynaRec/StaticAnalysis.h"
#include "DynaRec/Trace.h"
#include "OSHLE/ultra_R4300.h"


using namespace AssemblyUtils;

static const u32		NUM_MIPS_REGISTERS( 32 );
static const EArmReg	gMemoryBaseReg = ArmReg_R10;

// XX this optimisation works very well on the PSP, option to disable it was removed
static const bool		gDynarecStackOptimisation = true;

//Helper functions used for slow loads
s32 Read8Bits_Signed ( u32 address ) { return (s8) Read8Bits(address); };
s32 Read16Bits_Signed( u32 address ) { return (s16)Read16Bits(address); };

//*****************************************************************************
//	XXXX
//*****************************************************************************
void Dynarec_ClearedCPUStuffToDo(){}
void Dynarec_SetCPUStuffToDo(){}
//*****************************************************************************
//
//*****************************************************************************
CCodeGeneratorARM::CCodeGeneratorARM( CAssemblyBuffer * p_primary, CAssemblyBuffer * p_secondary )
:	CCodeGenerator( )
,	CAssemblyWriterARM( p_primary )
,	mSpCachedInESI( false )
,	mSetSpPostUpdate( 0 )
,	mpPrimary( p_primary )
,	mpSecondary( p_secondary )
{
}

void	CCodeGeneratorARM::Finalise( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps )
{
	if( !exception_handler_jumps.empty() )
	{
		GenerateExceptionHander( p_exception_handler_fn, exception_handler_jumps );
	}

	InsertLiteralPool();

	SetAssemblyBuffer( NULL );
	mpPrimary = NULL;
	mpSecondary = NULL;
}

#if 0
u32 gNumFragmentsExecuted = 0;
extern "C"
{

void LogFragmentEntry( u32 entry_address )
{
	gNumFragmentsExecuted++;
	if(gNumFragmentsExecuted >= 0x99990)
	{
		char buffer[ 128 ]
		sprintf( buffer, "Address %08x\n", entry_address );
		OutputDebugString( buffer );
	}
}

}
#endif

void CCodeGeneratorARM::Initialise( u32 entry_address, u32 exit_address, u32 * hit_counter, const void * p_base, const SRegisterUsageInfo & register_usage )
{
	//MOVI(ECX_CODE, entry_address);
	// CALL( CCodeLabel( LogFragmentEntry ) );

	//PUSH(1 << ArmReg_R14);

	if( hit_counter != NULL )
	{
		MOV32(ArmReg_R1, (u32)hit_counter);
		LDR(ArmReg_R0, ArmReg_R1, 0);
		ADD_IMM(ArmReg_R0, ArmReg_R0, 1);
		STR(ArmReg_R0, ArmReg_R1, 0);
	}

	// p_base/span_list ignored for now
}

// Stores a variable into memory (must be within offset range of &gCPUState)
void CCodeGeneratorARM::SetVar( const u32 * p_var, u32 value )
{
	uint16_t offset = (u32)p_var - (u32)&gCPUState;
	MOV32(ArmReg_R0, (u32)value);
	STR(ArmReg_R0, ArmReg_R12, offset);
}


void CCodeGeneratorARM::UpdateRegisterCaching( u32 instruction_idx )
{
	// This is ignored for now
}

//*****************************************************************************
//
//*****************************************************************************
RegisterSnapshotHandle	CCodeGeneratorARM::GetRegisterSnapshot()
{
	// This doesn't do anything useful yet.
	return RegisterSnapshotHandle( 0 );
}

//*****************************************************************************
//
//*****************************************************************************
CCodeLabel	CCodeGeneratorARM::GetEntryPoint() const
{
	return mpPrimary->GetStartAddress();
}

//*****************************************************************************
//
//*****************************************************************************
CCodeLabel	CCodeGeneratorARM::GetCurrentLocation() const
{
	return mpPrimary->GetLabel();
}

//*****************************************************************************
//
//*****************************************************************************
u32	CCodeGeneratorARM::GetCompiledCodeSize() const
{
	return mpPrimary->GetSize() + mpSecondary->GetSize();
}

//Get a (cached) N64 register mapped to an ARM register(usefull for dst register)
EArmReg	CCodeGeneratorARM::GetRegisterNoLoad( EN64Reg n64_reg, u32 lo_hi_idx, EArmReg scratch_reg )
{
	return scratch_reg;
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation CCodeGeneratorARM::GenerateExitCode( u32 exit_address, u32 jump_address, u32 num_instructions, CCodeLabel next_fragment )
{
	//DAEDALUS_ASSERT( exit_address != u32( ~0 ), "Invalid exit address" );
	DAEDALUS_ASSERT( !next_fragment.IsSet() || jump_address == 0, "Shouldn't be specifying a jump address if we have a next fragment?" );

#ifdef _DEBUG
	if(exit_address == u32(~0))
	{
		INT3();
	}
#endif
	MOV32(ArmReg_R0, num_instructions);

	//Call CPU_UpdateCounter
	CALL(CCodeLabel( (void*)CPU_UpdateCounter ));

	// This jump may be NULL, in which case we patch it below
	// This gets patched with a jump to the next fragment if the target is later found
	CJumpLocation jump_to_next_fragment( GenerateBranchIfNotSet( const_cast< u32 * >( &gCPUState.StuffToDo ), next_fragment ) );

	// If the flag was set, we need in initialise the pc/delay to exit with
	CCodeLabel interpret_next_fragment( GetAssemblyBuffer()->GetLabel() );

	u8		exit_delay;

	if( jump_address != 0 )
	{
		SetVar( &gCPUState.TargetPC, jump_address );
		exit_delay = EXEC_DELAY;
	}
	else
	{
		exit_delay = NO_DELAY;
	}

	SetVar( &gCPUState.Delay, exit_delay );
	SetVar( &gCPUState.CurrentPC, exit_address );

	// No need to call CPU_SetPC(), as this is handled by CFragment when we exit
	RET();

	// Patch up the exit jump
	if( !next_fragment.IsSet() )
	{
		PatchJumpLong( jump_to_next_fragment, interpret_next_fragment );
	}

	return jump_to_next_fragment;
}

//*****************************************************************************
// Handle branching back to the interpreter after an ERET
//*****************************************************************************
void CCodeGeneratorARM::GenerateEretExitCode( u32 num_instructions, CIndirectExitMap * p_map )
{
	MOV32(ArmReg_R0, num_instructions);

	//Call CPU_UpdateCounter
	CALL(CCodeLabel( (void*)CPU_UpdateCounter ));

	// We always exit to the interpreter, regardless of the state of gCPUState.StuffToDo

	// Eret is a bit bodged so we exit at PC + 4
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CurrentPC));
	ADD_IMM(ArmReg_R0, ArmReg_R0, 4);
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CurrentPC));

	SetVar( &gCPUState.Delay, NO_DELAY );

	// No need to call CPU_SetPC(), as this is handled by CFragment when we exit
	RET();
}

//*****************************************************************************
// Handle branching back to the interpreter after an indirect jump
//*****************************************************************************
void CCodeGeneratorARM::GenerateIndirectExitCode( u32 num_instructions, CIndirectExitMap * p_map )
{
	MOV32(ArmReg_R0, num_instructions);

	//Call CPU_UpdateCounter
	CALL(CCodeLabel( (void*)CPU_UpdateCounter ));

	CCodeLabel		no_target( NULL );
	CJumpLocation	jump_to_next_fragment( GenerateBranchIfNotSet( const_cast< u32 * >( &gCPUState.StuffToDo ), no_target ) );

	CCodeLabel		exit_dynarec( GetAssemblyBuffer()->GetLabel() );

	// New return address is in gCPUState.TargetPC
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, TargetPC));
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CurrentPC));

	SetVar( &gCPUState.Delay, NO_DELAY );

	// No need to call CPU_SetPC(), as this is handled by CFragment when we exit
	RET();

	// gCPUState.StuffToDo == 0, try to jump to the indirect target
	PatchJumpLong( jump_to_next_fragment, GetAssemblyBuffer()->GetLabel() );

	MOV32( ArmReg_R0, reinterpret_cast< u32 >( p_map ) );
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, TargetPC));

	CALL(CCodeLabel( (void*)IndirectExitMap_Lookup ));

	// If the target was not found, exit
	CMP_IMM( ArmReg_R0, 0 );
	BX_IMM( exit_dynarec, EQ);

	B( ArmReg_R0 );
}

//*****************************************************************************
//
//*****************************************************************************
void CCodeGeneratorARM::GenerateExceptionHander( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps )
{
	CCodeLabel exception_handler( GetAssemblyBuffer()->GetLabel() );

	MOV32(ArmReg_R0, (u32)p_exception_handler_fn );
	BLX(ArmReg_R0);

	RET();

	for( std::vector< CJumpLocation >::const_iterator it = exception_handler_jumps.begin(); it != exception_handler_jumps.end(); ++it )
	{
		CJumpLocation	jump( *it );
		PatchJumpLong( jump, exception_handler );
	}
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorARM::GenerateBranchAlways( CCodeLabel target )
{
	CJumpLocation jumpLocation = BX_IMM(target);
	InsertLiteralPool();

	return jumpLocation;
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorARM::GenerateBranchIfSet( const u32 * p_var, CCodeLabel target )
{
	MOV32(ArmReg_R1, (u32)p_var);
	LDR(ArmReg_R0, ArmReg_R1, 0);
	CMP_IMM(ArmReg_R0, 0);

	return BX_IMM(target, NE);
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorARM::GenerateBranchIfNotSet( const u32 * p_var, CCodeLabel target )
{
	MOV32(ArmReg_R1, (u32)p_var);
	LDR(ArmReg_R0, ArmReg_R1, 0);
	CMP_IMM(ArmReg_R0, 0);

	return BX_IMM(target, EQ);
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorARM::GenerateBranchIfEqual( const u32 * p_var, u32 value, CCodeLabel target )
{
	MOV32(ArmReg_R1, (u32)p_var);
	LDR(ArmReg_R0, ArmReg_R1, 0);
	MOV32(ArmReg_R1, value);

	CMP(ArmReg_R0, ArmReg_R1);

	return BX_IMM(target, EQ);
}

CJumpLocation	CCodeGeneratorARM::GenerateBranchIfNotEqual( const u32 * p_var, u32 value, CCodeLabel target )
{
	MOV32(ArmReg_R1, (u32)p_var);
	LDR(ArmReg_R0, ArmReg_R1, 0);
	MOV32(ArmReg_R1, value);

	CMP(ArmReg_R0, ArmReg_R1);

	return BX_IMM(target, NE);
}

//*****************************************************************************
//	Generates instruction handler for the specified op code.
//	Returns a jump location if an exception handler is required
//*****************************************************************************

CJumpLocation	CCodeGeneratorARM::GenerateOpCode( const STraceEntry& ti, bool branch_delay_slot, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump)
{
	u32 address = ti.Address;
	bool exception = false;
	OpCode op_code = ti.OpCode;

	CJumpLocation	exception_handler(NULL);

	if (op_code._u32 == 0)
	{
		if( branch_delay_slot )
		{
			MOV_IMM(ArmReg_R0, NO_DELAY);
			STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, Delay));
		}
		return CJumpLocation( NULL);
	}

	if( branch_delay_slot )
	{
		MOV_IMM(ArmReg_R0, EXEC_DELAY);
		STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, Delay));
	}

	const EN64Reg	rs = EN64Reg( op_code.rs );
	const EN64Reg	rt = EN64Reg( op_code.rt );
	const EN64Reg	rd = EN64Reg( op_code.rd );
	const u32		sa = op_code.sa;
	const EN64Reg	base = EN64Reg( op_code.base );
	//const u32		jump_target( (address&0xF0000000) | (op_code.target<<2) );
	//const u32		branch_target( address + ( ((s32)(s16)op_code.immediate)<<2 ) + 4);
	const u32		ft = op_code.ft;


	bool handled = false;

	switch(op_code.op)
	{
		case OP_J:			/* nothing to do */		handled = true; break;

		case OP_JAL: 	GenerateJAL( address ); handled = true; break;

		case OP_BEQ:	GenerateBEQ( rs, rt, p_branch, p_branch_jump ); handled = true; break;
		case OP_BEQL:	GenerateBEQ( rs, rt, p_branch, p_branch_jump ); handled = true; break;
		case OP_BNE:	GenerateBNE( rs, rt, p_branch, p_branch_jump );	handled = true; break;
		case OP_BNEL:	GenerateBNE( rs, rt, p_branch, p_branch_jump );	handled = true; break;
		case OP_ADDIU:	GenerateADDIU(rt, rs, s16(op_code.immediate)); handled = true; break;
		case OP_ADDI:	GenerateADDIU(rt, rs, s16(op_code.immediate)); handled = true; break;
		case OP_ANDI:	GenerateANDI( rt, rs, op_code.immediate ); handled = true; break;
		case OP_ORI:	GenerateORI( rt, rs, op_code.immediate ); handled = true; break;
		case OP_XORI:	GenerateXORI( rt, rs, op_code.immediate );handled = true; break;

		case OP_SW:		handled = GenerateSW(rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_SH:		handled = GenerateSH(rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_SB:		handled = GenerateSB(rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_SD:		handled = GenerateSD(rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_SWC1:	handled = GenerateSWC1(ft, base, s16(op_code.immediate)); exception = !handled; break;
		case OP_SDC1:	handled = GenerateSDC1(ft, base, s16(op_code.immediate)); exception = !handled; break;

		case OP_SLTIU: 	GenerateSLTI( rt, rs, s16( op_code.immediate ), true );  handled = true; break;
		case OP_SLTI:	GenerateSLTI( rt, rs, s16( op_code.immediate ), false ); handled = true; break;

		case OP_LW:		handled = GenerateLW(rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_LH:		handled = GenerateLH(rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_LHU: 	handled = GenerateLHU(rt, base, s16(op_code.immediate));  exception = !handled; break;
		case OP_LB: 	handled = GenerateLB(rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_LBU:	handled = GenerateLBU(rt, base, s16(op_code.immediate));  exception = !handled; break;
		case OP_LD:		handled = GenerateLD( rt, base, s16(op_code.immediate));  exception = !handled; break;
		case OP_LWC1:	handled = GenerateLWC1(ft, base, s16(op_code.immediate)); exception = !handled; break;
		case OP_LDC1:	handled = GenerateLDC1(ft, base, s16(op_code.immediate)); exception = !handled; break;

		case OP_LUI:	GenerateLUI( rt, s16( op_code.immediate ) ); handled = true; break;

		case OP_CACHE:	handled = GenerateCACHE( base, op_code.immediate, rt ); exception = !handled; break;

		case OP_REGIMM:
			switch( op_code.regimm_op )
			{
				// These can be handled by the same Generate function, as the 'likely' bit is handled elsewhere
				case RegImmOp_BLTZ:
				case RegImmOp_BLTZL: GenerateBLTZ( rs, p_branch, p_branch_jump ); handled = true; break;

				case RegImmOp_BGEZ:
				case RegImmOp_BGEZL: GenerateBGEZ( rs, p_branch, p_branch_jump ); handled = true; break;
			}
			break;

		case OP_SPECOP:
			switch( op_code.spec_op )
			{
				case SpecOp_SLL: 	GenerateSLL( rd, rt, sa ); handled = true; break;
				case SpecOp_SRA: 	GenerateSRA( rd, rt, sa ); handled = true; break;
				//Causes Rayman 2's menu to stop working
				//case SpecOp_SRL: 	GenerateSRL( rd, rt, sa ); handled = true; break;

				case SpecOp_OR:		GenerateOR( rd, rs, rt ); handled = true; break;
				case SpecOp_AND:	GenerateAND( rd, rs, rt ); handled = true; break;
				case SpecOp_XOR:	GenerateXOR( rd, rs, rt ); handled = true; break;

				case SpecOp_ADD:	GenerateADDU( rd, rs, rt );	handled = true; break;
				case SpecOp_ADDU:	GenerateADDU( rd, rs, rt );	handled = true; break;

				case SpecOp_SUB:	GenerateSUBU( rd, rs, rt );	handled = true; break;
				case SpecOp_SUBU:	GenerateSUBU( rd, rs, rt );	handled = true; break;

				case SpecOp_MULTU:	GenerateMULT( rs, rt, true );	handled = true; break;
				case SpecOp_MULT:	GenerateMULT( rs, rt, false );	handled = true; break;

				case SpecOp_MFLO:	GenerateMFLO( rd );			handled = true; break;
				case SpecOp_MFHI:	GenerateMFHI( rd );			handled = true; break;
				case SpecOp_MTLO:	GenerateMTLO( rd );			handled = true; break;
				case SpecOp_MTHI:	GenerateMTHI( rd );			handled = true; break;

				case SpecOp_SLT:	GenerateSLT( rd, rs, rt, false );	handled = true; break;
				case SpecOp_SLTU:	GenerateSLT( rd, rs, rt, true );	handled = true; break;

				//case SpecOp_JR:		GenerateJR( rs, p_branch, p_branch_jump );	handled = true; exception = true; break; //Not Working

				default: break;
			}
			break;

		case OP_COPRO1:
			switch( op_code.cop1_op )
			{
				case Cop1Op_MFC1:	GenerateMFC1( rt, op_code.fs ); handled = true; break;
				case Cop1Op_MTC1:	GenerateMTC1( op_code.fs, rt ); handled = true; break;

				case Cop1Op_CFC1:	GenerateCFC1( rt, op_code.fs ); handled = true; break;
				case Cop1Op_CTC1:	GenerateCTC1( op_code.fs, rt ); handled = true; break;

				case Cop1Op_DInstr:
					switch( op_code.cop1_funct )
					{
						case Cop1OpFunc_ADD:	GenerateADD_D( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_SUB:	GenerateSUB_D( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_MUL:	GenerateMUL_D( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_DIV:	GenerateDIV_D( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
					}
					break;

				case Cop1Op_SInstr:
					switch( op_code.cop1_funct )
					{
						case Cop1OpFunc_ADD:	GenerateADD_S( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_SUB:	GenerateSUB_S( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_MUL:	GenerateMUL_S( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_DIV:	GenerateDIV_S( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_SQRT:	GenerateSQRT_S( op_code.fd, op_code.fs ); handled = true; break;

						case Cop1OpFunc_TRUNC_W:	GenerateTRUNC_W( op_code.fd, op_code.fs ); handled = true; break;

						case Cop1OpFunc_CMP_F:		GenerateCMP_S( op_code.fs, op_code.ft, NV ); handled = true; break;
						case Cop1OpFunc_CMP_UN:		GenerateCMP_S( op_code.fs, op_code.ft, VS ); handled = true; break;
						case Cop1OpFunc_CMP_EQ:		GenerateCMP_S( op_code.fs, op_code.ft, EQ ); handled = true; break;
						//case Cop1OpFunc_CMP_UEQ:	GenerateCMP_S( op_code.fs, op_code.ft,  ); handled = true; break;
						case Cop1OpFunc_CMP_ULT:	GenerateCMP_S( op_code.fs, op_code.ft, LT ); handled = true; break;
						case Cop1OpFunc_CMP_OLE:	GenerateCMP_S( op_code.fs, op_code.ft, LS ); handled = true; break;
						case Cop1OpFunc_CMP_ULE:	GenerateCMP_S( op_code.fs, op_code.ft, LE ); handled = true; break;

						case Cop1OpFunc_CMP_SF:		GenerateCMP_S( op_code.fs, op_code.ft, NV ); handled = true; break;
						case Cop1OpFunc_CMP_NGLE:	GenerateCMP_S( op_code.fs, op_code.ft, NV ); handled = true; break;
						case Cop1OpFunc_CMP_SEQ:	GenerateCMP_S( op_code.fs, op_code.ft, EQ ); handled = true; break;
						case Cop1OpFunc_CMP_NGL:	GenerateCMP_S( op_code.fs, op_code.ft, EQ ); handled = true; break;
						case Cop1OpFunc_CMP_LT:		GenerateCMP_S( op_code.fs, op_code.ft, CC ); handled = true; break;
						case Cop1OpFunc_CMP_NGE:	GenerateCMP_S( op_code.fs, op_code.ft, CC ); handled = true; break;
						case Cop1OpFunc_CMP_LE:		GenerateCMP_S( op_code.fs, op_code.ft, LS ); handled = true; break;
					}
					break;

				default: break;
			}
			break;

		default: break;
	}

	if (!handled)
	{
		CCodeLabel	no_target( NULL );

		if( R4300_InstructionHandlerNeedsPC( op_code ) )
		{
			MOV32(ArmReg_R0, address);
			STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CurrentPC));
			exception = true;
		}

		GenerateGenericR4300( op_code, R4300_GetInstructionHandler( op_code ) );

		if( exception )
		{
			exception_handler = GenerateBranchIfSet( const_cast< u32 * >( &gCPUState.StuffToDo ), no_target );
		}

		// Check whether we want to invert the status of this branch
		if( p_branch != NULL )
		{
			//
			// Check if the branch has been taken
			//
			if( p_branch->Direct )
			{
				if( p_branch->ConditionalBranchTaken )
				{
					*p_branch_jump = GenerateBranchIfNotEqual( &gCPUState.Delay, DO_DELAY, no_target );
				}
				else
				{
					*p_branch_jump = GenerateBranchIfEqual( &gCPUState.Delay, DO_DELAY, no_target );
				}
			}
			else
			{
				// XXXX eventually just exit here, and skip default exit code below
				if( p_branch->Eret )
				{
					*p_branch_jump = GenerateBranchAlways( no_target );
				}
				else
				{
					*p_branch_jump = GenerateBranchIfNotEqual( &gCPUState.TargetPC, p_branch->TargetAddress, no_target );
				}
			}
		}
		else
		{
			if( branch_delay_slot )
			{
				SetVar( &gCPUState.Delay, NO_DELAY );
			}
		}
	}
	else
	{
		if(exception)
		{
			exception_handler = GenerateBranchIfSet( const_cast< u32 * >( &gCPUState.StuffToDo ), CCodeLabel(NULL) );
		}

		if( p_branch && branch_delay_slot )
		{
			SetVar( &gCPUState.Delay, NO_DELAY );
		}
	}

	//Insure literal pool will be within range
	if(GetLiteralPoolDistance() > 3900)
		InsertLiteralPool();

	return exception_handler;
}

void	CCodeGeneratorARM::GenerateBranchHandler( CJumpLocation branch_handler_jump, RegisterSnapshotHandle snapshot )
{
	PatchJumpLong( branch_handler_jump, GetAssemblyBuffer()->GetLabel() );
}

void	CCodeGeneratorARM::GenerateGenericR4300( OpCode op_code, CPU_Instruction p_instruction )
{
	// XXXX Flush all fp registers before a generic call

	// Call function - __fastcall
	MOV32(ArmReg_R0, op_code._u32);
	CALL( CCodeLabel( (void*)p_instruction ) );
}

CJumpLocation CCodeGeneratorARM::ExecuteNativeFunction( CCodeLabel speed_hack, bool check_return )
{
	CALL( speed_hack );
	 
	if( check_return )
	{
		CMP_IMM( ArmReg_R0, 0 );
		return BX_IMM( CCodeLabel(NULL), EQ );
	}
	else
	{
		return CJumpLocation(NULL);
	}
}

//*****************************************************************************
//
//	Load Instructions
//
//*****************************************************************************

//Helper function, loads into register R0
inline void CCodeGeneratorARM::GenerateLoad( EN64Reg base, s16 offset, u8 twiddle, u8 bits, bool is_signed, void* p_read_memory )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		offset = offset ^ twiddle;

		LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[base]._u32_0));

		ADD(ArmReg_R1, ArmReg_R1, gMemoryBaseReg);

		if(abs(offset) >> 8)
		{
			if(offset > 0)
			{
				ADD_IMM(ArmReg_R1, ArmReg_R1, abs(offset) >> 8, 0xC);
				offset = abs(offset) & 0xFF;
			}
			else
			{
				SUB_IMM(ArmReg_R1, ArmReg_R1, abs(offset) >> 8, 0xC);
				offset = -(abs(offset) & 0xFF);
			}
		}

		switch(bits)
		{
			case 32:	LDR(ArmReg_R0, ArmReg_R1, offset); break;

			case 16:	if(is_signed)	{ LDRSH(ArmReg_R0, ArmReg_R1, offset); }
						else			{ LDRH (ArmReg_R0, ArmReg_R1, offset); } break; 

			case 8:		if(is_signed)	{ LDRSB(ArmReg_R0, ArmReg_R1, offset); }
						else			{ LDRB (ArmReg_R0, ArmReg_R1, offset); } break; 
		}

		
	}
	else
	{	
		//Slow Load
		LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[base]._u32_0));
		MOV32(ArmReg_R2, offset);
		ADD(ArmReg_R0, ArmReg_R1, ArmReg_R2);

		CALL( CCodeLabel( (void*)p_read_memory ) );
	}
}

//Load Word
bool CCodeGeneratorARM::GenerateLW( EN64Reg rt, EN64Reg base, s16 offset )
{
	GenerateLoad( base, offset, 0, 32, false, (void*)Read32Bits );

	MOV_ASR(ArmReg_R1, ArmReg_R0, 0x1F); //Sign extend
		
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));

	return true;
}

//Load Double Word
bool CCodeGeneratorARM::GenerateLD( EN64Reg rt, EN64Reg base, s16 offset )
{
	GenerateLoad( base, offset, 0, 32, false, (void*)Read32Bits );
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));

	GenerateLoad( base, offset + 4, 0, 32, false, (void*)Read32Bits );
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));

	return true;
}

//Load half word signed
bool CCodeGeneratorARM::GenerateLH( EN64Reg rt, EN64Reg base, s16 offset )
{
	GenerateLoad( base, offset, U16_TWIDDLE, 16, true, (void*)Read16Bits_Signed );

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	MOV_ASR(ArmReg_R1, ArmReg_R0, 0x1F); //Sign extend
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));

	return true;
}

//Load half word unsigned
bool CCodeGeneratorARM::GenerateLHU( EN64Reg rt, EN64Reg base, s16 offset )
{
	GenerateLoad( base, offset, U16_TWIDDLE, 16, false, (void*)Read16Bits );

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	XOR(ArmReg_R1, ArmReg_R0, ArmReg_R0); //Clear Hi
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));

	return true;
}

//Load byte signed
bool CCodeGeneratorARM::GenerateLB( EN64Reg rt, EN64Reg base, s16 offset )
{
	GenerateLoad( base, offset, U8_TWIDDLE, 8, true, (void*)Read8Bits_Signed );

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	MOV_ASR(ArmReg_R1, ArmReg_R0, 0x1F); //Sign extend
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));

	return true;
}

//Load byte unsigned
bool CCodeGeneratorARM::GenerateLBU( EN64Reg rt, EN64Reg base, s16 offset )
{
	GenerateLoad( base, offset, U8_TWIDDLE, 8, false, (void*)Read8Bits );

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	XOR(ArmReg_R1, ArmReg_R0, ArmReg_R0); //Clear Hi
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));

	return true;
}

bool CCodeGeneratorARM::GenerateLWC1( u32 ft, EN64Reg base, s16 offset )
{
	GenerateLoad( base, offset, 0, 32, false, (void*)Read32Bits );
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[ft]._u32));
	return true;
}

bool CCodeGeneratorARM::GenerateLDC1( u32 ft, EN64Reg base, s16 offset )
{
	GenerateLoad( base, offset, 0, 32, false, (void*)Read32Bits );
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[ft + 1]._u32));

	GenerateLoad( base, offset + 4, 0, 32, false, (void*)Read32Bits );
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[ft]._u32));

	return true;
}

//Load Upper Immediate
void CCodeGeneratorARM::GenerateLUI( EN64Reg rt, s16 immediate )
{
	MOV32(ArmReg_R0, immediate << 16);
	
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	MOV_ASR(ArmReg_R1, ArmReg_R0, 0x1F); //Sign extend
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));
}

//*****************************************************************************
//
//	Store Instructions
//
//*****************************************************************************

//Helper function, stores register R1 into memory
inline void CCodeGeneratorARM::GenerateStore( EN64Reg base, s16 offset, u8 twiddle, u8 bits, void* p_write_memory )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		offset = offset ^ twiddle;

		LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[base]._u32_0));

		ADD(ArmReg_R0, ArmReg_R0, gMemoryBaseReg);

		if(abs(offset) >> 8)
		{
			if(offset > 0)
			{
				ADD_IMM(ArmReg_R0, ArmReg_R0, abs(offset) >> 8, 0xC);
				offset = abs(offset) & 0xFF;
			}
			else
			{
				SUB_IMM(ArmReg_R0, ArmReg_R0, abs(offset) >> 8, 0xC);
				offset = -(abs(offset) & 0xFF);
			}
		}

		switch(bits)
		{
			case 32:	STR (ArmReg_R1, ArmReg_R0, offset); break;
			case 16:	STRH(ArmReg_R1, ArmReg_R0, offset); break; 
			case 8:		STRB(ArmReg_R1, ArmReg_R0, offset); break; 
		}
	}
	else
	{	
		//Slow Store
		LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[base]._u32_0));
		MOV32(ArmReg_R2, offset);
		ADD(ArmReg_R0, ArmReg_R0, ArmReg_R2);

		CALL( CCodeLabel( (void*)p_write_memory ) );
	}
}

// Store Word From Copro 1
bool CCodeGeneratorARM::GenerateSWC1( u32 ft, EN64Reg base, s16 offset )
{
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[ft]._u32));
	GenerateStore( base, offset, 0, 32, (void*)Write32Bits );
	return true;
}

bool CCodeGeneratorARM::GenerateSDC1( u32 ft, EN64Reg base, s16 offset )
{
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[ft + 1]._u32));
	GenerateStore( base, offset, 0, 32, (void*)Write32Bits );

	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[ft]._u32));
	GenerateStore( base, offset + 4, 0, 32, (void*)Write32Bits );
	return true;
}

bool CCodeGeneratorARM::GenerateSW( EN64Reg rt, EN64Reg base, s16 offset )
{
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	GenerateStore( base, offset, 0, 32, (void*)Write32Bits );

	return true;
}

bool CCodeGeneratorARM::GenerateSD( EN64Reg rt, EN64Reg base, s16 offset )
{
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));
	GenerateStore( base, offset, 0, 32, (void*)Write32Bits );

	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	GenerateStore( base, offset + 4, 0, 32, (void*)Write32Bits );

	return true;
}

bool CCodeGeneratorARM::GenerateSH( EN64Reg rt, EN64Reg base, s16 offset )
{
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	GenerateStore( base, offset, U16_TWIDDLE, 16, (void*)Write16Bits );
	return true;
}

bool CCodeGeneratorARM::GenerateSB( EN64Reg rt, EN64Reg base, s16 offset )
{
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	GenerateStore( base, offset, U8_TWIDDLE, 8, (void*)Write8Bits );
	return true;
}

//*****************************************************************************
//*****************************************************************************
//*****************************************************************************

bool CCodeGeneratorARM::GenerateCACHE( EN64Reg base, s16 offset, u32 cache_op )
{
	u32 dwCache = cache_op & 0x3;
	u32 dwAction = (cache_op >> 2) & 0x7;

	// For instruction cache invalidation, make sure we let the CPU know so the whole
	// dynarec system can be invalidated
	if(dwCache == 0 && (dwAction == 0 || dwAction == 4))
	{
		LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[base]._u32_0));
		MOV32(ArmReg_R1, offset);
		ADD(ArmReg_R0, ArmReg_R0, ArmReg_R1);
		MOV_IMM(ArmReg_R1, 0x20);
		CALL(CCodeLabel( (void*)CPU_InvalidateICacheRange ));

		return true;
	}

	return false;
}



void	CCodeGeneratorARM::GenerateJAL( u32 address )
{
	MOV32(ArmReg_R0, address + 8);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[N64Reg_RA]._u32_0));
	XOR(ArmReg_R1, ArmReg_R0, ArmReg_R0);
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[N64Reg_RA]._u32_1));
}

void CCodeGeneratorARM::GenerateADDIU( EN64Reg rt, EN64Reg rs, s16 immediate )
{
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	MOV32(ArmReg_R1, (u32)immediate);

	ADD(ArmReg_R0, ArmReg_R0, ArmReg_R1);
	
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	MOV_ASR(ArmReg_R1, ArmReg_R0, 0x1F); //Sign extend
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));
}

void CCodeGeneratorARM::GenerateANDI( EN64Reg rt, EN64Reg rs, u16 immediate )
{
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	MOV32(ArmReg_R1, (u32)immediate);

	AND(ArmReg_R0, ArmReg_R0, ArmReg_R1);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	XOR(ArmReg_R1, ArmReg_R0, ArmReg_R0);
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));
}

void CCodeGeneratorARM::GenerateORI( EN64Reg rt, EN64Reg rs, u16 immediate )
{
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	MOV32(ArmReg_R1, (u32)immediate);

	ORR(ArmReg_R0, ArmReg_R0, ArmReg_R1);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	XOR(ArmReg_R1, ArmReg_R0, ArmReg_R0);
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));
}

void CCodeGeneratorARM::GenerateXORI( EN64Reg rt, EN64Reg rs, u16 immediate )
{
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	MOV32(ArmReg_R1, (u32)immediate);

	XOR(ArmReg_R0, ArmReg_R0, ArmReg_R1);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	XOR(ArmReg_R1, ArmReg_R0, ArmReg_R0);
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));
}

// Set on Less Than Immediate
void CCodeGeneratorARM::GenerateSLTI( EN64Reg rt, EN64Reg rs, s16 immediate, bool is_unsigned )
{
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	MOV32(ArmReg_R2, immediate);
	MOV_IMM(ArmReg_R0, 0);

	CMP(ArmReg_R1, ArmReg_R2);

	MOV_IMM(ArmReg_R0, 1, 0, is_unsigned ? CC : LT);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
	XOR(ArmReg_R1, ArmReg_R0, ArmReg_R0);
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));
}

//*****************************************************************************
//
//	Special OPs
//
//*****************************************************************************

//Shift left logical
void CCodeGeneratorARM::GenerateSLL( EN64Reg rd, EN64Reg rt, u32 sa )
{
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));

	MOV_LSL(ArmReg_R0, ArmReg_R1, sa);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_0));
	MOV_ASR(ArmReg_R1, ArmReg_R0, 0x1F); //Sign extend
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_1));
}

//Shift right logical
void CCodeGeneratorARM::GenerateSRL( EN64Reg rd, EN64Reg rt, u32 sa )
{
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));

	MOV_LSR(ArmReg_R0, ArmReg_R1, sa);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_0));
	MOV_ASR(ArmReg_R1, ArmReg_R0, 0x1F); //Sign extend
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_1));
}
//Shift right arithmetic 
void CCodeGeneratorARM::GenerateSRA( EN64Reg rd, EN64Reg rt, u32 sa )
{
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));

	MOV_ASR(ArmReg_R0, ArmReg_R1, sa);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_0));
	MOV_ASR(ArmReg_R1, ArmReg_R0, 0x1F); //Sign extend
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_1));
}

void CCodeGeneratorARM::GenerateOR( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	//lo
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));

	ORR(ArmReg_R0, ArmReg_R0, ArmReg_R1);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_0));

	//hi
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_1));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));

	ORR(ArmReg_R0, ArmReg_R0, ArmReg_R1);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_1));
}

void CCodeGeneratorARM::GenerateXOR( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	//lo
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));

	XOR(ArmReg_R0, ArmReg_R0, ArmReg_R1);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_0));

	//hi
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_1));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));

	XOR(ArmReg_R0, ArmReg_R0, ArmReg_R1);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_1));
}

void CCodeGeneratorARM::GenerateAND( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	//lo
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));

	AND(ArmReg_R0, ArmReg_R0, ArmReg_R1);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_0));

	//hi
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_1));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_1));

	AND(ArmReg_R0, ArmReg_R0, ArmReg_R1);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_1));
}


void CCodeGeneratorARM::GenerateADDU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	LDR(ArmReg_R2, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));

	ADD(ArmReg_R0, ArmReg_R1, ArmReg_R2);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_0));
	MOV_ASR(ArmReg_R1, ArmReg_R0, 0x1F);
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_1));
}

void CCodeGeneratorARM::GenerateSUBU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	LDR(ArmReg_R2, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));

	SUB(ArmReg_R0, ArmReg_R1, ArmReg_R2);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_0));
	MOV_ASR(ArmReg_R1, ArmReg_R0, 0x1F);
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_1));
}

void CCodeGeneratorARM::GenerateMULT( EN64Reg rs, EN64Reg rt, bool is_unsigned )
{
	LDR(ArmReg_R2, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	LDR(ArmReg_R3, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));

	if(is_unsigned)
		UMULL( ArmReg_R0, ArmReg_R1, ArmReg_R2, ArmReg_R3 );
	else
		SMULL( ArmReg_R0, ArmReg_R1, ArmReg_R2, ArmReg_R3 );

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, MultLo._u32_0));
	MOV_ASR(ArmReg_R2, ArmReg_R0, 0x1F);
	STR(ArmReg_R2, ArmReg_R12, offsetof(SCPUState, MultLo._u32_1));

	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, MultHi._u32_0));
	MOV_ASR(ArmReg_R2, ArmReg_R1, 0x1F);
	STR(ArmReg_R2, ArmReg_R12, offsetof(SCPUState, MultHi._u32_1));
}

void CCodeGeneratorARM::GenerateMFLO( EN64Reg rd )
{
	//gGPR[ op_code.rd ]._u64 = gCPUState.MultLo._u64;

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, MultLo._u32_0));
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_0));

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, MultLo._u32_1));
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_1));
}

void CCodeGeneratorARM::GenerateMFHI( EN64Reg rd )
{
	//gGPR[ op_code.rd ]._u64 = gCPUState.MultHi._u64;
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, MultHi._u32_0));
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_0));

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, MultHi._u32_1));
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_1));
}

void CCodeGeneratorARM::GenerateMTLO( EN64Reg rs )
{
	//gCPUState.MultLo._u64 = gGPR[ op_code.rs ]._u64;
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, MultLo._u32_0));

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_1));
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, MultLo._u32_1));
}

void CCodeGeneratorARM::GenerateMTHI( EN64Reg rs )
{
	//gCPUState.MultHi._u64 = gGPR[ op_code.rs ]._u64;
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, MultHi._u32_0));

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_1));
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, MultHi._u32_1));
}

void CCodeGeneratorARM::GenerateSLT( EN64Reg rd, EN64Reg rs, EN64Reg rt, bool is_unsigned )
{
	LDR(ArmReg_R2, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	LDR(ArmReg_R3, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));

	MOV_IMM(ArmReg_R0, 0);

	CMP(ArmReg_R2, ArmReg_R3);

	MOV_IMM(ArmReg_R0, 1, 0, is_unsigned ? CC : LT);

	MOV_ASR(ArmReg_R1, ArmReg_R0, 0);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_0));
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rd]._u32_1));
}

void CCodeGeneratorARM::GenerateJR( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	//SetVar( &gCPUState.Delay, DO_DELAY );

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, TargetPC));

	MOV32(ArmReg_R1, p_branch->TargetAddress);
	CMP(ArmReg_R0, ArmReg_R1);

	*p_branch_jump = BX_IMM(CCodeLabel(nullptr), NE);
}

//*****************************************************************************
//
//	Branch instructions
//
//*****************************************************************************

void CCodeGeneratorARM::GenerateBEQ( EN64Reg rs, EN64Reg rt, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BEQ?" );
	#endif

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));

	// XXXX This may actually need to be a 64 bit compare, but this is what R4300.cpp does
	CMP(ArmReg_R0, ArmReg_R1);

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), NE);
	}
	else
	{
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), EQ);
	}
}

void CCodeGeneratorARM::GenerateBNE( EN64Reg rs, EN64Reg rt, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BEQ?" );
	#endif

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));

	// XXXX This may actually need to be a 64 bit compare, but this is what R4300.cpp does
	CMP(ArmReg_R0, ArmReg_R1);

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), EQ);
	}
	else
	{
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), NE);
	}
}

void CCodeGeneratorARM::GenerateBLTZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BLTZ?" );
	#endif

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	MOV_IMM( ArmReg_R1, 0);

	// XXXX This may actually need to be a 64 bit compare, but this is what R4300.cpp does
	CMP(ArmReg_R0, ArmReg_R1);

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), GE);
	}
	else
	{
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), LT);
	}
}

void CCodeGeneratorARM::GenerateBGEZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BLTZ?" );
	#endif

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rs]._u32_0));
	MOV_IMM( ArmReg_R1, 0);

	// XXXX This may actually need to be a 64 bit compare, but this is what R4300.cpp does
	CMP(ArmReg_R0, ArmReg_R1);

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), LT);
	}
	else
	{
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), GE);
	}
}

//*****************************************************************************
//
//	CoPro1 instructions
//
//*****************************************************************************

void CCodeGeneratorARM::GenerateADD_S( u32 fd, u32 fs, u32 ft )
{
	VLDR(ArmVfpReg_S0, ArmReg_R12, offsetof(SCPUState, FPU[fs]._u32));
	VLDR(ArmVfpReg_S2, ArmReg_R12, offsetof(SCPUState, FPU[ft]._u32));
	VADD(ArmVfpReg_S4, ArmVfpReg_S0, ArmVfpReg_S2);
	VSTR(ArmVfpReg_S4, ArmReg_R12, offsetof(SCPUState, FPU[fd]._u32));
}

void CCodeGeneratorARM::GenerateSUB_S( u32 fd, u32 fs, u32 ft )
{
	VLDR(ArmVfpReg_S0, ArmReg_R12, offsetof(SCPUState, FPU[fs]._u32));
	VLDR(ArmVfpReg_S2, ArmReg_R12, offsetof(SCPUState, FPU[ft]._u32));
	VSUB(ArmVfpReg_S4, ArmVfpReg_S0, ArmVfpReg_S2);
	VSTR(ArmVfpReg_S4, ArmReg_R12, offsetof(SCPUState, FPU[fd]._u32));
}

void CCodeGeneratorARM::GenerateMUL_S( u32 fd, u32 fs, u32 ft )
{
	VLDR(ArmVfpReg_S0, ArmReg_R12, offsetof(SCPUState, FPU[fs]._u32));
	VLDR(ArmVfpReg_S2, ArmReg_R12, offsetof(SCPUState, FPU[ft]._u32));
	VMUL(ArmVfpReg_S4, ArmVfpReg_S0, ArmVfpReg_S2);
	VSTR(ArmVfpReg_S4, ArmReg_R12, offsetof(SCPUState, FPU[fd]._u32));
}

void CCodeGeneratorARM::GenerateDIV_S( u32 fd, u32 fs, u32 ft )
{
	VLDR(ArmVfpReg_S0, ArmReg_R12, offsetof(SCPUState, FPU[fs]._u32));
	VLDR(ArmVfpReg_S2, ArmReg_R12, offsetof(SCPUState, FPU[ft]._u32));
	VDIV(ArmVfpReg_S4, ArmVfpReg_S0, ArmVfpReg_S2);
	VSTR(ArmVfpReg_S4, ArmReg_R12, offsetof(SCPUState, FPU[fd]._u32));
}

void CCodeGeneratorARM::GenerateSQRT_S( u32 fd, u32 fs )
{
	VLDR(ArmVfpReg_S0, ArmReg_R12, offsetof(SCPUState, FPU[fs]._u32));

	VSQRT(ArmVfpReg_S4, ArmVfpReg_S0);
	VSTR(ArmVfpReg_S4, ArmReg_R12, offsetof(SCPUState, FPU[fd]._u32));
}

void CCodeGeneratorARM::GenerateTRUNC_W( u32 fd, u32 fs )
{
	VLDR(ArmVfpReg_S0, ArmReg_R12, offsetof(SCPUState, FPU[fs]._f32));

	VCVT_S32_F32(ArmVfpReg_S4, ArmVfpReg_S0);
	VSTR(ArmVfpReg_S4, ArmReg_R12, offsetof(SCPUState, FPU[fd]._s32));
}

void CCodeGeneratorARM::GenerateCMP_S( u32 fs, u32 ft, EArmCond cond )
{
	if(cond == NV)
	{
		MOV32(ArmReg_R2, ~FPCSR_C);

		LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));
		AND(ArmReg_R0, ArmReg_R1, ArmReg_R2);

		STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));
	}
	else
	{
		VLDR(ArmVfpReg_S0, ArmReg_R12, offsetof(SCPUState, FPU[fs]._u32));
		VLDR(ArmVfpReg_S2, ArmReg_R12, offsetof(SCPUState, FPU[ft]._u32));

		MOV32(ArmReg_R2, ~FPCSR_C);
		LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));
		AND(ArmReg_R0, ArmReg_R1, ArmReg_R2);

		VCMP(ArmVfpReg_S0, ArmVfpReg_S2);

		MOV_IMM(ArmReg_R2, 0x02, 0x5);
		ADD(ArmReg_R0, ArmReg_R0, ArmReg_R2, cond);

		STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));
	}
}

void CCodeGeneratorARM::GenerateADD_D( u32 fd, u32 fs, u32 ft )
{
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[fs]._u32));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[fs + 1]._u32));
	VMOV(ArmVfpReg_S0, ArmReg_R0, ArmReg_R1);

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[ft]._u32));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[ft + 1]._u32));
	VMOV(ArmVfpReg_S2, ArmReg_R0, ArmReg_R1);

	VADD_D(ArmVfpReg_S0, ArmVfpReg_S0, ArmVfpReg_S2);

	VMOV(ArmReg_R0, ArmReg_R1, ArmVfpReg_S0);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[fd]._u32));
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[fd + 1]._u32));
}

void CCodeGeneratorARM::GenerateSUB_D( u32 fd, u32 fs, u32 ft )
{
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[fs]._u32));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[fs + 1]._u32));
	VMOV(ArmVfpReg_S0, ArmReg_R0, ArmReg_R1);

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[ft]._u32));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[ft + 1]._u32));
	VMOV(ArmVfpReg_S2, ArmReg_R0, ArmReg_R1);

	VSUB_D(ArmVfpReg_S0, ArmVfpReg_S0, ArmVfpReg_S2);

	VMOV(ArmReg_R0, ArmReg_R1, ArmVfpReg_S0);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[fd]._u32));
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[fd + 1]._u32));
}

void CCodeGeneratorARM::GenerateDIV_D( u32 fd, u32 fs, u32 ft )
{
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[fs]._u32));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[fs + 1]._u32));
	VMOV(ArmVfpReg_S0, ArmReg_R0, ArmReg_R1);

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[ft]._u32));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[ft + 1]._u32));
	VMOV(ArmVfpReg_S2, ArmReg_R0, ArmReg_R1);

	VDIV_D(ArmVfpReg_S0, ArmVfpReg_S0, ArmVfpReg_S2);

	VMOV(ArmReg_R0, ArmReg_R1, ArmVfpReg_S0);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[fd]._u32));
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[fd + 1]._u32));
}

void CCodeGeneratorARM::GenerateMUL_D( u32 fd, u32 fs, u32 ft )
{
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[fs]._u32));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[fs + 1]._u32));
	VMOV(ArmVfpReg_S0, ArmReg_R0, ArmReg_R1);

	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[ft]._u32));
	LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[ft + 1]._u32));
	VMOV(ArmVfpReg_S2, ArmReg_R0, ArmReg_R1);

	VMUL_D(ArmVfpReg_S0, ArmVfpReg_S0, ArmVfpReg_S2);

	VMOV(ArmReg_R0, ArmReg_R1, ArmVfpReg_S0);

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[fd]._u32));
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPU[fd + 1]._u32));
}

void CCodeGeneratorARM::GenerateMFC1( EN64Reg rt, u32 fs )
{
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[fs]._s32));

	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._s32_0));
	MOV_ASR(ArmReg_R1, ArmReg_R0, 0x1F);// Sign extend
	STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._s32_1));
}

void CCodeGeneratorARM::GenerateMTC1( u32 fs, EN64Reg rt )
{
	LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._s32_0));
	STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPU[fs]._s32));
}

void CCodeGeneratorARM::GenerateCFC1( EN64Reg rt, u32 fs )
{
	if ( fs == 0 || fs == 31 )
	{
		LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPUControl[fs]._s32));

		STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._s32_0));
		MOV_ASR(ArmReg_R1, ArmReg_R0, 0x1F);// Sign extend
		STR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, CPU[rt]._s32_1));
	}
}

void CCodeGeneratorARM::GenerateCTC1( u32 fs, EN64Reg rt )
{
	if ( fs == 31 )
	{
		LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[rt]._u32_0));
		STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPUControl[fs]._u32));
	}
}