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
#ifdef __DJGPP__
#include <allegro.h>
#undef TRUE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include "snes9x.h"
#include "apu.h"
#include "memmap.h"
#include "cpuexec.h"
#include "soundux.h"

// gaussian table by libopenspc and SNEeSe
static const int32 gauss[512] =
{
	0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
	0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
	0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001,
	0x001, 0x001, 0x001, 0x002, 0x002, 0x002, 0x002, 0x002,
	0x002, 0x002, 0x003, 0x003, 0x003, 0x003, 0x003, 0x004,
	0x004, 0x004, 0x004, 0x004, 0x005, 0x005, 0x005, 0x005,
	0x006, 0x006, 0x006, 0x006, 0x007, 0x007, 0x007, 0x008,
	0x008, 0x008, 0x009, 0x009, 0x009, 0x00A, 0x00A, 0x00A,
	0x00B, 0x00B, 0x00B, 0x00C, 0x00C, 0x00D, 0x00D, 0x00E,
	0x00E, 0x00F, 0x00F, 0x00F, 0x010, 0x010, 0x011, 0x011,
	0x012, 0x013, 0x013, 0x014, 0x014, 0x015, 0x015, 0x016,
	0x017, 0x017, 0x018, 0x018, 0x019, 0x01A, 0x01B, 0x01B,
	0x01C, 0x01D, 0x01D, 0x01E, 0x01F, 0x020, 0x020, 0x021,
	0x022, 0x023, 0x024, 0x024, 0x025, 0x026, 0x027, 0x028,
	0x029, 0x02A, 0x02B, 0x02C, 0x02D, 0x02E, 0x02F, 0x030,
	0x031, 0x032, 0x033, 0x034, 0x035, 0x036, 0x037, 0x038,
	0x03A, 0x03B, 0x03C, 0x03D, 0x03E, 0x040, 0x041, 0x042,
	0x043, 0x045, 0x046, 0x047, 0x049, 0x04A, 0x04C, 0x04D,
	0x04E, 0x050, 0x051, 0x053, 0x054, 0x056, 0x057, 0x059,
	0x05A, 0x05C, 0x05E, 0x05F, 0x061, 0x063, 0x064, 0x066,
	0x068, 0x06A, 0x06B, 0x06D, 0x06F, 0x071, 0x073, 0x075,
	0x076, 0x078, 0x07A, 0x07C, 0x07E, 0x080, 0x082, 0x084,
	0x086, 0x089, 0x08B, 0x08D, 0x08F, 0x091, 0x093, 0x096,
	0x098, 0x09A, 0x09C, 0x09F, 0x0A1, 0x0A3, 0x0A6, 0x0A8,
	0x0AB, 0x0AD, 0x0AF, 0x0B2, 0x0B4, 0x0B7, 0x0BA, 0x0BC,
	0x0BF, 0x0C1, 0x0C4, 0x0C7, 0x0C9, 0x0CC, 0x0CF, 0x0D2,
	0x0D4, 0x0D7, 0x0DA, 0x0DD, 0x0E0, 0x0E3, 0x0E6, 0x0E9,
	0x0EC, 0x0EF, 0x0F2, 0x0F5, 0x0F8, 0x0FB, 0x0FE, 0x101,
	0x104, 0x107, 0x10B, 0x10E, 0x111, 0x114, 0x118, 0x11B,
	0x11E, 0x122, 0x125, 0x129, 0x12C, 0x130, 0x133, 0x137,
	0x13A, 0x13E, 0x141, 0x145, 0x148, 0x14C, 0x150, 0x153,
	0x157, 0x15B, 0x15F, 0x162, 0x166, 0x16A, 0x16E, 0x172,
	0x176, 0x17A, 0x17D, 0x181, 0x185, 0x189, 0x18D, 0x191,
	0x195, 0x19A, 0x19E, 0x1A2, 0x1A6, 0x1AA, 0x1AE, 0x1B2,
	0x1B7, 0x1BB, 0x1BF, 0x1C3, 0x1C8, 0x1CC, 0x1D0, 0x1D5,
	0x1D9, 0x1DD, 0x1E2, 0x1E6, 0x1EB, 0x1EF, 0x1F3, 0x1F8,
	0x1FC, 0x201, 0x205, 0x20A, 0x20F, 0x213, 0x218, 0x21C,
	0x221, 0x226, 0x22A, 0x22F, 0x233, 0x238, 0x23D, 0x241,
	0x246, 0x24B, 0x250, 0x254, 0x259, 0x25E, 0x263, 0x267,
	0x26C, 0x271, 0x276, 0x27B, 0x280, 0x284, 0x289, 0x28E,
	0x293, 0x298, 0x29D, 0x2A2, 0x2A6, 0x2AB, 0x2B0, 0x2B5,
	0x2BA, 0x2BF, 0x2C4, 0x2C9, 0x2CE, 0x2D3, 0x2D8, 0x2DC,
	0x2E1, 0x2E6, 0x2EB, 0x2F0, 0x2F5, 0x2FA, 0x2FF, 0x304,
	0x309, 0x30E, 0x313, 0x318, 0x31D, 0x322, 0x326, 0x32B,
	0x330, 0x335, 0x33A, 0x33F, 0x344, 0x349, 0x34E, 0x353,
	0x357, 0x35C, 0x361, 0x366, 0x36B, 0x370, 0x374, 0x379,
	0x37E, 0x383, 0x388, 0x38C, 0x391, 0x396, 0x39B, 0x39F,
	0x3A4, 0x3A9, 0x3AD, 0x3B2, 0x3B7, 0x3BB, 0x3C0, 0x3C5,
	0x3C9, 0x3CE, 0x3D2, 0x3D7, 0x3DC, 0x3E0, 0x3E5, 0x3E9,
	0x3ED, 0x3F2, 0x3F6, 0x3FB, 0x3FF, 0x403, 0x408, 0x40C,
	0x410, 0x415, 0x419, 0x41D, 0x421, 0x425, 0x42A, 0x42E,
	0x432, 0x436, 0x43A, 0x43E, 0x442, 0x446, 0x44A, 0x44E,
	0x452, 0x455, 0x459, 0x45D, 0x461, 0x465, 0x468, 0x46C,
	0x470, 0x473, 0x477, 0x47A, 0x47E, 0x481, 0x485, 0x488,
	0x48C, 0x48F, 0x492, 0x496, 0x499, 0x49C, 0x49F, 0x4A2,
	0x4A6, 0x4A9, 0x4AC, 0x4AF, 0x4B2, 0x4B5, 0x4B7, 0x4BA,
	0x4BD, 0x4C0, 0x4C3, 0x4C5, 0x4C8, 0x4CB, 0x4CD, 0x4D0,
	0x4D2, 0x4D5, 0x4D7, 0x4D9, 0x4DC, 0x4DE, 0x4E0, 0x4E3,
	0x4E5, 0x4E7, 0x4E9, 0x4EB, 0x4ED, 0x4EF, 0x4F1, 0x4F3,
	0x4F5, 0x4F6, 0x4F8, 0x4FA, 0x4FB, 0x4FD, 0x4FF, 0x500,
	0x502, 0x503, 0x504, 0x506, 0x507, 0x508, 0x50A, 0x50B,
	0x50C, 0x50D, 0x50E, 0x50F, 0x510, 0x511, 0x511, 0x512,
	0x513, 0x514, 0x514, 0x515, 0x516, 0x516, 0x517, 0x517,
	0x517, 0x518, 0x518, 0x518, 0x518, 0x518, 0x519, 0x519
};

//static const int32	*G1 = &gauss[256], *G2 = &gauss[512],
//						*G3 = &gauss[255], *G4 = &gauss[-1];

#define	G1(n)	gauss[256 + (n)]
#define	G2(n)	gauss[512 + (n)]
#define	G3(n)	gauss[255 + (n)]
#define	G4(n)	gauss[ -1 + (n)]

// envelope/noise table by libopenspc and SNEeSe
static int32 env_counter_table[32] =
{
	0x0000, 0x000F, 0x0014, 0x0018, 0x001E, 0x0028, 0x0030, 0x003C,
	0x0050, 0x0060, 0x0078, 0x00A0, 0x00C0, 0x00F0, 0x0140, 0x0180,
	0x01E0, 0x0280, 0x0300, 0x03C0, 0x0500, 0x0600, 0x0780, 0x0A00,
	0x0C00, 0x0F00, 0x1400, 0x1800, 0x1E00, 0x2800, 0x3C00, 0x7800
};

static int32		env_counter_max;
static const int32	env_counter_max_master = 0x7800;

#define FIXED_POINT 0x10000UL
#define FIXED_POINT_REMAINDER 0xffffUL
#define FIXED_POINT_SHIFT 16

