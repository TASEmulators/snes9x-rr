/**********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  (c) Copyright 1996 - 2002  Gary Henderson (gary.henderson@ntlworld.com),
                             Jerremy Koot (jkoot@snes9x.com)

  (c) Copyright 2002 - 2004  Matthew Kendora

  (c) Copyright 2002 - 2005  Peter Bortas (peter@bortas.org)

  (c) Copyright 2004 - 2005  Joel Yliluoma (http://iki.fi/bisqwit/)

  (c) Copyright 2001 - 2006  John Weidman (jweidman@slip.net)

  (c) Copyright 2002 - 2006  funkyass (funkyass@spam.shaw.ca),
                             Kris Bleakley (codeviolation@hotmail.com)

  (c) Copyright 2002 - 2007  Brad Jorsch (anomie@users.sourceforge.net),
                             Nach (n-a-c-h@users.sourceforge.net),
                             zones (kasumitokoduck@yahoo.com)

  (c) Copyright 2006 - 2007  nitsuja


  BS-X C emulator code
  (c) Copyright 2005 - 2006  Dreamer Nom,
                             zones

  C4 x86 assembler and some C emulation code
  (c) Copyright 2000 - 2003  _Demo_ (_demo_@zsnes.com),
                             Nach,
                             zsKnight (zsknight@zsnes.com)

  C4 C++ code
  (c) Copyright 2003 - 2006  Brad Jorsch,
                             Nach

  DSP-1 emulator code
  (c) Copyright 1998 - 2006  _Demo_,
                             Andreas Naive (andreasnaive@gmail.com)
                             Gary Henderson,
                             Ivar (ivar@snes9x.com),
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora,
                             Nach,
                             neviksti (neviksti@hotmail.com)

  DSP-2 emulator code
  (c) Copyright 2003         John Weidman,
                             Kris Bleakley,
                             Lord Nightmare (lord_nightmare@users.sourceforge.net),
                             Matthew Kendora,
                             neviksti


  DSP-3 emulator code
  (c) Copyright 2003 - 2006  John Weidman,
                             Kris Bleakley,
                             Lancer,
                             z80 gaiden

  DSP-4 emulator code
  (c) Copyright 2004 - 2006  Dreamer Nom,
                             John Weidman,
                             Kris Bleakley,
                             Nach,
                             z80 gaiden

  OBC1 emulator code
  (c) Copyright 2001 - 2004  zsKnight,
                             pagefault (pagefault@zsnes.com),
                             Kris Bleakley,
                             Ported from x86 assembler to C by sanmaiwashi

  SPC7110 and RTC C++ emulator code
  (c) Copyright 2002         Matthew Kendora with research by
                             zsKnight,
                             John Weidman,
                             Dark Force

  S-DD1 C emulator code
  (c) Copyright 2003         Brad Jorsch with research by
                             Andreas Naive,
                             John Weidman

  S-RTC C emulator code
  (c) Copyright 2001-2006    byuu,
                             John Weidman

  ST010 C++ emulator code
  (c) Copyright 2003         Feather,
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora

  Super FX x86 assembler emulator code
  (c) Copyright 1998 - 2003  _Demo_,
                             pagefault,
                             zsKnight,

  Super FX C emulator code
  (c) Copyright 1997 - 1999  Ivar,
                             Gary Henderson,
                             John Weidman

  Sound DSP emulator code is derived from SNEeSe and OpenSPC:
  (c) Copyright 1998 - 2003  Brad Martin
  (c) Copyright 1998 - 2006  Charles Bilyue'

  SH assembler code partly based on x86 assembler code
  (c) Copyright 2002 - 2004  Marcus Comstedt (marcus@mc.pp.se)

  2xSaI filter
  (c) Copyright 1999 - 2001  Derek Liauw Kie Fa

  HQ2x, HQ3x, HQ4x filters
  (c) Copyright 2003         Maxim Stepin (maxim@hiend3d.com)

  Win32 GUI code
  (c) Copyright 2003 - 2006  blip,
                             funkyass,
                             Matthew Kendora,
                             Nach,
                             nitsuja

  Mac OS GUI code
  (c) Copyright 1998 - 2001  John Stiles
  (c) Copyright 2001 - 2007  zones


  Specific ports contains the works of other authors. See headers in
  individual files.


  Snes9x homepage: http://www.snes9x.com

  Permission to use, copy, modify and/or distribute Snes9x in both binary
  and source form, for non-commercial purposes, is hereby granted without
  fee, providing that this license information and copyright notice appear
  with all copies and any derived work.

  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software or it's derivatives.

  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes,
  but is not limited to, charging money for Snes9x or software derived from
  Snes9x, including Snes9x or derivatives in commercial game bundles, and/or
  using Snes9x as a promotion for your commercial product.

  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.

  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
**********************************************************************************/



#include <stdio.h>
#include <stdlib.h>
#include "snes9x.h"
#include "gfx.h"
#include "soundux.h"
#include "movie.h"
#include "display.h"
#include "logger.h"

