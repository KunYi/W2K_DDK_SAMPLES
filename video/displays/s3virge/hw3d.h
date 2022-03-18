
/******************************Module*Header*******************************\
* Module Name: hw3d.h
*
* Contains specific ViRGE prototypes for the display driver.
*
* Copyright (c) 1992-1996 Microsoft Corporation
\**************************************************************************/

//
// This file contains equates pertaining to the S3D engine.
//

#define DLL_NAME            L"S3ViRGE"  // Name of the DLL in UNICODE

//
//  Stream processor register definitions (memory-mapped)
//      (do not count on the restrictions named in the comments)
//

// These memory-mapped registers are valid only for ViRGE and ViRGE-VX
#define STREAMS_FIFO_CONTROL    0x81EC
#define PRI_STREAM_WINDOW_START 0x81F0
// These are valid for ViRGE, ViRGE-VX, and ViRGE-DX/GX
#define PRI_STREAM_CONTROL      0x8180
#define PRI_STREAM_STRIDE       0x81C8
#define PRI_STREAM_WINDOW_SZ    0x81F4

// These are valid for ViRGE, ViRGE-VX, ViRGE-DX/GX, and ViRGE-GX2
#define COLOR_CHROMA_CONTROL    0x8184
#define SEC_STREAM_CONTROL      0x8190
#define CHROMA_UPPER_BOUND      0x8194
#define SEC_STREAM_STRETCH_FLTR 0x8198
// (in HW.H) #define BLEND_CONTROL           0x81A0
#define PRI_STREAM_FBUF_ADDR0   0x81C0
#define PRI_STREAM_FBUF_ADDR1   0x81C4
#define DBL_BUFFER_LPB_SUPPORT  0x81CC
#define SEC_STREAM_FBUF_ADDR0   0x81D0
#define SEC_STREAM_FBUF_ADDR1   0x81D4
#define SEC_STREAM_STRIDE       0x81D8
#define OPAQUE_OVERLAY_CONTROL  0x81DC
#define K1_VERT_SCALE           0x81E0
#define K2_VERT_SCALE           0x81E4
#define DDA_VERT_ACCUM_INIT     0x81E8
#define SEC_STREAM_WINDOW_START 0x81F8
#define SEC_STREAM_WINDOW_SZ    0x81FC

//
//  Memory Port Controller Registers (memory-mapped)
//

// These memory-mapped registers are valid only for ViRGE and ViRGE-VX
#define  FIFO_CONTROL            0x8200  // Stream FIFO and RAS Controls
// These are valid for ViRGE, ViRGE-VX, and ViRGE-DX/GX
#define MIU_CONTROL             0x8204
#define STREAMS_TIMEOUT         0x8208
#define MISC_TIMEOUT            0x820C

//
//  DMA Read Registers, including Video/Graphics Data Transfer Channel,
//  and Command Transfer Channel (memory-mapped)
//

// These are valid for ViRGE, ViRGE-VX, ViRGE-DX/GX, and ViRGE-GX2
#define DMA_READ_BASE           0x8220  // DMA Read Base Address register.
#define DMA_READ_STRIDE_WIDTH   0x8224  // Read Stride/Width Register
#define VIDEO_DMA_START_ADDR    0x8580  // Video DMA Starting System Memory
                                        // Address register.
#define VIDEO_DMA_XFER_LENGTH   0x8584  // Video DMA Transfer Length
#define VIDEO_DMA_XFER_ENABLE   0x8588  // Video DMA Transfer Enable
#define CMD_DMA_BASE_ADDR       0x8590  // Command DMA Base Address
#define CMD_DMA_WRITE_PTR       0x8594  // Command DMA Write Pointer
#define CMD_DMA_READ_PTR        0x8598  // Command DMA Read Pointer
#define CMD_DMA_ENABLE          0x859C  // Command DMA Enable

//
// Miscellaneous Registers (memory-mapped)
//

// These are valid for ViRGE, ViRGE-VX, ViRGE-DX/GX, and ViRGE-GX2
// (see also S3D_SUBSYSTEM_STATUS)
#define SUBSYSTEM_STATUS        0x8504
#define ADV_FUNCTION_CONTROL    0x850C


//
//  ViRGE MMIO offsets for DAC registers
//
#define S3D_DAC_MASK                            (0x83C6L)
#define S3D_DAC_READ_INDEX                      (0x83C7L)
#define S3D_DAC_STATUS                          (0x83C7L)
#define S3D_DAC_WRITE_INDEX                     (0x83C8L)
#define S3D_DAC_DATA                            (0x83C9L)
//
//  ViRGE MMIO offsets for CRTC registers
//
#define S3D_CRTC_INDEX                          (0x83D4L)
#define S3D_CRTC_DATA                           (0x83D5L)
#define S3D_STATUS_1                            (0x83DAL)


#define S3D_COLOR_PATTERN_DATA                  (0x0000A100L)

