# third_party

Vendored dependencies and referenced oracles. Everything here belongs to its
upstream project; this repository only carries or references it for build and
behavior-verification purposes. No authorship is claimed over any of it.

## lcms2 — LittleCMS 2.17 (vendored, committed)

- Upstream: https://github.com/mm2/Little-CMS
- Imported at tag `lcms2.17`, commit `5176347635785e53ee5cee92328f76fda766ecc6`
- Unmodified source import: the 26 C translation units and internal header
  from `src/`, the public `lcms2.h` / `lcms2_plugin.h` headers from `include/`,
  and the upstream `LICENSE` (MIT).
- Use: compiled statically into `pillow_c.dll` for `ImageCms`; no external
  LittleCMS DLL is required at runtime.

## ImagePut (git submodule, referenced)

- Upstream: https://github.com/iseahound/ImagePut
- Added as a git submodule pinned to the upstream revision recorded in
  `.gitmodules`; the files are not committed into this repository.
- Use: optional companion library for the ImagePut interop bridge
  (`Pillow.Image.FromImagePut` / `ToImagePut`) and the `examples/*.ahk`
  scripts. Clone this repository with `git clone --recurse-submodules` (or
  run `git submodule update --init` after cloning) to obtain it.
- License: upstream (MIT) — see `third_party/ImagePut/LICENSE`.

## pillow-11.3.0 — Pillow sdist oracle (referenced, NOT committed)

- Upstream: https://pypi.org/project/pillow/11.3.0/ (sdist
  `Pillow-11.3.0-source.zip`, ~48 MB)
- Use: the local behavior oracle for parity verification. Behavior is
  qualified against Python 3.10 + Pillow 11.3.0; the sdist sources are kept
  locally (ignored by `.gitignore`) for reference only.
- The directory `third_party/pillow-11.3.0/` is intentionally not tracked;
  re-download the sdist from the PyPI link above if it is missing.
