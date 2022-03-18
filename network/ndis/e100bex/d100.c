/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    d100.c

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
#pragma warning (disable: 4244 4514 4706)

//-----------------------------------------------------------------------------
// Procedure:   DeleteSharedAdapterMemory
//
// Description: This routine is responsible for the deallocation of the
//              shared memory data structures for the Adapter structure.  This
//              includes the both the cached and uncached memory allocations.
//              We also free any allocated map registers in this routine.
//
// Arguments:
//      Adapter - Ptr to the Adapter structure
//
// Returns:     (none)
//-----------------------------------------------------------------------------

VOID
DeleteSharedAdapterMemory(PD100_ADAPTER Adapter)

{
#if DBG
    UINT RfdNum;
#endif
    UINT count;
    D100SwRfd *SwRfdPtr;
    DEBUGFUNC("DeleteSharedAdapterMemory");

    INITSTR(("\n"));

    // Free memory for receive packetpool and bufferpool
    if (Adapter->ReceivePacketPool)
    {
        TRACESTR(Adapter,("Freeing Packet Pool resources\n"));
        // get the first one to free
        SwRfdPtr = (D100SwRfd *) QueuePopHead(&Adapter->RfdList);     // cached RFDs logical

#if DBG
        count = 0;
#endif
        while (SwRfdPtr != (PD100SwRfd) 0)
        {

            FreeSwRfd(Adapter,SwRfdPtr);

            // walk our list of receive packets
            SwRfdPtr = (PD100SwRfd) QueuePopHead(&Adapter->RfdList);
#if DBG
            ++count;
#endif
        }

#if DBG
        // assert if we didnt free the full number of Rfds
        ASSERT(count == Adapter->NumRfd);
        // assert if our pool isnt empty
//        RfdNum = NdisPacketPoolUsage(Adapter->ReceivePacketPool);
//        ASSERT(!RfdNum);
#endif

        NdisFreeBufferPool(Adapter->ReceiveBufferPool);
        NdisFreePacketPool(Adapter->ReceivePacketPool);

    }

    // go through each element in our array and free the memory
    // pointed to by the element
    for (count = 0; count < NUM_RMD ; ++count)
    {
        // the packets and buffers should have already been freed

        // if a virtual address exists, we have to free the unit
        if (Adapter->ReceiveMemoryDescArray[count].CachedMem.VirtualAddress)
        {
            FreeRMD(Adapter,&Adapter->ReceiveMemoryDescArray[count]);
        }

    }

    // Free any memory allocated for the Original Software
    // receive structures (SwRfds)
    if (Adapter->RecvCached)
    {
        TRACESTR(Adapter, ("Freeing %d bytes RecvCached\n", Adapter->RecvCachedSize));
        NdisFreeMemory((PVOID) Adapter->RecvCached, Adapter->RecvCachedSize, 0);
        Adapter->RecvCached = (PUCHAR) 0;
    }

    // Free any memory allocated for the shared receive structures (RFDs)
    // the original ones...
    if (Adapter->RecvUnCached)
    {
        TRACESTR(Adapter, ("Freeing %d bytes RecvUnCached\n", Adapter->RecvUnCachedSize));
        // Now free the shared memory that was used for the receive buffers.
        NdisMFreeSharedMemory(
            Adapter->D100AdapterHandle,
            Adapter->RecvUnCachedSize,
            FALSE,
            (PVOID) Adapter->RecvUnCached,
            Adapter->RecvUnCachedPhys);
        Adapter->RecvUnCached = (PUCHAR) 0;
    }

    // Free any memory allocated for the Software transmit structures (SwTcbs)
    if (Adapter->XmitCached)
    {
        TRACESTR(Adapter, ("Freeing %d bytes XmitCached\n", Adapter->XmitCachedSize));
        NdisFreeMemory((PVOID) Adapter->XmitCached, Adapter->XmitCachedSize, 0);
        Adapter->XmitCached = (PUCHAR) 0;
    }

    // Free any memory allocated for the shared transmit structures (TCBs,
    // TBDs, non transmit command blocks, etc.)
    if (Adapter->CbUnCached)
    {
        TRACESTR(Adapter, ("Freeing %d bytes XmitUnCached\n", Adapter->CbUnCachedSize));


        // Now free the shared memory that was used for the command blocks and
        // transmit buffers.

        NdisMFreeSharedMemory(
            Adapter->D100AdapterHandle,
            Adapter->CbUnCachedSize,
            FALSE,
            (PVOID) Adapter->CbUnCached,
            Adapter->CbUnCachedPhys
            );
        Adapter->CbUnCached = (PUCHAR) 0;
        Adapter->XmitUnCached = (PUCHAR) 0;
    }

    // If this is a miniport driver we must free our allocated map registers
    if (Adapter->NumMapRegisters)
    {
        NdisMFreeMapRegisters(Adapter->D100AdapterHandle);
    }
}


//-----------------------------------------------------------------------------
// Procedure:   FreeAdapterObject
//
// Description: This routine releases all resources defined in the ADAPTER
//              object and returns the ADAPTER object memory to the free pool.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:     (none)
//-----------------------------------------------------------------------------

VOID
FreeAdapterObject(PD100_ADAPTER Adapter)

{

    DEBUGFUNC("FreeAdapterObject");
    INITSTR(("\n"));

    // Free the transmit, receive, and SCB structures
    DeleteSharedAdapterMemory(Adapter);

    // we must delete any IO mappings that we have registered

    if (Adapter->MappedIoBase)
    {
        NdisMDeregisterIoPortRange(
            Adapter->D100AdapterHandle,
            (UINT) Adapter->AiBaseIo,
            Adapter->MappedIoRange,
            (PVOID) Adapter->MappedIoBase);
    }

    // free the adapter object itself
    D100_FREE_MEM(Adapter, sizeof(D100_ADAPTER));

}


//-----------------------------------------------------------------------------
// Procedure:   SoftwareReset
//
// Description: This routine is called by SelfTestHardware and InitializeD100.
//              It resets the D100 by issuing a PORT SOFTWARE RESET.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      (none)
//-----------------------------------------------------------------------------

VOID
SoftwareReset(PD100_ADAPTER Adapter)

