/******************************Module*Header*******************************\
*
*                           *******************
*                           * D3D SAMPLE CODE *
*                           *******************
* Module Name: d3dstate.c
*
*  Content:    Direct3D hw rasterizing state management.
*
* Copyright (C) 1996-1998 Microsoft Corporation.  All Rights Reserved.
\**************************************************************************/

#include "precomp.h"
#include "d3ddrv.h"
#include "d3dmath.h"
#include "hw3d.h"

//************************************************************************
// LPRENDERTRIANGLE __HWSetTriangleFunc
//
// Returns a pointer to the appropriate triangle rendering function to
// use according to the current rendering context
//
//************************************************************************
LPRENDERTRIANGLE __HWSetTriangleFunc(S3_CONTEXT *pCtxt)
{
    if (pCtxt->FillMode == D3DFILL_POINT)
        return (LPRENDERTRIANGLE)GenericPointTriangle;

    if (pCtxt->FillMode == D3DFILL_WIREFRAME)
        return (LPRENDERTRIANGLE)GenericWireFrameTriangle; 

    if((pCtxt->dwRCode & S3TEXTURED) &&
       (pCtxt->dwRCode & S3PERSPECTIVE))
        return (LPRENDERTRIANGLE)TesselateInUVSpace;

    return pRenderTriangle[pCtxt->dwRCode];
}


//************************************************************************
// DWORD ComputeAlignedStride
//
// This function is generally called by RenderPrimitive when the local 
// Z buffer mode mask is set up and when the stride is being calculated. 
// ComputeAlignedStride computes the alignment based on the pixel depth 
// of the DirectDraw surface (lpDDS) which is passed to it. If it is an 
// 8 bit surface, then bytesPerPix is set to one. If lpDDS is a 16 bit 
// surface, the bytesPerPix is set to 2 and if it is a 24 bit surface 
// the bytesPerPix is set to 3. The default case is to return 
// (DWORD) - 1. Otherwise the function returns the alignment for the 
// surface. 
//
//************************************************************************
DWORD ComputeAlignedStride(LPDDRAWI_DDRAWSURFACE_LCL lpDDS)
{
    return DDSurf_Pitch(lpDDS);
}

//************************************************************************
//
// DWORD __SelectFVFTexCoord
//
// This utility function sets the correct texture offset depending on the
// texturing coordinate set wished to be used from the FVF vertexes
//
//************************************************************************
void __SelectFVFTexCoord(LPS3FVFOFFSETS lpS3FVFOff, DWORD dwTexCoord)
{
        lpS3FVFOff->dwTexOffset = lpS3FVFOff->dwTexBaseOffset + 
                                    dwTexCoord * 2 * sizeof(D3DVALUE);
        // verify we don't exceed the FVF vertex structure provided , if
        // so go down to set 0 as a crash-avoiding alternative

        if (lpS3FVFOff->dwTexOffset >= lpS3FVFOff->dwStride)
            lpS3FVFOff->dwTexOffset = lpS3FVFOff->dwTexBaseOffset;
}


