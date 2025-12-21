#define _WIN32_WINNT 0x0A00

// Winsock needs to be included before windows.h
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <windows.h>
#include <commctrl.h>
#include <wchar.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shellscalingapi.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <shcore.h>
#pragma comment(lib, "shcore.lib")
#include "resource.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include <uxtheme.h>

#include <string.h>
#include <ras.h>
#include <rassapi.h>
#include <raserror.h>
#pragma comment(lib, "rasapi32.lib")

#pragma comment(lib, "Comctl32.lib")

// Missing RAS constants for older SDKs
#ifndef RASEO_Pap
#define RASEO_Pap 0x00000001
#endif
#ifndef RASEO_Chap
#define RASEO_Chap 0x00000002
#endif
#ifndef RASEO_MSChap
#define RASEO_MSChap 0x00000004
#endif
#ifndef RASEO_MSChap2
#define RASEO_MSChap2 0x00000008
#endif
#ifndef RASEO_RequireDataEncryption
#define RASEO_RequireDataEncryption 0x00000040
#endif
#ifndef RASEO_RequireEAP
#define RASEO_RequireEAP 0x00000080
#endif
#ifndef RASEO_Custom
#define RASEO_Custom 0x40000000
#endif
#ifndef RASEO2_SecureClientForMSNet
#define RASEO2_SecureClientForMSNet 0x00000001
#endif
#ifndef RASEO2_RequireMachineCertificates
#define RASEO2_RequireMachineCertificates 0x00000004
#endif

// The L2TP PSK is set via RasSetCredentials with the RASCM_PreSharedKey mask.
// The unofficial RasSetKey/RAS_L2TP_KEY definitions are no longer needed.

#ifndef RASNP_Ipv6
#define RASNP_Ipv6 0x00000008
#endif

#ifndef RASEO_ProhibitPAP
#define RASEO_ProhibitPAP 0x00001000
#endif
#ifndef RASEO_ProhibitCHAP
#define RASEO_ProhibitCHAP 0x00002000
#endif
#ifndef RASEO_ProhibitMsCHAP
#define RASEO_ProhibitMsCHAP 0x00004000
#endif
#ifndef RASEO_ProhibitMsCHAP2
#define RASEO_ProhibitMsCHAP2 0x00008000
#endif
// Defines for string lengths, if not already defined (usually in lmcons.h, wtypes.h)
#ifndef UNLEN
#define UNLEN 256
#endif
#ifndef PWLEN
#define PWLEN 256
#endif
#ifndef DNLEN
#define DNLEN 15
#endif

#ifndef RASEO_ProhibitEAP
#define RASEO_ProhibitEAP 0x00010000
#endif

#ifndef SIID_NETWORK
#define SIID_NETWORK 24
#endif

#ifndef SIID_REFRESH
#define SIID_REFRESH 16
#endif

#ifndef SIID_SYNCHRONIZE
#define SIID_SYNCHRONIZE 111
#endif

#ifndef SIID_FOLDERFRONT
#define SIID_FOLDERFRONT 77
#endif

#ifndef SHGSI_SHELLICONSIZE
#define SHGSI_SHELLICONSIZE 0x4
#endif

typedef struct _RASAUTHDATA {
  DWORD dwAuthFlags;
  union {
    struct {
      WCHAR szUserName[UNLEN + 1];
      WCHAR szPassword[PWLEN + 1];
      WCHAR szLogonDomain[DNLEN + 1];
    } s_Ez;
    struct {
      LPWSTR lpszUserName;
      LPWSTR lpszPassword;
      LPWSTR lpszLogonDomain;
    } s_Pointers;
  } anon_union; // Use anon_union for GCC compatibility
} RASAUTHDATA, *PRASAUTHDATA;

// Define function pointer for SetWindowTheme for dynamic loading
typedef HRESULT(WINAPI* PFN_SETWINDOWTHEME)(HWND, LPCWSTR, LPCWSTR);

// RasSetKeyW is no longer used.


// Data structure to pass downloaded content and size
typedef struct {
    char* buffer;
    DWORD size;
} DOWNLOADED_DATA;

// Data structure to pass data to the VPN connection thread
typedef struct {
    HWND hwnd;
    wchar_t serverIp[256];
} VPN_THREAD_DATA;

// Data structure to pass data to the traverse connection thread
typedef struct {
    HWND hwnd;
} TRAVERSE_THREAD_DATA;


#define IDC_LISTBOX 101
#define IDC_BUTTON 102
#define IDC_STATUSBAR 103
#define IDC_BUTTON_TRAVERSE 105
#define ID_LISTBOX_CONNECT 2001
#define IDT_CONNECT_TIMEOUT 2002
#define IDT_CONNECTDEVICE_TIMEOUT 2003

// Custom messages for download thread
#define WM_DOWNLOAD_SUCCESS (WM_APP + 3)
#define WM_DOWNLOAD_FAILURE (WM_APP + 4)
#define WM_VPN_STATUS_UPDATE (WM_APP + 5)
#define WM_TRAVERSE_COMPLETE (WM_APP + 7)
#define WM_APP_UPDATE_FONT (WM_APP + 8)

// Window procedure function
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HINSTANCE g_hInstance;

extern HFONT hGuiFont;
HWND g_hAboutDialog = NULL;

// Callback function to set the font for each child window
BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
    SendMessage(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
    return TRUE;
}

// Helper function to scale values for current DPI
static inline int DpiScale(int dip, UINT dpi)
{
    return MulDiv(dip, dpi, 96);
}

static void RepositionAboutControls(HWND hDlg)
{
    UINT dpi = GetDpiForWindow(hDlg);
    if (dpi == 0) dpi = 96;

    RECT rcClient;
    GetClientRect(hDlg, &rcClient);
    int clientWidth = rcClient.right - rcClient.left;
    int clientHeight = rcClient.bottom - rcClient.top;

    int padding = DpiScale(10, dpi);
    int iconSize = DpiScale(32, dpi);
    int textHeight = DpiScale(20, dpi); // Give a bit more room for text
    int yPos = padding;

    // Icon
    HWND hIcon = GetDlgItem(hDlg, IDC_ABOUT_ICON);
    if (hIcon)
    {
        SetWindowPos(hIcon, NULL, padding, yPos, iconSize, iconSize, SWP_NOZORDER);
    }

    int textX = padding * 2 + iconSize;
    int textWidth = clientWidth - textX - padding;
    if (textWidth < 0) textWidth = 0;

    // Version
    HWND hVersion = GetDlgItem(hDlg, IDC_ABOUT_VERSION);
    if (hVersion)
    {
        SetWindowPos(hVersion, NULL, textX, yPos, textWidth, textHeight, SWP_NOZORDER);
        yPos += textHeight;
    }

    // Description
    HWND hDesc = GetDlgItem(hDlg, IDC_ABOUT_DESC);
    if (hDesc)
    {
        SetWindowPos(hDesc, NULL, textX, yPos, textWidth, textHeight, SWP_NOZORDER);
        yPos += textHeight;
    }

    // Support author text
    HWND hSupport = GetDlgItem(hDlg, IDC_STATIC_SUPPORT_TEXT);
    if (hSupport)
    {
        SetWindowPos(hSupport, NULL, textX, yPos, textWidth, textHeight, SWP_NOZORDER);
    }

    // Center "OK" button and place it at the bottom
    HWND hOkButton = GetDlgItem(hDlg, IDOK);
    if (hOkButton)
    {
        int buttonWidth = DpiScale(80, dpi);
        int buttonHeight = DpiScale(30, dpi);
        int newX = (clientWidth - buttonWidth) / 2;
        int newY = clientHeight - buttonHeight - padding;
        //int panelTop = clientHeight - buttonHeight - padding * 2;
        //int newY = panelTop + padding;
        if (newX < 0) newX = 0;
        if (newY < 0) newY = 0;
        SetWindowPos(hOkButton, NULL, newX, newY, buttonWidth, buttonHeight, SWP_NOZORDER);
    }
}

