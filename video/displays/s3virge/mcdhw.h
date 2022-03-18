/******************************Module*Header*******************************\
* Module Name: mcdhw.h
*
* Driver-specific structures and defines for the S3 Virge MCD driver.
*
* Copyright (c) 1996,1997 Microsoft Corporation
\**************************************************************************/

// Features which can be turned on/off with #defines

#define TEX_ENABLE_MIP_MAPS  1 // Set to 1 to use mip maps. Note that if 0 it
                               // doesnt prevent detection and validation ,but
                               // space allocation, loading, and state setting
                               // are done as if there was only one level.

#define TEX_NON_SQR          1 // Set to 1 to enable fixes to run
                               // hw-accelerated non-square textures
                               // since the hw only supports directly square
                               // textures

#define TEXLUMSPEC_BLEND_DKN 1 // Set to 1 to enable substitution of luminance
                               // texture and a specific special blending
                               // function usually used for darkening effects
                               // with an alpha texture channel the S3Virge
                               // allows us

#define TEXALPHA_FUNC        1 // Set to 1 to enable sustitution of alpha
                               // function with reference value with texturing
                               // enabled usually used for billboarding with
                               // an alpha texture channel the S3Virge
                               // allows us

#define FORCE_SYNC           1 // Set to 1 to force waiting for 3D Drawing to be
                               // done before returning control from the MCD
                               // driver. This MUST be set to 1 for the S3Virge
                               // because HW_WAIT_DRAWING_DONE does a compulsory
                               // operation needed between 3D and 2D operations.

// Demonstrative code features only - not to be set!

#define TEX_PALETTED_SUPPORT 0 // The S3Virge doesn't handle paletted textures
                               // in the way OpenGL expects. We have this code
                               // here disabled by having TEX_PALETTED_SUPPORT
                               // set to 0, only in order to show some code
                               // when paletted textures are supported.

#define TEST_REQ_FLAGS       0 // Set to 1 to enable code which requests
                               // different scaling factors for the color and/or
                               // depth values

#define CLIP_TEXTURE_XFORM   0 // Set to 1 to enable clipping to be applied to
                               // the q,r values of a texture coordinate. Since
                               // we are however not using them, leave it as 0.


// Debugging #defines

#define DBG_TESSELATION      0 // Set to 1 to test the triangle tesselation
                               // code & see clearly which subtriangles are
                               // generated. Notice that tesselation is done
                               // only because on this hw we cannot span more
                               // than a certain number of texels during the
                               // rendering of a single triangle. If this
                               // limitation is not present, all of the
                               // tesselation related code can go away.

#define DBG_FAIL_TEX_DRAW    0 // Set to 1 to fail picking rendering functions
                               // when the RC state indicates the use of a
                               // texture so no hw texturing is done

#define DBG_ONLY_FRONTBUFFER 0 // Set to 1 to force drawing to the front buffer
                               // even if a back buffer was requested and
                               // allocated, disable swapping. Useful to see
                               // what an app is doing in the backbuffer.

#define DBG_TEX_WIPE_OUT     0 // Set to 1 in order to fill the texture map
                               // with white texels before unloading a texture
                               // to detect erroneous texture map usage.

#define DBG_3D_NO_DRAW       0 // Set to 1 on order to return from all MCDrvDraw
                               // calls without doing anything, and without
                               // falling back into sw rendering. Useful for
                               // debugging and performance monitoring.

#define DBG_FAIL_ALL_DRAWING 0 // Set to 1 on order to return from all MCDrvDraw
                               // calls without doing anything, however
                               // falls back into sw rendering. Useful for
                               // initial stages of driver building.

#define DBG_BUFALLOC         0 // Set to 1 to help debugging back and z buffer
                               // allocation

#define DBG_BUFALLOC_PARTIAL 0 // Used in conjunction with DBG_BUFALLOC, set to
                               // 1 to force use of per-window z and back buffers


#if DBG_TESSELATION
    // Private counter for textured triangle tesselation debugging code
    LONG iTessCount;
#endif


#define ASM_ACCEL         1     // Enable/disable asm code

#define __MCD_USER_CLIP_MASK    ((1 << MCD_MAX_USER_CLIP_PLANES) - 1)

#define __MCD_CW          0
#define __MCD_CCW         1

