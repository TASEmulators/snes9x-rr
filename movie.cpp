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

  Input recording/playback code
  (c) Copyright 2004 blip
 
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



//  Input recording/playback code
//  (c) Copyright 2004 blip

#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>

#if defined(__unix) || defined(__linux) || defined(__sun) || defined(__DJGPP) || defined(__MACOSX__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <time.h>

#ifdef WIN32
#include <io.h>
#ifndef W_OK
#define W_OK 2
#endif
#define ftruncate chsize
#endif

#include "movie.h"
#include "snes9x.h"
#include "cpuexec.h"
#include "snapshot.h"
#include "language.h"
#ifdef NETPLAY_SUPPORT
#include "netplay.h"
#endif

#include "s9xlua.h"

#define SMV_MAGIC	0x1a564d53		// SMV0x1a
#define SMV_VERSION	1
#define SMV_HEADER_SIZE	32
#define SMV_EXTRAROMINFO_SIZE	(2+sizeof(uint32)+sizeof(char)*23+1)
#define CONTROLLER_DATA_SIZE	2
#define BUFFER_GROWTH_SIZE	4096

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

enum MovieState
{
	MOVIE_STATE_NONE=0,
	MOVIE_STATE_PLAY,
	MOVIE_STATE_RECORD
};

static struct SMovie
{
	enum MovieState State;
	char   Filename [_MAX_PATH];
	FILE*  File;
	uint32 SaveStateOffset;
	uint32 ControllerDataOffset;
	uint32 MovieId;
	uint32 CurrentFrame;
	uint32 MaxFrame;
	uint32 RerecordCount;
	uint8  ControllersMask;
	uint8  Opts;
	uint8  SyncFlags;
	uint8  SyncFlags2;
	bool8  ReadOnly;
	uint32 BytesPerFrame;
	uint8* InputBuffer;
	uint32 InputBufferSize;
	uint8* InputBufferPtr;

	uint32 ROMCRC32;
	char   ROMName [23];

	bool8  RecordedThisSession;
	uint32 Version;

	bool8  RequiresReset;
} Movie;

/*
//	For illustration:
	struct MovieFileHeader
	{
		uint32	magic;		// SMV0x1a
		uint32	version; // for Snes9x 1.51 this must be 4. In Snes9x 1.43 it is 1.
		uint32	uid; // used to match savestates to a particular movie... this is also the date/time of first recording
		uint32	rerecord_count;
		uint32	length_frames;
		uint8	flags[4]; // (ControllersMask, Opts, Reserved, SyncFlags)
		uint32	offset_to_savestate; // pointer to embedded savestate or SRAM
		uint32	offset_to_controller_data; // pointer to controller data
	};
	// sizeof(MovieFileHeader) == 32

// after the header comes extra metadata, i.e. author info
	// sizeof(metadata) = (offset_to_savestate - sizeof(MovieFileHeader)) - sizeof(ExtraRomInfo);
	// that should be an even number of bytes because the author info consists of 2-byte characters

// after the metadata comes extra info about the ROM used for recording
	struct ExtraRomInfo
	{
		uint8	reserved[3];
		uint32	romCRC32;
		uint8	romName[23];
	};
	// sizeof(ExtraRomInfo) == 30

// after that comes the savestate or SRAM data (depending on the Opts flag)
	// sizeof(SaveData) <= offset_to_controller_data - offset_to_savestate

// after that comes the controller data
	// sizeof(ControllerData) == length_frames * sizeof(InputSample)
	// sizeof(InputSample) == 2*(sum of bits in ControllersMask)

*/

bool8 prevForcePal, prevPal, prevForceNTSC, delayedPrevRestore=false;
bool8 prevWIPAPUTiming, prevUpAndDown, prevSoundEnvelopeHeightReading, prevFakeMuteFix, prevSoundSync, prevCPUShutdown, prevClearFastROM;

static int bytes_per_frame()
{
	int i;
	int num_controllers;

	num_controllers=0;
	for(i=0; i<5; ++i)
	{
		if(Movie.ControllersMask & (1<<i))
		{
			++num_controllers;
		}
	}

	int bytes = CONTROLLER_DATA_SIZE*num_controllers;
	return bytes;
}

uint32 Read32(const uint8*& ptr)
{
	uint32 v=(ptr[0] | (ptr[1]<<8) | (ptr[2]<<16) | (ptr[3]<<24));
	ptr += 4;
	return v;
}

uint16 Read16(const uint8*& ptr) /* const version */
{
	uint16 v=(ptr[0] | (ptr[1]<<8));
	ptr += 2;
	return v;
}

uint16 Read16(uint8*& ptr) /* non-const version */
{
	uint16 v=(ptr[0] | (ptr[1]<<8));
	ptr += 2;
	return v;
}

static inline uint8 Read8(const uint8*& ptr) /* const version */
{
	uint8 v=(ptr[0]);
	ptr++;
	return v;
}

static inline uint8 Read8(uint8*& ptr) /* non-const version */
{
	uint8 v=(ptr[0]);
	ptr++;
	return v;
}

void Write32(uint32 v, uint8*& ptr)
{
	ptr[0]=(uint8)(v&0xff);
	ptr[1]=(uint8)((v>>8)&0xff);
	ptr[2]=(uint8)((v>>16)&0xff);
	ptr[3]=(uint8)((v>>24)&0xff);
	ptr += 4;
}

void Write16(uint16 v, uint8*& ptr)
{
	ptr[0]=(uint8)(v&0xff);
	ptr[1]=(uint8)((v>>8)&0xff);
	ptr += 2;
}

static inline void Write8(uint8 v, uint8*& ptr)
{
	ptr[0]=(uint8)(v);
	ptr++;
}

