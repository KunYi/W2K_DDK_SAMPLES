//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1998 - 1999
//
//  File:       util.c
//
//--------------------------------------------------------------------------

#include "pch.h"

// PptSucceedRequestNullInfo(Irp)                  - IoStatus.Status=STATUS_SUCCESS, IoStatus.Information=NULL
// PptSucceedRequest(Irp)                          - IoStatus.Status=STATUS_SUCCESS, IoStatus.Information unmodified
// PptSucceedRequestBoostPriority(Irp, Increment)  - IoStatus.Status=STATUS_SUCCESS, IoStatus.Information=NULL
// PptFailRequest(Irp, status)                     - IoStatus.Status=STATUS_SUCCESS, IoStatus.Information=NULL

NTSTATUS
PptFailRequest(PIRP Irp, NTSTATUS Status) {
    Irp->IoStatus.Status      = Status;
    Irp->IoStatus.Information = 0;
    PptCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
PptAcquireRemoveLockOrFailIrp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PDEVICE_EXTENSION extension  = DeviceObject->DeviceExtension;
    PIO_REMOVE_LOCK   removeLock = &extension->RemoveLock;

    NTSTATUS          status     = IoAcquireRemoveLock(removeLock, Irp);

    if( !NT_SUCCESS(status) ) {
        PptFailRequest(Irp, status);
    }

    return status;
}


NTSTATUS
PptDispatchPreProcessIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )
/*++

    - Acquire removelock
    - If(!Special Handling IRP) {
          check if we are running, stalled
         

--*/
{
    PDEVICE_EXTENSION Extension = DeviceObject->DeviceExtension;
    NTSTATUS status = PptAcquireRemoveLock(&Extension->RemoveLock, Irp);
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);


        if ( !NT_SUCCESS( status ) ) {
            //
            // Someone gave us a pnp irp after a remove.  Unthinkable!
            //
            ASSERT(FALSE);
            Irp->IoStatus.Information = 0;
            Irp->IoStatus.Status = status;
            PptCompleteRequest(Irp, IO_NO_INCREMENT);
        }

    return status;
}

NTSTATUS
PptDispatchPostProcessIrp() { return STATUS_SUCCESS; }

NTSTATUS
PptSynchCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PKEVENT Event
    )
/*++
      
Routine Description:
      
    This routine is for use with synchronous IRP processing.
    All it does is signal an event, so the driver knows it
    can continue.
      
Arguments:
      
    DriverObject - Pointer to driver object created by system.
      
    Irp          - Irp that just completed
      
    Event        - Event we'll signal to say Irp is done
      
Return Value:
      
    None.
      
--*/
{
    UNREFERENCED_PARAMETER( Irp );
    UNREFERENCED_PARAMETER( DeviceObject );

    KeSetEvent((PKEVENT) Event, 0, FALSE);
    return (STATUS_MORE_PROCESSING_REQUIRED);
}

PWSTR
PptGetPortNameFromPhysicalDeviceObject(
  PDEVICE_OBJECT PhysicalDeviceObject
)

/*++

Routine Description:

    Retrieve the PortName for the ParPort from the registry. This PortName
      will be used as the symbolic link name for the end of chain device 
      object created by ParClass for this ParPort.

    *** This function allocates pool. ExFreePool must be called when
          result is no longer needed.

Arguments:

    PortDeviceObject - The ParPort Device Object

Return Value:

    PortName - if successful
    NULL     - otherwise

--*/

{
    NTSTATUS                    status;
    HANDLE                      hKey;
    PKEY_VALUE_FULL_INFORMATION buffer;
    ULONG                       bufferLength;
    ULONG                       resultLength;
    PWSTR                       valueNameWstr;
    UNICODE_STRING              valueName;
    PWSTR                       portName;

    PAGED_CODE ();

    //
    // try to open the registry key
    //

    PptDumpV( ("PptGetPortNameFromPhysicalDeviceObject()\n") );

    status = IoOpenDeviceRegistryKey(PhysicalDeviceObject,
                                     PLUGPLAY_REGKEY_DEVICE,
                                     STANDARD_RIGHTS_ALL,
                                     &hKey);

    if( !NT_SUCCESS(status) ) {
        // unable to open key, bail out
        PptDumpV( ("PptGetPortNameFromPhysicalDeviceObject(): "
                   "registry key open failed, status = %x\n", status) );
        return NULL;    
    }

    //
    // we have a handle to the registry key
    //
    // loop trying to read registry value until either we succeed or
    //   we get a hard failure, grow the result buffer as needed
    //

    bufferLength  = 0;          // we will ask how large a buffer we need
    buffer        = NULL;
    valueNameWstr = (PWSTR)L"PortName";
    RtlInitUnicodeString(&valueName, valueNameWstr);
    status        = STATUS_BUFFER_TOO_SMALL;

    while(status == STATUS_BUFFER_TOO_SMALL) {

      status = ZwQueryValueKey(hKey,
                               &valueName,
                               KeyValueFullInformation,
                               buffer,
                               bufferLength,
                               &resultLength);

      if(status == STATUS_BUFFER_TOO_SMALL) {
        // 
        // buffer too small, free it and allocate a larger buffer
        //
        if(buffer) ExFreePool(buffer);
        buffer       = ExAllocatePool(PagedPool, resultLength);
        bufferLength = resultLength;
        if(!buffer) {
          // unable to allocate pool, clean up and exit
            PptDumpV( (" - unable to allocate pool to for PortName query\n") );
          ZwClose(hKey);
          return NULL;
        }
      }

    } // end while BUFFER_TOO_SMALL

    
    //
    // query is complete
    //

    // no longer need the handle so close it
    ZwClose(hKey);

    // check the status of our query
    if( !NT_SUCCESS(status) ) {
        if(buffer) ExFreePool(buffer);
        PptDumpV( (" - query for PortName FAILED - status = %x\n",
                  status) );
        return NULL;
    }

    // sanity check our result
    if( (buffer->Type != REG_SZ) || (!buffer->DataLength) ) {
        PptDumpV( (" - either bogus PortName data type or zero length\n",
                  status) );
        ExFreePool(buffer);       // query succeeded, so we know we have a buffer
        return NULL;
    }
    

    // 
    // result looks ok, copy PortName to its own allocation of the proper size
    //   and return a pointer to it
    //

    portName = ExAllocatePool(PagedPool, buffer->DataLength);
    if(!portName) {
      // unable to allocate pool, clean up and exit
        PptDumpV( (" - unable to allocate pool to hold PortName(SymbolicLinkName)\n") );
        ExFreePool(buffer);
        return NULL;
    }

    RtlCopyMemory(portName, 
                  (PUCHAR)buffer + buffer->DataOffset, 
                  buffer->DataLength);

    ExFreePool(buffer);

    return portName;
}

