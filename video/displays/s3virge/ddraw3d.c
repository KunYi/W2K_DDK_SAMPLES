/******************************Module*Header*******************************\
* Module Name: DDraw3D.c
*
* Implements all the DirectDraw components for the driver.
*
* Copyright (c) 1995-1996 Microsoft Corporation
\**************************************************************************/


#include "precomp.h"
#include "hw3D.h"
#include <mpc.h>

#define DDRAW_DEBUG_LEVEL 4             // -BBD
#define CCLKS_BLANKFIX  6
#define RoundDown(x,y) ((int)(x/y) * y)        // round x down to nearest y

USHORT GetMCLK(PDEV* ppdev)
{
     USHORT m, n, r;
     UINT   mclock;
     USHORT result;
     ULONG  lRefFreq;

     lRefFreq = GetRefFreq(ppdev);

     ACQUIRE_CRTC_CRITICAL_SECTION( ppdev );

     n = ReadSrReg( ppdev, 0x10);
     m = ReadSrReg( ppdev, 0x11) & 0x7f;

     RELEASE_CRTC_CRITICAL_SECTION( ppdev );

     r = (n >> 5) & 0x03;

     if(r==0)
        r=1;
     else
        r = 2<<(r-1);


     n = n & 0x1f;
     mclock = ((UINT)(m+2)*lRefFreq)/((UINT)((n+2)*r)*100L);
     result = (USHORT)(mclock/10L);
     if( (mclock % 10) >= 7 )
        result++;

     return(result);
}

USHORT GetDclksDuringBlank(PDEV* ppdev)
{
    USHORT cclkBlankStart;
    USHORT cclkBlankEnd;
    USHORT cclksDuringBlank;
    USHORT cclkDelay;
    BYTE  cr5d;
    BYTE  bDisplayState;

    ACQUIRE_CRTC_CRITICAL_SECTION( ppdev );

    VGAOUTPW( ppdev, SEQ_INDEX,  SEQ_REG_UNLOCK );

    if ( ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE )
    {

        //
        //  Whenever we set SR26 to IGA2-only access, we risk
        //  interruption (and loss of state) by an SMI event (hotkey
        //  display switch or suspend).  Don't bother with a
        //  RMW of SR26, since the read may be unreliable.
        //

        //  Get up-to-date DuoView status.
        VGAOUTP( ppdev, CRTC_INDEX, EXT_BIOS_FLAG3 );
        bDisplayState = VGAINP( ppdev, CRTC_DATA );
        if ( ( ppdev->Band.DuoViewFlags & DVW_SP_IGA1 ) ||
             (!(bDisplayState & M5_DUOVIEW)) )
        {
            VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA1 );
            cr5d             = ReadCrReg( ppdev, 0x5d);
            cclkBlankStart   = ReadCrReg( ppdev, 0x02);
            cclkBlankEnd     = ReadCrReg( ppdev, 0x03) & 0x1F;
            cclkBlankEnd    += (ReadCrReg( ppdev, 0x05) & 0x80) >> 1;
            cclkDelay        = ( ReadCrReg(ppdev, 0x85) & 0x07 );
        }
        else    // not DuoView or at least Streams on IGA1
        {
            VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA2_READS_WRITES );
            cr5d             = bReadCrDataWithShadow( ppdev, 0x5d);
            cclkBlankStart   = bReadCrDataWithShadow( ppdev, 0x02);
            cclkBlankEnd     = bReadCrDataWithShadow( ppdev, 0x03) & 0x1F;
            cclkBlankEnd    += (bReadCrDataWithShadow( ppdev, 0x05) & 0x80) >> 1;
            cclkDelay        = ( bReadCrDataWithShadow(ppdev, 0x85) & 0x07 );
            VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA1 );
        }
    }


//NOLOCK   VGAOUTPW( ppdev, SEQ_INDEX,  SEQ_REG_LOCK );

   RELEASE_CRTC_CRITICAL_SECTION( ppdev );

   cclkBlankStart += (cr5d & 0x04) << 6;
   cclkBlankStart &= 0x7F;

   cclkBlankEnd += (cr5d & 0x08) << 3;

   if (cclkBlankEnd < cclkBlankStart)
   {
      cclksDuringBlank = (512 - cclkBlankStart) + cclkBlankEnd;
   }
   else
   {
      cclksDuringBlank = (cclkBlankEnd - cclkBlankStart);
   }

   cclkDelay += CCLKS_BLANKFIX;

   cclksDuringBlank -= cclkDelay;

   return(cclksDuringBlank*8);
}
USHORT GetDCLK(PDEV* ppdev)
{
    USHORT  m, n, r;
    UINT    dclock;
    USHORT  result;
    ULONG   lRefFreq;

    lRefFreq = GetRefFreq(ppdev);

    ACQUIRE_CRTC_CRITICAL_SECTION( ppdev );

    n = (USHORT)ReadSrReg( ppdev, DCLK_VALUE_LOW_REG );
    m = (USHORT)ReadSrReg( ppdev, DCLK_VALUE_HIGH_REG ) & 0x7f;


    if ( ppdev->Band.id == 0x883D )
    {
        //
        // ViRGE/vX
        //

        r = (n >> 5) & 0x07;
    }
    else if ( ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE )
    {
        r = (n >> 6) & 0x03;
        r |= (USHORT)((ReadSrReg( ppdev, DCLK_PLL_OVERFLOW ) &
               DCLK1_PLL_R_VALUE_BIT2) << 2);
    }
    else
    {
        r = (n >> 5) & 0x03;
    }

    RELEASE_CRTC_CRITICAL_SECTION( ppdev );

    if (r==0)
    {
       r=1;
    }
    else
    {
       r = 2<<(r-1);
    }

    n = n & 0x1f;

    dclock = ((UINT)(m+2)*lRefFreq)/((UINT)((n+2)*r)*100L);

    result = (USHORT)(dclock/10L);

    if( (dclock % 10) >= 7 )
       result++;

    return(result);
}


USHORT GetDCLK2( PDEV * ppdev )
{
    USHORT  m, n, r;
    UINT    dclock;
    ULONG   lRefFreq;
    ULONG   lDclock;

    //
    //  Code currently valid only for M3/M5 and GX2...
    //
    if (! (ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE) )
    {
        return 0;
    }

    lRefFreq = GetRefFreq(ppdev);

    ACQUIRE_CRTC_CRITICAL_SECTION( ppdev );

    n = (USHORT)ReadSrReg( ppdev, DCLK2_VALUE_LOW_REG);

    r = (n >> 6) & 0x03;
    r |= (USHORT)(ReadSrReg( ppdev, DCLK_PLL_OVERFLOW ) &
        DCLK2_PLL_R_VALUE_BIT2);

    n = n & 0x3f;
    n |= (USHORT)((ReadSrReg( ppdev, DCLK_PLL_OVERFLOW ) &
        DCLK2_PLL_N_VALUE_BIT6) << 2);

    m = ((USHORT)(ReadSrReg( ppdev, DCLK_PLL_OVERFLOW ) &
        DCLK2_PLL_M_VALUE_BIT8) << 5);
    m |= (USHORT)ReadSrReg( ppdev, DCLK2_VALUE_HIGH_REG ) ;

    RELEASE_CRTC_CRITICAL_SECTION( ppdev );

    if (r == 0)
    {
        r = 1;
    }
    else
    {
        r = 2 << (r - 1);
    }


    lDclock = ((UINT)(m+2)*lRefFreq)/((UINT)((n+2)*r)*100L);

    return( (USHORT) (lDclock / 10) );
}


/******************************Public*Routine******************************\
* VOID vTurnOnStreamsProcessorMode
*
*       Note that the registers used in this routine are for ViRGE and
*       ViRGE-VX, but aren't all valid for ViRGE-DX/GX, ViRGE-GX2.
*
\**************************************************************************/

VOID vTurnOnStreamsProcessorMode(
PDEV*   ppdev)
{
    BYTE*   pjMmBase;
    BYTE    jStreamsProcessorModeSelect;
    DWORD   dwPFormat;
    unsigned char   StreamsControl;
    ULONG   ulStreamsTimeout;

    ASSERTDD(ppdev->ulCaps & CAPS_STREAMS_CAPABLE, "Must be streams capable");

    DISPDBG((DDRAW_DEBUG_LEVEL, "vTurnOnStreamsProcessorMode" ));

    ASSERTDD(ppdev->ulCaps & CAPS_S3D_ENGINE, "Must be streams capable");

    WAIT_DMA_IDLE(ppdev);       // wait till DMA is idle.
    TRIANGLE_WORKAROUND( ppdev );

    pjMmBase = ppdev->pjMmBase;

    S3DGPWait(ppdev);

    switch(ppdev->iBitmapFormat)
    {
    case BMF_8BPP:
        if ( ppdev->Band.id == 0x883D && ppdev->cxScreen == 320 )
        {
            dwPFormat = P_2x;
        }
        else
        {
            dwPFormat = P_RGB8;
        }
        break;

    case BMF_16BPP:
        if (IS_RGB15_R(ppdev->flRed))
        {
            dwPFormat = P_RGB15;
        }
        else
        {
            if ( ppdev->Band.id == 0x883D && ppdev->cyScreen == 240 )
            {
                dwPFormat = P_2x | P_RGB16;
            }
            else
            {
                dwPFormat = P_RGB16;
            }
        }
        break;

    case BMF_24BPP:
        dwPFormat = P_RGB24;
        break;

    case BMF_32BPP:
        dwPFormat = P_RGB32;
        break;

    default:
        RIP("Unexpected bitmap format");
    }

    // Initialize all of the streams processor registers

    if ( ppdev->ulDefaultFIFO == 0xFFFFFFFF )
    {
        ppdev->ulDefaultStreamsTimeout  =
            READ_REGISTER_ULONG( (BYTE *) ( pjMmBase + STREAMS_TIMEOUT ) );

        ppdev->ulDefaultMiscTimeout     =
            READ_REGISTER_ULONG( (BYTE *) ( pjMmBase + MISC_TIMEOUT ) );

        ppdev->ulDefaultMIUControl      =
            READ_REGISTER_ULONG( (BYTE *) ( pjMmBase + MIU_CONTROL ) );

        ppdev->ulDefaultSPFifo          =
            READ_REGISTER_ULONG( (BYTE *) ( pjMmBase + STREAMS_FIFO_CONTROL ) );
    }

    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    //
    // unlock (but do not leave CR38 == 0xA5)
    //

    VGAOUTPW(ppdev, CRTC_INDEX, ((REG_UNLOCK_1 << 8) | EXTREG_LOCK1));
    VGAOUTPW(ppdev, CRTC_INDEX, ((SYSCTL_UNLOCK << 8) | EXTREG_LOCK2));

    VGAOUTP( ppdev, CRTC_INDEX, EXT_MISC_CTRL2 );
    StreamsControl  = VGAINP( ppdev, CRTC_DATA );

    if ( ( StreamsControl & FULL_STREAMS ) != FULL_STREAMS )
    {
        LONG    fHeight    = (ppdev->cyScreen < 400) ? (ppdev->cyScreen * 2):
                                                       (ppdev->cyScreen);
        LONG    fWidth      = ppdev->cxScreen;

        // Enable streams
        StreamsControl  |= FULL_STREAMS;

        //
        // virge/vx hang
        //

        if ( ppdev->Band.id == 0x883D )
        {

            //
            // set fast page
            //

            VGAOUTP( ppdev, CRTC_INDEX, CONFIG_REG1_REG );
            ppdev->bCR36 = VGAINP( ppdev, CRTC_DATA );
            VGAOUTP( ppdev, CRTC_DATA, ppdev->bCR36 | FASTPAGE_MASK );
            if ( ppdev->cxScreen < 640 )
            {
                fWidth = fWidth * 2;
            }
        }

        VGAOUTP( ppdev, CRTC_INDEX, EXT_MISC_CTRL2 );

        // NAC - streams needs to turned on/off during VBLANK.
        while( VBLANK_IS_ACTIVE( ppdev ) );

        while( !VBLANK_IS_ACTIVE( ppdev ) );

        VGAOUTP( ppdev,
                 CRTC_DATA,
                 StreamsControl );

        WRITE_STREAM_D( pjMmBase,
                        PRI_STREAM_WINDOW_START,
                        0x00010001 );

        WRITE_STREAM_D( pjMmBase,
                        PRI_STREAM_WINDOW_SZ,
                        WH( fWidth, fHeight) );

        WRITE_STREAM_D( pjMmBase,
                        K1_VERT_SCALE,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        K2_VERT_SCALE,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        SEC_STREAM_STRETCH_FLTR,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        COLOR_CHROMA_CONTROL,
                        0x10000000 );

        WRITE_STREAM_D( pjMmBase,
                        CHROMA_UPPER_BOUND,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        SEC_STREAM_CONTROL,
                        0x06000000 );

        WRITE_STREAM_D( pjMmBase,
                        DBL_BUFFER_LPB_SUPPORT,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        DDA_VERT_ACCUM_INIT,
                        0x00000000 );

        ppdev->ulDefaultFIFO = 0x10400;

        WRITE_STREAM_D( pjMmBase,
                        PRI_STREAM_FBUF_ADDR0,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        PRI_STREAM_FBUF_ADDR1,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        PRI_STREAM_STRIDE,
                        ppdev->lDelta );

        WRITE_STREAM_D( pjMmBase,
                        BLEND_CONTROL,
                        POnS );

        WRITE_STREAM_D( pjMmBase,
                        OPAQUE_OVERLAY_CONTROL,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        SEC_STREAM_FBUF_ADDR0,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        SEC_STREAM_FBUF_ADDR1,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        SEC_STREAM_WINDOW_START,
                        0xffffffff );

        WRITE_STREAM_D( pjMmBase,
                        SEC_STREAM_WINDOW_SZ,
                        WH(10, 2) );
    }
    else
    {
        ppdev->ulDefaultFIFO = (ppdev->ulFifoValue & 0x0000001F)        |
                               ((ppdev->ulFifoValue & 0x000003E0) << 1) |
                               ((ppdev->ulFifoValue & 0x00007C00) << 2);
    }

    // select a known, harmless index into the address register
    VGAOUTP( ppdev,
             CRTC_INDEX,
             0x30 );

    WRITE_STREAM_D( pjMmBase,
                    PRI_STREAM_CONTROL,
                    dwPFormat );

    WRITE_STREAM_D( pjMmBase,
                    MIU_CONTROL,
                    ( ppdev->ulDefaultMIUControl & 0xFFFFF870 ) |
                    ( (ppdev->ulFifoValue & 0x8000) >> 13 ) );

    WRITE_STREAM_D( pjMmBase,
                    FIFO_CONTROL,
                    ppdev->ulDefaultFIFO );

    if ( ppdev->Band.id == 0x883D )
    {
        ulStreamsTimeout = 0x2008;
    }
    else if ( ppdev->Band.id == 0x5631 )
    {
        ulStreamsTimeout = 0x300E;
    }
    else
    {
        ulStreamsTimeout = 0x3010;
    }

    WRITE_STREAM_D( pjMmBase,
                    STREAMS_TIMEOUT,
                    ulStreamsTimeout );

    WRITE_STREAM_D( pjMmBase,
                    MISC_TIMEOUT,
                    0x08080804L );

    //
    // Leave CR39 unlocked, but don't leave = to 0xA5, or VGA compatibility
    // (in CPU latch and Attribute registers: CR22, CR24/CR26) will be broken.
    //

    VGAOUTPW(ppdev, CRTC_INDEX, ((SYSCTL_UNLOCK << 8) | EXTREG_LOCK2));


    RELEASE_CRTC_CRITICAL_SECTION(ppdev);

    //Update status flags...
    ppdev->flStatus |= STAT_STREAMS_ENABLED;
    //Mark stream as NOT utilized until call made to ddUpdateOverlay.
    ppdev->flStatus &= ~STAT_STREAMS_UTILIZED;

    DISPDBG((DDRAW_DEBUG_LEVEL, "Exit: vTurnOnStreamsProcessorMode" ));
}


