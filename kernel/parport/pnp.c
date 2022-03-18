/*++

Copyright (C) Microsoft Corporation, 1998 - 1999

Module Name:

    parport.sys

File Name:

    pnp.c

Abstract:

    This file contains the parport AddDevice routine and
      functions for handling PnP IRPs.

    Exports:

     - PptPnpAddDevice() - PnP AddDevice routine

     - PptDispatchPnp()  - PnP Dispatch routine

     - PptPnpInitDispatchFunctionTable() - Initialize PptPnpDispatchFunctionTable[]

--*/

#include "pch.h"
#include "pnp.h"

PDEVICE_RELATIONS
PptPnpBuildRemovalRelations( 
    IN PDEVICE_EXTENSION Extension 
    )
{
    PDEVICE_RELATIONS             relations      = NULL;
    PLIST_ENTRY                   listHead       = &Extension->RemovalRelationsList;
    PLIST_ENTRY                   thisListEntry  = NULL;
    PLIST_ENTRY                   firstListEntry = NULL;
    BOOLEAN                       done           = FALSE;
    PREMOVAL_RELATIONS_LIST_ENTRY node           = NULL;
    ULONG                         count;
    ULONG                         i;
    PDEVICE_OBJECT                pDevObj;

    PptDump2(PARPNP1, ("ioctl::PptPnpBuildRemovalRelations - Enter\n"));

    // lock list
    ExAcquireFastMutex( &Extension->ExtensionFastMutex );
    
    if( IsListEmpty( listHead ) ) {
        PptDump2(PARPNP1, ("ioctl::PptPnpBuildRemovalRelationsList - Empty List\n"));
        goto targetExit;
    }

    // get count
    count = 0;
    
    while( !done ) {

        thisListEntry = RemoveHeadList( listHead );
        node = CONTAINING_RECORD( thisListEntry, REMOVAL_RELATIONS_LIST_ENTRY, ListEntry );

        if( firstListEntry == thisListEntry ) {
            // done - we've already seen this one - push back onto front of list 
            InsertHeadList( listHead, &node->ListEntry );            
            done = TRUE;
            PptDump2(PARPNP1, ("ioctl::PptPnpBuildRemovalRelationsList - Done\n"));
        } else {
            // dump node info
            PptDump2(PARPNP1, ("ioctl::PptPnpBuildRemovalRelationsList - entry= %x %wZ\n", node->DeviceObject, &node->DeviceName));
            InsertTailList( listHead, &node->ListEntry );
            ++count;
        }

        if( !firstListEntry ) {
            // save first element - use for loop termination
            PptDump2(PARPNP1, ("ioctl::PptPnpBuildRemovalRelationsList - saving first\n"));
            firstListEntry = thisListEntry;
        }
    }
    PptDump2(PARPNP1, ("ioctl::PptPnpBuildRemovalRelationsList - count= %d\n", count));

    // allocate DEVICE_RELATIONS
    relations = ExAllocatePool(PagedPool, sizeof(DEVICE_RELATIONS) + (count-1) * sizeof(PDEVICE_OBJECT));
    if( !relations ) {
        PptDump2(PARPNP1, ("ioctl::PptPnpBuildRemovalRelationsList - unable to alloc pool to hold relations\n"));        
        goto targetExit;
    }
    
    // populate DEVICE_RELATIONS
    relations->Count = count;
    for( i=0 ; i < count ; ++i ) {
        thisListEntry = RemoveHeadList( listHead );
        node = CONTAINING_RECORD( thisListEntry, REMOVAL_RELATIONS_LIST_ENTRY, ListEntry );
        PptDump2(PARPNP1, ("ioctl::PptPnpBuildRemovalRelationsList - adding device= %x %wZ\n",
                           node->DeviceObject, &node->DeviceName));
        pDevObj = node->DeviceObject;
        ObReferenceObject( pDevObj );
        relations->Objects[i] = pDevObj;
        InsertTailList( listHead, &node->ListEntry );            
    }

targetExit:

    // unlock list
    ExReleaseFastMutex( &Extension->ExtensionFastMutex );    

    return relations;
}

NTSTATUS
PptPnpStartScanPciCardCmResourceList(
    IN  PDEVICE_EXTENSION Extension,
    IN  PIRP              Irp, 
    OUT PBOOLEAN          FoundPort,
    OUT PBOOLEAN          FoundIrq,
    OUT PBOOLEAN          FoundDma
    )
/*++dvdf3

Routine Description:

    This routine is used to parse the resource list for what we
      believe are PCI parallel port cards.

    This function scans the CM_RESOURCE_LIST supplied with the Pnp 
      IRP_MN_START_DEVICE IRP, extracts the resources from the list, 
      and saves them in the device extension.

Arguments:

    Extension    - The device extension of the target of the START IRP
    Irp          - The IRP
    FoundPort    - Did we find a  Port resource?
    FoundIrq     - Did we find an IRQ  resource?
    FoundDma     - Did we find a  DMA  resource?

Return Value:

    STATUS_SUCCESS                - if we were given a resource list,
    STATUS_INSUFFICIENT_RESOURCES - otherwise

--*/
{
    NTSTATUS                        status   = STATUS_SUCCESS;
    PIO_STACK_LOCATION              irpStack = IoGetCurrentIrpStackLocation( Irp );
    PCM_RESOURCE_LIST               ResourceList;
    PCM_FULL_RESOURCE_DESCRIPTOR    FullResourceDescriptor;
    PCM_PARTIAL_RESOURCE_LIST       PartialResourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialResourceDescriptor;
    ULONG                           i;
    ULONG                           length;
    
    PptDump2(PARRESOURCE, ("pnp::PptPnpStartScanPciCardCmResourceList - Enter - Extension= %x , Irp= %x\n", Extension, Irp));

    *FoundPort = FALSE;
    *FoundIrq  = FALSE;
    *FoundDma  = FALSE;
    
    ResourceList = irpStack->Parameters.StartDevice.AllocatedResourcesTranslated;
    
    FullResourceDescriptor = &ResourceList->List[0];
    
    if( FullResourceDescriptor ) {
        
        Extension->InterfaceType = FullResourceDescriptor->InterfaceType;
        
        PartialResourceList = &FullResourceDescriptor->PartialResourceList;
        
        for (i = 0; i < PartialResourceList->Count; i++) {
            
            PartialResourceDescriptor = &PartialResourceList->PartialDescriptors[i];
            
            switch (PartialResourceDescriptor->Type) {
                
            case CmResourceTypePort:
                
                length = PartialResourceDescriptor->u.Port.Length;

                //
                // Use a heuristic based on length to guess which register set is
                //   SPP+EPP, which is ECP, and which is PCI Config or other.
                //
                switch( length ) {

                case 8: // SPP + EPP base address

                    PptDump2(PARRESOURCE, ("pnp::PptPnpStartScanPciCardCmResourceList - found SPP+EPP\n"));
                    Extension->PortInfo.OriginalController = PartialResourceDescriptor->u.Port.Start;
                    Extension->PortInfo.SpanOfController   = PartialResourceDescriptor->u.Port.Length;
                    Extension->PortInfo.Controller         = (PUCHAR)(ULONG_PTR)Extension->PortInfo.OriginalController.QuadPart;
                    Extension->AddressSpace                = PartialResourceDescriptor->Flags;
                    *FoundPort = TRUE;
                    break;

                case 4: // ECP base address
                    
                    PptDump2(PARRESOURCE, ("pnp::PptPnpStartScanPciCardCmResourceList - found ECP\n"));
                    Extension->PnpInfo.OriginalEcpController = PartialResourceDescriptor->u.Port.Start;
                    Extension->PnpInfo.SpanOfEcpController   = PartialResourceDescriptor->u.Port.Length;
                    Extension->PnpInfo.EcpController         = (PUCHAR)(ULONG_PTR)Extension->PnpInfo.OriginalEcpController.QuadPart;
                    Extension->EcpAddressSpace               = PartialResourceDescriptor->Flags;
                    break;

                default:
                    // don't know what this is - ignore it
                    PptDump2(PARRESOURCE, ("pnp::PptPnpStartScanPciCardCmResourceList - unrecognised Port length\n"));
                }
                break;
                
            case CmResourceTypeBusNumber:
                
                Extension->BusNumber = PartialResourceDescriptor->u.BusNumber.Start;
                break;
                
            case CmResourceTypeInterrupt:
                
                *FoundIrq = TRUE;
                Extension->FoundInterrupt       = TRUE;
                Extension->InterruptLevel       = (KIRQL)PartialResourceDescriptor->u.Interrupt.Level;
                Extension->InterruptVector      = PartialResourceDescriptor->u.Interrupt.Vector;
                Extension->InterruptAffinity    = PartialResourceDescriptor->u.Interrupt.Affinity;
                
                if (PartialResourceDescriptor->Flags & CM_RESOURCE_INTERRUPT_LATCHED) {
                    
                    Extension->InterruptMode = Latched;
                    
                } else {
                    
                    Extension->InterruptMode = LevelSensitive;
                }
                break;
                
            case CmResourceTypeDma:
                
                *FoundDma = TRUE;
                Extension->DmaChannel   = PartialResourceDescriptor->u.Dma.Channel;
                Extension->DmaPort      = PartialResourceDescriptor->u.Dma.Port;
                Extension->DmaWidth     = PartialResourceDescriptor->Flags;
                break;
                
            default:

                break;

            } // end switch( PartialResourceDescriptor->Type )
        } // end for(... ; i < PartialResourceList->Count ; ...)
    } // end if( FullResourceDescriptor )
    
    return status;
}