NTSTATUS
PptConnectInterrupt(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++
      
Routine Description:
      
    This routine connects the port interrupt service routine
      to the interrupt.
      
Arguments:
      
    Extension   - Supplies the device extension.
      
Return Value:
      
    NTSTATUS code.
      
--*/
    
{
    NTSTATUS Status = STATUS_SUCCESS;
    
    if (!Extension->FoundInterrupt) {
        
        return STATUS_NOT_SUPPORTED;
        
    }
    
    //
    // Connect the interrupt.
    //
    
    Status = IoConnectInterrupt(&Extension->InterruptObject,
                                PptInterruptService,
                                Extension,
                                NULL,
                                Extension->InterruptVector,
                                Extension->InterruptLevel,
                                Extension->InterruptLevel,
                                Extension->InterruptMode,
                                TRUE,
                                Extension->InterruptAffinity,
                                FALSE);
    
    if (!NT_SUCCESS(Status)) {
        
        PptLogError(Extension->DeviceObject->DriverObject,
                    Extension->DeviceObject,
                    Extension->PortInfo.OriginalController,
                    PhysicalZero, 0, 0, 0, 14,
                    Status, PAR_INTERRUPT_CONFLICT);
        
        PptDump2(PARERRORS, ("Could not connect to interrupt for %x\n",
                             Extension->PortInfo.OriginalController.LowPart) );
    }
    
    return Status;
}

VOID
PptDisconnectInterrupt(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++
      
Routine Description:
      
    This routine disconnects the port interrupt service routine
      from the interrupt.
      
Arguments:
      
    Extension   - Supplies the device extension.
      
Return Value:
      
    None.
      
--*/
    
{
    IoDisconnectInterrupt(Extension->InterruptObject);
}

BOOLEAN
PptSynchronizedIncrement(
    IN OUT  PVOID   SyncContext
    )

/*++
      
Routine Description:
      
    This routine increments the 'Count' variable in the context and returns
      its new value in the 'NewCount' variable.
      
Arguments:
      
    SyncContext - Supplies the synchronize count context.
      
Return Value:
      
    TRUE
      
--*/
    
{
    ((PSYNCHRONIZED_COUNT_CONTEXT) SyncContext)->NewCount =
        ++(*(((PSYNCHRONIZED_COUNT_CONTEXT) SyncContext)->Count));
    return(TRUE);
}

BOOLEAN
PptSynchronizedDecrement(
    IN OUT  PVOID   SyncContext
    )

/*++
      
Routine Description:
      
    This routine decrements the 'Count' variable in the context and returns
      its new value in the 'NewCount' variable.
      
Arguments:
      
    SyncContext - Supplies the synchronize count context.
      
Return Value:
      
    TRUE
      
--*/
    
{
    ((PSYNCHRONIZED_COUNT_CONTEXT) SyncContext)->NewCount =
        --(*(((PSYNCHRONIZED_COUNT_CONTEXT) SyncContext)->Count));
    return(TRUE);
}

BOOLEAN
PptSynchronizedRead(
    IN OUT  PVOID   SyncContext
    )

/*++
      
Routine Description:
      
    This routine reads the 'Count' variable in the context and returns
      its value in the 'NewCount' variable.
      
Arguments:
      
    SyncContext - Supplies the synchronize count context.
      
Return Value:
      
    None.
      
--*/
    
{
    ((PSYNCHRONIZED_COUNT_CONTEXT) SyncContext)->NewCount =
        *(((PSYNCHRONIZED_COUNT_CONTEXT) SyncContext)->Count);
    return(TRUE);
}

BOOLEAN
PptSynchronizedQueue(
    IN  PVOID   Context
    )

/*++
      
Routine Description:
      
    This routine adds the given list entry to the given list.
      
Arguments:
      
    Context - Supplies the synchronized list context.
      
Return Value:
      
    TRUE
      
--*/
    
{
    PSYNCHRONIZED_LIST_CONTEXT  ListContext;
    
    ListContext = Context;
    InsertTailList(ListContext->List, ListContext->NewEntry);
    return(TRUE);
}

BOOLEAN
PptSynchronizedDisconnect(
    IN  PVOID   Context
    )

/*++
      
Routine Description:
    
    This routine removes the given list entry from the ISR
      list.
      
Arguments:
      
    Context - Supplies the synchronized disconnect context.
      
Return Value:
      
    FALSE   - The given list entry was not removed from the list.
    TRUE    - The given list entry was removed from the list.
      
--*/
    
{
    PSYNCHRONIZED_DISCONNECT_CONTEXT    DisconnectContext;
    PKSERVICE_ROUTINE                   ServiceRoutine;
    PVOID                               ServiceContext;
    PLIST_ENTRY                         Current;
    PISR_LIST_ENTRY                     ListEntry;
    
    DisconnectContext = Context;
    ServiceRoutine = DisconnectContext->IsrInfo->InterruptServiceRoutine;
    ServiceContext = DisconnectContext->IsrInfo->InterruptServiceContext;
    
    for (Current = DisconnectContext->Extension->IsrList.Flink;
         Current != &(DisconnectContext->Extension->IsrList);
         Current = Current->Flink) {
        
        ListEntry = CONTAINING_RECORD(Current, ISR_LIST_ENTRY, ListEntry);
        if (ListEntry->ServiceRoutine == ServiceRoutine &&
            ListEntry->ServiceContext == ServiceContext) {
            
            RemoveEntryList(Current);
            return TRUE;
        }
    }
    
    return FALSE;
}

VOID
PptCancelRoutine(
    IN OUT  PDEVICE_OBJECT  DeviceObject,
    IN OUT  PIRP            Irp
    )

/*++
      
Routine Description:
      
    This routine is called on when the given IRP is cancelled.  It
      will dequeue this IRP off the work queue and complete the
      request as CANCELLED.  If it can't get if off the queue then
      this routine will ignore the CANCEL request since the IRP
      is about to complete anyway.
      
Arguments:
      
    DeviceObject    - Supplies the device object.
      
    Irp             - Supplies the IRP.
      
Return Value:
      
    None.
      
--*/
    
{
    PDEVICE_EXTENSION           Extension;
    SYNCHRONIZED_COUNT_CONTEXT  SyncContext;
    
    PptDump2(PARCANCEL, ("CANCEL IRP %x\n", Irp) );

    Extension = DeviceObject->DeviceExtension;
    
    SyncContext.Count = &Extension->WorkQueueCount;
    
    if (Extension->InterruptRefCount) {
        
        KeSynchronizeExecution(Extension->InterruptObject,
                               PptSynchronizedDecrement,
                               &SyncContext);
    } else {
        PptSynchronizedDecrement(&SyncContext);
    }
    
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);

    IoReleaseCancelSpinLock(Irp->CancelIrql);
    
    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_CANCELLED;

    PptReleaseRemoveLock(&Extension->RemoveLock, Irp);

    PptCompleteRequest(Irp, IO_NO_INCREMENT);
}

VOID
PptFreePortDpc(
    IN      PKDPC   Dpc,
    IN OUT  PVOID   Extension,
    IN      PVOID   SystemArgument1,
    IN      PVOID   SystemArgument2
    )

/*++
      
Routine Description:
      
    This routine is a DPC that will free the port and if necessary
      complete an alloc request that is waiting.
      
Arguments:
      
    Dpc             - Not used.
      
    Extension       - Supplies the device extension.
      
    SystemArgument1 - Not used.
      
    SystemArgument2 - Not used.
      
Return Value:
      
    None.
      
--*/
    
{
    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SystemArgument1 );
    UNREFERENCED_PARAMETER( SystemArgument2 );

    PptFreePort(Extension);
}

