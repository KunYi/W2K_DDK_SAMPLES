
/******************************Module*Header*******************************\
* Module Name: textout.c
*
* On every TextOut, GDI provides an array of 'GLYPHPOS' structures
* for every glyph to be drawn.  Each GLYPHPOS structure contains a
* glyph handle and a pointer to a monochrome bitmap that describes
* the glyph.  (Note that unlike Windows 3.1, which provides a column-
* major glyph bitmap, Windows NT always provides a row-major glyph
* bitmap.)  As such, there are three basic methods for drawing text
* with hardware acceleration:
*
* 1) Glyph caching -- Glyph bitmaps are cached by the accelerator
*       (probably in off-screen memory), and text is drawn by
*       referring the hardware to the cached glyph locations.
*
* 2) Glyph expansion -- Each individual glyph is colour-expanded
*       directly to the screen from the monochrome glyph bitmap
*       supplied by GDI.
*
* 3) Buffer expansion -- The CPU is used to draw all the glyphs into
*       a 1bpp monochrome bitmap, and the hardware is then used
*       to colour-expand the result.
*
* The fastest method depends on a number of variables, such as the
* colour expansion speed, bus speed, CPU speed, average glyph size,
* and average string length.
*
* For the S3 with normal sized glyphs, I've found that caching the
* glyphs in off-screen memory is typically the slowest method.
* Buffer expansion is typically fastest on the slow ISA bus (or when
* memory-mapped I/O isn't available on the x86), and glyph expansion
* is best on fast buses such as VL and PCI.
*
* Glyph expansion is typically faster than buffer expansion for very
* large glyphs, even on the ISA bus, because less copying by the CPU
* needs to be done.  Unfortunately, large glyphs are pretty rare.
*
* An advantange of the buffer expansion method is that opaque text will
* never flash -- the other two methods typically need to draw the
* opaquing rectangle before laying down the glyphs, which may cause
* a flash if the raster is caught at the wrong time.
*
* This driver implements glyph expansion and buffer expansion --
* methods 2) and 3).  Depending on the hardware capabilities at
* run-time, we'll use whichever one will be faster.
*
* Copyright (c) 1992-1996 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.h"

 BYTE gajBit[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
                                // Converts bit index to set bit

#define     FIFTEEN_BITS        ((1 << 15)-1)


/******************************Public*Routine******************************\
* VOID vFreeCachedFont()
*
* Frees all memory associated with the cache we kept for this font.
*
\**************************************************************************/

VOID vFreeCachedFont(
CACHEDFONT* pcf)
{
    GLYPHALLOC* pga;
    GLYPHALLOC* pgaNext;


    pga = pcf->pgaChain;
    while (pga != NULL)
    {
        pgaNext = pga->pgaNext;
        EngFreeMem(pga);
        pga = pgaNext;
    }

    EngFreeMem(pcf);
}

/******************************Public*Routine******************************\
* VOID vAssertModeText
*
* Disables or re-enables the text drawing subcomponent in preparation for
* full-screen entry/exit.
*
\**************************************************************************/

//VOID vAssertModeText(
//PDEV*   ppdev,
//BOOL    bEnable)
//{
//    // If we were to do off-screen glyph caching, we would probably want
//    // to invalidate our cache here, because it will get destroyed when
//    // we switch to full-screen.
//}

/******************************Public*Routine******************************\
* VOID DrvDestroyFont
*
* Note: Don't forget to export this call in 'enable.c', otherwise you'll
*       get some pretty big memory leaks!
*
* We're being notified that the given font is being deallocated; clean up
* anything we've stashed in the 'pvConsumer' field of the 'pfo'.
*
\**************************************************************************/

VOID DrvDestroyFont(
FONTOBJ*    pfo)
{
    CACHEDFONT* pcf;

    pcf = pfo->pvConsumer;
    if (pcf != NULL)
    {
        vFreeCachedFont(pcf);
        pfo->pvConsumer = NULL;
    }
}

