/*++

Copyright (c) 1998  Microsoft Corporation

Module Name:

    isochapi.c

Abstract


Author:

    Peter Binder (pbinder) 7/26/97

Revision History:
Date     Who       What
-------- --------- ------------------------------------------------------------
7/26/97  pbinder   birth
4/14/98  pbinder   taken from 1394diag
--*/

#include "pch.h"

NTSTATUS
t1394Diag_IsochAllocateBandwidth(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            nMaxBytesPerFrameRequested,
    IN ULONG            fulSpeed,
    OUT PHANDLE         phBandwidth,
    OUT PULONG          pBytesPerFrameAvailable,
    OUT PULONG          pSpeedSelected
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIRB                pIrb;

    ENTER("t1394Diag_IsochAllocateBandwidth");

    TRACE(TL_TRACE, ("nMaxBytesPerFrameRequested = 0x%x\n", nMaxBytesPerFrameRequested));
    TRACE(TL_TRACE, ("fulSpeed = 0x%x\n", fulSpeed));

    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochAllocateBandwidth;
    } // if

    pIrb->FunctionNumber = REQUEST_ISOCH_ALLOCATE_BANDWIDTH;
    pIrb->Flags = 0;
    pIrb->u.IsochAllocateBandwidth.nMaxBytesPerFrameRequested = nMaxBytesPerFrameRequested;
    pIrb->u.IsochAllocateBandwidth.fulSpeed = fulSpeed;

    ntStatus = t1394Diag_SubmitIrpSynch(deviceExtension->StackDeviceObject, Irp, pIrb);

    if (NT_SUCCESS(ntStatus)) {

        *phBandwidth = pIrb->u.IsochAllocateBandwidth.hBandwidth;
        *pBytesPerFrameAvailable = pIrb->u.IsochAllocateBandwidth.BytesPerFrameAvailable;
        *pSpeedSelected = pIrb->u.IsochAllocateBandwidth.SpeedSelected;

        TRACE(TL_TRACE, ("hBandwidth = 0x%x\n", *phBandwidth));
        TRACE(TL_TRACE, ("BytesPerFrameAvailable = 0x%x\n", *pBytesPerFrameAvailable));

        // lets see if we got the speed we wanted
        if (fulSpeed != pIrb->u.IsochAllocateBandwidth.SpeedSelected) {

            TRACE(TL_TRACE, ("Different bandwidth speed selected.\n"));
        }

        TRACE(TL_TRACE, ("SpeedSelected = 0x%x\n", *pSpeedSelected));
    }
    else {

        TRACE(TL_ERROR, ("SubmitIrpSync failed = 0x%x\n", ntStatus));
        TRAP;
    }

    ExFreePool(pIrb);

Exit_IsochAllocateBandwidth:

    EXIT("t1394Diag_IsochAllocateBandwidth", ntStatus);
    return(ntStatus);
} // t1394Diag_IsochAllocateBandwidth

NTSTATUS
t1394Diag_IsochAllocateChannel(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            nRequestedChannel,
    OUT PULONG          pChannel,
    OUT PLARGE_INTEGER  pChannelsAvailable
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIRB                pIrb;

    ENTER("t1394Diag_IsochAllocateChannel");

    TRACE(TL_TRACE, ("nRequestedChannel = 0x%x\n", nRequestedChannel));

    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochAllocateChannel;
    } // if

    pIrb->FunctionNumber = REQUEST_ISOCH_ALLOCATE_CHANNEL;
    pIrb->Flags = 0;
    pIrb->u.IsochAllocateChannel.nRequestedChannel = nRequestedChannel;

    ntStatus = t1394Diag_SubmitIrpSynch(deviceExtension->StackDeviceObject, Irp, pIrb);

    if (NT_SUCCESS(ntStatus)) {

        *pChannel = pIrb->u.IsochAllocateChannel.Channel;
        *pChannelsAvailable = pIrb->u.IsochAllocateChannel.ChannelsAvailable;

        TRACE(TL_TRACE, ("Channel = 0x%x\n", *pChannel));
        TRACE(TL_TRACE, ("ChannelsAvailable.LowPart = 0x%x\n", pChannelsAvailable->LowPart));
        TRACE(TL_TRACE, ("ChannelsAvailable.HighPart = 0x%x\n", pChannelsAvailable->HighPart));
    }
    else {

        TRACE(TL_ERROR, ("SubmitIrpSync failed = 0x%x\n", ntStatus));
        TRAP;
    }

    ExFreePool(pIrb);

Exit_IsochAllocateChannel:

    EXIT("t1394Diag_IsochAllocateChannel", ntStatus);
    return(ntStatus);
} // t1394Diag_IsochAllocateChannel

