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
	 
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisable( GL_BLEND );
	
	// Default is ZBuffer disabled
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_DEPTH_TEST);
	
//	glEnable(GL_POLYGON_OFFSET_FILL);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void RendererVita::PrepareRenderState(const float (&mat_project)[16], bool disable_zbuffer )
{
	if ( disable_zbuffer )
	{
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
	else
	{
		// Decal mode
/*		if( gRDPOtherMode.zmode == 3 )
		{
			glPolygonOffset(-1.0, -1.0);
		}
		else
		{
			glPolygonOffset(0.0, 0.0);
		}*/

		
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
		float alpha_val = (float)alpha_threshold / 255.0f;
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
	
	// Second texture is sampled in 2 cycle mode if text_lod is clear (when set,
	// gRDPOtherMode.text_lod enables mipmapping, but we just set lod_frac to 0.
	bool use_t1 = cycle_mode == CYCLE_2CYCLE;

	bool install_textures[] = { true, use_t1 };
	
	// TODO: Add 2 cycle mode support
	if (install_textures[0]) {
		CNativeTexture *texture = mBoundTexture[0];
		if (texture != NULL) {
			texture->InstallTexture();
			
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
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mTexWrap[0].u);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mTexWrap[0].v);
		}
	}
	
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf((float*)mat_project);
}

