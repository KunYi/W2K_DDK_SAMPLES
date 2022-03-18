/*++

Copyright (c) 1999  Microsoft Corporation

Module Name:

  ffp-def.h

Abstract:

  Common definitions for ffp functionality.

Revision History:

--*/

#ifndef __FFP_DEF_H
#define __FFP_DEF_H

#include <ffp.h>

//
// Defines
//

// FFP Version Supported (1.0)
#define FFP_MAJOR_VERSION                  1
#define FFP_MINOR_VERSION                  0


// Protocol IDs copied from winsock2.h
#define IPPROTO_ICMP                       1
#define IPPROTO_TCP                        6
#define IPPROTO_UDP                       17

//
// Default cache sizes, hash values
//

#define DEFAULT_INCOMING_CACHE_ENTRIES  0xAB
#define DEFAULT_FRAGMENT_CACHE_ENTRIES  0xAB

#define DEFAULT_FFCACHE_SIZE         1 << 10
#define DEF_FFC_COMPUTED_SHIFT       16 - 10

#define MIN_FFCACHE_SIZE              1 << 8
#define MIN_FFC_COMPUTED_SHIFT        16 - 8

#define MAX_FFCACHE_SIZE             1 << 16
#define MAX_FFC_COMPUTED_SHIFT       16 - 16

//
// Packet Structure Definitions
//

#include <packon.h>

#define ETH_ADDR_LENGTH      6
//
// Ethernet Address struct
//
typedef struct _EnetAddress 
{
    UCHAR          EA_Addr[ETH_ADDR_LENGTH] ;
} EnetAddress;

//
// Ethernet Header format
//
typedef struct _EnetHeader 
{
    EnetAddress    EH_Dest;
    EnetAddress    EH_Src;
    USHORT         EH_Type;
} EnetHeader;

#include <packoff.h>

//
// Cache Structure Definitions
//

#include <packoff.h>

//
// Incoming and Fragment Cache Entry
//
typedef struct _IPHeaderInfo 
{
    NDIS_SPIN_LOCK IPHI_SpinLock;               // Spinlock guarding this entry
    HANDLE         IPHI_Adapter;                // Incoming adapter for flow
    ULONG          IPHI_SrcAddress;             // Src  IP address of flow
    ULONG          IPHI_DestAddress;            // Dest IP address of flow
    USHORT         IPHI_Protocol;               // Protocol of flow (TCP..)
    USHORT         IPHI_FragmentId;             // Fragment Id for last packet
    USHORT         IPHI_FirstWordAfterIPHeader; // Source port for TCP and UDP
                                                // and type and code for ICMP
    USHORT         IPHI_SecondWordAfterIPHeader;// Dest port for TCP and UDP
                                                // and not used at all for ICMP
} IPHeaderInfo;

//
// Locking Incoming/Fragment Cache Entry
//

#define ALLOCATE_INCOMING_ENTRY_SPINLOCK(cacheentry)  \
    NdisAllocateSpinLock(&((cacheentry))->IPHI_SpinLock)

#define ACQUIRE_INCOMING_ENTRY_SPINLOCK(cacheentry)   \
    NdisAcquireSpinLock(&((cacheentry))->IPHI_SpinLock)

#define RELEASE_INCOMING_ENTRY_SPINLOCK(cacheentry)   \
    NdisReleaseSpinLock(&((cacheentry))->IPHI_SpinLock)

#define FREE_INCOMING_ENTRY_SPINLOCK(cacheentry)      \
    NdisFreeSpinLock(&((cacheentry))->IPHI_SpinLock)


#define ALLOCATE_FRAGMENT_ENTRY_SPINLOCK(cacheentry)  \
    NdisAllocateSpinLock(&((cacheentry))->IPHI_SpinLock)

#define ACQUIRE_FRAGMENT_ENTRY_SPINLOCK(cacheentry)   \
    NdisAcquireSpinLock(&((cacheentry))->IPHI_SpinLock)

#define RELEASE_FRAGMENT_ENTRY_SPINLOCK(cacheentry)   \
    NdisReleaseSpinLock(&((cacheentry))->IPHI_SpinLock)

#define FREE_FRAGMENT_ENTRY_SPINLOCK(cacheentry)      \
    NdisFreeSpinLock(&((cacheentry))->IPHI_SpinLock)