INT_PTR CALLBACK AboutDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static UINT s_uCurrentDpi = 96;
    static GpBitmap* gpBitmapQR = NULL;

    switch (message)
    {
    case WM_INITDIALOG:
        {
            g_hAboutDialog = hDlg; // Store dialog handle

            // Get current DPI for the dialog
            s_uCurrentDpi = GetDpiForWindow(hDlg);
            if (s_uCurrentDpi == 0) s_uCurrentDpi = 96;
            UINT dpi = s_uCurrentDpi;

            // Define base size at 96 DPI and scale it
            int baseWidth = 300;
            int baseHeight = 400;
            int scaledWidth = DpiScale(baseWidth, dpi);
            int scaledHeight = DpiScale(baseHeight, dpi);

            // Use the global GUI font for the dialog and its controls. This ensures DPI-awareness.
            if (hGuiFont) {
                SendMessage(hDlg, WM_SETFONT, (WPARAM)hGuiFont, TRUE);
                EnumChildWindows(hDlg, EnumChildProc, (LPARAM)hGuiFont);
            }

            // --- Load the QR code bitmap once ---
            HRSRC hRes = FindResource(g_hInstance, MAKEINTRESOURCE(IDB_QRCODE_PNG), RT_RCDATA);
            if (hRes)
            {
                DWORD dwSize = SizeofResource(g_hInstance, hRes);
                HGLOBAL hResLoad = LoadResource(g_hInstance, hRes);
                if (hResLoad)
                {
                    LPVOID pResData = LockResource(hResLoad);
                    if (pResData)
                    {
                        IStream* pStream = NULL;
                        if (CreateStreamOnHGlobal(NULL, TRUE, &pStream) == S_OK)
                        {
                            ULONG written;
                            pStream->lpVtbl->Write(pStream, pResData, dwSize, &written);
                            LARGE_INTEGER seekPos = {0};
                            pStream->lpVtbl->Seek(pStream, seekPos, STREAM_SEEK_SET, NULL);
                            
                            GdipCreateBitmapFromStream(pStream, &gpBitmapQR);
                            
                            pStream->lpVtbl->Release(pStream);
                        }
                    }
                }
            }


            HWND hWndParent = GetParent(hDlg);
            RECT rcParent;
            GetWindowRect(hWndParent, &rcParent);

            // Center the dialog with the new scaled size
            int iX = rcParent.left + (rcParent.right - rcParent.left - scaledWidth) / 2;
            int iY = rcParent.top + (rcParent.bottom - rcParent.top - scaledHeight) / 2;

            SetWindowPos(hDlg, NULL, iX, iY, scaledWidth, scaledHeight, SWP_NOZORDER);
            
            RepositionAboutControls(hDlg);

            InvalidateRect(hDlg, NULL, TRUE);
            UpdateWindow(hDlg);
        }
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            // Clean up the GDI+ bitmap
            if (gpBitmapQR)
            {
                GdipDisposeImage((GpImage*)gpBitmapQR);
                gpBitmapQR = NULL;
            }
            g_hAboutDialog = NULL; // Clear dialog handle
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    case WM_APP_UPDATE_FONT:
        if (hGuiFont) {
            SendMessage(hDlg, WM_SETFONT, (WPARAM)hGuiFont, TRUE);
            EnumChildWindows(hDlg, EnumChildProc, (LPARAM)hGuiFont);
        }
        break;
    case WM_SIZE:
        RepositionAboutControls(hDlg);
        InvalidateRect(hDlg, NULL, TRUE);
        break;
    case WM_DPICHANGED:
        {
            s_uCurrentDpi = HIWORD(wParam);

            if (hGuiFont) {
                SendMessage(hDlg, WM_SETFONT, (WPARAM)hGuiFont, TRUE);
                EnumChildWindows(hDlg, EnumChildProc, (LPARAM)hGuiFont);
            }

            RepositionAboutControls(hDlg);

            RECT* const prcNewWindow = (RECT*)lParam;
            SetWindowPos(hDlg,
                NULL,
                prcNewWindow->left,
                prcNewWindow->top,
                prcNewWindow->right - prcNewWindow->left,
                prcNewWindow->bottom - prcNewWindow->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            
            RedrawWindow(hDlg, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
        }
        break;
    case WM_CTLCOLORDLG:
        return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
    case WM_CTLCOLORSTATIC:
        {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT);
            return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
        }
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hDlg, &ps);

            // Get client rect and DPI for manual painting
            RECT rcClient;
            GetClientRect(hDlg, &rcClient);
            UINT dpi = s_uCurrentDpi;
            if (dpi == 0) dpi = 96;

            // --- Paint the bottom panel for the button ---
            int padding = DpiScale(12, dpi);
            int buttonHeight = DpiScale(30, dpi);
            int clientHeight = rcClient.bottom - rcClient.top;
            
            // Calculate the top Y coordinate of the bottom panel area
            int panelTop = clientHeight - buttonHeight - (padding * 2);
            if (panelTop < 0) panelTop = 0;

            // Define the rectangle for the bottom area
            RECT rcBottom = {0, panelTop, rcClient.right, clientHeight};

            // Fill the bottom area with the standard dialog face color
            FillRect(hdc, &rcBottom, GetSysColorBrush(COLOR_3DFACE));

            // --- Paint the pre-loaded QR code ---
            if (gpBitmapQR)
            {
                GpGraphics* graphics = NULL;
                if (GdipCreateFromHDC(hdc, &graphics) == Ok && graphics)
                {
                    int width = DpiScale(200, dpi);
                    int height = DpiScale(200, dpi);
                    int x = (rcClient.right - width) / 2;
                    
                    // Calculate Y position to be below the text labels
                    int textBlockHeight = DpiScale(20, dpi) * 3; // 3 lines of text
                    int topPadding = DpiScale(10, dpi);
                    int y = topPadding + textBlockHeight + DpiScale(5, dpi);

                    if(x < 0) x = 0;

                    GdipDrawImageRectI(graphics, (GpImage*)gpBitmapQR, x, y, width, height);
                    GdipDeleteGraphics(graphics);
                }
            }
            EndPaint(hDlg, &ps);
        }
        break;
    }
    return (INT_PTR)FALSE;
}


// Global variables
HWND hStatusBar;
HRASCONN g_hRasConn = NULL; // Global handle for the active RAS connection
HANDLE hMutex = NULL;
HWND g_hMenuOwnerWnd = NULL;

RECT g_rcOriginalWindowPos; // Store original window position and size
BOOL g_wasMaximized = FALSE; // Store if the window was maximized

// Global flag for traverse status
BOOL g_traverseInProgress = FALSE;

// Global event and result for traverse connection synchronization
HANDLE g_hTraverseConnectEvent = NULL;  // Event to signal connection result
BOOL g_traverseConnectSuccess = FALSE;  // Connection success/failure flag



typedef struct {
    HWND hwnd;
} THREAD_DATA;

// 
// Tray Icon definitions and functions
//
#define WM_APP_TRAYMSG (WM_APP + 1)
#define WM_APP_SHOW (WM_APP + 2)
#define ID_TRAY_RESTORE 1001
#define ID_TRAY_EXIT 1002

NOTIFYICONDATAW g_nid;

// Global handles for tray icons
HICON g_hIconDefault = NULL;
HICON g_hIconConnected = NULL;

void UpdateTrayIcon(BOOL isConnected)
{
    if (isConnected) {
        g_nid.hIcon = g_hIconConnected;
    } else {
        g_nid.hIcon = g_hIconDefault;
    }
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}