/******************************Public*Routine******************************\
* VOID vTurnOffStreamsProcessorMode
*
\**************************************************************************/

VOID vTurnOffStreamsProcessorMode(
PDEV*   ppdev)
{
    BYTE*   pjMmBase;
    BYTE    jStreamsProcessorModeSelect;

    DISPDBG((DDRAW_DEBUG_LEVEL, "vTurnOffStreamsProcessorMode" ));

    WAIT_DMA_IDLE(ppdev);       // wait till DMA is idle.
    TRIANGLE_WORKAROUND( ppdev );

    pjMmBase = ppdev->pjMmBase;

    S3DGPWait( ppdev );

    if ( ppdev->ulDefaultFIFO != 0xFFFFFFFF )
    {
        WRITE_STREAM_D( pjMmBase, FIFO_CONTROL, ppdev->ulDefaultFIFO );

        WRITE_STREAM_D( pjMmBase,
                        STREAMS_TIMEOUT,
                        ppdev->ulDefaultStreamsTimeout );

        WRITE_STREAM_D( pjMmBase, MISC_TIMEOUT, ppdev->ulDefaultMiscTimeout );

        WRITE_STREAM_D( pjMmBase, MIU_CONTROL, ppdev->ulDefaultMIUControl );

        WRITE_STREAM_D( pjMmBase, STREAMS_FIFO_CONTROL, ppdev->ulDefaultSPFifo );

        ppdev->ulDefaultFIFO            = 0xFFFFFFFF;
        ppdev->ulDefaultStreamsTimeout  = 0xFFFFFFFF;
        ppdev->ulDefaultMiscTimeout     = 0xFFFFFFFF;
        ppdev->ulDefaultMIUControl      = 0xFFFFFFFF;
        ppdev->ulDefaultSPFifo          = 0xFFFFFFFF;
    }


    if ( !(ppdev->ulCaps2 & CAPS2_24BPP_NEEDS_STREAMS)   ||
          (ppdev->ulCaps2 & CAPS2_24BPP_NEEDS_STREAMS &&
           ppdev->cBitsPerPel != 24) )
    {
        ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

        VGAOUTP( ppdev, CRTC_INDEX, EXT_MISC_CTRL2 );

        jStreamsProcessorModeSelect = VGAINP( ppdev, CRTC_DATA ) &
                                      NO_STREAMS;

        if ( ppdev->Band.id == 0x883D )
        {
            WRITE_STREAM_D( pjMmBase, PRI_STREAM_CONTROL, 0x00000000 );
        }

        while( VBLANK_IS_ACTIVE( ppdev ) );

        while( !VBLANK_IS_ACTIVE( ppdev ) );

        VGAOUTP( ppdev, CRTC_DATA, jStreamsProcessorModeSelect );

        //
        // ViRGE/vX hang
        //

        if ( ppdev->Band.id == 0x883D )
        {
            VGAOUTP( ppdev, CRTC_INDEX, CONFIG_REG1_REG );
            VGAOUTP( ppdev, CRTC_DATA, ppdev->bCR36 );

        }
        RELEASE_CRTC_CRITICAL_SECTION(ppdev);
    }

    //Update status flags...
    ppdev->flStatus &= ~STAT_STREAMS_ENABLED;
    ppdev->flStatus &= ~STAT_STREAMS_UTILIZED;

    DISPDBG((DDRAW_DEBUG_LEVEL, "Exit: vTurnOffStreamsProcessorMode" ));
}

/******************************Public*Routine******************************\
* VOID vTurnOnGX2StreamsProcessorMode
*
*       Note that the registers used in this routine are for ViRGE-GX2 and
*       Mother Products (M3/M5).
*
\**************************************************************************/

VOID vTurnOnGX2StreamsProcessorMode(
PDEV*   ppdev)
{
    BYTE*   pjMmBase;
    BYTE    jStreamsProcessorModeSelect;
    DWORD   dwPFormat;
    unsigned char   StreamsControl;
    ULONG   ulStreamsTimeout;

//
// This is needed to check CR6B for the current display state (fp, tv, etc.)
//

//    BYTE    bCurrentDisplay;

    ASSERTDD(ppdev->ulCaps & CAPS_STREAMS_CAPABLE, "Must be streams capable");

    DISPDBG((DDRAW_DEBUG_LEVEL, "vTurnOnStreamsProcessorMode" ));

    ASSERTDD(ppdev->ulCaps & CAPS_S3D_ENGINE, "Must be streams capable");

    WAIT_DMA_IDLE(ppdev);       // wait till DMA is idle.
    TRIANGLE_WORKAROUND( ppdev );

    pjMmBase = ppdev->pjMmBase;

    S3DGPWait( ppdev );

    switch(ppdev->iBitmapFormat)
    {
    case BMF_8BPP:
        if ( ppdev->Band.id == 0x883D && ppdev->cxScreen == 320 )
        {
            dwPFormat = P_2x;
        }
        else
        {
            dwPFormat = P_RGB8;
        }
        break;

    case BMF_16BPP:
        if (IS_RGB15_R(ppdev->flRed))
        {
            dwPFormat = P_RGB15;
        }
        else
        {
            if ( ppdev->Band.id == 0x883D && ppdev->cyScreen == 240 )
            {
                dwPFormat = P_2x | P_RGB16;
            }
            else
            {
                dwPFormat = P_RGB16;
            }
        }
        break;

    case BMF_24BPP:
        dwPFormat = P_RGB24;
        break;

    case BMF_32BPP:
        dwPFormat = P_RGB32;
        break;

    default:
        RIP("Unexpected bitmap format");
    }

    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    //
    // unlock (but do not leave CR39 = 0xA5!)
    //

    VGAOUTPW(ppdev, CRTC_INDEX, ((REG_UNLOCK_1 << 8) | EXTREG_LOCK1));
//    VGAOUTPW(ppdev, CRTC_INDEX, ((SYSCTL_UNLOCK_EXTRA << 8) | EXTREG_LOCK2));
    VGAOUTPW(ppdev, CRTC_INDEX, ((SYSCTL_UNLOCK << 8) | EXTREG_LOCK2));

    VGAOUTPW( ppdev, SEQ_INDEX, SEQ_REG_UNLOCK);

    //
    // read from IGA1
    //

    VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA1 );

    VGAOUTP( ppdev, CRTC_INDEX, EXT_MISC_CTRL2 );
    StreamsControl  = VGAINP( ppdev, CRTC_DATA );

    if ( ( StreamsControl & FULL_STREAMS ) != FULL_STREAMS )
    {
        LONG    fHeight    = (ppdev->cyScreen < 400) ? (ppdev->cyScreen * 2):
                                                       (ppdev->cyScreen);
        LONG    fWidth      = ppdev->cxScreen;

        // Enable streams
        StreamsControl  |= FULL_STREAMS;

        //
        // Workaround for SPR #18646.  Presently coded to fix a
        // problem in M5+ and MXC.  For Mi4 and later chips, the polarity of
        // CR67[0] is reversed and the bit should stay cleared.
        //
        if ( ppdev->ulCaps2 & CAPS2_VERT_COUNTER_WORKAROUND)
        {
            StreamsControl |= M5PLUS_DONT_WRITE_CTR26;
        }

        VGAOUTP( ppdev, CRTC_INDEX, EXT_MISC_CTRL2 );

        while( VBLANK_IS_ACTIVE( ppdev ) );

        while( !VBLANK_IS_ACTIVE( ppdev ) );

        VGAOUTP( ppdev,
                 CRTC_DATA,
                 StreamsControl );

        //
        // workaround
        //

        //VGAOUTP( ppdev, CRTC_INDEX, 0x26 );
        //VGAOUTP( ppdev, CRTC_DATA, 0xff );

        WRITE_STREAM_D( pjMmBase,
                        PRI_STREAM_FBUF_ADDR0,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        PRI_STREAM_FBUF_ADDR1,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        PRI_STREAM_STRIDE,
                        ppdev->lDelta );

        WRITE_STREAM_D( pjMmBase,
                        BLEND_CONTROL,
                        POnS );

        WRITE_STREAM_D( pjMmBase,
                        OPAQUE_OVERLAY_CONTROL,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        SEC_STREAM_FBUF_ADDR0,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        SEC_STREAM_FBUF_ADDR1,
                        0x00000000 );

        WRITE_STREAM_D( pjMmBase,
                        SEC_STREAM_WINDOW_START,
                        0xffffffff );

        WRITE_STREAM_D( pjMmBase,
                        SEC_STREAM_WINDOW_SZ,
                        WH(10, 2) );
    }

    //
    // select a known, harmless index into the address register
    //

    VGAOUTP( ppdev,
             CRTC_INDEX,
             0x30 );

    RELEASE_CRTC_CRITICAL_SECTION(ppdev);

    //
    // Update status flags...
    // Mark stream as NOT utilized until call made to ddUpdateOverlay.
    //

    ppdev->flStatus |= STAT_STREAMS_ENABLED;
    ppdev->flStatus &= ~STAT_STREAMS_UTILIZED;


    DISPDBG((DDRAW_DEBUG_LEVEL, "Exit: vTurnOnStreamsProcessorMode" ));
}


/******************************Public*Routine******************************\
* VOID vTurnOffGX2StreamsProcessorMode
*
\**************************************************************************/