//
// Fast Forwarding Cache Entry
//
typedef struct _FFPCacheEntry 
{
    IPHeaderInfo    FFPCE_IPInfo;              // Incoming Info for this flow
    HANDLE          FFPCE_OutgoingAdapter;     // Outgoing adapter for flow
    EnetHeader      FFPCE_MACHeader;           // MAC hdr set for outgoing pkt
    LONG            FFPCE_CacheEntryType;      // Set to FFP_DISCARD_PACKET,
                                               //     or FFP_INDICATE_PACKET,
                                               //     or FFP_FORWARD_PACKET
} FFPCacheEntry;

//
// Locking Fast Forwarding Cache Entry
//

#define ALLOCATE_FORWARDING_ENTRY_SPINLOCK(cacheentry)  \
    NdisAllocateSpinLock(&((cacheentry))->FFPCE_IPInfo.IPHI_SpinLock)

#define ACQUIRE_FORWARDING_ENTRY_SPINLOCK(cacheentry)  \
    NdisAcquireSpinLock(&((cacheentry))->FFPCE_IPInfo.IPHI_SpinLock)

#define RELEASE_FORWARDING_ENTRY_SPINLOCK(cacheentry)  \
    NdisReleaseSpinLock(&((cacheentry))->FFPCE_IPInfo.IPHI_SpinLock)

#define FREE_FORWARDING_ENTRY_SPINLOCK(cacheentry)     \
    NdisFreeSpinLock(&((cacheentry))->FFPCE_IPInfo.IPHI_SpinLock)

//
// Function Pointers to FFP functions
//
typedef struct _FFPFunctions 
{
    ULONG (FASTCALL *pFFPProcessReceivedPacket) 
                              (IN  UNALIGNED EnetHeader *EthernetHeader,
                               IN  UNALIGNED FFPIPHeader *PacketHeader,
                               IN  HANDLE  IncomingAdapter,
                               OUT HANDLE *OutgoingAdapter);
                                         
    VOID (FASTCALL *pFFPProcessSentPacket)    
                              (IN  UNALIGNED EnetHeader *EthernetHeader,
                               IN  UNALIGNED FFPIPHeader *PacketHeader,
                               IN  HANDLE OutgoingAdapter);

    ULONG (FASTCALL *pFFPProcessSetInFFCache)
                              (IN  UNALIGNED FFPIPHeader *PacketHeader, 
                               IN  ULONG CacheEntryType);

    ULONG (FASTCALL *pFFPProcessQueryFFCache)    
                              (IN  UNALIGNED FFPIPHeader *PacketHeader);
} FFPFunctions;

//
// Global Externs
//

// Locks
extern NDIS_SPIN_LOCK FFPSpinLock;

// Caches and Cache Sizes
extern ULONG          FastForwardingCacheSize;
extern FFPCacheEntry *FastForwardingCache;
extern UCHAR         *FastForwardingCacheValidBmp;

extern ULONG          IncomingCacheSize;
extern IPHeaderInfo  *IncomingCache;
extern UCHAR         *IncomingCacheValidBmp;

extern ULONG          FragmentCacheSize;
extern IPHeaderInfo  *FragmentCache;
extern UCHAR         *FragmentCacheValidBmp;

extern ULONG          FFHashShift;
extern ULONG          FFHashMask;

// Cache Type Names
extern const CHAR    *CacheEntryTypes[3];

// FFP Function Pointers
extern FFPFunctions   FFPFilterFuncs;
extern FFPFunctions   FFPNofiltFuncs;

// Controls
extern FFPFunctions  *CurrFFPFuncs;
extern ULONG          FFPEnabled;

// Statistics
extern LONGLONG       FastForwardedCount;
extern LONGLONG       FastDroppedCount;
extern LONGLONG       FastPassedupCount;


//
// Macros to acquire Global FFP Lock
//

#define ALLOCATE_FFP_LOCK()               NdisAllocateSpinLock(&FFPSpinLock)

#define FREE_FFP_LOCK()                   NdisFreeSpinLock(&FFPSpinLock)

#define ACQUIRE_FFP_LOCK_SHARED()         NdisAcquireSpinLock(&FFPSpinLock)
#define RELEASE_FFP_LOCK_SHARED()         NdisReleaseSpinLock(&FFPSpinLock)

