/*
 ************************************************************************
 *
 *	NSCFIR.C
 *
 *
 * Portions Copyright (C) 1996-1998 National Semiconductor Corp.
 * All rights reserved.
 * Copyright (C) 1996-1998 Microsoft Corporation. All Rights Reserved.
 *
 *
 *
 *************************************************************************
 */


#include "nsc.h"
#include "nsctypes.h"
//#include "dma.h"

VOID NSC_FIR_ISR(IrDevice *thisDev, BOOLEAN *claimingInterrupt,
                 BOOLEAN *requireDeferredCallback)
{
    LOG("==> NSC_FIR_ISR", 0);
    DEBUGFIR(DBG_ISR|DBG_OUT, ("NSC: ==> NSC_FIR_ISR(0x%x)\n", thisDev));

    thisDev->InterruptMask =
    NSC_ReadBankReg(thisDev->portInfo.ioBase, 0, 1);
    thisDev->InterruptStatus =
    NSC_ReadBankReg(thisDev->portInfo.ioBase, 0, 2) & thisDev->IntMask;
    thisDev->AuxStatus =
    NSC_ReadBankReg(thisDev->portInfo.ioBase, 0, 7);
    thisDev->LineStatus =
    NSC_ReadBankReg(thisDev->portInfo.ioBase, 0, 5);

    LOG("InterruptMask: ", thisDev->InterruptMask);
    LOG("InterruptStatus: ", thisDev->InterruptStatus);
    LOG("AuxStatus: ", thisDev->AuxStatus);
    LOG("LineStatus: ", thisDev->LineStatus);
    DEBUGFIR(DBG_ISR|DBG_OUT, ("NSC: InterruptMask = 0x%x\n", thisDev->InterruptMask));
    DEBUGFIR(DBG_ISR|DBG_OUT, ("NSC: InterruptStatus = 0x%x\n", thisDev->InterruptStatus));
    DEBUGFIR(DBG_ISR|DBG_OUT, ("NSC: AuxStatus = 0x%x\n", thisDev->AuxStatus));
    DEBUGFIR(DBG_ISR|DBG_OUT, ("NSC: LineStatus = 0x%x\n", thisDev->LineStatus));
    if (thisDev->InterruptStatus) {
        //
        // After seeing the first packet switch the interrupt
        // to timer and use the DMA counter to decide if receptions
        // have stopped.
        //
        if (!thisDev->portInfo.writePending &&
            (thisDev->InterruptStatus & 0x04) &&
            (thisDev->LineStatus & 0x80)){
            thisDev->IntMask = 0x90;
            //
            // Enable the Timer
            //
            NSC_WriteBankReg(thisDev->portInfo.ioBase, 4, 2, 0x01);
            thisDev->LastReadDMACount = thisDev->rcvDmaOffset;
        }
        if (thisDev->InterruptStatus & 0x80){
            //
            //  Disable Timer during call to DPC bit.
            //
            NSC_WriteBankReg(thisDev->portInfo.ioBase, 4, 2, 0x00);
            NSC_WriteBankReg(thisDev->portInfo.ioBase, 0, 7, 0x80);
        }

        *claimingInterrupt = TRUE;
        *requireDeferredCallback = TRUE;

        SetCOMInterrupts(thisDev, FALSE);

        if (thisDev->UIR_Mid >= 0x16 )
        {
    	    // This is for the Frame stop mode when the ISR is interrupted
    	    // after every Frame Tx
    	    //

            if ((thisDev->AuxStatus & 0x08)
             && (thisDev->InterruptStatus & 0x04)
             && (thisDev->portInfo.writePending == TRUE))
            {
                if (thisDev->LineStatus&0x40)
                {
                    NSC_WriteBankReg(thisDev->portInfo.ioBase, 0, 7, 0x40);
                    DBGERR(("FIR: Transmit underrun\n"));
                }
            }
        }
    }
    else {
        *claimingInterrupt = FALSE;
        *requireDeferredCallback = FALSE;
    }
    LOG("claimingInterrupt = ", *claimingInterrupt);

    LOG("requireDeferredCallback = ", *requireDeferredCallback);
    DEBUGFIR(DBG_ISR|DBG_OUT, ("NSC: <== NSC_FIR_ISR\n"));

}

