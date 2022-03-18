/*++

Copyright (c) 1990-2000  Microsoft Corporation

Module Name:

    Testapp.c

Abstract:


Author:


Environment:

    User mode only.

Notes:


Future:



Revision History:


Fixed misc bugs, added proper error handling, etc - Eliyas Yakub 06/17/99



--*/
#define UNICODE 1

#include <windows.h>

#include <ntddndis.h>
#include <ntddpack.h>
#include "packet32.h"

#include "hellowin.h"

#if DBG
    
#define ODS(_x) OutputDebugString(TEXT(_x))

#else

#define ODS(_x)

#endif

ULONG   Filters[6]={0,
                    NDIS_PACKET_TYPE_DIRECTED,
                    NDIS_PACKET_TYPE_MULTICAST,
                    NDIS_PACKET_TYPE_BROADCAST,
                    NDIS_PACKET_TYPE_ALL_MULTICAST,
                    NDIS_PACKET_TYPE_PROMISCUOUS};

HWND    hwndchild;

HINSTANCE  hInst;

CONTROL_BLOCK Adapter;

TCHAR   szChildAppName[]=TEXT("Hexdump");
TCHAR   szTitle[]=TEXT("Packet Driver Test Application");

UINT    showdump=0;

#define NUMBER_OF_PACKETS_TO_SEND 5
#define MAX_ADAPTERS 10
char Buffer[MAX_ADAPTERS * 256];

//
// The structure to get the adapter info
//

typedef struct _ADAPTERS_INFO
{
   ULONG   NumAdapters;
   LPTSTR  AdapterName[MAX_ADAPTERS];
   LPTSTR  SymbolicLink[MAX_ADAPTERS];
} ADAPTERS_INFO, *PADAPTERS_INFO;

ADAPTERS_INFO AdaptersInfo;



LRESULT FAR PASCAL 
WndProc (
    HWND hwnd, 
    UINT message, 
    WPARAM wParam, 
    LPARAM lParam);
    
INT_PTR CALLBACK 
DlgProc(
    HWND hDlg, 
    UINT message, 
    WPARAM wParam, 
    LPARAM lParam);


LRESULT
HandleCommands(
    HWND     hWnd,
    UINT     uMsg,
    WPARAM   wParam,
    LPARAM   lParam
    );


int PASCAL WinMain (HINSTANCE hInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR     lpszCmdParam,
                    int       nCmdShow)
{
    static    TCHAR szAppName[]=TEXT("Packet");
    HWND      hwnd;
    MSG       msg;
    WNDCLASS  wndclass;

    hInst=hInstance;

    if (!hPrevInstance)
       {
         wndclass.style        =  CS_HREDRAW | CS_VREDRAW;
         wndclass.lpfnWndProc  =  WndProc;
         wndclass.cbClsExtra   =  0;
         wndclass.cbWndExtra   =  0;
         wndclass.hInstance    =  hInstance;
         wndclass.hIcon        =  LoadIcon (NULL, IDI_APPLICATION);
         wndclass.hCursor      =  LoadCursor(NULL, IDC_ARROW);
         wndclass.hbrBackground=  GetStockObject(WHITE_BRUSH);
         wndclass.lpszMenuName =  TEXT("GenericMenu");
         wndclass.lpszClassName=  szAppName;

         RegisterClass(&wndclass);
       }


    if (!hPrevInstance)
       {
         wndclass.style        =  CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS |
                                  CS_BYTEALIGNCLIENT;
         wndclass.lpfnWndProc  =  ChildWndProc;
         wndclass.cbClsExtra   =  0;
         wndclass.cbWndExtra   =  0;
         wndclass.hInstance    =  hInstance;
         wndclass.hIcon        =  LoadIcon (hInstance,MAKEINTRESOURCE(1000));
         wndclass.hCursor      =  LoadCursor(NULL, IDC_ARROW);
         wndclass.hbrBackground=  GetStockObject(WHITE_BRUSH);
         wndclass.lpszMenuName =  NULL;
         wndclass.lpszClassName=  szChildAppName;

         RegisterClass(&wndclass);

       }



    hwnd = CreateWindow (szAppName,
                         szTitle,
                         WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT,
                         CW_USEDEFAULT,
                         CW_USEDEFAULT,
                         CW_USEDEFAULT,
                         NULL,
                         NULL,
                         hInstance,
                         NULL);

    ShowWindow (hwnd,nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessage (&msg, NULL, 0,0))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

    return (0);
}