BOOLEAN PptIsPci(
    PDEVICE_EXTENSION Extension, 
    PIRP              Irp 
)
/*++

Does this look like a PCI card? Return TRUE if yes, FALSE otherwise

--*/
{
    NTSTATUS                        status   = STATUS_SUCCESS;
    PIO_STACK_LOCATION              irpStack = IoGetCurrentIrpStackLocation( Irp );
    PCM_RESOURCE_LIST               ResourceList;
    PCM_FULL_RESOURCE_DESCRIPTOR    FullResourceDescriptor;
    PCM_PARTIAL_RESOURCE_LIST       PartialResourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialResourceDescriptor;
    ULONG                           i;
    ULONG                           portResourceDescriptorCount = 0;
    BOOLEAN                         largePortRangeFound         = FALSE;
    ULONG                           rangeLength;
    
#if DBG
    PptDump2(PARRESOURCE, ("pnp::PptIsPci - Enter - Extension= %x , Irp= %x\n", Extension, Irp));
#else
    UNREFERENCED_PARAMETER( Extension );
#endif

    //
    // If there are more than 2 IO resource descriptors, or if any IO resource
    //   descriptor has a range > 8 bytes, then assume that this is a PCI device
    //   and requires non-traditional handling.
    //

    ResourceList = irpStack->Parameters.StartDevice.AllocatedResourcesTranslated;
    
    if (ResourceList == NULL) {
        // we weren't given any resources
        PptDump2(PARRESOURCE, ("pnp::PptIsPCI - No Resources - AllocatedResourcesTranslated == NULL\n") );
        return FALSE;
    }

    FullResourceDescriptor = &ResourceList->List[0];
    
    if (FullResourceDescriptor) {
        
        PptDump2(PARRESOURCE, ("pnp::PptIsPCI - Interface = %d\n", FullResourceDescriptor->InterfaceType));
        
        PartialResourceList = &FullResourceDescriptor->PartialResourceList;
        
        for (i = 0; i < PartialResourceList->Count; i++) {
            
            PartialResourceDescriptor = &PartialResourceList->PartialDescriptors[i];
            
            switch (PartialResourceDescriptor->Type) {
                
            case CmResourceTypePort:
                
                rangeLength = PartialResourceDescriptor->u.Port.Length;
                PptDump2(PARRESOURCE, ("pnp::PptIsPCI - CmResourceTypePort - Start= %I64x, Length= %x , \n",
                                       PartialResourceDescriptor->u.Port.Start.QuadPart,
                                       rangeLength));

                ++portResourceDescriptorCount;

                if( rangeLength > 8 ) {
                    PptDump2(PARRESOURCE, ("pnp::PptIsPCI - found largePortRange = %x\n", rangeLength));
                    largePortRangeFound = TRUE;
                }

                break;
                
            default:
                PptDump2(PARRESOURCE, ("pnp::PptIsPCI - Other ResourceType\n"));
            } // end switch( PartialResourceDescriptor->Type )
        } // end for(... ; i < PartialResourceList->Count ; ...)
    } // end if( FullResourceDescriptor )
    
    if( (portResourceDescriptorCount > 2) || (TRUE == largePortRangeFound) ) {
        PptDump2(PARRESOURCE, ("pnp::PptIsPCI - looks like PCI\n"));
        return TRUE;
    } else {
        PptDump2(PARRESOURCE, ("pnp::PptIsPCI - does not look like PCI\n"));
        return FALSE;
    }
}

VOID
PptCompleteRequest(
    IN PIRP Irp,
    IN CCHAR PriorityBoost
    )
{
#if 0
    PptDump2(PARENTRY, ("pnp::PptCompleteRequest - completing IRP= %x , UserEvent= %x\n",Irp, Irp->UserEvent));
    if( Irp->UserEvent ) {
        ASSERT_EVENT( Irp->UserEvent );
    }
#endif
    IoCompleteRequest(Irp, PriorityBoost);
}


//
// Table of pointers to functions for handling PnP IRPs. 
//
// IrpStack->MinorFunction is used as an index into the table. 
//   (Special handling required if IrpStack->MinorFunction > table size.)
//
//  - Initialized by PptPnpInitDispatchFunctionTable()
//
//  - Used by PptDispatchPnp()
// 
// 
static PDRIVER_DISPATCH PptPnpDispatchFunctionTable[ MAX_PNP_IRP_MN_HANDLED + 1 ];

VOID
PptPnpInitDispatchFunctionTable(
    VOID
    )
/*++dvdf8

Routine Description:

    This function is called from DriverEntry() to initialize
      PptPnpDispatchFunctionTable[] for use as a call table
      by the PnP dispatch routine PptDispatchPnp().

Arguments:

    none

Return Value:

    none

--*/
{
    //
    // Begin by initializing all entries to point to PptPnpUnhandledIrp() 
    //   and then override the table entries for PnP request types that 
    //   require special handling.
    //

    ULONG i;
    for( i=0 ; i <= MAX_PNP_IRP_MN_HANDLED ; ++i ) {
        PptPnpDispatchFunctionTable[i] = PptPnpUnhandledIrp;
    }

    PptPnpDispatchFunctionTable[ IRP_MN_START_DEVICE                 ] = PptPnpStartDevice;
    PptPnpDispatchFunctionTable[ IRP_MN_FILTER_RESOURCE_REQUIREMENTS ] = PptPnpFilterResourceRequirements;

    PptPnpDispatchFunctionTable[ IRP_MN_QUERY_DEVICE_RELATIONS       ] = PptPnpQueryDeviceRelations;

    PptPnpDispatchFunctionTable[ IRP_MN_QUERY_STOP_DEVICE            ] = PptPnpQueryStopDevice;
    PptPnpDispatchFunctionTable[ IRP_MN_CANCEL_STOP_DEVICE           ] = PptPnpCancelStopDevice;
    PptPnpDispatchFunctionTable[ IRP_MN_STOP_DEVICE                  ] = PptPnpStopDevice;

    PptPnpDispatchFunctionTable[ IRP_MN_QUERY_REMOVE_DEVICE          ] = PptPnpQueryRemoveDevice;
    PptPnpDispatchFunctionTable[ IRP_MN_CANCEL_REMOVE_DEVICE         ] = PptPnpCancelRemoveDevice;
    PptPnpDispatchFunctionTable[ IRP_MN_REMOVE_DEVICE                ] = PptPnpRemoveDevice;

    PptPnpDispatchFunctionTable[ IRP_MN_SURPRISE_REMOVAL             ] = PptPnpSurpriseRemoval;
}

NTSTATUS
PptPnpAddDevice(
    IN PDRIVER_OBJECT pDriverObject,
    IN PDEVICE_OBJECT pPhysicalDeviceObject
    )
/*++dvdf3

Routine Description:

    This routine creates a new parport device object and attaches 
      the device object to the device stack.

Arguments:

    pDriverObject           - pointer to the driver object for this instance of parport.

    pPhysicalDeviceObject   - pointer to the device object that represents the port.

Return Value:

    STATUS_SUCCESS          - if successful.
    Error Status            - otherwise.

--*/
{
    NTSTATUS            status        = STATUS_SUCCESS;
    PDEVICE_OBJECT      parentDevice;
    PDEVICE_OBJECT      pDeviceObject;
    PDEVICE_EXTENSION   Extension;

    DDPnP1(("-- AddDevice\n"));

    PptDump2(PARPNP1, ("pnp::PptPnpAddDevice - Enter - DriverObject = %08x , PDO = %08x\n",
                       pDriverObject, pPhysicalDeviceObject) );

#if DBG
    PptBreak(PAR_BREAK_ON_ADD_DEVICE, ("PptPnpAddDevice(PDRIVER_OBJECT, PDEVICE_OBJECT)\n") );
#endif

    //
    // Create and initialize device object 
    //
    pDeviceObject = PptBuildDeviceObject( pDriverObject, pPhysicalDeviceObject );
    if( NULL == pDeviceObject ) {
        PptDump2(PARERRORS, ("pnp::PptAddDevice - Create DeviceObject FAILED\n") );
        return STATUS_UNSUCCESSFUL;
    }
    Extension = pDeviceObject->DeviceExtension;

    //
    // Register device interface with PnP
    //
    status = IoRegisterDeviceInterface(pPhysicalDeviceObject, &GUID_PARALLEL_DEVICE, NULL, &Extension->SymbolicLinkName);
    if( !NT_SUCCESS( status ) ) {
        PptDump2(PARERRORS, ("pnp::PptAddDevice - IoRegisterDeviceInterface FAILED, status=%x", status) );
        IoDeleteDevice( pDeviceObject );
        return status;
    }

    //
    // Attach device object to device stack
    //
    parentDevice = IoAttachDeviceToDeviceStack( pDeviceObject, pPhysicalDeviceObject );
    if( NULL == parentDevice ) {
        PptDump2(PARERRORS, ("pnp::PptAddDevice - IoAttachDeviceToDeviceStack - FAILED\n") );
        IoDeleteDevice( pDeviceObject );
        return STATUS_UNSUCCESSFUL;
    }
    Extension->ParentDeviceObject = parentDevice;

    //
    // Tell the IO System that we have another parallel port
    //   (This count is used by legacy drivers.)
    //
    IoGetConfigurationInformation()->ParallelCount++;

    //
    // Done initializing - tell IO System that we are ready to receive IRPs
    //
    pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    PptDump2(PARPNP1, ("pnp::PptPnpAddDevice - SUCCESS\n") );

    return STATUS_SUCCESS;
}

