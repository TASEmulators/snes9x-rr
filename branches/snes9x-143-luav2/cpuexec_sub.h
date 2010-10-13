// common subroutines for both ASM/C core

#include "port.h"

START_EXTERN_C
extern bool8 Inside_Frame;

void OnFrameStart();
void OnFrameEnd();
END_EXTERN_C
