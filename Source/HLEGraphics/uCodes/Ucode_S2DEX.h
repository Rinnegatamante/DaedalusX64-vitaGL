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

#ifndef HLEGRAPHICS_UCODES_UCODE_S2DEX_H_
#define HLEGRAPHICS_UCODES_UCODE_S2DEX_H_

//*****************************************************************************
// Needed by S2DEX
//*****************************************************************************

#define	G_GBI2_SELECT_DL		0x04
#define	S2DEX_OBJLT_TXTRBLOCK	0x00001033
#define	S2DEX_OBJLT_TXTRTILE	0x00fc1034
#define	S2DEX_OBJLT_TLUT		0x00000030
#define	S2DEX_BGLT_LOADBLOCK	0x0033
#define	S2DEX_BGLT_LOADTILE		0xfff4

//*****************************************************************************
//
//*****************************************************************************
struct uObjBg
{
	u16 imageW;
	u16 imageX;
	u16 frameW;
	s16 frameX;
	u16 imageH;
	u16 imageY;
	u16 frameH;
	s16 frameY;

	u32 imagePtr;
	u8  imageSiz;
	u8  imageFmt;
	u16 imageLoad;
	u16 imageFlip;
	u16 imagePal;

	u16 tmemH;
	u16 tmemW;
	u16 tmemLoadTH;
	u16 tmemLoadSH;
	u16 tmemSize;
	u16 tmemSizeW;
};

//*****************************************************************************
//
//*****************************************************************************
struct uObjMtx
{
	s32	  A, B, C, D;

	short Y;
	short X;

	u16   BaseScaleY;
	u16   BaseScaleX;
};

//*****************************************************************************
//
//*****************************************************************************
struct uObjSubMtx
{
	short Y;
	short X;

	u16   BaseScaleY;
	u16   BaseScaleX;
};

//*****************************************************************************
//
//*****************************************************************************
struct Matrix2D
{
	f32 A, B, C, D;
	f32 X, Y;
	f32 BaseScaleX;
	f32 BaseScaleY;
} mat2D = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f };

//*****************************************************************************
//
//*****************************************************************************
struct	uObjScaleBg
{
	u16	imageW;
	u16	imageX;

	u16	frameW;
	s16	frameX;

	u16	imageH;
	u16	imageY;

	u16	frameH;
	s16	frameY;

	u32	imagePtr;

	u8	imageSiz;
	u8	imageFmt;
	u16	imageLoad;

	u16	imageFlip;
	u16	imagePal;

	u16	scaleH;
	u16	scaleW;

	s32	imageYorig;
	u8	padding[4];
};

//*****************************************************************************
//
//*****************************************************************************
struct	uObjTxtrBlock //PSP Format
{
	  u32	type;
	  u32	image;

	  u16	tsize;
	  u16	tmem;

	  u16	sid;
	  u16	tline;

	  u32	flag;
	  u32	mask;
};

//*****************************************************************************
//
//*****************************************************************************
struct uObjTxtrTile //PSP Format
{
	  u32	type;
	  u32	image;

	  u16	twidth;
	  u16	tmem;

	  u16	sid;
	  u16	theight;

	  u32	flag;
	  u32	mask;
};

//*****************************************************************************
//
//*****************************************************************************
struct uObjTxtrTLUT // PSP Format
{
	u32	type;
	u32	image;

	u16	pnum;
	u16	phead;

	u16	sid;
	u16   zero;

	u32	flag;
	u32	mask;
};

//*****************************************************************************
//
//*****************************************************************************
union uObjTxtr
{
	uObjTxtrBlock	block;
	uObjTxtrTile	tile;
	uObjTxtrTLUT	tlut;
};

//*****************************************************************************
//
//*****************************************************************************
struct uObjSprite
{
	u16  scaleW;
	short  objX;

	u16  paddingX;
	u16  imageW;

	u16  scaleH;
	short  objY;

	u16  paddingY;
	u16  imageH;

