/******************************Module*Header*******************************\
* Module Name: pointer.c
*
* This module contains the hardware pointer support for the display
* driver.  This supports both the built-in S3 hardware pointer and
* some common DAC hardware pointers.
*
* Copyright (c) 1992-1996 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"

#include <ntddvdeo.h>

#include "hw3d.h"

/******************************Public*Routine******************************\
* VOID vNewMmIoShowPointerS3
*
* Show or hide the S3 hardware pointer.
*
* PARAMETERS: PDEV* ppdev
*             VOID* ptp
*             BOOL  bShow  (if TRUE, show the pointer,
*                           if FALSE, hide the pointer)
*
*
\**************************************************************************/

VOID vNewMmIoShowPointerS3(
PDEV*   ppdev,
VOID*   ptp,
BOOL    bShow)
{

    if ( bShow )
    {
        WRITE_WORD ( ppdev->pjMmBase + MM_CRTC_INDEX,
            ppdev->ulHwGraphicsCursorModeRegister_45 | (HGC_ENABLE << 8) );

        ppdev->bHwPointerActive = TRUE;

    }
    else
    {
        WRITE_WORD ( ppdev->pjMmBase + MM_CRTC_INDEX,
                     ppdev->ulHwGraphicsCursorModeRegister_45 );

        ppdev->bHwPointerActive = FALSE;

        if ( ppdev->flStatus & STAT_STREAMS_ENABLED )
        {
            LONG    x   = ppdev->cxScreen + HW_POINTER_DIMENSION;
            LONG    y   = ppdev->cyScreen + HW_POINTER_DIMENSION;
            LONG    dx  = 0;
            LONG    dy  = HW_POINTER_HIDE;

            //
            // Note that due to register shadowing, these need to be written
            // in a specific order, otherwise you may get a flashing pointer:
            //

            WRITE_WORD ( ppdev->pjMmBase + MM_CRTC_INDEX,
                         HGC_ORGX_MSB | ((x >> 8)   << 8));

            WRITE_WORD ( ppdev->pjMmBase + MM_CRTC_INDEX,
                         HGC_ORGX_LSB | ((x & 0xff) << 8));

            WRITE_WORD ( ppdev->pjMmBase + MM_CRTC_INDEX,
                         HGC_ORGY_LSB | ((y & 0xff) << 8));

            WRITE_WORD ( ppdev->pjMmBase + MM_CRTC_INDEX,
                         HGC_DX       | ((dx)       << 8));

            WRITE_WORD ( ppdev->pjMmBase + MM_CRTC_INDEX,
                         HGC_DY       | ((dy)       << 8));

            WRITE_WORD ( ppdev->pjMmBase + MM_CRTC_INDEX,
                         HGC_ORGY_MSB | ((y >> 8)   << 8));

            //
            //  save the new pointer location in ppdev
            //

            ppdev->xPointer  = x;
            ppdev->yPointer  = y;
        }
    }

    return;
}

/******************************Public*Routine******************************\
* VOID vNewMmIoMovePointerS3
*
* Move the S3 hardware pointer.
*
\**************************************************************************/

VOID vNewMmIoMovePointerS3(
PDEV*   ppdev,
VOID*   pbp,
LONG    x,
LONG    y)
{
    LONG    dx;
    LONG    dy;
    DWORD   status;
    ULONG   ReturnedDataLength;
    S3_POINTER_POSITION  InputPosition, UpdatedPosition;
    MOBILE_INTERFACE MobileInterface;

    //
    //  if panning is enabled, call the miniport to pan the screen if needed
    //

    if ( ppdev->ulCaps & CAPS_PANNING )
    {
        //
        //  pass to miniport to handle screen panning
        //

        InputPosition.Column           = x;
        InputPosition.Row              = y;

        //
        //  For ViRGE/GX2 family only (not ViRGE/MX)
        //
        if ((!EngDeviceIoControl( ppdev->hDriver, // hDevice
                                 IOCTL_VIDEO_S3_MBL_GET_MOBILE_INTERFACE,
                                 NULL,
                                 0,
                                 (LPVOID)&MobileInterface,
                                 sizeof(MOBILE_INTERFACE),
                                 &ReturnedDataLength) ) &&
            ReturnedDataLength == sizeof(MOBILE_INTERFACE))
        {
            if ( (MobileInterface.ulSubType >= SUBTYPE_357) &&
                 (MobileInterface.ulSubType <= SUBTYPE_359) )
            {
                InputPosition.PanMemOffsetIga1 =
                InputPosition.PanMemOffsetIga2 = (ULONG)ppdev->fpVisibleBuffer;
            }
        }

        //
        //  EngDeviceIoControl returns 0 if no errors
        //

        status = EngDeviceIoControl (   ppdev->hDriver,
                                        IOCTL_VIDEO_SET_POINTER_POSITION,
                                        &InputPosition,
                                        sizeof (S3_POINTER_POSITION),
                                        &UpdatedPosition,
                                        sizeof (S3_POINTER_POSITION),
                                        &ReturnedDataLength );

        if ( !status && (ReturnedDataLength == sizeof (S3_POINTER_POSITION)) )
        {
            //
            //  update the cursor screen position for the current
            //  aperture position
            //
            //  NOTE:   the values programmed into the HW cursor position regs
            //          are screen relative, not frame buffer relative.  The
            //          miniport will pass back screen relative positions in
            //          case the screen has been panned from the upper left
            //          corner.
            //

            x = UpdatedPosition.Column;
            y = UpdatedPosition.Row;

            if (ppdev->ulCaps & CAPS_PANNING)
            {
                ppdev->ApertureIga1 = UpdatedPosition.PanMemOffsetIga1;
                ppdev->ApertureIga2 = UpdatedPosition.PanMemOffsetIga2;
            }

            if ((ppdev->ApertureXOffset != UpdatedPosition.ApertureX)    ||
                (ppdev->ApertureYOffset != UpdatedPosition.ApertureY))
            {
                ppdev->ApertureXOffset = UpdatedPosition.ApertureX;
                ppdev->ApertureYOffset = UpdatedPosition.ApertureY;

                if ((ppdev->flStatus & STAT_STREAMS_ENABLED)    &&
                    (ppdev->Band.DuoViewFlags & DVW_Panning_ON))
                {
                    ddUpdateSecondaryStreams (ppdev, SS_UPDATE_PANNING);
                }
            }
        }
    }

    // 'dx' and 'dy' are the offsets into the pointer bitmap at which
    // the hardware is supposed to start drawing, when the pointer is
    // along the left or top edge and needs to be clipped:

    x -= ppdev->xPointerHot;
    y -= ppdev->yPointerHot;

    dx = 0;
    dy = 0;

    if (x < 0)
    {
        dx = -x;
        x  = 0;
    }

    if (y < 0)
    {
        dy = -y;
        y  = 0;
    }

    //
    // Account for pointer position scaling in high-colour modes:
    //

    x <<= ppdev->cPointerShift;

    //
    //  save new cursor information
    //
    ppdev->dxPointer = dx;
    ppdev->dyPointer = dy;
    ppdev->xPointer  = x;
    ppdev->yPointer  = y;

    //
    //  the M65 has a hardware cursor bug, if CR48 & CR49 are programmed
    //  with 0, the cursor disappears.
    //
    if ( ( y == 0 ) &&
         ( ppdev->ulCaps & CAPS_CURSOR_Y0BUG ) )
    {
        y = 1;
    }

    //
    //  Note that due to register shadowing, these need to be written
    //  in a specific order, otherwise you may get a flashing pointer:
    //
    WRITE_WORD ( (BYTE *)(ppdev->pjMmBase + MM_CRTC_INDEX),
                            HGC_ORGX_MSB | ((x >> 8)   << 8));
    WRITE_WORD ( (BYTE *)(ppdev->pjMmBase + MM_CRTC_INDEX),
                            HGC_ORGX_LSB | ((x & 0xff) << 8));
    WRITE_WORD ( (BYTE *)(ppdev->pjMmBase + MM_CRTC_INDEX),
                            HGC_ORGY_LSB | ((y & 0xff) << 8));
    WRITE_WORD ( (BYTE *)(ppdev->pjMmBase + MM_CRTC_INDEX),
                            HGC_DX       | ((dx)       << 8));
    WRITE_WORD ( (BYTE *)(ppdev->pjMmBase + MM_CRTC_INDEX),
                            HGC_DY       | ((dy)       << 8));
    WRITE_WORD ( (BYTE *)(ppdev->pjMmBase + MM_CRTC_INDEX),
                            HGC_ORGY_MSB | ((y >> 8)   << 8));

}


