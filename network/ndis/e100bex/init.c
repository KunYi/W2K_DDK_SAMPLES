/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    init.c

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
#pragma warning (disable: 4514)

//-----------------------------------------------------------------------------
// Procedure:   ClaimAdapter
//
// Description: Locates a D100 based adapter and assigns (claims) the
//              adapter hardware.  This routine also stores the slot, base IO
//              Address, base physical memory address of the CSR, and the
//              permanent node address of the claimed adapter.
//
// Arguments:
//    Adapter - ptr to Adapter object instance.
//
// Returns:
//    NDIS_STATUS_SUCCESS - If an adapter is successfully found and claimed
//    NDIS_STATUS_FAILURE- If an adapter is not found/claimed
//-----------------------------------------------------------------------------

NDIS_STATUS
ClaimAdapter(
             IN OUT PD100_ADAPTER Adapter
             )

{
    USHORT              NumPciBoardsFound;
    ULONG               Bus;
    UINT                i,j;
    NDIS_STATUS         Status = NDIS_STATUS_SUCCESS;

    USHORT              VendorID = D100_VENDOR_ID;
    USHORT              DeviceID = D100_DEVICE_ID;
    PCI_CARDS_FOUND_STRUC PciCardsFound;

    PNDIS_RESOURCE_LIST AssignedResources;
    DEBUGFUNC("ClaimAdapter");

    INITSTR(("\n"));

    Bus = (ULONG) Adapter->BusNumber;

    if (Adapter->AiBusType == PCIBUS)
    {

        // this is the new case, where the os is fully plug
        // and play
        // since both NT5 and Win9X all support configuration manager managed
        // device installs, find an adapter without
        // scanning the bus at all (just depend on the CM to give us resources)

        DEBUGSTR(("Finding Adapter with non-invasive check\n"));
        NumPciBoardsFound = FindPciDevice50Scan(Adapter,
                                VendorID,
                                DeviceID,
                                &PciCardsFound);


        if(NumPciBoardsFound)
        {
            DEBUGSTR(("\n\n                   Found the following adapters\n"));

#if DBG
            for(i=0; i < NumPciBoardsFound; i++)
                DEBUGSTR(("slot=%x, mem_phys=%x, io=%x, irq=%x, node=%.2x %.2x %.2x %.2x %.2x %.2x\n",
                PciCardsFound.PciSlotInfo[i].SlotNumber,
                PciCardsFound.PciSlotInfo[i].MemPhysAddress,
                PciCardsFound.PciSlotInfo[i].BaseIo,
                PciCardsFound.PciSlotInfo[i].Irq,
                PciCardsFound.PciSlotInfo[i].NodeAddress[0],
                PciCardsFound.PciSlotInfo[i].NodeAddress[1],
                PciCardsFound.PciSlotInfo[i].NodeAddress[2],
                PciCardsFound.PciSlotInfo[i].NodeAddress[3],
                PciCardsFound.PciSlotInfo[i].NodeAddress[4],
                PciCardsFound.PciSlotInfo[i].NodeAddress[5]));
#endif
        }

        else
        {
            DEBUGSTR(("our PCI board was not found!!!!!!\n"));

            D100LogError(Adapter,
                EVENT_16,
                NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
                0);

            return NDIS_STATUS_FAILURE;
        }

        i = 0;   // only one adapter in the system
        //      NOTE: i == the index into PciCardsFound that we want to use.


        //------------------------------------------------
        // Store our allocated resources in Adapter struct
        //------------------------------------------------

        // Set the location information
        Adapter->AiSlot = PciCardsFound.PciSlotInfo[i].SlotNumber;

        // Save IRQ
        Adapter->AiInterrupt = PciCardsFound.PciSlotInfo[i].Irq;

        // Save our Chip Revision
        Adapter->AiRevID = PciCardsFound.PciSlotInfo[i].ChipRevision;

        // save our subvendor/subdeviceID for later reference (if needed)
        Adapter->AiSubVendor = (USHORT) PciCardsFound.PciSlotInfo[i].SubVendor_DeviceID;
        Adapter->AiSubDevice = (USHORT) (PciCardsFound.PciSlotInfo[i].SubVendor_DeviceID >> 16);

        // Store the permanent node address
        for(j=0; j < 6; j++)
            Adapter->AiPermanentNodeAddress[j] =
            PciCardsFound.PciSlotInfo[i].NodeAddress[j];

        // save the D100 CSR Physical Memory Address
        Adapter->CSRPhysicalAddress = PciCardsFound.PciSlotInfo[i].MemPhysAddress;

        // save the D100 CSR I/O base address
        Adapter->AiBaseIo = (USHORT)PciCardsFound.PciSlotInfo[i].BaseIo;

        DEBUGSTR(("Using card type sven=%4X sdev=%4X with slot=%x, mem_phys=%x, io=%x, irq=%x, rev=%d, node=%.2x %.2x %.2x %.2x %.2x %.2x\n",
            Adapter->AiSubVendor,
            Adapter->AiSubDevice,
            Adapter->AiSlot,
            Adapter->CSRPhysicalAddress,
            Adapter->AiBaseIo,
            Adapter->AiInterrupt,
            Adapter->AiRevID,
            Adapter->AiPermanentNodeAddress[0],
            Adapter->AiPermanentNodeAddress[1],
            Adapter->AiPermanentNodeAddress[2],
            Adapter->AiPermanentNodeAddress[3],
            Adapter->AiPermanentNodeAddress[4],
            Adapter->AiPermanentNodeAddress[5]));

    }  // End PCI Adapter

    else
        //Must be ISA, EISA or MicroChannel (not supported)
        return NDIS_STATUS_FAILURE;

    return NDIS_STATUS_SUCCESS;
}



//-----------------------------------------------------------------------------
// Procedure:   SetupCsrIoMapping
//
// Description: This routine is called by SetupAdapterInfo to setup the D100
//              adapter's IO mapping for its CSR registers.
//
// Arguments:
//    Adapter - ptr to Adapter object instance
//
// Returns:
//    NDIS_STATUS_SUCCESS - If the adapters CSR registers were setup
//    not NDIS_STATUS_SUCCESS - If we couldn't register the CSR's I/O range
//-----------------------------------------------------------------------------

NDIS_STATUS
SetupCsrIoMapping(
                  IN OUT PD100_ADAPTER Adapter
                  )

