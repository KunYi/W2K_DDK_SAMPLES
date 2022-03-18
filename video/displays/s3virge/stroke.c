/******************************Module*Header*******************************\
* Module Name: Stroke.c
*
* DrvStrokePath for S3 driver
*
* Copyright (c) 1992-1995 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"
#include "hw3d.h"

VOID LineBadFn(
PDEV*       ppdev,
STRIP*      pStrip,
ULONG       ulHwForeMix)
{

#if defined (_X86_)
    _asm int 3;
#endif

}

VOID (*gapfnStrip3D[])(PDEV*, STRIP*, ULONG) = {
    vrlSolidHorizontal3D,
    vrlSolidVertical3D,
    vrlSolidDiagonalHorizontal3D,
    vrlSolidDiagonalVertical3D,

// Should be NUM_STRIP_DRAW_DIRECTIONS = 4 strip drawers in every group

    LineBadFn,
    LineBadFn,
    LineBadFn,
    LineBadFn,

};

//
// Declaration to allow us to use the translation array in paint.c to convert
// a ROP2 into a ROP3 for the hardware
//
extern BYTE gaMix[];


/******************************Public*Routine******************************\
* BOOL DrvStrokePath(pso, ppo, pco, pxo, pbo, pptlBrush, pla, mix)
*
* Strokes the path.
*
\**************************************************************************/

