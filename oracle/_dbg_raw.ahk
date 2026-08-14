#Requires AutoHotkey v2.0
path := A_ScriptDir "\_dbg_raw.bin"
file := FileOpen(path, "w")
try {
    empty := Buffer(0, 0)
    try {
        file.RawWrite(empty, empty.Size)
        FileAppend("rawwrite empty OK`n", "*")
    } catch Error as err {
        FileAppend("rawwrite empty ERR: " err.Message "`n", "*")
    }
} finally {
    file.Close()
    FileDelete(path)
}
