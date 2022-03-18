/*++

Copyright (c) 1997-1998 Microsoft Corporation, All Rights Reserved

Module Name:

    pnp.c

Abstract:

    This module contains general PnP and Power code for the i8042prt Driver.

Environment:

    Kernel mode.

Revision History:

--*/
#include "i8042prt.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, I8xAddDevice)
#pragma alloc_text(PAGE, I8xFilterResourceRequirements)
#pragma alloc_text(PAGE, I8xFindPortCallout)
#pragma alloc_text(PAGE, I8xManuallyRemoveDevice)
#pragma alloc_text(PAGE, I8xPnP)
#pragma alloc_text(PAGE, I8xPower)
#pragma alloc_text(PAGE, I8xRegisterDeviceInterface)
#pragma alloc_text(PAGE, I8xRemovePort)
#pragma alloc_text(PAGE, I8xSendIrpSynchronously) 
#endif

NTSTATUS
I8xAddDevice (
    IN PDRIVER_OBJECT   Driver,
    IN PDEVICE_OBJECT   PDO
    )
/*++

Routine Description:

    Adds a device to the stack and sets up the appropriate flags and 
    device extension for the newly created device.
    
Arguments:

    Driver - The driver object
    PDO    - the device that we are attaching ourselves on top of
    
Return Value:

    NTSTATUS result code.

--*/
{
    PCOMMON_DATA             commonData;
    IO_ERROR_LOG_PACKET      errorLogEntry;
    PDEVICE_OBJECT           device;
    NTSTATUS                 status = STATUS_SUCCESS;
    ULONG                    maxSize;

    PAGED_CODE();

    Print(DBG_PNP_TRACE, ("enter Add Device \n"));

    maxSize = sizeof(PORT_KEYBOARD_EXTENSION) > sizeof(PORT_MOUSE_EXTENSION) ?
              sizeof(PORT_KEYBOARD_EXTENSION) :
              sizeof(PORT_MOUSE_EXTENSION);

    status = IoCreateDevice(Driver,                 // driver
                            maxSize,                // size of extension
                            NULL,                   // device name
                            FILE_DEVICE_8042_PORT,  // device type  ?? unknown at this time!!!
                            0,                      // device characteristics
                            FALSE,                  // exclusive
                            &device                 // new device
                            );

    if (!NT_SUCCESS(status)) {
        return (status);
    }

    RtlZeroMemory(device->DeviceExtension, maxSize);

    commonData = GET_COMMON_DATA(device->DeviceExtension);
    commonData->TopOfStack = IoAttachDeviceToDeviceStack(device, PDO);

    ASSERT(commonData->TopOfStack);

    commonData->Self =          device;
    commonData->PDO =           PDO;
    commonData->PowerState =    PowerDeviceD0;

    KeInitializeSpinLock(&commonData->InterruptSpinLock);

    //
    // Initialize the data consumption timer
    //
    KeInitializeTimer(&commonData->DataConsumptionTimer);

    //
    // Initialize the port DPC queue to log overrun and internal
    // device errors.
    //
    KeInitializeDpc(
        &commonData->ErrorLogDpc,
        (PKDEFERRED_ROUTINE) I8042ErrorLogDpc,
        device
        );

    //
    // Initialize the device completion DPC for requests that exceed the
    // maximum number of retries.
    //
    KeInitializeDpc(
        &commonData->RetriesExceededDpc,
        (PKDEFERRED_ROUTINE) I8042RetriesExceededDpc,
        device
        );

    //
    // Initialize the device completion DPC for requests that have timed out
    //
    KeInitializeDpc(
        &commonData->TimeOutDpc,
        (PKDEFERRED_ROUTINE) I8042TimeOutDpc,
        device
        );

    //
    // Initialize the port completion DPC object in the device extension.
    // This DPC routine handles the completion of successful set requests.
    //
    IoInitializeDpcRequest(device, I8042CompletionDpc);

    device->Flags |= DO_BUFFERED_IO;
    device->Flags |= DO_POWER_PAGABLE;
    device->Flags &= ~DO_DEVICE_INITIALIZING;

    Print(DBG_PNP_TRACE, ("Add Device (0x%x)\n", status));

    return status;
}

NTSTATUS
I8xSendIrpSynchronously (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN Strict
    )
/*++

Routine Description:

    Generic routine to send an irp DeviceObject and wait for its return up the
    device stack.
    
Arguments:

    DeviceObject - The device object to which we want to send the Irp
    
    Irp - The Irp we want to send
    
Return Value:

    return code from the Irp
--*/
{
    KEVENT   event;
    NTSTATUS status;

    PAGED_CODE();

    KeInitializeEvent(&event,
                      SynchronizationEvent,
                      FALSE
                      );

    IoCopyCurrentIrpStackLocationToNext(Irp);

    IoSetCompletionRoutine(Irp,
                           I8xPnPComplete,
                           &event,
                           TRUE,
                           TRUE,
                           TRUE
                           );

    status = IoCallDriver(DeviceObject, Irp);

    //
    // Wait for lower drivers to be done with the Irp
    //
    if (status == STATUS_PENDING) {
       KeWaitForSingleObject(&event,
                             Executive,
                             KernelMode,
                             FALSE,
                             NULL
                             );
       status = Irp->IoStatus.Status;
    }

    if (!Strict && 
        (status == STATUS_NOT_SUPPORTED ||
         status == STATUS_INVALID_DEVICE_REQUEST)) {
        status = STATUS_SUCCESS;
    }

    return status;
}