BOOL DrvStrokePath(
    SURFOBJ*   pso,
    PATHOBJ*   ppo,
    CLIPOBJ*   pco,
    XFORMOBJ*  pxo,
    BRUSHOBJ*  pbo,
    POINTL*    pptlBrush,
    LINEATTRS* pla,
    MIX        mix)
{
    LINESTATE     ls;
    PFNSTRIP3D*   apfn;
    FLONG         fl;
    PDEV*         ppdev;
    DSURF*        pdsurf;
    OH*           poh;
    ULONG         ulHwMix;
    RECTL         arclClip[4];                  // For rectangular clipping
    SURFOBJ*      psoDst;


    ppdev = (PDEV*) pso->dhpdev;
    pdsurf = (DSURF*) pso->dhsurf;
    poh   = pdsurf->poh;


    //
    // Pass the surface off to GDI if it's a device bitmap that we've
    // converted to a DIB:
    //


    if (pdsurf->dt == DT_DIB)
    {
        return( EngStrokePath( pdsurf->pso,
                               ppo,
                               pco,
                               pxo,
                               pbo,
                               pptlBrush,
                               pla,
                               mix ) );
    }

    //
    // If it is a styled line or 24bpp, let GDI handle it.
    //

    if ( ppdev->iBitmapFormat == BMF_24BPP ||
         pla->pstyle          != NULL      ||
         pla->fl & LA_ALTERNATE )
    {
        psoDst          = ppdev->psoPunt;
        psoDst->pvScan0 = poh->pvScan0;
        psoDst->lDelta  = poh->lDelta;

        return( EngStrokePath( psoDst,
                               ppo,
                               pco,
                               pxo,
                               pbo,
                               pptlBrush,
                               pla,
                               mix ) );
    }

    WAIT_DMA_IDLE(ppdev);
    TRIANGLE_WORKAROUND( ppdev );

    //
    // Convert the ROP2 into a ROP3 for the hardware
    //

    ulHwMix = gaMix[mix & 0xf];

// Get the device ready:

    S3writeHIU(ppdev, S3D_BLT_PATTERN_FOREGROUND_COLOR, pbo->iSolidColor);
    S3writeHIU(ppdev, S3D_BLT_PATTERN_BACKGROUND_COLOR, pbo->iSolidColor);
    S3writeHIU(ppdev, S3D_2D_LINE_PATTERN_FOREGROUND_COLOR, pbo->iSolidColor);
    S3writeHIU(ppdev, S3D_2D_LINE_DESTINATION_SOURCE_STRIDE, poh->lDelta << 16);
    S3writeHIU(ppdev, S3D_BLT_DESTINATION_SOURCE_STRIDE, poh->lDelta << 16);
    S3writeHIU(ppdev, S3D_2D_LINE_DESTINATION_BASE, poh->ulBase);

    //
    // these are needed for the FIX_ViRGE call
    //

    S3writeHIU( ppdev, S3D_BLT_SOURCE_BASE, poh->ulBase );
    S3writeHIU( ppdev, S3D_BLT_DESTINATION_BASE, poh->ulBase );

    fl = 0;

    apfn = &gapfnStrip3D[0];

// Set up to enumerate the path:

    if (pco->iDComplexity != DC_COMPLEX)
    {
        PATHDATA  pd;
        RECTL*    prclClip = (RECTL*) NULL;
        BOOL      bMore;
        ULONG     cptfx;
        POINTFIX  ptfxStartFigure;
        POINTFIX  ptfxLast;
        POINTFIX* pptfxFirst;
        POINTFIX* pptfxBuf;

        if (pco->iDComplexity == DC_RECT)
        {
            fl |= FL_SIMPLE_CLIP;

            arclClip[0]        =  pco->rclBounds;

        // FL_FLIP_D:

            arclClip[1].top    =  pco->rclBounds.left;
            arclClip[1].left   =  pco->rclBounds.top;
            arclClip[1].bottom =  pco->rclBounds.right;
            arclClip[1].right  =  pco->rclBounds.bottom;

        // FL_FLIP_V:

            arclClip[2].top    = -pco->rclBounds.bottom + 1;
            arclClip[2].left   =  pco->rclBounds.left;
            arclClip[2].bottom = -pco->rclBounds.top + 1;
            arclClip[2].right  =  pco->rclBounds.right;

        // FL_FLIP_V | FL_FLIP_D:

            arclClip[3].top    =  pco->rclBounds.left;
            arclClip[3].left   = -pco->rclBounds.bottom + 1;
            arclClip[3].bottom =  pco->rclBounds.right;
            arclClip[3].right  = -pco->rclBounds.top + 1;

            prclClip = arclClip;
        }

        pd.flags = 0;

        do {
            bMore = PATHOBJ_bEnum(ppo, &pd);

            cptfx = pd.count;
            if (cptfx == 0)
            {
                break;
            }

            if (pd.flags & PD_BEGINSUBPATH)
            {
                ptfxStartFigure  = *pd.pptfx;
                pptfxFirst       = pd.pptfx;
                pptfxBuf         = pd.pptfx + 1;
                cptfx--;
            }
            else
            {
                pptfxFirst       = &ptfxLast;
                pptfxBuf         = pd.pptfx;
            }

            if (cptfx > 0)
            {
                if (!bLines3D(ppdev,
                            pptfxFirst,
                            pptfxBuf,
                            (RUN*) NULL,
                            cptfx,
                            &ls,
                            prclClip,
                            apfn,
                            fl,
                            ulHwMix))
                    return(FALSE);
            }

            ptfxLast = pd.pptfx[pd.count - 1];

            if (pd.flags & PD_CLOSEFIGURE)
            {
                if (!bLines3D(ppdev,
                            &ptfxLast,
                            &ptfxStartFigure,
                            (RUN*) NULL,
                            1,
                            &ls,
                            prclClip,
                            apfn,
                            fl,
                            ulHwMix))
                    return(FALSE);
            }
        } while (bMore);

    }
    else
    {
    // Local state for path enumeration:

        BOOL bMore;
        union {
            BYTE     aj[offsetof(CLIPLINE, arun) + RUN_MAX * sizeof(RUN)];
            CLIPLINE cl;
        } cl;

        fl |= FL_COMPLEX_CLIP;

    // We use the clip object when non-simple clipping is involved:

        PATHOBJ_vEnumStartClipLines(ppo, pco, pso, pla);

        do {
            bMore = PATHOBJ_bEnumClipLines(ppo, sizeof(cl), &cl.cl);
            if (cl.cl.c != 0)
            {
                if (!bLines3D(ppdev,
                            &cl.cl.ptfxA,
                            &cl.cl.ptfxB,
                            &cl.cl.arun[0],
                            cl.cl.c,
                            &ls,
                            (RECTL*) NULL,
                            apfn,
                            fl,
                            ulHwMix))
                    return(FALSE);
            }
        } while (bMore);
    }

    return(TRUE);
}

