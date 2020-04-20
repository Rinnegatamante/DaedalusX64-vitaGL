#include "stdafx.h"
#include "RendererVita.h"

#include <vitaGL.h>

#include "Combiner/BlendConstant.h"
#include "Combiner/CombinerTree.h"
#include "Combiner/RenderSettings.h"
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

BaseRenderer *gRenderer    = nullptr;
RendererVita  *gRendererVita = nullptr;

extern float *gVertexBuffer;
extern uint32_t *gColorBuffer;
extern float *gTexCoordBuffer;

struct ScePspFMatrix4
{
	float m[16];
};


ScePspFMatrix4		gProjection;
void sceGuSetMatrix(int type, const ScePspFMatrix4 * mtx)
{
	if (type == GL_PROJECTION)
	{
		memcpy(&gProjection, mtx, sizeof(gProjection));
	}
}

static void InitBlenderMode()
{
	u32 cycle_type    = gRDPOtherMode.cycle_type;
	u32 cvg_x_alpha   = gRDPOtherMode.cvg_x_alpha;
	u32 alpha_cvg_sel = gRDPOtherMode.alpha_cvg_sel;
	u32 blendmode     = gRDPOtherMode.blender;

	// NB: If we're running in 1cycle mode, ignore the 2nd cycle.
	u32 active_mode = (cycle_type == CYCLE_2CYCLE) ? blendmode : (blendmode & 0xcccc);

	enum BlendType
	{
		kBlendModeOpaque,
		kBlendModeAlphaTrans,
		kBlendModeFade,
	};
	BlendType type = kBlendModeOpaque;

	// FIXME(strmnnrmn): lots of these need fog!

	switch (active_mode)
	{
	case 0x0040: // In * AIn + Mem * 1-A
		// MarioKart (spinning logo).
		type = kBlendModeAlphaTrans;
		break;
	case 0x0050: // In * AIn + Mem * 1-A | In * AIn + Mem * 1-A
		// Extreme-G.
		type = kBlendModeAlphaTrans;
		break;
	case 0x0440: // In * AFog + Mem * 1-A
		// Bomberman64. alpha_cvg_sel: 1 cvg_x_alpha: 1
		type = kBlendModeAlphaTrans;
		break;
	case 0x04d0: // In * AFog + Fog * 1-A | In * AIn + Mem * 1-A
		// Conker.
		type = kBlendModeAlphaTrans;
		break;
	case 0x0150: // In * AIn + Mem * 1-A | In * AFog + Mem * 1-A
		// Spiderman.
		type = kBlendModeAlphaTrans;
		break;
	case 0x0c08: // In * 0 + In * 1
		// MarioKart (spinning logo)
		// This blend mode doesn't use the alpha value
		type = kBlendModeOpaque;
		break;
	case 0x0c18: // In * 0 + In * 1 | In * AIn + Mem * 1-A
		// StarFox main menu.
		type = kBlendModeAlphaTrans;
		break;
	case 0x0c40: // In * 0 + Mem * 1-A
		// Extreme-G.
		type = kBlendModeFade;
		break;
	case 0x0f0a: // In * 0 + In * 1 | In * 0 + In * 1
		// Zelda OoT.
		type = kBlendModeOpaque;
		break;
	case 0x4c40: // Mem * 0 + Mem * 1-A
		//Waverace - alpha_cvg_sel: 0 cvg_x_alpha: 1
		type = kBlendModeFade;
		break;
	case 0x8410: // Bl * AFog + In * 1-A | In * AIn + Mem * 1-A
		// Paper Mario.
		type = kBlendModeAlphaTrans;
		break;
	case 0xc410: // Fog * AFog + In * 1-A | In * AIn + Mem * 1-A
		// Donald Duck (Dust)
		type = kBlendModeAlphaTrans;
		break;
	case 0xc440: // Fog * AFog + Mem * 1-A
		// Banjo Kazooie
		// Banjo Tooie sun glare
		// FIXME: blends fog over existing?
		type = kBlendModeAlphaTrans;
		break;
	case 0xc800: // Fog * AShade + In * 1-A
		//Bomberman64. alpha_cvg_sel: 0 cvg_x_alpha: 1
		type = kBlendModeOpaque;
		break;
	case 0xc810: // Fog * AShade + In * 1-A | In * AIn + Mem * 1-A
		// AeroGauge (ingame)
		type = kBlendModeAlphaTrans;
		break;
	// case 0x0321: // In * 0 + Bl * AMem
	// 	// Hmm - not sure about what this is doing. Zelda OoT pause screen.
	// 	type = kBlendModeAlphaTrans;
	// 	break;

	default:
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
		DebugBlender( cycle_type, active_mode, alpha_cvg_sel, cvg_x_alpha );
		DL_PF( "		 Blend: SRCALPHA/INVSRCALPHA (default: 0x%04x)", active_mode );
#endif
		break;
	}

	// NB: we only have alpha in the blender is alpha_cvg_sel is 0 or cvg_x_alpha is 1.
	bool have_alpha = !alpha_cvg_sel || cvg_x_alpha;

	if (type == kBlendModeAlphaTrans && !have_alpha)
		type = kBlendModeOpaque;

	switch (type)
	{
	case kBlendModeOpaque:
		glDisable(GL_BLEND);
		break;
	case kBlendModeAlphaTrans:
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		break;
	case kBlendModeFade:
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		break;
	}
}

