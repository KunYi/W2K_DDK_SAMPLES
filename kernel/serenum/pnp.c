/*++
Copyright (c) 1998  Microsoft Corporation

Module Name:

    PNP.C

Abstract:

    This module contains contains the plugplay calls
    PNP / WDM BUS driver.


Environment:

    kernel mode only

Notes:


--*/

#include "pch.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, Serenum_AddDevice)
#pragma alloc_text (PAGE, Serenum_PnP)
#pragma alloc_text (PAGE, Serenum_FDO_PnP)
#pragma alloc_text (PAGE, Serenum_PDO_PnP)
#pragma alloc_text (PAGE, Serenum_PnPRemove)
//#pragma alloc_text (PAGE, Serenum_Remove)
#endif


NTSTATUS
Serenum_AddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT BusPhysicalDeviceObject
    )
/*++
Routine Description.
    A bus has been found.  Attach our FDO to it.
    Allocate any required resources.  Set things up.  And be prepared for the
    first ``start device.''

Arguments:
    BusPhysicalDeviceObject - Device object representing the bus.  That to which
        we attach a new FDO.

    DriverObject - This very self referenced driver.

--*/
{
    NTSTATUS            status;
    PDEVICE_OBJECT      deviceObject;
    PFDO_DEVICE_DATA    DeviceData;
    ULONG               nameLength;

    PAGED_CODE ();

    Serenum_KdPrint_Def (SER_DBG_PNP_TRACE, ("Add Device: 0x%x\n",
                                          BusPhysicalDeviceObject));
    //
    // Create our FDO
    //

    status = IoCreateDevice (
                    DriverObject,  // our driver object
                    sizeof (FDO_DEVICE_DATA), // device object extension size
                    NULL, // FDOs do not have names
                    FILE_DEVICE_BUS_EXTENDER,
                    0, // No special characteristics
                    TRUE, // our FDO is exclusive
                    &deviceObject); // The device object created

    if (NT_SUCCESS (status)) {
        DeviceData = (PFDO_DEVICE_DATA) deviceObject->DeviceExtension;
        RtlFillMemory (DeviceData, sizeof (FDO_DEVICE_DATA), 0);

        DeviceData->IsFDO = TRUE;
        DeviceData->DebugLevel = SER_DEFAULT_DEBUG_OUTPUT_LEVEL;
        DeviceData->Self = deviceObject;
        DeviceData->AttachedPDO = NULL;
        DeviceData->NumPDOs = 0;
        DeviceData->DeviceState = PowerDeviceD0;
        DeviceData->SystemState = PowerSystemWorking;
        DeviceData->LastSetPowerState = PowerDeviceD0;
        DeviceData->PDOLegacyEnumerated = FALSE;
        DeviceData->PDOForcedRemove = FALSE;
        DeviceData->PollingLocks = 0;
        DeviceData->Polling = 0;

        DeviceData->SystemWake=PowerSystemUnspecified;
        DeviceData->DeviceWake=PowerDeviceUnspecified;

        DeviceData->PollingWorker = IoAllocateWorkItem(deviceObject);

        if (DeviceData->PollingWorker == NULL) {
           Serenum_KdPrint(DeviceData, SER_DBG_SS_ERROR,
                           ("Insufficient memory for Polling Routine.\n"));

           return STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Initialize the timer used for dynamic detection of attachment
        // and removal of PNP devices.
        //
        KeInitializeTimerEx(
            &DeviceData->PollingTimer,
            NotificationTimer);

        DeviceData->Removed = FALSE;

        // Set the PDO for use with PlugPlay functions
        DeviceData->UnderlyingPDO = BusPhysicalDeviceObject;


        //
        // Attach our filter driver to the device stack.
        // the return value of IoAttachDeviceToDeviceStack is the top of the
        // attachment chain.  This is where all the IRPs should be routed.
        //
        // Our filter will send IRPs to the top of the stack and use the PDO
        // for all PlugPlay functions.
        //
        DeviceData->TopOfStack = IoAttachDeviceToDeviceStack (
                                        deviceObject,
                                        BusPhysicalDeviceObject);

        // Bias outstanding request to 1 so that we can look for a
        // transition to zero when processing the remove device PlugPlay IRP.
        DeviceData->OutstandingIO = 1;

        KeInitializeEvent(&DeviceData->RemoveEvent,
                          SynchronizationEvent,
                          FALSE); // initialized to not signalled

        deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
        deviceObject->Flags |= DO_POWER_PAGABLE;

        //
        // Tell the PlugPlay system that this device will need an interface
        // device class shingle.
        //
        // It may be that the driver cannot hang the shingle until it starts
        // the device itself, so that it can query some of its properties.
        // (Aka the shingles guid (or ref string) is based on the properties
        // of the device.)
        //
        status = IoRegisterDeviceInterface (
                    BusPhysicalDeviceObject,
                    (LPGUID) &GUID_SERENUM_BUS_ENUMERATOR,
                    NULL, // No ref string
                    &DeviceData->DevClassAssocName);

        if (!NT_SUCCESS (status)) {
            Serenum_KdPrint (DeviceData, SER_DBG_PNP_ERROR,
                          ("AddDevice: IoRegisterDCA failed (%x)", status));
            IoFreeWorkItem(DeviceData->PollingWorker);
            IoDetachDevice (DeviceData->TopOfStack);
            IoDeleteDevice (deviceObject);
            return status;
        }

        //
        // If for any reason you need to save values in a safe location that
        // clients of this DeviceClassAssociate might be interested in reading
        // here is the time to do so, with the function
        // IoOpenDeviceClassRegistryKey
        // the symbolic link name used is was returned in
        // DeviceData->DevClassAssocName (the same name which is returned by
        // IoGetDeviceClassAssociations and the SetupAPI equivs.
        //

#if DBG
      {
         PWCHAR deviceName = NULL;

         status = IoGetDeviceProperty (BusPhysicalDeviceObject,
                                       DevicePropertyPhysicalDeviceObjectName,
                                       0,
                                       NULL,
                                       &nameLength);

         if ((nameLength != 0) && (status == STATUS_BUFFER_TOO_SMALL)) {
            deviceName = ExAllocatePool (NonPagedPool, nameLength);

            if (NULL == deviceName) {
               goto someDebugStuffExit;
            }

            IoGetDeviceProperty (BusPhysicalDeviceObject,
                                 DevicePropertyPhysicalDeviceObjectName,
                                 nameLength, deviceName, &nameLength);

            Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
                             ("AddDevice: %x to %x->%x (%ws) \n",
                              deviceObject, DeviceData->TopOfStack,
                              BusPhysicalDeviceObject, deviceName));
         }

someDebugStuffExit:;
         if (deviceName != NULL) {
            ExFreePool(deviceName);
         }
      }
#endif

        //
        // Turn on the shingle and point it to the given device object.
        //
        status = IoSetDeviceInterfaceState (
                        &DeviceData->DevClassAssocName,
                        TRUE);

        if (!NT_SUCCESS (status)) {
            Serenum_KdPrint (DeviceData, SER_DBG_PNP_ERROR,
                          ("AddDevice: IoSetDeviceClass failed (%x)", status));
            return status;
        }

        DeviceData->PollingAllowed = 0;
        DeviceData->PollingNotQueued = 1;

        KeInitializeEvent(&DeviceData->PollingEvent, SynchronizationEvent,
                          FALSE);
        KeInitializeEvent(&DeviceData->PollStateEvent, SynchronizationEvent,
                          TRUE);

        KeInitializeDpc(
            &DeviceData->DPCPolling,
            (PKDEFERRED_ROUTINE) Serenum_PollingTimerRoutine,
            DeviceData);
    }

    return status;
}