typedef struct {
    IrDevice *thisDev;
    ULONG_PTR Offset;
    ULONG_PTR Length;
} DMASPACE;

void SkipNonDmaBuffers(PLIST_ENTRY Head, PLIST_ENTRY *Entry)
{
    while (Head!=*Entry)
    {
        rcvBuffer *rcvBuf = CONTAINING_RECORD(*Entry,
                                              rcvBuffer,
                                              listEntry);

        if (rcvBuf->isDmaBuf)
        {
            break;
        }
        else
        {
            *Entry = (*Entry)->Flink;
        }
    }
}

//
// We have two lists of buffers which occupy our DMA space.  We
// want to walk this list and find the largest space for putting
// new packets.
//
rcvBuffer *GetNextPacket(DMASPACE *Space,
                         PLIST_ENTRY *CurrFull,
                         PLIST_ENTRY *CurrPend)
{
    rcvBuffer *Result = NULL;

    SkipNonDmaBuffers(&Space->thisDev->rcvBufFull, CurrFull);
    SkipNonDmaBuffers(&Space->thisDev->rcvBufPend, CurrPend);

    if (*CurrFull==&Space->thisDev->rcvBufFull)
    {
        // Full list is finished.
        if (*CurrPend!=&Space->thisDev->rcvBufPend)
        {
            // Any entry on the pend list is valid.  Take the
            // next one and advance the pointer.

            Result = CONTAINING_RECORD(*CurrPend,
                                       rcvBuffer,
                                       listEntry);

            *CurrPend = (*CurrPend)->Flink;
        }
        else
        {
            // Both lists are finished.  We will return NULL.
        }
    }
    else
    {
        if (*CurrPend==&Space->thisDev->rcvBufPend)
        {
            // Pend list is finished.  Take anything from the
            // Full list, advance the pointer.
            Result = CONTAINING_RECORD(*CurrFull,
                                       rcvBuffer,
                                       listEntry);

            *CurrFull = (*CurrFull)->Flink;
        }
        else
        {
            // Both list have valid entries.  Compare the two and take the
            // one that appears in the buffer first.
            rcvBuffer *Full, *Pend;

            Full = CONTAINING_RECORD(*CurrFull,
                                     rcvBuffer,
                                     listEntry);
            Pend = CONTAINING_RECORD(*CurrPend,
                                     rcvBuffer,
                                     listEntry);

            if (Full->dataBuf < Pend->dataBuf)
            {
                // Full is the winner.  Take it.

                Result = Full;
                *CurrFull = (*CurrFull)->Flink;
            }
            else
            {
                // Pend is the winner.  Take it.

                Result = Pend;
                *CurrPend = (*CurrPend)->Flink;
            }
        }
    }

    if (Result)
    {
        ASSERT(Result->isDmaBuf);
    }

    return Result;
}

BOOLEAN SynchronizedFindLargestSpace(IN PVOID Context)
{
    DMASPACE *Space = Context;
    IrDevice *thisDev = Space->thisDev;
    BOOLEAN Result;
    PLIST_ENTRY Full, Pend;
    rcvBuffer *Current = NULL;

    ASSERT(sizeof(ULONG)==sizeof(PVOID));


    if (IsListEmpty(&thisDev->rcvBufFull) && IsListEmpty(&thisDev->rcvBufPend))
    {
        Space->Offset = (ULONG_PTR)thisDev->portInfo.dmaReadBuf;
        Space->Length = RCV_DMA_SIZE;
    }
    else
    {
        ULONG_PTR EndOfLast;
        ULONG_PTR ThisSpace;

        Full = thisDev->rcvBufFull.Flink;
        Pend = thisDev->rcvBufPend.Flink;

        Space->Length = 0;

        EndOfLast = Space->Offset = (ULONG_PTR)thisDev->portInfo.dmaReadBuf;

        Current = GetNextPacket(Space, &Full, &Pend);
        while (Current)
        {
            // It's possible to get a packet that is from SIR.  If so, skip it.

            if (Current->isDmaBuf)
            {
                ASSERT((ULONG_PTR)Current->dataBuf >= EndOfLast);

                ThisSpace = (ULONG_PTR)Current->dataBuf - EndOfLast;

                // ASSERT the pointer is actually in our DMA buffer.
                ASSERT(Current->dataBuf >= thisDev->portInfo.dmaReadBuf ||
                       Current->dataBuf < thisDev->portInfo.dmaReadBuf+RCV_DMA_SIZE);

                if (ThisSpace > Space->Length)
                {
                    Space->Offset = EndOfLast;
                    Space->Length = ThisSpace;
                }

                EndOfLast = (ULONG_PTR)Current->dataBuf + Current->dataLen;
            }


            Current = GetNextPacket(Space, &Full, &Pend);
        }

        // Now we do one more calculation for the space after the end of the list.

        ThisSpace = (ULONG_PTR)thisDev->portInfo.dmaReadBuf + RCV_DMA_SIZE - (ULONG_PTR)EndOfLast;

        if (ThisSpace > Space->Length)
        {
            Space->Offset = EndOfLast;
            Space->Length = ThisSpace;
        }

        // Round off to start DMA on 4 byte boundary
        Space->Length -= 4 - (Space->Offset & 3);
        Space->Offset = (Space->Offset+3) & (~3);
    }

    // We want space relative to start of buffer.
    Space->Offset -= (ULONG_PTR)thisDev->portInfo.dmaReadBuf;

    Result = (Space->Length >= MAX_RCV_DATA_SIZE + FAST_IR_FCS_SIZE);

    if (!Result)
    {
        DEBUGFIR(DBG_ERR, ("NSC: ERROR: Not enough space to DMA full packet %x\n", thisDev));
    }

    return Result;
}

