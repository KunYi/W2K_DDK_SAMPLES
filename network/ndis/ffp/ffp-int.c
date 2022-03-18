/*++

Copyright (c) 1999  Microsoft Corporation

Module Name:

  ffp-int.c

Abstract:

  Contains code for implementing Fast Forwarding Path

--*/

#include <precomp.h>

#if FFP_SUPPORT

#include "ffp-glb.h"

//
// FFP Public Functions
//

ULONG
FASTCALL
FFPStartup (
    VOID
    )

/*++
  Routine Description

    Called to initialize the FFP state in the driver

  Arguments

     None
    
  Return Value

     NDIS_STATUS_SUCCESS or NDIS_(error_status) 

--*/

{
    // Allocate a lock to protect FFP resources in the future
    ALLOCATE_FFP_LOCK();

    return NDIS_STATUS_SUCCESS;
}

VOID
FASTCALL
FFPShutdown (
    VOID
    )

/*++
  Routine Description

    Called to cleanup the FFP state in the driver.

  Arguments

    None
    
  Return Value

    None

--*/

{
    // First disable all FFP functionality in the driver
    FFPEnabled = 0;

    //
    // BUGBUG: ** FFP Code might still be active here **
    //

    FFP_DELAY();

    ACQUIRE_FFP_LOCK_EXCLUSIVE();

    // Deallocate all FFP resources including all caches
    FFPDestroyCaches();

    RELEASE_FFP_LOCK_EXCLUSIVE();

    // Deallocate the spin lock guarding the FFP state 
    FREE_FFP_LOCK();
}

VOID
FASTCALL
FFPSetControlFlags (
    IN               ULONG                ControlFlagsDesc, 
    IN               ULONG                ControlFlagsValue
    )

/*++
  Routine Description

    Called to control all aspects of forwarding like filtering...
      
  Arguments

    ControlFlagsDesc  : DWord that describes the flags that we are setting
    ControlFlagsValue : DWord that gives the 0/1 values for the above flags
      
  Return Value

    None

--*/

{
    ULONG  ffpwasenabled;

    //
    // Assume that caller flushes FFP state, if
    // significant time elapses (causing the
    // FFP state to probably go stale) before
    // FFP is re-enabled after disabling it
    //

    if (ControlFlagsDesc & CF_FFENABLED)
    {
        if (ControlFlagsValue & CF_FFENABLED)
        {
            FFPEnabled = 1;
            FFPDbgPrint(("FFP Forwarding Enabled\n"));
        }
        else
        {
            FFPEnabled = 0;
            FFPDbgPrint(("FFP Forwarding Disabled\n"));
        }
    }

    //
    //@L attn - asynchronous to send and rcv pkts
    //

    if (ControlFlagsDesc & CF_FILTERING)
    {
        if (ControlFlagsValue & CF_FILTERING)
        {
            if (CurrFFPFuncs != &FFPFilterFuncs)
            {
                ACQUIRE_FFP_LOCK_EXCLUSIVE();

                // First disable all FFP functionality in the driver

                ffpwasenabled = FFPEnabled;
                FFPEnabled = 0;

                //
                // BUGBUG: ** FFP Code might still be active here **
                //

                FFP_DELAY();

                //
                // Flush caches and turning filtering on
                //

                FlushCaches();

                CurrFFPFuncs = &FFPFilterFuncs;

                // Re-enable FFP in the driver to pickup changes
                FFPEnabled = ffpwasenabled;

                RELEASE_FFP_LOCK_EXCLUSIVE();
            }

                
            FFPDbgPrint(("FFP Filtering Enabled\n"));
        }
        else
        {
            if (CurrFFPFuncs != &FFPNofiltFuncs)
            {   
                ACQUIRE_FFP_LOCK_EXCLUSIVE();
                
                // First disable all FFP functionality in the driver
                ffpwasenabled = FFPEnabled;
                FFPEnabled = 0;

                //
                // BUGBUG: ** FFP Code might still be active here **
                //

                FFP_DELAY();

                //
                // Flush caches and turning filtering off
                //

                FlushCaches();

                CurrFFPFuncs = &FFPNofiltFuncs;

                // Re-enable FFP in the driver to pickup changes
                FFPEnabled = ffpwasenabled;

                RELEASE_FFP_LOCK_EXCLUSIVE();
            }

            FFPDbgPrint(("FFP Filtering Disabled\n"));
        }
    }
}

ULONG 
FASTCALL 
FFPGetControlFlags (
    VOID
    )

/*++
  Routine Description

    Return the current control state 
    (forwarding enabled/not, 
     filtering enabled/not)
      
  Arguments
     None
    
  Return Value
     The double word that gives the 0/1 values of the control flags
--*/

{
    ULONG    controlflags = 0;

    ACQUIRE_FFP_LOCK_SHARED();
    
    // Forwarding enabled on rcv, snd paths ?
    if (FFPEnabled)
    {
        controlflags |= CF_FFENABLED;
    }

    // Are we doing forwarding with filtering ?
    if (CurrFFPFuncs == &FFPFilterFuncs)
    {
        controlflags |= CF_FILTERING;
    }

    RELEASE_FFP_LOCK_SHARED();
    
    return controlflags;
}

ULONG
FASTCALL
FFPSetParameters (
    IN               ULONG                FFCacheSize
    )

