// Lazy fix for Visual Studio 2015
// https://tenshil.blogspot.com/2017/06/visualstudio-2015-error-lnk2019.html

#include <stdio.h>

FILE _iob[] = {*stdin, *stdout, *stderr};

extern "C" FILE * __cdecl __iob_func(void)
{
    return _iob;
}
