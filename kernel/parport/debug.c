//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1998 - 1999
//
//  File:       debug.c
//
//--------------------------------------------------------------------------

#include "pch.h"

PCHAR PnpIrpName[] = {
    "IRP_MN_START_DEVICE                 0x00",
    "IRP_MN_QUERY_REMOVE_DEVICE          0x01",
    "IRP_MN_REMOVE_DEVICE                0x02",
    "IRP_MN_CANCEL_REMOVE_DEVICE         0x03",
    "IRP_MN_STOP_DEVICE                  0x04",
    "IRP_MN_QUERY_STOP_DEVICE            0x05",
    "IRP_MN_CANCEL_STOP_DEVICE           0x06",
    "IRP_MN_QUERY_DEVICE_RELATIONS       0x07",
    "IRP_MN_QUERY_INTERFACE              0x08",
    "IRP_MN_QUERY_CAPABILITIES           0x09",
    "IRP_MN_QUERY_RESOURCES              0x0A",
    "IRP_MN_QUERY_RESOURCE_REQUIREMENTS  0x0B",
    "IRP_MN_QUERY_DEVICE_TEXT            0x0C",
    "IRP_MN_FILTER_RESOURCE_REQUIREMENTS 0x0D",
    " unused MinorFunction               0x0E",
    "IRP_MN_READ_CONFIG                  0x0F",
    "IRP_MN_WRITE_CONFIG                 0x10",
    "IRP_MN_EJECT                        0x11",
    "IRP_MN_SET_LOCK                     0x12",
    "IRP_MN_QUERY_ID                     0x13",
    "IRP_MN_QUERY_PNP_DEVICE_STATE       0x14",
    "IRP_MN_QUERY_BUS_INFORMATION        0x15",
    "IRP_MN_DEVICE_USAGE_NOTIFICATION    0x16",
    "IRP_MN_SURPRISE_REMOVAL             0x17",
    "IRP_MN_QUERY_LEGACY_BUS_INFORMATION 0x18"
};


VOID
PptDebugDumpPnpIrpInfo(PDEVICE_OBJECT DeviceObject, PIRP Irp) 
{
    PDEVICE_EXTENSION  extension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpStack  = IoGetCurrentIrpStackLocation( Irp );
    PCHAR irpName;

    if( irpStack->MinorFunction <= IRP_MN_QUERY_LEGACY_BUS_INFORMATION ) {
        irpName = PnpIrpName[ irpStack->MinorFunction ];
    } else {
        PptDumpV( ("MinorFunction =          %#02x\n", irpStack->MinorFunction) );
        irpName = " - MinorFunction > 0x18 - don't handle  ";
    }
    PptDumpV( ("PptDebugDumpPnpIrpInfo(): PNP %s - DeviceObject= %x - [%S]\n",
               irpName, DeviceObject, extension->PnpInfo.PortName) );
}


// temp debug for remlock problem - begin
NTSTATUS
PptAcquireRemoveLock(
    IN PIO_REMOVE_LOCK RemoveLock,
    IN PVOID           Tag OPTIONAL
    )
{
    NTSTATUS status;
    // PptDumpV( ("PptAcquireRemoveLock: RemoveLock= %x , Tag= %x\n", RemoveLock, Tag) );
    status = IoAcquireRemoveLock(RemoveLock, Tag);
    // status = STATUS_SUCCESS;
    return status;
}

VOID
PptReleaseRemoveLock(
    IN PIO_REMOVE_LOCK RemoveLock,
    IN PVOID           Tag OPTIONAL
    )
{
    // PptDumpV( ("PptReleaseRemoveLock: RemoveLock= %x , Tag= %x\n", RemoveLock, Tag) );
    IoReleaseRemoveLock(RemoveLock, Tag);
}

VOID
PptReleaseRemoveLockAndWait(
    IN PIO_REMOVE_LOCK RemoveLock,
    IN PVOID           Tag
    )
{
    // PptDumpV( ("PptReleaseRemoveLockAndWait: RemoveLock= %x , Tag= %x\n", RemoveLock, Tag) );    
    IoReleaseRemoveLockAndWait(RemoveLock, Tag);
}
// temp debug for remlock problem - end

VOID
PptDebugDumpResourceList(
    PIO_RESOURCE_LIST ResourceList
    )
{
    ULONG count = ResourceList->Count;
    ULONG i;
    PIO_RESOURCE_DESCRIPTOR curDesc;

    PptDump2(PARRESOURCE,("Enter PptDebugDumpResourceList() - Count= %d - Descriptors:\n", count));

    for( i=0, curDesc=ResourceList->Descriptors ; i < count ; ++i, ++curDesc ) {
        switch (curDesc->Type) {
        case CmResourceTypeInterrupt :
            PptDump2(PARRESOURCE,(" i=%d, IRQ   Resource\n",i));
            break;
        case CmResourceTypeDma :
            PptDump2(PARRESOURCE,(" i=%d, DMA   Resource\n",i));
            break;
        case CmResourceTypePort :
            PptDump2(PARRESOURCE,(" i=%d, Port  Resource\n",i));
            break;
        case CmResourceTypeNull :
            PptDump2(PARRESOURCE,(" i=%d, Null  Resource\n",i));
            break;
        default:
            PptDump2(PARRESOURCE,(" i=%d, Other Resource\n",i));
        }
    }
}

VOID
PptDebugDumpResourceRequirementsList(
    PIO_RESOURCE_REQUIREMENTS_LIST ResourceRequirementsList
    )
{
    ULONG listCount = ResourceRequirementsList->AlternativeLists;
    PIO_RESOURCE_LIST curList;
    ULONG i;

    PptDump2(PARRESOURCE,("Enter PptDebugDumpResourceRequirementsList() - AlternativeLists= %d\n", listCount));

    i=0;
    curList = ResourceRequirementsList->List;
    while( i < listCount ) {
        PptDump2(PARRESOURCE,("list i=%d, curList= %x\n", i,curList));
        PptDebugDumpResourceList(curList);
        curList = (PIO_RESOURCE_LIST)(curList->Descriptors + curList->Count);
        ++i;
    }
}

