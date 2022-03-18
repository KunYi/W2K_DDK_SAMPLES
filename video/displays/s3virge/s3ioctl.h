//******************************Module*Header********************************
//*  Module Name: s3ioctl.h
//*
//*  Private IOCTLs from display driver to inform the miniport accessing
//*  CRTC registers
//*
//*  Copyright (c) Microsoft 1998, All Rights Reserved
//*
//***************************************************************************//

//
// Please KEEP IOCtls SORTED BY NUMBER!!!!!!
//

#define GENERAL_S3_BASE     0x800
#define MOBILE_IOCTL_BASE   0x900
#define S3_IOCTL(x)         CTL_CODE( FILE_DEVICE_VIDEO, \
                                      (x),               \
                                      METHOD_BUFFERED,   \
                                      FILE_ANY_ACCESS)

//
// General S3
//

#define IOCTL_VIDEO_S3_ACQUIRE_CRTC                                     \
    S3_IOCTL( GENERAL_S3_BASE + 0x0 )

#define IOCTL_VIDEO_S3_RELEASE_CRTC                                     \
    S3_IOCTL( GENERAL_S3_BASE + 0x1 )

#define IOCTL_VIDEO_S3_START_TIMER                                      \
    S3_IOCTL( GENERAL_S3_BASE + 0x2 )

#define IOCTL_VIDEO_S3_STOP_TIMER                                       \
    S3_IOCTL( GENERAL_S3_BASE + 0x3 )

#define IOCTL_VIDEO_S3_UMA_FUNCTION                                     \
    S3_IOCTL( GENERAL_S3_BASE + 0x4 )

#define IOCTL_VIDEO_S3_GET_DMA_BUFFER                                   \
    S3_IOCTL( GENERAL_S3_BASE + 0x5 )

//
//  Return DDC EDID information
//

#define IOCTL_VIDEO_S3_GET_EDID                                         \
    S3_IOCTL( GENERAL_S3_BASE + 0x6 )

#define IOCTL_VIDEO_S3_QUERY_STREAMS_PARAMETERS                         \
    S3_IOCTL( GENERAL_S3_BASE + 0x7 )

#define IOCTL_VIDEO_S3_QUERY_BANDWIDTH                                  \
    S3_IOCTL( GENERAL_S3_BASE + 0x8 )

#define IOCTL_VIDEO_S3_QUERY_STREAMS_STATE                              \
    S3_IOCTL( GENERAL_S3_BASE + 0x9 )

#define IOCTL_VIDEO_S3_GET_PHYSICAL_ADDRESS                             \
    S3_IOCTL( GENERAL_S3_BASE + 0xA )

#define IOCTL_VIDEO_S3_SHARE_ADDRESS                                    \
    S3_IOCTL( GENERAL_S3_BASE + 0xB )

#if SUPPORT_MIF

#define IOCTL_VIDEO_MIF_GETVIDEOINFO                                    \
    S3_IOCTL( GENERAL_S3_BASE + 0xC )

#define IOCTL_VIDEO_MIF_GETBIOSINFO                                     \
    S3_IOCTL( GENERAL_S3_BASE + 0xD )

#endif // SUPPORT_MIF


#define IOCTL_VIDEO_S3_GET_CLOCKS										\
    S3_IOCTL( GENERAL_S3_BASE + 0x11 )

#define IOCTL_VIDEO_S3_GET_REGISTRY_VALUE								\
    S3_IOCTL( GENERAL_S3_BASE + 0x12 )

//
// Mobile
//

#define IOCTL_VIDEO_S3_MBL_GET_PANEL_INFO                               \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x0 )

#define IOCTL_VIDEO_S3_MBL_GET_DISPLAY_CONTROL                          \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x1 )

#define IOCTL_VIDEO_S3_MBL_SET_DISPLAY_CONTROL                          \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x2 )

#define IOCTL_VIDEO_S3_MBL_GET_CENTEREXPAND_MODE                        \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x3 )

#define IOCTL_VIDEO_S3_MBL_SET_CENTEREXPAND_MODE                        \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x4 )

#define IOCTL_VIDEO_S3_MBL_GET_TV_FFILTER_STATUS                        \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x5 )

#define IOCTL_VIDEO_S3_MBL_SET_TV_FFILTER_STATUS                        \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x6 )

#define IOCTL_VIDEO_S3_MBL_GET_CONNECT_STATUS                           \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x7 )

#define IOCTL_VIDEO_S3_MBL_GET_CRT_PAN_RES                              \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x9 )

#define IOCTL_VIDEO_S3_MBL_SET_CRT_PAN_RES                              \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0xA )

#define IOCTL_VIDEO_S3_MBL_GET_TIMING_STATE                             \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0xB )

#define IOCTL_VIDEO_S3_MBL_SET_TIMING_STATE                             \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0xC )

