/******************************Module*Header*******************************\
*
*                        **************************
*                        * DirectDraw SAMPLE CODE *
*                        **************************
*                           * D3D SAMPLE CODE *
*                           *******************
* Module Name: ddraw.c
*
* Implements all the DirectDraw components for the driver.
*
* Copyright (c) 1995-1996 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"
#if _WIN32_WINNT >= 0x0500

#include <ntddvdeo.h>

#include "dx95type.h"
#define _NO_COM
#include "ddkernel.h"
#undef _NO_COM

#include "d3ddrv.h"

extern DWORD WINAPI DdGetDriverInfo(struct _DD_GETDRIVERINFODATA *);

// D3D Callbacks
extern DWORD WINAPI D3DClear2( LPD3DHAL_CLEAR2DATA);
extern DWORD WINAPI D3DValidateTextureStageState( LPD3DHAL_VALIDATETEXTURESTAGESTATEDATA);
extern DWORD WINAPI D3DDrawPrimitives2( LPD3DHAL_DRAWPRIMITIVES2DATA);
extern DWORD WINAPI D3DSetRenderTarget32(LPD3DHAL_SETRENDERTARGETDATA);
extern BOOL  WINAPI D3DCheckTextureFormat(LPDDPIXELFORMAT lpddpf);

ULONG a = (ULONG)DDERR_UNSUPPORTED;
ULONG b = (ULONG)DDERR_GENERIC;
ULONG c = (ULONG)DDERR_OUTOFCAPS;

extern DDHAL_DDKERNELCALLBACKS KernelCallbacks;
extern DDHAL_DDCOLORCONTROLCALLBACKS ColorControlCallbacks;
DDKERNELCAPS KernelCaps =
{
    sizeof( DDKERNELCAPS ),
    DDKERNELCAPS_SKIPFIELDS|DDKERNELCAPS_SETSTATE|
        DDKERNELCAPS_LOCK|DDKERNELCAPS_FLIPVIDEOPORT|DDKERNELCAPS_FLIPOVERLAY| \
        DDKERNELCAPS_FIELDPOLARITY,
    DDIRQ_DISPLAY_VSYNC|DDIRQ_VPORT0_VSYNC
};

#endif //_WIN32_WINNT >= 0x500

// S3VIRGE defined in VIRGE\SOURCES for building Virge Family drivers.
// This flag is here so that we can manage the video memory ourselves,
// instead of letting DirectDraw do the memory allocation. This is needed
// in order to make MCD work, else DrvEnableDirectDraw will start up allo-
// cating all the memory and there will be none left when MCD comes alive.
// For Trio driver, S3VIRGE is not defined.
//
#include "hw3d.h"

#define DDRAW_DEBUG_LEVEL 5             // -BBD
SHORT GetDCLK2( PDEV * );

/******************************Public*Routine******************************\
* Support routine to read and write Indexed Registers
*
\**************************************************************************/

/******************************Public*Routine******************************\
* VOID vStreamsDelay()
*
* This tries to work around a hardware timing bug.  Supposedly, consecutive
* writes to the streams processor in fast CPUs such as P120 and P133's
* have problems.  I haven't seen this problem, but this work-around exists
* in the Windows 95 driver, and at this point don't want to chance not
* having it.  Note that writes to the streams processor are not performance
* critical, so this is not a performance hit.
*
\**************************************************************************/

VOID vStreamsDelay()
{
    volatile LONG i;

    for (i = 32; i != 0; i--)
        ;
}
/******************************Public*Routine******************************\
* VOID ddCalcOpaqueOverlay
*
\**************************************************************************/

VOID ddCalcOpaqueOverlay (
PDEV*   ppdev,
DWORD   *pdwOpaqueCtrl,
DWORD   dwDesktopSSX,
DWORD   dwDesktopSSWidth)

{
    DWORD dwTmp;

    //
    //Disable until take care of issues :
    // 1) Screwed up on IGA2
    // 2) Source Color Key
    //

    *pdwOpaqueCtrl = 0;

    return;

    // M3/M5 and ViRGE GX2 ONLY...
    if (ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE)
    {
        //
        // calculate Stop fetch
        //
        //PxlStop = First QWORD  not to be fetched
        //        = [X1 * bytes per pixel/8] + 1  (rounded up)
        //NOTE, HAD TO PRETEND COMPUTING FOR 2*QWORD (BYTESperQWORD=16,see hw.h)!!!!!
        //Store value in dwOpaqueCtrl
        //
        *pdwOpaqueCtrl = (((dwDesktopSSX * ppdev->cjPelSize) + BYTESperQWORD/2) / BYTESperQWORD) + 1;

        //
        // calculate Start fetch
        //
        //PxlStart = Width in QWORDs + First QWORD - 1
        //         = [(X1 + Width) * bytes per pixel/8] - 1 (rounded down)
        //NOTE, HAD TO PRETEND COMPUTING FOR 2*QWORD (BYTESperQWORD=16,see hw.h)!!!!!
        //Store value in dwTmp
        //
        dwTmp = (((dwDesktopSSX + dwDesktopSSWidth) * ppdev->cjPelSize) /
                BYTESperQWORD);

        if (dwTmp != 0)
        {
            //
            //Never want dwTmp to be "negative"
            //

            --dwTmp;
        }

        if ( dwTmp <= *pdwOpaqueCtrl )
        {
            //
            // If pixel resume is before or same as pixel stop, then
            // big problems if we continue.
            //

            *pdwOpaqueCtrl = 0;
        }
        else
        {
            *pdwOpaqueCtrl = (*pdwOpaqueCtrl << OpqStart_Shift) & OpqStart_Mask;
            *pdwOpaqueCtrl |= (dwTmp << OpqEnd_Shift) & OpqEnd_Mask;
            *pdwOpaqueCtrl |= OpqCtrl_Mask;
        }
    }
    else
    {
        //
        // non-GX2, M3, M5
        //

        *pdwOpaqueCtrl = 0;
    }
}

/******************************Public*Routine******************************\
* VOID ddUpdateStreamsForPanning
*
\**************************************************************************/

VOID ddUpdateStreamsForPanning (
PDEV*   ppdev,
DWORD*  pNewX,
DWORD*  pNewY)

{
    BYTE*   pjMmBase;
    DWORD   XOffset, YOffset;
    DWORD   Width, Height;
    DWORD   StartAddr;
    DWORD   AddrCorrection;
    BOOLEAN WaitForVertBlank;
    DWORD   dwOpaqueCtrl;
    DWORD   dwLastClipped;

    pjMmBase = ppdev->pjMmBase;

    Width = ppdev->SSdstWidth;
    Height = ppdev->SSdstHeight;

    dwLastClipped = ppdev->SSClipped;

    //
    //  Make adjustments for concurrent centering or expansion, if needed
    //  (when the TV is on IGA1, it is possible to be both centering and
    //  panning).
    //
    if (ppdev->Band.DuoViewFlags & DVW_Expand_ON)
    {
        *pNewX = (*pNewX * (ppdev->XExpansion >> 16))   /
                     (ppdev->XExpansion & 0xFFFF) + ppdev->DisplayXOffset;
        *pNewY = (*pNewY * (ppdev->YExpansion >> 16))   /
                     (ppdev->YExpansion & 0xFFFF) + ppdev->DisplayYOffset;
    }
    else if (ppdev->Band.DuoViewFlags & DVW_Center_ON)
    {
        *pNewX += ppdev->DisplayXOffset;
        *pNewY += ppdev->DisplayYOffset;
    }

    ddClipStreamsToViewport ( ppdev,
                              pNewX,      pNewY,
                              &Width,     &Height,
                              &XOffset,   &YOffset);

    WaitForVertBlank = ((YOffset && (*pNewY < ppdev->SSY))    ||
                        (XOffset && (*pNewX < ppdev->SSX)));

    StartAddr = ppdev->SSBaseStartAddr +
                (YOffset * ppdev->SSStride) +
                (XOffset * ppdev->SSbytesPerPixel);

    if ( ppdev->SSbytesPerPixel != 3 )
    {
        StartAddr &= ~7;
    }
    else
    {
        AddrCorrection = XOffset & 0x7;
        AddrCorrection += AddrCorrection << 1;
        StartAddr -= AddrCorrection;
    }

    ppdev->dwOverlayFlipOffset = StartAddr;     // Save for flip
    StartAddr += (DWORD)ppdev->fpVisibleOverlay;

    //
    //  set the new position
    //

    WRITE_STREAM_D(pjMmBase, S_XY, XY(*pNewX, *pNewY));

    //
    //  if we need to clip the secondary window OR the secondary
    //  window had been previously clipped, then we need to reset
    //  the start address and window size
    //
    if (dwLastClipped || ppdev->SSClipped)
    {

        WRITE_STREAM_D(pjMmBase, S_WH, WH(Width, Height));

        //
        //  this code was added to eliminate the problem of garbage being
        //  visible on the screen when the start address is increasing
        //  and the window size is decreasing - the hardware tends to
        //  update the start address before the window size, allowing
        //  garbage from outside the secondary memory window to be
        //  displayed - waiting until the start of vertical blanking
        //  cures the problem
        //

        if (WaitForVertBlank)
        {
            do {} while ((VBLANK_IS_ACTIVE(ppdev)));
            do {} while (!(VBLANK_IS_ACTIVE(ppdev)));
        }

        WRITE_STREAM_D(pjMmBase, S_0, StartAddr);
    }

    if (ppdev->fpVisibleOverlay)
    {
        //
        // Update Opaque Overlay Control...
        //

        ddCalcOpaqueOverlay( ppdev,
                             &dwOpaqueCtrl,
                             *pNewX,
                             Width );

        WRITE_STREAM_D(pjMmBase, OPAQUE_CONTROL, dwOpaqueCtrl);
    }

    return;
}

/******************************Public*Routine******************************\
* DWORD dwGetPaletteEntry
*
\**************************************************************************/

DWORD dwGetPaletteEntry(
PDEV* ppdev,
DWORD iIndex)
{
    DWORD   dwRed;
    DWORD   dwGreen;
    DWORD   dwBlue;

    DISPDBG((4, "In dwGetPaletteEntry"));

    VGAOUTP(ppdev, 0x3c7, iIndex);

    dwRed   = VGAINP(ppdev, 0x3c9) << 2;
    dwGreen = VGAINP(ppdev, 0x3c9) << 2;
    dwBlue  = VGAINP(ppdev, 0x3c9) << 2;

    return((dwRed << 16) | (dwGreen << 8) | (dwBlue));
}

/******************************Public*Routine******************************\
* VOID vGetDisplayDuration
*
* Get the length, in EngQueryPerformanceCounter() ticks, of a refresh cycle.
*
* If we could trust the miniport to return back and accurate value for
* the refresh rate, we could use that.  Unfortunately, our miniport doesn't
* ensure that it's an accurate value.
*
\**************************************************************************/

#define NUM_VBLANKS_TO_MEASURE      1
#define NUM_MEASUREMENTS_TO_TAKE    8

VOID vGetDisplayDuration(
PDEV* ppdev)
{
    LONG        i;
    LONG        j;
    LONGLONG    li;
    LONGLONG    liFrequency;
    LONGLONG    liMin;
    LONGLONG    aliMeasurement[NUM_MEASUREMENTS_TO_TAKE + 1];

    memset(&ppdev->flipRecord, 0, sizeof(ppdev->flipRecord));

    // Warm up EngQUeryPerformanceCounter to make sure it's in the working
    // set:

    EngQueryPerformanceCounter(&li);

    // Unfortunately, since NT is a proper multitasking system, we can't
    // just disable interrupts to take an accurate reading.  We also can't
    // do anything so goofy as dynamically change our thread's priority to
    // real-time.
    //
    // So we just do a bunch of short measurements and take the minimum.
    //
    // It would be 'okay' if we got a result that's longer than the actual
    // VBlank cycle time -- nothing bad would happen except that the app
    // would run a little slower.  We don't want to get a result that's
    // shorter than the actual VBlank cycle time -- that could cause us
    // to start drawing over a frame before the Flip has occured.
    //
    // Skip a couple of vertical blanks to allow the hardware to settle
    // down after the mode change, to make our readings accurate:

    for (i = 2; i != 0; i--)
    {
        while (VBLANK_IS_ACTIVE(ppdev))
            ;
        while (!(VBLANK_IS_ACTIVE(ppdev)))
            ;
    }

    for (i = 0; i < NUM_MEASUREMENTS_TO_TAKE; i++)
    {
        // We're at the start of the VBlank active cycle!

        EngQueryPerformanceCounter(&aliMeasurement[i]);

        // Okay, so life in a multi-tasking environment isn't all that
        // simple.  What if we had taken a context switch just before
        // the above EngQueryPerformanceCounter call, and now were half
        // way through the VBlank inactive cycle?  Then we would measure
        // only half a VBlank cycle, which is obviously bad.  The worst
        // thing we can do is get a time shorter than the actual VBlank
        // cycle time.
        //
        // So we solve this by making sure we're in the VBlank active
        // time before and after we query the time.  If it's not, we'll
        // sync up to the next VBlank (it's okay to measure this period --
        // it will be guaranteed to be longer than the VBlank cycle and
        // will likely be thrown out when we select the minimum sample).
        // There's a chance that we'll take a context switch and return
        // just before the end of the active VBlank time -- meaning that
        // the actual measured time would be less than the true amount --
        // but since the VBlank is active less than 1% of the time, this
        // means that we would have a maximum of 1% error approximately
        // 1% of the times we take a context switch.  An acceptable risk.
        //
        // This next line will cause us wait if we're no longer in the
        // VBlank active cycle as we should be at this point:

        while (!(VBLANK_IS_ACTIVE(ppdev)))
            ;

        for (j = 0; j < NUM_VBLANKS_TO_MEASURE; j++)
        {
            while (VBLANK_IS_ACTIVE(ppdev))
                ;
            while (!(VBLANK_IS_ACTIVE(ppdev)))
                ;
        }
    }

    EngQueryPerformanceCounter(&aliMeasurement[NUM_MEASUREMENTS_TO_TAKE]);

    // Use the minimum:

    liMin = aliMeasurement[1] - aliMeasurement[0];

    DISPDBG((1, "Refresh count: %li - %li", 1, (ULONG) liMin));

    for (i = 2; i <= NUM_MEASUREMENTS_TO_TAKE; i++)
    {
        li = aliMeasurement[i] - aliMeasurement[i - 1];

        DISPDBG((1, "               %li - %li", i, (ULONG) li));

        if (li < liMin)
        {
            liMin = li;
            DISPDBG( ( 1, "display:vGetDisplayDuration => assigning liMin %d", li ) );
        }
    }


    //
    // Round the result:
    //

    ppdev->flipRecord.liFlipDuration =
                (DWORD) (liMin + (NUM_VBLANKS_TO_MEASURE / 2)) /
                        NUM_VBLANKS_TO_MEASURE;
    ppdev->flipRecord.bFlipFlag  = FALSE;
    ppdev->flipRecord.fpFlipFrom = 0;
    DISPDBG( ( 1,
               "display:vGetDisplayDuration => liFlipDuration <%d>",
               ppdev->flipRecord.liFlipDuration ) );

    //
    // We need the refresh rate in Hz to query the S3 miniport about the
    // streams parameters:
    //

    EngQueryPerformanceFrequency(&liFrequency);

    ppdev->ulRefreshRate =
            (ULONG) ((liFrequency + (ppdev->flipRecord.liFlipDuration / 2)) /
                    ppdev->flipRecord.liFlipDuration);

    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    VGAOUTP( ppdev, CRTC_INDEX, EXT_MODE_CONTROL_REG );

    if ( VGAINP( ppdev, CRTC_DATA ) & INTERLACED_MASK )
    {
            ppdev->ulRefreshRate /= 2;
    }

    RELEASE_CRTC_CRITICAL_SECTION(ppdev);

    DISPDBG((1, "Frequency: %li Hz", ppdev->ulRefreshRate));
}