BOOLEAN FindLargestSpace(IN IrDevice *thisDev,
                         OUT PULONG_PTR pOffset,
                         OUT PULONG_PTR pLength)
{
    DMASPACE Space;
    BOOLEAN Result;

    Space.Offset = 0;
    Space.Length = 0;
    Space.thisDev = thisDev;

    NdisAcquireSpinLock(&thisDev->QueueLock);
    Result = NdisMSynchronizeWithInterrupt(&thisDev->interruptObj,
                                           SynchronizedFindLargestSpace,
                                           &Space);
    NdisReleaseSpinLock(&thisDev->QueueLock);

    *pOffset = Space.Offset;
    *pLength = Space.Length;

    return Result;
}



BOOLEAN FIR_MegaSend(IrDevice *thisDev)
{
    NDIS_STATUS stat;
    PNDIS_PACKET Packet;
    PNDIS_IRDA_PACKET_INFO packetInfo;
    BOOLEAN ret_val;

    DEBUGMSG(DBG_TRACE_TX, ("S"));
    LOG("==> FIR_MegaSend", 0);
    DEBUGFIR(DBG_TX|DBG_OUT, ("NSC: ==> FIR_MegaSend(0x%x)\n", thisDev));

    thisDev->HangChk = 0;

    NdisAcquireSpinLock(&thisDev->QueueLock);

    if (IsListEmpty(&thisDev->SendQueue))
    {
        thisDev->portInfo.writePending = FALSE;
        NdisReleaseSpinLock(&thisDev->QueueLock);
        ret_val = FALSE;
    }
#if 0
    else if (thisDev->portInfo.writePending)
    {
        NdisReleaseSpinLock(&thisDev->QueueLock);
        ret_val = TRUE;
    }
#endif
    else
    {
        /*
         * Update statistics.
         */
        RegStats.TxPacketsStarted++;

        thisDev->portInfo.writePending = TRUE;

        Packet = HEAD_SEND_PACKET(thisDev);

        // Check for min turnaround time set.

        packetInfo = GetPacketInfo(Packet);

        if (packetInfo->MinTurnAroundTime)
        {
            // A popular mechanism to implement MinTurnAroundTime has been
            // NdisStallExecution().  This is wrong, and must be avoided, because
            // NdisStallExecution is limited 60us, and the Turnaround delay can
            // be much longer than that.
            UINT usecToWait = packetInfo->MinTurnAroundTime;
            UINT msecToWait;
            packetInfo->MinTurnAroundTime = 0;

            NdisReleaseSpinLock(&thisDev->QueueLock);

            // Ndis timer has a 1ms granularity (in theory).  Let's round off.

            msecToWait = (usecToWait<=1000) ? 1 : (usecToWait+500)/1000;

            NdisSetTimer(&thisDev->TurnaroundTimer, msecToWait);

            return TRUE; // Say we're successful.  We'll come back here.

        }

        if (thisDev->lastPacketAtOldSpeed==Packet)
        {
            thisDev->setSpeedAfterCurrentSendPacket = TRUE;
        }

        NdisToFirPacket(thisDev,
                        Packet, (UCHAR *) thisDev->portInfo.writeBuf,
                        MAX_IRDA_DATA_SIZE, &thisDev->portInfo.writeBufLen);


        DEBUGFIR(DBG_PKT, ("NSC: Sending packet\n"));
        DBGPRINTBUF(thisDev->portInfo.writeBuf, thisDev->portInfo.writeBufLen);

        NdisReleaseSpinLock(&thisDev->QueueLock);


        /* Setup Transmit DMA. */
        NdisMSetupDmaTransfer(&stat, thisDev->DmaHandle,
                              thisDev->xmitDmaBuffer, 0,
                              thisDev->portInfo.writeBufLen, TRUE);

        if (stat != NDIS_STATUS_SUCCESS) {
            LOG("NdisMSetupDmaTransfer(Tx)failed", 0);
            DBG_X(thisDev->xmitDmaBuffer);
            DBG_X(thisDev->portInfo.writeBufLen);
            DbgBreakPoint();
            ret_val = FALSE;
        }
        else {
            ret_val = TRUE;

            LOG("FIR_MegaSend number of data bytes sent: ",
                thisDev->portInfo.writeBufLen);
            DEBUGFIR(DBG_TX|DBG_OUT, ("NSC: FIR_MegaSend sent 0x%x bytes of data.\n",
                                      thisDev->portInfo.writeBufLen));
        }
    }


    LOG("<== FIR_MegaSend", 0);
    DEBUGFIR(DBG_TX|DBG_OUT, ("NSC: <== FIR_MegaSend\n"));

    return ret_val;
}


