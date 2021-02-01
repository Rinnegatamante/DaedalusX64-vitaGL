/*
Copyright (C) 2020 Rinnegatamante

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
#include "ROMFileNetwork.h"
#include "SysVita/UI/Menu.h"

//*****************************************************************************
//
//*****************************************************************************
ROMFileNetwork::ROMFileNetwork( const char * filename )
:	ROMFile( filename )
,	mRomSize( 0 )
{
}

//*****************************************************************************
//
//*****************************************************************************
ROMFileNetwork::~ROMFileNetwork()
{
}

//*****************************************************************************
//
//*****************************************************************************
bool ROMFileNetwork::Open( COutputStream & messages )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( mFH == NULL, "Opening the file twice?" );
	#endif

	//
	//	Determine which byteswapping mode to use
	//
	
	uint32_t *buf = (uint32_t*)rom_mem_buffer;
	u32		header = buf[0];

	if (!SetHeaderMagic( header ))
	{
		return false;
	}

	//
	//	Determine the rom size
	//
	mRomSize = temp_download_size;

	return true;
}

//*****************************************************************************
//
//*****************************************************************************
bool ROMFileNetwork::LoadRawData( u32 bytes_to_read, u8 *p_bytes, COutputStream & messages )
{
	if (p_bytes == NULL)
	{
		return false;
	}
	
	if (p_bytes != rom_mem_buffer) sceClibMemcpy(p_bytes, rom_mem_buffer, bytes_to_read);

	// Apply the bytesswapping before returning the buffer
	CorrectSwap( p_bytes, bytes_to_read );

	return true;
}

//*****************************************************************************
//
//*****************************************************************************
bool ROMFileNetwork::ReadChunk( u32 offset, u8 * p_dst, u32 length )
{
	// Try and read in data - reset to the specified offset
	sceClibMemcpy(p_dst, &rom_mem_buffer[offset], length);

	// Apply the bytesswapping before returning the buffer
	CorrectSwap( p_dst, length );
	return true;
}
