/*
Copyright (C) 2003 Azimer
Copyright (C) 2001,2006-2007 StrmnNrmn

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

//
//	N.B. This source code is derived from Azimer's Audio plugin (v0.55?)
//	and modified by StrmnNrmn to work with Daedalus PSP. Thanks Azimer!
//	Drop me a line if you get chance :)
//

#include "stdafx.h"
#include "AudioHLEProcessor.h"

#include <string.h>

#include "audiohle.h"

#include "Math/MathUtil.h"
#include "Utility/FastMemcpy.h"
#include "Core/RDRam.h"

static inline int32_t vmulf(int16_t x, int16_t y)
{
    return (((int32_t)(x))*((int32_t)(y))+0x4000)>>15;
}

static inline int align(int x, int n) {
	return (((x >> n) + 1) << n);
}

struct ramp_t{
    int64_t value;
    int64_t step;
    int64_t target;
};

static int16_t ramp_step(struct ramp_t* ramp)
{
	bool target_reached;

	ramp->value += ramp->step;

	target_reached = (ramp->step <= 0)
		? (ramp->value <= ramp->target)
		: (ramp->value >= ramp->target);

	if (target_reached)
	{
		ramp->value = ramp->target;
		ramp->step  = 0;
	}

	return (int16_t)(ramp->value >> 16);
}

static void sample_mix(int16_t* dst, int16_t src, int16_t gain)
{
	*dst = clamp_s16(*dst + ((src * gain) >> 15));
}

int32_t rdot(size_t n, const int16_t *x, const int16_t *y)
{
	int32_t accu = 0;

	y += n;

	while (n != 0) {
		accu += *(x++) * *(--y);
		--n;
	}

	return accu;
}

void envmix_mix(size_t n, int16_t** dst, const int16_t* gains, int16_t src)
{
    size_t i;

    for(i = 0; i < n; ++i)
        sample_mix(dst[i], src, gains[i]);
}

inline s32		FixedPointMulFull16( s32 a, s32 b )
{
	return s32( ( (s64)a * (s64)b ) >> 16 );
}

inline s32		FixedPointMul16( s32 a, s32 b )
{
	return s32( ( a * b ) >> 16 );
}

inline s32		FixedPointMul15( s32 a, s32 b )
{
	return s32( ( a * b ) >> 15 );
}

void SPNOOP( AudioHLECommand command ) {}
void UNKNOWN( AudioHLECommand command ) {}

AudioHLEState gAudioHLEState;

void	AudioHLEState::ClearBuffer( u16 addr, u16 count )
{
	// XXXX check endianness
	memset( Buffer+(addr & 0xfffc), 0, (count+3) & 0xfffc );
}

void	AudioHLEState::EnvMixer( u8 flags, u32 address )
{
	s16 *in=(s16 *)(Buffer+InBuffer);
	s16 *out=(s16 *)(Buffer+OutBuffer);
	s16 *aux1=(s16 *)(Buffer+AuxA);
	s16 *aux2=(s16 *)(Buffer+AuxC);
	s16 *aux3=(s16 *)(Buffer+AuxE);
	s16 Wet, Dry;
	
	bool aux = flags & A_AUX;
	bool init = flags & A_INIT;
	
	int32_t n = aux ? 4 : 2;
		
	ramp_t ramps[2];
	int32_t exp_seq[2];
    int32_t exp_rates[2];
	
	s16* buff = (s16*)(rdram+address);
	
	uint32_t ptr = 0;
	
	if (init)
	{
		Wet = EnvWet;
		Dry = EnvDry;
		ramps[0].step = VolRampLeft / 8;
		ramps[1].step = VolRampRight / 8;
		ramps[0].target = VolTrgLeft << 16;
		ramps[1].target = VolTrgRight << 16;
		ramps[0].value = VolLeft  << 16;
		ramps[1].value = VolRight << 16;
		exp_rates[0]    = VolRampLeft;
		exp_rates[1]    = VolRampRight;
		exp_seq[0]      = (VolLeft * VolRampLeft);
		exp_seq[1]      = (VolRight * VolRampRight);
	}
	else
	{
		Wet				= *(s16 *)(buff +  0); // 0-1
		Dry				= *(s16 *)(buff +  2); // 2-3
		ramps[0].target	= *(s32 *)(buff +  4); // 4-5
		ramps[1].target	= *(s32 *)(buff +  6); // 6-7
		exp_rates[0]    = *(s32 *)(buff +  8); /* 8-9 (save_buffer is a 16bit pointer) */
		exp_rates[1]    = *(s32 *)(buff + 10); /* 10-11 */
		exp_seq[0]      = *(s32 *)(buff + 12); /* 12-13 */
		exp_seq[1]      = *(s32 *)(buff + 14); /* 14-15 */
		ramps[0].value 	= *(s32 *)(buff + 16); // 16-17
		ramps[1].value	= *(s32 *)(buff + 18); // 18-19
	}
	
	ramps[0].step = ramps[0].target - ramps[0].value;
	ramps[1].step = ramps[1].target - ramps[1].value;
	
	for (int y = 0; y < Count; y += 16)
	{
		if (ramps[0].step != 0)
		{
			exp_seq[0] = ((int64_t)exp_seq[0]*(int64_t)exp_rates[0]) >> 16;
			ramps[0].step = (exp_seq[0] - ramps[0].value) >> 3;
		}

		if (ramps[1].step != 0)
		{
			exp_seq[1] = ((int64_t)exp_seq[1]*(int64_t)exp_rates[1]) >> 16;
			ramps[1].step = (exp_seq[1] - ramps[1].value) >> 3;
		}
		
		for (int x = 0; x < 8; ++x) {
            s16  gains[4];
            s16* buffers[4];
            s16 l_vol = ramp_step(&ramps[0]);
            s16 r_vol = ramp_step(&ramps[1]);

            buffers[0] = out + (ptr^1);
            buffers[1] = aux1 + (ptr^1);
            buffers[2] = aux2 + (ptr^1);
            buffers[3] = aux3 + (ptr^1);

            gains[0] = clamp_s16((l_vol * Dry + 0x4000) >> 15);
            gains[1] = clamp_s16((r_vol * Dry + 0x4000) >> 15);
            gains[2] = clamp_s16((l_vol * Wet + 0x4000) >> 15);
            gains[3] = clamp_s16((r_vol * Wet + 0x4000) >> 15);

            envmix_mix(n, buffers, gains, in[ptr^1]);
            ++ptr;
        }
	}

	*(s16 *)(buff +  0) = Wet; // 0-1
	*(s16 *)(buff +  2) = Dry; // 2-3
	*(s32 *)(buff +  4) = (int32_t)ramps[0].target; // 4-5
	*(s32 *)(buff +  6) = (int32_t)ramps[1].target; // 6-7
	*(s32 *)(buff +  8) = exp_rates[0];      /* 8-9 (save_buffer is a 16bit pointer) */
    *(s32 *)(buff + 10) = exp_rates[1];      /* 10-11 */
    *(s32 *)(buff + 12) = exp_seq[0];        /* 12-13 */
    *(s32 *)(buff + 14) = exp_seq[1];        /* 14-15 */
	*(s32 *)(buff + 16) = (int32_t)ramps[0].value; // 16-17
	*(s32 *)(buff + 18) = (int32_t)ramps[0].value; // 18-19
}