void FIR_MegaSendComplete(IrDevice *thisDev)
{
    NDIS_STATUS status;
    PLIST_ENTRY ListEntry;
    PNDIS_PACKET Packet;

    DEBUGMSG(DBG_TRACE_TX, ("C"));
    LOG("==> FIR_MegaSendComplete", 0);
    DEBUGFIR(DBG_TX|DBG_OUT, ("NSC: ==> FIR_MegaSendComplete(0x%x)\n", thisDev));

    thisDev->HangChk = 0;

    ListEntry = MyInterlockedRemoveHeadList(&thisDev->SendQueue,
                                            &thisDev->QueueLock);

    if (!ListEntry)
    {
        ASSERT(0);
    }
    else
    {
        Packet = CONTAINING_RECORD(ListEntry,
                                   NDIS_PACKET,
                                   MiniportReserved);

        RegStats.TxPacketsCompleted++;

        NdisMCompleteDmaTransfer(&status, thisDev->DmaHandle,
                                 thisDev->xmitDmaBuffer, 0,
                                 thisDev->portInfo.writeBufLen, TRUE);

        if (status != NDIS_STATUS_SUCCESS) {
            DBGERR(("NdisMCompleteDmaTransfer failed: %d\n", status));
#if DBG
            DbgBreakPoint();
#endif
        }

        /*
         * Check for Tx underrun.
         */
        if (NSC_ReadBankReg(thisDev->portInfo.ioBase, 0, 7) & 0x40) {

            NSC_WriteBankReg(thisDev->portInfo.ioBase, 0, 7, 0x40);

            RegStats.TxUnderruns++;
            status = NDIS_STATUS_FAILURE;

            LOG("FIR_MegaSendComplete: Transmit Underrun", 0);
            DEBUGFIR(DBG_TX|DBG_ERR, ("NSC: FIR_MegaSendComplete: Transmit Underrun\n"));
        }
        else {
            status = NDIS_STATUS_SUCCESS;
        }

        /* Reset Tx pending. */
        thisDev->portInfo.writePending = FALSE;

        /*
         * Notify NDIS of TX complete.
         */
        NdisMSendComplete(thisDev->ndisAdapterHandle, Packet, status);


        LOG("<== FIR_MegaSendComplete", 1);
        DEBUGFIR(DBG_TX|DBG_OUT, ("NSC: <== FIR_MegaSendComplete\n"));
    }

}

