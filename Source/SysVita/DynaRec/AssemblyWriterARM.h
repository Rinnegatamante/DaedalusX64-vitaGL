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

#include <vector>

#include "DynaRec/AssemblyBuffer.h"
#include "DynarecTargetARM.h"

class CAssemblyWriterARM
{
	struct Literal
	{
		CCodeLabel 	Target;
		uint32_t	Value;
	};

	public:
		CAssemblyWriterARM( CAssemblyBuffer * p_buffer ) : mpAssemblyBuffer( p_buffer )	{}

		CAssemblyBuffer*	GetAssemblyBuffer() const									{ return mpAssemblyBuffer; }
		void				SetAssemblyBuffer( CAssemblyBuffer * p_buffer )				{ mpAssemblyBuffer = p_buffer; }

		void				InsertLiteralPool();
		uint32_t			GetLiteralPoolDistance();

		inline void NOP()	{	EmitDWORD(0xe1a00000);	}

		void				ADD    (EArmReg rd, EArmReg rn, EArmReg rm, EArmCond = AL);
		void				ADD_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4 = 0);

		void				SUB(EArmReg rd, EArmReg rn, EArmReg rm, EArmCond = AL);

		void				MUL  (EArmReg rd, EArmReg rn, EArmReg rm);
		void				UMULL(EArmReg rdLo, EArmReg rdHi, EArmReg rn, EArmReg rm);
		void				SMULL(EArmReg rdLo, EArmReg rdHi, EArmReg rn, EArmReg rm);

		void				AND    (EArmReg rd, EArmReg rn, EArmReg rm, EArmCond = AL);
		void				AND_IMM(EArmReg rd, EArmReg rn, u8 imm);

		void				ORR(EArmReg rd, EArmReg rn, EArmReg rm);
		void				XOR(EArmReg rd, EArmReg rn, EArmReg rm);

		void				CMP_IMM(EArmReg rn, u8 imm);
		void				CMP(EArmReg rn, EArmReg rm);

		void				B  (u16 offset, EArmCond cond = AL);
		void				BX (EArmReg rm, EArmCond cond = AL);
		void				BLX(EArmReg rm, EArmCond cond = AL);
		
		void				PUSH(u16 regs);
		void				POP (u16 regs);

		void				LDR  (EArmReg rt, EArmReg rn, u16 offset);
		void				LDRB (EArmReg rt, EArmReg rn, u16 offset);
		void				LDRSB(EArmReg rt, EArmReg rn, u16 offset);
		void				LDRH (EArmReg rt, EArmReg rn, u16 offset);
		void				LDRSH(EArmReg rt, EArmReg rn, u16 offset);

		void				STR (EArmReg rt, EArmReg rn, u16 offset);
		void				STRH(EArmReg rt, EArmReg rn, u16 offset);
		void				STRB(EArmReg rt, EArmReg rn, u16 offset);

		void				MOV    (EArmReg rd, EArmReg rm);
		void				MOV_LSL(EArmReg rd, EArmReg rm, u8 imm5);
		void				MOV_LSR(EArmReg rd, EArmReg rm, u8 imm5);
		void				MOV_ASR(EArmReg rd, EArmReg rm, u8 imm5);
		void				MOV_IMM(EArmReg rd, u8 imm, u8 ror4 = 0, EArmCond = AL);

		void				MOVW(EArmReg reg, u16 imm);
		void				MOVT(EArmReg reg, u16 imm);

		/* Vfp instructions */
		void				VLDR (EArmVfpReg fd, EArmReg rn, s16 offset12);
		void				VSTR (EArmVfpReg fd, EArmReg rn, s16 offset12);
		void				VADD (EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm);
		void				VSUB (EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm);
		void				VMUL (EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm);
		void				VDIV (EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm);
		void				VSQRT(EArmVfpReg Sd, EArmVfpReg Sm);
		void				VCMP (EArmVfpReg Sd, EArmVfpReg Sm);

		/* Pseudo instructions for convinience */
		void				MOV32(EArmReg reg, u32 imm);
		void				CALL(CCodeLabel target);
		void				RET();
		CJumpLocation 		BX_IMM(CCodeLabel target, EArmCond cond = AL);

	private:
		inline void EmitBYTE(u8 byte)
		{
			mpAssemblyBuffer->EmitBYTE( byte );
		}

		inline void EmitWORD(u16 word)
		{
			mpAssemblyBuffer->EmitWORD( word );
		}

		inline void EmitDWORD(u32 dword)
		{
			mpAssemblyBuffer->EmitDWORD( dword );
		}

		CAssemblyBuffer*	mpAssemblyBuffer;
		std::vector<Literal> literals;
};