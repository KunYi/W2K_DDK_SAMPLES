/*++

Copyright (c) 1998 Microsoft Corporation

Module Name:

    1394diag.h

Abstract:

Author:

    Peter Binder (pbinder) 4/13/98

Revision History:
Date     Who       What
-------- --------- ------------------------------------------------------------
4/13/98  pbinder   taken from original 1394diag...
--*/

#include <basetyps.h>

#define MAX_SUFFIX_SIZE                     4*sizeof(WCHAR)
#define T1394DIAG_DEVICE_NAME               L"\\Device\\1394DIAG"
#define T1394DIAG_SYMBOLIC_LINK_NAME        L"\\DosDevices\\1394DIAG"

// {C459DF55-DB08-11d1-B009-00A0C9081FF6}

DEFINE_GUID(GUID_1394DIAG, 0xc459df55, 0xdb08, 0x11d1, 0xb0, 0x9, 0x0, 0xa0, 0xc9, 0x8, 0x1f, 0xf6);

#define GUID_1394DIAG_STR                   "C459DF55-DB08-11d1-B009-00A0C9081FF6"

// these guys are meant to be called from a ring 3 app
// call through the port device object 
#define IOCTL_1394_TOGGLE_ENUM_TEST_ON          CTL_CODE(               \
                                                FILE_DEVICE_UNKNOWN,    \
                                                0x88,                   \
                                                METHOD_BUFFERED,        \
                                                FILE_ANY_ACCESS         \
                                                )

#define IOCTL_1394_TOGGLE_ENUM_TEST_OFF         CTL_CODE(               \
                                                FILE_DEVICE_UNKNOWN,    \
                                                0x89,                   \
                                                METHOD_BUFFERED,        \
                                                FILE_ANY_ACCESS         \
                                                )

//
// IOCTL info, needs to be visible for application
//
#define DIAG1394_IOCTL_INDEX                            0x0800


#define IOCTL_ALLOCATE_ADDRESS_RANGE                    CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 0,       \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_FREE_ADDRESS_RANGE                        CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 1,       \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ASYNC_READ                                CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 2,       \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ASYNC_WRITE                               CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 3,       \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ASYNC_LOCK                                CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 4,       \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_ALLOCATE_BANDWIDTH                  CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 5,       \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_ALLOCATE_CHANNEL                    CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 6,       \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_ALLOCATE_RESOURCES                  CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 7,       \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_ATTACH_BUFFERS                      CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 8,       \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_DETACH_BUFFERS                      CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 9,       \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_FREE_BANDWIDTH                      CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 10,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_FREE_CHANNEL                        CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 11,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_FREE_RESOURCES                      CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 12,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_LISTEN                              CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 13,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_QUERY_CURRENT_CYCLE_TIME            CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 14,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_QUERY_RESOURCES                     CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 15,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_SET_CHANNEL_BANDWIDTH               CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 16,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_STOP                                CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 17,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ISOCH_TALK                                CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 18,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_GET_LOCAL_HOST_INFORMATION                CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 19,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_GET_1394_ADDRESS_FROM_DEVICE_OBJECT       CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 20,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_CONTROL                                   CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 21,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_GET_MAX_SPEED_BETWEEN_DEVICES             CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 22,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_SET_DEVICE_XMIT_PROPERTIES                CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 23,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_GET_CONFIGURATION_INFORMATION             CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 24,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_BUS_RESET                                 CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 25,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_GET_GENERATION_COUNT                      CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 26,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_SEND_PHY_CONFIGURATION_PACKET             CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 27,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_BUS_RESET_NOTIFICATION                    CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 28,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_ASYNC_STREAM                              CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 29,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_SET_LOCAL_HOST_INFORMATION                CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 30,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_SET_ADDRESS_DATA                          CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 40,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_BUS_RESET_NOTIFY                          CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 50,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#define IOCTL_GET_DIAG_VERSION                          CTL_CODE( FILE_DEVICE_UNKNOWN,  \
                                                        DIAG1394_IOCTL_INDEX + 51,      \
                                                        METHOD_BUFFERED,                \
                                                        FILE_ANY_ACCESS)

