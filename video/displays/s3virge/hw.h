/******************************Module*Header*******************************\
*   Module Name: hw.h
*
*   Hardware specific driver file stuff that is common to ALL chips.
*   Some of these definitions are mirrored in HW.INC.
*
*   Those register definitions etc. that are specific to a chip are defined
*   in HWTR.H (for Trio and pre-Trio family chips) and HW3D.H (for ViRGE
*   family chips).
*
* Copyright (c) 1992-1996 Microsoft Corporation
*
\**************************************************************************/


////////////////////////////////////////////////////////////////////////
//  Register definitions

//
//  VGA registers
//
#define STATUS_1                    0x03DA
#define DISPLAY_MODE_INACTIVE       0x01   // bit 0 set = not in disply md
#define VBLANK_ACTIVE               0x08   // bit 3 set = dsply is in vt retrace

//
//  CRT Controller Registers
//
#define CRTC_INDEX                  0x03D4
#define CRTC_DATA                   0x03D5
#define DISPLAY_START_HI            0x0C
#define DISPLAY_START_LO            0x0D

//
// The following are CRT registers
//
#define HTOTAL_REG                  0x00
#define VTOTAL_REG                  0x06
#define CRTC_OVERFLOW_REG           0x07

//
// Sequencer registers
//

#define SEQ_INDEX                   0x03C4
#define SEQ_DATA                    0x03C5

#define SEQ_REG_UNLOCK              0x0608  // unlock the sequencer registers
#define SEQ_REG_LOCK                0x0008  // lock the sequencer registers

//
// M5/GX2 Sequencer registers
//
#define PAIRED_REGISTER_SELECT      0x26
//
// Note that SR26[6] is defined, despite databook's listing it as
// "reserved".  We need to always leave this bit set, so if we are
// not doing a Read-Modify-Write of SR26, then we must at least
// ensure that SR[6] is SET.
//
#define SELECT_IGA1                 0x4026
#define SELECT_IGA2_READS_WRITES    0x4f26
#define SELECT_IGA1_RW_BYTE         0x40
#define SELECT_IGA2_RW_BYTE         0x4F

#define ARCH_CONFIG_REG             0x30
#define STREAMS_IGA_SELECT_BIT      0x02
#define STREAMS_ON_IGA1             0
#define STREAMS_ON_IGA2             0x02
#define DCLK2_VALUE_LOW_REG         0x0E
#define DCLK2_VALUE_HIGH_REG        0x0F
#define DCLK_PLL_OVERFLOW           0x29
#define DCLK_VALUE_LOW_REG          0x12
#define DCLK_VALUE_HIGH_REG         0x13
#define DCLK1_PLL_R_VALUE_BIT2      0x01
#define DCLK2_PLL_N_VALUE_BIT6      0x10
#define DCLK2_PLL_M_VALUE_BIT8      0x80
#define DCLK2_PLL_R_VALUE_BIT2      0x04
#define IGA2_3D4_SHADOW_SEQREG      0x67

#define EXT_VERTICAL_COUNTER_REGISTER   0x3E


//
//  Extended CRT Controller Registers
//

// CTR26 Synchronization 2 reg used for vertical interpolation synch.
#define SYNCH_2                     0x26

#define MEM_CONFIG                  0x31
#define CPUA_BASE                   0x01

#define BACKWARD_COMPAT2_REG        0x33
#define VSYNC_ACTIVE                0x04    //CR33[2] set = in vt retrace

#define CRT_REG_LOCK                0x35

#define CONFIG_REG1_REG             0x36
#define FASTPAGE_MASK               0x0c

#define EXTREG_LOCK1                0x38
#define REG_UNLOCK_1                0x48

#define EXTREG_LOCK2                0x39
#define SYSCTL_UNLOCK               0xA0
#define SYSCTL_UNLOCK_EXTRA         0xA5

#define SYSCTL_LOCK                 0x00
#define MISC_1                      0x3A    // Miscellaneous 1

#define SYS_CONFIG                  0x40    // System Configuration register

#define EXT_MODE_CONTROL_REG        0x42
#define INTERLACED_MASK             0x20

#define HGC_MODE                    0x45    // HW graphics cursor mode
#define HGC_ENABLE                  0x001
#define HGC_DISABLE                 0x000

#define HGC_ORGX_LSB                0x47    // HW graphics cursor x-origin
#define HGC_ORGX_MSB                0x46
#define HGC_ORGY_LSB                0x49    // HW graphics cursor y-origin
#define HGC_ORGY_MSB                0x48

#define CR4C                        0x4C    // HW graphics cursor start addr
#define CR4D                        0x4D    //

#define HGC_DX                      0x4E    // HW graphics cursor pat start
#define HGC_DY                      0x4F

#define EXT_SYS_CTRL2               0x51    // Extended System Control 2

#define EXT_MEM_CTRL1               0x53    // Extended Memory Control 1
#define ENABLE_OLDMMIO              0x10    // Trio64-type MMIO
#define ENABLE_NEWMMIO              0x08    // new (relocatable) MMIO
#define ENABLE_BOTHMMIO             0x18

#define LAW_CTRL                    0x58    // Linear Address Window
#define LIN_ADDR_ENABLE             0x10

//
// The following register must be used only on older chips where the CAPS bit
// identifies an external DAC.  Newer chips use the register differently.
//
#define EX_DAC_CT                   0x55    // Extended RAMDAC Ctrl (older chips)
#define EXT_DAC_CTRL                0x55    // Extended RAMDAC Ctrl (older chips)

#define EXT_HORIZ_OVERFLOW_REG      0x5D

#define EXT_VERTICAL_OVERFLOW_REG   0x5E
#define VTOTAL_BIT10                0x01
#define HTOTAL_BIT8                 0x01

#define EXT_MISC_CTRL2              0x67
#define M5PLUS_DONT_WRITE_CTR26     0x01  // For M5+, CR67[0] isn't just 
                                          // "reserved".  This Undocumented 
                                          // bit for M5+/MXC fixes a HW 
                                          // problem described in SPR #18646
#define FULL_STREAMS                0x0C
#define NO_STREAMS                  0xF3

#define EXT_SYS_CTRL3               0x69
#define EXT_SYS_CTRL4               0x6A

#define EXT_DEVICE_ID_HIGH          0x2D
#define EXT_DEVICE_ID_LOW           0x2E

#define EXT_MISC_CONTROL_REG        0x87  // ViRGE/GX & DX
#define DROP_UV_CBCR                0x01  // drop uv/cbcr needs 81e8_14=1
#define KEEP_UV_CBCR                0xFE  // keep uv/cbcr (default)

#define EXT_SSFIFO_CTRL1_REG        0x92  // ViRGE/GX & DX ss fetch control reg1
#define ENABLE_L2_PARAM             0x80  // enable s stream fifo length control
#define EXT_SSFIFO_CTRL2_REG        0x93  // ViRGE/Gx & DX ss fetch control reg2


//
// ViRGE/GX2 streams registers
//

#define GX2_PS1_TIMEOUT                 0x7B
#define GX2_PS2_TIMEOUT                 0x7C
#define GX2_SS_TIMEOUT                  0x7D
#define GX2_STN_READ_TIMEOUT            0x7F
#define GX2_STN_WRITE_TIMEOUT           0x80
#define GX2_PS1_FIFO_THRESHOLD          0x85
#define GX2_PS2_FIFO_THRESHOLD          0x86
#define GX2_SS_FIFO_THRESHOLD           0x87
#define GX2_STN_READ_FIFO_THRESHOLD     0x89
#define GX2_STN_WRITE_FIFO_THRESHOLD    0x8A

//
//  SR5E - BIOS event notification register for M1
//
#define M1_EVENT_REG        0x5E
#define M1_EVREG_DISPSW     0x01    // display switch
#define M1_EVREG_POWER      0x02    // power notification
#define M1_EVREG_HOREXPAND  0x04    // horizontal expansion
#define M1_EVREG_VERTEXPAND 0x08    // vertical expansion
#define M1_EVREG_RESUME     0x10    // resume notification
#define M1_EVREG_EVENT_BITS 0x1F    // all event bits


//
//  CR50 - BIOS event notification register for M5
//
#define M5_EVENT_REG        0x50
#define M5_EVREG_DISPSW     0x01    // display switch
#define M5_EVREG_EXPAND     0x08    // display expansion
#define M5_EVREG_RESUME     0x20    // resume notification
#define M5_EVREG_EVENT_BITS 0x2F    // all event bits (used to test for
                                    // notification bits of interest)

