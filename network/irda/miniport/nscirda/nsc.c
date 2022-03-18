/*
 ************************************************************************
 *
 *	NSC.c
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
#include "newdong.h"

extern UIR IrDongleResource;
/*
 *  We keep a linked list of device objects
 */
IrDevice *firstIrDevice = NULL;

/* This fuction sets up the device for Recv */
void SetupRecv(IrDevice *thisDev);

//
// Debug Counters
//
DebugCounters RegStats = {0,0,0,0,0,0,0,0,0};


#ifdef RECEIVE_PACKET_LOGGING

typedef struct {
    UCHAR Data[12];
} DATA_BITS;


typedef struct {
    USHORT Tag;
    USHORT Line;
    union {
        struct {
            PNDIS_PACKET    Packet;
            PVOID           DmaBuffer;
            ULONG           Length;
        } Packet;
        struct {
            PLIST_ENTRY     Head;
            PLIST_ENTRY     Entry;
        } List;
        struct {
            PVOID           Start;
            ULONG           Offset;
            ULONG           Length;
        } Dma;
        struct {
            ULONG           Length;
        } Discard;
        DATA_BITS Data;
    };
} RCV_LOG;

#define CHAIN_PACKET_TAG 'CP'
#define UNCHAIN_PACKET_TAG 'UP'
#define ADD_HEAD_LIST_TAG 'HA'
#define ADD_TAIL_LIST_TAG 'TA'
#define REMOVE_HEAD_LIST_TAG 'HR'
#define REMOVE_ENTRY_TAG 'ER'
#define DMA_TAG  'MD'
#define DATA_TAG 'AD'
#define DATA2_TAG '2D'
#define DISCARD_TAG 'XX'

#define NUM_RCV_LOG 256

ULONG   RcvLogIndex = 0;
RCV_LOG RcvLog[NUM_RCV_LOG];


BOOLEAN SyncGetRcvLogEntry(PVOID Context)
{
    *(ULONG*)Context = RcvLogIndex++;
    RcvLogIndex &= NUM_RCV_LOG-1;
    return TRUE;
}

ULONG GetRcvLogEntry(IrDevice *thisDev)
{
    ULONG Entry;

    NdisAcquireSpinLock(&thisDev->QueueLock);
    NdisMSynchronizeWithInterrupt(&thisDev->interruptObj, SyncGetRcvLogEntry, &Entry);
    NdisReleaseSpinLock(&thisDev->QueueLock);
    return Entry;
}




#define LOG_InsertHeadList(d, h, e)         \
{                                           \
    ULONG i = GetRcvLogEntry(d);            \
    RcvLog[i].Tag = ADD_HEAD_LIST_TAG;      \
    RcvLog[i].Line = __LINE__;              \
    RcvLog[i].List.Head = (h);                   \
    RcvLog[i].List.Entry = (PLIST_ENTRY)(e);                  \
}

#define LOG_InsertTailList(d, h, e)         \
{                                           \
    ULONG i = GetRcvLogEntry(d);            \
    RcvLog[i].Tag = ADD_TAIL_LIST_TAG;      \
    RcvLog[i].Line = __LINE__;              \
    RcvLog[i].List.Head = (h);              \
    RcvLog[i].List.Entry = (PLIST_ENTRY)(e);             \
}

#define LOG_RemoveHeadList(d, h, e)         \
{                                           \
    ULONG i = GetRcvLogEntry(d);            \
    RcvLog[i].Tag = REMOVE_HEAD_LIST_TAG;      \
    RcvLog[i].Line = __LINE__;              \
    RcvLog[i].List.Head = (h);              \
    RcvLog[i].List.Entry = (PLIST_ENTRY)(e);             \
}

#define LOG_RemoveEntryList(d, e)           \
{                                           \
    ULONG i = GetRcvLogEntry(d);            \
    RcvLog[i].Tag = REMOVE_ENTRY_TAG;       \
    RcvLog[i].Line = __LINE__;              \
    RcvLog[i].List.Head = NULL;             \
    RcvLog[i].List.Entry = (PLIST_ENTRY)(e);             \
}

#define LOG_PacketChain(d, p)                                   \
{                                                               \
    PNDIS_BUFFER NdisBuffer;                                    \
    PVOID Address;                                              \
    ULONG Len;                                                  \
    ULONG i = GetRcvLogEntry(d);                                \
    RcvLog[i].Tag = CHAIN_PACKET_TAG;                           \
    RcvLog[i].Line = __LINE__;                                  \
    NdisQueryPacket((p), NULL, NULL, &NdisBuffer, NULL);        \
    NdisQueryBuffer(NdisBuffer, &Address, &Len);                \
    RcvLog[i].Packet.Packet = (p);                              \
    RcvLog[i].Packet.DmaBuffer = Address;                       \
    RcvLog[i].Packet.Length = Len;                              \
}

#define LOG_PacketUnchain(d, p)                                 \
{                                                               \
    PNDIS_BUFFER NdisBuffer;                                    \
    PVOID Address;                                              \
    ULONG Len;                                                  \
    ULONG i = GetRcvLogEntry(d);                                \
    RcvLog[i].Tag = UNCHAIN_PACKET_TAG;                         \
    RcvLog[i].Line = __LINE__;                                  \
    NdisQueryPacket((p), NULL, NULL, &NdisBuffer, NULL);        \
    NdisQueryBuffer(NdisBuffer, &Address, &Len);                \
    RcvLog[i].Packet.Packet = (p);                              \
    RcvLog[i].Packet.DmaBuffer = Address;                       \
    RcvLog[i].Packet.Length = Len;                              \
}

#define LOG_Dma(d)                                              \
{                                                               \
    ULONG i = GetRcvLogEntry(d);                                \
    RcvLog[i].Tag = DMA_TAG;                                    \
    RcvLog[i].Line = __LINE__;                                  \
    RcvLog[i].Dma.Start = (d)->rcvDmaBuffer;                    \
    RcvLog[i].Dma.Offset = (d)->rcvDmaOffset;                   \
    RcvLog[i].Dma.Length = (d)->rcvDmaSize;                     \
}

#define LOG_Data(d,s)                                           \
{                                                               \
    ULONG i = GetRcvLogEntry(d);                                \
    RcvLog[i].Tag = DATA_TAG;                                   \
    RcvLog[i].Line = ((USHORT)(s))&0xffff;                      \
    RcvLog[i].Data = *(DATA_BITS*)(s);                          \
}

#define LOG_Data2(d,s)                                           \
{                                                               \
    ULONG i = GetRcvLogEntry(d);                                \
    RcvLog[i].Tag = DATA2_TAG;                                   \
    RcvLog[i].Line = ((USHORT)(s))&0xffff;                      \
    RcvLog[i].Data = *(DATA_BITS*)(s);                          \
}

#define LOG_Discard(d,s)                                        \
{                                                               \
    ULONG i = GetRcvLogEntry(d);                                \
    RcvLog[i].Tag = DISCARD_TAG;                                \
    RcvLog[i].Line = __LINE__;                                  \
    RcvLog[i].Discard.Length = (s);                             \
}

void DumpNdisPacket(PNDIS_PACKET Packet, UINT Line)
{
    UINT PhysBufCnt, BufCnt, TotLen, Len;
    PNDIS_BUFFER NdisBuffer;
    PVOID Address;

    DbgPrint("Badly formed NDIS packet at line %d\n", Line);

    NdisQueryPacket(Packet, &PhysBufCnt, &BufCnt, &NdisBuffer, &TotLen);
    DbgPrint("Packet:%08X  PhysBufCnt:%d BufCnt:%d TotLen:%d\n",
             Packet, PhysBufCnt, BufCnt, TotLen);
    while (NdisBuffer)
    {
        NdisQueryBuffer(NdisBuffer, &Address, &Len);
        DbgPrint("   Buffer:%08X Address:%08X Length:%d\n",
                 NdisBuffer, Address, Len);
        NdisGetNextBuffer(NdisBuffer, &NdisBuffer);
    }
    ASSERT(0);
}

#define VerifyNdisPacket(p, b) \
{                                                       \
    UINT BufCnt;                                        \
                                                        \
    NdisQueryPacket((p), NULL, &BufCnt, NULL, NULL);    \
    if (BufCnt>(b))                                     \
    {                                                   \
        DumpNdisPacket((p), __LINE__);                  \
    }                                                   \
}
#else
#define VerifyNdisPacket(p,b)
#define LOG_InsertHeadList(d, h, e)
#define LOG_InsertTailList(d, h, e)
#define LOG_RemoveHeadList(d, h, e)
#define LOG_RemoveEntryList(d, e)
#define LOG_PacketChain(d, p)
#define LOG_PacketUnchain(d, p)
#define LOG_Dma(d)
#define LOG_Data(d,s)
#define LOG_Data2(d,s)
#define LOG_Discard(d,s)
#endif

/*
 *************************************************************************
 *  MiniportCheckForHang
 *************************************************************************
 *
 *  Reports the state of the network interface card.
 *
 */
BOOLEAN MiniportCheckForHang(NDIS_HANDLE MiniportAdapterContext)
{
    IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);
    LOG("==> MiniportCheckForHang", 0);
    DBGOUT(("==> MiniportCheckForHang(0x%x)", MiniportAdapterContext));
#if 1
    // We have seen cases where we hang sending at high speeds.  This occurs only
    // on very old revisions of the NSC hardware.
    // This is an attempt to kick us off again.
    if (thisDev->currentSpeed > MAX_SIR_SPEED && thisDev->portInfo.writePending)
    {
        switch (thisDev->HangChk)
        {
            case 0:
                thisDev->HangChk++;
                break;
            default:
                thisDev->HangChk++;
                DBGERR(("NSCIRDA: CheckForHang--we appear hung\n"));

                // Issue a soft reset to the transmitter & receiver.

                NSC_WriteBankReg(thisDev->portInfo.ioBase, 0, 2, 0x06);

                MiniportHandleInterrupt(MiniportAdapterContext);
                break;
        }
    }
