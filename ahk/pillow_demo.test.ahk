#Requires AutoHotkey v2.0
#Include <stdlib\ahktest>
#Include "pillow.ahk"
#Include "..\..\2026-07-19-cnumpy-foundation\ahk\numpy.ahk"

; =============================================================================
; BEHAV-DEMO-001: every README/blog example, executed verbatim.
; The assertion names mirror the README/blog sections. Runs under the
; project runner; artifacts go to A_Temp.
; =============================================================================

PillowTestDemoDllPath() {
    SplitPath A_LineFile, , &ahkDir
    return ahkDir "\..\build\x64\Release\pillow_c.dll"
}

PillowTestDemoNumpyDllPath() {
    SplitPath A_LineFile, , &ahkDir
    return ahkDir "\..\..\2026-07-19-cnumpy-foundation\build\x64\Release\cnumpy_ahk.dll"
}

PillowTestDemoExampleRuns(*) {
    Pillow.Configure({ DllPath: PillowTestDemoDllPath() })
    Numpy.DllPath := PillowTestDemoNumpyDllPath()
    Numpy.Init()
    temp := A_Temp
    photoPng := temp "\pillow-c-demo-photo.png"
    photoJpg := temp "\pillow-c-demo-photo.jpg"

    ; README "Quick start": open, resize, save
    Pillow.Image.LinearGradient("L").Convert("RGB").Save(photoPng)
    img := Pillow.Image.Open(photoPng)
    img := img.Resize([800, 600])
    img.Save(temp "\pillow-c-demo-photo-800.jpg")
    img.Close()
    reopened := Pillow.Image.Open(temp "\pillow-c-demo-photo-800.jpg")
    AhkTest.AssertEqual([800, 600], reopened.Size)
    reopened.Close()

    ; README "Quick start": create and inspect
    canvas := Pillow.Image.New("RGB", [320, 200], "white")
    canvas.PutPixel([10, 10], "red")
    AhkTest.AssertEqual(3, canvas.GetPixel([10, 10]).Length)
    canvas.Close()

    ; README "Gradients and statistics"
    gray := Pillow.Image.LinearGradient("L")
    hist := gray.Histogram()
    ext := gray.GetExtrema()
    AhkTest.AssertEqual(256, hist.Length)
    AhkTest.AssertEqual([0, 255], ext)
    grad := Pillow.Image.RadialGradient("L")
    grad.Thumbnail([128, 128])
    grad.Save(temp "\pillow-c-demo-gradient.png")
    AhkTest.AssertTrue(FileExist(temp "\pillow-c-demo-gradient.png") != "")
    grad.Close()
    gray.Close()

    ; README "Drawing and filters": drawing
    draw := Pillow.Image.New("RGB", [240, 140], "white")
    d := Pillow.ImageDraw.Draw(draw)
    d.Line([[0, 0], [239, 0], [239, 139], [0, 139], [0, 0]], "blue", 3)
    d.RoundedRectangle([20, 20, 220, 120], 12, "yellow", "red", 2)
    d.Ellipse([60, 40, 180, 100], "lime", "black", 1)
    d.Text([12, 118], "Hello", "navy")
    draw.Save(temp "\pillow-c-demo-draw.png")
    AhkTest.AssertTrue(FileExist(temp "\pillow-c-demo-draw.png") != "")
    draw.Close()

    ; README "Drawing and filters": filters
    Pillow.Image.RadialGradient("L").Convert("RGB").Save(photoJpg, "JPEG")
    photo := Pillow.Image.Open(photoJpg)
    blurred := photo.Filter(Pillow.ImageFilter.GaussianBlur(2))
    sharp := photo.Filter(Pillow.ImageFilter.UnsharpMask(2, 150, 3))
    k := Pillow.ImageFilter.Kernel([3, 3], [-1,-1,-1, -1,8,-1, -1,-1,-1], 1, 0)
    edges := photo.Filter(k)
    AhkTest.AssertEqual(photo.Size[1], blurred.Size[1])
    AhkTest.AssertEqual(photo.Size[1], sharp.Size[1])
    AhkTest.AssertEqual(photo.Size[1], edges.Size[1])
    photo.Close()

    ; README/blog "Quantize"
    rgb := Pillow.Image.Open(photoJpg)
    p := rgb.Quantize(256, Pillow.Quantize.MEDIANCUT)
    p.Save(temp "\pillow-c-demo-photo-256.gif")
    AhkTest.AssertTrue(FileExist(temp "\pillow-c-demo-photo-256.gif") != "")
    gif := Pillow.Image.Open(temp "\pillow-c-demo-photo-256.gif")
    AhkTest.AssertEqual("P", gif.Mode)
    gif.Close()
    p.Close()
    rgb.Close()

    ; README "NumPy interop": image.AsArray() / Numpy.Mean
    img := Pillow.Image.Open(photoJpg)
    a := img.AsArray()
    AhkTest.AssertEqual([256, 256, 3], a.Shape)
    mean := a.Mean()
    AhkTest.AssertTrue(mean is Number)
    img.Close()
    AhkTest.AssertEqual([256, 256, 3], a.Shape)

    ; README "NumPy interop": FromArray
    b := Numpy.Zeros([64, 64, 4], 3)
    im := Pillow.Image.FromArray(b)
    AhkTest.AssertEqual("RGBA", im.Mode)
    im.Save(temp "\pillow-c-demo-black.png")
    AhkTest.AssertTrue(FileExist(temp "\pillow-c-demo-black.png") != "")
    im.Close()

    Numpy.Cleanup()
}

AhkTest.Test("Pillow demo example runs: README and blog examples execute verbatim", PillowTestDemoExampleRuns)