NTSTATUS
PptDispatchPnp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )
/*++dvdf8

Routine Description:

    This is the dispatch function for PnP IRPs.

     - Acquire the device's RemoveLock.
     - Forward the request to an appropriate handler.

Note: 

    The handler called by this routine must release the RemoveLock 
      before returning control to this routine!

Arguments:

    DeviceObject - The target device for the IRP
    Irp          - The IRP

Return Value:

    Status returned by PptAcquireRemoveLockOrFailIrp() - if unable to acquire RemoveLock

    Status returned by the IrpStack->MinorFunction handler - otherwise

--*/
{
    NTSTATUS status;

    //
    // Generate debug trace output useful in tracking PnP IRPs.
    //
    PptDebugDumpPnpIrpInfo( DeviceObject, Irp );


    //
    // Acquire RemoveLock to prevent DeviceObject from being REMOVED
    //   while we are using it. If we are unable to acquire the RemoveLock
    //   then the DeviceObject has already been REMOVED.
    //
    status = PptAcquireRemoveLockOrFailIrp(DeviceObject, Irp);

    if( NT_SUCCESS( status ) ) {

        PIO_STACK_LOCATION irpStack      = IoGetCurrentIrpStackLocation( Irp );
        UCHAR              minorFunction = irpStack->MinorFunction;

        //
        // RemoveLock is held. Forward the request to the appropriate handler.
        //
        // Note that the handler must release the RemoveLock prior to returning
        //   control to this function.
        //

        if( minorFunction > MAX_PNP_IRP_MN_HANDLED ) {
            //
            // Do standard processing for unrecognized PnP requests.
            //
            status = PptPnpUnhandledIrp( DeviceObject, Irp );
        } else {
            //
            // Forward request to appropriate handler based on type of PnP request.
            //
            status = PptPnpDispatchFunctionTable[ minorFunction ]( DeviceObject, Irp );
        }
    }

    return status;
}

NTSTATUS
PptPnpStartDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
) 
/*++dvdf8

Routine Description:

    This function handles PnP IRP_MN_START IRPs.

     - Wait for the bus driver and any drivers beneath 
         us in the driver stack to handle this first.
     - Get, validate, and save the resources given to us by PnP.
     - Assign IDs to and get a count of 1284.3 daisy chain devices
         connected to the port.
     - Determine the capabilities of the chipset (BYTE, EPP, ECP).
     - Set our PnP device interface state to trigger
         an interface arrival callback to anyone listening 
         on our GUID.

Arguments:

    DeviceObject - The target device for the IRP
    Irp          - The IRP

Return Value:

    STATUS_SUCCESS              - on success,
    an appropriate error status - otherwise

--*/
{
    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    NTSTATUS          status;
    BOOLEAN           foundPort = FALSE;
    BOOLEAN           foundIrq  = FALSE;
    BOOLEAN           foundDma  = FALSE;


    //
    // This IRP must be handled first by the parent bus driver
    //   and then by each higher driver in the device stack.
    //
    status = PptPnpBounceAndCatchPnpIrp(extension, Irp);
    if( !NT_SUCCESS( status ) && ( status != STATUS_NOT_SUPPORTED ) ) {
        // Someone below us in the driver stack explicitly failed the START.
        goto targetExit;
    }


    //
    // Extract resources from CM_RESOURCE_LIST and save them in our extension.
    //
    status = PptPnpStartScanCmResourceList(extension, Irp, &foundPort, &foundIrq, &foundDma);
    if( !NT_SUCCESS( status ) ) {
        goto targetExit;
    }

    //
    // Do our resources appear to be valid?
    //
    status = PptPnpStartValidateResources(DeviceObject, foundPort, foundIrq, foundDma);
    if( !NT_SUCCESS( status ) ) {
        goto targetExit;
    }


    //
    // Initialize the IEEE 1284.3 "bus" by assigning IDs [0..3] to 
    //   the 1284.3 daisy chain devices connected to the port. This
    //   function also gives us a count of the number of such 
    //   devices connected to the port.
    //
    extension->PnpInfo.Ieee1284_3DeviceCount = PptInitiate1284_3( extension );
    
    //
    // Determine the hardware modes supported (BYTE, ECP, EPP) by
    //   the parallel port chipset and save this information in our extension.
    //

    // Check to see if the filter parchip is there and use the modes it can set
    status = PptDetectChipFilter( extension );

    // if filter driver was not found use our own generic port detection
    if ( !NT_SUCCESS( status ) ) {
        PptDetectPortType( extension );
    }

    
    //
    // Register w/WMI
    //
    status = PptWmiInitWmi( DeviceObject );
    if( !NT_SUCCESS( status ) ) {
        goto targetExit;
    }


    //
    // Signal those who registered for PnP interface change notification 
    //   on our GUID that we have STARTED (trigger an INTERFACE_ARRIVAL
    //   PnP callback).
    //
    status = IoSetDeviceInterfaceState(&extension->SymbolicLinkName, TRUE);
    if( !NT_SUCCESS(status) ) {
        status = STATUS_NOT_SUPPORTED;
    }

targetExit:

    if( NT_SUCCESS( status ) ) {
        // 
        // Note in our extension that we have successfully STARTED.
        //
        ExAcquireFastMutex( &extension->ExtensionFastMutex );
        PptSetFlags( extension->DeviceStateFlags, PPT_DEVICE_STARTED );
        ExReleaseFastMutex( &extension->ExtensionFastMutex );
    }

    Irp->IoStatus.Status      = status;
    Irp->IoStatus.Information = 0;
    PptCompleteRequest( Irp, IO_NO_INCREMENT );

    PptReleaseRemoveLock( &extension->RemoveLock, Irp );

    return status;
}

NTSTATUS
PptPnpStartScanCmResourceList(
    IN  PDEVICE_EXTENSION Extension,
    IN  PIRP              Irp, 
    OUT PBOOLEAN          FoundPort,
    OUT PBOOLEAN          FoundIrq,
    OUT PBOOLEAN          FoundDma
    )