#define ACQUIRE_FFP_LOCK_EXCLUSIVE()      NdisAcquireSpinLock(&FFPSpinLock)
#define RELEASE_FFP_LOCK_EXCLUSIVE()      NdisReleaseSpinLock(&FFPSpinLock)

//
// For extracting FFP Control bits
//

#define CF_SELECTALL   ~0        // All bits in the dword are marked (set to 1)
#define CF_ENABLEALL   ~0        // All bits in the dword are marked (set to 1)

//
// Used in printing cache types
//
#define CACHEENTRYTYPE(value)   CacheEntryTypes[(value) + 1]

//
// IP Packet Macros
//

#define TTL_PKT_EXPIRED(IpHeader) ((IpHeader)->Ttl <= 1)

#define IS_FRAGMENTED(IpHeader)   (((IpHeader)->Offset & 0xff3f) != 0x0000)
#define NOT_FRAGMENTED(IpHeader)  (((IpHeader)->Offset & 0xff3f) == 0x0000)
#define FIRST_FRAGMENT(IpHeader)  ((IpHeader)->Offset == 0x0020)

#define FIRST_DWORD_AFTER_IP_HEADER(IpHeader)          \
                                  (*((ULONG *)(((UCHAR *)(IpHeader)) + 20)))

//
// Hashing Macros
//

#define ROTATE_LEFT_VALUE(Value)  ((Value) << 1 + (Value) >> 31)
#define INIT_HASH_VALUE(IpHeader) ((IpHeader)->Src \
                                   + ROTATE_LEFT_VALUE((IpHeader)->Dest))

//
// Macros to check if a cache entry is valid,
// and validate or invalidate a cache entry.
//

#define INVALID_FORWARDING_CACHE_ENTRY(HashIndex)      \
    (FastForwardingCacheValidBmp[(HashIndex)] == 0)

#define SET_FORWARDING_CACHE_ENTRY_TO_VALID(HashIndex) \
    FastForwardingCacheValidBmp[(HashIndex)] = 1;

#define INVALID_INCOMING_CACHE_ENTRY(HashIndex)        \
    (IncomingCacheValidBmp[(HashIndex)] == 0)

#define SET_INCOMING_CACHE_ENTRY_TO_VALID(HashIndex)   \
    IncomingCacheValidBmp[(HashIndex)] = 1;

#define INVALID_FRAGMENT_CACHE_ENTRY(HashIndex)        \
    (FragmentCacheValidBmp[(HashIndex)] == 0)

#define SET_FRAGMENT_CACHE_ENTRY_TO_VALID(HashIndex)   \
    FragmentCacheValidBmp[(HashIndex)] = 1;

//
// Misc Macros
//

#define FFP_DELAY() do { int z; for (z = 0; z < 10000; z++); } while (0);

//
// Debug Macros
//

#if (FFP_SUPPORT && DBG)
#define FFPDbgPrint(many_args)    DbgPrint many_args
#else
#define FFPDbgPrint(many_args)
#endif

#define FFPDbgPrintX

//
// Prototypes
//

ULONG 
FASTCALL 
FFPStartup (
    VOID
    ); 

VOID  
FASTCALL 
FFPShutdown (
    VOID
    );
                                    
VOID 
FASTCALL 
FFPSetControlFlags (
    IN      ULONG                           ControlFlagsDesc,
    IN      ULONG                           ControlFlagsValue
    );
                                     
ULONG 
FASTCALL 
FFPGetControlFlags (
    VOID
    );
                                     
ULONG 
FASTCALL 
FFPSetParameters (
    IN      ULONG                           FastforwardingCacheSize
    );

VOID 
FASTCALL 
FFPGetParameters (
    OUT      ULONG                         *FastforwardingCacheSize
    );

VOID
FASTCALL 
FFPFlushCaches (
    VOID
    );

ULONG
FASTCALL
FFPReInitializeCaches (
    IN      ULONG                           FastforwardingCacheSize OPTIONAL, 
    IN      ULONG                           IncomingCacheSize       OPTIONAL, 
    IN      ULONG                           FragmentCacheSize       OPTIONAL
    );

VOID
FASTCALL
FFPDestroyCaches (
    VOID
    );

