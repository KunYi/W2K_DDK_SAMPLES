/*++

Copyright (c) 1990-2000  Microsoft Corporation

Module Name:

    packet.c

Abstract:


Author:


Environment:

    Kernel mode only.

Notes:


Future:



Revision History:

  Fixed bugs and converted to NDIS 5.0. This driver handles 
  dynamic binding and unbinding to underlying NICs, receives
  pnp and power management callbacks, and implements 
  ReceivePacketHandler. 
                            - Eliyas Yakub June, 1999
  
--*/

#include "ntddk.h"
#include "ndis.h"
#include "ntddpack.h"
#include "packet.h"
#include "stdio.h"


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine initializes the Packet driver.

Arguments:

    DriverObject - Pointer to driver object created by system.

    RegistryPath - Pointer to the Unicode name of the registry path
        for this driver.

Return Value:

    NT Status code
    
--*/

{

    NDIS_PROTOCOL_CHARACTERISTICS   protocolChar;
    NTSTATUS                        status = STATUS_SUCCESS;
    NDIS_STRING                     protoName = NDIS_STRING_CONST("Packet");     
    UNICODE_STRING                  ntDeviceName;
    UNICODE_STRING                  win32DeviceName;
    BOOLEAN                         fSymbolicLink = FALSE;
    PDEVICE_OBJECT                  deviceObject;

    DebugPrint(("\n\nDriverEntry\n"));
    
    Globals.DriverObject = DriverObject;

    //
    // Save the RegistryPath.
    //

    Globals.RegistryPath.MaximumLength = RegistryPath->Length +
                                          sizeof(UNICODE_NULL);
    Globals.RegistryPath.Length = RegistryPath->Length;
    Globals.RegistryPath.Buffer = ExAllocatePool(
                                       PagedPool,
                                       Globals.RegistryPath.MaximumLength
                                       );    

    if (!Globals.RegistryPath.Buffer) {

        DebugPrint (("Couldn't allocate pool for registry path."));

        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    RtlCopyUnicodeString(&Globals.RegistryPath, RegistryPath);

    RtlInitUnicodeString(&ntDeviceName, NT_DEVICE_NAME);

    //
    // Create a control device object for this driver.
    // Application can send an IOCTL to this device to get 
    // bound adapter information.
    //

    status = IoCreateDevice (DriverObject,
                             0,
                             &ntDeviceName,
                             FILE_DEVICE_UNKNOWN,
                             0,
                             FALSE,
                             &deviceObject);

    
    if (!NT_SUCCESS (status)) {
        //
        // Either not enough memory to create a deviceobject or another
        // deviceobject with the same name exits. This could happen
        // if you install another instance of this device.
        //
        goto ERROR;
    }

    RtlInitUnicodeString(&win32DeviceName, DOS_DEVICE_NAME);

    status = IoCreateSymbolicLink( &win32DeviceName, &ntDeviceName );

    if (!NT_SUCCESS(status))    // If we couldn't create the link then
    {                           //  abort installation.
        goto ERROR;
    }

    fSymbolicLink = TRUE; // symboliclink is created
    
    deviceObject->Flags |= DO_BUFFERED_IO;
    Globals.ControlDeviceObject = deviceObject;

    InitializeListHead(&Globals.AdapterList);
    KeInitializeSpinLock(&Globals.GlobalLock);

    //
    // Initialize the protocol characterstic structure
    //
    
    NdisZeroMemory(&protocolChar,sizeof(NDIS_PROTOCOL_CHARACTERISTICS));

    protocolChar.MajorNdisVersion            = 5;
    protocolChar.MinorNdisVersion            = 0;
    protocolChar.Name                        = protoName;
    protocolChar.OpenAdapterCompleteHandler  = PacketOpenAdapterComplete;
    protocolChar.CloseAdapterCompleteHandler = PacketCloseAdapterComplete;
    protocolChar.SendCompleteHandler         = PacketSendComplete;
    protocolChar.TransferDataCompleteHandler = PacketTransferDataComplete;
    protocolChar.ResetCompleteHandler        = PacketResetComplete;
    protocolChar.RequestCompleteHandler      = PacketRequestComplete;
    protocolChar.ReceiveHandler              = PacketReceiveIndicate;
    protocolChar.ReceiveCompleteHandler      = PacketReceiveComplete;
    protocolChar.StatusHandler               = PacketStatus;
    protocolChar.StatusCompleteHandler       = PacketStatusComplete;
    protocolChar.BindAdapterHandler          = PacketBindAdapter;
    protocolChar.UnbindAdapterHandler        = PacketUnbindAdapter;
    protocolChar.UnloadHandler               = NULL;
    protocolChar.ReceivePacketHandler        = PacketReceivePacket;
    protocolChar.PnPEventHandler             = PacketPNPHandler;

    //
    // Register as a protocol driver
    //
    
    NdisRegisterProtocol(
        &status,
        &Globals.NdisProtocolHandle,
        &protocolChar,
        sizeof(NDIS_PROTOCOL_CHARACTERISTICS));

    if (status != NDIS_STATUS_SUCCESS) {
        DebugPrint(("Failed to register protocol with NDIS\n"));
        status = STATUS_UNSUCCESSFUL;
        goto ERROR;        
    }
    
    //
    // Now set only the dispatch points we would like to handle.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = PacketOpen;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]  = PacketClose;
    DriverObject->MajorFunction[IRP_MJ_READ]   = PacketRead;
    DriverObject->MajorFunction[IRP_MJ_WRITE]  = PacketWrite;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP]  = PacketCleanup;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = PacketIoControl;
    DriverObject->DriverUnload = PacketUnload;
       
    return(STATUS_SUCCESS);

ERROR:
    if(deviceObject)
        IoDeleteDevice(deviceObject);
    if(fSymbolicLink)
        IoDeleteSymbolicLink(&win32DeviceName);
    if(Globals.RegistryPath.Buffer)        
        ExFreePool(Globals.RegistryPath.Buffer);

    return status;


}