/*++dvdf3

Routine Description:

    This function is a helper function called by PptPnpStartDevice(). 

    This function scans the CM_RESOURCE_LIST supplied with the Pnp 
      IRP_MN_START_DEVICE IRP, extracts the resources from the list, 
      and saves them in the device extension.

Arguments:

    Extension    - The device extension of the target of the START IRP
    Irp          - The IRP
    FoundPort    - Did we find a  Port resource?
    FoundIrq     - Did we find an IRQ  resource?
    FoundDma     - Did we find a  DMA  resource?

Return Value:

    STATUS_SUCCESS                - if we were given a resource list,
    STATUS_INSUFFICIENT_RESOURCES - otherwise

--*/
{
    NTSTATUS                        status   = STATUS_SUCCESS;
    PIO_STACK_LOCATION              irpStack = IoGetCurrentIrpStackLocation( Irp );
    PCM_RESOURCE_LIST               ResourceList;
    PCM_FULL_RESOURCE_DESCRIPTOR    FullResourceDescriptor;
    PCM_PARTIAL_RESOURCE_LIST       PartialResourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialResourceDescriptor;
    ULONG                           i;
    PHYSICAL_ADDRESS                start;
    ULONG                           length;
    BOOLEAN                         isPci = FALSE;
    
    *FoundPort = FALSE;
    *FoundIrq  = FALSE;
    *FoundDma  = FALSE;
    
    ResourceList = irpStack->Parameters.StartDevice.AllocatedResourcesTranslated;
    
    if (ResourceList == NULL) {
        // we weren't given any resources, bail out
        PptDumpP( ("START - FAIL - No Resources - AllocatedResourcesTranslated == NULL\n") );
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto targetExit;
    }

    if(IsNotNEC_98) {
        // The NEC_98 not have PCI-Parallel.
        if( TRUE == PptIsPci( Extension, Irp ) ) {
            // This appears to be a PCI card
            PptDumpP( ("START - FAIL - Appears to be PCI card\n") );
            status = PptPnpStartScanPciCardCmResourceList(Extension, Irp, FoundPort, FoundIrq, FoundDma);
            isPci=TRUE;
            goto targetExit;
        }
    }
    
    //
    // Device appears to be traditional / non-PCI card parallel port
    //

    FullResourceDescriptor = &ResourceList->List[0];
    
    if (FullResourceDescriptor) {
        
        Extension->InterfaceType = FullResourceDescriptor->InterfaceType;
        
        PartialResourceList = &FullResourceDescriptor->PartialResourceList;
        
        for (i = 0; i < PartialResourceList->Count; i++) {
            
            PartialResourceDescriptor = &PartialResourceList->PartialDescriptors[i];
            
            switch (PartialResourceDescriptor->Type) {
                
            case CmResourceTypePort:
                
                start  = PartialResourceDescriptor->u.Port.Start;
                length = PartialResourceDescriptor->u.Port.Length;
                PptDump2(PARRESOURCE, ("pnp::PptPnpStartScanCmResourceList - start= %I64x , length=%x\n",start, length));

                *FoundPort = TRUE;
                if ((Extension->PortInfo.OriginalController.LowPart == 0) &&
                    (Extension->PortInfo.OriginalController.HighPart == 0)) {
                    
                    PptDump2(PARRESOURCE, ("pnp::PptPnpStartScanCmResourceList - assuming Controller\n"));

                    Extension->PortInfo.OriginalController = PartialResourceDescriptor->u.Port.Start;
                    Extension->PortInfo.SpanOfController   = PartialResourceDescriptor->u.Port.Length;
                    Extension->PortInfo.Controller         = (PUCHAR)(ULONG_PTR)Extension->PortInfo.OriginalController.QuadPart;
                    Extension->AddressSpace                = PartialResourceDescriptor->Flags;
                    
                    if ( (Extension->PortInfo.SpanOfController == 0x1000) && PptIsNecR98Machine() ) {
                        //
                        // Firmware bug with R98 machine.
                        //
                        Extension->PortInfo.SpanOfController = 8;
                    }
                    
                } else if ((Extension->PnpInfo.OriginalEcpController.LowPart == 0) &&
                           (Extension->PnpInfo.OriginalEcpController.HighPart == 0) &&
                           (IsNotNEC_98)) {
                    
                    if ((PartialResourceDescriptor->u.Port.Start.LowPart < Extension->PortInfo.OriginalController.LowPart) &&
                        (PartialResourceDescriptor->u.Port.Start.HighPart < Extension->PortInfo.OriginalController.HighPart)) {
                        
                        //
                        // Swapping address spaces
                        //
                        
                        PptDump2(PARRESOURCE, ("pnp::PptPnpStartScanCmResourceList - assuming Controller - Swapping Controller/EcpController\n"));

                        Extension->PnpInfo.OriginalEcpController = Extension->PortInfo.OriginalController;
                        Extension->PnpInfo.SpanOfEcpController   = Extension->PortInfo.SpanOfController;
                        Extension->PnpInfo.EcpController         = Extension->PortInfo.Controller;
                        Extension->EcpAddressSpace               = Extension->AddressSpace;
                        
                        Extension->PortInfo.OriginalController = PartialResourceDescriptor->u.Port.Start;
                        Extension->PortInfo.SpanOfController   = PartialResourceDescriptor->u.Port.Length;
                        Extension->PortInfo.Controller         = (PUCHAR)(ULONG_PTR)Extension->PortInfo.OriginalController.QuadPart;
                        Extension->AddressSpace                = PartialResourceDescriptor->Flags;
                        
                        if ( (Extension->PortInfo.SpanOfController == 0x1000) && PptIsNecR98Machine() ) {
                            //
                            // Firmware bug with R98 machine.
                            //
                            Extension->PortInfo.SpanOfController = 8;
                        }
                        
                    } else {
                        PptDump2(PARRESOURCE, ("pnp::PptPnpStartScanCmResourceList - assuming EcpController\n"));

                        Extension->PnpInfo.OriginalEcpController = PartialResourceDescriptor->u.Port.Start;
                        Extension->PnpInfo.SpanOfEcpController   = PartialResourceDescriptor->u.Port.Length;
                        Extension->PnpInfo.EcpController         = (PUCHAR)(ULONG_PTR)Extension->PnpInfo.OriginalEcpController.QuadPart;
                        Extension->EcpAddressSpace               = PartialResourceDescriptor->Flags;
                    }
                    
                }
                break;
                
            case CmResourceTypeBusNumber:
                
                Extension->BusNumber = PartialResourceDescriptor->u.BusNumber.Start;
                break;
                
            case CmResourceTypeInterrupt:
                
                *FoundIrq = TRUE;
                Extension->FoundInterrupt       = TRUE;
                Extension->InterruptLevel       = (KIRQL)PartialResourceDescriptor->u.Interrupt.Level;
                Extension->InterruptVector      = PartialResourceDescriptor->u.Interrupt.Vector;
                Extension->InterruptAffinity    = PartialResourceDescriptor->u.Interrupt.Affinity;
                
                if (PartialResourceDescriptor->Flags & CM_RESOURCE_INTERRUPT_LATCHED) {
                    
                    Extension->InterruptMode = Latched;
                    
                } else {
                    
                    Extension->InterruptMode = LevelSensitive;
                }
                break;
                
            case CmResourceTypeDma:
                
                *FoundDma = TRUE;
                Extension->DmaChannel   = PartialResourceDescriptor->u.Dma.Channel;
                Extension->DmaPort      = PartialResourceDescriptor->u.Dma.Port;
                Extension->DmaWidth     = PartialResourceDescriptor->Flags;
                break;
                
            default:

                break;

            } // end switch( PartialResourceDescriptor->Type )
        } // end for(... ; i < PartialResourceList->Count ; ...)
    } // end if( FullResourceDescriptor )
    
targetExit:

    if( FALSE == isPci ) {
        // we scanned the resources - dump what we found
        PptDump2(PARRESOURCE, ("pnp::PptPnpStartScanCmResourceList - done, found:\n"));
        PptDump2(PARRESOURCE, ("  OriginalEcpController= %I64x\n", Extension->PnpInfo.OriginalEcpController));
        PptDump2(PARRESOURCE, ("  EcpController        = %p\n",    Extension->PnpInfo.EcpController));
        PptDump2(PARRESOURCE, ("  SpanOfEcpController  = %x\n",    Extension->PnpInfo.SpanOfEcpController));
    }
    return status;
}

NTSTATUS
PptPnpStartValidateResources(
    IN PDEVICE_OBJECT    DeviceObject,                              
    IN BOOLEAN           FoundPort,
    IN BOOLEAN           FoundIrq,
    IN BOOLEAN           FoundDma
    )
/*++dvdf3

Routine Description:

    This function is a helper function called by PptPnpStartDevice(). 

    This function does a sanity check of the resources saved in our
      extension by PptPnpStartScanCmResourceList() to determine 
      if those resources appear to be valid. Checks for for Irq 
      and Dma resource validity are anticipated in a future version.

Arguments:

    DeviceObject - The target of the START IRP
    FoundPort    - Did we find a  Port resource?
    FoundIrq     - Did we find an IRQ  resource?
    FoundDma     - Did we find a  DMA  resource?

Return Value:

    STATUS_SUCCESS        - on success,
    STATUS_NO_SUCH_DEVICE - if we weren't given a port resource,
    STATUS_NONE_MAPPED    - if we were given a port resource but our 
                              port address is NULL

--*/
{
    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    NTSTATUS          status    = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER( FoundIrq ); // future use
    UNREFERENCED_PARAMETER( FoundDma ); // future use

    if( !FoundPort ) {
        status = STATUS_NO_SUCH_DEVICE;
    } else {
//         extension->PortInfo.Controller = (PUCHAR)(ULONG_PTR)extension->PortInfo.OriginalController.LowPart;
        extension->PortInfo.Controller = (PUCHAR)(ULONG_PTR)extension->PortInfo.OriginalController.QuadPart;

        if(!extension->PortInfo.Controller) {
            // ( Controller == NULL ) is invalid
            PptLogError(DeviceObject->DriverObject, DeviceObject,
                        extension->PortInfo.OriginalController, PhysicalZero, 0, 0, 0, 10,
                        STATUS_SUCCESS, PAR_REGISTERS_NOT_MAPPED);
            status = STATUS_NONE_MAPPED;
        }
    }
    return status;
}

