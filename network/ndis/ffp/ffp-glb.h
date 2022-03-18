/*++

Copyright (c) 1999  Microsoft Corporation

Module Name:

    ffp-glb.h

Abstract:

    Header included once in FFP project - All 
    Public Global Vars for the FFP project.

Revision History:

--*/

#ifndef __FFP_GLB_H
#define __FFP_GLB_H

#include "ffp-def.h"

//
// FFP Global Constants
//

// FFP Functions when filtering is ON
FFPFunctions   FFPFilterFuncs = {
                                  &FFPWithFiltering_FFPProcessReceivedPacket,
                                  &FFPWithFiltering_FFPProcessSentPacket,
                                  &FFPWithFiltering_FFPProcessSetInFFCache,
                                  &FFPWithFiltering_FFPProcessQueryFFCache
                                };

// FFP Functions when filtering is OFF
FFPFunctions   FFPNofiltFuncs = {
                                  &FFPNoFiltering_FFPProcessReceivedPacket,
                                  &FFPNoFiltering_FFPProcessSentPacket,
                                  &FFPNoFiltering_FFPProcessSetInFFCache,
                                  &FFPNoFiltering_FFPProcessQueryFFCache
                                };

// String representation for cache entry types
const char *CacheEntryTypes[3] =  { 
                                    "FFP_DISCARD_PACKET",
                                    "FFP_INDICATE_PACKET",
                                    "FFP_FORWARD_PACKET" 
                                  };

//
// Assumed all vars are aligned, so all
// reads and writes to them are atomic.
//

//
// FFP Control State and other statistics
//

// FFP Wrapper Function Descriptions
FFPFunctions  *CurrFFPFuncs     = NULL;

// FFP hooks enabled on snd, rcv paths ?
ULONG      FFPEnabled           = 0;

// Stats to trace the paths packets took
LONGLONG   FastForwardedCount   = 0;
LONGLONG   FastDroppedCount     = 0;
LONGLONG   FastPassedupCount    = 0;

// To ensure atomicity in FFP state changes
NDIS_SPIN_LOCK      FFPSpinLock;

//
// FFP Data Cache Sizes and Cache Memory
//

ULONG          FastForwardingCacheSize = 0;
FFPCacheEntry *FastForwardingCache = NULL;
UCHAR         *FastForwardingCacheValidBmp = NULL;

ULONG          IncomingCacheSize = 0;
IPHeaderInfo  *IncomingCache = NULL;
UCHAR         *IncomingCacheValidBmp = NULL;

ULONG          FragmentCacheSize = 0;
IPHeaderInfo  *FragmentCache = NULL;
UCHAR         *FragmentCacheValidBmp = NULL;

ULONG          FFHashShift = 0;
ULONG          FFHashMask = 0;

#endif // __FFP_GLB_H