#if !(defined(__unix) || defined(__linux) || defined(__sun) || defined(__DJGPP))
#define __builtin_expect(exp,c) ((exp)!=(c))
#endif

int dumpstreams = 0;
unsigned long maxframes = -1;
unsigned long ignoreframes = 0;

static int resetno = 0;
static int framecounter = 0;

int fastforwardpoint = -1;
int fastforwarddistance = 0;

int keypressscreen = 0;

static int drift = 0;

FILE *video=NULL, *audio=NULL;
char autodemo[128] = "";

int Logger_FrameCounter()
{
	return framecounter;
}

void Logger_NextFrame()
{
	framecounter++;
}

void breakpoint()
{

}

void ResetLogger()
{
	char buffer[256*224*4];

	if (!dumpstreams)
		return;

	framecounter = 0;
	drift=0;

	if (!resetno) { // don't create new files because of resets
	if (video)
		fclose(video);
	if (audio)
		fclose(audio);

	sprintf(buffer, "videostream%d.dat", resetno);
	video = fopen(buffer, "wb");
	if (!video)
	{
		printf("Opening %s failed. Logging cancelled.\n", buffer);
		return;
	}

	sprintf(buffer, "audiostream%d.dat", resetno);
	audio = fopen(buffer, "wb");
	if (!audio)
	{
		printf("Opening %s failed. Logging cancelled.\n", buffer);
		fclose(video);
		return;
	}

	char *logo = getenv("LOGO");
	if (!logo)
		logo = "logo.dat";
	FILE *l = fopen(logo, "rb");
	if (l)
	{
		const int soundsize = (so.sixteen_bit ? 2 : 1)*(so.stereo?2:1)*so.playback_rate * Settings.FrameTime / 1000000;
		printf("Soundsize: %d\n", soundsize);
		while (!feof(l))
		{
			if (fread(buffer, 1024,224, l) != 224)
				break;
			VideoLogger(buffer, 256, 224, 4,1024);
			memset(buffer, 0, soundsize);
			AudioLogger(buffer, soundsize);
		}
		fclose(l);
	}
	}
	resetno++;
}

char message[128];
int messageframe;


void VideoLogger(void *pixels, int width, int height, int depth, int bytes_per_line)
{
	int fc = S9xMovieGetFrameCounter();
	if (fc > 0)
		framecounter = fc;
	else
		framecounter++;
	
	if (video)
	{
		int i;
		char *data = (char*)pixels;
		static int lastwidth = width; // first resolution
		static int lastheight = height;
		if (lastwidth != width || lastheight != height) // this is just for informing the encoder that something has changed
		{
			printf("Frame %d, resolution changed from %dx%d to %dx%d!\n", fc, lastwidth, lastheight, width, height);
			lastwidth = width;
			lastheight = height;
		}
		for (i=0; i < height; i++)
			fwrite(data + i*bytes_per_line, depth, width, video);
		fflush(video);
		fflush(audio);
		drift++;

		if (maxframes > 0 && __builtin_expect((unsigned)framecounter >= maxframes, 0))
		{
			printf("-maxframes hit\ndrift:%d\n",drift);
			S9xExit();
		}

	}

	if (Settings.DisplayPressedKeys==1 || keypressscreen)
	{
		uint16 MovieGetJoypad(int i);

		int buttons = MovieGetJoypad(0);
		static char buffer[128];

		// This string spacing pattern is optimized for the 256 pixel wide screen.
                sprintf(buffer, "%s  %s  %s  %s  %s  %s  %c%c%c%c%c%c",
		buttons & SNES_START_MASK ? "Start" : "_____",
		buttons & SNES_SELECT_MASK ? "Select" : "______",
                buttons & SNES_UP_MASK ? "Up" : "__",
		buttons & SNES_DOWN_MASK ? "Down" : "____",
		buttons & SNES_LEFT_MASK ? "Left" : "____",
		buttons & SNES_RIGHT_MASK ? "Right" : "_____",
		buttons & SNES_A_MASK ? 'A':'_',
		buttons & SNES_B_MASK ? 'B':'_',
                buttons & SNES_X_MASK ? 'X':'_',
                buttons & SNES_Y_MASK ? 'Y':'_',
		buttons & SNES_TL_MASK ? 'L':'_',
		buttons & SNES_TR_MASK ? 'R':'_'
		/*framecounter*/);
		if (Settings.DisplayPressedKeys==1)
			fprintf(stderr, "%s %d           \r", buffer, framecounter);
		//if (keypressscreen)
                S9xSetInfoString(buffer);
	}

	if (__builtin_expect(messageframe >= 0 && framecounter == messageframe, 0))
	{
		S9xMessage(S9X_INFO, S9X_MOVIE_INFO, message);
		GFX.InfoStringTimeout = 300;
		messageframe = -1;
	}

/*	if (__builtin_expect(fastforwardpoint >= 0 && framecounter >= fastforwardpoint, 0))
	{
		Settings.FramesToSkip = fastforwarddistance;
		fastforwardpoint = -1;
	}*/
}


void AudioLogger(void *samples, int length)
{
	if (audio)
		fwrite(samples, 1, length, audio);
	drift--;
}