NTSTATUS
PptPnpFilterResourceRequirements(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    ) 
/*++dvdf8

Routine Description:

    This function handles PnP IRP_MN_FILTER_RESOURCE_REQUIREMENTS IRPs.

     - Wait for the bus driver and any drivers beneath 
         us in the driver stack to handle this first.
     - Query the registry to find the type of filtering desired.
     - Filter out IRQ resources as specified by the registry setting.

Arguments:

    DeviceObject - The target device for the IRP
    Irp          - The IRP

Return Value:

    STATUS_SUCCESS              - on success,
    an appropriate error status - otherwise

--*/
{
    PDEVICE_EXTENSION              extension               = DeviceObject->DeviceExtension;
    ULONG                          filterResourceMethod    = PPT_FORCE_USE_NO_IRQ;
    PIO_RESOURCE_REQUIREMENTS_LIST pResourceRequirementsIn;
    NTSTATUS                       status;


    //
    // DDK Rule: Add on the way down, modify on the way up. We are modifying
    //   the resource list so let the drivers beneath us handle this IRP first.
    //
    status    = PptPnpBounceAndCatchPnpIrp(extension, Irp);
    if( !NT_SUCCESS(status) && (status != STATUS_NOT_SUPPORTED) ) {
        // Someone below us in the driver stack explicitly failed the IRP.
        goto targetExit;
    }


    //
    // Find the "real" resource requirments list, either the PnP list
    //   or the list created by another driver in the stack.
    //
    if ( Irp->IoStatus.Information == 0 ) {
        //
        // No one else has created a new resource list. Use the original 
        //   list from the PnP Manager.
        //
        PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation( Irp );
        pResourceRequirementsIn = IrpStack->Parameters.FilterResourceRequirements.IoResourceRequirementList;

        if (pResourceRequirementsIn == NULL) {
            //
            // NULL list, nothing to do.
            //
            goto targetExit;
        }

    } else {
        //
        // Another driver has created a new resource list. Use the list that they created.
        //
        pResourceRequirementsIn = (PIO_RESOURCE_REQUIREMENTS_LIST)Irp->IoStatus.Information;
    }


    //
    // Check the registry to find out the desired type of resource filtering.
    //
    // The following call sets the default value for filterResourceMethod 
    //   if the registry query fails.
    //
    PptRegGetDeviceParameterDword( extension->PhysicalDeviceObject,
                                   (PWSTR)L"FilterResourceMethod",
                                   &filterResourceMethod );

    PptDump2(PARRESOURCE,("filterResourceMethod=%x\n", filterResourceMethod) );
    PptDump2(PARRESOURCE,("ResourceRequirementsList BEFORE Filtering:\n") );
    PptDebugDumpResourceRequirementsList(pResourceRequirementsIn);


    //
    // Do filtering based on registry setting.
    //
    switch( filterResourceMethod ) {

    case PPT_FORCE_USE_NO_IRQ: 
        //
        // Registry setting dictates that we should refuse to accept IRQ resources.
        //
        // * This is the default behavior which means that we make the IRQ available 
        //     for legacy net and sound cards that may not work if they cannot get
        //     the IRQ.
        //
        // - If we find a resource alternative that does not contain an IRQ resource
        //     then we remove those resource alternatives that do contain IRQ 
        //     resources from the list of alternatives.
        //
        // - Otherwise we have to play hardball. Since all resource alternatives
        //     contain IRQ resources we simply "nuke" the IRQ resource descriptors
        //     by changing their resource Type from CmResourceTypeInterrupt to
        //     CmResourceTypeNull.
        //

        PptDump2(PARRESOURCE,("PPT_FORCE_USE_NO_IRQ\n") );

        if( PptPnpFilterExistsNonIrqResourceList( pResourceRequirementsIn ) ) {

            PptDump2(PARRESOURCE,("Found Resource List with No IRQ - Filtering\n") );
            PptPnpFilterRemoveIrqResourceLists( pResourceRequirementsIn );

        } else {

            PptDump2(PARRESOURCE,("Did not find Resource List with No IRQ - Nuking IRQ resource descriptors\n") );
            PptPnpFilterNukeIrqResourceDescriptorsFromAllLists( pResourceRequirementsIn );

        }

        PptDump2(PARRESOURCE,("ResourceRequirementsList AFTER Filtering:\n") );
        PptDebugDumpResourceRequirementsList( pResourceRequirementsIn );
        break;


    case PPT_TRY_USE_NO_IRQ: 
        //
        // Registry setting dictates that we should TRY to give up IRQ resources.
        //
        // - If we find a resource alternative that does not contain an IRQ resource
        //     then we remove those resource alternatives that do contain IRQ 
        //     resources from the list of alternatives.
        //
        // - Otherwise we do nothing.
        //

        PptDump2(PARRESOURCE,("PPT_TRY_USE_NO_IRQ\n") );
        if( PptPnpFilterExistsNonIrqResourceList(pResourceRequirementsIn) ) {

            PptDump2(PARRESOURCE,("Found Resource List with No IRQ - Filtering\n") );
            PptPnpFilterRemoveIrqResourceLists(pResourceRequirementsIn);

            PptDump2(PARRESOURCE,("ResourceRequirementsList AFTER Filtering:\n") );
            PptDebugDumpResourceRequirementsList(pResourceRequirementsIn);

        } else {

            // leave the IO resource list as is
            PptDump2(PARRESOURCE,("Did not find Resource List with No IRQ - Do nothing\n") );

        }
        break;


    case PPT_ACCEPT_IRQ: 
        //
        // Registry setting dictates that we should NOT filter out IRQ resources.
        //
        // - Do nothing.
        //
        PptDump2(PARRESOURCE,("PPT_ACCEPT_IRQ\n") );
        break;


    default:
        //
        // Invalid registry setting. 
        //
        // - Do nothing.
        //
        // RMT dvdf - May be desirable to write an error log entry here.
        //
        PptDump2(PARERRORS, ("ERROR:IGNORED: bad filterResourceMethod=%x\n", filterResourceMethod) );
    }

targetExit:

    //
    // Preserve Irp->IoStatus.Information because it may point to a
    //   buffer and we don't want to cause a memory leak.
    //
    Irp->IoStatus.Status = status;
    PptCompleteRequest(Irp, IO_NO_INCREMENT);

    PptReleaseRemoveLock(&extension->RemoveLock, Irp);

    return status;
}

BOOLEAN
PptPnpFilterExistsNonIrqResourceList(
    IN PIO_RESOURCE_REQUIREMENTS_LIST ResourceRequirementsList
    )
/*++dvdf8

Routine Description:

    This function is a helper function called by 
      PptPnpFilterResourceRequirements(). 

    This function scans the IO_RESOURCE_REQUIREMENTS_LIST to determine
      whether there exists any resource alternatives that do NOT contain
      an IRQ resource descriptor. The method used to filter out IRQ
      resources may differ based on whether or not there exists a
      resource alternative that does not contain an IRQ resource
      descriptor.

Arguments:

    ResourceRequirementsList - The list to scan.

Return Value:

    TRUE  - There exists at least one resource alternative in the list that
              does not contain an IRQ resource descriptor.
    FALSE - Otherwise.           

--*/
{
    ULONG listCount = ResourceRequirementsList->AlternativeLists;
    PIO_RESOURCE_LIST curList;
    ULONG i;

    PptDump2(PARRESOURCE,("Enter PptPnpFilterExistsNonIrqResourceList() - AlternativeLists= %d\n", listCount));

    i=0;
    curList = ResourceRequirementsList->List;
    while( i < listCount ) {
        PptDump2(PARRESOURCE,("Searching List i=%d for an IRQ, curList= %x\n", i,curList));
        {
            ULONG                   remain   = curList->Count;
            PIO_RESOURCE_DESCRIPTOR curDesc  = curList->Descriptors;
            BOOLEAN                 foundIrq = FALSE;
            while( remain ) {
                PptDump2(PARRESOURCE,(" curDesc= %x , remain=%d\n", curDesc, remain));
                if(curDesc->Type == CmResourceTypeInterrupt) {
                    PptDump2(PARRESOURCE,(" Found IRQ - skip to next list\n"));
                    foundIrq = TRUE;
                    break;
                }
                ++curDesc;
                --remain;
            }
            if( foundIrq == FALSE ) {
                //
                // We found a resource list that does not contain an IRQ resource. 
                //   Our search is over.
                //
                PptDump2(PARRESOURCE,(" Found a list with NO IRQ - return TRUE from PptPnpFilterExistsNonIrqResourceList\n"));
                return TRUE;
            }
        }
        //
        // The next list starts immediately after the last descriptor of the current list.
        //
        curList = (PIO_RESOURCE_LIST)(curList->Descriptors + curList->Count);
        ++i;
    }

    //
    // All resource alternatives contain at least one IRQ resource descriptor.
    //
    PptDump2(PARRESOURCE,("all lists contain IRQs - return FALSE from PptPnpFilterExistsNonIrqResourceList\n"));
    return FALSE;
}

VOID
PptPnpFilterRemoveIrqResourceLists(
    PIO_RESOURCE_REQUIREMENTS_LIST ResourceRequirementsList
    )