{
    DEBUGFUNC("SoftWareReset");
    INITSTR(("\n"));

    DEBUGCHAR(Adapter,'W');

    // Issue a PORT command with a data word of 0
    Adapter->CSRAddress->Port = PORT_SOFTWARE_RESET;

    // wait 20 milliseconds for the reset to take effect
    D100StallExecution(20);

    // Mask off our interrupt line -- its unmasked after reset
    D100DisableInterrupt(Adapter);
}


//-----------------------------------------------------------------------------
// Procedure:   DriverEntry
//
// Description: This is the primary initialization routine for the D100
//              driver. It is simply responsible for the intializing the
//              wrapper and registering the adapter driver.  The routine gets
//              called once per driver, but D100Initialize(miniport) or
//              AddAdapter (mac) will get called multiple times if there are
//              multiple adapters.
//
// Arguments:
//      DriverObject - Pointer to driver object created by the system.
//      RegistryPath - The registry path of this driver
//
// Returns:
//  The status of the operation, normally this will be NDIS_STATUS_SUCCESS
//-----------------------------------------------------------------------------

NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject,
            PUNICODE_STRING RegistryPath)

{

    NDIS_STATUS         Status;
    NDIS_HANDLE         NdisWrapperHandle;

    NDIS_MINIPORT_CHARACTERISTICS D100Char;

    DEBUGFUNC("D100DriverEntry");
    INITSTR(("\n"));

    // Now we must initialize the wrapper, and then register the Miniport
    NdisMInitializeWrapper( &NdisWrapperHandle,
        DriverObject,
        RegistryPath,
        NULL
        );

    NdisZeroMemory(&D100Char, sizeof(D100Char));

    // Initialize the Miniport characteristics for the call to
    // NdisMRegisterMiniport.
    D100Char.MajorNdisVersion       = D100_NDIS_MAJOR_VERSION;
    D100Char.MinorNdisVersion       = D100_NDIS_MINOR_VERSION;
    D100Char.CheckForHangHandler    = D100CheckForHang;
    D100Char.DisableInterruptHandler= D100DisableInterrupt;
    D100Char.EnableInterruptHandler = D100EnableInterrupt;
    D100Char.HaltHandler            = D100Halt;
    D100Char.HandleInterruptHandler = D100HandleInterrupt;
    D100Char.InitializeHandler      = D100Initialize;
    D100Char.ISRHandler             = D100Isr;
    D100Char.QueryInformationHandler= D100QueryInformation;
    D100Char.ReconfigureHandler     = NULL;
    D100Char.ResetHandler           = D100Reset;
    D100Char.SetInformationHandler  = D100SetInformation;
    D100Char.SendHandler            = NULL;
    D100Char.SendPacketsHandler     = D100MultipleSend;
    D100Char.ReturnPacketHandler    = D100GetReturnedPackets;
    D100Char.TransferDataHandler    = NULL;
    D100Char.AllocateCompleteHandler = D100AllocateComplete;


    DEBUGSTR(("DriverEntry: About to call NdisMRegisterMiniport\n"));

    // Register this driver with the NDIS wrapper
    //   This will cause D100Initialize to be called before returning
    Status = NdisMRegisterMiniport(
        NdisWrapperHandle,
        &D100Char,
        sizeof(NDIS_MINIPORT_CHARACTERISTICS));

    DEBUGSTR(("DriverEntry: NdisMRegisterMiniport returns %X\n", Status));

    if (Status == NDIS_STATUS_SUCCESS)
        return STATUS_SUCCESS;

    DEBUGSTR(("NdisMRegisterMiniport failed (Status = 0x%x)\n", Status));
    return Status;
}


//-----------------------------------------------------------------------------
// D100GetReturnedPackets
//
// PARAMETERS: IN NDIS_HANDLE MiniportAdapterContext
//                 - a context version of our Adapter pointer
//             IN NDIS_PACKET Packet
//                 - the packet that is being freed
//
// DESCRIPTION: This function attempts to return to the receive free list the
//              packet passed to us by NDIS
//
// RETURNS: nothing
//
//-----------------------------------------------------------------------------

VOID
D100GetReturnedPackets(NDIS_HANDLE  MiniportAdapterContext,
                       PNDIS_PACKET Packet)
{
    PD100_ADAPTER   Adapter;
    PD100SwRfd      SwRfd;

    DEBUGFUNC("D100GetReturnedPackets");
    Adapter = PD100_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    NdisAcquireSpinLock(&Adapter->Lock);
    TRACE2(Adapter, ("\n"));

    ASSERT(Packet);

    DEBUGCHAR(Adapter,'G');
    SwRfd = *(D100SwRfd **)(Packet->MiniportReserved);
    ASSERT(SwRfd);

    InitAndChainPacket(Adapter, SwRfd);

    --Adapter->UsedRfdCount;
    ASSERT(Adapter->UsedRfdCount <= Adapter->NumRfd);

    NdisReleaseSpinLock(&Adapter->Lock);

    return;
}

//-----------------------------------------------------------------------------
// PROCEDURE: InitAndChainPacket
//
// PARAMETERS: Adapter structure Pointer
//              Pointer to SwRfd
//
//  DESCRIPTION: Re-initializes the given SwRfd super-structure
//               and chains it onto the current receive chain
//               We don't need to lock around this routine because
//
//  RETURNS: nothing
//
//-----------------------------------------------------------------------------

VOID
InitAndChainPacket(PD100_ADAPTER Adapter,
                   PD100SwRfd SwRfdPtr)
{
    PRFD_STRUC      Rfd;
    PD100SwRfd      LastRfd;

    DEBUGFUNC("InitAndChainPacket");
    TRACE2(Adapter,("\n"));
    DEBUGCHAR(Adapter,'9');

    Rfd = SwRfdPtr->Rfd;
    // Now we can release the receive resources back to the 82557.
    // We are done with this RFD so re-initialize it.
    Rfd->RfdCbHeader.CbStatus = 0;
    Rfd->RfdActualCount = 0;
    Rfd->RfdCbHeader.CbCommand = (RFD_EL_BIT);
    Rfd->RfdCbHeader.CbLinkPointer = DRIVER_NULL;

    // we remove this step right now because we
    // don't use any of the OOB data besides status
    // if we were noticing receive packet priorities we would probably
    // uncomment this code.
    //    NdisZeroMemory(NDIS_OOB_DATA_FROM_PACKET(SwRfd->ReceivePacket),14);


    // Append the RFD to the end of the 82557 RFD chain.  Normally, this
    // will always be done.  The only exception would be if the driver
    // was configured to use only 1 RFD (a degenerate case anyway).
    if (!QueueEmpty(&Adapter->RfdList))
    {
        LastRfd = (PD100SwRfd)QueueGetTail(&Adapter->RfdList);

        ASSERT(LastRfd);

        // Link it onto the end of the chain dynamically
        Rfd = LastRfd->Rfd;
        Rfd->RfdCbHeader.CbLinkPointer = SwRfdPtr->RfdPhys;
        Rfd->RfdCbHeader.CbCommand = 0;
        DEBUGCHAR(Adapter,(char)(LastRfd->RfdNum+48));
    }

    // The processing on this RFD is done, so put it back on the tail of
    // our list
    QueuePutTail(&Adapter->RfdList, &SwRfdPtr->Link);

    DEBUGCHAR(Adapter,'a');
}

