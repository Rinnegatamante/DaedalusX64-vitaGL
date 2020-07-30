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

#ifndef HLEGRAPHICS_UCODES_UCODE_CONKER_H_
#define HLEGRAPHICS_UCODES_UCODE_CONKER_H_

// Alot cheaper than check mux
// TODO: Should handle shadow eventually!
#define CONKER_SHADOW 0x005049d8//0x00ffe9ffffd21f0fLL
//*****************************************************************************
//
//*****************************************************************************
void DLParser_Vtx_Conker( MicroCodeCommand command )
{
	if( g_CI.Format != G_IM_FMT_RGBA || (gRDPOtherMode.L == CONKER_SHADOW) )
	{
		return;
	}

	u32 address = RDPSegAddr(command.inst.cmd1);
	u32 len    = ((command.inst.cmd0 >> 1 )& 0x7F) ;
	u32 n      = ((command.inst.cmd0 >> 12)& 0xFF);
	u32 v0		= len - n;

	gRenderer->SetNewVertexInfoConker( address, v0, n );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_Tri1_Conker( MicroCodeCommand command )
{

	// While the next command pair is Tri1, add vertices
	u32 pc = gDlistStack.address[gDlistStackPointer];
	u32 * pCmdBase = (u32 *)(g_pu8RamBase + pc);

	// If Off screen rendering is true then just skip the whole list of tris //Corn
	// Skip shadow as well
	if( g_CI.Format != G_IM_FMT_RGBA || (gRDPOtherMode.L == CONKER_SHADOW) )
	{
		do
		{
			command.inst.cmd0 = *pCmdBase++;
			command.inst.cmd1 = *pCmdBase++;
			pc += 8;
		} while ( command.inst.cmd == G_GBI2_TRI1 );
		gDlistStack.address[gDlistStackPointer] = pc-8;
		return;
	}

	bool tris_added = false;

	do
	{
		//DL_PF("    0x%08x: %08x %08x %-10s", pc-8, command.inst.cmd0, command.inst.cmd1, "G_GBI2_TRI1");

		u32 v0_idx = command.gbi2tri1.v0 >> 1;
		u32 v1_idx = command.gbi2tri1.v1 >> 1;
		u32 v2_idx = command.gbi2tri1.v2 >> 1;

		tris_added |= gRenderer->AddTri(v0_idx, v1_idx, v2_idx);

		command.inst.cmd0 = *pCmdBase++;
		command.inst.cmd1 = *pCmdBase++;
		pc += 8;
	} while ( command.inst.cmd == G_GBI2_TRI1 );

	gDlistStack.address[gDlistStackPointer] = pc-8;

	if (tris_added)
	{
		gRenderer->FlushTris();
	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_Tri2_Conker( MicroCodeCommand command )
{

	u32 pc = gDlistStack.address[gDlistStackPointer];
	u32 * pCmdBase = (u32 *)(g_pu8RamBase + pc);

	// If Off screen rendering is true then just skip the whole list of tris //Corn
	// Skip shadow as well
	if( g_CI.Format != G_IM_FMT_RGBA || (gRDPOtherMode.L == CONKER_SHADOW) )
	{
		do
		{
			command.inst.cmd0 = *pCmdBase++;
			command.inst.cmd1 = *pCmdBase++;
			pc += 8;
		} while ( command.inst.cmd == G_GBI2_TRI2 );
		gDlistStack.address[gDlistStackPointer] = pc-8;
		return;
	}

	bool tris_added = false;

	do
	{
		//DL_PF("    0x%08x: %08x %08x %-10s", pc-8, command.inst.cmd0, command.inst.cmd1, "G_GBI2_TRI2");

		// Vertex indices already divided in ucodedef
		u32 v0_idx = command.gbi2tri2.v0;
		u32 v1_idx = command.gbi2tri2.v1;
		u32 v2_idx = command.gbi2tri2.v2;

		tris_added |= gRenderer->AddTri(v0_idx, v1_idx, v2_idx);

		u32 v3_idx = command.gbi2tri2.v3;
		u32 v4_idx = command.gbi2tri2.v4;
		u32 v5_idx = command.gbi2tri2.v5;

		tris_added |= gRenderer->AddTri(v3_idx, v4_idx, v5_idx);

		command.inst.cmd0 = *pCmdBase++;
		command.inst.cmd1 = *pCmdBase++;
		pc += 8;
	} while ( command.inst.cmd == G_GBI2_TRI2 );

	gDlistStack.address[gDlistStackPointer] = pc-8;

	if (tris_added)
	{
		gRenderer->FlushTris();
	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_Tri4_Conker( MicroCodeCommand command )
{
	u32 pc = gDlistStack.address[gDlistStackPointer];		// This points to the next instruction

	// If Off screen rendering is true then just skip the whole list of tris //Corn
	// Skip shadow as well
	if( g_CI.Format != G_IM_FMT_RGBA || (gRDPOtherMode.L == CONKER_SHADOW) )
	{
		do
		{
			command.inst.cmd0 = *(u32 *)(g_pu8RamBase + pc+0);
			command.inst.cmd1 = *(u32 *)(g_pu8RamBase + pc+4);
			pc += 8;
		} while ((command.inst.cmd0>>28) == 1);
		gDlistStack.address[gDlistStackPointer] = pc-8;
		return;
	}

	bool tris_added = false;

	do
	{
		u32 idx[12];

		//Tri #1
		idx[0] = (command.inst.cmd1   )&0x1F;
		idx[1] = (command.inst.cmd1>> 5)&0x1F;
		idx[2] = (command.inst.cmd1>>10)&0x1F;

		tris_added |= gRenderer->AddTri(idx[0], idx[1], idx[2]);

		//Tri #2
		idx[3] = (command.inst.cmd1>>15)&0x1F;
		idx[4] = (command.inst.cmd1>>20)&0x1F;
		idx[5] = (command.inst.cmd1>>25)&0x1F;

		tris_added |= gRenderer->AddTri(idx[3], idx[4], idx[5]);

		//Tri #3
		idx[6] = (command.inst.cmd0    )&0x1F;
		idx[7] = (command.inst.cmd0>> 5)&0x1F;
		idx[8] = (command.inst.cmd0>>10)&0x1F;

		tris_added |= gRenderer->AddTri(idx[6], idx[7], idx[8]);

		//Tri #4
		idx[ 9] = (((command.inst.cmd0>>15)&0x7)<<2)|(command.inst.cmd1>>30);
		idx[10] = (command.inst.cmd0>>18)&0x1F;
		idx[11] = (command.inst.cmd0>>23)&0x1F;

		tris_added |= gRenderer->AddTri(idx[9], idx[10], idx[11]);

		command.inst.cmd0			= *(u32 *)(g_pu8RamBase + pc+0);
		command.inst.cmd1			= *(u32 *)(g_pu8RamBase + pc+4);
		pc += 8;
	} while ((command.inst.cmd0>>28) == 1);

	gDlistStack.address[gDlistStackPointer] = pc-8;

	if (tris_added)
	{
		gRenderer->FlushTris();
	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_MoveMem_Conker( MicroCodeCommand command )
{
	u32 type = command.inst.cmd0 & 0xFE;
	u32 address = RDPSegAddr(command.inst.cmd1);

	switch ( type )
	{
		case G_GBI2_MV_VIEWPORT:
		{
			RDP_MoveMemViewport( address );
		}
		break;

	case G_GBI2_MV_MATRIX:	//Get address to Light Normals
		{
			gAuxAddr = address;		//Conker VtxZ address
		}
		break;
	case G_GBI2_MV_LIGHT:
		{
			u32 offset2 = (command.inst.cmd0 >> 5) & 0x3FFF;
			u32 light_idx = (offset2 / 48);
			if (light_idx < 2)
			{
				return;
			}

			light_idx -= 2;
			N64Light *light = (N64Light*)(g_pu8RamBase + address);
			RDP_MoveMemLight(light_idx, light);

			gRenderer->SetLightPosition( light_idx, light->x, light->y, light->z , light->w);
			gRenderer->SetLightCBFD( light_idx, light->nonzero);
		}
		break;
	default:
		break;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_MoveWord_Conker( MicroCodeCommand command )
{
	u8 index = (u8)(( command.inst.cmd0 >> 16) & 0xFF);
	switch (index)
	{
	case G_MW_NUMLIGHT:
		{
			u32 num_lights = command.inst.cmd1 / 48;
				#ifdef DAEDALUS_DEBUG_DISPLAYLIST
			DL_PF("    G_MW_NUMLIGHT: %d", num_lights);
			#endif
			gRenderer->SetNumLights(num_lights);
		}
		break;

	case G_MW_SEGMENT:
		{
			u16 offset = (u16)( command.inst.cmd0 & 0xFFFF);
			u32 segment = (offset >> 2) & 0xF;
			#ifdef DAEDALUS_DEBUG_DISPLAYLIST
			DL_PF( "    G_MW_SEGMENT Segment[%d] = 0x%08x", segment, command.inst.cmd1 );
			#endif
			gSegments[segment] = command.inst.cmd1 & 0x00FFFFFF;
		}
		break;

	case 0x10:  // moveword coord mod
		{
			if ( (command.inst.cmd0 & 8) == 0 )
			{
				u32 idx = (command.inst.cmd0 >> 1) & 3;
				u32 pos = command.inst.cmd0 & 0x30;

				switch(pos)
				{
				case 0:
					gRenderer->SetCoordMod( 0+idx, (s16)(command.inst.cmd1 >> 16) );
					gRenderer->SetCoordMod( 1+idx, (s16)(command.inst.cmd1 & 0xFFFF) );
					break;
				case 0x10:
					gRenderer->SetCoordMod( 4+idx, (command.inst.cmd1 >> 16) / 65536.0f );
					gRenderer->SetCoordMod( 5+idx, (command.inst.cmd1 & 0xFFFF) / 65536.0f );
					gRenderer->SetCoordMod( 12+idx, gRenderer->GetCoordMod(0+idx) + gRenderer->GetCoordMod(4+idx) );
					gRenderer->SetCoordMod( 13+idx, gRenderer->GetCoordMod(1+idx) + gRenderer->GetCoordMod(5+idx) );
					break;
				case 0x20:
					gRenderer->SetCoordMod( 8+idx, (s16)(command.inst.cmd1 >> 16) );
					gRenderer->SetCoordMod( 9+idx, (s16)(command.inst.cmd1 & 0xFFFF) );
					break;
				}
			}
		}
		break;
	default:
		break;
	}
}



#endif // HLEGRAPHICS_UCODES_UCODE_CONKER_H_
