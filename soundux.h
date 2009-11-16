/*******************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
 
  (c) Copyright 1996 - 2002 Gary Henderson (gary.henderson@ntlworld.com) and
                            Jerremy Koot (jkoot@snes9x.com)

  (c) Copyright 2001 - 2004 John Weidman (jweidman@slip.net)

  (c) Copyright 2002 - 2004 Brad Jorsch (anomie@users.sourceforge.net),
                            funkyass (funkyass@spam.shaw.ca),
                            Joel Yliluoma (http://iki.fi/bisqwit/)
                            Kris Bleakley (codeviolation@hotmail.com),
                            Matthew Kendora,
                            Nach (n-a-c-h@users.sourceforge.net),
                            Peter Bortas (peter@bortas.org) and
                            zones (kasumitokoduck@yahoo.com)

  C4 x86 assembler and some C emulation code
  (c) Copyright 2000 - 2003 zsKnight (zsknight@zsnes.com),
                            _Demo_ (_demo_@zsnes.com), and Nach

  C4 C++ code
  (c) Copyright 2003 Brad Jorsch

  DSP-1 emulator code
  (c) Copyright 1998 - 2004 Ivar (ivar@snes9x.com), _Demo_, Gary Henderson,
                            John Weidman, neviksti (neviksti@hotmail.com),
                            Kris Bleakley, Andreas Naive

  DSP-2 emulator code
  (c) Copyright 2003 Kris Bleakley, John Weidman, neviksti, Matthew Kendora, and
                     Lord Nightmare (lord_nightmare@users.sourceforge.net

  OBC1 emulator code
  (c) Copyright 2001 - 2004 zsKnight, pagefault (pagefault@zsnes.com) and
                            Kris Bleakley
  Ported from x86 assembler to C by sanmaiwashi

  SPC7110 and RTC C++ emulator code
  (c) Copyright 2002 Matthew Kendora with research by
                     zsKnight, John Weidman, and Dark Force

  S-DD1 C emulator code
  (c) Copyright 2003 Brad Jorsch with research by
                     Andreas Naive and John Weidman
 
  S-RTC C emulator code
  (c) Copyright 2001 John Weidman
  
  ST010 C++ emulator code
  (c) Copyright 2003 Feather, Kris Bleakley, John Weidman and Matthew Kendora

  Super FX x86 assembler emulator code 
  (c) Copyright 1998 - 2003 zsKnight, _Demo_, and pagefault 

  Super FX C emulator code 
  (c) Copyright 1997 - 1999 Ivar, Gary Henderson and John Weidman


  SH assembler code partly based on x86 assembler code
  (c) Copyright 2002 - 2004 Marcus Comstedt (marcus@mc.pp.se) 

 
  Specific ports contains the works of other authors. See headers in
  individual files.
 
  Snes9x homepage: http://www.snes9x.com
 
  Permission to use, copy, modify and distribute Snes9x in both binary and
  source form, for non-commercial purposes, is hereby granted without fee,
  providing that this license information and copyright notice appear with
  all copies and any derived work.
 
  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software.
 
  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes
  charging money for Snes9x or software derived from Snes9x.
 
  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.
 
  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
*******************************************************************************/



#ifndef _SOUND_H_
#define _SOUND_H_

enum
{
	SOUND_SAMPLE = 0,
	SOUND_NOISE,
	SOUND_EXTRA_NOISE,
	SOUND_MUTE
};

enum
{
	SOUND_SILENT,
	SOUND_ATTACK,
	SOUND_DECAY,
	SOUND_SUSTAIN,
	SOUND_RELEASE,
	SOUND_GAIN,
	SOUND_INCREASE_LINEAR,
	SOUND_INCREASE_BENT_LINE,
	SOUND_DECREASE_LINEAR,
	SOUND_DECREASE_EXPONENTIAL
};

enum
{
	MODE_NONE = SOUND_SILENT,
	MODE_ADSR,
	MODE_RELEASE = SOUND_RELEASE,
	MODE_GAIN,
	MODE_INCREASE_LINEAR,
	MODE_INCREASE_BENT_LINE,
	MODE_DECREASE_LINEAR,
	MODE_DECREASE_EXPONENTIAL
};

#define MAX_ENVELOPE_HEIGHT 127
#define ENVELOPE_SHIFT 7
#define MAX_VOLUME 127
#define VOLUME_SHIFT 7
#define VOL_DIV 128

#define NUM_CHANNELS    8
#define SOUND_DECODE_LENGTH 16
#define SOUND_BUFFER_SIZE (1024 * 16)
#define MAX_BUFFER_SIZE SOUND_BUFFER_SIZE
#define SOUND_BUFFER_SIZE_MASK (SOUND_BUFFER_SIZE - 1)

#define SOUND_BUFS      4

#ifdef __sgi
#include <audio.h>
#endif /* __sgi */

typedef struct {
	int				sound_fd;				// ** port specific
	int				sound_switch;			// channel on/off
	int				playback_rate;			// 32000Hz is recommended
	int				buffer_size;			// ** port specific
	int				noise_gen;				// 
	bool8			mute_sound;				// mute
	int				stereo;					// stereo or mono
	bool8			sixteen_bit;			// 16bit or 8bit sample
	bool8			encoded;				// ** port specific
#ifdef __sun
	int				last_eof;				// ** port specific
#endif
#ifdef __sgi
	ALport			al_port;				// ** port specific
#endif /* __sgi */
	int32			samples_mixed_so_far;	// ** port specific
	int32			play_position;			// ** port specific
	uint32			err_counter;			// ** port specific
	uint32			err_rate;				// ** port specific

	double			pitch_mul;				// used with Settings.FixFrequency
} SoundStatus;

