/******************************Module*Header*******************************\
* Module Name: debug.c
*
* Debug helper routines.
*
* Copyright (c) 1992-1996 Microsoft Corporation
*
\**************************************************************************/
#include "precomp.h"
#include "hw3D.h"

#ifdef MCD95
#include <stdio.h>

// This critical section is not needed for the MCD95 version
// of the S3ViRGE driver.

#define ENGACQUIRESEMAPHORE(cs)
#define ENGRELEASESEMAPHORE(cs)
#else
#define ENGACQUIRESEMAPHORE(cs) EngAcquireSemaphore((cs))
#define ENGRELEASESEMAPHORE(cs) EngReleaseSemaphore((cs))
#endif

// Routines are being put here temporarily outside of the #if DBG
// and must be moved to another file.

#define LARGE_LOOP_COUNT  10000000 // to be moved down later

#if (DBG || P5TIMER)

////////////////////////////////////////////////////////////////////////////
// DEBUGGING INITIALIZATION CODE
//
// When you're bringing up your display for the first time, you can
// recompile with 'DebugLevel' set to 100.  That will cause absolutely
// all DISPDBG messages to be displayed on the kernel debugger (this
// is known as the "PrintF Approach to Debugging" and is about the only
// viable method for debugging driver initialization code).

LONG DebugLevel = 0;            // Set to '100' to debug initialization code
                                //   (the default is '0')

////////////////////////////////////////////////////////////////////////////

LONG gcFifo = 0;                // Number of currently free FIFO entries

BOOL gbCrtcCriticalSection = FALSE;
                                // Have we acquired the CRTC register
                                //   critical section?

// #define LARGE_LOOP_COUNT  10000000

#define LOG_SIZE_IN_BYTES 4000

typedef struct _LOGGER {
    ULONG ulEnd;
    ULONG ulCurrent;
    CHAR  achBuf[LOG_SIZE_IN_BYTES];
} DBGLOG;

#define GetAddress(dst, src)\
try {\
    dst = (VOID*) lpGetExpressionRoutine(src);\
} except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?\
            EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {\
    lpOutputRoutine("NTSD: Access violation on \"%s\", switch to server context\n", src);\
    return;\
}

DBGLOG glog = {0, 0};           // If you muck with this, fix 'dumplog' too
LONG   LogLevel = 1;

#endif // DBG


/*****************************************************************************
 *
 *   Routine Description:
 *
 *       This function is called as an NTSD extension to dump a LineState
 *
 *   Arguments:
 *
 *       hCurrentProcess - Supplies a handle to the current process (at the
 *           time the extension was called).
 *
 *       hCurrentThread - Supplies a handle to the current thread (at the
 *           time the extension was called).
 *
 *       CurrentPc - Supplies the current pc at the time the extension is
 *           called.
 *
 *       lpExtensionApis - Supplies the address of the functions callable
 *           by this extension.
 *
 *       lpArgumentString - the float to display
 *
 *   Return Value:
 *
 *       None.
 *
 ***************************************************************************/
