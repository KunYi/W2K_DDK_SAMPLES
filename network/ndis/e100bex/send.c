/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    send.c

This driver runs on the following hardware:
    - 82557/82558 based PCI 10/100Mb ethernet adapters
    (aka Intel EtherExpress(TM) PRO Adapters)

Environment:
    Kernel Mode - Or whatever is the equivalent on WinNT

Abstract:
    This module contains the send routines and send interrupt handling code

Revision History
    - JCB 8/14/97 Example Driver Created
*****************************************************************************/

#include "precomp.h"
#pragma hdrstop
#pragma warning (disable: 4514 4706)

//-----------------------------------------------------------------------------
// Procedure: D100MultipleSend
//
// Description:This function takes an array from NDIS and puts as many as it can
//             in our list to be immediately transferred. Each packet has its
//             status set (NDIS_STATUS_RESOURCES, NDIS_STATUS_PENDED,
//             or NDIS_STATUS_SUCCESS) in the PacketArray individually.
//
//
// Arguments: MiniportAdapterContext (Adapter Structure pointer)
//            PacketArray - an array of pointers to NDIS_PACKET structs
//            PacketCount - number of packets in PacketArray
//
// Returns: nothing
//
//-----------------------------------------------------------------------------
VOID
D100MultipleSend(NDIS_HANDLE MiniportAdapterContext,
                 PPNDIS_PACKET PacketArray,
                 UINT NumberOfPackets)
{
    PD100_ADAPTER       Adapter;
    NDIS_STATUS         Status;
    UINT                PacketCount;

    DEBUGFUNC("D100MultipleSend");

    Adapter = PD100_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    NdisAcquireSpinLock(&Adapter->Lock);
    TRACE2(Adapter, ("\n"));
    DEBUGCHAR(Adapter,'"');

    // take each packet in the array and return a status for it
    // if we return one packet with a NDIS_STATUS_RESOURCES,
    // the MiniPort layer assumes the rest of the packets have the same status
    // its up to the protocol to check those statuses after we return
    for(PacketCount=0;PacketCount < NumberOfPackets; PacketCount++)
    {
        // check for a zero pointer
        ASSERT(PacketArray[PacketCount]);

        // begin stub
        // NDIS 5
        // for Task Offloading the driver could handle a
        // packet with a request to offload the checksum
        // by doing something like the following.
        // NOTE: this code could probably be put somewhere deeper
        // into the transmit code.
        /******************************************************************************
        {
        NDIS_PACKET_EXTENSION               PktExt;
        NDIS_TCP_IP_CHECKSUM_PACKET_INFO    ChksumPktInfo;

          PktExt = NDIS_PACKET_EXTENSION_FROM_PACKET(Packet);

            if (PktExt->NdisPacketInfo[ProtocolIdPacketInfo] == NDIS_PROTOCOL_ID_TCP_IP)
            {
                ChksumPktInfo.Value = (ULONG)PktExt->NdisPacketInfo[TcpIpChecksumPacketInfo];
                if (ChksumPktInfo.Transmit.NdisPacketChecksumV4)
                    {
                    //
                    // Perform  the appropriate checksumming operations on this packet
                    // as was set by the miniport with OID_GEN_TASK_OFFLOAD.
                    //
                    }
            }
        }
        ******************************************************************************/

        // NDIS 4
        // NOTE THIS CODE WILL NOT WORK, IT IS JUST FOR REFERENCE
        // before we send this packet, we might check for OOB
        // data that would indicate a high priority send
        // and we might use that data to call a different send
        // routine. It would be best to encapsulate the priority query
        // into a separate routine, but for example purposes lets leave it here.
        /******************************************************************************
        {
        MEDIA_SPECIFIC_INFORMATION MediaInfo;
        UINT InfoSize;

          NDIS_GET_PACKET_MEDIA_SPECIFIC_INFO(PacketArray[PacketCount],(PVOID) &MediaInfo, &InfoSize);

            if (InfoSize
            && (MediaInfo.ClassId == NdisClass802_3Priority) )
            {
                // note: this routine could possibly send a normal priority packet
                Status = SetupPrioritySend(Adapter,PacketArray[PacketCount]);
            }


        }
        ******************************************************************************/
        // end stub
        Status = SetupNextSend(Adapter, PacketArray[PacketCount]);

        if (Status == NDIS_STATUS_RESOURCES)
        {
            DEBUGCHAR(Adapter,'Q');

            // Queue This packet for the Deserialized Miniport
            Adapter->NumPacketsQueued++;
            EnqueuePacket(Adapter->FirstTxQueue, Adapter->LastTxQueue, PacketArray[PacketCount]);
            Status = NDIS_STATUS_PENDING;
        }

        NDIS_SET_PACKET_STATUS(PacketArray[PacketCount], Status);

    }

    NdisReleaseSpinLock(&Adapter->Lock);
    return;
}



