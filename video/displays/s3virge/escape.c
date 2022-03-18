//******************************Module*Header********************************
//*  Module Name: escape.c
//*
//*  Handles the display driver's private escape functions.  The escape
//*  functions provide an interface to applications to access hardware
//*  capabilities.  They enable the application to query information
//*  unavailable in a device-independent DDI.
//*
//*  Copyright (c) Microsoft 1998, All Rights Reserved
//*
//***************************************************************************

//
//  Note that the PRECOMP.H header file includes a number of other needed
//  include files, including WINGDI.H, from the Visual C or Win32 SDK
//  directory, which defines QUERYESCSUPPORT.
//
#include "precomp.h"

#include <ntddvdeo.h>

#include "hw3d.h"

#if SUPPORT_MCD

extern BOOL MCDrvGetEntryPoints(MCDSURFACE *pMCDSurface, MCDDRIVER *pMCDDriver);

extern VOID CheckVirgeCap(IN PDEV* ppdev);

#endif

//
// Local function prototypes
//

ULONG APIENTRY ProcessMobileEscapes( PDEV *,
                                     SURFOBJ *,
                                     ULONG,
                                     PVOID,
                                     ULONG,
                                     PVOID );


//---------------------------------------------------------------------------
//  PROCEDURE:   DrvEscape
//
//  DESCRIPTION: Driver ESCAPE function entry point
//
//  ASSUMPTIONS: Calling application is expected to first make the standard
//               NT pre-defined ESC_QUERYESCSUPPORT query, passing a parameter
//               specifying the driver-specific query it wishes to make.  If
//               that succeeds (the driver supports the escape fn), then
//               the application can call DrvEscape again, using the driver-
//               specific Escape function.
//
//  CALLS:
//    DeviceIoControl  (Device IOCTL), as needed when obtaining hardware
//        information from miniport.
//
//  PARAMETERS:
//    IN SURFOBJ *pso
//        Pointer to SURFOBJ describing surface to which call is directed
//    IN ULONG iEsc
//        Specifies the query.  Dictates the meaning of the other paramters.
//        ESC_QUERYESCSUPPORT is the only pre-defined value; it queries whether
//        the driver supports a particular escape function.  In this case, pvIn
//        points to an escape function number; cjOut and pvOut are ignored, and
//        the function returns only TRUE or FALSE.
//    IN ULONG cjIn
//        Specifies the size, in bytes, of the buffer pointed to by pvIn.
//    IN PVOID *pvIn
//        Points to the input data for the call.  The format of the input data
//        depends on the query specified by the iEsc parameter. For example,
//        If iEsc is S3ESC_MBL_FUNCTION, then the first DWORD of pvIn will
//        be the private Escape value of the Mobile subfunction.
//    IN ULONG *cjOut
//        Specifies the size, in bytes, of the buffer pointed to by pvOut.
//    OUT PVOID *pvOut
//        Points to the output buffer.  The format of the output data depends
//        on the query specified by the iEsc parameter.
//
//  RETURNS:
//    If iEsc was QUERYESCSUPPORT, then function returns:
//        TRUE for any supported escape functions
//        FALSE for any unsupported escape functions
//    Otherwise, the return value (a ULONG) is dependent on the
//    the query specified by the iEsc function.
//
//  NOTES:  Escape function classes supported include
//              S3ESC_MBL_FUNCTION
//              S3COLOR_ADJUST_FUNCTION
//              S3ESC_GET_EDID
//
//---------------------------------------------------------------------------

//*****************************Public*Routine********************************\
// ULONG DrvEscape(SURFOBJ *, ULONG, ULONG, VOID *, ULONG cjOut, VOID *pvOut)
//
// Win32 API ExtEscape() results in this driver call
// Driver escape entry point.  This function should return TRUE for any
// supported escapes in response to QUERYESCSUPPORT, and FALSE for any
// others.  All supported escapes are called from this routine.
//***************************************************************************/

extern BOOL MCDrvGetEntryPoints(MCDSURFACE *pMCDSurface, MCDDRIVER *pMCDDriver);


#define ESC_IS_SUPPORTED            1
#define ESC_NOT_SUPPORTED           0xffffffff
#define NULL_INPUT                  0xfffffffe