RendererVita::RendererVita()
{
	//
	//	Set up RGB = T0, A = T0
	//
	mCopyBlendStates = new CBlendStates;
	{
		CAlphaRenderSettings *	alpha_settings( new CAlphaRenderSettings( "Copy" ) );
		CRenderSettingsModulate *	colour_settings( new CRenderSettingsModulate( "Copy" ) );

		alpha_settings->AddTermTexel0();
		colour_settings->AddTermTexel0();

		mCopyBlendStates->SetAlphaSettings( alpha_settings );
		mCopyBlendStates->AddColourSettings( colour_settings );
	}

	//
	//	Set up RGB = Diffuse, A = Diffuse
	//
	mFillBlendStates = new CBlendStates;
	{
		CAlphaRenderSettings *	alpha_settings( new CAlphaRenderSettings( "Fill" ) );
		CRenderSettingsModulate *	colour_settings( new CRenderSettingsModulate( "Fill" ) );

		alpha_settings->AddTermConstant( new CBlendConstantExpressionValue( BC_SHADE ) );
		colour_settings->AddTermConstant(  new CBlendConstantExpressionValue( BC_SHADE ) );

		mFillBlendStates->SetAlphaSettings( alpha_settings );
		mFillBlendStates->AddColourSettings( colour_settings );
	}

}

RendererVita::~RendererVita()
{
	delete mFillBlendStates;
	delete mCopyBlendStates;
}

void RendererVita::RestoreRenderStates()
{
	// Initialise the device to our default state

	// No fog
	glDisable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	
	glEnable(GL_SCISSOR_TEST);
	
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisable( GL_BLEND );
	
	// Default is ZBuffer disabled
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_DEPTH_TEST);
	
	// Enable this for rendering decals (glPolygonOffset).
	glEnable(GL_POLYGON_OFFSET_FILL);
		
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	float envcolor[4] = {c32::White.GetRf(), c32::White.GetGf(), c32::White.GetBf(), c32::White.GetAf()};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
	
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