LRESULT FAR PASCAL 
WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HDC       hdc;
    PAINTSTRUCT  ps;
    RECT         rect;

    switch (message)
      {
        case WM_COMMAND:
            HandleCommands(hwnd, message, wParam, lParam);
            return 0;
            
        case WM_CLOSE:
             if (Adapter.OpenInstance != NULL) {
                MessageBox(hwnd, TEXT("Close the device first!"), 
                                    TEXT("Error!"), MB_OK);
                return 0;
             }
             return  DefWindowProc(hwnd,message, wParam, lParam);

        case WM_LBUTTONDOWN:
          if (!showdump) {
               hwndchild = CreateWindow (szChildAppName,
                               TEXT("Hex Dump"),
                               WS_CHILD | WS_CAPTION | WS_SYSMENU | WS_VSCROLL |
                               WS_VISIBLE |  WS_THICKFRAME | WS_CLIPSIBLINGS,
                               0,0,
                               300,100,
                               hwnd,
                               (HMENU) 1,
                               hInst,
                               NULL);
          } else {

              SendMessage(hwndchild,WM_DUMPCHANGE,0,0l);
          }

          showdump=1;
          return 0;



        case WM_CREATE:

          Adapter.BufferSize=1514;

          Adapter.hMem=GlobalAlloc(GMEM_MOVEABLE,1514);

          Adapter.lpMem=GlobalLock(Adapter.hMem);

          Adapter.hMem2=GlobalAlloc(GMEM_MOVEABLE,1514);

          Adapter.lpMem2=GlobalLock(Adapter.hMem2);

          PacketStartDriver(TEXT("PACKET"));

          return 0;


        case WM_KEYDOWN:

          return 0;

    case WM_PAINT:
          hdc=BeginPaint(hwnd,&ps);
          GetClientRect (hwnd,&rect);

      EndPaint(hwnd,&ps);
          return 0;

    case WM_DESTROY:
          PacketStopDriver(TEXT("PACKET"));
          PostQuitMessage(0);
          return 0;
      }
    return DefWindowProc(hwnd,message, wParam, lParam);
  }

BOOLEAN
EnumAdapters(
    HWND   hWnd
)
{
    HANDLE hFile;
    ULONG bytesReturned;
    TCHAR *buffer;
    UINT i;

    memset(Buffer, 0, sizeof(Buffer));
    hFile=CreateFile(TEXT("\\\\.\\Packet"),
                         GENERIC_WRITE | GENERIC_READ,
                         0,
                         NULL,
                         OPEN_EXISTING,
                         FILE_FLAG_OVERLAPPED,
                         0
                         );
    if(hFile == INVALID_HANDLE_VALUE) {
        MessageBox(hWnd, TEXT("Driver is not loaded. Try reloading the app."), 
                                    TEXT("Error!"), MB_OK);
        return FALSE;
    }
    
    if(!DeviceIoControl(hFile,
        IOCTL_ENUM_ADAPTERS,
        NULL,
        0,
        Buffer,
        sizeof(Buffer),
        &bytesReturned,
        NULL
        ))

    {
        MessageBox(hWnd, TEXT("Enum ioctl failed"), TEXT("Error!"), MB_OK);
        return FALSE;
    }

    buffer = (TCHAR *)Buffer;

    //
    // Parse the output and fill the AdaptersInfo structure
    //

    AdaptersInfo.NumAdapters = *(PULONG)buffer;
    (PCHAR)buffer += sizeof(ULONG);

    i = 0;
    AdaptersInfo.AdapterName[i] = (LPTSTR)buffer;
    while (*(buffer++)) {
        while (*(buffer++)) {
            ;
        }
        AdaptersInfo.SymbolicLink[i] = (LPTSTR)buffer;
        while (*(buffer++)) {
            ;
        }
        if(++i == MAX_ADAPTERS) 
            break;
        AdaptersInfo.AdapterName[i] = (LPTSTR)buffer;
    }
   
    CloseHandle(hFile);
    return TRUE;
}


LRESULT
HandleCommands(
    HWND     hWnd,
    UINT     uMsg,
    WPARAM   wParam,
    LPARAM     lParam
    )

