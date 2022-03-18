//******************************Module*Header*********************************
// Module Name: Strips.c
//
// Contains the low level line drawing routines called by DrvStrokePath
//
// Copyright (c) 1992-1994  Microsoft Corporation
// Copyright (c) 1996       S3 Inc.
//****************************************************************************

#include "precomp.h"
#include "hw3D.h"

/******************************Public*Routine******************************\
* VOID vrlSolidHorizontal3D
*
* Draws left-to-right x-major near-horizontal lines using the blt engine.
*
\**************************************************************************/

VOID vrlSolidHorizontal3D(
PDEV*       ppdev,
STRIP*      pStrip,
ULONG       ulHwForeMix)
{
    LONG    cStrips;
    ULONG   Cmd;
    LONG    i, yInc, x, y;
    PLONG   pStrips;

    Cmd = ppdev->ulCommandBase           |
          S3D_AUTOEXECUTE                |
          S3D_BITBLT                     |
          S3D_MONOCHROME_PATTERN         |
          (ulHwForeMix << S3D_ROP_SHIFT) |
          S3D_X_POSITIVE_BLT             |
          S3D_Y_POSITIVE_BLT             |
          S3D_DRAW_ENABLE;

//
// Send the command to the accelerator.  Remember, we have autoexecute on
// so the command isn't executed until we write the destination_x_y register.
//
    S3DFifoWait(ppdev, 1);
    S3writeHIU(ppdev, S3D_BLT_COMMAND, Cmd);

    cStrips = pStrip->cStrips;

    x = pStrip->ptlStart.x;
    y = pStrip->ptlStart.y;

    yInc = 1;
    if (pStrip->flFlips & FL_FLIP_V)
        yInc = -1;

    pStrips = pStrip->alStrips;

    for (i = 0; i < cStrips; i++)
    {
        S3DFifoWait(ppdev, 2);

        S3writeHIU(ppdev, S3D_BLT_WIDTH_HEIGHT, (((*pStrips-1) << 16) | 1));
        S3writeHIU(ppdev, S3D_BLT_DESTINATION_X_Y,
                    ((x << 16) | y));

        x += *pStrips++;
        y += yInc;
    }

    pStrip->ptlStart.x = x;
    pStrip->ptlStart.y = y;

//
// Stop the autoexecute feature of the accelerator
//
    S3DFifoWait(ppdev, 1);
    S3writeHIU(ppdev, S3D_BLT_COMMAND, S3D_NOP);

//
// Temporary fix for the first silicon of ViRGE
//
// After a Line, Rectangle, and Polygon command, the engine is in an
// unstable state.  As a workaround, do a small screen to screen blt.
//
}

/******************************Public*Routine******************************\
* VOID vrlSolidVertical3D
*
* Draws left-to-right y-major near-vertical lines using the blt engine.
*
\**************************************************************************/

VOID vrlSolidVertical3D(
PDEV*       ppdev,
STRIP*      pStrip,
ULONG       ulHwForeMix)
{
    LONG    cStrips;
    ULONG   Cmd;
    LONG    i, x, y;
    PLONG   pStrips;

    cStrips = pStrip->cStrips;
    pStrips = pStrip->alStrips;

    x = pStrip->ptlStart.x;
    y = pStrip->ptlStart.y;


    if (!(pStrip->flFlips & FL_FLIP_V))
    {
        Cmd = ppdev->ulCommandBase           |
              S3D_AUTOEXECUTE                |
              S3D_BITBLT                     |
              S3D_MONOCHROME_PATTERN         |
              (ulHwForeMix << S3D_ROP_SHIFT) |
              S3D_X_POSITIVE_BLT             |
              S3D_Y_POSITIVE_BLT             |
              S3D_DRAW_ENABLE;

    //
    // Send the command to the accelerator.  Remember, we have autoexecute on
    // so the command isn't executed until we write the destination_x_y register.
    //
        S3DFifoWait(ppdev, 1);
        S3writeHIU(ppdev, S3D_BLT_COMMAND, Cmd);

        for (i = 0; i < cStrips; i++)
        {
            S3DFifoWait(ppdev, 2);
            S3writeHIU(ppdev, S3D_BLT_WIDTH_HEIGHT, 0 | *pStrips);
            S3writeHIU(ppdev, S3D_BLT_DESTINATION_X_Y,
                        ((x << 16) | y));

            y += *pStrips++;
            x++;
        }

    }
    else
    {
        Cmd = ppdev->ulCommandBase           |
              S3D_AUTOEXECUTE                |
              S3D_BITBLT                     |
              S3D_MONOCHROME_PATTERN         |
              (ulHwForeMix << S3D_ROP_SHIFT) |
              S3D_X_POSITIVE_BLT             |
              S3D_Y_NEGATIVE_BLT             |
              S3D_DRAW_ENABLE;

    //
    // Send the command to the accelerator.  Remember, we have autoexecute on
    // so the command isn't executed until we write the destination_x_y register.
    //
        S3DFifoWait(ppdev, 1);
        S3writeHIU(ppdev, S3D_BLT_COMMAND, Cmd);

        for (i = 0; i < cStrips; i++)
        {
            S3DFifoWait(ppdev, 2);
            S3writeHIU(ppdev, S3D_BLT_WIDTH_HEIGHT, 0 | *pStrips);
            S3writeHIU(ppdev, S3D_BLT_DESTINATION_X_Y,
                        ((x << 16) | y));

            y -= *pStrips++;
            x++;
        }
    }

    pStrip->ptlStart.x = x;
    pStrip->ptlStart.y = y;

//
// Stop the autoexecute feature of the accelerator
//
    S3DFifoWait(ppdev, 1);
    S3writeHIU(ppdev, S3D_BLT_COMMAND, S3D_NOP);

//
// Temporary fix for the first silicon of ViRGE
//
// After a Line, Rectangle, and Polygon command, the engine is in an
// unstable state.  As a workaround, do a small screen to screen blt.
//
}

