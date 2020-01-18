#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
//#include <unistd.h> // for unlink
#include <ctype.h>
#include <assert.h>
#include <math.h>
#ifdef WIN32
#include <direct.h>
#endif
#include <time.h>
#include <vector>
#include <map>
#include <string>
#include <algorithm>

#ifdef __linux
#include <sys/types.h>
#include <sys/wait.h>
#include <algorithm> // max() and min()
#include <sys/stat.h> // lstat
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _getcwd getcwd
#define _chdir chdir
#define max std::max
#define min std::min
#endif


#include "port.h"
#include "snes9x.h"
#include "memmap.h"
#include "display.h"
#include "messages.h"
#include "movie.h"
#include "snapshot.h"
#include "gfx.h"
#include "ppu.h"
#include "controls.h"
#include "65c816.h"
#include "apu.h"
#include "apumem.h"

extern "C" {
	#include "lua.h"
	#include "lauxlib.h"
	#include "lualib.h"
	#include "lstate.h"
}

#include "s9xlua.h"
#include "luasav.h"

#include "SFMT/SFMT.c"

#ifdef WIN32
#include <windows.h>
#include "win32/wsnes9x.h"
#include "win32/rsrc/resource.h"
#endif

extern void S9xReRefresh();

static void(*info_print)(int uid, const char* str);
static void(*info_onstart)(int uid);
static void(*info_onstop)(int uid);
static int info_uid;
#ifdef _WIN32
extern HWND LuaConsoleHWnd;
extern INT_PTR CALLBACK DlgLuaScriptDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
extern void PrintToWindowConsole(int hDlgAsInt, const char* str);
extern void WinLuaOnStart(int hDlgAsInt);
extern void WinLuaOnStop(int hDlgAsInt);
#endif

static lua_State *LUA;

// Are we running any code right now?
static char luaScriptName [_MAX_PATH] = {0};

// Current working directory of the script
static char luaCWD [_MAX_PATH] = {0};

// Are we running any code right now?
static bool8 luaRunning = FALSE;

// True at the frame boundary, false otherwise.
static bool8 frameBoundary = FALSE;


// The execution speed we're running at.
static enum {SPEED_NORMAL, SPEED_NOTHROTTLE, SPEED_TURBO, SPEED_MAXIMUM} speedmode = SPEED_NORMAL;

// Rerecord count skip mode
static bool8 skipRerecords = FALSE;

// Used by the registry to find our functions
static const char *frameAdvanceThread = "S9X.FrameAdvance";
static const char *guiCallbackTable = "S9X.GUI";

// True if there's a thread waiting to run after a run of frame-advance.
static bool8 frameAdvanceWaiting = FALSE;

// We save our pause status in the case of a natural death.
//static bool8 wasPaused = FALSE;

// Transparency strength. 255=opaque, 0=so transparent it's invisible
static int transparencyModifier = 255;


static bool8 gui_used = FALSE;
static bool8 gui_enabled = TRUE;
static uint8 *gui_data = NULL; // BGRA


// Protects Lua calls from going nuts.
// We set this to a big number like 1000 and decrement it
// over time. The script gets knifed once this reaches zero.
static int numTries;

// number of registered memory functions (1 per hooked byte)
static unsigned int numMemHooks;

// Look in snes9x.h for macros named like SNES9X_UP_MASK to determine the order.
static const char *button_mappings[] = {
	"","b1","b2","b3", // These are normally unused, but supported for custom input display
	
	"R", "L", "X", "A", "right", "left", "down", "up", "start", "select", "Y", "B"
};

#ifdef _MSC_VER
	#define snprintf _snprintf
	#define vscprintf _vscprintf
#else
	#define stricmp strcasecmp
	#define strnicmp strncasecmp
	#define __forceinline __attribute__((always_inline))
#endif


// these are necessary for properly handling 32-bit integers that are expected to exceed 0x7FFFFFFF.
// it depends on the platform and the luaconf.h Lua was compiled with, but defining these here/somewhere is safest.
static inline unsigned int lua_tounsignedinteger (lua_State *L, int idx) {
#if defined(LUA_NUMBER_DOUBLE) && !defined(LUA_ANSI) && !defined(__SSE2__) && (defined(__i386) || defined (_M_IX86) || defined(__i386__))
	volatile union luaui_Cast { double l_d; unsigned int l_ul; } u;
	u.l_d = lua_tonumber(L,idx) + 6755399441055744.0; // faster than regular casting, and still works for unsigned ints (unlike fld,fistp)
	return u.l_ul;
#else
	return (unsigned int)lua_tonumber(L,idx);
#endif
}
static inline unsigned int luaL_checkunsignedinteger (lua_State *L, int idx) {
	luaL_checktype(L, idx, LUA_TNUMBER);
	return lua_tounsignedinteger(L,idx);
}
static inline void lua_pushunsignedinteger (lua_State *L, unsigned int n) {
	lua_pushnumber(L, (lua_Number)n); // this is not slower than lua_pushinteger, anyway
}


static const char* luaCallIDStrings [] =
{
	"CALL_BEFOREEMULATION",
	"CALL_AFTEREMULATION",
	"CALL_BEFOREEXIT",
	"CALL_ONSTART",
};

//make sure we have the right number of strings
CTASSERT(sizeof(luaCallIDStrings)/sizeof(*luaCallIDStrings) == LUACALL_COUNT)

static const char* luaMemHookTypeStrings [] =
{
	"MEMHOOK_WRITE",
	"MEMHOOK_READ",
	"MEMHOOK_EXEC",

	"MEMHOOK_WRITE_SUB",
	"MEMHOOK_READ_SUB",
	"MEMHOOK_EXEC_SUB",
};

//make sure we have the right number of strings
CTASSERT(sizeof(luaMemHookTypeStrings)/sizeof(*luaMemHookTypeStrings) ==  LUAMEMHOOK_COUNT)

static char* rawToCString(lua_State* L, int idx=0);
static const char* toCString(lua_State* L, int idx=0);

char lua_spc_autosearch_filename[_MAX_PATH] = {0};

INLINE void S9xSetDWord (uint32 DWord, uint32 Address);
INLINE uint32 S9xGetDWord (uint32 Address);

INLINE uint32 S9xGetDWord (uint32 Address)
{
	enum s9xwrap_t w = WRAP_NONE;
	bool free = true;
	uint32 ret;

	ret = S9xGetWord(Address, w, free);
	ret |= (S9xGetWord(Address+2, w, free) << 16);
	return ret;
}

INLINE void S9xSetDWord (uint32 DWord, uint32 Address)
{
	enum s9xwrap_t w = WRAP_NONE;
	enum s9xwriteorder_t o = WRITE_01;
	bool free = true;

	S9xSetWordWrapped(DWord & 0xffff, Address, w, o, free);
	CallRegisteredLuaMemHook(Address, 2, DWord & 0xffff, LUAMEMHOOK_WRITE);
	S9xSetWordWrapped(DWord >> 16, Address+2, w, o, free);
	CallRegisteredLuaMemHook(Address, 2, DWord >> 16, LUAMEMHOOK_WRITE);
}


// PERIPHERAL_SUPPORT
#define SNESMOUSE_LEFT  0x40
#define SNESMOUSE_RIGHT 0x80
#define SUPERSCOPE_FIRE       0x80
#define SUPERSCOPE_CURSOR     0x40
#define SUPERSCOPE_TURBO      0x20
#define SUPERSCOPE_PAUSE      0x10
#define SUPERSCOPE_OFFSCREEN  0x02
#define JUSTIFIER_TRIGGER    0x80
#define JUSTIFIER_START      0x20
#define JUSTIFIER_SELECT     0x08
#define JUSTIFIER_OFFSCREEN  0x02
#define JUSTIFIER2_TRIGGER   0x40
#define JUSTIFIER2_START     0x10
#define JUSTIFIER2_SELECT    0x04
#define JUSTIFIER2_OFFSCREEN 0x01
void AddCommandTransformAxis(controllers type, int idx, int16 val, bool8 axis);
void AddCommandTransformButton(controllers type, int idx, bool8 on, uint8 mask);
void ClearCommandTransforms();



/**
 * Resets emulator speed / pause states after script exit.
 */
static void S9xLuaOnStop() {
	luaRunning = FALSE;
	ClearCommandTransforms();
	gui_used = false;
	//if (wasPaused)
	//	Settings.Paused = TRUE;
}

/**
 * Asks Lua if it wants control of the emulator's speed.
 * Returns 0 if no, 1 if yes. If yes, we also tamper with the
 * IPPU's settings for speed ourselves, so the calling code
 * need not do anything.
 */
int S9xLuaSpeed() {
	if (!LUA || !luaRunning)
		return 0;

	//printf("%d\n", speedmode);

	switch (speedmode) {
	case SPEED_NORMAL:
		return 0;
	case SPEED_NOTHROTTLE:
		IPPU.RenderThisFrame = TRUE;
		return 1;

	case SPEED_TURBO:
		IPPU.SkippedFrames++;
		if (IPPU.SkippedFrames >= 40) {
			IPPU.SkippedFrames = 0;
			IPPU.RenderThisFrame = TRUE;
		}
		else
			IPPU.RenderThisFrame = FALSE;
		return 1;

	// In mode 3, SkippedFrames is set to zero so that the frame
	// skipping code doesn't try anything funny.
	case SPEED_MAXIMUM:
		IPPU.SkippedFrames=0;
		IPPU.RenderThisFrame = FALSE;
		return 1;

	default:
		assert(false);
		return 0;
	
	}
}

///////////////////////////

// snes9x.speedmode(string mode)
//
//   Takes control of the emulation speed
//   of the system. Normal is normal speed (60fps, 50 for PAL),
//   nothrottle disables speed control but renders every frame,
//   turbo renders only a few frames in order to speed up emulation,
//   maximum renders no frames
static int snes9x_speedmode(lua_State *L) {
	const char *mode = luaL_checkstring(L,1);
	
	if (strcasecmp(mode, "normal")==0) {
		speedmode = SPEED_NORMAL;
	} else if (strcasecmp(mode, "nothrottle")==0) {
		speedmode = SPEED_NOTHROTTLE;
	} else if (strcasecmp(mode, "turbo")==0) {
		speedmode = SPEED_TURBO;
	} else if (strcasecmp(mode, "maximum")==0) {
		speedmode = SPEED_MAXIMUM;
	} else
		luaL_error(L, "Invalid mode %s to snes9x.speedmode",mode);
	
	//printf("new speed mode:  %d\n", speedmode);

	return 0;

}


// snes9x.frameadvnace()
//
//  Executes a frame advance. Occurs by yielding the coroutine, then re-running
//  when we break out.
static int snes9x_frameadvance(lua_State *L) {
	// We're going to sleep for a frame-advance. Take notes.

	if (frameAdvanceWaiting) 
		return luaL_error(L, "can't call snes9x.frameadvance() from here");

	frameAdvanceWaiting = TRUE;

	// Don't do this! The user won't like us sending their emulator out of control!
//	Settings.FrameAdvance = TRUE;
	
	// Now we can yield to the main 
	return lua_yield(L, 0);


	// It's actually rather disappointing...
}


// snes9x.pause()
//
//  Pauses the emulator, function "waits" until the user unpauses.
//  This function MAY be called from a non-frame boundary, but the frame
//  finishes executing anwyays. In this case, the function returns immediately.
static int snes9x_pause(lua_State *L) {
	
	Settings.Paused = TRUE;
	speedmode = SPEED_NORMAL;

	// Return control if we're midway through a frame. We can't pause here.
	if (frameAdvanceWaiting) {
		return 0;
	}

	// If it's on a frame boundary, we also yield.	
	frameAdvanceWaiting = TRUE;
	return lua_yield(L, 0);
	
}

static int snes9x_registerbefore(lua_State *L) {
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_settop(L,1);
	lua_getfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_BEFOREEMULATION]);
	lua_insert(L,1);
	lua_setfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_BEFOREEMULATION]);
	//StopScriptIfFinished(luaStateToUIDMap[L]);
	return 1;
}

static int snes9x_registerafter(lua_State *L) {
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_settop(L,1);
	lua_getfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_AFTEREMULATION]);
	lua_insert(L,1);
	lua_setfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_AFTEREMULATION]);
	//StopScriptIfFinished(luaStateToUIDMap[L]);
	return 1;
}

static int snes9x_registerexit(lua_State *L) {
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_settop(L,1);
	lua_getfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_BEFOREEXIT]);
	lua_insert(L,1);
	lua_setfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_BEFOREEXIT]);
	//StopScriptIfFinished(luaStateToUIDMap[L]);
	return 1;
}

static int snes9x_registerstart(lua_State *L) {
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_settop(L,1);
	lua_getfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_ONSTART]);
	lua_insert(L,1);
	lua_pushvalue(L,-1); // copy the function so we can also call it
	lua_setfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_ONSTART]);
	if (!lua_isnil(L,-1) && !Settings.StopEmulation)
		lua_call(L,0,0); // call the function now since the game has already started and this start function hasn't been called yet
	//StopScriptIfFinished(luaStateToUIDMap[L]);
	return 1;
}

static inline bool isalphaorunderscore(char c)
{
	return isalpha(c) || c == '_';
}

static std::vector<const void*> s_tableAddressStack; // prevents infinite recursion of a table within a table (when cycle is found, print something like table:parent)
static std::vector<const void*> s_metacallStack; // prevents infinite recursion if something's __tostring returns another table that contains that something (when cycle is found, print the inner result without using __tostring)

#define APPENDPRINT { int _n = snprintf(ptr, remaining,
#define END ); if(_n >= 0) { ptr += _n; remaining -= _n; } else { remaining = 0; } }
static void toCStringConverter(lua_State* L, int i, char*& ptr, int& remaining)
{
	if(remaining <= 0)
		return;

	const char* str = ptr; // for debugging

	// if there is a __tostring metamethod then call it
	int usedMeta = luaL_callmeta(L, i, "__tostring");
	if(usedMeta)
	{
		std::vector<const void*>::const_iterator foundCycleIter = std::find(s_metacallStack.begin(), s_metacallStack.end(), lua_topointer(L,i));
		if(foundCycleIter != s_metacallStack.end())
		{
			lua_pop(L, 1);
			usedMeta = false;
		}
		else
		{
			s_metacallStack.push_back(lua_topointer(L,i));
			i = lua_gettop(L);
		}
	}

	switch(lua_type(L, i))
	{
		case LUA_TNONE: break;
		case LUA_TNIL: APPENDPRINT "nil" END break;
		case LUA_TBOOLEAN: APPENDPRINT lua_toboolean(L,i) ? "true" : "false" END break;
		case LUA_TSTRING: APPENDPRINT "%s",lua_tostring(L,i) END break;
		case LUA_TNUMBER: APPENDPRINT "%.12Lg",lua_tonumber(L,i) END break;
		case LUA_TFUNCTION: 
			if((L->base + i-1)->value.gc->cl.c.isC)
			{
				//lua_CFunction func = lua_tocfunction(L, i);
				//std::map<lua_CFunction, const char*>::iterator iter = s_cFuncInfoMap.find(func);
				//if(iter == s_cFuncInfoMap.end())
					goto defcase;
				//APPENDPRINT "function(%s)", iter->second END 
			}
			else
			{
				APPENDPRINT "function(" END 
				Proto* p = (L->base + i-1)->value.gc->cl.l.p;
				int numParams = p->numparams + (p->is_vararg?1:0);
				for (int n=0; n<p->numparams; n++)
				{
					APPENDPRINT "%s", getstr(p->locvars[n].varname) END 
					if(n != numParams-1)
						APPENDPRINT "," END
				}
				if(p->is_vararg)
					APPENDPRINT "..." END
				APPENDPRINT ")" END
			}
			break;
defcase:default: APPENDPRINT "%s:%p",luaL_typename(L,i),lua_topointer(L,i) END break;
		case LUA_TTABLE:
		{
			// first make sure there's enough stack space
			if(!lua_checkstack(L, 4))
			{
				// note that even if lua_checkstack never returns false,
				// that doesn't mean we didn't need to call it,
				// because calling it retrieves stack space past LUA_MINSTACK
				goto defcase;
			}

			std::vector<const void*>::const_iterator foundCycleIter = std::find(s_tableAddressStack.begin(), s_tableAddressStack.end(), lua_topointer(L,i));
			if(foundCycleIter != s_tableAddressStack.end())
			{
				int parentNum = s_tableAddressStack.end() - foundCycleIter;
				if(parentNum > 1)
					APPENDPRINT "%s:parent^%d",luaL_typename(L,i),parentNum END
				else
					APPENDPRINT "%s:parent",luaL_typename(L,i) END
			}
			else
			{
				s_tableAddressStack.push_back(lua_topointer(L,i));
				struct Scope { ~Scope(){ s_tableAddressStack.pop_back(); } } scope;

				APPENDPRINT "{" END

				lua_pushnil(L); // first key
				int keyIndex = lua_gettop(L);
				int valueIndex = keyIndex + 1;
				bool first = true;
				bool skipKey = true; // true if we're still in the "array part" of the table
				lua_Number arrayIndex = (lua_Number)0;
				while(lua_next(L, i))
				{
					if(first)
						first = false;
					else
						APPENDPRINT ", " END
					if(skipKey)
					{
						arrayIndex += (lua_Number)1;
						bool keyIsNumber = (lua_type(L, keyIndex) == LUA_TNUMBER);
						skipKey = keyIsNumber && (lua_tonumber(L, keyIndex) == arrayIndex);
					}
					if(!skipKey)
					{
						bool keyIsString = (lua_type(L, keyIndex) == LUA_TSTRING);
						bool invalidLuaIdentifier = (!keyIsString || !isalphaorunderscore(*lua_tostring(L, keyIndex)));
						if(invalidLuaIdentifier)
							if(keyIsString)
								APPENDPRINT "['" END
							else
								APPENDPRINT "[" END

						toCStringConverter(L, keyIndex, ptr, remaining); // key

						if(invalidLuaIdentifier)
							if(keyIsString)
								APPENDPRINT "']=" END
							else
								APPENDPRINT "]=" END
						else
							APPENDPRINT "=" END
					}

					bool valueIsString = (lua_type(L, valueIndex) == LUA_TSTRING);
					if(valueIsString)
						APPENDPRINT "'" END

					toCStringConverter(L, valueIndex, ptr, remaining); // value

					if(valueIsString)
						APPENDPRINT "'" END

					lua_pop(L, 1);

					if(remaining <= 0)
					{
						lua_settop(L, keyIndex-1); // stack might not be clean yet if we're breaking early
						break;
					}
				}
				APPENDPRINT "}" END
			}
		}	break;
	}

	if(usedMeta)
	{
		s_metacallStack.pop_back();
		lua_pop(L, 1);
	}
}

static const int s_tempStrMaxLen = 64 * 1024;
static char s_tempStr [s_tempStrMaxLen];

static char* rawToCString(lua_State* L, int idx)
{
	int a = idx>0 ? idx : 1;
	int n = idx>0 ? idx : lua_gettop(L);

	char* ptr = s_tempStr;
	*ptr = 0;

	int remaining = s_tempStrMaxLen;
	for(int i = a; i <= n; i++)
	{
		toCStringConverter(L, i, ptr, remaining);
		if(i != n)
			APPENDPRINT " " END
	}

	if(remaining < 3)
	{
		while(remaining < 6)
			remaining++, ptr--;
		APPENDPRINT "..." END
	}
	APPENDPRINT "\r\n" END
	// the trailing newline is so print() can avoid having to do wasteful things to print its newline
	// (string copying would be wasteful and calling info.print() twice can be extremely slow)
	// at the cost of functions that don't want the newline needing to trim off the last two characters
	// (which is a very fast operation and thus acceptable in this case)

	return s_tempStr;
}
#undef APPENDPRINT
#undef END


// replacement for luaB_tostring() that is able to show the contents of tables (and formats numbers better, and show function prototypes)
// can be called directly from lua via tostring(), assuming tostring hasn't been reassigned
static int tostring(lua_State *L)
{
	char* str = rawToCString(L);
	str[strlen(str)-2] = 0; // hack: trim off the \r\n (which is there to simplify the print function's task)
	lua_pushstring(L, str);
	return 1;
}

// like rawToCString, but will check if the global Lua function tostring()
// has been replaced with a custom function, and call that instead if so
static const char* toCString(lua_State* L, int idx)
{
	int a = idx>0 ? idx : 1;
	int n = idx>0 ? idx : lua_gettop(L);
	lua_getglobal(L, "tostring");
	lua_CFunction cf = lua_tocfunction(L,-1);
	if(cf == tostring) // optimization: if using our own C tostring function, we can bypass the call through Lua and all the string object allocation that would entail
	{
		lua_pop(L,1);
		return rawToCString(L, idx);
	}
	else // if the user overrided the tostring function, we have to actually call it and store the temporarily allocated string it returns
	{
		lua_pushstring(L, "");
		for (int i=a; i<=n; i++) {
			lua_pushvalue(L, -2);  // function to be called
			lua_pushvalue(L, i);   // value to print
			lua_call(L, 1, 1);
			if(lua_tostring(L, -1) == NULL)
				luaL_error(L, LUA_QL("tostring") " must return a string to " LUA_QL("print"));
			lua_pushstring(L, (i<n) ? " " : "\r\n");
			lua_concat(L, 3);
		}
		const char* str = lua_tostring(L, -1);
		strncpy(s_tempStr, str, s_tempStrMaxLen);
		s_tempStr[s_tempStrMaxLen-1] = 0;
		lua_pop(L, 2);
		return s_tempStr;
	}
}