/*++
  Routine Description

      Sets the parameters (cache sizes and all) that control the FFP code

  Arguments

    fastforwardingcachesize = size (in bytes) taken up by FFP caches
    
  Return Value

      None
--*/
{
    ULONG   fastforwardingcachesize;
    ULONG   incomingcachesize;
    ULONG   fragmentcachesize;
    ULONG   ffpwasenabled;
    ULONG   status;

    ACQUIRE_FFP_LOCK_EXCLUSIVE();

    // Store state of FFP hooks on recv & send paths
    ffpwasenabled = FFPEnabled;

    // Disable FFP hooks on the recv & send paths
    FFPEnabled = 0;

    //
    // BUGBUG: ** FFP Code might still be active here **
    //

    FFP_DELAY();

    // Get cache sizes that fit the given num of bytes
    incomingcachesize = DEFAULT_INCOMING_CACHE_ENTRIES;
    fragmentcachesize = DEFAULT_FRAGMENT_CACHE_ENTRIES;
    fastforwardingcachesize = DEFAULT_FFCACHE_SIZE;
    
    status = FFPReInitializeCaches(fastforwardingcachesize,
                                   incomingcachesize,
                                   fragmentcachesize);

    // Restore the hooks on the recv & send paths
    FFPEnabled = ffpwasenabled;

    RELEASE_FFP_LOCK_EXCLUSIVE();

    return status;
}

VOID
FASTCALL
FFPGetParameters (
    OUT              ULONG                *FFCachesSize
    )
/*++

  Routine Description

    Fills in the parameters (cache sizes and all) that control the FFP code
      
  Arguments

    FFCachesSize = size (in bytes) taken up by FFP caches

  Return Value

    None
--*/
{
    ACQUIRE_FFP_LOCK_SHARED();
    
    *FFCachesSize = 
        FastForwardingCacheSize 
                    * (sizeof(FFPCacheEntry) + sizeof(UCHAR)) 
            + 
        (IncomingCacheSize + FragmentCacheSize) 
                    * (sizeof(IPHeaderInfo) + 2*sizeof(UCHAR));

    RELEASE_FFP_LOCK_SHARED();
}

VOID
FASTCALL
FFPFlushCaches (
    VOID
    )

/*++
  Routine Description

      Called to flush state from all FFP caches
      
  Arguments

      None

  Return Value

      None

--*/

{
    //
    // Take an exclusive lock to prevent the caches
    // from getting deleted while you are 0ing them
    //

    ACQUIRE_FFP_LOCK_EXCLUSIVE();

    FlushCaches();

    RELEASE_FFP_LOCK_EXCLUSIVE();
}

//
// Helper Functions
//

static
ULONG
FASTCALL
FFPReInitializeCaches (
    IN         ULONG                fastforwardingcachesize OPTIONAL,
    IN         ULONG                incomingcachesize       OPTIONAL,
    IN         ULONG                fragmentcachesize       OPTIONAL
    )