BOOLEAN
PptTryAllocatePortAtInterruptLevel(
    IN  PVOID   Context
    )

/*++
      
Routine Description:
      
    This routine is called at interrupt level to quickly allocate
      the parallel port if it is available.  This call will fail
      if the port is not available.
      
Arguments:
      
    Context - Supplies the device extension.
      
Return Value:
      
    FALSE   - The port was not allocated.
    TRUE    - The port was successfully allocated.
     
--*/
    
{
    if (((PDEVICE_EXTENSION) Context)->WorkQueueCount == -1) {
        
        ((PDEVICE_EXTENSION) Context)->WorkQueueCount = 0;
        
        ( (PDEVICE_EXTENSION)Context )->WmiPortAllocFreeCounts.PortAllocates++;

        return(TRUE);
        
    } else {
        
        return(FALSE);
    }
}

VOID
PptFreePortFromInterruptLevel(
    IN  PVOID   Context
    )

/*++
      
Routine Description:
      
    This routine frees the port that was allocated at interrupt level.
      
Arguments:
      
    Context - Supplies the device extension.
      
Return Value:
      
    None.
      
--*/
    
{
    // If no one is waiting for the port then this is simple operation,
    // otherwise queue a DPC to free the port later on.
    
    if (((PDEVICE_EXTENSION) Context)->WorkQueueCount == 0) {
        
        ((PDEVICE_EXTENSION) Context)->WorkQueueCount = -1;
        
    } else {
        
        KeInsertQueueDpc(&((PDEVICE_EXTENSION) Context)->FreePortDpc, NULL, NULL);
    }
}

BOOLEAN
PptInterruptService(
    IN  PKINTERRUPT Interrupt,
    IN  PVOID       Extension
    )

/*++
      
Routine Description:
      
    This routine services the interrupt for the parallel port.
      This routine will call out to all of the interrupt routines
      that connected with this device via
      IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT in order until
      one of them returns TRUE.
      
Arguments:
      
    Interrupt   - Supplies the interrupt object.
      
    Extension   - Supplies the device extension.
      
Return Value:
      
    FALSE   - The interrupt was not handled.
    TRUE    - The interrupt was handled.
      
--*/
    
