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

#ifndef HLEGRAPHICS_UCODES_UCODE_GBI1_H_
#define HLEGRAPHICS_UCODES_UCODE_GBI1_H_


//*****************************************************************************
// The previous way of calculating was based on the assumption that
// there was no "n" field. I didn't realise that the n/length fields shared the
// lower 16 bits (in a 6:10 split).
// u32 length    = (command.inst.cmd0)&0xFFFF;
// u32 num_verts = (length + 1) / 0x210;                        // 528
// u32 v0_idx    = ((command.inst.cmd0>>16)&0xFF)/gVertexStride;      // /5
//*****************************************************************************
void DLParser_GBI1_Vtx( MicroCodeCommand command )
{
	//u32 length    = (command.inst.cmd0)&0xFFFF;
	//u32 num_verts = (length + 1) / 0x410;
	//u32 v0_idx    = ((command.inst.cmd0>>16)&0x3f)/2;

	u32 addr = RDPSegAddr(command.vtx1.addr);
	u32 v0   = command.vtx1.v0;
	u32 n    = command.vtx1.n;

	// Wetrix
	if ( addr > MAX_RAM_ADDRESS )
	{
		return;
	}

	gRenderer->SetNewVertexInfo( addr, v0, n );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_ModifyVtx( MicroCodeCommand command )
{
	u32 offset = command.modifyvtx.offset;
	u32 vert   = command.modifyvtx.vtx;
	u32 value  = command.modifyvtx.value;

	// Cures crash after swinging in Mario Golf
	if( vert > 80 )
	{
		return;
	}

	gRenderer->ModifyVertexInfo( offset, vert, value );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_Mtx( MicroCodeCommand command )
{
	u32 address = RDPSegAddr(command.mtx1.addr);

	// Load matrix from address
	if (command.mtx1.projection)
	{
		gRenderer->SetProjection(address, command.mtx1.load);
	}
	else
	{
		gRenderer->SetWorldView(address, command.mtx1.push, command.mtx1.load);
	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_PopMtx( MicroCodeCommand command )
{
	// Do any of the other bits do anything?
	// So far only Extreme-G seems to Push/Pop projection matrices
	// Can't pop projection matrix
	if(command.inst.cmd1 == 0)
		gRenderer->PopWorldView();
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_MoveMem( MicroCodeCommand command )
{
	u32 type     = (command.inst.cmd0>>16)&0xFF;
	u32 address  = RDPSegAddr(command.inst.cmd1);

	switch( type )
	{
		case G_MV_VIEWPORT:
			{
				RDP_MoveMemViewport( address );
			}
			break;
		case G_MV_L0:
		case G_MV_L1:
		case G_MV_L2:
		case G_MV_L3:
		case G_MV_L4:
		case G_MV_L5:
		case G_MV_L6:
		case G_MV_L7:
			{
				u32 light_idx = (type-G_MV_L0) >> 1;
				N64Light *light = (N64Light*)(g_pu8RamBase + address);
				RDP_MoveMemLight(light_idx, light);
			}
			break;

		case G_MV_MATRIX_1:
			{
				// Rayman 2, Donald Duck, Tarzan, all wrestling games use this
				gRenderer->ForceMatrix( address );
				// ForceMatrix takes four cmds
				gDlistStack.address[gDlistStackPointer] += 24;
			}
			break;
		default:
			break;

	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_MoveWord( MicroCodeCommand command )
{
	static f32 old_fog_mult;
	static f32 old_fog_offs;
	
	// Type of movement is in low 8bits of cmd0.
	u32 value  = command.mw1.value;
	u32 offset = command.mw1.offset;

	switch ( command.mw1.type )
	{
	case G_MW_MATRIX:
		{
			gRenderer->InsertMatrix(command.inst.cmd0, command.inst.cmd1);
		}
		break;
	case G_MW_NUMLIGHT:
		{
			u32 num_lights = ((value - 0x80000000) >> 5) - 1;
			gRenderer->SetNumLights(num_lights);

		}
		break;
	case G_MW_SEGMENT:
		{
			u32 segment = (offset >> 2) & 0xF;
			gSegments[segment] = value & 0x00FFFFFF;
		}
		break;

	case G_MW_FOG:	// WIP, only works for the PSP
		{
			f32 mul = (f32)(s16)(value >> 16);	//Fog mult
			f32 offs = (f32)(s16)(value & 0xFFFF);	//Fog Offset
			if ((old_fog_mult != mul) || (old_fog_offs != offs)) {
				old_fog_mult = mul;
				old_fog_offs = offs;
#ifndef DAEDALUS_VITA
				gRenderer->SetFogMultOffs(mul, offs);
#else
				f32 rng = 128000.0f / mul;
			
				f32 fog_near = 500 - (offs * rng / 256.0f);
				f32 fog_far = rng + fog_near;
				gRenderer->SetFogMinMax(fog_near, fog_far);
#endif
			}
		}
		break;
	case G_MW_LIGHTCOL:
		{
			u32 field_offset = (offset & 0x7);
			u32 light_idx = offset >> 5;
			if (field_offset == 0)
			{
				// Light col
				u8 r = ((value>>24)&0xFF);
				u8 g = ((value>>16)&0xFF);
				u8 b = ((value>>8)&0xFF);
				u8 a = 255;
				gRenderer->SetLightCol(light_idx, r, g, b);
			}
		}
		break;
	case G_MW_POINTS:	// Used in FIFA 98
		{
			gRenderer->ModifyVertexInfo( (offset % 40), (offset / 40), value);
		}
		break;
	default:
		break;

	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_CullDL( MicroCodeCommand command )
{
	u32 first = command.culldl.first;
	u32 last = command.culldl.end;
	if( gRenderer->TestVerts( first, last ) )
	{
		return;
	}

	DLParser_PopDL();
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_DL( MicroCodeCommand command )
{
	if( command.dlist.param == G_DL_PUSH )
		gDlistStackPointer++;

	// Compiler gives much better asm if RDPSegAddr.. is sticked directly here
	gDlistStack.address[gDlistStackPointer] = RDPSegAddr(command.dlist.addr);
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_EndDL( MicroCodeCommand command )
{
	DLParser_PopDL();
}

//*****************************************************************************
// When the depth is less than the z value provided, branch to given address
//*****************************************************************************
void DLParser_GBI1_BranchZ( MicroCodeCommand command )
{
	if (g_ROM.ZELDA_HACK) { // Zelda games do W checking instead of Z
		if (gRenderer->GetVtxWeight(command.branchw.vtx) < (float)command.branchw.value) {
			u32 address = RDPSegAddr(gRDPHalf1);
			gDlistStack.address[gDlistStackPointer] = address;
		}
	} else {
		u32 zTest = u32((gRenderer->GetVtxDepth(command.branchz.vtx) / gRenderer->GetVtxWeight(command.branchz.vtx)) * 1023.0f);
		if (zTest > 0x3FF || zTest <= (u32)command.branchz.value) {
			u32 address = RDPSegAddr(gRDPHalf1);
			gDlistStack.address[gDlistStackPointer] = address;
		}
	}
}

//*****************************************************************************
// AST, Yoshi's World, Scooby Doo use this
//*****************************************************************************
void DLParser_GBI1_LoadUCode( MicroCodeCommand command )
{
	u32 code_base = (command.inst.cmd1 & 0x1fffffff);
	u32 code_size = 0x1000;
	u32 data_base = gRDPHalf1 & 0x1fffffff;         // Preceeding RDP_HALF1 sets this up
	u32 data_size = (command.inst.cmd0 & 0xFFFF) + 1;

	DLParser_InitMicrocode( code_base, code_size, data_base, data_size );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_GeometryMode( MicroCodeCommand command )
{
	const u32 mask = command.inst.cmd1;

	if(command.inst.cmd & 1)
	{
		gGeometryMode._u32 |= mask;
	}
	else
	{
		gGeometryMode._u32 &= ~mask;
	}

	TnLMode TnL;
	TnL._u32 = 0;
	
	TnL.Light		= gGeometryMode.GBI1_Lighting;
	TnL.TexGen		= gGeometryMode.GBI1_TexGen;
	TnL.TexGenLin   = gGeometryMode.GBI1_TexGenLin;
#ifdef DAEDALUS_VITA
	TnL.Fog			= gGeometryMode.GBI2_Fog;
#else
	TnL.Fog			= gGeometryMode.GBI2_Fog & gFogEnabled;// && (gRDPOtherMode.c1_m1a==3 || gRDPOtherMode.c1_m2a==3 || gRDPOtherMode.c2_m1a==3 || gRDPOtherMode.c2_m2a==3);
#endif
	TnL.Shade		= gGeometryMode.GBI1_Shade/* & gGeometryMode.GBI1_ShadingSmooth*/;
	TnL.Zbuffer		= gGeometryMode.GBI1_Zbuffer;

	// CULL_BACK has priority, Fixes Mortal Kombat 4
	TnL.TriCull		= gGeometryMode.GBI1_CullFront | gGeometryMode.GBI1_CullBack;
	TnL.CullBack	= gGeometryMode.GBI1_CullBack;

	gRenderer->SetTnLMode( TnL._u32 );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_SetOtherModeL( MicroCodeCommand command )
{
	const u32 mask = ((1 << command.othermode.len) - 1) << command.othermode.sft;

	gRDPOtherMode.L = (gRDPOtherMode.L & ~mask) | command.othermode.data;
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_SetOtherModeH( MicroCodeCommand command )
{
	const u32 mask = ((1 << command.othermode.len) - 1) << command.othermode.sft;

	gRDPOtherMode.H = (gRDPOtherMode.H & ~mask) | command.othermode.data;
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_Texture( MicroCodeCommand command )
{
	bool enabled = command.texture.enable_gbi0;
	gRenderer->SetTextureEnable(enabled);
	
	if (!enabled) return;
	
	gRenderer->SetTextureTile( command.texture.tile);
	
	f32 scale_s = f32(command.texture.scaleS)  / (65536.0f * 32.0f);
	f32 scale_t = f32(command.texture.scaleT)  / (65536.0f * 32.0f);

	gRenderer->SetTextureScale( scale_s, scale_t );

}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_Reserved( MicroCodeCommand command )
{
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_Noop( MicroCodeCommand command )
{
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_SpNoop( MicroCodeCommand command )
{
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_RDPHalf_Cont( MicroCodeCommand command )
{
	//DBGConsole_Msg( 0, "Unexpected RDPHalf_Cont: %08x %08x", command.inst.cmd0, command.inst.cmd1 );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_RDPHalf_2( MicroCodeCommand command )
{
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_RDPHalf_1( MicroCodeCommand command )
{
	gRDPHalf1 = command.inst.cmd1;
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_Tri2( MicroCodeCommand command )
{
	DLParser_GBI1_Tri2_T< 2 >(command);
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_Line3D( MicroCodeCommand command )
{
	DLParser_GBI1_Line3D_T< 2 >(command);
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_Tri1( MicroCodeCommand command )
{
	DLParser_GBI1_Tri1_T< 2 >(command);
}

//*****************************************************************************
// These are used to avoid duplicate code for microcodes with a different vertex stride ex 10 for GBI0 and 2 for GBI1
// Also to optimize the vertex indices at compile time
//*****************************************************************************
template< u32 VertexStride > 
void DLParser_GBI1_Tri1_T( MicroCodeCommand command )
{
	//DAEDALUS_PROFILE( "DLParser_GBI1_Tri1_T" );

	// While the next command pair is Tri1, add vertices
	u32 pc	= gDlistStack.address[gDlistStackPointer];
	u32 * pCmdBase = (u32 *)( g_pu8RamBase + pc );

	bool tris_added = false;

	do
	{
		//DL_PF("    0x%08x: %08x %08x %-10s", pc-8, command.inst.cmd0, command.inst.cmd1, "G_GBI1_TRI1");
		u32 v0_idx = command.gbi1tri1.v0 / VertexStride;
		u32 v1_idx = command.gbi1tri1.v1 / VertexStride;
		u32 v2_idx = command.gbi1tri1.v2 / VertexStride;

		tris_added |= gRenderer->AddTri(v0_idx, v1_idx, v2_idx);

		command.inst.cmd0= *pCmdBase++;
		command.inst.cmd1= *pCmdBase++;
		pc += 8;
	} while ( command.inst.cmd == G_GBI1_TRI1 );

	gDlistStack.address[gDlistStackPointer] = pc-8;

	if (tris_added)
	{
		gRenderer->FlushTris();
	}
}

//*****************************************************************************
//
//*****************************************************************************
template< u32 VertexStride > 
void DLParser_GBI1_Tri2_T( MicroCodeCommand command )
{
	// While the next command pair is Tri2, add vertices
	u32 pc = gDlistStack.address[gDlistStackPointer];
	u32 * pCmdBase = (u32 *)(g_pu8RamBase + pc);

	bool tris_added = false;

	do
	{
		//DL_PF("    0x%08x: %08x %08x %-10s", pc-8, command.inst.cmd0, command.inst.cmd1, "G_GBI1_TRI2");

		// Vertex indices are multiplied by 10 for GBI0, by 2 for GBI1
		u32 v0_idx = command.gbi1tri2.v0 >> 1;
		u32 v1_idx = command.gbi1tri2.v1 >> 1;
		u32 v2_idx = command.gbi1tri2.v2 >> 1;

		tris_added |= gRenderer->AddTri(v0_idx, v1_idx, v2_idx);

		u32 v3_idx = command.gbi1tri2.v3 >> 1;
		u32 v4_idx = command.gbi1tri2.v4 >> 1;
		u32 v5_idx = command.gbi1tri2.v5 >> 1;

		tris_added |= gRenderer->AddTri(v3_idx, v4_idx, v5_idx);

		command.inst.cmd0= *pCmdBase++;
		command.inst.cmd1= *pCmdBase++;
		pc += 8;
	} while ( command.inst.cmd == G_GBI1_TRI2 );

	gDlistStack.address[gDlistStackPointer] = pc-8;

	if (tris_added)
	{
		gRenderer->FlushTris();
	}
}

template< u32 VertexStride > 
void DLParser_GBI1_Line3D_T( MicroCodeCommand command )
{
	//DAEDALUS_PROFILE( "DLParser_GBI1_Line3D_T" );

	// While the next command pair is Tri1, add vertices
	u32 pc	= gDlistStack.address[gDlistStackPointer];
	u32 * pCmdBase = (u32 *)( g_pu8RamBase + pc );

	bool tris_added = false;

	do
	{
		//DL_PF("    0x%08x: %08x %08x %-10s", pc-8, command.inst.cmd0, command.inst.cmd1, "G_GBI1_LINE3D");
		u32 v0_idx   = command.gbi1line3d.v0 / VertexStride;
		u32 v1_idx   = command.gbi1line3d.v1 / VertexStride;
		u32 v2_idx   = command.gbi1line3d.v2 / VertexStride;
		u32 v3_idx   = command.gbi1line3d.v3 / VertexStride;

		tris_added |= gRenderer->AddTri(v0_idx, v1_idx, v2_idx);
		tris_added |= gRenderer->AddTri(v2_idx, v3_idx, v0_idx);

		command.inst.cmd0 = *pCmdBase++;
		command.inst.cmd1 = *pCmdBase++;
		pc += 8;
	} while ( command.inst.cmd == G_GBI1_LINE3D );

	gDlistStack.address[gDlistStackPointer] = pc-8;

	if (tris_added)
	{
		gRenderer->FlushTris();
	}
}

#endif // HLEGRAPHICS_UCODES_UCODE_GBI1_H_