RendererVita::SBlendStateEntry RendererVita::LookupBlendState( u64 mux, bool two_cycles )
{
	#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DAEDALUS_PROFILE( "RendererPSP::LookupBlendState" );
	mRecordedCombinerStates.insert( mux );
	#endif

	REG64 key;
	key._u64 = mux;

	// Top 8 bits are never set - use the very top one to differentiate between 1/2 cycles
	key._u32_1 |= (two_cycles << 31);

	BlendStatesMap::const_iterator	it( mBlendStatesMap.find( key._u64 ) );
	if( it != mBlendStatesMap.end() )
	{
		return it->second;
	}

	// Blendmodes with Inexact blends either get an Override blend or a Default blend (GU_TFX_MODULATE)
	// If its not an Inexact blend then we check if we need to Force a blend mode none the less// Salvy
	//
	SBlendStateEntry entry;
	CCombinerTree tree( mux, two_cycles );
	entry.States = tree.GetBlendStates();

	if( entry.States->IsInexact() )
	{
		entry.OverrideFunction = LookupOverrideBlendModeInexact( mux );
	}
	else
	{
		// This is for non-inexact blends, errg hacks and such to be more precise
		entry.OverrideFunction = LookupOverrideBlendModeForced( mux );
	}

	#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	printf( "Adding %08x%08x - %d cycles - %s\n", u32(mux>>32), u32(mux), two_cycles ? 2 : 1, entry.States->IsInexact() ?  IsCombinerStateDefault(mux) ? "Inexact(Default)" : "Inexact(Override)" : entry.OverrideFunction==nullptr ? "Auto" : "Forced");
	#endif

	//Add blend mode to the Blend States Map
	mBlendStatesMap[ key._u64 ] = entry;

	return entry;
}

void RendererVita::DrawPrimitives(DaedalusVtx * p_vertices, u32 num_vertices, u32 triangle_mode, bool has_texture)
{
	glEnableClientState(GL_COLOR_ARRAY);
	float *vtx_ptr = gVertexBuffer;
	for (uint32_t i = 0; i < num_vertices; i++) {
		gVertexBuffer[0] = p_vertices[i].Position.x;
		gVertexBuffer[1] = p_vertices[i].Position.y;
		gVertexBuffer[2] = p_vertices[i].Position.z;
		gColorBuffer[i] = p_vertices[i].Colour.GetColour();
		gVertexBuffer += 3;
	}
	vglVertexPointerMapped(vtx_ptr);
	vglColorPointerMapped(GL_UNSIGNED_BYTE, gColorBuffer);
	vglDrawObjects(triangle_mode, num_vertices, GL_TRUE);
	gColorBuffer += num_vertices;
}