#ifdef DRIVER

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT          StackDeviceObject;
    PDEVICE_OBJECT          PortDeviceObject;
    UNICODE_STRING          SymbolicLinkName;
    KSPIN_LOCK              ResetSpinLock;
    KSPIN_LOCK              CromSpinLock;
    KSPIN_LOCK              AsyncSpinLock;
    KSPIN_LOCK              IsochSpinLock;
    KSPIN_LOCK              IsochResourceSpinLock;
    BOOLEAN                 bShutdown;
    POWER_STATE             CurrentDevicePowerState;
    ULONG                   GenerationCount;
    LIST_ENTRY              BusResetIrps;
    LIST_ENTRY              CromData;
    LIST_ENTRY              AsyncAddressData;
    LIST_ENTRY              IsochDetachData;
    LIST_ENTRY              IsochResourceData;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// This is used to keep track of pending irp's for
// notification of bus resets.
//
typedef struct _BUS_RESET_IRP {
    LIST_ENTRY      BusResetIrpList;
    PIRP            Irp;
} BUS_RESET_IRP, *PBUS_RESET_IRP;

//
// This is used to keep track of dynamic crom calls.
//
typedef struct _CROM_DATA {
    LIST_ENTRY      CromList;
    HANDLE          hCromData;
    PVOID           Buffer;
    PMDL            pMdl;
} CROM_DATA, *PCROM_DATA;

//
// This is used to store data for each async address range. 
//
typedef struct _ASYNC_ADDRESS_DATA {
    LIST_ENTRY              AsyncAddressList;
    PDEVICE_EXTENSION       DeviceExtension;
    PVOID                   Buffer;
    ULONG                   nLength;
//    PADDRESS_OFFSET         Required1394Offset;
    ULONG                   nAddressesReturned;
    PADDRESS_RANGE          AddressRange;
    HANDLE                  hAddressRange;
    PMDL                    pMdl;
} ASYNC_ADDRESS_DATA, *PASYNC_ADDRESS_DATA;

#define ISOCH_DETACH_TAG    0xaabbbbaa

// 
// This is used to store data needed when calling IsochDetachBuffers.
// We need to store this data seperately for each call to IsochAttachBuffers.
//
typedef struct _ISOCH_DETACH_DATA {
    LIST_ENTRY              IsochDetachList;
    ULONG                   Tag;
    PDEVICE_EXTENSION       DeviceExtension;
    PISOCH_DESCRIPTOR       IsochDescriptor;
    PIRP                    Irp;
    PIRB                    DetachIrb;
    PIRB                    AttachIrb;
    NTSTATUS                AttachStatus;
    KTIMER                  Timer;
    KDPC                    TimerDpc;
    HANDLE                  hResource;
    ULONG                   numIsochDescriptors;
    ULONG                   outputBufferLength;
    ULONG                   bDetach;
    WORK_QUEUE_ITEM         WorkItem;
} ISOCH_DETACH_DATA, *PISOCH_DETACH_DATA;

//
// This is used to store allocated isoch resources.
// We use this information in case of a surprise removal.
//
typedef struct _ISOCH_RESOURCE_DATA {
    LIST_ENTRY      IsochResourceList;
    HANDLE          hResource;
} ISOCH_RESOURCE_DATA, *PISOCH_RESOURCE_DATA;

//
// 1394api.c
//
NTSTATUS
t1394Diag_GetLocalHostInformation(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            nLevel,
    IN OUT PULONG       UserStatus,
    IN OUT PVOID        Information
    );

NTSTATUS
t1394Diag_Get1394AddressFromDeviceObject(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            fulFlags,
    OUT PNODE_ADDRESS   pNodeAddress
    );