VOID vTurnOffGX2StreamsProcessorMode(
PDEV*   ppdev)
{
    BYTE*   pjMmBase;
    BYTE    jStreamsProcessorModeSelect;

    DISPDBG((DDRAW_DEBUG_LEVEL, "vTurnOffStreamsProcessorMode" ));

    WAIT_DMA_IDLE(ppdev);       // wait till DMA is idle.
    TRIANGLE_WORKAROUND( ppdev );

    pjMmBase = ppdev->pjMmBase;

    S3DGPWait( ppdev );

    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    VGAOUTP( ppdev, CRTC_INDEX, EXT_MISC_CTRL2 );

    jStreamsProcessorModeSelect = VGAINP( ppdev, CRTC_DATA ) &
                                  NO_STREAMS;

    //
    // If we had to set CR67[0] to workaround SPR 18646, we should clear it
    // now.
    //
    if ( ppdev->ulCaps2 & CAPS2_VERT_COUNTER_WORKAROUND)
    {
        jStreamsProcessorModeSelect &= ~M5PLUS_DONT_WRITE_CTR26;
    }


    VGAOUTP( ppdev, CRTC_INDEX, GX2_SS_FIFO_THRESHOLD );

    while( VBLANK_IS_ACTIVE( ppdev ) );

    while( !VBLANK_IS_ACTIVE( ppdev ) );

    //
    // set secondary stream FIFO Threshold to 0
    //

    VGAOUTP( ppdev, CRTC_DATA, 0 );

    VGAOUTP( ppdev, CRTC_INDEX, EXT_MISC_CTRL2 );

    VGAOUTP( ppdev, CRTC_DATA, jStreamsProcessorModeSelect );

    RELEASE_CRTC_CRITICAL_SECTION(ppdev);

    ddSetStreamsFifo( ppdev, TRUE );

    //Update status flags...
    ppdev->flStatus &= ~STAT_STREAMS_ENABLED;
    ppdev->flStatus &= ~STAT_STREAMS_UTILIZED;


    DISPDBG((DDRAW_DEBUG_LEVEL, "Exit: vTurnOffStreamsProcessorMode" ));
}


/******************************Public*Routine******************************\
* DWORD DdBlt
*
\**************************************************************************/

DWORD DdBlt(
PDD_BLTDATA lpBlt)
{
    PDD_SURFACE_GLOBAL  srcSurf;
    PDD_SURFACE_LOCAL   dstSurfx;
    PDD_SURFACE_GLOBAL  dstSurf;
    PDEV*               ppdev;
    BYTE*               pjMmBase;
    HRESULT             ddrval;
    DWORD               dstX;
    DWORD               dstY;
    WORD                srcPitch;
    WORD                dstPitch;
    DWORD               dwFlags;
    DWORD               dstWidth;
    DWORD               dstHeight;
    DWORD               srcWidth;
    DWORD               srcHeight;
    DWORD               dwError;
    DWORD               srcX;
    DWORD               srcY;
    ULONG               ulBltCmd;
    DWORD               dwVEctrl;
    DWORD               dwVEdda;
    DWORD               dwVEcrop;
    DWORD               dwVEdstAddr;
    DWORD               dwVEsrcAddr;
    DWORD               dwDstByteCount;
    DWORD               dwSrcByteCount;
    DWORD               dwSrcBytes;
    DWORD               dwCropSkip;
    DWORD               sxf;
    DWORD               dxf;
    LONG                i;
    FLATPTR             fp;
    PDDRAWDATA          pDstData;
    PDDRAWDATA          pSrcData;
    DWORD               dwSrcValue;
    DWORD               dwDstValue;

    DISPDBG((4, "DdBlt3D" ));


    ppdev    = (PDEV*) lpBlt->lpDD->dhpdev;
    pjMmBase = ppdev->pjMmBase;

    dstSurfx    = lpBlt->lpDDDestSurface;
    dstSurf     = dstSurfx->lpGbl;
    pDstData    = (PDDRAWDATA) dstSurf->dwReserved1;
    dwDstValue  = pDstData ? pDstData->dwValue : 0;

    // Is a flip in progress?

    ddrval = ddrvalUpdateFlipStatus(ppdev, dstSurf->fpVidMem);
    if (ddrval != DD_OK)
    {
        lpBlt->ddRVal = ddrval;
        DISPDBG((4, "DdBlt3D: Exit 1: Flip in progress." ));
        return(DDHAL_DRIVER_HANDLED);
    }

    dwFlags = lpBlt->dwFlags;
    if (dwFlags & DDBLT_ASYNC)
    {
        // If async, then only work if we won't have to wait on the
        // accelerator to start the command.
        //
        // The FIFO wait should account for the worst-case possible
        // blt that we would do:

        // if (MM_FIFO_BUSY(ppdev, pjMmBase, DDBLT_FIFO_COUNT))
        if (S3DFifoBusy(ppdev, DDBLT_FIFO_COUNT))
        {
            lpBlt->ddRVal = DDERR_WASSTILLDRAWING;
            DISPDBG((4, "DdBlt3D: Exit 2: FIFO is busy running ASYNC." ));
            return(DDHAL_DRIVER_HANDLED);
        }
    }

    // Copy src/dst rects:

    dstX      = lpBlt->rDest.left;
    dstY      = lpBlt->rDest.top;
    dstWidth  = lpBlt->rDest.right - lpBlt->rDest.left;
    dstHeight = lpBlt->rDest.bottom - lpBlt->rDest.top;
    dstPitch  = (WORD) dstSurf->lPitch;

    DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: \tdstX == 0x%lx.\n\t\tdstY == 0x%lx.",dstX, dstY));
    DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: \tdstWidth == 0x%lx.\n\t\tdstHeight == 0x%lx.",dstWidth, dstHeight));
    DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: \tdstPitch == 0x%lx.",dstPitch));


#if _WIN32_WINNT >= 0x0500
    if ((dwFlags & DDBLT_COLORFILL) || (dwFlags & DDBLT_DEPTHFILL))
#else
    if (dwFlags & DDBLT_COLORFILL)
#endif
    {
      // The S3 can't easily do colour fills for off-screen surfaces that
      // are a different pixel format than that of the primary display:

#if _WIN32_WINNT >= 0x0500
        if (!(dwFlags & DDBLT_DEPTHFILL) &&
             (dwDstValue & DD_RESERVED_DIFFERENTPIXELFORMAT))
#else
        if (dwDstValue & DD_RESERVED_DIFFERENTPIXELFORMAT)
#endif
        {
            DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: Can't do colorfill to odd pixel format.\n"));
            return(DDHAL_DRIVER_NOTHANDLED);
        }
        else
        {
            DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: Doing a colorfill.\n"));

            WAIT_DMA_IDLE(ppdev);       // wait till DMA is idle.
            TRIANGLE_WORKAROUND( ppdev );

            // convertToGlobalCord(dstX, dstY, dstSurf);

            S3DFifoWait(ppdev, 7);

            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_PATTERN_FOREGROUND_COLOR,
                        lpBlt->bltFX.dwFillColor
                      );

            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_PATTERN_BACKGROUND_COLOR,
                        lpBlt->bltFX.dwFillColor
                      );

           // Set the base address for the destination
           NW_SET_REG( ppdev, pjMmBase,
                    S3D_BLT_DESTINATION_BASE,
                    (DWORD)(dstSurf->fpVidMem)
                  );

            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_DESTINATION_SOURCE_STRIDE,
                        ( (DWORD) ((dstPitch & 0xFFF8) << 16) |
                          (DWORD) (dstPitch & 0xFFF8) )
                      );

            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_WIDTH_HEIGHT,
                        ((DWORD)((dstWidth-1) << 16) | (DWORD) (dstHeight))
                      );

            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_DESTINATION_X_Y,
                        (dstX << 16) | dstY
                      );

            // Send the command to the accelerator.

            S3DGPWait(ppdev);

#if _WIN32_WINNT >= 0x0500
            NW_SET_REG( ppdev,
                        pjMmBase,
                        S3D_BLT_COMMAND,
                        (((dwFlags & DDBLT_DEPTHFILL) ? (1<<2) : ppdev->ulCommandBase) |
                        S3D_BITBLT                     |
                        S3D_MONOCHROME_PATTERN         |
                        (PAT_COPY << S3D_ROP_SHIFT)    |
                        S3D_X_POSITIVE_BLT             |
                        S3D_Y_POSITIVE_BLT             |
                        S3D_DRAW_ENABLE) );
#else
            NW_SET_REG( ppdev,
                        pjMmBase,
                        S3D_BLT_COMMAND,
                        (ppdev->ulCommandBase           |
                         S3D_BITBLT                     |
                         S3D_MONOCHROME_PATTERN         |
                         (PAT_COPY << S3D_ROP_SHIFT)    |
                         S3D_X_POSITIVE_BLT             |
                         S3D_Y_POSITIVE_BLT             |
                         S3D_DRAW_ENABLE) );