NTSTATUS
t1394Diag_IsochAllocateResources(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            fulSpeed,
    IN ULONG            fulFlags,
    IN ULONG            nChannel,
    IN ULONG            nMaxBytesPerFrame,
    IN ULONG            nNumberOfBuffers,
    IN ULONG            nMaxBufferSize,
    IN ULONG            nQuadletsToStrip,
    OUT PHANDLE         phResource
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIRB                pIrb;

    ENTER("t1394Diag_IsochAllocateResources");

    TRACE(TL_TRACE, ("fulSpeed = 0x%x\n", fulSpeed));
    TRACE(TL_TRACE, ("fulFlags = 0x%x\n", fulFlags));
    TRACE(TL_TRACE, ("nChannel = 0x%x\n", nChannel));
    TRACE(TL_TRACE, ("nMaxBytesPerFrame = 0x%x\n", nMaxBytesPerFrame));
    TRACE(TL_TRACE, ("nNumberOfBuffers = 0x%x\n", nNumberOfBuffers));
    TRACE(TL_TRACE, ("nMaxBufferSize = 0x%x\n", nMaxBufferSize));
    TRACE(TL_TRACE, ("nQuadletsToStrip = 0x%x\n", nQuadletsToStrip));

    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochAllocateResources;
    } // if

    pIrb->FunctionNumber = REQUEST_ISOCH_ALLOCATE_RESOURCES;
    pIrb->Flags = 0;
    pIrb->u.IsochAllocateResources.fulSpeed = fulSpeed;
    pIrb->u.IsochAllocateResources.fulFlags = fulFlags;
    pIrb->u.IsochAllocateResources.nChannel = nChannel;
    pIrb->u.IsochAllocateResources.nMaxBytesPerFrame = nMaxBytesPerFrame;
    pIrb->u.IsochAllocateResources.nNumberOfBuffers = nNumberOfBuffers;
    pIrb->u.IsochAllocateResources.nMaxBufferSize = nMaxBufferSize;
    pIrb->u.IsochAllocateResources.nQuadletsToStrip = nQuadletsToStrip;

    ntStatus = t1394Diag_SubmitIrpSynch(deviceExtension->StackDeviceObject, Irp, pIrb);

    if (NT_SUCCESS(ntStatus)) {

        PISOCH_RESOURCE_DATA    IsochResourceData;
        KIRQL                   Irql;

        *phResource = pIrb->u.IsochAllocateResources.hResource;

        TRACE(TL_TRACE, ("hResource = 0x%x\n", *phResource));

        // need to add to our list...
        IsochResourceData = ExAllocatePool(NonPagedPool, sizeof(ISOCH_RESOURCE_DATA));

        if (IsochResourceData) {

            IsochResourceData->hResource = pIrb->u.IsochAllocateResources.hResource;

            KeAcquireSpinLock(&deviceExtension->IsochResourceSpinLock, &Irql);
            InsertHeadList(&deviceExtension->IsochResourceData, &IsochResourceData->IsochResourceList);
            KeReleaseSpinLock(&deviceExtension->IsochResourceSpinLock, Irql);
        }
        else {

            TRACE(TL_WARNING, ("Failed to allocate IsochResourceData!\n"));
        }

    }
    else {

        TRACE(TL_ERROR, ("SubmitIrpSync failed = 0x%x\n", ntStatus));
        TRAP;
    }

    ExFreePool(pIrb);

Exit_IsochAllocateResources:

    EXIT("t1394Diag_IsochAllocateResources", ntStatus);
    return(ntStatus);
} // t1394Diag_IsochAllocateResources