void RendererVita::RenderUsingRenderSettings( const CBlendStates * states, DaedalusVtx * p_vertices, u32 num_vertices, u32 triangle_mode)
{
	const CAlphaRenderSettings *	alpha_settings( states->GetAlphaSettings() );

	SRenderState	state;

	state.Vertices = p_vertices;
	state.NumVertices = num_vertices;
	state.PrimitiveColour = mPrimitiveColour;
	state.EnvironmentColour = mEnvColour;

	//Avoid copying vertices twice if we already save a copy to render fog //Corn
	/*DaedalusVtx * p_FogVtx( mVtx_Save );
	if( mTnL.Flags.Fog )
	{
		p_FogVtx = static_cast<DaedalusVtx *>(malloc(num_vertices * sizeof(DaedalusVtx)));
		memcpy( p_FogVtx, p_vertices, num_vertices * sizeof( DaedalusVtx ) );
	}
	else if( states->GetNumStates() > 1 )
	{
		memcpy( mVtx_Save, p_vertices, num_vertices * sizeof( DaedalusVtx ) );
	}*/
	
	glEnableClientState(GL_COLOR_ARRAY);
	float *vtx_ptr = gVertexBuffer;
	for (uint32_t i = 0; i < num_vertices; i++) {
		gVertexBuffer[0] = p_vertices[i].Position.x;
		gVertexBuffer[1] = p_vertices[i].Position.y;
		gVertexBuffer[2] = p_vertices[i].Position.z;
		gVertexBuffer += 3;
	}
	vglVertexPointerMapped(vtx_ptr);

	for( u32 i {}; i < states->GetNumStates(); ++i )
	{
		const CRenderSettings *		settings( states->GetColourSettings( i ) );

		bool install_texture0( settings->UsesTexture0() || alpha_settings->UsesTexture0() );
		bool install_texture1( settings->UsesTexture1() || alpha_settings->UsesTexture1() );

		SRenderStateOut out;

		memset( &out, 0, sizeof( out ) );

		settings->Apply( install_texture0 || install_texture1, state, out );
		alpha_settings->Apply( install_texture0 || install_texture1, state, out );

		// TODO: this nobbles the existing diffuse colour on each pass. Need to use a second buffer...
		/*if( i > 0 )
		{
			memcpy( p_vertices, p_FogVtx, num_vertices * sizeof( DaedalusVtx ) );
		}*/

		if(out.VertexExpressionRGB != nullptr)
		{
			out.VertexExpressionRGB->ApplyExpressionRGB( state );
		}
		if(out.VertexExpressionA != nullptr)
		{
			out.VertexExpressionA->ApplyExpressionAlpha( state );
		}

		bool installed_texture {false};

		u32 texture_idx {};

		if(install_texture0 || install_texture1)
		{
			u32	tfx = GL_MODULATE;
			switch( out.BlendMode )
			{
			case PBM_MODULATE:
				tfx = GL_MODULATE;
				break;
			case PBM_REPLACE:
				tfx = GL_REPLACE;
				break;
			case PBM_BLEND:
				tfx = GL_BLEND;
				float envcolor[4] = {out.TextureFactor.GetRf(), out.TextureFactor.GetGf(), out.TextureFactor.GetBf(), out.TextureFactor.GetAf()};
				glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envcolor);
				break;
			}
			
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, tfx);

			if( g_ROM.T1_HACK )
			{
				// NB if install_texture0 and install_texture1 are both set, 1 wins out
				texture_idx = install_texture1;

				// NOTE: Rinnegatamante 15/04/20
				// Technically we calculate this on DrawTriangles, is it enough?
				
				/*const CNativeTexture * texture1 = mBoundTexture[ 1 ];

				if( install_texture1 && texture1 && mTnL.Flags.Texture && (mTnL.Flags._u32 & (TNL_LIGHT|TNL_TEXGEN)) != (TNL_LIGHT|TNL_TEXGEN) )
				{
					
					float scale_x = texture1->GetScaleX();
					float scale_y = texture1->GetScaleY();

					sceGuTexOffset( -mTileTopLeft[ 1 ].s * scale_x / 4.f,
									-mTileTopLeft[ 1 ].t * scale_y / 4.f );
					sceGuTexScale( scale_x, scale_y );
				}*/
			}
			else
			{
				// NB if install_texture0 and install_texture1 are both set, 0 wins out
				texture_idx = install_texture0 ? 0 : 1;
			}

			CRefPtr<CNativeTexture> texture;

			if(out.MakeTextureWhite)
			{
				TextureInfo white_ti = mBoundTextureInfo[ texture_idx ];
				white_ti.SetWhite(true);
				texture = CTextureCache::Get()->GetOrCreateTexture( white_ti );
			}
			else
			{
				texture = mBoundTexture[ texture_idx ];
			}

			if(texture != nullptr)
			{
				texture->InstallTexture();
				installed_texture = true;
			}
		}

		// If no texture was specified, or if we couldn't load it, clear it out
		if( !installed_texture )
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mTexWrap[texture_idx].u);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mTexWrap[texture_idx].v);
		
		for (uint32_t i = 0; i < num_vertices; i++) {
			gColorBuffer[i] = p_vertices[i].Colour.GetColour();
		}
		vglColorPointerMapped(GL_UNSIGNED_BYTE, gColorBuffer);
		gColorBuffer += num_vertices;
		
		vglDrawObjects(triangle_mode, num_vertices, GL_TRUE);

		/*if ( mTnL.Flags.Fog )
		{
			RenderFog( p_FogVtx, num_vertices, triangle_mode, render_flags );
		}*/
	}
}