static const char* deferredJoySetIDString = "lazyjoy";
#define MAX_DEFERRED_COUNT 16384

// store the most recent C function call from Lua (and all its arguments)
// for later evaluation
void DeferFunctionCall(lua_State* L, const char* idstring)
{
	// there might be a cleaner way of doing this using lua_pushcclosure and lua_getref

	int num = lua_gettop(L);

	// get the C function pointer
	//lua_CFunction cf = lua_tocfunction(L, -(num+1));
	lua_CFunction cf = (L->ci->func)->value.gc->cl.c.f;
	assert(cf);
	lua_pushcfunction(L,cf);

	// make a list of the function and its arguments (and also pop those arguments from the stack)
	lua_createtable(L, num+1, 0);
	lua_insert(L, 1);
	for(int n = num+1; n > 0; n--)
		lua_rawseti(L, 1, n);

	// put the list into a global array
	lua_getfield(L, LUA_REGISTRYINDEX, idstring);
	lua_insert(L, 1);
	int curSize = lua_objlen(L, 1);
	lua_rawseti(L, 1, curSize+1);

	// clean the stack
	lua_settop(L, 0);
}
void CallDeferredFunctions(lua_State* L, const char* idstring)
{
	lua_settop(L, 0);
	lua_getfield(L, LUA_REGISTRYINDEX, idstring);
	int numCalls = lua_objlen(L, 1);
	for(int i = 1; i <= numCalls; i++)
	{
        lua_rawgeti(L, 1, i);  // get the function+arguments list
		int listSize = lua_objlen(L, 2);

		// push the arguments and the function
		for(int j = 1; j <= listSize; j++)
			lua_rawgeti(L, 2, j);

		// get and pop the function
		lua_CFunction cf = lua_tocfunction(L, -1);
		lua_pop(L, 1);

		// shift first argument to slot 1 and call the function
		lua_remove(L, 2);
		lua_remove(L, 1);
		cf(L);

		// prepare for next iteration
		lua_settop(L, 0);
		lua_getfield(L, LUA_REGISTRYINDEX, idstring);
	}

	// clear the list of deferred functions
	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, idstring);
	//LuaContextInfo& info = GetCurrentInfo();

	// clean the stack
	lua_settop(L, 0);
}

// replacement for luaB_print() that goes to the appropriate textbox instead of stdout
static int print(lua_State *L)
{
	const char* str = toCString(L);

	int uid = info_uid;//luaStateToUIDMap[L->l_G->mainthread];
	//LuaContextInfo& info = GetCurrentInfo();

	if(info_print)
		info_print(uid, str);
	else
		puts(str);

	//worry(L, 100);
	return 0;
}

// snes9x.message(string msg)
//
//  Displays the given message on the screen.
static int snes9x_message(lua_State *L) {
	const char *msg = luaL_checkstring(L,1);
	static char msgBuf[1024];
	strcpy(msgBuf, msg);
	S9xMessage(S9X_INFO, S9X_MOVIE_INFO, msgBuf);
	
	return 0;

}

// provides an easy way to copy a table from Lua
// (simple assignment only makes an alias, but sometimes an independent table is desired)
// currently this function only performs a shallow copy,
// but I think it should be changed to do a deep copy (possibly of configurable depth?)
// that maintains the internal table reference structure
static int copytable(lua_State *L)
{
	int origIndex = 1; // we only care about the first argument
	int origType = lua_type(L, origIndex);
	if(origType == LUA_TNIL)
	{
		lua_pushnil(L);
		return 1;
	}
	if(origType != LUA_TTABLE)
	{
		luaL_typerror(L, 1, lua_typename(L, LUA_TTABLE));
		lua_pushnil(L);
		return 1;
	}
	
	lua_createtable(L, lua_objlen(L,1), 0);
	int copyIndex = lua_gettop(L);

	lua_pushnil(L); // first key
	int keyIndex = lua_gettop(L);
	int valueIndex = keyIndex + 1;

	while(lua_next(L, origIndex))
	{
		lua_pushvalue(L, keyIndex);
		lua_pushvalue(L, valueIndex);
		lua_rawset(L, copyIndex); // copytable[key] = value
		lua_pop(L, 1);
	}

	// copy the reference to the metatable as well, if any
	if(lua_getmetatable(L, origIndex))
		lua_setmetatable(L, copyIndex);

	return 1; // return the new table
}

// because print traditionally shows the address of tables,
// and the print function I provide instead shows the contents of tables,
// I also provide this function
// (otherwise there would be no way to see a table's address, AFAICT)
static int addressof(lua_State *L)
{
	const void* ptr = lua_topointer(L,-1);
	lua_pushinteger(L, (lua_Integer)ptr);
	return 1;
}

struct registerPointerMap
{
	const char* registerName;
	unsigned int* pointer;
	int dataSize;
};

#define RPM_ENTRY(name,var) {name, (unsigned int*)&var, sizeof(var)},

registerPointerMap a65c816PointerMap [] = {
	RPM_ENTRY("db", Registers.DB)
	RPM_ENTRY("p", Registers.PL)
	RPM_ENTRY("e", Registers.PH) // 1bit flag, how should lua engine handle it?
	RPM_ENTRY("a", Registers.A.W)
	RPM_ENTRY("d", Registers.D.W)
	RPM_ENTRY("s", Registers.S.W)
	RPM_ENTRY("x", Registers.X.W)
	RPM_ENTRY("y", Registers.Y.W)
	RPM_ENTRY("pb", Registers.PB)
	RPM_ENTRY("pc", Registers.PCw)
	RPM_ENTRY("pbpc", Registers.PBPC)
	{}
};
registerPointerMap sa1PointerMap [] = {
	RPM_ENTRY("db", SA1Registers.DB)
	RPM_ENTRY("p", SA1Registers.PL)
	RPM_ENTRY("e", SA1Registers.PH) // 1bit flag, how should lua engine handle it?
	RPM_ENTRY("a", SA1Registers.A.W)
	RPM_ENTRY("d", SA1Registers.D.W)
	RPM_ENTRY("s", SA1Registers.S.W)
	RPM_ENTRY("x", SA1Registers.X.W)
	RPM_ENTRY("y", SA1Registers.Y.W)
	RPM_ENTRY("pb", SA1Registers.PB)
	RPM_ENTRY("pc", SA1Registers.PCw)
	RPM_ENTRY("pbpc", SA1Registers.PBPC)
	{}
};

struct cpuToRegisterMap
{
	const char* cpuName;
	registerPointerMap* rpmap;
}
cpuToRegisterMaps [] =
{
	{"65c816.", a65c816PointerMap},
	{"main.", a65c816PointerMap},
	{"sa1.", sa1PointerMap},
	{"", a65c816PointerMap},
};


//DEFINE_LUA_FUNCTION(memory_getregister, "cpu_dot_registername_string")
static int memory_getregister(lua_State *L)
{
	const char* qualifiedRegisterName = luaL_checkstring(L,1);
	lua_settop(L,0);
	for(int cpu = 0; cpu < sizeof(cpuToRegisterMaps)/sizeof(*cpuToRegisterMaps); cpu++)
	{
		cpuToRegisterMap ctrm = cpuToRegisterMaps[cpu];
		int cpuNameLen = strlen(ctrm.cpuName);
		if(!strnicmp(qualifiedRegisterName, ctrm.cpuName, cpuNameLen))
		{
			qualifiedRegisterName += cpuNameLen;
			for(int reg = 0; ctrm.rpmap[reg].dataSize; reg++)
			{
				registerPointerMap rpm = ctrm.rpmap[reg];
				if(!stricmp(qualifiedRegisterName, rpm.registerName))
				{
					switch(rpm.dataSize)
					{ default:
					case 1: lua_pushinteger(L, *(unsigned char*)rpm.pointer); break;
					case 2: lua_pushinteger(L, *(unsigned short*)rpm.pointer); break;
					case 4: lua_pushinteger(L, *(unsigned long*)rpm.pointer); break;
					}
					return 1;
				}
			}
			lua_pushnil(L);
			return 1;
		}
	}
	lua_pushnil(L);
	return 1;
}
//DEFINE_LUA_FUNCTION(memory_setregister, "cpu_dot_registername_string,value")
static int memory_setregister(lua_State *L)
{
	const char* qualifiedRegisterName = luaL_checkstring(L,1);
	unsigned long value = (unsigned long)(luaL_checkinteger(L,2));
	lua_settop(L,0);
	for(int cpu = 0; cpu < sizeof(cpuToRegisterMaps)/sizeof(*cpuToRegisterMaps); cpu++)
	{
		cpuToRegisterMap ctrm = cpuToRegisterMaps[cpu];
		int cpuNameLen = strlen(ctrm.cpuName);
		if(!strnicmp(qualifiedRegisterName, ctrm.cpuName, cpuNameLen))
		{
			qualifiedRegisterName += cpuNameLen;
			for(int reg = 0; ctrm.rpmap[reg].dataSize; reg++)
			{
				registerPointerMap rpm = ctrm.rpmap[reg];
				if(!stricmp(qualifiedRegisterName, rpm.registerName))
				{
					switch(rpm.dataSize)
					{ default:
					case 1: *(unsigned char*)rpm.pointer = (unsigned char)(value & 0xFF); break;
					case 2: *(unsigned short*)rpm.pointer = (unsigned short)(value & 0xFFFF); break;
					case 4: *(unsigned long*)rpm.pointer = value; break;
					}
					return 0;
				}
			}
			return 0;
		}
	}
	return 0;
}

void HandleCallbackError(lua_State* L)
{
	if(L->errfunc || L->errorJmp)
		luaL_error(L, "%s", lua_tostring(L,-1));
	else {
		lua_pushnil(LUA);
		lua_setfield(LUA, LUA_REGISTRYINDEX, guiCallbackTable);

		// Error?
#ifdef WIN32
		MessageBox( GUI.hWnd, lua_tostring(LUA,-1), "Lua run error", MB_OK | MB_ICONSTOP);
#else
		fprintf(stderr, "Lua thread bombed out: %s\n", lua_tostring(LUA,-1));
#endif

		S9xLuaStop();
	}
}

bool joy_set_queue = true;
void CallRegisteredLuaFunctions(LuaCallID calltype)
{
	assert((unsigned int)calltype < (unsigned int)LUACALL_COUNT);
	const char* idstring = luaCallIDStrings[calltype];

	if (!LUA)
		return;

	if(calltype == LUACALL_BEFOREEMULATION)
		joy_set_queue = false;

	if(calltype == LUACALL_BEFOREEMULATION)
		CallDeferredFunctions(LUA, deferredJoySetIDString);

	lua_settop(LUA, 0);
	lua_getfield(LUA, LUA_REGISTRYINDEX, idstring);

	int errorcode = 0;
	if (lua_isfunction(LUA, -1))
	{
		errorcode = lua_pcall(LUA, 0, 0, 0);
		if(calltype == LUACALL_BEFOREEMULATION)
			joy_set_queue = true;
		if (errorcode)
			HandleCallbackError(LUA);
	}
	else
	{
		lua_pop(LUA, 1);
		if(calltype == LUACALL_BEFOREEMULATION)
			joy_set_queue = true;
	}
}

// the purpose of this structure is to provide a way of
// QUICKLY determining whether a memory address range has a hook associated with it,
// with a bias toward fast rejection because the majority of addresses will not be hooked.
// (it must not use any part of Lua or perform any per-script operations,
//  otherwise it would definitely be too slow.)
// calculating the regions when a hook is added/removed may be slow,
// but this is an intentional tradeoff to obtain a high speed of checking during later execution
struct TieredRegion
{
	template<unsigned int maxGap>
	struct Region
	{
		struct Island
		{
			unsigned int start;
			unsigned int end;
			__forceinline bool Contains(unsigned int address, int size) const { return address < end && address+size > start; }
		};
		std::vector<Island> islands;

		void Calculate(const std::vector<unsigned int>& bytes)
		{
			islands.clear();

			unsigned int lastEnd = ~0;

			std::vector<unsigned int>::const_iterator iter = bytes.begin();
			std::vector<unsigned int>::const_iterator end = bytes.end();
			for(; iter != end; ++iter)
			{
				unsigned int addr = *iter;
				if(addr < lastEnd || addr > lastEnd + (long long)maxGap)
				{
					islands.push_back(Island());
					islands.back().start = addr;
				}
				islands.back().end = addr+1;
				lastEnd = addr+1;
			}
		}
		bool Contains(unsigned int address, int size) const
		{
            for (size_t i = 0; i != islands.size(); ++i)
            {
                if (islands[i].Contains(address, size))
                    return true;
            }
			return false;
		}
	};

	Region<0xFFFFFFFF> broad;
	Region<0x1000> mid;
	Region<0> narrow;

	void Calculate(std::vector<unsigned int>& bytes)
	{
		std::sort(bytes.begin(), bytes.end());

		broad.Calculate(bytes);
		mid.Calculate(bytes);
		narrow.Calculate(bytes);
	}

	TieredRegion()
	{
        std::vector <unsigned int> temp;
		Calculate(temp);
	}

	__forceinline int NotEmpty()
	{
		return broad.islands.size();
	}

	// note: it is illegal to call this if NotEmpty() returns 0
	__forceinline bool Contains(unsigned int address, int size)
	{
		return broad.islands[0].Contains(address,size) &&
		       mid.Contains(address,size) &&
			   narrow.Contains(address,size);
	}
};
TieredRegion hookedRegions [LUAMEMHOOK_COUNT];


static void CalculateMemHookRegions(LuaMemHookType hookType)
{
	std::vector<unsigned int> hookedBytes;
//	std::map<int, LuaContextInfo*>::iterator iter = luaContextInfo.begin();
//	std::map<int, LuaContextInfo*>::iterator end = luaContextInfo.end();
//	while(iter != end)
//	{
//		LuaContextInfo& info = *iter->second;
		if(/*info.*/ numMemHooks)
		{
			lua_State* L = LUA /*info.L*/;
			if(L)
			{
				lua_settop(L, 0);
				lua_getfield(L, LUA_REGISTRYINDEX, luaMemHookTypeStrings[hookType]);
				lua_pushnil(L);
				while(lua_next(L, -2))
				{
					if(lua_isfunction(L, -1))
					{
						unsigned int addr = lua_tointeger(L, -2);
						hookedBytes.push_back(addr);
					}
					lua_pop(L, 1);
				}
				lua_settop(L, 0);
			}
		}
//		++iter;
//	}
	hookedRegions[hookType].Calculate(hookedBytes);
}

static void CallRegisteredLuaMemHook_LuaMatch(unsigned int address, int size, unsigned int value, LuaMemHookType hookType)
{
//	std::map<int, LuaContextInfo*>::iterator iter = luaContextInfo.begin();
//	std::map<int, LuaContextInfo*>::iterator end = luaContextInfo.end();
//	while(iter != end)
//	{
//		LuaContextInfo& info = *iter->second;
		if(/*info.*/ numMemHooks)
		{
			lua_State* L = LUA /*info.L*/;
			if(L/* && !info.panic*/)
			{
#ifdef USE_INFO_STACK
				infoStack.insert(infoStack.begin(), &info);
				struct Scope { ~Scope(){ infoStack.erase(infoStack.begin()); } } scope;
#endif
				lua_settop(L, 0);
				lua_getfield(L, LUA_REGISTRYINDEX, luaMemHookTypeStrings[hookType]);
				for(int i = address; i != address+size; i++)
				{
					lua_rawgeti(L, -1, i);
					if (lua_isfunction(L, -1))
					{
						bool wasRunning = (luaRunning!=0) /*info.running*/;
						luaRunning /*info.running*/ = true;
						//RefreshScriptSpeedStatus();
						lua_pushinteger(L, address);
						lua_pushinteger(L, size);
						int errorcode = lua_pcall(L, 2, 0, 0);
						luaRunning /*info.running*/ = wasRunning;
						//RefreshScriptSpeedStatus();
						if (errorcode)
						{
							HandleCallbackError(L);
							//int uid = iter->first;
							//HandleCallbackError(L,info,uid,true);
						}
						break;
					}
					else
					{
						lua_pop(L,1);
					}
				}
				lua_settop(L, 0);
			}
		}
//		++iter;
//	}
}
void CallRegisteredLuaMemHook(unsigned int address, int size, unsigned int value, LuaMemHookType hookType)
{
	// performance critical! (called VERY frequently)
	// I suggest timing a large number of calls to this function in Release if you change anything in here,
	// before and after, because even the most innocent change can make it become 30% to 400% slower.
	// a good amount to test is: 100000000 calls with no hook set, and another 100000000 with a hook set.
	// (on my system that consistently took 200 ms total in the former case and 350 ms total in the latter case)
	if(hookedRegions[hookType].NotEmpty())
	{
		// TODO: add more mirroring
		if(address >= 0x0000 && address <= 0x1FFF)
			address |= 0x7E0000; // account for mirroring of LowRAM

		if(hookedRegions[hookType].Contains(address, size))
			CallRegisteredLuaMemHook_LuaMatch(address, size, value, hookType); // something has hooked this specific address
	}
}

static int memory_registerHook(lua_State* L, LuaMemHookType hookType, int defaultSize)
{
	// get first argument: address
	unsigned int addr = luaL_checkinteger(L,1);
	//if((addr & ~0xFFFFFF) == ~0xFFFFFF)
	//	addr &= 0xFFFFFF;

	// get optional second argument: size
	int size = defaultSize;
	int funcIdx = 2;
	if(lua_isnumber(L,2))
	{
		size = luaL_checkinteger(L,2);
		if(size < 0)
		{
			size = -size;
			addr -= size;
		}
		funcIdx++;
	}

	// check last argument: callback function
	bool clearing = lua_isnil(L,funcIdx);
	if(!clearing)
		luaL_checktype(L, funcIdx, LUA_TFUNCTION);
	lua_settop(L,funcIdx);

	// get the address-to-callback table for this hook type of the current script
	lua_getfield(L, LUA_REGISTRYINDEX, luaMemHookTypeStrings[hookType]);

	// count how many callback functions we'll be displacing
	int numFuncsAfter = clearing ? 0 : size;
	int numFuncsBefore = 0;
	for(unsigned int i = addr; i != addr+size; i++)
	{
		lua_rawgeti(L, -1, i);
		if(lua_isfunction(L, -1))
			numFuncsBefore++;
		lua_pop(L,1);
	}

	// put the callback function in the address slots
	for(unsigned int i = addr; i != addr+size; i++)
	{
		lua_pushvalue(L, -2);
		lua_rawseti(L, -2, i);
	}

	// adjust the count of active hooks
	//LuaContextInfo& info = GetCurrentInfo();
	/*info.*/ numMemHooks += numFuncsAfter - numFuncsBefore;

	// re-cache regions of hooked memory across all scripts
	CalculateMemHookRegions(hookType);

	//StopScriptIfFinished(luaStateToUIDMap[L]);
	return 0;
}

LuaMemHookType MatchHookTypeToCPU(lua_State* L, LuaMemHookType hookType)
{
	int cpuID = 0;

	int cpunameIndex = 0;
	if(lua_type(L,2) == LUA_TSTRING)
		cpunameIndex = 2;
	else if(lua_type(L,3) == LUA_TSTRING)
		cpunameIndex = 3;

	if(cpunameIndex)
	{
		const char* cpuName = lua_tostring(L, cpunameIndex);
		if(!stricmp(cpuName, "sub"))
			cpuID = 1;
		lua_remove(L, cpunameIndex);
	}

	switch(cpuID)
	{
	case 0:
		return hookType;

	case 1:
		switch(hookType)
		{
		case LUAMEMHOOK_WRITE: return LUAMEMHOOK_WRITE_SUB;
		case LUAMEMHOOK_READ: return LUAMEMHOOK_READ_SUB;
		case LUAMEMHOOK_EXEC: return LUAMEMHOOK_EXEC_SUB;
		}
	}
	return hookType;
}

static int memory_registerwrite(lua_State *L)
{
	return memory_registerHook(L, MatchHookTypeToCPU(L,LUAMEMHOOK_WRITE), 1);
}
static int memory_registerread(lua_State *L)
{
	return memory_registerHook(L, MatchHookTypeToCPU(L,LUAMEMHOOK_READ), 1);
}
static int memory_registerexec(lua_State *L)
{
	return memory_registerHook(L, MatchHookTypeToCPU(L,LUAMEMHOOK_EXEC), 1);
}

static int memory_readbyte(lua_State *L)
{
	lua_pushinteger(L, S9xGetByte(luaL_checkinteger(L,1), true));
	return 1;
}

static int memory_readbytesigned(lua_State *L) {
	signed char c = (signed char) S9xGetByte(luaL_checkinteger(L,1), true);
	lua_pushinteger(L, c);
	return 1;
}

static int memory_readword(lua_State *L)
{
	lua_pushinteger(L, S9xGetWord(luaL_checkinteger(L,1), WRAP_NONE, true));
	return 1;
}