#endif

            lpBlt->ddRVal = DD_OK;
            DISPDBG((4, "DdBlt3D: Exit 3: Colorfill" ));
            return(DDHAL_DRIVER_HANDLED);
        }
    }

    // We specified with Our ddCaps.dwCaps that we handle a limited number
    // of commands, and by this point in our routine we've handled everything
    // except DDBLT_ROP.  DirectDraw and GDI shouldn't pass us anything
    // else; we'll assert on debug builds to prove this:

    ASSERTDD((dwFlags & DDBLT_ROP) && (lpBlt->lpDDSrcSurface),
        "Expected dwFlags commands of only DDBLT_ASYNC and DDBLT_COLORFILL");

    // Get offset, dstWidth, and dstHeight for source:

    srcSurf     = lpBlt->lpDDSrcSurface->lpGbl;
    pSrcData    = (PDDRAWDATA) srcSurf->dwReserved1;
    dwSrcValue  = pSrcData ? pSrcData->dwValue : 0;
    srcX        = lpBlt->rSrc.left;
    srcY        = lpBlt->rSrc.top;
    srcWidth    = lpBlt->rSrc.right - lpBlt->rSrc.left;
    srcHeight   = lpBlt->rSrc.bottom - lpBlt->rSrc.top;
    srcPitch    = (WORD) srcSurf->lPitch;

    
    DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: \tsrcX == 0x%lx.\n\t\tsrcY == 0x%lx.",srcX, srcY));
    DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: \tsrcWidth == 0x%lx.\n\t\tsrcHeight == 0x%lx.",srcWidth, srcHeight));
    DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: \tsrcPitch == 0x%lx.",srcPitch));

    // If a stretch or a funky pixel format blt are involved, we'll have to
    // defer to the overlay:

    if ( srcWidth  == dstWidth                              &&
         srcHeight == dstHeight                             &&
        !(dwSrcValue & DD_RESERVED_DIFFERENTPIXELFORMAT)    &&
        !(dwDstValue & DD_RESERVED_DIFFERENTPIXELFORMAT) )
    {
        // Assume we can do the blt top-to-bottom, left-to-right:

        ulBltCmd = ppdev->ulCommandBase |
                   S3D_X_POSITIVE_BLT   |
                   S3D_Y_POSITIVE_BLT;

        if ((dstSurf == srcSurf) && (srcX + dstWidth  > dstX) &&
            (srcY + dstHeight > dstY) && (dstX + dstWidth > srcX) &&
            (dstY + dstHeight > srcY) &&
            (((srcY == dstY) && (dstX > srcX) )
                 || ((srcY != dstY) && (dstY > srcY))))
        {
            // Okay, we have to do the blt bottom-to-top, right-to-left:

            srcX = lpBlt->rSrc.right - 1;
            srcY = lpBlt->rSrc.bottom - 1;
            dstX = lpBlt->rDest.right - 1;
            dstY = lpBlt->rDest.bottom - 1;

            ulBltCmd = ppdev->ulCommandBase |
                       S3D_X_NEGATIVE_BLT   |
                       S3D_Y_NEGATIVE_BLT;
            DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: BtoT."));
        }
        else
        {
            DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: TtoB."));
        }
            

        // NT only ever gives us SRCCOPY rops, so don't even both checking
        // for anything else.

        // convertToGlobalCord(srcX, srcY, srcSurf);
        // convertToGlobalCord(dstX, dstY, dstSurf);

        WAIT_DMA_IDLE(ppdev);       // wait till DMA is idle.
        TRIANGLE_WORKAROUND( ppdev );

        S3DFifoWait(ppdev, 6);

        NW_SET_REG( ppdev, pjMmBase,
                    S3D_BLT_SOURCE_X_Y,
                    (srcX << 16) | srcY
                  );

        NW_SET_REG( ppdev, pjMmBase,
                    S3D_BLT_DESTINATION_X_Y,
                    ((DWORD)(dstX << 16) | (DWORD) dstY)
                  );

        NW_SET_REG( ppdev, pjMmBase,
                    S3D_BLT_WIDTH_HEIGHT,
                    ((DWORD)((dstWidth-1) << 16) | (DWORD) (dstHeight))
                  );

        DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: \tSourceBase == 0x%lx.\n",srcSurf->fpVidMem));

        NW_SET_REG( ppdev, pjMmBase,
                    S3D_BLT_SOURCE_BASE,
                    (DWORD)(srcSurf->fpVidMem)
                  );

        DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: \tDestBase == 0x%lx.\n",dstSurf->fpVidMem));

        NW_SET_REG( ppdev, pjMmBase,
                    S3D_BLT_DESTINATION_BASE,
                    (DWORD)(dstSurf->fpVidMem)
                  );

        NW_SET_REG( ppdev, pjMmBase,
                    S3D_BLT_DESTINATION_SOURCE_STRIDE,
                    ( (DWORD) ((dstPitch & 0xFFF8) << 16) |
                      (DWORD) (srcPitch & 0xFFF8) )
                  );

        if (dwFlags & DDBLT_KEYSRCOVERRIDE)
        {
            DISPDBG((DDRAW_DEBUG_LEVEL,"DdBlt3D: Transparent BLT."));

            S3DFifoWait(ppdev, 2);

            if( ppdev->cjPelSize == 1 )
            {
                NW_SET_REG( ppdev,
                            pjMmBase,
                            S3D_BLT_SOURCE_FOREGROUND_COLOR,
                            ( (lpBlt->bltFX.ddckSrcColorkey.dwColorSpaceLowValue) |
                              ((lpBlt->bltFX.ddckSrcColorkey.dwColorSpaceLowValue) << 8) )
                          );
            }
            else
            {
                NW_SET_REG( ppdev,
                            pjMmBase,
                            S3D_BLT_SOURCE_FOREGROUND_COLOR,
                            lpBlt->bltFX.ddckSrcColorkey.dwColorSpaceLowValue );
            }

            //Re-enable Graphics port wait to fix SPR 16530.
            S3DGPWait(ppdev);

            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_COMMAND,
                        (ulBltCmd                       |
                         S3D_TRANSPARENT                |
                         S3D_BITBLT                     |
                         S3D_DRAW_ENABLE                |
                         (SRC_COPY << S3D_ROP_SHIFT))
                      );
        }
        else
        {
            DISPDBG((DDRAW_DEBUG_LEVEL,"DdBlt3D: SrcCopy."));

            S3DFifoWait(ppdev, 1);

            //Uncommented out when added fix for SPR 16530
            S3DGPWait(ppdev);

            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_COMMAND,
                        (ulBltCmd                       |
                         S3D_BITBLT                     |
                         S3D_DRAW_ENABLE                |
                         (SRC_COPY << S3D_ROP_SHIFT))
                      );
        }
    }
    else
    {
        //////////////////////////////////////////////////////////////////////
        // Overlay Blts
        //
        // Here we have to take care of cases where the destination is a
        // funky pixel format.

        ASSERTDD(ppdev->flStatus & STAT_STREAMS_ENABLED,
            "Expected only overlay calls down here");

        // In order to make ActiveMovie and DirectVideo work, we have
        // to support blting between funky pixel format surfaces of the
        // same type.  This is used to copy the current frame to the
        // next overlay surface in line.
        //
        // Unfortunately, it's not easy to switch the S3 graphics
        // processor out of its current pixel depth, so we'll only support
        // the minimal functionality required:

        if (!(dwFlags & DDBLT_ROP)                     ||
            (srcX != 0)                                ||
            (srcY != 0)                                ||
            (dstX != 0)                                ||
            (dstY != 0)                                ||
            (dstWidth  != dstSurf->wWidth)             ||
            (dstHeight != dstSurf->wHeight)            ||
            (dstSurf->lPitch != srcSurf->lPitch)       ||
            (dstSurf->ddpfSurface.dwRGBBitCount
                != srcSurf->ddpfSurface.dwRGBBitCount))
        {
            DISPDBG((DDRAW_DEBUG_LEVEL, "Sorry, we do only full-surface blts between same-type"));
            DISPDBG((DDRAW_DEBUG_LEVEL, "surfaces that have a funky pixel format."));
            return(DDHAL_DRIVER_NOTHANDLED);
        }
        else
        {
            //
            // wait till dma is idle, and execute the workaround for
            // triangles if applicable.
            //

            WAIT_DMA_IDLE(ppdev);
            TRIANGLE_WORKAROUND( ppdev );

            S3DGPWait(ppdev);
            DISPDBG((DDRAW_DEBUG_LEVEL,"DdBlt3D: Funky pixel format BLT."));

            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_WIDTH_HEIGHT,
                        ((dstWidth-1) << 16) | dstHeight );

            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_SOURCE_X_Y,
                        (srcX << 16) | srcY );

            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_DESTINATION_X_Y,
                        (dstX << 16) | dstY );

		    DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: \tSourceBase == 0x%lx.\n",srcSurf->fpVidMem));

            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_SOURCE_BASE,
                        (DWORD)(srcSurf->fpVidMem)
                      );

			DISPDBG((DDRAW_DEBUG_LEVEL, "DdBlt3D: \tDestBase == 0x%lx.\n",dstSurf->fpVidMem));
            
            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_DESTINATION_BASE,
                        (DWORD)(dstSurf->fpVidMem)
                      );

            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_DESTINATION_SOURCE_STRIDE,
                        ((dstPitch & 0xFFF8) << 16) |
                         (srcPitch & 0xFFF8) );

            ulBltCmd = (dstSurf->ddpfSurface.dwRGBBitCount >> 1) - 4;

            NW_SET_REG( ppdev, pjMmBase,
                        S3D_BLT_COMMAND,
                        (ulBltCmd                    |
                         S3D_BITBLT                  |
                         S3D_DWORD_ALIGNED           |
                         S3D_X_POSITIVE_BLT          |
                         S3D_Y_POSITIVE_BLT          |
                         S3D_DRAW_ENABLE             |
                         (SRC_COPY << S3D_ROP_SHIFT)) );
        }
    }

    lpBlt->ddRVal =DD_OK;
    DISPDBG((4, "DdBlt3D: Exit 4: BLT." ));
    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdUpdateOverlay
*
\**************************************************************************/

