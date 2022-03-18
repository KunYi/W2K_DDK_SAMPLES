/*++

Copyright (c) 1998  Microsoft Corporation

Module Name:

  reset.c

Abstract:

   Isochronous transfer code for usbcamd USB camera driver

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

LARGE_INTEGER USBCAMD_30Milliseconds = {(ULONG)(-30 * 1000 * 10), -1};
LARGE_INTEGER ResetTimeout = {(ULONG) -80000000, -1};

NTSTATUS
USBCAMD_GetPortStatus(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION channelExtension,
    IN PULONG PortStatus
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
    NTSTATUS ntStatus, status ;
    PIRP irp;
    KEVENT event;
    IO_STATUS_BLOCK ioStatus;
    PIO_STACK_LOCATION nextStack;
    
    USBCAMD_SERIALIZE(DeviceExtension);

    USBCAMD_KdPrint (MAX_TRACE, ("enter USBCAMD_GetPortStatus on Stream #%d \n",
                     channelExtension->StreamNumber));

    *PortStatus = 0;
    status = STATUS_SUCCESS;
    ntStatus = STATUS_SUCCESS;
    
    //
    // issue a synchronous request
    //

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(
            IOCTL_INTERNAL_USB_GET_PORT_STATUS,
            DeviceExtension->StackDeviceObject,
            NULL,
            0,
            NULL,
            0,
            TRUE, /* INTERNAL */
            &event,
            &ioStatus);

    if (irp == NULL ) {
        USBCAMD_RELEASE(DeviceExtension);
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Call the class driver to perform the operation.  If the returned status
    // is PENDING, wait for the request to complete.
    //

    nextStack= IoGetNextIrpStackLocation(irp);

    ASSERT(nextStack != NULL);

    //
    // pass the URB to the USB driver stack
    //
    nextStack->Parameters.Others.Argument1 = PortStatus;

    USBCAMD_KdPrint (ULTRA_TRACE, ("calling USBD port status api\n"));

    if (DeviceExtension->Initialized) {
        ntStatus= IoCallDriver(DeviceExtension->StackDeviceObject,irp);
    } else {
        ntStatus = STATUS_DEVICE_DATA_ERROR;
    }

    USBCAMD_KdPrint (MAX_TRACE, ("return from IoCallDriver USBD %x\n", ntStatus));

    if (ntStatus == STATUS_PENDING) {

        USBCAMD_KdPrint (MAX_TRACE, ( "Wait for single object\n"));

        status = KeWaitForSingleObject(&event,
                                       Executive,
                                       KernelMode,
                                       FALSE,
                                       &USBCAMD_30Milliseconds);
        
        if (status == STATUS_TIMEOUT) {                        //
            //
            // USBD did not complete this request in 30 milliseconds, assume
            // that the USBD is hung and return an
            // error.                        
            //
            USBCAMD_RELEASE(DeviceExtension);
            return(STATUS_UNSUCCESSFUL);                    
        }

        USBCAMD_KdPrint (ULTRA_TRACE, ("Wait for single object, returned %x\n", status));
        
    } else {
        ioStatus.Status = ntStatus;
    }

    //
    // USBD maps the error code for us
    //
    ntStatus = ioStatus.Status;

    USBCAMD_KdPrint(MIN_TRACE, ("GetPortStatus returns (0x%x), Port Status (0x%x)\n",ntStatus, *PortStatus));
    
    USBCAMD_RELEASE(DeviceExtension);

    return ntStatus;
}


