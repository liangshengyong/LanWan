#define _WIN32_WINNT 0x0A00

// Winsock needs to be included before windows.h
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <windows.h>
#include <commctrl.h>
#include <wchar.h>
#include <shellapi.h>
#define INITGUID
#include <shlobj.h>
#include <knownfolders.h>
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

#ifndef ERROR_ALREADY_DIALING
#define ERROR_ALREADY_DIALING 801
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


#define ID_LISTBOX_CONNECT 2001
#define IDT_CONNECT_TIMEOUT 2002
#define IDT_CONNECTDEVICE_TIMEOUT 2003
#define ID_LISTBOX_COPY 2004
#define ID_LISTBOX_DISCONNECT 2005

// Custom messages for download thread
#define WM_DOWNLOAD_SUCCESS (WM_APP + 3)
#define WM_DOWNLOAD_FAILURE (WM_APP + 4)
#define WM_VPN_STATUS_UPDATE (WM_APP + 5)
#define WM_TRAVERSE_COMPLETE (WM_APP + 7)
#define WM_APP_UPDATE_FONT (WM_APP + 8)
#define WM_APP_UPDATE_CHECK_SUCCESS (WM_APP + 9)
#define WM_APP_UPDATE_CHECK_FAILURE (WM_APP + 10)
#define WM_APP_UPDATE_UI_BUTTON_STATES (WM_APP + 11)

// Window procedure function
INT_PTR CALLBACK MainDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HINSTANCE g_hInstance;

extern HFONT hGuiFont;
HWND g_hAboutDialog = NULL;

// Global variable to store the IP of the server currently connecting to
wchar_t g_currentConnectingServerIp[256] = {0};

// Global variable to store the IP of the last successfully connected server during traverse
wchar_t g_lastSuccessfullyConnectedIp[256] = {0};

// Global variable to store the IP of the currently connected server
wchar_t g_connectedIp[256] = {0};

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

// Helper function to measure text width
int GetTextWidth(HWND hWnd, HFONT hFont, const WCHAR* text)
{
    if (!hWnd || !hFont || !text || wcslen(text) == 0) return 0;

    HDC hdc = GetDC(hWnd);
    if (!hdc) return 0;

    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    SIZE size;
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &size);

    SelectObject(hdc, hOldFont);
    ReleaseDC(hWnd, hdc);

    return size.cx;
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

    // 程序名称
    HWND hName = GetDlgItem(hDlg, IDC_ABOUT_NAME);
    if (hName)
    {
        SetWindowPos(hName, NULL, textX, yPos, textWidth, textHeight, SWP_NOZORDER);
        yPos += textHeight;
    }

    // Description
    HWND hDesc = GetDlgItem(hDlg, IDC_ABOUT_VERSION);
    if (hDesc)
    {
        SetWindowPos(hDesc, NULL, textX, yPos, textWidth, textHeight, SWP_NOZORDER);
        yPos += textHeight * 2;
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
        int buttonHeight = DpiScale(24, dpi);
        int newX = (clientWidth - buttonWidth) / 2;
        int newY = clientHeight - buttonHeight - padding;
        //int panelTop = clientHeight - buttonHeight - padding * 2;
        //int newY = panelTop + padding;
        if (newX < 0) newX = 0;
        if (newY < 0) newY = 0;
        SetWindowPos(hOkButton, NULL, newX, newY, buttonWidth, buttonHeight, SWP_NOZORDER);
    }
}


typedef struct {
    HWND hDlg;
} UPDATE_CHECK_THREAD_DATA;

// Helper function to find a substring in a buffer with a given length
const char* strnstr(const char* haystack, const char* needle, size_t len) {
    if (!needle || *needle == '\0') return haystack;
    size_t needle_len = strlen(needle);
    if (needle_len == 0 || needle_len > len) return NULL;

    for (size_t i = 0; i <= len - needle_len; ++i) {
        if (strncmp(&haystack[i], needle, needle_len) == 0) {
            return &haystack[i];
        }
    }
    return NULL;
}

DWORD WINAPI CheckForUpdateThreadProc(LPVOID lpParameter) {
    UPDATE_CHECK_THREAD_DATA* pData = (UPDATE_CHECK_THREAD_DATA*)lpParameter;
    HWND hDlg = pData->hDlg;
    free(pData);

    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    BOOL bSuccess = FALSE;
    wchar_t* pTagName = NULL;

    hSession = WinHttpOpen(L"LanWan/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (hSession) {
        hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    }
    if (hConnect) {
        hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/repos/liangshengyong/LanWan/releases/latest", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    }
    if (hRequest) {
        const wchar_t* headers = L"User-Agent: LanWan\r\nAccept: application/vnd.github+json";
        if (WinHttpAddRequestHeaders(hRequest, headers, -1L, WINHTTP_ADDREQ_FLAG_ADD)) {
            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                if (WinHttpReceiveResponse(hRequest, NULL)) {
                    DWORD dwStatusCode = 0;
                    DWORD dwSize = sizeof(dwStatusCode);
                    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &dwStatusCode, &dwSize, NULL);

                    if (dwStatusCode == 200) {
                        char* buffer = NULL;
                        DWORD totalSize = 0;
                        
                        do {
                            dwSize = 0;
                            if (WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                                if (dwSize > 0 && dwSize <= (1024 * 1024) && (totalSize + dwSize) > totalSize) {
                                    char* new_buffer = (char*)realloc(buffer, totalSize + dwSize);
                                    if (new_buffer) {
                                        buffer = new_buffer;
                                        DWORD downloaded = 0;
                                        if (WinHttpReadData(hRequest, buffer + totalSize, dwSize, &downloaded)) {
                                            totalSize += downloaded;
                                        } else { free(buffer); buffer = NULL; break; }
                                    } else { free(buffer); buffer = NULL; break; }
                                }
                            }
                        } while (dwSize > 0);

                        if (buffer) {
                            const char* tag_name_start_key = "\"tag_name\":\"";
                            const char* tag_name_start = strnstr(buffer, tag_name_start_key, totalSize);
                            if (tag_name_start) {
                                tag_name_start += strlen(tag_name_start_key);
                                const char* tag_name_end = strchr(tag_name_start, '"');
                                if (tag_name_end) {
                                    size_t tag_name_len = tag_name_end - tag_name_start;
                                    int w_tag_name_len = MultiByteToWideChar(CP_UTF8, 0, tag_name_start, tag_name_len, NULL, 0);
                                    pTagName = (wchar_t*)calloc((size_t)w_tag_name_len + 1, sizeof(wchar_t));
                                    if (pTagName) {
                                        MultiByteToWideChar(CP_UTF8, 0, tag_name_start, tag_name_len, pTagName, w_tag_name_len);
                                        pTagName[w_tag_name_len] = L'\0';
                                        bSuccess = TRUE;
                                    }
                                }
                            }
                            free(buffer);
                        }
                    }
                }
            }
        }
    }

    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    if (bSuccess && pTagName) {
        PostMessage(hDlg, WM_APP_UPDATE_CHECK_SUCCESS, 0, (LPARAM)pTagName);
    } else {
        PostMessage(hDlg, WM_APP_UPDATE_CHECK_FAILURE, 0, 0);
    }

    return 0;
}

#include <strsafe.h>