void RendererVita::RenderTriangles(DaedalusVtx *p_vertices, u32 num_vertices, bool disable_zbuffer)
{
	if (mTnL.Flags.Texture)
	{
		UpdateTileSnapshots( mTextureTile );
		
		PrepareRenderState(gProjection.m, disable_zbuffer);
		
		CNativeTexture *texture = mBoundTexture[0];
		if (texture != NULL)
		{
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			
			float scale_x {texture->GetScaleX()};
			float scale_y {texture->GetScaleY()};
			
			// Hack to fix the sun in Zelda OOT/MM
			if( g_ROM.ZELDA_HACK && (gRDPOtherMode.L == 0x0c184241) )
			{
				scale_x *= 0.5f;
				scale_y *= 0.5f;
			}
			
			if (mTnL.Flags.Light && mTnL.Flags.TexGen)
			{	
				/*float *vtx_tex = gTexCoordBuffer;
				for (u32 i = 0; i < num_vertices; ++i)
				{
					gTexCoordBuffer[0] = (p_vertices[i].Texture.x * scale_x - mTileTopLeft[ 0 ].s * scale_x / 4.f) * scale_x;
					gTexCoordBuffer[1] = (p_vertices[i].Texture.y * scale_y - mTileTopLeft[ 0 ].t * scale_y / 4.f) * scale_y;
					gTexCoordBuffer += 2;
				}
				vglTexCoordPointerMapped(vtx_tex);*/
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			} else {
				float *vtx_tex = gTexCoordBuffer;
				for (u32 i = 0; i < num_vertices; ++i)
				{
					gTexCoordBuffer[0] = p_vertices[i].Texture.x * scale_x;
					gTexCoordBuffer[1] = p_vertices[i].Texture.y * scale_y;
					gTexCoordBuffer += 2;
				}
				vglTexCoordPointerMapped(vtx_tex);
			}
		}
	} else PrepareRenderState(gProjection.m, disable_zbuffer);
	
	glEnableClientState(GL_COLOR_ARRAY);
	float *vtx_ptr = gVertexBuffer;
	uint8_t *vtx_clr = (uint8_t*)gColorBuffer;
	for (int i = 0; i < num_vertices; i++) {
		gVertexBuffer[0] = p_vertices[i].Position.x;
		gVertexBuffer[1] = p_vertices[i].Position.y;
		gVertexBuffer[2] = p_vertices[i].Position.z;
		gColorBuffer[0] = p_vertices[i].Colour.GetColour();
		gColorBuffer++;
		gVertexBuffer += 3;
	}
	vglVertexPointerMapped(vtx_ptr);
	vglColorPointerMapped(GL_UNSIGNED_BYTE, vtx_clr);
	vglDrawObjects(GL_TRIANGLES, num_vertices, GL_TRUE);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void RendererVita::TexRect(u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1)
{
	// FIXME(strmnnrmn): in copy mode, depth buffer is always disabled. Might not need to check this explicitly.
	UpdateTileSnapshots( tile_idx );

	// NB: we have to do this after UpdateTileSnapshot, as it set up mTileTopLeft etc.
	// We have to do it before PrepareRenderState, because those values are applied to the graphics state.
	PrepareTexRectUVs(&st0, &st1);

	PrepareRenderState(mScreenToDevice.mRaw, gRDPOtherMode.depth_source ? false : true);
	
	v2 uv0( (float)st0.s / 32.f, (float)st0.t / 32.f );
	v2 uv1( (float)st1.s / 32.f, (float)st1.t / 32.f );

	v2 screen0;
	v2 screen1;
	ConvertN64ToScreen( xy0, screen0 );
	ConvertN64ToScreen( xy1, screen1 );

	const f32 depth = gRDPOtherMode.depth_source ? mPrimDepth : 0.0f;

	CNativeTexture *texture = mBoundTexture[0];
	float scale_x {texture->GetScaleX()};
	float scale_y {texture->GetScaleY()};

	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	gVertexBuffer[0] = screen0.x;
	gVertexBuffer[1] = screen0.y;
	gVertexBuffer[2] = depth;
	gVertexBuffer[3] = screen1.x;
	gVertexBuffer[4] = screen0.y;
	gVertexBuffer[5] = depth;
	gVertexBuffer[6] = screen0.x;
	gVertexBuffer[7] = screen1.y;
	gVertexBuffer[8] = depth;
	gVertexBuffer[9] = screen1.x;
	gVertexBuffer[10] = screen1.y;
	gVertexBuffer[11] = depth;
	gTexCoordBuffer[0] = uv0.x * scale_x;
	gTexCoordBuffer[1] = uv0.y * scale_y;
	gTexCoordBuffer[2] = uv1.x * scale_x;
	gTexCoordBuffer[3] = uv0.y * scale_y;
	gTexCoordBuffer[4] = uv0.x * scale_x;
	gTexCoordBuffer[5] = uv1.y * scale_y;
	gTexCoordBuffer[6] = uv1.x * scale_x;
	gTexCoordBuffer[7] = uv1.y * scale_y;
	vglVertexPointerMapped(gVertexBuffer);
	vglTexCoordPointerMapped(gTexCoordBuffer);
	gVertexBuffer += 12;
	gTexCoordBuffer += 8;
	vglDrawObjects(GL_TRIANGLE_STRIP, 4, GL_TRUE);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void RendererVita::TexRectFlip(u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1)
{
	// FIXME(strmnnrmn): in copy mode, depth buffer is always disabled. Might not need to check this explicitly.
	UpdateTileSnapshots( tile_idx );

	// NB: we have to do this after UpdateTileSnapshot, as it set up mTileTopLeft etc.
	// We have to do it before PrepareRenderState, because those values are applied to the graphics state.
	PrepareTexRectUVs(&st0, &st1);

	PrepareRenderState(mScreenToDevice.mRaw, gRDPOtherMode.depth_source ? false : true);

	v2 uv0( (float)st0.s / 32.f, (float)st0.t / 32.f );
	v2 uv1( (float)st1.s / 32.f, (float)st1.t / 32.f );

	v2 screen0;
	v2 screen1;
	ConvertN64ToScreen( xy0, screen0 );
	ConvertN64ToScreen( xy1, screen1 );

	const f32 depth = gRDPOtherMode.depth_source ? mPrimDepth : 0.0f;

	CNativeTexture *texture = mBoundTexture[0];
	float scale_x {texture->GetScaleX()};
	float scale_y {texture->GetScaleY()};

	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	gVertexBuffer[0] = screen0.x;
	gVertexBuffer[1] = screen0.y;
	gVertexBuffer[2] = depth;
	gVertexBuffer[3] = screen1.x;
	gVertexBuffer[4] = screen0.y;
	gVertexBuffer[5] = depth;
	gVertexBuffer[6] = screen0.x;
	gVertexBuffer[7] = screen1.y;
	gVertexBuffer[8] = depth;
	gVertexBuffer[9] = screen1.x;
	gVertexBuffer[10] = screen1.y;
	gVertexBuffer[11] = depth;
	gTexCoordBuffer[0] = uv0.x * scale_x;
	gTexCoordBuffer[1] = uv0.y * scale_y;
	gTexCoordBuffer[2] = uv0.x * scale_x;
	gTexCoordBuffer[3] = uv1.y * scale_y;
	gTexCoordBuffer[4] = uv1.x * scale_x;
	gTexCoordBuffer[5] = uv0.y * scale_y;
	gTexCoordBuffer[6] = uv1.x * scale_x;
	gTexCoordBuffer[7] = uv1.y * scale_y;
	vglVertexPointerMapped(gVertexBuffer);
	vglTexCoordPointerMapped(gTexCoordBuffer);
	gVertexBuffer += 12;
	gTexCoordBuffer += 8;
	vglDrawObjects(GL_TRIANGLE_STRIP, 4, GL_TRUE);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void RendererVita::FillRect(const v2 & xy0, const v2 & xy1, u32 color)
{
	PrepareRenderState(mScreenToDevice.mRaw, gRDPOtherMode.depth_source ? false : true);
	
	v2 screen0;
	v2 screen1;
	ConvertN64ToScreen( xy0, screen0 );
	ConvertN64ToScreen( xy1, screen1 );
	
	const f32 depth = gRDPOtherMode.depth_source ? mPrimDepth : 0.0f;
	
	glEnableClientState(GL_COLOR_ARRAY);
	gVertexBuffer[0] = screen0.x;
	gVertexBuffer[1] = screen0.y;
	gVertexBuffer[2] = depth;
	gVertexBuffer[3] = screen1.x;
	gVertexBuffer[4] = screen0.y;
	gVertexBuffer[5] = depth;
	gVertexBuffer[6] = screen0.x;
	gVertexBuffer[7] = screen1.y;
	gVertexBuffer[8] = depth;
	gVertexBuffer[9] = screen1.x;
	gVertexBuffer[10] = screen1.y;
	gVertexBuffer[11] = depth;
	gColorBuffer[0] = gColorBuffer[1] = gColorBuffer[2] = gColorBuffer[3] = color;
	vglVertexPointerMapped(gVertexBuffer);
	vglColorPointerMapped(GL_UNSIGNED_BYTE, gColorBuffer);
	gColorBuffer += 4;
	gVertexBuffer += 12;
	vglDrawObjects(GL_TRIANGLE_STRIP, 4, GL_TRUE);
}

void RendererVita::Draw2DTexture(f32 x0, f32 y0, f32 x1, f32 y1,
								f32 u0, f32 v0, f32 u1, f32 v1,
								const CNativeTexture * texture)
{
	gRDPOtherMode.cycle_type = CYCLE_COPY;
	PrepareRenderState(mScreenToDevice.mRaw, false);
	
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
	gRDPOtherMode.cycle_type = CYCLE_COPY;
	PrepareRenderState(mScreenToDevice.mRaw, false);
	
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