NTSTATUS
USBCAMD_EnablePort(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension
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
    KEVENT event;
    IO_STATUS_BLOCK ioStatus;
    PIO_STACK_LOCATION nextStack;

    USBCAMD_KdPrint (MAX_TRACE, ("enter USBCAMD_EnablePort\n"));

    //
    // issue a synchronous request
    //

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(
                IOCTL_INTERNAL_USB_ENABLE_PORT,
                DeviceExtension->StackDeviceObject,
                NULL,
                0,
                NULL,
                0,
                TRUE, /* INTERNAL */
                &event,
                &ioStatus);

    if (irp == NULL ) {
        return STATUS_UNSUCCESSFUL;
    }
  
    //
    // Call the class driver to perform the operation.  If the returned status
    // is PENDING, wait for the request to complete.
    //

    nextStack = IoGetNextIrpStackLocation(irp);
    ASSERT(nextStack != NULL);

    //
    // pass the URB to the USB driver stack
    //

    USBCAMD_KdPrint (ULTRA_TRACE, ("calling USBD enable port api\n"));

    if (DeviceExtension->Initialized) {
        ntStatus = IoCallDriver(DeviceExtension->StackDeviceObject,
                                irp);
    } else {
        ntStatus = STATUS_DEVICE_DATA_ERROR;
    }

    USBCAMD_KdPrint (ULTRA_TRACE, ("return from IoCallDriver USBD %x\n", ntStatus));

    if (ntStatus == STATUS_PENDING) {

        USBCAMD_KdPrint (ULTRA_TRACE, ( "Wait for single object\n"));

        status = KeWaitForSingleObject(
                       &event,
                       Executive,
                       KernelMode,
                       FALSE,
                       &USBCAMD_30Milliseconds);
        
        if (status == STATUS_TIMEOUT) {                        //
            //
            // USBD did not complete this request in 30 milliseconds, assume
            // that the USBD is hung and return an
            // error.                        
            //
            USBCAMD_RELEASE(DeviceExtension);
            return(STATUS_UNSUCCESSFUL);                    
        }

        USBCAMD_KdPrint (ULTRA_TRACE, ("Wait for single object, returned %x\n", status));
        
    } else {
        ioStatus.Status = ntStatus;
    }

    //
    // USBD maps the error code for us
    //
    ntStatus = ioStatus.Status;

    USBCAMD_KdPrint(MAX_TRACE, ("USBCAMD_EnablePort (%x)\n", ntStatus));

    return ntStatus;
}


/*++

Routine Description:

    This routine will Block here until the channel stream process is completely
    stopped

Arguments:

    ChannelExtension - 


Return Value:

    NT status code

--*/

VOID
USBCAMD_WaitOnResetEvent(
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension,
    IN PULONG pStatus)
{
   
    //
    // set the reset event now so that any subsequent reset 
    // requests get ignored
    //

    KeInitializeEvent(&ChannelExtension->ResetEvent, SynchronizationEvent, FALSE);
    
    //
    // reset request is now active tell our timeout DPC to signal
    // us when the stream is idle
    //
    
    ChannelExtension->Flags |= USBCAMD_TIMEOUT_STREAM_WAIT;
    

    USBCAMD_KdPrint (MAX_TRACE, ("Reset, Wait for stream #%d to stop\n", 
                                  ChannelExtension->StreamNumber));
    *pStatus = KeWaitForSingleObject(
                       &ChannelExtension->ResetEvent,
                       Executive,
                       KernelMode,
                       FALSE,
                       &ResetTimeout);

    if (*pStatus == STATUS_TIMEOUT) {
        USBCAMD_KdPrint (MIN_TRACE, ("*** Waiting on Reset Pipe Timed out.*** \n"));
        ChannelExtension->Flags &= ~USBCAMD_TIMEOUT_STREAM_WAIT;
//        TEST_TRAP();
    }
    else {
        USBCAMD_KdPrint (MAX_TRACE, ("Reset, stream #%d stopped status = 0x%x\n",
                                ChannelExtension->StreamNumber,
                                *pStatus));
    }
}


