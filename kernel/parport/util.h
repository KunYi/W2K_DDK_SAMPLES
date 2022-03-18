//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1998 - 1999
//
//  File:       util.h
//
//--------------------------------------------------------------------------


#ifndef _UTIL_
#define _UTIL_

#define GetStatus(RegisterBase) \
    (READ_PORT_UCHAR((RegisterBase)+DSR_OFFSET))

#define GetControl(RegisterBase) \
    (READ_PORT_UCHAR((RegisterBase)+DCR_OFFSET))


#define StoreControl(RegisterBase,ControlByte)  \
{                                               \
    WRITE_PORT_UCHAR((RegisterBase)+DCR_OFFSET, \
                     (UCHAR)ControlByte );      \
}

// The following macros may be used to test the contents of the Device
// Status Regisger (DSR).  These macros account for the hardware
// inversion of the nBusy (aka PtrBusy, PeriphAck) signal.
//////////////////////////////////////////////////////////////////////////////

#if (1 == DVRH_USE_FAST_MACROS)
    #define DSR_TEST_MASK(b7,b6,b5,b4,b3)  \
    ((UCHAR)(b7==DONT_CARE? 0:  BIT_7_SET) | \
            (b6==DONT_CARE? 0:  BIT_6_SET) | \
            (b5==DONT_CARE? 0:  BIT_5_SET) | \
            (b4==DONT_CARE? 0:  BIT_4_SET) | \
            (b3==DONT_CARE? 0:  BIT_3_SET) )
#else
    #define DSR_TEST_MASK(b7,b6,b5,b4,b3)  \
    ((UCHAR)((b7==DONT_CARE?0:1)<<BIT_7) | \
            ((b6==DONT_CARE?0:1)<<BIT_6) | \
            ((b5==DONT_CARE?0:1)<<BIT_5) | \
            ((b4==DONT_CARE?0:1)<<BIT_4) | \
            ((b3==DONT_CARE?0:1)<<BIT_3) )
#endif

#if (1 == DVRH_USE_FAST_MACROS)
    #define DSR_TEST_VALUE(b7,b6,b5,b4,b3)  \
    ((UCHAR) ((b7==DONT_CARE?0:(b7==ACTIVE?0        : BIT_7_SET)) | \
            (b6==DONT_CARE?0:(b6==ACTIVE?  BIT_6_SET: 0)) | \
            (b5==DONT_CARE?0:(b5==ACTIVE?  BIT_5_SET: 0)) | \
            (b4==DONT_CARE?0:(b4==ACTIVE?  BIT_4_SET: 0)) | \
            (b3==DONT_CARE?0:(b3==ACTIVE?  BIT_3_SET: 0)) ) )
#else
    #define DSR_TEST_VALUE(b7,b6,b5,b4,b3)  \
    ((UCHAR) (((b7==DONT_CARE?0:(b7==ACTIVE?0:1))<<BIT_7) | \
            ((b6==DONT_CARE?0:(b6==ACTIVE?1:0))<<BIT_6) | \
            ((b5==DONT_CARE?0:(b5==ACTIVE?1:0))<<BIT_5) | \
            ((b4==DONT_CARE?0:(b4==ACTIVE?1:0))<<BIT_4) | \
            ((b3==DONT_CARE?0:(b3==ACTIVE?1:0))<<BIT_3) ) )
#endif

#define TEST_DSR(registerValue,b7,b6,b5,b4,b3)  \
(((registerValue) & DSR_TEST_MASK(b7,b6,b5,b4,b3)) == DSR_TEST_VALUE(b7,b6,b5,b4,b3))


#define CHECK_DSR( addr, b7, b6, b5, b4, b3, usTime )                    \
    (TEST_DSR(READ_PORT_UCHAR(addr + DSR_OFFSET), b7, b6, b5, b4, b3 ) ? TRUE :   \
    CheckPort( addr + DSR_OFFSET,                                               \
             DSR_TEST_MASK( b7, b6, b5, b4, b3 ),                                   \
             DSR_TEST_VALUE( b7, b6, b5, b4, b3 ),                                  \
             usTime ) )

////////////////////////////////////////////////////////////////////////////////
// The CHECK_DSR_AND_FIFO macro may be used to invoke the CheckPort2 function, 
// without having to specify the mask and value components twice.
// CHECK_DSR_AND_FIFO does quick tests of the DSR and ECR ports first.
// If the peripheral has already responded with either of the
//  desired values, CheckPort2 need not be called.
////////////////////////////////////////////////////////////////////////////////

