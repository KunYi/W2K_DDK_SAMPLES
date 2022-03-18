/*++

Copyright (C) Microsoft Corporation, 1997 - 1998

Module Name:

    hpmc.h

Abstract:

Authors:

Revision History:

--*/

#ifndef _HP_MC_
#define _HP_MC_

typedef struct _SONY_ELEMENT_DESCRIPTOR {
    UCHAR ElementAddress[2];
    UCHAR Full : 1;
    UCHAR ImpExp : 1;
    UCHAR Exception : 1;
    UCHAR Accessible : 1;
    UCHAR InEnable : 1;
    UCHAR ExEnable : 1;
    UCHAR Reserved4 : 2;
    UCHAR Reserved5;
    UCHAR AdditionalSenseCode;
    UCHAR AddSenseCodeQualifier;
    UCHAR Lun : 3;
    UCHAR Reserved6 : 1;
    UCHAR LunValid : 1;
    UCHAR IdValid : 1;
    UCHAR Reserved7 : 1;
    UCHAR NotThisBus : 1;
    UCHAR BusAddress;
    UCHAR Reserved8;
    UCHAR Reserved9 : 6;
    UCHAR Invert : 1;
    UCHAR SValid : 1;
    UCHAR SourceStorageElementAddress[2];
} SONY_ELEMENT_DESCRIPTOR, *PSONY_ELEMENT_DESCRIPTOR;

typedef struct _SONY_ELEMENT_DESCRIPTOR_PLUS {
    UCHAR ElementAddress[2];
    UCHAR Full : 1;
    UCHAR ImpExp : 1;
    UCHAR Exception : 1;
    UCHAR Accessible : 1;
    UCHAR InEnable : 1;
    UCHAR ExEnable : 1;
    UCHAR Reserved4 : 2;
    UCHAR Reserved5;
    UCHAR AdditionalSenseCode;
    UCHAR AddSenseCodeQualifier;
    UCHAR Lun : 3;
    UCHAR Reserved6 : 1;
    UCHAR LunValid : 1;
    UCHAR IdValid : 1;
    UCHAR Reserved7 : 1;
    UCHAR NotThisBus : 1;
    UCHAR BusAddress;
    UCHAR Reserved8;
    UCHAR Reserved9 : 6;
    UCHAR Invert : 1;
    UCHAR SValid : 1;
    UCHAR SourceStorageElementAddress[2];
    UCHAR PVolTagInformation[36];
    UCHAR AVolTagInformation[36];
} SONY_ELEMENT_DESCRIPTOR_PLUS, *PSONY_ELEMENT_DESCRIPTOR_PLUS;

#define SONY_NO_ELEMENT          0xFFFF

#define SONY_SERIAL_NUMBER_LENGTH 16

typedef struct _SERIAL_NUMBER {
    UCHAR DeviceType;
    UCHAR PageCode;
    UCHAR Reserved;
    UCHAR PageLength;
    UCHAR ControllerSerialNumber[SONY_SERIAL_NUMBER_LENGTH];
    UCHAR MechanicalSerialNumber[SONY_SERIAL_NUMBER_LENGTH];
} SERIAL_NUMBER, *PSERIAL_NUMBER;

#define SCSIOP_ROTATE_MAILSLOT 0xC0
#define SONY_MAILSLOT_CLOSE 0x00
#define SONY_MAILSLOT_OPEN 0x01
#endif

