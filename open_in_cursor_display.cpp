// OpenInCursorDisplay - moves the active window to the monitor under the mouse cursor
// Runs as a background tray app
// Build (MSVC):
//   cl /EHsc /W4 /O2 /DUNICODE /D_UNICODE open_in_cursor_display.cpp user32.lib shell32.lib gdi32.lib advapi32.lib
// Build (MinGW):
//   g++ -municode -O2 -std=c++17 -Wall -Wextra -o OpenInCursorDisplay.exe open_in_cursor_display.cpp -luser32 -lshell32 -lgdi32 -ladvapi32

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include "resource.h"

#ifndef ARRAYSIZE
#define ARRAYSIZE(A) (sizeof(A) / sizeof((A)[0]))
#endif

static const wchar_t kAppClassName[] = L"ChaseTheCursorWindow";
static const wchar_t kAppDisplayName[] = L"ChaseTheCursor";
static const UINT WMAPP_NOTIFYICON = WM_APP + 1;
static const UINT kTrayIconId = 1;

static const UINT IDM_TOGGLE = 1001;
static const UINT IDM_STARTUP_TOGGLE = 1002;
static const UINT IDM_TASKBAR_TOGGLE = 1003;
static const UINT IDM_EXIT = 1004;

HINSTANCE g_hInst = nullptr;
HWINEVENTHOOK g_hWinEventHook = nullptr;
HWINEVENTHOOK g_hTaskbarHook = nullptr;
BOOL g_enabled = TRUE;
BOOL g_taskbarEnabled = TRUE;
BOOL g_startupEnabled = TRUE;
UINT g_uTaskbarRestart = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);
void AddTrayIcon(HWND hwnd);
void RemoveTrayIcon(HWND hwnd);
void UpdateTrayTooltip(HWND hwnd);
void ShowContextMenu(HWND hwnd);
void EnsureHook();
void RemoveHook();
void EnsureTaskbarHook();
void RemoveTaskbarHook();
void LoadOrInitStartupPreference();
void SaveStartupPreference();
void ApplyStartupRunEntry(BOOL enabled);
void MoveWindowToCursorDisplay(HWND hwnd);
void MoveWindowToCursorDisplaySmart(HWND hwnd);
BOOL IsInterestingWindow(HWND hwnd);

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    g_hInst = hInstance;

    // Basic DPI awareness for sensible coordinates
    // (system-DPI aware is enough for this use case)
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        typedef BOOL (WINAPI *SetProcessDPIAware_t)();
        auto pSetProcessDPIAware = reinterpret_cast<SetProcessDPIAware_t>(GetProcAddress(user32, "SetProcessDPIAware"));
        if (pSetProcessDPIAware) pSetProcessDPIAware();
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kAppClassName;
    wc.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_APPICON));
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc)) {
        MessageBoxW(nullptr, L"Failed to register window class", kAppDisplayName, MB_ICONERROR | MB_OK);
        return 1;
    }

    // Use WS_EX_TOOLWINDOW to keep it out of Alt-Tab
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kAppClassName, kAppDisplayName,
                                WS_OVERLAPPED, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) {
        MessageBoxW(nullptr, L"Failed to create hidden window", kAppDisplayName, MB_ICONERROR | MB_OK);
        return 1;
    }

    // Keep hidden
    ShowWindow(hwnd, SW_HIDE);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        g_uTaskbarRestart = RegisterWindowMessageW(L"TaskbarCreated");
        AddTrayIcon(hwnd);
        EnsureHook();
        EnsureTaskbarHook();
        LoadOrInitStartupPreference();
        UpdateTrayTooltip(hwnd);
        return 0;

    case WMAPP_NOTIFYICON:
        switch (LOWORD(lParam)) {
        case WM_CONTEXTMENU:
        case WM_RBUTTONUP:
            ShowContextMenu(hwnd);
            break;
        case WM_LBUTTONUP:
            // Quick action: move current foreground window now
            MoveWindowToCursorDisplay(GetForegroundWindow());
            break;
        case WM_LBUTTONDBLCLK:
            // Toggle enabled state
            g_enabled = !g_enabled;
            if (g_enabled) EnsureHook(); else RemoveHook();
            UpdateTrayTooltip(hwnd);
            break;
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_TOGGLE:
            g_enabled = !g_enabled;
            if (g_enabled) EnsureHook(); else RemoveHook();
            UpdateTrayTooltip(hwnd);
            break;
        case IDM_TASKBAR_TOGGLE:
            g_taskbarEnabled = !g_taskbarEnabled;
            if (g_taskbarEnabled) EnsureTaskbarHook(); else RemoveTaskbarHook();
            break;
        case IDM_STARTUP_TOGGLE:
            g_startupEnabled = !g_startupEnabled;
            SaveStartupPreference();
            ApplyStartupRunEntry(g_startupEnabled);
            UpdateTrayTooltip(hwnd);
            break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;

    case WM_DESTROY:
        RemoveHook();
        RemoveTaskbarHook();
        RemoveTrayIcon(hwnd);
        PostQuitMessage(0);
        return 0;

    default:
        if (uMsg == g_uTaskbarRestart) {
            // Explorer restarted, re-add tray icon
            AddTrayIcon(hwnd);
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void AddTrayIcon(HWND hwnd)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WMAPP_NOTIFYICON;
    nid.hIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    if (!nid.hIcon) nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);

    wchar_t tip[128];
    wsprintfW(tip, L"%s (%s%s%s)", kAppDisplayName, 
              g_enabled ? L"Auto" : L"Paused",
              g_taskbarEnabled ? L", Taskbar" : L"",
              g_startupEnabled ? L", Startup" : L"");
    lstrcpynW(nid.szTip, tip, ARRAYSIZE(nid.szTip));

    Shell_NotifyIconW(NIM_ADD, &nid);

    // Use the most recent behavior
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);
}

