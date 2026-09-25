# Castlevania: Legacy of Darkness recompilation WIP

This repository is an in-progress port of the Castlevania: Legacy of Darkness recompilation project.

This port is currently based on upstream v0.2.26.

## Nintendo Switch

### Installing

1. Download `LodRecomp-vX.Y.Z-switch-RCN.zip` from the releases page.
2. Extract to your SD card. This creates `sdmc:/switch/lodrecomp/LodRecomp.nro`.
3. Copy your legally dumped Castlevania: Legacy of Darkness ROM (US in `.z64` format) to the same folder, and name it as `rom.z64`:

Your folder should look like:

```text
sdmc:/switch/lodrecomp/LodRecomp.nro
sdmc:/switch/lodrecomp/rom.z64
```

If `rom.z64` is missing, the app also looks for any `.z64`/`.n64`/`.v64` file in `sdmc:/switch/lodrecomp/`, `sdmc:/roms/n64/`, `sdmc:/roms/`, and `sdmc:/switch/`. It prefers names containing "legacy", "lod" or "castlevania2".

### Launching

Launch via the homebrew launcher as you would any other app.

The first run is slower than the rest and you will encounter stutters. Subsequent runs shouldn't have this issue as much ideally.

### Controls

The in-game menu opens with `-`. The face buttons follow button position of an Xbox controller (so in the menu b is confirm, y is back). Default game controls:

- B = N64 A / jump
- Y = N64 B / attack 1 / primary attack
- X = N64 C-Left / attack 2 / secondary attack
- A = N64 C-Right / collect / interact
- \+ = Start
- Left stick = analog stick
- D-pad = N64 D-pad
- Left stick click = N64 R / lock-on
- R = N64 C-Down / throw item
- ZL = N64 Z
- ZR = N64 L
- Right stick = inverted N64 D-pad / camera

The mapping can be changed from the in-game menu, or in `controls.json` (format under [Controls](#controls)).

### Reporting problems

Please include:

- The release name (for example `v1.0.0-switch-RC1`).
- Whether the console was handheld or docked.
- Your Atmosphère and system firmware versions.
- `sdmc:/switch/lodrecomp/LodRecomp.log`.
- For a crash, the newest file in `sdmc:/atmosphere/crash_reports/`.
- What you were doing and where in the game you were.
- Your save file.

### Building the Switch release from source

This needs devkitPro with `switch-dev`, `switch-sdl2`, `switch-freetype`, `switch-zlib`, the [nxvk](https://github.com/PalindromicBreadLoaf/nxvk) portlib, CMake 3.24+, Ninja, and Python 3. You also need the generated/local files listed above, including `RecompiledFuncs/`.

```sh
cmake -S . -B build-switch -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-switch.cmake -DCMAKE_BUILD_TYPE=Release
ninja -C build-switch LodRecomp_nro
```

## Controls

Gamepad controls are configurable by editing `controls.json` in `sdmc:/switch/lodrecomp`':

The file is created on first launch.

Valid N64 binding values are:

```text
none, n64_a, n64_b, n64_z, n64_start,
n64_dpad_up, n64_dpad_down, n64_dpad_left, n64_dpad_right,
n64_l, n64_r,
n64_c_up, n64_c_down, n64_c_left, n64_c_right
```

Example:

```json
{
  "gamepad": {
    "buttons": {
      "a": "n64_a",
      "b": "n64_c_right",
      "x": "n64_b",
      "y": "n64_c_left",
      "start": "n64_start",
      "left_stick": "n64_r",
      "right_bumper": "n64_c_down",
      "dpad_up": "n64_dpad_up",
      "dpad_down": "n64_dpad_down",
      "dpad_left": "n64_dpad_left",
      "dpad_right": "n64_dpad_right"
    },
    "axes": {
      "left_trigger": "n64_z",
      "right_trigger": "n64_l",
      "trigger_threshold": 12000
    },
    "right_stick": {
      "mode": "n64_dpad",
      "invert_x": true,
      "invert_y": true,
      "deadzone": 0.5
    }
  }
}
```
