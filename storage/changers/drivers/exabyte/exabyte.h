/*++

Copyright (C) Microsoft Corporation, 1997 - 1998

Module Name:

    exabyte.h

Abstract:

Authors:

Revision History:

--*/

#ifndef _EXABYTE_MC_
#define _EXABYTE_MC_

//
// Exabyte uses an addition 4 bytes on their device capabilities page...
//

#define EXABYTE_DEVICE_CAP_EXTENSION 4

typedef union _EXA_ELEMENT_DESCRIPTOR {

    struct _EXA_FULL_ELEMENT_DESCRIPTOR {
        UCHAR ElementAddress[2];
        UCHAR Full : 1;
        UCHAR Reserved1 : 1;
        UCHAR Exception : 1;
        UCHAR Accessible : 1;
        UCHAR Reserved2 : 4;
        UCHAR Reserved3;
        UCHAR AdditionalSenseCode;
        UCHAR AddSenseCodeQualifier;
        UCHAR Lun : 3;
        UCHAR Reserved4 : 1;
        UCHAR LunValid : 1;
        UCHAR IdValid : 1;
        UCHAR Reserved5 : 1;
        UCHAR NotThisBus : 1;
        UCHAR BusAddress;
        UCHAR Reserved6;
        UCHAR Reserved7 : 6;
        UCHAR Invert : 1;
        UCHAR SValid : 1;
        UCHAR SourceStorageElementAddress[2];
        UCHAR PrimaryVolumeTag[36];
        UCHAR Reserved8[4];
    } EXA_FULL_ELEMENT_DESCRIPTOR, *PEXA_FULL_ELEMENT_DESCRIPTOR;

    struct _EXA_PARTIAL_ELEMENT_DESCRIPTOR {
        UCHAR ElementAddress[2];
        UCHAR Full : 1;
        UCHAR Reserved1 : 1;
        UCHAR Exception : 1;
        UCHAR Accessible : 1;
        UCHAR Reserved2 : 4;
        UCHAR Reserved3;
        UCHAR AdditionalSenseCode;
        UCHAR AddSenseCodeQualifier;
        UCHAR Lun : 3;
        UCHAR Reserved4 : 1;
        UCHAR LunValid : 1;
        UCHAR IdValid : 1;
        UCHAR Reserved5 : 1;
        UCHAR NotThisBus : 1;
        UCHAR BusAddress;
        UCHAR Reserved6;
        UCHAR Reserved7 : 6;
        UCHAR Invert : 1;
        UCHAR SValid : 1;
        UCHAR SourceStorageElementAddress[2];
        UCHAR Reserved8[4];
    } EXA_PARTIAL_ELEMENT_DESCRIPTOR, *PEXA_PARTIAL_ELEMENT_DESCRIPTOR;

} EXA_ELEMENT_DESCRIPTOR, *PEXA_ELEMENT_DESCRIPTOR;

#define EXA_PARTIAL_SIZE sizeof(struct _EXA_PARTIAL_ELEMENT_DESCRIPTOR)
#define EXA_FULL_SIZE sizeof(struct _EXA_FULL_ELEMENT_DESCRIPTOR)

#define EXA_DISPLAY_LINES        4
#define EXA_DISPLAY_LINE_LENGTH 20

typedef struct _LCD_MODE_PAGE {
    UCHAR PageCode : 6;
    UCHAR Reserved1 : 1;
    UCHAR PSBit : 1;
    UCHAR PageLength;
    UCHAR WriteLine : 4;
    UCHAR Reserved2 : 2;
    UCHAR LCDSecurity : 1;
    UCHAR SecurityValid : 1;
    UCHAR Reserved4;
    UCHAR DisplayLine[4][EXA_DISPLAY_LINE_LENGTH];
} LCD_MODE_PAGE, *PLCD_MODE_PAGE;

#define EXA_NO_ELEMENT 0xFFFF
#endif