	u16  imageAdrs;
	u16  imageStride;

	u8   imageFlags;
	u8   imagePal;
	u8   imageSiz;
	u8   imageFmt;
};

//*****************************************************************************
//
//*****************************************************************************
struct	uObjTxSprite
{
	uObjTxtr	txtr;
	uObjSprite	sprite;
};

//*****************************************************************************
//
//*****************************************************************************
enum ESpriteMode
{
	FULL_ROTATION,
	PARTIAL_ROTATION,
	NO_ROTATION
};

static uObjTxtr *gObjTxtr = NULL;
void DLParser_OB_YUV(const uObjSprite *sprite);
//*****************************************************************************
//
//*****************************************************************************
static inline CRefPtr<CNativeTexture> Load_ObjSprite( const uObjSprite *sprite, const uObjTxtr *txtr )
{
	TextureInfo ti;

	// When txtr is NULL, it means TLUT was loaded from ObjLoadTxtr ucode
	if( txtr == NULL )
	{
		// Get ti info from TextureDescriptor since there's no txtr for tile or block (txtr = NULL)
		ti = gRDPStateManager.GetUpdatedTextureDescriptor( gRenderer->GetTextureTile() );
#ifdef DAEDALUS_ACCURATE_TMEM
		// TLUT is loaded from ObjLoadTxtr ucode, so this is a direct load from ram (line = 0)
		ti.SetLine(0);
#endif
	}
	else
	{
		ti.SetFormat           (sprite->imageFmt);
		ti.SetSize             (sprite->imageSiz);
		ti.SetLoadAddress      (RDPSegAddr(txtr->block.image) + (sprite->imageAdrs<<3) );
		//ti.SetLine		   (0);	// Ensure line is 0?

		switch( txtr->block.type )
		{
		case S2DEX_OBJLT_TXTRBLOCK:
			// Worms tries to load huge sprites, need a more robust solution..
			ti.SetWidth            ( ((sprite->imageW >= 0x8000) ? (0x10000-sprite->imageW): sprite->imageW)/32 );
			ti.SetHeight           ( ((sprite->imageH >= 0x8000) ? (0x10000-sprite->imageH): sprite->imageH)/32 );
			ti.SetPitch			   ( (2047/(txtr->block.tline-1)) << 3 );
			break;
		case S2DEX_OBJLT_TXTRTILE:
			ti.SetWidth            (((txtr->tile.twidth+1)>>2)<<(4-sprite->imageSiz));
			ti.SetHeight           ((txtr->tile.theight+1)>>2);
			ti.SetPitch			   ( (sprite->imageSiz == G_IM_SIZ_4b) ? (ti.GetWidth() >> 1) : (ti.GetWidth() << (sprite->imageSiz-1)) );
			break;
		default:
			// This should not happen!
			#ifdef DAEDALUS_DEBUG_CONSOLE
			DAEDALUS_ERROR("Unhandled Obj texture\n");
			#endif
			return NULL;
		}

		ti.SetSwapped          (0);
		ti.SetPalette          (sprite->imagePal);
		ti.SetTlutAddress	   (TLUT_BASE);
		ti.SetTLutFormat       (kTT_RGBA16);
	}

	return gRenderer->LoadTextureDirectly(ti);
}