static int read_movie_header(FILE* fd, SMovie* movie)
{
	uint8 header[SMV_HEADER_SIZE];
	if(fread(header, 1, SMV_HEADER_SIZE, fd) != SMV_HEADER_SIZE)
		return WRONG_FORMAT;

	const uint8* ptr=header;
	uint32 magic=Read32(ptr);
	if(magic!=SMV_MAGIC)
		return WRONG_FORMAT;

	uint32 version=Read32(ptr);
	if(version>4)
		return WRONG_VERSION;

	movie->MovieId=Read32(ptr);
	movie->RerecordCount=Read32(ptr);
	movie->MaxFrame=Read32(ptr);
	movie->Version=version;

	{
		movie->ControllersMask=Read8(ptr);
		movie->Opts=Read8(ptr);
		movie->SyncFlags2=Read8(ptr); // previously reserved byte
		movie->SyncFlags=Read8(ptr); // previously reserved byte

		movie->SaveStateOffset=Read32(ptr);
		movie->ControllerDataOffset=Read32(ptr);
	}

	if(version!=SMV_VERSION)
		return WRONG_VERSION;

//	ptr += 0; // reserved bytes

	assert(ptr-header==SMV_HEADER_SIZE);

	return SUCCESS;
}

static int read_movie_extrarominfo(FILE* fd, SMovie* movie)
{
	if((movie->SyncFlags & MOVIE_SYNC_HASROMINFO) != 0)
	{
		fseek(fd, movie->SaveStateOffset - SMV_EXTRAROMINFO_SIZE, SEEK_SET);

		uint8 extraRomInfo[SMV_EXTRAROMINFO_SIZE];
		if(fread(extraRomInfo, 1, SMV_EXTRAROMINFO_SIZE, fd) != SMV_EXTRAROMINFO_SIZE)
			return WRONG_FORMAT;

		const uint8* ptr=extraRomInfo;

		ptr ++; // zero byte
		ptr ++; // zero byte
		ptr ++; // zero byte
		movie->ROMCRC32=Read32(ptr);
		strncpy(movie->ROMName,(const char*)ptr,23); ptr += 23;
	}
	else
	{
		movie->ROMCRC32=Memory.ROMCRC32;
		strncpy(movie->ROMName,(const char*)Memory.RawROMName,23);
	}

	return SUCCESS;
}

static void write_movie_header(FILE* fd, const SMovie* movie)
{
	uint8 header[SMV_HEADER_SIZE];
	uint8* ptr=header;

	Write32(SMV_MAGIC, ptr);
	Write32(SMV_VERSION, ptr);
	Write32(movie->MovieId, ptr);
	Write32(movie->RerecordCount, ptr);
	Write32(movie->MaxFrame, ptr);

	Write8(movie->ControllersMask, ptr);
	Write8(movie->Opts, ptr);
	Write8(movie->SyncFlags2, ptr); // previously reserved byte
	Write8(movie->SyncFlags, ptr); // previously reserved byte

	Write32(movie->SaveStateOffset, ptr);
	Write32(movie->ControllerDataOffset, ptr);

	assert(ptr-header==SMV_HEADER_SIZE);

	fwrite(header, 1, SMV_HEADER_SIZE, fd);

	assert(!ferror(fd));
}

static void write_movie_extrarominfo(FILE* fd, const SMovie* movie)
{
	if((movie->SyncFlags & MOVIE_SYNC_HASROMINFO) != 0) // should be true...
	{
		uint8 extraRomInfo [SMV_EXTRAROMINFO_SIZE];
		uint8* ptr = extraRomInfo;

		*ptr++=0; // zero byte
		*ptr++=0; // zero byte
		*ptr++=0; // zero byte
		Write32(movie->ROMCRC32, ptr);
		strncpy((char*)ptr,movie->ROMName,23); ptr += 23;

		fwrite(extraRomInfo, 1, SMV_EXTRAROMINFO_SIZE, fd);
		assert(!ferror(fd));
	}
}

static void flush_movie()
{
	if(!Movie.File)
		return;

	if((Movie.SyncFlags & MOVIE_SYNC_HASROMINFO) == 0) // if we have to insert the ROM info into a movie made by a previous version
	{
		fseek(Movie.File, 0, SEEK_END);
		long oldFileSize = (uint32)ftell(Movie.File);

		// copy whole movie into temporary memory
		char * tempFile = (char*)malloc(sizeof(char)*oldFileSize);
		fseek(Movie.File, 0, SEEK_SET);
		fread(tempFile, 1, oldFileSize, Movie.File);

		// write back part of the movie offset by SMV_EXTRAROMINFO_SIZE
		fseek(Movie.File, Movie.SaveStateOffset + SMV_EXTRAROMINFO_SIZE, SEEK_SET);
		fwrite(tempFile + Movie.SaveStateOffset, 1, oldFileSize - Movie.SaveStateOffset, Movie.File);

		// free the temporary movie in memory
		free(tempFile);

		// update flags and offset amounts
		Movie.SyncFlags |= MOVIE_SYNC_HASROMINFO;
		Movie.SaveStateOffset += SMV_EXTRAROMINFO_SIZE;
		Movie.ControllerDataOffset += SMV_EXTRAROMINFO_SIZE;

		// write the extra rom info into the newly inserted space in the file between the metadata and the save data
		fseek(Movie.File, Movie.SaveStateOffset - SMV_EXTRAROMINFO_SIZE, SEEK_SET);
		write_movie_extrarominfo(Movie.File, &Movie);
	}

	fseek(Movie.File, 0, SEEK_SET);
	write_movie_header(Movie.File, &Movie);
	fseek(Movie.File, Movie.ControllerDataOffset, SEEK_SET);
	fwrite(Movie.InputBuffer, 1, Movie.BytesPerFrame*(Movie.MaxFrame+1), Movie.File);
	assert(!ferror(Movie.File));
}