static int memory_readwordsigned(lua_State *L) {
	signed short c = (signed short) S9xGetWord(luaL_checkinteger(L,1), WRAP_NONE, true);
	lua_pushinteger(L, c);
	return 1;
}

static int memory_readdword(lua_State *L)
{
	uint32 addr = luaL_checkinteger(L,1);
	uint32 val = S9xGetDWord(addr);

	lua_pushunsignedinteger(L, val);
	return 1;
}

static int memory_readdwordsigned(lua_State *L) {
	uint32 addr = luaL_checkinteger(L,1);
	int32 val = (signed) S9xGetDWord(addr);

	lua_pushinteger(L, val);
	return 1;
}

static int memory_readbyterange(lua_State *L) {
	uint32 address = luaL_checkinteger(L,1);
	int length = luaL_checkinteger(L,2);

	if(length < 0)
	{
		address += length;
		length = -length;
	}

	// push the array
	lua_createtable(L, abs(length), 0);

	// put all the values into the (1-based) array
	for(int a = address, n = 1; n <= length; a++, n++)
	{
		unsigned char value = S9xGetByte(a, true);
		lua_pushinteger(L, value);
		lua_rawseti(L, -2, n);
	}

	return 1;
}

static int memory_writebyte(lua_State *L)
{
	S9xSetByte(luaL_checkinteger(L,2), luaL_checkinteger(L,1), true);
	return 0;
}

static int memory_writeword(lua_State *L)
{
	S9xSetWord(luaL_checkinteger(L,2), luaL_checkinteger(L,1), WRAP_NONE, WRITE_01, true);
	return 0;
}

static int memory_writedword(lua_State *L)
{
	S9xSetDWord(luaL_checkunsignedinteger(L,2), luaL_checkinteger(L,1));
	return 0;
}


INLINE uint8 LuaAPUGetByte (uint32 Address)
{
	if (Address >= 0xf4 && Address <= 0xf7)
		return IAPU.RAM[Address];
	else
		return S9xAPUGetByte(Address);
}
INLINE uint16 LuaAPUGetWord (uint32 Address)
{
	uint16 ret;
	ret = LuaAPUGetByte(Address);
	ret |= (LuaAPUGetByte(Address+1) << 8);
	return ret;
}
INLINE uint32 LuaAPUGetDWord (uint32 Address)
{
	uint32 ret;
	ret = LuaAPUGetByte(Address);
	ret |= (LuaAPUGetByte(Address+1) << 8);
	ret |= (LuaAPUGetByte(Address+2) << 16);
	ret |= (LuaAPUGetByte(Address+3) << 24);
	return ret;
}

static int apu_readbyte(lua_State *L)
{
	lua_pushinteger(L, LuaAPUGetByte(luaL_checkinteger(L,1)));
	return 1;
}

static int apu_readbytesigned(lua_State *L) {
	signed char c = (signed char) LuaAPUGetByte(luaL_checkinteger(L,1));
	lua_pushinteger(L, c);
	return 1;
}

static int apu_readword(lua_State *L)
{
	lua_pushinteger(L, LuaAPUGetWord(luaL_checkinteger(L,1)));
	return 1;
}

static int apu_readwordsigned(lua_State *L) {
	signed short c = (signed short) LuaAPUGetWord(luaL_checkinteger(L,1));
	lua_pushinteger(L, c);
	return 1;
}

static int apu_readdword(lua_State *L)
{
	uint32 addr = luaL_checkinteger(L,1);
	uint32 val = LuaAPUGetDWord(addr);

	lua_pushunsignedinteger(L, val);
	return 1;
}

static int apu_readdwordsigned(lua_State *L) {
	uint32 addr = luaL_checkinteger(L,1);
	int32 val = (signed) LuaAPUGetDWord(addr);

	lua_pushinteger(L, val);
	return 1;
}

static int apu_readbyterange(lua_State *L) {
	uint32 address = luaL_checkinteger(L,1);
	int length = luaL_checkinteger(L,2);

	if(length < 0)
	{
		address += length;
		length = -length;
	}

	// push the array
	lua_createtable(L, abs(length), 0);

	// put all the values into the (1-based) array
	for(int a = address, n = 1; n <= length; a++, n++)
	{
		unsigned char value = LuaAPUGetByte(a);
		lua_pushinteger(L, value);
		lua_rawseti(L, -2, n);
	}

	return 1;
}

INLINE void S9xAPUSetWord (uint16 Word, uint32 Address)
{
	S9xAPUSetByte(Word & 0xff, Address);
	S9xAPUSetByte(Word >> 8, Address+1);
}
INLINE void S9xAPUSetDWord (uint32 DWord, uint32 Address)
{
	S9xAPUSetWord(DWord & 0xffff, Address);
	S9xAPUSetWord(DWord >> 16, Address+2);
}

static int apu_writebyte(lua_State *L)
{
	S9xAPUSetByte(luaL_checkinteger(L,2), luaL_checkinteger(L,1));
	return 0;
}

static int apu_writeword(lua_State *L)
{
	S9xAPUSetWord(luaL_checkinteger(L,2), luaL_checkinteger(L,1));
	return 0;
}

static int apu_writedword(lua_State *L)
{
	S9xAPUSetDWord(luaL_checkunsignedinteger(L,2), luaL_checkinteger(L,1));
	return 0;
}

// apu.writespc(filename, autosearch = false)
static int apu_writespc(lua_State *L) {
	const char *filename = luaL_checkstring(L,1);
	bool autosearch = false;

	if (!lua_isnil(L,2))
		autosearch = (lua_toboolean(L,2)!=0);

	if (autosearch) {
		// FIXME: the function cannot handle more than two requests at the same time.
		if(!strcmp(lua_spc_autosearch_filename, ""))
			strcpy(lua_spc_autosearch_filename, filename);
	}
	else
		S9xSPCDump (filename);
	return 0;
}


// string joypad.gettype(int port)
//
//  Returns the type of controller at the given physical port index 1 or 2
//  (port is the same as "which" for other input functions, unless there's a multitap)
//  possible return values are "joypad", "mouse", "superscope", "justifier", "justifiers", "multitap", "none"
static int joypad_gettype(lua_State *L) {
	int port = luaL_checkinteger(L,1) - 1;

	controllers controller;
	int8 ids[4];
	S9xGetController(port, &controller, &ids[0],&ids[1],&ids[2],&ids[3]);

	switch(controller)
	{
		default:
		case CTL_NONE: lua_pushliteral(L,"none"); break;
		case CTL_JOYPAD: lua_pushliteral(L,"joypad"); break;
		case CTL_MOUSE: lua_pushliteral(L,"mouse"); break;
		case CTL_SUPERSCOPE: lua_pushliteral(L,"superscope"); break;
		case CTL_JUSTIFIER: lua_pushstring(L,ids[0]?"justifiers":"justifier"); break;
		case CTL_MP5: lua_pushliteral(L,"multitap"); break;
	}
	return 1;
}

// joypad.settype(int port, string typename)
//
//  Sets the type of controller at the given physical port index 1 or 2
//  (port is the same as "which" for other input functions, unless there's a multitap)
//  The SNES sees a new controller type get plugged in instantly.
//  note that it's an error to call this command while a movie is active.
//  note that a superscope must be plugged into port 2 for it to work, and other peripherals might have similar sorts of requirements
//  valid types are "joypad", "mouse", "superscope", "justifier", "justifiers", "multitap", "none"
static int joypad_settype(lua_State *L) {

	if (S9xMovieActive())
		luaL_error(L, "joypad.settype() can't be called while a movie is active.");

	int port = luaL_checkinteger(L,1) - 1;
	const char* type = luaL_checkstring(L,2);

	controllers controller;
	int8 ids[4];
	S9xGetController(port, &controller, &ids[0],&ids[1],&ids[2],&ids[3]);

	if(!strcmp(type, "joypad"))
	{
		controller = CTL_JOYPAD;
		ids[0] = port;
	}
	else if(!strcmp(type, "mouse"))
	{
		controller = CTL_MOUSE;
		ids[0] = port;
	}
	else if(!strcmp(type, "superscope"))
	{
		controller = CTL_SUPERSCOPE;
		ids[0] = 0;
	}
	else if(!strcmp(type, "justifier"))
	{
		controller = CTL_JUSTIFIER;
		ids[0] = 0;
	}
	else if(!strcmp(type, "justifiers"))
	{
		controller = CTL_JUSTIFIER;
		ids[0] = 1;
	}
	else if(!strcmp(type, "multitap"))
	{
		controller = CTL_MP5;
		if(port == 0)
		{
			ids[0] = 0;
			ids[1] = 1;
			ids[2] = 2;
			ids[3] = 3;
		}
	}
	else
		controller = CTL_NONE;

	Settings.MouseMaster = true;
	Settings.JustifierMaster = true;
	Settings.SuperScopeMaster = true;
	Settings.MultiPlayer5Master = true;

	S9xSetController(port, controller, ids[0],ids[1],ids[2],ids[3]);

	Settings.MultiPlayer5Master = false;
	Settings.SuperScopeMaster = false;
	Settings.JustifierMaster = false;
	Settings.MouseMaster = false;

	// now fix emulation settings and controller ids for multitap
	S9xGetController(0, &controller, &ids[0],&ids[1],&ids[2],&ids[3]);
	int max0id = max((signed char)0, max(ids[0], max(ids[1], max(ids[2], ids[3]))));
	if(controller == CTL_MOUSE) Settings.MouseMaster = true;
	if(controller == CTL_JUSTIFIER) Settings.JustifierMaster = true;
	if(controller == CTL_SUPERSCOPE) Settings.SuperScopeMaster = true;
	if(controller == CTL_MP5) Settings.MultiPlayer5Master = true;
	controllers controller2;
	S9xGetController(1, &controller2, &ids[0],&ids[1],&ids[2],&ids[3]);
	if(controller2 == CTL_MOUSE) Settings.MouseMaster = true;
	if(controller2 == CTL_JUSTIFIER) Settings.JustifierMaster = true;
	if(controller2 == CTL_SUPERSCOPE) Settings.SuperScopeMaster = true;
	if(controller2 == CTL_MP5) Settings.MultiPlayer5Master = true;
	if((controller2 == CTL_JOYPAD && controller == CTL_MP5) || controller2 == CTL_MP5)
	{
		ids[0] = max0id + 1;
		if(controller2 == CTL_MP5)
		{
			ids[1] = max0id + 2;
			ids[2] = max0id + 3;
			ids[3] = max0id + 4;
		}
		S9xSetController(port, controller2, ids[0],ids[1],ids[2],ids[3]);
	}

#ifdef WIN32
	// win32 needs some extra settings set before it will do anything with peripherals
	if(controller == CTL_MOUSE)
		GUI.ControllerOption = SNES_MOUSE;
	else if(controller2 == CTL_MOUSE)
		GUI.ControllerOption = SNES_MOUSE_SWAPPED;
	else if(controller2 == CTL_SUPERSCOPE)
		GUI.ControllerOption = SNES_SUPERSCOPE;
	else if(controller2 == CTL_JUSTIFIER)
		GUI.ControllerOption = ids[0] ? SNES_JUSTIFIER_2 : SNES_JUSTIFIER;
	else if(controller == CTL_MP5)
		GUI.ControllerOption = SNES_MULTIPLAYER8;
	else if(controller2 == CTL_MP5)
		GUI.ControllerOption = SNES_MULTIPLAYER5;
	else
		GUI.ControllerOption = SNES_JOYPAD;
    GUI.ControlForced = 0xff;
#endif

	return 0;
}

// table joypad.get([int which = 1])
//
//  Reads the joypads as inputted by the user.
static int joy_get_internal(lua_State *L, bool reportUp, bool reportDown) {

	// Reads the joypads as inputted by the user
	int which = lua_isnoneornil(L,1) ? 1 : luaL_checkinteger(L,1);
	if (which < 1 || which > 8) {
		luaL_error(L,"Invalid input port (valid range 1-8, specified %d)", which);
	}
	
	lua_newtable(L);


	controllers controller = CTL_JOYPAD;
	int8 ids[4];
	if(which <= 2) // could be a peripheral in ports 1 or 2, let's check
		S9xGetController(which-1, &controller, &ids[0],&ids[1],&ids[2],&ids[3]);

	switch(controller)
	{
	default: // joypad
		{
			uint32 buttons = MovieGetJoypad(which - 1);

			// set table with joypad buttons
			for (int i = 4; i < 16; i++) {
				bool pressed = (buttons & (1<<i))!=0;
				if ((pressed && reportDown) || (!pressed && reportUp)) {
					lua_pushboolean(L, pressed);
					lua_setfield(L, -2, button_mappings[i]);
				}
			}
		}
		break;
	case CTL_MOUSE:
		{
			uint8 buf [MOUSE_DATA_SIZE] = {0};
			if(MovieGetMouse(which - 1, buf))
			{
				int16 x = ((uint16*)buf)[0];
				int16 y = ((uint16*)buf)[1];
				uint8 buttons = buf[4];

				// set table with mouse status
				lua_pushinteger(L,x);     // note: the mouse does not really have x and y coordinates.
				lua_setfield(L, -2, "x"); //       so, these coordinates are "referenceless",
				lua_pushinteger(L,y);     //       they don't make sense except considering the difference
				lua_setfield(L, -2, "y"); //       between them and their previous value.
				lua_pushboolean(L,buttons&SNESMOUSE_LEFT);
				lua_setfield(L, -2, "left");
				lua_pushboolean(L,buttons&SNESMOUSE_RIGHT);
				lua_setfield(L, -2, "right");
			}
		}
		break;
	case CTL_SUPERSCOPE:
		{
			uint8 buf [SCOPE_DATA_SIZE] = {0};
			if(MovieGetScope(which - 1, buf))
			{
				int16 x = ((uint16*)buf)[0];
				int16 y = ((uint16*)buf)[1];
				uint8 buttons = buf[4];

				// set table with super scope status
				lua_pushinteger(L,x);
				lua_setfield(L, -2, "x");
				lua_pushinteger(L,y);
				lua_setfield(L, -2, "y");
				lua_pushboolean(L,buttons&SUPERSCOPE_FIRE);
				lua_setfield(L, -2, "fire");
				lua_pushboolean(L,buttons&SUPERSCOPE_CURSOR);
				lua_setfield(L, -2, "cursor");
				lua_pushboolean(L,buttons&SUPERSCOPE_TURBO);
				lua_setfield(L, -2, "turbo");
				lua_pushboolean(L,buttons&SUPERSCOPE_PAUSE);
				lua_setfield(L, -2, "pause");
				lua_pushboolean(L,buttons&SUPERSCOPE_OFFSCREEN);
				lua_setfield(L, -2, "offscreen");
			}
		}
		break;
	case CTL_JUSTIFIER:
		{
			uint8 buf [JUSTIFIER_DATA_SIZE] = {0};
			if(MovieGetJustifier(which - 1, buf))
			{
				bool weHaveTwoJustifiers = (ids[0] == 1);
				int16 x1 = ((uint16*)buf)[0];
				int16 y1 = ((uint16*)buf)[2];
				uint8 buttons = buf[8];
				bool8 offscreen1 = buf[9];

				// set table with justifier status
				lua_pushinteger(L,x1);
				lua_setfield(L, -2, "x");
				lua_pushinteger(L,y1);
				lua_setfield(L, -2, "y");
				lua_pushboolean(L,buttons&JUSTIFIER_TRIGGER);
				lua_setfield(L, -2, "trigger");
				lua_pushboolean(L,buttons&JUSTIFIER_START);
				lua_setfield(L, -2, "start");
				lua_pushboolean(L,buttons&JUSTIFIER_SELECT);
				lua_setfield(L, -2, "select");
				lua_pushboolean(L,offscreen1);
				lua_setfield(L, -2, "offscreen");

				if(weHaveTwoJustifiers)
				{
					int16 x2 = ((uint16*)buf)[1];
					int16 y2 = ((uint16*)buf)[3];
					bool8 offscreen2 = buf[10];

					// also set table with the second justifier's status
					lua_pushinteger(L,x2);
					lua_setfield(L, -2, "x2");
					lua_pushinteger(L,y2);
					lua_setfield(L, -2, "y2");
					lua_pushboolean(L,buttons&JUSTIFIER2_TRIGGER);
					lua_setfield(L, -2, "trigger2");
					lua_pushboolean(L,buttons&JUSTIFIER2_START);
					lua_setfield(L, -2, "start2");
					lua_pushboolean(L,buttons&JUSTIFIER2_SELECT);
					lua_setfield(L, -2, "select2");
					lua_pushboolean(L,offscreen2);
					lua_setfield(L, -2, "offscreen2");
				}
			}
		}
		break;
	}

	return 1;
}
// joypad.get(which)
// returns a table of every game button,
// true meaning currently-held and false meaning not-currently-held
// (as of last frame boundary)
// this WILL read input from a currently-playing movie
static int joypad_get(lua_State *L)
{
	return joy_get_internal(L, true, true);
}
// joypad.getdown(which)
// returns a table of every game button that is currently held
static int joypad_getdown(lua_State *L)
{
	return joy_get_internal(L, false, true);
}
// joypad.getup(which)
// returns a table of every game button that is not currently held
static int joypad_getup(lua_State *L)
{
	return joy_get_internal(L, true, false);
}


// joypad.set([int which = 1,] table buttons)
//
//   Sets the given buttons to be pressed during the next
//   frame advance. The table should have the right 
//   keys (no pun intended) set.
static int joypad_set(lua_State *L) {

	// Which joypad we're tampering with
	int which = !lua_isnumber(L,1) ? 1 : luaL_checkinteger(L,1);
	if (which < 1 || which > 8) {
		luaL_error(L,"Invalid output port (valid range 1-8, specified %d)", which);
	}

	if (IPPU.InMainLoop || joy_set_queue)
	{
		// defer this function until when we are processing input
		DeferFunctionCall(L, deferredJoySetIDString);
		return 0;
	}

	// And the table of buttons.
	int tableIndex = !lua_isnumber(L,1) ? 1 : 2;
	luaL_checktype(L,tableIndex,LUA_TTABLE);

	int port = which - 1;

	controllers con = CTL_JOYPAD;
	int8 ids[4];
	if(port <= 1) // could be a peripheral in ports 0 or 1, let's check
		S9xGetController(port, &con, &ids[0],&ids[1],&ids[2],&ids[3]);

	switch(con)
	{
	default: // joypad
		{
			uint16 input = 0;
			uint16 mask = 0;

			for (int i = 4; i < 16; i++) {
				const char* name = button_mappings[i];
				lua_getfield(L, tableIndex, name);
				if (!lua_isnil(L,-1)) {
					bool pressed = lua_toboolean(L,-1) != 0;
					uint16 bitmask = 1 << i;
					if (pressed)
						input |= bitmask;
					else
						input &= ~bitmask;
					mask |= bitmask;
				}
				lua_pop(L,1);
			}
			MovieSetJoypad(which - 1, input, mask);
		}
		break;
	case CTL_MOUSE:
		{
			lua_getfield(L, tableIndex, "x");
			if(lua_isnumber(L,-1))
				AddCommandTransformAxis(con,ids[0], lua_tointeger(L,-1), 0);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "y");
			if(lua_isnumber(L,-1))
				AddCommandTransformAxis(con,ids[0], lua_tointeger(L,-1), 1);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "left");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,ids[0], lua_toboolean(L,-1), SNESMOUSE_LEFT);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "right");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,ids[0], lua_toboolean(L,-1), SNESMOUSE_RIGHT);
			lua_pop(L,1);
		}
		break;
	case CTL_SUPERSCOPE:
		{
			lua_getfield(L, tableIndex, "x");
			if(lua_isnumber(L,-1))
				AddCommandTransformAxis(con,0, lua_tointeger(L,-1), 0);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "y");
			if(lua_isnumber(L,-1))
				AddCommandTransformAxis(con,0, lua_tointeger(L,-1), 1);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "fire");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,0, lua_toboolean(L,-1), SUPERSCOPE_FIRE);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "cursor");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,0, lua_toboolean(L,-1), SUPERSCOPE_CURSOR);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "turbo");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,0, lua_toboolean(L,-1), SUPERSCOPE_TURBO);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "pause");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,0, lua_toboolean(L,-1), SUPERSCOPE_PAUSE);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "offscreen");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,0, lua_toboolean(L,-1), SUPERSCOPE_OFFSCREEN);
			lua_pop(L,1);
		}
		break;
	case CTL_JUSTIFIER:
		{
			lua_getfield(L, tableIndex, "x");
			if(lua_isnumber(L,-1))
				AddCommandTransformAxis(con,0, lua_tointeger(L,-1), 0);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "y");
			if(lua_isnumber(L,-1))
				AddCommandTransformAxis(con,0, lua_tointeger(L,-1), 1);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "trigger");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,0, lua_toboolean(L,-1), JUSTIFIER_TRIGGER);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "start");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,0, lua_toboolean(L,-1), JUSTIFIER_START);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "select");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,0, lua_toboolean(L,-1), JUSTIFIER_SELECT);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "offscreen");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,0, lua_toboolean(L,-1), JUSTIFIER_OFFSCREEN);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "x2");
			if(lua_isnumber(L,-1))
				AddCommandTransformAxis(con,1, lua_tointeger(L,-1), 0);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "y2");
			if(lua_isnumber(L,-1))
				AddCommandTransformAxis(con,1, lua_tointeger(L,-1), 1);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "trigger2");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,1, lua_toboolean(L,-1), JUSTIFIER2_TRIGGER);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "start2");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,1, lua_toboolean(L,-1), JUSTIFIER2_START);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "select2");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,1, lua_toboolean(L,-1), JUSTIFIER2_SELECT);
			lua_pop(L,1);

			lua_getfield(L, tableIndex, "offscreen2");
			if(!lua_isnil(L,-1))
				AddCommandTransformButton(con,1, lua_toboolean(L,-1), JUSTIFIER2_OFFSCREEN);
			lua_pop(L,1);
		}
		break;
	}
	
	return 0;
}




