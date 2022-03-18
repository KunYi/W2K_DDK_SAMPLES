//===========================================================================
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
// KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
// PURPOSE.
//
// Copyright (c) 1996 - 1998  Microsoft Corporation.  All Rights Reserved.
//
//===========================================================================
/*++

Module Name:

    CapProp.c

Abstract:

    This is a driver for the Sony Desktop Camera (CCM-DS250 V1.0x).
    This file contains code to handle the video and camera control properties.

Author:
    
    Yee J. Wu 9-Sep-97

Environment:

    Kernel mode only

Revision History:


--*/

#include "strmini.h"
#include "ksmedia.h"
#include "1394.h"
#include "wdm.h"       // for DbgBreakPoint() defined in dbg.h
#include "dbg.h"
#include "dcamdef.h"
#include "dcampkt.h"

#include "capprop.h"   // Video and camera property function prototype
#include "PropData.h"  // Define once property data definition - for Sony DCam
#include "PropDta2.h"  // Define once property data definition - for TI DCam


#define ValidValueRange(Range,Value) (Value <= Range.SignedMaximum && Value >= Range.SignedMinimum)
#define DCAM_PROPERTY_NOTSUPPORTED 0xffffffff

//
// Registry subky and values wide character strings.
//
WCHAR wszSettings[]     = L"Settings";
WCHAR wszBrightness[]   = L"Brightness";
WCHAR wszHue[]          = L"Hue";
WCHAR wszSaturation[]   = L"Saturation";
WCHAR wszSharpness[]    = L"Sharpness";
WCHAR wszWhiteBalance[] = L"WhiteBalance";
WCHAR wszZoom[]         = L"Zoom";
WCHAR wszFocus[]        = L"Focus";

NTSTATUS
DCamGetProperty(
    IN PIRB pIrb,
    PDCAM_EXTENSION pDevExt, 
    ULONG ulFieldOffset,
    LONG * plValue,
    ULONG * pulCapability,
    ULONG * pulFlags
    )
{
    NTSTATUS status;
    DCamRegArea RegArea;

    // location of control regisgter - feature = 0x300 (== 768)
    status = DCamReadRegister(pIrb, pDevExt, ulFieldOffset - 768, &(RegArea.AsULONG));
    if (!status) {    

        RegArea.AsULONG = bswap(RegArea.AsULONG);

        if (RegArea.Feature.PresenceInq == 1) {

            *pulCapability = 0;  
            if (RegArea.Feature.AutoMode)
                *pulCapability |= KSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;  // == KSPROPERTY_CAMERACONTROL_FLAGS_AUTO

            if (RegArea.Feature.ManualMode)
                *pulCapability |= KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL;

            status = DCamReadRegister(pIrb, pDevExt, ulFieldOffset, &(RegArea.AsULONG));
            if(!status) {

                RegArea.AsULONG = bswap(RegArea.AsULONG);
                *plValue = (LONG) RegArea.Brightness.Value;

                // These only valid if it has these capabilities.
                if (RegArea.Brightness.AutoMode)
                    *pulFlags = KSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
                else 
                    *pulFlags = KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL;
    
                DbgMsg3(("\'DCamGetProperty: 0x%x: PresenceIng=%d, ONOff=%d, AutoMode=%d, Value=%d\n", 
                    RegArea.AsULONG, 
                    RegArea.Brightness.PresenceInq,   
                    RegArea.Brightness.OnOff,
                    RegArea.Brightness.AutoMode,
                    RegArea.Brightness.Value
                    ));

            }

            DbgMsg3(("\'DCamGetProperty: Currrent value is %d and is %s mode.\n", *plValue, RegArea.Brightness.AutoMode ? "AUTO" : "MANUAL"));

        } else {

            DbgMsg2(("\'DCamGetProperty: PresenceInq not set, feature not supported.\n"));
            return STATUS_NOT_IMPLEMENTED;

        }
    }

    return status;
}



