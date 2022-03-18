/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    d100dbg.h

This driver runs on the following hardware:
    - 82557/82558 based PCI 10/100Mb ethernet adapters
    (aka Intel EtherExpress(TM) PRO Adapters)

Environment:
    Kernel Mode - Or whatever is the equivalent on WinNT

Revision History
    - JCB 8/14/97 Example Driver Created
*****************************************************************************/

#ifndef _D100DBG_H
#define _D100DBG_H


VOID
NTAPI
DbgBreakPoint(
    VOID
    );

//-------------------------------------------------------------------------
// enable TRACEing
//-------------------------------------------------------------------------
#if DBG
#define   TRACE      1
#endif

//-------------------------------------------------------------------------
// Enable error message printing invoked when functions return a bad Status
//-------------------------------------------------------------------------
#if DBG
#define   ERR_TRACE  1
#endif


//-------------------------------------------------------------------------
// DEBUG enable bit definitions
//-------------------------------------------------------------------------
#define DBG_DISPLAY 0x0001      // Display DEBUGSTR messages
#define DBG_TRACE   0x0002      // Display TRACESTR messages
#define DBG_TRACE_PACKETS   0x0004  // Display packet dst/src addresses
#define DBG_TRACE_XFER  0x0008
#define DBG_LEVEL1  0x0010      // Display TRACE1 messages
#define DBG_LEVEL2  0x0020      // Display TRACE2 messages
#define DBG_LEVEL3  0x0040      // Display TRACE3 messages
#define DBG_THRESH  0x0080      // Display Threshold changes
#define DBG_REQUEST 0x0200      // Display Request messages
#define DBG_SILENT  0x0400
#define DBG_STATS   0x0800      // Display Stats updates
#define DBG_LEVELS  (DBG_LEVEL1|DBG_LEVEL2|DBG_LEVEL3)
#define DBG_NORMAL  (DBG_LEVEL1|DBG_REQUEST)
#define DBG_MEDIUM  (DBG_LEVEL1|DBG_TRACE|DBG_DISPLAY|DBG_REQUEST|DBG_STATS)
#define DBG_VERBOSE (DBG_LEVELS|DBG_TRACE|DBG_DISPLAY)


//-------------------------------------------------------------------------
// Definitions for all of the Debug macros.  If we're in a debug (DBG) mode,
// these macros will print information to the debug terminal.  If the
// driver is compiled in a free (non-debug) environment the macros become
// NOPs.
//-------------------------------------------------------------------------
#if DBG

#define DebugFlag(_A)   ((_A)->Debug)

#define DEBUGFUNC(__F)         static const char __FUNC__[] = __F;;

#define PFUNC(A)    DbgPrint("%s: ",__FUNC__)
//#define PFUNC(A)    DbgPrint("%s:%s: ",(A)->DbgNamePtr,__FUNC__)
#define TRACEXFER(A,S)  {if (DebugFlag((A)) & DBG_TRACE_XFER) {PFUNC((A));DbgPrint S;}}
#define TRACE1(A,S) {if (DebugFlag((A)) & DBG_LEVEL1) {PFUNC((A));DbgPrint S;}}
#define TRACE2(A,S) {if (DebugFlag((A)) & DBG_LEVEL2) {PFUNC((A));DbgPrint S;}}
#define TRACE3(A,S) {if (DebugFlag((A)) & DBG_LEVEL3) {PFUNC((A));DbgPrint S;}}
#define THRESH(A,S) {if (DebugFlag((A)) & DBG_THRESH) {PFUNC((A));DbgPrint S;}}
#define REQUEST(A,S)    {if (DebugFlag((A)) & DBG_REQUEST) {PFUNC((A));DbgPrint S;}}
#define STATS(A,S)    {if (DebugFlag((A)) & DBG_STATS) {PFUNC((A));DbgPrint S;}}
#define TRACEPKT(A,S)   {if (DebugFlag((A)) & DBG_TRACE_PACKETS) {PFUNC((A));DbgPrint S;}}
#define TRACEPOINT(A)   {if (DebugFlag((A)) & DBG_TRACE)  {DbgPrint("%s:%d\n",__FILE__,__LINE__);}}
#define TRACESTR(A,S)   {PFUNC((A));DbgPrint("%s:%d - ", __FILE__, __LINE__);DbgPrint S;}
#define DEBUGSTR(S) {DbgPrint("%s:%d - ", __FILE__, __LINE__);DbgPrint S;}
#define INITSTR(S)  {DbgPrint("%s: ", __FUNC__);DbgPrint S;}
#define INITSTRTX(A,S) {if (A) {DbgPrint("%s: ", __FUNC__);DbgPrint S;}}
#define TRACEPOWER(S)  {DbgPrint("%s: ", __FUNC__);DbgPrint S;}

#define DEBUGCHAR(A,C) {if (DebugFlag((A)) & DBG_SILENT) {                                                           \
                                                             (A)->DbgQueue[((A)->DbgIndex)]=C;                       \
                                                             (A)->DbgIndex = (((A)->DbgIndex+1) & DBG_QUEUE_LEN);    \
                                                             (A)->DbgQueue[((A)->DbgIndex)]='~';                     \
                                                          }}


#undef ASSERT
#define ASSERT( exp ) if (!(exp)) { DbgPrint("Assertion Failed: %s:%d %s\n",__FILE__,__LINE__,#exp); DbgBreakPoint(); }

#define DBGINT  DbgBreakPoint()

#else // !DBG

#define DebugFlag(A)    0

#if HDBG

#undef ASSERT
#define ASSERT( exp ) if (!(exp)) { DbgPrint("Assertion Failed: %s:%d %s\n",__FILE__,__LINE__,#exp); DbgBreakPoint(); }

#define TRACESTR(A,S)   {DbgPrint("%s:%d - ", __FILE__, __LINE__);DbgPrint S;}
#define DEBUGSTR(S) {DbgPrint("%s:%d - ", __FILE__, __LINE__);DbgPrint S;}
#define INITSTR(S)  {DbgPrint("%s:%d ",__FILE__,__LINE__);DbgPrint S;}
#define INITSTRTX(A,S) {if (A) {DbgPrint("%s:%d ",__FILE__,__LINE__);DbgPrint S;}}
#else
#define TRACESTR(A,S)
#define DEBUGSTR(S)
#define INITSTR(S)
#define INITSTRTX(A,S)
#define TRACEPOWER(S)
#endif

#define TRACEXFER(A,S)   ;
#define DEBUGFUNC(F);
#define TRACE1(A,S)      ;
#define TRACE2(A,S)      ;
#define TRACE3(A,S)      ;
#define THRESH(A,S)      ;
#define REQUEST(A,S)     ;
#define STATS(A,S)       ;
#define TRACEPKT(A,S)    ;
#define TRACEPOINT(A)    ;
#define DEBUGCHAR(A,C)     ;
#define UNLOCKEDDEBUGCHAR(A,C)      ;

#define DBGINT           ;
#endif

#if !DBG
#if !HDBG
#if !NDIS_NT
#undef ASSERT
#define ASSERT( exp )
#endif
#endif
#endif

#endif      // D100DBG_H

