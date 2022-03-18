/******************************Module*Header*******************************\
*
*                           *******************
*                           * D3D SAMPLE CODE *
*                           *******************
* Module Name: d3ddx6.c
*
*  Content:    Direct3D DX6 Callback function interface
*
* Copyright (C) 1998 Microsoft Corporation.  All Rights Reserved.
\**************************************************************************/

#include "precomp.h"
#include "d3ddrv.h"
#include "d3dmath.h"
#include "hw3d.h"

//**************************************************************************
//
// DX6 allows driver-level acceleration of the new vertex-buffer API. It 
// allows data and commands, indices and statechanges to be contained in 
// two separate DirectDraw surfaces. The DirectDraw surfaces can reside 
// in system, AGP, or video memory depending on the type of allocation
// requested by the user  The interface is designed to accomodate legacy
// ExecuteBuffer applications with no driver impact. This allows higher 
// performance on both legacy applications as well as the highest 
// possible performance through the vertex buffer API.
//
//**************************************************************************

#define STARTVERTEXSIZE (sizeof(D3DHAL_DP2STARTVERTEX))

#define NEXTINSTRUCTION(ptr, type, num, extrabytes)                            \
    ptr = (LPD3DHAL_DP2COMMAND)((LPBYTE)ptr + sizeof(D3DHAL_DP2COMMAND) +      \
                                ((num) * sizeof(type)) + (extrabytes))

#define NEXTINSTRUCTION_S(ptr, typesize, num, extrabytes)                      \
    ptr = (LPD3DHAL_DP2COMMAND)((LPBYTE)ptr + sizeof(D3DHAL_DP2COMMAND) +      \
                                ((num) * (typesize)) + (extrabytes))

#define PARSE_ERROR_AND_EXIT( pDP2Data, pIns, pStartIns, ddrvalue)             \
   {                                                                           \
            pDP2Data->dwErrorOffset = (DWORD)((LPBYTE)pIns-(LPBYTE)pStartIns); \
            pDP2Data->ddrval = ddrvalue;                                       \
            goto Exit_DrawPrimitives2;                                         \
   }

#define CHECK_CMDBUF_LIMITS( pDP2Data, pBuf)                                   \
   {                                                                           \
        LPBYTE pBase,pEnd;                                                     \
        pBase = (LPBYTE)(pDP2Data->lpDDCommands->lpGbl->fpVidMem +             \
                        pDP2Data->dwCommandOffset);                            \
        pEnd  = pBase + pDP2Data->dwCommandLength;                             \
        if (! ((LPBYTE)pBuf < pEnd) && ( pBase <= (LPBYTE)pBuf)) {             \
            DPF("DX6 D3D: Trying to read past Command Buffer limits "          \
                    "%x %x %x",pBase ,(LPBYTE)pBuf ,pEnd );                    \
            PARSE_ERROR_AND_EXIT( pDP2Data, lpIns, lpInsStart,                 \
                                  D3DERR_COMMAND_UNPARSED      );              \
        }                                                                      \
    }

#define CHECK_DATABUF_LIMITS( pDP2Data, iIndex)                                \
   {                                                                           \
        if (! (((LONG)iIndex >= 0) &&                                          \
               ((LONG)iIndex <(LONG)pDP2Data->dwVertexLength))) {              \
            DPF("DX6 D3D: Trying to read past Vertex Buffer limits "           \
                    "%d limit= %d ",(LONG)iIndex, (LONG)pDP2Data->dwVertexLength);         \
            PARSE_ERROR_AND_EXIT( pDP2Data, lpIns, lpInsStart,                 \
                                  D3DERR_COMMAND_UNPARSED      );              \
        }                                                                      \
    }

#define LP_FVF_VERTEX(lpBaseAddr, wIndex, S3FVFOffs)                         \
         (LPD3DTLVERTEX)((LPBYTE)(lpBaseAddr) + (wIndex) * (S3FVFOffs).dwStride)

#define LP_FVF_NXT_VTX(lpVtx, S3FVFOffs )                                    \
         (LPD3DTLVERTEX)((LPBYTE)(lpVtx) + (S3FVFOffs).dwStride)

extern HRESULT ddrvalUpdateFlipStatus(PDEV *, FLATPTR fpVidMem);
#define updateFlipStatus( a, b ) ddrvalUpdateFlipStatus(b, a)
 
HRESULT  __TextureBlt(S3_CONTEXT* pContext,
                D3DHAL_DP2TEXBLT* lpdp2texblt);
HRESULT  __SetRenderTarget(S3_CONTEXT* pContext,
                     DWORD hRenderTarget,
                     DWORD hZBuffer);
HRESULT  __Clear( S3_CONTEXT* pContext,
              DWORD   dwFlags,
              DWORD   dwFillColor,
              D3DVALUE dvFillDepth,
              DWORD   dwFillStencil,
              LPD3DRECT lpRects,
              DWORD   dwNumRects);


// Forward declaration of utility functions for FVF handling
HRESULT __CheckFVFRequest(DWORD dwFVF, LPS3FVFOFFSETS lpS3FVFOff);

