#Requires AutoHotkey v2.0
#Include "..\ahk\pillow.ahk"

Pillow.Configure({ DllPath: A_ScriptDir "\..\build\x64\Release\pillow_c.dll" })
path := "C:\Users\78442\AppData\Local\Temp\mpeg-zero.mpg"
FileAppend("accepts: " Pillow.Image.MpegAccepts(path) "`n", "*")
try {
    opened := Pillow.Image.Open(path)
    FileAppend("open OK`n", "*")
    opened.Close()
} catch Error as err {
    FileAppend("open ERR: " err.Message "`n", "*")
}