{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    DEBUGFUNC("SetupCsrIoMapping");
    INITSTR(("\n"));

    // the IO Space that the adapter uses
    // will have to be registered.

    // register our I/O space requirements with the OS, exit on error
    Adapter->MappedIoRange = sizeof(CSR_STRUC);

    Status = NdisMRegisterIoPortRange(
        (PVOID *) &Adapter->MappedIoBase,
        Adapter->D100AdapterHandle,
        (UINT) Adapter->AiBaseIo,
        Adapter->MappedIoRange);

    DEBUGSTR(("SetupPciRegs: io=%x, map_io=%x, size=%x, stat=%x\n",
        Adapter->AiBaseIo, Adapter->MappedIoBase, Adapter->MappedIoRange, Status));

    if (Status != NDIS_STATUS_SUCCESS)
    {
        INITSTR(("NdisMRegisterIoPortRange failed (Status = 0x%x)\n", Status));
        return Status;
    }
    return Status;
}

//-----------------------------------------------------------------------------
// Procedure:   SetupAdapterInfo
//
// Description: Sets up the various adapter fields in the specified Adapter
//              object.
// Arguments:
//    Adapter - ptr to Adapter object instance
//
// Returns:
//    NDIS_STATUS_SUCCESS - If an adapter's IO mapping was setup correctly
//    not NDIS_STATUS_SUCCESS- If an adapter's IO space could not be registered
//-----------------------------------------------------------------------------

NDIS_STATUS
SetupAdapterInfo(
                 IN OUT PD100_ADAPTER Adapter
                 )

{
    NDIS_STATUS Status;

    DEBUGFUNC("SetupAdapterInfo");

    INITSTR(("\n"));

    // Setup the I/O mapping of the CSR
    Status = SetupCsrIoMapping(Adapter);

    Adapter->InterruptMode = NdisInterruptLevelSensitive;

    return Status;

}



//-----------------------------------------------------------------------------
// Procedure:   SetupSharedAdapterMemory
//
// Description: This routine is responsible for the allocation of the
//              data structures inside the Adapter structure.
//
// Arguments:
//    Adapter - the adapter structure to allocate for.
//
// Returns:
//    NDIS_STATUS_SUCCESS - If the shared memory structures were setup
//    not NDIS_STATUS_SUCCESS- If not enough memory or map registers could be
//                             allocated
//-----------------------------------------------------------------------------

NDIS_STATUS
SetupSharedAdapterMemory(
                         IN PD100_ADAPTER Adapter
                         )

{
    PUCHAR                  MemP;
    ULONG                   CbPhys;
    NDIS_STATUS             Status;
    NDIS_PHYSICAL_ADDRESS   NdisPhysicalCsrAddress;

    DEBUGFUNC("SetupSharedAdapterMemory");

    INITSTR(("\n"));

    Adapter->NextFreeMapReg = 0;
    Adapter->OldestUsedMapReg = 0;

    Adapter->MaxPhysicalMappings = MAXIMUM_ETHERNET_PACKET_SIZE;

    // Get a virtual address for our CSR structure. So that we can access
    // the CSR's registers via memory mapping.

    NdisSetPhysicalAddressLow (NdisPhysicalCsrAddress, Adapter->CSRPhysicalAddress);
    NdisSetPhysicalAddressHigh (NdisPhysicalCsrAddress, 0);

    Status = NdisMMapIoSpace(OUT (PVOID *) &(Adapter->CSRAddress),
        Adapter->D100AdapterHandle,
        NdisPhysicalCsrAddress,
        sizeof (CSR_STRUC));

    // If we couldn't get a virtual memory address for our CSR, then error out.
    if ((Status != NDIS_STATUS_SUCCESS) || (Adapter->CSRAddress == NULL))
    {

        INITSTR(("Could not memory map the CSR phys = %x\n", Adapter->CSRPhysicalAddress));
        NdisMFreeMapRegisters(Adapter->D100AdapterHandle);
        return Status;
    }

    INITSTR(("D100 CSR phys = %x, D100 CSR virt = %x\n", Adapter->CSRPhysicalAddress,
        Adapter->CSRAddress));

    // set this up here to make sure we always have enough MAP REGISTERS
    // for all the TBDs we have allocated

    // we don't have to worry so much about using map registers now
    // that NT 3.51 came out with less restrictions on them.

    // Total number of TBDs equals number of TCBs * number of TBDs per TCB
    Adapter->NumTbd = (Adapter->NumTcb * Adapter->NumTbdPerTcb);

    Adapter->NumMapRegisters = Adapter->NumTbd;

    // NOTE -- This call MUST be made, even if you don't really want any map
    // registers, if you are going to allocate any shared memory.

    INITSTR(("Allocating %x map registers\n", Adapter->NumMapRegisters));

    Status = NdisMAllocateMapRegisters(Adapter->D100AdapterHandle,
        0,
        TRUE,
        Adapter->NumMapRegisters,
        Adapter->MaxPhysicalMappings);


    if (Status != NDIS_STATUS_SUCCESS)
    {
        D100LogError(Adapter, EVENT_11, NDIS_ERROR_CODE_OUT_OF_RESOURCES, 0);

        INITSTR(("NdisMAllocateMapRegister Failed - %x\n", Status));
        Adapter->NumMapRegisters = 0;
        return Status;
    }

    // Allocate cached memory for the SW receive frame descriptors. (RFD)
    Adapter->RecvCachedSize = (Adapter->NumRfd * sizeof(D100SwRfd));
    Status = D100_ALLOC_MEM(&Adapter->RecvCached, Adapter->RecvCachedSize);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        D100LogError(Adapter, EVENT_12, NDIS_ERROR_CODE_OUT_OF_RESOURCES, 0);

        Adapter->RecvCached = (PUCHAR) 0;
        INITSTR(("Could not allocate %d bytes for RecvCached mem\n", Adapter->RecvCachedSize));
        return Status;
    }

    INITSTR(("Allocated %08x %8d bytes for RecvCached mem\n", Adapter->RecvCached, Adapter->RecvCachedSize));

    NdisZeroMemory((PVOID) Adapter->RecvCached, Adapter->RecvCachedSize);

    // Allocate memory for the shared receive resources with enough
    // extra to paragraph align everything.
    Adapter->RecvUnCachedSize = 16 + (Adapter->NumRfd * sizeof(RFD_STRUC));

    // Allocate the shared memory for the receive data structures.
    NdisMAllocateSharedMemory(
        Adapter->D100AdapterHandle,
        Adapter->RecvUnCachedSize,
        FALSE,
        (PVOID) &Adapter->RecvUnCached,
        &Adapter->RecvUnCachedPhys
        );

    if (!Adapter->RecvUnCached)
    {
        D100LogError(Adapter, EVENT_13, NDIS_ERROR_CODE_OUT_OF_RESOURCES, 0);

        INITSTR(("Could not allocate %d bytes for RecvUnCached mem\n", Adapter->RecvUnCachedSize));
        return NDIS_STATUS_FAILURE;
    }

    INITSTR(("Allocated %08x %8d bytes for RecvUnCached mem\n", Adapter->RecvUnCached, Adapter->RecvUnCachedSize));
    NdisZeroMemory((PVOID) Adapter->RecvUnCached, Adapter->RecvUnCachedSize);

    // Allocate cached memory for the SW transmit structures.
    Adapter->XmitCachedSize = (Adapter->NumTcb * sizeof(D100SwTcb)) +
        (Adapter->NumCoalesce * sizeof(COALESCE));

    Status = D100_ALLOC_MEM( &Adapter->XmitCached, Adapter->XmitCachedSize);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        D100LogError(Adapter, EVENT_14, NDIS_ERROR_CODE_OUT_OF_RESOURCES, 0);

        Adapter->XmitCached = (PUCHAR) 0;
        INITSTR(("Could not allocate %d bytes for XmitCached mem\n", Adapter->XmitCachedSize));
        return Status;
    }

    INITSTR(("Allocated %08x %8d bytes for XmitCached mem\n", Adapter->XmitCached, Adapter->XmitCachedSize));

    // initialize this recently allocated area to zeros
    NdisZeroMemory((PVOID) Adapter->XmitCached, Adapter->XmitCachedSize);

    // Allocate memory for the shared transmit resources with enough extra mem
    // to paragraph align everything
    Adapter->CbUnCachedSize =
        sizeof(SELF_TEST_STRUC) + 16 +
        sizeof(DUMP_AREA_STRUC) + 16 +
        sizeof(NON_TRANSMIT_CB) + 16 +
        sizeof(ERR_COUNT_STRUC) + 16 +
        MINIMUM_ETHERNET_PACKET_SIZE +
        (Adapter->NumTcb * sizeof(TXCB_STRUC)) + 0x100 +
        (Adapter->NumTbd * sizeof(TBD_STRUC)) + 16 +
        ((Adapter->NumCoalesce + 1) * COALESCE_BUFFER_SIZE);

    // Allocate the shared memory for the command block data structures.
    NdisMAllocateSharedMemory(
        Adapter->D100AdapterHandle,
        Adapter->CbUnCachedSize,
        FALSE,
        (PVOID) &Adapter->CbUnCached,
        &Adapter->CbUnCachedPhys);

    if (!Adapter->CbUnCached)
    {
        D100LogError(Adapter, EVENT_15, NDIS_ERROR_CODE_OUT_OF_RESOURCES, 0);

        INITSTR(("Could not allocate %d bytes for XmitUnCached mem\n", Adapter->CbUnCachedSize));
        return NDIS_STATUS_FAILURE;
    }

    INITSTR(("Allocated %08x %8d bytes for XmitUnCached mem\n", Adapter->CbUnCached, Adapter->CbUnCachedSize));

    // initialize this recently allocated area to zeros
    NdisZeroMemory((PVOID) Adapter->CbUnCached, Adapter->CbUnCachedSize);

    // Initialize the 82557 Control Structures pointers
    CbPhys = NdisGetPhysicalAddressLow(Adapter->CbUnCachedPhys);

    // Physical addresses for certain structures must be aligned on
    // paragraph boundaries or other greater boundaries