#undef ABS
#define ABS(a) ((a) < 0 ? -(a) : (a))

extern int32	Loop[16];
extern int32	Echo[24000];
extern int32	FilterTaps[8];
extern int32	MixBuffer[SOUND_BUFFER_SIZE];
extern int32	EchoBuffer[SOUND_BUFFER_SIZE];
extern int32	DummyEchoBuffer[SOUND_BUFFER_SIZE];
extern uint32	FIRIndex;

extern long FilterValues[4][2];
extern int NoiseFreq [32];

static int32	noise_cache[SOUND_BUFFER_SIZE];
static int32	wave[SOUND_BUFFER_SIZE];

#define VOL_DIV8  0x8000
#define VOL_DIV16 0x0080
#define ENVX_SHIFT 24

void S9xAPUSetEndOfSample (int i, Channel *);
void S9xAPUSetEndX (int);
void S9xSetEnvRate (Channel *, int32, int32);
void MixStereo (int);
void MixMono (int);

static void DecodeBlock (Channel *);
STATIC inline uint8 *S9xGetSampleAddress (int);

int FakeMute = 1; 
int DoFakeMute = 0; 

// F is channel's current frequency and M is the 16-bit modulation waveform
// from the previous channel multiplied by the current envelope volume level.
#define PITCH_MOD(F,M) ((F) * ((((unsigned long) (M)) + 0x800000) >> 16) >> 7)
//#define PITCH_MOD(F,M) ((F) * ((((M) & 0x7fffff) >> 14) + 1) >> 8)

#define LAST_SAMPLE 0xffffff
#define JUST_PLAYED_LAST_SAMPLE(c) ((c)->sample_pointer >= LAST_SAMPLE)

static inline int32 absolute (int32);
static inline int16 sclip15 (int32);
static inline int8 sclamp8 (int32);
static inline int16 sclamp15 (int32);
static inline int16 sclamp16 (int32);

static inline int32 absolute (int32 x)
{
	return ((x < 0) ? -x : x);
}

static inline int16 sclip15 (int32 x)
{
	return (int16)((x & 16384) ? (x | ~16383) : (x & 16383));
}

static inline int8 sclamp8 (int32 x)
{
	return (int8)((x > 127) ? 127 : (x < -128) ? -128 : x);
}

static inline int16 sclamp15 (int32 x)
{
	return (int16)((x > 16383) ? 16383 : (x < -16384) ? -16384 : x);
}

static inline int16 sclamp16 (int32 x)
{
	return (int16)((x > 32767) ? 32767 : (x < -32768) ? -32768 : x);
}

STATIC inline uint8 *S9xGetSampleAddress (int sample_number)
{
    uint32 addr = (((APU.DSP[APU_DIR] << 8) + (sample_number << 2)) & 0xFFFF);
    return (IAPU.RAM + addr);
}

void S9xAPUSetEndOfSample (int i, Channel *ch)
{
    ch->state = SOUND_SILENT;
    ch->mode = MODE_NONE;
  if(!DoFakeMute || !Settings.FakeMuteFix) { 
    APU.DSP [APU_ENDX] |= 1 << i;
    APU.DSP [APU_KON] &= ~(1 << i);
    APU.DSP [APU_KOFF] &= ~(1 << i);
    APU.KeyedChannels &= ~(1 << i);
  }
}
#ifdef __DJGPP
END_OF_FUNCTION (S9xAPUSetEndOfSample)
#endif

void S9xAPUSetEndX (int ch)
{
  if(!DoFakeMute || !Settings.FakeMuteFix) { 
    APU.DSP [APU_ENDX] |= 1 << ch;
  }
}
#ifdef __DJGPP
END_OF_FUNCTION (S9xAPUSetEndX)
#endif

void S9xSetEnvRate (Channel *ch, unsigned long rate, int direction, int target)
{
    ch->envx_target = target;
	
    if (rate == ~0UL)
    {
		ch->direction = 0;
		rate = 0;
    }
    else
		ch->direction = direction;
	
    static int steps [] =
    {
		//	0, 64, 1238, 1238, 256, 1, 64, 109, 64, 1238
		0, 64, 619, 619, 128, 1, 64, 55, 64, 619
    };
	
    if (rate == 0)
		ch->erate = 0;
    else
    {
		ch->erate = (unsigned long)
			(((int64) FIXED_POINT * 1000 * steps [ch->state]) /
			(rate * so.playback_rate));
    }
}

#ifdef __DJGPP
END_OF_FUNCTION(S9xSetEnvRate);
#endif

void S9xSetEnvelopeRate (int channel, unsigned long rate, int direction,
						 int target)
{
    S9xSetEnvRate (&SoundData.channels [channel], rate, direction, target);
}

#ifdef __DJGPP
END_OF_FUNCTION(S9xSetEnvelopeRate);
#endif

void S9xSetSoundVolume (int channel, short volume_left, short volume_right)
{
	Channel *ch = &SoundData.channels[channel];
	if (!so.stereo)
		volume_left = (ABS(volume_right) + ABS(volume_left)) >> 1;

	ch->volume_left = volume_left;
	ch->volume_right = volume_right;
}

void S9xSetMasterVolume (short volume_left, short volume_right)
{
	if (Settings.DisableMasterVolume || SNESGameFixes.EchoOnlyOutput)
	{
		SoundData.master_volume_left = 127;
		SoundData.master_volume_right = 127;
		SoundData.master_volume [0] = SoundData.master_volume [1] = 127;
	}
	else
	{
		if (!so.stereo)
			volume_left = (ABS (volume_right) + ABS (volume_left)) >> 1;

		SoundData.master_volume_left = volume_left;
		SoundData.master_volume_right = volume_right;
		SoundData.master_volume [Settings.ReverseStereo] = volume_left;
		SoundData.master_volume [1 ^ Settings.ReverseStereo] = volume_right;
	}
}

void S9xSetEchoVolume (short volume_left, short volume_right)
{
	if (!so.stereo)
		volume_left = (ABS (volume_right) + ABS (volume_left)) >> 1;

	SoundData.echo_volume_left = volume_left;
	SoundData.echo_volume_right = volume_right;
	SoundData.echo_volume [Settings.ReverseStereo] = volume_left;
	SoundData.echo_volume [1 ^ Settings.ReverseStereo] = volume_right;
}

void S9xSetEchoEnable (uint8 byte)
{
    SoundData.echo_channel_enable = byte;
    if (!SoundData.echo_write_enabled || Settings.DisableSoundEcho)
		byte = 0;
    if (byte && !SoundData.echo_enable)
    {
		memset (Loop, 0, sizeof (Loop));
		memset (Echo, 0, sizeof (Echo));
    }
	
    SoundData.echo_enable = byte;
    for (int i = 0; i < 8; i++)
    {
		if (byte & (1 << i))
			SoundData.channels [i].echo_buf_ptr = EchoBuffer;
		else
			SoundData.channels [i].echo_buf_ptr = DummyEchoBuffer;
    }
}

void S9xSetEchoFeedback (int feedback)
{
    SoundData.echo_feedback = sclamp8(feedback);
}

void S9xSetEchoDelay (int delay)
{
    SoundData.echo_buffer_size = (512 * delay * so.playback_rate) / 32000;
    if (so.stereo)
		SoundData.echo_buffer_size <<= 1;
    if (SoundData.echo_buffer_size)
		SoundData.echo_ptr %= SoundData.echo_buffer_size;
    else
		SoundData.echo_ptr = 0;
    S9xSetEchoEnable (APU.DSP [APU_EON]);
}

void S9xSetEchoWriteEnable (uint8 byte)
{
    SoundData.echo_write_enabled = byte;
    S9xSetEchoDelay (APU.DSP [APU_EDL] & 15);
}

void S9xSetFrequencyModulationEnable (uint8 byte)
{
    SoundData.pitch_mod = byte & ~1;
}

void S9xSetSoundKeyOff (int channel)
{
    Channel *ch = &SoundData.channels[channel];
	
    if (ch->state != SOUND_SILENT)
    {
		ch->state = SOUND_RELEASE;
		ch->mode = MODE_RELEASE;
		S9xSetEnvRate (ch, 8, -1, 0);
    }
}

void S9xPrepareSoundForSnapshotSave (bool8 restore)
{
	if (!restore)
	{
		for (int i = 0; i < NUM_CHANNELS; i++)
		{
			Channel *ch = &SoundData.channels[i];

			assert(ENVX_SHIFT >= 4);
			ch-> left_vol_level = ((ch->envxx >> (ENVX_SHIFT - 4)) * ch->volume_left ) >> 11;
			ch->right_vol_level = ((ch->envxx >> (ENVX_SHIFT - 4)) * ch->volume_right) >> 11;
		}
	}
}

