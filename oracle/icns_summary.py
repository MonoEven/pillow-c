import json

d = json.load(open(r"oracle/probe_icns.json", encoding="utf-8"))
for k in sorted(d):
    v = d[k]
    if isinstance(v, dict):
        if "full" in v:
            print(k, ": full hex len =", len(v["full"]), "entries:", [(e[0], e[1]) for e in v["entries"]])
        else:
            print(k, "=", v)
    else:
        print(k, "=", v)