//-----------------------------------------------------------------------------
// Procedure:   SetupNextSend
//
// Description: This routine is called by D100Send.  It will setup all of the
//              necessary structures (TCBs, TBDs, etc.) to send a packet.  If
//              the device has the necessary resources, the packet will be
//              sent out on the active chain.  If the device doesn't have the
//              resources to send the packet, then the routine will return
//              NDIS_STATUS_RESOURCES.
//
//              The intention is to have this routine run only on a 82557
//              or a 82558 in compatibility mode (indicated by Enhanced = FALSE
//              in the registry)
// Arguments:
//      Adapter - ptr to Adapter object instance
//      Packet - A pointer to a descriptor for the packet that is to be
//               transmitted.
// Returns:
//      NDIS_STATUS_SUCCESS - We copied the entire packet into a host buffer,
//                            (either it was a short frame, or we coalesced
//                            the packet into a host buffer), so we can
//                            immediately return the buffer back to the upper
//                            layers.
//      NDIS_STATUS_PENDING - If we were able to acquire the necessary TBD's
//                            or Coalesce buffer for the packet.  This means
//                            that the device will send this packet soon.
//                            Eventually we'll return the packet back to the
//                            protocol stack by using the "SendComplete" call.
//      NDIS_STATUS_RESOURCES - We didn't have the resouces (TCBs or TBDs) to
//                              accept this packet. The NDIS packet should
//                              queue this packet, and give it back to us at a
//                              later time.
//-----------------------------------------------------------------------------