// Thread function to download the server list
DWORD WINAPI DownloadThreadProc(LPVOID lpParameter)
{
    THREAD_DATA* pData = (THREAD_DATA*)lpParameter;
    HWND hwnd = pData->hwnd;
    free(pData);

    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    LPSTR pszOutBuffer = NULL;
    BOOL  bResults = FALSE;
    HINTERNET hSession = NULL,
              hConnect = NULL,
              hRequest = NULL;

    // Use WinHttpOpen to obtain a session handle.
    hSession = WinHttpOpen(L"LanWan Client/1.0",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS, 0);

    // Specify an HTTP server.
    if (hSession)
        hConnect = WinHttpConnect(hSession, L"liangshengyong.github.io",
                                INTERNET_DEFAULT_HTTPS_PORT, 0);

    // Create an HTTP request handle.
    if (hConnect)
        hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/LanWan/data/servers.txt",
                                      NULL, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES,
                                      WINHTTP_FLAG_SECURE);

    // Send a request.
    if (hRequest)
    {
        WinHttpSetTimeouts(hRequest, 5000, 5000, 5000, 5000);
        bResults = WinHttpSendRequest(hRequest,
                                      WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                      WINHTTP_NO_REQUEST_DATA, 0,
                                      0, 0);
    }

    // End the request.
    if (bResults)
        bResults = WinHttpReceiveResponse(hRequest, NULL);

    // Keep checking for data until there is nothing left.
    if (bResults)
    {
        // Check for HTTP status code.
        DWORD dwStatusCode = 0;
        DWORD dwStatusCodeSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            NULL, &dwStatusCode, &dwStatusCodeSize, NULL);
        
        if (dwStatusCode == 200)
        {
            LPSTR tempBuffer = NULL;
            DWORD totalSize = 0;

            do
            {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
                {
                    free(tempBuffer);
                    tempBuffer = NULL;
                    break;
                }

                if (dwSize > 0)
                {
                    LPSTR newBuffer = (LPSTR)realloc(tempBuffer, totalSize + dwSize + 1);
                    if (newBuffer)
                    {
                        tempBuffer = newBuffer;
                        if (WinHttpReadData(hRequest, (LPVOID)(tempBuffer + totalSize), dwSize, &dwDownloaded))
                        {
                            totalSize += dwDownloaded;
                        } else {
                            // If reading data fails, treat it as a download failure.
                            free(tempBuffer);
                            tempBuffer = NULL;
                            break;
                        }
                    } else {
                        free(tempBuffer);
                        tempBuffer = NULL;
                        break;
                    }
                }
            } while (dwSize > 0);

            if (tempBuffer) {
                tempBuffer[totalSize] = '\0'; // Null-terminate the buffer
                DOWNLOADED_DATA* pData = (DOWNLOADED_DATA*)malloc(sizeof(DOWNLOADED_DATA));
                if (pData) {
                    pData->buffer = tempBuffer;
                    pData->size = totalSize;
                    PostMessage(hwnd, WM_DOWNLOAD_SUCCESS, 0, (LPARAM)pData);
                } else {
                    free(tempBuffer);
                    PostMessage(hwnd, WM_DOWNLOAD_FAILURE, 0, 0);
                }
            } else {
                PostMessage(hwnd, WM_DOWNLOAD_FAILURE, 0, 0);
            }
        } else {
            PostMessage(hwnd, WM_DOWNLOAD_FAILURE, 0, 0);
        }
    } else {
        PostMessage(hwnd, WM_DOWNLOAD_FAILURE, 0, 0);
    }


    // Close any open handles.
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    return 0;
}


// Forward declaration for ShowTaskDialog
HRESULT ShowTaskDialog(HWND hwnd, const WCHAR* title, const WCHAR* mainInstruction, const WCHAR* content, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, PCWSTR pszIcon, int* pnButton);

void AddTrayIcon(HWND hwnd)
{
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_TRAYMSG;
    g_nid.hIcon = g_hIconDefault;
    wcscpy_s(g_nid.szTip, sizeof(g_nid.szTip)/sizeof(wchar_t), L"蓝湾");

    if (!Shell_NotifyIconW(NIM_ADD, &g_nid)) {
        ShowTaskDialog(hwnd, L"错误", L"未能添加托盘图标！", NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON, NULL);
    } else {
        g_nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
    }
}

void RemoveTrayIcon(HWND hwnd)
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

HRESULT ShowTaskDialog(HWND hwnd, const WCHAR* title, const WCHAR* mainInstruction, const WCHAR* content, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, PCWSTR pszIcon, int* pnButton)
{
    TASKDIALOGCONFIG tdc = { sizeof(tdc) };
    tdc.hwndParent = hwnd;
    tdc.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW;
    tdc.pszWindowTitle = title;
    tdc.pszMainInstruction = mainInstruction;
    tdc.pszContent = content;
    tdc.pszMainIcon = pszIcon;
    tdc.dwCommonButtons = buttons;
    return TaskDialogIndirect(&tdc, pnButton, NULL, NULL);
}

void ShowTrayContextMenu(HWND hwnd)
{
    // Use the helper window as the menu owner to solve focus/Z-order issues.
    if (!g_hMenuOwnerWnd) {
        return; // Safety check
    }

    HMENU hPopupMenu = CreatePopupMenu();
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_RESTORE, L"显示主窗口");
    InsertMenuW(hPopupMenu, 1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);

    // --- Last Resort: AttachThreadInput Hack ---
    // This is an aggressive technique to steal foreground rights when all else fails.
    DWORD foregroundThreadId = 0;
    DWORD currentThreadId = GetCurrentThreadId();
    HWND foregroundHwnd = GetForegroundWindow();
    if (foregroundHwnd) {
        foregroundThreadId = GetWindowThreadProcessId(foregroundHwnd, NULL);
    }

    if (foregroundThreadId != 0 && foregroundThreadId != currentThreadId) {
        AttachThreadInput(foregroundThreadId, currentThreadId, TRUE);
    }
    // ---

    // Show helper window and set it as foreground.
    ShowWindow(g_hMenuOwnerWnd, SW_SHOWNOACTIVATE);
    SetForegroundWindow(g_hMenuOwnerWnd);
    
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;

    // Adjust menu alignment based on taskbar position.
    APPBARDATA abd = { sizeof(abd) };
    if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
        switch (abd.uEdge) {
            case ABE_BOTTOM:
                flags |= TPM_BOTTOMALIGN | TPM_LEFTALIGN; break;
            case ABE_RIGHT:
                flags |= TPM_TOPALIGN | TPM_RIGHTALIGN; break;
            default:
                flags |= TPM_TOPALIGN | TPM_LEFTALIGN; break;
        }
    } else {
        flags |= TPM_TOPALIGN | TPM_LEFTALIGN;
    }

    // Use the helper window as the owner for TrackPopupMenu.
    int command = TrackPopupMenu(hPopupMenu, flags, pt.x, pt.y, 0, g_hMenuOwnerWnd, NULL);
    
    // Hide the helper window again.
    ShowWindow(g_hMenuOwnerWnd, SW_HIDE);

    // --- Detach from the foreground thread's input ---
    if (foregroundThreadId != 0 && foregroundThreadId != currentThreadId) {
        AttachThreadInput(foregroundThreadId, currentThreadId, FALSE);
    }
    // ---

    DestroyMenu(hPopupMenu);

    if (command > 0)
    {
        // Post the command to the main application window to be handled.
        PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
    }
}

BOOL VpnEntryExists()
{
    RASENTRYW entry = {0};
    entry.dwSize = sizeof(RASENTRYW);
    DWORD dwEntrySize = sizeof(entry);
    DWORD rasResult = RasGetEntryPropertiesW(NULL, L"蓝湾网络", &entry, &dwEntrySize, NULL, NULL);
    return (rasResult == SUCCESS);
}

BOOL CreateVpnEntry()
{
    DWORD rasResult;
    RASENTRYW entry = {0};
    entry.dwSize = sizeof(RASENTRYW);
    DWORD dwEntrySize = sizeof(entry);

    // --- Create Entry ---
    RasGetEntryPropertiesW(NULL, NULL, &entry, &dwEntrySize, NULL, NULL);
    
    entry.dwfNetProtocols = RASNP_Ip | RASNP_Ipv6;
    entry.dwfOptions |= RASEO_RemoteDefaultGateway;
    entry.dwfOptions2 |= RASEO2_UsePreSharedKey;
    entry.dwfOptions |= RASEO_MSChap2 | RASEO_Chap;
    entry.dwfOptions &= ~(RASEO_Pap | RASEO_MSChap | RASEO_RequireEAP);
    entry.dwfOptions |= RASEO_RequireEncryptedPw;
    entry.dwfOptions |= RASEO_RequireMsEncryptedPw;
    entry.dwfOptions2 &= ~RASEO2_SecureClientForMSNet;
    entry.dwfOptions2 &= ~RASEO2_RequireMachineCertificates;
    entry.dwfOptions &= ~RASEO_Custom;
    entry.dwType = RASET_Vpn;
    entry.dwVpnStrategy = VS_L2tpOnly;
    wcscpy_s(entry.szDeviceType, RAS_MaxDeviceType + 1, L"vpn");
    wcscpy_s(entry.szDeviceName, RAS_MaxDeviceName + 1, L"L2TP");
    wcscpy_s(entry.szLocalPhoneNumber, RAS_MaxPhoneNumber + 1, L"");

    rasResult = RasSetEntryPropertiesW(NULL, L"蓝湾网络", &entry, entry.dwSize, NULL, 0);
    if (rasResult != SUCCESS) {
        return FALSE;
    }

    // --- Set Pre-Shared Key ---
    RASCREDENTIALSW pskCreds = {0};
    pskCreds.dwSize = sizeof(RASCREDENTIALSW);
    pskCreds.dwMask = RASCM_PreSharedKey;
    wcscpy_s(pskCreds.szPassword, PWLEN + 1, L"vpn");

    rasResult = RasSetCredentialsW(NULL, L"蓝湾网络", &pskCreds, FALSE);
    if (rasResult != SUCCESS) {
        return FALSE;
    }

    // --- Set User Credentials ---
    RASCREDENTIALSW userCreds = {0};
    userCreds.dwSize = sizeof(RASCREDENTIALSW);
    userCreds.dwMask = RASCM_UserName | RASCM_Password;
    wcscpy_s(userCreds.szUserName, UNLEN + 1, L"vpn");
    wcscpy_s(userCreds.szPassword, PWLEN + 1, L"vpn");

    rasResult = RasSetCredentialsW(NULL, L"蓝湾网络", &userCreds, FALSE);
    if (rasResult != SUCCESS) {
        return FALSE;
    }
    
    return TRUE;
}


GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;
    const wchar_t CLASS_NAME[]  = L"Sample Window Class";
    const wchar_t HELPER_CLASS_NAME[] = L"LanWanMenuHelper";
    const wchar_t MUTEX_NAME[] = L"Global\\LanWanApp_{E1F495A0-69A7-4A8A-9963-4C78A3A585A1}";
    const wchar_t CREATE_VPN_ARG[] = L"--create-vpn";

    // If called with --create-vpn, create the entry and exit.
    if (wcscmp(lpCmdLine, CREATE_VPN_ARG) == 0)
    {
        if (CreateVpnEntry()) {
            return 0; // Success
        } else {
            return 1; // Failure
        }
    }

    // Create DACL to allow all users to access the mutex
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;
    if (InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION))
    {
        if (SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE))
        {
            sa.nLength = sizeof(sa);
            sa.lpSecurityDescriptor = &sd;
            sa.bInheritHandle = FALSE;
            
            hMutex = CreateMutexW(&sa, TRUE, MUTEX_NAME);
        }
        else
        {
            hMutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
        }
    }
    else
    {
        hMutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    }
    if (hMutex == NULL)
    {
        MessageBoxW(NULL, L"Failed to create mutex.", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        HWND hWnd = FindWindowW(CLASS_NAME, NULL);
        if (hWnd)
        {
            if (!IsWindowVisible(hWnd))
            {
                PostMessageW(hWnd, WM_APP_SHOW, 0, 0);
            }
            else
            {
                if (IsIconic(hWnd))
                {
                    ShowWindow(hWnd, SW_RESTORE);
                }
                SetForegroundWindow(hWnd);
            }
        }
        CloseHandle(hMutex);
        return 0;
    }

    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    // Initialize GDI+ 
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    gdiplusStartupInput.GdiplusVersion = 1;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Register the window class.
    WNDCLASSW wc = { };

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.lpszMenuName  = MAKEINTRESOURCEW(IDR_MENU);
    HICON hWinIcon = NULL;
    LoadIconWithScaleDown(hInstance, MAKEINTRESOURCEW(MAINICON_ID), GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), &hWinIcon);
    wc.hIcon         = hWinIcon;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);

    RegisterClassW(&wc);

    // Register a helper window class for the tray menu owner
    WNDCLASSW wc_helper = { 0 };
    wc_helper.lpfnWndProc   = DefWindowProcW;
    wc_helper.hInstance     = hInstance;
    wc_helper.lpszClassName = HELPER_CLASS_NAME;
    RegisterClassW(&wc_helper);

    // Load icons for the tray
    int smIconWidth = GetSystemMetrics(SM_CXSMICON);
    int smIconHeight = GetSystemMetrics(SM_CYSMICON);
    LoadIconWithScaleDown(hInstance, MAKEINTRESOURCEW(MAINICON_ID), smIconWidth, smIconHeight, &g_hIconConnected);
    LoadIconWithScaleDown(hInstance, MAKEINTRESOURCEW(TRAYICON_ID), smIconWidth, smIconHeight, &g_hIconDefault);


    // Get the work area dimensions (excluding taskbar).
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int work_width = workArea.right - workArea.left;
    int work_height = workArea.bottom - workArea.top;

    // Calculate window dimensions using the golden ratio against the work area.
    int window_width = (int)(work_width * 0.618);
    int window_height = (int)(work_height * 0.618);

    // Center the window in the work area.
    int x = workArea.left + (work_width - window_width) / 2;
    int y = workArea.top + (work_height - window_height) / 2;

    // Create the window.
    HWND hwnd = CreateWindowExW(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"蓝湾",                         // Window text
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
        x, y, window_width, window_height,

        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL)
    {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 0;
    }

    // Create a hidden helper window to own the context menu
    g_hMenuOwnerWnd = CreateWindowExW(
        0, HELPER_CLASS_NAME, NULL, WS_POPUP,
        0, 0, 1, 1,
        hwnd, // Owner
        NULL, hInstance, NULL);

    // Store the original window position and size
    GetWindowRect(hwnd, &g_rcOriginalWindowPos);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Simulate a click on the refresh button after the window is shown
    PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDC_BUTTON, 0), 0);

    // Run the message loop.
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Shutdown GDI+ 
    GdiplusShutdown(gdiplusToken);
    CoUninitialize();

    // Release the mutex.
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);

    return (int)msg.wParam;
}



// Window procedure
HFONT hGuiFont = NULL;

void RestoreWindow(HWND hwnd)
{
    if (g_wasMaximized)
    {
        ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    }
    else
    {
        SetWindowPos(hwnd, NULL, g_rcOriginalWindowPos.left, g_rcOriginalWindowPos.top, g_rcOriginalWindowPos.right - g_rcOriginalWindowPos.left, g_rcOriginalWindowPos.bottom - g_rcOriginalWindowPos.top, SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(hwnd, SW_RESTORE);
    }
    SetForegroundWindow(hwnd);
}

void UpdateFont(HWND hwnd)
{
    if (hGuiFont)
    {
        DeleteObject(hGuiFont);
    }

    NONCLIENTMETRICSW ncm = {0};
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);

    // Get the system DPI to correctly interpret the font metrics.
    HDC hdc = GetDC(NULL);
    int systemDpi = 96; // Default value
    if (hdc) {
        systemDpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(NULL, hdc);
    }

    // Get the font size in points from the system settings.
    // The lfHeight from SystemParametersInfoW is for the system's DPI.
    LONG pointSize = -MulDiv(ncm.lfMessageFont.lfHeight, 72, systemDpi);

    // Get the current DPI for the window.
    UINT windowDpi = GetDpiForWindow(hwnd);
    if (windowDpi == 0) {
        windowDpi = systemDpi; // Fallback to system DPI
    }

    // Scale the font height for the current window's DPI.
    ncm.lfMessageFont.lfHeight = -MulDiv(pointSize, windowDpi, 72);

    hGuiFont = CreateFontIndirectW(&ncm.lfMessageFont);

    HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
    if (hListBox) SendMessageW(hListBox, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

    HWND hButton = GetDlgItem(hwnd, IDC_BUTTON);
    if (hButton) SendMessageW(hButton, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

    HWND hTraverseButton = GetDlgItem(hwnd, IDC_BUTTON_TRAVERSE);
    if (hTraverseButton) SendMessageW(hTraverseButton, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

    // Test button removed; no font to set

    HWND hStatusBar = GetDlgItem(hwnd, IDC_STATUSBAR);
    if (hStatusBar) SendMessageW(hStatusBar, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

    if (g_hAboutDialog)
    {
        PostMessageW(g_hAboutDialog, WM_APP_UPDATE_FONT, 0, 0);
    }
}

// Helper function to run a command synchronously and check its exit code.
BOOL RunCommand(wchar_t* command) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Hide the command window

    // Create the process.
    if (!CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return FALSE; // Failed to create process
    }

    // Wait for the process to complete.
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    BOOL result = GetExitCodeProcess(pi.hProcess, &exitCode);

    // Clean up handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Return TRUE if the process ran successfully and exited with code 0.
    return result && (exitCode == 0);
}


// RAS Dial callback function to update the status bar
VOID WINAPI RasDialCallback(UINT unMsg, RASCONNSTATE rascs, DWORD dwError, HRASCONN hrasconn)
{
    // Find the main window and its status bar
    HWND hMainWindow = FindWindowW(L"Sample Window Class", NULL);
    if (!hMainWindow) return;
    HWND hStatusBar = GetDlgItem(hMainWindow, IDC_STATUSBAR);
    if (!hStatusBar) return;

    // If the new state is not RASCS_ConnectDevice, kill the specific timer for it.
    if (rascs != RASCS_ConnectDevice) {
        KillTimer(hMainWindow, IDT_CONNECTDEVICE_TIMEOUT);
    }

    // If we've reached a final state (connected or disconnected), or an error occurred, kill the timeout timer.
    if (rascs == RASCS_Connected || rascs == RASCS_Disconnected || dwError != 0) {
        KillTimer(hMainWindow, IDT_CONNECT_TIMEOUT);
    }

    if (dwError)
    {
        wchar_t errorMsg[256];
        wchar_t statusMsg[300];
        if (RasGetErrorStringW(dwError, errorMsg, 256) == 0)
        {
            swprintf_s(statusMsg, 300, L"拨号失败: %s (%lu)", errorMsg, dwError);
        }
        else
        {
            swprintf_s(statusMsg, 100, L"拨号失败: 未知错误 %lu", dwError);
        }
        SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)statusMsg);
        UpdateTrayIcon(FALSE);
        // If traverse is in progress, signal failure
        if (g_traverseInProgress && g_hTraverseConnectEvent) {
            g_traverseConnectSuccess = FALSE;
            SetEvent(g_hTraverseConnectEvent);
        }
        g_hRasConn = NULL; // Clear global handle on failure
        return;
    }

    switch (rascs)
    {
        case RASCS_OpenPort: SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"正在打开端口..."); break;
        case RASCS_PortOpened: SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"端口已打开"); break;
        case RASCS_ConnectDevice: 
            SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"正在连接服务器...");
            SetTimer(hMainWindow, IDT_CONNECTDEVICE_TIMEOUT, 3000, NULL);
            break;
        case RASCS_DeviceConnected: SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"服务器已连接"); break;
        case RASCS_AllDevicesConnected: SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"所有服务器已连接"); break;
        case RASCS_Authenticate: SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"正在验证身份..."); break;
        case RASCS_AuthNotify: SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"等待验证通知..."); break;
        case RASCS_Authenticated: SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"身份验证成功"); break;
        case RASCS_Connected: 
            SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"已连接到 '蓝湾网络'");
            UpdateTrayIcon(TRUE);
            // If traverse is in progress, signal success
            if (g_traverseInProgress && g_hTraverseConnectEvent) {
                g_traverseConnectSuccess = TRUE;
                SetEvent(g_hTraverseConnectEvent);
            }
            g_hRasConn = NULL; // Clear global handle on success
            break;
        case RASCS_Disconnected: 
            SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"连接已断开");
            UpdateTrayIcon(FALSE);
            // If traverse is in progress and waiting, signal failure if not yet succeeded
            if (g_traverseInProgress && g_hTraverseConnectEvent && !g_traverseConnectSuccess) {
                SetEvent(g_hTraverseConnectEvent);
            }
            g_hRasConn = NULL; // Clear global handle on disconnect
            break;
        default: {
            wchar_t statusMsg[100];
            swprintf_s(statusMsg, 100, L"状态: %d", rascs);
            SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)statusMsg);
            break;
        }
    }
}