void S9xFixSoundAfterSnapshotLoad (int version)
{
    SoundData.echo_write_enabled = !(APU.DSP [APU_FLG] & 0x20);
    SoundData.echo_channel_enable = APU.DSP [APU_EON];
    S9xSetEchoDelay (APU.DSP [APU_EDL] & 0xf);
    S9xSetEchoFeedback ((signed char) APU.DSP [APU_EFB]);
	
    S9xSetFilterCoefficient (0, (signed char) APU.DSP [APU_C0]);
    S9xSetFilterCoefficient (1, (signed char) APU.DSP [APU_C1]);
    S9xSetFilterCoefficient (2, (signed char) APU.DSP [APU_C2]);
    S9xSetFilterCoefficient (3, (signed char) APU.DSP [APU_C3]);
    S9xSetFilterCoefficient (4, (signed char) APU.DSP [APU_C4]);
    S9xSetFilterCoefficient (5, (signed char) APU.DSP [APU_C5]);
    S9xSetFilterCoefficient (6, (signed char) APU.DSP [APU_C6]);
    S9xSetFilterCoefficient (7, (signed char) APU.DSP [APU_C7]);
    for (int i = 0; i < 8; i++)
    {
		//SoundData.channels[i].needs_decode = TRUE;
		SoundData.channels[i].block = SoundData.channels[i].decoded;

		S9xSetSoundFrequency (i, SoundData.channels[i].hertz);
		SoundData.channels[i].envxx = SoundData.channels[i].envx << ENVX_SHIFT;

		// not used
		SoundData.channels[i].next_sample = 0;
		SoundData.channels[i].interpolate = 0;

		// FIXME: sounds safety, but not true
		SoundData.channels[i].nb_index = 0;
		SoundData.channels[i].nb_sample[0] = 0;
		SoundData.channels[i].nb_sample[1] = 0;
		SoundData.channels[i].nb_sample[2] = 0;
		SoundData.channels[i].nb_sample[3] = 0;
		SoundData.channels[i].count = 0;

		SoundData.channels[i].previous [0] = (int32) SoundData.channels [i].previous16 [0];
		SoundData.channels[i].previous [1] = (int32) SoundData.channels [i].previous16 [1];
    }
	SoundData.noise_count = env_counter_max;
	SoundData.noise_seed = 0x4000;
	SoundData.master_volume [Settings.ReverseStereo] = SoundData.master_volume_left;
	SoundData.master_volume [1 ^ Settings.ReverseStereo] = SoundData.master_volume_right;
	SoundData.echo_volume [Settings.ReverseStereo] = SoundData.echo_volume_left;
	SoundData.echo_volume [1 ^ Settings.ReverseStereo] = SoundData.echo_volume_right;
    //IAPU.Scanline = 0;
}

void S9xSetFilterCoefficient (int tap, int value)
{
    FilterTaps [tap & 7] = value;
    SoundData.no_filter = (FilterTaps [0] == 127 || FilterTaps [0] == 0) && 
		FilterTaps [1] == 0   &&
		FilterTaps [2] == 0   &&
		FilterTaps [3] == 0   &&
		FilterTaps [4] == 0   &&
		FilterTaps [5] == 0   &&
		FilterTaps [6] == 0   &&
		FilterTaps [7] == 0;
}

void S9xSetSoundADSR (int channel, int attack_rate, int decay_rate,
					  int sustain_rate, int sustain_level, int release_rate)
{
    SoundData.channels[channel].attack_rate = attack_rate;
    SoundData.channels[channel].decay_rate = decay_rate;
    SoundData.channels[channel].sustain_rate = sustain_rate;
    SoundData.channels[channel].release_rate = release_rate;
    SoundData.channels[channel].sustain_level = sustain_level + 1;
	
    switch (SoundData.channels[channel].state)
    {
    case SOUND_ATTACK:
		S9xSetEnvelopeRate (channel, attack_rate, 1, 127);
		break;
		
    case SOUND_DECAY:
		S9xSetEnvelopeRate (channel, decay_rate, -1,
			(MAX_ENVELOPE_HEIGHT * (sustain_level + 1)) >> 3);
		break;
    case SOUND_SUSTAIN:
		S9xSetEnvelopeRate (channel, sustain_rate, -1, 0);
		break;
    }
}

void S9xSetEnvelopeHeight (int channel, int level)
{
	Channel *ch = &SoundData.channels[channel];

	ch->envx = level;
	ch->envxx = level << ENVX_SHIFT;

	if (ch->envx == 0 && ch->state != SOUND_SILENT && ch->state != SOUND_GAIN)
	{
		S9xAPUSetEndOfSample (channel, ch);
	}
}

int S9xGetEnvelopeHeight (int channel)
{
    if ((Settings.SoundEnvelopeHeightReading ||
		SNESGameFixes.SoundEnvelopeHeightReading2) &&
        SoundData.channels[channel].state != SOUND_SILENT &&
        SoundData.channels[channel].state != SOUND_GAIN)
    {
        return (SoundData.channels[channel].envx);
    }
	
    //siren fix from XPP
    if (SNESGameFixes.SoundEnvelopeHeightReading2 &&
        SoundData.channels[channel].state != SOUND_SILENT)
    {
        return (SoundData.channels[channel].envx);
    }
	
    return (0);
}

#if 1
void S9xSetSoundSample (int, uint16) 
{
}
#else
void S9xSetSoundSample (int channel, uint16 sample_number)
{
    register Channel *ch = &SoundData.channels[channel];
	
    if (ch->state != SOUND_SILENT && 
		sample_number != ch->sample_number)
    {
		int keep = ch->state;
		ch->state = SOUND_SILENT;
		ch->sample_number = sample_number;
		ch->loop = FALSE;
		ch->needs_decode = TRUE;
		ch->last_block = FALSE;
		ch->previous [0] = ch->previous[1] = 0;
		uint8 *dir = S9xGetSampleAddress (sample_number);
		ch->block_pointer = READ_WORD (dir);
		ch->sample_pointer = 0;
		ch->state = keep;
    }
}
#endif

void S9xSetSoundFrequency (int channel, int hertz)
{
	if (so.playback_rate)
	{
		if (SoundData.channels[channel].type == SOUND_NOISE)
			hertz = NoiseFreq [APU.DSP [APU_FLG] & 0x1f];
		SoundData.channels[channel].frequency = (int)
			(((int64) hertz * FIXED_POINT) / so.playback_rate);
	}
	if (Settings.FixFrequency)
		SoundData.channels[channel].frequency = (uint32)
			(SoundData.channels[channel].frequency * so.pitch_mul);
}

void S9xSetSoundHertz (int channel, int hertz)
{
    SoundData.channels[channel].hertz = hertz;
    S9xSetSoundFrequency (channel, hertz);
}

void S9xSetSoundType (int channel, int type_of_sound)
{
    SoundData.channels[channel].type = type_of_sound;
}

bool8 S9xSetSoundMute (bool8 mute)
{
    bool8 old = so.mute_sound;
    so.mute_sound = mute;
    return (old);
}

static void DecodeBlock (Channel *ch)
{
	uint8	filter;
	int16	*raw = ch->decoded;
	int8	*compressed = (int8 *) &IAPU.RAM[ch->block_pointer];

	ch->block = ch->decoded;

	if (ch->block_pointer > 0x10000 - 9)
	{
		for (int i = 0; i < 16; i++)
			*raw++ = 0;
		ch->previous[0] = 0;
		ch->previous[1] = 0;
		ch->last_block = TRUE;
		ch->loop = FALSE;
		return;
	}

	filter = (uint8) *compressed++;
	ch->last_block = filter & 1;
	ch->loop = (filter & 2) != 0;

	if (ch->state == SOUND_SILENT)
	{
		for (int i = 0; i < 16; i++)
			*raw++ = 0;
		ch->previous[0] = 0;
		ch->previous[1] = 0;
	}
	else
	{
		int32	out, S1, S2;
		uint8	shift;
		int8	sample1, sample2;
		bool	valid_header;

		shift = filter >> 4;
		valid_header = (shift < 0xD);
		filter &= 0x0C;

		S1 = ch->previous[0];
		S2 = ch->previous[1];

		for (int i = 0; i < 8; i++)
		{
			sample1 = *compressed++;
			sample2 = sample1 << 4;
			sample1 >>= 4;
			sample2 >>= 4;

			for (int nybble = 0; nybble < 2; nybble++)
			{
				out = nybble ? (int32) sample2 : (int32) sample1;
				out = valid_header ? ((out << shift) >> 1) : (out & ~0x7FF);

				switch (filter)
				{
					case 0x00: // Direct
						break;

					case 0x04: // 15/16
						out += S1 + ((-S1) >> 4);
						break;

					case 0x08: // 61/32 - 15/16
						out += (S1 << 1) + ((-((S1 << 1) + S1)) >> 5) - S2 + (S2 >> 4);
						break;

					case 0x0C: // 115/64 - 13/16
						out += (S1 << 1) + ((-(S1 + (S1 << 2) + (S1 << 3))) >> 6) - S2 + (((S2 << 1) + S2) >> 4);
						break;
				}

				out = sclip15(sclamp16(out));

				S2 = S1;
				S1 = out;
				*raw++ = (int16) out;
			}
		}

		ch->previous[0] = S1;
		ch->previous[1] = S2;
	}

	ch->block_pointer += 9;
}