{
    PLIST_ENTRY         Current;
    PISR_LIST_ENTRY     IsrListEntry;
    PDEVICE_EXTENSION   DeviceExtension;
    
    DeviceExtension = Extension;
    for (Current = DeviceExtension->IsrList.Flink;
         Current != &DeviceExtension->IsrList;
         Current = Current->Flink) {
        
        IsrListEntry = CONTAINING_RECORD(Current, ISR_LIST_ENTRY, ListEntry);
        if (IsrListEntry->ServiceRoutine(Interrupt, IsrListEntry->ServiceContext)) {
            return(TRUE);
        }
    }
    
    return(FALSE);
}

BOOLEAN
PptTryAllocatePort(
    IN  PVOID   Extension
    )

/*++
      
Routine Description:
      
    This routine attempts to allocate the port.  If the port is
      available then the call will succeed with the port allocated.
      If the port is not available the then call will fail
      immediately.
      
Arguments:
      
    Extension   - Supplies the device extension.
      
Return Value:
      
    FALSE   - The port was not allocated.
    TRUE    - The port was allocated.
      
--*/
    
{
    PDEVICE_EXTENSION   DeviceExtension = Extension;
    KIRQL               CancelIrql;
    BOOLEAN             b;
    
    if (DeviceExtension->InterruptRefCount) {
        
        b = KeSynchronizeExecution(DeviceExtension->InterruptObject,
                                   PptTryAllocatePortAtInterruptLevel,
                                   DeviceExtension);
        
    } else {
        
        IoAcquireCancelSpinLock(&CancelIrql);
        b = PptTryAllocatePortAtInterruptLevel(DeviceExtension);
        IoReleaseCancelSpinLock(CancelIrql);
    }
    
    PptDump2(PARDUMP_PORT_ALLOC_FREE,
            ("PptTryAllocatePort %x Allocate Port returned %x.\n", 
            DeviceExtension->PortInfo.Controller, b) );

    return b;
}

BOOLEAN
PptTraversePortCheckList(
    IN  PVOID   Extension
    )

/*++
      
Routine Description:
      
    This routine traverses the deferred port check routines.  This
      call must be synchronized at interrupt level so that real
      interrupts are blocked until these routines are completed.
      
Arguments:
      
    Extension   - Supplies the device extension.
      
Return Value:
      
    FALSE   - The port is in use so no action taken by this routine.
    TRUE    - All of the deferred interrupt routines were called.
      
--*/
    
{
    PDEVICE_EXTENSION   DeviceExtension = Extension;
    PLIST_ENTRY         Current;
    PISR_LIST_ENTRY     CheckEntry;
    
    //
    // First check to make sure that the port is still free.
    //
    if (DeviceExtension->WorkQueueCount >= 0) {
        return FALSE;
    }
    
    for (Current = DeviceExtension->IsrList.Flink;
         Current != &DeviceExtension->IsrList;
         Current = Current->Flink) {
        
        CheckEntry = CONTAINING_RECORD(Current,
                                       ISR_LIST_ENTRY,
                                       ListEntry);
        
        if (CheckEntry->DeferredPortCheckRoutine) {
            CheckEntry->DeferredPortCheckRoutine(CheckEntry->CheckContext);
        }
    }
    
    return TRUE;
}

VOID
PptFreePort(
    IN  PVOID   Extension
    )

/*++
      
Routine Description:
      
    This routine frees the port.
      
Arguments:
      
    Extension   - Supplies the device extension.
      
Return Value:
      
    None.
      
--*/
    
