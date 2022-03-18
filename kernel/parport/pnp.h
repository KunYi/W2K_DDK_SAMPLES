/*++

Copyright (C) Microsoft Corporation, 1998 - 1999

Module Name:

    parport.sys

File Name:

    pnp.h

Abstract:

    This file contains forward declarations for functions private to pnp.c

--*/

NTSTATUS
PptPnpFilterResourceRequirements(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    );

NTSTATUS
PptPnpQueryDeviceRelations(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    );

NTSTATUS
PptPnpQueryStopDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp
    );

NTSTATUS
PptPnpCancelStopDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    );

NTSTATUS
PptPnpStopDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    );

NTSTATUS
PptPnpQueryRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    );

NTSTATUS
PptPnpCancelRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    );

NTSTATUS
PptPnpRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    );

NTSTATUS
PptPnpSurpriseRemoval(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    );

NTSTATUS
PptPnpUnhandledIrp(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    );

NTSTATUS
PptPnpStartDevice(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP           Irp
    );

NTSTATUS
PptPnpStartValidateResources(
    IN PDEVICE_OBJECT DeviceObject,                              
    IN BOOLEAN        FoundPort,
    IN BOOLEAN        FoundIrq,
    IN BOOLEAN        FoundDma
    );

NTSTATUS
PptPnpStartScanCmResourceList(
    IN  PDEVICE_EXTENSION Extension,
    IN  PIRP              Irp, 
    OUT PBOOLEAN          FoundPort,
    OUT PBOOLEAN          FoundIrq,
    OUT PBOOLEAN          FoundDma
    );

NTSTATUS
PptPnpPassThroughPnpIrpAndReleaseRemoveLock(
    IN PDEVICE_EXTENSION Extension,
    IN PIRP              Irp
    );

NTSTATUS
PptPnpRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

VOID
PptPnpFilterNukeIrqResourceDescriptors(
    IN OUT PIO_RESOURCE_LIST IoResourceList
    );

VOID
PptPnpFilterNukeIrqResourceDescriptorsFromAllLists(
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST ResourceRequirementsList
    );

BOOLEAN
PptPnpFilterExistsNonIrqResourceList(
    IN PIO_RESOURCE_REQUIREMENTS_LIST ResourceRequirementsList
    );

PVOID
PptPnpFilterGetEndOfResourceRequirementsList(
    IN PIO_RESOURCE_REQUIREMENTS_LIST ResourceRequirementsList
    );

BOOLEAN
PptPnpListContainsIrqResourceDescriptor(
    IN PIO_RESOURCE_LIST List
    );

VOID
PptPnpFilterRemoveIrqResourceLists(
    PIO_RESOURCE_REQUIREMENTS_LIST ResourceRequirementsList
    );

NTSTATUS
PptPnpBounceAndCatchPnpIrp(
    IN PDEVICE_EXTENSION Extension,
    IN PIRP              Irp
    );

