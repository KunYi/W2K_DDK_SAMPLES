/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    interrup.c

This driver runs on the following hardware:
    - 82557/82558 based PCI 10/100Mb ethernet adapters
    (aka Intel EtherExpress(TM) PRO Adapters)

Environment:
    Kernel Mode - Or whatever is the equivalent on WinNT

Revision History
    - JCB 8/14/97 Example Driver Created
*****************************************************************************/

#include "precomp.h"
#pragma hdrstop
#pragma warning (disable: 4514 4706)

//-----------------------------------------------------------------------------
// Procedure:   D100Isr (miniport)
//
// Description: This is the interrupt service routine.  It will check to see
//              if there are any interrupts pending, and if there are, it will
//              disable board interrupts and schedule a HandleInterrupt
//              callback.
//
// Arguments:
//  MiniportAdapterContext - The context value returned by the Miniport
//                           when the adapter was initialized (see the call
//                           NdisMSetAttributes). In reality, it is a
//                           pointer to D100_ADAPTER.
//
// Result:
//  InterruptRecognized - Returns True if the interrupt belonged to this
//                        adapter, and false otherwise.
//  QueueMiniportHandleInterrupt - Returns True if we want a callback to
//                                 HandleInterrupt.
//
// Returns: (none)
//-----------------------------------------------------------------------------

VOID
D100Isr(
        OUT PBOOLEAN InterruptRecognized,
        OUT PBOOLEAN QueueMiniportHandleInterrupt,
        IN NDIS_HANDLE MiniportAdapterContext
        )

{
    PD100_ADAPTER Adapter = PD100_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    // We want to process the interrupt if the output from our interrupt line
    // is high, and our interrupt is not masked.  If our interrupt line
    // is already masked, then we must be currently processing interrupts.
    if ((!(Adapter->CSRAddress->ScbCommandHigh & SCB_INT_MASK)) &&
        (Adapter->CSRAddress->ScbStatus & SCB_ACK_MASK))
    {
        //        DEBUGSTR(("Our INT -- slot %x\n", Adapter->AiSlot));
        *InterruptRecognized = TRUE;
        *QueueMiniportHandleInterrupt = TRUE;
        D100DisableInterrupt(Adapter);
    }
    else
    {
        *InterruptRecognized = FALSE;
        *QueueMiniportHandleInterrupt = FALSE;
    }

    return;
}

//-----------------------------------------------------------------------------
// Procedure:   D100HandleInterrupt
//
// Description: This routine is queued by the ISR when some interrupt
//              processing needs to be done.  It's main job is to call the
//              interrupt processing code (process receives, cleanup completed
//              transmits, etc). It will only be called with the adapter's
//              interrupts masked. This is the DPC for this driver.
//
// Arguments:
//  MiniportAdapterContext (miniport) - The context value returned by the
//                           Miniport when the adapter was initialized (see the
//                           call NdisMSetAttributes). In reality, it is a
//                           pointer to D100_ADAPTER.
//
//  Context (mac) - The context value returned by the Mac when the adapter was
//            initialized. In reality, it is a pointer to D100_ADAPTER.
//
// Returns: (none)
//-----------------------------------------------------------------------------

VOID
D100HandleInterrupt(
                    IN NDIS_HANDLE MiniportAdapterContext
                    )

{
    // Context is actually our adapter
    PD100_ADAPTER Adapter = PD100_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    volatile USHORT      AckCommand;  // PLC: changed white space

    // dpcloopcount can be changed to other values
    // to optimize for hardware/protocol timing differences
    USHORT      DPCLoopCount = 2;

    DEBUGFUNC("D100HandleInterrupt");

    NdisAcquireSpinLock(&Adapter->Lock);

    TRACE2(Adapter, ("\n"));
    TRACE2(Adapter, ("D100HandleInterrupt for slot %x\n", Adapter->AiSlot));
    DEBUGCHAR(Adapter,'H');

    // Make no more than 'loopcount' loops through the interrupt processing
    // code.  We don't want to loop forever in handle interrupt if we have
    // interrupt processing to do, because this might lead to starvation
    // problems with certain protocols if we are receiving data constantly.

    while (DPCLoopCount)
    {
        // Check the interrupt status bits of the SCB, and see why we were
        // interrupted.
        TRACE3(Adapter, ("ScbStatus %04x\n", Adapter->CSRAddress->ScbStatus));

        // Strip off the ACK bits.
        AckCommand = (USHORT) (Adapter->CSRAddress->ScbStatus & SCB_ACK_MASK);

        // If there are no interrupts to process, then exit loop
        if (!AckCommand)
            break;

        // Ack all pending interrupts now
        Adapter->CSRAddress->ScbStatus = AckCommand;

        // Go handle receive events
        ProcessRXInterrupt(Adapter);

        // Cleanup transmits
        ProcessTXInterrupt(Adapter);

        // Start the receive unit if it had stopped
        StartReceiveUnit(Adapter);

        DPCLoopCount--;
    }

    DEBUGCHAR(Adapter,'h');

    NdisReleaseSpinLock(&Adapter->Lock);
    return;
}