HRESULT __HWSetupTexture(S3_CONTEXT *pCtxt, LPS3_TEXTURE lpTexture);
HRESULT WINAPI __HWSetupTSSState(S3_CONTEXT *pCtxt, LPS3FVFOFFSETS lpS3FVFOff);
//************************************************************************
//
// HRESULT __HWSetupPrimitive
//
// __HWSetupPrimitive takes a context that is passed to it and sets up 
// the hardware state based on this new context. Most of this code is 
// implementation specific but will give the driver writer an idea of 
// how to set up a piece of hardware to render primitives. 
//
// __HWSetupPrimitive is called by DevD3DRenderPrimitive, 
// DevD3DDrawOnePrimitive32, DevD3DDrawOneIndexedPrimitive32, and 
// DevD3DDrawPrimitives32 to prepare the card for rendering primitives. 
//************************************************************************
HRESULT WINAPI __HWSetupPrimitive(S3_CONTEXT *pCtxt, LPS3FVFOFFSETS lpS3FVFOff)
{
    enum PIXELALPHA { NONE, PRIMITIVE_ALPHA, TEXTURE_ALPHA} PixelAlpha;
    DWORD tex_bitdepth, tex_width, tex_height, tex_alphabitmask, dpt_lPitch,
          dst_bitdepth, dst_width, dst_height, dst_lPitch;
    FLATPTR tex_fpVidMem, dst_fpVidMem, dpt_fpVidMem;
    S3_TEXTURE *pSurfRender, *pSurfZBuffer;

    // if the context has changed, bytesPerPix is declared so that 
    // it can be used to hold the surface bit depth. The texture is 
    // loaded into lpTexture and hardware clipping is enabled. All 
    // cached values also need to be reset. This entails setting the 
    // bChanged flag back to FALSE and destination and source stride to zero.

    pSurfRender = TextureHandleToPtr(pCtxt->RenderSurfaceHandle ,pCtxt);
    if (!pSurfRender)
        return DDERR_SURFACELOST;

    dst_bitdepth = pSurfRender->dwRGBBitCount;
    dst_width = pSurfRender->dwWidth;
    dst_height = pSurfRender->dwHeight;
    dst_fpVidMem = pSurfRender->fpVidMem;
    dst_lPitch = pSurfRender->lPitch;

    if( pCtxt->bChanged ) {

        int bytesPerPix;
        LPS3_TEXTURE lpTexture;
        DWORD dwhTexture;
        DWORD rndCommand = cmd3D_CMD | cmdHWCLIP_EN;

        pCtxt->Alpha_workaround = FALSE;

        // Choose between legacy Texture Handle or TSS
        if (S3MULTITEXTURE & pCtxt->dwStatus)
        {
            //Setup TSS state AND textures 
            if (__HWSetupTSSState(pCtxt, lpS3FVFOff ) == DD_OK) 
            {
                lpTexture = (LPS3_TEXTURE)pCtxt->tssStates[D3DTSS_TEXTUREMAP];
                // if this FVF has no tex coordinates at all disable texturing
                if (lpS3FVFOff->dwTexBaseOffset == 0)
                {
                    DPF_DBG("No texture coords present in FVF"
                            " to texture map primitives");
                }
            } else {
                lpTexture = NULL;
                DPF_DBG("TSS Setup failed");
            }

            pCtxt->dwTexture = (DWORD_PTR)lpTexture;
            
            if (lpTexture)
            {
                lpTexture = TextureHandleToPtr(pCtxt->dwTexture, pCtxt);
            }
        }
        else
        {
            lpTexture = (LPS3_TEXTURE)pCtxt->dwTexture;

            if (pCtxt->dwTexture)
            {

                lpTexture = TextureHandleToPtr(pCtxt->dwTexture, pCtxt);

                __HWSetupTexture(pCtxt,lpTexture);
                __SelectFVFTexCoord( lpS3FVFOff, 0);
            }
        }

        // must reset all cached values
        pCtxt->bChanged = FALSE;

        // we have not yet selected to use either source color or 
        // texture alpha for blending
        PixelAlpha = NONE;

        // check textures and lock them
        if( lpTexture ) {

            DWORD dwTmp;

            tex_bitdepth = lpTexture->dwRGBBitCount;
            tex_alphabitmask = lpTexture->dwRGBAlphaBitMask;
            tex_width = lpTexture->dwWidth;
            tex_height = lpTexture->dwHeight;
            tex_fpVidMem = lpTexture->fpVidMem;

            // check to see if D3DTADDRESS_WRAP or D3DTADDRESS_CLAMP are enabled. 
            // If either are enabled, the rndCommand (a command word ready to 
            // send to the ViRGE hardware) is sent to enable wrapping on the 
            // card. 
            if( (pCtxt->TextureAddress == D3DTADDRESS_WRAP) || 
                (pCtxt->TextureAddress == D3DTADDRESS_CLAMP) )
                rndCommand |= cmdWRAP_EN;

            // The texture format is set, based on the local texture 
            // surface bit depth.
            switch( tex_bitdepth ) {
            case 32 :
                bytesPerPix = 4;
                rndCommand |= cmdTEX_CLR_FMT_ARGB8888;
                break;

            case 16 :
                bytesPerPix = 2;

                if ( tex_alphabitmask == S3VIRGE_RGBA4444_ALPHABITMASK )
                    rndCommand |= cmdTEX_CLR_FMT_ARGB4444;
                else
                    rndCommand |= cmdTEX_CLR_FMT_ARGB1555;
                break;

            case 8:
                bytesPerPix = 1;
                rndCommand |= cmdTEX_CLR_FMT_8BPP_ClrInd;
                break;
            } //switch

            // The destination and source stride are set based on the DirectDraw
            // surface width which is a function of the bytes per pixel of the 
            // texture and the local texture surface. The texture base address 
            // is set up by finding the starting address of texture memory and 
            // subtracting the frame address for this context. 
            pCtxt->rnd3DSetup.stp_DstSrc_Stride = 
                      ( tex_width * bytesPerPix & 0xFFFFL );
            pCtxt->rnd3DSetup.stp_TEX_BASE =
                      (ULONG)(tex_fpVidMem) - pCtxt->FrameAddr;

            // Texture filtering rules determine how the texel maps to a pixel.
            switch (pCtxt->TextureMode) {
            default:
            case D3DFILTER_NEAREST:
                rndCommand |= cmdTEX_FILT_MODE_1Tpp;
                break;
            case D3DFILTER_LINEAR:
                rndCommand |= cmdTEX_FILT_MODE_4Tpp;
                break;
            }

            // The maximum mip-map level is computed here, by using the width, 
            // height, and width and height factors. That value is used to 
            // set rsfMaxMipMapLevel. The texture value is then converted to 
            // an integer and made perspective correct if perspective 
            // correction is being used. 

            pCtxt->fTextureWidth = (float)(dwTmp = tex_width);
            pCtxt->fTextureHeight = (float)tex_height;

            // Calculate mip map level size
            pCtxt->rsfMaxMipMapLevel = dwTmp = ( IntLog2(dwTmp) - 1 );
            pCtxt->dTexValtoInt = (pCtxt->dwRCode & S3PERSPECTIVE) ?
                                        dTexValtoIntPerspective[dwTmp]:
                                        dTexValtoInt[dwTmp];
            rndCommand |= ( (dwTmp + 1) << 8 );
            DPF_DBG( "MipMapLevel = %d", (dwTmp + 1) );

            // Set up contect with specified texture blend mode
            switch (pCtxt->BlendMode)
            {
            case D3DTBLEND_COPY:
                rndCommand |= cmdCMD_TYPE_UnlitTex;
                rndCommand |= cmdLIT_TEX_MODE_Decal;
                PixelAlpha = TEXTURE_ALPHA;     
                break;

            case D3DTBLEND_DECAL:
                rndCommand |= cmdCMD_TYPE_UnlitTex;
                rndCommand |= cmdLIT_TEX_MODE_Decal;
                PixelAlpha = TEXTURE_ALPHA;     
                break;

            case D3DTBLEND_DECALALPHA:
                rndCommand |= cmdCMD_TYPE_UnlitTex;
                rndCommand |= cmdLIT_TEX_MODE_Decal;
                PixelAlpha = PRIMITIVE_ALPHA;      
                break;

            case D3DTBLEND_MODULATE:
                rndCommand |= cmdCMD_TYPE_LitTex;
                rndCommand |= cmdLIT_TEX_MODE_Modulate;
                PixelAlpha = TEXTURE_ALPHA;     
                break;

            case D3DTBLEND_MODULATEALPHA:
                rndCommand |= cmdCMD_TYPE_LitTex;
                rndCommand |= cmdLIT_TEX_MODE_Modulate;
                PixelAlpha = TEXTURE_ALPHA;  //not very correct, but for hellbender 
                break;
            
            case D3DTBLEND_ADD: 
                rndCommand |= cmdCMD_TYPE_LitTex;
                rndCommand |= cmdLIT_TEX_MODE_CmplxRflct;
                PixelAlpha = PRIMITIVE_ALPHA;  
                break;

            default:
                rndCommand |= cmdCMD_TYPE_LitTex;
                rndCommand |= cmdLIT_TEX_MODE_Modulate;
                PixelAlpha = PRIMITIVE_ALPHA;
                break;

            } // switch (pCtxt->BlendMode)

            // If perspective correction to be done set bit in rndCommand
            if (pCtxt->dwRCode & S3PERSPECTIVE) 
                rndCommand |= cmdCMD_TYPE_UnlitTexPersp - cmdCMD_TYPE_UnlitTex;
        
        } else { // if(lpTexture)
            rndCommand |= cmdCMD_TYPE_Gouraud;
            PixelAlpha = PRIMITIVE_ALPHA;
        }

        // validate we can handle the blending mode mix, set DstBlend
        // accordingly to the value set in SrcBlend because of hw limitations
        if(pCtxt->dwStatus & S3ALPHABLENDENABLE) {
            if ( pCtxt->SrcBlend == D3DBLEND_ONE )
                pCtxt->DstBlend = D3DBLEND_ZERO;
            if ( pCtxt->SrcBlend == D3DBLEND_SRCALPHA )
                pCtxt->DstBlend = D3DBLEND_INVSRCALPHA;
        }

        //set up alpha value for source and destination blending
        //BMSI
        if(lpTexture && 
           lpTexture->ColorKey && 
           (pCtxt->dwStatus & S3COLORKEYENABLE)) {
            rndCommand |= cmdALP_BLD_CTL_TexAlph;
        }else
        //EMSI
        if(pCtxt->dwStatus & S3ALPHABLENDENABLE) {
            //get alpha from source instead of dest.
            if ( pCtxt->SrcBlend == D3DBLEND_SRCALPHA ) {
                if(PixelAlpha == PRIMITIVE_ALPHA)
                    rndCommand |= cmdALP_BLD_CTL_SrcAlph;
                    else if((PixelAlpha == TEXTURE_ALPHA) && (lpTexture)) {
                          if ( tex_alphabitmask 
                          //BMSI
                               ||   (lpTexture->ColorKey && (pCtxt->dwStatus & S3COLORKEYENABLE))
                          //EMSI
                             ) 
                          {
                              rndCommand |= cmdALP_BLD_CTL_TexAlph;
                          }                         
                          else if ((pCtxt->BlendMode == D3DTBLEND_MODULATE) && 
                                          !(pCtxt->dwStatus & S3SPECULARENABLE))
                          {
                              rndCommand |= cmdALP_BLD_CTL_SrcAlph;
                          }
                  }
            }// srcalpha
        }//src and dest blending

        // Clear Z buffer mode bits
        rndCommand &= cmdZ_BUF_MODE_MASK;
        rndCommand &= cmdZ_BUF_CMP_MASK;
        rndCommand &= cmdZ_UPDATE_DIS; 

        pSurfZBuffer = TextureHandleToPtr(pCtxt->ZBufferHandle ,pCtxt);

        if ( !pSurfZBuffer && pCtxt->bZEnabled)
            return DDERR_SURFACELOST;

        if ( pSurfZBuffer && pCtxt->bZEnabled) {

            // Z-buffer use enable 
            rndCommand |= cmdZ_BUF_ON;

            dpt_fpVidMem = pSurfZBuffer->fpVidMem;
            dpt_lPitch = pSurfZBuffer->lPitch;

            // Z-buffer address and stride
            pCtxt->rnd3DSetup.stp_Z_BASE   = 
                    (ULONG)(dpt_fpVidMem) - pCtxt->FrameAddr;
            pCtxt->rnd3DSetup.stp_Z_STRIDE = dpt_lPitch;

            // Check if Z write is enabled & set write enable bit
            if (pCtxt->bZWriteEnabled)
                rndCommand |= cmdZ_UPDATE_EN;  //

            // Setup Z-buffer comparison function
            switch (pCtxt->ZCmpFunc) {
            case D3DCMP_NEVER:
                rndCommand |= cmdZ_BUF_CMP_NeverPass;
                break;
            case D3DCMP_LESS:
                rndCommand |= cmdZ_BUF_CMP_ZsLsZfb;
                break;
            case D3DCMP_EQUAL:
                rndCommand |= cmdZ_BUF_CMP_ZsEqZfb;
                break;
            case D3DCMP_LESSEQUAL:
                rndCommand |= cmdZ_BUF_CMP_ZsLeZfb;
                break;
            case D3DCMP_GREATER:
                rndCommand |= cmdZ_BUF_CMP_ZsGtZfb;
                break;
            case D3DCMP_NOTEQUAL:
                rndCommand |= cmdZ_BUF_CMP_ZsNeZfb;
                break;
            case D3DCMP_GREATEREQUAL:
                rndCommand |= cmdZ_BUF_CMP_ZsGeZfb;
                break;
            default:
                rndCommand |= cmdZ_BUF_CMP_AlwaysPass;
                break;
            } //switch

        } else {
            // Disable if no Z buffer or Z disabled.
            rndCommand |= cmdZ_BUF_OFF;
        }


        //found bug with texture alpha and fog on at same time in MechWarriorII.
        //so we don't enable fogging if texture alpha is enabled.
        //fogging, we let alpha blending override fogging, as hw 
        //only supports one at a time

        // Only enable fogging/specular lighting if we have a specular color
        // component in our FVF vertexes since we rely on that.
        if (lpS3FVFOff->dwSpcOffset) {
            if (rndCommand & (~cmdALP_BLD_CTL_MASK)) {   
                pCtxt->bFogEnabled = FALSE;
                pCtxt->bSpecular   = FALSE;
            } else if (pCtxt->dwStatus & S3FOGENABLE) {
                pCtxt->bFogEnabled = TRUE;
                pCtxt->bSpecular   = FALSE;
            } else if (pCtxt->dwStatus & S3SPECULARENABLE) {
                pCtxt->bFogEnabled = FALSE;
                pCtxt->bSpecular   = TRUE;
            } else {
                pCtxt->bFogEnabled = FALSE;
                pCtxt->bSpecular   = FALSE;
            }
    
            if (pCtxt->bSpecular || pCtxt->bFogEnabled)
                pCtxt->dwRCode |= S3FOGGED ;
            else
                pCtxt->dwRCode &= ~S3FOGGED;

            // Check if this context has fog enabled
            if (pCtxt->dwRCode & S3FOGGED) {
                // Cleaning alpha blending bits
                rndCommand &= cmdALP_BLD_CTL_MASK; 
                rndCommand |= cmdFOG_EN;

                // Set up the shading, texture mode, texture filtering mode, 
                // enables wrapping, sets up the stride and the white texture 
                // base address when dealing with a non-texture. The 
                // dwWhiteTexture is an implementation specific anomaly of this 
                //driver which is used as a default texture state. 
                if( !pCtxt->dwTexture ) {
                    pCtxt->bChanged= TRUE;       // this force reinitialize next time
                    rndCommand &= ~cmdCMD_TYPE_Gouraud;
                    rndCommand |= cmdCMD_TYPE_LitTex;
                    rndCommand |= cmdLIT_TEX_MODE_Modulate;
                    rndCommand |= cmdTEX_CLR_FMT_ARGB8888;
                    rndCommand |= cmdTEX_FILT_MODE_1Tpp;
                    rndCommand |= 3 << 8;
                    rndCommand |= cmdWRAP_EN;
                    pCtxt->rnd3DSetup.stp_DstSrc_Stride |= 8;
                    pCtxt->rnd3DSetup.stp_TEX_BASE = 
                             D3DGLOBALPTR(pCtxt)->D3DGlobals.dwWhiteTexture;
                }
            }
        } else {
            // We have no specular color component so this has to be reset
            pCtxt->bFogEnabled = FALSE;
            pCtxt->bSpecular   = FALSE;
            pCtxt->dwRCode &= ~S3FOGGED;
        }

        // check if when processing TSS we detected the alpha workaround
        // might be needed if the texture lacks an alpha channel
        if (pCtxt->Alpha_workaround)
        {
            // maintain the need for the fix if the texture lacks an alpha
            // channel, clear if otherwise.
            pCtxt->Alpha_workaround = ( tex_alphabitmask == 0 );
        }

        // The destination surface pixel format is set based on the bit 
        // depth of the local DirectDrawSurface
        switch (dst_bitdepth) {
        case 8:
            rndCommand |= cmdDEST_FMT_8BPP_PAL;
            break;
        case 16:
            rndCommand |= cmdDEST_FMT_ZRGB1555;
            break;
        case 24:
            rndCommand |= cmdDEST_FMT_RGB888;
            if (! pCtxt->Alpha_workaround )
            {
                if( ((rndCommand & ~cmdALP_BLD_CTL_MASK) != cmdALP_BLD_CTL_SrcAlph) &&
                    ((rndCommand & ~cmdALP_BLD_CTL_MASK) != cmdALP_BLD_CTL_TexAlph) &&
                    !pCtxt->bFogEnabled && !pCtxt->bSpecular) {
                        pCtxt->Alpha_workaround = TRUE;
                        rndCommand &= cmdALP_BLD_CTL_MASK;
                        rndCommand |= cmdALP_BLD_CTL_SrcAlph;
                }
            }
            break;
        }

        // save new rendering command dword in rendering context
        pCtxt->rndCommand = rndCommand;

    }  // if( pCtxt->bChanged )

    // Setup destination buffer address and stride
    pCtxt->rnd3DSetup.stp_DEST_BASE = 
                    ((ULONG) dst_fpVidMem) - pCtxt->FrameAddr;
    pCtxt->rnd3DSetup.stp_DstSrc_Stride &= 0xffffL;   //preserve texture stride
    pCtxt->rnd3DSetup.stp_DstSrc_Stride |= 
                            ( dst_lPitch  << 16 );
    pCtxt->rnd3DSetup.stp_CLIP_LftRt  =  dst_width - 1;
    pCtxt->rnd3DSetup.stp_CLIP_TopBtm =  dst_height - 1;

    _WRITE_S3D_HEADER ( 0x24 );

    // Write out framebuffer, zbuffer and textures addresses and strides
    // and also clipping parameters
    S3DFifoWait(  pCtxt->ppdev,  9 );
    _WRITE_REG_SET_LONG(pCtxt, Z_BASE,       pCtxt->rnd3DSetup.stp_Z_BASE );
    _WRITE_REG_SET_LONG(pCtxt, DEST_BASE,    pCtxt->rnd3DSetup.stp_DEST_BASE );
    _WRITE_REG_SET_LONG(pCtxt, CLIP_L_R,     pCtxt->rnd3DSetup.stp_CLIP_LftRt );
    _WRITE_REG_SET_LONG(pCtxt, CLIP_T_B,     pCtxt->rnd3DSetup.stp_CLIP_TopBtm );
    _WRITE_REG_SET_LONG(pCtxt, DEST_SRC_STR, pCtxt->rnd3DSetup.stp_DstSrc_Stride );
    _WRITE_REG_SET_LONG(pCtxt, Z_STRIDE,     pCtxt->rnd3DSetup.stp_Z_STRIDE );
    _WRITE_REG_SET_LONG(pCtxt, TEX_BASE,     pCtxt->rnd3DSetup.stp_TEX_BASE );
    _WRITE_REG_SET_LONG(pCtxt, TEX_BDR_CLR,  0xFFFFFFFF );

    if (pCtxt->bSpecular) {
        _WRITE_REG_SET_LONG(pCtxt, DC_FADE_CLR, 0xffffff );
    } else {
        _WRITE_REG_SET_LONG(pCtxt, DC_FADE_CLR, pCtxt->rnd3DSetup.stp_FOG_CLR );
    }

    _ADVANCE_INDEX( 0x24 );

    // Write out parameters for debugging
    DPF_DBG( "Z_BASE 0x%08x",       pCtxt->rnd3DSetup.stp_Z_BASE );
    DPF_DBG( "DEST_BASE 0x%08x",    pCtxt->rnd3DSetup.stp_DEST_BASE );
    DPF_DBG( "CLIP_L_R 0x%08x",     pCtxt->rnd3DSetup.stp_CLIP_LftRt );
    DPF_DBG( "CLIP_T_B 0x%08x",     pCtxt->rnd3DSetup.stp_CLIP_TopBtm );
    DPF_DBG( "DEST_SRC_STR 0x%08x", pCtxt->rnd3DSetup.stp_DstSrc_Stride );
    DPF_DBG( "Z_STRIDE 0x%08x",     pCtxt->rnd3DSetup.stp_Z_STRIDE );
    DPF_DBG( "TEX_BASE 0x%08x",     pCtxt->rnd3DSetup.stp_TEX_BASE );
    DPF_DBG( "TEX_BDR_CLR 0x%08x",  pCtxt->rnd3DSetup.stp_TEX_BDR_CLR );
    DPF_DBG( "DC_FADE_CLR 0x%08x",  pCtxt->rnd3DSetup.stp_FOG_CLR );
    DPF_DBG( "Cmd is %08x",         pCtxt->rndCommand );

    return DD_OK;
}; 
//*****************************************************************************
//
// void __HWSetupTextureAlpha
//
// Fixes the textures alpha channel so that it is usable by the S3 Virge
//
//*****************************************************************************
void __HWSetupTextureAlpha(S3_CONTEXT *pCtxt,
                           LPS3_TEXTURE lpTexture)
{
    ULONG i, j;
    ULONG tex_width = lpTexture->dwWidth;
    ULONG tex_height =lpTexture->dwHeight;
    ULONG bit_depth = lpTexture->dwRGBBitCount;
    DWORD *videoptr = (DWORD *)(lpTexture->fpVidMem + pCtxt->ppdev->pjScreen );
    ULONG alphabitmask = lpTexture->dwRGBAlphaBitMask;

    switch( bit_depth ) {
    case 32:
    {
         // RGBA 8888 texture format
        DWORD *memptr = (DWORD *)videoptr;

        for (i = 0; i < tex_width; i++)
        for (j = 0; j < tex_height; j++)
        {       
           if( TEXEL_IN_COLORKEY_RANGE(*memptr, 
                                        (DWORD)lpTexture->ColorKeyValueHigh , 
                                        (DWORD)lpTexture->ColorKeyValueLow ,
                                        (DWORD)0x00ff0000,
                                        (DWORD)0x0000ff00,
                                        (DWORD)0x000000ff) )
              *memptr &= 0x00ffffff;
           else
              *memptr |= 0xff000000;  
           
           memptr ++;
        }
    }  
    break;

    case 16:

    if ( alphabitmask == S3VIRGE_RGBA4444_ALPHABITMASK ) {
         // RGBA 4444 texture format
         WORD *memptr = (WORD *)videoptr;

         for (i = 0; i < tex_width; i++)
         for (j = 0; j < tex_height; j++)
         {
            if( TEXEL_IN_COLORKEY_RANGE(*memptr, 
                                        (WORD)lpTexture->ColorKeyValueHigh , 
                                        (WORD)lpTexture->ColorKeyValueLow ,
                                        (WORD)0x0f00,
                                        (WORD)0x00f0,
                                        (WORD)0x000f) )                

                *memptr &=0x0fff; 
            else 
                *memptr |= 0xf000;
            memptr ++;
         }
     } else {    
          // RGBA 1555 texture format
          WORD *memptr = (WORD *)videoptr;

          for (i = 0; i < tex_width; i++)
          for (j = 0; j < tex_height; j++)
          {                                                                                                             
            if( TEXEL_IN_COLORKEY_RANGE(*memptr, 
                                        (WORD)lpTexture->ColorKeyValueHigh ,
                                        (WORD)lpTexture->ColorKeyValueLow ,
                                        (WORD)0x7c00,
                                        (WORD)0x03e0,
                                        (WORD)0x001f) )  
                *memptr &=0x7fff; 
            else 
                *memptr |= 0x8000;

            memptr ++;
          } 
    
     } 
     break;
  
     case 8:
        break;
    } // switch  
}