/*++

Routine Description:

    This function restarts the streaming process from an error state at 
    PASSIVE_LEVEL.

Arguments:

    DeviceExtension - pointer to the device extension for this instance of the USB camera
                    devcice.
                    
    ChannelExtension - Channel to reset.    

Return Value:

--*/   
NTSTATUS
USBCAMD_ResetChannel(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension,
    IN ULONG portUsbStatus,
    IN ULONG portNtStatus
    )    
{
    NTSTATUS ntStatus ;
    ULONG status;
    LONG StreamNumber;

    USBCAMD_SERIALIZE(DeviceExtension);

    ntStatus = STATUS_SUCCESS;

    StreamNumber = ChannelExtension->StreamNumber;
    USBCAMD_KdPrint (MAX_TRACE, ("USBCAMD_ResetChannel #%d\n", StreamNumber));
    ASSERT_CHANNEL(ChannelExtension);
    ASSERT(DeviceExtension->ActualInstances[StreamNumber] > 0);
    ASSERT(ChannelExtension->ChannelPrepared == TRUE);


#ifdef MAX_DEBUG
//    TEST_TRAP();
#endif  

#ifndef WINNT
	if (NT_SUCCESS(portNtStatus) && !(portUsbStatus & USBD_PORT_ENABLED)) {
    //
    // port is disabled, attempt reset
    //
		USBCAMD_EnablePort(DeviceExtension);
	}
#endif

	//
	// channel may not be in error mode, make sure and issue 
	// an abort beforing waiting for the channel to spin down
	//

	ntStatus = USBCAMD_ResetPipes(DeviceExtension,
					   ChannelExtension, 
					   DeviceExtension->Interface,
					   TRUE);   
    
    if (NT_SUCCESS(ntStatus)) {

        // block waiting for streaming to stop on this channel.
        USBCAMD_WaitOnResetEvent(ChannelExtension, &status);
        if (status == STATUS_TIMEOUT ) {
            // still waiting for usb stack to return pending IRPs. 
            // canon't proceed. back off. to attempt later.
            USBCAMD_KdPrint (MIN_TRACE, ("USBCAMD_ResetChannel failed (0x%X) \n", status));
            USBCAMD_RELEASE(DeviceExtension);
            return status;
        }
        // 
        // we have been signalled by the timeout DPC that the stop is complete
        // go ahead and attempt to restart the channel.
	    //
	    // now reset the pipes
	    //

	    ntStatus = USBCAMD_ResetPipes(DeviceExtension,
								      ChannelExtension,
								      DeviceExtension->Interface,
								      FALSE);

	    //
	    // only restart the stream if it is already in the running state
	    //

	    if (ChannelExtension->ImageCaptureStarted) {
            if (DeviceExtension->Usbcamd_version == USBCAMD_VERSION_200) {

			    // send hardware stop and re-start
			    if (NT_SUCCESS(ntStatus)) {
				    ntStatus = (*DeviceExtension->DeviceDataEx.DeviceData2.CamStopCaptureEx)(
							    DeviceExtension->StackDeviceObject,      
							    USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension),
                                StreamNumber);
			    }                    

			    if (NT_SUCCESS(ntStatus)) {
				    ntStatus = (*DeviceExtension->DeviceDataEx.DeviceData2.CamStartCaptureEx)(
							    DeviceExtension->StackDeviceObject,
							    USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension),
                                StreamNumber);    
   
			    }                    
            }
            else {
			    // send hardware stop and re-start
			    if (NT_SUCCESS(ntStatus)) {
				    ntStatus = (*DeviceExtension->DeviceDataEx.DeviceData.CamStopCapture)(
							    DeviceExtension->StackDeviceObject,      
							    USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension));
			    }                    

			    if (NT_SUCCESS(ntStatus)) {
				    ntStatus = (*DeviceExtension->DeviceDataEx.DeviceData.CamStartCapture)(
							    DeviceExtension->StackDeviceObject,
							    USBCAMD_GET_DEVICE_CONTEXT(DeviceExtension));    
			    }                    

            }

		    if (NT_SUCCESS(ntStatus)) {

                ChannelExtension->SyncPipe = DeviceExtension->SyncPipe;
                if (StreamNumber == DeviceExtension->IsoPipeStreamType ) {
                    ChannelExtension->DataPipe = DeviceExtension->DataPipe;
                    ChannelExtension->DataPipeType = UsbdPipeTypeIsochronous;   
				    USBCAMD_StartIsoStream(DeviceExtension, ChannelExtension, TRUE);
                }
                else if (StreamNumber == DeviceExtension->BulkPipeStreamType ) {
                    ChannelExtension->DataPipe = DeviceExtension->BulkDataPipe;
                    ChannelExtension->DataPipeType = UsbdPipeTypeBulk;  
				    USBCAMD_StartBulkStream(DeviceExtension, ChannelExtension, TRUE);                    
                }
		    }        
	    }
        else {
            USBCAMD_KdPrint (MIN_TRACE, ("ImageCaptureStarted is False. \n"));
        }

    }    

    USBCAMD_KdPrint (MIN_TRACE, ("USBCAMD_ResetChannel exit (0x%X) \n", ntStatus));
    USBCAMD_RELEASE(DeviceExtension);

    return ntStatus;
}            


