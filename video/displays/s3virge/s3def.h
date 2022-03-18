//******************************Module*Header********************************
//*  Module Name: s3def.h
//*
//*  This file contains S3 type defintions that are common between miniport
//*  and display driver, and the constants that are related to the type
//*  definitions.
//*
//*  Copyright (c) Microsoft 1998, All Rights Reserved
//*
//***************************************************************************


typedef struct _VIDEO_QUERY_STREAMS_MODE {
    ULONG   ScreenWidth;
    ULONG   BitsPerPel;
    ULONG   RefreshRate;
    PVOID   pBand;
} VIDEO_QUERY_STREAMS_MODE;

typedef struct _S3_VIDEO_PAN_INFO {
    ULONG   StreamsIGA_ApertureX;
    ULONG   StreamsIGA_ApertureY;
    ULONG   IGA1_ApertureOffset;
    ULONG   IGA2_ApertureOffset;
} S3_VIDEO_PAN_INFO, *PS3_VIDEO_PAN_INFO;

typedef struct _S3_POINTER_POSITION {
    ULONG   Column;
    ULONG   Row;
    ULONG   ApertureX;
    ULONG   ApertureY;
    ULONG   PanMemOffsetIga1;
    ULONG   PanMemOffsetIga2;
} S3_POINTER_POSITION, *PS3_POINTER_POSITION;

#define BANDWIDTH_INIT          0
#define BANDWIDTH_SETPRIMARY    1
#define BANDWIDTH_SETSECONDARY  2


typedef struct _BANDINF
{
    short           Option;
    short           bpp;
    short           dclk;
    short           dclksDuringBlank;
    unsigned short  device;
    unsigned char   fam;
    unsigned char   fInterlaced;
    unsigned short  id;
    short           mclk;
    short           mType;
    short           mWidth;
    short           xMax;
    short           xRes;
    short           xSrc;
    short           xTrg;
    short           yMax;
    short           yRes;
    short           ySrc;
    short           yTrg;
    short           RefreshRate;
    PVOID           pBand;
} BANDINF, *PBANDINF;


typedef struct _VIDEO_DMA_BUFFER {
    ULONG   ulChipID;           // PCI Device ID read from CR2D | CR2E.
    PUCHAR  pjDmaBuffer;        // Pointer to DMA buffer allocated.
    ULONG   ulDmaPhysical;      // Physical address to DMA buffer.
} VIDEO_DMA_BUFFER, *PVIDEO_DMA_BUFFER;

// used by IOCTL_VIDEO_S3_MBL_GET_MOBILE_INTERFACE
typedef struct _MOBILE_INTERFACE {
    ULONG   ulMfg;             
    ULONG   ulSubType;    
} MOBILE_INTERFACE, *PMOBILE_INTERFACE;

typedef enum _MOBILE_MFG {
    GENERIC         = 0x00,
    M5_COMPAQ,
    M5_TOSHIBA
} MOBILE_MFG;

//
//  Get/Set Panel Centering/Expand defines
//
typedef enum _MBL_CTREXP    {
    MBL_CTREXP_NONE         = 0x00,
    MBL_CTREXP_CENTER       = 0x01,
    MBL_CTREXP_EXPAND       = 0x02
} MBL_CTREXP, *PMBL_CTREXP;
#define MBL_CTREXP_VALID_MASK  0x03     // mask for valid bits in CTREXP

// used by IOCTL_VIDEO_S3_MBL_GET/SET_DISPLAY_CONTROL
typedef struct _VIDEO_DISPLAY_CONTROL {
    ULONG   ActiveDisplayReq;
    ULONG   TvTechnologyReq;
} VIDEO_DISPLAY_CONTROL, *PVIDEO_DISPLAY_CONTROL;

// used by IOCTL_VIDEO_S3_MBL_GET/SET_TV_FFILTER_STATUS
typedef struct _TV_FLICKER_FILTER_CONTROL {
    ULONG   TvFFilterOnOffState;
    ULONG   TvFFilterSpecificSetting;
} TV_FLICKER_FILTER_CONTROL, *PTV_FLICKER_FILTER_CONTROL;

// used by IOCTL_VIDEO_S3_MBL_GET/SET_TV_POSITION
typedef struct _TV_POSITION_CONTROL {
    ULONG   TvPositionXRange;
    ULONG   TvPositionXPos;
    ULONG   TvPositionYRange;
    ULONG   TvPositionYPos;
} TV_POSITION_CONTROL, *PTV_POSITION_CONTROL;

