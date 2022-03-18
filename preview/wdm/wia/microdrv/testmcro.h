#ifndef TESTMCRO
#define TESTMCRO

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// #define WIN32_LEAN_AND_MEAN
#define INITGUID
#include <windows.h>

//
// Button GUIDS
//

DEFINE_GUID( guidScanButton, 0xa6c5a715, 0x8c6e, 0x11d2, 0x97, 0x7a, 0x0, 0x0, 0xf8, 0x7a, 0x92, 0x6f);

#undef INITGUID

#endif

