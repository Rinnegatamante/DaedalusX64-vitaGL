/*
Copyright (C) 2007 StrmnNrmn

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
#include "AudioBuffer.h"

#include "Config/ConfigOptions.h"
#include "Debug/DBGConsole.h"
#include "Utility/Thread.h"

#ifdef DAEDALUS_PSP
#include <pspsdk.h>
#include <pspkernel.h>
#include "SysPSP/Utility/CacheUtil.h"
#endif

#ifdef USE_SPEEXDSP
#include <speex/speex_resampler.h>
SpeexResamplerState *speex_resampler;
#endif

CAudioBuffer::CAudioBuffer( u32 buffer_size )
	:	mBufferBegin( new Sample[ buffer_size ] )
	,	mBufferEnd( mBufferBegin + buffer_size )
	,	mReadPtr( mBufferBegin )
	,	mWritePtr( mBufferBegin )
{
#ifdef USE_SPEEXDSP
	int err;
	speex_resampler = speex_resampler_init(2, DESIRED_OUTPUT_FREQUENCY, DESIRED_OUTPUT_FREQUENCY, 0, &err);
	speex_resampler_skip_zeros(speex_resampler);
#endif
}

CAudioBuffer::~CAudioBuffer()
{
	delete [] mBufferBegin;
}

u32 CAudioBuffer::GetNumBufferedSamples() const
{
	// Safe? What if we read mWrite, and then mRead moves to start of buffer?
	s32 diff = mWritePtr - mReadPtr;

	if( diff < 0 )
	{
		diff += (mBufferEnd - mBufferBegin);	// Add on buffer length
	}

	return diff;
}

void CAudioBuffer::AddSamples( const Sample * samples, u32 num_samples, u32 frequency, u32 output_freq )
{
#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( frequency <= output_freq, "Input frequency is too high" );
#endif

	const Sample *	read_ptr( mReadPtr );		// No need to invalidate, as this is uncached/volatile
	Sample *		write_ptr( mWritePtr );

	//
	//	'r' is the number of input samples we progress through for each output sample.
	//	's' keeps track of how far between the current two input samples we are.
	//	We increment it by 'r' for each output sample we generate.
	//	When it reaches 1.0, we know we've hit the next sample, so we increment in_idx
	//	and reduce s by 1.0 (to keep it in the range 0.0 .. 1.0)
	//	Principle is the same but rewritten to integer mode (faster & less ASM) //Corn

	const s32 r( (frequency << 12)  / output_freq );
	s32		  s( 0 );
	u32		  in_idx( 0 );
	u32		  output_samples( (( num_samples * output_freq ) / frequency) - 1);

#ifdef USE_SPEEXDSP
	speex_resampler_set_rate(speex_resampler, frequency, output_freq);
	
	Sample out_buf[1024 * 1024];
	uint32_t in_processed = num_samples;
	uint32_t out_processed = 1024 * 1024;
	speex_resampler_process_interleaved_int(speex_resampler, (s16*)samples, &in_processed, (s16*)out_buf, &out_processed);
#endif

#ifdef USE_SPEEXDSP
	for (u32 i = 0; i < out_processed; i++)
#else
	u32 i = output_samples;
	while (i != 0)
#endif
	{
#ifndef USE_SPEEXDSP
		// Resample in integer mode (faster & less ASM code) //Corn
		Sample	out;

		out.L = samples[ in_idx ].L + ((( samples[ in_idx + 1 ].L - samples[ in_idx ].L ) * s ) >> 12 );
		out.R = samples[ in_idx ].R + ((( samples[ in_idx + 1 ].R - samples[ in_idx ].R ) * s ) >> 12 );

		s += r;
		in_idx += s >> 12;
		s &= 4095;
		i--;
#endif

		write_ptr++;
		if( write_ptr >= mBufferEnd )
			write_ptr = mBufferBegin;

		while( write_ptr == read_ptr )
		{
			// The buffer is full - spin until the read pointer advances.
			//    Note - spends a lot of time here if program is running
			//    fast. This loop locks the speed to the playback rate
			//    as the program winds up waiting for the buffer to empty.
			// ToDo: Adjust Audio Frequency/ Look at Turok in this regard.
			// We might want to put a Sleep in when executing on the SC?
			read_ptr = mReadPtr;
		}
#ifdef USE_SPEEXDSP
		*write_ptr = out_buf[i];
#else
		*write_ptr = out;
#endif
	}

	//Todo: Check Cache Routines
	// Ensure samples array is written back before mWritePtr
	//dcache_wbinv_range_unaligned( mBufferBegin, mBufferEnd );

	mWritePtr = write_ptr;		// Needs cache wbinv
}

u32	CAudioBuffer::Drain( Sample * samples, u32 num_samples )
{
	const Sample *	read_ptr( mReadPtr );		// No need to invalidate, as this is uncached/volatile
	const Sample *	write_ptr( mWritePtr );		//

	Sample *	out_ptr( samples );
	u32			samples_required( num_samples );

	while( samples_required > 0 )
	{
		// Check if empty
		if( read_ptr == write_ptr )
			break;

		*out_ptr++ = *read_ptr++;

		if( read_ptr >= mBufferEnd )
			read_ptr = mBufferBegin;

		samples_required--;
	}

	mReadPtr = read_ptr;		// No need to invalidate, as this is uncached
	//
	//	If there weren't enough samples, zero out the buffer
	//	FIXME(strmnnrmn): Unnecessary on OSX...
	//
	if( samples_required > 0 )
	{
		//DBGConsole_Msg( 0, "Buffer underflow (%d samples)\n", samples_required );
		//printf( "Buffer underflow (%d samples)\n", samples_required );
		memset( out_ptr, 0, samples_required * sizeof( Sample ) );
	}

	// Return the number of samples written
	return num_samples - samples_required;
}