//*****************************************************************************
//
//*****************************************************************************
static inline void Draw_ObjSprite( const uObjSprite *sprite, ESpriteMode mode, const CNativeTexture * texture )
{
	f32 imageW = sprite->imageW / 32.0f;
	f32 imageH = sprite->imageH / 32.0f;

	f32 scaleW = sprite->scaleW / 1024.0f;
	f32 scaleH = sprite->scaleH / 1024.0f;

	f32 objX = sprite->objX / 4.0f;
	f32 objY = sprite->objY / 4.0f;

	f32 objW = imageW / scaleW + objX;
	f32 objH = imageH / scaleH + objY;

	f32  x0, y0, x1, y1, x2, y2, x3, y3;

	switch( mode )
	{
	case FULL_ROTATION:
		x0 = mat2D.A*objX + mat2D.B*objY + mat2D.X;
		y0 = mat2D.C*objX + mat2D.D*objY + mat2D.Y;
		x2 = mat2D.A*objW + mat2D.B*objH + mat2D.X;
		y2 = mat2D.C*objW + mat2D.D*objH + mat2D.Y;
		x1 = mat2D.A*objW + mat2D.B*objY + mat2D.X;
		y1 = mat2D.C*objW + mat2D.D*objY + mat2D.Y;
		x3 = mat2D.A*objX + mat2D.B*objH + mat2D.X;
		y3 = mat2D.C*objX + mat2D.D*objH + mat2D.Y;
#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT((sprite->imageFlags & 1) == 0, "Need to flip X" );
		DAEDALUS_ASSERT((sprite->imageFlags & 0x10) == 0, "Need to flip Y" );
#endif
		gRenderer->Draw2DTextureR(x0, y0, x1, y1, x2, y2, x3, y3, imageW, imageH);
		break;

	case PARTIAL_ROTATION:
		x0 = mat2D.X + objX / mat2D.BaseScaleX;
		y0 = mat2D.Y + objY / mat2D.BaseScaleY;
		x1 = mat2D.X + objW / mat2D.BaseScaleX - 1.0f;
		y1 = mat2D.Y + objH / mat2D.BaseScaleY - 1.0f;

		// Partial rotation doesn't flip sprites
		gRenderer->Draw2DTexture(x0, y0, x1, y1, 0, 0, imageW, imageH, texture);
		break;

	case NO_ROTATION:
		x0 = objX;
		y0 = objY;
		x1 = objW - 1.0f;
		y1 = objH - 1.0f;

		// Used by Worms
		if (sprite->imageFlags & 1)
			Swap< f32 >( x0, x1 );

		if (sprite->imageFlags & 0x10)
			Swap< f32 >( y0, y1 );

		gRenderer->Draw2DTexture(x0, y0, x1, y1, 0, 0, imageW, imageH, texture);
		break;
	}
}
//*****************************************************************************
//
//*****************************************************************************
// Bomberman : Second Atatck uses this
void DLParser_S2DEX_ObjSprite( MicroCodeCommand command )
{
	uObjSprite *sprite = (uObjSprite*)(g_pu8RamBase + RDPSegAddr(command.inst.cmd1));

	CRefPtr<CNativeTexture> texture = Load_ObjSprite( sprite, NULL );
	Draw_ObjSprite( sprite, FULL_ROTATION, texture );
}

//*****************************************************************************
//
//*****************************************************************************
// Pokemon Puzzle League uses this
// Note : This cmd loads textures from both ObjTxtr and LoadBlock/LoadTile!!
void DLParser_S2DEX_ObjRectangle( MicroCodeCommand command )
{
	uObjSprite *sprite = (uObjSprite*)(g_pu8RamBase + RDPSegAddr(command.inst.cmd1));

	CRefPtr<CNativeTexture> texture = Load_ObjSprite( sprite, gObjTxtr );
	Draw_ObjSprite( sprite, NO_ROTATION, texture );
}

//*****************************************************************************
//
//*****************************************************************************
// Untested.. I can't find any game that uses this.. but it should work fine
void DLParser_S2DEX_ObjRectangleR( MicroCodeCommand command )
{
	uObjSprite *sprite = (uObjSprite*)(g_pu8RamBase + RDPSegAddr(command.inst.cmd1));
	
	if (sprite->imageFmt == G_IM_FMT_YUV)
		DLParser_OB_YUV(sprite);
	
	CRefPtr<CNativeTexture> texture = Load_ObjSprite( sprite, gObjTxtr );
	Draw_ObjSprite( sprite, PARTIAL_ROTATION, texture );
}