// used by IOCTL_VIDEO_S3_MBL_GET/SET_TV_UNDERSCAN_STATUS
typedef enum _TV_VIEWMETHOD {
    TV_VIEWMETHOD_PAN       = 0x00,
    TV_VIEWMETHOD_UNDERSCAN = 0x01,
    TV_VIEWMETHOD_OVERSCAN  = 0x02
} TV_VIEWMETHOD;
#define TV_VIEWMETHOD_VALID 0x03  // mask for valid bits in TV_VIEWMETHOD

// used by IOCTL_VIDEO_S3_MBL_GET_BIOS_VERSION
//
// The MBL_BIOS_VERSION_INFO structure defines the
// format into which the miniport's IOCTL return information
// from the BIOS (4F14 fn 00 EBX dword of data) must be put,
// before returning it to the caller.
//
typedef struct _MBL_BIOS_VERSION_INFO {
    USHORT Bios_AA;         // BIOS [30:24]
    USHORT Bios_BB;         // BIOS [23:16]
    USHORT Bios_CC;         // BIOS [15: 8]
    USHORT Bios_DD;         // BIOS [ 7: 0]
    USHORT Bios_type;       // BIOS [31]
    USHORT Bios_reserved;   // 0xFFFF
} MBL_BIOS_VERSION_INFO;

//
// used by IOCTL_VIDEO_S3_MBL_GET_CLOCKS
//
typedef struct _CLOCK_INFO {
    ULONG   MemoryClock;
    ULONG   DClock1;
    ULONG   DClock2;
} CLOCK_INFO, *PCLOCK_INFO;

//
// Structure defined by S3SWITCH.CPQ app/driver
// interface, and used by
// ioctl    IOCTL_VIDEO_S3_MBL_QUERY_MOBILE_SUPPORT and
// escape   S3ESC_MBL_QUERY_DEVICE_SUPPORT
//
typedef struct _DISPLAY_SUPPORT {
    ULONG   SupportedDisplayTypes;
    ULONG   SupportedTvStandards;
} DISPLAY_SUPPORT, *PDISPLAY_SUPPORT;

//
// used by IOCTL_VIDEO_S3_MBL_GET_CRT_REFRESH_RATES
// escape call S3ESC_MBL_GET_CRT_REFRESH_RATES

typedef struct _APP_MODE_INFO {
    ULONG   dwWidth;
    ULONG   dwHeight;
    ULONG   dwAppModeIndex;
    ULONG   dwColorDepth;
    ULONG   dwRefresh;
} APP_MODE_INFO, *PAPP_MODE_INFO;


//
//  Structure defined by S3SWITCH.CPQ app/driver
//  interface, and used by
//  ioctl   IOCTL_VIDEO_S3_MBL_GET_CRT_RES_TABLE,
//  escape  S3ESC_MBL_GET_CRT_PAN_RES_TABLE:
//
typedef struct _S3_APP_CRT_RES {
    ULONG   ulWidth;
    ULONG   ulHeight;
    ULONG   ulModeIndex;
} S3_APP_CRT_RES, *PS3_APP_CRT_RES;

//
//  Structure defined by S3SWITCH.CPQ app/driver
//  interface, and used by
//  ioctl   IOCTL_VIDEO_S3_MBL_GET_CRT_REFRESH_RATES,
//  escape  S3ESC_MBL_GET_CRT_REFRESH_RATES
//
typedef struct _S3_APP_CRT_MODEFREQ {
    S3_APP_CRT_RES  CrtResolution;
    ULONG           ulColorDepth;
    ULONG           ulRefresh;
} S3_APP_CRT_MODEFREQ, *PS3_APP_CRT_MODEFREQ;

//
//  S3Switch app / driver interface defines
//
#define IGA1                0
#define IGA2                1
#define LGDESKTOP_WIDE      0
#define LGDESKTOP_TALL      1
#define S3_HWOVERLAY_OFF    0xFFFFFFFF

//
//  For the "ShowAll" flag passed in some S3Switch
//  escape calls
//
#define S3_M5_MODE_PRUNING  0
#define S3_M5_MODE_SHOWALL  1

#if SUPPORT_MIF

