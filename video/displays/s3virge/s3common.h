//***************************************************************************
//
//  This file contains per version info.  See corresponding .h files for
//  IOCtls, and type definitions.  The #includes are at the end of the file.
//  Search on EACHRELEASE, for those fields that commonly must be tailored
//  for each individual release (version number, specific product flags, etc.)
//
//  NOTE:  The product version string that the NT Control Panel displays for
//         the display driver is not explicitly defined here.  For Win NT
//         version 4.0, it will display simply as "4.00".  However, the
//         version number for the miniport will be correct (see 
//         S3_INTERNAL_PRODUCTVERSION_STR, below).
//

#define VER_LEGALCOPYRIGHT_YEARS    "1997"
#define VER_COMPANYNAME_STR         "S3 Incorporated"
#define VER_LEGALTRADEMARKS_STR     "S3 is a registered trademark of S3 Incorporated."
#define VER_LEGALCOPYRIGHT_STR      "Copyright S3 Incorporated. " VER_LEGALCOPYRIGHT_YEARS

// ***************
// * EACHRELEASE *
// ***************
//
// THIS RELEASE IS:  NT5.0 bootleg release for generic customer, mobile products:
//                   ViRGE/MX, ViRGE/MXC, ViRGE/MX+.  Intent of release is simply to
//                   test merged codebase.  The version used is 3.29.01, but this build
//                   is not actually equivalent to 3.29.01 -- just loosely based on it.
//

#define S3_INTERNAL_PRODUCTVERSION_STR      "3.29.02"

//
// S3_MAJOR_VERSION and S3_MINOR_VERSION are used to define DRIVER_VERSION
// which is used in the .rc files.
//
// Please keep the S3_MINOR_VERSION to 4 decimal digits
//

// ***************
// * EACHRELEASE *
// ***************

#define S3_MAJOR_VERSION                    329
#define S3_MINOR_VERSION                    0002

#define DRIVER_VERSION                      S3_MAJOR_VERSION, S3_MINOR_VERSION

#undef  VER_PRODUCTVERSION_STR2
#undef  VER_PRODUCTVERSION_STR1
#undef  VER_DDK_PRODUCTVERSION
#define VER_STR3(x,y)                #x "." #y
#define VER_PRODUCTVERSION_STR2(x,y) VER_STR3 x "." VER_STR3 y
#define VER_PRODUCTVERSION_STR1(x,y) VER_PRODUCTVERSION_STR2(x,y)

#if (_WIN32_WINNT == 0x0400)

    #define VER_DDK_PRODUCTVERSION      4,1024

#elif (_WIN32_WINNT == 0x0500)

	#define VER_DDK_PRODUCTVERSION		5,1024

#endif
#define VER_PRODUCTVERSION          VER_DDK_PRODUCTVERSION,DRIVER_VERSION
#define VER_PRODUCTVERSION_STR      VER_PRODUCTVERSION_STR1( (VER_DDK_PRODUCTVERSION), (DRIVER_VERSION) )

// ***************
// * EACHRELEASE *
// ***************
//
//  PCIMMIO:  can be set TRUE only for certain products (e.g. ViRGE, but not ViRGE/GX2 or
//            ViRGE/MX).  Define will eventually be removed.
//
#define PCIMMIO FALSE



//
//      M5
//

// Workarounds or conditional features
#define M5_DISABLE_POWER_DOWN_CONTROL   TRUE
#define M5_TV_POSITION                  TRUE
#define M5_CRTPAN                       FALSE

//
//  M5 debugging options
//

#define M5_ALWAYS_DETECT_DDC            FALSE

//
//  temporary flag to signal if high color depths should be
//  pruned in DuoView configurations, even on generic BIOS.
//
#define M5_PRUNE_ON_GENERIC_BIOS        FALSE



//
// include s3 type definition and s3 ioctls
//

#include "s3def.h"
#include "s3ioctl.h"