VOID
PacketUnload(
    IN PDRIVER_OBJECT DriverObject
    )
/*++

Routine Description:

    Free all the allocated resources, etc.

Arguments:

    DriverObject - pointer to a driver object.

Return Value:

    VOID.

--*/
{

    NDIS_STATUS        status;
    UNICODE_STRING     win32DeviceName;

    DebugPrint(("Unload Enter\n"));

    //
    // First delete the Control deviceobject and the corresponding
    // symbolicLink
    //

    RtlInitUnicodeString(&win32DeviceName, DOS_DEVICE_NAME);
    IoDeleteSymbolicLink(&win32DeviceName);           
    if(Globals.ControlDeviceObject)
        IoDeleteDevice(Globals.ControlDeviceObject);
    //
    // Unbind from all the adapters. The system removes the driver code 
    // pages from the memory as soon as the unload returns. So you 
    // must wait for all the CloseAdapterCompleteHandler to finish 
    // before returning from the unload routine. You don't any callbacks
    // to trigger after the driver is unloaded.
    // 
    
    while(DriverObject->DeviceObject)
    {
        PacketUnbindAdapter(&status,
                    DriverObject->DeviceObject->DeviceExtension,
                    NULL);
    }

    if(Globals.RegistryPath.Buffer)
        ExFreePool(Globals.RegistryPath.Buffer);

    DebugPrint(("Deregister\n"));

    NdisDeregisterProtocol(
        &status,
        Globals.NdisProtocolHandle
        );
    DebugPrint(("Unload Exit\n"));

}




NTSTATUS
PacketIoControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the dispatch routine for Filter OID and Reset requests.

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/