/*++dvdf8

Routine Description:

    This function is a helper function called by 
      PptPnpFilterResourceRequirements(). 

    This function removes all resource alternatives (IO_RESOURCE_LISTs) 
      that contain IRQ resources from the IO_RESOURCE_REQUIREMENTS_LIST 

Arguments:

    ResourceRequirementsList - The list to process.

Return Value:

    none.

--*/
{
    ULONG listCount = ResourceRequirementsList->AlternativeLists;
    PIO_RESOURCE_LIST curList;
    PIO_RESOURCE_LIST nextList;
    ULONG i;
    PCHAR currentEndOfResourceRequirementsList;
    LONG bytesToMove;

    PptDump2(PARRESOURCE,("Enter PptPnpFilterRemoveIrqResourceLists() - AlternativeLists= %d\n", listCount));

    //
    // We use the end of the list to compute the size of the memory
    //   block to move when we remove a resource alternative from the
    //   list of lists.
    //
    currentEndOfResourceRequirementsList = PptPnpFilterGetEndOfResourceRequirementsList(ResourceRequirementsList);

    i=0;
    curList = ResourceRequirementsList->List;

    //
    // Walk through the IO_RESOURCE_LISTs.
    //
    while( i < listCount ) {

        PptDump2(PARRESOURCE,("\n"));
        PptDump2(PARRESOURCE,("TopOfLoop: i=%d listCount=%d curList= %#x , curEndOfRRL= %#x\n",
                              i,listCount,curList,currentEndOfResourceRequirementsList));

        if( PptPnpListContainsIrqResourceDescriptor(curList) ) {
            //
            // The current list contains IRQ, remove it by shifting the 
            //   remaining lists into its place and decrementing the list count.
            //

            PptDump2(PARRESOURCE,("list contains an IRQ - Removing List\n"));

            //
            // Get a pointer to the start of the next list.
            //
            nextList = (PIO_RESOURCE_LIST)(curList->Descriptors + curList->Count);

            //
            // compute the number of bytes to move
            //
            bytesToMove = (LONG)(currentEndOfResourceRequirementsList - (PCHAR)nextList);

            //
            // if (currentEndOfResourceRequirementsList == next list), 
            //   then this is the last list so there is nothing to move.
            //
            if( bytesToMove > 0 ) {
                //
                // More lists remain - shift them into the hole.
                //
                RtlMoveMemory(curList, nextList, bytesToMove);

                //
                // Adjust the pointer to the end of of the 
                //   IO_RESOURCE_REQUIREMENTS_LIST (list of lists) due to the shift.
                //
                currentEndOfResourceRequirementsList -= ( (PCHAR)nextList - (PCHAR)curList );
            }

            //
            // Note that we removed an IO_RESOURCE_LIST from the IO_RESOURCE_REQUIREMENTS_LIST.
            //
            --listCount;

        } else {
            //
            // The current list does not contain an IRQ resource, advance to next list.
            //
            PptDump2(PARRESOURCE,("list does not contain an IRQ - i=%d listCount=%d curList= %#x\n", i,listCount,curList));
            curList = (PIO_RESOURCE_LIST)(curList->Descriptors + curList->Count);
            ++i;
        }
    }

    //
    // Note the post filtered list count in the ResourceRequirementsList.
    //
    ResourceRequirementsList->AlternativeLists = listCount;

    PptDump2(PARRESOURCE,("Leave PptPnpFilterRemoveIrqResourceLists() - AlternativeLists= %d\n", listCount));

    return;
}

PVOID
PptPnpFilterGetEndOfResourceRequirementsList(
    IN PIO_RESOURCE_REQUIREMENTS_LIST ResourceRequirementsList
    )
/*++dvdf8

Routine Description:

    This function is a helper function called by PptPnpFilterRemoveIrqResourceLists()

    This function finds the end of an IO_RESOURCE_REQUIREMENTS_LIST 
      (list of IO_RESOURCE_LISTs).

Arguments:

    ResourceRequirementsList - The list to scan.

Return Value:

    Pointer to the next address past the end of the IO_RESOURCE_REQUIREMENTS_LIST.

--*/
{
    ULONG listCount = ResourceRequirementsList->AlternativeLists;
    PIO_RESOURCE_LIST curList;
    ULONG i;

    i=0;
    curList = ResourceRequirementsList->List;
    while( i < listCount ) {
        //
        // Pointer arithmetic based on the size of an IO_RESOURCE_DESCRIPTOR.
        //
        curList = (PIO_RESOURCE_LIST)(curList->Descriptors + curList->Count);
        ++i;
    }
    return (PVOID)curList;
}

VOID
PptPnpFilterNukeIrqResourceDescriptorsFromAllLists(
    PIO_RESOURCE_REQUIREMENTS_LIST ResourceRequirementsList
    )
/*++dvdf8

Routine Description:

    This function is a helper function called by 
      PptPnpFilterResourceRequirements(). 

    This function "nukes" all IRQ resources descriptors
      in the IO_RESOURCE_REQUIREMENTS_LIST by changing the descriptor
      types from CmResourceTypeInterrupt to CmResourceTypeNull.

Arguments:

    ResourceRequirementsList - The list to process.

Return Value:

    none.

--*/
{
    ULONG             listCount = ResourceRequirementsList->AlternativeLists;
    ULONG             i         = 0;
    PIO_RESOURCE_LIST curList   = ResourceRequirementsList->List;

    PptDump2(PARRESOURCE,("Enter PptPnpFilterNukeIrqResourceDescriptorsFromAllLists()"
                          " - AlternativeLists= %d\n", listCount));

    //
    // Walk through the list of IO_RESOURCE_LISTs in the IO_RESOURCE_REQUIREMENTS list.
    //
    while( i < listCount ) {
        PptDump2(PARRESOURCE,("Nuking IRQs from List i=%d, curList= %x\n", i,curList));
        //
        // Nuke all IRQ resources from the current IO_RESOURCE_LIST.
        //
        PptPnpFilterNukeIrqResourceDescriptors( curList );
        curList = (PIO_RESOURCE_LIST)(curList->Descriptors + curList->Count);
        ++i;
    }
}

VOID
PptPnpFilterNukeIrqResourceDescriptors(
    PIO_RESOURCE_LIST IoResourceList
    )
/*++dvdf8

Routine Description:

    This function is a helper function called by 
      PptPnpFilterNukeIrqResourceDescriptorsFromAllLists().

    This function "nukes" all IRQ resources descriptors
      in the IO_RESOURCE_LIST by changing the descriptor
      types from CmResourceTypeInterrupt to CmResourceTypeNull.

Arguments:

    IoResourceList - The list to process.

Return Value:

    none.

--*/
{
    PIO_RESOURCE_DESCRIPTOR  pIoResourceDescriptorIn  = IoResourceList->Descriptors;
    ULONG                    i;

    //
    // Scan the descriptor list for Interrupt descriptors.
    //
    for (i = 0; i < IoResourceList->Count; ++i) {

        if (pIoResourceDescriptorIn->Type == CmResourceTypeInterrupt) {
            //
            // Found one - change resource type from Interrupt to Null.
            //
            pIoResourceDescriptorIn->Type = CmResourceTypeNull;
            PptDump2(PARRESOURCE,(" - giving up IRQ resource - MinimumVector: %d MaximumVector: %d\n",
                       pIoResourceDescriptorIn->u.Interrupt.MinimumVector,
                       pIoResourceDescriptorIn->u.Interrupt.MaximumVector) );
        }
        ++pIoResourceDescriptorIn;
    }
}

NTSTATUS
PptPnpQueryDeviceRelations(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    )
/*++

Routine Description:

    This function handles PnP IRP_MN_QUERY_DEVICE_RELATIONS.

Arguments:

    DeviceObject - The target device for the IRP
    Irp          - The IRP

Return Value:

    STATUS_SUCCESS              - on success,
    an appropriate error status - otherwise

--*/
{
    PDEVICE_EXTENSION    extension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION   irpSp     = IoGetCurrentIrpStackLocation( Irp );
    DEVICE_RELATION_TYPE type      = irpSp->Parameters.QueryDeviceRelations.Type;
    PDEVICE_RELATIONS    removalRelations;
    
    switch( type ) {
    case RemovalRelations:
        PptDump2(PARPNP1, ("pnp::PptPnpQueryDeviceRelations - RemovalRelations\n") );
        PptDumpRemovalRelationsList( extension );
        if( Irp->IoStatus.Information ) {
            // someone above us has already handled this - dvdf RMT
            PptDump2(PARPNP1, ("pnp::PptPnpQueryDeviceRelations - RemovalRelations - handled above us\n"));
        } else {
            removalRelations = PptPnpBuildRemovalRelations( extension );
            if( removalRelations ) {
                // report devices
                PptDump2(PARPNP1, ("pnp::PptPnpQueryDeviceRelations - RemovalRelations - reporting relations\n"));
                Irp->IoStatus.Status = STATUS_SUCCESS;
                Irp->IoStatus.Information = (ULONG_PTR)removalRelations;
            } else {
                // no devices to report
                PptDump2(PARPNP1, ("pnp::PptPnpQueryDeviceRelations - RemovalRelations - empty list - nothing to report\n"));
            }
        }

        break;
    case BusRelations:
        PptDump2(PARPNP1, ("pnp::PptPnpQueryDeviceRelations - BusRelations\n") );
        break;
    case EjectionRelations:
        PptDump2(PARPNP1, ("pnp::PptPnpQueryDeviceRelations - EjectionRelations\n") );
        break;
    case PowerRelations:
        PptDump2(PARPNP1, ("pnp::PptPnpQueryDeviceRelations - PowerRelations\n") );
        break;
    case TargetDeviceRelation:
        PptDump2(PARPNP1, ("pnp::PptPnpQueryDeviceRelations - TargetDeviceRelations\n") );
        break;
    default:
        PptDump2(PARPNP1, ("pnp::PptPnpQueryDeviceRelations - unrecognized Relations\n") );
        break;
    }

    return PptPnpPassThroughPnpIrpAndReleaseRemoveLock(DeviceObject->DeviceExtension, Irp);
}

