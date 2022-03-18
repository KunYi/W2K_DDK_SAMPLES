/*++

Copyright (c) 1999  Microsoft Corporation

Module Name:

  ffp-com.c

Abstract:

  Contains code for implementing Fast Forwarding Path

Revision History:

--*/

#include <precomp.h>

#if FFP_SUPPORT

#include "ffp-def.h"

__inline
ULONG
HashValue (
    IN               ULONG                Value
    )

/*++

  Routine Description

      Calculate the hash value given a 32-bit word. The
      hash computation has been changed from being
      a V Mod M , to this "wierd" scheme to avoid the
      DIV instruction that caused a big slowdown in
      the fast path. Initially (and ideally) the hash
      code was 
         return Value % FastForwardingCacheSize;

      Basically given a 32 bit word, we add the first 
      half with the second half to get a 16 bit word. 
      Then the first half (byte) in the word is added
      to the second one (byte) after a shift. This 
      operation is actually accomplished by adding the
      word to itself at an offset and ANDing the extra 
      bits out.

      Assume dword  is     A B C D
              =>    Adding the first and second half =>     A    B
                                                       +    C    D
                                                            ------
                                                            X    Y
                                                            ------

      Then we get the hash value as =>    (X >> Shift) + Y 
      [ This would reduce the range from 2^16 to 2^N (8 <= N <= 16) ]

      As we add the first two words in the dword, we optimize all 
      "Add TCP Src Port + Dst Port Words" operations by 
      doing "Add Src Port + Dst Port Dword" [ Saves time as 32 bit
      operations are faster than 8 or 16 bit operations ]

      The hash computation has been changed - now instead of adding 
      the source , destination ports, we add the whole dword [ just
      as we always did in ICMP case when we add the type/code word ].
      This can be done because - as part of the hash computation we 
      are adding the first and second words resulting indirectly in
      the addition of the source and destination ports.

      Note that this hash has approximately the same cyclic behavior
      for values of hash sizes = 2^N, for (8 <= N <= 16) as the mod
      function.
      
  Arguments

    Value    -    The 32 bit value that we are hashing
    
  Return Value
  
        The hash (bounded by a range)

--*/

{
    *((USHORT *) &Value) += *( ((USHORT *) &Value) + 1 );
    *((USHORT *) &Value) += *( ((USHORT *) &Value)) >> FFHashShift;
    Value &= FFHashMask;
    return Value;
}

__inline
ULONG
IsPacketFastForwardingWorthy (
    IN     UNALIGNED EnetHeader          *EthernetHeader,
    IN     UNALIGNED FFPIPHeader         *IpHeader
    )

/*++

  Routine Description

      Called to check if the packet can be fast forwarded.

  Arguments
  
      EthernetHeader - Ethernet header in which the packet is encapsulated
      IpHeader       - IP Header with atleast 20 bytes (without any options)
      
  Return Value
  
        TRUE or FALSE

--*/
{
    // If this is not an ip packet at all - return FALSE

    if (EthernetHeader->EH_Type != 0x0008)
    {
        return FALSE;
    }

    // If packet not IPv4 or if it has options - return FALSE
    
    if (IpHeader->Verlen != 0x45)
    {
        return FALSE;
    }
    
    // If dest ip addr is classD,E or broadcast - return FALSE

    if ((IpHeader->Dest & 0x000000e0) == 0x000000e0)
    {
        return FALSE;
    }

    // If you have are here, you might be fast forwarding worthy 
    // (still you might be an incoming packet with a TTL <= 1)
    // This check has been moved to FFPProcessReceivedPacket fn
    
    return TRUE;
}


static
ULONG
FASTCALL
IsPacketInFastForwardingCache (
    IN     UNALIGNED EnetHeader          *EthernetHeader,
    IN     UNALIGNED FFPIPHeader         *IpHeader,
    IN OUT ULONG                         *Hash, 
    IN     HANDLE                         IncomingAdapter, 
    OUT    HANDLE                        *OutgoingAdapter
    )