NTSTATUS
DCamSetProperty(
    IN PIRB pIrb,
    PDCAM_EXTENSION pDevExt, 
    ULONG ulFieldOffset,
    ULONG ulFlags,
    LONG  lValue
    )
{
    NTSTATUS status;
    DCamRegArea RegArea;

    status = DCamReadRegister(pIrb, pDevExt, ulFieldOffset, &(RegArea.AsULONG));
    if (!status) {    

        RegArea.AsULONG = bswap(RegArea.AsULONG);
  
        DbgMsg3(("\'DCamSetProperty: 0x%x: PresenceIng=%d, ONOff=%d, AutoMode=%d, Value=%d\n", 
            RegArea.AsULONG, 
            RegArea.Brightness.PresenceInq,   
            RegArea.Brightness.OnOff,
            RegArea.Brightness.AutoMode,
            RegArea.Brightness.Value
        ));


        // Feature supported and in Manual mode
        if(RegArea.Brightness.PresenceInq == 1) {

            if(ulFlags & KSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO) {
                // if not already in auto mode, set it so.
                if(RegArea.Brightness.AutoMode == 0)
                    status = DCamSetAutoMode( pIrb,pDevExt, ulFieldOffset, TRUE);

            } else {

                // if auto mode, set to manual mode
                if(RegArea.Brightness.AutoMode == 1) 
                    status = DCamSetAutoMode( pIrb,pDevExt, ulFieldOffset, FALSE);
                else {

                    // special case for white balance
                    if(FIELDOFFSET(CAMERA_REGISTER_MAP, WhiteBalance) == ulFieldOffset) 
                        RegArea.WhiteBalance.UValue = RegArea.WhiteBalance.VValue = lValue;
                    else 
                        RegArea.Brightness.Value = lValue;
    

                    RegArea.AsULONG = bswap(RegArea.AsULONG);

                    status = DCamWriteRegister(pIrb, pDevExt, ulFieldOffset, RegArea.AsULONG);

                    if(status) { 

                        ERROR_LOG(("\'DCamGetProperty: failed with status=0x%x\n", status));
                    }      
                }
            }
        } else {

            DbgMsg2(("\'DCamSetProperty: not available\n"));

            return STATUS_NOT_IMPLEMENTED;
        }
    }

    return status;

}



NTSTATUS
DCamSetAutoMode(
    IN PIRB pIrb,
    PDCAM_EXTENSION pDevExt, 
    ULONG ulFieldOffset,
    BOOL bAutoMode
    )
{
    NTSTATUS status;
    DCamRegArea RegArea;

    status = DCamReadRegister(pIrb, pDevExt, ulFieldOffset, &(RegArea.AsULONG));
    if (!status) {    

        RegArea.AsULONG = bswap(RegArea.AsULONG);

        DbgMsg3(("\'DCamSetAutoMode: 0x%x: PresenceIng=%d, ONOff=%d, AutoMode=%d, Value=%d\n", 
            RegArea.AsULONG, 
            RegArea.Brightness.PresenceInq,   
            RegArea.Brightness.OnOff,
            RegArea.Brightness.AutoMode,
            RegArea.Brightness.Value
        ));

        // Feature supported and in Manual mode
        if (RegArea.Brightness.PresenceInq == 1) {

            RegArea.Brightness.AutoMode = bAutoMode ? 1 : 0;

            RegArea.AsULONG = bswap(RegArea.AsULONG);

            status = DCamWriteRegister(pIrb, pDevExt, ulFieldOffset, RegArea.AsULONG);

            if (status) { 

                DbgMsg2(("\'DCamSetAutoMode: failed with status=0x%x\n", status));
 
            }
        } else {

            DbgMsg2(("\'DCamSetAutoMode: not available !\n"));

            return STATUS_NOT_IMPLEMENTED;
        }
    }

    return status;

}


NTSTATUS
DCamGetRange(
    IN PIRB pIrb,
    PDCAM_EXTENSION pDevExt,
    ULONG ulFieldOffset,
    LONG * pMinValue,
    LONG * pMaxValue
    )
{
    NTSTATUS status;
    DCamRegArea RegArea;


    status = DCamReadRegister(pIrb, pDevExt, ulFieldOffset, &(RegArea.AsULONG));

    if (!status) {    
  
        RegArea.AsULONG = bswap(RegArea.AsULONG);

        DbgMsg3(("\'DCamGetRange: 0x%x: PresenceIng=%d, ReadOut=%d, ONOff=%d, AutoMode=%d, Manual=%d, min=%d, max=%d\n", 
            RegArea.AsULONG, 
            RegArea.Feature.PresenceInq,   
            RegArea.Feature.ReadOut_Inq,
            RegArea.Feature.OnOff,
            RegArea.Feature.AutoMode,
            RegArea.Feature.ManualMode,
            RegArea.Feature.MIN_Value,
            RegArea.Feature.MAX_Value
            ));

        if (RegArea.Feature.PresenceInq == 1) {

            *pMinValue = RegArea.Feature.MIN_Value;  
            *pMaxValue = RegArea.Feature.MAX_Value; 
 
            DbgMsg3(("\'DCamGetRange: status=0x%x, (min=%d, max=%d)\n", status, *pMinValue, *pMaxValue));

            // Set to Manual Mode
            if (RegArea.Feature.AutoMode == 1) {
                DCamSetAutoMode(pIrb, pDevExt, ulFieldOffset, FALSE);
            }

         } else {     
            DbgMsg2(("\'DCamFeature: PresenceInq not set, feature not supported.\n"));
            *pMinValue = *pMinValue = 0;

            return STATUS_NOT_IMPLEMENTED;
        }
    }

    return status;
 
}


/*
** AdapterGetVideoProcAmpProperty ()
**
**    Handles Set operations on the VideoProcAmp property set.
**      Testcap uses this for demo purposes only.
**
** Arguments:
**
**      pSRB -
**          Pointer to the HW_STREAM_REQUEST_BLOCK 
**
** Returns:
**
** Side Effects:  none
*/

