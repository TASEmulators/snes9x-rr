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



#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#if defined(__unix) || defined(__linux) || defined(__sun) || defined(__DJGPP)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#define BOOL bool
#endif

#include "snapshot.h"
#include "snaporig.h"
#include "memmap.h"
#include "snes9x.h"
#include "65c816.h"
#include "ppu.h"
#include "cpuexec.h"
#include "display.h"
#include "apu.h"
#include "soundux.h"
#include "sa1.h"
#include "srtc.h"
#include "sdd1.h"
#include "spc7110.h"
#include "movie.h"
#include "dsp1.h"
#include "language.h"
#include "luasav.h"

//you would think everyone would have these
//since they're so useful.
#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

extern uint8 *SRAM;

#ifdef ZSNES_FX
START_EXTERN_C
void S9xSuperFXPreSaveState ();
void S9xSuperFXPostSaveState ();
void S9xSuperFXPostLoadState ();
END_EXTERN_C
#endif

#ifdef _DEBUG
	#define SNAPSHOT_VERIFY_SUPPORTED
#endif
#ifdef SNAPSHOT_VERIFY_SUPPORTED
	// verify that a snapshot saved of the current emulation state would exactly match
	// with the snapshot that's already in the given file or stream (this is for desync testing)
	bool S9xVerifySnapshotsIdentical (const char *filename);
	bool S9xVerifySnapshotsIdentical (STREAM stream);

	// list of things to ignore (if the error message contains one of these strings it will be discarded)
	// only things that aren't really part of the emulation state should go in here
	static const char* s_snapshotVerifyIgnoreFilter [] = {
		"&SoundData.", // because SoundData is full of stuff that changes asynchronously from the emulation
		"&GFX.", // because I think this can have stuff like GUI text in its pixels and it doesn't matter if it's not the same
		//"movie_freeze_buf", // for comparing states that are in different movie files but should otherwise be identical
	};

	#include <vector>
	#include <string>
	std::vector<std::string> g_verifyErrors;
	static std::vector<std::string> s_verifyInfoStack;
	struct PointerRangeInfo
	{
		void* start; // (pointer to a byte in the savestate)
		void* end; // (1 past the last byte)
		std::string name;
	};
	static std::vector<PointerRangeInfo> s_verifyActiveRangeInfo;

	static void PushVerifyError(void* pCurStateBlockStartByte, unsigned char* pCurStateByte, unsigned char* pOldStateByte)
	{
		std::string errStr = "";
		for(unsigned int i = 0; i < s_verifyInfoStack.size(); i++)
		{
			if(i)
				errStr += ':';
			errStr += s_verifyInfoStack[i];
		}
		for(unsigned int i = 0; i < s_verifyActiveRangeInfo.size(); i++)
		{
			if(pCurStateByte >= s_verifyActiveRangeInfo[i].start && pCurStateByte < s_verifyActiveRangeInfo[i].end)
			{
				errStr += '.';
				errStr += s_verifyActiveRangeInfo[i].name;
				pCurStateBlockStartByte = s_verifyActiveRangeInfo[i].start;
			}
		}
		errStr += ':';
		char temp [256];
		sprintf(temp, " " /*"mismatch at "*/ "byte %d(0x%X): %d(0x%X) != %d(0x%X)\n", (int)pCurStateByte-(int)pCurStateBlockStartByte, (int)pCurStateByte-(int)pCurStateBlockStartByte, *pCurStateByte,*pCurStateByte, *pOldStateByte,*pOldStateByte);
		errStr += temp;

		for(int i = 0; i < sizeof(s_snapshotVerifyIgnoreFilter)/sizeof(*s_snapshotVerifyIgnoreFilter); i++)
			if(strstr(errStr.c_str(), s_snapshotVerifyIgnoreFilter[i]))
				return; // ignore this error...

		g_verifyErrors.push_back(errStr); // <-- probably the best place for a breakpoint
	}

	static bool s_verifyingSnapshot = false;

	static int WRITE_STREAM_ORIGINAL(void* p, int l, STREAM s)
	{
		return WRITE_STREAM(p,l,s);
	}
	#undef WRITE_STREAM
	static void WRITE_STREAM(void* p, int l, STREAM s)
	{
		if(s_verifyingSnapshot)
		{
			unsigned char* temp = new unsigned char[l];
			READ_STREAM(temp,l,s);
			unsigned char* pCurStateByte = (unsigned char*)p;
			unsigned char* pOldStateByte = temp;
			int errorsFound = 0;
			for(int i = 0; i < l; i++, pCurStateByte++, pOldStateByte++)
			{
				if(*pCurStateByte != *pOldStateByte)
				{
					PushVerifyError(p, pCurStateByte, pOldStateByte);
					errorsFound++;
					if(errorsFound >= 10)
						break;
				}
			}
			delete[] temp;
		}
		else
		{
			WRITE_STREAM_ORIGINAL(p,l,s);
		}
	}
	static void PreVerifySnapshotsIdentical()
	{
		g_verifyErrors.clear();
		s_verifyInfoStack.clear();
		s_verifyActiveRangeInfo.clear();
		s_verifyingSnapshot = true;
	}
	static bool PostVerifySnapshotsIdentical()
	{
		s_verifyingSnapshot = false;
		if(!g_verifyErrors.empty())
		{
#ifdef WIN32
			OutputDebugString("\n");
#endif
			fputs("\n", stdout);

			for(unsigned int i = 0; i < min(100,g_verifyErrors.size()); i++)
			{
#ifdef WIN32
				OutputDebugString(g_verifyErrors[i].c_str());
#endif
				fputs(g_verifyErrors[i].c_str(), stdout);
			}

			return false;
		}
		return true;
	}
	bool S9xVerifySnapshotsIdentical (STREAM stream)
	{
		PreVerifySnapshotsIdentical();
		S9xFreezeToStream(stream);
		return PostVerifySnapshotsIdentical();
	}
	bool S9xVerifySnapshotsIdentical (const char *filename)
	{
		PreVerifySnapshotsIdentical();
		S9xFreezeGame(filename);
		return PostVerifySnapshotsIdentical();
	}