static void store_previous_settings()
{
	if(!delayedPrevRestore)
	{
		prevPal = Settings.PAL;
		prevCPUShutdown = Settings.ShutdownMaster;
	}
	delayedPrevRestore = false;
	prevForcePal = Settings.ForcePAL;
	prevForceNTSC = Settings.ForceNTSC;
	prevWIPAPUTiming = Settings.UseWIPAPUTiming;
	prevUpAndDown = Settings.UpAndDown;
	prevSoundEnvelopeHeightReading = Settings.SoundEnvelopeHeightReading;
	prevFakeMuteFix = Settings.FakeMuteFix;
	prevSoundSync = Settings.SoundSync;
	prevClearFastROM = Settings.InitFastROMSetting;
}

static void restore_previous_settings()
{
	Settings.ForcePAL = prevForcePal;
	Settings.ForceNTSC = prevForceNTSC;
	Settings.UseWIPAPUTiming = prevWIPAPUTiming;
	Settings.SoundEnvelopeHeightReading = prevSoundEnvelopeHeightReading;
	Settings.FakeMuteFix = prevFakeMuteFix;
	Settings.InitFastROMSetting = prevClearFastROM;
//	Settings.PAL = prevPal; // changing this after the movie while it's still emulating would be bad
//	Settings.ShutdownMaster = prevCPUShutdown; // changing this after the movie while it's still emulating would be bad
	delayedPrevRestore = true; // wait to change the above 2 settings until later
//	Settings.UpAndDown = prevUpAndDown; // doesn't actually affect synchronization, so leave the setting alone; the port can change it if it wants
//	Settings.SoundSync = prevSoundSync; // doesn't seem to affect synchronization, so leave the setting alone; the port can change it if it wants
}

static void store_movie_settings()
{
	bool hadRomInfo = (Movie.SyncFlags & MOVIE_SYNC_HASROMINFO) != 0;
	if(Settings.PAL)
		Movie.Opts |= MOVIE_OPT_PAL;
	else
		Movie.Opts &= ~MOVIE_OPT_PAL;
	Movie.SyncFlags = MOVIE_SYNC_DATA_EXISTS | (hadRomInfo ? MOVIE_SYNC_HASROMINFO : 0);
	if(Settings.UseWIPAPUTiming) Movie.SyncFlags |= MOVIE_SYNC_WIP1TIMING;
	if(Settings.UpAndDown) Movie.SyncFlags |= MOVIE_SYNC_LEFTRIGHT;
	if(Settings.SoundEnvelopeHeightReading) Movie.SyncFlags |= MOVIE_SYNC_VOLUMEENVX;
	if(Settings.FakeMuteFix || !Settings.APUEnabled) Movie.SyncFlags |= MOVIE_SYNC_FAKEMUTE;
	if(Settings.SoundSync) Movie.SyncFlags |= MOVIE_SYNC_SYNCSOUND;
	if(!Settings.ShutdownMaster) Movie.SyncFlags |= MOVIE_SYNC_NOCPUSHUTDOWN;
	if(Settings.InitFastROMSetting) Movie.SyncFlags2 |= MOVIE_SYNC2_INIT_FASTROM;
}

static void restore_movie_settings()
{
	if(Movie.Opts & MOVIE_OPT_PAL)
	{
		Settings.ForcePAL = Settings.PAL = TRUE; // OK to change while starting playing a movie because either we are re-loading the ROM or we are entering a state that already had this setting set
		Settings.ForceNTSC = FALSE;
	}
	else
	{
		Settings.ForcePAL = Settings.PAL = FALSE; // OK to change while starting playing a movie because either we are re-loading the ROM or we are entering a state that already had this setting set
		Settings.ForceNTSC = TRUE;
	}

	if(Movie.SyncFlags & MOVIE_SYNC_DATA_EXISTS)
	{
		Settings.UseWIPAPUTiming = (Movie.SyncFlags & MOVIE_SYNC_WIP1TIMING) ? TRUE : FALSE;
		Settings.SoundEnvelopeHeightReading = (Movie.SyncFlags & MOVIE_SYNC_VOLUMEENVX) ? TRUE : FALSE;
		Settings.FakeMuteFix = (Movie.SyncFlags & MOVIE_SYNC_FAKEMUTE) ? TRUE : FALSE;
		Settings.ShutdownMaster = (Movie.SyncFlags & MOVIE_SYNC_NOCPUSHUTDOWN) ? FALSE : TRUE; // OK to change while starting playing a movie because either we are re-loading the ROM or we are entering a state that already had this setting set
		Settings.InitFastROMSetting = (Movie.SyncFlags2 & MOVIE_SYNC2_INIT_FASTROM) ? TRUE : FALSE;
//		Settings.UpAndDown = (Movie.SyncFlags & MOVIE_SYNC_LEFTRIGHT) ? TRUE : FALSE; // doesn't actually affect synchronization, so leave the setting alone; the port can change it if it wants
//		Settings.SoundSync = (Movie.SyncFlags & MOVIE_SYNC_SYNCSOUND) ? TRUE : FALSE; // doesn't seem to affect synchronization, so leave the setting alone; the port can change it if it wants
	}
	else
	{
		Settings.ShutdownMaster = TRUE;
	}

	// memory speed init should *always* be on for slowrom games
	// (well, technically speaking it should be on for all games, but some old movies of fastrom games need it off)
	// and probably nobody has made a movie that they expect other people to play by opening a different game first
	if((Memory.ROMSpeed&0x10)==0)
		Settings.InitFastROMSetting = TRUE;
}

// file must still be open for this to work
static void truncate_movie()
{
	if(!Settings.MovieTruncate || !Movie.File)
		return;

	assert(Movie.SaveStateOffset <= Movie.ControllerDataOffset);
	if(Movie.SaveStateOffset > Movie.ControllerDataOffset)
		return;

	const unsigned long length = Movie.ControllerDataOffset + Movie.BytesPerFrame * (Movie.MaxFrame + 1);
	ftruncate(fileno(Movie.File), length);
}

