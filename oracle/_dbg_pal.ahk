#Requires AutoHotkey v2.0
#Include "..\ahk\pillow.ahk"

Pillow.Configure({ DllPath: A_ScriptDir "\..\build\x64\Release\pillow_c.dll" })
path := A_ScriptDir "\_dbg_pal2.gpl"
gplFull := "GIMP Palette`n"
loop 256
    gplFull .= (A_Index - 1) " " (A_Index - 1) " " (A_Index - 1) " " A_Index "`n"
FileAppend(gplFull, path)
try {
    result := Pillow.ImagePalette.GimpPaletteParse(FileRead(path))
    FileAppend("parse OK " result[1].Size " " result[2] "`n", "*")
} catch Error as err {
    FileAppend("parse ERR: " err.Message "`n", "*")
}
try {
    result2 := Pillow.ImagePalette.Load(path)
    FileAppend("load OK " result2[1].Size " " result2[2] "`n", "*")
} catch Error as err {
    FileAppend("load ERR: " err.Message "`n", "*")
}
FileDelete(path)
