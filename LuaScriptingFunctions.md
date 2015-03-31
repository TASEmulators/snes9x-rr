In future, these things should go to [EmuLua Wiki](http://wiki.emulua.com/wiki/)

See also [Lua manual](http://www.lua.org/manual/5.1/).

**Important: nil is not 0. nil and 0 are two different entities.**

# Global #

### print ###

Prints any value or values, mainly to help you debug your script. Unlike the default implementation, this can even print the contents of tables. Also, the printed values will go to the script's output window instead of stdout. Note that if you want to print a memory address you should use print(string.format("0x%X",address)) instead of print(address).

### tostring ###

Returns a string that represents the argument. You can use this if you want to get the same string that print would print, but use it for some purpose other than immediate printing. This function is actually what gives print its ability to print tables and other non-string values. Note that there is currently a limit of 65536 characters per result, after which only a "..." is appended, but in typical use you shouldn't ever run into this limit.

### addressof ###

Returns the pointer address of a reference-type value. In particular, this can be used on tables and functions to see what their addresses are. There's not much worth doing with a pointer address besides printing it to look at it and see that it's different from the address of something else. Please do not store the address to use for hashing or logical comparison, that is completely unnecessary in Lua because you can simply use the actual object instead of its address for those purposes. If the argument is not a reference type then this function will return 0.

### copytable ###

Returns a shallow copy of the given table. In other words, it gives you a different table that contains all of the same values as the original. This is unlike simple assignment of a table, which only copies a reference to the original table. You could write a Lua function that does what this function does, but it's such a common operation that it seems worth having a pre-defined function available to do it.

### AND,OR,XOR,SHIFT,BIT ###

Old bit operation functions, use [bit.\*](http://bitop.luajit.org/) functions instead.

# emu #

### emu.speedmode(string mode) ###

Changes the speed of emulation depending on mode. If "normal", emulator runs at normal speed. If "nothrottle", emulator runs at max speed without frameskip. If "turbo", emulator drops some frames. If "maximum", screen rendering is disabled.

### emu.frameadvance() ###

Pauses script until a frame is emulated. Cannot be called by a coroutine or registered function.

### emu.pause() ###

Pauses emulator when the current frame has finished emulating.

### int emu.framecount() ###

Returns the frame count for the movie, or the number of frames from last reset otherwise.

### int emu.lagcount() ###

Returns the lag count.

### boolean emu.lagged() ###

Returns true if the last frame was a lag frame, false otherwise.

### boolean emu.emulating() ###

Returns true if emulation has started, or false otherwise.

### emu.registerbefore(function func) ###

Registers a callback function to run immediately before each frame gets emulated. This runs after the next frame's input is known but before it's used, so this is your only chance to set the next frame's input using the next frame's would-be input. For example, if you want to make a script that filters or modifies ongoing user input, such as making the game think "left" is pressed whenever you press "right", you can do it easily with this.

Note that this is not quite the same as code that's placed before a call to emu.frameadvance. This callback runs a little later than that. Also, you cannot safely assume that this will only be called once per frame. Depending on the emulator's options, every frame may be simulated multiple times and your callback will be called once per simulation. If for some reason you need to use this callback to keep track of a stateful linear progression of things across frames then you may need to key your calculations to the results of emu.framecount.

Like other callback-registering functions provided by Snes9x, there is only one registered callback at a time per registering function per script. If you register two callbacks, the second one will replace the first, and the call to emu.registerbefore will return the old callback. You may register nil instead of a function to clear a previously-registered callback. If a script returns while it still has registered callbacks, Snes9x will keep it alive to call those callbacks when appropriate, until either the script is stopped by the user or all of the callbacks are de-registered.

### emu.registerafter(function func) ###

Registers a callback function to run immediately after each frame gets emulated. It runs at a similar time as (and slightly before) gui.register callbacks, except unlike with gui.register it doesn't also get called again whenever the screen gets redrawn. Similar caveats as those mentioned in emu.registerbefore apply.

### emu.registerexit(function func) ###

Registers a callback function that runs when the script stops. Whether the script stops on its own or the user tells it to stop, or even if the script crashes or the user tries to close the emulator, Snes9x will try to run whatever Lua code you put in here first. So if you want to make sure some code runs that cleans up some external resources or saves your progress to a file or just says some last words, you could put it here. (Of course, a forceful termination of the application or a crash from inside the registered exit function will still prevent the code from running.)

Suppose you write a script that registers an exit function and then enters an infinite loop. If the user clicks "Stop" your script will be forcefully stopped, but then it will start running its exit function. If your exit function enters an infinite loop too, then the user will have to click "Stop" a second time to really stop your script. That would be annoying. So try to avoid doing too much inside the exit function.

Note that restarting a script counts as stopping it and then starting it again, so doing so (either by clicking "Restart" or by editing the script while it is running) will trigger the callback. Note also that returning from a script generally does NOT count as stopping (because your script is still running or waiting to run its callback functions and thus does not stop... see here for more information), even if the exit callback is the only one you have registered.

### emu.message(string msg) ###

Displays the message on the screen.

# memory #

### int memory.readbyte(int addr) ###
### int memory.readbytesigned(int addr) ###
### int memory.readword(int addr) ###
### int memory.readwordsigned(int addr) ###
### int memory.readdword(int addr) ###
### int memory.readdwordsigned(int addr) ###

Reads value from memory address. RAM addresses are 0x7e0000-0x7fffff. Word=2 bytes, Dword=4 bytes.

Note: Do not access to chip registers, or you might experience a desync.

### string memory.readbyterange(int startaddr, int length) ###

Returns a chunk of memory from the given address with the given length as a string. To access, use _string.byte(str,offset)_.

### memory.writebyte(int addr) ###
### memory.writeword(int addr) ###
### memory.writedword(int addr) ###

Writes value to memory address. RAM addresses are 0x7e0000-0x7fffff.

### memory.getregister (cpuregistername) ###

Returns the current value of the given hardware register.
For example, memory.getregister("pc") will return the main CPU's current Program Counter.

Valid registers are: "db", "p", "e", "a", "d", "s", "x", "y", "pb", "pc", and "pbpc".

You can prefix the string with "sa1." to retrieve registers from SA1 instead of the 65C816, or you can explicitly use "main." or "65c816." if you want. For example, memory.getregister("sa1.a") will return the value stored in the SA1's A Register.

### memory.setregister (cpuregistername, value) ###

Sets the current value of the given hardware register.
For example, memory.setregister("pc",0x200) will change the main CPU's current Program Counter to 0x200.

Valid registers are: "db", "p", "e", "a", "d", "s", "x", "y", "pb", "pc", and "pbpc".

You can prefix the string with "sa1." to set registers on SA1 instead of the 65C816, or you can explicitly use "main." or "65c816." if you want.

You had better know exactly what you're doing or you're probably just going to crash the game if you try to use this function. That applies to the other memory.write functions as well, but to a lesser extent.

### `memory.registerwrite (address, [size,] [cpuname,] func)` ###

Calls the function whenever the given address is written to. Function can be nil.

### `memory.registerread (address, [size,] [cpuname,] func)` ###

Calls the function whenever the given address is read. Function can be nil.

### `memory.registerexec (address, [size,] [cpuname,] func)` ###

Calls the function whenever the given address is executed. Function can be nil.

# apu #

It's not very useful for TASing, but it might be useful for automatic SPC dumping.

### apu.readbyte, apu.readbytesigned, apu.readword, apu.readwordsigned, apu.readdword, apu.readdwordsigned, apu.readbyterange, apu.writebyte, apu.writeword, apu.writedword ###

Read/Write value from/to APU RAM. Besides that, most of the information about the same functions in the memory library applies to these functions as well.

### `apu.writespc(filename [, autosearch = false])` ###

Dumps SPC to file.

# joypad #

Before the next frame is emulated, one may set keys to be pressed. The buffer is cleared each frame.

### table joypad.get(int port) ###

Returns a table of every game button, where each entry is true if that button is currently held (as of the last time the emulation checked), or false if it is not held. If a movie is playing, this will read the input actually being received from the movie instead of what the user is pressing. By default this only checks controller 1's input, but the optional whichcontroller argument lets you choose.

Keys for joypad table: (R, L, X, A, right, left, down, up, start, select, Y, B). Keys are case-sensitive.

### table joypad.getdown(int port) ###

Returns a table of only the game buttons that are currently held. Each entry is true if that button is currently held (as of the last time the emulation checked), or nil if it is not held. If a movie is playing, this will read the input actually being received from the movie instead of what the user is pressing. By default this only checks controller 1's input, but the optional whichcontroller argument lets you choose.

Keys for joypad table: (R, L, X, A, right, left, down, up, start, select, Y, B). Keys are case-sensitive.

### table joypad.getup(int port) ###

Returns a table of only the game buttons that are not currently held. Each entry is nil if that button is currently held (as of the last time the emulation checked), or false if it is not held. If a movie is playing, this will read the input actually being received from the movie instead of what the user is pressing. By default this only checks controller 1's input, but the optional whichcontroller argument lets you choose.

Keys for joypad table: (R, L, X, A, right, left, down, up, start, select, Y, B). Keys are case-sensitive.

### joypad.set(int port, table buttons) ###

Sets the buttons to be pressed next frame. true for pressed, nil or false for not pressed.

# savestate #

### object savestate.create(int slot=nil) ###

Creates a savestate object. If any argument is given, it must be from 1 to 12, and it corresponds with the numbered savestate slots. If no argument is given, the savestate can only be accessed by Lua.

### savestate.save(object savestate) ###

Saves the current state to the savestate object.

### savestate.load(object savestate) ###

Loads the state of the savestate object as the current state.

### function savestate.registersave(function func) ###

Registers a function to be called when a state is saved. Function can be nil. The previous function is returned, possibly nil. The function is allowed to return values.

### function savestate.registerload(function func) ###

Registers a function to be called when a state is loaded. Function can be nil. The previous function is returned, possibly nil. The function is passed parameters, if any, returned from a function registered from registersave.

### savestate.loadscriptdata(location) ###

Returns the data associated with the given savestate (data that was earlier returned by a registered save callback) without actually loading the rest of that savestate or calling any callbacks. location should be a save slot number.

# movie #

### boolean movie.active () ###

Returns true if any movie file is open, or false otherwise.

### boolean movie.recording () ###

Returns true if a movie file is currently recording, or false otherwise.

### boolean movie.playing () ###

Returns true if a movie file is currently playing, or false otherwise.

### string movie.mode() ###

Returns "record" if movie is recording, "playback" if movie is replaying input, or nil if there is no movie.

### int movie.length() ###

Returns the total number of frames in the current movie.

### string movie.name() ###

Returns a string containing the full filename (path) of the current movie file.

### int movie.rerecordcount () ###

Returns the count of re-records that is stored in the current movie file.

### movie.setrerecordcount (number) ###

Sets the re-record count of the current movie file to the given number.

### movie.rerecordcounting(boolean skipcounting) ###

If set to true, no rerecords done by Lua are counted in the rerecord total. If set to false, rerecords done by Lua count. By default, rerecords count.

### movie.stop() ###

Stops the movie. Cannot be used if there is no movie.

# gui #

All functions assume that the height of the image is 256 and the width is 239.

Color can be given as "0xrrggbbaa" or as a name (e.g. "red").

### function gui.register(function func) ###

Registers a function to be called when the screen is updated. Function can be nil. The previous function is returned, possibly nil.

All drawing process should be done in the callback of this function.

### `gui.pixel(int x, int y [, type color])` ###

Draws a pixel at (x,y) with the given color.

### `gui.line(int x1, int y1, int x2, int y2 [, type color [, skipfirst]])` ###

Draws a line from (x1,y1) to (x2,y2) with the given color.

### `gui.box(int x1, int y1, int x2, int y2 [, type fillcolor [, outlinecolor]])` ###

Draws a box with (x1,y1) and (x2,y2) as opposite corners with the given color.

### string gui.gdscreenshot() ###

Takes a screenshot and returns it as a string that can be used by the [gd library](http://lua-gd.luaforge.net/).

For direct access, use _string.byte(str,offset)_. The gd image consists of a 11-byte header and each pixel is alpha,red,green,blue (1 byte each, alpha is 0 in this case) left to right then top to bottom.

### gui.opacity(float alpha) ###

Sets the opacity of drawings depending on alpha. 0.0 is invisible, 1.0 is drawn over. Values less than 0.0 or greater than 1.0 work by extrapolation.

### gui.transparency(float strength) ###

4.0 is invisible, 0.0 is drawn over. Values less than 0.0 or greater than 4.0 work by extrapolation.

### gui.text(int x, int y, string msg) ###

Draws the given text at (x,y). Not to be confused with _snes9x.message(string msg)_.

### gui.getpixel (x, y) ###

Returns the RGB color at the given onscreen pixel location. You can say local r,g,b = gui.getpixel(x,y). r,g,b are the red/green/blue color components of that pixel, each ranging from 0 to 255. If the coordinate you give is offscreen, you will receive the color values of the nearest onscreen pixel instead.

Note that this function can return colors that have already been written to the screen by GUI drawing functions. If for some reason you want to make sure that you only get the clean untampered-with colors the emulation drew onscreen, then you'll have to call this function before any GUI drawing functions have written to the screen for the current frame. Probably the most reliable way to do that is to call gui.getpixel inside of a callback function that you register with emu.registerafter.

### gui.parsecolor (color) ###

Returns the separate RGBA components of the given color.

For example, you can say local r,g,b,a = gui.parsecolor('orange') to retrieve the red/green/blue values of the preset color orange. (You could also omit the a in cases like this.) This uses the same conversion method that Gens uses internally to support the different representations of colors that the GUI library uses. Overriding this function will not change how Gens interprets color values, however.

### `gui.gdoverlay([int dx=0, int dy=0,] string str [, sx=0, sy=0, sw, sh] [, float alphamul=1.0])` ###

Overlays the given gd image with top-left corner at (dx,dy) and given opacity.

### `string gui.popup (msg [, type [, icon]])` ###

Brings up a modal popup dialog box (everything stops until the user dismisses it). The box displays the message tostring(msg). This function returns the name of the button the user clicked on (as a string).

type determines which buttons are on the dialog box, and it can be one of the following: 'ok', 'yesno', 'yesnocancel', 'okcancel', 'abortretryignore'.
type defaults to 'ok' for gui.popup, or to 'yesno' for input.popup.

icon indicates the purpose of the dialog box (or more specifically it dictates which title and icon is displayed in the box), and it can be one of the following: 'message', 'question', 'warning', 'error'.
icon defaults to 'message' for gui.popup, or to 'question' for input.popup.

Try to avoid using this function much if at all, because modal dialog boxes can be irritating.

# input #

### `string input.popup (msg [, type [, icon]])` ###

see gui.popup

### table input.get() ###

Returns a table of which keyboard buttons are pressed as well as mouse status. (note: this is not related to the SNES mouse peripheral, it's simply your PC mouse cursor.) Key values for keyboard buttons and mouse clicks are true for pressed, nil for not pressed. Mouse position is returned in terms of game screen pixel coordinates. Coordinates assume that the game screen is 256 by 224. Keys for mouse are (xmouse, ymouse, leftclick, rightclick, middleclick). Keys for keyboard buttons:
(backspace, tab, enter, shift, control, alt, pause, capslock, escape, space, pageup, pagedown, end, home, left, up, right, down, insert, delete,
0, 1, ..., 9, A, B, ..., Z, numpad0, numpad1, ..., numpad9, numpad`*`, numpad+, numpad-, numpad., numpad/, F1, F2, ..., F24,
numlock, scrolllock, semicolon, plus, comma, minus, period, slash, tilde, leftbracket, backslash, rightbracket, quote)

Keys are case-sensitive. Keys for keyboard buttons are for buttons, **not** ASCII characters, so there is no need to hold down shift. Key names may differ depending on keyboard layout. On US keyboard layouts, "slash" is /?, "tilde" is `````~, "leftbracket" is `[``{`, "backslash" is \|, "rightbracket" is `]``}`, "quote" is '".