/*++
  Routine Description

      Called to re-initialize the cache structures.

  Arguments

      fastforwardingcachesize: Driver suggested ffp cache size. 0 for default.
      incomingcachesize: Driver suggested incoming cache size.  0 for default.
      fragmentcachesize: Driver suggested fragment cache size.  0 for default.

  Return Value

      NDIS_STATUS_SUCCESS or NDIS_(error_status) 

--*/
{
    ULONG templong1;
    ULONG templong2;
    UCHAR tempcount;
    ULONG length;
    ULONG newffhashshift;
    ULONG newffhashmask;
    ULONG newincomingcachesize;
    ULONG newfragmentcachesize;
    ULONG newfastforwardingcachesize;
    UINT  i;
    NDIS_STATUS status;
    NDIS_PHYSICAL_ADDRESS phyaddr;

    // First calculate values of all cache sizes (from user suggested values)

    // For Incoming Cache
    newincomingcachesize = incomingcachesize ? incomingcachesize 
                                             : DEFAULT_INCOMING_CACHE_ENTRIES;

    // For Fragment Cache
    newfragmentcachesize = fragmentcachesize ? fragmentcachesize 
                                             : DEFAULT_FRAGMENT_CACHE_ENTRIES;

    // For Fast Forwarding Cache

    //
    // Allow FF cache size only in a certain range - between 2^8 and 2^16,
    // all FF cache size, cache value computations based on this assumption
    //

    if ( fastforwardingcachesize == 0 )            // default = 2^10
    {
        newfastforwardingcachesize = DEFAULT_FFCACHE_SIZE;
        newffhashshift = DEF_FFC_COMPUTED_SHIFT;
    }
    else
    if ( fastforwardingcachesize < MIN_FFCACHE_SIZE )    // = 2^8
    {
        newfastforwardingcachesize = MIN_FFCACHE_SIZE;
        newffhashshift = MIN_FFC_COMPUTED_SHIFT;
    }
    else
    if ( fastforwardingcachesize > MAX_FFCACHE_SIZE )    // = 2^16
    {
        newfastforwardingcachesize = MAX_FFCACHE_SIZE;
        newffhashshift = MAX_FFC_COMPUTED_SHIFT;
    }
    else
    {
        //
        // Increase the forwarding cache size to a power of 2
        // It enables faster cache computation (without a mod)
        //

        templong1 = fastforwardingcachesize >> 8;
        tempcount = 7;
        while (templong1)
        {
            templong1 >>= 1;
            tempcount++;
        }

        newfastforwardingcachesize = 1 << tempcount;
        if (newfastforwardingcachesize < fastforwardingcachesize)
        {
            newfastforwardingcachesize <<= 1;
            tempcount++;
        }
            
        newffhashshift = 16 - tempcount;
    }
    
    newffhashmask = newfastforwardingcachesize - 1;

    FFPDbgPrint(("\nSuggested FF Cache Size: %lu\n"              \
                 "\tActual Size: \t%lu\n\tActual Shift: \t%lu\n" \
                 "\tActual Mask: \t%lu\n\n",
                 fastforwardingcachesize,
                 newfastforwardingcachesize,
                 newffhashshift,
                 newffhashmask));

    //
    // Initialize max phy address struct used in allocate memory calls
    //

    NdisSetPhysicalAddressHigh (phyaddr, 0);
    NdisSetPhysicalAddressLow (phyaddr, 0xffffffff);

    //
    // Allocate (if cache size changed) and Zero FFP cache memory
    //

    do
    {
        if (FastForwardingCacheSize != newfastforwardingcachesize)
        {
            if (FastForwardingCacheSize)
            {
                FFPDbgPrint(("Free Mem: FFCache @ %08X, Size = %lu\n", 
                             FastForwardingCache, 
                             FastForwardingCacheSize));

                // Free the spinlock guarding each entry (dead code)
                for (i = 0; i < FastForwardingCacheSize; i++)
                {
                    FREE_FORWARDING_ENTRY_SPINLOCK(&FastForwardingCache[i]);
                }

                NdisFreeMemory(FastForwardingCache, 
                               FastForwardingCacheSize * sizeof(FFPCacheEntry),
                               0);

                NdisFreeMemory(FastForwardingCacheValidBmp, 
                               FastForwardingCacheSize * sizeof(CHAR), 
                               0);
            }

            FastForwardingCacheSize = newfastforwardingcachesize;
            FFHashMask = newffhashmask;
            FFHashShift = newffhashshift;

            status = 
             NdisAllocateMemory(&FastForwardingCache, 
                                FastForwardingCacheSize*sizeof(FFPCacheEntry), 
                                0, 
                                phyaddr);

            if (!NT_SUCCESS (status))
            {
                break;
            }

            status = 
             NdisAllocateMemory(&FastForwardingCacheValidBmp, 
                                FastForwardingCacheSize * sizeof(CHAR), 
                                0, 
                                phyaddr);

            if (!NT_SUCCESS (status))
            {
                break;
            }

            FFPDbgPrint(("Allocated Mem: FFCache @ %08X, Size = %lu\n", 
                         FastForwardingCache, 
                         FastForwardingCacheSize));
        }

#if 0
        NdisZeroMemory ((VOID *)FastForwardingCache, 
                        FastForwardingCacheSize * sizeof(FFPCacheEntry));
#endif

        // Initialize the spinlock guarding each entry
        for (i = 0; i < FastForwardingCacheSize; i++)
        {
            ALLOCATE_FORWARDING_ENTRY_SPINLOCK(&FastForwardingCache[i]);
        }

        NdisZeroMemory ((VOID *)FastForwardingCacheValidBmp, 
                        FastForwardingCacheSize * sizeof(CHAR));
        
        //
        // Allocate (if cache size changed) and Zero Incoming cache memory
        //

        if (IncomingCacheSize != newincomingcachesize)
        {
            if (IncomingCacheSize)
            {
                FFPDbgPrint(("Free Mem: IncCache @ %08X, Size = %lu\n", 
                             IncomingCache, 
                             IncomingCacheSize));

                // Free the spinlock guarding each entry (dead code)
                for (i = 0; i < IncomingCacheSize; i++)
                {
                    FREE_INCOMING_ENTRY_SPINLOCK(&IncomingCache[i]);
                }

                NdisFreeMemory(IncomingCache, 
                               IncomingCacheSize * sizeof(IPHeaderInfo), 0);

                NdisFreeMemory(IncomingCacheValidBmp, 
                               IncomingCacheSize * sizeof(CHAR), 0);
            }

            IncomingCacheSize = newincomingcachesize;
            
            status = 
             NdisAllocateMemory(&IncomingCache, 
                                IncomingCacheSize * sizeof(IPHeaderInfo), 
                                0, 
                                phyaddr);

            if (!NT_SUCCESS (status))
            {
                break;
            }

            status = 
             NdisAllocateMemory(&IncomingCacheValidBmp,
                                IncomingCacheSize * sizeof(CHAR), 
                                0, 
                                phyaddr);

            if (!NT_SUCCESS (status))
            {
                break;
            }

            FFPDbgPrint(("Allocated Mem: IncCache @ %08X, Size = %lu\n", 
                         IncomingCache, 
                         IncomingCacheSize));
        }
#if 0
        NdisZeroMemory((VOID *)IncomingCache, 
                       IncomingCacheSize * sizeof(IPHeaderInfo));
#endif

        // Initialize the spinlock guarding each entry
        for (i = 0; i < IncomingCacheSize; i++)
        {
            ALLOCATE_INCOMING_ENTRY_SPINLOCK(&IncomingCache[i]);
        }

        NdisZeroMemory((VOID *)IncomingCacheValidBmp, 
                       IncomingCacheSize * sizeof(CHAR));

        // Allocate (if cache size changed) and Zero Fragment cache memory

        if (FragmentCacheSize != newfragmentcachesize)
        {
            if (FragmentCacheSize)
            {
                FFPDbgPrint(("Free Mem: FrgCache @ %08X, Size = %lu\n", 
                             FragmentCache, 
                             FragmentCacheSize));

                // Free the spinlock guarding each entry (dead code)
                for (i = 0; i < FragmentCacheSize; i++)
                {
                    FREE_FRAGMENT_ENTRY_SPINLOCK(&FragmentCache[i]);
                }

                NdisFreeMemory(FragmentCache, 
                               FragmentCacheSize * sizeof(IPHeaderInfo), 0);

                NdisFreeMemory(FragmentCacheValidBmp, 
                               FragmentCacheSize * sizeof(CHAR), 0);
            }

            FragmentCacheSize = newfragmentcachesize;
            
            status = 
             NdisAllocateMemory(&FragmentCache, 
                                FragmentCacheSize * sizeof(IPHeaderInfo), 
                                0, 
                                phyaddr);

            if (!NT_SUCCESS (status))
            {
                break;
            }

            status = 
                NdisAllocateMemory(&FragmentCacheValidBmp, 
                                   FragmentCacheSize * sizeof(CHAR), 
                                   0, 
                                   phyaddr);

            if (!NT_SUCCESS (status))
            {
                break;
            }

            FFPDbgPrint(("Allocated Mem: FrgCache @ %08X, Size = %lu\n", 
                         FragmentCache, 
                         FragmentCacheSize));
        }
#if 0
        NdisZeroMemory((VOID *)FragmentCache, 
                       FragmentCacheSize * sizeof(IPHeaderInfo));
#endif

        // Initialize the spinlock guarding each entry
        for (i = 0; i < FragmentCacheSize; i++)
        {
            ALLOCATE_FRAGMENT_ENTRY_SPINLOCK(&FragmentCache[i]);
        }

        NdisZeroMemory((VOID *)FragmentCacheValidBmp, 
                       FragmentCacheSize * sizeof(CHAR));

        FFPDbgPrint(("Final Cache Sizes: \n\tFastFwdingCache: %lu\n"   \
                     "\tIncomingCache: %lu\n\tFragmentCache: %lu\n\n",
                     FastForwardingCacheSize,
                     IncomingCacheSize, 
                     FragmentCacheSize));

        return NDIS_STATUS_SUCCESS;
        
    } while (FALSE);

    // Clear out all caches as an error occurred
    FFPDestroyCaches();
    
    return status;
}