#define EXT_BIOS_FLAG3      0x6B    // Related display info reg
#define M5_ALT_DISPSW       0x40    // Display switch notify bit 6B[6]
#define M5_DUOVIEW          0x80    // DuoView bit 6B[7]

//
// SR1E - Power Management 5 Register for M3/M5/GX2
//

#define POWER_MANAGEMENT_5_REG                      0x1E
#define SR1E_DISABLE_DCLK2_WHEN_IGA2_IS_DISABLED    0x04

// Command types:

#define DRAW_LINE                       0x02000
#define RECTANGLE_FILL                  0x04000
#define POLYGON_SOLID                   0x06000
#define FOUR_POINT_TRAPEZOID_SOLID      0x08000
#define BRESENHAM_TRAPEZOID_SOLID       0x0A000
#define BITBLT                          0x0C000
#define PATTERN_FILL                    0x0E000
#define POLYLINE                        0x02800
#define POLYGON_PATTERN                 0x06800
#define FOUR_POINT_TRAPEZOID_PATTERN    0x08800
#define BRESENHAM_TRAPEZOID_PATTERN     0x0A800
#define ROPBLT                          0x0C800

#define BYTE_SWAP                       0x01000
#define BUS_SIZE_NEW_32                 0x00600
#define BUS_SIZE_32                     0x00400
#define BUS_SIZE_16                     0x00200
#define BUS_SIZE_8                      0x00000
#define WAIT                            0x00100

// Drawing directions (radial):

#define DRAWING_DIRECTION_0             0x0000
#define DRAWING_DIRECTION_45            0x0020
#define DRAWING_DIRECTION_90            0x0040
#define DRAWING_DIRECTION_135           0x0060
#define DRAWING_DIRECTION_180           0x0080
#define DRAWING_DIRECTION_225           0x00A0
#define DRAWING_DIRECTION_270           0x00C0
#define DRAWING_DIRECTION_315           0x00E0

// Drawing directions (x/y):

#define DRAWING_DIR_BTRLXM              0x0000
#define DRAWING_DIR_BTLRXM              0x0020
#define DRAWING_DIR_BTRLYM              0x0040
#define DRAWING_DIR_BTLRYM              0x0060
#define DRAWING_DIR_TBRLXM              0x0080
#define DRAWING_DIR_TBLRXM              0x00A0
#define DRAWING_DIR_TBRLYM              0x00C0
#define DRAWING_DIR_TBLRYM              0x00E0

// Drawing direction bits:

#define PLUS_X                          0x0020
#define PLUS_Y                          0x0080
#define MAJOR_Y                         0x0040

// Draw:

#define DRAW                            0x0010

// Direction type:

#define DIR_TYPE_RADIAL                 0x0008
#define DIR_TYPE_XY                     0x0000

// Last pixel:

#define LAST_PIXEL_OFF                  0x0004
#define LAST_PIXEL_ON                   0x0000

// Pixel mode:

#define MULTIPLE_PIXELS                 0x0002
#define SINGLE_PIXEL                    0x0000

// Read/write:

#define READ                            0x0000
#define WRITE                           0x0001

// Graphics processor status:

#define HARDWARE_BUSY                   0x0200
#define READ_DATA_AVAILABLE             0x0100
#define GP_ALL_EMPTY                    0x0400

// S3 chips that support MM I/O and ALL_EMPTY have 16 FIFO slots:

#define MM_ALL_EMPTY_FIFO_COUNT         16
#define IO_ALL_EMPTY_FIFO_COUNT         8

#define MULT_MISC_COLOR_COMPARE         0x0100

// Fifo status in terms of empty entries:

#define FIFO_1_EMPTY                    0x080
#define FIFO_2_EMPTY                    0x040
#define FIFO_3_EMPTY                    0x020
#define FIFO_4_EMPTY                    0x010
#define FIFO_5_EMPTY                    0x008
#define FIFO_6_EMPTY                    0x004
#define FIFO_7_EMPTY                    0x002
#define FIFO_8_EMPTY                    0x001

// These are the defines for the multifunction control register.
// The 4 MSBs define the function of the register.

#define RECT_HEIGHT                     0x00000

#define CLIP_TOP                        0x01000
#define CLIP_LEFT                       0x02000
#define CLIP_BOTTOM                     0x03000
#define CLIP_RIGHT                      0x04000

#define DATA_EXTENSION                  0x0A000
#define MULT_MISC_INDEX                 0x0E000
#define READ_SEL_INDEX                  0x0F000

#define ALL_ONES                        0x00000
#define CPU_DATA                        0x00080
#define DISPLAY_MEMORY                  0x000C0

// Colour source:

#define BACKGROUND_COLOR                0x000
#define FOREGROUND_COLOR                0x020
#define SRC_CPU_DATA                    0x040
#define SRC_DISPLAY_MEMORY              0x060

// Mix modes:

#define NOT_SCREEN                      0x00
#define LOGICAL_0                       0x01
#define LOGICAL_1                       0x02
#define LEAVE_ALONE                     0x03
#define NOT_NEW                         0x04
#define SCREEN_XOR_NEW                  0x05
#define NOT_SCREEN_XOR_NEW              0x06
#define OVERPAINT                       0x07
#define NOT_SCREEN_OR_NOT_NEW           0x08
#define SCREEN_OR_NOT_NEW               0x09
#define NOT_SCREEN_OR_NEW               0x0A
#define SCREEN_OR_NEW                   0x0B
#define SCREEN_AND_NEW                  0x0C
#define NOT_SCREEN_AND_NEW              0x0D
#define SCREEN_AND_NOT_NEW              0x0E
#define NOT_SCREEN_AND_NOT_NEW          0x0F


// When one of the following bits is set in a hardware mix, it means
// that a pattern is needed (i.e., is none of NOT_SCREEN, LOGICAL_0,
// LOGICAL_1 or LEAVE_ALONE):

#define MIX_NEEDSPATTERN                0x0C

////////////////////////////////////////////////////////////////////
// S3 Graphics Controler
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
// S3 port control
////////////////////////////////////////////////////////////////////

// Accelerator port addresses:

#define CUR_Y                           0x082E8
#define CUR_Y2                          0x082EA
#define CUR_X                           0x086E8
#define CUR_X2                          0x086EA
#define MM_CRTC_INDEX                   0x083D4
#define MM_CRTC_DATA                    0x083D5
#define MM_STATUS_1                     0x083DA
#define DEST_Y                          0x08AE8
#define DEST_Y2                         0x08AEA
#define DEST_X                          0x08EE8
#define DEST_X2                         0x08EEA
#define AXSTP                           0x08AE8
#define AXSTP2                          0x08AEA
#define DIASTP                          0x08EE8
#define DIASTP2                         0x08EEA
#define ERR_TERM                        0x092E8
#define ERR_TERM2                       0x092EA
#define MAJ_AXIS_PCNT                   0x096E8
#define MAJ_AXIS_PCNT2                  0x096EA
#define CMD                             0x09AE8
#define CMD2                            0x09AEA
#define SHORT_STROKE                    0x09EE8
#define BKGD_COLOR                      0x0A2E8
#define FRGD_COLOR                      0x0A6E8
#define WRT_MASK                        0x0AAE8
#define RD_MASK                         0x0AEE8
#define COLOR_CMP                       0x0B2E8
#define BKGD_MIX                        0x0B6E8
#define FRGD_MIX                        0x0BAE8
#define MULTIFUNC_CNTL                  0x0BEE8
#define MIN_AXIS_PCNT                   0x0BEE8
#define SCISSORS                        0x0BEE8
#define PIX_CNTL                        0x0BEE8
#define PIX_TRANS                       0x0E2E8
#define PAT_Y                           0x0EAE8
#define PAT_X                           0x0EAEA


// Packed addresses, for Trio64 or newer:

#define ALT_CURXY                       0x08100
#define ALT_CURXY2                      0x08104
#define ALT_STEP                        0x08108
#define ALT_STEP2                       0x0810C
#define ALT_ERR                         0x08110
#define ALT_CMD                         0x08118
#define ALT_MIX                         0x08134
#define ALT_PCNT                        0x08148
#define ALT_PAT                         0x08168
#define SCISSORS_T                      0x08138
#define SCISSORS_L                      0x0813A
#define SCISSORS_B                      0x0813C
#define SCISSORS_R                      0x0813E
#define MULT_MISC_READ_SEL              0x08144

////////////////////////////////////////////////////////////////////
// S3 streams processor
////////////////////////////////////////////////////////////////////

// Stream processor register definitions

// Some of these are not valid for all chips... Check hardware specific
// include files (HW3D.H, HWTR.H) as well.