#define __MCD_FRONTFACE   MCDVERTEX_FRONTFACE
#define __MCD_BACKFACE    MCDVERTEX_BACKFACE
#define __MCD_NOFACE      -1

#define __MCDENABLE_TWOSIDED                  0x0001
#define __MCDENABLE_Z                         0x0002
#define __MCDENABLE_SMOOTH                    0x0004
#define __MCDENABLE_GRAY_FOG                  0x0008
#define __MCDENABLE_BLEND                     0x0010
#define __MCDENABLE_COLORED                   0x0020
#define __MCDENABLE_TEXTURED                  0x0040
#define __MCDENABLE_TEXPERSP                  0x0080
#define __MCDENABLE_TEXMIPMAP                 0x0100
#define __MCDENABLE_TEX_NONSQR                0x0200
#define __MCDENABLE_TEXLUMSPEC_BLEND_DKN      0x0400
#define __MCDENABLE_TEXLUMSPEC_BLEND_MODE_DKN 0x0800
#define __MCDENABLE_TEXALPHAFUNC              0x1000
#define __MCDENABLE_TEXALPHAFUNC_MODE         0x2000


// Scaling factors for the fixed point S3Virge registers
// used for z and x-coordinates
#define S3_S15_SCALE (MCDFLOAT) (0x008000)
#define S3_S20_SCALE (MCDFLOAT) (0x100000)

// This delta value is added to the x coordinates for drawing lines and
// points in order to draw according to OpenGL rules and pass conformance
#define S3DELTA ((MCDFLOAT)(0.01))

typedef LONG MCDFIXED;

typedef struct _RGBACOLOR {
    MCDFIXED r, g, b, a;
} RGBACOLOR;

#define SWAP_COLOR(p)\
{\
    MCDFLOAT tempC;\
\
    tempC = (p)->colors[0].r;\
    (p)->colors[0].r = (p)->colors[1].r;\
    (p)->colors[1].r = tempC;\
\
    tempC = (p)->colors[0].g;\
    (p)->colors[0].g = (p)->colors[1].g;\
    (p)->colors[1].g = tempC;\
\
    tempC = (p)->colors[0].b;\
    (p)->colors[0].b = (p)->colors[1].b;\
    (p)->colors[1].b = tempC;\
}

#define COPY_COLOR(pDst, pSrc)\
{\
    pDst.r = pSrc.r;\
    pDst.g = pSrc.g;\
    pDst.b = pSrc.b;\
}

#define MCDFIXEDRGB(fixColor, fltColor)\
    fixColor.r = (MCDFIXED)(fltColor.r * pRc->rScale);\
    fixColor.g = (MCDFIXED)(fltColor.g * pRc->gScale);\
    fixColor.b = (MCDFIXED)(fltColor.b * pRc->bScale);

typedef struct _DRVPIXELFORMAT {
    UCHAR cColorBits;
    UCHAR rBits;
    UCHAR gBits;
    UCHAR bBits;
    UCHAR aBits;
    UCHAR rShift;
    UCHAR gShift;
    UCHAR bShift;
    UCHAR aShift;
} DRVPIXELFORMAT;

typedef struct _DEVWND {
    ULONG createFlags;              // (RC) creation flags
    LONG iPixelFormat;              // pixel format ID for this window
    ULONG dispUnique;               // display resolution uniqueness

    ULONG allocatedBufferHeight;    // Same for back and z on S3Virge
    ULONG allocatedBufferWidth;     // Same for back and z on S3Virge

    BOOL bValidBackBuffer;          // back buffer validity
    ULONG backBufferOffset;         // byte offset to start of back buffer
    ULONG ulBackBufferStride;       // back buffer stride.

    BOOL bValidZBuffer;             // z buffer validity
    ULONG zBufferBase;              // byte offset to start of z buffer pool
    ULONG zBufferOffset;            // byte offset to start of z buffer
    ULONG zPitch;                   // z buffer pitch in bytes

    ULONG numPadScans;              // number of pad scan lines in buffers

    OH* pohBackBuffer;              // ofscreen pools
    OH* pohZBuffer;
#ifdef MCD95
    UCHAR cColorBits;               // cached from DEVRC.pixelFormat
#endif

} DEVWND;

typedef struct _DEVRC DEVRC;