DWORD DdUpdateOverlay(
PDD_UPDATEOVERLAYDATA lpUpdateOverlay)
{
    PDEV*                           ppdev;
    BYTE*                           pjMmBase;
    DD_SURFACE_GLOBAL*              lpSource;
    DD_SURFACE_GLOBAL*              lpDestination;
    DD_SURFACE_GLOBAL*              lpColorKeySurface;
    DWORD                           dwFlags;
    DWORD                           dwGX2BlendControl;
    DWORD                           dwSecX, dwSecY;
    DWORD                           dwSecXOffset, dwSecYOffset;
    DWORD                           displayWidth, displayHeight;
    DWORD                           dwDisplaySize, dwPanelSize;
    UCHAR                           SrReg;
    DWORD                           dwDesktopSSX, dwDesktopSSWidth;
    DWORD                           dwStride;
    LONG                            srcWidth;
    LONG                            srcHeight;
    LONG                            dstWidth;
    LONG                            dstHeight;
    DWORD                           dwBitCount;
    DWORD                           dwStart;
    DWORD                           dwTmp;
    BOOL                            bColorKey;
    DWORD                           dwKeyLow;
    DWORD                           dwKeyHigh;
    DWORD                           dwBytesPerPixel;
    DWORD                           dwSecCtrl;
    DWORD                           dwBlendCtrl;
    DWORD                           ReturnedDataLength;
    BANDINF                         BandInf;
    DWORD                           dwVDDA;
    DWORD                           dwOpaqueCtrl;
    BOOL                            bDropUVcomponent = FALSE;


    DISPDBG((DDRAW_DEBUG_LEVEL, "DdUpdateOverlay" ));

    ppdev = (PDEV*) lpUpdateOverlay->lpDD->dhpdev;

// May have hook this call since code to turn on SP is now in DdCreateSurface.
//    ASSERTDD(ppdev->flStatus & STAT_STREAMS_ENABLED, "Shouldn't have hooked call");

    pjMmBase = ppdev->pjMmBase;

    dwOpaqueCtrl    = 0;
    bColorKey       = FALSE;
    dwBlendCtrl     = 0;
    dwKeyLow        = 0;
    dwKeyHigh       = 0;

    // 'Source' is the overlay surface, 'destination' is the surface to
    // be overlayed:

    lpSource = lpUpdateOverlay->lpDDSrcSurface->lpGbl;

    dwFlags = lpUpdateOverlay->dwFlags;
    if (dwFlags & DDOVER_HIDE)
    {
        if (lpSource->fpVidMem == ppdev->fpVisibleOverlay)
        {
            if ( ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE )
            {
                WRITE_STREAM_D( pjMmBase, BLEND_CONTROL, GX2_Kp_Mask );
            }
            else
            {
                WAIT_FOR_VBLANK(ppdev);
                WRITE_STREAM_D(pjMmBase, BLEND_CONTROL, POnS);
            }

            //
            // Set to 10x2 rectangle
            //

            WRITE_STREAM_D(pjMmBase, S_WH, WH(10, 2));

            //
            // disable opaque control
            //

            WRITE_STREAM_D(pjMmBase, OPAQUE_CONTROL, dwOpaqueCtrl);

            lpUpdateOverlay->lpDDSrcSurface->dwReserved1 &= ~OVERLAY_FLG_ENABLED;
            ppdev->fpVisibleOverlay = 0;

            //Flag status to indicate we are no longer utilizing the stream...
            ppdev->flStatus &= ~STAT_STREAMS_UTILIZED;

        }

        lpUpdateOverlay->ddRVal = DD_OK;
        return(DDHAL_DRIVER_HANDLED);
    }

    // If the user has disabled streams use for overlay, we must
    // kick it back to GDI here!
    if (ppdev->bHwOverlayOff == TRUE)
    {
        lpUpdateOverlay->ddRVal = DDERR_GENERIC;
        return (DDHAL_DRIVER_HANDLED);
    }

    //
    //  return an error if a request for both source AND destination
    //  color keying
    //

    if ((dwFlags & (DDOVER_KEYDEST | DDOVER_KEYDESTOVERRIDE))   &&
        ( dwFlags & (DDOVER_KEYSRC | DDOVER_KEYSRCOVERRIDE) ))
    {
        lpUpdateOverlay->ddRVal = DDERR_NOCOLORKEYHW;
        return (DDHAL_DRIVER_HANDLED);
    }

    // Dereference 'lpDDDestSurface' only after checking for the DDOVER_HIDE
    // case:

    lpDestination = lpUpdateOverlay->lpDDDestSurface->lpGbl;

    if (lpSource->fpVidMem != ppdev->fpVisibleOverlay)
    {
        if (lpUpdateOverlay->dwFlags & DDOVER_SHOW)
        {
            if (ppdev->fpVisibleOverlay != 0)
            {
                // Some other overlay is already visible:

                DISPDBG((DDRAW_DEBUG_LEVEL, "DdUpdateOverlay: An overlay is already visible"));

                lpUpdateOverlay->ddRVal = DDERR_OUTOFCAPS;
                return(DDHAL_DRIVER_HANDLED);
            }
            else
            {
                // We're going to make the overlay visible, so mark it as
                // such:

                ppdev->fpVisibleOverlay = lpSource->fpVidMem;
            }
        }
        else
        {
            // The overlay isn't visible, and we haven't been asked to make
            // it visible, so this call is trivially easy:

            //Flag status to indicate we are no longer utilizing the stream...
            ppdev->flStatus &= ~STAT_STREAMS_UTILIZED;

            lpUpdateOverlay->ddRVal = DD_OK;
            return(DDHAL_DRIVER_HANDLED);
        }
    }

    //Flag status to indicate we are going to utilize the stream...
    ppdev->flStatus |= STAT_STREAMS_UTILIZED;

    ppdev->SSStride =
    dwStride =  lpSource->lPitch;

    dwDesktopSSX =
    dwSecX = lpUpdateOverlay->rDest.left;
    dwSecY = lpUpdateOverlay->rDest.top;

    srcWidth =  lpUpdateOverlay->rSrc.right   - lpUpdateOverlay->rSrc.left;
    srcHeight = lpUpdateOverlay->rSrc.bottom  - lpUpdateOverlay->rSrc.top;
    dstWidth =  lpUpdateOverlay->rDest.right  - lpUpdateOverlay->rDest.left;
    dstHeight = lpUpdateOverlay->rDest.bottom - lpUpdateOverlay->rDest.top;

    if (ppdev->LPBData.VPType  == 2)  //PAL DVD
       srcWidth = RoundDown(srcWidth, 64);

#if _WIN32_WINNT >= 0x0500

    if (lpUpdateOverlay->dwFlags & DDOVER_INTERLEAVED)
    {
        // When creating an interleaved overlay, the client is expected to
        // pass in rectangles which assume "weave" mode.  However, we are
        // required to start out in "bob" mode, and will switch to weave
        // later (in the miniport).  So adjust stride and height for bob
        // mode on an interleaved surface.

        DISPDBG((DDRAW_DEBUG_LEVEL, "DdUpdateOverlay: Interleaved Bob Mode"));

        dwStride *= 2;
        srcHeight /= 2;
    }

#endif

    //
    //  save the input parameters for later
    //

    dwDesktopSSWidth =
    ppdev->SSdstWidth = dstWidth;
    ppdev->SSdstHeight = dstHeight;
    ppdev->SSsrcWidth = srcWidth;
    ppdev->SSsrcHeight = srcHeight;
    ppdev->SSX = dwSecX;
    ppdev->SSY = dwSecY;

    //
    //  fix the size & position parameters if Panel Expansion or
    //  Centering is enabled.
    //

    if (ppdev->Band.DuoViewFlags & DVW_Expand_ON)
    {
        //
        //  First, assume no X,Y adjustment (note that DdSetOverlayPosition
        //  relies on these fields having been initialized).
        //
        ppdev->DisplayXOffset = 0;
        ppdev->DisplayYOffset = 0;

        //
        //  Check and handle horizontal expansion
        //
        dwDisplaySize = ppdev->Band.VisibleWidthInPixels;
        dwPanelSize   = ppdev->Band.GSizeX2;
        ppdev->XExpansion = 0x00010001L;    // set to a valid default

        if ((2 * dwDisplaySize) <= dwPanelSize)
        {
            ppdev->XExpansion = 0x00020001L;
            dstWidth = 2 * dstWidth;
            //
            // NOTE:   the X starting offset has to be on a character clock,
            //         or 8 pixel boundary
            //
            ppdev->DisplayXOffset = ((dwPanelSize - (2 * dwDisplaySize)) / 2)
                                     & 0xFFF8;
        }
        else if ((3 * dwDisplaySize) <= (2 * dwPanelSize))
        {
            ppdev->XExpansion = 0x00030002L;
            dstWidth = (3 * dstWidth) / 2;
            ppdev->DisplayXOffset = ((dwPanelSize -
                                     ((3 * dwDisplaySize)/2)) / 2) & 0xFFF8;
        }
        else if ((5 * dwDisplaySize) <= (4 * dwPanelSize))
        {
            ppdev->XExpansion = 0x00050004L;
            dstWidth = (5 * dstWidth) / 4;
            ppdev->DisplayXOffset = ((dwPanelSize -
                                     ((5 * dwDisplaySize)/4)) / 2) & 0xFFF8;
        }
        else if ((9 * dwDisplaySize) <= (8 * dwPanelSize))
        {
            ppdev->XExpansion = 0x00090008L;
            dstWidth = (9 * dstWidth) / 8;
            ppdev->DisplayXOffset = ((dwPanelSize -
                                     ((9 * dwDisplaySize)/8)) / 2) & 0xFFF8;
        }

        //
        //  Check and handle vertical expansion
        //
        dwDisplaySize = ppdev->Band.VisibleHeightInPixels;
        dwPanelSize   = ppdev->Band.GSizeY2;
        ppdev->YExpansion = 0x00010001L;    // set to a valid default

        if ((2 * dwDisplaySize) <= dwPanelSize)
        {
            ppdev->YExpansion = 0x00020001L;
            dstHeight = 2 * dstHeight;
            ppdev->DisplayYOffset = (dwPanelSize - (2 * dwDisplaySize)) / 2;
        }
        else if ((3 * dwDisplaySize) <= (2 * dwPanelSize))
        {
            ppdev->YExpansion = 0x00030002L;
            dstHeight = (3 * dstHeight) / 2;
            ppdev->DisplayYOffset = (dwPanelSize -
                                     ((3 * dwDisplaySize)/2)) / 2;
        }
        else if ((4 * dwDisplaySize) <= (3 * dwPanelSize))
        {
            ppdev->YExpansion = 0x00040003L;
            dstHeight = (4 * dstHeight) / 3;
            ppdev->DisplayYOffset = (dwPanelSize -
                                     ((4 * dwDisplaySize)/3)) / 2;
        }
        else if ((5 * dwDisplaySize) <= (4 * dwPanelSize))
        {
            ppdev->YExpansion = 0x00050004L;
            dstHeight = (5 * dstHeight) / 4;
            ppdev->DisplayYOffset = (dwPanelSize -
                                     ((5 * dwDisplaySize)/4)) / 2;
        }
        else if ((6 * dwDisplaySize) <= (5 * dwPanelSize))
        {
            ppdev->YExpansion = 0x00060005L;
            dstHeight = (6 * dstHeight) / 5;
            ppdev->DisplayYOffset = (dwPanelSize -
                                     ((6 * dwDisplaySize)/5)) / 2;
        }

        dwSecX = ((dwSecX * (ppdev->XExpansion >> 16)) /
                      (ppdev->XExpansion & 0xFFFF)) + ppdev->DisplayXOffset;
        dwSecY = ((dwSecY * (ppdev->YExpansion >> 16)) /
                      (ppdev->YExpansion & 0xFFFF)) + ppdev->DisplayYOffset;
    }
    else if (ppdev->Band.DuoViewFlags & DVW_Center_ON)
    {
        //
        //  First, assume no X,Y adjustment (note that DdSetOverlayPosition
        //  relies on these fields having been initialized).
        //
        ppdev->DisplayXOffset = 0;
        ppdev->DisplayYOffset = 0;

        //
        // The preference is centering.  Check if the resolution is smaller
        // than the physical device (check if we're actively centering), and
        // adjust video window position if so.
        //
        if (ppdev->Band.GSizeX2 > ppdev->Band.VisibleWidthInPixels)
        {
            ppdev->DisplayXOffset = (((DWORD)ppdev->Band.GSizeX2 -
                                    ppdev->Band.VisibleWidthInPixels)/2)
                                    & 0xFFF8;
            dwSecX += ppdev->DisplayXOffset;
        }
        if (ppdev->Band.GSizeY2 > ppdev->Band.VisibleHeightInPixels)
        {
            ppdev->DisplayYOffset = ((DWORD)ppdev->Band.GSizeY2 -
                                    ppdev->Band.VisibleHeightInPixels)/2;
            dwSecY += ppdev->DisplayYOffset;
        }
    }

    //
    //  make sure the Horizontal K2 factor is within range
    //

    if ((ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE)  &&
        ((dstWidth - srcWidth) >= 1024))
    {
        DISPDBG( ( DDRAW_DEBUG_LEVEL, "Horiz K2 value is too large" ) );

        lpUpdateOverlay->ddRVal = DDERR_TOOBIGWIDTH;
        ppdev->flStatus &= ~STAT_STREAMS_UTILIZED;
        return( DDHAL_DRIVER_HANDLED );
    }

    ppdev->SSdisplayWidth = displayWidth = dstWidth;
    ppdev->SSdisplayHeight = displayHeight = dstHeight;

    dwSecXOffset = dwSecYOffset = 0;

    if (ppdev->Band.DuoViewFlags & DVW_Panning_ON)
    {
        ddClipStreamsToViewport (ppdev, &dwSecX, &dwSecY,
                                 &displayWidth, &displayHeight,
                                 &dwSecXOffset, &dwSecYOffset );
        dwDesktopSSX = dwSecX;
        dwDesktopSSWidth = displayWidth;

    }

    //
    // Calculate DDA horizonal accumulator initial value:
    //

    dwSecCtrl = HDDA(srcWidth, dstWidth);

    //
    // Overlay input data format:
    //

    ppdev->Band.IsSecFormatYUV = 0;

    if (lpSource->ddpfSurface.dwFlags & DDPF_FOURCC)
    {
        dwBitCount = lpSource->ddpfSurface.dwYUVBitCount;

        switch (lpSource->ddpfSurface.dwFourCC)
        {
        case FOURCC_YUY2:
            dwSecCtrl |= S_YCrCb422;    // Not S_YUV422!  Dunno why...
            if (!ppdev->f3TapFlickerFilterEnabled)
            {
                ppdev->Band.IsSecFormatYUV = 1;
            }

            break;


        default:
            RIP("Unexpected FourCC");
        }
    }
    else
    {
        ASSERTDD(lpSource->ddpfSurface.dwFlags & DDPF_RGB,
            "Expected us to have created only RGB or YUV overlays");

        // The overlay surface is in RGB format:

        dwBitCount = lpSource->ddpfSurface.dwRGBBitCount;

        switch (dwBitCount)
        {
        case 16:
            if (IS_RGB15_R( lpSource->ddpfSurface.dwRBitMask ))
            {
                dwSecCtrl |= S_RGB15;
            }
            else
            {
                dwSecCtrl |= S_RGB16;
            }
            break;

        case 24:
            dwSecCtrl |= S_RGB24;
            break;

        case 32:
            dwSecCtrl |= S_RGB32;
            break;

        default:
            break;
        }

    }


    if (ppdev->Band.SWFlags & SW_CALCULATE)
    {
        BandInf.Option              = BANDWIDTH_SETSECONDARY;
        BandInf.device              = ID_SWCODEC;
        BandInf.bpp                 = (short) dwBitCount;
        BandInf.xSrc                = (short) srcWidth;
        BandInf.ySrc                = (short) srcHeight;
        BandInf.xTrg                = (short) dstWidth;
        BandInf.yTrg                = (short) dstHeight;
        BandInf.xMax                = (short) srcWidth;
        BandInf.yMax                = (short) srcHeight;
        BandInf.RefreshRate         = (short) ppdev->ulRefreshRate;
        BandInf.pBand               = (PVOID) &ppdev->Band;
        ppdev->Band.GRefreshRate2   = (WORD) ppdev->ulRefreshRate2;

        if (EngDeviceIoControl( ppdev->hDriver,
                                IOCTL_VIDEO_S3_QUERY_BANDWIDTH,
                                &BandInf,
                                sizeof( BANDINF ),
                                &ppdev->Band,
                                sizeof( BAND ),
                                &ReturnedDataLength ))
        {
            DISPDBG( ( DDRAW_DEBUG_LEVEL,
                       "Miniport reported no streams parameters" ) );
        }
        else
        {
            ppdev->ulFifoValue          = ppdev->Band.FifoCtrl;

            if (ppdev->Band.HStretch != (ULONG) 1000 &&
                ppdev->Band.HStretch >  (ULONG) (1000*dstWidth/srcWidth))
            {
                lpUpdateOverlay->ddRVal = DDERR_UNSUPPORTED;
                ppdev->flStatus &= ~STAT_STREAMS_UTILIZED;
                return( DDHAL_DRIVER_HANDLED );
            }
        }

    }
    else if (ppdev->ulMinOverlayStretch != (ULONG) 1000 &&
             ppdev->ulMinOverlayStretch >  (ULONG) (1000*dstWidth/srcWidth))
    {
        lpUpdateOverlay->ddRVal = DDERR_UNSUPPORTED;
        ppdev->flStatus &= ~STAT_STREAMS_UTILIZED;
        return( DDHAL_DRIVER_HANDLED );
    }


    // Calculate start of video memory in QWORD boundary

    dwBytesPerPixel = dwBitCount >> 3;

    dwStart = (lpUpdateOverlay->rSrc.top * dwStride)
            + (lpUpdateOverlay->rSrc.left * dwBytesPerPixel);

    //
    //  save the Base Starting address
    //

    ppdev->SSBaseStartAddr = dwStart;
    ppdev->SSbytesPerPixel = dwBytesPerPixel;
    ppdev->SSbitsPerPixel = dwBitCount;
    //
    //  adjust for Panned view
    //

    dwStart += (dwSecYOffset * dwStride) + (dwSecXOffset * dwBytesPerPixel);

    // Note that since we're shifting the source's edge to the left, we
    // should really increase the source width to compensate.  However,
    // doing so when running at 1 to 1 would cause us to request a
    // shrinking overlay -- something the S3 can't do.

    //
    // NAC - fix SPR 14438, alignment problems when clipping.  wierd
    //       colors in the AVI.  Make sure quadword boundary is also
    //       pixel boundary for 24bpp overlay.
    //

    if (dwBytesPerPixel != 3)
    {
        dwStart &= ~7;
    }
    else
    {
        dwTmp    = lpUpdateOverlay->rSrc.left & 0x7;
        dwTmp   += dwTmp << 1;
        dwStart -= dwTmp;
    }

    ppdev->dwOverlayFlipOffset = dwStart;     // Save for flip
    dwStart += (DWORD)(lpSource->fpVidMem);

#if _WIN32_WINNT >= 0x0500

    if (lpUpdateOverlay->dwFlags & DDOVER_AUTOFLIP)
    {
        DISPDBG((0,"Hardware autoflipping..."));

        if (lpUpdateOverlay->dwFlags & DDOVER_BOB)
        {
            // Can't bob and hardware autoflip until the GX2 chip:

            DISPDBG((DDRAW_DEBUG_LEVEL, "Can't autoflip bob in hardware"));
            lpUpdateOverlay->ddRVal = DDERR_OUTOFCAPS;
            return(DDHAL_DRIVER_HANDLED);
        }

        // We have to verify that all the surface's are there that we're
        // expecting:

        if (ppdev->dwNumAutoflip == 2 )
        {
            DWORD dwOffset = 0;

            if (lpSource->fpVidMem == ppdev->AutoflipOffsets[0])
            {
                dwOffset = ppdev->AutoflipOffsets[1];
            }
            else if (lpSource->fpVidMem == ppdev->AutoflipOffsets[1])
            {
                dwOffset = ppdev->AutoflipOffsets[0];
            }
            if (dwOffset != 0)
            {
                WRITE_STREAM_D(pjMmBase, S_1,
                    ppdev->dwOverlayFlipOffset + dwOffset);

                WRITE_STREAM_D(pjMmBase, LPB_DB, 0x44);
            }
            else
            {
                DISPDBG((0, "source surface is not an autoflip surface"));
            }
        }
        else
        {
            DISPDBG((0, "Invalid number of autoflip surfaces"));
        }
    }

#endif

    // Set overlay filter characteristics:

    if ((dstWidth != srcWidth) || (dstHeight != srcHeight))
    {
        if (dstWidth >= (srcWidth << 2))
        {
            dwSecCtrl |= S_Beyond4x;    // Linear, 1-2-2-2-1, for >4X stretch
        }
        else if (dstWidth >= (srcWidth << 1))
        {
            dwSecCtrl |= S_2xTo4x;      // Bi-linear, for 2X to 4X stretch
        }
        else
        {
            dwSecCtrl |= S_Upto2x;      // Linear, 0-2-4-2-0, for X stretch
        }
    }

    // Extract colour key:

    ASSERTDD((lpUpdateOverlay->dwFlags & (DDOVER_KEYSRC | DDOVER_KEYDEST)) == 0,
        "NT guarantees that DDOVER_KEYSRC and DDOVER_KEYDEST are never set");

    dwGX2BlendControl = GX2_Ks_Mask;
    if (dwFlags & DDOVER_KEYDEST)
    {
        lpColorKeySurface = lpDestination;
        bColorKey = TRUE;
        dwKeyLow  = lpUpdateOverlay->lpDDDestSurface->ddckCKDestOverlay.dwColorSpaceLowValue;
        dwKeyHigh = lpUpdateOverlay->lpDDDestSurface->ddckCKDestOverlay.dwColorSpaceHighValue;
        dwBlendCtrl |= KeyOnP;
    }
    else if (dwFlags & DDOVER_KEYDESTOVERRIDE)
    {
        lpColorKeySurface = lpDestination;
        bColorKey = TRUE;
        dwKeyLow  = lpUpdateOverlay->overlayFX.dckDestColorkey.dwColorSpaceLowValue;
        dwKeyHigh = lpUpdateOverlay->overlayFX.dckDestColorkey.dwColorSpaceHighValue;
        dwBlendCtrl |= KeyOnP;
    }
    else if (dwFlags & DDOVER_KEYSRC)
    {
        lpColorKeySurface = lpSource;
        bColorKey = TRUE;
        dwKeyLow = lpUpdateOverlay->lpDDDestSurface->ddckCKSrcOverlay.dwColorSpaceLowValue;
        dwKeyHigh = lpUpdateOverlay->lpDDDestSurface->ddckCKSrcOverlay.dwColorSpaceHighValue;
        dwBlendCtrl |= KeyOnS;
        dwGX2BlendControl = GX2_Kp_Mask;
    }
    else if (dwFlags & DDOVER_KEYSRCOVERRIDE)
    {
        lpColorKeySurface = lpSource;
        bColorKey = TRUE;
        dwKeyLow = lpUpdateOverlay->overlayFX.dckSrcColorkey.dwColorSpaceLowValue;
        dwKeyHigh = lpUpdateOverlay->overlayFX.dckSrcColorkey.dwColorSpaceHighValue;
        dwBlendCtrl |= KeyOnS;
        dwGX2BlendControl = GX2_Kp_Mask;
    }

    if (ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE)
    {
        dwBlendCtrl = dwGX2BlendControl;

        //
        // fix SPR 17336, disable VFILTER if on a GX2 and using src colorkey
        //

        if (dwFlags & (DDOVER_KEYSRC | DDOVER_KEYSRCOVERRIDE))
        {
            ppdev->Band.HWFlags &= ~HW_VFILTER;
        }
    }

    if (bColorKey)
    {
        if (lpColorKeySurface->ddpfSurface.dwFlags & DDPF_FOURCC)
        {
            switch (lpColorKeySurface->ddpfSurface.dwFourCC)
            {
            case FOURCC_YUY2:
                dwKeyLow = (dwKeyLow & 0x00ff7f7f) | (~(dwKeyLow | 0x00ff7f7f));
                dwKeyHigh = (dwKeyHigh & 0x00ff7f7f) | (~(dwKeyHigh | 0x00ff7f7f));
                break;

            default:
                lpUpdateOverlay->ddRVal = DDERR_UNSUPPORTED;
                ppdev->flStatus &= ~STAT_STREAMS_UTILIZED;
                return( DDHAL_DRIVER_HANDLED );
                break;
            }
        }
        else if (lpColorKeySurface->ddpfSurface.dwFlags & DDPF_PALETTEINDEXED8)
        {
            if (ppdev->Band.SWFlags & SW_INDEXCOLORKEY)
            {
                dwKeyLow |= AlphaKeying | CompareBits0t7;
            }
            else
            {
                dwKeyLow = dwGetPaletteEntry(ppdev, dwKeyLow) | CompareBits2t7;
                dwKeyHigh = dwKeyLow;
            }
        }
        else
        {
            // We have to transform the colour key from its native format
            // to 8-8-8:

            switch (lpColorKeySurface->ddpfSurface.dwRGBBitCount)
            {
            case 8:
                dwKeyLow = dwGetPaletteEntry(ppdev, dwKeyLow);
                dwKeyHigh = dwKeyLow;
                dwKeyLow |= CompareBits2t7;
                break;
            case 16:
                if (IS_RGB15_R(lpColorKeySurface->ddpfSurface.dwRBitMask))
                {
                    dwKeyLow = RGB15to32(dwKeyLow);
                    dwKeyHigh = RGB15to32(dwKeyHigh);
                }
                else
                {
                    dwKeyLow = RGB16to32(dwKeyLow);
                    dwKeyHigh = RGB16to32( dwKeyHigh );
                }

                dwKeyLow |= CompareBits3t7;
                break;

            default:
                dwKeyLow |= CompareBits0t7;
                break;
            }
        }

        dwKeyLow |= KeyFromCompare;

        if (ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE)
        {
            dwKeyLow &= 0x07ffffff;
            dwKeyLow |= ColorKeying;

            if (dwFlags & (DDOVER_KEYDEST | DDOVER_KEYDESTOVERRIDE))
            {
                dwKeyLow |= KeySelectP;
            }
            else
            {
                dwKeyLow |= KeySelectS;
            }
        }
    }
    else if (ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE)
    {
        ddCalcOpaqueOverlay( ppdev,
                             &dwOpaqueCtrl,
                             dwDesktopSSX,
                             dwDesktopSSWidth );
    }

    //
    // enable vertical interpolation and memory bandwidth savings modes
    //

    dwVDDA = VDDA( dstHeight );

    if (ppdev->Band.SWFlags & SW_CALCULATE)
    {
        ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

        //
        // Turn on Vertical Interpolation if...
        // 1) The hardware supports it    -AND-
        // 2) Width of clip is a multiple of 16 pixels
        //

        //bchernis: Disabled checking for multiple 16 pixels (mith)
		//What needed was having SP and LPB strides properly alligned.
		//if( (ppdev->Band.HWFlags & HW_VFILTER) &&
        //    ((srcWidth & 0xF) == 0) )

		if(ppdev->Band.HWFlags & HW_VFILTER)
        {
            //
            // enable vertical interpolation
            //

            dwVDDA |= ENABLE_HWVFILTER;
            DISPDBG((1, "Vertical Interpolation Enabled"));

            //
            // enable memory bandwidth savings mode 2
            //

            if (ppdev->Band.HWFlags & HW_SAVEMODE2)
            {
                dwVDDA |= ENABLE_HWSAVEMODE2;
                bDropUVcomponent = TRUE;
            }
        }
        else
        {
            //
            // enable memory bandwidth savings mode 1
            // VFilter and savemode 2 must be enabled to be able to drop UV/CbCr.
            //

            if (ppdev->Band.HWFlags & HW_VFILTERYONLY2 && bDropUVcomponent)
            {
               dwVDDA |= ENABLE_HWSAVEMODE1;
            }
        }

        //
        // enable/disable dropping of UV/CbCr components
        //
        // bchernis: CR87_01 and MM81E8_15 has to be set/cleared together
		// to avoid a nasty gridded picture

        //if (ppdev->Band.HWFlags & HW_VFILTERYONLY2)
		if ((ppdev->Band.HWFlags & HW_VFILTERYONLY2) &&
		    (ppdev->Band.HWFlags & HW_VFILTER))
        {

            VGAOUTP( ppdev,
                     CRTC_INDEX,
                     EXT_MISC_CONTROL_REG );

            VGAOUTP( ppdev,
                     CRTC_DATA,
                     VGAINP( ppdev, CRTC_DATA ) | DROP_UV_CBCR );

        }
        else if (ppdev->Band.HWCaps & HW_VFILTERYONLY2)
        {

            VGAOUTP( ppdev,
                     CRTC_INDEX,
                     EXT_MISC_CONTROL_REG );

            VGAOUTP( ppdev,
                     CRTC_DATA,
                     VGAINP( ppdev, CRTC_DATA ) & KEEP_UV_CBCR );

        }

        //
        // set M & N parameters
        //

        if (ppdev->Band.HWFlags & HW_SETM1)
        {
            VGAOUTP( ppdev,
                     CRTC_INDEX,
                     0x54 );

            VGAOUTP( ppdev,
                     CRTC_DATA,
                     ppdev->Band.M1 );
        }

        if (ppdev->Band.HWFlags & HW_SETM2)
        {
            VGAOUTP( ppdev,
                     CRTC_INDEX,
                     0x60 );

            VGAOUTP( ppdev,
                     CRTC_DATA,
                     ppdev->Band.M2 );
        }

        if (ppdev->Band.HWFlags & HW_SETM3)
        {
            VGAOUTP( ppdev,
                     CRTC_INDEX,
                     0x72 );

            VGAOUTP( ppdev,
                     CRTC_DATA,
                     ppdev->Band.M3 );
        }

        //
        // set secondary stream fifo fetch control registers
        //

        if (ppdev->Band.HWFlags & HW_SETSSFETCH)
        {
            BYTE bSSFFCtrl1;              // CR 92
            BYTE bSSFFCtrl2;              // CR 93
            DWORD stride;

            //
            // calculate the secondary stream L2 parameter...
            // = [(number of bytes of displayed pixels per scan line) / 8] rounded up
            //   Note, number of bytes of displayed pixels per...  must be even!
            //
            stride = ( ((dwStride & 0xFFFFFFFE) + 7) / 8 );

            bSSFFCtrl2 = (BYTE)stride;
            bSSFFCtrl1 = ((BYTE)(stride>>8) & 0x7F) | ENABLE_L2_PARAM;

            VGAOUTP( ppdev,
                     CRTC_INDEX,
                     EXT_SSFIFO_CTRL1_REG );

            VGAOUTP( ppdev,
                     CRTC_DATA,
                     bSSFFCtrl1 );

            VGAOUTP( ppdev,
                     CRTC_INDEX,
                     EXT_SSFIFO_CTRL2_REG );

            VGAOUTP( ppdev,
                     CRTC_DATA,
                     bSSFFCtrl2 );
        }

        RELEASE_CRTC_CRITICAL_SECTION( ppdev );
    }

    //
    // Update and show:
    //

    S3DGPWait( ppdev );

    WAIT_FOR_VBLANK(ppdev);

    if (ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE)
    {
        //
        //  select which IGA for the secondary streams
        //

        ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

        VGAOUTPW( ppdev, SEQ_INDEX, SEQ_REG_UNLOCK);

        VGAOUTP( ppdev, SEQ_INDEX, ARCH_CONFIG_REG );
        SrReg = VGAINP( ppdev, SEQ_DATA ) & (~STREAMS_IGA_SELECT_BIT);
        if (ppdev->Band.DuoViewFlags & DVW_SP_IGA1)
        {
            SrReg |= STREAMS_ON_IGA1;
        }
        else
        {
            SrReg |= STREAMS_ON_IGA2;
        }
        VGAOUTP( ppdev, SEQ_DATA, SrReg );

        if (ppdev->bVertInterpSynched == 0)
        {
            //
            // Resynch vertical count to new IGA
            //

            vSynchVerticalCount( ppdev, SrReg );

            //
            // and clear our trigger...
            //

            ppdev->bVertInterpSynched = 1;
        }

        RELEASE_CRTC_CRITICAL_SECTION(ppdev);
    }

    WRITE_STREAM_D(pjMmBase, S_0,           dwStart);
    WRITE_STREAM_D(pjMmBase, S_1,           dwStart);

    WRITE_STREAM_D(pjMmBase, S_XY,          XY(dwSecX, dwSecY));
    WRITE_STREAM_D(pjMmBase, S_WH,          WH(displayWidth, displayHeight));
    WRITE_STREAM_D(pjMmBase, S_STRIDE,      dwStride);
    WRITE_STREAM_D(pjMmBase, S_CONTROL,     dwSecCtrl);
    WRITE_STREAM_D(pjMmBase, S_HK1K2,       HK1K2(srcWidth, dstWidth));
    WRITE_STREAM_D(pjMmBase, S_VK1,         VK1(srcHeight));
    WRITE_STREAM_D(pjMmBase, S_VK2,         VK2(srcHeight, dstHeight));
    WRITE_STREAM_D(pjMmBase, S_VDDA,        dwVDDA );
    WRITE_STREAM_D(pjMmBase, OPAQUE_CONTROL,dwOpaqueCtrl );

    if (bColorKey || ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE)
    {
        WRITE_STREAM_D(pjMmBase, CKEY_LOW,  dwKeyLow);
        WRITE_STREAM_D(pjMmBase, CKEY_HI,   dwKeyHigh);
    }

    WRITE_STREAM_D(pjMmBase, BLEND_CONTROL, dwBlendCtrl);


    if (ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE)
    {
        ddSetStreamsFifo( ppdev, FALSE );
    }
    else
    {
        WRITE_STREAM_D( pjMmBase,
                        MIU_CONTROL,
                        ( ppdev->ulDefaultMIUControl & 0xFFFFF870 ) |
                        ( ( ppdev->ulFifoValue & 0x8000 ) >> 13 ) );

        WRITE_STREAM_D( pjMmBase,
                        FIFO_CONTROL,
                        (ppdev->ulFifoValue  & 0x0000001F)          |
                        ((ppdev->ulFifoValue & 0x000003E0) << 1)    |
                        ((ppdev->ulFifoValue & 0x00007C00) << 2) );
    }

    lpUpdateOverlay->lpDDSrcSurface->dwReserved1 |= OVERLAY_FLG_ENABLED;

    lpUpdateOverlay->ddRVal = DD_OK;
    return(DDHAL_DRIVER_HANDLED);
}