/*++

  Routine Description

      Checks the fast forwarding cache. If packet is found as a
      positive cache entry, it updates the ethernet header and
      fixes the ttl and checksum and updates the adapter context
      the packet should be forwarded to. Assumes incoming 'hash'
      = src addr + dst addr + protocol, and fixes it to add the
      TCP/UDP src/dest ports dword (or) the ICMP type/code word.

  Arguments

      EthernetHeader  - ethernet header in which the packet is encapsulated
      IpHeader        - pointer to atleast 24 bytes of the ip header.
      Hash            - Hash = src addr + dst addr + protocol for the packet
                        on entry into this function,
      IncomingAdapter - Handle of the adapter on which the packet came in
                        (currently not used at all),
      OutgoingAdapter - Handle to adapter context on which pkt to be fwded

  Return Value
  
   FFP_DISCARD_PACKET  in case of a negative forwarding cache entry,
   FFP_INDICATE_PACKET in case of no matching entry (indicate to the IP layer),
   FFP_FORWARD_PACKET  in case of a positive cache entry (OutgoingAdapter retd)

--*/
{
    ULONG             hashindex;      // Bucket into which packet would hash
    UCHAR            *restofpacket;   // Pointer to byte after the IP header
    FFPCacheEntry    *cacheentry;     // Fast fwding cache entry for this query
    LONG              cacheentrytype; // Type of above fast forwarding entry

    //
    // To lookup in cache we must add the relevant fields. They are:
    // (dest address, src address, protocol)
    // if IPPROTO_TCP or udp - (src,dest dword)
    // if icmp - (type,code word in the pkt)
    //
    // Hash value here = IpHeader->Dest+IpHeader->Src+IpHeader->Protocol;
    //

#if __FILTER__
    restofpacket = (UCHAR *)IpHeader + 20;

#if FFP_DEBUG
    FFPDbgPrint(("ROP = %08X, Inc Adap = %08X, ", 
                 *((ULONG *)restofpacket), 
                 IncomingAdapter));
#endif

    switch (IpHeader->Protocol)
    {
        case IPPROTO_TCP:
        case IPPROTO_UDP:
            *Hash += *((ULONG *)restofpacket);
            break;
                    
        case IPPROTO_ICMP:
            *Hash += *((USHORT *)restofpacket);
    }
#endif

    hashindex = HashValue(*Hash);

#if FFP_DEBUG
    FFPDbgPrint(("IsPacketInFastFwdingCache: Hash1 = %08X, Hash2 = %04X\n",
                 *Hash, 
                 hashindex));
#endif

    //
    // You can make this check without having to 
    // take the cache entry spinlock - worst case
    // you use an consistent but obsolete entry
    //

    if (INVALID_FORWARDING_CACHE_ENTRY(hashindex))
    {
        return FFP_INDICATE_PACKET;
    }

    cacheentry = &FastForwardingCache[hashindex];

    ACQUIRE_FORWARDING_ENTRY_SPINLOCK(cacheentry);

    //
    // Compare the exact fields - since hash collisions
    // can happen and we dont keep lists on each bucket
    //

    if (   IpHeader->Dest     == cacheentry->FFPCE_IPInfo.IPHI_DestAddress 
        && IpHeader->Src      == cacheentry->FFPCE_IPInfo.IPHI_SrcAddress
#if __FILTER__
        && IpHeader->Protocol == cacheentry->FFPCE_IPInfo.IPHI_Protocol
#endif
        )
    {
#if __FILTER__

#if FFP_DEBUG
        FFPDbgPrint(("First  Word: In Packet: %04X, In Cache: %04X\n", 
                     *((USHORT *)restofpacket+0), 
                     cacheentry->FFPCE_IPInfo.IPHI_FirstWordAfterIPHeader));

        FFPDbgPrint(("Second Word: In Packet: %04X, In Cache: %04X\n", 
                     *((USHORT *)restofpacket+1), 
                     cacheentry->FFPCE_IPInfo.IPHI_SecondWordAfterIPHeader));
#endif
       switch (IpHeader->Protocol)
       {
       case IPPROTO_UDP:
       case IPPROTO_TCP:
           if (*((ULONG *) restofpacket) != *((ULONG *) 
                &(cacheentry->FFPCE_IPInfo.IPHI_FirstWordAfterIPHeader)))
           {
               RELEASE_FORWARDING_ENTRY_SPINLOCK(cacheentry);
               return FFP_INDICATE_PACKET;
           }
        
           break;

       case IPPROTO_ICMP:
           if (*((USHORT *) restofpacket) != *((USHORT *) 
                &(cacheentry->FFPCE_IPInfo.IPHI_FirstWordAfterIPHeader)))
           {
               RELEASE_FORWARDING_ENTRY_SPINLOCK(cacheentry);
               return FFP_INDICATE_PACKET;
           }
       }
#endif

        // Cache entry matched perfectly - So we have a matching FFP
    }
    else
    {
#if FFP_DEBUG
        // did not match - return
        FFPDbgPrint(("Not Found in FFC: S %08X D %08X P %02X F %04X ",
           IpHeader->Src,
           IpHeader->Dest,
           IpHeader->Protocol,
           IpHeader->Id));

#if __FILTER__
        FFPDbgPrint(("P1 %04X P2 %04X",
           *((USHORT *)restofpacket+0),
           *((USHORT *)restofpacket+1)));
#endif

        FFPDbgPrint(("\n"));
#endif
        RELEASE_FORWARDING_ENTRY_SPINLOCK(cacheentry);
        return FFP_INDICATE_PACKET;
    }

    FFPDbgPrint(("Found in FFC: CT %08X S %08X D %08X P %02X F %04X ",
                 cacheentry->FFPCE_CacheEntryType,
                 IpHeader->Src,
                 IpHeader->Dest,
                 IpHeader->Protocol,
                 IpHeader->Id));
       
#if __FILTER__
    FFPDbgPrint(("P1 %04X P2 %04X ",
                 *((USHORT *)restofpacket+0),
                 *((USHORT *)restofpacket+1)));
#endif

    FFPDbgPrint(("INC %04X OUT %04X\n",    
                 IncomingAdapter,
                 cacheentry->FFPCE_OutgoingAdapter));

    // If we are fastforwarding, adjust TTL and XSUM

    if (cacheentry->FFPCE_CacheEntryType == FFP_FORWARD_PACKET)
    {
        // This packet is going to be fast forwarded
        
#ifndef FFP_TESTING
        // Fix up ttl of the new packet
        IpHeader->Ttl--;

        // Fix up the ip header xsum
        IpHeader->Xsum++;
#endif

        //
        // Copy the ethernet header from the cache entry
        // onto the ethernet header passed in - the
        // card would use the new eth header for fwding
        //

        NdisMoveMemory(EthernetHeader, 
                       &cacheentry->FFPCE_MACHeader, 
                       sizeof(EnetHeader));

        // Fill in outgoing adapter from the cache entry
        *OutgoingAdapter = cacheentry->FFPCE_OutgoingAdapter;

        FFPDbgPrint(("And OUTMAC %02X.%02X.%02X.%02X.%02X.%02X\n",
                     cacheentry->FFPCE_MACHeader.EH_Dest.EA_Addr[0],
                     cacheentry->FFPCE_MACHeader.EH_Dest.EA_Addr[1],
                     cacheentry->FFPCE_MACHeader.EH_Dest.EA_Addr[2],
                     cacheentry->FFPCE_MACHeader.EH_Dest.EA_Addr[3],
                     cacheentry->FFPCE_MACHeader.EH_Dest.EA_Addr[4],
                     cacheentry->FFPCE_MACHeader.EH_Dest.EA_Addr[5]
                    ));
     }

    cacheentrytype = cacheentry->FFPCE_CacheEntryType;
    
    RELEASE_FORWARDING_ENTRY_SPINLOCK(cacheentry);

    return cacheentrytype;
}

static
ULONG
FASTCALL
SeedFastForwardingCache(
    IN     UNALIGNED EnetHeader          *EthernetHeader,
    IN     UNALIGNED FFPIPHeader         *IpHeader,
    IN     ULONG                          Hash, 
    IN     HANDLE                         IncomingAdapter, 
    IN     HANDLE                         OutgoingAdapter, 
    IN     ULONG                          CacheEntryType
    )