NDIS_STATUS
SetupNextSend(PD100_ADAPTER Adapter,
              PNDIS_PACKET Packet)
{
    PD100SwTcb          SwTcb;
    PTXCB_STRUC         HwTcb;
    NDIS_STATUS         Status;

    DEBUGFUNC("SetupNextSend");
    TRACE2(Adapter, ("\n"));


    //    DEBUGCHAR(Adapter,'T');
    // Attempt to acquire a Software TCB for the packet
    SwTcb = (PD100SwTcb) QueuePopHead(&Adapter->TxCBList);

    if (!(SwTcb))
    {
        TRACE2(Adapter, ("FailNoTcb\n"));

        // No TCBs available so return NO_RESOURCES
        Status = NDIS_STATUS_RESOURCES;
        return (Status);
    }

    // this next line tries to workaround the case where
    // on a D101 we can wrap our queue by setting the
    // 'S' bit on our head
    if (QueueEmpty(&Adapter->TxCBList))
    {
        TRACE2(Adapter,("Don't WRAP queue!!! Fail With Only 1 TCB left\n"));
        QueuePushHead(&Adapter->TxCBList, &SwTcb->Link);
        Status = NDIS_STATUS_RESOURCES;
        return(Status);
    }

    // Prepare the TCB for transmission of this packet
    if (PrepareForTransmit(Adapter, Packet, SwTcb))
    {

        // debug stuff
        TRACE3(Adapter, ("Assigning SwTcb %08x to 557\n", SwTcb));
        ASSERT(SwTcb);

        HwTcb = SwTcb->Tcb;
        HwTcb->TxCbHeader.CbStatus = 0;
        HwTcb->TxCbThreshold = (UCHAR) Adapter->AiThreshold;

        // If the packet is small then we don't use any TBDs, and we send
        // the packet in simplified mode.
        if (SwTcb->PacketLength <= MINIMUM_ETHERNET_PACKET_SIZE)
        {
            // Prep the hardware TCB.  This Tcb will not point to any TBDs
            // because the entire frame will be located in the TCB's data
            // area.
            HwTcb->TxCbHeader.CbCommand = CB_S_BIT | CB_TRANSMIT;
            HwTcb->TxCbTbdPointer = DRIVER_NULL;
            HwTcb->TxCbTbdNumber = 0;
            HwTcb->TxCbCount = (CB_TX_EOF_BIT | 0x003C);
        }

        // If the packet is not small then we do use TBDs, and thus send the
        // packet using flexible mode.
        else
        {
            // Prep the hardware TCB
            HwTcb->TxCbHeader.CbCommand = CB_S_BIT | CB_TRANSMIT | CB_TX_SF_BIT;
            HwTcb->TxCbTbdPointer = SwTcb->FirstTbdPhys;
            HwTcb->TxCbTbdNumber = (UCHAR) SwTcb->TbdsUsed;
            HwTcb->TxCbCount = 0;
        }

        // If the transmit unit is idle (very first transmit) then we must
        // setup the general pointer and issue a full CU-start
        if (Adapter->TransmitIdle)
        {
            TRACE2(Adapter, ("CU is idle -- First TCB added to Active List\n"));


            QueuePutTail(&Adapter->ActiveChainList, &SwTcb->Link);

            // Wait for the SCB to clear before we set the general pointer
            WaitScb(Adapter);

            // Don't try to start the transmitter if the command unit is not
            // idle ((not idle) == (Cu-Suspended or Cu-Active)).
            if ((Adapter->CSRAddress->ScbStatus & SCB_CUS_MASK) != SCB_CUS_IDLE)
            {
                TRACESTR(Adapter, ("CU Not IDLE\n"));
                ASSERT(0);
                NdisStallExecution(25);
            }

            Adapter->CSRAddress->ScbGeneralPointer = SwTcb->TcbPhys;

            D100IssueScbCommand(Adapter, SCB_CUC_START, FALSE);

            Adapter->TransmitIdle = FALSE;
            Adapter->ResumeWait = TRUE;
        }

        // If the command unit has already been started, then append this
        // TCB onto the end of the transmit chain, and issue a CU-Resume.
        else
        {
            TRACE2(Adapter, ("adding TCB to Active chain\n"));

            QueuePutTail(&Adapter->ActiveChainList, &SwTcb->Link);

            // Clear the suspend bit on the previous packet.
            SwTcb->PreviousTcb->TxCbHeader.CbCommand &= ~CB_S_BIT;

            // Issue a CU-Resume command to the device.  We only need to do a
            // WaitScb if the last command was NOT a RESUME.
            if (Adapter->ResumeWait)
            {
                if (!D100IssueScbCommand(Adapter, SCB_CUC_RESUME, TRUE))
                {
                    TRACESTR(Adapter, ("CU-resume failed\n"));
                }
            }
            else
                D100IssueScbCommand(Adapter, SCB_CUC_RESUME, FALSE);
        }


        // return NDIS_STATUS_PENDING and then later make a "SendComplete" call
        // to return the packet the protocol stack.
        Status = NDIS_STATUS_PENDING;
#if DBG
        Adapter->txsent++;
#endif
    }


    // If PrepareForTransmit didn't succeed, then put the TCB back on the free
    // list, and return the proper error code
    else
    {
        if ((!SwTcb->PacketLength) || (SwTcb->PacketLength > MAXIMUM_ETHERNET_PACKET_SIZE))
            Status = NDIS_STATUS_INVALID_PACKET;

        else
            Status = NDIS_STATUS_RESOURCES;

        // If prepare failed, then put the TCB back on queue because
        // we don't have an available coalesce buffer
        QueuePushHead(&Adapter->TxCBList, &SwTcb->Link);
    }

    //    DEBUGCHAR(Adapter,'t');

    return (Status);
}


//-----------------------------------------------------------------------------
// Procedure:   PrepareForTransmit
//
// Description: This routine will Prepare a software TCB, and a set
//              of linked TBDs, for the packet that is passed in.  When this
//              routine returns, this packet will be ready to send through the
//              adapter, and onto the medium.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//      Packet - A pointer to a descriptor for the packet that is to be
//               transmitted.
//      SwTcb - Pointer to a software structure that represents a hardware TCB.
//
// Returns:
//      TRUE If we were able to acquire the necessary TBD's or Coalesce buffer
//           for the packet in we are attempting to prepare for transmission.
//      FALSE If we needed a coalesce buffer, and we didn't have any available.
//-----------------------------------------------------------------------------

BOOLEAN
PrepareForTransmit(PD100_ADAPTER Adapter,
                   PNDIS_PACKET Packet,
                   PD100SwTcb SwTcb)

