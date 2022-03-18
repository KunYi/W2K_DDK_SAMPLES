/******************************Module*Header*******************************\
* Module Name: driver.h
*
* Contains prototypes for the display driver.
*
* Copyright (c) 1992-1996 Microsoft Corporation
\**************************************************************************/

//////////////////////////////////////////////////////////////////////
// Put all the conditional-compile constants here.  There had better
// not be many!

//  When running on an x86, we can make banked call-backs to GDI where
//  GDI can write directly on the frame buffer.  The Alpha has a weird
//  bus scheme, and can't do that:

#if !defined(_ALPHA_)
    #define GDI_BANKING         1
#else
    #define GDI_BANKING         0
#endif

//  Both fast and slow patterns are enabled by default:

#define FASTFILL_PATTERNS       1
#define SLOWFILL_PATTERNS       1

//  Multi-board support can be enabled by setting this to 1:

#define MULTI_BOARDS            0

//  M65 APM Support enabled by setting to 1
//  Note: in later releases, set to 0):

#define APM_SUPPORT             0


// We support DX6 DDI callbacks (vertex buffer)
#define DX6                     1

//  This is the maximum number of boards we'll support in a single
//  virtual driver:

#if MULTI_BOARDS
    #define MAX_BOARDS          16
    #define IBOARD(ppdev)       ((ppdev)->iBoard)

    //  We disable slow patterns for multi-boards, because I'm too lazy to
    //  change all the realization checks to account for possibly NULL
    //  prb->apbe[] values:

    #undef SLOWFILL_PATTERNS
    #define SLOWFILL_PATTERNS   0

#else
    #define MAX_BOARDS          1
    #define IBOARD(ppdev)       0
#endif

//  Useful for visualizing the 2-d heap:

#define DEBUG_HEAP              0

//-----------------------------------------------------------------------------
// Virge/VirgeVX Software patch information:
//-----------------------------------------------------------------------------
// Virge Patch Define   Hardware Errata Problem Description
// -------------------- -------------------------------------------------------
// VIRGE_PATCH11        Command following rectangle fills or line draws.
// VIRGE_PATCH12        8 QWORD image xfer or screen to screen BitBlt.
// VIRGE_PATCH13        Consecutive rectangle or line commands.
// VIRGE_PATCH14        Consecutive lines in 24Bpp color depth.
// VIRGE_PATCH15        Wrong data in last subspan of BitBlt.
// VIRGE_PATCH16        Dots in rectangles and lines.
// VIRGE_PATCH17        Transparent text corruption at all color depths
//------------------------------------------------------------------------------
//

//
// definition of flags moved to SOURCES file
//

#ifdef  VIRGE_PATCH12 //----------------
#define VIRGE_PATCH12_08BPP_MINWIDTH 57     //  8bpp Min width for patch needed
#define VIRGE_PATCH12_08BPP_MAXWIDTH 64     //  8bpp Max width for patch needed
#define VIRGE_PATCH12_08BPP_BLTWIDTH 65     //  8bpp Blt width for patch
#define VIRGE_PATCH12_16BPP_MINWIDTH 29     // 16bpp Min width for patch needed
#define VIRGE_PATCH12_16BPP_MAXWIDTH 32     // 16bpp Max width for patch needed
#define VIRGE_PATCH12_16BPP_BLTWIDTH 33     // 16bpp Blt width for patch
#define VIRGE_PATCH12_24BPP_MINWIDTH 16     // 24bpp Min width for patch needed
#define VIRGE_PATCH12_24BPP_MAXWIDTH 22     // 24bpp Max width for patch needed
#define VIRGE_PATCH12_24BPP_BLTWIDTH 23     // 24bpp Blt width for patch
#endif // VIRGE_PATCH12 //--------------

#ifdef  VIRGE_PATCH15 //----------------
#define VIRGE_PATCH15_08BPP_MAXWIDTH 16     //  8bpp Max width for patch needed
#define VIRGE_PATCH15_08BPP_BLTWIDTH 17     //  8bpp Blt width for patch
#define VIRGE_PATCH15_16BPP_MAXWIDTH 8      // 16bpp Max width for patch needed
#define VIRGE_PATCH15_16BPP_BLTWIDTH 9      // 16bpp Blt width for patch
#define VIRGE_PATCH15_24BPP_MAXWIDTH 5      // 24bpp Max width for patch needed
#define VIRGE_PATCH15_24BPP_BLTWIDTH 6      // 24bpp Blt width for patch
#endif // VIRGE_PATCH15 //--------------

//////////////////////////////////////////////////////////////////////
// Miscellaneous shared stuff

#define STANDARD_DEBUG_PREFIX   "S3: "  // All debug output is prefixed
                                        // by this string

#define ALLOC_TAG               '  3S'  // Four byte tag used for tracking
                                        // memory allocations (characters
                                        // are in reverse order)

#define CLIP_LIMIT          50  // We'll be taking 800 bytes of stack space

#define DRIVER_EXTRA_SIZE   0   // Size of the DriverExtra information in the
                                // DEVMODE structure

#define TMP_BUFFER_SIZE     8192  // Size in bytes of 'pvTmpBuffer'.  Has to
                                  // be at least enough to store an entire
                                  // scan line (i.e., 6400 for 1600x1200x32).

#if defined(_ALPHA_)
    #define XFER_BUFFERS    16  // Defines the maximum number of write buffers
                                // possible on any Alpha.  Must be a power
                                // of two.
#else
    #define XFER_BUFFERS    1   // On non-alpha systems, we don't have to
                                // worry about the chip caching our bus
#endif                          // writes.

#define MAX_AUTOFLIP        2   // Maximum surfaces supported by hardware autoflipping

#define XFER_MASK           (XFER_BUFFERS - 1)

typedef struct _CLIPENUM {
    LONG    c;
    RECTL   arcl[CLIP_LIMIT];   // Space for enumerating complex clipping

} CLIPENUM;                     /* ce, pce */

typedef struct _PDEV PDEV;      // Handy forward declaration

//////////////////////////////////////////////////////////////////////
// Text stuff

#define GLYPH_CACHE_HEIGHT  48  // Number of scans to allocate for glyph cache,
                                // divided by pel size

#define GLYPH_CACHE_CX      64  // Maximum width of glyphs that we'll consider
                                // caching

#define GLYPH_CACHE_CY      64  // Maximum height of glyphs that we'll consider
                                // caching

#define MAX_GLYPH_SIZE      ((GLYPH_CACHE_CX * GLYPH_CACHE_CY + 31) / 8)
                                // Maximum amount of off-screen memory required
                                // to cache a glyph, in bytes

#define GLYPH_ALLOC_SIZE    8100
                                // Do all cached glyph memory allocations
                                // in 8k chunks

#define HGLYPH_SENTINEL     ((ULONG) -1)
                                // GDI will never give us a glyph with a
                                // handle value of 0xffffffff, so we can
                                // use this as a sentinel for the end of
                                // our linked lists

#define GLYPH_HASH_SIZE     256

#define GLYPH_HASH_FUNC(x)  ((x) & (GLYPH_HASH_SIZE - 1))

// KV2 color adjustment
#define S3COLOR_ADJUST_FUNCTION                 0x2880

#define S3COLOR_ADJUST_STATUS                      1
#define S3COLOR_ADJUST_READCOLORADJUSTREGISTER     2
#define S3COLOR_ADJUST_WRITECOLORADJUSTREGISTER    3


//
//  Added escape call to return the DDC Monitor's EDID information
//
#define S3ESC_GET_EDID              0xF005

//
//  Mobile functions -- begin
//

//
//  Return codes for Escape calls
//
#define S3_MOBILE_SUCCESS           1
#define S3_MOBILE_FAILURE           0
#define S3_MOBILE_NOT_IMPLEMENTED   0xFFFFFFFF

//
//  Signals returned to S3Switch application to update its state
//  (WM_APP_UPDATE) or to call ChangeDisplaySettings for a modeset.
//
#define S3_MOBILE_NOTIFYAPP_UPDATE  0x8000
#define S3_MOBILE_NOTIFYAPP_MODESET 0x0001

//
//  S3Mobile Escape functions
//
#define S3ESC_MBL_FUNCTION          0x3000

