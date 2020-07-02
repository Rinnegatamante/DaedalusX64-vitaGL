/*
Copyright (C) 2001 StrmnNrmn

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
#include "DLParser.h"

#include "DLDebug.h"
#include "BaseRenderer.h"
#include "N64PixelFormat.h"
#include "Graphics/NativePixelFormat.h"
#include "RDP.h"
#include "RDPStateManager.h"
#include "TextureCache.h"
#include "ConvertImage.h"			// Convert555ToRGBA
#include "Microcode.h"
#include "uCodes/UcodeDefs.h"
#include "uCodes/Ucode.h"

#include "Config/ConfigOptions.h"
#include "Core/CPU.h"
#include "Core/Memory.h"
#include "Core/ROM.h"
#include "Debug/DBGConsole.h"
#include "Debug/Dump.h"
#include "Graphics/GraphicsContext.h"
#include "Math/MathUtil.h"
#include "OSHLE/ultra_gbi.h"
#include "OSHLE/ultra_rcp.h"
#include "OSHLE/ultra_sptask.h"
#include "Plugins/GraphicsPlugin.h"
#include "Test/BatchTest.h"
#include "Utility/IO.h"
#include "Utility/Profiler.h"

//*****************************************************************************
//
//*****************************************************************************
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
#define DL_UNIMPLEMENTED_ERROR( msg )			\
{												\
	static bool shown = false;					\
	if (!shown )								\
	{											\
		DL_PF( "~*Not Implemented %s", msg );	\
		DAEDALUS_DL_ERROR( "%s: %08x %08x", (msg), command.inst.cmd0, command.inst.cmd1 );				\
		shown = true;							\
	}											\
}
#else
#define DL_UNIMPLEMENTED_ERROR( msg )
#endif

#define MAX_DL_STACK_SIZE	32

#define N64COL_GETR( col )		(u8((col) >> 24))
#define N64COL_GETG( col )		(u8((col) >> 16))
#define N64COL_GETB( col )		(u8((col) >>  8))
#define N64COL_GETA( col )		(u8((col)      ))

#define N64COL_GETR_F( col )	(N64COL_GETR(col) * (1.0f/255.0f))
#define N64COL_GETG_F( col )	(N64COL_GETG(col) * (1.0f/255.0f))
#define N64COL_GETB_F( col )	(N64COL_GETB(col) * (1.0f/255.0f))
#define N64COL_GETA_F( col )	(N64COL_GETA(col) * (1.0f/255.0f))

// Mask down to 0x003FFFFF?
#define RDPSegAddr(seg) ( (gSegments[((seg)>>24)&0x0F]&0x00ffffff) + ((seg)&0x00FFFFFF) )

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//                     GFX State                        //
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

v2 aux_trans, aux_scale;
uint32_t aux_discard = 0;
uint32_t aux_draws = 0;

struct N64Viewport
{
    s16 scale_y, scale_x, scale_w, scale_z;
	s16 trans_y, trans_x, trans_w, trans_z;
};

struct N64mat
{
	struct _s16
	{
		s16 y, x, w, z;
	};

	struct _u16
	{
		u16 y, x, w, z;
	};

	_s16 h[4];
	_u16 l[4];
};

struct N64LightAcclaim
{
	s16 y, x;
	u8 g, r;
	s16 z, ca;
	u8 pad0, b;
	u16 qa, la;
};

struct N64Light
{
	u8 ca, b, g, r;					// Colour and ca (ca is different for conker)
	u8 la, b2, g2, r2;
	union
	{
		struct
		{
			s8 pad0, dir_z, dir_y, dir_x;	// Direction
			u8 pad1, qa, pad2, nonzero;
		};
		struct
		{
			s16 y1, x1, w1, z1;		// Position, GBI2 ex Majora's Mask
		};
	};
	s32 pad4, pad5, pad6, pad7;		// Padding..
	s16 y, x, w, z; 				// Position, Conker
};

struct TriDKR
{
    u8	v2, v1, v0, flag;
    s16	t0, s0;
    s16	t1, s1;
    s16	t2, s2;
};

struct RDP_Scissor
{
	u32 left, top, right, bottom;
};

// The display list PC stack. Before this was an array of 10
// items, but this way we can nest as deeply as necessary.
struct DList
{
	u32 address[MAX_DL_STACK_SIZE];
	s32 limit;
};

//*****************************************************************************
//
//*****************************************************************************
void RDP_MoveMemViewport(u32 address);
void MatrixFromN64FixedPoint( Matrix4x4 & mat, u32 address );
void DLParser_InitMicrocode( u32 code_base, u32 code_size, u32 data_base, u32 data_size );
void RDP_MoveMemLight(u32 light_idx, const N64Light *light);

// Used to keep track of when we're processing the first display list
static bool gFirstCall = true;

extern bool gCPURendering;

static u32				gSegments[16];
static RDP_Scissor		scissors;
static RDP_GeometryMode gGeometryMode;
static DList			gDlistStack;
static s32				gDlistStackPointer = -1;
static u32				gVertexStride = 10;
static u32				gRDPHalf1 = 0;

       SImageDescriptor g_TI = { G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 0 };
static SImageDescriptor g_CI = { G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 0 };
static SImageDescriptor g_DI = { G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 0 };

const MicroCodeInstruction *gUcodeFunc = gNormalInstruction[ GBI_0 ];

bool gFrameskipActive = false;

//*****************************************************************************
//
//*****************************************************************************
inline void FinishRDPJob()
{
	Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_DP);
	gCPUState.AddJob(CPU_CHECK_INTERRUPTS);
}

//*****************************************************************************
// Reads the next command from the display list, updates the PC.
//*****************************************************************************
inline void	DLParser_FetchNextCommand( MicroCodeCommand * p_command )
{
	// Current PC is the last value on the stack
	u32 & pc( gDlistStack.address[gDlistStackPointer] );
	*p_command = *(MicroCodeCommand*)(g_pu8RamBase + pc);
	pc+= 8;
}

//*****************************************************************************
//
//*****************************************************************************
inline void DLParser_PopDL()
{
	#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DL_PF("    Returning from DisplayList: level=%d", gDlistStackPointer+1);
	DL_PF("    ############################################");
	DL_PF("    /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\");
	DL_PF(" ");
	#endif
	gDlistStackPointer--;
}

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
//////////////////////////////////////////////////////////
//                      Debug vars                      //
//////////////////////////////////////////////////////////
void DLParser_DumpVtxInfo(u32 address, u32 v0_idx, u32 num_verts);

u32			gNumDListsCulled;
u32			gNumVertices;
u32			gNumRectsClipped;
u32			gNumInstructionsExecuted = 0;
#endif

//*****************************************************************************
//
//*****************************************************************************
u32 gRDPFrame = 0, gAuxAddr = 0;

extern u32 uViWidth, uViHeight;

//*****************************************************************************
// Include ucode header files
//*****************************************************************************
#include "uCodes/Ucode_GBI0.h"
#include "uCodes/Ucode_GBI1.h"
#include "uCodes/Ucode_GBI2.h"
#include "uCodes/Ucode_DKR.h"
#include "uCodes/Ucode_FB.h"
#include "uCodes/Ucode_GE.h"
#include "uCodes/Ucode_PD.h"
#include "uCodes/Ucode_Conker.h"
#include "uCodes/Ucode_LL.h"
#include "uCodes/Ucode_Beta.h"
#include "uCodes/Ucode_Sprite2D.h"
#include "uCodes/Ucode_S2DEX.h"

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//                      Strings                         //
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

static const char * const gFormatNames[8] = {"RGBA", "YUV", "CI", "IA", "I", "?1", "?2", "?3"};

static const char * const gSizeNames[4]   = {"4bpp", "8bpp", "16bpp", "32bpp"};
static const char * const gOnOffNames[2]  = {"Off", "On"};

//*****************************************************************************
//
//*****************************************************************************
bool DLParser_Initialise()
{
	gFirstCall = true;
	
	// Resetting number of executed frames
	gRDPFrame = 0;
	gAuxAddr = 0;
	gRDPHalf1 = 0;
	gCPURendering = true;

	// Reset scissor to default
	scissors.top = 0;
	scissors.left = 0;
	scissors.right = 320;
	scissors.bottom = 240;

	GBIMicrocode_Reset();

	gVertexStride = 10;
	gUcodeFunc = gNormalInstruction[ GBI_0 ];
	
	memset(gTlutLoadAddresses, 0, sizeof(gTlutLoadAddresses));
	
	return true;
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_Finalise() {}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_InitMicrocode( u32 code_base, u32 code_size, u32 data_base, u32 data_size )
{
	const UcodeInfo& ucode_info(GBIMicrocode_DetectVersion(code_base, code_size, data_base, data_size));

	gVertexStride = ucode_info.stride;
	gUcodeFunc = ucode_info.func;
}

//*****************************************************************************
//
//*****************************************************************************
#ifdef DAEDALUS_ENABLE_PROFILING
SProfileItemHandle * gpProfileItemHandles[ 256 ];

#define PROFILE_DL_CMD( cmd )								\
	if(gpProfileItemHandles[ (cmd) ] == nullptr)				\
	{														\
		gpProfileItemHandles[ (cmd) ] = new SProfileItemHandle( CProfiler::Get()->AddItem( gUcodeName[ cmd ] ));		\
	}														\
	CAutoProfile		_auto_profile( *gpProfileItemHandles[ (cmd) ] )

#else

#define PROFILE_DL_CMD( cmd )		do { } while(0)

#endif


//*****************************************************************************
//	Process the entire display list in one go
//*****************************************************************************
static u32 DLParser_ProcessDList(u32 instruction_limit)
{
	MicroCodeCommand command;

	u32 current_instruction_count = 0;

	while(gDlistStackPointer >= 0)
	{
		DLParser_FetchNextCommand( &command );

		DL_BEGIN_INSTR(current_instruction_count, command.inst.cmd0, command.inst.cmd1, gDlistStackPointer, gUcodeName[command.inst.cmd]);

		PROFILE_DL_CMD( command.inst.cmd );

		gUcodeFunc[ command.inst.cmd ]( command );

		DL_END_INSTR();

		// Check limit
		if (gDlistStack.limit >= 0)
		{
			if (--gDlistStack.limit < 0)
			{
				gDlistStackPointer--;
			}
		}
	}

	return current_instruction_count;
}
//*****************************************************************************
//
//*****************************************************************************
u32 DLParser_Process(u32 instruction_limit, DLDebugOutput * debug_output)
{
	if ( !CGraphicsContext::Get()->IsInitialised() || !gRenderer )
	{
		return 0;
	}

	// Shut down the debug console when we start rendering
	// TODO: Clear the front/backbuffer the first time this function is called
	// to remove any stuff lingering on the screen.
	if(gFirstCall)
	{
		CGraphicsContext::Get()->ClearAllSurfaces();
		gFirstCall = false;
		gRDPOtherMode.L = 0;
		gRDPOtherMode.H = 0x0CFF;
	}

	// Update Screen only when something is drawn, otherwise several games ex Army Men will flash or shake.
	if( g_ROM.GameHacks != CHAMELEON_TWIST_2 ) gGraphicsPlugin->UpdateScreen();

	OSTask * pTask = (OSTask *)(g_pu8SpMemBase + 0x0FC0);
	u32 code_base = (u32)pTask->t.ucode & 0x1fffffff;
	u32 code_size = pTask->t.ucode_size;
	u32 data_base = (u32)pTask->t.ucode_data & 0x1fffffff;
	u32 data_size = pTask->t.ucode_data_size;
	u32 stack_size = pTask->t.dram_stack_size >> 6;

	DLParser_InitMicrocode( code_base, code_size, data_base, data_size );

	if (g_ROM.GameHacks != QUAKE) gRDPOtherMode.L = 0;
	if (!g_ROM.KEEP_MODE_H_HACK) gRDPOtherMode.H = 0x0CFF;

	gRDPFrame++;

	CTextureCache::Get()->PurgeOldTextures();

	// Initialise stack
	gDlistStackPointer=0;
	gDlistStack.address[0] = (u32)pTask->t.data_ptr;
	gDlistStack.limit = -1;

	gRDPStateManager.Reset();

	u32 count;

	if(!gFrameskipActive)
	{
		gRenderer->ResetMatrices(stack_size);
		gRenderer->Reset();
		gRenderer->BeginScene();
		count = DLParser_ProcessDList(instruction_limit);
		gRenderer->EndScene();
	}

	// Hack for Chameleon Twist 2, only works if screen is update at last
	//
	if( g_ROM.GameHacks == CHAMELEON_TWIST_2 ) gGraphicsPlugin->UpdateScreen();

	// Do this regardless!
	FinishRDPJob();

	gCPURendering = false;

	return count;
}

//*****************************************************************************
//
//*****************************************************************************
void MatrixFromN64FixedPoint( Matrix4x4 & mat, u32 address )
{
	const f32 fRecip {1.0f / 65536.0f};
	const N64mat *Imat {(N64mat *)( g_pu8RamBase + address )};

	s16 hi;
	s32 tmp;
	for (u32 i = 0; i < 4; i++)
	{
		mat.m[i][0] = ((Imat->h[i].x << 16) | Imat->l[i].x) * fRecip;
		mat.m[i][1] = ((Imat->h[i].y << 16) | Imat->l[i].y) * fRecip;
		mat.m[i][2] = ((Imat->h[i].z << 16) | Imat->l[i].z) * fRecip;
		mat.m[i][3] = ((Imat->h[i].w << 16) | Imat->l[i].w) * fRecip;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void RDP_MoveMemLight(u32 light_idx, const N64Light *light)
{
	u8 r = light->r;
	u8 g = light->g;
	u8 b = light->b;

	s8 dir_x = light->dir_x;
	s8 dir_y = light->dir_y;
	s8 dir_z = light->dir_z;

	bool valid = (dir_x | dir_y | dir_z) != 0;

	//Light color
	gRenderer->SetLightCol( light_idx, r, g, b );

	//Direction
	gRenderer->SetLightDirection( light_idx, dir_x, dir_y, dir_z );
}

//*****************************************************************************
//
//*****************************************************************************
//0x000b46b0: dc080008 800b46a0 G_GBI2_MOVEMEM
//    Type: 08 Len: 08 Off: 0000
//        Scale: 640 480 511 0 = 160,120
//        Trans: 640 480 511 0 = 160,120
//vscale is the scale applied to the normalized homogeneous coordinates after 4x4 projection transformation
//vtrans is the offset added to the scaled number

void RDP_MoveMemViewport(u32 address)
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( address+16 < MAX_RAM_ADDRESS, "MoveMem Viewport, invalid memory" );
	#endif
	// address is offset into RD_RAM of 8 x 16bits of data...
	N64Viewport *vp = (N64Viewport*)(g_pu8RamBase + address);

	// With D3D we had to ensure that the vp coords are positive, so
	// we truncated them to 0. This happens a lot, as things
	// seem to specify the scale as the screen w/2 h/2
	
	//DBGConsole_Msg(0, "MoveMemViewport: trans (%f, %f), scale(%f, %f)", (f32)vp->trans_x, (f32)vp->trans_y, (f32)vp->scale_x, (f32)vp->scale_y);
	
	// Pokemon Stadium gamese use multiple framebuffers, thus causing viewport to have incorrect position.
	// Need proper auxiliary buffers support to fix this in a sane way. For now we hack the most important stuffs.
	if (g_ROM.GameHacks == POKEMON_STADIUM)
	{
		if (vp->scale_x == 200) { // Pokemon Stadium Pokemon Selection
			aux_trans = gRenderer->mVpTrans;
			aux_scale = gRenderer->mVpScale;
			aux_discard = 10;
			aux_draws = 60;
			vp->trans_x = 412.0f;
			vp->trans_y = 1250.0f;
		} else if (vp->scale_x == 112) { // Pokemon Stadium 2 Pokemon Selection
			return;
		}
	}
	
	v2 vec_scale( (f32)vp->scale_x * 0.25f, (f32)vp->scale_y * 0.25f );
	v2 vec_trans( (f32)vp->trans_x * 0.25f, (f32)vp->trans_y * 0.25f );
	gRenderer->SetN64Viewport( vec_scale, vec_trans );
}

//*****************************************************************************
//
//*****************************************************************************
//Nintro64 uses Sprite2d
void DLParser_Nothing( MicroCodeCommand command )
{
	DLParser_PopDL();

}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetKeyGB( MicroCodeCommand command )
{
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetKeyR( MicroCodeCommand command )
{
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetConvert( MicroCodeCommand command )
{
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetPrimDepth( MicroCodeCommand command )
{
	gRenderer->SetPrimitiveDepth( command.primdepth.z );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_RDPSetOtherMode( MicroCodeCommand command )
{
	gRDPOtherMode.H = command.inst.cmd0;
	gRDPOtherMode.L = command.inst.cmd1;
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_RDPLoadSync( MicroCodeCommand command )	{ /*DL_PF("    LoadSync: (Ignored)");*/ }
void DLParser_RDPPipeSync( MicroCodeCommand command )	{ /*DL_PF("    PipeSync: (Ignored)");*/ }
void DLParser_RDPTileSync( MicroCodeCommand command )	{ /*DL_PF("    TileSync: (Ignored)");*/ }