//
//  vGetDisplayDurationIGA2 ()
//      Note that the case of getting display duration on IGA2
//      is a little trickier.  See caveats in comments below.
//

VOID vGetDisplayDurationIGA2( PDEV * ppdev )
{
    LONG        i;
    LONG        j;
    LONGLONG    li;
    LONGLONG    liFrequency;
    LONGLONG    liMin;
    LONGLONG    aliMeasurement[NUM_MEASUREMENTS_TO_TAKE + 1];
    BYTE        bDisplayState;
    BOOL        fIGA2_OK = TRUE;

    //
    //  Execute only if IGA2 is active or else driver will hang.
    //
    //  Note that we may be interrupted by an SMI event (hotkey
    //  display switch or supsend event), in which case the
    //  sequencer index could be lost, we could have switched from
    //  IGA2 to IGA1, etc.  We need to handle this possibility as
    //  best we can, by ensuring we never hang the driver looping
    //  on an IGA2 register read (such as wait for vblank), for example.
    //
    //  We need up-to-the-minute DuoView state, so don't
    //  rely on Band flags for checking DuoView state, but
    //  go to the registers for DuoView state.
    //
    ACQUIRE_CRTC_CRITICAL_SECTION( ppdev );

    VGAOUTP( ppdev, CRTC_INDEX, EXT_BIOS_FLAG3 );

    bDisplayState = VGAINP( ppdev, CRTC_DATA );

    if (!(bDisplayState & M5_DUOVIEW))
    {
        RELEASE_CRTC_CRITICAL_SECTION(ppdev);
    }
    else
    {

        memset(&ppdev->flipRecord2, 0, sizeof(ppdev->flipRecord2));

        VGAOUTP( ppdev, CRTC_DATA, bDisplayState );

        VGAOUTPW( ppdev, SEQ_INDEX, SEQ_REG_UNLOCK );

        VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA2_READS_WRITES );

        //
        //Shadow CRTC index for IB workaround.
        //

        vWriteCrIndexWithShadow( ppdev,  BACKWARD_COMPAT2_REG );

        //
        // Warm up EngQUeryPerformanceCounter to make sure it's in the working
        // set:
        //

        EngQueryPerformanceCounter(&li);

        // Unfortunately, since NT is a proper multitasking system, we can't
        // just disable interrupts to take an accurate reading.  We also can't
        // do anything so goofy as dynamically change our thread's priority to
        // real-time.
        //
        // So we just do a bunch of short measurements and take the minimum.
        //
        // It would be 'okay' if we got a result that's longer than the actual
        // VBlank cycle time -- nothing bad would happen except that the app
        // would run a little slower.  We don't want to get a result that's
        // shorter than the actual VBlank cycle time -- that could cause us
        // to start drawing over a frame before the Flip has occured.
        //
        // Skip a couple of vertical blanks to allow the hardware to settle
        // down after the mode change, to make our readings accurate:

        for (i = 2; i != 0; i--)
        {
            if ( !fWaitForVsyncInactiveIGA2( ppdev, fIGA2_OK ) ||
                 !fWaitForVsyncActiveIGA2  ( ppdev, fIGA2_OK ) )
            {
                fIGA2_OK = FALSE;
                break;
            }
        }

        if (fIGA2_OK)
        {
            for (i = 0; i < NUM_MEASUREMENTS_TO_TAKE; i++)
            {
                // We're at the start of the VBlank active cycle!

                EngQueryPerformanceCounter(&aliMeasurement[i]);

                // Okay, so life in a multi-tasking environment isn't all that
                // simple.  What if we had taken a context switch just before
                // the above EngQueryPerformanceCounter call, and now were half
                // way through the VBlank inactive cycle?  Then we would measure
                // only half a VBlank cycle, which is obviously bad.  The worst
                // thing we can do is get a time shorter than the actual VBlank
                // cycle time.
                //
                // So we solve this by making sure we're in the VBlank active
                // time before and after we query the time.  If it's not, we'll
                // sync up to the next VBlank (it's okay to measure this period --
                // it will be guaranteed to be longer than the VBlank cycle and
                // will likely be thrown out when we select the minimum sample).
                // There's a chance that we'll take a context switch and return
                // just before the end of the active VBlank time -- meaning that
                // the actual measured time would be less than the true amount --
                // but since the VBlank is active less than 1% of the time, this
                // means that we would have a maximum of 1% error approximately
                // 1% of the times we take a context switch.  An acceptable risk.
                //
                // This next line will cause us wait if we're no longer in the
                // VBlank active cycle as we should be at this point:

                if ( !fWaitForVsyncActiveIGA2( ppdev, fIGA2_OK ) )
                {
                    fIGA2_OK = FALSE;
                    break;
                }

                for (j = 0; j < NUM_VBLANKS_TO_MEASURE; j++)
                {
                    if ( !fWaitForVsyncInactiveIGA2( ppdev, fIGA2_OK ) ||
                         !fWaitForVsyncActiveIGA2  ( ppdev, fIGA2_OK ) )
                    {
                        fIGA2_OK = FALSE;
                        break;
                    }
                }
            }
        }

        if (fIGA2_OK)
        {
            EngQueryPerformanceCounter(&aliMeasurement[NUM_MEASUREMENTS_TO_TAKE]);

            //
            // Use the minimum:
            //

            liMin = aliMeasurement[1] - aliMeasurement[0];

            DISPDBG((1, "Refresh count: %li - %li", 1, (ULONG) liMin));

            for (i = 2; i <= NUM_MEASUREMENTS_TO_TAKE; i++)
            {
                li = aliMeasurement[i] - aliMeasurement[i - 1];

                DISPDBG((1, "               %li - %li", i, (ULONG) li));

                if (li < liMin)
                {
                    liMin = li;
                    DISPDBG( ( 1, "display:vGetDisplayDuration => assigning liMin %d", li ) );
                }
            }

            //
            // Round the result:
            //

            ppdev->flipRecord2.liFlipDuration =
                    (DWORD) (liMin + (NUM_VBLANKS_TO_MEASURE / 2)) /
                            NUM_VBLANKS_TO_MEASURE;
            ppdev->flipRecord2.bFlipFlag  = FALSE;
            ppdev->flipRecord2.fpFlipFrom = 0;
            DISPDBG( ( 1,
                       "display:vGetDisplayDuration => liFlipDuration <%d>",
                       ppdev->flipRecord2.liFlipDuration ) );

            //
            // We need the refresh rate in Hz to query the S3 miniport about the
            // streams parameters:
            //

            EngQueryPerformanceFrequency(&liFrequency);

            ppdev->ulRefreshRate2 =
                (ULONG) ((liFrequency + (ppdev->flipRecord2.liFlipDuration / 2)) /
                        ppdev->flipRecord2.liFlipDuration);

            if ( bReadCrDataWithShadow( ppdev, EXT_MODE_CONTROL_REG ) &
                 INTERLACED_MASK )
            {
                ppdev->ulRefreshRate2 /= 2;
            }
        }
        else
        {
            ppdev->ulRefreshRate2 = 0;
        }

        VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA1 );


        RELEASE_CRTC_CRITICAL_SECTION(ppdev);

        DISPDBG((1, "Frequency: %li Hz", ppdev->ulRefreshRate2));
    }

    return;
}

/******************************Public*Routine******************************\
* HRESULT ddrvalUpdateFlipStatus
*
* Checks and sees if the most recent flip has occurred.
*
* Unfortunately, the hardware has no ability to tell us whether a vertical
* retrace has occured since the flip command was given other than by
* sampling the vertical-blank-active and display-active status bits.
*
\**************************************************************************/

HRESULT ddrvalUpdateFlipStatus(
PDEV*   ppdev,
FLATPTR fpVidMem)
{
    LONGLONG    liTime;

    if ((ppdev->flipRecord.bFlipFlag) &&
        ((fpVidMem == 0xFFFFFFFF) || (fpVidMem == ppdev->flipRecord.fpFlipFrom)))
    {
        if (VBLANK_IS_ACTIVE(ppdev))
        {
            if (ppdev->flipRecord.bWasEverInDisplay)
            {
                ppdev->flipRecord.bHaveEverCrossedVBlank = TRUE;
            }
        }
        else if (DISPLAY_IS_ACTIVE(ppdev))
        {
            if (ppdev->flipRecord.bHaveEverCrossedVBlank)
            {
                ppdev->flipRecord.bFlipFlag = FALSE;
                return(DD_OK);
            }
            ppdev->flipRecord.bWasEverInDisplay = TRUE;
        }

        // It's pretty unlikely that we'll happen to sample the vertical-
        // blank-active at the first vertical blank after the flip command
        // has been given.  So to provide better results, we also check the
        // time elapsed since the flip.  If it's more than the duration of
        // one entire refresh of the display, then we know for sure it has
        // happened:

        EngQueryPerformanceCounter(&liTime);

        if (liTime - ppdev->flipRecord.liFlipTime
                                <= ppdev->flipRecord.liFlipDuration)
        {
            return(DDERR_WASSTILLDRAWING);
        }

        ppdev->flipRecord.bFlipFlag = FALSE;
    }

    return(DD_OK);
}

/******************************Public*Routine******************************\
* DWORD DdFlip
*
* Note that lpSurfCurr may not necessarily be valid.
*
\**************************************************************************/

DWORD DdFlip(
PDD_FLIPDATA lpFlip)
{
    PDEV*       ppdev;
    BYTE*       pjMmBase;
    HRESULT     ddrval;
    ULONG       ulMemoryOffset;
    ULONG       ulIgaMemoryOffset;
    ULONG       ulLowOffset1;
    ULONG       ulMiddleOffset1;
    ULONG       ulHighOffset1;
    ULONG       ulLowOffset2;
    ULONG       ulMiddleOffset2;
    ULONG       ulHighOffset2;
    ULONG       ulLoopCount;
    BYTE        bDisplayState;
    BOOL        fDuoView = FALSE;
    DISPDBG((4, "In DdFlip"));

    ppdev    = (PDEV*) lpFlip->lpDD->dhpdev;
    pjMmBase = ppdev->pjMmBase;
    //
    // Get a more up-to-date picture of DuoView status.
    //


    ACQUIRE_CRTC_CRITICAL_SECTION( ppdev );

    VGAOUTP( ppdev, CRTC_INDEX, EXT_BIOS_FLAG3 );

    bDisplayState = VGAINP( ppdev, CRTC_DATA );

    if ((bDisplayState & M5_DUOVIEW) &&
        (ppdev->Band.DuoViewFlags & DVW_DualViewSameImage))
    {
        fDuoView = TRUE;
    }

    RELEASE_CRTC_CRITICAL_SECTION(ppdev);

    //
    // Is the current flip still in progress?
    //
    // Don't want a flip to work until after the last flip is done,
    // so we ask for the general flip status and ignore the vmem.
    //
    // Make sure that the border/blanking period isn't active; wait if
    // it is.  We could return DDERR_WASSTILLDRAWING in this case, but
    // that will increase the odds that we can't flip the next time:
    //

    ddrval = ddrvalUpdateFlipStatus(ppdev, 0xFFFFFFFF);

    if ( ddrval != DD_OK                              ||
        S3DGPBusy( ppdev )                            ||
        ( !fDuoView && !DISPLAY_IS_ACTIVE( ppdev ) ) )
    {
        lpFlip->ddRVal = DDERR_WASSTILLDRAWING;
        return(DDHAL_DRIVER_HANDLED);
    }

    ulMemoryOffset = (ULONG)lpFlip->lpSurfTarg->lpGbl->fpVidMem;

    if (ppdev->flStatus & STAT_STREAMS_ENABLED)
    {
        //
        // When using the streams processor, we have to do the flip via the
        // streams registers:
        //

        if (lpFlip->lpSurfCurr->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            WRITE_STREAM_D(pjMmBase, P_0, ulMemoryOffset);
        }
        else if (lpFlip->lpSurfCurr->ddsCaps.dwCaps & DDSCAPS_OVERLAY)
        {
            // Make sure that the overlay surface we're flipping from is
            // currently visible.  If you don't do this check, you'll get
            // really weird results when someone starts up two ActiveMovie
            // or DirectVideo movies simultaneously!

            if (lpFlip->lpSurfCurr->lpGbl->fpVidMem == ppdev->fpVisibleOverlay)
            {
                ppdev->fpVisibleOverlay = ulMemoryOffset;

                WRITE_STREAM_D(pjMmBase, S_0, ulMemoryOffset +
                               ppdev->dwOverlayFlipOffset);
            }
            else 
            {
                lpFlip->ddRVal = DDERR_OUTOFCAPS;
                return(DDHAL_DRIVER_HANDLED);
            }
        }
    }
    else
    {
        // Do the old way, via the CRTC registers:

        if (ppdev->ulCaps & CAPS_PANNING)
        {
            ppdev->fpVisibleBuffer = ulMemoryOffset;

            //Setup for Iga1 update...

            ulIgaMemoryOffset = (ulMemoryOffset + ppdev->ApertureIga1) >> 2;

            ulLowOffset1    = DISPLAY_START_LO |
                              ((ulIgaMemoryOffset & 0x0000ff) << 8);

            ulMiddleOffset1 = DISPLAY_START_HI |
                              ((ulIgaMemoryOffset & 0x00ff00));

            ulHighOffset1   = EXT_SYS_CTRL3                         |
                              ((ulIgaMemoryOffset & 0x0f0000) >> 8) |
                              ppdev->ulExtendedSystemControl3Register_69;

            //Setup for Iga2 update...

            ulIgaMemoryOffset = (ulMemoryOffset + ppdev->ApertureIga2) >> 2;

            ulLowOffset2    = DISPLAY_START_LO |
                              ((ulIgaMemoryOffset & 0x0000ff) << 8);

            ulMiddleOffset2 = DISPLAY_START_HI |
                              ((ulIgaMemoryOffset & 0x00ff00));

            ulHighOffset2   = EXT_SYS_CTRL3                         |
                              ((ulIgaMemoryOffset & 0x0f0000) >> 8) |
                              ppdev->ulExtendedSystemControl3Register_69;
        }
        else
        {
            //
            //Iga1 and Iga2 will be the same...
            //

            ulMemoryOffset >>= 2;

            ulLowOffset1    =
            ulLowOffset2    = DISPLAY_START_LO | ((ulMemoryOffset & 0x0000ff) << 8);

            ulMiddleOffset1 =
            ulMiddleOffset2 = DISPLAY_START_HI | ((ulMemoryOffset & 0x00ff00));

            ulHighOffset1 =
            ulHighOffset2   = EXT_SYS_CTRL3 | ((ulMemoryOffset & 0x0f0000) >> 8)
                              | ppdev->ulExtendedSystemControl3Register_69;
        }

        // Don't let the cursor thread touch the CRT registers while we're
        // using them:

        ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

        // Too bad that the S3's flip can't be done in a single atomic register
        // write; as it is, we stand a small chance of being context-switched
        // out and exactly hitting the vertical blank in the middle of doing
        // these outs, possibly causing the screen to momentarily jump.
        //
        // There are some hoops we could jump through to minimize the chances
        // of this happening; we could try to align the flip buffer such that
        // the minor registers are ensured to be identical for either flip
        // position, and so that only the high address need be written, an
        // obviously atomic operation.
        //
        // However, I'm simply not going to worry about it.

        //
        // takes effect next vsync
        //

        VGAOUTPW( ppdev, CRTC_INDEX, ulLowOffset1 );
        VGAOUTPW( ppdev, CRTC_INDEX, ulMiddleOffset1 );
        VGAOUTPW( ppdev, CRTC_INDEX, ulHighOffset1 );

        //
        // if DuoView wait for active display, program, then wait for
        // inactive display
        //

        if ( fDuoView && ppdev->ulRefreshRate2 > 0)
        {

            VGAOUTPW( ppdev, SEQ_INDEX, SEQ_REG_UNLOCK );
            VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA2_READS_WRITES );

            //
            //Shadow CRTC index for IB workaround
            //

            vWriteCrIndexWithShadow( ppdev, BACKWARD_COMPAT2_REG );

            if ( fWaitForVsyncActiveIGA2 ( ppdev, TRUE ) &&
                 fWaitForVsyncInactiveIGA2( ppdev, TRUE ) )
            {
                //
                // No shadow required since WORD writes...
                //

                VGAOUTPW(ppdev, CRTC_INDEX, ulLowOffset2);
                VGAOUTPW(ppdev, CRTC_INDEX, ulMiddleOffset2);
                VGAOUTPW(ppdev, CRTC_INDEX, ulHighOffset2);

                //
                //Shadow CRTC index for IB workaround
                //

                vWriteCrIndexWithShadow( ppdev, BACKWARD_COMPAT2_REG );
                fWaitForVsyncActiveIGA2 ( ppdev, TRUE );
            }

            VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA1 );
        }

        RELEASE_CRTC_CRITICAL_SECTION(ppdev);
    }

    // Remember where and when we were when we did the flip:

    EngQueryPerformanceCounter(&ppdev->flipRecord.liFlipTime);

    ppdev->flipRecord.bFlipFlag                  = TRUE;
    ppdev->flipRecord.bHaveEverCrossedVBlank = FALSE;
    ppdev->flipRecord.bWasEverInDisplay      = FALSE;

    ppdev->flipRecord.fpFlipFrom = lpFlip->lpSurfCurr->lpGbl->fpVidMem;

    lpFlip->ddRVal = DD_OK;
    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdLock
*
\**************************************************************************/

DWORD DdLock(
PDD_LOCKDATA lpLock)
{
    PDEV*   ppdev;
    BYTE*   pjMmBase;
    HRESULT ddrval;

    DISPDBG((4, "In DdLock"));

    ppdev = (PDEV*) lpLock->lpDD->dhpdev;
    pjMmBase = ppdev->pjMmBase;



    // Check to see if any pending physical flip has occurred.  Don't allow
    // a lock if a blt is in progress:

    ddrval = ddrvalUpdateFlipStatus(ppdev, lpLock->lpDDSurface->lpGbl->fpVidMem);
    if (ddrval != DD_OK)
    {
            lpLock->ddRVal = DDERR_WASSTILLDRAWING;
            return(DDHAL_DRIVER_HANDLED);
    }

    // Here's one of the places where the Windows 95 and Windows NT DirectDraw
    // implementations differ: on Windows NT, you should watch for
    // DDLOCK_WAIT and loop in the driver while the accelerator is busy.
    // On Windows 95, it doesn't really matter.
    //
    // (The reason is that Windows NT allows applications to draw directly
    // to the frame buffer even while the accelerator is running, and does
    // not synchronize everything on the Win16Lock.  Note that on Windows NT,
    // it is even possible for multiple threads to be holding different
    // DirectDraw surface locks at the same time.)

    if (lpLock->dwFlags & DDLOCK_WAIT)
    {
            S3DGPWait( ppdev );
    }
    else if ( S3DGPBusy( ppdev ) )
    {
            lpLock->ddRVal = DDERR_WASSTILLDRAWING;
            return(DDHAL_DRIVER_HANDLED);
    }


    return(DDHAL_DRIVER_NOTHANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdGetBltStatus
*
* Doesn't currently really care what surface is specified, just checks
* and goes.
*
\**************************************************************************/

DWORD DdGetBltStatus(
PDD_GETBLTSTATUSDATA lpGetBltStatus)
{
    PDEV*   ppdev;
    BYTE*   pjMmBase;
    HRESULT ddRVal;

    DISPDBG((4, "In DdGetBltStatus"));

    ppdev    = (PDEV*) lpGetBltStatus->lpDD->dhpdev;
    pjMmBase = ppdev->pjMmBase;

    ddRVal = DD_OK;
    if (lpGetBltStatus->dwFlags == DDGBS_CANBLT)
    {
            // DDGBS_CANBLT case: can we add a blt?

            ddRVal = ddrvalUpdateFlipStatus( ppdev,
                                        lpGetBltStatus->lpDDSurface->lpGbl->fpVidMem);

        if (ddRVal == DD_OK)
        {
            // There was no flip going on, so is there room in the FIFO
            // to add a blt?

            if ( S3DFifoBusy(ppdev, DDBLT_FIFO_COUNT) )
            {
                ddRVal = DDERR_WASSTILLDRAWING;
            }
        }
    }
    else
    {
        // DDGBS_ISBLTDONE case: is a blt in progress?

        if ( S3DGPBusy( ppdev ) )
        {
            ddRVal = DDERR_WASSTILLDRAWING;
        }
    }

    lpGetBltStatus->ddRVal = ddRVal;
    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdMapMemory
*
* This is a new DDI call specific to Windows NT that is used to map
* or unmap all the application modifiable portions of the frame buffer
* into the specified process's address space.
*
\**************************************************************************/

DWORD DdMapMemory(
PDD_MAPMEMORYDATA lpMapMemory)
{
    PDEV*                           ppdev;
    VIDEO_SHARE_MEMORY              ShareMemory;
    VIDEO_SHARE_MEMORY_INFORMATION  ShareMemoryInformation;
    DWORD                           ReturnedDataLength;

    DISPDBG((4, "In DdMapMemory"));

    ppdev = (PDEV*) lpMapMemory->lpDD->dhpdev;

    if (lpMapMemory->bMap)
    {
        ShareMemory.ProcessHandle = lpMapMemory->hProcess;

        // 'RequestedVirtualAddress' isn't actually used for the SHARE IOCTL:

        ShareMemory.RequestedVirtualAddress = 0;

        // We map in starting at the top of the frame buffer:

        ShareMemory.ViewOffset = 0;

        // We map down to the end of the frame buffer.
        //
        // Note: There is a 64k granularity on the mapping (meaning that
        //       we have to round up to 64k).
        //
        // Note: If there is any portion of the frame buffer that must
        //       not be modified by an application, that portion of memory
        //       MUST NOT be mapped in by this call.  This would include
        //       any data that, if modified by a malicious application,
        //       would cause the driver to crash.  This could include, for
        //       example, any DSP code that is kept in off-screen memory.

        ShareMemory.ViewSize = ROUND_UP_TO_64K(ppdev->cyMemory * ppdev->lDelta);

        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_SHARE_VIDEO_MEMORY,
                               &ShareMemory,
                               sizeof(VIDEO_SHARE_MEMORY),
                               &ShareMemoryInformation,
                               sizeof(VIDEO_SHARE_MEMORY_INFORMATION),
                               &ReturnedDataLength))
        {
            DISPDBG((DDRAW_DEBUG_LEVEL, "Failed IOCTL_VIDEO_SHARE_MEMORY"));

            lpMapMemory->ddRVal = DDERR_GENERIC;
            return(DDHAL_DRIVER_HANDLED);
        }

        lpMapMemory->fpProcess = (FLATPTR)ShareMemoryInformation.VirtualAddress;
    }
    else
    {
        ShareMemory.ProcessHandle               = lpMapMemory->hProcess;
        ShareMemory.ViewOffset                  = 0;
        ShareMemory.ViewSize                    = 0;
        ShareMemory.RequestedVirtualAddress = (VOID*) lpMapMemory->fpProcess;

        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_UNSHARE_VIDEO_MEMORY,
                               &ShareMemory,
                               sizeof(VIDEO_SHARE_MEMORY),
                               NULL,
                               0,
                               &ReturnedDataLength))
        {
            RIP("Failed IOCTL_VIDEO_UNSHARE_MEMORY");
        }
    }

    lpMapMemory->ddRVal = DD_OK;
    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdGetFlipStatus
*
* If the display has gone through one refresh cycle since the flip
* occurred, we return DD_OK.  If it has not gone through one refresh
* cycle we return DDERR_WASSTILLDRAWING to indicate that this surface
* is still busy "drawing" the flipped page.   We also return
* DDERR_WASSTILLDRAWING if the bltter is busy and the caller wanted
* to know if they could flip yet.
*
\**************************************************************************/