VOID dumplog(
    HANDLE hCurrentProcess,
    HANDLE hCurrentThread,
    DWORD dwCurrentPc,
    // PNTSD_EXTENSION_APIS lpExtensionApis,
    LPSTR lpExtensionApis,
    LPSTR lpArgumentString)
{

// The '.def' file cannot be conditionally compiled, so 'dumplog' is always
// exported.  As a result, we have to have a stub 'dumplog' function even
// in a free build:

#if 0
#if DBG

    PNTSD_OUTPUT_ROUTINE lpOutputRoutine;
    PNTSD_GET_EXPRESSION lpGetExpressionRoutine;
    PNTSD_GET_SYMBOL     lpGetSymbolRoutine;

    ULONG       cFrom;
    ULONG       cTo;
    ULONG       cCurrent;
    DBGLOG*     plogOriginal;
    DBGLOG*     plog;
    ULONG       ulCurrent;
    ULONG       ulEnd;
    CHAR*       pchEnd;
    CHAR*       pch;

    UNREFERENCED_PARAMETER(hCurrentThread);
    UNREFERENCED_PARAMETER(dwCurrentPc);

    lpOutputRoutine        = lpExtensionApis->lpOutputRoutine;
    lpGetExpressionRoutine = lpExtensionApis->lpGetExpressionRoutine;
    lpGetSymbolRoutine     = lpExtensionApis->lpGetSymbolRoutine;

    lpOutputRoutine("!s3.dumplog [<from#> [<to#>]]\n\n");

    cTo   = 1;              // Defaults
    cFrom = 20;

    pch = strpbrk(lpArgumentString, "0123456789");
    if (pch != NULL)        // Use defaults if no args given
    {
        cFrom = atoi(pch);
        pch = strchr(pch, ' ');
        if (pch != NULL)
        {
            pch = strpbrk(pch, "0123456789");
            if (pch != NULL)
                cTo = atoi(pch);
        }
    }

    // Do some parameter validation, then read the log into the
    // debugger process's address space:

    if (cTo >= cFrom)
        cTo = cFrom;

    if (cTo < 1)
    {
        cTo   = 1;
        cFrom = 1;
    }

    GetAddress(plogOriginal, "glog");

    if (!ReadProcessMemory(hCurrentProcess,
                          (LPVOID) &(plogOriginal->ulCurrent),
                          &ulCurrent,
                          sizeof(ulCurrent),
                          NULL))
        return;

    if (!ReadProcessMemory(hCurrentProcess,
                          (LPVOID) &(plogOriginal->ulEnd),
                          &ulEnd,
                          sizeof(ulEnd),
                          NULL))
        return;

    if (ulCurrent == 0 && ulEnd == 0)
    {
        lpOutputRoutine("Log empty\n\n");
        return;
    }

#ifdef MCD95
    plog = LocalAlloc(LMEM_FIXED, sizeof(DBGLOG));
#else
    plog = EngAllocMem(0, sizeof(DBGLOG), ALLOC_TAG);
#endif

    if (plog == NULL) {
        lpOutputRoutine("Couldn't allocate temporary buffer!\n");
        return;
    }

    if (!ReadProcessMemory(hCurrentProcess,
                          (LPVOID) &(plogOriginal->achBuf[0]),
                          &plog->achBuf[1],
                          LOG_SIZE_IN_BYTES,
                          NULL))
        return;

    // Mark the first byte in the buffer as being a zero, because
    // we're going to search backwards through the buffer for zeroes,
    // and we'll want to stop when we get to the beginning:

    plog->achBuf[0] = 0;
    ulCurrent++;
    ulEnd++;

    // Find the start string by going backwards through the buffer
    // and counting strings until the count becomes equal to 'cFrom':

    cCurrent = 0;
    pch      = &plog->achBuf[ulCurrent - 1];
    pchEnd   = &plog->achBuf[0];

    while (TRUE)
    {
        if (*(--pch) == 0)
        {
            cCurrent++;
            if (--cFrom == 0)
                break;

            if (pch == &plog->achBuf[ulCurrent - 1])
                break;         // We're back to where we started!
        }

        // Make sure we wrap the end of the buffer:

        if (pch <= pchEnd)
        {
            if (ulCurrent >= ulEnd)
                break;

            pch = &plog->achBuf[ulEnd - 1];
        }
    }

    // pch is pointing to zero byte before our start string:

    pch++;

    // Output the strings:

    pchEnd = &plog->achBuf[max(ulEnd, ulCurrent)];

    while (cCurrent >= cTo)
    {
        lpOutputRoutine("-%li: %s", cCurrent, pch);
        pch += strlen(pch) + 1;
        cCurrent--;

        // Make sure we wrap when we get to the end of the buffer:

        if (pch >= pchEnd)
            pch = &plog->achBuf[1];     // First char in buffer is a NULL
    }

    lpOutputRoutine("\n");

#ifdef MCD95
    LocalFree(plog);
#else
    EngFreeMem(plog);
#endif

#endif // DBG
#endif // 0

    return;
}


//
// DebugPrint function is turned on when P5TIMER is defined. This allows us to
// print out timing information in the free build. P5TIMER defined in sources.
//
#if (DBG || P5TIMER)