NTSTATUS
t1394Diag_IsochAttachBuffers(
    IN PDEVICE_OBJECT               DeviceObject,
    IN PIRP                         Irp,
    IN ULONG                        outputBufferLength,
    IN HANDLE                       hResource,
    IN ULONG                        nNumberOfDescriptors,
    OUT PISOCH_DESCRIPTOR           pIsochDescriptor,
    IN OUT PRING3_ISOCH_DESCRIPTOR  R3_IsochDescriptor
    )
{
    NTSTATUS                    ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION           deviceExtension = DeviceObject->DeviceExtension;
    PIRB                        pIrb;
    ULONG                       i;
    PISOCH_DETACH_DATA          pIsochDetachData;
    PRING3_ISOCH_DESCRIPTOR     pR3TempDescriptor;
    KIRQL                       Irql;
    PIO_STACK_LOCATION          NextIrpStack;
    LARGE_INTEGER               deltaTime;

    ENTER("t1394Diag_IsochAttachBuffers");

    TRACE(TL_TRACE, ("outputBufferLength = 0x%x\n", outputBufferLength));
    TRACE(TL_TRACE, ("hResource = 0x%x\n", hResource));
    TRACE(TL_TRACE, ("nNumberOfDescriptors = 0x%x\n", nNumberOfDescriptors));
    TRACE(TL_TRACE, ("R3_IsochDescriptor = 0x%x\n", R3_IsochDescriptor));

    //
    // allocate the irb
    //
    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochAttachBuffers;
    } // if

    //
    // allocate our isoch descriptors
    //
    pIsochDescriptor = ExAllocatePool(NonPagedPool, sizeof(ISOCH_DESCRIPTOR)*nNumberOfDescriptors);

    if (!pIsochDescriptor) {

        TRACE(TL_ERROR, ("Failed to allocate pIsochDescriptor!\n"));
        TRAP;

        if (pIrb)
            ExFreePool(pIrb);

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochAttachBuffers;
    }

    KeAcquireSpinLock(&deviceExtension->IsochSpinLock, &Irql);

    pR3TempDescriptor = R3_IsochDescriptor;

    for (i=0;i < nNumberOfDescriptors; i++) {

		TRACE(TL_TRACE, ("pR3TempDescriptor = 0x%x\n", pR3TempDescriptor));
        TRACE(TL_TRACE, ("pR3TempDescriptor->fulFlags = 0x%x\n", pR3TempDescriptor->fulFlags));
        TRACE(TL_TRACE, ("pR3TempDescriptor->ulLength = 0x%x\n", pR3TempDescriptor->ulLength));
        TRACE(TL_TRACE, ("pR3TempDescriptor->nMaxBytesPerFrame = 0x%x\n", pR3TempDescriptor->nMaxBytesPerFrame));
        TRACE(TL_TRACE, ("pR3TempDescriptor->ulSynch = 0x%x\n", pR3TempDescriptor->ulSynch));
        TRACE(TL_TRACE, ("pR3TempDescriptor->ulTag = 0x%x\n", pR3TempDescriptor->ulTag));
        TRACE(TL_TRACE, ("pR3TempDescriptor->CycleTime.CL_CycleOffset = 0x%x\n", pR3TempDescriptor->CycleTime.CL_CycleOffset));
        TRACE(TL_TRACE, ("pR3TempDescriptor->CycleTime.CL_CycleCount = 0x%x\n", pR3TempDescriptor->CycleTime.CL_CycleCount));
        TRACE(TL_TRACE, ("pR3TempDescriptor->CycleTime.CL_SecondCount = 0x%x\n", pR3TempDescriptor->CycleTime.CL_SecondCount));
        TRACE(TL_TRACE, ("pR3TempDescriptor->Data = 0x%x\n", &pR3TempDescriptor->Data));

        TRACE(TL_TRACE, ("pIsochDescriptor[%x] = 0x%x\n", i, &pIsochDescriptor[i]));

        pIsochDescriptor[i].Mdl = MmCreateMdl(NULL, pR3TempDescriptor->Data, pR3TempDescriptor->ulLength);
        MmBuildMdlForNonPagedPool(pIsochDescriptor[i].Mdl);

        pIsochDescriptor[i].fulFlags = pR3TempDescriptor->fulFlags;
        pIsochDescriptor[i].ulLength = MmGetMdlByteCount(pIsochDescriptor[i].Mdl);
        pIsochDescriptor[i].nMaxBytesPerFrame = pR3TempDescriptor->nMaxBytesPerFrame;
        pIsochDescriptor[i].ulSynch = pR3TempDescriptor->ulSynch;
        pIsochDescriptor[i].ulTag = pR3TempDescriptor->ulTag;
        pIsochDescriptor[i].CycleTime = pR3TempDescriptor->CycleTime;

        if (pR3TempDescriptor->bUseCallback) {

            //
            // i'm hoping this is the last descriptor. they should have only set this in the
            // last descriptor, since elsewhere it's not supported.
            //
            if (i != nNumberOfDescriptors-1) {

                TRACE(TL_TRACE, ("Callback on descriptor prior to last!\n"));

                // setting callback to NULL
                pIsochDescriptor[i].Callback = NULL;
            }
            else {

                // need to save hResource, numDescriptors and Irp to use when detaching.
                // this needs to be done before we submit the irp, since the isoch callback
                // can be called before the submitirpsynch call completes.
                pIsochDetachData = ExAllocatePool(NonPagedPool, sizeof(ISOCH_DETACH_DATA));

                if (!pIsochDetachData) {

                    TRACE(TL_ERROR, ("Failed to allocate pIsochDetachData!\n"));
                    TRAP;

                    if (pIsochDescriptor) {
                        ExFreePool(pIsochDescriptor);
                    }

                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                    goto Exit_IsochAttachBuffers;
                }

                pIsochDetachData->Tag = ISOCH_DETACH_TAG;
                pIsochDetachData->AttachIrb = pIrb;

                InsertHeadList(&deviceExtension->IsochDetachData, &pIsochDetachData->IsochDetachList);

                KeInitializeTimer(&pIsochDetachData->Timer);
                KeInitializeDpc(&pIsochDetachData->TimerDpc, t1394Diag_IsochTimeout, pIsochDetachData);

#define REQUEST_BUSY_RETRY_VALUE        (ULONG)(-100 * 100 * 100 * 100) //80 msecs in units of 100nsecs

                deltaTime.LowPart = REQUEST_BUSY_RETRY_VALUE;
                deltaTime.HighPart = -1;
                KeSetTimer(&pIsochDetachData->Timer, deltaTime, &pIsochDetachData->TimerDpc);

                pIsochDetachData->outputBufferLength = outputBufferLength;
                pIsochDetachData->DeviceExtension = deviceExtension;
                pIsochDetachData->hResource = hResource;
                pIsochDetachData->numIsochDescriptors = nNumberOfDescriptors;
                pIsochDetachData->IsochDescriptor = pIsochDescriptor;
                pIsochDetachData->Irp = Irp;
                pIsochDetachData->bDetach = pR3TempDescriptor->bAutoDetach;

                pIsochDescriptor[i].Callback = t1394Diag_IsochCallback;

                pIsochDescriptor[i].Context1 = deviceExtension;
                pIsochDescriptor[i].Context2 = pIsochDetachData;

                TRACE(TL_TRACE, ("IsochAttachBuffers: pIsochDetachData = 0x%x\n", pIsochDetachData));
                TRACE(TL_TRACE, ("IsochAttachBuffers: pIsochDetachData->Irp = 0x%x\n", pIsochDetachData->Irp));
            }
        }
        else {

            pIsochDescriptor[i].Callback = NULL;
        }

        TRACE(TL_TRACE, ("pIsochDescriptor[%x].fulFlags = 0x%x\n", i, pIsochDescriptor[i].fulFlags));
        TRACE(TL_TRACE, ("pIsochDescriptor[%x].ulLength = 0x%x\n", i, pIsochDescriptor[i].ulLength));
        TRACE(TL_TRACE, ("pIsochDescriptor[%x].nMaxBytesPerFrame = 0x%x\n", i, pIsochDescriptor[i].nMaxBytesPerFrame));
        TRACE(TL_TRACE, ("pIsochDescriptor[%x].ulSynch = 0x%x\n", i, pIsochDescriptor[i].ulSynch));
        TRACE(TL_TRACE, ("pIsochDescriptor[%x].ulTag = 0x%x\n", i, pIsochDescriptor[i].ulTag));
        TRACE(TL_TRACE, ("pIsochDescriptor[%x].CycleTime.CL_CycleOffset = 0x%x\n", i, pIsochDescriptor[i].CycleTime.CL_CycleOffset));
        TRACE(TL_TRACE, ("pIsochDescriptor[%x].CycleTime.CL_CycleCount = 0x%x\n", i, pIsochDescriptor[i].CycleTime.CL_CycleCount));
        TRACE(TL_TRACE, ("pIsochDescriptor[%x].CycleTime.CL_SecondCount = 0x%x\n", i, pIsochDescriptor[i].CycleTime.CL_SecondCount));

        pR3TempDescriptor =
           (PRING3_ISOCH_DESCRIPTOR)((ULONG_PTR)pR3TempDescriptor +
                                     pIsochDescriptor[i].ulLength +
                                     sizeof(RING3_ISOCH_DESCRIPTOR));
    } // for

    KeReleaseSpinLock(&deviceExtension->IsochSpinLock, Irql);

    // lets make sure the device is still around
    // if it isn't, we free the irb and return, our pnp
    // cleanup will take care of everything else
    if (deviceExtension->bShutdown) {

        TRACE(TL_TRACE, ("Shutdown!\n"));
        ntStatus = STATUS_NO_SUCH_DEVICE;
        goto Exit_IsochAttachBuffers;
    }

    pIrb->FunctionNumber = REQUEST_ISOCH_ATTACH_BUFFERS;
    pIrb->Flags = 0;
    pIrb->u.IsochAttachBuffers.hResource = hResource;
    pIrb->u.IsochAttachBuffers.nNumberOfDescriptors = nNumberOfDescriptors;
    pIrb->u.IsochAttachBuffers.pIsochDescriptor = pIsochDescriptor;

    NextIrpStack = IoGetNextIrpStackLocation(Irp);
    NextIrpStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    NextIrpStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_1394_CLASS;
    NextIrpStack->Parameters.Others.Argument1 = pIrb;

    IoSetCompletionRoutine( Irp,
                            t1394Diag_IsochAttachCompletionRoutine,
                            pIsochDetachData,
                            TRUE,
                            TRUE,
                            TRUE
                            );

    IoCallDriver(deviceExtension->StackDeviceObject, Irp);