#endif
    LOG("<== MiniportCheckForHang", 1);
    DBGOUT(("<== MiniportCheckForHang(0x%x)", MiniportAdapterContext));
    return FALSE;
}


/*
 *************************************************************************
 *  MiniportDisableInterrupt
 *************************************************************************
 *
 *  Disables the NIC from generating interrupts.
 *
 */
VOID MiniportDisableInterrupt(NDIS_HANDLE MiniportAdapterContext)
{
    IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);

    LOG("==> MiniportDisableInterrupt", 0);
    DBGOUT(("==> MiniportDisableInterrupt(0x%x)", MiniportAdapterContext));
    if (!thisDev->resourcesReleased){
        thisDev->intEnabled = FALSE;
        SetCOMInterrupts(thisDev, FALSE);
    }
    LOG("<== MiniportDisableInterrupt", 1);
    DBGOUT(("<== MiniportDisableInterrupt(0x%x)", MiniportAdapterContext));
}


/*
 *************************************************************************
 *  MiniportEnableInterrupt
 *************************************************************************
 *
 *  Enables the IR card to generate interrupts.
 *
 */
VOID MiniportEnableInterrupt(IN NDIS_HANDLE MiniportAdapterContext)
{
    IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);

    LOG("==> MiniportEnableInterrupt", 0);
    DBGOUT(("==> MiniportEnableInterrupt(0x%x)",  MiniportAdapterContext));
    if (!thisDev->resourcesReleased){
        thisDev->intEnabled = TRUE;
        SetCOMInterrupts(thisDev, TRUE);
    }
    LOG("<== MiniportEnableInterrupt", 1);
    DBGOUT(("<== MiniportEnableInterrupt(0x%x)",  MiniportAdapterContext));
}


/*
 *************************************************************************
 *  MiniportHalt
 *************************************************************************
 *
 *  Halts the network interface card.
 *
 */
VOID MiniportHalt(IN NDIS_HANDLE MiniportAdapterContext)
{
    IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);

    LOG("==> MiniportHalt", 0);
    DBGOUT(("==> MiniportHalt(0x%x)", MiniportAdapterContext));

    thisDev->hardwareStatus = NdisHardwareStatusClosing;
    /*
     *  Remove this device from our global list
     */

    if (thisDev == firstIrDevice){
        firstIrDevice = firstIrDevice->next;
    }
    else {
        IrDevice *dev;
        for (dev = firstIrDevice; dev && (dev->next != thisDev);
            dev = dev->next){}
        if (dev) {
            dev->next = dev->next->next;
        }
        else {
            /*
             * Don't omit this error check.  I've seen NDIS call
             * MiniportHalt with a bogus context when the system
             * gets corrupted.
             */
            LOG("Error: Bad context in MiniportHalt", 0);
            DBGERR(("Bad context in MiniportHalt"));
            return;
        }
    }


    /*
     *  Now destroy the device object.
     */
    NdisMDeregisterInterrupt(&thisDev->interruptObj);
    NSC_Shutdown(thisDev);
    DoClose(thisDev);
    if (thisDev->portInfo.ConfigIoBasePhysAddr)
    {
        NdisMDeregisterIoPortRange(thisDev->ndisAdapterHandle,
                                   thisDev->portInfo.ConfigIoBasePhysAddr,
                                   2,
                                   (PVOID)thisDev->portInfo.ConfigIoBaseAddr);
    }

    NdisMDeregisterIoPortRange(thisDev->ndisAdapterHandle,
                               thisDev->portInfo.ioBasePhys,
                               ((thisDev->CardType==PUMA108)?16:8),
                               (PVOID)thisDev->portInfo.ioBase);

    FreeDevice(thisDev);
    LOG("<== MiniportHalt", 1);
    DBGOUT(("<== MiniportHalt(0x%x)", MiniportAdapterContext));
}


/*
 *************************************************************************
 *  MiniportSyncHandleInterrupt
 *************************************************************************
 *
 * This function is called from MiniportHandleInterrupt via
 * NdisMSynchronizeWithInterrupt to synchronize with MiniportISR.  This is
 * required because the deferred procedure call (MiniportHandleInterrupt)
 * shares data with MiniportISR but cannot achieve mutual exclusion with a
 * spinlock because ISR's are not allowed to acquire spinlocks.
 * This function should be called WITH DEVICE LOCK HELD, however, to
 * synchronize with the rest of the miniport code (besides the ISR).
 * The device's IRQ is masked out in the PIC while this function executes,
 * so don't make calls up the stack.
 */
BOOLEAN MiniportSyncHandleInterrupt(PVOID MiniportAdapterContext)
{
    IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);

    // QueueLock should be held.

    LOG("==> MiniportSyncHandleInterrupt", 0);
    DBGOUT(("==> MiniportSyncHandleInterrupt(0x%x)", MiniportAdapterContext));


    LOG("<== MiniportSyncHandleInterrupt", 1);
    DBGOUT(("<== MiniportSyncHandleInterrupt(0x%x)", MiniportAdapterContext));
    return TRUE;
}

void InterlockedInsertBufferSorted(PLIST_ENTRY Head,
                                   rcvBuffer *rcvBuf,
                                   PNDIS_SPIN_LOCK Lock)
{
    PLIST_ENTRY ListEntry;

    NdisAcquireSpinLock(Lock);
    if (IsListEmpty(Head))
    {
        InsertHeadList(Head, &rcvBuf->listEntry);
    }
    else
    {
        BOOLEAN EntryInserted = FALSE;
        for (ListEntry = Head->Flink;
             ListEntry != Head;
             ListEntry = ListEntry->Flink)
        {
            rcvBuffer *temp = CONTAINING_RECORD(ListEntry,
                                                rcvBuffer,
                                                listEntry);
            if (temp->dataBuf > rcvBuf->dataBuf)
            {
                // We found one that comes after ours.
                // We need to insert before it

                InsertTailList(ListEntry, &rcvBuf->listEntry);
                EntryInserted = TRUE;
                break;
            }
        }
        if (!EntryInserted)
        {
            // We didn't find an entry on the last who's address was later
            // than our buffer.  We go at the end.
            InsertTailList(Head, &rcvBuf->listEntry);
        }
    }
    NdisReleaseSpinLock(Lock);
}

/*
 *************************************************************************
 *  DeliverFullBuffers
 *************************************************************************
 *
 *  Deliver received packets to the protocol.
 *  Return TRUE if delivered at least one frame.
 *
 */