#define CHECK_DSR_WITH_FIFO( addr, b7, b6, b5, b4, b3, ecr_mask, ecr_value, msTime ) \
( TEST_DSR( READ_PORT_UCHAR( addr + OFFSET_DSR ), b7, b6, b5, b4, b3 ) ? TRUE :       \
  CheckTwoPorts( addr + OFFSET_DSR,                                  \
                 DSR_TEST_MASK( b7, b6, b5, b4, b3 ),                \
                 DSR_TEST_VALUE( b7, b6, b5, b4, b3 ),               \
                 addr + ECR_OFFSET,                                  \
                 ecr_mask,                                           \
                 ecr_value,                                          \
                 msTime) )

//////////////////////////////////////////////////////////////////////////////
// The following defines and macros may be used to set, test, and
// update the Device Control Register (DCR).
//////////////////////////////////////////////////////////////////////////////

// The DCR_AND_MASK macro generates a byte constant that is used by
// the UPDATE_DCR macro.

#if (1 == DVRH_USE_FAST_MACROS)
    #define DCR_AND_MASK(b5,b4,b3,b2,b1,b0) \
    ((UCHAR)((b5==DONT_CARE?   BIT_5_SET:(b5==ACTIVE?  BIT_5_SET:  0)) | \
            (b4==DONT_CARE?    BIT_4_SET:(b4==ACTIVE?  BIT_4_SET:  0)) | \
            (b3==DONT_CARE?    BIT_3_SET:(b3==ACTIVE?  0:          BIT_3_SET)) | \
            (b2==DONT_CARE?    BIT_2_SET:(b2==ACTIVE?  BIT_2_SET:  0)) | \
            (b1==DONT_CARE?    BIT_1_SET:(b1==ACTIVE?  0:          BIT_1_SET)) | \
            (b0==DONT_CARE?    BIT_0_SET:(b0==ACTIVE?  0:          BIT_0_SET)) ) )
#else
    #define DCR_AND_MASK(b5,b4,b3,b2,b1,b0) \
    ((UCHAR)(((b5==DONT_CARE?1:(b5==ACTIVE?1:0))<<BIT_5) | \
            ((b4==DONT_CARE?1:(b4==ACTIVE?1:0))<<BIT_4) | \
            ((b3==DONT_CARE?1:(b3==ACTIVE?0:1))<<BIT_3) | \
            ((b2==DONT_CARE?1:(b2==ACTIVE?1:0))<<BIT_2) | \
            ((b1==DONT_CARE?1:(b1==ACTIVE?0:1))<<BIT_1) | \
            ((b0==DONT_CARE?1:(b0==ACTIVE?0:1))<<BIT_0) ) )
#endif  

// The DCR_OR_MASK macro generates a byte constant that is used by
// the UPDATE_DCR macro.

#if (1 == DVRH_USE_FAST_MACROS)
    #define DCR_OR_MASK(b5,b4,b3,b2,b1,b0) \
    ((UCHAR)((b5==DONT_CARE?   0:(b5==ACTIVE?  BIT_5_SET:  0)) | \
            (b4==DONT_CARE?    0:(b4==ACTIVE?  BIT_4_SET:  0)) | \
            (b3==DONT_CARE?    0:(b3==ACTIVE?  0:          BIT_3_SET)) | \
            (b2==DONT_CARE?    0:(b2==ACTIVE?  BIT_2_SET:  0)) | \
            (b1==DONT_CARE?    0:(b1==ACTIVE?  0:          BIT_1_SET)) | \
            (b0==DONT_CARE?    0:(b0==ACTIVE?  0:          BIT_0_SET)) ) )
#else
    #define DCR_OR_MASK(b5,b4,b3,b2,b1,b0) \
    ((UCHAR)(((b5==DONT_CARE?0:(b5==ACTIVE?1:0))<<BIT_5) | \
            ((b4==DONT_CARE?0:(b4==ACTIVE?1:0))<<BIT_4) | \
            ((b3==DONT_CARE?0:(b3==ACTIVE?0:1))<<BIT_3) | \
            ((b2==DONT_CARE?0:(b2==ACTIVE?1:0))<<BIT_2) | \
            ((b1==DONT_CARE?0:(b1==ACTIVE?0:1))<<BIT_1) | \
            ((b0==DONT_CARE?0:(b0==ACTIVE?0:1))<<BIT_0) ) )
