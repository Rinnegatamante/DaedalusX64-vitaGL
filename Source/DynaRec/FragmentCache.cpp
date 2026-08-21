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

	mFragments.reserve( 10000 );

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
	if( address == mCachedFragmentAddress )
		return mpCachedFragment;

	mCachedFragmentAddress = address;
	mpCachedFragment = nullptr;

	u32 ix = MakeHashIdx( address );
	for( u32 probe = 0; probe < HASH_TABLE_SIZE; ++probe )
	{
		const FHashT & slot = mpCacheHashTable[ ix ];
		if( slot.ptr == 0 )
			break;

		if( slot.addr == address )
		{
			mpCachedFragment = reinterpret_cast< CFragment * >( slot.ptr );
			break;
		}

		ix = (ix + 1) & (HASH_TABLE_SIZE - 1);
	}

	return mpCachedFragment;
}

//*************************************************************************************
//
//*************************************************************************************
void CFragmentCache::InsertFragment( CFragment * p_fragment )
{
	u32		fragment_address( p_fragment->GetEntryAddress() );

	const std::vector<u32> & code_pages = p_fragment->GetCodePages();
	for( u32 page : code_pages )
	{
		mCacheCoverage.ExtendCoverage( page, 1 );
	}

	mFragments.push_back( SFragmentEntry( fragment_address, p_fragment ) );

	u32 ix = MakeHashIdx( fragment_address );
	for( u32 probe = 0; probe < HASH_TABLE_SIZE; ++probe )
	{
		FHashT & slot = mpCacheHashTable[ ix ];
		if( slot.ptr == 0 || slot.addr == fragment_address )
		{
			slot.addr = fragment_address;
			slot.ptr = reinterpret_cast< u32 >( p_fragment );
			break;
		}

		ix = (ix + 1) & (HASH_TABLE_SIZE - 1);
	}

	// A failed lookup for this exact address may be cached in the one-entry fast path.
	if( mCachedFragmentAddress == fragment_address )
		mpCachedFragment = p_fragment;

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

	mFragments.clear();
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
EFragmentCacheInvalidationResult CFragmentCache::ValidateInvalidation( u32 address, u32 length, u32 * checked_words,
	u32 * changed_address, u32 * expected_opcode, u32 * current_opcode ) const
{
	if( checked_words != nullptr )
		*checked_words = 0;

	if( !mCacheCoverage.IsCovered( address, length ) )
		return FCIR_NO_OVERLAP;

	u32 total_checked = 0;
	bool exact_overlap = false;

	for( const SFragmentEntry & entry : mFragments )
	{
		u32 fragment_checked = 0;
		const EGuestCodeValidation result = entry.Fragment->ValidateGuestCodeRange( address, length, &fragment_checked,
			changed_address, expected_opcode, current_opcode );
		total_checked += fragment_checked;

		switch( result )
		{
		case GCV_NO_OVERLAP:
			break;
		case GCV_UNCHANGED:
			exact_overlap = true;
			break;
		case GCV_CHANGED:
			if( checked_words != nullptr )
				*checked_words = total_checked;
			return FCIR_CHANGED;
		case GCV_UNVERIFIABLE:
			if( checked_words != nullptr )
				*checked_words = total_checked;
			return FCIR_UNVERIFIABLE;
		default:
			return FCIR_UNVERIFIABLE;
		}
	}

	if( checked_words != nullptr )
		*checked_words = total_checked;
	return exact_overlap ? FCIR_UNCHANGED : FCIR_NO_OVERLAP;
}

//*************************************************************************************
//
//*************************************************************************************
u32 CFragmentCacheCoverage::AddressToIndex( u32 address )
{
	return (address & 0x1fffffffu) >> MEM_USAGE_SHIFT;
}

//*************************************************************************************
//
//*************************************************************************************
void CFragmentCacheCoverage::ExtendCoverage( u32 address, u32 len )
{
	if( len == 0 )
		return;

	u32 first_entry( AddressToIndex( address ) );
	u32 last_entry( AddressToIndex( address + len - 1 ) );

	if( first_entry >= NUM_MEM_USAGE_ENTRIES )
		return;

	if( last_entry >= NUM_MEM_USAGE_ENTRIES )
		last_entry = NUM_MEM_USAGE_ENTRIES - 1;

	for( u32 i = first_entry; i <= last_entry; ++i )
	{
		mCacheCoverage[ i ] = true;
	}
}

//*************************************************************************************
//
//*************************************************************************************
bool CFragmentCacheCoverage::IsCovered( u32 address, u32 len ) const
{
	if( len == 0 )
		return false;

	u32 first_entry( AddressToIndex( address ) );
	u32 last_entry( AddressToIndex( address + len - 1 ) );

	if( first_entry >= NUM_MEM_USAGE_ENTRIES )
		return false;

	if( last_entry >= NUM_MEM_USAGE_ENTRIES )
		last_entry = NUM_MEM_USAGE_ENTRIES - 1;

	for( u32 i = first_entry; i <= last_entry; ++i )
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
