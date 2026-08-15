#Requires AutoHotkey v2.0
; ---------------------------------------------------------------------------
; Example for the forum reply:
;   BOX downsampling = equal-weight area averaging.
;   NEAREST upsampling = no interpolation, preserving the blocky pixelated look.
;
; Run from the task directory:
;   AutoHotkey64.exe examples\pixelate_average.ahk
; ---------------------------------------------------------------------------
#Include "..\ahk\pillow.ahk"

Pillow.Configure({ DllPath: A_ScriptDir "\..\build\x64\Release\pillow_c.dll" })

failures := 0
Log(msg) {
    FileAppend msg "`n", A_ScriptDir "\pixelate_average.log"
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

; Build a 200x200 image with four solid 50x50 quadrants.
img := Pillow.Image.New("RGB", [200, 200], "white")
d := Pillow.ImageDraw.Draw(img)
d.Rectangle([0, 0, 99, 99], [255, 0, 0])        ; top-left
d.Rectangle([100, 0, 199, 99], [0, 255, 0])     ; top-right
d.Rectangle([0, 100, 99, 199], [0, 0, 255])     ; bottom-left
d.Rectangle([100, 100, 199, 199], [255, 255, 0]) ; bottom-right
img.Save(A_ScriptDir "\pixelate_original.png")

; BOX downsampling: 200x200 -> 4x4, each output pixel is one 50x50 region.
small := img.Resize([4, 4], Pillow.Resampling.BOX)
Check("BOX downsample produces 4x4", small.Width = 4 && small.Height = 4)

expected := [
    [[255, 0, 0],     [255, 0, 0],     [0, 255, 0],     [0, 255, 0]],
    [[255, 0, 0],     [255, 0, 0],     [0, 255, 0],     [0, 255, 0]],
    [[0, 0, 255],     [0, 0, 255],     [255, 255, 0],   [255, 255, 0]],
    [[0, 0, 255],     [0, 0, 255],     [255, 255, 0],   [255, 255, 0]]
]
allAverage := true
for y in [0, 1, 2, 3] {
    for x in [0, 1, 2, 3] {
        p := small.GetPixel([x, y])
        exp := expected[y + 1][x + 1]
        if !(p[1] = exp[1] && p[2] = exp[2] && p[3] = exp[3])
            allAverage := false
    }
}
Check("BOX pixels match 50x50 area averages", allAverage)

; NEAREST upsampling: 4x4 -> 200x200, no interpolation.
pixelated := small.Resize([200, 200], Pillow.Resampling.NEAREST)
Check("NEAREST upsample produces 200x200", pixelated.Width = 200 && pixelated.Height = 200)

; Each 50x50 block must be a single uniform color.
blockUniform := true
expectedBlocks := [
    [[255, 0, 0],     [0, 255, 0]],
    [[0, 0, 255],     [255, 255, 0]]
]
for by in [0, 1] {
    for bx in [0, 1] {
        x := bx * 100 + 10
        y := by * 100 + 10
        p := pixelated.GetPixel([x, y])
        exp := expectedBlocks[by + 1][bx + 1]
        if !(p[1] = exp[1] && p[2] = exp[2] && p[3] = exp[3])
            blockUniform := false
        ; Check another pixel inside the same block.
        q := pixelated.GetPixel([x + 40, y + 40])
        if !(q[1] = p[1] && q[2] = p[2] && q[3] = p[3])
            blockUniform := false
    }
}
Check("NEAREST blocks are uniform and match quadrant colors", blockUniform)

pixelated.Save(A_ScriptDir "\pixelated.png")

img.Close()
small.Close()
pixelated.Close()

if failures > 0 {
    Log("FAILURES: " failures)
    ExitApp(1)
}
Log("ALL CHECKS PASSED")
ExitApp(0)
