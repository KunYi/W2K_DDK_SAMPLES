/*++

Copyright (C) Microsoft Corporation, 1991 - 1999

Module Name:

    cdromp.h

Abstract:

    Private header file for cdrom.sys.  This contains private
    structure and function declarations as well as constant
    values which do not need to be exported.

Author:

Environment:

    kernel mode only

Notes:


Revision History:

--*/

#ifndef __CDROMP_H__
#define __CDROMP_H__


typedef struct _XA_CONTEXT {

    //
    // Pointer to the device object.
    //

    PDEVICE_OBJECT DeviceObject;

    //
    // Pointer to the original request when
    // a mode select must be sent.
    //

    PIRP OriginalRequest;

    //
    // Pointer to the mode select srb.
    //

    PSCSI_REQUEST_BLOCK Srb;
} XA_CONTEXT, *PXA_CONTEXT;

typedef struct _ERROR_RECOVERY_DATA {
    MODE_PARAMETER_HEADER   Header;
    MODE_PARAMETER_BLOCK BlockDescriptor;
    MODE_READ_RECOVERY_PAGE ReadRecoveryPage;
} ERROR_RECOVERY_DATA, *PERROR_RECOVERY_DATA;

typedef struct _ERROR_RECOVERY_DATA10 {
    MODE_PARAMETER_HEADER10 Header10;
    MODE_PARAMETER_BLOCK BlockDescriptor10;
    MODE_READ_RECOVERY_PAGE ReadRecoveryPage10;
} ERROR_RECOVERY_DATA10, *PERROR_RECOVERY_DATA10;

//
// CdRom specific addition to device extension.
//

typedef struct _CDROM_DATA {

    //
    // Indicates whether an audio play operation
    // is currently being performed.
    // Only thing this does is prevent reads and
    // toc requests while playing audio.
    //

    BOOLEAN PlayActive;

    //
    // Indicates whether the blocksize used for user data
    // is 2048 or 2352.
    //

    BOOLEAN RawAccess;

    //
    // Indicates that this is a DEC RRD cdrom.
    // This drive requires software to fix responses
    // from the faulty firmware
    //

    BOOLEAN IsDecRrd;

    //
    // This points to an irp which needs to be delayed for a bit before a
    // retry can be attempted.  The interval counter is set by the deferring
    // routine and will be decremented to zero in the tick handler.  Once
    // the counter goes to zero the irp will be issued again.
    // DelayedRetryResend controls whether the irp is resent to the lower
    // driver (TRUE) or reissued into the startio routine (FALSE)
    //

    BOOLEAN DelayedRetryResend;

    PIRP DelayedRetryIrp;

    ULONG DelayedRetryInterval;

    KSPIN_LOCK DelayedRetrySpinLock;

    //
    // indicate we need to pick a default dvd region
    // for the user if we can
    //

    ULONG PickDvdRegion;

    //
    // The interface strings registered for this device.
    //

    UNICODE_STRING CdromInterfaceString;
    UNICODE_STRING VolumeInterfaceString;

    //
    // The well known name link for this device.
    //

    UNICODE_STRING WellKnownName;

    //
    // Indicates whether 6 or 10 bytes mode sense/select
    // should be used
    //

    ULONG XAFlags;

    //
    // keep track of what type of DVD device we are
    //

    BOOLEAN DvdRpc0Device;
    BOOLEAN DvdRpc0LicenseFailure;
    UCHAR   Rpc0SystemRegion;           // bitmask, one means prevent play
    UCHAR   Rpc0SystemRegionResetCount;

    ULONG   Rpc0RetryRegistryCallback;   // one until initial region choosen

    KMUTEX  Rpc0RegionMutex;

    //
    // Storage for the error recovery page. This is used
    // as an easy method to switch block sizes.
    //
    // NOTE - doubly unnamed structs just aren't very clean looking code - this
    // should get cleaned up at some point in the future.
    //

    union {
        ERROR_RECOVERY_DATA;
        ERROR_RECOVERY_DATA10;
    };

} CDROM_DATA, *PCDROM_DATA;

#define DEVICE_EXTENSION_SIZE sizeof(FUNCTIONAL_DEVICE_EXTENSION) + sizeof(CDROM_DATA)
#define SCSI_CDROM_TIMEOUT          10
#define SCSI_CHANGER_BONUS_TIMEOUT  10
#define HITACHI_MODE_DATA_SIZE      12
#define MODE_DATA_SIZE              64
#define RAW_SECTOR_SIZE           2352
#define COOKED_SECTOR_SIZE        2048
#define CDROM_SRB_LIST_SIZE          4

#define PLAY_ACTIVE(x) (((PCDROM_DATA)(x->CommonExtension.DriverData))->PlayActive)