//
//  Bitblt/Rectangle Register Definitions
//
#define S3D_BLT_SOURCE_BASE                     (0x0000A4D4L)
#define S3D_BLT_DESTINATION_BASE                (0x0000A4D8L)
#define S3D_BLT_CLIP_LEFT_RIGHT                 (0x0000A4DCL)
#define S3D_BLT_CLIP_TOP_BOTTOM                 (0x0000A4E0L)
#define S3D_BLT_DESTINATION_SOURCE_STRIDE       (0x0000A4E4L)
#define S3D_BLT_MONO_PATTERN_0                  (0x0000A4E8L)
#define S3D_BLT_MONO_PATTERN_1                  (0x0000A4ECL)
#define S3D_BLT_PATTERN_BACKGROUND_COLOR        (0x0000A4F0L)
#define S3D_BLT_PATTERN_FOREGROUND_COLOR        (0x0000A4F4L)
#define S3D_BLT_SOURCE_BACKGROUND_COLOR         (0x0000A4F8L)
#define S3D_BLT_SOURCE_FOREGROUND_COLOR         (0x0000A4FCL)
#define S3D_BLT_COMMAND                         (0x0000A500L)
#define S3D_BLT_WIDTH_HEIGHT                    (0x0000A504L)
#define S3D_BLT_SOURCE_X_Y                      (0x0000A508L)
#define S3D_BLT_DESTINATION_X_Y                 (0x0000A50CL)


//
// Line Register Definitions
//
#define S3D_2D_LINE_SOURCE_BASE                 (0x0000A8D4L)
#define S3D_2D_LINE_DESTINATION_BASE            (0x0000A8D8L)
#define S3D_2D_LINE_CLIP_LEFT_RIGHT             (0x0000A8DCL)
#define S3D_2D_LINE_CLIP_TOP_BOTTOM             (0x0000A8E0L)
#define S3D_2D_LINE_DESTINATION_SOURCE_STRIDE   (0x0000A8E4L)
#define S3D_2D_LINE_PATTERN_FOREGROUND_COLOR    (0x0000A8F4L)
#define S3D_2D_LINE_COMMAND                     (0x0000A900L)
#define S3D_2D_LINE_XEND_0_1                    (0x0000A96CL)
#define S3D_2D_LINE_XDELTA                      (0x0000A970L)
#define S3D_2D_LINE_XSTART                      (0x0000A974L)
#define S3D_2D_LINE_YSTART                      (0x0000A978L)
#define S3D_2D_LINE_YCOUNT                      (0x0000A97CL)
#define S3D_2D_LINE_X_POSITIVE                  (0x80000000L)
#define S3D_2D_LINE_X_NEGATIVE                  (0x00000000L)



//
// Polygon Register Definitions
//
#define S3D_POLY_SOURCE_BASE                    (0x0000ACD4L)
#define S3D_POLY_DESTINATION_BASE               (0x0000ACD8L)
#define S3D_POLY_CLIP_LEFT_RIGHT                (0x0000ACDCL)
#define S3D_POLY_CLIP_TOP_BOTTOM                (0x0000ACE0L)
#define S3D_POLY_DESTINATION_SOURCE_STRIDE      (0x0000ACE4L)
#define S3D_POLY_MONO_PATTERN_0                 (0x0000ACE8L)
#define S3D_POLY_MONO_PATTERN_1                 (0x0000ACECL)
#define S3D_POLY_PATTERN_BACKGROUND_COLOR       (0x0000ACF0L)
#define S3D_POLY_PATTERN_FOREGROUND_COLOR       (0x0000ACF4L)
#define S3D_POLY_COMMAND                        (0x0000AD00L)
#define S3D_POLY_RIGHT_X_DELTA                  (0x0000AD68L)
#define S3D_POLY_RIGHT_X_START                  (0x0000AD6CL)
#define S3D_POLY_LEFT_X_DELTA                   (0x0000AD70L)
#define S3D_POLY_LEFT_X_START                   (0x0000AD74L)
#define S3D_POLY_Y_START                        (0x0000AD78L)
#define S3D_POLY_Y_COUNT                        (0x0000AD7CL)


//
// 2D Command Register Bit Definitions
//
#define S3D_AUTOEXECUTE                         (0x00000001L)
#define S3D_HARDWARE_CLIPPING                   (0x00000002L)

#define S3D_DRAW_ENABLE                         (0x00000020L)
#define S3D_MONOCHROME_SOURCE                   (0x00000040L)
#define S3D_CPU_DATA                            (0x00000080L)
#define S3D_MONOCHROME_PATTERN                  (0x00000100L)
#define S3D_TRANSPARENT                         (0x00000200L)

#define S3D_BYTE_ALIGNED                        (0x00000000L)
#define S3D_WORD_ALIGNED                        (0x00000400L)
#define S3D_DWORD_ALIGNED                       (0x00000800L)

#define S3D_FDO_ENTIRE_DWORD                    (0x00000000L)
#define S3D_FDO_2ND_BYTE                        (0x00001000L)
#define S3D_FDO_3RD_BYTE                        (0x00002000L)
#define S3D_FDO_4TH_BYTE                        (0x00003000L)

