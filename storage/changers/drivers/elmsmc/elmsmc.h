/*++

Copyright (C) Microsoft Corporation, 1997 - 1998

Module Name:

    elmsmc.h

Abstract:

Authors:

Revision History:

--*/

#ifndef _ELMS_MC_
#define _ELMS_MC_

typedef struct _ELMS_STORAGE_ELEMENT_DESCRIPTOR {
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
    UCHAR AdditionalSenseCodeQualifier;
} ELMS_STORAGE_ELEMENT_DESCRIPTOR, *PELMS_STORAGE_ELEMENT_DESCRIPTOR;

typedef struct _ELMS_ELEMENT_DESCRIPTOR {
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
    UCHAR Reserved10[4];
} ELMS_ELEMENT_DESCRIPTOR, *PELMS_ELEMENT_DESCRIPTOR;

#define ELMS_NO_ELEMENT 0xFFFF


typedef struct _SERIALNUMBER {
    UCHAR DeviceType;
    UCHAR PageCode;
    UCHAR Reserved;
    UCHAR PageLength;
    UCHAR SerialNumber[23];
    UCHAR Reserved1[6];
} SERIALNUMBER, *PSERIALNUMBER;

#endif

