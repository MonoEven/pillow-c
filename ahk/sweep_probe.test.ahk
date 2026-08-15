#Requires AutoHotkey v2.0
#Include D:\Tech\Projects\Autohotkey\Lib\visual_studio\tasks\2026-06-07-pillow-c-foundation\ahk\pillow.ahk
#Include D:\Tech\Projects\Autohotkey\Lib\visual_studio\tasks\2026-06-07-pillow-c-foundation\ahk\pillow.test.ahk

; Round-18 red-team sweep: the non-BMP cmap (format 12) glyphs and the GIF
; lossy-save palette surface against Pillow 11.3.0.
PillowTestRedTeamSweepTail(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    emoji := "C:\Windows\Fonts\seguiemj.ttf"
    if FileExist(emoji) {
        font := Pillow.ImageFont.Truetype(emoji, 24)
        try {
            ; format-12 (supplementary plane) cmap entries — Pillow pins
            AhkTest.AssertTrue(Abs(font.GetLength(Chr(0x1F600)) - 32.953125) < 0.0000001)
            AhkTest.AssertTrue(Abs(font.GetLength("a" Chr(0x1F600)) - 45.171875) < 0.0000001)
        } finally {
            font.Close()
        }
    }

    ; GIF lossy save: the colors are preserved in a 256-entry palette; the
    ; palette ORDER/index assignment is the documented quantizer divergence
    ; (Pillow runs median cut even for exactly-256-color sources).
    path := A_Temp "\pillow-sweep-" A_TickCount ".gif"
    im := Pillow.Image.New("RGB", [16, 16], [0, 0, 0])
    y := 0
    while y < 16 {
        x := 0
        while x < 16 {
            im.PutPixel([x, y], [x * 16, y * 16, (x + y) * 8])
            x++
        }
        y++
    }
    try {
        im.Save(path, "GIF")
        g := Pillow.Image.Open(path)
        try {
            AhkTest.AssertEqual(256, g.GetPalette().Length // 3)
            ; the first row maps to palette entries holding the exact
            ; source colors in gradient order
            AhkTest.AssertEqual([0, 1, 2, 3, 4, 5, 6, 7], [g.GetPixel([0, 0]), g.GetPixel([1, 0]), g.GetPixel([2, 0]), g.GetPixel([3, 0]), g.GetPixel([4, 0]), g.GetPixel([5, 0]), g.GetPixel([6, 0]), g.GetPixel([7, 0])])
            AhkTest.AssertEqual([0, 0, 0], PillowTestArraySlice(g.GetPalette(), 1, 3))
            AhkTest.AssertEqual([16, 0, 8], PillowTestArraySlice(g.GetPalette(), 4, 6))
        } finally {
            g.Close()
        }
    } finally {
        im.Close()
    }
    PillowTestDeleteFile(path)
}

AhkTest.Test("Pillow red-team sweep tail: non-BMP cmap and GIF lossy palette", PillowTestRedTeamSweepTail)