#define S3D_ROP_SHIFT                           (0x00000011L)

#define S3D_X_POSITIVE_BLT                      (0x02000000L)
#define S3D_X_NEGATIVE_BLT                      (0x00000000L)
#define S3D_Y_POSITIVE_BLT                      (0x04000000L)
#define S3D_Y_NEGATIVE_BLT                      (0x00000000L)

#define S3D_BITBLT                              (0x00000000L)
#define S3D_RECTANGLE                           (0x10000000L)
#define S3D_LINE_2D                             (0x18000000L)
#define S3D_POLYGON                             (0x28000000L)
#define S3D_NOP                                 (0x78000000L)


//
// 3D Line Register Definitions in MMIO Space.
//
#define S3D_3D_LINE_Z_BASE                      (0x0000B0D4L)
#define S3D_3D_LINE_DESTINATION_BASE            (0x0000B0D8L)
#define S3D_3D_LINE_CLIP_LEFT_RIGHT             (0x0000B0DCL)
#define S3D_3D_LINE_CLIP_TOP_BOTTOM             (0x0000B0E0L)
#define S3D_3D_LINE_DESTINATION_SOURCE_STRIDE   (0x0000B0E4L)
#define S3D_3D_LINE_Z_STRIDE                    (0x0000B0E8L)
#define S3D_3D_LINE_FOG_COLOR                   (0x0000B0F4L)
#define S3D_3D_LINE_COMMAND                     (0x0000B100L)
#define S3D_3D_LINE_GREEN_BLUE_DELTA            (0x0000B144L)
#define S3D_3D_LINE_ALPHA_RED_DELTA             (0x0000B148L)
#define S3D_3D_LINE_GREEN_BLUE_START            (0x0000B14CL)
#define S3D_3D_LINE_ALPHA_RED_START             (0x0000B150L)
#define S3D_3D_LINE_Z_DELTA                     (0x0000B158L)
#define S3D_3D_LINE_Z_START                     (0x0000B15CL)
#define S3D_3D_LINE_X_END_0_1                   (0x0000B16CL)
#define S3D_3D_LINE_X_DELTA                     (0x0000B170L)
#define S3D_3D_LINE_X_START                     (0x0000B174L)
#define S3D_3D_LINE_Y_START                     (0x0000B178L)
#define S3D_3D_LINE_Y_COUNT                     (0x0000B17CL)


//
// 3D Line Register Definitions in DMA Space. These are OFFSETS relative
// to the starting address written to the DMA header. Although the data
// book(page 15-34) says that the gaps in the register map can be filled
// with garbage values, however, this is not true. Filling the gap with
// 0s or garbage will render the engine into some unknown states. That's
// why we need to broken down the 3D line command into multiple batches.
//
#define DMA_3D_LINE_CLIP_LEFT_RIGHT             (0x00000000L)   // B0DCL
#define DMA_3D_LINE_CLIP_TOP_BOTTOM             (0x00000004L)   // B0E0L

#define DMA_3D_LINE_COMMAND                     (0x00000000L)   // B100L

#define DMA_3D_LINE_GREEN_BLUE_DELTA            (0x00000000L)   // B144L
#define DMA_3D_LINE_ALPHA_RED_DELTA             (0x00000004L)   // B148L
#define DMA_3D_LINE_GREEN_BLUE_START            (0x00000008L)   // B14CL
#define DMA_3D_LINE_ALPHA_RED_START             (0x0000000CL)   // B150L

#define DMA_3D_LINE_Z_DELTA                     (0x00000000L)   // B158L
#define DMA_3D_LINE_Z_START                     (0x00000004L)   // B15CL

#define DMA_3D_LINE_X_END_0_1                   (0x00000000L)   // B16CL
#define DMA_3D_LINE_X_DELTA                     (0x00000004L)   // B170L
#define DMA_3D_LINE_X_START                     (0x00000008L)   // B174L
#define DMA_3D_LINE_Y_START                     (0x0000000CL)   // B178L
#define DMA_3D_LINE_Y_COUNT                     (0x00000010L)   // B17CL


