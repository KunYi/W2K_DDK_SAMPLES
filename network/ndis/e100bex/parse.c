/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    parse.c

This driver runs on the following hardware:
    - 82557/82558 based PCI 10/100Mb ethernet adapters
    (aka Intel EtherExpress(TM) PRO Adapters)

Environment:
    Kernel Mode - Or whatever is the equivalent on WinNT

Revision History
    - JCB 8/14/97 Example Driver Created
*****************************************************************************/

#include "precomp.h"
#define NUM_REG_PARAM ( sizeof (D100GlobalRegTab) / sizeof (D100RegTabType) )

#pragma hdrstop
#pragma warning (disable: 4057 4514 )

//-----------------------------------------------------------------------------
// D100RegTabType
//
//      One instance of this structure will be used for every configuration
//      parameter that this driver supports.  The table contains all of the
//      relavent information about each parameter:  Name, whether or not it is
//      required, where it is located in the "Adapter" structure, the size of
//      the parameter in bytes, the default value for the parameter, and what
//      the minimum and maximum values are for the parameter.  In the debug
//      version of the driver, this table also contains a field for the ascii
//      name of the parameter.
//-----------------------------------------------------------------------------
typedef struct _D100RegTabType {

    NDIS_STRING RegVarName;             // variable name text
    char       *RegAscName;             // variable name text
    UINT        Mandantory;             // 1 -> manditory, 0 -> optional
    UINT        FieldOffset;            // offset to D100_ADAPTER field loaded
    UINT        FieldSize;              // size (in bytes) of the field
    UINT        Default;                // default value to use
    UINT        Min;                    // minimum value allowed
    UINT        Max;                    // maximum value allowed

} D100RegTabType;


//-----------------------------------------------------------------------------
// Global Registry Parameters Table
//
//      This table contains a list of all of the configuration parameters
//      that the driver supports.  The driver will attempt to find these
//      parameters in the registry and use the registry value for these
//      parameters.  If the parameter is not found in the registry, then the
//      default value is used.
//
//-----------------------------------------------------------------------------

D100RegTabType D100GlobalRegTab[ ] = {
    //                                                             Offset in to Adapter Struct                                 Default Value                   Min          Max
#if DBG
    {NDIS_STRING_CONST("Debug"),            "Debug",            0, D100_OFFSET(Debug),           D100_SIZE(Debug),             DBG_NORMAL,                      0,          0xffffffff},
#endif
    {NDIS_STRING_CONST("BusNumber"),        "BusNumber",        0, D100_OFFSET(BusNumber),       D100_SIZE(BusNumber),         0,                               0,          16},
    {NDIS_STRING_CONST("SlotNumber"),       "SlotNumber",       0, D100_OFFSET(AiSlot),          D100_SIZE(AiSlot),            0,                               0,          32},
    {NDIS_STRING_CONST("NumRfd"),           "NumRfd",           0, D100_OFFSET(NumRfd),          D100_SIZE(NumRfd),            32,                              1,          MAX_RECEIVE_DESCRIPTORS},
    {NDIS_STRING_CONST("NumTcb"),           "NumTcb",           0, D100_OFFSET(RegNumTcb),       D100_SIZE(RegNumTcb),         16,                              1,          0x40},
    {NDIS_STRING_CONST("NumTbdPerTcb"),     "NumTbdPerArray",   0, D100_OFFSET(NumTbdPerTcb),    D100_SIZE(NumTbdPerTcb),      8,                               1,          MAX_PHYS_DESC},
    {NDIS_STRING_CONST("NumCoalesce"),      "NumCoalesce",      0, D100_OFFSET(NumCoalesce),     D100_SIZE(NumCoalesce),       8,                               1,          32},
    {NDIS_STRING_CONST("MapRegisters"),     "MapRegisters",     0, D100_OFFSET(NumMapRegisters), D100_SIZE(NumMapRegisters),   64,                              0,          0xffff},
    {NDIS_STRING_CONST("PhyAddress"),       "PhyAddress",       0, D100_OFFSET(PhyAddress),      D100_SIZE(PhyAddress),        0xFF,                            0,          0xFF},
    {NDIS_STRING_CONST("Connector"),        "Connector",        0, D100_OFFSET(Connector),       D100_SIZE(Connector),         0,                               0,          0x2},
    {NDIS_STRING_CONST("BusTypeLocal"),     "BusTypeLocal",     0, D100_OFFSET(AiBusType),       D100_SIZE(AiBusType),         5,                               EISABUS,    PCIBUS},
    {NDIS_STRING_CONST("TxFifo"),           "TxFifo",           0, D100_OFFSET(AiTxFifo),        D100_SIZE(AiTxFifo),          DEFAULT_TX_FIFO_LIMIT,           0,          15},
    {NDIS_STRING_CONST("RxFifo"),           "RxFifo",           0, D100_OFFSET(AiRxFifo),        D100_SIZE(AiRxFifo),          DEFAULT_RX_FIFO_LIMIT,           0,          15},
    {NDIS_STRING_CONST("TxDmaCount"),       "TxDmaCount",       0, D100_OFFSET(AiTxDmaCount),    D100_SIZE(AiTxDmaCount),      0,                               0,          63},
    {NDIS_STRING_CONST("RxDmaCount"),       "RxDmaCount",       0, D100_OFFSET(AiRxDmaCount),    D100_SIZE(AiRxDmaCount),      0,                               0,          63},
    {NDIS_STRING_CONST("UnderrunRetry"),    "UnderrunRetry",    0, D100_OFFSET(AiUnderrunRetry), D100_SIZE(AiUnderrunRetry),   DEFAULT_UNDERRUN_RETRY,          0,          3},
    {NDIS_STRING_CONST("ForceDpx"),         "ForceDpx",         0, D100_OFFSET(AiForceDpx),      D100_SIZE(AiForceDpx),        0,                               0,          2},
    {NDIS_STRING_CONST("Speed"),            "Speed",            0, D100_OFFSET(AiTempSpeed),     D100_SIZE(AiTempSpeed),       0,                               0,          100},
    {NDIS_STRING_CONST("Threshold"),        "Threshold",        0, D100_OFFSET(AiThreshold),     D100_SIZE(AiThreshold),       200,                             0,          200},
    {NDIS_STRING_CONST("MCWA"),             "MCWA",             0, D100_OFFSET(McTimeoutFlag),   D100_SIZE(McTimeoutFlag),     2,                               0,          2},
    {NDIS_STRING_CONST("MWIEnable"),        "MWIEnable",        0, D100_OFFSET(MWIEnable),       D100_SIZE(MWIEnable),         1,                               0,          1},
    {NDIS_STRING_CONST("Congest"),          "Congest",          0, D100_OFFSET(Congest),         D100_SIZE(Congest),           0,                               0,          0x1}
};