void	AudioHLEState::EnvMixerGE( u8 flags, u32 address )
{
	s16 *in=(s16 *)(Buffer+InBuffer);
	s16 *out=(s16 *)(Buffer+OutBuffer);
	s16 *aux1=(s16 *)(Buffer+AuxA);
	s16 *aux2=(s16 *)(Buffer+AuxC);
	s16 *aux3=(s16 *)(Buffer+AuxE);
	s16 Wet, Dry;
	
	bool aux = flags & A_AUX;
	bool init = flags & A_INIT;
	
	int32_t n = aux ? 4 : 2;
		
	ramp_t ramps[2];	
	s16* buff = (s16*)(rdram+address);

	if (init)
	{
		Wet = EnvWet;
		Dry = EnvDry;
		ramps[0].step = VolRampLeft / 8;
		ramps[1].step = VolRampRight / 8;
		ramps[0].target = VolTrgLeft << 16;
		ramps[1].target = VolTrgRight << 16;
		ramps[0].value = VolLeft  << 16;
		ramps[1].value = VolRight << 16;
	}
	else
	{
		Wet				= *(s16 *)(buff +  0); // 0-1
		Dry				= *(s16 *)(buff +  2); // 2-3
		ramps[0].target	= *(s32 *)(buff +  4); // 4-5
		ramps[1].target	= *(s32 *)(buff +  6); // 6-7
		ramps[0].step	= *(s32 *)(buff +  8); // 8-9 (MixerWorkArea is a 16bit pointer)
		ramps[1].step	= *(s32 *)(buff + 10); // 10-11
		ramps[0].value 	= *(s32 *)(buff + 16); // 16-17
		ramps[1].value	= *(s32 *)(buff + 18); // 18-19
	}
	
	Count >>= 1;
	
	for (u32 k = 0; k < Count; k++)
	{
		int16_t  gains[4];
        int16_t* buffers[4];
        int16_t l_vol = ramp_step(&ramps[0]);
        int16_t r_vol = ramp_step(&ramps[1]);
		
		buffers[0] = out + (k^1);
        buffers[1] = aux1 + (k^1);
        buffers[2] = aux2 + (k^1);
        buffers[3] = aux3 + (k^1);
		
		gains[0] = clamp_s16((l_vol * Dry + 0x4000) >> 15);
        gains[1] = clamp_s16((r_vol * Dry + 0x4000) >> 15);
        gains[2] = clamp_s16((l_vol * Wet + 0x4000) >> 15);
        gains[3] = clamp_s16((r_vol * Wet + 0x4000) >> 15);
		
		envmix_mix(n, buffers, gains, in[k^1]);
	}

	*(s16 *)(buff +  0) = Wet; // 0-1
	*(s16 *)(buff +  2) = Dry; // 2-3
	*(s32 *)(buff +  4) = (int32_t)ramps[0].target; // 4-5
	*(s32 *)(buff +  6) = (int32_t)ramps[1].target; // 6-7
	*(s32 *)(buff +  8) = (int32_t)ramps[0].step;   // 8-9 (MixerWorkArea is a 16bit pointer)
	*(s32 *)(buff + 10) = (int32_t)ramps[1].step;   // 10-11
	*(s32 *)(buff + 16) = (int32_t)ramps[0].value; // 16-17
	*(s32 *)(buff + 18) = (int32_t)ramps[0].value; // 18-19
}