#define  P_CONTROL           0x8180  // Primary Stream Control register
#define  CKEY_LOW            0x8184  // Color/Chroma Key Control
#define  S_CONTROL           0x8190  // Secondary Stream Control
#define  CKEY_HI             0x8194  // Chroma Key Upper Bound
#define  COLOR_ADJUST_REG    0x819C  // Secondary Stream Color Adjust Register
#define  S_HK1K2             0x8198  // Secondary Stream Stretch/Filter const
#define  BLEND_CONTROL       0x81a0  // Blend Control
#define  P_0                 0x81c0  // Primary Stream Frame Buffer Address 0
#define  P_1                 0x81c4  // Primary Stream Frame Buffer Address 1
#define  P_STRIDE            0x81c8  // Primary Stream Stride
#define  LPB_DB              0x81cc  // Double buffer and LPB Support
#define  S_0                 0x81d0  // Secondary Stream Frame Buffer Addr 0
#define  S_1                 0x81d4  // Secondary Stream Frame Buffer Addr 1
#define  S_STRIDE            0x81d8  // Secondary Stream Stride
#define  OPAQUE_CONTROL      0x81dc  // Opaque Overlay Control
#define  S_VK1               0x81e0  // K1 Vertical Scale Factor
#define  S_VK2               0x81e4  // K2 Vertical Scale Factor
#define  S_VDDA              0x81e8  // DDA Vertical Accumulator Init Value

#define  P_XY                0x81f0  // Primary Stream Window Start Coord
#define  P_WH                0x81f4  // Primary Stream Window Size
#define  S_XY                0x81f8  // Secondary Stream Window Start Coord
#define  S_WH                0x81fc  // Secondary Stream Window Size

#define COLOR_ADJUST_ON     0x80000000

#define COLOR_ADJUST_ALLOWED    0x00000001
#define COLOR_STATUS_YUV        0x00000002
#define COLOR_ADJUSTED          0x00000004


#define P_Format_Shift      24
#define P_Format_Mask       (7L << P_Format_Shift)
#define P_RGB8              (0L << P_Format_Shift)
#define P_RGB15             (3L << P_Format_Shift)
#define P_RGB16             (5L << P_Format_Shift)
#define P_RGB24             (6L << P_Format_Shift)
#define P_RGB32             (7L << P_Format_Shift)
#define P_Filter_Shift      28
#define P_Filter_Mask       (7L << P_Filter_Shift)
#define P_1x                (0L << P_Filter_Shift)
#define P_2x                (1L << P_Filter_Shift)
#define P_2xBiLinear        (2L << P_Filter_Shift)

#define BVCr_Shift          0
#define BVCr_Mask           (255L << BVCr_Shift)
#define GUCb_Shift          8
#define GUCb_Mask           (255L << GUCb_Shift)
#define RY_Shift            16
#define RY_Mask             (255L << RY_Shift)
#define Compare_Shift       24
#define Compare_Mask        (255L << Compare_Shift)
#define CompareBits7        (0L  << Compare_Shift)
#define CompareBits6t7      (1L  << Compare_Shift)
#define CompareBits5t7      (2L  << Compare_Shift)
#define CompareBits4t7      (3L  << Compare_Shift)
#define CompareBits3t7      (4L  << Compare_Shift)
#define CompareBits2t7      (5L  << Compare_Shift)
#define CompareBits1t7      (6L  << Compare_Shift)
#define CompareBits0t7      (7L  << Compare_Shift)
#define KeyFrom_Shift       28
#define KeyFrom_Mask        (1L << KeyFrom_Shift)
#define KeyFromStream       (0L << KeyFrom_Shift)
#define KeyFromCompare      (1L << KeyFrom_Shift)

//
// GX2 streams compatible style
//

#define KMS_Shift           29
#define KeySelect_Shift     31
#define WindowKeying        ( 0L << KMS_Shift )
#define AlphaKeying         ( 1L << KMS_Shift )
#define ColorKeying         ( 2L << KMS_Shift )
#define ChromaKeying        ( 3L << KMS_Shift )
#define KeySelectP          ( 0L << KeySelect_Shift )
#define KeySelectS          ( 1L << KeySelect_Shift )
#define GX2_Ks_Mask         ( 8 << Ks_Shift )
#define GX2_Kp_Mask         ( 8 << Kp_Shift )

#define HDDA_Shift          0
#define HDDA_Mask           (((1L << 13)-1) << HDDA_Shift)
#define S_Format_Shift      24
#define S_Format_Mask       (7L << S_Format_Shift)
#define S_YCrCb422          (1L << S_Format_Shift)
#define S_YUV422            (2L << S_Format_Shift)
#define S_RGB15            (3L << S_Format_Shift)
#define S_YUV211            (4L << S_Format_Shift)
#define S_RGB16             (5L << S_Format_Shift)
#define S_RGB24             (6L << S_Format_Shift)
#define S_RGB32             (7L << S_Format_Shift)
#define S_UYVY              (9L << S_Format_Shift) //gxlt
#define S_Filter_Shift      28
#define S_Filter_Mask       (7L << S_Filter_Shift)
#define S_1x                (0L << S_Filter_Shift)
#define S_Upto2x            (1L << S_Filter_Shift)
#define S_2xTo4x            (2L << S_Filter_Shift)
#define S_Beyond4x          (3L << S_Filter_Shift)

#define HighVCr_Shift       0
#define HighVCr_Mask        (255L << HighVCr_Shift)
#define HighUCb_Shift       8
#define HighUCb_Mask        (255L << HighUCb_Shift)
#define HighY_Shift         16
#define HighY_Mask          (255L << HighY_Shift)

#define HK1_Shift           0
#define HK1_Mask            (((1L << 11) - 1) << HK1_Shift)
#define HK2_Shift           16
#define HK2_Mask            (((1L << 12) - 1) << HK2_Shift)
#define VK1_Shift           0
#define VK1_Mask            (((1L << 12) - 1) << VK1_Shift)
#define VK2_Shift           0
#define VK2_Mask            (((1L << 13) - 1) << VK2_Shift)
#define VDDA_Shift          0
#define VDDA_Mask           (((1L << 13) - 1) << VDDA_Shift)

#define Ks_Shift            2
#define Ks_Mask             (7L << Ks_Shift)
#define Kp_Shift            10
#define Kp_Mask             (7L << Kp_Shift)
#define Compose_Shift       24
#define Compose_Mask        (7L << Compose_Shift)
#define SOnP                (0L << Compose_Shift)
#define POnS                (1L << Compose_Shift)
#define Dissolve            (2L << Compose_Shift)
#define Fade                (3L << Compose_Shift)
#define KeyOnP              (5L << Compose_Shift)
#define KeyOnS              (6L << Compose_Shift)

#define FifoAlloc_Shift    0
#define FifoAlloc_Mask     (31L << StrFifoAlloc_Shift)
#define FifoAlloc24_0      (0L  << StrFifoAlloc_Shift)
#define FifoAlloc16_8      (8L  << StrFifoAlloc_Shift)
#define FifoAlloc12_12     (12L << StrFifoAlloc_Shift)
#define FifoAlloc8_16      (16L << StrFifoAlloc_Shift)
#define FifoAlloc0_24      (24L << StrFifoAlloc_Shift)


#define RASLowTime_Shift    15
#define RASLowTime_Mask     (1L << RASLowTime_Shift)
#define RASLowTimeFromCR68  (0L << RASLowTime_Shift)
#define RASLowTime2_5       (1L << RASLowTime_Shift)

#define RASPreCharge_Shift     16
#define RASPreCharge_Mask      (1L << RASPreCharge_Shift)
#define RASPreChargeFromCR68  (0L << RASPreCharge_Shift)
#define RASPreCharge1_5        (1L << RASPreCharge_Shift)

#define RASInactive_Shift  17
#define RASInactive_Mask   (1L << RASInactive_Shift)
#define RASInactiveLow     (0L << RASInactive_Shift)
#define RASInactiveHigh    (1L << RASInactive_Shift)

#define MemoryCycle_Shift  18
#define MemoryCycle_Mask   (1L << MemoryCycle_Shift)
#define MemoryCycle2    (0L << MemoryCycle_Shift)
#define MemoryCycle1    (1L << MemoryCycle_Shift)

#define H_Shift         0
#define H_Mask       (0x07ffL << H_Shift)
#define W_Shift         16
#define W_Mask       (0x07ffL << W_Shift)

#define Y_Shift         0
#define Y_Mask       (0x07ffL << Y_Shift)
#define X_Shift         16
#define X_Mask       (0x07ffL << X_Shift)

