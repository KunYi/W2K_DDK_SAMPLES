/*++

Copyright (C) Microsoft Corporation, 1991 - 1999

Module Name:

    classp.h

Abstract:

    Private header file for classpnp.sys modules.  This contains private
    structure and function declarations as well as constant values which do
    not need to be exported.

Author:

Environment:

    kernel mode only

Notes:


Revision History:

--*/

#ifndef __CLASSP_H__
#define __CLASSP_H__

#include "stddef.h"
#include "ntddk.h"
#include "scsi.h"

#include <wmidata.h>

#include "classpnp.h"

#if CLASS_INIT_GUID
#include <initguid.h>
#endif

#include "mountdev.h"
#include "ioevent.h"

#include <stdarg.h>

#define CLASS_FILE_OBJECT_EXTENSION_KEY     'eteP'

#define CLASSP_VOLUME_VERIFY_CHECKED        0x34

typedef struct _MEDIA_CHANGE_DETECTION_INFO {
    //
    // If MediaChangeNoMedia is true then we've already determined that there
    // isn't any media in the drive so we shouldn't signal the event again
    //

    MEDIA_CHANGE_DETECTION_STATE MediaChangeDetectionState;

    //
    // The media change event is being supported.  The media change timer
    // should be running whenever this is true.
    //

    LONG MediaChangeDetectionDisableCount;

    //
    // The timer value to support media change events.  This is a countdown
    // value used to determine when to poll the device for a media change.
    // The max value for the timer is 255 seconds.
    //

    LONG MediaChangeCountDown;

    //
    // Event to synchronize enable/disable requests with the periodic pinging
    // of the device.
    //

    KEVENT MediaChangeSynchronizationEvent;

    //
    // Pointer to the irp to be used for media change detection.
    //

    PIRP MediaChangeIrp;

    //
    // The srb for the media change detection.
    //

    SCSI_REQUEST_BLOCK MediaChangeSrb;

    PUCHAR SenseBuffer;

    ULONG SrbFlags;

    //
    // Second timer to keep track of how long the media change IRP has been
    // in use.  If this value exceeds the timeout (#defined) then we should
    // print out a message to the user and set the MediaChangeIrpLost flag
    //

    SHORT MediaChangeIrpTimeInUse;

    //
    // Set by CdRomTickHandler when we determine that the media change irp has
    // been lost
    //

    BOOLEAN MediaChangeIrpLost;

    //
    // Set by MediaChangeDetectionSanityCheck
    //

    ULONG MediaChangeIrpCompleted;

};

typedef enum {
    SimpleMediaLock,
    SecureMediaLock,
    InternalMediaLock
} MEDIA_LOCK_TYPE, *PMEDIA_LOCK_TYPE;

typedef struct _FAILURE_PREDICTION_INFO {
    FAILURE_PREDICTION_METHOD Method;
    ULONG CountDown;                // Countdown timer
    ULONG Period;                   // Countdown period

    PIO_WORKITEM WorkQueueItem;

    KEVENT Event;
} FAILURE_PREDICTION_INFO, *PFAILURE_PREDICTION_INFO;

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
ClassUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
ClassCreateClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ClasspCreateClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
ClasspCleanupProtectedLocks(
    IN PFILE_OBJECT_EXTENSION FsContext
    );

NTSTATUS
ClasspEjectionControl(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PIRP Irp,
    IN MEDIA_LOCK_TYPE LockType,
    IN BOOLEAN Lock
    );

NTSTATUS
ClassReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ClassDeviceControlDispatch(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
ClassDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
ClassDispatchPnp(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
ClassPnpStartDevice(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
ClassInternalIoControl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ClassShutdownFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ClassSystemControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

//
// Class internal routines
//

NTSTATUS
ClassAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN OUT PDEVICE_OBJECT PhysicalDeviceObject
    );

ClasspSendSynchronousCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
RetryRequest(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PSCSI_REQUEST_BLOCK Srb,
    BOOLEAN Associated
    );

NTSTATUS
ClassIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
ClassPnpQueryFdoRelations(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP Irp
    );

NTSTATUS
ClassRetrieveDeviceRelations(
    IN PDEVICE_OBJECT Fdo,
    IN DEVICE_RELATION_TYPE RelationType,
    OUT PDEVICE_RELATIONS *DeviceRelations
    );

NTSTATUS
ClassGetPdoId(
    IN PDEVICE_OBJECT Pdo,
    IN BUS_QUERY_ID_TYPE IdType,
    IN PUNICODE_STRING IdString
    );

NTSTATUS
ClassQueryPnpCapabilities(
    IN PDEVICE_OBJECT PhysicalDeviceObject,
    IN PDEVICE_CAPABILITIES Capabilities
    );

VOID
ClasspStartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ClasspPagingNotificationCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PDEVICE_OBJECT RealDeviceObject
    );

NTSTATUS
ClasspMediaChangeCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    );

PFILE_OBJECT_EXTENSION
ClasspGetFsContext(
    IN PCOMMON_DEVICE_EXTENSION CommonExtension,
    IN PFILE_OBJECT FileObject
    );

NTSTATUS
ClasspMcnControl(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PIRP Irp,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
ClasspRegisterMountedDeviceInterface(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
ClasspDisableTimer(
    PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
ClasspEnableTimer(
    PDEVICE_OBJECT DeviceObject
    );

//
// routines for dictionary list support
//

VOID
InitializeDictionary(
    IN PDICTIONARY Dictionary
    );

BOOLEAN
TestDictionarySignature(
    IN PDICTIONARY Dictionary
    );

NTSTATUS
AllocateDictionaryEntry(
    IN PDICTIONARY Dictionary,
    IN ULONGLONG Key,
    IN ULONG Size,
    IN ULONG Tag,
    OUT PVOID *Entry
    );

PVOID
GetDictionaryEntry(
    IN PDICTIONARY Dictionary,
    IN ULONGLONG Key
    );

VOID
FreeDictionaryEntry(
    IN PDICTIONARY Dictionary,
    IN PVOID Entry
    );


NTSTATUS
ClasspAllocateReleaseRequest(
    IN PDEVICE_OBJECT Fdo
    );

VOID
ClasspFreeReleaseRequest(
    IN PDEVICE_OBJECT Fdo
    );

NTSTATUS
ClassReleaseQueueCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
ClasspReleaseQueue(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP ReleaseQueueIrp
    );

VOID
ClasspDisablePowerNotification(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
);

//
// class power routines
//

NTSTATUS
ClassDispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ClassMinimalPowerHandler(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

//
// Child list routines
//

VOID
ClassAddChild(
    IN PFUNCTIONAL_DEVICE_EXTENSION Parent,
    IN PPHYSICAL_DEVICE_EXTENSION Child,
    IN BOOLEAN AcquireLock
    );

PPHYSICAL_DEVICE_EXTENSION
ClassRemoveChild(
    IN PFUNCTIONAL_DEVICE_EXTENSION Parent,
    IN PPHYSICAL_DEVICE_EXTENSION Child,
    IN BOOLEAN AcquireLock
    );

#endif // __CLASSP_H__