// Helper function to convert a savestate object to the filename it represents.
static const char *savestateobj2filename(lua_State *L, int offset) {
	
	// First we get the metatable of the indicated object
	int result = lua_getmetatable(L, offset);

	if (!result)
		luaL_error(L, "object not a savestate object");
	
	// Also check that the type entry is set
	lua_getfield(L, -1, "__metatable");
	if (strcmp(lua_tostring(L,-1), "snes9x Savestate") != 0)
		luaL_error(L, "object not a savestate object");
	lua_pop(L,1);
	
	// Now, get the field we want
	lua_getfield(L, -1, "filename");
	
	// Return it
	return lua_tostring(L, -1);
}


// Helper function for garbage collection.
static int savestate_gc(lua_State *L) {
	// The object we're collecting is on top of the stack
	lua_getmetatable(L,1);
	
	// Get the filename
	const char *filename;
	lua_getfield(L, -1, "filename");
	filename = lua_tostring(L,-1);

	// Delete the file
	remove(filename);
	
	// Also delete the .luasav file if we saved one of those with it
	char luaSaveFilename [512];
	strncpy(luaSaveFilename, filename, 512);
	luaSaveFilename[512-(1+7/*strlen(".luasav")*/)] = '\0';
	strcat(luaSaveFilename, ".luasav");
	remove(luaSaveFilename);

	// We exit, and the garbage collector takes care of the rest.
	return 0;
}

// object savestate.create(int which = nil)
//
//  Creates an object used for savestates.
//  The object can be associated with a player-accessible savestate
//  ("which" between 1 and 12) or not (which == nil).
static int savestate_create(lua_State *L) {
	int which = -1;
	if (lua_gettop(L) >= 1) {
		which = luaL_checkinteger(L, 1);
		if (which < 1 || which > 12) {
			luaL_error(L, "invalid player's savestate %d", which);
		}
	}
	

	char *filename;

	if (which > 0) {
		// Find an appropriate filename. This is OS specific, unfortunately.
		// So I turned the filename selection code into my bitch. :)
		// Numbers are 0 through 9 though.
		filename = S9xGetFreezeFilename(which - 1);
	}
	else {
		filename = tempnam(NULL, "snlua");
	}
	
	// Our "object". We don't care about the type, we just need the memory and GC services.
	lua_newuserdata(L,1);
	
	// The metatable we use, protected from Lua and contains garbage collection info and stuff.
	lua_newtable(L);
	
	// First, we must protect it
	lua_pushstring(L, "snes9x Savestate");
	lua_setfield(L, -2, "__metatable");
	
	
	// Now we need to save the file itself.
	lua_pushstring(L, filename);
	lua_setfield(L, -2, "filename");
	
	// If it's an anonymous savestate, we must delete the file from disk should it be gargage collected
	if (which < 0) {
		lua_pushcfunction(L, savestate_gc);
		lua_setfield(L, -2, "__gc");
		
	}
	
	// Set the metatable
	lua_setmetatable(L, -2);

	// The filename was allocated using malloc. Do something about that.
	free(filename);
	
	// Awesome. Return the object
	return 1;
	
}


// savestate.save(object state)
//
//   Saves a state to the given object.
static int savestate_save(lua_State *L) {

	const char *filename = savestateobj2filename(L,1);

//	printf("saving %s\n", filename);

	// Save states are very expensive. They take time.
	numTries--;

	bool8 retvalue = S9xFreezeGame(filename);
	if (!retvalue) {
		// Uh oh
		luaL_error(L, "savestate failed");
	}
	return 0;

}

// savestate.load(object state)
//
//   Loads the given state
static int savestate_load(lua_State *L) {

	const char *filename = savestateobj2filename(L,1);

	numTries--;

//	printf("loading %s\n", filename);
	bool8 retvalue = S9xUnfreezeGame(filename);
	if (!retvalue) {
		// Uh oh
		luaL_error(L, "loadstate failed");
	}
	return 0;

}


#ifdef _DEBUG
	#define SNAPSHOT_VERIFY_SUPPORTED
#endif
#ifdef SNAPSHOT_VERIFY_SUPPORTED

bool S9xVerifySnapshotsIdentical (const char *filename);
//bool S9xVerifySnapshotsIdentical (STREAM stream);
#include <vector>
#include <string>
extern std::vector<std::string> g_verifyErrors;

// savestate.verify(object state)
//
//   Verify that a snapshot saved of the current emulation state would exactly match
//   with the snapshot that's already in the given file or stream (this is for desync testing).
//   If verification fails, will error out with some info (with possibly more in the console or debugger output).
//   This function only exists in Debug, because it was only intended to be a tool for Snes9x developers.
static int savestate_verify(lua_State *L) {

	const char *filename = savestateobj2filename(L,1);

//	printf("verifying %s\n", filename);

	bool8 retvalue = S9xVerifySnapshotsIdentical(filename);
	if (!retvalue) {
		// Uh oh
		static unsigned int maxErrorsToShow = 20;
		unsigned int errorsToShow = min(maxErrorsToShow, g_verifyErrors.size());
		luaL_where(L, 1);
		lua_pushliteral(L, "savestate verification failed:\n");
		lua_checkstack(L, errorsToShow);
		for(unsigned int i = 0; i < errorsToShow; i++)
			lua_pushstring(L, g_verifyErrors[i].c_str());
		lua_concat(L, errorsToShow + 2);
		lua_error(L);
	}
	return 0;
}
#endif // SNAPSHOT_VERIFY_SUPPORTED


static int savestate_registersave(lua_State *L) {
 
	lua_settop(L,1);
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_getfield(L, LUA_REGISTRYINDEX, LUA_SAVE_CALLBACK_STRING);
	lua_pushvalue(L,1);
	lua_setfield(L, LUA_REGISTRYINDEX, LUA_SAVE_CALLBACK_STRING);
	return 1;
}
static int savestate_registerload(lua_State *L) {

	lua_settop(L,1);
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOAD_CALLBACK_STRING);
	lua_pushvalue(L,1);
	lua_setfield(L, LUA_REGISTRYINDEX, LUA_LOAD_CALLBACK_STRING);
	return 1;
}

static int savestate_loadscriptdata(lua_State *L) {

	const char *filename = savestateobj2filename(L,1);

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

			lua_settop(L, 0);
			saveData.LoadRecord(L, LUA_DATARECORDKEY, (unsigned int)-1);
			return lua_gettop(L);
		}
	}
	return 0;
}

// int snes9x.framecount()
//
//   Gets the frame counter for the movie, or the number of frames since last reset.
int snes9x_framecount(lua_State *L) {
	if (!S9xMovieActive()) {
		lua_pushinteger(L, Timings.TotalEmulatedFrames);
	}
	else {
		lua_pushinteger(L, S9xMovieGetFrameCounter());
	}
	return 1;
}

// int snes9x.lagcount()
int snes9x_lagcount(lua_State *L) {
	lua_pushinteger(L, Timings.LagCounter);
	return 1;
}

// boolean snes9x.lagged()
int snes9x_lagged(lua_State *L) {
	extern bool8 pad_read; // from controls.cpp
	lua_pushboolean(L, !pad_read);
	return 1;
}

// boolean snes9x.emulating()
int snes9x_emulating(lua_State *L) {
	lua_pushboolean(L, !Settings.StopEmulation);
	return 1;
}

// boolean snes9x.atframeboundary()
int snes9x_atframeboundary(lua_State *L) {
	lua_pushboolean(L, !IPPU.InMainLoop);
	return 1;
}

int movie_isactive(lua_State *L) {
	lua_pushboolean(L, S9xMovieActive());
	return 1;
}

int movie_isrecording(lua_State *L) {
	lua_pushboolean(L, S9xMovieRecording());
	return 1;
}

int movie_isplaying(lua_State *L) {
	lua_pushboolean(L, S9xMoviePlaying());
	return 1;
}

int movie_getname(lua_State *L) {
	if(S9xMovieActive())
		lua_pushstring(L, S9xMovieGetName());
	else
		lua_pushstring(L, "");
	return 1;
}

int movie_getlength(lua_State *L) {
	if(S9xMovieActive())
		lua_pushinteger(L, S9xMovieGetLength());
	else
		lua_pushinteger(L, 0);
	return 1;
}

// string movie.mode()
//
//   "record", "playback" or nil
int movie_getmode(lua_State *L) {
	if (!S9xMovieActive())
		lua_pushstring(L, "inactive");
	else if (S9xMoviePlaying())
		lua_pushstring(L, "playback");
	else if (S9xMovieRecording())
		lua_pushstring(L, "record");
	else if (S9xMovieFinished())
		lua_pushstring(L, "finished");
	else
		lua_pushnil(L);
	return 1;
}

static int movie_rerecordcount(lua_State *L) {
	if(S9xMovieActive())
		lua_pushinteger(L, S9xMovieGetRerecordCount());
	else
		lua_pushinteger(L, 0);
	return 1;
}

static int movie_setrerecordcount(lua_State *L) {
	if(S9xMovieActive())
		S9xMovieSetRerecordCount(luaL_checkinteger(L, 1));
	return 0;
}

static int movie_rerecordcounting(lua_State *L) {
	if (lua_gettop(L) == 0)
		luaL_error(L, "no parameters specified");

	skipRerecords = lua_toboolean(L,1);
	return 0;
}

static int movie_getreadonly(lua_State *L) {
#ifdef __WIN32__
	if (S9xMovieActive())
		lua_pushboolean(L, S9xMovieReadOnly());
	else
		lua_pushboolean(L, GUI.MovieReadOnly);
#else
	lua_pushboolean(L, S9xMovieReadOnly());
#endif
	return 1;
}

static int movie_setreadonly(lua_State *L) {
	int readonly = lua_toboolean(L,1) ? 1 : 0;
	S9xMovieSetReadOnly(readonly);
#ifdef __WIN32__
	GUI.MovieReadOnly = readonly;
#endif
	return 0;
}

// movie.stop()
//
//   Stops movie playback/recording. Bombs out if movie is not running.
static int movie_stop(lua_State *L) {
	if (!S9xMovieActive())
		luaL_error(L, "no movie");
	
	S9xMovieStop(FALSE);
	return 0;

}



#define LUA_SCREEN_WIDTH    256
#define LUA_SCREEN_HEIGHT   239

// Common code by the gui library: make sure the screen array is ready
static void gui_prepare() {
	if (!gui_data)
		gui_data = (uint8*)malloc(LUA_SCREEN_WIDTH*LUA_SCREEN_HEIGHT*4);
	if (!gui_used)
		memset(gui_data, 0, LUA_SCREEN_WIDTH*LUA_SCREEN_HEIGHT*4);
	gui_used = TRUE;
}

// pixform for lua graphics
#define BUILD_PIXEL_ARGB8888(A,R,G,B) (((int) (A) << 24) | ((int) (R) << 16) | ((int) (G) << 8) | (int) (B))
#define DECOMPOSE_PIXEL_ARGB8888(PIX,A,R,G,B) { (A) = ((PIX) >> 24) & 0xff; (R) = ((PIX) >> 16) & 0xff; (G) = ((PIX) >> 8) & 0xff; (B) = (PIX) & 0xff; }
#define LUA_BUILD_PIXEL BUILD_PIXEL_ARGB8888
#define LUA_DECOMPOSE_PIXEL DECOMPOSE_PIXEL_ARGB8888
#define LUA_PIXEL_A(PIX) (((PIX) >> 24) & 0xff)
#define LUA_PIXEL_R(PIX) (((PIX) >> 16) & 0xff)
#define LUA_PIXEL_G(PIX) (((PIX) >> 8) & 0xff)
#define LUA_PIXEL_B(PIX) ((PIX) & 0xff)

template <class T> static void swap(T &one, T &two) {
	T temp = one;
	one = two;
	two = temp;
}

// write a pixel to buffer
static inline void blend32(uint32 *dstPixel, uint32 colour)
{
	uint8 *dst = (uint8*) dstPixel;
	int a, r, g, b;
	LUA_DECOMPOSE_PIXEL(colour, a, r, g, b);

	if (a == 255 || dst[3] == 0) {
		// direct copy
		*(uint32*)(dst) = colour;
	}
	else if (a == 0) {
		// do not copy
	}
	else {
		// alpha-blending
		int a_dst = ((255 - a) * dst[3] + 128) / 255;
		int a_new = a + a_dst;

		dst[0] = (uint8) ((( dst[0] * a_dst + b * a) + (a_new / 2)) / a_new);
		dst[1] = (uint8) ((( dst[1] * a_dst + g * a) + (a_new / 2)) / a_new);
		dst[2] = (uint8) ((( dst[2] * a_dst + r * a) + (a_new / 2)) / a_new);
		dst[3] = (uint8) a_new;
	}
}
// check if a pixel is in the lua canvas
static inline bool gui_check_boundary(int x, int y) {
	return !(x < 0 || x >= LUA_SCREEN_WIDTH || y < 0 || y >= LUA_SCREEN_HEIGHT);
}

// write a pixel to gui_data (do not check boundaries for speedup)
static inline void gui_drawpixel_fast(int x, int y, uint32 colour) {
	//gui_prepare();
	blend32((uint32*) &gui_data[(y*LUA_SCREEN_WIDTH+x)*4], colour);
}

// write a pixel to gui_data (check boundaries)
static inline void gui_drawpixel_internal(int x, int y, uint32 colour) {
	//gui_prepare();
	if (gui_check_boundary(x, y))
		gui_drawpixel_fast(x, y, colour);
}

// draw a line on gui_data (checks boundaries)
static void gui_drawline_internal(int x1, int y1, int x2, int y2, bool lastPixel, uint32 colour) {

	//gui_prepare();

	// Note: New version of Bresenham's Line Algorithm
	// http://groups.google.co.jp/group/rec.games.roguelike.development/browse_thread/thread/345f4c42c3b25858/29e07a3af3a450e6?show_docid=29e07a3af3a450e6

	int swappedx = 0;
	int swappedy = 0;

	int xtemp = x1-x2;
	int ytemp = y1-y2;
	if (xtemp == 0 && ytemp == 0) {
		gui_drawpixel_internal(x1, y1, colour);
		return;
	}
	if (xtemp < 0) {
		xtemp = -xtemp;
		swappedx = 1;
	}
	if (ytemp < 0) {
		ytemp = -ytemp;
		swappedy = 1;
	}

	int delta_x = xtemp << 1;
	int delta_y = ytemp << 1;

	signed char ix = x1 > x2?1:-1;
	signed char iy = y1 > y2?1:-1;

	if (lastPixel)
		gui_drawpixel_internal(x2, y2, colour);

	if (delta_x >= delta_y) {
		int error = delta_y - (delta_x >> 1);

		while (x2 != x1) {
			if (error == 0 && !swappedx)
				gui_drawpixel_internal(x2+ix, y2, colour);
			if (error >= 0) {
				if (error || (ix > 0)) {
					y2 += iy;
					error -= delta_x;
				}
			}
			x2 += ix;
			gui_drawpixel_internal(x2, y2, colour);
			if (error == 0 && swappedx)
				gui_drawpixel_internal(x2, y2+iy, colour);
			error += delta_y;
		}
	}
	else {
		int error = delta_x - (delta_y >> 1);

		while (y2 != y1) {
			if (error == 0 && !swappedy)
				gui_drawpixel_internal(x2, y2+iy, colour);
			if (error >= 0) {
				if (error || (iy > 0)) {
					x2 += ix;
					error -= delta_y;
				}
			}
			y2 += iy;
			gui_drawpixel_internal(x2, y2, colour);
			if (error == 0 && swappedy)
				gui_drawpixel_internal(x2+ix, y2, colour);
			error += delta_x;
		}
	}
}

// draw a rect on gui_data
static void gui_drawbox_internal(int x1, int y1, int x2, int y2, uint32 colour) {

	if (x1 > x2) 
		swap<int>(x1, x2);
	if (y1 > y2) 
		swap<int>(y1, y2);
	if (x1 < 0)
		x1 = -1;
	if (y1 < 0)
		y1 = -1;
	if (x2 >= LUA_SCREEN_WIDTH)
		x2 = LUA_SCREEN_WIDTH;
	if (y2 >= LUA_SCREEN_HEIGHT)
		y2 = LUA_SCREEN_HEIGHT;

	//gui_prepare();

	gui_drawline_internal(x1, y1, x2, y1, true, colour);
	gui_drawline_internal(x1, y2, x2, y2, true, colour);
	gui_drawline_internal(x1, y1, x1, y2, true, colour);
	gui_drawline_internal(x2, y1, x2, y2, true, colour);
}