EXTERN_C volatile SoundStatus so;

typedef struct {
    int32			state;					// ADSR/GAIN/RELEASE/SILENT
    int32			type;					// sample or noise
    short			volume_left;			// VOL(L)
    short			volume_right;			// VOL(R)
    uint32			hertz;					// ((P(H) << 8) + P(L)) * 8
    uint32			frequency;				// normalized pitch
    int32			count;					// 
    bool8			loop;					// loop flag in BRR header
    int32			envx;					// 
    short			left_vol_level;			// 
    short			right_vol_level;		// 
    short			envx_target;			// 
    uint32			env_error;				// 
    uint32			erate;					// 
    int32 			direction;				// 
    uint32			attack_rate;			// 
    uint32			decay_rate;				// 
    uint32			sustain_rate;			// 
    uint32			release_rate;			// 
    uint32			sustain_level;			// 
    signed short	sample;					// OUTX << 4
    signed short	decoded[16];			// decoded 16 samples
    signed short	previous16[2];
    signed short	*block;
    uint16			sample_number;			// SRCN
    bool8			last_block;				// end flag in BRR header
    bool8			needs_decode;			// true when BRR block will be decoded
    uint32			block_pointer;			// currect block
    uint32			sample_pointer;			// pointer in a block
    int32			*echo_buf_ptr;			// EchoBuffer[] or DummyEchoBuffer[]
    int32			mode;					// ADSR/GAIN/RELEASE/SILENT
    int32			envxx;					// 
    signed short	next_sample;			// 
    int32			interpolate;			// 
    int32			previous[2];			// last two nybbles for BRR decode
    uint32			dummy[8];				// Just incase they are needed in the future,

	int32			nb_index;				// index of cached samples
	int16			nb_sample[4];			// cached samples for interpolation
	int16			raw_sample;				// signed 16 bit sample
} Channel;

typedef struct
{
    short			master_volume_left;		// MVOL(L)
    short			master_volume_right;	// MVOL(R)
    short			echo_volume_left;		// EVOL(L)
    short			echo_volume_right;		// EVOL(R)
    int32			echo_enable;			// EON
    int32			echo_feedback;			// EFB
    int32			echo_ptr;				// index of Echo[]
    int32			echo_buffer_size;		// num of echo samples
    int32			echo_write_enabled;		// ECEN
    int32			echo_channel_enable;	// 
    int32			pitch_mod;				// PMOD
    uint32			dummy[3];				// Just incase they are needed in the future,
											// for snapshot compatibility.
    Channel			channels[NUM_CHANNELS];
    bool8			no_filter;				// true when simple echo
    int32			master_volume[2];
    int32			echo_volume[2];
    int32			noise_hertz;			// ** unused

	int32			noise_count;			// counter for noise frequency
	int32			noise_rate;				// noise frequency from env_counter_table
	uint16			noise_seed;				// seed for noise generation
} SSoundData;

EXTERN_C SSoundData SoundData;

void S9xSetEnvelopeRate (int channel, unsigned long rate, int direction, int target);
void S9xSetSoundVolume (int channel, short volume_left, short volume_right);
void S9xSetMasterVolume (short master_volume_left, short master_volume_right);
void S9xSetEchoVolume (short echo_volume_left, short echo_volume_right);
void S9xSetEchoEnable (uint8 byte);
void S9xSetEchoFeedback (int echo_feedback);
void S9xSetEchoDelay (int byte);
void S9xSetEchoWriteEnable (uint8 byte);
void S9xSetFrequencyModulationEnable (uint8 byte);
void S9xSetSoundKeyOff (int channel);
void S9xPrepareSoundForSnapshotSave (bool8 restore);
void S9xFixSoundAfterSnapshotLoad (int version);
void S9xSetFilterCoefficient (int tap, int value);
void S9xSetSoundADSR (int channel, int attack, int decay, int sustain, int sustain_level, int release);
void S9xSetEnvelopeHeight (int channel, int height);
int S9xGetEnvelopeHeight (int channel);
void S9xSetSoundHertz (int channel, int hertz);
void S9xSetSoundType (int channel, int type_of_sound);
bool8 S9xSetSoundMute (bool8 mute);
void S9xResetSound (bool8 full);
void S9xSetPlaybackRate (uint32 rate);
bool8 S9xSetSoundMode (int channel, int mode);
void S9xSetSoundControl (int sound_switch);
void S9xPlaySample (int channel);

void S9xFixEnvelope (int channel, uint8 gain, uint8 adsr1, uint8 adsr2);
bool8 S9xOpenSoundDevice (int, bool8, int);

EXTERN_C void S9xMixSamples (uint8 *buffer, int sample_count);
EXTERN_C void S9xMixSamplesO (uint8 *buffer, int sample_count, int byte_offset);

void S9xSetSoundSample (int channel, uint16 sample_number);
void S9xSetSoundFrequency (int channel, int hertz);

#endif
