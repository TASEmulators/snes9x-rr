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



// DirectX.cpp: implementation of the CDirectX class.
//
//////////////////////////////////////////////////////////////////////

#include "wsnes9x.h"
#include "../snes9x.h"
#include "../soundux.h"
#include "directx.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CDirectX::CDirectX()
{
    lpDD = NULL;
    lpDDClipper = NULL;
    lpDDPalette = NULL;

    lpDDSPrimary2 = NULL;
    lpDDSOffScreen2 = NULL;

    Width = Height = -1;
    Depth = -1;
    DoubleBuffered = false;

    lpDS = NULL;
    lpDSB = NULL;
    lpDSBPrimary = NULL;
	
    DSAvailable = false;
}

CDirectX::~CDirectX()
{
    DeInitializeDirectDraw();
	DeInitializeDirectSound();
}

bool CDirectX::InitDirectX ()
{
	DDinitialized = false;
	if(DDinitialized)
		return true;

    dErr = DirectDrawCreate (NULL, &lpDD, NULL);
    if(FAILED(dErr))
        return false;

    dErr = lpDD -> CreateClipper (0, &lpDDClipper, NULL);
    if(FAILED(dErr))
        return false;

    dErr = lpDDClipper->SetHWnd (0, GUI.hWnd);
    if(FAILED(dErr))
        return false;

	DDinitialized = true;

    return (true);
}

bool CDirectX::InitDirectSound ()
{
    DSAvailable = true;

        if (!lpDS)
        {
            dErr = DirectSoundCreate (NULL, &lpDS, NULL);
            if (dErr != DS_OK)
            {
                DSAvailable = false;
                MessageBox (GUI.hWnd, TEXT("\
Unable to initialise DirectSound. You will not be able to hear any\n\
sound effects or music while playing.\n\n\
It is usually caused by not having DirectX installed, another\n\
application that has already opened DirectSound in exclusive\n\
mode or the Windows WAVE device has been opened."),
                            TEXT("Snes9X - Unable to Open DirectSound"), 
                            MB_OK | MB_ICONWARNING);
                return (false);
            }
        }
		dErr = lpDS->SetCooperativeLevel (GUI.hWnd, DSSCL_PRIORITY | DSSCL_EXCLUSIVE);
        if (!SUCCEEDED(dErr))
        {
			dErr = lpDS->SetCooperativeLevel (GUI.hWnd, DSSCL_PRIORITY);
			if (!SUCCEEDED(dErr))
			{
				if (!SUCCEEDED(lpDS -> SetCooperativeLevel (GUI.hWnd, DSSCL_NORMAL)))
				{
					lpDS -> Release();
					lpDS = NULL;

					DSAvailable = false;
				}
				if (DSAvailable)
					MessageBox (GUI.hWnd, TEXT("\
	Unable to set DirectSound's  priority cooperative level.\n\
	Another application is dicating the sound playback rate,\n\
	sample size and mono/stereo setting."),
						TEXT("Snes9X - Unable to Set DirectSound priority"), 
								MB_OK | MB_ICONWARNING);
				else
					MessageBox (GUI.hWnd, TEXT("\
	Unable to set any DirectSound cooperative level. You will\n\
	not be able to hear any sound effects or music while playing.\n\n\
	It is usually caused by another application that has already\n\
	opened DirectSound in exclusive mode."),
						TEXT("Snes9X - Unable to DirectSound"), 
								MB_OK | MB_ICONWARNING);
			}
		}

    return (true);
}

void CDirectX::DeInitializeDirectDraw()
{
    if (lpDD != NULL)
    {
        if (lpDDSPrimary2 != NULL)
        {
            lpDDSPrimary2->Release();
            lpDDSPrimary2 = NULL;
        }
        if (lpDDSOffScreen2 != NULL)
        {
            lpDDSOffScreen2->PageUnlock(0);
            lpDDSOffScreen2->Release();
            lpDDSOffScreen2 = NULL;
        }
        if (lpDDClipper != NULL)
        {
            lpDDClipper->Release();
            lpDDClipper = NULL;
        }
        if (lpDDPalette != NULL)
        {
            lpDDPalette->Release();
            lpDDPalette = NULL;
        }
        lpDD->Release();
        lpDD = NULL;
    }
	DDinitialized = false;
}

void CDirectX::DeInitializeDirectSound()
{
	if( lpDS != NULL)
    {
        if( lpDSB != NULL)
        {
            lpDSB->Stop ();
            lpDSB->Release();
            lpDSB = NULL;
        }
        if( lpDSBPrimary != NULL)
        {
            lpDSBPrimary->Stop ();
            lpDSBPrimary->Release();
            lpDSBPrimary = NULL;
        }
        lpDS->SetCooperativeLevel (GUI.hWnd, DSSCL_NORMAL);
        lpDS->Release ();
        lpDS = NULL;
    }
	DSAvailable = false;
}