#if 1 //1->fast, 0->original Azimer //Corn calc two sample (s16) at once so we get to save a u32
void	AudioHLEState::Resample( u8 flags, u32 pitch, u32 address )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( (flags & 0x2) == 0, "Resample: unhandled flags %02x", flags );		// Was breakpoint - StrmnNrmn
	#endif
	pitch *= 2;

	s16 *	in ( (s16 *)(Buffer) );
	u32 *	out( (u32 *)(Buffer) );	//Save some bandwith and fuse two sample in one write
	u32		srcPtr((InBuffer / 2) - 1);
	u32		dstPtr(OutBuffer / 4);
	u32		tmp;

	u32 accumulator;
	if (flags & 0x1)
	{
		in[srcPtr^1] = 0;
		accumulator = 0;
	}
	else
	{
		in[(srcPtr)^1] = ((u16 *)rdram)[((address >> 1))^1];
		accumulator = *(u16 *)(rdram + address + 10);
	}

	for(u32 i = (((Count + 0xF) & 0xFFF0) >> 2); i != 0 ; i-- )
	{
		tmp =  (in[srcPtr^1] + FixedPointMul16( in[(srcPtr+1)^1] - in[srcPtr^1], accumulator )) << 16;
		accumulator += pitch;
		srcPtr += accumulator >> 16;
		accumulator &= 0xFFFF;

		tmp |= (in[srcPtr^1] + FixedPointMul16( in[(srcPtr+1)^1] - in[srcPtr^1], accumulator )) & 0xFFFF;
		accumulator += pitch;
		srcPtr += accumulator >> 16;
		accumulator &= 0xFFFF;

		out[dstPtr++] = tmp;
	}

	((u16 *)rdram)[((address >> 1))^1] = in[srcPtr^1];
	*(u16 *)(rdram + address + 10) = (u16)accumulator;
}

#else