//-----------------------------------------------------------------------------
// Procedure:   D100CheckForHang
//
// Description: This routine should check to see if the adapter is "hung", and
//              fix it if it is.  Right now this routine does not check for a
//              hang, because the adapter should never "timeout".  In the
//              future, this is where the code to check for a timeout would go,
//              if there were bugs in the chipset that could cause transmit
//              "timeouts".
//
// Arguments:
//      MiniportAdapterContext (both) - pointer to the adapter object data area
//
// Returns:
//      False (miniport) - Always
//      Nothing (mac) - Cause the function is a VOID
//-----------------------------------------------------------------------------

BOOLEAN
D100CheckForHang(NDIS_HANDLE MiniportAdapterContext)
{
    PD100_ADAPTER Adapter;
    DEBUGFUNC("D100CheckForHang");

    Adapter = PD100_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    NdisAcquireSpinLock(&Adapter->Lock);
    DEBUGCHAR(Adapter,'U');


    // We need to dump our statistics if we are implementing the "periodic multicast
    // command workaround"

    if (Adapter->McTimeoutFlag)
    {
        // Dump the current stats
        DumpStatsCounters(Adapter);

        // If we haven't received any frames recently, and we are implementing
        // the "periodic multicast command workaround", then we'll chain in
        // a multicast command to the transmit chain here.
        if ((Adapter->McTimeoutFlag)
            && (!(Adapter->StatsCounters->RcvGoodFrames))
            && (Adapter->AiRevID < D101_A_STEP))
            DoBogusMulticast(Adapter);
    }

    // this is the implementation of the NDIS 4 feature for detecting
    // link status change. It effectively checks every two seconds
    // for link.
    if (Adapter->LinkIsActive != GetConnectionStatus(Adapter))
        // if status has changed
    {
        DEBUGSTR(("e100bex: CheckforHang: media state changed to %s\n",
                    ((Adapter->LinkIsActive == NdisMediaStateConnected)?
                        "Disconnected": "Connected")));

        switch ( Adapter->LinkIsActive )
        {
        case NdisMediaStateConnected:           // changing from connected
            Adapter->LinkIsActive = NdisMediaStateDisconnected;
            NdisReleaseSpinLock(&Adapter->Lock);
            NdisMIndicateStatus(Adapter->D100AdapterHandle,
                NDIS_STATUS_MEDIA_DISCONNECT,
                (PVOID)0,
                0);
            // NOTE:
            // have to indicate status complete every time you indicate status
            NdisMIndicateStatusComplete(Adapter->D100AdapterHandle);

            NdisAcquireSpinLock(&Adapter->Lock);
            break;
        case NdisMediaStateDisconnected:        // changing from disconnected
            Adapter->LinkIsActive = NdisMediaStateConnected;
            NdisReleaseSpinLock(&Adapter->Lock);
            NdisMIndicateStatus(Adapter->D100AdapterHandle,
                NDIS_STATUS_MEDIA_CONNECT,
                (PVOID)0,
                0);
            // NOTE:
            // have to indicate status complete every time you indicate status
            NdisMIndicateStatusComplete(Adapter->D100AdapterHandle);

            NdisAcquireSpinLock(&Adapter->Lock);
            break;
        }
    }


    // return false to indicate that the adapter is not hung, and that
    // D100Reset does NOT need to be called by the wrapper
    DEBUGCHAR(Adapter,'u');

    NdisReleaseSpinLock(&Adapter->Lock);

    return(FALSE);
}


//-----------------------------------------------------------------------------
// Procedure:   D100Halt
//
// Description: Removes an adapter instance that was previously initialized.
//              To "halt" or "remove" an adapter, we disable its interrupt,
//              abort its receive unit (otherwise it would continue to DMA in
//              data), and release all of the resources (memory, i/o space,
//              etc.) that the adapter instance was using.
//              This routine is only called when the adapter is "stopped"
//              or unloaded with a "net stop e100b". To see what is called
//              at machine shutdown see D100ShutdownHandler.
//
// Arguments:
//      MiniportAdapterContext - pointer to the adapter object data area.
//
// Returns:     (none)
//-----------------------------------------------------------------------------

VOID
D100Halt(NDIS_HANDLE MiniportAdapterContext)