static
VOID
FASTCALL
FFPDestroyCaches (
    VOID
    )

/*++
  Routine Description

      Called to remove all/some of the caches
      
   Arguments

     None
    
  Return Value

     None

--*/

{
    UINT   i;

    if (FastForwardingCache)
    {
        // Free the spinlock guarding each entry (dead code)
        for (i = 0; i < FastForwardingCacheSize; i++)
        {
            FREE_FORWARDING_ENTRY_SPINLOCK(&FastForwardingCache[i]);
        }

        NdisFreeMemory(FastForwardingCache, 
                       FastForwardingCacheSize * sizeof(FFPCacheEntry), 0);
    }

    if (FastForwardingCacheValidBmp)
    {
        NdisFreeMemory(FastForwardingCacheValidBmp, 
                       FastForwardingCacheSize * sizeof(CHAR), 0);
    }
    
    FFHashMask = 0;
    FFHashShift = 0;
    FastForwardingCacheSize = 0;
    FastForwardingCache = NULL;
    FastForwardingCacheValidBmp = NULL;

    if (IncomingCache)
    {
        // Free the spinlock guarding each entry (dead code)
        for (i = 0; i < IncomingCacheSize; i++)
        {
            FREE_INCOMING_ENTRY_SPINLOCK(&IncomingCache[i]);
        }

        NdisFreeMemory(IncomingCache, 
                       IncomingCacheSize * sizeof(IPHeaderInfo), 0);
    }

    if (IncomingCacheValidBmp)
    {
        NdisFreeMemory(IncomingCacheValidBmp, 
                       IncomingCacheSize * sizeof(CHAR), 0);
    }

    IncomingCacheSize = 0;
    IncomingCache = NULL;
    IncomingCacheValidBmp = NULL;

    if (FragmentCache)
    {
        // Free the spinlock guarding each entry (dead code)
        for (i = 0; i < FragmentCacheSize; i++)
        {
            FREE_FRAGMENT_ENTRY_SPINLOCK(&FragmentCache[i]);
        }

        NdisFreeMemory(FragmentCache, 
                       FragmentCacheSize * sizeof(IPHeaderInfo), 0);
    }

    if (FragmentCacheValidBmp)
    {
        NdisFreeMemory(FragmentCacheValidBmp, 
                       FragmentCacheSize * sizeof(CHAR), 0);
    }
    
    FragmentCacheSize = 0;
    FragmentCache = NULL;
    FragmentCacheValidBmp = NULL;
}

//
// OID Set and Query Request Handlers
//

