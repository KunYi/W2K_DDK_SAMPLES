/*
 ************************************************************************
 *
 *	NSC.h
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



#ifndef NSC_H
    #define NSC_H

    #include <ndis.h>
    #include <ntddndis.h>  // defines OID's

    #include "settings.h"
    #include "comm.h"
    #include "sync.h"

    #define pnpid_str "*pnp0511,*pnp0510,*pnp0501,*pnp0500"

    #define NSC_MAJOR_VERSION 1
    #define NSC_MINOR_VERSION 11
    #define NSC_LETTER_VERSION 's'
    #define NDIS_MAJOR_VERSION 5
    #define NDIS_MINOR_VERSION 0

//
// Registry Keywords.
//
#define CARDTYPE	    NDIS_STRING_CONST("BoardType")
#define CONFIGIOADDRESS NDIS_STRING_CONST("ConfigIoBaseAddress")
#define UARTIOADDRESS	NDIS_STRING_CONST("IoBaseAddress")
#define INTERRUPT	    NDIS_STRING_CONST("InterruptNumber")
#define DMACHANNEL	    NDIS_STRING_CONST("DmaChannel")
#define DONGLE_A_TYPE	NDIS_STRING_CONST("Dongle_A_Type")
#define DONGLE_B_TYPE	NDIS_STRING_CONST("Dongle_B_Type")
#define MAXCONNECTRATE  NDIS_STRING_CONST("MaxConnectRate")


//
// Valid value ranges for the DMA Channels.
//
    #define VALID_DMACHANNELS {0xFF,0x0,0x1,0x3}

    #define FIR_INT_MASK 0x14
//#define FIR_INT_MASK 0x50

enum NSC_EXT_INTS {
    RXHDL_EV    = (1 << 0),
    TXLDL_EV    = (1 << 1),
    LS_EV       = (1 << 2),
    MS_EV       = (1 << 3),
    DMA_EV      = (1 << 4),
    TXEMP_EV    = (1 << 5),
    SFIF_EV     = (1 << 6),
    TMR_EV      = (1 << 7)
};


typedef struct DebugCounters {
    ULONG TxPacketsStarted;
    ULONG TxPacketsCompleted;
    ULONG ReceivedPackets;
    ULONG WindowSize;
    ULONG StatusFIFOOverflows;
    ULONG TxUnderruns;
    ULONG ReceiveFIFOOverflows;
    ULONG MissedPackets;
    ULONG ReceiveCRCErrors;
    ULONG ReturnPacketHandlerCalled;
    ULONG RxWindow;
    ULONG RxWindowMax;
    ULONG RxDPC_Window;
    ULONG RxDPC_WindowMax;
    ULONG RxDPC_G1_Count;
} DebugCounters;

/*
 *  A receive buffer is either FREE (not holding anything) FULL
 * (holding undelivered data) or PENDING (holding data delivered
 * asynchronously)
 */
typedef enum rcvbufferStates {
    STATE_FREE,
    STATE_FULL,
    STATE_PENDING
} rcvBufferState;

typedef enum {
    ADAPTER_NONE=0,
    ADAPTER_TX,
    ADAPTER_RX
} adapterState;


typedef struct {
    LIST_ENTRY listEntry;
    rcvBufferState state;
    PNDIS_PACKET packet;
    UINT dataLen;
    PUCHAR dataBuf;
    BOOLEAN isDmaBuf;
} rcvBuffer;

typedef struct {
    UINT Length;            // Length of buffer.
    UCHAR NotUsed;          // Spare byte, not filled in.
    UCHAR StsCmd;           // For the sts cmd info.
    ULONG physAddress;      // Physical address of buffer
} DescTableEntry;


