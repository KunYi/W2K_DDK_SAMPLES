//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1998 - 1999
//
//  File:       parmode.h
//
//--------------------------------------------------------------------------

// Register Definitions for National ChipSets
#define  REG_CR0                    0x00
#define  REG_CR1                    0x01
#define  REG_CR2                    0x02
#define  REG_CR3                    0x03
#define  REG_CR4                    0x04
#define  REG_CR5                    0x05
#define  REG_CR6                    0x06
#define  REG_CR7                    0x07
#define  REG_CR8                    0x08

// National Chip ID's
#define PC87303                     0x30
#define PC87306                     0x70
#define PC87307                     0xC0
#define PC87308                     0xA0
#define PC87323                     0x20
#define PC87332                     0x10
#define PC87334                     0x50
#define PC87336                     0x90
#define PC87338                     0xB0
#define PC873xx                     0x60

// Additional definitions for National PC87307 and PC87308
#define PC873_LOGICAL_DEV_REG       0x07
#define PC873_PP_LDN                0x04
#define PC873_DEVICE_ID             0x20
#define PC873_PP_MODE_REG           0xF0
#define PC873_ECP_MODE              0xF2
#define PC873_EPP_MODE              0x62
#define PC873_SPP_MODE              0x92
#define PC873_BASE_IO_ADD_MSB       0x60
#define PC873_BASE_IO_ADD_LSB       0x61

NTSTATUS
PptDetectChipFilter(
    IN  PDEVICE_EXTENSION   Extension
    );

NTSTATUS
PptDetectPortType(
    IN  PDEVICE_EXTENSION   Extension
    );
    
NTSTATUS
PptDetectPortCapabilities(
    IN  PDEVICE_EXTENSION   Extension
    );
    
VOID
PptDetectEcpPort(
    IN  PDEVICE_EXTENSION   Extension
    );

VOID
PptDetectEppPortIfDot3DevicePresent(
    IN  PDEVICE_EXTENSION   Extension
    );

VOID
PptDetectEppPortIfUserRequested(
    IN  PDEVICE_EXTENSION   Extension
    );

VOID
PptDetectBytePort(
    IN  PDEVICE_EXTENSION   Extension
    );

VOID 
PptDetermineFifoDepth(
    IN PDEVICE_EXTENSION   Extension
    );

VOID
PptDetermineFifoWidth(
    IN PDEVICE_EXTENSION   Extension
    );

NTSTATUS
PptSetChipMode (
    IN  PDEVICE_EXTENSION  Extension,
    IN  UCHAR              ChipMode
    );

NTSTATUS
PptClearChipMode (
    IN  PDEVICE_EXTENSION  Extension,
    IN  UCHAR              ChipMode
    );

NTSTATUS
PptEcrSetMode(
    IN  PDEVICE_EXTENSION   Extension,
    IN  UCHAR               ChipMode
    );

NTSTATUS
PptCheckBidiMode(
    IN  PDEVICE_EXTENSION   Extension
    );

NTSTATUS
PptEcrClearMode(
    IN  PDEVICE_EXTENSION   Extension
    );
    
NTSTATUS
PptFindNatChip(
    IN  PDEVICE_EXTENSION   Extension
    );

NTSTATUS
PptBuildResourceList(
    IN  PDEVICE_EXTENSION   Extension,
    IN  ULONG               Partial,
    IN  PVOID               Addresses,
    OUT PCM_RESOURCE_LIST   Resources
    );

NTSTATUS
PptSetByteMode( 
    IN  PDEVICE_EXTENSION   Extension,
    IN  UCHAR               ChipMode
    );

NTSTATUS
PptClearByteMode( 
    IN  PDEVICE_EXTENSION   Extension
    );

NTSTATUS
PptCheckByteMode(
    IN  PDEVICE_EXTENSION   Extension
    );




