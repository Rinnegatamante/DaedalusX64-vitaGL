/*
Copyright (C) 2005-2007 StrmnNrmn

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
#include "Graphics/NativeTexture.h"
#include "Graphics/NativePixelFormat.h"
#include "Graphics/ColourValue.h"
#include "Utility/FastMemcpy.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "Utility/stb_image_write.h"

#include "Math/MathUtil.h"

#include <png.h>

#include <vitaGL.h>

static const u32 kPalette4BytesRequired = 16 * sizeof( NativePf8888 );
static const u32 kPalette8BytesRequired = 256 * sizeof( NativePf8888 );

static u32 GetTextureBlockWidth( u32 dimension, ETextureFormat texture_format )
{
	DAEDALUS_ASSERT( GetNextPowerOf2( dimension ) == dimension, "This is not a power of 2" );

	// Ensure that the pitch is at least 16 bytes
	while( CalcBytesRequired( dimension, texture_format ) < 16 )
	{
		dimension *= 2;
	}

	return dimension;
}

static inline u32 CorrectDimension( u32 dimension )
{
	static const u32 MIN_TEXTURE_DIMENSION = 1;
	return Max( GetNextPowerOf2( dimension ), MIN_TEXTURE_DIMENSION );
}

CRefPtr<CNativeTexture>	CNativeTexture::Create( u32 width, u32 height, ETextureFormat texture_format )
{
	return new CNativeTexture( width, height, texture_format );
}

CNativeTexture::CNativeTexture( u32 w, u32 h, ETextureFormat texture_format )
:	mTextureFormat( texture_format )
,	mWidth( w )
,	mHeight( h )
,	mCorrectedWidth( CorrectDimension( w ) )
,	mCorrectedHeight( CorrectDimension( h ) )
,	mTextureBlockWidth( GetTextureBlockWidth( mCorrectedWidth, texture_format ) )
,	mTextureId( 0 )
,	hasMipmaps( false )
{
	mScale.x = 1.0f / (float)mCorrectedWidth;
	mScale.y = 1.0f / (float)mCorrectedHeight;
	
	glGenTextures( 1, &mTextureId );
}

CNativeTexture::~CNativeTexture()
{
	glDeleteTextures( 1, &mTextureId );
}

bool CNativeTexture::HasData() const
{
	return mTextureId != 0;
}

void CNativeTexture::InstallTexture() const
{
	glBindTexture( GL_TEXTURE_2D, mTextureId );
}

void CNativeTexture::GenerateMipmaps()
{
	if (!hasMipmaps) {
		glGenerateMipmap(GL_TEXTURE_2D);
		hasMipmaps = true;
	}
}

void CNativeTexture::SetData( void * data, void * palette )
{
	// It's pretty gross that we don't pass this in, or better yet, provide a way for
	// the caller to write directly to our buffers instead of setting the data.
	size_t data_len = GetBytesRequired();

	if (HasData())
	{
		glBindTexture( GL_TEXTURE_2D, mTextureId );
		switch (mTextureFormat)
		{
/*		case TexFmt_5650:
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
						  mCorrectedWidth, mCorrectedHeight,
						  0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, data );
			break;*/
		case TexFmt_5551:
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
						  mCorrectedWidth, mCorrectedHeight,
						  0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, data );
			mpData = vglGetTexDataPointer(GL_TEXTURE_2D);
			break;
/*		case TexFmt_4444:
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
						  mCorrectedWidth, mCorrectedHeight,
						  0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4_REV, data );

			break;*/
		case TexFmt_8888:
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
						  mCorrectedWidth, mCorrectedHeight,
						  0, GL_RGBA, GL_UNSIGNED_BYTE, data );
			mpData = vglGetTexDataPointer(GL_TEXTURE_2D);
			break;
		case TexFmt_CI4_8888:
			{
				// Convert palletised texture to non-palletised. This is wsteful - we should avoid generating these updated for OSX.
				const NativePfCI44 * pix_ptr = static_cast< const NativePfCI44 * >( data );
				const NativePf8888 * pal_ptr = static_cast< const NativePf8888 * >( palette );

				NativePf8888 * out = static_cast<NativePf8888 *>( malloc(mCorrectedWidth * mCorrectedHeight * sizeof(NativePf8888)) );
				NativePf8888 * out_ptr = out;

				u32 pitch = GetStride();

				for (u32 y = 0; y < mCorrectedHeight; ++y)
				{
					for (u32 x = 0; x < mCorrectedWidth; ++x)
					{
						NativePfCI44	colors  = pix_ptr[ x / 2 ];
						u8				pal_idx = (x & 1) ? colors.GetIdxA() : colors.GetIdxB();

						*out_ptr = pal_ptr[ pal_idx ];
						out_ptr++;
					}

					pix_ptr = reinterpret_cast<const NativePfCI44 *>( reinterpret_cast<const u8 *>(pix_ptr) + pitch );
				}

				glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
							  mCorrectedWidth, mCorrectedHeight,
							  0, GL_RGBA, GL_UNSIGNED_BYTE, out );

				free(out);
				mpData = vglGetTexDataPointer(GL_TEXTURE_2D);
			}
			break;
		case TexFmt_CI8_8888:
			{
				// Convert palletised texture to non-palletised. This is wsteful - we should avoid generating these updated for OSX.
				const NativePfCI8 *  pix_ptr = static_cast< const NativePfCI8 * >( data );
				const NativePf8888 * pal_ptr = static_cast< const NativePf8888 * >( palette );

				NativePf8888 * out = static_cast<NativePf8888 *>( malloc(mCorrectedWidth * mCorrectedHeight * sizeof(NativePf8888)) );
				NativePf8888 * out_ptr = out;

				u32 pitch = GetStride();

				for (u32 y = 0; y < mCorrectedHeight; ++y)
				{
					for (u32 x = 0; x < mCorrectedWidth; ++x)
					{
						u8	pal_idx = pix_ptr[ x ].Bits;

						*out_ptr = pal_ptr[ pal_idx ];
						out_ptr++;
					}

					pix_ptr = reinterpret_cast<const NativePfCI8 *>( reinterpret_cast<const u8 *>(pix_ptr) + pitch );
				}

				glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
							  mCorrectedWidth, mCorrectedHeight,
							  0, GL_RGBA, GL_UNSIGNED_BYTE, out );

				free(out);
				mpData = vglGetTexDataPointer(GL_TEXTURE_2D);
			}
			break;
		default:
			printf("Unsupported texture format used %ld\n", mTextureFormat);
			break;
		}
	}
}

void CNativeTexture::Dump(const char *filename)
{
	stbi_write_png(filename, mCorrectedWidth, mCorrectedHeight, 4, mpData, mCorrectedWidth * 4);
}

u32	CNativeTexture::GetStride() const
{
	return CalcBytesRequired( mTextureBlockWidth, mTextureFormat );
}

u32 CNativeTexture::GetBytesRequired() const
{
	return GetStride() * mCorrectedHeight;
}
