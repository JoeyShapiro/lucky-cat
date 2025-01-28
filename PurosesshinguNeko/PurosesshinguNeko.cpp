// PurosesshinguNeko.cpp : Defines the entry point for the application.
//

/*
プロ (puro) = "pro"
セッ (ses) = "ces"
シン (shin) = "s-in"
グ (gu) = "g"
猫 (neko) = "cat"
*/

#include "framework.h"
#include "PurosesshinguNeko.h"
#include "shellapi.h"
#include "usb.h"
#include "usage.h"

#define MAX_LOADSTRING 100
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAYICON 1

/*
I send over the percentage of usage to the device normalized to its range of 1 byte
it will convert that to whatever it needs
in this case it will be used to determine how far it should move from the origin
*/
#define u8 unsigned char
#define u8_max sizeof(u8) * 255
#define VID 0x2E8A
#define PID 0x000A

// Global Variables:                              // current instance
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
USBDevice device;

NOTIFYICONDATA nid = {};
HWND hWnd;
HMENU hPopMenu;
BOOL running = true;
DWORD interval = 5000;

// Forward declarations of functions included in this code module:
BOOL                RegisterInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
DWORD   WINAPI      Update(LPVOID);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.
    device.Connect(VID, PID);
    CreateThread(NULL, 0, Update, NULL, 0, NULL);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_PUROSESSHINGUNEKO, szWindowClass, MAX_LOADSTRING);

    // Perform application initialization:
    if (!RegisterInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PUROSESSHINGUNEKO));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

BOOL RegisterInstance(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXW);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PUROSESSHINGUNEKO));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_PUROSESSHINGUNEKO);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassEx(&wcex);

    hInst = hInstance;

    hWnd = CreateWindowEx(0, szTitle, szWindowClass, WS_OVERLAPPED,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
       nullptr, nullptr, hInstance, nullptr);

    return hWnd ? TRUE : FALSE;
}

INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        SetDlgItemInt(hDlg, IDC_INTERVAL, interval, FALSE);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            interval = GetDlgItemInt(hDlg, IDC_INTERVAL, NULL, FALSE);
            EndDialog(hDlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // Initialize tray icon
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hwnd;
        nid.uID = ID_TRAYICON;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_PUROSESSHINGUNEKO));
        lstrcpy(nid.szTip, L"Purosesshingu Neko");
        Shell_NotifyIcon(NIM_ADD, &nid);
        return 0;
    }
    case WM_TRAYICON:
    {
        if (lParam == WM_RBUTTONUP)
        {
            POINT pt;
            GetCursorPos(&pt);
            hPopMenu = CreatePopupMenu();
            InsertMenu(hPopMenu, 0, MF_BYPOSITION | MF_STRING, IDM_SETTINGS, L"Settings");
            InsertMenu(hPopMenu, 1, MF_BYPOSITION | MF_STRING, IDM_STATE, running ? L"Pause" : L"Resume");
            InsertMenu(hPopMenu, 2, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"Exit");
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hPopMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN,
                pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hPopMenu);
        }
        return 0;
    }
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_SETTINGS:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, SettingsDialogProc);
                return 0;
            case IDM_STATE:
                running = !running;
                break;
            case IDM_EXIT:
                DestroyWindow(hwnd);
                break;
            default:
                return DefWindowProc(hwnd, message, wParam, lParam);
            }
        }
        break;
    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

DWORD WINAPI Update(LPVOID param) {
    Usage usage = Usage();
    u8 data[2]{};
    u8 buf[1]{};
    DWORD n = 0;

    while (true) {
        Sleep(interval);
        // try to connect if not connected
        if (!device.Connected()) {
            device.Connect(VID, PID);
        }
        // if it failed, then we skip
        if (!device.Connected() || !running) continue;

        if (usage.Measure()) {
            float mem = min(1, max(0, usage.Mem()));
            float cpu = min(1, max(0, usage.CPU()));
            ((u8*)data)[0] = mem * 127;
            ((u8*)data)[1] = cpu * u8_max;
        }

        device.SendData(data, 2);
        device.ReceiveData(buf, 1, &n);
    }

    return 0;
}