{
    PD100_ADAPTER Adapter;

    BOOLEAN     Cancelled;
    DEBUGFUNC("D100Halt");


    DEBUGSTR(("D100Halt\n"));

    Adapter = PD100_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    // Disable the device's interrupt line.
    D100DisableInterrupt(Adapter);

    // check to make sure there are no outstanding transmits
    while(Adapter->FirstTxQueue)
    {
        PNDIS_PACKET QueuePacket = Adapter->FirstTxQueue;

        Adapter->NumPacketsQueued--;
        DequeuePacket(Adapter->FirstTxQueue, Adapter->LastTxQueue);

        NdisMSendComplete(
            Adapter->D100AdapterHandle,
            QueuePacket,
            NDIS_STATUS_FAILURE);

    }

    while (!(QueueEmpty(&Adapter->ActiveChainList)))
    {
        // here we have to fail any sends that haven't completed yet
        PD100SwTcb pSwTcb = (PD100SwTcb) QueuePopHead(&Adapter->ActiveChainList);

        // If this wasn't a multicast command, then we need to check to see
        // if we need to issue send complete
        if ((pSwTcb->Tcb->TxCbHeader.CbCommand & CB_CMD_MASK) != CB_MULTICAST)
        {
            DEBUGCHAR(Adapter,'-');
            // Do a send Complete for this frame
            NdisMSendComplete(
                Adapter->D100AdapterHandle,
                pSwTcb->Packet,
                NDIS_STATUS_FAILURE);
        }
    }

    // deregister shutdown handler because we are halting now, and don't
    // want to have a bugcheck handler registered any more
    NdisMDeregisterAdapterShutdownHandler(Adapter->D100AdapterHandle);

    // added code to cancel our timer if for some reason it was active
    NdisMCancelTimer(&Adapter->D100AsyncResetTimer,
        &Cancelled);

    // Free the interrupt object
    NdisMDeregisterInterrupt(&Adapter->Interrupt);

    // Abort the Receive unit. so we don't continue to receive packets.  If
    // we didn't abort the Receive unit we could still DMA receive packets
    // into host memory after a warm boot.
    DEBUGCHAR(Adapter,'<');
    D100IssueScbCommand(Adapter, SCB_RUC_ABORT, TRUE);

    // Wait 30 Milliseconds for the device to abort the RU.  This really
    // is not necessary, but I'm a little paranoid about reseting the PHY
    // when the RU is active.
    D100StallExecution(30);

    // Reset the PHY chip.  We do this so that after a warm boot, the PHY will
    // be in a known state, with auto-negotiation enabled.
    ResetPhy(Adapter);

    NdisFreeSpinLock(&Adapter->Lock);

    // Free the entire adapter object, including the shared memory structures.
    FreeAdapterObject(Adapter);
}

//-----------------------------------------------------------------------------
// Procedure:   D100ShutdownHandler
//
// Description: Removes an adapter instance that was previously initialized.
//              To Shutdown simply Disable interrupts and Stop the receive unit.
//              Since the system is shutting down there is no need to release
//              resources (memory, i/o space, etc.) that the adapter instance
//              was using.
//
// Arguments:
//      MiniportAdapterContext - pointer to the adapter object data area.
//
// Returns:     (none)
//-----------------------------------------------------------------------------

VOID
D100ShutdownHandler(NDIS_HANDLE MiniportAdapterContext)

{

    PD100_ADAPTER Adapter;
    DEBUGFUNC("D100ShutdownHandler");

    Adapter = PD100_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    DEBUGSTR(("D100ShutdownHandler %8x\n",Adapter));
    DEBUGCHAR(Adapter,'!');

    // Disable the device's interrupt line.
    D100DisableInterrupt(Adapter);

    // Abort the Receive unit. so we don't continue to receive packets.  If
    // we didn't abort the Receive unit we could still DMA receive packets
    // into host memory after a warm boot.
    D100IssueScbCommand(Adapter, SCB_RUC_ABORT, TRUE);

    // Wait 30 Milliseconds for the device to abort the RU.  This really
    // is not necessary, but I'm a little paranoid about reseting the PHY
    // when the RU is active.
    D100StallExecution(30);

    // Reset the PHY chip.  We do this so that after a warm boot, the PHY will
    // be in a known state, with auto-negotiation enabled.
    ResetPhy(Adapter);

}

//-----------------------------------------------------------------------------
// Procedure:   D100Initialize/D100AddAdapter
//
// Description: This routine is called once per each supported adapter card in
//              the system.  This routine is responsible for initializing each
//              adapter.  This includes parsing all of the necessary parameters
//              from the registry, allocating and initializing shared memory
//              structures, configuring the 82557 chip, registering the
//              interrupt, and starting the receive unit.
//
// Arguments:
//      OpenErrorStatus (mini) - Returns more info about any failure
//      SelectedMediumIndex (mini) - Returns the index in MediumArray of the
//                                   medium that the miniport is using
//      MediumArraySize (mini) - An array of medium types that the driver
//                               supports
//      MiniportAdapterHandle (mini) - pointer to the adapter object data area.
//
//      WrapperConfigurationContext (both) - A value that we will pass to
//                                           NdisOpenConfiguration.
//
//
// Returns:
//      NDIS_STATUS_SUCCESS - If the adapter was initialized successfully.
//      <not NDIS_STATUS_SUCCESS> - If for some reason the adapter didn't
//                                  initialize
//-----------------------------------------------------------------------------