DWORD DdGetFlipStatus(
PDD_GETFLIPSTATUSDATA lpGetFlipStatus)
{
    PDEV*   ppdev;
    BYTE*   pjMmBase;

    ppdev    = (PDEV*) lpGetFlipStatus->lpDD->dhpdev;
    pjMmBase = ppdev->pjMmBase;

    // We don't want a flip to work until after the last flip is done,
    // so we ask for the general flip status and ignore the vmem:

    lpGetFlipStatus->ddRVal = ddrvalUpdateFlipStatus(ppdev, 0xFFFFFFFF);

    // Check if the bltter is busy if someone wants to know if they can
    // flip:

    if (lpGetFlipStatus->dwFlags == DDGFS_CANFLIP)
    {
        if ((lpGetFlipStatus->ddRVal == DD_OK) && ( S3DGPBusy( ppdev ) ))
        {
            lpGetFlipStatus->ddRVal = DDERR_WASSTILLDRAWING;
        }
    }

    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdWaitForVerticalBlank
*
\**************************************************************************/

DWORD DdWaitForVerticalBlank(
PDD_WAITFORVERTICALBLANKDATA lpWaitForVerticalBlank)
{
    PDEV*   ppdev;

    ppdev    = (PDEV*) lpWaitForVerticalBlank->lpDD->dhpdev;

    switch (lpWaitForVerticalBlank->dwFlags)
    {
    case DDWAITVB_I_TESTVB:

        // If TESTVB, it's just a request for the current vertical blank
        // status:

        if (VBLANK_IS_ACTIVE(ppdev))
            lpWaitForVerticalBlank->bIsInVB = TRUE;
        else
            lpWaitForVerticalBlank->bIsInVB = FALSE;

        lpWaitForVerticalBlank->ddRVal = DD_OK;
        return(DDHAL_DRIVER_HANDLED);

    case DDWAITVB_BLOCKBEGIN:

        // If BLOCKBEGIN is requested, we wait until the vertical blank
        // is over, and then wait for the display period to end:

        while (VBLANK_IS_ACTIVE(ppdev))
            ;
        while (!(VBLANK_IS_ACTIVE(ppdev)))
            ;

        lpWaitForVerticalBlank->ddRVal = DD_OK;
        return(DDHAL_DRIVER_HANDLED);

    case DDWAITVB_BLOCKEND:

        // If BLOCKEND is requested, we wait for the vblank interval to end:

        while (!(VBLANK_IS_ACTIVE(ppdev)))
            ;
        while (VBLANK_IS_ACTIVE(ppdev))
            ;

        lpWaitForVerticalBlank->ddRVal = DD_OK;
        return(DDHAL_DRIVER_HANDLED);
    }

    return(DDHAL_DRIVER_NOTHANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdCanCreateSurface
*
\**************************************************************************/

DWORD DdCanCreateSurface(
PDD_CANCREATESURFACEDATA lpCanCreateSurface)
{
    PDEV*           ppdev;
    DWORD           dwRet;
    LPDDSURFACEDESC lpSurfaceDesc;

    DISPDBG((4, "In DdCanCreateSurface"));

    ppdev = (PDEV*) lpCanCreateSurface->lpDD->dhpdev;
    lpSurfaceDesc = lpCanCreateSurface->lpDDSurfaceDesc;

    lpCanCreateSurface->ddRVal = DD_OK;
    dwRet = DDHAL_DRIVER_NOTHANDLED;

#if _WIN32_WINNT >= 0x0500
    if (lpSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_TEXTURE)
    {
        // We need to determine if the texture format is supported.
        // There are 2 possibilities: the user specified the format, or 
        // it's the same as the primary. 

        LPDDPIXELFORMAT pf;

        if (lpCanCreateSurface->bIsDifferentPixelFormat)
        {
            pf = &lpSurfaceDesc->ddpfPixelFormat;
        }
        else
        {
            pf = &ppdev->ddHALInfo.vmiData.ddpfDisplay;
        }

        if (!D3DCheckTextureFormat(pf))
        { 
            lpCanCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;  
        }

        return(DDHAL_DRIVER_HANDLED);
    }
    
    // We only support 16 bit z-buffers
    if (lpSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_ZBUFFER)
    {
        DWORD dwBitDepth;
        
        // verify where the right z buffer bit depth is
        if (DDSD_ZBUFFERBITDEPTH & lpSurfaceDesc->dwFlags)
            dwBitDepth = lpSurfaceDesc->dwZBufferBitDepth;
        else
            dwBitDepth = lpSurfaceDesc->ddpfPixelFormat.dwZBufferBitDepth;

       if (dwBitDepth  != 16)
       {
           lpCanCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;  
       }
       return(DDHAL_DRIVER_HANDLED);
    }
#endif

    // When using the Streams processor, we handle only overlays of
    // different pixel formats -- not any off-screen memory:

    if ( lpSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_OVERLAY) 
    {
        // We handle two types of YUV overlay surfaces:

        if (lpSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_FOURCC)
        {
            // Check first for a supported YUV type:

            if (lpSurfaceDesc->ddpfPixelFormat.dwFourCC == FOURCC_YUY2)
            {
                lpSurfaceDesc->ddpfPixelFormat.dwYUVBitCount = 16;
                dwRet = DDHAL_DRIVER_HANDLED;
            }
            else
            {
                dwRet = DDHAL_DRIVER_HANDLED;
                lpCanCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
            }
        }
        else if ( lpSurfaceDesc->ddpfPixelFormat.dwFlags &
                  DDPF_PALETTEINDEXED8 )
        {
            dwRet = DDHAL_DRIVER_HANDLED;
            lpCanCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
        }
        else if ( lpSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_RGB )
        {
            switch( lpSurfaceDesc->ddpfPixelFormat.dwRGBBitCount )
            {
            case 16:
                if ( IS_RGB15( &lpSurfaceDesc->ddpfPixelFormat ) )
                {
                    dwRet = DDHAL_DRIVER_HANDLED;
                }
                else if ( IS_RGB16( &lpSurfaceDesc->ddpfPixelFormat ) )
                {
                    dwRet = DDHAL_DRIVER_HANDLED;
                }
                break;
            case 24:
                if ( ppdev->Band.SWFlags & SW_NOTSPT24BOVLY )
                {
                    dwRet = DDHAL_DRIVER_HANDLED;
                    lpCanCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
                }
                else
                {
                    dwRet = DDHAL_DRIVER_HANDLED;
                }
                break;

            case 32:
                if ( ppdev->Band.SWFlags & SW_NOTSPT32BOVLY )
                {
                    dwRet = DDHAL_DRIVER_HANDLED;
                    lpCanCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
                }
                else
                {
                    dwRet = DDHAL_DRIVER_HANDLED;
                }
                break;

            default:
                dwRet = DDHAL_DRIVER_HANDLED;
                lpCanCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
                break;
            }
        }
        else if ( lpSurfaceDesc->ddpfPixelFormat.dwFlags == 0 &&
                  ppdev->cBitsPerPel == 8 )
        {
            //
            // When selecting surface type == primary surface in overlay
            // the flags are 0, so check for 0 and if 8bpp overlay.
            // We really should fail everything that gets here, but since
            // we have to check for a specific case, the DDRAW layer in
            // NT 4.0 must not be setting things correctly.
            //

            dwRet = DDHAL_DRIVER_HANDLED;
            lpCanCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
        }
    }

    // Print some spew if this was a surface we refused to create:

    if (dwRet == DDHAL_DRIVER_NOTHANDLED)
    {
        if (lpSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_RGB)
        {
            DISPDBG((DDRAW_DEBUG_LEVEL, "Failed creation of %libpp RGB surface %lx %lx %lx",
                lpSurfaceDesc->ddpfPixelFormat.dwRGBBitCount,
                lpSurfaceDesc->ddpfPixelFormat.dwRBitMask,
                lpSurfaceDesc->ddpfPixelFormat.dwGBitMask,
                lpSurfaceDesc->ddpfPixelFormat.dwBBitMask));
        }
        else
        {
            DISPDBG((DDRAW_DEBUG_LEVEL, "Failed creation of type 0x%lx YUV 0x%lx surface",
                lpSurfaceDesc->ddpfPixelFormat.dwFlags,
                lpSurfaceDesc->ddpfPixelFormat.dwFourCC));
        }
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* DWORD DdCreateSurface
*
\**************************************************************************/

DWORD DdCreateSurface(
PDD_CREATESURFACEDATA lpCreateSurface)
{
    PDEV*               ppdev;
    DD_SURFACE_LOCAL*   lpSurfaceLocal;
    DD_SURFACE_GLOBAL*  lpSurfaceGlobal;
    LPDDSURFACEDESC     lpSurfaceDesc;
    LONG                lLinearPitch;
    LONG                fSurfaceWidth;
    DWORD               dwHeight;
    DWORD               dwWidth;
    DWORD               dwByteCount     = 1;
    DWORD               dwRet           = DDHAL_DRIVER_NOTHANDLED;
    PDDRAWDATA          pDDData;
    BOOL                fOverlay;
    BOOL                fGX2_Style_SP;
    BOOL                fStreamsIsOff;
#if SUPPORT_MCD
    OH                 *poh;
#endif


    DISPDBG((4, "In DdCreateSurface"));

    ppdev = (PDEV*) lpCreateSurface->lpDD->dhpdev;

    ppdev->ulCanAdjustColor &= ~2;

    // On Windows NT, dwSCnt will always be 1, so there will only ever
    // be one entry in the 'lplpSList' array:

    lpSurfaceLocal  = lpCreateSurface->lplpSList[0];
    lpSurfaceGlobal = lpSurfaceLocal->lpGbl;
    lpSurfaceDesc   = lpCreateSurface->lpDDSurfaceDesc;

    lpCreateSurface->ddRVal = DD_OK;

    fOverlay        = lpSurfaceLocal->ddsCaps.dwCaps & DDSCAPS_OVERLAY;
    fGX2_Style_SP   = ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE;
    fStreamsIsOff   = !(ppdev->flStatus & STAT_STREAMS_ENABLED) &&
                      ppdev->ulCaps & CAPS_STREAMS_CAPABLE;

    //
    // NOTE: dwWidth and dwHeight may change for allocation of memory
    //

    dwWidth  = lpSurfaceGlobal->wWidth;
    dwHeight = lpSurfaceGlobal->wHeight;

    //
    // Allocate memory we pass around in dwReserved1
    //

    pDDData = (PDDRAWDATA) EngAllocMem( FL_ZERO_MEMORY,
                                        sizeof( DDRAWDATA ),
                                        ALLOC_TAG );
    if ( !pDDData )
    {
        lpCreateSurface->ddRVal = DDERR_OUTOFMEMORY;
//        dwRet = DDHAL_DRIVER_HANDLED;
		return (DDHAL_DRIVER_HANDLED);
    }


    // We repeat the same checks we did in 'DdCanCreateSurface' because
    // it's possible that an application doesn't call 'DdCanCreateSurface'
    // before calling 'DdCreateSurface'.

    ASSERTDD(lpSurfaceGlobal->ddpfSurface.dwSize == sizeof(DDPIXELFORMAT),
        "NT is supposed to guarantee that ddpfSurface.dwSize is valid");

    // DdCanCreateSurface already validated whether the hardware supports
    // the surface, so we don't need to do any validation here.  We'll
    // just go ahead and allocate it.
    //
    // Note that we don't do anything special for RGB surfaces that are
    // the same pixel format as the display -- by returning DDHAL_DRIVER_
    // NOTHANDLED, DirectDraw will automatically handle the allocation
    // for us.
    //
    // Also, since we'll be making linear surfaces, make sure the width
    // isn't unreasonably large.
    //
    // Note that on NT, an overlay can be created only if the driver
    // okay's it here in this routine.  Under Win95, the overlay will be
    // created automatically if it's the same pixel format as the primary
    // display.

#if !SUPPORT_MCD

    if ( fOverlay                                            ||
        (lpSurfaceGlobal->ddpfSurface.dwFlags & DDPF_FOURCC) ||
        (lpSurfaceGlobal->ddpfSurface.dwYUVBitCount
            != (DWORD) 8 * ppdev->cjPelSize)                 ||
        (lpSurfaceGlobal->ddpfSurface.dwRBitMask != ppdev->flRed))
    {

#endif //!SUPPORT_MCD

      if (1)
// DVD need to allocate a 720x240 surface, 640x480 will fail this check
// But Virge memory IS linear, we shouldn't be checking in the first place
//      if (dwWidth <= (DWORD) ppdev->cxMemory)
      {
        // The S3 cannot easily draw to YUV surfaces or surfaces that are
        // a different RGB format than the display.  So we'll make them
        // linear surfaces to save some space:

        if (lpSurfaceGlobal->ddpfSurface.dwFlags & DDPF_FOURCC)
        {
            ASSERTDD(((lpSurfaceGlobal->ddpfSurface.dwFourCC == FOURCC_YUY2)
            ),
                    "Expected our DdCanCreateSurface to allow only YUY2");

            if ( fOverlay )
            {
                //
                // YUV data can adjust color
                //

                ppdev->ulCanAdjustColor = 2;
            }


            dwByteCount = ( (lpSurfaceGlobal->ddpfSurface.dwFourCC == FOURCC_YUY2)
               ) 
                ? 2 : 1;

            // We have to fill in the bit-count for FourCC surfaces:

            lpSurfaceGlobal->ddpfSurface.dwYUVBitCount = 8 * dwByteCount;

            DISPDBG((DDRAW_DEBUG_LEVEL, "Created YUV: %li x %li",
                     dwWidth, dwHeight));
        }
        else
        {
            dwByteCount = lpSurfaceGlobal->ddpfSurface.dwRGBBitCount >> 3;

            DISPDBG((DDRAW_DEBUG_LEVEL, "Created RGB %libpp: %li x %li Red: %lx",
                8 * dwByteCount, dwWidth, dwHeight,
                lpSurfaceGlobal->ddpfSurface.dwRBitMask));

            // The S3 can't handle palettized or 32bpp overlays.  Note that
            // we sometimes don't get a chance to say no to these surfaces
            // in CanCreateSurface, because DirectDraw won't call
            // CanCreateSurface if the surface to be created is the same
            // pixel format as the primary display:

            if ( fOverlay )
            {
                switch( dwByteCount )
                {
                case 1:
                    dwRet = DDHAL_DRIVER_HANDLED;
                    lpCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
                    break;

                case 3:
                    if ( ppdev->Band.SWFlags & SW_NOTSPT24BOVLY )
                    {
                        dwRet = DDHAL_DRIVER_HANDLED;
                        lpCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
                    }
                    break;

                case 4:
                    if ( ppdev->Band.SWFlags & SW_NOTSPT32BOVLY )
                    {
                        dwRet = DDHAL_DRIVER_HANDLED;
                        lpCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
                    }
                    break;

                default:
                    break;
                }
            }

            if ( lpCreateSurface->ddRVal != DD_OK )
            {
                return( dwRet );
            }
        }

#if !SUPPORT_MCD

        //
        // We want to allocate a linear surface to store the FourCC
        // surface, but DirectDraw is using a 2-D heap-manager because
        // the rest of our surfaces have to be 2-D.  So here we have to
        // convert the linear size to a 2-D size.
        //
        // The stride has to be a dword multiple:
        //

        lLinearPitch = (dwWidth * dwByteCount + HEAP_X_ALIGNMENT - 1) &
                       ~(HEAP_X_ALIGNMENT - 1);

        //
        // Now compute the height in pixels of the surface
        // (fix for SPR 16317)
        //

        dwHeight = (dwHeight * lLinearPitch + ppdev->lDelta - 1) /
                   ppdev->lDelta;

        // Now fill in enough stuff to have the DirectDraw heap-manager
        // do the allocation for us:

        lpSurfaceGlobal->fpVidMem       = DDHAL_PLEASEALLOC_BLOCKSIZE;
        lpSurfaceGlobal->dwBlockSizeX   = ppdev->lDelta;  // Specified in bytes
        lpSurfaceGlobal->dwBlockSizeY   = dwHeight;
        lpSurfaceGlobal->lPitch         = lLinearPitch;
        lpSurfaceDesc->lPitch           = lLinearPitch;
        lpSurfaceDesc->dwFlags         |= DDSD_PITCH;

#else

        //
        // We fall through here whether it is overlay or offscreen surface
        // Allocate a space in off-screen memory, using our own heap
        // manager:
        //

        //
        // check to see if we have any video memory left!
        //

        if ( ppdev->heap.ulMaxOffscrnSize)
        {
            //
            // this allocation will unfortunately waste at most 1023 bytes,
            // but at least we call it "linear".  That way we don't have
            // to worry about the size of our off-screen surfaces.  Just allocate
            // and set the stride.  The "rectangle" is for the benefit of the
            // allocation routines.
            //

            lLinearPitch = (dwWidth * dwByteCount + HEAP_X_ALIGNMENT - 1) &
                           ~(HEAP_X_ALIGNMENT - 1);

            // fSurfaceWidth used only to determine how much memory to
            // allocate for the stream.  To fix SPR 15880, we must make sure
            // the dwWidth used in the calculation is 32 pixel aligned!  If
            // we do this, no need to worry about the heap alignment...

            fSurfaceWidth = ( dwWidth + 31 ) & ~31;

            // Now compensate for possible difference in desktop pixel depth
            // vs video pixel depth.

            fSurfaceWidth = ( fSurfaceWidth + (ppdev->cjPelSize - 1) ) *
                            dwByteCount / ppdev->cjPelSize;

            //
            // work around for Elhatv1.mpg, where the .mpg file is running
            // pass the allocated memory, similar to SPR 15880 above.
            // MS has been informed about this problem, but no response
            // as of yet.
            //

            if (lpSurfaceGlobal->ddpfSurface.dwFlags & DDPF_FOURCC)
            {
                dwHeight += 8;
            }

            poh = pohAllocate( ppdev,
                               fSurfaceWidth,
                               dwHeight,
                               FLOH_MAKE_PERMANENT );

            if (poh != NULL)
            {
                DISPDBG((1, "DdCreateSurface:poh, fpVidMem, Caps = %lx, %lx, %lx",
                             poh, poh->ulBase, lpSurfaceLocal->ddsCaps.dwCaps ));
                DISPDBG((1, "dwWidth, dwHeight = %d, %d", dwWidth, dwHeight));

                pDDData->poh                    = poh;
                lpSurfaceGlobal->fpVidMem       = (FLATPTR) poh->ulBase;
                lpSurfaceGlobal->lPitch         = lLinearPitch;
                lpSurfaceGlobal->xHint          = poh->x;
                lpSurfaceGlobal->yHint          = poh->y;
                lpSurfaceDesc->lPitch           = lLinearPitch;
                lpSurfaceDesc->dwFlags         |= DDSD_PITCH;

                //
                // We handled the creation entirely ourselves, so we have to
                // set the return code and return DDHAL_DRIVER_HANDLED:
                //

                dwRet = DDHAL_DRIVER_HANDLED;

            }
            else
            {
                //
                // return NOTHANDLED or ActiveMovie fails!
                //

                dwRet = DDHAL_DRIVER_NOTHANDLED;
            }
        }
        else
        {
            //
            // return NOTHANDLED or ActiveMovie fails!
            //

            dwRet = DDHAL_DRIVER_NOTHANDLED;
        }

#endif

        //
        // This needs to be assigned under this if condition because
        // ViRGE can still allocate memory, but doesn't set this flag
        // accordingly.
        //

#if SUPPORT_MCD

        if ( lpSurfaceGlobal->ddpfSurface.dwYUVBitCount !=
                                        (DWORD) (8 * ppdev->cjPelSize)  ||
             ( lpSurfaceGlobal->ddpfSurface.dwRBitMask != ppdev->flRed ) )
        {
            pDDData->dwValue = DD_RESERVED_DIFFERENTPIXELFORMAT;
        }

#else

        pDDData->dwValue = DD_RESERVED_DIFFERENTPIXELFORMAT;

#endif //SUPPORT_MCD

        }
        else
        {
            DISPDBG( (DDRAW_DEBUG_LEVEL,
                     "Refused to create surface with large width") );
            lpCreateSurface->ddRVal = DDERR_TOOBIGSIZE;
            dwRet = DDHAL_DRIVER_HANDLED;
        }

#if !SUPPORT_MCD

      }

#endif //!SUPPORT_MCD

    //
    // always pass around, if successful, setting up for "future" use
    //

    lpSurfaceGlobal->dwReserved1 = (ULONG_PTR)pDDData;

    if ( fStreamsIsOff )
    {
        //
        // turn on streams always, more efficient for flip (?)
        // NOTE, M3/M5 PS Frame Buffer 0 not funct. and never able
        // to do FLIP!
        //

        if ( (fOverlay && fGX2_Style_SP) || !fGX2_Style_SP )
        {
            ppdev->pfnTurnOnStreams (ppdev);
        }
    }

    if ( ppdev->ulCaps2 & CAPS2_COLOR_ADJUST_ALLOWED )
    {
        if ( ppdev->ulCanAdjustColor == (COLOR_ADJUSTED | COLOR_STATUS_YUV) )
        {
            WRITE_STREAM_D( ppdev->pjMmBase,
                            COLOR_ADJUST_REG,
                            READ_REGISTER_ULONG(
                                (BYTE *) (ppdev->pjMmBase + COLOR_ADJUST_REG) ) |
                                COLOR_ADJUST_ON );
        }
        else if ( ppdev->ulCanAdjustColor == COLOR_ADJUSTED )
        {
            WRITE_STREAM_D( ppdev->pjMmBase,
                            COLOR_ADJUST_REG,
                            READ_REGISTER_ULONG(
                                (BYTE *) (ppdev->pjMmBase + COLOR_ADJUST_REG) ) &
                                ~COLOR_ADJUST_ON );
        }
    }

    return( dwRet );

}

/******************************Public*Routine******************************\
* DWORD DdSetColorKey
*
\**************************************************************************/

DWORD DdSetColorKey(
PDD_SETCOLORKEYDATA lpSetColorKey)
{
    PDEV*               ppdev;
    BYTE*               pjMmBase;
    DD_SURFACE_GLOBAL*  lpSurface;
    DWORD               dwKeyLow;
    DWORD               dwKeyHigh;
    DWORD               dwBlendCtrl;

    DISPDBG((4, "In DdSetColorKey"));

    ppdev = (PDEV*) lpSetColorKey->lpDD->dhpdev;

    pjMmBase  = ppdev->pjMmBase;
    lpSurface = lpSetColorKey->lpDDSurface->lpGbl;

    // We don't have to do anything for normal blt source colour keys:

    if (lpSetColorKey->dwFlags & DDCKEY_SRCBLT)
    {
        lpSetColorKey->ddRVal = DD_OK;
        return(DDHAL_DRIVER_HANDLED);
    }
    else if (lpSetColorKey->dwFlags & DDCKEY_DESTOVERLAY)
    {
        dwBlendCtrl = KeyOnP;
    }
    else if ( lpSetColorKey->dwFlags & DDCKEY_SRCOVERLAY )
    {
        dwBlendCtrl = KeyOnS;
    }
    else
    {
        lpSetColorKey->ddRVal = DDERR_UNSUPPORTED;
        return( DDHAL_DRIVER_HANDLED );
    }

    dwKeyLow = lpSetColorKey->ckNew.dwColorSpaceLowValue;
    dwKeyHigh = lpSetColorKey->ckNew.dwColorSpaceHighValue;

    if ( lpSurface->ddpfSurface.dwFlags & DDPF_FOURCC )
    {
        switch ( lpSurface->ddpfSurface.dwFourCC )
        {
            case FOURCC_YUY2:
            dwKeyLow = (dwKeyLow & 0x00ff7f7f) | (~(dwKeyLow | 0x00ff7f7f));
            dwKeyHigh = (dwKeyHigh & 0x00ff7f7f) | (~(dwKeyHigh | 0x00ff7f7f));
            break;

        default:
            lpSetColorKey->ddRVal = DDERR_UNSUPPORTED;
            return( DDHAL_DRIVER_HANDLED );
            break;
        }
    }
    else if (lpSurface->ddpfSurface.dwFlags & DDPF_PALETTEINDEXED8)
    {
        if ( ppdev->Band.SWFlags & SW_INDEXCOLORKEY )
        {
            dwKeyLow |= AlphaKeying | CompareBits0t7;
        }
        else
        {
            dwKeyLow = dwGetPaletteEntry(ppdev, dwKeyLow);
            dwKeyHigh = dwKeyLow;
            dwKeyLow |= CompareBits2t7;
        }
    }
    else
    {
        ASSERTDD(lpSurface->ddpfSurface.dwFlags & DDPF_RGB,
            "Expected only RGB cases here");

        // We have to transform the colour key from its native format
        // to 8-8-8:

        switch ( lpSurface->ddpfSurface.dwRGBBitCount )
        {
        case 8:
            if ( ppdev->Band.SWFlags & SW_INDEXCOLORKEY )
            {
                dwKeyLow |= AlphaKeying | CompareBits0t7;
            }
            else
            {
                dwKeyLow = dwGetPaletteEntry(ppdev, dwKeyLow);
                dwKeyHigh = dwKeyLow;
                dwKeyLow |= CompareBits2t7;
            }
            break;
        case 16:
            if (IS_RGB15_R(lpSurface->ddpfSurface.dwRBitMask))
            {
                dwKeyLow = RGB15to32(dwKeyLow);
                dwKeyHigh = RGB15to32(dwKeyHigh);
            }
            else
            {
                dwKeyLow = RGB16to32(dwKeyLow);
                dwKeyHigh = RGB16to32(dwKeyHigh);
            }

            dwKeyLow |= CompareBits3t7;
            break;

        default:
            dwKeyLow |= CompareBits0t7;
            break;
        }
    }

    dwKeyLow |= KeyFromCompare;

    if ( ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE )
    {
        dwKeyLow &= 0x07ffffff;
        dwKeyLow |= ColorKeying;

        if ( (dwBlendCtrl & KeyOnP) == KeyOnP )
        {
            dwKeyLow |= KeySelectP;
        }
        else
        {
            dwKeyLow |= KeySelectS;
        }
        dwBlendCtrl = GX2_Ks_Mask;

        //
        // Disable Opaque Control to avoid noise.
        //

        WRITE_STREAM_D(pjMmBase, OPAQUE_CONTROL, 0);
        WAIT_FOR_VBLANK(ppdev);
    }
    else
    {
        WAIT_FOR_VBLANK(ppdev);
    }


    WRITE_STREAM_D(pjMmBase, BLEND_CONTROL, dwBlendCtrl );
    WRITE_STREAM_D(pjMmBase, CKEY_LOW, dwKeyLow);
    WRITE_STREAM_D(pjMmBase, CKEY_HI,  dwKeyHigh);

    lpSetColorKey->ddRVal = DD_OK;
    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdSetOverlayPosition
*
* ASSUMPTIONS:  Assumes that the following fields have already been
*               initialized:
*               ppdev->DisplayXOffset
*               ppdev->DisplayYOffset
*               ppdev->XExpansion
*               ppdev->YExpansion
*
\**************************************************************************/

DWORD DdSetOverlayPosition(
PDD_SETOVERLAYPOSITIONDATA lpSetOverlayPosition)
{
    PDEV*   ppdev;
    BYTE*   pjMmBase;
    DWORD   NewX, NewY;
    WORD    DuoViewFlags;

    DISPDBG((4, "In DdSetOverlayPosition"));

    ppdev = (PDEV*) lpSetOverlayPosition->lpDD->dhpdev;
    pjMmBase = ppdev->pjMmBase;

    //
    //  make sure the overlay is visible
    //

    if (lpSetOverlayPosition->lpDDSrcSurface->lpGbl->fpVidMem !=
                                            ppdev->fpVisibleOverlay)
    {
        lpSetOverlayPosition->ddRVal = DD_OK;
        return(DDHAL_DRIVER_HANDLED);
    }

    //
    //  The following code assumes we are on a GX2 compatible streams
    //  processor, since the older ViRGE and Trio code did not support
    //  streams on a Dual IGA (mobile) platform.
    //

    DuoViewFlags = ppdev->Band.DuoViewFlags;
    NewX = lpSetOverlayPosition->lXPos;
    NewY = lpSetOverlayPosition->lYPos;

    if (DuoViewFlags & DVW_Panning_ON)
    {
        ddUpdateStreamsForPanning (ppdev, &NewX, &NewY);
        //
        //  Note that ddUpdateStreamsForPanning does check and handle
        //  DVW_Expand_ON and DVW_Center_ON flags.
        //
    }
    else
    {
        if (DuoViewFlags & DVW_Expand_ON)
        {
            NewX = (NewX * (ppdev->XExpansion >> 16))               /
                                    (ppdev->XExpansion & 0xFFFF)    +
                                    ppdev->DisplayXOffset;
            NewY = (NewY * (ppdev->YExpansion >> 16))               /
                                    (ppdev->YExpansion & 0xFFFF)    +
                                    ppdev->DisplayYOffset;
        }
        else if (DuoViewFlags & DVW_Center_ON)
        {
            NewX += ppdev->DisplayXOffset;
            NewY += ppdev->DisplayYOffset;
        }

        WRITE_STREAM_D(pjMmBase, S_XY, XY(NewX, NewY));
    }

    //
    //  save the new position
    //

    ppdev->SSX = lpSetOverlayPosition->lXPos;
    ppdev->SSY = lpSetOverlayPosition->lYPos;

    lpSetOverlayPosition->ddRVal = DD_OK;
    return(DDHAL_DRIVER_HANDLED);
}

#if _WIN32_WINNT >= 0x0500
extern BOOL WINAPI
DevD3DHALCreateDriver(LPD3DHAL_GLOBALDRIVERDATA *lplpGlobal,
                      LPD3DHAL_CALLBACKS *lplpHALCallbacks);
#endif // _WIN32_WINNT >= 0x0500

/******************************Public*Routine******************************\
* BOOL DrvGetDirectDrawInfo
*
* Will be called twice before DrvEnableDirectDraw is called.
*
\**************************************************************************/

BOOL DrvGetDirectDrawInfo(
DHPDEV          dhpdev,
DD_HALINFO*     pHalInfo,
DWORD*          pdwNumHeaps,
VIDEOMEMORY*    pvmList,            // Will be NULL on first call
DWORD*          pdwNumFourCC,
DWORD*          pdwFourCC)          // Will be NULL on first call
{
    PDEV*       ppdev;
    LONGLONG    li;
    OH*         poh;
    LONG        bpp;
#if _WIN32_WINNT >= 0x0500
    BOOL        b3DSupport = FALSE;
#endif //_WIN32_WINNT

    DISPDBG((4, "In DrvGetDirectDrawInfo"));

    ppdev = (PDEV*) dhpdev;

    *pdwNumFourCC = 0;
    *pdwNumHeaps = 0;

    // We may not support DirectDraw on this card:

    if (!(ppdev->flStatus & STAT_DIRECTDRAW_CAPABLE))
    {
        return(FALSE);
    }

    pHalInfo->dwSize = sizeof(*pHalInfo);

    // Current primary surface attributes.  Since HalInfo is zero-initialized
    // by GDI, we only have to fill in the fields which should be non-zero:

    pHalInfo->vmiData.fpPrimary       = 0;
    pHalInfo->vmiData.pvPrimary       = ppdev->pjScreen;
    pHalInfo->vmiData.dwDisplayWidth  = ppdev->cxScreen;
    pHalInfo->vmiData.dwDisplayHeight = ppdev->cyScreen;
    pHalInfo->vmiData.lDisplayPitch   = ppdev->lDelta;

    pHalInfo->vmiData.ddpfDisplay.dwSize  = sizeof(DDPIXELFORMAT);
    pHalInfo->vmiData.ddpfDisplay.dwFlags = DDPF_RGB;

    pHalInfo->vmiData.ddpfDisplay.dwRGBBitCount = (ppdev->cBitsPerPel+1) & ~7;

#if _WIN32_WINNT >= 0x0500
    if (ppdev->ulCaps & CAPS_S3D_ENGINE)
        b3DSupport = TRUE;
#endif //_WIN32_WINNT

    if (ppdev->iBitmapFormat == BMF_8BPP)
    {
        pHalInfo->vmiData.ddpfDisplay.dwFlags |= DDPF_PALETTEINDEXED8;
    }

    // These masks will be zero at 8bpp:

    pHalInfo->vmiData.ddpfDisplay.dwRBitMask = ppdev->flRed;
    pHalInfo->vmiData.ddpfDisplay.dwGBitMask = ppdev->flGreen;
    pHalInfo->vmiData.ddpfDisplay.dwBBitMask = ppdev->flBlue;

    // Free up as much off-screen memory as possible:
    bMoveAllDfbsFromOffscreenToDibs(ppdev);

#if SUPPORT_MCD

    //
    // Added to support MCD.
    // Since we do our own memory allocation, we have to set dwVidMemTotal
    // ourselves.  Note that this represents the amount of available off-
    // screen memory, not all of video memory:
    //

    pHalInfo->ddCaps.dwVidMemTotal   = ppdev->heap.ulMaxOffscrnSize;

#else

    // Now simply reserve the biggest chunk for use by DirectDraw:

    poh = ppdev->pohDirectDraw;
    if (poh == NULL)
    {
        bpp=CONVERT_TO_BYTES(1,ppdev);
        poh = pohAllocate(ppdev,
                          ppdev->heap.ulMaxOffscrnSize/bpp,
                          1,
                          FLOH_MAKE_PERMANENT);
        ppdev->pohDirectDraw = poh;
    }

    if (poh != NULL)
    {
        *pdwNumHeaps = 1;

        // Fill in the list of off-screen rectangles if we've been asked
        // to do so:

        if (pvmList != NULL)
        {
            pvmList->dwHeight       = poh->cy;
            pvmList->ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
            pvmList->fpStart        = poh->ulBase;

            pvmList->dwFlags        = VIDMEM_ISLINEAR;
            pvmList->fpEnd          = pvmList->fpStart +
                                      ((poh->cy-1) * ppdev->lDelta) +
                                      (poh->cx * ppdev->cjPelSize) - 1;
            pvmList->fpEnd         -= 1024; // hardware cursor
#if _WIN32_WINNT >= 0x0500
            // allow space for default white texture
            pvmList->fpEnd         -= 8;
            ppdev->D3DGlobals.dwWhiteTexture = pvmList->fpEnd + 1;

            DPF( "DirectDraw gets %li x %li surface at (%li, %li) - @ %x - %x",
                    poh->cx, poh->cy, poh->x, poh->y, pvmList->fpStart, pvmList->fpEnd);
#endif //_WIN32_WINNT
        }
    }
#endif //SUPPORT_MCD

    // dword alignment must be guaranteed for off-screen surfaces:

    pHalInfo->vmiData.dwOffscreenAlign = HEAP_X_ALIGNMENT;

#if _WIN32_WINNT >= 0x0500
    pHalInfo->vmiData.dwZBufferAlign = 8;
    pHalInfo->vmiData.dwTextureAlign = 8;
#endif

    // Capabilities supported:

    pHalInfo->ddCaps.dwCaps = DDCAPS_BLT
                            | DDCAPS_BLTCOLORFILL
#if _WIN32_WINNT >= 0x0500
                            | DDCAPS_BLTDEPTHFILL
#endif
                            | DDCAPS_COLORKEY;

#if _WIN32_WINNT >= 0x0500
    pHalInfo->ddCaps.dwCaps2 = DDCAPS2_COPYFOURCC;

    //declare we can handle textures wider than the primary
    pHalInfo->ddCaps.dwCaps2 |= DDCAPS2_WIDESURFACES;

    if (b3DSupport)
        pHalInfo->ddCaps.dwCaps |= DDCAPS_3D;
#endif

    if ( ppdev->cBitsPerPel != 24 )
    {
        pHalInfo->ddCaps.dwCKeyCaps = DDCKEYCAPS_SRCBLT;
    }

    pHalInfo->ddCaps.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN
                                                    | DDSCAPS_PRIMARYSURFACE
                                                    | DDSCAPS_FLIP;
#if _WIN32_WINNT >= 0x0500
    if (b3DSupport)
        pHalInfo->ddCaps.ddsCaps.dwCaps |= DDSCAPS_ZBUFFER
                                        |  DDSCAPS_TEXTURE
                                        |  DDSCAPS_3DDEVICE;

    // Let DirectDraw know that we support :

    pHalInfo->GetDriverInfo = DdGetDriverInfo;
    pHalInfo->dwFlags |= DDHALINFO_GETDRIVERINFOSET;

#endif //_WIN32_WINNT >= 0x0500

    // The Trio 64V+ has overlay streams capabilities which are a superset
    // of the above:

    if (ppdev->ulCaps & CAPS_STREAMS_CAPABLE)
    {
        // Overlays need 8-byte alignment.  Note that if 24bpp overlays are
        // ever supported, this will have to change to compensate:

        pHalInfo->vmiData.dwOverlayAlign = 8;

        pHalInfo->ddCaps.dwCaps |= DDCAPS_OVERLAY
                                             | DDCAPS_OVERLAYSTRETCH
                                             | DDCAPS_OVERLAYFOURCC
                                             | DDCAPS_OVERLAYCANTCLIP
                                             | DDCAPS_COLORKEY;

        //
        // For M5 and GX-2, enable DdReadScanline Function.
        //

        if (ppdev->ulCaps2 & CAPS2_READ_SCANLINE)
        {
            pHalInfo->ddCaps.dwCaps |= DDCAPS_READSCANLINE;
        }

        pHalInfo->ddCaps.dwFXCaps |= DDFXCAPS_OVERLAYSTRETCHX |
                                     DDFXCAPS_OVERLAYSTRETCHY;

        // We support only destination colour keying because that's the
        // only permutation we've had a chance to test.

        pHalInfo->ddCaps.dwCKeyCaps |= DDCKEYCAPS_SRCOVERLAY |
                                                   DDCKEYCAPS_SRCOVERLAYCLRSPACEYUV |
                                                   DDCKEYCAPS_SRCOVERLAYONEACTIVE |
                                                   DDCKEYCAPS_SRCOVERLAYYUV |
                                               DDCKEYCAPS_DESTOVERLAY |
                                                   DDCKEYCAPS_DESTOVERLAYCLRSPACEYUV |
                                               DDCKEYCAPS_DESTOVERLAYONEACTIVE |
                                                   DDCKEYCAPS_DESTOVERLAYYUV;

        pHalInfo->ddCaps.ddsCaps.dwCaps |= DDSCAPS_OVERLAY;

        *pdwNumFourCC = 1;
        if (pdwFourCC)
        {
            pdwFourCC[0] = FOURCC_YUY2;
        }

        pHalInfo->ddCaps.dwMaxVisibleOverlays = 1;

        pHalInfo->ddCaps.dwMinOverlayStretch   =
        pHalInfo->ddCaps.dwMinLiveVideoStretch =
        pHalInfo->ddCaps.dwMinHwCodecStretch   = ppdev->ulMinOverlayStretch;

        pHalInfo->ddCaps.dwMaxOverlayStretch   =
        pHalInfo->ddCaps.dwMaxLiveVideoStretch =
        pHalInfo->ddCaps.dwMaxHwCodecStretch   = 9999;
    }

#if _WIN32_WINNT >= 0x0500

        // LPB_DriverInit is in some common code shared with the Win95
        // driver.  It expects some data to be pre-initialized in the
        // 'sData' structure (which under NT is actually a 'ppdev'),
        // which we'll do here:

        ppdev->lpMMReg   = (ULONG_PTR)ppdev->pjMmBase;
        ppdev->ddHALInfo = *pHalInfo;

        RtlZeroMemory(&ppdev->LPBData, sizeof(ppdev->LPBData));

        // Initialize the S3 LPB, which handles VPE.

        LPB_DriverInit(ppdev);

        // Again, this weirdness of temporarily copying 'pHalInfo' is due
        // to having to share code with the Win95 driver:

        *pHalInfo = ppdev->ddHALInfo;

#endif


#if _WIN32_WINNT >= 0x0500

    if( b3DSupport )
    {
        DevD3DHALCreateDriver((LPD3DHAL_GLOBALDRIVERDATA *)&pHalInfo->lpD3DGlobalDriverData,
                              (LPD3DHAL_CALLBACKS *)&pHalInfo->lpD3DHALCallbacks);
    }

#endif // _WIN32_WINNT >= 0x0500

    return(TRUE);
}

/******************************Public*Routine******************************\
* DWORD DdDestroySurface
*
* Note that if DirectDraw did the allocation, DDHAL_DRIVER_NOTHANDLED
* should be returned.
*
\**************************************************************************/

DWORD DdDestroySurface(
PDD_DESTROYSURFACEDATA lpDestroySurface)
{
    PDEV*               ppdev;
    DD_SURFACE_GLOBAL*  lpSurface;
    LONG                lPitch;
    PDDRAWDATA          pData;

#if !SUPPORT_MCD && (_WIN32_WINNT >= 0x0500)
    lpDestroySurface->ddRVal = DD_OK;
    return(DDHAL_DRIVER_NOTHANDLED);
#else

    ppdev       = (PDEV*) lpDestroySurface->lpDD->dhpdev;
    lpSurface   = lpDestroySurface->lpDDSurface->lpGbl;
    pData       = (PDDRAWDATA) lpSurface->dwReserved1;

    if ( pData )
    {
        DISPDBG( ( 1,
                   "DdDestroySurface: poh, Caps = %lx, %lx",
                   lpSurface->dwReserved1,
                   lpDestroySurface->lpDDSurface->ddsCaps.dwCaps ) );

        if ( pData->poh )
        {
            pohFree( ppdev, pData->poh );
        }

        EngFreeMem( (PVOID) pData );
    }

    // Since we did the original allocation ourselves, we have to
    // return DDHAL_DRIVER_HANDLED here:

    lpDestroySurface->ddRVal = DD_OK;
    return(DDHAL_DRIVER_HANDLED);

#endif
}

/**************************************************************************\
* ULONG TotalAvailVideoMemory
*
*    Added for GetAvailVideoMemoty calback
*    Calculate total amount of offscreen video memory without permanent
*    driver allocations. We need to do it here since we won't be able
*    to distinguish between driver's permanent allocation and ddraw's
*    permanent allocation later.
*
\**************************************************************************/

ULONG TotalAvailVideoMemory(PPDEV ppdev)
{
    OH      *poh;
    OH      *pohSentinel;
    ULONG    ulSize;
    ULONG    i;

    ASSERTDD(ppdev != NULL,"Bad ppdev TotalAvailVideoMemory");

    ulSize   = 0;
    pohSentinel = &ppdev->heap.ohFree;

    for (i = 2; i != 0; i--)
    {
        for (poh = pohSentinel->pohNext; poh != pohSentinel; poh = poh->pohNext)
        {
            if( poh->ohState != OH_PERMANENT )
            {
                ulSize += poh->ulSize;
            }
        }

        // Second time through, loop through the list of discardable
        // rectangles:

        pohSentinel = &ppdev->heap.ohDiscardable;
    }

    return ulSize;
}

/******************************Public*Routine******************************\
* BOOL DrvEnableDirectDraw
*
* This function is called by GDI to enable DirectDraw when a DirectDraw
* program is started and DirectDraw is not already active.
*
\**************************************************************************/

BOOL DrvEnableDirectDraw(
DHPDEV                  dhpdev,
DD_CALLBACKS*           pCallBacks,
DD_SURFACECALLBACKS*    pSurfaceCallBacks,
DD_PALETTECALLBACKS*    pPaletteCallBacks)
{
    PDEV* ppdev;

    ppdev = (PDEV*) dhpdev;

    DISPDBG((4, "In DrvEnableDirectDraw"));

    //
    //  If going into a direct draw mode that requires panning,
    //  only the software cursor updates (though DrvMovePointer)
    //  will be called.  Thus, to enable panning, we have to set
    //  the flStatus flag to force panning updates from DrvMovePointer.
    //
    //  Note, this flag is reset whenever we go through a hardware
    //  set cursor... but if we happen to have an animated hardware
    //  cursor, we might get the SW_CURSOR_PAN flag cleared while we
    //  still need to be handling panning.  So set STAT_DDRAW_PAN as well.
    //

    if ( ppdev->ulCaps & CAPS_PANNING )
    {
        ppdev->flStatus |= STAT_SW_CURSOR_PAN;
        ppdev->flStatus |= STAT_DDRAW_PAN;
    }

    pCallBacks->WaitForVerticalBlank = DdWaitForVerticalBlank;
    pCallBacks->MapMemory            = DdMapMemory;

#if SUPPORT_MCD

    pCallBacks->CreateSurface        = DdCreateSurface;
    pCallBacks->CanCreateSurface     = DdCanCreateSurface;
    pCallBacks->dwFlags              = DDHAL_CB32_WAITFORVERTICALBLANK
                                     | DDHAL_CB32_MAPMEMORY
                                     | DDHAL_CB32_CREATESURFACE
                                     | DDHAL_CB32_CANCREATESURFACE;

#else

    pCallBacks->dwFlags              = DDHAL_CB32_WAITFORVERTICALBLANK
                                     | DDHAL_CB32_MAPMEMORY;

#endif


    if (ppdev->ulCaps2 & CAPS2_READ_SCANLINE)
    {
        pCallBacks->GetScanLine = DdGetScanLine;
        pCallBacks->dwFlags    |= DDHAL_CB32_GETSCANLINE;
    }


    pSurfaceCallBacks->Blt = DdBlt;

    pSurfaceCallBacks->Flip           = DdFlip;
    pSurfaceCallBacks->Lock           = DdLock;
    pSurfaceCallBacks->GetBltStatus   = DdGetBltStatus;
    pSurfaceCallBacks->GetFlipStatus  = DdGetFlipStatus;
    pSurfaceCallBacks->DestroySurface = DdDestroySurface;
    pSurfaceCallBacks->dwFlags        = DDHAL_SURFCB32_BLT
                                      | DDHAL_SURFCB32_LOCK
                                      | DDHAL_SURFCB32_FLIP
                                      | DDHAL_SURFCB32_GETBLTSTATUS
                                      | DDHAL_SURFCB32_DESTROYSURFACE
                                      | DDHAL_SURFCB32_GETFLIPSTATUS;

    if (ppdev->ulCaps2 & CAPS2_TV_UNDERSCAN_CAPABLE)
    {
        pSurfaceCallBacks->Unlock   = DdUnlock;
        pSurfaceCallBacks->dwFlags |= DDHAL_SURFCB32_UNLOCK;
    }

    // We can do overlays only when the Streams processor is enabled:

    if (ppdev->ulCaps & CAPS_STREAMS_CAPABLE)
    {

#if !SUPPORT_MCD

        pCallBacks->CreateSurface             = DdCreateSurface;
        pCallBacks->CanCreateSurface          = DdCanCreateSurface;
        pCallBacks->dwFlags                  |= DDHAL_CB32_CREATESURFACE
                                              | DDHAL_CB32_CANCREATESURFACE;
#endif

        pSurfaceCallBacks->SetColorKey        = DdSetColorKey;
        pSurfaceCallBacks->UpdateOverlay      = DdUpdateOverlay;
        pSurfaceCallBacks->SetOverlayPosition = DdSetOverlayPosition;
        pSurfaceCallBacks->dwFlags           |= DDHAL_SURFCB32_SETCOLORKEY
                                              | DDHAL_SURFCB32_UPDATEOVERLAY
                                              | DDHAL_SURFCB32_SETOVERLAYPOSITION;

    pSurfaceCallBacks->SetColorKey    = D3DSetColorKey32;
    pSurfaceCallBacks->dwFlags        |= DDHAL_SURFCB32_SETCOLORKEY;

        // The DrvEnableDirectDraw call can occur while we're in full-
        // screen DOS mode.  Do not turn on the streams processor now
        // if that's the case, instead wait until AssertMode switches
        // us back to graphics mode:
    }

    // Note that we don't call 'vGetDisplayDuration' here, for a couple of
    // reasons:
    //
    //  o Because the system is already running, it would be disconcerting
    //    to pause the graphics for a good portion of a second just to read
    //    the refresh rate;
    //  o More importantly, we may not be in graphics mode right now.
    //
    // For both reasons, we always measure the refresh rate when we switch
    // to a new mode.

#if _WIN32_WINNT >= 0x0500
    ppdev->ulTotalAvailVideoMemory = TotalAvailVideoMemory(ppdev);
#endif


    return(TRUE);
}

/******************************Public*Routine******************************\
* VOID DrvDisableDirectDraw
*
* This function is called by GDI when the last active DirectDraw program
* is quit and DirectDraw will no longer be active.
*
\**************************************************************************/

VOID DrvDisableDirectDraw(
DHPDEV      dhpdev)
{
    PDEV* ppdev;

    DISPDBG((4, "In DrvDisableDirectDraw"));

    ppdev = (PDEV*) dhpdev;

    // Only turn off the streams processor if we're still in graphics mdoe:

    if ((ppdev->flStatus & STAT_STREAMS_ENABLED) && (ppdev->bEnabled))
    {
        ppdev->pfnTurnOffStreams (ppdev);
    }

    ppdev->fpVisibleBuffer = 0;

    // We only need to free up all off-screen memory here when running on
    // Trio. This is to free up memory allocated at DrvGetDirectDrawInfo
    // time. For Virge driver, since we are are handling memory management
    // ourselves, we will call pohFree at DrvDestroySurface time.
    //
#if !SUPPORT_MCD
    // DirectDraw is done with the display, so we can go back to using
    // all of off-screen memory ourselves:

    pohFree(ppdev, ppdev->pohDirectDraw);
    ppdev->pohDirectDraw = NULL;
#endif

    ppdev->ulCanAdjustColor &= ~(2);        // clean up

        ppdev->flStatus &= ~STAT_DDRAW_PAN;

}

/******************************Public*Routine******************************\
* VOID vAssertModeDirectDraw
*
* This function is called by enable.c when entering or leaving the
* DOS full-screen character mode.
*
\**************************************************************************/

VOID vAssertModeDirectDraw(
PDEV*   ppdev,
BOOL    bEnable)
{
    DISPDBG((4, "In vAssertModeDirectDraw"));

    if (ppdev->flStatus & STAT_STREAMS_ENABLED)
    {
        if (bEnable)            // Enable graphics mode
        {
            ppdev->pfnTurnOnStreams (ppdev);
        }
        else                    // Disable graphics mode
        {
            ppdev->pfnTurnOffStreams (ppdev);
        }
    }
}

/******************************Public*Routine******************************\
* VOID vDisableDirectDraw
*
* This function is called by enable.c when the driver is shutting down.
*
\**************************************************************************/

VOID vDisableDirectDraw(
PDEV*   ppdev)
{
    DISPDBG((4, "In vDisableDirectDraw"));

    // Note that we don't have to free 'ppdev->pohVideoEngineScratch', as
    // the off-screen heap code will do that automatically.
}


// ---------------------------------------------------------------------------

VOID ddClipStreamsToViewport (
    PDEV*   ppdev,
    DWORD*  pdwSecX,
    DWORD*  pdwSecY,
    DWORD*  pWidth,
    DWORD*  pHeight,
    DWORD*  pXOffset,
    DWORD*  pYOffset )

{
    DWORD dwDisplayEnd;
    DWORD dwAdjustedSecY;

    //
    //  assume no adjustment needed
    //

    *pXOffset = *pYOffset = 0;
    ppdev->SSClipped = FALSE;

    //
    //  correct screen location for aperture
    //
    //
    //  first clip in X direction
    //

    if (ppdev->ApertureXOffset > *pdwSecX)
    {
        //
        //  start X is off screen (left)
        //

        if (*pdwSecX + ppdev->SSdstWidth <= ppdev->ApertureXOffset)
        {
            //
            //  entire stream is off screen
            //

            *pdwSecX = *pdwSecY = *pHeight = 0;
            *pWidth = 1;
            ppdev->SSClipped = TRUE;
            return;
        }
        else
        {
            //
            //  part of the stream is still on the screen
            //
            //  correct the width of the stream that will be on screen
            //  and determine the offset into the source stream
            //

            *pWidth -= ppdev->ApertureXOffset - *pdwSecX;
            *pXOffset = ((ppdev->ApertureXOffset - *pdwSecX) *
                            ppdev->SSsrcWidth) / ppdev->SSdstWidth;
            *pdwSecX = 0;
            ppdev->SSClipped = TRUE;
        }
    }
    else
    {
        *pdwSecX -= ppdev->ApertureXOffset;

        //
        // Check to see if we need to clip X on right
        //

        dwDisplayEnd = ppdev->Band.GSizeX2 + *pXOffset;

        if ((*pdwSecX + *pWidth) > dwDisplayEnd)
        {
            *pWidth = dwDisplayEnd - *pdwSecX;
            ppdev->SSClipped = TRUE;
        }
    }

    //
    //  now clip in the Y direction
    //

    dwAdjustedSecY = *pdwSecY - ppdev->DisplayYOffset;

//
// SPR 18098 fix begin
//

    if ((ppdev->ApertureYOffset) > dwAdjustedSecY)
    {
        //
        //  start Y is off screen
        //
        if (dwAdjustedSecY + ppdev->SSdstHeight <= ppdev->ApertureYOffset)
        {
            //
            //  entire stream is off screen
            //

            *pdwSecX = *pdwSecY = *pHeight = 0;
            *pWidth = 1;
            ppdev->SSClipped = TRUE;
        }
        else
        {
            //
            //  part of the stream is still on the screen
            //

            //  correct the Height of the stream that will be on screen
            //  and determine the offset into the source stream
            //

            *pHeight -= ppdev->ApertureYOffset - dwAdjustedSecY;
            *pYOffset = ((ppdev->ApertureYOffset - dwAdjustedSecY) *
                            ppdev->SSsrcHeight) / ppdev->SSdstHeight;

            //
            //  account for possible blanking area.
            //

            *pdwSecY = ppdev->DisplayYOffset;

            ppdev->SSClipped = TRUE;
        }
    }
    else
    {
        *pdwSecY -= ppdev->ApertureYOffset;


        //
        // Check to see if we need to clip Y on Bottom
        //

        dwDisplayEnd = ppdev->Band.VisibleHeightInPixels +
            ppdev->DisplayYOffset;

        if ((*pdwSecY + *pHeight) > dwDisplayEnd)
        {

            if (*pdwSecY >= dwDisplayEnd)
            {
                *pdwSecX = *pdwSecY = *pHeight = 0;
                *pWidth = 1;
            }
            else
            {
                *pHeight = dwDisplayEnd - *pdwSecY;
            }

//
// SPR 18098 fix end
//

            ppdev->SSClipped = TRUE;
        }
    }

    return;
}

// ---------------------------------------------------------------------------

VOID ddSetStreamsFifo ( PDEV*   ppdev,
                        BOOL    fRestoreBootSettings )
{
    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    if (fRestoreBootSettings)
    {
        /*  Do not modify STN FIFO values.

        //
        // STN Fifo and Timeout, and Secondary Streams Fifo and Timeout
        //

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->bDefaultSTNRdTimeOut<<8) | GX2_STN_READ_TIMEOUT );

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->bDefaultSTNWrTimeOut<<8) | GX2_STN_WRITE_TIMEOUT );

        */

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->bDefaultSSTimeOut<<8) | GX2_SS_TIMEOUT );

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->bDefaultSSThreshold<<8) | GX2_SS_FIFO_THRESHOLD );

        //
        // Primary Stream Fifo and Timeout
        //

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->bDefaultPS1TimeOut<<8) | GX2_PS1_TIMEOUT );

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->bDefaultPS1Threshold<<8) | GX2_PS1_FIFO_THRESHOLD );

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->bDefaultPS2TimeOut<<8) | GX2_PS2_TIMEOUT );

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->bDefaultPS2Threshold<<8) | GX2_PS2_FIFO_THRESHOLD );
    }
    else
    {
        //
        // STN Fifo and Timeout, and Secondary Streams Fifo and Timeout
        //

        /*  Do not modify STN FIFO values.

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->Band.STNRdTimeOut<<8) | GX2_STN_READ_TIMEOUT );

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->Band.STNWrTimeOut<<8) | GX2_STN_WRITE_TIMEOUT );

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->Band.STNRdThreshold<<8) | GX2_STN_READ_FIFO_THRESHOLD );

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->Band.STNWrThreshold<<8) | GX2_STN_WRITE_FIFO_THRESHOLD );

        */

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->Band.SSTimeOut<<8) | GX2_SS_TIMEOUT );

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->Band.Fifo.SSThreshold<<8) | GX2_SS_FIFO_THRESHOLD );

        //
        // Primary Stream Fifo and Timeout
        //

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->Band.PS1TimeOut<<8) | GX2_PS1_TIMEOUT );

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->Band.Fifo.PS1Threshold<<8) | GX2_PS1_FIFO_THRESHOLD );

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->Band.PS2TimeOut<<8) | GX2_PS2_TIMEOUT );

        VGAOUTPW( ppdev,
                  CRTC_INDEX,
                  (ppdev->Band.Fifo.PS2Threshold<<8) | GX2_PS2_FIFO_THRESHOLD );
    }

    RELEASE_CRTC_CRITICAL_SECTION(ppdev);
}