/******************************Public*Routine******************************\
* VOID vNewMmIoSetPointerShapeS3
*
* Using NewMmIO
*
\**************************************************************************/

BOOL vNewMmIoSetPointerShapeS3(
PDEV*       ppdev,
VOID*       pbp,
LONG        x,              // Relative coordinates
LONG        y,              // Relative coordinates
LONG        xHot,
LONG        yHot,
LONG        cx,
LONG        cy,
BYTE*       pjShape,
FLONG       fl)
{
    ULONG*  pulDst;
    ULONG*  pulSrc;
    LONG    i;

    //
    //  now transfer the new shape to the offscreen save buffer and display
    //  the new shape.
    //
    //  1. Hide the current pointer.
    //
    if (!(fl & SPS_ANIMATEUPDATE))
    {
        //
        // Hide the pointer to try and lessen the jumpiness when the
        // new shape has a different hot spot.  We don't hide the
        // pointer while animating, because that definitely causes
        // flashing:
        //

        ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);
        WRITE_WORD ( (BYTE *)(ppdev->pjMmBase + MM_CRTC_INDEX),
                        ppdev->ulHwGraphicsCursorModeRegister_45);

        RELEASE_CRTC_CRITICAL_SECTION(ppdev);
    }

    //
    //  2. Set the new pointer position.
    //
    ppdev->xPointerHot = xHot;    // save the new hot spot
    ppdev->yPointerHot = yHot;

    //  Note: Must pass relative coordinates!
    ppdev->pfnMovePointer(ppdev, NULL, x, y);

    //
    //  3. Download the new pointer shape to offscreen memory.
    //
    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    pulSrc = (ULONG*) pjShape;
    pulDst = (ULONG*) ppdev->pvPointerShape;

    if (DIRECT_ACCESS(ppdev))
    {
        for (i = HW_POINTER_TOTAL_SIZE / sizeof(ULONG); i != 0; i--)
        {
            *pulDst++ = *pulSrc++;
        }
    }
    else
    {
        for (i = HW_POINTER_TOTAL_SIZE / sizeof(ULONG); i != 0; i--)
        {
            WRITE_REGISTER_ULONG(pulDst, *pulSrc);
            pulSrc++;
            pulDst++;
        }
    }

    //
    //  4. Turn on the cursor
    //
    ppdev->pfnShowPointer(ppdev, ppdev->pvPointerData, TRUE);

    RELEASE_CRTC_CRITICAL_SECTION(ppdev);

    return(TRUE);
}


/******************************Public*Routine******************************\
* VOID vSetPointerShapeS3withWait
*
* Using NewMmIO with wait for vertical retrace
*
\**************************************************************************/

