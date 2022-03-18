/*++

Copyright (c) 1998  Microsoft Corporation

Module Name:

    usbcamd.c

Abstract:

    USB device driver for camera

Environment:

    kernel mode only

Notes:

  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
  PURPOSE.

  Copyright (c) 1998 Microsoft Corporation.  All Rights Reserved.


Revision History:

    Original 3/96 John Dunn
    Updated  3/98 Husni Roukbi

--*/

#include "usbcamd.h"


extern ULONG USBCAMD_CameraFRC;

#define DEADMAN_TIMEOUT     5000     //timeout in ms

#if DBG
ULONG USBCAMD_StreamEnable = 1;
// Global debug vars
ULONG USBCAMD_DebugTraceLevel =
#ifdef MAX_DEBUG
    MAX_TRACE;
#else
    MIN_TRACE;
#endif
#endif



//---------------------------------------------------------------------------
// USBCAMD_StartDevice
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_StartDevice(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:

    Initializes a given instance of the camera device on the USB.

Arguments:

    deviceExtension - points to the driver specific DeviceExtension

    Irp - Irp associated with this request


Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus;
    PUSB_DEVICE_DESCRIPTOR deviceDescriptor = NULL;
    PURB urb;
    ULONG siz,i;

    USBCAMD_KdPrint (MAX_TRACE, ("enter USBCAMD_StartDevice\n"));

    KeInitializeSemaphore(&DeviceExtension->Semaphore, 1, 1);
    KeInitializeSemaphore(&DeviceExtension->CallUSBSemaphore, 1, 1);

    //
    // Fetch the device descriptor for the device
    //
    urb = USBCAMD_ExAllocatePool(NonPagedPool,
                         sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST));

    if (urb) {

        siz = sizeof(USB_DEVICE_DESCRIPTOR);

        deviceDescriptor = USBCAMD_ExAllocatePool(NonPagedPool,
                                                  siz);

        if (deviceDescriptor) {

            UsbBuildGetDescriptorRequest(urb,
                                         (USHORT) sizeof (struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                                         USB_DEVICE_DESCRIPTOR_TYPE,
                                         0,
                                         0,
                                         deviceDescriptor,
                                         NULL,
                                         siz,
                                         NULL);

            ntStatus = USBCAMD_CallUSBD(DeviceExtension, urb);

            if (NT_SUCCESS(ntStatus)) {
                USBCAMD_KdPrint (MAX_TRACE, ("'Device Descriptor = %x, len %x\n",
                                deviceDescriptor,
                                urb->UrbControlDescriptorRequest.TransferBufferLength));

                USBCAMD_KdPrint (MAX_TRACE, ("'USBCAMD Device Descriptor:\n"));
                USBCAMD_KdPrint (MAX_TRACE, ("'-------------------------\n"));
                USBCAMD_KdPrint (MAX_TRACE, ("'bLength %d\n", deviceDescriptor->bLength));
                USBCAMD_KdPrint (MAX_TRACE, ("'bDescriptorType 0x%x\n", deviceDescriptor->bDescriptorType));
                USBCAMD_KdPrint (MAX_TRACE, ("'bcdUSB 0x%x\n", deviceDescriptor->bcdUSB));
                USBCAMD_KdPrint (MAX_TRACE, ("'bDeviceClass 0x%x\n", deviceDescriptor->bDeviceClass));
                USBCAMD_KdPrint (MAX_TRACE, ("'bDeviceSubClass 0x%x\n", deviceDescriptor->bDeviceSubClass));
                USBCAMD_KdPrint (MAX_TRACE, ("'bDeviceProtocol 0x%x\n", deviceDescriptor->bDeviceProtocol));
                USBCAMD_KdPrint (MAX_TRACE, ("'bMaxPacketSize0 0x%x\n", deviceDescriptor->bMaxPacketSize0));
                USBCAMD_KdPrint (MAX_TRACE, ("'idVendor 0x%x\n", deviceDescriptor->idVendor));
                USBCAMD_KdPrint (MAX_TRACE, ("'idProduct 0x%x\n", deviceDescriptor->idProduct));
                USBCAMD_KdPrint (MAX_TRACE, ("'bcdDevice 0x%x\n", deviceDescriptor->bcdDevice));
                USBCAMD_KdPrint (MIN_TRACE, ("'iManufacturer 0x%x\n", deviceDescriptor->iManufacturer));
                USBCAMD_KdPrint (MAX_TRACE, ("'iProduct 0x%x\n", deviceDescriptor->iProduct));
                USBCAMD_KdPrint (MAX_TRACE, ("'iSerialNumber 0x%x\n", deviceDescriptor->iSerialNumber));
                USBCAMD_KdPrint (MAX_TRACE, ("'bNumConfigurations 0x%x\n", deviceDescriptor->bNumConfigurations));
            }
        } else {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }

        if (NT_SUCCESS(ntStatus)) {
            DeviceExtension->DeviceDescriptor = deviceDescriptor;
        } else if (deviceDescriptor) {
            USBCAMD_ExFreePool(deviceDescriptor);
        }

        USBCAMD_ExFreePool(urb);

    } else {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Now configure the device.
    //

    if (NT_SUCCESS(ntStatus)) {
        ntStatus = USBCAMD_ConfigureDevice(DeviceExtension);
    }

    if (NT_SUCCESS(ntStatus)) {
        //
        // initialize our f ref count and semaphores
        //
        for ( i=0; i< MAX_STREAM_COUNT; i++) {
            DeviceExtension->ActualInstances[i] = 0;
        }


        for (i=0; i < MAX_STREAM_COUNT; i++) {
            DeviceExtension->TimeoutCount[i] = -1;
        }
    }

	if (ntStatus != STATUS_SUCCESS){
	//
    // since this failure will return all the way in the IRP_MN_SATRT_DEVICE.
    // the driver will unload w/o sending IRP_MN_REMOVE_DEVICE where we typically
    // do the clean up of our allocated memory. Hence, we need to do it now.
    //
   	    if (DeviceExtension->DeviceDescriptor) 
        	USBCAMD_ExFreePool(DeviceExtension->DeviceDescriptor);
    	if (DeviceExtension->Interface) 
        	USBCAMD_ExFreePool(DeviceExtension->Interface);
    	if ( DeviceExtension->Usbcamd_version == USBCAMD_VERSION_200) 
    		if (DeviceExtension->PipePinRelations)
        		USBCAMD_ExFreePool(DeviceExtension->PipePinRelations);
        //
        // call client driver in order to do some clean up as well
        //
        if ( DeviceExtension->Usbcamd_version == USBCAMD_VERSION_200) {
                     (*DeviceExtension->DeviceDataEx.DeviceData2.CamConfigureEx)(
                                DeviceExtension->StackDeviceObject,
                                USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension),
                                NULL,
                                NULL,
                                0,
                                NULL,
                                NULL);

        }
        else {
        		(*DeviceExtension->DeviceDataEx.DeviceData.CamConfigure)(
                	 DeviceExtension->StackDeviceObject,
                     USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension),
                     NULL,
                     NULL,
                     NULL,
                     NULL);
        }
	}

    USBCAMD_KdPrint (MIN_TRACE, ("exit USBCAMD_StartDevice (%x)\n", ntStatus));
    return ntStatus;
}


//---------------------------------------------------------------------------
// USBCAMD_RemoveDevice
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_RemoveDevice(
    IN PUSBCAMD_DEVICE_EXTENSION  DeviceExtension
    )
/*++

Routine Description:

    Removes a given instance of the USB camera.

    NOTE: When we get a remove we can asume the device is gone.

Arguments:

    deviceExtension - points to the driver specific DeviceExtension

    Irp - Irp associated with this request

Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    USBCAMD_KdPrint (MAX_TRACE, ("enter USBCAMD_RemoveDevice\n"));

    ASSERT((DeviceExtension->ActualInstances[STREAM_Capture] == 0) &&
        (DeviceExtension->ActualInstances[STREAM_Still] == 0));

    ntStatus = (*DeviceExtension->DeviceDataEx.DeviceData.CamUnInitialize)(
                     DeviceExtension->StackDeviceObject,
                     USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension));

    if ( DeviceExtension->Usbcamd_version == USBCAMD_VERSION_200) {
 		//
		// make sure that camera driver has cancelled a bulk or Interrupt
		// transfer request.
    	//

   		USBCAMD_CancelOutstandingBulkIntIrps(DeviceExtension,FALSE);

    	//
    	// and any pipeconif structures.
    	//

        USBCAMD_ExFreePool(DeviceExtension->PipePinRelations);
    }
    
    if (DeviceExtension->DeviceDescriptor) {
        USBCAMD_ExFreePool(DeviceExtension->DeviceDescriptor);
    }

    //
    // Free up any interface structures
    //

    if (DeviceExtension->Interface) {
        USBCAMD_ExFreePool(DeviceExtension->Interface);
    }

    USBCAMD_CameraFRC--;
    USBCAMD_KdPrint (MIN_TRACE, ("exit USBCAMD_RemoveDevice (%x)\n", ntStatus));

    return ntStatus;
}

//******************************************************************************
//
// USBCAMD_CallUsbdCompletion()
//
// Completion routine used by USBCAMD_CallUsbd() 
//
//******************************************************************************

NTSTATUS
USBCAMD_CallUsbdCompletion (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PVOID            Context
    )
{
    PKEVENT kevent = (PKEVENT)Context;
    KeSetEvent(kevent, IO_NO_INCREMENT,FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

//---------------------------------------------------------------------------
// USBCAMD_CallUSBD
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_CallUSBD(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PURB Urb
    )
/*++

Routine Description:

    Passes a URB to the USBD class driver

Arguments:

    DeviceExtension - pointer to the device extension for this instance of an USB camera

    Urb - pointer to Urb request block

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise

--*/
{
    NTSTATUS ntStatus, status = STATUS_SUCCESS;
    PIRP irp;
    KEVENT TimeoutEvent;
    PIO_STACK_LOCATION nextStack;

    KeWaitForSingleObject(&DeviceExtension->CallUSBSemaphore,Executive,KernelMode,FALSE,NULL);

    // Initialize the event we'll wait on
    //
    KeInitializeEvent(&TimeoutEvent,SynchronizationEvent,FALSE);

    // Allocate the Irp
    //
    irp = IoAllocateIrp(DeviceExtension->StackDeviceObject->StackSize, FALSE);

    if (irp == NULL){
        ntStatus =  STATUS_UNSUCCESSFUL;
        goto Exit_CallUSB;
    }
    //
    // Set the Irp parameters
    //
    nextStack = IoGetNextIrpStackLocation(irp);
    ASSERT(nextStack != NULL);
    nextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    nextStack->Parameters.DeviceIoControl.IoControlCode =  IOCTL_INTERNAL_USB_SUBMIT_URB;
    nextStack->Parameters.Others.Argument1 = Urb;
    //
    // Set the completion routine.
    //
    IoSetCompletionRoutine(irp,USBCAMD_CallUsbdCompletion,&TimeoutEvent, TRUE, TRUE,TRUE);   
    //
    // pass the irp down usb stack
    //
    if (DeviceExtension->Initialized ) {
        ntStatus = IoCallDriver(DeviceExtension->StackDeviceObject,irp);
    } else {
        ntStatus = STATUS_DEVICE_DATA_ERROR;
    }

    USBCAMD_KdPrint (MAX_TRACE, ("return from IoCallDriver USBD %x\n", ntStatus));

    if (ntStatus == STATUS_PENDING) {
        // Irp i spending. we have to wait till completion..
        LARGE_INTEGER timeout;

        // Specify a timeout of 5 seconds to wait for this call to complete.
        //
        timeout.QuadPart = -10000 * 5000;

        ntStatus = KeWaitForSingleObject(&TimeoutEvent, Executive,KernelMode,FALSE, &timeout);
        if (ntStatus == STATUS_TIMEOUT) {
             ntStatus = STATUS_IO_TIMEOUT;

            // Cancel the Irp we just sent.
            //
            IoCancelIrp(irp);

            // And wait until the cancel completes
            //
            KeWaitForSingleObject(&TimeoutEvent,Executive, KernelMode, FALSE,NULL);
        }
        else {
            ntStatus = irp->IoStatus.Status;
        }
    }

    // Done with the Irp, now free it.
    //
    IoFreeIrp(irp);

Exit_CallUSB:

    KeReleaseSemaphore(&DeviceExtension->CallUSBSemaphore,LOW_REALTIME_PRIORITY,1,FALSE);

    if (NT_ERROR(ntStatus)) {
        USBCAMD_KdPrint(MIN_TRACE, ("***Error*** USBCAMD_CallUSBD (%x)\n", ntStatus));
    }

    return ntStatus;
}