static void change_state(MovieState new_state)
{
	if(new_state==Movie.State)
		return;

	if(Movie.State==MOVIE_STATE_RECORD)
	{
		flush_movie();
	}

	if(new_state==MOVIE_STATE_NONE)
	{
		// truncate movie to MaxSample length if Settings.MovieTruncate is true
		truncate_movie();

		fclose(Movie.File);
		Movie.File=NULL;

		if(S9xMoviePlaying() || S9xMovieRecording()) // even if recording, it could have been switched to from playback
		{
			restore_previous_settings();
		}

		Movie.RequiresReset = false;
	}

	if(new_state!=MOVIE_STATE_PLAY)
	{
		Settings.HighSpeedSeek = 0;
	}

	Movie.State=new_state;
}

static void reserve_buffer_space(uint32 space_needed)
{
	if(space_needed > Movie.InputBufferSize)
	{
		uint32 ptr_offset = Movie.InputBufferPtr - Movie.InputBuffer;
		uint32 alloc_chunks = space_needed / BUFFER_GROWTH_SIZE;
		Movie.InputBufferSize = BUFFER_GROWTH_SIZE * (alloc_chunks+1);
		Movie.InputBuffer = (uint8*)realloc(Movie.InputBuffer, Movie.InputBufferSize);
		Movie.InputBufferPtr = Movie.InputBuffer + ptr_offset;
	}
}

static bool does_frame_data_mean_reset ()
{
	bool reset = false;
	int i;

	if (Movie.State == MOVIE_STATE_PLAY)
	{
		// one frame of all 1 bits = reset code
		// (the SNES controller doesn't have enough buttons to possibly generate this sequence)
		// (a single bit indicator was not used, to avoid having to special-case peripheral recording here)
		if(Movie.InputBufferPtr[0] == 0xFF)
		{
			reset = true;
			for(i=1; i<(int)Movie.BytesPerFrame; i++)
			{
				if(Movie.InputBufferPtr[i] != 0xFF)
				{
					reset = false;
					break;
				}
			}
		}
	}
	return reset;
}

static void read_frame_controller_data()
{
	int i;

	for(i=0; i<5; ++i)
	{
		if(Movie.ControllersMask & (1<<i))
		{
			IPPU.Joypads[i]=(uint32)(Read16(Movie.InputBufferPtr)) | 0x80000000L;
		}
		else
		{
			IPPU.Joypads[i]=0;		// pretend the controller is disconnected
		}
	}
}

static void write_frame_controller_data()
{
	reserve_buffer_space((uint32)((Movie.InputBufferPtr+Movie.BytesPerFrame)-Movie.InputBuffer));

	int i;
	for(i=0; i<5; ++i)
	{
		if(Movie.ControllersMask & (1<<i))
		{
			Write16((uint16)(IPPU.Joypads[i] & 0xffff), Movie.InputBufferPtr);
		}
		else
		{
///			IPPU.Joypads[i]=0;		// pretend the controller is disconnected // not here!
		}
	}
}

void S9xMovieInit ()
{
	memset(&Movie, 0, sizeof(Movie));
	Movie.State = MOVIE_STATE_NONE;
}

void S9xMovieShutdown ()
{
	// even if movie is not active, we need to do this in case of ports that output these settings to .cfg file on exit
	if(delayedPrevRestore)
	{
		// ok to restore these now (because emulation is shutting down and snes9x is exiting)
		Settings.PAL = prevPal;
		Settings.ShutdownMaster = prevCPUShutdown;
		delayedPrevRestore = false;
	}

	if(S9xMovieActive())
		S9xMovieStop (TRUE);
}

static char *FrameCountToTime(char *str, int frame, int fps)
{
	int h, m, s, f, ms;

	if (!str)
		return NULL;

	f = frame;
	s = f / fps;
	m = s / 60;
	h = m / 60;
	f %= fps;
	s %= 60;
	m %= 60;
	ms = ((f * 10000 / fps) + 5) / 10;
	sprintf(str, "%d:%02d:%02d.%02d", h, m, s, (ms + 5) / 10);
	return str;
}

void S9xUpdateFrameCounter (int offset)
{
	static char tmpBuf[1024];

//	offset++;
	if (!Settings.DisplayFrame)
		*GFX.FrameDisplayString = 0;
	else if (Movie.State == MOVIE_STATE_RECORD) {
		if (Settings.CounterInFrames)
			sprintf(tmpBuf, "%d", max(0,(int)(Movie.CurrentFrame+offset)));
		else
			FrameCountToTime(tmpBuf, max(0,(int)(Movie.CurrentFrame)), Memory.ROMFramesPerSecond);
		sprintf(GFX.FrameDisplayString, "%s%s", Settings.OldFashionedFrameCounter ? "Recording frame: " : "", tmpBuf);
		if (!Settings.OldFashionedFrameCounter)
			strcat(GFX.FrameDisplayString, " [Rec]");
	}
	else if (Movie.State == MOVIE_STATE_PLAY) {
		if (Settings.CounterInFrames)
			sprintf(tmpBuf, "%d%s%d", max(0,(int)(Movie.CurrentFrame+offset)), Settings.OldFashionedFrameCounter ? " / " : "/", Movie.MaxFrame);
		else
			FrameCountToTime(tmpBuf, max(0,(int)(Movie.CurrentFrame)), Memory.ROMFramesPerSecond);
		sprintf(GFX.FrameDisplayString, "%s%s", Settings.OldFashionedFrameCounter ? "Playing frame: " : "", tmpBuf);
		if (!Settings.OldFashionedFrameCounter)
			strcat(GFX.FrameDisplayString, " [Play]");
	}
#ifdef NETPLAY_SUPPORT
	else if(Settings.NetPlay) {
		if (Settings.CounterInFrames)
			sprintf(GFX.FrameDisplayString, "%d", max(0,(int)(NetPlay.FrameCount+offset)));
		else
			FrameCountToTime(GFX.FrameDisplayString, max(0,(int)(NetPlay.FrameCount)), Memory.ROMFramesPerSecond);
		if (Settings.NetPlayServer)
			strcat(GFX.FrameDisplayString, " [Server]");
		else
			strcat(GFX.FrameDisplayString, " [Client]");
	}
#endif
	else {
		if (Settings.CounterInFrames)
			sprintf(GFX.FrameDisplayString, "%d", max(0,(int)(IPPU.TotalEmulatedFrames+offset)));
		else
			FrameCountToTime(GFX.FrameDisplayString, max(0,(int)(IPPU.TotalEmulatedFrames)), Memory.ROMFramesPerSecond);
	}

	if (!Settings.DisplayLagCounter)
		*GFX.LagCounterString = 0;
	else {
		if (Settings.CounterInFrames)
			sprintf(GFX.LagCounterString, "%d", max(0,(int)(IPPU.LagCounter)));
		else
			FrameCountToTime(GFX.LagCounterString, max(0,(int)(IPPU.LagCounter)), Memory.ROMFramesPerSecond);
		if (!IPPU.pad_read)
			strcat(GFX.LagCounterString, " *");
	}

	if (Settings.OldFashionedFrameCounter && *GFX.LagCounterString != 0) {
		if (*GFX.FrameDisplayString != 0)
			strcat(GFX.FrameDisplayString, " | ");
		strcat(GFX.FrameDisplayString, GFX.LagCounterString);
		*GFX.LagCounterString = 0;
	}
}

