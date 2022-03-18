//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1998 - 1999
//
//  File:       registry.c
//
//--------------------------------------------------------------------------

#include "pch.h"

VOID
PptRegInitDriverSettings (
    IN PUNICODE_STRING RegistryPath
   )
/*++

Routine Description:

    Initialize driver variables from registry; set to default values
      if anything fails.

Arguments:

    RegistryPath            - Root path in registry where we should look for values

Return Value:

    None        

--*/
{
    NTSTATUS                 Status;
    RTL_QUERY_REGISTRY_TABLE paramTable[3];
    PWSTR                    path;
    ULONG                    defaultDebugLevel = PARDUMP_SILENT;
    ULONG                    defaultBreakOn    = PAR_BREAK_ON_NOTHING;


    //
    // We were given a counted string, but we need a null terminated string
    //
    path = ExAllocatePool(PagedPool, RegistryPath->Length+sizeof(WCHAR));

    if (!path) {
        // can't get a buffer, use defaults and return
        PptDebugLevel = defaultDebugLevel;
        PptBreakOn    = defaultBreakOn;
        return;
    }

    RtlMoveMemory(path, RegistryPath->Buffer, RegistryPath->Length);
    path[ (RegistryPath->Length) / 2 ] = UNICODE_NULL;


    //
    // set up table entries for call to RtlQueryRegistryValues
    //
    RtlZeroMemory(&paramTable[0], sizeof(paramTable));

    paramTable[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[0].Name          = (PWSTR)L"PptDebugLevel";
    paramTable[0].EntryContext  = &PptDebugLevel;
    paramTable[0].DefaultType   = REG_DWORD;
    paramTable[0].DefaultData   = &defaultDebugLevel;
    paramTable[0].DefaultLength = sizeof(ULONG);

    paramTable[1].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[1].Name          = (PWSTR)L"PptBreakOn";
    paramTable[1].EntryContext  = &PptBreakOn;
    paramTable[1].DefaultType   = REG_DWORD;
    paramTable[1].DefaultData   = &defaultBreakOn;
    paramTable[1].DefaultLength = sizeof(ULONG);

    //
    // leave paramTable[2] as all zeros - this terminates the table
    //

    Status = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                     path,
                                     &paramTable[0],
                                     NULL,
                                     NULL);
       
    if (!NT_SUCCESS(Status)) {
        // registry read failed, use defaults
        PptDebugLevel = defaultDebugLevel;
        PptBreakOn    = defaultBreakOn;
    }

    ExFreePool(path);

    PptDumpV( ("PptDebugLevel = %08x , PptBreakOn = %08x\n", PptDebugLevel, PptBreakOn) );
}

NTSTATUS
PptRegGetDeviceParameterDword(
    IN     PDEVICE_OBJECT  Pdo,
    IN     PWSTR           ParameterName,
    IN OUT PULONG          ParameterValue
    )
/*++

Routine Description:

    retrieve a devnode registry parameter of type dword

Arguments:

    Pdo - ParPort PDO

    ParameterName - parameter name to look up

    ParameterValue - default parameter value

Return Value:

    Status - if RegKeyValue does not exist or other failure occurs,
               then default is returned via ParameterValue

--*/
{
    NTSTATUS                 status;
    HANDLE                   hKey;
    RTL_QUERY_REGISTRY_TABLE queryTable[2];
    ULONG                    defaultValue;

    PAGED_CODE();

    status = IoOpenDeviceRegistryKey(Pdo, PLUGPLAY_REGKEY_DEVICE, KEY_READ, &hKey);

    if(!NT_SUCCESS(status)) {
        return status;
    }

    defaultValue = *ParameterValue;

    RtlZeroMemory(&queryTable, sizeof(queryTable));

    queryTable[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    queryTable[0].Name          = ParameterName;
    queryTable[0].EntryContext  = ParameterValue;
    queryTable[0].DefaultType   = REG_DWORD;
    queryTable[0].DefaultData   = &defaultValue;
    queryTable[0].DefaultLength = sizeof(ULONG);

    status = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE, hKey, queryTable, NULL, NULL);

    if ( !NT_SUCCESS(status) ) {
        *ParameterValue = defaultValue;
    }

    ZwClose(hKey);

    return status;
}

NTSTATUS
PptRegSetDeviceParameterDword(
    IN PDEVICE_OBJECT  Pdo,
    IN PWSTR           ParameterName,
    IN PULONG          ParameterValue
    )
/*++

Routine Description:

    Create/set a devnode registry parameter of type dword

Arguments:

    Pdo - ParPort PDO

    ParameterName - parameter name

    ParameterValue - parameter value

Return Value:

    Status - status from attempt

--*/
{
    NTSTATUS                 status;
    HANDLE                   hKey;
    UNICODE_STRING           valueName;

    PAGED_CODE();

    status = IoOpenDeviceRegistryKey(Pdo, PLUGPLAY_REGKEY_DEVICE, KEY_WRITE, &hKey);

    if( !NT_SUCCESS( status ) ) {
        PptDump2(PARERRORS, ("registry::PptRegSetDeviceParameterDword - openKey FAILED - %x", status) );
        return status;
    }

    RtlInitUnicodeString( &valueName, ParameterName );

    status = ZwSetValueKey( hKey, &valueName, 0, REG_DWORD, ParameterValue, sizeof(ULONG) );
    if( !NT_SUCCESS( status ) ) {
        PptDump2(PARERRORS, ("registry::PptRegSetDeviceParameterDword - setValue FAILED - %x", status) );
    }

    ZwClose(hKey);

    return status;
}

