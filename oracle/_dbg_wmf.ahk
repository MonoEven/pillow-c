#Requires AutoHotkey v2.0
#Include "..\ahk\pillow.test.ahk"

records := [
    [0x0103, [8]],
    [0x020B, [0, 0]],
    [0x020C, [100, 100]],
    [0x041B, [100, 100, 0, 0]]
]
blob := PillowTestWmf(, , records)
path := A_ScriptDir "\_dbg_wmf_rec.wmf"
file := FileOpen(path, "w")
file.RawWrite(blob, blob.Size)
file.Close()
FileAppend("blob size " blob.Size "`n", "*")
out := ""
loop blob.Size
    out .= Format("{:02X}", NumGet(blob, A_Index - 1, "UChar"))
FileAppend(out "`n", "*")