/******************************Public*Routine******************************\
*
* DWORD D3DDrawPrimitives2
*
* The D3DDrawPrimitives2 callback must be filled in by drivers which directly
* support the rendering primitives using the new DDI. 
*
* PARAMETERS
*
*      lpdp2d   This structure is used when D3DDrawPrimitives2 is called 
*               to draw a set of primitives using a vertex buffer. The
*               surface specified by the lpDDCommands in 
*               D3DHAL_DRAWPRIMITIVES2DATA contains a sequence of 
*               D3DHAL_DP2COMMAND structures. Each D3DHAL_DP2COMMAND 
*               specifies either a primitive to draw, a state change to
*               process, or a re-base command.
*
\**************************************************************************/
DWORD WINAPI D3DDrawPrimitives2( LPD3DHAL_DRAWPRIMITIVES2DATA lpdp2d )
{
    LPDDRAWI_DDRAWSURFACE_LCL lpCmdLcl, lpDataLcl;
    LPD3DHAL_DP2COMMAND       lpIns, lpResumeIns;  
    LPD3DTLVERTEX             lpVertices, lpV0, lpV1, lpV2, lpV3;
    LPRENDERTRIANGLE          pTriangle; 
    LPBYTE                    lpInsStart, lpPrim;
    S3_CONTEXT                *pCtxt;
    UINT                      i,j;
    WORD                      wCount, wIndex, wIndex1, wIndex2, wIndex3, 
                              wFlags, wIndxBase;
    HRESULT                   ddrval;
    S3FVFOFFSETS              S3FVFOff;
    DWORD                     dwEdgeFlags;
    D3DHAL_DP2TEXTURESTAGESTATE *lpRState;
    DWORD                     dwRSType, dwRSVal;
    S3_TEXTURE                *pSurfRender;

    CHOP_ROUND_ON();

    DPF_DBG("D3DDrawPrimitive2 called");
    DPF_DBG("  dwhContext = %x",lpdp2d->dwhContext);
    DPF_DBG("  dwFlags = %x",lpdp2d->dwFlags);
    DPF_DBG("  dwVertexType = %d",lpdp2d->dwVertexType);
    DPF_DBG("  dwCommandOffset = %d",lpdp2d->dwCommandOffset);
    DPF_DBG("  dwCommandLength = %d",lpdp2d->dwCommandLength);
    DPF_DBG("  dwVertexOffset = %d",lpdp2d->dwVertexOffset);
    DPF_DBG("  dwVertexLength = %d",lpdp2d->dwVertexLength);

    DPF_DBG("   Compiled at " __TIME__ __DATE__ );

    pCtxt = (LPS3_CONTEXT) lpdp2d->dwhContext;

    // Check if we got a valid context handle.
    CHK_CONTEXT(pCtxt,lpdp2d->ddrval,"D3DDrawPrimitive2");

    pSurfRender = TextureHandleToPtr(pCtxt->RenderSurfaceHandle 
                                     ,pCtxt);
    if (pSurfRender)
    {
        // check to see if any pending physical flip has occurred
        lpdp2d->ddrval = updateFlipStatus( pSurfRender->fpVidMem, 
                                           D3DGLOBALPTR(pCtxt) );
        if( lpdp2d->ddrval != DD_OK ) 
            goto Exit_DrawPrimitives2;
    } else
    {
        lpdp2d->ddrval = DDERR_SURFACELOST;
        goto Exit_DrawPrimitives2;
    }

    // Get appropriate pointers to commands
    lpInsStart = (LPBYTE)(lpdp2d->lpDDCommands->lpGbl->fpVidMem);
    if (lpInsStart == NULL) {
        DPF("DX6 Command Buffer pointer is null");
        lpdp2d->ddrval = DDERR_INVALIDPARAMS;
        goto Exit_DrawPrimitives2;
    }

    lpIns = (LPD3DHAL_DP2COMMAND)(lpInsStart + lpdp2d->dwCommandOffset);

    // Check if vertex buffer resides in user memory or in a DDraw surface
    if (lpdp2d->dwFlags & D3DHALDP2_USERMEMVERTICES) {

        // Get appropriate pointer to vertices , memory is already secured
        lpVertices = (LPD3DTLVERTEX)((LPBYTE)lpdp2d->lpVertices + 
                                             lpdp2d->dwVertexOffset);
    } else {
        // Get appropriate pointer to vertices 
        lpVertices = 
           (LPD3DTLVERTEX)((LPBYTE)lpdp2d->lpDDVertex->lpGbl->fpVidMem
                                                     + lpdp2d->dwVertexOffset);
    }

    if (lpVertices == NULL) {
        DPF("DX6 Vertex Buffer pointer is null");
        lpdp2d->ddrval = DDERR_INVALIDPARAMS;
        goto Exit_DrawPrimitives2;
    }

    // Check if the FVF format being passed is invalid. 
    if (__CheckFVFRequest(lpdp2d->dwVertexType, &S3FVFOff) != DD_OK) {
        DPF("DrawPrimitives2 cannot handle Flexible Vertex Format requested");
        PARSE_ERROR_AND_EXIT(lpdp2d, lpIns, lpInsStart, D3DERR_COMMAND_UNPARSED);
    }

    // Setup everything
    pCtxt->bChanged = TRUE;

    // Setup HW for current rendering state
    ddrval = __HWSetupPrimitive( pCtxt , &S3FVFOff);
    if( ddrval != DD_OK )
    {
        DPF("Error processing D3DDP2OP_RENDERSTATE");
        PARSE_ERROR_AND_EXIT(lpdp2d, lpIns, lpInsStart, ddrval);
    }

    // Select the appropriate triangle rendering function 
    // according to the currnt rendering context
    pTriangle = __HWSetTriangleFunc(pCtxt); 

    // Process commands while we haven't exhausted the command buffer
    while ((LPBYTE)lpIns < 
           (lpInsStart + lpdp2d->dwCommandLength + lpdp2d->dwCommandOffset))  
    {
        // Get pointer to first primitive structure past the D3DHAL_DP2COMMAND
        lpPrim = (LPBYTE)lpIns + sizeof(D3DHAL_DP2COMMAND);

        DPF_DBG("D3DDrawPrimitive2: parsing instruction %d count = %d @ %x", 
                lpIns->bCommand, lpIns->wPrimitiveCount, lpIns);

        switch( lpIns->bCommand )
        {

        case D3DDP2OP_RENDERSTATE:

            // Specifies a render state change that requires processing. 
            // The rendering state to change is specified by one or more 
            // D3DHAL_DP2RENDERSTATE structures following D3DHAL_DP2COMMAND.
            
            DPF_DBG("D3DDP2OP_RENDERSTATE "
                    "state count = %d", lpIns->wStateCount);

            for (i = lpIns->wStateCount; i > 0; i--)
            { 
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);

                dwRSVal = ((D3DHAL_DP2RENDERSTATE*)lpPrim)->dwState;
                dwRSType = ((D3DHAL_DP2RENDERSTATE*)lpPrim)->RenderState;
#if D3D_STATEBLOCKS
                if (!pCtxt->bStateRecMode)
                {
                    // Store the state in the context
                    if (dwRSType < MAX_STATE)
                        pCtxt->RenderStates[dwRSType] = dwRSVal;
#endif D3D_STATEBLOCKS
                    //Tell setup not to look for multi-texture stuff
                    // if we are setting our texture through renderstates
                    if (((D3DHAL_DP2RENDERSTATE*)lpPrim)->RenderState == 
                            D3DRENDERSTATE_TEXTUREHANDLE)
                            pCtxt->dwStatus &= ~S3MULTITEXTURE;

                    __HWSetupState( pCtxt, dwRSType, dwRSVal,
                                    lpdp2d->lpdwRStates);
#if D3D_STATEBLOCKS
                }
                else
                {
                    if ((pCtxt->pCurrSS != NULL) && (dwRSType < MAX_STATE))
                    {
                        DPF_DBG("Recording RS %x = %x",dwRSType,dwRSVal);

                        // Recording the state in a stateblock
                        pCtxt->pCurrSS->u.uc.RenderStates[dwRSType] = dwRSVal;
                        FLAG_SET(pCtxt->pCurrSS->u.uc.bStoredRS,dwRSType);
                    }
                }
#endif D3D_STATEBLOCKS
                lpPrim += sizeof(D3DHAL_DP2RENDERSTATE);
            }

            NEXTINSTRUCTION(lpIns, D3DHAL_DP2RENDERSTATE, 
                            lpIns->wStateCount, 0);

            // Setup HW for current rendering state
            ddrval = __HWSetupPrimitive( pCtxt , &S3FVFOff);
            if( ddrval != DD_OK )
            {
                DPF("Error processing D3DDP2OP_RENDERSTATE");
                PARSE_ERROR_AND_EXIT(lpdp2d, lpIns, lpInsStart, ddrval);
            }

            // Update triangle rendering function
            pTriangle = __HWSetTriangleFunc(pCtxt);

            break;

        case D3DDP2OP_TEXTURESTAGESTATE:

            DPF_DBG("D3DDP2OP_TEXTURESTAGESTATE");

            lpRState = (D3DHAL_DP2TEXTURESTAGESTATE *)(lpPrim);
            for (i=0; i < lpIns->wStateCount; i++)
            {
                CHECK_CMDBUF_LIMITS(lpdp2d, lpRState);
                if (0 == lpRState->wStage)
                {
                     // Tell __HWSetupPrimitive to look at stage state data
                    if (!(S3MULTITEXTURE & pCtxt->dwStatus))
                    {
                        pCtxt->dwStatus |= S3MULTITEXTURE;
                        pCtxt->bChanged = TRUE;
                    }

                    if ((lpRState->TSState >= D3DTSS_TEXTUREMAP) &&
                        (lpRState->TSState <= D3DTSS_BUMPENVLOFFSET)) {
#if D3D_STATEBLOCKS
                        if (!pCtxt->bStateRecMode)
                        {
#endif //D3D_STATEBLOCKS
                            if (pCtxt->tssStates[lpRState->TSState] != lpRState->dwValue)
                            {
                                pCtxt->tssStates[lpRState->TSState] = lpRState->dwValue;
                                DPF_DBG("TSS State Chg , Stage %li, State %li, Value %li",
                                        (LONG)lpRState->wStage, 
                                        (LONG)lpRState->TSState, 
                                        (LONG)lpRState->dwValue);
                                pCtxt->bChanged = TRUE;
                            }
#if D3D_STATEBLOCKS
                        } 
                        else
                        {
                            if (pCtxt->pCurrSS != NULL)
                            {
                                DPF_DBG("Recording RS %x = %x",
                                         lpRState->TSState, lpRState->dwValue);

                                // Recording the state in a stateblock
                                pCtxt->pCurrSS->u.uc.TssStates[lpRState->TSState] =
                                                                    lpRState->dwValue;
                                FLAG_SET(pCtxt->pCurrSS->u.uc.bStoredTSS,
                                         lpRState->TSState);
                            }
                        }
#endif //D3D_STATEBLOCKS
                    } else
                    {
                        DPF("Unhandled texture stage state %li value %li",
                            (LONG)lpRState->TSState, (LONG)lpRState->dwValue);
                    }
                }
                else
                {
                    DPF("Texture Stage other than 0 received, not supported on Virge");
                }
                lpRState ++;
            }

            ddrval = __HWSetupPrimitive( pCtxt, &S3FVFOff);
            if( ddrval != DD_OK )
            {
                DPF("Error processing D3DDP2OP_TEXTURESTAGESTATE primitive states");
                PARSE_ERROR_AND_EXIT(lpdp2d, lpIns, lpInsStart, ddrval);
            }

            // Update triangle rendering function
            pTriangle = __HWSetTriangleFunc(pCtxt);

            NEXTINSTRUCTION(lpIns, D3DHAL_DP2TEXTURESTAGESTATE, 
                            lpIns->wStateCount, 0); 
            break;


        case D3DNTDP2OP_VIEWPORTINFO:
            // Skip data
            NEXTINSTRUCTION(lpIns, D3DHAL_DP2VIEWPORTINFO, 
                            lpIns->wStateCount, 0); 
            break;

        case D3DNTDP2OP_WINFO:
            // We dont implement a w-buffer in this driver so we just skip any of this
            // data that might be sent to us 
            NEXTINSTRUCTION(lpIns, D3DHAL_DP2WINFO, 
                            lpIns->wStateCount, 0); 
            break;

        case D3DDP2OP_POINTS:

            DPF_DBG("D3DDP2OP_POINTS");

            // Point primitives in vertex buffers are defined by the 
            // D3DHAL_DP2POINTS structure. The driver should render
            // wCount points starting at the initial vertex specified 
            // by wFirst. Then for each D3DHAL_DP2POINTS, the points
            // rendered will be (wFirst),(wFirst+1),...,
            // (wFirst+(wCount-1)). The number of D3DHAL_DP2POINTS
            // structures to process is specified by the wPrimitiveCount
            // field of D3DHAL_DP2COMMAND.

            for (i = lpIns->wPrimitiveCount; i > 0; i--)
            { 
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
                wIndex = ((D3DHAL_DP2POINTS*)lpPrim)->wVStart;
                wCount = ((D3DHAL_DP2POINTS*)lpPrim)->wCount;

                lpV0 = LP_FVF_VERTEX(lpVertices, wIndex, S3FVFOff);

                // Check first & last vertex
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex);
                CHECK_DATABUF_LIMITS(lpdp2d, (LONG)wIndex + wCount - 1);
                for (j = 0; j < wCount; j++)
                {
                    __HWRenderPoint(pCtxt, lpV0, &S3FVFOff);
                    lpV0 = LP_FVF_NXT_VTX(lpV0, S3FVFOff);
                }

                lpPrim += sizeof(D3DHAL_DP2POINTS);
            }

            NEXTINSTRUCTION(lpIns, D3DHAL_DP2POINTS, 
                                   lpIns->wPrimitiveCount, 0);
            break;

        case D3DDP2OP_LINELIST:

            DPF_DBG("D3DDP2OP_LINELIST");

            // Non-indexed vertex-buffer line lists are defined by the 
            // D3DHAL_DP2LINELIST structure. Given an initial vertex, 
            // the driver will render a sequence of independent lines, 
            // processing two new vertices with each line. The number 
            // of lines to render is specified by the wPrimitiveCount
            // field of D3DHAL_DP2COMMAND. The sequence of lines 
            // rendered will be 
            // (wVStart, wVStart+1),(wVStart+2, wVStart+3),...,
            // (wVStart+(wPrimitiveCount-1)*2), wVStart+wPrimitiveCount*2 - 1).

            CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
            wIndex = ((D3DHAL_DP2LINELIST*)lpPrim)->wVStart;

            lpV0 = LP_FVF_VERTEX(lpVertices, wIndex, S3FVFOff);
            lpV1 = LP_FVF_NXT_VTX(lpV0, S3FVFOff);

            // Check first & last vertex
            CHECK_DATABUF_LIMITS(lpdp2d, wIndex);
            CHECK_DATABUF_LIMITS(lpdp2d, (LONG)wIndex + 2*lpIns->wPrimitiveCount - 1);
            for (i = lpIns->wPrimitiveCount; i > 0; i--)
            {
                __HWRenderLine(pCtxt, lpV0, lpV1, lpV0, &S3FVFOff);

                lpV0 = LP_FVF_NXT_VTX(lpV1, S3FVFOff);
                lpV1 = LP_FVF_NXT_VTX(lpV0, S3FVFOff);
            }

            NEXTINSTRUCTION(lpIns, D3DHAL_DP2LINELIST, 1, 0);
            break;

        case D3DDP2OP_INDEXEDLINELIST:

            DPF_DBG("D3DDP2OP_INDEXEDLINELIST");

            // The D3DHAL_DP2INDEXEDLINELIST structure specifies 
            // unconnected lines to render using vertex indices.
            // The line endpoints for each line are specified by wV1 
            // and wV2. The number of lines to render using this 
            // structure is specified by the wPrimitiveCount field of
            // D3DHAL_DP2COMMAND.  The sequence of lines 
            // rendered will be (wV[0], wV[1]), (wV[2], wV[3]),...
            // (wVStart[(wPrimitiveCount-1)*2], wVStart[wPrimitiveCount*2-1]).

            for (i = lpIns->wPrimitiveCount; i > 0; i--)
            { 
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
                wIndex1 = ((D3DHAL_DP2INDEXEDLINELIST*)lpPrim)->wV1;
                wIndex2 = ((D3DHAL_DP2INDEXEDLINELIST*)lpPrim)->wV2;

                lpV1 = LP_FVF_VERTEX(lpVertices, wIndex1, S3FVFOff);
                lpV2 = LP_FVF_VERTEX(lpVertices, wIndex2, S3FVFOff);

                // Must check each new vertex
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex1);
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex2);
                __HWRenderLine(pCtxt, lpV1, lpV2, lpV1, &S3FVFOff);

                lpPrim += sizeof(D3DHAL_DP2INDEXEDLINELIST);
            }

            NEXTINSTRUCTION(lpIns, D3DHAL_DP2INDEXEDLINELIST, 
                                   lpIns->wPrimitiveCount, 0);
            break;

        case D3DDP2OP_INDEXEDLINELIST2:

            DPF_DBG("D3DDP2OP_INDEXEDLINELIST2");

            // The D3DHAL_DP2INDEXEDLINELIST structure specifies 
            // unconnected lines to render using vertex indices.
            // The line endpoints for each line are specified by wV1 
            // and wV2. The number of lines to render using this 
            // structure is specified by the wPrimitiveCount field of
            // D3DHAL_DP2COMMAND.  The sequence of lines 
            // rendered will be (wV[0], wV[1]), (wV[2], wV[3]),
            // (wVStart[(wPrimitiveCount-1)*2], wVStart[wPrimitiveCount*2-1]).
            // The indexes are relative to a base index value that 
            // immediately follows the command

            CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
            wIndxBase = ((D3DHAL_DP2STARTVERTEX*)lpPrim)->wVStart;
            lpPrim = lpPrim + sizeof(D3DHAL_DP2STARTVERTEX);

            for (i = lpIns->wPrimitiveCount; i > 0; i--)
            { 
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
                wIndex1 = ((D3DHAL_DP2INDEXEDLINELIST*)lpPrim)->wV1;
                wIndex2 = ((D3DHAL_DP2INDEXEDLINELIST*)lpPrim)->wV2;

                lpV1 = LP_FVF_VERTEX(lpVertices, (wIndex1+wIndxBase), S3FVFOff);
                lpV2 = LP_FVF_VERTEX(lpVertices, (wIndex2+wIndxBase), S3FVFOff);

                // Must check each new vertex
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex1 + wIndxBase);
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex2 + wIndxBase);
                __HWRenderLine(pCtxt, lpV1, lpV2, lpV1, &S3FVFOff);

                lpPrim += sizeof(D3DHAL_DP2INDEXEDLINELIST);
            }

            NEXTINSTRUCTION(lpIns, D3DHAL_DP2INDEXEDLINELIST, 
                                   lpIns->wPrimitiveCount, STARTVERTEXSIZE);
            break;

        case D3DDP2OP_LINESTRIP:

            DPF_DBG("D3DDP2OP_LINESTRIP");

            // Non-index line strips rendered with vertex buffers are
            // specified using D3DHAL_DP2LINESTRIP. The first vertex 
            // in the line strip is specified by wVStart. The 
            // number of lines to process is specified by the 
            // wPrimitiveCount field of D3DHAL_DP2COMMAND. The sequence
            // of lines rendered will be (wVStart, wVStart+1),
            // (wVStart+1, wVStart+2),(wVStart+2, wVStart+3),...,
            // (wVStart+wPrimitiveCount, wVStart+wPrimitiveCount+1).

            CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
            wIndex = ((D3DHAL_DP2LINESTRIP*)lpPrim)->wVStart;

            lpV0 = LP_FVF_VERTEX(lpVertices, wIndex, S3FVFOff);
            lpV1 = LP_FVF_NXT_VTX(lpV0, S3FVFOff);

            // Check first & last vertex
            CHECK_DATABUF_LIMITS(lpdp2d, wIndex);
            CHECK_DATABUF_LIMITS(lpdp2d, wIndex + lpIns->wPrimitiveCount);

            for (i = lpIns->wPrimitiveCount; i > 0; i--)
            {
                __HWRenderLine(pCtxt, lpV0, lpV1, lpV0, &S3FVFOff);

                lpV0 = lpV1;
                lpV1 = LP_FVF_NXT_VTX(lpV1, S3FVFOff);
            }

            NEXTINSTRUCTION(lpIns, D3DHAL_DP2LINESTRIP, 1, 0);
            break;

        case D3DDP2OP_INDEXEDLINESTRIP:

            DPF_DBG("D3DDP2OP_INDEXEDLINESTRIP");

            // Indexed line strips rendered with vertex buffers are 
            // specified using D3DHAL_DP2INDEXEDLINESTRIP. The number
            // of lines to process is specified by the wPrimitiveCount
            // field of D3DHAL_DP2COMMAND. The sequence of lines 
            // rendered will be (wV[0], wV[1]), (wV[1], wV[2]),
            // (wV[2], wV[3]), ...
            // (wVStart[wPrimitiveCount-1], wVStart[wPrimitiveCount]). 
            // Although the D3DHAL_DP2INDEXEDLINESTRIP structure only
            // has enough space allocated for a single line, the wV 
            // array of indices should be treated as a variable-sized 
            // array with wPrimitiveCount+1 elements.
            // The indexes are relative to a base index value that 
            // immediately follows the command

            CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
            wIndxBase = ((D3DHAL_DP2STARTVERTEX*)lpPrim)->wVStart;
            lpPrim = lpPrim + sizeof(D3DHAL_DP2STARTVERTEX);

            // guard defensively against pathological commands
            if ( lpIns->wPrimitiveCount > 0 ) {
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
                wIndex1 = ((D3DHAL_DP2INDEXEDLINESTRIP*)lpPrim)->wV[0];
                wIndex2 = ((D3DHAL_DP2INDEXEDLINESTRIP*)lpPrim)->wV[1];
                lpV1 = 
                lpV2 = LP_FVF_VERTEX(lpVertices, wIndex1+wIndxBase, S3FVFOff);

                //We need to check each vertex separately
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex1 + wIndxBase);
            }

            for (i = 0; i < lpIns->wPrimitiveCount; i++)
            { 
                lpV1 = lpV2;
                lpV2 = LP_FVF_VERTEX(lpVertices, wIndex2 + wIndxBase, S3FVFOff);

                CHECK_DATABUF_LIMITS(lpdp2d, wIndex2 + wIndxBase);
                __HWRenderLine(pCtxt, lpV1, lpV2, lpV1, &S3FVFOff);

                if( i % 2 ) {
                    wIndex2 = ((D3DHAL_DP2INDEXEDLINESTRIP*)lpPrim)->wV[1];
                } else if ((i+1) < lpIns->wPrimitiveCount ) {
                    lpPrim += sizeof(D3DHAL_DP2INDEXEDLINESTRIP);
                    CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
                    wIndex2 = ((D3DHAL_DP2INDEXEDLINESTRIP*)lpPrim)->wV[0];
                }
            }

            // Point to next D3DHAL_DP2COMMAND in the command buffer
            // Advance only as many vertex indices there are, with no padding!
            NEXTINSTRUCTION(lpIns, WORD, 
                            lpIns->wPrimitiveCount + 1, STARTVERTEXSIZE);
            break;

        case D3DDP2OP_TRIANGLELIST:

            DPF_DBG("D3DDP2OP_TRIANGLELIST");

            // Non-indexed vertex buffer triangle lists are defined by 
            // the D3DHAL_DP2TRIANGLELIST structure. Given an initial
            // vertex, the driver will render independent triangles, 
            // processing three new vertices with each triangle. The
            // number of triangles to render is specified by the 
            // wPrimitveCount field of D3DHAL_DP2COMMAND. The sequence
            // of vertices processed will be  (wVStart, wVStart+1, 
            // vVStart+2), (wVStart+3, wVStart+4, vVStart+5),...,
            // (wVStart+(wPrimitiveCount-1)*3), wVStart+wPrimitiveCount*3-2, 
            // vStart+wPrimitiveCount*3-1).


            CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
            wIndex = ((D3DHAL_DP2TRIANGLELIST*)lpPrim)->wVStart;

            lpV0 = LP_FVF_VERTEX(lpVertices, wIndex, S3FVFOff);
            lpV1 = LP_FVF_NXT_VTX(lpV0, S3FVFOff);
            lpV2 = LP_FVF_NXT_VTX(lpV1, S3FVFOff);

            // Check first & last vertex
            CHECK_DATABUF_LIMITS(lpdp2d, wIndex);
            CHECK_DATABUF_LIMITS(lpdp2d, (LONG)wIndex + 3*lpIns->wPrimitiveCount - 1);
            for (i = lpIns->wPrimitiveCount; i > 0; i--)
            {
                if (!CULL_TRI(pCtxt,lpV0,lpV1,lpV2))
                    (*pTriangle)(pCtxt, lpV0, lpV1, lpV2, &S3FVFOff);

                lpV0 = LP_FVF_NXT_VTX(lpV2, S3FVFOff);
                lpV1 = LP_FVF_NXT_VTX(lpV0, S3FVFOff);
                lpV2 = LP_FVF_NXT_VTX(lpV1, S3FVFOff);
            }

            NEXTINSTRUCTION(lpIns, D3DHAL_DP2TRIANGLELIST, 1, 0);
            break;

        case D3DDP2OP_INDEXEDTRIANGLELIST:

            DPF_DBG("D3DDP2OP_INDEXEDTRIANGLELIST");

            // The D3DHAL_DP2INDEXEDTRIANGLELIST structure specifies 
            // unconnected triangles to render with a vertex buffer.
            // The vertex indices are specified by wV1, wV2 and wV3. 
            // The wFlags field allows specifying edge flags identical 
            // to those specified by D3DOP_TRIANGLE. The number of 
            // triangles to render (that is, number of 
            // D3DHAL_DP2INDEXEDTRIANGLELIST structures to process) 
            // is specified by the wPrimitiveCount field of 
            // D3DHAL_DP2COMMAND.

            // This is the only indexed primitive where we don't get 
            // an offset into the vertex buffer in order to maintain
            // DX3 compatibility. A new primitive 
            // (D3DDP2OP_INDEXEDTRIANGLELIST2) has been added to handle
            // the corresponding DX6 primitive.

            for (i = lpIns->wPrimitiveCount; i > 0; i--)
            { 
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
                wIndex1 = ((D3DHAL_DP2INDEXEDTRIANGLELIST*)lpPrim)->wV1;
                wIndex2 = ((D3DHAL_DP2INDEXEDTRIANGLELIST*)lpPrim)->wV2;
                wIndex3 = ((D3DHAL_DP2INDEXEDTRIANGLELIST*)lpPrim)->wV3;
                wFlags  = ((D3DHAL_DP2INDEXEDTRIANGLELIST*)lpPrim)->wFlags;


                lpV1 = LP_FVF_VERTEX(lpVertices, wIndex1, S3FVFOff);
                lpV2 = LP_FVF_VERTEX(lpVertices, wIndex2, S3FVFOff);
                lpV3 = LP_FVF_VERTEX(lpVertices, wIndex3, S3FVFOff);

                // Must check each new vertex
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex1);
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex2);
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex3);
                if (!CULL_TRI(pCtxt,lpV1,lpV2,lpV3)) {

                    if (pCtxt->FillMode == D3DFILL_POINT) {
                        __HWRenderPoint( pCtxt, lpV1, &S3FVFOff);
                        __HWRenderPoint( pCtxt, lpV2, &S3FVFOff);
                        __HWRenderPoint( pCtxt, lpV3, &S3FVFOff);
                    } else if (pCtxt->FillMode == D3DFILL_WIREFRAME) {
                        if( wFlags & D3DTRIFLAG_EDGEENABLE1 )
                            __HWRenderLine( pCtxt, lpV1, lpV2, lpV1, &S3FVFOff);
                        if( wFlags & D3DTRIFLAG_EDGEENABLE2 )
                            __HWRenderLine( pCtxt, lpV2, lpV3, lpV1, &S3FVFOff);
                        if( wFlags & D3DTRIFLAG_EDGEENABLE3 )
                            __HWRenderLine( pCtxt, lpV3, lpV1, lpV1, &S3FVFOff);
                    } else
                        (*pTriangle)(pCtxt, lpV1, lpV2, lpV3, &S3FVFOff);
                }

                lpPrim += sizeof(D3DHAL_DP2INDEXEDTRIANGLELIST);
            }

            NEXTINSTRUCTION(lpIns, D3DHAL_DP2INDEXEDTRIANGLELIST, 
                                   lpIns->wPrimitiveCount, 0);
            break;

        case D3DDP2OP_INDEXEDTRIANGLELIST2:

            DPF_DBG("D3DDP2OP_INDEXEDTRIANGLELIST2 ");

            // The D3DHAL_DP2INDEXEDTRIANGLELIST2 structure specifies 
            // unconnected triangles to render with a vertex buffer.
            // The vertex indices are specified by wV1, wV2 and wV3. 
            // The wFlags field allows specifying edge flags identical 
            // to those specified by D3DOP_TRIANGLE. The number of 
            // triangles to render (that is, number of 
            // D3DHAL_DP2INDEXEDTRIANGLELIST structures to process) 
            // is specified by the wPrimitiveCount field of 
            // D3DHAL_DP2COMMAND.
            // The indexes are relative to a base index value that 
            // immediately follows the command

            CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
            wIndxBase = ((D3DHAL_DP2STARTVERTEX*)lpPrim)->wVStart;
            lpPrim = lpPrim + sizeof(D3DHAL_DP2STARTVERTEX);

            for (i = lpIns->wPrimitiveCount; i > 0; i--)
            { 
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
                wIndex1 = ((D3DHAL_DP2INDEXEDTRIANGLELIST2*)lpPrim)->wV1;
                wIndex2 = ((D3DHAL_DP2INDEXEDTRIANGLELIST2*)lpPrim)->wV2;
                wIndex3 = ((D3DHAL_DP2INDEXEDTRIANGLELIST2*)lpPrim)->wV3;

                lpV1 = LP_FVF_VERTEX(lpVertices, wIndex1+wIndxBase, S3FVFOff);
                lpV2 = LP_FVF_VERTEX(lpVertices, wIndex2+wIndxBase, S3FVFOff);
                lpV3 = LP_FVF_VERTEX(lpVertices, wIndex3+wIndxBase, S3FVFOff);

                // Must check each new vertex
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex1+wIndxBase);
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex2+wIndxBase);
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex3+wIndxBase);
                if (!CULL_TRI(pCtxt,lpV1,lpV2,lpV3)) {

                    if (pCtxt->FillMode == D3DFILL_POINT) {
                        __HWRenderPoint( pCtxt, lpV1, &S3FVFOff);
                        __HWRenderPoint( pCtxt, lpV2, &S3FVFOff);
                        __HWRenderPoint( pCtxt, lpV3, &S3FVFOff);
                    } else if (pCtxt->FillMode == D3DFILL_WIREFRAME) {
                            __HWRenderLine( pCtxt, lpV1, lpV2, lpV1, &S3FVFOff);
                            __HWRenderLine( pCtxt, lpV2, lpV3, lpV1, &S3FVFOff);
                            __HWRenderLine( pCtxt, lpV3, lpV1, lpV1, &S3FVFOff);
                    } else
                        (*pTriangle)(pCtxt, lpV1, lpV2, lpV3, &S3FVFOff);
                }

                lpPrim += sizeof(D3DHAL_DP2INDEXEDTRIANGLELIST2);
            }

            NEXTINSTRUCTION(lpIns, D3DHAL_DP2INDEXEDTRIANGLELIST2, 
                                   lpIns->wPrimitiveCount, STARTVERTEXSIZE);
            break;

        case D3DDP2OP_TRIANGLESTRIP:

            DPF_DBG("D3DDP2OP_TRIANGLESTRIP");

            // Non-index triangle strips rendered with vertex buffers 
            // are specified using D3DHAL_DP2TRIANGLESTRIP. The first 
            // vertex in the triangle strip is specified by wVStart. 
            // The number of triangles to process is specified by the 
            // wPrimitiveCount field of D3DHAL_DP2COMMAND. The sequence
            // of triangles rendered for the odd-triangles case will 
            // be (wVStart, wVStart+1, vVStart+2), (wVStart+1, 
            // wVStart+3, vVStart+2),.(wVStart+2, wVStart+3, 
            // vVStart+4),.., (wVStart+wPrimitiveCount-1), 
            // wVStart+wPrimitiveCount, vStart+wPrimitiveCount+1). For an
            // even number of , the last triangle will be .,
            // (wVStart+wPrimitiveCount-1, wVStart+wPrimitiveCount+1, 
            // vStart+wPrimitiveCount).

            // guard defensively against pathological commands
            if ( lpIns->wPrimitiveCount > 0 ) {
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
                wIndex = ((D3DHAL_DP2TRIANGLESTRIP*)lpPrim)->wVStart;
                lpV2 = LP_FVF_VERTEX(lpVertices, wIndex, S3FVFOff);
                lpV1 = LP_FVF_NXT_VTX(lpV2, S3FVFOff);

                // Check first and last vertex
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex);
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex + lpIns->wPrimitiveCount + 1);
            }

            for (i = 0; i < lpIns->wPrimitiveCount; i++)
            { 
                if( i % 2 ) 
                {
                    lpV0 = lpV1;
                    lpV1 = LP_FVF_NXT_VTX(lpV2, S3FVFOff);
                }
                else 
                {
                    lpV0 = lpV2;
                    lpV2 = LP_FVF_NXT_VTX(lpV1, S3FVFOff);
                }

                if (!CULL_TRI(pCtxt,lpV0,lpV1,lpV2))
                    (*pTriangle)(pCtxt, lpV0, lpV1, lpV2, &S3FVFOff);
            }

            // Point to next D3DHAL_DP2COMMAND in the command buffer
            NEXTINSTRUCTION(lpIns, D3DHAL_DP2TRIANGLESTRIP, 1, 0);
            break;

        case D3DDP2OP_INDEXEDTRIANGLESTRIP:

            DPF_DBG("D3DDP2OP_INDEXEDTRIANGLESTRIP");

            // Indexed triangle strips rendered with vertex buffers are 
            // specified using D3DHAL_DP2INDEXEDTRIANGLESTRIP. The number
            // of triangles to process is specified by the wPrimitiveCount
            // field of D3DHAL_DP2COMMAND. The sequence of triangles 
            // rendered for the odd-triangles case will be 
            // (wV[0],wV[1],wV[2]),(wV[1],wV[3],wV[2]),
            // (wV[2],wV[3],wV[4]),...,(wV[wPrimitiveCount-1],
            // wV[wPrimitiveCount],wV[wPrimitiveCount+1]). For an even
            // number of triangles, the last triangle will be
            // (wV[wPrimitiveCount-1],wV[wPrimitiveCount+1],
            // wV[wPrimitiveCount]).Although the 
            // D3DHAL_DP2INDEXEDTRIANGLESTRIP structure only has 
            // enough space allocated for a single line, the wV 
            // array of indices should be treated as a variable-sized 
            // array with wPrimitiveCount+2 elements.
            // The indexes are relative to a base index value that 
            // immediately follows the command

            CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
            wIndxBase = ((D3DHAL_DP2STARTVERTEX*)lpPrim)->wVStart;
            lpPrim = lpPrim + sizeof(D3DHAL_DP2STARTVERTEX);

            // guard defensively against pathological commands
            if ( lpIns->wPrimitiveCount > 0 ) {
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
                wIndex  = ((D3DHAL_DP2INDEXEDTRIANGLESTRIP*)lpPrim)->wV[0];
                wIndex1 = ((D3DHAL_DP2INDEXEDTRIANGLESTRIP*)lpPrim)->wV[1];

                // We need to check each vertex
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex + wIndxBase);
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex1 + wIndxBase);

                lpV2 = LP_FVF_VERTEX(lpVertices, wIndex + wIndxBase, S3FVFOff);
                lpV1 = LP_FVF_VERTEX(lpVertices, wIndex1 + wIndxBase, S3FVFOff);
            }

            for (i = 0; i < lpIns->wPrimitiveCount; i++)
            { 
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
                wIndex2 = ((D3DHAL_DP2INDEXEDTRIANGLESTRIP*)lpPrim)->wV[2];

                // We need to check each new vertex
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex2+wIndxBase);


                if( i % 2 ) 
                {
                    lpV0 = lpV1;
                    lpV1 = LP_FVF_VERTEX(lpVertices, wIndex2+wIndxBase, S3FVFOff);
                }
                else 
                {
                    lpV0 = lpV2;
                    lpV2 = LP_FVF_VERTEX(lpVertices, wIndex2+wIndxBase, S3FVFOff);
                }


                if (!CULL_TRI(pCtxt,lpV0,lpV1,lpV2))
                    (*pTriangle)(pCtxt, lpV0, lpV1, lpV2, &S3FVFOff);

                // We will advance our pointer only one WORD in order 
                // to fetch the next index
                lpPrim += sizeof(WORD);
            }

            // Point to next D3DHAL_DP2COMMAND in the command buffer
            NEXTINSTRUCTION(lpIns, WORD , 
                            lpIns->wPrimitiveCount + 2, STARTVERTEXSIZE);
            break;

        case D3DDP2OP_TRIANGLEFAN:

            DPF_DBG("D3DDP2OP_TRIANGLEFAN");

            // The D3DHAL_DP2TRIANGLEFAN structure is used to draw 
            // non-indexed triangle fans. The sequence of triangles
            // rendered will be (wVStart, wVstart+1, wVStart+2),
            // (wVStart,wVStart+2,wVStart+3), (wVStart,wVStart+3,
            // wVStart+4),...,(wVStart,wVStart+wPrimitiveCount,
            // wVStart+wPrimitiveCount+1).

            CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
            wIndex = ((D3DHAL_DP2TRIANGLEFAN*)lpPrim)->wVStart;

            lpV0 = LP_FVF_VERTEX(lpVertices, wIndex, S3FVFOff);
            lpV1 = LP_FVF_NXT_VTX(lpV0, S3FVFOff);
            lpV2 = LP_FVF_NXT_VTX(lpV1, S3FVFOff);

            // Check first & last vertex
            CHECK_DATABUF_LIMITS(lpdp2d, wIndex);
            CHECK_DATABUF_LIMITS(lpdp2d, wIndex + lpIns->wPrimitiveCount + 1);

            for (i = 0; i < lpIns->wPrimitiveCount; i++)
            {
                if (!CULL_TRI(pCtxt,lpV0,lpV1,lpV2))
                    (*pTriangle)(pCtxt, lpV1, lpV2, lpV0, &S3FVFOff);

                lpV1 = lpV2;
                lpV2 = LP_FVF_NXT_VTX(lpV2, S3FVFOff);
            }

            NEXTINSTRUCTION(lpIns, D3DHAL_DP2TRIANGLEFAN, 1, 0);
            break;

        case D3DDP2OP_INDEXEDTRIANGLEFAN:

            DPF_DBG("D3DDP2OP_INDEXEDTRIANGLEFAN");

            // The D3DHAL_DP2INDEXEDTRIANGLEFAN structure is used to 
            // draw indexed triangle fans. The sequence of triangles
            // rendered will be (wV[0], wV[1], wV[2]), (wV[0], wV[2],
            // wV[3]), (wV[0], wV[3], wV[4]),...,(wV[0],
            // wV[wPrimitiveCount], wV[wPrimitiveCount+1]).
            // The indexes are relative to a base index value that 
            // immediately follows the command

            CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
            wIndxBase = ((D3DHAL_DP2STARTVERTEX*)lpPrim)->wVStart;
            lpPrim = lpPrim + sizeof(D3DHAL_DP2STARTVERTEX);

            // guard defensively against pathological commands
            if ( lpIns->wPrimitiveCount > 0 ) {
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
                wIndex  = ((D3DHAL_DP2INDEXEDTRIANGLEFAN*)lpPrim)->wV[0];
                wIndex1 = ((D3DHAL_DP2INDEXEDTRIANGLEFAN*)lpPrim)->wV[1];
                lpV0 = LP_FVF_VERTEX(lpVertices, wIndex + wIndxBase, S3FVFOff);
                lpV1 = lpV2 = LP_FVF_VERTEX(lpVertices, wIndex1 + wIndxBase, S3FVFOff);

                // We need to check each vertex
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex + wIndxBase);
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex1 + wIndxBase);
            }

            for (i = 0; i < lpIns->wPrimitiveCount; i++)
            { 
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
                wIndex2 = ((D3DHAL_DP2INDEXEDTRIANGLEFAN*)lpPrim)->wV[2];
                lpV1 = lpV2;
                lpV2 = LP_FVF_VERTEX(lpVertices, wIndex2 + wIndxBase, S3FVFOff);

                // We need to check each vertex
                CHECK_DATABUF_LIMITS(lpdp2d, wIndex2 + wIndxBase);

                if (!CULL_TRI(pCtxt,lpV0,lpV1,lpV2))
                    (*pTriangle)(pCtxt, lpV1, lpV2, lpV0, &S3FVFOff);

                // We will advance our pointer only one WORD in order 
                // to fetch the next index
                lpPrim += sizeof(WORD);
            }

            // Point to next D3DHAL_DP2COMMAND in the command buffer
            NEXTINSTRUCTION(lpIns, WORD , 
                            lpIns->wPrimitiveCount + 2, STARTVERTEXSIZE);
            break;

        case D3DDP2OP_LINELIST_IMM:

            DPF_DBG("D3DDP2OP_LINELIST_IMM");

            // Draw a set of lines specified by pairs of vertices 
            // that immediately follow this instruction in the
            // command stream. The wPrimitiveCount member of the
            // D3DHAL_DP2COMMAND structure specifies the number
            // of lines that follow. The type and size of the
            // vertices are determined by the dwVertexType member
            // of the D3DHAL_DRAWPRIMITIVES2DATA structure.

            // Primitives in an IMM instruction are stored in the
            // command buffer and are DWORD aligned
            lpPrim = (LPBYTE)((ULONG_PTR)(lpPrim + 3 ) & ~3 );

            // Get vertex pointers
            lpV0 = (LPD3DTLVERTEX)lpPrim;
            lpV1 = LP_FVF_NXT_VTX(lpV0, S3FVFOff);
            CHECK_CMDBUF_LIMITS(lpdp2d, lpV0);

            for (i = 0; i < lpIns->wPrimitiveCount; i++)
            {
                // these vertices are in the command buffer!
                CHECK_CMDBUF_LIMITS(lpdp2d, lpV1);

                __HWRenderLine(pCtxt, lpV0, lpV1, lpV0, &S3FVFOff);

                lpV0 = lpV1;
                lpV1 = LP_FVF_NXT_VTX(lpV1, S3FVFOff);
            }

            // Realign next command since vertices are dword aligned
            // and store # of primitives before affecting the pointer
            wCount = lpIns->wPrimitiveCount;
            lpIns  = (LPD3DHAL_DP2COMMAND)(( ((ULONG_PTR)lpIns) + 3 ) & ~ 3);

            NEXTINSTRUCTION_S(lpIns, S3FVFOff.dwStride, wCount + 1, 0);

            break;

        case D3DDP2OP_TRIANGLEFAN_IMM:

            DPF_DBG("D3DDP2OP_TRIANGLEFAN_IMM");

            // Draw a triangle fan specified by pairs of vertices 
            // that immediately follow this instruction in the
            // command stream. The wPrimitiveCount member of the
            // D3DHAL_DP2COMMAND structure specifies the number
            // of triangles that follow. The type and size of the
            // vertices are determined by the dwVertexType member
            // of the D3DHAL_DRAWPRIMITIVES2DATA structure.

            // Get Edge flags
            dwEdgeFlags = ((D3DHAL_DP2TRIANGLEFAN_IMM *)lpPrim)->dwEdgeFlags;
            lpPrim = (LPBYTE)lpPrim + sizeof(D3DHAL_DP2TRIANGLEFAN_IMM); 

            // Vertices in an IMM instruction are stored in the
            // command buffer and are DWORD aligned
            lpPrim = (LPBYTE)((ULONG_PTR)(lpPrim + 3 ) & ~3 );

            // Get vertex pointers
            lpV0 = (LPD3DTLVERTEX)lpPrim;
            lpV1 = LP_FVF_NXT_VTX(lpV0, S3FVFOff);
            lpV2 = LP_FVF_NXT_VTX(lpV1, S3FVFOff);

            // these vertices are in the command buffer!
            CHECK_CMDBUF_LIMITS(lpdp2d, lpV0); 
            CHECK_CMDBUF_LIMITS(lpdp2d, lpV1);

            for (i = 0 ; i < lpIns->wPrimitiveCount ; i++)
            {
                // these vertices are in the command buffer!
                CHECK_CMDBUF_LIMITS(lpdp2d, lpV2);

                if (!CULL_TRI(pCtxt,lpV0,lpV1,lpV2)) {
                    if (pCtxt->FillMode == D3DFILL_POINT) {
                        if (0 == i) {
                            __HWRenderPoint( pCtxt, lpV0, &S3FVFOff);
                            __HWRenderPoint( pCtxt, lpV1, &S3FVFOff);
                        }
                        __HWRenderPoint( pCtxt, lpV2, &S3FVFOff);
                    } else if (pCtxt->FillMode == D3DFILL_WIREFRAME) {
                        // dwEdgeFlags is a bit sequence representing the edge 
                        // flag for each one of the outer edges of the 
                        // triangle fan
                        if (0 == i) {
                            if (dwEdgeFlags & 0x0001)
                                __HWRenderLine( pCtxt, lpV0, lpV1, lpV0, &S3FVFOff);
                            dwEdgeFlags >>= 1;
                        }

                        if (dwEdgeFlags & 0x0001)
                            __HWRenderLine( pCtxt, lpV1, lpV2, lpV0, &S3FVFOff);
                        dwEdgeFlags >>= 1;

                        if (i == (UINT)lpIns->wPrimitiveCount - 1)
                        {
                            // last triangle fan edge
                            if (dwEdgeFlags & 0x0001)
                                __HWRenderLine( pCtxt, lpV2, lpV0, lpV0, &S3FVFOff);
                        }
                    }
                    else
                        (*pTriangle)(pCtxt, lpV1, lpV2, lpV0, &S3FVFOff);
                }


                lpV1 = lpV2;
                lpV2 = LP_FVF_NXT_VTX(lpV2, S3FVFOff);
            }

            // Realign next command since vertices are dword aligned
            // and store # of primitives before affecting the pointer
            wCount = lpIns->wPrimitiveCount;
            lpIns  = (LPD3DHAL_DP2COMMAND)(( ((ULONG_PTR)lpIns) + 3 ) & ~ 3);

            NEXTINSTRUCTION_S(lpIns, S3FVFOff.dwStride, 
                              wCount + 2, sizeof(D3DHAL_DP2TRIANGLEFAN_IMM));


            break;                                     

        case D3DDP2OP_TEXBLT:
            // Inform the drivers to perform a BitBlt operation from a source
            // texture to a destination texture. A texture can also be cubic
            // environment map. The driver should copy a rectangle specified
            // by rSrc in the source texture to the location specified by pDest
            // in the destination texture. The destination and source textures
            // are identified by handles that the driver was notified with
            // during texture creation time. If the driver is capable of
            // managing textures, then it is possible that the destination
            // handle is 0. This indicates to the driver that it should preload
            // the texture into video memory (or wherever the hardware
            // efficiently textures from). In this case, it can ignore rSrc and
            // pDest. Note that for mipmapped textures, only one D3DDP2OP_TEXBLT
            // instruction is inserted into the D3dDrawPrimitives2 command stream.
            // In this case, the driver is expected to BitBlt all the mipmap
            // levels present in the texture.

            DPF_DBG("D3DDP2OP_TEXBLT");

            for ( i = 0; i < lpIns->wStateCount; i++)
            {
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);
                __TextureBlt(pCtxt, (D3DHAL_DP2TEXBLT*)(lpPrim));
                lpPrim += sizeof(D3DHAL_DP2TEXBLT);
            }

            NEXTINSTRUCTION(lpIns, D3DHAL_DP2TEXBLT, lpIns->wStateCount, 0);
            break;

        case D3DDP2OP_STATESET:
            {
                P2D3DHAL_DP2STATESET *pStateSetOp = (P2D3DHAL_DP2STATESET*)(lpPrim);
                DPF_DBG("D3DDP2OP_STATESET");
#if D3D_STATEBLOCKS
                for (i = 0; i < lpIns->wStateCount; i++, pStateSetOp++)
                {
                    switch (pStateSetOp->dwOperation)
                    {
                    case D3DHAL_STATESETBEGIN  :
                        __BeginStateSet(pCtxt,pStateSetOp->dwParam);
                        break;
                    case D3DHAL_STATESETEND    :
                        __EndStateSet(pCtxt);
                        break;
                    case D3DHAL_STATESETDELETE :
                        __DeleteStateSet(pCtxt,pStateSetOp->dwParam);
                        break;
                    case D3DHAL_STATESETEXECUTE:
                        __ExecuteStateSet(pCtxt,pStateSetOp->dwParam);
                        // Update state to pickup new render/depth surfaces
                        pCtxt->bChanged = TRUE;
                        ddrval = __HWSetupPrimitive( pCtxt, &S3FVFOff);
                        if( ddrval != DD_OK )
                        {
                            DPF("Error processing D3DDP2OP_SETRENDERTARGET primitive states");
                            PARSE_ERROR_AND_EXIT(lpdp2d, lpIns, lpInsStart, ddrval);
                        }
                        // Select the appropriate triangle rendering function 
                        // according to the currnt rendering context
                        pTriangle = __HWSetTriangleFunc(pCtxt); 
                        break;
                    case D3DHAL_STATESETCAPTURE:
                        __CaptureStateSet(pCtxt,pStateSetOp->dwParam);
                        break;
                    default :
                        DPF_DBG("D3DDP2OP_STATESET has invalid"
                            "dwOperation %08lx",pStateSetOp->dwOperation);
                    }
                }
#endif //D3D_STATEBLOCKS
                // Update the command buffer pointer
                NEXTINSTRUCTION(lpIns, P2D3DHAL_DP2STATESET, 
                                lpIns->wStateCount, 0);
            }
            break;

        case D3DDP2OP_SETPALETTE:
            // Attach a palette to a texture, that is , map an association
            // between a palette handle and a surface handle, and specify
            // the characteristics of the palette. The number of
            // D3DNTHAL_DP2SETPALETTE structures to follow is specified by
            // the wStateCount member of the D3DNTHAL_DP2COMMAND structure
            // The S3Virge does not offer paletted textures.

            {
                D3DHAL_DP2SETPALETTE* lpSetPal =
                                            (D3DHAL_DP2SETPALETTE*)(lpPrim);

                DPF_DBG("D3DDP2OP_SETPALETTE");

                // Verify the command buffer validity
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);

                // Skip any palette updates since we don't 
                //support paletted textures on the S3Virge
                NEXTINSTRUCTION(lpIns, D3DHAL_DP2SETPALETTE,
                                lpIns->wStateCount, 0);
            }
            break;

        case D3DDP2OP_UPDATEPALETTE:
            // Perform modifications to the palette that is used for palettized
            // textures. The palette handle attached to a surface is updated
            // with wNumEntries PALETTEENTRYs starting at a specific wStartIndex
            // member of the palette. (A PALETTENTRY (defined in wingdi.h and
            // wtypes.h) is actually a DWORD with an ARGB color for each byte.) 
            // After the D3DNTHAL_DP2UPDATEPALETTE structure in the command
            // stream the actual palette data will follow (without any padding),
            // comprising one DWORD per palette entry. There will only be one
            // D3DNTHAL_DP2UPDATEPALETTE structure (plus palette data) following
            // the D3DNTHAL_DP2COMMAND structure regardless of the value of
            // wStateCount.
            // The S3Virge does not offer paletted textures.

            {
                D3DHAL_DP2UPDATEPALETTE* lpUpdatePal =
                                          (D3DHAL_DP2UPDATEPALETTE*)(lpPrim);

                DPF_DBG("D3DDP2OP_UPDATEPALETTE");

                // Verify the command buffer validity
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);

                // We will ALWAYS have only 1 palette update structure + palette
                // following the D3DDP2OP_UPDATEPALETTE token
                ASSERTDD(1 == lpIns->wStateCount,
                         "1 != wStateCount in D3DDP2OP_UPDATEPALETTE");

                // Skip any palette updates since we don't 
                //support paletted textures on the S3Virge
                NEXTINSTRUCTION(lpIns, D3DHAL_DP2UPDATEPALETTE, 
                                1,
                                (DWORD)lpUpdatePal->wNumEntries * 
                                     sizeof(PALETTEENTRY));
            }
            break;

        case D3DDP2OP_SETRENDERTARGET:
            // Map a new rendering target surface and depth buffer in
            // the current context.  This replaces the old D3dSetRenderTarget
            // callback. 

            {
                D3DHAL_DP2SETRENDERTARGET* pSRTData;

                // Verify the command buffer validity
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);

                // Get new data by ignoring all but the last structure
                pSRTData = (D3DHAL_DP2SETRENDERTARGET*)lpPrim +
                           (lpIns->wStateCount - 1);

                __SetRenderTarget(pCtxt,
                                  pSRTData->hRenderTarget,
                                  pSRTData->hZBuffer);

                // Update state to pickup new render/depth surfaces
                pCtxt->bChanged = TRUE;
                ddrval = __HWSetupPrimitive( pCtxt, &S3FVFOff);
                if( ddrval != DD_OK )
                {
                    DPF("Error processing D3DDP2OP_SETRENDERTARGET primitive states");
                    PARSE_ERROR_AND_EXIT(lpdp2d, lpIns, lpInsStart, ddrval);
                }

                NEXTINSTRUCTION(lpIns, D3DHAL_DP2SETRENDERTARGET,
                                lpIns->wStateCount, 0);
            }
            break;

        case D3DDP2OP_CLEAR:
            // Perform hardware-assisted clearing on the rendering target,
            // depth buffer or stencil buffer. This replaces the old D3dClear
            // and D3dClear2 callbacks. 

            {
                D3DHAL_DP2CLEAR* pClear;

                // Verify the command buffer validity
                CHECK_CMDBUF_LIMITS(lpdp2d, lpPrim);

                // Get new data by ignoring all but the last structure
                pClear = (D3DHAL_DP2CLEAR*)lpPrim;

                DPF_DBG("D3DDP2OP_CLEAR dwFlags=%08lx dwColor=%08lx "
                           "dvZ=%08lx dwStencil=%08lx",
                           pClear->dwFlags,
                           pClear->dwFillColor,
                           (DWORD)(pClear->dvFillDepth*0x0000FFFF),
                           pClear->dwFillStencil);

                __Clear(pCtxt, 
                        pClear->dwFlags,        // in:  surfaces to clear
                        pClear->dwFillColor,    // in:  Color value for rtarget
                        pClear->dvFillDepth,    // in:  Depth value for
                                                //      Z-buffer (0.0-1.0)
                        pClear->dwFillStencil,  // in:  value used to clear stencil
                                                // in:  Rectangles to clear
                        (LPD3DRECT)((LPBYTE)pClear + 
                                    sizeof(D3DHAL_DP2CLEAR) -
                                    sizeof(RECT)),
                        (DWORD)lpIns->wStateCount); // in:  Number of rectangles

                NEXTINSTRUCTION(lpIns, RECT, lpIns->wStateCount, 
                                (sizeof(D3DHAL_DP2CLEAR) - sizeof(RECT))); 
            }
            break;

        default:

            ASSERTDD((pCtxt->ppdev->pD3DParseUnknownCommand),
                     "D3D DX6 ParseUnknownCommand callback == NULL");

            if( (ddrval=( pCtxt->ppdev->pD3DParseUnknownCommand)( lpIns , &lpResumeIns)) == DD_OK) {
                // Resume buffer processing after D3DParseUnknownCommand
                // was succesful in processing an unknown command
                lpIns = lpResumeIns;
                break;
            }

            DPF_DBG("unhandled opcode (%d)- returning D3DERR_COMMAND_UNPARSED @ addr %x", 
                                                               lpIns->bCommand,lpIns);
            PARSE_ERROR_AND_EXIT( lpdp2d, lpIns, lpInsStart, ddrval);
        } // switch

    } //while

    lpdp2d->ddrval = DD_OK;

