# Notes for Claude

## sfwin32 and the original game executables

- Never build or run [sfwin32](https://github.com/mnkraus/sfwin32) or the
  original executables (`FIGHT.EXE`, `DESKTOP.EXE`, `SF*.EXE`, …) on the host.
  They may only run inside an isolated, offline, snapshotted Windows VM as a
  non-admin user, following the checklist in
  [docs/ROADMAP.md](docs/ROADMAP.md#working-safely-with-sfwin32-and-the-original-executables).
  If asked to run them, point to the checklist and leave running them to the
  user.
- Reading sfwin32's source as a reference is fine. Clone it into a separate
  scratch directory, never into this repository, and record the commit hash
  you looked at.
- Verify game executables against the checksums in the roadmap before relying
  on them.
- Use sfwin32 and the original game only to observe behaviour. Implement
  features independently; never copy disassembled or decompiled logic into
  this GPL code base.
