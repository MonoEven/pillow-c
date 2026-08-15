#Requires AutoHotkey v2.0
; ---------------------------------------------------------------------------
; ImagePut is the main driver; pillow-c is used only as the auxiliary
; pixelation/averaging engine.
;
; Flow:
;   ImagePutBuffer(source) -> Pillow.Image.FromImagePut()
;   -> Pillow Resize(BOX) + Resize(NEAREST)
;   -> ToImagePut() -> ImagePutFile() / ImagePutWindow()
; ---------------------------------------------------------------------------
#Include "..\..\..\third_party\ImagePut\ImagePut.ahk"
#Include "..\ahk\pillow.ahk"

Pillow.Configure({ DllPath: A_ScriptDir "\..\build\x64\Release\pillow_c.dll" })

failures := 0
Log(msg) {
    FileAppend msg "`n", A_ScriptDir "\imageput_pixelate.log"
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

; --- Prepare a deterministic input file with Pillow (test fixture only) ------
srcPath := A_ScriptDir "\imageput_pixelate_src.png"
outPath := A_ScriptDir "\imageput_pixelate_out.png"

fixture := Pillow.Image.New("RGB", [200, 200], "white")
d := Pillow.ImageDraw.Draw(fixture)
d.Rectangle([0, 0, 99, 99], [255, 0, 0])
d.Rectangle([100, 0, 199, 99], [0, 255, 0])
d.Rectangle([0, 100, 99, 199], [0, 0, 255])
d.Rectangle([100, 100, 199, 199], [255, 255, 0])
fixture.Save(srcPath)
fixture.Close()

; --- Main flow: ImagePut drives input and output -----------------------------
myImage := ImagePutBuffer(srcPath)                 ; ImagePut loads the file

p := Pillow.Image.FromImagePut(myImage)            ; ImagePut -> pillow-c
small := p.Resize([4, 4], Pillow.Resampling.BOX)   ; pillow-c: area average
pixelated := small.Resize([200, 200], Pillow.Resampling.NEAREST) ; pillow-c: pixelate

result := pixelated.ToImagePut()                   ; pillow-c -> ImagePut
ImagePutFile(result, outPath)                      ; ImagePut saves the result

; --- Verify with pillow-c that the ImagePut round trip worked ---------------
Check("ImagePut output file exists", FileExist(outPath) != "")
out := Pillow.Image.Open(outPath)
Check("Output size is 200x200", out.Width = 200 && out.Height = 200)
Log("  TL=" ArrStr(out.GetPixel([10, 10])) " TR=" ArrStr(out.GetPixel([110, 10])) " BL=" ArrStr(out.GetPixel([10, 110])) " BR=" ArrStr(out.GetPixel([110, 110])))
Check("Top-left block is red", ArrStr(out.GetPixel([10, 10])) = ArrStr([255, 0, 0, 255]))
Check("Top-right block is green", ArrStr(out.GetPixel([110, 10])) = ArrStr([0, 255, 0, 255]))
Check("Bottom-left block is blue", ArrStr(out.GetPixel([10, 110])) = ArrStr([0, 0, 255, 255]))
Check("Bottom-right block is yellow", ArrStr(out.GetPixel([110, 110])) = ArrStr([255, 255, 0, 255]))

pixelated.Close()
small.Close()
p.Close()
out.Close()

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
