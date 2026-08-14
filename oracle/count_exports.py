"""Count DLL exports and list names for parity record."""
import pefile
import sys

pe = pefile.PE(r"build\x64\Release\pillow_c.dll", fast_load=True)
pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
exports = []
if hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
    for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        name = exp.name.decode("ascii") if exp.name else f"ordinal_{exp.ordinal}"
        exports.append(name)
print(f"EXPORTS={len(exports)}")
for name in sorted(exports):
    print(name)