BOOL vSetPointerShapeS3withWait(
PDEV*       ppdev,
VOID*       pbp,
LONG        x,              // Relative coordinates
LONG        y,              // Relative coordinates
LONG        xHot,
LONG        yHot,
LONG        cx,
LONG        cy,
BYTE*       pjShape,
FLONG       fl)
{
    ULONG*  pulDst;
    ULONG*  pulSrc;
    LONG    i;

    //
    //  now transfer the new shape to the offscreen save buffer and display
    //  the new shape.
    //
    //  1. Hide the current pointer.
    //
    if (!(fl & SPS_ANIMATEUPDATE))
    {
        //
        // Hide the pointer to try and lessen the jumpiness when the
        // new shape has a different hot spot.  We don't hide the
        // pointer while animating, because that definitely causes
        // flashing:
        //

        ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);
        WRITE_WORD ( (BYTE *)(ppdev->pjMmBase + MM_CRTC_INDEX),
                        ppdev->ulHwGraphicsCursorModeRegister_45);

        RELEASE_CRTC_CRITICAL_SECTION(ppdev);
    }

    //
    //  2. Set the new pointer position.
    //
    ppdev->xPointerHot = xHot;          // save the new hot spot
    ppdev->yPointerHot = yHot;

    // If we don't wait for vertical retrace here, the S3 sometimes ignores
    // the setting of the new pointer position:

    while (VGAINP(ppdev, STATUS_1) & VBLANK_ACTIVE)
        ;                               // Wait for bit 3 to become 0
    while (!(VGAINP(ppdev, STATUS_1) & VBLANK_ACTIVE))
        ;                               // Wait for bit 3 to become 1

    //  Note: Must pass relative coordinates!
    ppdev->pfnMovePointer(ppdev, NULL, x, y);

    //
    //  3. Download the new pointer shape to offscreen memory.
    //
    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    pulSrc = (ULONG*) pjShape;
    pulDst = (ULONG*) ppdev->pvPointerShape;

    if (DIRECT_ACCESS(ppdev))
    {
        for (i = HW_POINTER_TOTAL_SIZE / sizeof(ULONG); i != 0; i--)
        {
            *pulDst++ = *pulSrc++;
        }
    }
    else
    {
        for (i = HW_POINTER_TOTAL_SIZE / sizeof(ULONG); i != 0; i--)
        {
            WRITE_REGISTER_ULONG(pulDst, *pulSrc);
            pulSrc++;
            pulDst++;
        }
    }

    //
    //  4. Turn on the cursor
    //
    ppdev->pfnShowPointer(ppdev, ppdev->pvPointerData, TRUE);

    RELEASE_CRTC_CRITICAL_SECTION(ppdev);

    return(TRUE);
}


/******************************Public*Routine******************************\
* VOID vEnablePointerTi025
*
* Get the hardware ready to use the S3 hardware pointer.
*
* Don't do word outs to the DAC because they may not be performed correctly
* on some ISA machines.
*
\**************************************************************************/

VOID vEnablePointerS3(
PDEV*               ppdev,
VOID*               ptp,
BOOL                bEnable
)
{
    ULONG*  pulDst;
    LONG    i;
    LONG    lPointerShape;

        if (bEnable)
        {
            ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

            //
            // We download an invisible pointer shape because we're about
            // to enable the hardware pointer, but we still want the
            // pointer hidden until we get the first DrvSetPointerShape
            // call:
            //
            pulDst = (ULONG*) ppdev->pvPointerShape;

            if (DIRECT_ACCESS(ppdev))
            {

                for (i = HW_POINTER_TOTAL_SIZE / sizeof(ULONG); i != 0; i--)
                {
                   *pulDst++ = 0x0000ffff;
                }
            }
            else
            {
                for (i = HW_POINTER_TOTAL_SIZE / sizeof(ULONG); i != 0; i--)
                {
                    WRITE_REGISTER_ULONG(pulDst, 0x0000ffff);
                      pulDst++;
                }
            }

            //
            //  Point the S3 to where we're storing the pointer shape.
            //  The location is specified as a multiple of 1024:
            //
            lPointerShape = ppdev->cjPointerOffset / 1024;

            WRITE_WORD ( (BYTE *)(ppdev->pjMmBase + MM_CRTC_INDEX),
                        CR4C | ((lPointerShape >> 8)   << 8));
            WRITE_WORD ( (BYTE *)(ppdev->pjMmBase + MM_CRTC_INDEX),
                        CR4D | ((lPointerShape & 0xff) << 8));

            //
            //  Enable the hardware pointer.  As per the 8/31/93 Design
            //  Alert from S3 Incorporated, there's a goofy bug in all
            //  S3 chips up to the 928 where writing to this register
            //  at the same time as a horizontal sync may cause the
            //  chip to crash.  So we watch for the vertical sync.
            //
            //  Note that since we're a preemptive multitasking
            //  operating system, the following code is not guaranteed
            //  to be safe.  To do that, we would have to put this in
            //  the miniport, where we could disable all interrupts while
            //  we wait for the vertical sync.
            //
            //  However, this is only ever executed once at initialization
            //  and every time full-screen is executed, so I would expect
            //  the chances of there still being a problem to be extremely
            //  small:
            //
            WRITE_WORD ( (BYTE *)(ppdev->pjMmBase + MM_CRTC_INDEX),
                ppdev->ulHwGraphicsCursorModeRegister_45 | (HGC_ENABLE << 8));

            RELEASE_CRTC_CRITICAL_SECTION(ppdev);
        }
}


/******************************Public*Routine******************************\
* VOID DrvMovePointer
*
* NOTE: Because we have set GCAPS_ASYNCMOVE, this call may occur at any
*       time, even while we're executing another drawing call!
*
*       Consequently, we have to explicitly synchronize any shared
*       resources.  In our case, since we touch the CRTC register here,
*       we synchronize access using a critical section.
*
\**************************************************************************/