Exit_IsochAttachBuffers:

    if (ntStatus == STATUS_INSUFFICIENT_RESOURCES) {

        KeReleaseSpinLock(&deviceExtension->IsochSpinLock, Irql);
    }

    EXIT("t1394Diag_IsochAttachBuffers", ntStatus);
    return(STATUS_PENDING);
} // t1394Diag_IsochAttachBuffers

NTSTATUS
t1394Diag_IsochDetachBuffers(
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp,
    IN HANDLE               hResource,
    IN ULONG                nNumberOfDescriptors,
    IN PISOCH_DESCRIPTOR    IsochDescriptor
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIRB                pIrb;
    ULONG               i;

    ENTER("");

    TRACE(TL_TRACE, ("hResource = 0x%x\n", hResource));
    TRACE(TL_TRACE, ("nNumberOfDescriptors = 0x%x\n", nNumberOfDescriptors));
    TRACE(TL_TRACE, ("IsochDescriptor = 0x%x\n", IsochDescriptor));

    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochDetachBuffers;
    } // if

    pIrb->FunctionNumber = REQUEST_ISOCH_DETACH_BUFFERS;
    pIrb->Flags = 0;
    pIrb->u.IsochDetachBuffers.hResource = hResource;
    pIrb->u.IsochDetachBuffers.nNumberOfDescriptors = nNumberOfDescriptors;
    pIrb->u.IsochDetachBuffers.pIsochDescriptor = IsochDescriptor;

    ntStatus = t1394Diag_SubmitIrpSynch(deviceExtension->StackDeviceObject, Irp, pIrb);

    if (!NT_SUCCESS(ntStatus)) {

        TRACE(TL_ERROR, ("SubmitIrpSync failed = 0x%x\n", ntStatus));
        TRAP;
    }

    if (pIrb)
        ExFreePool(pIrb);

    for (i=0; i<nNumberOfDescriptors; i++)
        ExFreePool(IsochDescriptor[i].Mdl);
    	
    ExFreePool(IsochDescriptor);

Exit_IsochDetachBuffers:

    EXIT("t1394Diag_IsochDetachBuffers", ntStatus);
    return(ntStatus);
} // t1394Diag_IsochDetachBuffers

NTSTATUS
t1394Diag_IsochFreeBandwidth(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN HANDLE           hBandwidth
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIRB                pIrb;

    ENTER("t1394Diag_IsochFreeBandwidth");

    TRACE(TL_TRACE, ("hBandwidth = 0x%x\n", hBandwidth));

    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochFreeBandwidth;
    } // if

    pIrb->FunctionNumber = REQUEST_ISOCH_FREE_BANDWIDTH;
    pIrb->Flags = 0;
    pIrb->u.IsochFreeBandwidth.hBandwidth = hBandwidth;

	ntStatus = t1394Diag_SubmitIrpSynch(deviceExtension->StackDeviceObject, Irp, pIrb);

    if (!NT_SUCCESS(ntStatus)) {

        TRACE(TL_ERROR, ("SubmitIrpSync failed = 0x%x\n", ntStatus));
        TRAP;
    }
	
    ExFreePool(pIrb);

Exit_IsochFreeBandwidth:

    EXIT("t1394Diag_IsochFreeBandwidth", ntStatus);
    return(ntStatus);
} // t1394Diag_IsochFreeBandwidth

NTSTATUS
t1394Diag_IsochFreeChannel(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            nChannel
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIRB                pIrb;

    ENTER("t1394Diag_IsochFreeChannel");

    TRACE(TL_TRACE, ("nChannel = 0x%x\n", nChannel));

    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochFreeChannel;
    } // if

    pIrb->FunctionNumber = REQUEST_ISOCH_FREE_CHANNEL;
    pIrb->Flags = 0;
    pIrb->u.IsochFreeChannel.nChannel = nChannel;

    ntStatus = t1394Diag_SubmitIrpSynch(deviceExtension->StackDeviceObject, Irp, pIrb);

    if (!NT_SUCCESS(ntStatus)) {

        TRACE(TL_ERROR, ("SubmitIrpSync failed = 0x%x\n", ntStatus));
        TRAP;
    }
	
    ExFreePool(pIrb);