//
//  Structure defined by S3 MIF file gerneration utility / driver interface.
//  Used by
//  ioctl   IOCTL_MIF_GETCOMPONENTINFO,
//  escape  S3ESC_MIF_GETCOMPONENTINFO
//

typedef struct _S3_MIF_ComponentInfo {
    ULONG       ChipID;
    UCHAR       DriverVersion[32];
    ULONG       OperatingSystem; // Designates the OS
    WCHAR       DriverName[260];
} S3_MIF_ComponentInfo, *PS3_MIF_ComponentInfo;

typedef struct _S3_MIF_Video_Info {
    ULONG       CurrentMode;
    ULONG       MinimumRefresh;
    ULONG       MaximumRefresh;
    ULONG       CurrentRefresh;
    ULONG       VideoMemType;
    ULONG       VideoMemSize;
    ULONG       ScanMode;
} S3_MIF_VideoInfo, *PS3_MIF_VideoInfo;

typedef struct _S3_MIF_BIOSInfo {
    ULONG       BIOSManufacturer;
    ULONG       BIOSVersion;
    ULONG       BIOSReleaseDate;
    ULONG       BIOSCharacteristics;
} S3_MIF_BIOSInfo, *PS3_MIF_BIOSInfo;


#define S3_CHIPID_UNKNOWN           0
#define S3_MIF_CHIPTYPE_GX2         1
#define S3_MIF_CHIPTYPE_MX          2
#define S3_MIF_CHIPTYPE_365         3

#define S3_CHIPID_365               0x365
#define S3_CHIPID_366               0x366

// S3MIF Escape functions
//
#define S3ESC_MIF_FUNCTION          0x3001

// S3 MIF Subfunctions

#define S3ESC_MIF_GETCOMPONENTINFO  0x0
#define S3ESC_MIF_GETVIDEOINFO      0x1
#define S3ESC_MIF_GETBIOSINFO       0x2
#define S3ESC_MIF_GETMONITORINFO    0x3

#define OS_WINNT                    0x1

#endif // SUPPORT_MIF

//
// Chip subtypes -- for more differentiation within families
//
// Note that ordering is important. Code in S3.C will sometimes apply logic
// to ranges of chip subtypes, so be careful about making any changes.
//

typedef enum _S3_SUBTYPE {
    SUBTYPE_775=0,      // Trio64V2 (775/DX, 785/GX)
    SUBTYPE_M65,        // Aurora64V+, M1, M65
    SUBTYPE_325,        // ViRGE
    SUBTYPE_988,        // ViRGE/VX
    SUBTYPE_375,        // ViRGE/DX
    SUBTYPE_240,        // MXC
    SUBTYPE_260,        // M5
    SUBTYPE_280,        // M5+ with Mv
    SUBTYPE_262,        // MXi
    SUBTYPE_282,        // MXi with Mv
    SUBTYPE_357,        // ViRGE/GX2
    SUBTYPE_358,        // ViRGE/GX2+ with Mv
    SUBTYPE_359,        // ViRGE/GX2+
    SUBTYPE_INVALID
} S3_CHIPSET;

typedef struct _S3_VIDEO_MODE_INFORMATION
{
    ULONG Length;
    ULONG ModeIndex;
    ULONG VisScreenWidth;
    ULONG VisScreenHeight;
    ULONG ScreenStride;
    ULONG NumberOfPlanes;
    ULONG BitsPerPlane;
    ULONG Frequency;
    ULONG XMillimeter;
    ULONG YMillimeter;
    ULONG NumberRedBits;
    ULONG NumberGreenBits;
    ULONG NumberBlueBits;
    ULONG RedMask;
    ULONG GreenMask;
    ULONG BlueMask;
    ULONG AttributeFlags;
    ULONG VideoMemoryBitmapWidth;
    ULONG VideoMemoryBitmapHeight;
    ULONG DriverSpecificAttributeFlags;
    ULONG DriverSpecificAttributeFlags2;
    ULONG DriverSpecificAttributeFlags3;
    ULONG DriverSpecificAttributeFlags4;
    ULONG DriverSpecificAttributeFlags5;
    ULONG DriverSpecificAttributeFlags6;
    ULONG DriverSpecificAttributeFlags7;
    ULONG DriverSpecificAttributeFlags8;
} S3_VIDEO_MODE_INFORMATION, * PS3_VIDEO_MODE_INFORMATION;