{
    PDEVICE_EXTENSION               DeviceExtension = Extension;
    SYNCHRONIZED_COUNT_CONTEXT      SyncContext;
    KIRQL                           CancelIrql;
    PLIST_ENTRY                     Head;
    PIRP                            Irp;
    PIO_STACK_LOCATION              IrpSp;
    ULONG                           InterruptRefCount;
    PPARALLEL_1284_COMMAND          Command;
    BOOLEAN                         Allocated;

    PptDump2(PARDUMP_PORT_ALLOC_FREE,
            ("PptFreePort %x FREE Port\n", DeviceExtension->PortInfo.Controller) );

    SyncContext.Count = &DeviceExtension->WorkQueueCount;
    
    IoAcquireCancelSpinLock(&CancelIrql);
    if (DeviceExtension->InterruptRefCount) {
        KeSynchronizeExecution(DeviceExtension->InterruptObject,
                               PptSynchronizedDecrement,
                               &SyncContext);
    } else {
        PptSynchronizedDecrement(&SyncContext);
    }
    IoReleaseCancelSpinLock(CancelIrql);

    //
    // Log the free for WMI
    //
    DeviceExtension->WmiPortAllocFreeCounts.PortFrees++;

    //
    // Port is free, check for queued ALLOCATE and/or SELECT requests
    //

    Allocated = FALSE;
    while ( !Allocated && SyncContext.NewCount >= 0 ) {

        //
        // We have ALLOCATE and/or SELECT requests queued,
        //   satisfy the first request
        //
        IoAcquireCancelSpinLock(&CancelIrql);
        Head = RemoveHeadList(&DeviceExtension->WorkQueue);
        Irp = CONTAINING_RECORD(Head, IRP, Tail.Overlay.ListEntry);
        PptSetCancelRoutine(Irp, NULL);

        if ( Irp->Cancel ) {

            Irp->IoStatus.Status = STATUS_CANCELLED;

            // Irp was cancelled so have to get next in line
            SyncContext.Count = &DeviceExtension->WorkQueueCount;
    
            if (DeviceExtension->InterruptRefCount) {
                KeSynchronizeExecution(DeviceExtension->InterruptObject,
                                       PptSynchronizedDecrement,
                                       &SyncContext);
            } else {
                PptSynchronizedDecrement(&SyncContext);
            }

            IoReleaseCancelSpinLock(CancelIrql);

        } else {

            Allocated = TRUE;
            IoReleaseCancelSpinLock(CancelIrql);
        
            // Finding out what kind of IOCTL it was
            IrpSp = IoGetCurrentIrpStackLocation(Irp);
        
            // Check to see if we need to select a 
            if (IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_SELECT_DEVICE ) {

                // request at head of queue was a SELECT
                // so call the select function with the device command saying we already have port

                Command  = (PPARALLEL_1284_COMMAND)Irp->AssociatedIrp.SystemBuffer;
                Command->CommandFlags |= PAR_HAVE_PORT_KEEP_PORT;

                // Call Function to try to select device
                Irp->IoStatus.Status = PptTrySelectDevice( Extension, Command );
            
            } else {
                // request at head of queue was an ALLOCATE
                Irp->IoStatus.Status = STATUS_SUCCESS;
            }
        
            //
            // Note that another Allocate request has been granted for WMI
            //
            DeviceExtension->WmiPortAllocFreeCounts.PortAllocates++;

        }

        // Remove remove lock on Irp and Complete request whether the Irp
        // was cancelled or we acquired the port
        PptReleaseRemoveLock(&DeviceExtension->RemoveLock, Irp);
        if ( Irp->IoStatus.Status != STATUS_CANCELLED ) {
            // only increase if we got the port
            PptCompleteRequest(Irp, IO_PARALLEL_INCREMENT);
        } else {
            PptCompleteRequest(Irp, IO_NO_INCREMENT);
        }

    }
        

    if ( !Allocated ) {
        //
        // ALLOCATE/SELECT request queue was empty
        //
        IoAcquireCancelSpinLock(&CancelIrql);
        InterruptRefCount = DeviceExtension->InterruptRefCount;
        IoReleaseCancelSpinLock(CancelIrql);
        if ( InterruptRefCount ) {
            KeSynchronizeExecution(DeviceExtension->InterruptObject,
                                   PptTraversePortCheckList,
                                   DeviceExtension);
        }
    }

}

ULONG
PptQueryNumWaiters(
    IN  PVOID   Extension
    )

/*++
      
Routine Description:
      
    This routine returns the number of irps queued waiting for
      the parallel port.
      
Arguments:
      
    Extension   - Supplies the device extension.
      
Return Value:
      
    The number of irps queued waiting for the port.
      
--*/
    
{
    PDEVICE_EXTENSION           DeviceExtension = Extension;
    KIRQL                       CancelIrql;
    SYNCHRONIZED_COUNT_CONTEXT  SyncContext;
    
    SyncContext.Count = &DeviceExtension->WorkQueueCount;
    if (DeviceExtension->InterruptRefCount) {
        KeSynchronizeExecution(DeviceExtension->InterruptObject,
                               PptSynchronizedRead,
                               &SyncContext);
    } else {
        IoAcquireCancelSpinLock(&CancelIrql);
        PptSynchronizedRead(&SyncContext);
        IoReleaseCancelSpinLock(CancelIrql);
    }
    
    return((SyncContext.NewCount >= 0) ? ((ULONG) SyncContext.NewCount) : 0);
}

PVOID
PptSetCancelRoutine(PIRP Irp, PDRIVER_CANCEL CancelRoutine)
{
    // #pragma warning( push )
// 4054: 'type cast' : from function pointer to data pointer
// 4055: 'type cast' : from data pointer to function pointer
// 4152:  nonstandard extension, function/data pointer conversion in expression
#pragma warning( disable : 4054 4055 4152 )
    return IoSetCancelRoutine(Irp, CancelRoutine);
    // #pragma warning( pop )
}

BOOLEAN
CheckPort(
    IN  PUCHAR  wPortAddr,
    IN  UCHAR   bMask,
    IN  UCHAR   bValue,
    IN  USHORT  usTimeDelay
    )
/*++

Routine Description:
    This routine will loop for a given time period (actual time is
    passed in as an arguement) and wait for the dsr to match
    predetermined value (dsr value is passed in).

Arguments:
    wPortAddr   - Supplies the base address of the parallel port + some offset.
                  This will have us point directly to the dsr (controller + 1).
    bMask       - Mask used to determine which bits we are looking at
    bValue      - Value we are looking for.
    usTimeDelay - Max time to wait for peripheral response (in us)

Return Value:
    TRUE if a dsr match was found.
    FALSE if the time period expired before a match was found.
--*/