#define P_Select_Shift          0
#define P_Select_Mask           (1L << P_Select_Shift)
#define P_Select0               (0L << P_Select_Shift)
#define P_Select1               (1L << P_Select_Shift)
#define S_Select_Shift          1
#define S_Select_Mask           (3L << S_Select_Shift)
#define S_Select0               (0L << S_Select_Shift)
#define S_Select1               (1L << S_Select_Shift)
#define S_Select00Or11          (2L << S_Select_Shift)
#define S_Select01Or10          (3L << S_Select_Shift)
#define L_Select_Shift          4
#define L_Select_Mask           (1L << L_Select_Shift)
#define L_Select0               (0L << L_Select_Shift)
#define L_Select1               (1L << L_Select_Shift)
#define L_SelWait_Shift         5
#define L_SelWait_Mask          (1L << L_SelWait_Shift)
#define L_SelWaitNo             (0L << L_SelWait_Shift)
#define L_SelWaitYes            (1L << L_SelWait_Shift)
#define L_SelAutoToggle_Shift   6
#define L_SelAutoToggle_Mask    (1L << L_SelAutoToggle_Shift)
#define L_SelAutoToggleNo       (0L << L_SelAutoToggle_Shift)
#define L_SelAutoToggleYes      (1L << L_SelAutoToggle_Shift)
#define L_FramesToDrop_Shift  8
#define L_FramesToDrop_Mask     (3L << L_FramesToDrop_Shift)

// IN ORDER TO GET Opaque Overlays working, had to pretend
// 16 bytes in a QWORD!!!
#define BYTESperQWORD           16

#define OpqStart_Shift          3
#define OpqStart_Mask           (((1L << 10) - 1) << OpqStart_Shift)
#define OpqEnd_Shift            19
#define OpqEnd_Mask             (((1L << 10) - 1) << OpqEnd_Shift)
#define OpqTopSel_Shift         30
#define OpqTopSel_Mask          (1L << OpqTopSel_Shift)
#define OpqTopSelS_             (0L << OpqTopSel_Shift)
#define OpqTopSelP_             (1L << OpqTopSel_Shift)
#define OpqCtrl_Shift           31
#define OpqCtrl_Mask            (1L << OpqCtrl_Shift)
#define OpqDisabled             (0L << OpqCtrl_Shift)
#define OpqEnabled              (1L << OpqCtrl_Shift)

// The following defines are for VL and PCI system configuration
// Although defined here, they currently do not seem to be used.
//
#define  K2V_SRD_LPB_MASK        0x03
#define  K2V_SRD_LPB             0x00
#define  K2V_SRD_FC              K2V_SRD_LPB_MASK
#define  K2V_SRD_COMP            0x02

#define  K2V_CR5C_SRC_MASK       0x03
#define  K2V_CR5C_SRC_DIGITIZER  0x02
#define  K2V_CR5C_SRC_MPEG       0x01

#define  K2V_SR1C_MASK           0x03
#define  K2V_SR1C_VL             0x01
#define  K2V_SR1C_PCI            0x02

// Useful macros

#define HDDA(w0,w1)  (((2*(w0-1)-(w1-1)) << HDDA_Shift) & HDDA_Mask )
#define VDDA(h1)     (((1-h1)            << VDDA_Shift) & VDDA_Mask )
#define HK1(w0)      (((w0 - 1)          << HK1_Shift)  & HK1_Mask  )
#define HK2(w0,w1)   (((w0 - w1)         << HK2_Shift)  & HK2_Mask  )
#define HK1K2(w0,w1) (HK1(w0) | HK2(w0, w1))
#define VK1(h0)      (((h0 - 1)          << VK1_Shift)  & VK1_Mask  )
#define VK2(h0,h1)   (((h0 - h1)         << VK2_Shift)  & VK2_Mask  )
#define XY(x,y)      ((((x+1)<<X_Shift)&X_Mask) | (((y+1)<<Y_Shift)&Y_Mask))
#define WH(w,h)      ((((w-1)<<W_Shift)&W_Mask) | (((h)<<H_Shift)&H_Mask))

#define HWCODEC          0
#define SWCODEC          1
#define DIGITIZER        2
#define MAX_DEVICE       3
#define DSTWIN_SIZES     5

////////////////////////////////////////////////////////////////////
// S3 pixel formatter
////////////////////////////////////////////////////////////////////

// Equates for Pixel Formatter (Video Engine) 868/968

#define INPUT_RGB8      0x00000000
#define INPUT_RGB15     0x00600000
#define INPUT_RGB16     0x00700000
#define INPUT_RGB32     0x00300000
#define INPUT_YUV422    0x00480000
#define INPUT_YCrCb422  0x00400000
#define INPUT_RAW       0x00500000
#define OUTPUT_RGB8     0x00000000
#define OUTPUT_RGB15    0x00060000
#define OUTPUT_RGB16    0x00070000
#define OUTPUT_RGB32    0x00030000
#define OUTPUT_YUY2     0x000C0000
#define OUTPUT_RAW      0x00050000

#define CSCENABLE       0x40000000
#define STRETCH         0x00000000
#define SHRINK          0x80000000
#define SCREEN          0x00000000
#define HOST            0x40000000
#define FILTERENABLE    0x80000000
#define BILINEAR        0x00000000
#define LINEAR02420     0x00004000
#define LINEAR12221     0x00008000

#define PF_BUSY         0x80000000

#define PF_NOP          0x00018080
#define PF_CONTROL      0x00018088
#define PF_DDA          0x0001808C
#define PF_STEP         0x00018090
#define PF_CROP         0x00018094
#define PF_SRCADDR      0x00018098
#define PF_DSTADDR      0x0001809C

//////////////////////////////////////////////////////////////////////
// PowerPC considerations
//
// The PowerPC does not guarantee that I/O to separate addresses will
// be executed in order.  However, the PowerPC guarantees that
// output to the same address will be executed in order.
//
// Consequently, we use the following synchronization macros.  They
// are relatively expensive in terms of performance, so we try to avoid
// them whereever possible.
//
// CP_EIEIO() 'Ensure In-order Execution of I/O'
//    - Used to flush any pending I/O in situations where we wish to
//      avoid out-of-order execution of I/O to separate addresses.
//
// CP_MEMORY_BARRIER()
//    - Used to flush any pending I/O in situations where we wish to
//      avoid out-of-order execution or 'collapsing' of I/O to
//      the same address.  We used to have to do this separately for
//      the Alpha because unlike the PowerPC it did not guarantee that
//      output to the same address will be exectued in order.  However,
//      with the move to kernel-mode, on Alpha we are now calling HAL
//      routines for every port I/O which ensure that this is not a
//      problem.

#if defined(_PPC_)

    // On PowerPC, CP_MEMORY_BARRIER doesn't do anything.

    #define CP_EIEIO()          MEMORY_BARRIER()
    #define CP_MEMORY_BARRIER()

#elif defined(_ALPHA_)

    // On Alpha, since we must do all non-frame-buffer I/O through HAL
    // routines, which automatically do memory-barriers, we don't have
    // to do memory barriers ourselves (and should not, because it's a
    // performance hit).

    #define CP_EIEIO()
    #define CP_MEMORY_BARRIER()

#else

    // On other systems, both CP_EIEIO and CP_MEMORY_BARRIER don't do anything.

    #define CP_EIEIO()          MEMORY_BARRIER()
    #define CP_MEMORY_BARRIER() MEMORY_BARRIER()

#endif

////////////////////////////////////////////////////////////////////
// Macros for accessing accelerator registers:

#if defined(i386)

    /////////////////////////////////////////////////////////////////////////
    // x86

    #define CSR_BASE 0

    // OUT_WORD, OUT_BYTE, IN_BYTE, WRITE_WORD
    //
    // For accessing common accelerator registers.  Be careful -- these
    // macros do no MEMORY BARRIERs.  See the non-x86 definitions below.

    #define WRITE_WORD(address, x)                                  \
   WRITE_REGISTER_USHORT((address), (USHORT) (x));

#else

    /////////////////////////////////////////////////////////////////////////
    // Non-x86
    //
    // The code makes extensive use of the inp, inpw, outp and outpw x86
    // intrinsic functions. Since these don't exist on the Alpha platform,
    // map them into something we can handle.  Since the CSRs are mapped
    // on Alpha, we have to add the register base to the register number
    // passed in the source.

    // OUT_WORD, OUT_BYTE, IN_BYTE, WRITE_WORD
    //
    // These are for quick I/O where we can explicitly handle
    // MEMORY BARRIERs ourselves.  It is best to use OUTPW for non-critical
    // code, because it's easy to overwrite the IO cache when MEMORY_BARRIERs
    // aren't bracketing everything.  Note that the IO_ routines provide
    // the necessary abstraction so that you don't usually have to think
    // about memory barriers.
    //
    // READ/WRITE_FAST_* routines need to know the byte alignment of
    // I/O port address in order to compute the lane shift of word data on
    // the Alpha.  Since all S3 graphics accelerator ports are aligned on
    // 0xE8 boundaries, we cheat a little, and only tell the macros about
    // this, instead of the entire I/O port address.

    #define WRITE_WORD(p, v)     WRITE_REGISTER_USHORT((p), (USHORT)(v));