NDIS_STATUS
ParseRegistryParameters(
                        IN PD100_ADAPTER Adapter,
                        IN NDIS_HANDLE ConfigHandle
                        );


//-----------------------------------------------------------------------------
// Procedure:   ParseRegistryParameters
//
// Description: This routine will parse all of the parameters out of the
//              registry/PROTOCOL.INI, and store the values in the "Adapter"
//              Structure.  If the parameter is not present in the registry,
//              then the default value for the parameter will be placed into
//              the "Adapter" structure.  This routine also checks the validity
//              of the parameter value, and if the value is out of range, the
//              driver will the min/max value allowed.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//      ConfigHandle - NDIS Configuration Registery handle
//
// Returns:
//      NDIS_STATUS_SUCCESS - All mandatory parameters were parsed
//      NDIS_STATUS_FAILED - A mandatory parameter was not present
//-----------------------------------------------------------------------------

NDIS_STATUS
ParseRegistryParameters(
                        IN PD100_ADAPTER Adapter,
                        IN NDIS_HANDLE ConfigHandle
                        )

{
    NDIS_STATUS         Status;
    D100RegTabType      *RegTab;
    UINT                i;
    UINT                value;
    PUCHAR              fieldPtr;

#if DBG
    char                ansiRegName[32];
#endif

    PNDIS_CONFIGURATION_PARAMETER ReturnedValue;

    DEBUGFUNC("ParseRegistryParameters");

    INITSTR(("\n"));


    // Grovel through the registry parameters and aquire all of the values
    // stored therein.
    for (i = 0, RegTab = D100GlobalRegTab; i < NUM_REG_PARAM; i++, RegTab++)
    {
        fieldPtr = ((PUCHAR) Adapter) + RegTab->FieldOffset;

#if DBG
        strcpy(ansiRegName, RegTab->RegAscName);
#endif

        // Get the configuration value for a specific parameter.  Under NT the
        // parameters are all read in as DWORDs.
        NdisReadConfiguration(&Status,
            &ReturnedValue,
            ConfigHandle,
            &RegTab->RegVarName,
            NdisParameterInteger);

        // If the parameter was present, then check its value for validity.
        if (Status == NDIS_STATUS_SUCCESS)
        {
            // Check that param value is not too small or too large
            if (ReturnedValue->ParameterData.IntegerData < RegTab->Min ||
                ReturnedValue->ParameterData.IntegerData > RegTab->Max)
            {
                value = RegTab->Default;
            }

            // Use the value if it is within range
            else
            {
                value = ReturnedValue->ParameterData.IntegerData;
            }

            INITSTR(("%-25s 0x%X\n", ansiRegName, value));
        }

        // If a mandatory parameter wasn't present then error out.
        else if (RegTab->Mandantory)
        {
            DEBUGSTR(("Could not find mandantory '%s' in registry\n\n", ansiRegName));
#if DBG
            DbgBreakPoint();
#endif
                return (NDIS_STATUS_FAILURE);
        }

        // If a non-mandatory parameter wasn't present, then set it to its
        // default value.
        else
        {
            value = RegTab->Default;
            INITSTR(("%-25s 0x%X\n", ansiRegName, value));
        }

        // Store the value in the adapter structure.
        switch (RegTab->FieldSize)
        {
        case 1:
            {
                *((PUCHAR) fieldPtr) = (UCHAR) value;
                break;
            }
        case 2:
            {
                *((PUSHORT) fieldPtr) = (USHORT) value;
                break;
            }
        case 4:
            {
                *((PULONG) fieldPtr) = (ULONG) value;
                break;
            }
        default:
            DEBUGSTR(("Bogus field size %d\n", RegTab->FieldSize));
            break;
        }
    }
    return NDIS_STATUS_SUCCESS;
                        }