Exit_DrawPrimitives2:

    // Setup flag to fix 3D-2D primitive rendering transition case
    pCtxt->ppdev->fTriangleFix = TRUE;

    CHOP_ROUND_OFF();

    DPF_DBG("leaving D3DDrawPrimitive2");

    return DDHAL_DRIVER_HANDLED;
}

/******************************Public*Routine******************************\
*
* DWORD D3DValidateTextureStageState
*
* ValidateTextureStageState evaluates the current state for blending 
* operations (including multitexture) and returns the number of passes the 
* hardware can do it in. This is a mechanism to query the driver about 
* whether it is able to handle the current stage state setup that has been 
* set up in hardware.  For example, some hardware cannot do two simultaneous 
* modulate operations because they have only one multiplication unit and one 
* addition unit.  
*
* The other reason for this function is that some hardware may not map 
* directly onto the Direct3D state architecture. This is a mechanism to map 
* the hardware's capabilities onto what the Direct3D DDI expects.
*
* Parameters
*
*      lpvtssd
*
*          .dwhContext
*               Context handle
*          .dwFlags
*               Flags, currently set to 0
*          .dwReserved
*               Reserved
*          .dwNumPasses
*               Number of passes the hardware can perform the operation in
*          .ddrval
*               return value
*
\**************************************************************************/
DWORD WINAPI 
D3DValidateTextureStageState (LPD3DHAL_VALIDATETEXTURESTAGESTATEDATA lpvtssd )
{
    LPS3_TEXTURE lpTexture;
    S3_CONTEXT   *pCtxt;
    DWORD mag, min, cop, ca1, ca2, aop, aa1, aa2, alphabitmask;

    DPF_DBG("D3DValidateTextureStageState called");

    pCtxt = (LPS3_CONTEXT) lpvtssd->dwhContext;

    // Check if we got a valid context handle.
    CHK_CONTEXT(pCtxt,lpvtssd->ddrval,"D3DValidateTextureStageState");

    lpvtssd->dwNumPasses = 0;
    lpvtssd->ddrval =  DD_OK;

    mag = pCtxt->tssStates[D3DTSS_MAGFILTER];
    min = pCtxt->tssStates[D3DTSS_MINFILTER];
    cop = pCtxt->tssStates[D3DTSS_COLOROP];
    ca1 = pCtxt->tssStates[D3DTSS_COLORARG1];
    ca2 = pCtxt->tssStates[D3DTSS_COLORARG2];
    aop = pCtxt->tssStates[D3DTSS_ALPHAOP];
    aa1 = pCtxt->tssStates[D3DTSS_ALPHAARG1];
    aa2 = pCtxt->tssStates[D3DTSS_ALPHAARG2];


    if (!pCtxt->tssStates[D3DTSS_TEXTUREMAP])
    {
        lpvtssd->dwNumPasses = 1;
        DPF_DBG("no texture");

        // Current is the same as diffuse in stage 0
        if (ca2 == D3DTA_CURRENT)
            ca2 = D3DTA_DIFFUSE;
        if (aa2 == D3DTA_CURRENT)
            aa2 = D3DTA_DIFFUSE;

        // Check TSS even with texture handle = 0 since
        // certain operations with the fragments colors might
        // be  possible. Here we only allow plain "classic" rendering

        if ((ca1 == D3DTA_DIFFUSE )    && 
            (cop == D3DTOP_SELECTARG1) &&
            (aa1 == D3DTA_DIFFUSE )    &&
            (aop == D3DTOP_SELECTARG1))
        {
        }
        else if ((ca2 == D3DTA_DIFFUSE )    && 
                 (cop == D3DTOP_SELECTARG2) &&
                 (aa2 == D3DTA_DIFFUSE) &&
                 (aop == D3DTOP_SELECTARG2))
        {
        } 
        // Default modulation
        else if ((ca2 == D3DTA_DIFFUSE)   && 
                 (ca1 == D3DTA_TEXTURE)   && 
                 (cop == D3DTOP_MODULATE) &&
                 (aa1 == D3DTA_TEXTURE)   && 
                 (aop == D3DTOP_SELECTARG1)) 
        {
        }
        // Check disable
        else if (cop == D3DTOP_DISABLE) 
        {
        }
        else
            goto Fail_Validate;



    } else
    if ((mag != D3DTFG_POINT && mag != D3DTFG_LINEAR) || 
        (min != D3DTFG_POINT && min != D3DTFG_LINEAR)
       )
    {
        lpvtssd->ddrval = D3DERR_CONFLICTINGTEXTUREFILTER;
        DPF_DBG("D3DERR_CONFLICTINGTEXTUREFILTER");
    } else
    {
        lpvtssd->dwNumPasses = 1;

        lpTexture = TextureHandleToPtr(pCtxt->tssStates[D3DTSS_TEXTUREMAP],
                                       pCtxt);
        if (lpTexture == NULL)
        {
            lpvtssd->ddrval =  DDERR_SURFACELOST;
            return (DDHAL_DRIVER_HANDLED);
        }

        alphabitmask = lpTexture->dwRGBAlphaBitMask;

        // Current is the same as diffuse in stage 0
        if(ca2 == D3DTA_CURRENT)
            ca2 = D3DTA_DIFFUSE;
        if(aa2 == D3DTA_CURRENT)
            aa2 = D3DTA_DIFFUSE;

        // Check decal
        if((ca1 == D3DTA_TEXTURE )    && 
           (cop == D3DTOP_SELECTARG1) &&
           (aa1 == D3DTA_TEXTURE)     && 
           (aop == D3DTOP_SELECTARG1)) {
        }
        // Check modulate
        else if((ca2 == D3DTA_DIFFUSE)   && 
                (ca1 == D3DTA_TEXTURE)   && 
                (cop == D3DTOP_MODULATE) &&
                (aa1 == D3DTA_TEXTURE)   && 
                (aop == D3DTOP_SELECTARG1)) 
        {
            // texture modulation must have alpha channel as virge
            // is using AlphaBlending to do modulate
            if (!alphabitmask && 
                (cop == D3DTOP_MODULATE))
            {
                lpvtssd->ddrval = D3DERR_WRONGTEXTUREFORMAT;
                lpvtssd->dwNumPasses = 0;
                DPF_DBG("D3DERR_WRONGTEXTUREFORMAT a format with alpha must be used");
                goto Exit_ValidateTSS;
            }
        }
        // Check modulate (legacy)
        else if((ca2 == D3DTA_DIFFUSE)   && 
                (ca1 == D3DTA_TEXTURE)   && 
                (cop == D3DTOP_MODULATE) &&
                (aa1 == D3DTA_TEXTURE)   && 
                (aop == D3DTOP_LEGACY_ALPHAOVR)) 
        {
        }
        // Check decal alpha
        else if((ca2 == D3DTA_DIFFUSE)            && 
                (ca1 == D3DTA_TEXTURE)            && 
                (cop == D3DTOP_BLENDTEXTUREALPHA) &&
                (aa2 == D3DTA_DIFFUSE)            && 
                (aop == D3DTOP_SELECTARG2)) {
        }
        // Check add
        else if((ca2 == D3DTA_DIFFUSE) && 
                (ca1 == D3DTA_TEXTURE) && 
                (cop == D3DTOP_ADD)    &&
                (aa2 == D3DTA_DIFFUSE) && 
                (aop == D3DTOP_SELECTARG2)) {
        }
        // Check disable
        else if((cop == D3DTOP_DISABLE) || 
                  (cop == D3DTOP_SELECTARG2 && 
                   ca2 == D3DTA_DIFFUSE     && 
                   aop == D3DTOP_SELECTARG2 && 
                   aa2 == D3DTA_DIFFUSE)       ) {
        }
        // Don't understand
        else {
Fail_Validate:
            DPF_DBG("Failing with cop=%d ca1=%d ca2=%d aop=%d aa1=%d aa2=%d",
                       cop,ca1,ca2,aop,aa1,aa2);

            if (!((cop == D3DTOP_DISABLE)           ||
                  (cop == D3DTOP_ADD)               ||
                  (cop == D3DTOP_MODULATE)          ||
                  (cop == D3DTOP_BLENDTEXTUREALPHA) ||
                  (cop == D3DTOP_SELECTARG2)        ||
                  (cop == D3DTOP_SELECTARG1)))
                    lpvtssd->ddrval = D3DERR_UNSUPPORTEDCOLOROPERATION;
            
            else if (!((aop == D3DTOP_SELECTARG1)      ||
                       (aop == D3DTOP_SELECTARG2)      ||
                       (aop == D3DTOP_MODULATE)        ||
                       (aop == D3DTOP_LEGACY_ALPHAOVR)))
                    lpvtssd->ddrval = D3DERR_UNSUPPORTEDALPHAOPERATION;

            else if (!(ca1 == D3DTA_TEXTURE))
                    lpvtssd->ddrval = D3DERR_UNSUPPORTEDCOLORARG;

            else if (!(ca2 == D3DTA_DIFFUSE))
                    lpvtssd->ddrval = D3DERR_UNSUPPORTEDCOLORARG;

            else if (!(aa1 == D3DTA_TEXTURE))
                    lpvtssd->ddrval = D3DERR_UNSUPPORTEDALPHAARG;

            else if (!(aa2 == D3DTA_DIFFUSE))
                    lpvtssd->ddrval = D3DERR_UNSUPPORTEDALPHAARG;
            else
                    lpvtssd->ddrval = D3DERR_UNSUPPORTEDALPHAOPERATION;

            lpvtssd->dwNumPasses = 0;
            goto Exit_ValidateTSS;
        }

Exit_ValidateTSS:
        ;
    }


    DPF_DBG("D3DValidateTextureStageState returned with dwNumPasses=%d error=%x",
                lpvtssd->dwNumPasses,lpvtssd->ddrval);
    return DDHAL_DRIVER_HANDLED;
}