#define IOCTL_VIDEO_S3_MBL_GET_IMAGE_STATE                              \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0xD )

#define IOCTL_VIDEO_S3_MBL_SET_IMAGE_STATE                              \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0xE )

#define IOCTL_VIDEO_S3_MBL_GET_PRIMARY_DEVICE                           \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0xF )

#define IOCTL_VIDEO_S3_MBL_SET_PRIMARY_DEVICE                           \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x10 )

#define IOCTL_VIDEO_S3_MBL_GET_CRT_CAPABILITY                           \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x11 )

#define IOCTL_VIDEO_S3_MBL_SET_CRT_CAPABILITY                           \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x12 )

#define IOCTL_VIDEO_S3_MBL_GET_TV_UNDERSCAN_STATUS                      \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x13 )

#define IOCTL_VIDEO_S3_MBL_SET_TV_UNDERSCAN_STATUS                      \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x14 )

#define IOCTL_VIDEO_S3_MBL_GET_TV_OUTPUT_TYPE                           \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x15 )

#define IOCTL_VIDEO_S3_MBL_SET_TV_OUTPUT_TYPE                           \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x16 )

#define IOCTL_VIDEO_S3_MBL_GET_TV_POSITION                              \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x17 )

#define IOCTL_VIDEO_S3_MBL_SET_TV_POSITION                              \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x18 )

#define IOCTL_VIDEO_S3_MBL_GET_TV_CENTEROPTION                          \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x19 )

#define IOCTL_VIDEO_S3_MBL_SET_TV_CENTEROPTION                          \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x1A )

#define IOCTL_VIDEO_S3_MBL_GET_RESOLUTION_TABLE                         \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x1B )

#define IOCTL_VIDEO_S3_MBL_GET_CHIPID                                   \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x1C )

#define IOCTL_VIDEO_S3_MBL_ENABLE_EVENTSIGNAL                           \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x1D )

#define IOCTL_VIDEO_S3_MBL_DISABLE_EVENTSIGNAL                          \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x1E )

#define IOCTL_VIDEO_S3_MBL_DISPLAY_SWITCH                               \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x1F )

#define IOCTL_VIDEO_S3_MBL_DISPLAY_STATUS                               \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x20 )

#define IOCTL_VIDEO_S3_MBL_GET_TV_STANDARD                              \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x21 )

#define IOCTL_VIDEO_S3_MBL_SET_TV_STANDARD                              \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x22 )

#define IOCTL_VIDEO_S3_MBL_CTR_TV_VIEWPORT_NOW                          \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x23 )

#define IOCTL_VIDEO_S3_MBL_GET_BIOS_VERSION                             \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x24 )

#define IOCTL_VIDEO_S3_MBL_GET_VIDEO_OUT_IGA                            \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x25 )

#define IOCTL_VIDEO_S3_MBL_SET_VIDEO_OUT_IGA                            \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x26 )

#define IOCTL_VIDEO_S3_MBL_SUSPEND                                      \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x27 )

#define IOCTL_VIDEO_S3_MBL_RESUME                                       \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x28 )

#define IOCTL_VIDEO_S3_MBL_EXPANSION_CHANGE                             \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x29 )

#define IOCTL_VIDEO_S3_MBL_GET_APP_HWND                                 \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x2A )

#define IOCTL_VIDEO_S3_MBL_SET_APP_HWND                                 \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x2B )

#define IOCTL_VIDEO_S3_MBL_QUERY_VIDEO_PAN_INFO                         \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x2C )

#define IOCTL_VIDEO_S3_MBL_QUERY_MOBILE_SUPPORT                        \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x2D )

#define IOCTL_VIDEO_S3_MBL_QUERY_DUOVIEW_POSSIBLE                      \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x2E )

#define IOCTL_VIDEO_S3_MBL_QUERY_HWOVERLAY_POSSIBLE                    \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x2F )

#define IOCTL_VIDEO_S3_MBL_QUERY_LGDESKTOP_POSSIBLE                    \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x30 )

#define IOCTL_VIDEO_S3_MBL_GET_CRT_RES_TABLE                           \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x31 )

#define IOCTL_VIDEO_S3_MBL_GET_CRT_REFRESH_RATES                       \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x32 )

#define IOCTL_VIDEO_S3_MBL_SET_CRT_REFRESH_RATE                        \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x33 )

#define IOCTL_VIDEO_S3_MBL_GET_MOBILE_INTERFACE                        \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x34 )

#define IOCTL_VIDEO_S3_MBL_STOP_FORCE_SW_POINTER                       \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x35 )

#define IOCTL_VIDEO_S3_MBL_GET_FORCE_SW_POINTER                        \
    S3_IOCTL( MOBILE_IOCTL_BASE + 0x36 )

//
// Please KEEP IOCtls SORTED BY NUMBER!!!!!!
//