//-----------------------------------------------------------------------------
// Procedure:   ProcessRXInterrupt
//
// Description: This routine will indicate any received packets to the NDIS
//              wrapper.  This routine is architected so that we will indicate
//              all of the completed receive frames that we have in our buffer,
//              before we exit the routine.  If we receive any errored frames,
//              those frames are dropped by the driver, and not indicated to
//              the ndis wrapper.
//
// Arguments:
//  Adapter - ptr to Adapter object instance
//
// Returns:
//      TRUE if a receive was indicated
//      FALSE if a receive was not indicated
//-----------------------------------------------------------------------------

BOOLEAN
ProcessRXInterrupt(
                   IN PD100_ADAPTER Adapter
                   )
                   
{

    PD100SwRfd      SwRfd;
    PD100SwRfd      LastRfd;
    PRFD_STRUC      Rfd;
    PNDIS_PACKET    PacketArray[MAX_NUM_ALLOCATED_RFDS];
    UINT            PacketArrayCount;
    UINT            PacketFreeCount;
    UINT            i;
    BOOLEAN         ContinueToCheckRFDs=FALSE;
    DEBUGFUNC("ProcessRxInterrupt")
    
    TRACE2(Adapter, ("\n"));
    
    DEBUGCHAR(Adapter,'R');
    
    do
    {
        PacketArrayCount = 0;
        PacketFreeCount = 0;
        
        // We stay in the while loop until we have processed all pending receives.
        while (1)
        {
            if (QueueEmpty(&Adapter->RfdList))
            {
                // This should never happen because we limit the number of 
                // receive buffers that the protocol can take from us
                DEBUGSTR(("Receive buffers went to 0!, numused=%d\n",Adapter->UsedRfdCount));
                ASSERT(0);
                ContinueToCheckRFDs = FALSE; // we don't need to looptop any more
                break;
            }
            // Get the next unprocessed RFD
            SwRfd = (PD100SwRfd) QueueGetHead(&Adapter->RfdList);
            ASSERT(SwRfd);
            
            // Get the pointer to our "hardware" Rfd.
            Rfd = SwRfd->Rfd;
            ASSERT(Rfd);
            
            // If the RFD does not have its complete bit set, then bail out.
            if (!((SwRfd->Status = Rfd->RfdCbHeader.CbStatus) & RFD_STATUS_COMPLETE))
            {
                ContinueToCheckRFDs = FALSE; // we don't need to looptop any more
                break;
            }
            
            // Remove the RFD from the head of the List
            QueueRemoveHead(&Adapter->RfdList);

            // Get the packet length
            SwRfd->FrameLength = ((Rfd->RfdActualCount) & 0x3fff);
            
            ASSERT(SwRfd->FrameLength <= MAXIMUM_ETHERNET_PACKET_SIZE);
            TRACE3(Adapter, ("Received packet length %d\n", SwRfd->FrameLength));
            
            // Check the Status of this RFD.  Don't indicate if the frame wasn't
            // good.
            if (!(SwRfd->Status & RFD_STATUS_OK))
            {
                TRACE2(Adapter, ("Receive error!!!\n"));
                // looks like there might have been a bug here,
                // we're not doing anything useful if we got a receive error
                // so clean up the bad packet, and keep going
                InitAndChainPacket(Adapter,SwRfd);

                continue;
            }
            
            // Indicate the good packet to the NDIS wrapper
            else
            {
                ASSERT(SwRfd->Status & RFD_STATUS_OK);
                ASSERT((Rfd->RfdActualCount & 0xc000) == 0xc000);
                
                // Adjust our buffer length for this swrfd
                NdisAdjustBufferLength((PNDIS_BUFFER) SwRfd->ReceiveBuffer,
                    (UINT) SwRfd->FrameLength);
                ASSERT(SwRfd->FrameLength == (UINT)(Rfd->RfdActualCount & 0x3fff));
                
                // we dont do recalculatepacketcounts because we only 
                // have one buffer and its count is always correct
                // we could, however, do it if we were paranoid
                // or supported multiple receive buffers per packet
                //  NdisRecalculatePacketCounts(SwRfd->ReceivePacket);

                PacketArray[PacketArrayCount] = SwRfd->ReceivePacket;
                
                // keep track of how many we've used
                Adapter->UsedRfdCount++;
                ASSERT(Adapter->UsedRfdCount <= Adapter->NumRfd);
                
                // set the status on the packet, either resources or success
                if (Adapter->UsedRfdCount < Adapter->NumRfd - MIN_NUM_RFD)
                {
                    NDIS_SET_PACKET_STATUS(PacketArray[PacketArrayCount],NDIS_STATUS_SUCCESS);
                    // Keep track of the highest array index in which a packet
                    // with status NDIS_STATUS_SUCCESS is found. Once we hit the low
                    // resource condition, all further packets will be marked with status
                    // NDIS_STATUS_RESOURCES. This is because we hold the adapter lock
                    // here which prevents UsedRfdCount/NumRfd from being modified
                    // elsewhere.
                }
                else
                {
                    NDIS_SET_PACKET_STATUS(PacketArray[PacketArrayCount],
                        NDIS_STATUS_RESOURCES);
                        
                    PacketFreeCount++;
                    
                    // okay, we ran low on resources so allocate a bunch more
                    if ((Adapter->Last_RMD_used != (NUM_RMD - 1))
                        && (Adapter->AsynchronousAllocationPending == FALSE))
                    {
                        NDIS_STATUS     AllocationStatus;
                        
                        AllocationStatus = NdisMAllocateSharedMemoryAsync(
                            Adapter->D100AdapterHandle, 
                            packet_count[Adapter->Last_RMD_used] * sizeof(RFD_STRUC),
                            FALSE,
                            (PVOID) Adapter->Last_RMD_used);            
                        
                        if (AllocationStatus != NDIS_STATUS_PENDING)
                        {
                            INITSTR(("Async memory allocation not possible at this time!!!"));
                        }
                        else
                        {
                            // set this state variable so if we keep on receiving
                            // before our new memory is set up, we wont repeatedly
                            // call our allocate function
                            Adapter->AsynchronousAllocationPending = TRUE;
                        }
                        
                    }
                }
                
                // have to increment our packetcount
                PacketArrayCount++;
                
                // this limits the number of packets that we will indicate
                // to MAX_ARRAY_RECEIVE_PACKETS or the available number of RFDS
                if (( PacketArrayCount >= MAX_ARRAY_RECEIVE_PACKETS) ||
                    ( Adapter->UsedRfdCount == Adapter->NumRfd))
                {
                    ContinueToCheckRFDs = TRUE;
                    break; 
                }
                
            }
            
        }// end while 1
        
        // if we didn't process any receives, just return from here
        if (0 == PacketArrayCount)
        {
            DEBUGCHAR(Adapter,'g');
            return(FALSE);
        }
        
    #if DBG
        Adapter->IndicateReceivePacketCounter++;
        Adapter->PacketsIndicated+=PacketArrayCount;
    #endif    
        ////////////////////////////////////////////////////////////////
        // Indicate the packets and return the completed ones to our
        // receive list. If the packet is pended NDIS gets to keep it
        // and will return it with D100GetReturnedPackets.
        // NDIS should always leave us at least MIN_NUM_RFD packets
        // because we set Status_resources on our last MIN_NUM_RFD packets
        ////////////////////////////////////////////////////////////////
        if(PacketArrayCount)
        {
            NdisReleaseSpinLock(&Adapter->Lock);
            NdisMIndicateReceivePacket(Adapter->D100AdapterHandle,
                PacketArray,
                PacketArrayCount);
            
            NdisAcquireSpinLock(&Adapter->Lock);
            
        }

        //
        // NDIS will call our ReturnPackets handler for each packet
        // that was *not* marked NDIS_STATUS_RESOURCES. For those
        // that were marked with the resources status, the miniport
        // should assume that they are immediately returned.
        // Go through the packets that were marked NDIS_STATUS_RESOURCES
        // and reclaim them.
        //
        
        for (i=PacketArrayCount-PacketFreeCount;i<PacketArrayCount ;i++ )
        {
            // get the SwRfd associated with this packet
            SwRfd = *(D100SwRfd **)(PacketArray[i]->MiniportReserved);
            
            InitAndChainPacket(Adapter,SwRfd);
            Adapter->UsedRfdCount--;
            ASSERT(Adapter->UsedRfdCount <= Adapter->NumRfd);
        } // end for 
    } while (ContinueToCheckRFDs);
    
    // check to see if we came out of this routine with the correct amount of 
    // receive buffers left over
    ASSERT(Adapter->UsedRfdCount <= (Adapter->NumRfd - MIN_NUM_RFD));
    
    DEBUGCHAR(Adapter,'r');
    return (TRUE);
}