//  S3Mobile Subfunction
//  Respect the ordering of the subfunction values, as they match those of
//  the existing display configuration utilities.
//
#define S3ESC_MBL_GET_PANEL_INFO            0
#define S3ESC_MBL_GET_DISPLAY_CONTROL       1
#define S3ESC_MBL_SET_DISPLAY_CONTROL       2
#define S3ESC_MBL_GET_HORZ_STATE            3
#define S3ESC_MBL_SET_HORZ_STATE            4
#define S3ESC_MBL_GET_VERT_STATE            5
#define S3ESC_MBL_SET_VERT_STATE            6
#define S3ESC_MBL_GET_CONNECT_STATUS        7
#define S3ESC_MBL_UPDATEREFRESH             8
#define S3ESC_MBL_GET_CRT_PAN_RES           9
#define S3ESC_MBL_SET_CRT_PAN_RES           10
#define S3ESC_MBL_GET_TIMING_STATE          11
#define S3ESC_MBL_SET_TIMING_STATE          12
#define S3ESC_MBL_GET_IMAGE_STATE           13
#define S3ESC_MBL_SET_IMAGE_STATE           14
#define S3ESC_MBL_GET_PRIMARY_DEVICE        15
#define S3ESC_MBL_SET_PRIMARY_DEVICE        16
#define S3ESC_MBL_GET_TV_FILTER_STATE       17
#define S3ESC_MBL_SET_TV_FILTER_STATE       18
#define S3ESC_MBL_GET_TV_UNDERSCAN_STATE    19
#define S3ESC_MBL_SET_TV_UNDERSCAN_STATE    20
#define S3ESC_MBL_GET_TV_OUTPUT_TYPE        21
#define S3ESC_MBL_SET_TV_OUTPUT_TYPE        22
#define S3ESC_MBL_GET_TV_POSITION           23
#define S3ESC_MBL_SET_TV_POSITION           24
#define S3ESC_MBL_GET_TV_CENTERING_OPTION   25
#define S3ESC_MBL_SET_TV_CENTERING_OPTION   26
#define S3ESC_MBL_GET_CRT_PAN_RES_TABLE     27
#define S3ESC_MBL_GET_CHIPID                28
#define S3ESC_MBL_GET_BIOS_VERSION          29
#define S3ESC_MBL_GET_CRT_CAPABILITY        30
#define S3ESC_MBL_SET_CRT_CAPABILITY        31
#define S3ESC_MBL_GET_VIDEOOUT_DEVICES      32
#define S3ESC_MBL_SET_VIDEOOUT_DEVICES      33
#define S3ESC_MBL_GET_TV_STANDARD           34
#define S3ESC_MBL_SET_TV_STANDARD           35
#define S3ESC_MBL_GET_VIDEO_MEMSIZE         36
#define S3ESC_MBL_SET_APP_HWND              37
#define S3ESC_MBL_CENTER_TV_VIEWPORT_NOW    38
#define S3ESC_MBL_QUERY_DEVICE_SUPPORT      60
// code 61 reserved for future use
// #define S3ESC_MBL_QUERY_CUSTOM_DRIVER       61
#define S3ESC_MBL_VALIDATE_HW_OVERLAY       62
#define S3ESC_MBL_VALIDATE_DUOVIEW          63
#define S3ESC_MBL_VALIDATE_LARGE_DESKTOP    64
#define S3ESC_MBL_GET_CRT_REFRESH_RATES     65
#define S3ESC_MBL_SET_CRT_REFRESH_RATE      66
#define S3ESC_MBL_STOP_FORCE_SW_POINTER     67

//
//  added for BIOS event tracking
//

//  S3ESC_MBL_EVENT_ENABLE and DISABLE redefined
//  as codes 41 and 42 (were 37 and 38)

#define S3ESC_MBL_EVENT_STATUS              39

//  M5 display switch status detection
#define S3ESC_MBL_DISPLAY_STATUS            40

#define S3ESC_MBL_EVENT_ENABLE              41
#define S3ESC_MBL_EVENT_DISABLE             42

//  APM and display Events
#define S3ESC_MBL_SUSPEND                   50
#define S3ESC_MBL_RESUME                    51
#define S3ESC_MBL_M5_EVENT_STATUS           52
#define S3ESC_MBL_GET_APP_HWND              53

//
//  Mobile subfunction input buffer definitions
//
//  Note: Helps track interface definition!
//
typedef struct _MBL_INP_GET_DISPLAY_CONTROL {
    ULONG MblEscSubfunction;
} MBL_INP_GET_DISPLAY_CONTROL;

typedef struct _MBL_INP_SET_DISPLAY_CONTROL {
    ULONG MblEscSubfunction;
    ULONG MblSetDisplayType;
    ULONG MblSetTvStandard;  // newer M5 utility does not request
} MBL_INP_SET_DISPLAY_CONTROL;

typedef struct _MBL_INP_GET_CONNECT_STATUS {
    ULONG MblEscSubfunction;
} MBL_INP_GET_CONNECT_STATUS;

typedef struct _MBL_INP_SET_CENTEREXPAND_MODE {
    ULONG MblEscSubfunction;
    ULONG MblSetCenterExpandMode;
} MBL_INP_SET_CENTEREXPAND_MODE;

typedef struct _MBL_INP_GET_CRT_PAN_RES_TABLE {
    ULONG MblEscSubfunction;
    ULONG MblShowAll;    // 1 means no pruning
} MBL_INP_GET_CRT_PAN_RES_TABLE;

typedef struct _MBL_INP_SET_PRIMARY_DEVICE {
    ULONG MblEscSubfunction;
    ULONG MblPrimaryDevice;
} MBL_INP_SET_PRIMARY_DEVICE;

typedef struct _MBL_INP_GET_TV_FILTER_STATE {
    ULONG MblEscSubfunction;
} MBL_INP_GET_TV_FILTER_STATE;

typedef struct _MBL_INP_SET_TV_FILTER_STATE {
    ULONG MblEscSubfunction;
    ULONG MblSetTvFFilterState;
    ULONG MblSetTvFFilterFraction;
} MBL_INP_SET_TV_FILTER_STATE;

typedef struct _MBL_INP_SET_TV_STANDARD {
    ULONG MblEscSubfunction;
    ULONG MblSetTvStandard;
} MBL_INP_SET_TV_STANDARD;

typedef struct _MBL_INP_GET_TV_UNDERSCAN_STATE {
    ULONG MblEscSubfunction;
} MBL_INP_GET_TV_UNDERSCAN_STATE;

typedef struct _MBL_INP_SET_TV_UNDERSCAN_STATE {
    ULONG MblEscSubfunction;
    ULONG MblSetTvUnderscanState;
} MBL_INP_SET_TV_UNDERSCAN_STATE;

typedef struct _MBL_INP_GET_CRT_REFRESH_RATES {
    ULONG MblEscSubfunction;
    ULONG MblShowAll;   // 1 means no pruning
    S3_APP_CRT_MODEFREQ MblModeInfo;  // defines mode for which to retrieve
} MBL_INP_GET_CRT_REFRESH_RATES;

typedef struct _MBL_INP_SET_CRT_REFRESH_RATE {
    ULONG MblEscSubfunction;
    S3_APP_CRT_MODEFREQ MblModeInfo;  // defines refresh and for which mode
} MBL_INP_SET_CRT_REFRESH_RATE;

//
//  Mobile subfunction output buffer definitions
//
typedef struct _MBL_OUTP_GET_DISPLAY_CONTROL {
    ULONG MblOutDisplayType;
    ULONG MblOutTvStandard; // newer M5 utility does not request
} MBL_OUTP_GET_DISPLAY_CONTROL;

typedef struct _MBL_OUTP_SET_DISPLAY_CONTROL {
    ULONG MblOutDisplaySet;
} MBL_OUTP_SET_DISPLAY_CONTROL;

typedef struct _MBL_OUTP_GET_CONNECT_STATUS {
    ULONG MblConnectionStatus;
} MBL_OUTP_GET_CONNECT_STATUS;

typedef struct _MBL_OUTP_GET_PANEL_INFO {
    USHORT MblOutXres;
    USHORT MblOutYres;
    USHORT MblOutPanelType;
} MBL_OUTP_GET_PANEL_INFO;

typedef struct _MBL_OUTP_GET_TV_FILTER_STATE {
    ULONG MblTvFFilterState;
    ULONG MblTvFFilterFraction;
} MBL_OUTP_GET_TV_FILTER_STATE;

typedef struct _MBL_OUTP_GET_TV_UNDERSCAN_STATE {
    ULONG MblFlickerUnderscanState;
} MBL_OUTP_GET_TV_UNDERSCAN_STATE;

typedef struct _MBL_OUTP_GET_DEVICE_SUPPORT {
    ULONG MblDisplayDeviceSupport;
    ULONG MblTvStandardSupport;
} MBL_OUTP_GET_DEVICE_SUPPORT;

//
// Mobile functions -- end.
//

//  Power Management defines

//  These IOCTL definitions should be same as those in NTDDVDEO.H
//  IOCTL_VIDEO_SET_POWER_MANAGEMENT - Tells the device to change the power
//                                    consumption level of the device to the
//                                    new state.
//  IOCTL_VIDEO_GET_POWER_MANAGEMENT - Return the current power consumption
//                                    level of the device.
//
//  NOTE:
//  These IOCTLs are based on the VESA DPMS proposal.
//  Changes to the DPMS standard will be refelcted in this IOCTL.
//

#define IOCTL_VIDEO_SET_POWER_MANAGEMENT \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x11b, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_GET_POWER_MANAGEMENT \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x11c, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Escape Functions
#define     SET_POWER_STATE     0x6000
#define     GET_POWER_STATE     0x6001

// End Power Management defines

#if APM_SUPPORT

typedef struct _SHADOW_REGS {
// Shadowed Accelerator port addresses:

   USHORT       wCUR_Y;                  // 0x082E8 , 0x08100
   USHORT       wCUR_X;                  // 0x086E8 , 0x08102

   USHORT       wDEST_Y;                 // 0x08AE8 , 0x08108
   USHORT       wDEST_X;                 // 0x08EE8 , 0x0810A

   USHORT       wERR_TERM;               // 0x092E8 , 0x08110
   USHORT       wCMD;                    // 0x09AE8 , 0x08118

   ULONG        dwBKGD_COLOR;            // 0x0A2E8 , 0x08120
   ULONG        dwFRGD_COLOR;            // 0x0A6E8 , 0x08124
   ULONG        dwWRT_MASK;              // 0x0AAE8 , 0x08128
   ULONG        dwRD_MASK;               // 0x0AEE8 , 0x0812C
   ULONG        dwCOLOR_CMP;             // 0x0B2E8 , 0x08130

   USHORT       wBKGD_MIX;               // 0x0B6E8 , 0x08134
   USHORT       wFRGD_MIX;               // 0x0BAE8 , 0x08136

   USHORT       wPIX_TRANS;              // 0x0E2E8

   USHORT       wMIN_AXIS_PCNT;          // 0x0BEE8_0 , 0x8148
   USHORT       wMAJ_AXIS_PCNT;          // 0x096E8    , 0x814A

   USHORT       wSCISSORS_T;             // 0x0BEE8_1 , 0x08138
   USHORT       wSCISSORS_L;             // 0x0BEE8_2 , 0x0813A
   USHORT       wSCISSORS_B;             // 0x0BEE8_3 , 0x0813C
   USHORT       wSCISSORS_R;             // 0x0BEE8_4 , 0x0813E

   USHORT       wPIX_CNTL;               // 0x0BEE8_A , 0x08140

   USHORT       wMULT_MISC;              // 0x0BEE8_E , 0x08144
   USHORT       wREAD_SEL;               // 0x0BEE8_F

} SHADOW_REGS;