ULONG
FASTCALL
FFPHandleOidSetRequest (
    IN         HANDLE                       Adapter,
    IN         NDIS_OID                     Oid,
    IN         PVOID                        InformationBuffer,
    IN         ULONG                        InformationBufferLength,
    OUT        PULONG                       BytesRead,
    OUT        PULONG                       BytesNeeded
    );

ULONG
FASTCALL
FFPHandleOidQueryRequest (
    IN         HANDLE                       Adapter,
    IN         NDIS_OID                     Oid,
    IN         PVOID                        InformationBuffer,
    IN         ULONG                        InformationBufferLength,
    OUT        PULONG                       BytesWritten,
    OUT        PULONG                       BytesNeeded
    );

VOID
FFPOnPacketSendPath(
    IN         HANDLE                       IncomingAdapter,
    IN         PNDIS_PACKET                 Packet
    );

ULONG
FASTCALL
FFPWithFiltering_FFPProcessReceivedPacket (
    IN     UNALIGNED EnetHeader            *EthernetHeader, 
    IN     UNALIGNED FFPIPHeader           *PacketHeader, 
    IN     HANDLE                           IncomingAdapter, 
    OUT    HANDLE                          *OutgoingAdapter
    );

ULONG
FASTCALL
FFPNoFiltering_FFPProcessReceivedPacket (
    IN     UNALIGNED EnetHeader            *EthernetHeader, 
    IN     UNALIGNED FFPIPHeader           *PacketHeader, 
    IN     HANDLE                           IncomingAdapter, 
    OUT    HANDLE                          *OutgoingAdapter
    );

VOID
FASTCALL
FFPWithFiltering_FFPProcessSentPacket (
    IN     UNALIGNED EnetHeader            *EthernetHeader, 
    IN     UNALIGNED FFPIPHeader           *PacketHeader, 
    IN     HANDLE                           OutgoingAdapter
    );

VOID
FASTCALL
FFPNoFiltering_FFPProcessSentPacket (
    IN     UNALIGNED EnetHeader            *EthernetHeader, 
    IN     UNALIGNED FFPIPHeader           *PacketHeader, 
    IN     HANDLE                           OutgoingAdapter
    );

ULONG
FASTCALL
FFPWithFiltering_FFPProcessSetInFFCache (
    IN     UNALIGNED FFPIPHeader          *PacketHeader, 
    IN     ULONG                           CacheEntryType
    );

ULONG
FASTCALL
FFPNoFiltering_FFPProcessSetInFFCache (
    IN     UNALIGNED FFPIPHeader         *PacketHeader, 
    IN     ULONG                          CacheEntryType
    );

ULONG
FASTCALL
FFPWithFiltering_FFPProcessQueryFFCache (
    IN     UNALIGNED FFPIPHeader         *PacketHeader
    );

ULONG
FASTCALL
FFPNoFiltering_FFPProcessQueryFFCache (
    IN     UNALIGNED FFPIPHeader         *PacketHeader
    );

//
// FFP Wrapper Macros
//

/*++

VOID
FASTCALL
FlushCaches (
    VOID
    )

  Macro Description

      Called to flush state from all FFP caches
      
  Arguments

      None

  Return Value

      None

--*/

#define FlushCaches()                                                         \
    NdisZeroMemory(FastForwardingCacheValidBmp,                               \
                   FastForwardingCacheSize * sizeof(CHAR));                   \
                                                                              \
    NdisZeroMemory(IncomingCacheValidBmp,                                     \
                   IncomingCacheSize * sizeof(CHAR));                         \
                                                                              \
    NdisZeroMemory(FragmentCacheValidBmp,                                     \
                   FragmentCacheSize * sizeof(CHAR));                         \


/*++

VOID FFPResetStatistics (
    IN     FFPAdapterStats                 *AdapterStat,
    IN     FFPAdapterStats                 *InputBuffer
    )

  Macro Description

      Reset (Update) the statistics that are being recorded by the FFP code
      
  Arguments

    AdapterStat - Struct that is being reset to InputBuffer by the call
    InputBuffer - Struct that has the values being used in the reset
    
  Return Value

      None
--*/

#define FFPResetStatistics(_AdapterStats_, _InputBuffer_)                     \
        ACQUIRE_FFP_LOCK_EXCLUSIVE();                                         \
        NdisMoveMemory(_AdapterStats_,_InputBuffer_,sizeof(FFPAdapterStats)); \
        RELEASE_FFP_LOCK_EXCLUSIVE();                                         \