void MixStereo (int sample_count)
{
	DoFakeMute=FakeMute;

	int byte_count = so.sixteen_bit ? (sample_count << 1) : sample_count;
	int pitch_mod = SoundData.pitch_mod & ~APU.DSP[APU_NON] & ~1;

	int32 noise_index = 0;
	int32 noise_count = SoundData.noise_count;
	int32 noise_count_next = SoundData.noise_count;

	// noise_rate cannot be changed during the mixing
	SoundData.noise_rate = env_counter_table[APU.DSP[APU_FLG] & 0x1F];

	if (APU.DSP[APU_NON])
	{
		noise_cache[noise_index] = SoundData.noise_seed;
		for (uint32 I = 0; I < (uint32) sample_count; I += 2)
		{
			noise_count -= SoundData.noise_rate;
			while (noise_count <= 0)
			{
				SoundData.noise_seed = (SoundData.noise_seed >> 1) | (((SoundData.noise_seed << 14) ^ (SoundData.noise_seed << 13)) & 0x4000);
				noise_index = (noise_index + 1) % SOUND_BUFFER_SIZE;
				noise_cache[noise_index] = SoundData.noise_seed;
				noise_count += env_counter_max;
			}
		}
		noise_count_next = noise_count;
	}

	for (uint32 J = 0; J < NUM_CHANNELS; J++) 
	{
		Channel *ch = &SoundData.channels[J];
		uint32 freq = (uint32) ((double) ch->frequency * 32000 / 32768);

		bool8 last_block = FALSE;

		if (ch->type == SOUND_NOISE)
		{
			noise_index = 0;
			noise_count = SoundData.noise_count;
		}

		bool8 mod1 = pitch_mod & (1 << J);
		bool8 mod2 = pitch_mod & (1 << (J + 1));
		bool8 sound_switch = so.sound_switch & (1 << J);

		if (ch->state == SOUND_SILENT || last_block) {
			// just in case
			ch->sample = 0;
			if (mod2)
				memset(wave, 0, byte_count * sizeof(int32));
			continue;
		}

		if (ch->needs_decode)
		{
			DecodeBlock(ch);
			ch->needs_decode = FALSE;
			ch->raw_sample = ch->block[0];
			ch->sample_pointer = 0;
		}

		for (uint32 I = 0; I < (uint32) sample_count; I += 2)
		{
			ch->env_error += ch->erate;
			if (ch->env_error >= FIXED_POINT) 
			{
				uint32 step = ch->env_error >> FIXED_POINT_SHIFT;

				switch (ch->state)
				{
				case SOUND_ATTACK:
					ch->env_error &= FIXED_POINT_REMAINDER;
					ch->envx += step << 1;
					ch->envxx = ch->envx << ENVX_SHIFT;

					if (ch->envx >= 126)
					{
						ch->envx = 127;
						ch->envxx = 127 << ENVX_SHIFT;
						ch->state = SOUND_DECAY;
						if (ch->sustain_level != 8) 
						{
							S9xSetEnvRate (ch, ch->decay_rate, -1,
								(MAX_ENVELOPE_HEIGHT * ch->sustain_level)
								>> 3);
							break;
						}
						ch->state = SOUND_SUSTAIN;
						S9xSetEnvRate (ch, ch->sustain_rate, -1, 0);
					}
					break;

				case SOUND_DECAY:
					while (ch->env_error >= FIXED_POINT)
					{
						ch->envxx = (ch->envxx >> 8) * 255;
						ch->env_error -= FIXED_POINT;
					}
					ch->envx = ch->envxx >> ENVX_SHIFT;
					if (ch->envx <= ch->envx_target)
					{
						if (ch->envx <= 0)
						{
							S9xAPUSetEndOfSample (J, ch);
							goto stereo_exit;
						}
						ch->state = SOUND_SUSTAIN;
						S9xSetEnvRate (ch, ch->sustain_rate, -1, 0);
					}
					break;
					
				case SOUND_SUSTAIN:
					while (ch->env_error >= FIXED_POINT)
					{
						ch->envxx = (ch->envxx >> 8) * 255;
						ch->env_error -= FIXED_POINT;
					}
					ch->envx = ch->envxx >> ENVX_SHIFT;
					if (ch->envx <= 0)
					{
						S9xAPUSetEndOfSample (J, ch);
						goto stereo_exit;
					}
					break;

				case SOUND_RELEASE:
					while (ch->env_error >= FIXED_POINT)
					{
						ch->envxx -= (MAX_ENVELOPE_HEIGHT << ENVX_SHIFT) / 256;
						ch->env_error -= FIXED_POINT;
					}
					ch->envx = ch->envxx >> ENVX_SHIFT;
					if (ch->envx <= 0)
					{
						S9xAPUSetEndOfSample (J, ch);
						goto stereo_exit;
					}
					break;

				case SOUND_INCREASE_LINEAR:
					ch->env_error &= FIXED_POINT_REMAINDER;
					ch->envx += step << 1;
					ch->envxx = ch->envx << ENVX_SHIFT;

					if (ch->envx >= 126)
					{
						ch->envx = 127;
						ch->envxx = 127 << ENVX_SHIFT;
						ch->state = SOUND_GAIN;
						ch->mode = MODE_GAIN;
						S9xSetEnvRate (ch, 0, -1, 0);
					}
					break;

				case SOUND_INCREASE_BENT_LINE:
					if (ch->envx >= (MAX_ENVELOPE_HEIGHT * 3) / 4)
					{
						while (ch->env_error >= FIXED_POINT)
						{
							ch->envxx += (MAX_ENVELOPE_HEIGHT << ENVX_SHIFT) / 256;
							ch->env_error -= FIXED_POINT;
						}
						ch->envx = ch->envxx >> ENVX_SHIFT;
					}
					else
					{
						ch->env_error &= FIXED_POINT_REMAINDER;
						ch->envx += step << 1;
						ch->envxx = ch->envx << ENVX_SHIFT;
					}
					
					if (ch->envx >= 126)
					{
						ch->envx = 127;
						ch->envxx = 127 << ENVX_SHIFT;
						ch->state = SOUND_GAIN;
						ch->mode = MODE_GAIN;
						S9xSetEnvRate (ch, 0, -1, 0);
					}
					break;

				case SOUND_DECREASE_LINEAR:
					ch->env_error &= FIXED_POINT_REMAINDER;
					ch->envx -= step << 1;
					ch->envxx = ch->envx << ENVX_SHIFT;
					if (ch->envx <= 0)
					{
						S9xAPUSetEndOfSample (J, ch);
						goto stereo_exit;
					}
					break;

				case SOUND_DECREASE_EXPONENTIAL:
					while (ch->env_error >= FIXED_POINT)
					{
						ch->envxx = (ch->envxx >> 8) * 255;
						ch->env_error -= FIXED_POINT;
					}
					ch->envx = ch->envxx >> ENVX_SHIFT;
					if (ch->envx <= 0)
					{
						S9xAPUSetEndOfSample (J, ch);
						goto stereo_exit;
					}
					break;

				case SOUND_GAIN:
					S9xSetEnvRate (ch, 0, -1, 0);
					break;
				}
			}

			ch->count += mod1 ? (((int64) freq * (32768 + wave[I >> 1])) >> 15) : freq;
//			ch->count += mod1 ? PITCH_MOD(freq, wave [I >> 1]) : freq;

			int32 count = ch->count >> FIXED_POINT_SHIFT;
			while (ch->count >= 0)
			{
				ch->count -= FIXED_POINT;
				ch->nb_sample[ch->nb_index] = ch->raw_sample;
				ch->nb_index = (ch->nb_index + 1) & 3;

				ch->sample_pointer++;
				if (ch->sample_pointer == SOUND_DECODE_LENGTH)
				{
					ch->sample_pointer = 0;

					if (ch->last_block)
					{
						S9xAPUSetEndX (J);
						if (!ch->loop)
						{
							last_block = TRUE;
							S9xAPUSetEndOfSample (J, ch);
							while (ch->count >= 0)
							{
								ch->count -= FIXED_POINT;
								ch->nb_sample[ch->nb_index] = 0;
								ch->nb_index = (ch->nb_index + 1) & 3;
							}
							break;
						}
						else
						{
							ch->last_block = FALSE;
							ch->sample_number = APU.DSP[APU_SRCN + J * 0x10]; // SRCN might be different than before, so update it. This apparently fixes Mystical Ninja pipe sound.
							uint8 *dir = S9xGetSampleAddress (ch->sample_number);
							ch->block_pointer = READ_WORD(dir + 2);
						}
					}

					DecodeBlock (ch);
				}
				ch->raw_sample = ch->block[ch->sample_pointer];
			}

			int32 outx, d;

			if (ch->type == SOUND_SAMPLE)
			{
				if (Settings.InterpolatedSound)
				{
					// 4-point gaussian interpolation
					d = ch->count >> (FIXED_POINT_SHIFT - 8);
					outx  = ((G4(-d) * ch->nb_sample[ ch->nb_index         ]) >> 11);
					outx += ((G3(-d) * ch->nb_sample[(ch->nb_index + 1) & 3]) >> 11);
					outx += ((G2( d) * ch->nb_sample[(ch->nb_index + 2) & 3]) >> 11);
					outx  = sclip15(outx);
					outx += ((G1( d) * ch->nb_sample[(ch->nb_index + 3) & 3]) >> 11);
					outx  = sclamp15(outx);
				}
				else
					outx = ch->raw_sample;
			}
			else // SAMPLE_NOISE
			{
				noise_count -= SoundData.noise_rate;
				while (noise_count <= 0)
				{
					noise_count += env_counter_max;
					noise_index = (noise_index + 1) % SOUND_BUFFER_SIZE;
				}
				outx = sclip15(noise_cache[noise_index]);
			}

			assert(ENVX_SHIFT >= 4);
			outx = (outx * (ch->envxx >> (ENVX_SHIFT - 4))) >> 11;
			ch->sample = (short)((outx << 1) & 0xFFFF);

			if (mod2)
				wave[I >> 1] = outx << 1;
//				wave[I >> 1] = (outx * ch->envx) << 1;

			int32 VL, VR;

			if (sound_switch) {
				VL = ((outx * ch->volume_left ) >> 7) << 1;
				VR = ((outx * ch->volume_right) >> 7) << 1;
			}
			else
				VL = VR = 0;

			MixBuffer[I      ^ Settings.ReverseStereo ] += VL;
			MixBuffer[I + (1 ^ Settings.ReverseStereo)] += VR;
			ch->echo_buf_ptr[I      ^ Settings.ReverseStereo ] += VL;
			ch->echo_buf_ptr[I + (1 ^ Settings.ReverseStereo)] += VR;
		}

	stereo_exit: ;
	}
	DoFakeMute=0;

	if (APU.DSP[APU_NON])
		SoundData.noise_count = noise_count_next;
}

