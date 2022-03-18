//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1998 - 1999
//
//  File:       debug.h
//
//--------------------------------------------------------------------------

//
// Debug Defines and Macros
//   

// #define DDPnP1(_x_) DbgPrint _x_
#define DDPnP1(_x_)

//
// Defines used with PptDump* macros below.
//
#define PARDUMP_SILENT                ((ULONG)0x00000000)
#define PARCANCEL                     ((ULONG)0x00000001) // display message when IRP cancelled
#define PARUNLOAD                     ((ULONG)0x00000002) // display message when driver unloaded
#define PARINITDEV                    ((ULONG)0x00000004)
#define PARIRPPATH                    ((ULONG)0x00000008)
#define PARIOCTL                      ((ULONG)0x00000010) // display IOCTL related messages
#define PARPUSHER                     ((ULONG)0x00000020)
#define PARERRORS                     ((ULONG)0x00000040) // display error related messages
#define PARTHREAD                     ((ULONG)0x00000080)

#define PAREXIT                       ((ULONG)0x00000200)
#define PARENTRY                      ((ULONG)0x00000400)
#define PARENTRY_EXIT                 (PARENTRY | PAREXIT)
#define PARINFO                       ((ULONG)0x00000800)

#define PARNECR98                     ((ULONG)0x00001000)
#define PARRESOURCE                   ((ULONG)0x00002000)

#define PARDUMP_PORT_ALLOC_FREE       ((ULONG)0x00004000)
#define PARDUMP_DOT3_SELECT_DESELECT  ((ULONG)0x00008000)

#define PARPNP1                       ((ULONG)0x00010000)

#define PARLGZIP                      ((ULONG)0x00100000) // dump info regarding Legacy Zip drive

#define PARDUMP_VERBOSE_MAX           ((ULONG)0x80000000)   

// use with PptBreakOn
#define PAR_BREAK_ON_NOTHING          ((ULONG)0x00000000)
#define PAR_BREAK_ON_DRIVER_ENTRY     ((ULONG)0x00000001)
#define PAR_BREAK_ON_ADD_DEVICE       ((ULONG)0x00000002)

#if DBG
#define DVRH_HACKS   1

#if DVRH_HACKS
    #define PptDump(LEVEL,STRING) \
                if (PptDebugLevel & LEVEL) DbgPrint STRING
#else
    #define PptDump(LEVEL,STRING) \
            do { \
                if (PptDebugLevel & LEVEL) { \
                    DbgPrint STRING; \
                } \
            } while (0)
#endif // DVRH_HACKS

#if DVRH_HACKS
    #define PptDump2(LEVEL,STRING) \
                if (PptDebugLevel & LEVEL) { \
                    DbgPrint("PARPORT: "); \
                    DbgPrint STRING; \
                }
#else
    #define PptDump2(LEVEL,STRING) \
            do { \
                if (PptDebugLevel & LEVEL) { \
                    DbgPrint("PARPORT: "); \
                    DbgPrint STRING; \
                } \
            } while (0)
#endif // DVRH_HACKS


//
// display if we want PnP info
// 
#if DVRH_HACKS
    #define PptDumpP(STRING) \
                if (PptDebugLevel & PARPNP1) { \
                    DbgPrint("PARPORT: PnP: "); \
                    DbgPrint STRING; \
                }
#else
    #define PptDumpP(STRING) \
            do { \
                if (PptDebugLevel & PARPNP1) { \
                    DbgPrint("PARPORT: PnP: "); \
                    DbgPrint STRING; \
                } \
            } while (0)
#endif // DVRH_HACKS


//
// display only if we want all debug messages
//
#if DVRH_HACKS
    #define PptDumpV(STRING) \
                if (PptDebugLevel & PARDUMP_VERBOSE_MAX) { \
                    DbgPrint("PARPORT: "); \
                    DbgPrint STRING; \
                }
#else
    #define PptDumpV(STRING) \
            do { \
                if (PptDebugLevel & PARDUMP_VERBOSE_MAX) { \
                    DbgPrint("PARPORT:V: "); \
                    DbgPrint STRING; \
                } \
            } while (0)
#endif // DVRH_HACKS

#if DVRH_HACKS
    #define PptBreak(BREAK_CONDITION,STRING) \
                if ( (PptBreakOn & BREAK_CONDITION)) { \
                    DbgPrint("PARPORT: Break: "); \
                    DbgPrint STRING; \
                    DbgBreakPoint(); \
                }
#else
    #define PptBreak(BREAK_CONDITION,STRING) \
            do { \
                ULONG _breakCondition = (BREAK_CONDITION); \
                if ( (PptBreakOn & _breakCondition)) { \
                    DbgPrint("PARPORT: Break: "); \
                    DbgPrint STRING; \
                    DbgBreakPoint(); \
                } \
            } while (0)
#endif // DVRH_HACKS

#else // !DBG
    #if DVRH_HACKS
        #define PptDump(LEVEL,STRING)  //lint !e760
        #define PptDump2(LEVEL,STRING) //lint !e760
        #define PptDumpP(STRING)       //lint !e760
        #define PptDumpV(STRING)       //lint !e760
        #define PptBreak(LEVEL,STRING) //lint !e760
    #else
        #define PptDump(LEVEL,STRING)  do {NOTHING;} while (0)
        #define PptDump2(LEVEL,STRING) do {NOTHING;} while (0)
        #define PptDumpP(STRING)       do {NOTHING;} while (0)
        #define PptDumpV(STRING)       do {NOTHING;} while (0)
        #define PptBreak(LEVEL,STRING) do {NOTHING;} while (0)
    #endif
#endif // DBG

// 
// Specific Diagnostics
// 

//
// Diagnostics for Enabling Generic EPP detection only if we can
//   screen any printer with a 1284.3 daisy chain device or
//   legacy Zip. Also do the detection if explicitly requested
//   in the port devnode: RequestEppTest : REG_DWORD : 0x1
//
// #define PARDD01(_x_) DbgPrint _x_
#define PARDD01(_x_) 