/*++

  Routine Description

      Puts the packet in the Fast Forwarding Cache using the 
      information from the packet and the incoming adapter 
      passed. Assumes that the incoming hash value = 
      src addr + dst addr + protocol 
         + (icmp type, icmp code word) in case of ICMP, and
      src addr + dst addr + protocol
         + ( src port, dst port dword)  in case of TCP/UDP.

    No checking is done whatsoever on any data being stored
    in the FF cache, Garbage In Garbage Out (GIGO)
    
  Arguments

      EthernetHeader  - ethernet header in which the packet is encapsulated,
      IpHeader        - pointer to atleast 24 bytes of the ip header,
      Hash            - Hash = src addr + dst addr + protocol + ports info
                        on entry into this function,
      IncomingAdapter - Handle of the adapter on which the packet came in
                        (currently not used at all),
      OutgoingAdapter - Handle to adapter context on which pkt to be fwded,
      CacheEntryType  - FFP_DISCARD_PACKET, INDICATE_PACKET, or FORWARD_PACKET
                        (determines the nature of the entry being set in cache)

  Return Value

      TRUE if set or FALSE if any error occured
--*/
{
    ULONG             hashindex;      // Bucket into which packet would hash
    USHORT           *restofpacket;   // Pointer to byte after the IP header
    FFPCacheEntry    *cacheentry;     // Fast fwding cache entry for this query

    //
    // If the incoming adapter and the outgoing adapter is the same, 
    // do not seed the cache. Any such packet (along the recv path)
    // is passed up as IP might give an ICMP redirect to the source
    //

    if ((CacheEntryType == FFP_FORWARD_PACKET) &&
        (IncomingAdapter == OutgoingAdapter))
    {
        return FALSE;
    }

    //
    // Hash at this point = 
    // IpHeader->Dest+IpHeader->Src+IpHeader->Protocol + (port/type/code info)
    //

    hashindex = HashValue(Hash);

    cacheentry = &FastForwardingCache[hashindex];

    ACQUIRE_FORWARDING_ENTRY_SPINLOCK(cacheentry);

    //
    // Set the entry to valid after acquiring lock
    // to prevent reads from reading invalid data
    //

    SET_FORWARDING_CACHE_ENTRY_TO_VALID(hashindex);

#if __FILTER__
    restofpacket = (USHORT *) ((UCHAR *)IpHeader+20);

#if FFP_DEBUG
    FFPDbgPrint(("ROP = %08X, Inc Adap = %08X, ", 
                *((ULONG *)restofpacket), 
                IncomingAdapter));
#endif

#endif

#if FFP_DEBUG
  FFPDbgPrint(("SeedFastForwardingCache: Hash1 = %08X, Hash2 = %04X\n",
               Hash, 
               HashValue(Hash)));
#endif

    cacheentry->FFPCE_IPInfo.IPHI_DestAddress = IpHeader->Dest;
    cacheentry->FFPCE_IPInfo.IPHI_SrcAddress  = IpHeader->Src;

#if __FILTER__
    cacheentry->FFPCE_IPInfo.IPHI_Protocol = IpHeader->Protocol;

    //
    // The following two fields may not be necessary for 
    // non UDP/TCP case but putting them there does not 
    // hurt and we avoid a couple of 'if then' statements
    //

    restofpacket = (USHORT *) ((UCHAR *)IpHeader+20);

#if FFP_DEBUG
    FFPDbgPrint(("First  Word: In Packet: %04X, In Cache: %04X\n",
                *restofpacket, 
                cacheentry->FFPCE_IPInfo.IPHI_FirstWordAfterIPHeader));

    FFPDbgPrint(("Second Word: In Packet: %04X, In Cache: %04X\n", 
                *(restofpacket+1), 
                cacheentry->FFPCE_IPInfo.IPHI_SecondWordAfterIPHeader));
#endif

    cacheentry->FFPCE_IPInfo.IPHI_FirstWordAfterIPHeader = *restofpacket;
    cacheentry->FFPCE_IPInfo.IPHI_SecondWordAfterIPHeader = *(restofpacket+1);
#endif

    cacheentry->FFPCE_CacheEntryType = CacheEntryType;

    FFPDbgPrint(("Put in FFC: CT %08X S %08X D %08X P %02X F %04X ",
                 CacheEntryType,
                 IpHeader->Src,
                 IpHeader->Dest,
                 IpHeader->Protocol,
                 IpHeader->Id));
        
#if __FILTER__
    FFPDbgPrint(("P1 %04X P2 %04X ",
                 *((USHORT *)restofpacket+0),
                 *((USHORT *)restofpacket+1)));
#endif

    FFPDbgPrint(("INC %08X OUT %08X\n",
                 IncomingAdapter,
                 OutgoingAdapter));

    if (CacheEntryType == FFP_FORWARD_PACKET)
    {
        // copy in the outgoing adapter context

        cacheentry->FFPCE_OutgoingAdapter = OutgoingAdapter;

        // Copy in the ethernet header

        NdisMoveMemory (&cacheentry->FFPCE_MACHeader, 
                        EthernetHeader, 
                        sizeof(EnetHeader));

        FFPDbgPrint(("And OUTMAC %02X.%02X.%02X.%02X.%02X.%02X\n",
                     EthernetHeader->EH_Dest.EA_Addr[0],
                     EthernetHeader->EH_Dest.EA_Addr[1],
                     EthernetHeader->EH_Dest.EA_Addr[2],
                     EthernetHeader->EH_Dest.EA_Addr[3],
                     EthernetHeader->EH_Dest.EA_Addr[4],
                     EthernetHeader->EH_Dest.EA_Addr[5]
                    ));
    }
        
    RELEASE_FORWARDING_ENTRY_SPINLOCK(cacheentry);

    return TRUE;
}

static
ULONG
FASTCALL
IsPacketInIncomingCache (
    IN     UNALIGNED FFPIPHeader         *IpHeader,
    IN OUT ULONG                         *Hash, 
    OUT    HANDLE                        *IncomingAdapter
    )
/*++

  Routine Description

    Checks the incoming cache. If entry found - it fills in the
    incoming adapter stored in there and returns TRUE. Assumes
    that the incoming hash value = src addr + dst addr + protocol,
    and fixes it to add TCP/UDP ports (or) ICMP type and code.

  Arguments

      IpHeader        - pointer to atleast 24 bytes of the ip header.
      Hash            - Hash = src addr + dst addr + protocol for the packet
                        on entry into this function,
      IncomingAdapter - Handle of the adapter on which the packet came in
                        (filled in here),
  Return Value
  
      TRUE if incoming cache entry exists, FALSE if not
--*/

{
    ULONG         hashindex;          // Bucket into which packet would hash
    UCHAR        *restofpacket;       // Pointer to byte after the IP header
    IPHeaderInfo *incomingcacheentry; // Incoming cache entry for this query

    //
    // Hash value at this point 
    //      = IpHeader->Dest+IpHeader->Src+IpHeader->Protocol;
    //

#if __FILTER__
    restofpacket = (UCHAR *)IpHeader + 20;
    
    switch (IpHeader->Protocol)
    {
    case IPPROTO_TCP:
    case IPPROTO_UDP:
        *Hash += *((ULONG *)restofpacket);
        break;
                    
    case IPPROTO_ICMP:
        *Hash += *((USHORT *)restofpacket);
    }
#endif

#if FFP_DEBUG
    FFPDbgPrint(("Hash Value in CheckIncomingCache: %08X\n", 
                 *Hash));
#endif

    hashindex = *Hash % IncomingCacheSize;

    //
    // You can make this check without having to 
    // take the cache entry spinlock - worst case
    // you use an consistent but obsolete entry
    //

    if (INVALID_INCOMING_CACHE_ENTRY(hashindex))
    {
        return FALSE;
    }

    incomingcacheentry = &IncomingCache[hashindex];

    ACQUIRE_INCOMING_ENTRY_SPINLOCK(incomingcacheentry);

    //
    // Compare the exact fields - since hash collisions
    // can happen and we dont keep lists on each bucket
    //

    if (   IpHeader->Dest == incomingcacheentry->IPHI_DestAddress 
        && IpHeader->Src  == incomingcacheentry->IPHI_SrcAddress  
#if __FILTER__
        && IpHeader->Protocol == incomingcacheentry->IPHI_Protocol
#endif
       )
    {
#if __FILTER__
        switch (IpHeader->Protocol)
        {
        case IPPROTO_UDP:
        case IPPROTO_TCP:
           if (*((ULONG *)restofpacket) != 
               *((ULONG *)&(incomingcacheentry->IPHI_FirstWordAfterIPHeader)))
           {
               RELEASE_INCOMING_ENTRY_SPINLOCK(incomingcacheentry);
               return FALSE;
           }
            
           break;

        case IPPROTO_ICMP:
           if (*((USHORT *)restofpacket) != 
               *((USHORT *)&(incomingcacheentry->IPHI_FirstWordAfterIPHeader)))
            {
                RELEASE_INCOMING_ENTRY_SPINLOCK(incomingcacheentry);
                return FALSE;
            }
        }
#endif
        *IncomingAdapter = incomingcacheentry->IPHI_Adapter;

        FFPDbgPrint(("Found in Incoming: S %08X D %08X P %02X F %04X ",
                     IpHeader->Src,
                     IpHeader->Dest,
                     IpHeader->Protocol,
                     IpHeader->Id));
           
#if __FILTER__
        FFPDbgPrint(("P1 %04X P2 %04X ",
                     *(restofpacket+0),
                     *(restofpacket+1)));
#endif

        FFPDbgPrint(("%08X\n", *IncomingAdapter));

        RELEASE_INCOMING_ENTRY_SPINLOCK(incomingcacheentry);

        return TRUE;
    }

#if FFP_DEBUG    
    FFPDbgPrint(("Not found in Incoming: S %08X D %08X P %02X F %04X ",
                 IpHeader->Src,
                 IpHeader->Dest,
                 IpHeader->Protocol,
                 IpHeader->Id));
           
#if __FILTER__
    FFPDbgPrint(("P1 %04X P2 %04X ",
                 *(restofpacket+0),
                 *(restofpacket+1)));
#endif

    FFPDbgPrint(("\n"));
#endif

    RELEASE_INCOMING_ENTRY_SPINLOCK(incomingcacheentry);

    return FALSE;
}