Exit_IsochFreeChannel:

    EXIT("t1394Diag_IsochFreeChannel", ntStatus);
    return(ntStatus);
} // t1394Diag_IsochFreeChannel

NTSTATUS
t1394Diag_IsochFreeResources(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN HANDLE           hResource
    )
{
    NTSTATUS                ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION       deviceExtension = DeviceObject->DeviceExtension;
    PIRB                    pIrb;
    PISOCH_RESOURCE_DATA    IsochResourceData;
    KIRQL                   Irql;

    ENTER("t1394Diag_IsochFreeResources");

    TRACE(TL_TRACE, ("hResource = 0x%x\n", hResource));

    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochFreeResources;
    } // if

    // remove this one from our list...
    KeAcquireSpinLock(&deviceExtension->IsochResourceSpinLock, &Irql);

    IsochResourceData = (PISOCH_RESOURCE_DATA)deviceExtension->IsochResourceData.Flink;

    while (IsochResourceData) {

        TRACE(TL_TRACE, ("Removing hResource = 0x%x\n", hResource));

        if (IsochResourceData->hResource == hResource) {

            RemoveEntryList(&IsochResourceData->IsochResourceList);
            ExFreePool(IsochResourceData);
            break;
        }
        else if (IsochResourceData->IsochResourceList.Flink == &deviceExtension->IsochResourceData) {
            break;
        }
        else
            IsochResourceData = (PISOCH_RESOURCE_DATA)IsochResourceData->IsochResourceList.Flink;
    }

    KeReleaseSpinLock(&deviceExtension->IsochResourceSpinLock, Irql);

    pIrb->FunctionNumber = REQUEST_ISOCH_FREE_RESOURCES;
    pIrb->Flags = 0;
    pIrb->u.IsochFreeResources.hResource = hResource;

    ntStatus = t1394Diag_SubmitIrpSynch(deviceExtension->StackDeviceObject, Irp, pIrb);

    if (!NT_SUCCESS(ntStatus)) {

        TRACE(TL_ERROR, ("SubmitIrpSync failed = 0x%x\n", ntStatus));
        TRAP;
    }

    ExFreePool(pIrb);

Exit_IsochFreeResources:

    EXIT("t1394Diag_IsochFreeResources", ntStatus);
    return(ntStatus);
} // t1394Diag_IsochFreeResources

NTSTATUS
t1394Diag_IsochListen(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN HANDLE           hResource,
    IN ULONG            fulFlags,
    IN CYCLE_TIME       StartTime
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIRB                pIrb;

    ENTER("t1394Diag_IsochListen");

    TRACE(TL_TRACE, ("hResource = 0x%x\n", hResource));
    TRACE(TL_TRACE, ("fulFlags = 0x%x\n", fulFlags));
    TRACE(TL_TRACE, ("StartTime.CL_CycleOffset = 0x%x\n", StartTime.CL_CycleOffset));
    TRACE(TL_TRACE, ("StartTime.CL_CycleCount = 0x%x\n", StartTime.CL_CycleCount));
    TRACE(TL_TRACE, ("StartTime.CL_SecondCount = 0x%x\n", StartTime.CL_SecondCount));

    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochListen;
    } // if

    pIrb->FunctionNumber = REQUEST_ISOCH_LISTEN;
    pIrb->Flags = 0;
    pIrb->u.IsochListen.hResource = hResource;
    pIrb->u.IsochListen.fulFlags = fulFlags;
    pIrb->u.IsochListen.StartTime = StartTime;

    ntStatus = t1394Diag_SubmitIrpSynch(deviceExtension->StackDeviceObject, Irp, pIrb);

    if (!NT_SUCCESS(ntStatus)) {

        TRACE(TL_ERROR, ("SubmitIrpSync failed = 0x%x\n", ntStatus));
        TRAP;
    }
	
    ExFreePool(pIrb);

Exit_IsochListen:

    EXIT("t1394Diag_IsochListen", ntStatus);
    return(ntStatus);
} // t1394Diag_IsochListen

NTSTATUS
t1394Diag_IsochQueryCurrentCycleTime(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    OUT PCYCLE_TIME     pCurrentCycleTime
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIRB                pIrb;

    ENTER("t1394Diag_IsochQueryCurrentCycleTime");

    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochQueryCurrentCycleTime;
    } // if

    pIrb->FunctionNumber = REQUEST_ISOCH_QUERY_CYCLE_TIME;
    pIrb->Flags = 0;

    ntStatus = t1394Diag_SubmitIrpSynch(deviceExtension->StackDeviceObject, Irp, pIrb);

    if (NT_SUCCESS(ntStatus)) {

        *pCurrentCycleTime = pIrb->u.IsochQueryCurrentCycleTime.CycleTime;
		
        TRACE(TL_TRACE, ("CurrentCycleTime.CL_CycleOffset = 0x%x\n", pCurrentCycleTime->CL_CycleOffset));
        TRACE(TL_TRACE, ("CurrentCycleTime.CL_CycleCount = 0x%x\n", pCurrentCycleTime->CL_CycleCount));
        TRACE(TL_TRACE, ("CurrentCycleTime.CL_SecondCount = 0x%x\n", pCurrentCycleTime->CL_SecondCount));
    }
    else {

        TRACE(TL_ERROR, ("SubmitIrpSync failed = 0x%x\n", ntStatus));
        TRAP;
    }

    ExFreePool(pIrb);