#endif


typedef struct _CACHEDGLYPH CACHEDGLYPH;
typedef struct _CACHEDGLYPH
{
    CACHEDGLYPH*    pcgNext;    // Points to next glyph that was assigned
                                //   to the same hash table bucket
    HGLYPH          hg;         // Handles in the bucket-list are kept in
                                //   increasing order
    POINTL          ptlOrigin;  // Origin of glyph bits

    // Device specific fields below here:

    LONG            cxLessOne;  // Glyph width less one
    LONG            cyLessOne;  // Glyph height less one
    LONG            cxcyLessOne;// Packed width and height, less one
    LONG            cw;         // Number of words to be transferred
    LONG            cd;         // Number of dwords to be transferred
    ULONG           ad[1];      // Start of glyph bits
} CACHEDGLYPH;  /* cg, pcg */

typedef struct _GLYPHALLOC GLYPHALLOC;
typedef struct _GLYPHALLOC
{
    GLYPHALLOC*     pgaNext;    // Points to next glyph structure that
                                //   was allocated for this font
    CACHEDGLYPH     acg[1];     // This array is a bit misleading, because
                                //   the CACHEDGLYPH structures are actually
                                //   variable sized
} GLYPHAALLOC;  /* ga, pga */

typedef struct _CACHEDFONT CACHEDFONT;
typedef struct _CACHEDFONT
{
    CACHEDFONT*     pcfNext;    // Points to next entry in CACHEDFONT list
    CACHEDFONT*     pcfPrev;    // Points to previous entry in CACHEDFONT list
    GLYPHALLOC*     pgaChain;   // Points to start of allocated memory list
    CACHEDGLYPH*    pcgNew;     // Points to where in the current glyph
                                //   allocation structure a new glyph should
                                //   be placed
    LONG            cjAlloc;    // Bytes remaining in current glyph allocation
                                //   structure
    CACHEDGLYPH     cgSentinel; // Sentinel entry of the end of our bucket
                                //   lists, with a handle of HGLYPH_SENTINEL
    CACHEDGLYPH*    apcg[GLYPH_HASH_SIZE];
                                // Hash table for glyphs

} CACHEDFONT;   /* cf, pcf */

#ifndef MCD95

typedef struct _XLATECOLORS {       // Specifies foreground and background
ULONG   iBackColor;                 //   colours for faking a 1bpp XLATEOBJ
ULONG   iForeColor;
} XLATECOLORS;                          /* xlc, pxlc */

BOOL bEnableText(PDEV*);
VOID vDisableText(PDEV*);
//VOID vAssertModeText(PDEV*, BOOL);

VOID vFastText(GLYPHPOS*, ULONG, BYTE*, ULONG, ULONG, RECTL*, RECTL*,
               FLONG, RECTL*, RECTL*);
VOID vClearMemDword(ULONG*, ULONG);

//////////////////////////////////////////////////////////////////////
// Dither stuff

// Describes a single colour tetrahedron vertex for dithering:

typedef struct _VERTEX_DATA {
    ULONG ulCount;              // Number of pixels in this vertex
    ULONG ulVertex;             // Vertex number
} VERTEX_DATA;                      /* vd, pv */

VERTEX_DATA*    vComputeSubspaces(ULONG, VERTEX_DATA*);
VOID            vDitherColor(ULONG*, VERTEX_DATA*, VERTEX_DATA*, ULONG);

#endif

//////////////////////////////////////////////////////////////////////
//
//  Brush stuff
//
//  'Slow' brushes are used when we don't have hardware pattern capability,
//  and we have to handle patterns using screen-to-screen blts:

#define SLOW_BRUSH_CACHE_DIM    3   // Controls the number of brushes cached
                                    // in off-screen memory, when we don't
                                    // have the S3 hardware pattern support.
                                    // We allocate 3 x 3 brushes, so we can
                                    // cache a total of 9 brushes:
#define SLOW_BRUSH_COUNT        (SLOW_BRUSH_CACHE_DIM * SLOW_BRUSH_CACHE_DIM)
#define SLOW_BRUSH_DIMENSION    64  // After alignment is taken care of,
                                    // every off-screen brush cache entry
                                    // will be 64 pels in both dimensions
#define SLOW_BRUSH_ALLOCATION   (SLOW_BRUSH_DIMENSION + 8)
                                    // Actually allocate 72x72 pels for each
                                    // pattern, using the 8 extra for brush
                                    // alignment

// 'Fast' brushes are used when we have hardware pattern capability:

#define FAST_BRUSH_COUNT        16  // Total number of non-hardware brushes
                                    // cached off-screen
#define FAST_BRUSH_DIMENSION    8   // Every off-screen brush cache entry
                                    // is 8 pels in both dimensions
#define FAST_BRUSH_ALLOCATION   8   // We have to align ourselves, so this is
                                    // the dimension of each brush allocation

// Common to both implementations:

#define RBRUSH_2COLOR           1   // For RBRUSH flags

#define TOTAL_BRUSH_COUNT       max(FAST_BRUSH_COUNT, SLOW_BRUSH_COUNT)
                                    // This is the maximum number of brushes
                                    // we can possibly have cached off-screen
#define TOTAL_BRUSH_SIZE        64  // We'll only ever handle 8x8 patterns,
                                    // and this is the number of pels

typedef struct _BRUSHENTRY BRUSHENTRY;

// NOTE: Changes to the RBRUSH or BRUSHENTRY structures must be reflected
//       in strucs.inc!

typedef struct _RBRUSH {
    FLONG       fl;             // Type flags
    BOOL        bTransparent;   // Transparency flag
                                // TRUE if brush was realized for a transparent
                                // blt (meaning colours are white and black),
                                // FALSE if not (meaning it's already been
                                // colour-expanded to the correct colours).
                                // Value is undefined if the brush isn't
                                // 2 colour.
                                //
    ULONG       ulForeColor;    // Foreground colour if 1bpp
    ULONG       ulBackColor;    // Background colour if 1bpp
    POINTL      ptlBrushOrg;    // Brush origin of cached pattern.  Initial
                                // value should be -1
    BRUSHENTRY* apbe[MAX_BOARDS];// Points to brush-entry that keeps track
                                // of the cached off-screen brush bits
    ULONG*      pulAligned;     // Pointer to an aligned copy of the brush
                                // Used on 3D chips only!
    ULONG       aulPattern[1];  // Open-ended array for keeping copy of the
                                // actual pattern bits in case the brush
                                // origin changes, or someone else steals
                                // our brush entry (declared as a ULONG
                                // for proper dword alignment)
    //
    // Don't put anything after here (aulPattern[1]), or you'll be sorry!
    //

} RBRUSH;                           /* rb, prb */

typedef struct _BRUSHENTRY {
    RBRUSH*     prbVerify;      // We never dereference this pointer to
                                // find a brush realization; it is only
                                // ever used in a compare to verify
                                // that for a given realized brush, our
                                // off-screen brush entry is still valid.
    LONG        x;              // x-position of cached pattern
    LONG        y;              // y-position of cached pattern

} BRUSHENTRY;                       /* be, pbe */

typedef union _RBRUSH_COLOR {
    RBRUSH*     prb;
    ULONG       iSolidColor;
} RBRUSH_COLOR;                     /* rbc, prbc */

BOOL bEnableBrushCache(PDEV*);
VOID vDisableBrushCache(PDEV*);
VOID vAssertModeBrushCache(PDEV*, BOOL);

//////////////////////////////////////////////////////////////////////
// Stretch stuff

typedef struct _STR_BLT {
    PDEV*   ppdev;
    PBYTE   pjSrcScan;
    LONG    lDeltaSrc;
    LONG    XSrcStart;
    PBYTE   pjDstScan;
    LONG    lDeltaDst;
    LONG    XDstStart;
    LONG    XDstEnd;
    LONG    YDstStart;
    LONG    YDstCount;
    ULONG   ulXDstToSrcIntCeil;
    ULONG   ulXDstToSrcFracCeil;
    ULONG   ulYDstToSrcIntCeil;
    ULONG   ulYDstToSrcFracCeil;
    ULONG   ulXFracAccumulator;
    ULONG   ulYFracAccumulator;
} STR_BLT;

typedef VOID (*PFN_DIRSTRETCH)(STR_BLT*);

VOID vDirectStretch8Narrow(STR_BLT*);
VOID vDirectStretch8(STR_BLT*);
VOID vDirectStretch16(STR_BLT*);
VOID vDirectStretch32(STR_BLT*);

/////////////////////////////////////////////////////////////////////////
// Heap stuff

typedef enum
{
    OH_FREE = 0,        // The off-screen allocation is available for use
    OH_DISCARDABLE,     // The allocation is occupied by a discardable bitmap
                        //   that may be moved out of off-screen memory
    OH_PERMANENT,       // The allocation is occupied by a permanent bitmap
                        //   that cannot be moved out of off-screen memory
} OHSTATE;

