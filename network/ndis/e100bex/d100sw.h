/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    d100sw.h

This driver runs on the following hardware:
    - 82557/82558 based PCI 10/100Mb ethernet adapters
    (aka Intel EtherExpress(TM) PRO Adapters)

Environment:
    Kernel Mode - Or whatever is the equivalent on WinNT

Revision History
    - JCB 8/14/97 Example Driver Created
*****************************************************************************/


#ifndef _D100SW_
#define _D100SW_

#define D100_OFFSET(field) ( (UINT) FIELD_OFFSET(D100_ADAPTER,field) )
#define D100_SIZE(field)    sizeof( ((D100_ADAPTER *)0)->field )


#define D100_NDIS_MAJOR_VERSION        0x5
#define D100_NDIS_MINOR_VERSION        0x0

#define D100_DRIVER_VERSION ((D100_NDIS_MAJOR_VERSION*0x100) + D100_NDIS_MINOR_VERSION)

// NDIS BusType values
#define     ISABUS           1
#define     EISABUS          2
#define     PCIBUS           5


//- Driver defaults
#define     LOOKAHEAD_SIZE      222

//-------------------------------------------------------------------------
//- Macros to read and write from IO ports
//-------------------------------------------------------------------------
#define D100_READ_UCHAR(_port, _pValue) \
    NdisRawReadPortUchar( \
    (ULONG)(_port), \
    (PUCHAR)(_pValue) \
)

#define D100_WRITE_UCHAR(_port, _Value) \
    NdisRawWritePortUchar( \
    (ULONG)(_port), \
    (UCHAR) (_Value) \
)

#define D100_READ_USHORT(_port, _pValue) \
    NdisRawReadPortUshort( \
    (ULONG)(_port), \
    (PUSHORT)(_pValue) \
)

#define D100_WRITE_USHORT(_port, _Value) \
    NdisRawWritePortUshort( \
    (ULONG)(_port), \
    (USHORT) (_Value) \
)

//- Macros peculiar to NT
//- The highest physical address that can be allocated to buffers.
#define D100_ALLOC_MEM(_pbuffer, _length) NdisAllocateMemoryWithTag( \
    (PVOID*)(_pbuffer), \
    (_length), \
    '001E')


#define D100_FREE_MEM(_buffer,_length) NdisFreeMemory((_buffer), (_length), 0)

//- Size of the Ethernet Header
#define ENET_HEADER_SIZE       sizeof(ETH_HEADER_STRUC)

#define MAX_PCI_CARDS 12

//-------------------------------------------------------------------------
// PCI Cards found - returns hardware info after scanning for devices
//-------------------------------------------------------------------------
typedef struct _PCI_CARDS_FOUND_STRUC
{
    USHORT NumFound;
    struct
    {
        ULONG           BaseIo;
        UCHAR           ChipRevision;
        ULONG           SubVendor_DeviceID;
        USHORT          SlotNumber;     // Ndis Slot number
        ULONG           MemPhysAddress; // CSR Physical address
        UCHAR           Irq;
        UCHAR           NodeAddress[ETH_LENGTH_OF_ADDRESS];
    } PciSlotInfo[MAX_PCI_CARDS];

} PCI_CARDS_FOUND_STRUC, *PPCI_CARDS_FOUND_STRUC;


//-------------------------------------------------------------------------
// D100_LIST_ENTRY
//-------------------------------------------------------------------------
typedef struct _D100_LIST_ENTRY {

    LIST_ENTRY  Link;

} D100_LIST_ENTRY, *PD100_LIST_ENTRY;


//-------------------------------------------------------------------------
// NON_TRANSMIT_CB -- Generic Non-Transmit Command Block
//-------------------------------------------------------------------------
typedef struct _NON_TRANSMIT_CB
{
    union
    {
        MULTICAST_CB_STRUC  Multicast;
        CONFIG_CB_STRUC     Config;
        IA_CB_STRUC         Setup;
        DUMP_CB_STRUC       Dump;
    }   NonTxCb;

} NON_TRANSMIT_CB, *PNON_TRANSMIT_CB;

//-------------------------------------------------------------------------
// COALESCE -- This structure describes a coalesce buffer resource
//-------------------------------------------------------------------------
typedef struct _COALESCE {

    D100_LIST_ENTRY     Link;
    PVOID               OwningTcb;
    PUCHAR              CoalesceBufferPtr;
    ULONG               CoalesceBufferPhys;

} COALESCE, *PCOALESCE;


//-------------------------------------------------------------------------
// D100SwTcb -- Software Transmit Control Block.  This structure contains
// all of the variables that are related to a specific Transmit Control
// block (TCB)
//-------------------------------------------------------------------------