void FIR_DeliverFrames(IrDevice *thisDev)
{
    UCHAR frameStat;
    NDIS_STATUS stat;
    ULONG rcvFrameSize;
    PUCHAR NewFrame;
    ULONG_PTR LastReadDMACount, EndOfData;
    BOOLEAN resetDma = FALSE, OverflowOccurred = FALSE;
    BOOLEAN Discarding = thisDev->DiscardNextPacketSet;
    const    UINT    fcsSize    =    (   thisDev->currentSpeed    >=    MIN_FIR_SPEED)    ?
                                     FAST_IR_FCS_SIZE : MEDIUM_IR_FCS_SIZE;
    LOG("==> FIR_DeliverFrames", 0);
    DEBUGFIR(DBG_RX|DBG_OUT, ("NSC: ==> FIR_DeliverFrames(0x%x)\n", thisDev));

    LastReadDMACount = NdisMReadDmaCounter(thisDev->DmaHandle);

    // Check for data received since the last time we were here.
    // We also check for data in fifo.  If there is some, we wait, as long
    // as the DMA still has room to capture data.

    if (LastReadDMACount != thisDev->LastReadDMACount ||
        ((NSC_ReadBankReg(thisDev->portInfo.ioBase, 2, 7) & 0x3f) &&
         LastReadDMACount != 0)){
        thisDev->LastReadDMACount = LastReadDMACount;
        //
        //  Set Timer Enable bit for another Timeout.
        //
        NSC_WriteBankReg(thisDev->portInfo.ioBase, 4, 2, 0x01);
        LOG("<== FIR_DeliverFrames-Early Return", 1);
        DEBUGFIR(DBG_RX|DBG_OUT, ("NSC: <== FIR_DeliverFrames\n"));
        return;
    }
    RegStats.RxDPC_Window++;
    thisDev->IntMask = 0x04;

    NdisMCompleteDmaTransfer(&stat, thisDev->DmaHandle,
                             thisDev->rcvDmaBuffer,
                             (ULONG)thisDev->rcvDmaOffset,
                             (ULONG)thisDev->rcvDmaSize, FALSE);
    if (stat != NDIS_STATUS_SUCCESS) {
        DBGERR(("NdisMCompleteDmaTransfer failed: %d\n", stat));
        ASSERT(0);
    }

    thisDev->DiscardNextPacketSet = FALSE;

    // Get the exact count.
    LastReadDMACount = NdisMReadDmaCounter(thisDev->DmaHandle);

    EndOfData = thisDev->rcvDmaOffset + thisDev->rcvDmaSize - LastReadDMACount;

    frameStat = NSC_ReadBankReg(thisDev->portInfo.ioBase, 5, 5);

    LOG("frameStat: ", (UINT) frameStat);
    DEBUGFIR(DBG_RX|DBG_OUT, ("NSC: frameStat = %xh\n", (UINT) frameStat));

    while ((frameStat & 0x80) && thisDev->rcvPktOffset < EndOfData) {

        /* Do we need to read these regs in order??? */
        rcvFrameSize =
        NSC_ReadBankReg(thisDev->portInfo.ioBase, 5, 6);
        rcvFrameSize |=
        ((USHORT) NSC_ReadBankReg(thisDev->portInfo.ioBase,
                                  5, 7) << 8);
        /*
         *  This status byte is valid; see what else it says...
         *  Also mask off indeterminate bit.
         */
        frameStat &= ~0xA0;

        /*
         * Whether the frame is good or bad, we must read the counter
         * FIFO to synchronize it with the frame status FIFO.
         */
        if (Discarding)
        {
             // Do nothing
        }
        else if (frameStat) {
            /*
             *  Some rcv error occurred. Reset DMA.
             */
            LOG("*** RCV ERR: framStat in FIR_DeliverFrames:",
                frameStat);
            DEBUGFIR(DBG_RX|DBG_ERR, ("NSC: RCV ERR: frameStat=%xh FrameSize=%d \n",
                                      (UINT)frameStat,rcvFrameSize));

            if (frameStat & 0x40) {
                if (frameStat & 0x01) {
                    RegStats.StatusFIFOOverflows++;
                }
                if (frameStat & 0x02) {
                    RegStats.ReceiveFIFOOverflows++;
                }
                RegStats.MissedPackets += rcvFrameSize;
            }
            else{
                if (frameStat & 0x01) {
                    RegStats.StatusFIFOOverflows++;
                }
                if (frameStat & 0x02) {
                    RegStats.ReceiveFIFOOverflows++;
                }
                if (frameStat & 0x04) {
                    RegStats.ReceiveCRCErrors++;
                }
                if (frameStat & 0x08) {
                }
                if (frameStat & 0x10) {
                }
                LOG("Bytes Lost:",rcvFrameSize);
                ASSERT((thisDev->rcvPktOffset + rcvFrameSize)<= RCV_DMA_SIZE);

                /* Advance pointer past bad packet.  */
                thisDev->rcvPktOffset += rcvFrameSize;
            }
        }

        else if (thisDev->rcvPktOffset + rcvFrameSize > EndOfData && !(frameStat&0x40))
        {
            DBGERR(("Packet won't fit in received data!\n"));
            DBGERR(("rcvPktOffset:%x rcvFrameSize:%x LastReadDmaCount:%x\n",
                     thisDev->rcvPktOffset,
                     rcvFrameSize,
                     LastReadDMACount));
            DBGERR(("rcvDmaOffset:%x rcvDmaSize:%x EndOfData:%x\n",
                     thisDev->rcvDmaOffset, thisDev->rcvDmaSize, EndOfData));

            // This packet will actually show up at the beginning of the
            // *next* DMA.  We have inadvertently read the status early.
            // Save it for next time and get out.

#if 0
            thisDev->nextFrameStat = frameStat;
            thisDev->nextFrameSize = rcvFrameSize;

            // Clear the line status register, of any past events.
            thisDev->LineStatus = NSC_ReadBankReg(thisDev->portInfo.ioBase, 0, 5);
#endif

            break;
        }

        else {

            DEBUGFIR(DBG_RX|DBG_OUT, ("NSC:  *** >>> FIR_DeliverFrames DMA offset 0x%x:\n",
                                      thisDev->rcvDmaOffset));

            NewFrame = thisDev->portInfo.dmaReadBuf +
                       thisDev->rcvPktOffset;

            /* Step offset for next buffer.  */
            thisDev->rcvPktOffset += rcvFrameSize;

            ASSERT(thisDev->rcvPktOffset < RCV_DMA_SIZE);

            /* Chop off FCS */
            rcvFrameSize -= fcsSize;
            if (rcvFrameSize <= MAX_NDIS_DATA_SIZE &&
                rcvFrameSize >= IR_ADDR_SIZE + IR_CONTROL_SIZE)
            {
                //
                // Queue this rcv packet.  Move Newframe pointer
                // into RxDMA buffer.
                //
                RegStats.ReceivedPackets++;
                RegStats.RxWindow++;
                QueueReceivePacket(thisDev, NewFrame, rcvFrameSize, TRUE);
            }
            else {
                LOG("Error: invalid packet size "
                    "in FIR_DeliverFrames", rcvFrameSize);
                DEBUGFIR(DBG_RX|DBG_ERR, ("NSC: invalid packet size in FIR_DeliverFrames; %xh > %xh\n", rcvFrameSize, MAX_RCV_DATA_SIZE));
                //
                // Discard the rest of the packets.
                //
                while (NSC_ReadBankReg(thisDev->portInfo.ioBase, 5, 5)&0x80)
                {
                    NSC_ReadBankReg(thisDev->portInfo.ioBase, 5, 6);
                    NSC_ReadBankReg(thisDev->portInfo.ioBase, 5, 7);
                }
                thisDev->DiscardNextPacketSet = TRUE;
            }
        }

        frameStat = NSC_ReadBankReg(thisDev->portInfo.ioBase, 5, 5);

        LOG("frameStat: ", (UINT) frameStat);
        DEBUGFIR(DBG_RX|DBG_OUT, ("NSC: frameStat = %xh\n", (UINT) frameStat));

        // Clear the line status register, of any past events.
        thisDev->LineStatus =
        NSC_ReadBankReg(thisDev->portInfo.ioBase, 0, 5);
    }

    SetupRecv(thisDev);

    LOG("<== FIR_DeliverFrames", 1);
    DEBUGFIR(DBG_RX|DBG_OUT, ("NSC: <== FIR_DeliverFrames\n"));
}

