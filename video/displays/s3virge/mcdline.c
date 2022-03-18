/******************************Module*Header*******************************\
* Module Name: mcdline.c
*
* Contains all of the line-rendering routines for the S3 Virge MCD driver.
*
* Copyright (c) 1996,1997 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"

#if SUPPORT_MCD

#include "hw3d.h"
#include "mcdhw.h"
#include "mcdmath.h"
#include "mcdutil.h"

#define IABS(x)   ((x)>0?(x):-(x))



//*************************************************************************
//
// VOID FASTCALL __MCDRenderFlatFogLine
//
// HW Render a single fogged line
//
//**************************************************************************
VOID FASTCALL __MCDRenderFlatFogLine(DEVRC *pRc, MCDVERTEX *pv1,
                                     MCDVERTEX *pv2, BOOL resetLine)
{
    MCDCOLOR c1, c2;

    c1 = pv1->colors[0];
    c2 = pv2->colors[0];
    __MCDCalcFogColor(pRc, pv1, &pv1->colors[0], &c1);
    __MCDCalcFogColor(pRc, pv2, &pv2->colors[0], &c2);
    (*pRc->renderLineX)(pRc, pv1, pv2, resetLine);
    pv1->colors[0] = c1;
    pv2->colors[0] = c2;
}

//*************************************************************************
//
// VOID FASTCALL __MCDClip_n_RenderHWLine
//
// Render a line, clipping it if neccesary against the left window edge
//
// Determines if a line can be rendered as it is, or if it is
// necessary to clip it against the window's left edge when the line's
// x coordinates become negative (which is not appropriately handled by hw)
//
//**************************************************************************
VOID FASTCALL __MCDClip_n_RenderHWLine(DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                                       BOOL resetLine)
{
    MCDVERTEX nv;
    BOOL aclip, bclip;
    PDEV *ppdev = pRc->ppdev;   

    // Due to hw limitations clipping neg. x values, we will clip by sw
    // No extra clipping is performed if the window is not crossing the
    // left screen edge
    if (pRc->pMCDSurface->pWnd->clientRect.left >= 0) {
        (*ppdev->mcdHWLine)(pRc, a, b, resetLine);
        return;
    }

    // Otherwise, check coordinates of line to see if they
    // cross the left edge
    aclip = a->windowCoord.x + (long)pRc->xOffset < 0;
    bclip = b->windowCoord.x + (long)pRc->xOffset < 0;

    // no clipping needed
    if (!(aclip || bclip))
        (*ppdev->mcdHWLine)(pRc, a, b, resetLine);

    // full line is clipped out
    if (aclip && bclip)
        return;

    if (aclip) {
        __MCDClip2Vert(pRc, &nv, b, a);
        (*ppdev->mcdHWLine)(pRc, &nv, b, resetLine);
    }

    if (bclip) {
        __MCDClip2Vert(pRc, &nv, a, b);
        (*ppdev->mcdHWLine)(pRc, a, &nv, resetLine);
    }
}

//*************************************************************************
//
// VOID FASTCALL __MCDLineBegin
//
// Setup S3Virge registers before starting to draw a batch of lines
//
//*************************************************************************
VOID FASTCALL __MCDLineBegin(DEVRC *pRc)
{
}

//*************************************************************************
//
// VOID FASTCALL __MCDLineBegin
//
// Setup S3Virge registers after ending the draw of a batch of lines
//
//**************************************************************************
VOID FASTCALL __MCDLineEnd(DEVRC *pRc)
{
}

//**************************************************************************
// VOID FASTCALL __MCDRenderFlatLine
//
// Render a flat line
//
// We are sharing a function to draw flat and smoothed lines in this
// implementation but we have left this function call if you want to have
// them separated
//
//**************************************************************************
VOID FASTCALL __MCDRenderFlatLine(DEVRC *pRc, MCDVERTEX *a,
                                  MCDVERTEX *b, BOOL resetLine)
{
    __MCDClip_n_RenderHWLine(pRc, a, b, resetLine);
}

//**************************************************************************
// VOID FASTCALL __MCDRenderSmoothLine
//
// Render a smooth line
//
// We are sharing a function to draw flat and smoothed lines in this
// implementation but we have left this function call if you want to have
// them separated
//
//**************************************************************************
VOID FASTCALL __MCDRenderSmoothLine(DEVRC *pRc, MCDVERTEX *a,
                                    MCDVERTEX *b, BOOL resetLine)
{
    __MCDClip_n_RenderHWLine(pRc, a, b, resetLine);
}

#endif

