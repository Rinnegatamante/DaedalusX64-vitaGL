/*
Copyright (C) 2003 Azimer
Copyright (C) 2001,2006 StrmnNrmn

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
#include "AudioOutput.h"

#include <stdio.h>
#include <new>

#include <vitasdk.h>

#include "Config/ConfigOptions.h"
#include "Debug/DBGConsole.h"
#include "HLEAudio/AudioBuffer.h"
#include "Utility/FramerateLimiter.h"
#include "Utility/Thread.h"

extern u32 gSoundSync;

static const u32	DESIRED_OUTPUT_FREQUENCY = 44100;

//static const u32	ADAPTIVE_FREQUENCY_ADJUST = 2000;
// Large BUFFER_SIZE creates huge delay on sound //Corn
static const u32	BUFFER_SIZE  = 1024 * 2;

static const u32	VITA_NUM_SAMPLES = 512;

// Global variables
static SceUID bufferEmpty;
static SceUID playbackSema;

static volatile s32 sound_volume = 32767;
static volatile u32 sound_status;

static bool audio_open = false;

static AudioOutput * ac;

CAudioBuffer *mAudioBuffer;

static int audioOutput(unsigned int args, void *argp)
{
	uint16_t *playbuf = (uint16_t*)malloc(BUFFER_SIZE);
	int pcmflip = 0;
	
	// reserve audio channel
	SceUID sound_channel = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, VITA_NUM_SAMPLES, DESIRED_OUTPUT_FREQUENCY, SCE_AUDIO_OUT_MODE_STEREO);
	sceAudioOutSetConfig(sound_channel, -1, -1, (SceAudioOutMode)-1);
	
	int vol_stereo[] = {32767, 32767};
	sceAudioOutSetVolume(sound_channel, (SceAudioOutChannelFlag)(SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), vol_stereo);

	while(sound_status != 0xDEADBEEF)
	{
		mAudioBuffer->Drain( reinterpret_cast< Sample * >( playbuf ), VITA_NUM_SAMPLES );
		sceAudioOutOutput(sound_channel, playbuf);
	}
	sceAudioOutReleasePort(sound_channel);
	sceKernelExitDeleteThread(0);
	return 0;
}

static void AudioInit()
{
	sound_status = 0; // threads running

	// create audio playback thread to provide timing
	SceUID audioThid = sceKernelCreateThread("audioOutput", &audioOutput, 0x10000100, 0x10000, 0, 0, NULL);
	sceKernelStartThread(audioThid, 0, NULL);

	// Everything OK
	audio_open = true;
}

static void AudioExit()
{
	// Stop stream
	if (audio_open)
	{
		sound_status = 0xDEADBEEF;
		sceKernelDelayThread(100*1000);
	}

	audio_open = false;
}

AudioOutput::AudioOutput()
:	mAudioPlaying( false )
,	mFrequency( 44100 )
{
	// Allocate audio buffer with malloc_64 to avoid cached/uncached aliasing
	void * mem = malloc( sizeof( CAudioBuffer ) );
	mAudioBuffer = new( mem ) CAudioBuffer( BUFFER_SIZE );
}

AudioOutput::~AudioOutput( )
{
	StopAudio();

	mAudioBuffer->~CAudioBuffer();
	free( mAudioBuffer );
}

void AudioOutput::SetFrequency( u32 frequency )
{
	mFrequency = frequency;
}

void AudioOutput::AddBuffer( u8 *start, u32 length )
{
	if (length == 0)
		return;

	if (!mAudioPlaying)
		StartAudio();

	u32 num_samples = length / sizeof( Sample );

	//Adapt Audio to sync% //Corn
	//		output_freq = DESIRED_OUTPUT_FREQUENCY;
			/*
	u32 output_freq {};
	if (gAudioRateMatch)
	{
		if (gSoundSync > 88200)			output_freq = 88200;	//limit upper rate
		else if (gSoundSync < 44100)	output_freq = 44100;	//limit lower rate
		else							output_freq = gSoundSync;
	}
	else
	{

	}
*/
	switch( gAudioPluginEnabled )
	{
	case APM_DISABLED:
		break;

	case APM_ENABLED_ASYNC:
		break;

	case APM_ENABLED_SYNC:
		{
			mAudioBuffer->AddSamples( reinterpret_cast< const Sample * >( start ), num_samples, mFrequency, 44100 );
		}
		break;
	}
}

void AudioOutput::StartAudio()
{
	if (mAudioPlaying)
		return;

	mAudioPlaying = true;

	ac = this;

	AudioInit();
}

void AudioOutput::StopAudio()
{
	if (!mAudioPlaying)
		return;

	mAudioPlaying = false;

	AudioExit();
}