BOOLEAN NSC_Setup(IrDevice *thisDev)
{
    NDIS_DMA_DESCRIPTION DMAChannelDcr;
    NDIS_STATUS stat;

    thisDev->rcvDmaOffset = 0;

    /*
     *  Because we enable rcv DMA while SIR receives may still be
     *  going on, we need to keep a separate receive buffer for DMA.
     *  This buffer gets swapped with the rcvBuffer data pointer
     *  and must be the same size.
     */
    thisDev->portInfo.dmaReadBuf = MyMemAlloc(RCV_DMA_SIZE, TRUE);
    if (!thisDev->portInfo.dmaReadBuf){
        return FALSE;
    }

    NdisAllocateBufferPool(&stat, &thisDev->dmaBufferPoolHandle, 2);
    if (stat != NDIS_STATUS_SUCCESS){
        LOG("Error: NdisAllocateBufferPool failed in NSC_Setup", 0);
        DEBUGFIR(DBG_ERR, ("NSC: NdisAllocateBufferPool failed in NSC_Setup\n"));
        return FALSE;
    }

    NdisAllocateBuffer(&stat, &thisDev->rcvDmaBuffer,
                       thisDev->dmaBufferPoolHandle,
                       thisDev->portInfo.dmaReadBuf,
                       RCV_DMA_SIZE);
    if (stat != NDIS_STATUS_SUCCESS) {
        LOG("Error: NdisAllocateBuffer failed (rcv) in NSC_Setup", 0);
        DEBUGFIR(DBG_ERR, ("NSC: NdisAllocateBuffer failed (rcv) in NSC_Setup\n"));
        return FALSE;
    }

    NdisAllocateBuffer(&stat, &thisDev->xmitDmaBuffer,
                       thisDev->dmaBufferPoolHandle,
                       thisDev->portInfo.writeBuf,
                       MAX_IRDA_DATA_SIZE);
    if (stat != NDIS_STATUS_SUCCESS) {
        LOG("NdisAllocateBuffer failed (xmit) in NSC_Setup", 0);
        DEBUGFIR(DBG_ERR, ("NSC: NdisAllocateBuffer failed (xmit) in NSC_Setup\n"));
        return FALSE;
    }


    /*
     *  Initialize rcv DMA channel
     */
    DMAChannelDcr.DemandMode = TRUE;
    DMAChannelDcr.AutoInitialize = FALSE;
    DMAChannelDcr.DmaChannelSpecified = FALSE;
    DMAChannelDcr.DmaWidth = Width8Bits;
    DMAChannelDcr.DmaSpeed = Compatible;
    DMAChannelDcr.DmaPort = 0;
    DMAChannelDcr.DmaChannel = thisDev->portInfo.DMAChannel; // 0;

    stat = NdisMRegisterDmaChannel(&thisDev->DmaHandle,
                                   thisDev->ndisAdapterHandle,
                                   thisDev->portInfo.DMAChannel,
                                   FALSE, &DMAChannelDcr, RCV_DMA_SIZE);

    if (stat != NDIS_STATUS_SUCCESS) {
        DEBUGFIR(DBG_ERR, ("NSC: NdisMRegisterDmaChannel failed\n"));
        DbgBreakPoint();
        return FALSE;
    }

    return TRUE;
}