//*****************************************************************************
//
//*****************************************************************************
// Nintendo logo, shade, items, enemies & foes, sun, and pretty much everything in Yoshi
void DLParser_S2DEX_ObjLdtxSprite( MicroCodeCommand command )
{
	uObjTxSprite *sprite = (uObjTxSprite*)(g_pu8RamBase + RDPSegAddr(command.inst.cmd1));

	CRefPtr<CNativeTexture> texture = Load_ObjSprite( &sprite->sprite, &sprite->txtr );
	Draw_ObjSprite( &sprite->sprite, FULL_ROTATION, texture );
}

//*****************************************************************************
//
//*****************************************************************************
// No Rotation. Intro logo, Awesome command screens and HUD in game :)
void DLParser_S2DEX_ObjLdtxRect( MicroCodeCommand command )
{
	uObjTxSprite *sprite = (uObjTxSprite*)(g_pu8RamBase + RDPSegAddr(command.inst.cmd1));

	CRefPtr<CNativeTexture> texture = Load_ObjSprite( &sprite->sprite, &sprite->txtr );
	Draw_ObjSprite( &sprite->sprite, NO_ROTATION, texture );
}

//*****************************************************************************
//
//*****************************************************************************
// With Rotation. Text, smoke, and items in Yoshi
void DLParser_S2DEX_ObjLdtxRectR( MicroCodeCommand command )
{
	uObjTxSprite *sprite = (uObjTxSprite*)(g_pu8RamBase + RDPSegAddr(command.inst.cmd1));

	CRefPtr<CNativeTexture> texture = Load_ObjSprite( &sprite->sprite, &sprite->txtr );
	Draw_ObjSprite( &sprite->sprite, PARTIAL_ROTATION, texture );
}

//*****************************************************************************
//
//*****************************************************************************
// Used for Sprite rotation
void DLParser_S2DEX_ObjMoveMem( MicroCodeCommand command )
{
	u32 addr = RDPSegAddr(command.inst.cmd1);
	u32 index = command.inst.cmd0 & 0xFFFF;

	if( index == 0 )	// Mtx
	{
		uObjMtx* mtx = (uObjMtx *)(addr+g_pu8RamBase);
		mat2D.A = mtx->A/65536.0f;
		mat2D.B = mtx->B/65536.0f;
		mat2D.C = mtx->C/65536.0f;
		mat2D.D = mtx->D/65536.0f;
		mat2D.X = f32(mtx->X>>2);
		mat2D.Y = f32(mtx->Y>>2);
		mat2D.BaseScaleX = mtx->BaseScaleX/1024.0f;
		mat2D.BaseScaleY = mtx->BaseScaleY/1024.0f;
	}
	else if( index == 2 )	// Sub Mtx
	{
		uObjSubMtx* sub = (uObjSubMtx*)(addr+g_pu8RamBase);
		mat2D.X = f32(sub->X>>2);
		mat2D.Y = f32(sub->Y>>2);
		mat2D.BaseScaleX = sub->BaseScaleX/1024.0f;
		mat2D.BaseScaleY = sub->BaseScaleY/1024.0f;
	}
}

//*****************************************************************************
//
//*****************************************************************************
// Kirby uses this for proper palette loading
void DLParser_S2DEX_ObjLoadTxtr( MicroCodeCommand command )
{
	uObjTxtr* ObjTxtr = (uObjTxtr*)(g_pu8RamBase + RDPSegAddr(command.inst.cmd1));
	
	g_TI.Format		= G_IM_FMT_RGBA;
	g_TI.Size		= G_IM_SIZ_16b;
	
	switch (ObjTxtr->block.type) {
		case S2DEX_OBJLT_TXTRBLOCK:
			g_TI.Width		= ObjTxtr->block.tsize + 1;
			g_TI.Address	= RDPSegAddr(ObjTxtr->block.image);
			gObjTxtr = (uObjTxtr*)ObjTxtr;
			break;
		case S2DEX_OBJLT_TXTRTILE:
			g_TI.Width		= ObjTxtr->tile.twidth + 1;
			g_TI.Address	= RDPSegAddr(ObjTxtr->tile.image);
			gObjTxtr = (uObjTxtr*)ObjTxtr;
			break;
		case S2DEX_OBJLT_TLUT:
			g_TI.Width		= 1;
			g_TI.Address	= RDPSegAddr(ObjTxtr->tlut.image);
			gTlutLoadAddresses[ (ObjTxtr->tlut.phead>>2) & 0x3F ] = (u32*)(g_pu8RamBase + RDPSegAddr(ObjTxtr->tlut.image));
			gObjTxtr = NULL;
			break;
		default:
			break;
	}
}