NTSTATUS
Serenum_FDO_PnPComplete (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Pirp,
    IN PVOID            Context
    );

NTSTATUS
Serenum_PnP (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
/*++
Routine Description:
    Answer the plethora of Irp Major PnP IRPS.
--*/
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status;
    PCOMMON_DEVICE_DATA     commonData;
    KIRQL                   oldIrq;

    PAGED_CODE ();

    status = STATUS_SUCCESS;
    irpStack = IoGetCurrentIrpStackLocation (Irp);
    ASSERT (IRP_MJ_PNP == irpStack->MajorFunction);

    commonData = (PCOMMON_DEVICE_DATA) DeviceObject->DeviceExtension;

    if (commonData->Removed) {

        Serenum_KdPrint (commonData, SER_DBG_PNP_TRACE,
                      ("PNP: removed DO: %x got IRP: %x\n", DeviceObject, Irp));

        Irp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);

    } else if (commonData->IsFDO) {
        Serenum_KdPrint (commonData, SER_DBG_PNP_TRACE,
                      ("PNP: Functional DO: %x IRP: %x\n", DeviceObject, Irp));

        status = Serenum_FDO_PnP (
                    DeviceObject,
                    Irp,
                    irpStack,
                    (PFDO_DEVICE_DATA) commonData);

    } else {
        Serenum_KdPrint (commonData, SER_DBG_PNP_TRACE,
                      ("PNP: Physical DO: %x IRP: %x\n", DeviceObject, Irp));

        status = Serenum_PDO_PnP (
                    DeviceObject,
                    Irp,
                    irpStack,
                    (PPDO_DEVICE_DATA) commonData);
    }

    return status;
}

NTSTATUS
Serenum_FDO_PnP (
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp,
    IN PIO_STACK_LOCATION   IrpStack,
    IN PFDO_DEVICE_DATA     DeviceData
    )
