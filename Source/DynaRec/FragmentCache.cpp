/*
Copyright (C) 2006 StrmnNrmn

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
#include "FragmentCache.h"

#include <stdio.h>

#include <algorithm>

#include "Fragment.h"
#include "CodeBufferManager.h"

#include "Utility/IO.h"

#include "AssemblyUtils.h"

using namespace AssemblyUtils;

//*************************************************************************************
//
//*************************************************************************************
CFragmentCache::CFragmentCache()
:	mCachedFragmentAddress( 0 )
,	mpCachedFragment( nullptr )
{
	memset( mpCacheHashTable, 0, sizeof(mpCacheHashTable) );

	mFragments.reserve( 2000 );

	mpCodeBufferManager = CCodeBufferManager::Create();
	if(mpCodeBufferManager != nullptr)
	{
		mpCodeBufferManager->Initialise();
	}
}

//*************************************************************************************
//
//*************************************************************************************
CFragmentCache::~CFragmentCache()
{
	Clear();

	mpCodeBufferManager->Finalise();
	delete mpCodeBufferManager;
}

//*************************************************************************************
//
//*************************************************************************************
CFragment * CFragmentCache::LookupFragmentQ( u32 address ) const
{
	if( address != mCachedFragmentAddress )
	{
		mCachedFragmentAddress = address;

		// check if in hash table
		u32 ix {MakeHashIdx( address )};

		if ( address != mpCacheHashTable[ix].addr )
		{
			SFragmentEntry				entry( address, nullptr );
			FragmentVec::const_iterator	it( std::lower_bound( mFragments.begin(), mFragments.end(), entry ) );
			if( it != mFragments.end() && it->Address == address )
			{
				mpCachedFragment = it->Fragment;
			}
			else
			{
				mpCachedFragment = nullptr;
			}

			// put in hash table
			mpCacheHashTable[ix].addr = address;
			mpCacheHashTable[ix].ptr = reinterpret_cast< u32 >( mpCachedFragment );
		}
		else
		{
			mpCachedFragment = reinterpret_cast< CFragment * >( mpCacheHashTable[ix].ptr );
		}

	}

	return mpCachedFragment;
}

//*************************************************************************************
//
//*************************************************************************************
void CFragmentCache::InsertFragment( CFragment * p_fragment )
{
	u32		fragment_address( p_fragment->GetEntryAddress() );

	mCacheCoverage.ExtendCoverage( fragment_address, p_fragment->GetInputLength() );

	SFragmentEntry				entry( fragment_address, nullptr );
	FragmentVec::iterator		it( std::lower_bound( mFragments.begin(), mFragments.end(), entry ) );

	entry.Fragment = p_fragment;
	mFragments.insert( it, entry );

	// Update the hash table (it stores failed lookups now, so we need to be sure to purge any stale entries in there
	u32 ix {MakeHashIdx( fragment_address )};
	mpCacheHashTable[ix].addr = fragment_address;
	mpCacheHashTable[ix].ptr = reinterpret_cast< u32 >( p_fragment );

	// Process any jumps for this before inserting new ones
	JumpMap::iterator	jump_it( mJumpMap.find( fragment_address ) );
	if( jump_it != mJumpMap.end() )
	{
		const JumpList &		jumps( jump_it->second );
		for( JumpList::const_iterator it = jumps.begin(); it != jumps.end(); ++it )
		{
			//DBGConsole_Msg( 0, "Inserting [R%08x], patching jump at %08x ", address, (*it) );
			PatchJumpLongAndFlush( (*it), p_fragment->GetEntryTarget() );
		}

		// All patched - clear
		mJumpMap.erase( jump_it );
	}

	// Finally register any links that this fragment may have
	const FragmentPatchList &	patch_list( p_fragment->GetPatchList() );
	for( FragmentPatchList::const_iterator it = patch_list.begin(); it != patch_list.end(); ++it )
	{
		u32				target_address( it->Address );
		CJumpLocation	jump( it->Jump );

		CFragment * p_fragment( LookupFragmentQ( target_address ) );
		if( p_fragment != nullptr )
		{
			PatchJumpLongAndFlush( jump, p_fragment->GetEntryTarget() );
		}
		else if( target_address != u32(~0) )
		{
			// Store the address for later processing
			mJumpMap[ target_address ].push_back( jump );
		}
	}

	// Free memoire
	p_fragment->DiscardPatchList();

	// For simulation only
	p_fragment->SetCache( this );
}

//*************************************************************************************
//
//*************************************************************************************
void CFragmentCache::Clear()
{
	// Clear out all the framents
	for(FragmentVec::iterator it = mFragments.begin(); it != mFragments.end(); ++it)
	{
		delete it->Fragment;
	}

	mFragments.erase( mFragments.begin(), mFragments.end() );
	mCachedFragmentAddress = 0;
	mpCachedFragment = nullptr;
	memset( mpCacheHashTable, 0, sizeof(mpCacheHashTable) );
	mJumpMap.clear();

	mCacheCoverage.Reset();

	mpCodeBufferManager->Reset();
}

//*************************************************************************************
//
//*************************************************************************************
bool CFragmentCache::ShouldInvalidateOnWrite( u32 address, u32 length ) const
{
	return mCacheCoverage.IsCovered( address, length );
}

//*************************************************************************************
//
//*************************************************************************************
#define AddressToIndex( addr ) ((addr - BASE_ADDRESS) >> MEM_USAGE_SHIFT)

//*************************************************************************************
//
//*************************************************************************************
void CFragmentCacheCoverage::ExtendCoverage( u32 address, u32 len )
{
	u32 first_entry( AddressToIndex( address ) );
	u32 last_entry( AddressToIndex( address + len - 1 ) );

	// Mark all entries as true
	for( u32 i = first_entry; i <= last_entry && i < NUM_MEM_USAGE_ENTRIES; ++i )
	{
		mCacheCoverage[ i ] = true;
	}
}

//*************************************************************************************
//
//*************************************************************************************
bool CFragmentCacheCoverage::IsCovered( u32 address, u32 len ) const
{
	u32 first_entry( AddressToIndex( address ) );
	u32 last_entry( AddressToIndex( address + len - 1 ) );

	// Mark all entries as true
	for( u32 i = first_entry; i <= last_entry && i < NUM_MEM_USAGE_ENTRIES; ++i )
	{
		if( mCacheCoverage[ i ] )
			return true;
	}

	return false;
}

//*************************************************************************************
//
//*************************************************************************************
void CFragmentCacheCoverage::Reset( )
{
	sceClibMemset( mCacheCoverage, 0, sizeof( mCacheCoverage ) );
}