void RendererVita::RenderUsingCurrentBlendMode(const float (&mat_project)[16], DaedalusVtx * p_vertices, u32 num_vertices, u32 triangle_mode, bool disable_zbuffer )
{
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf((float*)mat_project);
	
	if ( disable_zbuffer )
	{
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
	else
	{
		// Decal mode
		if( gRDPOtherMode.zmode == 3 )
		{
			glPolygonOffset(-1.0, -1.0);
		}
		else
		{
			glPolygonOffset(0.0, 0.0);
		}
		
		// Enable or Disable ZBuffer test
		if ( (mTnL.Flags.Zbuffer & gRDPOtherMode.z_cmp) | gRDPOtherMode.z_upd )
		{
			glEnable(GL_DEPTH_TEST);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}

		glDepthMask( gRDPOtherMode.z_upd ? GL_TRUE : GL_FALSE );
	}
	
	// Initiate Texture Filter
	//
	// G_TF_AVERAGE : 1, G_TF_BILERP : 2 (linear)
	// G_TF_POINT   : 0 (nearest)
	//
	if( (gRDPOtherMode.text_filt != G_TF_POINT) || (gGlobalPreferences.ForceLinearFilter) )
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
		InitBlenderMode();
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
		float alpha_val = mBlendColour.GetAf();
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
	
	SBlendStateEntry		blend_entry;

	switch ( cycle_mode )
	{
		case CYCLE_COPY:		blend_entry.States = mCopyBlendStates; break;
		case CYCLE_FILL:		blend_entry.States = mFillBlendStates; break;
		case CYCLE_1CYCLE:		blend_entry = LookupBlendState( mMux, false ); break;
		case CYCLE_2CYCLE:		blend_entry = LookupBlendState( mMux, true ); break;
	}
	
	if( blend_entry.OverrideFunction != nullptr )
	{
		// Local vars for now
		SBlendModeDetails details;

		details.EnvColour = mEnvColour;
		details.PrimColour = mPrimitiveColour;
		details.InstallTexture = true;
		details.ColourAdjuster.Reset();

		blend_entry.OverrideFunction( details );

		bool installed_texture = false;
		if( details.InstallTexture )
		{
			int texture_idx = g_ROM.T1_HACK ? 1 : 0;
			if( mBoundTexture[ texture_idx ] )
			{
				mBoundTexture[ texture_idx ]->InstallTexture();
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mTexWrap[texture_idx].u);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mTexWrap[texture_idx].v);
				installed_texture = true;
			}
		}
		
		// If no texture was specified, or if we couldn't load it, clear it out
		if( !installed_texture )
		{
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		
		/*if ( mTnL.Flags.Fog )
		{
		}
		else*/
		{
			details.ColourAdjuster.Process(p_vertices, num_vertices);
			DrawPrimitives(p_vertices, num_vertices, triangle_mode, installed_texture);
		}
	}else if( blend_entry.States != nullptr )
	{
		RenderUsingRenderSettings( blend_entry.States, p_vertices, num_vertices, triangle_mode );
	}
	else
	{
		#ifdef DAEDALUS_DEBUG_CONSOLE
		// Set default states
		DAEDALUS_ERROR( "Unhandled blend mode" );
		#endif
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		DrawPrimitives(p_vertices, num_vertices, triangle_mode, false);
	}

}