////////////////////////////////////////////////////////////////////////////
// Miscellaneous Driver Debug Routines
////////////////////////////////////////////////////////////////////////////

/*****************************************************************************
 *
 *   Routine Description:
 *
 *      This function is variable-argument, level-sensitive debug print
 *      routine.
 *      If the specified debug level for the print statement is lower or equal
 *      to the current debug level, the message will be printed.
 *
 *   Arguments:
 *
 *      DebugPrintLevel - Specifies at which debugging level the string should
 *          be printed
 *
 *      DebugMessage - Variable argument ascii c string
 *
 *   Return Value:
 *
 *      None.
 *
 ***************************************************************************/

VOID
DebugPrint(
    LONG  DebugPrintLevel,
    PCHAR DebugMessage,
    ...
    )
{
    va_list ap;

    va_start(ap, DebugMessage);

    if (DebugPrintLevel <= DebugLevel)
    {
#ifdef MCD95
        char buffer[256];

        OutputDebugStringA(STANDARD_DEBUG_PREFIX);
        vsprintf(buffer, DebugMessage, ap);
        OutputDebugStringA(buffer);
        OutputDebugStringA("\n");
#else
        EngDebugPrint(STANDARD_DEBUG_PREFIX, DebugMessage, ap);
        EngDebugPrint("", "\n", ap);
#endif
    }

    va_end(ap);

} // DebugPrint()

#endif

#if DBG
VOID
DPF(
    PCHAR DebugMessage,
    ...
    )
{
    va_list ap;

    va_start(ap, DebugMessage);

    {
#ifdef MCD95
        char buffer[256];

        OutputDebugStringA(STANDARD_DEBUG_PREFIX);
        vsprintf(buffer, DebugMessage, ap);
        OutputDebugStringA(buffer);
        OutputDebugStringA("\n");
#else
        EngDebugPrint(STANDARD_DEBUG_PREFIX, DebugMessage, ap);
        EngDebugPrint("", "\n", ap);
#endif
    }

    va_end(ap);

} // DebugPrint()



/*****************************************************************************
 *
 *   Routine Description:
 *
 *      This function is variable-argument, level-sensitive debug log
 *      routine.
 *      If the specified debug level for the log statement is lower or equal
 *      to the current debug level, the message will be logged.
 *
 *   Arguments:
 *
 *      DebugLogLevel - Specifies at which debugging level the string should
 *          be logged
 *
 *      DebugMessage - Variable argument ascii c string
 *
 *   Return Value:
 *
 *      None.
 *
 ***************************************************************************/

VOID
DebugLog(
    LONG  DebugLogLevel,
    PCHAR DebugMessage,
    ...
    )
{
    va_list ap;

    va_start(ap, DebugMessage);

    if (DebugLogLevel <= LogLevel) {

        char buffer[128];
        int  length = 0;

// !!!        length = vsprintf(buffer, DebugMessage, ap);

        length++;           // Don't forget '\0' terminator!

        // Wrap around to the beginning of the log if not enough room for
        // string:

        if (glog.ulCurrent + length >= LOG_SIZE_IN_BYTES) {
            glog.ulEnd     = glog.ulCurrent;
            glog.ulCurrent = 0;
        }

        memcpy(&glog.achBuf[glog.ulCurrent], buffer, length);
        glog.ulCurrent += length;
    }

    va_end(ap);

} // DebugLog()

/******************************Public*Routine******************************\
* VOID vS3DFifoWait
*
* Low level routine to wait for a specific number of fifo slots
*
*
\**************************************************************************/

VOID vS3DFifoWait(
PDEV* ppdev,
ULONG  slots)

{
    LONG    i;

    ASSERTDD((slots > 0) && (slots <= 16), "Illegal wait level");

    gcFifo = slots;

    for (i = LARGE_LOOP_COUNT; i != 0; i--)
    {
        if (((S3readHIU(ppdev, S3D_SUBSYSTEM_STATUS) >> 8)
            & S3D_FIFO_SLOTSFREE_MASK) >= slots)
            return;

    }

    RIP("vS3DFifoWait timeout -- The hardware is in a funky state.");
}