VOID DrvMovePointer(
SURFOBJ*    pso,
LONG        x,
LONG        y,
RECTL*      prcl)
{
    PDEV*   ppdev;
    ULONG   ReturnedDataLength;
    S3_POINTER_POSITION  InputPosition, UpdatedPosition;
    DWORD   status;
    MOBILE_INTERFACE  MobileInterface;

    ppdev = (PDEV*) pso->dhpdev;

    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    if (!EngDeviceIoControl( ppdev->hDriver,
                            IOCTL_VIDEO_S3_MBL_GET_MOBILE_INTERFACE,
                            NULL,
                            0,
                            (LPVOID)&MobileInterface,
                            sizeof(MOBILE_INTERFACE),
                            &ReturnedDataLength )  ||
                            ( ReturnedDataLength == sizeof(MOBILE_INTERFACE)))
    {

    //
    //  if x != -1, we need to draw the cursor
    //
    if (x != -1)
    {
        //
        //  if y is negative we are just being informed of the new position
        //  so we can handle screen panning
        //
        if ( y < 0 )
        {
            //
            //  only go check panning if we are not using the HW cursor,
            //  else let vMovePointerS3 handle the panning/cursor movement
            //

            if (((MobileInterface.ulSubType >= SUBTYPE_240) &&
                 (MobileInterface.ulSubType <= SUBTYPE_282) &&
                 (ppdev->flStatus & STAT_SW_CURSOR_PAN) )   ||
                 (!((MobileInterface.ulSubType >= SUBTYPE_240)&&
                 (MobileInterface.ulSubType <= SUBTYPE_282))))
            {

                //
                //  need to correct the negative Y by adding the bitmap size
                //

                y += pso->sizlBitmap.cy;

                //
                // Convert the pointer's position from relative to absolute
                // coordinates (this is only significant for multiple board
                // support):
#if MULTI_BOARDS
                OH* poh;
                poh = ((DSURF*) pso->dhsurf)->poh;
                x += poh->x;
                y += poh->y;
#endif
                //
                //  put cursor position in structure for call
                //

                InputPosition.Column           = x;
                InputPosition.Row              = y;

                if ( (MobileInterface.ulSubType >= SUBTYPE_357) &&
                    (MobileInterface.ulSubType <= SUBTYPE_359) )
                {
                    //
                    //  For ViRGE/GX2 family only (not ViRGE/MX)
                    //
                    InputPosition.PanMemOffsetIga1 =
                    InputPosition.PanMemOffsetIga2 = (ULONG)ppdev->fpVisibleBuffer;
                }

                //
                //  EngDeviceIoControl returns 0 if no errors
                //

                status = EngDeviceIoControl (   ppdev->hDriver,
                                        IOCTL_VIDEO_SET_POINTER_POSITION,
                                        &InputPosition,
                                        sizeof (S3_POINTER_POSITION),
                                        &UpdatedPosition,
                                        sizeof (S3_POINTER_POSITION),
                                        &ReturnedDataLength );

                if ( !status && (ReturnedDataLength ==
                                            sizeof (S3_POINTER_POSITION) ) )
                {
                    if (ppdev->ulCaps & CAPS_PANNING)
                    {
                        ppdev->ApertureIga1 = UpdatedPosition.PanMemOffsetIga1;
                        ppdev->ApertureIga2 = UpdatedPosition.PanMemOffsetIga2;
                    }

                    if ((ppdev->ApertureXOffset != UpdatedPosition.ApertureX)    ||
                        (ppdev->ApertureYOffset != UpdatedPosition.ApertureY))
                    {
                        ppdev->ApertureXOffset = UpdatedPosition.ApertureX;
                        ppdev->ApertureYOffset = UpdatedPosition.ApertureY;

                        if ((ppdev->flStatus & STAT_STREAMS_ENABLED)    &&
                            (ppdev->Band.DuoViewFlags & DVW_Panning_ON))
                        {
                            ddUpdateSecondaryStreams (ppdev, SS_UPDATE_PANNING);
                        }
                    }
                }
            }
        }

        //
        //  y >= 0, normal set position call
        //
        else
        {

            // Convert the pointer's position from relative to absolute
            // coordinates (this is only significant for multiple board
            // support):

        #if MULTI_BOARDS
            {
                OH* poh;

                poh = ((DSURF*) pso->dhsurf)->poh;
                x += poh->x;
                y += poh->y;
            }
        #endif


            if (!ppdev->bHwPointerActive )
            {
                // We have to make the pointer visible:

                // randre 5/22/97
                // DO NOT try to make pointer visible until we confirm the
                // the ppdev has valid Move and Show functions.  These funcs
                // are NULL if in the middle of a reset and in MODE 3!
                // Fix for SPR 14843.
                if (ppdev->pfnMovePointer && ppdev->pfnShowPointer)
                {
                    // HW cursor is off; coordinate may be out of
                    // date; wait for vertical blank to update the
                    // coordate first in order to avoid jumping cursor

                    WAIT_FOR_VBLANK( ppdev );
                    ppdev->pfnMovePointer(ppdev, ppdev->pvPointerData, x, y);
                    ppdev->pfnShowPointer(ppdev, ppdev->pvPointerData, TRUE);
                }
            }
            else
            {
                ppdev->pfnMovePointer(ppdev, ppdev->pvPointerData, x, y);
            }
        }
    }

    //
    //  x == -1, hide the cursor request
    //
    else
    {
        if (ppdev->bHwPointerActive)
        {

            ppdev->pfnShowPointer(ppdev, ppdev->pvPointerData, FALSE);
        }
    }
    }

    RELEASE_CRTC_CRITICAL_SECTION(ppdev);
}

/******************************Public*Routine******************************\
* VOID NewMmIoSetPointerShape
*
* Sets the new pointer shape.
*
\**************************************************************************/