//*****************************************************************************
//
// void __HWSetupTexture
//
// Fixes the texture so that it is usable by the S3 Virge
//
//*****************************************************************************
HRESULT __HWSetupTexture(S3_CONTEXT *pCtxt, LPS3_TEXTURE lpTexture)
{
    if (!lpTexture)
        return (DDHAL_DRIVER_HANDLED);

    DPF_DBG("In __HWSetupTexture");

    //software colorkey setup
    if(lpTexture->ColorKey &&
       !lpTexture->ColorKeySet)
    {
        __HWSetupTextureAlpha(pCtxt, lpTexture);
        lpTexture->ColorKeySet = TRUE;
    }

    return DD_OK;
} 


//*****************************************************************************
//
// HRESULT __HWSetupStageStates
//
// Processes the state changes related to the DX6 texture stage states in the 
// current rendering context
//
//*****************************************************************************
HRESULT WINAPI __HWSetupTSSState(S3_CONTEXT *pCtxt, LPS3FVFOFFSETS lpS3FVFOff)
{
    // If we are to texture map our primitives
    if (pCtxt->tssStates[D3DTSS_TEXTUREMAP])
    {
        DWORD mag = pCtxt->tssStates[D3DTSS_MAGFILTER];
        DWORD min = pCtxt->tssStates[D3DTSS_MINFILTER];
        DWORD mip = pCtxt->tssStates[D3DTSS_MIPFILTER];
        DWORD cop = pCtxt->tssStates[D3DTSS_COLOROP];
        DWORD ca1 = pCtxt->tssStates[D3DTSS_COLORARG1];
        DWORD ca2 = pCtxt->tssStates[D3DTSS_COLORARG2];
        DWORD aop = pCtxt->tssStates[D3DTSS_ALPHAOP];
        DWORD aa1 = pCtxt->tssStates[D3DTSS_ALPHAARG1];
        DWORD aa2 = pCtxt->tssStates[D3DTSS_ALPHAARG2];
        DWORD tad = pCtxt->tssStates[D3DTSS_ADDRESS];
        DWORD txc = pCtxt->tssStates[D3DTSS_TEXCOORDINDEX];

        // Choose texture coord to use
        __SelectFVFTexCoord( lpS3FVFOff, txc);

        // Current is the same as diffuse in stage 0
        if(ca2 == D3DTA_CURRENT)
            ca2 = D3DTA_DIFFUSE;
        if(aa2 == D3DTA_CURRENT)
            aa2 = D3DTA_DIFFUSE;

        // Check if we need to disable texturing 
        if(cop == D3DTOP_DISABLE || 
            (cop == D3DTOP_SELECTARG2 && ca2 == D3DTA_DIFFUSE && 
             aop == D3DTOP_SELECTARG2 && aa2 == D3DTA_DIFFUSE))
        {
            pCtxt->tssStates[D3DTSS_TEXTUREMAP] = 0;
            pCtxt->dwRCode &= ~S3TEXTURED;
            return DD_OK;
        }

        // setup the address mode
        pCtxt->TextureAddress = tad;

        // Setup the equivalent texture filtering state
        if(mip == D3DTFP_NONE) 
        {
            if (min == D3DTFN_LINEAR ||  mag == D3DTFN_LINEAR)
            {
                pCtxt->TextureMode = D3DFILTER_LINEAR;
            }
            else if (min == D3DTFG_POINT ||  mag == D3DTFG_POINT)
            {
                pCtxt->TextureMode = D3DFILTER_NEAREST;
            }
        }
        else if(mip == D3DTFP_POINT) 
        {
            if(min == D3DTFN_POINT) 
            {
                pCtxt->TextureMode = D3DFILTER_MIPNEAREST;
            }
            else if(min == D3DTFN_LINEAR) 
            {
                pCtxt->TextureMode = D3DFILTER_MIPLINEAR;
            }
        }
        else 
        { // mip == D3DTFP_LINEAR
            if(min == D3DTFN_POINT) 
            {
                pCtxt->TextureMode = D3DFILTER_LINEARMIPNEAREST;
            }
            else if(min == D3DTFN_LINEAR) 
            {
                pCtxt->TextureMode = D3DFILTER_LINEARMIPLINEAR;
            }
        }

        // Setup the equivalent texture blending state
        // Check if we need to decal

        if((ca1 == D3DTA_TEXTURE && cop == D3DTOP_SELECTARG1) &&
            (aa1 == D3DTA_TEXTURE && aop == D3DTOP_SELECTARG1)) 
        {
            pCtxt->BlendMode = D3DTBLEND_COPY;
        }
        // Check if we need to modulate
        else if((ca2 == D3DTA_DIFFUSE && ca1 == D3DTA_TEXTURE) &&
                 cop == D3DTOP_MODULATE &&
                (aa1 == D3DTA_TEXTURE && aop == D3DTOP_SELECTARG1)) 
        {
            pCtxt->BlendMode = D3DTBLEND_MODULATE;
            // we might need to fix the vertex color alpha 
            // channel if the texture doesn't have an alpha channel
            pCtxt->Alpha_workaround = TRUE;
        }
        // Check if we need to modulate ( legacy style )
        else if((ca2 == D3DTA_DIFFUSE && ca1 == D3DTA_TEXTURE) &&
                 cop == D3DTOP_MODULATE &&
                (aa1 == D3DTA_TEXTURE && aop == D3DTOP_LEGACY_ALPHAOVR)) 
        {
            pCtxt->BlendMode = D3DTBLEND_MODULATE;
        }
        // Check if we need to decal alpha
        else if((ca2 == D3DTA_DIFFUSE && ca1 == D3DTA_TEXTURE) && 
                 cop == D3DTOP_BLENDTEXTUREALPHA &&
                (aa2 == D3DTA_DIFFUSE && aop == D3DTOP_SELECTARG2)) 
        {
            pCtxt->BlendMode = D3DTBLEND_DECALALPHA;
        }
        // Check if we need to modulate alpha
        else if((ca2 == D3DTA_DIFFUSE && ca1 == D3DTA_TEXTURE) && 
                 cop == D3DTOP_MODULATE &&
                (aa2 == D3DTA_DIFFUSE && aa1 == D3DTA_TEXTURE) && 
                 aop == D3DTOP_MODULATE) 
        {
            pCtxt->BlendMode = D3DTBLEND_MODULATEALPHA;
        }
        // Check if we need to add
        else if((ca2 == D3DTA_DIFFUSE && ca1 == D3DTA_TEXTURE) && 
                 cop == D3DTOP_ADD &&
                (aa2 == D3DTA_DIFFUSE && aop == D3DTOP_SELECTARG2)) 
        {
            pCtxt->BlendMode = D3DTBLEND_ADD;
        }

        __HWSetupTexture( pCtxt , 
                            TextureHandleToPtr(pCtxt->tssStates[D3DTSS_TEXTUREMAP], pCtxt));
        pCtxt->dwRCode |= S3TEXTURED;

    } else
        // No texturing
        pCtxt->dwRCode &= ~S3TEXTURED;

    return DD_OK;
}

