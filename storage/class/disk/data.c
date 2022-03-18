/*++

Copyright (C) Microsoft Corporation, 1991 - 1999

Module Name:

    disk.c

Abstract:

    SCSI disk class driver

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "disk.h"

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE")
#endif

#if 0
//
// Build a whole bunch of vendor strings so to initialize the tables with.
//

UCHAR _TOSHIBA[]="TOSHIBA";
UCHAR _CONNER[]="CONNER";
UCHAR _OLIVETTI[]="OLIVETTI";
UCHAR _SyQuest[]="SyQuest";
UCHAR _SEAGATE[]="SEAGATE";
UCHAR _FUJITSU[]="FUJITSU";
UCHAR _MAXTOR[]="MAXTOR";
UCHAR _MICROP[]="MICROP";
UCHAR _COMPAQ[]="COMPAQ";
UCHAR _HP[]="HP";
UCHAR _iomega[]="iomega";
UCHAR _IOMEGA[]="IOMEGA";
UCHAR _PINNACLE[]="PINNACLE";
UCHAR _SONY[] = "SONY";

UCHAR _MK538FB[]="MK538FB";
UCHAR _CP3500[]="CP3500";
UCHAR _SQ5110[]="SQ5110";
UCHAR _ST41601N[]="ST41601N";
UCHAR _ST3655N[]="ST3655N";
UCHAR _ST3390N[]="ST3390N";
UCHAR _ST12550N[]="ST12550N";
UCHAR _ST32430N[]="ST32430N";
UCHAR _ST31230N[]="ST31230N";
UCHAR _ST15230N[]="ST15230N";
UCHAR _M2652S_512[]="M2652S-512";
UCHAR _MXT_540SL[]="MXT-540SL";
UCHAR _1936_21MW1002002[]="1936-21MW1002002";
UCHAR _PD_1[]="PD-1";

UCHAR _jaz[]="jaz";
UCHAR _ZIP[]="ZIP";
UCHAR _PD_1_LF_1094[]="PD-1 LF-1094";
UCHAR _Apex_4_6GB[]="Apex 4.6GB";
UCHAR _SMO_F541[]="SMO-F541";

UCHAR _60[]="60";
UCHAR _CHC[]="CHC";
UCHAR _0102[]="0102";
UCHAR _I1_2[]="I1.2";

#endif

/*

typedef struct _BAD_CONTROLLER_INFORMATION {
    PCHAR   VendorId;
    PCHAR   ProductId;
    PCHAR   ProductRevision;
    const ULONG DisableTaggedQueuing : 1;
    const ULONG DisableSynchronousTransfers : 1;
    const ULONG DisableSpinDown : 1;
    const ULONG DisableWriteCache : 1;
    const ULONG CauseNotReportableHack : 1;
}BAD_CONTROLLER_INFORMATION, *PBAD_CONTROLLER_INFORMATION;

*/

BAD_CONTROLLER_INFORMATION const DiskBadControllers[] = {
    { "COMPAQ"  , "PD-1"            , NULL,   FALSE, TRUE,  FALSE, FALSE, FALSE },
    { "CONNER"  , "CP3500"          , NULL,   FALSE, TRUE,  FALSE, FALSE, FALSE },
    { "FUJITSU" , "M2652S-512"      , NULL,   TRUE,  FALSE, FALSE, FALSE, FALSE },
    { "iomega"  , "jaz"             , NULL,   FALSE, FALSE, FALSE, FALSE, TRUE  },
    { "IOMEGA"  , "ZIP"             , NULL,   TRUE,  TRUE,  TRUE,  FALSE, FALSE },
    { "MAXTOR"  , "MXT-540SL"       , "I1.2", TRUE,  FALSE, FALSE, FALSE, FALSE },
    { "MICROP"  , "1936-21MW1002002", NULL,   TRUE,  TRUE,  FALSE, FALSE, FALSE },
    { "OLIVETTI", "CP3500"          , NULL,   FALSE, TRUE,  FALSE, FALSE, FALSE },
    { "SEAGATE" , "ST41601N"        , "0102", FALSE, TRUE,  FALSE, FALSE, FALSE },
    { "SEAGATE" , "ST3655N"         , NULL,   FALSE, FALSE, FALSE, TRUE,  FALSE },
    { "SEAGATE" , "ST3390N"         , NULL,   FALSE, FALSE, FALSE, TRUE,  FALSE },
    { "SEAGATE" , "ST12550N"        , NULL,   FALSE, FALSE, FALSE, TRUE,  FALSE },
    { "SEAGATE" , "ST32430N"        , NULL,   FALSE, FALSE, FALSE, TRUE,  FALSE },
    { "SEAGATE" , "ST31230N"        , NULL,   FALSE, FALSE, FALSE, TRUE,  FALSE },
    { "SEAGATE" , "ST15230N"        , NULL,   FALSE, FALSE, FALSE, TRUE,  FALSE },
    { "SyQuest" , "SQ5110"          , "CHC",  TRUE,  TRUE,  FALSE, FALSE, FALSE },
    { "TOSHIBA" , "MK538FB"         , "60",   TRUE,  FALSE, FALSE, FALSE, FALSE },

//  { "SEAGATE" , "ST34371W"        , NULL,   FALSE, FALSE, TRUE,  FALSE, FALSE },
//  { "WDIGTL"  , "ENTERPRISE"      , NULL,   FALSE, FALSE, TRUE,  FALSE, FALSE },

    { NULL      , NULL              , NULL,   FALSE, FALSE, FALSE, FALSE, FALSE }
};

DISK_MEDIA_TYPES_LIST const DiskMediaTypes[] = {
    { "COMPAQ"  , "PD-1 LF-1094" , NULL,  1, 1, PC_5_RW           , 0      , 0      , 0 },
    { "HP"      , NULL           , NULL,  2, 2, MO_5_WO           , MO_5_RW, 0      , 0 },
    { "iomega"  , "jaz"          , NULL,  1, 1, IOMEGA_JAZ        , 0      , 0      , 0 },
    { "IOMEGA"  , "ZIP"          , NULL,  1, 1, IOMEGA_ZIP        , 0      , 0      , 0 },
    { "PINNACLE", "Apex 4.6GB"   , NULL,  3, 2, PINNACLE_APEX_5_RW, MO_5_RW, MO_5_WO, 0 },
    { "SONY"    , "SMO-F541"     , NULL,  2, 2, MO_5_WO           , MO_5_RW, 0      , 0 },
    { "SONY"    , "SMO-F551"     , NULL,  2, 2, MO_5_WO           , MO_5_RW, 0      , 0 },
    { NULL      , NULL           , NULL,  0, 0, 0                 , 0      , 0      , 0 }
};

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg()
#endif