/******************************Public*Routine******************************\
* BOOL vS3DFifoBusy
*
* Low level routine check specific number of fifo slots
*
*
\**************************************************************************/

BOOL vS3DFifoBusy(
PDEV* ppdev,
ULONG  slots)

{
    LONG    i;

    ASSERTDD((slots > 0) && (slots <= 16), "Illegal wait level");

    return (((S3readHIU(ppdev, S3D_SUBSYSTEM_STATUS) >> 8)
            & S3D_FIFO_SLOTSFREE_MASK) < slots);
}



/******************************Public*Routine******************************\
* BOOL vS3DGPBusy
*
* Low level routine to wait for the graphics processor to become idle
*
*
\**************************************************************************/

BOOL vS3DGPBusy(
PDEV* ppdev)

{
  return( ! (S3readHIU(ppdev, S3D_SUBSYSTEM_STATUS) & S3D_ENGINE_IDLE) );
}

/******************************Public*Routine******************************\
* VOID vS3DGPWait
*
* Low level routine to wait for the S3D graphics processor to become idle
*
*
\**************************************************************************/

VOID vS3DGPWait(
PDEV* ppdev)
{
    LONG    i;

    if (ppdev->ulCaps & CAPS_S3D_ENGINE)
    {

#if M5_DISABLE_POWER_DOWN_CONTROL

        if ( ppdev->ulCaps2 & CAPS2_DISABLE_POWER_DOWN_CTRL )
        {
            ACQUIRE_CRTC_CRITICAL_SECTION( ppdev );

            VGAOUTPW( ppdev, SEQ_INDEX, SEQ_REG_UNLOCK );

            //
            // Turn off Engine Power Down Control, and Extend S3d Engine
            // Busy for better performance
            //

            VGAOUTP( ppdev, SEQ_INDEX, 0x09 );
            VGAOUTP( ppdev, SEQ_DATA, VGAINP( ppdev, SEQ_DATA ) & ~0x02 );
            VGAOUTP( ppdev, SEQ_INDEX, 0x0A );
            VGAOUTP( ppdev, SEQ_DATA, VGAINP( ppdev, SEQ_DATA ) & ~0x40 );
        }

#endif

        for (i = LARGE_LOOP_COUNT; i != 0; i--)
        {
            if (S3readHIU(ppdev, S3D_SUBSYSTEM_STATUS) & S3D_ENGINE_IDLE)
            {
                break;
            }
        }

#if M5_DISABLE_POWER_DOWN_CONTROL

        if ( ppdev->ulCaps2 & CAPS2_DISABLE_POWER_DOWN_CTRL )
        {
            //
            // restore engine power down control, and extend s3d engine busy
            //

            VGAOUTP( ppdev, SEQ_INDEX, 0x0A );
            VGAOUTP( ppdev, SEQ_DATA, VGAINP( ppdev, SEQ_DATA ) | 0x40 );
            VGAOUTP( ppdev, SEQ_INDEX, 0x09 );
            VGAOUTP( ppdev, SEQ_DATA, VGAINP( ppdev, SEQ_DATA ) | 0x02 );

//NOLOCK            VGAOUTPW( ppdev, SEQ_INDEX, 0x0008 );

            RELEASE_CRTC_CRITICAL_SECTION( ppdev );
        }

#endif

        if ( !i )
        {
            RIP("vS3DGP_WAIT timeout -- The hardware is in a funky state.");

#if M5_DISABLE_POWER_DOWN_CONTROL
            if ( ppdev->ulCaps2 & CAPS2_DISABLE_POWER_DOWN_CTRL )
            {
                RELEASE_CRTC_CRITICAL_SECTION( ppdev );
            }
#endif
        }
    }
    else
    {
        RIP("vS3DGP_WAIT - called wrong version.");
    }
}

/******************************Public*Routines*****************************\
* UCHAR  vgaInp()     - INP()
* USHORT vgaInpW()    - INPW()
* VOID   vgaOutp()    - OUTP()
* VOID   vgaOutpW()   - OUTPW()
*
* Debug thunks for general I/O routines.  This is used primarily to verify
* that any code accessing the CRTC register has grabbed the CRTC critical
* section (necessary because with GCAPS_ASYNCMOVE, DrvMovePointer calls
* may happen at any time, and they need to access the CRTC register).
*
\**************************************************************************/