/******************************Public*Routine******************************\
* BOOL bEnableDirectDraw
*
* This function is called by enable.c when the mode is first initialized,
* right after the miniport does the mode-set.
*
\**************************************************************************/

BOOL bEnableDirectDraw(
PDEV*   ppdev)
{
    BYTE*             pjIoBase;
    DWORD             ReturnedDataLength;
    BANDINF           BandInf;
    WORD              wBitsPerPel;
    S3_VIDEO_PAN_INFO PanningInfo;
    BOOL              fDuoView;
    BYTE              bTmp;
    TV_FLICKER_FILTER_CONTROL TVFFControl;
    CLOCK_INFO        ClockInfo;
    MOBILE_INTERFACE  MobileInterface;


    // We're not going to bother to support accelerated DirectDraw on
    // those S3s that can't support memory-mapped I/O, simply because
    // they're old cards and it's not worth the effort.  We also
    // require DIRECT_ACCESS to the frame buffer.
    //
    // We also don't support 864/964 cards because writing to the frame
    // buffer can hang the entire system if an accelerated operation is
    // going on at the same time.


    DISPDBG((DDRAW_DEBUG_LEVEL, "bEnableDirectDraw" ));

    pjIoBase = ppdev->pjIoBase;

    // We have to preserve the contents of register 0x69 on the S3's page
    // flip:

    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    VGAOUTP( ppdev, CRTC_INDEX, EXT_SYS_CTRL3 );

    //
    // Save off bits 0 - 4, disp-start-addr (0-4 used on ViRGE-VX, but only
    // 0-3 used on ViRGE and ViRGE DX/GX).
    //

    ppdev->ulExtendedSystemControl3Register_69
        = (VGAINP(ppdev,CRTC_DATA) & 0xe0) << 8;


    RELEASE_CRTC_CRITICAL_SECTION(ppdev);

    //
    //  get flags/state for Dual IGA controllers
    //

    EngDeviceIoControl( ppdev->hDriver,
                        IOCTL_VIDEO_S3_QUERY_STREAMS_STATE,
                        &ppdev->Band,
                        sizeof( BAND ),
                        &ppdev->Band,
                        sizeof( BAND ),
                        &ReturnedDataLength );

    //
    //  query also the video aperture
    //
    EngDeviceIoControl( ppdev->hDriver,
                        IOCTL_VIDEO_S3_MBL_QUERY_VIDEO_PAN_INFO,
                        NULL,
                        0,
                        &PanningInfo,
                        sizeof( S3_VIDEO_PAN_INFO ),
                        &ReturnedDataLength );

    ppdev->ApertureXOffset = PanningInfo.StreamsIGA_ApertureX;
    ppdev->ApertureYOffset = PanningInfo.StreamsIGA_ApertureY;
    ppdev->ApertureIga1 = PanningInfo.IGA1_ApertureOffset;
    ppdev->ApertureIga2 = PanningInfo.IGA2_ApertureOffset;

    //
    //  Some initialization (positioning offsets, which will be altered
    //  if TV/LCD centering/expansion occurs).
    //
    ppdev->DisplayXOffset  = 0;
    ppdev->DisplayYOffset  = 0;
    ppdev->fpVisibleBuffer = 0;


    // Accurately measure the refresh rate for later:

    vGetDisplayDuration(ppdev);
    vGetDisplayDurationIGA2( ppdev );

    // DirectDraw is all set to be used on this card:

    ppdev->flStatus |= STAT_DIRECTDRAW_CAPABLE;

    ACQUIRE_CRTC_CRITICAL_SECTION( ppdev );

    BandInf.fam     = ReadCrReg( ppdev, 0x2D);

    BandInf.id      = (BandInf.fam << 8) | (ReadCrReg( ppdev, 0x2E));

    RELEASE_CRTC_CRITICAL_SECTION( ppdev );

    BandInf.pBand   = (PVOID) &ppdev->Band;

    if (ppdev->ulCaps & CAPS_STREAMS_CAPABLE)
    {
        // Query the miniport to get the correct streams parameters
        // for this mode:

        ClockInfo.MemoryClock = 0;

        if ((!EngDeviceIoControl( ppdev->hDriver, // hDevice
                                 IOCTL_VIDEO_S3_MBL_GET_MOBILE_INTERFACE,
                                 NULL,
                                 0,
                                 (LPVOID)&MobileInterface,
                                 sizeof(MOBILE_INTERFACE),
                                 &ReturnedDataLength) ) &&
            ReturnedDataLength == sizeof(MOBILE_INTERFACE))
        {
            if ( (MobileInterface.ulSubType >= SUBTYPE_240) &&
                 (MobileInterface.ulSubType <= SUBTYPE_282) )
            {
                if ( (!EngDeviceIoControl(  ppdev->hDriver,
                                        IOCTL_VIDEO_S3_GET_CLOCKS,
                                        0,
                                        0,
                                        &ClockInfo,
                                        sizeof( CLOCK_INFO ),
                                        &ReturnedDataLength ) ) &&
                    ReturnedDataLength == sizeof(CLOCK_INFO))
                {
                    BandInf.mclk = (USHORT)ClockInfo.MemoryClock;
                    BandInf.dclk = (USHORT)ClockInfo.DClock1;
                }
            }
            else
            {
            BandInf.mclk    = GetMCLK(ppdev);
            }
        }

        //
        // Miniport will fill in the real values
        //

        BandInf.mType   =
        BandInf.mWidth  = 1;

        BandInf.Option  = BANDWIDTH_INIT;

        ppdev->wChipId  = BandInf.id;

        wBitsPerPel     = (WORD)((ppdev->cBitsPerPel + 1) & ~7);

        if ( !EngDeviceIoControl( ppdev->hDriver,
                                  IOCTL_VIDEO_S3_QUERY_BANDWIDTH,
                                  &BandInf,
                                  sizeof( BANDINF ),
                                  &ppdev->Band,
                                  sizeof( BAND ),
                                  &ReturnedDataLength ) )
        {
            BandInf.Option            = BANDWIDTH_SETPRIMARY;
            if (ClockInfo.MemoryClock == 0)
            {
                BandInf.dclk              = GetDCLK( ppdev );
            }
            BandInf.xRes              = (short) ppdev->cxScreen;
            BandInf.yRes              = (short)ppdev->cyScreen;
            BandInf.bpp               = (short) ppdev->cBitsPerPel;
            if (ClockInfo.MemoryClock == 0)
            {
                ppdev->Band.MHzDCLK2      = GetDCLK2( ppdev );
            }
            else
            {
                ppdev->Band.MHzDCLK2      = (USHORT)ClockInfo.DClock2;
            }
            BandInf.dclksDuringBlank  = GetDclksDuringBlank(ppdev);
            BandInf.RefreshRate       = (short) ppdev->ulRefreshRate;
            ppdev->Band.GRefreshRate2 = (WORD) ppdev->ulRefreshRate2;

            ACQUIRE_CRTC_CRITICAL_SECTION( ppdev );

            VGAOUTPW( ppdev, SEQ_INDEX,  SEQ_REG_UNLOCK );
            VGAOUTPW( ppdev, SEQ_INDEX,  SELECT_IGA2_READS_WRITES );

            VGAOUTP( ppdev, CRTC_INDEX, EXT_MODE_CONTROL_REG );
            BandInf.fInterlaced = VGAINP( ppdev, CRTC_DATA ) & INTERLACED_MASK;

            VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA1 );

            RELEASE_CRTC_CRITICAL_SECTION( ppdev );

            if ( !EngDeviceIoControl( ppdev->hDriver,
                                     IOCTL_VIDEO_S3_QUERY_BANDWIDTH,
                                     &BandInf,
                                     sizeof( BANDINF ),
                                     &ppdev->Band,
                                     sizeof( BAND ),
                                     &ReturnedDataLength ) )
            {
                //
                // Calculate for the worst case
                //

                if ( ppdev->Band.SWFlags & SW_CALCULATE )
                {
                    BandInf.bpp     = 16;
                }
                else
                {
                    BandInf.bpp     = wBitsPerPel;
                }

                BandInf.Option      = BANDWIDTH_SETSECONDARY;
                BandInf.device      = ID_SWCODEC;
                BandInf.xSrc        = 720;
                BandInf.ySrc        = 240;
                BandInf.xTrg        = 720;
                BandInf.yTrg        = 240;
                BandInf.xMax        = 720;
                BandInf.yMax        = 240;
                BandInf.RefreshRate = (SHORT) ppdev->ulRefreshRate;

                if ( !EngDeviceIoControl( ppdev->hDriver,
                                          IOCTL_VIDEO_S3_QUERY_BANDWIDTH,
                                          &BandInf,
                                          sizeof(BandInf),
                                          &ppdev->Band,
                                          sizeof( BAND ),
                                          &ReturnedDataLength ) )
                {
                    ppdev->ulMinOverlayStretch  = ppdev->Band.HStretch;
                    ppdev->ulFifoValue          = ppdev->Band.FifoCtrl;

                    ppdev->ulDefaultFIFO            = 0xffffffff;
                    ppdev->ulDefaultSPFifo          = 0xffffffff;
                    ppdev->ulDefaultStreamsTimeout  = 0xffffffff;
                    ppdev->ulDefaultMiscTimeout     = 0xffffffff;
                    ppdev->ulDefaultMIUControl      = 0xffffffff;

                    //
                    // save of the default values
                    //
                    // if no LCD support, set the STN timeouts and thresholds
                    // to 0
                    //
                    // set secondary streams timeout and fifo values to 0
                    //
                    // NOTE: this can be done in a cleaner way, but this is
                    //       set up just in case we want to tweak some values
                    //

                    if ( ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE )
                                        {
                        // Try to reduce the DuoView noise in 24 bit mode.

                        ddSetStreamsFifo(ppdev, FALSE);

                        ACQUIRE_CRTC_CRITICAL_SECTION( ppdev );

                        VGAOUTP( ppdev, CRTC_INDEX, EXT_BIOS_FLAG3 );

                        bTmp = VGAINP( ppdev, CRTC_DATA );

                        if ( ( bTmp & M5_DUOVIEW ) &&
                             (ppdev->Band.DuoViewFlags & DVW_DualViewSameImage) )
                        {
                            fDuoView = TRUE;
                        }
                        else
                        {
                            fDuoView = FALSE;
                        }

                        //
                        // if TV is turned on, check the flicker filter state
                        //

                        if (bTmp & S3_ACTIVE_TV)
                        {
                            EngDeviceIoControl(
                                    ppdev->hDriver,
                                    IOCTL_VIDEO_S3_MBL_GET_TV_FFILTER_STATUS,
                                    NULL,
                                    0,
                                    (LPVOID)&TVFFControl,
                                    sizeof(TV_FLICKER_FILTER_CONTROL),
                                    &ReturnedDataLength );

                            if (TVFFControl.TvFFilterSpecificSetting >=
                                TV_FFILTER_STATE3)
                            {
                                ppdev->f3TapFlickerFilterEnabled = TRUE;
                            }
                            else
                            {
                                ppdev->f3TapFlickerFilterEnabled = FALSE;
                            }
                        }

                        if (ppdev->ulCaps2 & CAPS2_LCD_SUPPORT)
                        {
                            VGAOUTP( ppdev, CRTC_INDEX, GX2_STN_READ_TIMEOUT );
                            ppdev->bDefaultSTNRdTimeOut =
                                                    VGAINP( ppdev, CRTC_DATA );

                            VGAOUTP( ppdev, CRTC_INDEX, GX2_STN_WRITE_TIMEOUT );
                            ppdev->bDefaultSTNWrTimeOut =
                                                    VGAINP( ppdev, CRTC_DATA );
                        }
                        else
                        {
                            ppdev->bDefaultSTNRdTimeOut   =
                            ppdev->bDefaultSTNWrTimeOut   = 0;
                        }

                        ppdev->bDefaultSSTimeOut   =
                        ppdev->bDefaultSSThreshold = 0;


                        //
                        // fix SPR 16964, need to set the PS1 Fifo threshold
                        // and timeout values in a DuoView case
                        //

                        if (fDuoView)
                        {
                            ppdev->bDefaultPS1Threshold =
                                                ppdev->Band.Fifo.PS1Threshold;
                            ppdev->bDefaultPS1TimeOut =
                                                ppdev->Band.PS1TimeOut;
                        }
                        else
                        {
                            VGAOUTP( ppdev,
                                     CRTC_INDEX,
                                     GX2_PS1_FIFO_THRESHOLD );

                            ppdev->bDefaultPS1Threshold =
                                        VGAINP( ppdev, CRTC_DATA );

                            VGAOUTP( ppdev, CRTC_INDEX,  GX2_PS1_TIMEOUT );

                            ppdev->bDefaultPS1TimeOut =
                                                VGAINP( ppdev, CRTC_DATA );
                        }

                        VGAOUTP( ppdev, CRTC_INDEX,  GX2_PS2_TIMEOUT );
                        ppdev->bDefaultPS2TimeOut = VGAINP( ppdev, CRTC_DATA );

                        VGAOUTP( ppdev, CRTC_INDEX, GX2_PS2_FIFO_THRESHOLD );
                        ppdev->bDefaultPS2Threshold = VGAINP( ppdev, CRTC_DATA );

                        RELEASE_CRTC_CRITICAL_SECTION( ppdev );

                        ddSetStreamsFifo( ppdev, TRUE );
                    }

                    DISPDBG( (DDRAW_DEBUG_LEVEL,
                              "Refresh rate: %li Minimum overlay stretch: %li.%03li Fifo value: %lx",
                              ppdev->ulRefreshRate,
                              ppdev->ulMinOverlayStretch / 1000,
                              ppdev->ulMinOverlayStretch % 1000,
                              ppdev->ulFifoValue ) );
                }
            }
        }
    }

    DISPDBG((DDRAW_DEBUG_LEVEL, "Exit: bEnableDirectDraw" ));

    return(TRUE);
}

