#Requires AutoHotkey v2.0
; ---------------------------------------------------------------------------
#Include "..\third_party\ImagePut\ImagePut.ahk"
#Include "..\ahk\pillow.ahk"

Pillow.Configure({ DllPath: A_ScriptDir "\..\build\x64\Release\pillow_c.dll" })

failures := 0
Log(msg) {
    FileAppend msg "`n", A_ScriptDir "\imageput_bridge.log"
}
Check(name, cond) {
    global failures
    if cond
        Log("PASS  " name)
    else {
        failures++
        Log("FAIL  " name)
    }
}

; Build a small RGBA image.
img := Pillow.Image.New("RGBA", [2, 2], "white")
img.PutPixel([0, 0], [255, 0, 0, 255])
img.PutPixel([1, 0], [0, 255, 0, 128])
img.PutPixel([0, 1], [0, 0, 255, 255])
img.PutPixel([1, 1], [255, 255, 0, 64])

; Pillow -> ImagePut
ip := img.ToImagePut()
Check("ToImagePut returns an ImagePut buffer object", IsObject(ip) && ip.HasProp("width") && ip.HasProp("height"))
Check("ToImagePut keeps dimensions", ip.width = 2 && ip.height = 2)

; ImagePut -> Pillow
img2 := Pillow.Image.FromImagePut(ip)
Check("FromImagePut returns RGBA", img2.Mode = "RGBA")
Check("FromImagePut keeps dimensions", img2.Width = 2 && img2.Height = 2)
Check("FromImagePut preserves top-left pixel", ArrStr(img2.GetPixel([0, 0])) = ArrStr([255, 0, 0, 255]))
Check("FromImagePut preserves alpha", ArrStr(img2.GetPixel([1, 1])) = ArrStr([255, 255, 0, 64]))

; Also test FromImagePut from a saved file path.
pngPath := A_ScriptDir "\imageput_bridge.png"
img.Save(pngPath)
img3 := Pillow.Image.FromImagePut(pngPath)
Check("FromImagePut accepts a file path", img3.Width = 2 && img3.Height = 2)

img.Close()
img2.Close()
img3.Close()

if failures > 0 {
    Log("FAILURES: " failures)
    ExitApp(1)
}
Log("ALL CHECKS PASSED")
ExitApp(0)

ArrStr(arr) {
    s := "["
    for i, v in arr
        s .= (i > 1 ? ", " : "") . v
    return s . "]"
}
