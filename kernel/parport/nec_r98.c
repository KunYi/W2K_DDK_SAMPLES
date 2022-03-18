//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1998 - 1999
//
//  File:       nec_r98.c
//
//--------------------------------------------------------------------------

#include "pch.h"

BOOLEAN
PptIsNecR98Machine(
    void
    )

/*++
      
Routine Description:
      
    This routine checks the machine type in the registry to determine
      if this is an Nec R98 machine.
      
Arguments:
      
    None.
      
Return Value:
      
    TRUE - this machine is an R98
    FALSE - this machine is not
      
      
--*/
    
{
    
    UNICODE_STRING              Path;
    RTL_QUERY_REGISTRY_TABLE    ParamTable[2];
    NTSTATUS                    Status;
    
    UNICODE_STRING identifierString;
    UNICODE_STRING necR98Identifier;
    UNICODE_STRING necR98JIdentifier;
    
    RtlInitUnicodeString(&Path, (PWSTR)L"\\Registry\\Machine\\HARDWARE\\DESCRIPTION\\System");
    RtlInitUnicodeString(&necR98Identifier, (PWSTR)L"NEC-R98");
    RtlInitUnicodeString(&necR98JIdentifier, (PWSTR)L"NEC-J98");
    
    
    identifierString.Length = 0;
    identifierString.MaximumLength = 32;
    identifierString.Buffer = ExAllocatePool(PagedPool, identifierString.MaximumLength);
    
    if(!identifierString.Buffer)    return FALSE;
    
    RtlZeroMemory(ParamTable, sizeof(ParamTable));
    ParamTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT |
                          RTL_QUERY_REGISTRY_REQUIRED;
    ParamTable[0].Name = (PWSTR)L"Identifier";
    ParamTable[0].EntryContext = &identifierString;
    ParamTable[0].DefaultType = REG_SZ;
    ParamTable[0].DefaultData = &Path;
    ParamTable[0].DefaultLength = 0;
    
    Status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE,
                                    Path.Buffer,
                                    ParamTable,
                                    NULL,
                                    NULL);
    
    
    if(NT_SUCCESS(Status))  {
        
        if((RtlCompareUnicodeString(&identifierString,
                                    &necR98Identifier, FALSE) == 0) ||
           (RtlCompareUnicodeString(&identifierString,
                                    &necR98JIdentifier, FALSE) == 0)) {
            
            PptDump(PARNECR98,
                    ("PARPORT: "
                     "parport!PptIsNecR98Machine - this an R98 machine\n") );
            
            ExFreePool(identifierString.Buffer);
            return TRUE;
        }
    } else {
        
        PptDump(PARNECR98, 
                ("PARPORT: "
                 "parport!PptIsNecR98Machine - "
                 "RtlQueryRegistryValues failed [status 0x%x]\n", 
                 Status) );
        
        ExFreePool(identifierString.Buffer);
        return FALSE;
    }
    
    PptDump(PARNECR98, 
            ("PARPORT: "
             "parport!PptIsNecR98Machine - this is not an R98 machine\n") );
    
    ExFreePool(identifierString.Buffer);
    return FALSE;
}