// Define the callback function pointer type, as the one in headers is incorrect for RasDialW
typedef VOID (WINAPI *RasDialFunc)(UINT, RASCONNSTATE, DWORD, HRASCONN);

// Worker thread to handle VPN connection logic asynchronously
DWORD WINAPI ConnectVpnThreadProc(LPVOID lpParameter)
{
    VPN_THREAD_DATA* pData = (VPN_THREAD_DATA*)lpParameter;
    HWND hwnd = pData->hwnd;
    wchar_t serverIp[256];
    wcscpy_s(serverIp, 256, pData->serverIp);
    free(pData);

    wchar_t* statusMsg;
    DWORD rasResult;

    // --- Check for and hang up existing connection ---
    RASCONNW conn[10];
    DWORD connSize = sizeof(conn);
    DWORD numConn = 0;
    conn[0].dwSize = sizeof(RASCONNW);

    if (RasEnumConnectionsW(conn, &connSize, &numConn) == SUCCESS && numConn > 0) {
        for (DWORD i = 0; i < numConn; i++) {
            if (wcscmp(conn[i].szEntryName, L"蓝湾网络") == 0) {
                statusMsg = _wcsdup(L"检测到现有连接，正在断开...");
                if(statusMsg) PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);

                HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
                if (hEvent == NULL) { // Event creation failed, just try to hang up without waiting.
                    RasHangUpW(conn[i].hrasconn);
                    break; // Exit loop
                }

                // Step 1: Register for the disconnection event (must be before HangUp)
                DWORD ret = RasConnectionNotificationW(conn[i].hrasconn, hEvent, RASCN_Disconnection);

                if (ret != ERROR_SUCCESS) { // Registration failed, hang up without waiting.
                    RasHangUpW(conn[i].hrasconn);
                    CloseHandle(hEvent);
                    break; // Exit loop
                }

                // Step 2: Start the disconnection
                RasHangUpW(conn[i].hrasconn);

                // Step 3: Wait for the event
                DWORD waitResult = WaitForSingleObject(hEvent, 10000); // 10-second timeout
                CloseHandle(hEvent);

                if (waitResult != WAIT_OBJECT_0) {
                    statusMsg = _wcsdup(L"断开旧连接超时或失败。");
                    if(statusMsg) PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);
                } else {
                    statusMsg = _wcsdup(L"旧连接已断开。");
                    if(statusMsg) PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);
                }
                
                break; // Found and handled, exit loop
            }
        }
    }

    // --- Update Server IP and Dial ---
    statusMsg = _wcsdup(L"正在更新服务器地址...");
    if(statusMsg) PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);

    RASENTRYW entry = {0};
    entry.dwSize = sizeof(RASENTRYW);
    DWORD dwEntrySize = sizeof(entry);
    
    // Get the existing properties for "蓝湾网络"
    rasResult = RasGetEntryPropertiesW(NULL, L"蓝湾网络", &entry, &dwEntrySize, NULL, NULL);
    if (rasResult != SUCCESS)
    {
        statusMsg = _wcsdup(L"错误：找不到 '蓝湾网络' 条目，无法连接。");
        if(statusMsg) PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);
        return 1;
    }

    // Update only the server address
    wcscpy_s(entry.szLocalPhoneNumber, RAS_MaxPhoneNumber + 1, serverIp);

    // Set the updated properties
    rasResult = RasSetEntryPropertiesW(NULL, L"蓝湾网络", &entry, entry.dwSize, NULL, 0);
    if (rasResult != SUCCESS)
    {
        statusMsg = _wcsdup(L"更新服务器地址失败。");
        if(statusMsg) PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);
        return 1;
    }

    // --- Dial the connection ---
    statusMsg = _wcsdup(L"正在拨号...");
    if(statusMsg) PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);

    // Safety check: ensure no active connection exists before dialing.
    RASCONNW conn_check[1];
    DWORD conn_check_size = sizeof(conn_check);
    DWORD conn_count = 0;
    conn_check[0].dwSize = sizeof(RASCONNW);
    if (RasEnumConnectionsW(conn_check, &conn_check_size, &conn_count) == SUCCESS && conn_count > 0)
    {
        for (DWORD i = 0; i < conn_count; i++) {
            if (wcscmp(conn_check[i].szEntryName, L"蓝湾网络") == 0) {
                // A connection still exists, so we should not dial.
                statusMsg = _wcsdup(L"拨号失败：一个同名连接已处于活动状态。");
                if(statusMsg) PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);
                return 1; // Abort.
            }
        }
    }
    
    RASDIALPARAMSW rasDialParams = {0};
    rasDialParams.dwSize = sizeof(RASDIALPARAMSW);
    wcscpy_s(rasDialParams.szEntryName, RAS_MaxEntryName + 1, L"蓝湾网络");
    // Supply credentials directly to the dialer for robustness, even though they are saved.
    wcscpy_s(rasDialParams.szUserName, UNLEN + 1, L"vpn");
    wcscpy_s(rasDialParams.szPassword, PWLEN + 1, L"vpn");
    
    g_hRasConn = NULL; 
    rasResult = RasDialW(NULL, NULL, &rasDialParams, 0, (RasDialFunc)RasDialCallback, &g_hRasConn);

    if (rasResult == 0 || rasResult == ERROR_IO_PENDING)
    {
        SetTimer(hwnd, IDT_CONNECT_TIMEOUT, 10000, NULL);
    }
    else
    {
        wchar_t errorStr[256];
        wchar_t finalErrorMsg[512];
        if (RasGetErrorStringW(rasResult, errorStr, 256) == 0)
        {
            swprintf_s(finalErrorMsg, 512, L"拨号启动失败: %s", errorStr);
        }
        else
        {
            swprintf_s(finalErrorMsg, 512, L"拨号启动失败: 未知错误 %lu", rasResult);
        }
        statusMsg = _wcsdup(finalErrorMsg);
        if(statusMsg) PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);
        g_hRasConn = NULL;
        return 1;
    }

    return 0;
}

