# Repository Operating Notes

This repository is a performance-first native DLL layer for AutoHotkey v2. The
main product is `pillow_c.dll`, with `ahk/pillow.ahk` providing a Python-like
Pillow facade where it is useful and stable.

## Required Startup For Pillow Work

Before continuing any Pillow compatibility or implementation task:

1. Read `docs/pillow-gap-checkpoint.md`.
2. Read `docs/pillow-gap-analysis.md`.
3. Use the gap ID from those files as the work packet when possible.
4. Read `docs/native-abi.md` before changing exported DLL behavior.
5. Read `docs/testing.md` before running AHK tests.
6. Check the current worktree before editing.

Do not start each session by re-auditing the whole project. The gap ledger is
the persistent project memory; update it whenever coverage changes or a new
Pillow behavior probe resolves an uncertainty.

After an interruption, context compaction, or handoff, resume from the on-disk
ledger instead of chat memory. Use `docs/pillow-gap-checkpoint.md` as the
short current-state readout and `docs/pillow-gap-analysis.md` as the detailed
execution ledger. If a task is already in progress, use the current work-packet
checkpoint in those docs as the next debugging entry point.

After reading the ledger, state the current completion estimate, the latest
covered gap, the selected next gap ID, and the known test state before editing.
If the user asks about an area that has no gap ID yet, add a concrete ledger row
first instead of doing a broad fresh audit.

## Disk-First Gap Workflow

Treat `docs/pillow-gap-analysis.md` as the authoritative detailed source of
truth for Pillow coverage, with `docs/pillow-gap-checkpoint.md` as its short
front page. A Pillow task is not ready for implementation until the current
worker can name:

- the exact gap ID being worked,
- the covered behavior that should not be re-probed,
- the unknown Pillow behavior that still needs an oracle/source check,
- the native/facade/test files to open first,
- and the targeted plus full test commands that will prove the change.

If those facts are missing or too vague, update the gap ledger first and stop
there unless the user explicitly asks to continue into implementation. Do not
replace the ledger with chat-only analysis.

## Project Constraints

- Behavior authority is `F:\Python\Python310\python.exe` with Pillow `11.3.0`.
- Do not use the parent `pillow.ahk` as a reference implementation.
- Do not modify parent `tools`; tests may call the parent runner only.
- Every AHK test run should include `-TimeoutSeconds 120`.
- AHK tests must use `ahktest` and captured errors, not modal popups.
- Do not run AHK tests in parallel.
- If native code changes, rebuild and keep `build/x64/Release/pillow_c.dll`
  current.
- Do not remote, push, or change GitHub project wiring unless explicitly asked.

## Build And Test Shape

Build from this repository:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\src\pillow_c.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

Run the full AHK suite from the parent `visual_studio` directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run-ahktest.ps1 -Target .\tasks\2026-06-07-pillow-c-foundation\ahk -Report .codex\pillow-c-report.txt -TimeoutSeconds 120
```

For targeted tests, keep the same runner and timeout shape.