typedef struct _DSURF DSURF;
typedef struct _OH OH;
typedef struct _OH
{
#ifndef MCD95
    OHSTATE  ohState;       // State of off-screen allocation
#endif
    LONG     x;             // x-coordinate of left edge of allocation
    LONG     y;             // y-coordinate of top edge of allocation
    LONG     cx;            // Width in pixels of allocation
    LONG     cy;            // Height in pixels of allocation
    LONG     lDelta;        // Distance from one scan to the next.
#ifndef MCD95
    LONG     cxReserved;    // Dimensions of original reserved rectangle;
    LONG     cyReserved;    //   zero if rectangle is not 'reserved'
    OH*      pohNext;       // When OH_FREE or OH_RESERVE, points to the next
                            //   free node, in ascending cxcy value.  This is
                            //   kept as a circular doubly-linked list with a
                            //   sentinel at the end.
                            // When OH_DISCARDABLE, points to the next most
                            //   recently created allocation.  This is kept as
                            //   a circular doubly-linked list.
    OH*      pohPrev;       // Opposite of 'pohNext'
    ULONG    cxcy;          // Width and height in a dword for searching
    DSURF*   pdsurf;        // Points to our DSURF structure
    ULONG    ulSize;        // Available Size
#endif
    VOID*    pvScan0;       // Points to start of first scan-line
    ULONG    ulBase;
#ifdef MCD95
    FLONG            fl;            // Allocation flags
    MCD_DDRAWINFO    mcdDD;         // MCD DirectDraw info
    LONG             cLocks;        // Lock count
    BOOL             bLostSurf;
#endif
};              /* oh, poh */

// This is the smallest structure used for memory allocations:

typedef struct _OHALLOC OHALLOC;
typedef struct _OHALLOC
{
    OHALLOC* pohaNext;
    OH       aoh[1];
} OHALLOC;      /* oha, poha */

typedef struct _HEAP
{
    ULONG   ulMaxFreeSize;      // Largest possible free space
    ULONG   ulMaxOffscrnSize;   // Largest offscreen Size

    OH       ohFree;        // Head of the free list, containing those
                            //   rectangles in off-screen memory that are
                            //   available for use.  pohNext points to
                            //   hte smallest available rectangle, and pohPrev
                            //   points to the largest available rectangle,
                            //   sorted by cxcy.
    OH       ohDiscardable; // Head of the discardable list that contains all
                            //   bitmaps located in offscreen memory that
                            //   are eligible to be tossed out of the heap.
                            //   It is kept in order of creation: pohNext
                            //   points to the most recently created; pohPrev
                            //   points to the least recently created.
    OH       ohPermanent;   // List of permanently allocated rectangles
    OH*      pohFreeList;   // List of OH node data structures available
    OHALLOC* pohaChain;     // Chain of allocations
} HEAP;         /* heap, pheap */

typedef enum {
    DT_SCREEN,              // Surface is kept in screen memory
    DT_DIB                  // Surface is kept as a DIB
} DSURFTYPE;    /* dt, pdt */

typedef struct _DSURF
{
    DSURFTYPE dt;           // DSURF status (whether off-screen or in a DIB)
    SIZEL     sizl;         // Size of the original bitmap (could be smaller
                            //   than poh->sizl)
    PDEV*     ppdev;        // Need this for deleting the bitmap
    union {
        OH*         poh;    // If DT_SCREEN, points to off-screen heap node
        SURFOBJ*    pso;    // If DT_DIB, points to locked GDI surface
    };

    // The following are used for DT_DIB only...

    ULONG     cBlt;         // Counts down the number of blts necessary at
                            //   the current uniqueness before we'll consider
                            //   putting the DIB back into off-screen memory
    ULONG     iUniq;        // Tells us whether there have been any heap
                            //   'free's since the last time we looked at
                            //   this DIB

} DSURF;                          /* dsurf, pdsurf */

// GDI expects dword alignment for any bitmaps on which it is expected
// to draw.  Since we occasionally ask GDI to draw directly on our off-
// screen bitmaps, this means that any off-screen bitmaps must be dword
// aligned in the frame buffer.  We enforce this merely by ensuring that
// all off-screen bitmaps are four-pel aligned (we may waste a couple of
// pixels at the higher colour depths):

//bchernis:Steve Gibson's suggested fix to keep the correct stride granularity
//#define HEAP_X_ALIGNMENT    8
#define HEAP_X_ALIGNMENT    16

// Number of blts necessary before we'll consider putting a DIB DFB back
// into off-screen memory:

#define HEAP_COUNT_DOWN     6

// Flags for 'pohAllocate':

typedef enum {
    FLOH_ONLY_IF_ROOM       = 0x0001,   // Don't kick stuff out of off-
                                        //   screen memory to make room
    FLOH_MAKE_PERMANENT     = 0x0002,   // Allocate a permanent entry
    FLOH_RESERVE            = 0x0004,   // Allocate an off-screen entry,
                                        //   but let it be used by discardable
                                        //   bitmaps until it is needed
#ifdef MCD95
    FLOH_FRONTBUFFER        = 0x1000,   // Front buffer access
    FLOH_BACKBUFFER         = 0x2000,   // MCD backbuffer allocation
    FLOH_ZBUFFER            = 0x4000,   // MCD zbuffer allocation
    FLOH_TEXTURE            = 0x8000,   // MCD texture allocation
#endif
} FLOH;

// Publicly callable heap APIs:

OH*  pohAllocate(PDEV*, LONG, LONG, FLOH);
BOOL bOhCommit(PDEV*, OH*, BOOL);
OH*  pohFree(PDEV*, OH*);

#ifdef MCD95
OH* pohLock(PDEV *ppdev, OH *poh);
OH* pohUnlock(PDEV *ppdev, OH *poh);
OH* pohValid(PDEV *ppdev, OH *poh);
#endif

OH*  pohMoveOffscreenDfbToDib(PDEV*, OH*);
BOOL bMoveDibToOffscreenDfbIfRoom(PDEV*, DSURF*);
BOOL bMoveAllDfbsFromOffscreenToDibs(PDEV* ppdev);


BOOL bEnableOffscreenHeap(PDEV*);
VOID vDisableOffscreenHeap(PDEV*);
BOOL bAssertModeOffscreenHeap(PDEV*, BOOL);


#ifndef MCD95
BOOL bEnableBanking(PDEV*);
VOID vDisableBanking(PDEV*);


/////////////////////////////////////////////////////////////////////////
//  Pointer stuff

#define POINTER_DATA_SIZE       40      // Number of bytes to allocate for the
                                        //   miniport down-loaded pointer code
                                        //   working space
#define HW_INVISIBLE_OFFSET     2       // Offset from 'ppdev->yPointerBuffer'
                                        //   to the invisible pointer
#define HW_POINTER_DIMENSION    64      // Maximum dimension of default
                                        //   (built-in) hardware pointer
#define HW_POINTER_HIDE         63      // Hardware pointer start pixel
                                        //   position used to hide the pointer
#define HW_POINTER_TOTAL_SIZE   1024    // Total size in bytes required
                                        //   to define the hardware pointer
                                        //   (must be a power of 2 for
                                        //   allocating space for the shape)

typedef VOID (FNSHOWPOINTER)(PDEV*, VOID*, BOOL);
typedef VOID (FNMOVEPOINTER)(PDEV*, VOID*, LONG, LONG);
typedef BOOL (FNSETPOINTERSHAPE)(PDEV*, VOID*, LONG, LONG, LONG, LONG, LONG,
                                 LONG, BYTE*, FLONG );
typedef VOID (FNENABLEPOINTER)(PDEV*, VOID*, BOOL);

BOOL bEnablePointer(PDEV*);
VOID vAssertModePointer(PDEV*, BOOL);

/////////////////////////////////////////////////////////////////////////
//  Palette stuff

BOOL bEnablePalette(PDEV*);
VOID vDisablePalette(PDEV*);
VOID vAssertModePalette(PDEV*, BOOL);

BOOL bInitializePalette(PDEV*, DEVINFO*);
VOID vUninitializePalette(PDEV*);

#define MAX_CLUT_SIZE (sizeof(VIDEO_CLUT) + (sizeof(ULONG) * 256))

/////////////////////////////////////////////////////////////////////////
//  DirectDraw stuff

//  There's a 64K granularity that applies to the mapping of the frame
//  buffer into the application's address space:

#define ROUND_UP_TO_64K(x)  (((ULONG)(x) + 0x10000 - 1) & ~(0x10000 - 1))

//  Defines we'll use in the surface's 'dwReserved1' field:

#define DD_RESERVED_DIFFERENTPIXELFORMAT    0x0001

//  FourCC formats are encoded in reverse because we're little endian:

#define FOURCC_YUY2         '2YUY'  // Encoded in reverse because we're little
#define FOURCC_Y211         '112Y'  //   endian
#define FOURCC_UYVY         'YVYU'  //   add for GXLT

//  Worst-case possible number of FIFO entries we'll have to wait for in
//  DdBlt for any operation:

#define DDBLT_FIFO_COUNT    13          // TRIO: used 9 instead of 13

typedef struct _FLIPRECORD
{
    FLATPTR         fpFlipFrom;             // Surface we last flipped from
    LONGLONG        liFlipTime;             // Time at which last flip
                                            //   occured
    LONGLONG        liFlipDuration;         // Precise amount of time it
                                            //   takes from vblank to vblank
    BOOL            bHaveEverCrossedVBlank; // True if we noticed that we
                                            //   switched from inactive to
                                            //   vblank
    BOOL            bWasEverInDisplay;      // True if we ever noticed that
                                            //   we were inactive
    BOOL            bFlipFlag;              // True if we think a flip is
                                            //   still pending
} FLIPRECORD;