ULONG NewMmIoSetPointerShape(
PDEV*       ppdev,
SURFOBJ*    psoMsk,
SURFOBJ*    psoColor,
XLATEOBJ*   pxlo,
LONG        xHot,
LONG        yHot,
LONG        x,
LONG        y,
RECTL*      prcl,
FLONG       fl,
BYTE*       pBuf)
{
    ULONG   cx;
    ULONG   cy;
    LONG    i;
    LONG    j;
    BYTE*   pjSrcScan;
    BYTE*   pjDstScan;
    LONG    lSrcDelta;
    LONG    lDstDelta;
    WORD*   pwSrc;
    WORD*   pwDst;
    BYTE*   pbSrc;
    BYTE*   pbDst;

    ULONG*  pulDst;
    ULONG*  pulSrc;

    LONG    cxWhole;
    LONG    xHotWordBnd;

    ULONG   ulTransp = 0xFFFF0000L;
    ULONG   ulData, ulPreviousData;

    UCHAR ucTemp;



    // We're not going to handle any colour pointers, pointers that
    // are larger than our hardware allows, or flags that we don't
    // understand.
    //
    // (Note that the spec says we should decline any flags we don't
    // understand, but we'll actually be declining if we don't see
    // the only flag we *do* understand...)
    //
    // Our old documentation says that 'psoMsk' may be NULL, which means
    // that the pointer is transparent.  Well, trust me, that's wrong.
    // I've checked GDI's code, and it will never pass us a NULL psoMsk:

    cx = psoMsk->sizlBitmap.cx;         // Note that 'sizlBitmap.cy' accounts
    cy = psoMsk->sizlBitmap.cy >> 1;    //   for the double height due to the
                                        //   inclusion of both the AND masks
                                        //   and the XOR masks.  For now, we're
                                        //   only interested in the true
                                        //   pointer dimensions, so we divide
                                        //   by 2.


    //
    // 'psoMsk' is actually cy * 2 scans high; the first 'cy' scans
    // define the AND mask.  So we start with that:

    pjSrcScan    = psoMsk->pvScan0;
    lSrcDelta    = psoMsk->lDelta;
    lDstDelta    = HW_POINTER_DIMENSION / 4; // Every 8 pels is one AND/XOR word

    cxWhole      = cx / 16;                 // Each word accounts for 16 pels

    // calculating pointer checksum whether update the pointer or not
    pulSrc = (ULONG*) pjSrcScan;
    ulData = 0L;

    if (cx == 32)        // DWORD boundary
    {
        for (i = cy; i != 0; i--)
        {
            ulData += *pulSrc;
            ((PUCHAR)pulSrc) += lSrcDelta;
        }
    }
    else
    {
        for (i = cy; i != 0; i--)
        {
            pwSrc = (WORD*) pulSrc;

            for (j = cxWhole; j != 0; j--)
            {
                ulData += (ULONG) *pwSrc;
                pwSrc += 1;             // Go to next word in source mask
            }
            ((PUCHAR)pulSrc) += lSrcDelta;
        }
    }

    if (ulData == ppdev->ulPointerCheckSum)  // same HW cursor
    {
        ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);
        if (x < 0)           // GDI just wants to turn off the HW cursor
        {
            if (ppdev->bHwPointerActive == TRUE)
            {
                ppdev->pfnShowPointer(ppdev, ppdev->pvPointerData, FALSE);
            }
        }
        else
        {
            if ( (x != ppdev->xPointer) || (y != ppdev->yPointer) )
                ppdev->pfnMovePointer(ppdev, NULL, x, y);
            ppdev->pfnShowPointer(ppdev, ppdev->pvPointerData, TRUE);
        }

        RELEASE_CRTC_CRITICAL_SECTION(ppdev);
        return(SPS_ACCEPT_NOEXCLUDE);
    }

    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    if (!(fl & SPS_ANIMATEUPDATE))
        ppdev->pfnShowPointer(ppdev, ppdev->pvPointerData, FALSE);

    if (x >= 0)
    {
        if ( (x != ppdev->xPointer) || (y != ppdev->yPointer) )
                ppdev->pfnMovePointer(ppdev, NULL, x, y);
    }

    ppdev->ulPointerCheckSum = ulData;


    RELEASE_CRTC_CRITICAL_SECTION(ppdev);


    // Now we're going to take the requested pointer AND masks and XOR
    // masks and combine them into our work buffer, being careful of
    // the edges so that we don't disturb the transparency when the
    // requested pointer size is not a multiple of 16.


    pulDst = (ULONG*) pBuf;

    for (i = 0; i < HW_POINTER_TOTAL_SIZE / sizeof(ULONG); i++)
    {
        // Here we initialize the entire pointer work buffer to be
        // transparent (the S3 has no means of specifying a pointer size
        // other than 64 x 64 -- so if we're asked to draw a 32 x 32
        // pointer, we want the unused portion to be transparent).
        //
        // The S3's hardware pointer is defined by an interleaved pattern
        // of AND words and XOR words.  So a totally transparent pointer
        // starts off with the word 0xffff, followed by the word 0x0000,
        // followed by 0xffff, etc..  Since we're a little endian system,
        // this is simply the repeating dword '0x0000ffff'.
        //
        // The compiler is nice enough to optimize this into a REP STOSD
        // for us:

        *pulDst++ = 0x0000ffff;
    }

    // ekl - take care word bnd.
    // Start with first AND word
    pjDstScan = (BYTE *) pBuf;

    pjDstScan +=  ((HW_POINTER_DIMENSION / 2 - yHot) * lDstDelta +
        (HW_POINTER_DIMENSION / 2 - ((xHot+15) & 0xFFFFFFF0L)) / 4);

    cxWhole      = cx / 16;                 // Each word accounts for 16 pels


    xHotWordBnd = xHot % 16;

    if (xHotWordBnd)
    {
        ulTransp >>= (16 - xHotWordBnd);
        cxWhole *= 2;

        for (i = cy; i != 0; i--)
        {
            pbSrc = pjSrcScan;
            pbDst = pjDstScan;

            ulPreviousData = ulTransp << 16;

            for (j = 0;  j < cxWhole; j++, pbSrc++)
            {
                ulData = (ULONG) (*pbSrc);
                ulData <<= (8 + xHotWordBnd);
                ulData |= ulPreviousData;

                ucTemp = (UCHAR)(ulData >> 24);
                *pbDst = ucTemp;

                pbDst += (j % 2 ? 3 : 1);

                // next byte
                ulData <<= 8;
                ulPreviousData = ulData;

            }

            // last word
            ulData |= ulTransp;
            ucTemp = (UCHAR)(ulData >> 24);
            *pbDst = ucTemp;

            ++pbDst;
            ucTemp = (UCHAR)(ulData >> 16);
            *pbDst = ucTemp;

            pjSrcScan += lSrcDelta;
            pjDstScan += lDstDelta;
        }

    }
    else
    {
        for (i = cy; i != 0; i--)
        {
            pwSrc = (WORD*) pjSrcScan;
            pwDst = (WORD*) pjDstScan;

            for (j = cxWhole; j != 0; j--)
            {
                *pwDst = *pwSrc;
                pwSrc += 1;    // Go to next word in source mask
                pwDst += 2;    // Skip over the XOR word in the dest mask
            }

            pjSrcScan += lSrcDelta;
            pjDstScan += lDstDelta;
        }
    }


    // Now handle the XOR mask:

    pjDstScan = (BYTE *) pBuf;
    pjDstScan +=  (2 + (HW_POINTER_DIMENSION / 2 - yHot) * lDstDelta +
        (HW_POINTER_DIMENSION / 2 - ((xHot+15) & 0xFFFFFFF0L)) / 4);

    if (xHotWordBnd)
    {
        for (i = cy; i != 0; i--)
        {
            pbSrc = pjSrcScan;
            pbDst = pjDstScan;

            ulPreviousData = 0;

            for (j = 0;  j < cxWhole; j++, pbSrc++)
            {
                ulData = (ULONG) (*pbSrc);
                ulData <<= (8 + xHotWordBnd);
                ulData |= ulPreviousData;

                ucTemp = (UCHAR)(ulData >> 24);
                *pbDst = ucTemp;

                pbDst += (j % 2 ? 3 : 1);

                // Next byte
                ulData <<= 8;
                ulPreviousData = ulData;

            }

            ucTemp = (UCHAR)(ulData >> 24);
            *pbDst = ucTemp;

            ++pbDst;
            ucTemp = (UCHAR)(ulData >> 16);
            *pbDst = ucTemp;

            pjSrcScan += lSrcDelta;
            pjDstScan += lDstDelta;
        }
    }
    else
    {

        for (i = cy; i != 0; i--)
        {
            pwSrc = (WORD*) pjSrcScan;
            pwDst = (WORD*) pjDstScan;

            for (j = cxWhole; j != 0; j--)
            {
                *pwDst = *pwSrc;
                pwSrc += 1;             // Go to next word in source mask
                pwDst += 2;             // Skip over the AND word in the dest mask
            }

            pjSrcScan += lSrcDelta;
            pjDstScan += lDstDelta;
        }
    }


    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    pulSrc = (ULONG*) pBuf;
    pulDst = (ULONG*) ppdev->pvPointerShape;

    //
    //  Ensure ppdev->pvPointerShape is non-zero.
    //
    if (pulDst)
    {
        if (DIRECT_ACCESS(ppdev))
        {
            for (i = HW_POINTER_TOTAL_SIZE / sizeof(ULONG); i != 0; i--)
            {
                *pulDst++ = *pulSrc++;
            }
        }
        else
        {
            for (i = HW_POINTER_TOTAL_SIZE / sizeof(ULONG); i != 0; i--)
            {
                WRITE_REGISTER_ULONG(pulDst, *pulSrc);
                pulSrc++;
                pulDst++;
            }
        }
    }
    else
    {
        DISPDBG((0, "NewMmIoSetPointerShape: pulDst was NULL!"));
    }

    if (x >= 0)
    {
        // GDI wants to turn off the HW cursor
        // and update the shape

        ppdev->pfnShowPointer(ppdev, ppdev->pvPointerData, TRUE);
    }
    RELEASE_CRTC_CRITICAL_SECTION(ppdev);

    // fix the hot spot at the center of the HW cursor
    ppdev->xPointerHot = HW_POINTER_DIMENSION / 2;
    ppdev->yPointerHot = HW_POINTER_DIMENSION / 2;

    if (!(ppdev->flStatus & STAT_DDRAW_PAN))
    {
        ppdev->flStatus &= ~STAT_SW_CURSOR_PAN;
    }

    return(SPS_ACCEPT_NOEXCLUDE);

}


