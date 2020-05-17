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
#include "AssemblyWriterARM.h"
#include <cstdio>

void	CAssemblyWriterARM::CMP_IMM(EArmReg rn, u8 imm)
{
	EmitDWORD(0xe3500000 | rn << 12 | imm);
}

void	CAssemblyWriterARM::CMP(EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe1500000 | rn << 16 | rm);
}

void	CAssemblyWriterARM::ADD(EArmReg rd, EArmReg rn, EArmReg rm, EArmCond cond, u8 S)
{
	EmitDWORD(0x00800000 | (cond << 28) | (S << 20) | ((rd) << 12) | ((rn) << 16) | rm);
}

void	CAssemblyWriterARM::ADD_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4)
{
	EmitDWORD(0xe2800000 | (rd << 12) | (rn << 16) | ((ror4 & 0xf) << 8) | imm );
}

void	CAssemblyWriterARM::SUB(EArmReg rd, EArmReg rn, EArmReg rm, EArmCond cond, u8 S)
{
	EmitDWORD(0x00400000 | (cond << 28) | (S << 20) | ((rd) << 12) | ((rn) << 16) | rm);
}

void	CAssemblyWriterARM::SUB_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4, u8 S)
{
	EmitDWORD(0xe2400000 | (S << 20) | (rd << 12) | (rn << 16) | ((ror4 & 0xf) << 8) | imm );
}

void	CAssemblyWriterARM::RSB_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4)
{
	EmitDWORD(0xe2600000 | (rd << 12) | (rn << 16) | ((ror4 & 0xf) << 8) | imm );
}

void	CAssemblyWriterARM::ADC(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe0a00000 | (rd << 12) | (rn << 16) | rm);
}

void	CAssemblyWriterARM::ADC_IMM(EArmReg rd, EArmReg rn, u16 imm)
{
	EmitDWORD(0xe2a00000 | (rd << 12) | (rn << 16) | (imm & 0x0fff) );
}

void	CAssemblyWriterARM::SBC(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe0c00000 | (rd << 12) | (rn << 16) | rm);
}

void 	CAssemblyWriterARM::MUL(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe0000090 | (rm << 12) | (rd << 16) | rn);
}

void 	CAssemblyWriterARM::MLA(EArmReg rd, EArmReg rn, EArmReg rm, EArmReg ra)
{
	EmitDWORD(0xe0200090 | (rd << 16) | rn | (rm << 8) | (ra << 12));
}

void 	CAssemblyWriterARM::UMULL(EArmReg rdLo, EArmReg rdHi, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe0800090 | (rdHi << 16) | (rdLo << 12) | (rn << 8) | rm );
}

void 	CAssemblyWriterARM::SMULL(EArmReg rdLo, EArmReg rdHi, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe0c00090 | (rdHi << 16) | (rdLo << 12) | (rn << 8) | rm );
}

void	CAssemblyWriterARM::NEG(EArmReg rd, EArmReg rm)
{
	EmitDWORD(0xe2600000 | (rd << 12) | (rm << 16));
}

void	CAssemblyWriterARM::BIC_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4)
{
	EmitDWORD(0xe3c00000 | (rd << 12) | (rn << 16) | ((ror4 & 0xf) << 8) | imm);
}

void	CAssemblyWriterARM::AND(EArmReg rd, EArmReg rn, EArmReg rm, EArmCond cond)
{
	EmitDWORD(0x00000000 | (cond << 28) | ((rd) << 12) | ((rn) << 16) | rm);
}

void	CAssemblyWriterARM::AND_IMM(EArmReg rd, EArmReg rn, u8 imm)
{
	EmitDWORD(0xe2000000 | (rd << 12) | (rn << 16) | imm );
}

void	CAssemblyWriterARM::ORR(EArmReg rd, EArmReg rn, EArmReg rm, EArmCond cond)
{
	EmitDWORD(0x01800000 | (cond << 28) | (rd << 12) | (rn << 16) | rm);
}

void	CAssemblyWriterARM::ORR_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4, EArmCond cond)
{
	EmitDWORD(0x03800000 | (cond << 28) | (rd << 12) | (rn << 16) | ((ror4 & 0xf) << 8) | imm );
}

void	CAssemblyWriterARM::ORR_LSL(EArmReg rd, EArmReg rn, EArmReg rm, EArmReg rs)
{
	EmitDWORD(0xe1800010 | (rd << 12) | (rn << 16) | rm | (rs << 8));
}

void	CAssemblyWriterARM::ORR_LSR(EArmReg rd, EArmReg rn, EArmReg rm, EArmReg rs)
{
	EmitDWORD(0xe1800030 | (rd << 12) | (rn << 16) | rm | (rs << 8));
}

