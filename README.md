# Blitz: The League II — native PC recompilation

An unofficial native Windows and Linux port of **Blitz: The League II** (Xbox 360, 2008), made
by *static recompilation* with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).
The game's PowerPC code is translated to C++ ahead of time and compiled into a normal x64
program (`blitz2.exe` on Windows, `blitz2` on Linux). No emulator runs at play time; the SDK
provides the Xbox 360 kernel, GPU (Vulkan / Direct3D 12), audio and input layers.

Companion to [Blitz-TheLeague-Recomp](https://github.com/nacholuver1/Blitz-TheLeague-Recomp)
(the first game); most of what that project worked out carried over.

**This repository contains no game data.** No disc image, no game files, and none of the
C++ that the recompiler generates from the game's executable. To play, you need your own
copy of the game.

> **AI-assisted project.** The code, build scripts, debugging and documentation in this
> repository were written by [Claude](https://claude.com/claude-code) (Anthropic's AI, via
> Claude Code), directed and play-tested by [nacholuver1](https://github.com/nacholuver1).
> Every commit carries a `Co-Authored-By: Claude` trailer. Treat it like any community
> project: read the code, and report problems in Issues.

## Playing

1. Download the latest release from [Releases](../../releases): the `-win64.zip` for Windows or
   the `-linux-x64.tar.gz` for Linux. Extract it anywhere (it's portable).
2. Dump your own **Blitz: The League II** disc (USA / Europe release) to an `.iso`.
3. Run `blitz2.exe` (Windows) or `./blitz2` (Linux), choose **Use my disc image (.iso)** and
   pick your dump. It checks the game executable, then unpacks the disc into a `game` folder
   next to the program (about 6.7 GB, one time).
4. The launcher opens: pick settings, then **START GAME**.

Runs at full speed (60 fps) at 2x render resolution (2560x1440) with FXAA and 16x texture
filtering on an RTX 4070 SUPER. Keep **VSync on**: game speed follows the frame rate, and the
game needs the vertical blank to advance.

The player README ([release/README.txt](release/README.txt)) covers controls, settings and
command-line flags (`--blitz.iso=`, `--title.game_root=`, `--blitz.launcher=false`,
`--blitz.skip_intro=true`, `--ui.fullscreen=`, `--gpu.driver=`, and any `--section.name=value`
from `blitz2.toml`).

Everything (game files, saves, settings, shader cache, logs) lives in the program's folder.

## Building from source

Requirements (Windows x64):

- Visual Studio 2022 Build Tools (C++ workload, Windows SDK 10.0.26100)
- LLVM / Clang ≥ 20 (built with Clang 23) at `C:\Program Files\LLVM`
- Vulkan SDK (built with 1.4.363) at `C:\VulkanSDK\1.4.363.0`
- CMake ≥ 3.25 and Ninja (both come with the Build Tools)
- Python 3 (only for the helper scripts in `tools/`)
- Your own dump of the game

Different install paths: set `BLITZ_VCVARS` (path to `vcvars64.bat`), `LLVM_DIR` and
`VULKAN_SDK` before running the scripts.

```bat
git clone --recursive https://github.com/nacholuver1/Blitz-TheLeague-II-Recomp.git
cd Blitz-TheLeague-II-Recomp

rem 1. ReXGlue SDK v0.11.0 (submodule) + this project's patches, then build and install it
git -C deps/rexglue-sdk apply ../../patches/rexglue-dcbz-128.patch ../../patches/rexglue-audio-diag.patch ../../patches/rexglue-vsync-relaxed.patch
tools\build_sdk.bat

rem 2. Put your extracted disc in game\ (must contain default.xex), e.g.
python tools\extract_xiso.py "path\to\your.iso" game

rem 3. Build. The first build runs `rexglue codegen` (reads game\default.xex, writes the
rem    recompiled C++ to blitz\generated\), reconfigures, then compiles.
tools\build_blitz.bat release

rem 4. Player zip (blitz2.exe + DLLs + README + licenses, no game data) in dist\
powershell -ExecutionPolicy Bypass -File tools\make_release.ps1 -Version 1.0.0
```

`tools\build_blitz.bat package` builds Release and copies the program files into a local
`BlitzPC\` play folder without touching its saves or settings.

### Linux (x86-64)

Tested on Ubuntu 24.04 (under WSL2). Build on the oldest distro you want to support: the
binary needs that glibc (2.38+) and libstdc++ (GCC 13+) or newer.

```bash
git clone --recursive https://github.com/nacholuver1/Blitz-TheLeague-II-Recomp.git
cd Blitz-TheLeague-II-Recomp
sudo bash tools/linux_deps.sh          # cmake, ninja, clang-20 (apt.llvm.org), SDL/GTK/audio dev packages
source ~/vulkan/<version>/setup-env.sh # Vulkan SDK 1.4.313+ from https://vulkan.lunarg.com/sdk/home#linux
tools/build_linux.sh sdk               # applies patches/, builds and installs the SDK
python3 tools/extract_xiso.py /path/to/your.iso game
tools/build_linux.sh release           # codegen + build: blitz/out/build/linux-amd64/Release/blitz2
tools/make_release_linux.sh 1.0.0 --no-build   # dist/BlitzTheLeagueII-1.0.0-linux-x64.tar.gz
```

With 8 GB of RAM or less, limit parallel jobs (`export CMAKE_BUILD_PARALLEL_LEVEL=6`).

### Game version

The setup screen only accepts the exact `default.xex` this project was made against (title
`4D5707E8`, SHA-256 in [blitz/src/blitz_setup.cpp](blitz/src/blitz_setup.cpp)). Another
release of the game needs its own addresses in the manifest and hooks, and a new hash.

## How it works (and what it took)

| Where | What |
|---|---|
| [tools/port_addresses.py](tools/port_addresses.py) | Finds functions from Blitz 1 in Blitz 2 by instruction pattern (whole functions from `.pdata`, with branch targets and address immediates masked). Ported the CRT `setjmp`/`longjmp`, the XAPI fiber functions, the engine's recursive-lock wrapper and `XUserCheckPrivilege` in minutes instead of re-deriving them. |
| [blitz/blitz2_manifest.toml](blitz/blitz2_manifest.toml) | Codegen manifest: `setjmp`/`longjmp`, XAPI fibers mapped to SDK host-fiber shims, and **console-exact vector math** — with the SDK's fast `vmaddfp`/estimates, player faces and arms shot out into long spikes in close-ups. |
| [blitz/src/main.cpp](blitz/src/main.cpp) | **Vulkan by default** — on Direct3D 12 the game ran at ~10 fps in play (the same was reported for Xenia: [#1628](https://github.com/xenia-project/game-compatibility/issues/1628), [#332](https://github.com/xenia-canary/game-compatibility/issues/332)). **Resolve readback `fast`** — `full` (Blitz 1's setting) stalled the CPU on every resolve and held the game at 30 fps, i.e. slow motion; `none` left motion-blur ghost trails. Synchronous shader compilation, game-folder discovery. |
| [blitz/src/blitz_hooks.cpp](blitz/src/blitz_hooks.cpp) | Offline user-created content (`XUserCheckPrivilege` grants privilege 247 for campaign create-a-team saves). Optional intro skip: detours CRI Sofdec's `mwPlyStartFname` so the boot movie can't open, and the game moves straight on. Blitz 1's boot-deadlock fix isn't needed — Blitz 2 ships the corrected lock release. |
| [blitz/src/blitz_setup.cpp](blitz/src/blitz_setup.cpp) | First-run setup: XDVDFS disc-image unpacker, XEX title-ID / hash check, setup screen. |
| [blitz/src/blitz_menu.cpp](blitz/src/blitz_menu.cpp) | Pre-game launcher and in-game options menu (F1 / Guide / LB+RB+Back). |
| [patches/](patches/) | SDK patches shared with Blitz 1: `dcbz` clears a 128-byte cache line on Xenon (the codegen cleared 32), and extra audio-stream diagnostics in the log. |
| [tools/](tools/) | Build scripts (Windows and Linux), release packers, ISO extractor, XEX header dump, and debugging helpers. |

## Known issues

- 3x / 4x render resolution is GPU-bound (about 40 / 28 fps on an RTX 4070 SUPER), and the
  game slows down below 60 fps. 2x matches a 1440p screen.
- The first launch shows a black screen for up to a minute while shaders compile; later
  launches reach the title screen in about 30 seconds.
- Direct3D 12 (`--gpu.driver=d3d12`) works but runs at ~10 fps in play.
- Linux: builds and runs (tested under WSL2 with software rendering); not yet play-tested on
  real Linux hardware with a GPU driver.

## Credits

- Written with [Claude Code](https://claude.com/claude-code) (Anthropic); project direction
  and play-testing by nacholuver1.
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) by Tom Clay, built on [Xenia](https://xenia.jp).
- Menu layout adapted from [darkuni's outrun-arcade-recomp](https://github.com/darkuni/outrun-arcade-recomp) (MIT).
- Xenia testers' compatibility reports for this game pointed at the Direct3D 12 slowdown and
  the importance of VSync.
- See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Legal

This project's own code is MIT licensed ([LICENSE](LICENSE)). Blitz: The League II and its
code, assets and trademarks belong to their owners; this project is not affiliated with or
endorsed by them. The repository contains none of the game's files; the release `blitz2.exe`
contains code recompiled from the game's executable and loads all game data (models,
textures, audio, video) from your own disc. Use it only with a copy of the game you own.