/******************************Public*Routine******************************\
* VOID vrlSolidDiagonalHorizontal3D
*
* Draws left-to-right x-major near-diagonal lines using radial lines.
*
\**************************************************************************/

VOID vrlSolidDiagonalHorizontal3D(
PDEV*       ppdev,
STRIP*      pStrip,
ULONG       ulHwForeMix)
{
    LONG    cStrips;
    ULONG   Cmd;
    LONG    i, x, y;
    PLONG   pStrips;

    cStrips = pStrip->cStrips;
    pStrips = pStrip->alStrips;

    x = pStrip->ptlStart.x;
    y = pStrip->ptlStart.y;

    if (!(pStrip->flFlips & FL_FLIP_V))
    {
    //
    // 315 degree diagonal line
    //
        Cmd = ppdev->ulCommandBase           |
              S3D_LINE_2D                    |
              S3D_MONOCHROME_PATTERN         |
              (ulHwForeMix << S3D_ROP_SHIFT) |
              S3D_DRAW_ENABLE;


        for (i = 0; i < cStrips; i++)
        {
            register LONG adjust;

            adjust = *pStrips - 1;

            S3DFifoWait(ppdev, 5);
            S3writeHIU(ppdev, S3D_2D_LINE_XDELTA, (ULONG)-(1 << 20 ));
            S3writeHIU(ppdev, S3D_2D_LINE_XSTART, (x + adjust) << 20);
            S3writeHIU(ppdev, S3D_2D_LINE_XEND_0_1, ((x + adjust) << 16) | x);
            S3writeHIU(ppdev, S3D_2D_LINE_YSTART, y + adjust);
            S3writeHIU(ppdev, S3D_2D_LINE_YCOUNT,
                       *pStrips | S3D_2D_LINE_X_NEGATIVE);
            S3DGPWait(ppdev);
            S3writeHIU(ppdev, S3D_2D_LINE_COMMAND, Cmd);

            y += *pStrips - 1;
            x += *pStrips++;
        }

    }
    else
    {
    //
    // 45 degree diagonal line
    //
        Cmd = ppdev->ulCommandBase           |
              S3D_LINE_2D                    |
              S3D_MONOCHROME_PATTERN         |
              (ulHwForeMix << S3D_ROP_SHIFT) |
              S3D_DRAW_ENABLE;

        for (i = 0; i < cStrips; i++)
        {
            register LONG adjust;

            adjust = *pStrips - 1;

            S3DFifoWait(ppdev, 5);
            S3writeHIU(ppdev, S3D_2D_LINE_XDELTA, (1 << 20 ));
            S3writeHIU(ppdev, S3D_2D_LINE_XSTART, x << 20);
            S3writeHIU(ppdev, S3D_2D_LINE_XEND_0_1, (x << 16) | x + adjust);
            S3writeHIU(ppdev, S3D_2D_LINE_YSTART, y);
            S3writeHIU(ppdev, S3D_2D_LINE_YCOUNT,
                       adjust + 1 | S3D_2D_LINE_X_POSITIVE);
            S3DGPWait(ppdev);
            S3writeHIU(ppdev, S3D_2D_LINE_COMMAND, Cmd);

            y -= adjust;
            x += *pStrips++;
        }
    }

    pStrip->ptlStart.x = x;
    pStrip->ptlStart.y = y;

//
// Temporary fix for the first silicon of ViRGE
//
// After a Line, Rectangle, and Polygon command, the engine is in an
// unstable state.  As a workaround, do a small screen to screen blt.
//

#ifdef VIRGE_PATCH11

    FIX_ViRGE(ppdev);

#endif

}