VOID 
AdapterGetVideoProcAmpProperty(
    PHW_STREAM_REQUEST_BLOCK pSrb
    )
{
    NTSTATUS status;

    PDCAM_EXTENSION pDevExt = (PDCAM_EXTENSION) pSrb->HwDeviceExtension;

    PSTREAM_PROPERTY_DESCRIPTOR pSPD = pSrb->CommandData.PropertyInfo;

    PKSPROPERTY_VIDEOPROCAMP_S pS = (PKSPROPERTY_VIDEOPROCAMP_S) pSPD->PropertyInfo;    // pointer to the data

    ASSERT (pSPD->PropertyOutputSize >= sizeof (KSPROPERTY_VIDEOPROCAMP_S));   

    switch (pSPD->Property->Id) {

    case KSPROPERTY_VIDEOPROCAMP_BRIGHTNESS:  
        status = DCamGetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Brightness), &pS->Value, &pS->Capabilities, &pS->Flags);
        break;

    case KSPROPERTY_VIDEOPROCAMP_SHARPNESS:  
        status = DCamGetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Sharpness), &pS->Value, &pS->Capabilities, &pS->Flags);
        break;

    case KSPROPERTY_VIDEOPROCAMP_HUE:  
        status = DCamGetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Hue), &pS->Value, &pS->Capabilities, &pS->Flags); 
        break;
        
    case KSPROPERTY_VIDEOPROCAMP_SATURATION:
        status = DCamGetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Saturation), &pS->Value, &pS->Capabilities, &pS->Flags);
        break;

    case KSPROPERTY_VIDEOPROCAMP_WHITEBALANCE:
        status = DCamGetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, WhiteBalance), &pS->Value, &pS->Capabilities, &pS->Flags);
        break;


    default:
        DbgMsg2(("\'AdapterGetVideoProcAmpProperty, Id (%x)not supported.\n", pSPD->Property->Id));
        DCAM_ASSERT(FALSE);
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    pSrb->Status = status;
    pSrb->ActualBytesTransferred = sizeof (KSPROPERTY_VIDEOPROCAMP_S);

}

/*
** AdapterGetCameraControlProperty ()
**
**    Handles Set operations on the VideoProcAmp property set.
**      Testcap uses this for demo purposes only.
**
** Arguments:
**
**      pSRB -
**          Pointer to the HW_STREAM_REQUEST_BLOCK 
**
** Returns:
**
** Side Effects:  none
*/

VOID 
AdapterGetCameraControlProperty(
    PHW_STREAM_REQUEST_BLOCK pSrb
    )
{
    NTSTATUS status;

    PDCAM_EXTENSION pDevExt = (PDCAM_EXTENSION) pSrb->HwDeviceExtension;

    PSTREAM_PROPERTY_DESCRIPTOR pSPD = pSrb->CommandData.PropertyInfo;

    PKSPROPERTY_CAMERACONTROL_S pS = (PKSPROPERTY_CAMERACONTROL_S) pSPD->PropertyInfo;    // pointer to the data

    ASSERT (pSPD->PropertyOutputSize >= sizeof (KSPROPERTY_CAMERACONTROL_S));

    switch (pSPD->Property->Id) {

    case KSPROPERTY_CAMERACONTROL_FOCUS:

        status = DCamGetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Focus), &pS->Value, &pS->Capabilities, &pS->Flags);
        break;       

    case KSPROPERTY_CAMERACONTROL_ZOOM:

        status = DCamGetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Zoom), &pS->Value, &pS->Capabilities, &pS->Flags);
        break;       

    default:     
        DbgMsg2(("\'AdapterGetCameraControlProperty, Id (%x)not supported.\n", pSPD->Property->Id));
        DCAM_ASSERT(FALSE);
        status = STATUS_NOT_IMPLEMENTED;  
        break;
    }

    pSrb->Status = status;
    pSrb->ActualBytesTransferred = sizeof (KSPROPERTY_CAMERACONTROL_S);

}


/*
** AdapterGetProperty ()
**
**    Handles Get operations for all adapter properties.
**
** Arguments:
**
**      pSRB -
**          Pointer to the HW_STREAM_REQUEST_BLOCK 
**
** Returns:
**
** Side Effects:  none
*/

VOID
STREAMAPI 
AdapterGetProperty(
    PHW_STREAM_REQUEST_BLOCK pSrb
    )

{
    PSTREAM_PROPERTY_DESCRIPTOR pSPD = pSrb->CommandData.PropertyInfo;

    if (IsEqualGUID(&PROPSETID_VIDCAP_VIDEOPROCAMP, &pSPD->Property->Set)) {
        AdapterGetVideoProcAmpProperty (pSrb);
    } else  if (IsEqualGUID(&PROPSETID_VIDCAP_CAMERACONTROL, &pSPD->Property->Set)) {
        AdapterGetCameraControlProperty (pSrb);
    } else {
        //
        // We should never get here
        //

        DCAM_ASSERT(FALSE);
    }
}