//-----------------------------------------------------------------------------
//
// void __MapRS_Into_TSS0
//
// Map Renderstate changes into the corresponding change in the Texture Stage
// State #0 .
//
//-----------------------------------------------------------------------------
void 
__MapRS_Into_TSS0(S3_CONTEXT* pContext,
                  DWORD dwRSType,
                  DWORD dwRSVal)
{
    DPF_DBG("Entering __MapRS_Into_TSS0");

    // Process each specific renderstate
    switch (dwRSType)
    {

    case D3DRENDERSTATE_TEXTUREHANDLE:
        //Mirror texture related render states into TSS stage 0
        pContext->tssStates[D3DTSS_TEXTUREMAP] = dwRSVal;
        break;

    case D3DRENDERSTATE_TEXTUREMAPBLEND:
        switch (dwRSVal)
        {
            case D3DTBLEND_DECALALPHA:
                //Mirror texture related render states into TSS stage 0
                pContext->tssStates[D3DTSS_COLOROP] =
                                               D3DTOP_BLENDTEXTUREALPHA;
                pContext->tssStates[D3DTSS_COLORARG1] = D3DTA_TEXTURE;
                pContext->tssStates[D3DTSS_COLORARG2] = D3DTA_DIFFUSE;
                pContext->tssStates[D3DTSS_ALPHAOP] = D3DTOP_SELECTARG2;
                pContext->tssStates[D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;
                pContext->tssStates[D3DTSS_ALPHAARG2] = D3DTA_DIFFUSE;
                break;
            case D3DTBLEND_MODULATE:
                //Mirror texture related render states into TSS stage 0
                pContext->tssStates[D3DTSS_COLOROP] = D3DTOP_MODULATE;
                pContext->tssStates[D3DTSS_COLORARG1] = D3DTA_TEXTURE;
                pContext->tssStates[D3DTSS_COLORARG2] = D3DTA_DIFFUSE;
                // a special legacy alpha operation is called for
                // that depends on the format of the texture
                pContext->tssStates[D3DTSS_ALPHAOP] = D3DTOP_LEGACY_ALPHAOVR;
                pContext->tssStates[D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;
                pContext->tssStates[D3DTSS_ALPHAARG2] = D3DTA_DIFFUSE;
                break;
            case D3DTBLEND_MODULATEALPHA:
                //Mirror texture related render states into TSS stage 0
                pContext->tssStates[D3DTSS_COLOROP] = D3DTOP_MODULATE;
                pContext->tssStates[D3DTSS_COLORARG1] = D3DTA_TEXTURE;
                pContext->tssStates[D3DTSS_COLORARG2] = D3DTA_DIFFUSE;
                pContext->tssStates[D3DTSS_ALPHAOP] = D3DTOP_MODULATE;
                pContext->tssStates[D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;;
                pContext->tssStates[D3DTSS_ALPHAARG2] = D3DTA_DIFFUSE;
                break;
            case D3DTBLEND_COPY:
            case D3DTBLEND_DECAL:
                //Mirror texture related render states into TSS stage 0
                pContext->tssStates[D3DTSS_COLOROP] = D3DTOP_SELECTARG1;
                pContext->tssStates[D3DTSS_COLORARG1] = D3DTA_TEXTURE;
                pContext->tssStates[D3DTSS_ALPHAOP] = D3DTOP_SELECTARG1;
                pContext->tssStates[D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;
                break;
            case D3DTBLEND_ADD:
                //Mirror texture related render states into TSS stage 0
                pContext->tssStates[D3DTSS_COLOROP] = D3DTOP_ADD;
                pContext->tssStates[D3DTSS_COLORARG1] = D3DTA_TEXTURE;
                pContext->tssStates[D3DTSS_COLORARG2] = D3DTA_DIFFUSE;
                pContext->tssStates[D3DTSS_ALPHAOP] = D3DTOP_SELECTARG2;
                pContext->tssStates[D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;
                pContext->tssStates[D3DTSS_ALPHAARG2] = D3DTA_DIFFUSE;
        }
        break;

    case D3DRENDERSTATE_BORDERCOLOR:
        //Mirror texture related render states into TSS stage 0
        pContext->tssStates[D3DTSS_BORDERCOLOR] = dwRSVal;
        break;

    case D3DRENDERSTATE_MIPMAPLODBIAS:
        //Mirror texture related render states into TSS stage 0
        pContext->tssStates[D3DTSS_MIPMAPLODBIAS] = dwRSVal;
        break;

    case D3DRENDERSTATE_ANISOTROPY:
        //Mirror texture related render states into TSS stage 0
        pContext->tssStates[D3DTSS_MAXANISOTROPY] = dwRSVal;
        break;

    case D3DRENDERSTATE_TEXTUREADDRESS:
        //Mirror texture related render states into TSS stage 0
        pContext->tssStates[D3DTSS_ADDRESSU] =
        pContext->tssStates[D3DTSS_ADDRESSV] = dwRSVal; 
        break;

    case D3DRENDERSTATE_TEXTUREADDRESSU:
        //Mirror texture related render states into TSS stage 0
        pContext->tssStates[D3DTSS_ADDRESSU] = dwRSVal;
        break;

    case D3DRENDERSTATE_TEXTUREADDRESSV:
        //Mirror texture related render states into TSS stage 0
        pContext->tssStates[D3DTSS_ADDRESSV] = dwRSVal;
        break;

    case D3DRENDERSTATE_TEXTUREMAG:
        switch(dwRSVal)
        {
            case D3DFILTER_NEAREST:
                pContext->tssStates[D3DTSS_MAGFILTER] = D3DTFG_POINT;
                break;
            case D3DFILTER_LINEAR:
            case D3DFILTER_MIPLINEAR:
            case D3DFILTER_MIPNEAREST:
            case D3DFILTER_LINEARMIPNEAREST:
            case D3DFILTER_LINEARMIPLINEAR:
                pContext->tssStates[D3DTSS_MAGFILTER] = D3DTFG_LINEAR;
                break;
            default:
                break;
        }
        break;

    case D3DRENDERSTATE_TEXTUREMIN:
        switch(dwRSVal)
        {
            case D3DFILTER_NEAREST:
                pContext->tssStates[D3DTSS_MINFILTER] = D3DTFN_POINT;
                pContext->tssStates[D3DTSS_MIPFILTER] = D3DTFP_NONE;
                break;
            case D3DFILTER_LINEAR:
                pContext->tssStates[D3DTSS_MINFILTER] = D3DTFN_LINEAR;
                pContext->tssStates[D3DTSS_MIPFILTER] = D3DTFP_NONE;
                break;
            case D3DFILTER_MIPNEAREST:
                pContext->tssStates[D3DTSS_MINFILTER] = D3DTFN_POINT;
                pContext->tssStates[D3DTSS_MIPFILTER] = D3DTFP_POINT;
                break;
            case D3DFILTER_MIPLINEAR:
                pContext->tssStates[D3DTSS_MINFILTER] = D3DTFN_LINEAR;
                pContext->tssStates[D3DTSS_MIPFILTER] = D3DTFP_POINT;
                break;
            case D3DFILTER_LINEARMIPNEAREST:
                pContext->tssStates[D3DTSS_MINFILTER] = D3DTFN_POINT;
                pContext->tssStates[D3DTSS_MIPFILTER] = D3DTFP_LINEAR;
                break;
            case D3DFILTER_LINEARMIPLINEAR:
                pContext->tssStates[D3DTSS_MINFILTER] = D3DTFN_LINEAR;
                pContext->tssStates[D3DTSS_MIPFILTER] = D3DTFP_LINEAR;
                break;;
            default:
                break;
        }
        break;

    default:
        // All other renderstates don't have a corresponding TSS state so
        // we don't have to worry about mapping them.
        break;

    } // switch (dwRSType of renderstate)

    DPF_DBG("Exiting __MapRS_Into_TSS0");

} // __MapRS_Into_TSS0


//*****************************************************************************
//
// HRESULT __HWSetupState
//
// Stores the state changes in the current rendering context
//
//*****************************************************************************
#define RSTATE_UPDATE(lpdwRS, dwStateNum, dwStateVal)  \
    if (lpdwRS) {                                      \
         lpdwRS[dwStateNum] = dwStateVal;              \
    }

HRESULT WINAPI __HWSetupState(S3_CONTEXT *pCtxt,DWORD StateType,
                              DWORD StateValue, LPDWORD lpdwRStates)
{
    HRESULT hr;

    DPF_DBG("State change = %8d ( %8x) value = %8d ( %8x )",
            StateType,StateType,StateValue,StateValue);
    // The state to deal with comes in the renderStateType var.
    switch(StateType) {

    case D3DRENDERSTATE_TEXTUREHANDLE:
        // Get Texture
        pCtxt->bChanged = TRUE;

        if( pCtxt->dwTexture == StateValue )
            return  DD_OK;   // no change  

        pCtxt->dwTexture = StateValue;
        RSTATE_UPDATE(lpdwRStates, StateType, (DWORD)pCtxt->dwTexture);

        if (pCtxt->dwTexture)
            pCtxt->dwRCode |= S3TEXTURED; 
        else 
            pCtxt->dwRCode &= ~S3TEXTURED;

        if(pCtxt->dwTexture) {
            hr = __HWSetupTexture(pCtxt,
                                  TextureHandleToPtr(pCtxt->dwTexture, pCtxt));
            RSTATE_UPDATE(lpdwRStates, StateType, (DWORD)pCtxt->dwTexture);
            return hr;
        }

        break;

    case D3DRENDERSTATE_TEXTUREADDRESS:
        pCtxt->TextureAddress = (D3DTEXTUREADDRESS) StateValue;
        pCtxt->bChanged = TRUE;
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue);
        break;

    case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        // Turn our global to the value sent (0=off , 1=on)
        if(StateValue)
            pCtxt->dwRCode |= S3PERSPECTIVE; 
        else
            pCtxt->dwRCode &= ~S3PERSPECTIVE;
        pCtxt->bChanged = TRUE;       
        RSTATE_UPDATE(lpdwRStates, StateType, 
                        (ULONG)(pCtxt->dwRCode & S3PERSPECTIVE) );
        break;

    case D3DRENDERSTATE_WRAPU:
        pCtxt->bWrapU = StateValue;  
        // Doesn't affect the hw state setup so we don't need to flag bChanged
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue);
        break;

    case D3DRENDERSTATE_WRAPV:
        pCtxt->bWrapV = StateValue; 
        // Doesn't affect the hw state setup so we don't need to flag bChanged
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue);
        break;

    case D3DRENDERSTATE_WRAP0:
        pCtxt->bWrapU = (StateValue & D3DWRAP_U);
        pCtxt->bWrapV = (StateValue & D3DWRAP_V);
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue);
        break;

    case D3DRENDERSTATE_ZENABLE:
        if (pCtxt->bZEnabled = (BOOL) StateValue)
            pCtxt->dwRCode |= S3ZBUFFER; 
        else
            pCtxt->dwRCode &= ~S3ZBUFFER;
        pCtxt->bChanged = TRUE;
        RSTATE_UPDATE(lpdwRStates, StateType, 
                        (ULONG)(pCtxt->dwRCode & S3ZBUFFER) );
        break;

    case D3DRENDERSTATE_FILLMODE:
        pCtxt->FillMode = (D3DFILLMODE) StateValue;
        pCtxt->bChanged = TRUE;
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue);
        break;

    case D3DRENDERSTATE_SHADEMODE:
        pCtxt->ShadeMode = (D3DSHADEMODE) StateValue;

        if ((D3DSHADEMODE) StateValue != D3DSHADE_FLAT)
            pCtxt->dwRCode |= S3GOURAUD;
        else
            pCtxt->dwRCode &= ~S3GOURAUD;
        pCtxt->bChanged = TRUE;
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue);
        break;

    case D3DRENDERSTATE_ZWRITEENABLE:
        pCtxt->bZWriteEnabled = StateValue;
        pCtxt->bChanged = TRUE;
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue);
        break;

    case D3DRENDERSTATE_TEXTUREMAG:
        pCtxt->TextureMode = (D3DTEXTUREFILTER) StateValue;
        pCtxt->bChanged = TRUE;
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue);
        break;

    case D3DRENDERSTATE_TEXTUREMIN:     
        pCtxt->TextureMode = (D3DTEXTUREFILTER) StateValue;
        pCtxt->bChanged = TRUE;
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue); 
        break;

    case D3DRENDERSTATE_SRCBLEND:       
        if (( StateValue == D3DBLEND_ONE ) ||
            ( StateValue == D3DBLEND_SRCALPHA )) {
            pCtxt->SrcBlend = StateValue;
            pCtxt->bChanged = TRUE;
        }
        RSTATE_UPDATE(lpdwRStates, StateType, pCtxt->SrcBlend);
        break;

    case D3DRENDERSTATE_DESTBLEND:      
        if (( StateValue == D3DBLEND_ZERO ) ||
            ( StateValue == D3DBLEND_INVSRCALPHA )) {
            pCtxt->DstBlend = StateValue;
            pCtxt->bChanged = TRUE;
        } 
        RSTATE_UPDATE(lpdwRStates, StateType, pCtxt->DstBlend);
        break;

    case D3DRENDERSTATE_TEXTUREMAPBLEND:
        pCtxt->BlendMode = StateValue;
        pCtxt->bChanged = TRUE;
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue);
        break;

    case D3DRENDERSTATE_CULLMODE:       
        pCtxt->CullMode = StateValue; 
        // Doesn't affect the hw state setup so we don't need to flag bChanged
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue);
        break;

    case D3DRENDERSTATE_ZFUNC:          
        pCtxt->ZCmpFunc = (D3DCMPFUNC) StateValue;
        pCtxt->bChanged = TRUE;
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue);
        break;

    case D3DRENDERSTATE_ALPHABLENDENABLE:
    //case D3DRENDERSTATE_BLENDENABLE: 
        if (StateValue)
            pCtxt->dwStatus |= S3ALPHABLENDENABLE;
        else
            pCtxt->dwStatus &= ~S3ALPHABLENDENABLE;
        DPF_DBG("ALPHA StateValue=%d",StateValue);
        pCtxt->bChanged = TRUE;
        RSTATE_UPDATE(lpdwRStates, StateType, 
                        (ULONG)(pCtxt->dwStatus & S3ALPHABLENDENABLE) );
        break;

    case D3DRENDERSTATE_FOGENABLE:
        if (StateValue)
            pCtxt->dwStatus |= S3FOGENABLE;
        else
            pCtxt->dwStatus &= ~S3FOGENABLE;
        pCtxt->bChanged = TRUE;
        RSTATE_UPDATE(lpdwRStates, StateType, 
                        (ULONG)(pCtxt->dwStatus & S3FOGENABLE) );
        break;

    case D3DRENDERSTATE_SPECULARENABLE:
        if (StateValue)
            pCtxt->dwStatus |= S3SPECULARENABLE;
        else
            pCtxt->dwStatus &= ~S3SPECULARENABLE;
        pCtxt->bChanged = TRUE;
        RSTATE_UPDATE(lpdwRStates, StateType, 
                        (ULONG)(pCtxt->dwStatus & S3SPECULARENABLE) );
        break;

    case D3DRENDERSTATE_ZVISIBLE:
        pCtxt->bZVisible = StateValue;   
        // Doesn't affect the hw state setup so we don't need to flag bChanged
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue);
        break;

    case D3DRENDERSTATE_FOGCOLOR:        
        pCtxt->rnd3DSetup.stp_FOG_CLR = StateValue; 
        // Doesn't affect the hw state setup so we don't need to flag bChanged
        RSTATE_UPDATE(lpdwRStates, StateType, StateValue);
        break;

    case D3DRENDERSTATE_COLORKEYENABLE: 
        if (StateValue)
            pCtxt->dwStatus |= S3COLORKEYENABLE;
        else
            pCtxt->dwStatus &= ~S3COLORKEYENABLE;
        DPF_DBG("COLOR StateValue=%d",StateValue);
        pCtxt->bChanged = TRUE;
        RSTATE_UPDATE(lpdwRStates, StateType, 
                        (ULONG)(pCtxt->dwStatus & S3COLORKEYENABLE) );
        break;


    /* Not implemented */
    case D3DRENDERSTATE_ANTIALIAS:
    case D3DRENDERSTATE_LINEPATTERN:
    case D3DRENDERSTATE_MONOENABLE:
    case D3DRENDERSTATE_ROP2:
    case D3DRENDERSTATE_PLANEMASK:
    case D3DRENDERSTATE_ALPHATESTENABLE:
    case D3DRENDERSTATE_LASTPIXEL:
    case D3DRENDERSTATE_ALPHAREF:
    case D3DRENDERSTATE_ALPHAFUNC:
    case D3DRENDERSTATE_DITHERENABLE:
    case D3DRENDERSTATE_SUBPIXEL:
    case D3DRENDERSTATE_SUBPIXELX:
    case D3DRENDERSTATE_STIPPLEDALPHA:
    
    case D3DRENDERSTATE_FOGTABLEMODE:
    case D3DRENDERSTATE_FOGTABLESTART:
    case D3DRENDERSTATE_FOGTABLEEND:
    case D3DRENDERSTATE_FOGTABLEDENSITY:

    case D3DRENDERSTATE_STIPPLEENABLE:
    case D3DRENDERSTATE_EDGEANTIALIAS:

    case D3DRENDERSTATE_BORDERCOLOR:
    case D3DRENDERSTATE_TEXTUREADDRESSU:
    case D3DRENDERSTATE_TEXTUREADDRESSV:
    case D3DRENDERSTATE_MIPMAPLODBIAS:
    case D3DRENDERSTATE_ZBIAS:
    case D3DRENDERSTATE_RANGEFOGENABLE:
    case D3DRENDERSTATE_ANISOTROPY:
    case D3DRENDERSTATE_FLUSHBATCH:
   
    case D3DRENDERSTATE_STIPPLEPATTERN00:
    case D3DRENDERSTATE_STIPPLEPATTERN01:
    case D3DRENDERSTATE_STIPPLEPATTERN02:
    case D3DRENDERSTATE_STIPPLEPATTERN03:
    case D3DRENDERSTATE_STIPPLEPATTERN04:
    case D3DRENDERSTATE_STIPPLEPATTERN05:
    case D3DRENDERSTATE_STIPPLEPATTERN06:
    case D3DRENDERSTATE_STIPPLEPATTERN07:
    case D3DRENDERSTATE_STIPPLEPATTERN08:
    case D3DRENDERSTATE_STIPPLEPATTERN09:
    case D3DRENDERSTATE_STIPPLEPATTERN10:
    case D3DRENDERSTATE_STIPPLEPATTERN11:
    case D3DRENDERSTATE_STIPPLEPATTERN12:
    case D3DRENDERSTATE_STIPPLEPATTERN13:
    case D3DRENDERSTATE_STIPPLEPATTERN14:
    case D3DRENDERSTATE_STIPPLEPATTERN15:
    case D3DRENDERSTATE_STIPPLEPATTERN16:
    case D3DRENDERSTATE_STIPPLEPATTERN17:
    case D3DRENDERSTATE_STIPPLEPATTERN18:
    case D3DRENDERSTATE_STIPPLEPATTERN19:
    case D3DRENDERSTATE_STIPPLEPATTERN20:
    case D3DRENDERSTATE_STIPPLEPATTERN21:
    case D3DRENDERSTATE_STIPPLEPATTERN22:
    case D3DRENDERSTATE_STIPPLEPATTERN23:
    case D3DRENDERSTATE_STIPPLEPATTERN24:
    case D3DRENDERSTATE_STIPPLEPATTERN25:
    case D3DRENDERSTATE_STIPPLEPATTERN26:
    case D3DRENDERSTATE_STIPPLEPATTERN27:
    case D3DRENDERSTATE_STIPPLEPATTERN28:
    case D3DRENDERSTATE_STIPPLEPATTERN29:
    case D3DRENDERSTATE_STIPPLEPATTERN30:
    case D3DRENDERSTATE_STIPPLEPATTERN31:


    // DX7 renderstates we ignore
    case D3DRENDERSTATE_CLIPPING:
    case D3DRENDERSTATE_LIGHTING:
    case D3DRENDERSTATE_EXTENTS:
    case D3DRENDERSTATE_AMBIENT:
    case D3DRENDERSTATE_FOGVERTEXMODE:
    case D3DRENDERSTATE_COLORVERTEX:
    case D3DRENDERSTATE_LOCALVIEWER:
    case D3DRENDERSTATE_NORMALIZENORMALS:
    case D3DRENDERSTATE_COLORKEYBLENDENABLE:
    case D3DRENDERSTATE_DIFFUSEMATERIALSOURCE:
    case D3DRENDERSTATE_SPECULARMATERIALSOURCE:
    case D3DRENDERSTATE_AMBIENTMATERIALSOURCE:
    case D3DRENDERSTATE_EMISSIVEMATERIALSOURCE:
    case D3DRENDERSTATE_SCENECAPTURE:
        // This state pass TRUE or FALSE to replace the functionality
    // of D3DHALCallbacks->SceneCapture()

    default:
        // not handled is a do not care
        RSTATE_UPDATE(lpdwRStates, StateType, 0);
        break;
    }

    __MapRS_Into_TSS0(pCtxt, StateType, StateValue);
    return DD_OK;
}