// draw a circle on gui_data
static void gui_drawcircle_internal(int x0, int y0, int radius, uint32 colour) {

	//gui_prepare();

	if (radius < 0)
		radius = -radius;
	if (radius == 0)
		return;
	if (radius == 1) {
		gui_drawpixel_internal(x0, y0, colour);
		return;
	}

	// http://en.wikipedia.org/wiki/Midpoint_circle_algorithm

	int f = 1 - radius;
	int ddF_x = 1;
	int ddF_y = -2 * radius;
	int x = 0;
	int y = radius;

	gui_drawpixel_internal(x0, y0 + radius, colour);
	gui_drawpixel_internal(x0, y0 - radius, colour);
	gui_drawpixel_internal(x0 + radius, y0, colour);
	gui_drawpixel_internal(x0 - radius, y0, colour);
 
	// same pixel shouldn't be drawed twice,
	// because each pixel has opacity.
	// so now the routine gets ugly.
	while(true)
	{
		assert(ddF_x == 2 * x + 1);
		assert(ddF_y == -2 * y);
		assert(f == x*x + y*y - radius*radius + 2*x - y + 1);
		if(f >= 0) 
		{
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;
		if (x < y) {
			gui_drawpixel_internal(x0 + x, y0 + y, colour);
			gui_drawpixel_internal(x0 - x, y0 + y, colour);
			gui_drawpixel_internal(x0 + x, y0 - y, colour);
			gui_drawpixel_internal(x0 - x, y0 - y, colour);
			gui_drawpixel_internal(x0 + y, y0 + x, colour);
			gui_drawpixel_internal(x0 - y, y0 + x, colour);
			gui_drawpixel_internal(x0 + y, y0 - x, colour);
			gui_drawpixel_internal(x0 - y, y0 - x, colour);
		}
		else if (x == y) {
			gui_drawpixel_internal(x0 + x, y0 + y, colour);
			gui_drawpixel_internal(x0 - x, y0 + y, colour);
			gui_drawpixel_internal(x0 + x, y0 - y, colour);
			gui_drawpixel_internal(x0 - x, y0 - y, colour);
			break;
		}
		else
			break;
	}
}

// draw fill rect on gui_data
static void gui_fillbox_internal(int x1, int y1, int x2, int y2, uint32 colour) {

	if (x1 > x2) 
		swap<int>(x1, x2);
	if (y1 > y2) 
		swap<int>(y1, y2);
	if (x1 < 0)
		x1 = 0;
	if (y1 < 0)
		y1 = 0;
	if (x2 >= LUA_SCREEN_WIDTH)
		x2 = LUA_SCREEN_WIDTH - 1;
	if (y2 >= LUA_SCREEN_HEIGHT)
		y2 = LUA_SCREEN_HEIGHT - 1;

	//gui_prepare();

	int ix, iy;
	for (iy = y1; iy <= y2; iy++) {
		for (ix = x1; ix <= x2; ix++) {
			gui_drawpixel_fast(ix, iy, colour);
		}
	}
}

// fill a circle on gui_data
static void gui_fillcircle_internal(int x0, int y0, int radius, uint32 colour) {

	//gui_prepare();

	if (radius < 0)
		radius = -radius;
	if (radius == 0)
		return;
	if (radius == 1) {
		gui_drawpixel_internal(x0, y0, colour);
		return;
	}

	// http://en.wikipedia.org/wiki/Midpoint_circle_algorithm

	int f = 1 - radius;
	int ddF_x = 1;
	int ddF_y = -2 * radius;
	int x = 0;
	int y = radius;

	gui_drawline_internal(x0, y0 - radius, x0, y0 + radius, true, colour);
 
	while(true)
	{
		assert(ddF_x == 2 * x + 1);
		assert(ddF_y == -2 * y);
		assert(f == x*x + y*y - radius*radius + 2*x - y + 1);
		if(f >= 0) 
		{
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		if (x < y) {
			gui_drawline_internal(x0 + x, y0 - y, x0 + x, y0 + y, true, colour);
			gui_drawline_internal(x0 - x, y0 - y, x0 - x, y0 + y, true, colour);
			if (f >= 0) {
				gui_drawline_internal(x0 + y, y0 - x, x0 + y, y0 + x, true, colour);
				gui_drawline_internal(x0 - y, y0 - x, x0 - y, y0 + x, true, colour);
			}
		}
		else if (x == y) {
			gui_drawline_internal(x0 + x, y0 - y, x0 + x, y0 + y, true, colour);
			gui_drawline_internal(x0 - x, y0 - y, x0 - x, y0 + y, true, colour);
			break;
		}
		else
			break;
	}
}

// Helper for a simple hex parser
static int hex2int(lua_State *L, char c) {
	if (c >= '0' && c <= '9')
		return c-'0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return luaL_error(L, "invalid hex in colour");
}

static const struct ColorMapping
{
	const char* name;
	int value;
}
s_colorMapping [] =
{
	{"white",     0xFFFFFFFF},
	{"black",     0x000000FF},
	{"clear",     0x00000000},
	{"gray",      0x7F7F7FFF},
	{"grey",      0x7F7F7FFF},
	{"red",       0xFF0000FF},
	{"orange",    0xFF7F00FF},
	{"yellow",    0xFFFF00FF},
	{"chartreuse",0x7FFF00FF},
	{"green",     0x00FF00FF},
	{"teal",      0x00FF7FFF},
	{"cyan" ,     0x00FFFFFF},
	{"blue",      0x0000FFFF},
	{"purple",    0x7F00FFFF},
	{"magenta",   0xFF00FFFF},
};

/**
 * Converts an integer or a string on the stack at the given
 * offset to a RGB32 colour. Several encodings are supported.
 * The user may construct their own RGB value, given a simple colour name,
 * or an HTML-style "#09abcd" colour. 16 bit reduction doesn't occur at this time.
 */
static inline bool str2colour(uint32 *colour, lua_State *L, const char *str) {
	if (str[0] == '#') {
		int color;
		sscanf(str+1, "%X", &color);
		int len = strlen(str+1);
		int missing = max(0, 8-len);
		color <<= missing << 2;
		if(missing >= 2) color |= 0xFF;
		*colour = color;
		return true;
	}
	else {
		if(!strnicmp(str, "rand", 4)) {
			*colour = gen_rand32() | 0xFF; //((rand()*255/RAND_MAX) << 8) | ((rand()*255/RAND_MAX) << 16) | ((rand()*255/RAND_MAX) << 24) | 0xFF;
			return true;
		}
		for(int i = 0; i < sizeof(s_colorMapping)/sizeof(*s_colorMapping); i++) {
			if(!stricmp(str,s_colorMapping[i].name)) {
				*colour = s_colorMapping[i].value;
				return true;
			}
		}
	}
	return false;
}
static inline uint32 gui_getcolour_wrapped(lua_State *L, int offset, bool hasDefaultValue, uint32 defaultColour) {
	switch (lua_type(L,offset)) {
	case LUA_TSTRING:
		{
			const char *str = lua_tostring(L,offset);
			uint32 colour;

			if (str2colour(&colour, L, str))
				return colour;
			else {
				if (hasDefaultValue)
					return defaultColour;
				else
					return luaL_error(L, "unknown colour %s", str);
			}
		}
	case LUA_TNUMBER:
		{
			uint32 colour = (uint32) lua_tounsignedinteger(L,offset);
			return colour;
		}
	case LUA_TTABLE:
		{
			int color = 0xFF;
			lua_pushnil(L); // first key
			int keyIndex = lua_gettop(L);
			int valueIndex = keyIndex + 1;
			bool first = true;
			while(lua_next(L, offset))
			{
				bool keyIsString = (lua_type(L, keyIndex) == LUA_TSTRING);
				bool keyIsNumber = (lua_type(L, keyIndex) == LUA_TNUMBER);
				int key = keyIsString ? tolower(*lua_tostring(L, keyIndex)) : (keyIsNumber ? lua_tointeger(L, keyIndex) : 0);
				int value = lua_tointeger(L, valueIndex);
				if(value < 0) value = 0;
				if(value > 255) value = 255;
				switch(key)
				{
				case 1: case 'r': color |= value << 24; break;
				case 2: case 'g': color |= value << 16; break;
				case 3: case 'b': color |= value << 8; break;
				case 4: case 'a': color = (color & ~0xFF) | value; break;
				}
				lua_pop(L, 1);
			}
			return color;
		}	break;
	case LUA_TFUNCTION:
		luaL_error(L, "invalid colour"); // NYI
		return 0;
	default:
		if (hasDefaultValue)
			return defaultColour;
		else
			return luaL_error(L, "invalid colour");
	}
}
static uint32 gui_getcolour(lua_State *L, int offset) {
	uint32 colour;
	int a, r, g, b;

	colour = gui_getcolour_wrapped(L, offset, false, 0);
	a = ((colour & 0xff) * transparencyModifier) / 255;
	if (a > 255) a = 255;
	b = (colour >> 8) & 0xff;
	g = (colour >> 16) & 0xff;
	r = (colour >> 24) & 0xff;
	return LUA_BUILD_PIXEL(a, r, g, b);
}
static uint32 gui_optcolour(lua_State *L, int offset, uint32 defaultColour) {
	uint32 colour;
	int a, r, g, b;
	uint8 defA, defB, defG, defR;

	LUA_DECOMPOSE_PIXEL(defaultColour, defA, defR, defG, defB);
	defaultColour = (defR << 24) | (defG << 16) | (defB << 8) | defA;

	colour = gui_getcolour_wrapped(L, offset, true, defaultColour);
	a = ((colour & 0xff) * transparencyModifier) / 255;
	if (a > 255) a = 255;
	b = (colour >> 8) & 0xff;
	g = (colour >> 16) & 0xff;
	r = (colour >> 24) & 0xff;
	return LUA_BUILD_PIXEL(a, r, g, b);
}

// gui.drawpixel(x,y,colour)
static int gui_drawpixel(lua_State *L) {

	int x = luaL_checkinteger(L, 1);
	int y = luaL_checkinteger(L,2);

	uint32 colour = gui_getcolour(L,3);

//	if (!gui_check_boundary(x, y))
//		luaL_error(L,"bad coordinates");

	gui_prepare();

	gui_drawpixel_internal(x, y, colour);

	return 0;
}

// gui.drawline(x1,y1,x2,y2,color,skipFirst)
static int gui_drawline(lua_State *L) {

	int x1,y1,x2,y2;
	uint32 color;
	x1 = luaL_checkinteger(L,1);
	y1 = luaL_checkinteger(L,2);
	x2 = luaL_checkinteger(L,3);
	y2 = luaL_checkinteger(L,4);
	color = gui_optcolour(L,5,LUA_BUILD_PIXEL(255, 255, 255, 255));
	int skipFirst = lua_toboolean(L,6);

	gui_prepare();

	gui_drawline_internal(x2, y2, x1, y1, !skipFirst, color);

	return 0;
}

// gui.drawbox(x1, y1, x2, y2, fillcolor, outlinecolor)
static int gui_drawbox(lua_State *L) {

	int x1,y1,x2,y2;
	uint32 fillcolor;
	uint32 outlinecolor;

	x1 = luaL_checkinteger(L,1);
	y1 = luaL_checkinteger(L,2);
	x2 = luaL_checkinteger(L,3);
	y2 = luaL_checkinteger(L,4);
	fillcolor = gui_optcolour(L,5,LUA_BUILD_PIXEL(63, 255, 255, 255));
	outlinecolor = gui_optcolour(L,6,LUA_BUILD_PIXEL(255, LUA_PIXEL_R(fillcolor), LUA_PIXEL_G(fillcolor), LUA_PIXEL_B(fillcolor)));

	if (x1 > x2) 
		swap<int>(x1, x2);
	if (y1 > y2) 
		swap<int>(y1, y2);

	gui_prepare();

	gui_drawbox_internal(x1, y1, x2, y2, outlinecolor);
	if ((x2 - x1) >= 2 && (y2 - y1) >= 2)
		gui_fillbox_internal(x1+1, y1+1, x2-1, y2-1, fillcolor);

	return 0;
}

// gui.drawcircle(x0, y0, radius, colour)
static int gui_drawcircle(lua_State *L) {

	int x, y, r;
	uint32 colour;

	x = luaL_checkinteger(L,1);
	y = luaL_checkinteger(L,2);
	r = luaL_checkinteger(L,3);
	colour = gui_getcolour(L,4);

	gui_prepare();

	gui_drawcircle_internal(x, y, r, colour);

	return 0;
}

// gui.fillbox(x1, y1, x2, y2, colour)
static int gui_fillbox(lua_State *L) {

	int x1,y1,x2,y2;
	uint32 colour;

	x1 = luaL_checkinteger(L,1);
	y1 = luaL_checkinteger(L,2);
	x2 = luaL_checkinteger(L,3);
	y2 = luaL_checkinteger(L,4);
	colour = gui_getcolour(L,5);

//	if (!gui_check_boundary(x1, y1))
//		luaL_error(L,"bad coordinates");
//
//	if (!gui_check_boundary(x2, y2))
//		luaL_error(L,"bad coordinates");

	gui_prepare();

	gui_fillbox_internal(x1, y1, x2, y2, colour);

	return 0;
}

// gui.fillcircle(x0, y0, radius, colour)
static int gui_fillcircle(lua_State *L) {

	int x, y, r;
	uint32 colour;

	x = luaL_checkinteger(L,1);
	y = luaL_checkinteger(L,2);
	r = luaL_checkinteger(L,3);
	colour = gui_getcolour(L,4);

	gui_prepare();

	gui_fillcircle_internal(x, y, r, colour);

	return 0;
}

static int gui_getpixel(lua_State *L) {

	int x = luaL_checkinteger(L, 1);
	int y = luaL_checkinteger(L, 2);

	if(!gui_check_boundary(x,y))
	{
		lua_pushinteger(L, 0);
		lua_pushinteger(L, 0);
		lua_pushinteger(L, 0);
	}
	else
	{
		int pitch = GFX.Pitch;
		int width = IPPU.RenderedScreenWidth;
		int xscale = (width == 512) ? 2 : 1;
#if 0
		switch(bpp)
		{
		case 16:
#endif
			{
				uint16 *screen = (uint16*) GFX.Screen;
				uint16 pix = screen[y*pitch/2 + (x*xscale/*+x2*/)];
				lua_pushinteger(L, (pix >> 8) & 0xF8); // red
				lua_pushinteger(L, (pix >> 3) & 0xFC); // green
				lua_pushinteger(L, (pix << 3) & 0xF8); // blue
			}
#if 0
			break;
		case 24:
			{
				uint8 *screen = (uint8*) s;
				lua_pushinteger(L, screen[y*pitch + (x*xscale/*+x2*/)*3 + 2]); // red
				lua_pushinteger(L, screen[y*pitch + (x*xscale/*+x2*/)*3 + 1]); // green
				lua_pushinteger(L, screen[y*pitch + (x*xscale/*+x2*/)*3 + 0]); // blue
			}
			break;
		case 32:
			{
				uint8 *screen = (uint8*) s;
				lua_pushinteger(L, screen[y*pitch + (x*xscale/*+x2*/)*4 + 2]); // red
				lua_pushinteger(L, screen[y*pitch + (x*xscale/*+x2*/)*4 + 1]); // green
				lua_pushinteger(L, screen[y*pitch + (x*xscale/*+x2*/)*4 + 0]); // blue
			}
			break;
		default:
			lua_pushinteger(L, 0);
			lua_pushinteger(L, 0);
			lua_pushinteger(L, 0);
			break;
		}
#endif
	}

	return 3;
}

static int gui_parsecolor(lua_State *L)
{
	int r, g, b, a;
	uint32 color = gui_getcolour(L,1);
	LUA_DECOMPOSE_PIXEL(color, a, r, g, b);
	lua_pushinteger(L, r);
	lua_pushinteger(L, g);
	lua_pushinteger(L, b);
	lua_pushinteger(L, a);
	return 4;
}

// gui.gdscreenshot()
//
//  Returns a screen shot as a string in gd's v1 file format.
//  This allows us to make screen shots available without gd installed locally.
//  Users can also just grab pixels via substring selection.
//
//  I think...  Does lua support grabbing byte values from a string? // yes, string.byte(str,offset)
//  Well, either way, just install gd and do what you like with it.
//  It really is easier that way.
// example: gd.createFromGdStr(gui.gdscreenshot()):png("outputimage.png")
static int gui_gdscreenshot(lua_State *L) {

	int width = IPPU.RenderedScreenWidth;
	int height = IPPU.RenderedScreenHeight;

	int imgwidth=width;
	int imgheight=height;
	if(Settings.StretchScreenshots==1){
		if(width<=256 && height>SNES_HEIGHT_EXTENDED) imgwidth=width<<1;
		if(width>256 && height<=SNES_HEIGHT_EXTENDED) imgheight=height<<1;
	} else if(Settings.StretchScreenshots==2){
		if(width<=256) imgwidth=width<<1;
		if(height<=SNES_HEIGHT_EXTENDED) imgheight=height<<1;
	}

	int size = 11 + imgwidth * imgheight * 4;
	char* str = new char[size+1];
	str[size] = 0;
	unsigned char* ptr = (unsigned char*)str;

	// GD format header for truecolor image (11 bytes)
	*ptr++ = (65534 >> 8) & 0xFF;
	*ptr++ = (65534     ) & 0xFF;
	*ptr++ = (imgwidth >> 8) & 0xFF;
	*ptr++ = (imgwidth     ) & 0xFF;
	*ptr++ = (imgheight >> 8) & 0xFF;
	*ptr++ = (imgheight     ) & 0xFF;
	*ptr++ = 1;
	*ptr++ = 255;
	*ptr++ = 255;
	*ptr++ = 255;
	*ptr++ = 255;

	uint8 *screen=(uint8 *)GFX.Screen;
	for(int y=0; y<height; y++, screen+=GFX.Pitch){
		for(int rows=0; rows<((imgheight!=height)?2:1); rows++){
			for(int x=0; x<width; x++){
				uint32 r, g, b;
				DECOMPOSE_PIXEL((*(uint16 *)(screen+2*x)), r, g, b);

				// Diagram:   000XXxxx -> XXxxx000 ;// -> XXxxxXXx
				r = ((r << 3) & 0xff) ;// | ((r >> 2) & 0xff);
				g = ((g << 3) & 0xff) ;// | ((g >> 2) & 0xff);
				b = ((b << 3) & 0xff) ;// | ((b >> 2) & 0xff);

				// overlay uncommited Lua drawings if needed
				if (Settings.LuaDrawingsInScreen && gui_used && gui_enabled) {
					const uint8 gui_alpha = gui_data[(y*LUA_SCREEN_WIDTH+x)*4+3];
					const uint8 gui_red   = gui_data[(y*LUA_SCREEN_WIDTH+x)*4+2];
					const uint8 gui_green = gui_data[(y*LUA_SCREEN_WIDTH+x)*4+1];
					const uint8 gui_blue  = gui_data[(y*LUA_SCREEN_WIDTH+x)*4];

					if (gui_alpha == 255) {
						// direct copy
						r = gui_red;
						g = gui_green;
						b = gui_blue;
					}
					else if (gui_alpha != 0) {
						// alpha-blending
						r = (((int) gui_red   - r) * gui_alpha / 255 + r) & 255;
						g = (((int) gui_green - g) * gui_alpha / 255 + g) & 255;
						b = (((int) gui_blue  - b) * gui_alpha / 255 + b) & 255;
					}
				}

				*ptr++ = 0;
				*ptr++ = r;
				*ptr++ = g;
				*ptr++ = b;
				if(imgwidth!=width){
					*ptr++ = 0;
					*ptr++ = r;
					*ptr++ = g;
					*ptr++ = b;
				}
			}
		}
	}

	lua_pushlstring(L, str, size);
	delete[] str;
	return 1;
}


// gui.opacity(number alphaValue)
// sets the transparency of subsequent draw calls
// 0.0 is completely transparent, 1.0 is completely opaque
// non-integer values are supported and meaningful, as are values greater than 1.0
// it is not necessary to use this function to get transparency (or the less-recommended gui.transparency() either),
// because you can provide an alpha value in the color argument of each draw call.
// however, it can be convenient to be able to globally modify the drawing transparency
static int gui_setopacity(lua_State *L) {
	double opacF = luaL_checknumber(L,1);
	transparencyModifier = (int) (opacF * 255);
	if (transparencyModifier < 0)
		transparencyModifier = 0;
	return 0;
}

// gui.transparency(int strength)
//
//  0 = solid, 
static int gui_transparency(lua_State *L) {
	double trans = luaL_checknumber(L,1);
	transparencyModifier = (int) ((4.0 - trans) / 4.0 * 255);
	if (transparencyModifier < 0)
		transparencyModifier = 0;
	return 0;
}

// gui.clearuncommitted()
//
//  undoes uncommitted drawing commands
static int gui_clearuncommitted(lua_State *L) {
	S9xLuaClearGui();
	return 0;
}



static const uint32 Small_Font_Data[] =
{
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,			// 32	 
	0x00000000, 0x00000300, 0x00000400, 0x00000500, 0x00000000, 0x00000700, 0x00000000,			// 33	!
	0x00000000, 0x00040002, 0x00050003, 0x00000000, 0x00000000, 0x00000000, 0x00000000,			// 34	"
	0x00000000, 0x00040002, 0x00050403, 0x00060004, 0x00070605, 0x00080006, 0x00000000,			// 35	#
	0x00000000, 0x00040300, 0x00000403, 0x00000500, 0x00070600, 0x00000706, 0x00000000,			// 36	$
	0x00000000, 0x00000002, 0x00050000, 0x00000500, 0x00000005, 0x00080000, 0x00000000,			// 37	%
	0x00000000, 0x00000300, 0x00050003, 0x00000500, 0x00070005, 0x00080700, 0x00000000,			// 38	&
	0x00000000, 0x00000300, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x00000000,			// 39	'
	0x00000000, 0x00000300, 0x00000003, 0x00000004, 0x00000005, 0x00000700, 0x00000000,			// 40	(
	0x00000000, 0x00000300, 0x00050000, 0x00060000, 0x00070000, 0x00000700, 0x00000000,			// 41	)
	0x00000000, 0x00000000, 0x00000400, 0x00060504, 0x00000600, 0x00080006, 0x00000000,			// 42	*
	0x00000000, 0x00000000, 0x00000400, 0x00060504, 0x00000600, 0x00000000, 0x00000000,			// 43	+
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000600, 0x00000700, 0x00000007,			// 44	,
	0x00000000, 0x00000000, 0x00000000, 0x00060504, 0x00000000, 0x00000000, 0x00000000,			// 45	-
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000700, 0x00000000,			// 46	.
	0x00030000, 0x00040000, 0x00000400, 0x00000500, 0x00000005, 0x00000006, 0x00000000,			// 47	/
	0x00000000, 0x00000300, 0x00050003, 0x00060004, 0x00070005, 0x00000700, 0x00000000,			// 48	0
	0x00000000, 0x00000300, 0x00000403, 0x00000500, 0x00000600, 0x00000700, 0x00000000,			// 49	1
	0x00000000, 0x00000302, 0x00050000, 0x00000500, 0x00000005, 0x00080706, 0x00000000,			// 50	2
	0x00000000, 0x00000302, 0x00050000, 0x00000504, 0x00070000, 0x00000706, 0x00000000,			// 51	3
	0x00000000, 0x00000300, 0x00000003, 0x00060004, 0x00070605, 0x00080000, 0x00000000,			// 52	4
	0x00000000, 0x00040302, 0x00000003, 0x00000504, 0x00070000, 0x00000706, 0x00000000,			// 53	5
	0x00000000, 0x00000300, 0x00000003, 0x00000504, 0x00070005, 0x00000700, 0x00000000,			// 54	6
	0x00000000, 0x00040302, 0x00050000, 0x00000500, 0x00000600, 0x00000700, 0x00000000,			// 55	7
	0x00000000, 0x00000300, 0x00050003, 0x00000500, 0x00070005, 0x00000700, 0x00000000,			// 56	8
	0x00000000, 0x00000300, 0x00050003, 0x00060500, 0x00070000, 0x00000700, 0x00000000,			// 57	9
	0x00000000, 0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000700, 0x00000000,			// 58	:
	0x00000000, 0x00000000, 0x00000000, 0x00000500, 0x00000000, 0x00000700, 0x00000007,			// 59	;
	0x00000000, 0x00040000, 0x00000400, 0x00000004, 0x00000600, 0x00080000, 0x00000000,			// 60	<
	0x00000000, 0x00000000, 0x00050403, 0x00000000, 0x00070605, 0x00000000, 0x00000000,			// 61	=
	0x00000000, 0x00000002, 0x00000400, 0x00060000, 0x00000600, 0x00000006, 0x00000000,			// 62	>
	0x00000000, 0x00000302, 0x00050000, 0x00000500, 0x00000000, 0x00000700, 0x00000000,			// 63	?
	0x00000000, 0x00000300, 0x00050400, 0x00060004, 0x00070600, 0x00000000, 0x00000000,			// 64	@
	0x00000000, 0x00000300, 0x00050003, 0x00060504, 0x00070005, 0x00080006, 0x00000000,			// 65	A
	0x00000000, 0x00000302, 0x00050003, 0x00000504, 0x00070005, 0x00000706, 0x00000000,			// 66	B
	0x00000000, 0x00040300, 0x00000003, 0x00000004, 0x00000005, 0x00080700, 0x00000000,			// 67	C
	0x00000000, 0x00000302, 0x00050003, 0x00060004, 0x00070005, 0x00000706, 0x00000000,			// 68	D
	0x00000000, 0x00040302, 0x00000003, 0x00000504, 0x00000005, 0x00080706, 0x00000000,			// 69	E
	0x00000000, 0x00040302, 0x00000003, 0x00000504, 0x00000005, 0x00000006, 0x00000000,			// 70	F
	0x00000000, 0x00040300, 0x00000003, 0x00060004, 0x00070005, 0x00080700, 0x00000000,			// 71	G
	0x00000000, 0x00040002, 0x00050003, 0x00060504, 0x00070005, 0x00080006, 0x00000000,			// 72	H
	0x00000000, 0x00000300, 0x00000400, 0x00000500, 0x00000600, 0x00000700, 0x00000000,			// 73	I
	0x00000000, 0x00040000, 0x00050000, 0x00060000, 0x00070005, 0x00000700, 0x00000000,			// 74	J
	0x00000000, 0x00040002, 0x00050003, 0x00000504, 0x00070005, 0x00080006, 0x00000000,			// 75	K
	0x00000000, 0x00000002, 0x00000003, 0x00000004, 0x00000005, 0x00080706, 0x00000000,			// 76	l
	0x00000000, 0x00040002, 0x00050403, 0x00060004, 0x00070005, 0x00080006, 0x00000000,			// 77	M
	0x00000000, 0x00000302, 0x00050003, 0x00060004, 0x00070005, 0x00080006, 0x00000000,			// 78	N
	0x00000000, 0x00040302, 0x00050003, 0x00060004, 0x00070005, 0x00080706, 0x00000000,			// 79	O
	0x00000000, 0x00000302, 0x00050003, 0x00000504, 0x00000005, 0x00000006, 0x00000000,			// 80	P
	0x00000000, 0x00040302, 0x00050003, 0x00060004, 0x00070005, 0x00080706, 0x00090000,			// 81	Q
	0x00000000, 0x00000302, 0x00050003, 0x00000504, 0x00070005, 0x00080006, 0x00000000,			// 82	R
	0x00000000, 0x00040300, 0x00000003, 0x00000500, 0x00070000, 0x00000706, 0x00000000,			// 83	S
	0x00000000, 0x00040302, 0x00000400, 0x00000500, 0x00000600, 0x00000700, 0x00000000,			// 84	T
	0x00000000, 0x00040002, 0x00050003, 0x00060004, 0x00070005, 0x00080706, 0x00000000,			// 85	U
	0x00000000, 0x00040002, 0x00050003, 0x00060004, 0x00000600, 0x00000700, 0x00000000,			// 86	V
	0x00000000, 0x00040002, 0x00050003, 0x00060004, 0x00070605, 0x00080006, 0x00000000,			// 87	W
	0x00000000, 0x00040002, 0x00050003, 0x00000500, 0x00070005, 0x00080006, 0x00000000,			// 88	X
	0x00000000, 0x00040002, 0x00050003, 0x00000500, 0x00000600, 0x00000700, 0x00000000,			// 89	Y
	0x00000000, 0x00040302, 0x00050000, 0x00000500, 0x00000005, 0x00080706, 0x00000000,			// 90	Z
	0x00000000, 0x00040300, 0x00000400, 0x00000500, 0x00000600, 0x00080700, 0x00000000,			// 91	[
	0x00000000, 0x00000002, 0x00000400, 0x00000500, 0x00070000, 0x00080000, 0x00000000,			// 92	'\'
	0x00000000, 0x00000302, 0x00000400, 0x00000500, 0x00000600, 0x00000706, 0x00000000,			// 93	]
	0x00000000, 0x00000300, 0x00050003, 0x00000000, 0x00000000, 0x00000000, 0x00000000,			// 94	^
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00080706, 0x00000000,			// 95	_
	0x00000000, 0x00000002, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x00000000,			// 96	`
	0x00000000, 0x00000000, 0x00050400, 0x00060004, 0x00070005, 0x00080700, 0x00000000,			// 97	a
	0x00000000, 0x00000002, 0x00000003, 0x00000504, 0x00070005, 0x00000706, 0x00000000,			// 98	b
	0x00000000, 0x00000000, 0x00050400, 0x00000004, 0x00000005, 0x00080700, 0x00000000,			// 99	c
	0x00000000, 0x00040000, 0x00050000, 0x00060500, 0x00070005, 0x00080700, 0x00000000,			// 100	d
	0x00000000, 0x00000000, 0x00050400, 0x00060504, 0x00000005, 0x00080700, 0x00000000,			// 101	e
	0x00000000, 0x00040300, 0x00000003, 0x00000504, 0x00000005, 0x00000006, 0x00000000,			// 102	f
	0x00000000, 0x00000000, 0x00050400, 0x00060004, 0x00070600, 0x00080000, 0x00000807,			// 103	g
	0x00000000, 0x00000002, 0x00000003, 0x00000504, 0x00070005, 0x00080006, 0x00000000,			// 104	h
	0x00000000, 0x00000300, 0x00000000, 0x00000500, 0x00000600, 0x00000700, 0x00000000,			// 105	i
	0x00000000, 0x00000300, 0x00000000, 0x00000500, 0x00000600, 0x00000700, 0x00000007,			// 106	j
	0x00000000, 0x00000002, 0x00000003, 0x00060004, 0x00000605, 0x00080006, 0x00000000,			// 107	k
	0x00000000, 0x00000300, 0x00000400, 0x00000500, 0x00000600, 0x00080000, 0x00000000,			// 108	l
	0x00000000, 0x00000000, 0x00050003, 0x00060504, 0x00070005, 0x00080006, 0x00000000,			// 109	m
	0x00000000, 0x00000000, 0x00000403, 0x00060004, 0x00070005, 0x00080006, 0x00000000,			// 110	n
	0x00000000, 0x00000000, 0x00000400, 0x00060004, 0x00070005, 0x00000700, 0x00000000,			// 111	o
	0x00000000, 0x00000000, 0x00000400, 0x00060004, 0x00000605, 0x00000006, 0x00000007,			// 112	p
	0x00000000, 0x00000000, 0x00000400, 0x00060004, 0x00070600, 0x00080000, 0x00090000,			// 113	q
	0x00000000, 0x00000000, 0x00050003, 0x00000504, 0x00000005, 0x00000006, 0x00000000,			// 114	r
	0x00000000, 0x00000000, 0x00050400, 0x00000004, 0x00070600, 0x00000706, 0x00000000,			// 115	s
	0x00000000, 0x00000300, 0x00050403, 0x00000500, 0x00000600, 0x00080000, 0x00000000,			// 116	t
	0x00000000, 0x00000000, 0x00050003, 0x00060004, 0x00070005, 0x00080700, 0x00000000,			// 117	u
	0x00000000, 0x00000000, 0x00050003, 0x00060004, 0x00070005, 0x00000700, 0x00000000,			// 118	v
	0x00000000, 0x00000000, 0x00050003, 0x00060004, 0x00070605, 0x00080006, 0x00000000,			// 119	w
	0x00000000, 0x00000000, 0x00050003, 0x00000500, 0x00070005, 0x00080006, 0x00000000,			// 120	x
	0x00000000, 0x00000000, 0x00050003, 0x00060004, 0x00000600, 0x00000700, 0x00000007,			// 121	y
	0x00000000, 0x00000000, 0x00050403, 0x00000500, 0x00000005, 0x00080706, 0x00000000,			// 122	z
	0x00000000, 0x00040300, 0x00000400, 0x00000504, 0x00000600, 0x00080700, 0x00000000,			// 123	{
	0x00000000, 0x00000300, 0x00000400, 0x00000000, 0x00000600, 0x00000700, 0x00000000,			// 124	|
	0x00000000, 0x00000302, 0x00000400, 0x00060500, 0x00000600, 0x00000706, 0x00000000,			// 125	}
	0x00000000, 0x00000302, 0x00050000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,			// 126	~
	0x00000000, 0x00000000, 0x00000400, 0x00060004, 0x00070605, 0x00000000, 0x00000000,			// 127	
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

static void PutTextInternal (const char *str, int len, short x, short y, int color, int backcolor)
{
	int Opac = (color >> 24) & 0xFF;
	int backOpac = (backcolor >> 24) & 0xFF;
	int origX = x;

	if(!Opac && !backOpac)
		return;

	while(*str && len && y < LUA_SCREEN_HEIGHT)
	{
		int c = *str++;
		while (x > LUA_SCREEN_WIDTH && c != '\n') {
			c = *str;
			if (c == '\0')
				break;
			str++;
		}
		if(c == '\n')
		{
			x = origX;
			y += 8;
			continue;
		}
		else if(c == '\t') // just in case
		{
			const int tabSpace = 8;
			x += (tabSpace-(((x-origX)/4)%tabSpace))*4;
			continue;
		}
		if((unsigned int)(c-32) >= 96)
			continue;
		const unsigned char* Cur_Glyph = (const unsigned char*)&Small_Font_Data + (c-32)*7*4;

		for(int y2 = 0; y2 < 8; y2++)
		{
			unsigned int glyphLine = *((unsigned int*)Cur_Glyph + y2);
			for(int x2 = -1; x2 < 4; x2++)
			{
				int shift = x2 << 3;
				int mask = 0xFF << shift;
				int intensity = (glyphLine & mask) >> shift;

				if(intensity && x2 >= 0 && y2 < 7)
				{
					//int xdraw = max(0,min(LUA_SCREEN_WIDTH - 1,x+x2));
					//int ydraw = max(0,min(LUA_SCREEN_HEIGHT - 1,y+y2));
					//gui_drawpixel_fast(xdraw, ydraw, color);
					gui_drawpixel_internal(x+x2, y+y2, color);
				}
				else if(backOpac)
				{
					for(int y3 = max(0,y2-1); y3 <= min(6,y2+1); y3++)
					{
						unsigned int glyphLine = *((unsigned int*)Cur_Glyph + y3);
						for(int x3 = max(0,x2-1); x3 <= min(3,x2+1); x3++)
						{
							int shift = x3 << 3;
							int mask = 0xFF << shift;
							intensity |= (glyphLine & mask) >> shift;
							if (intensity)
								goto draw_outline; // speedup?
						}
					}
draw_outline:
					if(intensity)
					{
						//int xdraw = max(0,min(LUA_SCREEN_WIDTH - 1,x+x2));
						//int ydraw = max(0,min(LUA_SCREEN_HEIGHT - 1,y+y2));
						//gui_drawpixel_fast(xdraw, ydraw, backcolor);
						gui_drawpixel_internal(x+x2, y+y2, backcolor);
					}
				}
			}
		}

		x += 4;
		len--;
	}
}

static int strlinelen(const char* string)
{
	const char* s = string;
	while(*s && *s != '\n')
		s++;
	if(*s)
		s++;
	return s - string;
}

static void LuaDisplayString (const char *string, int y, int x, uint32 color, uint32 outlineColor)
{
	if(!string)
		return;

	gui_prepare();

	PutTextInternal(string, strlen(string), x, y, color, outlineColor);
/*
	const char* ptr = string;
	while(*ptr && y < LUA_SCREEN_HEIGHT)
	{
		int len = strlinelen(ptr);
		int skip = 0;
		if(len < 1) len = 1;

		// break up the line if it's too long to display otherwise
		if(len > 63)
		{
			len = 63;
			const char* ptr2 = ptr + len-1;
			for(int j = len-1; j; j--, ptr2--)
			{
				if(*ptr2 == ' ' || *ptr2 == '\t')
				{
					len = j;
					skip = 1;
					break;
				}
			}
		}

		int xl = 0;
		int yl = 0;
		int xh = (LUA_SCREEN_WIDTH - 1 - 1) - 4*len;
		int yh = LUA_SCREEN_HEIGHT - 1;
		int x2 = min(max(x,xl),xh);
		int y2 = min(max(y,yl),yh);

		PutTextInternal(ptr,len,x2,y2,color,outlineColor);

		ptr += len + skip;
		y += 8;
	}
*/
}

// gui.text(int x, int y, string msg)
//
//  Displays the given text on the screen, using the same font and techniques as the
//  main HUD.
static int gui_text(lua_State *L) {

	extern int font_height;
	const char *msg;
	int x, y;
	uint32 colour, borderColour;

	x = luaL_checkinteger(L,1);
	y = luaL_checkinteger(L,2);
	msg = luaL_checkstring(L,3);

//	if (x < 0 || x >= LUA_SCREEN_WIDTH || y < 0 || y >= (LUA_SCREEN_HEIGHT - font_height))
//		luaL_error(L,"bad coordinates");

	uint8 displayColorR = (Settings.DisplayColor >> 11) << 3;
	uint8 displayColorG = ((Settings.DisplayColor >> 6) & 0x1f) << 3;
	uint8 displayColorB = (Settings.DisplayColor & 0x1f) << 3;
	colour = gui_optcolour(L,4,LUA_BUILD_PIXEL(255, displayColorR, displayColorG, displayColorB));
	borderColour = gui_optcolour(L,5,LUA_BUILD_PIXEL(255, 0, 0, 0));

	gui_prepare();

	LuaDisplayString(msg, y, x, colour, borderColour);

	return 0;

}

// gui.gdoverlay([int dx=0, int dy=0,] string str [, sx=0, sy=0, sw, sh] [, float alphamul=1.0])
//
//  Overlays the given image on the screen.
// example: gui.gdoverlay(gd.createFromPng("myimage.png"):gdStr())
static int gui_gdoverlay(lua_State *L) {

	int argCount = lua_gettop(L);

	int xStartDst = 0;
	int yStartDst = 0;
	int xStartSrc = 0;
	int yStartSrc = 0;

	int index = 1;
	if(lua_type(L,index) == LUA_TNUMBER)
	{
		xStartDst = lua_tointeger(L,index++);
		if(lua_type(L,index) == LUA_TNUMBER)
			yStartDst = lua_tointeger(L,index++);
	}

	luaL_checktype(L,index,LUA_TSTRING);
	const unsigned char* ptr = (const unsigned char*)lua_tostring(L,index++);

	if (ptr[0] != 255 || (ptr[1] != 254 && ptr[1] != 255))
		luaL_error(L, "bad image data");
	bool trueColor = (ptr[1] == 254);
	ptr += 2;
	int imgwidth = *ptr++ << 8;
	imgwidth |= *ptr++;
	int width = imgwidth;
	int imgheight = *ptr++ << 8;
	imgheight |= *ptr++;
	int height = imgheight;
	if ((!trueColor && *ptr) || (trueColor && !*ptr))
		luaL_error(L, "bad image data");
	ptr++;
	int pitch = imgwidth * (trueColor?4:1);

	if ((argCount - index + 1) >= 4) {
		xStartSrc = luaL_checkinteger(L,index++);
		yStartSrc = luaL_checkinteger(L,index++);
		width = luaL_checkinteger(L,index++);
		height = luaL_checkinteger(L,index++);
	}

	int alphaMul = transparencyModifier;
	if(lua_isnumber(L, index))
		alphaMul = (int)(alphaMul * lua_tonumber(L, index++));
	if(alphaMul <= 0)
		return 0;

	// since there aren't that many possible opacity levels,
	// do the opacity modification calculations beforehand instead of per pixel
	int opacMap[256];
	for(int i = 0; i < 128; i++)
	{
		int opac = 255 - ((i << 1) | (i & 1)); // gdAlphaMax = 127, not 255
		opac = (opac * alphaMul) / 255;
		if(opac < 0) opac = 0;
		if(opac > 255) opac = 255;
		opacMap[i] = opac;
	}
	for(int i = 128; i < 256; i++)
		opacMap[i] = 0; // what should we do for them, actually?

	int colorsTotal = 0;
	if (!trueColor) {
		colorsTotal = *ptr++ << 8;
		colorsTotal |= *ptr++;
	}
	int transparent = *ptr++ << 24;
	transparent |= *ptr++ << 16;
	transparent |= *ptr++ << 8;
	transparent |= *ptr++;
	struct { uint8 r, g, b, a; } pal[256];
	if (!trueColor) for (int i = 0; i < 256; i++) {
		pal[i].r = *ptr++;
		pal[i].g = *ptr++;
		pal[i].b = *ptr++;
		pal[i].a = opacMap[*ptr++];
	}

	// some of clippings
	if (xStartSrc < 0) {
		width += xStartSrc;
		xStartDst -= xStartSrc;
		xStartSrc = 0;
	}
	if (yStartSrc < 0) {
		height += yStartSrc;
		yStartDst -= yStartSrc;
		yStartSrc = 0;
	}
	if (xStartSrc+width >= imgwidth)
		width = imgwidth - xStartSrc;
	if (yStartSrc+height >= imgheight)
		height = imgheight - yStartSrc;
	if (xStartDst < 0) {
		width += xStartDst;
		if (width <= 0)
			return 0;
		xStartSrc = -xStartDst;
		xStartDst = 0;
	}
	if (yStartDst < 0) {
		height += yStartDst;
		if (height <= 0)
			return 0;
		yStartSrc = -yStartDst;
		yStartDst = 0;
	}
	if (xStartDst+width >= LUA_SCREEN_WIDTH)
		width = LUA_SCREEN_WIDTH - xStartDst;
	if (yStartDst+height >= LUA_SCREEN_HEIGHT)
		height = LUA_SCREEN_HEIGHT - yStartDst;
	if (width <= 0 || height <= 0)
		return 0; // out of screen or invalid size

	gui_prepare();

	const uint8* pix = (const uint8*)(&ptr[yStartSrc*pitch + (xStartSrc*(trueColor?4:1))]);
	int bytesToNextLine = pitch - (width * (trueColor?4:1));
	if (trueColor)
		for (int y = yStartDst; y < height+yStartDst && y < LUA_SCREEN_HEIGHT; y++, pix += bytesToNextLine) {
			for (int x = xStartDst; x < width+xStartDst && x < LUA_SCREEN_WIDTH; x++, pix += 4) {
				gui_drawpixel_fast(x, y, LUA_BUILD_PIXEL(opacMap[pix[0]], pix[1], pix[2], pix[3]));
			}
		}
	else
		for (int y = yStartDst; y < height+yStartDst && y < LUA_SCREEN_HEIGHT; y++, pix += bytesToNextLine) {
			for (int x = xStartDst; x < width+xStartDst && x < LUA_SCREEN_WIDTH; x++, pix++) {
				gui_drawpixel_fast(x, y, LUA_BUILD_PIXEL(pal[*pix].a, pal[*pix].r, pal[*pix].g, pal[*pix].b));
			}
		}

	return 0;
}


// function gui.register(function f)
//
//  This function will be called just before a graphical update.
//  More complicated, but doesn't suffer any frame delays.
//  Nil will be accepted in place of a function to erase
//  a previously registered function, and the previous function
//  (if any) is returned, or nil if none.
static int gui_register(lua_State *L) {

	// We'll do this straight up.


	// First set up the stack.
	lua_settop(L,1);
	
	// Verify the validity of the entry
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);

	// Get the old value
	lua_getfield(L, LUA_REGISTRYINDEX, guiCallbackTable);
	
	// Save the new value
	lua_pushvalue(L,1);
	lua_setfield(L, LUA_REGISTRYINDEX, guiCallbackTable);
	
	// The old value is on top of the stack. Return it.
	return 1;

}

static int doPopup(lua_State *L, const char* deftype, const char* deficon) {
	const char *str = luaL_checkstring(L, 1);
	const char* type = lua_type(L,2) == LUA_TSTRING ? lua_tostring(L,2) : deftype;
	const char* icon = lua_type(L,3) == LUA_TSTRING ? lua_tostring(L,3) : deficon;

	int itype = -1, iters = 0;
	while(itype == -1 && iters++ < 2)
	{
		if(!stricmp(type, "ok")) itype = 0;
		else if(!stricmp(type, "yesno")) itype = 1;
		else if(!stricmp(type, "yesnocancel")) itype = 2;
		else if(!stricmp(type, "okcancel")) itype = 3;
		else if(!stricmp(type, "abortretryignore")) itype = 4;
		else type = deftype;
	}
	assert(itype >= 0 && itype <= 4);
	if(!(itype >= 0 && itype <= 4)) itype = 0;

	int iicon = -1; iters = 0;
	while(iicon == -1 && iters++ < 2)
	{
		if(!stricmp(icon, "message") || !stricmp(icon, "notice")) iicon = 0;
		else if(!stricmp(icon, "question")) iicon = 1;
		else if(!stricmp(icon, "warning")) iicon = 2;
		else if(!stricmp(icon, "error")) iicon = 3;
		else icon = deficon;
	}
	assert(iicon >= 0 && iicon <= 3);
	if(!(iicon >= 0 && iicon <= 3)) iicon = 0;

	static const char * const titles [] = {"Notice", "Question", "Warning", "Error"};
	const char* answer = "ok";

#ifdef WIN32
	static const int etypes [] = {MB_OK, MB_YESNO, MB_YESNOCANCEL, MB_OKCANCEL, MB_ABORTRETRYIGNORE};
	static const int eicons [] = {MB_ICONINFORMATION, MB_ICONQUESTION, MB_ICONWARNING, MB_ICONERROR};
	int ianswer = MessageBox(GUI.hWnd, str, titles[iicon], etypes[itype] | eicons[iicon]);
	switch(ianswer)
	{
		case IDOK: answer = "ok"; break;
		case IDCANCEL: answer = "cancel"; break;
		case IDABORT: answer = "abort"; break;
		case IDRETRY: answer = "retry"; break;
		case IDIGNORE: answer = "ignore"; break;
		case IDYES: answer = "yes"; break;
		case IDNO: answer = "no"; break;
	}

	lua_pushstring(L, answer);
	return 1;
#else

	char *t;
#ifdef __linux

	// The Linux backend has a "FromPause" variable.
	// If set to 1, assume some known external event has screwed with the flow of time.
	// Since this pauses the emulator waiting for a response, we set it to 1.
	extern int FromPause;
	FromPause = 1;


	int pid; // appease compiler

	// Before doing any work, verify the correctness of the parameters.
	if (strcmp(type, "ok") == 0)
		t = "OK:100";
	else if (strcmp(type, "yesno") == 0)
		t = "Yes:100,No:101";
	else if (strcmp(type, "yesnocancel") == 0)
		t = "Yes:100,No:101,Cancel:102";
	else
		return luaL_error(L, "invalid popup type \"%s\"", type);

	// Can we find a copy of xmessage? Search the path.
	
	char *path = strdup(getenv("PATH"));

	char *current = path;
	
	char *colon;

	int found = 0;

	while (current) {
		colon = strchr(current, ':');
		
		// Clip off the colon.
		*colon++ = 0;
		
		int len = strlen(current);
		char *filename = (char*)malloc(len + 12); // always give excess
		snprintf(filename, len+12, "%s/xmessage", current);
		
		if (access(filename, X_OK) == 0) {
			free(filename);
			found = 1;
			break;
		}
		
		// Failed, move on.
		current = colon;
		free(filename);
		
	}

	free(path);

	// We've found it?
	if (!found)
		goto use_console;

	pid = fork();
	if (pid == 0) {// I'm the virgin sacrifice
	
		// I'm gonna be dead in a matter of microseconds anyways, so wasted memory doesn't matter to me.
		// Go ahead and abuse strdup.
		char * parameters[] = {"xmessage", "-buttons", t, strdup(str), NULL};

		execvp("xmessage", parameters);
		
		// Aw shitty
		perror("exec xmessage");
		exit(1);
	}
	else if (pid < 0) // something went wrong!!! Oh hell... use the console
		goto use_console;
	else {
		// We're the parent. Watch for the child.
		int r;
		int res = waitpid(pid, &r, 0);
		if (res < 0) // wtf?
			goto use_console;
		
		// The return value gets copmlicated...
		if (!WIFEXITED(r)) {
			luaL_error(L, "don't screw with my xmessage process!");
		}
		r = WEXITSTATUS(r);
		
		// We assume it's worked.
		if (r == 0)
		{
			return 0; // no parameters for an OK
		}
		if (r == 100) {
			lua_pushstring(L, "yes");
			return 1;
		}
		if (r == 101) {
			lua_pushstring(L, "no");
			return 1;
		}
		if (r == 102) {
			lua_pushstring(L, "cancel");
			return 1;
		}
		
		// Wtf?
		return luaL_error(L, "popup failed due to unknown results involving xmessage (%d)", r);
	}

use_console:
#endif

	// All else has failed

	if (strcmp(type, "ok") == 0)
		t = "";
	else if (strcmp(type, "yesno") == 0)
		t = "yn";
	else if (strcmp(type, "yesnocancel") == 0)
		t = "ync";
	else
		return luaL_error(L, "invalid popup type \"%s\"", type);

	fprintf(stderr, "Lua Message: %s\n", str);

	while (true) {
		char buffer[64];

		// We don't want parameters
		if (!t[0]) {
			fprintf(stderr, "[Press Enter]");
			fgets(buffer, sizeof(buffer), stdin);
			// We're done
			return 0;

		}
		fprintf(stderr, "(%s): ", t);
		fgets(buffer, sizeof(buffer), stdin);
		
		// Check if the option is in the list
		if (strchr(t, tolower(buffer[0]))) {
			switch (tolower(buffer[0])) {
			case 'y':
				lua_pushstring(L, "yes");
				return 1;
			case 'n':
				lua_pushstring(L, "no");
				return 1;
			case 'c':
				lua_pushstring(L, "cancel");
				return 1;
			default:
				luaL_error(L, "internal logic error in console based prompts for gui.popup");
			
			}
		}
		
		// We fell through, so we assume the user answered wrong and prompt again.
	
	}

	// Nothing here, since the only way out is in the loop.
#endif

}

// string gui.popup(string message, string type = "ok", string icon = "message")
// string input.popup(string message, string type = "yesno", string icon = "question")
static int gui_popup(lua_State *L)
{
	return doPopup(L, "ok", "message");
}
static int input_popup(lua_State *L)
{
	return doPopup(L, "yesno", "question");
}

#ifdef WIN32
const char* s_keyToName[256] =
{
	NULL,
	"leftclick",
	"rightclick",
	NULL,
	"middleclick",
	NULL,
	NULL,
	NULL,
	"backspace",
	"tab",
	NULL,
	NULL,
	NULL,
	"enter",
	NULL,
	NULL,
	"shift", // 0x10
	"control",
	"alt",
	"pause",
	"capslock",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"escape",
	NULL,
	NULL,
	NULL,
	NULL,
	"space", // 0x20
	"pageup",
	"pagedown",
	"end",
	"home",
	"left",
	"up",
	"right",
	"down",
	NULL,
	NULL,
	NULL,
	NULL,
	"insert",
	"delete",
	NULL,
	"0","1","2","3","4","5","6","7","8","9",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"A","B","C","D","E","F","G","H","I","J",
	"K","L","M","N","O","P","Q","R","S","T",
	"U","V","W","X","Y","Z",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"numpad0","numpad1","numpad2","numpad3","numpad4","numpad5","numpad6","numpad7","numpad8","numpad9",
	"numpad*","numpad+",
	NULL,
	"numpad-","numpad.","numpad/",
	"F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12",
	"F13","F14","F15","F16","F17","F18","F19","F20","F21","F22","F23","F24",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"numlock",
	"scrolllock",
	NULL, // 0x92
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, // 0xB9
	"semicolon",
	"plus",
	"comma",
	"minus",
	"period",
	"slash",
	"tilde",
	NULL, // 0xC1
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, // 0xDA
	"leftbracket",
	"backslash",
	"rightbracket",
	"quote",
};

#endif

// input.get()
// takes no input, returns a lua table of entries representing the current input state,
// independent of the joypad buttons the emulated game thinks are pressed
// for example:
//   if the user is holding the W key and the left mouse button
//   and has the mouse at the bottom-right corner of the game screen,
//   then this would return {W=true, leftclick=true, xmouse=255, ymouse=223}
static int input_getcurrentinputstatus(lua_State *L) {
	lua_newtable(L);

#ifdef WIN32
	// keyboard and mouse button status
	{
		unsigned char keys [256];
		if(!GUI.BackgroundInput)
		{
			if(GetKeyboardState(keys))
			{
				for(int i = 1; i < 255; i++)
				{
					int mask = (i == VK_CAPITAL || i == VK_NUMLOCK || i == VK_SCROLL) ? 0x01 : 0x80;
					if(keys[i] & mask)
					{
						const char* name = s_keyToName[i];
						if(name)
						{
							lua_pushboolean(L, true);
							lua_setfield(L, -2, name);
						}
					}
				}
			}
		}
		else // use a slightly different method that will detect background input:
		{
			for(int i = 1; i < 255; i++)
			{
				const char* name = s_keyToName[i];
				if(name)
				{
					int active;
					if(i == VK_CAPITAL || i == VK_NUMLOCK || i == VK_SCROLL)
						active = GetKeyState(i) & 0x01;
					else
						active = GetAsyncKeyState(i) & 0x8000;
					if(active)
					{
						lua_pushboolean(L, true);
						lua_setfield(L, -2, name);
					}
				}
			}
		}
	}
	// mouse position in game screen pixel coordinates
	{
		extern BOOL GetCursorPosSNES(LPPOINT lpPoint, bool clip);

		POINT mouse;
		GetCursorPosSNES(&mouse, false); // mouse coordinate clip is disabled because it made some useful things impossible to do, whereas the user can always clip it themselves if they want that
		if (IPPU.RenderedScreenWidth > SNES_WIDTH)
			mouse.x /= 2;

		lua_pushinteger(L, mouse.x);
		lua_setfield(L, -2, "xmouse");
		lua_pushinteger(L, mouse.y);
		lua_setfield(L, -2, "ymouse");
	}
#else
	// NYI (well, return an empty table)
#endif

	return 1;
}

// same as math.random, but uses SFMT instead of C rand()
// FIXME: this function doesn't care multi-instance,
//        original math.random either though (Lua 5.1)
static int sfmt_random (lua_State *L) {
	lua_Number r = (lua_Number) genrand_real2();
	switch (lua_gettop(L)) {  // check number of arguments
		case 0: {  // no arguments
			lua_pushnumber(L, r);  // Number between 0 and 1
			break;
		}
		case 1: {  // only upper limit
			int u = luaL_checkint(L, 1);
			luaL_argcheck(L, 1<=u, 1, "interval is empty");
			lua_pushnumber(L, floor(r*u)+1);  // int between 1 and `u'
			break;
		}
		case 2: {  // lower and upper limits
			int l = luaL_checkint(L, 1);
			int u = luaL_checkint(L, 2);
			luaL_argcheck(L, l<=u, 2, "interval is empty");
			lua_pushnumber(L, floor(r*(u-l+1))+l);  // int between `l' and `u'
			break;
		}
		default: return luaL_error(L, "wrong number of arguments");
	}
	return 1;
}

// same as math.randomseed, but uses SFMT instead of C srand()
// FIXME: this function doesn't care multi-instance,
//        original math.randomseed either though (Lua 5.1)
static int sfmt_randomseed (lua_State *L) {
	init_gen_rand(luaL_checkint(L, 1));
	return 0;
}

// the following bit operations are ported from LuaBitOp 1.0.1,
// because it can handle the sign bit (bit 31) correctly.

/*
** Lua BitOp -- a bit operations library for Lua 5.1.
** http://bitop.luajit.org/
**
** Copyright (C) 2008-2009 Mike Pall. All rights reserved.
**
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
** [ MIT license: http://www.opensource.org/licenses/mit-license.php ]
*/

#ifdef _MSC_VER
/* MSVC is stuck in the last century and doesn't have C99's stdint.h. */
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

typedef int32_t SBits;
typedef uint32_t UBits;

typedef union {
  lua_Number n;
#ifdef LUA_NUMBER_DOUBLE
  uint64_t b;
#else
  UBits b;
#endif
} BitNum;

/* Convert argument to bit type. */
static UBits barg(lua_State *L, int idx)
{
  BitNum bn;
  UBits b;
  bn.n = lua_tonumber(L, idx);
#if defined(LUA_NUMBER_DOUBLE)
  bn.n += 6755399441055744.0;  /* 2^52+2^51 */
#ifdef SWAPPED_DOUBLE
  b = (UBits)(bn.b >> 32);
#else
  b = (UBits)bn.b;
#endif
#elif defined(LUA_NUMBER_INT) || defined(LUA_NUMBER_LONG) || \
      defined(LUA_NUMBER_LONGLONG) || defined(LUA_NUMBER_LONG_LONG) || \
      defined(LUA_NUMBER_LLONG)
  if (sizeof(UBits) == sizeof(lua_Number))
    b = bn.b;
  else
    b = (UBits)(SBits)bn.n;
#elif defined(LUA_NUMBER_FLOAT)
#error "A 'float' lua_Number type is incompatible with this library"
#else
#error "Unknown number type, check LUA_NUMBER_* in luaconf.h"
#endif
  if (b == 0 && !lua_isnumber(L, idx))
    luaL_typerror(L, idx, "number");
  return b;
}

/* Return bit type. */
#define BRET(b)  lua_pushnumber(L, (lua_Number)(SBits)(b)); return 1;

static int bit_tobit(lua_State *L) { BRET(barg(L, 1)) }
static int bit_bnot(lua_State *L) { BRET(~barg(L, 1)) }

#define BIT_OP(func, opr) \
  static int func(lua_State *L) { int i; UBits b = barg(L, 1); \
    for (i = lua_gettop(L); i > 1; i--) b opr barg(L, i); BRET(b) }
BIT_OP(bit_band, &=)
BIT_OP(bit_bor, |=)
BIT_OP(bit_bxor, ^=)

#define bshl(b, n)  (b << n)
#define bshr(b, n)  (b >> n)
#define bsar(b, n)  ((SBits)b >> n)
#define brol(b, n)  ((b << n) | (b >> (32-n)))
#define bror(b, n)  ((b << (32-n)) | (b >> n))
#define BIT_SH(func, fn) \
  static int func(lua_State *L) { \
    UBits b = barg(L, 1); UBits n = barg(L, 2) & 31; BRET(fn(b, n)) }
BIT_SH(bit_lshift, bshl)
BIT_SH(bit_rshift, bshr)
BIT_SH(bit_arshift, bsar)
BIT_SH(bit_rol, brol)
BIT_SH(bit_ror, bror)

static int bit_bswap(lua_State *L)
{
  UBits b = barg(L, 1);
  b = (b >> 24) | ((b >> 8) & 0xff00) | ((b & 0xff00) << 8) | (b << 24);
  BRET(b)
}

static int bit_tohex(lua_State *L)
{
  UBits b = barg(L, 1);
  SBits n = lua_isnone(L, 2) ? 8 : (SBits)barg(L, 2);
  const char *hexdigits = "0123456789abcdef";
  char buf[8];
  int i;
  if (n < 0) { n = -n; hexdigits = "0123456789ABCDEF"; }
  if (n > 8) n = 8;
  for (i = (int)n; --i >= 0; ) { buf[i] = hexdigits[b & 15]; b >>= 4; }
  lua_pushlstring(L, buf, (size_t)n);
  return 1;
}

static const struct luaL_Reg bit_funcs[] = {
  { "tobit",	bit_tobit },
  { "bnot",	bit_bnot },
  { "band",	bit_band },
  { "bor",	bit_bor },
  { "bxor",	bit_bxor },
  { "lshift",	bit_lshift },
  { "rshift",	bit_rshift },
  { "arshift",	bit_arshift },
  { "rol",	bit_rol },
  { "ror",	bit_ror },
  { "bswap",	bit_bswap },
  { "tohex",	bit_tohex },
  { NULL, NULL }
};

/* Signed right-shifts are implementation-defined per C89/C99.
** But the de facto standard are arithmetic right-shifts on two's
** complement CPUs. This behaviour is required here, so test for it.
*/
#define BAD_SAR		(bsar(-8, 2) != (SBits)-2)

bool luabitop_validate(lua_State *L) // originally named as luaopen_bit
{
  UBits b;
  lua_pushnumber(L, (lua_Number)1437217655L);
  b = barg(L, -1);
  if (b != (UBits)1437217655L || BAD_SAR) {  /* Perform a simple self-test. */
    const char *msg = "compiled with incompatible luaconf.h";
#ifdef LUA_NUMBER_DOUBLE
#ifdef _WIN32
    if (b == (UBits)1610612736L)
      msg = "use D3DCREATE_FPU_PRESERVE with DirectX";
#endif
    if (b == (UBits)1127743488L)
      msg = "not compiled with SWAPPED_DOUBLE";
#endif
    if (BAD_SAR)
      msg = "arithmetic right-shift broken";
    luaL_error(L, "bit library self-test failed (%s)", msg);
    return false;
  }
  return true;
}

// LuaBitOp ends here

static int bit_bshift_emulua(lua_State *L)
{
	int shift = luaL_checkinteger(L,2);
	if (shift < 0) {
		lua_pushinteger(L, -shift);
		lua_replace(L, 2);
		return bit_lshift(L);
	}
	else
		return bit_rshift(L);
}

static int bitbit(lua_State *L)
{
	int rv = 0;
	int numArgs = lua_gettop(L);
	for(int i = 1; i <= numArgs; i++) {
		int where = luaL_checkinteger(L,i);
		if (where >= 0 && where < 32)
			rv |= (1 << where);
	}
	lua_settop(L,0);
	BRET(rv);
}


// The function called periodically to ensure Lua doesn't run amok.
static void S9xLuaHookFunction(lua_State *L, lua_Debug *dbg) {
	
	if (numTries-- == 0) {

		int kill = 0;

#ifdef WIN32
		// Uh oh
		int ret = MessageBox(GUI.hWnd, "The Lua script running has been running a long time. It may have gone crazy. Kill it?\n\n(No = don't check anymore either)", "Lua Script Gone Nuts?", MB_YESNO);
		
		if (ret == IDYES) {
			kill = 1;
		}

#else
		fprintf(stderr, "The Lua script running has been running a long time.\nIt may have gone crazy. Kill it? (I won't ask again if you say No)\n");
		char buffer[64];
		while (true) {
			fprintf(stderr, "(y/n): ");
			fgets(buffer, sizeof(buffer), stdin);
			if (buffer[0] == 'y' || buffer[0] == 'Y') {
				kill = 1;
				break;
			}
			
			if (buffer[0] == 'n' || buffer[0] == 'N')
				break;
		}
#endif

		if (kill) {
			luaL_error(L, "Killed by user request.");
			S9xLuaOnStop();
		}

		// else, kill the debug hook.
		lua_sethook(L, NULL, 0, 0);
	}
	

}

void printfToOutput(const char* fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	int len = vscprintf(fmt, list);
	char* str = new char[len+1];
	vsprintf(str, fmt, list);
	va_end(list);
	if(info_print)
	{
		lua_State* L = LUA /*info.L*/;
		int uid = info_uid /*luaStateToUIDMap[L]*/;
		info_print(uid, str);
		info_print(uid, "\r\n");
		//worry(L,300);
	}
	else
	{
		fprintf(stdout, "%s\n", str);
	}
	delete[] str;
}

bool FailVerifyAtFrameBoundary(lua_State* L, const char* funcName, int unstartedSeverity=2, int inframeSeverity=2)
{
	if (Settings.StopEmulation)
	{
		static const char* msg = "cannot call %s() when emulation has not started.";
		switch(unstartedSeverity)
		{
		case 0: break;
		case 1: printfToOutput(msg, funcName); break;
		default: case 2: luaL_error(L, msg, funcName); break;
		}
		return true;
	}
	if(IPPU.InMainLoop)
	{
		static const char* msg = "cannot call %s() inside an emulation frame.";
		switch(inframeSeverity)
		{
		case 0: break;
		case 1: printfToOutput(msg, funcName); break;
		default: case 2: luaL_error(L, msg, funcName); break;
		}
		return true;
	}
	return false;
}


static const struct luaL_reg snes9xlib [] = {

	{"speedmode", snes9x_speedmode},
	{"frameadvance", snes9x_frameadvance},
	{"pause", snes9x_pause},
	{"framecount", snes9x_framecount},
	{"lagcount", snes9x_lagcount},
	{"lagged", snes9x_lagged},
	{"emulating", snes9x_emulating},
	{"atframeboundary", snes9x_atframeboundary},
	{"registerbefore", snes9x_registerbefore},
	{"registerafter", snes9x_registerafter},
	{"registerstart", snes9x_registerstart},
	{"registerexit", snes9x_registerexit},
	{"message", snes9x_message},
	{"print", print}, // sure, why not
	{NULL,NULL}
};

static const struct luaL_reg memorylib [] = {

	{"readbyte", memory_readbyte},
	{"readbytesigned", memory_readbytesigned},
	{"readword", memory_readword},
	{"readwordsigned", memory_readwordsigned},
	{"readdword", memory_readdword},
	{"readdwordsigned", memory_readdwordsigned},
	{"readbyterange", memory_readbyterange},
	{"writebyte", memory_writebyte},
	{"writeword", memory_writeword},
	{"writedword", memory_writedword},
	{"getregister", memory_getregister},
	{"setregister", memory_setregister},
	// alternate naming scheme for word and double-word and unsigned
	{"readbyteunsigned", memory_readbyte},
	{"readwordunsigned", memory_readword},
	{"readdwordunsigned", memory_readdword},
	{"readshort", memory_readword},
	{"readshortunsigned", memory_readword},
	{"readshortsigned", memory_readwordsigned},
	{"readlong", memory_readdword},
	{"readlongunsigned", memory_readdword},
	{"readlongsigned", memory_readdwordsigned},
	{"writeshort", memory_writeword},
	{"writelong", memory_writedword},

	// memory hooks
	{"registerwrite", memory_registerwrite},
	{"registerread", memory_registerread},
	{"registerexec", memory_registerexec},
	// alternate names
	{"register", memory_registerwrite},
	{"registerrun", memory_registerexec},
	{"registerexecute", memory_registerexec},

	{NULL,NULL}
};

static const struct luaL_reg apulib [] = {

	{"readbyte", apu_readbyte},
	{"readbytesigned", apu_readbytesigned},
	{"readword", apu_readword},
	{"readwordsigned", apu_readwordsigned},
	{"readdword", apu_readdword},
	{"readdwordsigned", apu_readdwordsigned},
	{"readbyterange", apu_readbyterange},
	{"writebyte", apu_writebyte},
	{"writeword", apu_writeword},
	{"writedword", apu_writedword},
	{"writespc", apu_writespc},

	{NULL,NULL}
};

static const struct luaL_reg joypadlib[] = {
	{"get", joypad_get},
	{"getdown", joypad_getdown},
	{"getup", joypad_getup},
	{"set", joypad_set},
	{"gettype", joypad_gettype},
	{"settype", joypad_settype},
	// alternative names
	{"read", joypad_get},
	{"write", joypad_set},
	{"readdown", joypad_getdown},
	{"readup", joypad_getup},
	{NULL,NULL}
};

static const struct luaL_reg savestatelib[] = {
	{"create", savestate_create},
	{"save", savestate_save},
	{"load", savestate_load},
#ifdef SNAPSHOT_VERIFY_SUPPORTED
	{"verify", savestate_verify},
#endif

	{"registersave", savestate_registersave},
	{"registerload", savestate_registerload},
	{"loadscriptdata", savestate_loadscriptdata},

	{NULL,NULL}
};

static const struct luaL_reg movielib[] = {

	{"active", movie_isactive},
	{"recording", movie_isrecording},
	{"playing", movie_isplaying},
	{"mode", movie_getmode},

	{"length", movie_getlength},
	{"name", movie_getname},
	{"rerecordcount", movie_rerecordcount},
	{"setrerecordcount", movie_setrerecordcount},

	{"rerecordcounting", movie_rerecordcounting},
	{"readonly", movie_getreadonly},
	{"setreadonly", movie_setreadonly},
	{"framecount", snes9x_framecount}, // for those familiar with other emulators that have movie.framecount() instead of emulatorname.framecount()

	{"stop", movie_stop},

	// alternative names
	{"close", movie_stop},
	{"getname", movie_getname},
	{"getreadonly", movie_getreadonly},
	{NULL,NULL}
};


static const struct luaL_reg guilib[] = {
	{"register", gui_register},
	{"text", gui_text},
	{"box", gui_drawbox},
	{"line", gui_drawline},
	{"pixel", gui_drawpixel},
	{"opacity", gui_setopacity},
	{"transparency", gui_transparency},
	{"popup", gui_popup},
	{"parsecolor", gui_parsecolor},
	{"gdscreenshot", gui_gdscreenshot},
	{"gdoverlay", gui_gdoverlay},
	{"getpixel", gui_getpixel},
	{"clearuncommitted", gui_clearuncommitted},
	// alternative names
	{"drawtext", gui_text},
	{"drawbox", gui_drawbox},
	{"drawline", gui_drawline},
	{"drawpixel", gui_drawpixel},
	{"setpixel", gui_drawpixel},
	{"writepixel", gui_drawpixel},
	{"rect", gui_drawbox},
	{"drawrect", gui_drawbox},
	{"drawimage", gui_gdoverlay},
	{"image", gui_gdoverlay},
	{"readpixel", gui_getpixel},
	{NULL,NULL}
};

static const struct luaL_reg inputlib[] = {
	{"get", input_getcurrentinputstatus},
	{"popup", input_popup},
	// alternative names
	{"read", input_getcurrentinputstatus},
	{NULL, NULL}
};

void CallExitFunction() {
	if (!LUA)
		return;

	lua_settop(LUA, 0);
	lua_getfield(LUA, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_BEFOREEXIT]);

	int errorcode = 0;
	if (lua_isfunction(LUA, -1))
	{
		chdir(luaCWD);
		errorcode = lua_pcall(LUA, 0, 0, 0);
		_getcwd(luaCWD, _MAX_PATH);
	}

	if (errorcode)
		HandleCallbackError(LUA);
}

void S9xLuaFrameBoundary() {

//	printf("Lua Frame\n");

	ClearCommandTransforms();

	// HA!
	if (!LUA || !luaRunning)
		return;

	// Our function needs calling
	lua_settop(LUA,0);
	lua_getfield(LUA, LUA_REGISTRYINDEX, frameAdvanceThread);
	lua_State *thread = lua_tothread(LUA,1);	

	// Lua calling C must know that we're busy inside a frame boundary
	frameBoundary = TRUE;
	frameAdvanceWaiting = FALSE;

	numTries = 1000;
	chdir(luaCWD);
	int result = lua_resume(thread, 0);
	_getcwd(luaCWD, _MAX_PATH);
	
	if (result == LUA_YIELD) {
		// Okay, we're fine with that.
		//LuaUpdateJoypads();
	} else if (result != 0) {
		// Done execution by bad causes
		S9xLuaOnStop();
		lua_pushnil(LUA);
		lua_setfield(LUA, LUA_REGISTRYINDEX, frameAdvanceThread);
		lua_pushnil(LUA);
		lua_setfield(LUA, LUA_REGISTRYINDEX, guiCallbackTable);

		// Error?
#ifdef WIN32
		MessageBox( GUI.hWnd, lua_tostring(thread,-1), "Lua run error", MB_OK | MB_ICONSTOP);
#else
		fprintf(stderr, "Lua thread bombed out: %s\n", lua_tostring(thread,-1));
#endif
	} else {
		S9xLuaOnStop();
		printf("Script died of natural causes.\n");
	}

	// Past here, the snes actually runs, so any Lua code is called mid-frame. We must
	// not do anything too stupid, so let ourselves know.
	frameBoundary = FALSE;

	if (!frameAdvanceWaiting) {
		S9xLuaOnStop();
	}

}

/**
 * Loads and runs the given Lua script.
 * The emulator MUST be paused for this function to be
 * called. Otherwise, all frame boundary assumptions go out the window.
 *
 * Returns true on success, false on failure.
 */
int S9xLoadLuaCode(const char *filename) {
	static bool sfmtInitialized = false;
	if (!sfmtInitialized) {
		init_gen_rand((unsigned) time(NULL));
		sfmtInitialized = true;
	}

	strncpy(luaScriptName, filename, _MAX_PATH);
	luaScriptName[_MAX_PATH-1] = '\0';

	//stop any lua we might already have had running
	S9xLuaStop();

	// Set current directory from filename (for dofile)
	char dir[_MAX_PATH];
	char *slash, *backslash;
	strcpy(dir, filename);
	slash = strrchr(dir, '/');
	backslash = strrchr(dir, '\\');
	if (!slash || (backslash && backslash < slash))
		slash = backslash;
	if (slash) {
		slash[1] = '\0';    // keep slash itself for some reasons
		_chdir(dir);
	}
	_getcwd(luaCWD, _MAX_PATH);

	if (!LUA) {
		LUA = lua_open();
		luaL_openlibs(LUA);

		luaL_register(LUA, "emu", snes9xlib); // added for better cross-emulator compatibility
		luaL_register(LUA, "snes9x", snes9xlib); // kept for backward compatibility
		luaL_register(LUA, "memory", memorylib);
		luaL_register(LUA, "apu", apulib);
		luaL_register(LUA, "joypad", joypadlib);
		luaL_register(LUA, "savestate", savestatelib);
		luaL_register(LUA, "movie", movielib);
		luaL_register(LUA, "gui", guilib);
		luaL_register(LUA, "input", inputlib);
		luaL_register(LUA, "bit", bit_funcs); // LuaBitOp library
		lua_settop(LUA, 0); // clean the stack, because each call to luaL_register leaves a table on top

		// register a few utility functions outside of libraries (in the global namespace)
		lua_register(LUA, "print", print);
		lua_register(LUA, "tostring", tostring);
		lua_register(LUA, "addressof", addressof);
		lua_register(LUA, "copytable", copytable);

		// old bit operation functions
		lua_register(LUA, "AND", bit_band);
		lua_register(LUA, "OR", bit_bor);
		lua_register(LUA, "XOR", bit_bxor);
		lua_register(LUA, "SHIFT", bit_bshift_emulua);
		lua_register(LUA, "BIT", bitbit);

		luabitop_validate(LUA);

		lua_pushstring(LUA, "math");
		lua_gettable(LUA, LUA_GLOBALSINDEX);
		lua_pushcfunction(LUA, sfmt_random);
		lua_setfield(LUA, -2, "random");
		lua_pushcfunction(LUA, sfmt_randomseed);
		lua_setfield(LUA, -2, "randomseed");
		lua_settop(LUA, 0);

		// push arrays for storing hook functions in
		for(int i = 0; i < LUAMEMHOOK_COUNT; i++)
		{
			lua_newtable(LUA);
			lua_setfield(LUA, LUA_REGISTRYINDEX, luaMemHookTypeStrings[i]);
		}

		// deferred evaluation table
		lua_newtable(LUA);
		lua_setfield(LUA, LUA_REGISTRYINDEX, deferredJoySetIDString);
	}

	// We make our thread NOW because we want it at the bottom of the stack.
	// If all goes wrong, we let the garbage collector remove it.
	lua_State *thread = lua_newthread(LUA);
	
	// Load the data	
	int result = luaL_loadfile(LUA,filename);

	if (result) {
#ifdef WIN32
		MessageBox( GUI.hWnd, lua_tostring(LUA,-1), "Lua load error", MB_OK | MB_ICONSTOP);
#else
		fprintf(stderr, "Failed to compile file: %s\n", lua_tostring(LUA,-1));
#endif

		// Wipe the stack. Our thread
		lua_settop(LUA,0);
		return 0; // Oh shit.
	}

	
	// Get our function into it
	lua_xmove(LUA, thread, 1);
	
	// Save the thread to the registry. This is why I make the thread FIRST.
	lua_setfield(LUA, LUA_REGISTRYINDEX, frameAdvanceThread);
	

	// Initialize settings
	luaRunning = TRUE;
	skipRerecords = FALSE;
	numMemHooks = 0;
	transparencyModifier = 255; // opaque
	ClearCommandTransforms();

	//wasPaused = Settings.Paused;
	//Settings.Paused = FALSE;
	speedmode = SPEED_NORMAL;

	// Set up our protection hook to be executed once every 10,000 bytecode instructions.
	lua_sethook(thread, S9xLuaHookFunction, LUA_MASKCOUNT, 10000);

#ifdef _WIN32
	info_print = PrintToWindowConsole;
	info_onstart = WinLuaOnStart;
	info_onstop = WinLuaOnStop;
	if(!LuaConsoleHWnd)
		LuaConsoleHWnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_LUA), GUI.hWnd, (DLGPROC) DlgLuaScriptDialog);
	info_uid = (int)LuaConsoleHWnd;