void	AudioHLEState::Resample( u8 flags, u32 pitch, u32 address )
{
	bool	init( (flags & 0x1) != 0 );
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( (flags & 0x2) == 0, "Resample: unhandled flags %02x", flags );		// Was breakpoint - StrmnNrmn
	#endif
	pitch *= 2;

	s16 *	buffer( (s16 *)(Buffer) );
	u32		srcPtr(InBuffer/2);
	u32		dstPtr(OutBuffer/2);
	srcPtr -= 4;

	u32 accumulator;
	if (init)
	{
		for (u32 x=0; x < 4; x++)
		{
			buffer[(srcPtr+x)^1] = 0;
		}
		accumulator = 0;
	}
	else
	{
		for (u32 x=0; x < 4; x++)
		{
			buffer[(srcPtr+x)^1] = ((u16 *)rdram)[((address/2)+x)^1];
		}
		accumulator = *(u16 *)(rdram+address+10);
	}


	u32		loops( ((Count+0xf) & 0xFFF0)/2 );
	for(u32 i = 0; i < loops ; ++i )
	{
		u32			location( (accumulator >> 0xa) << 0x3 );
		const s16 *	lut( (s16 *)(((u8 *)ResampleLUT) + location) );

		s32 accum;

		accum  = FixedPointMul15( buffer[(srcPtr+0)^1], lut[0] );
		accum += FixedPointMul15( buffer[(srcPtr+1)^1], lut[1] );
		accum += FixedPointMul15( buffer[(srcPtr+2)^1], lut[2] );
		accum += FixedPointMul15( buffer[(srcPtr+3)^1], lut[3] );

		buffer[dstPtr^1] = Saturate<s16>(accum);
		dstPtr++;
		accumulator += pitch;
		srcPtr += (accumulator>>16);
		accumulator&=0xffff;
	}

	for (u32 x=0; x < 4; x++)
	{
		((u16 *)rdram)[((address/2)+x)^1] = buffer[(srcPtr+x)^1];
	}
	*(u16 *)(rdram+address+10) = (u16)accumulator;
}
#endif

inline void AudioHLEState::ExtractSamplesScale( s32 * output, u32 inPtr, s32 vscale ) const
{
	u8 icode;

	// loop of 8, for 8 coded nibbles from 4 bytes which yields 8 s16 pcm values
	icode = Buffer[(InBuffer+inPtr++)^3];
	*output++ = FixedPointMul16( (s16)((icode&0xf0)<< 8), vscale );
	*output++ = FixedPointMul16( (s16)((icode&0x0f)<<12), vscale );
	icode = Buffer[(InBuffer+inPtr++)^3];
	*output++ = FixedPointMul16( (s16)((icode&0xf0)<< 8), vscale );
	*output++ = FixedPointMul16( (s16)((icode&0x0f)<<12), vscale );
	icode = Buffer[(InBuffer+inPtr++)^3];
	*output++ = FixedPointMul16( (s16)((icode&0xf0)<< 8), vscale );
	*output++ = FixedPointMul16( (s16)((icode&0x0f)<<12), vscale );
	icode = Buffer[(InBuffer+inPtr++)^3];
	*output++ = FixedPointMul16( (s16)((icode&0xf0)<< 8), vscale );
	*output++ = FixedPointMul16( (s16)((icode&0x0f)<<12), vscale );
}

inline void AudioHLEState::ExtractSamples( s32 * output, u32 inPtr ) const
{
	u8 icode;

	// loop of 8, for 8 coded nibbles from 4 bytes which yields 8 s16 pcm values
	icode = Buffer[(InBuffer+inPtr++)^3];
	*output++ = (s16)((icode&0xf0)<< 8);
	*output++ = (s16)((icode&0x0f)<<12);
	icode = Buffer[(InBuffer+inPtr++)^3];
	*output++ = (s16)((icode&0xf0)<< 8);
	*output++ = (s16)((icode&0x0f)<<12);
	icode = Buffer[(InBuffer+inPtr++)^3];
	*output++ = (s16)((icode&0xf0)<< 8);
	*output++ = (s16)((icode&0x0f)<<12);
	icode = Buffer[(InBuffer+inPtr++)^3];
	*output++ = (s16)((icode&0xf0)<< 8);
	*output++ = (s16)((icode&0x0f)<<12);
}