int S9xMovieOpen (const char* filename, bool8 read_only, uint8 sync_flags, uint8 sync_flags2)
{
	FILE* fd;
	STREAM stream;
	int result;
	int fn;

	char movie_filename [_MAX_PATH];
#ifdef WIN32
	_fullpath(movie_filename, filename, _MAX_PATH);
#else
	strcpy(movie_filename, filename);
#endif

	if(!(fd=fopen(movie_filename, "rb+")))
		if(!(fd=fopen(movie_filename, "rb")))
			return FILE_NOT_FOUND;
		else
			read_only = TRUE;

	const bool8 wasPaused = Settings.Paused;
	const uint32 prevFrameTime = Settings.FrameTime;

	// stop current movie before opening
	change_state(MOVIE_STATE_NONE);

	// read header
	if((result=read_movie_header(fd, &Movie))!=SUCCESS)
	{
		fclose(fd);
		return result;
	}

	read_movie_extrarominfo(fd, &Movie);

	fn=dup(fileno(fd));
	fclose(fd);

	// apparently this lseek is necessary
	lseek(fn, Movie.SaveStateOffset, SEEK_SET);
	if(!(stream=REOPEN_STREAM(fn, "rb")))
		return FILE_NOT_FOUND;

	// store previous, before changing to the movie's settings
	store_previous_settings();

	// store default
	if (sync_flags & MOVIE_SYNC_DATA_EXISTS)
	{
		Settings.UseWIPAPUTiming = (sync_flags & MOVIE_SYNC_WIP1TIMING) ? TRUE : FALSE;
		Settings.SoundEnvelopeHeightReading = (sync_flags & MOVIE_SYNC_VOLUMEENVX) ? TRUE : FALSE;
		Settings.FakeMuteFix = (sync_flags & MOVIE_SYNC_FAKEMUTE) ? TRUE : FALSE;
		Settings.UpAndDown = (sync_flags & MOVIE_SYNC_LEFTRIGHT) ? TRUE : FALSE; // doesn't actually affect synchronization
		Settings.SoundSync = (sync_flags & MOVIE_SYNC_SYNCSOUND) ? TRUE : FALSE; // doesn't seem to affect synchronization
		Settings.InitFastROMSetting = (sync_flags2 & MOVIE_SYNC2_INIT_FASTROM) ? TRUE : FALSE;
		//Settings.ShutdownMaster = (sync_flags & MOVIE_SYNC_NOCPUSHUTDOWN) ? FALSE : TRUE;
	}

	// set from movie
	restore_movie_settings();

	if(Movie.Opts & MOVIE_OPT_FROM_RESET)
	{
		Movie.State = MOVIE_STATE_PLAY; // prevent NSRT controller switching (in S9xPostRomInit)
		if(!Memory.LoadLastROM())
			S9xReset();
		Memory.ClearSRAM(false); // in case the SRAM read fails
		Movie.State = MOVIE_STATE_NONE;

		// save only SRAM for a from-reset snapshot
		result=(READ_STREAM(Memory.SRAM, 0x20000, stream) == 0x20000) ? SUCCESS : WRONG_FORMAT;
	}
	else
	{
		result=S9xUnfreezeFromStream(stream);
	}
	CLOSE_STREAM(stream);

	if(result!=SUCCESS)
	{
		return result;
	}

	if(!(fd=fopen(movie_filename, "rb+")))
		if(!(fd=fopen(movie_filename, "rb")))
			return FILE_NOT_FOUND;
		else
			read_only = TRUE;

	if(fseek(fd, Movie.ControllerDataOffset, SEEK_SET))
		return WRONG_FORMAT;

	// read controller data
	Movie.File=fd;
	Movie.BytesPerFrame=bytes_per_frame();
	Movie.InputBufferPtr=Movie.InputBuffer;
	uint32 to_read=Movie.BytesPerFrame * (Movie.MaxFrame+1);
	reserve_buffer_space(to_read);
	fread(Movie.InputBufferPtr, 1, to_read, fd);

	// read "baseline" controller data
	if(Movie.MaxFrame)
		read_frame_controller_data();

	strncpy(Movie.Filename, movie_filename, _MAX_PATH);
	Movie.Filename[_MAX_PATH-1]='\0';
	Movie.CurrentFrame=0;
	Movie.ReadOnly=read_only;
	change_state(MOVIE_STATE_PLAY);

	Settings.Paused = wasPaused;
	Settings.FrameTime = prevFrameTime; // restore emulation speed

	Movie.RecordedThisSession = false;
	S9xUpdateFrameCounter(-1);

	Movie.RequiresReset = false;

	S9xMessage(S9X_INFO, S9X_MOVIE_INFO, MOVIE_INFO_REPLAY);
	return SUCCESS;
}