NDIS_STATUS
D100Initialize(PNDIS_STATUS OpenErrorStatus,
               PUINT SelectedMediumIndex,
               PNDIS_MEDIUM MediumArray,
               UINT MediumArraySize,
               NDIS_HANDLE MiniportAdapterHandle,
               NDIS_HANDLE WrapperConfigurationContext)
{
    ULONG               i;
    NDIS_STATUS         Status;
    PD100_ADAPTER       Adapter;
    NDIS_HANDLE         ConfigHandle;
    NDIS_INTERFACE_TYPE IfType;
    PVOID               OverrideNetAddress;

    DEBUGFUNC("D100Initialize");
    INITSTR(("\n"));

    // If this is a miniport, then fill in the media information.

    // if medium type 802.3 not found in list, exit with error)
    for (i = 0; i < MediumArraySize; i++)
    {
        if (MediumArray[i] == NdisMedium802_3) break;
    }

    if (i == MediumArraySize)
    {
        DEBUGSTR(("802.3 Media type not found.\n"));
        return NDIS_STATUS_UNSUPPORTED_MEDIA;
    }

    // Select ethernet
    *SelectedMediumIndex = i;

    // Allocate the Adapter Object, exit if error occurs
    Status = D100_ALLOC_MEM(&Adapter, sizeof(D100_ADAPTER));

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DEBUGSTR(("ADAPTER Allocate Memory failed (Status = 0x%x)\n", Status));
        return Status;
    }

    //Zero out the adapter object space
    NdisZeroMemory(Adapter, sizeof(D100_ADAPTER));

    Adapter->D100AdapterHandle = MiniportAdapterHandle;

    INITSTR(("Adapter structure pointer is %08x\n", Adapter));

    // Open Registry, exit if error occurs.
    NdisOpenConfiguration(&Status,
        &ConfigHandle,
        WrapperConfigurationContext);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DEBUGSTR(("NdisOpenConfiguration failed (Status = 0x%x)\n", Status));
        FreeAdapterObject(Adapter);
        return NDIS_STATUS_FAILURE;
    }

    // Parse all of our configuration parameters.
    Status = ParseRegistryParameters(Adapter, ConfigHandle);

    // If a required configuration parameter was not present, then error out
    if (Status != NDIS_STATUS_SUCCESS)
    {
        DEBUGSTR(("ParseRegistryParameters failed (Status = 0x%x)\n", Status));
        NdisCloseConfiguration(ConfigHandle);
        FreeAdapterObject(Adapter);
        return Status;
    }

    // initialize the number of Tcbs we will have in our queue
    // this is to work around limitations of the hardware and the S-bit
    Adapter->NumTcb = Adapter->RegNumTcb + 1;

    DEBUGSTR(("ParseRegistryParameters Completed successfully\n"));

    // Look for a Node Address (IA) override
    // this is where the registry parameter NodeAddress in
    // HKLM\System\CCS\Services\E100BX\Parameters

    NdisReadNetworkAddress(&Status,
        &OverrideNetAddress,
        (UINT *) &i,
        ConfigHandle);

    // If there is an IA override save it to the adapter object
    if ((i == ETH_LENGTH_OF_ADDRESS) && (Status == NDIS_STATUS_SUCCESS))
        NdisMoveMemory(Adapter->AiNodeAddress,
        OverrideNetAddress,
        ETH_LENGTH_OF_ADDRESS);

    // We read out all of our config info, so close the gateway to the registry
    NdisCloseConfiguration(ConfigHandle);

    // register our adapter object with the OS as a PCI adapter
    IfType = NdisInterfacePci;

    // call NdisMSetAttributesEx in order to let NDIS know
    // what kind of driver and features we support
    NdisMSetAttributesEx(
        Adapter->D100AdapterHandle,
        (NDIS_HANDLE) Adapter,
        0,
        (ULONG)
        NDIS_ATTRIBUTE_DESERIALIZE |
        NDIS_ATTRIBUTE_BUS_MASTER,
        IfType
        );

    // Assign (Claim) a physical Adapter for this Adapter object
    // In this function is where we find our adapter on the PCI bus and
    // call NdisMPciAssignResources
    if (ClaimAdapter(Adapter) != NDIS_STATUS_SUCCESS)
    {
        DEBUGSTR(("No adapter detected\n"));
        FreeAdapterObject(Adapter);
        return NDIS_STATUS_FAILURE;
    }

    // set up our Command Register's I/O mapping (the CSR)
    Status = SetupAdapterInfo(Adapter);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DEBUGSTR(("I/O Space allocation failed (Status = 0x%X)\n",Status));
        FreeAdapterObject(Adapter);
        return(NDIS_STATUS_FAILURE);
    }

    // allocate & Initialize the required 82557 (D100) Shared Memory areas
    Status = SetupSharedAdapterMemory(Adapter);

    // Check the status returned from SetupSharedAdapterMemory
    if (Status != NDIS_STATUS_SUCCESS)
    {
        // Since we couldn't allocate enough shared memory, free any resources
        // that we previously allocated and error out.
        D100LogError(Adapter, EVENT_10, NDIS_ERROR_CODE_OUT_OF_RESOURCES, 0);
        DEBUGSTR(("Shared Memory Allocation failed (Status = 0x%x)\n", Status));

        // Free our adapter object
        FreeAdapterObject(Adapter);

        return NDIS_STATUS_FAILURE;
    }

    // Disable interrupts while we finish with the initialization
    // Must SetupSharedAdapterMemory() before you can do this
    // (fixed bug) must check for success of alloc sharedmem before
    // calling D100DisableInterrupt.
    D100DisableInterrupt(Adapter);

    // Next we'll register our interrupt with the NDIS wrapper.

    // Hook our interrupt vector.  We used level-triggered, shared interrupts
    // with our PCI adapters
    DEBUGSTR(("RegisterIrq: handl=%x, irq=%x, mode=%x\n",
        Adapter->D100AdapterHandle, Adapter->AiInterrupt,
        Adapter->InterruptMode));

    Status = NdisMRegisterInterrupt(&Adapter->Interrupt,
        Adapter->D100AdapterHandle,
        Adapter->AiInterrupt,
        Adapter->AiInterrupt,
        FALSE,
        TRUE, /* shared irq */
        Adapter->InterruptMode);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        TRACESTR(Adapter,("Interrupt conflict, Status %08x, IRQ %d, %s Sensitive\n",
            Status,
            Adapter->AiInterrupt,
            (Adapter->InterruptMode==NdisInterruptLevelSensitive)?"Level":"Edge")
            );
        D100LogError(Adapter,
            EVENT_0,
            NDIS_ERROR_CODE_INTERRUPT_CONNECT,
            (ULONG) Adapter -> AiInterrupt);

        // Free the entire adapter object and error out
        FreeAdapterObject(Adapter);

        return NDIS_STATUS_FAILURE;
    }

    // Test our adapter hardware.  If the adapter is not seated in a bus
    // mastering slot, or if the master enable bit is not set in the adapter's
    // PCI configuration space, the self-test will fail.
    Status = SelfTestHardware(Adapter);

    if(Status != NDIS_STATUS_SUCCESS)
    {
        // Since the adapter failed the self-test, free any resources that
        // we previously allocated and error out.
        DEBUGSTR(("Adapter Self Test Failure.\n"));

        NdisMDeregisterInterrupt(&Adapter->Interrupt);
        FreeAdapterObject(Adapter);
        return NDIS_STATUS_FAILURE;
    }

    // Setup and initialize the transmit structures.
    SetupTransmitQueues(Adapter, TRUE);

    // configure the 82557 (D100) chip.
    if (!InitializeAdapter(Adapter))
    {
        // Since the adapter failed to initialize, free any resources that
        // we previously allocated and error out.
        DEBUGSTR(("InitializeAdapter Failed.\n"));

        NdisMDeregisterInterrupt(&Adapter->Interrupt);
        FreeAdapterObject(Adapter);
        return NDIS_STATUS_FAILURE;
    }

    // allocate a spin lock for locking at all our entry points
    NdisAllocateSpinLock(&Adapter->Lock);
    //    DbgPrint ("E100B: Adapter->Lock address is %8X\n", (char *) &Adapter->Lock);
    //    DbgPrint ("E100B: Adapter->SendQueueListHead address is  0x%8X\n", (char *) &(Adapter->FirstTxQueue));

    // Setup and initialize the receive structures
    SetupReceiveQueues(Adapter);

    // Start the receive unit -- we can now receive packets off the wire
    StartReceiveUnit(Adapter);

    // register a shutdown handler...
    NdisMRegisterAdapterShutdownHandler(Adapter->D100AdapterHandle,
        (PVOID) Adapter,
        (ADAPTER_SHUTDOWN_HANDLER) D100ShutdownHandler);

    // Init a timer for use with our reset routine...
    NdisMInitializeTimer(&Adapter->D100AsyncResetTimer,
        Adapter->D100AdapterHandle,
        (PNDIS_TIMER_FUNCTION) D100ResetComplete,
        (PVOID) Adapter);

    // Enable board interrupts
    D100EnableInterrupt(Adapter);

    DEBUGSTR(("D100Initialize: Completed Init Successfully\n"));
    return NDIS_STATUS_SUCCESS;
}