////////////////////////////////////////////////////////////////////////
//  Capabilities flags
//
//  These are private flags passed to the S3 display driver.  They're
//  put in the high word of the 'AttributeFlags' field of the
//  'VIDEO_MODE_INFORMATION' structure (found in 'ntddvdeo.h') passed
//  to the display driver via an 'VIDEO_QUERY_AVAIL_MODES' or
//  'VIDEO_QUERY_CURRENT_MODE' IOCTL.
//
//  NOTE: These definitions must match those in the S3 display driver's
//        'hw.inc'!
//

typedef enum {
    CAPS_MM_TRANSFER        = 0x00000001,   // Memory-mapped image transfers
    CAPS_MM_32BIT_TRANSFER  = 0x00000002,   // Can do 32bit bus size transfers
    CAPS_MM_IO              = 0x00000004,   // Memory-mapped I/O
    CAPS_NEW_MMIO           = 0x00000008,   // Can use 'new memory-mapped
                                            //   I/O' scheme introduced with
                                            //   868/968
    CAPS_HW_PATTERNS        = 0x00000010,   // 8x8 hardware pattern support
    CAPS_SW_POINTER         = 0x00000020,   // No hardware pointer; use   
    CAPS_RGB525_POINTER     = 0x00000040,   // Use RGB525 pointer
    CAPS_BT485_POINTER      = 0x00000080,   // Use Brooktree 485 pointer
    CAPS_TI025_POINTER      = 0x00000100,   // Use TI TVP3020/3025 pointer
    CAPS_16_ENTRY_FIFO      = 0x00000200,   // At least 16 entries in FIFO
    CAPS_MM_GLYPH_EXPAND    = 0x00000400,   // Use memory-mapped I/O glyph-
                                            //   expand method of drawing text.
    CAPS_NEWER_BANK_CONTROL = 0x00000800,   // Set if 864/964 style banking
    CAPS_NO_DIRECT_ACCESS   = 0x00001000,   // Frame buffer can't be directly
                                            //   accessed by GDI or DCI,
                                            //   because dword or word reads
                                            //   would crash system, or Alpha
                                            //   is running in sparse space.
    CAPS_WAIT_ON_PALETTE    = 0x00002000,   // Wait for vertical retrace before
                                            //   setting the palette registers.
    CAPS_PANNING            = 0x00004000,   // screen panning enabled
    CAPS_CURSOR_Y0BUG       = 0x00008000,   // M65 hw cursor bug: if CR48
                                            //   & CR49 are programmed w/ 0,
                                            //   the cursor disappears.
    CAPS_S3D_ENGINE         = 0x00010000,   // ViRGE 3D graphics engine
    CAPS_LINEAR_FRAME_BUFFER= 0x00020000,   // Linear Frame Buffer
    CAPS_STREAMS_CAPABLE    = 0x00040000,   // Has S3 overlay streams processor
    CAPS_SGRAM_PROBLEMS     = 0x00080000,    // Need fixes for SGRAM problems
    CAPS_BAD_DWORD_READS    = 0x00100000,   // Dword or word reads from the
                                            //   frame buffer will occasionally
                                            //   return an incorrect result.
    CAPS_FORCE_DWORD_REREADS= 0x00200000,   // Handle irregular dword reads
    CAPS_RE_REALIZE_PATTERN = 0x00400000,   // Set if we must work around the
                                            //   864/964 hardware pattern bug.
    CAPS_SCALE_POINTER      = 0x00800000,   // Set if the S3 hardware pointer
                                            //   x position has to be scaled
                                            //   by two.
    CAPS_PIXEL_FORMATTER    = 0x01000000,   // Can do colour space conversions,
                                            //   and one-dimensional hardware
                                            //   stretches.
#if defined(_ALPHA_)
    CAPS_SPARSE_SPACE       = 0x02000000,   // Frame buffer is mapped in sparse
                                            //   space on the Alpha.
#endif
} CAPS;