Exit_IsochQueryCurrentCycleTime:

    EXIT("t1394Diag_IsochQueryCurrentCycleTime", ntStatus);
    return(ntStatus);
} // t1394Diag_IsochQueryCurrentCycleTime

NTSTATUS
t1394Diag_IsochQueryResources(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            fulSpeed,
    OUT PULONG          pBytesPerFrameAvailable,
    OUT PLARGE_INTEGER  pChannelsAvailable
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIRB                pIrb;

    ENTER("t1394Diag_IsochQueryResources");

    TRACE(TL_TRACE, ("fulSpeed = 0x%x\n", fulSpeed));

    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochQueryResources;
    } // if

    pIrb->FunctionNumber = REQUEST_ISOCH_QUERY_RESOURCES;
    pIrb->Flags = 0;
    pIrb->u.IsochQueryResources.fulSpeed = fulSpeed;

    ntStatus = t1394Diag_SubmitIrpSynch(deviceExtension->StackDeviceObject, Irp, pIrb);

    if (NT_SUCCESS(ntStatus)) {

        *pBytesPerFrameAvailable = pIrb->u.IsochQueryResources.BytesPerFrameAvailable;
        *pChannelsAvailable = pIrb->u.IsochQueryResources.ChannelsAvailable;

        TRACE(TL_TRACE, ("BytesPerFrameAvailable = 0x%x\n", *pBytesPerFrameAvailable));
        TRACE(TL_TRACE, ("ChannelsAvailable.LowPart = 0x%x\n", pChannelsAvailable->LowPart));
        TRACE(TL_TRACE, ("ChannelsAvailable.HighPart = 0x%x\n", pChannelsAvailable->HighPart));
    }
    else {

        TRACE(TL_ERROR, ("SubmitIrpSync failed = 0x%x\n", ntStatus));
        TRAP;
    }

    ExFreePool(pIrb);

Exit_IsochQueryResources:

    EXIT("t1394Diag_IsochQueryResources", ntStatus);
    return(ntStatus);
} // t1394Diag_IsochQueryResources

NTSTATUS
t1394Diag_IsochSetChannelBandwidth(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN HANDLE           hBandwidth,
    IN ULONG            nMaxBytesPerFrame
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIRB                pIrb;

    ENTER("t1394Diag_IsochSetChannelBandwidth");

    TRACE(TL_TRACE, ("hBandwidth = 0x%x\n", hBandwidth));
    TRACE(TL_TRACE, ("nMaxBytesPerFrame = 0x%x\n", nMaxBytesPerFrame));

    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochSetChannelBandwidth;
    } // if

    pIrb->FunctionNumber = REQUEST_ISOCH_SET_CHANNEL_BANDWIDTH;
    pIrb->Flags = 0;
    pIrb->u.IsochSetChannelBandwidth.hBandwidth = hBandwidth;
    pIrb->u.IsochSetChannelBandwidth.nMaxBytesPerFrame = nMaxBytesPerFrame;

	ntStatus = t1394Diag_SubmitIrpSynch(deviceExtension->StackDeviceObject, Irp, pIrb);

    if (!NT_SUCCESS(ntStatus)) {

        TRACE(TL_ERROR, ("SubmitIrpSync failed = 0x%x\n", ntStatus));
        TRAP;
    }
	
    ExFreePool(pIrb);

Exit_IsochSetChannelBandwidth:

    EXIT("t1394Diag_IsochSetChannelBandwidth",  ntStatus);
    return(ntStatus);
} // t1394Diag_IsochSetChannelBandwidth

NTSTATUS
t1394Diag_IsochStop(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN HANDLE           hResource,
    IN ULONG            fulFlags
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIRB                pIrb;

    ENTER("t1394Diag_IsochStop");

    TRACE(TL_TRACE, ("hResource = 0x%x\n", hResource));
    TRACE(TL_TRACE, ("fulFlags = 0x%x\n", fulFlags));

    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochStop;
    } // if

    pIrb->FunctionNumber = REQUEST_ISOCH_STOP;
    pIrb->Flags = 0;
    pIrb->u.IsochStop.hResource = hResource;
    pIrb->u.IsochStop.fulFlags = fulFlags;

    ntStatus = t1394Diag_SubmitIrpSynch(deviceExtension->StackDeviceObject, Irp, pIrb);

    if (!NT_SUCCESS(ntStatus)) {

        TRACE(TL_ERROR, ("SubmitIrpSync failed = 0x%x\n", ntStatus));
        TRAP;
    }
	
    ExFreePool(pIrb);
	
Exit_IsochStop:

    EXIT("t1394Diag_IsochStop", ntStatus);
    return(ntStatus);
} // t1394Diag_IsochStop

NTSTATUS
t1394Diag_IsochTalk(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN HANDLE           hResource,
    IN ULONG            fulFlags,
    CYCLE_TIME          StartTime
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIRB                pIrb;

    ENTER("t1394Diag_IsochTalk");

    TRACE(TL_TRACE, ("hResource = 0x%x\n", hResource));
    TRACE(TL_TRACE, ("fulFlags = 0x%x\n", fulFlags));
    TRACE(TL_TRACE, ("StartTime.CL_CycleOffset = 0x%x\n", StartTime.CL_CycleOffset));
    TRACE(TL_TRACE, ("StartTime.CL_CycleCount = 0x%x\n", StartTime.CL_CycleCount));
    TRACE(TL_TRACE, ("StartTime.CL_SecondCount = 0x%x\n", StartTime.CL_SecondCount));

    pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

    if (!pIrb) {

        TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
        TRAP;

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit_IsochTalk;
    } // if

    pIrb->FunctionNumber = REQUEST_ISOCH_TALK;
    pIrb->Flags = 0;
    pIrb->u.IsochTalk.hResource = hResource;
    pIrb->u.IsochTalk.fulFlags = fulFlags;
    pIrb->u.IsochTalk.StartTime = StartTime;

    ntStatus = t1394Diag_SubmitIrpSynch(deviceExtension->StackDeviceObject, Irp, pIrb);

    if (!NT_SUCCESS(ntStatus)) {

        TRACE(TL_ERROR, ("SubmitIrpSync failed = 0x%x\n", ntStatus));
        TRAP;
    }
	
    ExFreePool(pIrb);

Exit_IsochTalk:

    EXIT("t1394Diag_IsochTalk", ntStatus);
    return(ntStatus);
} // t1394Diag_IsochTalk