//
// 3D Triangle Register Definitions in MMIO Space.
//
#define S3D_TRI_Z_BASE                          (0x0000B4D4L)
#define S3D_TRI_DESTINATION_BASE                (0x0000B4D8L)
#define S3D_TRI_CLIP_LEFT_RIGHT                 (0x0000B4DCL)
#define S3D_TRI_CLIP_TOP_BOTTOM                 (0x0000B4E0L)
#define S3D_TRI_DESTINATION_SOURCE_STRIDE       (0x0000B4E4L)
#define S3D_TRI_Z_STRIDE                        (0x0000B4E8L)
#define S3D_TRI_TEXTURE_BASE                    (0x0000B4ECL)
#define S3D_TRI_TEXTURE_BORDER_COLOR            (0x0000B4F0L)
#define S3D_TRI_FOG_COLOR                       (0x0000B4F4L)
#define S3D_TRI_COLOR_0                         (0x0000B4F8L)
#define S3D_TRI_COLOR_1                         (0x0000B4FCL)
#define S3D_TRI_COMMAND                         (0x0000B500L)
#define S3D_TRI_BASE_V                          (0x0000B504L)
#define S3D_TRI_BASE_U                          (0x0000B508L)
#define S3D_TRI_W_X_DELTA                       (0x0000B50CL)
#define S3D_TRI_W_Y_DELTA                       (0x0000B510L)
#define S3D_TRI_W_START                         (0x0000B514L)
#define S3D_TRI_D_X_DELTA                       (0x0000B518L)
#define S3D_TRI_V_X_DELTA                       (0x0000B51CL)
#define S3D_TRI_U_X_DELTA                       (0x0000B520L)
#define S3D_TRI_D_Y_DELTA                       (0x0000B524L)
#define S3D_TRI_V_Y_DELTA                       (0x0000B528L)
#define S3D_TRI_U_Y_DELTA                       (0x0000B52CL)
#define S3D_TRI_D_START                         (0x0000B530L)
#define S3D_TRI_V_START                         (0x0000B534L)
#define S3D_TRI_U_START                         (0x0000B538L)
#define S3D_TRI_GREEN_BLUE_X_DELTA              (0x0000B53CL)
#define S3D_TRI_ALPHA_RED_X_DELTA               (0x0000B540L)
#define S3D_TRI_GREEN_BLUE_Y_DELTA              (0x0000B544L)
#define S3D_TRI_ALPHA_RED_Y_DELTA               (0x0000B548L)
#define S3D_TRI_GREEN_BLUE_START                (0x0000B54CL)
#define S3D_TRI_ALPHA_RED_START                 (0x0000B550L)
#define S3D_TRI_Z_X_DELTA                       (0x0000B554L)
#define S3D_TRI_Z_Y_DELTA                       (0x0000B558L)
#define S3D_TRI_Z_START                         (0x0000B55CL)
#define S3D_TRI_X_Y_1_2_DELTA                   (0x0000B560L)
#define S3D_TRI_X_1_2_END                       (0x0000B564L)
#define S3D_TRI_X_Y_0_1_DELTA                   (0x0000B568L)
#define S3D_TRI_X_0_1_END                       (0x0000B56CL)
#define S3D_TRI_X_Y_0_2_DELTA                   (0x0000B570L)
#define S3D_TRI_X_START                         (0x0000B574L)
#define S3D_TRI_Y_START                         (0x0000B578L)
#define S3D_TRI_Y_COUNT                         (0x0000B57CL)


//
// 3D Triangle Register Definitions in DMA Space. These are offsets
// relative to the starting address written to DMA header. The commented
// address following each define is each register's MMIO address.
//
#define DMA_TRI_CLIP_LEFT_RIGHT                 (0x00000000L)   // B4DCL
#define DMA_TRI_CLIP_TOP_BOTTOM                 (0x00000004L)   // B4E0L

#define DMA_TRI_COMMAND                         (0x00000000L)   // B500L

#define DMA_TRI_BASE_V                          (0x00000000L)   // B504L
#define DMA_TRI_BASE_U                          (0x00000004L)   // B508L
#define DMA_TRI_W_X_DELTA                       (0x00000008L)   // B50CL
#define DMA_TRI_W_Y_DELTA                       (0x0000000CL)   // B510L
#define DMA_TRI_W_START                         (0x00000010L)   // B514L
#define DMA_TRI_D_X_DELTA                       (0x00000014L)   // B518L
#define DMA_TRI_V_X_DELTA                       (0x00000018L)   // B51CL
#define DMA_TRI_U_X_DELTA                       (0x0000001CL)   // B520L
#define DMA_TRI_D_Y_DELTA                       (0x00000020L)   // B524L
#define DMA_TRI_V_Y_DELTA                       (0x00000024L)   // B528L
#define DMA_TRI_U_Y_DELTA                       (0x00000028L)   // B52CL
#define DMA_TRI_D_START                         (0x0000002CL)   // B530L
#define DMA_TRI_V_START                         (0x00000030L)   // B534L
#define DMA_TRI_U_START                         (0x00000034L)   // B538L
#define DMA_TRI_GREEN_BLUE_X_DELTA              (0x00000038L)   // B53CL
#define DMA_TRI_ALPHA_RED_X_DELTA               (0x0000003CL)   // B540L
#define DMA_TRI_GREEN_BLUE_Y_DELTA              (0x00000040L)   // B544L
#define DMA_TRI_ALPHA_RED_Y_DELTA               (0x00000044L)   // B548L
#define DMA_TRI_GREEN_BLUE_START                (0x00000048L)   // B54CL
#define DMA_TRI_ALPHA_RED_START                 (0x0000004CL)   // B550L
#define DMA_TRI_Z_X_DELTA                       (0x00000050L)   // B554L
#define DMA_TRI_Z_Y_DELTA                       (0x00000054L)   // B558L
#define DMA_TRI_Z_START                         (0x00000058L)   // B55CL
#define DMA_TRI_X_Y_1_2_DELTA                   (0x0000005CL)   // B560L
#define DMA_TRI_X_1_2_END                       (0x00000060L)   // B564L
#define DMA_TRI_X_Y_0_1_DELTA                   (0x00000064L)   // B568L
#define DMA_TRI_X_0_1_END                       (0x00000068L)   // B56CL
#define DMA_TRI_X_Y_0_2_DELTA                   (0x0000006CL)   // B570L
#define DMA_TRI_X_START                         (0x00000070L)   // B574L
#define DMA_TRI_Y_START                         (0x00000074L)   // B578L
#define DMA_TRI_Y_COUNT                         (0x00000078L)   // B57CL