/******************************Public*Routine******************************\
* DWORD dwdGetScanLine
\**************************************************************************/

DWORD dwGetScanLine( PDEV * ppdev )
{
    BYTE    bTmp1;
    BYTE    bTmp2;

    ACQUIRE_CRTC_CRITICAL_SECTION( ppdev );

    if ( ppdev->ulCaps2 & CAPS2_GX2_STREAMS_COMPATIBLE )
    {
        VGAOUTPW( ppdev, CRTC_INDEX, ((REG_UNLOCK_1 << 8) | EXTREG_LOCK1));
        VGAOUTPW( ppdev, CRTC_INDEX, ((SYSCTL_UNLOCK << 8) | EXTREG_LOCK2));
        VGAOUTPW( ppdev, SEQ_INDEX, SELECT_IGA1 );
    }

    VGAOUTP( ppdev, CRTC_INDEX, EXT_VERTICAL_COUNTER_REGISTER );

    do
    {
        bTmp1 = VGAINP( ppdev, CRTC_DATA );
        bTmp2 = VGAINP( ppdev, CRTC_DATA );
    } while ( bTmp1 != bTmp2 );

    RELEASE_CRTC_CRITICAL_SECTION( ppdev );

    return( (DWORD) (bTmp1 * 8) );
}

/******************************Public*Routine******************************\
* BOOL DdGetScanLine
*
* This function returns the current scan line, and is only supported when
* CAPS_READ_SCANLINE is enabled in the miniport.  This is a DirectDraw
* function.
*
\**************************************************************************/