static
VOID
FASTCALL
SeedIncomingCache (
    IN     UNALIGNED FFPIPHeader         *IpHeader,
    IN     ULONG                          Hash, 
    IN     HANDLE                         IncomingAdapter
    )
/*++

  Routine Description

      Puts the header information in the incoming cache (to 
      get incoming adapter later). Assumes that incoming 
      hash value = src addr + dst addr + protocol 
          + (icmp type + icmp code) in case of ICMP, and
                 = src addr + dst addr + protocol
          + ( src port + dst port ) in case of TCP/UDP.

  Arguments

      IpHeader        - pointer to atleast 24 bytes of the ip header.
      Hash            - Hash = src addr + dst addr + protocol + ports info
                        on entry into this function,
      IncomingAdapter - Handle of the adapter on which the packet came in
                        (used in hashing),

  Return Value

      None
      
--*/

{
    ULONG         hashindex;          // Bucket into which packet would hash
    USHORT       *restofpacket;       // Pointer to byte after the IP header
    IPHeaderInfo *incomingcacheentry; // Incoming cache entry for this query

#if FFP_DEBUG
    FFPDbgPrint(("Hash Value in SeedIncomingCache: %08X\n", 
                 Hash));
#endif

    hashindex = Hash % IncomingCacheSize;

    incomingcacheentry = &IncomingCache [hashindex];

    ACQUIRE_INCOMING_ENTRY_SPINLOCK(incomingcacheentry);

    //
    // Set the entry to valid after acquiring lock
    // to prevent reads from reading invalid data
    //

    SET_INCOMING_CACHE_ENTRY_TO_VALID(hashindex);
    
    incomingcacheentry->IPHI_Adapter = IncomingAdapter;
    incomingcacheentry->IPHI_DestAddress = IpHeader->Dest;
    incomingcacheentry->IPHI_SrcAddress = IpHeader->Src;

#if __FILTER__
    incomingcacheentry->IPHI_Protocol = IpHeader->Protocol;

    //
    // The following two fields may not be necessary for 
    // non UDP/TCP case but putting them there does not 
    // hurt and we avoid a couple of 'if then' statements
    //

    restofpacket = (USHORT *) ((UCHAR *)IpHeader+20);

    incomingcacheentry->IPHI_FirstWordAfterIPHeader = *restofpacket;
    incomingcacheentry->IPHI_SecondWordAfterIPHeader = *(restofpacket+1);
#endif

    FFPDbgPrint(("SeedIncoming: S %08X D %08X P %02X F %04X ",
                 IpHeader->Src,
                 IpHeader->Dest,
                 IpHeader->Protocol,
                 IpHeader->Id));

#if __FILTER__
    FFPDbgPrint(("P1 %04X P2 %04X ",
                 *(restofpacket+0),
                 *(restofpacket+1)));
#endif

    FFPDbgPrint(("%08X\n", IncomingAdapter));

    RELEASE_INCOMING_ENTRY_SPINLOCK(incomingcacheentry);

    return;
}


#if __FILTER__

static
ULONG
FASTCALL
IsPacketInFragmentCache (
    IN     UNALIGNED FFPIPHeader         *IpHeader,
    IN     ULONG                          Hash, 
    OUT    ULONG                         *DWordAfterIPHeader, 
    OUT    HANDLE                        *IncomingAdapter
    )

/*++

  Routine Description

      Checks the fragment cache. If entry found, it fills in 
      the port/code information and incoming adapter context
      stored in there, and returns TRUE. Assumes that the
      incoming hash value = src addr + dst addr + protocol.

  Arguments

      IpHeader           - pointer to atleast 24 bytes of the ip header.
      Hash               - Hash = src addr + dst addr + protocol for the packet
                           on entry into this function,
      DWordAfterIPHeader - location where first 32-bit dword after IP header 
                           is returned      
      IncomingAdapter    - Handle of the adapter on which the packet came in
                           (filled in here),
  Return Value

      TRUE or FALSE

--*/
{
    ULONG         hashindex;          // Bucket into which packet would hash
    IPHeaderInfo *fragmentcacheentry; // Fragment cache entry for this query

#if FFP_DEBUG
    FFPDbgPrint(("Hash Value in CheckFragmentCache: %08X\n", 
                 Hash + IpHeader->Id));
#endif

    hashindex = (Hash + IpHeader->Id) % FragmentCacheSize;

    //
    // You can make this check without having to 
    // take the cache entry spinlock - worst case
    // you use an consistent but obsolete entry
    //

    if (INVALID_FRAGMENT_CACHE_ENTRY(hashindex))
    {
        return FALSE;
    }
   
    fragmentcacheentry = &FragmentCache [hashindex];
    
    ACQUIRE_FRAGMENT_ENTRY_SPINLOCK(fragmentcacheentry);

    //
    // Compare the exact fields - since hash collisions
    // can happen and we dont keep lists on each bucket
    //

    if (IpHeader->Dest == fragmentcacheentry->IPHI_DestAddress &&
        IpHeader->Src  == fragmentcacheentry->IPHI_SrcAddress  &&
        IpHeader->Protocol == fragmentcacheentry->IPHI_Protocol &&
        IpHeader->Id == fragmentcacheentry->IPHI_FragmentId)
    {
        *DWordAfterIPHeader = 
            (fragmentcacheentry->IPHI_SecondWordAfterIPHeader << 16 ) 
            + fragmentcacheentry->IPHI_FirstWordAfterIPHeader;

        *IncomingAdapter = fragmentcacheentry->IPHI_Adapter;

        FFPDbgPrint(("Found in Fragment: S %08X D %08X " \
                     "P %02X F %04X P1P2 %08X INC %08X\n",
                     IpHeader->Src,
                     IpHeader->Dest,
                     IpHeader->Protocol,
                     IpHeader->Id,
                     *DWordAfterIPHeader,
                     *IncomingAdapter));

        RELEASE_FRAGMENT_ENTRY_SPINLOCK(fragmentcacheentry);

        return TRUE;
    }
    
#if FFP_DEBUG
    FFPDbgPrint(("Not found in Fragment: S %08X D %08X P %02X F %04X\n",
                 IpHeader->Src,
                 IpHeader->Dest,
                 IpHeader->Protocol,
                 IpHeader->Id)); 
#endif

    RELEASE_FRAGMENT_ENTRY_SPINLOCK(fragmentcacheentry);

    return FALSE;
}

static
VOID
FASTCALL
SeedFragmentCache(
    IN     UNALIGNED FFPIPHeader         *IpHeader,
    IN     ULONG                          Hash, 
    IN     HANDLE                         IncomingAdapter
    )