void
t1394Diag_IsochCallback(
    IN PDEVICE_EXTENSION    DeviceExtension,
    IN PISOCH_DETACH_DATA   IsochDetachData
    )
{
    KIRQL               Irql;

    ENTER("t1394Diag_IsochCallback");

    if (!DeviceExtension->bShutdown) {

        KeAcquireSpinLock(&DeviceExtension->IsochSpinLock, &Irql);
        RemoveEntryList(&IsochDetachData->IsochDetachList);
        KeCancelTimer(&IsochDetachData->Timer);

        TRACE(TL_TRACE, ("IsochCallback: IsochDetachData = 0x%x\n", IsochDetachData));
        TRACE(TL_TRACE, ("IsochCallback: IsochDetachData->Irp = 0x%x\n", IsochDetachData->Irp));

        if (IsochDetachData->Tag != ISOCH_DETACH_TAG) {

            TRACE(TL_ERROR, ("Invalid Detach Tag!\n"));
            TRAP;

            KeReleaseSpinLock(&DeviceExtension->IsochSpinLock, Irql);
        }
        else {

            // clear the tag...
            IsochDetachData->Tag = 0;

            KeReleaseSpinLock(&DeviceExtension->IsochSpinLock, Irql);

            // need to save the status of the attach
            // we'll clean up in the same spot for success's and timeout's
            IsochDetachData->AttachStatus = IsochDetachData->Irp->IoStatus.Status;

            t1394Diag_IsochCleanup(IsochDetachData);
        }
    }

    EXIT("t1394Diag_IsochCallback", 0);
} // t1394Diag_IsochCallback

void
t1394Diag_IsochTimeout(
    IN PKDPC                Dpc,
    IN PISOCH_DETACH_DATA   IsochDetachData,
    IN PVOID                SystemArgument1,
    IN PVOID                SystemArgument2
    )
{
    KIRQL               Irql;
    PDEVICE_EXTENSION   DeviceExtension;

    ENTER("t1394Diag_IsochTimeout");

    TRACE(TL_WARNING, ("Isoch Timeout!\n"));

    DeviceExtension = IsochDetachData->DeviceExtension;

    if (!DeviceExtension->bShutdown) {

        KeAcquireSpinLock(&DeviceExtension->IsochSpinLock, &Irql);
        RemoveEntryList(&IsochDetachData->IsochDetachList);
        KeCancelTimer(&IsochDetachData->Timer);

        TRACE(TL_TRACE, ("IsochTimeout: IsochDetachData = 0x%x\n", IsochDetachData));
        TRACE(TL_TRACE, ("IsochTimeout: IsochDetachData->Irp = 0x%x\n", IsochDetachData->Irp));

        if (IsochDetachData->Tag != ISOCH_DETACH_TAG) {

            TRACE(TL_ERROR, ("Invalid Detach Tag!\n"));
            TRAP;

            KeReleaseSpinLock(&DeviceExtension->IsochSpinLock, Irql);
        }
        else {

            // clear the tag...
            IsochDetachData->Tag = 0;

            KeReleaseSpinLock(&DeviceExtension->IsochSpinLock, Irql);

            // need to save the status of the attach
            // we'll clean up in the same spot for success's and timeout's
            IsochDetachData->AttachStatus = STATUS_TIMEOUT;

            t1394Diag_IsochCleanup(IsochDetachData);
        }
    }

    EXIT("t1394Diag_IsochTimeout", 0);
} // t1394Diag_IsochTimeout