{
    UCHAR  dsr;
    LARGE_INTEGER   Wait;
    LARGE_INTEGER   Start;
    LARGE_INTEGER   End;

    // Do a quick check in case we have one stinkingly fast peripheral!
    dsr = READ_PORT_UCHAR(wPortAddr);
    if ((dsr & bMask) == bValue)
        return TRUE;

    Wait.QuadPart = (usTimeDelay * 10 * 10) + KeQueryTimeIncrement();
    KeQueryTickCount(&Start);

CheckPort_Start:
    KeQueryTickCount(&End);
    dsr = READ_PORT_UCHAR(wPortAddr);
    if ((dsr & bMask) == bValue)
        return TRUE;

    if ((End.QuadPart - Start.QuadPart) * KeQueryTimeIncrement() > Wait.QuadPart)
    {
        // We timed out!!!

        #if DBG
            PptDump2(PARERRORS, ("CheckPort: Timeout\n"));
            PptDump2(PARERRORS, ("<==========================================================\n"));
            {
                int i;

                for (i = 3; i < 8; i++) {
        
                    if ((bMask >> i) & 1) {
                    
                        if (((bValue >> i) & 1) !=  ((dsr >> i) & 1)) {
                        
                            PptDump2(PARERRORS, ("\t\t Bit %d is %d and should be %d!!!\n",
                                        i, (dsr >> i) & 1, (bValue >> i) & 1));
                        }
                    }
                }
            }
            PptDump2(PARERRORS, ("<==========================================================\n"));
        #endif
        goto CheckPort_TimeOut;
    }
    goto CheckPort_Start;

CheckPort_TimeOut:
    return FALSE;    
}

NTSTATUS
PptBuildParallelPortDeviceName(
    IN  ULONG           Number,
    OUT PUNICODE_STRING DeviceName
    )