/*++

  Routine Description

      Puts the header information of the first fragment
      in the fragment cache. Assumes that the incoming 
      hash value = src addr + dst addr + protocol.

  Arguments

      IpHeader        - pointer to atleast 24 bytes of the ip header.
      Hash            - Hash = src addr + dst addr + protocol for the packet
                        on entry into this function,
      IncomingAdapter - Handle of the adapter on which the packet came in
                        (used in hashing).
  Return Value

      None

--*/

{
    ULONG         hashindex;          // Bucket into which packet would hash
    USHORT       *restofpacket;       // Pointer to byte after the IP header
    IPHeaderInfo *fragmentcacheentry; // Fragment cache entry for this query

#if FFP_DEBUG
    FFPDbgPrint(("Hash Value in SeedFragmentCache: %08X\n", 
                 Hash + IpHeader->Id));
#endif

    hashindex = (Hash + IpHeader->Id) % FragmentCacheSize;

    fragmentcacheentry = &FragmentCache [hashindex];
    
    ACQUIRE_FRAGMENT_ENTRY_SPINLOCK(fragmentcacheentry);

    //
    // Set the entry to valid after acquiring lock
    // to prevent reads from reading invalid data
    //
    
    SET_FRAGMENT_CACHE_ENTRY_TO_VALID(hashindex);

    fragmentcacheentry->IPHI_Adapter = IncomingAdapter;
    fragmentcacheentry->IPHI_DestAddress = IpHeader->Dest;
    fragmentcacheentry->IPHI_SrcAddress = IpHeader->Src;
    fragmentcacheentry->IPHI_Protocol = IpHeader->Protocol;
    fragmentcacheentry->IPHI_FragmentId = IpHeader->Id;

    //
    // The following two fields may not be necessary for 
    // non UDP/TCP case but putting them there does not 
    // hurt and we avoid a couple of 'if then' statements
    //

    restofpacket = (USHORT *) ((UCHAR *)IpHeader+20);

    fragmentcacheentry->IPHI_FirstWordAfterIPHeader  = *restofpacket;
    fragmentcacheentry->IPHI_SecondWordAfterIPHeader = *(restofpacket+1);

    FFPDbgPrint(("SeedFragment: S %08X D %08X P %02X " \
                 "F %04X P1 %04X P2 %04X %08X\n",
                 IpHeader->Src,
                 IpHeader->Dest,
                 IpHeader->Protocol,
                 IpHeader->Id,
                 *restofpacket,
                 *(restofpacket+1),
                 IncomingAdapter));

    RELEASE_FRAGMENT_ENTRY_SPINLOCK(fragmentcacheentry);

    return;
}

#endif

ULONG
FASTCALL
#if __FILTER__
FFPWithFiltering_FFPProcessReceivedPacket
#else
FFPNoFiltering_FFPProcessReceivedPacket
#endif
    (
    IN     UNALIGNED EnetHeader            *EthernetHeader, 
    IN     UNALIGNED FFPIPHeader           *PacketHeader, 
    IN     HANDLE                           IncomingAdapter, 
    OUT    HANDLE                          *OutgoingAdapter
    )

/*++

  Routine Description

    Called on the up path [when layer 2 receives a packet and
    passes into to layer 3], tries to bypass the layer 3 (N/W
    layer) path by using the fast forwarding cache.

    Also seeds the incoming or fragment caches with information 
    useful for generating forwarding cache entries on packet's
    down path. Also see 'FFPProcessSentPacket'

  Arguments

      EthernetHeader  - ethernet header in which the packet is encapsulated,
      PacketHeader    - pointer to atleast 24 bytes of the packet header,
      IncomingAdapter - Handle of the adapter on which the packet came in,
      OutgoingAdapter - Handle to adapter context on which pkt to be fwded
      
  Return Value
  
      FFP_DISCARD_PACKET (= -1) in case of a negative fast fwding cache entry,
      FFP_INDICATE_PACKET (= +0) in case of no matching entry (indicate to IP),
      FFP_FORWARD_PACKET (= +1) for a +ve cache entry (OutgoingAdapter filled)

--*/

