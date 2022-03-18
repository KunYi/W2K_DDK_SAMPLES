/*++


Module Name:

    IntBulk.c

Abstract:
        
    this module handle all interfaces to bulk & interrupt pipes 
    and performs read and write operations on these pipes.

Author:

    3/9/98 Husni Roukbi

Environment:

    kernel mode only

Notes:

  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
  PURPOSE.

  Copyright (c) 1998  Microsoft Corporation

Revision History:

--*/

#include "usbcamd.h"


/*++

Routine Description:

    This routine performs a read or write operation on a specified 
    bulk pipe. 

Arguments:

    DeviceContext - 

    PipeIndex - 

    Buffer - 

    BufferLength - 

    CommandComplete -

    CommandContext -


Return Value:

    NT status code

--*/

NTSTATUS
USBCAMD_BulkReadWrite( 
    IN PVOID DeviceContext,
    IN USHORT PipeIndex,
    IN PVOID Buffer,
    IN ULONG BufferLength,
    IN PCOMMAND_COMPLETE_FUNCTION CommandComplete,
    IN PVOID CommandContext
    )
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
   
    PUSBCAMD_DEVICE_EXTENSION deviceExtension;
    PUSBD_PIPE_INFORMATION pipeHandle ;
    PEVENTWAIT_WORKITEM workitem;
	PLIST_ENTRY listEntry =NULL;
    ULONG i;

    deviceExtension = USBCAMD_GET_DEVICE_EXTENSION(DeviceContext);


    USBCAMD_KdPrint ( MAX_TRACE, ("Enter USBCAMD_BulkReadWrite\n"));


    //
    // check if port is still connected.
    //
    if (deviceExtension ->CameraUnplugged ) {
        USBCAMD_KdPrint(MAX_TRACE,("Bulk Read/Write request after device removed!\n"));
        ntStatus = STATUS_FILE_CLOSED;        
        return ntStatus;        
    }
  
    //
    // do some parameter validation.
    //

    if (PipeIndex > deviceExtension->Interface->NumberOfPipes) {
        
        USBCAMD_KdPrint(MAX_TRACE,("BulkReadWrite invalid pipe index!\n"));
        ntStatus = STATUS_INVALID_PARAMETER;        
        return ntStatus;        
    }

    // check if we have a pending read or write already. 
    // we only accept one read and one write at atime.

    if (USBCAMD_OutstandingIrp(deviceExtension, PipeIndex) ) {
        USBCAMD_KdPrint(MAX_TRACE,("Bulk Read/Write Ovelapping request !\n"));
        ntStatus = STATUS_INVALID_PARAMETER;        
        return ntStatus;            
    }
    
    
    pipeHandle = &deviceExtension->Interface->Pipes[PipeIndex];

    if (pipeHandle->PipeType != UsbdPipeTypeBulk ) {
     
        USBCAMD_KdPrint(MAX_TRACE,("BulkReadWrite invalid pipe type!\n"));
        ntStatus = STATUS_INVALID_PARAMETER;        
        return ntStatus;        
    }

	if ( Buffer == NULL ) {
        USBCAMD_KdPrint(MIN_TRACE,("BulkReadWrite NULL buffer pointer!\n"));
        ntStatus = STATUS_INVALID_PARAMETER;        
        return ntStatus;        
    }

    
    //  
    // call the transfer function
    //

    if (KeGetCurrentIrql() < DISPATCH_LEVEL) {
        //
        // we are at passive level, just do the command
        //
        ntStatus = USBCAMD_IntOrBulkTransfer(deviceExtension,
                                             NULL,
                                             Buffer,
                                             BufferLength,
                                             PipeIndex,
                                             CommandComplete,
                                             CommandContext,
                                             0,
                                             BULK_TRANSFER);        
    } else {

//        TEST_TRAP();
        //
        // schedule a work item
        //
        ntStatus = STATUS_PENDING;

        workitem = USBCAMD_ExAllocatePool(NonPagedPool,
                                          sizeof(EVENTWAIT_WORKITEM));
        if (workitem) {
        
            ExInitializeWorkItem(&workitem->WorkItem,
                                 USBCAMD_EventWaitWorkItem,
                                 workitem);

            workitem->DeviceExtension = deviceExtension;
            workitem->ChannelExtension = NULL;
            workitem->PipeIndex = PipeIndex;
            workitem->Buffer = Buffer;
            workitem->BufferLength = BufferLength;
            workitem->EventComplete = CommandComplete;
            workitem->EventContext = CommandContext;
            workitem->LoopBack = 0;
            workitem->TransferType = BULK_TRANSFER;

            ExQueueWorkItem(&workitem->WorkItem,
                            DelayedWorkQueue);
   
        } else {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    
    return ntStatus;
}

/*++

Routine Description:

    This routine performs a read from an interrupt pipe. 

Arguments:

    DeviceContext - 

    PipeIndex - 

    Buffer - 

    BufferLength - 

    EventComplete -

    EventContext -


Return Value:

    NT status code

--*/

NTSTATUS
USBCAMD_WaitOnDeviceEvent( 
    IN PVOID DeviceContext,
    IN ULONG PipeIndex,
    IN PVOID Buffer,
    IN ULONG BufferLength,
    IN PCOMMAND_COMPLETE_FUNCTION   EventComplete,
    IN PVOID EventContext,
    IN BOOLEAN LoopBack
    )
{
    PUSBCAMD_DEVICE_EXTENSION deviceExtension;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PUSBD_PIPE_INFORMATION pipeHandle ;
    PEVENTWAIT_WORKITEM workitem;

    deviceExtension = USBCAMD_GET_DEVICE_EXTENSION(DeviceContext);


    USBCAMD_KdPrint ( MIN_TRACE, ("Enter USBCAMD_WaitOnDeviceEvent\n"));
   
    //
    // check if port is still connected.
    //

    if (deviceExtension->CameraUnplugged ) {
        USBCAMD_KdPrint(MAX_TRACE,("WaitOnDeviceEvent after device removed!\n"));
        ntStatus = STATUS_FILE_CLOSED;        
        return ntStatus;        
    }

    //
    // do some parameter validation.
    //

    if (PipeIndex > deviceExtension->Interface->NumberOfPipes) {
        
        USBCAMD_KdPrint(MIN_TRACE,("WaitOnDeviceEvent invalid pipe index!\n"));
        ntStatus = STATUS_INVALID_PARAMETER;        
        return ntStatus;        
    }
    
    // check if we have a pending interrupt request already. 
    // we only accept one interrupt request at atime.

    if (USBCAMD_OutstandingIrp(deviceExtension, PipeIndex) ) {
        USBCAMD_KdPrint(MIN_TRACE,("Ovelapping Interrupt request !\n"));
        ntStatus = STATUS_INVALID_PARAMETER;        
        return ntStatus;            
    }
   
    pipeHandle = &deviceExtension->Interface->Pipes[PipeIndex];

    if (pipeHandle->PipeType != UsbdPipeTypeInterrupt ) {
     
        USBCAMD_KdPrint(MIN_TRACE,("WaitOnDeviceEvent invalid pipe type!\n"));
        ntStatus = STATUS_INVALID_PARAMETER;        
        return ntStatus;        
    }

    if ( Buffer == NULL ) {
        USBCAMD_KdPrint(MIN_TRACE,("WaitOnDeviceEvent NULL buffer pointer!\n"));
        ntStatus = STATUS_INVALID_PARAMETER;        
        return ntStatus;        
    }

    if ( ( pipeHandle->PipeType == UsbdPipeTypeInterrupt) && 
         ( BufferLength < (ULONG) pipeHandle->MaximumPacketSize ) ) {
        USBCAMD_KdPrint(MIN_TRACE,("WaitOnDeviceEvent buffer is smaller than max. pkt size!\n"));
        ntStatus = STATUS_INVALID_PARAMETER;        
        return ntStatus;        
    }
   
    //  
    // call the transfer function
    //

    if (KeGetCurrentIrql() < DISPATCH_LEVEL) {
        //
        // we are at passive level, just do the command
        //
        ntStatus = USBCAMD_IntOrBulkTransfer(deviceExtension,
                                             NULL,
                                             Buffer,
                                             BufferLength,
                                             PipeIndex,
                                             EventComplete,
                                             EventContext,
                                             LoopBack,
                                             INTERRUPT_TRANSFER);        
    } else {

        //
        // schedule a work item
        //
        ntStatus = STATUS_PENDING;

        workitem = USBCAMD_ExAllocatePool(NonPagedPool,sizeof(EVENTWAIT_WORKITEM));
        if (workitem) {
        
            ExInitializeWorkItem(&workitem->WorkItem,
                                 USBCAMD_EventWaitWorkItem,
                                 workitem);

            workitem->DeviceExtension = deviceExtension;
            workitem->ChannelExtension = NULL;
            workitem->PipeIndex = PipeIndex;
            workitem->Buffer = Buffer;
            workitem->BufferLength = BufferLength;
            workitem->EventComplete = EventComplete;
            workitem->EventContext = EventContext; 
            workitem->LoopBack = LoopBack;
            workitem->TransferType = INTERRUPT_TRANSFER;

            ExQueueWorkItem(&workitem->WorkItem,DelayedWorkQueue);
   
        } else {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    return ntStatus;
}

/*++

Routine Description:


Arguments:

Return Value:

    None.

--*/
VOID
USBCAMD_EventWaitWorkItem(
    PVOID Context
    )
{
    NTSTATUS ntStatus;
    PEVENTWAIT_WORKITEM workItem = Context;
    ntStatus = USBCAMD_IntOrBulkTransfer(workItem->DeviceExtension,
                                         workItem->ChannelExtension,
                                         workItem->Buffer,
                                         workItem->BufferLength,
                                         workItem->PipeIndex,
                                         workItem->EventComplete,
                                         workItem->EventContext,
                                         workItem->LoopBack,
                                         workItem->TransferType);
    USBCAMD_ExFreePool(workItem);
}


/*++

Routine Description:

Arguments:

Return Value:
    NT Status - STATUS_SUCCESS

--*/

NTSTATUS
USBCAMD_IntOrBulkTransfer(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension,
    IN PVOID    pBuffer,        
    IN ULONG    TransferSize,
    IN ULONG    PipeIndex,
    IN PCOMMAND_COMPLETE_FUNCTION commandComplete,
    IN PVOID    commandContext,
    IN BOOLEAN  LoopBack,
    IN UCHAR    TransferType
)
{
    NTSTATUS                    ntStatus ;
    PUSBCAMD_TRANSFER_EXTENSION pTransferContext;
    ULONG                       siz = 0;
    ULONG                       MaxPacketSize;
    ULONG                       MaxTransferSize;


    USBCAMD_KdPrint(MAX_TRACE,("Bulk transfer called. size = %d, pBuffer = 0x%X\n",
                                TransferSize, pBuffer));

    MaxTransferSize = DeviceExtension->Interface->Pipes[PipeIndex].MaximumTransferSize;
    MaxPacketSize   = DeviceExtension->Interface->Pipes[PipeIndex].MaximumPacketSize;

    if ( TransferSize > MaxTransferSize) {
        USBCAMD_KdPrint(MIN_TRACE,("Bulk Transfer > Max transfer size.\n"));
    }

    //
    // Allocate and initialize Transfer Context
    //
    
    if ( ChannelExtension == NULL ) {

        pTransferContext = USBCAMD_ExAllocatePool(NonPagedPool, sizeof(USBCAMD_TRANSFER_EXTENSION));

     	if (pTransferContext) {
     		RtlZeroMemory(pTransferContext, sizeof(USBCAMD_TRANSFER_EXTENSION));  
     		ntStatus = USBCAMD_InitializeBulkTransfer(DeviceExtension,
            	                                  ChannelExtension,
                	                              DeviceExtension->Interface,
                    	                          pTransferContext,
                        	                      PipeIndex);
        }
        else {
         	USBCAMD_KdPrint(MIN_TRACE,(" cannot allocate Transfer Context\n"));
        	ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        	return ntStatus;
        }
        
      	if (ntStatus != STATUS_SUCCESS) {
        	USBCAMD_ExFreePool(pTransferContext);
        	ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        	return ntStatus;
        }     
   	}
    else {
        pTransferContext = &ChannelExtension->TransferExtension[ChannelExtension->CurrentBulkTransferIndex];
    }
    
    ASSERT(pTransferContext);

    pTransferContext->BulkContext.fDestinedForReadBuffer = FALSE;
    pTransferContext->BulkContext.RemainingTransferLength = TransferSize;
    pTransferContext->BulkContext.ChunkSize = TransferSize;
    pTransferContext->BulkContext.PipeIndex = PipeIndex;
    pTransferContext->BulkContext.pTransferBuffer = pBuffer;
    pTransferContext->BulkContext.pOriginalTransferBuffer = pBuffer;
    pTransferContext->BulkContext.CommandCompleteCallback = commandComplete;
    pTransferContext->BulkContext.CommandCompleteContext = commandContext;
    pTransferContext->BulkContext.LoopBack = LoopBack;
    pTransferContext->BulkContext.TransferType = TransferType;
	pTransferContext->BulkContext.NBytesTransferred = 0;

   
    //
    // If chunksize is bigger than MaxTransferSize, then set it to MaxTransferSize.  The
    // transfer completion routine will issue additional transfers until the total size has
    // been transferred.
    // 

    if (pTransferContext->BulkContext.ChunkSize > MaxTransferSize) {
        pTransferContext->BulkContext.ChunkSize = MaxTransferSize;
    }

    if  (DeviceExtension->PipePinRelations[PipeIndex].PipeDirection == INPUT_PIPE){

        //
        // If this read is smaller than a USB packet, then issue a request for a 
        // whole usb packet and make sure it goes into the read buffer first.
        //

        if (pTransferContext->BulkContext.ChunkSize < MaxPacketSize) {
            USBCAMD_KdPrint(MAX_TRACE,("Request is < packet size - transferring whole packet into read buffer.\n"));
            pTransferContext->BulkContext.fDestinedForReadBuffer = TRUE;
            pTransferContext->BulkContext.pOriginalTransferBuffer = 
                pTransferContext->BulkContext.pTransferBuffer;  // save off original transfer ptr.
            pTransferContext->BulkContext.pTransferBuffer = pTransferContext->WorkBuffer =
            		USBCAMD_ExAllocatePool(NonPagedPool,MaxPacketSize); 
            if (pTransferContext->WorkBuffer == NULL ) {
    			if ( ChannelExtension == NULL) 
        			USBCAMD_ExFreePool(pTransferContext);
        		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        		return ntStatus;
            }
            pTransferContext->BulkContext.ChunkSize = MaxPacketSize;
        }

        //
        // Truncate the size of the read to an integer number of packets.  If necessary, 
        // the completion routine will handle any fractional remaining packets (with the read buffer).
        //         
        pTransferContext->BulkContext.ChunkSize = (pTransferContext->BulkContext.ChunkSize 
                                                        / MaxPacketSize) * MaxPacketSize;
    }

    ASSERT(pTransferContext->BulkContext.RemainingTransferLength);
    ASSERT(pTransferContext->BulkContext.pTransferBuffer);    
    ASSERT(pTransferContext->DataUrb);

    //
    // Initialize URB
    //

    UsbBuildInterruptOrBulkTransferRequest(pTransferContext->DataUrb,
                                           sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                                           DeviceExtension->Interface->Pipes[PipeIndex].PipeHandle,
                                           pTransferContext->BulkContext.pTransferBuffer,
                                           NULL,
                                           pTransferContext->BulkContext.ChunkSize,
                                           USBD_SHORT_TRANSFER_OK,
                                           NULL);

    //
    // Setup stack location for lower driver
    //
    pTransferContext->Pending = 1;
    USBCAMD_RecycleIrp(pTransferContext, 
                       pTransferContext->DataIrp,
                       pTransferContext->DataUrb,
                       USBCAMD_BulkTransferComplete);
    
  
    // queue IRP in pending queue.

    USBCAMD_QueueIrp(DeviceExtension,
                                  PipeIndex,
                                  pTransferContext,
                                  &DeviceExtension->PipePinRelations[PipeIndex].IrpPendingQueue);

    ntStatus = IoCallDriver(DeviceExtension->StackDeviceObject, pTransferContext->DataIrp);

    //
    // handle errors returned from submitting requests
    //

    if (ntStatus != STATUS_PENDING) {
        // stream error if we get an immediate error
        if ( ChannelExtension != NULL ) {
            ChannelExtension->StreamError = TRUE;
            TEST_TRAP();
        }
        // we have an error on the submission set the stream error flag
        // and exit.
        //
        // Note the completion routine will handle cleanup
    } 
    	
    USBCAMD_KdPrint(MAX_TRACE,("USBCAMD_IntOrBulkTransfer exit (0x%X).\n", ntStatus));
        
	return STATUS_SUCCESS;
}

/*++

Routine Description:

Arguments:
    DeviceExtension    - Pointer to Device Extension.
    PipeIndex       - Pipe index.
    Irp             - ptr to Read/write request packet

Return Value:
    NT Status - STATUS_SUCCESS
    
--*/

VOID
USBCAMD_QueueIrp(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN ULONG    PipeIndex,
    IN PUSBCAMD_TRANSFER_EXTENSION pTransferExt,
    PLIST_ENTRY pListHead
    )
{

    KIRQL Irql;
    
    KeAcquireSpinLock(&DeviceExtension->PipePinRelations[PipeIndex].OutstandingIrpSpinlock, &Irql);
    if (pTransferExt)
		InsertTailList(pListHead,&pTransferExt->ListEntry);

    KeReleaseSpinLock(&DeviceExtension->PipePinRelations[PipeIndex].OutstandingIrpSpinlock, Irql);

}    


/*++

Routine Description:

Arguments:
    DeviceExtension    - Pointer to Device Extension.
    PipeIndex       - Pipe index.

Return Value:
    NT Status - STATUS_SUCCESS
    
--*/

PUSBCAMD_TRANSFER_EXTENSION
USBCAMD_DequeueIrp(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN ULONG    PipeIndex,
    IN PUSBCAMD_TRANSFER_EXTENSION pTransToDequeue,
    IN PLIST_ENTRY pListHead)
{

    KIRQL Irql;
    PLIST_ENTRY pListEntry;
    PUSBCAMD_TRANSFER_EXTENSION pTransferExt ;
    BOOLEAN bFoundIrp = FALSE;

   	KeAcquireSpinLock(&DeviceExtension->PipePinRelations[PipeIndex].OutstandingIrpSpinlock, &Irql);

	ASSERT (pTransToDequeue  );

    //
    // See if we have this Irp in the pending Q.
    //    

    for(pListEntry = pListHead->Flink; pListEntry != pListHead; pListEntry = pListEntry->Flink) {
        
        pTransferExt = (PUSBCAMD_TRANSFER_EXTENSION) CONTAINING_RECORD(pListEntry, 
        															USBCAMD_TRANSFER_EXTENSION, 
        															ListEntry);

		ASSERT_TRANSFER(pTransferExt);
        if(pTransferExt == pTransToDequeue)  { // found it.
        	bFoundIrp = TRUE;
            break;
        }
    }

  	if (bFoundIrp) {
	    RemoveEntryList(pListEntry);
	} 
	else {
		pTransferExt = NULL;
	}
   
    KeReleaseSpinLock(&DeviceExtension->PipePinRelations[PipeIndex].OutstandingIrpSpinlock, Irql);
	return pTransferExt;
}    

/*++

Routine Description:

Arguments:
    DeviceExtension    - Pointer to Device Extension.
    PipeIndex       - Pipe index.

Return Value:
    NT Status - STATUS_SUCCESS
    
--*/

PUSBCAMD_TRANSFER_EXTENSION
USBCAMD_DequeueFirstIrp(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN ULONG    PipeIndex,
    IN PLIST_ENTRY pListHead)
{

    KIRQL Irql;
    PLIST_ENTRY pListEntry;
    PUSBCAMD_TRANSFER_EXTENSION pTransExt ;

   	KeAcquireSpinLock(&DeviceExtension->PipePinRelations[PipeIndex].OutstandingIrpSpinlock, &Irql);

   	if ( IsListEmpty(pListHead)) 
       	pTransExt = NULL;
   	else {
    	pListEntry = RemoveHeadList(pListHead);	
        pTransExt = (PUSBCAMD_TRANSFER_EXTENSION) CONTAINING_RECORD(pListEntry, 
        						USBCAMD_TRANSFER_EXTENSION, ListEntry);   
        ASSERT_TRANSFER(pTransExt);
	}
   
    KeReleaseSpinLock(&DeviceExtension->PipePinRelations[PipeIndex].OutstandingIrpSpinlock, Irql);
	return pTransExt;
}    


/*++

Routine Description: retrieves the first queued Irp and
			its ListEntry w/o removeing it from Q.
		this fcn used for IrpPending Q only.
Arguments:
    DeviceExtension    - Pointer to Device Extension.
    PipeIndex       - Pipe index.

Return Value:
    PUSBCAMD_TRANSFER_EXTENSION or NULL.
    
--*/

PUSBCAMD_TRANSFER_EXTENSION
USBCAMD_GetFirstQueuedIrp(
    PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    ULONG    PipeIndex
    )
{

    KIRQL Irql;
    PUSBCAMD_TRANSFER_EXTENSION pTransExt;
	PLIST_ENTRY pListEntry,pListHead;

	pListHead = &DeviceExtension->PipePinRelations[PipeIndex].IrpPendingQueue;

   	KeAcquireSpinLock(&DeviceExtension->PipePinRelations[PipeIndex].OutstandingIrpSpinlock, &Irql);

    if ( IsListEmpty(pListHead)) 
        pTransExt = NULL;
    else {
    	pListEntry = pListHead->Flink;
    	pTransExt = (PUSBCAMD_TRANSFER_EXTENSION) CONTAINING_RECORD(pListEntry, 
    						USBCAMD_TRANSFER_EXTENSION, 
    						ListEntry);
    	ASSERT_TRANSFER(pTransExt);
	}
    KeReleaseSpinLock(&DeviceExtension->PipePinRelations[PipeIndex].OutstandingIrpSpinlock, Irql);
    return pTransExt;
}    


/*++

Routine Description:
    
      Returns a count of queued IRPs in the designated Q.

Arguments:
    DeviceExtension    - Pointer to Device Extension.
    PipeIndex       - Pipe index.
    Irp             - Read/write request packet

Return Value:
    ULONG Irp counr
    
--*/

ULONG 
USBCAMD_GetQueuedIrpCount(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN ULONG    PipeIndex,
    IN PLIST_ENTRY pListHead
)
{
    ULONG ulCount=0;
    PLIST_ENTRY pListEntry;
    KIRQL Irql;

   	KeAcquireSpinLock(&DeviceExtension->PipePinRelations[PipeIndex].OutstandingIrpSpinlock, &Irql);

    if (IsListEmpty(pListHead)) {
        ulCount = 0;
    }
    else{
		pListEntry  = pListHead->Flink;
        ulCount = 0;
        do {
			ulCount++;
			pListEntry = pListEntry->Flink;
		} while  (pListEntry != pListHead);
	}
    KeReleaseSpinLock(&DeviceExtension->PipePinRelations[PipeIndex].OutstandingIrpSpinlock, Irql);
	return ulCount;
}



/*++

Routine Description:

Arguments:
    DeviceExtension    - Pointer to Device Extension.
    PipeIndex       - Pipe index.

Return Value:
    NT Status - STATUS_SUCCESS
    
--*/

BOOLEAN
USBCAMD_OutstandingIrp(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN ULONG    PipeIndex)
{

    KIRQL Irql;
    BOOLEAN Pending = FALSE;
    PLIST_ENTRY pListHead; 

   	KeAcquireSpinLock(&DeviceExtension->PipePinRelations[PipeIndex].OutstandingIrpSpinlock, &Irql);

    pListHead = &DeviceExtension->PipePinRelations[PipeIndex].IrpPendingQueue;
    Pending = IsListEmpty(pListHead);

    KeReleaseSpinLock(&DeviceExtension->PipePinRelations[PipeIndex].OutstandingIrpSpinlock, Irql);

    return (!Pending);
}    

NTSTATUS
USBCAMD_BulkTransferComplete(
    IN PDEVICE_OBJECT   pDeviceObject,
	IN PIRP             pIrp,
	IN PVOID            Context
)
/*++

Routine Description:

Arguments:
    pDeviceObject    - Device object for a device.
    pIrp             - Read/write request packet
    pTransferContext - context info for transfer

Return Value:
    NT Status - STATUS_SUCCESS
    
--*/
{
    PURB                        pUrb;
    ULONG                       CompletedTransferLength;
    NTSTATUS                    CompletedTransferStatus;
    ULONG                       MaxPacketSize,PipeIndex;
    PUSBCAMD_TRANSFER_EXTENSION pTransferContext, pQueTransfer;
    PUSBCAMD_CHANNEL_EXTENSION channelExtension;
    PUSBCAMD_DEVICE_EXTENSION deviceExtension;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN                     fShortTransfer = FALSE;
    PLIST_ENTRY listEntry;

    USBCAMD_KdPrint (ULTRA_TRACE, ("enter USBCAMD_BulkTransferComplete \n"));
   
    pTransferContext = Context;
    channelExtension = pTransferContext->ChannelExtension;
    deviceExtension = pTransferContext->DeviceExtension;
    PipeIndex = pTransferContext->BulkContext.PipeIndex;
    ASSERT_TRANSFER(pTransferContext);

    //
    // one less irp pending
    //
    pTransferContext->Pending--;

    // reset outstanding IRP in device extension.

    pQueTransfer = USBCAMD_DequeueIrp(deviceExtension,
                       				PipeIndex,
                       				pTransferContext,
                       &deviceExtension->PipePinRelations[PipeIndex].IrpPendingQueue);

	ASSERT ( pQueTransfer == pTransferContext);

    if ( channelExtension && (channelExtension->ImageCaptureStarted == FALSE)) {
        //
        // we've hit the first completion routine before 
        // the stream is started.  
        // This means the system is really screwed up -- or
        // is running with excessive debug spew.
        // We raise a stream error here so that we won't try 
        // to re-submit.
        //
        //TEST_TRAP();
        channelExtension->StreamError = TRUE;
		return STATUS_MORE_PROCESSING_REQUIRED;
    }

    if (channelExtension && (channelExtension->Flags & USBCAMD_STOP_STREAM)) {
    	// we need to stop, and let the DPC routine handle clean up.
    	return STATUS_MORE_PROCESSING_REQUIRED;
    }
	
    MaxPacketSize =  deviceExtension->Interface->Pipes[PipeIndex].MaximumPacketSize;    

    pUrb = pTransferContext->DataUrb;
    CompletedTransferLength = pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength;
    CompletedTransferStatus = pUrb->UrbBulkOrInterruptTransfer.Hdr.Status;
    
    if (STATUS_SUCCESS == CompletedTransferStatus) {

        if (CompletedTransferLength < pTransferContext->BulkContext.ChunkSize) {
            USBCAMD_KdPrint(MAX_TRACE,("Short bulk transfer received. Length = %d, ChunkSize = %d\n",
                                       CompletedTransferLength, pTransferContext->BulkContext.ChunkSize));
            fShortTransfer = TRUE;
        }
        
        //
        // If this transfer went into the read buffer, then this should be the final read 
        // of  a single very small read (< single usb packet).
        // In either case, we need to copy the appropriate amount of data into the user's irp, update the
        // read buffer variables, and complete the user's irp.
        //

        if (pTransferContext->BulkContext.fDestinedForReadBuffer) {
            USBCAMD_KdPrint(MAX_TRACE,("Read bulk buffer transfer completed. size = %d\n", CompletedTransferLength));
            ASSERT(CompletedTransferLength <= MaxPacketSize);
            ASSERT(pTransferContext->BulkContext.pOriginalTransferBuffer);
            ASSERT(pTransferContext->BulkContext.pTransferBuffer);
            ASSERT(pTransferContext->WorkBuffer == pTransferContext->BulkContext.pTransferBuffer);
            ASSERT(pTransferContext->BulkContext.RemainingTransferLength < MaxPacketSize);

            ASSERT(CompletedTransferLength < MaxPacketSize);            
            RtlCopyMemory(pTransferContext->BulkContext.pOriginalTransferBuffer,
                          pTransferContext->WorkBuffer,
                          CompletedTransferLength);
            pTransferContext->BulkContext.pTransferBuffer = 
                pTransferContext->BulkContext.pOriginalTransferBuffer;            
        }

        //
        // Update the number of bytes transferred, remaining bytes to transfer 
        // and advance the transfer buffer pointer appropriately.
        //

        pTransferContext->BulkContext.NBytesTransferred += CompletedTransferLength;
        pTransferContext->BulkContext.pTransferBuffer += CompletedTransferLength;
        pTransferContext->BulkContext.RemainingTransferLength -= CompletedTransferLength;

        //
        // If there is still data to transfer and the previous transfer was NOT a
        // short transfer, then issue another request to move the next chunk of data.
        //
        
        if (pTransferContext->BulkContext.RemainingTransferLength > 0) {
            if (!fShortTransfer) {

                USBCAMD_KdPrint(MAX_TRACE,("Queuing next chunk. RemainingSize = %d, pBuffer = 0x%x\n",
                                           pTransferContext->BulkContext.RemainingTransferLength,
                                           pTransferContext->BulkContext.pTransferBuffer));

                if (pTransferContext->BulkContext.RemainingTransferLength < pTransferContext->BulkContext.ChunkSize) {
                    pTransferContext->BulkContext.ChunkSize = pTransferContext->BulkContext.RemainingTransferLength;
                }

                //
                // Reinitialize URB
                //
                // If the next transfer is < than 1 packet, change it's destination to be
                // the read buffer.  When this transfer completes, the appropriate amount of data will be
                // copied out of the read buffer and into the user's irp.  
                //

                if  (deviceExtension->PipePinRelations[PipeIndex].PipeDirection == INPUT_PIPE){
                    if (pTransferContext->BulkContext.ChunkSize < MaxPacketSize) {
                        pTransferContext->BulkContext.fDestinedForReadBuffer = TRUE;
                        pTransferContext->BulkContext.pOriginalTransferBuffer = pTransferContext->BulkContext.pTransferBuffer;
						if (pTransferContext->WorkBuffer)
							pTransferContext->BulkContext.pTransferBuffer = pTransferContext->WorkBuffer;
               			else {
							pTransferContext->BulkContext.pTransferBuffer = 
           			       	pTransferContext->WorkBuffer =
            							USBCAMD_ExAllocatePool(NonPagedPool,MaxPacketSize); 
            				if (pTransferContext->WorkBuffer == NULL ){
            					USBCAMD_KdPrint (MIN_TRACE, ("Error allocating bulk transfer work buffer. \n"));
        						return STATUS_MORE_PROCESSING_REQUIRED;
            				}
                        }	
                        pTransferContext->BulkContext.ChunkSize = MaxPacketSize;
                    }
                    pTransferContext->BulkContext.ChunkSize = (pTransferContext->BulkContext.ChunkSize / MaxPacketSize) * MaxPacketSize;
                }

                ASSERT(pTransferContext->BulkContext.ChunkSize >= MaxPacketSize);
                ASSERT(0 == pTransferContext->BulkContext.ChunkSize % MaxPacketSize);     
                
                UsbBuildInterruptOrBulkTransferRequest(pUrb,
                    sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                    deviceExtension->Interface->Pipes[PipeIndex].PipeHandle,
                    pTransferContext->BulkContext.pTransferBuffer,
                    NULL,
                    pTransferContext->BulkContext.ChunkSize,
                    USBD_SHORT_TRANSFER_OK,
                    NULL);

                pTransferContext->Pending = 1;
                USBCAMD_RecycleIrp(pTransferContext, 
                                   pTransferContext->DataIrp,
                                   pTransferContext->DataUrb,
                                   USBCAMD_BulkTransferComplete);

                // save outstanding IRP in device extension.

                USBCAMD_QueueIrp(deviceExtension,
                                 PipeIndex,
                                 pTransferContext,
                                 &deviceExtension->PipePinRelations[PipeIndex].IrpPendingQueue);

                ntStatus = IoCallDriver(deviceExtension->StackDeviceObject, pTransferContext->DataIrp);

                if (ntStatus != STATUS_PENDING) {
                    // stream error if we get an immediate error
                    if ( channelExtension != NULL ) {
                        channelExtension->StreamError = TRUE;
                    }
                } 
                return STATUS_MORE_PROCESSING_REQUIRED;               
            }
        }

        USBCAMD_KdPrint(MAX_TRACE,("Completing bulk transfer request. nbytes transferred = %d \n",
                                   pTransferContext->BulkContext.NBytesTransferred));        

        ntStatus = STATUS_MORE_PROCESSING_REQUIRED;
        //
        // we need to complete the read/write erequest.
        //
        
        if ( channelExtension == NULL ) {
            
            //
            // this is an external read/write request.
            //

            if (pTransferContext->BulkContext.CommandCompleteCallback) {
                // call the completion handler
                (*pTransferContext->BulkContext.CommandCompleteCallback)
                                    (USBCAMD_GET_DEVICE_CONTEXT(deviceExtension), 
                                     pTransferContext->BulkContext.CommandCompleteContext, 
                                     ntStatus);
            }   

            // notify STI mon if this was an interrupt event.
            if ( pTransferContext->BulkContext.TransferType == INTERRUPT_TRANSFER) {
                if (deviceExtension->CamControlFlag & USBCAMD_CamControlFlag_EnableDeviceEvents) {
                    USBCAMD_NotifyStiMonitor(deviceExtension);
                }
            }

            // check if we need to loop back.
            if ( pTransferContext->BulkContext.LoopBack ) {
                USBCAMD_RestoreOutstandingIrp(deviceExtension,
                                              PipeIndex, 
                                              pTransferContext);
                return ntStatus;
            }
                           
        }
        else {

            //
            // this is a stream class bulk read request on the video/still pin.
            //
            USBCAMD_CompleteBulkRead(channelExtension, CompletedTransferStatus);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

    } else {
       
        if (( USBD_STATUS_CANCELED == CompletedTransferStatus) ||
        	 ( pIrp->IoStatus.Status == STATUS_CANCELLED)) {
            
            // 
            // signal the cancel event if one is waiting
            //
            
            if (pTransferContext->BulkContext.Flags & USBCAMD_CANCEL_IRP) {
                
                pTransferContext->BulkContext.Flags &= ~USBCAMD_CANCEL_IRP;
                KeSetEvent(&pTransferContext->BulkContext.CancelEvent,1,FALSE);

                USBCAMD_KdPrint(MIN_TRACE,("**** Bulk transfer Irp Cancelled.\n"));
                                
                // return w/o freeing transfercontext. We will use later when we resubmit
                // the transfer again to USBD.

                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
        else {

    	        USBCAMD_KdPrint(MIN_TRACE,("Int/Bulk transfer error. USB status = 0x%X\n",CompletedTransferStatus));
	
				// set the stream error flag. Timeout routine will reset the port.
				// and start again.
				//TEST_TRAP();
				if ( channelExtension)
					channelExtension->StreamError = TRUE;    
        }
    }
    
    // free resources allocated for external requests.

    if ( channelExtension == NULL ) {
   		USBCAMD_FreeBulkTransfer(pTransferContext);
        USBCAMD_ExFreePool(pTransferContext);
    }
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
USBCAMD_InitializeBulkTransfer(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension,
    IN PUSBD_INTERFACE_INFORMATION InterfaceInformation,
    IN PUSBCAMD_TRANSFER_EXTENSION TransferExtension,
    IN ULONG PipeIndex
    )
/*++

Routine Description:

    Initializes a bulk or interrupt transfer.

Arguments:

    DeviceExtension - pointer to the device extension for this instance of the USB camera
                    devcice.

    ChannelExtension - extension specific to this video channel

    InterfaceInformation - pointer to USBD interface information structure 
        describing the currently active interface.

    TransferExtension - context information assocaited with this transfer set.        


Return Value:

    NT status code

--*/
{
    ULONG packetSize;
    NTSTATUS ntStatus = STATUS_SUCCESS;

    if ( ChannelExtension != NULL ) {
        ASSERT_CHANNEL(ChannelExtension);
    }
       
    USBCAMD_KdPrint (ULTRA_TRACE, ("enter USBCAMD_InitializeBulkTransfer\n"));

    //
    // allocate some contiguous memory for this request
    //

    TransferExtension->Sig = USBCAMD_TRANSFER_SIG;     
    TransferExtension->DeviceExtension = DeviceExtension;
    TransferExtension->ChannelExtension = ChannelExtension;
    TransferExtension->BulkContext.PipeIndex = PipeIndex;

    //
    // No pending transfers yet
    //
    TransferExtension->Pending = 0;                
    packetSize = InterfaceInformation->Pipes[PipeIndex].MaximumPacketSize;
    TransferExtension->SyncBuffer = NULL;

    //
    // Allocate and initialize URB
    //
    
    TransferExtension->DataUrb = USBCAMD_ExAllocatePool(NonPagedPool, 
                                                sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER));
    if (NULL == TransferExtension->DataUrb) {
        USBCAMD_KdPrint(MIN_TRACE,(" cannot allocated bulk URB\n"));
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        return ntStatus;
    }

	RtlZeroMemory(TransferExtension->DataUrb, sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER));

	TransferExtension->WorkBuffer = NULL;
	TransferExtension->SyncIrp = (PIRP)-1;

    //
    // Build the data request
    //

    TransferExtension->DataIrp = USBCAMD_BuildIoRequest(DeviceExtension,
                                  TransferExtension,
                                  TransferExtension->DataUrb,
                                  USBCAMD_BulkTransferComplete);
    if (TransferExtension->DataIrp == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
	}
    USBCAMD_KdPrint (MAX_TRACE, ("exit USBCAMD_InitializeBulkTransfer 0x%x\n", ntStatus));

    return ntStatus;
}

NTSTATUS
USBCAMD_FreeBulkTransfer(
    IN PUSBCAMD_TRANSFER_EXTENSION TransferExtension
    )
/*++

Routine Description:

    Opposite of USBCAMD_InitializeBulkTransfer, frees resources allocated for an 
    iso transfer.

Arguments:


    TransferExtension - context information for this transfer (pair of iso 
        urbs).

Return Value:

    NT status code

--*/
{
    ASSERT_TRANSFER(TransferExtension);
  
    USBCAMD_KdPrint (MAX_TRACE, ("Free Bulk Transfer\n"));
    //
    // first free the Irps
    //
    
    if (TransferExtension->DataIrp != NULL) {
        IoFreeIrp(TransferExtension->DataIrp);
    }

	if (TransferExtension->WorkBuffer) {
		USBCAMD_ExFreePool(TransferExtension->WorkBuffer);
		TransferExtension->WorkBuffer = NULL;
	}
	
    if (TransferExtension->DataUrb) {
        USBCAMD_ExFreePool(TransferExtension->DataUrb);
        TransferExtension->DataUrb = NULL;
    }

    return STATUS_SUCCESS;
}

VOID
USBCAMD_CompleteBulkRead(
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension,
    IN NTSTATUS ntStatus
    )
/*++

Routine Description:

    This routine completes the bnulk read/write request for the video/still pin

Arguments:

Return Value:

--*/    
{
    PUSBCAMD_WORK_ITEM usbWorkItem;

    // 
	// we increment capture frame counter in ch ext. regardles of read srb
	// availability
	ChannelExtension->FrameCaptured++;  

    //
    // Queue a work item for this Irp
    //

    usbWorkItem = USBCAMD_ExAllocatePool(NonPagedPool, sizeof(*usbWorkItem));
    if (usbWorkItem) {
        ExInitializeWorkItem(&usbWorkItem->WorkItem,
                             USBCAMD_ProcessStillReadWorkItem,
                             usbWorkItem);

        usbWorkItem->Request = NULL;       
        usbWorkItem->ChannelExtension = ChannelExtension;
        usbWorkItem->status = ntStatus;
        ExQueueWorkItem(&usbWorkItem->WorkItem,
                        DelayedWorkQueue);

    } 
    else
    	TEST_TRAP();
}

//
// code to handle packet processing outside the DPC routine
//

VOID
USBCAMD_ProcessStillReadWorkItem(
    PVOID Context
    )
/*++

Routine Description:

    Call the mini driver to convert a raw still frame to the proper format.

Arguments:

Return Value:

    None.

--*/
{
    PUSBCAMD_WORK_ITEM usbWorkItem = Context;
    PVOID frameBuffer;
    ULONG maxLength,i;
    PUSBCAMD_CHANNEL_EXTENSION channelExtension;    
    PUSBCAMD_READ_EXTENSION readExtension;
    PUSBCAMD_DEVICE_EXTENSION deviceExtension;
    ULONG bytesTransferred, index;
    NTSTATUS status;
    PHW_STREAM_REQUEST_BLOCK srb;
    PKSSTREAM_HEADER dataPacket;
    PUSBCAMD_TRANSFER_EXTENSION pTransferContext;
    PLIST_ENTRY listEntry;
	LARGE_INTEGER DelayTime = {(ULONG)(-5 * 1000 * 10), -1};

    status = usbWorkItem->status;
	channelExtension = usbWorkItem->ChannelExtension;
    ASSERT_CHANNEL(channelExtension);

    
    pTransferContext = &channelExtension->TransferExtension[channelExtension->CurrentBulkTransferIndex];  
    //
    // DSHOW buffer len returned will be equal raw frame len unless we 
    // process raw frame buffer in ring 0.
    //
    bytesTransferred = pTransferContext->BulkContext.NBytesTransferred;
    deviceExtension = channelExtension->DeviceExtension;

	//
    // get a pending read srb
    //

    for ( i=0; i < 2; i++) {
	    listEntry =  ExInterlockedRemoveHeadList( &(channelExtension->PendingIoList),
                                             &channelExtension->PendingIoListSpin);
		if (listEntry )
			break;

		USBCAMD_KdPrint (MIN_TRACE, ("No Read Srbs available. Delay excution \n"));

        KeDelayExecutionThread(KernelMode,FALSE,&DelayTime);
    }   
    
  	if ( listEntry ) { // chk if no more read SRBs in Q. 

		readExtension = (PUSBCAMD_READ_EXTENSION) CONTAINING_RECORD(listEntry, 
                        			                 USBCAMD_READ_EXTENSION, 
                                    			     ListEntry);       

	    ASSERT_READ(readExtension);

		readExtension->FrameNumber = pTransferContext->ChannelExtension->FrameCaptured;
		
	    // Let client driver initiate the SRB extension.
       	
       	(*deviceExtension->DeviceDataEx.DeviceData2.CamNewVideoFrameEx)
                                       (USBCAMD_GET_DEVICE_CONTEXT(deviceExtension),
                                        USBCAMD_GET_FRAME_CONTEXT(readExtension),
                                        channelExtension->StreamNumber,
                                        &readExtension->ActualRawFrameLen);
		

    	srb = readExtension->Srb;
   		dataPacket = srb->CommandData.DataBufferArray;
		dataPacket->OptionsFlags =0;    

    	if ((status == STATUS_SUCCESS) && (!channelExtension->NoRawProcessingRequired)) {

        	frameBuffer = USBCAMD_GetFrameBufferFromSrb(readExtension->Srb,&maxLength);

        	status = 
            	(*deviceExtension->DeviceDataEx.DeviceData2.CamProcessRawVideoFrameEx)(
                	deviceExtension->StackDeviceObject,
                 	USBCAMD_GET_DEVICE_CONTEXT(deviceExtension),
                 	USBCAMD_GET_FRAME_CONTEXT(readExtension),
                 	frameBuffer,
                 	maxLength,
                 	pTransferContext->DataBuffer,
                 	readExtension->RawFrameLength,
                 	0,
                 	&bytesTransferred,
                 	pTransferContext->BulkContext.NBytesTransferred,
                 	srb->StreamObject->StreamNumber);                	
    	}

    	USBCAMD_KdPrint (MAX_TRACE, ("CamProcessRawframeEx Completed, length = %d status = 0x%X \n",
        	                            bytesTransferred,status));

		// The number of bytes transfer of the read is set above just before
		// USBCAMD_CompleteReadRequest is called.

    	USBCAMD_CompleteRead(channelExtension,readExtension,status,bytesTransferred); 
    	channelExtension->CurrentRequest = NULL;
	}
	else {
	    	USBCAMD_KdPrint (MIN_TRACE, ("Dropping Video Frame.\n"));
#if DBG
	    	pTransferContext->ChannelExtension->VideoFrameLostCount++;
#endif

	    	
	    	// and send a note to the camera driver about the cancellation.
            // send a CamProcessrawFrameEx with null buffer ptr.
            if ( !channelExtension->NoRawProcessingRequired) {

            	status = 
                        (*deviceExtension->DeviceDataEx.DeviceData2.CamProcessRawVideoFrameEx)(
                             deviceExtension->StackDeviceObject,
                             USBCAMD_GET_DEVICE_CONTEXT(deviceExtension),
                             NULL,
                             NULL,
                             0,
                             NULL,
                             0,
                             0,
                             NULL,
                             0,
                             0);
            }
            
	}

	channelExtension->CurrentBulkTransferIndex ^= 1; // toggle index.
	index = channelExtension->CurrentBulkTransferIndex;
	status = USBCAMD_IntOrBulkTransfer(deviceExtension,
                        channelExtension,
                        channelExtension->TransferExtension[index].DataBuffer,
                        channelExtension->TransferExtension[index].BufferLength,
                        channelExtension->DataPipe,
                        NULL,
                        NULL,
                        0,
                        BULK_TRANSFER);        


   if (status != STATUS_SUCCESS) {
        // we have an error on the submission set the stream error flag
        // and exit.
        channelExtension->StreamError = TRUE;
		TEST_TRAP();
   }

	USBCAMD_ExFreePool(usbWorkItem);
}	


/*++

Routine Description:


Arguments:

Return Value:

    None.

--*/

NTSTATUS
USBCAMD_CancelOutstandingBulkIntIrps(
        IN PUSBCAMD_DEVICE_EXTENSION deviceExtension,
        IN BOOLEAN bSaveIrp
        )
{

    NTSTATUS ntStatus= STATUS_SUCCESS;
    ULONG PipeIndex;


   for ( PipeIndex = 0; PipeIndex < deviceExtension->Interface->NumberOfPipes; PipeIndex++ ) {

        if ( USBCAMD_OutstandingIrp(deviceExtension, PipeIndex)) {

            // there is a pending IRP on this Pipe. Cancel it
            ntStatus = USBCAMD_CancelOutstandingIrp(deviceExtension,PipeIndex,bSaveIrp);
        }
    }

    return ntStatus;
}

/*++

Routine Description:


Arguments:

Return Value:

    None.

--*/

NTSTATUS
USBCAMD_CancelOutstandingIrp(
        IN PUSBCAMD_DEVICE_EXTENSION deviceExtension,
        IN ULONG PipeIndex,
        IN BOOLEAN bSaveIrp
        )
{

    NTSTATUS ntStatus= STATUS_SUCCESS;
    PUSBCAMD_TRANSFER_EXTENSION pTransferContext;
    PLIST_ENTRY pListHead = &deviceExtension->PipePinRelations[PipeIndex].IrpPendingQueue;

	do {
		pTransferContext =  USBCAMD_GetFirstQueuedIrp(deviceExtension,PipeIndex);
		if ( pTransferContext == NULL )
			break;
			
	  	ASSERT_TRANSFER(pTransferContext);

		if ( !pTransferContext->ChannelExtension) {
    		//
    		// Block here until the bulk/int transfer is completed with 
    		// STAUTS_CANCELLED.
    		//
    
    		KeInitializeEvent(&pTransferContext->BulkContext.CancelEvent, SynchronizationEvent, FALSE);
   
    		//
    		// Irp Cancel request is now active tell our completion routine to signal
    		// us when the IRP is cancelled.
    		//
    		pTransferContext->BulkContext.Flags |= USBCAMD_CANCEL_IRP;
    		USBCAMD_KdPrint (MAX_TRACE, ("Wait for Bulk transfer Irp to complete with Cancel.\n"));
    		IoCancelIrp(pTransferContext->DataIrp); // now we got the IRP , cancel it.

    		ntStatus = KeWaitForSingleObject(
                       &pTransferContext->BulkContext.CancelEvent,
                       Executive,
                       KernelMode,
                       FALSE,
                       NULL);   
                       
			pTransferContext->BulkContext.Flags |= ~USBCAMD_CANCEL_IRP;
		}
		else {
			ntStatus = USBCAMD_ResetPipes(deviceExtension,
					   					pTransferContext->ChannelExtension, 
					   					deviceExtension->Interface,TRUE);   
    		if (NT_SUCCESS(ntStatus)) {

    	    	// block waiting for streaming to stop on this channel.
	        	USBCAMD_WaitOnResetEvent(pTransferContext->ChannelExtension, &ntStatus);
        		if (ntStatus == STATUS_TIMEOUT ) {
            		// still waiting for usb stack to return pending IRPs. 
            		// canon't proceed. back off. to attempt later.
            		USBCAMD_KdPrint (MIN_TRACE, ("Abort Bulk Pipe failed (0x%X) \n", ntStatus));
	            	return ntStatus;
	            }	
        	}		
        }


		if ( bSaveIrp) {
			USBCAMD_QueueIrp(deviceExtension,
						 PipeIndex,
						 pTransferContext,
						 &deviceExtension->PipePinRelations[PipeIndex].IrpRestoreQueue);
		}
		else {
        
        	// Inform Cam minidriver only if cancellation is permanent.
        	if (pTransferContext->BulkContext.CommandCompleteCallback) {
            	// call the completion handler
            	(*pTransferContext->BulkContext.CommandCompleteCallback)
                                (USBCAMD_GET_DEVICE_CONTEXT(deviceExtension), 
                                 pTransferContext->BulkContext.CommandCompleteContext, 
                                 STATUS_CANCELLED);
        	}
   
        	// recycle allocate resources for the cancelled transfer.
        	if ( pTransferContext->ChannelExtension == NULL ) {
        		USBCAMD_FreeBulkTransfer(pTransferContext);  
            	USBCAMD_ExFreePool(pTransferContext);
            }
    	}

    }while (pTransferContext);

    return ntStatus;
}


/*++

Routine Description:


Arguments:

Return Value:

    None.

--*/

NTSTATUS
USBCAMD_RestoreOutstandingBulkIntIrps(
        IN PUSBCAMD_DEVICE_EXTENSION deviceExtension
        )
{

   NTSTATUS ntStatus= STATUS_SUCCESS;
   ULONG PipeIndex;
   PUSBCAMD_TRANSFER_EXTENSION pTransExt;

   for ( PipeIndex = 0; PipeIndex < deviceExtension->Interface->NumberOfPipes; PipeIndex++ ) {

        if ( USBCAMD_GetQueuedIrpCount(deviceExtension, 
        								PipeIndex,
        								&deviceExtension->PipePinRelations[PipeIndex].IrpRestoreQueue)) {
            // there are pending IRPs on this Pipe. restore them
            for ( ;;) {
            	// Dequeue this irp from the restore Q.

		    	pTransExt = USBCAMD_DequeueFirstIrp(deviceExtension,
    							 PipeIndex,
    				   			&deviceExtension->PipePinRelations[PipeIndex].IrpRestoreQueue);
    
				if ( pTransExt == NULL ) 
					break;

            	ntStatus = USBCAMD_RestoreOutstandingIrp(deviceExtension,PipeIndex,pTransExt);
            }
        }
   }
   return ntStatus;
}


/*++

Routine Description:


Arguments:

Return Value:

    None.

--*/

NTSTATUS
USBCAMD_RestoreOutstandingIrp(
        IN PUSBCAMD_DEVICE_EXTENSION deviceExtension,
        IN ULONG PipeIndex,
        IN PUSBCAMD_TRANSFER_EXTENSION pTransferContext
        )
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PVOID pBuffer,commandContext;
    ULONG TransferSize;
    PCOMMAND_COMPLETE_FUNCTION commandComplete;
    PUSBCAMD_CHANNEL_EXTENSION channelExtension;
    BOOLEAN LoopBack;
    UCHAR TransferType;
			

    ASSERT_TRANSFER(pTransferContext);
    USBCAMD_KdPrint (MAX_TRACE, ("Restore Bulk/int transfer .\n"));

    // get all the relavent data from transfer context.
    pBuffer = pTransferContext->BulkContext.pOriginalTransferBuffer;
    TransferSize = pTransferContext->BulkContext.ChunkSize;
    commandComplete = pTransferContext->BulkContext.CommandCompleteCallback;
    commandContext = pTransferContext->BulkContext.CommandCompleteContext;
    LoopBack = pTransferContext->BulkContext.LoopBack;
    TransferType = pTransferContext->BulkContext.TransferType;
    channelExtension = pTransferContext->ChannelExtension;
   
    // recycle allocate resources for the cancelled transfer.

    if ( channelExtension == NULL ) {
       USBCAMD_FreeBulkTransfer(pTransferContext);  
       USBCAMD_ExFreePool(pTransferContext);
    }

   	// request a new transfer with the resotred data.
    ntStatus = USBCAMD_IntOrBulkTransfer(deviceExtension,
                                         channelExtension,
                                         pBuffer,
                                         TransferSize,
                                         PipeIndex,
                                         commandComplete,
                                         commandContext,
                                         LoopBack,
                                         TransferType);        
   	return ntStatus;
}

/*++

Routine Description:

    This routine will cancel any pending a read or write operation on a specified 
    bulk pipe. 

Arguments:

    DeviceContext - 

    PipeIndex - 



Return Value:

    NT status code

--*/

NTSTATUS
USBCAMD_CancelBulkReadWrite( 
    IN PVOID DeviceContext,
    IN ULONG PipeIndex
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
 
    PUSBCAMD_DEVICE_EXTENSION deviceExtension;
    PUSBD_PIPE_INFORMATION pipeHandle ;

    deviceExtension = USBCAMD_GET_DEVICE_EXTENSION(DeviceContext);


    USBCAMD_KdPrint ( MAX_TRACE, ("Enter USBCAMD_CancelBulkReadWrite\n"));

    //
    // do some parameter validation.
    //

    if (PipeIndex > deviceExtension->Interface->NumberOfPipes) {
        
        USBCAMD_KdPrint(MIN_TRACE,("invalid pipe index!\n"));
        ntStatus = STATUS_INVALID_PARAMETER;        
        return ntStatus;        
    }

    // check if we have a pending read or write already. 

    if (!USBCAMD_OutstandingIrp(deviceExtension, PipeIndex) ) {
        // no pending IRP for this pipe ...
        ntStatus = STATUS_SUCCESS;        
        return ntStatus;            
    }
        
    pipeHandle = &deviceExtension->Interface->Pipes[PipeIndex];

    if (pipeHandle->PipeType < UsbdPipeTypeBulk ) {
     
        USBCAMD_KdPrint(MIN_TRACE,("invalid pipe type!\n"));
        ntStatus = STATUS_INVALID_PARAMETER;        
        return ntStatus;        
    }

    if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {
        USBCAMD_KdPrint(MIN_TRACE,("BulkCancel is cancelable at Passive Level Only!\n"));
        ntStatus = STATUS_INVALID_PARAMETER;   
        TEST_TRAP();
        return ntStatus;       
    }
  
    // there is a pending IRP on this Pipe. Cancel it
    ntStatus = USBCAMD_CancelOutstandingIrp(deviceExtension,PipeIndex,FALSE);

    return ntStatus;

}


                             


