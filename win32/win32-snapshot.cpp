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

#include "../port.h"
#include "../snes9x.h"
#include "../display.h"
#include "../movie.h"
#include "../ppu.h"
#include "../gfx.h"

#include "wsnes9x.h"
#include "lazymacro.h"
#include "win32-snapshot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <windows.h>

extern bool8 pad_read, pad_read_last;
extern void S9xReRefresh();

/*****************************************************************************/

bool GetPrivateProfileBool(LPCTSTR lpAppName, LPCTSTR lpKeyName, bool bDefault, LPCTSTR lpFileName)
{
	static TCHAR text[256];
	GetPrivateProfileString(lpAppName, lpKeyName, bDefault ? _T("true") : _T("false"), text, COUNT(text), lpFileName);
	if(lstrcmpi(text, _T("true")) == 0)
		return true;
	else if(lstrcmpi(text, _T("false")) == 0)
		return false;
	else
		return bDefault;
}

BOOL WritePrivateProfileInt(LPCTSTR lpAppName, LPCTSTR lpKeyName, INT nValue, LPCTSTR lpFileName)
{
	static TCHAR intText[256];
	wsprintf(intText, _T("%d"), nValue);
	return WritePrivateProfileString(lpAppName, lpKeyName, intText, lpFileName);
}

BOOL WritePrivateProfileBool(LPCTSTR lpAppName, LPCTSTR lpKeyName, bool bBoolean, LPCTSTR lpFileName)
{
	return WritePrivateProfileString(lpAppName, lpKeyName, bBoolean ? _T("true") : _T("false"), lpFileName);
}

/*****************************************************************************/

void GetPlatformSnapPath (char *path, const char *base)
{
	TCHAR tempDir[MAX_PATH + 1];

	GetTempPath(MAX_PATH, tempDir);
	wsprintf(path, _T("%s\\%s.s9xw"), GUI.PlatformSnapIntoTempDir ? 
		tempDir : S9xGetDirectory(SNAPSHOT_DIR), S9xBasename(base));
}

EXTERN_C bool8 S9xFreezePlatformDepends (const char *basefilename)
{
	static TCHAR filepath [_MAX_PATH + 1];
	bool result = true;

	// GetPlatformSnapPath should return full-path always
	GetPlatformSnapPath(filepath, basefilename);

	// TODO: more abstract implementation
	result &= MacroSaveState(filepath);
	// TODO/FIXME?: they must be removed when they're stored in platform-independent snapshot
	//WritePrivateProfileBool(_T("Control"), _T("pad_read"), pad_read!=0, filepath);
	//WritePrivateProfileBool(_T("Control"), _T("pad_read_last"), pad_read_last!=0, filepath);
	//WritePrivateProfileInt(_T("Timings"), _T("TotalEmulatedFrames"), IPPU.TotalEmulatedFrames, filepath);
	//WritePrivateProfileInt(_T("Timings"), _T("LagCounter"), IPPU.LagCounter, filepath);
	return result;
}

EXTERN_C bool8 S9xUnfreezePlatformDepends (const char *basefilename)
{
	static TCHAR filepath [_MAX_PATH + 1];
	bool result = true;

	GetPlatformSnapPath(filepath, basefilename);

	// TODO: more abstract implementation
	result &= MacroLoadState(filepath);

	// TODO/FIXME?: they must be removed when they're stored in platform-independent snapshot
	/*
	bool8 pad_read_temp;
	bool adjustLagCounter;
	pad_read = GetPrivateProfileBool(_T("Control"), _T("pad_read"), pad_read!=0, filepath);
	pad_read_last = GetPrivateProfileBool(_T("Control"), _T("pad_read_last"), pad_read_last!=0, filepath);
	IPPU.TotalEmulatedFrames = GetPrivateProfileInt(_T("Timings"), _T("TotalEmulatedFrames"), IPPU.TotalEmulatedFrames, filepath);
	IPPU.LagCounter = GetPrivateProfileInt(_T("Timings"), _T("LagCounter"), IPPU.LagCounter, filepath);

	// messy
	adjustLagCounter = (IPPU.LagCounter && !pad_read_last);

	pad_read_temp = pad_read;
	pad_read = pad_read_last;
	IPPU.LagCounter -= (adjustLagCounter ? 1 : 0);
	S9xUpdateFrameCounter (-1);
	IPPU.LagCounter += (adjustLagCounter ? 1 : 0);
	pad_read = pad_read_temp;
	*/

	// disabled because the refresh needs to happen later than when this function gets called
//	S9xReRefresh();

	return result;
}