typedef struct _D100SwTcb {

    // Link to the next SwTcb in the list
    D100_LIST_ENTRY     Link;

    // The NDIS packet that this TCB is sending
    PNDIS_PACKET        Packet;

    // physical and virtual pointers to the hardware TCB
    PTXCB_STRUC         Tcb;
    ULONG               TcbPhys;

    // virtual pointer to the previous hardware TCB in the chain
    PTXCB_STRUC         PreviousTcb;

    // Physical and virtual pointers to the TBD array for this TCB
    PTBD_STRUC          FirstTbd;
    ULONG               FirstTbdPhys;

    // number of map registers used by this TCB
    UINT                MapsUsed;

    // number of TBDs used by this TCB
    UINT                TbdsUsed;

    // When a virtual buffer is decomposed into it's constituent
    // physical vectors, each vector is stored here.
    UINT                NumPhysDesc;
    NDIS_PHYSICAL_ADDRESS_UNIT PhysDesc[MAX_PHYS_DESC];

    // If there are more physical segments then map registers,
    // then use a coalesce buffer.
    PCOALESCE           Coalesce;
    UINT                CoalesceBufferLen;

    // Describes the length of the packet as sent by the protocol.
    UINT                PacketLength;

    // Pointer to the first buffer in the packet
    PNDIS_BUFFER        FirstBuffer;

    // The number of buffers that the packet has
    UINT                BufferCount;

#if DBG
    UINT                TcbNum;
    UINT                BufferCountCheck;
    UINT                NumPhysDescCheck;
#endif

} D100SwTcb, *PD100SwTcb;


//-------------------------------------------------------------------------
// D100SwRfd
//
//  In addition to the Receive Entry fields which the D100 defines, we need
//  some additional fields for our own purposes.  To ensure that these
//  fields are properly aligned (and to ensure that the actual Receive
//  Entry is properly aligned) we'll defined a Super Receive Entry.
//  This structure will contain a "normal" Hardware Receive Entry (RFD) plus
//  some additional fields.
//-------------------------------------------------------------------------
typedef struct _D100SwRfd {

    D100_LIST_ENTRY     Link;
#if DBG
    UINT RfdNum;
#endif

    //
    // References an active chain of buffers when
    // on the receive indication list.
    //
    volatile PRFD_STRUC Rfd;             // ptr to hardware RFD
    ULONG               RfdPhys;         // physical address of RFD
    PNDIS_BUFFER        ReceiveBuffer;   // Pointer to Buffer
    PNDIS_PACKET        ReceivePacket;   // Pointer to Packet
    USHORT              Status;          // receive status (quick retrieval)
    UINT                FrameLength;     // total size of receive frame

} D100SwRfd, *PD100SwRfd;

// supporting piece of ReceiveMemoryDescriptor
typedef struct _CMD {
    ULONG   VirtualAddress;
    ULONG   Size;
} CACHED_MEM_DESCRIPTOR, *PCACHED_MEM_DESCRIPTOR;

// supporting piece of ReceiveMemoryDescriptor
typedef struct _UCMD {
    ULONG   VirtualAddress;
    NDIS_PHYSICAL_ADDRESS PhysicalAddress;
    ULONG   Size;
} UNCACHED_MEM_DESCRIPTOR, *PUNCACHED_MEM_DESCRIPTOR;

//-------------------------------------------------------------------------
// ReceiveMemoryDescriptor
//
// In order to manage receives in an environment where our receive
// buffer space might be growing, this structure was created to have
// a set of pointers that for each growth instance would allow us
// to monitor and free that growth area if need be.
// this structure is all just memory pointers and counters.
//-------------------------------------------------------------------------
typedef struct _RMD {
    D100_LIST_ENTRY         Link;       // forward and backward links
    CACHED_MEM_DESCRIPTOR   CachedMem;
    UNCACHED_MEM_DESCRIPTOR UnCachedMem;
    UINT                    ActivePoolCount;
} ReceiveMemoryDescriptor, *PReceiveMemoryDescriptor;



//-------------------------------------------------------------------------
// This macro might be used to debug spinlock acquires and releases
// with the NdisAcquireSpinlock() and NdisReleaseSpinlock() call
// it aligns itself over 8 dwords, making reading in the debugger
// easy. Beware the NdisGetTime call sometimes returns the same
// value... when called in close proximity to each other...
// you might have a macro that replaced spinlock calls with some
// fill in of this structure and then acquire/release the spinlock
//-------------------------------------------------------------------------
#if DBG
typedef struct _D100SpinDebug
{
    // 8 dwords total (hopefully) trying to debug a spin lock problem
    USHORT      action;
    USHORT      line;
    ULONG       file;
    LARGE_INTEGER    time;
    //    LONGLONG    blank;
} D100SpinDebug, *PD100SpinDebug;
#endif