#define MSF_TO_LBA(Minutes,Seconds,Frames) \
                (ULONG)((60 * 75 * (Minutes)) + (75 * (Seconds)) + ((Frames) - 150))

#define LBA_TO_MSF(Lba,Minutes,Seconds,Frames)               \
{                                                            \
    (Minutes) = (UCHAR)(Lba  / (60 * 75));                   \
    (Seconds) = (UCHAR)((Lba % (60 * 75)) / 75);             \
    (Frames)  = (UCHAR)((Lba % (60 * 75)) % 75);             \
}

#define DEC_TO_BCD(x) (((x / 10) << 4) + (x % 10))

//
// Define flags for XA, CDDA, and Mode Select/Sense
//

#define XA_USE_6_BYTE             0x01
#define XA_USE_10_BYTE            0x02

#define XA_NOT_SUPPORTED          0x10
#define XA_USE_READ_CD            0x20
#define XA_PLEXTOR_CDDA           0x40
#define XA_NEC_CDDA               0x80

//
// Sector types for READ_CD
//

#define ANY_SECTOR                0
#define CD_DA_SECTOR              1
#define YELLOW_MODE1_SECTOR       2
#define YELLOW_MODE2_SECTOR       3
#define FORM2_MODE1_SECTOR        4
#define FORM2_MODE2_SECTOR        5

#define MAX_COPY_PROTECT_AGID     4

#ifdef ExAllocatePool
#undef ExAllocatePool
#define ExAllocatePool #assert(FALSE)
#endif

#define CDROM_TAG_DC_EVENT      'ECcS'  // "ScCE" - device control synch event
#define CDROM_TAG_DISK_GEOM     'GCcS'  // "ScCG" - disk geometry buffer
#define CDROM_TAG_HITACHI_ERROR 'HCcS'  // "ScCH" - hitachi error buffer
#define CDROM_TAG_SENSE_INFO    'ICcS'  // "ScCI" - sense info buffers
#define CDROM_TAG_POWER_IRP     'iCcS'  // "ScCi" - irp for power request
#define CDROM_TAG_SRB           'SCcS'  // "ScCS" - srb allocation
#define CDROM_TAG_STRINGS       'sCcS'  // "ScCs" - assorted string data
#define CDROM_TAG_MODE_DATA     'MCcS'  // "ScCM" - mode data buffer
#define CDROM_TAG_READ_CAP      'PCcS'  // "ScCP" - read capacity buffer
#define CDROM_TAG_PLAY_ACTIVE   'pCcS'  // "ScCp" - play active checks
#define CDROM_TAG_SUB_Q         'QCcS'  // "ScCQ" - read sub q buffer
#define CDROM_TAG_RAW           'RCcS'  // "ScCR" - raw mode read buffer
#define CDROM_TAG_TOC           'TCcS'  // "ScCT" - read toc buffer
#define CDROM_TAG_TOSHIBA_ERROR 'tCcS'  // "ScCt" - toshiba error buffer
#define CDROM_TAG_DEC_ERROR     'dCcS'  // "ScCt" - DEC error buffer
#define CDROM_TAG_UPDATE_CAP    'UCcS'  // "ScCU" - update capacity path
#define CDROM_TAG_VOLUME        'VCcS'  // "ScCV" - volume control buffer
#define CDROM_TAG_VOLUME_INT    'vCcS'  // "ScCv" - volume control buffer

#define DVD_TAG_READ_STRUCTURE  'SVcS'  // "ScVS" - used for dvd structure reads
#define DVD_TAG_READ_KEY        'kVcS'  // "ScVk" - read buffer for dvd key
#define DVD_TAG_SEND_KEY        'KVcS'  // "ScVK" - write buffer for dvd key
#define DVD_TAG_RPC2_CHECK      'sVcS'  // "ScVs" - read buffer for dvd/rpc2 check
#define DVD_TAG_DVD_REGION      'tVcS'  // "ScVt" - read buffer for rpc2 check
#define DVD_TAG_SECURITY        'XVcS' // "ScVX" - security descriptor

//
// DVD Registry Value Names for RPC0 Device
//
#define DVD_DEFAULT_REGION       L"DefaultDvdRegion"    // this is init. by the dvd class installer
#define DVD_CURRENT_REGION       L"DvdR"
#define DVD_REGION_RESET_COUNT   L"DvdRCnt"
#define DVD_MAX_REGION_RESET_COUNT  2
#define DVD_MAX_REGION              8



#define BAIL_OUT(Irp) \
    DebugPrint((2, "Cdrom: [%p] Bailing with status " \
                " %lx at line %x file %s\n",          \
                (Irp), (Irp)->IoStatus.Status,        \
                __LINE__, __FILE__))