BOOLEAN DeliverFullBuffers(IrDevice *thisDev)
{
    BOOLEAN result = FALSE;
    PLIST_ENTRY ListEntry;

    LOG("==> DeliverFullBuffers", 0);
    DBGOUT(("==> DeliverFullBuffers(0x%x)", thisDev));


    /*
     *  Deliver all full rcv buffers
     */

    for (
         ListEntry = NDISSynchronizedRemoveHeadList(&thisDev->rcvBufFull,
                                                    &thisDev->interruptObj);
         ListEntry;

         ListEntry = NDISSynchronizedRemoveHeadList(&thisDev->rcvBufFull,
                                                    &thisDev->interruptObj)
        )
    {
        rcvBuffer *rcvBuf = CONTAINING_RECORD(ListEntry,
                                              rcvBuffer,
                                              listEntry);
        NDIS_STATUS stat;
        PNDIS_BUFFER packetBuf;
        SLOW_IR_FCS_TYPE fcs;

        VerifyNdisPacket(rcvBuf->packet, 0);

        if (thisDev->currentSpeed <= MAX_SIR_SPEED) {
        /*
         * The packet we have already has had BOFs,
         * EOF, and * escape-sequences removed.  It
         * contains an FCS code at the end, which we
         * need to verify and then remove before
         * delivering the frame.  We compute the FCS
         * on the packet with the packet FCS attached;
         * this should produce the constant value
         * GOOD_FCS.
         */
            fcs = ComputeFCS(rcvBuf->dataBuf,
                             rcvBuf->dataLen);

            if (fcs != GOOD_FCS) {
            /*
             *  FCS Error.  Drop this frame.
             */
                LOG("Error: Bad FCS in DeliverFullBuffers", fcs);
                DBGERR(("Bad FCS in DeliverFullBuffers 0x%x!=0x%x.",
                        (UINT)fcs, (UINT) GOOD_FCS));
                rcvBuf->state = STATE_FREE;

                DBGSTAT(("Dropped %d/%d pkts; BAD FCS (%xh!=%xh):",
                         ++thisDev->packetsDropped,
                         thisDev->packetsDropped +
                         thisDev->packetsRcvd, fcs,
                         GOOD_FCS));

                DBGPRINTBUF(rcvBuf->dataBuf,
                            rcvBuf->dataLen);

                if (!rcvBuf->isDmaBuf)
                {
                    NDISSynchronizedInsertTailList(&thisDev->rcvBufBuf,
                                                   RCV_BUF_TO_LIST_ENTRY(rcvBuf->dataBuf),
                                                   &thisDev->interruptObj);
                }
                rcvBuf->dataBuf = NULL;
                rcvBuf->isDmaBuf = FALSE;

                VerifyNdisPacket(rcvBuf->packet, 0);
                NDISSynchronizedInsertHeadList(&thisDev->rcvBufFree,
                                               &rcvBuf->listEntry,
                                               &thisDev->interruptObj);

                //break;
                continue;
            }

        /* Remove the FCS from the end of the packet. */
            rcvBuf->dataLen -= SLOW_IR_FCS_SIZE;
        }
#ifdef DBG_ADD_PKT_ID
        if (addPktIdOn) {

            /* Remove dbg packet id. */
            USHORT uniqueId;
            rcvBuf->dataLen -= sizeof(USHORT);
            uniqueId = *(USHORT *)(rcvBuf->dataBuf+
                                   rcvBuf->dataLen);
            DBGOUT(("ID: RCVing packet %xh **",
                    (UINT)uniqueId));
            LOG("ID: Rcv Pkt id:", uniqueId);
        }
#endif

    /*
     * The packet array is set up with its NDIS_PACKET.
     * Now we need to allocate a single NDIS_BUFFER for
     * the NDIS_PACKET and set the NDIS_BUFFER to the
     * part of dataBuf that we want to deliver.
     */
        NdisAllocateBuffer(&stat, &packetBuf,
                           thisDev->bufferPoolHandle,
                           (PVOID)rcvBuf->dataBuf, rcvBuf->dataLen);

        if (stat != NDIS_STATUS_SUCCESS){
            LOG("Error: NdisAllocateBuffer failed", 0);
            DBGERR(("NdisAllocateBuffer failed"));
            ASSERT(0);
            break;
        }

        VerifyNdisPacket(rcvBuf->packet, 0);
        NdisChainBufferAtFront(rcvBuf->packet, packetBuf);
        LOG_PacketChain(thisDev, rcvBuf->packet);
        VerifyNdisPacket(rcvBuf->packet, 1);

    /*
     *  Fix up some other packet fields.
     */
        NDIS_SET_PACKET_HEADER_SIZE(rcvBuf->packet,
                                    IR_ADDR_SIZE+IR_CONTROL_SIZE);

        DBGPKT(("Indicating rcv packet 0x%x.", rcvBuf->packet));
        //DBGPRINTBUF(rcvBuf->dataBuf, rcvBuf->dataLen);

    /*
     * Indicate to the protocol that another packet is
     * ready.  Set the rcv buffer's state to PENDING first
     * to avoid a race condition with NDIS's call to the
     * return packet handler.
     */
        rcvBuf->state = STATE_PENDING;

        *(rcvBuffer **)rcvBuf->packet->MiniportReserved = rcvBuf;
        InterlockedInsertBufferSorted(&thisDev->rcvBufPend,
                                      rcvBuf,
                                      &thisDev->QueueLock);

        VerifyNdisPacket(rcvBuf->packet, 1);
        LOG_Data2(thisDev, rcvBuf->dataBuf);
        NdisMIndicateReceivePacket(thisDev->ndisAdapterHandle,
                                   &rcvBuf->packet, 1);
        result = TRUE;

        stat = NDIS_GET_PACKET_STATUS(rcvBuf->packet);
        if (stat == NDIS_STATUS_PENDING) {
        /*
         * The packet is being delivered asynchronously.
         * Leave the rcv buffer's state as PENDING;
         * we'll get a callback when the transfer is
         * complete.  Do NOT step firstRcvBufIndex.
         * We don't really need to break out here,
         * but we will anyways just to make things
         * simple.  This is ok since we get this
         * deferred interrupt callback for each packet
         * anyway.  It'll give the protocol a chance
         * to catch up.
         */
            LOG("Indicated rcv complete (Async) bytes:",
                rcvBuf->dataLen);
            DBGSTAT(("Rcv Pending. Rcvd %d packets",
                     ++thisDev->packetsRcvd));
        }
        else {
        /*
         * If there was an error, we are dropping this
         * packet; otherwise, this packet was delivered
         * synchronously.  We can free the packet
         * buffer and make this rcv frame available.
         */
            LOG_RemoveEntryList(thisDev, rcvBuf);
            NdisAcquireSpinLock(&thisDev->QueueLock);
            RemoveEntryList(&rcvBuf->listEntry);
            NdisReleaseSpinLock(&thisDev->QueueLock);
            LOG("Indicated rcv complete (sync) bytes:",
                rcvBuf->dataLen);

            LOG_PacketUnchain(thisDev, rcvBuf->packet);
            NdisUnchainBufferAtFront(rcvBuf->packet,
                                     &packetBuf);
            if (packetBuf){
                NdisFreeBuffer(packetBuf);
            }

            rcvBuf->state = STATE_FREE;
            VerifyNdisPacket(rcvBuf->packet, 0);

            if (!rcvBuf->isDmaBuf) {

                //
                // At SIR speeds, we manage a group of buffers that
                // we keep on the rcvBufBuf queue.
                //

                NDISSynchronizedInsertTailList(&thisDev->rcvBufBuf,
                                               RCV_BUF_TO_LIST_ENTRY(rcvBuf->dataBuf),
                                               &thisDev->interruptObj);
                // ASSERT the pointer is actually outside our FIR DMA buffer
                ASSERT(rcvBuf->dataBuf < thisDev->portInfo.dmaReadBuf ||
                       rcvBuf->dataBuf >= thisDev->portInfo.dmaReadBuf+RCV_DMA_SIZE);
            }

            rcvBuf->dataBuf = NULL;

            VerifyNdisPacket(rcvBuf->packet, 0);
            NDISSynchronizedInsertHeadList(&thisDev->rcvBufFree,
                                           &rcvBuf->listEntry,
                                           &thisDev->interruptObj);


            if (stat == NDIS_STATUS_SUCCESS){
                DBGSTAT(("Rcvd %d packets",
                         ++thisDev->packetsRcvd));
            }
            else {
                DBGSTAT(("Dropped %d/%d rcv packets.",
                         thisDev->packetsDropped++,
                         thisDev->packetsDropped +
                         thisDev->packetsRcvd));
            }
        }

    }

    LOG("<== DeliverFullBuffers", 1);
    DBGOUT(("<== DeliverFullBuffers"));
    return result;
}


/*
 *************************************************************************
 *  MiniportHandleInterrupt
 *************************************************************************
 *
 *
 *  This is the deferred interrupt processing routine (DPC) which is
 *  optionally called following an interrupt serviced by MiniportISR.
 *
 */
VOID MiniportHandleInterrupt(NDIS_HANDLE MiniportAdapterContext)
{
    IrDevice    *thisDev    =    CONTEXT_TO_DEV(   MiniportAdapterContext);

    LOG("==> MiniportHandleInterrupt", 0);
    DBGOUT(("==> MiniportHandleInterrupt(0x%x)", MiniportAdapterContext));

    if (thisDev->resourcesReleased){
        DBGOUT(("<== MiniportHandleInterrupt, blow off, no resources!"));
        return;
    }

    /*
     * If we finished the last send packet in the interrupt, we must change
     * speed.
     */
    if (thisDev->setSpeedNow){
        thisDev->setSpeedNow = FALSE;
        SetSpeed(thisDev);
    }

    /*
     * If we have just started receiving a packet, indicate media-busy
     * to the protocol.
     */
    if (thisDev->mediaBusy && !thisDev->haveIndicatedMediaBusy) {

        if (thisDev->currentSpeed > MAX_SIR_SPEED) {
            LOG("Error: MiniportHandleInterrupt is in wrong state",
                thisDev->currentSpeed);
            DBGERR(("MiniportHandleInterrupt is in wrong state: speed is 0x%x",
                    thisDev->currentSpeed));
            ASSERT(0);
        }

        NdisMIndicateStatus(thisDev->ndisAdapterHandle,
                            NDIS_STATUS_MEDIA_BUSY, NULL, 0);
        NdisMIndicateStatusComplete(thisDev->ndisAdapterHandle);

        thisDev->haveIndicatedMediaBusy = TRUE;
    }

    /* FIR mode */
    if (thisDev->currentSpeed > MAX_SIR_SPEED) {
        if (thisDev->portInfo.writePending) {
            FIR_MegaSendComplete(thisDev);
            /*
             * Any more Tx packets?
             */
            if (!IsListEmpty(&thisDev->SendQueue))
            {
            /* Kick off another Tx. */
                FIR_MegaSend(thisDev);
            }
            else {
                thisDev->IntMask = 0x04;
                SetupRecv(thisDev);
                /*
                 *  If we just sent the last frame to be sent at the old speed,
                 *  set the hardware to the new speed.
                 *  From OLD sytle!
                 */
                if (thisDev->setSpeedAfterCurrentSendPacket) {
                    thisDev->setSpeedAfterCurrentSendPacket = FALSE;
                    SetSpeed(thisDev);
                }
            }
        }
        else {

            FIR_DeliverFrames(thisDev);

            DeliverFullBuffers(thisDev);
        }
    }

    /* SIR mode */
    else {
        /*
         * We delivered a receive packet.  Update the rcv queue
         * 'first' and 'last' pointers.  We cannot use a spinlock
         * to coordinate accesses to the rcv buffers with the ISR,
         * since ISR's are not allowed to acquire spinlocks.  So
         * instead, we synchronize with the ISR using this special
         * mechanism.  MiniportSyncHandleInterrupt will do our work
         * for us with the IRQ masked out in the PIC.
         */
        if (DeliverFullBuffers(thisDev)) {
#if  0
            NdisMSynchronizeWithInterrupt(&thisDev->interruptObj,
                                          MiniportSyncHandleInterrupt,
                                          (PVOID)MiniportAdapterContext);
#endif
        }
        /*
         *  Send any pending write packets if possible.
         */
        if (IsCommReadyForTransmit(thisDev)) {
            PortReadyForWrite(thisDev, TRUE);
        }
    }

    LOG("<== MiniportHandleInterrupt", 1);
    DBGOUT(("<== MiniportHandleInterrupt"));

}

/*
 *************************************************************************
 *  GetPnPResources
 *************************************************************************
 *
 *
 */