void RemoveTrayIcon(HWND hwnd)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void UpdateTrayTooltip(HWND hwnd)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_TIP;

    wchar_t tip[128];
    wsprintfW(tip, L"%s (%s%s%s)", kAppDisplayName, 
              g_enabled ? L"Auto" : L"Paused",
              g_taskbarEnabled ? L", Taskbar" : L"",
              g_startupEnabled ? L", Startup" : L"");
    lstrcpynW(nid.szTip, tip, ARRAYSIZE(nid.szTip));

    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void ShowContextMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    UINT toggleFlags = MF_STRING | (g_enabled ? MF_CHECKED : 0);
    UINT taskbarFlags = MF_STRING | (g_taskbarEnabled ? MF_CHECKED : 0);
    UINT startupFlags = MF_STRING | (g_startupEnabled ? MF_CHECKED : 0);
    AppendMenuW(hMenu, toggleFlags, IDM_TOGGLE, L"Auto move active window to mouse display");
    AppendMenuW(hMenu, taskbarFlags, IDM_TASKBAR_TOGGLE, L"Taskbar selection moves to mouse display");
    AppendMenuW(hMenu, startupFlags, IDM_STARTUP_TOGGLE, L"Start with Windows");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

    SetForegroundWindow(hwnd);
    TrackPopupMenuEx(hMenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);
    DestroyMenu(hMenu);
}

void EnsureHook()
{
    if (g_hWinEventHook) return;
    g_hWinEventHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        nullptr,
        WinEventProc,
        0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
}

void RemoveHook()
{
    if (g_hWinEventHook) {
        UnhookWinEvent(g_hWinEventHook);
        g_hWinEventHook = nullptr;
    }
}

void EnsureTaskbarHook()
{
    if (g_hTaskbarHook) return;
    g_hTaskbarHook = SetWinEventHook(
        EVENT_OBJECT_FOCUS,
        EVENT_OBJECT_FOCUS,
        nullptr,
        WinEventProc,
        0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
}

void RemoveTaskbarHook()
{
    if (g_hTaskbarHook) {
        UnhookWinEvent(g_hTaskbarHook);
        g_hTaskbarHook = nullptr;
    }
}

void ApplyStartupRunEntry(BOOL enabled)
{
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0,
                        KEY_SET_VALUE | KEY_QUERY_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        if (enabled) {
            wchar_t path[MAX_PATH]{};
            if (GetModuleFileNameW(nullptr, path, ARRAYSIZE(path)) > 0) {
                wchar_t value[1024];
                wsprintfW(value, L"\"%s\"", path);
                RegSetValueExW(hKey, kAppDisplayName, 0, REG_SZ, reinterpret_cast<const BYTE*>(value),
                               (lstrlenW(value) + 1) * sizeof(wchar_t));
            }
        } else {
            RegDeleteValueW(hKey, kAppDisplayName);
        }
        RegCloseKey(hKey);
    }
}