UCHAR vgaInp(PDEV* ppdev, ULONG p)
{
    ASSERTDD((p < 0x1000), "Vga I/O macro used for non-vga address");

    if (((p == CRTC_INDEX) || (p == CRTC_DATA)) &&
        (!gbCrtcCriticalSection))
    {
        RIP("Must have acquired CRTC critical section to access CRTC register");
    }

    CP_EIEIO();

    #if PCIMMIO
        return(READ_REGISTER_UCHAR(ppdev->pjMmBase + (p) + 0x8000));
    #else
        return(READ_PORT_UCHAR(ppdev->pjIoBase + (p)));
    #endif
}

USHORT vgaInpW(PDEV* ppdev, ULONG p)
{
    ASSERTDD((p < 0x1000), "Vga I/O macro used for non-vga address");

    if (((p == CRTC_INDEX) || (p == CRTC_DATA)) &&
        (!gbCrtcCriticalSection))
    {
        RIP("Must have acquired CRTC critical section to access CRTC register");
    }

    CP_EIEIO();
    #if PCIMMIO
        return(READ_REGISTER_USHORT(ppdev->pjMmBase + (p) + 0x8000));
    #else
        return(READ_PORT_USHORT(ppdev->pjIoBase + (p)));
    #endif
}

VOID vgaOutp(PDEV* ppdev, ULONG p, ULONG v)
{
    ASSERTDD((p < 0x1000), "Vga I/O macro used for non-vga address");

    if (((p == CRTC_INDEX) || (p == CRTC_DATA)) &&
        (!gbCrtcCriticalSection))
    {
        RIP("Must have acquired CRTC critical section to access CRTC register");
    }

    CP_EIEIO();
    #if PCIMMIO
        WRITE_REGISTER_UCHAR(ppdev->pjMmBase + (p) + 0x8000, (UCHAR)(v));
    #else
        WRITE_PORT_UCHAR(ppdev->pjIoBase + (p), (UCHAR)(v));
    #endif
    CP_EIEIO();
}

VOID vgaOutpW(PDEV* ppdev, ULONG p, ULONG v)
{
    ASSERTDD((p < 0x1000), "Vga I/O macro used for non-vga address");

    if (((p == CRTC_INDEX) || (p == CRTC_DATA)) &&
        (!gbCrtcCriticalSection))
    {
        RIP("Must have acquired CRTC critical section to access CRTC register");
    }

    CP_EIEIO();
    #if PCIMMIO
        WRITE_REGISTER_USHORT(ppdev->pjMmBase + (p) + 0x8000, (USHORT)(v));
    #else
        WRITE_PORT_USHORT(ppdev->pjIoBase + (p), (v));
    #endif
    CP_EIEIO();
}

/******************************Public*Routine******************************\
* VOID vAcquireCrtc()
* VOID vReleaseCrtc()
*
* Debug thunks for grabbing the CRTC register critical section.
*
\**************************************************************************/

VOID vAcquireCrtc(PDEV* ppdev)
{
    ENGACQUIRESEMAPHORE(ppdev->csCrtc);

    if (gbCrtcCriticalSection)
        RIP("Had already acquired Critical Section");
    gbCrtcCriticalSection = TRUE;
}

VOID vReleaseCrtc(PDEV* ppdev)
{
    // 80x/805i/928 and 928PCI chips have a bug where if I/O registers
    // are left unlocked after accessing them, writes to memory with
    // similar addresses can cause writes to I/O registers.  The problem
    // registers are 0x40, 0x58, 0x59 and 0x5c.  We will simply always
    // leave the index set to an innocuous register (namely, the text
    // mode cursor start scan line):

    VGAOUTP(ppdev, CRTC_INDEX, 0xa);

    if (!gbCrtcCriticalSection)
        RIP("Hadn't yet acquired Critical Section");
    gbCrtcCriticalSection = FALSE;
    ENGRELEASESEMAPHORE(ppdev->csCrtc);
}


#endif // DBG