#ifdef __DJGPP
END_OF_FUNCTION(MixStereo);
#endif

void MixMono (int sample_count)
{
	DoFakeMute=FakeMute;

	int byte_count = so.sixteen_bit ? (sample_count << 1) : sample_count;
	int pitch_mod = SoundData.pitch_mod & (~APU.DSP[APU_NON]) & ~1;

	int32 noise_index = 0;
	int32 noise_count = SoundData.noise_count;
	int32 noise_count_next = SoundData.noise_count;

	// noise_rate cannot be changed during the mixing
	SoundData.noise_rate = env_counter_table[APU.DSP[APU_FLG] & 0x1F];

	if (APU.DSP[APU_NON])
	{
		noise_cache[noise_index] = SoundData.noise_seed;
		for (uint32 I = 0; I < (uint32) sample_count; I++)
		{
			noise_count -= SoundData.noise_rate;
			while (noise_count <= 0)
			{
				SoundData.noise_seed = (SoundData.noise_seed >> 1) | (((SoundData.noise_seed << 14) ^ (SoundData.noise_seed << 13)) & 0x4000);
				noise_index = (noise_index + 1) % SOUND_BUFFER_SIZE;
				noise_cache[noise_index] = SoundData.noise_seed;
				noise_count += env_counter_max;
			}
		}
		noise_count_next = noise_count;
	}

	for (uint32 J = 0; J < NUM_CHANNELS; J++) 
	{
		Channel *ch = &SoundData.channels[J];
		uint32 freq = (uint32) ((double) ch->frequency * 32000 / 32768);

		bool8 last_block = FALSE;

		if (ch->type == SOUND_NOISE)
		{
			noise_index = 0;
			noise_count = SoundData.noise_count;
		}

		bool8 mod1 = pitch_mod & (1 << J);
		bool8 mod2 = pitch_mod & (1 << (J + 1));
		bool8 sound_switch = so.sound_switch & (1 << J);

		if (ch->state == SOUND_SILENT || last_block) {
			// just in case
			ch->sample = 0;
			if (mod2)
				memset(wave, 0, byte_count * sizeof(int32));
			continue;
		}

		if (ch->needs_decode) 
		{
			DecodeBlock(ch);
			ch->needs_decode = FALSE;
			ch->raw_sample = ch->block[0];
			ch->sample_pointer = 0;
		}

		for (uint32 I = 0; I < (uint32) sample_count; I++)
		{
			ch->env_error += ch->erate;
			if (ch->env_error >= FIXED_POINT) 
			{
				uint32 step = ch->env_error >> FIXED_POINT_SHIFT;
				
				switch (ch->state)
				{
				case SOUND_ATTACK:
					ch->env_error &= FIXED_POINT_REMAINDER;
					ch->envx += step << 1;
					ch->envxx = ch->envx << ENVX_SHIFT;

					if (ch->envx >= 126)
					{
						ch->envx = 127;
						ch->envxx = 127 << ENVX_SHIFT;
						ch->state = SOUND_DECAY;
						if (ch->sustain_level != 8) 
						{
							S9xSetEnvRate (ch, ch->decay_rate, -1,
								(MAX_ENVELOPE_HEIGHT * ch->sustain_level)
								>> 3);
							break;
						}
						ch->state = SOUND_SUSTAIN;
						S9xSetEnvRate (ch, ch->sustain_rate, -1, 0);
					}
					break;

				case SOUND_DECAY:
					while (ch->env_error >= FIXED_POINT)
					{
						ch->envxx = (ch->envxx >> 8) * 255;
						ch->env_error -= FIXED_POINT;
					}
					ch->envx = ch->envxx >> ENVX_SHIFT;
					if (ch->envx <= ch->envx_target)
					{
						if (ch->envx <= 0)
						{
							S9xAPUSetEndOfSample (J, ch);
							goto mono_exit;
						}
						ch->state = SOUND_SUSTAIN;
						S9xSetEnvRate (ch, ch->sustain_rate, -1, 0);
					}
					break;

				case SOUND_SUSTAIN:
					while (ch->env_error >= FIXED_POINT)
					{
						ch->envxx = (ch->envxx >> 8) * 255;
						ch->env_error -= FIXED_POINT;
					}
					ch->envx = ch->envxx >> ENVX_SHIFT;
					if (ch->envx <= 0)
					{
						S9xAPUSetEndOfSample (J, ch);
						goto mono_exit;
					}
					break;

				case SOUND_RELEASE:
					while (ch->env_error >= FIXED_POINT)
					{
						ch->envxx -= (MAX_ENVELOPE_HEIGHT << ENVX_SHIFT) / 256;
						ch->env_error -= FIXED_POINT;
					}
					ch->envx = ch->envxx >> ENVX_SHIFT;
					if (ch->envx <= 0)
					{
						S9xAPUSetEndOfSample (J, ch);
						goto mono_exit;
					}
					break;

				case SOUND_INCREASE_LINEAR:
					ch->env_error &= FIXED_POINT_REMAINDER;
					ch->envx += step << 1;
					ch->envxx = ch->envx << ENVX_SHIFT;
					
					if (ch->envx >= 126)
					{
						ch->envx = 127;
						ch->envxx = 127 << ENVX_SHIFT;
						ch->state = SOUND_GAIN;
						ch->mode = MODE_GAIN;
						S9xSetEnvRate (ch, 0, -1, 0);
					}
					break;

				case SOUND_INCREASE_BENT_LINE:
					if (ch->envx >= (MAX_ENVELOPE_HEIGHT * 3) / 4)
					{
						while (ch->env_error >= FIXED_POINT)
						{
							ch->envxx += (MAX_ENVELOPE_HEIGHT << ENVX_SHIFT) / 256;
							ch->env_error -= FIXED_POINT;
						}
						ch->envx = ch->envxx >> ENVX_SHIFT;
					}
					else
					{
						ch->env_error &= FIXED_POINT_REMAINDER;
						ch->envx += step << 1;
						ch->envxx = ch->envx << ENVX_SHIFT;
					}
					
					if (ch->envx >= 126)
					{
						ch->envx = 127;
						ch->envxx = 127 << ENVX_SHIFT;
						ch->state = SOUND_GAIN;
						ch->mode = MODE_GAIN;
						S9xSetEnvRate (ch, 0, -1, 0);
					}
					break;

				case SOUND_DECREASE_LINEAR:
					ch->env_error &= FIXED_POINT_REMAINDER;
					ch->envx -= step << 1;
					ch->envxx = ch->envx << ENVX_SHIFT;
					if (ch->envx <= 0)
					{
						S9xAPUSetEndOfSample (J, ch);
						goto mono_exit;
					}
					break;

				case SOUND_DECREASE_EXPONENTIAL:
					while (ch->env_error >= FIXED_POINT)
					{
						ch->envxx = (ch->envxx >> 8) * 255;
						ch->env_error -= FIXED_POINT;
					}
					ch->envx = ch->envxx >> ENVX_SHIFT;
					if (ch->envx <= 0)
					{
						S9xAPUSetEndOfSample (J, ch);
						goto mono_exit;
					}
					break;

				case SOUND_GAIN:
					S9xSetEnvRate (ch, 0, -1, 0);
					break;
				}
			}

			ch->count += mod1 ? (((int64) freq * (32768 + wave[I])) >> 15) : freq;
//			ch->count += mod1 ? PITCH_MOD(freq, wave [I]) : freq;

			int32 count = ch->count >> FIXED_POINT_SHIFT;
			while (ch->count >= 0)
			{
				ch->count -= FIXED_POINT;
				ch->nb_sample[ch->nb_index] = ch->raw_sample;
				ch->nb_index = (ch->nb_index + 1) & 3;

				ch->sample_pointer++;
				if (ch->sample_pointer == SOUND_DECODE_LENGTH)
				{
					ch->sample_pointer = 0;

					if (ch->last_block)
					{
						S9xAPUSetEndX (J);
						if (!ch->loop)
						{
							last_block = TRUE;
							S9xAPUSetEndOfSample (J, ch);
							while (ch->count >= 0)
							{
								ch->count -= FIXED_POINT;
								ch->nb_sample[ch->nb_index] = 0;
								ch->nb_index = (ch->nb_index + 1) & 3;
							}
							break;
						}
						else
						{
							ch->last_block = FALSE;
							ch->sample_number = APU.DSP[APU_SRCN + J * 0x10]; // SRCN might be different than before, so update it. This apparently fixes Mystical Ninja pipe sound.
							uint8 *dir = S9xGetSampleAddress (ch->sample_number);
							ch->block_pointer = READ_WORD(dir + 2);
						}
					}

					DecodeBlock (ch);
				}
				ch->raw_sample = ch->block[ch->sample_pointer];
			}

			int32 outx, d;

			if (ch->type == SOUND_SAMPLE)
			{
				if (Settings.InterpolatedSound)
				{
					// 4-point gaussian interpolation
					d = ch->count >> (FIXED_POINT_SHIFT - 8);
					outx  = ((G4(-d) * ch->nb_sample[ ch->nb_index         ]) >> 11);
					outx += ((G3(-d) * ch->nb_sample[(ch->nb_index + 1) & 3]) >> 11);
					outx += ((G2( d) * ch->nb_sample[(ch->nb_index + 2) & 3]) >> 11);
					outx  = sclip15(outx);
					outx += ((G1( d) * ch->nb_sample[(ch->nb_index + 3) & 3]) >> 11);
					outx  = sclamp15(outx);
				}
				else
					outx = ch->raw_sample;
			}
			else // SAMPLE_NOISE
			{
				noise_count -= SoundData.noise_rate;
				while (noise_count <= 0)
				{
					noise_count += env_counter_max;
					noise_index = (noise_index + 1) % SOUND_BUFFER_SIZE;
				}
				outx = sclip15(noise_cache[noise_index]);
			}

			assert(ENVX_SHIFT >= 4);
			outx = (outx * (ch->envxx >> (ENVX_SHIFT - 4))) >> 11;
			ch->sample = (short)((outx << 1) & 0xFFFF);

			if (mod2)
				wave[I] = outx << 1;
//				wave[I] = (outx * ch->envx) << 1;

			int32 V;

			if (sound_switch)
				V = ((outx * ch->volume_left ) >> 7) << 1;
			else
				V = 0;

			MixBuffer [I] += V;
			ch->echo_buf_ptr [I] += V;
		}

	mono_exit: ;
	}
	DoFakeMute=0;

	if (APU.DSP[APU_NON])
		SoundData.noise_count = noise_count_next;
}
#ifdef __DJGPP
END_OF_FUNCTION(MixMono);
#endif