//-------------------------------------------------------------------------
// D100_ADAPTER
//
//  The main Adapter structure definition. This structure has all fields
//  relevant to the hardware.
//-------------------------------------------------------------------------
typedef struct _D100_ADAPTER
{
#if DBG
    UINT                Debug;
    USHORT              txsent;
    USHORT              txind;
    UINT    sdebugindex;

#define DBG_QUEUE_LEN      4095   //0xfff
    UINT                DbgIndex;
    UCHAR               DbgQueue[DBG_QUEUE_LEN];
    UINT IndicateReceivePacketCounter;
    UINT PacketsIndicated;
    UINT ReceiveCompleteCounter;
#endif

    // Holds the interrupt object for this adapter.
    NDIS_MINIPORT_INTERRUPT     Interrupt;

    // setup the timer structure used for Async Resets
    NDIS_MINIPORT_TIMER        D100AsyncResetTimer;

    // the pointer to the first packet we have queued in send
    // deserialized miniport support variables
    PNDIS_PACKET        FirstTxQueue;
    PNDIS_PACKET        LastTxQueue;
    UINT                NumPacketsQueued;

    // Handle given by NDIS when the Adapter registered itself.
    NDIS_HANDLE         D100AdapterHandle;

    // CSR memory address pointers
    PCSR_STRUC          CSRAddress;
    ULONG               CSRPhysicalAddress;

    // Adapter Information Variable (set via Registry entries)
    UINT                BusNumber;      //' BusNumber'
    USHORT              BusDevice;      // PCI Bus/Device #

    USHORT              AiBaseIo;       // Base I/O address
    ULONG               AiBoardId;      // last 4 bytes of node address (EID), overloaded with PCI device id after init
    UINT                AiBusType;      // 'BusType' (EISA or PCI)
    USHORT              AiTxFifo;       // TX FIFO Threshold
    USHORT              AiRxFifo;       // RX FIFO Threshold
    UINT                AiInterrupt;    // 'InterruptNumber'
    USHORT              AiTempSpeed;    // 'Speed', user over-ride of line speed
    UCHAR               AiNodeAddress[ETH_LENGTH_OF_ADDRESS];
    UCHAR               AiPermanentNodeAddress[ETH_LENGTH_OF_ADDRESS];
    USHORT              AiSlot;         // 'Slot', PCI Slot Number
    USHORT              AiThreshold;    // 'Threshold', Transmit Threshold
    UCHAR               AiUnderrunRetry; // The underrun retry mechanism
    UCHAR               AiForceDpx;     // duplex setting
    UCHAR               AiTxDmaCount;   // Tx dma count
    UCHAR               AiRxDmaCount;   // Rx dma count
    UCHAR               Congest;        // Enables congestion control
    USHORT              TxPerUnderrun;  // Num transmits per underrun
    USHORT              AiLineSpeedCur; // Current line speed
    USHORT              AiDuplexCur;    // Current duplex mode
    UINT                McTimeoutFlag;  // MC workaround flag

    // This variable should be initialized to false, and set to true
    // to prevent re-entrancy in our driver during reset spinlock and unlock
    // stuff related to checking our link status
    BOOLEAN             ResetInProgress;    

    NDIS_MEDIA_STATE    LinkIsActive;

    // Address of the phy component
    UINT                PhyAddress;
    UCHAR               Connector;      // 0=Auto, 1=TPE, 2=MII

    // Fields for various D100 specific parameters
    UINT                NumCoalesce;    // 'NumCoalese'
    UINT                NumRfd;         // 'NumRfd'
    UINT                OriginalNumRfd;
    UINT                NumTbdPerTcb;   // 'NumTbdPerTcb'
    UINT                NumTbd;         // Total number of TBDs
    UINT                RegNumTcb;      // number of transmit control blocks the registry says
    UINT                NumTcb;         // 'NumTcb' (number of tcb we are actually using (usually RegNumTcb+1))

    // Mapped Base I/O values, used for accessing Adapter I/O
    ULONG               MappedIoBase;
    UINT                MappedIoRange;

    // Map register variables
    UINT                 NumMapRegisters;
    UINT                 NextFreeMapReg;
    UINT                 OldestUsedMapReg;

    NDIS_INTERRUPT_MODE InterruptMode;

    // command unit status flags
    BOOLEAN             TransmitIdle;
    BOOLEAN             ResumeWait;

    ULONG               MaxPhysicalMappings;

    // 82557/82558 Control Structures. (note: VOLATILE memory area)
    PUCHAR              CbUnCached;
    UINT                CbUnCachedSize;
    NDIS_PHYSICAL_ADDRESS   CbUnCachedPhys;


    volatile PSELF_TEST_STRUC   SelfTest;       // 82557/82558 SelfTest
    ULONG                       SelfTestPhys;

    volatile PNON_TRANSMIT_CB   NonTxCmdBlock;  // 82557/82558 (non transmit) Command Block
    ULONG                       NonTxCmdBlockPhys;

    volatile PDUMP_AREA_STRUC   DumpSpace;       // 82557/82558 dump buffer area
    ULONG                       DumpSpacePhys;

    volatile PCB_HEADER_STRUC   NonTxCmdBlockHdr;

    // Receive memory parameters
    PUCHAR              RecvCached;
    UINT                RecvCachedSize;

    PUCHAR              RecvUnCached;
    UINT                RecvUnCachedSize;
    NDIS_PHYSICAL_ADDRESS      RecvUnCachedPhys;

    // support for async shared memory allocation
    ReceiveMemoryDescriptor ReceiveMemoryDescArray[NUM_RMD];
    UINT                Last_RMD_used;
    BOOLEAN             AsynchronousAllocationPending;

    D100_LIST_ENTRY     RfdList;

    // Transmit memory structures.
    PUCHAR              XmitCached;
    UINT                XmitCachedSize;

    PUCHAR              XmitUnCached;
    UINT                XmitUnCachedSize;
    NDIS_PHYSICAL_ADDRESS   XmitUnCachedPhys;


    // Map into a local array to avoid overflow in the Tcb if the virtual
    // buffer is made of too many physical buffers. This is a temporary
    // scratch area.
    NDIS_PHYSICAL_ADDRESS_UNIT pUnits[MAX_PHYS_DESC];


    // Keep a list of free transmit descriptors.
    D100_LIST_ENTRY     TxCBList;

    D100_LIST_ENTRY     ActiveChainList;

    D100_LIST_ENTRY     CompletedChainList;

    // Keep a coalesce buffer list around for those times when there are too many
    // physical mappings.
    D100_LIST_ENTRY     CoalesceBufferList;


    // The Current Global Packet Filter and look ahead size.
    ULONG               PacketFilter;
    UINT                LookAhead;

    volatile PERR_COUNT_STRUC   StatsCounters;
    ULONG                       StatsCounterPhys;

    // Packet counts
    UINT                GoodTransmits;
    UINT                GoodReceives;
    UINT                NumTxSinceLastAdjust;

    // Count of transmit errors
    UINT                TxAbortExcessCollisions;
    UINT                TxLateCollisions;
    UINT                TxDmaUnderrun;
    UINT                TxLostCRS;
    UINT                TxOKButDeferred;
    UINT                OneRetry;
    UINT                MoreThanOneRetry;
    UINT                TotalRetries;

    // Count of receive errors
    UINT                RcvCrcErrors;
    UINT                RcvAlignmentErrors;
    UINT                RcvResourceErrors;
    UINT                RcvDmaOverrunErrors;
    UINT                RcvCdtFrames;
    UINT                RcvRuntErrors;

    // old packet filter
    UCHAR               OldParameterField;

    // We need this for multicast address changes during resets
    UCHAR               PrivateMulticastBuffer[MAX_MULTICAST_ADDRESSES][ETH_LENGTH_OF_ADDRESS];
    UINT                NumberOfMcAddresses;

    NDIS_SPIN_LOCK      Lock;

    // Receive Routine Data Area
    NDIS_HANDLE          ReceivePacketPool;
    NDIS_HANDLE          ReceiveBufferPool;

    // some counters for our Free/Used pool
    UINT                 UsedRfdCount;

    // place to hold the revision id of the d100 chip
    UCHAR               AiRevID;
    USHORT              AiSubVendor;
    USHORT              AiSubDevice;

    // save the status of the Memory Write Invalidate bit in the PCI command word
    BOOLEAN             MWIEnable;

    // store the revision of the PHY
    UINT                PhyId;
    // store the current state of our equalizer state machine (0 or 0xf)
    BOOLEAN             Force_EQ_Zero;
    BOOLEAN             Renegotiating;

    NDIS_DEVICE_POWER_STATE CurrentPowerState;
    NDIS_DEVICE_POWER_STATE NextPowerState;

    // WMI support
    ULONG               CustomDriverSet;

} D100_ADAPTER, *PD100_ADAPTER;