#define FreezeStruct(stream,name,base,fields,num_fields)\
	do{ if(s_verifyingSnapshot) s_verifyInfoStack.push_back(#base);\
	FreezeStructF(stream,name,base,fields,num_fields);\
	if(s_verifyingSnapshot) s_verifyInfoStack.pop_back();} while(0)

#define FreezeBlock(stream,name,block,size)\
	do{ if(s_verifyingSnapshot) s_verifyInfoStack.push_back(#block);\
	FreezeBlockF(stream,name,block,size);\
	if(s_verifyingSnapshot) s_verifyInfoStack.pop_back();} while(0)

#else
	#define FreezeStruct FreezeStructF
	#define FreezeBlock FreezeBlockF
#endif

void S9xResetSaveTimer(bool8 dontsave){
    static time_t t=-1;

    if(!dontsave && t!=-1 && time(NULL)-t>300){{
        char def [PATH_MAX];
        char filename [PATH_MAX];
        char drive [_MAX_DRIVE];
        char dir [_MAX_DIR];
        char ext [_MAX_EXT];

        _splitpath(Memory.ROMFilename, drive, dir, def, ext);
        sprintf(filename, "%s%s%s.%.*s", S9xGetDirectory(SNAPSHOT_DIR),
                SLASH_STR, def, _MAX_EXT-1, "oops");
        S9xMessage(S9X_INFO, S9X_FREEZE_FILE_INFO, "Auto-saving 'oops' savestate");
        Snapshot(filename);
    }}
    t=time(NULL);
}

bool8 S9xUnfreezeZSNES (const char *filename);

typedef struct {
    int offset;
    int offset2;
    int size; // this is not necessarily in bytes (use FreezeSize to calculate that)
    int type;
    uint16 debuted_in, deleted_in;
    const char* name;
} FreezeData;

enum {
    INT_V, uint8_ARRAY_V, uint16_ARRAY_V, uint32_ARRAY_V,
    uint8_INDIR_ARRAY_V, uint16_INDIR_ARRAY_V, uint32_INDIR_ARRAY_V, POINTER_V // not exist in older version
};

static struct Obsolete {
    uint8 dummy;
} Obsolete;

#define COUNT(ARRAY) (sizeof (ARRAY) / sizeof (ARRAY[0]))
#define SIZE_TO_ARRAY_TYPE(s) ((s)==1 ? uint8_ARRAY_V : ((s)==2 ? uint16_ARRAY_V : uint32_ARRAY_V))

#define Offset(field,structure) \
((int) (((char *) (&(((structure)NULL)->field))) - ((char *) NULL)))
#define DUMMY(f) Offset(f,struct Obsolete *)
#define DELETED(f) (-1)
#define OFFSET(f) Offset(f, STRUCT *)
//#define ARRAY_ENTRY(save_version_introduced, field) {OFFSET(field), COUNT(((STRUCT*)NULL)->field), SIZE_TO_ARRAY_TYPE(sizeof(((STRUCT*)NULL)->field)), save_version_introduced, 9999}
#define INT_ENTRY(save_version_introduced, field) {OFFSET(field),0, sizeof(((STRUCT*)NULL)->field), INT_V, save_version_introduced, 9999, #field}
#define ARRAY_ENTRY(save_version_introduced, field, count, elemType) {OFFSET(field),0, count, elemType, save_version_introduced, 9999, #field}
#define POINTER_ENTRY(save_version_introduced, field, relativeToField) {OFFSET(field),OFFSET(relativeToField), 4, POINTER_V, save_version_introduced, 9999, #field} // size=4 -> (field - relativeToField) must fit in 4 bytes
#define MEMPOINTER_ENTRY(save_version_introduced, field) {OFFSET(field),-1, 4, POINTER_V, save_version_introduced, 9999, #field} // like POINTER_ENTRY but always uses SNES memory Memory.Base as the pointer base
#define OBSOLETE_INT_ENTRY(save_version_introduced, save_version_removed, field) {DUMMY(field),0, sizeof(((struct Obsolete*)NULL)->field), INT_V, save_version_introduced, save_version_removed, #field}
#define OBSOLETE_ARRAY_ENTRY(save_version_introduced, save_version_removed, field, count, elemType) {DUMMY(field),0, count, elemType, save_version_introduced, save_version_removed, #field}
#define OBSOLETE_POINTER_ENTRY(save_version_introduced, save_version_removed, field, relativeToField) {DUMMY(field),DUMMY(relativeToField), 4, POINTER_V, save_version_introduced, save_version_removed, #field} // size=4 -> (field - relativeToField) must fit in 4 bytes
#define DELETED_INT_ENTRY(save_version_introduced, save_version_removed, field, size) {DELETED(field),0, size, INT_V, save_version_introduced, save_version_removed, #field}
#define DELETED_ARRAY_ENTRY(save_version_introduced, save_version_removed, field, count, elemType) {DELETED(field),0, count, elemType, save_version_introduced, save_version_removed, #field}
#define DELETED_POINTER_ENTRY(save_version_introduced, save_version_removed, field, relativeToField) {DELETED(field),DELETED(relativeToField), 4, POINTER_V, save_version_introduced, save_version_removed, #field} // size=4 -> (field - relativeToField) must fit in 4 bytes

#define V1_RR_UNOFFICIAL 1430  // unofficial members added by rerecording team
// (see SNAPSHOT_VERSION)

struct SnapshotMovieInfo
{
	uint32	MovieInputDataSize;
};

#undef STRUCT
#define STRUCT struct SnapshotMovieInfo

static FreezeData SnapMovie [] = {
	INT_ENTRY(1, MovieInputDataSize),
};

#undef STRUCT
#define STRUCT struct SCPUState

static FreezeData SnapCPU [] = {
	INT_ENTRY(1, Flags),
	INT_ENTRY(1, BranchSkip),
	INT_ENTRY(1, NMIActive),
	INT_ENTRY(1, IRQActive),
	INT_ENTRY(1, WaitingForInterrupt),
	INT_ENTRY(1, WhichEvent),
	INT_ENTRY(1, Cycles),
	INT_ENTRY(1, NextEvent),
	INT_ENTRY(1, V_Counter),
	INT_ENTRY(1, MemSpeed),
	INT_ENTRY(1, MemSpeedx2),
	INT_ENTRY(1, FastROMSpeed),

	// not sure if the following are necessary, but better safe than sorry
	INT_ENTRY(V1_RR_UNOFFICIAL, WaitCounter),
	INT_ENTRY(V1_RR_UNOFFICIAL, TriedInterleavedMode2),
	INT_ENTRY(V1_RR_UNOFFICIAL, InDMA),
	INT_ENTRY(V1_RR_UNOFFICIAL, NMICycleCount),
	INT_ENTRY(V1_RR_UNOFFICIAL, IRQCycleCount),
	INT_ENTRY(V1_RR_UNOFFICIAL, NMITriggerPoint),
	INT_ENTRY(V1_RR_UNOFFICIAL, BRKTriggered),
    MEMPOINTER_ENTRY(V1_RR_UNOFFICIAL, PC),
    MEMPOINTER_ENTRY(V1_RR_UNOFFICIAL, PCAtOpcodeStart),
	MEMPOINTER_ENTRY(V1_RR_UNOFFICIAL, WaitAddress),
    MEMPOINTER_ENTRY(V1_RR_UNOFFICIAL, PCBase),
};

#undef STRUCT
#define STRUCT struct SICPU

static FreezeData SnapICPU [] = {
	// not sure if the following are necessary (actually I'm pretty sure some of them are), but better safe than sorry
    INT_ENTRY(V1_RR_UNOFFICIAL, _Carry),
    INT_ENTRY(V1_RR_UNOFFICIAL, _Zero),
    INT_ENTRY(V1_RR_UNOFFICIAL, _Negative),
    INT_ENTRY(V1_RR_UNOFFICIAL, _Overflow),
    INT_ENTRY(V1_RR_UNOFFICIAL, CPUExecuting),
    INT_ENTRY(V1_RR_UNOFFICIAL, ShiftedPB),
    INT_ENTRY(V1_RR_UNOFFICIAL, ShiftedDB),
    INT_ENTRY(V1_RR_UNOFFICIAL, Frame),
    INT_ENTRY(V1_RR_UNOFFICIAL, Scanline),
};


extern uint8 *HDMAMemPointers [8];
extern uint8 *HDMABasePointers [8];

// some global variables that unfortunately we probably have to save.
// only Work32 is confirmed definitely necessary to save due to a bug in spc700.cpp
// which is only in the 1.43 branch (seems to be already fixed in 1.51).
// the rest are a "just in case, maybe they are needed too" deal.
#define GLOBAL_JUNK_DEF \
	GJFORMAT(uint8,       A1       ) \
	GJFORMAT(uint8,       A2       ) \
	GJFORMAT(uint8,       A3       ) \
	GJFORMAT(uint8,       A4       ) \
	GJFORMAT(uint8,       W1       ) \
	GJFORMAT(uint8,       W2       ) \
	GJFORMAT(uint8,       W3       ) \
	GJFORMAT(uint8,       W4       ) \
	GJFORMAT(uint8,       Ans8     ) \
	GJFORMAT(uint16,      Ans16    ) \
	GJFORMAT(uint32,      Ans32    ) \
	GJFORMAT(uint8,       Work8    ) \
	GJFORMAT(uint16,      Work16   ) \
	GJFORMAT(uint32,      Work32   ) \
	GJFORMAT(signed char, Int8     ) \
	GJFORMAT(short,       Int16    ) \
	GJFORMAT(long,        Int32    ) \
	GJFORMAT(long,        OpAddress) \
	GJFORMAT(uint8,       OpenBus  ) \
	GJFORMAT(int,         OBC1_Address ) \
	GJFORMAT(int,         OBC1_BasePtr ) \
	GJFORMAT(int,         OBC1_Shift ) \
	GJFORMAT(uint8,       KeyOn ) \
	GJFORMAT(uint8,       KeyOnPrev ) \
	//end

// extern the globals so we can reference them
#define GJFORMAT(t,v) extern "C" t v;
	GLOBAL_JUNK_DEF
#undef GJFORMAT

// define a struct to put them in so we can save and load the struct
struct SGlobalJunk
{
	#define GJFORMAT(t,v) t v;
		GLOBAL_JUNK_DEF
	#undef GJFORMAT
	uint8* HDMAMemPointers [8];
	uint8* HDMABasePointers [8];
};

// define a function that fills a struct with the current global variables and returns them, so we can save the struct
static SGlobalJunk GetCurrentGlobalJunk()
{
	SGlobalJunk gj = {
		#define GJFORMAT(t,v) v,
			GLOBAL_JUNK_DEF
		#undef GJFORMAT
		{HDMAMemPointers[0], HDMAMemPointers[1], HDMAMemPointers[2], HDMAMemPointers[3], HDMAMemPointers[4], HDMAMemPointers[5], HDMAMemPointers[6], HDMAMemPointers[7], },
		{HDMABasePointers[0], HDMABasePointers[1], HDMABasePointers[2], HDMABasePointers[3], HDMABasePointers[4], HDMABasePointers[5], HDMABasePointers[6], HDMABasePointers[7], },
	};
	return gj;
}

// define a function that takes a struct and sets the current global variables from them, so we can load the struct
static void SetCurrentGlobalJunk(SGlobalJunk gj)
{
	#define GJFORMAT(t,v) v = gj.v;
		GLOBAL_JUNK_DEF
	#undef GJFORMAT
	for(int i = 0; i < 8; i++)
		HDMAMemPointers[i] = gj.HDMAMemPointers[i];
	for(int i = 0; i < 8; i++)
		HDMABasePointers[i] = gj.HDMABasePointers[i];
}

void ResetGlobalJunk()
{
	#define GJFORMAT(t,v) v = 0;
		GLOBAL_JUNK_DEF
	#undef GJFORMAT
}

// define a snapshot definition struct like usual

#undef STRUCT
#define STRUCT struct SGlobalJunk

static FreezeData SnapGlobalJunk [] = {
	#define GJFORMAT(t,v) INT_ENTRY(V1_RR_UNOFFICIAL, v),
		GLOBAL_JUNK_DEF
	#undef GJFORMAT
#define O(N) MEMPOINTER_ENTRY(V1_RR_UNOFFICIAL, HDMAMemPointers[N])
    O(0), O(1), O(2), O(3), O(4), O(5), O(6), O(7),
#undef O
#define O(N) MEMPOINTER_ENTRY(V1_RR_UNOFFICIAL, HDMABasePointers[N])
    O(0), O(1), O(2), O(3), O(4), O(5), O(6), O(7),
#undef O
};

// that's all for that
#undef GLOBAL_JUNK_DEF



#ifdef ZSNES_FX

extern "C" uint8 regptra;
extern "C" uint8 regptwa;
extern "C" uint8* SFXPlotTable;

#define GLOBAL_SUPERFX_DEF \
	GSFXFORMAT(uint32, SfxR0, 0) \
	GSFXFORMAT(uint32, SfxR1, 0) \
	GSFXFORMAT(uint32, SfxR2, 0) \
	GSFXFORMAT(uint32, SfxR3, 0) \
	GSFXFORMAT(uint32, SfxR4, 0) \
	GSFXFORMAT(uint32, SfxR5, 0) \
	GSFXFORMAT(uint32, SfxR6, 0) \
	GSFXFORMAT(uint32, SfxR7, 0) \
	GSFXFORMAT(uint32, SfxR8, 0) \
	GSFXFORMAT(uint32, SfxR9, 0) \
	GSFXFORMAT(uint32, SfxR10, 0) \
	GSFXFORMAT(uint32, SfxR11, 0) \
	GSFXFORMAT(uint32, SfxR12, 0) \
	GSFXFORMAT(uint32, SfxR13, 0) \
	GSFXFORMAT(uint32, SfxR14, 0) \
	GSFXFORMAT(uint32, SfxR15, 0) \
	GSFXFORMAT(uint32, SfxSFR, 0) \
	GSFXFORMAT(uint32, SfxBRAMR, 0) \
	GSFXFORMAT(uint32, SfxPBR, 0) \
	GSFXFORMAT(uint32, SfxROMBR, 0) \
	GSFXFORMAT(uint32, SfxCFGR, 0) \
	GSFXFORMAT(uint32, SfxSCBR, 0) \
	GSFXFORMAT(uint32, SfxCLSR, 0) \
	GSFXFORMAT(uint32, SfxSCMR, 0) \
	GSFXFORMAT(uint32, SfxVCR, 0) \
	GSFXFORMAT(uint32, SfxRAMBR, 0) \
	GSFXFORMAT(uint32, SfxCBR, 0) \
	GSFXFORMAT(uint32, SfxCOLR, 0) \
	GSFXFORMAT(uint32, SfxPOR, 0) \
	GSFXFORMAT(uint32, SfxCacheFlags, 0) \
	GSFXFORMAT(uint8*, SfxLastRamAdr, Memory.SRAM) \
	GSFXFORMAT(uint32, SfxDREG, 0) \
	GSFXFORMAT(uint32, SfxSREG, 0) \
	GSFXFORMAT(uint8*, SfxRomBuffer, Memory.ROM) \
	GSFXFORMAT(uint32, SfxPIPE, 0) \
	GSFXFORMAT(uint32, SfxPipeAdr, 0) \
	GSFXFORMAT(uint32, SfxnRamBanks, 0) \
	GSFXFORMAT(uint32, SfxnRomBanks, 0) \
	GSFXFORMAT(uint32, SfxvScreenHeight, 0) \
	GSFXFORMAT(uint32, SfxvScreenSize, 0) \
	GSFXFORMAT(uint32, SfxCacheActive, 0) \
	GSFXFORMAT(uint32, SfxCarry, 0) \
	GSFXFORMAT(uint32, SfxSignZero, 0) \
	GSFXFORMAT(uint32, SfxB, 0) \
	GSFXFORMAT(uint32, SfxOverflow, 0) \
	/* note: PHnum2writesfxreg MUST NOT be listed here */ \
	GSFXFORMAT(uint8*, SfxCPB, Memory.ROM) \
	GSFXFORMAT(uint8*, SfxCROM, Memory.ROM) \
	GSFXFORMAT(uint8*, SfxRAMMem, Memory.SRAM) \
	GSFXFORMAT(uint32, withr15sk, 0) \
	GSFXFORMAT(uint8*, sfxclineloc, SFXPlotTable) \
	GSFXFORMAT(uint32, fxbit01pcal, 0) \
	GSFXFORMAT(uint32, fxbit23pcal, 0) \
	GSFXFORMAT(uint32, fxbit45pcal, 0) \
	GSFXFORMAT(uint32, fxbit67pcal, 0) \
	GSFXFORMAT(uint32, SFXCounter, 0) \
	GSFXFORMAT(uint32, SfxAC, 0) \
	GSFXFORMAT(uint32, flagnz, 0) \
	GSFXFORMAT(uint8*, regptr, &regptra - 0x8000) \
	GSFXFORMAT(uint8*, regptw, &regptwa - 0x8000) \
	GSFXFORMAT(uint32, NumberOfOpcodes, 0) \
	GSFXFORMAT(uint32, NumberOfOpcodesBU, 0) \
	GSFXFORMAT(uint8,  sfxwarningb, 0) \
	GSFXFORMAT(uint8*, sfx128lineloc, SFXPlotTable) \
	GSFXFORMAT(uint8*, sfx160lineloc, SFXPlotTable) \
	GSFXFORMAT(uint8*, sfx192lineloc, SFXPlotTable) \
	GSFXFORMAT(uint8*, sfxobjlineloc, SFXPlotTable) \
	GSFXFORMAT(uint8,  cachewarning, 0) \
	GSFXFORMAT(uint32, SFXProc, 0) \
	GSFXFORMAT(int32,  ChangeOps, 0) \
	//end

#define GSFXFORMAT(t,v,r) extern "C" t v;
	GLOBAL_SUPERFX_DEF
#undef GSFXFORMAT

struct SGlobalSuperFX
{
	#define GSFXFORMAT(t,v,r) t v;
		GLOBAL_SUPERFX_DEF
	#undef GSFXFORMAT
};

static SGlobalSuperFX GetCurrentGlobalSuperFX()
{
	SGlobalSuperFX gsfx = {
		#define GSFXFORMAT(t,v,r) (r) ? (v ? v - pint(r) : (t)(-1)) : v,
			GLOBAL_SUPERFX_DEF
		#undef GSFXFORMAT
	};
	return gsfx;
}

static void SetCurrentGlobalSuperFX(SGlobalSuperFX gsfx)
{
	#define GSFXFORMAT(t,v,r) v = (r) ? (gsfx.v!=(t)(-1) ? gsfx.v + pint(r) : NULL) : gsfx.v;
		GLOBAL_SUPERFX_DEF
	#undef GSFXFORMAT
}

void ResetGlobalSuperFX()
{
	#define GSFXFORMAT(t,v,r) v = 0;
		GLOBAL_SUPERFX_DEF
	#undef GSFXFORMAT
	SfxSignZero = 1;
	SfxnRamBanks = 4;
}

#undef STRUCT
#define STRUCT struct SGlobalSuperFX

static FreezeData SnapGlobalSuperFX [] = {
	#define GSFXFORMAT(t,v,r) INT_ENTRY(V1_RR_UNOFFICIAL, v),
		GLOBAL_SUPERFX_DEF
	#undef GSFXFORMAT
};

#undef GLOBAL_SUPERFX_DEF

#endif // ZSNES_FX





#undef STRUCT
#define STRUCT struct SRegisters

static FreezeData SnapRegisters [] = {
    INT_ENTRY(1, PB),
    INT_ENTRY(1, DB),
    INT_ENTRY(1, P.W),
    INT_ENTRY(1, A.W),
    INT_ENTRY(1, D.W),
    INT_ENTRY(1, S.W),
    INT_ENTRY(1, X.W),
    INT_ENTRY(1, Y.W),
    INT_ENTRY(1, PC),
};

#undef STRUCT
#define STRUCT struct SPPU

static FreezeData SnapPPU [] = {
    INT_ENTRY(1, BGMode),
    INT_ENTRY(1, BG3Priority),
    INT_ENTRY(1, Brightness),
    INT_ENTRY(1, VMA.High),
    INT_ENTRY(1, VMA.Increment),
    INT_ENTRY(1, VMA.Address),
    INT_ENTRY(1, VMA.Mask1),
    INT_ENTRY(1, VMA.FullGraphicCount),
    INT_ENTRY(1, VMA.Shift),
    INT_ENTRY(1, BG[0].SCBase),
    INT_ENTRY(1, BG[0].VOffset),
    INT_ENTRY(1, BG[0].HOffset),
    INT_ENTRY(1, BG[0].BGSize),
    INT_ENTRY(1, BG[0].NameBase),
    INT_ENTRY(1, BG[0].SCSize),

    INT_ENTRY(1, BG[1].SCBase),
    INT_ENTRY(1, BG[1].VOffset),
    INT_ENTRY(1, BG[1].HOffset),
    INT_ENTRY(1, BG[1].BGSize),
    INT_ENTRY(1, BG[1].NameBase),
    INT_ENTRY(1, BG[1].SCSize),

    INT_ENTRY(1, BG[2].SCBase),
    INT_ENTRY(1, BG[2].VOffset),
    INT_ENTRY(1, BG[2].HOffset),
    INT_ENTRY(1, BG[2].BGSize),
    INT_ENTRY(1, BG[2].NameBase),
    INT_ENTRY(1, BG[2].SCSize),

    INT_ENTRY(1, BG[3].SCBase),
    INT_ENTRY(1, BG[3].VOffset),
    INT_ENTRY(1, BG[3].HOffset),
    INT_ENTRY(1, BG[3].BGSize),
    INT_ENTRY(1, BG[3].NameBase),
    INT_ENTRY(1, BG[3].SCSize),

    INT_ENTRY(1, CGFLIP),
    ARRAY_ENTRY(1, CGDATA, 256, uint16_ARRAY_V),
    INT_ENTRY(1, FirstSprite),
#define O(N) \
    INT_ENTRY(1, OBJ[N].HPos), \
    INT_ENTRY(1, OBJ[N].VPos), \
    INT_ENTRY(1, OBJ[N].Name), \
    INT_ENTRY(1, OBJ[N].VFlip), \
    INT_ENTRY(1, OBJ[N].HFlip), \
    INT_ENTRY(1, OBJ[N].Priority), \
    INT_ENTRY(1, OBJ[N].Palette), \
    INT_ENTRY(1, OBJ[N].Size)

    O(  0), O(  1), O(  2), O(  3), O(  4), O(  5), O(  6), O(  7),
    O(  8), O(  9), O( 10), O( 11), O( 12), O( 13), O( 14), O( 15),
    O( 16), O( 17), O( 18), O( 19), O( 20), O( 21), O( 22), O( 23),
    O( 24), O( 25), O( 26), O( 27), O( 28), O( 29), O( 30), O( 31),
    O( 32), O( 33), O( 34), O( 35), O( 36), O( 37), O( 38), O( 39),
    O( 40), O( 41), O( 42), O( 43), O( 44), O( 45), O( 46), O( 47),
    O( 48), O( 49), O( 50), O( 51), O( 52), O( 53), O( 54), O( 55),
    O( 56), O( 57), O( 58), O( 59), O( 60), O( 61), O( 62), O( 63),
    O( 64), O( 65), O( 66), O( 67), O( 68), O( 69), O( 70), O( 71),
    O( 72), O( 73), O( 74), O( 75), O( 76), O( 77), O( 78), O( 79),
    O( 80), O( 81), O( 82), O( 83), O( 84), O( 85), O( 86), O( 87),
    O( 88), O( 89), O( 90), O( 91), O( 92), O( 93), O( 94), O( 95),
    O( 96), O( 97), O( 98), O( 99), O(100), O(101), O(102), O(103),
    O(104), O(105), O(106), O(107), O(108), O(109), O(110), O(111),
    O(112), O(113), O(114), O(115), O(116), O(117), O(118), O(119),
    O(120), O(121), O(122), O(123), O(124), O(125), O(126), O(127),
#undef O
    INT_ENTRY(1, OAMPriorityRotation),
    INT_ENTRY(1, OAMAddr),
    INT_ENTRY(1, OAMFlip),
    INT_ENTRY(1, OAMTileAddress),
    INT_ENTRY(1, IRQVBeamPos),
    INT_ENTRY(1, IRQHBeamPos),
    INT_ENTRY(1, VBeamPosLatched),
    INT_ENTRY(1, HBeamPosLatched),
    INT_ENTRY(1, HBeamFlip),
    INT_ENTRY(1, VBeamFlip),
    INT_ENTRY(1, HVBeamCounterLatched),
    INT_ENTRY(1, MatrixA),
    INT_ENTRY(1, MatrixB),
    INT_ENTRY(1, MatrixC),
    INT_ENTRY(1, MatrixD),
    INT_ENTRY(1, CentreX),
    INT_ENTRY(1, CentreY),
	INT_ENTRY(1, Joypad1ButtonReadPos),
	INT_ENTRY(1, Joypad2ButtonReadPos),
	INT_ENTRY(1, Joypad3ButtonReadPos),
    INT_ENTRY(1, CGADD),
    INT_ENTRY(1, FixedColourRed),
    INT_ENTRY(1, FixedColourGreen),
    INT_ENTRY(1, FixedColourBlue),
    INT_ENTRY(1, SavedOAMAddr),
    INT_ENTRY(1, ScreenHeight),
    INT_ENTRY(1, WRAM),
    INT_ENTRY(1, ForcedBlanking),
    INT_ENTRY(1, OBJNameSelect),
    INT_ENTRY(1, OBJSizeSelect),
    INT_ENTRY(1, OBJNameBase),
    INT_ENTRY(1, OAMReadFlip),
    INT_ENTRY(1, VTimerEnabled),
    INT_ENTRY(1, HTimerEnabled),
    INT_ENTRY(1, HTimerPosition),
    INT_ENTRY(1, Mosaic),
    INT_ENTRY(1, Mode7HFlip),
    INT_ENTRY(1, Mode7VFlip),
    INT_ENTRY(1, Mode7Repeat),
    INT_ENTRY(1, Window1Left),
    INT_ENTRY(1, Window1Right),
    INT_ENTRY(1, Window2Left),
    INT_ENTRY(1, Window2Right),
#define O(N) \
    INT_ENTRY(1, ClipWindowOverlapLogic[N]), \
    INT_ENTRY(1, ClipWindow1Enable[N]), \
    INT_ENTRY(1, ClipWindow2Enable[N]), \
    INT_ENTRY(1, ClipWindow1Inside[N]), \
    INT_ENTRY(1, ClipWindow2Inside[N])

    O(0), O(1), O(2), O(3), O(4), O(5),

#undef O
    INT_ENTRY(1, CGFLIPRead),
    INT_ENTRY(1, Need16x8Mulitply),
    ARRAY_ENTRY(1, BGMosaic, 4, uint8_ARRAY_V),
    ARRAY_ENTRY(1, OAMData, 512 + 32, uint8_ARRAY_V),
    ARRAY_ENTRY(1, MouseSpeed, 2, uint8_ARRAY_V),
    INT_ENTRY(1, OAMWriteRegister), // ZELDA FIX
    INT_ENTRY(1, BGnxOFSbyte), // ZELDA FIX
    INT_ENTRY(1, OpenBus1), // ZELDA FIX
    INT_ENTRY(1, OpenBus2), // ZELDA FIX

	// not sure if the following are necessary, but better safe than sorry
    INT_ENTRY(V1_RR_UNOFFICIAL, OBJThroughMain),
    INT_ENTRY(V1_RR_UNOFFICIAL, OBJThroughSub),
    INT_ENTRY(V1_RR_UNOFFICIAL, OBJAddition),
#define O(N) \
    INT_ENTRY(V1_RR_UNOFFICIAL, ClipCounts[N])
    O(0), O(1), O(2), O(3), O(4), O(5),
#undef O
    INT_ENTRY(V1_RR_UNOFFICIAL, HDMA),
    INT_ENTRY(V1_RR_UNOFFICIAL, HDMAStarted),
    INT_ENTRY(V1_RR_UNOFFICIAL, RangeTimeOver),
    //INT_ENTRY(V1_RR_UNOFFICIAL, RecomputeClipWindows), // actually shouldn't be saved, since it ends up differently depending on the frameskip settings and only affects the IPPU
};

#undef STRUCT
#define STRUCT struct SDMA

static FreezeData SnapDMA [] = {
#define O(N) \
    {OFFSET (TransferDirection) + N * sizeof (struct SDMA),0, 1, INT_V, 1, 9999, "TransferDirection"}, \
    {OFFSET (AAddressFixed) + N * sizeof (struct SDMA),0,     1, INT_V, 1, 9999, "AAddressFixed"}, \
    {OFFSET (AAddressDecrement) + N * sizeof (struct SDMA),0, 1, INT_V, 1, 9999, "AAddressDecrement"}, \
    {OFFSET (TransferMode) + N * sizeof (struct SDMA),0,      1, INT_V, 1, 9999, "TransferMode"}, \
    {OFFSET (ABank) + N * sizeof (struct SDMA),0,             1, INT_V, 1, 9999, "ABank"}, \
    {OFFSET (AAddress) + N * sizeof (struct SDMA),0,          2, INT_V, 1, 9999, "AAddress"}, \
    {OFFSET (Address) + N * sizeof (struct SDMA),0,           2, INT_V, 1, 9999, "Address"}, \
    {OFFSET (BAddress) + N * sizeof (struct SDMA),0,          1, INT_V, 1, 9999, "BAddress"}, \
    {OFFSET (TransferBytes) + N * sizeof (struct SDMA),0,     2, INT_V, 1, 2, "TransferBytes"}, \
    {OFFSET (HDMAIndirectAddressing) + N * sizeof (struct SDMA),0, 1, INT_V, 1, 9999, "HDMAIndirectAddressing"}, \
    {OFFSET (IndirectAddress) + N * sizeof (struct SDMA),0,   2, INT_V, 1, 9999, "IndirectAddress"}, \
    {OFFSET (IndirectBank) + N * sizeof (struct SDMA),0,      1, INT_V, 1, 9999, "IndirectBank"}, \
    {OFFSET (Repeat) + N * sizeof (struct SDMA),0,            1, INT_V, 1, 9999, "Repeat"}, \
    {OFFSET (LineCount) + N * sizeof (struct SDMA),0,         1, INT_V, 1, 9999, "LineCount"}, \
    {OFFSET (FirstLine) + N * sizeof (struct SDMA),0,         1, INT_V, 1, 9999, "FirstLine"}

    O(0), O(1), O(2), O(3), O(4), O(5), O(6), O(7)
#undef O
};

#undef STRUCT
#define STRUCT struct SAPU

static FreezeData SnapAPU [] = {
    INT_ENTRY(1, Cycles),
    INT_ENTRY(1, ShowROM),
    INT_ENTRY(1, Flags),
    INT_ENTRY(1, KeyedChannels),
    ARRAY_ENTRY(1, OutPorts, 4, uint8_ARRAY_V),
    ARRAY_ENTRY(1, DSP, 0x80, uint8_ARRAY_V),
    ARRAY_ENTRY(1, ExtraRAM, 64, uint8_ARRAY_V),
    ARRAY_ENTRY(1, Timer, 3, uint16_ARRAY_V),
    ARRAY_ENTRY(1, TimerTarget, 3, uint16_ARRAY_V),
    ARRAY_ENTRY(1, TimerEnabled, 3, uint8_ARRAY_V),
    ARRAY_ENTRY(1, TimerValueWritten, 3, uint8_ARRAY_V),

	// not sure if the following are necessary, but better safe than sorry
    INT_ENTRY(V1_RR_UNOFFICIAL, NextAPUTimerPos),
    INT_ENTRY(V1_RR_UNOFFICIAL, APUTimerCounter),
};

#undef STRUCT
#define STRUCT struct SIAPU

static FreezeData SnapIAPU [] = {
	// not sure if the following are necessary (actually I'm pretty sure some of them are), but better safe than sorry
    INT_ENTRY(V1_RR_UNOFFICIAL, APUExecuting),
    INT_ENTRY(V1_RR_UNOFFICIAL, Bit),
    INT_ENTRY(V1_RR_UNOFFICIAL, Address),
    INT_ENTRY(V1_RR_UNOFFICIAL, WaitCounter),
    INT_ENTRY(V1_RR_UNOFFICIAL, _Carry),
    INT_ENTRY(V1_RR_UNOFFICIAL, _Zero),
    INT_ENTRY(V1_RR_UNOFFICIAL, _Overflow),
    INT_ENTRY(V1_RR_UNOFFICIAL, TimerErrorCounter),
    INT_ENTRY(V1_RR_UNOFFICIAL, Scanline),
    INT_ENTRY(V1_RR_UNOFFICIAL, OneCycle),
    INT_ENTRY(V1_RR_UNOFFICIAL, TwoCycles),
    POINTER_ENTRY(V1_RR_UNOFFICIAL, PC, RAM),
    POINTER_ENTRY(V1_RR_UNOFFICIAL, DirectPage, RAM),
    POINTER_ENTRY(V1_RR_UNOFFICIAL, WaitAddress1, RAM),
    POINTER_ENTRY(V1_RR_UNOFFICIAL, WaitAddress2, RAM),
};

#undef STRUCT
#define STRUCT struct SAPURegisters

static FreezeData SnapAPURegisters [] = {
    INT_ENTRY(1, P),
    INT_ENTRY(1, YA.W),
    INT_ENTRY(1, X),
    INT_ENTRY(1, S),
    INT_ENTRY(1, PC),
};

#undef STRUCT
#define STRUCT SSoundData

static FreezeData SnapSoundData [] = {
    INT_ENTRY(1, master_volume_left),
    INT_ENTRY(1, master_volume_right),
    INT_ENTRY(1, echo_volume_left),
    INT_ENTRY(1, echo_volume_right),
    INT_ENTRY(1, echo_enable),
    INT_ENTRY(1, echo_feedback),
    INT_ENTRY(1, echo_ptr),
    INT_ENTRY(1, echo_buffer_size),
    INT_ENTRY(1, echo_write_enabled),
    INT_ENTRY(1, echo_channel_enable),
    INT_ENTRY(1, pitch_mod),
    ARRAY_ENTRY(1, dummy, 3, uint32_ARRAY_V),
#define O(N) \
    INT_ENTRY(1, channels [N].state), \
    INT_ENTRY(1, channels [N].type), \
    INT_ENTRY(1, channels [N].volume_left), \
    INT_ENTRY(1, channels [N].volume_right), \
    INT_ENTRY(1, channels [N].hertz), \
    INT_ENTRY(1, channels [N].count), \
    INT_ENTRY(1, channels [N].loop), \
    INT_ENTRY(1, channels [N].envx), \
    INT_ENTRY(1, channels [N].left_vol_level), \
    INT_ENTRY(1, channels [N].right_vol_level), \
    INT_ENTRY(1, channels [N].envx_target), \
    INT_ENTRY(1, channels [N].env_error), \
    INT_ENTRY(1, channels [N].erate), \
    INT_ENTRY(1, channels [N].direction), \
    INT_ENTRY(1, channels [N].attack_rate), \
    INT_ENTRY(1, channels [N].decay_rate), \
    INT_ENTRY(1, channels [N].sustain_rate), \
    INT_ENTRY(1, channels [N].release_rate), \
    INT_ENTRY(1, channels [N].sustain_level), \
    INT_ENTRY(1, channels [N].sample), \
    ARRAY_ENTRY(1, channels [N].decoded, 16, uint16_ARRAY_V), \
    ARRAY_ENTRY(1, channels [N].previous16, 2, uint16_ARRAY_V), \
    INT_ENTRY(1, channels [N].sample_number), \
    INT_ENTRY(1, channels [N].last_block), \
    INT_ENTRY(1, channels [N].needs_decode), \
    INT_ENTRY(1, channels [N].block_pointer), \
    INT_ENTRY(1, channels [N].sample_pointer), \
    INT_ENTRY(1, channels [N].mode)

    O(0), O(1), O(2), O(3), O(4), O(5), O(6), O(7),
#undef O
};

#undef STRUCT
#define STRUCT struct SSA1Registers

static FreezeData SnapSA1Registers [] = {
    INT_ENTRY(1, PB),
    INT_ENTRY(1, DB),
    INT_ENTRY(1, P.W),
    INT_ENTRY(1, A.W),
    INT_ENTRY(1, D.W),
    INT_ENTRY(1, S.W),
    INT_ENTRY(1, X.W),
    INT_ENTRY(1, Y.W),
    INT_ENTRY(1, PC),
};

#undef STRUCT
#define STRUCT struct SSA1

static FreezeData SnapSA1 [] = {
    INT_ENTRY(1, Flags),
    INT_ENTRY(1, NMIActive),
    INT_ENTRY(1, IRQActive),
    INT_ENTRY(1, WaitingForInterrupt),
    INT_ENTRY(1, op1),
    INT_ENTRY(1, op2),
    INT_ENTRY(1, arithmetic_op),
    INT_ENTRY(1, sum),
    INT_ENTRY(1, overflow),

	// not sure if the following are necessary, but better safe than sorry
    INT_ENTRY(V1_RR_UNOFFICIAL, CPUExecuting),
    INT_ENTRY(V1_RR_UNOFFICIAL, ShiftedPB),
    INT_ENTRY(V1_RR_UNOFFICIAL, ShiftedDB),
    INT_ENTRY(V1_RR_UNOFFICIAL, Executing),
    INT_ENTRY(V1_RR_UNOFFICIAL, Waiting),
    INT_ENTRY(V1_RR_UNOFFICIAL, WaitCounter),
    INT_ENTRY(V1_RR_UNOFFICIAL, VirtualBitmapFormat),
    INT_ENTRY(V1_RR_UNOFFICIAL, in_char_dma),
    INT_ENTRY(V1_RR_UNOFFICIAL, variable_bit_pos),
	INT_ENTRY(V1_RR_UNOFFICIAL, _Carry),
	INT_ENTRY(V1_RR_UNOFFICIAL, _Zero),
	INT_ENTRY(V1_RR_UNOFFICIAL, _Negative),
	INT_ENTRY(V1_RR_UNOFFICIAL, _Overflow),
    MEMPOINTER_ENTRY(V1_RR_UNOFFICIAL, PC),
    MEMPOINTER_ENTRY(V1_RR_UNOFFICIAL, PCAtOpcodeStart),
    MEMPOINTER_ENTRY(V1_RR_UNOFFICIAL, WaitAddress),
    MEMPOINTER_ENTRY(V1_RR_UNOFFICIAL, PCBase),
};

#undef STRUCT
#define STRUCT struct SDSP1

static FreezeData SnapDSP1 [] = {
	INT_ENTRY(1, version),
	INT_ENTRY(1, waiting4command),
	INT_ENTRY(1, first_parameter),
	INT_ENTRY(1, command),
	INT_ENTRY(1, in_count),
	INT_ENTRY(1, in_index),
	INT_ENTRY(1, out_count),
	INT_ENTRY(1, out_index),
	ARRAY_ENTRY(1, parameters, 512, uint8_ARRAY_V),
	ARRAY_ENTRY(1, output, 512, uint8_ARRAY_V),
	ARRAY_ENTRY(V1_RR_UNOFFICIAL, temp_save_data, sizeof(DSP1.temp_save_data), uint8_ARRAY_V),
};

#undef STRUCT
#define STRUCT struct SPC7110EmuVars

static FreezeData SnapSPC7110 [] = {
    INT_ENTRY(1, reg4800),
    INT_ENTRY(1, reg4801),
    INT_ENTRY(1, reg4802),
    INT_ENTRY(1, reg4803),
    INT_ENTRY(1, reg4804),
    INT_ENTRY(1, reg4805),
    INT_ENTRY(1, reg4806),
    INT_ENTRY(1, reg4807),
    INT_ENTRY(1, reg4808),
    INT_ENTRY(1, reg4809),
    INT_ENTRY(1, reg480A),
    INT_ENTRY(1, reg480B),
    INT_ENTRY(1, reg480C),
    INT_ENTRY(1, reg4811),
    INT_ENTRY(1, reg4812),
    INT_ENTRY(1, reg4813),
    INT_ENTRY(1, reg4814),
    INT_ENTRY(1, reg4815),
    INT_ENTRY(1, reg4816),
    INT_ENTRY(1, reg4817),
    INT_ENTRY(1, reg4818),
    INT_ENTRY(1, reg4820),
    INT_ENTRY(1, reg4821),
    INT_ENTRY(1, reg4822),
    INT_ENTRY(1, reg4823),
    INT_ENTRY(1, reg4824),
    INT_ENTRY(1, reg4825),
    INT_ENTRY(1, reg4826),
    INT_ENTRY(1, reg4827),
    INT_ENTRY(1, reg4828),
    INT_ENTRY(1, reg4829),
    INT_ENTRY(1, reg482A),
    INT_ENTRY(1, reg482B),
    INT_ENTRY(1, reg482C),
    INT_ENTRY(1, reg482D),
    INT_ENTRY(1, reg482E),
    INT_ENTRY(1, reg482F),
    INT_ENTRY(1, reg4830),
    INT_ENTRY(1, reg4831),
    INT_ENTRY(1, reg4832),
    INT_ENTRY(1, reg4833),
    INT_ENTRY(1, reg4834),
    INT_ENTRY(1, reg4840),
    INT_ENTRY(1, reg4841),
    INT_ENTRY(1, reg4842),
    INT_ENTRY(1, AlignBy),
    INT_ENTRY(1, written),
    INT_ENTRY(1, offset_add),
    INT_ENTRY(1, DataRomOffset),
    INT_ENTRY(1, DataRomSize),
    INT_ENTRY(1, bank50Internal),
    ARRAY_ENTRY(1, bank50, 0x10000, uint8_ARRAY_V),
};

#undef STRUCT
#define STRUCT struct SPC7110RTC

static FreezeData SnapS7RTC [] = {
    ARRAY_ENTRY(1, reg, 16, uint8_ARRAY_V),
    INT_ENTRY(1, index),
    INT_ENTRY(1, control),
    INT_ENTRY(1, init),
    INT_ENTRY(1, last_used),
};

#undef STRUCT
#define STRUCT struct InternalPPU

static FreezeData SnapIPPU [] = {
    ARRAY_ENTRY(1, Joypads, 5, uint32_ARRAY_V),
    INT_ENTRY(1, SuperScope),
    ARRAY_ENTRY(1, Mouse, 2, uint32_ARRAY_V),
    ARRAY_ENTRY(1, PrevMouseX, 2, uint32_ARRAY_V),
    ARRAY_ENTRY(1, PrevMouseY, 2, uint32_ARRAY_V),

    INT_ENTRY(V1_RR_UNOFFICIAL, pad_read),
    INT_ENTRY(V1_RR_UNOFFICIAL, pad_read_last),
    INT_ENTRY(V1_RR_UNOFFICIAL, TotalEmulatedFrames),
    INT_ENTRY(V1_RR_UNOFFICIAL, LagCounter),
};

#ifndef NEW_SNAPSHOT_SCREENSHOT

#undef STRUCT
#define STRUCT struct SGFX

static FreezeData SnapGFX [] = {
	ARRAY_ENTRY(1, FrameDisplayString, 256, uint8_ARRAY_V),
	ARRAY_ENTRY(1, LastScreen, MAX_SNES_WIDTH * MAX_SNES_HEIGHT * 4, uint8_ARRAY_V),
};

#else

// deleted blocks
static FreezeData SnapGFX [] = {
	DELETED_ARRAY_ENTRY(3,4, Junk, 22+256+MAX_SNES_WIDTH*MAX_SNES_HEIGHT*2, uint8_ARRAY_V),
};

struct SnapshotScreenshotInfo
{
	uint16 Width;
	uint16 Height;
	uint8 Data [MAX_SNES_WIDTH * MAX_SNES_HEIGHT * 3];
//	uint8 Interlaced;
};

#undef STRUCT
#define STRUCT struct SnapshotScreenshotInfo

static FreezeData SnapScreenshot [] = {
	INT_ENTRY(1, Width),
	INT_ENTRY(1, Height),
	ARRAY_ENTRY(1, Data, MAX_SNES_WIDTH * MAX_SNES_HEIGHT * 3, uint8_ARRAY_V),
//	INT_ENTRY(1, Interlaced), // needed in case interlacing was on before loading a state where it is off
};

#endif // !NEW_SNAPSHOT_SCREENSHOT

static char ROMFilename [_MAX_PATH];
//static char SnapshotFilename [_MAX_PATH];

void FreezeStructF (STREAM stream, char *name, void *base, FreezeData *fields,
				   int num_fields);
void FreezeBlockF (STREAM stream, char *name, uint8 *block, int size);

int UnfreezeStruct (STREAM stream, char *name, void *base, FreezeData *fields,
					int num_fields, int version);
int UnfreezeBlock (STREAM stream, char *name, uint8 *block, int size);

int UnfreezeStructCopy (STREAM stream, char *name, uint8** block, FreezeData *fields, int num_fields, int version);

void UnfreezeStructFromCopy (void *base, FreezeData *fields, int num_fields, uint8* block, int version);

int UnfreezeBlockCopy (STREAM stream, char *name, uint8** block, int size);

bool8 Snapshot (const char *filename)
{
    return (S9xFreezeGame (filename));
}

bool8 S9xFreezeGame (const char *filename)
{
    char def [PATH_MAX + 1 ];
    char drive [_MAX_DRIVE + 1];
    char dir [_MAX_DIR + 1 ];
    char ext [_MAX_EXT + 1 ] ;

    _splitpath (filename, drive, dir, def, ext);
	int stateNumber = -2;
	sscanf(ext, ".%03d", &stateNumber);
	stateNumber++;
	// call savestate.save callback if any and store the results in a luasav file if any
#ifdef SNAPSHOT_VERIFY_SUPPORTED
	if(!s_verifyingSnapshot)
#endif
	{
		LuaSaveData saveData;
		CallRegisteredLuaSaveFunctions(stateNumber, saveData);

		char luaSaveFilename [512];
		strncpy(luaSaveFilename, filename, 512);
		luaSaveFilename[512-(1+7/*strlen(".luasav")*/)] = '\0';
		strcat(luaSaveFilename, ".luasav");
		if(saveData.recordList)
		{
			FILE* luaSaveFile = fopen(luaSaveFilename, "wb");
			if(luaSaveFile)
			{
				saveData.ExportRecords(luaSaveFile);
				fclose(luaSaveFile);
			}
		}
		else
		{
			unlink(luaSaveFilename);
		}
	}


#ifndef NEW_SNAPSHOT_SCREENSHOT
	// backup screen for snapshot screenshot
	{
		uint16 Height = min(IPPU.RenderedScreenHeight, MAX_SNES_HEIGHT);

		memset(GFX.LastScreen, 0, MAX_SNES_WIDTH * MAX_SNES_HEIGHT * 4);
		for (int y = 0; y < Height; y++)
			memcpy((uint8*)GFX.LastScreen + y * GFX.RealPitch, (uint8*)GFX.Screen + y * GFX.RealPitch, GFX.RealPitch);
	}
#endif // !NEW_SNAPSHOT_SCREENSHOT

	STREAM stream = NULL;

#ifdef SNAPSHOT_VERIFY_SUPPORTED
	BOOL openForRead = s_verifyingSnapshot;
#else
	BOOL openForRead = FALSE;
#endif

	if (S9xOpenSnapshotFile (filename, openForRead, &stream))
    {
		#ifdef WIN32
		EnterCriticalSection(&GUI.SoundCritSect);
		#endif

		S9xPrepareSoundForSnapshotSave (FALSE);

		S9xFreezeToStream (stream);
		S9xCloseSnapshotFile (stream);

		#ifdef WIN32 // S9xFreezePlatformDepends
			S9xFreezePlatformDepends (filename);
		#endif // !S9xFreezePlatformDepends

		S9xPrepareSoundForSnapshotSave (TRUE);

		S9xResetSaveTimer (TRUE);

		#ifdef WIN32
		LeaveCriticalSection(&GUI.SoundCritSect);
		#endif

		if(S9xMovieActive())
		{
			const char * name = S9xBasename (filename);
			if(name && strlen(name) > 3)
				name += strlen(name) - 3;
			else
				name = filename;
			sprintf(String, MOVIE_INFO_SNAPSHOT " %s", name);
			S9xMessage (S9X_INFO, S9X_FREEZE_FILE_INFO, String);
		    GFX.InfoStringTimeout /= 4;
		}
		else
		{
			sprintf(String, SAVE_INFO_SNAPSHOT " %s", S9xBasename (filename));
			S9xMessage (S9X_INFO, S9X_FREEZE_FILE_INFO, String);
		}

		return (TRUE);
    }
    return (FALSE);
}

bool8 S9xLoadSnapshot (const char *filename)
{
    return (S9xUnfreezeGame (filename));
}

bool8 S9xUnfreezeGame (const char *filename)
{
    char def [PATH_MAX + 1 ];
    char drive [_MAX_DRIVE + 1];
    char dir [_MAX_DIR + 1 ];
    char ext [_MAX_EXT + 1 ] ;

    _splitpath (filename, drive, dir, def, ext);
    S9xResetSaveTimer (!strcmp(ext, "oops") || !strcmp(ext, "oop"));

	// this is so we don't mix the previous frame's gui drawing with any drawing that the savestate triggers
	S9xLuaEnableGui(false);
	// this is so we don't refresh the screen too early before the lua code knows that we loaded a savestate (since it can't know what to draw if that happens)
	extern bool disableMessageImmediateRefresh;
	bool prev_disableMessageImmediateRefresh = disableMessageImmediateRefresh;
	disableMessageImmediateRefresh = true;

    ZeroMemory (&Obsolete, sizeof(Obsolete));
    
    STREAM snapshot = NULL;

    if (S9xLoadOrigSnapshot (filename))
		goto successFinish;
	
    if (S9xUnfreezeZSNES (filename))
		goto successFinish;
	
    if (S9xOpenSnapshotFile (filename, TRUE, &snapshot))
    {
		int result;
		if ((result = S9xUnfreezeFromStream (snapshot)) != SUCCESS)
		{
			switch (result)
			{
			case WRONG_FORMAT:
				S9xMessage (S9X_ERROR, S9X_WRONG_FORMAT, SAVE_ERR_WRONG_FORMAT);
				break;
			case WRONG_VERSION:
				S9xMessage (S9X_ERROR, S9X_WRONG_VERSION, SAVE_ERR_WRONG_VERSION);
				break;
			case WRONG_MOVIE_SNAPSHOT:
				S9xMessage (S9X_ERROR, S9X_WRONG_MOVIE_SNAPSHOT, MOVIE_ERR_SNAPSHOT_WRONG_MOVIE);
				break;
			case NOT_A_MOVIE_SNAPSHOT:
				S9xMessage (S9X_ERROR, S9X_NOT_A_MOVIE_SNAPSHOT, MOVIE_ERR_SNAPSHOT_NOT_MOVIE);
				break;
			case SNAPSHOT_INCONSISTENT:
				S9xMessage (S9X_ERROR, S9X_SNAPSHOT_INCONSISTENT, MOVIE_ERR_SNAPSHOT_INCONSISTENT);
				break;
			default:
			case FILE_NOT_FOUND:
				sprintf (String, SAVE_ERR_ROM_NOT_FOUND, ROMFilename);
				S9xMessage (S9X_ERROR, S9X_ROM_NOT_FOUND, String);
				break;
			}
			S9xCloseSnapshotFile (snapshot);
			goto failFinish;
		}

		if(S9xMovieActive())
		{
			const char * name = S9xBasename (filename);
			if(name && strlen(name) > 3)
				name += strlen(name) - 3;
			else
				name = filename;
			if(S9xMovieReadOnly())
				sprintf(String, MOVIE_INFO_REWIND " %s", name);
			else
				sprintf(String, MOVIE_INFO_RERECORD " %s", name);
			S9xMessage (S9X_INFO, S9X_FREEZE_FILE_INFO, String);
		    GFX.InfoStringTimeout /= 4;
		}
		else
		{
			sprintf(String, SAVE_INFO_LOAD " %s", S9xBasename (filename));
			S9xMessage (S9X_INFO, S9X_FREEZE_FILE_INFO, String);
		}

		S9xCloseSnapshotFile (snapshot);

		#ifdef WIN32 // S9xUnfreezePlatformDepends
			S9xUnfreezePlatformDepends (filename);
		#endif // !S9xUnfreezePlatformDepends

		goto successFinish;
    }

	// failed; error message:
	{
		char name [PATH_MAX];
		strcpy(name, S9xBasename (filename));
		int len = strlen(name);
		if(len > 3 && name[len-3] == 'z' && name[len-2] == 's')
			name[len-3] = name[len-2] = '0';
		sprintf(String, SAVE_ERR_SAVE_NOT_FOUND, name);
		S9xMessage (S9X_INFO, S9X_FREEZE_FILE_INFO, String);
	}

failFinish:

	S9xLuaClearGui(); // (needs to be here)
	S9xLuaEnableGui(true);
	disableMessageImmediateRefresh = prev_disableMessageImmediateRefresh;

	// refresh has to happen now to at least show the error message since we disabled it happening earlier
#ifdef WIN32
	void S9xReRefresh ();
	S9xReRefresh();
#endif

    return (FALSE);

successFinish:

	S9xLuaClearGui(); // (needs to be here)
	S9xLuaEnableGui(true);
	disableMessageImmediateRefresh = prev_disableMessageImmediateRefresh;

	int stateNumber = -2;
	sscanf(ext, ".%03d", &stateNumber);
	stateNumber++;
	// call savestate.registerload callback if any, and pass it the result from the previous savestate.registerload callback to the same state if any
#ifdef SNAPSHOT_VERIFY_SUPPORTED
	if(!s_verifyingSnapshot)
#endif
	{
		LuaSaveData saveData;

		char luaSaveFilename [512];
		strncpy(luaSaveFilename, filename, 512);
		luaSaveFilename[512-(1+7/*strlen(".luasav")*/)] = '\0';
		strcat(luaSaveFilename, ".luasav");
		FILE* luaSaveFile = fopen(luaSaveFilename, "rb");
		if(luaSaveFile)
		{
			saveData.ImportRecords(luaSaveFile);
			fclose(luaSaveFile);
		}

		CallRegisteredLuaLoadFunctions(stateNumber, saveData);
	}
	// the refresh should happen last and only last, because lua code can't know what to draw in the gui if the refresh happens before savestate.registerload callbacks get called
#ifdef WIN32
	void S9xReRefresh ();
	S9xReRefresh();
#endif

	return (TRUE);
}

bool diagnostic_freezing = false;
//#define DIAGNOSTIC_FREEZING_SUPPORT // obsoleted by SNAPSHOT_VERIFY_SUPPORTED

bool freezing_to_stream = false;

void S9xFreezeToStream (STREAM stream)
{
    char buffer [1024];
    int i;

	freezing_to_stream = true;

	#ifdef WIN32
	EnterCriticalSection(&GUI.SoundCritSect);
	#endif

    S9xSetSoundMute (TRUE);
//#ifdef ZSNES_FX
//    if (Settings.SuperFX)
//		S9xSuperFXPreSaveState ();
//#endif
	
	S9xUpdateRTC();
    S9xSRTCPreSaveState ();
	
    for (i = 0; i < 8; i++)
    {
		SoundData.channels [i].previous16 [0] = (int16) SoundData.channels [i].previous [0];
		SoundData.channels [i].previous16 [1] = (int16) SoundData.channels [i].previous [1];
    }
	sprintf (buffer, "%s:%04d\n", SNAPSHOT_MAGIC, diagnostic_freezing ? 9999 : SNAPSHOT_VERSION);
    WRITE_STREAM (buffer, strlen (buffer), stream);
    sprintf (buffer, "NAM:%06d:%s%c", (int)strlen (Memory.ROMFilename) + 1,
		Memory.ROMFilename, 0);
    WRITE_STREAM (buffer, strlen (buffer) + 1, stream);
    FreezeStruct (stream, "CPU", &CPU, SnapCPU, COUNT (SnapCPU));
    FreezeStruct (stream, "REG", &Registers, SnapRegisters, COUNT (SnapRegisters));
    FreezeStruct (stream, "PPU", &PPU, SnapPPU, COUNT (SnapPPU));
    FreezeStruct (stream, "DMA", DMA, SnapDMA, COUNT (SnapDMA));
    FreezeStruct (stream, "ICP", &ICPU, SnapICPU, COUNT (SnapICPU));

	// RAM and VRAM
    FreezeBlock (stream, "VRA", Memory.VRAM, 0x10000);
    FreezeBlock (stream, "RAM", Memory.RAM, 0x20000);
    FreezeBlock (stream, "SRA", Memory.SRAM, 0x20000);
    FreezeBlock (stream, "FIL", Memory.FillRAM, 0x8000);
    if (Settings.APUEnabled)
    {
		// APU
		FreezeStruct (stream, "APU", &APU, SnapAPU, COUNT (SnapAPU));
		FreezeStruct (stream, "ARE", &APURegisters, SnapAPURegisters,
			COUNT (SnapAPURegisters));
		FreezeBlock (stream, "ARA", IAPU.RAM, 0x10000);
		FreezeStruct (stream, "SOU", &SoundData, SnapSoundData,
			COUNT (SnapSoundData));
		FreezeStruct (stream, "IAP", &IAPU, SnapIAPU, COUNT (SnapIAPU));
    }

	{
		SGlobalJunk globalJunk = GetCurrentGlobalJunk();
		FreezeStruct (stream, "GBJ", &globalJunk, SnapGlobalJunk, COUNT(SnapGlobalJunk));
	}

    // Special chips
    if (Settings.SA1)
    {
		SA1Registers.PC = SA1.PC - SA1.PCBase;
		S9xSA1PackStatus ();
		FreezeStruct (stream, "SA1", &SA1, SnapSA1, COUNT (SnapSA1));
		FreezeStruct (stream, "SAR", &SA1Registers, SnapSA1Registers, 
			COUNT (SnapSA1Registers));
    }

#ifdef ZSNES_FX
	if(Settings.SuperFX)
	{
		SGlobalSuperFX globalSuperFX = GetCurrentGlobalSuperFX();
		FreezeStruct (stream, "GSF", &globalSuperFX, SnapGlobalSuperFX, COUNT(SnapGlobalSuperFX));
	}
#endif

	if (Settings.SPC7110)
    {
		FreezeStruct (stream, "SP7", &s7r, SnapSPC7110, COUNT (SnapSPC7110));
    }

	if (Settings.SPC7110RTC)
	{
		FreezeStruct (stream, "RTC", &rtc_f9, SnapS7RTC, COUNT (SnapS7RTC));
	}

	if (S9xMovieActive ())
	{
		uint8* movie_freeze_buf;
		uint32 movie_freeze_size;

		S9xMovieFreeze(&movie_freeze_buf, &movie_freeze_size);
		if(movie_freeze_buf)
		{
			struct SnapshotMovieInfo mi;
			mi.MovieInputDataSize = movie_freeze_size;
			FreezeStruct (stream, "MOV", &mi, SnapMovie, COUNT (SnapMovie));
		    FreezeBlock (stream, "MID", movie_freeze_buf, movie_freeze_size);
			delete [] movie_freeze_buf;
		}
	}

	// DSP1 chip
	if(Settings.DSP1Master)
	{
		S9xPreSaveDSP1();
		FreezeStruct (stream, "DSP", &DSP1, SnapDSP1, COUNT (SnapDSP1));
	}

	if (Settings.C4)
	{
		FreezeBlock (stream, "CX4", Memory.C4RAM, 8192);
	}

	FreezeStruct (stream, "IPU", &IPPU, SnapIPPU, COUNT (SnapIPPU));

#ifndef NEW_SNAPSHOT_SCREENSHOT
	FreezeStruct (stream, "GFX", &GFX, SnapGFX, COUNT (SnapGFX));
#else
	if(Settings.SnapshotScreenshots)
	{
		SnapshotScreenshotInfo *ssi = new SnapshotScreenshotInfo;
		ssi->Width = min(IPPU.RenderedScreenWidth, MAX_SNES_WIDTH);
		ssi->Height = min(IPPU.RenderedScreenHeight, MAX_SNES_HEIGHT);
//		ssi->Interlaced = GFX.DoInterlace;

		uint8 *rowpix=ssi->Data;
		uint16 *screen=(uint16*)GFX.Screen;
		uint32 RealPPL=GFX.Pitch2/2;
		for(int y=0; y<ssi->Height; y++, screen+=RealPPL){
			for(int x=0; x<ssi->Width; x++){
				uint32 r, g, b;
				DECOMPOSE_PIXEL(screen[x], r, g, b);
				*(rowpix++) = r; // save pixel as 15-bits-in-3-bytes, for simplicity
				*(rowpix++) = g;
				*(rowpix++) = b;
			}
		}
		memset(rowpix, 0, sizeof(ssi->Data) + ssi->Data - rowpix);

	 	FreezeStruct (stream, "SHO", ssi, SnapScreenshot, COUNT (SnapScreenshot));
		delete ssi;
	}
#endif // !NEW_SNAPSHOT_SCREENSHOT

	S9xSetSoundMute (FALSE);
//#ifdef ZSNES_FX
//	if (Settings.SuperFX)
//		S9xSuperFXPostSaveState ();
//#endif

	#ifdef WIN32
	LeaveCriticalSection(&GUI.SoundCritSect);
	#endif

	freezing_to_stream = false;
}

bool disableMessageImmediateRefresh = false;

int S9xUnfreezeFromStream (STREAM stream)
{
    char buffer [_MAX_PATH + 1];
    char rom_filename [_MAX_PATH + 1];
    int result;
	
    int version;
    int len = strlen (SNAPSHOT_MAGIC) + 1 + 4 + 1;
    if (READ_STREAM (buffer, len, stream) != len)
		return (WRONG_FORMAT);
    if (strncmp (buffer, SNAPSHOT_MAGIC, strlen (SNAPSHOT_MAGIC)) != 0)
		return (WRONG_FORMAT);
    if ((version = atoi (&buffer [strlen (SNAPSHOT_MAGIC) + 1])) > SNAPSHOT_VERSION)
		return (WRONG_VERSION);
	
    if ((result = UnfreezeBlock (stream, "NAM", (uint8 *) rom_filename, _MAX_PATH)) != SUCCESS)
		return (result);

	disableMessageImmediateRefresh = true;

	#ifdef WIN32
	EnterCriticalSection(&GUI.SoundCritSect);
	#endif

    if (strcasecmp (rom_filename, Memory.ROMFilename) != 0 &&
		strcasecmp (S9xBasename (rom_filename), S9xBasename (Memory.ROMFilename)) != 0)
    {
		S9xMessage (S9X_WARNING, S9X_FREEZE_ROM_NAME,
			"Current loaded ROM image doesn't match that required by freeze-game file.");
    }
	
// ## begin load ##
	uint8* local_cpu = NULL;
	uint8* local_registers = NULL;
	uint8* local_ppu = NULL;
	uint8* local_dma = NULL;
	uint8* local_icpu = NULL;
	uint8* local_vram = NULL;
	uint8* local_ram = NULL;
	uint8* local_sram = NULL;
	uint8* local_fillram = NULL;
	uint8* local_apu = NULL;
	uint8* local_apu_registers = NULL;
	uint8* local_apu_ram = NULL;
	uint8* local_global_junk = NULL;
	uint8* local_apu_sounddata = NULL;
	uint8* local_iapu = NULL;
	uint8* local_sa1 = NULL;
	uint8* local_sa1_registers = NULL;
	uint8* local_zsuperfx = NULL;
	uint8* local_spc = NULL;
	uint8* local_spc_rtc = NULL;
	uint8* local_movie_data = NULL;
	uint8* local_dsp1 = NULL;
	uint8* local_cx4_data = NULL;
	uint8* local_ippu = NULL;
#ifndef NEW_SNAPSHOT_SCREENSHOT
	uint8* local_gfx = NULL;
#else
	uint8* local_screenshot = NULL;
#endif // !NEW_SNAPSHOT_SCREENSHOT
	uint8* local_dummy[2] = {NULL,NULL};

	do
	{
		if ((result = UnfreezeStructCopy (stream, "CPU", &local_cpu, SnapCPU, COUNT (SnapCPU), version)) != SUCCESS)
			break;
		if ((result = UnfreezeStructCopy (stream, "REG", &local_registers, SnapRegisters, COUNT (SnapRegisters), version)) != SUCCESS)
			break;
		if ((result = UnfreezeStructCopy (stream, "PPU", &local_ppu, SnapPPU, COUNT (SnapPPU), version)) != SUCCESS)
			break;
		if ((result = UnfreezeStructCopy (stream, "DMA", &local_dma, SnapDMA, COUNT (SnapDMA), version)) != SUCCESS)
			break;
		UnfreezeStructCopy (stream, "ICP", &local_icpu, SnapICPU, COUNT (SnapICPU), version);
		if ((result = UnfreezeBlockCopy (stream, "VRA", &local_vram, 0x10000)) != SUCCESS)
			break;
		if ((result = UnfreezeBlockCopy (stream, "RAM", &local_ram, 0x20000)) != SUCCESS)
			break;
		if ((result = UnfreezeBlockCopy (stream, "SRA", &local_sram, 0x20000)) != SUCCESS)
			break;
		if ((result = UnfreezeBlockCopy (stream, "FIL", &local_fillram, 0x8000)) != SUCCESS)
			break;
		if (UnfreezeStructCopy (stream, "APU", &local_apu, SnapAPU, COUNT (SnapAPU), version) == SUCCESS)
		{
			if ((result = UnfreezeStructCopy (stream, "ARE", &local_apu_registers, SnapAPURegisters, COUNT (SnapAPURegisters), version)) != SUCCESS)
				break;
			if ((result = UnfreezeBlockCopy (stream, "ARA", &local_apu_ram, 0x10000)) != SUCCESS)
				break;
			if ((result = UnfreezeStructCopy (stream, "SOU", &local_apu_sounddata, SnapSoundData, COUNT (SnapSoundData), version)) != SUCCESS)
				break;
			UnfreezeStructCopy (stream, "IAP", &local_iapu, SnapIAPU, COUNT (SnapIAPU), version);
		}

		UnfreezeStructCopy (stream, "GBJ", &local_global_junk, SnapGlobalJunk, COUNT (SnapGlobalJunk), version);

		if ((result = UnfreezeStructCopy (stream, "SA1", &local_sa1, SnapSA1, COUNT(SnapSA1), version)) == SUCCESS)
		{
			if ((result = UnfreezeStructCopy (stream, "SAR", &local_sa1_registers, SnapSA1Registers, COUNT (SnapSA1Registers), version)) != SUCCESS)
				break;
		}
		else if (Settings.SA1)
			break;

#ifdef ZSNES_FX
		UnfreezeStructCopy (stream, "GSF", &local_zsuperfx, SnapGlobalSuperFX, COUNT (SnapGlobalSuperFX), version);
#endif

		if ((result = UnfreezeStructCopy (stream, "SP7", &local_spc, SnapSPC7110, COUNT(SnapSPC7110), version)) != SUCCESS)
			if (Settings.SPC7110)
				break;

		if ((result = UnfreezeStructCopy (stream, "RTC", &local_spc_rtc, SnapS7RTC, COUNT (SnapS7RTC), version)) != SUCCESS)
			if (Settings.SPC7110RTC)
				break;

		// movie
		{
			SnapshotMovieInfo mi;
			if ((result = UnfreezeStruct (stream, "MOV", &mi, SnapMovie, COUNT(SnapMovie), version)) != SUCCESS)
			{
				if (S9xMovieActive ())
				{
					result = NOT_A_MOVIE_SNAPSHOT;
					break;
				}
			} else {

				if ((result = UnfreezeBlockCopy (stream, "MID", &local_movie_data, mi.MovieInputDataSize)) != SUCCESS)
				{
					if (S9xMovieActive ())
					{
						result = NOT_A_MOVIE_SNAPSHOT;
						break;
					}
				}

				if (S9xMovieActive ())
				{
					result = S9xMovieUnfreeze(local_movie_data, mi.MovieInputDataSize);
					if(result != SUCCESS)
						break;
				}
			}
		}

		if ((result = UnfreezeStructCopy (stream, "DSP", &local_dsp1, SnapDSP1, COUNT(SnapDSP1), version)) != SUCCESS)
			if(Settings.DSP1Master)
				break;

		if ((result = UnfreezeBlockCopy (stream, "CX4", &local_cx4_data, 8192)) != SUCCESS)
			if(Settings.C4)
				break;

		UnfreezeStructCopy (stream, "IPU", &local_ippu, SnapIPPU, COUNT(SnapIPPU), version);
#ifndef NEW_SNAPSHOT_SCREENSHOT
		UnfreezeStructCopy (stream, "GFX", &local_gfx, SnapGFX, COUNT(SnapGFX), version);
#else
		UnfreezeStructCopy (stream, "GFX", &local_dummy[1], SnapGFX, COUNT(SnapGFX), version); // obsolete
		UnfreezeStructCopy (stream, "SHO", &local_screenshot, SnapScreenshot, COUNT(SnapScreenshot), version);
#endif // !NEW_SNAPSHOT_SCREENSHOT

		result=SUCCESS;

	} while(false);
// ## end load ##

	if (result == SUCCESS)
	{
		uint32 old_flags = CPU.Flags;
		uint32 sa1_old_flags = SA1.Flags;
		S9xReset ();
		S9xSetSoundMute (TRUE);

		UnfreezeStructFromCopy (&CPU, SnapCPU, COUNT (SnapCPU), local_cpu, version);
		UnfreezeStructFromCopy (&Registers, SnapRegisters, COUNT (SnapRegisters), local_registers, version);
		UnfreezeStructFromCopy (&PPU, SnapPPU, COUNT (SnapPPU), local_ppu, version);
		UnfreezeStructFromCopy (DMA, SnapDMA, COUNT (SnapDMA), local_dma, version);
		if(local_icpu)
			UnfreezeStructFromCopy (&ICPU, SnapICPU, COUNT (SnapICPU), local_icpu, version);
		memcpy (Memory.VRAM, local_vram, 0x10000);
		memcpy (Memory.RAM, local_ram, 0x20000);
		memcpy (Memory.SRAM, local_sram, 0x20000);
		memcpy (Memory.FillRAM, local_fillram, 0x8000);
		APU.NextAPUTimerPos = CPU.Cycles * 10000L;
		APU.APUTimerCounter = 0; 
		if(local_apu)
		{
			UnfreezeStructFromCopy (&APU, SnapAPU, COUNT (SnapAPU), local_apu, version);
			UnfreezeStructFromCopy (&APURegisters, SnapAPURegisters, COUNT (SnapAPURegisters), local_apu_registers, version);
			memcpy (IAPU.RAM, local_apu_ram, 0x10000);
			UnfreezeStructFromCopy (&SoundData, SnapSoundData, COUNT (SnapSoundData), local_apu_sounddata, version);
			if(local_iapu)
				UnfreezeStructFromCopy (&IAPU, SnapIAPU, COUNT (SnapIAPU), local_iapu, version);
		}
		if(local_global_junk)
		{
			SGlobalJunk globalJunk;
			UnfreezeStructFromCopy (&globalJunk, SnapGlobalJunk, COUNT (SnapGlobalJunk), local_global_junk, version);
			SetCurrentGlobalJunk(globalJunk);
		}
		if(local_sa1)
		{
			SA1.Executing = 2; // version hack for S9xFixSA1AfterSnapshotLoad...
			UnfreezeStructFromCopy (&SA1, SnapSA1, COUNT (SnapSA1), local_sa1, version);
			UnfreezeStructFromCopy (&SA1Registers, SnapSA1Registers, COUNT (SnapSA1Registers), local_sa1_registers, version);
		}
		if(local_zsuperfx)
		{
			SGlobalSuperFX globalSuperFX;
			UnfreezeStructFromCopy (&globalSuperFX, SnapGlobalSuperFX, COUNT (SnapGlobalSuperFX), local_zsuperfx, version);
			SetCurrentGlobalSuperFX(globalSuperFX);
		}
		if(local_spc)
		{
			UnfreezeStructFromCopy (&s7r, SnapSPC7110, COUNT (SnapSPC7110), local_spc, version);
		}
		if(local_spc_rtc)
		{
			UnfreezeStructFromCopy (&rtc_f9, SnapS7RTC, COUNT (SnapS7RTC), local_spc_rtc, version);
		}

		if(local_dsp1)
		{
			UnfreezeStructFromCopy (&DSP1, SnapDSP1, COUNT (SnapDSP1), local_dsp1, version);
			S9xPostLoadDSP1();
		}

		if (local_cx4_data)
		{
			memcpy(Memory.C4RAM, local_cx4_data, 8192);
		}

		if(local_ippu)
		{
			UnfreezeStructFromCopy (&IPPU, SnapIPPU, COUNT (SnapIPPU), local_ippu, version);
		}

		if(GFX.Screen)
		{
#ifndef NEW_SNAPSHOT_SCREENSHOT
			if(local_gfx)
			{
				UnfreezeStructFromCopy (&GFX, SnapGFX, COUNT (SnapGFX), local_gfx, version);
			}
			else
			{
				// couldn't load graphics, so black out the screen instead
				for (uint32 y = 0; y < (uint32)IPPU.RenderedScreenHeight; y++)
					memset(GFX.LastScreen + y * GFX.RealPitch, 0, GFX.RealPitch);
			}

			for (uint32 y = 0; y < (uint32)IPPU.RenderedScreenHeight; y++)
				memcpy(GFX.Screen + y * GFX.RealPitch, GFX.LastScreen + y * GFX.RealPitch, GFX.RealPitch);
#else
			uint32 RealPPL=GFX.Pitch2/2;
			if(local_screenshot)
			{
				SnapshotScreenshotInfo *ssi = new SnapshotScreenshotInfo;
	 			UnfreezeStructFromCopy (ssi, SnapScreenshot, COUNT (SnapScreenshot), local_screenshot, version);
				IPPU.RenderedScreenWidth = min(ssi->Width, IMAGE_WIDTH);
				IPPU.RenderedScreenHeight = min(ssi->Height, IMAGE_HEIGHT);
				const bool scaleDownX = IPPU.RenderedScreenWidth < ssi->Width;
				const bool scaleDownY = IPPU.RenderedScreenHeight < ssi->Height && ssi->Height > SNES_HEIGHT_EXTENDED;
	//			GFX.DoInterlace = Settings.SupportHiRes ? ssi->Interlaced : 0;

				uint8 *rowpix=ssi->Data;
				uint16 *screen=(uint16*)GFX.Screen;
				for(int y=0; y<IPPU.RenderedScreenHeight; y++, screen+=RealPPL){
					for(int x=0; x<IPPU.RenderedScreenWidth; x++){
					 	uint32 r, g, b;
						r = *(rowpix++);
						g = *(rowpix++);
						b = *(rowpix++);
						if(scaleDownX)
						{
							r = (r + *(rowpix++))>>1;
							g = (g + *(rowpix++))>>1;
							b = (b + *(rowpix++))>>1;
							if(x+x+1 >= ssi->Width)
								break;
						}
						screen[x] = BUILD_PIXEL(r, g, b);
					}
					if(scaleDownY)
					{
						rowpix += 3*ssi->Width;
						if(y+y+1 >= ssi->Height)
							break;
					}
				}

				// black out what we might have missed
	 			for (uint32 y = IPPU.RenderedScreenHeight; y < (uint32)(IMAGE_HEIGHT); y++)
					memset((uint16*)GFX.Screen + y * RealPPL, 0, RealPPL*2);
				delete ssi;
			}
			else
			{
				// couldn't load graphics, so black out the screen instead
	 			for (uint32 y = 0; y < (uint32)(IMAGE_HEIGHT); y++)
					memset((uint16*)GFX.Screen + y * RealPPL, 0, RealPPL*2);
			}
#endif // !NEW_SNAPSHOT_SCREENSHOT
		} /* GFX.Screen */

		if(local_movie_data)
		{
			bool8 pad_read_temp = IPPU.pad_read;
			IPPU.pad_read = IPPU.pad_read_last;
			S9xUpdateFrameCounter (-1);
			IPPU.pad_read = pad_read_temp;
		}

		Memory.FixROMSpeed ();
		CPU.Flags |= old_flags & (DEBUG_MODE_FLAG | TRACE_FLAG |
			SINGLE_STEP_FLAG | FRAME_ADVANCE_FLAG);

	    IPPU.ColorsChanged = TRUE;
		IPPU.OBJChanged = TRUE;
//		CPU.InDMA = FALSE;
		S9xFixColourBrightness ();
		IPPU.RenderThisFrame = TRUE; // was FALSE, but for most games it's more useful to see that frame

		if (local_apu)
		{
			S9xSetSoundMute (FALSE);
			if(!local_iapu)
			{
				// something seems to be wrong with this logic
				// (for example, sometimes APUExecuting is false when a savestate is made, but this sets it to always true)
				// so this branch has been replaced by simply saving the IAPU struct instead

				IAPU.PC = IAPU.RAM + APURegisters.PC;
				S9xAPUUnpackStatus ();
				if (APUCheckDirectPage ())
					IAPU.DirectPage = IAPU.RAM + 0x100;
				else
					IAPU.DirectPage = IAPU.RAM;
				IAPU.APUExecuting = TRUE;
			}
			Settings.APUEnabled = TRUE;
		}
		else
		{
			Settings.APUEnabled = FALSE;
			IAPU.APUExecuting = FALSE;
			S9xSetSoundMute (TRUE);
		}

		if (local_sa1)
		{
			S9xFixSA1AfterSnapshotLoad ();
			SA1.Flags |= sa1_old_flags & (TRACE_FLAG);
		}

		if (local_spc_rtc)
		{
			S9xUpdateRTC();
		}

		S9xFixSoundAfterSnapshotLoad (version);

//		uint8 hdma_byte = Memory.FillRAM[0x420c];
//		S9xSetCPU(hdma_byte, 0x420c); // was sometimes causing desync

#if 0 // disabled because it happens even for some new savestates, which causes desync
		if(!Memory.FillRAM[0x4213]){
			// most likely an old savestate
			Memory.FillRAM[0x4213]=Memory.FillRAM[0x4201];
			if(!Memory.FillRAM[0x4213])
				Memory.FillRAM[0x4213]=Memory.FillRAM[0x4201]=0xFF;
		}
#endif

		if(!local_icpu)
		{
			ICPU.ShiftedPB = Registers.PB << 16;
			ICPU.ShiftedDB = Registers.DB << 16;
		}
		S9xSetPCBase (ICPU.ShiftedPB + Registers.PC);
		if(!local_icpu)
			S9xUnpackStatus (); // seems to have something incorrect about its logic (because saving then immediately loading sometimes flips ICPU._Negative) so it's been replaced by simply saving the ICPU struct
		S9xFixCycles ();
//		S9xReschedule ();				// <-- this causes desync when recording or playing movies

//#ifdef ZSNES_FX
//		if (Settings.SuperFX)
//			S9xSuperFXPostLoadState ();
//#endif
		
		S9xSRTCPostLoadState ();
		if (Settings.SDD1)
			S9xSDD1PostLoadState ();
	}

	if (local_cpu)           delete [] local_cpu;
	if (local_registers)     delete [] local_registers;
	if (local_ppu)           delete [] local_ppu;
	if (local_dma)           delete [] local_dma;
	if (local_icpu)          delete [] local_icpu;
	if (local_vram)          delete [] local_vram;
	if (local_ram)           delete [] local_ram;
	if (local_sram)          delete [] local_sram;
	if (local_fillram)       delete [] local_fillram;
	if (local_apu)           delete [] local_apu;
	if (local_apu_registers) delete [] local_apu_registers;
	if (local_apu_ram)       delete [] local_apu_ram;
	if (local_apu_sounddata) delete [] local_apu_sounddata;
	if (local_iapu)          delete [] local_iapu;
	if (local_global_junk)   delete [] local_global_junk;
	if (local_sa1)           delete [] local_sa1;
	if (local_sa1_registers) delete [] local_sa1_registers;
	if (local_zsuperfx)      delete [] local_zsuperfx;
	if (local_spc)           delete [] local_spc;
	if (local_spc_rtc)       delete [] local_spc_rtc;
	if (local_movie_data)    delete [] local_movie_data;
	if (local_dsp1)          delete [] local_dsp1;
	if (local_cx4_data)      delete [] local_cx4_data;
	if (local_ippu)          delete [] local_ippu;
#ifndef NEW_SNAPSHOT_SCREENSHOT
	if (local_gfx)			 delete [] local_gfx;
#else
	if (local_screenshot)	 delete [] local_screenshot;
#endif // !NEW_SNAPSHOT_SCREENSHOT
	for(int i=0; i<2; i++)
	if (local_dummy[i])		 delete [] local_dummy[i];

	#ifdef WIN32
	LeaveCriticalSection(&GUI.SoundCritSect);
	#endif

	disableMessageImmediateRefresh = false;

	return (result);
}


/*****************************************************************/

int FreezeSize (int size, int type)
{
    switch (type)
    {
      case uint16_ARRAY_V:
      case uint16_INDIR_ARRAY_V:
        return (size * 2);
      case uint32_ARRAY_V:
      case uint32_INDIR_ARRAY_V:
        return (size * 4);
      default:
        return (size);
    }
}

void FreezeStructF (STREAM stream, char *name, void *base, FreezeData *fields,
				   int num_fields)
{
    // Work out the size of the required block
    int len = 0;
    int i;
    int j;

    for (i = 0; i < num_fields; i++)
    {
		if(fields[i].debuted_in > SNAPSHOT_VERSION)
		{
			fprintf(stderr, "%s[%p]: field has bad debuted_in value %d, > %d.", name, (void *)fields, fields[i].debuted_in, SNAPSHOT_VERSION);
			continue;
		}

        if (SNAPSHOT_VERSION<fields[i].deleted_in)
		{
            len += FreezeSize (fields[i].size, fields[i].type);

#ifdef DIAGNOSTIC_FREEZING_SUPPORT
			if(diagnostic_freezing)
			{
				if(fields[i].name && *fields[i].name)
					len += 1+strlen(fields[i].name)+2;
				else
					len += 10/*strlen("\nUNKNOWN: ")*/;
			}
#endif
		}
    }
    //fprintf(stderr, "%s: freeze size is %d\n", name, len);

    uint8 *block = new uint8 [len];
    uint8 *ptr = block;
    uint16 word;
    uint32 dword;
    int64  qword;

    // Build the block ready to be streamed out
    for (i = 0; i < num_fields; i++)
    {
        if (SNAPSHOT_VERSION>=fields[i].deleted_in) continue;
        if (SNAPSHOT_VERSION<fields[i].debuted_in) continue;

#ifdef DIAGNOSTIC_FREEZING_SUPPORT
		// if diagnostic_freezing is set, output full field name information too.
		// it's already proved useful in tracking down savestate problems, although it renders the state unloadable
		if(diagnostic_freezing)
		{
			if(fields[i].name && *fields[i].name)
			{
				*ptr++ = '\n';
				memcpy(ptr, fields[i].name, strlen(fields[i].name));
				ptr += strlen(fields[i].name);
				*ptr++ = ':';
				*ptr++ = ' ';
			}
			else
			{
				memcpy(ptr, "\nUNKNOWN: ", 10);
				ptr += 10;
			}
		}
#endif
#ifdef SNAPSHOT_VERIFY_SUPPORTED
		if(s_verifyingSnapshot)
		{
			if(fields[i].name && *fields[i].name)
			{
				PointerRangeInfo pri = {ptr, ptr+FreezeSize(fields[i].size,fields[i].type), fields[i].name};
				s_verifyActiveRangeInfo.push_back(pri);
			}
		}
#endif

		uint8 *addr = (uint8 *) base + fields[i].offset;

		// determine real address of indirect-type fields
		// (where the structure contains a pointer to an array rather than the array itself)
		if (fields[i].type == uint8_INDIR_ARRAY_V || fields[i].type == uint16_INDIR_ARRAY_V || fields[i].type == uint32_INDIR_ARRAY_V)
			addr = (uint8 *)(*((pint*)addr));

		// convert pointer-type saves from absolute to relative pointers
		int relativeAddr;
		if(fields[i].type == POINTER_V)
		{
			uint8* pointer = (uint8*)*((pint*)((uint8 *) base + fields[i].offset));
			uint8* relativeTo = (fields[i].offset2 == -1) ? Memory.Base : (uint8*)*((pint*)((uint8 *) base + fields[i].offset2));
			if(!pointer) relativeTo = (uint8*)1; // store NULL as -1
			relativeAddr = pointer - relativeTo;
			addr = (uint8*)&relativeAddr;
		}

		switch (fields[i].type)
		{
		case INT_V:
		case POINTER_V:
			switch (fields[i].size)
			{
			case 1:
				*ptr++ = *(addr);
				break;
			case 2:
				word = *((uint16 *) (addr));
				*ptr++ = (uint8) (word >> 8);
				*ptr++ = (uint8) word;
				break;
			case 4:
				dword = *((uint32 *) (addr));
				*ptr++ = (uint8) (dword >> 24);
				*ptr++ = (uint8) (dword >> 16);
				*ptr++ = (uint8) (dword >> 8);
				*ptr++ = (uint8) dword;
				break;
			case 8:
				qword = *((int64 *) (addr));
                *ptr++ = (uint8) (qword >> 56);
                *ptr++ = (uint8) (qword >> 48);
                *ptr++ = (uint8) (qword >> 40);
                *ptr++ = (uint8) (qword >> 32);
                *ptr++ = (uint8) (qword >> 24);
                *ptr++ = (uint8) (qword >> 16);
                *ptr++ = (uint8) (qword >> 8);
                *ptr++ = (uint8) qword;
                break;
            }
            break;
            case uint8_ARRAY_V:
			case uint8_INDIR_ARRAY_V:
				memmove (ptr, addr, fields[i].size);
				ptr += fields[i].size;
				break;
			case uint16_ARRAY_V:
			case uint16_INDIR_ARRAY_V:
				for (j = 0; j < fields[i].size; j++)
				{
					word = *((uint16 *) (addr + j * 2));
					*ptr++ = (uint8) (word >> 8);
					*ptr++ = (uint8) word;
				}
				break;
			case uint32_ARRAY_V:
			case uint32_INDIR_ARRAY_V:
				for (j = 0; j < fields[i].size; j++)
				{
					dword = *((uint32 *) (addr + j * 4));
                *ptr++ = (uint8) (dword >> 24);
                *ptr++ = (uint8) (dword >> 16);
                *ptr++ = (uint8) (dword >> 8);
                *ptr++ = (uint8) dword;
            }
            break;
        }
    }
    //fprintf(stderr, "%s: Wrote %d bytes\n", name, ptr-block);

    FreezeBlockF (stream, name, block, len);
    delete[] block;

#ifdef SNAPSHOT_VERIFY_SUPPORTED
	if(s_verifyingSnapshot)
		s_verifyActiveRangeInfo.clear();
#endif
}

void FreezeBlockF (STREAM stream, char *name, uint8 *block, int size)
{
//#ifdef SNAPSHOT_VERIFY_SUPPORTED
//	if(s_verifyingSnapshot)
//		s_verifyInfoStack.push_back(name);
//#endif

    char buffer [512];

    if(size <= 999999) // check if it fits in 6 digits. (letting it go over and using strlen isn't safe)
        sprintf (buffer, "%s:%06d:", name, size);
    else
    {
        // to make it fit, pack it in the bytes instead of as digits
        sprintf (buffer, "%s:------:", name);
        buffer[6] = (unsigned char)((unsigned)size >> 24);
        buffer[7] = (unsigned char)((unsigned)size >> 16);
        buffer[8] = (unsigned char)((unsigned)size >> 8);
        buffer[9] = (unsigned char)((unsigned)size >> 0);
    }
    buffer[11] = 0;

	WRITE_STREAM (buffer, 11, stream);
    WRITE_STREAM (block, size, stream);

//#ifdef SNAPSHOT_VERIFY_SUPPORTED
//	if(s_verifyingSnapshot)
//		s_verifyInfoStack.pop_back();
//#endif
}


/*****************************************************************/

int UnfreezeBlock (STREAM stream, char *name, uint8 *block, int size)
{
    char buffer [20];
    int len = 0;
    int rem = 0;
    long rewind = FIND_STREAM(stream);

	size_t l = READ_STREAM (buffer, 11, stream);
	buffer[l] = 0;
    if (l != 11
     || strncmp (buffer, name, 3) != 0
     || buffer[3] != ':')
    {
    err:
#ifdef _DEBUG
		fprintf(stdout, "absent: %s(%d); next: '%.11s'\n", name, size, buffer);
#endif
		REVERT_STREAM(stream, FIND_STREAM(stream)-l, 0);
		return (WRONG_FORMAT);
    }

    if(buffer[4] == '-')
    {
        len = (((unsigned char)buffer[6]) << 24)
            | (((unsigned char)buffer[7]) << 16)
            | (((unsigned char)buffer[8]) << 8)
            | (((unsigned char)buffer[9]) << 0);
    }
    else
    {
        len = atoi(buffer+4);
    }
    if(len <= 0) goto err;

    if (len > size)
    {
        rem = len - size;
        len = size;
    }
    ZeroMemory (block, size);
    if (READ_STREAM (block, len, stream) != len)
    {
        REVERT_STREAM(stream, rewind, 0);
        return (WRONG_FORMAT);
    }
    if (rem)
    {
        char *junk = new char [rem];
        len = READ_STREAM (junk, rem, stream);
        delete [] junk;
        if (len != rem)
        {
            REVERT_STREAM(stream, rewind, 0);
            return (WRONG_FORMAT);
        }
    }

    return (SUCCESS);
}

int UnfreezeBlockCopy (STREAM stream, char *name, uint8** block, int size)
{
    *block = new uint8 [size];
    int result;
	
    if ((result = UnfreezeBlock (stream, name, *block, size)) != SUCCESS)
    {
		delete [] (*block);
		*block = NULL;
		return (result);
    }
	
    return (result);
}

int UnfreezeStruct (STREAM stream, char *name, void *base, FreezeData *fields,
					int num_fields, int version)
{
    uint8 *block = NULL;
    int result;

    result = UnfreezeStructCopy (stream, name, &block, fields, num_fields, version);
    if (result != SUCCESS)
    {
        if (block!=NULL) delete [] block;
        return result;
    }
    UnfreezeStructFromCopy (base, fields, num_fields, block, version);
    delete [] block;
    return SUCCESS;
}

int UnfreezeStructCopy (STREAM stream, char *name, uint8** block, FreezeData *fields, int num_fields, int version)
{
    // Work out the size of the required block
    int len = 0;
    int i;

    for (i = 0; i < num_fields; i++)
    {
        if (version>=fields[i].debuted_in && version<fields[i].deleted_in)
            len += FreezeSize (fields[i].size, fields[i].type);
    }
    //fprintf(stderr, "%s[%p]: unfreeze size is %d\n", name, fields, len);

    return (UnfreezeBlockCopy (stream, name, block, len));
}

void UnfreezeStructFromCopy (void *sbase, FreezeData *fields, int num_fields, uint8* block, int version)
{
    int i;
    int j;
    uint8 *ptr = block;
    uint16 word;
    uint32 dword;
    int64  qword;
    void *base;

    // Unpack the block of data into a C structure
    for (i = 0; i < num_fields; i++)
    {
        if (version<fields[i].debuted_in || version>=fields[i].deleted_in) continue;
        base = (SNAPSHOT_VERSION>=fields[i].deleted_in)?((void *)&Obsolete):sbase;

		uint8 *addr = (uint8 *) base + fields[i].offset;

		// determine real address of indirect-type fields
		// (where the structure contains a pointer to an array rather than the array itself)
		if (fields[i].type == uint8_INDIR_ARRAY_V || fields[i].type == uint16_INDIR_ARRAY_V || fields[i].type == uint32_INDIR_ARRAY_V)
			addr = (uint8 *)(*((pint*)addr));

        switch (fields[i].type)
        {
          case INT_V:
          case POINTER_V:
            switch (fields[i].size)
            {
              case 1:
                if(fields[i].offset<0){ ptr++; break; }
                *(addr) = *ptr++;
                break;
              case 2:
                if(fields[i].offset<0){ ptr+=2; break; }
                word  = *ptr++ << 8;
                word |= *ptr++;
                *((uint16 *) (addr)) = word;
                break;
              case 4:
                if(fields[i].offset<0){ ptr+=4; break; }
                dword  = *ptr++ << 24;
                dword |= *ptr++ << 16;
                dword |= *ptr++ << 8;
                dword |= *ptr++;
				//assert(!((uint8*)dword >= Memory.Base && (uint8*)dword < Memory.Base + 0x80000+0x20000+0x20000+0x10000+0x200+0x8000+0x800000));
                *((uint32 *) (addr)) = dword;
                break;
              case 8:
                if(fields[i].offset<0){ ptr+=8; break; }
                qword  = (int64) *ptr++ << 56;
                qword |= (int64) *ptr++ << 48;
                qword |= (int64) *ptr++ << 40;
                qword |= (int64) *ptr++ << 32;
                qword |= (int64) *ptr++ << 24;
                qword |= (int64) *ptr++ << 16;
                qword |= (int64) *ptr++ << 8;
                qword |= (int64) *ptr++;
                *((int64 *) (addr)) = qword;
                break;
			  default:
				  assert(0);
				  break;
            }
            break;
          case uint8_ARRAY_V:
		  case uint8_INDIR_ARRAY_V:
            if(fields[i].offset>=0)
                memmove (addr, ptr, fields[i].size);
            ptr += fields[i].size;
            break;
          case uint16_ARRAY_V:
		  case uint16_INDIR_ARRAY_V:
            if(fields[i].offset<0){ ptr+=fields[i].size*2; break; }
            for (j = 0; j < fields[i].size; j++)
            {
                word  = *ptr++ << 8;
                word |= *ptr++;
                *((uint16 *) (addr + j * 2)) = word;
            }
            break;
          case uint32_ARRAY_V:
		  case uint32_INDIR_ARRAY_V:
            if(fields[i].offset<0){ ptr+=fields[i].size*4; break; }
            for (j = 0; j < fields[i].size; j++)
            {
                dword  = *ptr++ << 24;
                dword |= *ptr++ << 16;
                dword |= *ptr++ << 8;
                dword |= *ptr++;
                *((uint32 *) (addr + j * 4)) = dword;
            }
            break;
        }

		// convert pointer-type saves from relative to absolute pointers
		if(fields[i].type == POINTER_V)
		{
			int relativeAddr = (int)*((pint*)((uint8 *) base + fields[i].offset));
			uint8* relativeTo = (fields[i].offset2 == -1) ? Memory.Base : (uint8*)*((pint*)((uint8 *) base + fields[i].offset2));
			if(relativeAddr == -1) relativeTo = (uint8*)1; // NULL is stored as -1
			*((pint *) (addr)) = (pint)(relativeTo + relativeAddr);
		}
	}
    //fprintf(stderr, "%p: Unfroze %d bytes\n", fields, ptr-block);
}


/*****************************************************************/

bool8 S9xSPCDump (const char *filename)
{
    static uint8 header [] = {
		'S', 'N', 'E', 'S', '-', 'S', 'P', 'C', '7', '0', '0', ' ',
			'S', 'o', 'u', 'n', 'd', ' ', 'F', 'i', 'l', 'e', ' ',
			'D', 'a', 't', 'a', ' ', 'v', '0', '.', '3', '0', 26, 26, 26
    };
    static uint8 version = {
		0x1e
    };
	const uint8 reserved[64] = { 0 };
	
    FILE *fs;
		
    if (!(fs = fopen (filename, "wb")))
		return (FALSE);

    S9xSetSoundMute (TRUE);

    // The SPC file format:
    // 0000: header:	'SNES-SPC700 Sound File Data v0.30',26,26,26
    // 0036: version:	$1e
    // 0037: SPC700 PC:
    // 0039: SPC700 A:
    // 0040: SPC700 X:
    // 0041: SPC700 Y:
    // 0042: SPC700 P:
    // 0043: SPC700 S:
    // 0044: Reserved: 0, 0, 0, 0
    // 0048: Title of game: 32 bytes
    // 0000: Song name: 32 bytes
    // 0000: Name of dumper: 32 bytes
    // 0000: Comments: 32 bytes
    // 0000: Date of SPC dump: 4 bytes
    // 0000: Fade out time in milliseconds: 4 bytes
    // 0000: Fade out length in milliseconds: 2 bytes
    // 0000: Default channel enables: 1 bytes
    // 0000: Emulator used to dump .SPC files: 1 byte, 1 == ZSNES
    // 0000: Reserved: 36 bytes
    // 0256: SPC700 RAM: 64K
    // ----: DSP Registers: 256 bytes
	
    if (fwrite (header, sizeof (header), 1, fs) != 1 ||
		fputc (version, fs) == EOF ||
		fseek (fs, 37, SEEK_SET) == EOF ||
		fputc (APURegisters.PC & 0xff, fs) == EOF ||
		fputc (APURegisters.PC >> 8, fs) == EOF ||
		fputc (APURegisters.YA.B.A, fs) == EOF ||
		fputc (APURegisters.X, fs) == EOF ||
		fputc (APURegisters.YA.B.Y, fs) == EOF ||
		fputc (APURegisters.P, fs) == EOF ||
		fputc (APURegisters.S, fs) == EOF ||
		fseek (fs, 256, SEEK_SET) == EOF ||
		fwrite (IAPU.RAM, 0x10000, 1, fs) != 1 ||
		fwrite (APU.DSP, 1, 128, fs) != 128 ||
		fwrite (reserved, 1, 64, fs) != 64 ||
		fwrite (APU.ExtraRAM, 64, 1, fs) != 1 ||
		fclose (fs) < 0)
    {
		S9xSetSoundMute (FALSE);
		return (FALSE);
    }
    S9xSetSoundMute (FALSE);
    return (TRUE);
}

bool8 S9xUnfreezeZSNES (const char *filename)
{
    FILE *fs;
    uint8 t [4000];
	
    if (!(fs = fopen (filename, "rb")))
		return (FALSE);
	
    if (fread (t, 64, 1, fs) == 1 &&
		strncmp ((char *) t, "ZSNES Save State File V0.6", 26) == 0)
    {
		S9xReset ();
		S9xSetSoundMute (TRUE);
		
		// 28 Curr cycle
		CPU.V_Counter = READ_WORD (&t[29]);
		// 33 instrset
		Settings.APUEnabled = t[36];
		
		// 34 bcycpl cycles per scanline
		// 35 cycphb cyclers per hblank
		
		Registers.A.W   = READ_WORD (&t[41]);
		Registers.DB    = t[43];
		Registers.PB    = t[44];
		Registers.S.W   = READ_WORD (&t[45]);
		Registers.D.W   = READ_WORD (&t[47]);
		Registers.X.W   = READ_WORD (&t[49]);
		Registers.Y.W   = READ_WORD (&t[51]);
		Registers.P.W   = READ_WORD (&t[53]);
		Registers.PC    = READ_WORD (&t[55]);
		
		fread (t, 1, 8, fs);
		fread (t, 1, 3019, fs);
		S9xSetCPU (t[2], 0x4200);
		Memory.FillRAM [0x4210] = t[3];
		PPU.IRQVBeamPos = READ_WORD (&t[4]);
		PPU.IRQHBeamPos = READ_WORD (&t[2527]);
		PPU.Brightness = t[6];
		PPU.ForcedBlanking = t[8] >> 7;
		
		int i;
		for (i = 0; i < 544; i++)
			S9xSetPPU (t[0464 + i], 0x2104);
		
		PPU.OBJNameBase = READ_WORD (&t[9]);
		PPU.OBJNameSelect = READ_WORD (&t[13]) - PPU.OBJNameBase;
		switch (t[18])
		{
		case 4:
			if (t[17] == 1)
				PPU.OBJSizeSelect = 0;
			else
				PPU.OBJSizeSelect = 6;
			break;
		case 16:
			if (t[17] == 1)
				PPU.OBJSizeSelect = 1;
			else
				PPU.OBJSizeSelect = 3;
			break;
		default:
		case 64:
			if (t[17] == 1)
				PPU.OBJSizeSelect = 2;
			else
				if (t[17] == 4)
					PPU.OBJSizeSelect = 4;
				else
					PPU.OBJSizeSelect = 5;
				break;
		}
		PPU.OAMAddr = READ_WORD (&t[25]);
		PPU.SavedOAMAddr =  READ_WORD (&t[27]);
		PPU.FirstSprite = t[29];
		PPU.BGMode = t[30];
		PPU.BG3Priority = t[31];
		PPU.BG[0].BGSize = (t[32] >> 0) & 1;
		PPU.BG[1].BGSize = (t[32] >> 1) & 1;
		PPU.BG[2].BGSize = (t[32] >> 2) & 1;
		PPU.BG[3].BGSize = (t[32] >> 3) & 1;
		PPU.Mosaic = t[33] + 1;
		PPU.BGMosaic [0] = (t[34] & 1) != 0;
		PPU.BGMosaic [1] = (t[34] & 2) != 0;
		PPU.BGMosaic [2] = (t[34] & 4) != 0;
		PPU.BGMosaic [3] = (t[34] & 8) != 0;
		PPU.BG [0].SCBase = READ_WORD (&t[35]) >> 1;
		PPU.BG [1].SCBase = READ_WORD (&t[37]) >> 1;
		PPU.BG [2].SCBase = READ_WORD (&t[39]) >> 1;
		PPU.BG [3].SCBase = READ_WORD (&t[41]) >> 1;
		PPU.BG [0].SCSize = t[67];
		PPU.BG [1].SCSize = t[68];
		PPU.BG [2].SCSize = t[69];
		PPU.BG [3].SCSize = t[70];
		PPU.BG[0].NameBase = READ_WORD (&t[71]) >> 1;
		PPU.BG[1].NameBase = READ_WORD (&t[73]) >> 1;
		PPU.BG[2].NameBase = READ_WORD (&t[75]) >> 1;
		PPU.BG[3].NameBase = READ_WORD (&t[77]) >> 1;
		PPU.BG[0].HOffset = READ_WORD (&t[79]);
		PPU.BG[1].HOffset = READ_WORD (&t[81]);
		PPU.BG[2].HOffset = READ_WORD (&t[83]);
		PPU.BG[3].HOffset = READ_WORD (&t[85]);
		PPU.BG[0].VOffset = READ_WORD (&t[89]);
		PPU.BG[1].VOffset = READ_WORD (&t[91]);
		PPU.BG[2].VOffset = READ_WORD (&t[93]);
		PPU.BG[3].VOffset = READ_WORD (&t[95]);
		PPU.VMA.Increment = READ_WORD (&t[97]) >> 1;
		PPU.VMA.High = t[99];
#ifndef CORRECT_VRAM_READS
                IPPU.FirstVRAMRead = t[100];
#endif
		S9xSetPPU (t[2512], 0x2115);
		PPU.VMA.Address = READ_DWORD (&t[101]);
		for (i = 0; i < 512; i++)
			S9xSetPPU (t[1488 + i], 0x2122);

		PPU.CGADD = (uint8) READ_WORD (&t[105]);
		Memory.FillRAM [0x212c] = t[108];
		Memory.FillRAM [0x212d] = t[109];
		PPU.ScreenHeight = READ_WORD (&t[111]);
		Memory.FillRAM [0x2133] = t[2526];
		Memory.FillRAM [0x4202] = t[113];
		Memory.FillRAM [0x4204] = t[114];
		Memory.FillRAM [0x4205] = t[115];
		Memory.FillRAM [0x4214] = t[116];
		Memory.FillRAM [0x4215] = t[117];
		Memory.FillRAM [0x4216] = t[118];
		Memory.FillRAM [0x4217] = t[119];
		PPU.VBeamPosLatched = READ_WORD (&t[122]);
		PPU.HBeamPosLatched = READ_WORD (&t[120]);
		PPU.Window1Left = t[127];
		PPU.Window1Right = t[128];
		PPU.Window2Left = t[129];
		PPU.Window2Right = t[130];
		S9xSetPPU (t[131] | (t[132] << 4), 0x2123);
		S9xSetPPU (t[133] | (t[134] << 4), 0x2124);
		S9xSetPPU (t[135] | (t[136] << 4), 0x2125);
		S9xSetPPU (t[137], 0x212a);
		S9xSetPPU (t[138], 0x212b);
		S9xSetPPU (t[139], 0x212e);
		S9xSetPPU (t[140], 0x212f);
		S9xSetPPU (t[141], 0x211a);
		PPU.MatrixA = READ_WORD (&t[142]);
		PPU.MatrixB = READ_WORD (&t[144]);
		PPU.MatrixC = READ_WORD (&t[146]);
		PPU.MatrixD = READ_WORD (&t[148]);
		PPU.CentreX = READ_WORD (&t[150]);
		PPU.CentreY = READ_WORD (&t[152]);
		// JoyAPos t[154]
		// JoyBPos t[155]
		Memory.FillRAM [0x2134] = t[156]; // Matrix mult
		Memory.FillRAM [0x2135] = t[157]; // Matrix mult
		Memory.FillRAM [0x2136] = t[158]; // Matrix mult
		PPU.WRAM = READ_DWORD (&t[161]);
		
		for (i = 0; i < 128; i++)
			S9xSetCPU (t[165 + i], 0x4300 + i);
		
		if (t[294])
			CPU.IRQActive |= PPU_V_BEAM_IRQ_SOURCE | PPU_H_BEAM_IRQ_SOURCE;
		
		S9xSetCPU (t[296], 0x420c);
		// hdmadata t[297] + 8 * 19
		PPU.FixedColourRed = t[450];
		PPU.FixedColourGreen = t[451];
		PPU.FixedColourBlue = t[452];
		S9xSetPPU (t[454], 0x2130);
		S9xSetPPU (t[455], 0x2131);
		// vraminctype ...
		
		fread (Memory.RAM, 1, 128 * 1024, fs);
		fread (Memory.VRAM, 1, 64 * 1024, fs);
		
		if (Settings.APUEnabled)
		{
			// SNES SPC700 RAM (64K)
			fread (IAPU.RAM, 1, 64 * 1024, fs);
			
			// Junk 16 bytes
			fread (t, 1, 16, fs);
			
			// SNES SPC700 state and internal ZSNES SPC700 emulation state
			fread (t, 1, 304, fs);
			
			APURegisters.PC   = READ_DWORD (&t[0]);
			APURegisters.YA.B.A = t[4];
			APURegisters.X    = t[8];
			APURegisters.YA.B.Y = t[12];
			APURegisters.P    = t[16];
			APURegisters.S    = t[24];
			
			APU.Cycles = READ_DWORD (&t[32]);
			APU.ShowROM = (IAPU.RAM [0xf1] & 0x80) != 0;
			APU.OutPorts [0] = t[36];
			APU.OutPorts [1] = t[37];
			APU.OutPorts [2] = t[38];
			APU.OutPorts [3] = t[39];
			
			APU.TimerEnabled [0] = (t[40] & 1) != 0;
			APU.TimerEnabled [1] = (t[40] & 2) != 0;
			APU.TimerEnabled [2] = (t[40] & 4) != 0;
			S9xSetAPUTimer (0xfa, t[41]);
			S9xSetAPUTimer (0xfb, t[42]);
			S9xSetAPUTimer (0xfc, t[43]);
			APU.Timer [0] = t[44];
			APU.Timer [1] = t[45];
			APU.Timer [2] = t[46];
			
			memmove (APU.ExtraRAM, &t[48], 64);
			
			// Internal ZSNES sound DSP state
			fread (t, 1, 1068, fs);
			
			// SNES sound DSP register values
			fread (t, 1, 256, fs);
			
			uint8 saved = IAPU.RAM [0xf2];
			
			for (i = 0; i < 128; i++)
			{
				switch (i)
				{
				case APU_KON:
				case APU_KOFF:
					break;
				case APU_FLG:
					t[i] &= ~APU_SOFT_RESET;
				default:
					IAPU.RAM [0xf2] = i;
					S9xSetAPUDSP (t[i]);
					break;
				}
			}
			IAPU.RAM [0xf2] = APU_KON;
			S9xSetAPUDSP (t[APU_KON]);
			IAPU.RAM [0xf2] = saved;
			
			S9xSetSoundMute (FALSE);
			IAPU.PC = IAPU.RAM + APURegisters.PC;
			S9xAPUUnpackStatus ();
			if (APUCheckDirectPage ())
				IAPU.DirectPage = IAPU.RAM + 0x100;
			else
				IAPU.DirectPage = IAPU.RAM;
			Settings.APUEnabled = TRUE;
			IAPU.APUExecuting = TRUE;
		}
		else
		{
			Settings.APUEnabled = FALSE;
			IAPU.APUExecuting = FALSE;
			S9xSetSoundMute (TRUE);
		}
		
		if (Settings.SuperFX)
		{
			fread (Memory.SRAM, 1, 64 * 1024, fs);
			fseek (fs, 64 * 1024, SEEK_CUR);
			fread (Memory.FillRAM + 0x7000, 1, 692, fs);
		}
		if (Settings.SA1)
		{
			fread (t, 1, 2741, fs);
			S9xSetSA1 (t[4], 0x2200);  // Control
			S9xSetSA1 (t[12], 0x2203);	// ResetV low
			S9xSetSA1 (t[13], 0x2204); // ResetV hi
			S9xSetSA1 (t[14], 0x2205); // NMI low
			S9xSetSA1 (t[15], 0x2206); // NMI hi
			S9xSetSA1 (t[16], 0x2207); // IRQ low
			S9xSetSA1 (t[17], 0x2208); // IRQ hi
			S9xSetSA1 (((READ_DWORD (&t[28]) - (4096*1024-0x6000))) >> 13, 0x2224);
			S9xSetSA1 (t[36], 0x2201);
			S9xSetSA1 (t[41], 0x2209);
			
			SA1Registers.A.W = READ_DWORD (&t[592]);
			SA1Registers.X.W = READ_DWORD (&t[596]);
			SA1Registers.Y.W = READ_DWORD (&t[600]);
			SA1Registers.D.W = READ_DWORD (&t[604]);
			SA1Registers.DB  = t[608];
			SA1Registers.PB  = t[612];
			SA1Registers.S.W = READ_DWORD (&t[616]);
			SA1Registers.PC  = READ_DWORD (&t[636]);
			SA1Registers.P.W = t[620] | (t[624] << 8);
			
			memmove (&Memory.FillRAM [0x3000], t + 692, 2 * 1024);
			
			fread (Memory.SRAM, 1, 64 * 1024, fs);
			fseek (fs, 64 * 1024, SEEK_CUR);
			S9xFixSA1AfterSnapshotLoad ();
		}
		if(Settings.SPC7110)
		{
			uint32 temp;
			fread(&s7r.bank50, 1,0x10000, fs);
			
			//NEWSYM SPCMultA, dd 0  4820-23
			fread(&temp, 1, 4, fs);
			
			s7r.reg4820=temp&(0x0FF);
			s7r.reg4821=(temp>>8)&(0x0FF);
			s7r.reg4822=(temp>>16)&(0x0FF);
			s7r.reg4823=(temp>>24)&(0x0FF);

			//NEWSYM SPCMultB, dd 0				4824-5
			fread(&temp, 1,4,fs);
			s7r.reg4824=temp&(0x0FF);
			s7r.reg4825=(temp>>8)&(0x0FF);


			//NEWSYM SPCDivEnd, dd 0				4826-7
			fread(&temp, 1,4,fs);
			s7r.reg4826=temp&(0x0FF);
			s7r.reg4827=(temp>>8)&(0x0FF);

			//NEWSYM SPCMulRes, dd 0				4828-B
			fread(&temp, 1, 4, fs);
			
			s7r.reg4828=temp&(0x0FF);
			s7r.reg4829=(temp>>8)&(0x0FF);
			s7r.reg482A=(temp>>16)&(0x0FF);
			s7r.reg482B=(temp>>24)&(0x0FF);
			
			//NEWSYM SPCDivRes, dd 0				482C-D
			fread(&temp, 1,4,fs);
			s7r.reg482C=temp&(0x0FF);
			s7r.reg482D=(temp>>8)&(0x0FF);

			//NEWSYM SPC7110BankA, dd 020100h		4831-3
			fread(&temp, 1, 4, fs);
			
			s7r.reg4831=temp&(0x0FF);
			s7r.reg4832=(temp>>8)&(0x0FF);
			s7r.reg4833=(temp>>16)&(0x0FF);
			
			//NEWSYM SPC7110RTCStat, dd 0			4840,init,command, index
			fread(&temp, 1, 4, fs);
			
			s7r.reg4840=temp&(0x0FF);

//NEWSYM SPC7110RTC, db 00,00,00,00,00,00,01,00,01,00,00,00,00,00,0Fh,00
fread(&temp, 1, 4, fs);
if(Settings.SPC7110RTC)
{
	rtc_f9.reg[0]=temp&(0x0FF);
	rtc_f9.reg[1]=(temp>>8)&(0x0FF);
	rtc_f9.reg[2]=(temp>>16)&(0x0FF);
	rtc_f9.reg[3]=(temp>>24)&(0x0FF);
}
fread(&temp, 1, 4, fs);
if(Settings.SPC7110RTC)
{
	rtc_f9.reg[4]=temp&(0x0FF);
	rtc_f9.reg[5]=(temp>>8)&(0x0FF);
	rtc_f9.reg[6]=(temp>>16)&(0x0FF);
	rtc_f9.reg[7]=(temp>>24)&(0x0FF);
}
fread(&temp, 1, 4, fs);
if(Settings.SPC7110RTC)
{
	rtc_f9.reg[8]=temp&(0x0FF);
	rtc_f9.reg[9]=(temp>>8)&(0x0FF);
	rtc_f9.reg[10]=(temp>>16)&(0x0FF);
	rtc_f9.reg[11]=(temp>>24)&(0x0FF);
}
fread(&temp, 1, 4, fs);
if(Settings.SPC7110RTC)
{
	rtc_f9.reg[12]=temp&(0x0FF);
	rtc_f9.reg[13]=(temp>>8)&(0x0FF);
	rtc_f9.reg[14]=(temp>>16)&(0x0FF);
	rtc_f9.reg[15]=(temp>>24)&(0x0FF);
}
//NEWSYM SPC7110RTCB, db 00,00,00,00,00,00,01,00,01,00,00,00,00,01,0Fh,06
fread(&temp, 1, 4, fs);
fread(&temp, 1, 4, fs);
fread(&temp, 1, 4, fs);
fread(&temp, 1, 4, fs);

//NEWSYM SPCROMPtr, dd 0		4811-4813
			fread(&temp, 1, 4, fs);
			
			s7r.reg4811=temp&(0x0FF);
			s7r.reg4812=(temp>>8)&(0x0FF);
			s7r.reg4813=(temp>>16)&(0x0FF);
//NEWSYM SPCROMtoI, dd SPCROMPtr
			fread(&temp, 1, 4, fs);
//NEWSYM SPCROMAdj, dd 0      4814-5
			fread(&temp, 1, 4, fs);
			s7r.reg4814=temp&(0x0FF);
			s7r.reg4815=(temp>>8)&(0x0FF);
//NEWSYM SPCROMInc, dd 0		4816-7
			fread(&temp, 1, 4, fs);
			s7r.reg4816=temp&(0x0FF);
			s7r.reg4817=(temp>>8)&(0x0FF);
//NEWSYM SPCROMCom, dd 0		4818
fread(&temp, 1, 4, fs);
			
			s7r.reg4818=temp&(0x0FF);
//NEWSYM SPCCompPtr, dd 0  4801-4804 (+b50i) if"manual"
			fread(&temp, 1, 4, fs);

			//do table check
			
			s7r.reg4801=temp&(0x0FF);
			s7r.reg4802=(temp>>8)&(0x0FF);
			s7r.reg4803=(temp>>16)&(0x0FF);
			s7r.reg4804=(temp>>24)&(0x0FF);
///NEWSYM SPCDecmPtr, dd 0  4805-6   +b50i
			fread(&temp, 1, 4, fs);
			s7r.reg4805=temp&(0x0FF);
			s7r.reg4806=(temp>>8)&(0x0FF);
//NEWSYM SPCCompCounter, dd 0  4809-A
			fread(&temp, 1, 4, fs);
			s7r.reg4809=temp&(0x0FF);
			s7r.reg480A=(temp>>8)&(0x0FF);
//NEWSYM SPCCompCommand, dd 0  480B
fread(&temp, 1, 4, fs);
			
			s7r.reg480B=temp&(0x0FF);
//NEWSYM SPCCheckFix, dd 0		written(if 1, then set writtne to max value!)
fread(&temp, 1, 4, fs);
(temp&(0x0FF))?s7r.written=0x1F:s7r.written=0x00;
//NEWSYM SPCSignedVal, dd 0	482E
fread(&temp, 1, 4, fs);
			
			s7r.reg482E=temp&(0x0FF);
			
		}
		fclose (fs);
		
		Memory.FixROMSpeed ();
		IPPU.ColorsChanged = TRUE;
		IPPU.OBJChanged = TRUE;
		CPU.InDMA = FALSE;
		S9xFixColourBrightness ();
		IPPU.RenderThisFrame = FALSE;
		
		S9xFixSoundAfterSnapshotLoad (1);
		ICPU.ShiftedPB = Registers.PB << 16;
		ICPU.ShiftedDB = Registers.DB << 16;
		S9xSetPCBase (ICPU.ShiftedPB + Registers.PC);
		S9xUnpackStatus ();
		S9xFixCycles ();
		S9xReschedule ();
#ifdef ZSNES_FX
		if (Settings.SuperFX)
			S9xSuperFXPostLoadState ();
#endif
		return (TRUE);
    }
    fclose (fs);
    return (FALSE);
}

