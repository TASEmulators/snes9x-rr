#ifndef _S9XLUA_H
#define _S9XLUA_H

enum LuaCallID
{
	LUACALL_BEFOREEMULATION, // also written in asmstruc.h for cpuexec.s!
	LUACALL_AFTEREMULATION,
	LUACALL_BEFOREEXIT,

	LUACALL_COUNT
};
EXTERN_C void CallRegisteredLuaFunctions(LuaCallID calltype);

// Just forward function declarations 

void S9xLuaWrite(uint32 addr);
void S9xLuaFrameBoundary();
int S9xLoadLuaCode(const char *filename);
int S9xReloadLuaCode();
void S9xLuaStop();
int S9xLuaRunning();

int S9xLuaUsingJoypad(int);
int S9xLuaReadJoypad(int);
int S9xLuaSpeed();
bool8 S9xLuaRerecordCountSkip();

void S9xLuaGui(void *, int width, int height, int bpp, int pitch);
EXTERN_C void S9xLuaClearGui();
void S9xLuaEnableGui(bool enabled);

struct lua_State* S9xGetLuaState();
char* S9xGetLuaScriptName(); // note: returns a pointer to a writable static buffer of _MAX_PATH length

// And some interesting REVERSE declarations!
char *S9xGetFreezeFilename(int slot);

// See getset.h!
//inline void S9xLuaWriteInform(uint32 address);


#endif
