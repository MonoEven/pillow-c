# LittleCMS 2.17

This directory contains an unmodified source import from the official
[Little-CMS repository](https://github.com/mm2/Little-CMS), tag `lcms2.17`,
commit `5176347635785e53ee5cee92328f76fda766ecc6`.

Imported files are the 26 C translation units and internal header from
`src/`, the public `lcms2.h` and `lcms2_plugin.h` headers from `include/`, and
the upstream `LICENSE`. The project compiles these sources statically into
`pillow_c.dll`; no external LittleCMS DLL is required at runtime.

The import is intentionally limited to the library files needed by
`MODE-COLOR-001BB`. Tests, command-line tools, plugins, and build-system files
from the upstream repository are not vendored.
