#include "stdafx.h"
#include "RendererVita.h"

#include <vitaGL.h>

//#include "Combiner/BlendConstant.h"
//#include "Combiner/CombinerTree.h"
//#include "Combiner/RenderSettings.h"
#include "Core/ROM.h"
#include "Debug/Dump.h"
#include "Graphics/GraphicsContext.h"
#include "Graphics/NativeTexture.h"
#include "HLEGraphics/CachedTexture.h"
#include "HLEGraphics/DLDebug.h"
#include "HLEGraphics/RDPStateManager.h"
#include "HLEGraphics/TextureCache.h"
#include "Math/MathUtil.h"
#include "OSHLE/ultra_gbi.h"
#include "Utility/IO.h"
#include "Utility/Profiler.h"

#define DRAW_MODE GL_TRIANGLES

BaseRenderer *gRenderer    = nullptr;
RendererVita  *gRendererVita = nullptr;

extern float *gVertexBuffer;
extern uint32_t *gColorBuffer;
extern float *gTexCoordBuffer;

extern void InitBlenderMode( u32 blendmode );

struct ScePspFMatrix4
{
	float m[16];
};


static ScePspFMatrix4 gProjection;

RendererVita::RendererVita()
{
}

RendererVita::~RendererVita()
{
}

void RendererVita::RestoreRenderStates()
{
	// Initialise the device to our default state

	// No fog
	glDisable(GL_FOG);
	
	// We do our own culling
	glDisable(GL_CULL_FACE);
	
	glEnable(GL_SCISSOR_TEST);
	 
	glAlphaFunc(GL_GEQUAL, 0.01569f);
	glEnable(GL_ALPHA_TEST);
	
	glDisable(GL_BLEND);
	
	// Default is ZBuffer disabled
	glDepthMask(GL_TRUE);	// GL_TRUE to disable z-writes
	glDepthFunc(GL_GEQUAL);		// GEQUAL?
	glDisable(GL_DEPTH_TEST);
	
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void RendererVita::PrepareCurrentBlendMode( u32 render_mode, bool disable_zbuffer )
{
	if ( disable_zbuffer )
	{
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
	else
	{
		// Enable or Disable ZBuffer test
		if ( (mTnL.Flags.Zbuffer & gRDPOtherMode.z_cmp) | gRDPOtherMode.z_upd )
		{
			glEnable(GL_DEPTH_TEST);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}

		// GL_TRUE to disable z-writes
		glDepthMask( gRDPOtherMode.z_upd ? GL_TRUE : GL_FALSE );
	}
	
	// Initiate Texture Filter
	//
	// G_TF_AVERAGE : 1, G_TF_BILERP : 2 (linear)
	// G_TF_POINT   : 0 (nearest)
	//
	if( (gRDPOtherMode.text_filt != G_TF_POINT) | (gGlobalPreferences.ForceLinearFilter) )
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}
	
	u32 cycle_mode = gRDPOtherMode.cycle_type;

	// Initiate Blender
	//
	if(cycle_mode < CYCLE_COPY && gRDPOtherMode.force_bl)
	{
		InitBlenderMode(gRDPOtherMode.blender);
	}
	else
	{
		glDisable( GL_BLEND );
	}
	
	// Initiate Alpha test
	//
	if( (gRDPOtherMode.alpha_compare == G_AC_THRESHOLD) && !gRDPOtherMode.alpha_cvg_sel )
	{
		u8 alpha_threshold = mBlendColour.GetA();
		float alpha_val = 1.0f / (float)alpha_threshold;
		glAlphaFunc( (alpha_threshold | g_ROM.ALPHA_HACK) ? GL_GEQUAL : GL_GREATER, alpha_val);
		glEnable(GL_ALPHA_TEST);
	}
	else if (gRDPOtherMode.cvg_x_alpha)
	{
		// Going over 0x70 breaks OOT, but going lesser than that makes lines on games visible...ex: Paper Mario.
		// Also going over 0x30 breaks the birds in Tarzan :(. Need to find a better way to leverage this.
		glAlphaFunc(GL_GREATER, 0.4392f);
		glEnable(GL_ALPHA_TEST);
	}
	else
	{
		glDisable(GL_ALPHA_TEST);
	}
}

void RendererVita::RenderTriangles(DaedalusVtx * p_vertices, u32 num_vertices, bool disable_zbuffer)
{
	PrepareCurrentBlendMode(0, gRDPOtherMode.depth_source ? false : true);
	
/*	SBlendStateEntry		blend_entry;

	switch ( cycle_mode )
	{
		case CYCLE_COPY:		blend_entry.States = mCopyBlendStates; break;
		case CYCLE_FILL:		blend_entry.States = mFillBlendStates; break;
		case CYCLE_1CYCLE:		blend_entry = LookupBlendState( mMux, false ); break;
		case CYCLE_2CYCLE:		blend_entry = LookupBlendState( mMux, true ); break;
	}

	u32 render_flags( GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | render_mode );

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	// Used for Blend Explorer, or Nasty texture
	//
	if( DebugBlendmode( p_vertices, num_vertices, triangle_mode, render_flags, mMux ) )
		return;
#endif

	// This check is for inexact blends which were handled either by a custom blendmode or auto blendmode thing
	//
	if( blend_entry.OverrideFunction != NULL )
	{
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
		// Used for dumping mux and highlight inexact blend
		//
		DebugMux( blend_entry.States, p_vertices, num_vertices, triangle_mode, render_flags, mMux );
#endif

		// Local vars for now
		SBlendModeDetails details;

		details.EnvColour = mEnvColour;
		details.PrimColour = mPrimitiveColour;
		details.InstallTexture = true;
		details.ColourAdjuster.Reset();

		blend_entry.OverrideFunction( details );

		bool installed_texture( false );

		if( details.InstallTexture )
		{
			u32 texture_idx = g_ROM.T1_HACK ? 1 : 0;

			if( mBoundTexture[ texture_idx ] )
			{
				mBoundTexture[ texture_idx ]->InstallTexture();

				sceGuTexWrap( mTexWrap[ texture_idx ].u, mTexWrap[ texture_idx ].v );

				installed_texture = true;
			}
		}

		// If no texture was specified, or if we couldn't load it, clear it out
		if( !installed_texture )
		{
			sceGuDisable( GU_TEXTURE_2D );
		}

		if ( mTnL.Flags.Fog )
		{
			DaedalusVtx * p_FogVtx = static_cast<DaedalusVtx *>(sceGuGetMemory(num_vertices * sizeof(DaedalusVtx)));
			memcpy( p_FogVtx, p_vertices, num_vertices * sizeof( DaedalusVtx ) );
			details.ColourAdjuster.Process( p_vertices, num_vertices );
			sceGuDrawArray( triangle_mode, render_flags, num_vertices, NULL, p_vertices );
			RenderFog( p_FogVtx, num_vertices, triangle_mode, render_flags );
		}
		else
		{
			details.ColourAdjuster.Process( p_vertices, num_vertices );
			sceGuDrawArray( triangle_mode, render_flags, num_vertices, NULL, p_vertices );
		}
	}
	else if( blend_entry.States != NULL )
	{
		RenderUsingRenderSettings( blend_entry.States, p_vertices, num_vertices, triangle_mode, render_flags );
	}
	else*/
	{
		#ifdef DAEDALUS_DEBUG_CONSOLE
		// Set default states
		DAEDALUS_ERROR( "Unhandled blend mode" );
		#endif
		glEnableClientState(GL_COLOR_ARRAY);
		float *vtx_ptr = gVertexBuffer;
		uint8_t *vtx_clr = (uint8_t*)gColorBuffer;
		for (int i = 0; i < num_vertices; i++) {
			memcpy(gVertexBuffer, (float*)&p_vertices->Position.x, sizeof(float) * 3);
			gColorBuffer[0] = p_vertices->Colour.GetColour();
			gColorBuffer++;
			gVertexBuffer += 3;
		}
		vglVertexPointerMapped(vtx_ptr);
		vglColorPointerMapped(GL_UNSIGNED_BYTE, vtx_clr);
		vglDrawObjects(DRAW_MODE, num_vertices, GL_TRUE);
	}
}

void RendererVita::TexRect(u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1)
{
}

void RendererVita::TexRectFlip(u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1)
{
}

void RendererVita::FillRect(const v2 & xy0, const v2 & xy1, u32 color)
{
	PrepareCurrentBlendMode(0, true);
	
	v2 screen0;
	v2 screen1;
	ConvertN64ToScreen( xy0, screen0 );
	ConvertN64ToScreen( xy1, screen1 );
	
	glEnableClientState(GL_COLOR_ARRAY);
	gVertexBuffer[0] = screen0.x;
	gVertexBuffer[1] = screen0.y;
	gVertexBuffer[2] = 0.0f;
	gVertexBuffer[3] = screen1.x;
	gVertexBuffer[4] = screen0.y;
	gVertexBuffer[5] = 0.0f;
	gVertexBuffer[6] = screen1.x;
	gVertexBuffer[7] = screen1.y;
	gVertexBuffer[8] = 0.0f;
	gVertexBuffer[9] = screen0.x;
	gVertexBuffer[10] = screen1.y;
	gVertexBuffer[11] = 0.0f;
	gColorBuffer[0] = gColorBuffer[1] = gColorBuffer[2] = gColorBuffer[3] = color;
	vglVertexPointerMapped(gVertexBuffer);
	vglColorPointerMapped(GL_UNSIGNED_BYTE, gColorBuffer);
	gColorBuffer += 4;
	gVertexBuffer += 12;
	vglDrawObjects(GL_TRIANGLE_FAN, 4, GL_TRUE);
}

void RendererVita::Draw2DTexture(f32 x0, f32 y0, f32 x1, f32 y1,
								f32 u0, f32 v0, f32 u1, f32 v1,
								const CNativeTexture * texture)
{
}

void RendererVita::Draw2DTextureR(f32 x0, f32 y0, f32 x1, f32 y1,
								 f32 x2, f32 y2, f32 x3, f32 y3,
								 f32 s, f32 t)
{
}

bool CreateRenderer()
{
	gRendererVita = new RendererVita();
	gRenderer    = gRendererVita;
	return true;
}

void DestroyRenderer()
{
	delete gRendererVita;
	gRendererVita = nullptr;
	gRenderer    = nullptr;
}