void
t1394Diag_IsochCleanup(
    IN PISOCH_DETACH_DATA   IsochDetachData
    )
{
    ULONG               i;
    PDEVICE_EXTENSION   DeviceExtension;

    ENTER("t1394Diag_IsochCleanup");

    DeviceExtension = IsochDetachData->DeviceExtension;

    //
    // see if we need to detach this buffer
    //
    if (IsochDetachData->bDetach) {

        PIRB                pIrb;
        NTSTATUS            ntStatus;
        PIO_STACK_LOCATION  NextIrpStack;

        pIrb = ExAllocatePool(NonPagedPool, sizeof(IRB));

        if (!pIrb) {

            TRACE(TL_ERROR, ("Failed to allocate pIrb!\n"));
            TRACE(TL_WARNING, ("Can't detach buffer!\n"));
            TRAP;

            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto Exit_IsochDetachBuffers;
        } // if

        // save the irb in our detach data context
        IsochDetachData->DetachIrb = pIrb;

        pIrb->FunctionNumber = REQUEST_ISOCH_DETACH_BUFFERS;
        pIrb->Flags = 0;
        pIrb->u.IsochDetachBuffers.hResource = IsochDetachData->hResource;
        pIrb->u.IsochDetachBuffers.nNumberOfDescriptors = IsochDetachData->numIsochDescriptors;
        pIrb->u.IsochDetachBuffers.pIsochDescriptor = IsochDetachData->IsochDescriptor;

        NextIrpStack = IoGetNextIrpStackLocation(IsochDetachData->Irp);
        NextIrpStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
        NextIrpStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_1394_CLASS;
        NextIrpStack->Parameters.Others.Argument1 = pIrb;

        IoSetCompletionRoutine( IsochDetachData->Irp,
                                t1394Diag_IsochDetachCompletionRoutine,
                                IsochDetachData,
                                TRUE,
                                TRUE,
                                TRUE
                                );

        IoCallDriver(DeviceExtension->StackDeviceObject, IsochDetachData->Irp);
    }
    else {

Exit_IsochDetachBuffers:

        TRACE(TL_TRACE, ("Complete Irp.\n"));

        if (IsochDetachData->AttachIrb)
            ExFreePool(IsochDetachData->AttachIrb);

        for (i=0; i<IsochDetachData->numIsochDescriptors; i++)
            ExFreePool(IsochDetachData->IsochDescriptor[i].Mdl);

        ExFreePool(IsochDetachData->IsochDescriptor);

        IsochDetachData->Irp->IoStatus.Status = IsochDetachData->AttachStatus;

        // only set this if its a success...
        if (NT_SUCCESS(IsochDetachData->AttachStatus))
            IsochDetachData->Irp->IoStatus.Information = IsochDetachData->outputBufferLength;

        IoCompleteRequest(IsochDetachData->Irp, IO_NO_INCREMENT);

        // all done with IsochDetachData, lets deallocate it...
        ExFreePool(IsochDetachData);
    }

    EXIT("t1394Diag_IsochCleanup", 0);
} // t1394Diag_IsochCleanup

NTSTATUS
t1394Diag_IsochDetachCompletionRoutine(
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp,
    IN PISOCH_DETACH_DATA   IsochDetachData
    )
{
    NTSTATUS        ntStatus = STATUS_SUCCESS;
    ULONG           i;

    ENTER("t1394Diag_IsochDetachCompletionRoutine");

    if (IsochDetachData->DetachIrb)
        ExFreePool(IsochDetachData->DetachIrb);

    TRACE(TL_TRACE, ("Complete Irp.\n"));

    if (IsochDetachData->AttachIrb)
        ExFreePool(IsochDetachData->AttachIrb);

    for (i=0; i<IsochDetachData->numIsochDescriptors; i++)
        ExFreePool(IsochDetachData->IsochDescriptor[i].Mdl);
    	
    ExFreePool(IsochDetachData->IsochDescriptor);

    // only set this if its a success...
    if (NT_SUCCESS(IsochDetachData->AttachStatus))
        IsochDetachData->Irp->IoStatus.Information = IsochDetachData->outputBufferLength;

    IsochDetachData->Irp->IoStatus.Status = IsochDetachData->AttachStatus;

    IoCompleteRequest(IsochDetachData->Irp, IO_NO_INCREMENT);

    // all done with IsochDetachData, lets deallocate it...
    ExFreePool(IsochDetachData);
    
    EXIT("t1394Diag_IsochDetachCompletionRoutine", ntStatus);
    return(STATUS_MORE_PROCESSING_REQUIRED);
} // t1394Diag_IsochDetachCompletionRoutine

NTSTATUS
t1394Diag_IsochAttachCompletionRoutine(
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp,
    IN PISOCH_DETACH_DATA   IsochDetachData
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    ULONG               i;
    KIRQL               Irql;
    PDEVICE_EXTENSION   DeviceExtension;

    ENTER("t1394Diag_IsochAttachCompletionRoutine");

    if (Irp->IoStatus.Status) {

        TRACE(TL_ERROR, ("Isoch Attach Failed! = 0x%x\n", Irp->IoStatus.Status));
//        TRAP;

        if (!IsochDetachData) {
        
            goto Exit_IsochAttachCompletionRoutine;
        }

        DeviceExtension = IsochDetachData->DeviceExtension;

        if (!DeviceExtension->bShutdown) {

            KeAcquireSpinLock(&DeviceExtension->IsochSpinLock, &Irql);
            RemoveEntryList(&IsochDetachData->IsochDetachList);
            KeCancelTimer(&IsochDetachData->Timer);

            TRACE(TL_TRACE, ("IsochAttachCompletionRoutine: IsochDetachData = 0x%x\n", IsochDetachData));
            TRACE(TL_TRACE, ("IsochAttachCompletionRoutine: IsochDetachData->Irp = 0x%x\n", IsochDetachData->Irp));

            if (IsochDetachData->Tag != ISOCH_DETACH_TAG) {

                TRACE(TL_ERROR, ("Invalid Detach Tag!\n"));
                TRAP;

                KeReleaseSpinLock(&DeviceExtension->IsochSpinLock, Irql);
            }
            else {

                // clear the tag...
                IsochDetachData->Tag = 0;

                KeReleaseSpinLock(&DeviceExtension->IsochSpinLock, Irql);

                TRACE(TL_TRACE, ("Complete Irp.\n"));

                if (IsochDetachData->AttachIrb)
                    ExFreePool(IsochDetachData->AttachIrb);

                for (i=0; i<IsochDetachData->numIsochDescriptors; i++)
                    ExFreePool(IsochDetachData->IsochDescriptor[i].Mdl);
    	
                ExFreePool(IsochDetachData->IsochDescriptor);

                IoCompleteRequest(Irp, IO_NO_INCREMENT);

                // all done with IsochDetachData, lets deallocate it...
                ExFreePool(IsochDetachData);
            }
        }
    }

Exit_IsochAttachCompletionRoutine:

    EXIT("t1394Diag_IsochAttachCompletionRoutine", ntStatus);
    return(STATUS_MORE_PROCESSING_REQUIRED);
} // t1394Diag_IsochAttachCompletionRoutine


