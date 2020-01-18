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



#include <string.h>
#include "memmap.h"
#include "obc1.h"

static uint8 *OBC1_RAM = NULL;

extern "C"
{
int OBC1_Address;
int OBC1_BasePtr;
int OBC1_Shift;

uint8 GetOBC1 (uint16 Address)
{
	switch(Address) {
		case 0x7ff0:
			return OBC1_RAM[OBC1_BasePtr + (OBC1_Address << 2)];

		case 0x7ff1:
			return OBC1_RAM[OBC1_BasePtr + (OBC1_Address << 2) + 1];

		case 0x7ff2:
			return OBC1_RAM[OBC1_BasePtr + (OBC1_Address << 2) + 2];

		case 0x7ff3:
			return OBC1_RAM[OBC1_BasePtr + (OBC1_Address << 2) + 3];

		case 0x7ff4:
			return OBC1_RAM[OBC1_BasePtr + (OBC1_Address >> 2) + 0x200];
	}

	return OBC1_RAM[Address & 0x1fff];
}

void SetOBC1 (uint8 Byte, uint16 Address)
{
	switch(Address) {
		case 0x7ff0:
		{
			OBC1_RAM[OBC1_BasePtr + (OBC1_Address << 2)] = Byte;
			break;
		}

		case 0x7ff1:
		{
			OBC1_RAM[OBC1_BasePtr + (OBC1_Address << 2) + 1] = Byte;
			break;
		}

		case 0x7ff2:
		{
			OBC1_RAM[OBC1_BasePtr + (OBC1_Address << 2) + 2] = Byte;
			break;
		}

		case 0x7ff3:
		{
			OBC1_RAM[OBC1_BasePtr + (OBC1_Address << 2) + 3] = Byte;
			break;
		}

		case 0x7ff4:
		{
			unsigned char Temp;

			Temp = OBC1_RAM[OBC1_BasePtr + (OBC1_Address >> 2) + 0x200];
			Temp = (Temp & ~(3 << OBC1_Shift)) | ((Byte & 3) << OBC1_Shift);
			OBC1_RAM[OBC1_BasePtr + (OBC1_Address >> 2) + 0x200] = Temp;
			break;
		}

		case 0x7ff5:
		{
			if (Byte & 1)
				OBC1_BasePtr = 0x1800;
			else
				OBC1_BasePtr = 0x1c00;

			break;
		}

		case 0x7ff6:
		{
			OBC1_Address = Byte & 0x7f;
			OBC1_Shift = (Byte & 3) << 1;
			break;
		}
	}

	OBC1_RAM[Address & 0x1fff] = Byte;
}

uint8 * S9xGetBasePointerOBC1 (uint16 Address)
{
	if (Address >= 0x7ff0 && Address <= 0x7ff6)
		return (NULL);
	return (OBC1_RAM - 0x6000);
}

uint8 * S9xGetMemPointerOBC1 (uint16 Address)
{
	if (Address >= 0x7ff0 && Address <= 0x7ff6)
		return (NULL);
	return (OBC1_RAM - 0x6000 + (Address & 0xffff));
}

void ResetOBC1()
{
	OBC1_RAM = &Memory.FillRAM[0x6000];

	if (OBC1_RAM[0x1ff5] & 1)
		OBC1_BasePtr = 0x1800;
	else
		OBC1_BasePtr = 0x1c00;

	OBC1_Address = OBC1_RAM[0x1ff6] & 0x7f;
	OBC1_Shift = (OBC1_RAM[0x1ff6] & 3) << 1;
}

}