#ifdef __sun
extern uint8 int2ulaw (int);
#endif

// For backwards compatibility with older port specific code
void S9xMixSamplesO (uint8 *buffer, int sample_count, int byte_offset)
{
    S9xMixSamples (buffer+byte_offset, sample_count);
}
#ifdef __DJGPP
END_OF_FUNCTION(S9xMixSamplesO);
#endif

void S9xMixSamples (uint8 *buffer, int sample_count)
{
	int I, J;

	memset (MixBuffer, 0, sample_count * sizeof (MixBuffer [0]));
	if (SoundData.echo_enable)
		memset (EchoBuffer, 0, sample_count * sizeof (EchoBuffer [0]));

	if (so.stereo)
		MixStereo (sample_count);
	else
		MixMono (sample_count);

	/* Mix and convert waveforms */
	if (so.sixteen_bit)
	{
		if (SoundData.echo_enable && SoundData.echo_buffer_size)
		{
			if (so.stereo)
			{
				// 16-bit stereo sound with echo enabled ...
				if (SoundData.no_filter)
				{
					// ... but no filter defined.
					for (J = 0; J < sample_count; J++)
					{
						int E = Echo [SoundData.echo_ptr];

						Loop[FIRIndex & 15] = E;
						E = (E * 127) >> 7;
						FIRIndex++;

						if (SoundData.echo_write_enabled)
						{
							I = EchoBuffer[J] + ((E * SoundData.echo_feedback) >> 7);
							Echo[SoundData.echo_ptr] = sclamp16(I);
						}
						else // FIXME: Snes9x's echo buffer is not in APU_RAM
							Echo[SoundData.echo_ptr] = 0;

						if (++SoundData.echo_ptr >= SoundData.echo_buffer_size)
							SoundData.echo_ptr = 0;

						I = (MixBuffer[J] * SoundData.master_volume[J & 1] +
							E * SoundData.echo_volume[J & 1]) >> 7;
						((int16 *) buffer) [J] = sclamp16(I);
					}
				}
				else
				{
					// ... with filter defined.
					for (J = 0; J < sample_count; J++)
					{
						int E = Echo [SoundData.echo_ptr];

						Loop[FIRIndex & 15] = E;
						E  = E                          * FilterTaps[0];
						E += Loop[(FIRIndex -  2) & 15] * FilterTaps[1];
						E += Loop[(FIRIndex -  4) & 15] * FilterTaps[2];
						E += Loop[(FIRIndex -  6) & 15] * FilterTaps[3];
						E += Loop[(FIRIndex -  8) & 15] * FilterTaps[4];
						E += Loop[(FIRIndex - 10) & 15] * FilterTaps[5];
						E += Loop[(FIRIndex - 12) & 15] * FilterTaps[6];
						E += Loop[(FIRIndex - 14) & 15] * FilterTaps[7];
						E >>= 7;
						FIRIndex++;

						if (SoundData.echo_write_enabled)
						{
							I = EchoBuffer[J] + ((E * SoundData.echo_feedback) >> 7);
							Echo[SoundData.echo_ptr] = sclamp16(I);
						}
						else // FIXME: Snes9x's echo buffer is not in APU_RAM
							Echo[SoundData.echo_ptr] = 0;

						if (++SoundData.echo_ptr >= SoundData.echo_buffer_size)
							SoundData.echo_ptr = 0;

						I = (MixBuffer[J] * SoundData.master_volume[J & 1] +
							E * SoundData.echo_volume[J & 1]) >> 7;
						((int16 *) buffer) [J] = sclamp16(I);
					}
				}
			}
			else
			{
				// 16-bit mono sound with echo enabled...
				if (SoundData.no_filter)
				{
					// ... no filter defined
					for (J = 0; J < sample_count; J++)
					{
						int E = Echo[SoundData.echo_ptr];

						Loop[FIRIndex & 7] = E;
						E = (E * 127) >> 7;
						FIRIndex++;

						if (SoundData.echo_write_enabled)
						{
							I = EchoBuffer[J] + ((E * SoundData.echo_feedback) >> 7);
							Echo[SoundData.echo_ptr] = sclamp16(I);
						}
						else // FIXME: Snes9x's echo buffer is not in APU_RAM
							Echo[SoundData.echo_ptr] = 0;

						if (++SoundData.echo_ptr >= SoundData.echo_buffer_size)
							SoundData.echo_ptr = 0;

						I = (MixBuffer[J] * SoundData.master_volume[0] +
							E * SoundData.echo_volume[0]) >> 7;
						((int16 *) buffer) [J] = sclamp16(I);
					}
				}
				else
				{
					// ... with filter defined
					for (J = 0; J < sample_count; J++)
					{
						int E = Echo[SoundData.echo_ptr];

						Loop[FIRIndex & 7] = E;
						E  = E                        * FilterTaps[0];
						E += Loop[(FIRIndex - 1) & 7] * FilterTaps[1];
						E += Loop[(FIRIndex - 2) & 7] * FilterTaps[2];
						E += Loop[(FIRIndex - 3) & 7] * FilterTaps[3];
						E += Loop[(FIRIndex - 4) & 7] * FilterTaps[4];
						E += Loop[(FIRIndex - 5) & 7] * FilterTaps[5];
						E += Loop[(FIRIndex - 6) & 7] * FilterTaps[6];
						E += Loop[(FIRIndex - 7) & 7] * FilterTaps[7];
						E >>= 7;
						FIRIndex++;

						if (SoundData.echo_write_enabled)
						{
							I = EchoBuffer[J] + ((E * SoundData.echo_feedback) >> 7);
							Echo[SoundData.echo_ptr] = sclamp16(I);
						}
						else // FIXME: Snes9x's echo buffer is not in APU_RAM
							Echo[SoundData.echo_ptr] = 0;

						if (++SoundData.echo_ptr >= SoundData.echo_buffer_size)
							SoundData.echo_ptr = 0;

						I = (MixBuffer[J] * SoundData.master_volume[0] +
							E * SoundData.echo_volume[0]) >> 7;
						((int16 *) buffer) [J] = sclamp16(I);
					}
				}
			}
		}
		else
		{
			// 16-bit mono or stereo sound, no echo
			for (J = 0; J < sample_count; J++)
			{
				I = (MixBuffer[J] * SoundData.master_volume[J & 1]) >> 7;
				((int16 *) buffer) [J] = sclamp16(I);
			}
		}
	}
	else
	{
#ifdef __sun
		if (so.encoded)
		{
			for (J = 0; J < sample_count; J++)
			{
				I = (MixBuffer[J] * SoundData.master_volume[0]) >> 7;
				buffer[J] = int2ulaw (sclamp16(I));
			}
		}
		else
#endif
		{
			if (SoundData.echo_enable && SoundData.echo_buffer_size)
			{
				if (so.stereo)
				{
					// 8-bit stereo sound with echo enabled...
					if (SoundData.no_filter)
					{
						// ... but no filter
						for (J = 0; J < sample_count; J++)
						{
							int E = Echo[SoundData.echo_ptr];

							Loop[FIRIndex & 15] = E;
							E = (E * 127) >> 7;
							FIRIndex++;

							if (SoundData.echo_write_enabled)
							{
								I = EchoBuffer[J] + ((E * SoundData.echo_feedback) >> 7);
								Echo[SoundData.echo_ptr] = sclamp16(I);
							}
							else // FIXME: Snes9x's echo buffer is not in APU_RAM
								Echo[SoundData.echo_ptr] = 0;

							if (++SoundData.echo_ptr >= SoundData.echo_buffer_size)
								SoundData.echo_ptr = 0;

							I = (MixBuffer[J] * SoundData.master_volume[J & 1] +
								E * SoundData.echo_volume[J & 1]) >> 15;
							buffer[J] = sclamp8(I) + 128;
						}
					}
					else
					{
						// ... with filter
						for (J = 0; J < sample_count; J++)
						{
							int E = Echo[SoundData.echo_ptr];

							Loop[FIRIndex & 15] = E;
							E  = E                          * FilterTaps[0];
							E += Loop[(FIRIndex -  2) & 15] * FilterTaps[1];
							E += Loop[(FIRIndex -  4) & 15] * FilterTaps[2];
							E += Loop[(FIRIndex -  6) & 15] * FilterTaps[3];
							E += Loop[(FIRIndex -  8) & 15] * FilterTaps[4];
							E += Loop[(FIRIndex - 10) & 15] * FilterTaps[5];
							E += Loop[(FIRIndex - 12) & 15] * FilterTaps[6];
							E += Loop[(FIRIndex - 14) & 15] * FilterTaps[7];
							E >>= 7;
							FIRIndex++;

							if (SoundData.echo_write_enabled)
							{
								I = EchoBuffer[J] + ((E * SoundData.echo_feedback) >> 7);
								Echo[SoundData.echo_ptr] = sclamp16(I);
							}
							else // FIXME: Snes9x's echo buffer is not in APU_RAM
								Echo[SoundData.echo_ptr] = 0;

							if (++SoundData.echo_ptr >= SoundData.echo_buffer_size)
								SoundData.echo_ptr = 0;

							I = (MixBuffer[J] * SoundData.master_volume[J & 1] +
								E * SoundData.echo_volume[J & 1]) >> 15;
							buffer[J] = sclamp8(I) + 128;
						}
					}
				}
				else
				{
					// 8-bit mono sound with echo enabled...
					if (SoundData.no_filter)
					{
						// ... but no filter.
						for (J = 0; J < sample_count; J++)
						{
							int E = Echo[SoundData.echo_ptr];

							Loop[FIRIndex & 7] = E;
							E = (E * 127) >> 7;
							FIRIndex++;

							if (SoundData.echo_write_enabled)
							{
								I = EchoBuffer[J] + ((E * SoundData.echo_feedback) >> 7);
								Echo[SoundData.echo_ptr] = sclamp16(I);
							}
							else // FIXME: Snes9x's echo buffer is not in APU_RAM
								Echo[SoundData.echo_ptr] = 0;

							if (++SoundData.echo_ptr >= SoundData.echo_buffer_size)
								SoundData.echo_ptr = 0;

							I = (MixBuffer[J] * SoundData.master_volume[0] +
								E * SoundData.echo_volume[0]) >> 15;
							buffer[J] = sclamp8(I) + 128;
						}
					}
					else
					{
						// ... with filter.
						for (J = 0; J < sample_count; J++)
						{
							int E = Echo[SoundData.echo_ptr];

							Loop[FIRIndex & 7] = E;
							E  = E                        * FilterTaps[0];
							E += Loop[(FIRIndex - 1) & 7] * FilterTaps[1];
							E += Loop[(FIRIndex - 2) & 7] * FilterTaps[2];
							E += Loop[(FIRIndex - 3) & 7] * FilterTaps[3];
							E += Loop[(FIRIndex - 4) & 7] * FilterTaps[4];
							E += Loop[(FIRIndex - 5) & 7] * FilterTaps[5];
							E += Loop[(FIRIndex - 6) & 7] * FilterTaps[6];
							E += Loop[(FIRIndex - 7) & 7] * FilterTaps[7];
							E >>= 7;
							FIRIndex++;

							if (SoundData.echo_write_enabled)
							{
								I = EchoBuffer[J] + ((E * SoundData.echo_feedback) >> 7);
								Echo[SoundData.echo_ptr] = sclamp16(I);
							}
							else // FIXME: Snes9x's echo buffer is not in APU_RAM
								Echo[SoundData.echo_ptr] = 0;

							if (++SoundData.echo_ptr >= SoundData.echo_buffer_size)
								SoundData.echo_ptr = 0;

							I = (MixBuffer[J] * SoundData.master_volume[0] +
								E * SoundData.echo_volume[0]) >> 15;
							buffer[J] = sclamp8(I) + 128;
						}
					}
				}
			}
			else
			{
				// 8-bit mono or stereo sound, no echo
				for (J = 0; J < sample_count; J++)
				{
					I = (MixBuffer[J] * SoundData.master_volume[J & 1]) >> 15;
					buffer[J] = sclamp8(I) + 128;
				}
			}
		}
	}

	if (so.mute_sound)
	{
		if (so.sixteen_bit)
			memset (buffer, 0, sample_count << 1);
		else
			memset (buffer, 128, sample_count);
	}
}