//
//	l1/l2 are IN/OUT
//
#if 1 //1->fast, 0->original Azimer //Corn
inline void DecodeSamples( s16 * out, s32 & l1, s32 & l2, const s32 * input, const s16 * book1, const s16 * book2 )
{
	s32 a[8];

	a[0]= (s32)book1[0]*l1;
	a[0]+=(s32)book2[0]*l2;
	a[0]+=input[0]*2048;

	a[1] =(s32)book1[1]*l1;
	a[1]+=(s32)book2[1]*l2;
	a[1]+=(s32)book2[0]*input[0];
	a[1]+=input[1]*2048;

	a[2] =(s32)book1[2]*l1;
	a[2]+=(s32)book2[2]*l2;
	a[2]+=(s32)book2[1]*input[0];
	a[2]+=(s32)book2[0]*input[1];
	a[2]+=input[2]*2048;

	a[3] =(s32)book1[3]*l1;
	a[3]+=(s32)book2[3]*l2;
	a[3]+=(s32)book2[2]*input[0];
	a[3]+=(s32)book2[1]*input[1];
	a[3]+=(s32)book2[0]*input[2];
	a[3]+=input[3]*2048;

	a[4] =(s32)book1[4]*l1;
	a[4]+=(s32)book2[4]*l2;
	a[4]+=(s32)book2[3]*input[0];
	a[4]+=(s32)book2[2]*input[1];
	a[4]+=(s32)book2[1]*input[2];
	a[4]+=(s32)book2[0]*input[3];
	a[4]+=input[4]*2048;

	a[5] =(s32)book1[5]*l1;
	a[5]+=(s32)book2[5]*l2;
	a[5]+=(s32)book2[4]*input[0];
	a[5]+=(s32)book2[3]*input[1];
	a[5]+=(s32)book2[2]*input[2];
	a[5]+=(s32)book2[1]*input[3];
	a[5]+=(s32)book2[0]*input[4];
	a[5]+=input[5]*2048;

	a[6] =(s32)book1[6]*l1;
	a[6]+=(s32)book2[6]*l2;
	a[6]+=(s32)book2[5]*input[0];
	a[6]+=(s32)book2[4]*input[1];
	a[6]+=(s32)book2[3]*input[2];
	a[6]+=(s32)book2[2]*input[3];
	a[6]+=(s32)book2[1]*input[4];
	a[6]+=(s32)book2[0]*input[5];
	a[6]+=input[6]*2048;

	a[7] =(s32)book1[7]*l1;
	a[7]+=(s32)book2[7]*l2;
	a[7]+=(s32)book2[6]*input[0];
	a[7]+=(s32)book2[5]*input[1];
	a[7]+=(s32)book2[4]*input[2];
	a[7]+=(s32)book2[3]*input[3];
	a[7]+=(s32)book2[2]*input[4];
	a[7]+=(s32)book2[1]*input[5];
	a[7]+=(s32)book2[0]*input[6];
	a[7]+=input[7]*2048;

	*out++ =      Saturate<s16>( a[1] >> 11 );
	*out++ =      Saturate<s16>( a[0] >> 11 );
	*out++ =      Saturate<s16>( a[3] >> 11 );
	*out++ =      Saturate<s16>( a[2] >> 11 );
	*out++ =      Saturate<s16>( a[5] >> 11 );
	*out++ =      Saturate<s16>( a[4] >> 11 );
	*out++ = l2 = Saturate<s16>( a[7] >> 11 );
	*out++ = l1 = Saturate<s16>( a[6] >> 11 );
}

