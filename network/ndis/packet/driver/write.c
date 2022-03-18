/*++

Copyright (c) 1990-2000  Microsoft Corporation

Module Name:

    write.c

Abstract:


Author:


Environment:

    Kernel mode only.

Notes:


Future:



Revision History:

    Updated for Windows 2000 - Eliyas Yakub June, 1999

--*/

#include "ntddk.h"
#include "ndis.h"
#include "packet.h"


NTSTATUS
PacketWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the dispatch routine for create/open and close requests.
    These requests complete successfully.

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/

{
    POPEN_INSTANCE      open;
    PNDIS_PACKET        pPacket;
    NDIS_STATUS         Status;

    DebugPrint(("SendAdapter\n"));


    open = DeviceObject->DeviceExtension;

    IoIncrement(open);

    //
    // Check to see whether you are still bound to the adapter
    //
    
    if(!open->Bound)
    {
        Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        IoDecrement(open);
        return STATUS_UNSUCCESSFUL;
    }


    //
    //  Try to get a packet from our list of free ones
    //

    NdisAllocatePacket(
        &Status,
        &pPacket,
        open->PacketPool
        );

    if (Status != NDIS_STATUS_SUCCESS) {

        //
        //  No free packets
        //
        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        IoDecrement(open);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RESERVED(pPacket)->Irp=Irp;

    //
    //  Attach the writes buffer to the packet
    //
    NdisChainBufferAtFront(pPacket,Irp->MdlAddress);

    //
    // Important: Since we have marked the IRP pending, we must return 
    // STATUS_PENDING even we happen to complete the IRP synchronously.
    // 

    IoMarkIrpPending(Irp);

    NdisSend(
        &Status,
        open->AdapterHandle,
        pPacket);


    if (Status != NDIS_STATUS_PENDING) {
        //
        //  The send didn't pend so call the completion handler now
        //
        PacketSendComplete(
            open,
            pPacket,
            Status
            );
    }

    return STATUS_PENDING;

}

VOID
PacketSendComplete(
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN PNDIS_PACKET  pPacket,
    IN NDIS_STATUS   Status
    )
/*++

Routine Description:

    This is a required function. PacketSendComplete 
    is called for each packet transmitted with a 
    call to NdisSend that returned NDIS_STATUS_PENDING 
    as the     status of the send operation. If an 
    array of packets is sent, PacketSendComplete 
    is called once for each packet passed to NdisSendPackets, 
    whether or not it returned pending. 

Arguments:


Return Value:


--*/
{
    PIRP                irp;
    PIO_STACK_LOCATION  irpSp;

    DebugPrint(("Packet: SendComplete :%x\n", Status));

    irp=RESERVED(pPacket)->Irp;
    irpSp = IoGetCurrentIrpStackLocation(irp);

    //
    //  Put the packet back on the free list
    //
    NdisFreePacket(pPacket);

    if(Status == NDIS_STATUS_SUCCESS) {
        irp->IoStatus.Information = irpSp->Parameters.Write.Length;
        irp->IoStatus.Status = STATUS_SUCCESS;        
    } else {
        irp->IoStatus.Information = 0;
        irp->IoStatus.Status = STATUS_UNSUCCESSFUL;        
    }

    IoCompleteRequest(irp, IO_NO_INCREMENT);
    IoDecrement((POPEN_INSTANCE)ProtocolBindingContext);
    return;

}