BOOLEAN GetPnPResources(IrDevice *thisDev, NDIS_HANDLE WrapperConfigurationContext)
{
	NDIS_STATUS stat;
    BOOLEAN result = FALSE;

    /*
     *  We should only need 2 adapter resources (2 IO and 1 interrupt),
     *  but I've seen devices get extra resources.
     *  So give the NdisMQueryAdapterResources call room for 10 resources.
     */
    #define RESOURCE_LIST_BUF_SIZE (sizeof(NDIS_RESOURCE_LIST) + (10*sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)))

    UCHAR buf[RESOURCE_LIST_BUF_SIZE];
    PNDIS_RESOURCE_LIST resList = (PNDIS_RESOURCE_LIST)buf;
    UINT bufSize = RESOURCE_LIST_BUF_SIZE;

    NdisMQueryAdapterResources(&stat, WrapperConfigurationContext, resList, &bufSize);
    if (stat == NDIS_STATUS_SUCCESS){
        PCM_PARTIAL_RESOURCE_DESCRIPTOR resDesc;
        BOOLEAN     haveIRQ = FALSE,
                    haveIOAddr = FALSE,
                    haveDma = FALSE;
        UINT i;

        for (resDesc = resList->PartialDescriptors, i = 0;
             i < resList->Count;
             resDesc++, i++){

            switch (resDesc->Type){
                case CmResourceTypePort:
                    if (thisDev->CardType==PC87108 &&
                        (resDesc->u.Port.Start.LowPart==0xEA ||
                         resDesc->u.Port.Start.LowPart==0x398 ||
                         resDesc->u.Port.Start.LowPart==0x150))
                    {
                        // This is an eval board and this is the config io base address

                        thisDev->portInfo.ConfigIoBasePhysAddr = resDesc->u.Port.Start.LowPart;
                    }
                    else if (thisDev->CardType==PC87308 &&
                             (resDesc->u.Port.Start.LowPart==0x2E ||
                              resDesc->u.Port.Start.LowPart==0x15C))
                    {
                        // This is an eval board and this is the config io base address

                        thisDev->portInfo.ConfigIoBasePhysAddr = resDesc->u.Port.Start.LowPart;
                    }
                    else if (thisDev->CardType==PC87338 &&
                             (resDesc->u.Port.Start.LowPart==0x2E ||
                              resDesc->u.Port.Start.LowPart==0x398 ||
                              resDesc->u.Port.Start.LowPart==0x15C))
                    {
                        // This is an eval board and this is the config io base address

                        thisDev->portInfo.ConfigIoBasePhysAddr = resDesc->u.Port.Start.LowPart;
                    }
                    else
                    {
                        if (haveIOAddr){
                            /*
                             *  The *PNP0510 chip on the IBM ThinkPad 760EL
                             *  gets an extra IO range assigned to it.
                             *  So only pick up the first IO port range;
                             *  ignore this subsequent one.
                             */
                            DBGERR(("Ignoring extra PnP IO base %xh because already using %xh.",
                                      (UINT)resDesc->u.Port.Start.LowPart,
                                      (UINT)thisDev->portInfo.ioBasePhys));
                        }
                        else {
                            thisDev->portInfo.ioBasePhys = resDesc->u.Port.Start.LowPart;
                            haveIOAddr = TRUE;
                            DBGOUT(("Got UART IO addr: %xh.", thisDev->portInfo.ioBasePhys));
                        }
                    }
                    break;

                case CmResourceTypeInterrupt:
                    if (haveIRQ){
                        DBGERR(("Ignoring second PnP IRQ %xh because already using %xh.",
                                (UINT)resDesc->u.Interrupt.Level, thisDev->portInfo.irq));
                    }
                    else {
	                    thisDev->portInfo.irq = resDesc->u.Interrupt.Level;
                        haveIRQ = TRUE;
                        DBGOUT(("Got PnP IRQ: %d.", thisDev->portInfo.irq));
                    }
                    break;

                case CmResourceTypeDma:
                    if (haveDma){
                        DBGERR(("Ignoring second DMA address %d because already using %d.",
                                (UINT)resDesc->u.Dma.Channel, (UINT)thisDev->portInfo.DMAChannel));
                    }
                    else {
                        ASSERT(!(resDesc->u.Dma.Channel&0xffffff00));
                        thisDev->portInfo.DMAChannel = (UCHAR)resDesc->u.Dma.Channel;
                        haveDma = TRUE;
                        DBGOUT(("Got DMA channel: %d.", thisDev->portInfo.DMAChannel));
                    }
                    break;
            }
        }

        result = (haveIOAddr && haveIRQ && haveDma);
    }

    return result;
}


/*
 *************************************************************************
 *  Configure
 *************************************************************************
 *
 *  Read configurable parameters out of the system registry.
 *
 */