#else
inline void DecodeSamples( s16 * out, s32 & l1, s32 & l2, const s32 * input, const s16 * book1, const s16 * book2 )
{
	s32 a[8];

	a[0]= (s32)book1[0]*l1;
	a[0]+=(s32)book2[0]*l2;
	a[0]+=input[0]*2048;

	a[1] =(s32)book1[1]*l1;
	a[1]+=(s32)book2[1]*l2;
	a[1]+=(s32)book2[0]*input[0];
	a[1]+=input[1]*2048;

	a[2] =(s32)book1[2]*l1;
	a[2]+=(s32)book2[2]*l2;
	a[2]+=(s32)book2[1]*input[0];
	a[2]+=(s32)book2[0]*input[1];
	a[2]+=input[2]*2048;

	a[3] =(s32)book1[3]*l1;
	a[3]+=(s32)book2[3]*l2;
	a[3]+=(s32)book2[2]*input[0];
	a[3]+=(s32)book2[1]*input[1];
	a[3]+=(s32)book2[0]*input[2];
	a[3]+=input[3]*2048;

	a[4] =(s32)book1[4]*l1;
	a[4]+=(s32)book2[4]*l2;
	a[4]+=(s32)book2[3]*input[0];
	a[4]+=(s32)book2[2]*input[1];
	a[4]+=(s32)book2[1]*input[2];
	a[4]+=(s32)book2[0]*input[3];
	a[4]+=input[4]*2048;

	a[5] =(s32)book1[5]*l1;
	a[5]+=(s32)book2[5]*l2;
	a[5]+=(s32)book2[4]*input[0];
	a[5]+=(s32)book2[3]*input[1];
	a[5]+=(s32)book2[2]*input[2];
	a[5]+=(s32)book2[1]*input[3];
	a[5]+=(s32)book2[0]*input[4];
	a[5]+=input[5]*2048;

	a[6] =(s32)book1[6]*l1;
	a[6]+=(s32)book2[6]*l2;
	a[6]+=(s32)book2[5]*input[0];
	a[6]+=(s32)book2[4]*input[1];
	a[6]+=(s32)book2[3]*input[2];
	a[6]+=(s32)book2[2]*input[3];
	a[6]+=(s32)book2[1]*input[4];
	a[6]+=(s32)book2[0]*input[5];
	a[6]+=input[6]*2048;

	a[7] =(s32)book1[7]*l1;
	a[7]+=(s32)book2[7]*l2;
	a[7]+=(s32)book2[6]*input[0];
	a[7]+=(s32)book2[5]*input[1];
	a[7]+=(s32)book2[4]*input[2];
	a[7]+=(s32)book2[3]*input[3];
	a[7]+=(s32)book2[2]*input[4];
	a[7]+=(s32)book2[1]*input[5];
	a[7]+=(s32)book2[0]*input[6];
	a[7]+=input[7]*2048;

	s16 r[8];
	for(u32 j=0;j<8;j++)
	{
		u32 idx( j^1 );
		r[idx] = Saturate<s16>( a[idx] >> 11 );
		*(out++) = r[idx];
	}

	l1=r[6];
	l2=r[7];
}
#endif

void AudioHLEState::ADPCMDecode( u8 flags, u32 address )
{
	bool	init( (flags&0x1) != 0 );
	bool	loop( (flags&0x2) != 0 );

	u16 inPtr=0;
	s16 *out=(s16 *)(Buffer+OutBuffer);

	if(init)
	{
		memset( out, 0, 32 );
	}
	else
	{
		u32 addr( loop ? LoopVal : address );
		memmove( out, &rdram[addr], 32 );
	}

	s32 l1=out[15];
	s32 l2=out[14];
	out+=16;

	s32 inp1[8];
	s32 inp2[8];

	s32 count = (s16)Count;		// XXXX why convert this to signed?
	while(count>0)
	{
													// the first iteration through, these values are
													// either 0 in the case of A_INIT, from a special
													// area of memory in the case of A_LOOP or just
													// the values we calculated the last time

		u8 code=Buffer[(InBuffer+inPtr)^3];
		u32 index=code&0xf;							// index into the adpcm code table
		s16 * book1=(s16 *)&ADPCMTable[index<<4];
		s16 * book2=book1+8;
		code>>=4;									// upper nibble is scale

		inPtr++;									// coded adpcm data lies next

		if( code < 12 )
		{
			s32 vscale=(0x8000>>((12-code)-1));			// very strange. 0x8000 would be .5 in 16:16 format
														// so this appears to be a fractional scale based
														// on the 12 based inverse of the scale value.  note
														// that this could be negative, in which case we do
														// not use the calculated vscale value... see the
														// if(code>12) check below
			ExtractSamplesScale( inp1, inPtr + 0, vscale );
			ExtractSamplesScale( inp2, inPtr + 4, vscale );
		}
		else
		{
			ExtractSamples( inp1, inPtr + 0 );
			ExtractSamples( inp2, inPtr + 4 );
		}

		DecodeSamples( out + 0, l1, l2, inp1, book1, book2 );
		DecodeSamples( out + 8, l1, l2, inp2, book1, book2 );

		inPtr += 8;
		out += 16;
		count-=32;
	}
	out-=16;
	memmove(&rdram[address],out,32);
}

void	AudioHLEState::LoadBuffer( u32 address )
{
	LoadBuffer( InBuffer, address, Count );
}

void	AudioHLEState::SaveBuffer( u32 address )
{
	SaveBuffer( address, OutBuffer, Count );
}

