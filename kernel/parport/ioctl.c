//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1998 - 1999
//
//  File:       ioctl.c
//
//--------------------------------------------------------------------------

#include "pch.h"

VOID
PptCleanRemovalRelationsList(PDEVICE_EXTENSION Extension) 
//
// Clean up any remaining list pool allocations.
//
// Called from IRP_MN_REMOVE handler
//
{
    PLIST_ENTRY                   listHead      = &Extension->RemovalRelationsList;
    PLIST_ENTRY                   thisListEntry = NULL;
    PREMOVAL_RELATIONS_LIST_ENTRY node          = NULL;

    ExAcquireFastMutex( &Extension->ExtensionFastMutex );

    while( !IsListEmpty( listHead ) ) {
        thisListEntry = RemoveHeadList( listHead );
        node = CONTAINING_RECORD( thisListEntry, REMOVAL_RELATIONS_LIST_ENTRY, ListEntry );
        PptDump2(PARPNP1, ("ioctl::PptCleanRemovalRelationsList - deleting entry for node=%x devobj=%x %wZ\n",
                               node, node->DeviceObject, &node->DeviceName));
        RtlFreeUnicodeString( &node->DeviceName );
        ExFreePool( node );
    }

    ExReleaseFastMutex( &Extension->ExtensionFastMutex );

    return;
}