// Function to get the application's product version from resources
BOOL GetAppVersion(wchar_t* version_str, int len)
{
    wchar_t module_path[MAX_PATH];
    if (GetModuleFileNameW(NULL, module_path, MAX_PATH) == 0) {
        return FALSE;
    }

    DWORD ver_handle = 0;
    DWORD ver_info_size = GetFileVersionInfoSizeW(module_path, &ver_handle);
    if (ver_info_size == 0) {
        return FALSE;
    }

    void* ver_data = malloc(ver_info_size);
    if (!ver_data) {
        return FALSE;
    }

    if (!GetFileVersionInfoW(module_path, ver_handle, ver_info_size, ver_data)) {
        free(ver_data);
        return FALSE;
    }

    LPVOID lp_buffer = NULL;
    UINT pu_len = 0;
    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lp_translate;

    if (VerQueryValueW(ver_data, L"\\VarFileInfo\\Translation", (LPVOID*)&lp_translate, &pu_len)) {
        wchar_t query_str[50];
        StringCchPrintfW(query_str, 50, L"\\StringFileInfo\\%04x%04x\\ProductVersion",
            lp_translate[0].wLanguage, lp_translate[0].wCodePage);

        if (VerQueryValueW(ver_data, query_str, &lp_buffer, &pu_len)) {
            wcsncpy_s(version_str, len, (wchar_t*)lp_buffer, _TRUNCATE);
            free(ver_data);
            return TRUE;
        }
    }

    free(ver_data);
    return FALSE;
}

// Compare two version strings (e.g., "2026.1.1")
// Returns: >0 if v1 > v2, <0 if v1 < v2, 0 if v1 == v2
int CompareVersions(const wchar_t* v1, const wchar_t* v2) {
    int v1_parts[3] = {0};
    int v2_parts[3] = {0};

    swscanf_s(v1, L"%d.%d.%d", &v1_parts[0], &v1_parts[1], &v1_parts[2]);
    swscanf_s(v2, L"%d.%d.%d", &v2_parts[0], &v2_parts[1], &v2_parts[2]);

    for (int i = 0; i < 3; i++) {
        if (v1_parts[i] > v2_parts[i]) return 1;
        if (v1_parts[i] < v2_parts[i]) return -1;
    }
    return 0;
}

