import json

with open(r".codex\boundary-matrix.json", encoding="utf-8") as fp:
    report = json.load(fp)

print("FEATURES:", report["features"])
print("MODULES:", report["modules"])
print()

for fmt, entry in report.items():
    if fmt in ("features", "modules"):
        continue
    parts = []
    for mode in ["L", "RGB", "RGBA", "P"]:
        s = entry[f"save_{mode}"]
        if s["status"] == "ok":
            parts.append(f"{mode}:ok")
        else:
            parts.append(f"{mode}:{s['status']}|{s.get('message', '')[:45]}")
    reopen = entry["reopen_own_l"]
    if reopen["status"] == "ok":
        ro = f"ok {reopen.get('mode')}"
    else:
        ro = f"{reopen['status']}|{reopen.get('message', '')[:55]}"
    print(f"{fmt:10s} SAVE[{'; '.join(parts)}]")
    print(f"{'':10s} REOPEN_L[{ro}]")