// ---------------------------------------------------------------------------

VOID ddUpdateSecondaryStreams (
    PDEV*           ppdev,
    SS_UPDATE_FLAGS UpdateFlag
)

{
    BYTE*   pjMmBase = ppdev->pjMmBase;
    DWORD   StartAddr, AddrCorrection;
    DWORD   NewX = ppdev->SSX;
    DWORD   NewY = ppdev->SSY;
    DWORD   Width = ppdev->SSdstWidth;
    DWORD   Height = ppdev->SSdstHeight;
    DWORD   XOffset, YOffset;
    DWORD   dwDisplaySize, dwPanelSize;
    DWORD   DuoViewFlags = ppdev->Band.DuoViewFlags;
    UCHAR   SrReg;
    DWORD   ReturnedDataLength;
    BANDINF BandInf;
    BOOLEAN UpdateRequired = FALSE;

    //
    // Only continue if using a GX2 compat. stream AND we are currently
    // utilizing the stream hardware...
    //

    if ( !(ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE) ||
         !(ppdev->flStatus & STAT_STREAMS_UTILIZED) )
    {
        return;
    }

    if (UpdateFlag == SS_UPDATE_IGA)
    {
        //
        //  need to move the secondary streams to the new IGA
        //

        ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

        VGAOUTPW( ppdev, SEQ_INDEX, SEQ_REG_UNLOCK);

        VGAOUTP( ppdev, SEQ_INDEX, ARCH_CONFIG_REG );
        SrReg = VGAINP( ppdev, SEQ_DATA ) & (~STREAMS_IGA_SELECT_BIT);
        if (DuoViewFlags & DVW_SP_IGA1)
        {
            SrReg |= STREAMS_ON_IGA1;
        }
        else
        {
            SrReg |= STREAMS_ON_IGA2;
        }
        VGAOUTP( ppdev, SEQ_DATA, SrReg );

        //
        // Resynch vertical count to new IGA for the
        // temporary display until the mode set happens!
        //

        vSynchVerticalCount( ppdev, SrReg );

        //
        // and clear our trigger...
        // NOTE, since ppdev is reinitialized after every mode set,
        // this variable will be cleared on every mode set!
        //

        ppdev->bVertInterpSynched = 1;

        RELEASE_CRTC_CRITICAL_SECTION(ppdev);

        //
        //  now make adjustments for possible state changes
        //

        if (DuoViewFlags & (DVW_Expand_ON | DVW_Center_ON))
        {
            UpdateFlag = SS_UPDATE_CENTER_EXPAND;
            //
            // and the case for handling CENTER_EXPAND will also
            // check for and handle DVW_Panning_ON, if needed.
            //
        }
        else if (DuoViewFlags & DVW_Panning_ON)
        {
            UpdateFlag = SS_UPDATE_PANNING;
        }
    }

    switch (UpdateFlag)
    {
    case SS_UPDATE_PANNING:
        ddUpdateStreamsForPanning (ppdev, &NewX, &NewY);
        break;

    case SS_UPDATE_CENTER_EXPAND:

        //
        //  First, assume no X,Y adjustment (note that DdSetOverlayPosition
        //  relies on these fields having been initialized).
        //
        ppdev->DisplayXOffset = 0;
        ppdev->DisplayYOffset = 0;

        if (DuoViewFlags & DVW_Expand_ON)
        {
            //
            //  Physical Screen Size = GSizeX2, GSizeY2
            //  Viewable Pixels      = ppdev->VisibleWidth,VisibleHeight
            //  Note, additions inspired by BIOS update 0.05.35!
            //
            dwDisplaySize = ppdev->Band.VisibleWidthInPixels;
            dwPanelSize   = ppdev->Band.GSizeX2;
            ppdev->XExpansion = 0x00010001L;    // set to a valid default

            if ((2 * dwDisplaySize) <= dwPanelSize)
            {
                ppdev->XExpansion = 0x00020001L;
                Width = 2 * Width;
                //
                // NOTE:   the X starting offset has to be on a character
                //         clock, or 8 pixel boundary
                //

                ppdev->DisplayXOffset =
                     ((dwPanelSize - (2 * dwDisplaySize)) / 2) & 0xFFF8;
            }
            else if ((3 * dwDisplaySize) <= (2 * dwPanelSize))
            {
                ppdev->XExpansion = 0x00030002L;
                Width = (3 * Width) / 2;
                ppdev->DisplayXOffset = ((dwPanelSize -
                                      ((3 * dwDisplaySize)/2)) / 2) & 0xFFF8;
            }
            else if ((5 * dwDisplaySize) <= (4 * dwPanelSize))
            {
                ppdev->XExpansion = 0x00050004L;
                Width = (5 * Width) / 4;
                ppdev->DisplayXOffset = ((dwPanelSize -
                                      ((5 * dwDisplaySize)/4)) / 2) & 0xFFF8;
            }
            else if ((9 * dwDisplaySize) <= (8 * dwPanelSize))
            {
                ppdev->XExpansion = 0x00090008L;
                Width = (9 * Width) / 8;
                ppdev->DisplayXOffset = ((dwPanelSize -
                                      ((9 * dwDisplaySize)/8)) / 2) & 0xFFF8;
            }

            dwDisplaySize = ppdev->Band.VisibleHeightInPixels;
            dwPanelSize   = ppdev->Band.GSizeY2;
            ppdev->YExpansion = 0x00010001L;    // set to a valid default

            if ((2 * dwDisplaySize) <= dwPanelSize)
            {
                ppdev->YExpansion = 0x00020001L;
                Height = 2 * Height;
                ppdev->DisplayYOffset = (dwPanelSize - (2 * dwDisplaySize)) / 2;
            }
            else if ((3 * dwDisplaySize) <= (2 * dwPanelSize))
            {
                ppdev->YExpansion = 0x00030002L;
                Height = (3 * Height) / 2;
                ppdev->DisplayYOffset = (dwPanelSize -
                                          ((3 * dwDisplaySize)/2)) / 2;
            }
            else if ((4 * dwDisplaySize) <= (3 * dwPanelSize))
            {
                ppdev->YExpansion = 0x00040003L;
                Height = (4 * Height) / 3;
                ppdev->DisplayYOffset = (dwPanelSize -
                                      ((4 * dwDisplaySize)/3)) / 2;
            }
            else if ((5 * dwDisplaySize) <= (4 * dwPanelSize))
            {
                ppdev->YExpansion = 0x00050004L;
                Height = (5 * Height) / 4;
                ppdev->DisplayYOffset = (dwPanelSize -
                                      ((5 * dwDisplaySize)/4)) / 2;
            }
            else if ((6 * dwDisplaySize) <= (5 * dwPanelSize))
            {
                ppdev->YExpansion = 0x00060005L;
                Height = (6 * Height) / 5;
                ppdev->DisplayYOffset = (dwPanelSize -
                                      ((6 * dwDisplaySize)/5)) / 2;
            }

            if ((ppdev->XExpansion != 0x00010001L) ||
                (ppdev->YExpansion != 0x00010001L))
            {
                UpdateRequired = TRUE;

                NewX = ((NewX * (ppdev->XExpansion >> 16)) /
                         (ppdev->XExpansion & 0xFFFF)) + ppdev->DisplayXOffset;
                NewY = ((NewY * (ppdev->YExpansion >> 16)) /
                         (ppdev->YExpansion & 0xFFFF)) + ppdev->DisplayYOffset;
            }
        }
        else if (ppdev->Band.DuoViewFlags & DVW_Center_ON)
        {
            //
            //  Physical Screen Size = GSizeX2, GSizeY2
            //  Viewable Pixels      = ppdev->VisibleWidth,VisibleHeight
            //  Note, additions inspired by BIOS update 0.05.35!
            //
            //  Check if Physical > Viewable; if so, make positioning
            //  adjustment.
            //
            if (ppdev->Band.GSizeX2 > ppdev->Band.VisibleWidthInPixels)
            {
                ppdev->DisplayXOffset = (((DWORD)ppdev->Band.GSizeX2 -
                                        ppdev->Band.VisibleWidthInPixels)/2)
                                        & 0xFFF8;
                NewX += ppdev->DisplayXOffset;
                UpdateRequired = TRUE;
            }
            if (ppdev->Band.GSizeY2 > ppdev->Band.VisibleHeightInPixels)
            {
                ppdev->DisplayYOffset = ((DWORD)ppdev->Band.GSizeY2 -
                                        ppdev->Band.VisibleHeightInPixels)/2;
                NewY += ppdev->DisplayYOffset;
                UpdateRequired = TRUE;
            }
        }

        if (UpdateRequired)
        {
            ppdev->SSdisplayWidth = Width;
            ppdev->SSdisplayHeight = Height;

            if (DuoViewFlags & DVW_Panning_ON)
            {
                //
                //  If we're panning as well as centering/expanding, then
                //  we didn't need to update NewX and NewY, and should leave
                //  that up to ddUpdateStreamsForPanning(), which will use
                //  our newly initialized DisplayXOffset, DisplayYOffset,
                //  XExpansion, YExpansion values.
                //
                NewX = ppdev->SSX;
                NewY = ppdev->SSY;

                ddUpdateStreamsForPanning ( ppdev,
                                            &NewX,
                                            &NewY);
            }
            else
            {
                WRITE_STREAM_D(pjMmBase, S_XY,    XY(NewX, NewY));
                WRITE_STREAM_D(pjMmBase, S_WH,    WH(Width, Height));
            }
            WRITE_STREAM_D(pjMmBase, S_HK1K2, HK1K2(ppdev->SSsrcWidth, Width));
            WRITE_STREAM_D(pjMmBase, S_VK2,   VK2(ppdev->SSsrcHeight, Height));
        }
        break;

    case SS_UPDATE_DISPLAY_CONTROL:


        if ( ppdev->Band.SWFlags & SW_CALCULATE )
        {
            BandInf.Option      = BANDWIDTH_SETSECONDARY;
            BandInf.device      = ID_SWCODEC;
            BandInf.bpp         = 16; //(worst case...)
            BandInf.xSrc        = (short) ppdev->SSsrcWidth;
            BandInf.ySrc        = (short) ppdev->SSsrcHeight;
            BandInf.xTrg        = (short) ppdev->SSdstWidth;
            BandInf.yTrg        = (short) ppdev->SSdstHeight;
            BandInf.xMax        = (short) ppdev->SSsrcWidth;
            BandInf.yMax        = (short) ppdev->SSsrcHeight;
            BandInf.RefreshRate = (short) ppdev->ulRefreshRate;
            BandInf.pBand       = (PVOID) &ppdev->Band;
            ppdev->Band.GRefreshRate2   = (WORD) ppdev->ulRefreshRate2;

            if ( EngDeviceIoControl( ppdev->hDriver,
                                     IOCTL_VIDEO_S3_QUERY_BANDWIDTH,
                                     &BandInf,
                                     sizeof( BANDINF ),
                                     &ppdev->Band,
                                     sizeof( BAND ),
                                     &ReturnedDataLength ) )
            {
                DISPDBG( ( DDRAW_DEBUG_LEVEL,
                           "Miniport reported no streams parameters" ) );
            }
            else
            {
                ddSetStreamsFifo( ppdev, FALSE );
            }
        }
        break;

    }
}