//
// DEVRC per S3, Qword-aligned floating point variables.
//
typedef struct _DEVRC
{
    DEVRC *pOldDevRC;             // Pointer to unaligned DEVRC.

// Triangle-filling parameters and state:
    MCDFLOAT halfArea;
    ULONG hwZBase;                  // ZBuffer specific info

    MCDFLOAT invHalfArea;
    ULONG hwZStride;

    MCDFLOAT dx02;
    MCDFIXED fxdrdx;

    MCDFLOAT dx01;
    MCDFIXED fxdrdy;        // fixed-point delta values

    MCDFLOAT dx12;
    MCDFIXED fxdgdx;

    MCDFLOAT dy02;
    MCDFIXED fxdgdy;

    MCDFLOAT dy01;
    MCDFIXED fxdbdx;

    MCDFLOAT dy12;
    MCDFIXED fxdbdy;

    MCDFLOAT invdy02;
    LONG cullFlag;

    MCDFLOAT dr01;
    LONG dr01Fill;

    MCDFLOAT dr02;
    LONG dr02Fill;

    MCDFLOAT drdx;
    LONG drdxFill;

    MCDFLOAT drdy;  // delta values
    LONG drdyFill;

    MCDFLOAT dg01;
    LONG dg01Fill;

    MCDFLOAT dg02;
    LONG dg02Fill;

    MCDFLOAT dgdx;
    LONG dgdxFill;

    MCDFLOAT dgdy;
    LONG dgdyFill;

    MCDFLOAT db01;
    LONG db01Fill;

    MCDFLOAT db02;
    LONG db02Fill;

    MCDFLOAT dbdx;
    LONG dbdxFill;

    MCDFLOAT dbdy;
    LONG dbdyFill;

    MCDFLOAT da01;
    LONG da01Fill;

    MCDFLOAT da02;
    LONG da02Fill;

    MCDFLOAT dadx;
    LONG dadxFill;

    MCDFLOAT dady;
    LONG dadyFill;

    MCDFLOAT dz01;
    LONG dz01Fill;

    MCDFLOAT dz02;
    LONG dx02Fill;

    MCDFLOAT dzdx;
    LONG dzdxFill;

    MCDFLOAT dzdy;
    LONG dzdyFill;

    MCDFLOAT du01;
    MCDFIXED fxdudx;

    MCDFLOAT du02;
    MCDFIXED fxdudy;

    MCDFLOAT dudx;
    MCDFIXED fxdvdx;

    MCDFLOAT dudy;  // delta values for textures
    MCDFIXED fxdvdy;

    MCDFLOAT dv01;
    MCDFIXED fxdwdx;

    MCDFLOAT dv02;
    MCDFIXED fxdwdy;

    MCDFLOAT dvdx;
    MCDFIXED fxdddx;

    MCDFLOAT dvdy;
    MCDFIXED fxdddy;

    MCDFLOAT dw01;
    LONG xOffset;

    MCDFLOAT dw02;
    LONG yOffset;

    MCDFLOAT dwdx;
    LONG viewportXAdjust;

    MCDFLOAT dwdy;
    LONG viewportYAdjust;

    MCDFLOAT dd01;
    LONG dd01Fill;

    MCDFLOAT dd02;
    LONG dd02Fill;

    MCDFLOAT dddx;
    LONG dddxFill;

    MCDFLOAT dddy;
    LONG dddyFill;

    MCDFLOAT ds01;
    LONG ds01Fill;

    MCDFLOAT ds02;
    LONG ds02Fill;

    MCDFLOAT dt01;
    LONG dt01Fill;

    MCDFLOAT dt02; // For mip mapping & filtering
    LONG dt02Fill;

    MCDFLOAT dsdx;
    LONG dsdxFill;

    MCDFLOAT dsdy;
    LONG dsdyFill;

    MCDFLOAT dtdx;
    LONG dtdxFill;

    MCDFLOAT dtdy;
    LONG dtdyFill;

// Scales to convert from float to fixed point formats
    MCDFLOAT rScale;
    MCDFIXED fxdadx;

    MCDFLOAT gScale;
    MCDFIXED fxdady;

    MCDFLOAT bScale;
    MCDFIXED fxdzdx;

    MCDFLOAT aScale;
    MCDFIXED fxdzdy;

    MCDFLOAT zScale;
    ULONG hwBpp;

    MCDFLOAT xScale;

    MCDFLOAT uvScale;
    ULONG hwVideoMode;

    MCDFLOAT wScale;
    ULONG hwZFunc;

    MCDFLOAT dScale;
    ULONG hwLineClipFunc;

    MCDFLOAT uBaseScale;
    ULONG hwTriClipFunc;

    MCDFLOAT vBaseScale;
    ULONG hwTexFunc;

    MCDFLOAT ufix;
    ULONG hwFillColor;

    MCDFLOAT vfix;
    ULONG hwFillZValue;

    MCDFLOAT uvMaxTexels;       // maximum delta U and V allowed along each triangle edge
    ULONG privateEnables;

// Texturing specific info
    MCDFLOAT uBase;
    ULONG    texMagFilter;

    MCDFLOAT vBase;
    ULONG    texMinFilter;

    MCDFLOAT texwidth;
    ULONG    bTexWrap;

    MCDFLOAT uFactor;
    LONG     texwidthLog2;

    MCDFLOAT vFactor;
    PDEV* ppdev;                // Valid for primitives only

    MCDFLOAT dudyEdge;          // delta values for textures
    MCDFIXED fxdrdyEdge;        // fixed-point delta values

    MCDFLOAT dvdyEdge;
    MCDFIXED fxdgdyEdge;

    MCDFLOAT dwdyEdge;
    MCDFIXED fxdbdyEdge;

    MCDFLOAT dddyEdge;
    MCDFIXED fxdadyEdge;

    MCDFLOAT duAB;
    MCDFIXED fxdzdyEdge;

    MCDFLOAT duAC;
    MCDFIXED fxdudyEdge;       // fixed-point delta values for textures

    MCDFLOAT dvAB;
    MCDFIXED fxdvdyEdge;

    MCDFLOAT dwAB;
    MCDFIXED fxdwdyEdge;

    MCDFLOAT dwAC;
    MCDFIXED fxdddyEdge;

    MCDFLOAT ddAB;
    LONG     ddABPad;

    MCDFLOAT ddAC;
    LONG     ddACPad;

    MCDFLOAT dxAC;
    LONG     dxACPad;

    MCDFLOAT dxAB;
    LONG     dxABPad;

    MCDFLOAT dxBC;
    LONG     dxBCPad;

    MCDFLOAT dyAC;
    LONG     dyACPad;

    MCDFLOAT dyAB;
    LONG     dyABPad;

    MCDFLOAT dyBC;
    LONG     dyBCPad;

    MCDFLOAT drAB;
    LONG     drABPad;

    MCDFLOAT drAC;
    LONG     drACPad;

    MCDFLOAT drdyEdge;  // delta values
    LONG     drdyEdgePad;

    MCDFLOAT dgAB;
    LONG     dgABPad;

    MCDFLOAT dgAC;
    LONG     dgACPad;

    MCDFLOAT dgdyEdge;
    LONG     dgdyEdgePad;

    MCDFLOAT dbAB;
    LONG     dbABPad;

    MCDFLOAT dbAC;
    LONG     dbACPad;

    MCDFLOAT dbdyEdge;
    LONG     dbdyEdgePad;

    MCDFLOAT daAB;
    LONG     daABPad;

    MCDFLOAT daAC;
    LONG     daACPad;

    MCDFLOAT dadyEdge;
    LONG     dadyEdgePad;

    MCDFLOAT dzAB;
    LONG     dzABPad;

    MCDFLOAT dzAC;
    LONG     dzACPad;

    MCDFLOAT dzdyEdge;
    LONG     dzdyEdgePad;

    MCDFLOAT invdyAC;
    LONG     invdyACPad;

    MCDFLOAT dvAC;
    LONG cFifo;

    MCDFLOAT dsAB,dsAC,dtAB,dtAC; // For mip mapping & filtering


    MCDRENDERSTATE MCDState;
    MCDTEXENVSTATE MCDTexEnvState;
    MCDVIEWPORT MCDViewport;

    // storage and pointers for clip processing:

    MCDVERTEX clipTemp[6 + MCD_MAX_USER_CLIP_PLANES];
    VOID (FASTCALL *lineClipParam)(MCDVERTEX*, const MCDVERTEX*, const MCDVERTEX*, MCDFLOAT);
    VOID (FASTCALL *polyClipParam)(MCDVERTEX*, const MCDVERTEX*, const MCDVERTEX*, MCDFLOAT);

    // Rendering functions:

    VOID (FASTCALL *renderPoint)(DEVRC *pRc, MCDVERTEX *pv);
    VOID (FASTCALL *renderLine)(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, BOOL resetLine);
    VOID (FASTCALL *renderTri)(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, MCDVERTEX *pv3);
    VOID (FASTCALL *clipLine)(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, BOOL bResetLine);
    VOID (FASTCALL *clipTri)(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, MCDVERTEX *pv3, ULONG clipFlags);
    VOID (FASTCALL *clipPoly)(DEVRC *pRc, MCDVERTEX *pv, ULONG numVert);
    VOID (FASTCALL *doClippedPoly)(DEVRC *pRc, MCDVERTEX **pv, ULONG numVert, ULONG clipFlags);
    VOID (FASTCALL *renderPointX)(DEVRC *pRc, MCDVERTEX *pv);
    VOID (FASTCALL *renderLineX)(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, BOOL resetLine);
    VOID (FASTCALL *renderTriX)(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, MCDVERTEX *pv3);

// Primitive-rendering function table:

    MCDCOMMAND * (FASTCALL *primFunc[10])(DEVRC *pRc, MCDCOMMAND *pCommand);

// Internal table of rendering functions:

    VOID (FASTCALL *drawPoint)(DEVRC *pRc, MCDVERTEX *pv);
    VOID (FASTCALL *drawLine)(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, BOOL resetLine);
    VOID (FASTCALL *drawTri)(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, MCDVERTEX *pv3, BOOL bCCW);

// Rendering helper functions:

    VOID (FASTCALL *HWSetupClipRect)(DEVRC *pRc, RECTL *pRect);

    VOID (FASTCALL *beginLineDrawing)(DEVRC *pRc);
    VOID (FASTCALL *endLineDrawing)(DEVRC *pRc);

    VOID (FASTCALL *beginPointDrawing)(DEVRC *pRc);

//
    DRVPIXELFORMAT pixelFormat;

    BOOL bCheapFog;
    BOOL allPrimFail;           // TRUE is the driver can't draw *any*
                                // primitives for current state
    BOOL pickNeeded;
    BOOL resetLineStipple;


    ULONG polygonFace[2];       // front/back face tables
    ULONG polygonMode[2];


    BOOL  bNeedFillColorBuffer;
    BOOL  bNeedFillZBuffer;

    BOOL zBufEnabled;
    BOOL backBufEnabled;

    ENUMRECTS *pEnumClip;       // Valid for primitives only

    MCDSURFACE *pMCDSurface;    // Valid for primitives only
    MCDRC *pMCDRc;              // Valid for primitives only

    MCDVERTEX *pvProvoking;     // provoking vertex
    UCHAR *pMemMax;             // command-buffer memory bounds

    UCHAR *pMemMin;
    BOOL xDerCCW;             // direction used in precalculated x derivatives
    BOOL bRGBMode;

    ULONG hwBlendFunc;

    MCDVERTEX *pNextClipTemp;
} DEVRC;

