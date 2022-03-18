/***************************************************************************\
* Module Name: debug.h
*
* Commonly used debugging macros.
*
* Copyright (c) 1992-1996 Microsoft Corporation
\***************************************************************************/
extern
VOID
DebugPrint(
    LONG DebugPrintLevel,
    PCHAR DebugMessage,
    ...
    );

//
// Enable DebugPrint statement when P5TIMER is defined. This let us
// print out time count, so that section of codes within driver can be
// monitored. Note that there are too much overhead on the checked build,
// to allow for any meaningfull timing analysis. Therefore, usually this
// flag in turned on for the free build. P5TIMER flag can be added in
// sources under USER_C_FLAGS. We are using the Pentium internal 64-bit
// timer for timing collection here. The unit is in clock count.
//
#if P5TIMER

// For printing up the 64-bit time count.
#define HIDWORD(l)          ((DWORD)(((DWORDLONG)(l) >> 32) & 0xFFFFFFFF))

#define LODWORD(l)          ((DWORD)(l))

#define PRINTIME(arg)       DebugPrint arg

//
// Clear Pentium 64-bit timer, using WRMSR instruction.
//
#define CLEARTPC(ppdev) \
    _asm    {   xor     eax, eax            };\
    _asm    {   mov     edx, eax            };\
    _asm    {   mov     ecx, 010h           };\
    _asm    {   _emit   0x0F                };\
    _asm    {   _emit   0x30                };\

//
// Read Pentium 64-bit timer, using RDMSR instruction.
//
#define READTPC(ppdev) \
    _asm    {   mov     ecx, 010h           };\
    _asm    {   _emit   0x0F                };\
    _asm    {   _emit   0x32                };\
    _asm    {   mov     edi, ppdev          };\
    _asm    {   mov     DWORD PTR[OFFSET(PDEV.Timer)][edi], eax};   \
    _asm    {   mov     DWORD PTR[OFFSET(PDEV.Timer+4)][edi], edx}; \

#else

#define DISPTIME(arg)

#define CLEARTPC(ppdev)

#define READTPC(ppdev)

#endif


#if DBG

extern VOID DPF(LPSTR szFormat, ...);

VOID DebugLog(LONG, CHAR*, ...);
// VOID DebugState(LONG);
VOID SetInt3();

#define DISPDBG(arg) DebugPrint arg
#define STATEDBG(level) DebugState(level)
#ifdef MCD95
#define RIP(x) { DebugPrint(0, x); DebugBreak();}
#else
#define RIP(x) { DebugPrint(0, x); EngDebugBreak();}
#endif
#define ASSERTDD(x, y) if (!(x)) RIP (y)

// If we are not in a debug environment, we want all of the debug
// information to be stripped out.

#else
#define DPF 1 ? (void) 0: (void)
#define DISPDBG(arg)
#define STATEDBG(level)
#define LOGDBG(arg)
#define RIP(x)
#define ASSERTDD(x, y)

#endif