/*
** AdapterSetVideoProcAmpProperty ()
**
**    Handles Set operations on the VideoProcAmp property set.
**      Testcap uses this for demo purposes only.
**
** Arguments:
**
**      pSRB -
**          Pointer to the HW_STREAM_REQUEST_BLOCK 
**
** Returns:
**
** Side Effects:  none
*/

VOID 
AdapterSetVideoProcAmpProperty(
    PHW_STREAM_REQUEST_BLOCK pSrb
    )
{
    NTSTATUS status;

    PDCAM_EXTENSION pDevExt = (PDCAM_EXTENSION) pSrb->HwDeviceExtension;

    PSTREAM_PROPERTY_DESCRIPTOR pSPD = pSrb->CommandData.PropertyInfo;

    PKSPROPERTY_VIDEOPROCAMP_S pS = (PKSPROPERTY_VIDEOPROCAMP_S) pSPD->PropertyInfo;    // pointer to the data

    ASSERT (pSPD->PropertyOutputSize >= sizeof (KSPROPERTY_VIDEOPROCAMP_S));    

    switch (pSPD->Property->Id) {

    case KSPROPERTY_VIDEOPROCAMP_BRIGHTNESS:    

        if (ValidValueRange(pDevExt->BrightnessRange,pS->Value)) {
            if(STATUS_SUCCESS == (status = DCamSetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Brightness), pS->Flags, pS->Value)))
                pDevExt->Brightness = pS->Value;
        } else 
            status = STATUS_INVALID_PARAMETER;
        break;
        
    case KSPROPERTY_VIDEOPROCAMP_SHARPNESS:

        if (ValidValueRange(pDevExt->SharpnessRange,pS->Value)) {
            if(STATUS_SUCCESS == (status = DCamSetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Sharpness), pS->Flags, pS->Value)))
                pDevExt->Sharpness = pS->Value;
        } else 
            status = STATUS_INVALID_PARAMETER;
        break;

    case KSPROPERTY_VIDEOPROCAMP_HUE:

        if (ValidValueRange(pDevExt->HueRange,pS->Value)) { 
            if(STATUS_SUCCESS == (status = DCamSetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Hue), pS->Flags, pS->Value)))       
                pDevExt->Hue = pS->Value;
        } else 
            status = STATUS_INVALID_PARAMETER;
        break;

    case KSPROPERTY_VIDEOPROCAMP_SATURATION:

        if (ValidValueRange(pDevExt->SaturationRange,pS->Value)) {
            if(STATUS_SUCCESS == (status = DCamSetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Saturation), pS->Flags, pS->Value)))                
                pDevExt->Saturation = pS->Value;
        } else 
            status = STATUS_INVALID_PARAMETER;
        break;

    case KSPROPERTY_VIDEOPROCAMP_WHITEBALANCE:

        if (ValidValueRange(pDevExt->WhiteBalanceRange,pS->Value)) {
            if(STATUS_SUCCESS == (status = DCamSetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, WhiteBalance), pS->Flags, pS->Value)))       
                pDevExt->WhiteBalance = pS->Value;
        } else 
            status = STATUS_INVALID_PARAMETER;
        break;

    default:
        status = STATUS_NOT_IMPLEMENTED;        
        break;
    }

    pSrb->Status = status;
    pSrb->ActualBytesTransferred = (status == STATUS_SUCCESS ? sizeof (KSPROPERTY_VIDEOPROCAMP_S) : 0);
 

}


/*
** AdapterSetCameraControlProperty ()
**
**    Handles Set operations on the CameraControl property set.
**
** Arguments:
**
**      pSRB -
**          Pointer to the HW_STREAM_REQUEST_BLOCK 
**
** Returns:
**
** Side Effects:  none
*/

VOID 
AdapterSetCameraControlProperty(
    PHW_STREAM_REQUEST_BLOCK pSrb
    )
{
    NTSTATUS status;

    PDCAM_EXTENSION pDevExt = (PDCAM_EXTENSION) pSrb->HwDeviceExtension;

    PSTREAM_PROPERTY_DESCRIPTOR pSPD = pSrb->CommandData.PropertyInfo;

    PKSPROPERTY_CAMERACONTROL_S pS = (PKSPROPERTY_CAMERACONTROL_S) pSPD->PropertyInfo;    // pointer to the data

    ASSERT (pSPD->PropertyOutputSize >= sizeof (KSPROPERTY_CAMERACONTROL_S));    

    switch (pSPD->Property->Id) {

    case KSPROPERTY_CAMERACONTROL_FOCUS:
  
        if (ValidValueRange(pDevExt->FocusRange,pS->Value)) { 
            if(STATUS_SUCCESS == (status = DCamSetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Focus), pS->Flags, pS->Value)))
                pDevExt->Focus = pS->Value;
        } else 
            status = STATUS_INVALID_PARAMETER;
  
        break;

    case KSPROPERTY_CAMERACONTROL_ZOOM:

        if (ValidValueRange(pDevExt->ZoomRange,pS->Value))  {
            if(STATUS_SUCCESS == (status = DCamSetProperty((PIRB) pSrb->SRBExtension, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Zoom), pS->Flags, pS->Value)))
                pDevExt->Zoom = pS->Value;             
        } else 
            status = STATUS_INVALID_PARAMETER;          

        break;
 
    default:
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    pSrb->Status = status;
    pSrb->ActualBytesTransferred = (status == STATUS_SUCCESS ? sizeof (KSPROPERTY_CAMERACONTROL_S) : 0);

}


