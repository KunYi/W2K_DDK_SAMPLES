//==========================================================================;
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
//  PURPOSE.
//
//  Copyright (c) 1992 - 1996  Microsoft Corporation.  All Rights Reserved.
//
//==========================================================================;


#ifndef __CAPDEBUG_H
#define __CAPDEBUG_H

#ifdef DBG

// Debug Logging
// 0 = Errors only
// 1 = Info, stream state changes, stream open close
// 2 = Verbose trace

extern ULONG gDebugLevel;

#define DbgLogError(x)  { if( gDebugLevel >= 0)	 KdPrint(x); }
#define DbgLogInfo(x)   { if( gDebugLevel >= 1)	 KdPrint(x); }
#define DbgLogTrace(x)  { if( gDebugLevel >= 2)  KdPrint(x); }

# define TRAP   KdBreakPoint();
# define DbgKdPrint(args)  KdPrint(args)
#else //_DEBUG
# define TRAP
# define DbgKdPrint(args)
#endif //_DEBUG

#endif // #ifndef __CAPDEBUG_H

