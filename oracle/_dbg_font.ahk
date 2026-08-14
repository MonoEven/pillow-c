#Requires AutoHotkey v2.0
#Include "..\ahk\pillow.ahk"

Pillow.Configure({ DllPath: A_ScriptDir "\..\build\x64\Release\pillow_c.dll" })
font := Pillow.ImageFont.LoadDefault()
FileAppend("A bbox: " PillowTestJoin(font.GetBbox("A")) "`n", "*")
FileAppend("AB bbox: " PillowTestJoin(font.GetBbox("AB")) "`n", "*")
FileAppend("AB length: " font.GetLength("AB") "`n", "*")
maskA := font.GetMask("A")
FileAppend("A mask: " maskA.Size[1] "x" maskA.Size[2] "`n", "*")
maskA.Close()
maskAB := font.GetMask("AB")
FileAppend("AB mask: " maskAB.Size[1] "x" maskAB.Size[2] "`n", "*")
maskAB.Close()
font.Close()

PillowTestJoin(arr) {
    return arr[1] "," arr[2] "," arr[3] "," arr[4]
}
