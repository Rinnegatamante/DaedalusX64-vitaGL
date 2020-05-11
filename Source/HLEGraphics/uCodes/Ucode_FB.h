/*
Copyright (C) 2013 StrmnNrmn

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

#ifndef HLEGRAPHICS_UCODES_UCODE_FB_H_
#define HLEGRAPHICS_UCODES_UCODE_FB_H_

#if !defined(DAEDALUS_PSP)
static inline CRefPtr<CNativeTexture> LoadFrameBuffer(u32 origin)
{
	u32 width  = Memory_VI_GetRegister( VI_WIDTH_REG );
	if( width == 0 )
	{
		//DAEDALUS_ERROR("Loading 0 size frame buffer?");
		return NULL;
	}

	if( origin <= width*2 )
	{
		//DAEDALUS_ERROR("Loading small frame buffer not supported");
		return NULL;
	}
	//ToDO: We should use uViWidth+1 and uViHeight+1
#define FB_WIDTH  320
#define FB_HEIGHT 240

#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT(g_CI.Size == G_IM_SIZ_16b,"32b frame buffer is not supported");
	//DAEDALUS_ASSERT((uViWidth+1) == FB_WIDTH,"Variable width is not handled");
	//DAEDALUS_ASSERT((uViHeight+1) == FB_HEIGHT,"Variable height is not handled");
#endif
	TextureInfo ti;

	ti.SetSwapped			(0);
	ti.SetPalette			(0);
	ti.SetTlutAddress		(TLUT_BASE);
	ti.SetTLutFormat		(kTT_RGBA16);
	ti.SetFormat			(0);
	ti.SetSize				(2);

	ti.SetLoadAddress		(origin - width * 2);
	ti.SetWidth				(FB_WIDTH);
	ti.SetHeight			(FB_HEIGHT);
	ti.SetPitch				((width << 2) >> 1);

	return gRenderer->LoadTextureDirectly(ti);
}

void RenderFrameBuffer(u32 origin)
{
	gRenderer->BeginScene();

	CRefPtr<CNativeTexture> texture = LoadFrameBuffer(origin);
	if(texture != NULL)
		gRenderer->Draw2DTexture(0, 0, FB_WIDTH, FB_HEIGHT, 0, 0, FB_WIDTH, FB_HEIGHT, texture);
	
	gRenderer->EndScene();
	gGraphicsPlugin->UpdateScreen();
}

#endif // !DAEDALUS_PSP
#endif // HLEGRAPHICS_UCODES_UCODE_FB_H_