// Helper function to update traverse button state based on list box content
void UpdateTraverseButtonState(HWND hwnd)
{
    HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
    HWND hTraverseButton = GetDlgItem(hwnd, IDC_BUTTON_TRAVERSE);
    
    if (!hListBox || !hTraverseButton) return; 
    
    // Get item count from list box
    LRESULT itemCount = SendMessageW(hListBox, LB_GETCOUNT, 0, 0);
    
    // Enable button only if there are items
    if (itemCount > 0) {
        EnableWindow(hTraverseButton, TRUE);
    } else {
        EnableWindow(hTraverseButton, FALSE);
    }
}

// Helper function to launch the VPN connection thread
void ConnectVpn(HWND hwnd, const wchar_t* serverIp)
{
    VPN_THREAD_DATA* pData = (VPN_THREAD_DATA*)malloc(sizeof(VPN_THREAD_DATA));
    if (pData)
    {
        pData->hwnd = hwnd;
        wcscpy_s(pData->serverIp, 256, serverIp);
        HANDLE hThread = CreateThread(NULL, 0, ConnectVpnThreadProc, pData, 0, NULL);
        if (hThread)
        {
            CloseHandle(hThread); // Fire and forget
        }
        else
        {
            free(pData);
            SendMessageW(GetDlgItem(hwnd, IDC_STATUSBAR), SB_SETTEXTW, 0, (LPARAM)L"无法创建连接线程");
        }
    }
}

// Thread to traverse and test connections from the server list
DWORD WINAPI TraverseConnectionThreadProc(LPVOID lpParameter)
{
    TRAVERSE_THREAD_DATA* pData = (TRAVERSE_THREAD_DATA*)lpParameter;
    HWND hwnd = pData->hwnd;
    free(pData);

    HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
    if (!hListBox) {
        g_traverseInProgress = FALSE;
        PostMessageW(hwnd, WM_TRAVERSE_COMPLETE, 0, 0);
        return 1;
    }

    // Get the total number of items in the list box
    LRESULT itemCount = SendMessageW(hListBox, LB_GETCOUNT, 0, 0);
    if (itemCount <= 0) {
        SendMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)_wcsdup(L"列表为空"));
        g_traverseInProgress = FALSE;
        PostMessageW(hwnd, WM_TRAVERSE_COMPLETE, 0, 0);
        return 1;
    }

    // Create event for synchronization between RAS callback and this thread
    g_hTraverseConnectEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!g_hTraverseConnectEvent) {
        SendMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)_wcsdup(L"无法创建同步事件"));
        g_traverseInProgress = FALSE;
        PostMessageW(hwnd, WM_TRAVERSE_COMPLETE, 0, 0);
        return 1;
    }

    // Traverse through each server
    BOOL connectionSuccess = FALSE;
    for (int i = 0; i < itemCount && !connectionSuccess; i++) {
        // Select the current item
        SendMessageW(hListBox, LB_SETCURSEL, i, 0);

        // Get the server IP address from the list box
        int len = (int)SendMessageW(hListBox, LB_GETTEXTLEN, i, 0);
        if (len <= 0) {
            continue;
        }

        wchar_t* buffer = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
        if (!buffer) {
            continue;
        }

        SendMessageW(hListBox, LB_GETTEXT, i, (LPARAM)buffer);

        // Check if the string contains more than just whitespace
        BOOL isBlank = TRUE;
        for (int j = 0; j < len; j++) {
            if (!iswspace(buffer[j])) {
                isBlank = FALSE;
                break;
            }
        }

        if (isBlank) {
            free(buffer);
            continue;
        }

        // Update status showing which server is being tested
        wchar_t* statusMsg = (wchar_t*)malloc(512 * sizeof(wchar_t));
        if (statusMsg) {
            swprintf_s(statusMsg, 512, L"正在测试 [%d/%d] %s", i + 1, itemCount, buffer);
            PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);
        }

        // Reset the event before attempting connection
        ResetEvent(g_hTraverseConnectEvent);
        g_traverseConnectSuccess = FALSE;

        // Attempt to connect to this server
        ConnectVpn(hwnd, buffer);

        // Wait for the RAS callback to signal connection result (max 10 seconds)
        DWORD waitResult = WaitForSingleObject(g_hTraverseConnectEvent, 10000);
        
        if (waitResult == WAIT_OBJECT_0 && g_traverseConnectSuccess) {
            // Connection was successful, break the loop and keep the connection.
            connectionSuccess = TRUE;
        }

        free(buffer);
    }

    // Close the event
    if (g_hTraverseConnectEvent) {
        CloseHandle(g_hTraverseConnectEvent);
        g_hTraverseConnectEvent = NULL;
    }

    // Update final status
    wchar_t* finalMsg = NULL;
    if (connectionSuccess) {
        finalMsg = _wcsdup(L"连接成功。");
    } else {
        finalMsg = _wcsdup(L"所有服务器均连接失败。");
    }
    if (finalMsg) {
        PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)finalMsg);
    }

    g_traverseInProgress = FALSE;
    PostMessageW(hwnd, WM_TRAVERSE_COMPLETE, 0, 0);
    return 0;
}

// IKE/UDP probe functions removed: UI no longer exposes a "测试" action.

// Helper function to ensure VPN entry exists, prompting user to create it if not.
BOOL EnsureVpnEntryExists(HWND hwnd)
{
    if (VpnEntryExists()) {
        return TRUE;
    }

    // If entry doesn't exist, prompt user to create it with admin rights.
    TASKDIALOG_BUTTON aCustomButtons[] = {
        { 1001, L"继续" },
        { 1002, L"取消" }
    };

    TASKDIALOGCONFIG tdc = { sizeof(tdc) };
    tdc.hwndParent = hwnd; // Use main window as parent
    tdc.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW;
    tdc.pButtons = aCustomButtons;
    tdc.cButtons = ARRAYSIZE(aCustomButtons);
    tdc.nDefaultButton = 1002;
    tdc.pszWindowTitle = L"初始化配置";
    tdc.pszMainIcon = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = L"需要管理员权限完成初始化配置";
    tdc.pszContent = L"该操作将为系统配置 L2TP/IPsec 安全参数，仅在首次使用时执行一次。";

    int nClickedButton = 0;
    HRESULT hr = TaskDialogIndirect(&tdc, &nClickedButton, NULL, NULL);

    if (SUCCEEDED(hr) && nClickedButton == 1001) // User clicks "继续"
    {
        wchar_t szPath[MAX_PATH];
        if (GetModuleFileNameW(NULL, szPath, MAX_PATH))
        {
            SHELLEXECUTEINFOW sei = { sizeof(SHELLEXECUTEINFOW) };
            sei.cbSize = sizeof(SHELLEXECUTEINFOW);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpVerb = L"runas";
            sei.lpFile = szPath;
            sei.lpParameters = L"--create-vpn";
            sei.hwnd = NULL;
            sei.nShow = SW_HIDE;
            
            if (ShellExecuteExW(&sei)) {
                WaitForSingleObject(sei.hProcess, INFINITE);
                CloseHandle(sei.hProcess);

                if (!VpnEntryExists()) { // Admin process ran but failed
                     ShowTaskDialog(hwnd, L"配置失败", L"无法创建VPN连接条目。", L"请尝试手动以管理员身份运行本程序一次。", TDCBF_OK_BUTTON, TD_ERROR_ICON, NULL);
                     return FALSE; // Creation failed
                }
                return TRUE; // Creation successful
            } else { // UAC prompt was cancelled by user
                return FALSE;
            }
        } else { // GetModuleFileNameW failed
             ShowTaskDialog(hwnd, L"错误", L"无法找到程序路径，无法进行提权。", NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON, NULL);
             return FALSE;
        }
    }
    
    // User clicked "取消" or closed the dialog
    return FALSE;
}

