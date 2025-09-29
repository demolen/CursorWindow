# OpenInCursorDisplay

A tiny background Windows (Win32) tray app that automatically moves the active window to the monitor where your mouse cursor is.

- Runs in the background with a tray icon
- Auto action: when the foreground window changes, it is moved to the cursor monitor
- Tray menu: toggle auto-move, taskbar selection, start with Windows, exit

## Build

You can build with either Microsoft Visual C++ (MSVC) or MinGW (g++).

### Custom icon

- Place your `.ico` file in this folder and name it `app.ico` (or edit `app.rc` to point to your file).
- The app embeds this icon into the EXE and uses it for the tray.

### MSVC (Visual Studio Developer Command Prompt)

1) Open a "x64 Native Tools Command Prompt for VS" (or any Developer Command Prompt).
2) Navigate to this folder and run:

```
rc /nologo /fo app.res app.rc
cl /EHsc /W4 /O2 open_in_cursor_display.cpp app.res user32.lib shell32.lib gdi32.lib advapi32.lib
```

This produces `open_in_cursor_display.exe` in the same directory.

### MinGW (MSYS2)

1) Install MSYS2 and the MinGW toolchain (e.g., `pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-binutils`).
2) Open the appropriate MinGW shell, then run:

```
windres -i app.rc -o app.res

g++ -municode -O2 -std=c++17 -Wall -Wextra -o OpenInCursorDisplay.exe open_in_cursor_display.cpp app.res -luser32 -lshell32 -lgdi32 -ladvapi32
```

If your MinGW environment doesn’t have `windres` on PATH, try `x86_64-w64-mingw32-windres` instead of `windres`.

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