/*++
Routine Description:
    Handle requests from the PlugPlay system for the BUS itself

    NB: the various Minor functions of the PlugPlay system will not be
    overlapped and do not have to be reentrant

--*/
{
    NTSTATUS    status;
    KIRQL       oldIrq;
    KEVENT      event;
    ULONG       length;
    ULONG       i;
    PLIST_ENTRY entry;
    PPDO_DEVICE_DATA    pdoData;
    PDEVICE_RELATIONS   relations;
    PIO_STACK_LOCATION  stack;
    PRTL_QUERY_REGISTRY_TABLE QueryTable = NULL;
    ULONG DebugLevelDefault = SER_DEFAULT_DEBUG_OUTPUT_LEVEL;

    PAGED_CODE ();

    status = Serenum_IncIoCount (DeviceData);
    if (!NT_SUCCESS (status)) {
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    stack = IoGetCurrentIrpStackLocation (Irp);

    switch (IrpStack->MinorFunction) {
    case IRP_MN_START_DEVICE:
        //
        // BEFORE you are allowed to ``touch'' the device object to which
        // the FDO is attached (that send an irp from the bus to the Device
        // object to which the bus is attached).   You must first pass down
        // the start IRP.  It might not be powered on, or able to access or
        // something.
        //

        Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE, ("Start Device\n"));

        if (DeviceData->Started) {
            Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
                ("Device already started\n"));
            status = STATUS_SUCCESS;
            break;
        }

        KeInitializeEvent (&event, NotificationEvent, FALSE);
        IoCopyCurrentIrpStackLocationToNext (Irp);

        IoSetCompletionRoutine (Irp,
                                Serenum_FDO_PnPComplete,
                                &event,
                                TRUE,
                                TRUE,
                                TRUE);

        status = IoCallDriver (DeviceData->TopOfStack, Irp);

        if (STATUS_PENDING == status) {
            // wait for it...

            status = KeWaitForSingleObject (&event,
                                            Executive,
                                            KernelMode,
                                            FALSE, // Not allertable
                                            NULL); // No timeout structure

            ASSERT (STATUS_SUCCESS == status);

            status = Irp->IoStatus.Status;
        }

        if (NT_SUCCESS(status)) {
            //
            // Now we can touch the lower device object as it is now started.
            //

            if (DeviceData->TopOfStack->Flags & DO_BUFFERED_IO) {
                DeviceObject->Flags |= DO_BUFFERED_IO;
            } else if (DeviceData->TopOfStack->Flags & DO_DIRECT_IO) {
                DeviceObject->Flags |= DO_DIRECT_IO;
            }

            //
            // Get the debug level from the registry
            //

            if (NULL == (QueryTable = ExAllocatePool(
                               PagedPool,
                               sizeof(RTL_QUERY_REGISTRY_TABLE)*2
                               ))) {
                Serenum_KdPrint (DeviceData, SER_DBG_PNP_ERROR,
                    ("Failed to allocate memory to query registy\n"));
                DeviceData->DebugLevel = DebugLevelDefault;
            } else {
                RtlZeroMemory(
                         QueryTable,
                         sizeof(RTL_QUERY_REGISTRY_TABLE)*2
                         );

                QueryTable[0].QueryRoutine = NULL;
                QueryTable[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
                QueryTable[0].EntryContext = &DeviceData->DebugLevel;
                QueryTable[0].Name      = L"DebugLevel";
                QueryTable[0].DefaultType   = REG_DWORD;
                QueryTable[0].DefaultData   = &DebugLevelDefault;
                QueryTable[0].DefaultLength= sizeof(ULONG);

                // CIMEXCIMEX: The rest of the table isn't filled in!

                if (!NT_SUCCESS(RtlQueryRegistryValues(
                    RTL_REGISTRY_SERVICES,
                    L"Serenum",
                    QueryTable,
                    NULL,
                    NULL))) {
                    Serenum_KdPrint (DeviceData,SER_DBG_PNP_ERROR,
                        ("Failed to get debug level from registry.  Using default\n"));
                    DeviceData->DebugLevel = DebugLevelDefault;
                }

                ExFreePool( QueryTable );
            }


            Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
                             ("Start Device: Device started successfully\n"));
            DeviceData->Started = TRUE;

            Serenum_StartPolling(DeviceData, SERENUM_STOP_LOCK);
        }

        //
        // We must now complete the IRP, since we stopped it in the
        // completetion routine with MORE_PROCESSING_REQUIRED.
        //
        break;

    case IRP_MN_QUERY_STOP_DEVICE:
        Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
            ("Query Stop Device\n"));

        //
        // Test to see if there are any PDO created as children of this FDO
        // If there are then conclude the device is busy and fail the
        // query stop.
        //
        // CIMEXCIMEX
        // We could do better, by seing if the children PDOs are actually
        // currently open.  If they are not then we could stop, get new
        // resouces, fill in the new resouce values, and then when a new client
        // opens the PDO use the new resources.  But this works for now.
        //
        if (DeviceData->AttachedPDO) {
            status = STATUS_UNSUCCESSFUL;

        } else {
            status = STATUS_SUCCESS;
        }

        Irp->IoStatus.Status = status;

        if (NT_SUCCESS(status)) {
           IoSkipCurrentIrpStackLocation (Irp);
           status = IoCallDriver (DeviceData->TopOfStack, Irp);
        } else {
          IoCompleteRequest(Irp, IO_NO_INCREMENT);
        }

        Serenum_DecIoCount (DeviceData);
        return status;

    case IRP_MN_STOP_DEVICE:
        Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE, ("Stop Device\n"));

        //
        // After the start IRP has been sent to the lower driver object, the
        // bus may NOT send any more IRPS down ``touch'' until another START
        // has occured.
        // What ever access is required must be done before the Irp is passed
        // on.
        //
        // Stop device means that the resources given durring Start device
        // are no revoked.  So we need to stop using them
        //

        if (DeviceData->Started) {
            Serenum_StopPolling(DeviceData, SERENUM_STOP_LOCK);
        }

        DeviceData->Started = FALSE;

        //
        // We don't need a completion routine so fire and forget.
        //
        // Set the current stack location to the next stack location and
        // call the next device object.
        //
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoSkipCurrentIrpStackLocation (Irp);
        status = IoCallDriver (DeviceData->TopOfStack, Irp);

        Serenum_DecIoCount (DeviceData);
        return status;

    case IRP_MN_REMOVE_DEVICE:
        Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE, ("Remove Device\n"));

        //
        // The PlugPlay system has detected the removal of this device.  We
        // have no choice but to detach and delete the device object.
        // (If we wanted to express and interest in preventing this removal,
        // we should have filtered the query remove and query stop routines.)
        //
        // Note! we might receive a remove WITHOUT first receiving a stop.
        // ASSERT (!DeviceData->Removed);

        // We will accept no new requests
        //
        DeviceData->Removed = TRUE;

        //
        // Complete any outstanding IRPs queued by the driver here.
        //

        //
        // Make the DCA go away.  Some drivers may choose to remove the DCA
        // when they receive a stop or even a query stop.  We just don't care.
        //
        IoSetDeviceInterfaceState (&DeviceData->DevClassAssocName, FALSE);

        //
        // Here if we had any outstanding requests in a personal queue we should
        // complete them all now.
        //
        // Note, the device is guarenteed stopped, so we cannot send it any non-
        // PNP IRPS.
        //

        //
        // Fire and forget
        //
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoSkipCurrentIrpStackLocation (Irp);
        IoCallDriver (DeviceData->TopOfStack, Irp);

        //
        // Wait for all outstanding requests to complete
        //
        Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
            ("Waiting for outstanding requests\n"));
        i = InterlockedDecrement (&DeviceData->OutstandingIO);

        ASSERT (0 < i);

        if (0 != InterlockedDecrement (&DeviceData->OutstandingIO)) {
            Serenum_KdPrint (DeviceData, SER_DBG_PNP_INFO,
                          ("Remove Device waiting for request to complete\n"));

            KeWaitForSingleObject (&DeviceData->RemoveEvent,
                                   Suspended,
                                   KernelMode,
                                   FALSE, // Not Alertable
                                   NULL); // No timeout
        }
        //
        // Free the associated resources
        //

        //
        // Detach from the underlying devices.
        //
        Serenum_KdPrint(DeviceData, SER_DBG_PNP_INFO,
                        ("IoDetachDevice: 0x%x\n", DeviceData->TopOfStack));
        IoDetachDevice (DeviceData->TopOfStack);

        //
        // Clean up any resources here
        //
        if (DeviceData->Started) {
            Serenum_StopPolling(DeviceData, SERENUM_STOP_LOCK);
        }

	if (DeviceData->PollingWorker != NULL) {
            IoFreeWorkItem(DeviceData->PollingWorker);
	    DeviceData->PollingWorker = NULL;
	}

        ExFreePool (DeviceData->DevClassAssocName.Buffer);
        Serenum_KdPrint(DeviceData, SER_DBG_PNP_INFO,
                        ("IoDeleteDevice: 0x%x\n", DeviceObject));

        //
        // Remove any PDO's we ejected
        //

        if (DeviceData->AttachedPDO != NULL) {
           ASSERT(DeviceData->NumPDOs == 1);

           Serenum_PnPRemove(DeviceData->AttachedPDO, DeviceData->PdoData);
           DeviceData->PdoData = NULL;
           DeviceData->AttachedPDO = NULL;
           DeviceData->NumPDOs = 0;
        }

        IoDeleteDevice(DeviceObject);

        return STATUS_SUCCESS;

    case IRP_MN_QUERY_DEVICE_RELATIONS:
        if (BusRelations != IrpStack->Parameters.QueryDeviceRelations.Type) {
            //
            // We don't support this
            //
            Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
                ("Query Device Relations - Non bus\n"));
            goto SER_FDO_PNP_DEFAULT;
        }

        Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
            ("Query Bus Relations\n"));
        Serenum_StopPolling(DeviceData, SERENUM_QDR_LOCK);

        // Check for new devices or if old devices still there.
        if (DeviceData->PollingPeriod != (ULONG)-1) {

            status = Serenum_ReenumerateDevices(Irp, DeviceData);
        }

        //
        // Tell the plug and play system about all the PDOs.
        //
        // There might also be device relations below and above this FDO,
        // so, be sure to propagate the relations from the upper drivers.
        //
        // No Completion routine is needed so long as the status is preset
        // to success.  (PDOs complete plug and play irps with the current
        // IoStatus.Status and IoStatus.Information as the default.)
        //

        //KeAcquireSpinLock (&DeviceData->Spin, &oldIrq);

        i = (0 == Irp->IoStatus.Information) ? 0 :
            ((PDEVICE_RELATIONS) Irp->IoStatus.Information)->Count;
        // The current number of PDOs in the device relations structure

        Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
                           ("#PDOS = %d + %d\n", i, DeviceData->NumPDOs));

        length = sizeof(DEVICE_RELATIONS) +
                ((DeviceData->NumPDOs + i) * sizeof (PDEVICE_OBJECT));

        relations = (PDEVICE_RELATIONS) ExAllocatePool (NonPagedPool, length);

        if (NULL == relations) {
           Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
           IoCompleteRequest(Irp, IO_NO_INCREMENT);
           Serenum_DecIoCount(DeviceData);
           return STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Copy in the device objects so far
        //
        if (i) {
            RtlCopyMemory (
                  relations->Objects,
                  ((PDEVICE_RELATIONS) Irp->IoStatus.Information)->Objects,
                  i * sizeof (PDEVICE_OBJECT));
        }
        relations->Count = DeviceData->NumPDOs + i;

        //
        // For each PDO on this bus add a pointer to the device relations
        // buffer, being sure to take out a reference to that object.
        // The PlugPlay system will dereference the object when it is done with
        // it and free the device relations buffer.
        //

        if (DeviceData->NumPDOs) {
            relations->Objects[relations->Count-1] = DeviceData->AttachedPDO;
            ObReferenceObject (DeviceData->AttachedPDO);
        }

        //
        // Set up and pass the IRP further down the stack
        //
        Irp->IoStatus.Status = STATUS_SUCCESS;

        if (0 != Irp->IoStatus.Information) {
            ExFreePool ((PVOID) Irp->IoStatus.Information);
        }
        Irp->IoStatus.Information = (ULONG_PTR)relations;

        IoSkipCurrentIrpStackLocation (Irp);
        status = IoCallDriver (DeviceData->TopOfStack, Irp);

        Serenum_StartPolling(DeviceData, SERENUM_QDR_LOCK);

        Serenum_DecIoCount (DeviceData);

        return status;

    case IRP_MN_QUERY_REMOVE_DEVICE:
        //
        // If we were to fail this call then we would need to complete the
        // IRP here.  Since we are not, set the status to SUCCESS and
        // call the next driver.
        //

        Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
            ("Query Remove Device\n"));
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoSkipCurrentIrpStackLocation (Irp);
        status = IoCallDriver (DeviceData->TopOfStack, Irp);
        Serenum_DecIoCount (DeviceData);
        return status;


    case IRP_MN_QUERY_CAPABILITIES: {

        PIO_STACK_LOCATION  irpSp;

        //
        // Send this down to the PDO first
        //

        KeInitializeEvent (&event, NotificationEvent, FALSE);
        IoCopyCurrentIrpStackLocationToNext (Irp);

        IoSetCompletionRoutine (Irp,
                                Serenum_FDO_PnPComplete,
                                &event,
                                TRUE,
                                TRUE,
                                TRUE);

        status = IoCallDriver (DeviceData->TopOfStack, Irp);

        if (STATUS_PENDING == status) {
            // wait for it...

            status = KeWaitForSingleObject (&event,
                                            Executive,
                                            KernelMode,
                                            FALSE, // Not allertable
                                            NULL); // No timeout structure

            ASSERT (STATUS_SUCCESS == status);

            status = Irp->IoStatus.Status;
        }

        if (NT_SUCCESS(status)) {

            irpSp = IoGetCurrentIrpStackLocation(Irp);

            DeviceData->SystemWake=irpSp->Parameters.DeviceCapabilities.Capabilities->SystemWake;
            DeviceData->DeviceWake=irpSp->Parameters.DeviceCapabilities.Capabilities->DeviceWake;
        }

        break;
    }



