# Blitz: The League II — native PC recompilation (work in progress)

A static recompilation of **Blitz: The League II** (Xbox 360, 2008) with the
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), following the same approach as
[Blitz-TheLeague-Recomp](https://github.com/nacholuver1/Blitz-TheLeague-Recomp).

**Status:** project skeleton. Not playable yet.

**This repository contains no game data** and none of the C++ the recompiler generates from
the game's executable. You need your own copy of the game.

> **AI-assisted project.** The code, build scripts and documentation are written by
> [Claude](https://claude.com/claude-code) (Anthropic's AI, via Claude Code), directed and
> play-tested by [nacholuver1](https://github.com/nacholuver1).

## Carried over from Blitz: The League

- First-run setup from your own ISO (with a check of the game executable), launcher and
  in-game options menu, Windows and Linux builds.
- `tools/port_addresses.py` finds functions from Blitz 1 (CRT `setjmp`/`longjmp`, XAPI
  fibers, the engine's recursive-lock wrapper, `XUserCheckPrivilege`) in Blitz 2 by
  instruction pattern, so the fixes that took debugging on the first game can be checked
  and applied here.

## License

MIT for this project's own code ([LICENSE](LICENSE)); see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Not affiliated with or endorsed by the
owners of Blitz: The League II.
