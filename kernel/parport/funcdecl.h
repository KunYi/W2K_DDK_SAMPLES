/*++

Copyright (C) Microsoft Corporation, 1998 - 1999

Module Name:

    parport.sys

File Name:

    funcdecl.h

Abstract:

    This file contains the parport function declarations for functions
    that are called from a translation unit other than the one in
    which the function is defined.

--*/

NTSTATUS
PptWmiInitWmi(PDEVICE_OBJECT DeviceObject); 

NTSTATUS
PptDispatchSystemControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

//
// pnp.c - dvdf
//
VOID
PptPnpInitDispatchFunctionTable(
    VOID
    );

NTSTATUS
PptPnpAddDevice(
    IN PDRIVER_OBJECT pDriverObject,
    IN PDEVICE_OBJECT pPhysicalDeviceObject
    );

NTSTATUS
PptDispatchPnp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

//
//
//

NTSTATUS
PptFailRequest(
    IN PIRP Irp, 
    IN NTSTATUS Status
    );

NTSTATUS
PptDispatchPreProcessIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

NTSTATUS
PptDispatchPostProcessIrp();


//
// initunld.c
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
PptUnload(
    IN  PDRIVER_OBJECT  DriverObject
    );


//
// parport.c
//

NTSTATUS
PptSystemControl (
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP           pIrp
   );

NTSTATUS
PptSynchCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PKEVENT Event
    );

VOID
PptLogError(
    IN  PDRIVER_OBJECT      DriverObject,
    IN  PDEVICE_OBJECT      DeviceObject OPTIONAL,
    IN  PHYSICAL_ADDRESS    P1,
    IN  PHYSICAL_ADDRESS    P2,
    IN  ULONG               SequenceNumber,
    IN  UCHAR               MajorFunctionCode,
    IN  UCHAR               RetryCount,
    IN  ULONG               UniqueErrorValue,
    IN  NTSTATUS            FinalStatus,
    IN  NTSTATUS            SpecificIOStatus
    );

NTSTATUS
PptConnectInterrupt(
    IN  PDEVICE_EXTENSION   Extension
    );

VOID
PptDisconnectInterrupt(
    IN  PDEVICE_EXTENSION   Extension
    );