{
    static ULONG    Filter=0;
    PVOID  Packet;
    INT    i;
    static INT iSelection = IDM_NO_FILTER;

    switch (wParam) {

        case IDM_OPEN:

            if(!EnumAdapters(hWnd))
            {
                return FALSE;
            }
            if (Adapter.OpenInstance == NULL) {
                i = (INT)DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG), hWnd,
                                                                (DLGPROC)DlgProc);
                if(i == LB_ERR)
                    return TRUE;    
                Adapter.OpenInstance=PacketOpenAdapter(AdaptersInfo.SymbolicLink[i]);

                if (Adapter.OpenInstance != NULL) {
                    SetWindowText(hWnd, AdaptersInfo.AdapterName[i]);                   
                    CheckMenuItem(GetMenu(hWnd), iSelection, MF_CHECKED);

                }
                else
                {
                   MessageBox(hWnd, TEXT("Unable to open"), TEXT("Error!"), MB_OK);
                }
            }
            else
            {
                MessageBox(hWnd, TEXT("Device is already open"), TEXT("Error!"), MB_OK);
            }

            return TRUE;


        case IDM_CLOSE:
            if (Adapter.OpenInstance != NULL) {

                PacketCloseAdapter(Adapter.OpenInstance);

                Adapter.OpenInstance = NULL;
                SetWindowText(hWnd, szTitle);
                CheckMenuItem(GetMenu(hWnd), iSelection, MF_UNCHECKED);
                iSelection = IDM_NO_FILTER;
            }
            else
            {
                MessageBox(hWnd, TEXT("Device not open"), TEXT("Error!"), MB_OK);
            }

            
            return TRUE;


        case  IDM_NO_FILTER:

        case  IDM_DIRECTED:

        case  IDM_MULTICAST:

        case  IDM_BROADCAST:

        case  IDM_ALL_MULTICAST:

        case  IDM_PROMISCUOUS:

          if(Adapter.OpenInstance != NULL)
          {
                HMENU hMenu = GetMenu(hWnd);
                
                Filter=Filters[wParam-IDM_NO_FILTER];
                if(Adapter.OpenInstance != NULL) {

                    if(PacketSetFilter(
                        Adapter.OpenInstance,
                        Filter
                        )) {
                        CheckMenuItem(hMenu, iSelection, MF_UNCHECKED);
                        iSelection = (INT) wParam;
                        CheckMenuItem(hMenu, iSelection, MF_CHECKED);
                    } else {
                        MessageBox(hWnd, TEXT("Set Filter Failed"), TEXT("Error!"), MB_OK);                                 
                    }
                    
                }
          }
          else
          {
                MessageBox(hWnd, TEXT("Device not open"), TEXT("Error!"), MB_OK);
          }

            return TRUE;

        case IDM_RESET:

          if(Adapter.OpenInstance != NULL)
          {

            if (!PacketResetAdapter(Adapter.OpenInstance)){
                MessageBox(hWnd, TEXT("Reset Failed"), TEXT("Error!"), MB_OK);                                 
            }
          }
          else
          {
                MessageBox(hWnd, TEXT("Device not open"), TEXT("Error!"), MB_OK);
          }

          return TRUE;



        case IDM_READ:
          if(Adapter.OpenInstance != NULL)
          {
              if (Filter != 0) {

                  Packet=PacketAllocatePacket(Adapter.OpenInstance);

                  if (Packet != NULL) {

                      PacketInitPacket(
                          Packet,
                          Adapter.lpMem,
                          1514
                          );


                      if(!PacketReceivePacket(
                          Adapter.OpenInstance,
                          Packet,
                          TRUE,
                          &Adapter.PacketLength
                          )) {
                          MessageBox(hWnd, TEXT("Read Failed"), TEXT("Error!"), MB_OK);                                 
                          break;
                      }                          
                      SendMessage(hWnd, WM_LBUTTONDOWN, 0,0l); 
                      PacketFreePacket(Packet);
                  }
              }
              else
              {
                  MessageBox(hWnd, 
                              TEXT("Set the filter to a valid mode"), 
                              TEXT("Error!"), MB_OK);
              }
              
          }
          else
          {
                MessageBox(hWnd, TEXT("Device not open"), TEXT("Error!"), MB_OK);
          }
          return TRUE;


        case IDM_SEND:
          if(Adapter.OpenInstance != NULL)
          {


              Packet=PacketAllocatePacket(Adapter.OpenInstance);

              if (Packet != NULL) {

                  PacketInitPacket(
                      Packet,
                      Adapter.lpMem2,
                      64
                      );



                  Adapter.lpMem2[0]=(UCHAR)0xff;
                  Adapter.lpMem2[1]=(UCHAR)0xff;
                  Adapter.lpMem2[2]=(UCHAR)0xff;
                  Adapter.lpMem2[3]=(UCHAR)0xff;
                  Adapter.lpMem2[4]=(UCHAR)0xff;
                  Adapter.lpMem2[5]=(UCHAR)0xff;

                  for (i=0;i<NUMBER_OF_PACKETS_TO_SEND ;i++) {

                      if(!PacketSendPacket(
                          Adapter.OpenInstance,
                          Packet,
                          TRUE
                          )) {
                          MessageBox(hWnd, TEXT("Send Failed"), TEXT("Error!"), MB_OK);                                 
                          break;
                      }
                      Sleep(10);
                  }

                  PacketFreePacket(Packet);
              }
          }
          else
          {
                MessageBox(hWnd, TEXT("Device not open"), TEXT("Error!"), MB_OK);
          }

          return TRUE;


        default:

            return 0;

    }
    return 0;

}



INT_PTR CALLBACK 
DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND hWndList;
    LRESULT result=0;
    ULONG i;
    
    switch(message)
    {
        case WM_INITDIALOG:
            hWndList = GetDlgItem(hDlg, IDC_LIST);
            for(i=0; i< AdaptersInfo.NumAdapters; i++)
            {
                SendMessage(hWndList, LB_ADDSTRING, 0, 
                                (LPARAM)AdaptersInfo.AdapterName[i]);
            }
            
            return TRUE;

        case WM_COMMAND:    
            switch( wParam)
            {
                case ID_OK:
                    hWndList = GetDlgItem(hDlg, IDC_LIST);
                    result = SendMessage(hWndList, LB_GETCURSEL, 0, 0);   
                    EndDialog(hDlg, result);
                    return TRUE;
            }
            break;
                      
    }
    return FALSE;
}