#if D3D_STATEBLOCKS
//-----------------------------------------------------------------------------
//
// P2StateSetRec *FindStateSet
//
// Find a state identified by dwHandle starting from pRootSS.
// If not found, returns NULL.
//
//-----------------------------------------------------------------------------
S3StateSetRec *FindStateSet(S3_CONTEXT* pContext,
                            DWORD dwHandle)
{
    if (dwHandle <= pContext->dwMaxSSIndex)
        return pContext->pIndexTableSS[dwHandle - 1];
    else
    {
        DPF_DBG("State set %x not found (Max = %x)",
                    dwHandle, pContext->dwMaxSSIndex);
        return NULL;
    }
}

//-----------------------------------------------------------------------------
//
// void DumpStateSet
//
// Dump info stored in a state set
//
//-----------------------------------------------------------------------------
#define ELEMS_IN_ARRAY(a) ((sizeof(a)/sizeof(a[0])))

void DumpStateSet(S3StateSetRec *pSSRec)
{
    DWORD i;

    DPF_DBG("DumpStateSet %x, Id=%x bCompressed=%x",
                pSSRec,pSSRec->dwHandle,pSSRec->bCompressed);

    if (!pSSRec->bCompressed)
    {
        // uncompressed state set

        // Dump render states values
        for (i=0; i< MAX_STATE; i++)
        {
            DPF_DBG(0,"RS %x = %x",i, pSSRec->u.uc.RenderStates[i]);
        }

        // Dump TSS's values
        for (i=0; i<= D3DTSS_TEXTURETRANSFORMFLAGS; i++)
        {
            DPF_DBG("TSS %x = %x",i, pSSRec->u.uc.TssStates[i]);
        }

        // Dump RS bit masks
        for (i=0; i< ELEMS_IN_ARRAY(pSSRec->u.uc.bStoredRS); i++)
        {
            DPF_DBG("bStoredRS[%x] = %x",i, pSSRec->u.uc.bStoredRS[i]);
        }

        // Dump TSS bit masks
        for (i=0; i< ELEMS_IN_ARRAY(pSSRec->u.uc.bStoredTSS); i++)
        {
            DPF_DBG(0,"bStoredTSS[%x] = %x",i, pSSRec->u.uc.bStoredTSS[i]);
        }

    }
    else
    {
        // compressed state set

        DPF_DBG("dwNumRS =%x  dwNumTSS=%x",
                    pSSRec->u.cc.dwNumRS,pSSRec->u.cc.dwNumTSS);

        // dump compressed state
        for (i=0; i< pSSRec->u.cc.dwNumTSS + pSSRec->u.cc.dwNumRS; i++)
        {
            DPF_DBG("RS/TSS %x = %x",
                        pSSRec->u.cc.pair[i].dwType, 
                        pSSRec->u.cc.pair[i].dwValue);
        }

    }

}