#ifdef __DJGPP
END_OF_FUNCTION(S9xMixSamples);
#endif

void S9xResetSound (bool8 full)
{
    for (int i = 0; i < 8; i++)
    {
		SoundData.channels[i].state = SOUND_SILENT;
		SoundData.channels[i].mode = MODE_NONE;
		SoundData.channels[i].type = SOUND_SAMPLE;
		SoundData.channels[i].volume_left = 0;
		SoundData.channels[i].volume_right = 0;
		SoundData.channels[i].hertz = 0;
		SoundData.channels[i].count = 0;
		SoundData.channels[i].loop = FALSE;
		SoundData.channels[i].envx_target = 0;
		SoundData.channels[i].env_error = 0;
		SoundData.channels[i].erate = 0;
		SoundData.channels[i].envx = 0;
		SoundData.channels[i].envxx = 0;
		SoundData.channels[i].direction = 0;
		SoundData.channels[i].attack_rate = 0;
		SoundData.channels[i].decay_rate = 0;
		SoundData.channels[i].sustain_rate = 0;
		SoundData.channels[i].release_rate = 0;
		SoundData.channels[i].sustain_level = 0;
		SoundData.echo_ptr = 0;
		SoundData.echo_feedback = 0;
		SoundData.echo_buffer_size = 1;
    }
    FilterTaps [0] = 127;
    FilterTaps [1] = 0;
    FilterTaps [2] = 0;
    FilterTaps [3] = 0;
    FilterTaps [4] = 0;
    FilterTaps [5] = 0;
    FilterTaps [6] = 0;
    FilterTaps [7] = 0;
    so.mute_sound = TRUE;
	SoundData.noise_count = env_counter_max;
	SoundData.noise_seed = 0x4000;
    so.samples_mixed_so_far = 0;
    so.play_position = 0;
    so.err_counter = 0;
	
    if (full)
    {
		SoundData.master_volume_left = 0;
		SoundData.master_volume_right = 0;
		SoundData.echo_volume_left = 0;
		SoundData.echo_volume_right = 0;
		SoundData.echo_enable = 0;
		SoundData.echo_write_enabled = 0;
		SoundData.echo_channel_enable = 0;
		SoundData.pitch_mod = 0;
		SoundData.dummy[0] = 0;
		SoundData.dummy[1] = 0;
		SoundData.dummy[2] = 0;
		SoundData.master_volume[0] = 0;
		SoundData.master_volume[1] = 0;
		SoundData.echo_volume[0] = 0;
		SoundData.echo_volume[1] = 0;
		SoundData.noise_hertz = 0;
		memset (Loop, 0, sizeof (Loop));
		memset (Echo, 0, sizeof (Echo));
    }

    SoundData.master_volume_left = 127;
    SoundData.master_volume_right = 127;
    SoundData.master_volume [0] = SoundData.master_volume [1] = 127;
    if (so.playback_rate) {
		so.err_rate = (uint32) (FIXED_POINT * SNES_SCANLINE_TIME / (1.0 / so.playback_rate));
		env_counter_max = env_counter_max_master * so.playback_rate / 32000;
	}
    else {
		so.err_rate = 0;
		env_counter_max = 0;
	}
    SoundData.no_filter = TRUE;
}