void	AudioHLEState::LoadBuffer( u16 dram_dst, u32 ram_src, u16 count )
{
	if( count > 0 )
	{
		// XXXX Masks look suspicious - trying to get around endian issues?
		memmove( Buffer+(dram_dst & 0xFFFC), rdram+(ram_src&0xfffffc), (count+3) & 0xFFFC );
	}
}


void	AudioHLEState::SaveBuffer( u32 ram_dst, u16 dmem_src, u16 count )
{
	if( count > 0 )
	{
		// XXXX Masks look suspicious - trying to get around endian issues?
		memmove( rdram+(ram_dst & 0xfffffc), Buffer+(dmem_src & 0xFFFC), (count+3) & 0xFFFC);
	}
}
/*
void	AudioHLEState::SetSegment( u8 segment, u32 address )
{
	DAEDALUS_ASSERT( segment < 16, "Invalid segment" );

	Segments[segment&0xf] = address;
}
*/
void	AudioHLEState::SetLoop( u32 loopval )
{
	LoopVal = loopval;
	//VolTrgLeft  = (s16)(LoopVal>>16);		// m_LeftVol
	//VolRampLeft = (s16)(LoopVal);	// m_LeftVolTarget
}


void	AudioHLEState::SetBuffer( u8 flags, u16 in, u16 out, u16 count )
{
	if (flags & 0x8)
	{
		// A_AUX - Auxillary Sound Buffer Settings
		AuxA = in;
		AuxC = out;
		AuxE = count;
	}
	else
	{
		// A_MAIN - Main Sound Buffer Settings
		InBuffer  = in;
		OutBuffer = out;
		Count	  = count;
	}
}

void	AudioHLEState::DmemMove( u32 dst, u32 src, u16 count )
{
	count = (count + 3) & 0xfffc;

#if 1	//1->fast, 0->slow

	//Can't use fast_memcpy_swizzle, since this code can run on the ME, and VFPU is not accessible
	memcpy_swizzle(Buffer + dst, Buffer + src, count);
#else
	for (u32 i = 0; i < count; i++)
	{
		*(u8 *)(Buffer+((i+dst)^3)) = *(u8 *)(Buffer+((i+src)^3));
	}
#endif
}

void	AudioHLEState::LoadADPCM( u32 address, u16 count )
{
	u32	loops( count / 16 );

	const u16 *table( (const u16 *)(rdram + address) );
	for (u32 x = 0; x < loops; x++)
	{
		ADPCMTable[0x1+(x<<3)] = table[0];
		ADPCMTable[0x0+(x<<3)] = table[1];

		ADPCMTable[0x3+(x<<3)] = table[2];
		ADPCMTable[0x2+(x<<3)] = table[3];

		ADPCMTable[0x5+(x<<3)] = table[4];
		ADPCMTable[0x4+(x<<3)] = table[5];

		ADPCMTable[0x7+(x<<3)] = table[6];
		ADPCMTable[0x6+(x<<3)] = table[7];
		table += 8;
	}
}

void	AudioHLEState::Interleave( u16 outaddr, u16 laddr, u16 raddr, u16 count )
{
	u32 *		out = (u32 *)(Buffer + outaddr);	//Save some bandwith also corrected left and right//Corn
	const u16 *	inr = (const u16 *)(Buffer + raddr);
	const u16 *	inl = (const u16 *)(Buffer + laddr);

	for( u32 x = (count >> 2); x != 0; x-- )
	{
		const u16 right = *inr++;
		const u16 left  = *inl++;

		*out++ = (*inr++ << 16) | *inl++;
		*out++ = (right  << 16) | left;
	}
}

void	AudioHLEState::Interleave( u16 laddr, u16 raddr )
{
	Interleave( OutBuffer, laddr, raddr, Count );
}

void	AudioHLEState::Mixer( u16 dmemout, u16 dmemin, s32 gain, u16 count )
{
#if 1	//1->fast, 0->safe/slow //Corn

	// Make sure we are on even address (YOSHI)
	s16*  in( (s16 *)(Buffer + dmemin) );
	s16* out( (s16 *)(Buffer + dmemout) );

	for( u32 x = count >> 1; x != 0; x-- )
	{
		*out = Saturate<s16>( FixedPointMul15( *in++, gain ) + s32( *out ) );
		out++;
	}

#else
	for( u32 x=0; x < count; x+=2 )
	{
		s16 in( *(s16 *)(Buffer+(dmemin+x)) );
		s16 out( *(s16 *)(Buffer+(dmemout+x)) );

		*(s16 *)(Buffer+((dmemout+x) & (N64_AUDIO_BUFF - 2)) ) = Saturate<s16>( FixedPointMul15( in, gain ) + s32( out ) );
	}
#endif
}