//************************************************************************
//
// HRESULT __CheckFVFRequest
//
// This utility function verifies that the requested FVF format makes sense
// and computes useful offsets into the data and a stride between succesive
// vertices.
//
//************************************************************************
HRESULT __CheckFVFRequest(DWORD dwFVF, LPS3FVFOFFSETS lpS3FVFOff)
{
    DWORD stride;
    UINT iTexCount; 

    memset(lpS3FVFOff, 0, sizeof(S3FVFOFFSETS));

    if ( (dwFVF & (D3DFVF_RESERVED0 | D3DFVF_RESERVED1 | D3DFVF_RESERVED2 |
         D3DFVF_NORMAL)) ||
         ((dwFVF & (D3DFVF_XYZ | D3DFVF_XYZRHW)) == 0) )
    {
        // can't set reserved bits, shouldn't have normals in
        // output to rasterizers, and must have coordinates
        return DDERR_INVALIDPARAMS;
    }

    lpS3FVFOff->dwStride = sizeof(D3DVALUE) * 3;

    if (dwFVF & D3DFVF_XYZRHW)
    {
        lpS3FVFOff->dwStride += sizeof(D3DVALUE);
    }
    if (dwFVF & D3DFVF_DIFFUSE)
    {
        lpS3FVFOff->dwColOffset = lpS3FVFOff->dwStride;
        lpS3FVFOff->dwStride += sizeof(D3DCOLOR);
    }
    if (dwFVF & D3DFVF_SPECULAR)
    {
        lpS3FVFOff->dwSpcOffset = lpS3FVFOff->dwStride;
        lpS3FVFOff->dwStride  += sizeof(D3DCOLOR);
    }

    iTexCount = (dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;

    if (iTexCount >= 1)
    {
        lpS3FVFOff->dwTexBaseOffset = lpS3FVFOff->dwStride;
        lpS3FVFOff->dwTexOffset = lpS3FVFOff->dwTexBaseOffset;

        if (0xFFFF0000 & dwFVF)
        {
            //expansion of FVF, these 16 bits are designated for up to 
            //8 sets of texture coordinates with each set having 2bits
            //Normally a capable driver has to process all coordinates
            //However, code below only show correct parsing w/o really
            //observing all the texture coordinates.In reality,this would 
            //result in incorrect result.
            UINT i,numcoord;
            DWORD extrabits;
            for (i = 0; i < iTexCount; i++)
            {
                extrabits= (dwFVF >> (16+2*i)) & 0x0003;
                switch(extrabits)
                {
                case    1:
                    // one more D3DVALUE for 3D textures
                    numcoord = 3;
                case    2:
                    // two more D3DVALUEs for 4D textures
                    numcoord = 4;
                case    3:
                    // one less D3DVALUE for 1D textures
                    numcoord = 1;
                default:
                    // i.e. case 0 regular 2 D3DVALUEs
                    numcoord = 2;
                }

                DPF_DBG("Expanded TexCoord set %d has a offset %8lx",
                           i,lpS3FVFOff->dwStride);

                lpS3FVFOff->dwStride += sizeof(D3DVALUE) * numcoord;
            }
            DPF_DBG("Expanded dwVertexType=0x%08lx has %d Texture Coords "
                       "with total stride=0x%08lx",
                       dwFVF, iTexCount, lpS3FVFOff->dwStride);
        }
        else
            lpS3FVFOff->dwStride   += iTexCount*sizeof(D3DVALUE) * 2;

    } else {
        lpS3FVFOff->dwTexBaseOffset = 0;
        lpS3FVFOff->dwTexOffset = 0;
    }

    return DD_OK;
}

//-----------------------------------------------------------------------------
//
// void __TextureBlt
//
// Transfer a texture from system memory into AGP or video memory
//-----------------------------------------------------------------------------
HRESULT 
__TextureBlt(S3_CONTEXT* pContext,
             D3DHAL_DP2TEXBLT* lpdp2texblt)
{
    S3_TEXTURE *dsttex,*srctex;
    ULONG ulBltCmd;
    PPDEV ppdev = pContext->ppdev;
    BYTE* pjMmBase = ppdev->pjMmBase;
    DWORD srcX, srcY, dstX, dstY, dstWidth, dstHeight;

    DPF_DBG("Entering __TextureBlt");

    ASSERTDD(0 != lpdp2texblt->dwDDDestSurface, "in __TextureBlt");
    ASSERTDD(0 != lpdp2texblt->dwDDSrcSurface, "in __TextureBlt");

    if ((!lpdp2texblt->dwDDDestSurface)||(!lpdp2texblt->dwDDSrcSurface))
        return DDERR_INVALIDPARAMS;

    dsttex = TextureHandleToPtr(lpdp2texblt->dwDDDestSurface,pContext);
    srctex = TextureHandleToPtr(lpdp2texblt->dwDDSrcSurface,pContext);

    if (NULL != dsttex && NULL != srctex)
    {
        DPF_DBG("TexBlt from %x to %x",srctex,dsttex);

        // Get size of blt transfer
        srcX = lpdp2texblt->rSrc.left;
        srcY = lpdp2texblt->rSrc.top;
        dstX = lpdp2texblt->pDest.x;
        dstY = lpdp2texblt->pDest.y;
        dstWidth = lpdp2texblt->rSrc.right - lpdp2texblt->rSrc.left;
        dstHeight = lpdp2texblt->rSrc.bottom - lpdp2texblt->rSrc.top;

        // Assume we can do the blt top-to-bottom, left-to-right:
        ulBltCmd = ppdev->ulCommandBase |
                   S3D_X_POSITIVE_BLT   |
                   S3D_Y_POSITIVE_BLT;

        if ((dsttex == srctex) && (srcX + dstWidth  > dstX) &&
            (srcY + dstHeight > dstY) && (dstX + dstWidth > srcX) &&
            (dstY + dstHeight > srcY) &&
            (((srcY == dstY) && (dstX > srcX) )
                 || ((srcY != dstY) && (dstY > srcY))))
        {
            // Okay, we have to do the blt bottom-to-top, right-to-left:

            srcX = lpdp2texblt->rSrc.right - 1;
            srcY = lpdp2texblt->rSrc.bottom - 1;
            dstX = (lpdp2texblt->pDest.x + dstWidth) - 1;
            dstY = (lpdp2texblt->pDest.y + dstHeight) - 1;

            ulBltCmd = ppdev->ulCommandBase |
                       S3D_X_NEGATIVE_BLT   |
                       S3D_Y_NEGATIVE_BLT;
        }

        WAIT_DMA_IDLE(ppdev);       // wait till DMA is idle.
        TRIANGLE_WORKAROUND( ppdev );

        S3DFifoWait(ppdev, 7);

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

        NW_SET_REG( ppdev, pjMmBase,
                    S3D_BLT_SOURCE_BASE,
                    (DWORD)(srctex->fpVidMem)
                  );

        NW_SET_REG( ppdev, pjMmBase,
                    S3D_BLT_DESTINATION_BASE,
                    (DWORD)(dsttex->fpVidMem)
                  );

        NW_SET_REG( ppdev, pjMmBase,
                    S3D_BLT_DESTINATION_SOURCE_STRIDE,
                    ( (DWORD) ((dsttex->lPitch & 0xFFF8) << 16) |
                      (DWORD) (srctex->lPitch & 0xFFF8) )
                  );

        S3DGPWait(ppdev);

        // texture download
        NW_SET_REG( ppdev, pjMmBase,
                    S3D_BLT_COMMAND,
                    (ulBltCmd                       |
                     S3D_BITBLT                     |
                     S3D_DRAW_ENABLE                |
                     (SRC_COPY << S3D_ROP_SHIFT))
                  );

    } else
        return DDERR_SURFACELOST;

    DPF_DBG("Exiting __TextureBlt");

    return DD_OK;
}   //__TextureBlt

//-----------------------------------------------------------------------------
//
// void __SetRenderTarget
//
// Set new render and z buffer target surfaces
//-----------------------------------------------------------------------------
            
HRESULT  __SetRenderTarget(S3_CONTEXT* pContext,
                           DWORD hRenderTarget,
                           DWORD hZBuffer)
{
    PPDEV ppdev = pContext->ppdev;
    S3_TEXTURE *pNewRenderBuffer = NULL,*pNewZBuffer = NULL;

    DPF_DBG("Entering __SetRenderTarget Target=%d Z=%d",
                                  hRenderTarget,hZBuffer);

    // Verify we have a valid handle to the new render target, and that it
    // translates into a valid surface structure which describes a surface
    // located in video memory
    if (0 == hRenderTarget)
    {
        DPF_DBG("ERROR: 0==hRenderTarget in __SetRenderTarget, exiting");
        return DDERR_INVALIDPARAMS;    
    }

    pNewRenderBuffer = TextureHandleToPtr(hRenderTarget,pContext);

    if (NULL == pNewRenderBuffer)
    {
        DPF_DBG("ERROR: NULL == pNewRenderBuffer "
                   "in __SetRenderTarget, exiting");
        return DDERR_INVALIDPARAMS;
    }

    if (DDSCAPS_SYSTEMMEMORY & pNewRenderBuffer->dwCaps)
    {
        DPF_DBG("ERROR: Render Surface in SYSTEMMEMORY"
                    " in __SetRenderTarget, exiting");
        return DDERR_INVALIDPARAMS;    
    }

    // Even if we formerly had  a Z buffer doesn't imply we need
    // to have one now!
    if (0 != hZBuffer)
    {
        pNewZBuffer = TextureHandleToPtr(hZBuffer,pContext);
        if (NULL != pNewZBuffer)
        {
            if (DDSCAPS_SYSTEMMEMORY & pNewZBuffer->dwCaps)
            {
                DPF_DBG("ERROR: Render Surface in SYSTEMMEMORY "
                            "in __SetRenderTarget");
                return DDERR_INVALIDPARAMS;    
            }

        }
        else
        {
            return DDERR_INVALIDPARAMS;    
        }
    }
    else
    {
        pNewZBuffer = NULL;
    }

    pContext->pSurfRender = pNewRenderBuffer;
    pContext->pSurfZBuffer = pNewZBuffer;
    pContext->RenderSurfaceHandle = hRenderTarget;
    pContext->ZBufferHandle = hZBuffer;

    DPF_DBG("Exiting __SetRenderTarget");

    return DD_OK;
} // __SetRenderTarget


//-----------------------------------------------------------------------------
//
// void __ClearOneRect
//
// Clears one rectangle region at a time using the S3 engine
//
//-----------------------------------------------------------------------------
void __ClearOneRect(PPDEV ppdev,
                    RECTL* pRect,
                    DWORD dwFillValue,
                    FLATPTR fpVidMem,
                    DWORD dwPitch,
                    DWORD dwFlags)
{
    BYTE* pjMmBase = ppdev->pjMmBase;
    DWORD dstX      = pRect->left;
    DWORD dstY      = pRect->top;
    DWORD dstWidth  = pRect->right - pRect->left;
    DWORD dstHeight = pRect->bottom - pRect->top;


    WAIT_DMA_IDLE(ppdev);       // wait till DMA is idle.
    TRIANGLE_WORKAROUND( ppdev );

    S3DFifoWait(ppdev, 7);

    NW_SET_REG( ppdev, pjMmBase,
                S3D_BLT_PATTERN_FOREGROUND_COLOR,
                dwFillValue
              );

    NW_SET_REG( ppdev, pjMmBase,
                S3D_BLT_PATTERN_BACKGROUND_COLOR,
                dwFillValue
              );

    // Set the base address for the destination
    NW_SET_REG( ppdev, pjMmBase,
            S3D_BLT_DESTINATION_BASE,
            (DWORD)(fpVidMem)
          );

    NW_SET_REG( ppdev, pjMmBase,
                S3D_BLT_DESTINATION_SOURCE_STRIDE,
                ( (DWORD) ((dwPitch & 0xFFF8) << 16) |
                  (DWORD) (dwPitch & 0xFFF8) )
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

}

//-----------------------------------------------------------------------------
//
// void __Clear
//
// Clears selectively the frame buffer, z buffer and stencil buffer for the 
// D3D Clear2 callback and for the D3DDP2OP_CLEAR command token.
//
//-----------------------------------------------------------------------------
HRESULT  __Clear( S3_CONTEXT* pContext,
              DWORD   dwFlags,        // in:  surfaces to clear
              DWORD   dwFillColor,    // in:  Color value for rtarget
              D3DVALUE dvFillDepth,   // in:  Depth value for
                                      //      Z-buffer (0.0-1.0)
              DWORD   dwFillStencil,  // in:  value used to clear stencil buffer
              LPD3DRECT lpRects,      // in:  Rectangles to clear
              DWORD   dwNumRects)     // in:  Number of rectangles
{
    int i;
    RECTL*  pRect;
    PPDEV   ppdev = pContext->ppdev;
    S3_TEXTURE *pSurfRender, *pSurfZBuffer;

    DPF_DBG("In __Clear");




    if (D3DCLEAR_TARGET & dwFlags)
    {
        DWORD   r,g,b;

        pSurfRender = TextureHandleToPtr(pContext->RenderSurfaceHandle 
                                        ,pContext);
        if (!pSurfRender)
        {
            return DDERR_SURFACELOST;
        }

        // Translate 8888RGBA value into hw specific value
        switch(ppdev->cjPelSize)
        {
        case 1: //8bpp
            // we shouldn't get this format since we dont' hw accelerate 3d
            // in this mode.
            break;
        case 2: //16bpp
            dwFillColor = ( ( (dwFillColor & 0x0000F8) >> 3 ) |
                            ( (dwFillColor & 0x00F800) >> 6 ) |
                            ( (dwFillColor & 0xF80000) >> 9 ) );
        default:
        case 3: //24bpp
            break; // no conversion necessary
        }

        pRect = (RECTL*)lpRects;

        // wait until all drawing is done
        while (updateFlipStatus( pSurfRender->fpVidMem, 
                                           D3DGLOBALPTR(pContext) )
               != DD_OK) ;

        // Do clear for each Rect that we have
        for (i = dwNumRects; i > 0; i--)
        {
            DPF_DBG("calling __ClearOneRect for color");

            __ClearOneRect(ppdev,
                           pRect,
                           dwFillColor,
                           pSurfRender->fpVidMem,
                           pSurfRender->lPitch,
                           DDBLT_COLORFILL);
            pRect++;
        }
    }

    if ((D3DCLEAR_ZBUFFER & dwFlags) 
        && (0 != pContext->ZBufferHandle))
    {
        DWORD   dwZbufferClearValue;

        pRect = (RECTL*)lpRects;

        // convert z value into 16bit fixed value, since this is the only
        // zbuffer format supported on this card
        dwZbufferClearValue = (DWORD)(dvFillDepth*0x0000FFFF);

        pSurfZBuffer = TextureHandleToPtr(pContext->ZBufferHandle
                                        ,pContext);

        if (!pSurfZBuffer)
        {
            return DDERR_SURFACELOST;
        }

        for (i = dwNumRects; i > 0; i--)
        {
            DPF_DBG("calling __ClearOneRect for depth");

            __ClearOneRect(ppdev,
                           pRect,
                           dwZbufferClearValue,
                           pSurfZBuffer->fpVidMem,
                           pSurfZBuffer->lPitch,
                           DDBLT_DEPTHFILL);
            pRect++;
        }
    }
    return DD_OK;

} // __Clear