INT_PTR CALLBACK AboutDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static UINT s_uCurrentDpi = 96;
    static GpBitmap* gpBitmapQR = NULL;
    static wchar_t s_currentVersion[32] = {0};

    switch (message)
    {
    case WM_INITDIALOG:
        {
            g_hAboutDialog = hDlg; // Store dialog handle

            // Get and store the current app version
            if (!GetAppVersion(s_currentVersion, ARRAYSIZE(s_currentVersion))) {
                wcscpy_s(s_currentVersion, ARRAYSIZE(s_currentVersion), L"Unknown");
            }

            // Set initial text
            wchar_t initial_text[256];
            swprintf_s(initial_text, ARRAYSIZE(initial_text), L"版本 %s 正在检查更新...", s_currentVersion);
            SetDlgItemTextW(hDlg, IDC_ABOUT_VERSION, initial_text);


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
    case WM_SHOWWINDOW:
        if (wParam == TRUE) // Dialog is being shown
        {
            UPDATE_CHECK_THREAD_DATA* pData = (UPDATE_CHECK_THREAD_DATA*)malloc(sizeof(UPDATE_CHECK_THREAD_DATA));
            if (pData) {
                pData->hDlg = hDlg;
                HANDLE hThread = CreateThread(NULL, 0, CheckForUpdateThreadProc, pData, 0, NULL);
                if (hThread) {
                    CloseHandle(hThread);
                } else {
                    free(pData);
                    PostMessage(hDlg, WM_APP_UPDATE_CHECK_FAILURE, 0, 0);
                }
            } else {
                 PostMessage(hDlg, WM_APP_UPDATE_CHECK_FAILURE, 0, 0);
            }
        }
        break;
    case WM_APP_UPDATE_CHECK_SUCCESS:
        {
            wchar_t* new_tag = (wchar_t*)lParam;
            if (new_tag)
            {
                wchar_t message[256];
                if (CompareVersions(new_tag, s_currentVersion) > 0) {
                    swprintf_s(message, ARRAYSIZE(message), L"版本 %s 发现新版本 <A HREF=\"download\">前往下载</A>", s_currentVersion);
                } else {
                    swprintf_s(message, ARRAYSIZE(message), L"版本 %s 已是最新版本", s_currentVersion);
                }
                SetDlgItemTextW(hDlg, IDC_ABOUT_VERSION, message);
                free(new_tag);
            }
        }
        break;
    case WM_APP_UPDATE_CHECK_FAILURE:
        {
            wchar_t message[256];
            swprintf_s(message, ARRAYSIZE(message), L"版本 %s 检查更新失败", s_currentVersion);
            SetDlgItemTextW(hDlg, IDC_ABOUT_VERSION, message);
        }
        break;
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
    case WM_NOTIFY:
        {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->idFrom == IDC_ABOUT_NAME) {
                switch (pnmh->code) {
                    case NM_CLICK:
                    case NM_RETURN:
                        {
                            PNMLINK pNMLink = (PNMLINK)lParam;
                            if (wcscmp(pNMLink->item.szUrl, L"repository") == 0)
                            {
                                ShellExecuteW(NULL, L"open", L"https://github.com/liangshengyong/LanWan", NULL, NULL, SW_SHOWNORMAL);
                            }
                        }
                        break;
                }
            }
            if (pnmh->idFrom == IDC_ABOUT_VERSION) {
                switch (pnmh->code) {
                    case NM_CLICK:
                    case NM_RETURN:
                        {
                            PNMLINK pNMLink = (PNMLINK)lParam;
                            if (wcscmp(pNMLink->item.szUrl, L"download") == 0)
                            {
                                ShellExecuteW(NULL, L"open", L"https://github.com/liangshengyong/LanWan/releases/latest", NULL, NULL, SW_SHOWNORMAL);
                            }
                        }
                        break;
                }
            }
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
            int padding = DpiScale(10, dpi);
            int buttonHeight = DpiScale(24, dpi);
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
                    // Get the x-coordinate of IDC_STATIC_SUPPORT_TEXT
                    HWND hStaticText = GetDlgItem(hDlg, IDC_STATIC_SUPPORT_TEXT);
                    int x = 0; // Default if control not found
                    if (hStaticText)
                    {
                        RECT rcStaticTextScreen;
                        GetWindowRect(hStaticText, &rcStaticTextScreen);
                        POINT ptStaticTextClient = {rcStaticTextScreen.left, rcStaticTextScreen.top};
                        ScreenToClient(hDlg, &ptStaticTextClient);
                        x = ptStaticTextClient.x;
                    }

                    int initialWidth = DpiScale(200, dpi); // Use an initial width for calculation
                    int calculatedWidth = rcClient.right - (2 * x);
                    if (calculatedWidth < 0) calculatedWidth = initialWidth; // Fallback if calculation is negative

                    int width = calculatedWidth;
                    int height = calculatedWidth; // Assume square for QR code
                    
                    // Calculate Y position to be below the text labels
                    int textBlockHeight = DpiScale(20, dpi) * 4; // 3 lines of text
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

// Global flag for single dial-up connection status
BOOL g_dialInProgress = FALSE;

// Global flag to indicate that a refresh is in progress
BOOL g_isRefreshing = FALSE;

// Global flag to indicate if the "confirm disconnect" dialog is active
BOOL g_isConfirmDisconnectDialogActive = FALSE;

// Global event and result for traverse connection synchronization
HANDLE g_hTraverseConnectEvent = NULL;  // Event to signal connection result
BOOL g_traverseConnectSuccess = FALSE;  // Connection success/failure flag





// 
// Tray Icon definitions and functions
//
#define WM_APP_TRAYMSG (WM_APP + 1)
#define WM_APP_SHOW (WM_APP + 2)
#define ID_TRAY_RESTORE 3001
#define ID_TRAY_EXIT 3002
#define ID_TRAY_CONNECT 3003
#define ID_TRAY_DISCONNECT 3004

NOTIFYICONDATAW g_nid;

// Global handles for tray icons
HICON g_hIconDefault = NULL;
HICON g_hIconConnected = NULL;

void UpdateTrayIcon(BOOL isConnected)
{
    if (isConnected) {
        g_nid.hIcon = g_hIconConnected;
        wcscpy_s(g_nid.szTip, sizeof(g_nid.szTip)/sizeof(wchar_t), L"蓝湾 已连接");
    } else {
        g_nid.hIcon = g_hIconDefault;
        wcscpy_s(g_nid.szTip, sizeof(g_nid.szTip)/sizeof(wchar_t), L"蓝湾 未连接");
    }
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}



// Data for the master download thread, which manages multiple download workers
typedef struct {
    HWND hwnd;
} MASTER_DOWNLOAD_THREAD_DATA;

// Data for each worker downloader thread
typedef struct {
    const wchar_t* url;
    DOWNLOADED_DATA* pResult;
    volatile LONG* pSuccessFlag;
} WORKER_DOWNLOAD_THREAD_DATA;

// Forward declaration for the worker thread
DWORD WINAPI SingleDownloadThreadProc(LPVOID lpParameter);

// Master thread function to orchestrate downloading from multiple URLs
DWORD WINAPI MasterDownloadThreadProc(LPVOID lpParameter)
{
    MASTER_DOWNLOAD_THREAD_DATA* pMasterData = (MASTER_DOWNLOAD_THREAD_DATA*)lpParameter;
    HWND hwnd = pMasterData->hwnd;
    free(pMasterData);

    const wchar_t* urls[] = {
        L"https://liangshengyong.github.io/LanWan/data/servers.txt",
        L"https://raw.githubusercontent.com/liangshengyong/LanWan/refs/heads/master/data/servers.txt",
        L"https://cdn.jsdelivr.net/gh/liangshengyong/LanWan/data/servers.txt"
    };
    int numUrls = sizeof(urls) / sizeof(urls[0]);

    HANDLE hThreads[ARRAYSIZE(urls)];
    WORKER_DOWNLOAD_THREAD_DATA workerData[ARRAYSIZE(urls)];
    DOWNLOADED_DATA result = {0};
    volatile LONG successFlag = 0;
    int createdThreads = 0;

    for (int i = 0; i < numUrls; ++i) {
        workerData[i].url = urls[i];
        workerData[i].pResult = &result;
        workerData[i].pSuccessFlag = &successFlag;

        HANDLE hThread = CreateThread(NULL, 0, SingleDownloadThreadProc, &workerData[i], 0, NULL);
        if (hThread) {
            hThreads[createdThreads++] = hThread;
        }
    }

    if (createdThreads > 0) {
        WaitForMultipleObjects(createdThreads, hThreads, TRUE, INFINITE);
    }

    for (int i = 0; i < createdThreads; ++i) {
        CloseHandle(hThreads[i]);
    }

    if (successFlag == 1 && result.buffer) {
        DOWNLOADED_DATA* pMsgData = (DOWNLOADED_DATA*)malloc(sizeof(DOWNLOADED_DATA));
        if (pMsgData) {
            *pMsgData = result;
            PostMessage(hwnd, WM_DOWNLOAD_SUCCESS, 0, (LPARAM)pMsgData);
        } else {
            free(result.buffer); // Avoid memory leak
            PostMessage(hwnd, WM_DOWNLOAD_FAILURE, 0, 0);
        }
    } else {
        PostMessage(hwnd, WM_DOWNLOAD_FAILURE, 0, 0);
    }

    return 0;
}

// Worker thread function to download from a single URL
DWORD WINAPI SingleDownloadThreadProc(LPVOID lpParameter)
{
    WORKER_DOWNLOAD_THREAD_DATA* pData = (WORKER_DOWNLOAD_THREAD_DATA*)lpParameter;
    if (InterlockedAdd(pData->pSuccessFlag, 0) == 1) {
        return 0;
    }

    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    BOOL bResults = FALSE;
    LPSTR tempBuffer = NULL;
    DWORD totalSize = 0;

    URL_COMPONENTSW urlComps;
    wchar_t szHostName[256];
    wchar_t szUrlPath[2048];
    ZeroMemory(&urlComps, sizeof(urlComps));
    urlComps.dwStructSize = sizeof(urlComps);
    urlComps.lpszHostName = szHostName;
    urlComps.dwHostNameLength = ARRAYSIZE(szHostName);
    urlComps.lpszUrlPath = szUrlPath;
    urlComps.dwUrlPathLength = ARRAYSIZE(szUrlPath);

    if (!WinHttpCrackUrl(pData->url, 0, 0, &urlComps)) {
        return 1;
    }

    hSession = WinHttpOpen(L"LanWan Client/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return 1;

    if (InterlockedAdd(pData->pSuccessFlag, 0) == 1) { WinHttpCloseHandle(hSession); return 0; }

    hConnect = WinHttpConnect(hSession, szHostName, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return 1; }

    if (InterlockedAdd(pData->pSuccessFlag, 0) == 1) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return 0; }

    hRequest = WinHttpOpenRequest(hConnect, L"GET", szUrlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return 1; }

    WinHttpSetTimeouts(hRequest, 5000, 5000, 5000, 5000);
    
    if (InterlockedAdd(pData->pSuccessFlag, 0) == 1) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return 0; }
    
    bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }
    
    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwStatusCodeSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &dwStatusCode, &dwStatusCodeSize, NULL);

        if (dwStatusCode == 200) {
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;
            do {
                if (InterlockedAdd(pData->pSuccessFlag, 0) == 1) { free(tempBuffer); bResults = FALSE; break; }
                
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) { bResults = FALSE; break; }

                if (dwSize > 0) {
                    LPSTR newBuffer = (LPSTR)realloc(tempBuffer, totalSize + dwSize + 1);
                    if (newBuffer) {
                        tempBuffer = newBuffer;
                        if (WinHttpReadData(hRequest, (LPVOID)(tempBuffer + totalSize), dwSize, &dwDownloaded)) {
                            totalSize += dwDownloaded;
                        } else { bResults = FALSE; break; }
                    } else { bResults = FALSE; break; }
                }
            } while (dwSize > 0);

            if (bResults) {
                tempBuffer[totalSize] = '\0';
                if (InterlockedCompareExchange(pData->pSuccessFlag, 1, 0) == 0) {
                    pData->pResult->buffer = tempBuffer;
                    pData->pResult->size = totalSize;
                } else {
                    free(tempBuffer);
                }
            } else {
                free(tempBuffer);
            }
        } else {
            bResults = FALSE;
        }
    }

    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    return bResults ? 0 : 1;
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
    wcscpy_s(g_nid.szTip, sizeof(g_nid.szTip)/sizeof(wchar_t), L"蓝湾 未连接");

    if (!Shell_NotifyIconW(NIM_ADD, &g_nid)) {
        ShowTaskDialog(hwnd, L"错误", L"未能添加托盘图标！", NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON, NULL);
    }
}

void RemoveTrayIcon(HWND hwnd)
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

