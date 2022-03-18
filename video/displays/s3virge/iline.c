//****************************************************************************
// Module Name: iline.c
//
// Handles drawing lines with integer endpoints.
//
// Copyright (c) 1993-1994 Microsoft Corporation
// Copyright (c) 1992      Digital Equipment Corporation
// Copyright (c) 1996      S3 Incorporated
//****************************************************************************

#include "precomp.h"
#include "hw3D.h"

LONG xAdjust = -513;

//----------------------------------------------------------------------------
// PROCEDURE:   bIntegerLine3D:
//
// DESCRIPTION: This routine attempts to draw a line segment between two
//              points. It will only draw if both end points are whole
//              integers: it does not support fractional endpoints.
//
// ASSUMPTION:
//
// CALLS
//
// PARAMETERS:  PDEV*   ppdev       pointer to display's pdevice
//              ULONG   X1          starting X
//              ULONG   Y1          starting Y
//              ULONG   X2          ending X
//              ULONG   Y2          ending Y
//              ULONG   ulHwMix     draw command mix
//
// RETURN:      TRUE    line segment was drawn
//              FALSE   line segment was not drawn
//
// NOTES:
//
//----------------------------------------------------------------------------
//
BOOL
bIntegerLine3D(
PDEV*   ppdev,
ULONG   X1,
ULONG   Y1,
ULONG   X2,
ULONG   Y2,
ULONG   ulHwMix
)
{
    LONG     Cmd;
    LONG     DeltaX, DeltaY;
    LONG     xStart;
    LONG     xDelta;
    ULONG    fl = 0;


//
// The coordinates for the line are passed in as 28.4 fixed point numbers.
// Convert the fixed point numbers to integers.  All coordinate fractional
// bits are known to be 0.
//
    X1 >>= 4;
    Y1 >>= 4;
    X2 >>= 4;
    Y2 >>= 4;

    Cmd = ppdev->ulCommandBase           |
          S3D_LINE_2D                    |
          S3D_MONOCHROME_PATTERN         |
          (ulHwMix << S3D_ROP_SHIFT)     |
          S3D_DRAW_ENABLE;


    if (Y1 > Y2)   // If we're already drawing from bottom to top
    {
    //
    // Compute the Deltas
    //
        DeltaX = X1 - X2;
        DeltaY = Y1 - Y2;

        if (DeltaY != 0)
        {
            xDelta = -((DeltaX << 20) / DeltaY);
        }
        else
        {
            xDelta = 0;
        }

        if (X1 < X2)
        {
        //
        // The drawing direction is left to right
        //
            fl |= S3D_2D_LINE_X_POSITIVE;

            if ( -DeltaX > DeltaY )
            {
            //
            // X-major line
            //
                xStart = (X1 << 20) + xDelta / 2;
                xStart--;

            //
            // Since we can't tell the accelerator not to draw the last pixel,
            // we need to adjust the coordinate
            //
                X2--;
            }
            else
            {
            //
            // Y-major line
            //
                xStart = (X1 << 20) + ((1 << 20) / 2);
                xStart--;

            //
            // Adjusting the x coordinate will also clip y-major lines.  Its
            // a strange artifact of the fixed point algorithm used by the
            // hardware.
            //
                X2--;
            }
        }
        else
        {
        //
        // The drawing direction is right to left
        //
            fl |= S3D_2D_LINE_X_NEGATIVE;

            if ( DeltaX > DeltaY )
            {
            //
            // X-major line
            //
                xStart = (X1 << 20) + xDelta / 2;

            //
            // Since we can't tell the accelerator not to draw the last pixel,
            // we need to adjust the coordinate
            //
                X2++;

            //
            // Adjust xStart by 1 pixel to the right due to the Floor(xStart)
            // function used by the hardware.
            //
                xStart += (1 << 20);
            }
            else
            {
            //
            // Y-major line
            //
                xStart = (X1 << 20) + ((1 << 20) / 2);

            //
            // Adjusting the x coordinate will also clip y-major lines.  Its
            // a strange artifact of the fixed point algorithm used by the
            // hardware.
            //
                X2++;

            //
            // Adjust xStart by almost 1 pixel to the right
            //
            //xStart += (1 << 20);
            //
                xStart += xAdjust;
            }
        }

    //
    // Adjust Delta Y by 1 to get the correct drawing height
    //
        DeltaY++;

    //
    // Check to see if we're running 24bpp.  If we are, we need to tiptoe
    // around a bug.
    //
        if (ppdev->iBitmapFormat == BMF_24BPP)
        {
        //
        // Do an XOR operation on our flag to see if we have a != condition
        //
//            if ((ppdev->ulLastLineDirection ^ fl) != 0)
            {
            //
            // We have changed drawing directions.  Due to a bug in ViRGE,
            // if we don't clip away the next line, we'll end up with color
            // problems for a few pixels on the line.
            //
                S3DFifoWait(ppdev, 8);
                S3writeHIU(ppdev, S3D_2D_LINE_XDELTA, xDelta);
                S3writeHIU(ppdev, S3D_2D_LINE_XSTART, xStart);
                S3writeHIU(ppdev, S3D_2D_LINE_XEND_0_1, (X2 << 16) | X1);
                S3writeHIU(ppdev, S3D_2D_LINE_YSTART, Y2);
                S3writeHIU(ppdev, S3D_2D_LINE_YCOUNT, DeltaY | fl);

                if (fl)
                {
                    S3writeHIU(ppdev, S3D_2D_LINE_CLIP_LEFT_RIGHT, X1 - 1);
                    S3writeHIU(ppdev, S3D_2D_LINE_CLIP_TOP_BOTTOM, 1L);
                }
                else
                {
                    S3writeHIU(ppdev, S3D_2D_LINE_CLIP_LEFT_RIGHT, (X1 + 1) << 16 | X1 + 10);
                    S3writeHIU(ppdev, S3D_2D_LINE_CLIP_TOP_BOTTOM, 1L);
                }
                S3writeHIU(ppdev, S3D_2D_LINE_COMMAND, Cmd | S3D_HARDWARE_CLIPPING);
            }

            ppdev->ulLastLineDirection = fl;

        }

    //
    // Send the values to the accelerator to draw the line
    //
        S3DFifoWait(ppdev, 6);
        S3writeHIU(ppdev, S3D_2D_LINE_XDELTA, xDelta);
        S3writeHIU(ppdev, S3D_2D_LINE_XSTART, xStart);
        S3writeHIU(ppdev, S3D_2D_LINE_XEND_0_1, (X1 << 16) | X2);
        S3writeHIU(ppdev, S3D_2D_LINE_YSTART, Y1);
        S3writeHIU(ppdev, S3D_2D_LINE_YCOUNT, DeltaY | fl);
        S3writeHIU(ppdev, S3D_2D_LINE_COMMAND, Cmd);
    }
    else  // We're drawing from top to bottom
    {
    //
    // Compute the Deltas
    //
        DeltaX = X2 - X1;
        DeltaY = Y2 - Y1;

        if (DeltaY != 0)
        {
            xDelta = -((DeltaX << 20) / DeltaY);
        }
        else
        {
            xDelta = 0;
        }

        if (X2 < X1)
        {
        //
        // The drawing direction is left to right
        //
            fl |= S3D_2D_LINE_X_POSITIVE;

            if ( -DeltaX > DeltaY )
            {
            //
            // X-major line
            //
                xStart = (X2 << 20) + xDelta / 2;
                xStart--;

            //
            // Since we can't tell the accelerator not to draw the last pixel,
            // we need to adjust the coordinate
            //
                X2++;
            }
            else
            {
            //
            // Y-major line
            //
                xStart = (X2 << 20) + ((1 << 20) / 2);
                xStart--;

            //
            // Adjusting the x coordinate will also clip y-major lines.  Its
            // a strange artifact of the fixed point algorithm used by the
            // hardware.
            //
                X2++;
            }
        }
        else
        {
        //
        // The drawing direction is right to left
        //
            fl |= S3D_2D_LINE_X_NEGATIVE;

            if ( DeltaX > DeltaY )
            {
            //
            // X-major line
            //
                xStart = (X2 << 20) + xDelta / 2;

            //
            // Since we can't tell the accelerator not to draw the last pixel,
            // we need to adjust the coordinate
            //
                X2--;

            //
            // Adjust xStart by almost 1 pixel to the right
            //
                xStart += (1 << 20);
            }
            else
            {
            //
            // Y-major line
            //
                xStart = (X2 << 20) + ((1 << 20) / 2);

            //
            // Adjusting the x coordinate will also clip y-major lines.  Its
            // a strange artifact of the fixed point algorithm used by the
            // hardware.
            //
                X2--;

            //
            // Adjust xStart by almost 1 pixel to the right
            //
            //  xStart += (1 << 20);
            //
                xStart += xAdjust;
            }
        }
    //
    // Adjust Delta Y by 1 to get the correct drawing height
    //
        DeltaY++;

    //
    // Check to see if we're running 24bpp.  If we are, we need to tiptoe
    // around a bug.
    //
        if (ppdev->iBitmapFormat == BMF_24BPP)
        {
        //
        // Do an XOR operation on our flag to see if we have a != condition
        //
//            if ((ppdev->ulLastLineDirection ^ fl) != 0)
            {
            //
            // We have changed drawing directions.  Due to a bug in ViRGE,
            // if we don't clip away the next line, we'll end up with color
            // problems for a few pixels on the line.
            //
                S3DFifoWait(ppdev, 8);
                S3writeHIU(ppdev, S3D_2D_LINE_XDELTA, xDelta);
                S3writeHIU(ppdev, S3D_2D_LINE_XSTART, xStart);
                S3writeHIU(ppdev, S3D_2D_LINE_XEND_0_1, (X2 << 16) | X1);
                S3writeHIU(ppdev, S3D_2D_LINE_YSTART, Y2);
                S3writeHIU(ppdev, S3D_2D_LINE_YCOUNT, DeltaY | fl);

                if (fl)
                {
                    S3writeHIU(ppdev, S3D_2D_LINE_CLIP_LEFT_RIGHT, X1 - 1);
                    S3writeHIU(ppdev, S3D_2D_LINE_CLIP_TOP_BOTTOM, 1L);
                }
                else
                {
                    S3writeHIU(ppdev, S3D_2D_LINE_CLIP_LEFT_RIGHT, (X1 + 1) << 16 | X1 + 10);
                    S3writeHIU(ppdev, S3D_2D_LINE_CLIP_TOP_BOTTOM, 1L);
                }
                S3writeHIU(ppdev, S3D_2D_LINE_COMMAND, Cmd | S3D_HARDWARE_CLIPPING);
            }

            ppdev->ulLastLineDirection = fl;

        }

    //
    // Send the values to the accelerator to draw the line
    //
        S3DFifoWait(ppdev, 6);
        S3writeHIU(ppdev, S3D_2D_LINE_XDELTA, xDelta);
        S3writeHIU(ppdev, S3D_2D_LINE_XSTART, xStart);
        S3writeHIU(ppdev, S3D_2D_LINE_XEND_0_1, (X2 << 16) | X1);
        S3writeHIU(ppdev, S3D_2D_LINE_YSTART, Y2);
        S3writeHIU(ppdev, S3D_2D_LINE_YCOUNT, DeltaY | fl);
        S3writeHIU(ppdev, S3D_2D_LINE_COMMAND, Cmd);
    }

//
// After a Line, Rectangle, and Polygon command, the engine is in an
// unstable state.  As a workaround, do a small screen to screen blt.
//

    //
    // fix SPR 16806, HCT failure in path01.ver and path02.ver (WinGDI)
    //

    S3DGPWait( ppdev );
    FIX_ViRGE(ppdev);

    return TRUE;

}


