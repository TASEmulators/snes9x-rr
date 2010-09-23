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


  Win32 GUI code
  (c) Copyright 2003-2004 blip, Nach, Matthew Kendora and funkyass

   
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



#ifndef CONFIG_H
#define CONFIG_H

#include <set>
#include <vector>
#include <string>

#include "port.h"
#include "reader.h"

class ConfigFile {
  public:
    ConfigFile(void);

    void Clear(void);

    // return false on failure
    bool LoadFile(const char *filename);
    void LoadFile(Reader *r, const char *name=NULL);

    // return false if key does not exist or is empty
    bool Exists(const char *key);

    // return the value / default
    std::string GetString(const char *key, std::string def);
    char *GetString(const char *key, char *out, uint32 outlen); // return NULL if it doesn't exist, out not affected
    const char *GetString(const char *key, const char *def=NULL); // NOTE: returned pointer becomes invalid when key is deleted/modified, or the ConfigFile is Clear()ed or deleted.
    char *GetStringDup(const char *key, const char *def=NULL); // Much like "strdup(GetString(key, def))"
    int32 GetInt(const char *key, int32 def=-1, bool *bad=NULL);
    uint32 GetUInt(const char *key, uint32 def=0, int base=0, bool *bad=NULL); // base = 0, 8, 10, or 16
    bool GetBool(const char *key, bool def=false, bool *bad=NULL);
    const char* GetComment(const char *key); // NOTE: returned pointer becomes invalid when key is deleted/modified, or the ConfigFile is Clear()ed or deleted.

    // return true if the key existed prior to setting
    bool SetString(const char *key, std::string val, const char *comment="");
    bool SetInt(const char *key, int32 val, const char *comment="");
    bool SetUInt(const char *key, uint32 val, int base=10, const char *comment=""); // base = 8, 10, or 16
    bool SetBool(const char *key, bool val, const char *true_val="TRUE", const char *false_val="FALSE", const char *comment="");
    bool DeleteKey(const char *key);

    // Operation on entire sections
    bool DeleteSection(const char *section);
    typedef std::vector<std::pair<std::string,std::string> > secvec_t;
    secvec_t GetSection(const char *section);
    int GetSectionSize(const char *section);

	// Clears all key-value pairs that didn't receive a Set command, or a Get command with autoAdd on
    void ClearUnused(void);

	// Clears all stored line numbers
    void ClearLines(void);

    bool SaveTo(const char *filename);

    static void SetDefaultAutoAdd(bool autoAdd);
    static void SetNiceAlignment(bool align);
    static void SetShowComments(bool show);
    static void SetAlphaSort(bool sort);
    static void SetTimeSort(bool sort);

  private:
    std::string Get(const char *key);
    bool Has(const char *key);

    class ConfigEntry {
      protected:
        int line;
        std::string section;
        std::string key;
        std::string val;
        std::string comment;
		mutable bool used;

        struct section_then_key_less {
            bool operator()(const ConfigEntry &a, const ConfigEntry &b);
        };

        struct key_less {
            bool operator()(const ConfigEntry &a, const ConfigEntry &b) const{
                if(a.section!=b.section) return a.section<b.section;
                return a.key<b.key;
            }
        };

        struct line_less {
            bool operator()(const ConfigEntry &a, const ConfigEntry &b){
                if(a.line==b.line){
                    if(b.val.empty() == a.val.empty()) return a.key<b.key;
                    if(a.val.empty()) return false;
                    if(b.val.empty()) return true;
                }
                if(b.line<0) return true;
                if(a.line<0) return false;
                return a.line<b.line;
            }
        };

        static void trim(std::string &s){
            int i;
            i=s.find_first_not_of(" \f\n\r\t\v");
            if(i==-1){
                s.clear();
                return;
            }
            if(i>0) s.erase(0, i); // erase leading whitespace
            i=s.find_last_not_of(" \f\n\r\t\v");
            if(i!=-1) s.erase(i+1); // erase trailing whitespace
			return;
        }

		// trims comments and leading/trailing whitespace from s, and returns any trimmed comments
		// make sure not to call this more than once on the same string
        static std::string trimCommented(std::string &s){
			std::string cmt;
            int i;
            i=s.find_first_not_of(" \f\n\r\t\v");
            if(i==-1){
                s.clear();
                return cmt;
            }
            if(i>0) s.erase(0, i); // erase leading whitespace
			int off=0;
			for(;;){
				i=s.find('#',off); // find trailing comment
				if(i>=0)
					if((int)s.length()>i+1 && s.at(i+1) == '#') {
						s.erase(i,1);  // ignore ## and change to #
						off = i+1;
						continue;
					} else {
						int j=s.find_first_not_of(" \f\n\r\t\v",i+1);
						if(j!=-1) cmt = s.substr(j); // store
						s.erase(i); // erase trailing comment
					}
				break;
			}
            i=s.find_last_not_of(" \f\n\r\t\v");
            if(i!=-1) s.erase(i+1); // erase trailing whitespace
			return cmt;
        }

      public:
        ConfigEntry(int l, const std::string &s, const std::string &k, const std::string &v) :
            line(l), section(s), key(k), val(v) {
            trim(section);
            trim(key);
			used=false;
        }

        void parse_key(const std::string &k){
            int i=k.find("::");
            if(i==-1){
                section="Uncategorized"; key=k;
            } else {
                section=k.substr(0,i); key=k.substr(i+2);
            }
            trim(section);
            trim(key);
			used=false;
        }

        ConfigEntry(const std::string k){
            parse_key(k);
        }

        ConfigEntry(const std::string k, const std::string &v) : line(-1), val(v) {
            parse_key(k);
        }

        friend class ConfigFile;
        friend struct key_less;
        friend struct line_less;
    };
    std::set<ConfigEntry, ConfigEntry::key_less> data;
	int linectr;
	static bool defaultAutoAdd;
	static bool niceAlignment;
	static bool showComments;
	static bool alphaSort;
	static bool timeSort;
};

/* Config file format:
 *
 * Comments are any lines whose first non-whitespace character is ';' or '#'.
 * Note that comments can also follow a value, on the same line.
 * To intentionally have a '#' character in the value, use ##
 *
 * All parameters fall into sections. To name a section, the first
 * non-whitespace character on the line will be '[', and the last will be ']'.
 *
 * Parameters are simple key=value pairs. Whitespace around the '=', and at the
 * beginning or end of the line is ignored. Key names may not contain '=' nor
 * begin with '[', however values can. If the last character of the value is
 * '\', the next line (sans leading/trailing whitespace) is considered part of
 * the value as well. Programmatically, the key "K" in section "S" is referred
 * to as "S::K", much like C++ namespaces. For example:
 *   [Section1]
 *   # this is a comment
 *   foo = bar \
 *      baz\
 *      quux \
 *   ## this is not a comment! # this IS a comment
 * means the value of "Section1::foo" is "bar bazquux # this is not a comment!"
 *
 * Parameters may be of several types:
 *  String - Bare characters. If the first and last characters are both '"',
 *           they are removed (so just double them if you really want quotes
 *           there)
 *  Int - A decimal number from -2147483648 to 2147483647
 *  UInt - A number in decimal, hex, or octal from 0 to 4294967295 (or
 *         0xffffffff, or 037777777777)
 *  Bool - true/false, 0/1, on/off, yes/no
 *
 * Of course, the actual accepted values for a parameter may be further
 * restricted ;)
 */


/* You must write this for your port */
void S9xParsePortConfig(ConfigFile &, int pass);

/* This may or may not be useful to you */
char *S9xParseDisplayConfig(ConfigFile &, int pass);

#endif
