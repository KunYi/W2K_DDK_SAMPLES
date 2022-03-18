/*++

Copyright (C) Microsoft Corporation, 1997 - 1998

Module Name:

    adicsc.h

Abstract:

Authors:

Revision History:

--*/
#ifndef _ADIC_MC_
#define _ADIC_MC_

typedef struct _ADICS_ELEMENT_DESCRIPTOR {
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
    UCHAR Lun : 3;                      // true for drives only
    UCHAR Reserved6 : 1;                // true for drives only
    UCHAR LunValid : 1;                 // true for drives only
    UCHAR IdValid : 1;                  // true for drives only
    UCHAR Reserved7 : 1;                // true for drives only
    UCHAR NotThisBus : 1;               // true for drives only
    UCHAR BusAddress;                   // true for drives only
    UCHAR Reserved8;
    UCHAR Reserved9 : 6;
    UCHAR Invert : 1;
    UCHAR SValid : 1;
    UCHAR SourceStorageElementAddress[2];
    UCHAR Reserved10[4];
} ADICS_ELEMENT_DESCRIPTOR, *PADICS_ELEMENT_DESCRIPTOR;

typedef struct _ADICS_ELEMENT_DESCRIPTOR_PLUS {
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
    UCHAR VolumeTagInformation[36];
    UCHAR Reserved10[4];
} ADICS_ELEMENT_DESCRIPTOR_PLUS, *PADICS_ELEMENT_DESCRIPTOR_PLUS;


#define ADIC_NO_ELEMENT 0xFFFF
#endif