{

    POPEN_INSTANCE      open;
    PIO_STACK_LOCATION  irpSp;
    PINTERNAL_REQUEST   pRequest;
    ULONG               functionCode;
    NDIS_STATUS         status;
    ULONG               dataLength =0;

    DebugPrint(("IoControl\n"));
    
    irpSp = IoGetCurrentIrpStackLocation(Irp);

    functionCode=irpSp->Parameters.DeviceIoControl.IoControlCode;

    //
    // Check whether this request is to get bound adapter list.
    //
    if (functionCode == IOCTL_ENUM_ADAPTERS) {
        //
        // If the request is not made to the controlobject, fail
        // the request.
        //
        if(DeviceObject != Globals.ControlDeviceObject) {
            status = STATUS_INVALID_DEVICE_REQUEST;
        } else {
            status = PacketGetAdapterList(
                            Irp->AssociatedIrp.SystemBuffer, 
                            irpSp->Parameters.DeviceIoControl.OutputBufferLength,
                            &dataLength
                            );        
        }
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = dataLength;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    //
    // Increment the outstanding IRP count.
    //
    open =  DeviceObject->DeviceExtension;
    IoIncrement(open);

    //
    // Check to see whether you are still bound to the adapter
    //

    if(!open->Bound)
    {
        Irp->IoStatus.Status = status = STATUS_UNSUCCESSFUL;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoDecrement(open);
        return status;
    }


    DebugPrint(("Function code is %08lx  buff size=%08lx  %08lx\n",
            functionCode,irpSp->Parameters.DeviceIoControl.InputBufferLength,
            irpSp->Parameters.DeviceIoControl.OutputBufferLength));

    //
    // Important: Since we have marked the IRP pending, we must return 
    // STATUS_PENDING even we happen to complete the IRP synchronously.
    // 
    
    IoMarkIrpPending(Irp);

    if (functionCode == IOCTL_PROTOCOL_RESET) {


        DebugPrint(("IoControl - Reset request\n"));

        //
        // Since NDIS doesn't have an interface to cancel a request
        // pending at miniport, we cannot set a cancel routine.
        // As a result if the application that made the request
        // terminates, we wait in the Cleanup routine for all pending
        // NDIS requests to complete.
        //
        
        ExInterlockedInsertTailList(
                &open->ResetIrpList,
                &Irp->Tail.Overlay.ListEntry,
                &open->ResetQueueLock);


        NdisReset(
            &status,
            open->AdapterHandle
            );


        if (status != NDIS_STATUS_PENDING) {

            DebugPrint(("IoControl - ResetComplete being called\n"));

            PacketResetComplete(
                open,
                status
                );

        }

    } else {
        //
        //  See if it is an Ndis request
        //
        PPACKET_OID_DATA    OidData=Irp->AssociatedIrp.SystemBuffer;

        pRequest = ExAllocatePool(NonPagedPool, sizeof(INTERNAL_REQUEST));

        if(NULL == pRequest)
        {
            Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            IoCompleteRequest (Irp, IO_NO_INCREMENT);
            IoDecrement(open);
            return STATUS_PENDING;
        }         
        pRequest->Irp=Irp;

        if (((functionCode == IOCTL_PROTOCOL_SET_OID) 
                        || (functionCode == IOCTL_PROTOCOL_QUERY_OID))
            &&
            (irpSp->Parameters.DeviceIoControl.InputBufferLength 
                        == irpSp->Parameters.DeviceIoControl.OutputBufferLength)
            &&
            (irpSp->Parameters.DeviceIoControl.InputBufferLength 
                        >= sizeof(PACKET_OID_DATA))
            &&
            (irpSp->Parameters.DeviceIoControl.InputBufferLength 
                        >= sizeof(PACKET_OID_DATA)-1+OidData->Length)) {

            DebugPrint(("IoControl: Request: Oid=%08lx, Length=%08lx\n",
                            OidData->Oid,OidData->Length));

            //
            //  The buffer is valid
            //
            if (functionCode == IOCTL_PROTOCOL_SET_OID) {

                pRequest->Request.RequestType=NdisRequestSetInformation;
                pRequest->Request.DATA.SET_INFORMATION.Oid=OidData->Oid;

                pRequest->Request.DATA.SET_INFORMATION.InformationBuffer=
                                                                OidData->Data;
                pRequest->Request.DATA.SET_INFORMATION.InformationBufferLength=
                                                           OidData->Length;


            } else {


                pRequest->Request.RequestType=NdisRequestQueryInformation;
                pRequest->Request.DATA.QUERY_INFORMATION.Oid= OidData->Oid;

                pRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer=
                                        OidData->Data;
                pRequest->Request.DATA.QUERY_INFORMATION.InformationBufferLength=
                                        OidData->Length;

            }

            //
            //  submit the request
            //
            NdisRequest(
                &status,
                open->AdapterHandle,
                &pRequest->Request
                );

        } else {
            //
            //  Buffer too small. The irp is completed by
            //  PacketRequestComplete routine.
            //
            status=NDIS_STATUS_FAILURE;
            pRequest->Request.DATA.SET_INFORMATION.BytesRead=0;
            pRequest->Request.DATA.QUERY_INFORMATION.BytesWritten=0;

        }

        if (status != NDIS_STATUS_PENDING) {

            DebugPrint(("Calling RequestCompleteHandler\n"));

            PacketRequestComplete(
                open,
                &pRequest->Request,
                status
                );
                
        }

    }
    
    return STATUS_PENDING;

}






VOID
PacketRequestComplete(
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS   Status
    )
/*++

Routine Description:

    This routine is called when a protocol-initiated query or set 
    operation, begun with a call to NdisRequest that returned 
    NDIS_STATUS_PENDING, is completed. 

Arguments:


Return Value:


--*/

{
    POPEN_INSTANCE      open;
    PIO_STACK_LOCATION  irpSp;
    PIRP                irp;
    PINTERNAL_REQUEST   pRequest;
    UINT                functionCode;

    PPACKET_OID_DATA    OidData;

    DebugPrint(("RequestComplete\n"));

    open = (POPEN_INSTANCE)ProtocolBindingContext;

    pRequest=CONTAINING_RECORD(NdisRequest,INTERNAL_REQUEST,Request);
    irp = pRequest->Irp;

    if(Status == NDIS_STATUS_SUCCESS)
    {
        irpSp = IoGetCurrentIrpStackLocation(irp);

        functionCode=irpSp->Parameters.DeviceIoControl.IoControlCode;

        OidData = irp->AssociatedIrp.SystemBuffer;

        if (functionCode == IOCTL_PROTOCOL_SET_OID) {

            OidData->Length=pRequest->Request.DATA.SET_INFORMATION.BytesRead;

        } else {

            if (functionCode == IOCTL_PROTOCOL_QUERY_OID) {

                OidData->Length=pRequest->Request.DATA.QUERY_INFORMATION.BytesWritten;

            }

        }

        irp->IoStatus.Status = STATUS_SUCCESS;
        irp->IoStatus.Information=irpSp->Parameters.DeviceIoControl.InputBufferLength;
    } else {
        irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
        irp->IoStatus.Information = 0;
    }
    
    ExFreePool(pRequest);
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    IoDecrement(open);

    return;
}



VOID
PacketStatus(
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN NDIS_STATUS   Status,
    IN PVOID         StatusBuffer,
    IN UINT          StatusBufferSize
    )
/*++

Routine Description:

    This routine is called to handle status changes indicated 
    by the underlying NDIS driver. 

Arguments:


Return Value:


--*/

{

    DebugPrint(("Indication Status: %0x, StatusBufferSize: %d\n",
                            Status, StatusBufferSize));

    return;

}



VOID
PacketStatusComplete(
    IN NDIS_HANDLE  ProtocolBindingContext
    )
/*++

Routine Description:

    This routine is called by NDIS, along with PacketStatus, 
    to report the start and end of an NDIS- or NIC-driver-
    initiated status indicate operation. 

Arguments:


Return Value:


--*/
{

    DebugPrint(("StatusIndicationComplete\n"));

    return;

}

NTSTATUS
PacketGetAdapterList(
    IN  PVOID              Buffer,
    IN  ULONG              Length,
    IN  OUT PULONG         DataLength
    )

/*++

Routine Description:

    This routine walks the adapter list and gets the symbolic
    link and NIC description and fills it in the Buffer.
    The format of the information is given below.
    
Arguments:


Return Value:

--*/
{
    ULONG               requiredLength = 0, numOfAdapters = 0;
    KIRQL               oldIrql;
    PLIST_ENTRY         thisEntry, listHead;
    POPEN_INSTANCE      open;   

    DebugPrint(("Enter PacketGetAdapterList\n"));

    KeAcquireSpinLock(&Globals.GlobalLock, &oldIrql);

    //
    // Walks the list to find out total space required for AdapterName
    // and Symbolic Link.
    //
    listHead = &Globals.AdapterList;
    
    for(thisEntry = listHead->Flink; 
        thisEntry != listHead;
        thisEntry = thisEntry->Flink)
    {
        open = CONTAINING_RECORD(thisEntry, OPEN_INSTANCE, AdapterListEntry);
        
        requiredLength += open->AdapterName.Length + sizeof(UNICODE_NULL); 
        requiredLength += open->SymbolicLink.Length + sizeof(UNICODE_NULL); 
        numOfAdapters++; 
    }

    //
    // We will return the data in the following format: 
    // numOfAdapters + One_Or_More("AdapterName\0" + "SymbolicLink\0") + UNICODE_NULL
    // So let's include the numOfAdapters and UNICODE_NULL size
    // to the total length.
    //
    
    requiredLength += sizeof(ULONG) + sizeof(UNICODE_NULL);     

    *DataLength = requiredLength;
    
    if(requiredLength > Length) {
        KeReleaseSpinLock(&Globals.GlobalLock, oldIrql);
        return STATUS_BUFFER_TOO_SMALL;
    }

    *(PULONG)Buffer = numOfAdapters;
    (PCHAR)Buffer += sizeof(ULONG);
    
    //
    // Copy the name and symbolic link of each adapter.
    //
    
    for(thisEntry = listHead->Flink; 
        thisEntry != listHead;
        thisEntry = thisEntry->Flink)
    {
        open = CONTAINING_RECORD(thisEntry, OPEN_INSTANCE, AdapterListEntry);
        
        RtlCopyMemory(Buffer, open->AdapterName.Buffer,
                            open->AdapterName.Length+sizeof(WCHAR));

        (PCHAR)Buffer += open->AdapterName.Length+sizeof(WCHAR);
        
        RtlCopyMemory(Buffer, open->SymbolicLink.Buffer,
                            open->SymbolicLink.Length+sizeof(WCHAR));

        (PCHAR)Buffer += open->SymbolicLink.Length+sizeof(WCHAR);
                            
    }
    
    *(PWCHAR)Buffer = UNICODE_NULL;
    
    KeReleaseSpinLock(&Globals.GlobalLock, oldIrql);
    
    return STATUS_SUCCESS;

}

VOID
PacketBindAdapter(
    OUT PNDIS_STATUS            Status,
    IN  NDIS_HANDLE             BindContext,
    IN  PNDIS_STRING            DeviceName,
    IN  PVOID                   SystemSpecific1,
    IN  PVOID                   SystemSpecific2
    )
/*++

Routine Description:

    Called by NDIS to bind to a miniport below.

Arguments:

    Status             - Return status of bind here.
    BindContext        - Can be passed to NdisCompleteBindAdapter 
                         if this call is pended.
    DeviceName         - Device name to bind to. This is passed to 
                            NdisOpenAdapter.
    SystemSpecific1    - Can be passed to NdisOpenProtocolConfiguration to 
                            read per-binding information
    SystemSpecific2    - Unused for NDIS 4.0.


Return Value:

    NDIS_STATUS_PENDING    if this call is pended. In this case call 
                           NdisCompleteBindAdapter to complete.
    Anything else         Completes this call synchronously

--*/
{
    NDIS_STATUS         status;
    UINT                mediumIndex;
    USHORT              length;
    POPEN_INSTANCE      open = NULL;
    UNICODE_STRING      unicodeDeviceName;
    PDEVICE_OBJECT      deviceObject = NULL;
    PWSTR               symbolicLink = NULL, deviceNameStr = NULL;
    NDIS_MEDIUM         mediumArray=NdisMedium802_3;// This sample only 
                                                    //supports Ethernet medium.
    DebugPrint(("Binding DeviceName %ws\n", DeviceName->Buffer));

    do{

        //
        // Create a deviceobject for every adapter we bind to.
        // To make a name for the deviceObject, we will append Packet_ 
        // to the name portion of the input DeviceName.
        //

        unicodeDeviceName.Buffer = NULL;
        length = DeviceName->Length + 7 * sizeof(WCHAR) + sizeof(UNICODE_NULL);

        deviceNameStr = ExAllocatePool(NonPagedPool, length);
        if (!deviceNameStr) {

            DebugPrint(("Memory allocation for create symbolic failed\n"));
            *Status = NDIS_STATUS_FAILURE;
            break;
        }
        swprintf(deviceNameStr, L"\\Device\\Packet_%ws", &DeviceName->Buffer[8]);
        RtlInitUnicodeString(&unicodeDeviceName, deviceNameStr);
        
        DebugPrint(("Exported DeviceName %ws\n", unicodeDeviceName.Buffer));

        //
        // Create the deviceobject
        //
        status = IoCreateDevice(
                    Globals.DriverObject,
                    sizeof(OPEN_INSTANCE),
                    &unicodeDeviceName,
                    FILE_DEVICE_PROTOCOL,
                    0,
                    TRUE, // only one handle to the device at a time.
                    &deviceObject
                    );
       
        if (status != STATUS_SUCCESS) {

            DebugPrint(("CreateDevice Failed: %x\n", status));
            *Status = NDIS_STATUS_FAILURE;
            break;
        }
        
        deviceObject->Flags |= DO_DIRECT_IO;

        open  =  (POPEN_INSTANCE) deviceObject->DeviceExtension;
        open->DeviceObject = deviceObject;

        //
        // Create a symbolic link.
        // We need to replace Device from \Device\Packet_{GUID} with DosDevices
        // to create a symbolic link of the form \DosDevices\Packet_{GUID}
        // There is a four character difference between these two 
        // strings.
        //
        
        length = unicodeDeviceName.Length + sizeof(UNICODE_NULL) + 
                    (4 * sizeof(WCHAR));
        
        symbolicLink = ExAllocatePool(NonPagedPool, length);
        if (!symbolicLink) {

            DebugPrint(("Memory allocation for create symbolic failed\n"));
            *Status = NDIS_STATUS_FAILURE;
            break;
        }

        swprintf(symbolicLink, L"\\DosDevices\\%ws", 
                    &unicodeDeviceName.Buffer[8]);
        
        RtlInitUnicodeString(&open->SymbolicLink,symbolicLink);

        DebugPrint(("Symbolic Link: %ws\n", open->SymbolicLink.Buffer));        
        
        status = IoCreateSymbolicLink(
                (PUNICODE_STRING) &open->SymbolicLink,
                (PUNICODE_STRING) &unicodeDeviceName
                );
        if (status != STATUS_SUCCESS) {

            DebugPrint(("Create symbolic failed\n"));
            *Status = NDIS_STATUS_FAILURE;
            break;
        }
       
        ExFreePool(unicodeDeviceName.Buffer);
        unicodeDeviceName.Buffer = NULL;

        //
        //  Allocate a packet pool for our xmit and receive packets
        //

        NdisAllocatePacketPool(
            &status,
            &open->PacketPool,
            TRANSMIT_PACKETS,
            sizeof(PACKET_RESERVED));

        if (status != NDIS_STATUS_SUCCESS) {
            DebugPrint(("Packet: Failed to allocate packet pool\n"));
            break;
        }
        
        //
        //  Initializing the Event used for synchronizing open and close.
        //
        
        NdisInitializeEvent(&open->Event);

        //
        // List to hold irp's want to reset the adapter
        //
        InitializeListHead(&open->ResetIrpList);

        //
        // Initialize the spinlock used for synchronizing access
        // to the reset list.
        //
        KeInitializeSpinLock(&open->ResetQueueLock);
        
        //
        //  Initialize list for holding pending read requests
        //
        KeInitializeSpinLock(&open->RcvQSpinLock);
        InitializeListHead(&open->RcvList);

        //
        // Now open the adapter below and complete the initialization
        //
        
        NdisOpenAdapter(Status,
                          &status,
                          &open->AdapterHandle,
                          &mediumIndex,
                          &mediumArray,
                          sizeof(mediumArray)/sizeof(NDIS_MEDIUM),
                          Globals.NdisProtocolHandle,
                          open,
                          DeviceName,
                          0,
                          NULL);

        if(*Status == NDIS_STATUS_PENDING)
        {
              NdisWaitEvent(&open->Event, 0);
              *Status = open->Status;
        }
        if(*Status != NDIS_STATUS_SUCCESS)
        {
            DebugPrint(("Failed to openAdapter\n"));
            break;
        }
        open->IrpCount = 0;

        InterlockedExchange( (PLONG)&open->Bound, TRUE );
        NdisInitializeEvent(&open->CleanupEvent);

        //
        // Let the initial state of the event to be signalled state.
        //

        NdisSetEvent(&open->CleanupEvent);

        //
        //  Save the the friendly name of the MAC driver
        //

        NdisQueryAdapterInstanceName(&open->AdapterName, 
                                        open->AdapterHandle);
        DebugPrint(("Bound AdapterName %ws\n", open->AdapterName.Buffer));

        open->Medium = mediumArray;
        //
        // Link this instance to the global adapterlist.
        //
        
        InitializeListHead(&open->AdapterListEntry);
        
        ExInterlockedInsertTailList(&Globals.AdapterList,
                                    &open->AdapterListEntry, 
                                    &Globals.GlobalLock);

        //
        // Clear the DO_DEVICE_INITIALIZING flag. This is required
        // if you create deviceobjects outside of DriverEntry.
        // Untill you do this, application cannot send I/O request.
        // 

        deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;


    } while(FALSE);

    if (*Status != NDIS_STATUS_SUCCESS)
    {
        if (open && open->PacketPool != NULL) {
             NdisFreePacketPool(open->PacketPool);
        }
        if (deviceObject != NULL) {
            IoDeleteDevice(deviceObject);
        }
        
        if(unicodeDeviceName.Buffer)
            ExFreePool(unicodeDeviceName.Buffer);

        if(symbolicLink) {
            IoDeleteSymbolicLink(&open->SymbolicLink);
            ExFreePool(open->SymbolicLink.Buffer);
        }
    }

    DebugPrint(("Return BindAdapter :0x%x\n", *Status));
}


VOID
PacketUnbindAdapter(
    OUT PNDIS_STATUS        Status,
    IN  NDIS_HANDLE         ProtocolBindingContext,
    IN  NDIS_HANDLE         UnbindContext
    )
/*++

Routine Description:

    Called by NDIS when we are required to unbind to the adapter below.

Arguments:

    Status                    Placeholder for return status
    ProtocolBindingContext    Pointer to the adapter structure
    UnbindContext            Context for NdisUnbindComplete() if this pends

Return Value:


--*/
{
    POPEN_INSTANCE   open =(POPEN_INSTANCE)ProtocolBindingContext;
    KIRQL            oldIrql;

    DebugPrint(("PacketUnbindAdapter :%ws\n", open->AdapterName.Buffer));

    if(open->AdapterHandle != NULL)
    {
        NdisResetEvent(&open->Event);

        //
        // Your are no longer bound to any adapter
        //

        InterlockedExchange( (PLONG) &open->Bound, FALSE );

        //
        // Cancel all the pending reads.
        // 

        PacketCancelReadIrps(open->DeviceObject);
        
        //
        // Wait for all the outstanding IRPs to complete
        //
        DebugPrint(("Waiting on CleanupEvent\n"));

        NdisWaitEvent(&open->CleanupEvent, 0);

        NdisCloseAdapter(Status, open->AdapterHandle);

        //
        // Wait for it to complete
        //
        if(*Status == NDIS_STATUS_PENDING)
        {
            NdisWaitEvent(&open->Event, 0);
            *Status = open->Status;
        }
        else
        {
            *Status = NDIS_STATUS_FAILURE;
            ASSERT(0);
        }

        //
        // Delink this instance from the global adapter list.
        //
        
        KeAcquireSpinLock(&Globals.GlobalLock, &oldIrql);
        RemoveEntryList(&open->AdapterListEntry);
        KeReleaseSpinLock(&Globals.GlobalLock, oldIrql);
        
        NdisFreePacketPool(open->PacketPool);
        
        NdisFreeMemory(open->AdapterName.Buffer, open->AdapterName.Length, 0);

        IoDeleteSymbolicLink(&open->SymbolicLink);
        ExFreePool(open->SymbolicLink.Buffer);
        
        IoDeleteDevice(open->DeviceObject);
    }

    DebugPrint(("Exit PacketUnbindAdapter\n"));
}

VOID
PacketOpenAdapterComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  Status,
    IN NDIS_STATUS  OpenErrorStatus
    )