//
// 3D Command Register Bit Definitions
//
#define S3D_AUTOEXECUTE                         (0x00000001L)
#define S3D_HARDWARE_CLIPPING                   (0x00000002L)

#define S3D_DEST_8BPP_INDX                      (0x00000000L)
#define S3D_DEST_16BPP_1555                     (0x00000004L)
#define S3D_DEST_24BPP_888                      (0x00000008L)

#define S3D_TEXTURE_32BPP_8888                  (0x00000000L)
#define S3D_TEXTURE_16BPP_4444                  (0x00000020L)
#define S3D_TEXTURE_16BPP_1555                  (0x00000040L)
#define S3D_TEXTURE_8BPP_44                     (0x00000060L)
#define S3D_TEXTURE_4BPP_4LOW                   (0x00000080L)
#define S3D_TEXTURE_4BPP_4HIGH                  (0x000000A0L)
#define S3D_TEXTURE_8BPP_PALETTE                (0x000000C0L)
#define S3D_TEXTURE_YU_VU                       (0x000000E0L)

#define S3D_MIPMAP_LEVEL_SIZE_SHIFT             (0x00000008L)

#define S3D_TEXTURE_FILTER_MIP_NEAREST          (0x00000000L)
#define S3D_TEXTURE_FILTER_LINEAR_MIP_NEAREST   (0x00001000L)
#define S3D_TEXTURE_FILTER_MIP_LINEAR           (0x00002000L)
#define S3D_TEXTURE_FILTER_LINEAR_MIP_LINEAR    (0x00003000L)
#define S3D_TEXTURE_FILTER_NEAREST              (0x00004000L)
#define S3D_TEXTURE_FILTER_YU_VU                (0x00005000L)
#define S3D_TEXTURE_FILTER_LINEAR               (0x00006000L)
#define S3D_TEXTURE_FILTER_RESERVED             (0x00007000L)

#define S3D_TEXTURE_BLEND_COMPLEX_REFLECTION    (0x00000000L)
#define S3D_TEXTURE_BLEND_MODULATE              (0x00008000L)
#define S3D_TEXTURE_BLEND_DECAL                 (0x00010000L)

#define S3D_FOG_ENABLE                          (0x00020000L)

#define S3D_ALPHA_BLEND_NONE                    (0x00000000L)
#define S3D_ALPHA_BLEND_TEXTURE_ALPHA           (0x00080000L)
#define S3D_ALPHA_BLEND_SOURCE_ALPHA            (0x000C0000L)

#define S3D_Z_COMP_NEVER                        (0x00000000L)
#define S3D_Z_COMP_S_GT_B                       (0x00100000L)
#define S3D_Z_COMP_S_EQ_B                       (0x00200000L)
#define S3D_Z_COMP_S_GE_B                       (0x00300000L)
#define S3D_Z_COMP_S_LT_B                       (0x00400000L)
#define S3D_Z_COMP_S_NE_B                       (0x00500000L)
#define S3D_Z_COMP_S_LE_B                       (0x00600000L)
#define S3D_Z_COMP_ALWAYS                       (0x00700000L)

#define S3D_Z_BUFFER_UPDATE                     (0x00800000L)

#define S3D_Z_BUFFER_NORMAL                     (0x00000000L)
#define S3D_Z_MUX_Z_PASS                        (0x01000000L)
#define S3D_Z_MUX_DRAW_PASS                     (0x02000000L)
#define S3D_NO_Z_BUFFERING                      (0x03000000L)

#define S3D_TEXTURE_WRAP_ENABLE                 (0x04000000L)


#define S3D_GOURAUD_SHADED_TRIANGLE             (0x00000000L)
#define S3D_LIT_TEXTURED_TRIANGLE               (0x08000000L)
#define S3D_UNLIT_TEXTURED_TRIANGLE             (0x10000000L)
#define S3D_PERSPECTIVE_LIT_TEXTURED_TRIANGLE   (0x28000000L)
#define S3D_PERSPECTIVE_UNLIT_TEXTURED_TRIANGLE (0x30000000L)
#define S3D_LINE_3D                             (0x40000000L)
#define S3D_NOP_3D                              (0x78000000L)

#define S3D_COMMAND_3D                          (0x80000000L)

#define S3D_SUBSYSTEM_STATUS                    (0x00008504L)