{
    PNDIS_BUFFER                CurrBuff;
    UINT                        i;
    UINT                        BytesCopied;
    PCOALESCE                   Coalesce;
    PTBD_STRUC                  LastTbd = (PTBD_STRUC) 0;
    PNDIS_PHYSICAL_ADDRESS_UNIT p;
    UINT                        ArrayIndex = 0;

    // Debug Code
    DEBUGFUNC("PrepareForTransmit");
    TRACE2(Adapter, ("\n"));

    TRACE3(Adapter, ("Preparing to transmit SwTcb %08x Packet %08x\n", SwTcb, Packet));

    ASSERT(SwTcb);

    //    DEBUGCHAR(Adapter,'U');
    // Assign the packet
    SwTcb->Packet = Packet;

    // Init some variables in the SwTcb
    SwTcb->MapsUsed = 0;
    SwTcb->NumPhysDesc = 0;
    SwTcb->TbdsUsed = 0;
    SwTcb->Coalesce = (PCOALESCE) 0;
    SwTcb->CoalesceBufferLen = 0;

    // Get a virtual buffer count and packet length.
    NdisQueryPacket(SwTcb->Packet,
        &SwTcb->NumPhysDesc,
        &SwTcb->BufferCount,
        &SwTcb->FirstBuffer,
        &SwTcb->PacketLength);

    ASSERT(SwTcb->FirstBuffer);
    ASSERT(SwTcb->BufferCount);
    ASSERT(SwTcb->NumPhysDesc);
    ASSERT(SwTcb->PacketLength);
    ASSERT(SwTcb->PacketLength <= 1514);
    ASSERT(SwTcb->BufferCount <= 512);
    ASSERT(SwTcb->NumPhysDesc <= 512);

    // if the packet is not a valid length, then error out
    if ((!SwTcb->PacketLength) || (SwTcb->PacketLength > MAXIMUM_ETHERNET_PACKET_SIZE))
        return FALSE;

    // start with the first buffer
    CurrBuff = SwTcb->FirstBuffer;


#if DBG
    SwTcb->BufferCountCheck = 0;
    SwTcb->NumPhysDescCheck = SwTcb->NumPhysDesc;
#endif

    // If the packet is less than minimum size, then we'll just copy the packet
    // into the data portion of the TCB.  This means that we won't be acquiring
    // any TBD's.  We also won't need to get a coalesce buffer.
    if (SwTcb->PacketLength <= MINIMUM_ETHERNET_PACKET_SIZE)
    {
        TRACE2(Adapter, ("short packet\n"));
        CurrBuff = 0;
    }

    // If there are too many physical mappings, try to get a coalesce buffer.
    // We do this because its actually faster for us to copy many fragments
    // into a big buffer and give that big buffer to the adapter, than to have
    // the hardware fetch numerous small fragments (this often leads to
    // underruns). The NDIS tester often asks us to send packets that
    // have more than 30 physical components. This method also saves TBD resources.

    else if (SwTcb->NumPhysDesc > Adapter->NumTbdPerTcb)
    {
        // Debug Code
        ASSERT(!SwTcb->Coalesce);
        TRACE2(Adapter,
            ("Failed-> Physical descriptors %d > num Tbd per Tcb %d\n",
            SwTcb->NumPhysDesc, Adapter->NumTbdPerTcb));

        // Try to get the coalesce buffer
        if (!AcquireCoalesceBuffer(Adapter, SwTcb))
            return (FALSE);
        CurrBuff = 0;
    }

    //  Clear NumPhysDesc.
    SwTcb->NumPhysDesc = 0;



    // If we are not coalescing, and the frame is not short, then for each
    // virtual buffer, get the physical components.  We'll need these physical
    // attributes so that we can instruct the adapter to copy the fragments
    // across the bus and into the adapter's internal FIFO.
    for (i = 0; CurrBuff; i++)
    {
        UINT    ArraySize = 0;

        // If the mapping is successful, and there are not too many, then this
        // is where they go.
        PNDIS_PHYSICAL_ADDRESS_UNIT pUnit = &SwTcb->PhysDesc[SwTcb->NumPhysDesc];

        // Decompose the virtual buffer into one or more physical buffers.
        NdisMStartBufferPhysicalMapping(Adapter->D100AdapterHandle,
            CurrBuff,
            Adapter->NextFreeMapReg,
            TRUE,
            Adapter->pUnits,
            &ArraySize);

#if DBG
        if (ArraySize == 0)
        {
            //            TRACESTR(Adapter, ("ZERO Array size -- Buffer %d ArraySize %d\n", i, ArraySize));
            ASSERT(CurrBuff);
        }
#endif

        // Adjust free map register variables.  Since map registers are such
        // a precious system resource we dynamically use them when necessary,
        // rather than allocate so many map registers per TCB.
        Adapter->NextFreeMapReg++;

        // check for wrap condition
        if (Adapter->NextFreeMapReg == Adapter->NumMapRegisters)
            Adapter->NextFreeMapReg = 0;

        // Debug code
        TRACE3(Adapter, ("Buffer %d ArraySize %d\n", i, ArraySize));

        // Mark the number of valid buffer mappings and copy
        // the new mappings into the transmit descriptor.
        SwTcb->MapsUsed++;
        NdisMoveMemory((PVOID) pUnit,
            (PVOID) Adapter->pUnits,
            ArraySize * sizeof(NDIS_PHYSICAL_ADDRESS_UNIT));

        SwTcb->NumPhysDesc += ArraySize;

        // Flush the current buffer because it could be cached
        NdisFlushBuffer(CurrBuff, TRUE);

        // point to the next buffer
        NdisGetNextBuffer(CurrBuff, &CurrBuff);

#if DBG
        SwTcb->BufferCountCheck++;
        ASSERT(SwTcb->BufferCountCheck <= SwTcb->BufferCount);
#endif


    }

    // ------------------------------- NOTE: ----------------------------------
    // At this point, we should have either locked down each physical fragment
    // through the use of map registers, or we should have a coalesce buffer,
    // or we should have a short packet that we'll copy into the TCB data area.


#if DBG
    // Check to make sure that our buffer count was valid
    if (SwTcb->MapsUsed)
    {
        ASSERT(SwTcb->BufferCountCheck == SwTcb->BufferCount);
    }
#endif

    // Check for coalesce buffer
    if (SwTcb->Coalesce)
    {
        ASSERT(SwTcb->CoalesceBufferLen);
        ASSERT(!SwTcb->NumPhysDesc);
        ASSERT(!SwTcb->MapsUsed);

        Coalesce = SwTcb->Coalesce;
        ASSERT(Coalesce->OwningTcb == (PVOID) SwTcb);

        // Copy the packet into the coalesce buffer
        ASSERT(SwTcb->PacketLength <= MAXIMUM_ETHERNET_PACKET_SIZE);

        // Copy all of the packet data to our coalesce buffer
        D100CopyFromPacketToBuffer(Adapter,
            SwTcb->Packet,
            SwTcb->PacketLength,
            (PCHAR) Coalesce->CoalesceBufferPtr,
            SwTcb->FirstBuffer,
            &BytesCopied);


        ASSERT(BytesCopied == SwTcb->PacketLength);

        SwTcb->TbdsUsed = 1;


    }

    // Check if we a doing a fragmented send with TBDs
    else if (SwTcb->NumPhysDesc)
    {
        SwTcb->TbdsUsed = SwTcb->NumPhysDesc;
        ASSERT(!SwTcb->CoalesceBufferLen);
        ASSERT(SwTcb->MapsUsed);
    }

    // check if we are using the TCB's data area with no TBDs.  This should
    // only happen on frames that are <= the minimum ethernet length
    else if ((!SwTcb->NumPhysDesc) && (!SwTcb->Coalesce))
    {
        ASSERT(!SwTcb->CoalesceBufferLen);
        ASSERT(!SwTcb->NumPhysDesc);
        ASSERT(!SwTcb->MapsUsed);

        ASSERT(SwTcb->PacketLength <= MINIMUM_ETHERNET_PACKET_SIZE);

        // Copy the packet into the immediate data portion of the TCB.
        D100CopyFromPacketToBuffer(Adapter,
            SwTcb->Packet,
            SwTcb->PacketLength,
            (PCHAR) &SwTcb->Tcb->TxCbData,
            SwTcb->FirstBuffer,
            &BytesCopied);


        ASSERT(BytesCopied == SwTcb->PacketLength);

        //        // Check for below minimum length packets.
        //        if (SwTcb->PacketLength < MINIMUM_ETHERNET_PACKET_SIZE)
        //        {
        //            NdisZeroMemory(
        //                    ((PCHAR) &SwTcb->Tcb->TxCbData) + SwTcb->PacketLength,
        //                    MINIMUM_ETHERNET_PACKET_SIZE - (SwTcb->PacketLength));
        //        }

    }


    // ------------------------------- NOTE: ----------------------------------
    // At this point, any copying of of data into a coalesce buffer or into the
    // the TCB itself should be complete.  Now the last thing left to do in
    // this routine is to setup the TBD array if TBDs are being used for this
    // particular transmit frame.

    for (i = 0; i < SwTcb->TbdsUsed; i++)
    {
        PTBD_STRUC  Tbd;
        ULONG       OriginalBufferAddress, OriginalBufferCount;

        // Setup pointer to the particular TBD
        Tbd = SwTcb->FirstTbd + i;
        TRACE3(Adapter, ("Setting TBD num %d at virtual addr %x\n", i, Tbd));

        // Assign the physical address of the transmit buffer, and the
        // TBD count to the TCB.
        if (SwTcb->Coalesce)
        {
            Tbd->TbdBufferAddress = Coalesce->CoalesceBufferPhys;
            Tbd->TbdCount = (ULONG) SwTcb->CoalesceBufferLen;
        }
        else
        {
            p = &SwTcb->PhysDesc[ArrayIndex];
            Tbd->TbdBufferAddress = NdisGetPhysicalAddressLow(p->PhysicalAddress);
            Tbd->TbdCount = (ULONG) p->Length;

            ASSERT(p->Length <= 0x00003fff);
            TRACE3(Adapter, ("Buff %d len %d phys %08x\n", ArrayIndex, p->Length, Tbd->TbdBufferAddress));

        }
        ArrayIndex++;

    }

    //    DEBUGCHAR(Adapter,'u');

    return (TRUE);
}