//*****************************************************************************
//
//*****************************************************************************
inline void DLParser_Yoshi_MemRect( MicroCodeCommand command )
{
	//
	// Fetch the next two instructions
	//
	u32 pc = gDlistStack.address[gDlistStackPointer];
	u32 * pCmdBase = (u32 *)( g_pu8RamBase + pc );
	gDlistStack.address[gDlistStackPointer]+= 16;

	RDP_MemRect mem_rect;
	mem_rect.cmd0 = command.inst.cmd0;
	mem_rect.cmd1 = command.inst.cmd1;
	mem_rect.cmd2 = pCmdBase[1];

	const RDP_Tile & rdp_tile( gRDPStateManager.GetTile( mem_rect.tile_idx ) );

	u32	x0 = mem_rect.x0;
	u32	y0 = mem_rect.y0;
	u32	y1 = mem_rect.y1;

	// Get base address of texture
	u32 tile_addr = gRDPStateManager.GetTileAddress( rdp_tile.tmem );

	if (y1 > scissors.bottom)
		y1 = scissors.bottom;
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DL_PF ("    MemRect->Addr[0x%08x] (%d, %d -> %d, %d) Width[%d]", tile_addr, x0, y0, mem_rect.x1, y1, g_CI.Width);
#endif
#if 1	//1->Optimized, 0->Generic
	// This assumes Yoshi always copy 16 bytes per line and dst is aligned and we force alignment on src!!! //Corn
	u32 tex_width = rdp_tile.line << 3;
	u32 texaddr = ((u32)g_pu8RamBase + tile_addr + tex_width * (mem_rect.s >> 5) + (mem_rect.t >> 5) + 3) & ~3;
	u32 fbaddr = (u32)g_pu8RamBase + g_CI.Address + x0;

	for (u32 y = y0; y < y1; y++)
	{
		u32 *src = (u32*)(texaddr + (y - y0) * tex_width);
		u32 *dst = (u32*)(fbaddr + y * g_CI.Width);

		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
	}
#else
	u32 width = x1 - x0;
	u32 tex_width = rdp_tile.line << 3;
	u8 * texaddr = g_pu8RamBase + tile_addr + tex_width * (mem_rect.s >> 5) + (mem_rect.t >> 5);
	u8 * fbaddr = g_pu8RamBase + g_CI.Address + x0;

	for (u32 y = y0; y < y1; y++)
	{
		u8 *src = texaddr + (y - y0) * tex_width;
		u8 *dst = fbaddr + y * g_CI.Width;
		memcpy(dst, src, width);
	}
#endif

}

static u16 YUVtoRGBA(u8 y, u8 u, u8 v)
{
	f32 r = y + (1.370705f * (v-128));
	f32 g = y - (0.698001f * (v-128)) - (0.337633f * (u-128));
	f32 b = y + (1.732446f * (u-128));
	r *= 0.125f;
	g *= 0.125f;
	b *= 0.125f;

	//clipping the result
	if (r > 31) r = 31;
	if (g > 31) g = 31;
	if (b > 31) b = 31;
	if (r < 0) r = 0;
	if (g < 0) g = 0;
	if (b < 0) b = 0;
	
	return (u16)(((u16)(r) << 11) |((u16)(g) << 6) |((u16)(b) << 1) | 1);
}