#else
	info_print = NULL;
	info_onstart = NULL;
	info_onstop = NULL;
#endif
	if (info_onstart)
		info_onstart(info_uid);

	// And run it right now. :)
	S9xLuaFrameBoundary();
	S9xReRefresh();

	// We're done.
	return 1;
}

/**
 * Equivalent to repeating the last S9xLoadLuaCode() call.
 */
int S9xReloadLuaCode()
{
	if (!*luaScriptName) {
		S9xSetInfoString("There's no script to reload.");
		return 0;
	}
	else
		return S9xLoadLuaCode(luaScriptName);
}

/**
 * Terminates a running Lua script by killing the whole Lua engine.
 *
 * Always safe to call, except from within a lua call itself (duh).
 *
 */
void S9xLuaStop() {

	//already killed
	if (!LUA) return;

	//execute the user's shutdown callbacks
	CallExitFunction();

	/*info.*/numMemHooks = 0;
	for(int i = 0; i < LUAMEMHOOK_COUNT; i++)
		CalculateMemHookRegions((LuaMemHookType)i);

	//sometimes iup uninitializes com
	//MBG TODO - test whether this is really necessary. i dont think it is
	#ifdef WIN32
	CoInitialize(0);
	#endif

	//lua_gc(LUA,LUA_GCCOLLECT,0);

	if (info_onstop)
		info_onstop(info_uid);

	lua_close(LUA); // this invokes our garbage collectors for us
	LUA = NULL;
	S9xLuaOnStop();
}

