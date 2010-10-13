// common subroutines for both ASM/C core

#include "port.h"
#include "snes9x.h"
#include "cpuexec.h"
#include "lua-engine.h"

bool8 Inside_Frame;

void OnFrameStart()
{
	IPPU.pad_read = false;

	CallRegisteredLuaFunctions(LUACALL_BEFOREEMULATION);
	Inside_Frame = true;
}

void OnFrameEnd()
{
	if (!IPPU.pad_read)
	{
		++IPPU.LagCounter;
	}
	IPPU.pad_read_last = IPPU.pad_read;

	Inside_Frame = false;
	CallRegisteredLuaFunctions(LUACALL_AFTEREMULATION);
}