/*++

Routine Description:

    Completion routine for NdisOpenAdapter issued from within the 
    PacketBindAdapter. Simply unblock the caller.

Arguments:

    ProtocolBindingContext    Pointer to the adapter
    Status                    Status of the NdisOpenAdapter call
    OpenErrorStatus            Secondary status(ignored by us).

Return Value:

    None

--*/
{

    POPEN_INSTANCE    open = ProtocolBindingContext;

    DebugPrint(("Packet: OpenAdapterComplete\n"));

    open->Status = Status;
    NdisSetEvent(&open->Event);
    return;

}

VOID
PacketCloseAdapterComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  Status
    )
/*++

Routine Description:

    Completion routine for NdisCloseAdapter issued from within the 
    PacketUnBindAdapter. Simply unblock the caller.

Arguments:

    ProtocolBindingContext    Pointer to the adapter
    Status                    Status of the NdisOpenAdapter call

Return Value:

    None

--*/

{
    POPEN_INSTANCE    open = ProtocolBindingContext;

    DebugPrint(("CloseAdapterComplete\n"));

    open->Status = Status;
    NdisSetEvent(&open->Event);
    
    return;

}


NDIS_STATUS
PacketPNPHandler(
    IN    NDIS_HANDLE        ProtocolBindingContext,
    IN    PNET_PNP_EVENT     NetPnPEvent
    )