#define ALIGN_CBandP \
    CbPhys += 0x0000000F; \
    CbPhys &= (~0x0000000F); \
    MemP = (Adapter->CbUnCached + \
    (CbPhys - NdisGetPhysicalAddressLow(Adapter->CbUnCachedPhys)));

#define ALIGN_TCB \
    CbPhys += 0x000000FF; \
    CbPhys &= (~0x000000FF); \
    MemP = (Adapter->CbUnCached + \
    (CbPhys - NdisGetPhysicalAddressLow(Adapter->CbUnCachedPhys)));


    // Setup SelfTest Command Block Pointers
    ALIGN_CBandP
        Adapter->SelfTest = (PSELF_TEST_STRUC) MemP;
    Adapter->SelfTestPhys = CbPhys;
    CbPhys += sizeof(SELF_TEST_STRUC);

    // Setup non-Transmit Command Block Pointers
    ALIGN_CBandP
        Adapter->NonTxCmdBlock = (PNON_TRANSMIT_CB) MemP;
    Adapter->NonTxCmdBlockHdr = (PCB_HEADER_STRUC) MemP;
    Adapter->NonTxCmdBlockPhys = CbPhys;
    CbPhys += sizeof(NON_TRANSMIT_CB);

    // Setup dump buffer area
    ALIGN_CBandP
        Adapter->DumpSpace = (PDUMP_AREA_STRUC) MemP;
    Adapter->DumpSpacePhys = CbPhys;
    CbPhys += sizeof(DUMP_AREA_STRUC);

    // Setup stats counters area
    ALIGN_CBandP
        Adapter->StatsCounters = (PERR_COUNT_STRUC) MemP;
    Adapter->StatsCounterPhys = CbPhys;
    CbPhys += sizeof(ERR_COUNT_STRUC);

    // Save Transmit Memory Structures Starting Addresses
    ALIGN_TCB
        Adapter->XmitUnCached = MemP;
    Adapter->XmitUnCachedPhys = Adapter->CbUnCachedPhys;
    NdisSetPhysicalAddressLow(Adapter->XmitUnCachedPhys, CbPhys);

    INITSTR(("SelfTest area ptr=%x\n", Adapter->SelfTest));
    INITSTR(("NonTxCmd Block ptr=%x\n", Adapter->NonTxCmdBlock));
    INITSTR(("Dump buffer ptr=%x\n", Adapter->DumpSpace));
    INITSTR(("Stats counts dump area ptr=%x\n", Adapter->StatsCounters));
    INITSTR(("First Transmit Block ptr=%x\n", Adapter->XmitUnCached));

    return NDIS_STATUS_SUCCESS;
}


//-----------------------------------------------------------------------------
// Procedure:   SelfTestHardware
//
// Description: This routine will issue PORT Self-test command to test the
//              D100.  The self-test will fail if the adapter's master-enable
//              bit is not set in the PCI Command Register, of if the adapter
//              is not seated in a PCI master-enabled slot.
//
// Arguments:
//    Adapter - ptr to Adapter object instance
//
// Returns:
//    NDIS_STATUS_SUCCESS - If the adapter passes the self-test
//    NDIS_STATUS_FAILURE- If the adapter fails the self-test
//-----------------------------------------------------------------------------