HRESULT ShowTaskDialog(HWND hwnd, const WCHAR* title, const WCHAR* mainInstruction, const WCHAR* content, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, PCWSTR pszIcon, int* pnButton)
{
    TASKDIALOGCONFIG tdc = { sizeof(tdc) };
    if (IsIconic(hwnd)) {
        tdc.hwndParent = g_hMenuOwnerWnd;
    } else {
        tdc.hwndParent = hwnd;
        tdc.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW;
    }
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
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | MF_DEFAULT, ID_TRAY_RESTORE, L"显示主窗口");
    InsertMenuW(hPopupMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    
    // Get list box item count from the main window
    HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
    LRESULT itemCount = 0;
    if (hListBox) {
        itemCount = SendMessageW(hListBox, LB_GETCOUNT, 0, 0);
    }

    // Determine the state for "顺序连接" based on traverse status, dial status and item count
    UINT connectFlags = MF_BYPOSITION | MF_STRING;
    if (g_traverseInProgress || g_dialInProgress || itemCount <= 0 || g_isRefreshing) {
        connectFlags |= MF_GRAYED;
    }
    InsertMenuW(hPopupMenu, 2, connectFlags, ID_TRAY_CONNECT, L"顺序连接");

    // Check connection status to enable/disable "Disconnect"
    BOOL isConnected = FALSE;
    RASCONNW conn[1];
    DWORD connSize = sizeof(conn);
    DWORD numConn = 0;
    conn[0].dwSize = sizeof(RASCONNW);
    if (RasEnumConnectionsW(conn, &connSize, &numConn) == SUCCESS && numConn > 0) {
        for (DWORD i = 0; i < numConn; i++) {
            if (wcscmp(conn[i].szEntryName, L"蓝湾") == 0) {
                isConnected = TRUE;
                break;
            }
        }
    }

    UINT disconnectFlags = MF_BYPOSITION | MF_STRING;
    if (!isConnected) {
        disconnectFlags |= MF_GRAYED;
    }
    InsertMenuW(hPopupMenu, 3, disconnectFlags, ID_TRAY_DISCONNECT, L"断开");
    InsertMenuW(hPopupMenu, 4, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(hPopupMenu, 5, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);

    // This is the crucial part to make the menu disappear correctly.
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
    
    // Post a null message to the helper window to help fix focus issues after the menu is closed.
    PostMessage(g_hMenuOwnerWnd, WM_NULL, 0, 0);

    DestroyMenu(hPopupMenu);

    if (command > 0)
    {
        // Post the command to the main application window to be handled.
        PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
    }
}

// Global variable for the user-specific phonebook path
WCHAR g_phonebookPath[MAX_PATH] = {0};

BOOL InitializePhonebookPath()
{
    if (g_phonebookPath[0] != L'\0') return TRUE;

    PWSTR pszPath = NULL;
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &pszPath);

    if (SUCCEEDED(hr))
    {
        swprintf_s(g_phonebookPath, MAX_PATH, L"%s\\Microsoft\\Network\\Connections\\Pbk\\rasphone.pbk", pszPath);
        CoTaskMemFree(pszPath);

        // Ensure the directory structure exists before trying to write the phonebook file.
        WCHAR dir[MAX_PATH];
        wcscpy_s(dir, MAX_PATH, g_phonebookPath);
        WCHAR* last_slash = wcsrchr(dir, L'\\');
        if (last_slash)
        {
            *last_slash = L'\0';
            SHCreateDirectoryExW(NULL, dir, NULL);
        }
        return TRUE;
    }
    return FALSE;
}

BOOL VpnEntryExists()
{
    RASENTRYW entry = {0};
    entry.dwSize = sizeof(RASENTRYW);
    DWORD dwEntrySize = sizeof(entry);
    DWORD rasResult = RasGetEntryPropertiesW(g_phonebookPath, L"蓝湾", &entry, &dwEntrySize, NULL, NULL);
    return (rasResult == SUCCESS);
}

BOOL CreateVpnEntry()
{
    DWORD rasResult;
    RASENTRYW entry = {0};
    entry.dwSize = sizeof(RASENTRYW);
    DWORD dwEntrySize = sizeof(entry);

    // --- Create Entry ---
    RasGetEntryPropertiesW(g_phonebookPath, L"", &entry, &dwEntrySize, NULL, NULL);
    
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

    rasResult = RasSetEntryPropertiesW(g_phonebookPath, L"蓝湾", &entry, entry.dwSize, NULL, 0);
    if (rasResult != SUCCESS) {
        return FALSE;
    }

    // --- Set Pre-Shared Key ---
    RASCREDENTIALSW pskCreds = {0};
    pskCreds.dwSize = sizeof(RASCREDENTIALSW);
    pskCreds.dwMask = RASCM_PreSharedKey;
    wcscpy_s(pskCreds.szPassword, PWLEN + 1, L"vpn");

    rasResult = RasSetCredentialsW(g_phonebookPath, L"蓝湾", &pskCreds, FALSE);
    if (rasResult != SUCCESS) {
        return FALSE;
    }
    
    return TRUE;
}


void DisconnectOnExit()
{
    RASCONNW conn[10];
    DWORD connSize = sizeof(conn);
    DWORD numConn = 0;
    conn[0].dwSize = sizeof(RASCONNW);

    if (RasEnumConnectionsW(conn, &connSize, &numConn) == SUCCESS && numConn > 0) {
        for (DWORD i = 0; i < numConn; i++) {
            if (wcscmp(conn[i].szEntryName, L"蓝湾") == 0) {
                RasHangUpW(conn[i].hrasconn);
                // Initiated hangup. The process will exit, and the OS will clean up the connection.
                // No need to wait here, which avoids blocking the exit process.
                break; // Assuming we only care about hanging up one.
            }
        }
    }
}

GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;
    const wchar_t HELPER_CLASS_NAME[] = L"LanWanMenuHelper";
    const wchar_t MUTEX_NAME[] = L"Global\\LanWanApp_{E1F495A0-69A7-4A8A-9963-4C78A3A585A1}";
    const wchar_t CREATE_VPN_ARG[] = L"init";

    if (!InitializePhonebookPath())
    {
        MessageBoxW(NULL, L"错误：无法获取用户电话簿路径。", L"初始化失败", MB_OK | MB_ICONERROR);
        return 1;
    }

    // If called with init, create the entry and exit.
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
        HWND hWnd = FindWindowW(L"#32770", L"蓝湾");
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

    HWND hwnd = CreateDialogW(hInstance, MAKEINTRESOURCE(IDD_MAIN_DIALOG), NULL, MainDlgProc);

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
    PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDC_BUTTON_REFRESH, 0), 0);

    // Run the message loop.
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessage(hwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    DisconnectOnExit();

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

    HWND hButton = GetDlgItem(hwnd, IDC_BUTTON_REFRESH);
    if (hButton) SendMessageW(hButton, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

    HWND hTraverseButton = GetDlgItem(hwnd, IDC_BUTTON_CONNECT);
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
    HWND hMainWindow = FindWindowW(L"#32770", L"蓝湾");
    if (!hMainWindow) return;
    HWND hStatusBar = GetDlgItem(hMainWindow, IDC_STATUSBAR);
    if (!hStatusBar) return;

    // If the new state is not RASCS_ConnectDevice, kill the specific timer for it.
    if (rascs != RASCS_ConnectDevice) {
        KillTimer(hMainWindow, IDT_CONNECTDEVICE_TIMEOUT);
    }

    // If we've reached a final state (connected or disconnected), or an error occurred, kill the timeout timer.
    // Also, if a dial is in progress, reset the flag and update the UI.
    if (rascs == RASCS_Connected || rascs == RASCS_Disconnected || dwError != 0) {
        KillTimer(hMainWindow, IDT_CONNECT_TIMEOUT);
        if (g_dialInProgress) {
            g_dialInProgress = FALSE;
            PostMessageW(hMainWindow, WM_APP_UPDATE_UI_BUTTON_STATES, 0, 0);
        }
    }

    if (dwError)
    {
        wchar_t errorMsg[256];
        wchar_t statusMsg[300];
        if (RasGetErrorStringW(dwError, errorMsg, 256) == 0)
        {
            swprintf_s(statusMsg, 300, L"连接失败: %s (%lu)", errorMsg, dwError);
        }
        else
        {
            swprintf_s(statusMsg, 100, L"连接失败: 未知错误 %lu", dwError);
        }
        SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)statusMsg);
        UpdateTrayIcon(FALSE);
        // If traverse is in progress, signal failure
        if (g_traverseInProgress && g_hTraverseConnectEvent) {
            g_traverseConnectSuccess = FALSE;
            SetEvent(g_hTraverseConnectEvent);
        }
        g_hRasConn = NULL; // Clear global handle on failure
        g_currentConnectingServerIp[0] = L'\0'; // Clear global IP
        g_connectedIp[0] = L'\0'; // Clear connected IP on failure
        return;
    }

    switch (rascs)
    {
        case RASCS_OpenPort: SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"正在打开端口..."); break;
        case RASCS_PortOpened: SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"端口已打开"); break;
        case RASCS_ConnectDevice: 
            SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"正在连接服务器...");
            SetTimer(hMainWindow, IDT_CONNECTDEVICE_TIMEOUT, 3000, NULL);
            break;
        case RASCS_DeviceConnected: SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"服务器已连接"); break;
        case RASCS_Authenticate: SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"正在验证身份..."); break;
        case RASCS_AuthNotify: SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"等待验证通知..."); break;
        case RASCS_Authenticated: SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"身份验证成功"); break;
        case RASCS_AuthProject: SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"正在配置网络参数..."); break;
        case RASCS_Connected: { // Use a block for local variable
            wchar_t statusText[256];
            swprintf_s(statusText, sizeof(statusText)/sizeof(wchar_t), L"已连接到 %s", g_currentConnectingServerIp);
            SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)statusText);
            UpdateTrayIcon(TRUE);

            // Store the successfully connected IP
            wcscpy_s(g_connectedIp, ARRAYSIZE(g_connectedIp), g_currentConnectingServerIp);

            // If traverse is in progress, signal success
            if (g_traverseInProgress && g_hTraverseConnectEvent) {
                g_traverseConnectSuccess = TRUE;
                wcscpy_s(g_lastSuccessfullyConnectedIp, ARRAYSIZE(g_lastSuccessfullyConnectedIp), g_currentConnectingServerIp); // Store connected IP
                SetEvent(g_hTraverseConnectEvent);
            }
            g_hRasConn = NULL; // Clear global handle on success
            g_currentConnectingServerIp[0] = L'\0'; // Clear global IP
            if (g_dialInProgress) { // If it was a single dial, reset flag and update UI
                g_dialInProgress = FALSE;
                PostMessageW(hMainWindow, WM_APP_UPDATE_UI_BUTTON_STATES, 0, 0);
            }
            break;
        }
        case RASCS_Disconnected: 
            SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"连接已断开");
            UpdateTrayIcon(FALSE);
            g_connectedIp[0] = L'\0'; // Clear connected IP on disconnect
            // If traverse is in progress and waiting, signal failure if not yet succeeded
            if (g_traverseInProgress && g_hTraverseConnectEvent && !g_traverseConnectSuccess) {
                SetEvent(g_hTraverseConnectEvent);
            }
            g_hRasConn = NULL; // Clear global handle on disconnect
            g_currentConnectingServerIp[0] = L'\0'; // Clear global IP
            if (g_dialInProgress) { // If it was a single dial, reset flag and update UI
                g_dialInProgress = FALSE;
                PostMessageW(hMainWindow, WM_APP_UPDATE_UI_BUTTON_STATES, 0, 0);
            }
            break;
        default: {
            wchar_t statusMsg[100];
            swprintf_s(statusMsg, 100, L"状态: %d", rascs);
            SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)statusMsg);
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

    // Set global flag and update UI
    g_dialInProgress = TRUE;
    // Post a message to the main thread to update UI, as direct UI manipulation from a worker thread is unsafe.
    PostMessageW(hwnd, WM_APP_UPDATE_UI_BUTTON_STATES, 0, 0);

    wchar_t* statusMsg;
    DWORD rasResult;

    // --- Check for and hang up existing connection ---
    RASCONNW conn[10];
    DWORD connSize = sizeof(conn);
    DWORD numConn = 0;
    // Per documentation, only the dwSize of the first element needs to be set.
    conn[0].dwSize = sizeof(RASCONNW);

    if (RasEnumConnectionsW(conn, &connSize, &numConn) == SUCCESS && numConn > 0) {
        for (DWORD i = 0; i < numConn; i++) {
            if (wcscmp(conn[i].szEntryName, L"蓝湾") == 0) {
                statusMsg = _wcsdup(L"正在断开当前连接...");
                if(statusMsg) PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);

                RasHangUpW(conn[i].hrasconn);
                while (TRUE)
                {
                    RASCONNSTATUSW st = {sizeof(st)};
                    if (RasGetConnectStatusW(conn[i].hrasconn, &st) != ERROR_SUCCESS)
                        break;
                    Sleep(100);
                }
                Sleep(1000);

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
    
    // Get the existing properties for "蓝湾"
    rasResult = RasGetEntryPropertiesW(g_phonebookPath, L"蓝湾", &entry, &dwEntrySize, NULL, NULL);
    if (rasResult != SUCCESS)
    {
        statusMsg = _wcsdup(L"错误：找不到'蓝湾'，无法连接。");
        if(statusMsg) PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);
        return 1;
    }

    // Update only the server address
    wcscpy_s(entry.szLocalPhoneNumber, RAS_MaxPhoneNumber + 1, serverIp);

    // Set the updated properties
    rasResult = RasSetEntryPropertiesW(g_phonebookPath, L"蓝湾", &entry, entry.dwSize, NULL, 0);
    if (rasResult != SUCCESS)
    {
        statusMsg = _wcsdup(L"更新服务器地址失败。");
        if(statusMsg) PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);
        return 1;
    }

    // --- Dial the connection ---
    statusMsg = _wcsdup(L"正在连接...");
    if(statusMsg) PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);


    RASDIALPARAMSW rasDialParams = {0};
    rasDialParams.dwSize = sizeof(RASDIALPARAMSW);
    wcscpy_s(rasDialParams.szEntryName, RAS_MaxEntryName + 1, L"蓝湾");
    // Supply credentials directly to the dialer for robustness.
    wcscpy_s(rasDialParams.szUserName, UNLEN + 1, L"vpn");
    wcscpy_s(rasDialParams.szPassword, PWLEN + 1, L"vpn");
    
    wcscpy_s(g_currentConnectingServerIp, ARRAYSIZE(g_currentConnectingServerIp), serverIp); // Store in global variable

    g_hRasConn = NULL; 
    rasResult = RasDialW(NULL, g_phonebookPath, &rasDialParams, 0, (RasDialFunc)RasDialCallback, &g_hRasConn); // No dwCallbackData needed

    if (rasResult == 0 || rasResult == ERROR_IO_PENDING)
    {
        //设置总超时为8秒
        SetTimer(hwnd, IDT_CONNECT_TIMEOUT, 8000, NULL);
    }
    else
    {
        wchar_t errorStr[256];
        wchar_t finalErrorMsg[512];
        if (RasGetErrorStringW(rasResult, errorStr, 256) == 0)
        {
            swprintf_s(finalErrorMsg, 512, L"连接失败: %s (%lu)", errorStr, rasResult);
        }
        // else
        // {
        //     swprintf_s(finalErrorMsg, 512, L"连接失败: 未知错误 %lu", rasResult);
        // }
        statusMsg = _wcsdup(finalErrorMsg);
        if (statusMsg)
            PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)statusMsg);

        g_hRasConn = NULL;
        return 1;
    }

    return 0;
}

