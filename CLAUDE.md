# Notes for Claude

## sfwin32 and the original game executables

- Never build or run [sfwin32](https://github.com/mnkraus/sfwin32) (its
  `fightport.exe` runs the game's machine code natively) on the host. It may
  only run inside an isolated, offline, snapshotted Windows VM as a non-admin
  user, following the checklist in
  [docs/ROADMAP.md](docs/ROADMAP.md#working-safely-with-sfwin32-and-the-original-executables).
  If asked to run it, point to the checklist and leave running it to the user.
- The original DOS executables (`FIGHT.EXE`, `DESKTOP.EXE`, `SF*.EXE`, …) may
  run on the host only under DOSBox-X (Homebrew `dosbox-x`, an interpreter),
  with a *copy* of the game folder as the only mount (plus the CD image), never
  the GOG installation itself, and never natively or under a Windows runner.
  Verify their checksums first. How to drive them is in
  [docs/reverse-engineering.md](docs/reverse-engineering.md).
- Reading sfwin32's source as a reference is fine. Clone it into a separate
  scratch directory, never into this repository, and record the commit hash
  you looked at.
- Verify game executables against the checksums in the roadmap before relying
  on them.
- Use sfwin32 and the original game only to observe behaviour. Implement
  features independently; never copy disassembled or decompiled logic into
  this GPL code base.
