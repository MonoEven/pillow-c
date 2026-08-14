#Requires AutoHotkey v2.0
records := [
    [0x0103, [8]],
    [0x020B, [0, 0]]
]
out := ""
for record in records {
    out .= "func=" record[1] " len=" record[2].Length " params=" record[2][1] "`n"
}
FileAppend(out, "*")
