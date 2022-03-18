/*++

Copyright (c) 1999-2000  Microsoft Corporation

Module Name:

    setupdi.c

Abstract:

    Support routines to display devices in a TreeView window control.

Environment:

    user mode only

Notes:

  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
  PURPOSE.

  Copyright (c) 1999-2000 Microsoft Corporation.  All Rights Reserved.


Revision History:

  10/17/99: Created Keith S. Garner

--*/

#include "common.h"

/*++
Routine Description:

    Refresh the Device Nodes in a TreeView
  
Arguments:
    
    ShowHidden - Should this function display Hidden Device Nodes?

    hwndTree - Window Handle to TreeView.

    hDevInfo - Handle to a set of Device Information Nodes.

Return Value:
    
    If TRUE, then we should not display the Class
      
--*/
BOOL EnumAddDevices(BOOL ShowHidden, HWND hwndTree, HDEVINFO hDevInfo)
{
    DWORD i, Status, Problem;
    TV_INSERTSTRUCT tvInsertStruc;
    SP_DEVINFO_DATA DeviceInfoData = {sizeof(SP_DEVINFO_DATA)};

    //
    // Clean off all the items in a TreeView.
    //
    TreeView_DeleteItem(hwndTree,TVI_ROOT);

    //
    // Enumerate though all the devices.
    //
    for (i=0;SetupDiEnumDeviceInfo(hDevInfo,i,&DeviceInfoData);i++)
    {
        //
        // Should we display this device, or move onto the next one.
        //
        if (CR_SUCCESS != CM_Get_DevNode_Status(&Status, &Problem,
                    DeviceInfoData.DevInst,0))
        {
            DisplayError(TEXT("Get_DevNode_Status"));
            continue;
        }

        if (!(ShowHidden || !((Status & DN_NO_SHOW_IN_DM) || 
            IsClassHidden(&DeviceInfoData.ClassGuid))))
            continue;

        //
        // Fill in the structures.
        //
        ZeroMemory(&tvInsertStruc, sizeof(TV_INSERTSTRUCT) );

        tvInsertStruc.hParent = TVI_ROOT;
        tvInsertStruc.hInsertAfter = TVI_LAST;
        tvInsertStruc.item.mask = TVIF_TEXT | TVIF_PARAM;

        //
        // Store the Index for selected Device 
        //
        tvInsertStruc.item.lParam = i; 

        //
        // Get a friendly name for the device.
        //
        ConstructDeviceName( hDevInfo,&DeviceInfoData,
            &tvInsertStruc.item.pszText,
            &tvInsertStruc.item.cchTextMax);

        //
        // Try to get an icon index for this device.
        // 
        if (GetClassImageIndex(&DeviceInfoData.ClassGuid,  
            &tvInsertStruc.item.iImage))
        {
            tvInsertStruc.item.iSelectedImage = tvInsertStruc.item.iImage;
            tvInsertStruc.item.mask |= (TVIF_STATE | TVIF_IMAGE | 
                TVIF_SELECTEDIMAGE);
            tvInsertStruc.item.stateMask = LVIS_OVERLAYMASK;
            tvInsertStruc.item.state = INDEXTOOVERLAYMASK(0);

            if (Problem == CM_PROB_DISABLED) // red (X)
            {
                tvInsertStruc.item.state = INDEXTOOVERLAYMASK(
                    IDI_DISABLED_OVL - IDI_CLASSICON_OVERLAYFIRST + 1);
            }
            else if (Problem) // yellow (!)
            {
                tvInsertStruc.item.state = INDEXTOOVERLAYMASK(
                    IDI_PROBLEM_OVL - IDI_CLASSICON_OVERLAYFIRST + 1);
            }

            if (Status & DN_NO_SHOW_IN_DM) // Greyed out
            {
                tvInsertStruc.item.state |= LVIS_CUT;
                tvInsertStruc.item.stateMask |= LVIS_CUT;
            }
        }

        //
        // Add the Item.
        //
        TreeView_InsertItem(hwndTree, &tvInsertStruc );

        //
        // Free the memory allocated from ConstructDeviceName().
        //
        if (tvInsertStruc.item.pszText) 
            LocalFree(tvInsertStruc.item.pszText);
    }

    // --------------------
    return TRUE;
}

/*++
Routine Description:

    Given a DeviceNode, this function will change it's state.
  
Arguments:
    
    NewState - State to change to.

    SelectedItem - Selected Item to change.

    hDevInfo - Handle to a set of Device Information Nodes.

Return Value:
    
    Returns the status of the function. True/False
      
--*/
BOOL StateChange(DWORD NewState, DWORD SelectedItem,HDEVINFO hDevInfo)
{
    SP_PROPCHANGE_PARAMS PropChangeParams = {sizeof(SP_CLASSINSTALL_HEADER)};
    SP_DEVINFO_DATA DeviceInfoData = {sizeof(SP_DEVINFO_DATA)};
    HCURSOR hCursor;

    //
    // This may take a while :^(
    //
    hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    //
    // Get a handle to the Selected Item.
    //
    if (!SetupDiEnumDeviceInfo(hDevInfo,SelectedItem,&DeviceInfoData))
    {
        DisplayError(TEXT("EnumDeviceInfo"));
        return FALSE;
    }

    //
    // Set the PropChangeParams structure.
    //
    PropChangeParams.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
    PropChangeParams.Scope = DICS_FLAG_GLOBAL;
    PropChangeParams.StateChange = NewState; 

    if (!SetupDiSetClassInstallParams(hDevInfo,
        &DeviceInfoData,
        (SP_CLASSINSTALL_HEADER *)&PropChangeParams,
        sizeof(PropChangeParams)))
    {
        DisplayError(TEXT("SetClassInstallParams"));
        SetCursor(hCursor);
        return FALSE;
    }

    //
    // Call the ClassInstaller and perform the change.
    //
    if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE,
        hDevInfo,
        &DeviceInfoData))
    {
        DisplayError(TEXT("SetClassInstallParams"));
        SetCursor(hCursor);
        return TRUE;
    }

    SetCursor(hCursor);
    return TRUE;
}