NDIS_STATUS
SelfTestHardware(
                 IN PD100_ADAPTER Adapter
                 )

{


    ULONG    SelfTestCommandCode;
    DEBUGFUNC("SelfTestHardware");

    INITSTR(("\n"));

    // Issue a software reset to the adapter
    (void) SoftwareReset(Adapter);

    // Execute The PORT Self Test Command On The 82557/82558.
    ASSERT( Adapter->SelfTestPhys != 0 );
    SelfTestCommandCode = Adapter->SelfTestPhys;

    // Setup SELF TEST Command Code in D3 - D0
    SelfTestCommandCode |= PORT_SELFTEST;

    // Initialize the self-test signature and results DWORDS
    Adapter->SelfTest->StSignature = 0;
    Adapter->SelfTest->StResults = 0xffffffff;

    // Do the port command
    Adapter->CSRAddress->Port = SelfTestCommandCode;

    // Wait 5 milliseconds for the self-test to complete
    D100StallExecution(5);

    // if The First Self Test DWORD Still Zero, We've timed out.  If the second
    // DWORD is not zero then we have an error.
    if ((Adapter->SelfTest->StSignature == 0) || (Adapter->SelfTest->StResults != 0))
    {
        D100LogError(Adapter, EVENT_3, NDIS_ERROR_CODE_ADAPTER_DISABLED, 0);

        INITSTR(("Self-Test failed. Sig =%x, Results = %x\n",
            Adapter->SelfTest->StSignature, Adapter->SelfTest->StResults));

        return NDIS_STATUS_FAILURE;
    }

    return NDIS_STATUS_SUCCESS;

}


//-----------------------------------------------------------------------------
// Procedure:   SetupTransmitQueues
//
// Description: Setup TCBs, TBDs and coalesce buffers at INIT time. This
//              routine may also be called at RESET time.
//
// Arguments:
//    Adapter - ptr to Adapter object instance
//    DebugPrint - A boolean value that will be TRUE if this routine is to
//                 write all of transmit queue debug info to the debug terminal.
//
// Returns:    (none)
//-----------------------------------------------------------------------------

VOID
SetupTransmitQueues(
                    IN PD100_ADAPTER Adapter,
                    IN BOOLEAN DebugPrint
                    )

{

    // TCB local variables
    PD100SwTcb  SwTcbPtr;           // cached TCB list logical pointers
    PTXCB_STRUC HwTcbPtr;           // uncached TCB list logical pointers
    ULONG       HwTcbPhys;          // uncached TCB list physical pointer
    UINT        TcbCount;

    // TBD local variables
    PTBD_STRUC  HwTbdPtr;           // uncached TBD list pointers
    ULONG       HwTbdPhys;          // uncached TBD list physical pointer

    UINT        CoalesceCount;
    PCOALESCE   Coalesce;
    PUCHAR      CoalesceBufVirtPtr;
    ULONG       CoalesceBufPhysPtr;

    DEBUGFUNC("SetupTransmitQueues");

    INITSTR(("\n"));

    DEBUGCHAR(Adapter,'F');
    // init the lists
    QueueInitList(&Adapter->TxCBList);
    QueueInitList(&Adapter->ActiveChainList);
    QueueInitList(&Adapter->CompletedChainList);
    QueueInitList(&Adapter->CoalesceBufferList);

    // init the command unit flags
    Adapter->TransmitIdle = TRUE;
    Adapter->ResumeWait = TRUE;

    INITSTRTX(DebugPrint, ("Free TCB list %x\n", &Adapter->TxCBList));
    INITSTRTX(DebugPrint, ("Active TCB List %x\n", &Adapter->ActiveChainList));

    // print some basic sizing debug info
    INITSTRTX(DebugPrint, ("sizeof(SwTcb)=%x\n", sizeof(D100SwTcb)));
    INITSTRTX(DebugPrint, ("sizeof(HwTcb)=%x\n", sizeof(TXCB_STRUC)));
    INITSTRTX(DebugPrint, ("Adapter->NumTcb=%d\n", Adapter->NumTcb));
    INITSTRTX(DebugPrint, ("sizeof(HwTbd)=%x\n", sizeof(TBD_STRUC)));
    INITSTRTX(DebugPrint, ("Adapter->NumTbdPerTcb=%d\n", Adapter->NumTbdPerTcb));
    INITSTRTX(DebugPrint, ("Adapter->NumTbd=%d\n", Adapter->NumTbd));

    // Setup the initial pointers to the HW and SW TCB data space
    SwTcbPtr = (PD100SwTcb) Adapter->XmitCached;
    HwTcbPtr = (PTXCB_STRUC) Adapter->XmitUnCached;
    HwTcbPhys = NdisGetPhysicalAddressLow(Adapter->XmitUnCachedPhys);

    // Setup the initial pointers to the TBD data space.
    // TBDs are located immediately following the TCBs
    HwTbdPtr = (PTBD_STRUC) (Adapter->XmitUnCached +
        (sizeof(TXCB_STRUC) * Adapter->NumTcb));
    HwTbdPhys = HwTcbPhys + (sizeof(TXCB_STRUC) * Adapter->NumTcb);

    // Go through and set up each TCB
    for (TcbCount = 0; TcbCount < Adapter->NumTcb;
    TcbCount++, SwTcbPtr++, HwTcbPtr++, HwTcbPhys += sizeof(TXCB_STRUC),
        HwTbdPtr = (PTBD_STRUC) (((ULONG_PTR) HwTbdPtr) +
        ((ULONG) (sizeof(TBD_STRUC) * Adapter->NumTbdPerTcb))),
        HwTbdPhys += (sizeof(TBD_STRUC) * Adapter->NumTbdPerTcb))
    {

#if DBG
        SwTcbPtr->TcbNum = TcbCount;
#endif

        INITSTRTX(DebugPrint, (" TcbCount=%d\n", TcbCount));
        INITSTRTX(DebugPrint, ("   SwTcbPtr=%lx\n", SwTcbPtr));
        INITSTRTX(DebugPrint, ("   HwTcbPtr=%lx\n", HwTcbPtr));
        INITSTRTX(DebugPrint, ("   HwTcbPhys=%lx\n", HwTcbPhys));
        INITSTRTX(DebugPrint, ("   FirstTbdPtr=%lx\n", HwTbdPtr));
        INITSTRTX(DebugPrint, ("   FirstTbdPhys=%lx\n", HwTbdPhys));

        // point the cached TCB to the logical address of the uncached one
        SwTcbPtr->Tcb = HwTcbPtr;

        // save the uncached TCB physical address in the cached TCB
        SwTcbPtr->TcbPhys = (ULONG)HwTcbPhys;

        // store virtual and physical pointers to the TBD array
        SwTcbPtr->FirstTbd = HwTbdPtr;
        SwTcbPtr->FirstTbdPhys = HwTbdPhys;

        // initialize the uncached TCB contents -- status is zeroed
        HwTcbPtr->TxCbHeader.CbStatus = 0;

        // This is the last in CBL
        HwTcbPtr->TxCbHeader.CbCommand = CB_EL_BIT | CB_TX_SF_BIT | CB_TRANSMIT;

        // Set the link pointer so that this TCB points to the next TCB in the
        // chain.  If this is the last TCB in the chain, then it should point
        // back to the first TCB.
        if (TcbCount == (Adapter->NumTcb -1))
            HwTcbPtr->TxCbHeader.CbLinkPointer =  NdisGetPhysicalAddressLow(Adapter->XmitUnCachedPhys);
        else
            HwTcbPtr->TxCbHeader.CbLinkPointer = HwTcbPhys + sizeof(TXCB_STRUC);

        // Set the D100's early transmit threshold
        HwTcbPtr->TxCbThreshold = (UCHAR) Adapter->AiThreshold;

        // Pointer this TCB's TBD array
        HwTcbPtr->TxCbTbdPointer = HwTbdPhys;

        // Store a pointer to the previous TCB in the SwTcb structure.  This
        // pointer will be used later when we have to clear the S-bit in the
        // previous TCB
        if (TcbCount == 0)
            SwTcbPtr->PreviousTcb = (PTXCB_STRUC) (Adapter->XmitUnCached +
            ((Adapter->NumTcb -1) * sizeof(TXCB_STRUC)));
        else
            SwTcbPtr->PreviousTcb = (HwTcbPtr -1);

        // add this TCB to the free list
        QueuePutTail(&Adapter->TxCBList, &SwTcbPtr->Link);
    }

    // Setup pointers to the first coalesce buffer.  The SW coalesce structures
    // will be located immediately after the last SwTcb, and the HW coalesce
    // structures will located after the last TBD on a 2k boundary.
    Coalesce = (PCOALESCE) SwTcbPtr;
    CoalesceBufVirtPtr = (PUCHAR) HwTbdPtr;
    CoalesceBufPhysPtr = (ULONG) HwTbdPhys;

    CoalesceBufPhysPtr += 0x000007FF;
    CoalesceBufPhysPtr &= (~0x000007FF);

    CoalesceBufVirtPtr = (PUCHAR) (((ULONG_PTR) HwTbdPtr) +
        (CoalesceBufPhysPtr - (ULONG) HwTbdPhys));

    // Go through each coalesce buffer
    for (CoalesceCount = 0; CoalesceCount < Adapter->NumCoalesce; CoalesceCount++, Coalesce++)
    {
        INITSTRTX(DebugPrint, ("Coalesce struct %d @ %08x, buffer %08x %08x\n",
            CoalesceCount, Coalesce, CoalesceBufVirtPtr, CoalesceBufPhysPtr));

        QueuePutTail(&Adapter->CoalesceBufferList, &Coalesce->Link);

        // Set the phys and virtual pointers for each coalesce buffer
        Coalesce->CoalesceBufferPtr = CoalesceBufVirtPtr;
        Coalesce->CoalesceBufferPhys = CoalesceBufPhysPtr;

        CoalesceBufPhysPtr += COALESCE_BUFFER_SIZE;
        CoalesceBufVirtPtr += COALESCE_BUFFER_SIZE;
    }


    // Initialize the Transmit queueing pointers to NULL
    Adapter->FirstTxQueue = (PNDIS_PACKET) NULL;
    Adapter->LastTxQueue = (PNDIS_PACKET) NULL;
    Adapter->NumPacketsQueued = 0;

}