{
    ULONG hash;          // Hash value storage (to prevent some recomputation)
    ULONG retcode;       // Result of the query into the fast forwarding cache
#if __FILTER__
    ULONG bInFragmentCache;                // Flag denoting that fragment's id 
                                           // is in the cache
    ULONG dwordAfterIPHeaderThisFragment;  // The 32 bit word after IP Header
                                           // for the current fragment
    ULONG dwordAfterIPHeaderFirstFragment; // The 32 bit word after IP Header 
                                           // for 1st fragment with current id
#endif

    //
    // Check if the current packet is worthy of 
    // fast forwarding (except the TTL check)
    //

    if (!IsPacketFastForwardingWorthy (EthernetHeader, PacketHeader))
    {
        return FFP_INDICATE_PACKET;
    }

#if FFP_DEBUG
#if __FILTER__
    FFPDbgPrint(("\nFFPWithFiltering_FFPReceivedPacket - Enter\n"));
#else
    FFPDbgPrint(("\nFFPNoFiltering_FFPReceivedPacket - Enter\n"));
#endif

    PrintIPPacketHeader (PacketHeader);
#endif

    // We are here - so we know that the packet is IPv4 packet with no options
    // Is ttl of incoming packet is > 1 ? - only then can it be fast forwarded

    if (TTL_PKT_EXPIRED(PacketHeader))
    {
#if FFP_DEBUG
        FFPDbgPrint(("\nFFPRecvPacket - Leave @1\n"));
#endif
        return FFP_INDICATE_PACKET;
    }

    //
    // Initialize hash to the sum of src and dest IP addr ( rotate dest 
    // addr left by one bit before adding, to remove any source,dest
    // symmetry ). If filtering is enabled, add the protocol as we can
    // filter based on protocol id (TCP, UDP, ICMP ...)
    //

    hash = INIT_HASH_VALUE(PacketHeader);
    
#if __FILTER__
    hash += PacketHeader->Protocol;
#endif

    //
    // If we are filtering based on higher layer (TCP, UDP, ...) info,
    // then fragmentation adds additional complexity as this info is
    // only present in the first fragment of all that make the packet
    //

#if __FILTER__
    if (NOT_FRAGMENTED(PacketHeader))
    {
#endif
        //
        // Pkt not fragmented ; check the fast forwarding 
        // cache for a entry(to bypass the network layer)
        //
        
        if ((retcode = IsPacketInFastForwardingCache (EthernetHeader, 
                                                      PacketHeader, 
                                                      &hash, 
                                                      IncomingAdapter, 
                                                      OutgoingAdapter)) 
                                                        == FFP_INDICATE_PACKET)
        {
            //
            // No FF entry; atleast preserve the incoming adapter
            // for use on the packet's down path
            //

            SeedIncomingCache(PacketHeader, 
                              hash, 
                              IncomingAdapter);
        }

#if FFP_DEBUG
        FFPDbgPrint(("\nFFPRecvPacket - Leave @2\n"));
#endif
        
        return retcode;
        
#if __FILTER__
    }
    else
    {
        //
        // Pkt fragmented ; but only the first fragment 
        // has valid higher layer (port/type/code) info
        //
    
        //
        // Set initial state  (current fragment 
        // not yet found in the fragment cache)
        //
    
        bInFragmentCache = FALSE;
    
        if (FIRST_FRAGMENT(PacketHeader))
        {
            //
            // Store the port/type/code information tagged
            // with packet id (for use with later fragments)
            //
    
            SeedFragmentCache(PacketHeader, 
                              hash, 
                              IncomingAdapter);
        }
        else
        {
            //
            // Get the protocol specific info 
            // (TCP | UDP Ports / ICMP Type & Code) 
            // for the fragment stashed before
            //
    
            if (!(bInFragmentCache = 
                  IsPacketInFragmentCache (PacketHeader, 
                                           hash, 
                                           &dwordAfterIPHeaderFirstFragment, 
                                           &IncomingAdapter)))
            {
#if FFP_DEBUG
                FFPDbgPrint(("\nFFPRecvPacket - Leave @3\n"));
#endif
                return FFP_INDICATE_PACKET;
            }
    
            //
            // Substitute the port-specific words for this
            // fragment with those from the first fragment
            //
#if FFP_DEBUG    
            dwordAfterIPHeaderThisFragment = 0;
    
            FFPDbgPrint(("\n@1 dwordAfterIPHeaderThisFragment = %08X\n"   \
                         "dwordAfterIPHeaderFirstFragment = %08X\n"       \
                         "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = %08X\n",
                         dwordAfterIPHeaderThisFragment, 
                         dwordAfterIPHeaderFirstFragment, 
                         FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
            dwordAfterIPHeaderThisFragment =
                FIRST_DWORD_AFTER_IP_HEADER(PacketHeader);
#if FFP_DEBUG
            FFPDbgPrint(("\n@2 dwordAfterIPHeaderThisFragment = %08X\n"   \
                         "dwordAfterIPHeaderFirstFragment = %08X\n"       \
                         "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = %08X\n",
                         dwordAfterIPHeaderThisFragment, 
                         dwordAfterIPHeaderFirstFragment, 
                         FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
            FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) =
                dwordAfterIPHeaderFirstFragment;
#if FFP_DEBUG
            FFPDbgPrint(("\n@3 dwordAfterIPHeaderThisFragment = %08X\n"   \
                         "dwordAfterIPHeaderFirstFragment = %08X\n"       \
                         "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = %08X\n",
                         dwordAfterIPHeaderThisFragment, 
                         dwordAfterIPHeaderFirstFragment, 
                         FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
        }
        
        retcode = IsPacketInFastForwardingCache(EthernetHeader, 
                                                PacketHeader, 
                                                &hash, 
                                                IncomingAdapter, 
                                                OutgoingAdapter);
        if (bInFragmentCache)
        {
            //
            // Put back the "this packet specific
            // information" substituted earlier 
            //
    
            FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = 
                dwordAfterIPHeaderThisFragment;

#if FFP_DEBUG
            FFPDbgPrint(("\n@4 dwordAfterIPHeaderThisFragment = %08X\n"   \
                         "dwordAfterIPHeaderFirstFragment = %08X\n"       \
                         "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = %08X\n",
                         dwordAfterIPHeaderThisFragment, 
                         dwordAfterIPHeaderFirstFragment, 
                         FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
        }

#if FFP_DEBUG
        FFPDbgPrint(("\nFFPRecvPacket - Leave @4\n"));
#endif
        return retcode;
    }

#endif // __FILTER__
}

VOID
FASTCALL
#if __FILTER__
FFPWithFiltering_FFPProcessSentPacket 
#else
FFPNoFiltering_FFPProcessSentPacket 
#endif
    (
    IN     UNALIGNED EnetHeader            *EthernetHeader, 
    IN     UNALIGNED FFPIPHeader           *PacketHeader, 
    IN     HANDLE                           OutgoingAdapter
    )

/*++

  Routine Description

    Called on the down path [when layer 3 passes a packet 
    to layer 2 for forwarding], tries to captute forwarding
    information by seeding the fast-forwarding cache with
    information gathered from the packet (on its down path),
    and corresposing info in the incoming or fragment cache
    (that was captured on the packet's up path). Also see 
    'FFPProcessReceivedPacket'

  Arguments

      EthernetHeader  - ethernet header in which the packet is encapsulated,
      PacketHeader    - pointer to atleast 24 bytes of the packet header,
      OutgoingAdapter - Handle to adapter context on which pkt is being sent.

  Return Value

      None

--*/

{
    ULONG  hash;          // Hash value storage (to prevent some recomputation)
    HANDLE hIncomingAdapter;                // Handle to the incoming adapter
#if __FILTER__
    ULONG  bInFragmentCache;                // Flag denoting that fragment's id 
                                            // is in the cache
    ULONG  dwordAfterIPHeaderThisFragment;  // The 32 bit word after IP Header
                                            // for the current fragment
    ULONG  dwordAfterIPHeaderFirstFragment; // The 32 bit word after IP Header 
                                            // for 1st fragment with current id
#endif

    //
    // Check if the packet was worthy of fast 
    // forwarding on its initial arrival (rcv)
    //

    if (!IsPacketFastForwardingWorthy (EthernetHeader, PacketHeader))
    {
        return;
    }

#if FFP_DEBUG
#if __FILTER__
    FFPDbgPrint(("\nFFPWithFiltering_FFPSentPacket - Enter\n"));
#else
    FFPDbgPrint(("\nFFPNoFiltering_FFPSentPacket - Enter\n"));
#endif

    PrintIPPacketHeader ((FFPIPHeader *)PacketHeader);
#endif

    //
    // Initialize hash to the sum of src and dest IP addr 
    // ( rotate dest addr left by one bit before adding,
    // to remove any source,destination symmetry ). If
    // filtering is enabled, add the protocol as we can
    // filter based on protocol id (TCP, UDP, ICMP...).
    //

    hash = INIT_HASH_VALUE(PacketHeader);
    
#if __FILTER__
    hash += PacketHeader->Protocol;
#endif

    //
    // If we are filtering based on higher layer (TCP, UDP, ...) info,
    // then fragmentation adds additional complexity as this info is
    // only present in the first fragment of all that make the packet
    //

#if __FILTER__
    if (NOT_FRAGMENTED(PacketHeader))
    {
        //
        // Packet not fragmented ; check incoming 
        // cache for incoming adapter information
        //
#endif

        if (IsPacketInIncomingCache (PacketHeader, &hash, &hIncomingAdapter))
        {
            //
            // Use the packet info (Addrs & Ports ...) and incoming
            // adapter and seed FF cache - retcode is ignored
            //

            SeedFastForwardingCache(EthernetHeader,
                                    PacketHeader,
                                    hash,
                                    hIncomingAdapter,
                                    OutgoingAdapter,
                                    FFP_FORWARD_PACKET);
        }
        
#if __FILTER__
    }
    else
    {
        //
        // Seed only for first fragment (to take care of
        // filters that pass all non-first fragments)
        //

        if (FIRST_FRAGMENT(PacketHeader))
        {    
            //
            // Packet fragmented; check fragment cache
            // for incoming adapter & ports/code info
            //

            if (IsPacketInFragmentCache (PacketHeader, 
                                         hash, 
                                         &dwordAfterIPHeaderFirstFragment, 
                                         &hIncomingAdapter))
            {
#if FFP_DEBUG
               dwordAfterIPHeaderThisFragment = 0;

               FFPDbgPrint(("\n@1 dwordAfterIPHeaderThisFragment = %08X\n" \
                            "dwordAfterIPHeaderFirstFragment = %08X\n"     \
                            "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)=%08X\n",
                            dwordAfterIPHeaderThisFragment, 
                            dwordAfterIPHeaderFirstFragment, 
                            FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
               dwordAfterIPHeaderThisFragment =  
                   FIRST_DWORD_AFTER_IP_HEADER(PacketHeader);

#if FFP_DEBUG
               FFPDbgPrint(("\n@2 dwordAfterIPHeaderThisFragment = %08X\n" \
                            "dwordAfterIPHeaderFirstFragment = %08X\n"     \
                            "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)=%08X\n",
                            dwordAfterIPHeaderThisFragment, 
                            dwordAfterIPHeaderFirstFragment, 
                            FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
               FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = 
                   dwordAfterIPHeaderFirstFragment;

#if FFP_DEBUG
               FFPDbgPrint(("\n@3 dwordAfterIPHeaderThisFragment = %08X\n" \
                            "dwordAfterIPHeaderFirstFragment = %08X\n"     \
                            "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)=%08X\n",
                            dwordAfterIPHeaderThisFragment, 
                            dwordAfterIPHeaderFirstFragment, 
                            FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
               //
               // Update hash value with the information 
               // from the first fragment with the same id
               //

               switch ( PacketHeader->Protocol )
               {
               case IPPROTO_TCP:
               case IPPROTO_UDP:
                   hash += (ULONG) dwordAfterIPHeaderFirstFragment;
                   break;
                        
               case IPPROTO_ICMP:
                   hash += (USHORT) dwordAfterIPHeaderFirstFragment;
               }

               //
               // Use the packet info (Addrs & Ports ...) and incoming
               // adapter and seed FF cache - retcode is ignored
               //

               SeedFastForwardingCache(EthernetHeader, 
                                       PacketHeader, 
                                       hash, 
                                       hIncomingAdapter, 
                                       OutgoingAdapter, 
                                       FFP_FORWARD_PACKET);

                FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = 
                    dwordAfterIPHeaderThisFragment;
#if FFP_DEBUG
               FFPDbgPrint(("\n@4 dwordAfterIPHeaderThisFragment = %08X\n" \
                            "dwordAfterIPHeaderFirstFragment = %08X\n"     \
                            "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)=%08X\n",
                            dwordAfterIPHeaderThisFragment, 
                            dwordAfterIPHeaderFirstFragment, 
                            FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
            }
        }
    }
    
#endif // __FILTER__

#if FFP_DEBUG
    FFPDbgPrint(("\nFFPSentPacket - Leave\n"));
#endif
}

ULONG
FASTCALL
#if __FILTER__
FFPWithFiltering_FFPProcessSetInFFCache
#else
FFPNoFiltering_FFPProcessSetInFFCache
#endif
    (
    IN     UNALIGNED FFPIPHeader         *PacketHeader, 
    IN     ULONG                          CacheEntryType
    )

/*++

  Routine Description

      Puts the packet in the Fast Forwarding Cache using
      the information from the packet and the type of the
      cache entry. This function can only be used to see 
      a negative cache entry, or remove an existing entry
      by seeding a invalid cache entry. Positive cache
      entries cannot be seeded and can only be learned.
    
  Arguments

      PacketHeader    - pointer to atleast 24 bytes of the packet header,
      CacheEntryType  - FFP_DISCARD_PACKET, INDICATE_PACKET, or FORWARD_PACKET
                        (determines the nature of the entry being set in cache)
  Return Value

      TRUE or FALSE
--*/

{
    ULONG  hash;          // Hash value storage (to prevent some recomputation)
    ULONG  retcode;       // Result of the seed into the fast forwarding cache
#if __FILTER__
    ULONG   bInFragmentCache;               // Flag denoting that fragment's id 
                                            // is in the cache
    HANDLE  hIncomingAdapter;               // Handle to the incoming adapter
    UCHAR  *restofpacket;                   // Pointer to byte after IP header
    ULONG   dwordAfterIPHeaderThisFragment; // The 32 bit word after IP Header
                                            // for the current fragment
    ULONG   dwordAfterIPHeaderFirstFragment;// The 32 bit word after IP Header 
                                            // for 1st fragment with current id
#endif

    //
    // Cannot seed a +ve fast forwarding entry
    //
    
    if (CacheEntryType == FFP_FORWARD_PACKET)
    {
        return FALSE;
    }

    //
    // Make sure that the packet being seeded 
    // in is an IPv4 packet, and that dest ip
    // addr is not a broadcast or a multicast
    //

    if ((PacketHeader->Verlen != 0x45) || 
        ((PacketHeader->Dest & 0x000000e0) == 0x000000e0))
    {
        return FALSE;
    }

    //
    // Initialize hash to the sum of src and 
    // dest IP addr ( rotate dest addr left 
    // by one bit before adding, to remove 
    // any source,destination symmetry ). If
    // filtering is enabled, add the protocol
    // as we can filter based on the protocol.
    //

    hash = INIT_HASH_VALUE(PacketHeader);
    
#if __FILTER__
    hash += PacketHeader->Protocol;
#endif

    //
    // If we are filtering based on higher layer (TCP, UDP, ...) info,
    // then fragmentation adds additional complexity as this info is
    // only present in the first fragment of all that make the packet
    //

#if __FILTER__

    //
    // Set initial state (current fragment
    // not yet found in the fragment cache)
    //

    bInFragmentCache = FALSE;
    
    if ((NOT_FRAGMENTED(PacketHeader) || FIRST_FRAGMENT(PacketHeader)))
    {
        restofpacket = (UCHAR *)PacketHeader + 20;
    
        switch (PacketHeader->Protocol)
        {
        case IPPROTO_TCP:
        case IPPROTO_UDP:
            hash += *((ULONG *)restofpacket);
            break;
                    
        case IPPROTO_ICMP:
            hash += *((USHORT *)restofpacket);
        }
    }
    else    
    {
        //
        // Filtering is enabled and packet being
        // seeded is not the first fragment ...
        //

        //
        // Bad Port/Code/Type Information - See if
        // we can get right info from fragment cache
        //

        if (bInFragmentCache = 
                IsPacketInFragmentCache(PacketHeader, 
                                        hash, 
                                        &dwordAfterIPHeaderFirstFragment, 
                                        &hIncomingAdapter))
        {
#if FFP_DEBUG
            dwordAfterIPHeaderThisFragment = 0;
    
            FFPDbgPrint(("\n@1 dwordAfterIPHeaderThisFragment = %08X\n"   \
                         "dwordAfterIPHeaderFirstFragment = %08X\n"       \
                         "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = %08X\n",
                         dwordAfterIPHeaderThisFragment, 
                         dwordAfterIPHeaderFirstFragment, 
                         FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
            dwordAfterIPHeaderThisFragment = 
                FIRST_DWORD_AFTER_IP_HEADER(PacketHeader);

#if FFP_DEBUG
            FFPDbgPrint(("\n@2 dwordAfterIPHeaderThisFragment = %08X\n"   \
                         "dwordAfterIPHeaderFirstFragment = %08X\n"       \
                         "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = %08X\n",
                         dwordAfterIPHeaderThisFragment, 
                         dwordAfterIPHeaderFirstFragment, 
                         FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
            FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = 
                dwordAfterIPHeaderFirstFragment;
    
#if FFP_DEBUG
            FFPDbgPrint(("\n@3 dwordAfterIPHeaderThisFragment = %08X\n"   \
                         "dwordAfterIPHeaderFirstFragment = %08X\n"       \
                         "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = %08X\n",
                         dwordAfterIPHeaderThisFragment, 
                         dwordAfterIPHeaderFirstFragment, 
                         FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
    
            switch ( PacketHeader->Protocol )
            {
            case IPPROTO_TCP:
            case IPPROTO_UDP:
                hash += (ULONG) dwordAfterIPHeaderFirstFragment;
                break;
                    
            case IPPROTO_ICMP:
                hash += (USHORT) dwordAfterIPHeaderFirstFragment;
            }
        }
        else
        {
            //
            // We do not have good packet information as
            // we do not have the corr. first fragment
            //

            return FALSE;
        }
    }
        
#endif // __FILTER__

    //
    // Seed a zero -ve incoming adapter - as this entry is -ve
    // or invalid in which case incoming adapter doesnt matter.
    //

    retcode = SeedFastForwardingCache(NULL, 
                                      PacketHeader, 
                                      hash, 
                                      0, 
                                      0, 
                                      CacheEntryType);
#if __FILTER__
    if (bInFragmentCache)
    {
        //
        // Put back the "this packet specific 
        // information" substituted earlier 
        //

        FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = 
            dwordAfterIPHeaderThisFragment;

#if FFP_DEBUG
        FFPDbgPrint(("\n@4 dwordAfterIPHeaderThisFragment = %08X\n"   \
                     "dwordAfterIPHeaderFirstFragment = %08X\n"       \
                     "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = %08X\n",
                     dwordAfterIPHeaderThisFragment, 
                     dwordAfterIPHeaderFirstFragment, 
                     FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
    }
#endif // __FILTER__

    return retcode;
}

ULONG
FASTCALL
#if __FILTER__
FFPWithFiltering_FFPProcessQueryFFCache 
#else
FFPNoFiltering_FFPProcessQueryFFCache 
#endif
    (
    IN     UNALIGNED FFPIPHeader         *PacketHeader
    )

/*++

  Routine Description

      Checks the fast forwarding cache for an entry that matches the Packet

  Arguments

      PacketHeader    - pointer to atleast 24 bytes of the packet header,

  Return Value

      FFP_DISCARD_PACKET (= -1) in case of a negative fast fwding cache entry,
      FFP_INDICATE_PACKET (= +0) in case of no matching entry (indicate to IP),
      FFP_FORWARD_PACKET (= +1) for a +ve cache entry (OutgoingAdapter filled)

--*/

{
    ULONG  hash;          // Hash value storage (to prevent some recomputation)
    ULONG  retcode;       // Result of the query into the fast forwarding cache
#if __FILTER__
    ULONG   bInFragmentCache;               // Flag denoting that fragment's id 
                                            // is in the cache
    HANDLE  hIncomingAdapter;               // Handle to the incoming adapter
    UCHAR  *restofpacket;                   // Pointer to byte after IP header
    ULONG   dwordAfterIPHeaderThisFragment; // The 32 bit word after IP Header
                                            // for the current fragment
    ULONG   dwordAfterIPHeaderFirstFragment;// The 32 bit word after IP Header 
                                            // for 1st fragment with current id
#endif

    //
    // Make sure that the packet being queried
    // in is an IPv4 packet, and that dest ip
    // addr is not a broadcast or a multicast
    //

    if ((PacketHeader->Verlen != 0x45) || 
        ((PacketHeader->Dest & 0x000000e0) == 0x000000e0))
    {
        return FFP_INDICATE_PACKET;
    }

    //
    // Initialize hash to the sum of src and dest IP addr ( rotate dest 
    // addr left by one bit before adding, to remove any source,dest
    // symmetry ). If filtering is enabled, add the protocol as we can
    // filter based on protocol id (TCP, UDP, ICMP ...)
    //

    hash = INIT_HASH_VALUE(PacketHeader);
    
#if __FILTER__
    hash += PacketHeader->Protocol;
#endif

    //
    // If we are filtering based on higher layer (TCP, UDP, ...) info,
    // then fragmentation adds additional complexity as this info is
    // only present in the first fragment of all that make the packet
    //

#if __FILTER__

    //
    // Set initial state (current fragment
    // not yet found in the fragment cache)
    //

    bInFragmentCache = FALSE;
    
    if (!(NOT_FRAGMENTED(PacketHeader) || FIRST_FRAGMENT(PacketHeader)))
    {    
        //
        // Filtering is enabled and packet being
        // seeded is not the first fragment ...
        //

        //
        // Bad Port/Code/Type Information - See if
        // we can get right info from fragment cache
        //
        
        if (bInFragmentCache = 
                IsPacketInFragmentCache(PacketHeader,
                                        hash, 
                                        &dwordAfterIPHeaderFirstFragment,
                                        &hIncomingAdapter))
        {
#if FFP_DEBUG
            dwordAfterIPHeaderThisFragment = 0;

            FFPDbgPrint(("\n@1 dwordAfterIPHeaderThisFragment = %08X\n"   \
                         "dwordAfterIPHeaderFirstFragment = %08X\n"       \
                         "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = %08X\n",
                         dwordAfterIPHeaderThisFragment, 
                         dwordAfterIPHeaderFirstFragment, 
                         FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
            dwordAfterIPHeaderThisFragment = 
                FIRST_DWORD_AFTER_IP_HEADER(PacketHeader);

#if FFP_DEBUG
            FFPDbgPrint(("\n@2 dwordAfterIPHeaderThisFragment = %08X\n"   \
                         "dwordAfterIPHeaderFirstFragment = %08X\n"       \
                         "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = %08X\n",
                         dwordAfterIPHeaderThisFragment, 
                         dwordAfterIPHeaderFirstFragment, 
                         FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
            FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = 
                dwordAfterIPHeaderFirstFragment;
    
#if FFP_DEBUG
            FFPDbgPrint(("\n@3 dwordAfterIPHeaderThisFragment = %08X\n"   \
                         "dwordAfterIPHeaderFirstFragment = %08X\n"       \
                         "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = %08X\n",
                         dwordAfterIPHeaderThisFragment, 
                         dwordAfterIPHeaderFirstFragment, 
                         FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
        }
        else
        {
            //
            // We do not have good packet information as
            // we do not have the corr. first fragment
            //

            return FALSE;
        }
    }
        
#endif

    //
    // Check for the existence of a cache entry in the FF cache.
    // Give a zero -ve incoming adapter - as this entry is -ve
    // or invalid in which case incoming adapter doesnt matter.
    //

    retcode = IsPacketInFastForwardingCache(NULL, 
                                            PacketHeader, 
                                            &hash, 
                                            0, 
                                            NULL);

#if __FILTER__
    if (bInFragmentCache)
    {
        //
        // Put back the "this packet specific
        // information" substituted earlier 
        //

        FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = 
            dwordAfterIPHeaderThisFragment;

#if FFP_DEBUG
        FFPDbgPrint(("\n@4 dwordAfterIPHeaderThisFragment = %08X\n"   \
                     "dwordAfterIPHeaderFirstFragment = %08X\n"       \
                     "FIRST_DWORD_AFTER_IP_HEADER(PacketHeader) = %08X\n",
                     dwordAfterIPHeaderThisFragment, 
                     dwordAfterIPHeaderFirstFragment, 
                     FIRST_DWORD_AFTER_IP_HEADER(PacketHeader)));
#endif
    }
#endif // __FILTER__

    return retcode;
}

#endif // if FFP_SUPPORT

