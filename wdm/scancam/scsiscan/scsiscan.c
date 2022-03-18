

/*++

Copyright (C) Microsoft Corporation, 1997 - 1999

Module Name:

    scsiscan.c

Abstract:

    The scsi scanner class driver translates IRPs to SRBs with embedded CDBs
    and sends them to its devices through the port driver.

Author:

    Ray Patrick (raypat)

Environment:

    kernel mode only

Notes:

Revision History:

--*/
#define INITGUID

#include <stdio.h>
#include "stddef.h"
#include "wdm.h"
#include "scsi.h"
#include "ntddstor.h"
#include "ntddscsi.h"
#include "scsiscan.h"
#include "private.h"

#include <initguid.h>
#include <devguid.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, DriverEntry)
#pragma alloc_text(PAGE, SSPnp)
#pragma alloc_text(PAGE, SSPnpAddDevice)
#pragma alloc_text(PAGE, SSOpen)
#pragma alloc_text(PAGE, SSClose)
#pragma alloc_text(PAGE, SSReadWrite)
#pragma alloc_text(PAGE, SSDeviceControl)
#pragma alloc_text(PAGE, SSAdjustTransferSize)
#pragma alloc_text(PAGE, SSBuildTransferContext)
#pragma alloc_text(PAGE, SSCreateSymbolicLink)
#pragma alloc_text(PAGE, SSDestroySymbolicLink)
#pragma alloc_text(PAGE, SSUnload)
#endif

DEFINE_GUID(GUID_STI_DEVICE, 0xF6CBF4C0L, 0xCC61, 0x11D0, 0x84, 0xE5, 0x00, 0xA0, 0xC9, 0x27, 0x65, 0x27);

// Globals

ULONG NextDeviceInstance = 0;

//ULONG DebugTraceLevel = MIN_TRACE;
ULONG DebugTraceLevel = MAX_TRACE;

#define DBG_DEVIOCTL 1

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This routine initializes the scanner class driver. The driver
    opens the port driver by name and then receives configuration
    information used to attach to the scanner devices.

Arguments:

    DriverObject

Return Value:

    NT Status

--*/
{

    PAGED_CODE();

#if DBG
    MyDebugInit(RegistryPath);
#endif

    SCSISCAN_KdPrint(MAX_TRACE,("SCSI Scanner Class driver entry called\n"));

    //
    // Set up the device driver entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_READ] = SSReadWrite;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = SSReadWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SSDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = SSOpen;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = SSClose;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = SSPnp;
    DriverObject->MajorFunction[IRP_MJ_PNP] = SSPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = SSPower;
    DriverObject->DriverUnload = SSUnload;
    DriverObject->DriverExtension->AddDevice = SSPnpAddDevice;
    return STATUS_SUCCESS;

} // end DriverEntry



NTSTATUS
SSPnpAddDevice(
    IN PDRIVER_OBJECT pDriverObject,
    IN PDEVICE_OBJECT pPhysicalDeviceObject
    )