//
// Defines for S3D_SUBSYSTEM_STATUS register
//
//  S3D_ENGINE_IDLE okay for ViRGE, V-VX, V-DX/GX, V-GX2.
//
#define S3D_ENGINE_IDLE                         (0x00002000L)
//
//  S3D_FIFO_SLOTSFREE_MASK okay for ViRGE, V-VX, V-DX/GX (16 slots)
//  For V-GX2, there are 64 slots and additional bits to check.
//
#define S3D_FIFO_SLOTSFREE_MASK         0x1F
#define S3D_SIXTYFOUR_SLOTSFREE         0x8000


// A couple of important ROPs for us
//
#define PAT_COPY        0xF0
#define SRC_COPY        0xCC
#define DEST_PAT_XOR    0x5A
#define DEST_NOT        0x55
#define ZERO_ROP        0

//
// 3D DMA Register Definitions.
//
#define S3D_DMA_COMMAND_BASE                    (0x00008590L)
#define S3D_DMA_WRITE_POINTER                   (0x00008594L)
#define S3D_DMA_READ_POINTER                    (0x00008598L)
#define S3D_DMA_ENABLE_REG                      (0x0000859CL)

//
// 3D Command Base Register Bit Definitions.
//
#define S3D_DMA_4K_BUFFER                       (0x00000000L)
#define S3D_DMA_64K_BUFFER                      (0x00000002L)

//
// 3D Command DMA Enable Register Bit Definitions.
//
#define S3D_DMA_ENABLE                          (0x00000001L)
#define S3D_DMA_DISABLE                         (0x00000000L)

//
// 3D Command DMA Write Pointer Register Bit Definitions.
//
#define S3D_DMA_WRITE_PTR_UPD                   (0x00010000L)


//
// Macros used to access the S3D engine registers
//

#define S3writeHIU( ppdev, MMIO_Offset, Value ) \
    WRITE_REGISTER_ULONG( (BYTE *)(ppdev->pjMmBase + MMIO_Offset ), Value)

#define S3readHIU( ppdev, MMIO_Offset ) \
    (READ_REGISTER_ULONG( (BYTE *)(ppdev->pjMmBase + MMIO_Offset )))

#define NW_SET_REG(ppdev, pjMmBase, RegAddr, RegData)         \
    WRITE_REGISTER_ULONG( (BYTE*) (ppdev->pjMmBase + RegAddr), (RegData) )

/*

#define NW_ABS_CURXY_FAST(ppdev, pjMmBase, x, y)        \
    WRITE_REGISTER_ULONG( (BYTE *)(ppdev->pjMmBase + S3D_BLT_SOURCE_X_Y ),      \
                          ((x) << 16) | (y) )

#define NW_ABS_DESTXY_FAST(ppdev, pjMmBase, x, y)        \
    WRITE_REGISTER_ULONG( (BYTE *)(ppdev->pjMmBase + S3D_BLT_DESTINATION_X_Y ), \
                          ((x) << 16) | (y) )

#define NW_ALT_PCNT(ppdev, pjMmBase, x, y)        \
    WRITE_REGISTER_ULONG( (BYTE *)(ppdev->pjMmBase + S3D_BLT_WIDTH_HEIGHT ),    \
                          ((x) << 16) | (y) )

#define NW_ALT_CMD(ppdev, pjMmBase, x)                  \
    WRITE_REGISTER_ULONG( (BYTE *)(ppdev->pjMmBase + S3D_BLT_COMMAND ), \
                          (x) )

*/

#define ENABLE_HWSAVEMODE1 0x00002000
#define ENABLE_HWSAVEMODE2 0x00004000
#define ENABLE_HWVFILTER   0x00008000

#if !DBG

    #if !M5_DISABLE_POWER_DOWN_CONTROL

        #define S3DGPWait(ppdev)\
        while (!(S3readHIU(ppdev, S3D_SUBSYSTEM_STATUS) & S3D_ENGINE_IDLE));

    #else

        __inline VOID S3DGPWait( PDEV * ppdev )
        {
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

                while( !(S3readHIU( ppdev, S3D_SUBSYSTEM_STATUS ) &
                            S3D_ENGINE_IDLE ) );

                //
                // restore engine power down control, and extend s3d engine busy
                //

                VGAOUTP( ppdev, SEQ_INDEX, 0x0A );
                VGAOUTP( ppdev, SEQ_DATA, VGAINP( ppdev, SEQ_DATA ) | 0x40 );
                VGAOUTP( ppdev, SEQ_INDEX, 0x09 );
                VGAOUTP( ppdev, SEQ_DATA, VGAINP( ppdev, SEQ_DATA ) | 0x02 );

//NOLOCK                VGAOUTPW( ppdev, SEQ_INDEX, 0x0008 );

                RELEASE_CRTC_CRITICAL_SECTION( ppdev );
            }
            else
            {
                while( !(S3readHIU( ppdev, S3D_SUBSYSTEM_STATUS ) & S3D_ENGINE_IDLE));
            }

            return;
        }

    #endif

#else

    VOID    vS3DGPWait(PDEV*);
    #define S3DGPWait(ppdev)            vS3DGPWait((ppdev))