BOOLEAN Configure(
                 IrDevice *thisDev,
                 NDIS_HANDLE WrapperConfigurationContext
                 )
{
    //
    // Status of Ndis calls.
    //
    NDIS_STATUS Status;

    //
    // The handle for reading from the registry.
    //
    NDIS_HANDLE ConfigHandle;

    //
    // TRUE if there is a configuration error.
    //
    BOOLEAN ConfigError = FALSE;

    //
    // A special value to log concerning the error.
    //
    ULONG ConfigErrorValue = 0;

    ULONG SlotNumber;

    ULONG PCI_ConfigInfo = 0;

     //
    // The value read from the registry.
    //
    PNDIS_CONFIGURATION_PARAMETER ReturnedValue;

    //
    // String names of all the parameters that will be read.
    //
    NDIS_STRING CardTypeStr         = CARDTYPE;
    NDIS_STRING ConfigIOAddressStr  = CONFIGIOADDRESS;
    NDIS_STRING UartIOAddressStr    = UARTIOADDRESS;
    NDIS_STRING InterruptStr        = INTERRUPT;
    NDIS_STRING DMAChannelStr       = DMACHANNEL;
    NDIS_STRING Dongle_A_TypeStr	= DONGLE_A_TYPE;
    NDIS_STRING Dongle_B_TypeStr	= DONGLE_B_TYPE;
    NDIS_STRING MaxConnectRateStr   = MAXCONNECTRATE;


    UCHAR Valid_DMAChannels[] = VALID_DMACHANNELS;
    UINT Valid_DongleTypes[] = VALID_DONGLETYPES;

    DBGOUT(("Configure(0x%x)", thisDev));
    NdisOpenConfiguration(&Status, &ConfigHandle, WrapperConfigurationContext);
    if (Status != NDIS_STATUS_SUCCESS){
        DBGERR(("NdisOpenConfiguration failed in Configure()"));
        return FALSE;
    }
    //
    // Read Ir108 Configuration I/O base Address
    //
    //DbgBreakPoint();
    NdisReadConfiguration(
                         &Status,
                         &ReturnedValue,
                         ConfigHandle,
                         &CardTypeStr,
                         NdisParameterHexInteger
                         );
    if (Status != NDIS_STATUS_SUCCESS){
        DBGERR(("NdisReadConfiguration failed in accessing CardType."));
        return FALSE;
    }
    thisDev->CardType = (UCHAR)ReturnedValue->ParameterData.IntegerData;

    if (thisDev->CardType <= PUMA108 ){
#if 0
        if (thisDev->CardType < PNPUIR ){
            //
            // Read Ir108 Configuration I/O base Address
            //
            NdisReadConfiguration(
                                 &Status,
                                 &ReturnedValue,
                                 ConfigHandle,
                                 &ConfigIOAddressStr,
                                 NdisParameterHexInteger
                                 );
            if (Status != NDIS_STATUS_SUCCESS){
                DBGERR(("NdisReadConfiguration failed in accessing ConfigIoBaseAddr."));
                return FALSE;
            }
            else
                thisDev->portInfo.ConfigIoBaseAddr = (UINT)(ReturnedValue->ParameterData.IntegerData);
        }
#endif
        if (!GetPnPResources(thisDev, WrapperConfigurationContext)){
            DBGERR(("GetPnPResources failed\n"));

            // We failed to get resources, try the legacy method.
            if (thisDev->CardType < PNPUIR ){
                //
                // Read Ir108 Configuration I/O base Address
                //
                NdisReadConfiguration(
                                     &Status,
                                     &ReturnedValue,
                                     ConfigHandle,
                                     &ConfigIOAddressStr,
                                     NdisParameterHexInteger
                                     );
                if (Status != NDIS_STATUS_SUCCESS){
                    DBGERR(("NdisReadConfiguration failed in accessing ConfigIoBaseAddr."));
                    return FALSE;
                }
                else
                    thisDev->portInfo.ConfigIoBaseAddr = (UINT)(ReturnedValue->ParameterData.IntegerData);
            }

            //
            // Read I/O Address
            //
            NdisReadConfiguration(
                                 &Status,
                                 &ReturnedValue,
                                 ConfigHandle,
                                 &UartIOAddressStr,
                                 NdisParameterHexInteger
                                 );

            if (Status != NDIS_STATUS_SUCCESS){
                DBGERR(("NdisReadConfiguration failed in accessing UartIoBaseAddr."));
                return FALSE;
            }
            thisDev->portInfo.ioBase = (UINT)(ReturnedValue->ParameterData.IntegerData);

            //
            // Read interrupt number
            //
            NdisReadConfiguration(
                                 &Status,
                                 &ReturnedValue,
                                 ConfigHandle,
                                 &InterruptStr,
                                 NdisParameterHexInteger
                                 );
            if (Status != NDIS_STATUS_SUCCESS){
                DBGERR(("NdisReadConfiguration failed in accessing InterruptNumber."));
                return FALSE;
            }
            thisDev->portInfo.irq = (UINT)(ReturnedValue->ParameterData.IntegerData);

            //
            // Read Receive DMA channel Number.
            //
            NdisReadConfiguration(
                                 &Status,
                                 &ReturnedValue,
                                 ConfigHandle,
                                 &DMAChannelStr,
                                 NdisParameterHexInteger
                                 );

            if (Status != NDIS_STATUS_SUCCESS){
                DBGERR(("NdisReadConfiguration failed in accessing DMAChannel (0x%x).",Status));
            }
            thisDev->portInfo.DMAChannel = (UCHAR) (ReturnedValue->ParameterData.IntegerData);

        }
    }



    // Read Dongle type constant Number.
    //
    NdisReadConfiguration(&Status,
			  &ReturnedValue,
			  ConfigHandle,
			  &Dongle_A_TypeStr,
			  NdisParameterInteger);

    if (Status != NDIS_STATUS_SUCCESS){
    	DBGERR(("NdisReadConfiguration failed in accessing DongleType (0x%x).",Status));
    }
    thisDev->DonglesSupported = 1;
    thisDev->DongleTypes[0] =
	(UCHAR)Valid_DongleTypes[(UCHAR)ReturnedValue->ParameterData.IntegerData];

    // Read Dongle type constant Number.
    //
    NdisReadConfiguration(&Status,
			  &ReturnedValue,
			  ConfigHandle,
			  &Dongle_B_TypeStr,
			  NdisParameterInteger);

    if (Status != NDIS_STATUS_SUCCESS){
    	 DBGERR(("NdisReadConfiguration failed in accessing DongleType (0x%x).",
		  Status));
    }
    thisDev->DongleTypes[1] = (UCHAR)Valid_DongleTypes[(UCHAR)ReturnedValue->ParameterData.IntegerData];
    thisDev->DonglesSupported++;

    // Read MaxConnectRate.
    //
    NdisReadConfiguration(&Status,
			  &ReturnedValue,
			  ConfigHandle,
			  &MaxConnectRateStr,
			  NdisParameterInteger);

    if (Status != NDIS_STATUS_SUCCESS){
    	DBGERR(("NdisReadConfiguration failed in accessing MaxConnectRate (0x%x).",Status));
        thisDev->AllowedSpeedMask = ALL_IRDA_SPEEDS;
    }
    else
    {
        thisDev->AllowedSpeedMask = 0;

        switch (ReturnedValue->ParameterData.IntegerData)
        {
            default:
            case 4000000:
                thisDev->AllowedSpeedMask |= NDIS_IRDA_SPEED_4M;
            case 1152000:
                thisDev->AllowedSpeedMask |= NDIS_IRDA_SPEED_1152K;
            case 115200:
                thisDev->AllowedSpeedMask |= NDIS_IRDA_SPEED_115200;
            case 57600:
                thisDev->AllowedSpeedMask |= NDIS_IRDA_SPEED_57600;
            case 38400:
                thisDev->AllowedSpeedMask |= NDIS_IRDA_SPEED_38400;
            case 19200:
                thisDev->AllowedSpeedMask |= NDIS_IRDA_SPEED_19200;
            case 9600:
                thisDev->AllowedSpeedMask |= NDIS_IRDA_SPEED_9600 | NDIS_IRDA_SPEED_2400;
                break;
        }

    }


    NdisCloseConfiguration(ConfigHandle);

    if (thisDev->CardType > PUMA108 ){
        for (SlotNumber = 0;(SlotNumber < 255) && (PCI_ConfigInfo != 0x11000B10);SlotNumber++)
            NdisImmediateReadPciSlotInformation(
                                               WrapperConfigurationContext,
                                               SlotNumber,
                                               (ULONG)0,
                                               (PVOID *)&PCI_ConfigInfo,
                                               (ULONG) sizeof(ULONG)
                                               );
        if ( SlotNumber == 255) return FALSE;
     //
     // Read the Scatter Gather DMA IO base address register setup.
     //
        NdisImmediateReadPciSlotInformation(
                                           WrapperConfigurationContext,
                                           SlotNumber,
                                           (ULONG)50,
                                           (PVOID *)&(thisDev->sgIO_Base),
                                           (ULONG) sizeof(ULONG)
                                           );
     //
     // Read the UART IO base address register setup.
     //
        NdisImmediateReadPciSlotInformation(
                                           WrapperConfigurationContext,
                                           SlotNumber,
                                           (ULONG)98,
                                           (PVOID *)&(thisDev->portInfo.ioBase),
                                           (ULONG) sizeof(UINT)
                                           );
     //
     // Read the UART interrupt line setup.
     //
        NdisImmediateReadPciSlotInformation(
                                           WrapperConfigurationContext,
                                           SlotNumber,
                                           (ULONG)98,
                                           (PVOID *)&PCI_ConfigInfo,
                                           (ULONG) sizeof(ULONG)
                                           );
     //
     // Mask off bits refering to interrup lines used by serial port
     // 2.
     //
        PCI_ConfigInfo &= 0xF0;
        thisDev->portInfo.irq = (UINT)(PCI_ConfigInfo >> 4);
    }

    DBGOUT(("Configure done: ConfigIO=0x%x UartIO=0x%x irq=%d DMA=%d",
            thisDev->portInfo.ConfigIoBaseAddr,thisDev->portInfo.ioBase,
            thisDev->portInfo.irq,thisDev->portInfo.DMAChannel));

    return TRUE;
}


/*
 *************************************************************************
 *  MiniportInitialize
 *************************************************************************
 *
 *
 *  Initializes the network interface card.
 *
 *
 *
 */
NDIS_STATUS MiniportInitialize  (   PNDIS_STATUS OpenErrorStatus,
                                    PUINT SelectedMediumIndex,
                                    PNDIS_MEDIUM MediumArray,
                                    UINT MediumArraySize,
                                    NDIS_HANDLE NdisAdapterHandle,
                                    NDIS_HANDLE WrapperConfigurationContext
                                )
{
    UINT mediumIndex;
    IrDevice *thisDev = NULL;
    NDIS_STATUS retStat, result = NDIS_STATUS_SUCCESS;

    DBGOUT(("MiniportInitialize()"));

    /*
     *  Search the passed-in array of supported media for the IrDA medium.
     */
    for (mediumIndex = 0; mediumIndex < MediumArraySize; mediumIndex++){
        if (MediumArray[mediumIndex] == NdisMediumIrda){
            break;
        }
    }
    if (mediumIndex < MediumArraySize){
        *SelectedMediumIndex = mediumIndex;
    }
    else {
        /*
         *  Didn't see the IrDA medium
         */
        DBGERR(("Didn't see the IRDA medium in MiniportInitialize"));
        result = NDIS_STATUS_UNSUPPORTED_MEDIA;
        goto _initDone;
    }

    /*
     *  Allocate a new device object to represent this connection.
     */
    thisDev = NewDevice();
    if (!thisDev){
        return NDIS_STATUS_NOT_ACCEPTED;
    }

    thisDev->hardwareStatus = NdisHardwareStatusInitializing;
    /*
     *  Allocate resources for this connection.
     */
    if (!OpenDevice(thisDev)){
        DBGERR(("OpenDevice failed"));
        result = NDIS_STATUS_FAILURE;
        goto _initDone;
    }


    /*
     *  Read the system registry to get parameters like COM port number, etc.
     */
    if (!Configure(thisDev, WrapperConfigurationContext)){
        DBGERR(("Configure failed"));
        result = NDIS_STATUS_FAILURE;
        goto _initDone;
    }


    if (thisDev->CardType > PUMA108){
        MyMemFree(thisDev->portInfo.writeBuf, MAX_IRDA_DATA_SIZE * 8, TRUE);
        NdisMAllocateSharedMemory(
                                 NdisAdapterHandle,
                                 (sizeof(DescTableEntry) * 8),
                                 FALSE,
                                 (PVOID )&thisDev->portInfo.writeBuf,
                                 (PVOID )&thisDev->SGDMA_BuffPhyAddr
                                 );
        thisDev->SGDMA_Buff = thisDev->portInfo.writeBuf;
    }
    /*
     *  This call will associate our adapter handle with the wrapper's
     *  adapter handle.  The wrapper will then always use our handle
     *  when calling us.  We use a pointer to the device object as the context.
     */
    NdisMSetAttributesEx(NdisAdapterHandle,
                         (NDIS_HANDLE)thisDev,
                         0,
                         NDIS_ATTRIBUTE_IGNORE_REQUEST_TIMEOUT |
                         NDIS_ATTRIBUTE_DESERIALIZE,
                         NdisInterfaceInternal);


    /*
     *  Tell NDIS about the range of IO space that we'll be using.
     *  Puma uses Chip-select mode, so the ConfigIOBase address actually
     *  follows the regular io, so get both in one shot.
     */
    retStat = NdisMRegisterIoPortRange( (PVOID)&thisDev->portInfo.ioBase,
                                        NdisAdapterHandle,
                                        thisDev->portInfo.ioBasePhys,
                                        ((thisDev->CardType==PUMA108)?16:8));
    if (retStat != NDIS_STATUS_SUCCESS){
        DBGERR(("NdisMRegisterIoPortRange failed"));
        result = NDIS_STATUS_FAILURE;
        goto _initDone;
    }

    if (thisDev->portInfo.ConfigIoBasePhysAddr)
    {
        /*
         *  Eval boards require a second IO range.
         *
         */
        retStat = NdisMRegisterIoPortRange( (PVOID)&thisDev->portInfo.ConfigIoBaseAddr,
                                            NdisAdapterHandle,
                                            thisDev->portInfo.ConfigIoBasePhysAddr,
                                            2);
        if (retStat != NDIS_STATUS_SUCCESS){
            DBGERR(("NdisMRegisterIoPortRange config failed"));
            result = NDIS_STATUS_FAILURE;
            goto _initDone;
        }
    }

    /*
     *  Record the NDIS wrapper's handle for this adapter, which we use
     *  when we call up to the wrapper.
     *  (This miniport's adapter handle is just thisDev, the pointer to the device object.).
     */
    DBGOUT(("NDIS handle: %xh <-> IRMINI handle: %xh", NdisAdapterHandle, thisDev));
    thisDev->ndisAdapterHandle = NdisAdapterHandle;


    /*
     *  Open COMM communication channel.
     *  This will let the dongle driver update its capabilities from their default values.
     */
    if (!DoOpen(thisDev)){
        DBGERR(("DoOpen failed"));
        result = NDIS_STATUS_FAILURE;
        goto _initDone;
    }


    /*
     *  Do special NSC setup
     *  (do this after comport resources, like read buf, have been allocated).
     */
    if (!NSC_Setup(thisDev)){
        DBGERR(("NSC_Setup failed"));
        result = NDIS_STATUS_FAILURE;
        goto _initDone;
    }


    /*
     *  Register an interrupt with NDIS.
     */
    retStat = NdisMRegisterInterrupt(   (PNDIS_MINIPORT_INTERRUPT)&thisDev->interruptObj,
                                        NdisAdapterHandle,
                                        thisDev->portInfo.irq,
                                        thisDev->portInfo.irq,
                                        TRUE,   // want ISR
                                        TRUE,   // MUST share interrupts
                                        NdisInterruptLatched
                                    );
    if (retStat != NDIS_STATUS_SUCCESS){
        DBGERR(("NdisMRegisterInterrupt failed"));
        result = NDIS_STATUS_FAILURE;
        goto _initDone;
    }

    _initDone:
    if (result == NDIS_STATUS_SUCCESS){

        /*
         *  Add this device object to the beginning of our global list.
         */
        thisDev->next = firstIrDevice;
        firstIrDevice = thisDev;
        thisDev->resourcesReleased=FALSE;
        thisDev->hardwareStatus = NdisHardwareStatusReady;
        DBGOUT(("MiniportInitialize succeeded"));
    }
    else {
        if (thisDev){
            FreeDevice(thisDev);
        }
        DBGOUT(("MiniportInitialize failed"));
    }
    return result;

}



