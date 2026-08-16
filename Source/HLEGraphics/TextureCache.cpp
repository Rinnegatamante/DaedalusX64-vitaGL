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

// Manages textures for RDP code
// Uses a HashTable (hashing on TImg) to allow quick access
//  to previously used textures

#include "stdafx.h"

#include "TextureCache.h"
#include "TextureInfo.h"

#include "DLDebug.h"

#include <vector>
#include <algorithm>

//#define PROFILE_TEXTURE_CACHE

template<> bool CSingleton< CTextureCache >::Create()
{
	mpInstance = new CTextureCache();
	return mpInstance != nullptr;
}

CTextureCache::CTextureCache()
{
	memset( mpCacheHashTable, 0, sizeof(mpCacheHashTable) );
}

CTextureCache::~CTextureCache()
{
	DropTextures();
}

inline u32 CTextureCache::MakeHashIdxA( const TextureInfo & ti )
{
	u32 address( ti.GetLoadAddress() );
	u32 hash( (address >> (HASH_TABLE_BITS*2)) ^ (address >> HASH_TABLE_BITS) ^ address );

	hash ^= ti.GetPalette() >> 2;			// Useful for palettised fonts, e.g in Starfox

	return hash & (HASH_TABLE_SIZE-1);
}

inline u32 CTextureCache::MakeHashIdxB( const TextureInfo & ti )
{
	return ti.GetHashCode() & (HASH_TABLE_SIZE-1);
}

// Purge any textures that haven't been used recently
void CTextureCache::PurgeOldTextures()
{
	MutexLock lock(GetDebugMutex());

	//
	//	Erase expired textures in reverse order, which should require less
	//	copying when large clumps of textures are released simultaneously.
	//
	for( s32 i = mTextures.size() - 1; i >= 0; --i )
	{
		CachedTexture * texture = mTextures[ i ];
		if ( texture->HasExpired() )
		{
			u32	ixa = MakeHashIdxA( texture->GetTextureInfo() );
			u32 ixb = MakeHashIdxB( texture->GetTextureInfo() );

			if( mpCacheHashTable[ixa] == texture )
			{
				mpCacheHashTable[ixa] = nullptr;
			}
			if( mpCacheHashTable[ixb] == texture )
			{
				mpCacheHashTable[ixb] = nullptr;
			}

			mTextures.erase( mTextures.begin() + i );

			delete texture;
		}
	}
}

void CTextureCache::DropTextures()
{
	MutexLock lock(GetDebugMutex());

	for( u32 i {}; i < mTextures.size(); ++i)
	{
		delete mTextures[i];
	}
	mTextures.clear();
	for( u32 i {}; i < HASH_TABLE_SIZE; ++i )
	{
		mpCacheHashTable[i] = nullptr;
	}
}

struct SSortTextureEntries
{
public:
	bool operator()( const TextureInfo & a, const TextureInfo & b ) const
	{
		return a < b;
	}
	bool operator()( const CachedTexture * a, const TextureInfo & b ) const
	{
		return a->GetTextureInfo() < b;
	}
	bool operator()( const TextureInfo & a, const CachedTexture * b ) const
	{
		return a < b->GetTextureInfo();
	}
	bool operator()( const CachedTexture * a, const CachedTexture * b ) const
	{
		return a->GetTextureInfo() < b->GetTextureInfo();
	}
};

// If already in table, return cached copy
// Otherwise, create surfaces, and load texture into memory
CachedTexture * CTextureCache::GetOrCreateCachedTexture(const TextureInfo & ti)
{
	if (ti.GetWidth() > 4096 || ti.GetHeight() > 4096)
	{
		return nullptr;
	}
	
	// NB: this is a no-op in normal builds.
	MutexLock lock(GetDebugMutex());

	//
	// Retrieve the texture from the cache (if it already exists)
	//
	u32	ixa = MakeHashIdxA( ti );
	if( mpCacheHashTable[ixa] && mpCacheHashTable[ixa]->GetTextureInfo() == ti )
	{
		mpCacheHashTable[ixa]->UpdateIfNecessary();

		return mpCacheHashTable[ixa];
	}

	u32 ixb {MakeHashIdxB( ti )};
	if( mpCacheHashTable[ixb] && mpCacheHashTable[ixb]->GetTextureInfo() == ti )
	{
		mpCacheHashTable[ixb]->UpdateIfNecessary();

		return mpCacheHashTable[ixb];
	}

	CachedTexture *	texture = nullptr;
	TextureVec::iterator	it = std::lower_bound( mTextures.begin(), mTextures.end(), ti, SSortTextureEntries() );
	if( it != mTextures.end() && (*it)->GetTextureInfo() == ti )
	{
		texture = *it;
	}
	else
	{
		texture = CachedTexture::Create( ti );
		if (texture != nullptr)
		{
			mTextures.insert( it, texture );
		}
	}

	// Update the hashtable
	if( texture )
	{
		texture->UpdateIfNecessary();

		mpCacheHashTable[ixa] = texture;
		mpCacheHashTable[ixb] = texture;
	}

	return texture;
}

CRefPtr<CNativeTexture> CTextureCache::GetOrCreateTexture(const TextureInfo & ti)
{
	CachedTexture * base_texture = GetOrCreateCachedTexture(ti);
	if (!base_texture)
		return nullptr;

	return base_texture->GetTexture();
}
