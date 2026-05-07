# LVGL Motion Demo

LVGL simulator demo for a 960x400 task-flow screen. The project is kept platform-neutral in CMake; Windows uses MSYS2 MinGW64 + SDL2, while macOS can build with Homebrew SDL2.

## Build

### Windows

Use the MSYS2 MinGW64 toolchain and SDL2:

```powershell
$env:Path='D:\msys64\mingw64\bin;D:\msys64\usr\bin;' + $env:Path
D:\ST\STM32CubeCLT_1.18.0\CMake\bin\cmake.exe -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=D:/msys64/mingw64/bin/gcc.exe
D:\ST\STM32CubeCLT_1.18.0\CMake\bin\cmake.exe --build build
```

If SDL2 is missing:

```powershell
D:\msys64\usr\bin\pacman.exe -S --needed --noconfirm mingw-w64-x86_64-SDL2
```

### macOS

Install dependencies:

```bash
brew install cmake ninja pkg-config sdl2
```

Build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

### Windows

```powershell
.\build\lvgl_motion_demo.exe
```

### macOS

```bash
./build/lvgl_motion_demo
```

## Controls

- Overview starts with no single focused card; all visible agents are highlighted for monitoring.
- Up / Down: move selection without auto-expanding.
- Enter: open the selected item; from overview, open the first CHECK/ERR item.
- Tab: jump to the next CHECK/ERR item.
- Left / Right: switch decision action in CHECK detail.
- A / R: approve or reject command permission examples.
- N / D: add or delete a simulated task.
- S: cycle the selected task status.
- Esc: close detail and return to overview monitoring.

## Notes

- LVGL is fixed to `v9.5.0` through CMake `FetchContent`.
- Layout uses LVGL flex, percent width, content size, and measured layout dimensions.
- The code does not use `lv_obj_set_pos`, `lv_obj_set_x`, `lv_obj_set_y`, or `lv_obj_align`.
- `960x400` is only used for SDL window creation, not for deriving widget positions.
- Logo source files are official-domain UI icons for prototype validation. See `assets/logos/source/README.md`.