//-----------------------------------------------------------------------------
// Procedure:    SetupReceiveQueues
//
// Description: Setup the Receive Frame Area (RFA) at INIT time.  This
//              includes setting up each RFD in the RFA, and linking all of the
//              RFDs together.
//              Also set up our buffers for NDIS 5 and multiple receive indications
//              through a packet array.
//
// Arguments:
//    Adapter - ptr to Adapter object instance
//
// Returns:    (none)
//-----------------------------------------------------------------------------

VOID
SetupReceiveQueues(
                   IN PD100_ADAPTER Adapter

                   )
{
    // RFD local variables
    D100SwRfd *SwRfdPtr, *SwRfdNext;  // cached RFD list logical pointers
    RFD_STRUC *HwRfdPtr, *HwRfdNext;  // uncached RFD list logical pointers
    ULONG HwRfdPhys;  // uncached RFD list physical pointer
    UINT    RfdCount;
    NDIS_STATUS AllocationStatus;
    PD100SwRfd *TempPtr;

    DEBUGFUNC("SetupReceiveQueues");

    INITSTR(("\n"));

    // save the old number of Rfds to make cleaning up easier
    Adapter->OriginalNumRfd = Adapter->NumRfd;

    /*************************************************/
    /* initialize the logical and physical RFD lists */
    /*************************************************/

    // init the lists
    QueueInitList(&Adapter->RfdList);
    SwRfdNext = (D100SwRfd *)Adapter->RecvCached;     // cached RFDs logical
    HwRfdNext = (RFD_STRUC *)Adapter->RecvUnCached;   // uncached RFDs logical

    // uncached RFD physical address
    HwRfdPhys = NdisGetPhysicalAddressLow(Adapter->RecvUnCachedPhys);

    INITSTR(("sizeof(SwRfd)=%x\n", sizeof(D100SwRfd)));
    INITSTR(("sizeof(HwRfd)=%x\n", sizeof(RFD_STRUC)));
    INITSTR(("Adapter->NumRfd=%d\n", Adapter->NumRfd));

    // Set up a pool of data for us to build our packet array out of
    // for indicating groups of packets to NDIS
    // this could be quite the memory hog, but makes management
    // of the pointers associated with Asynchronous memory allocation
    // easier
    NdisAllocatePacketPool(&AllocationStatus,
        &Adapter->ReceivePacketPool,
        MAX_RECEIVE_DESCRIPTORS,
        NUM_BYTES_PROTOCOL_RESERVED_SECTION);

    ASSERT(AllocationStatus == NDIS_STATUS_SUCCESS);

    // Set up our pool of buffer descriptors...
    // we will at most have 1 per packet, so just allocate as
    // many buffers as we have packets.
    NdisAllocateBufferPool(&AllocationStatus,
        &Adapter->ReceiveBufferPool,
        MAX_RECEIVE_DESCRIPTORS);

    // AllocateBufferPool suppossedly always returns success...
    ASSERT(AllocationStatus == NDIS_STATUS_SUCCESS);

    // Setup each RFD
    for (RfdCount = 0; RfdCount < Adapter->NumRfd; RfdCount++)
    {
        // point to the next RFD (after the first time through)
        SwRfdPtr = SwRfdNext;
        HwRfdPtr = HwRfdNext;

        INITSTR((" RfdCount=%d\n", RfdCount));
        INITSTR(("   SwRfdPtr=%lx\n", SwRfdPtr));
        INITSTR(("   HwRfdPtr=%lx\n", HwRfdPtr));
        INITSTR(("   HwRfdPhys=%lx\n", HwRfdPhys));
#if DBG
        SwRfdPtr->RfdNum = RfdCount;
#endif
        // point the logical RFD to the pointer of the physical one
        SwRfdPtr->Rfd = HwRfdPtr;

        // store the physical address in the Software RFD Structure
        SwRfdPtr->RfdPhys = HwRfdPhys;

        // point to the next RFD (add sizeof RFD struc)
        // The compiler takes care of translating +1 into a +sizeof(RFD_STRUC)
        SwRfdNext = SwRfdPtr + 1;
        HwRfdNext = HwRfdPtr + 1;
        HwRfdPhys += sizeof(RFD_STRUC);

        // initialize the physical RFD contents
        if (RfdCount < (Adapter->NumRfd - 1))
        {
            // if not the last RFD...
            HwRfdPtr->RfdCbHeader.CbCommand =0;
            HwRfdPtr->RfdCbHeader.CbLinkPointer = HwRfdPhys;
        }
        else
        {
            // if this is the last RFD...
            HwRfdPtr->RfdCbHeader.CbCommand = RFD_EL_BIT;
            HwRfdPtr->RfdCbHeader.CbLinkPointer = DRIVER_NULL;
        }

        // Init each RFD header
        HwRfdPtr->RfdCbHeader.CbStatus = 0;
        HwRfdPtr->RfdRbdPointer = DRIVER_NULL;
        HwRfdPtr->RfdActualCount= 0;
        HwRfdPtr->RfdSize = sizeof(ETH_RX_BUFFER_STRUC);

        // set up the packet structure for passing up this Rfd
        // with NdisMIndicateReceivePacket

        NdisAllocatePacket(&AllocationStatus,
            &SwRfdPtr->ReceivePacket,
            Adapter->ReceivePacketPool);

        // probably should do some error handling here
        ASSERT(AllocationStatus == NDIS_STATUS_SUCCESS);

        NDIS_SET_PACKET_HEADER_SIZE(SwRfdPtr->ReceivePacket,
            ETHERNET_HEADER_SIZE);

        // point our buffer for receives at this Rfd
        NdisAllocateBuffer(&AllocationStatus,
            &SwRfdPtr->ReceiveBuffer,
            Adapter->ReceiveBufferPool,
            (PVOID)&HwRfdPtr->RfdBuffer.RxMacHeader,
            MAXIMUM_ETHERNET_PACKET_SIZE);

        // probably should do some error handling here
        ASSERT(AllocationStatus == NDIS_STATUS_SUCCESS);


        NdisChainBufferAtFront(SwRfdPtr->ReceivePacket,
            SwRfdPtr->ReceiveBuffer);

        // set up the reverse path from Packet to SwRfd
        // this is so when D100GetReturnedPackets is called we
        // can find our software superstructure that owns this receive area.
        TempPtr = (PD100SwRfd *) &SwRfdPtr->ReceivePacket->MiniportReserved;
        *TempPtr = SwRfdPtr;

        // Add this RFD to the free list
        QueuePutTail(&Adapter->RfdList, &SwRfdPtr->Link);

    }

}