NTSTATUS
PptPnpQueryStopDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    )
/*++dvdf8

Routine Description:

    This function handles PnP IRP_MN_QUERY_STOP_DEVICE.

    FAIL the request if there are open handles, SUCCEED otherwise.
    
    Other drivers may cache pointers to the parallel port registers that 
      they obtained via IOCTL_INTERNAL_GET_PARALLEL_PORT_INFO and there
      is currently no mechanism to find and inform all such drivers that the 
      parallel port registers have changed and their their cached pointers are 
      now invalid without breaking legacy drivers.

    This function is identical to PptPnpQueryStopDevice() except
      for the flag that gets set in extension->DeviceStateFlags.

Arguments:

    DeviceObject - The target device for the IRP
    Irp          - The IRP

Return Value:

    STATUS_SUCCESS     - No open handles - SUCCEED IRP
    STATUS_DEVICE_BUSY - Open handles - FAIL IRP

--*/
{
    NTSTATUS          status       = STATUS_SUCCESS;
    PDEVICE_EXTENSION extension    = DeviceObject->DeviceExtension;
    BOOLEAN           handlesOpen;

    //
    // RMT - dvdf - race condition - small timing window - sequence:
    //   1. Test indicates no open handles - decide to SUCCEED QUERY_STOP
    //   2. CREATE arrives and is SUCCEEDED - open handle
    //   3. We SUCCEED QUERY_STOP
    //   4. Client obtains register addresses via IOCTL
    //   5. PnP Rebalances us - registers change
    //   6. Client acquires port via IOCTL
    //   7. Client tries to access registers at pre-rebalance location
    //   8. BOOM!!!
    //

    ExAcquireFastMutex( &extension->OpenCloseMutex );
    handlesOpen = (BOOLEAN)( extension->OpenCloseRefCount > 0 );
    ExReleaseFastMutex( &extension->OpenCloseMutex );

    if( handlesOpen ) {
        
        status = STATUS_DEVICE_BUSY;
        PptFailRequest( Irp, status );
        PptReleaseRemoveLock( &extension->RemoveLock, Irp );

    } else {

        Irp->IoStatus.Status = STATUS_SUCCESS;
        status = PptPnpPassThroughPnpIrpAndReleaseRemoveLock( extension, Irp );

        ExAcquireFastMutex( &extension->ExtensionFastMutex );
        PptSetFlags( extension->DeviceStateFlags, ( PPT_DEVICE_STOP_PENDING | PPT_DEVICE_PAUSED ) );
        ExReleaseFastMutex( &extension->ExtensionFastMutex );
    }
    
    return status;
}

NTSTATUS
PptPnpCancelStopDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    ) 
/*++dvdf8

Routine Description:

    This function handles PnP IRP_MN_CANCEL_STOP_DEVICE.

    If we previously SUCCEEDed a QUERY_STOP (PPT_DEVICE_STOP_PENDING 
      flag is set) then we reset the appropriate device state flags 
      and resume normal operation. Otherwise treat this as an
      informational message. 

    This function is identical to PptPnpCancelRemoveDevice() except for
      the extension->DeviceStateFlags.

Arguments:

    DeviceObject - The target device for the IRP
    Irp          - The IRP

Return Value:

    Status returned from IoCallDriver.

--*/
{
    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;

    ExAcquireFastMutex( &extension->ExtensionFastMutex );
    if( extension->DeviceStateFlags & PPT_DEVICE_STOP_PENDING ) {
        PptClearFlags( extension->DeviceStateFlags, ( PPT_DEVICE_STOP_PENDING | PPT_DEVICE_PAUSED ) );
    }
    ExReleaseFastMutex( &extension->ExtensionFastMutex );

    Irp->IoStatus.Status = STATUS_SUCCESS;
    return PptPnpPassThroughPnpIrpAndReleaseRemoveLock( extension, Irp );
}

NTSTATUS
PptPnpStopDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    ) 
/*++dvdf8

Routine Description:

    This function handles PnP IRP_MN_STOP_DEVICE.

    We previously SUCCEEDed QUERY_STOP. Set flags
      to indicate that we are now STOPPED.

Arguments:

    DeviceObject - The target device for the IRP
    Irp          - The IRP

Return Value:

    Status returned from IoCallDriver.

--*/
{
    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;

    ExAcquireFastMutex( &extension->ExtensionFastMutex );

    //
    // Assert that we are in a STOP_PENDING state.
    //
    ASSERT( extension->DeviceStateFlags & PPT_DEVICE_STOP_PENDING );
    ASSERT( extension->DeviceStateFlags & PPT_DEVICE_PAUSED );

    //
    // PPT_DEVICE_PAUSED remains set
    //
    PptSetFlags( extension->DeviceStateFlags,   PPT_DEVICE_STOPPED );
    PptClearFlags( extension->DeviceStateFlags, ( PPT_DEVICE_STOP_PENDING | PPT_DEVICE_STARTED ) );

    ExReleaseFastMutex( &extension->ExtensionFastMutex );

    Irp->IoStatus.Status = STATUS_SUCCESS;
    return PptPnpPassThroughPnpIrpAndReleaseRemoveLock(extension, Irp);
}

NTSTATUS
PptPnpQueryRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp
    ) 
/*++dvdf8

Routine Description:

    This function handles PnP IRP_MN_QUERY_REMOVE_DEVICE.

    FAIL the request if there are open handles, SUCCEED otherwise.
    
    This function is identical to PptPnpQueryStopDevice() except
      for the flag that gets set in extension->DeviceStateFlags.

Arguments:

    DeviceObject - The target device for the IRP
    Irp          - The IRP

Return Value:

    STATUS_SUCCESS     - No open handles - SUCCEED IRP
    STATUS_DEVICE_BUSY - Open handles - FAIL IRP

--*/
{
    //
    // Always succeed query - PnP will veto Query Remove on our behalf if 
    //   there are open handles
    //

    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;

    DDPnP1(("-- QueryRemove\n"));

    PptDump2(PARPNP1, ("pnp::PptPnpQueryRemoveDevice - SUCCEED - as always\n"));

    ExAcquireFastMutex( &extension->ExtensionFastMutex );
    PptSetFlags( extension->DeviceStateFlags, ( PPT_DEVICE_REMOVE_PENDING | PPT_DEVICE_PAUSED ) );
    ExReleaseFastMutex( &extension->ExtensionFastMutex );

    Irp->IoStatus.Status = STATUS_SUCCESS;

    return PptPnpPassThroughPnpIrpAndReleaseRemoveLock( extension, Irp );

#if 0

    // original code

    NTSTATUS          status       = STATUS_SUCCESS;
    PDEVICE_EXTENSION extension    = DeviceObject->DeviceExtension;
    BOOLEAN           handlesOpen;

    //
    // RMT - dvdf - race condition - small timing window 
    //            - see PptPnpQueryStop() for details.
    //

    ExAcquireFastMutex( &extension->OpenCloseMutex );
    handlesOpen = (BOOLEAN)( extension->OpenCloseRefCount > 0 );
    ExReleaseFastMutex( &extension->OpenCloseMutex );

    if( handlesOpen ) {
        
        PptDump2(PARPNP1, ("pnp::PptPnpQueryRemoveDevice - FAIL - handlesOpen=%d\n", handlesOpen));
        status = STATUS_DEVICE_BUSY;
        PptFailRequest( Irp, status );
        PptReleaseRemoveLock( &extension->RemoveLock, Irp );

    } else {

        PptDump2(PARPNP1, ("pnp::PptPnpQueryRemoveDevice - SUCCEED\n"));
        Irp->IoStatus.Status = STATUS_SUCCESS;
        status = PptPnpPassThroughPnpIrpAndReleaseRemoveLock( extension, Irp );

        ExAcquireFastMutex( &extension->ExtensionFastMutex );
        PptSetFlags( extension->DeviceStateFlags, ( PPT_DEVICE_REMOVE_PENDING | PPT_DEVICE_PAUSED ) );
        ExReleaseFastMutex( &extension->ExtensionFastMutex );
    }
    
    return status;
#endif
}
NTSTATUS
PptPnpCancelRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    ) 
/*++dvdf8

Routine Description:

    This function handles PnP IRP_MN_CANCEL_REMOVE_DEVICE.

    If we previously SUCCEEDed a QUERY_REMOVE (PPT_DEVICE_REMOVE_PENDING 
      flag is set) then we reset the appropriate device state flags 
      and resume normal operation. Otherwise treat this as an
      informational message. 

    This function is identical to PptPnpCancelStopDevice() except for
      the extension->DeviceStateFlags.

Arguments:

    DeviceObject - The target device for the IRP
    Irp          - The IRP

Return Value:

    Status returned from IoCallDriver.

--*/
{
    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;

    DDPnP1(("-- CancelRemove\n"));

    ExAcquireFastMutex( &extension->ExtensionFastMutex );
    if( extension->DeviceStateFlags & PPT_DEVICE_REMOVE_PENDING ) {
        PptClearFlags( extension->DeviceStateFlags, ( PPT_DEVICE_REMOVE_PENDING | PPT_DEVICE_PAUSED ) );
    }
    ExReleaseFastMutex( &extension->ExtensionFastMutex );

    Irp->IoStatus.Status = STATUS_SUCCESS;
    return PptPnpPassThroughPnpIrpAndReleaseRemoveLock( extension, Irp );
}

