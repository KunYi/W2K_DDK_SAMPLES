/******************************Module*Header*******************************\
* Module Name: mcdtex.c
*
* Contains various utility routines for handling textures in the S3 Virge
* MCD driver such as texture loading, unloading and setting the appropriate
* state
*
* Copyright (c) 1996,1997 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"

#if SUPPORT_MCD

#include "hw3d.h"
#include <excpt.h>
#include "mcdhw.h"
#include "mcdmath.h"
#include "mcdutil.h"

#ifdef MCD95
#define TEXTURE_ALLOC_FLAGS     (FLOH_MAKE_PERMANENT | FLOH_TEXTURE)
#else
#define TEXTURE_ALLOC_FLAGS     (FLOH_MAKE_PERMANENT)
#endif

#define B_POWER_OF_2(lnum) (!(lnum & (lnum - 1)))

extern MCDFLOAT negPowerOfTwo [];

typedef LONG (*PTEXLOADFUNC)(DEVRC *pRc, PDEV *ppdev, DEVTEXTURE *pDevTexture,
                            ULONG level, RECTL *pRect);


//*************************************************************************
//
// LONG UpdateTexContextParm
//
// Validate and setup necessary state to handle the texturing parameters
// (wrapping modes, filters and border color)
//
//**************************************************************************
LONG UpdateTexContextParm(DEVRC *pRc, DEVTEXTURE *pDevTexture)
{
    PDEV *ppdev = pRc->ppdev;
    ULONG iColor;
    ULONG rBits, gBits, bBits;

    // Detect states which the HW cannot handle
    if (pDevTexture->sWrapMode != pDevTexture->tWrapMode) {
    MCDBG_PRINT(DBG_TEX, "Can't handle different wrapping modes for s and t"
                     " = %d %d", pDevTexture->sWrapMode,pDevTexture->tWrapMode);
    return FALSE;
    }

    // Handle GL_TEXTURE_WRAP_S parameter
    switch (pDevTexture->sWrapMode) {
        case GL_REPEAT:
            pRc->hwTexFunc |= S3D_TEXTURE_WRAP_ENABLE;
            break ;
        case GL_CLAMP:
                MCDBG_PRINT(DBG_TEX, "UpdateTexContextParm:HW Texture "
                                     "clamping not supported");
            return FALSE; // reject CLAMPING
        default:
                MCDBG_PRINT(DBG_ERR, "UpdateTexContextParm:s Wrap Mode Unknown %d",
                                     pDevTexture->tWrapMode);
            return FALSE;
    }

    // Handle GL_TEXTURE_WRAP_T parameter
    switch (pDevTexture->tWrapMode) {
        case GL_REPEAT:
            pRc->hwTexFunc |= S3D_TEXTURE_WRAP_ENABLE;
            break ;
        case GL_CLAMP:
                MCDBG_PRINT(DBG_TEX, "UpdateTexContextParm:HW Texture "
                                     "clamping not supported");
            return FALSE; // reject CLAMPING
        default:
                MCDBG_PRINT(DBG_ERR, "UpdateTexContextParm:t Wrap Mode "
                                     "Unknown %d",pDevTexture->tWrapMode);
            return FALSE;
    }

    // store GL_TEXTURE_MIN_FILTER so that later when we process triangles we
    // may choose between the minimization and the magnification filters
    pRc->texMinFilter = pDevTexture->minFilter;

    // store GL_TEXTURE_MAG_FILTER so that later when we process triangles we
    // may choose between the minimization and the magnification filters
    pRc->texMagFilter = pDevTexture->magFilter;

    // Handle GL_TEXTURE_BORDER_COLOR parameter
    rBits = ICLAMP(FTOL(pDevTexture->borderColor.r * pRc->rScale));
    gBits = ICLAMP(FTOL(pDevTexture->borderColor.g * pRc->gScale));
    bBits = ICLAMP(FTOL(pDevTexture->borderColor.b * pRc->bScale));
    switch(ppdev->iBitmapFormat) {
        case BMF_16BPP:
        iColor =(rBits & 0x7C00)      |
            (gBits & 0x7C00) >>  5|
            (bBits & 0x7C00) >> 10;
        break;
        case BMF_24BPP:
        iColor =(rBits & 0x7F80) <<  9 |
            (gBits & 0x7F80) <<  1 |
            (bBits & 0x7F80) >>  7;
        break;
        case BMF_8BPP:
        case BMF_32BPP:
        default:
            MCDBG_PRINT(DBG_ERR, "UpdateTexContextParm:S3Virge invalid "
                                 "3D bitmap format");
        return FALSE;
    }

    // Set the texture boder color

    S3DFifoWait(ppdev, 1);
    S3writeHIU(ppdev, S3D_TRI_TEXTURE_BORDER_COLOR, iColor);

    return TRUE;
}

//*************************************************************************
//
// LONG HWTexEnvState
//
// Validate and setup necessary state to handle the texturing function
// Here the S3Virge blending mode should be chosen and the cmd type
// (lit/unlit persp/no persp) should be selected
//
//**************************************************************************
LONG HWTexEnvState(DEVRC *pRc, DEVTEXTURE *pDevTexture)
{
    PDEV *ppdev = pRc->ppdev;

    switch(pRc->MCDTexEnvState.texEnvMode) {
    case GL_MODULATE:
        switch(pDevTexture->MipMapLevels[0].internalFormat) {
        case GL_ALPHA:
            MCDBG_PRINT(DBG_TEX, "HWTexEnvState: GL_MODULATE "
                                     "+ GL_ALPHA unsupported");
            return FALSE;
            break;
        case GL_INTENSITY:
        case GL_RGB:
        case GL_BGR_EXT:
            pRc->hwTexFunc |= S3D_TEXTURE_BLEND_MODULATE;
            pRc->hwTexFunc |= S3D_LIT_TEXTURED_TRIANGLE;
            break;
        case GL_LUMINANCE:
        case GL_LUMINANCE_ALPHA:
        case GL_RGBA:
        case GL_BGRA_EXT:
            pRc->hwTexFunc |= S3D_TEXTURE_BLEND_MODULATE;
            pRc->hwTexFunc |= S3D_LIT_TEXTURED_TRIANGLE;
            pRc->hwBlendFunc = S3D_ALPHA_BLEND_TEXTURE_ALPHA;
            break;
        default:
            MCDBG_PRINT(DBG_ERR, "HWTexEnvState: Unrecognizable "
                                     "internal format");
            return FALSE;
            break;
        }
        break;

    case GL_DECAL:
        // No need to generate colors, only texture
        pRc->privateEnables &= ~ __MCDENABLE_SMOOTH;
        switch(pDevTexture->MipMapLevels[0].internalFormat) {
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_LUMINANCE_ALPHA:
        case GL_INTENSITY:
        case GL_RGBA:
        case GL_RGB:
            MCDBG_PRINT(DBG_TEX, "HWTexEnvState: GL_DECAL only "
                                     "supported w/ GL_BGR_EXT ");
            return FALSE;
            break;
        case GL_BGRA_EXT: //. valid decal cases
            pRc->hwTexFunc |= S3D_TEXTURE_BLEND_DECAL;
            pRc->hwTexFunc |= S3D_UNLIT_TEXTURED_TRIANGLE;
            pRc->hwBlendFunc = S3D_ALPHA_BLEND_TEXTURE_ALPHA;
            break;
        case GL_BGR_EXT:
            pRc->hwTexFunc |= S3D_TEXTURE_BLEND_DECAL;
            pRc->hwTexFunc |= S3D_UNLIT_TEXTURED_TRIANGLE;
            break;
        default:
            MCDBG_PRINT(DBG_ERR, "HWTexEnvState: Unrecognizable "
                                     "internal format");
            return FALSE;
            break;
        }
        break;

    case GL_BLEND:
        MCDBG_PRINT(DBG_TEX, "HWTexEnvState: GL_BLEND unsupported");
        return FALSE; // Blending not possible on S3Virge

    case GL_REPLACE:
        // No need to generate colors, only texture
        // On S3V seems we need at least flat colors
        // so only disable smooth color generation!
        pRc->privateEnables &= ~ __MCDENABLE_SMOOTH;
        switch(pDevTexture->MipMapLevels[0].internalFormat) {
        case GL_ALPHA:
            MCDBG_PRINT(DBG_TEX, "HWTexEnvState: GL_REPLACE "
                                     "+ GL_ALPHA unsupported");
            return FALSE;
            break;
        case GL_INTENSITY:
        case GL_RGB:
        case GL_BGR_EXT:
            pRc->hwTexFunc |= S3D_TEXTURE_BLEND_DECAL;
            pRc->hwTexFunc |= S3D_UNLIT_TEXTURED_TRIANGLE;
            break;
        case GL_RGBA:
        case GL_LUMINANCE:
        case GL_LUMINANCE_ALPHA:
        case GL_BGRA_EXT:
            pRc->hwTexFunc |= S3D_TEXTURE_BLEND_DECAL;
            pRc->hwTexFunc |= S3D_UNLIT_TEXTURED_TRIANGLE;
            pRc->hwBlendFunc = S3D_ALPHA_BLEND_TEXTURE_ALPHA;
            break;
        default:
            MCDBG_PRINT(DBG_ERR, "HWTexEnvState: Unrecognizable "
                                     "internal format");
            return FALSE;
            break;
        }

        break;
    default:
        // This happens when an older(NT4) OPENGL32.DLL being used does not
        // have yet the fix for setting coorrectly the DBG_TEXENV_STATE. We
        // handle it this way in order to "fail graciously"
        MCDBG_PRINT(DBG_ERR,
                    "HWTexEnvState: Unrecognizable texturing function"
                     "defaulting to GL_DECAL to handle older (NT4)OPENGL32.DLL");
        pRc->hwTexFunc |= S3D_TEXTURE_BLEND_DECAL;
        pRc->hwTexFunc |= S3D_UNLIT_TEXTURED_TRIANGLE;
        break;
    }

    return TRUE;
}


//**************************************************************************
//                    TEXTURE MEMORY MANAGEMENT
//**************************************************************************

//**************************************************************************
// VOID TexListFree
//
// Delete texture from ppdev linked list of loadad textures
//
//**************************************************************************
VOID TexListFree(PDEV *ppdev, DEVTEXTURE *pDevTexture)
{
    DEVTEXTURE *pWrkDevTexture;

    // make pWrkDevTexture point to preceding texture
    pWrkDevTexture = pDevTexture;
    while (pWrkDevTexture->pNextDevTexture != pDevTexture)
    pWrkDevTexture = pWrkDevTexture->pNextDevTexture;

    //if they are the same, the list has only one element
    if (pWrkDevTexture == pDevTexture) {
    pDevTexture->pNextDevTexture = NULL;
    ppdev->pListDevTex = NULL;
    } else {
    pWrkDevTexture->pNextDevTexture = pDevTexture->pNextDevTexture;
    pDevTexture->pNextDevTexture = NULL;
    if (ppdev->pListDevTex == pDevTexture)
        ppdev->pListDevTex = pWrkDevTexture;
    }
}

//**************************************************************************
//
// VOID TexListInsert
//
// Update linked list in ppdev of loaded textures in S3Virge memory
//
//**************************************************************************
VOID TexListInsert(PDEV *ppdev, DEVTEXTURE *pDevTexture)
{
    DEVTEXTURE  *pWrkDevTexture, *pNextDevTexture;

    if (!ppdev->pListDevTex) {
    ppdev->pListDevTex = pDevTexture;
    pDevTexture->pNextDevTexture = pDevTexture;
    } else {
    pWrkDevTexture = ppdev->pListDevTex;
    pNextDevTexture = pWrkDevTexture->pNextDevTexture;
    pWrkDevTexture->pNextDevTexture = pDevTexture;
    pDevTexture->pNextDevTexture    = pNextDevTexture;
    }
}

//*************************************************************************
//
// VOID HWUnLoadTex
//
// This funtion unloads a specific texture from memory
//
//*************************************************************************
VOID HWUnLoadTex(PDEV *ppdev, DEVTEXTURE *pDevTexture)
{
#if DBG_TEX_WIPE_OUT

    ULONG tSize;

    //Wipe out the texture memory (fill with white texels)
#if TEX_NON_SQR
    tSize = pDevTexture->maxDim * pDevTexture->maxDim * 4;
#else
    tSize = pDevTexture->MipMapLevels[0].width *
        pDevTexture->MipMapLevels[0].width * 4;
#endif

#if TEX_ENABLE_MIP_MAPS
    // We will need the double of space if its a mipmapped texture
    if (pDevTexture->bMipMaps)
    tSize = tSize * 2 + 3;
#endif

    RtlFillMemory((BYTE *)(ppdev->pjScreen +
                 pDevTexture->MipMapLevels[0].BaseAddress), tSize, 0xFF0000FF);
#endif

    if (HWResidentTex( ppdev, pDevTexture ))
    {
        pohFree(ppdev, pDevTexture->MipMapLevels[0].pohMipMapTex);
    }

    MCDBG_PRINT(DBG_TEX, "Free texture = %x", pDevTexture);
    pDevTexture->MipMapLevels[0].pohMipMapTex = NULL;
    pDevTexture->MipMapLevels[0].BaseAddress  = 0;

    TexListFree(ppdev,pDevTexture); //update linked list of resident textures
}

//*************************************************************************
//
// VOID HWFreeAllTextureMemory
//
// This funtion frees all loaded textures in the S3Virge memory in
// order to enable any front/back/depth buffer reconfiguration
// which may be necessary.
//
//**************************************************************************
VOID HWFreeAllTextureMemory(PDEV *ppdev)
{
    DEVTEXTURE *pDevTexture;

    // wait for any potential use of textures to end
    HW_WAIT_DRAW_TEX(ppdev);

    while (pDevTexture = ppdev->pListDevTex)
        HWUnLoadTex(ppdev, pDevTexture);
}

//**************************************************************************
//
// LONG HWFreeAnyTexMemory
//
// This funtion frees some loaded texture which it sees fit in order
// to make space for a new texture.
//
// We use the priority field of pDevTexture (which is updated in
// MCDrvUpdateTexturePriority) to implement a priority driven texture heap.
//
// If we want to implement a different scheme (for example, an LRU scheme) ,
// this is basically the only function which we need to modify.
//
//**************************************************************************
LONG HWFreeAnyTexMemory(PDEV *ppdev)
{
    DEVTEXTURE *pDevTexture, *pWrkDevTexture;

    // wait for any potential use of textures to end
    HW_WAIT_DRAW_TEX(ppdev);

    // unload texture of lowest priority in the S3Virge memory
    if (pDevTexture = pWrkDevTexture = ppdev->pListDevTex) {
    do {
            pDevTexture = pDevTexture->pNextDevTexture;
        if ((pDevTexture->MipMapLevels[0].pohMipMapTex != NULL) &&
                (pDevTexture->priority < pWrkDevTexture->priority))
                pWrkDevTexture = pDevTexture;
    } while (pDevTexture!= ppdev->pListDevTex);
        HWUnLoadTex(ppdev, pWrkDevTexture);

        return TRUE;
    }

    return FALSE;
}

//**************************************************************************
//
// LONG HWFreeTexMemory
//
// We free the texture memory associated to the device texture
// if still present!
//
//**************************************************************************
LONG HWFreeTexMemory(PDEV *ppdev, DEVTEXTURE *pDevTexture)
{
    // wait for any potential use of the texture to end
    HW_WAIT_DRAW_TEX(ppdev);

    // unload any remaining instance of this texture in the S3Virge memory
    if (pDevTexture->MipMapLevels[0].pohMipMapTex != NULL) {
            HWUnLoadTex(ppdev, pDevTexture);
        return TRUE;   // succesful
    } else
        return TRUE;   // texture was already out of Tex memory
}

//**************************************************************************
//
// LONG HWResidentTex
//
// We check if the device texture is still resident
//
//**************************************************************************
LONG HWResidentTex(PDEV *ppdev, DEVTEXTURE *pDevTexture)
{
    DEVTEXTURE *pCurrentTexture;
    BOOL        fFound;

    pCurrentTexture = ppdev->pListDevTex;

    if (pCurrentTexture)
    {
        fFound = FALSE;

        do
        {
            if (pCurrentTexture == pDevTexture)
            {
                fFound = TRUE;
                break;
            }
            else
            {
                pCurrentTexture = pCurrentTexture->pNextDevTexture;
            }

        } while (pCurrentTexture != ppdev->pListDevTex);

        if (fFound)
        {
            if (pDevTexture->MipMapLevels[0].pohMipMapTex)
            {
                return( MCDRV_TEXTURE_RESIDENT );
            }
        }
    }

    return( FALSE );
}

//**************************************************************************
//                       TEXTURE DATA LOADING
//**************************************************************************

//**************************************************************************
//
// LONG HWTexOffset
//
// Calculate offset in texture memory to specified level of detail
// NOTE: Remember that in this implementation the hw requires all textures
//       to be square.
//
//**************************************************************************
LONG HWTexOffset(DEVTEXTURE* pDevTexture, ULONG level)
{
    ULONG offset, lod;

    offset = 0;

    for (lod=0; lod<level; lod++) {
        if (pDevTexture->MipMapLevels[lod].width >
            pDevTexture->MipMapLevels[lod].height)
            offset += pDevTexture->MipMapLevels[lod].width *
                      pDevTexture->MipMapLevels[lod].width * 4;
        else
            offset += pDevTexture->MipMapLevels[lod].height *
                      pDevTexture->MipMapLevels[lod].height * 4;
    }

    return offset;
}

//**************************************************************************
//
// LONG HWLoadLUMITexRegion
//
// Transfer a luminance texture region for a specific level into already
// allocated S3Virge memory. Right now we are updating the whole level
//
//**************************************************************************
LONG HWLoadLUMITexRegion(DEVRC *pRc, PDEV *ppdev, DEVTEXTURE *pDevTexture,
                         ULONG level, RECTL *pRect)
{
    ULONG offset, horzOffset;
    LONG tSize, tsizeBand, i, j, numCopies;
    BYTE *pTexScanLine, *pBGRAVal, lumVal;
#ifndef MCD95
    HANDLE hMem;
#endif
    MCDFLOAT *pLumiVal;

    // we need to setup a temporary buffer to
    // transfer the texture in BGRA format
    pTexScanLine = (BYTE *)MCDMemAlloc(pDevTexture->MipMapLevels[0].width * 4);
    if (pTexScanLine == NULL) {
    MCDBG_PRINT(DBG_ERR, "Could not allocate temporary buffer for "
                             "GL_LUMINANCE texture");
    return FALSE;
    }

//EngProbeForRead/EngSecureMem does not exist for Win95; wrap
// code block that reads texture memory with try/except.
#ifdef MCD95
    try {
#else
    // Probe memory to see if its safe to read, if not cause an exception
    try {
        EngProbeForRead((VOID *)pDevTexture->MipMapLevels[level].pTexels,
                         pDevTexture->MipMapLevels[level].width *
                         pDevTexture->MipMapLevels[level].height * 4,
                         1);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        MCDBG_PRINT(DBG_ERR,"HWLoadLUMITexRegion: Unsafe texture "
                            "data pointer for reading");
        MCDMemFree(pTexScanLine); //free temporary buffer
        return FALSE;
    }

    // Lock memory during transfer, if unable to then abort transfer
    hMem = EngSecureMem((VOID *)pDevTexture->MipMapLevels[level].pTexels,
                        pDevTexture->MipMapLevels[level].width *
                        pDevTexture->MipMapLevels[level].height * 4);
    if (!hMem) {
        MCDBG_PRINT(DBG_ERR,"HWLoadLUMITexRegion: Unsafe texture "
                            "data pointer for reading");
        MCDMemFree(pTexScanLine); //free temporary buffer
        return FALSE;
    }

#endif

    // Calculate offset in texture memory to specified level of detail
    offset = HWTexOffset(pDevTexture,level);


    // Since now the internal format is GL_LUMINANCE we
    // need to make any munging before transferring it to S3V memory

#if TEX_NON_SQR
    if (pDevTexture->MipMapLevels[level].width >
        pDevTexture->MipMapLevels[level].height) {
    //horizontal texture case
    tSize = pDevTexture->MipMapLevels[level].width * 4;
    tsizeBand = tSize * pDevTexture->MipMapLevels[level].height;

    numCopies = pDevTexture->MipMapLevels[level].width /
            pDevTexture->MipMapLevels[level].height;

    pLumiVal = (MCDFLOAT *)pDevTexture->MipMapLevels[level].pTexels;
    for(j=0; j<pDevTexture->MipMapLevels[level].height; j++) {

        pBGRAVal = pTexScanLine;
#if TEXLUMSPEC_BLEND_DKN
        if (pRc->privateEnables & __MCDENABLE_TEXLUMSPEC_BLEND_MODE_DKN)
        for (i=0; i<pDevTexture->MipMapLevels[level].width; i++) {
            lumVal = (BYTE)(*(pLumiVal++) * 255.0);
            *(pBGRAVal++) = (BYTE)0x00;
            *(pBGRAVal++) = (BYTE)0x00;
            *(pBGRAVal++) = (BYTE)0x00;
            *(pBGRAVal++) = lumVal;
        }
        else
#endif
        for (i=0; i<pDevTexture->MipMapLevels[level].width; i++) {
            lumVal = (BYTE)(*(pLumiVal++) * 255.0);
            *(pBGRAVal++) = lumVal;
            *(pBGRAVal++) = lumVal;
            *(pBGRAVal++) = lumVal;
            *(pBGRAVal++) = (BYTE)0xFF;
        }

        horzOffset = 0;
        for(i=0; i<numCopies; i++) {
        RtlCopyMemory((BYTE *)(ppdev->pjScreen +
             pDevTexture->MipMapLevels[0].BaseAddress + offset + horzOffset),
             pTexScanLine, tSize);
        horzOffset += tsizeBand;
        }
        offset += tSize;
    }
    offset +=(numCopies-1) * tsizeBand;

    } else
#endif
    {
    // vertical texture case AND square texture case
    tSize = pDevTexture->MipMapLevels[level].width * 4;

    numCopies = pDevTexture->MipMapLevels[level].height /
            pDevTexture->MipMapLevels[level].width;

    pLumiVal = (MCDFLOAT *)pDevTexture->MipMapLevels[level].pTexels;
    for(j=0; j<pDevTexture->MipMapLevels[level].height; j++) {

        pBGRAVal = pTexScanLine;
#if TEXLUMSPEC_BLEND_DKN
        if (pRc->privateEnables & __MCDENABLE_TEXLUMSPEC_BLEND_MODE_DKN)
        for (i=0; i<pDevTexture->MipMapLevels[level].width; i++) {
            lumVal = (BYTE)(*(pLumiVal++) * 255.0);
            *(pBGRAVal++) = (BYTE)0x00;
            *(pBGRAVal++) = (BYTE)0x00;
            *(pBGRAVal++) = (BYTE)0x00;
            *(pBGRAVal++) = lumVal;
        }
        else
#endif
        for (i=0; i<pDevTexture->MipMapLevels[level].width; i++) {
            lumVal = (BYTE)(*(pLumiVal++) * 255.0);
            *(pBGRAVal++) = lumVal;
            *(pBGRAVal++) = lumVal;
            *(pBGRAVal++) = lumVal;
            *(pBGRAVal++) = (BYTE)0xFF;
        }

        for(i=0; i<numCopies; i++) {
        RtlCopyMemory((BYTE *)(ppdev->pjScreen +
             pDevTexture->MipMapLevels[0].BaseAddress + offset),
             pTexScanLine, tSize);
        offset += tSize;
        }
    }
    }


#if TEX_ENABLE_MIP_MAPS
    //If MipMapping, copy last level another 3 times (4 texels instead of 1)
    if ((pDevTexture->bMipMaps) &&
        (pDevTexture->MipMapLevels[level].width == 1) &&
        (pDevTexture->MipMapLevels[level].height == 1)) {
        ULONG *pDstTexBuffer,texValue;

        pDstTexBuffer = (ULONG *)(ppdev->pjScreen +
         pDevTexture->MipMapLevels[0].BaseAddress + offset);
        texValue = *(ULONG*)(pTexScanLine);

        *(pDstTexBuffer++) = texValue;
        *(pDstTexBuffer++) = texValue;
        *(pDstTexBuffer)   = texValue;
    }
#endif

#ifdef MCD95
    } except (EXCEPTION_EXECUTE_HANDLER) {
       MCDBG_PRINT(DBG_ERR,"HWLoadLUMITexRegion: Unsafe texture "
                           "data pointer for reading");
       MCDMemFree(pTexScanLine); // free temporary buffer
       return FALSE;
    }
#else
    // Release locked memory and temporary buffer
    EngUnsecureMem(hMem);
#endif
    MCDMemFree(pTexScanLine);
    return TRUE;
}


//**************************************************************************
//
// LONG HWLoadBGRATexRegion
//
// Transfer a BGRA texture region for a specific level into already
// allocated S3Virge memory.  Right now we are updating the whole level
//
//**************************************************************************
LONG HWLoadBGRATexRegion(DEVRC *pRc, PDEV *ppdev, DEVTEXTURE *pDevTexture,
                         ULONG level, RECTL *pRect)
{
    ULONG tSize, offset, srcOffset, totTexels, i, numCopies;
    LONG  j;
    BYTE  *pBGRAVal;
#ifndef MCD95
    HANDLE hMem;
#endif

#ifdef MCD95
    try {
#else
    // Probe memory to see if its safe to read, if not cause an exception
    try {
        EngProbeForRead((VOID *)pDevTexture->MipMapLevels[level].pTexels,
                         pDevTexture->MipMapLevels[level].width *
                         pDevTexture->MipMapLevels[level].height * 4,
                         1);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        MCDBG_PRINT(DBG_ERR,"HWLoadBGRATexRegion: Unsafe texture "
                            "data pointer for reading");
        return FALSE;
    }

    // Lock memory during transfer, if unable to then abort transfer
    hMem = EngSecureMem((VOID *)pDevTexture->MipMapLevels[level].pTexels,
                        pDevTexture->MipMapLevels[level].width *
                        pDevTexture->MipMapLevels[level].height * 4);
    if (!hMem) {
        MCDBG_PRINT(DBG_ERR,"HWLoadBGRATexRegion: Unsafe texture "
                            "data pointer for reading");
        return FALSE;
    }
#endif

    // Calculate offset in texture memory to specified level of detail
    offset = HWTexOffset(pDevTexture, level);

#if TEXALPHA_FUNC
    // Check that all the alphas of this level are 0.0 or 1.0 to apply
    // quick subsitution fix
    if (pDevTexture->bAlpha01) {
    totTexels = pDevTexture->MipMapLevels[level].width *
            pDevTexture->MipMapLevels[level].height;
    pBGRAVal = pDevTexture->MipMapLevels[level].pTexels;
    pBGRAVal += 3;
    for (i=0; i < totTexels;i++,pBGRAVal += 4) {
        if (*pBGRAVal == 0x00) continue;
        if (*pBGRAVal == 0xFF) continue;
        pDevTexture->bAlpha01 = FALSE;
        break;
    }
    }
#endif


#if TEX_NON_SQR
    if (pDevTexture->MipMapLevels[level].width >
        pDevTexture->MipMapLevels[level].height) {
    //horizontal texture case
    tSize = pDevTexture->MipMapLevels[level].width *
        pDevTexture->MipMapLevels[level].height * 4;
    numCopies = pDevTexture->MipMapLevels[level].width /
            pDevTexture->MipMapLevels[level].height;

    for(i=0; i<numCopies; i++) {
        RtlCopyMemory((BYTE *)(ppdev->pjScreen +
         pDevTexture->MipMapLevels[0].BaseAddress + offset),
         pDevTexture->MipMapLevels[level].pTexels,
         tSize);
        offset += tSize;
    }

    } else
#endif
    {
    // vertical and square texture case
    tSize = pDevTexture->MipMapLevels[level].width * 4;

    numCopies = pDevTexture->MipMapLevels[level].height /
            pDevTexture->MipMapLevels[level].width;
    srcOffset = 0;

    for(j=0; j<pDevTexture->MipMapLevels[level].height; j++) {
        for(i=0; i<numCopies; i++) {
        RtlCopyMemory((BYTE *)(ppdev->pjScreen +
             pDevTexture->MipMapLevels[0].BaseAddress + offset),
             pDevTexture->MipMapLevels[level].pTexels + srcOffset,
             tSize);
        offset += tSize;
        }
        srcOffset += tSize;
    }
    }


#if TEX_ENABLE_MIP_MAPS
    // If MipMapping, and its the last lod then
    // copy last level another 3 times (4 texels instead of 1)
    if ((pDevTexture->bMipMaps) &&
        (pDevTexture->MipMapLevels[level].width == 1) &&
        (pDevTexture->MipMapLevels[level].height == 1)) {
        ULONG *pDstTexBuffer,texValue;

        pDstTexBuffer = (ULONG *)(ppdev->pjScreen +
         pDevTexture->MipMapLevels[0].BaseAddress + offset);
        texValue = *(ULONG*)(pDevTexture->MipMapLevels[level].pTexels);

        *(pDstTexBuffer++) = texValue;
        *(pDstTexBuffer++) = texValue;
        *(pDstTexBuffer)   = texValue;
    }
#endif

#ifdef MCD95
    } except (EXCEPTION_EXECUTE_HANDLER) {
       MCDBG_PRINT(DBG_ERR,"HWLoadBGRATexRegion: Unsafe texture "
                           "data pointer for reading");
       return FALSE;
    }
#else
    // Release locked memory
    EngUnsecureMem(hMem);
#endif
    return TRUE;
}

//*************************************************************************
//
// LONG HWLoadFullTextureLevel
//
// Transfer a BGRA texture into already allocated S3Virge memory
//
//**************************************************************************
LONG HWLoadFullTextureLevel(DEVRC *pRc, PDEV *ppdev, DEVTEXTURE *pDevTexture,
                            PTEXLOADFUNC ptlfn)
{
    LONG level;
    RECTL fullRect;

    level  = -1;
    do {
        level++;

        fullRect.left   = 0;
        fullRect.top    = 0;
        fullRect.right  = pDevTexture->MipMapLevels[level].width;
        fullRect.bottom = pDevTexture->MipMapLevels[level].height;
        if (!((*ptlfn)(pRc, ppdev, pDevTexture, (ULONG)level, &fullRect)))
            return FALSE;

#if !TEX_ENABLE_MIP_MAPS
    break; //exit loop to avoid loading more levels if any
#endif
    } while (pDevTexture->bMipMaps &&
             !((pDevTexture->MipMapLevels[level].width  == 1) &&
               (pDevTexture->MipMapLevels[level].height == 1)));

    return TRUE;
}

//***************************************************************************
//
// LONG HWLoadTexRegion
//
// Setup and load texture data into the S3Virge framebuffer memory for a
// specific level and a region
//
//***************************************************************************
LONG HWLoadTexRegion(PDEV *ppdev, DEVRC *pRc,
                     DEVTEXTURE *pDevTexture, ULONG lod, RECTL *pRect)
{
    BOOL bRet = FALSE;
    OH   *pohtex = (OH *) NULL;

    if (HWResidentTex(ppdev,pDevTexture) == MCDRV_TEXTURE_RESIDENT) {

#ifdef MCD95
        // In the MCD95 case, we need to lock the texture memory
        // before we can use it.  If the memory is already locked,
        // this just adds an extra lock reference.

        ASSERTDD(pDevTexture->MipMapLevels[0].pohMipMapTex,
                 "HWLoadTexRegion: bad pohMipMapTex");

        pohtex = pDevTexture->MipMapLevels[0].pohMipMapTex;
        if (pohLock(ppdev, pohtex)) {
#endif
            switch (pDevTexture->MipMapLevels[0].internalFormat) {
                case GL_BGR_EXT:
                case GL_BGRA_EXT:

                    if (HWLoadBGRATexRegion(pRc, ppdev, pDevTexture, lod, pRect))
                        bRet = TRUE;

                    break;
                case GL_LUMINANCE:

                    if (HWLoadLUMITexRegion(pRc, ppdev, pDevTexture, lod, pRect))
                        bRet = TRUE;

                    break;
                default:
                    break;
            }

#ifdef MCD95
            // Unlock texture memory before function returns.

            if (!pohUnlock(ppdev, pohtex)) {
                MCDBG_PRINT(DBG_ERR, "HWLoadTexRegion: pohUnlock failed");
            }
        } else {
            MCDBG_PRINT(DBG_ERR, "HWLoadTexRegion: pohLock failed");
        }
#endif
    }

    return bRet;
}

//**************************************************************************
//
// LONG HWLoadTexMemory
//
// Setup and load texture data into the S3Virge framebuffer memory
//
//**************************************************************************
LONG HWLoadTexMemory(DEVRC *pRc, PDEV *ppdev, DEVTEXTURE *pDevTexture)
{
    OH    *pohtex = (OH *) NULL;
    LONG  tSize, tszwidth, tszbytewidth, tszheight;

    BOOL  bMadeMoreS3Mem;

    // If texture already loaded then do nothing!
    if (HWResidentTex(ppdev,pDevTexture) == MCDRV_TEXTURE_RESIDENT) {
#ifdef MCD95
        // Actually, in the MCD95 case, we need to lock the
        // texture memory and, depending on the state of the
        // memory, we may need to fall below to load the
        // texture into memory.

        ASSERTDD(pDevTexture->MipMapLevels[0].pohMipMapTex,
                 "HWLoadTexMemory: bad pohMipMapTex");

        pohtex = pDevTexture->MipMapLevels[0].pohMipMapTex;
        if (pohLock(ppdev, pohtex)) {

            return TRUE;
        } else if (pohtex->bLostSurf) {
            // Since this texture may have been allocated in a previous
            // batch, we could lose it (display mode change, etc.).
            // If we lose the surface, delete it and fall into the
            // allocation code below.
            pohFree(ppdev, pohtex);
            pDevTexture->MipMapLevels[0].pohMipMapTex = NULL;
            pDevTexture->MipMapLevels[0].BaseAddress  = 0;
            pohtex = (OH *) NULL;
        } else {
            MCDBG_PRINT(DBG_ERR, "HWLoadTexMemory: pohLock failed");

            return FALSE;
        }
#else
    return TRUE;
#endif
    }

    MCDBG_PRINT(DBG_TEX,"Loading Texture , key= %x size = %i x %i "
                "Mipmaps= %i intFormat=%i",pDevTexture,
        pDevTexture->MipMapLevels[0].width,
                pDevTexture->MipMapLevels[0].height,
        pDevTexture->bMipMaps,
                pDevTexture->MipMapLevels[0].internalFormat);

    // Need to allocate and transfer texture to S3 memory
    // even if fields of pDevTexture are already initialized
    // since that only means the texture was bumped out.

    // Calculate necessary space for requested texture
    // , this is the size of the main mipmap (0)
#if TEX_NON_SQR
    tSize = pDevTexture->maxDim * pDevTexture->maxDim * 4;
#else
    tSize = pDevTexture->MipMapLevels[0].width *
            pDevTexture->MipMapLevels[0].width * 4;
#endif

//    tszwidth     = ppdev->heap.cxMax; //ppdev->cxMemory;
//    tszbytewidth = ppdev->heap.cxMax; //ppdev->lDelta;

    tszwidth     = ppdev->cxMemory;
    tszbytewidth = ppdev->lDelta;

#if TEX_ENABLE_MIP_MAPS
    // We will need the double of space if its a mipmapped texture
    if (pDevTexture->bMipMaps)
        //
        // Last mip map level repeated 4 times for S3Virge.
        // The right formula for mipmap tSize should be 4*(tSize+8)/3. 
        // In order to round up the floating point, use 
        // (4*(tSize+8)+2)/3 instead.
        // 
        tSize = (4 * (tSize + 8) + 2) / 3; 
#endif

    tszheight= tSize / tszbytewidth;

    if (tSize % tszbytewidth) tszheight++;

    // Try to allocate texture, if unsuccesful then try to unload a texture
    // from S3's memory, if even that fails then fail texture allocation
    bMadeMoreS3Mem = FALSE;
    do {
    // for mip mapping we'll need twice as much memory
    pohtex = pohAllocate(ppdev, tszwidth, tszheight,
                             TEXTURE_ALLOC_FLAGS);

    if (!pohtex)
        bMadeMoreS3Mem = HWFreeAnyTexMemory(ppdev);

    } while ( !pohtex && bMadeMoreS3Mem );

    if (!pohtex) {
        MCDBG_PRINT(DBG_ERR,
                    "TexFail: Unable to allocate space for texture of %i x %i",
                    pDevTexture->MipMapLevels[0].width,
                    pDevTexture->MipMapLevels[0].height);
        return FALSE;
    }

#ifdef MCD95
    if (!pohLock(ppdev, pohtex)) {
        MCDBG_PRINT(DBG_ERR,"HWLoadTexMemory: pohLock failed");

        // Shouldn't be able to lose surface allocated
        // within MCDrvDraw (but if it does happen, we can
        // fall through and fail the batch).
        ASSERTDD(!pohtex->bLostSurf,
                 "HWLoadTexMemory: unexpected surface lost");

        // Abandon texture load, release memory.
        pohFree(ppdev, pohtex);

        return FALSE;
    }
#endif


    // Assign properties to newly created texture space
    ASSERTDD(pohtex->x == 0,"Texture buffer should be 0-aligned");

    pDevTexture->MipMapLevels[0].pohMipMapTex = pohtex;
    pDevTexture->MipMapLevels[0].BaseAddress  = pohtex->ulBase;


#if TEXALPHA_FUNC
    if (pRc->privateEnables & __MCDENABLE_TEXALPHAFUNC_MODE)
    pDevTexture->bAlpha01 = TRUE; // until proven false!
    else
    pDevTexture->bAlpha01 = FALSE;
#endif

    // Copy mip map levels into S3 memory. Notice that the address
    // in S3 memory of all concatenated mipmaps is stored in level 0
    switch (pDevTexture->MipMapLevels[0].internalFormat) {
        case GL_BGR_EXT:
        case GL_BGRA_EXT:

            if (!HWLoadFullTextureLevel(pRc, ppdev, pDevTexture,
                                       HWLoadBGRATexRegion)) {
                // reset reclaimed texture space if fail
                pohFree(ppdev,pDevTexture->MipMapLevels[0].pohMipMapTex);
                pDevTexture->MipMapLevels[0].pohMipMapTex = NULL;
                pDevTexture->MipMapLevels[0].BaseAddress  = 0;
                return FALSE;
            }

        break;
        case GL_LUMINANCE:

            if (!HWLoadFullTextureLevel(pRc, ppdev, pDevTexture,
                                       HWLoadLUMITexRegion)) {
                // reset reclaimed texture space if fail
                pohFree(ppdev,pDevTexture->MipMapLevels[0].pohMipMapTex);
                pDevTexture->MipMapLevels[0].pohMipMapTex = NULL;
                pDevTexture->MipMapLevels[0].BaseAddress  = 0;
                return FALSE;
            }

        break;
        default:
        break;
    }

    // Update Texture List
    TexListInsert(ppdev, pDevTexture);

    return TRUE;
}

//**************************************************************************
//                   TEXTURE VALIDATION, INITIALIZATION AND SETUP
//**************************************************************************

//**************************************************************************
//
// LONG bValidTexture
//
// Check the texture structure is valid for our purpouses
//
//**************************************************************************
LONG bValidTexture(DEVTEXTURE *pDevTexture)
{

    LONG level, width0, height0;
    LONG widthLev, heightLev, widthLevPrev, heightLevPrev;

    // Don't do the check if we already did it in the past
    if (pDevTexture->validState == DEV_TEXTURE_VALID)
        return TRUE;


    if (pDevTexture->validState == DEV_TEXTURE_INVALID) {
        MCDBG_PRINT(DBG_TEX,"TexFail: Previously deemed  invalid");
        return FALSE;
    }

    if (pDevTexture->validState != DEV_TEXTURE_NOT_VALIDATED) {
        MCDBG_PRINT(DBG_ERR,"TexFail: Texture key seems to be invalid!!!!!");
        return FALSE;
    }

    // Assume failure
    pDevTexture->validState = DEV_TEXTURE_INVALID;

    // Check that the texture is 2D (we don't handle 1D textures)
    if (pDevTexture->textureDimension != 2) {
        MCDBG_PRINT(DBG_TEX,"TexFail:Texture is not 2D");
        return FALSE;
    }


    /////////////////////////////////////////////////////
    // Check correctnes and determine kind of texture map
    /////////////////////////////////////////////////////

    if (pDevTexture->MipMapLevels == NULL) {
        MCDBG_PRINT(DBG_ERR,"Trying to use an already deleted texture!");
        return FALSE;
    }

    width0  = pDevTexture->MipMapLevels[0].width;
    height0 = pDevTexture->MipMapLevels[0].height;

    // Cannot handle any texture > 512 texels
    if ((width0  > 512) || (height0 > 512)) {
        MCDBG_PRINT(DBG_TEX,"TexFail: Texture width or height is > 512 "
                                "texels, can't be handled");
        return FALSE;
    }


    // Handle square and non-square textures
#if TEX_NON_SQR
    pDevTexture->bSqrTex = TRUE;
#endif

    if (width0 != height0) {
#if TEX_NON_SQR
        pDevTexture->bSqrTex = FALSE;

        if (width0 > height0)
            pDevTexture->maxDim = width0;
        else
            pDevTexture->maxDim = height0;

        pDevTexture->uFactor = (MCDFLOAT)width0 /
                                   (MCDFLOAT)pDevTexture->maxDim;
        pDevTexture->vFactor = (MCDFLOAT)height0 /
                                   (MCDFLOAT)pDevTexture->maxDim;

    } 
    else {
        pDevTexture->maxDim = width0;
    }
#else
        // can't handle it
        MCDBG_PRINT(DBG_TEX,
                "TexFail: Texture is not square (width(%i)!=height(%i))",width0,height0);
        return FALSE;
    }
#endif


    // All textures must have a size which is a power of 2
    // we just check first level and afterwards check correct relationship
    // between mipmap levels if any more present
    if (!B_POWER_OF_2(width0) || !B_POWER_OF_2(height0)) {
        MCDBG_PRINT(DBG_TEX, "TexFail: Width or height not a power of 2");
        return FALSE;
    }


    if (pDevTexture->totLevels == 1)
        pDevTexture->bMipMaps = FALSE;
    else {
        // Check if mip map levels are present and valid
        widthLev  = width0;
        heightLev = height0;
        level = 0;
        pDevTexture->bMipMaps = FALSE;
        do {
            level++;
                widthLevPrev  = widthLev;
                heightLevPrev = heightLev;
                widthLev      = pDevTexture->MipMapLevels[level].width;
                heightLev     = pDevTexture->MipMapLevels[level].height;

            // check that actual and previous mip map level sizes are consistent
            // (that is, twice as big or both == 1 when non square textures)
            if (! ((widthLev * 2 == widthLevPrev) ||
              ((widthLev == 1) && (widthLevPrev == 1) &&
                        (heightLevPrev != 1)) ) ) {
                MCDBG_PRINT(DBG_ERR,"TexFail: Mip map levels are in wrong "
                                            "sequence or incomplete");
                return FALSE;
            }
            if (! ((heightLev * 2 == heightLevPrev) ||
              ((heightLev == 1) && (heightLevPrev == 1) &&
                        (widthLevPrev != 1)) )) {
                MCDBG_PRINT(DBG_ERR,"TexFail: Mip map levels are in wrong "
                                            "sequence or incomplete");
                return FALSE;
            }


        } while (!((widthLev == 1) && (heightLev == 1)));

        //
        // This section of code will never execute, see above do-while loop
        //

//        // Check that the mip map sequence is complete
//        if (pDevTexture->bMipMaps)
//        if ( (widthLev != 1) || (heightLev != 1)  ) {
//            MCDBG_PRINT(DBG_ERR, "TexFail: Mip map sequence incomplete");
//            return FALSE; // The mip map sequence is incomplete - refuse it
//        }

        pDevTexture->bMipMaps = TRUE;

    }
    // Check if we can handle the internal format
    switch (pDevTexture->MipMapLevels[0].internalFormat) {
        case GL_BGR_EXT:
        case GL_BGRA_EXT:
        case GL_LUMINANCE:
        break; // These are handled by the hw or munged in HWLoadTexMemory
        case GL_ALPHA:
        case GL_LUMINANCE_ALPHA:
        case GL_INTENSITY:
        case GL_RGB:
        case GL_RGBA:
        default:
        MCDBG_PRINT(DBG_TEX,"TexFail: cannot handle internal format = %i",
                pDevTexture->MipMapLevels[0].internalFormat);
        return FALSE;
    }

    MCDBG_PRINT(DBG_TEX, "New validated Texture is %i by %i , format %i",
    width0, height0, (long)pDevTexture->MipMapLevels[0].internalFormat);

    pDevTexture->validState = DEV_TEXTURE_VALID;
    return TRUE;
}

//*************************************************************************
//
// VOID InitTexState
//
// Initialize RC state and HW registers
//
//*************************************************************************
VOID InitTexState(DEVRC *pRc, DEVTEXTURE *pDevTexture)
{
    PDEV   *ppdev = pRc->ppdev;
    LONG    texBpp;
    ULONG   ulStride;

    HW_WAIT_DRAW_TEX(ppdev);

    // Setup according to the internal format
    switch (pDevTexture->MipMapLevels[0].internalFormat) {
        case GL_BGR_EXT:
        case GL_BGRA_EXT:
        case GL_LUMINANCE:
            pRc->hwTexFunc |= S3D_TEXTURE_32BPP_8888;
            texBpp = 4;
            break;
        default:
            // This should never happen, but lets check just to catch
            // potential bugs!
            MCDBG_PRINT(DBG_ERR,"Invalid internal texture format "
                                        "being handled!");
            break;
    }

    // Establish that we are dealing with a mip mapped texture
    // and check that the texturing filter is valid for mipmapping
    // otherwise, even if we have mipmaps, they shouldn't be applied
#if TEX_ENABLE_MIP_MAPS
    if ((pDevTexture->bMipMaps) &&
        ((pRc->texMinFilter == GL_NEAREST_MIPMAP_NEAREST) ||
     (pRc->texMinFilter == GL_LINEAR_MIPMAP_NEAREST ) ||
     (pRc->texMinFilter == GL_NEAREST_MIPMAP_LINEAR ) ||
     (pRc->texMinFilter == GL_LINEAR_MIPMAP_LINEAR  )  ))
        pRc->privateEnables |= __MCDENABLE_TEXMIPMAP;
#endif

    // Setup perspective correction bit if necessary
    if (pRc->privateEnables & __MCDENABLE_TEXPERSP)
        pRc->hwTexFunc |=
            (S3D_PERSPECTIVE_LIT_TEXTURED_TRIANGLE - S3D_LIT_TEXTURED_TRIANGLE);

    if (pDevTexture->MipMapLevels[0].width >=
        pDevTexture->MipMapLevels[0].height) {
        //set new texture size
        pRc->hwTexFunc |= pDevTexture->MipMapLevels[0].widthLog2 <<
                                S3D_MIPMAP_LEVEL_SIZE_SHIFT;

        //Set necessary parameters for textured triangle rendering
        pRc->texwidth     = pDevTexture->MipMapLevels[0].widthImagef;
        pRc->texwidthLog2 = pDevTexture->MipMapLevels[0].widthLog2;
    }
    else {
        //set new texture size
        pRc->hwTexFunc |= pDevTexture->MipMapLevels[0].heightLog2 <<
                                S3D_MIPMAP_LEVEL_SIZE_SHIFT;

        //Set necessary parameters for textured triangle rendering
        pRc->texwidth     = pDevTexture->MipMapLevels[0].heightImagef;
        pRc->texwidthLog2 = pDevTexture->MipMapLevels[0].heightLog2;
    }

    if (pRc->privateEnables & __MCDENABLE_TEXPERSP) {
        pRc->uBaseScale = (MCDFLOAT)( 1 << 16);
        pRc->vBaseScale = (MCDFLOAT)( 1 << 16);
        pRc->wScale     = (MCDFLOAT)(1 << 19);

        if (ppdev->ulChipID == SUBTYPE_325 || ppdev->ulChipID == SUBTYPE_988)
        {
            pRc->uvScale    = (MCDFLOAT)(1 << (12 + pRc->texwidthLog2) );
            pRc->ufix = (MCDFLOAT)(-0.5) * negPowerOfTwo[pRc->texwidthLog2];
            if (pRc->texwidthLog2 >= 8)
            {
                pRc->vfix = (MCDFLOAT)(-0.5) * negPowerOfTwo[pRc->texwidthLog2];
            }
            else
            {
                pRc->vfix = (MCDFLOAT)(-0.496093) * negPowerOfTwo[pRc->texwidthLog2];
            }
        }
        else
        {
            pRc->uvScale    = (MCDFLOAT)(1 << (8 + pRc->texwidthLog2) );
            pRc->ufix =
            pRc->vfix = (MCDFLOAT) 0.0;
        }
    } else {
        pRc->uBaseScale = __MCDZERO;
        pRc->vBaseScale = __MCDZERO;
        pRc->uvScale = (MCDFLOAT)(1 << 27 );

        if (ppdev->ulChipID == SUBTYPE_325 || ppdev->ulChipID == SUBTYPE_988)
        {
            pRc->ufix = (MCDFLOAT)(-0.5) *(MCDFLOAT)(1 << (27 - pRc->texwidthLog2));
            pRc->vfix = (MCDFLOAT)(-0.5) *(MCDFLOAT)(1 << (27 - pRc->texwidthLog2));
        }
        else
        {
            pRc->ufix =
            pRc->vfix = (MCDFLOAT) 0.0;
        }

        // lets setup now the U and V BASE so we
        // can spare doing it repeated times later
        S3DFifoWait(ppdev, 2);
        S3writeHIU(ppdev, S3D_TRI_BASE_U, 0x0L);
        S3writeHIU(ppdev, S3D_TRI_BASE_V, 0x0L);
    }

    // Initialize parameters for mip mapping
    if (pRc->privateEnables & __MCDENABLE_TEXMIPMAP) {
        pRc->dScale  = (MCDFLOAT)(1 << 27 );
    }


#if TEXLUMSPEC_BLEND_DKN
    // Prepare to use special case of luminance texture & blending function
    // with src = GL_ZERO and dst = GL_ONE_MINUS_DST_ALPHA
    if ((pRc->privateEnables & __MCDENABLE_TEXLUMSPEC_BLEND_MODE_DKN) &&
    (pDevTexture->MipMapLevels[0].internalFormat == GL_LUMINANCE))
         pRc->privateEnables |= __MCDENABLE_TEXLUMSPEC_BLEND_DKN;
     else
         pRc->privateEnables &= ~__MCDENABLE_TEXLUMSPEC_BLEND_DKN;

#endif


#if TEXALPHA_FUNC
    // Prepare to use special case of BGRA texture & alpha function
    // with func = GL_GREATER and  0.0 < ref < 1.0
    if ((pRc->privateEnables & __MCDENABLE_TEXALPHAFUNC_MODE) &&
    (pDevTexture->bAlpha01) &&
    (pDevTexture->MipMapLevels[0].internalFormat == GL_BGRA_EXT))
         pRc->privateEnables |= __MCDENABLE_TEXALPHAFUNC;
     else
         pRc->privateEnables &= ~__MCDENABLE_TEXALPHAFUNC;

#endif


    // setup texture base address, and set up the destination-source stride
    // to take into account the texture stride
    S3DFifoWait(ppdev, 2);
    S3writeHIU(ppdev, S3D_TRI_TEXTURE_BASE,
               pDevTexture->MipMapLevels[0].BaseAddress);
    MCDBG_PRINT(DBG_TEX,"Tex key= %x Tex address = %x", pDevTexture,
                        pDevTexture->MipMapLevels[0].BaseAddress);


    if ( pRc->MCDState.drawBuffer == GL_BACK )
    {
        ulStride = ppdev->ulBackBufferStride << 16;
    }
    else
    {
        ulStride = ppdev->lDelta << 16;
    }

    if (pDevTexture->MipMapLevels[0].width >= pDevTexture->MipMapLevels[0].height)
    {
        S3writeHIU( ppdev,
                    S3D_BLT_DESTINATION_SOURCE_STRIDE,
                    ulStride |
                    (pDevTexture->MipMapLevels[0].width * texBpp));
    }
    else
    {
        S3writeHIU( ppdev,
                    S3D_BLT_DESTINATION_SOURCE_STRIDE,
                    ulStride |
                    (pDevTexture->MipMapLevels[0].height * texBpp));
    }

#if TEX_NON_SQR
    //
    // Initialize texture non-square state in pRc first.
    //
    pRc->privateEnables &= ~__MCDENABLE_TEX_NONSQR;

    //
    // Setup u,v factors to handle non-square textures in this hw.
    //
    if (!pDevTexture->bSqrTex)
    {
        pRc->uFactor = pDevTexture->uFactor;
        pRc->vFactor = pDevTexture->vFactor;
        pRc->privateEnables |= __MCDENABLE_TEX_NONSQR;
    }
#endif
}


//**************************************************************************
//
// LONG HWUpdateTexState
//
// Setup the S3Virge and/or necessary state to render with a specific texture
//
// Here we make any necessary setup before drawing a command batch
// If we are requested something the S3Virge cannot handle, we
// must return FALSE in order to force sw rendering to be done
//
// Note: in the MCD95 case, texture memory is left locked (by
// HWLoadTexMemory).  Caller should ensure that memory is unlocked
// by calling HWUnlockTexKey.
//
//**************************************************************************
LONG HWUpdateTexState(DEVRC *pRc, ULONG textureKey)
{
    DEVTEXTURE *pDevTexture;
    PDEV *ppdev = pRc->ppdev;

    pRc->hwTexFunc      = 0;    // clear up hwTexFunc to fix 3D Maze Preview.

    if (textureKey == 0) {
        MCDBG_PRINT(DBG_ERR,"HWUpdateTexState: null device texture");
    return FALSE;
    }

    pDevTexture = (DEVTEXTURE *)textureKey;

    // Check if the texture is well structured for our purpouses
    if (!bValidTexture(pDevTexture))
    return FALSE;

    // Check texturing function (DECAL,BLEND,REPLACE,MODULATE) and set
    // necessary state, if it can't be handled by the S3 Virge then fail
    if (!HWTexEnvState(pRc,pDevTexture))
        return FALSE;

    // Check texturing parameters (GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T,
    // GL_TEXTURE_MIN_FILTER,GL_TEXTURE_MAG_FILTER,GL_TEXTURE_BORDER_COLOR)
    // and set state, fail if they cannot be handled
    if (!UpdateTexContextParm(pRc, pDevTexture))
        return FALSE;

    // Load texture into S3Virge frame buffer memory if possible
    // In the MCD95 case this will have the side-effect of locking the texture
    // memory.
    if (!HWLoadTexMemory(pRc, ppdev, pDevTexture))
    return FALSE;

    // Now initialize RC state and HW registers
    InitTexState(pRc, pDevTexture);

    return TRUE;
}

#ifdef MCD95
//**************************************************************************
//
// LONG HWUnlockTexKey
//
// Unlock texture memory under Win95
//
//**************************************************************************
LONG HWUnlockTexKey(DEVRC *pRc, ULONG textureKey)
{
    DEVTEXTURE *pDevTexture;
    PDEV *ppdev = pRc->ppdev;
    OH *pohTex;
    BOOL bRet = FALSE;

    if (textureKey != 0) {
        pDevTexture = (DEVTEXTURE *)textureKey;
        pohTex = pDevTexture->MipMapLevels[0].pohMipMapTex;

        if (pohUnlock(ppdev, pohTex)) {
            bRet = TRUE;
        } else {
            MCDBG_PRINT(DBG_ERR,"HWUnlockTexKey: pohUnlock failed");
        }
    } else {
        MCDBG_PRINT(DBG_ERR,"HWUnlockTexKey: null device texture");
    }

    return bRet;
}
#endif


//**************************************************************************
//
// LONG HWAllocTexMemory
//
// Initialize the mip map device structure.
//
// We are defering loading of textures until the drawing stage in this
// particular implementation.
//
//**************************************************************************
LONG HWAllocTexMemory(PDEV  *ppdev, DEVRC *pRc, DEVMIPMAPLEVEL *newlevel,
              MCDMIPMAPLEVEL *McdLevel)
{
    // Don't trust the MCD Mipmap level
    if (!McdLevel)
    return FALSE;

    // Just store the relevant information now, later we will
    // load the texture when the appropriate time comes
    newlevel->pTexels           =   McdLevel->pTexels;

    newlevel->width             =   McdLevel->width;
    newlevel->height            =   McdLevel->height;
    newlevel->widthImage        =   McdLevel->widthImage;
    newlevel->heightImage       =   McdLevel->heightImage;
    newlevel->widthImagef       =   McdLevel->widthImagef;
    newlevel->heightImagef      =   McdLevel->heightImagef;
    newlevel->widthLog2         =   McdLevel->widthLog2;
    newlevel->heightLog2        =   McdLevel->heightLog2;
    newlevel->border            =   McdLevel->border;
    newlevel->requestedFormat   =   McdLevel->requestedFormat;
    newlevel->baseFormat        =   McdLevel->baseFormat;
    newlevel->internalFormat    =   McdLevel->internalFormat;
    newlevel->redSize           =   McdLevel->redSize;
    newlevel->greenSize         =   McdLevel->greenSize;
    newlevel->blueSize          =   McdLevel->blueSize;
    newlevel->alphaSize         =   McdLevel->alphaSize;
    newlevel->luminanceSize     =   McdLevel->luminanceSize;
    newlevel->intensitySize     =   McdLevel->intensitySize;

    newlevel->pohMipMapTex      =   NULL;
    newlevel->BaseAddress       =   0;

    return TRUE;
}

#endif