//-----------------------------------------------------------------------------
// Procedure:   D100Reset
//
// Description: Instructs the Miniport to issue a hardware reset to the
//              network adapter.  The driver also resets its software state.
//              this function also resets the transmit queues.
//
// Arguments:
//      AddressingReset - TRUE if the wrapper needs to call
//                        MiniportSetInformation to restore the addressing
//                        information to the current values
//      MiniportAdapterContext - pointer to the adapter object data area.
//
// Returns:
//      NDIS_STATUS_PENDING - This function sets a timer to complete, so
//                            pending is always returned
//-----------------------------------------------------------------------------

NDIS_STATUS
D100Reset(PBOOLEAN AddressingReset,
          NDIS_HANDLE MiniportAdapterContext)
{
    PD100_ADAPTER Adapter;
    DEBUGFUNC("D100Reset");
    INITSTR(("\n"));

    Adapter = PD100_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    NdisAcquireSpinLock(&Adapter->Lock);

    DEBUGCHAR(Adapter,'$');
    *AddressingReset = TRUE;

    // *** possible temporary code
    // *** NDIS may actually handle this
    Adapter->ResetInProgress = TRUE;

    // Disable interrupts while we re-init the transmit structures
    D100DisableInterrupt(Adapter);

    // The NDIS 5 support for deserialized miniports requires that
    // when reset is called, the driver de-queue and fail all uncompleted
    // sends, and complete any uncompleted sends. Essentially we must have
    // no pending send requests left when we leave this routine.


    // we will fail all sends that we have left right now.
    DEBUGSTR(("DeQing: "));
    while(Adapter->FirstTxQueue)
    {
        PNDIS_PACKET QueuePacket = Adapter->FirstTxQueue;

        Adapter->NumPacketsQueued--;
        DequeuePacket(Adapter->FirstTxQueue, Adapter->LastTxQueue);

        // we must release the lock here before returning control to ndis
        // (even temporarily like this)
        NdisReleaseSpinLock(&Adapter->Lock);
        NdisMSendComplete(
            Adapter->D100AdapterHandle,
            QueuePacket,
            NDIS_STATUS_FAILURE);

        NdisAcquireSpinLock(&Adapter->Lock);
    }
    DEBUGSTR(("\nDone!\n"));

    // clean up all the packets we have successfully TX'd
    ProcessTXInterrupt(Adapter);

    while (!(QueueEmpty(&Adapter->ActiveChainList)))
    {
        // here we have to fail any sends that haven't completed yet
        PD100SwTcb pSwTcb = (PD100SwTcb) QueuePopHead(&Adapter->ActiveChainList);

        // If this wasn't a multicast command, then we need to check to see
        // if we need to issue send complete
        if ((pSwTcb->Tcb->TxCbHeader.CbCommand & CB_CMD_MASK) != CB_MULTICAST)
        {
            DEBUGCHAR(Adapter,'-');
            NdisReleaseSpinLock(&Adapter->Lock);
            // Do a send Complete for this frame
            NdisMSendComplete(
                Adapter->D100AdapterHandle,
                pSwTcb->Packet,
                NDIS_STATUS_FAILURE);
            NdisAcquireSpinLock(&Adapter->Lock);
        }
    }

    // Issue a selective reset to make the CU idle.  This will also abort
    // the RU, and make the receive unit go idle.
    D100IssueSelectiveReset(Adapter);

    // since the d100/d101 both assert the interrupt line after reset,
    // lets try disabling ints here, because we don't handle
    // anything on a reset interrupt anyway.
    D100DisableInterrupt(Adapter);

    // Clear out our software transmit structures
    NdisZeroMemory((PVOID) Adapter->XmitCached, Adapter->XmitCachedSize);

    // re-init the map register related variables
    Adapter->NextFreeMapReg = 0;
    Adapter->OldestUsedMapReg = 0;

    // re-initialize the transmit structures
    DEBUGSTR(("D100Reset: Calling SetupTransmitQueues\n"));
    SetupTransmitQueues(Adapter, FALSE);

    // set a timer to call us back so
    // that we don't try to hold a spinlock too long
    // delay 500 ms
    NdisMSetTimer(&Adapter->D100AsyncResetTimer,500);

    NdisReleaseSpinLock(&Adapter->Lock);

    // return status_pending so that we can finish this later
    return(NDIS_STATUS_PENDING);
}

//-----------------------------------------------------------------------------
// D100ResetComplete
//
// PARAMETERS: NDIS_HANDLE MiniportAdapterContext
//
// DESCRIPTION: This function is called by a timer indicating our
//              reset is done (by way of .5 seconds expiring)
//
// RETURNS: nothing, but sets NdisMResetComplete, enables ints
//          and starts the receive unit
//
//-----------------------------------------------------------------------------