ULONG APIENTRY DrvEscape(
SURFOBJ *pso,
ULONG    iEsc,
ULONG    cjIn,
PVOID    pvIn,
ULONG    cjOut,
PVOID    pvOut)
{
    DWORD   ReturnedDataLength;
    PDEV    *ppdev;
    ULONG   ulInputData;
    ULONG   ulOutputData        = 0;


    int     iQuery;
    ULONG   ulRet = DDI_ERROR;


    ppdev = (PDEV*) pso->dhpdev;

    switch( iEsc )
    {

    case QUERYESCSUPPORT:
        iQuery = *(int *)pvIn;

        switch(iQuery)
        {
        case S3ESC_MBL_FUNCTION:
        case S3COLOR_ADJUST_FUNCTION:
        case S3ESC_GET_EDID:
#if SUPPORT_MCD
        case MCDFUNCS:
#endif

            return TRUE;
        default:
            return FALSE;
        }
        break;

#if SUPPORT_MCD
    case MCDFUNCS:

        if (!ppdev->hMCD)
        {
            WCHAR uDllName[50];
            UCHAR dllName[50];
            ULONG nameSize;

            CheckVirgeCap(ppdev);   // check for Virge DX/GX cap, msu.

            EngMultiByteToUnicodeN(uDllName, sizeof(uDllName), &nameSize,
                MCDENGDLLNAME, sizeof(MCDENGDLLNAME));

            if (ppdev->hMCD = EngLoadImage(uDllName))
            {
                MCDENGINITFUNC pMCDEngInit;

                pMCDEngInit = EngFindImageProcAddress(
                    ppdev->hMCD,
                    (LPSTR) MCDENGINITFUNCNAME );

                if (pMCDEngInit)
                {
                    (*pMCDEngInit)(pso, MCDrvGetEntryPoints);
                    ppdev->pMCDFilterFunc = EngFindImageProcAddress(
                        ppdev->hMCD,
                        (LPSTR) MCDENGESCFILTERNAME );
                }
            }
        }

        if (ppdev->pMCDFilterFunc)
        {
            if( (*ppdev->pMCDFilterFunc)(pso,
                                         iEsc,
                                         cjIn,
                                         pvIn,
                                         cjOut,
                                         pvOut,
                                         &ulRet))
            {
                return ulRet;
            }
            else
            {
                return FALSE;
            }
        } else {
            return FALSE;
        }
        break;
#endif

    case S3COLOR_ADJUST_FUNCTION:
        iQuery = *(int *)pvIn;

        switch (iQuery)
        {
        case S3COLOR_ADJUST_STATUS:
            if ( !cjOut || (pvOut == NULL) ) return FALSE;

            if ( ppdev->ulCaps2 & CAPS2_COLOR_ADJUST_ALLOWED )
            {
                *(ULONG *)pvOut = (ppdev->ulCanAdjustColor & ~COLOR_ADJUSTED) |
                                  COLOR_ADJUST_ALLOWED;
            }
            return TRUE;

            break;

        case S3COLOR_ADJUST_READCOLORADJUSTREGISTER:
            if ( !cjOut || (pvOut == NULL) ) return FALSE;

            *(ULONG *) pvOut = READ_REGISTER_ULONG( (BYTE *)(ppdev->pjMmBase +
                                                             COLOR_ADJUST_REG));
            return TRUE;

        case S3COLOR_ADJUST_WRITECOLORADJUSTREGISTER:

            if ( !cjIn  || (pvIn == NULL) )
                return FALSE;

            ulInputData = *( (ULONG *)pvIn + 1);

            WRITE_STREAM_D( ppdev->pjMmBase,
                            COLOR_ADJUST_REG,
                            ulInputData );

            ppdev->ulCanAdjustColor |= COLOR_ADJUSTED;

            return TRUE;

        default:
            break;


        }
        return FALSE;

    case S3ESC_MBL_FUNCTION:

        return( ProcessMobileEscapes( ppdev,
                                      pso,
                                      cjIn,
                                      pvIn,
                                      cjOut,
                                      pvOut ));
    //
    //  Added to support Compaq's IM (Intelligent Manageability) requirements
    //  for returning the Monitor EDID information
    //

    case S3ESC_GET_EDID:
        //
        //  first check to see if a Monitor number was passed in
        //

        ulInputData = 0;            // assume Monitor 0
        if (pvIn != NULL)
        {
            //
            //  the size of the input value must be a ULONG
            //

            if (cjIn == sizeof(ULONG))
            {
                ulInputData = *(PULONG)pvIn;
            }
        }

        if (EngDeviceIoControl(
                ppdev->hDriver,             // hDevice
                IOCTL_VIDEO_S3_GET_EDID,
                (LPVOID)&ulInputData,       // lpInBuffer
                sizeof(ULONG),              // DWORD
                (LPVOID)pvOut,               // lpOutBuffer
                cjOut,                       // OutBufferSize
                &ReturnedDataLength))        // LPDWORD lpBytesReturned
        {
            return 0L;
        }
        else
        {
            return (ReturnedDataLength);
        }

    break;

    case SET_POWER_STATE:

        if (cjIn == sizeof (VIDEO_POWER_MANAGEMENT))
        {
            if(!(EngDeviceIoControl(
                 ppdev->hDriver,                   // hDevice
                 IOCTL_VIDEO_SET_POWER_MANAGEMENT, // dwIoControlCode
                 pvIn,                             // lpInBuffer
                 cjIn,                             // DWORD InBufferSize
                 NULL,                             // lpOutBuffer
                 0,                                // DWORD OutBufferSize
                 &ReturnedDataLength)))            // LPDWORD lpBytesReturned
            {
                 return TRUE;
            }
        }
        return FALSE;

    break;

    case GET_POWER_STATE:

        if ( (cjOut==sizeof (VIDEO_POWER_MANAGEMENT)) && (pvOut != NULL) )
        {
           if(!(EngDeviceIoControl(
                ppdev->hDriver,                   // hDevice
                IOCTL_VIDEO_GET_POWER_MANAGEMENT, // dwIoControlCode
                NULL,                             // lpInBuffer
                0,                                // DWORD InBufferSize
                pvOut,                                                  // lpOutBuffer
                cjOut,                            // DWORD OutBufferSize
                &ReturnedDataLength)))            // LPDWORD lpBytesReturned
           {
                return TRUE;
           }
        }
        return FALSE;   // Not enough input parameters

    break;


    default:
        return( 0xFFFFFFFF );  // FAILURE/UNDEFINED
        break;

    } // end of switch statement

} // end of DrvEscape()