/*++

Routine Description:

    This routine is called to create a new instance of the device.

Arguments:

    pDriverObject - pointer to the driver object for this instance of SS
    pPhysicalDeviceObject - pointer to the device object that represents the scanner
    on the scsi bus.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise

--*/
{
    UCHAR                       aName[64];
    ANSI_STRING                 ansiName;
    UNICODE_STRING              uName;
    PDEVICE_OBJECT              pDeviceObject = NULL;
    NTSTATUS                    Status;
    PSCSISCAN_DEVICE_EXTENSION  pde;

    PAGED_CODE();

    SCSISCAN_KdPrint(MAX_TRACE,("SSPnpAddDevice called\n"));

    //
    // Create the Functional Device Object (FDO) for this device.
    //

    sprintf(aName,"\\Device\\Scanner%d",NextDeviceInstance);
    RtlInitAnsiString(&ansiName, aName);
    SCSISCAN_KdPrint(MIN_TRACE,("Create device object %s\n", aName));
    RtlAnsiStringToUnicodeString(&uName, &ansiName, TRUE);

    //
    // Create device object for this scanner.
    //

    Status = IoCreateDevice(pDriverObject,
                            sizeof(SCSISCAN_DEVICE_EXTENSION),
                            &uName,
                            FILE_DEVICE_SCANNER,
                            0,
                            FALSE,
                            &pDeviceObject);

    RtlFreeUnicodeString(&uName);

    if (!NT_SUCCESS(Status)) {
        SCSISCAN_KdPrint(MIN_TRACE,("Can't create device object\n"));
        DEBUG_BREAKPOINT();
        return Status;
    }

    //
    // Indicate that IRPs should include MDLs and it's pawer pagable.
    //

    pDeviceObject->Flags |=  DO_DIRECT_IO;
    pDeviceObject->Flags |=  DO_POWER_PAGABLE;

    pde = (PSCSISCAN_DEVICE_EXTENSION)(pDeviceObject -> DeviceExtension);
    RtlZeroMemory(pde, sizeof(SCSISCAN_DEVICE_EXTENSION));

    //
    // Attach our new FDO to the PDO (Physical Device Object).
    //

    pde -> pStackDeviceObject = IoAttachDeviceToDeviceStack(pDeviceObject,
                                                            pPhysicalDeviceObject);
    if (NULL == pde -> pStackDeviceObject) {
        SCSISCAN_KdPrint(MIN_TRACE,("Cannot attach FDO to PDO.\n"));
        DEBUG_BREAKPOINT();
        IoDeleteDevice( pDeviceObject );
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Remember the PDO in our device extension.
    //

    pde -> pPhysicalDeviceObject = pPhysicalDeviceObject;

    //
    // Remember the DeviceInstance number.
    //

    pde -> DeviceInstance = NextDeviceInstance;

    pde->LastSrbError = 0L;

    //
    // Disable synchronous transfer for scanner requests.
    //

    pde -> SrbFlags = SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    //
    // Set timeout value in half seconds.
    //

    pde -> TimeOutValue = 60;

    //
    // Handle exporting interface
    //

    Status = ScsiScanHandleInterface(
        pPhysicalDeviceObject,
        &pde->InterfaceNameString,
        TRUE
        );

    //
    // Each time AddDevice gets called, we advance the global DeviceInstance variable.
    //

    NextDeviceInstance++;

    //
    // Finishing initialize.
    //

    pDeviceObject -> Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;

} // end SSPnpAddDevice()


NTSTATUS SSPnp (
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP           pIrp
   )
/*++

Routine Description:

    This routine handles all PNP irps.

Arguments:

    pDevciceObject - represents a scsi scanner device
    pIrp - PNP irp

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise

--*/
{
    NTSTATUS                      Status;
    PSCSISCAN_DEVICE_EXTENSION    pde;
    PIO_STACK_LOCATION            pIrpStack;
    STORAGE_PROPERTY_ID           PropertyId;
    KEVENT                        event;
    PDEVICE_CAPABILITIES          pCaps;

    PAGED_CODE();

    SCSISCAN_KdPrint(MAX_TRACE,("SSPnp called\n"));

    pde = (PSCSISCAN_DEVICE_EXTENSION)pDeviceObject -> DeviceExtension;
    pIrpStack = IoGetCurrentIrpStackLocation( pIrp );

    Status = STATUS_SUCCESS;

    switch (pIrpStack -> MajorFunction) {

        case IRP_MJ_SYSTEM_CONTROL:
            SCSISCAN_KdPrint(MIN_TRACE,("IRP_MJ_SYSTEM_CONTROL\n"));
            IoCopyCurrentIrpStackLocationToNext( pIrp );
            Status = IoCallDriver(pde -> pStackDeviceObject, pIrp);
            return Status;
            break;

        case IRP_MJ_PNP:
            switch (pIrpStack->MinorFunction) {

                case IRP_MN_QUERY_CAPABILITIES:
                    pCaps = pIrpStack -> Parameters.DeviceCapabilities.Capabilities;

                    //
                    // fill in the structure with non-controversial values
                    //

                    pCaps -> D1Latency = 10;
                    pCaps -> D2Latency = 10;
                    pCaps -> D3Latency = 10;

                    //
                    // Call down synchronously.
                    //

                    pIrp -> IoStatus.Status = STATUS_SUCCESS;
                    Status = SSCallNextDriverSynch(pde, pIrp);
                    if(!NT_SUCCESS(Status)){
                        SCSISCAN_KdPrint(MIN_TRACE,("SSPnp: ERROR!! Call down failed\n Status=0x%x", Status));
                    }

                    //
                    // Complete IRP.
                    //

                    IoCompleteRequest( pIrp, IO_NO_INCREMENT );
                    return Status;

                case IRP_MN_START_DEVICE:
                    SCSISCAN_KdPrint(MIN_TRACE,("IRP_MJ_START_DEVICE\n"));

                    //
                    // Initialize PendingIoEvent.  Set the number of pending i/o requests for this device to 1.
                    // When this number falls to zero, it is okay to remove, or stop the device.
                    //

                    pde -> PendingIoCount = 0;
                    KeInitializeEvent(&pde -> PendingIoEvent, NotificationEvent, FALSE);
                    SSIncrementIoCount(pDeviceObject);

                    //
                    // First, let the port driver start the device.
                    //

                    Status = SSCallNextDriverSynch(pde, pIrp);
                    if(!NT_SUCCESS(Status)){
                        SCSISCAN_KdPrint(MIN_TRACE,("SSPnp: ERROR!! Call down failed\n Status=0x%x", Status));
                    }

                    //
                    // The port driver has started the device.  It is time for
                    // us to do some initialization and create symbolic links
                    // for the device.

                    //
                    // Call port driver to get adapter capabilities.
                    //

                    PropertyId = StorageAdapterProperty;
                    pde -> pAdapterDescriptor = NULL;
                    Status = ClassGetDescriptor(pde -> pStackDeviceObject,
                                                &PropertyId, &(pde -> pAdapterDescriptor));
                    if(!NT_SUCCESS(Status)) {
                        SCSISCAN_KdPrint(MIN_TRACE, ("SSDeviceStarted: unable to retrieve adapter descriptor "
                            "[%#08lx]\n", Status));
                        if (NULL != pde -> pAdapterDescriptor) {
                            ExFreePool( pde -> pAdapterDescriptor);
                        }
                        break;
                    }

                    //
                    // Create the symbolic link for this device.
                    //

                    Status = SSCreateSymbolicLink( pde );
                    if (!NT_SUCCESS(Status)) {
                        SCSISCAN_KdPrint(MIN_TRACE, ("Can't create symbolic link.\n"));
                        DEBUG_BREAKPOINT();
                        if (NULL != pde -> pAdapterDescriptor) {
                            ExFreePool( pde -> pAdapterDescriptor);
                        }
                        break;
                    }

                    //
                    // Indicate device is now ready.
                    //

                    pde -> DeviceLock = 0;
                    pde -> OpenInstanceCount = 0;
                    pde -> AcceptingRequests = TRUE;

                    pde -> LastSrbError = 0L;

                    pIrp -> IoStatus.Status = Status;
                    IoCompleteRequest( pIrp, IO_NO_INCREMENT );
                    return Status;

                case IRP_MN_REMOVE_DEVICE:
                    SCSISCAN_KdPrint(MIN_TRACE,("IRP_MN_REMOVE_DEVICE\n"));

                    //
                    // Forward remove message to lower driver.
                    //

                    pIrp -> IoStatus.Status = STATUS_SUCCESS;

                    Status = SSCallNextDriverSynch(pde, pIrp);
                    if(!NT_SUCCESS(Status)){
                        SCSISCAN_KdPrint(MIN_TRACE,("SSPnp: ERROR!! Call down failed\n Status=0x%x", Status));
                    }

                    if (pde -> AcceptingRequests) {
                        pde -> AcceptingRequests = FALSE;
                        SSDestroySymbolicLink( pde );
                    }

                    ScsiScanHandleInterface(pde-> pPhysicalDeviceObject,
                                            &pde->InterfaceNameString,
                                            FALSE);

                    if (pde->InterfaceNameString.Buffer != NULL) {
                        IoSetDeviceInterfaceState(&pde->InterfaceNameString,FALSE);
                    }

                    //
                    // wait for any io requests pending in our driver to
                    // complete before finishing the remove
                    //

                    SSDecrementIoCount(pDeviceObject);
                    KeWaitForSingleObject(&pde -> PendingIoEvent, Suspended, KernelMode,
                                          FALSE,NULL);

                    if (pde -> pAdapterDescriptor) {
                        ExFreePool(pde -> pAdapterDescriptor);
                    }

                    IoDetachDevice(pde -> pStackDeviceObject);
                    IoDeleteDevice (pDeviceObject);

                    IoCompleteRequest( pIrp, IO_NO_INCREMENT );
                    return Status;

            case IRP_MN_STOP_DEVICE:
                    SCSISCAN_KdPrint(MIN_TRACE,("IRP_MN_STOP_DEVICE\n"));

                    //
                    // Indicate device is not ready.
                    //

                    ASSERT(pde -> AcceptingRequests);
                    pde -> AcceptingRequests = FALSE;

                    //
                    // Remove symbolic link.
                    //
                    SSDestroySymbolicLink( pde );

                    if (pde->InterfaceNameString.Buffer != NULL) {
                        IoSetDeviceInterfaceState(&pde->InterfaceNameString,FALSE);
                    }

                    //
                    // Let the port driver stop the device.
                    //

                    pIrp -> IoStatus.Status = STATUS_SUCCESS;

                    Status = SSCallNextDriverSynch(pde, pIrp);
                    if(!NT_SUCCESS(Status)){
                        SCSISCAN_KdPrint(MIN_TRACE,("SSPnp: ERROR!! Call down failed\n Status=0x%x", Status));
                    }

                    //
                    // wait for any io requests pending in our driver to
                    // complete before finishing the remove
                    //

                    SSDecrementIoCount(pDeviceObject);
                    KeWaitForSingleObject(&pde -> PendingIoEvent, Suspended, KernelMode,
                                          FALSE,NULL);

                    ASSERT(pde -> pAdapterDescriptor);
                    ExFreePool(pde -> pAdapterDescriptor);

                    IoCompleteRequest( pIrp, IO_NO_INCREMENT );
                    return Status;

            case IRP_MN_QUERY_STOP_DEVICE:
                    SCSISCAN_KdPrint(MIN_TRACE,("IRP_MN_QUERY_STOP_DEVICE\n"));
                    pIrp->IoStatus.Status = STATUS_SUCCESS;
                    break;

            case IRP_MN_QUERY_REMOVE_DEVICE:
                    SCSISCAN_KdPrint(MIN_TRACE,("IRP_MN_QUERY_REMOVE_DEVICE\n"));
                    pIrp->IoStatus.Status = STATUS_SUCCESS;
                    break;

            case IRP_MN_CANCEL_STOP_DEVICE:
                    SCSISCAN_KdPrint(MIN_TRACE,("IRP_MN_CANCEL_STOP_DEVICE\n"));
                    pIrp->IoStatus.Status = STATUS_SUCCESS;
                    break;

            case IRP_MN_CANCEL_REMOVE_DEVICE:
                    SCSISCAN_KdPrint(MIN_TRACE,("IRP_MN_CANCEL_REMOVE_DEVICE\n"));
                    pIrp->IoStatus.Status = STATUS_SUCCESS;
                    break;

            case IRP_MN_SURPRISE_REMOVAL:
                    SCSISCAN_KdPrint(MIN_TRACE,("IRP_MN_SURPRISE_REMOVAL\n"));
                    pIrp->IoStatus.Status = STATUS_SUCCESS;
                    break;

            default:
                    SCSISCAN_KdPrint(MIN_TRACE,("Minor PNP message received, MinFunction = %x\n",
                                                pIrpStack->MinorFunction));
                    break;

            } /* case MinorFunction, MajorFunction == IRP_MJ_PNP_POWER  */

            ASSERT(Status == STATUS_SUCCESS);
            if (!NT_SUCCESS(Status)) {
                pIrp -> IoStatus.Status = Status;
                IoCompleteRequest( pIrp, IO_NO_INCREMENT );
                return Status;
            }

            IoCopyCurrentIrpStackLocationToNext(pIrp);

            SCSISCAN_KdPrint(MIN_TRACE,("Passing Pnp Irp down,  status = %x\n", Status));

            Status = IoCallDriver(pde -> pStackDeviceObject, pIrp);
            return Status;
            break; // IRP_MJ_PNP

        default:
            SCSISCAN_KdPrint(MIN_TRACE,("Major PNP IOCTL not handled\n"));
            ASSERT(0); // We should never ever get here
            Status = STATUS_INVALID_PARAMETER;
            pIrp -> IoStatus.Status = Status;
            IoCompleteRequest( pIrp, IO_NO_INCREMENT );
            return Status;

    } /* switch (MajorFunction) */

} // end SSPnp()


NTSTATUS
SSOpen(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine is called to establish a connection to the device
    class driver. It does no more than return STATUS_SUCCESS.

Arguments:
    pDeviceObject - Device object for a device.
    pIrp - Open request packet

Return Value:
    NT Status - STATUS_SUCCESS

--*/
{
    NTSTATUS                    Status;
    PSCSISCAN_DEVICE_EXTENSION  pde;
    PIO_STACK_LOCATION          pIrpStack;

    PAGED_CODE();

    SSIncrementIoCount( pDeviceObject );

    SCSISCAN_KdPrint(MAX_TRACE,("SSOpen called.\n"));

    pde = (PSCSISCAN_DEVICE_EXTENSION)pDeviceObject -> DeviceExtension;

    pIrpStack = IoGetCurrentIrpStackLocation( pIrp );

    InterlockedIncrement(&pde -> OpenInstanceCount);
    (ULONG)(UINT_PTR)(pIrpStack -> FileObject -> FsContext) = pde -> OpenInstanceCount;

    Status = STATUS_SUCCESS;

    //
    // Check if device is not going away, in which case fail open request
    //
    if (pde -> AcceptingRequests == FALSE) {

        Status = STATUS_DELETE_PENDING;

        pIrp -> IoStatus.Information = 0;
        pIrp -> IoStatus.Status = Status;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);

        SSDecrementIoCount(pDeviceObject);

        SCSISCAN_KdPrint(MAX_TRACE,("SSOpen: Leaving , not accepting requests.. Status = %x.\n", Status));

        return Status;
    }

    SSDecrementIoCount(pDeviceObject);

    pIrp -> IoStatus.Information = 0;
    pIrp -> IoStatus.Status = Status;

    IoCopyCurrentIrpStackLocationToNext( pIrp );
    return IoCallDriver(pde -> pStackDeviceObject, pIrp);

} // end SSOpen()


NTSTATUS
SSClose(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp
    )
/*++

Routine Description:

Arguments:
    pDeviceObject - Device object for a device.
    pIrp - Open request packet

Return Value:
    NT Status - STATUS_SUCCESS

--*/
{
    NTSTATUS                    Status;
    PSCSISCAN_DEVICE_EXTENSION  pde;
    PIO_STACK_LOCATION          pIrpStack;

    PAGED_CODE();

    SSIncrementIoCount( pDeviceObject );

    SCSISCAN_KdPrint(MAX_TRACE,("SSClose called.\n"));

    pde = (PSCSISCAN_DEVICE_EXTENSION)pDeviceObject -> DeviceExtension;

    Status = STATUS_SUCCESS;

    pIrpStack = IoGetCurrentIrpStackLocation( pIrp );
    pIrpStack -> FileObject -> FsContext = 0;

    pIrp -> IoStatus.Information = 0;
    pIrp -> IoStatus.Status = Status;

    SSDecrementIoCount(pDeviceObject);

    IoCopyCurrentIrpStackLocationToNext( pIrp );
    return IoCallDriver(pde -> pStackDeviceObject, pIrp);

} // end SSClose()



NTSTATUS
SSDeviceControl(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp
    )
/*++

Routine Description:
    This function allows a user mode client to send CDBs to the device.

Arguments:
    pDeviceObject - Device object for a device.
    pIrp - Open request packet

Return Value:
    NT Status - STATUS_SUCCESS

--*/
{
    PIO_STACK_LOCATION          pIrpStack;
    PIO_STACK_LOCATION          pNextIrpStack;
    ULONG                       IoControlCode;
    ULONG                       OldTimeout;
    PSCSISCAN_DEVICE_EXTENSION  pde;
    PTRANSFER_CONTEXT           pTransferContext = NULL;
    PSCSISCAN_CMD               pCmd;
    PMDL                        pMdl = NULL;
    NTSTATUS                    Status;
    PVOID                       Owner;
    PULONG                      pTimeOut;
    PCDB                        pCdb;
    PVOID                       pUserBuffer;

    BOOLEAN                     fLockedSenseBuffer, fLockedSRBStatus;

    PAGED_CODE();

    SSIncrementIoCount( pDeviceObject );

    pde = (PSCSISCAN_DEVICE_EXTENSION)pDeviceObject -> DeviceExtension;

    //
    // Validate state of the device
    //
    if (pde -> AcceptingRequests == FALSE) {
        SCSISCAN_KdPrint(MIN_TRACE,("IOCTL issued after device stopped/removed!\n"));
        DEBUG_BREAKPOINT();
        Status = STATUS_DELETE_PENDING;
        pIrp -> IoStatus.Information = 0;
        goto SSDeviceControl_Complete;
    }

    //
    // Indicate that MDLs are not locked yet
    //
    fLockedSenseBuffer = fLockedSRBStatus = FALSE;

    //
    // Get context pointers
    //
    pIrpStack     = IoGetCurrentIrpStackLocation( pIrp );
    pNextIrpStack = IoGetNextIrpStackLocation( pIrp );
    IoControlCode = pIrpStack->Parameters.DeviceIoControl.IoControlCode;
    //ControlCode   = (IoControlCode >> 2) & 0x00000FFF;

    // Get owner of device (0 = locked, >0 if someone has it locked)

    Owner = InterlockedCompareExchangePointer(&pde -> DeviceLock,
                                              NULL,
                                              NULL);

    if (Owner != NULL) {
        if (Owner != pIrpStack -> FileObject -> FsContext) {
            SCSISCAN_KdPrint(MIN_TRACE,("Device i/o control request failed because device is locked\n"));
            Status = STATUS_DEVICE_BUSY;
            pIrp -> IoStatus.Information = 0;
            goto SSDeviceControl_Complete;
        }
    }

    SCSISCAN_KdPrint(MAX_TRACE,("Control code %d received\n", IoControlCode));
    pCmd = pIrp -> AssociatedIrp.SystemBuffer;

    switch (IoControlCode) {

        case IOCTL_SCSISCAN_SET_TIMEOUT:

            SCSISCAN_KdPrint(MAX_TRACE,("SCSISCAN_SET_TIMEOUT\n"));
            pTimeOut = pIrp -> AssociatedIrp.SystemBuffer;

            //
            // Validate size of the input parameter
            //
            if (pIrpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(pde -> TimeOutValue) ) {
                SCSISCAN_KdPrint(MIN_TRACE,("Timeout value   buffer too small\n"));
                Status = STATUS_INVALID_PARAMETER;
                goto SSDeviceControl_Complete;
            }

            OldTimeout = *pTimeOut ;
            OldTimeout = InterlockedExchange(&pde -> TimeOutValue, *pTimeOut );

            pIrp -> IoStatus.Information = 0;

            //
            // If caller wanted to get old timeout value back - give it to him.
            // Ideally we should've require nonNULL value for output buffer, but it had not been speced
            // and now we can't change compatibility.
            //
            if (pIrpStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(OldTimeout) ) {
                *pTimeOut = OldTimeout;
                pIrp -> IoStatus.Information = sizeof(OldTimeout) ;
            }

            Status = STATUS_SUCCESS;
            goto SSDeviceControl_Complete;

        case IOCTL_SCSISCAN_LOCKDEVICE:
            //
            // Lock device
            //
            SCSISCAN_KdPrint(MAX_TRACE,("SCSISCAN_LOCKDEVICE\n"));

            Status = STATUS_DEVICE_BUSY;
            if (NULL == InterlockedCompareExchangePointer(&pde -> DeviceLock,
                                                          pIrpStack -> FileObject -> FsContext,
                                                          NULL)) {
                Status = STATUS_SUCCESS;
            }
            goto SSDeviceControl_Complete;

        case IOCTL_SCSISCAN_UNLOCKDEVICE:
            //
            // Unlock device
            //
            SCSISCAN_KdPrint(MAX_TRACE,("SCSISCAN_UNLOCKDEVICE\n"));

            Status = STATUS_DEVICE_BUSY;
            if (pIrpStack -> FileObject -> FsContext ==
                InterlockedCompareExchangePointer(&pde -> DeviceLock,
                                                  NULL,
                                                  pIrpStack -> FileObject -> FsContext)) {
                Status = STATUS_SUCCESS;
            }
            goto SSDeviceControl_Complete;

        case IOCTL_SCSISCAN_CMD:

            //
            // Issue SCSI command
            //
            #if DBG_DEVIOCTL
            {
                PCDB                          pCdb;

                pCdb = (PCDB)pCmd -> Cdb;

                SCSISCAN_KdPrint(MAX_TRACE,("SSDeviceControl SCSICAN_CMD_CODE \n"));
                SCSISCAN_KdPrint(MAX_TRACE,("SSDeviceControl CDB->ControlCode = %d  \n",pCdb->CDB6GENERIC.OperationCode));

            }
            #endif

            pTransferContext = SSBuildTransferContext(pde,
                                                      pIrp,
                                                      pCmd,
                                                      pIrpStack -> Parameters.DeviceIoControl.InputBufferLength,
                                                      pIrp -> MdlAddress,
                                                      TRUE
                                                      );
            if (NULL == pTransferContext) {
                SCSISCAN_KdPrint(MIN_TRACE,("SSDeviceControl: Can't create transfer context!\n"));
                DEBUG_BREAKPOINT();
                Status = STATUS_INVALID_PARAMETER;
                goto SSDeviceControl_Complete;
            }

            //
            // Fill in transfer length in the CDB.
            //

            if(10 == pCmd -> CdbLength){

                //
                // Currently Scsiscan only supports flagmentation of 10bytes CDB.
                //

                SSSetTransferLengthToCdb((PCDB)pCmd -> Cdb, pTransferContext -> TransferLength);

            } else if (6 != pCmd -> CdbLength){

                //
                // If CdbLength is not 6 or 10 and transfer size exceeds adapter limit, SCSISCAN cannot handle it.
                //

                if(pTransferContext -> TransferLength != pCmd -> TransferLength){
                    SCSISCAN_KdPrint(MIN_TRACE,("SSDeviceControl: TransferLength (CDB !=6 or 10) exceeds limits!\n"));
                    DEBUG_BREAKPOINT();
                    Status = STATUS_INVALID_PARAMETER;
                    goto SSDeviceControl_Complete;
                }
            }

            //
            // Create system address for the user's sense buffer (if any).
            //

            if (pCmd -> SenseLength) {

                pTransferContext -> pSenseMdl = MmCreateMdl(NULL,
                                                            pCmd -> pSenseBuffer,
                                                            pCmd -> SenseLength);

                if (NULL == pTransferContext -> pSenseMdl) {

                    SCSISCAN_KdPrint(MIN_TRACE,("Can't create MDL for sense buffer!\n"));
                    DEBUG_BREAKPOINT();

                    Status = STATUS_INSUFFICIENT_RESOURCES;

                    goto SSDeviceControl_Error_With_Status;
                }

                //
                // Probe and lock the pages associated with the
                // caller's buffer for write access , using processor mode of the requestor
                // Nb: Probing may cause an exception
                //
                try{

                    MmProbeAndLockPages(pTransferContext -> pSenseMdl,
                                        pIrp -> RequestorMode,
                                        IoModifyAccess
                                        );

                } except(EXCEPTION_EXECUTE_HANDLER) {

                    SCSISCAN_KdPrint(MIN_TRACE,("SCSISCAN Buffer (pTransferContext -> pSenseMdl) validation logic \n"));
                    SCSISCAN_KdPrint(MIN_TRACE,("Read/Write registers  buffer pointer is invalid\n"));

                    Status = GetExceptionCode();

                    // DEBUG_BREAKPOINT();

                    pIrp -> IoStatus.Information = 0;

                    goto SSDeviceControl_Error_With_Status;

                }  // except

                //
                // Indicate we succesfully locked sense buffer
                //
               fLockedSenseBuffer = TRUE;

                pTransferContext -> pSenseMdl -> MdlFlags |= MDL_MAPPING_CAN_FAIL;
                pTransferContext -> pSenseBuffer =
                                     MmGetSystemAddressForMdl(pTransferContext -> pSenseMdl);
                if (NULL == pTransferContext -> pSenseBuffer) {

                    SCSISCAN_KdPrint(MIN_TRACE,("Can't get system address for sense buffer!\n"));
                    DEBUG_BREAKPOINT();

                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto SSDeviceControl_Error_With_Status;

                }
            }

            //
            // Create system address for the user's srb status byte.
            //

            pMdl = MmCreateMdl(NULL,
                               pCmd -> pSrbStatus,
                               sizeof(UCHAR)
                               );

            if (NULL == pMdl) {

                SCSISCAN_KdPrint(MIN_TRACE,("Can't create MDL for pSrbStatus!\n"));
                DEBUG_BREAKPOINT();

                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto SSDeviceControl_Error_With_Status;

            }

            //
            // Probe and lock the pages associated with the caller's
            // buffer for write access , using processor mode of the requestor
            // Nb: Probing may cause an exception
            //
            try{

                MmProbeAndLockPages(pMdl,
                                    pIrp -> RequestorMode,
                                    IoModifyAccess);

            } except(EXCEPTION_EXECUTE_HANDLER) {

                SCSISCAN_KdPrint(MIN_TRACE,("SCSISCAN Buffer (pMdl) validation logic \n"));
                SCSISCAN_KdPrint(MIN_TRACE,("Read/Write registers  buffer pointer is invalid\n"));

                Status = GetExceptionCode();

                // DEBUG_BREAKPOINT();

                pIrp -> IoStatus.Information = 0;
                goto SSDeviceControl_Error_With_Status;
            }               // except

            //
            // Indicate we successfully locked SRB status
            //
            fLockedSRBStatus = TRUE;

            pMdl->MdlFlags |= MDL_MAPPING_CAN_FAIL;
            pCmd -> pSrbStatus =  MmGetSystemAddressForMdl(pMdl);
            if (NULL == pCmd -> pSrbStatus) {

                SCSISCAN_KdPrint(MIN_TRACE,("Can't get system address for pSrbStatus!\n"));
                DEBUG_BREAKPOINT();

                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto SSDeviceControl_Error_With_Status;

            }
            pTransferContext -> pSrbStatusMdl = pMdl;

            break;

        case IOCTL_SCSISCAN_GET_INFO:

            //
            // Get and return SCSI information block for the scanner device
            //
            SCSISCAN_KdPrint(MAX_TRACE,("SCSISCAN_GET_INFO\n"));

            if (sizeof(SCSISCAN_INFO) != pIrpStack->Parameters.DeviceIoControl.OutputBufferLength) {
                SCSISCAN_KdPrint(MIN_TRACE,("Output buffer size is wring!\n"));
                DEBUG_BREAKPOINT();

                Status = STATUS_INVALID_PARAMETER;
                goto SSDeviceControl_Error_With_Status;
            }

            if (sizeof(SCSISCAN_INFO) > MmGetMdlByteCount(pIrp->MdlAddress)) {
                SCSISCAN_KdPrint(MIN_TRACE,("Buffer is too small!\n"));
                DEBUG_BREAKPOINT();

                Status = STATUS_INVALID_PARAMETER;
                goto SSDeviceControl_Error_With_Status;
            }

            pIrp->MdlAddress->MdlFlags |= MDL_MAPPING_CAN_FAIL;
            pUserBuffer =  MmGetSystemAddressForMdl(pIrp->MdlAddress);
            if(NULL == pUserBuffer){
                SCSISCAN_KdPrint(MIN_TRACE,(" MmGetSystemAddressForMdl failed!\n"));
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto SSDeviceControl_Complete;
            }

            Status = ClassGetInfo(pde -> pStackDeviceObject, pUserBuffer);
            if(!NT_SUCCESS(Status)){
                SCSISCAN_KdPrint(MIN_TRACE,("ClassGetInfo failed!\n"));
            } else {
                SCSISCAN_KdPrint(MIN_TRACE,("ClassGetInfo OK!!\n"));
            }
                goto SSDeviceControl_Complete;

        default:
            //
            // Unsupported IOCTL code - bail out
            //
            SCSISCAN_KdPrint(MIN_TRACE,("Unsupported IOCTL!\n"));
            DEBUG_BREAKPOINT();

            Status = STATUS_NOT_SUPPORTED;
            goto SSDeviceControl_Complete;
            break;
    }

    //
    // Pass request down and mark as pending
    //
    IoMarkIrpPending(pIrp);
    IoSetCompletionRoutine(pIrp, SSIoctlIoComplete, pTransferContext, TRUE, TRUE, FALSE);
    SSSendScannerRequest(pDeviceObject, pIrp, pTransferContext, FALSE);

    return STATUS_PENDING;

    //
    // Cleanup
    //

SSDeviceControl_Error_With_Status:

    //
    // Clean up if something went wrong when allocating resources
    //
    if (pMdl) {

        if (fLockedSRBStatus) {
            MmUnlockPages(pMdl);
        }

        IoFreeMdl(pMdl);

        if (pTransferContext) {
            pTransferContext -> pSrbStatusMdl = NULL;
        }

        pCmd -> pSrbStatus = NULL;
    }

    if (pTransferContext) {

        if (pTransferContext -> pSenseMdl) {

            if ( fLockedSenseBuffer ) {
                MmUnlockPages(pTransferContext -> pSenseMdl);
            }

            IoFreeMdl(pTransferContext -> pSenseMdl);

            pTransferContext -> pSenseMdl = NULL;
            pTransferContext -> pSenseBuffer = NULL;
        }
    }


SSDeviceControl_Complete:
    //
    // Everything seems to be OK - complet I/O request
    //
    pIrp -> IoStatus.Status = Status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    SSDecrementIoCount(pDeviceObject);
    return Status;

}   // end SSDeviceControl()



NTSTATUS
SSReadWrite(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This is the entry called by the I/O system for scanner IO.

Arguments:

    DeviceObject - the system object for the device.
    Irp - IRP involved.

Return Value:

    NT Status

--*/
{
    NTSTATUS                      Status;
    PIO_STACK_LOCATION            pIrpStack;
    PSCSISCAN_DEVICE_EXTENSION    pde;
    PTRANSFER_CONTEXT             pTransferContext;
    PMDL                          pMdl;
    PSCSISCAN_CMD                 pCmd;
    PCDB                          pCdb;
    PVOID                         Owner;

    PAGED_CODE();

    pCmd = NULL;

    SSIncrementIoCount( pDeviceObject );

    pde = (PSCSISCAN_DEVICE_EXTENSION)pDeviceObject -> DeviceExtension;

    if (pde -> AcceptingRequests == FALSE) {
        SCSISCAN_KdPrint(MIN_TRACE,("Read/write request issued after device stopped/removed!\n"));
        Status = STATUS_DELETE_PENDING;
        pIrp -> IoStatus.Information = 0;
        goto SSReadWrite_Complete;
    }

    pIrpStack = IoGetCurrentIrpStackLocation( pIrp );
#if DBG
    if (pIrpStack -> MajorFunction == IRP_MJ_READ) {
        SCSISCAN_KdPrint(MAX_TRACE,("Read request received\n"));
    } else {
        SCSISCAN_KdPrint(MAX_TRACE,("Write request received\n"));
    }
#endif

    Owner = InterlockedCompareExchangePointer(&pde -> DeviceLock,
                                              pIrpStack -> FileObject -> FsContext,
                                              pIrpStack -> FileObject -> FsContext);
    if (Owner != 0) {
        if (Owner != pIrpStack -> FileObject -> FsContext) {
            SCSISCAN_KdPrint(MAX_TRACE,("Read/Write request failed because device is locked\n"));
            Status = STATUS_DEVICE_BUSY;
            pIrp -> IoStatus.Information = 0;
            goto SSReadWrite_Complete;
        }
    }


    pMdl = pIrp -> MdlAddress;

    //
    // Allocate a SCSISCAN_CMD structure and initialize it.
    //

    pCmd = ExAllocatePool(NonPagedPool, sizeof(SCSISCAN_CMD));
    if (NULL == pCmd) {
        SCSISCAN_KdPrint(MIN_TRACE, ("SSReadWrite: cannot allocated SCSISCAN_CMD structure\n"));
        DEBUG_BREAKPOINT();
        pIrp -> IoStatus.Information = 0;
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SSReadWrite_Complete;
    }

    memset(pCmd,0, sizeof(SCSISCAN_CMD));

    //
    // Fill out SCSISCAN_CMD structure.
    //

#if DBG
    pCmd -> Reserved1      = 'dmCS';
#endif
    pCmd -> Size           = sizeof(SCSISCAN_CMD);
    pCmd -> SrbFlags       = SRB_FLAGS_DATA_IN;
    pCmd -> CdbLength      = 6;
    pCmd -> SenseLength    = SENSE_BUFFER_SIZE;
    pCmd -> TransferLength = pIrpStack->Parameters.Read.Length;

    pCmd -> pSenseBuffer   = NULL;
    //
    // Point pSrbStatus to a reserved field in the SCSISCAN_CMD structure.
    // The ReadFile / WriteFile code path never looks at it, but BuildTransferContext
    // will complain if this pointer is NULL.
    //

    pCmd -> pSrbStatus     = &(pCmd -> Reserved2);

    pCdb = (PCDB)pCmd -> Cdb;
    pCdb -> CDB6READWRITE.OperationCode = SCSIOP_READ6;
    if (pIrpStack -> MajorFunction == IRP_MJ_WRITE) {
        pCmd -> SrbFlags = SRB_FLAGS_DATA_OUT;
        pCdb -> CDB6READWRITE.OperationCode = SCSIOP_WRITE6;
    }

    //
    // Allocate a sense buffer.
    //

    pCmd -> pSenseBuffer = ExAllocatePool(NonPagedPool, SENSE_BUFFER_SIZE);
    if (NULL == pCmd -> pSenseBuffer) {
        SCSISCAN_KdPrint(MIN_TRACE, ("Cannot allocate sense buffer\n"));
        DEBUG_BREAKPOINT();
        pIrp -> IoStatus.Information = 0;
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SSReadWrite_Complete;
    }

#if DBG
    *(PULONG)(pCmd ->pSenseBuffer) = 'sneS';
#endif

    //
    // Build a transfer context.
    //

    pTransferContext = SSBuildTransferContext(pde, pIrp, pCmd, sizeof(SCSISCAN_CMD), pMdl, TRUE);
    if (NULL == pTransferContext) {
        SCSISCAN_KdPrint(MIN_TRACE,("SSDeviceControl: Can't create transfer context!\n"));
        DEBUG_BREAKPOINT();

        ExFreePool(pCmd -> pSenseBuffer);
        ExFreePool(pCmd);
        pCmd = NULL;

        Status = STATUS_INSUFFICIENT_RESOURCES;
        pIrp -> IoStatus.Information = 0;
        goto SSReadWrite_Complete;
    }

    //
    // Fill in transfer length in the CDB.
    //

    pCdb -> PRINT.TransferLength[2] = ((PFOUR_BYTE)&(pTransferContext -> TransferLength)) -> Byte0;
    pCdb -> PRINT.TransferLength[1] = ((PFOUR_BYTE)&(pTransferContext -> TransferLength)) -> Byte1;
    pCdb -> PRINT.TransferLength[0] = ((PFOUR_BYTE)&(pTransferContext -> TransferLength)) -> Byte2;

    //
    // Save retry count in transfer context.
    //

    pTransferContext -> RetryCount = MAXIMUM_RETRIES;

    //
    // Mark IRP with status pending.
    //

    IoMarkIrpPending(pIrp);

    //
    // Set the completion routine and issue scanner request.
    //

    IoSetCompletionRoutine(pIrp, SSReadWriteIoComplete, pTransferContext, TRUE, TRUE, FALSE);
    SSSendScannerRequest(pDeviceObject, pIrp, pTransferContext, FALSE);
    return STATUS_PENDING;


SSReadWrite_Complete:

    //
    // Free allocated command and sense buffers
    //

    if (pCmd ) {

        if (pCmd -> pSenseBuffer) {
            ExFreePool(pCmd -> pSenseBuffer);
        }

        ExFreePool(pCmd);
        pCmd = NULL;
    }

    pIrp->IoStatus.Status = Status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    SSDecrementIoCount( pDeviceObject );

    return Status;


} // end SSReadWrite()


PTRANSFER_CONTEXT
SSBuildTransferContext(
    PSCSISCAN_DEVICE_EXTENSION  pde,
    PIRP                        pIrp,
    PSCSISCAN_CMD               pCmd,
    ULONG                       CmdLength,
    PMDL                        pTransferMdl,
    BOOLEAN                     AllowMultipleTransfer
    )
/*++

Routine Description:

Arguments:

Return Value:

    NULL if error

--*/
{
    PMDL                        pSenseMdl;
    PTRANSFER_CONTEXT           pTransferContext;

    PAGED_CODE();

    pTransferContext = NULL;
    pSenseMdl        = NULL;

    //
    // Validate the SCSISCAN_CMD structure.
    //

    if ( (CmdLength < sizeof(SCSISCAN_CMD)) ||
         (pCmd -> Size != sizeof(SCSISCAN_CMD)) ||
         (0 == pCmd -> CdbLength)   ||
         (pCmd -> CdbLength > sizeof(pCmd -> Cdb))) {
        SCSISCAN_KdPrint(MIN_TRACE,("Badly formed SCSISCAN_CMD struture!\n"));
        DEBUG_BREAKPOINT();
        goto BuildTransferContext_Error;
    }

    //
    // Verify that pSrbStatus is non-zero.
    //

    if (NULL == pCmd -> pSrbStatus) {
        SCSISCAN_KdPrint(MIN_TRACE,("NULL pointer for pSrbStatus!\n"));
        DEBUG_BREAKPOINT();
        goto BuildTransferContext_Error;
    }


#if DBG
    pCmd -> Reserved1      = 'dmCS';
#endif

    //
    // Verify that if TransferLength is non-zero, a transfer direction has also been specified.
    //

    if (0 != pCmd -> TransferLength) {
        if (0 == (pCmd -> SrbFlags & (SRB_FLAGS_DATA_IN | SRB_FLAGS_DATA_OUT))) {
            SCSISCAN_KdPrint(MIN_TRACE,("Transfer length specified with no direction!\n"));
            DEBUG_BREAKPOINT();
            goto BuildTransferContext_Error;
        }
    }

    //
    // Verify that if the direction bits have been set, a transfer length has also be specified.
    //

    if (0 != (pCmd -> SrbFlags & (SRB_FLAGS_DATA_IN | SRB_FLAGS_DATA_OUT))) {
        if (0 == pCmd -> TransferLength) {
            SCSISCAN_KdPrint(MIN_TRACE,("Direction bits have been set with 0 transfer buffer size!\n"));
            DEBUG_BREAKPOINT();
            goto BuildTransferContext_Error;
        }
    }

    //
    // Verify that if TransferLength is non-zero, then an associated MDL has also been specified.
    // Also, verify that the transfer length does not exceed the transfer buffer size.
    //


    if (0 != pCmd -> TransferLength) {
        if (NULL == pTransferMdl) {
            SCSISCAN_KdPrint(MIN_TRACE,("Non-zero transfer length specified with NULL transfer buffer!\n"));
            DEBUG_BREAKPOINT();
            goto BuildTransferContext_Error;
        }
        if (pCmd -> TransferLength > MmGetMdlByteCount(pTransferMdl)) {
            SCSISCAN_KdPrint(MIN_TRACE,("Transfer length exceeds transfer buffer size!\n"));
            DEBUG_BREAKPOINT();
            goto BuildTransferContext_Error;
        }
    }

    //
    // Verify that if SenseLength is non-zero, then pSenseBuffer is non-zero as well.
    //

    if (pCmd -> SenseLength) {
        if (NULL == pCmd -> pSenseBuffer) {
            SCSISCAN_KdPrint(MIN_TRACE,("Non-zero sense length specified with NULL sense buffer!\n"));
            DEBUG_BREAKPOINT();
            goto BuildTransferContext_Error;
        }

        if (pCmd -> SrbFlags & SRB_FLAGS_DISABLE_AUTOSENSE) {
            SCSISCAN_KdPrint(MIN_TRACE,("Warning: autosense disabled with NON-null sense buffer.\n"));
        }
    }

    pTransferContext = ExAllocatePool(NonPagedPool, sizeof(TRANSFER_CONTEXT));

    if (NULL == pTransferContext) {
        SCSISCAN_KdPrint(MIN_TRACE,("Failed to allocate transfer context\n"));
        DEBUG_BREAKPOINT();
        return NULL;
    }


    memset(pTransferContext, 0, sizeof(TRANSFER_CONTEXT));
#if DBG
    pTransferContext -> Signature = 'refX';
#endif
    pTransferContext -> pCmd = pCmd;

    if (pCmd -> TransferLength) {

#ifdef WINNT
        pTransferContext -> pTransferBuffer = MmGetMdlVirtualAddress(pTransferMdl);
#else
        pTransferContext -> pTransferBuffer = MmGetSystemAddressForMdl(pTransferMdl);
#endif
        if(NULL == pTransferContext -> pTransferBuffer){
            SCSISCAN_KdPrint(MIN_TRACE,("Failed to create address for MDL.\n"));
            DEBUG_BREAKPOINT();
            goto BuildTransferContext_Error;
        }

        pTransferContext -> RemainingTransferLength = pCmd -> TransferLength;
        pTransferContext -> TransferLength = pCmd -> TransferLength;

        //
        // Adjust the transfer size to work within the limits of the hardware.  Fail if the transfer is too
        // big and the caller doesn't want the transfer to be split up.
        //

        SSAdjustTransferSize( pde, pTransferContext );

        if (pTransferContext -> RemainingTransferLength !=
            (LONG)pTransferContext -> TransferLength) {
            if (!AllowMultipleTransfer) {
                SCSISCAN_KdPrint(MIN_TRACE,("Transfer exceeds hardware limits!\n"));
                DEBUG_BREAKPOINT();
                goto BuildTransferContext_Error;
            }
        }
    }

    pTransferContext -> pSenseBuffer = pCmd -> pSenseBuffer;
    return pTransferContext;


BuildTransferContext_Error:
    if (pTransferContext) {
        ExFreePool( pTransferContext );
    }
    return NULL;
}   // end SSBuildTransferContext()



VOID
SSAdjustTransferSize(
    PSCSISCAN_DEVICE_EXTENSION  pde,
    PTRANSFER_CONTEXT pTransferContext
    )
/*++

Routine Description:
    This is the entry called by the I/O system for scanner IO.

Arguments:

Return Value:

    NT Status

--*/
{
    ULONG MaxTransferLength;
    ULONG nTransferPages;

    PAGED_CODE();

    MaxTransferLength = pde -> pAdapterDescriptor -> MaximumTransferLength;

    //
    // Make sure the transfer size does not exceed the limitations of the underlying hardware.
    // If so, we will break the transfer up into chunks.
    //

    if (pTransferContext -> TransferLength > MaxTransferLength) {
        SCSISCAN_KdPrint(MAX_TRACE,("Request size (%d) greater than maximum (%d)\n",
                                    pTransferContext -> TransferLength,
                                    MaxTransferLength));
        pTransferContext -> TransferLength = MaxTransferLength;
    }

    //
    // Calculate number of pages in this transfer.
    //

    nTransferPages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(
        pTransferContext -> pTransferBuffer,
        pTransferContext -> TransferLength);

    if (nTransferPages > pde -> pAdapterDescriptor -> MaximumPhysicalPages) {
        SCSISCAN_KdPrint(MAX_TRACE,("Request number of pages (%d) greater than maximum (%d).\n",
                                    nTransferPages,
                                    pde -> pAdapterDescriptor -> MaximumPhysicalPages));

        //
        // Calculate maximum bytes to transfer that gaurantees that
        // we will not exceed the maximum number of page breaks,
        // assuming that the transfer may not be page alligned.
        //

        pTransferContext -> TransferLength = (pde -> pAdapterDescriptor -> MaximumPhysicalPages - 1) * PAGE_SIZE;
    }
} // end SSAdjustTransferSize()


VOID
SSSetTransferLengthToCdb(
    PCDB  pCdb,
    ULONG TransferLength
    )
/*++

Routine Description:
    Set transfer length to CDB due to its SCSI command.

Arguments:
    pCdb            -   pointer to CDB
    TransferLength  -   size of data to transfer
Return Value:

    none

--*/
{

    switch (pCdb->SEEK.OperationCode) {

        case 0x24:                  // Scanner SetWindow command
        case SCSIOP_READ_CAPACITY:  // Scanner GetWindow command
        case SCSIOP_READ:           // Scanner Read command
        case SCSIOP_WRITE:          // Scanner Send Command
        default:                    // All other commands
        {
            pCdb -> SEEK.Reserved2[2] = ((PFOUR_BYTE)&TransferLength) -> Byte0;
            pCdb -> SEEK.Reserved2[1] = ((PFOUR_BYTE)&TransferLength) -> Byte1;
            pCdb -> SEEK.Reserved2[0] = ((PFOUR_BYTE)&TransferLength) -> Byte2;

            break;
        }

        case 0x34       :           // Scanner GetDataBufferStatus Command
        {
            pCdb -> SEEK.Reserved2[2] = ((PFOUR_BYTE)&TransferLength) -> Byte0;
            pCdb -> SEEK.Reserved2[1] = ((PFOUR_BYTE)&TransferLength) -> Byte1;

            break;
        }

    }

} // end SSSetTransferLengthToCdb()

VOID
SSSendScannerRequest(
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp,
    PTRANSFER_CONTEXT pTransferContext,
    BOOLEAN Retry
    )
/*++

Routine Description:

Arguments:

Return Value:

    None.

--*/
{
    PSCSISCAN_DEVICE_EXTENSION      pde;
    PIO_STACK_LOCATION              pIrpStack;
    PIO_STACK_LOCATION              pNextIrpStack;
    PSRB                            pSrb;
    PCDB                            pCdb;
    PSCSISCAN_CMD                   pCmd;

    SCSISCAN_KdPrint(MAX_TRACE,("SendScannerRequest pirp=%lx TransferBuffer=%lx\n",
                                pIrp,pTransferContext->pTransferBuffer));

    pde = (PSCSISCAN_DEVICE_EXTENSION)pDeviceObject -> DeviceExtension;
    pIrpStack = IoGetCurrentIrpStackLocation( pIrp );
    pNextIrpStack = IoGetNextIrpStackLocation( pIrp );
    ASSERT(pTransferContext);
    pSrb = &(pTransferContext -> Srb);
    ASSERT(pSrb);
    pCmd = pTransferContext -> pCmd;
    ASSERT(pCmd);

    //
    // Write length to SRB.
    //

    pSrb -> Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set up IRP Address.
    //

    pSrb -> OriginalRequest = pIrp;

    pSrb -> Function = SRB_FUNCTION_EXECUTE_SCSI;

    pSrb -> DataBuffer = pTransferContext -> pTransferBuffer;

    //
    // Save byte count of transfer in SRB Extension.
    //

    pSrb -> DataTransferLength = pTransferContext -> TransferLength;

    //
    // Initialize the queue actions field.
    //

    pSrb -> QueueAction = SRB_SIMPLE_TAG_REQUEST;

    //
    // Queue sort key is not used.
    //

    pSrb -> QueueSortKey = 0;

    //
    // Indicate auto request sense by specifying buffer and size.
    //

    pSrb -> SenseInfoBuffer = pTransferContext -> pSenseBuffer;
    pSrb -> SenseInfoBufferLength = pCmd -> SenseLength;

    //
    // Set timeout value in seconds.
    //

    pSrb -> TimeOutValue = pde -> TimeOutValue;

    //
    // Zero status fields
    //


    pSrb -> SrbStatus = pSrb -> ScsiStatus = 0;
    pSrb -> NextSrb = 0;

    //
    // Get pointer to CDB in SRB.
    //

    pCdb = (PCDB)(pSrb -> Cdb);

    //
    // Set length of CDB.
    //

    pSrb -> CdbLength = pCmd -> CdbLength;

    //
    // Copy the user's CDB into our private CDB.
    //

    RtlCopyMemory(pCdb, pCmd -> Cdb, pCmd -> CdbLength);

    //
    // Set the srb flags.
    //

    pSrb -> SrbFlags = pCmd -> SrbFlags;

    //
    // Or in the default flags from the device object.
    //

    pSrb -> SrbFlags |= pde -> SrbFlags;

    if (Retry) {
                // Disable synchronous data transfers and
                // disable tagged queuing. This fixes some errors.

                SCSISCAN_KdPrint(MAX_TRACE,("SscsiScan :: Retrying \n"));

                //
                // Original code also added disable disconnect flag to SRB.
                // That action would lock SCSI bus and in a case when paging drive is
                // located on the same bus and scanner is taking long timeouts ( for example
                // when it is mechanically locked) memory manager would hit timeout and
                // bugcheck.
                //
                // pSrb -> SrbFlags |= SRB_FLAGS_DISABLE_DISCONNECT |
                //

                pSrb -> SrbFlags |= SRB_FLAGS_DISABLE_SYNCH_TRANSFER;
                pSrb -> SrbFlags &= ~SRB_FLAGS_QUEUE_ACTION_ENABLE;

                SCSISCAN_KdPrint(MAX_TRACE,("SSSendScannerRequest: Retry branch .Srb flags=(%x) \n",
                                 pSrb -> SrbFlags  ));

                pSrb -> QueueTag = SP_UNTAGGED;
    }

    //
    // Set up major SCSI function.
    //

    pNextIrpStack -> MajorFunction = IRP_MJ_SCSI;

    //
    // Save SRB address in next stack for port driver.
    //

    pNextIrpStack -> Parameters.Scsi.Srb = pSrb;

    //
    // Print out SRB fields
    //

    // SCSISCAN_KdPrint(MAX_TRACE,("SSSendScannerRequest: SRB ready. Flags=(%#X)Func=(%#x) DataLen=%d \nDataBuffer(16)=[%16s] \n",
     SCSISCAN_KdPrint(MAX_TRACE,("SSSendScannerRequest: SRB ready. Flags=(%#X)Func=(%#x) DataLen=%d \nDataBuffer(16)=[%lx] \n",
                         pSrb -> SrbFlags ,pSrb -> Function,
                         pSrb -> DataTransferLength,
                         pSrb -> DataBuffer));

    IoCallDriver(pde -> pStackDeviceObject, pIrp);

} // end SSSendScannerRequest()


NTSTATUS
SSReadWriteIoComplete(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp,
    IN PTRANSFER_CONTEXT pTransferContext
    )
/*++

Routine Description:
    This routine executes when the port driver has completed a request.
    It looks at the SRB status in the completing SRB and if not success
    it checks for valid request sense buffer information. If valid, the
    info is used to update status with more precise message of type of
    error. This routine deallocates the SRB.

Arguments:
    pDeviceObject - Supplies the device object which represents the logical
        unit.
    pIrp - Supplies the Irp which has completed.

Return Value:
    NT status

--*/
{
    PIO_STACK_LOCATION              pIrpStack;
    PIO_STACK_LOCATION              pNextIrpStack;
    NTSTATUS                        Status;
    BOOLEAN                         Retry;
    PSRB                            pSrb;
    UCHAR                           SrbStatus;
    PCDB                            pCdb;
    PSCSISCAN_CMD                   pCmd;

    SCSISCAN_KdPrint(MAX_TRACE,("ReadWriteIoComplete: IRP completed. %lx Transfer ctxt=%lx \n",
                                pIrp,pTransferContext->pTransferBuffer));

    pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
    pNextIrpStack = IoGetNextIrpStackLocation(pIrp);
    Status = STATUS_SUCCESS;

    ASSERT(NULL != pTransferContext);

    pSrb = &(pTransferContext -> Srb);
    SrbStatus = SRB_STATUS(pSrb -> SrbStatus);

    if (SrbStatus != SRB_STATUS_SUCCESS) {
        SCSISCAN_KdPrint(MAX_TRACE,("ReadWriteIoComplete: Irp error. %lx SRB %lx\n", pIrp, pSrb));

        //
        // Release the queue if it is frozen.
        //

        if (pSrb -> SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            SCSISCAN_KdPrint(MAX_TRACE,("ReadWriteIoComplete: Release queue. IRP %lx \n", pIrp));
           ClassReleaseQueue(pDeviceObject);
        }

        Retry = ClassInterpretSenseInfo(
                                        pDeviceObject,
                                        pSrb,
                                        pNextIrpStack->MajorFunction,
                                        0,
                                        MAXIMUM_RETRIES - ((ULONG)(UINT_PTR)pIrpStack->Parameters.Others.Argument4),
                                        &Status);

        if (Retry && pTransferContext -> RetryCount--) {
            SCSISCAN_KdPrint(MAX_TRACE,("ReadWriteIoComplete: Retry request %lx TransferBuffer=%lx \n",
                                        pIrp,pTransferContext->pTransferBuffer));
            IoSetCompletionRoutine(pIrp, SSReadWriteIoComplete, pTransferContext, TRUE, TRUE, FALSE);
            SSSendScannerRequest(pDeviceObject, pIrp, pTransferContext, TRUE);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }


        if (SRB_STATUS_DATA_OVERRUN == SrbStatus) {
            SCSISCAN_KdPrint(MAX_TRACE,("ReadWriteIoComplete: Data overrun %lx\n", pIrp));
            pTransferContext -> NBytesTransferred += pSrb -> DataTransferLength;
            Status = STATUS_SUCCESS;

        } else {
            SCSISCAN_KdPrint(MIN_TRACE,("ReadWriteIoComplete: Request failed. IRP %lx\n", pIrp));
            DEBUG_BREAKPOINT();
            pTransferContext -> NBytesTransferred = 0;
            Status = STATUS_IO_DEVICE_ERROR;
        }

    } else {

        pTransferContext -> NBytesTransferred += pSrb -> DataTransferLength;
        pTransferContext -> RemainingTransferLength -= pSrb -> DataTransferLength;
        pTransferContext -> pTransferBuffer += pSrb -> DataTransferLength;
        if (pTransferContext -> RemainingTransferLength > 0) {

            if ((LONG)(pTransferContext -> TransferLength) > pTransferContext -> RemainingTransferLength) {
                pTransferContext -> TransferLength = pTransferContext -> RemainingTransferLength;
                pCmd = pTransferContext -> pCmd;
                pCdb = (PCDB)pCmd -> Cdb;
                pCdb -> PRINT.TransferLength[2] = ((PFOUR_BYTE)&(pTransferContext -> TransferLength)) -> Byte0;
                pCdb -> PRINT.TransferLength[1] = ((PFOUR_BYTE)&(pTransferContext -> TransferLength)) -> Byte1;
                pCdb -> PRINT.TransferLength[0] = ((PFOUR_BYTE)&(pTransferContext -> TransferLength)) -> Byte2;
            }

            IoSetCompletionRoutine(pIrp, SSReadWriteIoComplete, pTransferContext, TRUE, TRUE, FALSE);
            SSSendScannerRequest(pDeviceObject, pIrp, pTransferContext, FALSE);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        Status = STATUS_SUCCESS;
    }

    pIrp -> IoStatus.Information = pTransferContext -> NBytesTransferred;

    ExFreePool(pTransferContext -> pCmd -> pSenseBuffer);
    ExFreePool(pTransferContext -> pCmd);
    ExFreePool(pTransferContext);

    pIrp -> IoStatus.Status = Status;

    SSDecrementIoCount( pDeviceObject );

    return Status;

} // end SSReadWriteIoComplete()



NTSTATUS
SSIoctlIoComplete(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp,
    IN PTRANSFER_CONTEXT pTransferContext
    )
/*++

Routine Description:
    This routine executes when an DevIoctl request has completed.

Arguments:
    pDeviceObject - Supplies the device object which represents the logical
        unit.
    pIrp - Supplies the Irp which has completed.
    pTransferContext - pointer to info about the request.

Return Value:
    NT status

--*/
{
    PIO_STACK_LOCATION              pIrpStack;
    NTSTATUS                        Status;
    PSRB                            pSrb;
    PSCSISCAN_CMD                   pCmd;
    PCDB                            pCdb;


    SCSISCAN_KdPrint(MAX_TRACE,("IoctlIoComplete: IRP completed. %lx\n", pIrp));
    pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
    Status = STATUS_SUCCESS;
    ASSERT(NULL != pTransferContext);
    pSrb = &(pTransferContext -> Srb);
    pCmd = pTransferContext -> pCmd;
    ASSERT(NULL != pCmd);

    //
    // Copy the SRB Status back into the user's SCSISCAN_CMD buffer.
    //

    *(pCmd -> pSrbStatus) = pSrb -> SrbStatus;

    //
    // If an error occurred on this transfer, release the frozen queue if necessary.
    //

    if (SRB_STATUS(pSrb -> SrbStatus) != SRB_STATUS_SUCCESS) {
        SCSISCAN_KdPrint(MIN_TRACE,("IoctlIoComplete: Irp error. %lx SRB status:%lx\n", pIrp, pSrb -> SrbStatus));

        if (pSrb -> SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            SCSISCAN_KdPrint(MAX_TRACE,("IoctlIoComplete: Release queue. IRP %lx \n", pIrp));
           ClassReleaseQueue(pDeviceObject);
        }
    } else {
        pTransferContext -> NBytesTransferred += pSrb -> DataTransferLength;
        pTransferContext -> RemainingTransferLength -= pSrb -> DataTransferLength;
        pTransferContext -> pTransferBuffer += pSrb -> DataTransferLength;
        if (pTransferContext -> RemainingTransferLength > 0) {

            if ((LONG)(pTransferContext -> TransferLength) > pTransferContext -> RemainingTransferLength) {
                pTransferContext -> TransferLength = pTransferContext -> RemainingTransferLength;
                pCmd = pTransferContext -> pCmd;
                pCdb = (PCDB)pCmd -> Cdb;

                //
                // SCSISCAN only supports 10bytes CDB fragmentation.
                //

                ASSERT(pCmd->CdbLength == 10);

                //
                // Fill in transfer length in the CDB.
                //

                SSSetTransferLengthToCdb((PCDB)pCmd -> Cdb, pTransferContext -> TransferLength);

            }

            IoSetCompletionRoutine(pIrp, SSIoctlIoComplete, pTransferContext, TRUE, TRUE, FALSE);
            SSSendScannerRequest(pDeviceObject, pIrp, pTransferContext, FALSE);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        Status = STATUS_SUCCESS;
    }

    //
    // Clean up and return.
    //

    if (pTransferContext -> pSrbStatusMdl) {
        MmUnlockPages(pTransferContext -> pSrbStatusMdl);
        IoFreeMdl(pTransferContext -> pSrbStatusMdl);

        //pTransferContext -> pSrbStatusMdl = NULL;
    }

    if (pTransferContext -> pSenseMdl) {
        MmUnlockPages(pTransferContext -> pSenseMdl);
        IoFreeMdl(pTransferContext -> pSenseMdl);

        //pTransferContext -> pSenseMdl = NULL;
    }

    pIrp -> IoStatus.Information = pTransferContext -> NBytesTransferred;
    pIrp -> IoStatus.Status = Status;

    ExFreePool(pTransferContext);

    SSDecrementIoCount( pDeviceObject );

    return Status;

} // end SSIoctlIoComplete()


NTSTATUS
SSCreateSymbolicLink(
    PSCSISCAN_DEVICE_EXTENSION  pde
    )
{

    NTSTATUS                      Status;
    UNICODE_STRING                uName;
    UNICODE_STRING                uName2;
    ANSI_STRING                   ansiName;
    CHAR                          aName[32];
    HANDLE                        hSwKey;

    PAGED_CODE();

    //
    // Create the symbolic link for this device.
    //

    sprintf(aName,"\\Device\\Scanner%d",pde -> DeviceInstance);
    RtlInitAnsiString(&ansiName, aName);
    RtlAnsiStringToUnicodeString(&uName, &ansiName, TRUE);

    sprintf(aName,"\\DosDevices\\Scanner%d",pde -> DeviceInstance);
    RtlInitAnsiString(&ansiName, aName);
    RtlAnsiStringToUnicodeString(&(pde -> SymbolicLinkName), &ansiName, TRUE);

    Status = IoCreateSymbolicLink( &(pde -> SymbolicLinkName), &uName );

    RtlFreeUnicodeString( &uName );

    if (STATUS_SUCCESS != Status ) {
        SCSISCAN_KdPrint(MIN_TRACE,("Cannot create symbolic link.\n"));
        DEBUG_BREAKPOINT();
        Status = STATUS_NOT_SUPPORTED;
        return Status;
    }

    //
    // Now, stuff the symbolic link into the CreateFileName key so that STI can find the device.
    //

    IoOpenDeviceRegistryKey( pde -> pPhysicalDeviceObject,
                             PLUGPLAY_REGKEY_DRIVER, KEY_WRITE, &hSwKey);

    RtlInitUnicodeString(&uName,L"CreateFileName");
    sprintf(aName,"\\\\.\\Scanner%d",pde -> DeviceInstance);
    RtlInitAnsiString(&ansiName, aName);
    RtlAnsiStringToUnicodeString(&uName2, &ansiName, TRUE);
    ZwSetValueKey(hSwKey,&uName,0,REG_SZ,uName2.Buffer,uName2.Length);
    RtlFreeUnicodeString( &uName2 );

    return STATUS_SUCCESS;
}


NTSTATUS
SSDestroySymbolicLink(
    PSCSISCAN_DEVICE_EXTENSION  pde
    )
{

    UNICODE_STRING                uName;
    UNICODE_STRING                uName2;
    ANSI_STRING                   ansiName;
    CHAR                          aName[32];
    HANDLE                        hSwKey;

    PAGED_CODE();

    SCSISCAN_KdPrint(MIN_TRACE,("DestroySymbolicLink\n"));

    //
    // Delete the symbolic link to this device.
    //

    IoDeleteSymbolicLink( &(pde -> SymbolicLinkName) );

    //
    // Remove the CreateFile name from the s/w key.
    //

    IoOpenDeviceRegistryKey( pde -> pPhysicalDeviceObject,
                             PLUGPLAY_REGKEY_DRIVER, KEY_WRITE, &hSwKey);

    RtlInitUnicodeString(&uName,L"CreateFileName");
    memset(aName, 0, sizeof(aName));
    RtlInitAnsiString(&ansiName, aName);
    RtlAnsiStringToUnicodeString(&uName2, &ansiName, TRUE);
    ZwSetValueKey(hSwKey,&uName,0,REG_SZ,uName2.Buffer,uName2.Length);
    RtlFreeUnicodeString( &uName2 );
    RtlFreeUnicodeString( &(pde -> SymbolicLinkName) );

    ZwClose(hSwKey);

    return STATUS_SUCCESS;

}


VOID
SSIncrementIoCount(
    IN PDEVICE_OBJECT pDeviceObject
    )
/*++

Routine Description:

Arguments:

Return Value:


--*/
{
    PSCSISCAN_DEVICE_EXTENSION  pde;

    pde = (PSCSISCAN_DEVICE_EXTENSION)(pDeviceObject -> DeviceExtension);
    InterlockedIncrement(&pde -> PendingIoCount);
}


LONG
SSDecrementIoCount(
    IN PDEVICE_OBJECT pDeviceObject
    )
/*++

Routine Description:

Arguments:

Return Value:


--*/
{
    PSCSISCAN_DEVICE_EXTENSION  pde;
    LONG                        ioCount;

    pde = (PSCSISCAN_DEVICE_EXTENSION)(pDeviceObject -> DeviceExtension);

    ioCount = InterlockedDecrement(&pde -> PendingIoCount);

//    SCSISCAN_KdPrint(MAX_TRACE,("Pending io count = %x\n",ioCount));

    if (0 == ioCount) {
        KeSetEvent(&pde -> PendingIoEvent,
                   1,
                   FALSE);
    }

    return ioCount;
}


NTSTATUS
SSDeferIrpCompletion(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp,
    IN PVOID Context
    )
/*++

Routine Description:

    This routine is called when the port driver completes an IRP.

Arguments:

    pDeviceObject - Pointer to the device object for the class device.

    pIrp - Irp completed.

    Context - Driver defined context.

Return Value:

    The function value is the final status from the operation.

--*/
{
    PKEVENT pEvent = Context;

    KeSetEvent(pEvent,
               1,
               FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;

}


NTSTATUS
SSPower(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP           pIrp
    )
/*++

Routine Description:

    Process the Power IRPs sent to the PDO for this device.

Arguments:

    pDeviceObject - pointer to the functional device object (FDO) for this device.
    pIrp          - pointer to an I/O Request Packet

Return Value:

    NT status code

--*/
{
    NTSTATUS                        Status;
    PSCSISCAN_DEVICE_EXTENSION      pde;
    PIO_STACK_LOCATION              pIrpStack;
    BOOLEAN                         hookIt = FALSE;

    PAGED_CODE();

    SSIncrementIoCount( pDeviceObject );

    pde       = (PSCSISCAN_DEVICE_EXTENSION)pDeviceObject -> DeviceExtension;
    pIrpStack = IoGetCurrentIrpStackLocation( pIrp );
    Status    = STATUS_SUCCESS;

    switch (pIrpStack -> MinorFunction) {
        case IRP_MN_SET_POWER:
            SCSISCAN_KdPrint(MIN_TRACE,("IRP_MN_SET_POWER\n"));
            PoStartNextPowerIrp(pIrp);
            IoSkipCurrentIrpStackLocation  (pIrp);
            Status = PoCallDriver(pde -> pStackDeviceObject, pIrp);
            SSDecrementIoCount(pDeviceObject);
            break; /* IRP_MN_QUERY_POWER */

        case IRP_MN_QUERY_POWER:
            SCSISCAN_KdPrint(MIN_TRACE,("IRP_MN_QUERY_POWER\n"));
            PoStartNextPowerIrp(pIrp);
            IoSkipCurrentIrpStackLocation  (pIrp);
            Status = PoCallDriver(pde -> pStackDeviceObject, pIrp);
            SSDecrementIoCount(pDeviceObject);
            break; /* IRP_MN_QUERY_POWER */

        default:
            SCSISCAN_KdPrint(MIN_TRACE,("Unknown power message (%x)\n",pIrpStack->MinorFunction));
            PoStartNextPowerIrp(pIrp);
            IoSkipCurrentIrpStackLocation  (pIrp);
            Status = PoCallDriver(pde -> pStackDeviceObject, pIrp);
            SSDecrementIoCount(pDeviceObject);

    } /* irpStack->MinorFunction */

    return Status;
}


VOID
SSUnload(
    IN PDRIVER_OBJECT pDriverObject
    )
/*++

Routine Description:

    This routine is called when the driver is unloaded.

Arguments:
    pDriverObject - Pointer to the driver object.evice object for the class device.

Return Value:
    none.

--*/
{
    PAGED_CODE();

    SCSISCAN_KdPrint(MIN_TRACE,("Driver unloaded.\n"));
}


NTSTATUS
ScsiScanHandleInterface(
    PDEVICE_OBJECT      DeviceObject,
    PUNICODE_STRING     InterfaceName,
    BOOLEAN             Create
    )
/*++

Routine Description:

Arguments:

    DeviceObject    - Supplies the device object.

Return Value:

    None.

--*/
{

    NTSTATUS           Status;

    if (Create) {

        Status=IoRegisterDeviceInterface(
            DeviceObject,
            &GUID_DEVCLASS_IMAGE,
            NULL,
            InterfaceName
            );

        SCSISCAN_KdPrint(MIN_TRACE,("Called IoRegisterDeviceInterface . Returned=0x%X\n",Status));

        if (NT_SUCCESS(Status)) {

            IoSetDeviceInterfaceState(
                InterfaceName,
                TRUE
                );

            SCSISCAN_KdPrint(MIN_TRACE,("Called IoSetDeviceInterfaceState(TRUE) . \n"));


        }

    } else {

        if (InterfaceName->Buffer != NULL) {

            IoSetDeviceInterfaceState(
                InterfaceName,
                FALSE
                );

            SCSISCAN_KdPrint(MIN_TRACE,("Called IoSetDeviceInterfaceState(FALSE) . \n"));

            RtlFreeUnicodeString(
                InterfaceName
                );

            InterfaceName->Buffer = NULL;

        }

    }

    return Status;

}


NTSTATUS
SSCallNextDriverSynch(
    IN PSCSISCAN_DEVICE_EXTENSION   pde,
    IN PIRP                         pIrp
)
/*++

Routine Description:

    Calls lower driver and waits for result

Arguments:

    DeviceExtension - pointer to device extension
    Irp - pointer to IRP

Return Value:

    none.

--*/
{
    KEVENT          Event;
    PIO_STACK_LOCATION IrpStack;
    NTSTATUS        Status;

    SCSISCAN_KdPrint(MAX_TRACE,("SSCallNextDriverSynch: Enter..\n"));

    IrpStack = IoGetCurrentIrpStackLocation(pIrp);

    //
    // Copy IRP stack to the next.
    //

    IoCopyCurrentIrpStackLocationToNext(pIrp);

    //
    // Initialize synchronizing event.
    //

    KeInitializeEvent(&Event,
                      SynchronizationEvent,
                      FALSE);

    //
    // Set completion routine
    //

    IoSetCompletionRoutine(pIrp,
                           SSDeferIrpCompletion,
                           &Event,
                           TRUE,
                           TRUE,
                           TRUE);

    //
    // Call down
    //

    Status = IoCallDriver(pde -> pStackDeviceObject, pIrp);

    if (Status == STATUS_PENDING) {

        //
        // Waiting for the completion.
        //

        SCSISCAN_KdPrint(MAX_TRACE,("SSCallNextDriverSynch: STATUS_PENDING. Wait for event.\n"));
        KeWaitForSingleObject(&Event,
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);
        Status = pIrp -> IoStatus.Status;
    }

    //
    // Return
    //

    SCSISCAN_KdPrint(MAX_TRACE,("SSCallNextDriverSynch: Leaving.. Status = %x\n", Status));
    return (Status);
}


#if DBG

VOID
MyDebugInit(
    IN  PUNICODE_STRING pRegistryPath
)
/*++

Routine Description:

    Read SCSISCAN_KdPrintLevel key from driver's registry if exists.

Arguments:

    pRegistryPath   -   pointer to a unicode string representing the path
                        to driver-specific key in the registry

Return Value:

    none.

--*/
{

    HANDLE                          hDriverRegistry;
    OBJECT_ATTRIBUTES               ObjectAttributes;
    UNICODE_STRING                  unicodeKeyName;
    ULONG                           DataSize;
    PKEY_VALUE_PARTIAL_INFORMATION  pValueInfo;
    NTSTATUS                        Status;

    SCSISCAN_KdPrint(MAX_TRACE,("MyDebugInit: Enter... \n"));

    //
    // Initialize status.
    //

    Status          = STATUS_SUCCESS;
    hDriverRegistry = NULL;
    pValueInfo      = NULL;
    pValueInfo      = NULL;
    DataSize        = 0;

    //
    // Initialize object attribute and open registry key.
    //

    RtlZeroMemory(&ObjectAttributes, sizeof(ObjectAttributes));
    InitializeObjectAttributes(&ObjectAttributes,
                               pRegistryPath,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    Status = ZwOpenKey(&hDriverRegistry,
                       KEY_READ,
                       &ObjectAttributes);
    if(!NT_SUCCESS(Status)){
        SCSISCAN_KdPrint(MAX_TRACE,("MyDebugInit: ERROR!! Can't open driver registry key.\n"));
        goto MyDebugInit_return;
    }

    //
    // Read "SCSISCAN_KdPrintLevel" key.
    //

    SCSISCAN_KdPrint(MAX_TRACE,("MyDebugInit: Query %wZ\\%ws.\n", pRegistryPath, REG_DEBUGLEVEL));

    //
    // Query required size.
    //

    RtlInitUnicodeString(&unicodeKeyName, REG_DEBUGLEVEL);
    Status = ZwQueryValueKey(hDriverRegistry,
                             &unicodeKeyName,
                             KeyValuePartialInformation,
                             NULL,
                             0,
                             &DataSize);
    if( (Status != STATUS_BUFFER_OVERFLOW)
     && (Status != STATUS_BUFFER_TOO_SMALL)
     && (Status != STATUS_SUCCESS) )
    {
        SCSISCAN_KdPrint(MAX_TRACE, ("MyDebugInit: ERROR!! Cannot retrieve reqired data size.\n"));
        goto MyDebugInit_return;
    }

    //
    // Check size of data.
    //

    if (MAX_TEMPBUF < DataSize) {
        SCSISCAN_KdPrint(MIN_TRACE, ("MyDebugInit: ERROR!! DataSize (0x%x) is too big.\n", DataSize));
        goto MyDebugInit_return;
    }

    if (0 == DataSize) {
        SCSISCAN_KdPrint(MIN_TRACE, ("MyDebugInit: ERROR!! Cannot retrieve required data size.\n"));
        goto MyDebugInit_return;
    }

    //
    // Allocate memory for temp buffer. size +2 for NULL.
    //

    pValueInfo = ExAllocatePool(NonPagedPool, DataSize+2);
    if(NULL == pValueInfo){
        SCSISCAN_KdPrint(MAX_TRACE, ("MyDebugInit: ERROR!! Buffer allocate failed.\n"));
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto MyDebugInit_return;
    }
    RtlZeroMemory(pValueInfo, DataSize+2);

    //
    // Query specified value.
    //

    Status = ZwQueryValueKey(hDriverRegistry,
                             &unicodeKeyName,
                             KeyValuePartialInformation,
                             pValueInfo,
                             DataSize,
                             &DataSize);
    if(!NT_SUCCESS(Status)){
        SCSISCAN_KdPrint(MAX_TRACE, ("MyDebugInit: ERROR!! ZwQueryValueKey failed.\n"));
        goto MyDebugInit_return;
    }

    //
    // Set SCSISCAN_KdPrintLevel.
    //

    SCSISCAN_KdPrint(MIN_TRACE, ("MyDebugInit: Reg-key found. DebugTraceLevel=0x%x.\n", *((PULONG)pValueInfo->Data)));
    DebugTraceLevel = *((PULONG)pValueInfo->Data);

MyDebugInit_return:

    //
    // Clean up.
    //

    if(pValueInfo){
        ExFreePool(pValueInfo);
    }

    SCSISCAN_KdPrint(MAX_TRACE,("MyDebugInit: Leaving... Status=0x%x, Ret=VOID.\n", Status));
    return;
}

#endif // DBG