//-----------------------------------------------------------------------------
// Procedure:   AcquireCoalesceBuffer
//
// Description: This routine will attempt to acquire a coalesce buffer, if
//              there were too many physical vectors for a single packet.
//              The coalesce buffer will allow us to double buffer using a
//              only a single destination buffer.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//      SwTcb - Pointer to a software structure that represents a hardware TCB.
//
// Returns:
//      TRUE If we were able to acquire the necessary Coalesce buffer.
//      FALSE If we were not able to get a coalesce buffer.
//-----------------------------------------------------------------------------

BOOLEAN
AcquireCoalesceBuffer(PD100_ADAPTER Adapter,
                      PD100SwTcb SwTcb
                      )
{
    DEBUGFUNC("AcquireCoalesceBuffer");
    TRACE2(Adapter, ("\n"));

    // Debug code.
    ASSERT(SwTcb);
    ASSERT(!SwTcb->Coalesce);
    ASSERT(!SwTcb->CoalesceBufferLen);
    //    ASSERT(!SwTcb->MapsUsed);

    // Check preconditions for use. Return FALSE if there are none
    // available.
    if (QueueEmpty(&Adapter->CoalesceBufferList))
    {
        TRACE2(Adapter, ("No free coalesce buffers!!!\n"));
        return (FALSE);
    }

    // Allocate the coalesce buffer from the list. The SwTcb will now
    // own it until it's returned to the list.

    SwTcb->Coalesce = (PCOALESCE) QueuePopHead(&Adapter->CoalesceBufferList);

    SwTcb->Coalesce->OwningTcb = (PVOID) SwTcb;
    SwTcb->CoalesceBufferLen = SwTcb->PacketLength;

    TRACE2(Adapter, ("Acquired CoalesceBuffer %08x \n", SwTcb->Coalesce));
    return (TRUE);
}