/*
 *************************************************************************
 * QueueReceivePacket
 *************************************************************************
 *
 *
 *
 *
 */
VOID QueueReceivePacket(IrDevice *thisDev, PUCHAR data, UINT dataLen, BOOLEAN IsFIR)
{
    rcvBuffer *rcvBuf = NULL;
    PLIST_ENTRY ListEntry;

    /*
     * Note: We cannot use a spinlock to protect the rcv buffer structures
     * in an ISR.  This is ok, since we used a sync-with-isr function
     * the the deferred callback routine to access the rcv buffers.
     */

    LOG("==> QueueReceivePacket", 0);
    DBGOUT(("==> QueueReceivePacket(0x%x, 0x%lx, 0x%x)",
            thisDev, data, dataLen));
    LOG("QueueReceivePacket, len: ", dataLen);

    if (!IsFIR)
    {
        // This function is called inside the ISR during SIR mode.
        if (IsListEmpty(&thisDev->rcvBufFree))
        {
            ListEntry = NULL;
        }
        else
        {
            ListEntry = RemoveHeadList(&thisDev->rcvBufFree);
        }
    }
    else
    {
        ListEntry = NDISSynchronizedRemoveHeadList(&thisDev->rcvBufFree,
                                                   &thisDev->interruptObj);
    }
    if (ListEntry)
    {
        rcvBuf = CONTAINING_RECORD(ListEntry,
                                   rcvBuffer,
                                   listEntry);
        if (IsFIR)
        {
            LOG_Data(thisDev, data);
        }
    }




    if (rcvBuf){
        rcvBuf->dataBuf = data;

        VerifyNdisPacket(rcvBuf->packet, 0);
        rcvBuf->state = STATE_FULL;
        rcvBuf->dataLen = dataLen;


        if (!IsFIR)
        {
            rcvBuf->isDmaBuf = FALSE;
            InsertTailList(&thisDev->rcvBufFull,
                           ListEntry);
        }
        else
        {
            rcvBuf->isDmaBuf = TRUE;
            LOG_InsertTailList(thisDev, &thisDev->rcvBufFull, rcvBuf);
            NDISSynchronizedInsertTailList(&thisDev->rcvBufFull,
                                           ListEntry,
                                           &thisDev->interruptObj);
        }
    }
    LOG("<== QueueReceivePacket", 1);
    DBGOUT(("<== QueueReceivePacket"));
}


/*
 *************************************************************************
 * MiniportISR
 *************************************************************************
 *
 *
 *  This is the miniport's interrupt service routine (ISR).
 *
 *
 */
VOID MiniportISR(PBOOLEAN InterruptRecognized,
                 PBOOLEAN QueueMiniportHandleInterrupt,
                 NDIS_HANDLE MiniportAdapterContext)
{
    IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);
    UCHAR BankSelect;

    //LOG("==> MiniportISR", ++thisDev->interruptCount);
    //DBGOUT(("==> MiniportISR(0x%x), interrupt #%d)", (UINT)thisDev,
    //					thisDev->interruptCount));
    if (!thisDev->resourcesReleased){
        BankSelect = GetCOMPort(thisDev->portInfo.ioBase, 3);

        SetCOMPort(thisDev->portInfo.ioBase, 3, (UCHAR)(BankSelect & 0x7f));
        /*
         *  Service the interrupt.
         */
        if (thisDev->currentSpeed > MAX_SIR_SPEED){
            NSC_FIR_ISR(thisDev, InterruptRecognized,
                        QueueMiniportHandleInterrupt);
        }
        else {
            COM_ISR(thisDev, InterruptRecognized,
                    QueueMiniportHandleInterrupt);
        }

        SetCOMPort(thisDev->portInfo.ioBase, 3, BankSelect);
    }
    LOG("<== MiniportISR", 1);
    DBGOUT(("<== MiniportISR"));
}


/*
 *************************************************************************
 *  MiniportReconfigure
 *************************************************************************
 *
 *
 *  Reconfigures the network interface card to new parameters available
 *  in the NDIS library configuration functions.
 *
 *
 */
NDIS_STATUS MiniportReconfigure (   OUT PNDIS_STATUS OpenErrorStatus,
                                    IN NDIS_HANDLE MiniportAdapterContext,
                                    IN NDIS_HANDLE WrapperConfigurationContext
                                )
{
    IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);
    NDIS_STATUS result;

    DBGOUT(("MiniportReconfigure(0x%x)", MiniportAdapterContext));

    MiniportHalt(MiniportAdapterContext);

    if (Configure(thisDev, WrapperConfigurationContext)){
        result = NDIS_STATUS_SUCCESS;
    }
    else {
        result = NDIS_STATUS_FAILURE;
    }

    DBGOUT(("MiniportReconfigure"));
    *OpenErrorStatus = result;
    return result;
}


VOID MiniportResetCallback(PNDIS_WORK_ITEM pWorkItem, PVOID pVoid)
{
    IrDevice *thisDev = pWorkItem->Context;

    NdisFreeMemory(pWorkItem, sizeof(NDIS_WORK_ITEM), 0);

#if 0
    DoClose(thisDev);
    CloseDevice(thisDev);
    OpenDevice(thisDev);
    DoOpen(thisDev);
#else
    CloseCOM(thisDev);
    OpenCOM(thisDev);
#endif

    thisDev->hardwareStatus = NdisHardwareStatusReady;

    NdisMResetComplete(thisDev->ndisAdapterHandle,
                       NDIS_STATUS_SUCCESS,
                       TRUE);  // Addressing reset
}

/*
 *************************************************************************
 * MiniportReset
 *************************************************************************
 *
 *
 *  MiniportReset issues a hardware reset to the network interface card.
 *  The miniport driver also resets its software state.
 *
 *
 */
NDIS_STATUS MiniportReset(PBOOLEAN AddressingReset, NDIS_HANDLE MiniportAdapterContext)
{
    IrDevice *dev, *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);
    NDIS_STATUS result = NDIS_STATUS_PENDING;
    PNDIS_WORK_ITEM pWorkItem;
    NDIS_PHYSICAL_ADDRESS noMaxAddr = NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);

    DBGERR(("MiniportReset(0x%x)", MiniportAdapterContext));


    /*
     *  Sanity check on the context
     */
    for (dev = firstIrDevice; dev && (dev != thisDev); dev = dev->next){}
    if (!dev){
        DBGERR(("Bad context in MiniportReset"));
        return NDIS_STATUS_FAILURE;
    }

    thisDev->hardwareStatus = NdisHardwareStatusReset;

    result = NdisAllocateMemory(&pWorkItem, sizeof(NDIS_WORK_ITEM), 0, noMaxAddr);
    if (!pWorkItem)
    {
        thisDev->hardwareStatus = NdisHardwareStatusReady;
        return result;
    }

    NdisInitializeWorkItem(pWorkItem, MiniportResetCallback, thisDev);
    result = NdisScheduleWorkItem(pWorkItem);

    if (result!=NDIS_STATUS_SUCCESS)
    {
        NdisFreeMemory(pWorkItem, sizeof(NDIS_WORK_ITEM), 0);
        thisDev->hardwareStatus = NdisHardwareStatusReady;
        return result;
    }

    *AddressingReset = TRUE;

    DBGOUT(("MiniportReset done."));
    return NDIS_STATUS_PENDING;
}

/*
 *************************************************************************
 *  MiniportSend
 *************************************************************************
 *
 *
 *  Transmits a packet through the network interface card onto the medium.
 *
 *
 *
 */