void	CAssemblyWriterARM::ORR_ASR(EArmReg rd, EArmReg rn, EArmReg rm, EArmReg rs, EArmCond cond)
{
	EmitDWORD(0x01800030 | (cond << 28) | (rd << 12) | (rn << 16) | rm | (rs << 8));
}

void	CAssemblyWriterARM::XOR(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe0200000 | (rd << 12) | (rn << 16) | rm);
}

void	CAssemblyWriterARM::B(u16 offset, EArmCond cond)
{
	EmitDWORD(0x0a000000 | (cond << 28) | (offset >> 2));
}

void	CAssemblyWriterARM::BX(EArmReg rm, EArmCond cond)
{
	EmitDWORD(0x012fff10 | rm | (cond << 28));
}

void	CAssemblyWriterARM::BLX(EArmReg rm, EArmCond cond)
{
	EmitDWORD(0x012fff30 | rm | (cond << 28));
}

void	CAssemblyWriterARM::PUSH(u16 regs)
{
	EmitDWORD(0xe92d0000 | regs);
}

void	CAssemblyWriterARM::POP(u16 regs)
{
	EmitDWORD(0xe8bd0000 | regs);
}

void	CAssemblyWriterARM::LDR(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe5100000 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | (offset & 0xfff));
}

void	CAssemblyWriterARM::LDRB(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe5500000 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | (offset & 0xfff));
}

void	CAssemblyWriterARM::LDRSB(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe15000d0 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | ((abs(offset) & 0xf0) << 4) | (abs(offset) & 0xf));
}

void	CAssemblyWriterARM::LDRSH(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe15000f0 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | ((abs(offset) & 0xf0) << 4) | (abs(offset) & 0xf));
}

void	CAssemblyWriterARM::LDRH(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe15000b0 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | ((abs(offset) & 0xf0) << 4) | (abs(offset) & 0xf));
}

void	CAssemblyWriterARM::LDRD(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe14000d0 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | ((abs(offset & 0xf0)) << 4) | (abs(offset) & 0xf));
}

void	CAssemblyWriterARM::LDRD_REG(EArmReg rt, EArmReg rn, EArmReg rm, u8 U)
{
	EmitDWORD(0xe10000d0 | (U << 23) | ( rn << 16 ) | ( rt << 12 ) | rm);
}

void	CAssemblyWriterARM::LDMIA(EArmReg rn, u16 regs)
{
	EmitDWORD(0xe8900000 | ( rn << 16 ) | regs);
}

void	CAssemblyWriterARM::STR(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe5000000 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | (offset & 0xfff));
}

void	CAssemblyWriterARM::STRH(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe14000b0 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | ((abs(offset) & 0xf0) << 4) | (abs(offset) & 0xf));
}

void	CAssemblyWriterARM::STRB(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe5400000 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | (offset & 0xfff));
}

void	CAssemblyWriterARM::STRD(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe14000f0 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | ((abs(offset) & 0xf0) << 4) | (abs(offset) & 0xf));
}

void	CAssemblyWriterARM::STRD_REG(EArmReg rt, EArmReg rn, EArmReg rm, u8 U)
{
	EmitDWORD(0xe10000f0 | (U << 23) | ( rn << 16 ) | ( rt << 12 ) | rm);
}

void	CAssemblyWriterARM::MVN(EArmReg rd, EArmReg rm)
{
	EmitDWORD(0xe1e00000 | (rd << 12) | rm);
}

void	CAssemblyWriterARM::MOV(EArmReg rd, EArmReg rm)
{
	EmitDWORD(0xe1a00000 | (rd << 12) | rm);
}

void	CAssemblyWriterARM::MOV_IMM(EArmReg rd, u8 imm, u8 ror4, EArmCond cond)
{
	EmitDWORD(0x03a00000 | (cond << 28) | (rd << 12) | ((ror4 & 0xf) << 8) | imm);
}

void	CAssemblyWriterARM::MOV_LSL(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe1a00010 | (rd << 12) | rn | (rm << 8));
}

void	CAssemblyWriterARM::MOV_LSR(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe1a00030 | (rd << 12) | rn | (rm << 8));
}

void	CAssemblyWriterARM::MOV_ASR(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe1a00050 | (rd << 12) | rn | (rm << 8));
}

void	CAssemblyWriterARM::MOV_LSL_IMM(EArmReg rd, EArmReg rm, u8 imm5)
{
	EmitDWORD(0xe1a00000 | (rd << 12) | rm | (imm5 << 7));
}

void	CAssemblyWriterARM::MOV_LSR_IMM(EArmReg rd, EArmReg rm, u8 imm5)
{
	EmitDWORD(0xe1a00020 | (rd << 12) | rm | (imm5 << 7));
}

void	CAssemblyWriterARM::MOV_ASR_IMM(EArmReg rd, EArmReg rm, u8 imm5)
{
	EmitDWORD(0xe1a00040 | (rd << 12) | rm | (imm5 << 7));
}