BOOL bEnableDirectDraw(PDEV*);
VOID vDisableDirectDraw(PDEV*);
VOID vAssertModeDirectDraw(PDEV*, BOOL);
VOID vTurnOnStreamsProcessorMode(PDEV* ppdev);
VOID vTurnOffStreamsProcessorMode(PDEV* ppdev);
DWORD DdBlt(PDD_BLTDATA lpBlt);
HRESULT ddrvalUpdateFlipStatus(PDEV* ppdev, FLATPTR fpVidMem);
DWORD DdUpdateOverlay(PDD_UPDATEOVERLAYDATA lpUpdateOverlay);
DWORD dwGetPaletteEntry(PDEV* ppdev, DWORD iIndex);
VOID vGetDisplayDuration(PDEV* ppdev);
VOID vGetDisplayDurationIGA2( PDEV * );
DWORD DdGetScanLine (PDD_GETSCANLINEDATA);
DWORD DdUnlock (PDD_UNLOCKDATA);

#endif

//////////////////////////////////////////////////////////////////////
// Low-level blt function prototypes
#ifndef MCD95
typedef VOID (FNFILL)(PDEV*, OH*, LONG, RECTL*, ULONG, ULONG, RBRUSH_COLOR,
                      POINTL*);
typedef VOID (FNXFER)(PDEV*, OH*, LONG, RECTL*, ULONG, ULONG, SURFOBJ*, POINTL*,
                      RECTL*, XLATEOBJ*);
#endif
typedef VOID (FNCOPY)(PDEV*, OH*, OH*, LONG, RECTL*, ULONG, POINTL*, RECTL*);
#ifndef MCD95
typedef VOID (FNFASTPATREALIZE)(PDEV*, OH*, RBRUSH*, POINTL*, BOOL);
typedef VOID (FNIMAGETRANSFER)(PDEV*, BYTE*, LONG, LONG, LONG, ULONG);
typedef BOOL (FNTEXTOUT)(SURFOBJ*, OH*, STROBJ*, FONTOBJ*, CLIPOBJ*, RECTL*,
                         BRUSHOBJ*, BRUSHOBJ*);
typedef VOID (FNPATXFER)(PDEV*, OH*, LONG, RECTL*, ULONG, ULONG, SURFOBJ*,
                         POINTL*, RECTL*, XLATEOBJ*, BRUSHOBJ*, POINTL*);
typedef VOID (FNPATCOPY)(PDEV*, OH*, OH*, LONG, RECTL*, ULONG, POINTL*, RECTL*,
                         BRUSHOBJ*, POINTL*);

typedef VOID (FNPUTGETBITS)( PDEV*, SURFOBJ*, RECTL*, POINTL* );

typedef VOID (FNSTREAMS)( PDEV * );


FNXFER              vXferScreenTo1bpp;
FNXFER              vXferNativeSrccopy;

FNFILL              vMmFillSolid3D;
FNXFER              vMmXfer1bpp3D;
FNXFER              vMmXferNative3D;
#endif
FNCOPY              vMmCopyBlt3D;
#ifndef MCD95
FNFILL              vMmFillPatFast3D;
FNFASTPATREALIZE    vMmFastPatRealize3D;
FNPATXFER           vMmPatXfer1bpp3D;
FNPATXFER           vMmPatXferNative3D;
FNPATCOPY           vMmPatCopyBlt3D;

FNPUTGETBITS vPutBits;
FNPUTGETBITS vGetBits;
VOID vAlignedCopy(PDEV*,   BYTE**, LONG, BYTE** , LONG, LONG, LONG, BOOL);

CACHEDFONT* pcfAllocateCachedFont(PDEV*   ppdev);

VOID FIX_ViRGE(PDEV*);
VOID S3DImageTransferMm32(PDEV*, BYTE*, LONG, LONG, LONG, ULONG, ULONG);
#endif

VOID GetDllName(DEVMODEW*   pdm);
VOID InitPDevFuncPointers(PDEV* ppdev);

FNSTREAMS vTurnOnStreamsProcessorMode;
FNSTREAMS vTurnOffStreamsProcessorMode;
FNSTREAMS vTurnOnGX2StreamsProcessorMode;
FNSTREAMS vTurnOffGX2StreamsProcessorMode;

//  For now, we're linear only when using with 'New MM I/O'
#define CAPS_LINEAR_FRAMEBUFFER CAPS_NEW_MMIO

//  DIRECT_ACCESS(ppdev) returns TRUE if GDI and DCI can directly access
//  the frame buffer.  It returns FALSE if there are hardware bugs
//  when reading words or dwords from the frame buffer that cause non-x86
//  systems to crash.  It will also return FALSE is the Alpha frame buffer
//  is mapped in using 'sparse space'.

#if !defined(_ALPHA_)
    #define DIRECT_ACCESS(ppdev)    1
#else
    #define DIRECT_ACCESS(ppdev)    \
        (!(ppdev->ulCaps & (CAPS_NO_DIRECT_ACCESS | CAPS_SPARSE_SPACE)))
#endif

//  DENSE(ppdev) returns TRUE if the normal 'dense space' mapping of the
//  frame buffer is being used.  It returns FALSE only on the Alpha when
//  the frame buffer is mapped in using 'sparse space,' meaning that all
//  reads and writes to and from the frame buffer must be done through the
//  funky 'ioaccess.h' macros.

#if defined(_ALPHA_)
    #define DENSE(ppdev)        (!(ppdev->ulCaps & CAPS_SPARSE_SPACE))
#else
    #define DENSE(ppdev)        1
#endif

////////////////////////////////////////////////////////////////////////
//  Status flags

typedef enum {
    STAT_GLYPH_CACHE        = 0x0001,   // Glyph cache successfully allocated
    STAT_BRUSH_CACHE        = 0x0002,   // Brush cache successfully allocated
    STAT_DIRECTDRAW_CAPABLE = 0x0004,   // Card is DirectDraw capable
    STAT_TVUNDERSCAN_ENABLED= 0x0008,   // TV Underscanning is enabled
    STAT_STREAMS_ENABLED    = 0x0010,   // Streams are enabled for this mode
    STAT_SW_CURSOR_PAN      = 0x0040,   // software cursor panning enabled
    STAT_PALSUPPORT_ENABLED = 0x0080,   // TV is a PAL TV
    STAT_STREAMS_UTILIZED   = 0x0100,   // Streams enabled AND currently being
                                        // used (vs not off due to bandwidth.)
    STAT_DDRAW_PAN          = 0x0200    // Panning under DDRAW (DrvMovePointer
                                        // calls only)
} STATUS;

#ifdef MCD95
////////////////////////////////////////////////////////////////////////
// The DIB Engine data structure -- WIN95 ONLY!

typedef struct
{
    WORD         deType;
    WORD         deWidth;
    WORD         deHeight;
    WORD         deWidthBytes;
    BYTE         dePlanes;
    BYTE         deBitsPixel;
    DWORD        deReserved1;
    DWORD        deDeltaScan;
    LPBYTE       delpPDevice;               // Warning: not 32-bit flat
    DWORD        deBitsOffset;              // Warning: not 32-bit flat
    WORD         deBitsSelector;
    WORD         deFlags;
    WORD         deVersion;
    LPBITMAPINFO deBitmapInfo;              // Warning: not 32-bit flat
    void         (FAR *deBeginAccess)();    // Warning: not 32-bit flat
    void         (FAR *deEndAccess)();      // Warning: not 32-bit flat
    DWORD        deDriverReserved;
} DIBENGINE, FAR *LPDIBENGINE;
#endif


// Global Data for D3D
#if _WIN32_WINNT >= 0x0500
typedef struct {
        struct _S3_CONTEXT        *FirstContxt;
        // hw parm regs for 3D triangle
        volatile        vi13D_TRIANGLE FAR *g_p3DTri;
        // hw setup regs for 3D triangle
        volatile        vi13D_SETUP FAR *g_p3DStp;
#ifdef IS_32
        BOOL            bInitialized;
        BOOL            DMA_Choice;
#else
        DWORD           bInitialized;
        DWORD           DMA_Choice;
#endif
        DWORD           dwWhiteTexture;
        volatile ULONG  dma_possible;
        double          __UVRANGE;
        double          coord_adj;
        ULONG           uBaseHigh ;
        unsigned long   g_DmaReadPtr;
        unsigned long   g_DmaIndex;
        unsigned long     CMD_SET ;
        unsigned long*  g_DmaLinAddr ;
        volatile unsigned long *g_lpReadPtrReg;
        volatile unsigned long *g_lpWritePtrReg;
        volatile unsigned long *g_lpDmaEnableReg;
        BYTE            DXGX ;
} D3DGLOBALS;
typedef D3DGLOBALS FAR * LPD3DGLOBALS;
#endif

//
//  Some defines for MCD function prototypes in PDEV.
//

////////////////////////////////////////////////////////////////////////
//  The Physical Device data structure