typedef struct _DEVMIPMAPLEVEL
{
    struct _DEVTEXTURE *parent;         // pointer to main texture structure
    UCHAR *pTexels;                     // pointer to client texture data

    LONG width, height;
    LONG widthImage, heightImage;       // Image dimensions without the border
    MCDFLOAT widthImagef, heightImagef; // Floating-point versions of above
    LONG widthLog2, heightLog2;         // Log2 of above
    LONG border;                        // Border size
    LONG requestedFormat;               // Requested internal format
    LONG baseFormat;                    // Base format
    LONG internalFormat;                // Actual internal format

    ULONG components;                   // Component resolution
    LONG redSize;
    LONG greenSize;
    LONG blueSize;
    LONG alphaSize;
    LONG luminanceSize;
    LONG intensitySize;

    // additional state needed for texture memory management
    OH    *pohMipMapTex;            // handle on memory region
    ULONG BaseAddress ;             // physical address of start of memory region

} DEVMIPMAPLEVEL;

typedef struct _DEVTEXTURE
{
    DEVMIPMAPLEVEL *MipMapLevels;   // Level specific data
                                    // Number of levels is determined by the last
                                    // element of the array with width==height==0
                                    // element [0] of the array has special significance
                                    // to us in order to take the format and other data
                                    // from there, and to store there the address of
                                    // the loaded texture

    ULONG sWrapMode;               // Wrap modes
    ULONG tWrapMode;

    ULONG minFilter;               // Min/mag filters
    ULONG magFilter;

    MCDCOLOR borderColor;          // Border color

    ULONG name;                    // "name" of texture object
    MCDFLOAT priority;             // priority of the texture object

    ULONG textureDimension;        // 1D or 2D texture

    ULONG paletteSize;             // Texture palette information
    RGBQUAD *paletteData;
    ULONG paletteBaseFormat;
    ULONG paletteRequestedFormat;

    BOOL  bMipMaps;                // Auxiliary flag to easily check that we are dealing with
                                   //   a mipmapped texture
#if TEX_NON_SQR
    MCDFLOAT uFactor;              // Auxiliary multiplying factors when texture is non-sqr
    MCDFLOAT vFactor;
    LONG     maxDim;               // Maximum dimension of the texture, either its height or width
    BOOL     bSqrTex;              // Flag indicating if texture is square(w==h) or not
#endif

#if TEXALPHA_FUNC
    BOOL     bAlpha01;             // Flag to check that all texture alpha values are 0.0 or 1.0
#endif

    ULONG totLevels;               // Total of transferred levels;
    ULONG validState;              // Flag indicating that texture has already been validated
    struct _DEVTEXTURE *pNextDevTexture;   // Pointer to next texture that is being handled, notice
                                   //    that it doesn't have to belong to the same pRc or the
                                   //    same pDevWnd!
} DEVTEXTURE;