//
//  Mobile Escapes (S3ESC_MBL_FUNCTION)
//
ULONG APIENTRY ProcessMobileEscapes( PDEV    *ppdev,
                                     SURFOBJ *pso,
                                     ULONG    cjIn,
                                     PVOID    pvIn,
                                     ULONG    cjOut,
                                     PVOID    pvOut )
{
    DISPLAY_SUPPORT              DisplaySupport;
    DWORD                        ReturnedDataLength;
    MBL_BIOS_VERSION_INFO        BiosVersionInfo;
    MBL_OUTP_GET_DISPLAY_CONTROL DisplayControl;
    MBL_OUTP_GET_PANEL_INFO      PanelInformation;
    MBL_OUTP_GET_TV_FILTER_STATE TVFFControl;
    TV_FLICKER_FILTER_CONTROL    TVFFControlReq;
    ULONG                        EventReg;
    ULONG                        ulDisplayStatusReg;
    ULONG                        ulExpandStatus;
    ULONG                        ulInputData;
    ULONG                        ulOutputData        = 0;
    MOBILE_INTERFACE             MobileInterface    ={GENERIC, SUBTYPE_INVALID};
    VIDEO_DISPLAY_CONTROL        DisplayControlReq;

    //
    //  For these Mobile functions, the first ULONG of the input
    //  buffer is always the subfunction, so cjIn must be at least 4.
    //

    switch ( *(PULONG)pvIn )
    {

    case S3ESC_MBL_SUSPEND:

       if (EngDeviceIoControl(
               ppdev->hDriver,              // hDevice
               IOCTL_VIDEO_S3_MBL_SUSPEND,
               NULL,                        // lpInBuffer
               0,                           // DWORD nInBufferSize
               NULL,                        // lpOutBuffer
               0,                           // DWORD nOutBufferSize
               &ReturnedDataLength))        // LPDWORD lpBytesReturned
       {
           return S3_MOBILE_FAILURE;
       }
       return S3_MOBILE_SUCCESS;

    case S3ESC_MBL_RESUME:

       if (EngDeviceIoControl(
               ppdev->hDriver,              // hDevice
               IOCTL_VIDEO_S3_MBL_RESUME,
               NULL,                        // lpInBuffer
               0,                           // DWORD nInBufferSize
               NULL,                        // lpOutBuffer
               0,                           // DWORD nOutBufferSize
               &ReturnedDataLength))        // LPDWORD lpBytesReturned
       {
           return S3_MOBILE_FAILURE;
       }

       return S3_MOBILE_SUCCESS;

    case S3ESC_MBL_M5_EVENT_STATUS:

        if ( (cjOut != sizeof(ULONG)) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        if ( (MobileInterface.ulMfg != M5_COMPAQ) ||
             (ppdev->bCompaqDispSwitchPending == FALSE) )
        {
            //
            //  Read the signalling protocol register.
            //
            ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

            VGAOUTP( ppdev, CRTC_INDEX, M5_EVENT_REG);
            EventReg = VGAINP( ppdev, CRTC_DATA );

            RELEASE_CRTC_CRITICAL_SECTION(ppdev);
        }
        else
        {
            //
            // Fake NO EVENT if there's an display switch already pending.
            //
            EventReg = 0;
        }

        *(PULONG)pvOut = 0L;

        //
        //  Test for event notification bits we care about
        //
        if (EventReg & M5_EVREG_EVENT_BITS)
        {

            //
            //  IF the expansion state change bit is set
            //

            if (EventReg & M5_EVREG_EXPAND)
            {
                //
                //  EngDeviceIoControl returns 0 if no errors
                //

                if ((EngDeviceIoControl(
                        ppdev->hDriver,              // hDevice
                        IOCTL_VIDEO_S3_MBL_EXPANSION_CHANGE,
                        NULL,                        // lpInBuffer
                        0,                           // DWORD nInBufferSize
                        &ulExpandStatus,             // lpOutBuffer
                        sizeof(ULONG),               // DWORD nOutBufferSize
                        &ReturnedDataLength) == 0)   // LPDWORD lpBytesReturned
                    && ReturnedDataLength == sizeof(ULONG))
                {
                    switch ( ulExpandStatus )
                    {
                    case MBL_CTREXP_EXPAND:
                        ppdev->Band.DuoViewFlags |= DVW_Expand_ON;
                        break;

                    case MBL_CTREXP_CENTER:
                        ppdev->Band.DuoViewFlags |= DVW_Center_ON;
                        break;

                    case MBL_CTREXP_NONE:
                    default:
                        ppdev->Band.DuoViewFlags &=
                            ~(DVW_Center_ON | DVW_Expand_ON);
                        break;
                    }

                    if ( ppdev->flStatus & STAT_STREAMS_ENABLED )
                    {
                        //
                        //  get flags/state for Dual IGA controllers
                        //

                        if ( !EngDeviceIoControl( ppdev->hDriver,
                                IOCTL_VIDEO_S3_QUERY_STREAMS_STATE,
                                &ppdev->Band,
                                sizeof( BAND ),
                                &ppdev->Band,
                                sizeof( BAND ),
                                &ReturnedDataLength ) )
                        {
                            if (ppdev->flStatus & STAT_STREAMS_ENABLED )
                            {
                                ddUpdateSecondaryStreams( ppdev,
                                    SS_UPDATE_CENTER_EXPAND );
                            }
                        }
                    }

                    //
                    //  Don't update app for DirectX modes.
                    //
                    if ((ppdev->cxScreen >= 640) && (ppdev->cyScreen >= 480))
                    {
                        *(PULONG)pvOut |= S3_MOBILE_NOTIFYAPP_UPDATE;
                    }

                    return S3_MOBILE_SUCCESS;

                }
            }

            //
            //  IF the display switch bit is set
            //

            if (EventReg & M5_EVREG_DISPSW)
            {
                //
                //  EngDeviceIoControl returns 0 if no errors
                //

//
//  The Compaq event signalling protocol requires the assistance of the driver
//  to complete the display switch, so the switch event signal cannot be
//  ignored even when the mode is a VGA mode.
                if(ppdev->ulCaps2 & CAPS2_LCD_SUPPORT)
                {
                    //
                    //  call the miniport to get the information
                    //

                    if (EngDeviceIoControl( ppdev->hDriver, // hDevice
                            IOCTL_VIDEO_S3_MBL_GET_MOBILE_INTERFACE,
                            NULL,                           // lpInBuffer
                            0,                              // DWORD nInBufferSize
                            (LPVOID)&MobileInterface,       // lpOutBuffer
                            sizeof(MOBILE_INTERFACE),                  // DWORD nOutBufferSize
                            &ReturnedDataLength) )          // LPDWORD lpBytesReturned
                    {
                        return S3_MOBILE_FAILURE;
                    }
                }

                if (  ((MobileInterface.ulMfg != M5_COMPAQ) && (ppdev->bEnabled)) ||
                       (MobileInterface.ulMfg == M5_COMPAQ) )
                {
                
                    //
                    // For Compaq we clear the signalling bit during this call.
                    // We need to set this flag to prevent another event from
                    // being processed before this one is complete.
                    // 
                if (MobileInterface.ulMfg == M5_COMPAQ)
                    {
                       ppdev->bCompaqDispSwitchPending = TRUE;
                    }
                    
                    if (EngDeviceIoControl(
                        ppdev->hDriver,             // hDevice
                        IOCTL_VIDEO_S3_MBL_DISPLAY_SWITCH,
                        NULL,                       // lpInBuffer
                        0,                          // DWORD nInBufferSize
                        (LPVOID)&ulOutputData,      // lpOutBuffer
                        sizeof(ULONG),              // DWORD nOutBufferSize
                        &ReturnedDataLength) == 0)  // LPDWORD lpBytesReturned
                    {
//
//  Leave enabled so that if we need to repaint screen due to 
//  a display switch at some time when a system-wide modeset is blocked
//  (e.g. before NT login completed, or at Ctrl-Alt-Delete Windows NT Security
//  screen), we can handle the switch.
//
                        if (cjOut == ReturnedDataLength)
                        {
                            if (EngDeviceIoControl(ppdev->hDriver,
                                IOCTL_VIDEO_SET_CURRENT_MODE,
                                &ppdev->ulMode,  // input buffer
                                sizeof(DWORD),
                                (LPVOID)&ulOutputData,
                                sizeof(ULONG),
                                &ReturnedDataLength))
                            {
                                return S3_MOBILE_FAILURE;
                            }

                            if ( !EngDeviceIoControl( ppdev->hDriver,
                                IOCTL_VIDEO_S3_QUERY_STREAMS_STATE,
                                &ppdev->Band,
                                sizeof( BAND ),
                                &ppdev->Band,
                                sizeof( BAND ),
                                &ReturnedDataLength ) )
                            {
                                //
                                //  Signal that we will want a modeset.
                                //
                                *(PULONG)pvOut = S3_MOBILE_NOTIFYAPP_MODESET;
                            }
                        }
                    }

                    //
                    //  Update the configuration utility with the current
                    //  state
                    //

                    //
                    //  Don't update app for DirectX modes.
                    //
                    if ((ppdev->cxScreen >= 640) && (ppdev->cyScreen >= 480))
                    {
                        *(PULONG)pvOut |= S3_MOBILE_NOTIFYAPP_UPDATE;
                    }

                    *(PULONG)pvOut |= S3_MOBILE_NOTIFYAPP_MODESET;

                }
                else if (MobileInterface.ulMfg != M5_COMPAQ)  // not an enhanced mode
                {
                  //
                  //  Clear the display notification bits.
                  //  Pass the display switch through without driver
                  //  action or app notification.
                  //
                  ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

                  VGAOUTP( ppdev, CRTC_INDEX, EXT_BIOS_FLAG3);
                  VGAOUTP( ppdev, CRTC_DATA,
                      VGAINP( ppdev, CRTC_DATA ) & ~M5_ALT_DISPSW );

                  VGAOUTP( ppdev, CRTC_INDEX, M5_EVENT_REG);
                  VGAOUTP( ppdev, CRTC_DATA,
                      VGAINP( ppdev, CRTC_DATA ) & ~M5_EVREG_DISPSW );

                  RELEASE_CRTC_CRITICAL_SECTION(ppdev);

                }
                else
                {
                    //
                    // For Compaq, clear this flag to allow the next display
                    // switch to be processed.
                    // 
                    ppdev->bCompaqDispSwitchPending = FALSE;
                }
            }
            //
            //  Temporarily ignore the resume bit, until there's
            //  something we know we want to do in response to it.
            //  Also, the code to handle this bit must be customer
            //  specific, as not all customer BIOSes implement
            //  signalling on this bit.  The bit is a non-blocking
            //  notification bit, so it is safe to ignore it.
            //
            //  We are now handling this bit, because Compaq reported
            //  bug CR23195, when system resume from Hibernation, 
            //  in the mode LCD(16x12) and CRT(6x4 or 8x6) DuoView,
            //  CRT is blank. we solve the problem by issue a modeset
            //  which will restore CRT setting. Patrick
            // 
            
            if(EventReg & M5_EVREG_RESUME)
            {
                DISPDBG((5, "Resume catched!"));
                *(PULONG)pvOut |= S3_MOBILE_NOTIFYAPP_MODESET;
 
                //
                //  Clear the resume notification bits.
                //

                ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

                VGAOUTP( ppdev, CRTC_INDEX, M5_EVENT_REG);
                VGAOUTP( ppdev, CRTC_DATA,
                    VGAINP( ppdev, CRTC_DATA ) & ~M5_EVREG_RESUME );

                RELEASE_CRTC_CRITICAL_SECTION(ppdev);
            }
        }

        return S3_MOBILE_SUCCESS;
        break;

    case S3ESC_MBL_GET_PANEL_INFO:
        if ( (cjOut < (2 * sizeof(USHORT))) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_PANEL_INFO,// dwIoControlCode
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&PanelInformation,   // lpOutBuffer
                sizeof(MBL_OUTP_GET_PANEL_INFO),
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (ReturnedDataLength == sizeof(MBL_OUTP_GET_PANEL_INFO))
            {
                //
                //  return info
                //

                ((MBL_OUTP_GET_PANEL_INFO*)pvOut)->MblOutXres =
                                    PanelInformation.MblOutXres;
                ((MBL_OUTP_GET_PANEL_INFO*)pvOut)->MblOutYres =
                                    PanelInformation.MblOutYres;
                if (cjOut >= (3 * sizeof(USHORT)))
                {
                    ((MBL_OUTP_GET_PANEL_INFO*)pvOut)->MblOutPanelType =
                                    PanelInformation.MblOutPanelType;
                }
                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return (S3_MOBILE_FAILURE);
            }
        }
        break;


    case S3ESC_MBL_GET_DISPLAY_CONTROL:
        if ( (cjOut < 4) || (pvOut == NULL) ) return FALSE;

        if (EngDeviceIoControl(
                ppdev->hDriver,
                IOCTL_VIDEO_S3_MBL_GET_DISPLAY_CONTROL,
                pvIn,
                cjIn,
                (LPVOID)&DisplayControl,
                cjOut,
                &ReturnedDataLength) )
            return S3_MOBILE_FAILURE;
        else
        {
            if ((cjOut >= 4) && (ReturnedDataLength >= 4))
            {
                ((MBL_OUTP_GET_DISPLAY_CONTROL*)
                  pvOut)->MblOutDisplayType =
                  DisplayControl.MblOutDisplayType;

                if ((cjOut >= 8) && (ReturnedDataLength >= 8))
                {
                    ((MBL_OUTP_GET_DISPLAY_CONTROL*)
                      pvOut)->MblOutTvStandard =
                      DisplayControl.MblOutTvStandard;
                }
                return S3_MOBILE_SUCCESS;
            }
            else
                return S3_MOBILE_FAILURE;
        }
        break;

    case S3ESC_MBL_SET_DISPLAY_CONTROL:

        //
        // There must be a minimum of one ULONG as a parameter.
        //
        if ( (cjIn < 8) || (pvIn == NULL) )
            return S3_MOBILE_FAILURE;

        DisplayControlReq.ActiveDisplayReq =
            ((MBL_INP_SET_DISPLAY_CONTROL *)
              pvIn)->MblSetDisplayType;

        if (cjIn >= 12)
        {
            DisplayControlReq.TvTechnologyReq =
                ((MBL_INP_SET_DISPLAY_CONTROL *)
                pvIn)->MblSetTvStandard;
        }
        else
        {
            DisplayControlReq.TvTechnologyReq = 0;
        }

        if (EngDeviceIoControl(
                ppdev->hDriver,
                IOCTL_VIDEO_S3_MBL_SET_DISPLAY_CONTROL,
                &DisplayControlReq,
                (cjIn - 4),
                (LPVOID)&ulOutputData,
                sizeof(ULONG),
                &ReturnedDataLength) )
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if ( (pvOut != NULL) && (cjOut >= 4)
                    && (ReturnedDataLength >= 4) )
            {
               ((MBL_OUTP_SET_DISPLAY_CONTROL*)
                 pvOut)->MblOutDisplaySet = ulOutputData;
            }

            //
            //  get flags/state for Dual IGA controllers
            //

            if ( !EngDeviceIoControl( ppdev->hDriver,
                     IOCTL_VIDEO_S3_QUERY_STREAMS_STATE,
                     &ppdev->Band,
                     sizeof( BAND ),
                     &ppdev->Band,
                     sizeof( BAND ),
                     &ReturnedDataLength ) )
            {
                if (ppdev->flStatus & STAT_STREAMS_ENABLED )
                {
                    ddUpdateSecondaryStreams (ppdev,
                                            SS_UPDATE_DISPLAY_CONTROL);
                }
            }
            return S3_MOBILE_SUCCESS;
        }

        break;

    case S3ESC_MBL_GET_HORZ_STATE:
    case S3ESC_MBL_GET_VERT_STATE:

    //
    //  get the current centering/expansion mode
    //

        if ( (cjOut == 0) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_CENTEREXPAND_MODE,
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the current mode
                //

                *(PULONG)pvOut = ulOutputData;

                //
                //  If the panel (or TV-IGA1) physical size > physical res
                //  then the Center or Expand preference is currently
                //  active, and we need to update the DuoView flags.
                //

                if ((ppdev->Band.GSizeX2 > ppdev->Band.VisibleWidthInPixels)
                    ||
                    (ppdev->Band.GSizeY2 > ppdev->Band.VisibleHeightInPixels))
                {
                    switch ( ulOutputData )
                    {
                    case MBL_CTREXP_EXPAND:
                        ppdev->Band.DuoViewFlags |= DVW_Expand_ON;
                        break;

                    case MBL_CTREXP_CENTER:
                        ppdev->Band.DuoViewFlags |= DVW_Center_ON;
                        break;

                    case MBL_CTREXP_NONE:
                    default:
                        ppdev->Band.DuoViewFlags &=
                            ~(DVW_Center_ON | DVW_Expand_ON);
                    break;
                    }

                    if ( ppdev->flStatus & STAT_STREAMS_ENABLED )
                    {
                        ddUpdateSecondaryStreams (
                            ppdev,
                            SS_UPDATE_CENTER_EXPAND );
                    }
                }

                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return (S3_MOBILE_FAILURE);
            }
        }
        break;

    //
    //  set the current centering/expansion mode
    //

    case S3ESC_MBL_SET_HORZ_STATE:
    case S3ESC_MBL_SET_VERT_STATE:
        //
        //  make sure the input data buffer is large enough
        //  to hold the new display mode
        //

        if (cjIn < (2 * sizeof(ULONG)))
            return S3_MOBILE_FAILURE;

        //
        //  get the new mode
        //

        ulInputData = ((MBL_INP_SET_CENTEREXPAND_MODE*)
                        pvIn)->MblSetCenterExpandMode;

        //
        //  call the miniport to set the new mode
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_SET_CENTEREXPAND_MODE,
                &ulInputData,                // lpInBuffer
                sizeof(ULONG),               // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            //
            //  get flags/state for Dual IGA controllers
            //

            if ( !EngDeviceIoControl( ppdev->hDriver,
                     IOCTL_VIDEO_S3_QUERY_STREAMS_STATE,
                     &ppdev->Band,
                     sizeof( BAND ),
                     &ppdev->Band,
                     sizeof( BAND ),
                     &ReturnedDataLength ) )
            {
                if (ppdev->flStatus & STAT_STREAMS_ENABLED)
                {
                    ddUpdateSecondaryStreams (ppdev,
                                            SS_UPDATE_CENTER_EXPAND);
                }
            }
            return S3_MOBILE_SUCCESS;
        }
        break;

    case S3ESC_MBL_GET_CONNECT_STATUS:
        if ( !cjOut || (pvOut == NULL) ) return FALSE;

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_CONNECT_STATUS,// dwIoControlCode
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
            return S3_MOBILE_FAILURE;
        else
        {
            if ((cjOut >= 4) && (ReturnedDataLength >= 4))
            {
                ((MBL_OUTP_GET_CONNECT_STATUS*)pvOut)->MblConnectionStatus =
                    ulOutputData;

                return S3_MOBILE_SUCCESS;
            }
            else
                return S3_MOBILE_FAILURE;
        }
        break;

    case S3ESC_MBL_GET_CRT_PAN_RES:
        if ( (cjOut == 0) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_CRT_PAN_RES,
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the current value
                //

                *(PULONG)pvOut = ulOutputData;
                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }
        break;

    case S3ESC_MBL_SET_CRT_PAN_RES:
        //
        //  make sure the input data buffer is large enough
        //  to hold the new resolution index
        //

        if (cjIn < (2 * sizeof(ULONG)))
            return S3_MOBILE_FAILURE;

        //
        //  get the new index
        //

        ulInputData = ((PULONG) pvIn) [1];

        //
        //  call the miniport to set the new mode
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_SET_CRT_PAN_RES,
                &ulInputData,                // lpInBuffer
                sizeof(ULONG),               // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            return S3_MOBILE_SUCCESS;
        }
        break;

    case S3ESC_MBL_GET_TIMING_STATE:
        if ( (cjOut == 0) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_TIMING_STATE,
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the current value
                //

                *(PULONG)pvOut = ulOutputData;
                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }
        break;

    case S3ESC_MBL_GET_IMAGE_STATE:
        if ( (cjOut == 0) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_IMAGE_STATE,
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the current value
                //

                *(PULONG)pvOut = ulOutputData;
                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }
        break;

    case S3ESC_MBL_GET_PRIMARY_DEVICE:
        if ( (cjOut == 0) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_PRIMARY_DEVICE,
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the current value
                //

                *(PULONG)pvOut = ulOutputData;
                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }
        break;

    case S3ESC_MBL_SET_PRIMARY_DEVICE:
        //
        //  make sure the input data buffer is large enough
        //  to hold the new primary device
        //

        if (cjIn < (2 * sizeof(ULONG)))
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  get the new primary device
        //

        ulInputData = ((MBL_INP_SET_PRIMARY_DEVICE*)
                                            pvIn)->MblPrimaryDevice;

        //
        //  call the miniport to set the new mode
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_SET_PRIMARY_DEVICE,
                &ulInputData,                // lpInBuffer
                sizeof(ULONG),               // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            return S3_MOBILE_SUCCESS;
        }
        break;

    case S3ESC_MBL_GET_TV_FILTER_STATE:
        if ( (cjOut < 4) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,
                IOCTL_VIDEO_S3_MBL_GET_TV_FFILTER_STATUS,
                pvIn,
                cjIn,
                (LPVOID)&TVFFControl,
                sizeof(MBL_OUTP_GET_TV_FILTER_STATE),
                &ReturnedDataLength) )
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if ((cjOut >= 4) && (ReturnedDataLength >= 4))
            {
                ((MBL_OUTP_GET_TV_FILTER_STATE *)
                  pvOut)->MblTvFFilterState =
                  TVFFControl.MblTvFFilterState;

                if ((cjOut >= 8) && (ReturnedDataLength >= 8))
                {
                    ((MBL_OUTP_GET_TV_FILTER_STATE *)
                      pvOut)->MblTvFFilterFraction =
                      TVFFControl.MblTvFFilterFraction;
                }
                return S3_MOBILE_SUCCESS;
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }

        break;

    case S3ESC_MBL_SET_TV_FILTER_STATE:
        if (cjIn < (2 * sizeof(ULONG)))
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  collect the requested new flicker filter state.
        //

        ulInputData = ((MBL_INP_SET_TV_FILTER_STATE*)
                        pvIn)->MblSetTvFFilterState;

        TVFFControlReq.TvFFilterOnOffState =
            ((MBL_INP_SET_TV_FILTER_STATE *)
              pvIn)->MblSetTvFFilterState;

        TVFFControlReq.TvFFilterSpecificSetting =
            ((MBL_INP_SET_TV_FILTER_STATE *)
              pvIn)->MblSetTvFFilterFraction;

        //
        //  call the miniport to set the new mode
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_SET_TV_FFILTER_STATUS,
                &TVFFControlReq,             // lpInBuffer
                sizeof(TV_FLICKER_FILTER_CONTROL), // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            return S3_MOBILE_SUCCESS;
        }

        //
        // There's no return data for this Escape call.
        //

        break;


    case S3ESC_MBL_GET_TV_UNDERSCAN_STATE:
        if ( (cjOut < 4) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_TV_UNDERSCAN_STATUS,
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the current value
                //

                if (ulOutputData == TV_VIEWMETHOD_UNDERSCAN)
                {
                    ppdev->flStatus |= STAT_TVUNDERSCAN_ENABLED;
                }
                else
                {
                    ppdev->flStatus &= ~STAT_TVUNDERSCAN_ENABLED;
                }

                *(PULONG)pvOut = ulOutputData;
                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }
        break;


    case S3ESC_MBL_SET_TV_UNDERSCAN_STATE:
        if (cjIn < (2 * sizeof(ULONG)))
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  collect the requested new tv viewing method state.
        //

        ulInputData = ((MBL_INP_SET_TV_UNDERSCAN_STATE*)
                        pvIn)->MblSetTvUnderscanState;

        //
        //  validate the request as one that is supported
        //

        if (ulInputData < TV_VIEWMETHOD_VALID)
        {

            //
            //  call the miniport to set the new mode
            //

            if (EngDeviceIoControl(
                    ppdev->hDriver,              // hDevice
                    IOCTL_VIDEO_S3_MBL_SET_TV_UNDERSCAN_STATUS,
                    &ulInputData,                // lpInBuffer
                    sizeof(ULONG),               // DWORD nInBufferSize
                    (LPVOID)&ulOutputData,       // lpOutBuffer
                    sizeof(ULONG),               // DWORD nOutBufferSize
                    &ReturnedDataLength) )       // LPDWORD lpBytesReturned
            {
                return S3_MOBILE_FAILURE;
            }
            else
            {
                if (ulInputData == TV_VIEWMETHOD_UNDERSCAN)
                {
                    ppdev->flStatus |= STAT_TVUNDERSCAN_ENABLED;
                }
                else
                {
                    ppdev->flStatus &= ~STAT_TVUNDERSCAN_ENABLED;
                }
                return S3_MOBILE_SUCCESS;
            }

            //
            // There's no return data for this Escape call.
            //
        }
        else
        {
            return S3_MOBILE_FAILURE;
        }

        break;


    case S3ESC_MBL_GET_TV_OUTPUT_TYPE:
        if ( (cjOut < 4) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }
        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_TV_OUTPUT_TYPE,
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the current value
                //

                *(PULONG)pvOut = ulOutputData;

                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }
        break;


    case S3ESC_MBL_SET_TV_OUTPUT_TYPE:
        if (cjIn < (2 * sizeof(ULONG)))
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  collect the requested new TV standard.
        //

        ulInputData = *( (ULONG *)pvIn + 1);

        //
        //  call the miniport to set the new mode
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_SET_TV_OUTPUT_TYPE,
                &ulInputData,                // lpInBuffer
                sizeof(ULONG),               // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            return S3_MOBILE_SUCCESS;
        }

        //
        // There's no return data for this Escape call.
        //

        break;


    case S3ESC_MBL_GET_TV_CENTERING_OPTION:

        if ( (cjOut < (sizeof(ULONG))) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_TV_CENTEROPTION,
                NULL,                        // lpInBuffer
                0,                           // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the current value
                //

                *(PULONG)pvOut = ulOutputData;
                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }
        break;


    case S3ESC_MBL_SET_TV_CENTERING_OPTION:
        if (cjIn < (2 * sizeof(ULONG)))
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  collect the requested centering option.
        //

        ulInputData = *( (ULONG *)pvIn + 1);

        //
        //  call the miniport to set the new mode
        //

        if (EngDeviceIoControl(
            ppdev->hDriver,              // hDevice
            IOCTL_VIDEO_S3_MBL_SET_TV_CENTEROPTION,
            &ulInputData,                // lpInBuffer
            sizeof(ULONG),               // DWORD nInBufferSize
            NULL,                        // lpOutBuffer
            0,                           // DWORD nOutBufferSize
            &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            return S3_MOBILE_SUCCESS;
        }

        //
        // There's no return data for this Escape call.
        //

        break;

    case S3ESC_MBL_GET_CRT_PAN_RES_TABLE:
        if ( (pvOut == NULL) ||
             (cjOut == 0)    ||
             (pvIn == NULL)  ||
             (cjIn < sizeof(MBL_INP_GET_CRT_PAN_RES_TABLE)) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_CRT_RES_TABLE,// dwIoControlCode
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                pvOut,                       // lpOutBuffer
                cjOut,
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            //
            //  return success for pvOut updated OK!
            //

            if (ReturnedDataLength <= cjOut)
            {
                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return (S3_MOBILE_FAILURE);
            }
        }
        break;

    case S3ESC_MBL_GET_CHIPID:
        if ( (cjOut < sizeof(ULONG)) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_CHIPID,
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the current value
                //

                *(PULONG)pvOut = ulOutputData;
                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }
        break;

    case S3ESC_MBL_GET_BIOS_VERSION:
        if ( (cjOut < (sizeof(MBL_BIOS_VERSION_INFO))) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_BIOS_VERSION,
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&BiosVersionInfo,       // lpOutBuffer
                sizeof(MBL_BIOS_VERSION_INFO),  // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                *(MBL_BIOS_VERSION_INFO *)pvOut = BiosVersionInfo;
                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }
        break;

    case S3ESC_MBL_GET_CRT_CAPABILITY:
        if ( (cjOut == 0) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_CRT_CAPABILITY,
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the current value
                //

                *(PULONG)pvOut = ulOutputData;
                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }
        break;

    case S3ESC_MBL_SET_CRT_CAPABILITY:
        //
        //  make sure the input data buffer is large enough
        //  to hold the new capability index
        //

        if (cjIn < (2 * sizeof(ULONG)))
            return S3_MOBILE_FAILURE;

        //
        //  get the new index
        //

        ulInputData = ((PULONG) pvIn) [1];

        //
        //  call the miniport to set the new value
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_SET_CRT_CAPABILITY,
                &ulInputData,                // lpInBuffer
                sizeof(ULONG),               // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            return S3_MOBILE_SUCCESS;
        }
        break;

    case S3ESC_MBL_GET_VIDEOOUT_DEVICES:
        if ( (cjOut == 0) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //
        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_VIDEO_OUT_IGA,
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the current value
                //

                if (ulOutputData == S3_HWOVERLAY_OFF)
                {
                    ppdev->bHwOverlayOff = TRUE;
                }
                else
                {
                    ppdev->bHwOverlayOff = FALSE;
                }

                *(PULONG)pvOut = ulOutputData;
                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }

        break;

    case S3ESC_MBL_SET_VIDEOOUT_DEVICES:
        //
        //  make sure the input data buffer is large enough
        //  to hold the new IGA value
        //

        if (cjIn < (2 * sizeof(ULONG)))
            return S3_MOBILE_FAILURE;

        //
        //  get the new IGA #
        //

        ulInputData = ((PULONG) pvIn) [1];

        //
        //  call the miniport to set the new value
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_SET_VIDEO_OUT_IGA,
                &ulInputData,                // lpInBuffer
                sizeof(ULONG),               // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            //
            //  IF the selected IGA has changed, we need to update
            //  our streams state
            //

            if (ulInputData == S3_HWOVERLAY_OFF)
            {
                ppdev->bHwOverlayOff = TRUE;
            }
            else
            {
                ppdev->bHwOverlayOff = FALSE;

                if ( ((((PULONG) pvIn)[1] == IGA1) &&
                      (!(ppdev->Band.DuoViewFlags & DVW_SP_IGA1)))
                                ||
                     ((((PULONG) pvIn)[1] == IGA2) &&
                      (ppdev->Band.DuoViewFlags & DVW_SP_IGA1)) )
                {
                    //
                    //  get flags/state for Dual IGA controllers
                    //

                    if ( !EngDeviceIoControl( ppdev->hDriver,
                             IOCTL_VIDEO_S3_QUERY_STREAMS_STATE,
                             &ppdev->Band,
                             sizeof( BAND ),
                             &ppdev->Band,
                             sizeof( BAND ),
                             &ReturnedDataLength ) )
                    {
                        if ( ppdev->flStatus & STAT_STREAMS_ENABLED )
                        {
                            ddUpdateSecondaryStreams (ppdev, SS_UPDATE_IGA);
                        }
                    }
                }
            }

            return S3_MOBILE_SUCCESS;
        }
        break;

    case S3ESC_MBL_GET_TV_STANDARD:
        if ( (cjOut < 4) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_GET_TV_STANDARD,
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the current value
                //

                *(PULONG)pvOut = ulOutputData;
                return (S3_MOBILE_SUCCESS);
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }
        break;


    case S3ESC_MBL_SET_TV_STANDARD:
        if (cjIn < (2 * sizeof(ULONG)))
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  collect the requested new TV standard.
        //

        ulInputData = ((MBL_INP_SET_TV_STANDARD *)
                        pvIn)->MblSetTvStandard;

        //
        //  call the miniport to set the new mode
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_SET_TV_STANDARD,
                &ulInputData,                // lpInBuffer
                sizeof(ULONG),               // DWORD nInBufferSize
                (LPVOID)&ulOutputData,       // lpOutBuffer
                sizeof(ULONG),               // DWORD nOutBufferSize
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            return S3_MOBILE_SUCCESS;
        }

        //
        // There's no return data for this Escape call.
        //

        break;


    case S3ESC_MBL_GET_VIDEO_MEMSIZE:
        if ( (cjOut < sizeof(ULONG)) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        cjOut = sizeof(ULONG);
        *(PULONG)pvOut = ppdev->ulFrameBufferLength;
        return (S3_MOBILE_SUCCESS);
        break;


    case S3ESC_MBL_EVENT_ENABLE:
        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_ENABLE_EVENTSIGNAL,
                NULL,                        // lpInBuffer
                0,                           // DWORD nInBufferSize
                NULL,                        // lpOutBuffer
                0,                           // DWORD nOutBufferSize
                &ReturnedDataLength))        // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            return S3_MOBILE_SUCCESS;
        }
        break;

    case S3ESC_MBL_EVENT_DISABLE:
        EngDeviceIoControl(
                ppdev->hDriver,             // hDevice
                IOCTL_VIDEO_S3_MBL_DISABLE_EVENTSIGNAL,
                NULL,                       // lpInBuffer
                0,                          // DWORD nInBufferSize
                (LPVOID)&ulOutputData,      // lpOutBuffer
                sizeof(ULONG),              // DWORD nOutBufferSize
                &ReturnedDataLength);       // LPDWORD lpBytesReturned

        return S3_MOBILE_SUCCESS;
        break;

    case S3ESC_MBL_EVENT_STATUS:            // AURORA only!
        if ( (cjOut != sizeof(ULONG)) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  first disable the event tracking and get the current
        //  status of the BIOS scratch register
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,             // hDevice
                IOCTL_VIDEO_S3_MBL_DISABLE_EVENTSIGNAL,
                NULL,                       // lpInBuffer
                0,                          // DWORD nInBufferSize
                (LPVOID)&EventReg,
                sizeof(ULONG),
                &ReturnedDataLength)        // LPDWORD lpBytesReturned
                ||
             (ReturnedDataLength != 4))
        {
            return S3_MOBILE_FAILURE;
        }

        *(PULONG)pvOut = 0L;
        if (EventReg & M1_EVREG_EVENT_BITS)
        {
            //
            //  IF the display switch bit is set
            //

            if (EventReg & M1_EVREG_DISPSW)
            {
                //
                //  EngDeviceIoControl returns 0 if no errors
                //

                if (EngDeviceIoControl(
                    ppdev->hDriver,             // hDevice
                    IOCTL_VIDEO_S3_MBL_DISPLAY_SWITCH,
                    NULL,                       // lpInBuffer
                    0,                          // DWORD nInBufferSize
                    (LPVOID)&ulOutputData,      // lpOutBuffer
                    sizeof(ULONG),              // DWORD nOutBufferSize
                    &ReturnedDataLength) == 0)  // LPDWORD lpBytesReturned
                {
                    if (cjOut == ReturnedDataLength)
                    {
                        //
                        //  return the return value
                        //

                        *(PULONG)pvOut = ulOutputData;

                        //
                        //  we know that during a display switch, the
                        //  BIOS also sets the power and resume
                        //  notification bits, so we will clear the
                        //  resume bit now if we plan to have a mode
                        //  set done
                        //

                        if (ulOutputData)
                        {
                            EventReg &= ~M1_EVREG_RESUME;
                        }
                    }
                }
            }

            if (EventReg & M1_EVREG_RESUME)
            {
#if APM_SUPPORT
                RestoreShadowRegs (ppdev);
#endif
                EventReg &= ~M1_EVREG_RESUME;
            }

        }

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_ENABLE_EVENTSIGNAL,
                NULL,                        // lpInBuffer
                0,                           // DWORD nInBufferSize
                NULL,                        // lpOutBuffer
                0,                           // DWORD nOutBufferSize
                &ReturnedDataLength))        // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }

        return S3_MOBILE_SUCCESS;
        break;

    case S3ESC_MBL_DISPLAY_STATUS:
        if ( (cjOut != sizeof(ULONG)) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        if (EngDeviceIoControl(
                ppdev->hDriver,             // hDevice
                IOCTL_VIDEO_S3_MBL_DISPLAY_STATUS,
                NULL,                       // lpInBuffer
                0,                          // DWORD nInBufferSize
                (LPVOID)&ulDisplayStatusReg,
                sizeof(ULONG),
                &ReturnedDataLength)        // LPDWORD lpBytesReturned
                ||
             (ReturnedDataLength != 4))
        {
            return S3_MOBILE_FAILURE;
        }

        *(PULONG)pvOut = ulDisplayStatusReg;

        return S3_MOBILE_SUCCESS;
        break;

    case S3ESC_MBL_SET_APP_HWND:

        if ( (cjIn < 8) || (pvIn == NULL) )
            return S3_MOBILE_FAILURE;

        ulInputData = *( (ULONG *)pvIn + 1);

        if (EngDeviceIoControl(
            ppdev->hDriver,             // hDevice
            IOCTL_VIDEO_S3_MBL_SET_APP_HWND,
            &ulInputData,               // lpInBuffer
            sizeof(ULONG),              // DWORD nInBufferSize
            NULL,                       // lpOutBuffer
            0,                          // DWORD nOutBufferSize
            &ReturnedDataLength) == 0)  // LPDWORD lpBytesReturned
        {
           return S3_MOBILE_SUCCESS;
        }
        else
        {
           return S3_MOBILE_FAILURE;
        }
        break;

    case S3ESC_MBL_GET_APP_HWND:

        if ( (cjOut < sizeof(ULONG)) || (pvOut == NULL) )
        {
            return S3_MOBILE_FAILURE;
        }

        if (EngDeviceIoControl(
            ppdev->hDriver,             // hDevice
            IOCTL_VIDEO_S3_MBL_GET_APP_HWND,
            NULL,                       // lpInBuffer
            0,                          // DWORD nInBufferSize
            (LPVOID)&ulOutputData,      // lpOutBuffer
            sizeof(ULONG),              // DWORD nOutBufferSize
            &ReturnedDataLength) == 0)  // LPDWORD lpBytesReturned
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the return value
                //

                *(PULONG)pvOut = ulOutputData;
            }
            return S3_MOBILE_SUCCESS;
        }
        else
        {
            return S3_MOBILE_FAILURE;
        }
        break;

    case S3ESC_MBL_CENTER_TV_VIEWPORT_NOW:

        // no input or output parameters.

        if (EngDeviceIoControl(
            ppdev->hDriver,             // hDevice
            IOCTL_VIDEO_S3_MBL_CTR_TV_VIEWPORT_NOW,
            NULL,                       // lpInBuffer
            0,                          // DWORD nInBufferSize
            NULL,
            0,
            &ReturnedDataLength))       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }

        return S3_MOBILE_SUCCESS;

        break;

    case S3ESC_MBL_QUERY_DEVICE_SUPPORT:

        if ( (cjIn < 4) || !pvIn || (cjOut < 4) || !pvOut )
        {
            return( S3_MOBILE_FAILURE );
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl( ppdev->hDriver,
                            IOCTL_VIDEO_S3_MBL_QUERY_MOBILE_SUPPORT,
                            NULL,
                            0,
                            (LPVOID)&DisplaySupport,
                            cjOut,
                            &ReturnedDataLength))
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {

            if ((cjOut >= 4) && (ReturnedDataLength >= 4))
            {
                ppdev->ulDisplayDevicesSupported =
                    DisplaySupport.SupportedDisplayTypes;

                ((MBL_OUTP_GET_DEVICE_SUPPORT*)
                    pvOut)->MblDisplayDeviceSupport =
                    (ppdev->ulDisplayDevicesSupported & S3_CRTLCD_MASK);

                if ( ppdev->ulDisplayDevicesSupported >
                   (S3_CRT_SUPPORT | S3_LCD_SUPPORT) )
                {
                    ((MBL_OUTP_GET_DEVICE_SUPPORT*)
                        pvOut)->MblDisplayDeviceSupport |= S3_TV_SUPPORT;
                }

                if ((cjOut >= 8) && (ReturnedDataLength >= 8))
                {
                    ((MBL_OUTP_GET_DEVICE_SUPPORT*)
                        pvOut)->MblTvStandardSupport =
                        DisplaySupport.SupportedTvStandards;
                }

                return S3_MOBILE_SUCCESS;

            }
            else
            {
                return S3_MOBILE_FAILURE;
            }

        }

        break;

    case S3ESC_MBL_VALIDATE_HW_OVERLAY:

        //
        // currently no input buffer
        //

        if ( (cjOut < 4) || !pvOut )
        {
            return( S3_MOBILE_FAILURE );
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl( ppdev->hDriver,
                IOCTL_VIDEO_S3_MBL_QUERY_HWOVERLAY_POSSIBLE,
                NULL,
                0,
                (LPVOID)&ulOutputData,
                cjOut,
                &ReturnedDataLength))
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            if (cjOut == ReturnedDataLength)
            {
                //
                //  return the return value
                //

                *(PULONG)pvOut = ulOutputData;

                return S3_MOBILE_SUCCESS;
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }

        }

        break;

    case S3ESC_MBL_VALIDATE_DUOVIEW:

        //
        // currently no input or output buffer
        //

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl( ppdev->hDriver,
                IOCTL_VIDEO_S3_MBL_QUERY_DUOVIEW_POSSIBLE,
                NULL,
                0,
                NULL,
                0,
                NULL))
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            return S3_MOBILE_SUCCESS;

        }

        break;

    case S3ESC_MBL_GET_CRT_REFRESH_RATES:
        if ( (pvIn == NULL)  ||
             (cjIn < sizeof(MBL_INP_GET_CRT_REFRESH_RATES)) ||
             (pvOut == NULL) ||
             (cjOut == 0) )
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to get the information
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,
                IOCTL_VIDEO_S3_MBL_GET_CRT_REFRESH_RATES,
                pvIn,
                cjIn,
                pvOut,
                cjOut,
                &ReturnedDataLength) )
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            //
            //  return success for pvOut updated OK!
            //

            if (ReturnedDataLength <= cjOut)
            {
                return S3_MOBILE_SUCCESS;
            }
            else
            {
                return S3_MOBILE_FAILURE;
            }
        }
        break;

    case S3ESC_MBL_SET_CRT_REFRESH_RATE:

        if ((pvIn == NULL) ||
            (cjIn < sizeof(MBL_INP_SET_CRT_REFRESH_RATE)))
        {
            return S3_MOBILE_FAILURE;
        }

        //
        //  call the miniport to set the new freq.
        //

        if (EngDeviceIoControl(
                ppdev->hDriver,              // hDevice
                IOCTL_VIDEO_S3_MBL_SET_CRT_REFRESH_RATE,// dwIoControlCode
                pvIn,                        // lpInBuffer
                cjIn,                        // DWORD nInBufferSize
                pvOut,                       // lpOutBuffer
                cjOut,
                &ReturnedDataLength) )       // LPDWORD lpBytesReturned
        {
            return S3_MOBILE_FAILURE;
        }
        else
        {
            //
            //  no output data
            //

            return (S3_MOBILE_SUCCESS);
        }
        break;

    case S3ESC_MBL_STOP_FORCE_SW_POINTER:

        ppdev->bForceSwPointer = FALSE;
        
        if ( EngDeviceIoControl( 
                ppdev->hDriver,
                IOCTL_VIDEO_S3_MBL_STOP_FORCE_SW_POINTER,
                NULL,      
                0,
                NULL,         
                0,
                &ReturnedDataLength))
        {
            DISPDBG((0, "bEnablePointer - error to turn off s/w pointer"));
        }
        break;

    default:
        //
        //  unsupported mobile/multi-display subfunction
        //

        return (S3_MOBILE_NOT_IMPLEMENTED);
    }
    return (S3_MOBILE_NOT_IMPLEMENTED); //unsupported if comes here - needed here or VC6.0 complained
}