NTSTATUS
USBCAMD_ResetPipes(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension,
    IN PUSBD_INTERFACE_INFORMATION InterfaceInformation,
    IN BOOLEAN Abort
    )
/*++

Routine Description:

    Reset both pipes associated with a video channel on the
    camera.

Arguments:

Return Value:


--*/
{
    NTSTATUS ntStatus;
	PURB urb;

    USBCAMD_KdPrint (MAX_TRACE, ("USBCAMD_ResetPipes\n"));

	urb = USBCAMD_ExAllocatePool(NonPagedPool, 
						 sizeof(struct _URB_PIPE_REQUEST));

	if (urb) {
	
        urb->UrbHeader.Length = (USHORT) sizeof (struct _URB_PIPE_REQUEST);
    	urb->UrbHeader.Function = (USHORT) (Abort ? URB_FUNCTION_ABORT_PIPE : 
                                                   	URB_FUNCTION_RESET_PIPE);
                                                           	
        urb->UrbPipeRequest.PipeHandle = 
            InterfaceInformation->Pipes[ChannelExtension->DataPipe].PipeHandle;

        ntStatus = USBCAMD_CallUSBD(DeviceExtension, urb);
        if ( !NT_SUCCESS(ntStatus) )  {
            if (Abort) {
                USBCAMD_KdPrint (MIN_TRACE, ("Abort Data Pipe Failed (0x%x) \n", ntStatus));
               // TEST_TRAP();
            }
        }

        if (NT_SUCCESS(ntStatus) && ChannelExtension->SyncPipe != -1)  {
            urb->UrbHeader.Length = (USHORT) sizeof (struct _URB_PIPE_REQUEST);
        	urb->UrbHeader.Function =(USHORT) (Abort ? URB_FUNCTION_ABORT_PIPE : 
                                                    	URB_FUNCTION_RESET_PIPE);
            urb->UrbPipeRequest.PipeHandle = 
                InterfaceInformation->Pipes[ChannelExtension->SyncPipe].PipeHandle;
                
            ntStatus = USBCAMD_CallUSBD(DeviceExtension, urb);
            if ( !NT_SUCCESS(ntStatus) )  {
                if (Abort) {
                    USBCAMD_KdPrint (MIN_TRACE, ("Abort Sync Pipe Failed (0x%x) \n", ntStatus));
                 //   TEST_TRAP();
                }
            }
        }            

        USBCAMD_ExFreePool(urb);
        
	} else {
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;		
    }		

    return ntStatus;
}	