#if _WIN32_WINNT >= 0x0500
/*
 *  GetAvailDriverMemory
 *
 *  DDraw 'miscellaneous' callback returning the amount of free memory in driver's
 *  'private' heap
 */

DWORD WINAPI GetAvailDriverMemory (PDD_GETAVAILDRIVERMEMORYDATA  pDmd)
{
    PPDEV ppdev = (PPDEV)(pDmd->lpDD->dhpdev);

    ASSERTDD(ppdev != NULL,"Bad ppdev in GetAvailDriverMemory");

    pDmd->dwTotal = ppdev->ulTotalAvailVideoMemory;
    pDmd->dwFree  = TotalAvailVideoMemory(ppdev);
    pDmd->ddRVal = DD_OK;

    return DDHAL_DRIVER_HANDLED;
}


/*
 ** GetDriverInfo32
 *
 *  FILENAME: C:\win9x\display\mini\s3\S3_DD32.C
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: DirectDraw has had many compatability problems
 *               in the past, particularly from adding or modifying
 *               members of public structures.  GetDriverInfo is an extension
 *               architecture that intends to allow DirectDraw to
 *               continue evolving, while maintaining backward compatability.
 *               This function is passed a GUID which represents some DirectDraw
 *               extension.  If the driver recognises and supports this extension,
 *               it fills out the required data and returns.
 *  RETURNS:
 *
 */