#endif
// The UPDATE_DCR macro generates provides a selective update of specific bits
// in the DCR.  Any bit positions specified as DONT_CARE will be left
// unchanged.  The macro accounts for the hardware inversion of
// certain signals.

#define UPDATE_DCR(registerValue,b5,b4,b3,b2,b1,b0) \
((UCHAR)(((registerValue) & DCR_AND_MASK(b5,b4,b3,b2,b1,b0)) | DCR_OR_MASK(b5,b4,b3,b2,b1,b0)))

// The DCR_TEST_MASK macro generates a byte constant which may be used
// to mask of DCR bits that we don't care about

#if (1 == DVRH_USE_FAST_MACROS)
    #define DCR_TEST_MASK(b5,b4,b3,b2,b1,b0)  \
    ((UCHAR)((b5==DONT_CARE?0:BIT_5_SET) | \
            (b4==DONT_CARE?0:BIT_4_SET) | \
            (b3==DONT_CARE?0:BIT_3_SET) | \
            (b2==DONT_CARE?0:BIT_2_SET) | \
            (b1==DONT_CARE?0:BIT_1_SET) | \
            (b0==DONT_CARE?0:BIT_0_SET) ) )
#else
    #define DCR_TEST_MASK(b5,b4,b3,b2,b1,b0)  \
    ((UCHAR)( ((b5==DONT_CARE?0:1)<<BIT_5) | \
            ((b4==DONT_CARE?0:1)<<BIT_4) | \
            ((b3==DONT_CARE?0:1)<<BIT_3) | \
            ((b2==DONT_CARE?0:1)<<BIT_2) | \
            ((b1==DONT_CARE?0:1)<<BIT_1) | \
            ((b0==DONT_CARE?0:1)<<BIT_0) ) )
#endif
// The DCR_TEST_VALUE macro generates a byte constant that may be used
// to compare against a masked DCR value.  This macro takes into
// account which signals are inverted by hardware before driving the
// signal line.

#if (1 == DVRH_USE_FAST_MACROS)
    #define DCR_TEST_VALUE(b5,b4,b3,b2,b1,b0)  \
    ((UCHAR)((b5==DONT_CARE?0:(b5==ACTIVE? BIT_5_SET:  0)) | \
            (b4==DONT_CARE?0:(b4==ACTIVE?  BIT_4_SET:  0)) | \
            (b3==DONT_CARE?0:(b3==ACTIVE?  0:          BIT_3_SET)) | \
            (b2==DONT_CARE?0:(b2==ACTIVE?  BIT_2_SET:  0)) | \
            (b1==DONT_CARE?0:(b1==ACTIVE?  0:          BIT_1_SET)) | \
            (b0==DONT_CARE?0:(b0==ACTIVE?  0:          BIT_0_SET)) ) )
#else
    #define DCR_TEST_VALUE(b5,b4,b3,b2,b1,b0)  \
    ((UCHAR)(((b5==DONT_CARE?0:(b5==ACTIVE?1:0))<<BIT_5) | \
            ((b4==DONT_CARE?0:(b4==ACTIVE?1:0))<<BIT_4) | \
            ((b3==DONT_CARE?0:(b3==ACTIVE?0:1))<<BIT_3) | \
            ((b2==DONT_CARE?0:(b2==ACTIVE?1:0))<<BIT_2) | \
            ((b1==DONT_CARE?0:(b1==ACTIVE?0:1))<<BIT_1) | \
            ((b0==DONT_CARE?0:(b0==ACTIVE?0:1))<<BIT_0) ) )
#endif
// The TEST_DCR macro may be used to generate a boolean result that is
// TRUE if the DCR value matches the specified signal levels and FALSE
// otherwise.

#define TEST_DCR(registerValue,b5,b4,b3,b2,b1,b0)  \
(((registerValue) & DCR_TEST_MASK(b5,b4,b3,b2,b1,b0)) == DCR_TEST_VALUE(b5,b4,b3,b2,b1,b0))

BOOLEAN CheckPort(IN PUCHAR offset_Controller,
                  IN UCHAR dsrMask,
                  IN UCHAR dsrValue,
                  IN USHORT msTimeDelay);


#endif