VOID
D100ResetComplete(PVOID sysspiff1,
                  NDIS_HANDLE MiniportAdapterContext,
                  PVOID sysspiff2, PVOID sysspiff3)
{
    PD100_ADAPTER Adapter;
    DEBUGFUNC("D100ResetComplete");

    INITSTR(("\n"));
    Adapter = PD100_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    DEBUGCHAR(Adapter,'@');

    NdisMResetComplete(Adapter->D100AdapterHandle,
        (NDIS_STATUS) NDIS_STATUS_SUCCESS,
        FALSE);

    Adapter->ResetInProgress = FALSE;


    DEBUGSTR(("D100Reset: Calling StartReceiveUnit\n"));
    // Start the receive unit and indicate any pending receives that we
    // had left in our queue.

    if (StartReceiveUnit(Adapter))
    {
        TRACE2(Adapter, ("Indicating Receive complete\n"));
        DEBUGCHAR(Adapter,'^');
        NdisMEthIndicateReceiveComplete(Adapter->D100AdapterHandle);
    }

    D100EnableInterrupt(Adapter);

    return;
}
//-----------------------------------------------------------------------------
// Procedure: D100AllocateComplete
//
// Description: This function handles initialization of new receive memory
//              when the os returns some shared memory to us because we
//              called NdisMAllocateSharedMemoryAsync when we ran low on
//              receive buffers.
//
// Arguments: MiniportAdapterContext - a pointer to our adapter structure
//            VirtualAddress - The virtual address of the new memory
//            PhysicalAddress - _pointer to_ The physical address of the
//                              new memory
//            Length - The length of the new memory
//            Context - The offset into our MemoryDescriptor array to
//                      initialize (zero-based)
//
// Returns: Nothing
//
//-----------------------------------------------------------------------------

VOID
D100AllocateComplete(NDIS_HANDLE MiniportAdapterContext,
                     IN PVOID VirtualAddress,
                     IN PNDIS_PHYSICAL_ADDRESS PhysicalAddress,
                     IN ULONG Length,
                     IN PVOID Context)
{
    PD100_ADAPTER Adapter = PD100_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);
    NDIS_STATUS AllocationStatus;
    NDIS_STATUS Status;
    UINT RfdCount;
    UINT OldNumRfds = Adapter->NumRfd;
    D100SwRfd *SwRfdPtr, *SwRfdNext;  // cached RFD list logical pointers
    ULONG HwRfdPhys;  // uncached RFD list physical pointer
    ReceiveMemoryDescriptor *current;

    DEBUGFUNC("D100AllocateComplete");

    current = &Adapter->ReceiveMemoryDescArray[PtrToUint(Context)];
    INITSTR(("\n"));

    // check if any of the three variables that indicate failure are zero,
    // indicating that a part of the allocation failed
    if ((VirtualAddress == 0)
        || (NdisGetPhysicalAddressLow(*PhysicalAddress) == 0)
        || (Length == 0))
    {
        // ndismfreesharedmemory?
        Adapter->AsynchronousAllocationPending = FALSE;
        return;
    }


    // catch the case where we have waaaay too many receive buffers.
    if((Adapter->NumRfd + packet_count[PtrToUint(Context)]) >= MAX_RECEIVE_DESCRIPTORS)
    {
        NdisMFreeSharedMemory(Adapter->D100AdapterHandle,
            Length,
            FALSE,
            (PVOID) VirtualAddress,
            *PhysicalAddress);
        Adapter->AsynchronousAllocationPending = FALSE;
        return;
    }

    NdisAcquireSpinLock(&Adapter->Lock);

    // allocate and setup some cached memory
    // also initialize the uncached areas of the RMD
    // (receive memory descriptor)
    Status = AllocateRMD(current,
        packet_count[PtrToUint(Context)],
        (ULONG_PTR) VirtualAddress,
        *PhysicalAddress,
        Length);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        D100LogError(Adapter, EVENT_20, NDIS_ERROR_CODE_OUT_OF_RESOURCES, 0);

        INITSTR(("Could not allocate %d bytes for ExtraRecvCached mem\n", current->CachedMem.Size));

        NdisReleaseSpinLock(&Adapter->Lock);

        NdisMFreeSharedMemory(Adapter->D100AdapterHandle,
            Length,
            FALSE,
            (PVOID) VirtualAddress,
            *PhysicalAddress);

        Adapter->AsynchronousAllocationPending = FALSE;
        return;
    }

    // increment our RMD counter
    ++(Adapter->Last_RMD_used);

    // update the number of Rfds we have
    Adapter->NumRfd += packet_count[PtrToUint(Context)];

    //    SwRfdNext = (D100SwRfd *)current->CachedMem.VirtualAddress;// cached RFDs logical
    //    HwRfdNext = (RFD_STRUC *)xtraRecvUnCached;   // uncached RFDs logical


    for (RfdCount = 0;
    RfdCount < (Adapter->NumRfd - OldNumRfds);
    ++RfdCount)
    {
        SwRfdPtr = BuildSwRfd(Adapter,current, RfdCount);

        // if something goes wrong with packet pool (should be impossible)
        if (SwRfdPtr == NULL)
            break;

        // call init and chain packet to put the packet on our available list
        InitAndChainPacket(Adapter,SwRfdPtr);

    }


    Adapter->AsynchronousAllocationPending = FALSE;

    NdisReleaseSpinLock(&Adapter->Lock);

    return;
}

//-----------------------------------------------------------------------------
// Procedure: AllocateRMD
//
// Description: This function allocates and initializes a
//              ReceiveMemoryDescriptor (RMD) for use in maintaining pointers
//              when asyncronously allocating receive memory.
//              this includes allocating cached memory for software structures
//              and initializing the uncached memory associated with it.
//
// Arguments: new - the pointer to the structure/memory to init
//            count - how many receive frames will be in this cached/uncached mem
//            Virt - The virtual address of the uncached memory
//            Phys - the physical address of the uncached memory
//            Len - the length of the uncached memory
//
// Returns: a modified new pointer and an NDIS_STATUS of the success or failure
//          to allocate cached memory
//
//-----------------------------------------------------------------------------
NDIS_STATUS
AllocateRMD(ReceiveMemoryDescriptor *new,
            UINT count,
            ULONG_PTR Virt,
            NDIS_PHYSICAL_ADDRESS Phys,
            UINT Len)
{

    NDIS_STATUS status;
    DEBUGFUNC("AllocateRMD");


    // first take care of the Cached Memory needs for our
    // SwRfds
    new->CachedMem.Size = (count * sizeof(D100SwRfd));
    status = D100_ALLOC_MEM(&new->CachedMem.VirtualAddress, new->CachedMem.Size);

    if (status == NDIS_STATUS_SUCCESS)
    {
        INITSTR(("Allocated %08x %8d bytes for ExtraRecvCached mem\n",new->CachedMem.VirtualAddress,new->CachedMem.Size));

        NdisZeroMemory((PVOID) new->CachedMem.VirtualAddress, new->CachedMem.Size);

        // now set up the uncached memory
        new->UnCachedMem.VirtualAddress  = (ULONG)(Virt);
        new->UnCachedMem.PhysicalAddress = Phys;
        new->UnCachedMem.Size            = Len;
    }

    return(status);

}