/*++

VOID FFPGetStatistics (
    OUT    FFPAdapterStats                 *OutputBuffer,
    IN     FFPAdapterStats                 *AdapterStats 
    )

  Macro Description

      Fills in the statistics that are being recorded by the FFP code
      
  Arguments

    OutputBuffer - Struct that is being filled by stats from AdapterStats
    AdapterStats - Struct that has the stats being retrieved by the call

  Return Value

      None
--*/

#define FFPGetStatistics(_OutputBuffer_, _AdapterStats_)                      \
        ACQUIRE_FFP_LOCK_SHARED();                                            \
        NdisMoveMemory(_OutputBuffer_,_AdapterStats_,sizeof(FFPAdapterStats));\
        RELEASE_FFP_LOCK_SHARED();                                            \


/*++

ULONG
FASTCALL
FFPProcessReceivedPacket (
    IN     UNALIGNED EnetHeader            *EthernetHeader, 
    IN     UNALIGNED FFPIPHeader           *PacketHeader, 
    IN     HANDLE                           IncomingAdapter, 
    OUT    HANDLE                          *OutgoingAdapter
    )

  Routine Description

    Dummy wrapper that calls the appropriate FFP receive handler
      
--*/

#define FFPProcessReceivedPacket CurrFFPFuncs->pFFPProcessReceivedPacket


/*++

VOID
FASTCALL
FFPProcessSentPacket (
    IN     UNALIGNED EnetHeader            *EthernetHeader, 
    IN     UNALIGNED FFPIPHeader           *PacketHeader, 
    IN     HANDLE                           OutgoingAdapter
    )

  Routine Description

    Dummy wrapper that calls the appropriate FFP handler 
    to capture the packet's send path information.
      
--*/

#define FFPProcessSentPacket     CurrFFPFuncs->pFFPProcessSentPacket


/*++

ULONG
FASTCALL
FFPProcessSetInFFCache (
    IN     UNALIGNED FFPIPHeader         *PacketHeader, 
    IN     ULONG                          CacheEntryType
    )
  Routine Description

    Dummy wrapper that calls the appropriate FFP handler
    to seed the fast forwarding cache.

--*/

#define FFPProcessSetInFFCache CurrFFPFuncs->pFFPProcessSetInFFCache


/*++

ULONG
FASTCALL
FFPProcessQueryFFCache (
    IN     UNALIGNED FFPIPHeader         *PacketHeader
    )

  Routine Description

    Dummy wrapper that calls the appropriate FFP handler
    to query the fast forwarding cache.
      
--*/

#define FFPProcessQueryFFCache CurrFFPFuncs->pFFPProcessQueryFFCache


/*++

VOID
FASTCALL
PrintIPPacketHeader (
    IN     UNALIGNED FFPIPHeader         *IpHeader
    )

  Routine Description
  
      Prints all the non options fields in an IP Header 
      (first 20 bytes), and the next 4 bytes [which are
      TCP / UDP src, dest ports when no IP options are 
      specified].

  Arguments
      ipheader - pointer to atleast 24 bytes of the header.

  Return Value
     None

--*/
#define    PrintIPPacketHeader(ipheader)                                      \
            FFPDbgPrint (("\n\nIPHeader\n\tVerlen:\t%02X \n"                  \
                          "\tTOS:\t%02X \n\tLength:\t%04X \n"                 \
                          "\tID:\t%04X \n\tOffset:\t%04X \n"                  \
                          "\tTTL:\t%02X \n\tProto:\t%02X \n"                  \
                          "\tSrc:\t%08X \n\tDest:\t%08X \n"                   \
                          "\tDWord:\t%08X \n\n",                              \
                          (ipheader)->Verlen,                             \
                          (ipheader)->Tos,                                \
                          (ipheader)->Length,                             \
                          (ipheader)->Id,                                 \
                          (ipheader)->Offset,                             \
                          (ipheader)->Ttl,                                \
                          (ipheader)->Protocol,                           \
                          (ipheader)->Src,                                \
                          (ipheader)->Dest,                               \
                          *((ULONG *)(((UCHAR *)(ipheader)) + 20))))

#endif // __FFP_DEF_H