int S9xMovieCreate (const char* filename, uint8 controllers_mask, uint8 opts, const wchar_t* metadata, int metadata_length)
{
	FILE* fd;
	STREAM stream;
	int fn;

	if(controllers_mask==0)
		return WRONG_FORMAT;

	char movie_filename [_MAX_PATH];
#ifdef WIN32
	_fullpath(movie_filename, filename, _MAX_PATH);
#else
	strcpy(movie_filename, filename);
#endif

	if(!(fd=fopen(movie_filename, "wb")))
		return FILE_NOT_FOUND;

	const bool8 wasPaused = Settings.Paused;
	const uint32 prevFrameTime = Settings.FrameTime;

	// store new settings, otherwise they'll always get overridden when stopping a movie
	store_previous_settings();

	// stop current movie before opening
	change_state(MOVIE_STATE_NONE);

	if(metadata_length>MOVIE_MAX_METADATA)
	{
		metadata_length=MOVIE_MAX_METADATA;
	}

	Movie.MovieId=(uint32)time(NULL);
	Movie.RerecordCount=0;
	Movie.MaxFrame=0;
	Movie.SaveStateOffset=SMV_HEADER_SIZE+(sizeof(uint16)*metadata_length) + SMV_EXTRAROMINFO_SIZE;
	Movie.ControllerDataOffset=0;
	Movie.ControllersMask=controllers_mask;
	Movie.Opts=opts;
	Movie.SyncFlags=MOVIE_SYNC_DATA_EXISTS|MOVIE_SYNC_HASROMINFO;
	Movie.SyncFlags2=0;

	// store settings in movie
	store_movie_settings();

	// extra rom info
	Movie.ROMCRC32 = Memory.ROMCRC32;
	strncpy(Movie.ROMName, Memory.RawROMName, 23);


	write_movie_header(fd, &Movie);


	// convert wchar_t metadata string/array to a uint16 array
	if(metadata_length>0)
	{
		uint8 meta_buf[MOVIE_MAX_METADATA * sizeof(uint16)];
		for(int i=0; i<metadata_length; ++i)
		{
			uint16 c=(uint16)metadata[i];
			meta_buf[i+i]  =(uint8)(c&0xff);
			meta_buf[i+i+1]=(uint8)((c>>8)&0xff);
		}

		fwrite(meta_buf, sizeof(uint16), metadata_length, fd);
		assert(!ferror(fd));
	}

	write_movie_extrarominfo(fd, &Movie);

	// write snapshot
	fn=dup(fileno(fd));
	fclose(fd);

	// lseek(fn, Movie.SaveStateOffset, SEEK_SET);
	if(!(stream=REOPEN_STREAM(fn, "ab")))
		return FILE_NOT_FOUND;

	if(opts & MOVIE_OPT_FROM_RESET)
	{
		if(!Memory.LoadLastROM())
			S9xReset();
		Memory.ClearSRAM(true); // clear non-saving SRAM
		// save only SRAM for a from-reset snapshot
		WRITE_STREAM(Memory.SRAM, 0x20000, stream);
	}
	else
	{
		S9xFreezeToStream(stream);
	}
	CLOSE_STREAM(stream);

	if(!(fd=fopen(movie_filename, "rb+")))
		return FILE_NOT_FOUND;

	fseek(fd, 0, SEEK_END);
	Movie.ControllerDataOffset=(uint32)ftell(fd);

	// 16-byte align the controller input, for hex-editing friendliness if nothing else
	while(Movie.ControllerDataOffset % 16)
	{
		fputc(0xCC, fd); // arbitrary
		Movie.ControllerDataOffset++;
	}

	// write "baseline" controller data
	Movie.File=fd;
	Movie.BytesPerFrame=bytes_per_frame();
	Movie.InputBufferPtr=Movie.InputBuffer;
	write_frame_controller_data();

	strncpy(Movie.Filename, movie_filename, _MAX_PATH);
	Movie.Filename[_MAX_PATH-1]='\0';
	Movie.CurrentFrame=0;
	Movie.ReadOnly=false;
	change_state(MOVIE_STATE_RECORD);

	Settings.Paused = wasPaused;
	Settings.FrameTime = prevFrameTime; // restore emulation speed

	Movie.RecordedThisSession = true;
	S9xUpdateFrameCounter(-1);

	Movie.RequiresReset = false;

	S9xMessage(S9X_INFO, S9X_MOVIE_INFO, MOVIE_INFO_RECORD);
	return SUCCESS;
}

bool8 S9xMovieRestart ()
{
	return false; // NYI
}

void S9xMovieRecordReset ()
{
	switch(Movie.State)
	{
		case MOVIE_STATE_RECORD:
		{
			Movie.RequiresReset = true;
		}
		break;
		default: break;
	}
}

// do not refer to Movie.RequiresReset directly
// because it is used only with movie recording
bool S9xMovieRequiresReset ()
{
	switch(Movie.State)
	{
		case MOVIE_STATE_NONE:
		{
			Movie.RequiresReset = false;
		}
		break;

		case MOVIE_STATE_PLAY:
		{
			Movie.RequiresReset = does_frame_data_mean_reset();
		}
		break;
		default: break;
	}
	return Movie.RequiresReset!=0;
}