//-----------------------------------------------------------------------------
// Procedure:   StartReceiveUnit
//
// Description: This routine checks the status of the 82557's receive unit(RU),
//              and starts the RU if it was not already active.  However,
//              before restarting the RU, the driver cleans up any recent
//              pending receives (this is very important).
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      TRUE - If we indicated any receives during this function call
//      FALSE - If we didn't indicate any receives
//-----------------------------------------------------------------------------

BOOLEAN
StartReceiveUnit(
                 IN PD100_ADAPTER Adapter
                 )

{
    PD100SwRfd      SwRfd;
    BOOLEAN         Status;
    UINT            WaitCount = 80000;

#if DBG
    UINT            i;
#endif

    DEBUGFUNC("StartReceiveUnit");

    TRACE2(Adapter, ("\n"));

    Status = FALSE;

    DEBUGCHAR(Adapter,'>');
    // If the receiver is ready, then don't try to restart.
    if ((Adapter->CSRAddress->ScbStatus & SCB_RUS_MASK) == SCB_RUS_READY)
        return Status;

    TRACE2(Adapter, ("Re-starting the RU!!!\n"));

    SwRfd = (PD100SwRfd) QueueGetHead(&Adapter->RfdList);

    // Check to make sure that our RFD head is available.  If its not, then
    // we should process the rest of our receives
    if ((SwRfd == NULL) || (SwRfd->Rfd->RfdCbHeader.CbStatus))
    {
        Status = ProcessRXInterrupt(Adapter);

        // Get the new RFD "head" after processing the pending receives.
        SwRfd = (PD100SwRfd) QueueGetHead(&Adapter->RfdList);
    }

#if DBG
    if ((Adapter->CSRAddress->ScbStatus & SCB_RUS_MASK) == SCB_RUS_READY)
    {
        TRACESTR(Adapter, ("RU is active!!!\n"));
    }

    // Big hack to check to make sure that all of the RFDs are indeed clear.
    // If they are not then we'll generate a break point.
    for (i=0; i< (Adapter->NumRfd - Adapter->UsedRfdCount); i++)
    {
        SwRfd = (PD100SwRfd) QueuePopHead(&Adapter->RfdList);
        if (SwRfd->Rfd->RfdCbHeader.CbStatus & RFD_STATUS_COMPLETE)
        {
            TRACESTR(Adapter,("RFD NOT PROCESSED!!!\n"));
//            DbgBreakPoint();
        }
        QueuePutTail(&Adapter->RfdList, &SwRfd->Link);
    }

    SwRfd = (PD100SwRfd) QueueGetHead(&Adapter->RfdList);
#endif //DBG


    // Wait for the SCB to clear before we set the general pointer
    if (WaitScb(Adapter) == FALSE)
        ASSERT(0);

    if (SwRfd != NULL)
    {
        // Set the SCB General Pointer to point the current Rfd
        Adapter->CSRAddress->ScbGeneralPointer = SwRfd->RfdPhys;
    }

    // Issue the SCB RU start command
    (void) D100IssueScbCommand(Adapter, SCB_RUC_START, FALSE);

    // wait for the command to be accepted
    WaitScb(Adapter);

    // wait for RUS to be Ready
    while (WaitCount !=0)
    {
        if ((Adapter->CSRAddress->ScbStatus & SCB_RUS_MASK) == SCB_RUS_READY)
            break;

        NdisStallExecution(10);
        WaitCount--;
    }
    if (!WaitCount)
    {
        HARDWARE_NOT_RESPONDING (Adapter);
    }

#if DBG
    // If we fall through, we have a problem.
    if (WaitCount == 0)
        TRACESTR(Adapter, ("Failed, RU won't ready -- ScbStatus %08x\n",
        Adapter->CSRAddress->ScbStatus));
#endif

    return Status;
}