NTSTATUS
PptPnpRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    ) 
/*++dvdf8

Routine Description:

    This function handles PnP IRP_MN_REMOVE_DEVICE.

    Notify those listening on our device interface GUID that 
      we have gone away, wait until all other IRPs that the
      device is processing have drained, and clean up.

Arguments:

    DeviceObject - The target device for the IRP
    Irp          - The IRP

Return Value:

    Status returned from IoCallDriver.

--*/
{
    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    NTSTATUS          status;

    DDPnP1(("-- RemoveDevice\n"));

    //
    // Set flags in our extension to indicate that we have received 
    //   IRP_MN_REMOVE_DEVICE so that we can fail new requests as appropriate.
    //
    ExAcquireFastMutex( &extension->ExtensionFastMutex );
    PptSetFlags( extension->DeviceStateFlags, PPT_DEVICE_REMOVED );
    ExReleaseFastMutex( &extension->ExtensionFastMutex );

    //
    // Unregister w/WMI
    //
    IoWMIRegistrationControl(DeviceObject, WMIREG_ACTION_DEREGISTER);

    //
    // Tell those listening on our device interface GUID that we have
    //   gone away. Ignore status from the call since we can do
    //   nothing on failure.
    //
    IoSetDeviceInterfaceState(&extension->SymbolicLinkName, FALSE);

    //
    // Pass the IRP down the stack and wait for all other IRPs
    //   that are being processed by the device to drain.
    //
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoSkipCurrentIrpStackLocation( Irp );
    status = IoCallDriver( extension->ParentDeviceObject, Irp );
    PptReleaseRemoveLockAndWait( &extension->RemoveLock, Irp );

    //
    // Clean up pool allocations
    // 
    PptCleanRemovalRelationsList( extension );
    RtlFreeUnicodeString( &extension->DeviceName);
    RtlFreeUnicodeString( &extension->SymbolicLinkName );
    if( extension->PnpInfo.PortName ) {
        ExFreePool( extension->PnpInfo.PortName );
        extension->PnpInfo.PortName = NULL;
    }

    //
    // Detach and delete our device object.
    //
    IoDetachDevice( extension->ParentDeviceObject );
    IoDeleteDevice( DeviceObject );
    
    return status;
}

NTSTATUS
PptPnpSurpriseRemoval(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp
    )
/*++dvdf6

Routine Description:

    This function handles PnP IRP_MN_SURPRISE_REMOVAL.

    Set flags accordingly in our extension, notify those 
      listening on our device interface GUID that 
      we have gone away, and pass the IRP down the
      driver stack.

Arguments:

    DeviceObject - The target device for the IRP
    Irp          - The IRP

Return Value:

    Status returned from IoCallDriver.

--*/
{
    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;

    //
    // Set flags in our extension to indicate that we have received 
    //   IRP_MN_SURPRISE_REMOVAL so that we can fail new requests 
    //   as appropriate.
    //
    ExAcquireFastMutex( &extension->ExtensionFastMutex );
    PptSetFlags( extension->DeviceStateFlags, PPT_DEVICE_SURPRISE_REMOVED );
    ExReleaseFastMutex( &extension->ExtensionFastMutex );

    //
    // Fail outstanding allocate/select requests for the port
    //
    {
        PIRP                nextIrp;
        KIRQL               cancelIrql;
        
        IoAcquireCancelSpinLock(&cancelIrql);
        
        while( !IsListEmpty( &extension->WorkQueue ) ) {
                
            nextIrp = CONTAINING_RECORD( extension->WorkQueue.Blink, IRP, Tail.Overlay.ListEntry );
            nextIrp->Cancel        = TRUE;
            nextIrp->CancelIrql    = cancelIrql;
            nextIrp->CancelRoutine = NULL;
            PptCancelRoutine( DeviceObject, nextIrp );
            
            // PptCancelRoutine() releases the cancel SpinLock so we need to reaquire
            IoAcquireCancelSpinLock( &cancelIrql );
        }
        
        IoReleaseCancelSpinLock( cancelIrql );
    }

    //
    // Tell those listening on our device interface GUID that we have
    //   gone away. Ignore status from the call since we can do
    //   nothing on failure.
    //
    IoSetDeviceInterfaceState(&extension->SymbolicLinkName, FALSE);

    //
    // Succeed, pass the IRP down the stack, and release the RemoveLock.
    //
    Irp->IoStatus.Status = STATUS_SUCCESS;
    return PptPnpPassThroughPnpIrpAndReleaseRemoveLock( extension, Irp );
}


BOOLEAN
PptPnpListContainsIrqResourceDescriptor(
    IN PIO_RESOURCE_LIST List
)
{
    ULONG i;
    PIO_RESOURCE_DESCRIPTOR curDesc = List->Descriptors;

    for(i=0; i<List->Count; ++i) {
        if(curDesc->Type == CmResourceTypeInterrupt) {
            return TRUE;
        } else {
            ++curDesc;
        }
    }
    return FALSE;
}

NTSTATUS
PptPnpBounceAndCatchPnpIrp(
    PDEVICE_EXTENSION Extension,
    PIRP              Irp
)
/*++

  Pass a PnP IRP down the stack to our parent and catch it on the way back
    up after it has been handled by the drivers below us in the driver stack.

--*/
{
    NTSTATUS       status;
    KEVENT         event;
    PDEVICE_OBJECT parentDevObj = Extension->ParentDeviceObject;

    PptDump2(PARRESOURCE,("PptBounceAndCatchPnpIrp()\n") );

    // setup
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp, PptSynchCompletionRoutine, &event, TRUE, TRUE, TRUE);

    // send
    status = IoCallDriver(parentDevObj, Irp);

    // wait for completion routine to signal that it has caught the IRP on
    //   its way back out
    KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);

    if (status == STATUS_PENDING) {
        // If IoCallDriver returned STATUS_PENDING, then we must
        //   extract the "real" status from the IRP
        status = Irp->IoStatus.Status;
    }

    return status;
}

NTSTATUS
PptPnpPassThroughPnpIrpAndReleaseRemoveLock(
    IN PDEVICE_EXTENSION Extension,
    IN PIRP              Irp
)
/*++

  Pass a PnP IRP down the stack to our parent, 
    release RemoveLock, and return status from IoCallDriver.

--*/
{
    NTSTATUS status;

    IoSkipCurrentIrpStackLocation(Irp);
    status = IoCallDriver(Extension->ParentDeviceObject, Irp);
    PptReleaseRemoveLock(&Extension->RemoveLock, Irp);
    return status;
}

NTSTATUS
PptPnpUnhandledIrp(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    )
/*++dvdf8

Routine Description:

    This function is the default handler for PnP IRPs. 
      All PnP IRPs that are not explicitly handled by another 
      routine (via an entry in the PptPnpDispatchFunctionTable[]) are
      handled by this routine.

     - Pass the IRP down the stack to the device below us in the
         driver stack and release our device RemoveLock. 

Arguments:

    DeviceObject - The target device for the IRP
    Irp          - The IRP

Return Value:

    STATUS_SUCCESS              - on success,
    an appropriate error status - otherwise

--*/
{
    return PptPnpPassThroughPnpIrpAndReleaseRemoveLock(DeviceObject->DeviceExtension, Irp);
}

