#Requires AutoHotkey v2.0
; ---------------------------------------------------------------------------
; pillow_c_demo.ahk — executes every README/blog example verbatim and
; asserts the observable results. Run from the repository root:
;   AutoHotkey64.exe /iLib <Lib> /ErrorStdOut pillow_c_demo.ahk
; Exits 0 when all examples pass, 1 otherwise.
; ---------------------------------------------------------------------------
#Include "ahk\pillow.ahk"
#Include "..\2026-07-19-cnumpy-foundation\ahk\numpy.ahk"

FileAppend "demo-start`n", A_Temp "\pillow-c-demo-start.txt"

SetWorkingDir A_ScriptDir
Pillow.Configure({ DllPath: A_ScriptDir "\build\x64\Release\pillow_c.dll" })
Numpy.DllPath := A_ScriptDir "\..\2026-07-19-cnumpy-foundation\build\x64\Release\cnumpy_ahk.dll"
Numpy.Init()

failures := 0
report := ""

log(msg) {
    global report
    report .= msg "`n"
}
check(name, cond) {
    global failures
    if cond
        log("PASS  " name)
    else {
        failures++
        log("FAIL  " name)
    }
}
step(name, fn) {
    global failures
    try {
        fn()
        log("PASS  " name)
    } catch as err {
        failures++
        log("FAIL  " name " : " err.Message)
    }
}

temp := A_Temp
photoPng := temp "\pillow-c-demo-photo.png"
photoJpg := temp "\pillow-c-demo-photo.jpg"

; --- 0. build the input photos the examples open ---------------------------
step("build input photo.png", () => (
    Pillow.Image.LinearGradient("L").Convert("RGB").Save(photoPng)
))
step("build input photo.jpg", () => (
    Pillow.Image.RadialGradient("L").Convert("RGB").Save(photoJpg, "JPEG")
))

; --- 1. README quick start: open, resize, save ------------------------------
step("open/resize/save example", () => (
    (
        img := Pillow.Image.Open(photoPng),
        img := img.Resize([800, 600]),
        img.Save(temp "\pillow-c-demo-photo-800.jpg"),
        img.Close(),
        reopened := Pillow.Image.Open(temp "\pillow-c-demo-photo-800.jpg"),
        check("resized size is [800, 600]", reopened.Size[1] = 800 && reopened.Size[2] = 600),
        reopened.Close()
    )
))
step("create and inspect example", () => (
    (
        canvas := Pillow.Image.New("RGB", [320, 200], "white"),
        canvas.PutPixel([10, 10], "red"),
        check("GetPixel tuple length is 3", canvas.GetPixel([10, 10]).Length = 3),
        canvas.Close()
    )
))

; --- 2. README gradients and statistics -------------------------------------
step("gradients and statistics example", () => (
    (
        gray := Pillow.Image.LinearGradient("L"),
        hist := gray.Histogram(),
        ext := gray.GetExtrema(),
        check("L histogram has 256 entries", hist.Length = 256),
        check("L extrema are [0, 255]", ext[1] = 0 && ext[2] = 255),
        grad := Pillow.Image.RadialGradient("L"),
        grad.Thumbnail([128, 128]),
        grad.Save(temp "\pillow-c-demo-gradient.png"),
        check("gradient.png exists", FileExist(temp "\pillow-c-demo-gradient.png") != ""),
        grad.Close(),
        gray.Close()
    )
))

; --- 3. README drawing and filters -------------------------------------------
step("drawing example", () => (
    (
        img := Pillow.Image.New("RGB", [240, 140], "white"),
        d := Pillow.ImageDraw.Draw(img),
        d.Line([[0, 0], [239, 0], [239, 139], [0, 139], [0, 0]], "blue", 3),
        d.RoundedRectangle([20, 20, 220, 120], 12, "yellow", "red", 2),
        d.Ellipse([60, 40, 180, 100], "lime", "black", 1),
        d.Text([12, 118], "Hello", "navy"),
        img.Save(temp "\pillow-c-demo-draw.png"),
        check("draw.png exists", FileExist(temp "\pillow-c-demo-draw.png") != ""),
        img.Close()
    )
))
step("filters example", () => (
    (
        photo := Pillow.Image.Open(photoJpg),
        blurred := photo.Filter(Pillow.ImageFilter.GaussianBlur(2)),
        sharp := photo.Filter(Pillow.ImageFilter.UnsharpMask(2, 150, 3)),
        k := Pillow.ImageFilter.Kernel([3, 3], [-1,-1,-1, -1,8,-1, -1,-1,-1], 1, 0),
        edges := photo.Filter(k),
        check("filter results keep the photo size",
            blurred.Size[1] = photo.Size[1] && sharp.Size[1] = photo.Size[1] && edges.Size[1] = photo.Size[1]),
        photo.Close()
    )
))

; --- 4. README/blog quantize --------------------------------------------------
step("quantize example", () => (
    (
        rgb := Pillow.Image.Open(photoJpg),
        p := rgb.Quantize(256, Pillow.Quantize.MEDIANCUT),
        p.Save(temp "\pillow-c-demo-photo-256.gif"),
        check("photo-256.gif exists", FileExist(temp "\pillow-c-demo-photo-256.gif") != ""),
        gif := Pillow.Image.Open(temp "\pillow-c-demo-photo-256.gif"),
        check("quantized GIF reopens as mode P", gif.Mode = "P"),
        gif.Close(),
        p.Close(),
        rgb.Close()
    )
))

; --- 5. README NumPy interop (cnumpy) ----------------------------------------
step("numpy interop example", () => (
    (
        img := Pillow.Image.Open(photoJpg),
        a := img.AsArray(),
        check("AsArray shape is [H, W, 3]", a.Shape.Length = 3 && a.Shape[3] = 3),
        mean := a.Mean(),
        check("Mean returns a number", mean is Number),
        img.Close(),
        check("array snapshot survives image close", a.Shape.Length = 3),
        b := Numpy.Zeros([64, 64, 4], 3),
        im := Pillow.Image.FromArray(b),
        check("FromArray uint8 (H,W,4) yields mode RGBA", im.Mode = "RGBA"),
        im.Save(temp "\pillow-c-demo-black.png"),
        check("black.png exists", FileExist(temp "\pillow-c-demo-black.png") != ""),
        im.Close()
    )
))

; --- finish -------------------------------------------------------------------
Numpy.Cleanup()
FileAppend report, temp "\pillow-c-demo-report.txt"
if failures > 0 {
    FileAppend "FAILURES: " failures "`n", temp "\pillow-c-demo-report.txt"
    ExitApp(1)
}
FileAppend "ALL EXAMPLES PASSED`n", temp "\pillow-c-demo-report.txt"
ExitApp(0)