void S9xMovieUpdateOnReset ()
{
	switch(Movie.State)
	{
		case MOVIE_STATE_PLAY:
		{
			assert(!does_frame_data_mean_reset());
			// skip the reset
			Movie.CurrentFrame++;
			Movie.InputBufferPtr += Movie.BytesPerFrame;
		}
		break;

		case MOVIE_STATE_RECORD:
		{
			reserve_buffer_space((uint32)((Movie.InputBufferPtr+Movie.BytesPerFrame)-Movie.InputBuffer));
			memset(Movie.InputBufferPtr, 0xFF, Movie.BytesPerFrame);
			Movie.InputBufferPtr += Movie.BytesPerFrame;
			Movie.MaxFrame = ++Movie.CurrentFrame;
			fwrite((Movie.InputBufferPtr - Movie.BytesPerFrame), 1, Movie.BytesPerFrame, Movie.File);
			assert(!ferror(Movie.File));
		}
		break;
		default: break;
	}
	Movie.RequiresReset = false;
}

bool MovieGetJoypadNext(int which, uint16 &pad)
{
	if (which < 0 || which > 4)
		return false;

	switch(Movie.State)
	{
	case MOVIE_STATE_PLAY:
		{
			if(Movie.CurrentFrame>=Movie.MaxFrame)
				return false;
			else
			{
				if(Movie.ControllersMask & (1<<which)) {
					uint8 *inputBuf;
					int padOffset;
					int i;

					padOffset = 0;
					for (i = 0; i < which; i++) {
						if (Movie.ControllersMask & (1<<i)) {
							padOffset += 2;
						}
					}

					inputBuf = &Movie.InputBufferPtr[padOffset];
					pad = Read16(inputBuf);
				}
				else
					pad = 0;		// pretend the controller is disconnected
				return true;
			}
		}
		break;

	default:
		return false;
	}
}

void S9xMovieUpdate ()
{
movieUpdateStart:
	switch(Movie.State)
	{
	case MOVIE_STATE_PLAY:
		{
			if(Movie.CurrentFrame>=Movie.MaxFrame)
			{
				if(!Movie.RecordedThisSession)
				{
					// stop movie; it reached the end
					change_state(MOVIE_STATE_NONE);
					S9xMessage(S9X_INFO, S9X_MOVIE_INFO, MOVIE_INFO_END);
					return;
				}
				else
				{
					// if user has been recording this movie since the last time it started playing,
					// they probably don't want the movie to end now during playback,
					// so switch back to recording when it reaches the end
					change_state(MOVIE_STATE_RECORD);
					S9xMessage(S9X_INFO, S9X_MOVIE_INFO, MOVIE_INFO_RECORD);
					fseek(Movie.File, Movie.ControllerDataOffset+(Movie.BytesPerFrame * (Movie.CurrentFrame+1)), SEEK_SET);
					Settings.Paused = true; // also pause so it doesn't keep going unless they want it to
					goto movieUpdateStart;
				}
			}
			else
			{
				S9xUpdateFrameCounter();

				read_frame_controller_data();
				++Movie.CurrentFrame;
			}
		}
		break;

	case MOVIE_STATE_RECORD:
		{
			S9xUpdateFrameCounter();

			write_frame_controller_data();
			++Movie.CurrentFrame;
			Movie.MaxFrame=Movie.CurrentFrame;
			fwrite((Movie.InputBufferPtr - Movie.BytesPerFrame), 1, Movie.BytesPerFrame, Movie.File);
			assert(!ferror(Movie.File));

			Movie.RecordedThisSession = true;
		}
		break;

	default:
		S9xUpdateFrameCounter();
		break;
	}
}

void S9xMovieStop (bool8 suppress_message)
{
	if(Movie.State!=MOVIE_STATE_NONE)
	{
		change_state(MOVIE_STATE_NONE);

		if(!suppress_message)
			S9xMessage(S9X_INFO, S9X_MOVIE_INFO, MOVIE_INFO_STOP);
	}
}

int S9xMovieGetInfo (const char* filename, struct MovieInfo* info)
{
	flush_movie();

	FILE* fd;
	int result;
	SMovie local_movie;
	int metadata_length;

	memset(info, 0, sizeof(*info));
	if(!(fd=fopen(filename, "rb")))
		return FILE_NOT_FOUND;

	result = read_movie_header(fd, &local_movie);

	info->TimeCreated=(time_t)local_movie.MovieId;
	info->RerecordCount=local_movie.RerecordCount;
	info->LengthFrames=local_movie.MaxFrame;
	info->Version=local_movie.Version;

	if(result!=SUCCESS)
		return result;

	info->Opts=local_movie.Opts;
	info->SyncFlags=local_movie.SyncFlags;
	info->SyncFlags2=local_movie.SyncFlags2;
	info->ControllersMask=local_movie.ControllersMask;

	if(local_movie.SaveStateOffset > SMV_HEADER_SIZE)
	{
		uint8 meta_buf[MOVIE_MAX_METADATA * sizeof(uint16)];
		int i;

		int curRomInfoSize = (local_movie.SyncFlags & MOVIE_SYNC_HASROMINFO) != 0 ? SMV_EXTRAROMINFO_SIZE : 0;

		metadata_length=((int)local_movie.SaveStateOffset-SMV_HEADER_SIZE-curRomInfoSize)/sizeof(uint16);
		metadata_length=(metadata_length>=MOVIE_MAX_METADATA) ? MOVIE_MAX_METADATA-1 : metadata_length;
		metadata_length=(int)fread(meta_buf, sizeof(uint16), metadata_length, fd);

		for(i=0; i<metadata_length; ++i)
		{
			uint16 c=meta_buf[i+i] | (meta_buf[i+i+1] << 8);
			info->Metadata[i]=(wchar_t)c;
		}
		info->Metadata[i]='\0';
	}
	else
	{
		info->Metadata[0]='\0';
	}

	read_movie_extrarominfo(fd, &local_movie);

	info->ROMCRC32=local_movie.ROMCRC32;
	strncpy(info->RawROMName,local_movie.ROMName,23);
	strncpy(info->ROMName,Memory.Safe((const char *) info->RawROMName),23);

	fclose(fd);

	if(access(filename, W_OK))
		info->ReadOnly=true;

	return SUCCESS;
}