NTSTATUS
PptDispatchCreate(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

NTSTATUS
PptDispatchClose(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

BOOLEAN
PptSynchronizedIncrement(
    IN OUT  PVOID   SyncContext
    );

BOOLEAN
PptSynchronizedDecrement(
    IN OUT  PVOID   SyncContext
    );

BOOLEAN
PptSynchronizedRead(
    IN OUT  PVOID   SyncContext
    );

BOOLEAN
PptSynchronizedQueue(
    IN  PVOID   Context
    );

BOOLEAN
PptSynchronizedDisconnect(
    IN  PVOID   Context
    );

VOID
PptCancelRoutine(
    IN OUT  PDEVICE_OBJECT  DeviceObject,
    IN OUT  PIRP            Irp
    );

VOID
PptFreePortDpc(
    IN      PKDPC   Dpc,
    IN OUT  PVOID   Extension,
    IN      PVOID   SystemArgument1,
    IN      PVOID   SystemArgument2
    );

BOOLEAN
PptTryAllocatePortAtInterruptLevel(
    IN  PVOID   Context
    );

VOID
PptFreePortFromInterruptLevel(
    IN  PVOID   Context
    );

BOOLEAN
PptInterruptService(
    IN  PKINTERRUPT Interrupt,
    IN  PVOID       Extension
    );

BOOLEAN
PptTryAllocatePort(
    IN  PVOID   Extension
    );

BOOLEAN
PptTraversePortCheckList(
    IN  PVOID   Extension
    );

VOID
PptFreePort(
    IN  PVOID   Extension
    );

ULONG
PptQueryNumWaiters(
    IN  PVOID   Extension
    );

NTSTATUS
PptDispatchInternalDeviceControl(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

VOID
PptCleanupDevice(
    IN  PDEVICE_EXTENSION   Extension
    );

NTSTATUS
PptDispatchCleanup(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

BOOLEAN
PptIsNecR98Machine(
    void
    );

NTSTATUS
PptDispatchPower (
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP           pIrp
    );

VOID
PptRegInitDriverSettings(
    PUNICODE_STRING   RegistryPath
    );

PWSTR
PptGetPortNameFromPhysicalDeviceObject(
  PDEVICE_OBJECT PhysicalDeviceObject
  );

PVOID
PptSetCancelRoutine(
    IN PIRP           Irp, 
    IN PDRIVER_CANCEL CancelRoutine
);

NTSTATUS
PptAcquireRemoveLockOrFailIrp(
    IN PDEVICE_OBJECT DeviceObject, 
    PIRP              Irp
);

//
// debug.c
//

VOID
PptDebugDumpPnpIrpInfo(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    );

NTSTATUS
PptAcquireRemoveLock(
    IN PIO_REMOVE_LOCK RemoveLock,
    IN PVOID           Tag OPTIONAL
    );

VOID
PptReleaseRemoveLock(
    IN PIO_REMOVE_LOCK RemoveLock,
    IN PVOID           Tag OPTIONAL
    );

VOID
PptReleaseRemoveLockAndWait(
    IN PIO_REMOVE_LOCK RemoveLock,
    IN PVOID           Tag
    );

VOID
PptDebugDumpResourceList(
    PIO_RESOURCE_LIST ResourceList
    );

VOID
PptDebugDumpResourceRequirementsList(
    PIO_RESOURCE_REQUIREMENTS_LIST ResourceRequirementsList
    );

//
//
//

VOID
PptLogError(
    IN  PDRIVER_OBJECT      DriverObject,
    IN  PDEVICE_OBJECT      DeviceObject OPTIONAL,
    IN  PHYSICAL_ADDRESS    P1,
    IN  PHYSICAL_ADDRESS    P2,
    IN  ULONG               SequenceNumber,
    IN  UCHAR               MajorFunctionCode,
    IN  UCHAR               RetryCount,
    IN  ULONG               UniqueErrorValue,
    IN  NTSTATUS            FinalStatus,
    IN  NTSTATUS            SpecificIOStatus
    );

VOID
PptReportResourcesDevice(
    IN  PDEVICE_EXTENSION   Extension,
    IN  BOOLEAN             ClaimInterrupt,
    OUT PBOOLEAN            ConflictDetected
    );

VOID
PptUnReportResourcesDevice(
    IN OUT  PDEVICE_EXTENSION   Extension
    );

NTSTATUS
PptConnectInterrupt(
    IN  PDEVICE_EXTENSION   Extension
    );

VOID
PptDisconnectInterrupt(
    IN  PDEVICE_EXTENSION   Extension
    );

NTSTATUS
PptDispatchCreateClose(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

BOOLEAN
PptSynchronizedIncrement(
    IN OUT  PVOID   SyncContext
    );

BOOLEAN
PptSynchronizedDecrement(
    IN OUT  PVOID   SyncContext
    );

BOOLEAN
PptSynchronizedRead(
    IN OUT  PVOID   SyncContext
    );

BOOLEAN
PptSynchronizedQueue(
    IN  PVOID   Context
    );

BOOLEAN
PptSynchronizedDisconnect(
    IN  PVOID   Context
    );

VOID
PptCancelRoutine(
    IN OUT  PDEVICE_OBJECT  DeviceObject,
    IN OUT  PIRP            Irp
    );

VOID
PptFreePortDpc(
    IN      PKDPC   Dpc,
    IN OUT  PVOID   Extension,
    IN      PVOID   SystemArgument1,
    IN      PVOID   SystemArgument2
    );

BOOLEAN
PptTryAllocatePortAtInterruptLevel(
    IN  PVOID   Context
    );

VOID
PptFreePortFromInterruptLevel(
    IN  PVOID   Context
    );

BOOLEAN
PptInterruptService(
    IN  PKINTERRUPT Interrupt,
    IN  PVOID       Extension
    );

BOOLEAN
PptTryAllocatePort(
    IN  PVOID   Extension
    );

BOOLEAN
PptTraversePortCheckList(
    IN  PVOID   Extension
    );

VOID
PptFreePort(
    IN  PVOID   Extension
    );

ULONG
PptQueryNumWaiters(
    IN  PVOID   Extension
    );

NTSTATUS
PptDispatchDeviceControl(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

VOID
PptCleanupDevice(
    IN  PDEVICE_EXTENSION   Extension
    );

NTSTATUS
PptDispatchCleanup(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

VOID
PptUnload(
    IN  PDRIVER_OBJECT  DriverObject
    );

BOOLEAN
PptIsNecR98Machine(
    void
    );

//
// parmode.c
//

NTSTATUS
PptDetectChipFilter(
    IN  PDEVICE_EXTENSION   Extension
    );

NTSTATUS
PptDetectPortType(
    IN  PDEVICE_EXTENSION   Extension
    );

NTSTATUS
PptSetChipMode (
    IN  PDEVICE_EXTENSION  Extension,
    IN  UCHAR              ChipMode
    );

NTSTATUS
PptClearChipMode (
    IN  PDEVICE_EXTENSION  Extension,
    IN  UCHAR              ChipMode
    );

//
// par12843.c
//

ULONG
PptInitiate1284_3(
    IN  PVOID   Extension
    );

NTSTATUS
PptTrySelectDevice(
    IN  PVOID   Context,
    IN  PVOID   TrySelectCommand
    );

NTSTATUS
PptDeselectDevice(
    IN  PVOID   Context,
    IN  PVOID   DeselectCommand
    );

ULONG
Ppt1284_3AssignAddress(
    IN  PDEVICE_EXTENSION    DeviceExtension
    );

BOOLEAN
PptSend1284_3Command(
    IN  PDEVICE_EXTENSION  DeviceExtension,
    IN  UCHAR              Command
    );

//
// Ppt RemoveLock function declarations
//
NTSTATUS
PptAcquireRemoveLock(
    IN PIO_REMOVE_LOCK RemoveLock,
    IN PVOID           Tag OPTIONAL
    );

VOID
PptReleaseRemoveLock(
    IN PIO_REMOVE_LOCK RemoveLock,
    IN PVOID           Tag OPTIONAL
    );

VOID
PptReleaseRemoveLockAndWait(
    IN PIO_REMOVE_LOCK RemoveLock,
    IN PVOID           Tag
    );

//
// power management function declarations
//
NTSTATUS
PptPowerDispatch (
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP           pIrp
    );



//
// other function declarations
//

PWSTR
PptGetPortNameFromPhysicalDeviceObject(
  PDEVICE_OBJECT PhysicalDeviceObject
  );

NTSTATUS
PptSynchCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PKEVENT Event
    );

NTSTATUS
PptTrySelectLegacyZip(
    IN  PVOID   Context,
    IN  PVOID   TrySelectCommand
    );

NTSTATUS
PptDeselectLegacyZip(
    IN  PVOID   Context,
    IN  PVOID   DeselectCommand
    );

VOID
PptDumpRemovalRelationsList(
    IN PDEVICE_EXTENSION Extension
    );

NTSTATUS
PptRegGetDeviceParameterDword(
    IN     PDEVICE_OBJECT  Pdo,
    IN     PWSTR           ParameterName,
    IN OUT PULONG          ParameterValue
    );

NTSTATUS
PptRegSetDeviceParameterDword(
    IN PDEVICE_OBJECT  Pdo,
    IN PWSTR           ParameterName,
    IN PULONG          ParameterValue
    );

NTSTATUS
PptBuildParallelPortDeviceName(
    IN  ULONG           Number,
    OUT PUNICODE_STRING DeviceName
    );

NTSTATUS
PptInitializeDeviceExtension(
    IN PDRIVER_OBJECT  pDriverObject,
    IN PDEVICE_OBJECT  pPhysicalDeviceObject,
    IN PDEVICE_OBJECT  pDeviceObject,
    IN PUNICODE_STRING uniNameString,
    IN PWSTR           portName,
    IN ULONG           portNumber
    );

NTSTATUS
PptGetPortNumberFromLptName( 
    IN  PWSTR  PortName, 
    OUT PULONG PortNumber 
    );

PDEVICE_OBJECT
PptBuildDeviceObject( 
    IN PDRIVER_OBJECT pDriverObject, 
    IN PDEVICE_OBJECT pPhysicalDeviceObject 
    );

VOID
PptDetectEppPort(
    IN  PDEVICE_EXTENSION   Extension
    );

VOID
PptCleanRemovalRelationsList(
    IN PDEVICE_EXTENSION Extension
    );