/******************************Public*Routine******************************\
* VOID DrvSetPointerShape
*
* Sets the new pointer shape.
*
\**************************************************************************/

ULONG DrvSetPointerShape(
SURFOBJ*    pso,
SURFOBJ*    psoMsk,
SURFOBJ*    psoColor,
XLATEOBJ*   pxlo,
LONG        xHot,
LONG        yHot,
LONG        x,
LONG        y,
RECTL*      prcl,
FLONG       fl)
{
    PDEV*   ppdev;
    DWORD*  pul;
    ULONG   cx;
    ULONG   cy;
    LONG    i;
    LONG    j;
    BYTE*   pjSrcScan;
    BYTE*   pjDstScan;
    LONG    lSrcDelta;
    LONG    lDstDelta;
    WORD*   pwSrc;
    WORD*   pwDst;
    LONG    cwWhole;
    BOOL    bAccept;
    BYTE    ajBuf[HW_POINTER_TOTAL_SIZE];

    ppdev = (PDEV*) pso->dhpdev;

    if (ppdev->bForceSwPointer == TRUE)
    {
        return(SPS_DECLINE);
    }

    // When CAPS_SW_POINTER is set, we have no hardware pointer available,
    // so we always ask GDI to simulate the pointer for us, using
    // DrvCopyBits calls:

    if (ppdev->ulCaps & CAPS_SW_POINTER)
    {
        return(SPS_DECLINE);
    }

    //
    // M5/GX2 on some revs:
    //    hw cursor doesn't work with streams enabled and a not mask
    //    (AND and XOR bit masks are 1) so just use a sw pointer
    //    whenever streams is enabled
    //

    if ( (ppdev->ulCaps2  & CAPS2_M5_STREAMS_CURSOR_BUG) &&
         (ppdev->flStatus & STAT_STREAMS_ENABLED) )
    {
        goto HideAndDecline;
    }

    //  We're not going to handle any colour pointers, pointers that
    //  are larger than our hardware allows, or flags that we don't
    //  understand.
    //
    //  (Note that the spec says we should decline any flags we don't
    //  understand, but we'll actually be declining if we don't see
    //  the only flag we *do* understand...)
    //
    //  Our old documentation says that 'psoMsk' may be NULL, which means
    //  that the pointer is transparent.  Well, trust me, that's wrong.
    //  I've checked GDI's code, and it will never pass us a NULL psoMsk:

    cx = psoMsk->sizlBitmap.cx;         // Note that 'sizlBitmap.cy' accounts
    cy = psoMsk->sizlBitmap.cy >> 1;    //   for the double height due to the
                                        //   inclusion of both the AND masks
                                        //   and the XOR masks.  For now, we're
                                        //   only interested in the true
                                        //   pointer dimensions, so we divide
                                        //   by 2.

    //  We reserve the bottom scan of the pointer shape and keep it
    //  empty so that we can hide the pointer by changing the S3's
    //  display start y-pixel position register to show only the bottom
    //  scan of the pointer shape:

    if ((cx > HW_POINTER_DIMENSION)       ||
        (cy > (HW_POINTER_DIMENSION - 1)) ||
        (psoColor != NULL)                ||
        !(fl & SPS_CHANGE)                ||
        (cx & 0x7))     // make sure cx is a multiple of 8 (byte aligned).
    {
        goto HideAndDecline;
    }

    ASSERTDD(psoMsk != NULL, "GDI gave us a NULL psoMsk.  It can't do that!");

    if ( (cx <= HW_POINTER_DIMENSION / 2)    &&
        !(ppdev->ulCaps & CAPS_DAC_POINTER) )
    {
        return( NewMmIoSetPointerShape(
            ppdev,
            psoMsk,
            psoColor,
            pxlo,
            xHot,
            yHot,
            x,
            y,
            prcl,
            fl,
            ajBuf
            ));
    }


    pul = (ULONG*) &ajBuf[0];
    for (i = HW_POINTER_TOTAL_SIZE / sizeof(ULONG); i != 0; i--)
    {
        //
        //  Here we initialize the entire pointer work buffer to be
        //  transparent (the S3 has no means of specifying a pointer size
        //  other than 64 x 64 -- so if we're asked to draw a 32 x 32
        //  pointer, we want the unused portion to be transparent).
        //
        //  The S3's hardware pointer is defined by an interleaved pattern
        //  of AND words and XOR words.  So a totally transparent pointer
        //  starts off with the word 0xffff, followed by the word 0x0000,
        //  followed by 0xffff, etc..  Since we're a little endian system,
        //  this is simply the repeating dword '0x0000ffff'.
        //
        //  The compiler is nice enough to optimize this into a REP STOSD
        //  for us:
        //

        *pul++ = 0x0000ffff;
    }

    //
    //  Now we're going to take the requested pointer AND masks and XOR
    //  masks and combine them into our work buffer, being careful of
    //  the edges so that we don't disturb the transparency when the
    //  requested pointer size is not a multiple of 16.
    //
    //  'psoMsk' is actually cy * 2 scans high; the first 'cy' scans
    //  define the AND mask.  So we start with that:

    pjSrcScan    = psoMsk->pvScan0;
    lSrcDelta    = psoMsk->lDelta;
    pjDstScan    = &ajBuf[0];               // Start with first AND word
    lDstDelta    = HW_POINTER_DIMENSION / 4;// Every 8 pels is one AND/XOR word

    cwWhole      = cx / 16;                 // Each word accounts for 16 pels

    for (i = cy; i != 0; i--)
    {
        pwSrc = (WORD*) pjSrcScan;
        pwDst = (WORD*) pjDstScan;

        for (j = cwWhole; j != 0; j--)
        {
            *pwDst = *pwSrc;
            pwSrc += 1;             // Go to next word in source mask
            pwDst += 2;             // Skip over the XOR word in the dest mask
        }

        pjSrcScan += lSrcDelta;
        pjDstScan += lDstDelta;
    }

    // Now handle the XOR mask:

    pjDstScan = &ajBuf[2];          // Start with first XOR word
    for (i = cy; i != 0; i--)
    {
        pwSrc = (WORD*) pjSrcScan;
        pwDst = (WORD*) pjDstScan;

        for (j = cwWhole; j != 0; j--)
        {
            *pwDst = *pwSrc;
            pwSrc += 1;             // Go to next word in source mask
            pwDst += 2;             // Skip over the AND word in the dest mask
        }

        pjSrcScan += lSrcDelta;
        pjDstScan += lDstDelta;
    }

    //  Okay, I admit it -- I'm wildly inconsistent here.  I pass
    //  absolute (x, y) coordinates to pfnSetPointerShape, but pass
    //  relative (x, y) coordinates to vSetPointerShapeS3.
    //  Future clean up...

    #if MULTI_BOARDS
    {
        OH*  poh;
        if (x != -1)
        {
            poh = ((DSURF*) pso->dhsurf)->poh;
            x += poh->x;
            y += poh->y;
        }
    }
    #endif

    bAccept = ppdev->pfnSetPointerShape(ppdev, ppdev->pvPointerData, x, y,
                                        xHot, yHot, cx, cy, &ajBuf[0], fl);

    if (!bAccept)
        goto HideAndDecline;

    //  Since it's a hardware pointer, GDI doesn't have to worry about
    //  overwriting the pointer on drawing operations (meaning that it
    //  doesn't have to exclude the pointer), so we return 'NOEXCLUDE'.
    //  Since we're returning 'NOEXCLUDE', we also don't have to update
    //  the 'prcl' that GDI passed us.

    //
    //  Clear the SW Cursor panning flag
    //
    if (!(ppdev->flStatus & STAT_DDRAW_PAN))
    {
        ppdev->flStatus &= ~STAT_SW_CURSOR_PAN;
    }

    return(SPS_ACCEPT_NOEXCLUDE);

HideAndDecline:

    //  Since we're declining the new pointer, GDI will simulate it via
    //  DrvCopyBits calls.  So we should really hide the old hardware
    //  pointer if it's visible.  We can get DrvMovePointer to do this
    //  for us:

    DrvMovePointer(pso, -1, -1, NULL);

    //
    //  indicate we have to worry about SW cursor panning
    //
    if ( ppdev->ulCaps & CAPS_PANNING )
    {
        ppdev->flStatus |= STAT_SW_CURSOR_PAN;
    }

    return(SPS_DECLINE);
}