//-----------------------------------------------------------------------------
// Procedure:   InitializeAdapter
//
// Description: This routine performs a reset on the adapter, and configures
//              the adapter.  This includes configuring the 82557 LAN
//              controller, validating and setting the node address, detecting
//              and configuring the Phy chip on the adapter, and initializing
//              all of the on chip counters.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      TRUE - If the adapter was initialized
//      FALSE - If the adapter failed initialization
//-----------------------------------------------------------------------------

BOOLEAN
InitializeAdapter(
                  IN PD100_ADAPTER Adapter
                  )

{
    USHORT  EepromFlags;

    DEBUGFUNC("InitializeAdapter");

    INITSTR(("\n"));

    // Check if there was a node address over-ride.  If there isn't then we'll
    // use our adapter's permanent node address as our "current" node address.
    // If a node address over-ride is present in the registry, then we'll use
    // that override address as our node address instead of the permanent
    // address in the adapter's EEPROM
    if ((Adapter->AiNodeAddress[0]==0) &&
        (Adapter->AiNodeAddress[1]==0) &&
        (Adapter->AiNodeAddress[2]==0) &&
        (Adapter->AiNodeAddress[3]==0) &&
        (Adapter->AiNodeAddress[4]==0) &&
        (Adapter->AiNodeAddress[5]==0))
    {
        // No node address override so use the permanent address
        INITSTR(("InitializeAdapter: No node address over-ride, using permanent address\n"));
        NdisMoveMemory(Adapter->AiNodeAddress,
            Adapter->AiPermanentNodeAddress,
            ETH_LENGTH_OF_ADDRESS);
    }

    INITSTR(("InitializeAdapter: Node Address is  %.2x %.2x %.2x %.2x %.2x %.2x\n",
        Adapter->AiNodeAddress[0],
        Adapter->AiNodeAddress[1],
        Adapter->AiNodeAddress[2],
        Adapter->AiNodeAddress[3],
        Adapter->AiNodeAddress[4],
        Adapter->AiNodeAddress[5]
        ));

    // Validate our current node address (make sure its not a mulicast)
    if ((UCHAR) Adapter->AiNodeAddress[0] & 1)
    {
        INITSTR(("InitializeAdapter: Node address invalid -- its a MC address\n"));

        D100LogError(Adapter, EVENT_2, NDIS_ERROR_CODE_NETWORK_ADDRESS, 0);

        return (FALSE);
    }

    // Detect the serial component, and set up the Phy if necessary
    if (!PhyDetect(Adapter))
        return (FALSE);

    // Set the McTimeoutFlag variable.  This flag will determine whether the
    // driver issues a periodic multicast command or not.  Issuing a periodic
    // multicast command will reset the 82557's internal receive state machine,
    // and bring the 82557 out of a "Receive-lockup" state. This is a documented
    // errata in the 82557 software developement manual.

    // If the end user didn't over-ride the McTimeout flag, then it should be
    // enabled only for 10mb operation, and only if the EEPROM indicates that
    // this adapter has a potential rcv-lockup problem

    if (Adapter->McTimeoutFlag == 2)
    {
        // Read the EEPROM flags register
        EepromFlags = ReadEEprom(Adapter, EEPROM_FLAGS_WORD_3, Adapter->AiBaseIo);
        if (((Adapter->AiLineSpeedCur == 10) && (!(EepromFlags & EEPROM_FLAG_10MC))) ||
            ((Adapter->AiLineSpeedCur == 100) && (!(EepromFlags & EEPROM_FLAG_100MC))))
            Adapter->McTimeoutFlag = 1;
        else
            Adapter->McTimeoutFlag = 0;
    }

    INITSTR(("MC Timeout Flag = %x\n", Adapter->McTimeoutFlag));

    // set up our link indication variable
    // it doesn't matter what this is right now because it will be
    // set correctly if link fails
    Adapter->LinkIsActive = NdisMediaStateConnected;


    Adapter->CurrentPowerState = NdisDeviceStateD0;
    Adapter->NextPowerState    = NdisDeviceStateD0;

    // Now Initialize the D100
    return (InitializeD100(Adapter));
}