bool8 S9xMovieActive ()
{
	return (Movie.State!=MOVIE_STATE_NONE);
}
bool8 S9xMoviePlaying ()
{
	return (Movie.State==MOVIE_STATE_PLAY);
}
bool8 S9xMovieRecording ()
{
	return (Movie.State==MOVIE_STATE_RECORD);
}

uint8 S9xMovieControllers ()
{
	return Movie.ControllersMask;
}

bool8 S9xMovieReadOnly ()
{
	if(!S9xMovieActive())
		return false;

	return Movie.ReadOnly;
}

uint32 S9xMovieGetId ()
{
	if(!S9xMovieActive())
		return 0;

	return Movie.MovieId;
}

uint32 S9xMovieGetLength ()
{
	if(!S9xMovieActive())
		return 0;

	return Movie.MaxFrame;
}

uint32 S9xMovieGetFrameCounter ()
{
	if(!S9xMovieActive())
		return 0;

	return Movie.CurrentFrame;
}

void S9xMovieToggleRecState()
{
   Movie.ReadOnly=!Movie.ReadOnly;
        
   if (Movie.ReadOnly) 
       S9xMessage(S9X_INFO, S9X_MOVIE_INFO, "Movie is now read-only.");
   else
       S9xMessage(S9X_INFO, S9X_MOVIE_INFO, "Movie is now read+write.");
}

void S9xMovieToggleFrameDisplay ()
{
	Settings.DisplayFrame = !Settings.DisplayFrame;
	// updating the frame counter string here won't work, because it may or may not be 1 too high now
	extern void S9xReRefresh();
	S9xReRefresh();
}

void S9xMovieFreeze (uint8** buf, uint32* size)
{
	// sanity check
	if(!S9xMovieActive())
	{
		return;
	}

	*buf = NULL;
	*size = 0;

	// compute size needed for the buffer
	uint32 size_needed = sizeof(Movie.MovieId) + sizeof(Movie.CurrentFrame) + sizeof(Movie.MaxFrame);
	size_needed += (uint32)(Movie.BytesPerFrame * (Movie.MaxFrame+1));
	*buf=new uint8[size_needed];
	*size=size_needed;

	uint8* ptr = *buf;
	if(!ptr)
	{
		return;
	}

	Write32(Movie.MovieId, ptr);
	Write32(Movie.CurrentFrame, ptr);
	Write32(Movie.MaxFrame, ptr);

	memcpy(ptr, Movie.InputBuffer, Movie.BytesPerFrame * (Movie.MaxFrame+1));
}

int S9xMovieUnfreeze (const uint8* buf, uint32 size)
{
	// sanity check
	if(!S9xMovieActive())
	{
		return FILE_NOT_FOUND;
	}

	const uint8* ptr = buf;
	if(size < sizeof(Movie.MovieId) + sizeof(Movie.CurrentFrame) + sizeof(Movie.MaxFrame) )
	{
		return WRONG_FORMAT;
	}

	uint32 movie_id = Read32(ptr);
	uint32 current_frame = Read32(ptr);
	uint32 max_frame = Read32(ptr);
	uint32 space_needed = (Movie.BytesPerFrame * (max_frame+1));

	if(current_frame > max_frame ||
		space_needed > size)
	{
		return WRONG_MOVIE_SNAPSHOT;
	}

	if(movie_id != Movie.MovieId)
		if(Settings.WrongMovieStateProtection)
			if(max_frame < Movie.MaxFrame ||
				memcmp(Movie.InputBuffer, ptr, space_needed))
				return WRONG_MOVIE_SNAPSHOT;

	if(!Movie.ReadOnly)
	{
		// here, we are going to take the input data from the savestate
		// and make it the input data for the current movie, then continue
		// writing new input data at the currentframe pointer
		change_state(MOVIE_STATE_RECORD);
//		S9xMessage(S9X_INFO, S9X_MOVIE_INFO, MOVIE_INFO_RERECORD);

		Movie.CurrentFrame = current_frame;
		Movie.MaxFrame = max_frame;
		if (!S9xLuaRerecordCountSkip())
			++Movie.RerecordCount;

		// when re-recording, update the sync info in the movie to the new settings as of the last re-record.
		store_movie_settings();

		reserve_buffer_space(space_needed);
		memcpy(Movie.InputBuffer, ptr, space_needed);
		flush_movie();
		fseek(Movie.File, Movie.ControllerDataOffset+(Movie.BytesPerFrame * (Movie.CurrentFrame+1)), SEEK_SET);
	}
	else
	{
		// here, we are going to keep the input data from the movie file
		// and simply rewind to the currentframe pointer
		// this will cause a desync if the savestate is not in sync // <-- NOT ANYMORE
		// with the on-disk recording data, but it's easily solved
		// by loading another savestate or playing the movie from the beginning

		// don't allow loading a state inconsistent with the current movie
		uint32 space_shared = (Movie.BytesPerFrame * (current_frame+1));
		if(current_frame > Movie.MaxFrame ||
			memcmp(Movie.InputBuffer, ptr, space_shared))
		{
			return SNAPSHOT_INCONSISTENT;
		}

		change_state(MOVIE_STATE_PLAY);
//		S9xMessage(S9X_INFO, S9X_MOVIE_INFO, MOVIE_INFO_REWIND);

		Movie.CurrentFrame = current_frame;
	}

	Movie.InputBufferPtr = Movie.InputBuffer + (Movie.BytesPerFrame * Movie.CurrentFrame);
	Movie.RequiresReset = false;
	read_frame_controller_data();

	return SUCCESS;
}