typedef enum
{
    CAPS2_GX2_STREAMS_COMPATIBLE = 0x00000001,  // ViRGE/GX2 streams processor
                                                // compatible
    CAPS2_LCD_SUPPORT            = 0x00000002,  // flat panel (LCD) support
    CAPS2_TV_SUPPORT             = 0x00000004,  // Television support
    CAPS2_COLOR_ADJUST_ALLOWED   = 0x00000008,  // Color Adjust hardware
                                                // present
    CAPS2_HW_CURSOR_WKAROUND     = 0x00000010,  // Workaound HW cursor
                                                // waiting for V-BLANK
    CAPS2_DISABLE_POWER_DOWN_CTRL= 0x00000020,  // workaround slowness on M5
    CAPS2_TV_UNDERSCAN_CAPABLE   = 0x00000040,  // Can do TV underscanning
    CAPS2_24BPP_NEEDS_STREAMS    = 0x00000080,  // 24bpp modes need streams
    CAPS2_READ_SCANLINE          = 0x00000100,  // can read current scanline
    CAPS2_M5_STREAMS_CURSOR_BUG  = 0x00000200,  // hw cursor and streams don't
                                                // work properly with NOT masks
    CAPS2_HW_3DALPHA_FIX         = 0x00000400,  // GX2, M5
    CAPS2_IGA2_SR26_READS_BROKEN = 0x00000800,  // can't read SR26 if= 0x4E
    CAPS2_IGA2_3D4_READS_BROKEN  = 0x00001000,  // can't read 3D4 if SR26=4E
    CAPS2_DDC_ENABLED            = 0x00002000,  // DDC enabled, EDID retrieved
    CAPS2_DONT_ACCESS_8514_REG   = 0x00004000,  // Avoid access to 8514 registers.
    CAPS2_VERT_COUNTER_WORKAROUND =0x00008000,  //
    CAPS2_32BPP_DFB_MODE          =0x00010000   // 32bpp dump frame buffer mode for GXLT.
} CAPS2;

#define CAPS_DAC_POINTER    (CAPS_BT485_POINTER | CAPS_TI025_POINTER | \
                             CAPS_RGB525_POINTER)

//
// Shift adjustment to place a bit in the CAPS_STREAMS_CAPABLE position
//
#define DEV_CAPS_STREAMS_CAPABLE_SHIFT 30
//
// These are shifted ( >> 16 ) values for BIOS Query Function 0
//

#define S3_CRT_SUPPORT      0x01        // CRT Display Supported
#define S3_LCD_SUPPORT      0x02        // LCD (panel) Display supported
#define S3_NTSCTV_SUPPORT   0x04        // NTSC TV Display supported
#define S3_PALTV_SUPPORT    0x08        // PAL TV Display supported
#define S3_CRTLCD_MASK      0x03        // used in DrvEscape to mask out the
                                        // TV Support
//
//  These definitions are for the S3SWITCH utility specification.
//  (note that S3_CRT_SUPPORT and S3_LCD_SUPPORT from the BIOS spec may
//  be reused, but the TV bits do not correspond).
//
//  Display device support (app format)
#define S3_TV_SUPPORT       0x04        // used in DrvEscape to return to
                                        // s3switch that we support TV

//  TV Standard bit masks (app format)
#define S3_TVSTD_SUPPORT_NTSC   0x01
#define S3_TVSTD_SUPPORT_PAL    0x02
#define S3_TVSTD_SUPPORT_NTSCJ  0x04
#define S3_TVSTD_SUPPORT_PAL_M  0x08
#define S3_TVSTD_VALID_MASK     0x0F

//
//      4F14 fn3,sub0  Set Active Display Device
//      4F14 fn3,sub1  Get Active Display Device
//
//
//      Active Display Device Byte (used by SET/GET ACTIVE DISPLAY calls)
//
#define S3_ACTIVE_CRT       0x01    // Bit[0]    CRT
#define S3_ACTIVE_LCD       0x02    // Bit[1]    LCD
#define S3_ACTIVE_TV        0x04    // Bit[2]    TV
#define S3_ACTIVE_RESERVED  0x08    // Bit[3]    Reserved for sys/video BIOS
#define S3_ACTIVE_HKEY_SEQ  0x30    // Bits[5:4] Hot-Key sequence control
#define S3_DISP_CONFIG_CHG  0x40    // Bit[6]    Display Configuration Change
#define S3_ACTIVE_DUOVIEW   0x80    // Bit[7]    DuoView
#define S3_ACTIVE_DEVMASK   0x07    // selects active device bits only
#define S3_ACTIVE_CAPS_MASK 0x000000FF  // mask to select low byte of ECX
                                        // which holds Active Displays
                                        // information.

//
//  TV FF filter fraction settings
//
#define TV_FFILTER_STATE1   1
#define TV_FFILTER_STATE2   2
#define TV_FFILTER_STATE3   3
#define TV_FFILTER_STATE4   4