SER_FDO_PNP_DEFAULT:
    default:
        //
        // In the default case we merely call the next driver since
        // we don't know what to do.
        //
        Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE, ("Default Case\n"));

        //
        // Fire and Forget
        //
        IoSkipCurrentIrpStackLocation (Irp);

        //
        // Done, do NOT complete the IRP, it will be processed by the lower
        // device object, which will complete the IRP
        //

        status = IoCallDriver (DeviceData->TopOfStack, Irp);
        Serenum_DecIoCount (DeviceData);
        return status;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    Serenum_DecIoCount (DeviceData);
    return status;
}


NTSTATUS
Serenum_FDO_PnPComplete (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PVOID            Context
    )
/*++
Routine Description:
    A completion routine for use when calling the lower device objects to
    which our bus (FDO) is attached.

--*/
{
    UNREFERENCED_PARAMETER (DeviceObject);
    UNREFERENCED_PARAMETER (Irp);

    KeSetEvent ((PKEVENT) Context, 1, FALSE);
    // No special priority
    // No Wait

    return STATUS_MORE_PROCESSING_REQUIRED; // Keep this IRP
}

NTSTATUS
Serenum_PDO_PnP (IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp,
                 IN PIO_STACK_LOCATION IrpStack, IN PPDO_DEVICE_DATA DeviceData)
