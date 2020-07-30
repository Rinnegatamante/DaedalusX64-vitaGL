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

#ifndef HLEGRAPHICS_UCODES_UCODE_GBI0_H_
#define HLEGRAPHICS_UCODES_UCODE_GBI0_H_

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI0_Vtx( MicroCodeCommand command )
{
	u32 addr = RDPSegAddr(command.vtx0.addr);
	u32 v0   = command.vtx0.v0;
	u32 n    = command.vtx0.n + 1;

	gRenderer->SetNewVertexInfo( addr, v0, n );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI0_CullDL( MicroCodeCommand command )
{
	u32 first = (command.inst.cmd0 & 0x00FFFFFF) / 40;
	u32 last = (command.inst.cmd1 / 40) - 1;

	if( gRenderer->TestVerts( first, last ) )
	{
		return;
	}

	DLParser_PopDL();
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI0_Tri1( MicroCodeCommand command )
{
	DLParser_GBI1_Tri1_T< 10 >(command);
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI0_Line3D( MicroCodeCommand command )
{
	DLParser_GBI1_Line3D_T< 10 >(command);
}

#endif // HLEGRAPHICS_UCODES_UCODE_GBI0_H_
