//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1998 - 1999
//
//  File:       pch.h
//
//--------------------------------------------------------------------------

#define WANT_WDM

// 4115 - named type definition in parentheses
// 4127 - conditional expression is constant
// 4201 - nonstandard extension used : nameless struct/union
// 4214 - nonstandard extension used : bit field types other than int
// 4514 - unreferenced inline function has been removed
#pragma warning( disable : 4115 4127 4201 4214 4514 )

#include "ntddk.h"

#define DVRH_USE_PARPORT_ECP_ADDR 1
#include "parallel.h"

#include <wmilib.h>
#include "wmidata.h"
#include "parport.h"
#include "parlog.h"
#include "funcdecl.h"
#include "debug.h"
#include "util.h"