/******************************Public*Routine******************************\
* VOID vDisablePointer
*
\**************************************************************************/

//VOID vDisablePointer(
//PDEV*   ppdev)
//{
//    // Nothing to do, really
//}

/******************************Public*Routine******************************\
* VOID vAssertModePointer
*
\**************************************************************************/

VOID vAssertModePointer(
PDEV*   ppdev,
BOOL    bEnable)
{
    ULONG*  pulDst;
    LONG    i;
    LONG    lPointerShape;

    //  We will turn off any hardware pointer -- either in the S3 or in the
    //  DAC -- to begin with:

    ppdev->bHwPointerActive = FALSE;

/*    if (ppdev->ulCaps & CAPS_SW_POINTER)
    {
        //  With a software pointer, we don't have to do anything.
        //
        //  if panning is enabled, set the SW cursor panning flag
        //
        if ( ppdev->ulCaps & CAPS_PANNING )
        {
            ppdev->flStatus |= STAT_SW_CURSOR_PAN;
        }
    }
    else    // Patrick */

    //
    // leave the panning flag, Patrick
    //

        if ( ppdev->ulCaps & CAPS_PANNING )
            ppdev->flStatus |= STAT_SW_CURSOR_PAN;
    {
        // Hide the DAC pointer:
        ppdev->pfnEnablePointer(ppdev, ppdev->pvPointerData, TRUE);


        ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

        ppdev->pfnShowPointer(ppdev, ppdev->pvPointerData, TRUE);

        RELEASE_CRTC_CRITICAL_SECTION(ppdev);
    }
}