VOID
USBCAMD_ChannelTimeoutDPC(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
/*++

Routine Description:

    This routine runs at DISPATCH_LEVEL IRQL. 

    
    
Arguments:

    Dpc - Pointer to the DPC object.

    DeferredContext - 

    SystemArgument1 - not used.
    
    SystemArgument2 - not used.

Return Value:

    None.

--*/
{
    PUSBCAMD_CHANNEL_EXTENSION channelExtension;
    PUSBCAMD_DEVICE_EXTENSION deviceExtension;
    LARGE_INTEGER dueTime;
    BOOLEAN inQueue;
    ULONG StreamNumber;

    channelExtension = DeferredContext;    
    deviceExtension = channelExtension->DeviceExtension;

        
    ASSERT_CHANNEL(channelExtension);
    ASSERT(channelExtension->ChannelPrepared == TRUE);
    
    StreamNumber = channelExtension->StreamNumber;

    if ( channelExtension->StreamError ||
        (channelExtension->Flags & USBCAMD_TIMEOUT_STREAM_WAIT) ||
        (channelExtension->Flags & USBCAMD_STOP_STREAM) ||
        deviceExtension->CameraUnplugged) {

        BOOLEAN idle = TRUE;
        ULONG i;

        // First clean out any pending reads
        
        // 
        // We have an error on the stream or a stop request
        // pending.  Scan through the transfers if nothiing 
        // is pending then go ahead and complete any outstanding 
        // read requests with an error.
        //

        if ( !channelExtension->VirtualStillPin ) {
            for (i=0; i<USBCAMD_MAX_REQUEST; i++) {
                if (channelExtension->TransferExtension[i].Pending > 0) {
                    idle = FALSE;
                    break;
                }                
            }            
        }

        if (channelExtension->ImageCaptureStarted == FALSE) {
            //
            // reset called before stream fully started
            // just ignore for now.
            idle = FALSE;
        }
        
        if (idle) {
            
            //
            // if streamerror flag is on. schedule a worker thread to reset the channel
            // w/o cancelling the pending READ_SRBs only if the camera is still plugged in 
            // and there is not pending request to stop the stream.
            //
            
            if ( (channelExtension->StreamError == TRUE)  && 
                 (deviceExtension->CameraUnplugged == FALSE) &&
                 ((channelExtension->Flags & USBCAMD_STOP_STREAM) != TRUE) ) {
                USBCAMD_KdPrint(MAX_TRACE, ("***USB Error*** on stream # %d. Flags = %d \n", 
                                            channelExtension->StreamNumber,
                                            channelExtension->Flags));
		        USBCAMD_ProcessResetRequest(deviceExtension,channelExtension,NULL); 
                // reset the stream error flag.
                // channelExtension->StreamError = FALSE;
            }

            //
            // now complete any pending reads in queue for a stop request only. or
            // if device has been unplugged
            //

            if ( ((!channelExtension->IdleIsoStream) && 
                 (channelExtension->Flags & USBCAMD_STOP_STREAM)) ||
                 (deviceExtension->CameraUnplugged)){


                do {
                    PLIST_ENTRY listEntry;
                    PUSBCAMD_READ_EXTENSION readExtension;

                    listEntry = 
                        ExInterlockedRemoveHeadList(&(channelExtension->PendingIoList),
                                                    &channelExtension->PendingIoListSpin);
                
                    if (listEntry != NULL) {
                        readExtension =     
                            (PUSBCAMD_READ_EXTENSION) CONTAINING_RECORD(listEntry, 
                                                     USBCAMD_READ_EXTENSION  , 
                                                     ListEntry);                        
                        USBCAMD_KdPrint (MIN_TRACE, 
                            ("Cancelling queued read SRB on stream %d, Ch. Flag(0x%x)\n",StreamNumber,
                               channelExtension->Flags));    
                       
						
                        USBCAMD_CompleteRead(channelExtension,readExtension,STATUS_CANCELLED,0);
                    
                    } else {
                        break;
                    }

                } while (1);
            }
            //
            // if we had an irp for the current frame complete it as well
            //
            KeAcquireSpinLockAtDpcLevel(&channelExtension->CurrentRequestSpinLock);
            if ( ((channelExtension->CurrentRequest) && (channelExtension->Flags & USBCAMD_STOP_STREAM)) ||
                  (deviceExtension->CameraUnplugged) ) {
                
                PUSBCAMD_READ_EXTENSION readExtension = channelExtension->CurrentRequest;              
                
                if (readExtension != NULL ) {

                    channelExtension->CurrentRequest = NULL;                        
                
                    if ( channelExtension->IdleIsoStream) {
                        // recycle the read SRB
                        ExInterlockedInsertHeadList( &(channelExtension->PendingIoList),
                                                     &(readExtension->ListEntry),
                                                     &channelExtension->PendingIoListSpin);
                    }
                    else {
                        // complete current read SRB 
                        USBCAMD_CompleteRead(channelExtension,readExtension,STATUS_CANCELLED,0);
                        USBCAMD_KdPrint (MIN_TRACE, 
                                ("Cancelling current read SRB on stream %d, Ch. Flag(0x%x)\n",StreamNumber,
                                   channelExtension->Flags));    

                    }
                }
            }
            KeReleaseSpinLockFromDpcLevel(&channelExtension->CurrentRequestSpinLock);
            
            // 
            // signal the reset request if one is waiting
            //
            
            if (channelExtension->Flags & USBCAMD_TIMEOUT_STREAM_WAIT) {
                channelExtension->Flags &= ~USBCAMD_TIMEOUT_STREAM_WAIT;
                channelExtension->StreamError = FALSE;
                KeSetEvent(&channelExtension->ResetEvent,1,FALSE);
            }

            //
            // stream has stopped go ahead and signal the stop event
            // if we have one
            //

            if (channelExtension->Flags & USBCAMD_STOP_STREAM) {
                channelExtension->Flags &= ~USBCAMD_STOP_STREAM;
                //
                // image capture has been stopped
                //
                KeSetEvent(&channelExtension->StopEvent,1,FALSE);
            } 
        }
    }
    
    //
    // Schedule the next one
    //
    
    if (channelExtension->Flags & USBCAMD_ENABLE_TIMEOUT_DPC) {

        KeInitializeTimerEx(&channelExtension->TimeoutTimer,SynchronizationEvent);
        KeInitializeDpc(&channelExtension->TimeoutDpc,
                        USBCAMD_ChannelTimeoutDPC,
                        channelExtension);

        if ( StreamNumber == STREAM_Capture ) {                
            dueTime.QuadPart = -10000 * USBCAMD_TIMEOUT_INTERVAL;
        }
        else {
            dueTime.QuadPart = -10000 * USBCAMD_STILL_TIMEOUT;
        }

        inQueue = KeSetTimer(&channelExtension->TimeoutTimer,
                             dueTime,
                             &channelExtension->TimeoutDpc);        

        ASSERT(inQueue == FALSE);                             
        
    }
}


BOOLEAN
USBCAMD_ProcessResetRequest(
    IN PUSBCAMD_DEVICE_EXTENSION DeviceExtension,
    IN PUSBCAMD_CHANNEL_EXTENSION ChannelExtension,
    IN PHW_STREAM_REQUEST_BLOCK Srb    
    )
/*++

Routine Description:

    Request a reset of the ISO stream.
    This function is re-entarnt and can be called at DPC level

Arguments:

Return Value:

    None.

--*/
{
    ASSERT_CHANNEL(ChannelExtension);


    if (InterlockedIncrement(&DeviceExtension->TimeoutCount[ChannelExtension->StreamNumber]) > 0) {
        USBCAMD_KdPrint (MIN_TRACE, ("Stream # %d timeout already scheduled\n", ChannelExtension->StreamNumber));
        InterlockedDecrement(&DeviceExtension->TimeoutCount[ChannelExtension->StreamNumber]);
        return FALSE;
    }
    
    USBCAMD_KdPrint (MAX_TRACE, ("Stream # %d reset scheduled\n", ChannelExtension->StreamNumber));
    ChannelExtension->pWorkItem = (PVOID) USBCAMD_ExAllocatePool(NonPagedPool, sizeof(USBCAMD_WORK_ITEM));

    if (ChannelExtension->pWorkItem) {
             
        ExInitializeWorkItem(&((PUSBCAMD_WORK_ITEM)ChannelExtension->pWorkItem)->WorkItem,
                             USBCAMD_ResetWorkItem,
                             ChannelExtension->pWorkItem);
        ((PUSBCAMD_WORK_ITEM)ChannelExtension->pWorkItem)->StreamNumber = ChannelExtension->StreamNumber;
        ((PUSBCAMD_WORK_ITEM)ChannelExtension->pWorkItem)->Srb = Srb;
        ((PUSBCAMD_WORK_ITEM)ChannelExtension->pWorkItem)->ChannelExtension = ChannelExtension;
        ExQueueWorkItem(&((PUSBCAMD_WORK_ITEM)ChannelExtension->pWorkItem)->WorkItem,
                        CriticalWorkQueue);

    } else {
        //
        // failed to schedule the timeout
        //
        InterlockedDecrement(&DeviceExtension->TimeoutCount[ChannelExtension->StreamNumber]);
    }
    return TRUE;
}


VOID
USBCAMD_ResetWorkItem(
    PVOID Context
    )
/*++

Routine Description:

    Work item executed at passive level to reset the camera

Arguments:

Return Value:

    None.

--*/
{
    PUSBCAMD_DEVICE_EXTENSION deviceExtension;
//    ULONG status[MAX_STREAM_COUNT];
    NTSTATUS ntStatus[MAX_STREAM_COUNT] ;
    ULONG portStatus[MAX_STREAM_COUNT];
    BOOLEAN bExceptionFlag = FALSE;
//    PHW_STREAM_REQUEST_BLOCK Srb;

#ifndef WINNT
    static timeoutCounter = 0;
#endif


    ASSERT_CHANNEL(((PUSBCAMD_WORK_ITEM) Context)->ChannelExtension);
    deviceExtension = ((PUSBCAMD_WORK_ITEM) Context)->ChannelExtension->DeviceExtension;    
    
    if (deviceExtension->CameraUnplugged ) {
        //
        // Camera is unplugged, proceed with canceling pending SRBs
        //
        USBCAMD_KdPrint (MIN_TRACE, ("***ERROR*** :Camera unplugged...\n"));
        InterlockedDecrement(&deviceExtension->TimeoutCount[STREAM(Context)]);
        USBCAMD_ExFreePool(Context);
        return ;        
    }

    
    // if we are dealing with a virtual still channel. then no HW reset is required on 
    // this channel. The video channel will eventually reset the ISO pipe since they both
    // use the same pipe.

    if (((PUSBCAMD_WORK_ITEM) Context)->ChannelExtension->VirtualStillPin ) {
        InterlockedDecrement(&deviceExtension->TimeoutCount[STREAM(Context)]);
#ifndef WINNT
        timeoutCounter--;
#endif
        USBCAMD_ExFreePool(Context);
        return ;            
    }
    
#ifndef WINNT
	//
    // Check the port state.
    //

    ntStatus[STREAM(Context)] = USBCAMD_GetPortStatus(deviceExtension,
                                     ((PUSBCAMD_WORK_ITEM) Context)->ChannelExtension, 
                                     &portStatus[STREAM(Context)]);


    if ( !NT_SUCCESS(ntStatus[STREAM(Context)])){
        // 
        // ***NOTICE***
        // we will fail getportstatus if we are a dealing with composite device.
        // this is a temp hack till we figure out a way around this issue.
        // if ISO completion routine set the ch error flag. That means we got a catastrophy
        // which in association to SC timeout indicates that camera is most likely gone.
        //
        ++timeoutCounter;
        if (( timeoutCounter == 5 ) && 
            ((PUSBCAMD_WORK_ITEM) Context)->ChannelExtension->StreamError  ){
            bExceptionFlag = TRUE;
        }
        else {
            InterlockedDecrement(&deviceExtension->TimeoutCount[STREAM(Context)]);
            USBCAMD_ExFreePool(Context);
            return ;            
        }
    }

    if ( (NT_SUCCESS(ntStatus[STREAM(Context)]) && !(portStatus[STREAM(Context)] & USBD_PORT_CONNECTED))

         || bExceptionFlag ) {

        //
        // Camera is unplugged, proceed with canceling pending SRBs
        //
#ifdef MAX_DEBUG
        USBCAMD_DumpReadQueues(deviceExtension);
#endif
        deviceExtension->CameraUnplugged = TRUE;
        USBCAMD_KdPrint (MIN_TRACE, ("***ERROR*** :Camera unplugged discovered...\n"));

#ifdef MAX_DEBUG
       USBCAMD_DumpReadQueues(deviceExtension);
#endif
        InterlockedDecrement(&deviceExtension->TimeoutCount[STREAM(Context)]);
        USBCAMD_ExFreePool(Context);
        return ;   
    }
    
#endif
    // camera is still plugged. now check for some other boundary conditions.
    
    // from now, the assumption is that either an ISO or BULK transfer has went bad,
    // and we need to reset the respected pipe associated with this channel.
    
    if ( ((PUSBCAMD_WORK_ITEM) Context)->Srb ) {
        USBCAMD_KdPrint(MIN_TRACE, ("SRB %x Timed out on stream #%d . Reset Pipe.. \n", 
                                    ((PUSBCAMD_WORK_ITEM) Context)->Srb,
                                     STREAM(Context)));
    }
    else {
        USBCAMD_KdPrint(MIN_TRACE, ("USB Error on Stream # %d. Reset Pipe.. \n", STREAM(Context)));
    }

#ifdef MAX_DEBUG
    USBCAMD_DumpReadQueues(deviceExtension);
#endif

    USBCAMD_ResetChannel(deviceExtension,
                         ((PUSBCAMD_WORK_ITEM) Context)->ChannelExtension,
                         portStatus[STREAM(Context)],
                         ntStatus[STREAM(Context)]);  

    // OK to handle another reset now
    InterlockedDecrement(&deviceExtension->TimeoutCount[STREAM(Context)]);
    
    USBCAMD_ExFreePool(Context);
}


