//***************************************************************************
//
//  debug.CPP
//
//  Module: CDM Provider
//
//  Purpose: Debugging routines
//
//  Copyright (c) 2000 Microsoft Corporation
//
//***************************************************************************

#include <windows.h>
#include <stdio.h>

#include "debug.h"


void __cdecl DebugOut(char *Format, ...)
{
    char Buffer[1024];
    va_list pArg;
    ULONG i;

    va_start(pArg, Format);
    i = _vsnprintf(Buffer, sizeof(Buffer), Format, pArg);
    OutputDebugString(Buffer);
}