void S9xSetPlaybackRate (uint32 playback_rate)
{
    so.playback_rate = playback_rate;
    so.err_rate = (uint32) (SNES_SCANLINE_TIME * FIXED_POINT / (1.0 / (double) so.playback_rate));
	env_counter_max = env_counter_max_master * so.playback_rate / 32000;

    S9xSetEchoDelay (APU.DSP [APU_EDL] & 0xf);
    for (int i = 0; i < 8; i++)
		S9xSetSoundFrequency (i, SoundData.channels [i].hertz);
}

bool8 S9xInitSound (int mode, bool8 stereo, int buffer_size)
{
    so.sound_fd = -1;
    so.sound_switch = 255;
	
    so.playback_rate = 32000;
    so.buffer_size = 0;
    so.stereo = stereo;
    so.sixteen_bit = Settings.SixteenBitSound;
    so.encoded = FALSE;
    so.pitch_mul = 1.0;
    
    S9xResetSound (TRUE);
	
    if (!(mode & 7))
		return (1);
	
    S9xSetSoundMute (TRUE);
    if (!S9xOpenSoundDevice (mode, stereo, buffer_size))
    {
#ifdef NOSOUND
                S9xMessage (S9X_WARNING, S9X_SOUND_NOT_BUILT,
                            "No sound support compiled in");
#else
		S9xMessage (S9X_ERROR, S9X_SOUND_DEVICE_OPEN_FAILED,
                            "Sound device open failed");
#endif
		return (0);
    }
	
    return (1);
}

bool8 S9xSetSoundMode (int channel, int mode)
{
    Channel *ch = &SoundData.channels[channel];
	
    switch (mode)
    {
    case MODE_RELEASE:
		if (ch->mode != MODE_NONE)
		{
			ch->mode = MODE_RELEASE;
			return (TRUE);
		}
		break;
		
    case MODE_DECREASE_LINEAR:
    case MODE_DECREASE_EXPONENTIAL:
    case MODE_GAIN:
		if (ch->mode != MODE_RELEASE)
		{
			ch->mode = mode;
			if (ch->state != SOUND_SILENT)
				ch->state = mode;
			
			return (TRUE);
		}
		break;
		
    case MODE_INCREASE_LINEAR:
    case MODE_INCREASE_BENT_LINE:
		if (ch->mode != MODE_RELEASE)
		{
			ch->mode = mode;
			if (ch->state != SOUND_SILENT)
				ch->state = mode;
			
			return (TRUE);
		}
		break;
		
    case MODE_ADSR:
		if (ch->mode == MODE_NONE || ch->mode == MODE_ADSR)
		{
			ch->mode = mode;
			return (TRUE);
		}
    }
	
    return (FALSE);
}

void S9xSetSoundControl (int sound_switch)
{
    so.sound_switch = sound_switch;
}

void S9xPlaySample (int channel)
{
    Channel *ch = &SoundData.channels[channel];
    
    ch->state = SOUND_SILENT;
    ch->mode = MODE_NONE;
    ch->envx = 0;
    ch->envxx = 0;
	
    S9xFixEnvelope (channel,
		APU.DSP [APU_GAIN  + (channel << 4)], 
		APU.DSP [APU_ADSR1 + (channel << 4)],
		APU.DSP [APU_ADSR2 + (channel << 4)]);
	
    ch->sample_number = APU.DSP [APU_SRCN + channel * 0x10];
    if (APU.DSP [APU_NON] & (1 << channel))
		ch->type = SOUND_NOISE;
    else
		ch->type = SOUND_SAMPLE;
	
    S9xSetSoundFrequency (channel, ch->hertz);
    ch->loop = FALSE;
    ch->needs_decode = TRUE;
    ch->last_block = FALSE;
    ch->previous [0] = ch->previous[1] = 0;
    uint8 *dir = S9xGetSampleAddress (ch->sample_number);
    ch->block_pointer = READ_WORD (dir);
    ch->sample_pointer = 0;
    ch->env_error = 0;
    ch->next_sample = 0;
    ch->interpolate = 0;
	ch->count = 3 * FIXED_POINT; // since gaussian interpolation uses 4 points
	ch->nb_sample[0] = 0;
	ch->nb_sample[1] = 0;
	ch->nb_sample[2] = 0;
	ch->nb_sample[3] = 0;
	ch->nb_index = 0;
    switch (ch->mode)
    {
    case MODE_ADSR:
		if (ch->attack_rate == 0)
		{
			if (ch->decay_rate == 0 || ch->sustain_level == 8)
			{
				ch->state = SOUND_SUSTAIN;
				ch->envx = (MAX_ENVELOPE_HEIGHT * ch->sustain_level) >> 3;
				S9xSetEnvRate (ch, ch->sustain_rate, -1, 0);
			}
			else
			{
				ch->state = SOUND_DECAY;
				ch->envx = MAX_ENVELOPE_HEIGHT;
				S9xSetEnvRate (ch, ch->decay_rate, -1, 
					(MAX_ENVELOPE_HEIGHT * ch->sustain_level) >> 3);
			}
		}
		else
		{
			ch->state = SOUND_ATTACK;
			ch->envx = 0;
			S9xSetEnvRate (ch, ch->attack_rate, 1, MAX_ENVELOPE_HEIGHT);
		}
		ch->envxx = ch->envx << ENVX_SHIFT;
		break;
		
    case MODE_GAIN:
		ch->state = SOUND_GAIN;
		break;
		
    case MODE_INCREASE_LINEAR:
		ch->state = SOUND_INCREASE_LINEAR;
		break;
		
    case MODE_INCREASE_BENT_LINE:
		ch->state = SOUND_INCREASE_BENT_LINE;
		break;
		
    case MODE_DECREASE_LINEAR:
		ch->state = SOUND_DECREASE_LINEAR;
		break;
		
    case MODE_DECREASE_EXPONENTIAL:
		ch->state = SOUND_DECREASE_EXPONENTIAL;
		break;
		
    default:
		break;
    }
	
    S9xFixEnvelope (channel,
		APU.DSP [APU_GAIN  + (channel << 4)], 
		APU.DSP [APU_ADSR1 + (channel << 4)],
		APU.DSP [APU_ADSR2 + (channel << 4)]);
}
