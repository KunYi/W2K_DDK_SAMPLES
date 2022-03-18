/*++

Copyright (c) 1990-2000  Microsoft Corporation

Module Name:

    packet.h

Abstract:

Author:

Revision History:

    Converted to Windows 2000 - Eliyas Yakub 

--*/
#ifndef __PACKET_H
#define __PACKET_H

#undef  ExAllocatePool
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a, b, 'kcaP')

#if DBG
#define DebugPrint(_x_) \
                DbgPrint("PACKET: ");\
                DbgPrint _x_;

#else

#define DebugPrint(_x_) 

#endif

#define NT_DEVICE_NAME L"\\Device\\Packet"
#define DOS_DEVICE_NAME L"\\DosDevices\\Packet"

typedef struct _GLOBAL
{
    PDRIVER_OBJECT DriverObject;

    NDIS_HANDLE    NdisProtocolHandle;

    // 
    // Path to the driver's Services Key in the registry
    //

    UNICODE_STRING RegistryPath;

    //
    // List of deviceobjecs that are created for every
    // adapter we bind to.
    //

    LIST_ENTRY  AdapterList;
    KSPIN_LOCK  GlobalLock; // To synchronize access to the list.

    //
    // Control deviceObject for the driver.
    //

    PDEVICE_OBJECT  ControlDeviceObject;

} GLOBAL, *PGLOBAL;

GLOBAL Globals;


typedef struct _INTERNAL_REQUEST {

    PIRP           Irp;
    NDIS_REQUEST   Request;

} INTERNAL_REQUEST, *PINTERNAL_REQUEST;


typedef struct _OPEN_INSTANCE {

    PDEVICE_OBJECT      DeviceObject;

    ULONG               IrpCount;

    NDIS_STRING         AdapterName;

    NDIS_STRING         SymbolicLink;
    
    NDIS_HANDLE         AdapterHandle;

    NDIS_HANDLE         PacketPool;

    KSPIN_LOCK          RcvQSpinLock;
    LIST_ENTRY          RcvList;

    NDIS_MEDIUM         Medium;

    KSPIN_LOCK          ResetQueueLock;
    LIST_ENTRY          ResetIrpList;

    NDIS_STATUS         Status;   

    NDIS_EVENT          Event;     

    NDIS_EVENT          CleanupEvent;

    //
    // List entry to link to the other deviceobjects.
    //

    LIST_ENTRY          AdapterListEntry;

    BOOLEAN             Bound; // Set to TRUE when OpenAdapter is complete
                               // Set to FALSE when CloseAdpater is complete
    CHAR                Filler[3];

} OPEN_INSTANCE, *POPEN_INSTANCE;



typedef struct _PACKET_RESERVED {
    LIST_ENTRY     ListElement;
    PIRP           Irp;
    PMDL           pMdl;
}  PACKET_RESERVED, *PPACKET_RESERVED;


#define  ETHERNET_HEADER_LENGTH   14

#define RESERVED(_p) ((PPACKET_RESERVED)((_p)->ProtocolReserved))

#define  TRANSMIT_PACKETS    16


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );
    
NTSTATUS
PacketCancelReadIrps(
    IN PDEVICE_OBJECT DeviceObject
);

NTSTATUS
PacketCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );
    
VOID
PacketBindAdapter(
    OUT PNDIS_STATUS            Status,
    IN  NDIS_HANDLE             BindContext,
    IN  PNDIS_STRING            DeviceName,
    IN  PVOID                   SystemSpecific1,
    IN  PVOID                   SystemSpecific2
    );
VOID
PacketUnbindAdapter(
    OUT PNDIS_STATUS        Status,
    IN  NDIS_HANDLE            ProtocolBindingContext,
    IN  NDIS_HANDLE            UnbindContext
    );
    

VOID
PacketOpenAdapterComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  Status,
    IN NDIS_STATUS  OpenErrorStatus
    );

VOID
PacketCloseAdapterComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  Status
    );


NDIS_STATUS
PacketReceiveIndicate(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookAheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    );

VOID
PacketReceiveComplete(
    IN NDIS_HANDLE  ProtocolBindingContext
    );


VOID
PacketRequestComplete(
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN PNDIS_REQUEST pRequest,
    IN NDIS_STATUS   Status
    );

VOID
PacketSendComplete(
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN PNDIS_PACKET  pPacket,
    IN NDIS_STATUS   Status
    );


VOID
PacketResetComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  Status
    );


VOID
PacketStatus(
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN NDIS_STATUS   Status,
    IN PVOID         StatusBuffer,
    IN UINT          StatusBufferSize
    );


VOID
PacketStatusComplete(
    IN NDIS_HANDLE  ProtocolBindingContext
    );

VOID
PacketTransferDataComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    );


NTSTATUS
PacketShutdown(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
PacketUnload(
    IN PDRIVER_OBJECT DriverObject
    );



NTSTATUS
PacketOpen(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
PacketClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
PacketWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
PacketRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
PacketIoControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
PacketCancelRoutine (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );
    
INT
PacketReceivePacket(
    IN    NDIS_HANDLE            ProtocolBindingContext,
    IN    PNDIS_PACKET        Packet
    );

NTSTATUS
PacketGetAdapterList(
    IN  PVOID              Buffer,
    IN  ULONG              Length,
    IN  OUT PULONG          DataLength
    );
    
NDIS_STATUS
PacketPNPHandler(
    IN    NDIS_HANDLE        ProtocolBindingContext,
    IN    PNET_PNP_EVENT    pNetPnPEvent
    );


VOID
IoIncrement (
    IN  OUT POPEN_INSTANCE  Open
    );

VOID
IoDecrement (
    IN  OUT POPEN_INSTANCE  Open
    );
    
#endif