void RendererVita::RenderTriangles(DaedalusVtx *p_vertices, u32 num_vertices, bool disable_zbuffer)
{
	if (mTnL.Flags.Texture)
	{
		UpdateTileSnapshots( mTextureTile );
		
		CNativeTexture *texture = mBoundTexture[0];
		
		float *vtx_tex = gTexCoordBuffer;
		
		if( texture && (mTnL.Flags._u32 & (TNL_LIGHT|TNL_TEXGEN)) != (TNL_LIGHT|TNL_TEXGEN) )
		{
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			float scale_x = texture->GetScaleX();
			float scale_y = texture->GetScaleY();
				
			// Hack to fix the sun in Zelda OOT/MM
			if( g_ROM.ZELDA_HACK && (gRDPOtherMode.L == 0x0c184241) )
			{
				scale_x *= 0.5f;
				scale_y *= 0.5f;
			}
				
			for (u32 i = 0; i < num_vertices; ++i)
			{
				gTexCoordBuffer[0] = (p_vertices[i].Texture.x * scale_x - (mTileTopLeft[ 0 ].s  / 4.f * scale_x));
				gTexCoordBuffer[1] = (p_vertices[i].Texture.y * scale_y - (mTileTopLeft[ 0 ].t  / 4.f * scale_y));
				gTexCoordBuffer += 2;
			}	
		} else {
			for (u32 i = 0; i < num_vertices; ++i)
			{
				gTexCoordBuffer[0] = p_vertices[i].Texture.x;
				gTexCoordBuffer[1] = p_vertices[i].Texture.y;
				gTexCoordBuffer += 2;
			}
		}
			
		vglTexCoordPointerMapped(vtx_tex);
	}
	
	RenderUsingCurrentBlendMode(gProjection.m, p_vertices, num_vertices, GL_TRIANGLES, disable_zbuffer);
}

