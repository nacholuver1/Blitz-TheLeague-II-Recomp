BLITZ: THE LEAGUE II - native PC recompilation
==============================================

An unofficial native Windows and Linux port of Blitz: The League II (Xbox 360),
made by static recompilation with the ReXGlue SDK. No emulator. This download
contains none of the game's data (models, textures, audio, video): you need
your own copy of the game.

What you need
  - Your own Blitz: The League II disc for Xbox 360 (the USA / Europe release),
    dumped to a disc image (.iso). Other releases won't work: the program only
    matches that exact game executable.
  - Windows 10 or 11, 64-bit, and a GPU with Vulkan (Direct3D 12 also works
    but is much slower); or Linux x86-64 with glibc 2.38 and GCC 13's
    libstdc++ or newer (Ubuntu 24.04, Fedora 39, current Arch / SteamOS /
    Bazzite) and a Vulkan driver.
  - About 7 GB of free space next to the program for the unpacked game files.

First run
  1. Unzip (Windows) or untar (Linux) this folder anywhere you like - it is
     fully portable.
  2. Run blitz2.exe (Windows) or ./blitz2 (Linux). On Linux the file picker
     needs xdg-desktop-portal or zenity; without one, use --blitz.iso=PATH.
  3. Choose "Use my disc image (.iso)" and pick your dump. It is checked, then
     unpacked into a "game" folder next to the program (one time only; you can
     delete the .iso afterwards if you want).
     Already extracted the disc with another tool? Choose "Use an extracted
     game folder" and pick the folder that contains default.xex.
  4. The launcher opens: pick your settings, then START GAME.
  The very first launch shows a black screen for up to a minute while shaders
  compile. Later launches reach the title screen in about 30 seconds.

Controls
  Launcher / menu : arrows or D-pad / left stick to move, Left/Right to change,
                    Enter, A or Start to choose. Mouse works too.
  In game         : F1 or the Guide button (or LB + RB + Back) opens the menu.
                    B, Esc or F1 closes it.  F3 toggles the FPS counter.
  Xbox 360 / XInput controllers work out of the box. A wireless controller
  that went to sleep is picked up again as soon as you press a button.

Settings
  Saved to blitz2.toml next to the program. Render resolution, anti-aliasing
  and the audio buffer apply at startup; changing them in the launcher
  relaunches once.
  Recommended: 2x render resolution, FXAA (extreme), 16x texture filtering.
  Keep VSync ON: the game runs at 60 fps and its speed follows the frame rate,
  so anything below 60 is slow motion (and with VSync off it doesn't advance
  at all). 3x and 4x render resolution need a very fast GPU.
  "Intro video: Skip" skips the opening logo video at startup.

Updates
  The launcher asks GitHub for a newer release when it opens. When there is
  one, a popup offers UPDATE NOW or LATER: Update now downloads it, checks it
  against the checksum GitHub lists, puts the new files in place and
  restarts. Your settings, saves (user\) and game files (game\) stay. The
  Updates row shows the status; Left/Right turns the check off.

Command-line options (every setting in blitz2.toml works as --section.name=value)
  --blitz.iso=PATH            set up from this disc image without the menu
  --title.game_root=PATH      play from this extracted game folder
  --blitz.launcher=false      skip the launcher and start the game directly
  --blitz.check_updates=false no update check this time
  --blitz.skip_intro=true     skip the opening logo video
  --ui.fullscreen=true|false  fullscreen or windowed
  --gpu.driver=d3d12          use Direct3D 12 instead of Vulkan (Windows only;
                              much slower in this game)
  --gpu.resolution_scale_x=2 --gpu.resolution_scale_y=2   render resolution
  Note: options given on the command line are saved to blitz2.toml if you
  change a setting in the menu during that run.

Everything stays in this folder
  game\       the unpacked game files (from your disc)
  user\       profiles, campaign saves, achievements
  blitz2.toml settings
  cache\      compiled shaders (safe to delete; rebuilt on first use)
  logs\       a log of each run
  crashes\    crash dumps, if it ever crashes
  Delete the folder and it's all gone. The launcher updates in place; to
  update by hand, copy the new blitz2.exe
  and DLLs over the old ones; keep game\, user\ and blitz2.toml.

Known issues
  - The first time a new effect appears there can be a short stutter while its
    shader compiles (cached afterwards).
  - Linux has been tested under WSL2 only; reports from real Linux / Steam Deck
    hardware are welcome.

Credits and licenses
  AI-assisted: the code was written by Claude (Anthropic's AI, via Claude
  Code), directed and play-tested by nacholuver1.
  ReXGlue SDK (https://github.com/rexglue/rexglue-sdk), based on Xenia
  (https://xenia.jp). Options menu design adapted from darkuni's MIT-licensed
  recomp menus (https://github.com/darkuni/outrun-arcade-recomp). See the
  licenses folder. Blitz: The League II is a trademark of its owners; this
  project is not affiliated with or endorsed by them.
