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
		//DBGConsole_Msg(0, "Loading 0 size frame buffer?");
		return NULL;
	}

	if( origin <= width*2 )
	{
		//DBGConsole_Msg(0, "Loading small frame buffer not supported");
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
	if(texture != NULL) {
		
		u32 corrected_width = GetNextPowerOf2(FB_WIDTH);
		u32 corrected_height = GetNextPowerOf2(FB_HEIGHT);
		
		// LoadFrameBuffer caching makes the framebuffer not properly update, so we do it manually
		u16 * pixels = (u16*)malloc(corrected_width * corrected_height * sizeof(u16));
		u32 src_offset = 0;

		for (u32 y = 0; y < FB_HEIGHT; ++y)
		{
			u32 dst_row_offset = y * corrected_width;
			u32 dst_offset     = dst_row_offset;

			for (u32 x = 0; x < Memory_VI_GetRegister( VI_WIDTH_REG ); ++x)
			{
				pixels[dst_offset] = (g_pu8RamBase[(origin + src_offset)^U8_TWIDDLE]<<8) | g_pu8RamBase[(origin + src_offset+  1)^U8_TWIDDLE] | 1;  // NB: or 1 to ensure we have alpha
				dst_offset += 1;
				src_offset += 2;
			}
		}
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, corrected_width, corrected_height, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, pixels);

		gRenderer->Draw2DTexture(FB_WIDTH - Memory_VI_GetRegister( VI_WIDTH_REG ), 0, FB_WIDTH, FB_HEIGHT, 0, 0, FB_WIDTH, FB_HEIGHT, texture);
		free(pixels);
	}
	gRenderer->EndScene();
	CGraphicsContext::Get()->UpdateFrame( false );
}

#endif // !DAEDALUS_PSP
#endif // HLEGRAPHICS_UCODES_UCODE_FB_H_