#ifndef MCD95
typedef struct  _PDEV
{
    // -------------------------------------------------------------------
    // NOTE: Changes between here and NOTE1 in the PDEV structure must be
    // reflected in i386\strucs.inc (assuming you're on an x86, of course)!

    LONG        xOffset;                // Pixel offset from (0, 0) to current
    LONG        yOffset;                //   DFB located in off-screen memory
    BYTE*       pjMmBase;               // Start of memory mapped I/O
    BYTE*       pjScreen;               // Points to base screen address
    LONG        lDelta;                 // Distance from one scan to the next.
    LONG        cjPelSize;              // 1 if 8bpp, 2 if 16bpp, 3 if 24bpp,
                                        //   4 if 32bpp
    ULONG       iBitmapFormat;          // BMF_8BPP or BMF_16BPP or BMF_32BPP
                                        //   (our current colour depth)
    LONG        iBoard;                 // Logical multi-board identifier
                                        //   (zero by default)

    // Important data for accessing the frame buffer.

    BOOL        bMmIo;                  // Can do CAPS_MM_IO

    //
    // Capabilities bits (note that these are set, based on miniport
    // PER-MODE information, upon modeset)
    //
    ULONG       ulCaps;
    ULONG       ulCaps2;
    ULONG       ulCaps3;
    ULONG       ulCaps4;
    ULONG       ulCaps5;
    ULONG       ulCaps6;
    ULONG       ulCaps7;
    ULONG       ulCaps8;


    // -------------------------------------------------------------------
    // NOTE1: Changes up to here in the PDEV structure must be reflected in
    // i386\strucs.inc (assuming you're on an x86, of course)!

    STATUS      flStatus;               // Status flags

    BOOL        bEnabled;               // In graphics mode (not full-screen)
    BOOL        bMustNotifyAppModeSet;  // Signal that config app must update
    BOOL        bHwOverlayOff;          // HwOverlay streams operation disabled
    BOOL        bVertInterpSynched;     // Set to 1 when Vertical interpolation
                                        // vertical count has been synched.
    BOOL        bCompaqDispSwitchPending;// COMPAQ flag to prevent processing
                                        // display switches while one's already
                                        // being done.

    HANDLE      hDriver;                // Handle to \Device\Screen
    HDEV        hdevEng;                // Engine's handle to PDEV
    HSURF       hsurfScreen;            // Engine's handle to screen surface
    DSURF*      pdsurfScreen;           // Our private DSURF for the screen

    LONG        cxScreen;               // Visible screen width
    LONG        cyScreen;               // Visible screen height
    LONG        cxMemory;               // Width of Video RAM
    LONG        cyMemory;               // Height of Video RAM
    LONG        cBitsPerPel;            // Bits per pel (8, 15, 16, 24 or 32)
    ULONG       ulMode;                 // Mode the mini-port driver is in.

    FLONG       flHooks;                // What we're hooking from GDI
    ULONG       ulWhite;                // 0xff if 8bpp, 0xffff if 16bpp,
                                        //   0xffffffff if 32bpp
    UCHAR*      pjIoBase;               // Mapped IO port base for this PDEV
    VOID*       pvTmpBuffer;            // General purpose temporary buffer,
                                        //   TMP_BUFFER_SIZE bytes in size
                                        //   (Remember to synchronize if you
                                        //   use this for device bitmaps or
                                        //   async pointers)
    USHORT*     apwMmXfer[XFER_BUFFERS];// Pre-computed array of unique
    ULONG*      apdMmXfer[XFER_BUFFERS];//   addresses for doing memory-mapped
                                        //   transfers without memory barriers
                                        // Note that the 868/968 chips have a
                                        //   hardware bug and can't do byte
                                        //   transfers
    HSEMAPHORE  csCrtc;                 // Used for synchronizing access to
                                        //   the CRTC register

    LONG        cPelSize;               // 0 if 8bpp, 1 if 16bpp, 2 if 32bpp
    ULONG       ulCommandBase;          // Base for commands sent to 3d chips
    ULONG       ulLastLineDirection;    // Direction flag for last line drawn


    ////////// Low-level blt function pointers:

    FNFILL*             pfnFillSolid;
    FNFILL*             pfnFillPat;
    FNXFER*             pfnXfer1bpp;
    FNXFER*             pfnXfer4bpp;
    FNXFER*             pfnXferNative;
    FNCOPY*             pfnCopyBlt;
    FNFASTPATREALIZE*   pfnFastPatRealize;
    FNIMAGETRANSFER*    pfnImageTransfer;
    FNTEXTOUT*          pfnTextOut;

    //
    // Virge patch variables
    //
#ifdef  VIRGE_PATCH12 //----------------

    LONG        Virge_Patch12_MinWidth;     // Minimum width to apply patch on
    LONG        Virge_Patch12_MaxWidth;     // Maximum width to apply patch on
    LONG        Virge_Patch12_BltWidth;     // Blt width to perform

#endif // VIRGE_PATCH12 //--------------

#ifdef  VIRGE_PATCH15 //----------------

    LONG        Virge_Patch15_MaxWidth;     // Maximum width to apply patch on
    LONG        Virge_Patch15_BltWidth;     // Blt width to perform

#endif // VIRGE_PATCH15 //--------------

    ////////// Palette stuff:

    PALETTEENTRY* pPal;                 // The palette if palette managed
    HPALETTE    hpalDefault;            // GDI handle to the default palette.
    FLONG       flRed;                  // Red mask for 16/32bpp bitfields
    FLONG       flGreen;                // Green mask for 16/32bpp bitfields
    FLONG       flBlue;                 // Blue mask for 16/32bpp bitfields
    ULONG       cPaletteShift;          // number of bits the 8-8-8 palette must
                                        // be shifted by to fit in the hardware
                                        // palette.
    ////////// Heap stuff:

    HEAP        heap;                   // All our off-screen heap data
    ULONG       iHeapUniq;              // Incremented every time room is freed
                                        //   in the off-screen heap
    SURFOBJ*    psoPunt;                // Wrapper surface for having GDI draw
                                        //   on off-screen bitmaps
    SURFOBJ*    psoPunt2;               // Another one for off-screen to off-
                                        //   screen blts
    OH*         pohScreen;              // Off-screen heap structure for the
                                        //   visible screen

    ULONG       ulFrameBufferLength;

    ////////// Banking stuff:

    SURFOBJ*    psoBank;                // Surface object for banked call backs
                                        // Private work area for downloaded
                                        //   miniport banking code


    ////////// Pointer stuff:

    BOOL        bHwPointerActive;       // Currently using the h/w pointer?
    BOOL        bForceSwPointer;      	 // Force to use s/w cursor if TRUE
    LONG        xPointerHot;            // xHot of current hardware pointer
    LONG        yPointerHot;            // yHot of current hardware pointer

    LONG        cjPointerOffset;        // Byte offset from start of frame
                                        //   buffer to off-screen memory where
                                        //   we stored the pointer shape
    LONG        xPointerShape;          // x-coordinate
    LONG        yPointerShape;          // y-coordinate
    LONG        iPointerBank;           // Bank containing pointer shape
    ULONG       ulPointerCheckSum;      // Keep track the HW cursor's checksum
                                        // Update the HW cursor upon the checksum
    VOID*       pvPointerShape;         // Points to pointer shape when bank
                                        //   is mapped in
    LONG        xPointer;               // Start x-position for the current
                                        //   S3 pointer
    LONG        yPointer;               // Start y-position for the current
                                        //   S3 pointer
    LONG        dxPointer;              // Start x-pixel position for the
                                        //   current S3 pointer
    LONG        dyPointer;              // Start y-pixel position for the
                                        //   current S3 pointer
    LONG        cPointerShift;          // Horizontal scaling factor for
                                        //   hardware pointer position

    ULONG       ulHwGraphicsCursorModeRegister_45;
                                        // Default value for index 45
    VOID*       pvPointerData;          // Points to ajPointerData[0]
    BYTE        ajPointerData[POINTER_DATA_SIZE];
                                        // Private work area for downloaded
                                        //   miniport pointer code

    FNSHOWPOINTER*      pfnShowPointer;
    FNMOVEPOINTER*      pfnMovePointer;
    FNSETPOINTERSHAPE*  pfnSetPointerShape;
    FNENABLEPOINTER*    pfnEnablePointer;

    LONG        iBrushCache;            // Index for next brush to be allocated
    LONG        cBrushCache;            // Total number of brushes cached
    BRUSHENTRY  abe[TOTAL_BRUSH_COUNT]; // Keeps track of brush cache
    POINTL      ptlReRealize;           // Work area for 864/964 pattern
                                        //   hardware bug work-around

    BOOL        bRealizeTransparent;    // Hint to DrvRealizeBrush for whether
                                        //   the brush should be realized as
                                        //   transparent or not
    LONG        cAlignedBrushSize;      // Size of aligned brushes for 3D chips

    ////////// Text stuff:

    SURFOBJ*    psoText;                // 1bpp surface to which we will have
                                        //   GDI draw our glyphs for us

    /////////// DirectDraw stuff:

    FLIPRECORD  flipRecord;             // Used to track vertical blank status
    FLIPRECORD  flipRecord2;            // Used to track vertical blank status
                                        // on IGA2
    ULONG       ulRefreshRate;          // Refresh rate in Hz
    ULONG       ulRefreshRate2;         // Refresh rate in Hz for IGA2
    ULONG       ulMinOverlayStretch;    // Minimum stretch ratio for this mode,
                                        //   expressed as a multiple of 1000
    ULONG       ulFifoValue;            // Optimial FIFO value for this mode
    ULONG       ulExtendedSystemControl3Register_69;
                                        // Masked original contents of
                                        //   S3 register 0x69, in high byte
    ULONG       ulMiscState;            // Default state of the MULT_MISC
                                        //   register
    OH*         pohDirectDraw;          // Off-screen heap allocation for use
                                        //   by DirectDraw
    OH*         pohVideoEngineScratch;  // Location of one entire scan line that
                                        //   can be used for temporary memory
                                        //   by the 868/968 pixel formatter
    BYTE        jSavedCR2;              // Saved contents of register CR2
    FLATPTR     fpVisibleOverlay;       // Frame buffer offset to currently
                                        //   visible overlay; will be zero if
                                        //   no overlay is visible

        FLATPTR     fpVisibleBuffer;        // Frame buffer offset to currently
                                        // visible buffer.  Will be zero if
                                        // showing front buffer, or non-zero if
                                        // showing a back buffer.

    DWORD       dwOverlayFlipOffset;    // Overlay flip offset
    DWORD       dwVEstep;               // 868 video engine step value
    DWORD       wChipId;                // Device ID

#if _WIN32_WINNT >= 0x0500
    /////////// VPE stuff:

    ULONG_PTR    lpMMReg;                // Same as ppdev->pjMmBase, used for
                                        //   staying source compatible with
                                        //   Win95
    DDHALINFO   ddHALInfo;              // Our HALINFO structure
    LPB         LPBData;                // A bunch of S3 videoport hardware
                                        //   state
    DWORD       AutoflipOffsets[MAX_AUTOFLIP]; // Frame buffer offsets of surfaces being autoflipped
    DWORD       dwNumAutoflip;          // Number of surfaces being autoflipped
#endif

    //
    //  expansion factors when desktop is expanded to Panel size
    //

    DWORD       XExpansion;             // X or width expansion factor
    DWORD       YExpansion;             // Y or height expansion factor

    //
    //  if centering/expansion/panning is enabled, the start of the displayed
    //  desktop has an offset into the panels display
    //

    DWORD       DisplayXOffset;         // X offset for Expand/Center
    DWORD       DisplayYOffset;         // Y offset
    DWORD       ApertureXOffset;        // Panning Aperture X offset
    DWORD       ApertureYOffset;        //                  Y offset
    DWORD       ApertureIga1;           // Panning Linear Aperture offset, Iga1
    DWORD       ApertureIga2;           // Panning Linear Aperture offset, Iga2
    DWORD       SSBaseStartAddr;        // Sec stream base start address
    DWORD       SSsrcWidth;             // Sec stream source width
    DWORD       SSsrcHeight;            // Sec stream source height
    DWORD       SSdstWidth;             // Sec stream dest width
    DWORD       SSdstHeight;            // Sec stream dest height
    DWORD       SSdisplayWidth;         // Sec stream actual width on display
    DWORD       SSdisplayHeight;        //                   heigth on display
    DWORD       SSStride;               // Sec stream stride
    DWORD       SSbytesPerPixel;        // Sec stream bytes per pixel
    DWORD       SSbitsPerPixel;         // Sec stream bits per pixel
    DWORD       SSX;                    // Sec stream current X position
    DWORD       SSY;                    // Sec stream current Y position
    DWORD       SSClipped;              // Sec stream is clipped

     //////////  KV2 - color adjustment on streams processor

    ULONG       ulCanAdjustColor;       // Bit 0 - KV2;
                                        // Bit 1 - going to decode YUV
     //////////  Single Binary/Source Code support

    ULONG       iUniqueness;            // display uniqueness for tracking
                                        //   resolution changes
#if SUPPORT_MCD
    ////////// OpenGL MCD stuff:

    LONG        cDoubleBufferRef;       // Reference count for current number
                                        //   of RC's that have active double-
                                        //   buffers
    OH*         pohBackBuffer;          // Our 2-d heap allocation structure
                                        //   for the back-buffer
    LONG        cZBufferRef;            // Reference count for current number
                                        //   of RC's that have active z-buffers
                                        //   (which, on Athenta, is all RC's)
    OH*         pohZBuffer;             // Our 2-d heap allocation structure
                                        //   for the z-buffer

    //  Texture buffers related data
    struct _DEVTEXTURE* pListDevTex;    // A pointer to the list of device
                                        //   textures being handled by the
                                        //   physical device

    ULONG       lastRenderedTexKey;     // Contains a reference to the texture,
                                        //   if any, used in the last hardware
                                        //   primitive rendering. This helps us
                                        //   to avoid corrupting the texture
                                        //   if still in use by the hw.

    HANDLE      hMCD;                   // Handle to MCD engine dll

    MCDENGESCFILTERFUNC pMCDFilterFunc; // MCD engine filter function

    //
    // OpenGL MCD stuff: added to support Virge DX/GX, Virge/MX and GX-2.
    //
    ULONG       ulChipID;               // Chip ID.
    MCDFLOAT    uvMaxTexels;            // maximun delta U and V allowed along
                                        // each triangle edge.
    VOID        *pOldDevRC;             // old pointer to DEVRC.

    //
    // Function prototype for various MMIO or DMA worker routines.
    //
    VOID (FASTCALL *mcdFillSubTriangle)(VOID *pRc, MCDVERTEX *c, MCDVERTEX *b, MCDVERTEX *a, BOOL bCCW, BOOL bTrust);
    VOID (FASTCALL *mcdHWLine)(VOID *pRc, MCDVERTEX *a, MCDVERTEX *b, BOOL resetLine);
    VOID (FASTCALL *hwLineSetupClipRect)(VOID *pRc, RECTL *pRect);
    VOID (FASTCALL *hwTriSetupClipRect)(VOID *pRc, RECTL *pRect);

    //
    //  OpenGL MCD stuff: added to support command DMA.
    //
    PUCHAR      pjDmaBuffer;            // Pointer to DMA buffer.
    ULONG       ulDmaIndex;             // DMA Buffer index.
    ULONG       ulDmaFreeSlots;         // number of bytes left in DMA buffer

    ULONG       ulZBufferStride;
    ULONG       ulBackBufferStride;
#endif

#ifdef P5TIMER

    DWORDLONG    Timer;                 // unsigned 64-bit Timer Counter.

#endif

    BAND        Band;

    BYTE        bCR36;                  // store CR36 value for streams
    ULONG       ulDefaultFIFO;
    ULONG       ulDefaultSPFifo;
    ULONG       ulDefaultStreamsTimeout;
    ULONG       ulDefaultMiscTimeout;
    ULONG       ulDefaultMIUControl;

    FNSTREAMS   *pfnTurnOnStreams;
    FNSTREAMS   *pfnTurnOffStreams;

#if _WIN32_WINNT >= 0x0500
    // D3D stuff

    D3DGLOBALS  D3DGlobals;             // chip specific to 3D
    WORD        wDeviceId;              // quick fix
#if DX6
    //pointer to vertex buffer unknown command processing function
    PFND3DPARSEUNKNOWNCOMMAND pD3DParseUnknownCommand;
#endif

    // Added for GetAvailVideoMemory callback in DDraw
    ULONG       ulTotalAvailVideoMemory;
#endif


    BYTE        bDefaultSTNRdTimeOut;
    BYTE        bDefaultSTNWrTimeOut;
    BYTE        bDefaultSTNRdThreshold;
    BYTE        bDefaultSTNWrThreshold;
    BYTE        bDefaultSSTimeOut;
    BYTE        bDefaultSSThreshold;
    BYTE        bDefaultPS1TimeOut;
    BYTE        bDefaultPS1Threshold;
    BYTE        bDefaultPS2TimeOut;
    BYTE        bDefaultPS2Threshold;

    //
    //  Stores values from BIOS Query (0x4f14, function 0)
    //  check with S3_*_SUPPORT in s3def.h for values
    //

    ULONG       ulDisplayDevicesSupported;

    //
    //  TV 3-tap flicker filter (tested for interaction with vt interpolation)
    //
    BOOL        f3TapFlickerFilterEnabled;


    BOOL        b3DDMAHwUsed;           // Helps us avoid resetting state if no #d
                                        // drawing was done
    BOOL        fTriangleFix;           // 3D Triangle workaround

    PVOID       pUnalignedPDEV;

    BOOL        bAllocFromDD;           // pohAllocate is calling from DirectDraw.

} PDEV, *PPDEV;