NDIS_STATUS MiniportSend(
                        IN NDIS_HANDLE MiniportAdapterContext,
                        IN PNDIS_PACKET Packet,
                        IN UINT Flags
                        )
{
    IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);
    NDIS_STATUS result;

    DEBUGMSG(DBG_TRACE_TX, ("m"));
    DBGOUT(("MiniportSend(thisDev=0x%x)", thisDev));

    /*
     *  If we have temporarily lost access to the hardware, don't queue up any sends.
     *  Just pretend everything is going smoothly until we regain access to the hw.
     */
    if (thisDev->resourcesReleased){
        return NDIS_STATUS_SUCCESS;
    }

    DBGPKT(("Queueing send packet 0x%x.", Packet));
    //
    // Use MiniportReserved as a LIST_ENTRY.  First check so no one
    // ever changes the size of these the wrong way.
    //
    ASSERT(sizeof(Packet->MiniportReserved)>=sizeof(LIST_ENTRY));

    NdisInterlockedInsertTailList(&thisDev->SendQueue,
                                  (PLIST_ENTRY)Packet->MiniportReserved,
                                  &thisDev->QueueLock);

    /*
     *  Try to send the first queued send packet.
     */
    if (IsCommReadyForTransmit(thisDev)){
        BOOLEAN firstBufIsPending;

        firstBufIsPending = (BOOLEAN)(Packet != HEAD_SEND_PACKET(thisDev));

        result = PortReadyForWrite(thisDev, firstBufIsPending);
    }
    else {
        result = NDIS_STATUS_PENDING;
    }

    DBGOUT(("MiniportSend returning %s", DBG_NDIS_RESULT_STR(result)));
    return result;
}



/*
 *************************************************************************
 *  MiniportTransferData
 *************************************************************************
 *
 *
 *  Copies the contents of the received packet to a specified packet buffer.
 *
 *
 *
 */
NDIS_STATUS MiniportTransferData    (
                                    OUT PNDIS_PACKET   Packet,
                                    OUT PUINT          BytesTransferred,
                                    IN NDIS_HANDLE     MiniportAdapterContext,
                                    IN NDIS_HANDLE     MiniportReceiveContext,
                                    IN UINT            ByteOffset,
                                    IN UINT            BytesToTransfer
                                    )
{

    DBGERR(("MiniportTransferData - should not get called."));

    /*
     *  We always pass the entire packet up in the indicate-receive call,
     *  so we will never get this callback.
     *  (We can't do anything but return failure anyway,
     *   since NdisMIndicateReceivePacket does not pass up a packet context).
     */
    *BytesTransferred = 0;
    return NDIS_STATUS_FAILURE;
}


/*
 *************************************************************************
 *  ReturnPacketHandler
 *************************************************************************
 *
 *  When NdisMIndicateReceivePacket returns asynchronously,
 *  the protocol returns ownership of the packet to the miniport via this function.
 *
 */
VOID ReturnPacketHandler(NDIS_HANDLE MiniportAdapterContext, PNDIS_PACKET Packet)
{
    IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);
    rcvBuffer *rcvBuf;

    DBGOUT(("ReturnPacketHandler(0x%x)", MiniportAdapterContext));
    RegStats.ReturnPacketHandlerCalled++;

    //
    // MiniportReserved contains the pointer to our rcvBuffer
    //

    rcvBuf = *(rcvBuffer**) Packet->MiniportReserved;

    VerifyNdisPacket(Packet, 1);

    if (rcvBuf->state == STATE_PENDING){
        PNDIS_BUFFER ndisBuf;

        DBGPKT(("Reclaimed rcv packet 0x%x.", Packet));

        LOG_RemoveEntryList(thisDev, rcvBuf);
        NDISSynchronizedRemoveEntryList(&rcvBuf->listEntry, &thisDev->interruptObj);

        LOG_PacketUnchain(thisDev, rcvBuf->packet);
        NdisUnchainBufferAtFront(Packet, &ndisBuf);
        if (ndisBuf){
            NdisFreeBuffer(ndisBuf);
        }

        if (!rcvBuf->isDmaBuf)
        {
            NDISSynchronizedInsertTailList(&thisDev->rcvBufBuf,
                                           RCV_BUF_TO_LIST_ENTRY(rcvBuf->dataBuf),
                                           &thisDev->interruptObj);
            // ASSERT the pointer is actually outside our FIR DMA buffer
            ASSERT(rcvBuf->dataBuf < thisDev->portInfo.dmaReadBuf ||
                   rcvBuf->dataBuf >= thisDev->portInfo.dmaReadBuf+RCV_DMA_SIZE);
        }
        rcvBuf->dataBuf = NULL;

        rcvBuf->state = STATE_FREE;

        VerifyNdisPacket(rcvBuf->packet, 0);
        NDISSynchronizedInsertHeadList(&thisDev->rcvBufFree,
                                       &rcvBuf->listEntry,
                                       &thisDev->interruptObj);
    }
    else {
        LOG("Error: Packet in ReturnPacketHandler was "
            "not PENDING", 0);
        DBGERR(("Packet in ReturnPacketHandler was not PENDING."));
    }
    VerifyNdisPacket(rcvBuf->packet, 1);

}



/*
 *************************************************************************
 *  SendPacketsHandler
 *************************************************************************
 *
 *  Send an array of packets simultaneously.
 *
 */
VOID SendPacketsHandler(NDIS_HANDLE MiniportAdapterContext,
                        PPNDIS_PACKET PacketArray, UINT NumberofPackets)
{
    NDIS_STATUS stat;
    BOOLEAN TxWasActive;
    UINT i;

    IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);

    DEBUGMSG(DBG_TRACE_TX, ("M"));
    LOG("==> SendPacketsHandler", 0);
    DBGOUT(("==> SendPacketsHandler(0x%x)", MiniportAdapterContext));

    //
    //  If we have temporarily lost access to the hardware, don't queue up any sends.
    //  Just pretend everything is going smoothly until we regain access to the hw.
    //
    if (thisDev->resourcesReleased){
        return;
    }

    LOG("Number of transmit Burst ", NumberofPackets);

    //
    // NDIS gives us the PacketArray, but it is not ours to keep, so we have to take
    // the packets out and store them elsewhere.
    //

    if (thisDev->currentSpeed > MAX_SIR_SPEED) {
        NdisAcquireSpinLock(&thisDev->QueueLock);
        TxWasActive = (thisDev->AdapterState==ADAPTER_TX);
        thisDev->AdapterState = ADAPTER_TX;
        for (i = 0; i < NumberofPackets; i++) {
            NDIS_SET_PACKET_STATUS(PacketArray[i],
                                   NDIS_STATUS_PENDING);

            InsertTailList(&thisDev->SendQueue,
                           (PLIST_ENTRY)PacketArray[i]->MiniportReserved);
        }
        NdisReleaseSpinLock(&thisDev->QueueLock);

        if (!TxWasActive)
        {
        //
        // Complete the Receive DMA before starting the next
        // set of transmits.
        //
            NdisMCompleteDmaTransfer(&stat, thisDev->DmaHandle,
                                     thisDev->rcvDmaBuffer,
                                     (ULONG)thisDev->rcvDmaOffset,
                                     (ULONG)thisDev->rcvDmaSize, FALSE);

            if (!RegStats.RxWindow) {
                LOG("SendPacketsHandler - RxWindow has zero value", 0);
                DBGOUT(("SendPacketsHandler - RxWindow has zero value"));
            }

            if (RegStats.RxDPC_Window > 1) {
                RegStats.RxDPC_G1_Count++;
            }

            RegStats.RxWindowMax = MAX(RegStats.RxWindowMax, RegStats.RxWindow);
            RegStats.RxWindow = 0;
            RegStats.RxDPC_WindowMax = MAX(RegStats.RxDPC_WindowMax, RegStats.RxDPC_Window);
            RegStats.RxDPC_Window = 0;

            //
            // Use DMA swap bit to switch to DMA to Transmit.
            //
            NSC_WriteBankReg(thisDev->portInfo.ioBase, 2, 2, 0x0B);

            //
            // Switch on the DMA interrupt to decide when
            // transmission is complete.
            //
            thisDev->IntMask = 0x14;
            SetCOMInterrupts(thisDev, TRUE);
            //
            // Kick off the first transmit.
            //
            FIR_MegaSend(thisDev);
        }
    }
    else {
        /*
         *  This is a great opportunity to be lazy.
         *  Just call MiniportSend with each packet in sequence and
         *  set the result in the packet array object.
         */
        for (i = 0; i < NumberofPackets; i++) {
            stat = MiniportSend(MiniportAdapterContext,
                                PacketArray[i], 0);
            NDIS_SET_PACKET_STATUS(PacketArray[i], stat);
        }
    }
    LOG("<== SendPacketsHandler", 1);
    DBGOUT(("<== SendPacketsHandler"));
}



/*
 *************************************************************************
 *  AllocateCompleteHandler
 *************************************************************************
 *
 *  Indicate completion of an NdisMAllocateSharedMemoryAsync call.
 *  We never call that function, so we should never get entered here.
 *
 */
VOID AllocateCompleteHandler(   NDIS_HANDLE MiniportAdapterContext,
                                PVOID VirtualAddress,
                                PNDIS_PHYSICAL_ADDRESS  PhysicalAddress,
                                ULONG Length,
                                PVOID Context)
{
    DBGERR(("AllocateCompleteHandler - should not get called"));
}




/*
 *************************************************************************
 *  PortReadyForWrite
 *************************************************************************
 *
 *  Called when COM port is ready for another write packet.
 *  Send the first frame in the send queue.
 *
 *  Return TRUE iff send succeeded.
 *
 *  NOTE: Do not call inside of interrupt context.
 *
 */