#define DEV_TEXTURE_NOT_VALIDATED   0x010101
#define DEV_TEXTURE_INVALID         0x020202
#define DEV_TEXTURE_VALID           0x030303

// External declarations

MCDCOMMAND * FASTCALL __MCDPrimDrawPoints(DEVRC *pRc, MCDCOMMAND *pCmd);
MCDCOMMAND * FASTCALL __MCDPrimDrawLines(DEVRC *pRc, MCDCOMMAND *pCmd);
MCDCOMMAND * FASTCALL __MCDPrimDrawLineLoop(DEVRC *pRc, MCDCOMMAND *pCmd);
MCDCOMMAND * FASTCALL __MCDPrimDrawLineStrip(DEVRC *pRc, MCDCOMMAND *pCmd);
MCDCOMMAND * FASTCALL __MCDPrimDrawTriangles(DEVRC *pRc, MCDCOMMAND *pCmd);
MCDCOMMAND * FASTCALL __MCDPrimDrawTriangleStrip(DEVRC *pRc, MCDCOMMAND *pCmd);
MCDCOMMAND * FASTCALL __MCDPrimDrawTriangleFan(DEVRC *pRc, MCDCOMMAND *pCmd);
MCDCOMMAND * FASTCALL __MCDPrimDrawQuads(DEVRC *pRc, MCDCOMMAND *pCmd);
MCDCOMMAND * FASTCALL __MCDPrimDrawQuadStrip(DEVRC *pRc, MCDCOMMAND *pCmd);
MCDCOMMAND * FASTCALL __MCDPrimDrawPolygon(DEVRC *pRc, MCDCOMMAND *_pCmd);