/**
 * Returns true if there is a Lua script running.
 *
 */
int S9xLuaRunning() {
	// FIXME: return false when no callback functions are registered.
	return (int) (LUA != NULL); // should return true if callback functions are active.
}


/**
 * If this function returns true, the movie code should NOT increment
 * the rerecord count for a load-state.
 *
 * This function will not return true if a script is not running.
 */
bool8 S9xLuaRerecordCountSkip() {
	// FIXME: return true if (there are any active callback functions && skipRerecords)
	return LUA && luaRunning && skipRerecords;
}


/**
 * Given a 16 bit screen with the indicated resolution,
 * draw the current GUI onto it.
 *
 * Currently we only support 256x* resolutions.
 */
//static inline int dibPitchOf(int width, int bpp) { return (((width*bpp+31)/8)&~3); }
void S9xLuaGui(void *s, int width, int height, int bpp, int pitch) {

	if (!LUA/* || !luaRunning*/)
		return;

	// First, check if we're being called by anybody
	lua_getfield(LUA, LUA_REGISTRYINDEX, guiCallbackTable);
	
	if (lua_isfunction(LUA, -1)) {
		// We call it now
		numTries = 1000;
		int ret = lua_pcall(LUA, 0, 0, 0);
		if (ret != 0) {
			// This is grounds for trashing the function
			// Note: This must be done before the messagebox pops up,
			//       otherwise the messagebox will cause a paint event which causes a weird
			//       infinite call sequence that makes Snes9x silently exit with error code 3,
			//       if a Lua GUI function crashes. (nitsuja)
			lua_pushnil(LUA);
			lua_setfield(LUA, LUA_REGISTRYINDEX, guiCallbackTable);

#ifdef WIN32
			MessageBox(GUI.hWnd, lua_tostring(LUA, -1), "Lua Error in GUI function", MB_OK);
#else
			fprintf(stderr, "Lua error in gui.register function: %s\n", lua_tostring(LUA, -1));
#endif
		}
	}

	// And wreak the stack
	lua_settop(LUA, 0);

	if (!gui_used || !gui_enabled)
		return;

	gui_used = FALSE;

	if (width != 256 && width != 512) {
		assert(width == 256 || width == 512);
		return;
	}

	int x, y, x2, y2;
	int xscale = (width == 512) ? 2 : 1;
	int yscale = (height >= 448) ? 2 : 1;

	switch(bpp)
	{
	case 16:
	 {
		uint16 *screen = (uint16*) s;
		int ppl = pitch/2;
		for (y=0; y < height && y < LUA_SCREEN_HEIGHT; y++) {
			for (x=0; x < LUA_SCREEN_WIDTH; x++) {
				const uint8 gui_alpha = gui_data[(y*LUA_SCREEN_WIDTH+x)*4+3];
				if (gui_alpha == 0) {
					// do nothing
					continue;
				}

				const uint8 gui_red   = gui_data[(y*LUA_SCREEN_WIDTH+x)*4+2];
				const uint8 gui_green = gui_data[(y*LUA_SCREEN_WIDTH+x)*4+1];
				const uint8 gui_blue  = gui_data[(y*LUA_SCREEN_WIDTH+x)*4];
				int red, green, blue;

				for (x2 = 0; x2 < xscale; x2++) {
					if (gui_alpha == 255) {
						// direct copy
						red = gui_red;
						green = gui_green;
						blue = gui_blue;
					}
					else {
						// alpha-blending
						const uint8 scr_red   = ((screen[y*ppl + (x*xscale+x2)] >> 11) & 31) << 3;
						const uint8 scr_green = ((screen[y*ppl + (x*xscale+x2)] >> 5)  & 63) << 2;
						const uint8 scr_blue  = ( screen[y*ppl + (x*xscale+x2)]        & 31) << 3;
						red   = (((int) gui_red   - scr_red)   * gui_alpha / 255 + scr_red)   & 255;
						green = (((int) gui_green - scr_green) * gui_alpha / 255 + scr_green) & 255;
						blue  = (((int) gui_blue  - scr_blue)  * gui_alpha / 255 + scr_blue)  & 255;
					}
					for (y2 = 0; y2 < yscale; y2++) {
						screen[(y*yscale+y2)*ppl + (x*xscale+x2)] =  ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
					}
				}
			}
		}
		break;
	 }
	case 24:
	 {
		#define bytesPerPixel   3
		uint8 *screen = (uint8*) s;
		for (y=0; y < height && y < LUA_SCREEN_HEIGHT; y++) {
			for (x=0; x < LUA_SCREEN_WIDTH; x++) {
				const uint8 gui_alpha = gui_data[(y*LUA_SCREEN_WIDTH+x)*4+3];
				if (gui_alpha == 0) {
					// do nothing
					continue;
				}

				const uint8 gui_red   = gui_data[(y*LUA_SCREEN_WIDTH+x)*4+2];
				const uint8 gui_green = gui_data[(y*LUA_SCREEN_WIDTH+x)*4+1];
				const uint8 gui_blue  = gui_data[(y*LUA_SCREEN_WIDTH+x)*4];
				int red, green, blue;

				for (x2 = 0; x2 < xscale; x2++) {
					if (gui_alpha == 255) {
						// direct copy
						red = gui_red;
						green = gui_green;
						blue = gui_blue;
					}
					else {
						// alpha-blending
						const uint8 scr_red   = screen[y*pitch + (x*xscale+x2)*bytesPerPixel + 2];
						const uint8 scr_green = screen[y*pitch + (x*xscale+x2)*bytesPerPixel + 1];
						const uint8 scr_blue  = screen[y*pitch + (x*xscale+x2)*bytesPerPixel];
						red   = (((int) gui_red   - scr_red)   * gui_alpha / 255 + scr_red)   & 255;
						green = (((int) gui_green - scr_green) * gui_alpha / 255 + scr_green) & 255;
						blue  = (((int) gui_blue  - scr_blue)  * gui_alpha / 255 + scr_blue)  & 255;
					}
					for (y2 = 0; y2 < yscale; y2++) {
						screen[(y*yscale+y2)*pitch + (x*xscale+x2)*bytesPerPixel] = blue;
						screen[(y*yscale+y2)*pitch + (x*xscale+x2)*bytesPerPixel + 1] = green;
						screen[(y*yscale+y2)*pitch + (x*xscale+x2)*bytesPerPixel + 2] = red;
					}
				}
			}
		}
		#undef bytesPerPixel
		break;
	 }
	case 32:
	 {
		#define bytesPerPixel   4
		uint8 *screen = (uint8*) s;
		for (y=0; y < height && y < LUA_SCREEN_HEIGHT; y++) {
			for (x=0; x < LUA_SCREEN_WIDTH; x++) {
				const uint8 gui_alpha = gui_data[(y*LUA_SCREEN_WIDTH+x)*4+3];
				if (gui_alpha == 0) {
					// do nothing
					continue;
				}

				const uint8 gui_red   = gui_data[(y*LUA_SCREEN_WIDTH+x)*4+2];
				const uint8 gui_green = gui_data[(y*LUA_SCREEN_WIDTH+x)*4+1];
				const uint8 gui_blue  = gui_data[(y*LUA_SCREEN_WIDTH+x)*4];
				int red, green, blue;

				for (x2 = 0; x2 < xscale; x2++) {
					if (gui_alpha == 255) {
						// direct copy
						red = gui_red;
						green = gui_green;
						blue = gui_blue;
					}
					else {
						// alpha-blending
						const uint8 scr_red   = screen[y*pitch + (x*xscale+x2)*bytesPerPixel + 2];
						const uint8 scr_green = screen[y*pitch + (x*xscale+x2)*bytesPerPixel + 1];
						const uint8 scr_blue  = screen[y*pitch + (x*xscale+x2)*bytesPerPixel];
						red   = (((int) gui_red   - scr_red)   * gui_alpha / 255 + scr_red)   & 255;
						green = (((int) gui_green - scr_green) * gui_alpha / 255 + scr_green) & 255;
						blue  = (((int) gui_blue  - scr_blue)  * gui_alpha / 255 + scr_blue)  & 255;
					}
					for (y2 = 0; y2 < yscale; y2++) {
						screen[(y*yscale+y2)*pitch + (x*xscale+x2)*bytesPerPixel] = blue;
						screen[(y*yscale+y2)*pitch + (x*xscale+x2)*bytesPerPixel + 1] = green;
						screen[(y*yscale+y2)*pitch + (x*xscale+x2)*bytesPerPixel + 2] = red;
					}
				}
			}
		}
		#undef bytesPerPixel
		break;
	 }
	default:
		assert(false /* unsupported color-depth */);
	}
	return;
}

void S9xLuaClearGui() {
	gui_used = false;
}
void S9xLuaEnableGui(bool enabled) {
	gui_enabled = enabled;
}


lua_State* S9xGetLuaState() {
	return LUA;
}
char* S9xGetLuaScriptName() {
	return luaScriptName;
}