#endif
/////////////////////////////////////////////////////////////////////////
//  Miscellaneous prototypes:

BOOL    bIntersect(RECTL*, RECTL*, RECTL*);
LONG    cIntersect(RECTL*, RECTL*, LONG);
#ifndef MCD95
DWORD   getAvailableModes(HANDLE, PS3_VIDEO_MODE_INFORMATION*, DWORD*);
#endif
VOID    AcquireMiniPortCrtc(PDEV * ppdev);
VOID    ReleaseMiniPortCrtc(PDEV * ppdev);

BOOL    bInitializeModeFields(PDEV*, GDIINFO*, DEVINFO*, DEVMODEW*);

//#ifndef MCD95
//BOOL bFastFill(PDEV*, LONG, POINTFIX*, ULONG, ULONG, RBRUSH*, POINTL*, RECTL*);
//#endif

BOOL    bEnableHardware(PDEV*);
VOID    vDisableHardware(PDEV*);
BOOL    bAssertModeHardware(PDEV*, BOOL);

extern  BYTE gajHwMixFromMix[];
extern  BYTE gaRop3FromMix[];
extern  ULONG gaulHwMixFromRop2[];

#if _WIN32_WINNT >= 0x0500
DWORD LPB_DriverInit(PPDEV ppdev);
#endif

/////////////////////////////////////////////////////////////////////////
//  The x86 C compiler insists on making a divide and modulus operation
//  into two DIVs, when it can in fact be done in one.  So we use this
//  macro.
//
//  Note: QUOTIENT_REMAINDER implicitly takes unsigned arguments.

#if defined(_X86_)

#define QUOTIENT_REMAINDER(ulNumerator, ulDenominator, ulQuotient, ulRemainder) \
{                                                               \
    __asm mov eax, ulNumerator                                  \
    __asm sub edx, edx                                          \
    __asm div ulDenominator                                     \
    __asm mov ulQuotient, eax                                   \
    __asm mov ulRemainder, edx                                  \
}

#else

#define QUOTIENT_REMAINDER(ulNumerator, ulDenominator, ulQuotient, ulRemainder) \
{                                                               \
    ulQuotient  = (ULONG) ulNumerator / (ULONG) ulDenominator;  \
    ulRemainder = (ULONG) ulNumerator % (ULONG) ulDenominator;  \
}

#endif

/////////////////////////////////////////////////////////////////////////
// OVERLAP - Returns TRUE if the same-size lower-right exclusive
//           rectangles defined by 'pptl' and 'prcl' overlap:

#define OVERLAP(prcl, pptl)                                             \
    (((prcl)->right  > (pptl)->x)                                   &&  \
     ((prcl)->bottom > (pptl)->y)                                   &&  \
     ((prcl)->left   < ((pptl)->x + (prcl)->right - (prcl)->left))  &&  \
     ((prcl)->top    < ((pptl)->y + (prcl)->bottom - (prcl)->top)))

/////////////////////////////////////////////////////////////////////////
// SWAP - Swaps the value of two variables, using a temporary variable

#define SWAP(a, b, tmp) { (tmp) = (a); (a) = (b); (b) = (tmp); }

//////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////
// CONVERT_TO_BYTES - Converts to byte count.

#define CONVERT_TO_BYTES(x, pdev)   ( (x) * pdev->cjPelSize)

//////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////
// CONVERT_FROM_BYTES - Converts to byte count.

#define CONVERT_FROM_BYTES(x, pdev) ( (x) / pdev->cjPelSize)

//////////////////////////////////////////////////////////////////////

// These Mul prototypes are thunks for multi-board support:

ULONG   MulGetModes(HANDLE, ULONG, DEVMODEW*);
DHPDEV  MulEnablePDEV(DEVMODEW*, PWSTR, ULONG, HSURF*, ULONG, ULONG*,
                      ULONG, DEVINFO*, HDEV, PWSTR, HANDLE);
VOID    MulCompletePDEV(DHPDEV, HDEV);
HSURF   MulEnableSurface(DHPDEV);
BOOL    MulStrokePath(SURFOBJ*, PATHOBJ*, CLIPOBJ*, XFORMOBJ*, BRUSHOBJ*,
                      POINTL*, LINEATTRS*, MIX);
BOOL    MulFillPath(SURFOBJ*, PATHOBJ*, CLIPOBJ*, BRUSHOBJ*, POINTL*,
                    MIX, FLONG);
BOOL    MulBitBlt(SURFOBJ*, SURFOBJ*, SURFOBJ*, CLIPOBJ*, XLATEOBJ*,
                  RECTL*, POINTL*, POINTL*, BRUSHOBJ*, POINTL*, ROP4);
VOID    MulDisablePDEV(DHPDEV);
VOID    MulDisableSurface(DHPDEV);
BOOL    MulAssertMode(DHPDEV, BOOL);
VOID    MulMovePointer(SURFOBJ*, LONG, LONG, RECTL*);
ULONG   MulSetPointerShape(SURFOBJ*, SURFOBJ*, SURFOBJ*, XLATEOBJ*, LONG,
                           LONG, LONG, LONG, RECTL*, FLONG);
ULONG   MulDitherColor(DHPDEV, ULONG, ULONG, ULONG*);
BOOL    MulSetPalette(DHPDEV, PALOBJ*, FLONG, ULONG, ULONG);
BOOL    MulCopyBits(SURFOBJ*, SURFOBJ*, CLIPOBJ*, XLATEOBJ*, RECTL*, POINTL*);
BOOL    MulTextOut(SURFOBJ*, STROBJ*, FONTOBJ*, CLIPOBJ*, RECTL*, RECTL*,
                   BRUSHOBJ*, BRUSHOBJ*, POINTL*, MIX);
VOID    MulDestroyFont(FONTOBJ*);
BOOL    MulPaint(SURFOBJ*, CLIPOBJ*, BRUSHOBJ*, POINTL*, MIX);
BOOL    MulRealizeBrush(BRUSHOBJ*, SURFOBJ*, SURFOBJ*, SURFOBJ*, XLATEOBJ*,
                        ULONG);
HBITMAP MulCreateDeviceBitmap(DHPDEV, SIZEL, ULONG);
VOID    MulDeleteDeviceBitmap(DHSURF);
BOOL    MulStretchBlt(SURFOBJ*, SURFOBJ*, SURFOBJ*, CLIPOBJ*, XLATEOBJ*,
                      COLORADJUSTMENT*, POINTL*, RECTL*, RECTL*, POINTL*,
                      ULONG);

// These Dbg prototypes are thunks for debugging:

ULONG   DbgGetModes(HANDLE, ULONG, DEVMODEW*);
DHPDEV  DbgEnablePDEV(DEVMODEW*, PWSTR, ULONG, HSURF*, ULONG, ULONG*,
                      ULONG, DEVINFO*, HDEV, PWSTR, HANDLE);
VOID    DbgCompletePDEV(DHPDEV, HDEV);
HSURF   DbgEnableSurface(DHPDEV);
BOOL    DbgLineTo(SURFOBJ*, CLIPOBJ*, BRUSHOBJ*, LONG, LONG, LONG, LONG,
                  RECTL*, MIX);
BOOL    DbgStrokePath(SURFOBJ*, PATHOBJ*, CLIPOBJ*, XFORMOBJ*, BRUSHOBJ*,
                      POINTL*, LINEATTRS*, MIX);
BOOL    DbgFillPath(SURFOBJ*, PATHOBJ*, CLIPOBJ*, BRUSHOBJ*, POINTL*,
                    MIX, FLONG);
BOOL    DbgBitBlt(SURFOBJ*, SURFOBJ*, SURFOBJ*, CLIPOBJ*, XLATEOBJ*,
                  RECTL*, POINTL*, POINTL*, BRUSHOBJ*, POINTL*, ROP4);
VOID    DbgDisablePDEV(DHPDEV);
VOID    DbgDisableSurface(DHPDEV);
BOOL    DbgAssertMode(DHPDEV, BOOL);
VOID    DbgMovePointer(SURFOBJ*, LONG, LONG, RECTL*);
ULONG   DbgSetPointerShape(SURFOBJ*, SURFOBJ*, SURFOBJ*, XLATEOBJ*, LONG,
                           LONG, LONG, LONG, RECTL*, FLONG);
ULONG   DbgDitherColor(DHPDEV, ULONG, ULONG, ULONG*);
BOOL    DbgSetPalette(DHPDEV, PALOBJ*, FLONG, ULONG, ULONG);
BOOL    DbgCopyBits(SURFOBJ*, SURFOBJ*, CLIPOBJ*, XLATEOBJ*, RECTL*, POINTL*);
BOOL    DbgTextOut(SURFOBJ*, STROBJ*, FONTOBJ*, CLIPOBJ*, RECTL*, RECTL*,
                   BRUSHOBJ*, BRUSHOBJ*, POINTL*, MIX);
VOID    DbgDestroyFont(FONTOBJ*);
BOOL    DbgPaint(SURFOBJ*, CLIPOBJ*, BRUSHOBJ*, POINTL*, MIX);
BOOL    DbgRealizeBrush(BRUSHOBJ*, SURFOBJ*, SURFOBJ*, SURFOBJ*, XLATEOBJ*,
                        ULONG);
HBITMAP DbgCreateDeviceBitmap(DHPDEV, SIZEL, ULONG);
VOID    DbgDeleteDeviceBitmap(DHSURF);
BOOL    DbgStretchBlt(SURFOBJ*, SURFOBJ*, SURFOBJ*, CLIPOBJ*, XLATEOBJ*,
                      COLORADJUSTMENT*, POINTL*, RECTL*, RECTL*, POINTL*,
                      ULONG);
ULONG   DbgEscape(SURFOBJ*, ULONG, ULONG, VOID*, ULONG, VOID*);

BOOL    DbgResetPDEV(DHPDEV, DHPDEV);

#ifndef MCD95
BOOL    DbgGetDirectDrawInfo(DHPDEV, DD_HALINFO*, DWORD*, VIDEOMEMORY*,
                             DWORD*, DWORD*);
BOOL    DbgEnableDirectDraw(DHPDEV, DD_CALLBACKS*, DD_SURFACECALLBACKS*,
                            DD_PALETTECALLBACKS*);
VOID    DbgDisableDirectDraw(DHPDEV);
#endif
ULONG   DbgEscape(SURFOBJ*, ULONG, ULONG, VOID*, ULONG, VOID*);

//  TRACK mode and resolution changes
ULONG GetDisplayUniqueness(PDEV*);

typedef struct _DDRAWDATA
{
    OH     *poh;
    DWORD   dwValue;
} DDRAWDATA;

typedef DDRAWDATA * PDDRAWDATA;


typedef enum {
    SS_UPDATE_PANNING = 1,
    SS_UPDATE_CENTER_EXPAND,
    SS_UPDATE_IGA,
    SS_UPDATE_DISPLAY_CONTROL
} SS_UPDATE_FLAGS;

VOID ddClipStreamsToViewport (  PDEV*, DWORD*, DWORD*,
                                DWORD*, DWORD*,
                                DWORD*, DWORD* );

VOID ddSetStreamsFifo ( PDEV*, BOOL );

VOID ddUpdateSecondaryStreams ( PDEV*           ppdev,
                                SS_UPDATE_FLAGS UpdateFlag );

VOID ddCalcOpaqueOverlay (PDEV*, DWORD*, DWORD, DWORD );

//
//  Routines for M5 rev IB workaround for not being able to read
//  CRTC index when paired for IGA2 (Sr26 == 4f).
//
VOID vWriteCrIndexWithShadow ( PDEV *ppdev, BYTE bIndex );
BYTE bReadCrDataWithShadow ( PDEV *ppdev, BYTE bIndex );


//
// Registry Access defines
//

#define MCD_BATCH_KEYNAME   L"MCDBatchSize"
#define COMMAND_DMA_KEYNAME L"CommandDMA"

#define VSYNC_TIMEOUT_VALUE 0xFFFFFF

#define OVERLAY_FLG_ENABLED           (DWORD)0x00000002

VOID AssertModeDirect3D(PDEV *ppdev, BOOL bEnable);