#endif

#if DBG

    /////////////////////////////////////////////////////////////////////////
    // Checked Build
    //
    // We hook some of the accelerator macros on checked (debug) builds
    // for sanity checking.

    UCHAR   vgaInp(PDEV*, ULONG);
    USHORT  vgaInpW(PDEV*, ULONG);
    VOID    vgaOutp(PDEV*, ULONG, ULONG);
    VOID    vgaOutpW(PDEV*, ULONG, ULONG);

    VOID    vAcquireCrtc(PDEV*);
    VOID    vReleaseCrtc(PDEV*);

    VOID    vS3DFifoWait(PDEV*, ULONG);
    BOOL    vS3DFifoBusy(PDEV*, ULONG);
    BOOL    vS3DGPBusy(PDEV*);

    #define S3DFifoWait(ppdev, slots)   vS3DFifoWait((ppdev), (slots))

    #define S3DFifoBusy(ppdev, slots)   vS3DFifoBusy((ppdev), (slots))
    #define S3DGPBusy(ppdev)            vS3DGPBusy((ppdev))

    #define VGAOUTPW(ppdev, p, v)        vgaOutpW((ppdev), (p), (ULONG) (v))
    #define VGAOUTP(ppdev, p, v)         vgaOutp((ppdev), (p), (ULONG) (v))
    #define VGAINPW(ppdev, p)            vgaInpW((ppdev), (p))
    #define VGAINP(ppdev, p)             vgaInp((ppdev), (p))

    // The CRTC register critical section must be acquired before
    // touching the CRTC register (because of async pointers):

    #define ACQUIRE_CRTC_CRITICAL_SECTION(ppdev)                        \
                AcquireMiniPortCrtc(ppdev);                                                             \
        vAcquireCrtc(ppdev)

    #define RELEASE_CRTC_CRITICAL_SECTION(ppdev)                                \
                ReleaseMiniPortCrtc(ppdev);                                                             \
                vReleaseCrtc(ppdev)

#else

    /////////////////////////////////////////////////////////////////////////
    // Free Build
    //
    // For a free (non-debug build), we make everything in-line.

    // Safe port access macros -- these macros automatically do memory
    // -----------------------    barriers, so you don't have to worry
    //                            about them:

    #define S3DFifoWait(ppdev, slots)\
    while (((S3readHIU(ppdev, S3D_SUBSYSTEM_STATUS) >> 8) & 0x1F) < slots);


    #define S3DFifoBusy(ppdev, slots)\
            (((S3readHIU(ppdev, S3D_SUBSYSTEM_STATUS) >> 8) & 0x1F) < (ULONG) slots)

    #define S3DGPBusy(ppdev)\
            (!(S3readHIU(ppdev, S3D_SUBSYSTEM_STATUS) & S3D_ENGINE_IDLE))



    #if defined(_X86_)

        // x86 doesn't need 'pjIoBase' added in, so save some code space:

        #if ( PCIMMIO || NT_AGP_WORKAROUND )

            //
            // Macros for accessing the VGA register set
            //

            #define VGAOUTPW(ppdev, p, v)   \
            { \
                WRITE_REGISTER_USHORT((ppdev->pjMmBase + p + 0x8000), (USHORT)(v)); \
            }

            #define VGAOUTP(ppdev, p, v)    \
            {\
                WRITE_REGISTER_UCHAR((ppdev->pjMmBase + p + 0x8000), (UCHAR)(v));\
            }

            __inline USHORT VGAINPW(PDEV* ppdev, ULONG p)
            {
                return(READ_REGISTER_USHORT((ppdev->pjMmBase + p + 0x8000)));
            }

            __inline UCHAR VGAINP(PDEV* ppdev, ULONG p)
            {
                return( READ_REGISTER_UCHAR( ( ppdev->pjMmBase + p + 0x8000 ) ) );
            }

            //
            // Macros for accessing the extended register set
            //

            #define OUTPW(ppdev, p, v)   \
            {\
                WRITE_REGISTER_USHORT((ppdev->pjMmBase + p), (USHORT)(v));\
            }

            #define OUTP(ppdev, p, v)    \
            {\
                WRITE_REGISTER_UCHAR((ppdev->pjMmBase + p), (UCHAR)(v));\
            }

            __inline USHORT INPW(PDEV* ppdev, ULONG p)
            {
                return(READ_REGISTER_USHORT((ppdev->pjMmBase + p)));
            }

            __inline UCHAR INP(PDEV* ppdev, ULONG p)
            {
                return(READ_REGISTER_UCHAR((ppdev->pjMmBase + p)));
            }

        #else //PCIMMIO

            //
            // Macros for accessing the VGA register set
            //

            #define VGAOUTPW(ppdev, p, v)   WRITE_PORT_USHORT((p), (v))
            #define VGAOUTP(ppdev, p, v)    WRITE_PORT_UCHAR((p), (v))
            #define VGAINPW(ppdev, p)       READ_PORT_USHORT((p))
            #define VGAINP(ppdev, p)        READ_PORT_UCHAR((p))

        #endif //PCIMMIO
    #else

        // Non-x86 platforms have the I/O range starting at 'pjIoBase':

        #define OUTPW(ppdev, p, v)                       \
        {                                                   \
            CP_EIEIO();                                     \
            WRITE_PORT_USHORT((ppdev->pjIoBase) + (p), (v));       \
            CP_EIEIO();                                     \
        }
        #define OUTP(ppdev, p, v)                        \
        {                                                   \
            CP_EIEIO();                                     \
            WRITE_PORT_UCHAR((ppdev->pjIoBase) + (p), (v));        \
            CP_EIEIO();                                     \
        }

        __inline USHORT INPW(PDEV* ppdev, ULONG p)
        {
            CP_EIEIO();
            return(READ_PORT_USHORT(ppdev->pjIoBase + p));
        }

        __inline UCHAR INP(PDEV* ppdev, ULONG p)
        {
            CP_EIEIO();
            return(READ_PORT_UCHAR(ppdev->pjIoBase + p));
        }

        #define VGAOUTPW(ppdev, p, v)      OUTPW((ppdev), (p), (v))
        #define VGAOUTP(ppdev, p, v)       OUTP((ppdev), (p), (v))
        #define VGAINPW(ppdev, p)          INPW((ppdev), (p))
        #define VGAINP(ppdev, p)           INP((ppdev), (p))



    #endif //defined(_X86_)

    // The CRTC register critical section must be acquired before
    // touching the CRTC register (because of async pointers):

    #define ACQUIRE_CRTC_CRITICAL_SECTION(ppdev)                \
                AcquireMiniPortCrtc(ppdev);                                                             \
        EngAcquireSemaphore(ppdev->csCrtc);

    // 80x/805i/928 and 928PCI chips have a bug where if I/O registers
    // are left unlocked after accessing them, writes to memory with
    // similar addresses can cause writes to I/O registers.  The problem
    // registers are 0x40, 0x58, 0x59 and 0x5c.  We will simply always
    // leave the index set to an innocuous register (namely, the text
    // mode cursor start scan line):

    #define RELEASE_CRTC_CRITICAL_SECTION(ppdev)                \
    {                                                           \
                ReleaseMiniPortCrtc(ppdev);                                                             \
        VGAOUTP(ppdev, CRTC_INDEX, 0xa);                 \
        EngReleaseSemaphore(ppdev->csCrtc);                     \
    }

#endif

//////////////////////////////////////////////////////////////////////

// Note: The PACKXY_FAST macro is unsafe with negative coordinates

#define PACKXY(x, y)        (((x) << 16) | ((y) & 0xffff))
#define PACKXY_FAST(x, y)   (((x) << 16) | (y))

/////////////////////////////////////////////////////////////////////////////
// MM_TRANSFER routines

