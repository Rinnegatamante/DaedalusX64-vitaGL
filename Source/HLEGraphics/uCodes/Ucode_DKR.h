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

#ifndef HLEGRAPHICS_UCODES_UCODE_DKR_H_
#define HLEGRAPHICS_UCODES_UCODE_DKR_H_

u32 gDKRMatrixAddr = 0;
u32 gDKRVtxCount = 0;
bool gDKRBillBoard = false;

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI0_Vtx_DKR( MicroCodeCommand command )
{
	u32 address		= command.inst.cmd1 + gAuxAddr;
	u32 num_verts   = ((command.inst.cmd0 >> 19) & 0x1F);
	u32 v0_idx		= 0;

	// Increase by one num verts for DKR
	if( g_ROM.GameHacks == DKR ) num_verts++;

	if( command.inst.cmd0 & 0x00010000 )
	{
		if( gDKRBillBoard )
			gDKRVtxCount = 1;
	}
	else
	{
		gDKRVtxCount = 0;
	}

	v0_idx = ((command.inst.cmd0 >> 9) & 0x1F) + gDKRVtxCount;
	gRenderer->SetNewVertexInfoDKR(address, v0_idx, num_verts, gDKRBillBoard);

	gDKRVtxCount += num_verts;
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_DLInMem( MicroCodeCommand command )
{
	gDlistStackPointer++;
	gDlistStack.address[gDlistStackPointer] = RDPSegAddr(command.inst.cmd1);
	gDlistStack.limit = (command.inst.cmd0 >> 16) & 0xFF;
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_Mtx_DKR( MicroCodeCommand command )
{
	u32 address	= command.inst.cmd1 + RDPSegAddr(gDKRMatrixAddr);
	u32 index = (command.inst.cmd0 >> 16) & 0xF;

	bool mul = false;

	if (index == 0)
	{
		//DKR : no mult
		index = (command.inst.cmd0 >> 22) & 0x3;
	}
	else
	{
		//JFG : mult but only if bit is set
		mul = ((command.inst.cmd0 >> 23) & 0x1);
	}

	// Load matrix from address
	gRenderer->SetDKRMat(address, mul, index);
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_MoveWord_DKR( MicroCodeCommand command )
{
	switch( command.inst.cmd0 & 0xFF )
	{
	case G_MW_NUMLIGHT:
		gDKRBillBoard = command.inst.cmd1 & 0x1;
		#ifdef DAEDALUS_DEBUG_DISPLAYLIST
		DL_PF("    DKR BillBoard: %d", gDKRBillBoard);
		#endif
		break;

	case G_MW_LIGHTCOL:
		{
		u32 idx = (command.inst.cmd1 >> 6) & 0x3;
		gRenderer->DKRMtxChanged( idx );
		#ifdef DAEDALUS_DEBUG_DISPLAYLIST
		DL_PF("    DKR MtxIdx: %d", idx);
		#endif
		}
		break;

	default:
		DLParser_GBI1_MoveWord( command );
		break;
	}
}
//*****************************************************************************
//
//*****************************************************************************
void DLParser_Set_Addr_DKR( MicroCodeCommand command )
{
	gDKRMatrixAddr  = command.inst.cmd0 & 0x00FFFFFF;
	gAuxAddr		= RDPSegAddr(command.inst.cmd1 & 0x00FFFFFF);
	gDKRVtxCount	= 0;
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_DMA_Tri_DKR( MicroCodeCommand command )
{
	u32 address = RDPSegAddr(command.inst.cmd1);
	u32 count = (command.inst.cmd0 >> 4) & 0x1F;	//Count should never exceed 16

	TriDKR *tri = (TriDKR*)(g_pu8RamBase + address);

	bool tris_added = false;

	for (u32 i = 0; i < count; i++)
	{
		u32 v0_idx = tri->v0;
		u32 v1_idx = tri->v1;
		u32 v2_idx = tri->v2;

		gRenderer->SetCullMode( !(tri->flag & 0x40), true );

		const u32 new_v0_idx = i * 3 + 32;
		const u32 new_v1_idx = i * 3 + 33;
		const u32 new_v2_idx = i * 3 + 34;

		gRenderer->CopyVtx( v0_idx, new_v0_idx);
		gRenderer->CopyVtx( v1_idx, new_v1_idx);
		gRenderer->CopyVtx( v2_idx, new_v2_idx);

		if( gRenderer->AddTri(new_v0_idx, new_v1_idx, new_v2_idx) )
		{
			tris_added = true;
			// Generate texture coordinates...
			gRenderer->SetVtxTextureCoord( new_v0_idx, tri->s0, tri->t0 );
			gRenderer->SetVtxTextureCoord( new_v1_idx, tri->s1, tri->t1 );
			gRenderer->SetVtxTextureCoord( new_v2_idx, tri->s2, tri->t2 );
		}
		tri++;
	}

	if(tris_added)
	{
		gRenderer->FlushTris();
	}

	gDKRVtxCount = 0;
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_Texture_DKR( MicroCodeCommand command )
{
	u32 tile    = command.texture.tile;

	// Force enable texture in DKR Ucode, fixes static texture bug etc
	gRenderer->SetTextureEnable(true);
	gRenderer->SetTextureTile(tile);

	f32 scale_s = f32(command.texture.scaleS)  / (65535.0f * 32.0f);
	f32 scale_t = f32(command.texture.scaleT)  / (65535.0f * 32.0f);

	gRenderer->SetTextureScale( scale_s, scale_t );
}

#endif // HLEGRAPHICS_UCODES_UCODE_DKR_H_