/*++
Routine Description:
    Handle requests from the PlugPlay system for the devices on the BUS

--*/
{
   PDEVICE_CAPABILITIES    deviceCapabilities;
   ULONG                   information;
   PWCHAR                  buffer;
   ULONG                   length, i, j;
   NTSTATUS                status;
   KIRQL                   oldIrq;
   HANDLE                  keyHandle;
   UNICODE_STRING          keyName;
   ULONG                   PollingPeriod;
   PWCHAR returnBuffer = NULL;

   PAGED_CODE();

   status = Irp->IoStatus.Status;

   //
   // NB: since we are a bus enumerator, we have no one to whom we could
   // defer these irps.  Therefore we do not pass them down but merely
   // return them.
   //

   switch (IrpStack->MinorFunction) {
   case IRP_MN_QUERY_CAPABILITIES:

      Serenum_KdPrint (DeviceData, SER_DBG_PNP_TRACE, ("Query Caps \n"));

      //
      // Get the packet.
      //

      deviceCapabilities=IrpStack->Parameters.DeviceCapabilities.Capabilities;

      //
      // Set the capabilities.
      //

      deviceCapabilities->Version = 1;
      deviceCapabilities->Size = sizeof (DEVICE_CAPABILITIES);

      //
      // We cannot wake the system.
      //

      deviceCapabilities->SystemWake = ((PFDO_DEVICE_DATA)DeviceData->ParentFdo->DeviceExtension)->SystemWake;
      deviceCapabilities->DeviceWake = ((PFDO_DEVICE_DATA)DeviceData->ParentFdo->DeviceExtension)->DeviceWake;

      //
      // We have no latencies
      //

      deviceCapabilities->D1Latency = 0;
      deviceCapabilities->D2Latency = 0;
      deviceCapabilities->D3Latency = 0;

      deviceCapabilities->UniqueID = FALSE;
      status = STATUS_SUCCESS;
      break;

   case IRP_MN_QUERY_DEVICE_TEXT: {
      if ((IrpStack->Parameters.QueryDeviceText.DeviceTextType
          != DeviceTextDescription) || DeviceData->DevDesc.Buffer == NULL) {
         break;
      }

      returnBuffer = ExAllocatePool(PagedPool, DeviceData->DevDesc.Length);

      if (returnBuffer == NULL) {
         status = STATUS_INSUFFICIENT_RESOURCES;
         break;
      }

      status = STATUS_SUCCESS;

      RtlCopyMemory(returnBuffer, DeviceData->DevDesc.Buffer,
                    DeviceData->DevDesc.Length);

      Serenum_KdPrint(DeviceData, SER_DBG_PNP_TRACE,
                            ("TextID: buf 0x%x\n", returnBuffer));

      Irp->IoStatus.Information = (ULONG_PTR)returnBuffer;
      break;
   }


   case IRP_MN_QUERY_ID:
      //
      // Query the IDs of the device
      //

      Serenum_KdPrint(DeviceData, SER_DBG_PNP_TRACE,
                      ("QueryID: 0x%x\n", IrpStack->Parameters.QueryId.IdType));

      switch (IrpStack->Parameters.QueryId.IdType) {


      case BusQueryInstanceID:
         //
         // Build an instance ID.  This is what PnP uses to tell if it has
         // seen this thing before or not.  Build it from the first hardware
         // id and the port number.
         //
         // NB since we do not incorperate the port number
         // this method does not produce unique ids;
         //
         // return 0000 for all devices and have the flag set to not unique
         //

         status = STATUS_SUCCESS;

         length = SERENUM_INSTANCE_IDS_LENGTH * sizeof(WCHAR);
         returnBuffer = ExAllocatePool(PagedPool, length);

         if (returnBuffer != NULL) {
            RtlCopyMemory(returnBuffer, SERENUM_INSTANCE_IDS, length);
         } else {
            status = STATUS_INSUFFICIENT_RESOURCES;
         }

         Serenum_KdPrint(DeviceData, SER_DBG_PNP_TRACE,
                      ("InstanceID: buf 0x%x\n", returnBuffer));

         Irp->IoStatus.Information = (ULONG_PTR)returnBuffer;
         break;


      //
      // The other ID's we just copy from the buffers and are done.
      //

      case BusQueryDeviceID:
      case BusQueryHardwareIDs:
      case BusQueryCompatibleIDs:
         {
            PUNICODE_STRING pId;
            status = STATUS_SUCCESS;

            switch (IrpStack->Parameters.QueryId.IdType) {
            case BusQueryDeviceID:
               pId = &DeviceData->DeviceIDs;
               break;

            case BusQueryHardwareIDs:
               pId = &DeviceData->HardwareIDs;
               break;

            case BusQueryCompatibleIDs:
               pId = &DeviceData->CompIDs;
               break;
            }

            buffer = pId->Buffer;

            if (buffer != NULL) {
               length = pId->Length;
               returnBuffer = ExAllocatePool(PagedPool, length);
               if (returnBuffer != NULL) {
#if DBG
                  RtlFillMemory(returnBuffer, length, 0xff);
#endif
                  RtlCopyMemory(returnBuffer, buffer, pId->Length);
               } else {
                  status = STATUS_INSUFFICIENT_RESOURCES;
               }
            }

            Serenum_KdPrint(DeviceData, SER_DBG_PNP_TRACE,
                            ("ID: Unicode 0x%x\n", pId));
            Serenum_KdPrint(DeviceData, SER_DBG_PNP_TRACE,
                            ("ID: buf 0x%x\n", returnBuffer));

            Irp->IoStatus.Information = (ULONG_PTR)returnBuffer;
         }
         break;
      }
      break;

      case IRP_MN_QUERY_BUS_INFORMATION: {
       PPNP_BUS_INFORMATION pBusInfo;

       ASSERTMSG("Serenum appears not to be the sole bus?!?",
                 Irp->IoStatus.Information == (ULONG_PTR)NULL);

       pBusInfo = ExAllocatePool(PagedPool, sizeof(PNP_BUS_INFORMATION));

       if (pBusInfo == NULL) {
          status = STATUS_INSUFFICIENT_RESOURCES;
          break;
       }

       pBusInfo->BusTypeGuid = GUID_BUS_TYPE_SERENUM;
       pBusInfo->LegacyBusType = PNPBus;

       //
       // We really can't track our bus number since we can be torn
       // down with our bus
       //

       pBusInfo->BusNumber = 0;

       Irp->IoStatus.Information = (ULONG_PTR)pBusInfo;
       status = STATUS_SUCCESS;
       break;
       }

   case IRP_MN_QUERY_DEVICE_RELATIONS:
      switch (IrpStack->Parameters.QueryDeviceRelations.Type) {
      case TargetDeviceRelation: {
         PDEVICE_RELATIONS pDevRel;

         //
         // No one else should respond to this since we are the PDO
         //

         ASSERT(Irp->IoStatus.Information == 0);

         if (Irp->IoStatus.Information != 0) {
            break;
         }


         pDevRel = ExAllocatePool(PagedPool, sizeof(DEVICE_RELATIONS));

         if (pDevRel == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
         }

         pDevRel->Count = 1;
         pDevRel->Objects[0] = DeviceObject;
         ObReferenceObject(DeviceObject);

         status = STATUS_SUCCESS;
         Irp->IoStatus.Information = (ULONG_PTR)pDevRel;
         break;
      }


      default:
         break;
      }

      break;

   case IRP_MN_START_DEVICE:
      Serenum_KdPrint(DeviceData, SER_DBG_PNP_TRACE, ("Start Device\n"));

      //
      // Here we do what ever initialization and ``turning on'' that is
      // required to allow others to access this device.
      //

      //
      // Set the polling period value in the registry for this device.
      // This value is the same as the Parent FDO's polling period.
      //

      status = IoOpenDeviceRegistryKey(DeviceObject, PLUGPLAY_REGKEY_DEVICE,
                                       STANDARD_RIGHTS_WRITE, &keyHandle);

      if (!NT_SUCCESS(status)) {
         //
         // This is a fatal error.  If we can't get to our registry key,
         // we are sunk.
         //
         Serenum_KdPrint(DeviceData, SER_DBG_SS_ERROR,
                          ("IoOpenDeviceRegistryKey failed - %x\n", status));
      } else {
         RtlInitUnicodeString(&keyName, L"DeviceDetectionTimeout");

         PollingPeriod =
            ((PFDO_DEVICE_DATA)DeviceData->ParentFdo->DeviceExtension)->
               PollingPeriod;

         //
         // Doesn't matter whether this works or not.
         //

         ZwSetValueKey(keyHandle, &keyName, 0, REG_DWORD, &PollingPeriod,
                       sizeof(ULONG));

         ZwClose(keyHandle);
      }

      DeviceData->Started = TRUE;
      status = STATUS_SUCCESS;
      break;

   case IRP_MN_STOP_DEVICE:
      Serenum_KdPrint(DeviceData, SER_DBG_PNP_TRACE, ("Stop Device\n"));

      //
      // Here we shut down the device.  The opposite of start.
      //

      DeviceData->Started = FALSE;
      status = STATUS_SUCCESS;
      break;

   case IRP_MN_REMOVE_DEVICE:
      Serenum_KdPrint(DeviceData, SER_DBG_PNP_TRACE, ("Remove Device\n"));

      //
      // Attached is only set to FALSE by the enumeration process.
      //
      if (!DeviceData->Attached) {

          status = Serenum_PnPRemove(DeviceObject, DeviceData);
      }
      else {
          //
          // Succeed the remove
          ///
          status = STATUS_SUCCESS;
      }

      break;

   case IRP_MN_QUERY_STOP_DEVICE:
      Serenum_KdPrint(DeviceData, SER_DBG_PNP_TRACE, ("Query Stop Device\n"));

      //
      // No reason here why we can't stop the device.
      // If there were a reason we should speak now for answering success
      // here may result in a stop device irp.
      //

      status = STATUS_SUCCESS;
      break;

   case IRP_MN_CANCEL_STOP_DEVICE:
      Serenum_KdPrint(DeviceData, SER_DBG_PNP_TRACE, ("Cancel Stop Device\n"));
      //
      // The stop was canceled.  Whatever state we set, or resources we put
      // on hold in anticipation of the forcoming STOP device IRP should be
      // put back to normal.  Someone, in the long list of concerned parties,
      // has failed the stop device query.
      //

      status = STATUS_SUCCESS;
      break;

   case IRP_MN_QUERY_REMOVE_DEVICE:
      Serenum_KdPrint(DeviceData, SER_DBG_PNP_TRACE, ("Query Remove Device\n"));
      //
      // Just like Query Stop only now the impending doom is the remove irp
      //
      status = STATUS_SUCCESS;
      break;

   case IRP_MN_CANCEL_REMOVE_DEVICE:
      Serenum_KdPrint(DeviceData, SER_DBG_PNP_TRACE, ("Cancel Remove Device"
                                                      "\n"));
      //
      // Clean up a remove that did not go through, just like cancel STOP.
      //
      status = STATUS_SUCCESS;
      break;

   case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
   case IRP_MN_READ_CONFIG:
   case IRP_MN_WRITE_CONFIG: // we have no config space
   case IRP_MN_EJECT:
   case IRP_MN_SET_LOCK:
   case IRP_MN_QUERY_INTERFACE: // We do not have any non IRP based interfaces.
   default:
      Serenum_KdPrint(DeviceData, SER_DBG_PNP_TRACE, ("PNP Not handled 0x%x\n",
                                                      IrpStack->MinorFunction));
      // For PnP requests to the PDO that we do not understand we should
      // return the IRP WITHOUT setting the status or information fields.
      // They may have already been set by a filter (eg acpi).
      break;
   }

   Irp->IoStatus.Status = status;
   IoCompleteRequest (Irp, IO_NO_INCREMENT);

   return status;
}