// Helper function to check for an active connection and ask the user to disconnect
BOOL CheckAndConfirmDisconnect(HWND hwnd)
{
    // Check if a connection is already active
    BOOL isConnected = FALSE;
    RASCONNW conn[1];
    DWORD connSize = sizeof(conn);
    DWORD numConn = 0;
    conn[0].dwSize = sizeof(RASCONNW);

    if (RasEnumConnectionsW(conn, &connSize, &numConn) == SUCCESS && numConn > 0) {
        for (DWORD i = 0; i < numConn; i++) {
            if (wcscmp(conn[i].szEntryName, L"蓝湾网络") == 0) {
                isConnected = TRUE;
                break;
            }
        }
    }

    if (isConnected) {
        TASKDIALOG_BUTTON aCustomButtons[] = {
            { 1001, L"继续" },
            { 1002, L"取消" }
        };

        TASKDIALOGCONFIG tdc = { sizeof(tdc) };
        tdc.hwndParent = hwnd;
        tdc.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW;
        tdc.pButtons = aCustomButtons;
        tdc.cButtons = ARRAYSIZE(aCustomButtons);
        tdc.nDefaultButton = 1002; // Default to Cancel
        tdc.pszWindowTitle = L"确认";
        tdc.pszMainIcon = TD_WARNING_ICON;
        tdc.pszMainInstruction = L"该操作将断开当前的连接。";
        tdc.pszContent = L"是否继续？";

        int nClickedButton = 0;
        HRESULT hr = TaskDialogIndirect(&tdc, &nClickedButton, NULL, NULL);

        if (SUCCEEDED(hr) && nClickedButton == 1001) {
            return TRUE; // Continue was clicked
        } else {
            return FALSE; // Anything else (Cancel, close dialog, error) means stop
        }
    }

    return TRUE; // Proceed
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        {
            // Create a list box
            CreateWindowW(
                L"LISTBOX",
                NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_STANDARD,
                10, 10, 200, 300,
                hwnd,
                (HMENU)IDC_LISTBOX,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                NULL);

            // --- Theming and Button Setup ---
            HMODULE hUxtheme = LoadLibraryW(L"uxtheme.dll");
            PFN_SETWINDOWTHEME pfnSetWindowTheme = NULL;
            if (hUxtheme) {
                pfnSetWindowTheme = (PFN_SETWINDOWTHEME)GetProcAddress(hUxtheme, "SetWindowTheme");
            }

            // Create a button
            HWND hButton = CreateWindowW(
                L"BUTTON", L"刷新",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                220, 10, 100, 30, hwnd, (HMENU)IDC_BUTTON,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            if (pfnSetWindowTheme) {
                pfnSetWindowTheme(hButton, NULL, NULL);
            }

            // Create a traverse test button
            HWND hTraverseButton = CreateWindowW(
                L"BUTTON", L"连接",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                220, 50, 100, 30, hwnd, (HMENU)IDC_BUTTON_TRAVERSE,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            if (pfnSetWindowTheme) {
                pfnSetWindowTheme(hTraverseButton, NULL, NULL);
            }
            if (hUxtheme) {
                FreeLibrary(hUxtheme);
            }
            

            // Create the status bar.
            hStatusBar = CreateWindowExW(
                0,
                STATUSCLASSNAMEW,
                NULL,
                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                0, 0, 0, 0,
                hwnd,
                (HMENU)IDC_STATUSBAR,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                NULL);
            
            SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"就绪");
            
            UpdateFont(hwnd);
            UpdateTraverseButtonState(hwnd);
            AddTrayIcon(hwnd);
        }
        break;

    case WM_DPICHANGED:
        {
            UpdateFont(hwnd);

            // The WM_DPICHANGED message recommends resizing the window based on the suggested rect.
            RECT* const prcNewWindow = (RECT*)lParam;
            SetWindowPos(hwnd,
                NULL,
                prcNewWindow->left,
                prcNewWindow->top,
                prcNewWindow->right - prcNewWindow->left,
                prcNewWindow->bottom - prcNewWindow->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;

    case WM_CLOSE:
        g_wasMaximized = IsZoomed(hwnd);
        if (!g_wasMaximized)
        {
            GetWindowRect(hwnd, &g_rcOriginalWindowPos);
        }
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_SIZE:
        {
            // Resize the status bar.
            SendMessageW(hStatusBar, WM_SIZE, wParam, lParam);

            // Get the height of the status bar.
            RECT rectStatusBar;
            GetWindowRect(hStatusBar, &rectStatusBar);
            int statusBarHeight = rectStatusBar.bottom - rectStatusBar.top;

            // Get the new dimensions of the client area.
            int clientWidth = LOWORD(lParam);
            int clientHeight = HIWORD(lParam);

            HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
            HWND hButton = GetDlgItem(hwnd, IDC_BUTTON);
            HWND hTraverseButton = GetDlgItem(hwnd, IDC_BUTTON_TRAVERSE);

            UINT dpi = GetDpiForWindow(hwnd);
            if (dpi == 0) dpi = 96;
            int padding = MulDiv(10, dpi, 96);
            int buttonWidth = MulDiv(100, dpi, 96);
            int buttonHeight = MulDiv(30, dpi, 96);
            int buttonPadding = MulDiv(5, dpi, 96);


            SetWindowPos(hListBox, NULL, padding, padding, clientWidth - buttonWidth - (padding*3), clientHeight - statusBarHeight - (padding*2), SWP_NOZORDER);
            SetWindowPos(hButton, NULL, clientWidth - buttonWidth - padding, padding, buttonWidth, buttonHeight, SWP_NOZORDER);
            SetWindowPos(hTraverseButton, NULL, clientWidth - buttonWidth - padding, padding + buttonHeight + buttonPadding, buttonWidth, buttonHeight, SWP_NOZORDER);
        }
        break;

    case WM_CONTEXTMENU:
        {
            HWND hClickedWnd = (HWND)wParam;
            HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);

            if (hClickedWnd == hListBox)
            {
                POINTS pts = MAKEPOINTS(lParam);
                POINT client_pt = { pts.x, pts.y };
                ScreenToClient(hListBox, &client_pt);

                LRESULT item_index_result = SendMessageW(hListBox, LB_ITEMFROMPOINT, 0, MAKELPARAM(client_pt.x, client_pt.y));
                
                if (HIWORD(item_index_result) == 0)
                {
                    int item_index = LOWORD(item_index_result);
                    
                    int len = (int)SendMessageW(hListBox, LB_GETTEXTLEN, item_index, 0);
                    if (len > 0)
                    {
                        wchar_t* buffer = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
                        if (buffer)
                        {
                            SendMessageW(hListBox, LB_GETTEXT, item_index, (LPARAM)buffer);
                            
                            BOOL isBlank = TRUE;
                            for (int i = 0; i < len; i++) {
                                if (!iswspace(buffer[i])) {
                                    isBlank = FALSE;
                                    break;
                                }
                            }
                            free(buffer);

                            if (!isBlank)
                            {
                                SendMessageW(hListBox, LB_SETCURSEL, item_index, 0);
                                
                                HMENU hPopupMenu = CreatePopupMenu();
                                InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_LISTBOX_CONNECT, L"连接");

                                SetForegroundWindow(hwnd);
                                int command = TrackPopupMenu(hPopupMenu, TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RETURNCMD, pts.x, pts.y, 0, hwnd, NULL);
                                DestroyMenu(hPopupMenu);

                                if (command > 0)
                                {
                                    PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
                                }
                            }
                        }
                    }
                }
            }
        }
        break;

    case WM_DESTROY:
        {
            SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"正在删除连接条目...");
            // Cleanly delete the RAS entry using the API. This is synchronous.
            RasDeleteEntryW(NULL, L"蓝湾网络");
        }
        DestroyIcon(g_hIconDefault);
        DestroyIcon(g_hIconConnected);
        RemoveTrayIcon(hwnd);
        // Only delete the font if it was created
        if (hGuiFont)
        {
            DeleteObject(hGuiFont);
        }
        PostQuitMessage(0);
        return 0;

    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDC_LISTBOX:
                    if (HIWORD(wParam) == LBN_DBLCLK)
                    {
                        if (!EnsureVpnEntryExists(hwnd)) break;

                        if (!CheckAndConfirmDisconnect(hwnd)) break;

                        HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
                        int index = (int)SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
                        if (index != LB_ERR)
                        {
                            int len = (int)SendMessageW(hListBox, LB_GETTEXTLEN, index, 0);
                            if (len > 0) // LB_ERR is -1, so this also handles errors. An empty string has len 0.
                            {
                                wchar_t* buffer = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
                                if (buffer)
                                {
                                    SendMessageW(hListBox, LB_GETTEXT, index, (LPARAM)buffer);
                                    
                                    // Check if the string contains more than just whitespace
                                    BOOL isBlank = TRUE;
                                    for (int i = 0; i < len; i++)
                                    {
                                        if (!iswspace(buffer[i]))
                                        {
                                            isBlank = FALSE;
                                            break;
                                        }
                                    }

                                    if (!isBlank)
                                    {
                                        // The logic for checking and hanging up existing connections
                                        // is now inside ConnectVpnThreadProc.
                                        ConnectVpn(hwnd, buffer);
                                    }
                                    
                                    free(buffer);
                                }
                            }
                        }
                    }
                    break;
                case IDC_BUTTON:
                {
                    HWND hButton = GetDlgItem(hwnd, IDC_BUTTON);
                    EnableWindow(hButton, FALSE);
                    SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"正在刷新...");

                    THREAD_DATA* pData = (THREAD_DATA*)malloc(sizeof(THREAD_DATA));
                    if (pData)
                    {
                        pData->hwnd = hwnd;
                        HANDLE hThread = CreateThread(NULL, 0, DownloadThreadProc, pData, 0, NULL);
                        if (hThread)
                        {
                            CloseHandle(hThread);
                        }
                    } else {
                        SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"内存分配失败");
                        EnableWindow(hButton, TRUE);
                    }
                    break;
                }
                case IDC_BUTTON_TRAVERSE:
                {
                    if (!EnsureVpnEntryExists(hwnd)) break;

                    if (g_traverseInProgress) {
                        SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"正在顺序连接...");
                        break;
                    }
                    
                    if (!CheckAndConfirmDisconnect(hwnd)) {
                        break; // User chose not to continue
                    }

                    HWND hTraverseButton = GetDlgItem(hwnd, IDC_BUTTON_TRAVERSE);
                    EnableWindow(hTraverseButton, FALSE);
                    SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"开始顺序连接...");

                    g_traverseInProgress = TRUE;

                    TRAVERSE_THREAD_DATA* pData = (TRAVERSE_THREAD_DATA*)malloc(sizeof(TRAVERSE_THREAD_DATA));
                    if (pData)
                    {
                        pData->hwnd = hwnd;
                        HANDLE hThread = CreateThread(NULL, 0, TraverseConnectionThreadProc, pData, 0, NULL);
                        if (hThread)
                        {
                            CloseHandle(hThread);
                        }
                        else
                        {
                            free(pData);
                            EnableWindow(hTraverseButton, TRUE);
                            g_traverseInProgress = FALSE;
                            SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"无法创建顺序连接线程");
                        }
                    }
                    else
                    {
                        EnableWindow(hTraverseButton, TRUE);
                        g_traverseInProgress = FALSE;
                        SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"内存分配失败");
                    }
                    break;
                }
                case ID_HELP_ABOUT:
                    DialogBoxW(g_hInstance, MAKEINTRESOURCEW(IDD_ABOUTBOX), hwnd, AboutDialogProc);
                    break;
                case ID_TRAY_RESTORE:
                    if (IsWindowVisible(hwnd))
                    {
                        if (IsIconic(hwnd))
                        {
                            ShowWindow(hwnd, SW_RESTORE);
                        }
                        SetForegroundWindow(hwnd);
                    }
                    else
                    {
                        RestoreWindow(hwnd);
                    }
                    break;
                case ID_TRAY_EXIT:
                    DestroyWindow(hwnd);
                    break;
                // "测试" menu and test button removed
                case ID_LISTBOX_CONNECT:
                    {
                        if (!EnsureVpnEntryExists(hwnd)) break;

                        if (!CheckAndConfirmDisconnect(hwnd)) break;
                        
                        HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
                        int index = (int)SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
                        if (index != LB_ERR)
                        {
                            int len = (int)SendMessageW(hListBox, LB_GETTEXTLEN, index, 0);
                            if (len > 0)
                            {
                                wchar_t* buffer = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
                                if (buffer)
                                {
                                    SendMessageW(hListBox, LB_GETTEXT, index, (LPARAM)buffer);
                                    
                                    // Call the VPN connection function
                                    ConnectVpn(hwnd, buffer);

                                    free(buffer);
                                }
                            }
                        }
                    }
                    break;
            }
        }
        break;
        
    case WM_DOWNLOAD_SUCCESS:
    {
        DOWNLOADED_DATA* pData = (DOWNLOADED_DATA*)lParam;
        char* pszData = pData->buffer;
        DWORD dataSize = pData->size;

        // For debugging, save the downloaded content to a file
        HANDLE hFile = CreateFileW(L"C:\\Users\\梁升勇\\.gemini\\tmp\\975576e5cca9abbc0f489763bdeb291d493e2087a4bf097ae4d7b208579f88a4\\download_content.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            DWORD bytesWritten;
            WriteFile(hFile, pszData, dataSize, &bytesWritten, NULL);
            CloseHandle(hFile);
        }

        HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
        HWND hButton = GetDlgItem(hwnd, IDC_BUTTON);

        // Convert multi-byte string (UTF-8 from web) to wide-char string
        // Use the explicit data size instead of relying on null termination (-1)
        int wideCharCount = MultiByteToWideChar(CP_UTF8, 0, pszData, dataSize, NULL, 0);
        if (wideCharCount > 0)
        {
            // Allocate enough space for the wide-char string plus a null terminator
            wchar_t* pwszData = (wchar_t*)malloc((wideCharCount + 1) * sizeof(wchar_t));
            if (pwszData)
            {
                MultiByteToWideChar(CP_UTF8, 0, pszData, dataSize, pwszData, wideCharCount);
                pwszData[wideCharCount] = L'\0'; // Manually add null terminator

                SendMessageW(hListBox, LB_RESETCONTENT, 0, 0);
                
                wchar_t* context = NULL;
                wchar_t* line = wcstok_s(pwszData, L"\n", &context);
                while (line != NULL)
                {
                    // Trim trailing \r if present
                    size_t len = wcslen(line);
                    if (len > 0 && line[len-1] == L'\r') { 
                        line[len-1] = L'\0';
                    }
                    SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)line);
                    line = wcstok_s(NULL, L"\n", &context);
                }
                free(pwszData);
            }
        }

        free(pszData); // Free the original buffer
        free(pData);   // Free the container struct

        // Get the number of items in the list box
        LRESULT serverCount = SendMessageW(hListBox, LB_GETCOUNT, 0, 0);

        // Format the success message with the server count
        wchar_t statusText[100];
        swprintf_s(statusText, sizeof(statusText)/sizeof(wchar_t), L"刷新成功，共 %lld 台服务器", serverCount);
        
        SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)statusText);
        EnableWindow(hButton, TRUE);
        
        // Update traverse button state based on list content
        UpdateTraverseButtonState(hwnd);
        break;
    }

    case WM_DOWNLOAD_FAILURE:
    {
        HWND hButton = GetDlgItem(hwnd, IDC_BUTTON);
        HWND hTraverseButton = GetDlgItem(hwnd, IDC_BUTTON_TRAVERSE);
        SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"刷新失败");
        EnableWindow(hButton, TRUE);
        // 如果列表框中仍有数据，则允许用户点击“依次连接”
        UpdateTraverseButtonState(hwnd);
        break;
    }

    case WM_VPN_STATUS_UPDATE:
    {
        wchar_t* statusMsg = (wchar_t*)lParam;
        if (statusMsg)
        {
            SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)statusMsg);
            free(statusMsg);
        }
        break;
    }

    case WM_TRAVERSE_COMPLETE:
    {
        // Re-enable the traverse button when the traversal completes
        HWND hTraverseButton = GetDlgItem(hwnd, IDC_BUTTON_TRAVERSE);
        if (hTraverseButton) {
            EnableWindow(hTraverseButton, TRUE);
        }
        break;
    }

    // IKE probe messages removed

    case WM_TIMER:
        if (wParam == IDT_CONNECT_TIMEOUT)
        {
            // Kill the timer to prevent it from firing again
            KillTimer(hwnd, IDT_CONNECT_TIMEOUT);
            
            // If the connection is still trying to connect, hang it up
            if (g_hRasConn != NULL)
            {
                SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"连接超时");
                RasHangUpW(g_hRasConn);
                g_hRasConn = NULL; // Clear the handle
            }
        }
        else if (wParam == IDT_CONNECTDEVICE_TIMEOUT)
        {
            KillTimer(hwnd, IDT_CONNECTDEVICE_TIMEOUT);
            if (g_hRasConn != NULL)
            {
                SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"连接服务器超时");
                RasHangUpW(g_hRasConn);
                g_hRasConn = NULL;
            }
        }
        break;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            // Background is now handled by the window class brush.
            EndPaint(hwnd, &ps);
        }
        return 0;
    
    case WM_APP_SHOW:
        RestoreWindow(hwnd);
        break;
    
    case WM_APP_TRAYMSG:
        switch (LOWORD(lParam))
        {
            case WM_CONTEXTMENU:
                ShowTrayContextMenu(hwnd);
                break;
            case WM_LBUTTONDBLCLK:
                if (IsWindowVisible(hwnd))
                {
                    if (IsIconic(hwnd))
                    {
                        ShowWindow(hwnd, SW_RESTORE);
                    }
                    SetForegroundWindow(hwnd);
                }
                else
                {
                    RestoreWindow(hwnd);
                }
                break;
        }
        break;

    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
