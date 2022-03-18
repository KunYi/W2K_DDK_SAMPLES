/*++

Copyright (C) Microsoft Corporation, 1997 - 1998

Module Name:

    nsmmc.h

Abstract:

Authors:

Revision History:

--*/

#ifndef _NSM_MC_
#define _NSM_MC_

typedef struct _NSM_STORAGE_ELEMENT_DESCRIPTOR {
    UCHAR ElementAddress[2];
    UCHAR Full : 1;
    UCHAR ImpExp : 1;
    UCHAR Exception : 1;
    UCHAR Accessible : 1;
    UCHAR ExEnable : 1;
    UCHAR InEnable : 1;
    UCHAR Reserved4 : 2;
    UCHAR Reserved5;
    UCHAR AdditionalSenseCode;
    UCHAR AdditionalSenseCodeQualifier;
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
} NSM_ELEMENT_DESCRIPTOR, *PNSM_ELEMENT_DESCRIPTOR;

#define NSM_NO_ELEMENT 0xFFFF


#define NSM_SERIAL_NUMBER_LENGTH        12

typedef struct _SERIALNUMBER {
    UCHAR DeviceType : 5;
    UCHAR PeripheralQualifier : 3;
    UCHAR PageCode;
    UCHAR Reserved;
    UCHAR PageLength;
    UCHAR SerialNumber[NSM_SERIAL_NUMBER_LENGTH];
} SERIALNUMBER, *PSERIALNUMBER;

#endif