void LoadOrInitStartupPreference()
{
    HKEY hKey;
if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\ChaseTheCursor", 0, nullptr, 0,
                        KEY_READ | KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD val = 0, type = 0, size = sizeof(val);
        if (RegQueryValueExW(hKey, L"StartupEnabled", nullptr, &type, reinterpret_cast<BYTE*>(&val), &size) == ERROR_SUCCESS && type == REG_DWORD) {
            g_startupEnabled = (val != 0);
        } else {
            g_startupEnabled = TRUE; // default ON first run
            val = 1;
            RegSetValueExW(hKey, L"StartupEnabled", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        }
        RegCloseKey(hKey);
    } else {
        g_startupEnabled = TRUE;
    }
    ApplyStartupRunEntry(g_startupEnabled);
}

void SaveStartupPreference()
{
    HKEY hKey;
if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\ChaseTheCursor", 0, nullptr, 0,
                        KEY_READ | KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD val = g_startupEnabled ? 1 : 0;
        RegSetValueExW(hKey, L"StartupEnabled", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        RegCloseKey(hKey);
    }
}

BOOL IsInterestingWindow(HWND hwnd)
{
    if (!hwnd || hwnd == GetShellWindow()) return FALSE;

    // Foreground should always be a top-level window, but double-check
    HWND root = GetAncestor(hwnd, GA_ROOT);
    if (root != hwnd) hwnd = root;

    if (!IsWindowVisible(hwnd)) return FALSE;

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return FALSE; // skip tool windows / palette windows

    // Skip the taskbar and tray
    wchar_t className[128]{};
    GetClassNameW(hwnd, className, ARRAYSIZE(className));
    if (lstrcmpiW(className, L"Shell_TrayWnd") == 0) return FALSE;

    return TRUE;
}

void MoveWindowToCursorDisplay(HWND hwnd)
{
    if (!g_enabled) return;
    if (!IsInterestingWindow(hwnd)) return;

    // Identify monitors for the window and the current cursor
    HMONITOR monWindow = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (!monWindow) return;

    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monCursor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    if (monCursor == monWindow) return; // already on the cursor monitor

    WINDOWPLACEMENT wp{}; wp.length = sizeof(wp);
    if (!GetWindowPlacement(hwnd, &wp)) return;

    if (wp.showCmd == SW_SHOWMINIMIZED) {
        // Skip minimized windows
        return;
    }

    RECT wr{};
    if (!GetWindowRect(hwnd, &wr)) return;

    MONITORINFO miSrc{}; miSrc.cbSize = sizeof(miSrc);
    MONITORINFO miDst{}; miDst.cbSize = sizeof(miDst);
    if (!GetMonitorInfoW(monWindow, &miSrc)) return;
    if (!GetMonitorInfoW(monCursor, &miDst)) return;

    const int width = wr.right - wr.left;
    const int height = wr.bottom - wr.top;

    // Compute offset relative to source work area
    int dx = wr.left - miSrc.rcWork.left;
    int dy = wr.top - miSrc.rcWork.top;

    int newLeft = miDst.rcWork.left + dx;
    int newTop = miDst.rcWork.top + dy;

    // Clamp to destination work area
    if (newLeft + width > miDst.rcWork.right) newLeft = miDst.rcWork.right - width;
    if (newTop + height > miDst.rcWork.bottom) newTop = miDst.rcWork.bottom - height;
    if (newLeft < miDst.rcWork.left) newLeft = miDst.rcWork.left;
    if (newTop < miDst.rcWork.top) newTop = miDst.rcWork.top;

    BOOL wasMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
    if (wasMaximized) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    SetWindowPos(hwnd, nullptr, newLeft, newTop, width, height, SWP_NOZORDER | SWP_NOACTIVATE);

    if (wasMaximized) {
        ShowWindow(hwnd, SW_MAXIMIZE);
    }
}