//-----------------------------------------------------------------------------
// Procedure:   InitializeD100
//
// Description: This routine will perform the initial configuration on the
//              the 82557 (D100) chip.  This will include loading the CU and
//              RU base values (0 in both cases), and calling other routines
//              that will issue a configure command to the 82257, notify the
//              82557 of its node address, and clear all of the on-chip
//              counters.
//
                  // Arguments:
                  //      Adapter - ptr to Adapter object instance
//
// Returns:
//      TRUE - If 82557 chip was initialized
//      FALSE - If 82557 failed initialization
//-----------------------------------------------------------------------------

BOOLEAN
InitializeD100(
               IN PD100_ADAPTER Adapter
               )

{
    DEBUGFUNC("InitializeD100");

    INITSTR(("\n"));

    // Issue a software reset to the D100
    (void) SoftwareReset(Adapter);

    // Load the CU BASE (set to 0, because we use linear mode)
    Adapter->CSRAddress->ScbGeneralPointer = 0;
    D100IssueScbCommand(Adapter, SCB_CUC_LOAD_BASE, FALSE);

    // Wait for the SCB command word to clear before we set the general pointer
    WaitScb(Adapter);

    // Load the RU BASE (set to 0, because we use linear mode)
    Adapter->CSRAddress->ScbGeneralPointer = 0;
    D100IssueScbCommand(Adapter, SCB_RUC_LOAD_BASE, FALSE);

    // Configure the adapter
    if (!Configure(Adapter))
        return (FALSE);

    // Wait 500 milliseconds
    D100StallExecution(500);

    if (!SetupIAAddress(Adapter))
        return (FALSE);

    // Clear the internal counters
    ClearAllCounters(Adapter);

    return (TRUE);
}


//-----------------------------------------------------------------------------
// Procedure:   Configure
//
// Description: This routine will issue a configure command to the 82557.
//              This command will be executed in polled mode.  The
//              Configuration parameters that are user configurable will
//              have been set when the driver parsed the configuration
//              parameters out of the registry.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      TRUE - If the configure command was successfully issued and completed
//      FALSE - If the configure command failed to complete properly
//-----------------------------------------------------------------------------

BOOLEAN
Configure(
          IN PD100_ADAPTER Adapter
          )

{
    UINT    i;

    DEBUGFUNC("Configure");

    INITSTR(("\n"));

    // Init the packet filter to directed and multicast.
    Adapter->PacketFilter = NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST;

    // Setup the non-transmit command block header for the configure command.
    Adapter->NonTxCmdBlockHdr->CbStatus = 0;
    Adapter->NonTxCmdBlockHdr->CbCommand = CB_CONFIGURE;
    Adapter->NonTxCmdBlockHdr->CbLinkPointer = DRIVER_NULL;

    // Fill in the configure command data.

    // First fill in the static (end user can't change) config bytes
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[0] = CB_557_CFIG_DEFAULT_PARM0;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[2] = CB_557_CFIG_DEFAULT_PARM2;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[3] = CB_557_CFIG_DEFAULT_PARM3;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[6] = CB_557_CFIG_DEFAULT_PARM6;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[9] = CB_557_CFIG_DEFAULT_PARM9;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[10] = CB_557_CFIG_DEFAULT_PARM10;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[11] = CB_557_CFIG_DEFAULT_PARM11;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[12] = CB_557_CFIG_DEFAULT_PARM12;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[13] = CB_557_CFIG_DEFAULT_PARM13;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[14] = CB_557_CFIG_DEFAULT_PARM14;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[16] = CB_557_CFIG_DEFAULT_PARM16;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[17] = CB_557_CFIG_DEFAULT_PARM17;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[18] = CB_557_CFIG_DEFAULT_PARM18;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[20] = CB_557_CFIG_DEFAULT_PARM20;
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[21] = CB_557_CFIG_DEFAULT_PARM21;

    // Now fill in the rest of the configuration bytes (the bytes that contain
    // user configurable parameters).

    // Set the Tx and Rx Fifo limits
    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[1] =
        (UCHAR) ((Adapter->AiTxFifo << 4) | Adapter->AiRxFifo);

    if (Adapter->MWIEnable)
    {
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[3] |= CB_CFIG_B3_MWI_ENABLE;
    }

    // Set the Tx and Rx DMA maximum byte count fields.
    if ((Adapter->AiRxDmaCount) || (Adapter->AiTxDmaCount))
    {
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[4] =
            Adapter->AiRxDmaCount;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[5] =
            (UCHAR) (Adapter->AiTxDmaCount | CB_CFIG_DMBC_EN);
    }
    else
    {
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[4] =
            CB_557_CFIG_DEFAULT_PARM4;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[5] =
            CB_557_CFIG_DEFAULT_PARM5;
    }


    Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[7] =
        (UCHAR) ((CB_557_CFIG_DEFAULT_PARM7 & (~CB_CFIG_URUN_RETRY)) |
        (Adapter->AiUnderrunRetry << 1)
        );

    // Setup for MII or 503 operation.  The CRS+CDT bit should only be set
    // when operating in 503 mode.
    if (Adapter->PhyAddress == 32)
    {
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[8] =
            (CB_557_CFIG_DEFAULT_PARM8 & (~CB_CFIG_503_MII));
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[15] =
            (CB_557_CFIG_DEFAULT_PARM15 | CB_CFIG_CRS_OR_CDT);
    }
    else
    {
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[8] =
            (CB_557_CFIG_DEFAULT_PARM8 | CB_CFIG_503_MII);
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[15] =
            (CB_557_CFIG_DEFAULT_PARM15 & (~CB_CFIG_CRS_OR_CDT));
    }


    // Setup Full duplex stuff

    // If forced to half duplex
    if (Adapter->AiForceDpx == 1)
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[19] =
        (CB_557_CFIG_DEFAULT_PARM19 &
        (~(CB_CFIG_FORCE_FDX| CB_CFIG_FDX_ENABLE)));

    // If forced to full duplex
    else if (Adapter->AiForceDpx == 2)
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[19] =
        (CB_557_CFIG_DEFAULT_PARM19 | CB_CFIG_FORCE_FDX);

    // If auto-duplex
    else
    {
        // We must force full duplex on if we are using PHY 0, and we are
        // supposed to run in FDX mode.  We do this because the D100 has only
        // one FDX# input pin, and that pin will be connected to PHY 1.
        if ((Adapter->PhyAddress == 0) && (Adapter->AiDuplexCur == 2))
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[19] =
            (CB_557_CFIG_DEFAULT_PARM19 | CB_CFIG_FORCE_FDX);
        else
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[19] =
            CB_557_CFIG_DEFAULT_PARM19;
    }

    //
    // Store the current setting for BROADCAST/PROMISCUOS modes
    Adapter->OldParameterField = CB_557_CFIG_DEFAULT_PARM15;

    // display the config info to the debugger
    INITSTR(("Issuing Configure command\n"));
    INITSTR(("Config Block at virt addr %x, phys address %x\n",
        &Adapter->NonTxCmdBlockHdr->CbStatus, Adapter->NonTxCmdBlockPhys));

    for (i=0; i < CB_CFIG_BYTE_COUNT; i++)
        INITSTR(("  Config byte %x = %.2x\n", i, Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[i]));

    // Wait for the SCB command word to clear before we set the general pointer
    WaitScb(Adapter);

    ASSERT(Adapter->CSRAddress->ScbCommandLow == 0)
        Adapter->CSRAddress->ScbGeneralPointer = Adapter->NonTxCmdBlockPhys;

    // Submit the configure command to the chip, and wait for it to complete.
    if (!D100SubmitCommandBlockAndWait(Adapter))
        return (FALSE);
    else
        return (TRUE);
}