#if defined(i386)
//
////////////////////////////////////////////////////////////////////////////
//// Macros for transferring data via memory-mapped I/O
////
//// I ran into a problem with these macros where more than one
//// instance couldn't be declared in a function without causing
//// compiler errors from duplicate labels.  I got around this by
//// explictly passing in a unique 'Id' identifier to append to the
//// end of the labels, to make them unique (so 'Id' can be pretty
//// much anything).
////
//// The caller must ensure that the 32k memory-mapped transfer
////       space is never exceeded.
//
////////////////////////////
//// MM_TRANSFER_BYTE  - Byte transfers using memory-mapped I/O transfers.
//// IO_TRANSFER_BYTE  - Byte transfers using normal I/O.
//// TXT_TRANSFER_BYTE - Byte transfers using memory-mapped I/O for text output.
////
//// Unfortunately, we can't just do what amounts to a REP MOVSB because
//// the S3 will drop every second byte.  We simplify this loop a bit by
//// simply outputing new bytes to the same memory-mapped address:
//
//#define MM_TRANSFER_BYTE(ppdev, pjMmBase, p, c, Id)                         \
//{                                                                           \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    __asm mov ecx, c                                                        \
//    __asm mov esi, p                                                        \
//    __asm mov edi, pjMmBase                                                 \
//    __asm shr ecx, 1                                                        \
//    __asm rep movsw                                                         \
//    __asm jnc short AllDone##Id                                             \
//    __asm mov cx, [esi]                                                     \
//    __asm mov [edi], cx                                                     \
//    __asm AllDone##Id:                                                      \
//}
//
////#define NEW_TXT_TRANSFER_BYTE(ppdev, pjMmBase, p, c, Id)                    \
////{                                                                           \
////    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
////    __asm mov edi, pjMmBase                                                 \
////    __asm mov esi, p                                                        \
////    __asm mov ecx, c                                                        \
////    __asm shr ecx, 1                                                        \
////    __asm WordLoop##Id:                                                     \
////    __asm mov ax, [esi]                                                     \
////    __asm mov [edi], ax                                                     \
////    __asm add esi,2                                                         \
////    __asm dec ecx                                                           \
////    __asm jnz short WordLoop##Id                                            \
////    __asm mov ecx,c                                                         \
////    __asm and ecx,1                                                         \
////    __asm jz short NTDone##Id                                               \
////    __asm xor ax, ax                                                        \
////    __asm mov al, [esi]                                                     \
////    __asm mov [edi], ax                                                     \
////    __asm inc esi                                                           \
////    __asm NTDone##Id:                                                       \
////}
//
//#define IO_TRANSFER_BYTE(ppdev, p, c, Id)                                   \
//{                                                                           \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    __asm mov edx, ppdev                                                    \
//    __asm mov ecx, c                                                        \
//    __asm mov esi, p                                                        \
//    __asm mov edx, [edx]ppdev.ioPix_trans                                   \
//    __asm rep outsb                                                         \
//}
//
//#define TXT_TRANSFER_BYTE MM_TRANSFER_BYTE
//
//
////////////////////////////
//// MM_TRANSFER_BYTE_THIN  - Glyph transfers using memory-mapped I/O transfers.
////
//// NOTE: The first versions of the 868/968 have a bug where they can't do
////       byte-sized memory-mapped transfers.  Consequently, we always do
////       word transfers.
//
//#define MM_TRANSFER_BYTE_THIN(ppdev, pjMmBase, p, c, Id)                    \
//{                                                                           \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    __asm mov edi, pjMmBase                                                 \
//    __asm mov esi, p                                                        \
//    __asm mov ecx, c                                                        \
//    __asm ByteThinLoop##Id:                                                 \
//    __asm mov ax, [esi]                                                     \
//    __asm mov [edi], ax                                                     \
//    __asm add edi, 2                                                        \
//    __asm inc esi                                                           \
//    __asm loop short ByteThinLoop##Id                                       \
//}
//
////////////////////////////
//// MM_TRANSFER_WORD_ALIGNED  - Word transfers using memory-mapped transfers.
//// IO_TRANSFER_WORD_ALIGNED  - Word transfers using normal I/O.
//// TXT_TRANSFER_WORD_ALIGNED - Word transfers using either method for text
////                             output.
////
//// Source must be dword aligned (enforced to help non-x86 platforms)
//
//#define MM_TRANSFER_WORD_ALIGNED(ppdev, pjMmBase, p, c, Id)                 \
//{                                                                           \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    ASSERTDD((((ULONG) p) & 3) == 0, "Transfer not dword aligned");         \
//    MM_TRANSFER_WORD(ppdev, pjMmBase, (p), (c), Id);                        \
//}
//
//#define IO_TRANSFER_WORD_ALIGNED(ppdev, p, c, Id)                           \
//{                                                                           \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    ASSERTDD((((ULONG) p) & 3) == 0, "Transfer not dword aligned");         \
//    IO_TRANSFER_WORD(ppdev, (p), (c), Id);                                  \
//}
//
//#define TXT_TRANSFER_WORD_ALIGNED MM_TRANSFER_WORD_ALIGNED
//
////////////////////////////
//// MM_TRANSFER_WORD  - Word transfers using memory-mapped transfers.
//// IO_TRANSFER_WORD  - Word transfers using normal I/O.
//// TXT_TRANSFER_WORD - Word transfers using either method for text output.
////
//// Source does not have to be dword aligned.
//
//#define MM_TRANSFER_WORD(ppdev, pjMmBase, p, c, Id)                         \
//{                                                                           \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    __asm mov ecx, c                                                        \
//    __asm mov esi, p                                                        \
//    __asm mov edi, pjMmBase                                                 \
//    __asm shr ecx, 1                                                        \
//    __asm rep movsd                                                         \
//    __asm jnc short AllDone##Id                                             \
//    __asm mov cx, [esi]                                                     \
//    __asm mov [edi], cx                                                     \
//    __asm AllDone##Id:                                                      \
//}
//
//#define IO_TRANSFER_WORD(ppdev, p, c, Id)                                   \
//{                                                                           \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    __asm mov edx, ppdev                                                    \
//    __asm mov ecx, c                                                        \
//    __asm mov esi, p                                                        \
//    __asm mov edx, [edx]ppdev.ioPix_trans                                   \
//    __asm rep outsw                                                         \
//}
//
//#define TXT_TRANSFER_WORD MM_TRANSFER_WORD
//
////////////////////////////
//// MM_TRANSFER_DWORD_ALIGNED - Dword transfers using memory-mapped transfers.
////
//// Source must be dword aligned!
//
//#define IO_TRANSFER_DWORD_ALIGNED(ppdev, p, c, Id)                          \
//{                                                                           \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    ASSERTDD((((ULONG) p) & 3) == 0, "Transfer not dword aligned");         \
//    IO_TRANSFER_DWORD(ppdev, (p), (c), Id);                                 \
//}
//
//////////////////////////
// MM_TRANSFER_DWORD - Dword transfers using memory-mapped transfers.
//
// Source does not have to be dword aligned.

#define MM_TRANSFER_DWORD(ppdev, pjMmBase, p, c, Id)                        \
{                                                                           \
    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
    __asm mov ecx, c                                                        \
    __asm mov esi, p                                                        \
    __asm mov edi, pjMmBase                                                 \
    __asm rep movsd                                                         \
}

//////////////////////////
// MM_TRANSFER - transfer a specified number of bytes
//
// Always does dword writes, but will only read the specified number
// of bytes.

#define MM_TRANSFER(ppdev, pjMmBase, p, c)                                  \
{                                                                           \
    PUCHAR pDst = (PUCHAR)(pjMmBase);                                       \
    PUCHAR pSrc = (PUCHAR)(p);                                              \
    ULONG Count = (c);                                                      \
    ULONG cDwords;                                                          \
    ULONG FinalBits=0;                                                      \
    ULONG Rotate=0;                                                         \
                                                                            \
    cDwords = Count >> 2;                                                   \
    Count = Count & 0x3;                                                    \
                                                                            \
    while (cDwords--) {                                                     \
        WRITE_REGISTER_ULONG(((PULONG)pDst)++, *((ULONG UNALIGNED *)pSrc)++);\
    }                                                                       \
                                                                            \
    if (Count) {                                                            \
        while (Count--) {                                                   \
            FinalBits |= ((ULONG)(*pSrc++)) << Rotate;                      \
            Rotate += 8;                                                    \
        }                                                                   \
        WRITE_REGISTER_ULONG((PULONG)pDst, FinalBits);                      \
    }                                                                       \
}