void MoveWindowToCursorDisplaySmart(HWND hwnd)
{
    if (!IsInterestingWindow(hwnd)) return;

    // Identify monitors for the window and the current cursor
    HMONITOR monWindow = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (!monWindow) return;

    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monCursor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    if (monCursor == monWindow) return; // already on the cursor monitor

    WINDOWPLACEMENT wp{}; wp.length = sizeof(wp);
    if (!GetWindowPlacement(hwnd, &wp)) return;

    if (wp.showCmd == SW_SHOWMINIMIZED) {
        // Skip minimized windows
        return;
    }

    RECT wr{};
    if (!GetWindowRect(hwnd, &wr)) return;

    MONITORINFO miSrc{}; miSrc.cbSize = sizeof(miSrc);
    MONITORINFO miDst{}; miDst.cbSize = sizeof(miDst);
    if (!GetMonitorInfoW(monWindow, &miSrc)) return;
    if (!GetMonitorInfoW(monCursor, &miDst)) return;

    BOOL wasMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
    
    int newWidth, newHeight;
    int newLeft, newTop;
    
    if (wasMaximized) {
        // For maximized windows, fit to the destination monitor's work area
        ShowWindow(hwnd, SW_RESTORE);
        newWidth = miDst.rcWork.right - miDst.rcWork.left;
        newHeight = miDst.rcWork.bottom - miDst.rcWork.top;
        newLeft = miDst.rcWork.left;
        newTop = miDst.rcWork.top;
    } else {
        // For normal windows, preserve size but constrain to destination monitor
        const int width = wr.right - wr.left;
        const int height = wr.bottom - wr.top;
        
        // Calculate max size that fits in destination monitor
        const int maxWidth = miDst.rcWork.right - miDst.rcWork.left;
        const int maxHeight = miDst.rcWork.bottom - miDst.rcWork.top;
        
        newWidth = min(width, maxWidth);
        newHeight = min(height, maxHeight);
        
        // Compute offset relative to source work area
        int dx = wr.left - miSrc.rcWork.left;
        int dy = wr.top - miSrc.rcWork.top;
        
        newLeft = miDst.rcWork.left + dx;
        newTop = miDst.rcWork.top + dy;
        
        // Clamp to destination work area
        if (newLeft + newWidth > miDst.rcWork.right) newLeft = miDst.rcWork.right - newWidth;
        if (newTop + newHeight > miDst.rcWork.bottom) newTop = miDst.rcWork.bottom - newHeight;
        if (newLeft < miDst.rcWork.left) newLeft = miDst.rcWork.left;
        if (newTop < miDst.rcWork.top) newTop = miDst.rcWork.top;
    }

    SetWindowPos(hwnd, nullptr, newLeft, newTop, newWidth, newHeight, SWP_NOZORDER | SWP_NOACTIVATE);

    if (wasMaximized) {
        ShowWindow(hwnd, SW_MAXIMIZE);
    }
}

void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
{
    if (event == EVENT_SYSTEM_FOREGROUND) {
        if (idObject != OBJID_WINDOW) return;
        if (hWinEventHook == g_hWinEventHook) {
            MoveWindowToCursorDisplay(hwnd);
        }
    } else if (event == EVENT_OBJECT_FOCUS) {
        if (hWinEventHook == g_hTaskbarHook && g_taskbarEnabled) {
            // Check if focus is on a taskbar button
            wchar_t className[128]{};
            if (GetClassNameW(hwnd, className, ARRAYSIZE(className))) {
                if (lstrcmpiW(className, L"MSTaskListWClass") == 0 || 
                    lstrcmpiW(className, L"TaskListThumbnailWnd") == 0 ||
                    wcsstr(className, L"TaskBar") != nullptr) {
                    // Small delay to let the window become foreground
                    Sleep(50);
                    HWND foreground = GetForegroundWindow();
                    if (foreground) {
                        MoveWindowToCursorDisplaySmart(foreground);
                    }
                }
            }
        }
    }
}