void	CAssemblyWriterARM::VLDR(EArmVfpReg fd, EArmReg rn, s16 offset12)
{
	EmitDWORD(0xed100a00 | ((offset12 < 0) ? 0 : 1) << 23 | ((fd & 1) << 22) | (rn << 16) | (((fd >> 1) & 15) << 12) | ((abs(offset12) >> 2) & 255));
}

void	CAssemblyWriterARM::VSTR(EArmVfpReg fd, EArmReg rn, s16 offset12)
{
	EmitDWORD(0xed000a00 | ((offset12 < 0) ? 0 : 1) << 23 | ((fd & 1) << 22) | (rn << 16) | (((fd >> 1) & 15) << 12) | ((abs(offset12) >> 2) & 255));
}

void	CAssemblyWriterARM::VADD(EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm)
{
	EmitDWORD(0xee300a00 | ((Sd & 1) << 22) | (((Sn >> 1) & 15) << 16) | (((Sd >> 1) & 15) << 12) | ((Sn & 1) << 7) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VSUB(EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm)
{
	EmitDWORD(0xee300a40 | ((Sd & 1) << 22) | (((Sn >> 1) & 15) << 16) | (((Sd >> 1) & 15) << 12) | ((Sn & 1) << 7) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VMUL(EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm)
{
	EmitDWORD(0xee200a00 | ((Sd & 1) << 22) | (((Sn >> 1) & 15) << 16) | (((Sd >> 1) & 15) << 12) | ((Sn & 1) << 7) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VDIV(EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm)
{
	EmitDWORD(0xee800a00 | ((Sd & 1) << 22) | (((Sn >> 1) & 15) << 16) | (((Sd >> 1) & 15) << 12) | ((Sn & 1) << 7) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VSQRT(EArmVfpReg Sd, EArmVfpReg Sm)
{
	EmitDWORD(0xeeb10ac0 | ((Sd & 1) << 22) | (((Sd >> 1) & 15) << 12) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VABS(EArmVfpReg Sd, EArmVfpReg Sm)
{
	EmitDWORD(0xeeb00ac0 | ((Sd & 1) << 22) | (((Sd >> 1) & 15) << 12) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VNEG(EArmVfpReg Sd, EArmVfpReg Sm)
{
	EmitDWORD(0xeeb10a40 | ((Sd & 1) << 22) | (((Sd >> 1) & 15) << 12) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VCMP(EArmVfpReg Sd, EArmVfpReg Sm, u8 E)
{
	EmitDWORD(0xeeb40a40 | ((Sd & 1) << 22) | (((Sd >> 1) & 15) << 12) | ((Sm & 1) << 5) | ((Sm >> 1) & 15) | (E << 7));
	
	//vmrs    APSR_nzcv, FPSCR    @ Get the flags into APSR.
	EmitDWORD(0xeef1fa10);
}

void	CAssemblyWriterARM::VCVT_S32_F32(EArmVfpReg Sd, EArmVfpReg Sm)
{
	EmitDWORD(0xeebd0ac0 | ((Sd & 1) << 22) | (((Sd >> 1) & 15) << 12) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VCVT_F64_F32(EArmVfpReg Dd, EArmVfpReg Sm)
{
	EmitDWORD(0xeeb70ac0 | (((Dd >> 4) & 1) << 22) | ((Dd & 15) << 12) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VADD_D(EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm)
{
	EmitDWORD(0xee300b00 | (((Dd >> 4) & 1) << 22) | ((Dn & 15) << 16) | ((Dd & 15) << 12) | (((Dn >> 4) & 1) << 7) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VSUB_D(EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm)
{
	EmitDWORD(0xee300b40 | (((Dd >> 4) & 1) << 22) | ((Dn & 15) << 16) | ((Dd & 15) << 12) | (((Dn >> 4) & 1) << 7) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VMUL_D(EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm)
{
	EmitDWORD(0xee200b00 | (((Dd >> 4) & 1) << 22) | ((Dn & 15) << 16) | ((Dd & 15) << 12) | (((Dn >> 4) & 1) << 7) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VDIV_D(EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm)
{
	EmitDWORD(0xee800b00 | (((Dd >> 4) & 1) << 22) | ((Dn & 15) << 16) | ((Dd & 15) << 12) | (((Dn >> 4) & 1) << 7) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VSQRT_D(EArmVfpReg Dd, EArmVfpReg Dm)
{
	EmitDWORD(0xeeb10bc0 | (((Dd >> 4) & 1) << 22) | ((Dd & 15) << 12) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VABS_D(EArmVfpReg Dd, EArmVfpReg Dm)
{
	EmitDWORD(0xeeb00bc0 | (((Dd >> 4) & 1) << 22) | ((Dd & 15) << 12) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VNEG_D(EArmVfpReg Dd, EArmVfpReg Dm)
{
	EmitDWORD(0xeeb10b40 | (((Dd >> 4) & 1) << 22) | ((Dd & 15) << 12) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VMOV(EArmVfpReg Dm, EArmReg Rt, EArmReg Rt2)
{
	EmitDWORD(0xec400b10 | (Rt2 << 16) | (Rt << 12) | (Dm & 0b1111) | (((Dm >> 4) & 1) << 5));
}

void	CAssemblyWriterARM::VMOV(EArmReg Rt, EArmReg Rt2, EArmVfpReg Dm)
{
	EmitDWORD(0xec500b10 | (Rt2 << 16) | (Rt << 12) | (Dm & 0b1111) | (((Dm >> 4) & 1) << 5));
}

void	CAssemblyWriterARM::VLDR_D(EArmVfpReg Dd, EArmReg Rn, s16 offset12)
{
	EmitDWORD(0xed100b00 | ((offset12 < 0) ? 0 : 1) << 23 | (((Dd >> 4) & 1) << 22) | (Rn << 16) | ((Dd & 15) << 12) | ((abs(offset12) >> 2) & 255));
}

void	CAssemblyWriterARM::VSTR_D(EArmVfpReg Dd, EArmReg Rn, s16 offset12)
{
	EmitDWORD(0xed000b00 | ((offset12 < 0) ? 0 : 1) << 23 | (((Dd >> 4) & 1) << 22) | (Rn << 16) | ((Dd & 15) << 12) | ((abs(offset12) >> 2) & 255));
}

void	CAssemblyWriterARM::VCMP_D(EArmVfpReg Dd, EArmVfpReg Dm, u8 E)
{
	EmitDWORD(0xeeb40b40 | (((Dd >> 4) & 1) << 22) | ((Dd & 15) << 12) | (((Dm >> 4) & 1) << 5) | (Dm & 15) | (E << 7));
	
	//vmrs    APSR_nzcv, FPSCR    @ Get the flags into APSR.
	EmitDWORD(0xeef1fa10);
}

void	CAssemblyWriterARM::VCVT_S32_F64(EArmVfpReg Sd, EArmVfpReg Dm)
{
	EmitDWORD(0xeebd0bc0 | ((Sd & 1) << 22) | (((Sd >> 1) & 15) << 12) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VCVT_F32_F64(EArmVfpReg Sd, EArmVfpReg Dm)
{
	EmitDWORD(0xeeb70bc0 | ((Sd & 1) << 22) | (((Sd >> 1) & 15) << 12) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

#ifdef DYNAREC_ARMV7
void	CAssemblyWriterARM::MOVW(EArmReg reg, u16 imm)
{
	EmitDWORD(0xe3000000 | (reg << 12) | ((imm & 0xf000) << 4) | (imm & 0x0fff));
}

void	CAssemblyWriterARM::MOVT(EArmReg reg, u16 imm)
{
	EmitDWORD(0xe3400000 | (reg << 12) | ((imm & 0xf000) << 4) | (imm & 0x0fff));
}
#endif

void	CAssemblyWriterARM::MOV32(EArmReg reg, u32 imm)
{
	#ifdef DYNAREC_ARMV7
	MOVW(reg, imm);
	if (imm >> 16)
	{
		MOVT(reg, imm >> 16);
	}
	#else
	LDMIA(ArmReg_R15, 1 << reg); // LDMIA PC, {reg}
	B(1);
	EmitDWORD(imm);
	#endif
}

CJumpLocation CAssemblyWriterARM::BX_IMM( CCodeLabel target, EArmCond cond )
{
	u32 address( target.GetTargetU32() );

	CJumpLocation jump_location( mpAssemblyBuffer->GetJumpLocation() );

	#ifdef DYNAREC_ARMV7
	MOVW(ArmReg_R4, address);
	MOVT(ArmReg_R4, address >> 16);
	#else
	MOV_IMM(ArmReg_R4, address);
	ADD_IMM(ArmReg_R4, ArmReg_R4, address >> 8, 0xc);
	ADD_IMM(ArmReg_R4, ArmReg_R4, address >> 16, 0x8);
	ADD_IMM(ArmReg_R4, ArmReg_R4, address >> 24, 0x4);
	#endif

	BX(ArmReg_R4, cond);

	return jump_location;
}

void CAssemblyWriterARM::CALL( CCodeLabel target )
{
	PUSH(0x1000); //R12

	MOV32(ArmReg_R4, target.GetTargetU32());
	BLX(ArmReg_R4);

	POP(0x1000); //R12
}

void CAssemblyWriterARM::RET()
{
	POP(0x8ff0); //POP {R4-R11, PC}
}
