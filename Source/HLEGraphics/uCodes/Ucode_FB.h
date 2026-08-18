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
static bool fb_re2_video_hack;
static u32 fb_source_width;
static u32 fb_source_height;
static u32 fb_source_stride;

static bool LoadFrameBuffer(u32 origin)
{
	// TODO: This whole function could be optimized by adopting a double/triple buffering approach and re-usage of the pixels buffers
	u32 width = Memory_VI_GetRegister(VI_WIDTH_REG);
	u32 vi_type = Memory_VI_GetRegister(VI_CONTROL_REG) & 0x3;

	if (width == 0 || (origin <= width * 2) || g_ROM.SKIP_CPU_REND_HACK)
		return false;

	if (vi_type != 2 && vi_type != 3)
		return false;

	fb_re2_video_hack = g_ROM.rh.CartID == 0x4552 && vi_type == 3 && width == 264;

	// Hack to deal with RE2 real fraembuffer size during video playback
	if (fb_re2_video_hack)
	{
		fb_ratio = 1.0f;
		fb_source_width = 240;
		fb_source_height = 120;
	}
	else
	{
		fb_ratio = (float)FB_WIDTH / (float)width;
		fb_source_width = width;
		fb_source_height = (u32)((float)FB_HEIGHT * fb_ratio);
	}
	fb_source_stride = width;

	CRefPtr<CNativeTexture> texture = CNativeTexture::Create(fb_source_width, fb_source_height, TexFmt_8888);
	u32 tex_width = texture->GetCorrectedWidth();
	u32 tex_height = texture->GetCorrectedHeight();
	texture->InstallTexture();
	gRenderer->mBoundTexture[0] = texture;

	if (vi_type == 2) // RGBA5551
	{
		u16 *pixels = (u16 *)malloc(tex_width * tex_height * sizeof(u16));
		memset(pixels, 0, tex_width * tex_height * sizeof(u16));

		for (u32 y = 0; y < fb_source_height; ++y)
		{
			u32 src_offset = y * fb_source_stride * 2;
			for (u32 x = 0; x < fb_source_width; ++x)
			{
				u32 addr = origin + src_offset + x * 2;
				pixels[y * tex_width + x] =
					(g_pu8RamBase[addr ^ U8_TWIDDLE] << 8) |
					g_pu8RamBase[(addr + 1) ^ U8_TWIDDLE] | 1;
			}
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0,
			GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, pixels);
		free(pixels);
	}
	else // RGBA8888
	{
		u8 *pixels = (u8 *)malloc(tex_width * tex_height * 4);
		memset(pixels, 0, tex_width * tex_height * 4);

		for (u32 y = 0; y < fb_source_height; ++y)
		{
			u32 src_offset = y * fb_source_stride * 4;
			for (u32 x = 0; x < fb_source_width; ++x)
			{
				u32 addr = origin + src_offset + x * 4;
				u8 *dst = pixels + ((y * tex_width + x) << 2);
				dst[0] = g_pu8RamBase[addr ^ U8_TWIDDLE];
				dst[1] = g_pu8RamBase[(addr + 1) ^ U8_TWIDDLE];
				dst[2] = g_pu8RamBase[(addr + 2) ^ U8_TWIDDLE];
				dst[3] = 0xff;
			}
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		free(pixels);
	}

	return true;
}

void RenderFrameBuffer(u32 origin)
{
	gRenderer->BeginScene();

	if (LoadFrameBuffer(origin))
	{
		// HACK: Upscaling to fullscreen RE2 video playback frames
		if (fb_re2_video_hack)
		{
			gRenderer->ForceViewport(320.0f, 240.0f);
			gRenderer->Draw2DTexture(
				0.0f, 40.0f, 320.0f, 200.0f,
				0.0f, 0.0f, 240.0f, 120.0f);
		}
		else
		{
			gRenderer->ForceViewport((float)fb_source_width, (float)fb_source_height);
			gRenderer->Draw2DTexture(
				0.0f, 0.0f, (float)fb_source_width, (float)fb_source_height,
				0.0f, 0.0f, (float)fb_source_width, (float)fb_source_height);
		}
	}

	gRenderer->EndScene();
	CGraphicsContext::Get()->UpdateFrame(false);
}

#endif // HLEGRAPHICS_UCODES_UCODE_FB_H_