//
////////////////////////////
//// MM_TRANSFER_WORD_ODD - Word transfers for glyphs of odd byte length
////                        and more than one byte wide.
////
//// Source must be word aligned.
//
//#define MM_TRANSFER_WORD_ODD(ppdev, pjMmBase, p, cjWidth, cy, Id)           \
//{                                                                           \
//    ASSERTDD( ((cjWidth) > 0) && ((cy) > 0),                                \
//              "Can't have a zero transfer count");                          \
//    ASSERTDD((cjWidth) & 1, "Must be odd byte width");                      \
//    ASSERTDD((cjWidth) > 2, "Must be more than 2 bytes wide");              \
//    __asm mov   edi, pjMmBase                                               \
//    __asm mov   esi, p                                                      \
//    __asm mov   ecx, cy                                                     \
//    __asm mov   edx, cjWidth                                                \
//    __asm shr   edx, 1                                                      \
//    __asm WordOddLoop##Id:                                                  \
//    __asm push  ecx                                                         \
//    __asm mov   ecx, edx                                                    \
//    __asm rep   movsw                                                       \
//    __asm pop   ecx                                                         \
//    __asm mov   ax, [esi]                                                   \
//    __asm mov   [edi], ax                                                   \
//    __asm inc   esi                                                         \
//    __asm add   edi, 2                                                      \
//    __asm loop  short WordOddLoop##Id                                       \
//}
//

#else

//
///////////////////////////////////////////////////////////////////////////////
//// MM_TRANSFER routines
////
//// For better Alpha performance, if we have memory-mapped transfers, we
//// try to do each write to the data transfer register to a unique
//// address, in order to avoid costly memory barriers.  Plus, we use
//// precomputed adresses so that we can use the cheap WRITE_FAST_UCHAR
//// macro instead of the very expensive WRITE_REGISTER_UCHAR macro.
//// (This is all taken care of by using the 'apjMmXfer' array.)
////
//// The 'Id' parameter may be ignored (it's there only for x86).
//
///////////////////////////////////////////////////////////////////////////////
//// TXT_MM_TRANSFER
////
//// We use the memory-mapped transfer register if we have one available
//// on the Alpha, because we can spread the writes over a bunch of
//// addresses, thus foiling the write buffer and avoiding memory barriers.
////
//// On anything that doesn't have funky write buffers, it doesn't matter,
//// so we'll always use the data transfer register.
//
//#if defined(ALPHA)
//    #define TXT_MM_TRANSFER(ppdev) (ppdev->ulCaps & CAPS_MM_TRANSFER)
//#else
//    #define TXT_MM_TRANSFER(ppdev) 0
//#endif
//
////////////////////////////
//// MM_TRANSFER_BYTE  - Byte transfers using memory-mapped I/O transfers.
//// IO_TRANSFER_BYTE  - Byte transfers using normal I/O.
//// TXT_TRANSFER_BYTE - Byte transfers using either method for text output.
//
//#define MM_TRANSFER_BYTE(ppdev, pjMmBase, p, c, Id)                         \
//{                                                                           \
//             ULONG    mcw        = (c) >> 1;                                \
//             BYTE*    mpjSrc     = (BYTE*) (p);                             \
//             USHORT** mapwMmXfer = ppdev->apwMmXfer;                        \
//                                                                            \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    CP_MEMORY_BARRIER();                                                    \
//    while (mcw-- != 0)                                                      \
//    {                                                                       \
//        WRITE_REGISTER_USHORT(MM(pjMmBase, mapwMmXfer[mcw & XFER_MASK]),    \
//                              *((USHORT UNALIGNED *) mpjSrc));              \
//        mpjSrc += 2;                                                        \
//    }                                                                       \
//    if ((c) & 1)                                                            \
//    {                                                                       \
//        WRITE_REGISTER_USHORT(MM(pjMmBase, mapwMmXfer[XFER_MASK]),          \
//                          (USHORT) (*mpjSrc));                              \
//    }                                                                       \
//}
//
////#define MM_TRANSFER_BYTE(ppdev, pjMmBase, p, c, Id)                         \
////{                                                                           \
////    register ULONG   mcj        = (c);                                      \
////    register BYTE*   mpjSrc     = (BYTE*) (p);                              \
////    register UCHAR** mapjMmXfer = ppdev->apjMmXfer;                         \
////    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
////    MEMORY_BARRIER();                                                       \
////    do {                                                                    \
////        WRITE_PORT_UCHAR(mapjMmXfer[mcj & XFER_MASK], *mpjSrc++);        \
////    } while (--mcj);                                                        \
////}
//
//#define IO_TRANSFER_BYTE(ppdev, p, c, Id)                                   \
//{                                                                           \
//    register ULONG mcj    = (c);                                            \
//    register BYTE* mpjSrc = (BYTE*) (p);                                    \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    do {                                                                    \
//   IO_PIX_TRANS_BYTE(ppdev, *mpjSrc++);                                \
//    } while (--mcj);                                                        \
//}
//
//#define TXT_TRANSFER_BYTE(ppdev, pjMmBase, p, c, Id)                        \
//{                                                                           \
//    if (TXT_MM_TRANSFER(ppdev))                                             \
//    {                                                                       \
//   MM_TRANSFER_BYTE(ppdev, pjMmBase, (p), (c), Id);                    \
//    }                                                                       \
//    else                                                                    \
//    {                                                                       \
//   IO_TRANSFER_BYTE(ppdev, (p), (c), Id);                              \
//    }                                                                       \
//}
//
////////////////////////////
//// MM_TRANSFER_BYTE_THIN  - Glyph transfers using memory-mapped I/O transfers.
////
//// NOTE: The first versions of the 868/968 have a bug where they can't do
////       byte-sized memory-mapped transfers.  Consequently, we always do
////       word transfers.
//
//#define MM_TRANSFER_BYTE_THIN(ppdev, pjMmBase, p, c)                        \
//{                                                                           \
//             ULONG    mcj        = (c);                                     \
//             BYTE*    mpjSrc     = (BYTE*) (p);                             \
//             USHORT** mapwMmXfer = ppdev->apwMmXfer;                        \
//                                                                            \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    CP_MEMORY_BARRIER();                                                    \
//    do {                                                                    \
//        WRITE_REGISTER_USHORT(MM(pjMmBase, mapwMmXfer[mcj & XFER_MASK]),    \
//                          (USHORT) (*mpjSrc));                              \
//        mpjSrc++;                                                           \
//    } while (--mcj);                                                        \
//}
//
////////////////////////////
//// MM_TRANSFER_WORD_ALIGNED  - Word transfers using memory-mapped transfers.
//// IO_TRANSFER_WORD_ALIGNED  - Word transfers using normal I/O.
//// TXT_TRANSFER_WORD_ALIGNED - Word transfers using either method for text
////                             output.
////
//// Source must be dword aligned!
//
//#define MM_TRANSFER_WORD_ALIGNED(ppdev, pjMmBase, p, c, Id)                 \
//{                                                                           \
//    register ULONG   mcd          = (c) >> 1;                               \
//    register ULONG*  mpdSrc       = (ULONG*) (p);                           \
//    register ULONG** mapdMmXfer   = ppdev->apdMmXfer;                       \
//    ASSERTDD((((ULONG) p) & 3) == 0, "Transfer not dword aligned");         \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    MEMORY_BARRIER();                                                       \
//    while (mcd-- != 0)                                                      \
//    {                                                                       \
//        WRITE_PORT_ULONG(mapdMmXfer[mcd & XFER_MASK], *mpdSrc++);           \
//    }                                                                       \
//    if ((c) & 1)                                                            \
//    {                                                                       \
//        WRITE_PORT_USHORT(ppdev->apwMmXfer[XFER_MASK],                      \
//               *((USHORT*) mpdSrc));                         \
//    }                                                                       \
//}
//
//#define IO_TRANSFER_WORD_ALIGNED(ppdev, p, c, Id)                           \
//{                                                                           \
//    register ULONG   mcw    = (c);                                          \
//    register USHORT* mpwSrc = (USHORT*) (p);                                \
//    ASSERTDD((((ULONG) p) & 3) == 0, "Transfer not dword aligned");         \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    do {                                                                    \
//   IO_PIX_TRANS(ppdev, *mpwSrc++);                                     \
//    } while (--mcw);                                                        \
//}
//
//#define TXT_TRANSFER_WORD_ALIGNED(ppdev, pjMmBase, p, c, Id)                \
//{                                                                           \
//    if (TXT_MM_TRANSFER(ppdev))                                             \
//    {                                                                       \
//   MM_TRANSFER_WORD_ALIGNED(ppdev, pjMmBase, (p), (c), Id);            \
//    }                                                                       \
//    else                                                                    \
//    {                                                                       \
//   IO_TRANSFER_WORD_ALIGNED(ppdev, (p), (c), Id);                      \
//    }                                                                       \
//}
//
////////////////////////////
//// MM_TRANSFER_WORD  - Word transfers using memory-mapped transfers.
//// IO_TRANSFER_WORD  - Word transfers using normal I/O.
//// TXT_TRANSFER_WORD - Word transfers using either method for text output.
////
//// Source does not have to be dword aligned.
//
//#define MM_TRANSFER_WORD(ppdev, pjMmBase, p, c, Id)                         \
//{                                                                           \
//    register ULONG UNALIGNED * mpdSrc     = (ULONG*) (p);                   \
//    register ULONG             mcd        = (c) >> 1;                       \
//    register ULONG**           mapdMmXfer = ppdev->apdMmXfer;               \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    MEMORY_BARRIER();                                                       \
//    while (mcd-- != 0)                                                      \
//    {                                                                       \
//        WRITE_PORT_ULONG(mapdMmXfer[mcd & XFER_MASK], *mpdSrc++);           \
//    }                                                                       \
//    if ((c) & 1)                                                            \
//    {                                                                       \
//        WRITE_PORT_USHORT(ppdev->apwMmXfer[XFER_MASK],                      \
//               *((USHORT UNALIGNED *) mpdSrc));              \
//    }                                                                       \
//}
//
//#define IO_TRANSFER_WORD(ppdev, p, c, Id)                                   \
//{                                                                           \
//    register ULONG              mcw    = (c);                               \
//    register USHORT UNALIGNED * mpwSrc = (USHORT*) (p);                     \
//    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
//    do {                                                                    \
//   IO_PIX_TRANS(ppdev, *mpwSrc++);                                     \
//    } while (--mcw);                                                        \
//}
//
//#define TXT_TRANSFER_WORD(ppdev, pjMmBase, p, c, Id)                        \
//{                                                                           \
//    if (TXT_MM_TRANSFER(ppdev))                                             \
//    {                                                                       \
//   MM_TRANSFER_WORD(ppdev, pjMmBase, (p), (c), Id);                    \
//    }                                                                       \
//    else                                                                    \
//    {                                                                       \
//   IO_TRANSFER_WORD(ppdev, (p), (c), Id);                              \
//    }                                                                       \
//}
//