#endif

//
// Wait till DMA is idle, before issuing any more MMIO commands. This inline
// macro is used when SUPPORT_MCD is defined. The macros are added to the
// 2D driver entry points and DirectDraw, to synchronize engine DMA commands.
//
#if SUPPORT_MCD
__inline void WAIT_DMA_IDLE(PDEV *ppdev)
{
    ULONG ulWritePtr;

    //
    // If DMA is turned on, then wait until the DMA is idle before issueing
    // any MMIO commands, msu.
    //

    //
    // check only b3DHwUsed since the BOOLEAN is set during START_DMA
    // and is only turned on for DMA use.  No need to check DMA bit on
    // register.
    //

    if (ppdev->b3DDMAHwUsed)
    {
        if (ppdev->pjDmaBuffer)
        {
            ulWritePtr = S3readHIU(ppdev, S3D_DMA_WRITE_POINTER) & 0xFFFC;

            while ( (S3readHIU(ppdev, S3D_DMA_READ_POINTER) & 0xFFFC) !=
                    ulWritePtr );
        }
        ppdev->b3DDMAHwUsed = FALSE;
    }
}

__inline void TRIANGLE_WORKAROUND( PDEV *ppdev )
{
    if (ppdev->fTriangleFix)
    {
        ppdev->fTriangleFix = FALSE;

        S3DGPWait(ppdev);
        S3writeHIU(ppdev,S3D_BLT_SOURCE_BASE,0L);
        S3writeHIU(ppdev,S3D_BLT_DESTINATION_BASE,0L);
        S3writeHIU(ppdev,S3D_BLT_DESTINATION_SOURCE_STRIDE, ppdev->lDelta << 16);
        S3writeHIU(ppdev,S3D_BLT_WIDTH_HEIGHT, 1);
        S3writeHIU(ppdev,S3D_BLT_DESTINATION_X_Y, 0 );
        S3writeHIU(ppdev,S3D_BLT_SOURCE_X_Y, 0 );

        S3writeHIU( ppdev,
                    S3D_BLT_COMMAND,
                    ppdev->ulCommandBase      |
                    SRC_COPY << S3D_ROP_SHIFT |
                    S3D_BITBLT                |
                    S3D_DRAW_ENABLE           |
                    S3D_X_POSITIVE_BLT        |
                    S3D_Y_POSITIVE_BLT );

        S3DGPWait( ppdev );
    }
}
#else
#define WAIT_DMA_IDLE(ppdev)
#define TRIANGLE_WORKAROUND( ppdev )
#endif

//
//  fWaitForVsyncActiveIGA2 ()
//      Assumes caller has set SR26 for IGA2 (0x4f)
//      Assumes caller has already set CR index to 0x33.
//      Assumes caller has ACQUIRE_CRTC_CRITICAL_SECTION!
//

__inline BOOL fWaitForVsyncActiveIGA2 ( PDEV * ppdev, BOOL fStatus)
{
    ULONG       ulLoopCount;

    if (fStatus)
    {
        for (ulLoopCount = 0; ulLoopCount < VSYNC_TIMEOUT_VALUE; ulLoopCount++)
        {
            if ( VGAINP( ppdev, CRTC_DATA ) & VSYNC_ACTIVE )
            {
                break;
            }
        }

        if (ulLoopCount == VSYNC_TIMEOUT_VALUE)
        {
            fStatus = FALSE;
        }
    }

    return (fStatus);
}

//
//  fWaitForVsyncInactiveIGA2 ()
//      Assumes caller has set SR26 for IGA2 (0x4f)
//      Assumes caller has already set CR index to 0x33.
//      Assumes caller has ACQUIRE_CRTC_CRITICAL_SECTION!
//

__inline BOOL fWaitForVsyncInactiveIGA2 ( PDEV * ppdev, BOOL fStatus )
{
    ULONG       ulLoopCount;

    if (fStatus)
    {
        for (ulLoopCount = 0; ulLoopCount < VSYNC_TIMEOUT_VALUE; ulLoopCount++)
        {
            if ( !(VGAINP( ppdev, CRTC_DATA ) & VSYNC_ACTIVE) )
            {
                break;
            }
        }

        if (ulLoopCount == VSYNC_TIMEOUT_VALUE)
        {
            fStatus = FALSE;
        }
    }
    return (fStatus);
}

//
//  vWriteCrIndexWithShadow
//      Assumes SR26 set for IGA2 (0x4f)
//

__inline VOID vWriteCrIndexWithShadow ( PDEV *ppdev, BYTE bIndex )
{
    VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA1 );
    VGAOUTPW( ppdev, SEQ_INDEX, ( ( (WORD)bIndex  << 8) | IGA2_3D4_SHADOW_SEQREG) );
    VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA2_READS_WRITES );
    VGAOUTP( ppdev, CRTC_INDEX, bIndex );
}

//
//  vReadCrDataWithShadow
//      Assumes SR26 set for IGA2 (0x4f)
//      Does not preserve original index!
//