bool CDirectX::SetDisplayMode( 
		int pWidth, int pHeight, int pScale,
		char pDepth, int pRefreshRate, bool pWindowed, bool pDoubleBuffered)
{
	if(pScale < 2) pScale = 2;

    static bool BLOCK = false;
    DDSURFACEDESC ddsd;
    PALETTEENTRY PaletteEntries [256];

    if (BLOCK)
        return (false);

    BLOCK = true;

    if (pWindowed)
        pDoubleBuffered = false;

    if (pDepth == 0)
        pDepth = Depth;

    if (lpDDSPrimary2 != NULL)
    {
        lpDDSPrimary2->Release();
        lpDDSPrimary2 = NULL;
    }
    if (lpDDSOffScreen2 != NULL)
    {
        lpDDSOffScreen2->PageUnlock(0);
        lpDDSOffScreen2->Release();
        lpDDSOffScreen2 = NULL;
    }
    if( lpDDPalette != NULL)
    {
        lpDDPalette->Release();
        lpDDPalette = NULL;
    }

    lpDD->FlipToGDISurface();

    if (pWindowed)
    {	
        lpDD->RestoreDisplayMode();

        SetWindowLong( GUI.hWnd, GWL_STYLE, WS_POPUPWINDOW|WS_CAPTION|
                       WS_THICKFRAME|WS_VISIBLE|WS_MINIMIZEBOX|WS_MAXIMIZEBOX);

		// disabled because it messes up window maximization
        //if (!VOODOO_MODE)
        //    SetWindowPos( GUI.hWnd, HWND_TOP, 0, 0, 0, 0,
        //                  SWP_DRAWFRAME|SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE);
        //else
        //    SetWindowPos( GUI.hWnd, HWND_TOP, 0, 0, 0, 0,
        //                  SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE);

        ZeroMemory (&ddsd, sizeof (ddsd));

        ddsd.dwSize = sizeof (ddsd);
        ddsd.dwFlags = DDSD_PIXELFORMAT;
        dErr = lpDD->GetDisplayMode (&ddsd);
		if (FAILED(dErr))
			pDepth = 8;
        else
        {
            if (ddsd.ddpfPixelFormat.dwFlags&DDPF_RGB)
                pDepth = (char) ddsd.ddpfPixelFormat.dwRGBBitCount;
            else
                pDepth = 8;
        }
        if (pDepth == 8)
            dErr = lpDD->SetCooperativeLevel (GUI.hWnd, DDSCL_FULLSCREEN|
                                              DDSCL_EXCLUSIVE|DDSCL_ALLOWREBOOT);
        else
            dErr = lpDD->SetCooperativeLevel (GUI.hWnd, DDSCL_NORMAL|DDSCL_ALLOWREBOOT);
    } 
    else 
    {
        SetWindowLong (GUI.hWnd, GWL_STYLE, WS_POPUP|WS_VISIBLE);

        if (!VOODOO_MODE)
        {
            SetWindowPos (GUI.hWnd, HWND_TOP, 0, 0, 0, 0, SWP_DRAWFRAME|SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE);
            dErr = lpDD->SetCooperativeLevel (GUI.hWnd, DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN|DDSCL_ALLOWREBOOT);
			// XXX: TODO: use pRefreshRate!
            dErr = lpDD->SetDisplayMode (pWidth, pHeight, pDepth);
        }
        else
        {
            SetWindowPos (GUI.hWnd, HWND_TOP, 0, 0, 0, 0, SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE);
            dErr = lpDD->SetCooperativeLevel (GUI.hWnd, DDSCL_NORMAL|DDSCL_ALLOWREBOOT);
        }
    }

	if (FAILED(dErr))
    {
        BLOCK = false;
        return false;
    }

    ZeroMemory (&ddsd, sizeof (ddsd));
    ddsd.dwSize = sizeof (ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	if(GUI.ddrawUseVideoMemory)
	{
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY | (GUI.ddrawUseLocalVidMem ? DDSCAPS_LOCALVIDMEM : DDSCAPS_NONLOCALVIDMEM);
	}
	else
	{
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	}
    ddsd.dwWidth = SNES_WIDTH * pScale;
    ddsd.dwHeight = SNES_HEIGHT_EXTENDED * pScale;
	
    LPDIRECTDRAWSURFACE lpDDSOffScreen;
    if (FAILED(lpDD->CreateSurface (&ddsd, &lpDDSOffScreen, NULL)))
    {
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY | (GUI.ddrawUseLocalVidMem ? DDSCAPS_NONLOCALVIDMEM : DDSCAPS_LOCALVIDMEM);
		if(!GUI.ddrawUseVideoMemory || FAILED(lpDD->CreateSurface (&ddsd, &lpDDSOffScreen, NULL)))
		{
			ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
			if(!GUI.ddrawUseVideoMemory || FAILED(lpDD->CreateSurface (&ddsd, &lpDDSOffScreen, NULL)))
			{
				BLOCK = false;
				return (false);
			}
		}
    }
    
    if (FAILED (lpDDSOffScreen->QueryInterface (IID_IDirectDrawSurface2, 
                                                (void **)&lpDDSOffScreen2)))
    {
        lpDDSOffScreen->Release();
        BLOCK = false;
        return (false);
    }
    lpDDSOffScreen2->PageLock(0);
    lpDDSOffScreen->Release();

    ZeroMemory (&ddsd, sizeof (ddsd));
    if (pDoubleBuffered)
    {
        ddsd.dwSize = sizeof( ddsd);
        ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
        ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_COMPLEX | DDSCAPS_FLIP;
        GUI.NumFlipFrames = 3;
        ddsd.dwBackBufferCount = 2;
    }
    else
    {
        GUI.NumFlipFrames = 1;
        ddsd.dwSize = sizeof (ddsd);
        ddsd.dwFlags = DDSD_CAPS;
        ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    }

    LPDIRECTDRAWSURFACE lpDDSPrimary;

    dErr = lpDD->CreateSurface (&ddsd, &lpDDSPrimary, NULL);
    if( FAILED(dErr) )
    {
        if (pDoubleBuffered)
        {
            ddsd.dwBackBufferCount = 1;
            GUI.NumFlipFrames = 2;
            if (FAILED(dErr = lpDD->CreateSurface (&ddsd, &lpDDSPrimary, NULL)))
            {
                ddsd.dwFlags = DDSD_CAPS;
                ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

                pDoubleBuffered = false;
                GUI.NumFlipFrames = 1;
                dErr = lpDD->CreateSurface (&ddsd, &lpDDSPrimary, NULL);
            }
    	}

        if (FAILED(dErr))
        {
            BLOCK = false; 
            lpDDSOffScreen2->PageUnlock(0);
            lpDDSOffScreen2->Release();
            lpDDSOffScreen2 = NULL;
            
            return (false);
        }
    }

    ZeroMemory (&DDPixelFormat, sizeof (DDPixelFormat));
    DDPixelFormat.dwSize = sizeof (DDPixelFormat);
    lpDDSPrimary->GetPixelFormat (&DDPixelFormat);

    Clipped = true;
    if((!pWindowed && pDoubleBuffered) || FAILED(lpDDSPrimary->SetClipper( lpDDClipper)))
        Clipped = false;

    if (FAILED (lpDDSPrimary->QueryInterface (IID_IDirectDrawSurface2, (void **)&lpDDSPrimary2)))
    {
        BLOCK = false;
        lpDDSPrimary->Release();
        lpDDSPrimary = NULL;

        return (FALSE);
    }

    lpDDSPrimary->Release();
    lpDDSPrimary = NULL;

    if((!pWindowed && pDoubleBuffered) || FAILED(lpDDSPrimary2->SetClipper( lpDDClipper)))
		Clipped = false;

    if (pDepth == 8)
    {
        dErr = lpDD->CreatePalette (DDPCAPS_8BIT | DDPCAPS_ALLOW256, 
                                    PaletteEntries, &lpDDPalette, NULL);
        if( FAILED(dErr))
        {
            lpDDPalette = NULL;
            BLOCK = false;
            return false;
        }
    }

    Depth = pDepth;
    Height = pHeight;
    Width = pWidth;
    DoubleBuffered = pDoubleBuffered;
    BLOCK = false;

    return (true);
}

void CDirectX::GetPixelFormat ()
{
    if (lpDDSPrimary2)
    {
        ZeroMemory (&DDPixelFormat, sizeof (DDPixelFormat));
        DDPixelFormat.dwSize = sizeof (DDPixelFormat);
        lpDDSPrimary2->GetPixelFormat (&DDPixelFormat);
    }
}

extern long _buffernos;
extern long _blocksize;
extern long _buffersize;
extern long _samplecount;
extern long _bytecount;
extern unsigned long _interval;

bool CDirectX::SetSoundMode( void)
{
    if (lpDS != NULL)
    {
        if (lpDSB != NULL)
        {
            lpDSB->Stop ();
            lpDSB->Release();
            lpDSB = NULL;
        }
        if (lpDSBPrimary != NULL)
        {
            lpDSBPrimary->Stop ();
            lpDSBPrimary->Release();
            lpDSBPrimary = NULL;
        }
        lpDS->SetCooperativeLevel (GUI.hWnd, DSSCL_NORMAL);
        lpDS->Release ();
        lpDS = NULL;
    }


    InitDirectSound ();

        DSBUFFERDESC dsbd;

        if (!Settings.APUEnabled || Settings.Mute ||!DSAvailable)
            return false;
        
		uint32 CorrectSoundPlaybackRate(uint32);

        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = Settings.Stereo ? 2 : 1;
        wfx.nSamplesPerSec = CorrectSoundPlaybackRate(Settings.SoundPlaybackRate);
        wfx.nBlockAlign = (Settings.SixteenBitSound ? 2 : 1) * (Settings.Stereo ? 2 : 1);
        wfx.wBitsPerSample = Settings.SixteenBitSound ? 16 : 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        wfx.cbSize = 0;
        
        ZeroMemory (&dsbd, sizeof(DSBUFFERDESC) );
        dsbd.dwSize = sizeof(dsbd);
        dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_STICKYFOCUS;
        
        dErr = lpDS->CreateSoundBuffer (&dsbd, &lpDSBPrimary, NULL);
        if (dErr != DS_OK)
        {
            lpDSB = NULL;
            return (false);
        }

		HRESULT SetFormatResult;
		SetFormatResult = lpDSBPrimary->SetFormat (&wfx);
        if (lpDSBPrimary->GetFormat (&wfx, sizeof (wfx), NULL) == DS_OK)
        {
			if(FAILED(SetFormatResult))
				fprintf(stderr, "SetFormat Error %08X: %dHz %dbit %s", SetFormatResult, so.playback_rate, so.sixteen_bit ? 16 : 8, so.stereo ? "Stereo" : "Mono");
            so.playback_rate = wfx.nSamplesPerSec;
            so.stereo = wfx.nChannels > 1;
            so.sixteen_bit = wfx.wBitsPerSample == 16;
			if(FAILED(SetFormatResult))
				fprintf(stderr, " -> %dHz %dbit %s\n", so.playback_rate, so.sixteen_bit ? 16 : 8, so.stereo ? "Stereo" : "Mono");
        }
        
//        DSBCAPS caps;
//        caps.dwSize = sizeof (DSBCAPS);
//        lpDSBPrimary->GetCaps (&caps);
  
        lpDSBPrimary->Play (0, 0, DSBPLAY_LOOPING);

#if 0
        DWORD buffer_bytes = (200 * so.playback_rate * 
                              (Settings.Stereo ? 2 : 1) *
                              (Settings.SixteenBitSound ? 2 : 1)) / 1000;
#else
        DWORD buffer_bytes = _blocksize * _buffernos;
#endif
        ZeroMemory (&dsbd, sizeof (dsbd));
        dsbd.dwSize = sizeof( dsbd);
        dsbd.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLVOLUME | 
	               DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_STICKYFOCUS;
        dsbd.dwBufferBytes = buffer_bytes;
        dsbd.lpwfxFormat = &wfx;
        
        if (lpDS->CreateSoundBuffer (&dsbd, &lpDSB, NULL) != DS_OK)
        {
            lpDSBPrimary->Release ();
            lpDSBPrimary = NULL;
            lpDSB->Release();
            lpDSB = NULL;
            return (false);
        }

#if 0
        // Compute how far ahead, in milliseconds, the sound buffer write
        // position is compared to the play position.
        DWORD play_pos = 0, write_pos = 0;
        DWORD len, milli;

        lpDSB->Play (0, 0, DSBPLAY_LOOPING);
        if (lpDSB->GetCurrentPosition (&play_pos, &write_pos) != DS_OK)
            milli = 150;
        else
        {
            if (write_pos < play_pos)
                len = write_pos + buffer_bytes - play_pos;
            else
                len = write_pos - play_pos;
        
            milli = (1000 * len) / so.playback_rate;
            if (Settings.Stereo)
                milli /= 2;
            if (Settings.SixteenBitSound)
                milli /= 2;
        }
        lpDSB->Stop (); 
        lpDSB->Release ();
        lpDSB = NULL;

        milli = (milli * 3) / 2;
        if (milli < _interval)
            milli = _interval;

        int s = (milli * so.playback_rate *
                 (Settings.Stereo ? 2 : 1) *
                 (Settings.SixteenBitSound ? 2 : 1)) / 1000;

        _blocksize = 64;
        while (_blocksize < s)
            _blocksize *= 2;

        ZeroMemory (&dsbd, sizeof (dsbd));
        dsbd.dwSize = sizeof (dsbd);
        dsbd.dwFlags = DSBCAPS_CTRLDEFAULT | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_STICKYFOCUS;
        dsbd.dwBufferBytes = _blocksize * _buffernos;
        dsbd.lpwfxFormat = &wfx;
        
        if (lpDS->CreateSoundBuffer (&dsbd, &lpDSB, NULL) != DS_OK)
        {
            lpDSBPrimary->Release ();
            lpDSBPrimary = NULL;
            lpDSB->Release();
            lpDSB = NULL;
            return (false);
        }
        _buffersize = _blocksize * _buffernos;
#endif

    return (true);
}