void	AudioHLEState::Deinterleave( u16 outaddr, u16 inaddr, u16 count )
{
	while( count-- )
	{
		*(s16 *)(Buffer+(outaddr^2)) = *(s16 *)(Buffer+(inaddr^2));
		outaddr += 2;
		inaddr  += 4;
	}
}

void	AudioHLEState::Mixer( u16 dmemout, u16 dmemin, s32 gain )
{
	Mixer( dmemout, dmemin, gain, Count );
}

void AudioHLEState::Polef( u8 flags, u32 address, u16 dmemo, u16 dmemi, u16 gain, u16 count )
{
	s16 *dst = (s16*)(gAudioHLEState.Buffer + dmemo);
	const int16_t* const h1 = ADPCMTable;
    int16_t* const h2 = ADPCMTable + 8;
	
	unsigned i;
    int16_t l1, l2;
    int16_t h2_before[8];
	
	count = align(count, 16);
	
	if (flags & A_INIT) {
		l1 = 0; l2 = 0;
	} else {
		l1 = *(u16*)(g_pu8RamBase + address + 4);
		l2 = *(u16*)(g_pu8RamBase + address + 6);
	}
	
	for(i = 0; i < 8; ++i) {
		h2_before[i] = h2[i];
		h2[i] = (((int32_t)h2[i] * gain) >> 14);
	}

	do
	{
		int16_t frame[8];

		for(i = 0; i < 8; ++i, dmemi += 2)
			frame[i] = *(s16*)(gAudioHLEState.Buffer + ((dmemi ^ 2) & 0xFFF));

		for(i = 0; i < 8; ++i) {
			int32_t accu = frame[i] * gain;
			accu += h1[i]*l1 + h2_before[i]*l2 + rdot(i, h2, frame);
			dst[i^1] = clamp_s16(accu >> 14);
		}

		l1 = dst[6^1];
		l2 = dst[7^1];

		dst += 8;
		count -= 16;
	} while (count != 0);

	rdram_write_many_u32((uint32_t*)(dst - 4), address, 2);
}

void AudioHLEState::Iirf( u8 flags, u32 address, u16 dmemo, u16 dmemi, u16 count )
{
	s16 *dst = (s16*)(gAudioHLEState.Buffer + dmemo);
	int32_t i, prev;
	int16_t frame[8];
	int16_t ibuf[4];
	uint16_t index = 7;
	
	count = align(count, 16);
	
	if (flags & A_INIT) {
		for(i = 0; i < 8; ++i)
			frame[i] = 0;
		ibuf[1] = 0;
		ibuf[2] = 0;
	} else {
		frame[6] = *(u16*)(g_pu8RamBase + address + 4);
		frame[7] = *(u16*)(g_pu8RamBase + address + 6);
		ibuf[1] = (s16)*(u16*)(g_pu8RamBase + address + 8);
		ibuf[2] = (s16)*(u16*)(g_pu8RamBase + address + 10);
	}
	
	prev = vmulf(ADPCMTable[9], frame[6]) * 2;

	do
	{
		for(i = 0; i < 8; ++i)
		{
			int32_t accu;
			ibuf[index&3] = *(s16*)(gAudioHLEState.Buffer + ((dmemi ^ 2) & 0xFFF));

			accu = prev + vmulf(ADPCMTable[0], ibuf[index&3]) + vmulf(ADPCMTable[1], ibuf[(index-1)&3]) + vmulf(ADPCMTable[0], ibuf[(index-2)&3]);
			accu += vmulf(ADPCMTable[8], frame[index]) * 2;
			prev = vmulf(ADPCMTable[9], frame[index]) * 2;
			dst[i^1] = frame[i] = accu;

			index=(index+1)&7;
			dmemi += 2;
        }
        dst += 8;
        count -= 0x10;
	} while (count > 0);
	
	rdram_write_many_u16((uint16_t*)&frame[6], address + 4, 4);
	rdram_write_many_u16((uint16_t*)&ibuf[(index-2)&3], address+8, 2);
	rdram_write_many_u16((uint16_t*)&ibuf[(index-1)&3], address+10, 2);
}