//-------------------------------------------------------------------------
// Miniport and MAC specific Defines
//-------------------------------------------------------------------------
typedef struct _D100_RESERVED {

    // next packet in the chain of queued packets being allocated,
    // or waiting for the finish of transmission.
    //
    // We always keep the packet on a list so that in case the
    // the adapter is closing down or resetting, all the packets
    // can easily be located and "canceled".
    //
    PNDIS_PACKET Next;
} D100_RESERVED,*PD100_RESERVED;

#define PD100_RESERVED_FROM_PACKET(_Packet) \
    ((PD100_RESERVED)((_Packet)->MiniportReserved))

#define EnqueuePacket(_Head, _Tail, _Packet)           \
{                                                      \
    if (!_Head) {                                      \
    _Head = _Packet;                                   \
    } else {                                           \
    PD100_RESERVED_FROM_PACKET(_Tail)->Next = _Packet; \
}                                                      \
    PD100_RESERVED_FROM_PACKET(_Packet)->Next = NULL;  \
    _Tail = _Packet;                                   \
}

#define DequeuePacket(Head, Tail)                      \
{                                                      \
    PD100_RESERVED Reserved =                          \
    PD100_RESERVED_FROM_PACKET(Head);                  \
    if (!Reserved->Next) {                             \
    Tail = NULL;                                       \
    }                                                  \
    Head = Reserved->Next;                             \
}