DWORD WINAPI DdGetDriverInfo(struct _DD_GETDRIVERINFODATA *lpInput)
{
    DWORD dwSize;
    PDEV  *ppdev = lpInput->dhpdev;
    WORD Id;
    ULONG Revision;

    DPF("GetDriverInfo:");

#ifdef DEBUG
#define CHECKSIZE(x)                              \
        if (lpInput->dwExpectedSize != sizeof(x)) \
            DPF("GetDriverInfo: #x structure size mismatch");
#else
#define CHECKSIZE(x) ((void)0)
#endif

    lpInput->ddRVal = DDERR_CURRENTLYNOTAVAIL;

#if (_WIN32_WINNT >= 0x0500)
    // A Windows NT display driver indicates that it supports Direct3D
    // DX6 acceleration by responding to the GUID_D3DCallbacks3 query in
    // GetDriverInfo.

    if ( IsEqualIID(&lpInput->guidInfo, &GUID_D3DParseUnknownCommandCallback))
    {
        DPF("Get D3DParseUnknownCommandCallback");

        ppdev->pD3DParseUnknownCommand =  lpInput->lpvData;

        ASSERTDD((ppdev->pD3DParseUnknownCommand),
                 "D3D DX6 ParseUnknownCommand callback == NULL");

        lpInput->ddRVal = DD_OK;
    }
    else
    if ((ppdev->ulCaps & CAPS_S3D_ENGINE) &&
            IsEqualIID(&lpInput->guidInfo, &GUID_D3DCallbacks3) )
    {
        D3DHAL_CALLBACKS3 D3DCallbacks3;
        memset(&D3DCallbacks3, 0, sizeof(D3DCallbacks3));

        DPF("Get D3D Callbacks3");
        dwSize = min(lpInput->dwExpectedSize, sizeof(D3DHAL_CALLBACKS3));
        lpInput->dwActualSize = sizeof(D3DHAL_CALLBACKS3);
        CHECKSIZE(D3DHAL_CALLBACKS3);

        D3DCallbacks3.dwSize = dwSize;
        D3DCallbacks3.dwFlags = D3DHAL3_CB32_VALIDATETEXTURESTAGESTATE  |
                                D3DHAL3_CB32_DRAWPRIMITIVES2;


        D3DCallbacks3.Clear2                    = NULL;
        D3DCallbacks3.lpvReserved               = NULL;
        D3DCallbacks3.ValidateTextureStageState = D3DValidateTextureStageState;
        D3DCallbacks3.DrawPrimitives2           = D3DDrawPrimitives2;

        memcpy(lpInput->lpvData, &D3DCallbacks3, dwSize);
        lpInput->ddRVal = DD_OK;
    }
    else
        if ((ppdev->ulCaps & CAPS_S3D_ENGINE) &&
           IsEqualIID(&lpInput->guidInfo, &GUID_D3DExtendedCaps) )
    {
        D3DNTHAL_D3DEXTENDEDCAPS D3DExtendedCaps;

        DPF("Get D3D Extended caps");
        memset(&D3DExtendedCaps, 0, sizeof(D3DExtendedCaps));
        dwSize = min(lpInput->dwExpectedSize, sizeof(D3DNTHAL_D3DEXTENDEDCAPS));

        lpInput->dwActualSize = sizeof(D3DNTHAL_D3DEXTENDEDCAPS);
        D3DExtendedCaps.dwSize = sizeof(D3DNTHAL_D3DEXTENDEDCAPS);

        D3DExtendedCaps.dwFVFCaps = 1;

        D3DExtendedCaps.dwMinTextureWidth  = 1;
        D3DExtendedCaps.dwMinTextureHeight = 1;

        if (S3GetDeviceRevision(ppdev,&Id ,&Revision) == DD_OK)
        {
            if ((Id == D_S3VIRGE) || (Id == D_S3VIRGEVX))
            {
                D3DExtendedCaps.dwMaxTextureWidth  = 128;
                D3DExtendedCaps.dwMaxTextureHeight = 128;
                D3DExtendedCaps.dwMaxTextureRepeat = 128;
            } else
            {
                // any other chip than the plain Virge and the VX can render
                // large ranges of texels
                D3DExtendedCaps.dwMaxTextureWidth  = 2048;
                D3DExtendedCaps.dwMaxTextureHeight = 2048;
                D3DExtendedCaps.dwMaxTextureRepeat = 2048;
            }
        }
        else
        {
            D3DExtendedCaps.dwMaxTextureWidth  = 128;
            D3DExtendedCaps.dwMaxTextureHeight = 128;
            D3DExtendedCaps.dwMaxTextureRepeat = 128;
        }

        // Fix texture sizes if running on the DX or GX
//      if ( (D3DGLOBALPTR(pCtxt)->wDeviceId !=  D_S3VIRGE) && 
//           (D3DGLOBALPTR(pCtxt)->wDeviceId !=  D_S3VIRGEVX)  )
//      {
//          D3DExtendedCaps.dwMaxTextureWidth  = 2048;
//          D3DExtendedCaps.dwMaxTextureHeight = 2048;
//      }

        // Set supported stipple min/max dimensions.
//     D3DExtendedCaps.dwMinStippleWidth  = 0;
//     D3DExtendedCaps.dwMaxStippleWidth  = 0;
//     D3DExtendedCaps.dwMinStippleHeight = 0;
//     D3DExtendedCaps.dwMaxStippleHeight = 0;

        D3DExtendedCaps.dwTextureOpCaps =
            D3DTEXOPCAPS_DISABLE                   |
            D3DTEXOPCAPS_SELECTARG1                |
            D3DTEXOPCAPS_SELECTARG2                |
            D3DTEXOPCAPS_MODULATE                  |
//          D3DTEXOPCAPS_MODULATE2X                |
//          D3DTEXOPCAPS_MODULATE4X                |
            D3DTEXOPCAPS_ADD                       |
//          D3DTEXOPCAPS_ADDSIGNED                 |
//          D3DTEXOPCAPS_ADDSIGNED2X               |
//          D3DTEXOPCAPS_SUBTRACT                  |
//          D3DTEXOPCAPS_ADDSMOOTH                 |
//          D3DTEXOPCAPS_BLENDDIFFUSEALPHA         |
            D3DTEXOPCAPS_BLENDTEXTUREALPHA         |
//          D3DTEXOPCAPS_BLENDFACTORALPHA          |
//          D3DTEXOPCAPS_BLENDTEXTUREALPHAPM       |
//          D3DTEXOPCAPS_BLENDCURRENTALPHA         |
//          D3DTEXOPCAPS_PREMODULATE               |
//          D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR    |
//          D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA    |
//          D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR |
//          D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA |
            0;

        D3DExtendedCaps.wMaxTextureBlendStages = 1;
        D3DExtendedCaps.wMaxSimultaneousTextures = 1;

        memcpy(lpInput->lpvData, &D3DExtendedCaps, dwSize);
        lpInput->ddRVal = DD_OK;
    }
    else
        if ((ppdev->ulCaps & CAPS_S3D_ENGINE) &&
           IsEqualIID(&lpInput->guidInfo, &GUID_ZPixelFormats) )
    {
        DDPIXELFORMAT ddZBufPixelFormat;
        DWORD         dwNumZPixelFormats;

        DPF("Get Z Pixel Formats");
        memset(&ddZBufPixelFormat, 0, sizeof(ddZBufPixelFormat));
        dwSize = min(lpInput->dwExpectedSize, sizeof(DDPIXELFORMAT));

        lpInput->dwActualSize = sizeof(DDPIXELFORMAT) + sizeof(DWORD);

        // We only fill one 16-bit Z Buffer format since that is all
        // what the Virge supports. Drivers have to report here all
        // Z Buffer formats supported only if they support the Clear2
        // callback. We implement it here for illustration purposes.

        dwNumZPixelFormats = 1;

        ddZBufPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
        ddZBufPixelFormat.dwFlags = DDPF_ZBUFFER;
        ddZBufPixelFormat.dwFourCC = 0;
        ddZBufPixelFormat.dwZBufferBitDepth = 16;
        ddZBufPixelFormat.dwStencilBitDepth = 0;
        ddZBufPixelFormat.dwZBitMask = 0xFFFF;
        ddZBufPixelFormat.dwStencilBitMask = 0x0000;
        ddZBufPixelFormat.dwRGBZBitMask = 0;

        memcpy(lpInput->lpvData, &dwNumZPixelFormats, sizeof(DWORD));
        memcpy((LPVOID)((LPBYTE)(lpInput->lpvData) + sizeof(DWORD)),
               &ddZBufPixelFormat, dwSize);

        lpInput->ddRVal = DD_OK;
    }
    else
#endif //(_WIN32_WINNT >= 0x0500)
    if ( IsEqualIID(&lpInput->guidInfo, &GUID_MiscellaneousCallbacks) )
    {
        DD_MISCELLANEOUSCALLBACKS MiscellaneousCallbacks;

        memset( &MiscellaneousCallbacks,
                0,
                sizeof( MiscellaneousCallbacks ) );

        DPF("Get Miscelaneous Callbacks");

        dwSize = min( lpInput->dwExpectedSize,
                      sizeof( DD_MISCELLANEOUSCALLBACKS ) );

        MiscellaneousCallbacks.dwSize  = dwSize;
        MiscellaneousCallbacks.dwFlags = DDHAL_MISCCB32_GETAVAILDRIVERMEMORY;
        MiscellaneousCallbacks.GetAvailDriverMemory = GetAvailDriverMemory;
        memcpy( lpInput->lpvData,
                &MiscellaneousCallbacks,
                dwSize );
        lpInput->ddRVal = DD_OK;
    }
    else
    if (IsEqualIID(&(lpInput->guidInfo), &GUID_Miscellaneous2Callbacks) )
    {
        DDHAL_DDMISCELLANEOUS2CALLBACKS MISC2_CB;

        DPF("  GUID_Miscellaneous2Callbacks2");

        memset(&MISC2_CB, 0, sizeof(DDHAL_DDMISCELLANEOUS2CALLBACKS));
        MISC2_CB.dwSize = sizeof(DDHAL_DDMISCELLANEOUS2CALLBACKS);

        MISC2_CB.dwFlags  = 0
            | DDHAL_MISC2CB32_CREATESURFACEEX
            | DDHAL_MISC2CB32_GETDRIVERSTATE
            | DDHAL_MISC2CB32_DESTROYDDLOCAL;

        MISC2_CB.GetDriverState = D3DGetDriverState;
        MISC2_CB.CreateSurfaceEx = D3DCreateSurfaceEx;
        MISC2_CB.DestroyDDLocal = D3DDestroyDDLocal;

        lpInput->dwActualSize = sizeof(MISC2_CB);
        dwSize = min(sizeof(MISC2_CB),lpInput->dwExpectedSize);
        memcpy(lpInput->lpvData, &MISC2_CB, dwSize);
        lpInput->ddRVal = DD_OK;
    } else

    if (ppdev->ulCaps & CAPS_STREAMS_CAPABLE)
    {
        if (IsEqualIID(&lpInput->guidInfo, &GUID_VideoPortCallbacks))
        {
            dwSize = min(lpInput->dwExpectedSize,
                         sizeof(DDHAL_DDVIDEOPORTCALLBACKS));

            lpInput->dwActualSize = dwSize;
            lpInput->ddRVal = DD_OK;

            memcpy(lpInput->lpvData, &LPBVideoPortCallbacks, dwSize);
        }

        if (IsEqualIID(&lpInput->guidInfo, &GUID_VideoPortCaps))
        {
            dwSize = min(lpInput->dwExpectedSize,
                         sizeof(DDVIDEOPORTCAPS));

            lpInput->dwActualSize = dwSize;
            lpInput->ddRVal = DD_OK;

            // !!! Has to be inited first!

            memcpy(lpInput->lpvData, &ppdev->LPBData.ddVideoPortCaps,
                   dwSize);
        }

        if (IsEqualIID(&lpInput->guidInfo, &GUID_KernelCallbacks))
        {
            dwSize = min(lpInput->dwExpectedSize,
                         sizeof(DDHAL_DDKERNELCALLBACKS));

            lpInput->dwActualSize = dwSize;
            lpInput->ddRVal = DD_OK;

            // !!! Has to be inited first!
            memcpy(lpInput->lpvData, &KernelCallbacks, dwSize);
        }

        if (IsEqualIID(&lpInput->guidInfo, &GUID_KernelCaps))
        {
             dwSize = min(lpInput->dwExpectedSize,
                          sizeof(DDKERNELCAPS));

             lpInput->dwActualSize = dwSize;
             lpInput->ddRVal = DD_OK;

             // !!! Has to be inited first!

             memcpy(lpInput->lpvData, &KernelCaps, dwSize);
        }
    }

    return DDHAL_DRIVER_HANDLED;
}

