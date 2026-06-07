# Testing

Tests use `ahktest` from [MonoEven/stdlib-ahk](https://github.com/MonoEven/stdlib-ahk).

In the current local workspace, run tests through the parent `visual_studio\tools\run-ahktest.ps1` wrapper. That runner adds `#ErrorStdOut`, captures unhandled AHK errors, writes reports, and prevents modal error popups from blocking automation.

## Build

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\tasks\2026-06-07-pillow-c-foundation\src\pillow_c.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

From inside this repository:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\src\pillow_c.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

## Oracle

Regenerate Pillow behavior fixtures with local Python 3.10.11:

```powershell
F:\Python\Python310\python.exe .\tasks\2026-06-07-pillow-c-foundation\oracle\pillow_oracle.py
```

From inside this repository:

```powershell
F:\Python\Python310\python.exe .\oracle\pillow_oracle.py
```

## AHK Tests

From the parent `visual_studio` workspace:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run-ahktest.ps1 -Target .\tasks\2026-06-07-pillow-c-foundation\ahk -Report .codex\pillow-c-report.txt -TimeoutSeconds 20
```

This suite currently covers both raw DLL calls and the AHK facade.