// High-level rendering functions:

VOID __MCDPickRenderingFuncs(DEVRC *pRc, DEVWND *pDevWnd);

VOID FASTCALL __MCDRenderGenericPoint(DEVRC *pRc, MCDVERTEX *pv);
VOID FASTCALL __MCDRenderFogPoint(DEVRC *pRc, MCDVERTEX *pv);

VOID FASTCALL __MCDRenderFlatLine(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, BOOL resetLine);
VOID FASTCALL __MCDRenderFlatFogLine(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, BOOL resetLine);
VOID FASTCALL __MCDRenderSmoothLine(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, BOOL resetLine);

VOID FASTCALL __MCDRenderFlatTriangle(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, MCDVERTEX *pv3);
VOID FASTCALL __MCDRenderFlatFogTriangle(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, MCDVERTEX *pv3);
VOID FASTCALL __MCDRenderSmoothTriangle(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, MCDVERTEX *pv3);
VOID FASTCALL __MCDRenderGenTriangle(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2, MCDVERTEX *pv3);

// Low-level drawing functions:

VOID FASTCALL __MCDPointBegin(DEVRC *pRc);
VOID FASTCALL __MCDLineBegin(DEVRC *pRc);
VOID FASTCALL __MCDLineEnd(DEVRC *pRc);