//-----------------------------------------------------------------------------
//
// void AddStateSetIndexTableEntry
//
// Add an antry to the index table. If necessary, grow it.
//-----------------------------------------------------------------------------
void AddStateSetIndexTableEntry(S3_CONTEXT* pContext,
                                DWORD dwNewHandle,
                                S3StateSetRec *pNewSSRec)
{
    DWORD dwNewSize;
    S3StateSetRec **pNewIndexTableSS;

    // If the current list is not large enough, we'll have to grow a new one.
    if (dwNewHandle > pContext->dwMaxSSIndex)
    {
        // New size of our index table
        // (round up dwNewHandle in steps of SSPTRS_PERPAGE)
        dwNewSize = ((dwNewHandle -1 + SSPTRS_PERPAGE) / SSPTRS_PERPAGE)
                      * SSPTRS_PERPAGE;

        // we have to grow our list
        pNewIndexTableSS = (S3StateSetRec **)MEMALLOC(dwNewSize*sizeof(S3StateSetRec *));

        if (pContext->pIndexTableSS)
        {
            // if we already had a previous list, we must transfer its data
            memcpy(pNewIndexTableSS, 
                   pContext->pIndexTableSS,
                   pContext->dwMaxSSIndex*sizeof(S3StateSetRec *));
            
            //and get rid of it
            MEMFREE(pContext->pIndexTableSS);
        }

        // New index table data
        pContext->pIndexTableSS = pNewIndexTableSS;
        pContext->dwMaxSSIndex = dwNewSize;
    }

    // Store our state set pointer into our access list
    pContext->pIndexTableSS[dwNewHandle - 1] = pNewSSRec;
}