//-----------------------------------------------------------------------------
// Procedure:   SetupIAAddress
//
// Description: This routine will issue the IA setup command.  This command
//              will notify the 82557 (D100) of what its individual (node)
//              address is.  This command will be executed in polled mode.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      TRUE - If the IA setup command was successfully issued and completed
//      FALSE - If the IA setup command failed to complete properly
//-----------------------------------------------------------------------------

BOOLEAN
SetupIAAddress(
               IN PD100_ADAPTER Adapter
               )

{
    UINT        i;

    DEBUGFUNC("SetupIAAddress");


    // Individual Address Setup
    Adapter->NonTxCmdBlockHdr->CbStatus = 0;
    Adapter->NonTxCmdBlockHdr->CbCommand = CB_IA_ADDRESS;
    Adapter->NonTxCmdBlockHdr->CbLinkPointer = DRIVER_NULL;

    // Copy in the station's individual address
    for (i = 0; i < ETH_LENGTH_OF_ADDRESS; i++)
        Adapter->NonTxCmdBlock->NonTxCb.Setup.IaAddress[i] = Adapter->AiNodeAddress[i];

    // Update the command list pointer.  We don't need to do a WaitSCB here
    // because this command is either issued immediately after a reset, or
    // after another command that runs in polled mode.  This guarantees that
    // the low byte of the SCB command word will be clear.  The only commands
    // that don't run in polled mode are transmit and RU-start commands.
    ASSERT(Adapter->CSRAddress->ScbCommandLow == 0)
        Adapter->CSRAddress->ScbGeneralPointer = Adapter->NonTxCmdBlockPhys;

    // Submit the IA configure command to the chip, and wait for it to complete.
    if (D100SubmitCommandBlockAndWait(Adapter) == FALSE)
    {
        TRACESTR(Adapter, ("IA setup failed\n"));
        return (FALSE);
    }

    else
        return (TRUE);
}


//-----------------------------------------------------------------------------
// Procedure:   ClearAllCounters
//
// Description: This routine will clear the 82596/82556 error statistic
//              counters.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//-----------------------------------------------------------------------------

VOID
ClearAllCounters(
                 IN PD100_ADAPTER Adapter
                 )

{
    UINT    counter;
    DEBUGFUNC("ClearAllCounters");

    INITSTR(("\n"));


    // Load the dump counters pointer.  Since this command is generated only
    // after the IA setup has complete, we don't need to wait for the SCB
    // command word to clear
    ASSERT(Adapter->CSRAddress->ScbCommandLow == 0)
        Adapter->CSRAddress->ScbGeneralPointer = Adapter->StatsCounterPhys;

    // Issue the load dump counters address command
    D100IssueScbCommand(Adapter, SCB_CUC_DUMP_ADDR, FALSE);

    // Now dump and reset all of the statistics
    D100IssueScbCommand(Adapter, SCB_CUC_DUMP_RST_STAT, TRUE);

    // Now wait for the dump/reset to complete
    for (counter = 100000; counter != 0; counter--)
    {
        if (Adapter->StatsCounters->CommandComplete == 0xA007)
            break;
        NdisStallExecution(20);
    }
    if (!counter)
    {
        HARDWARE_NOT_RESPONDING (Adapter);
        return;
    }

    // init packet counts
    Adapter->GoodTransmits = 0;
    Adapter->GoodReceives = 0;

    // init transmit error counts
    Adapter->TxAbortExcessCollisions = 0;
    Adapter->TxLateCollisions = 0;
    Adapter->TxDmaUnderrun = 0;
    Adapter->TxLostCRS = 0;
    Adapter->TxOKButDeferred = 0;
    Adapter->OneRetry = 0;
    Adapter->MoreThanOneRetry = 0;
    Adapter->TotalRetries = 0;

    // init receive error counts
    Adapter->RcvCrcErrors = 0;
    Adapter->RcvAlignmentErrors = 0;
    Adapter->RcvResourceErrors = 0;
    Adapter->RcvDmaOverrunErrors = 0;
    Adapter->RcvCdtFrames = 0;
    Adapter->RcvRuntErrors = 0;
}