VOID FASTCALL __MCDFillTriangle(DEVRC *pRc, MCDVERTEX *pv1, MCDVERTEX *pv2,
                                MCDVERTEX *pv3, BOOL bCCW);

// Clipping functions:

VOID FASTCALL __MCDPickClipFuncs(DEVRC *pRc);
VOID FASTCALL __MCDClipLine(DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                            BOOL bResetLine);
VOID FASTCALL __MCDClipTriangle(DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                                MCDVERTEX *c, ULONG orClipCode);
VOID FASTCALL __MCDClipPolygon(DEVRC *pRc, MCDVERTEX *v0, ULONG nv);
VOID FASTCALL __MCDDoClippedPolygon(DEVRC *pRc, MCDVERTEX **iv, ULONG nout,
                                    ULONG allClipCodes);


VOID FASTCALL __MCDCalcDeltaProps(DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                                 MCDVERTEX *c);
MCDFLOAT FASTCALL __MCDGetZOffsetDelta(DEVRC *pRc);

// Fog function

VOID __MCDCalcFogColor(DEVRC *pRc, MCDVERTEX *a, MCDCOLOR *pResult, MCDCOLOR *pColor);


//
// MMIO or DMA worker routines for __MCDFillSubTriangle.
//
VOID FASTCALL __HWSetupDeltas(DEVRC *pRc);
VOID FASTCALL __HWSetupDeltasDMA(DEVRC *pRc);

MCDFLOAT FASTCALL __AdjustVertexAllProps(DEVRC *pRc,MCDVERTEX *p, LONG iyVal, MCDFLOAT dxdy_slope);
MCDFLOAT FASTCALL __AdjustVertexAllPropsDMA(DEVRC *pRc,MCDVERTEX *p, LONG iyVal, MCDFLOAT dxdy_slope);

VOID FASTCALL __HWDrawCommand(DEVRC *pRc, LONG y_start, LONG ylen01, LONG ylen12, BOOL bCCW);
VOID FASTCALL __HWDrawCommandDMA(DEVRC *pRc, LONG y_start, LONG ylen01, LONG ylen12, BOOL bCCW);

VOID FASTCALL __MCDFillSubTriangle(DEVRC *pRc, MCDVERTEX *c, MCDVERTEX *b, MCDVERTEX *a, BOOL bCCW, BOOL bTrust);
VOID FASTCALL __MCDFillSubTriangleDMA(DEVRC *pRc, MCDVERTEX *c, MCDVERTEX *b, MCDVERTEX *a, BOOL bCCW, BOOL bTrust);


//
// MMIO or DMA worker routines for hardware 3D line or triangle clipping.
//
VOID FASTCALL HWLineSetupClipping(DEVRC *pRc, RECTL *pClip);
VOID FASTCALL HWLineSetupClippingDMA(DEVRC *pRc, RECTL *pClip);

VOID FASTCALL HWTriangleSetupClipping(DEVRC *pRc, RECTL *pClip);
VOID FASTCALL HWTriangleSetupClippingDMA(DEVRC *pRc, RECTL *pClip);


//
// MMIO or DMA worker routines for 3D lines.
//
VOID FASTCALL __MCDRenderHWLine(VOID *pRc, MCDVERTEX *a, MCDVERTEX *b, BOOL resetLine);
VOID FASTCALL __MCDRenderHWLineDMA(VOID *pRc, MCDVERTEX *a, MCDVERTEX *b, BOOL resetLine);


//
// Verification that vertex data is reasonable within the screen
// in order not to hang the 3D accelerator
//
#define MCD_VTX_VAL_LIMIT (65536.0)

#define CHECK_MCD_VERTEX_VALUE(pMCDVertex)                           \
    if ((pMCDVertex->windowCoord.x >  MCD_VTX_VAL_LIMIT) ||          \
        (pMCDVertex->windowCoord.y >  MCD_VTX_VAL_LIMIT) ||          \
        (pMCDVertex->windowCoord.x < -MCD_VTX_VAL_LIMIT) ||          \
        (pMCDVertex->windowCoord.y < -MCD_VTX_VAL_LIMIT)) {          \
        MCDBG_PRINT(DBG_VERT,"Vertex exceeded limits: "              \
                "(%li,%li)",                                         \
                 (LONG)pMCDVertex->windowCoord.x,                    \
                 (LONG)pMCDVertex->windowCoord.y);                   \
        /* abort rendering operation */                              \
        return;                                                      \
    }

