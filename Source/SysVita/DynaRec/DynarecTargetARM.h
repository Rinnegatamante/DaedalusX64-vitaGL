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

#include "Core/R4300OpCode.h"

// ARM register codes.
enum EArmReg
{
	ArmReg_R0=0,   ArmReg_R1,  ArmReg_R2,  ArmReg_R3,
	ArmReg_R4,     ArmReg_R5,  ArmReg_R6,  ArmReg_R7,
	ArmReg_R8,     ArmReg_R9,  ArmReg_R10, ArmReg_R11,
	ArmReg_R12,    ArmReg_R13, ArmReg_R14, ArmReg_R15,

	NUM_ARM_REGISTERS = 16,

	//Aliases
};

enum EArmVfpReg
{
	F0 = 0, F1, F2, F3, F4,

	//Aliases
};

// ARM conditions. Do NOT reorder these
enum EArmCond
{
   EQ, NE, CS, CC, MI, PL, VS, VC,
   HI, LS, GE, LT, GT, LE, AL, NV
};

// Return true if this register dont need sign extension //Corn
inline bool	N64Reg_DontNeedSign( EN64Reg n64_reg )	{ return (0x30000001 >> n64_reg) & 1;}