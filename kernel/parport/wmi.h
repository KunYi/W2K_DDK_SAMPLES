//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1998 - 1999
//
//  File:       wmi.h
//
//--------------------------------------------------------------------------

NTSTATUS
PptWmiQueryWmiRegInfo(
    IN  PDEVICE_OBJECT  PDevObj, 
    OUT PULONG          PRegFlags,
    OUT PUNICODE_STRING PInstanceName,
    OUT PUNICODE_STRING *PRegistryPath,
    OUT PUNICODE_STRING MofResourceName,
    OUT PDEVICE_OBJECT  *Pdo 
);

NTSTATUS
PptWmiQueryWmiDataBlock(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN ULONG            GuidIndex,
    IN ULONG            InstanceIndex,
    IN ULONG            InstanceCount,
    IN OUT PULONG       InstanceLengthArray,
    IN ULONG            OutBufferSize,
    OUT PUCHAR          Buffer
    );