//-----------------------------------------------------------------------------
//
// void CompressStateSet
//
// Compress a state set so it uses the minimum necessary space. Since we expect 
// some apps to make extensive use of state sets we want to keep things tidy.
// Returns address of new structure (ir old, if it wasn't compressed)
//
//-----------------------------------------------------------------------------
S3StateSetRec * CompressStateSet(S3_CONTEXT* pContext,
                                 S3StateSetRec *pUncompressedSS)
{
    S3StateSetRec *pCompressedSS;
    DWORD i, dwSize, dwIndex, dwCount;

    // Create a new state set of just the right size we need

    // Calculate how large 
    dwCount = 0;
    for (i=0; i< MAX_STATE; i++)
        if (IS_FLAG_SET(pUncompressedSS->u.uc.bStoredRS , i))
        {
            dwCount++;
        };

    for (i=0; i<= D3DTSS_TEXTURETRANSFORMFLAGS; i++)
        if (IS_FLAG_SET(pUncompressedSS->u.uc.bStoredTSS , i))
        {
            dwCount++;
        };

    // Create a new state set of just the right size we need
    // ANY CHANGE MADE TO THE S3StateSetRec structure MUST BE REFLECTED HERE!
    dwSize = 2*sizeof(DWORD) +                          // handle , flags
             2*sizeof(DWORD) +                          // # of RS & TSS
             2*dwCount*sizeof(DWORD);                   // compressed structure

    if (dwSize >= sizeof(S3StateSetRec))
    {
        // it is not efficient to compress, leave uncompressed !
        pUncompressedSS->bCompressed = FALSE;
        return pUncompressedSS;
    }

    pCompressedSS = (S3StateSetRec *)MEMALLOC(dwSize);

    if (pCompressedSS)
    {
        // adjust data in new compressed state set
        pCompressedSS->bCompressed = TRUE;
        pCompressedSS->dwHandle = pUncompressedSS->dwHandle;

        // Transfer our info to this new state set
        pCompressedSS->u.cc.dwNumRS = 0;
        pCompressedSS->u.cc.dwNumTSS = 0;
        dwIndex = 0;

        for (i=0; i< MAX_STATE; i++)
            if (IS_FLAG_SET(pUncompressedSS->u.uc.bStoredRS , i))
            {
                pCompressedSS->u.cc.pair[dwIndex].dwType = i;
                pCompressedSS->u.cc.pair[dwIndex].dwValue = 
                                    pUncompressedSS->u.uc.RenderStates[i];
                pCompressedSS->u.cc.dwNumRS++;
                dwIndex++;
            }

        for (i=0; i<= D3DTSS_TEXTURETRANSFORMFLAGS; i++)
            if (IS_FLAG_SET(pUncompressedSS->u.uc.bStoredTSS , i))
            {
                pCompressedSS->u.cc.pair[dwIndex].dwType = i;
                pCompressedSS->u.cc.pair[dwIndex].dwValue = 
                                    pUncompressedSS->u.uc.TssStates[i];
                pCompressedSS->u.cc.dwNumTSS++;
                dwIndex++;
            }

        // Get rid of the old(uncompressed) one
        MEMFREE(pUncompressedSS);
        return pCompressedSS;

    }
    else
    {
        DPF_DBG("Not enough memory left to compress D3D state set");
        pUncompressedSS->bCompressed = FALSE;
        return pUncompressedSS;
    }

}