VOID vShowPointerDummy (
    PDEV*   ppdev,
    VOID*   ptp,
    BOOL    bShow)

{
    return;
}

VOID vMovePointerDummy (
    PDEV*   ppdev,
    VOID*   pbp,
    LONG    x,
    LONG    y)

{
    return;
}

BOOL bSetPointerShapeDummy (
    PDEV*       ppdev,
    VOID*       pbp,
    LONG        x,              // Relative coordinates
    LONG        y,              // Relative coordinates
    LONG        xHot,
    LONG        yHot,
    LONG        cx,
    LONG        cy,
    BYTE*       pjShape,
    FLONG       fl          )

{
    return TRUE;
}

VOID vEnablePointerDummy (
    PDEV*       ppdev,
    VOID*       ptp,
    BOOL        bEnable )

{
    return;
}

/******************************Public*Routine******************************\
* BOOL bEnablePointer
*
\**************************************************************************/

BOOL bEnablePointer(
PDEV*   ppdev)
{
    ULONG   ulReturnedData, ReturnedDataLength;
    MOBILE_INTERFACE MobileInterface;

    if (!EngDeviceIoControl( ppdev->hDriver, 
                            IOCTL_VIDEO_S3_MBL_GET_MOBILE_INTERFACE,
                            NULL,                           
                            0,                              
                            (LPVOID)&MobileInterface,       
                            sizeof(MOBILE_INTERFACE),                  
                            &ReturnedDataLength )  ||
                            ( ReturnedDataLength == sizeof(MOBILE_INTERFACE)))
    {
        if ((MobileInterface.ulMfg == M5_COMPAQ) && 
            (MobileInterface.ulSubType == 260))
        {
            ppdev->bForceSwPointer = TRUE;
        }
        else
        {
            ppdev->bForceSwPointer = FALSE;
        }
    }
    else
    {
        ppdev->bForceSwPointer = FALSE;
    }

    if ((ppdev->ulCaps & CAPS_SW_POINTER) &&
        (!(ppdev->ulCaps2 & CAPS2_LCD_SUPPORT)))
    {
        // With a software pointer, we don't have to do anything.
        ppdev->pfnShowPointer     = vShowPointerDummy;
        ppdev->pfnMovePointer     = vMovePointerDummy;
        ppdev->pfnSetPointerShape = bSetPointerShapeDummy;
        ppdev->pfnEnablePointer   = vEnablePointerDummy;
    }
    else
    {
        //  Enable the S3 hardware pointer.
        //
        // 'pvPointerShape' is the pointer to be the beginning of the
        //  pointer shape bits in off-screen memory:
        //

        ppdev->pvPointerShape = ppdev->pjScreen + ppdev->cjPointerOffset;

        //
        //  Get a copy of the current register '45' state, so that whenever
        //  we enable or disable the S3 hardware pointer, we don't have to
        //  do a read-modify-write on this register:
        //

        ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

        WRITE_REGISTER_UCHAR ((BYTE *)(ppdev->pjMmBase + MM_CRTC_INDEX),
                              HGC_MODE);

        ppdev->ulHwGraphicsCursorModeRegister_45 =
           (ULONG)
           (((READ_REGISTER_UCHAR ((BYTE *)(ppdev->pjMmBase + MM_CRTC_DATA))
           & ~HGC_ENABLE) << 8) | HGC_MODE);

        RELEASE_CRTC_CRITICAL_SECTION(ppdev);
        ppdev->pfnSetPointerShape = vNewMmIoSetPointerShapeS3;

        if(ppdev->ulCaps2 & CAPS2_HW_CURSOR_WKAROUND)
        {
            ppdev->pfnSetPointerShape = vSetPointerShapeS3withWait;
        }

        ppdev->pfnShowPointer     = vNewMmIoShowPointerS3;
        ppdev->pfnMovePointer     = vNewMmIoMovePointerS3;
        ppdev->pfnEnablePointer   = vEnablePointerS3;

        ppdev->pvPointerData = &ppdev->ajPointerData[0];
    }

    vAssertModePointer(ppdev, TRUE);

    DISPDBG((5, "Passed bEnablePointer"));

    return(TRUE);
}