/*++
      
Routine Description:
      
    Build a Device Name of the form: \Device\ParallelPortN
      
    *** On success this function returns allocated memory that must be freed by the caller

Arguments:
      
    DriverObject          - ParPort driver object
    PhysicalDeviceObject  - PDO whose stack the ParPort FDO will attach to
    DeviceObject          - ParPort FDO
    UniNameString         - the DeviceName (e.g., \Device\ParallelPortN)
    PortName              - the "LPTx" PortName from the devnode
    PortNumber            - the "N" in \Device\ParallelPortN
      
Return Value:
      
    STATUS_SUCCESS on success

    error status otherwise
      
--*/
{
    UNICODE_STRING      uniDeviceString;
    UNICODE_STRING      uniBaseNameString;
    UNICODE_STRING      uniPortNumberString;
    WCHAR               wcPortNum[10];
    NTSTATUS            status;

    PptDump2(PARPNP1, ("util::PptBuildParallelPortDeviceName - Enter - Number=%d\n", Number) );

    //
    // Init strings
    //
    RtlInitUnicodeString( DeviceName, NULL );
    RtlInitUnicodeString( &uniDeviceString, (PWSTR)L"\\Device\\" );
    RtlInitUnicodeString( &uniBaseNameString, (PWSTR)DD_PARALLEL_PORT_BASE_NAME_U );


    //
    // Convert Port Number to UNICODE_STRING
    //
    uniPortNumberString.Length        = 0;
    uniPortNumberString.MaximumLength = sizeof( wcPortNum );
    uniPortNumberString.Buffer        = wcPortNum;

    status = RtlIntegerToUnicodeString( Number, 10, &uniPortNumberString);
    if( !NT_SUCCESS( status ) ) {
        PptDump2(PARERRORS, ("util::PptBuildParallelPortDeviceName - RtlIntegerToUnicodeString FAILED\n") );
        return status;
    }


    //
    // Compute size required and alloc a buffer
    //
    DeviceName->MaximumLength = (USHORT)( uniDeviceString.Length +
                                          uniBaseNameString.Length +
                                          uniPortNumberString.Length +
                                          sizeof(UNICODE_NULL) );

    DeviceName->Buffer = ExAllocatePool( PagedPool, DeviceName->MaximumLength );
    if( NULL == DeviceName->Buffer ) {
        PptDump2(PARERRORS, ("util::PptBuildParallelPortDeviceName - Alloc Buffer FAILED\n") );
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory( DeviceName->Buffer, DeviceName->MaximumLength );


    //
    // Catenate the parts to construct the DeviceName
    //
    RtlAppendUnicodeStringToString(DeviceName, &uniDeviceString);
    RtlAppendUnicodeStringToString(DeviceName, &uniBaseNameString);
    RtlAppendUnicodeStringToString(DeviceName, &uniPortNumberString);

    return STATUS_SUCCESS;
}

NTSTATUS
PptInitializeDeviceExtension(
    IN PDRIVER_OBJECT  DriverObject,
    IN PDEVICE_OBJECT  PhysicalDeviceObject,
    IN PDEVICE_OBJECT  DeviceObject,
    IN PUNICODE_STRING UniNameString,
    IN PWSTR           PortName,
    IN ULONG           PortNumber
    )
/*++
      
Routine Description:
      
    Initialize a ParPort FDO DeviceExtension
      
Arguments:
      
    DriverObject          - ParPort driver object
    PhysicalDeviceObject  - PDO whose stack the ParPort FDO will attach to
    DeviceObject          - ParPort FDO
    UniNameString         - the DeviceName (e.g., \Device\ParallelPortN)
    PortName              - the "LPTx" PortName from the devnode
    PortNumber            - the "N" in \Device\ParallelPortN
      
Return Value:
      
    STATUS_SUCCESS on success

    error status otherwise
      
--*/
{
    PDEVICE_EXTENSION Extension = DeviceObject->DeviceExtension;

    RtlZeroMemory( Extension, sizeof(DEVICE_EXTENSION) );

    //
    // ExtensionSignature helps confirm that we are looking at a Parport DeviceExtension
    //
    Extension->ExtensionSignatureBegin = 0x78877887; 
    Extension->ExtensionSignatureEnd   = 0x87788778; 

    //
    // Standard Info
    //
    Extension->DriverObject         = DriverObject;
    Extension->PhysicalDeviceObject = PhysicalDeviceObject;
    Extension->DeviceObject         = DeviceObject;
    Extension->PnpInfo.PortNumber   = PortNumber; // this is the "N" in \Device\ParallelPortN

    //
    // Mutual Exclusion initialization
    //
    IoInitializeRemoveLock(&Extension->RemoveLock, PARPORT_POOL_TAG, 1, 10);
    ExInitializeFastMutex(&Extension->OpenCloseMutex);
    ExInitializeFastMutex(&Extension->ExtensionFastMutex);

    //
    // chipset detection initialization - redundant, but safer
    //
    Extension->NationalChipFound = FALSE;
    Extension->NationalChecked   = FALSE;

    //
    // Initialize 'WorkQueue' - a Queue for Allocate and Select requests
    //
    InitializeListHead(&Extension->WorkQueue);
    Extension->WorkQueueCount = -1;

    //
    // Initialize Exports - Exported via Internal IOCTLs
    //
    Extension->PortInfo.FreePort            = PptFreePort;
    Extension->PortInfo.TryAllocatePort     = PptTryAllocatePort;
    Extension->PortInfo.QueryNumWaiters     = PptQueryNumWaiters;
    Extension->PortInfo.Context             = Extension;

    Extension->PnpInfo.HardwareCapabilities = PPT_NO_HARDWARE_PRESENT;
    Extension->PnpInfo.TrySetChipMode       = PptSetChipMode;
    Extension->PnpInfo.ClearChipMode        = PptClearChipMode;
    Extension->PnpInfo.TrySelectDevice      = PptTrySelectDevice;
    Extension->PnpInfo.DeselectDevice       = PptDeselectDevice;
    Extension->PnpInfo.Context              = Extension;
    Extension->PnpInfo.PortName             = PortName;

    //
    // List of devices to report in reponse to 
    //   QUERY_DEVICE_RELATIONS/RemovalRelations
    //
    InitializeListHead( &Extension->RemovalRelationsList );

    //
    // Empty list of interrupt service routines, interrupt NOT connected
    //
    InitializeListHead( &Extension->IsrList );
    Extension->InterruptObject   = NULL;
    Extension->InterruptRefCount = 0;

    //
    // Initialize the free port DPC.
    //
    KeInitializeDpc( &Extension->FreePortDpc, PptFreePortDpc, Extension );

    //
    // Save Device Name in our extension
    //
    {
        ULONG bufferLength = UniNameString->MaximumLength + sizeof(UNICODE_NULL);
        Extension->DeviceName.Buffer = ExAllocatePool(NonPagedPool, bufferLength);
        if( !Extension->DeviceName.Buffer ) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        RtlZeroMemory( Extension->DeviceName.Buffer, bufferLength );
        Extension->DeviceName.Length        = 0;
        Extension->DeviceName.MaximumLength = UniNameString->MaximumLength;
        RtlCopyUnicodeString( &Extension->DeviceName, UniNameString );
    }

    //
    // Port is in default mode and mode has not been set 
    //   by a lower filter driver
    //
    Extension->PnpInfo.CurrentMode  = INITIAL_MODE;
    Extension->FilterMode           = FALSE;

    return STATUS_SUCCESS;
}

NTSTATUS
PptGetPortNumberFromLptName( 
    IN  PWSTR  PortName, 
    OUT PULONG PortNumber 
    )
/*++
      
Routine Description:
      
    Verify that the LptName is of the form LPTn, if so then return
    the integer value of n
      
Arguments:
      
    PortName   - the PortName extracted from the devnode - expected to be 
                   of the form: "LPTn"

    PortNumber - points to the UNLONG that will hold the result on success
      
Return Value:
      
    STATUS_SUCCESS on success - *PortNumber will contain the integer value of n

    error status otherwise
      
--*/
{
    NTSTATUS       status;
    UNICODE_STRING str;

    //
    // Verify that the PortName looks like LPTx where x is a number
    //

    if( PortName[0] != L'L' || PortName[1] != L'P' || PortName[2] != L'T' ) {
        PptDump2(PARPNP1, ("util::PptGetPortNumberFromLptName - name prefix doesn't look like LPT\n"));
        return STATUS_UNSUCCESSFUL;
    }

    //
    // prefix is LPT, check for integer suffix with value > 0
    //
    RtlInitUnicodeString( &str, (PWSTR)&PortName[3] );

    status = RtlUnicodeStringToInteger( &str, 10, PortNumber );

    if( !NT_SUCCESS( status ) ) {
        PptDump2(PARPNP1, ("util::PptGetPortNumberFromLptName - name suffix doesn't look like an integer\n"));
        return STATUS_UNSUCCESSFUL;
    }

    if( *PortNumber == 0 ) {
        PptDump2(PARPNP1, ("util::PptGetPortNumberFromLptName - name suffix == 0 - FAIL - Invalid value\n"));
        return STATUS_UNSUCCESSFUL;
    }

    PptDump2(PARPNP1, ("util::PptGetPortNumberFromLptName - LPT name suffix= %d\n", *PortNumber));

    return STATUS_SUCCESS;
}

PDEVICE_OBJECT
PptBuildDeviceObject( 
    IN PDRIVER_OBJECT DriverObject, 
    IN PDEVICE_OBJECT PhysicalDeviceObject 
    )
/*++
      
Routine Description:
      
    This routine constructs and initializes a parport FDO
      
Arguments:
      
    DriverObject         - Pointer to the parport driver object
    PhysicalDeviceObject - Pointer to the PDO whose stack we will attach to
      
Return Value:
      
    Pointer to the new ParPort Device Object on Success

    NULL otherwise
      
--*/
{
    UNICODE_STRING      uniNameString = {0,0,0};
    ULONG               portNumber    = 0;
    PWSTR               portName      = NULL;
    NTSTATUS            status        = STATUS_SUCCESS;
    PDEVICE_OBJECT      deviceObject = NULL;

    PptDump2(PARPNP1, ("util::PptBuildDeviceObject - Enter\n"));

    //
    // Get the LPTx name for this port from the registry.
    //
    // The initial LPTx name for a port is determined by the ports class installer 
    //   msports.dll, but the name can subsequently be changed by the user via
    //   a device manager property page.
    //
    portName = PptGetPortNameFromPhysicalDeviceObject( PhysicalDeviceObject );
    if( NULL == portName ) {
        PptDump2(PARERRORS, ("util::PptBuildDeviceObject - get LPTx Name from registry - FAILED\n") );
        goto targetExit;
    }

    PptDump2(PARPNP1, ("util::PptBuildDeviceObject - portName = <%S>\n", portName));

    //
    // Extract the preferred port number N to use for the \Device\ParallelPortN 
    //   DeviceName from the LPTx name
    //
    // Preferred DeviceName for LPT(n) is ParallelPort(n-1) - e.g., LPT3 -> ParallelPort2
    //
    status = PptGetPortNumberFromLptName( portName, &portNumber );
    if( !NT_SUCCESS( status ) ) {
        PptDump2(PARERRORS, ("util::PptBuildDeviceObject - extract portNumber from LPTx Name - FAILED\n") );
        ExFreePool( portName );
        goto targetExit;
    }
    --portNumber;               // convert 1 (LPT) based number to 0 (ParallelPort) based number
    PptDump2(PARPNP1, ("util::PptBuildDeviceObject - portNumber (after decrement) = %d\n", portNumber));

    //
    // Build a DeviceName of the form: \Device\ParallelPortN
    //
    status = PptBuildParallelPortDeviceName(portNumber, &uniNameString);
    if( !NT_SUCCESS( status ) ) {
        // we couldn't make up a name - bail out
        PptDump2(PARERRORS, ("util::PptBuildDeviceObject - Build ParallelPort DeviceName - FAILED\n") );
        ExFreePool( portName );
        goto targetExit;
    }

    PptDump2(PARPNP1, ("util::PptBuildDeviceObject - uniNameString = <%wZ>\n", &uniNameString));

    //
    // Create the device object for this device.
    //
    status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), &uniNameString, 
                            FILE_DEVICE_PARALLEL_PORT, FILE_DEVICE_SECURE_OPEN, FALSE, &deviceObject);

    
    if( STATUS_OBJECT_NAME_COLLISION == status ) {
        //
        // Preferred DeviceName already exists - try made up names
        // 

        PptDump2(PARPNP1, ("util::PptBuildDeviceObject - Initial Device Creation FAILED - Name Collision\n"));

        //
        // use an offset so that our made up names won't collide with 
        //   the preferred names of ports that have not yet started
        //   (start with ParallelPort8)
        //
        #define PPT_CLASSNAME_OFFSET 7
        portNumber = PPT_CLASSNAME_OFFSET;

        do {
            RtlFreeUnicodeString( &uniNameString );
            ++portNumber;
            status = PptBuildParallelPortDeviceName(portNumber, &uniNameString);
            if( !NT_SUCCESS( status ) ) {
                // we couldn't make up a name - bail out
                PptDump2(PARERRORS, ("util::PptBuildDeviceObject - Build ParallelPort DeviceName - FAILED\n") );
                ExFreePool( portName );
                goto targetExit;
            }
            PptDump2(PARPNP1, ("util::PptBuildDeviceObject - Trying Device Creation <%wZ>\n", &uniNameString));
            status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), &uniNameString, 
                                    FILE_DEVICE_PARALLEL_PORT, FILE_DEVICE_SECURE_OPEN, FALSE, &deviceObject);

        } while( STATUS_OBJECT_NAME_COLLISION == status );
    }

    if( !NT_SUCCESS( status ) ) {
        // we got a failure other than a name collision - bail out
        PptDump2(PARPNP1, ("util::PptBuildDeviceObject - Device Creation FAILED - status=%x\n", status));
        deviceObject = NULL;
        ExFreePool( portName );
        goto targetExit;
    }

    //
    // We have a deviceObject - Initialize DeviceExtension
    //
    status = PptInitializeDeviceExtension( DriverObject, PhysicalDeviceObject, deviceObject, 
                                           &uniNameString, portName, portNumber );
    if( !NT_SUCCESS( status ) ) {
        // failure initializing the device extension - clean up and bail out
        PptDump2(PARPNP1, ("util::PptBuildDeviceObject - Device Initialization FAILED - status=%x\n", status));
        IoDeleteDevice( deviceObject );
        deviceObject = NULL;
        ExFreePool( portName );
        goto targetExit;
    }

    //
    // Propagate the power pagable flag of the PDO to our new FDO
    //
    if( PhysicalDeviceObject->Flags & DO_POWER_PAGABLE ) {
        deviceObject->Flags |= DO_POWER_PAGABLE;
    }

    PptDump2(PARPNP1, ("util::PptBuildDeviceObject - SUCCESS - deviceObject= %x\n", deviceObject));

targetExit:

    RtlFreeUnicodeString( &uniNameString );

    return deviceObject;
}