//-----------------------------------------------------------------------------
// Procedure: BuildSwRfd
//
// Description: Initializes a single SwRfd structure
//
// Arguments:   Adapter - the adapter structure
//              newmem  - a pointer to the RMD structure that this RFD will
//                        be allocated from
//              startpoint - the number of the RFD in this RMD
//
// Returns: a pointer to the SwRfd initialized
//
//-----------------------------------------------------------------------------
PD100SwRfd
BuildSwRfd(PD100_ADAPTER Adapter,
           ReceiveMemoryDescriptor *newmem,
           UINT startpoint)
{
    D100SwRfd *rfdptr;       // the new SwRfd we are creating
    RFD_STRUC *hwptr;     // uncached RFD list logical pointer
    ULONG hwphys;
    NDIS_STATUS AllocationStatus;
    D100SwRfd **TempPtr;
    D100SwRfd *rfdvirtual;
    RFD_STRUC *hwptrvirtual;
    RFD_STRUC *hwphysvirtual;

    DEBUGFUNC("BuildSwRfd");

    // first spin the cached pointer along to where it needs to be
    rfdvirtual = (D100SwRfd *) newmem->CachedMem.VirtualAddress;
    rfdptr = &rfdvirtual[startpoint];

    // now move the uncached pointer along also
    hwptrvirtual = (RFD_STRUC *) newmem->UnCachedMem.VirtualAddress;
    hwptr = &hwptrvirtual[startpoint];

    hwphysvirtual = (RFD_STRUC *) NdisGetPhysicalAddressLow(newmem->UnCachedMem.PhysicalAddress);
    hwphys = (ULONG) PtrToUlong(&hwphysvirtual[startpoint]);

    INITSTR((" RfdCount=%d\n", startpoint));
    INITSTR(("   SwRfdPtr=%lx\n", rfdptr));
    INITSTR(("   HwRfdPtr=%lx\n", hwptr));
    INITSTR(("   HwRfdPhys=%lx\n", hwphys));

    // point the logical RFD to the pointer of the physical one
    rfdptr->Rfd = hwptr;

    // store the physical address in the Software RFD Structure
    rfdptr->RfdPhys = hwphys;

    // Init each RFD header
    hwptr->RfdCbHeader.CbStatus = 0;
    hwptr->RfdRbdPointer = DRIVER_NULL;
    hwptr->RfdActualCount= 0;
    hwptr->RfdSize = sizeof(ETH_RX_BUFFER_STRUC);

    // set up the packet structure for passing up this Rfd
    // with NdisMIndicateReceivePacket

    NdisAllocatePacket(&AllocationStatus,
        &rfdptr->ReceivePacket,
        Adapter->ReceivePacketPool);

    if (AllocationStatus != NDIS_STATUS_SUCCESS)
    {
        INITSTR(("Ran out of packet pool\n"));
        return(NULL);
    }

    NDIS_SET_PACKET_HEADER_SIZE(rfdptr->ReceivePacket,
        ETHERNET_HEADER_SIZE);

    // point our buffer for receives at this Rfd
    NdisAllocateBuffer(&AllocationStatus,
        &rfdptr->ReceiveBuffer,
        Adapter->ReceiveBufferPool,
        (PVOID)&hwptr->RfdBuffer.RxMacHeader,
        MAXIMUM_ETHERNET_PACKET_SIZE);

    if (AllocationStatus != NDIS_STATUS_SUCCESS)
    {
        INITSTR(("Ran out of packet buffer pool\n"));
        return(NULL);
    }


    NdisChainBufferAtFront(rfdptr->ReceivePacket,
        rfdptr->ReceiveBuffer);

    // set up the reverse path from Packet to SwRfd
    // this is so when D100GetReturnedPackets is called we
    // can find our software superstructure that owns this receive area.
    TempPtr = (D100SwRfd **) &rfdptr->ReceivePacket->MiniportReserved;
    *TempPtr = rfdptr;

    return(rfdptr);

}

//-----------------------------------------------------------------------------
// Procedure: FreeSwRfd
//
// Description: this function unlinks the SwRfd structure, preparing it
//              to be freed
//
// Arguments: adapter - the adapter structure
//            rfd     - a pointer to the SwRfd to operate on
//
// Returns: nothing but a modified rfd pointer
//
//-----------------------------------------------------------------------------
VOID
FreeSwRfd(D100_ADAPTER *adapter,
          D100SwRfd *rfd)
{
    DEBUGFUNC("FreeSwRfd");

    INITSTR(("rfd %8lX, rfdphys=%8lX\n",rfd->Rfd,rfd->RfdPhys));

    // unchain from packet and free the buffer back to the pool
    NdisFreeBuffer(rfd->ReceiveBuffer);
    // free the packet back to the pool
    NdisFreePacket(rfd->ReceivePacket);

    return;

}

//-----------------------------------------------------------------------------
// Procedure: FreeRMD
//
// Description: This function uninitializes a RMD and frees its memory
//
// Arguments: adapter - the adapter structure pointer
//            rmd     - the RMD to be operated upon
//
// Returns: nothing but a modified and uninitialized RMD pointer
//
//-----------------------------------------------------------------------------
VOID
FreeRMD(D100_ADAPTER *adapter,
        ReceiveMemoryDescriptor *rmd)
{
    DEBUGFUNC("FreeRMD");

    TRACESTR(adapter, ("Freeing %d bytes ExtraRecvCached\n", rmd->CachedMem.Size));
    // free the cached memory
    NdisFreeMemory((PVOID) rmd->CachedMem.VirtualAddress, rmd->CachedMem.Size, 0);
    rmd->CachedMem.VirtualAddress = (ULONG) PtrToUlong(NULL);

    TRACESTR(adapter, ("Freeing %d bytes ExtraRecvUnCached\n", rmd->UnCachedMem.Size));

    // Now free the shared memory that was used for the receive buffers.
    NdisMFreeSharedMemory(
        adapter->D100AdapterHandle,
        rmd->UnCachedMem.Size,
        FALSE,
        (PVOID) rmd->UnCachedMem.VirtualAddress,
        rmd->UnCachedMem.PhysicalAddress);

    rmd->UnCachedMem.VirtualAddress = (ULONG) PtrToUlong(NULL);

    return;

}