//Given a MiniportContextHandle return the PD100_ADAPTER it represents.
#define PD100_ADAPTER_FROM_CONTEXT_HANDLE(Handle) ((PD100_ADAPTER)(Handle))


// Uniquely defines the location of the error
#define D100LogError(_Adapt, _ProcId, _ErrCode, _Spec1) \
    NdisWriteErrorLogEntry((NDIS_HANDLE)(_Adapt)->D100AdapterHandle, \
        (NDIS_ERROR_CODE)(_ErrCode), (ULONG) 4, (ULONG_PTR)(_ProcId), \
        (ULONG_PTR)(_Spec1), (ULONG_PTR)(_Adapt), (ULONG_PTR)(_Adapt->CSRAddress))


// Each entry in the error log will be tagged with a unique event code so that
// we'll be able to grep the driver source code for that specific event, and
// get an idea of why that particular event was logged.  Each time a new
// "D100LogError" statement is added to the code, a new Event tag should be
// added below.
typedef enum _D100_EVENT_VIEWER_CODES
{
        EVENT_0,                    // couldn't register the specified interrupt
        EVENT_1,                    // One of our PCI cards didn't get required resources
        EVENT_2,                    // bad node address (it was a multicast address)
        EVENT_3,                    // failed self-test
        EVENT_4,                    // Wait for SCB failed
        EVENT_5,                    // NdisRegisterAdapter failed for the MAC driver
        EVENT_6,                    // WaitSCB failed
        EVENT_7,                    // Command complete status was never posted to the SCB
        EVENT_8,                    // Couldn't find a phy at over-ride address 0
        EVENT_9,                    // Invalid duplex or speed setting with the detected phy
        EVENT_10,                   // Couldn't memory map the CSR.
        EVENT_11,                   // couldn't allocate enough map registers
        EVENT_12,                   // couldn't allocate enough recv cached memory
        EVENT_13,                   // couldn't allocate enough recv uncached shared memory
        EVENT_14,                   // couldn't allocate enough xmit cached memory
        EVENT_15,                   // couldn't allocate enough cb uncached shared memory
        EVENT_16,                   // Didn't find any PCI boards
        EVENT_17,          // 11    // Multiple PCI were found, but none matched our id.
        EVENT_18,          // 12    // NdisMPciAssignResources Error
        EVENT_19,          // 13    // Didn't Find Any PCI Boards that matched our subven/subdev
        EVENT_20,          // 14    // ran out of cached memory to allocate in async allocation
        EVENT_30           // 1e    // WAIT_TRUE timed out
} D100_EVENT_VIEWER_CODES;



//================================================
// Global Variables shared by all driver instances
//================================================

// arrange this any way you want, but it can't add up to more
// than regparam NUMRFD + MAX_RECEIVE_DESCRIPTORS,
// total max is 1024 OR 0X400 right now
static const UINT packet_count[NUM_RMD] =
    {0x10,0x20,0x30,0x40,0x50,0x20,0x20,0x20,0x20,0x20};

// This constant is used for places where NdisAllocateMemory needs to be
// called and the HighestAcceptableAddress does not matter.
static const NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
    NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);

#endif /* _D100SW_ */