/*
** AdapterSetProperty ()
**
**    Handles Get operations for all adapter properties.
**
** Arguments:
**
**      pSRB -
**          Pointer to the HW_STREAM_REQUEST_BLOCK 
**
** Returns:
**
** Side Effects:  none
*/

VOID
STREAMAPI 
AdapterSetProperty(
    PHW_STREAM_REQUEST_BLOCK pSrb
    )

{
    PSTREAM_PROPERTY_DESCRIPTOR pSPD = pSrb->CommandData.PropertyInfo;

    if (IsEqualGUID(&PROPSETID_VIDCAP_VIDEOPROCAMP, &pSPD->Property->Set)) {
        AdapterSetVideoProcAmpProperty (pSrb);
    } else  if (IsEqualGUID(&PROPSETID_VIDCAP_CAMERACONTROL, &pSPD->Property->Set)) {
        AdapterSetCameraControlProperty (pSrb);
    } else {
        //
        // We should never get here
        //

        DCAM_ASSERT(FALSE);
    }
}


NTSTATUS 
CreateRegistryKeySingle(
    IN HANDLE hKey,
    IN ACCESS_MASK desiredAccess,
    PWCHAR pwszSection,
    OUT PHANDLE phKeySection
    )
{
    NTSTATUS status;
    UNICODE_STRING ustr;
    OBJECT_ATTRIBUTES objectAttributes;

    RtlInitUnicodeString(&ustr, pwszSection);
	   InitializeObjectAttributes(
		      &objectAttributes,
		      &ustr,
		      OBJ_CASE_INSENSITIVE,
		      hKey,
		      NULL
		      );

    status = 
	       ZwCreateKey(
		          phKeySection,
		          desiredAccess,
		          &objectAttributes,
		          0,
		          NULL,				            /* optional*/
		          REG_OPTION_NON_VOLATILE,
		          NULL
		          );         

    return status;
}



NTSTATUS 
CreateRegistrySubKey(
    IN HANDLE hKey,
    IN ACCESS_MASK desiredAccess,
    PWCHAR pwszSection,
    OUT PHANDLE phKeySection
    )
{
    UNICODE_STRING ustr;
    USHORT usPos = 1;             // Skip first backslash
    static WCHAR wSep = '\\';
    NTSTATUS status = STATUS_SUCCESS;

    RtlInitUnicodeString(&ustr, pwszSection);

    while(usPos < ustr.Length) {
        if(ustr.Buffer[usPos] == wSep) {

            // NULL terminate our partial string
            ustr.Buffer[usPos] = UNICODE_NULL;
            status = 
                CreateRegistryKeySingle(
                    hKey,
                    desiredAccess,
                    ustr.Buffer,
                    phKeySection
                    );
            ustr.Buffer[usPos] = wSep;

            if(NT_SUCCESS(status)) {
                ZwClose(*phKeySection);
            } else {
                break;
            }
        }
        usPos++;
    }

    // Create the full key
    if(NT_SUCCESS(status)) {
        status = 
            CreateRegistryKeySingle(
                 hKey,
                 desiredAccess,
                 ustr.Buffer,
                 phKeySection
                 );
    }

    return status;
}



NTSTATUS 
GetRegistryKeyValue (
    IN HANDLE Handle,
    IN PWCHAR KeyNameString,
    IN ULONG KeyNameStringLength,
    IN PVOID Data,
    IN PULONG DataLength
    )

/*++

Routine Description:
    
    This routine gets the specified value out of the registry

Arguments:

    Handle - Handle to location in registry

    KeyNameString - registry key we're looking for

    KeyNameStringLength - length of registry key we're looking for

    Data - where to return the data

    DataLength - how big the data is

Return Value:

    status is returned from ZwQueryValueKey

--*/

{
    NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;
    UNICODE_STRING keyName;
    ULONG length;
    PKEY_VALUE_FULL_INFORMATION fullInfo;


    RtlInitUnicodeString(&keyName, KeyNameString);
    
    length = sizeof(KEY_VALUE_FULL_INFORMATION) + 
            KeyNameStringLength + *DataLength;
            
    fullInfo = ExAllocatePool(PagedPool, length); 
     
    if (fullInfo) { 
       
        status = ZwQueryValueKey(
                    Handle,
                   &keyName,
                    KeyValueFullInformation,
                    fullInfo,
                    length,
                   &length
                    );
                        
        if (NT_SUCCESS(status)){

            DCAM_ASSERT(fullInfo->DataLength <= *DataLength); 

            RtlCopyMemory(
                Data,
                ((PUCHAR) fullInfo) + fullInfo->DataOffset,
                fullInfo->DataLength
                );

        }            

        *DataLength = fullInfo->DataLength;
        ExFreePool(fullInfo);

    }        
    
    return (status);

}