DWORD DdGetScanLine( PDD_GETSCANLINEDATA pGetScanLine )
{
    PDEV *  ppdev   = (PDEV*) pGetScanLine->lpDD->dhpdev;

    if ( VBLANK_IS_ACTIVE( ppdev ) )
    {
        pGetScanLine->ddRVal = DDERR_VERTICALBLANKINPROGRESS;
    }
    else
    {
        pGetScanLine->dwScanLine = dwGetScanLine( ppdev );
        pGetScanLine->ddRVal = DD_OK;
    }

    return( DDHAL_DRIVER_HANDLED );
}

/******************************Public*Routine******************************\
* BOOL DdUnlock
*
* This function is called to unlock the surface, and is called when
* CAPS_TV_UNDERSCAN_CAPABLE is enabled in the miniport.  This is a
* DirectDraw function.
*
\**************************************************************************/

DWORD DdUnlock( PDD_UNLOCKDATA pUnlockData )
{
    PDEV *              ppdev       = (PDEV*) pUnlockData->lpDD->dhpdev;
    PDD_SURFACE_GLOBAL  pSurface    = pUnlockData->lpDDSurface->lpGbl;
    DWORD               dwSrcWidth;
    DWORD               dwSrcHeight;
    DWORD               dwSrcBase;
    DWORD               dwDstBase;
    DWORD               dwStart;
    BYTE                bBPP;
    ULONG               ulEngDrawCmd;
    DWORD               dwStride;
    DWORD               dwTmp;

    if ( ppdev->flStatus & STAT_TVUNDERSCAN_ENABLED &&
         ppdev->fpVisibleOverlay == pSurface->fpVidMem )
    {
        dwStride    = pSurface->lPitch;
        dwSrcHeight = pSurface->wHeight;
        dwSrcWidth  = pSurface->wWidth;
        bBPP        = (BYTE) (dwStride/dwSrcWidth);
        dwStart     = (DWORD)(pSurface->fpVidMem);

        ulEngDrawCmd = S3D_BITBLT                   |
                       (SRC_COPY << S3D_ROP_SHIFT)  |
                       S3D_X_POSITIVE_BLT           |
                       S3D_Y_POSITIVE_BLT           |
                       (bBPP - 1) << 2              |
                       S3D_DRAW_ENABLE;

        S3DFifoWait(ppdev, 4);

        S3writeHIU( ppdev,
                    S3D_BLT_DESTINATION_SOURCE_STRIDE,
                    (dwStride & 0xFFF8) << 16 | (dwStride & 0xFFF8));

        S3writeHIU( ppdev, S3D_BLT_SOURCE_X_Y,      0 );

        S3writeHIU( ppdev, S3D_BLT_DESTINATION_X_Y, 0 );

        if ( ppdev->flStatus & STAT_PALSUPPORT_ENABLED )
        {
            S3writeHIU( ppdev,
                        S3D_BLT_WIDTH_HEIGHT,
                        (dwSrcWidth - 1) << 16 | 14 );

            for ( dwTmp = 0; dwTmp < dwSrcHeight/15; dwTmp++ )
            {
                dwSrcBase = dwStart + dwStride * dwTmp *15;
                dwDstBase = dwStart + dwStride * dwTmp *14 +
                            dwStride * dwSrcHeight;

                S3DFifoWait(ppdev, 3);

                S3writeHIU( ppdev,
                            S3D_BLT_SOURCE_BASE,
                            dwSrcBase & 0xFFFFFFF8 );

                S3writeHIU( ppdev,
                            S3D_BLT_DESTINATION_BASE,
                            dwDstBase & 0xFFFFFFF8 );

                S3writeHIU( ppdev, S3D_BLT_COMMAND,         ulEngDrawCmd );
            }

            if ( dwSrcHeight % 15 )
            {
                dwSrcBase = dwStart + dwStride * (dwSrcHeight/15) * 15;
                dwDstBase = dwStart + dwStride * (dwSrcHeight/15) * 14 +
                            dwStride * dwSrcHeight;

                S3DFifoWait(ppdev, 4);

                S3writeHIU( ppdev,
                            S3D_BLT_SOURCE_BASE,
                            dwSrcBase & 0xFFFFFFF8 );

                S3writeHIU( ppdev,
                            S3D_BLT_DESTINATION_BASE,
                            dwDstBase & 0xFFFFFFF8 );

                S3writeHIU( ppdev,
                            S3D_BLT_WIDTH_HEIGHT,
                            (dwSrcWidth - 1) << 16 | (dwSrcHeight % 15) );

                S3writeHIU( ppdev, S3D_BLT_COMMAND,         ulEngDrawCmd );
            }
        }
        else
        {
            S3writeHIU( ppdev,
                        S3D_BLT_WIDTH_HEIGHT,
                        (dwSrcWidth - 1) << 16 | 5 );

            for ( dwTmp = 0; dwTmp < dwSrcHeight/6; dwTmp++)
            {
                dwSrcBase = dwStart + dwStride * dwTmp * 6;
                dwDstBase = dwStart + dwStride * dwTmp * 5 +
                            dwStride * dwSrcHeight;

                S3DFifoWait(ppdev, 3);

                S3writeHIU( ppdev,
                            S3D_BLT_SOURCE_BASE,
                            dwSrcBase & 0xFFFFFFF8 );

                S3writeHIU( ppdev,
                            S3D_BLT_DESTINATION_BASE,
                            dwDstBase & 0xFFFFFFF8 );

                S3writeHIU( ppdev, S3D_BLT_COMMAND,         ulEngDrawCmd );
            }

            if ( dwSrcHeight % 6 )
            {
                dwSrcBase = dwStart + dwStride * (dwSrcHeight/6) * 6;
                dwDstBase = dwStart + dwStride * (dwSrcHeight/6) * 5 +
                            dwStride * dwSrcHeight;

                S3DFifoWait(ppdev, 4);

                S3writeHIU( ppdev,
                            S3D_BLT_WIDTH_HEIGHT,
                            (dwSrcWidth - 1) << 16 | (dwSrcHeight % 6) );

                S3writeHIU( ppdev,
                            S3D_BLT_SOURCE_BASE,
                            dwSrcBase & 0xFFFFFFF8 );

                S3writeHIU( ppdev,
                            S3D_BLT_DESTINATION_BASE,
                            dwDstBase & 0xFFFFFFF8 );

                S3writeHIU( ppdev, S3D_BLT_COMMAND,         ulEngDrawCmd );
            }
        }
    }

    return( DDHAL_DRIVER_NOTHANDLED );
}



