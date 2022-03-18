/******************************Module*Header*******************************\
* Module Name: precomp.h
*
* Common headers used throughout the display driver.  This entire include
* file will typically be pre-compiled.
*
* Copyright (c) 1993-1995 Microsoft Corporation
\**************************************************************************/

    #if defined(_X86_)
        #define FASTCALL _fastcall
    #else
        #define FASTCALL
    #endif


    #include <stddef.h>
    #include <stdarg.h>
    #include <limits.h>
    #include <windef.h>

#if _WIN32_WINNT >= 0x0500
    #include <d3d.h>
#endif

    #include "s3common.h"

    #include <winerror.h>
    #include <wingdi.h>
    #include <winddi.h>
    #include <mmsystem.h>
    #include <devioctl.h>

    #include <ioaccess.h>
    #include <math.h>



    #include <mcdrv.h>              // for MCD
    #include <gl\gl.h>              // for MCD

    #include "lines.h"
#if _WIN32_WINNT >= 0x0500
    #include "dx95type.h"
    #include "lpb.h"
    #include "virge1.h"
#endif
    #include "mpc.h"
    #include "driver.h"
    #include "hw.h"
    #include "debug.h"
#if _WIN32_WINNT < 0x0500
    #include "basetsd.h"             // for compiling 64-bit Merced codes.
#endif

