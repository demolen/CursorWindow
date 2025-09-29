// OpenInCursorDisplay - moves the active window to the monitor under the mouse cursor
// Runs as a background tray app
// Build (MSVC):
//   cl /EHsc /W4 /O2 /DUNICODE /D_UNICODE open_in_cursor_display.cpp user32.lib shell32.lib gdi32.lib
// Build (MinGW):
//   g++ -municode -O2 -std=c++17 -Wall -Wextra -o OpenInCursorDisplay.exe open_in_cursor_display.cpp -luser32 -lshell32 -lgdi32

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>

#ifndef ARRAYSIZE
#define ARRAYSIZE(A) (sizeof(A) / sizeof((A)[0]))
#endif

static const wchar_t kAppClassName[] = L"OpenInCursorDisplayWindow";
static const wchar_t kAppDisplayName[] = L"OpenInCursorDisplay";
static const UINT WMAPP_NOTIFYICON = WM_APP + 1;
static const UINT kTrayIconId = 1;

static const UINT IDM_TOGGLE = 1001;
static const UINT IDM_MOVE_NOW = 1002;
static const UINT IDM_EXIT = 1003;

HINSTANCE g_hInst = nullptr;
HWINEVENTHOOK g_hWinEventHook = nullptr;
BOOL g_enabled = TRUE;
UINT g_uTaskbarRestart = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);
void AddTrayIcon(HWND hwnd);
void RemoveTrayIcon(HWND hwnd);
void UpdateTrayTooltip(HWND hwnd);
void ShowContextMenu(HWND hwnd);
void EnsureHook();
void RemoveHook();
void MoveWindowToCursorDisplay(HWND hwnd);
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
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
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
        case IDM_MOVE_NOW:
            MoveWindowToCursorDisplay(GetForegroundWindow());
            break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;

    case WM_DESTROY:
        RemoveHook();
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
    nid.hIcon = (HICON)LoadImageW(nullptr, IDI_APPLICATION, IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    if (!nid.hIcon) nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);

    wchar_t tip[128];
    wsprintfW(tip, L"%s (%s)", kAppDisplayName, g_enabled ? L"Enabled" : L"Paused");
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
    wsprintfW(tip, L"%s (%s)", kAppDisplayName, g_enabled ? L"Enabled" : L"Paused");
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
    AppendMenuW(hMenu, toggleFlags, IDM_TOGGLE, g_enabled ? L"Auto-move (Enabled)" : L"Auto-move (Paused)");
    AppendMenuW(hMenu, MF_STRING, IDM_MOVE_NOW, L"Move foreground window now");
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

void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
{
    if (event != EVENT_SYSTEM_FOREGROUND) return;
    if (idObject != OBJID_WINDOW) return;
    MoveWindowToCursorDisplay(hwnd);
}