NTSTATUS
Serenum_PnPRemove (PDEVICE_OBJECT Device, PPDO_DEVICE_DATA PdoData)
/*++
Routine Description:
    The PlugPlay subsystem has instructed that this PDO should be removed.

    We should therefore
    - Complete any requests queued in the driver
    - If the device is still attached to the system,
      then complete the request and return.
    - Otherwise, cleanup device specific allocations, memory, events...
    - Call IoDeleteDevice
    - Return from the dispatch routine.

    Note that if the device is still connected to the bus (IE in this case
    the control panel has not yet told us that the serial device has
    disappeared) then the PDO must remain around, and must be returned during
    any query Device relaions IRPS.

--*/

{
   Serenum_KdPrint(PdoData, SER_DBG_PNP_TRACE,
                        ("Serenum_PnPRemove: 0x%x\n", Device));


   ASSERT(PdoData->Removed == FALSE);

   PdoData->Removed = TRUE;

    //
    // Complete any outstanding requests with STATUS_DELETE_PENDING.
    //
    // Serenum does not queue any irps at this time so we have nothing to do.
    //

    if (PdoData->Attached) {
        return STATUS_SUCCESS;
    }

    //
    // Free any resources.
    //

    RtlFreeUnicodeString(&PdoData->HardwareIDs);
    RtlFreeUnicodeString(&PdoData->CompIDs);
    RtlFreeUnicodeString(&PdoData->DeviceIDs);

    Serenum_KdPrint(PdoData, SER_DBG_PNP_INFO,
                        ("IoDeleteDevice: 0x%x\n", Device));

    IoDeleteDevice(Device);


    return STATUS_SUCCESS;
}