NDIS_STATUS PortReadyForWrite(IrDevice *thisDev, BOOLEAN firstBufIsPending)
{
    NDIS_STATUS Result = NDIS_STATUS_FAILURE;
    BOOLEAN sendSucceeded;
    PLIST_ENTRY ListEntry;

    UNREFERENCED_PARAMETER(firstBufIsPending);

    DBGOUT(   (   "PortReadyForWrite(dev=0x%x, %xh, %s)",
                  thisDev,
                  thisDev->portInfo.ioBase,
                  (CHAR *)(firstBufIsPending ? "pend" : "not pend")));

    ListEntry = MyInterlockedRemoveHeadList(&thisDev->SendQueue,
                                            &thisDev->QueueLock);

    if (ListEntry){

        PNDIS_PACKET packetToSend = CONTAINING_RECORD(ListEntry,
                                                      NDIS_PACKET,
                                                      MiniportReserved);
        PNDIS_IRDA_PACKET_INFO packetInfo;


        thisDev->portInfo.writePending = TRUE;
        /*
         *  Enforce the minimum turnaround time that must transpire
         *  after the last receive.
         */
        packetInfo = GetPacketInfo(packetToSend);

        if (packetInfo->MinTurnAroundTime){

            UINT usecToWait = packetInfo->MinTurnAroundTime;
            UINT msecToWait;
            packetInfo->MinTurnAroundTime = 0;

            NdisInterlockedInsertHeadList(&thisDev->SendQueue,
                                          (PLIST_ENTRY)packetToSend->MiniportReserved,
                                          &thisDev->QueueLock);
            // Ndis timer has a 1ms granularity (in theory).  Let's round off.

            msecToWait = (usecToWait<1000) ? 1 : (usecToWait+500)/1000;
            NdisSetTimer(&thisDev->TurnaroundTimer, msecToWait);
            return NDIS_STATUS_PENDING; // Say we're successful.  We'll come back here.
        }

        /*
         * See if this was the last packet before we need to change
         * speed.
         */
        if (packetToSend == thisDev->lastPacketAtOldSpeed){
            thisDev->lastPacketAtOldSpeed = NULL;
            thisDev->setSpeedAfterCurrentSendPacket = TRUE;
        }

        /*
         *  Send one packet to the COMM port.
         */
        DBGPKT(("Sending packet 0x%x (0x%x).", thisDev->packetsSent++, packetToSend));
        sendSucceeded = DoSend(thisDev, packetToSend);

        /*
         *  If the buffer we just sent was pending
         *  (i.e. we returned NDIS_STATUS_PENDING for it in MiniportSend),
         *  then hand the sent packet back to the protocol.
         *  Otherwise, we're just delivering it synchronously from MiniportSend.
         */

        // We're deserialized, so call NdisMSendComplete in any case.

        Result = sendSucceeded ? NDIS_STATUS_SUCCESS : NDIS_STATUS_FAILURE;

        DBGOUT(("Calling NdisMSendComplete"));
        NdisMSendComplete(thisDev->ndisAdapterHandle, packetToSend, Result );

        if (Result)
        {
            DBGERR(("NSC: SendPacket status %x\n", Result));
        }

    }


    DBGOUT(("PortReadyForWrite done."));

    return Result;
}

BOOLEAN AbortLoad = FALSE;
/*
 *************************************************************************
 *  DriverEntry
 *************************************************************************
 *
 *  Only include if IRMINI is a stand-alone driver.
 *
 */
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
#pragma NDIS_INIT_FUNCTION(DriverEntry)
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS result = STATUS_SUCCESS, stat;
    NDIS_HANDLE wrapperHandle;
    NDIS50_MINIPORT_CHARACTERISTICS info;

    LOG("==> DriverEntry", 0);
    DBGOUT(("==> DriverEntry"));

    //DbgBreakPoint();
    if (AbortLoad)
    {
        return STATUS_CANCELLED;
    }

    NdisZeroMemory(&info, sizeof(info));

    NdisMInitializeWrapper( (PNDIS_HANDLE)&wrapperHandle,
                            DriverObject,
                            RegistryPath,
                            NULL
                          );
    DBGOUT(("Wrapper handle is %xh", wrapperHandle));

    info.MajorNdisVersion           =   (UCHAR)NDIS_MAJOR_VERSION;
    info.MinorNdisVersion           =   (UCHAR)NDIS_MINOR_VERSION;
    //info.Flags						=	0;
    info.CheckForHangHandler        =   MiniportCheckForHang;
    info.HaltHandler                =   MiniportHalt;
    info.InitializeHandler          =   MiniportInitialize;
    info.QueryInformationHandler    =   MiniportQueryInformation;
    info.ReconfigureHandler         =   MiniportReconfigure;
    info.ResetHandler               =   MiniportReset;
    info.SendHandler                =   MiniportSend;
    info.SetInformationHandler          =       MiniportSetInformation;
    info.TransferDataHandler        =   MiniportTransferData;

    info.HandleInterruptHandler     =   MiniportHandleInterrupt;
    info.ISRHandler                 =   MiniportISR;
    info.DisableInterruptHandler    =   MiniportDisableInterrupt;
    info.EnableInterruptHandler     =   MiniportEnableInterrupt;


    /*
     *  New NDIS 4.0 fields
     */
    info.ReturnPacketHandler        =   ReturnPacketHandler;
    info.SendPacketsHandler         =   SendPacketsHandler;
    info.AllocateCompleteHandler    =   AllocateCompleteHandler;


    stat = NdisMRegisterMiniport(   wrapperHandle,
                                    (PNDIS_MINIPORT_CHARACTERISTICS)&info,
                                    sizeof(info));
    if (stat != NDIS_STATUS_SUCCESS){
        DBGERR(("NdisMRegisterMiniport failed in DriverEntry"));
        result = STATUS_UNSUCCESSFUL;
        goto _entryDone;
    }

    _entryDone:
    DBGOUT(("DriverEntry %s", (PUCHAR)((result == NDIS_STATUS_SUCCESS) ? "succeeded" : "failed")));

    LOG("<== DriverEntry", 1);
    DBGOUT(("<== DriverEntry"));
    return result;
}

PNDIS_IRDA_PACKET_INFO GetPacketInfo(PNDIS_PACKET packet)
{
    MEDIA_SPECIFIC_INFORMATION *mediaInfo;
    UINT size;
    NDIS_GET_PACKET_MEDIA_SPECIFIC_INFO(packet, &mediaInfo, &size);
    return (PNDIS_IRDA_PACKET_INFO)mediaInfo->ClassInformation;
}

/* Setup for Recv */
// This function is always called at MIR & FIR speeds
void SetupRecv(IrDevice *thisDev)
{
    NDIS_STATUS stat;
    UINT FifoClear = 8;

    LOG("SetupRecv - Begin Rcv Setup", 0);
    NdisAcquireSpinLock(&thisDev->QueueLock);
    thisDev->AdapterState = ADAPTER_RX;
    NdisReleaseSpinLock(&thisDev->QueueLock);

    FindLargestSpace(thisDev, &thisDev->rcvDmaOffset, &thisDev->rcvDmaSize);

    // Drain the status fifo of any pending packets
    //
    while ((NSC_ReadBankReg(thisDev->portInfo.ioBase, 5, 5)&0x80) && FifoClear--)
    {
        ULONG Size = NSC_ReadBankReg(thisDev->portInfo.ioBase, 5, 6);
        Size |= NSC_ReadBankReg(thisDev->portInfo.ioBase, 5, 7);
        LOG_Discard(thisDev, Size);
        thisDev->DiscardNextPacketSet = TRUE;
    }

    thisDev->rcvPktOffset = thisDev->rcvDmaOffset;

    //
    // Use DMA swap bit to switch to DMA to Receive.
    //
    NSC_WriteBankReg(thisDev->portInfo.ioBase,
                     2, 2, 0x03);
    LOG_Dma(thisDev);
    NdisMSetupDmaTransfer(&stat, thisDev->DmaHandle,
                          thisDev->rcvDmaBuffer,
                          (ULONG)thisDev->rcvDmaOffset,
                          (ULONG)thisDev->rcvDmaSize, FALSE);

    if (stat != NDIS_STATUS_SUCCESS) {
        LOG("Error: NdisMSetupDmaTransfer failed in SetupRecv", stat);
        DBGERR(("NdisMSetupDmaTransfer failed (%xh) in SetupRecv", (UINT)stat));
        ASSERT(0);
    }
    LOG("SetupRecv - End Rcv Setup", 0);
}

VOID DelayedWrite(IN PVOID SystemSpecific1,
                  IN PVOID FunctionContext,
                  IN PVOID SystemSpecific2,
                  IN PVOID SystemSpecific3)
{
    IrDevice *thisDev = FunctionContext;

    /* FIR mode */
    if (thisDev->currentSpeed > MAX_SIR_SPEED) {
        /* Kick off another Tx. */
        FIR_MegaSend(thisDev);
    }

    /* SIR mode */
    else {
        PortReadyForWrite(thisDev, TRUE);
    }
}


// NdisInterlockedRemoveHeadList may be misbehaved in Win98.  We provided
// our own.
PLIST_ENTRY
MyInterlockedRemoveHeadList(
	IN	PLIST_ENTRY				ListHead,
	IN	PNDIS_SPIN_LOCK			SpinLock
	)
{
    PLIST_ENTRY pListEntry;

    NdisAcquireSpinLock(SpinLock);
    if (IsListEmpty(ListHead))
    {
        pListEntry = NULL;
    }
    else
    {
        pListEntry = RemoveHeadList(ListHead);
    }
    NdisReleaseSpinLock(SpinLock);
    return pListEntry;
}



