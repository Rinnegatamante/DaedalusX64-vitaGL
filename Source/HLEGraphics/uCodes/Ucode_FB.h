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

#define FB_WIDTH  320
#define FB_HEIGHT 240

static float fb_ratio;

#if !defined(DAEDALUS_PSP)
static inline CRefPtr<CNativeTexture> LoadFrameBuffer(u32 origin)
{
	u32 width  = Memory_VI_GetRegister( VI_WIDTH_REG );
	
	if (width == 0 || (origin <= width*2) || g_ROM.SKIP_CPU_REND_HACK)
		return NULL;
	
	fb_ratio = (float)FB_WIDTH / (float)width;
	u32 height = (u32)((float)FB_HEIGHT * fb_ratio);
	
	CRefPtr<CNativeTexture> texture = CNativeTexture::Create(width, height, TexFmt_8888 );
	u32 tex_width = texture->GetCorrectedWidth();
	u32 tex_height = texture->GetCorrectedHeight();
	texture->InstallTexture();
	
	u16 *pixels = (u16*)malloc(tex_width * tex_height * sizeof(u16));
	u32 src_offset = 0;

	for (u32 y = 0; y < height; ++y)
	{
		u32 dst_row_offset = y * tex_width;
		u32 dst_offset     = dst_row_offset;

		for (u32 x = 0; x < width; ++x)
		{
			pixels[dst_offset] = (g_pu8RamBase[(origin + src_offset)^U8_TWIDDLE]<<8) | g_pu8RamBase[(origin + src_offset+  1)^U8_TWIDDLE] | 1;  // NB: or 1 to ensure we have alpha
			dst_offset += 1;
			src_offset += 2;
		}
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, pixels);
	free(pixels);
	
	return texture;
}

void RenderFrameBuffer(u32 origin)
{
	gRenderer->BeginScene();
	
	CRefPtr<CNativeTexture> texture = LoadFrameBuffer(origin);
	if(texture != NULL) {
		gRenderer->ForceViewport(Memory_VI_GetRegister( VI_WIDTH_REG ), (float)FB_HEIGHT * fb_ratio);
		gRenderer->Draw2DTexture(0, 0, Memory_VI_GetRegister( VI_WIDTH_REG ), (float)FB_HEIGHT * fb_ratio, 0, 0, Memory_VI_GetRegister( VI_WIDTH_REG ), FB_HEIGHT, texture);
	}

	gRenderer->EndScene();
	CGraphicsContext::Get()->UpdateFrame( false );
}

#endif // !DAEDALUS_PSP
#endif // HLEGRAPHICS_UCODES_UCODE_FB_H_
