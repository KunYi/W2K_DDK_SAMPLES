/******************************Module*Header*******************************\
* Module Name: nodma.c
*
* Copyright (c) 1992-1997 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.h"

#if SUPPORT_MCD

#undef S3D_DMA

#include "hw3d.h"
#include "mcdhw.h"
#include "mcdmath.h"
#include "mcdutil.h"


#define IABS(x)   ((x)>0?(x):-(x))

#define MCDDIVEPS    ((MCDFLOAT)(1.0e-4))
#define MCDUVEPSILON ((MCDFLOAT)(1.0e-10))

//replaced with ppdev->uvMaxTexels to accomondate Virge DX, GX, MX and GX-2.
//#define UVMAXTEXELS  ((MCDFLOAT)(128.0))

#define MOVE_XDER_BACK 1  // Set to 1 to calculate x derivates before any triangle
                          // splitting, since it is invariant across the whole
                          // triangle. Unfortunately, the y derivates in this
                          // implementation for the S3 Virge require to be along
                          // the longest edge, so we cannot do the same for them.
                          // Also, perspective-corrected textured triangles
                          // require a particular derivate calculation so it is
                          // not used in such a case.

#include "s3dtri.c"
#include "s3dline.c"

#endif

