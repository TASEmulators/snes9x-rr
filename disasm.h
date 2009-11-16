// simple but powerful trace logger from bsnes

#ifndef SNES9X_DISASM_H_INCLUDED
#define SNES9X_DISASM_H_INCLUDED

#include <stdio.h>

extern FILE *S9xTraceLogStream;

START_EXTERN_C // for ASMs
extern void S9xTraceCPUToBuf(char *output);
extern void S9xTraceCPU();
END_EXTERN_C

#endif // !SNES9X_DISASM_H_INCLUDED
