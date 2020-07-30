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

#ifndef HLEGRAPHICS_UCODES_UCODE_GBI2_H_
#define HLEGRAPHICS_UCODES_UCODE_GBI2_H_

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_Vtx( MicroCodeCommand command )
{
	u32 address = RDPSegAddr(command.vtx2.addr);

	u32 vend   = command.vtx2.vend >> 1;
	u32 n      = command.vtx2.n;
	u32 v0	   = vend - n;

	if ( vend > 64 )
	{
		return;
	}

	// Check that address is valid...
	// Only games I seen that set this are Mario Golf/Tennis, but it looks like is caused by a dynarec issue, anyways they crash eventually
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( (address + (n*16) ) < MAX_RAM_ADDRESS, "Address out of range (0x%08x)", address );
	#endif

	gRenderer->SetNewVertexInfo( address, v0, n );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_Vtx_AM( MicroCodeCommand command )
{
	u32 address = RDPSegAddr(command.vtx2.addr);

	u32 vend   = command.vtx2.vend >> 1;
	u32 n      = command.vtx2.n;
	u32 v0	   = vend - n;

	if ( vend > 64 )
	{
		return;
	}

	gRenderer->SetNewVertexInfoDAM( address, v0, n );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_Mtx( MicroCodeCommand command )
{
	u32 address = RDPSegAddr(command.mtx2.addr);

	// Load matrix from address
	if (command.mtx2.projection)
	{
		gRenderer->SetProjection(address, command.mtx2.load);
	}
	else
	{
		gRenderer->SetWorldView(address, command.mtx2.nopush==0, command.mtx2.load);
	}
}
//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_PopMtx( MicroCodeCommand command )
{
	#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DL_PF("    Command: (%s)",	command.inst.cmd1 ? "Projection" : "ModelView");
	#endif
	// Banjo Tooie, pops more than one matrix
	u32 num = command.inst.cmd1>>6;

	// Just pop the worldview matrix
	gRenderer->PopWorldView(num);
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_MoveWord_AM( MicroCodeCommand command )
{
	static f32 old_fog_mult;
	static f32 old_fog_offs;
	
	u32 value  = command.mw2.value;
	u32 offset = command.mw2.offset;
	
	switch (command.mw2.type)
	{
	case G_MW_MATRIX:
		{
			gRenderer->InsertMatrix(command.inst.cmd0, command.inst.cmd1);
		}
		break;

	case G_MW_NUMLIGHT:
		{
			u32 num_lights = value / 24;
			gRenderer->SetNumLights(num_lights);
		}
		break;
	case G_MW_SEGMENT:
		{
			u32 segment = offset >> 2;
			u32 address	= value;
			gSegments[segment] = address & 0x00FFFFFF;
		}
		break;
	case G_MW_FOG:
		{
			switch (offset) {
			case 0x00:
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
			case 0x0C:
				gRenderer->SetTextureScaleX(value);
				break;
			case 0x10:
				gRenderer->SetTextureScaleY(value);
				break;
			default:
				break;
			}
		}
		break;

	case G_MW_LIGHTCOL:
		{
			u32 light_idx = offset / 0x18;
			u32 field_offset = (offset & 0x7);
			if (field_offset == 0)
			{
				u8 r = ((value>>24)&0xFF);
				u8 g = ((value>>16)&0xFF);
				u8 b = ((value>>8)&0xFF);
				gRenderer->SetLightCol(light_idx, r, g, b);
			}
		}
		break;
	default:
		break;
	}
}

void DLParser_GBI2_MoveWord( MicroCodeCommand command )
{
	static f32 old_fog_mult;
	static f32 old_fog_offs;
	
	u32 value  = command.mw2.value;
	u32 offset = command.mw2.offset;
	
	switch (command.mw2.type)
	{
	case G_MW_MATRIX:
		{
			gRenderer->InsertMatrix(command.inst.cmd0, command.inst.cmd1);
		}
		break;

	case G_MW_NUMLIGHT:
		{
			u32 num_lights = value / 24;
			gRenderer->SetNumLights(num_lights);
		}
		break;
	case G_MW_SEGMENT:
		{
			u32 segment = offset >> 2;
			u32 address	= value;
			gSegments[segment] = address;
		}
		break;
	case G_MW_FOG:
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
			u32 light_idx = offset / 0x18;
			u32 field_offset = (offset & 0x7);
			if (field_offset == 0)
			{
				u8 r = ((value>>24)&0xFF);
				u8 g = ((value>>16)&0xFF);
				u8 b = ((value>>8)&0xFF);
				gRenderer->SetLightCol(light_idx, r, g, b);
			}
		}
		break;
	default:
		break;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_MoveMem( MicroCodeCommand command )
{
	u32 address	 = RDPSegAddr(command.inst.cmd1);
	u32 type	 = (command.inst.cmd0     ) & 0xFE;

	switch (type)
	{
	case G_GBI2_MV_VIEWPORT:
		{
			RDP_MoveMemViewport( address );
		}
		break;

	case G_GBI2_MV_LIGHT:
		{
			u32 offset = (command.inst.cmd0 >> 5) & 0x7F8;
			u32 light_idx = offset / 24;
			if (light_idx < 2)
			{
				return;
			}

			light_idx -= 2;
			N64Light *light = (N64Light*)(g_pu8RamBase + address);
			RDP_MoveMemLight(light_idx, light);

			gRenderer->SetLightPosition(light_idx, light->x1, light->y1, light->z1, 1.0f);
			gRenderer->SetLightEx(light_idx, light->ca, light->la, light->qa);
		}
		break;

	case G_GBI2_MV_MATRIX:
		{
			// Rayman 2, Donald Duck, Tarzan, all wrestling games use this
			gRenderer->ForceMatrix( address );
			// ForceMatrix takes two cmds
			gDlistStack.address[gDlistStackPointer] += 8;
		}
		break;
	case 0x00:
	case 0x02:
		{
			// Ucode for Evangelion.v64
			// 0 ObjMtx
			// 2 SubMtx
			DLParser_S2DEX_ObjMoveMem( command );
		}
		break;

	default:
		break;
	}
}

void DLParser_MoveMem_Acclaim( MicroCodeCommand command )
{
	u32 address	 = RDPSegAddr(command.inst.cmd1);
	u32 type = (command.inst.cmd0) & 0xFF;
	switch (type)
	{
	case G_GBI2_MV_VIEWPORT:
		{
			RDP_MoveMemViewport( address );
		}
		break;
	case G_GBI2_MV_LIGHT:
		{
			u32 offset = (command.inst.cmd0 >> 5) & 0x7F8;
			if (offset <= 24 * 3) {
				u32 light_idx = offset / 24;
				if (light_idx < 2) {
				} else {
					light_idx -= 2;
					N64Light *light = (N64Light*)(g_pu8RamBase + address);
					RDP_MoveMemLight(light_idx, light);

					gRenderer->SetLightPosition(light_idx, light->x1, light->y1, light->z1, 1.0f);
					gRenderer->SetLightEx(light_idx, light->ca, light->la, light->qa);
				}
			} else {
				u32 light_idx = 2 + (offset - 24 * 4) / 16;
				if (light_idx < 10) {
					N64LightAcclaim *light = (N64LightAcclaim*)(g_pu8RamBase + address);
					
					gRenderer->SetLightCol( light_idx, light->r, light->g, light->b );
					gRenderer->SetLightPosition(light_idx, light->x, light->y, light->z, 1.0f);
					gRenderer->SetLightEx(light_idx, light->ca, light->la, light->qa);
				}
			}
		}
		break;
	case G_GBI2_MV_MATRIX:
		{
			gRenderer->ForceMatrix( address );
			// ForceMatrix takes two cmds
			gDlistStack.address[gDlistStackPointer] += 8;
		}
		break;
	default:
		break;
	}
}


//*****************************************************************************
// Kirby 64, SSB and Cruisn' Exotica use this
//*****************************************************************************
void DLParser_GBI2_DL_Count( MicroCodeCommand command )
{
	u32 address  = RDPSegAddr(command.inst.cmd1);

	// For SSB and Kirby, otherwise we'll end up scrapping the pc
	if (address == 0)
	{
		return;
	}

	gDlistStackPointer++;
	gDlistStack.address[gDlistStackPointer] = address;
	gDlistStack.limit = command.inst.cmd0 & 0xFFFF;
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_GeometryMode( MicroCodeCommand command )
{
	gGeometryMode._u32 &= command.inst.arg0;
	gGeometryMode._u32 |= command.inst.arg1;
	TnLMode TnL;
	TnL._u32 = 0;
	
	TnL.Light		= gGeometryMode.GBI2_Lighting;
	TnL.TexGen		= gGeometryMode.GBI2_TexGen;
	TnL.TexGenLin	= gGeometryMode.GBI2_TexGenLin;
#ifdef DAEDALUS_VITA
	TnL.Fog			= gGeometryMode.GBI2_Fog;
#else
	TnL.Fog			= gGeometryMode.GBI2_Fog & gFogEnabled;// && (gRDPOtherMode.c1_m1a==3 || gRDPOtherMode.c1_m2a==3 || gRDPOtherMode.c2_m1a==3 || gRDPOtherMode.c2_m2a==3);
#endif
	TnL.Shade		= !(gGeometryMode.GBI2_TexGenLin/* & (g_ROM.GameHacks != TIGERS_HONEY_HUNT)*/);
	TnL.Zbuffer		= gGeometryMode.GBI2_Zbuffer;
	TnL.TriCull		= gGeometryMode.GBI2_CullFront | gGeometryMode.GBI2_CullBack;
	TnL.CullBack	= gGeometryMode.GBI2_CullBack;
	TnL.PointLight	= gGeometryMode.GBI2_PointLight;

	gRenderer->SetTnLMode( TnL._u32 );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_SetOtherModeL( MicroCodeCommand command )
{
	// Mask is constructed slightly differently
	const u32 mask = (u32)((s32)(0x80000000) >> command.othermode.len) >> command.othermode.sft;

	gRDPOtherMode.L = (gRDPOtherMode.L & ~mask) | command.othermode.data;
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_SetOtherModeH( MicroCodeCommand command )
{
	// Mask is constructed slightly differently
	const u32 mask = (u32)((s32)(0x80000000) >> command.othermode.len) >> command.othermode.sft;

	gRDPOtherMode.H = (gRDPOtherMode.H & ~mask) | command.othermode.data;
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_Texture( MicroCodeCommand command )
{
	bool enabled = command.texture.enable_gbi2;
	gRenderer->SetTextureEnable(enabled);
	
	if (!enabled) return;
	
	gRenderer->SetTextureTile( command.texture.tile );

	f32 scale_s = f32(command.texture.scaleS) / (65536.0f * 32.0f);
	f32 scale_t = f32(command.texture.scaleT)  / (65536.0f * 32.0f);

	gRenderer->SetTextureScale( scale_s, scale_t );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_Texture_AM( MicroCodeCommand command )
{
	gRenderer->SetTextureTile( command.texture.tile );
	gRenderer->SetTextureEnable( command.texture.enable_gbi2 );

	f32 scale_s = 1.0f / (65535.0f * 32.0f);
	f32 scale_t = 1.0f / (65535.0f * 32.0f);
	
	gRenderer->SetTextureScale( scale_s, scale_t );
	gRenderer->SetTextureScaleDAM(command.inst.cmd1);
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_DMA_IO( MicroCodeCommand command )
{
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_Quad( MicroCodeCommand command )
{
	// While the next command pair is Tri2, add vertices
	u32 pc = gDlistStack.address[gDlistStackPointer];
	u32 * pCmdBase = (u32 *)(g_pu8RamBase + pc);

	bool tris_added = false;

	do
	{
		//DL_PF("    0x%08x: %08x %08x %-10s", pc-8, command.inst.cmd0, command.inst.cmd1, "G_GBI2_QUAD");

		// Vertex indices are multiplied by 2
		u32 v0_idx = command.gbi2line3d.v0 >> 1;
		u32 v1_idx = command.gbi2line3d.v1 >> 1;
		u32 v2_idx = command.gbi2line3d.v2 >> 1;

		tris_added |= gRenderer->AddTri(v0_idx, v1_idx, v2_idx);

		u32 v3_idx = command.gbi2line3d.v3 >> 1;
		u32 v4_idx = command.gbi2line3d.v4 >> 1;
		u32 v5_idx = command.gbi2line3d.v5 >> 1;

		tris_added |= gRenderer->AddTri(v3_idx, v4_idx, v5_idx);

		//printf("Q 0x%08x: %08x %08x %d\n", pc-8, command.inst.cmd0, command.inst.cmd1, tris_added);

		command.inst.cmd0 = *pCmdBase++;
		command.inst.cmd1 = *pCmdBase++;
		pc += 8;
	} while ( command.inst.cmd == G_GBI2_QUAD );

	gDlistStack.address[gDlistStackPointer] = pc-8;

	if (tris_added)
	{
		gRenderer->FlushTris();
	}
}

//*****************************************************************************
//
//*****************************************************************************
// XXX SpiderMan uses this command.DLParser_GBI2_Tri2
void DLParser_GBI2_Line3D( MicroCodeCommand command )
{
	// While the next command pair is Tri2, add vertices
	u32 pc = gDlistStack.address[gDlistStackPointer];
	u32 * pCmdBase = (u32 *)(g_pu8RamBase + pc);

	bool tris_added = false;

	do
	{
		//DL_PF("    0x%08x: %08x %08x %-10s", pc-8, command.inst.cmd0, command.inst.cmd1, "G_GBI2_LINE3D");

		u32 v0_idx = command.gbi2line3d.v0 >> 1;
		u32 v1_idx = command.gbi2line3d.v1 >> 1;
		u32 v2_idx = command.gbi2line3d.v2 >> 1;

		tris_added |= gRenderer->AddTri(v0_idx, v1_idx, v2_idx);

		u32 v3_idx = command.gbi2line3d.v3 >> 1;
		u32 v4_idx = command.gbi2line3d.v4 >> 1;
		u32 v5_idx = command.gbi2line3d.v5 >> 1;

		tris_added |= gRenderer->AddTri(v3_idx, v4_idx, v5_idx);

		command.inst.cmd0 = *pCmdBase++;
		command.inst.cmd1 = *pCmdBase++;
		pc += 8;
	} while ( command.inst.cmd == G_GBI2_LINE3D );

	gDlistStack.address[gDlistStackPointer] = pc-8;

	if (tris_added)
	{
		gRenderer->FlushTris();
	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI2_Tri1( MicroCodeCommand command )
{

	// While the next command pair is Tri1, add vertices
	u32 pc = gDlistStack.address[gDlistStackPointer];
	u32 * pCmdBase = (u32 *)(g_pu8RamBase + pc);

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
// While the next command pair is Tri2, add vertices
//*****************************************************************************
void DLParser_GBI2_Tri2( MicroCodeCommand command )
{

	u32 pc = gDlistStack.address[gDlistStackPointer];
	u32 * pCmdBase = (u32 *)(g_pu8RamBase + pc);

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
/*
void DLParser_GBI2_0x8( MicroCodeCommand command )
{
	if( (command.inst.arg0 == 0x2F && ((command.inst.cmd1)&0xFF000000) == 0x80000000 )
	{
		// V-Rally 64
		DLParser_S2DEX_ObjLdtxRectR(command);
	}
	else
	{
		DLParser_Nothing(command);
	}
}
*/

#endif // HLEGRAPHICS_UCODES_UCODE_GBI2_H_