void NSC_Shutdown(IrDevice *thisDev)
{
    if (thisDev->DmaHandle){
        NdisMDeregisterDmaChannel(thisDev->DmaHandle);
        thisDev->DmaHandle = NULL;
    }

    if (thisDev->xmitDmaBuffer){
        NdisFreeBuffer(   thisDev->xmitDmaBuffer);
        thisDev->xmitDmaBuffer = NULL;
    }

    if (thisDev->rcvDmaBuffer){
        NdisFreeBuffer(thisDev->rcvDmaBuffer);
        thisDev->rcvDmaBuffer = NULL;
    }

    if (thisDev->dmaBufferPoolHandle){
        NdisFreeBufferPool(thisDev->dmaBufferPoolHandle);
        thisDev->dmaBufferPoolHandle = NULL;
    }
    if (thisDev->portInfo.dmaReadBuf){
        MyMemFree(thisDev->portInfo.dmaReadBuf, RCV_DMA_SIZE, TRUE);
        thisDev->portInfo.dmaReadBuf = NULL;
    }
}


BOOLEAN NdisToFirPacket(IrDevice *thisDev, PNDIS_PACKET Packet,
                        UCHAR *irPacketBuf, UINT irPacketBufLen, UINT *irPacketLen)
{
    PNDIS_BUFFER ndisBuf;
    UINT ndisPacketBytes = 0;
    UINT ndisPacketLen;

    LOG("==> NdisToFirPacket", 0);
    DEBUGFIR(DBG_OUT, ("NSC: ==> NdisToFirPacket(0x%x)\n", thisDev));

    /*
     *  Get the packet's entire length and its first NDIS buffer
     */
    NdisQueryPacket(Packet, NULL, NULL, &ndisBuf, &ndisPacketLen);

    LOG("NdisToFirPacket, number of bytes:", ndisPacketLen);
    DEBUGFIR(DBG_OUT, ("NSC: NdisToFirPacket, number of bytes: %d\n", ndisPacketLen));

    /*
     *  Make sure that the packet is big enough to be legal.
     *  It consists of an A, C, and variable-length I field.
     */
    if (ndisPacketLen < IR_ADDR_SIZE + IR_CONTROL_SIZE){
        LOG("Error: packet too short in ", ndisPacketLen);
        DEBUGFIR(DBG_ERR, ("NSC: packet too short in NdisToFirPacket (%d bytes)\n", ndisPacketLen));
        return FALSE;
    }

    /*
     *  Make sure that we won't overwrite our contiguous buffer.
     */
    if (ndisPacketLen > irPacketBufLen){
        /*
         *  The packet is too large
         *  Tell the caller to retry with a packet size large
         *  enough to get past this stage next time.
         */
        LOG("Error: packet too large in ", ndisPacketLen);
        DEBUGFIR(DBG_ERR, ("NSC: Packet too large in NdisToIrPacket (%d=%xh bytes), MAX_IRDA_DATA_SIZE=%d, irPacketBufLen=%d.\n",
                           ndisPacketLen, ndisPacketLen, MAX_IRDA_DATA_SIZE, irPacketBufLen));
        *irPacketLen = ndisPacketLen;
        return FALSE;
    }


    /*
     *  Read the NDIS packet into a contiguous buffer.
     *  We have to do this in two steps so that we can compute the
     *  FCS BEFORE applying escape-byte transparency.
     */
    while (ndisBuf) {
        UCHAR *bufData;
        UINT bufLen;

        NdisQueryBuffer(ndisBuf, (PVOID *)&bufData, &bufLen);

        if (ndisPacketBytes + bufLen > ndisPacketLen){
            /*
             *  Packet was corrupt -- it misreported its size.
             */
            *irPacketLen = 0;
            return FALSE;
        }

        NdisMoveMemory((PVOID)(irPacketBuf+ndisPacketBytes),
                       (PVOID)bufData, (ULONG)bufLen);
        ndisPacketBytes += bufLen;

        NdisGetNextBuffer(ndisBuf, &ndisBuf);
    }

    LOG("Ir Command byte ", (UINT)*(irPacketBuf+1));

    /*
     *  Do a sanity check on the length of the packet.
     */
    if (ndisPacketBytes != ndisPacketLen){
        /*
         *  Packet was corrupt -- it misreported its size.
         */
        LOG("Error: Packet corrupt in NdisToIrPacket "
            "(buffer lengths don't add up to packet length)", 0);
        DEBUGFIR(DBG_ERR, ("NSC: Packet corrupt in NdisToIrPacket (buffer lengths don't add up to packet length).\n"));
        *irPacketLen = 0;
        return FALSE;
    }

#ifdef DBG_ADD_PKT_ID
    if (addPktIdOn){
        static USHORT uniqueId = 0;
        DEBUGFIR(DBG_OUT, ("NSC:  *** --> SEND PKT ID: %xh\n", (UINT)uniqueId));
        LOG("ID: Send (FIR) Pkt id:", uniqueId);
        *(USHORT *)(irPacketBuf+ndisPacketBytes) = uniqueId++;
        ndisPacketBytes += sizeof(USHORT);
    }
#endif

    *irPacketLen = ndisPacketBytes;

    LOG("<== NdisToFirPacket", 0);
    DEBUGFIR(DBG_OUT, ("NSC: <== NdisToFirPacket\n"));

    return TRUE;
}