NTSTATUS
t1394Diag_Control(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

NTSTATUS
t1394Diag_GetMaxSpeedBetweenDevices(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            fulFlags,
    IN ULONG            ulNumberOfDestinations,
    IN PDEVICE_OBJECT   hDestinationDeviceObjects[64],
    OUT PULONG          fulSpeed
    );

NTSTATUS
t1394Diag_SetDeviceXmitProperties(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            fulSpeed,
    IN ULONG            fulPriority
    );

NTSTATUS
t1394Diag_GetConfigurationInformation(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

NTSTATUS
t1394Diag_BusReset(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            fulFlags
    );

NTSTATUS
t1394Diag_GetGenerationCount(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN OUT PULONG       GenerationCount
    );

NTSTATUS
t1394Diag_SendPhyConfigurationPacket(
    IN PDEVICE_OBJECT               DeviceObject,
    IN PIRP                         Irp,
    IN PHY_CONFIGURATION_PACKET     PhyConfigurationPacket
    );

NTSTATUS
t1394Diag_BusResetNotification(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            fulFlags
    );

NTSTATUS
t1394Diag_SetLocalHostProperties(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            nLevel,
    IN PVOID            Information
    );

void
t1394Diag_BusResetRoutine(
    IN PVOID    Context
    );

//
// 1394diag.c
//
NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT   DriverObject,
    IN PUNICODE_STRING  RegistryPath
    );

NTSTATUS
t1394Diag_Create(
    IN PDEVICE_OBJECT   DriverObject,
    IN PIRP             Irp
    );

NTSTATUS
t1394Diag_Close(
    IN PDEVICE_OBJECT   DriverObject,
    IN PIRP             Irp
    );

NTSTATUS
t1394Diag_SubmitIrpSynch(
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp,
    IN PIRB                 Irb
    );

NTSTATUS
t1394Diag_SynchCompletionRoutine(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PKEVENT          Event
    );

void
t1394Diag_CancelIrp(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

//
// async.c
//
NTSTATUS
t1394Diag_AllocateAddressRange(
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp,
    IN ULONG                fulAllocateFlags,
    IN ULONG                fulFlags,
    IN ULONG                nLength,
    IN ULONG                MaxSegmentSize,
    IN ULONG                fulAccessType,
    IN ULONG                fulNotificationOptions,
    IN OUT PADDRESS_OFFSET  Required1394Offset,
    OUT PHANDLE             phAddressRange,
    IN OUT PULONG           Data
    );

NTSTATUS
t1394Diag_FreeAddressRange(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN HANDLE           hAddressRange
    );

NTSTATUS
t1394Diag_SetAddressData(
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp,
    IN HANDLE               hAddressRange,
    IN ULONG                nLength,
    IN ULONG                ulOffset,
    IN PVOID                Data
    );

NTSTATUS
t1394Diag_AsyncRead(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            bRawMode,
    IN ULONG            bGetGeneration,
    IN IO_ADDRESS       DestinationAddress,
    IN ULONG            nNumberOfBytesToRead,
    IN ULONG            nBlockSize,
    IN ULONG            fulFlags,
    IN ULONG            ulGeneration,
    IN OUT PULONG       Data
    );

NTSTATUS
t1394Diag_AsyncWrite(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            bRawMode,
    IN ULONG            bGetGeneration,
    IN IO_ADDRESS       DestinationAddress,
    IN ULONG            nNumberOfBytesToWrite,
    IN ULONG            nBlockSize,
    IN ULONG            fulFlags,
    IN ULONG            ulGeneration,
    IN OUT PULONG       Data
    );

NTSTATUS
t1394Diag_AsyncLock(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            bRawMode,
    IN ULONG            bGetGeneration,
    IN IO_ADDRESS       DestinationAddress,
    IN ULONG            nNumberOfArgBytes,
    IN ULONG            nNumberOfDataBytes,
    IN ULONG            fulTransactionType,
    IN ULONG            fulFlags,
    IN ULONG            Arguments[2],
    IN ULONG            DataValues[2],
    IN ULONG            ulGeneration,
    IN OUT PVOID        Buffer
    );

NTSTATUS
t1394Diag_AsyncStream(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            nNumberOfBytesToStream,
    IN ULONG            fulFlags,
    IN ULONG            ulTag,
    IN ULONG            nChannel,
    IN ULONG            ulSynch,
    IN UCHAR            nSpeed,
    IN OUT PULONG       Data
    );

//
// ioctl.c
//
NTSTATUS
t1394Diag_IoControl(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

//
// isochapi.c
//
NTSTATUS
t1394Diag_IsochAllocateBandwidth(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            nMaxBytesPerFrameRequested,
    IN ULONG            fulSpeed,
    OUT PHANDLE         phBandwidth,
    OUT PULONG          pBytesPerFrameAvailable,
    OUT PULONG          pSpeedSelected
    );

NTSTATUS
t1394Diag_IsochAllocateChannel(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            nRequestedChannel,
    OUT PULONG          pChannel,
    OUT PLARGE_INTEGER  pChannelsAvailable
    );

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
    );

NTSTATUS
t1394Diag_IsochAttachBuffers(
    IN PDEVICE_OBJECT               DeviceObject,
    IN PIRP                         Irp,
    IN ULONG                        outputBufferLength,
    IN HANDLE                       hResource,
    IN ULONG                        nNumberOfDescriptors,
    OUT PISOCH_DESCRIPTOR           pIsochDescriptor,
    IN OUT PRING3_ISOCH_DESCRIPTOR  R3_IsochDescriptor
    );

NTSTATUS
t1394Diag_IsochDetachBuffers(
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp,
    IN HANDLE               hResource,
    IN ULONG                nNumberOfDescriptors,
    IN PISOCH_DESCRIPTOR    IsochDescriptor
    );

NTSTATUS
t1394Diag_IsochFreeBandwidth(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN HANDLE           hBandwidth
    );

NTSTATUS
t1394Diag_IsochFreeChannel(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            nChannel
    );

NTSTATUS
t1394Diag_IsochFreeResources(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN HANDLE           hResource
    );

NTSTATUS
t1394Diag_IsochListen(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN HANDLE           hResource,
    IN ULONG            fulFlags,
    IN CYCLE_TIME       StartTime
    );

NTSTATUS
t1394Diag_IsochQueryCurrentCycleTime(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    OUT PCYCLE_TIME     pCycleTime
    );

NTSTATUS
t1394Diag_IsochQueryResources(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            fulSpeed,
    OUT PULONG          pBytesPerFrameAvailable,
    OUT PLARGE_INTEGER  pChannelsAvailable
    );

NTSTATUS
t1394Diag_IsochSetChannelBandwidth(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN HANDLE           hBandwidth,
    IN ULONG            nMaxBytesPerFrame
    );

NTSTATUS
t1394Diag_IsochStop(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN HANDLE           hResource,
    IN ULONG            fulFlags
    );

NTSTATUS
t1394Diag_IsochTalk(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN HANDLE           hResource,
    IN ULONG            fulFlags,
    CYCLE_TIME          StartTime
    );

void
t1394Diag_IsochCallback(
    IN PDEVICE_EXTENSION    DeviceExtension,
    IN PISOCH_DETACH_DATA   IsochDetachData
    );

void
t1394Diag_IsochTimeout(
    IN PKDPC                Dpc,
    IN PISOCH_DETACH_DATA   IsochDetachData,
    IN PVOID                SystemArgument1,
    IN PVOID                SystemArgument2
    );

void
t1394Diag_IsochCleanup(
    IN PISOCH_DETACH_DATA   IsochDetachData
    );

NTSTATUS
t1394Diag_IsochDetachCompletionRoutine(
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp,
    IN PISOCH_DETACH_DATA   IsochDetachData
    );

NTSTATUS
t1394Diag_IsochAttachCompletionRoutine(
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp,
    IN PISOCH_DETACH_DATA   IsochDetachData
    );

//
// pnp.c
//
NTSTATUS
t1394Diag_PnpAddDevice(
    IN PDRIVER_OBJECT   DriverObject,
    IN PDEVICE_OBJECT   PhysicalDeviceObject
    );

NTSTATUS
t1394Diag_Pnp(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

NTSTATUS
t1394Diag_PnpStartDevice(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

NTSTATUS
t1394Diag_PnpStopDevice(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

NTSTATUS
t1394Diag_PnpRemoveDevice(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

NTSTATUS
t1394Diag_PnpBusReset(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

//
// power.c
//
NTSTATUS
t1394Diag_Power(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

void
t1394Diag_PowerRequestCompletion(
    IN PDEVICE_OBJECT       DeviceObject,
    IN UCHAR                MinorFunction,
    IN POWER_STATE          PowerState,
    IN PIRP                 Irp,
    IN PIO_STATUS_BLOCK     IoStatus
    );

#endif


