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
	public:
		CAssemblyWriterARM( CAssemblyBuffer * p_buffer ) : mpAssemblyBuffer( p_buffer )	{}

		CAssemblyBuffer*	GetAssemblyBuffer() const									{ return mpAssemblyBuffer; }
		void				SetAssemblyBuffer( CAssemblyBuffer * p_buffer )				{ mpAssemblyBuffer = p_buffer; }

		inline void NOP()	{	EmitDWORD(0xe1a00000);	}

		void				ADD    (EArmReg rd, EArmReg rn, EArmReg rm, EArmCond = AL, u8 S = 0);
		void				ADD_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4 = 0);

		void				ADC    (EArmReg rd, EArmReg rn, EArmReg rm);
		void				ADC_IMM(EArmReg rd, EArmReg rn, u16 imm);

		void				SBC(EArmReg rd, EArmReg rn, EArmReg rm);

		void				SUB(EArmReg rd, EArmReg rn, EArmReg rm, EArmCond = AL, u8 S = 0);
		void				SUB_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4 = 0, u8 S = 0);
		void				RSB_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4 = 0);

		void				MUL  (EArmReg rd, EArmReg rn, EArmReg rm);
		void				MLA  (EArmReg rd, EArmReg rn, EArmReg rm, EArmReg ra);
		void				UMULL(EArmReg rdLo, EArmReg rdHi, EArmReg rn, EArmReg rm);
		void				SMULL(EArmReg rdLo, EArmReg rdHi, EArmReg rn, EArmReg rm);

		void				NEG(EArmReg rd, EArmReg rm);

		void				BIC_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4 = 0);
		void				AND    (EArmReg rd, EArmReg rn, EArmReg rm, EArmCond = AL);
		void				AND_IMM(EArmReg rd, EArmReg rn, u8 imm);

		void				ORR    (EArmReg rd, EArmReg rn, EArmReg rm, EArmCond = AL);
		void				ORR_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4 = 0, EArmCond = AL);
		void				ORR_LSL(EArmReg rd, EArmReg rn, EArmReg rm, EArmReg rs);
		void				ORR_LSR(EArmReg rd, EArmReg rn, EArmReg rm, EArmReg rs);
		void				ORR_ASR(EArmReg rd, EArmReg rn, EArmReg rm, EArmReg rs, EArmCond = AL);

		void				XOR(EArmReg rd, EArmReg rn, EArmReg rm);

		void				TST    (EArmReg rn, EArmReg rm);
		void				CMP    (EArmReg rn, EArmReg rm);
		void				CMP_IMM(EArmReg rn, u8 imm);

		void				B  (u16 offset, EArmCond cond = AL);
		void				BX (EArmReg rm, EArmCond cond = AL);
		void				BLX(EArmReg rm, EArmCond cond = AL);
		
		void				PUSH(u16 regs);
		void				POP (u16 regs);

		void				LDR  (EArmReg rt, EArmReg rn, s16 offset);
		void				LDRB (EArmReg rt, EArmReg rn, s16 offset);
		void				LDRSB(EArmReg rt, EArmReg rn, s16 offset);
		void				LDRH (EArmReg rt, EArmReg rn, s16 offset);
		void				LDRSH(EArmReg rt, EArmReg rn, s16 offset);
		void				LDRD (EArmReg rt, EArmReg rn, s16 offset);
		void				LDRD_REG(EArmReg rt, EArmReg rn, EArmReg rm, u8 U = 1);
		void				LDMIA(EArmReg rn, u16 regs);

		void				STR (EArmReg rt, EArmReg rn, s16 offset);
		void				STRH(EArmReg rt, EArmReg rn, s16 offset);
		void				STRB(EArmReg rt, EArmReg rn, s16 offset);
		void				STRD(EArmReg rt, EArmReg rn, s16 offset);
		void				STRD_REG(EArmReg rt, EArmReg rn, EArmReg rm, u8 U = 1);

		void				MVN(EArmReg rd, EArmReg rm);

		void				MOV        (EArmReg rd, EArmReg rm);
		void				MOV_IMM    (EArmReg rd, u8 imm, u8 ror4 = 0, EArmCond = AL);
		void				MOV_LSL    (EArmReg rd, EArmReg rn, EArmReg rm);
		void				MOV_LSR    (EArmReg rd, EArmReg rn, EArmReg rm);
		void				MOV_ASR    (EArmReg rd, EArmReg rn, EArmReg rm);
		void				MOV_LSL_IMM(EArmReg rd, EArmReg rm, u8 imm5);
		void				MOV_LSR_IMM(EArmReg rd, EArmReg rm, u8 imm5);
		void				MOV_ASR_IMM(EArmReg rd, EArmReg rm, u8 imm5);

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
		void				VABS (EArmVfpReg Sd, EArmVfpReg Sm);
		void				VNEG (EArmVfpReg Sd, EArmVfpReg Sm);
		void				VCMP (EArmVfpReg Sd, EArmVfpReg Sm, u8 E = 0);
		void				VCVT_S32_F32(EArmVfpReg Sd, EArmVfpReg Sm);
		void				VCVT_F64_F32(EArmVfpReg Dd, EArmVfpReg Sm);

		void				VMOV (EArmVfpReg dm, EArmReg rt, EArmReg rt2);
		void				VMOV (EArmReg rt, EArmReg rt2, EArmVfpReg dm);

		void				VADD_D (EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm);
		void				VSUB_D (EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm);
		void				VMUL_D (EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm);
		void				VDIV_D (EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm);
		void				VSQRT_D(EArmVfpReg Dd, EArmVfpReg Dm);
		void				VABS_D (EArmVfpReg Dd, EArmVfpReg Dm);
		void				VNEG_D (EArmVfpReg Dd, EArmVfpReg Dm);
		void				VCMP_D (EArmVfpReg Sd, EArmVfpReg Sm, u8 E = 0);
		void				VCVT_S32_F64(EArmVfpReg Sd, EArmVfpReg Dm);
		void				VCVT_F32_F64(EArmVfpReg Sd, EArmVfpReg Dm);

		void				VLDR_D (EArmVfpReg dd, EArmReg rn, s16 offset12);
		void				VSTR_D (EArmVfpReg dd, EArmReg rn, s16 offset12);

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
};