//-----------------------------------------------------------------------------
//
// void __DeleteAllStateSets
//
// Delete any remaining state sets for cleanup purpouses
//
//-----------------------------------------------------------------------------
void __DeleteAllStateSets(S3_CONTEXT* pContext)
{
    S3StateSetRec *pSSRec;
    DWORD dwSSIndex;

    DPF_DBG("Entering __DeleteAllStateSets");

    if (pContext->pIndexTableSS)
    {
        for(dwSSIndex = 0; dwSSIndex < pContext->dwMaxSSIndex; dwSSIndex++)
        {
            if (pSSRec = pContext->pIndexTableSS[dwSSIndex])
            {
                MEMFREE(pSSRec);
            }
        }

        // free fast index table
        MEMFREE(pContext->pIndexTableSS);
    }

    DPF_DBG("Exiting __DeleteAllStateSets");
}

//-----------------------------------------------------------------------------
//
// void __BeginStateSet
//
// Create a new state set identified by dwParam and start recording states
//
//-----------------------------------------------------------------------------
void __BeginStateSet(S3_CONTEXT* pContext, DWORD dwParam)
{
    S3StateSetRec *pSSRec;

    DPF_DBG("Entering __BeginStateSet dwParam=%08lx",dwParam);

    // Create a new state set
    pSSRec = (S3StateSetRec *)MEMALLOC(sizeof(S3StateSetRec));

    if (!pSSRec)
    {
        DPF_DBG("Run out of memory for additional state sets");
        return;
    }

    // remember handle to current state set
    pSSRec->dwHandle = dwParam;
    pSSRec->bCompressed = FALSE;

    // Get pointer to current recording state set
    pContext->pCurrSS = pSSRec;

    // Start recording mode
    pContext->bStateRecMode = TRUE;

    DPF_DBG("Exiting __BeginStateSet");
}

//-----------------------------------------------------------------------------
//
// void __EndStateSet
//
// stop recording states - revert to executing them.
//
//-----------------------------------------------------------------------------
void __EndStateSet(S3_CONTEXT* pContext)
{
    DWORD dwHandle;
    S3StateSetRec *pNewSSRec;

    DPF_DBG("Entering __EndStateSet");

    if (pContext->pCurrSS)
    {
        dwHandle = pContext->pCurrSS->dwHandle;

        // compress the current state set
        // Note: after being compressed the uncompressed version is free'd.
        pNewSSRec = CompressStateSet(pContext, pContext->pCurrSS);

        AddStateSetIndexTableEntry(pContext, dwHandle, pNewSSRec);
    }

    // No state set being currently recorded
    pContext->pCurrSS = NULL;

    // End recording mode
    pContext->bStateRecMode = FALSE;


    DPF_DBG("Exiting __EndStateSet");
}

//-----------------------------------------------------------------------------
//
// void __DeleteStateSet
//
// Delete the recorder state ste identified by dwParam
//
//-----------------------------------------------------------------------------
void __DeleteStateSet(S3_CONTEXT* pContext, DWORD dwParam)
{
    S3StateSetRec *pSSRec;
    DWORD i;

    DPF_DBG("Entering __DeleteStateSet dwParam=%08lx",dwParam);


    if (pSSRec = FindStateSet(pContext, dwParam))
    {
        // Clear index table entry
        pContext->pIndexTableSS[dwParam - 1] = NULL;

        // Now delete the actual state set structure
        MEMFREE(pSSRec);
    }

    DPF_DBG("Exiting __DeleteStateSet");
}

//-----------------------------------------------------------------------------
//
// void __ExecuteStateSet
//
//
//-----------------------------------------------------------------------------
void __ExecuteStateSet(S3_CONTEXT* pContext, DWORD dwParam)
{

    S3StateSetRec *pSSRec;
    DWORD i;

    DPF_DBG("Entering __ExecuteStateSet dwParam=%08lx",dwParam);


    if (pSSRec = FindStateSet(pContext, dwParam))
    {

        if (!pSSRec->bCompressed)
        {
            // uncompressed state set

            // Execute any necessary render states
            for (i=0; i< MAX_STATE; i++)
                if (IS_FLAG_SET(pSSRec->u.uc.bStoredRS , i))
                {
                    DWORD dwRSType, dwRSVal;

                    dwRSType = i;
                    dwRSVal = pSSRec->u.uc.RenderStates[dwRSType];

                    // Store the state in the context
                    pContext->RenderStates[dwRSType] = dwRSVal;

                    DPF_DBG("__ExecuteStateSet RS %x = %x",
                                dwRSType, dwRSVal);

                    // Process it
                    __HWSetupState(pContext, dwRSType, dwRSVal, NULL);
                }

            // Execute any necessary TSS's
            for (i=0; i<= D3DTSS_TEXTURETRANSFORMFLAGS; i++)
                if (IS_FLAG_SET(pSSRec->u.uc.bStoredTSS , i))
                {
                    DWORD dwTSState, dwValue;

                    dwTSState = i;
                    dwValue = pSSRec->u.uc.TssStates[dwTSState];

                    DPF_DBG("__ExecuteStateSet TSS %x = %x",
                                dwTSState, dwValue);

                    // Store value associated to this stage state
                    pContext->tssStates[dwTSState] = dwValue;
                }

            // Execute any necessary state for lights, materials, transforms,
            // viewport info, z range and clip planes - here -
        }
        else
        {
            // compressed state set

            // Execute any necessary render states
            for (i=0; i< pSSRec->u.cc.dwNumRS; i++)
            {
                DWORD dwRSType, dwRSVal;

                dwRSType = pSSRec->u.cc.pair[i].dwType;
                dwRSVal = pSSRec->u.cc.pair[i].dwValue;

                // Store the state in the context
                pContext->RenderStates[dwRSType] = dwRSVal;

                DPF_DBG("__ExecuteStateSet RS %x = %x",
                            dwRSType, dwRSVal);

                // Process it
                __HWSetupState(pContext, dwRSType, dwRSVal, NULL);

            }

            // Execute any necessary TSS's
            for (; i< pSSRec->u.cc.dwNumTSS + pSSRec->u.cc.dwNumRS; i++)
            {
                DWORD dwTSState, dwValue;

                dwTSState = pSSRec->u.cc.pair[i].dwType;
                dwValue = pSSRec->u.cc.pair[i].dwValue;

                DPF_DBG("__ExecuteStateSet TSS %x = %x",
                            dwTSState, dwValue);

                // Store value associated to this stage state
                pContext->tssStates[dwTSState] = dwValue;
            }

            // Execute any necessary state for lights, materials, transforms,
            // viewport info, z range and clip planes - here -
        }
    }

    DPF_DBG("Exiting __ExecuteStateSet");
}

//-----------------------------------------------------------------------------
//
// void __CaptureStateSet
//
//
//-----------------------------------------------------------------------------
void __CaptureStateSet(S3_CONTEXT* pContext, DWORD dwParam)
{
    S3StateSetRec *pSSRec;
    DWORD i;

    DPF_DBG("Entering __CaptureStateSet dwParam=%08lx",dwParam);

    if (pSSRec = FindStateSet(pContext, dwParam))
    {
        if (!pSSRec->bCompressed)
        {
            // uncompressed state set

            // Capture any necessary render states
            for (i=0; i< MAX_STATE; i++)
                if (IS_FLAG_SET(pSSRec->u.uc.bStoredRS , i))
                {
                    pSSRec->u.uc.RenderStates[i] = pContext->RenderStates[i];
                }

            // Capture any necessary TSS's
            for (i=0; i<= D3DTSS_TEXTURETRANSFORMFLAGS; i++)
                if (IS_FLAG_SET(pSSRec->u.uc.bStoredTSS , i))
                {
            pSSRec->u.uc.TssStates[i] = pContext->tssStates[i];
                }

            // Capture any necessary state for lights, materials, transforms,
            // viewport info, z range and clip planes - here -
        }
        else
        {
            // compressed state set

            // Capture any necessary render states
            for (i=0; i< pSSRec->u.cc.dwNumRS; i++)
            {
                DWORD dwRSType;

                dwRSType = pSSRec->u.cc.pair[i].dwType;
                pSSRec->u.cc.pair[i].dwValue = pContext->RenderStates[dwRSType];

            }

            // Capture any necessary TSS's
            for (; i< pSSRec->u.cc.dwNumTSS + pSSRec->u.cc.dwNumRS; i++)
                {
                    DWORD dwTSState;

                    dwTSState = pSSRec->u.cc.pair[i].dwType;
            pSSRec->u.cc.pair[i].dwValue = pContext->tssStates[dwTSState];
                }

            // Capture any necessary state for lights, materials, transforms,
            // viewport info, z range and clip planes - here -

        }
    }

    DPF_DBG("Exiting __CaptureStateSet");
}
#endif //D3D_STATEBLOCKS