NTSTATUS
SetRegistryKeyValue(
   HANDLE hKey,
   PWCHAR pwszEntry, 
   LONG nValue
   )
{
    NTSTATUS status;
    UNICODE_STRING ustr;

    RtlInitUnicodeString(&ustr, pwszEntry);

    status =	      
        ZwSetValueKey(
		          hKey,
		          &ustr,
		          0,			/* optional */
		          REG_DWORD,
		          &nValue,
		          sizeof(nValue)
		          );         

   return status;
}


BOOL
GetPropertyValuesFromRegistry(
    PDCAM_EXTENSION pDevExt
    )
{
    NTSTATUS Status;
    HANDLE hPDOKey, hKeySettings;
    ULONG ulLength; 

    DbgMsg2(("\'GetPropertyValuesFromRegistry: pDevExt=%x; pDevExt->BusDeviceObject=%x\n", pDevExt, pDevExt->BusDeviceObject));


    //
    // Registry key: 
    //   Windows 2000:
    //   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\
    //   {6BDD1FC6-810F-11D0-BEC7-08002BE2092F\000x
    //
    // Win98:
    //    HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\Class\Image\000x
    // 
    Status = 
        IoOpenDeviceRegistryKey(
            pDevExt->PhysicalDeviceObject, 
            PLUGPLAY_REGKEY_DRIVER,
            STANDARD_RIGHTS_READ, 
            &hPDOKey);

    // PDO might be deleted when it was removed.    
    if(! pDevExt->bDevRemoved) {
        DCAM_ASSERT(Status == STATUS_SUCCESS);
    }

    //
    // loop through our table of strings,
    // reading the registry for each.
    //
    if(NT_SUCCESS(Status)) {

        // Create or open the settings key
        Status =         
            CreateRegistrySubKey(
                hPDOKey,
                KEY_ALL_ACCESS,
                wszSettings,
                &hKeySettings
                );

        if(NT_SUCCESS(Status)) {

            // Brightness
            ulLength = sizeof(LONG);
            Status = GetRegistryKeyValue(
                hKeySettings, 
                wszBrightness, 
                sizeof(wszBrightness), 
                (PVOID) &pDevExt->Brightness, 
                &ulLength);
            DbgMsg3(("\'GetPropertyValuesFromRegistry: Status %x, ulLength %d, Brightness %d\n", 
                     Status, ulLength, pDevExt->Brightness));

            // Hue
            ulLength = sizeof(LONG);
            Status = GetRegistryKeyValue(
                hKeySettings, 
                wszHue, 
                sizeof(wszHue), 
                (PVOID) &pDevExt->Hue, 
                &ulLength);
            DbgMsg3(("\'GetPropertyValuesFromRegistry: Status %x, ulLength %d, Hue %d\n", 
                     Status, ulLength, pDevExt->Hue));

            // Saturation
            ulLength = sizeof(LONG);
            Status = GetRegistryKeyValue(
                hKeySettings, 
                wszSaturation, 
                sizeof(wszSaturation), 
                (PVOID) &pDevExt->Saturation, 
                &ulLength);
            DbgMsg3(("\'GetPropertyValuesFromRegistry: Status %x, ulLength %d, Saturation %d\n", 
                     Status, ulLength, pDevExt->Saturation));

            // Sharpness
            ulLength = sizeof(LONG);
            Status = GetRegistryKeyValue(
                hKeySettings, 
                wszSharpness, 
                sizeof(wszSharpness), 
                (PVOID) &pDevExt->Sharpness, 
                &ulLength);
            DbgMsg3(("\'GetPropertyValuesFromRegistry: Status %x, ulLength %d, Sharpness %d\n", 
                     Status, ulLength, pDevExt->Sharpness));

            // WhiteBalance
            ulLength = sizeof(LONG);
            Status = GetRegistryKeyValue(
                hKeySettings, 
                wszWhiteBalance, 
                sizeof(wszWhiteBalance), 
                (PVOID) &pDevExt->WhiteBalance, 
                &ulLength);
            DbgMsg3(("\'GetPropertyValuesFromRegistry: Status %x, ulLength %d, WhiteBalance %d\n", 
                     Status, ulLength, pDevExt->WhiteBalance));

            // Zoom
            ulLength = sizeof(LONG);
            Status = GetRegistryKeyValue(
                hKeySettings, 
                wszZoom, 
                sizeof(wszZoom), 
                (PVOID) &pDevExt->Zoom, 
                &ulLength);
            DbgMsg3(("\'GetPropertyValuesFromRegistry: Status %x, ulLength %d, Zoom %d\n", 
                     Status, ulLength, pDevExt->Zoom));

            // Focus
            ulLength = sizeof(LONG);
            Status = GetRegistryKeyValue(
                hKeySettings,
                wszFocus,
                sizeof(wszFocus),
                (PVOID) &pDevExt->Focus,
                &ulLength);
            DbgMsg3(("\'GetPropertyValuesFromRegistry: Status %x, ulLength %d, Focus %d\n", 
                    Status, ulLength, pDevExt->Focus));

            ZwClose(hKeySettings);
            ZwClose(hPDOKey);

            return TRUE;

        } else {

            ERROR_LOG(("\'GetPropertyValuesFromRegistry: CreateRegistrySubKey failed with Status=%x\n", Status));

        }

        ZwClose(hPDOKey);

    } else {

        ERROR_LOG(("\'GetPropertyValuesFromRegistry: IoOpenDeviceRegistryKey failed with Status=%x\n", Status));

    }

    // Not implemented so always return FALSE to use the defaults.
    return FALSE;
}