//-----------------------------------------------------------------------------
// Procedure:   D100CopyFromPacketToBuffer
//
// Description: This routine will copy a packet to a the passed buffer (which
//              in this case will be a coalesce buffer).
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//      Packet - The packet to copy from.
//      BytesToCopy - The number of bytes to copy from the packet.
//      DestBuffer - The destination of the copy.
//      FirstBuffer - The first buffer of the packet that we are copying from.
//
// Result:
//      BytesCopied - The number of bytes actually copied
//
// Returns:
//-----------------------------------------------------------------------------

VOID
D100CopyFromPacketToBuffer(  PD100_ADAPTER Adapter,
                           PNDIS_PACKET Packet,
                           UINT BytesToCopy,
                           PCHAR DestBuffer,
                           PNDIS_BUFFER FirstBuffer,
                           PUINT BytesCopied)

{
    PNDIS_BUFFER    CurrentBuffer;
    PVOID           VirtualAddress;
    UINT            CurrentLength;
    UINT            AmountToMove;

    *BytesCopied = 0;
    if (!BytesToCopy)
        return;

    CurrentBuffer = FirstBuffer;

    NdisQueryBuffer(
        CurrentBuffer,
        &VirtualAddress,
        &CurrentLength);

    while (BytesToCopy)
    {
        while (!CurrentLength)
        {
            NdisGetNextBuffer(
                CurrentBuffer,
                &CurrentBuffer);

            // If we've reached the end of the packet.  We return with what
            // we've done so far (which must be shorter than requested).
            if (!CurrentBuffer)
                return;

            NdisQueryBuffer(
                CurrentBuffer,
                &VirtualAddress,
                &CurrentLength);
        }


        // Compute how much data to move from this fragment
        if (CurrentLength > BytesToCopy)
            AmountToMove = BytesToCopy;
        else
            AmountToMove = CurrentLength;

        // Copy the data.
        NdisMoveMemory(DestBuffer, VirtualAddress, AmountToMove);

        // Update destination pointer
        DestBuffer = (PCHAR) DestBuffer + AmountToMove;

        // Update counters
        *BytesCopied +=AmountToMove;
        BytesToCopy -=AmountToMove;
        CurrentLength = 0;
    }
}


