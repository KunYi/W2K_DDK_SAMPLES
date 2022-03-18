/*++

Copyright (C) Microsoft Corporation, 1991 - 1999

Module Name:

    disk.c

Abstract:

    SCSI disk class driver

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "ntddk.h"
#include "scsi.h"
#include <wmidata.h>
#include "classpnp.h"
#if defined(JAPAN) && defined(_X86_)
#include "machine.h"
#endif

#include <wmistr.h>

#if defined(_X86_)
#include "mountdev.h"
#endif

#include "ntddstor.h"  // need the interface class guids contained therein

#ifdef ExAllocatePool
#undef ExAllocatePool
#define ExAllocatePool #assert(FALSE)
#endif

#define DISK_TAG_GENERAL        ' DcS'  // "ScD " - generic tag
#define DISK_TAG_SMART          'aDcS'  // "ScDa" - SMART allocations
#define DISK_TAG_INFO_EXCEPTION 'ADcS'  // "ScDA" - Info Exceptions
#define DISK_TAG_DISABLE_CACHE  'CDcS'  // "ScDC" - disable cache paths
#define DISK_TAG_CCONTEXT       'cDcS'  // "ScDc" - disk allocated completion context
#define DISK_TAG_DISK_GEOM      'GDcS'  // "ScDG" - disk geometry buffer
#define DISK_TAG_UPDATE_GEOM    'gDcS'  // "ScDg" - update disk geometry paths
#define DISK_TAG_SENSE_INFO     'IDcS'  // "ScDI" - sense info buffers
#define DISK_TAG_PNP_ID         'iDcS'  // "ScDp" - pnp ids
#define DISK_TAG_MODE_DATA      'MDcS'  // "ScDM" - mode data buffer
#define DISK_CACHE_MBR_CHECK    'mDcS'  // "ScDM" - mbr checksum code
#define DISK_TAG_NAME           'NDcS'  // "ScDN" - disk name code
#define DISK_TAG_READ_CAP       'PDcS'  // "ScDP" - read capacity buffer
#define DISK_TAG_PART_LIST      'pDcS'  // "ScDp" - disk partition lists
#define DISK_TAG_SRB            'SDcS'  // "ScDS" - srb allocation
#define DISK_TAG_START          'sDcS'  // "ScDs" - start device paths
#define DISK_TAG_UPDATE_CAP     'UDcS'  // "ScDU" - update capacity path
#define DISK_TAG_NEC_98         '9DcS'  // 'ScD9" - allocations in NEC98 specific code.

typedef
VOID
(*PDISK_UPDATE_PARTITIONS) (
    IN PDEVICE_OBJECT Fdo,
    IN OUT PDRIVE_LAYOUT_INFORMATION PartitionList
    );

#if defined(_X86_)
//
// Disk device data
//

typedef enum _DISK_GEOMETRY_SOURCE {
    DiskGeometryUnknown,
    DiskGeometryFromBios,
    DiskGeometryFromPort,
    DiskGeometryFromNec98,
    DiskGeometryGuessedFromBios,
    DiskGeometryFromDefault
} DISK_GEOMETRY_SOURCE, *PDISK_GEOMETRY_SOURCE;
#endif

//

typedef struct _DISK_DATA {

    //
    // Disk signature (from MBR)
    //

    ULONG Signature;

    //
    // MBR checksum
    //

    ULONG MbrCheckSum;

    //
    // Number of hidden sectors for BPB.
    //

    ULONG HiddenSectors;

    //
    // This field is the ordinal of a partition as it appears on a disk.
    //

    ULONG PartitionOrdinal;

    //
    // Partition type of this device object
    //
    // This field is set by:
    //
    //     1)  Initially set according to the partition list entry partition
    //         type returned by IoReadPartitionTable.
    //
    //     2)  Subsequently set by the IOCTL_DISK_SET_PARTITION_INFORMATION
    //         I/O control function when IoSetPartitionInformation function
    //         successfully updates the partition type on the disk.
    //

    UCHAR PartitionType;

    //
    // Boot indicator - indicates whether this partition is a bootable (active)
    // partition for this device
    //
    // This field is set according to the partition list entry boot indicator
    // returned by IoReadPartitionTable.
    //

    BOOLEAN BootIndicator;

    struct {
        //
        // This flag is set when the well known name is created (through
        // DiskCreateSymbolicLinks) and cleared when destroying it
        // (by calling DiskDeleteSymbolicLinks).
        //

        BOOLEAN WellKnownNameCreated : 1;

        //
        // This flag is set when the PhysicalDriveN link is created (through
        // DiskCreateSymbolicLinks) and is cleared when destroying it (through
        // DiskDeleteSymbolicLinks)
        //

        BOOLEAN PhysicalDriveLinkCreated : 1;

    } LinkStatus;

    //
    // ReadyStatus - STATUS_SUCCESS indicates that the drive is ready for
    // use.  Any error status is to be returned as an explaination for why
    // a request is failed.
    //

    NTSTATUS ReadyStatus;

    //
    // Routine to be called when updating the disk partitions.  This routine
    // is different for removable and non-removable media and is called by
    // (among other things) DiskEnumerateDevice
    //

    PDISK_UPDATE_PARTITIONS UpdatePartitionRoutine;

    //
    // SCSI address used for SMART operations.
    //

    SCSI_ADDRESS ScsiAddress;

    //
    // Event used to synchronize partitioning operations and enumerations.
    //

    KEVENT PartitioningEvent;

    //
    // These unicode strings hold the disk and volume interface strings.  If
    // the interfaces were not registered or could not be set then the string
    // buffer will be NULL.
    //

    UNICODE_STRING DiskInterfaceString;
    UNICODE_STRING PartitionInterfaceString;

    //
    // What type of failure prediction mechanism is available
    //

    FAILURE_PREDICTION_METHOD FailurePredictionCapability;
    BOOLEAN AllowFPPerfHit;

#if defined(_X86_)
    //
    // This flag indiciates that a non-default geometry for this drive has
    // already been determined by the disk driver.  This field is ignored
    // for removable media drives.
    //

    DISK_GEOMETRY_SOURCE GeometrySource;

    //
    // If GeometryDetermined is TRUE this will contain the geometry which was
    // reported by the firmware or by the BIOS.  For removable media drives
    // this will contain the last geometry used when media was present.
    //

    DISK_GEOMETRY RealGeometry;
#endif

} DISK_DATA, *PDISK_DATA;

// Define a general structure of identfing disk controllers with bad
// hardware.
//

typedef struct _BAD_CONTROLLER_INFORMATION {
    PCHAR   VendorId;
    PCHAR   ProductId;
    PCHAR   ProductRevision;
    const ULONG DisableTaggedQueuing : 1;
    const ULONG DisableSynchronousTransfers : 1;
    const ULONG DisableSpinDown : 1;
    const ULONG DisableWriteCache : 1;
    const ULONG CauseNotReportableHack : 1;
}BAD_CONTROLLER_INFORMATION, *PBAD_CONTROLLER_INFORMATION;

#define FUNCTIONAL_EXTENSION_SIZE sizeof(FUNCTIONAL_DEVICE_EXTENSION) + sizeof(DISK_DATA)
#define PHYSICAL_EXTENSION_SIZE sizeof(PHYSICAL_DEVICE_EXTENSION) + sizeof(DISK_DATA)

#define MODE_DATA_SIZE      192
#define VALUE_BUFFER_SIZE  2048
#define SCSI_DISK_TIMEOUT    10
#define PARTITION0_LIST_SIZE  4

#define MAX_MEDIA_TYPES 4
typedef struct _DISK_MEDIA_TYPES_LIST {
    PCHAR VendorId;
    PCHAR ProductId;
    PCHAR Revision;
    const ULONG NumberOfTypes;
    const ULONG NumberOfSides;
    const STORAGE_MEDIA_TYPE MediaTypes[MAX_MEDIA_TYPES];
} DISK_MEDIA_TYPES_LIST, *PDISK_MEDIA_TYPES_LIST;

//
// Poll for Failure Prediction every hour
//
#define DISK_DEFAULT_FAILURE_POLLING_PERIOD 1 * 60 * 60

//
// Static global lookup tables.
//

extern const BAD_CONTROLLER_INFORMATION DiskBadControllers[];
extern const DISK_MEDIA_TYPES_LIST DiskMediaTypes[];

//
// Macros
//


//
// Routine prototypes.
//


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
DiskUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
DiskAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT Pdo
    );

NTSTATUS
DiskInitFdo(
    IN PDEVICE_OBJECT Fdo
    );

NTSTATUS
DiskInitPdo(
    IN PDEVICE_OBJECT Pdo
    );

NTSTATUS
DiskStartFdo(
    IN PDEVICE_OBJECT Fdo
    );

NTSTATUS
DiskStartPdo(
    IN PDEVICE_OBJECT Pdo
    );

NTSTATUS
DiskStopDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR Type
    );

NTSTATUS
DiskRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR Type
    );

NTSTATUS
DiskReadWriteVerification(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
DiskDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
DiskFdoProcessError(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    );

NTSTATUS
DiskShutdownFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
DisableWriteCache(
    IN PDEVICE_OBJECT DeviceObject,
    IN PSTORAGE_DEVICE_DESCRIPTOR DeviceDescriptor
    );

NTSTATUS
DiskModeSelect(
    IN PDEVICE_OBJECT DeviceObject,
    IN PCHAR ModeSelectBuffer,
    IN ULONG Length,
    IN BOOLEAN SavePage
    );


NTSTATUS
DiskPerformSmartCommand(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN ULONG SrbControlCode,
    IN UCHAR Command,
    IN UCHAR Feature,
    IN UCHAR SectorCount,
    IN OUT PSRB_IO_CONTROL SrbControl,
    OUT PULONG BufferSize
    );

NTSTATUS
DiskGetInfoExceptionInformation(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    OUT PBOOLEAN LogErr,
    OUT PBOOLEAN SupportInfoException
    );

NTSTATUS
DiskSetInfoExceptionInformation(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN BOOLEAN Enable,
    IN BOOLEAN AllowPerfHit,
    IN BOOLEAN InduceFailure
    );

NTSTATUS
DiskDetectFailurePrediction(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    PFAILURE_PREDICTION_METHOD FailurePredictCapability
    );

NTSTATUS
DiskReadSignature(
    IN PFUNCTIONAL_DEVICE_EXTENSION DeviceExtension
    );

BOOLEAN
EnumerateBusKey(
    IN PFUNCTIONAL_DEVICE_EXTENSION DeviceExtension,
    HANDLE BusKey,
    PULONG DiskNumber
    );

NTSTATUS
DiskCreateFdo(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT LowerDeviceObject,
    IN PULONG DeviceCount,
    IN BOOLEAN DasdAccessOnly
    );

VOID
UpdateDeviceObjects(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
ScanForSpecial(
    PDEVICE_OBJECT Fdo
    );

VOID
ResetBus(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
DiskEnumerateDevice(
    IN PDEVICE_OBJECT Fdo
    );

NTSTATUS
DiskQueryId(
    IN PDEVICE_OBJECT Pdo,
    IN BUS_QUERY_ID_TYPE IdType,
    IN PUNICODE_STRING UnicodeIdString
    );

NTSTATUS
DiskQueryPnpCapabilities(
    IN PDEVICE_OBJECT DeviceObject,
    IN PDEVICE_CAPABILITIES Capabilities
    );

NTSTATUS
DiskGenerateDeviceName(
    IN BOOLEAN IsFdo,
    IN ULONG DeviceNumber,
    IN OPTIONAL ULONG PartitionNumber,
    IN OPTIONAL PLARGE_INTEGER StartingOffset,
    IN OPTIONAL PLARGE_INTEGER PartitionLength,
    OUT PUCHAR *RawName
    );

VOID
DiskCreateSymbolicLinks(
    IN PDEVICE_OBJECT DeviceObject
    );

VOID
DiskUpdatePartitions(
    IN PDEVICE_OBJECT Fdo,
    IN OUT PDRIVE_LAYOUT_INFORMATION PartitionList
    );

VOID
DiskUpdateRemovablePartitions(
    IN PDEVICE_OBJECT Fdo,
    IN OUT PDRIVE_LAYOUT_INFORMATION PartitionList
    );

NTSTATUS
DiskCreatePdo(
    IN PDEVICE_OBJECT Fdo,
    IN ULONG PartitionOrdinal,
    IN PPARTITION_INFORMATION PartitionEntry,
    OUT PDEVICE_OBJECT *Pdo
    );

VOID
DiskDeleteSymbolicLinks(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
DiskPdoQueryWmiRegInfo(
    IN PDEVICE_OBJECT DeviceObject,
    OUT ULONG *RegFlags,
    OUT PUNICODE_STRING InstanceName
    );

NTSTATUS
DiskPdoQueryWmiDataBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG BufferAvail,
    OUT PUCHAR Buffer
    );

NTSTATUS
DiskPdoSetWmiDataBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG BufferSize,
    IN PUCHAR Buffer
    );

NTSTATUS
DiskPdoSetWmiDataItem(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG DataItemId,
    IN ULONG BufferSize,
    IN PUCHAR Buffer
    );

NTSTATUS
DiskPdoExecuteWmiMethod(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG MethodId,
    IN ULONG InBufferSize,
    IN ULONG OutBufferSize,
    IN PUCHAR Buffer
    );

NTSTATUS
DiskFdoQueryWmiRegInfo(
    IN PDEVICE_OBJECT DeviceObject,
    OUT ULONG *RegFlags,
    OUT PUNICODE_STRING InstanceName
    );

NTSTATUS
DiskFdoQueryWmiDataBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG BufferAvail,
    OUT PUCHAR Buffer
    );

NTSTATUS
DiskFdoSetWmiDataBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG BufferSize,
    IN PUCHAR Buffer
    );

NTSTATUS
DiskFdoSetWmiDataItem(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG DataItemId,
    IN ULONG BufferSize,
    IN PUCHAR Buffer
    );

NTSTATUS
DiskFdoExecuteWmiMethod(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG MethodId,
    IN ULONG InBufferSize,
    IN ULONG OutBufferSize,
    IN PUCHAR Buffer
    );

NTSTATUS
DiskWmiFunctionControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN CLASSENABLEDISABLEFUNCTION Function,
    IN BOOLEAN Enable
    );

NTSTATUS
DiskReadFailurePredictStatus(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    PSTORAGE_FAILURE_PREDICT_STATUS DiskSmartStatus
    );

NTSTATUS
DiskReadFailurePredictData(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    PSTORAGE_FAILURE_PREDICT_DATA DiskSmartData
    );

NTSTATUS
DiskEnableDisableFailurePrediction(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    BOOLEAN Enable
    );

NTSTATUS
DiskEnableDisableFailurePredictPolling(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    BOOLEAN Enable,
    ULONG PollTimeInSeconds
    );

VOID
DiskAcquirePartitioningLock(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    );

VOID
DiskReleasePartitioningLock(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    );

NTSTATUS
DiskClearPartitionTable(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG SectorSize,
    IN ULONG SectorsPerTrack,
    IN ULONG NumberOfHeads
    );

extern GUIDREGINFO DiskWmiFdoGuidList[];
extern GUIDREGINFO DiskWmiPdoGuidList[];

#if defined(_X86_)
NTSTATUS
DiskReadDriveCapacity(
    IN PDEVICE_OBJECT Fdo
    );
#else
#define DiskReadDriveCapacity(Fdo)  ClassReadDriveCapacity(Fdo)
#endif


#if defined(_X86_)
NTSTATUS
DiskQuerySuggestedLinkName(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
DiskSaveDetectInfo(
    PDRIVER_OBJECT DriverObject
    );

VOID
DiskCleanupDetectInfo(
    IN PDRIVER_OBJECT DriverObject
    );

VOID
DiskDriverReinitialization (
    IN PDRIVER_OBJECT DriverObject,
    IN PVOID Nothing,
    IN ULONG Count
    );


#endif