BOOL
SetPropertyValuesToRegistry(	
    PDCAM_EXTENSION pDevExt
    )
{
    // Set the default to :
    //		HLM\Software\DeviceExtension->pchVendorName\1394DCam

    NTSTATUS Status;
    HANDLE hPDOKey, hKeySettings;

    DbgMsg2(("\'SetPropertyValuesToRegistry: pDevExt=%x; pDevExt->BusDeviceObject=%x\n", pDevExt, pDevExt->BusDeviceObject));


    //
    // Registry key: 
    //   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\
    //   {6BDD1FC6-810F-11D0-BEC7-08002BE2092F\000x
    //
    Status = 
        IoOpenDeviceRegistryKey(
            pDevExt->PhysicalDeviceObject, 
            PLUGPLAY_REGKEY_DRIVER,
            STANDARD_RIGHTS_WRITE, 
            &hPDOKey);

    // PDO might be deleted when it was removed.    
    if(! pDevExt->bDevRemoved) {
        DCAM_ASSERT(Status == STATUS_SUCCESS);
    }

    //
    // loop through our table of strings,
    // reading the registry for each.
    //
    if(NT_SUCCESS(Status)) {

        // Create or open the settings key
        Status =         
            CreateRegistrySubKey(
                hPDOKey,
                KEY_ALL_ACCESS,
                wszSettings,
                &hKeySettings
                );

        if(NT_SUCCESS(Status)) {


            // Brightness
            Status = SetRegistryKeyValue(
                hKeySettings,
                wszBrightness,
                pDevExt->Brightness);
            DbgMsg2(("\'SetPropertyValuesToRegistry: Status %x, Brightness %d\n", Status, pDevExt->Brightness));

            // Hue
            Status = SetRegistryKeyValue(
                hKeySettings,
                wszHue,
                pDevExt->Hue);
            DbgMsg2(("\'SetPropertyValuesToRegistry: Status %x, Hue %d\n", Status, pDevExt->Hue));

            // Saturation
            Status = SetRegistryKeyValue(
                hKeySettings,
                wszSaturation,
                pDevExt->Saturation);
            DbgMsg2(("\'SetPropertyValuesToRegistry: Status %x, Saturation %d\n", Status, pDevExt->Saturation));

            // Sharpness
            Status = SetRegistryKeyValue(
                hKeySettings,
                wszSharpness,
                pDevExt->Sharpness);
            DbgMsg2(("\'SetPropertyValuesToRegistry: Status %x, Sharpness %d\n", Status, pDevExt->Sharpness));

            // WhiteBalance
            Status = SetRegistryKeyValue(
                hKeySettings,
                wszWhiteBalance,
                pDevExt->WhiteBalance);
            DbgMsg2(("\'SetPropertyValuesToRegistry: Status %x, WhiteBalance %d\n", Status, pDevExt->WhiteBalance));

            // Zoom
            Status = SetRegistryKeyValue(
                hKeySettings,
                wszZoom,
                pDevExt->Zoom);
            DbgMsg2(("\'SetPropertyValuesToRegistry: Status %x, Zoom %d\n", Status, pDevExt->Zoom));

            // Focus
            Status = SetRegistryKeyValue(
                hKeySettings,
                wszFocus,
                pDevExt->Focus);
            DbgMsg2(("\'SetPropertyValuesToRegistry: Status %x, Focus %d\n", Status, pDevExt->Focus));

            ZwClose(hKeySettings);
            ZwClose(hPDOKey);

            return TRUE;

        } else {

            ERROR_LOG(("\'GetPropertyValuesToRegistry: CreateRegistrySubKey failed with Status=%x\n", Status));

        }

        ZwClose(hPDOKey);

    } else {

        DbgMsg2(("\'GetPropertyValuesToRegistry: IoOpenDeviceRegistryKey failed with Status=%x\n", Status));

    }

    return FALSE;
}