//-----------------------------------------------------------------------------
// Procedure:   ProcessTXInterrupt
//
// Description: This routine is called by HandleInterrupt to process transmit
//              interrupts for D100 adapters.  Basically, this routine will
//              remove any completed packets from the active transmit chain,
//              append them to the completed list.  TransmitCleanup will be
//              called to free up the TCBs, map registers, etc, that the
//              packets on the completed list consumed.  At the end of this
//              routine, the MAC version of this driver will try to send any
//              packets that it queued because of an earlier lack of resources.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Return:
//      TRUE - If we indicated any loopback packets during this function call
//      FALSE - If we didn't indicate any loopaback packets
//-----------------------------------------------------------------------------

BOOLEAN
ProcessTXInterrupt(PD100_ADAPTER Adapter)

{
    PD100SwTcb          SwTcb;
    BOOLEAN             Status = FALSE;
    NDIS_STATUS         SendStatus;

    DEBUGFUNC("ProcessTXInterrupt");
    TRACE2(Adapter, ("\n"));


    DEBUGCHAR(Adapter,'X');
    // If we don't have an active transmit chain, AND we haven't pended any transmits,
    // then we don't have anything to clean-up.
    if (QueueEmpty(&Adapter->ActiveChainList) && !Adapter->FirstTxQueue)
    {
        DEBUGCHAR(Adapter,'z');
        return FALSE;
    }


    // Look at the TCB at the head of the queue.  If it has been completed
    // then pop it off and place it at the tail of the completed list.
    // Repeat this process until all the completed TCBs have been moved to the
    // completed list
    while (SwTcb = (PD100SwTcb) QueueGetHead(&Adapter->ActiveChainList))
    {
        // check to see if the TCB has been DMA'd
        if (SwTcb->Tcb->TxCbHeader.CbStatus & CB_STATUS_COMPLETE)
        {
            DEBUGCHAR(Adapter,'c');

            TRACE3(Adapter, ("Found a completed TCB\n"));

            // Remove the TCB from the active queue.
            SwTcb = (PD100SwTcb) QueuePopHead(&Adapter->ActiveChainList);

            // Put the TCB on the completed queue.
            QueuePutTail(&Adapter->CompletedChainList, &SwTcb->Link);
        }
        else
            break;
    }

    // Cleanup after the transmits that have already been sent -- free their
    // TCBs, map registers, and coalesce buffers.
    if (!QueueEmpty(&Adapter->CompletedChainList))
        Status = TransmitCleanup(Adapter);

    // If we queued any transmits because we didn't have any TCBs earlier,
    // dequeue and send those packets now, as long as we have free TCBs.
    while ((Adapter->FirstTxQueue) &&
        ((PD100SwTcb) QueueGetHead(&Adapter->TxCBList)))
    {
        PNDIS_PACKET QueuePacket;

        // If any packets are in the queue, dequeue it, send it, and
        // acknowledge success.
        QueuePacket = Adapter->FirstTxQueue;
        Adapter->NumPacketsQueued--;
        DequeuePacket(Adapter->FirstTxQueue, Adapter->LastTxQueue);

        DEBUGCHAR(Adapter,'q');

        // Attempt to put it onto the hardware
        SendStatus = SetupNextSend(Adapter, QueuePacket);

        // If there were no resources for this packet, then we'll just have
        // to try and send it later.
        if (SendStatus == NDIS_STATUS_RESOURCES)
        {
            // re-queue the packet
            Adapter->NumPacketsQueued++;
            EnqueuePacket(Adapter->FirstTxQueue, Adapter->LastTxQueue, QueuePacket);

            DEBUGCHAR(Adapter,'Q');

            break;
        }
    }

    DEBUGCHAR(Adapter,'x');
    return Status;
}


//-----------------------------------------------------------------------------
// Procedure:   TransmitCleanup
//
// Description: This routine will clean up after a transmitted frame.  It will
//              update the transmit statistic counters, free up TCBs and map
//              regs, and issue a send complete which will unlock the pages.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      TRUE - If we indicated any loopback packets during this function call
//      FALSE - If we didn't indicate any loopaback packets
//-----------------------------------------------------------------------------