//Ogre Battle needs to copy YUV texture to frame buffer
void DLParser_OB_YUV(const uObjSprite *sprite)
{
	f32 imageW = sprite->imageW / 32.0f;
	f32 imageH = sprite->imageH / 32.0f;
	f32 scaleW = sprite->scaleW / 1024.0f;
	f32 scaleH = sprite->scaleH / 1024.0f;

	f32 objX = sprite->objX / 4.0f;
	f32 objY = sprite->objY / 4.0f;

	u16 ul_x = (u16)(objX/mat2D.BaseScaleX + mat2D.X);
	u16 lr_x = (u16)((objX + imageW/scaleW)/mat2D.BaseScaleX + mat2D.X);
	u16 ul_y = (u16)(objY/mat2D.BaseScaleY + mat2D.Y);
	u16 lr_y = (u16)((objY + imageH/scaleH)/mat2D.BaseScaleY + mat2D.Y);

	u32 ci_width = g_CI.Width;
	u32 ci_height = scissors.bottom;

	if ((ul_x >= ci_width) || (ul_y >= ci_height))
		return;

	u32 width = 16;
	u32 height = 16;

	if (lr_x > ci_width)	width = ci_width - ul_x;
	if (lr_y > ci_height)	height = ci_height - ul_y;

	u32 *mb = (u32*)(g_pu8RamBase + g_TI.Address); //pointer to the first macro block
	u16 *dst = (u16*)(g_pu8RamBase + g_CI.Address);
	dst += ul_x + ul_y * ci_width;
	
	
	//yuv macro block contains 16x16 texture. we need to put it in the proper place inside cimg
	for (u16 h = 0; h < 16; h++)
	{
		for (u16 w = 0; w < 16; w+=2)
		{
			u32 t = *(mb++); //each u32 contains 2 pixels
			if ((h < height) && (w < width)) //clipping. texture image may be larger than color image
			{
				u8 y0 = (u8)(t      ) & 0xFF;
				u8 v  = (u8)(t >> 8 ) & 0xFF;
				u8 y1 = (u8)(t >> 16) & 0xFF;
				u8 u  = (u8)(t >> 24) & 0xFF;
				*(dst++) = YUVtoRGBA(y0, u, v);
				*(dst++) = YUVtoRGBA(y1, u, v);
			}
		}
		dst += ci_width - 16;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_S2DEX_RDPHalf_0( MicroCodeCommand command )
{
	//RDP: RSP_S2DEX_RDPHALF_0 (0xe449c0a8 0x003b40a4)
	//0x001d3c88: e449c0a8 003b40a4 RDP_TEXRECT
	//0x001d3c90: b4000000 00000000 RSP_RDPHALF_1
	//0x001d3c98: b3000000 04000400 RSP_RDPHALF_2

	if( g_ROM.GameHacks == YOSHI )
		DLParser_Yoshi_MemRect( command );
	else
		DLParser_TexRect( command );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_S2DEX_ObjRendermode( MicroCodeCommand command )
{
	#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DL_PF( "    S2DEX_ObjRendermode (Ignored)" );
	#endif
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_S2DEX_SelectDl( MicroCodeCommand command )
{
	#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DL_PF( "    S2DEX_SelectDl (Ignored)" );
	#endif
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_S2DEX_BgCopy( MicroCodeCommand command )
{
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DL_PF("    DLParser_S2DEX_BgCopy");
#endif
	uObjBg *objBg = (uObjBg*)(g_pu8RamBase + RDPSegAddr(command.inst.cmd1));

	u16 imageX = objBg->imageX >> 5;
	u16 imageY = objBg->imageY >> 5;

	u16 imageW = objBg->imageW >> 2;
	u16 imageH = objBg->imageH >> 2;

	s16 frameX = objBg->frameX >> 2;
	s16 frameY = objBg->frameY >> 2;
	u16 frameW = (objBg->frameW >> 2) + frameX;
	u16 frameH = (objBg->frameH >> 2) + frameY;

	TextureInfo ti;

	ti.SetFormat           (objBg->imageFmt);
	ti.SetSize             (objBg->imageSiz);

	ti.SetLoadAddress      (RDPSegAddr(objBg->imagePtr));
	ti.SetWidth            (imageW);
	ti.SetHeight           (imageH);
	ti.SetPitch			   (((imageW << objBg->imageSiz >> 1)>>3)<<3); //force 8-bit alignment

	ti.SetSwapped          (0);

	ti.SetPalette          (objBg->imagePal);
	ti.SetTlutAddress	   (TLUT_BASE);
	ti.SetTLutFormat       (kTT_RGBA16);

	CRefPtr<CNativeTexture> texture = gRenderer->LoadTextureDirectly(ti);
	gRenderer->Draw2DTexture( (float)frameX, (float)frameY, (float)frameW, (float)frameH,
							  (float)imageX, (float)imageY, (float)imageW, (float)imageH,
							  texture );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_S2DEX_Bg1cyc( MicroCodeCommand command )
{
	if( g_ROM.GameHacks == ZELDA_MM )
		return;

	uObjScaleBg *objBg = (uObjScaleBg *)(g_pu8RamBase + RDPSegAddr(command.inst.cmd1));

	f32 frameX = objBg->frameX / 4.0f;
	f32 frameY = objBg->frameY / 4.0f;

	f32 frameW = (objBg->frameW / 4.0f) + frameX;
	f32 frameH = (objBg->frameH / 4.0f) + frameY;

	f32 imageX = objBg->imageX / 32.0f;
	f32 imageY = objBg->imageY / 32.0f;

	f32 scaleX = objBg->scaleW/1024.0f;
	f32 scaleY = objBg->scaleH/1024.0f;

	f32 imageW = objBg->imageW/4.0f;
	f32 imageH = objBg->imageH/4.0f;

	TextureInfo ti;

	ti.SetFormat           (objBg->imageFmt);
	ti.SetSize             (objBg->imageSiz);

	ti.SetLoadAddress      (RDPSegAddr(objBg->imagePtr));
	ti.SetWidth            (objBg->imageW/4);
	ti.SetHeight           (objBg->imageH/4);
	ti.SetPitch			   (((objBg->imageW/4 << ti.GetSize() >> 1)>>3)<<3); //force 8-bit alignment, this what sets our correct viewport.

	ti.SetSwapped          (0);

	ti.SetPalette		   (objBg->imagePal);
	ti.SetTlutAddress	   (TLUT_BASE);
	ti.SetTLutFormat       (kTT_RGBA16);

	CRefPtr<CNativeTexture> texture = gRenderer->LoadTextureDirectly(ti);

	if (g_ROM.GameHacks != YOSHI)
	{
		f32 s1 = (frameW-frameX)*scaleX + imageX;
		f32 t1 = (frameH-frameY)*scaleY + imageY;

		gRenderer->Draw2DTexture( frameX, frameY, frameW, frameH, imageX, imageY, s1, t1, texture );
	}
	else
	{
		f32 x2 = frameX + (imageW-imageX)/scaleX;
		f32 y2 = frameY + (imageH-imageY)/scaleY;

		f32 u1 = (frameW-x2)*scaleX;
		f32 v1 = (frameH-y2)*scaleY;

		gRenderer->Draw2DTexture(frameX, frameY, x2, y2, imageX, imageY, imageW, imageH, texture);
		gRenderer->Draw2DTexture(x2, frameY, frameW, y2, 0, imageY, u1, imageH, texture);
		gRenderer->Draw2DTexture(frameX, y2, x2, frameH, imageX, 0, imageW, v1, texture);
		gRenderer->Draw2DTexture(x2, y2, frameW, frameH, 0, 0, u1, v1, texture);
	}
}

#endif // HLEGRAPHICS_UCODES_UCODE_S2DEX_H_