__inline BYTE bReadCrDataWithShadow ( PDEV *ppdev, BYTE bIndex )
{
    BYTE    bData;

    vWriteCrIndexWithShadow( ppdev, bIndex );
    bData = VGAINP( ppdev, CRTC_DATA );

    return bData;
}

__inline UCHAR ReadCrReg( PDEV* ppdev, UCHAR Index )
{
    UCHAR   OldIndex;
    UCHAR   Data;

    OldIndex = VGAINP( ppdev, CRTC_INDEX );
    VGAOUTP( ppdev, CRTC_INDEX, Index);
    Data = VGAINP( ppdev, CRTC_DATA );
    VGAOUTP( ppdev, CRTC_INDEX, OldIndex);

    return Data;
}

__inline VOID WriteCrReg( PDEV* ppdev, UCHAR Index, UCHAR Data )
{
    VGAOUTP( ppdev, CRTC_INDEX, Index);
    VGAOUTP( ppdev, CRTC_DATA, Data);
    return;
}

__inline UCHAR ReadSrReg( PDEV* ppdev, UCHAR Index )
{
    UCHAR   OldIndex;
    UCHAR   Data;

    OldIndex = VGAINP( ppdev, SEQ_INDEX );

    // unlock SR register

    VGAOUTPW(ppdev, SEQ_INDEX, SEQ_REG_UNLOCK);

    // Read from SR register

    VGAOUTP(ppdev, SEQ_INDEX, Index);
    Data = VGAINP(ppdev, SEQ_DATA);

    // restore old index

    VGAOUTP(ppdev, SEQ_INDEX, OldIndex);

    return Data;
}

__inline VOID WriteSrReg( PDEV * ppdev, BYTE bSRreg, BYTE bValue )
{
    BYTE    bOldValue;

    bOldValue = VGAINP( ppdev, SEQ_INDEX );

    VGAOUTPW( ppdev, SEQ_INDEX, SEQ_REG_UNLOCK );

    VGAOUTPW( ppdev, SEQ_INDEX, (bValue << 8) | bSRreg );

    VGAOUTP( ppdev, SEQ_INDEX, bOldValue );

    return;
}

__inline LONG GetRefFreq(PDEV* ppdev)
{
    //
    // 27000 is only for Mobile products (M5) currently
    //

    if ( ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE &&
         ppdev->ulCaps2 & CAPS2_LCD_SUPPORT )
    {
        return 27000;
    }
    else
    {
        return 14318;
    }
}

//
//  vSynchVerticalCount
//      Re-Synchronize Vertical counter after assigning SP to an IGA
//      for Vertical Interpolation support.
//
//      Assumes caller has ACQUIRE_CRTC_CRITICAL_SECTION!
//      Assumes already checked for ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE.
//
//      Parameter bIGA_Select is the contents of the read SR reg ARCH_CONFIG_REG,
//      which determines what IGA is being used by the secondary stream!
//

__inline void vSynchVerticalCount ( PDEV * ppdev, BYTE bIGA_Select )
{
    BOOL    fStatus;

    //
    // Setup special CR39 lock 0A5h for accessing CTR26 (synchronization),
    // making sure to save the original value!
    //

    VGAOUTPW(ppdev, CRTC_INDEX, ((SYSCTL_UNLOCK_EXTRA << 8) | EXTREG_LOCK2));

    //
    // Determine what IGA stream is currently assigned to...
    // and wait for start of vertical display...
    //

    if ( bIGA_Select & STREAMS_ON_IGA2 )
    {
        //
        // Select IGA2 read writes...
        //

        VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA2_READS_WRITES );

        //
        // Shadow CRTC index for IB workaround.
        // We need index at CR33 for vsynch waits...
        //

        vWriteCrIndexWithShadow( ppdev,  BACKWARD_COMPAT2_REG );

        //
        // Wait for START OF Display Active
        //

        fStatus = TRUE;
        fStatus = fWaitForVsyncActiveIGA2  ( ppdev, fStatus );
        fStatus = fWaitForVsyncInactiveIGA2( ppdev, fStatus );

        //
        // We are now at the beginning of the active display start.
        // Thus, synchronize the vertical counter now by writing 0
        // to CTR26.
        //

        VGAOUTPW(ppdev, CRTC_INDEX, SYNCH_2);

        //
        // Restore to required default of IGA1!
        //

        VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA1 );

    }
    else
    {
        //
        // Wait for START OF Display Active
        //

        while (!(VBLANK_IS_ACTIVE(ppdev)))
            ;
        while (VBLANK_IS_ACTIVE(ppdev))
            ;

        //
        // We are now at the beginning of the active display start.
        // Thus, synchronize the vertical counter now by writing 0
        // to CTR26.
        //

        VGAOUTPW(ppdev, CRTC_INDEX, SYNCH_2);

    }


    //
    // Restore to normal CR39 lock 0A0h
    //

    VGAOUTPW(ppdev, CRTC_INDEX, ((SYSCTL_UNLOCK << 8) | EXTREG_LOCK2));

}