/*

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

*/

void  Bandwidth_SetSecondary_NT(PPDEV ppdev, PBAND pb, WORD device, SHORT bpp,
      SHORT xSrc, SHORT ySrc, SHORT xTrg, SHORT yTrg, SHORT xMax, SHORT yMax)
{
        BANDINF BandInf;
        BAND    Band;
    DWORD   ReturnedValue;
    DWORD   ReturnedDataLength;

        BandInf.Option = BANDWIDTH_SETSECONDARY;
        BandInf.pBand = pb;
        BandInf.device = device;
        BandInf.bpp = bpp;
        BandInf.xSrc = xSrc;
        BandInf.ySrc = ySrc;
        BandInf.xTrg = xTrg;
        BandInf.yTrg = yTrg;
        BandInf.xMax = xMax;
        BandInf.yMax = yMax;
        BandInf.RefreshRate  = (short) ppdev->ulRefreshRate;
    pb->GRefreshRate2    = (short) ppdev->ulRefreshRate2;

    ReturnedValue = EngDeviceIoControl(ppdev->hDriver,
                                               IOCTL_VIDEO_S3_QUERY_BANDWIDTH,
                                               &BandInf,
                                               sizeof(BandInf),
                                               &Band,
                                               sizeof(Band),
                                               &ReturnedDataLength);

    ASSERTDD(!ReturnedValue, "Unexpected failure from QUERY_BANDWIDTH");
}

void  Bandwidth_Exit(PBAND pb);

#endif // _WIN32_WINNT >= 0x0500