NTSTATUS
I8xPnPComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PKEVENT Event
    )
/*++

Routine Description:

    Completion routine for all PnP IRPs
    
Arguments:

    DeviceObject - Pointer to the DeviceObject

    Irp - Pointer to the request packet
    
    Event - The event to set once processing is complete 

Return Value:

    STATUS_SUCCESSFUL if successful,
    an valid NTSTATUS error code otherwise

--*/
{
    PIO_STACK_LOCATION  stack;
    NTSTATUS            status;

    UNREFERENCED_PARAMETER (DeviceObject);

    status = STATUS_SUCCESS;
    stack = IoGetCurrentIrpStackLocation(Irp);

    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }

    KeSetEvent(Event, 0, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
I8xPnP (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This is the dispatch routine for PnP requests
Arguments:

    DeviceObject - Pointer to the device object

    Irp - Pointer to the request packet


Return Value:

    STATUS_SUCCESSFUL if successful,
    an valid NTSTATUS error code otherwise

--*/
{
    PPORT_KEYBOARD_EXTENSION   kbExtension;
    PPORT_MOUSE_EXTENSION      mouseExtension;
    PCOMMON_DATA               commonData;
    PIO_STACK_LOCATION         stack;
    NTSTATUS                   status = STATUS_SUCCESS;
    KIRQL                      oldIrql;

    PAGED_CODE();

    commonData = GET_COMMON_DATA(DeviceObject->DeviceExtension);
    stack = IoGetCurrentIrpStackLocation(Irp);

    Print(DBG_PNP_TRACE,
          ("I8xPnP (%s),  enter (min func=0x%x)\n",
          commonData->IsKeyboard ? "kb" : "mou",
          (ULONG) stack->MinorFunction
          ));

    switch (stack->MinorFunction) {
    case IRP_MN_START_DEVICE:

        //
        // The device is starting.
        //
        // We cannot touch the device (send it any non pnp irps) until a
        // start device has been passed down to the lower drivers.
        //
        status = I8xSendIrpSynchronously(commonData->TopOfStack, Irp, TRUE);

        if (NT_SUCCESS(status) && NT_SUCCESS(Irp->IoStatus.Status)) {
            //
            // As we are successfully now back from our start device
            // we can do work.

            ExAcquireFastMutexUnsafe(&Globals.DispatchMutex);

            if (commonData->Started) {
                Print(DBG_PNP_ERROR,
                      ("received 1+ starts on %s\n",
                      commonData->IsKeyboard ? "kb" : "mouse"
                      ));
            }
            else {
                //
                // commonData->IsKeyboard is set during
                //  IOCTL_INTERNAL_KEYBOARD_CONNECT to TRUE and 
                //  IOCTL_INTERNAL_MOUSE_CONNECT to FALSE
                //
                if (commonData->IsKeyboard) {
                    status = I8xKeyboardStartDevice(
                      (PPORT_KEYBOARD_EXTENSION) DeviceObject->DeviceExtension,
                      stack->Parameters.StartDevice.AllocatedResourcesTranslated
                      );
                }
                else {
                    status = I8xMouseStartDevice(
                      (PPORT_MOUSE_EXTENSION) DeviceObject->DeviceExtension,
                      stack->Parameters.StartDevice.AllocatedResourcesTranslated
                      );
                }
    
                if (NT_SUCCESS(status)) {
                    InterlockedIncrement(&Globals.StartedDevices);
                    commonData->Started = TRUE;
                }
            }

            ExReleaseFastMutexUnsafe(&Globals.DispatchMutex);
        }

        //
        // We must now complete the IRP, since we stopped it in the
        // completetion routine with MORE_PROCESSING_REQUIRED.
        //
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        break;

    case IRP_MN_FILTER_RESOURCE_REQUIREMENTS: 
        //
        // The general rule of thumb for handling this minor code is this:  
        //    add resources when the irp is going down the stack and
        //    remove resources when the irp is coming back up the stack
        //
        // The irp has the original resources on the way down.
        //
        status = I8xSendIrpSynchronously(commonData->TopOfStack, Irp, FALSE);

        if (NT_SUCCESS(status)) {
            status = I8xFilterResourceRequirements(DeviceObject,
                                                   Irp
                                                   );
        }

        if (!NT_SUCCESS(status)) {
           Print(DBG_PNP_ERROR,
                 ("error pending filter res req event (0x%x)\n",
                 status
                 ));
        }
   
        //
        // Irp->IoStatus.Information will contain the new i/o resource 
        // requirements list so leave it alone
        //
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        break;
    
    case IRP_MN_QUERY_PNP_DEVICE_STATE: 

        status = I8xSendIrpSynchronously(commonData->TopOfStack, Irp, FALSE);

        //
        // do stuff here...
        //
        if (NT_SUCCESS(status)) {
            (PNP_DEVICE_STATE) Irp->IoStatus.Information |=
                commonData->PnpDeviceState;
        }

        if (!NT_SUCCESS(status)) {
           Print(DBG_PNP_ERROR,
                 ("error pending query pnp device state event (0x%x)\n",
                 status
                 ));
        }
   
        //
        // Irp->IoStatus.Information will contain the new i/o resource 
        // requirements list so leave it alone
        //
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        break;

    //
    // Don't let either of the requests succeed, otherwise the kb/mouse
    // might be rendered useless.
    //
    //  NOTE: this behavior is particular to i8042prt.  Any other driver,
    //        especially any other keyboard or port driver, should 
    //        succeed the query remove or stop.  i8042prt has this different 
    //        behavior because of the shared I/O ports but independent interrupts.
    //
    //        FURTHERMORE, if you allow the query to succeed, it should be sent
    //        down the stack (see sermouse.sys for an example of how to do this)
    //
    case IRP_MN_QUERY_REMOVE_DEVICE:
    case IRP_MN_QUERY_STOP_DEVICE:
        status = (MANUALLY_REMOVED(commonData) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);    
        Irp->IoStatus.Status = status; 
        Irp->IoStatus.Information = 0;    
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        break;

    //
    // PnP rules dictate we send the IRP down to the PDO first
    //
    case IRP_MN_CANCEL_REMOVE_DEVICE:
    case IRP_MN_CANCEL_STOP_DEVICE:
        status = I8xSendIrpSynchronously(commonData->TopOfStack, Irp, FALSE);

        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;    
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        break;

    case IRP_MN_REMOVE_DEVICE:
        
        Print(DBG_PNP_TRACE, ("remove device\n"));

        if (commonData->Started && !MANUALLY_REMOVED(commonData)) {
            //
            // This should never happen.  The only way we can get a remove is if
            // a start has failed.  
            //
            //  NOTE:  Again, this should never happen for i8042prt, but any
            //         other input port driver should allow itself to be removed
            //         (see sermouse.sys on how to do this correctly)
            //
            Print(DBG_PNP_ERROR, ("Cannot remove a started device!!!\n"));
            ASSERT(FALSE);
        }
        
        if (commonData->Initialized) {
            IoWMIRegistrationControl(commonData->Self,
                                     WMIREG_ACTION_DEREGISTER
                                     );
        }

        ExAcquireFastMutexUnsafe(&Globals.DispatchMutex);
        if (commonData->IsKeyboard) {
            I8xKeyboardRemoveDevice(DeviceObject);
        }
        else {
            I8xMouseRemoveDevice(DeviceObject);
        }
        ExReleaseFastMutexUnsafe(&Globals.DispatchMutex);

        //
        // Nothing has been allocated or connected
        //
        IoSkipCurrentIrpStackLocation(Irp);
        IoCallDriver(commonData->TopOfStack, Irp);

        IoDetachDevice(commonData->TopOfStack); 
        IoDeleteDevice(DeviceObject);

        status = STATUS_SUCCESS;
        break;

    case IRP_MN_STOP_DEVICE:
    case IRP_MN_QUERY_DEVICE_RELATIONS:
    case IRP_MN_QUERY_INTERFACE:
    case IRP_MN_QUERY_CAPABILITIES:
    case IRP_MN_QUERY_DEVICE_TEXT:
    case IRP_MN_QUERY_RESOURCES:
    case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
    case IRP_MN_READ_CONFIG:
    case IRP_MN_WRITE_CONFIG:
    case IRP_MN_EJECT:
    case IRP_MN_SET_LOCK:
    case IRP_MN_QUERY_ID:
    default:
        //
        // Here the driver below i8042prt might modify the behavior of these IRPS
        // Please see PlugPlay documentation for use of these IRPs.
        //
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(commonData->TopOfStack, Irp);
        break;
    }

    Print(DBG_PNP_TRACE,
          ("I8xPnP (%s) exit (status=0x%x)\n",
          commonData->IsKeyboard ? "kb" : "mou",
          status
          ));

    return status;
}

LONG
I8xManuallyRemoveDevice(
    PCOMMON_DATA CommonData
    )
/*++

Routine Description:

    Invalidates CommonData->PDO's device state and sets the manually removed 
    flag
    
Arguments:

    CommonData - represent either the keyboard or mouse
    
Return Value:

    new device count for that particular type of device
    
--*/
{
    LONG deviceCount;

    PAGED_CODE();

    if (CommonData->IsKeyboard) {

        deviceCount = InterlockedDecrement(&Globals.AddedKeyboards);
        if (deviceCount < 1) {
            Print(DBG_PNP_INFO, ("clear kb (manually remove)\n"));
            CLEAR_KEYBOARD_PRESENT();
        }

    } else {

        deviceCount = InterlockedDecrement(&Globals.AddedMice);
        if (deviceCount < 1) {
            Print(DBG_PNP_INFO, ("clear mou (manually remove)\n"));
            CLEAR_MOUSE_PRESENT();
        }
        
    }

    CommonData->PnpDeviceState |= PNP_DEVICE_REMOVED | PNP_DEVICE_DONT_DISPLAY_IN_UI;
    IoInvalidateDeviceState(CommonData->PDO);

    return deviceCount;
}

#define PhysAddrCmp(a,b) ( (a).LowPart == (b).LowPart && (a).HighPart == (b).HighPart )

BOOLEAN
I8xRemovePort(
    IN PIO_RESOURCE_DESCRIPTOR ResDesc
    )
/*++

Routine Description:

    If the physical address contained in the ResDesc is not in the list of 
    previously seen physicall addresses, it is placed within the list.
    
Arguments:

    ResDesc - contains the physical address

Return Value:

    TRUE  - if the physical address was found in the list
    FALSE - if the physical address was not found in the list (and thus inserted
            into it)
--*/
{
    ULONG               i;
    PHYSICAL_ADDRESS   address;

    PAGED_CODE();

    if (Globals.ControllerData->KnownPortsCount == -1) {
        return FALSE;
    }

    address =  ResDesc->u.Port.MinimumAddress;
    for (i = 0; i < Globals.ControllerData->KnownPortsCount; i++) {
        if (PhysAddrCmp(address, Globals.ControllerData->KnownPorts[i])) {
            return TRUE;
        }
    }

    if (Globals.ControllerData->KnownPortsCount < MaximumPortCount) {
        Globals.ControllerData->KnownPorts[
            Globals.ControllerData->KnownPortsCount++] = address;
    }

    Print(DBG_PNP_INFO,
          ("Saw port [0x%08x %08x] - [0x%08x %08x]\n",
          address.HighPart,
          address.LowPart,
          ResDesc->u.Port.MaximumAddress.HighPart,
          ResDesc->u.Port.MaximumAddress.LowPart
          ));

    return FALSE;
}

NTSTATUS
I8xFindPortCallout(
    IN PVOID                        Context,
    IN PUNICODE_STRING              PathName,
    IN INTERFACE_TYPE               BusType,
    IN ULONG                        BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE           ControllerType,
    IN ULONG                        ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE           PeripheralType,
    IN ULONG                        PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    )
/*++

Routine Description:

    This is the callout routine sent as a parameter to
    IoQueryDeviceDescription.  It grabs the keyboard controller and
    peripheral configuration information.

Arguments:

    Context - Context parameter that was passed in by the routine
        that called IoQueryDeviceDescription.

    PathName - The full pathname for the registry key.

    BusType - Bus interface type (Isa, Eisa, Mca, etc.).

    BusNumber - The bus sub-key (0, 1, etc.).

    BusInformation - Pointer to the array of pointers to the full value
        information for the bus.

    ControllerType - The controller type (should be KeyboardController).

    ControllerNumber - The controller sub-key (0, 1, etc.).

    ControllerInformation - Pointer to the array of pointers to the full
        value information for the controller key.

    PeripheralType - The peripheral type (should be KeyboardPeripheral).

    PeripheralNumber - The peripheral sub-key.

    PeripheralInformation - Pointer to the array of pointers to the full
        value information for the peripheral key.


Return Value:

    None.  If successful, will have the following side-effects:

        - Sets DeviceObject->DeviceExtension->HardwarePresent.
        - Sets configuration fields in
          DeviceObject->DeviceExtension->Configuration.

--*/
{
    PUCHAR                          controllerData;
    NTSTATUS                        status = STATUS_UNSUCCESSFUL;
    ULONG                           i,
                                    listCount,
                                    portCount = 0;
    PIO_RESOURCE_LIST               pResList = (PIO_RESOURCE_LIST) Context;
    PIO_RESOURCE_DESCRIPTOR         pResDesc;
    PKEY_VALUE_FULL_INFORMATION     controllerInfo = NULL;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceDescriptor;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(PathName);
    UNREFERENCED_PARAMETER(BusType);
    UNREFERENCED_PARAMETER(BusNumber);
    UNREFERENCED_PARAMETER(BusInformation);
    UNREFERENCED_PARAMETER(ControllerType);
    UNREFERENCED_PARAMETER(ControllerNumber);
    UNREFERENCED_PARAMETER(PeripheralType);
    UNREFERENCED_PARAMETER(PeripheralNumber);
    UNREFERENCED_PARAMETER(PeripheralInformation);

    pResDesc = pResList->Descriptors + pResList->Count;
    controllerInfo = ControllerInformation[IoQueryDeviceConfigurationData];

    Print(DBG_PNP_TRACE, ("I8xFindPortCallout enter\n"));

    if (controllerInfo->DataLength != 0) {
        controllerData = ((PUCHAR) controllerInfo) + controllerInfo->DataOffset;
        controllerData += FIELD_OFFSET(CM_FULL_RESOURCE_DESCRIPTOR,
                                       PartialResourceList);

        listCount = ((PCM_PARTIAL_RESOURCE_LIST) controllerData)->Count;

        resourceDescriptor =
            ((PCM_PARTIAL_RESOURCE_LIST) controllerData)->PartialDescriptors;

        for (i = 0; i < listCount; i++, resourceDescriptor++) {
            switch(resourceDescriptor->Type) {
            case CmResourceTypePort:
                
                if (portCount < 2) {

                    Print(DBG_PNP_INFO, 
                          ("found port [0x%x 0x%x] with length %d\n",
                          resourceDescriptor->u.Port.Start.HighPart,
                          resourceDescriptor->u.Port.Start.LowPart,
                          resourceDescriptor->u.Port.Length
                          ));

                    pResDesc->Type = resourceDescriptor->Type;
                    pResDesc->Flags = resourceDescriptor->Flags;
                    pResDesc->ShareDisposition = CmResourceShareDeviceExclusive;

                    pResDesc->u.Port.Alignment = 1;
                    pResDesc->u.Port.Length =
                        resourceDescriptor->u.Port.Length;
                    pResDesc->u.Port.MinimumAddress.QuadPart =
                        resourceDescriptor->u.Port.Start.QuadPart;
                    pResDesc->u.Port.MaximumAddress.QuadPart = 
                        pResDesc->u.Port.MinimumAddress.QuadPart +
                        pResDesc->u.Port.Length - 1;

                    pResList->Count++;

                    //
                    // We want to record the ports we stole from the kb as seen
                    // so that if the keyboard is started later, we can trim
                    // its resources and not have a resource conflict...
                    //
                    // ...we are getting too smart for ourselves here :]
                    //
                    I8xRemovePort(pResDesc);
                    pResDesc++;
                }

                status = STATUS_SUCCESS;

                break;

            default:
                Print(DBG_PNP_NOISE, ("type 0x%x found\n",
                                      (LONG) resourceDescriptor->Type));
                break;
            }
        }

    }

    Print(DBG_PNP_TRACE, ("I8xFindPortCallout exit (0x%x)\n", status));
    return status;
}

NTSTATUS
I8xFilterResourceRequirements(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    Iterates through the resource requirements list contained in the IRP and removes
    any duplicate requests for I/O ports.  (This is a common problem on the Alphas.)
    
    No removal is performed if more than one resource requirements list is present.
    
Arguments:

    DeviceObject - A pointer to the device object

    Irp - A pointer to the request packet which contains the resource req. list.


Return Value:

    None.
    
--*/
{
    PIO_RESOURCE_REQUIREMENTS_LIST  pReqList = NULL,
                                    pNewReqList = NULL;
    PIO_RESOURCE_LIST               pResList = NULL,
                                    pNewResList = NULL;
    PIO_RESOURCE_DESCRIPTOR         pResDesc = NULL,
                                    pNewResDesc = NULL;
    ULONG                           i = 0, j = 0,
                                    removeCount,
                                    reqCount,
                                    size;
    BOOLEAN                         foundInt = FALSE,
                                    foundPorts = FALSE;

    PIO_STACK_LOCATION  stack;

    PAGED_CODE();

    ASSERT(DeviceObject);
    ASSERT(DeviceObject->DeviceExtension);

    Print(DBG_PNP_NOISE,
          ("Received IRP_MN_FILTER_RESOURCE_REQUIREMENTS for %s\n",
          (GET_COMMON_DATA(DeviceObject->DeviceExtension))->IsKeyboard ? "kb" : "mouse"
          ));

    stack = IoGetCurrentIrpStackLocation(Irp);

    //
    // The list can be in either the information field, or in the current
    //  stack location.  The Information field has a higher precedence over
    //  the stack location.
    //
    if (Irp->IoStatus.Information == 0) {
        pReqList =
            stack->Parameters.FilterResourceRequirements.IoResourceRequirementList;
        Irp->IoStatus.Information = (ULONG_PTR) pReqList;
    }
    else {
        pReqList = (PIO_RESOURCE_REQUIREMENTS_LIST) Irp->IoStatus.Information;
    }

    if (!pReqList) {
        // 
        // Not much can be done here except return
        //
        Print(DBG_PNP_MASK & ~ DBG_PNP_TRACE, 
              ("(%s) NULL resource list in I8xFilterResourceRequirements\n",
              (GET_COMMON_DATA(DeviceObject->DeviceExtension))->IsKeyboard ?
                  "kb" : "mou"
              ));

        return STATUS_SUCCESS;
    }

    ASSERT(Irp->IoStatus.Information != 0);
    ASSERT(pReqList != 0);

    reqCount = pReqList->AlternativeLists;

    //
    // Only one AlternativeList is supported.  If there is more than one list,
    // then there is now way of knowing which list will be chosen.  Also, if
    // there are multiple lists, then chances are that a list with no i/o port
    // conflicts will be chosen.
    //
    if (reqCount > 1) {
        return STATUS_SUCCESS;
    }

    pResList = pReqList->List;
    removeCount = 0;

    for (j = 0; j < pResList->Count; j++) {
        pResDesc = &pResList->Descriptors[j];
        switch (pResDesc->Type) {
        case CmResourceTypePort:
            Print(DBG_PNP_INFO, 
                  ("option = 0x%x, flags = 0x%x\n",
                  (LONG) pResDesc->Option,
                  (LONG) pResDesc->Flags
                  ));

            if (I8xRemovePort(pResDesc)) {
                //
                // Increment the remove count and tag this resource as
                // one that we don't want to copy to the new list
                //
                removeCount++;
                pResDesc->Type = I8X_REMOVE_RESOURCE;
            }

            foundPorts = TRUE;
            break;

        case CmResourceTypeInterrupt:
            if (Globals.ControllerData->Configuration.SharedInterrupts) {
                if (pResDesc->ShareDisposition != CmResourceShareShared) {
                    Print(DBG_PNP_INFO, ("forcing non shared int to shared\n"));
                }
                pResDesc->ShareDisposition = CmResourceShareShared;
            }

            foundInt = TRUE;
            break;

        default:
            break;
        }
    }

    if (removeCount) {
        size = pReqList->ListSize;

        // 
        // One element of the array is already allocated (via the struct 
        //  definition) so make sure that we are allocating at least that 
        //  much memory.
        //

        ASSERT(pResList->Count >= removeCount);
        if (pResList->Count > 1) {
            size -= removeCount * sizeof(IO_RESOURCE_DESCRIPTOR);
        }

        pNewReqList =
            (PIO_RESOURCE_REQUIREMENTS_LIST) ExAllocatePool(PagedPool, size);

        if (!pNewReqList) {
            //
            // This is not good, but the system doesn't really need to know about
            //  this, so just fix up our munging and return the original list
            //
            pReqList = stack->Parameters.FilterResourceRequirements.IoResourceRequirementList;
            reqCount = pReqList->AlternativeLists;
            removeCount = 0;
       
            for (i = 0; i < reqCount; i++) {
                pResList = &pReqList->List[i];
       
                for (j = 0; j < pResList->Count; j++) {
                    pResDesc = &pResList->Descriptors[j];
                    if (pResDesc->Type == I8X_REMOVE_RESOURCE) {
                        pResDesc->Type = CmResourceTypePort;
                    }
                }
            
            }

            return STATUS_SUCCESS;
        }

        //
        // Clear out the newly allocated list
        //
        RtlZeroMemory(pNewReqList,
                      size
                      );

        //
        // Copy the list header information except for the IO resource list
        // itself
        //
        RtlCopyMemory(pNewReqList,
                      pReqList,
                      sizeof(IO_RESOURCE_REQUIREMENTS_LIST) - 
                        sizeof(IO_RESOURCE_LIST)
                      );
        pNewReqList->ListSize = size;

        pResList = pReqList->List;
        pNewResList = pNewReqList->List;

        //
        // Copy the list header information except for the IO resource
        // descriptor list itself
        //
        RtlCopyMemory(pNewResList,
                      pResList,
                      sizeof(IO_RESOURCE_LIST) -
                        sizeof(IO_RESOURCE_DESCRIPTOR)
                      );

        pNewResList->Count = 0;
        pNewResDesc = pNewResList->Descriptors;

        for (j = 0; j < pResList->Count; j++) {
            pResDesc = &pResList->Descriptors[j];
            if (pResDesc->Type != I8X_REMOVE_RESOURCE) {
                //
                // Keep this resource, so copy it into the new list and
                // incement the count and the location for the next
                // IO resource descriptor
                //
                *pNewResDesc = *pResDesc;
                pNewResDesc++;
                pNewResList->Count++;

                Print(DBG_PNP_INFO,
                     ("List #%d, Descriptor #%d ... keeping res type %d\n",
                     i, j,
                     (ULONG) pResDesc->Type
                     ));
            }
            else {
                //
                // Decrement the remove count so we can assert it is
                //  zero once we are done
                //
                Print(DBG_PNP_INFO,
                      ("Removing port [0x%08x %08x] - [0x%#08x %08x]\n",
                      pResDesc->u.Port.MinimumAddress.HighPart,
                      pResDesc->u.Port.MinimumAddress.LowPart,
                      pResDesc->u.Port.MaximumAddress.HighPart,
                      pResDesc->u.Port.MaximumAddress.LowPart
                      ));
                removeCount--;
              }
        }

        ASSERT(removeCount == 0);

        //
        // There have been bugs where the old list was being used.  Zero it out to
        //  make sure that no conflicts arise.  (Not to mention the fact that some
        //  other code is accessing freed memory
        //
        RtlZeroMemory(pReqList,
                      pReqList->ListSize
                      );

        //
        // Free the old list and place the new one in its place
        //
        ExFreePool(pReqList);
        stack->Parameters.FilterResourceRequirements.IoResourceRequirementList =
            pNewReqList;
        Irp->IoStatus.Information = (ULONG_PTR) pNewReqList;
    }
    else if (!KEYBOARD_PRESENT() && !foundPorts && foundInt) {
        INTERFACE_TYPE                      interfaceType;
        NTSTATUS                            status;
        ULONG                               i,
                                            prevCount;
        CONFIGURATION_TYPE                  controllerType = KeyboardController;
        CONFIGURATION_TYPE                  peripheralType = KeyboardPeripheral;

        ASSERT( MOUSE_PRESENT() );

        Print(DBG_PNP_INFO, ("Adding ports to res list!\n"));

        //
        // We will now yank the resources from the keyboard to start the mouse
        // solo
        //
        size = pReqList->ListSize + 2 * sizeof(IO_RESOURCE_DESCRIPTOR);
        pNewReqList = (PIO_RESOURCE_REQUIREMENTS_LIST)
                        ExAllocatePool(
                            PagedPool,
                            size
                            );

        if (!pNewReqList) {
            return STATUS_SUCCESS;
        }

        //
        // Clear out the newly allocated list
        //
        RtlZeroMemory(pNewReqList,
                      size
                      );

        //
        // Copy the entire old list
        //
        RtlCopyMemory(pNewReqList,
                      pReqList,
                      pReqList->ListSize
                      );

        pResList = pReqList->List;
        pNewResList = pNewReqList->List;

        prevCount = pNewResList->Count;
        for (i = 0; i < MaximumInterfaceType; i++) {

            //
            // Get the registry information for this device.
            //
            interfaceType = i;
            status = IoQueryDeviceDescription(
                &interfaceType,
                NULL,
                &controllerType,
                NULL,
                &peripheralType,
                NULL,
                I8xFindPortCallout,
                (PVOID) pNewResList
                );

            if (NT_SUCCESS(status)) {
                break;
            }
        }

        if (NT_SUCCESS(status)) {
            pNewReqList->ListSize = size - (2 - (pNewResList->Count - prevCount));
    
            //
            // Free the old list and place the new one in its place
            //
            ExFreePool(pReqList);
            stack->Parameters.FilterResourceRequirements.IoResourceRequirementList =
                pNewReqList;
            Irp->IoStatus.Information = (ULONG_PTR) pNewReqList;
        }
        else {
            ExFreePool(pNewReqList);
        }
    }

    return STATUS_SUCCESS;
}


NTSTATUS
I8xRegisterDeviceInterface(
    PDEVICE_OBJECT PDO,
    CONST GUID * Guid,
    PUNICODE_STRING SymbolicName
    )
{
    NTSTATUS status;

    PAGED_CODE();

    status = IoRegisterDeviceInterface(
                PDO,
                Guid,
                NULL,
                SymbolicName 
                );

    if (NT_SUCCESS(status)) {
        status = IoSetDeviceInterfaceState(SymbolicName,
                                           TRUE
                                           );
    }

    return status;
}


NTSTATUS
I8xPower (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This is the dispatch routine for power requests.  

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    STATUS_SUCCESSFUL if successful,
    an valid NTSTATUS error code otherwise

--*/
{
    PCOMMON_DATA        commonData;
    PIO_STACK_LOCATION  stack;
    NTSTATUS            status = STATUS_SUCCESS;

    PAGED_CODE();

    commonData = GET_COMMON_DATA(DeviceObject->DeviceExtension);

    stack = IoGetCurrentIrpStackLocation(Irp);

    Print(DBG_POWER_TRACE,
          ("Power (%s), enter\n",
          commonData->IsKeyboard ? "keyboard" :
                                   "mouse"
          ));

    switch(stack->MinorFunction) {
    case IRP_MN_WAIT_WAKE:
        Print(DBG_POWER_NOISE, ("Got IRP_MN_WAIT_WAKE\n" ));
        break;

    case IRP_MN_POWER_SEQUENCE:
        Print(DBG_POWER_NOISE, ("Got IRP_MN_POWER_SEQUENCE\n" ));
        break;

    case IRP_MN_SET_POWER:
        Print(DBG_POWER_NOISE, ("Got IRP_MN_SET_POWER\n" ));

        //
        // Don't handle anything but DevicePowerState changes
        //
        if (stack->Parameters.Power.Type != DevicePowerState) {
            Print(DBG_POWER_INFO, ("not a device power irp\n"));
            break;
        }

        //
        // Check for no change in state, and if none, do nothing
        //
        if (stack->Parameters.Power.State.DeviceState ==
            commonData->PowerState) {
            Print(DBG_POWER_INFO,
                  ("no change in state (PowerDeviceD%d)\n",
                  commonData->PowerState-1
                  ));
            break;
        }

        switch (stack->Parameters.Power.State.DeviceState) {
        case PowerDeviceD0:
            Print(DBG_POWER_INFO, ("Powering up to PowerDeviceD0\n"));

            commonData->IsKeyboard ? KEYBOARD_POWERED_UP_STARTED()
                                   : MOUSE_POWERED_UP_STARTED();
                                
            IoCopyCurrentIrpStackLocationToNext(Irp);
            IoSetCompletionRoutine(Irp,
                                   I8xPowerUpToD0Complete,
                                   NULL,
                                   TRUE,                // on success
                                   TRUE,                // on error
                                   TRUE                 // on cancel
                                   );

            //
            // PoStartNextPowerIrp() gets called when the irp gets completed 
            //
            IoMarkIrpPending(Irp);
            PoCallDriver(commonData->TopOfStack, Irp);

            return STATUS_PENDING;

        case PowerDeviceD1:
        case PowerDeviceD2:
        case PowerDeviceD3:
            Print(DBG_POWER_INFO,
                  ("Powering down to PowerDeviceD%d\n",
                  stack->Parameters.Power.State.DeviceState-1
                  ));

            PoSetPowerState(DeviceObject,
                            stack->Parameters.Power.Type,
                            stack->Parameters.Power.State
                            );
            commonData->PowerState = stack->Parameters.Power.State.DeviceState;
            commonData->ShutdownType = stack->Parameters.Power.ShutdownType;

            //
            // For what we are doing, we don't need a completion routine
            // since we don't race on the power requests.
            //
            Irp->IoStatus.Status = STATUS_SUCCESS;
            IoCopyCurrentIrpStackLocationToNext(Irp);  // skip ?

            PoStartNextPowerIrp(Irp);
            return  PoCallDriver(commonData->TopOfStack, Irp);

        default:
            Print(DBG_POWER_INFO, ("unknown state\n"));
            break;
        }
        break;

    case IRP_MN_QUERY_POWER:
        Print(DBG_POWER_NOISE, ("Got IRP_MN_QUERY_POWER\n" ));
        break;

    default:
        Print(DBG_POWER_NOISE,
              ("Got unhandled minor function (%d)\n",
              stack->MinorFunction
              ));
        break;
    }

    Print(DBG_POWER_TRACE, ("Power, exit\n"));

    PoStartNextPowerIrp(Irp);

    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(commonData->TopOfStack, Irp);
}

NTSTATUS
I8xPowerUpToD0Complete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
/*++

Routine Description:

    Reinitializes the i8042 haardware after any type of hibernation/sleep.
    
Arguments:

    DeviceObject - Pointer to the device object

    Irp - Pointer to the request
    
    Context - Context passed in from the funciton that set the completion
              routine. UNUSED.


Return Value:

    STATUS_SUCCESSFUL if successful,
    an valid NTSTATUS error code otherwise

--*/
{
    NTSTATUS            status;
    PCOMMON_DATA        commonData;
    PWORK_QUEUE_ITEM    item;
    KIRQL               irql;
    BOOLEAN             queueItem = FALSE; 

    UNREFERENCED_PARAMETER(Context);

    commonData = GET_COMMON_DATA(DeviceObject->DeviceExtension);

    Print(DBG_POWER_TRACE,
          ("PowerUpToD0Complete (%s), Enter\n",
          commonData->IsKeyboard ? "kb" : "mouse"
          ));

    KeAcquireSpinLock(&Globals.ControllerData->PowerUpSpinLock, &irql);

    Globals.PoweredUpDevices++;
    if (NT_SUCCESS(Irp->IoStatus.Status)) {
        
        commonData->IsKeyboard ? KEYBOARD_POWERED_UP_SUCCESSFULLY() 
                               : MOUSE_POWERED_UP_SUCCESSFULLY();

        commonData->OutstandingPowerIrp = Irp;
        status = STATUS_MORE_PROCESSING_REQUIRED;
                       
        if (Globals.PoweredUpDevices == Globals.StartedDevices) {
            queueItem = TRUE;
        }
        else {
            //
            // Do nothing until the other power up irp comes along
            //
            Print(DBG_POWER_INFO, ("Postponing hw init until next device\n"));
        }
    }
    else {
        commonData->IsKeyboard ? KEYBOARD_POWERED_UP_FAILED() 
                               : MOUSE_POWERED_UP_FAILED();

        status = Irp->IoStatus.Status;

        if (A_POWERED_UP_SUCCEEDED()) {
            //
            // If a device has been successfully powered up, then this is the 
            // last device to attemp to power up
            //
            ASSERT(Globals.PoweredUpDevices == Globals.StartedDevices);
            queueItem = TRUE;
        }
#if DBG
        else {
            //
            // Both devices failed to power up, assert this state 
            //
            if (commonData->IsKeyboard) {
                ASSERT(MOUSE_POWERED_UP_FAILED());
            }
            else {
                ASSERT(KEYBOARD_POWERED_UP_FAILED());
            }
        }
#endif // DBG
    }

    KeReleaseSpinLock(&Globals.ControllerData->PowerUpSpinLock, irql);

    if (queueItem) {
        item = (PWORK_QUEUE_ITEM) ExAllocatePool(NonPagedPool,
                                                 sizeof(WORK_QUEUE_ITEM));
    
        if (!item) {
            //
            // complete any queued power irps
            //
            Print(DBG_POWER_INFO | DBG_POWER_ERROR,
                  ("failed to alloc work item\n"));
    
            if (MOUSE_PRESENT() && Globals.MouseExtension) {
                Irp = Globals.MouseExtension->OutstandingPowerIrp;
                Globals.MouseExtension->OutstandingPowerIrp = NULL;
    
                if (Irp) {
                    Print(DBG_POWER_ERROR, ("completing mouse power irp 0x%x", Irp));
    
                    Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                    Irp->IoStatus.Information = 0x0;
                    IoCompleteRequest(Irp, IO_NO_INCREMENT);
                }
            }
    
            if (KEYBOARD_PRESENT() && Globals.KeyboardExtension) {
                Irp = Globals.KeyboardExtension->OutstandingPowerIrp;
                Globals.KeyboardExtension->OutstandingPowerIrp = NULL;
    
                if (Irp) {
                    Print(DBG_POWER_ERROR, ("completing kbd power irp 0x%x", Irp));
    
                    Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                    Irp->IoStatus.Information = 0x0;
                    IoCompleteRequest(Irp, IO_NO_INCREMENT);
                }
            }
    
            CLEAR_POWERUP_FLAGS();
        }
        else {
            if (MOUSE_STARTED()) {
                SET_RECORD_STATE(Globals.MouseExtension,
                                 RECORD_RESUME_FROM_POWER);
            }
    
            Print(DBG_POWER_INFO, ("queueing work item for init\n"));
    
            ExInitializeWorkItem(item, I8xReinitializeHardware, item);
            ExQueueWorkItem(item, DelayedWorkQueue);
        }
    }

    return status;
}