void RendererVita::TexRect(u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1)
{
	// FIXME(strmnnrmn): in copy mode, depth buffer is always disabled. Might not need to check this explicitly.
	UpdateTileSnapshots( tile_idx );

	// NB: we have to do this after UpdateTileSnapshot, as it set up mTileTopLeft etc.
	// We have to do it before PrepareRenderState, because those values are applied to the graphics state.
	PrepareTexRectUVs(&st0, &st1);
	
	v2 uv0( (float)st0.s / 32.f, (float)st0.t / 32.f );
	v2 uv1( (float)st1.s / 32.f, (float)st1.t / 32.f );

	v2 screen0;
	v2 screen1;
	ScaleN64ToScreen( xy0, screen0 );
	ScaleN64ToScreen( xy1, screen1 );

	const f32 depth = gRDPOtherMode.depth_source ? mPrimDepth : 0.0f;

	CNativeTexture *texture = mBoundTexture[0];
	float scale_x {texture->GetScaleX()};
	float scale_y {texture->GetScaleY()};

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	gTexCoordBuffer[0] = uv0.x * scale_x;
	gTexCoordBuffer[1] = uv0.y * scale_y;
	gTexCoordBuffer[2] = uv1.x * scale_x;
	gTexCoordBuffer[3] = uv0.y * scale_y;
	gTexCoordBuffer[4] = uv0.x * scale_x;
	gTexCoordBuffer[5] = uv1.y * scale_y;
	gTexCoordBuffer[6] = uv1.x * scale_x;
	gTexCoordBuffer[7] = uv1.y * scale_y;
	vglTexCoordPointerMapped(gTexCoordBuffer);
	gTexCoordBuffer += 8;
	
	DaedalusVtx * p_vertices = static_cast<DaedalusVtx *>(malloc(4 * sizeof(DaedalusVtx)));
	
	p_vertices[0].Position.x = screen0.x;
	p_vertices[0].Position.y = screen0.y;
	p_vertices[0].Position.z = depth;
	p_vertices[0].Colour = c32(0xffffffff);

	p_vertices[1].Position.x = screen1.x;
	p_vertices[1].Position.y = screen0.y;
	p_vertices[1].Position.z = depth;
	p_vertices[1].Colour = c32(0xffffffff);

	p_vertices[2].Position.x = screen0.x;
	p_vertices[2].Position.y = screen1.y;
	p_vertices[2].Position.z = depth;
	p_vertices[2].Colour = c32(0xffffffff);

	p_vertices[3].Position.x = screen1.x;
	p_vertices[3].Position.y = screen1.y;
	p_vertices[3].Position.z = depth;
	p_vertices[3].Colour = c32(0xffffffff);

	RenderUsingCurrentBlendMode(mScreenToDevice.mRaw, p_vertices, 4, GL_TRIANGLE_STRIP, gRDPOtherMode.depth_source ? false : true);
	free(p_vertices);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void RendererVita::TexRectFlip(u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1)
{
	// FIXME(strmnnrmn): in copy mode, depth buffer is always disabled. Might not need to check this explicitly.
	UpdateTileSnapshots( tile_idx );

	// NB: we have to do this after UpdateTileSnapshot, as it set up mTileTopLeft etc.
	// We have to do it before PrepareRenderState, because those values are applied to the graphics state.
	PrepareTexRectUVs(&st0, &st1);

	v2 uv0( (float)st0.s / 32.f, (float)st0.t / 32.f );
	v2 uv1( (float)st1.s / 32.f, (float)st1.t / 32.f );

	v2 screen0;
	v2 screen1;
	ScaleN64ToScreen( xy0, screen0 );
	ScaleN64ToScreen( xy1, screen1 );

	const f32 depth = gRDPOtherMode.depth_source ? mPrimDepth : 0.0f;

	CNativeTexture *texture = mBoundTexture[0];
	float scale_x {texture->GetScaleX()};
	float scale_y {texture->GetScaleY()};
	
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	gTexCoordBuffer[0] = uv0.x * scale_x;
	gTexCoordBuffer[1] = uv0.y * scale_y;
	gTexCoordBuffer[2] = uv0.x * scale_x;
	gTexCoordBuffer[3] = uv1.y * scale_y;
	gTexCoordBuffer[4] = uv1.x * scale_x;
	gTexCoordBuffer[5] = uv0.y * scale_y;
	gTexCoordBuffer[6] = uv1.x * scale_x;
	gTexCoordBuffer[7] = uv1.y * scale_y;
	vglTexCoordPointerMapped(gTexCoordBuffer);
	gTexCoordBuffer += 8;
	
	DaedalusVtx * p_vertices = static_cast<DaedalusVtx *>(malloc(4 * sizeof(DaedalusVtx)));

	p_vertices[0].Position.x = screen0.x;
	p_vertices[0].Position.y = screen0.y;
	p_vertices[0].Position.z = 0.0f;
	p_vertices[0].Colour = c32(0xffffffff);

	p_vertices[1].Position.x = screen1.x;
	p_vertices[1].Position.y = screen0.y;
	p_vertices[1].Position.z = 0.0f;
	p_vertices[1].Colour = c32(0xffffffff);;

	p_vertices[2].Position.x = screen0.x;
	p_vertices[2].Position.y = screen1.y;
	p_vertices[2].Position.z = 0.0f;
	p_vertices[2].Colour = c32(0xffffffff);

	p_vertices[3].Position.x = screen1.x;
	p_vertices[3].Position.y = screen1.y;
	p_vertices[3].Position.z = 0.0f;
	p_vertices[3].Colour = c32(0xffffffff);

	RenderUsingCurrentBlendMode(mScreenToDevice.mRaw, p_vertices, 4, GL_TRIANGLE_STRIP, gRDPOtherMode.depth_source ? false : true);
	free(p_vertices);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void RendererVita::FillRect(const v2 & xy0, const v2 & xy1, u32 color)
{
	v2 screen0;
	v2 screen1;
	ScaleN64ToScreen( xy0, screen0 );
	ScaleN64ToScreen( xy1, screen1 );
	
	const f32 depth = gRDPOtherMode.depth_source ? mPrimDepth : 0.0f;
	
	DaedalusVtx * p_vertices = static_cast<DaedalusVtx *>(malloc(4 * sizeof(DaedalusVtx)));
	
	p_vertices[0].Position.x = screen0.x;
	p_vertices[0].Position.y = screen0.y;
	p_vertices[0].Position.z = 0.0f;
	p_vertices[0].Colour = c32(color);

	p_vertices[1].Position.x = screen1.x;
	p_vertices[1].Position.y = screen0.y;
	p_vertices[1].Position.z = 0.0f;
	p_vertices[1].Colour = c32(color);

	p_vertices[2].Position.x = screen0.x;
	p_vertices[2].Position.y = screen1.y;
	p_vertices[2].Position.z = 0.0f;
	p_vertices[2].Colour = c32(color);

	p_vertices[3].Position.x = screen1.x;
	p_vertices[3].Position.y = screen1.y;
	p_vertices[3].Position.z = 0.0f;
	p_vertices[3].Colour = c32(color);
	
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	RenderUsingCurrentBlendMode(mScreenToDevice.mRaw, p_vertices, 4, GL_TRIANGLE_STRIP, true);
	free(p_vertices);
}

void RendererVita::Draw2DTexture(f32 x0, f32 y0, f32 x1, f32 y1,
								f32 u0, f32 v0, f32 u1, f32 v1,
								const CNativeTexture * texture)
{
	
	glEnable(GL_BLEND);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	float sx0 = N64ToScreenX(x0);
	float sy0 = N64ToScreenY(y0);

	float sx1 = N64ToScreenX(x1);
	float sy1 = N64ToScreenY(y1);

	const f32 depth = 0.0f;

	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	gVertexBuffer[0] = sx0;
	gVertexBuffer[1] = sy0;
	gVertexBuffer[2] = depth;
	gVertexBuffer[3] = sx1;
	gVertexBuffer[4] = sy0;
	gVertexBuffer[5] = depth;
	gVertexBuffer[6] = sx0;
	gVertexBuffer[7] = sy1;
	gVertexBuffer[8] = depth;
	gVertexBuffer[9] = sx1;
	gVertexBuffer[10] = sy1;
	gVertexBuffer[11] = depth;
	gTexCoordBuffer[0] = u0;
	gTexCoordBuffer[1] = v0;
	gTexCoordBuffer[2] = u1;
	gTexCoordBuffer[3] = v0;
	gTexCoordBuffer[4] = u0;
	gTexCoordBuffer[5] = v1;
	gTexCoordBuffer[6] = u1;
	gTexCoordBuffer[7] = v1;
	vglVertexPointerMapped(gVertexBuffer);
	vglTexCoordPointerMapped(gTexCoordBuffer);
	gVertexBuffer += 12;
	gTexCoordBuffer += 8;
	vglDrawObjects(GL_TRIANGLE_STRIP, 4, GL_TRUE);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void RendererVita::Draw2DTextureR(f32 x0, f32 y0, f32 x1, f32 y1,
								 f32 x2, f32 y2, f32 x3, f32 y3,
								 f32 s, f32 t)
{
	
	glEnable(GL_BLEND);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	const f32 depth = 0.0f;

	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	gVertexBuffer[0] = N64ToScreenX(x0);
	gVertexBuffer[1] = N64ToScreenY(y0);
	gVertexBuffer[2] = depth;
	gVertexBuffer[3] = N64ToScreenX(x1);
	gVertexBuffer[4] = N64ToScreenY(y1);
	gVertexBuffer[5] = depth;
	gVertexBuffer[6] = N64ToScreenX(x2);
	gVertexBuffer[7] = N64ToScreenY(y2);
	gVertexBuffer[8] = depth;
	gVertexBuffer[9] = N64ToScreenX(x3);
	gVertexBuffer[10] = N64ToScreenY(y3);
	gVertexBuffer[11] = depth;
	gTexCoordBuffer[0] = 0.0f;
	gTexCoordBuffer[1] = 0.0f;
	gTexCoordBuffer[2] = s;
	gTexCoordBuffer[3] = 0.0f;
	gTexCoordBuffer[4] = s;
	gTexCoordBuffer[5] = t;
	gTexCoordBuffer[6] = 0.0f;
	gTexCoordBuffer[7] = t;
	vglVertexPointerMapped(gVertexBuffer);
	vglTexCoordPointerMapped(gTexCoordBuffer);
	gVertexBuffer += 12;
	gTexCoordBuffer += 8;
	vglDrawObjects(GL_TRIANGLE_FAN, 4, GL_TRUE);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
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