/******************************Public*Routine******************************\
* VOID vrlSolidDiagonalVertical3D
*
* Draws left-to-right y-major near-diagonal lines using radial lines.
*
\**************************************************************************/

VOID vrlSolidDiagonalVertical3D(
PDEV*       ppdev,
STRIP*      pStrip,
ULONG       ulHwForeMix)
{
    LONG    cStrips;
    ULONG   Cmd;
    LONG    i, x, y;
    PLONG   pStrips;

    cStrips = pStrip->cStrips;
    pStrips = pStrip->alStrips;

    x = pStrip->ptlStart.x;
    y = pStrip->ptlStart.y;

    if (!(pStrip->flFlips & FL_FLIP_V))
    {
    //
    // 315 degree diagonal line
    //
        Cmd = ppdev->ulCommandBase           |
              S3D_LINE_2D                    |
              S3D_MONOCHROME_PATTERN         |
              (ulHwForeMix << S3D_ROP_SHIFT) |
              S3D_DRAW_ENABLE;

        for (i = 0; i < cStrips; i++)
        {
            register LONG adjust;

            adjust = *pStrips - 1;

            S3DFifoWait(ppdev, 5);
            S3writeHIU(ppdev, S3D_2D_LINE_XDELTA, (ULONG)-(1 << 20 ));
            S3writeHIU(ppdev, S3D_2D_LINE_XSTART, (ULONG)(x + adjust) << 20);
            S3writeHIU(ppdev, S3D_2D_LINE_XEND_0_1, ((x + adjust) << 16) | x);
            S3writeHIU(ppdev, S3D_2D_LINE_YSTART, y + adjust);
            S3writeHIU(ppdev, S3D_2D_LINE_YCOUNT,
                       *pStrips | S3D_2D_LINE_X_NEGATIVE);
            S3DGPWait(ppdev);
            S3writeHIU(ppdev, S3D_2D_LINE_COMMAND, Cmd);

            y += *pStrips;
            x += *pStrips++ - 1;
        }

    }
    else
    {
    //
    // 45 degree diagonal line
    //
        Cmd = ppdev->ulCommandBase           |
              S3D_LINE_2D                    |
              S3D_MONOCHROME_PATTERN         |
              (ulHwForeMix << S3D_ROP_SHIFT) |
              S3D_DRAW_ENABLE;

        for (i = 0; i < cStrips; i++)
        {
            register LONG adjust;

            adjust = *pStrips - 1;

            S3DFifoWait(ppdev, 5);
            S3writeHIU(ppdev, S3D_2D_LINE_XDELTA, (1 << 20 ));
            S3writeHIU(ppdev, S3D_2D_LINE_XSTART, x << 20);
            S3writeHIU(ppdev, S3D_2D_LINE_XEND_0_1, (x << 16) | x + adjust);
            S3writeHIU(ppdev, S3D_2D_LINE_YSTART, y);
            S3writeHIU(ppdev, S3D_2D_LINE_YCOUNT,
                       adjust + 1 | S3D_2D_LINE_X_POSITIVE);
            S3DGPWait(ppdev);
            S3writeHIU(ppdev, S3D_2D_LINE_COMMAND, Cmd);

            y -= *pStrips;
            x += *pStrips++ - 1;
        }
    }


    pStrip->ptlStart.x = x;
    pStrip->ptlStart.y = y;

//
// Temporary fix for the first silicon of ViRGE
//
// After a Line, Rectangle, and Polygon command, the engine is in an
// unstable state.  As a workaround, do a small screen to screen blt.
//

#ifdef VIRGE_PATCH11

    FIX_ViRGE(ppdev);

#endif

}