__inline
VOID
CdRomCompleteIrpAndStartNextPacketSafely(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine grabs an extra remove lock using a local variable
    for a unique tag.  It then completes the irp in question, and
    the just-acquired removelock guarantees that it is still safe
    to call IoStartNextPacket().  When that finishes, we release
    the newly acquired RemoveLock and return.

Arguments:

    DeviceObject - the device object for the StartIo queue
    Irp - the request we are completing

Return Value:

    None

Notes:

    This is implemented as an inline function to allow the compiler
    to optimize this as either a function call or as actual inline code.

--*/
{
    UCHAR uniqueAddress;
    ClassAcquireRemoveLock(DeviceObject, (PIRP)&uniqueAddress);
    ClassReleaseRemoveLock(DeviceObject, Irp);
    ClassCompleteRequest(DeviceObject, Irp, IO_CD_ROM_INCREMENT);
    IoStartNextPacket(DeviceObject, FALSE);
    ClassReleaseRemoveLock(DeviceObject, (PIRP)&uniqueAddress);
    return;
}

VOID
CdRomDeviceControlDvdReadStructure(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP OriginalIrp,
    IN PIRP NewIrp,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
CdRomDeviceControlDvdEndSession(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP OriginalIrp,
    IN PIRP NewIrp,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
CdRomDeviceControlDvdStartSessionReadKey(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP OriginalIrp,
    IN PIRP NewIrp,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
CdRomDeviceControlDvdSendKey(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP OriginalIrp,
    IN PIRP NewIrp,
    IN PSCSI_REQUEST_BLOCK Srb
    );



NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
CdRomUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
CdRomAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT Pdo
    );

NTSTATUS
CdRomOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CdRomReadVerification(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CdRomSwitchMode(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG SectorSize,
    IN PIRP  OriginalRequest
    );

NTSTATUS
CdRomDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CdRomDeviceControlCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
CdRomSetVolumeIntermediateCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
CdRomSwitchModeCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
CdRomXACompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
CdRomClassIoctlCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
CdRomStartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
CdRomTickHandler(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
CdRomUpdateCapacity(
    IN PFUNCTIONAL_DEVICE_EXTENSION DeviceExtension,
    IN PIRP IrpToComplete,
    IN OPTIONAL PKEVENT IoctlEvent
    );

NTSTATUS
CreateCdRomDeviceObject(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT Pdo,
    IN ULONG CdRomCount
    );

VOID
ScanForSpecial(
    PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
CdRomIsPlayActive(
    IN PDEVICE_OBJECT DeviceObject
    );

VOID
HitachiProcessErrorGD2000(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    );

VOID
HitachiProcessError(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    );

VOID
ToshibaProcessError(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    );

NTSTATUS
ToshibaProcessErrorCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    );

VOID
CdRomCreateNamedEvent(
    IN PFUNCTIONAL_DEVICE_EXTENSION DeviceExtension,
    IN ULONG DeviceNumber
    );

NTSTATUS
CdRomInitDevice(
    IN PDEVICE_OBJECT Fdo
    );

NTSTATUS
CdRomStartDevice(
    IN PDEVICE_OBJECT Fdo
    );

NTSTATUS
CdRomStopDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR Type
    );

NTSTATUS
CdRomRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR Type
    );

NTSTATUS
CdRomDvdEndAllSessionsCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
CdRomDvdReadDiskKeyCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

DEVICE_TYPE
CdRomGetDeviceType(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
CdRomCreateWellKnownName(
    IN PDEVICE_OBJECT DeviceObject
    );

VOID
CdRomDeleteWellKnownName(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
CdromGetDeviceParameter (
    IN     PDEVICE_OBJECT      DeviceObject,
    IN     PWSTR               ParameterName,
    IN OUT PULONG              ParameterValue
    );

NTSTATUS
CdromSetDeviceParameter (
    IN PDEVICE_OBJECT DeviceObject,
    IN PWSTR          ParameterName,
    IN ULONG          ParameterValue
    );

VOID
CdromPickDvdRegion (
    IN PDEVICE_OBJECT Fdo
);

NTSTATUS
CdRomRetryRequest(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PIRP Irp,
    IN ULONG Delay,
    IN BOOLEAN ResendIrp
    );

NTSTATUS
CdRomRerunRequest(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN OPTIONAL PIRP Irp,
    IN BOOLEAN ResendIrp
    );

NTSTATUS
CdromGetRpc0Settings(
    IN PDEVICE_OBJECT Fdo
    );

NTSTATUS
CdromSetRpc0Settings(
    IN PDEVICE_OBJECT Fdo,
    IN UCHAR NewRegion
    );

#endif // __CDROMP_H__