typedef struct IrDevice {
    /*
     * This is the handle that the NDIS wrapper associates with a
     * connection.  The handle that the miniport driver associates with
     * the connection is just an index into the devStates array).
     */
    NDIS_HANDLE ndisAdapterHandle;

    int CardType;

    /*
     *  Current speed setting, in bits/sec.
     *  (Note: this is updated when we ACTUALLY change the speed,
     *         not when we get the request to change speed via
     *         MiniportSetInformation).
     */
    UINT currentSpeed;

    // Current dongle setting, 0 for dongle A, 1 for dongle B
    // and so on.
    //
    UCHAR DonglesSupported;
    UCHAR currentDongle;
    UCHAR DongleTypes[2];

    UINT AllowedSpeedMask;
    /*
     *  This structure holds information about our ISR.
     *  It is used to synchronize with the ISR.
     */
    NDIS_MINIPORT_INTERRUPT interruptObj;

    //
    // Interrupt Mask.
    //
    UCHAR IntMask;

    /*
     *  Memory-mapped port range
     */
    UCHAR mappedPortRange[8];

    /*
     *  Circular queue of pending receive buffers
     */
#define NUM_RCV_BUFS 16
//    #define NEXT_RCV_BUF_INDEX(i) (((i)==NO_BUF_INDEX) ? 0 : (((i)+1)%NUM_RCV_BUFS))
    LIST_ENTRY rcvBufBuf;       // Protected by SyncWithInterrupt
    LIST_ENTRY rcvBufFree;      // Protected by SyncWithInterrupt
    LIST_ENTRY rcvBufFull;      // Protected by SyncWithInterrupt
    LIST_ENTRY rcvBufPend;      // Protected by QueueLock
    ULONG_PTR LastReadDMACount;


    NDIS_SPIN_LOCK QueueLock;
    LIST_ENTRY SendQueue;

    /* Define a buffer of packet lengths that are Tx'ed. Assuming max of 8 */

    DescTableEntry *DescTableArray;
    PNDIS_PHYSICAL_ADDRESS DescTablePhyAddr;

    //
    // Physical Address used by SG_DMA for start of contiguous buffer
    // for Tx and Rx
    //
    PUCHAR SGDMA_Buff;
    PNDIS_PHYSICAL_ADDRESS SGDMA_BuffPhyAddr;

    /*
     *  Handle to NDIS packet pool, from which packets are
     *  allocated.
     */
    NDIS_HANDLE packetPoolHandle;
    NDIS_HANDLE bufferPoolHandle;


    /*
     * mediaBusy is set TRUE any time that this miniport driver moves a
     * data frame.  It can be reset by the protocol via
     * MiniportSetInformation and later checked via
     * MiniportQueryInformation to detect interleaving activity.
     */
    BOOLEAN mediaBusy;
    BOOLEAN haveIndicatedMediaBusy;

    /*
     * nowReceiving is set while we are receiving a frame.
     * It (not mediaBusy) is returned to the protocol when the protocol
     * queries OID_MEDIA_BUSY
     */
    BOOLEAN nowReceiving;
    adapterState AdapterState;

    UCHAR LineStatus;
    UCHAR InterruptMask;
    UCHAR InterruptStatus;
    UCHAR AuxStatus;

    /*
     *  Current link speed information.
     */
    baudRateInfo *linkSpeedInfo;

    /*
     *  When speed is changed, we have to clear the send queue before
     *  setting the new speed on the hardware.
     *  These vars let us remember to do it.
     */
    PNDIS_PACKET lastPacketAtOldSpeed;
    BOOLEAN setSpeedAfterCurrentSendPacket;
    BOOLEAN setSpeedNow;

    /*
     *  Information on the COM port and send/receive FSM's.
     */
    comPortInfo portInfo;

    UINT sgIO_Base;

    /*
     *  HW resources may be temporarily released via an OID from
     *  the protocol.
     */
    BOOLEAN resourcesReleased;

    UINT hardwareStatus;

    /*
     *  UIR Module ID.
     */
    int UIR_Mid;

    BOOLEAN intEnabled;
    UCHAR intMask;

    /*
     *  Maintain statistical debug info.
     */
    UINT packetsRcvd;
    UINT packetsDropped;
    UINT packetsSent;
    UINT interruptCount;


    /*
     *  DMA handles
     */
    NDIS_HANDLE DmaHandle;
    NDIS_HANDLE dmaBufferPoolHandle;
    PNDIS_BUFFER xmitDmaBuffer, rcvDmaBuffer;
    ULONG_PTR rcvDmaOffset;
    ULONG_PTR rcvDmaSize;
    ULONG_PTR rcvPktOffset;

    UINT nextFrameStat;
    UINT nextFrameSize;

    NDIS_TIMER TurnaroundTimer;

    ULONG HangChk;

    BOOLEAN DiscardNextPacketSet;
    /*
     *  Pointer to next device in global list.
     */
    struct IrDevice *next;

} IrDevice;

#define HEAD_SEND_PACKET(dev)                                   \
    (PNDIS_PACKET) (IsListEmpty(&(dev)->SendQueue) ? NULL :     \
        CONTAINING_RECORD((dev)->SendQueue.Flink,               \
                          NDIS_PACKET,                          \
                          MiniportReserved))

/*
 *  We use a pointer to the IrDevice structure as the miniport's device context.
 */
    #define CONTEXT_TO_DEV(__deviceContext) ((IrDevice *)(__deviceContext))
    #define DEV_TO_CONTEXT(__irdev) ((NDIS_HANDLE)(__irdev))

    #define ON  TRUE
    #define OFF FALSE

    #include "externs.h"


    /*
     *  This is a structure from configmg.h .
     */
    #define  MAX_MEM_REGISTERS		9
    #define  MAX_IO_PORTS			20
    #define  MAX_IRQS				7
    #define  MAX_DMA_CHANNELS		7
    #pragma pack(push,1)

typedef struct Config_Buff_s_align {
    USHORT  wNumMemWindows;         // Num memory windows
    USHORT  wReserved[1];
    UINT    dMemBase[MAX_MEM_REGISTERS];  // Memory window base
    UINT    dMemLength[MAX_MEM_REGISTERS];   // Memory window length
    USHORT  wMemAttrib[MAX_MEM_REGISTERS];   // Memory window Attrib
    USHORT  wNumIOPorts;         // Num IO ports
    USHORT  wIOPortBase[MAX_IO_PORTS]; // I/O port base
    USHORT  wIOPortLength[MAX_IO_PORTS];  // I/O port length
    USHORT  wNumIRQs;         // Num IRQ info
    UCHAR   bIRQRegisters[MAX_IRQS];   // IRQ list
    UCHAR   bIRQAttrib[MAX_IRQS];      // IRQ Attrib list
    USHORT  wNumDMAs;         // Num DMA channels
    UCHAR   bDMALst[MAX_DMA_CHANNELS]; // DMA list
    UCHAR   bReserved1[1];
    USHORT  wDMAAttrib[MAX_DMA_CHANNELS]; // DMA Attrib list
} CMCONFIG_A;
    #pragma pack(pop)
#endif NSC_H