//////////////////////////
// MM_TRANSFER_DWORD - Dword transfers using memory-mapped transfers.
//
// Source does not have to be dword aligned.
//
#define MM_TRANSFER_DWORD(ppdev, pjMmBase, p, c, Id)                        \
{                                                                           \
    ULONG Count = (c);                                                      \
    ULONG UNALIGNED *Src = (ULONG*)(p);                                     \
    ULONG *Dst = (ULONG *)(pjMmBase);                                       \
    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
                               \
    while (Count--) {                                                       \
        WRITE_REGISTER_ULONG(Dst++, *Src++);                                \
    }                                                                       \
}


//////////////////////////
// MM_TRANSFER_REP_DWORD - transfers a DWORD repeatedly using memory-mapped transfers.
//
// Source has to be dword aligned.
//
#define MM_TRANSFER_REP_DWORD(ppdev, pjMmBase, p, c, Id)                        \
{                                                                           \
    ULONG Count = (c);                                                      \
    ULONG *Src = (ULONG*)(p);                                     \
    ULONG *Dst = (ULONG *)(pjMmBase);                                       \
    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
                               \
    while (Count--) {                                                       \
        WRITE_REGISTER_ULONG(Dst++, *Src);                                \
    }                                                                       \
}

//////////////////////////
// MM_TRANSFER - transfer a specified number of bytes
//
// Always does dword writes, but will only read the specified number
// of bytes.

#define MM_TRANSFER(ppdev, pjMmBase, p, c)                                  \
{                                                                           \
    PUCHAR pDst = (PUCHAR)(pjMmBase);                                       \
    PUCHAR pSrc = (PUCHAR)(p);                                              \
    ULONG Count = (c);                                                      \
    ULONG cDwords;                                                          \
    ULONG FinalBits=0;                                                      \
    ULONG Rotate=0;                                                         \
                                                                            \
    cDwords = Count >> 2;                                                   \
    Count = Count & 0x3;                                                    \
                                                                            \
    while (cDwords--) {                                                     \
        WRITE_REGISTER_ULONG(((PULONG)pDst)++, *((ULONG UNALIGNED *)pSrc)++);\
    }                                                                       \
                                                                            \
    if (Count) {                                                            \
        while (Count--) {                                                   \
            FinalBits |= ((ULONG)(*pSrc++)) << Rotate;                      \
            Rotate += 8;                                                    \
        }                                                                   \
        WRITE_REGISTER_ULONG((PULONG)pDst, FinalBits);                      \
    }                                                                       \
}


////////////////////////////
//// MM_TRANSFER_WORD_ODD - Word transfers for glyphs of odd byte length
////                        and more than one byte wide.
////
//// Source must be word aligned.
//
//
//#define MM_TRANSFER_WORD_ODD(ppdev, pjMmBase, p, cjWidth, cy, Id)           \
//{                                                                           \
//             BYTE*    mpjSrc     = (BYTE*) (p);                             \
//             USHORT** mapwMmXfer = ppdev->apwMmXfer;                        \
//             LONG     mi         = 0;                                       \
//             LONG     mcy        = (cy);                                    \
//             LONG     mcw        = ((cjWidth) >> 1);                        \
//             LONG     mc;                                                   \
//                                                                            \
//    ASSERTDD(((cjWidth) > 0) && ((cy) > 0), "Can't have a zero transfer count");\
//    ASSERTDD((cjWidth) & 1, "Must be odd byte width");                      \
//    ASSERTDD((cjWidth) > 2, "Must be more than 2 bytes wide");              \
//                                                                            \
//    CP_MEMORY_BARRIER();                                                    \
//    do {                                                                    \
//        mc = mcw;                                                           \
//        do {                                                                \
//            WRITE_REGISTER_USHORT(MM(pjMmBase, mapwMmXfer[(mi++) & XFER_MASK]), \
//                                  *((USHORT UNALIGNED *) mpjSrc));          \
//            mpjSrc += 2;                                                    \
//        } while (--mc != 0);                                                \
//                                                                            \
//        WRITE_REGISTER_USHORT(MM(pjMmBase, mapwMmXfer[(mi++) & XFER_MASK]), \
//                              (USHORT) (*(mpjSrc)));                        \
//        mpjSrc++;                                                           \
//    } while (--mcy != 0);                                                   \
//}
//

#endif

/////////////////////////////////////////////////////////////////////////////
// DirectDraw stuff

#define IS_RGB15_R(flRed) \
        (flRed == 0x7c00)

#define IS_RGB15(this) \
   (((this)->dwRBitMask == 0x7c00) && \
    ((this)->dwGBitMask == 0x03e0) && \
    ((this)->dwBBitMask == 0x001f))

#define IS_RGB16(this) \
   (((this)->dwRBitMask == 0xf800) && \
    ((this)->dwGBitMask == 0x07e0) && \
    ((this)->dwBBitMask == 0x001f))

#define IS_RGB24(this) \
   (((this)->dwRBitMask == 0x00ff0000) && \
    ((this)->dwGBitMask == 0x0000ff00) && \
    ((this)->dwBBitMask == 0x000000ff))

#define IS_RGB32(this) \
   (((this)->dwRBitMask == 0x00ff0000) && \
    ((this)->dwGBitMask == 0x0000ff00) && \
    ((this)->dwBBitMask == 0x000000ff))

#define RGB15to32(c) \
   (((c & 0x7c00) << 9) | \
    ((c & 0x03e0) << 6) | \
    ((c & 0x001f) << 3))

#define RGB16to32(c) \
   (((c & 0xf800) << 8) | \
    ((c & 0x07e0) << 5) | \
    ((c & 0x001f) << 3))

#define VBLANK_IS_ACTIVE(ppdev) \
    (VGAINP(ppdev, STATUS_1) & VBLANK_ACTIVE)

#define DISPLAY_IS_ACTIVE(ppdev) \
    (!(VGAINP(ppdev, STATUS_1) & DISPLAY_MODE_INACTIVE))

// #define WAIT_FOR_VBLANK(ppdev) \
//     do {} while (!(VBLANK_IS_ACTIVE(ppdev)));


#define WAIT_FOR_VBLANK(ppdev)                                 \
{                                                              \
   ULONG count;                                                \
                                                               \
   for (count=0xFFFFFFFF; count!=0; count--)                   \
   {                                                           \
      if (VBLANK_IS_ACTIVE(ppdev))                             \
      {                                                        \
          break;                                               \
      }                                                        \
   }                                                           \
}


extern VOID vStreamsDelay();        // Work around 765 timing bug

#define WRITE_STREAM_D(pjMmBase, Register, x)                   \
{                                                               \
    WRITE_REGISTER_ULONG((BYTE*) (pjMmBase) + Register, (x));   \
    CP_EIEIO();                                                 \
    vStreamsDelay();                                            \
}