BOOLEAN
TransmitCleanup(PD100_ADAPTER Adapter)
{
    PD100SwTcb          SwTcb;
    PNDIS_PACKET        Packet;
    PNDIS_BUFFER        CurrBuff;
    BOOLEAN             Status = FALSE;
    BOOLEAN             DoSendComplete = TRUE;
    INT                 MapRegToFree;

    DEBUGFUNC("TransmitCleanup");
    TRACE2(Adapter, ("\n"));

    DEBUGCHAR(Adapter,'_');
    // If the completed transmit list is empty, then we have nothing to do here
    while (!QueueEmpty(&Adapter->CompletedChainList))
    {
        // Get the first entry in the completed list.
        SwTcb = (PD100SwTcb) QueuePopHead(&Adapter->CompletedChainList);

        ASSERT(SwTcb);

        // PACKET is nothin when the element being processed is a MC Command
        Packet = SwTcb->Packet;

        TRACE3(Adapter, ("Processing SwTcb %08x, Packet %08x\n", SwTcb, Packet));

        // Free a coalesce buffer (if there were any)
        if (SwTcb->Coalesce)
        {
            // debug checks
            ASSERT(SwTcb->CoalesceBufferLen);
            //            ASSERT(!SwTcb->NumPhysDesc);
            //            ASSERT(!SwTcb->MapsUsed);

            ASSERT(SwTcb->Coalesce->OwningTcb == (PVOID) SwTcb);

            // Return the coalesce buffer back to the list
            QueuePutTail(&Adapter->CoalesceBufferList, &SwTcb->Coalesce->Link);

            // Clear the coalesce buffer pointer
            SwTcb->Coalesce = (PCOALESCE) 0;
            SwTcb->CoalesceBufferLen = 0;
        }

        // unlock all of the pages held by this packet
        if (SwTcb->MapsUsed > 0)
        {
            // start with the first buffer
            CurrBuff = SwTcb->FirstBuffer;

            // free the map register associated with each buffer
            while (CurrBuff)
            {

                // free a map register
                MapRegToFree = Adapter->OldestUsedMapReg;

                Adapter->OldestUsedMapReg++;

                if (Adapter->OldestUsedMapReg == Adapter->NumMapRegisters)
                    Adapter->OldestUsedMapReg = 0;

#if DBG
                SwTcb->MapsUsed--;
#endif

                // Release the map register and its associated physical mapping

                NdisMCompleteBufferPhysicalMapping(
                    Adapter->D100AdapterHandle,
                    CurrBuff,
                    MapRegToFree);

                // Get the next buffer that needs the be "de-mapped"
                NdisGetNextBuffer(CurrBuff, &CurrBuff);
            }

#if DBG
            // This counter gets decremented each time a map reg is freed,
            // under the debug build.  Under the free build, we just zero
            // the counter at the end of the loop.
            ASSERT(SwTcb->MapsUsed == 0);
#endif
        }

        //  Clear NumPhysDesc, and NumMapsUsed.
        SwTcb->NumPhysDesc =
            SwTcb->MapsUsed = 0;

        // Free the TCB for the given frame
        TRACE3(Adapter, ("Releasing SwTcb %08x\n", SwTcb));

        QueuePutTail(&Adapter->TxCBList, &SwTcb->Link);

        // If this wasn't a multicast command, then we need to check to see
        // if we need to issue send complete
        if ((SwTcb->Tcb->TxCbHeader.CbCommand & CB_CMD_MASK) != CB_MULTICAST)
        {
#if DBG
            Adapter->txind++;
#endif
            DEBUGCHAR(Adapter,'-');
            // If we originally returned NDIS_STATUS_PENDING on this packet, then
            // we need to tell the protocol that we are finished with the packet
            // now.  If we originally returned NDIS_STATUS_SUCCESS, then we don't
            // need to make the SendComplete call, because the protocol will have
            // already freed or re-used the "packet".
            if (DoSendComplete == TRUE)
            {

                NdisReleaseSpinLock(&Adapter->Lock);
                // Do a send Complete for this frame
                NdisMSendComplete(
                    Adapter->D100AdapterHandle,
                    Packet,
                    NDIS_STATUS_SUCCESS);

                NdisAcquireSpinLock(&Adapter->Lock);
            }
        }
    }

    DEBUGCHAR(Adapter,'=');
    return Status;
}