// Helper function to update all relevant UI button states
void UpdateUiButtonStates(HWND hwnd)
{
    HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
    HWND hTraverseButton = GetDlgItem(hwnd, IDC_BUTTON_CONNECT);
    HWND hRefreshButton = GetDlgItem(hwnd, IDC_BUTTON_REFRESH); // Get refresh button handle

    if (!hListBox || !hTraverseButton || !hRefreshButton) return;

    // Disable if any connection is in progress (traverse or single dial)
    BOOL anyConnectionInProgress = g_traverseInProgress || g_dialInProgress;

    // ListBox: Disable if any connection is in progress
    EnableWindow(hListBox, !anyConnectionInProgress);

    LRESULT itemCount = SendMessageW(hListBox, LB_GETCOUNT, 0, 0);

    // Logic for IDC_BUTTON_CONNECT (Traverse button)
    // Disabled if any connection is in progress OR list is empty.
    if (anyConnectionInProgress || itemCount <= 0 || g_isRefreshing) {
        EnableWindow(hTraverseButton, FALSE);
    } else {
        EnableWindow(hTraverseButton, TRUE);
    }

    // Logic for IDC_BUTTON_REFRESH (Refresh button)
    // Disabled if any connection is in progress.
    if (anyConnectionInProgress || g_isRefreshing) {
        EnableWindow(hRefreshButton, FALSE);
    } else {
        EnableWindow(hRefreshButton, TRUE);
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
            SendMessageW(GetDlgItem(hwnd, IDC_STATUSBAR), SB_SETTEXTW, 2, (LPARAM)L"无法创建连接线程");
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

        wchar_t* buffer = (wchar_t*)calloc((size_t)len + 1, sizeof(wchar_t));
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
        wchar_t* statusMsg = (wchar_t*)calloc(512, sizeof(wchar_t));
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
        } else if (waitResult == WAIT_TIMEOUT) {
            // If the connection attempt times out, we must explicitly hang up before proceeding.
            if (g_hRasConn != NULL) {
                RasHangUpW(g_hRasConn);
                // Wait for the hang up to complete. The callback will signal g_hTraverseConnectEvent on disconnection.
                WaitForSingleObject(g_hTraverseConnectEvent, 3000); // 3-second timeout for hangup.
            }
        }
        // If waitResult is WAIT_OBJECT_0 and g_traverseConnectSuccess is FALSE, it means the connection failed quickly.
        // In that case, we just loop to the next server.

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
        wchar_t successText[512];
        swprintf_s(successText, ARRAYSIZE(successText), L"已连接到 %s", g_lastSuccessfullyConnectedIp);
        finalMsg = _wcsdup(successText);
    } else {
        finalMsg = _wcsdup(L"所有服务器均连接失败");
    }
    if (finalMsg) {
        PostMessageW(hwnd, WM_VPN_STATUS_UPDATE, 0, (LPARAM)finalMsg);
    }
    g_lastSuccessfullyConnectedIp[0] = L'\0'; // Clear the IP after use

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
    if (IsIconic(hwnd)) {
        tdc.hwndParent = g_hMenuOwnerWnd;
    } else {
        tdc.hwndParent = hwnd;
        tdc.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW;
    }
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
            sei.lpParameters = L"init";
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
    // If the dialog is already active, prevent showing another one
    if (g_isConfirmDisconnectDialogActive) {
        return FALSE;
    }

    // Check if a connection is already active
    BOOL isConnected = FALSE;
    RASCONNW conn[1];
    DWORD connSize = sizeof(conn);
    DWORD numConn = 0;
    conn[0].dwSize = sizeof(RASCONNW);

    if (RasEnumConnectionsW(conn, &connSize, &numConn) == SUCCESS && numConn > 0) {
        for (DWORD i = 0; i < numConn; i++) {
            if (wcscmp(conn[i].szEntryName, L"蓝湾") == 0) {
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
        if (IsIconic(hwnd)) {
            tdc.hwndParent = g_hMenuOwnerWnd;
        } else {
            tdc.hwndParent = hwnd;
            tdc.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW;
        }
        tdc.pButtons = aCustomButtons;
        tdc.cButtons = ARRAYSIZE(aCustomButtons);
        tdc.nDefaultButton = 1002; // Default to Cancel
        tdc.pszWindowTitle = L"确认";
        tdc.pszMainIcon = TD_WARNING_ICON;
        tdc.pszMainInstruction = L"该操作将断开当前的连接。";
        tdc.pszContent = L"是否继续？";

        // Set flag before showing the dialog
        g_isConfirmDisconnectDialogActive = TRUE;
        int nClickedButton = 0;
        HRESULT hr = TaskDialogIndirect(&tdc, &nClickedButton, NULL, NULL);
        // Reset flag after the dialog is closed
        g_isConfirmDisconnectDialogActive = FALSE;

        if (SUCCEEDED(hr) && nClickedButton == 1001) {
            return TRUE; // Continue was clicked
        } else {
            return FALSE; // Anything else (Cancel, close dialog, error) means stop
        }
    }

    return TRUE; // Proceed
}

static void UpdateStatusBarParts(HWND hwnd, UINT dpi)
{
    if (dpi == 0)
    {
        dpi = GetDpiForWindow(hwnd);
        if (dpi == 0) dpi = 96;
    }

    int paddingDIP = 6;

    // Part 1: Server count. Based on "99 台服务器"
    int textWidth1 = GetTextWidth(hStatusBar, hGuiFont, L"99 台服务器");
    int totalPaddingPX1 = DpiScale(paddingDIP * 2, dpi);
    int part1_width = textWidth1 + totalPaddingPX1;

    // Part 2: Refresh status. Based on "正在刷新..."
    int textWidth2 = GetTextWidth(hStatusBar, hGuiFont, L"正在刷新...");
    int totalPaddingPX2 = DpiScale(paddingDIP * 2, dpi);
    int part2_width = textWidth2 + totalPaddingPX2;

    int parts[3] = { part1_width, part1_width + part2_width, -1 };
    SendMessageW(hStatusBar, SB_SETPARTS, 3, (LPARAM)parts);
}

INT_PTR CALLBACK MainDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
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
            
            SetWindowPos(hwnd, NULL, x, y, window_width, window_height, SWP_NOZORDER);

            // Set window icons
            HICON hIcon = NULL;
            LoadIconWithScaleDown(g_hInstance, MAKEINTRESOURCE(MAINICON_ID), GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), &hIcon);
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            LoadIconWithScaleDown(g_hInstance, MAKEINTRESOURCE(MAINICON_ID), GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), &hIcon);
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            
            // Create the status bar.
            hStatusBar = CreateWindowExW(
                0,
                STATUSCLASSNAMEW,
                NULL,
                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                0, 0, 0, 0,
                hwnd,
                (HMENU)IDC_STATUSBAR,
                g_hInstance,
                NULL);
            
            UpdateFont(hwnd); // Moved here

            UpdateStatusBarParts(hwnd, 0);
            
            // Add a default server entry on initialization
            HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
            SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)L"14.132.22.67");
            SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)L"1.66.33.164");
            SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)L"219.100.37.1");

            // Update status bar for the default entry
            SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"3 台服务器");
            SendMessageW(hStatusBar, SB_SETTEXTW, 1, (LPARAM)L"");
            SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"");
            
            UpdateUiButtonStates(hwnd);
            AddTrayIcon(hwnd);

            // Perform initial layout to avoid flicker on startup
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            SendMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rcClient.right - rcClient.left, rcClient.bottom - rcClient.top));
        }
        return (INT_PTR)TRUE;

    case WM_DPICHANGED:
        {
            UpdateFont(hwnd);

            UINT dpi = HIWORD(wParam);

            UpdateStatusBarParts(hwnd, dpi);

            // The WM_DPICHANGED message recommends resizing the window based on the suggested rect.
            RECT* const prcNewWindow = (RECT*)lParam;
            SetWindowPos(hwnd,
                NULL,
                prcNewWindow->left,
                prcNewWindow->top,
                prcNewWindow->right - prcNewWindow->left,
                prcNewWindow->bottom - prcNewWindow->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            
            // Force a repaint of the window and its children to apply layout and font changes.
            RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
        break;

    case WM_CLOSE:
        g_wasMaximized = IsZoomed(hwnd);
        if (!g_wasMaximized)
        {
            GetWindowRect(hwnd, &g_rcOriginalWindowPos);
        }
        ShowWindow(hwnd, SW_HIDE);
        return (INT_PTR)TRUE;

    case WM_SIZE:
        {
            UINT dpi = GetDpiForWindow(hwnd); // Declare dpi once at the top
            if (dpi == 0) dpi = 96;

            // Resize the status bar.
            SendMessageW(hStatusBar, WM_SIZE, wParam, lParam);

            // Re-apply parts to handle resizing correctly.
            UpdateStatusBarParts(hwnd, dpi);

            // Get the height of the status bar.
            RECT rectStatusBar;
            GetWindowRect(hStatusBar, &rectStatusBar);
            int statusBarHeight = rectStatusBar.bottom - rectStatusBar.top;

            // Get the new dimensions of the client area.
            int clientWidth = LOWORD(lParam);
            int clientHeight = HIWORD(lParam);

            HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
            HWND hButton = GetDlgItem(hwnd, IDC_BUTTON_REFRESH);
            HWND hTraverseButton = GetDlgItem(hwnd, IDC_BUTTON_CONNECT);

            // Use DpiScale for consistency
            int padding = DpiScale(10, dpi);
            int buttonWidth = DpiScale(100, dpi);
            int buttonHeight = DpiScale(30, dpi);
            int buttonPadding = DpiScale(5, dpi);


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
                        wchar_t* buffer = (wchar_t*)calloc((size_t)len + 1, sizeof(wchar_t));
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

                            if (!isBlank)
                            {
                                SendMessageW(hListBox, LB_SETCURSEL, item_index, 0);
                                
                                HMENU hPopupMenu = CreatePopupMenu();
                                // If the selected IP is the one currently connected, show "Disconnect".
                                if (g_connectedIp[0] != L'\0' && wcscmp(buffer, g_connectedIp) == 0)
                                {
                                    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | MF_DEFAULT, ID_LISTBOX_DISCONNECT, L"断开");
                                }
                                else // Otherwise, show "Connect".
                                {
                                    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | MF_DEFAULT, ID_LISTBOX_CONNECT, L"连接");
                                }
                                InsertMenuW(hPopupMenu, 1, MF_BYPOSITION | MF_STRING, ID_LISTBOX_COPY, L"复制");

                                SetForegroundWindow(hwnd);
                                int command = TrackPopupMenu(hPopupMenu, TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RETURNCMD, pts.x, pts.y, 0, hwnd, NULL);
                                DestroyMenu(hPopupMenu);

                                if (command > 0)
                                {
                                    PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
                                }
                            }
                            free(buffer);
                        }
                    }
                }
            }
        }
        break;

    case WM_DESTROY:
        DestroyIcon(g_hIconDefault);
        DestroyIcon(g_hIconConnected);
        RemoveTrayIcon(hwnd);
        // Only delete the font if it was created
        if (hGuiFont)
        {
            DeleteObject(hGuiFont);
        }
        PostQuitMessage(0);
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDC_LISTBOX:
                    if (HIWORD(wParam) == LBN_DBLCLK)
                    {
                        if (!EnsureVpnEntryExists(hwnd)) break;

                        HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
                        
                        // Get the mouse position in screen coordinates and convert to client coordinates of the listbox.
                        POINT pt;
                        GetCursorPos(&pt);
                        ScreenToClient(hListBox, &pt);

                        // Find which item is at that point.
                        LRESULT item_index_result = SendMessageW(hListBox, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
                        
                        // HIWORD is non-zero if the point is outside the client rectangle.
                        // We also check if LB_GETCOUNT is 0, in which case LB_ITEMFROMPOINT result is not reliable.
                        int itemCount = (int)SendMessageW(hListBox, LB_GETCOUNT, 0, 0);
                        if (HIWORD(item_index_result) != 0 || itemCount == 0)
                        {
                            // Clicked outside the listbox client area or listbox is empty.
                            return (INT_PTR)TRUE;
                        }

                        int index = LOWORD(item_index_result);

                        // Check if the point is actually within the item's rectangle.
                        // LB_ITEMFROMPOINT returns the nearest item, even if the click is in whitespace below it.
                        RECT item_rect;
                        if (SendMessageW(hListBox, LB_GETITEMRECT, index, (LPARAM)&item_rect) == LB_ERR)
                        {
                            // Invalid index, should not happen but good to check.
                            return (INT_PTR)TRUE;
                        }

                        if (!PtInRect(&item_rect, pt))
                        {
                            // Click was in whitespace, not on an item.
                            return (INT_PTR)TRUE;
                        }

                        // At this point, we are sure the user double-clicked an actual item.
                        // The rest of the original logic can proceed.
                        int len = (int)SendMessageW(hListBox, LB_GETTEXTLEN, index, 0);
                        if (len > 0)
                        {
                            wchar_t* buffer = (wchar_t*)calloc((size_t)len + 1, sizeof(wchar_t));
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
                                    ConnectVpn(hwnd, buffer);
                                }
                                
                                free(buffer);
                            }
                        }
                    }
                    break;
                case IDC_BUTTON_REFRESH:
                {
                    if (g_isRefreshing)
                    {
                        break;
                    }
                    g_isRefreshing = TRUE;
                    
                    UpdateUiButtonStates(hwnd); // Disable buttons
                    SendMessageW(hStatusBar, SB_SETTEXTW, 1, (LPARAM)L"正在刷新...");

                    MASTER_DOWNLOAD_THREAD_DATA* pData = (MASTER_DOWNLOAD_THREAD_DATA*)malloc(sizeof(MASTER_DOWNLOAD_THREAD_DATA));
                    if (pData)
                    {
                        pData->hwnd = hwnd;
                        HANDLE hThread = CreateThread(NULL, 0, MasterDownloadThreadProc, pData, 0, NULL);
                        if (hThread)
                        {
                            CloseHandle(hThread);
                        }
                        else
                        {
                            free(pData);
                            g_isRefreshing = FALSE;
                            SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"无法创建线程");
                            UpdateUiButtonStates(hwnd);
                        }
                    } else {
                        g_isRefreshing = FALSE;
                        SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"内存分配失败");
                        UpdateUiButtonStates(hwnd);
                    }
                    break;
                }
                case ID_TRAY_CONNECT:
                case IDC_BUTTON_CONNECT:
                {
                    if (!EnsureVpnEntryExists(hwnd)) break;

                    if (g_traverseInProgress) {
                        SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"正在顺序连接...");
                        break;
                    }
                    
                    if (!CheckAndConfirmDisconnect(hwnd)) {
                        break; // User chose not to continue
                    }

                    SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"开始顺序连接...");

                    g_traverseInProgress = TRUE;
                    UpdateUiButtonStates(hwnd);

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
                            g_traverseInProgress = FALSE; // Set flag first
                            UpdateUiButtonStates(hwnd); // Then update UI
                            SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"无法创建顺序连接线程");
                        }
                    }
                    else // This is also a problematic else block
                    {
                        g_traverseInProgress = FALSE; // Set flag first
                        UpdateUiButtonStates(hwnd); // Then update UI
                        SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"内存分配失败");
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
                case ID_LISTBOX_DISCONNECT:
                case ID_TRAY_DISCONNECT:
                    {
                        RASCONNW conn[10];
                        DWORD connSize = sizeof(conn);
                        DWORD numConn = 0;
                        conn[0].dwSize = sizeof(RASCONNW);

                        if (RasEnumConnectionsW(conn, &connSize, &numConn) == SUCCESS && numConn > 0) {
                            for (DWORD i = 0; i < numConn; i++) {
                                if (wcscmp(conn[i].szEntryName, L"蓝湾") == 0) {
                                    RasHangUpW(conn[i].hrasconn);
                                    break;
                                }
                            }
                        }
                        
                        // Update UI immediately after initiating hangup.
                        SendMessageW(GetDlgItem(hwnd, IDC_STATUSBAR), SB_SETTEXTW, 2, (LPARAM)L"连接已断开");
                        UpdateTrayIcon(FALSE);
                        g_connectedIp[0] = L'\0'; // Immediately clear the connected IP
                    }
                    break;
                // "测试" menu and test button removed
                case ID_LISTBOX_CONNECT:
                    {
                        if (!EnsureVpnEntryExists(hwnd)) break;

                        HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
                        int index = (int)SendMessageW(hListBox, LB_GETCURSEL, 0, 0); // Already selected by right-click
                        if (index != LB_ERR)
                        {
                            int len = (int)SendMessageW(hListBox, LB_GETTEXTLEN, index, 0);
                            if (len > 0)
                            {
                                wchar_t* buffer = (wchar_t*)calloc((size_t)len + 1, sizeof(wchar_t));
                                if (buffer)
                                {
                                    SendMessageW(hListBox, LB_GETTEXT, index, (LPARAM)buffer);
                                    // As per user request, "confirm disconnect" dialog is not shown for single connect from listbox.
                                    ConnectVpn(hwnd, buffer); 
                                    free(buffer);
                                }
                            }
                        }
                    }
                    break;
                case ID_LISTBOX_COPY:
                    {
                        HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
                        int index = (int)SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
                        if (index != LB_ERR)
                        {
                            int len = (int)SendMessageW(hListBox, LB_GETTEXTLEN, index, 0);
                            if (len > 0)
                            {
                                wchar_t* buffer = (wchar_t*)calloc((size_t)len + 1, sizeof(wchar_t));
                                if (buffer)
                                {
                                    SendMessageW(hListBox, LB_GETTEXT, index, (LPARAM)buffer);
                                    
                                    if (OpenClipboard(hwnd))
                                    {
                                        EmptyClipboard();
                                        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
                                        if (hg)
                                        {
                                            wchar_t* p = (wchar_t*)GlobalLock(hg);
                                            if (p)
                                            {
                                                wcscpy_s(p, len + 1, buffer);
                                                GlobalUnlock(hg);
                                                SetClipboardData(CF_UNICODETEXT, hg);
                                            }
                                        }
                                        CloseClipboard();
                                    }
                                    
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
        HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
        wchar_t* selectedText = NULL;

        // Guard against null data
        if (!pData) {
            SendMessageW(hStatusBar, SB_SETTEXTW, 1, (LPARAM)L"刷新失败 (内部错误)");
            g_isRefreshing = FALSE;
            UpdateUiButtonStates(hwnd);
            break;
        }

        // --- 1. Preserve selection safely ---
        int selectedIndex = (int)SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
        if (selectedIndex != LB_ERR)
        {
            int textLen = (int)SendMessageW(hListBox, LB_GETTEXTLEN, selectedIndex, 0);
            if (textLen > 0 && textLen != LB_ERR)
            {
                selectedText = (wchar_t*)calloc((size_t)textLen + 1, sizeof(wchar_t));
                if (selectedText)
                {
                    // Ensure buffer is null-terminated in case of error
                    selectedText[0] = L'\0';
                    SendMessageW(hListBox, LB_GETTEXT, selectedIndex, (LPARAM)selectedText);
                }
            }
        }

        // --- 2. Always clear the listbox before populating ---
        SendMessageW(hListBox, LB_RESETCONTENT, 0, 0);

        // --- 3. Process downloaded data and populate listbox ---
        if (pData->buffer && pData->size > 0)
        {
            int wideCharCount = MultiByteToWideChar(CP_UTF8, 0, pData->buffer, pData->size, NULL, 0);
            if (wideCharCount > 0)
            {
                wchar_t* pwszData = (wchar_t*)calloc((size_t)wideCharCount + 1, sizeof(wchar_t));
                if (pwszData)
                {
                    MultiByteToWideChar(CP_UTF8, 0, pData->buffer, pData->size, pwszData, wideCharCount);
                    pwszData[wideCharCount] = L'\0'; // Null-terminate

                    wchar_t* context = NULL;
                    wchar_t* line = wcstok_s(pwszData, L"\n", &context);
                    while (line != NULL)
                    {
                        size_t len = wcslen(line);
                        if (len > 0 && line[len-1] == L'\r') { 
                            line[len-1] = L'\0';
                        }
                        // Do not add empty lines
                        if (wcslen(line) > 0) {
                            SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)line);
                        }
                        line = wcstok_s(NULL, L"\n", &context);
                    }
                    free(pwszData);
                }
            }
        }
        
        // --- 4. Free downloaded data ---
        free(pData->buffer);
        free(pData);

        // --- 5. Restore selection safely ---
        if (selectedText)
        {
            if (selectedText[0] != L'\0')
            {
                int newIndex = (int)SendMessageW(hListBox, LB_FINDSTRINGEXACT, -1, (LPARAM)selectedText);
                if (newIndex != LB_ERR)
                {
                    SendMessageW(hListBox, LB_SETCURSEL, newIndex, 0);
                }
            }
            free(selectedText);
        }

        // --- 6. ALWAYS update status and UI state ---
        LRESULT serverCount = SendMessageW(hListBox, LB_GETCOUNT, 0, 0);
        wchar_t serverCountText[50];
        swprintf_s(serverCountText, sizeof(serverCountText)/sizeof(wchar_t), L"%lld 台服务器", serverCount);
        SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)serverCountText);
        SendMessageW(hStatusBar, SB_SETTEXTW, 1, (LPARAM)L"刷新成功");
        
        g_isRefreshing = FALSE;
        UpdateUiButtonStates(hwnd);
        break;
    }

    case WM_DOWNLOAD_FAILURE:
    {
        SendMessageW(hStatusBar, SB_SETTEXTW, 1, (LPARAM)L"刷新失败");
        g_isRefreshing = FALSE;
        // 如果列表框中仍有数据，则允许用户点击“顺序连接”
        UpdateUiButtonStates(hwnd);
        break;
    }

    case WM_VPN_STATUS_UPDATE:
    {
        wchar_t* statusMsg = (wchar_t*)lParam;
        if (statusMsg)
        {
            SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)statusMsg);
            free(statusMsg);
        }
        break;
    }

    case WM_TRAVERSE_COMPLETE:
        {
            g_traverseInProgress = FALSE;
            UpdateUiButtonStates(hwnd);
            break;
        }

    case WM_APP_UPDATE_UI_BUTTON_STATES:
        {
            UpdateUiButtonStates(hwnd);
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
                SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"连接超时");
                RasHangUpW(g_hRasConn);
                g_hRasConn = NULL; // Clear the handle
            }
        }
        else if (wParam == IDT_CONNECTDEVICE_TIMEOUT)
        {
            KillTimer(hwnd, IDT_CONNECTDEVICE_TIMEOUT);
            if (g_hRasConn != NULL)
            {
                SendMessageW(hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"连接服务器超时");
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
        return (INT_PTR)TRUE;
    
    case WM_APP_SHOW:
        RestoreWindow(hwnd);
        break;
    
    case WM_APP_TRAYMSG:
        switch (lParam) // For legacy notifications, the message is directly in lParam
        {
            case WM_LBUTTONDBLCLK:
                PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(ID_TRAY_RESTORE, 0), 0);
                break;
            case WM_RBUTTONUP:
                ShowTrayContextMenu(hwnd);
                break;
        }
        return (INT_PTR)TRUE;

    }
    return (INT_PTR)FALSE;
}