VOID
SetCurrentDevicePropertyValues(
    PDCAM_EXTENSION pDevExt,
    PIRB pIrb
				)
{
    // Set to the last saved values or the defaults
    if(DCAM_PROPERTY_NOTSUPPORTED != pDevExt->Brightness)
        DCamSetProperty(pIrb, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Brightness),  KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL, pDevExt->Brightness);

    if(DCAM_PROPERTY_NOTSUPPORTED != pDevExt->Hue)
        DCamSetProperty(pIrb, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Hue),         KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL, pDevExt->Hue);

    if(DCAM_PROPERTY_NOTSUPPORTED != pDevExt->Saturation)
        DCamSetProperty(pIrb, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Saturation),  KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL, pDevExt->Saturation);  

    if(DCAM_PROPERTY_NOTSUPPORTED != pDevExt->Sharpness)
        DCamSetProperty(pIrb, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Sharpness),   KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL, pDevExt->Sharpness);

    if(DCAM_PROPERTY_NOTSUPPORTED != pDevExt->WhiteBalance)
        DCamSetProperty(pIrb, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, WhiteBalance),KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL, pDevExt->WhiteBalance);

    if(DCAM_PROPERTY_NOTSUPPORTED != pDevExt->Zoom)
        DCamSetProperty(pIrb, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Zoom),        KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL, pDevExt->Zoom);

    if(DCAM_PROPERTY_NOTSUPPORTED != pDevExt->Focus)
        DCamSetProperty(pIrb, pDevExt, FIELDOFFSET(CAMERA_REGISTER_MAP, Focus),       KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL, pDevExt->Focus);	
}


void
InitializePropertyArray(
    IN PHW_STREAM_REQUEST_BLOCK pSrb
    )
{

    PHW_STREAM_HEADER StreamHeader = &(pSrb->CommandData.StreamBuffer->StreamHeader);        
    PDCAM_EXTENSION pDevExt = (PDCAM_EXTENSION) pSrb->HwDeviceExtension;


#if 0
    //
    // Work item to make a generic 1394 digital camera driver:
    //
    // 1. Dynamically allocate device property table
    // 2. Query device for spported properties 
    // 3. Retrieve its current defaults from its registry
    // 4. Save this table as part of the device extension
    // 5. dynamically construct the stream properties table to be return to the stream class.
    //
#endif

    // Unknown vendor
    if (!pDevExt->pchVendorName)
        return;

    //  Dcam's range
    if(RtlCompareMemory(pDevExt->pchVendorName, "SONY", 4) == 4) {

        // Initialize to default values.
        pDevExt->Brightness   = SONYDCAM_DEF_BRIGHTNESS;
        pDevExt->Hue          = SONYDCAM_DEF_HUE;
        pDevExt->Saturation   = SONYDCAM_DEF_SATURATION;
        pDevExt->Sharpness    = SONYDCAM_DEF_SHARPNESS;
        pDevExt->WhiteBalance = SONYDCAM_DEF_WHITEBALANCE;
        pDevExt->Zoom         = SONYDCAM_DEF_ZOOM;
        pDevExt->Focus        = SONYDCAM_DEF_FOCUS;

        // Get last saved values
        GetPropertyValuesFromRegistry(pDevExt);
  
        //
        // Range are read only values
        //
        pDevExt->BrightnessRange   = BrightnessRangeAndStep[0].Bounds;
        pDevExt->HueRange          = HueRangeAndStep[0].Bounds;
        pDevExt->SaturationRange   = SaturationRangeAndStep[0].Bounds;
        pDevExt->SharpnessRange    = SharpnessRangeAndStep[0].Bounds;
        pDevExt->WhiteBalanceRange = WhiteBalanceRangeAndStep[0].Bounds;
        pDevExt->ZoomRange         = ZoomRangeAndStep[0].Bounds;
        pDevExt->FocusRange        = FocusRangeAndStep[0].Bounds;

        StreamHeader->NumDevPropArrayEntries = NUMBER_OF_ADAPTER_PROPERTY_SETS;
        StreamHeader->DevicePropertiesArray = (PKSPROPERTY_SET) AdapterPropertyTable; 

    } else {

        if(RtlCompareMemory(pDevExt->pchVendorName, "TI", 2) == 2) {

            // Initialize to default values.
            pDevExt->Brightness   = TIDCAM_DEF_BRIGHTNESS;
            pDevExt->Sharpness    = TIDCAM_DEF_SHARPNESS;
            pDevExt->WhiteBalance = TIDCAM_DEF_WHITEBALANCE;
            pDevExt->Focus        = TIDCAM_DEF_FOCUS;

            // Set to last saved values
            GetPropertyValuesFromRegistry(pDevExt);

            pDevExt->BrightnessRange   = BrightnessRangeAndStep2[0].Bounds;
            pDevExt->SharpnessRange    = SharpnessRangeAndStep2[0].Bounds;
            pDevExt->WhiteBalanceRange = WhiteBalanceRangeAndStep2[0].Bounds;
            pDevExt->FocusRange        = FocusRangeAndStep2[0].Bounds;

            StreamHeader->NumDevPropArrayEntries = NUMBER_OF_ADAPTER_PROPERTY_SETS2;
            StreamHeader->DevicePropertiesArray = (PKSPROPERTY_SET) AdapterPropertyTable2; 

        } else {
        // other camera ?
            ERROR_LOG(("\'InitializePropertyArray: Unknown camera from Vendor %s\n", pDevExt->pchVendorName));
            DCAM_ASSERT(FALSE);
            return;
        }
    }

}