//*****************************************************************************
//
//*****************************************************************************
void DLParser_RDPFullSync( MicroCodeCommand command )
{
	// We now do this regardless
	// This is done after DLIST processing anyway
	//FinishRDPJob();

	/*DL_PF("    FullSync: (Generating Interrupt)");*/
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetScissor( MicroCodeCommand command )
{
	// The coords are all in 10:2 fixed point
	// Set up scissoring zone, we'll use it to scissor other stuff ex Texrect
	//
	scissors.left    = command.scissor.x0>>2;
	scissors.top     = command.scissor.y0>>2;
	scissors.right   = command.scissor.x1>>2;
	scissors.bottom  = command.scissor.y1>>2;

	// Hack to correct Super Bowling's right and left screens
	if ( g_ROM.GameHacks == SUPER_BOWLING && g_CI.Address%0x100 != 0 )
	{
		scissors.left += 160;
		scissors.right += 160;
		v2 vec_trans( 240, 120 );
		v2 vec_scale( 80, 120 );
		gRenderer->SetN64Viewport( vec_scale, vec_trans );
	}

	// Set the cliprect now...
	if ( scissors.left < scissors.right && scissors.top < scissors.bottom )
	{
		gRenderer->SetScissor( scissors.left, scissors.top, scissors.right, scissors.bottom );
	}
}
//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetTile( MicroCodeCommand command )
{
	RDP_Tile tile;
	tile.cmd0 = command.inst.cmd0;
	tile.cmd1 = command.inst.cmd1;

	gRDPStateManager.SetTile( tile );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetTileSize( MicroCodeCommand command )
{
	RDP_TileSize tile;
	tile.cmd0 = command.inst.cmd0;
	tile.cmd1 = command.inst.cmd1;

	gRDPStateManager.SetTileSize( tile );
}


//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetTImg( MicroCodeCommand command )
{
	g_TI.Format		= command.img.fmt;
	g_TI.Size		= command.img.siz;
	g_TI.Width		= command.img.width + 1;
	g_TI.Address	= RDPSegAddr(command.img.addr) & (MAX_RAM_ADDRESS-1);
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_LoadBlock( MicroCodeCommand command )
{
	gRDPStateManager.LoadBlock( command.loadtile );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_LoadTile( MicroCodeCommand command )
{
	gRDPStateManager.LoadTile( command.loadtile );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_LoadTLut( MicroCodeCommand command )
{
	gRDPStateManager.LoadTlut( command.loadtile );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_TexRect( MicroCodeCommand command )
{
	MicroCodeCommand command2;
	MicroCodeCommand command3;

	DLParser_FetchNextCommand( &command2 );
	DLParser_FetchNextCommand( &command3 );

	RDP_TexRect tex_rect;
	tex_rect.cmd0 = command.inst.cmd0;
	tex_rect.cmd1 = command.inst.cmd1;
	tex_rect.cmd2 = command2.inst.cmd1;
	tex_rect.cmd3 = command3.inst.cmd1;

	// NB: In FILL and COPY mode, rectangles are scissored to the nearest four pixel boundary.
	// This isn't currently handled, but I don't know of any games that depend on it.

	//Keep integers for as long as possible //Corn

	// X for upper left corner should be less than X for lower right corner else skip rendering it, seems to happen in Rayman 2 and Star Soldier
	//if( tex_rect.x0 >= tex_rect.x1 )

	// Hack for Banjo Tooie shadow
	if (g_ROM.GameHacks == BANJO_TOOIE && gRDPOtherMode.L == 0x00504241)
	{
		return;
	}

	// Fixes black box in SSB when moving far way from the screen and offscreen in Conker
	if (g_DI.Address == g_CI.Address || g_CI.Format != G_IM_FMT_RGBA)
	{
		return;
	}

	// Removes offscreen texrect, also fixes several glitches like in John Romero's Daikatana
	if( tex_rect.x0 >= (scissors.right<<2) ||
		tex_rect.y0 >= (scissors.bottom<<2) ||
		tex_rect.x1 <  (scissors.left<<2) ||
		tex_rect.y1 <  (scissors.top<<2) )
	{
		return;
	};

	s16 rect_s0 {(s16)tex_rect.s};
	s16 rect_t0 {(s16)tex_rect.t};

	s32 rect_dsdx {tex_rect.dsdx};
	s32 rect_dtdy {tex_rect.dtdy};

	rect_s0 += (((u32)rect_dsdx >> 31) << 5);	//Fixes California Speed, if(rect_dsdx<0) rect_s0 += 32;
	rect_t0 += (((u32)rect_dtdy >> 31) << 5);

	// In Fill/Copy mode the coordinates are inclusive (i.e. add 1<<2 to the w/h)
	u32 cycle_mode {gRDPOtherMode.cycle_type};
	if ( cycle_mode >= CYCLE_COPY )
	{
		// In copy mode 4 pixels are copied at once.
		if ( cycle_mode == CYCLE_COPY )
			rect_dsdx = rect_dsdx >> 2;

		tex_rect.x1 += 4;
		tex_rect.y1 += 4;
	}

	s16 rect_s1 {(s16)(rect_s0 + (rect_dsdx * ( tex_rect.x1 - tex_rect.x0 ) >> 7))};	// 7 = (>>10)=1/1024, (>>2)=1/4 and (<<5)=32
	s16 rect_t1 {(s16)(rect_t0 + (rect_dtdy * ( tex_rect.y1 - tex_rect.y0 ) >> 7))};

	TexCoord st0( rect_s0, rect_t0 );
	TexCoord st1( rect_s1, rect_t1 );

	v2 xy0( tex_rect.x0 / 4.0f, tex_rect.y0 / 4.0f );
	v2 xy1( tex_rect.x1 / 4.0f, tex_rect.y1 / 4.0f );

	gRenderer->TexRect( tex_rect.tile_idx, xy0, xy1, st0, st1 );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_TexRectFlip( MicroCodeCommand command )
{
	MicroCodeCommand command2;
	MicroCodeCommand command3;

	DLParser_FetchNextCommand( &command2 );
	DLParser_FetchNextCommand( &command3 );

	RDP_TexRect tex_rect;
	tex_rect.cmd0 = command.inst.cmd0;
	tex_rect.cmd1 = command.inst.cmd1;
	tex_rect.cmd2 = command2.inst.cmd1;
	tex_rect.cmd3 = command3.inst.cmd1;

	//Keep integers for as long as possible //Corn

	s16 rect_s0 {(s16)tex_rect.s};
	s16 rect_t0 {(s16)tex_rect.t};

	s32 rect_dsdx {tex_rect.dsdx};
	s32 rect_dtdy {tex_rect.dtdy};

	rect_s0 += (((u32)rect_dsdx >> 31) << 5);	// For Wetrix
	rect_t0 += (((u32)rect_dtdy >> 31) << 5);

	// In Fill/Copy mode the coordinates are inclusive (i.e. add 1<<2 to the w/h)
	u32 cycle_mode {gRDPOtherMode.cycle_type};
	if ( cycle_mode >= CYCLE_COPY )
	{
		// In copy mode 4 pixels are copied at once.
		if ( cycle_mode == CYCLE_COPY )
			rect_dsdx = rect_dsdx >> 2;

		tex_rect.x1 += 4;
		tex_rect.y1 += 4;
	}

	s16 rect_s1 {(s16)(rect_s0 + (rect_dsdx * ( tex_rect.y1 - tex_rect.y0 ) >> 7))};	// Flip - use y
	s16 rect_t1 {(s16)(rect_t0 + (rect_dtdy * ( tex_rect.x1 - tex_rect.x0 ) >> 7))};	// Flip - use x

	TexCoord st0( rect_s0, rect_t0 );
	TexCoord st1( rect_s1, rect_t1 );

	v2 xy0( tex_rect.x0 / 4.0f, tex_rect.y0 / 4.0f );
	v2 xy1( tex_rect.x1 / 4.0f, tex_rect.y1 / 4.0f );

	gRenderer->TexRectFlip( tex_rect.tile_idx, xy0, xy1, st0, st1 );
}

//Clear framebuffer, thanks Gonetz! http://www.emutalk.net/threads/15818-How-to-implement-quot-emulate-clear-quot-Answer-and-Question
//This fixes the jumpy camera in DK64, also the sun and flames glare in Zelda
void Clear_N64DepthBuffer( MicroCodeCommand command )
{
	u32 x0 {(u32)(command.fillrect.x0 + 1)};
	u32 x1 {(u32)(command.fillrect.x1 + 1)};
	u32 y1 {command.fillrect.y1};
	u32 y0 {command.fillrect.y0};

	// Using s32 to force min/max to be done in a single op code for the PSP
	x0 = Min<s32>(Max<s32>(x0, scissors.left), scissors.right);
	x1 = Min<s32>(Max<s32>(x1, scissors.left), scissors.right);
	y1 = Min<s32>(Max<s32>(y1, scissors.top), scissors.bottom);
	y0 = Min<s32>(Max<s32>(y0, scissors.top), scissors.bottom);
	x0 >>= 1;
	x1 >>= 1;
	u32 zi_width_in_dwords {g_CI.Width >> 1};
	u32 fill_colour {gRenderer->GetFillColour()};
	u32 * dst {(u32*)(g_pu8RamBase + g_CI.Address) + y0 * zi_width_in_dwords};

	for( u32 y = y0; y <y1; y++ )
	{
		for( u32 x = x0; x < x1; x++ )
		{
			dst[x] = fill_colour;
		}
		dst += zi_width_in_dwords;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_FillRect( MicroCodeCommand command )
{
	//
	// Removes annoying rect that appears in Conker and fillrects that cover screen in banjo tooie
	if( g_CI.Format != G_IM_FMT_RGBA )
	{
		return;
	}

	//Always clear Zbuffer if Depthbuffer is selected //Corn
	if (g_DI.Address == g_CI.Address)
	{
		CGraphicsContext::Get()->ClearZBuffer();

		if(gClearDepthFrameBuffer)
		{
			Clear_N64DepthBuffer(command);
		}
		return;
	}

	// Note, in some modes, the right/bottom lines aren't drawn

	// TODO - Check colour image format to work out how this should be decoded!
	// Should we init with Prim or Blend colour? Doesn't work well see Mk64 transition before a race
	c32		colour {};

	u32 cycle_mode {gRDPOtherMode.cycle_type};
	//
	// In Fill/Copy mode the coordinates are inclusive (i.e. add 1.0f to the w/h)
	//
	if ( cycle_mode >= CYCLE_COPY )
	{
		if ( cycle_mode == CYCLE_FILL )
		{
			u32 fill_colour = gRenderer->GetFillColour();
			if(g_CI.Size == G_IM_SIZ_16b)
			{
				const N64Pf5551	c( (u16)fill_colour );
				colour = ConvertPixelFormat< c32, N64Pf5551 >( c );
			}
			else
			{
				const N64Pf8888	c( (u32)fill_colour );
				colour = ConvertPixelFormat< c32, N64Pf8888 >( c );
			}

			u32 clear_screen_x = command.fillrect.x1 - command.fillrect.x0;
			u32 clear_screen_y = command.fillrect.y1 - command.fillrect.y0;

			// Clear color buffer (screen clear)
			if( uViWidth == clear_screen_x && uViHeight == clear_screen_y )
			{
				CGraphicsContext::Get()->ClearColBuffer( colour );
				return;
			}
		}

		command.fillrect.x1++;
		command.fillrect.y1++;
	}
	//Converting int->float with bitfields, gives some damn good asm on the PSP
	v2 xy0( (f32)command.fillrect.x0, (f32)command.fillrect.y0 );
	v2 xy1( (f32)command.fillrect.x1, (f32)command.fillrect.y1 );

	// TODO - In 1/2cycle mode, skip bottom/right edges!?
	// This is done in BaseRenderer.

	gRenderer->FillRect( xy0, xy1, colour.GetColour() );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetZImg( MicroCodeCommand command )
{
	// No need check for (MAX_RAM_ADDRESS-1) here, since g_DI.Address is never used to reference a RAM location
	g_DI.Address = RDPSegAddr(command.inst.cmd1);
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetCImg( MicroCodeCommand command )
{
	g_CI.Format = command.img.fmt;
	g_CI.Size   = command.img.siz;
	g_CI.Width  = command.img.width + 1;
	g_CI.Address = RDPSegAddr(command.img.addr) & (MAX_RAM_ADDRESS-1);
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetCombine( MicroCodeCommand command )
{
	//Swap the endian
	REG64 Mux {};
	Mux._u32_0 = command.inst.cmd1;
	Mux._u32_1 = command.inst.arg0;

	gRenderer->SetMux( Mux._u64 );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetFillColor( MicroCodeCommand command )
{
	u32 fill_colour {command.inst.cmd1};

	gRenderer->SetFillColour( fill_colour );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetFogColor( MicroCodeCommand command )
{
	//c32	fog_colour( command.color.r, command.color.g, command.color.b, command.color.a );
	c32	fog_colour( command.color.r, command.color.g, command.color.b, 0 );	//alpha is always 0

	gRenderer->SetFogColour( fog_colour );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetBlendColor( MicroCodeCommand command )
{
	c32	blend_colour( command.color.r, command.color.g, command.color.b, command.color.a );

	gRenderer->SetBlendColour( blend_colour );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetPrimColor( MicroCodeCommand command )
{
	c32	prim_colour( command.color.r, command.color.g, command.color.b, command.color.a );

	gRenderer->SetPrimitiveLODFraction(command.color.prim_level / 256.f);
	gRenderer->SetPrimitiveColour( prim_colour );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_SetEnvColor( MicroCodeCommand command )
{
	c32	env_colour( command.color.r, command.color.g,command.color.b, command.color.a );

	gRenderer->SetEnvColour( env_colour );
}

//*****************************************************************************
//RSP TRI commands..
//In HLE emulation you NEVER see these commands !
//*****************************************************************************
void DLParser_TriRSP( MicroCodeCommand command ){}