NTSTATUS
PptAddPptRemovalRelation(PDEVICE_EXTENSION Extension, PPARPORT_REMOVAL_RELATIONS PptRemovalRelations) 
{
    PREMOVAL_RELATIONS_LIST_ENTRY node = ExAllocatePool(PagedPool, sizeof(REMOVAL_RELATIONS_LIST_ENTRY));
    PptDump2(PARPNP1, ("ioctl::PptAddPptRemovalRelation - DeviceObject= %x %wZ\n", 
                       PptRemovalRelations->DeviceObject, PptRemovalRelations->DeviceName));

    if( !node ) {
        PptDump2(PARPNP1, ("ioctl::PptAddPptRemovalRelation - can't get storage for node\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // initialize node
    node->DeviceObject = PptRemovalRelations->DeviceObject;
    node->Flags        = PptRemovalRelations->Flags;
    RtlZeroMemory( &node->DeviceName, sizeof(UNICODE_STRING) );
    node->DeviceName.Buffer = ExAllocatePool(PagedPool, PptRemovalRelations->DeviceName->MaximumLength);
    if( !(node->DeviceName.Buffer) ) {
        PptDump2(PARPNP1, ("ioctl::PptAddPptRemovalRelation - can't get storage for UNICODE_STRING\n"));
        ExFreePool( node );
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    node->DeviceName.MaximumLength = PptRemovalRelations->DeviceName->MaximumLength;
    RtlCopyUnicodeString( &node->DeviceName, PptRemovalRelations->DeviceName);

    // add to list
    ExAcquireFastMutex( &Extension->ExtensionFastMutex );
    InsertTailList( &Extension->RemovalRelationsList, &node->ListEntry );
    ExReleaseFastMutex( &Extension->ExtensionFastMutex );
    return STATUS_SUCCESS;
}

NTSTATUS
PptRemovePptRemovalRelation(
    IN PDEVICE_EXTENSION          Extension, 
    IN PPARPORT_REMOVAL_RELATIONS PptRemovalRelations
)
/*++

  Search the list for the element specified. If found, remove it 
    from the list and free pool associated with the entry.

--*/
{
    PDEVICE_OBJECT                callerDevObj   = PptRemovalRelations->DeviceObject;
    PLIST_ENTRY                   listHead       = &Extension->RemovalRelationsList;
    PDEVICE_OBJECT                listDevObj     = NULL;
    PLIST_ENTRY                   thisListEntry  = NULL;
    PLIST_ENTRY                   firstListEntry = NULL;
    BOOLEAN                       found          = FALSE;
    BOOLEAN                       done           = FALSE;
    PREMOVAL_RELATIONS_LIST_ENTRY node           = NULL;

    PptDump2(PARPNP1, ("ioctl::PptRemovePptRemovalRelation - Entry - DeviceObject= %x %wZ\n", 
                       PptRemovalRelations->DeviceObject, PptRemovalRelations->DeviceName) );
    
    ExAcquireFastMutex( &Extension->ExtensionFastMutex );
    if( IsListEmpty( listHead ) ) {
        PptDump2(PARPNP1, ("ioctl::PptRemovePptRemovalRelation - Empty List\n"));
        ExReleaseFastMutex( &Extension->ExtensionFastMutex );
        return STATUS_SUCCESS;
    } else {
        PptDump2(PARPNP1, ("ioctl::PptRemovePptRemovalRelation - Nonempty List - begin search\n"));
        ExReleaseFastMutex( &Extension->ExtensionFastMutex );    
    }

    ExAcquireFastMutex( &Extension->ExtensionFastMutex );
    while( !done ) {

        thisListEntry = RemoveHeadList( listHead );
        node = CONTAINING_RECORD( thisListEntry, REMOVAL_RELATIONS_LIST_ENTRY, ListEntry );

        if( node->DeviceObject == callerDevObj ) {
            // found it
            PptDump2(PARPNP1, ("ioctl::PptRemovePptRemovalRelation - Found device= %x %wZ\n",
                               node->DeviceObject, &node->DeviceName));
            found = TRUE;
            done  = TRUE;
        } else if( firstListEntry == thisListEntry ) {
            // searched entire list and didn't find it
            PptDump2(PARPNP1, ("ioctl::PptRemovePptRemovalRelation - Not Found\n"));
            done = TRUE;
        } else if( !firstListEntry ) {
            // this is first element in list - save as marker for loop termination
            PptDump2(PARPNP1, ("ioctl::PptRemovePptRemovalRelation - Not Found - First Entry\n"));
            firstListEntry = thisListEntry;
        }

        if( !found ) {
            // return current element to end of list
            // PptDump2(PARPNP1, ("ioctl::PptRemovePptRemovalRelation - InsertTailList\n"));
            InsertTailList( listHead, &node->ListEntry );
        }
    }
    ExReleaseFastMutex( &Extension->ExtensionFastMutex );

    if( found ) {
        // found the matching entry - clean up allocations
        // PptDump2(PARPNP1, ("ioctl::PptRemovePptRemovalRelation - Found - freeing pool\n"));
        RtlFreeUnicodeString( &node->DeviceName );
        ExFreePool( node );
    }          

    PptDump2(PARPNP1, ("ioctl::PptRemovePptRemovalRelation - Returning\n"));
    return STATUS_SUCCESS; // always
}

VOID
PptDumpRemovalRelationsList(PDEVICE_EXTENSION Extension) 
{
    PLIST_ENTRY                   listHead       = &Extension->RemovalRelationsList;
    PLIST_ENTRY                   thisListEntry  = NULL;
    PLIST_ENTRY                   firstListEntry = NULL;
    BOOLEAN                       done           = FALSE;
    PREMOVAL_RELATIONS_LIST_ENTRY node           = NULL;

    PptDump2(PARPNP1, ("ioctl::PptDumpRemovalRelationsList\n"));

    
    ExAcquireFastMutex( &Extension->ExtensionFastMutex );
    if( IsListEmpty( listHead ) ) {
        PptDump2(PARPNP1, ("ioctl::PptDumpRemovalRelationsList - Empty List\n"));
        ExReleaseFastMutex( &Extension->ExtensionFastMutex );
        return;
    } else {
        PptDump2(PARPNP1, ("ioctl::PptDumpRemovalRelationsList - Nonempty List - begin scan/dump\n"));
    }

    while( !done ) {

        thisListEntry = RemoveHeadList( listHead );
        node = CONTAINING_RECORD( thisListEntry, REMOVAL_RELATIONS_LIST_ENTRY, ListEntry );

        if( firstListEntry == thisListEntry ) {
            // done - push back onto front of list
            InsertHeadList( listHead, &node->ListEntry );            
            done = TRUE;
            PptDump2(PARPNP1, ("ioctl::PptDumpRemovalRelationsList - Done\n"));
        } else {
            // dump node info
            PptDump2(PARPNP1, ("ioctl::PptDumpRemovalRelationsList - entry= %x %wZ\n", node->DeviceObject, &node->DeviceName));
            InsertTailList( listHead, &node->ListEntry );
        }

        if( !firstListEntry ) {
            // save first element - use for loop termination
            firstListEntry = thisListEntry;
        }
    }
    ExReleaseFastMutex( &Extension->ExtensionFastMutex );
    return;
}

VOID
PptDumpPptRemovalRelationsStruct(PPARPORT_REMOVAL_RELATIONS PptRemovalRelations) 
{
#if DBG
    ASSERT( PptRemovalRelations );
    ASSERT( PptRemovalRelations->DeviceName );
    PptDump2(PARPNP1, ("ioctl::PptDumpPptRemovalRelationsStruct - DevObj  = %x\n", PptRemovalRelations->DeviceObject));
    PptDump2(PARPNP1, ("ioctl::PptDumpPptRemovalRelationsStruct - DevName = %wZ\n", PptRemovalRelations->DeviceName));
#else
    UNREFERENCED_PARAMETER( PptRemovalRelations );
#endif
    return;
}

NTSTATUS
PptDispatchInternalDeviceControl(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

/*++
      
Routine Description:
      
    This routine is the dispatch routine for IRP_MJ_INTERNAL_DEVICE_CONTROL.
      
Arguments:
      
    DeviceObject    - Supplies the device object.
      
    Irp             - Supplies the I/O request packet.
      
Return Value:
      
    STATUS_SUCCESS              - Success.
    STATUS_UNSUCCESSFUL         - The request was unsuccessful.
    STATUS_PENDING              - The request is pending.
    STATUS_INVALID_PARAMETER    - Invalid parameter.
    STATUS_CANCELLED            - The request was cancelled.
    STATUS_BUFFER_TOO_SMALL     - The supplied buffer is too small.
    STATUS_INVALID_DEVICE_STATE - The current chip mode is invalid to change to asked mode
    
--*/
    
{
    PIO_STACK_LOCATION                  IrpSp;
    PDEVICE_EXTENSION                   Extension = DeviceObject->DeviceExtension;
    NTSTATUS                            Status;
    PPARALLEL_PORT_INFORMATION          PortInfo;
    PPARALLEL_PNP_INFORMATION           PnpInfo;
    PMORE_PARALLEL_PORT_INFORMATION     MorePortInfo;
    KIRQL                               CancelIrql;
    SYNCHRONIZED_COUNT_CONTEXT          SyncContext;
    PPARALLEL_INTERRUPT_SERVICE_ROUTINE IsrInfo;
    PPARALLEL_INTERRUPT_INFORMATION     InterruptInfo;
    PISR_LIST_ENTRY                     IsrListEntry;
    SYNCHRONIZED_LIST_CONTEXT           ListContext;
    SYNCHRONIZED_DISCONNECT_CONTEXT     DisconnectContext;
    BOOLEAN                             DisconnectInterrupt;


    PptDump2(PARENTRY, ("ioctl::PptDispatchInternalDeviceControl - Irp= %x , UserEvent= %x\n", Irp, Irp->UserEvent));
    if(Irp->UserEvent) {
        ASSERT_EVENT(Irp->UserEvent);
    }

    //
    // Verify that our device has not been SUPRISE_REMOVED. Generally
    //   only parallel ports on hot-plug busses (e.g., PCMCIA) and
    //   parallel ports in docking stations will be surprise removed.
    //
    // dvdf - RMT - It would probably be a good idea to also check
    //   here if we are in a "paused" state (stop-pending, stopped, or
    //   remove-pending) and queue the request until we either return to
    //   a fully functional state or are removed.
    //
    if( Extension->DeviceStateFlags & PPT_DEVICE_SURPRISE_REMOVED ) {
        PptDumpV( ("IRP_MJ_CREATE - FAIL - %S, DEVICE_SURPRISE_REMOVED\n", Extension->PnpInfo.PortName) );
        return PptFailRequest( Irp, STATUS_DELETE_PENDING );
    }


    //
    // Try to acquire RemoveLock to prevent the device object from going
    //   away while we're using it.
    //
    Status = PptAcquireRemoveLockOrFailIrp( DeviceObject, Irp );
    if ( !NT_SUCCESS(Status) ) {
        PptDumpV( ("IRP_MJ_CREATE - FAIL - unable to acquire RemoveLock\n") );
        return Status;
    }

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    
    Irp->IoStatus.Information = 0;
    

    switch (IrpSp->Parameters.DeviceIoControl.IoControlCode) {
        
    case IOCTL_INTERNAL_REGISTER_FOR_REMOVAL_RELATIONS:

        PptDump2(PARPNP1, ("ioctl:: - RegisterForRemovalRelations - Enter\n") );

        if( IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(PARPORT_REMOVAL_RELATIONS) ) {
            PptDump2(PARPNP1, ("ioctl:: - RegisterForRemovalRelations - BUFFER_TOO_SMALL\n") );
            Status = STATUS_BUFFER_TOO_SMALL;
        } else {
            PPARPORT_REMOVAL_RELATIONS removalRelations = Irp->AssociatedIrp.SystemBuffer;
            PptDumpPptRemovalRelationsStruct( removalRelations );
            PptDumpRemovalRelationsList(Extension);
            PptAddPptRemovalRelation(Extension, removalRelations);
            PptDumpRemovalRelationsList(Extension);
            PptDump2(PARPNP1, ("ioctl:: - RegisterForRemovalRelations - SUCCESS\n") );
            Status = STATUS_SUCCESS;
        }

        Irp->IoStatus.Status = Status;
        PptReleaseRemoveLock(&Extension->RemoveLock, Irp);
        PptCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;

    case IOCTL_INTERNAL_UNREGISTER_FOR_REMOVAL_RELATIONS:

        PptDump2(PARPNP1, ("ioctl:: - UnregisterForRemovalRelations - Enter\n") );
        
        if( IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(PARPORT_REMOVAL_RELATIONS) ) {
            PptDump2(PARPNP1, ("ioctl:: - RegisterForRemovalRelations - BUFFER_TOO_SMALL\n") );
            Status = STATUS_BUFFER_TOO_SMALL;
        } else {
            PPARPORT_REMOVAL_RELATIONS removalRelations = Irp->AssociatedIrp.SystemBuffer;
            PptDumpPptRemovalRelationsStruct(Irp->AssociatedIrp.SystemBuffer);
            PptDumpRemovalRelationsList(Extension);
            PptRemovePptRemovalRelation(Extension, removalRelations);
            PptDumpRemovalRelationsList(Extension);
            PptDump2(PARPNP1, ("ioctl:: - RegisterForRemovalRelations - SUCCESS\n") );
            Status = STATUS_SUCCESS;
        }

        Irp->IoStatus.Status = Status;
        PptReleaseRemoveLock(&Extension->RemoveLock, Irp);
        PptCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;

    case IOCTL_INTERNAL_PARALLEL_PORT_FREE:

        PptFreePort(Extension);

        Irp->IoStatus.Status = STATUS_SUCCESS;
        PptReleaseRemoveLock(&Extension->RemoveLock, Irp);
        PptCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;

    case IOCTL_INTERNAL_PARALLEL_PORT_ALLOCATE:
        
        IoAcquireCancelSpinLock(&CancelIrql);
        
        if (Irp->Cancel) {
            
            Status = STATUS_CANCELLED;
            
        } else {
            
            SyncContext.Count = &Extension->WorkQueueCount;
            
            if (Extension->InterruptRefCount) {
                
                KeSynchronizeExecution(Extension->InterruptObject,
                                       PptSynchronizedIncrement,
                                       &SyncContext);
            } else {
                
                PptSynchronizedIncrement(&SyncContext);
            }
            
            if (SyncContext.NewCount) {
                
                // someone else currently has the port, queue request
                PptSetCancelRoutine(Irp, PptCancelRoutine);
                IoMarkIrpPending(Irp);
                InsertTailList(&Extension->WorkQueue,
                               &Irp->Tail.Overlay.ListEntry);
                Status = STATUS_PENDING;

            } else {
                // port aquired
                Extension->WmiPortAllocFreeCounts.PortAllocates++;
                Status = STATUS_SUCCESS;
            }
        }
        
        IoReleaseCancelSpinLock(CancelIrql);

#if DBG
        // debug print statements moved here (following release of spin lock) 
        //   because NT conversion tables used to convert UNICODE to 
        //   ANSI for debug printing are paged and thus DbgPrint's bugcheck at
        //   IRQL > APC level
        if(Status == STATUS_SUCCESS) {
            PptDump2(PARDUMP_PORT_ALLOC_FREE, 
                     ("%wZ %S ALLOCATE\n", &Extension->DeviceName, Extension->PnpInfo.PortName) );
        } else if(Status == STATUS_PENDING) {
            PptDump2(PARDUMP_PORT_ALLOC_FREE,
                     ("%wZ %S Allocate request queued\n", &Extension->DeviceName, Extension->PnpInfo.PortName) );
        }
#endif

        break;
        
    case IOCTL_INTERNAL_GET_PARALLEL_PORT_INFO:
        
        PptDump2(PARIOCTL, ("PptDispatchDeviceControl(): IOCTL_INTERNAL_GET_PARALLEL_PORT_INFO\n") );
        
        if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(PARALLEL_PORT_INFORMATION)) {
            
            Status = STATUS_BUFFER_TOO_SMALL;
            
        } else {
            
            Irp->IoStatus.Information = sizeof(PARALLEL_PORT_INFORMATION);
            PortInfo = Irp->AssociatedIrp.SystemBuffer;
            *PortInfo = Extension->PortInfo;
            Status = STATUS_SUCCESS;
        }
        break;
        
    case IOCTL_INTERNAL_RELEASE_PARALLEL_PORT_INFO:
        
        PptDump2(PARIOCTL, ("PptDispatchDeviceControl(): IOCTL_INTERNAL_RELEASE_PARALLEL_PORT_INFO\n") );
        Status = STATUS_SUCCESS;
        break;
        
    case IOCTL_INTERNAL_GET_PARALLEL_PNP_INFO:
        
        PptDump2(PARIOCTL, ("PptDispatchDeviceControl(): IOCTL_INTERNAL_GET_PARALLEL_PNP_INFO\n") );

        if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(PARALLEL_PNP_INFORMATION)) {
            
            Status = STATUS_BUFFER_TOO_SMALL;
            
        } else {
            
            Irp->IoStatus.Information = sizeof(PARALLEL_PNP_INFORMATION);
            PnpInfo  = Irp->AssociatedIrp.SystemBuffer;
            *PnpInfo = Extension->PnpInfo;
            
            Status = STATUS_SUCCESS;
        }
        break;
        
    case IOCTL_INTERNAL_GET_MORE_PARALLEL_PORT_INFO:
        
        PptDump2(PARIOCTL, ("PptDispatchDeviceControl(): IOCTL_INTERNAL_GET_MORE_PARALLEL_PORT_INFO\n") );
        
        if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(MORE_PARALLEL_PORT_INFORMATION)) {
            
            Status = STATUS_BUFFER_TOO_SMALL;
            
        } else {
            
            Irp->IoStatus.Information = sizeof(MORE_PARALLEL_PORT_INFORMATION);
            MorePortInfo = Irp->AssociatedIrp.SystemBuffer;
            MorePortInfo->InterfaceType = Extension->InterfaceType;
            MorePortInfo->BusNumber = Extension->BusNumber;
            MorePortInfo->InterruptLevel = Extension->InterruptLevel;
            MorePortInfo->InterruptVector = Extension->InterruptVector;
            MorePortInfo->InterruptAffinity = Extension->InterruptAffinity;
            MorePortInfo->InterruptMode = Extension->InterruptMode;
            Status = STATUS_SUCCESS;
        }
        break;
        
    case IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT:
        
        PptDump2(PARIOCTL, ("PptDispatchDeviceControl(): IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT\n") );
        
        {
            //
            // Verify that this interface has been explicitly enabled via the registry flag, otherwise
            //   FAIL the request with STATUS_UNSUCCESSFUL
            //
            ULONG EnableConnectInterruptIoctl = 0;
            PptRegGetDeviceParameterDword( Extension->PhysicalDeviceObject, 
                                           (PWSTR)L"EnableConnectInterruptIoctl", 
                                           &EnableConnectInterruptIoctl );
            PptDump2(PARIOCTL, ("ioctl::PptDispatchDeviceControl: IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT"
                                " - EnableConnectInterruptIoctl = %x\n", EnableConnectInterruptIoctl) );
            if( 0 == EnableConnectInterruptIoctl ) {
                PptDump2(PARIOCTL, ("ioctl::PptDispatchDeviceControl: IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT - DISABLED\n") );
                Status = STATUS_UNSUCCESSFUL;
                goto targetExit;
            } else {
                PptDump2(PARIOCTL, ("ioctl::PptDispatchDeviceControl: IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT - ENABLED\n") );
            }
        }


        //
        // This interface has been explicitly enabled via the registry flag, process request.
        //

        if (IrpSp->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(PARALLEL_INTERRUPT_SERVICE_ROUTINE) ||
            IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(PARALLEL_INTERRUPT_INFORMATION)) {
            
            Status = STATUS_BUFFER_TOO_SMALL;
            
        } else {
            
            IsrInfo = Irp->AssociatedIrp.SystemBuffer;
            InterruptInfo = Irp->AssociatedIrp.SystemBuffer;
            IoAcquireCancelSpinLock(&CancelIrql);
            
            if (Extension->InterruptRefCount) {
                
                ++Extension->InterruptRefCount;
                IoReleaseCancelSpinLock(CancelIrql);
                Status = STATUS_SUCCESS;
                
            } else {
                
                IoReleaseCancelSpinLock(CancelIrql);
                Status = PptConnectInterrupt(Extension);
                if (NT_SUCCESS(Status)) {
                    IoAcquireCancelSpinLock(&CancelIrql);
                    ++Extension->InterruptRefCount;
                    IoReleaseCancelSpinLock(CancelIrql);
                }
            }
            
            if (NT_SUCCESS(Status)) {
                
                IsrListEntry = ExAllocatePool(NonPagedPool,
                                              sizeof(ISR_LIST_ENTRY));
                
                if (IsrListEntry) {
                    
                    IsrListEntry->ServiceRoutine =
                        IsrInfo->InterruptServiceRoutine;
                    IsrListEntry->ServiceContext =
                        IsrInfo->InterruptServiceContext;
                    IsrListEntry->DeferredPortCheckRoutine =
                        IsrInfo->DeferredPortCheckRoutine;
                    IsrListEntry->CheckContext =
                        IsrInfo->DeferredPortCheckContext;
                    
                    // Put the ISR_LIST_ENTRY onto the ISR list.
                    
                    ListContext.List = &Extension->IsrList;
                    ListContext.NewEntry = &IsrListEntry->ListEntry;
                    KeSynchronizeExecution(Extension->InterruptObject,
                                           PptSynchronizedQueue,
                                           &ListContext);
                    
                    InterruptInfo->InterruptObject =
                        Extension->InterruptObject;
                    InterruptInfo->TryAllocatePortAtInterruptLevel =
                        PptTryAllocatePortAtInterruptLevel;
                    InterruptInfo->FreePortFromInterruptLevel =
                        PptFreePortFromInterruptLevel;
                    InterruptInfo->Context =
                        Extension;
                    
                    Irp->IoStatus.Information =
                        sizeof(PARALLEL_INTERRUPT_INFORMATION);
                    Status = STATUS_SUCCESS;
                    
                } else {
                    
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                }
            }
        }
        break;
        
    case IOCTL_INTERNAL_PARALLEL_DISCONNECT_INTERRUPT:
        
        PptDump2(PARIOCTL, ("PptDispatchDeviceControl(): IOCTL_INTERNAL_PARALLEL_DISCONNECT_INTERRUPT\n") );
        
        if (IrpSp->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(PARALLEL_INTERRUPT_SERVICE_ROUTINE)) {
            
            Status = STATUS_BUFFER_TOO_SMALL;
            
        } else {
            
            IsrInfo = Irp->AssociatedIrp.SystemBuffer;
            
            // Take the ISR out of the ISR list.
            
            IoAcquireCancelSpinLock(&CancelIrql);
            
            if (Extension->InterruptRefCount) {
                
                IoReleaseCancelSpinLock(CancelIrql);
                
                DisconnectContext.Extension = Extension;
                DisconnectContext.IsrInfo = IsrInfo;
                
                if (KeSynchronizeExecution(Extension->InterruptObject,
                                           PptSynchronizedDisconnect,
                                           &DisconnectContext)) {
                    
                    Status = STATUS_SUCCESS;
                    IoAcquireCancelSpinLock(&CancelIrql);
                    
                    if (--Extension->InterruptRefCount == 0) {
                        DisconnectInterrupt = TRUE;
                    } else {
                        DisconnectInterrupt = FALSE;
                    }
                    
                    IoReleaseCancelSpinLock(CancelIrql);
                    
                } else {
                    Status = STATUS_INVALID_PARAMETER;
                    DisconnectInterrupt = FALSE;
                }
                
            } else {
                IoReleaseCancelSpinLock(CancelIrql);
                DisconnectInterrupt = FALSE;
                Status = STATUS_INVALID_PARAMETER;
            }
            
            //
            // Disconnect the interrupt if appropriate.
            //
            if (DisconnectInterrupt) {
                PptDisconnectInterrupt(Extension);
            }
        }
        break;
        
        ///////////////////////////////////////////////////////////////////////////////////
    case IOCTL_INTERNAL_PARALLEL_SET_CHIP_MODE:
        
        PptDump2(PARIOCTL, ("PptDispatchDeviceControl(): IOCTL_INTERNAL_PARALLEL_SET_CHIP_MODE\n") );
        
        //
        // Port already acquired?
        //
        // Make sure right parameters are sent in
        if (IrpSp->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(PARALLEL_CHIP_MODE) ) {
            
            Status = STATUS_BUFFER_TOO_SMALL;
            
        } else {
            PptDumpV( ("Calling function to set the chip mode by either the filter or us\n") );
            Status = PptSetChipMode (Extension, 
                                ((PPARALLEL_CHIP_MODE)Irp->AssociatedIrp.SystemBuffer)->ModeFlags );
        } // end check input buffer
        
        break;
        
    case IOCTL_INTERNAL_PARALLEL_CLEAR_CHIP_MODE:
        
        PptDump2(PARIOCTL, ("PptDispatchDeviceControl(): IOCTL_INTERNAL_PARALLEL_CLEAR_CHIP_MODE\n") );
        
        //
        // Port already acquired?
        //
        // Make sure right parameters are sent in
        if ( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(PARALLEL_CHIP_MODE) ){
            
            Status = STATUS_BUFFER_TOO_SMALL;
            
        } else {
            PptDumpV( ("Calling function to clear the mode set by parport or the chip filter\n") );
            Status = PptClearChipMode (Extension, 
                         ((PPARALLEL_CHIP_MODE)Irp->AssociatedIrp.SystemBuffer)->ModeFlags);
        } // end check input buffer
        
        break;
        
    case IOCTL_INTERNAL_INIT_1284_3_BUS:

        PptDump2(PARIOCTL, ("PptDispatchDeviceControl(): IOCTL_INTERNAL_INIT_1284_3_BUS\n") );

        ///////////////////////////////////////////////////////////////////////////////
        // Test code
        // Initialize the 1284.3 bus

        // Port is locked out already?

        Extension->PnpInfo.Ieee1284_3DeviceCount = PptInitiate1284_3( Extension );

        Status = STATUS_SUCCESS;
        
        break;
            
        // Takes a flat namespace Id for the device, also acquires the port
    case IOCTL_INTERNAL_SELECT_DEVICE:
        
        PptDump2(PARIOCTL, ("PptDispatchDeviceControl(): IOCTL_INTERNAL_SELECT_DEVICE\n") );

        // validate input buffer size
        if ( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(PARALLEL_1284_COMMAND) ){
            
            PptDumpV( ("Dot3 SELECT_DEVICE IRP %S - FAIL -  buffer too small\n",
                       Extension->PnpInfo.PortName) );
            Status = STATUS_BUFFER_TOO_SMALL;
            
        } else {
            
            // check to see if irp has been cancelled
            if ( Irp->Cancel ) {
                Status = STATUS_CANCELLED;
            } else {
                // Call Function to try to select device
                Status = PptTrySelectDevice( Extension, Irp->AssociatedIrp.SystemBuffer );

                IoAcquireCancelSpinLock(&CancelIrql);
                if ( Status == STATUS_PENDING ) {
                    PptSetCancelRoutine(Irp, PptCancelRoutine);
                    IoMarkIrpPending(Irp);
                    InsertTailList(&Extension->WorkQueue,
                                   &Irp->Tail.Overlay.ListEntry);
                    PptDump2( PARINFO, ("DOT3 SELECT_DEVICE IRP - Port BUSY - request queued\n") );
                }
                IoReleaseCancelSpinLock(CancelIrql);
            }

        } // endif - buffer too small
        
        break;
        
        // Deselects the current device, also releases the port
    case IOCTL_INTERNAL_DESELECT_DEVICE:
        
        PptDump2(PARIOCTL, ("PptDispatchDeviceControl(): IOCTL_INTERNAL_DESELECT_DEVICE\n") );
        
        // validate input buffer size
        if ( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(PARALLEL_1284_COMMAND) ){
            
            PptDumpV( ("DOT3 DESELECT_DEVICE %S - FAIL - buffer too small\n",
                       Extension->PnpInfo.PortName) );
            Status = STATUS_BUFFER_TOO_SMALL;
            
        } else {
            
            // Call Function to deselect the Device
            Status = PptDeselectDevice( Extension, Irp->AssociatedIrp.SystemBuffer );

        } // endif - buffer too small
        break;
        
        /////////////////////////////////////////////////////////////////////////////////
        
    default:
        
        PptDump2(PARIOCTL, ("PptDispatchDeviceControl(): default case - STATUS_INVALID_PARAMETER\n") );
        Status = STATUS_INVALID_PARAMETER;
        break;
    }
    
targetExit:

    if (Status != STATUS_PENDING) {
        Irp->IoStatus.Status = Status;
        PptReleaseRemoveLock(&Extension->RemoveLock, Irp);
        PptCompleteRequest(Irp, IO_NO_INCREMENT);
    }
    
    return Status;
}