/*++
Routine Description:

    NDIS calls ProtocolPnPEvent to indicate a Plug and Play 
    event or a Power Management event.  
    All PNP Related OIDS(requests) are routed to this function. 

Arguments:

    ProtocolBindingContext    Pointer to our adapter structure.
    NetPnPEvent               Pointer to a Net_PnP_Event

Return Value:

    NDIS_STATUS_SUCCESS: as we do not do much here

--*/
{
    POPEN_INSTANCE              open  =(POPEN_INSTANCE)ProtocolBindingContext;
    NDIS_STATUS                 Status  = NDIS_STATUS_SUCCESS;
    PNET_DEVICE_POWER_STATE     powerState;

    DebugPrint(("PacketPNPHandler\n"));
    
    powerState = (PNET_DEVICE_POWER_STATE)NetPnPEvent->Buffer;

    //
    // This will happen when all entities in the system need to be notified
    //
    //if(open == NULL)
    //{
    //  return Status;
    //}

    switch(NetPnPEvent->NetEvent)
    {
         case  NetEventSetPower :
            DebugPrint(("NetEventSetPower\n"));
            switch (*powerState) {
            
                case NetDeviceStateD0:
                    Status = NDIS_STATUS_SUCCESS;
                    break;

                default:
                    //
                    // We can't suspend, so we ask NDIS to Unbind us by
                    // returning this status:
                    //
                    Status = NDIS_STATUS_NOT_SUPPORTED;
                    break;
            }
            break;
         case NetEventQueryPower :
            DebugPrint(("NetEventQueryPower\n"));
            break;
            
         case NetEventQueryRemoveDevice  :
            DebugPrint(("NetEventQueryRemoveDevice \n"));
            break;

         case NetEventCancelRemoveDevice  :
            DebugPrint(("NetEventCancelRemoveDevice \n"));
            break;
            
         case NetEventReconfigure :
            //
            // The protocol should always succeed this event 
            // by returning NDIS_STATUS_SUCCESS
            //
            DebugPrint(("NetEventReconfigure\n"));
            break;

        case NetEventBindsComplete  :
            DebugPrint(("NetEventBindsComplete \n"));
            break;

        case NetEventPnPCapabilities  :
            DebugPrint(("NetEventPnPCapabilities \n"));
        case NetEventBindList:
            DebugPrint(("NetEventBindList \n"));
        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    return Status;
}


VOID
IoIncrement (
    IN  OUT POPEN_INSTANCE  Open
    )   

/*++
Routine Description:

    This routine increments the number of requests the device receives
    
Arguments:

    Open - pointer to the device extension.
    
Return Value:
--*/

{
    LONG            result;
    
    result = InterlockedIncrement(&Open->IrpCount);

    //DebugPrint(("IoIncrement %d\n", result));

    //
    // Need to clear event (when IrpCount bumps from 0 to 1) 
    //
    
    if (result == 1) {
        //
        // We need to clear the event
        //
        NdisResetEvent(&Open->CleanupEvent);
    }

    return;
}

VOID
IoDecrement (
    IN  OUT POPEN_INSTANCE  Open
    )
/*++
Routine Description:

    This routine decrements as it complete the request it receives

Arguments:

    Open - pointer to the device extension.
    
Return Value:

--*/
{
    LONG            result;
    
    result = InterlockedDecrement(&Open->IrpCount);

    //DebugPrint(("IoDecrement %d\n", result));

    if (result == 0) {

        //
        // Set the event when the count transition from 1 to 0.
        //
 
        NdisSetEvent (&Open->CleanupEvent);
    }
    return;
}