ULONG
FASTCALL
FFPHandleOidSetRequest (
    IN         NDIS_HANDLE          MiniportAdapter,
    IN         NDIS_OID             Oid,
    IN         PVOID                InformationBuffer,
    IN         ULONG                InformationBufferLength,
    OUT        PULONG               BytesRead,
    OUT        PULONG               BytesNeeded
    )
{
    NDIS_STATUS     Status;

    Status = NDIS_STATUS_SUCCESS;

    *BytesRead = *BytesNeeded = 0;

    switch (Oid)
    {
    case OID_FFP_SUPPORT:
       {
        FFPSupportParams *supportParams;
        
        FFPDbgPrintX(("Netflex3SetInformation:OID_FFP_SUPPORT:\n Param Length: %d\n",
                                                        InformationBufferLength));

        // Validate the incoming buffer size
        if (InformationBufferLength != sizeof(FFPSupportParams))
        {
           *BytesNeeded = sizeof(FFPSupportParams);
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }
        
        *BytesRead = sizeof(FFPSupportParams);

        supportParams = (FFPSupportParams *)InformationBuffer;

        if (supportParams->NdisProtocolType != NDIS_PROTOCOL_ID_TCP_IP)
        {
            Status = NDIS_STATUS_INVALID_DATA;
            break;
        }

        FFPDbgPrintX(("Start Params: FF Size (in Bytes): %lu, Flags = %08x\n",
                        supportParams->FastForwardingCacheSize,
                        supportParams->FFPControlFlags));

        // Set Cache Sizes
        FFPSetParameters(supportParams->FastForwardingCacheSize);

        // Set control state
        FFPSetControlFlags(CF_SELECTALL, supportParams->FFPControlFlags);
        
        break;
       }
       
    case OID_FFP_FLUSH:
       {
        FFPFlushParams *flushParams;
       
        FFPDbgPrintX(("Netflex3SetInformation:OID_GEN_FFPFLUSH:\n Param Length: %d\n",
                                                        InformationBufferLength));
        
        // Validate the incoming buffer size
        if (InformationBufferLength != sizeof(FFPFlushParams))
        {
           *BytesNeeded = sizeof(FFPFlushParams);
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        *BytesRead = sizeof(FFPFlushParams);

        flushParams = (FFPFlushParams *)InformationBuffer;

        if (flushParams->NdisProtocolType != NDIS_PROTOCOL_ID_TCP_IP)
        {
            Status = NDIS_STATUS_INVALID_DATA;
            break;
        }

        FFPDbgPrintX(("Flush FFC: No Params\n"));

        //    Flush the required caches
        FFPFlushCaches();
        break;
       }

    case OID_FFP_CONTROL:
       {
        FFPControlParams *controlParams;
        
        FFPDbgPrintX(("Netflex3SetInformation:OID_FFP_CONTROL:\n Param Length: %d\n",
                                                        InformationBufferLength));

        // Validate the incoming buffer size
        if (InformationBufferLength != sizeof(FFPControlParams))
        {
           *BytesNeeded = sizeof(FFPControlParams);
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        *BytesRead = sizeof(FFPControlParams);

        controlParams = (FFPControlParams *)InformationBuffer;

        if (controlParams->NdisProtocolType != NDIS_PROTOCOL_ID_TCP_IP)
        {
            Status = NDIS_STATUS_INVALID_DATA;
            break;
        }

        FFPDbgPrintX(("Control Flags: %lu\n", controlParams->FFPControlFlags));

        // Set control state
        FFPSetControlFlags(CF_SELECTALL, controlParams->FFPControlFlags);

        break;
       }
       
    case OID_FFP_PARAMS:
       {
        FFPCacheParams *cacheParams;

        FFPDbgPrintX(("Netflex3SetInformation:OID_FFP_PARAMS:\n Param Length: %d\n",
                                                        InformationBufferLength));

        // Validate the incoming buffer size
        if (InformationBufferLength != sizeof(FFPCacheParams))
        {
            *BytesNeeded = sizeof(FFPCacheParams);
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        *BytesRead = sizeof(FFPCacheParams);

        cacheParams = (FFPCacheParams *)InformationBuffer;

        if (cacheParams->NdisProtocolType != NDIS_PROTOCOL_ID_TCP_IP)
        {
            Status = NDIS_STATUS_INVALID_DATA;
            break;
        }

        FFPDbgPrintX(("Cache Size: %lu bytes\n", cacheParams->FastForwardingCacheSize));

        // Set cache sizes
        FFPSetParameters(cacheParams->FastForwardingCacheSize);

        break;
       }
       
    case OID_FFP_ADAPTER_STATS:
       {
        FFPAdapterStats *adpStats;

        FFPDbgPrintX(("Netflex3SetInformation:OID_FFP_ADAPTER_STATS:\n Param Length: %d\n",
                                                        InformationBufferLength));

        // Validate the incoming buffer size
        if (InformationBufferLength != sizeof(FFPAdapterStats))
        {
            *BytesNeeded = sizeof(FFPAdapterStats);
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        *BytesRead = sizeof(FFPAdapterStats);

        adpStats = (FFPAdapterStats *)InformationBuffer;

        if (adpStats->NdisProtocolType != NDIS_PROTOCOL_ID_TCP_IP)
        {
            Status = NDIS_STATUS_INVALID_DATA;
            break;
        }

        FFPDbgPrintX(("Set Value:   Stats:\n\tIncPktsFwd %08X, \n\tIncOctsFwd %08X,"
                                    "\n\tIncPktsDrp %08X, \n\tIncOctsDrp %08X,"
                                    "\n\tIncPktsPas %08X, \n\tIncOctsPas %08X,"
                                    "\n\tOutPktsFwd %08X, \n\tOutOctsFwd %08X\n",
                            adpStats->InPacketsForwarded,
                            adpStats->InOctetsForwarded,
                            adpStats->InPacketsDiscarded,
                            adpStats->InOctetsDiscarded,
                            adpStats->InPacketsIndicated,
                            adpStats->InOctetsIndicated,
                            adpStats->OutPacketsForwarded,
                            adpStats->OutOctetsForwarded));
                                
        // Reset the statistics
        FFPResetStatistics(FFP_ADAPTER_STATS(MiniportAdapter), adpStats);

        break;
       }
       
    case OID_FFP_DRIVER_STATS:
       {
        FFPAdapterStats  *adpStats;
        PVOID    vpCurrAdapter;

        FFPDbgPrintX(("Netflex3SetInformation:OID_FFP_DRIVER_STATS:\n Param Length: %d\n",
                                                        InformationBufferLength));

        // Validate the incoming buffer size
        if (InformationBufferLength != sizeof(FFPAdapterStats))
        {
            *BytesNeeded = sizeof(FFPAdapterStats);
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        *BytesRead = sizeof(FFPAdapterStats);

        adpStats = (FFPAdapterStats *)InformationBuffer;

        if (adpStats->NdisProtocolType != NDIS_PROTOCOL_ID_TCP_IP)
        {
            Status = NDIS_STATUS_INVALID_DATA;
            break;
        }

        FFPDbgPrintX(("Set Value:   Stats:\n\tIncPktsFwd %08X, \n\tIncOctsFwd %08X,"
                                    "\n\tIncPktsDrp %08X, \n\tIncOctsDrp %08X,"
                                    "\n\tIncPktsPas %08X, \n\tIncOctsPas %08X,"
                                    "\n\tOutPktsFwd %08X, \n\tOutOctsFwd %08X\n",
                            adpStats->InPacketsForwarded,
                            adpStats->InOctetsForwarded,
                            adpStats->InPacketsDiscarded,
                            adpStats->InOctetsDiscarded,
                            adpStats->InPacketsIndicated,
                            adpStats->InOctetsIndicated,
                            adpStats->OutPacketsForwarded,
                            adpStats->OutOctetsForwarded));
        
        for (vpCurrAdapter = GET_FIRST_ADAPTER(); 
                    ARE_ADAPTERS_LEFT(vpCurrAdapter); 
                        vpCurrAdapter = GET_NEXT_ADAPTER(vpCurrAdapter))
        {
            FFPResetStatistics(FFP_ADAPTER_STATS(vpCurrAdapter), adpStats);
        }
        
        break;
       }

    case OID_FFP_DATA:
       {
        FFPDataParams *seedParams;
        
        FFPDbgPrintX(("Netflex3SetInformation:OID_FFP_DATA:\n Param Length: %d\n",
                                                    InformationBufferLength));

        // Validate the incoming buffer size
        if (InformationBufferLength != sizeof(FFPDataParams))
        {
            *BytesNeeded = sizeof(FFPDataParams);
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        *BytesRead = sizeof(FFPDataParams);

        seedParams = (FFPDataParams *)InformationBuffer;

        if (seedParams->NdisProtocolType != NDIS_PROTOCOL_ID_TCP_IP)
        {
            Status = NDIS_STATUS_INVALID_DATA;
            break;
        }

        if (seedParams->HeaderSize != (sizeof(FFPIPHeader) + sizeof(ULONG)))
        {
            Status = NDIS_STATUS_INVALID_DATA;
            break;
        }

        FFPDbgPrintX(("CacheEntryType: %08X\n", seedParams->CacheEntryType));
        PrintIPPacketHeader(&seedParams->IpHeader.Header);
        FFPDbgPrintX(("DwordAfterIPHdr: %08X\n", seedParams->IpHeader.DwordAfterHeader));

        // Set cache entries to FFP_INDICATE_PACKET or DROPPED - map return status
        Status = FFPProcessSetInFFCache(&seedParams->IpHeader.Header, 
                                        seedParams->CacheEntryType);

        if (Status)
        {
            Status = NDIS_STATUS_SUCCESS;
        }
        else
        {
            Status = NDIS_STATUS_INVALID_DATA;
        }

        break;
       }

    default:
        {
         Status = NDIS_STATUS_NOT_SUPPORTED;
         break;
        }

    }

    return Status;
}

ULONG
FASTCALL
FFPHandleOidQueryRequest (
    IN         NDIS_HANDLE          MiniportAdapter,
    IN         NDIS_OID             Oid,
    IN         PVOID                InformationBuffer,
    IN         ULONG                InformationBufferLength,
    OUT        PULONG               BytesWritten,
    OUT        PULONG               BytesNeeded
    )
{
    NDIS_STATUS     Status;

    Status = NDIS_STATUS_SUCCESS;

    *BytesWritten = *BytesNeeded = 0;

    switch (Oid)
    {
    case OID_FFP_SUPPORT:
       {
        FFPVersionParams *versionParams;
        
        FFPDbgPrintX(("Netflex3QueryInformation:OID_FFP_SUPPORT:\n Param Length: %d\n",
                                                        InformationBufferLength));

        // Validate the incoming buffer size
        if (InformationBufferLength != sizeof(FFPVersionParams))
        {
           *BytesNeeded = sizeof(FFPVersionParams);
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        versionParams = (FFPVersionParams *)InformationBuffer;

        if (versionParams->NdisProtocolType != NDIS_PROTOCOL_ID_TCP_IP)
        {
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
        }

        *BytesWritten = sizeof(FFPVersionParams) - sizeof(ULONG);

        // Return current version of FFP
        versionParams->FFPVersion = FFP_MAJOR_VERSION << 16 + FFP_MINOR_VERSION;
        
        FFPDbgPrintX(("Query Value: FFP Version: %lu\n", 
                        versionParams->FFPVersion));
        break;
       }

    case OID_FFP_FLUSH:
       {
        FFPDbgPrintX(("Netflex3QueryInformation:OID_GEN_FFPFLUSH:\n Buffer Length: %d\n",
                                                        InformationBufferLength));

        Status = NDIS_STATUS_NOT_SUPPORTED;
        break;
       }

    case OID_FFP_CONTROL:
       {
        FFPControlParams *controlParams;
        
        FFPDbgPrintX(("Netflex3QueryInformation:OID_FFP_CONTROL:\n Param Length: %d\n",
                                                        InformationBufferLength));

        // Validate the incoming buffer size
        if (InformationBufferLength != sizeof(FFPControlParams))
        {
           *BytesNeeded = sizeof(FFPControlParams);
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        controlParams = (FFPControlParams *)InformationBuffer;

        if (controlParams->NdisProtocolType != NDIS_PROTOCOL_ID_TCP_IP)
        {
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
        }

        *BytesWritten = sizeof(FFPControlParams) - sizeof(ULONG);

        // Return FFP control flags
        controlParams->FFPControlFlags = FFPGetControlFlags();
        
        FFPDbgPrintX(("Query Value: Control Flags: %lu\n", 
                        controlParams->FFPControlFlags));
        break;
       }

    case OID_FFP_PARAMS:
       {
        FFPCacheParams *cacheParams;
        
        FFPDbgPrintX(("Netflex3QueryInformation:OID_FFP_PARAMS:\n Buffer Length: %d\n",
                                                        InformationBufferLength));

        // Validate the incoming buffer size
        if (InformationBufferLength != sizeof(FFPCacheParams))
        {
           *BytesNeeded = sizeof(FFPCacheParams);
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        cacheParams = (FFPCacheParams *)InformationBuffer;

        if (cacheParams->NdisProtocolType != NDIS_PROTOCOL_ID_TCP_IP)
        {
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
        }

        *BytesWritten = sizeof(FFPCacheParams) - sizeof(ULONG);

        FFPGetParameters(&cacheParams->FastForwardingCacheSize);
        
        FFPDbgPrintX(("Query Value: FF Size (in Bytes): %lu\n",
                        cacheParams->FastForwardingCacheSize));
        break;
       }
        
    case OID_FFP_ADAPTER_STATS:
       {
        FFPAdapterStats *adpStats;
        
        FFPDbgPrintX(("Netflex3QueryInformation:OID_FFP_ADAPTER_STATS:\n Buffer Length: %d\n",
                                                            InformationBufferLength));

        // Validate the incoming buffer size
        if (InformationBufferLength != sizeof(FFPAdapterStats))
        {
           *BytesNeeded = sizeof(FFPAdapterStats);
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        adpStats = (FFPAdapterStats *)InformationBuffer;

        if (adpStats->NdisProtocolType != NDIS_PROTOCOL_ID_TCP_IP)
        {
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
        }

        *BytesWritten = sizeof(FFPAdapterStats) - sizeof(ULONG);

        NdisMoveMemory((PUCHAR)adpStats + sizeof(ULONG), 
                       (PUCHAR)FFP_ADAPTER_STATS(MiniportAdapter) + sizeof(ULONG),
                       *BytesWritten);
        
        FFPDbgPrintX(("Query Value: Stats:\n\tIncPktsFwded %08X, \n\tIncOctsFwded %08X,"
                                    "\n\tIncPktsDropd %08X, \n\tIncOctsDropd %08X,"
                                    "\n\tIncPktsPasdUp %08X, \n\tIncOctsPasdUp %08X,"
                                    "\n\tOutPktsFwded %08X, \n\tOutOctsFwded %08X\n",
                        adpStats->InPacketsForwarded,
                        adpStats->InOctetsForwarded,
                        adpStats->InPacketsDiscarded,
                        adpStats->InOctetsDiscarded,
                        adpStats->InPacketsIndicated,
                        adpStats->InOctetsIndicated,
                        adpStats->OutPacketsForwarded,
                        adpStats->OutOctetsForwarded));
        break;
       }
       
    case OID_FFP_DRIVER_STATS:
       {
        PVOID    vpCurrAdapter;
        FFPAdapterStats *Src;
        FFPDriverStats *Dst;
        FFPDriverStats *glbStats;
        
        FFPDbgPrintX(("Netflex3QueryInformation:OID_FFP_DRIVER_STATS:\n Buffer Length: %d\n",
                                                            InformationBufferLength));

        // Validate the incoming buffer size
        if (InformationBufferLength != sizeof(FFPDriverStats))
        {
           *BytesNeeded = sizeof(FFPDriverStats);
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        glbStats = (FFPDriverStats *)InformationBuffer;

        if (glbStats->NdisProtocolType != NDIS_PROTOCOL_ID_TCP_IP)
        {
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
        }

        *BytesWritten = sizeof(FFPDriverStats) - sizeof(ULONG);

        NdisZeroMemory((PUCHAR)glbStats + sizeof(ULONG), *BytesWritten);

        for (vpCurrAdapter = GET_FIRST_ADAPTER(); 
                    ARE_ADAPTERS_LEFT(vpCurrAdapter); 
                        vpCurrAdapter = GET_NEXT_ADAPTER(vpCurrAdapter))
        {
            Src = FFP_ADAPTER_STATS(vpCurrAdapter);
            
            glbStats->PacketsForwarded     += Src->InPacketsForwarded;
            glbStats->OctetsForwarded     += Src->InOctetsForwarded;
            glbStats->PacketsDiscarded     += Src->InPacketsDiscarded;
            glbStats->OctetsDiscarded     += Src->InOctetsDiscarded;
            glbStats->PacketsIndicated     += Src->InPacketsIndicated;
            glbStats->OctetsIndicated     += Src->InOctetsIndicated;
        }

        FFPDbgPrintX(("Query Value: Stats:\n\tTotIncPktsFwded %08X, \n\tTotIncOctsFwded %08X,"
                                    "\n\tTotIncPktsDropd %08X, \n\tTotIncOctsDropd %08X,"
                                    "\n\tTotIncPktsPasdUp %08X, \n\tTotIncOctsPasdUp %08X\n",
                        glbStats->PacketsForwarded,
                        glbStats->OctetsForwarded,
                        glbStats->PacketsDiscarded,
                        glbStats->OctetsDiscarded,
                        glbStats->PacketsIndicated,
                        glbStats->OctetsIndicated));
        
        break;
       }
       
    case OID_FFP_DATA:
       {
        FFPDataParams *seedParams;
        
        FFPDbgPrintX(("Netflex3QueryInformation:OID_FFP_DATA:\n Buffer Length: %d\n",
                                                    InformationBufferLength));

        // Validate the incoming buffer size
        if (InformationBufferLength != sizeof(FFPDataParams))
        {
            *BytesNeeded = sizeof(FFPDataParams);
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        seedParams = (FFPDataParams *)InformationBuffer;

        if (seedParams->NdisProtocolType != NDIS_PROTOCOL_ID_TCP_IP)
        {
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
        }

        *BytesWritten = sizeof(ULONG);

        // Query the nature of cache entry - FFP_INDICATE_PACKET or DROPPED or FFP_FORWARD_PACKET
        seedParams->CacheEntryType = FFPProcessQueryFFCache((FFPIPHeader *)InformationBuffer); 

        FFPDbgPrintX(("CacheEntryType: %08X\n", seedParams->CacheEntryType));
        PrintIPPacketHeader(&seedParams->IpHeader.Header);
        FFPDbgPrintX(("DwordAfterIPHdr: %08X\n", seedParams->IpHeader.DwordAfterHeader));
        
        break;
       }

    default:
       {
        Status = NDIS_STATUS_NOT_SUPPORTED;
        break;
       }
    }

    return Status;
}

//
// FFP Hook into Packet Send Handler 
//

VOID
FFPOnPacketSendPath(
    IN         NDIS_HANDLE          Adapter,
    IN         PNDIS_PACKET         Packet
    )
{
    //
    // Pick up vital information needed for FFP - if the
    // packet being sent out was previously recevied 
    //
    
    if (FFPEnabled)
    {
        UINT            PhysicalBufferCount, BufferCount;
        UINT            TotalPacketLength;
        PNDIS_BUFFER    SourceBuffer;

        NdisQueryPacket (Packet,
            &PhysicalBufferCount,
            &BufferCount,
            &SourceBuffer,
            &TotalPacketLength);

        // Check if the packet's size > SIZEOF(ENET_HEADER) 
        //                            + SIZEOF(IP_HEADER) 
        //                            + SIZEOF(TCP/IP_PORT_INFO)
                                    
        if (TotalPacketLength >= 38)
        {
            UCHAR *enetheader ;
            UCHAR *ipinfo ;
            ULONG size ;
            UCHAR ipinfobuf[50] ;
            UCHAR *tempptr ;
            ULONG totalread = 0 ;

            // First get the enetheader filled.

            NdisQueryBuffer (SourceBuffer, &tempptr, &size) ;

            if (size > 38)
                size = 38 ;

            NdisMoveMemory (ipinfobuf, tempptr, size) ;

            totalread = size ;

            if (totalread < 38)
            {
                NdisFlushBuffer (SourceBuffer, TRUE);
                NdisGetNextBuffer (SourceBuffer, &SourceBuffer);
                NdisQueryBuffer (SourceBuffer, &tempptr, &size) ;

                NdisMoveMemory (&ipinfobuf[totalread], tempptr, 
                    ((size+totalread) > 38 ? (38 - totalread) : size));

                totalread += size ;

                if (totalread < 38)
                {
                    NdisFlushBuffer (SourceBuffer, TRUE);
                    NdisGetNextBuffer (SourceBuffer, &SourceBuffer);
                    NdisQueryBuffer (SourceBuffer, &tempptr, &size) ;

                    NdisMoveMemory (&ipinfobuf[totalread], tempptr, 
                        ((size+totalread) > 38 ? (38 - totalread) : size)) ;
                }
            }

            // total read should be 38 here
            enetheader = &ipinfobuf[0] ;
            ipinfo     = &ipinfobuf[14] ;

            FFPProcessSentPacket ((UNALIGNED EnetHeader *)enetheader, 
                                    (UNALIGNED FFPIPHeader *)ipinfo, 
                                        (HANDLE)Adapter) ;
        } 
        else
        {
            ; // FFPDbgPrint(("FFP: packet smaller than 38 bytes total !!\n"));
        }
    }
}

#endif // if FFP_SUPPORT