//---------------------------------------------------------------------------
// USBCAMD_ConfigureDevice
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_ConfigureDevice(
    IN  PUSBCAMD_DEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:

    Configure the USB camera.

Arguments:

    DeviceExtension - pointer to the device object for this instance of the USB camera
                    devcice.


Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PURB urb;
    ULONG siz;
    PUSB_CONFIGURATION_DESCRIPTOR configurationDescriptor = NULL;

    USBCAMD_KdPrint (MAX_TRACE, ("enter USBCAMD_ConfigureDevice\n"));

    //
    // configure the device
    //

    urb = USBCAMD_ExAllocatePool(NonPagedPool,
                         sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST));

    if (urb) {

        siz = 0x40;

get_config_descriptor_retry:

        configurationDescriptor = USBCAMD_ExAllocatePool(NonPagedPool,
                                                 siz);

        if (configurationDescriptor) {

            UsbBuildGetDescriptorRequest(urb,
                                         (USHORT) sizeof (struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                                         USB_CONFIGURATION_DESCRIPTOR_TYPE,
                                         0,
                                         0,
                                         configurationDescriptor,
                                         NULL,
                                         siz,
                                         NULL);

            ntStatus = USBCAMD_CallUSBD(DeviceExtension, urb);

            USBCAMD_KdPrint (MAX_TRACE, ("'Configuration Descriptor = %x, len %x\n",
                            configurationDescriptor,
                            urb->UrbControlDescriptorRequest.TransferBufferLength));
        } else {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // if we got some data see if it was enough.
        //
        // NOTE: we may get an error in URB because of buffer overrun
        //
        if (urb->UrbControlDescriptorRequest.TransferBufferLength>0 &&
                configurationDescriptor->wTotalLength > siz) {

            siz = configurationDescriptor->wTotalLength;
            USBCAMD_ExFreePool(configurationDescriptor);
            configurationDescriptor = NULL;
            goto get_config_descriptor_retry;
        }

        USBCAMD_ExFreePool(urb);

    } else {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    if (configurationDescriptor) {

        //
        // Get our pipes
        //
        if (NT_SUCCESS(ntStatus)) {
            ntStatus = USBCAMD_SelectConfiguration(DeviceExtension, configurationDescriptor);

            if (NT_SUCCESS(ntStatus)) {
                ntStatus = (*DeviceExtension->DeviceDataEx.DeviceData.CamInitialize)(
                      DeviceExtension->StackDeviceObject,
                      USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension));
            }

            USBCAMD_ExFreePool(configurationDescriptor);
        }

    }

    USBCAMD_KdPrint (MIN_TRACE, ("'exit USBCAMD_ConfigureDevice (%x)\n", ntStatus));

//    TRAP_ERROR(ntStatus);

    return ntStatus;
}


//---------------------------------------------------------------------------
// USBCAMD_SelectConfiguration
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_SelectConfiguration(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSB_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor
    )
/*++

Routine Description:

    Initializes the USBCAMD camera to configuration one, interface zero

Arguments:

    DeviceExtension - pointer to the device extension for this instance of the USB camera
                    devcice.

    ConfigurationDescriptor - pointer to the USB configuration
                    descriptor containing the interface and endpoint
                    descriptors.

Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus;
    PURB urb = NULL;
    ULONG numberOfInterfaces, numberOfPipes,i;
    PUSB_INTERFACE_DESCRIPTOR interfaceDescriptor;
    PUSBD_INTERFACE_INFORMATION interface;
    PUSBD_INTERFACE_LIST_ENTRY interfaceList, tmp;
    PUSBCAMD_Pipe_Config_Descriptor PipeConfig = NULL;

    USBCAMD_KdPrint (MAX_TRACE, ("'enter USBCAMD_SelectConfiguration\n"));

    //
    // get this from the config descriptor
    //
    numberOfInterfaces = ConfigurationDescriptor->bNumInterfaces;

    // We only support cameras with one interface
  //  ASSERT(numberOfInterfaces == 1);


    tmp = interfaceList =
        USBCAMD_ExAllocatePool(PagedPool, sizeof(USBD_INTERFACE_LIST_ENTRY) *
                       (numberOfInterfaces+1));


    if (tmp) {
        
        for ( i = 0; i < numberOfInterfaces; i++ ) {

            interfaceDescriptor =
                USBD_ParseConfigurationDescriptorEx(
                    ConfigurationDescriptor,
                    ConfigurationDescriptor,
                    i,    // interface number
                    -1, //alt setting, don't care
                    -1, // hub class
                    -1, // subclass, don't care
                    -1); // protocol, don't care

            interfaceList->InterfaceDescriptor =
                interfaceDescriptor;
            interfaceList++;

        }
        interfaceList->InterfaceDescriptor = NULL;

        //
        // Allocate a URB big enough for this request
        //

        urb = USBD_CreateConfigurationRequestEx(ConfigurationDescriptor, tmp);

        if (urb) {

            if ( DeviceExtension->Usbcamd_version == USBCAMD_VERSION_200) {
                numberOfPipes = tmp->Interface->NumberOfPipes;
                PipeConfig = USBCAMD_ExAllocatePool(PagedPool,
                                    sizeof(USBCAMD_Pipe_Config_Descriptor) * numberOfPipes);
                if (PipeConfig ) {

                    ntStatus =
                        (*DeviceExtension->DeviceDataEx.DeviceData2.CamConfigureEx)(
                                DeviceExtension->StackDeviceObject,
                                USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension),
                                tmp->Interface,
                                ConfigurationDescriptor,
                                numberOfPipes,
                                PipeConfig,
                                DeviceExtension->DeviceDescriptor);

                }
                else {
                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                }
            }
            else {
                ntStatus =
                    (*DeviceExtension->DeviceDataEx.DeviceData.CamConfigure)(
                            DeviceExtension->StackDeviceObject,
                            USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension),
                            tmp->Interface,
                            ConfigurationDescriptor,
                            &DeviceExtension->DataPipe,
                            &DeviceExtension->SyncPipe);
                //
                // initialize the new parameters to default values in order to
                // insure backward compatibilty.
                //

                DeviceExtension->IsoPipeStreamType = STREAM_Capture;
                DeviceExtension->BulkPipeStreamType = -1;
                DeviceExtension->BulkDataPipe = -1;
                DeviceExtension->VirtualStillPin = FALSE;
            }
            USBCAMD_ExFreePool(tmp);
        } else {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }

    }
    else {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    if (NT_SUCCESS(ntStatus)) {

        interface = &urb->UrbSelectConfiguration.Interface;

        USBCAMD_KdPrint (MAX_TRACE, ("'size of interface request = %d\n", interface->Length));

        ntStatus = USBCAMD_CallUSBD(DeviceExtension, urb);

        if (NT_SUCCESS(ntStatus) && USBD_SUCCESS(URB_STATUS(urb))) {

            if ( DeviceExtension->Usbcamd_version == USBCAMD_VERSION_200) {

                DeviceExtension->PipePinRelations = USBCAMD_ExAllocatePool(NonPagedPool,
                        sizeof(USBCAMD_PIPE_PIN_RELATIONS) * numberOfPipes);
                if ( DeviceExtension->PipePinRelations) {
                    for (i=0; i < numberOfPipes; i++) {
                        DeviceExtension->PipePinRelations[i].PipeType =
                            interface->Pipes[i].PipeType & USB_ENDPOINT_TYPE_MASK;
                        DeviceExtension->PipePinRelations[i].PipeDirection =
                            (interface->Pipes[i].EndpointAddress & USB_ENDPOINT_DIRECTION_MASK) ? INPUT_PIPE : OUTPUT_PIPE;
                        DeviceExtension->PipePinRelations[i].MaxPacketSize =
                            interface->Pipes[i].MaximumPacketSize;
                        DeviceExtension->PipePinRelations[i].PipeConfig = PipeConfig[i];
                        InitializeListHead(&DeviceExtension->PipePinRelations[i].IrpPendingQueue);
	                    InitializeListHead(&DeviceExtension->PipePinRelations[i].IrpRestoreQueue);
	                    KeInitializeSpinLock (&DeviceExtension->PipePinRelations[i].OutstandingIrpSpinlock);
                    }
                    ntStatus = USBCAMD_Parse_PipeConfig(DeviceExtension,numberOfPipes);
                }
                else {
                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                }
            }

            //
            // Save the configuration handle for this device
            //

            DeviceExtension->ConfigurationHandle =
                urb->UrbSelectConfiguration.ConfigurationHandle;


            DeviceExtension->Interface = USBCAMD_ExAllocatePool(NonPagedPool,
                                                        interface->Length);

            if (DeviceExtension->Interface) {
                ULONG j;

                //
                // save a copy of the interface information returned
                //
                RtlCopyMemory(DeviceExtension->Interface, interface, interface->Length);

                //
                // Dump the interface to the debugger
                //
                USBCAMD_KdPrint (MAX_TRACE, ("'---------\n"));
                USBCAMD_KdPrint (MAX_TRACE, ("'NumberOfPipes 0x%x\n", DeviceExtension->Interface->NumberOfPipes));
                USBCAMD_KdPrint (MAX_TRACE, ("'Length 0x%x\n", DeviceExtension->Interface->Length));
                USBCAMD_KdPrint (MAX_TRACE, ("'Alt Setting 0x%x\n", DeviceExtension->Interface->AlternateSetting));
                USBCAMD_KdPrint (MAX_TRACE, ("'Interface Number 0x%x\n", DeviceExtension->Interface->InterfaceNumber));

                // Dump the pipe info

                for (j=0; j<interface->NumberOfPipes; j++) {
                    PUSBD_PIPE_INFORMATION pipeInformation;

                    pipeInformation = &DeviceExtension->Interface->Pipes[j];

                    USBCAMD_KdPrint (MAX_TRACE, ("'---------\n"));
                    USBCAMD_KdPrint (MAX_TRACE, ("'PipeType 0x%x\n", pipeInformation->PipeType));
                    USBCAMD_KdPrint (MAX_TRACE, ("'EndpointAddress 0x%x\n", pipeInformation->EndpointAddress));
                    USBCAMD_KdPrint (MAX_TRACE, ("'MaxPacketSize 0x%x\n", pipeInformation->MaximumPacketSize));
                    USBCAMD_KdPrint (MAX_TRACE, ("'Interval 0x%x\n", pipeInformation->Interval));
                    USBCAMD_KdPrint (MAX_TRACE, ("'Handle 0x%x\n", pipeInformation->PipeHandle));
                }

                USBCAMD_KdPrint (MAX_TRACE, ("'---------\n"));

            }
        }

        if (urb)
        	ExFreePool(urb);

    } else {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    USBCAMD_KdPrint (MIN_TRACE, ("'exit USBCAMD_SelectConfiguration (%x)\n", ntStatus));

    if ( DeviceExtension->Usbcamd_version == USBCAMD_VERSION_200) {
    	if (PipeConfig) 
        	USBCAMD_ExFreePool(PipeConfig);
    }

    return ntStatus;
}

/*++

Routine Description:


Arguments:

    DeviceExtension - pointer to the device extension for this instance of the USB camera
                    devcice.


Return Value:

    NT status code

--*/

NTSTATUS
USBCAMD_Parse_PipeConfig(
     IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
     IN ULONG NumberOfPipes
     )
{
    int i;
    ULONG PinCount;
    NTSTATUS ntStatus= STATUS_SUCCESS;

    PUSBCAMD_PIPE_PIN_RELATIONS PipePinArray;

    PipePinArray = DeviceExtension->PipePinRelations;

    DeviceExtension->VirtualStillPin = FALSE;
    DeviceExtension->DataPipe = -1;
    DeviceExtension->SyncPipe = -1;
    DeviceExtension->BulkDataPipe = -1;
    DeviceExtension->IsoPipeStreamType = -1;
    DeviceExtension->BulkPipeStreamType = -1;
    PinCount = 0;

    ASSERT (PipePinArray);

    for ( i=0; i < (int)NumberOfPipes; i++) {

        if (PipePinArray[i].PipeConfig.PipeConfigFlags & USBCAMD_DONT_CARE_PIPE) {
            continue; // this pipe has no use for us.
        }
        switch ( PipePinArray[i].PipeConfig.PipeConfigFlags) {

        case USBCAMD_MULTIPLEX_PIPE:

            if ((PipePinArray[i].PipeConfig.StreamAssociation & USBCAMD_VIDEO_STILL_STREAM) &&
                (PipePinArray[i].PipeDirection & INPUT_PIPE  ) ) {
                    // we found an input data pipe (iso or bulk) that is used for both
                    // video & still.
                    if ( PipePinArray[i].PipeType & UsbdPipeTypeIsochronous) {
                        // we found an input iso pipe that is used for video data.
                        DeviceExtension->DataPipe = i;
                        DeviceExtension->IsoPipeStreamType = STREAM_Capture;
                    }
                    else if (PipePinArray[i].PipeType & UsbdPipeTypeBulk) {
                        // we found an input bulk pipe that is used for video data.
                        DeviceExtension->BulkDataPipe = i;
                        DeviceExtension->BulkPipeStreamType = STREAM_Capture;
                    }
                    DeviceExtension->VirtualStillPin = TRUE;
                    PinCount += 2;
            }
            break;

        case USBCAMD_SYNC_PIPE:

            if ((PipePinArray[i].PipeType & UsbdPipeTypeIsochronous) &&
                (PipePinArray[i].PipeDirection & INPUT_PIPE  ) ) {
                    // we found an input iso pipe that is used for out of band signalling.
                    DeviceExtension->SyncPipe = i;
            }
            break;

        case USBCAMD_DATA_PIPE:

            if ((PipePinArray[i].PipeConfig.StreamAssociation != USBCAMD_VIDEO_STILL_STREAM )&&
                (PipePinArray[i].PipeDirection & INPUT_PIPE  ) ) {
                // we found an input iso or bulk pipe that is used exclusively per video or still
                // stream.
                if ( PipePinArray[i].PipeType & UsbdPipeTypeIsochronous) {
                    // we found an input iso pipe that is used for video or still.
                    DeviceExtension->DataPipe = i;
                    DeviceExtension->IsoPipeStreamType =
                        (PipePinArray[i].PipeConfig.StreamAssociation & USBCAMD_VIDEO_STREAM ) ?
                            STREAM_Capture: STREAM_Still;
                }
                else if (PipePinArray[i].PipeType & UsbdPipeTypeBulk) {
                    // we found an input bulk pipe that is used for video or still data.
                    DeviceExtension->BulkDataPipe = i;
                    DeviceExtension->BulkPipeStreamType =
                        PipePinArray[i].PipeConfig.StreamAssociation & USBCAMD_VIDEO_STREAM  ?
                            STREAM_Capture: STREAM_Still;
                }
                PinCount++;
            }
            break;

        default:
            break;
        }
    }

    // override the default pin count of one with the actual pin count.
    if ( PinCount != 0 ) {
        DeviceExtension->StreamCount = PinCount;
    }

    //
    // Dump the result to the debugger
    //
    USBCAMD_KdPrint (MIN_TRACE, ("NumberOfPins %d\n", PinCount));
    USBCAMD_KdPrint (MIN_TRACE, ("IsoPipeIndex %d\n", DeviceExtension->DataPipe));
    USBCAMD_KdPrint (MIN_TRACE, ("IsoPipeStreamtype %d\n", DeviceExtension->IsoPipeStreamType));
    USBCAMD_KdPrint (MIN_TRACE, ("Sync Pipe Index %d\n", DeviceExtension->SyncPipe));
    USBCAMD_KdPrint (MIN_TRACE, ("Bulk Pipe Index %d\n", DeviceExtension->BulkDataPipe));
    USBCAMD_KdPrint (MIN_TRACE, ("BulkPipeStreamType %d\n", DeviceExtension->BulkPipeStreamType));

    // do some error checing in here.
    // if both data pipe and bulk data pipes are not set, then return error.
    if (((DeviceExtension->DataPipe == -1) && (DeviceExtension->BulkDataPipe == -1)) ||
         (PinCount > MAX_STREAM_COUNT)){
        // cam driver provided mismatched data.
        ntStatus = STATUS_INVALID_PARAMETER;
    }
    return ntStatus;
}

//---------------------------------------------------------------------------
// USBCAMD_SelectAlternateInterface
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_SelectAlternateInterface(
    IN PVOID DeviceContext,
    IN PUSBD_INTERFACE_INFORMATION RequestInterface
    )
/*++

Routine Description:

    Select one of the cameras alternate interfaces

Arguments:

    DeviceExtension - pointer to the device extension for this instance of the USB camera
                    devcice.

    ChannelExtension - extension specific to this video channel

Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus;
    PURB urb;
    ULONG siz;
    PUSBD_INTERFACE_INFORMATION interface;
    PUSBCAMD_DEVICE_EXTENSION deviceExtension;

    USBCAMD_KdPrint (MAX_TRACE, ("'enter USBCAMD_SelectAlternateInterface\n"));

    deviceExtension = USBCAMD_GET_DEVICE_EXTENSION(DeviceContext);

    if (deviceExtension->Usbcamd_version == USBCAMD_VERSION_200) {

        //
        // before we process this request, we need to cancel all outstanding
        // IRPs for this interface on all pipes (bulk, interupt)
		//
        ntStatus = USBCAMD_CancelOutstandingBulkIntIrps(deviceExtension,TRUE);


        if (!NT_SUCCESS(ntStatus)) {
            USBCAMD_KdPrint (MIN_TRACE, ("Failed to Cancel outstanding (Bulk/Int.)IRPs.\n"));
            ntStatus = STATUS_DEVICE_DATA_ERROR;
            return ntStatus;
        }
    }

    //
    // Dump the current interface
    //

    ASSERT(deviceExtension->Interface != NULL);


    siz = GET_SELECT_INTERFACE_REQUEST_SIZE(deviceExtension->Interface->NumberOfPipes);

    USBCAMD_KdPrint (MAX_TRACE, ("size of interface request Urb = %d\n", siz));

    urb = USBCAMD_ExAllocatePool(NonPagedPool, siz);

    if (urb) {

        interface = &urb->UrbSelectInterface.Interface;

        RtlCopyMemory(interface,
                      RequestInterface,
                      RequestInterface->Length);

        // set up the request for the first and only interface

        USBCAMD_KdPrint (MAX_TRACE, ("'size of interface request = %d\n", interface->Length));

        urb->UrbHeader.Function = URB_FUNCTION_SELECT_INTERFACE;
        urb->UrbSelectInterface.ConfigurationHandle =
            deviceExtension->ConfigurationHandle;

        ntStatus = USBCAMD_CallUSBD(deviceExtension, urb);


        if (NT_SUCCESS(ntStatus) && USBD_SUCCESS(URB_STATUS(urb))) {

            ULONG j;

            //
            // save a copy of the interface information returned
            //
            RtlCopyMemory(deviceExtension->Interface, interface, interface->Length);
            RtlCopyMemory(RequestInterface, interface, interface->Length);

            //
            // Dump the interface to the debugger
            //
            USBCAMD_KdPrint (MAX_TRACE, ("'---------\n"));
            USBCAMD_KdPrint (MAX_TRACE, ("'NumberOfPipes 0x%x\n", deviceExtension->Interface->NumberOfPipes));
            USBCAMD_KdPrint (MAX_TRACE, ("'Length 0x%x\n", deviceExtension->Interface->Length));
            USBCAMD_KdPrint (MIN_TRACE, ("'Alt Setting 0x%x\n", deviceExtension->Interface->AlternateSetting));
            USBCAMD_KdPrint (MAX_TRACE, ("'Interface Number 0x%x\n", deviceExtension->Interface->InterfaceNumber));

            // Dump the pipe info

            for (j=0; j<interface->NumberOfPipes; j++) {
                PUSBD_PIPE_INFORMATION pipeInformation;

                pipeInformation = &deviceExtension->Interface->Pipes[j];

                USBCAMD_KdPrint (MAX_TRACE, ("'---------\n"));
                USBCAMD_KdPrint (MAX_TRACE, ("'PipeType 0x%x\n", pipeInformation->PipeType));
                USBCAMD_KdPrint (MAX_TRACE, ("'EndpointAddress 0x%x\n", pipeInformation->EndpointAddress));
                USBCAMD_KdPrint (MAX_TRACE, ("'MaxPacketSize 0x%x\n", pipeInformation->MaximumPacketSize));
                USBCAMD_KdPrint (MAX_TRACE, ("'Interval 0x%x\n", pipeInformation->Interval));
                USBCAMD_KdPrint (MAX_TRACE, ("'Handle 0x%x\n", pipeInformation->PipeHandle));
            }

            //
            // success update our internal state to
            // indicate the new frame rate
            //

            USBCAMD_KdPrint (MAX_TRACE, ("'Selecting Camera Interface\n"));
        }

        USBCAMD_ExFreePool(urb);

    } else {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    if (deviceExtension->Usbcamd_version == USBCAMD_VERSION_200) {

        // restore the cancelled Interrupt or bulk IRPs if any.
        USBCAMD_RestoreOutstandingBulkIntIrps(deviceExtension);
    }

    USBCAMD_KdPrint (MIN_TRACE, ("'exit USBCAMD_SelectAlternateInterface (%x)\n", ntStatus));

//    TRAP_ERROR(ntStatus);

    return ntStatus;
}


//---------------------------------------------------------------------------
// USBCAMD_OpenChannel
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_OpenChannel(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension,
    IN PVOID Format
    )
/*++

Routine Description:

    Opens a video or still stream on the device.

Arguments:

    DeviceExtension - points to the driver specific DeviceExtension
    ChannelExtension - context data for this channel.
    Format - pointer to format information associated with this
            channel.

Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG i,StreamNumber;

    USBCAMD_SERIALIZE(DeviceExtension);

    USBCAMD_KdPrint( MAX_TRACE, ("'enter USBCAMD_OpenChannel %x\n", Format));

    //
    // Initialize structures for this channel
    //
    ChannelExtension->Sig = USBCAMD_CHANNEL_SIG;
    ChannelExtension->DeviceExtension = DeviceExtension;
    ChannelExtension->CurrentFormat = Format;
    ChannelExtension->RawFrameLength = 0;
    ChannelExtension->CurrentFrameIsStill = FALSE;
    ChannelExtension->IdleIsoStream = FALSE;


#if DBG
    // verify our serialization is working
    ChannelExtension->InCam = 0;
    ChannelExtension->InCam++;
    ASSERT(ChannelExtension->InCam == 1);
#endif

    StreamNumber = ChannelExtension->StreamNumber;

    if (DeviceExtension->ActualInstances[StreamNumber] > 0) {
        // channel already open
        ntStatus = STATUS_DEVICE_DATA_ERROR;
        goto USBCAMD_OpenChannel_Done;
    }


    //
    // empty read list
    //
    InitializeListHead(&ChannelExtension->PendingIoList);

    //
    // no current Irp
    //
    ChannelExtension->CurrentRequest = NULL;

    //
    // streaming is off
    //
    ChannelExtension->ImageCaptureStarted = FALSE;

    //
    // Channel not prepared
    //
    ChannelExtension->ChannelPrepared = FALSE;

    //
    // No error condition
    //
    ChannelExtension->StreamError = FALSE;

    //
    // no stop, reset requests are pending
    //
    ChannelExtension->Flags = 0;

    //
    // initialize the io list spin lock
    //

    KeInitializeSpinLock(&ChannelExtension->PendingIoListSpin);

    //
    // and current request spin lock.
    //
    KeInitializeSpinLock(&ChannelExtension->CurrentRequestSpinLock);


    //
    // initialize streaming structures
    //

    for (i=0; i< USBCAMD_MAX_REQUEST; i++) {
        ChannelExtension->TransferExtension[i].ChannelExtension = NULL;
        ChannelExtension->TransferExtension[i].DataIrp = NULL;
        ChannelExtension->TransferExtension[i].DataUrb = NULL;
        ChannelExtension->TransferExtension[i].SyncIrp = NULL;
        ChannelExtension->TransferExtension[i].SyncUrb = NULL;
        ChannelExtension->TransferExtension[i].WorkBuffer = NULL;
    }


USBCAMD_OpenChannel_Done:

    USBCAMD_KdPrint( MIN_TRACE, ("'exit USBCAMD_OpenChannel (%x)\n", ntStatus));


#if DBG
    ChannelExtension->InCam--;
    ASSERT(ChannelExtension->InCam == 0);
#endif

    USBCAMD_RELEASE(DeviceExtension);

    return ntStatus;
}


//---------------------------------------------------------------------------
// USBCAMD_CloseChannel
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_CloseChannel(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension
    )
/*++

Routine Description:

    Closes a video channel.

Arguments:

    DeviceExtension - points to the driver specific DeviceExtension

    ChannelExtension - context data for this channel.

Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PUSBCAMD_READ_EXTENSION readExtension;
    int StreamNumber;
    USBCAMD_SERIALIZE(DeviceExtension);

    USBCAMD_KdPrint( MAX_TRACE, ("'enter USBCAMD_CloseChannel\n"));

#if DBG
    ChannelExtension->InCam++;
    ASSERT(ChannelExtension->InCam == 1);
#endif


    StreamNumber = ChannelExtension->StreamNumber;
    DeviceExtension->ActualInstances[StreamNumber]--;

    //
    // since we only support one channel this
    // should be zero
    //
    ASSERT(DeviceExtension->ActualInstances[StreamNumber] == 0);

    //
    // NOTE:
    // image capture should be stopped/unprepared when we get here
    //

    ASSERT_CHANNEL(ChannelExtension);
    ASSERT(ChannelExtension->ImageCaptureStarted == FALSE);
    ASSERT(ChannelExtension->CurrentRequest == NULL);
    ASSERT(ChannelExtension->ChannelPrepared == FALSE);


    //
    // We are going to complete any reads left here
    //

    do {
        PLIST_ENTRY listEntry;

        listEntry =
              ExInterlockedRemoveHeadList(&(ChannelExtension->PendingIoList),
                                          &ChannelExtension->PendingIoListSpin);

        if (listEntry != NULL) {
            readExtension = (PUSBCAMD_READ_EXTENSION) CONTAINING_RECORD(listEntry,
                                             USBCAMD_READ_EXTENSION,
                                             ListEntry);

            USBCAMD_CompleteRead(ChannelExtension,readExtension,STATUS_CANCELLED,0);

        } else {
            break;
        }

    } while (1);

#if DBG
    ChannelExtension->InCam--;
    ASSERT(ChannelExtension->InCam == 0);
#endif

    USBCAMD_RELEASE(DeviceExtension);

    //
    // allow any pending reset events to run now
    //
    while (DeviceExtension->TimeoutCount[StreamNumber] >= 0) {

        LARGE_INTEGER dueTime;

        dueTime.QuadPart = -10000 * 2;

        KeDelayExecutionThread(KernelMode,
                                      FALSE,
                                      &dueTime);
    }

    USBCAMD_KdPrint( MIN_TRACE, ("'exit USBCAMD_CloseChannel (%x)\n", ntStatus));

    return ntStatus;
}


//---------------------------------------------------------------------------
// USBCAMD_PrepareChannel
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_PrepareChannel(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension
    )
/*++

Routine Description:

    Prepare the Video channel for streaming, this is where the necessary
        USB BW is allocated.

Arguments:

    DeviceExtension - points to the driver specific DeviceExtension

    Irp - Irp associated with this request.

    ChannelExtension - context data for this channel.

Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    LONG StreamNumber;
    ULONG i;
	HANDLE hThread;
	BOOLEAN inQueue;
    LARGE_INTEGER dueTime;

    USBCAMD_SERIALIZE(DeviceExtension);

    USBCAMD_KdPrint (MAX_TRACE, ("'enter USBCAMD_PrepareChannel\n"));

    StreamNumber = ChannelExtension->StreamNumber;

    ASSERT_CHANNEL(ChannelExtension);

    if (ChannelExtension->ChannelPrepared ||
        ChannelExtension->ImageCaptureStarted) {
        // fail the call if the channel is not in the
        // proper state.
        TRAP();
        ntStatus = STATUS_UNSUCCESSFUL;
        goto USBCAMD_PrepareChannel_Done;
    }

    //
    // This driver function will select the appropriate alternate
    // interface.
    // This code performs the select_alt interface and gets us the
    // pipehandles
    //
    if (DeviceExtension->Usbcamd_version == USBCAMD_VERSION_200) {

        ntStatus =
            (*DeviceExtension->DeviceDataEx.DeviceData2.CamAllocateBandwidthEx)(
                    DeviceExtension->StackDeviceObject,
                    USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension),
                    &ChannelExtension->RawFrameLength,
                    ChannelExtension->CurrentFormat,
                    StreamNumber);

        if (NT_SUCCESS(ntStatus)) {
            ntStatus =
                (*DeviceExtension->DeviceDataEx.DeviceData2.CamStartCaptureEx)(
                        DeviceExtension->StackDeviceObject,
                        USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension),
                        StreamNumber);
        }

    }
    else {

        ntStatus =
            (*DeviceExtension->DeviceDataEx.DeviceData.CamAllocateBandwidth)(
                    DeviceExtension->StackDeviceObject,
                    USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension),
                    &ChannelExtension->RawFrameLength,
                    ChannelExtension->CurrentFormat);

        if (NT_SUCCESS(ntStatus)) {
            ntStatus =
                (*DeviceExtension->DeviceDataEx.DeviceData.CamStartCapture)(
                        DeviceExtension->StackDeviceObject,
                        USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension));
        }
    }

	if ( ChannelExtension->RawFrameLength == 0 ) {
		ntStatus = STATUS_DEVICE_DATA_ERROR;  // client driver provided false info.
		goto USBCAMD_PrepareChannel_Done;	// pin open will fail.
	}
	
    if (NT_SUCCESS(ntStatus)) {

        //
        // we have the BW, go ahead and initailize our iso or bulk structures
        //

        // associate the right pipe index with this channel datapipe index.
        // we will never get here for a virtual still pin open.

        if (StreamNumber == DeviceExtension->IsoPipeStreamType ) {
            ChannelExtension->DataPipe = DeviceExtension->DataPipe;
            ChannelExtension->DataPipeType = UsbdPipeTypeIsochronous;

            ntStatus = USBCAMD_StartIsoThread(DeviceExtension); // start iso thread.
            if (!NT_SUCCESS(ntStatus))
            	goto USBCAMD_PrepareChannel_Done;
            else 
            	USBCAMD_KdPrint (MIN_TRACE,("Iso Thread Started\n"));
        }
        else if (StreamNumber == DeviceExtension->BulkPipeStreamType ) {
            ChannelExtension->DataPipe = DeviceExtension->BulkDataPipe;
            ChannelExtension->DataPipeType = UsbdPipeTypeBulk;
            //
            // allocate bulk buffers for each transfer extension.
            //
            for ( i =0; i < USBCAMD_MAX_REQUEST; i++) {
            	ChannelExtension->TransferExtension[i].DataBuffer =
            		USBCAMD_AllocateRawFrameBuffer(ChannelExtension->RawFrameLength);

            	if (ChannelExtension->TransferExtension[i].DataBuffer == NULL) {
	        		USBCAMD_KdPrint (MIN_TRACE, ("'Bulk buffer alloc failed\n"));
    	    		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    	    		goto USBCAMD_PrepareChannel_Done;
    			}
    			ChannelExtension->TransferExtension[i].BufferLength =
    				ChannelExtension->RawFrameLength;	

    			// initilize bulk transfer parms.                                        
    			ntStatus = USBCAMD_InitializeBulkTransfer(DeviceExtension,
            					                    ChannelExtension,
                                				    DeviceExtension->Interface,
                                   					&ChannelExtension->TransferExtension[i],
                                   					ChannelExtension->DataPipe);
				if (ntStatus != STATUS_SUCCESS) {
	        		USBCAMD_KdPrint (MIN_TRACE, ("Bulk Transfer Init failed\n"));
    	    		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    	    		goto USBCAMD_PrepareChannel_Done;
    			}
            }
        }
        else if ( ChannelExtension->VirtualStillPin) {
            ChannelExtension->DataPipe = DeviceExtension->ChannelExtension[STREAM_Capture]->DataPipe;
            ChannelExtension->DataPipeType = DeviceExtension->ChannelExtension[STREAM_Capture]->DataPipeType;
        }
        else {
            TEST_TRAP();
        }

        ChannelExtension->SyncPipe = DeviceExtension->SyncPipe;

        if ( ChannelExtension->DataPipeType == UsbdPipeTypeIsochronous ) {

                for (i=0; i< USBCAMD_MAX_REQUEST; i++) {

                    ntStatus = USBCAMD_InitializeIsoTransfer(DeviceExtension,
                                                             ChannelExtension,
                                                             DeviceExtension->Interface,
                                                             &ChannelExtension->TransferExtension[i],
                                                             i);

                    if (!NT_SUCCESS(ntStatus)) {

                        // The close channel code will clean up anything we
                        // allocated
                        //
                        break;
                    }
                }
        }
    }

    if (NT_SUCCESS(ntStatus)) {

        //
        // we have the BW and memory we need, go ahead and start
        // our timeoutDPC
        //

        ChannelExtension->ChannelPrepared = TRUE;
        //
        // start out ChannelTimeoutDPC here
        //
        ChannelExtension->Flags |= USBCAMD_ENABLE_TIMEOUT_DPC;

        KeInitializeTimerEx(&ChannelExtension->TimeoutTimer,SynchronizationEvent);
        KeInitializeDpc(&ChannelExtension->TimeoutDpc,
                        USBCAMD_ChannelTimeoutDPC,
                        ChannelExtension);

        if ( StreamNumber == STREAM_Capture ) {
            dueTime.QuadPart = -10000 * USBCAMD_TIMEOUT_INTERVAL;
        }
        else {
            dueTime.QuadPart = -10000 * USBCAMD_STILL_TIMEOUT;
        }

        inQueue = KeSetTimer(&ChannelExtension->TimeoutTimer,
                             dueTime,
                             &ChannelExtension->TimeoutDpc);

        ASSERT(inQueue == FALSE);
    }

USBCAMD_PrepareChannel_Done:

    USBCAMD_KdPrint (MIN_TRACE, ("'exit USBCAMD_PrepareChannel (%x)\n", ntStatus));

    USBCAMD_RELEASE(DeviceExtension);

    return ntStatus;
}


NTSTATUS
USBCAMD_StartIsoThread(
IN PUSBCAMD_DEVICE_EXTENSION pDeviceExt
)
{
	NTSTATUS ntStatus ;
	HANDLE hThread;

	//
	// we are ready to start the thread that handle read SRb completeion 
	// after iso transfer completion routine puts them in the que.
	//
	pDeviceExt->StopIsoThread = FALSE;
	ntStatus = PsCreateSystemThread(&hThread,
									(ACCESS_MASK)0,
									NULL,
									(HANDLE) 0,
									NULL,
									USBCAMD_ProcessIsoIrps,
									pDeviceExt);
		
	if (!NT_SUCCESS(ntStatus)) {								
   		USBCAMD_KdPrint (MIN_TRACE, ("Iso Thread Creation Failed\n"));
        return ntStatus;
	}

	// get a pointer to the thread object.
	ObReferenceObjectByHandle(hThread,
							  THREAD_ALL_ACCESS,
							  NULL,
							  KernelMode,
							  &pDeviceExt->IsoThreadObject,
							  NULL);
								  
	// release the thread handle.
	ZwClose( hThread);

	return ntStatus;
}

//---------------------------------------------------------------------------
// USBCAMD_UnPrepareChannel
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_UnPrepareChannel(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension
    )
/*++

Routine Description:

    Frees resources allocated in PrepareChannel.

Arguments:

    DeviceExtension - points to the driver specific DeviceExtension

    Irp - Irp associated with this request.

    ChannelExtension - context data for this channel.

Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG i,StreamNumber;

    USBCAMD_SERIALIZE(DeviceExtension);

    USBCAMD_KdPrint (MAX_TRACE, ("'enter USBCAMD_UnPrepareChannel\n"));
    StreamNumber = ChannelExtension->StreamNumber;

    ASSERT_CHANNEL(ChannelExtension);
    ASSERT(DeviceExtension->ActualInstances[StreamNumber] > 0);

    if (!ChannelExtension->ChannelPrepared ||
        ChannelExtension->ImageCaptureStarted) {
        // fail the call if the channel is not in the
        // proper state.
        TRAP();
        ntStatus = STATUS_UNSUCCESSFUL;
        goto USBCAMD_UnPrepareChannel_Done;
    }


    //
    // stop our timeoutDPC routine
    //

    ChannelExtension->Flags &= ~USBCAMD_ENABLE_TIMEOUT_DPC;
    KeCancelTimer(&ChannelExtension->TimeoutTimer);

    //
    // hopefully put us in the mode that uses no bandwidth
    // ie select and alt interface that has a minimum iso
    // packet size
    //

    if (ChannelExtension->VirtualStillPin == TRUE) {
        ntStatus = STATUS_SUCCESS;
        goto USBCAMD_UnPrepareChannel_Done;
    }

    // attempt to stop
    if (DeviceExtension->Usbcamd_version == USBCAMD_VERSION_200) {
        (*DeviceExtension->DeviceDataEx.DeviceData2.CamStopCaptureEx)(
                DeviceExtension->StackDeviceObject,
                USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension),
                StreamNumber);
    }
    else {
        (*DeviceExtension->DeviceDataEx.DeviceData.CamStopCapture)(
                DeviceExtension->StackDeviceObject,
                USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension));

    }

    if (DeviceExtension->Usbcamd_version == USBCAMD_VERSION_200) {
        ntStatus =
            (*DeviceExtension->DeviceDataEx.DeviceData2.CamFreeBandwidthEx)(
                    DeviceExtension->StackDeviceObject,
                    USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension),
                    StreamNumber);
    }
    else {
        ntStatus =
            (*DeviceExtension->DeviceDataEx.DeviceData.CamFreeBandwidth)(
                    DeviceExtension->StackDeviceObject,
                    USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension));
    }

    if (!NT_SUCCESS(ntStatus)) {
        USBCAMD_KdPrint (MAX_TRACE, (
            "USBCAMD_UnPrepareChannel failed stop capture  (%x)\n", ntStatus));

        //
        // ignore any errors on the stop
        //
        ntStatus = STATUS_SUCCESS;
    }

    //
    // Note:
    // We may get an error here if the camera hs been unplugged,
    // if this is the case we still need to free up the
    // channel resources
    //
    if ( ChannelExtension->DataPipeType == UsbdPipeTypeIsochronous ) {

        for (i=0; i< USBCAMD_MAX_REQUEST; i++) {
            USBCAMD_FreeIsoTransfer(ChannelExtension,
                                    &ChannelExtension->TransferExtension[i]);
        }

        // kill the iso thread.
        USBCAMD_KillIsoThread(DeviceExtension);
    }
    else {
    	//
    	// free bulk buffers in channel transfer extensions.
    	//
    	for ( i =0; i < USBCAMD_MAX_REQUEST; i++) {
            if (ChannelExtension->TransferExtension[i].DataBuffer != NULL) {
            	USBCAMD_FreeRawFrameBuffer(ChannelExtension->TransferExtension[i].DataBuffer);
				ChannelExtension->TransferExtension[i].DataBuffer = NULL;
    		}

    		if ( ChannelExtension->ImageCaptureStarted )
            	USBCAMD_FreeBulkTransfer(&ChannelExtension->TransferExtension[i]);
        }
    }

USBCAMD_UnPrepareChannel_Done:
    //
    // channel is no longer prepared
    //

    ChannelExtension->ChannelPrepared = FALSE;


    USBCAMD_KdPrint (MIN_TRACE, ("'exit USBCAMD_UnPrepareChannel (%x)\n", ntStatus));

    USBCAMD_RELEASE(DeviceExtension);

    return ntStatus;
}

VOID
USBCAMD_KillIsoThread(
	IN PUSBCAMD_DEVICE_EXTENSION pDeviceExt)
{
	pDeviceExt->StopIsoThread = TRUE; // Set the thread stop flag

	// Wake up the thread if asleep.
	KeReleaseSemaphore(&pDeviceExt->CompletedSrbListSemaphore,0,1,TRUE);
	USBCAMD_KdPrint (MIN_TRACE,("Waiting for Iso Thread to Terminate\n"));
	// Wait for the iso thread to kill himself
	KeWaitForSingleObject(pDeviceExt->IsoThreadObject,Executive,KernelMode,
							FALSE,NULL);
	USBCAMD_KdPrint (MIN_TRACE,("Iso Thread Terminated\n"));

	ObDereferenceObject(pDeviceExt->IsoThreadObject);
}


//---------------------------------------------------------------------------
// USBCAMD_ReadChannel
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_ReadChannel(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension,
    IN PUSBCAMD_READ_EXTENSION ReadExtension
    )
/*++

Routine Description:

    Reads a video frame from a channel.

Arguments:

    DeviceExtension - points to the driver specific DeviceExtension

    Irp - Irp associated with this request.

    ChannelExtension - context data for this channel.

    Mdl - Mdl for this read request.

    Length - Number of bytes to read.

Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG StreamNumber;
    PHW_STREAM_REQUEST_BLOCK Srb;

    USBCAMD_KdPrint (ULTRA_TRACE, ("'enter USBCAMD_ReadChannel\n"));
    //
    // make sure we don't get reads on a closed channel
    //
    StreamNumber = ChannelExtension->StreamNumber;

    ASSERT_READ(ReadExtension);
    ASSERT_CHANNEL(ChannelExtension);
    ASSERT(DeviceExtension->ActualInstances[StreamNumber] > 0);
    ASSERT(ChannelExtension->ChannelPrepared == TRUE);

    Srb = ReadExtension->Srb;

    if (  ChannelExtension->RawFrameLength == 0) 
	   	 return STATUS_INSUFFICIENT_RESOURCES;	
	//
	// for streaming on bulk pipes. we use the buffer allocated in
	// transfer extension.
	//
	if (ChannelExtension->DataPipeType == UsbdPipeTypeBulk ) {
       	ReadExtension->RawFrameLength = ReadExtension->ActualRawFrameLen = 
       			ChannelExtension->RawFrameLength;
       			ReadExtension->RawFrameBuffer = NULL;

	}
	else { 
		if ( ChannelExtension->NoRawProcessingRequired) {
        	// no buffer allocation needed. use DS allocated buffer.
        	if ( ChannelExtension->RawFrameLength <=
            	  ChannelExtension->VideoInfoHeader->bmiHeader.biSizeImage ){
            	ReadExtension->RawFrameBuffer =
                	(PUCHAR) ((PHW_STREAM_REQUEST_BLOCK) Srb)->CommandData.DataBufferArray->Data;
            	ReadExtension->RawFrameLength =
                	((PHW_STREAM_REQUEST_BLOCK) Srb)->CommandData.DataBufferArray->FrameExtent;
        	}
        	else 
		    	 return STATUS_INSUFFICIENT_RESOURCES;	
    	}
    	else {

        	ASSERT(ChannelExtension->RawFrameLength > 0 &&
            	   ChannelExtension->RawFrameLength < 550000 );
        	USBCAMD_KdPrint (ULTRA_TRACE, ("RawFrameLength %d\n",ChannelExtension->RawFrameLength));

        	ReadExtension->RawFrameLength = ChannelExtension->RawFrameLength;

        	ReadExtension->RawFrameBuffer =
            	USBCAMD_AllocateRawFrameBuffer(ReadExtension->RawFrameLength);

            if (ReadExtension->RawFrameBuffer == NULL) {
	        	USBCAMD_KdPrint (MIN_TRACE, ("'Read alloc failed\n"));
    	    	ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    	    	return ntStatus;
    		}
    	}
	}
	
    USBCAMD_KdPrint (MAX_TRACE, ("Que SRB (%x) S# %d.\n",
                    ReadExtension->Srb ,StreamNumber));

    ExInterlockedInsertTailList( &(ChannelExtension->PendingIoList),
                                     &(ReadExtension->ListEntry),
                                     &ChannelExtension->PendingIoListSpin);

    USBCAMD_KdPrint (ULTRA_TRACE, ("'exit USBCAMD_ReadChannel 0x%x\n", ntStatus));

    return STATUS_SUCCESS;
}

//---------------------------------------------------------------------------
// USBCAMD_StartChannel
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_StartChannel(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION  ChannelExtension
    )
/*++

Routine Description:

    Starts the streaming process for a video channel.

Arguments:

    DeviceExtension - points to the driver specific DeviceExtension

    ChannelExtension - context data for this channel.

Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG StreamNumber;

    USBCAMD_SERIALIZE(DeviceExtension);

    USBCAMD_KdPrint (MAX_TRACE, ("enter USBCAMD_StartChannel\n"));

    ASSERT_CHANNEL(ChannelExtension);
    StreamNumber = ChannelExtension->StreamNumber;


    if (ChannelExtension->ImageCaptureStarted) {
        // fail the call if the channel is not in the
        // proper state.
        TRAP();
        ntStatus = STATUS_UNSUCCESSFUL;
        goto USBCAMD_StartChannel_Done;
    }

#if DBG
    {
        ULONG i;

        ASSERT(DeviceExtension->ActualInstances[StreamNumber] > 0);
        ASSERT(ChannelExtension->StreamError == FALSE);
        //ASSERT(ChannelExtension->Flags == 0);

        if ( ChannelExtension->VirtualStillPin == FALSE) {

            if (ChannelExtension->DataPipeType == UsbdPipeTypeIsochronous ) {
                for (i=0; i< USBCAMD_MAX_REQUEST; i++) {
                    ASSERT(ChannelExtension->TransferExtension[i].ChannelExtension != NULL);
                }
            }
        }
    }
#endif

    if ( ChannelExtension->VirtualStillPin == TRUE) {
        // check if the capture pin has started yet?
        if ( (DeviceExtension->ChannelExtension[STREAM_Capture] != NULL) &&
             (DeviceExtension->ChannelExtension[STREAM_Capture]->ImageCaptureStarted) ){
            ChannelExtension->ImageCaptureStarted = TRUE;
        }
        else{
            // We can't start a virtual still pin till after we start the capture pin.
            ntStatus = STATUS_UNSUCCESSFUL;
        }
    }
    else {

        //
        // Perform a reset on the pipes
        //
        if ( ChannelExtension->DataPipeType == UsbdPipeTypeIsochronous ){

            ntStatus = USBCAMD_ResetPipes(DeviceExtension,
                                          ChannelExtension,
                                          DeviceExtension->Interface,
                                          FALSE);
        }

        //
        // start the stream up, we don't check for errors here
        //

        if (NT_SUCCESS(ntStatus)) {

            if ( ChannelExtension->DataPipeType == UsbdPipeTypeIsochronous ){
                ntStatus = USBCAMD_StartIsoStream(DeviceExtension, ChannelExtension, TRUE);
            }
            else {
                ntStatus = USBCAMD_StartBulkStream(DeviceExtension, ChannelExtension, TRUE);
            }
        }
    }

USBCAMD_StartChannel_Done:

    USBCAMD_KdPrint (MIN_TRACE, ("exit USBCAMD_StartChannel (%x)\n", ntStatus));

    USBCAMD_RELEASE(DeviceExtension);

    return ntStatus;
}

//---------------------------------------------------------------------------
// USBCAMD_StopChannel
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_StopChannel(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension
    )
/*++

Routine Description:

    Stops the streaming process for a video channel.

Arguments:

    DeviceExtension - points to the driver specific DeviceExtension

    Irp - Irp associated with this request.

    ChannelExtension - context data for this channel.

Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG StreamNumber;

    USBCAMD_SERIALIZE(DeviceExtension);

    USBCAMD_KdPrint (MAX_TRACE, ("enter USBCAMD_StopChannel\n"));

    ASSERT_CHANNEL(ChannelExtension);
    StreamNumber = ChannelExtension->StreamNumber;
    ASSERT(ChannelExtension->ChannelPrepared == TRUE);
    ASSERT(DeviceExtension->ActualInstances[StreamNumber] > 0);

    if (!ChannelExtension->ImageCaptureStarted ) {
        //
        // we are not started so we just return success
        //
        USBCAMD_KdPrint (MAX_TRACE, ("stop before start -- return success\n"));
        ntStatus = STATUS_SUCCESS;
        goto USBCAMD_StopChannel_Done;
    }


	if ( ChannelExtension->DataPipeType == UsbdPipeTypeBulk ) {
    	// for bulk pipes. Just make sure to cancel the current read request.
        // there is a pending IRP on this Pipe. Cancel it
        ntStatus = USBCAMD_CancelOutstandingIrp(DeviceExtension,
                                                ChannelExtension->DataPipe,
                                                FALSE);
        ChannelExtension->StreamError = FALSE;
    	ChannelExtension->ImageCaptureStarted = FALSE;
        goto USBCAMD_StopChannel_Done;
    }

    //
    // first we set our stop flag
    //
    // initialize the event before setting StopIrp
    // since StopIrp is what our DPC looks at.
    //
    //

    KeInitializeEvent(&ChannelExtension->StopEvent, SynchronizationEvent, FALSE);
    ChannelExtension->Flags |= USBCAMD_STOP_STREAM;

    //
    // now send an abort pipe for both our pipes, this should flush out any
    // transfers that are running
    //

    if ( ChannelExtension->VirtualStillPin == FALSE) {
        // we only need to abort for iso pipes.
        if ( ChannelExtension->DataPipeType == UsbdPipeTypeIsochronous ) {
            ntStatus = USBCAMD_AbortPipe(DeviceExtension,
                    DeviceExtension->Interface->Pipes[ChannelExtension->DataPipe].PipeHandle);

            if (NT_ERROR(ntStatus)) {
               TEST_TRAP();
            }

            if (ChannelExtension->SyncPipe != -1) {
                ntStatus = USBCAMD_AbortPipe(DeviceExtension,
                        DeviceExtension->Interface->Pipes[ChannelExtension->SyncPipe].PipeHandle);
                if (NT_ERROR(ntStatus)) {
                    TEST_TRAP();
                }
            }
        }
    }


    //
    // block the stop for now, we will let our timeoutDPC complete
    // it when all iso irps are no longer pending
    //

    {
        NTSTATUS status;
        // specify a timeout value of 8 seconds for this call to complete.
        static LARGE_INTEGER Timeout = {(ULONG) -80000000, -1};

        status = KeWaitForSingleObject(
                           &ChannelExtension->StopEvent,
                           Executive,
                           KernelMode,
                           FALSE,
                           &Timeout);

        if (status == STATUS_TIMEOUT) {
            USBCAMD_KdPrint (MIN_TRACE, ("*** Waiting on Abort Pipe Timed out.*** \n"));
            //TEST_TRAP();
        }

    }

    //
    // clear the error state flag, we are now stopped
    //

    ChannelExtension->StreamError = FALSE;
    ChannelExtension->ImageCaptureStarted = FALSE;

USBCAMD_StopChannel_Done:


#if DBG
    USBCAMD_DebugStats(ChannelExtension);
#endif

    USBCAMD_KdPrint (MIN_TRACE, ("exit USBCAMD_StopChannel (%x)\n", ntStatus));
    USBCAMD_RELEASE(DeviceExtension);
    return ntStatus;
}




//---------------------------------------------------------------------------
// USBCAMD_AbortPipe
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_AbortPipe(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN USBD_PIPE_HANDLE PipeHandle
    )
/*++

Routine Description:

    Abort pending transfers for a given USB pipe.

Arguments:

    DeviceExtension - Pointer to the device extension for this instance of the USB camera
                    devcice.

    PipeHandle - usb pipe handle to abort trasnsfers for.


Return Value:

    NT status code.

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PURB urb;
    ULONG currentUSBFrame = 0;

    urb = USBCAMD_ExAllocatePool(NonPagedPool,
                         sizeof(struct _URB_PIPE_REQUEST));

    if (urb) {

        urb->UrbHeader.Length = (USHORT) sizeof (struct _URB_PIPE_REQUEST);
        urb->UrbHeader.Status = 0;
        urb->UrbHeader.Function = URB_FUNCTION_ABORT_PIPE;
        urb->UrbPipeRequest.PipeHandle = PipeHandle;

        ntStatus = USBCAMD_CallUSBD(DeviceExtension, urb);

        USBCAMD_ExFreePool(urb);

    } else {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    USBCAMD_KdPrint (MIN_TRACE, ("Abort Pipe Return ntStatus(%x) \n",ntStatus));
    return ntStatus;
}


//---------------------------------------------------------------------------
// USBCAMD_StartStream
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_StartIsoStream(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension,
    IN BOOLEAN Initialize
    )
/*++

Routine Description:

    This is the code that starts the streaming process.

Arguments:

    DeviceExtension - Pointer to the device extension for this instance of the USB camera
                    device.

Return Value:

    NT status code.

--*/
{
    ULONG i;
	NTSTATUS ntStatus = STATUS_SUCCESS;

#if DBG
    // initialize debug count variables
    ChannelExtension->IgnorePacketCount =
    ChannelExtension->ErrorDataPacketCount =
    ChannelExtension->ErrorSyncPacketCount =
    ChannelExtension->SyncNotAccessedCount =
    ChannelExtension->DataNotAccessedCount = 0;

    if (USBCAMD_StreamEnable == 0) {
        return ntStatus;
    }
#endif


    ChannelExtension->CurrentUSBFrame =
        USBCAMD_GetCurrentFrame(DeviceExtension) + 10;

    for (i=0; i<USBCAMD_MAX_REQUEST; i++) {
        PUSBCAMD_TRANSFER_EXTENSION transferExtension;

        transferExtension = &ChannelExtension->TransferExtension[i];

        if (Initialize) {

            if (ChannelExtension->SyncPipe != -1) {
                USBCAMD_RecycleIrp(transferExtension,
                                   transferExtension->SyncIrp,
                                   transferExtension->SyncUrb,
                                   USBCAMD_IsoIrp_Complete);

                USBCAMD_InitializeIsoUrb(DeviceExtension,
                                         transferExtension->SyncUrb,
                                         &DeviceExtension->Interface->Pipes[ChannelExtension->SyncPipe],
                                         transferExtension->SyncBuffer);

                RtlZeroMemory(transferExtension->SyncBuffer,
                        USBCAMD_NUM_ISO_PACKETS_PER_REQUEST);
            }

            USBCAMD_RecycleIrp(transferExtension,
                               transferExtension->DataIrp,
                               transferExtension->DataUrb,
                               USBCAMD_IsoIrp_Complete);

            USBCAMD_InitializeIsoUrb(DeviceExtension,
                                     transferExtension->DataUrb,
                                     &DeviceExtension->Interface->Pipes[ChannelExtension->DataPipe],
                                     transferExtension->DataBuffer);

        }

        ntStatus = USBCAMD_SubmitIsoTransfer(DeviceExtension,
                                  transferExtension,
                                  ChannelExtension->CurrentUSBFrame,
                                  FALSE);

        ChannelExtension->CurrentUSBFrame +=
            USBCAMD_NUM_ISO_PACKETS_PER_REQUEST;

    }
    if ( ntStatus == STATUS_SUCCESS) 
	    ChannelExtension->ImageCaptureStarted = TRUE;
	return ntStatus;
}

//---------------------------------------------------------------------------
// USBCAMD_StartBulkStream
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_StartBulkStream(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension,
    IN BOOLEAN Initialize
    )
/*++

Routine Description:

    This is the code that starts the streaming process.

Arguments:

    DeviceExtension - Pointer to the device extension for this instance of the USB camera
                    device.

Return Value:

    NT status code.

--*/
{
  ULONG i;
  ULONG ntStatus = STATUS_SUCCESS;

#if DBG
    // initialize debug count variables
    ChannelExtension->IgnorePacketCount =
    ChannelExtension->ErrorDataPacketCount =
    ChannelExtension->ErrorSyncPacketCount =
    ChannelExtension->SyncNotAccessedCount =
    ChannelExtension->DataNotAccessedCount = 0;

#endif
    
ChannelExtension->CurrentBulkTransferIndex = i = 0;
		
ntStatus = USBCAMD_IntOrBulkTransfer(DeviceExtension,
	                   		ChannelExtension,
    	                    ChannelExtension->TransferExtension[i].DataBuffer,
        	                ChannelExtension->TransferExtension[i].BufferLength,
            	            ChannelExtension->DataPipe,
                	        NULL,
                    	    NULL,
                        	0,
                        	BULK_TRANSFER);        

if ( ntStatus == STATUS_SUCCESS) 
	    ChannelExtension->ImageCaptureStarted = TRUE;
return ntStatus;

}


//---------------------------------------------------------------------------
// USBCAMD_ControlVendorCommand
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_ControlVendorCommandWorker(
    IN PVOID DeviceContext,
    IN UCHAR Request,
    IN USHORT Value,
    IN USHORT Index,
    IN PVOID Buffer,
    IN OUT PULONG BufferLength,
    IN BOOLEAN GetData
    )
/*++

Routine Description:

    Send a vendor command to the camera to fetch data.

Arguments:

    DeviceExtension - pointer to the device extension for this instance of the USB camera
                    devcice.

    Request - Request code for setup packet.

    Value - Value for setup packet.

    Index - Index for setup packet.

    Buffer - Pointer to input buffer

    BufferLength - pointer size of input/output buffer (optional)

Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus;
    BOOLEAN allocated = FALSE;
    PUCHAR localBuffer;
    PUCHAR buffer;
    PURB urb;
    PUSBCAMD_DEVICE_EXTENSION deviceExtension;
    ULONG length = BufferLength ? *BufferLength : 0;

    USBCAMD_KdPrint (MAX_TRACE, ("'enter USBCAMD_ControlVendorCommand\n"));

    deviceExtension = USBCAMD_GET_DEVICE_EXTENSION(DeviceContext);

    buffer = USBCAMD_ExAllocatePool(NonPagedPool,
                            sizeof(struct
                            _URB_CONTROL_VENDOR_OR_CLASS_REQUEST) + length);


    if (buffer) {
        urb = (PURB) (buffer + length);

        USBCAMD_KdPrint (ULTRA_TRACE, ("'enter USBCAMD_ControlVendorCommand req %x val %x index %x\n",
            Request, Value, Index));

        if (BufferLength && *BufferLength != 0) {
            localBuffer = buffer;
            if (!GetData) {
                RtlCopyMemory(localBuffer, Buffer, *BufferLength);
            }
        } else {
            localBuffer = NULL;
        }

        UsbBuildVendorRequest(urb,
                              URB_FUNCTION_VENDOR_DEVICE,
                              sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
                              GetData ? USBD_TRANSFER_DIRECTION_IN :
                                  0,
                              0,
                              Request,
                              Value,
                              Index,
                              localBuffer,
                              NULL,
                              length,
                              NULL);

        USBCAMD_KdPrint (ULTRA_TRACE, ("'BufferLength =  0x%x buffer = 0x%x\n",
            length, localBuffer));

        ntStatus = USBCAMD_CallUSBD(deviceExtension, urb);

        if (NT_SUCCESS(ntStatus)) {
            if (BufferLength) {
                *BufferLength =
                    urb->UrbControlVendorClassRequest.TransferBufferLength;

                USBCAMD_KdPrint (ULTRA_TRACE, ("'BufferLength =  0x%x buffer = 0x%x\n",
                    *BufferLength, localBuffer));
                if (localBuffer && GetData) {
                    RtlCopyMemory(Buffer, localBuffer, *BufferLength);
                }
            }
        }
        else {
            USBCAMD_KdPrint (MIN_TRACE, ("USBCAMD_ControlVendorCommand Error 0x%x\n", ntStatus));            

            // Only expected failure.
            // TEST_TRAP();        
        }

        USBCAMD_ExFreePool(buffer);
    } else {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        USBCAMD_KdPrint (MIN_TRACE, ("'USBCAMD_ControlVendorCommand Error 0x%x\n", ntStatus));
    }

    return ntStatus;

}


//---------------------------------------------------------------------------
// USBCAMD_ControlVendorCommand
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_ControlVendorCommand(
    IN PVOID DeviceContext,
    IN UCHAR Request,
    IN USHORT Value,
    IN USHORT Index,
    IN PVOID Buffer,
    IN OUT PULONG BufferLength,
    IN BOOLEAN GetData,
    IN PCOMMAND_COMPLETE_FUNCTION CommandComplete,
    IN PVOID CommandContext
    )
/*++

Routine Description:

    Send a vendor command to the camera to fetch data.

Arguments:

    DeviceExtension - pointer to the device extension for this instance of the USB camera
                    devcice.

    Request - Request code for setup packet.

    Value - Value for setup packet.

    Index - Index for setup packet.

    Buffer - Pointer to input buffer

    BufferLength - pointer size of input/output buffer (optional)

Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus;
    PCOMMAND_WORK_ITEM workitem;

    USBCAMD_KdPrint (MAX_TRACE, ("'enter USBCAMD_ControlVendorCommand2\n"));

    if (KeGetCurrentIrql() < DISPATCH_LEVEL) {
        //
        // we are at passive level, just do the command
        //
        ntStatus = USBCAMD_ControlVendorCommandWorker(DeviceContext,
                                                Request,
                                                Value,
                                                Index,
                                                Buffer,
                                                BufferLength,
                                                GetData);

        if (CommandComplete) {
            // call the completion handler
            (*CommandComplete)(DeviceContext, CommandContext, ntStatus);
        }

    } else {
//        TEST_TRAP();
        //
        // schedule a work item
        //
        ntStatus = STATUS_PENDING;

        workitem = USBCAMD_ExAllocatePool(NonPagedPool,
                                          sizeof(COMMAND_WORK_ITEM));
        if (workitem) {

            ExInitializeWorkItem(&workitem->WorkItem,
                                 USBCAMD_CommandWorkItem,
                                 workitem);

            workitem->DeviceContext = DeviceContext;
            workitem->Request = Request;
            workitem->Value = Value;
            workitem->Index = Index;
            workitem->Buffer = Buffer;
            workitem->BufferLength = BufferLength;
            workitem->GetData = GetData;
            workitem->CommandComplete = CommandComplete;
            workitem->CommandContext = CommandContext;

            ExQueueWorkItem(&workitem->WorkItem,
                            DelayedWorkQueue);

        } else {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }

    }

    return ntStatus;
}


VOID
USBCAMD_CommandWorkItem(
    PVOID Context
    )
/*++

Routine Description:

    Call the mini driver to convert a raw packet to the proper format.

Arguments:

Return Value:

    None.

--*/
{
    NTSTATUS ntStatus;
    PCOMMAND_WORK_ITEM workItem = Context;

    ntStatus = USBCAMD_ControlVendorCommandWorker(workItem->DeviceContext,
                                            workItem->Request,
                                            workItem->Value,
                                            workItem->Index,
                                            workItem->Buffer,
                                            workItem->BufferLength,
                                            workItem->GetData);


    if (workItem->CommandComplete) {
        // call the completion handler
        (*workItem->CommandComplete)(workItem->DeviceContext,
                                   workItem->CommandContext,
                                   ntStatus);
    }

    USBCAMD_ExFreePool(workItem);
}


NTSTATUS
USBCAMD_GetRegistryKeyValue (
    IN HANDLE Handle,
    IN PWCHAR KeyNameString,
    IN ULONG KeyNameStringLength,
    IN PVOID Data,
    IN ULONG DataLength
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    NTSTATUS ntStatus = STATUS_NO_MEMORY;
    UNICODE_STRING keyName;
    ULONG length;
    PKEY_VALUE_FULL_INFORMATION fullInfo;

    RtlInitUnicodeString(&keyName, KeyNameString);

    length = sizeof(KEY_VALUE_FULL_INFORMATION) +
            KeyNameStringLength + DataLength;

    fullInfo = USBCAMD_ExAllocatePool(PagedPool, length);
    USBCAMD_KdPrint(MAX_TRACE, ("' USBD_GetRegistryKeyValue buffer = 0x%p\n", (ULONG_PTR) fullInfo));

    if (fullInfo) {
        ntStatus = ZwQueryValueKey(Handle,
                        &keyName,
                        KeyValueFullInformation,
                        fullInfo,
                        length,
                        &length);

        if (NT_SUCCESS(ntStatus)){
            ASSERT(DataLength == fullInfo->DataLength);
            RtlCopyMemory(Data, ((PUCHAR) fullInfo) + fullInfo->DataOffset, DataLength);
        }

        USBCAMD_ExFreePool(fullInfo);
    }

    return ntStatus;
}

#if DBG

typedef struct _RAW_SIG {
    ULONG Sig;
    ULONG length;
} RAW_SIG, *PRAW_SIG;


PVOID
USBCAMD_AllocateRawFrameBuffer(
    ULONG RawFrameLength
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    PRAW_SIG rawsig;
    PUCHAR pch;

    pch = USBCAMD_ExAllocatePool(NonPagedPool,
                         RawFrameLength + sizeof(*rawsig)*2);

    if (pch) {
        // begin sig
        rawsig = (PRAW_SIG) pch;
        rawsig->Sig = USBCAMD_RAW_FRAME_SIG;
        rawsig->length = RawFrameLength;


        // end sig
        rawsig = (PRAW_SIG) (pch+RawFrameLength+sizeof(*rawsig));
        rawsig->Sig = USBCAMD_RAW_FRAME_SIG;
        rawsig->length = RawFrameLength;

        pch += sizeof(*rawsig);
    }

    return pch;
}


VOID
USBCAMD_FreeRawFrameBuffer(
    PVOID RawFrameBuffer
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    PUCHAR pch;

    USBCAMD_CheckRawFrameBuffer(RawFrameBuffer);

    pch = RawFrameBuffer;
    pch -= sizeof(RAW_SIG);

    USBCAMD_ExFreePool(pch);
}


VOID
USBCAMD_CheckRawFrameBuffer(
    PVOID RawFrameBuffer
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{

}

typedef struct _NODE_HEADER {
    ULONG Length;
    ULONG Sig;
} NODE_HEADER, *PNODE_HEADER;

PVOID
USBCAMD_ExAllocatePool(
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes
    )
{
    PNODE_HEADER tmp;

    tmp = ExAllocatePoolWithTag(PoolType, NumberOfBytes+sizeof(*tmp), 'MACU');

    if (tmp) {
        USBCAMD_HeapCount += NumberOfBytes;
        tmp->Length = NumberOfBytes;
        tmp->Sig = 0xDEADBEEF;
        tmp++;
    }

    return tmp;
}


VOID
USBCAMD_ExFreePool(
    IN PVOID p
    )
{
    PNODE_HEADER tmp;

    tmp = p;
    tmp--;
    ASSERT(tmp->Sig == 0xDEADBEEF);
    tmp->Sig = 0;

    USBCAMD_HeapCount-=tmp->Length;

    ExFreePool(tmp);

}

#endif


//---------------------------------------------------------------------------
// USBCAMD_SetDevicePowerState
//---------------------------------------------------------------------------
NTSTATUS
USBCAMD_SetDevicePowerState(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PHW_STREAM_REQUEST_BLOCK Srb
    )
/*++

Routine Description:

Arguments:

    DeviceExtension - points to the driver specific DeviceExtension

    DevicePowerState - Device power state to enter.

Return Value:

    NT status code

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
	DEVICE_POWER_STATE DevicePowerState = Srb->CommandData.DeviceState;

    USBCAMD_KdPrint (MIN_TRACE, ("enter SetDevicePowerState\n"));

    if (DeviceExtension->CurrentPowerState != DevicePowerState) {

        switch (DevicePowerState) {
			case PowerDeviceD0:
				//
				// we can't talk to usb stack till the IRP assocaited with this 
				// SRB is completed by everybody on the stack.
				// Schedule a work item to complete this later.
				//
 				if ( DeviceExtension->CurrentPowerState == PowerDeviceD3 ) {
                   USBCAMD_KdPrint (MIN_TRACE, ("USBCAMD: Switching from D3 to D0\n"));
                   
                   /*
					ntStatus =
						(*DeviceExtension->DeviceDataEx.DeviceData.CamRestoreState)(
						DeviceExtension->StackDeviceObject,
						USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension));
					if (NT_ERROR (ntStatus)) {
						USBCAMD_KdPrint (MIN_TRACE, ("USBCAMD: Cam Driver Failed to restore its state\n"));
					}
					*/
                    // start iso stream if any.
                    if ((DeviceExtension->ChannelExtension[STREAM_Capture] != NULL) &&
                        (DeviceExtension->ChannelExtension[STREAM_Capture]->DataPipeType == UsbdPipeTypeIsochronous )){
	
                        USBCAMD_KdPrint (MIN_TRACE, ("Restore ISO stream .\n"));
						DeviceExtension->ChannelExtension[STREAM_Capture]->StreamError = TRUE;
						DeviceExtension->ChannelExtension[STREAM_Capture]->IdleIsoStream = FALSE;
        				DeviceExtension->ChannelExtension[STREAM_Capture]->ImageCaptureStarted = TRUE;

					/*
                        USBCAMD_ProcessSetIsoPipeState(DeviceExtension,
                                            DeviceExtension->ChannelExtension[STREAM_Capture],
                                            USBCAMD_START_STREAM);
                    */
                    }
					
				}
				break;
	
			case PowerDeviceD1:
			case PowerDeviceD2:
				ntStatus = STATUS_SUCCESS;
				break;
			case PowerDeviceD3:
				
				if ( DeviceExtension->CurrentPowerState == PowerDeviceD0 ) {
					USBCAMD_KdPrint (MIN_TRACE, ("USBCAMD: Switching from D0 to D3\n"));
					
                    // stop iso stream if any.
                    if ((DeviceExtension->ChannelExtension[STREAM_Capture] != NULL) &&
                        (DeviceExtension->ChannelExtension[STREAM_Capture]->DataPipeType == UsbdPipeTypeIsochronous )){

                        USBCAMD_KdPrint (MIN_TRACE, ("Stop ISO stream .\n"));

                        USBCAMD_ProcessSetIsoPipeState(DeviceExtension,
                                            DeviceExtension->ChannelExtension[STREAM_Capture],
                                            USBCAMD_STOP_STREAM);

                        if (DeviceExtension->Usbcamd_version == USBCAMD_VERSION_200) {

			                // send hardware stop
				            ntStatus = (*DeviceExtension->DeviceDataEx.DeviceData2.CamStopCaptureEx)(
							            DeviceExtension->StackDeviceObject,
							            USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension),
                                        STREAM_Capture);
                        }
                        else {
			                // send hardware stop
				                ntStatus = (*DeviceExtension->DeviceDataEx.DeviceData.CamStopCapture)(
							                DeviceExtension->StackDeviceObject,
							                USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension));
                        }
                    }
                    /*
					ntStatus =
						(*DeviceExtension->DeviceDataEx.DeviceData.CamSaveState)(
						DeviceExtension->StackDeviceObject,
						USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension));
					if (NT_ERROR (ntStatus)) {
						USBCAMD_KdPrint (MIN_TRACE, ("USBCAMD: Cam Driver Failed to save its state\n"));
					}
					*/
				}
				
				ntStatus = STATUS_SUCCESS;
				break;
			default:
					ntStatus = STATUS_INVALID_PARAMETER;
					break;
        }
		DeviceExtension->CurrentPowerState = DevicePowerState;
    }

    USBCAMD_KdPrint (MIN_TRACE, ("'exit USBCAMD_SetDevicePowerState 0x%x\n", ntStatus));
    return ntStatus;
}

