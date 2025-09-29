# OpenInCursorDisplay

A tiny background Windows (Win32) tray app that automatically moves the active window to the monitor where your mouse cursor is.

- Runs in the background with a tray icon
- Auto action: when the foreground window changes, it is moved to the cursor monitor
- Tray menu: toggle auto-move, move-now, exit

## Build

You can build with either Microsoft Visual C++ (MSVC) or MinGW (g++).

### MSVC (Visual Studio Developer Command Prompt)

1) Open a "x64 Native Tools Command Prompt for VS" (or any Developer Command Prompt).
2) Navigate to this folder and run:

```
cl /EHsc /W4 /O2 /DUNICODE /D_UNICODE open_in_cursor_display.cpp user32.lib shell32.lib gdi32.lib
```

This produces `open_in_cursor_display.exe` in the same directory.

### MinGW (MSYS2)

1) Install MSYS2 and the MinGW toolchain (e.g., `pacman -S mingw-w64-ucrt-x86_64-gcc`).
2) Open the appropriate MinGW shell, then run:

```
g++ -municode -O2 -std=c++17 -Wall -Wextra -o OpenInCursorDisplay.exe open_in_cursor_display.cpp -luser32 -lshell32 -lgdi32
```

## Usage

- Run the compiled executable. It hides itself and shows a tray icon.
- Actions:
  - Left-click the tray icon: immediately move the current foreground window to the cursor monitor.
  - Right-click the tray icon: open the context menu.
    - Auto-move (Enabled/Paused): toggles whether the app moves windows automatically when foreground changes.
    - Move foreground window now: manual move.
    - Exit: quit the app.

Windows with special behavior (e.g., minimized windows, tool windows, UWP or system windows) may be skipped or behave differently. Maximized windows are restored, moved, then re-maximized to switch monitors cleanly.

## Start with Windows (optional)

- Quick method: place a shortcut to the EXE in `shell:startup` (Win+R → type `shell:startup`).

## Uninstall

- Just delete the EXE. There are no registry writes; the app does not install any services.
