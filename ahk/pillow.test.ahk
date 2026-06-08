#Requires AutoHotkey v2.0
#Include <stdlib\ahktest>
#Include "pillow.ahk"

PillowTestDllPath() {
    SplitPath A_LineFile, , &ahkDir
    return ahkDir "\..\build\x64\Release\pillow_c.dll"
}

PillowTestBuffer(values) {
    buf := Buffer(values.Length, 0)
    for index, value in values
        NumPut("UChar", value, buf, index - 1)
    return buf
}

PillowTestBufferToArray(buf) {
    values := []
    loop buf.Size
        values.Push(NumGet(buf, A_Index - 1, "UChar"))
    return values
}

PillowTestArraySlice(values, firstIndex, lastIndex) {
    out := []
    loop lastIndex - firstIndex + 1
        out.Push(values[firstIndex + A_Index - 1])
    return out
}

PillowTestAssertArrayNear(expected, actual, tolerance) {
    AhkTest.AssertEqual(expected.Length, actual.Length)
    for index, expectedValue in expected
        AhkTest.AssertTrue(Abs(expectedValue - actual[index]) <= tolerance)
}

PillowTestWriteFileBytes(path, values) {
    buf := PillowTestBuffer(values)
    file := FileOpen(path, "w")
    try {
        file.RawWrite(buf, buf.Size)
    } finally {
        file.Close()
    }
}

PillowTestReadFileBytes(path) {
    file := FileOpen(path, "r")
    try {
        buf := Buffer(file.Length, 0)
        file.RawRead(buf, buf.Size)
        return PillowTestBufferToArray(buf)
    } finally {
        file.Close()
    }
}

PillowTestTempBmpPath(name) {
    return A_Temp "\pillow-ahk-" name "-" A_TickCount "-" Random(1, 1000000) ".bmp"
}

PillowTestTempPngPath(name) {
    return A_Temp "\pillow-ahk-" name "-" A_TickCount "-" Random(1, 1000000) ".png"
}

PillowTestTempJpegPath(name) {
    return A_Temp "\pillow-ahk-" name "-" A_TickCount "-" Random(1, 1000000) ".jpg"
}

PillowTestTempTiffPath(name) {
    return A_Temp "\pillow-ahk-" name "-" A_TickCount "-" Random(1, 1000000) ".tiff"
}

PillowTestTempGifPath(name) {
    return A_Temp "\pillow-ahk-" name "-" A_TickCount "-" Random(1, 1000000) ".gif"
}

PillowTestDeleteFile(path) {
    try FileDelete path
}

AhkTest.Test("Pillow facade exposes native ABI version", (*) => (
    Pillow.Configure({ DllPath: PillowTestDllPath() }),
    AhkTest.AssertEqual([0, 1, 0], Pillow.AbiVersion())
))

PillowTestImageNewCreatesModeAwareNativeImage(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("RGB", [2, 1])
    try {
        AhkTest.AssertEqual("RGB", image.Mode)
        AhkTest.AssertEqual([2, 1], image.Size)
        AhkTest.AssertEqual(2, image.Width)
        AhkTest.AssertEqual(1, image.Height)
        AhkTest.AssertEqual(3, image.Channels)
        AhkTest.AssertEqual(6, image.ByteSize)
        AhkTest.AssertEqual([0, 0, 0, 0, 0, 0], PillowTestBufferToArray(image.ToBytes()))
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow Image.New creates a mode-aware native image", PillowTestImageNewCreatesModeAwareNativeImage)

PillowTestImageNewWithColorUsesNativeFill(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.New("RGB", [3, 2], [10, 20, 30])
    l := Pillow.Image.New("L", [4, 1], 7)
    one := Pillow.Image.New("1", [4, 1], 256)
    try {
        AhkTest.AssertEqual([
            10, 20, 30, 10, 20, 30, 10, 20, 30,
            10, 20, 30, 10, 20, 30, 10, 20, 30,
        ], PillowTestBufferToArray(rgb.ToBytes()))
        AhkTest.AssertEqual([7, 7, 7, 7], PillowTestBufferToArray(l.ToBytes()))
        AhkTest.AssertEqual([0xF0], PillowTestBufferToArray(one.ToBytes()))

        one.Fill([256])
        AhkTest.AssertEqual([0xF0], PillowTestBufferToArray(one.ToBytes()))

        one.Fill([-1])
        AhkTest.AssertEqual([0x00], PillowTestBufferToArray(one.ToBytes()))
    } finally {
        one.Close()
        l.Close()
        rgb.Close()
    }
}

AhkTest.Test("Pillow Image.New fills a solid color through the native DLL", PillowTestImageNewWithColorUsesNativeFill)

PillowTestImageLinearGradientUsesNativeGenerator(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := 0
    one := 0
    p := 0
    try {
        l := Pillow.Image.LinearGradient("L")
        one := Pillow.Image.LinearGradient("1")
        p := Pillow.Image.LinearGradient("P")

        AhkTest.AssertEqual("L", l.Mode)
        AhkTest.AssertEqual([256, 256], l.Size)
        AhkTest.AssertEqual([0], [l.GetPixel([0, 0])])
        AhkTest.AssertEqual([127], [l.GetPixel([255, 127])])
        AhkTest.AssertEqual([255], [l.GetPixel([128, 255])])

        AhkTest.AssertEqual("1", one.Mode)
        AhkTest.AssertEqual([0], [one.GetPixel([0, 0])])
        AhkTest.AssertEqual([1], [one.GetPixel([0, 1])])
        oneBytes := PillowTestBufferToArray(one.ToBytes())
        AhkTest.AssertEqual(8192, oneBytes.Length)
        AhkTest.AssertEqual(0, oneBytes[1])
        AhkTest.AssertEqual(255, oneBytes[33])

        AhkTest.AssertEqual("P", p.Mode)
        AhkTest.AssertEqual([128], [p.GetPixel([10, 128])])
        AhkTest.AssertEqual([], p.GetPalette())
    } finally {
        for image in [p, one, l] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.LinearGradient creates native 256x256 gradients", PillowTestImageLinearGradientUsesNativeGenerator)

PillowTestImageRadialGradientUsesNativeGenerator(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := 0
    one := 0
    p := 0
    try {
        l := Pillow.Image.RadialGradient("L")
        one := Pillow.Image.RadialGradient("1")
        p := Pillow.Image.RadialGradient("P")

        AhkTest.AssertEqual("L", l.Mode)
        AhkTest.AssertEqual([256, 256], l.Size)
        AhkTest.AssertEqual([255], [l.GetPixel([0, 0])])
        AhkTest.AssertEqual([0], [l.GetPixel([128, 128])])
        AhkTest.AssertEqual([1], [l.GetPixel([129, 128])])
        AhkTest.AssertEqual([179], [l.GetPixel([255, 128])])
        AhkTest.AssertEqual([254], [l.GetPixel([255, 255])])

        AhkTest.AssertEqual("1", one.Mode)
        AhkTest.AssertEqual([0], [one.GetPixel([128, 128])])
        AhkTest.AssertEqual([1], [one.GetPixel([129, 128])])
        oneBytes := PillowTestBufferToArray(one.ToBytes())
        AhkTest.AssertEqual(8192, oneBytes.Length)

        AhkTest.AssertEqual("P", p.Mode)
        AhkTest.AssertEqual([90], [p.GetPixel([64, 128])])
        AhkTest.AssertEqual([], p.GetPalette())
    } finally {
        for image in [p, one, l] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.RadialGradient creates native 256x256 gradients", PillowTestImageRadialGradientUsesNativeGenerator)

PillowTestImageEffectMandelbrotUsesNativeGenerator(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := 0
    empty := 0
    try {
        image := Pillow.Image.EffectMandelbrot([4, 3], [-2.0, -1.0, 1.0, 1.0], 10)
        empty := Pillow.Image.EffectMandelbrot([0, 1], [-2.0, -1.0, 1.0, 1.0], 10)

        AhkTest.AssertEqual("L", image.Mode)
        AhkTest.AssertEqual([4, 3], image.Size)
        AhkTest.AssertEqual([76, 102, 0, 102, 0, 0, 0, 102, 76, 102, 0, 102], PillowTestBufferToArray(image.ToBytes()))
        AhkTest.AssertEqual("L", empty.Mode)
        AhkTest.AssertEqual([0, 1], empty.Size)
        AhkTest.AssertEqual([], PillowTestBufferToArray(empty.ToBytes()))

        errorWasRaised := false
        try {
            Pillow.Image.EffectMandelbrot([2, 2], [1.0, 0.0, -1.0, 1.0], 10)
        } catch Error as err {
            errorWasRaised := InStr(err.Message, "invalid argument") > 0
        }
        AhkTest.AssertTrue(errorWasRaised)
    } finally {
        for item in [empty, image] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow Image.EffectMandelbrot creates native L images", PillowTestImageEffectMandelbrotUsesNativeGenerator)

PillowTestImageEffectNoiseUsesNativeGenerator(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := 0
    zeroSigma := 0
    empty := 0
    try {
        image := Pillow.Image.EffectNoise([4, 3], 12.5)
        zeroSigma := Pillow.Image.EffectNoise([3, 1], 0.0)
        empty := Pillow.Image.EffectNoise([0, 1], 12.5)

        AhkTest.AssertEqual("L", image.Mode)
        AhkTest.AssertEqual([4, 3], image.Size)
        AhkTest.AssertEqual([121, 160, 124, 137, 125, 151, 118, 124, 131, 141, 147, 115], PillowTestBufferToArray(image.ToBytes()))
        AhkTest.AssertEqual("L", zeroSigma.Mode)
        AhkTest.AssertEqual([128, 128, 128], PillowTestBufferToArray(zeroSigma.ToBytes()))
        AhkTest.AssertEqual("L", empty.Mode)
        AhkTest.AssertEqual([0, 1], empty.Size)
        AhkTest.AssertEqual([], PillowTestBufferToArray(empty.ToBytes()))

        errorWasRaised := false
        try {
            Pillow.Image.EffectNoise("bad-size", 12.5)
        } catch Error {
            errorWasRaised := true
        }
        AhkTest.AssertTrue(errorWasRaised)
    } finally {
        for item in [empty, zeroSigma, image] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow Image.EffectNoise creates native L images", PillowTestImageEffectNoiseUsesNativeGenerator)

PillowTestImageEffectSpreadUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        0, 1, 2, 3, 4, 5, 6, 7, 8,
        9, 10, 11, 12, 13, 14, 15, 16, 17,
    ]))
    p := Pillow.Image.FromBytes("P", [3, 2], PillowTestBuffer([0, 1, 2, 3, 4, 5]))
    lOut := 0
    rgbOut := 0
    pOut := 0
    identity := 0
    try {
        p.PutPalette([10, 20, 30, 40, 50, 60, 70, 80, 90])

        DllCall("ucrtbase\srand", "UInt", 1, "Cdecl")
        lOut := l.EffectSpread(2)
        DllCall("ucrtbase\srand", "UInt", 1, "Cdecl")
        rgbOut := Pillow.Image.EffectSpread(rgb, 2)
        DllCall("ucrtbase\srand", "UInt", 1, "Cdecl")
        pOut := p.EffectSpread(2)
        identity := l.EffectSpread(0)

        AhkTest.AssertEqual("L", lOut.Mode)
        AhkTest.AssertEqual([4, 3], lOut.Size)
        AhkTest.AssertEqual([0, 1, 2, 3, 8, 9, 10, 7, 4, 5, 11, 10], PillowTestBufferToArray(lOut.ToBytes()))
        AhkTest.AssertEqual("RGB", rgbOut.Mode)
        AhkTest.AssertEqual([
            12, 13, 14, 3, 4, 5, 6, 7, 8,
            9, 10, 11, 0, 1, 2, 15, 16, 17,
        ], PillowTestBufferToArray(rgbOut.ToBytes()))
        AhkTest.AssertEqual("P", pOut.Mode)
        AhkTest.AssertEqual([4, 1, 2, 3, 0, 5], PillowTestBufferToArray(pOut.ToBytes()))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90], pOut.GetPalette())
        AhkTest.AssertEqual([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11], PillowTestBufferToArray(identity.ToBytes()))
        AhkTest.AssertEqual([0, 1, 2, 3, 4, 5], PillowTestBufferToArray(p.ToBytes()))

        errorWasRaised := false
        try {
            l.EffectSpread(-1)
        } catch Error as err {
            errorWasRaised := InStr(err.Message, "invalid argument") > 0
        }
        AhkTest.AssertTrue(errorWasRaised)
    } finally {
        for item in [identity, pOut, rgbOut, lOut, p, rgb, l] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow Image.EffectSpread uses native path", PillowTestImageEffectSpreadUsesNativePath)

PillowTestImageFromBytesOwnsNativeCopy(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    data := PillowTestBuffer([10, 20, 30, 40, 50, 60])
    image := Pillow.Image.FromBytes("RGB", [2, 1], data)
    try {
        NumPut("UChar", 99, data, 0)
        AhkTest.AssertEqual("RGB", image.Mode)
        AhkTest.AssertEqual([2, 1], image.Size)
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60], PillowTestBufferToArray(image.ToBytes()))
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow Image.FromBytes copies caller bytes into native storage", PillowTestImageFromBytesOwnsNativeCopy)

PillowTestImageSaveAndOpenBmpUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("RGB", [2, 2], PillowTestBuffer([
        10, 20, 30, 40, 50, 60,
        70, 80, 90, 100, 110, 120
    ]))
    savedPath := PillowTestTempBmpPath("save-open")
    oraclePath := PillowTestTempBmpPath("oracle-open")
    loaded := 0
    try {
        image.Save(savedPath, "BMP")
        AhkTest.AssertEqual([
            66, 77, 70, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,
            40, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 1, 0, 24, 0,
            0, 0, 0, 0, 16, 0, 0, 0, 196, 14, 0, 0, 196, 14, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            90, 80, 70, 120, 110, 100, 0, 0,
            30, 20, 10, 60, 50, 40, 0, 0
        ], PillowTestReadFileBytes(savedPath))

        PillowTestWriteFileBytes(oraclePath, PillowTestReadFileBytes(savedPath))
        loaded := Pillow.Image.Open(oraclePath)

        AhkTest.AssertEqual("RGB", loaded.Mode)
        AhkTest.AssertEqual([2, 2], loaded.Size)
        AhkTest.AssertEqual([
            10, 20, 30, 40, 50, 60,
            70, 80, 90, 100, 110, 120
        ], PillowTestBufferToArray(loaded.ToBytes()))
    } finally {
        if IsObject(loaded)
            loaded.Close()
        PillowTestDeleteFile(oraclePath)
        PillowTestDeleteFile(savedPath)
        image.Close()
    }
}

AhkTest.Test("Pillow Image.Save and Image.Open support BMP through native path", PillowTestImageSaveAndOpenBmpUsesNativePath)

PillowTestImageOpenSaveBmpRejectsUnsupportedInputs(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("CMYK", [1, 1], PillowTestBuffer([1, 2, 3, 4]))
    badPath := PillowTestTempBmpPath("bad")
    try {
        try {
            image.Save(badPath, "NO_SUCH_FORMAT")
            AhkTest.Fail("Expected Image.Save to reject unsupported format")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "unsupported") > 0)
        }

        try {
            image.Save(badPath, "BMP")
            AhkTest.Fail("Expected Image.Save to reject unsupported CMYK BMP")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }

        PillowTestWriteFileBytes(badPath, [1, 2, 3, 4])
        try {
            Pillow.Image.Open(badPath)
            AhkTest.Fail("Expected Image.Open to reject invalid BMP")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }
    } finally {
        PillowTestDeleteFile(badPath)
        image.Close()
    }
}

AhkTest.Test("Pillow Image.Open and Save BMP reject unsupported inputs", PillowTestImageOpenSaveBmpRejectsUnsupportedInputs)

PillowTestImageSaveAndOpenPngUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [2, 2], PillowTestBuffer([
        10, 20, 30, 40, 50, 60,
        70, 80, 90, 100, 110, 120
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([
        10, 20, 30, 40, 50, 60, 70, 80
    ]))
    savedPath := PillowTestTempPngPath("save-open")
    oraclePath := PillowTestTempPngPath("oracle-open")
    loaded := 0
    loadedRgba := 0
    try {
        rgb.Save(savedPath, "PNG")
        AhkTest.AssertEqual([137, 80, 78, 71, 13, 10, 26, 10], PillowTestArraySlice(PillowTestReadFileBytes(savedPath), 1, 8))
        loaded := Pillow.Image.Open(savedPath)

        AhkTest.AssertEqual("RGB", loaded.Mode)
        AhkTest.AssertEqual([2, 2], loaded.Size)
        AhkTest.AssertEqual([
            10, 20, 30, 40, 50, 60,
            70, 80, 90, 100, 110, 120
        ], PillowTestBufferToArray(loaded.ToBytes()))

        PillowTestWriteFileBytes(oraclePath, [
            137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
            0, 0, 0, 2, 0, 0, 0, 1, 8, 6, 0, 0, 0, 244, 34, 127, 138,
            0, 0, 0, 14, 73, 68, 65, 84, 120, 156, 99, 228, 18, 145, 211,
            0, 1, 0, 3, 250, 1, 6, 247, 12, 146, 55, 0, 0, 0, 0,
            73, 69, 78, 68, 174, 66, 96, 130
        ])
        loadedRgba := Pillow.Image.Open(oraclePath, ["PNG"])
        AhkTest.AssertEqual("RGBA", loadedRgba.Mode)
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80], PillowTestBufferToArray(loadedRgba.ToBytes()))

        rgba.Save(savedPath)
        if IsObject(loadedRgba)
            loadedRgba.Close()
        loadedRgba := Pillow.Image.Open(savedPath)
        AhkTest.AssertEqual("RGBA", loadedRgba.Mode)
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80], PillowTestBufferToArray(loadedRgba.ToBytes()))
    } finally {
        for image in [loadedRgba, loaded, rgba, rgb] {
            if IsObject(image)
                image.Close()
        }
        PillowTestDeleteFile(oraclePath)
        PillowTestDeleteFile(savedPath)
    }
}

AhkTest.Test("Pillow Image.Save and Image.Open support PNG through native path", PillowTestImageSaveAndOpenPngUsesNativePath)

PillowTestImageOpenSavePngRejectsUnsupportedInputs(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("CMYK", [1, 1], PillowTestBuffer([1, 2, 3, 4]))
    badPath := PillowTestTempPngPath("bad")
    try {
        try {
            image.Save(badPath, "PNG")
            AhkTest.Fail("Expected Image.Save to reject unsupported CMYK PNG")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }

        PillowTestWriteFileBytes(badPath, [1, 2, 3, 4])
        try {
            Pillow.Image.Open(badPath, ["PNG"])
            AhkTest.Fail("Expected Image.Open to reject invalid PNG")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }
    } finally {
        PillowTestDeleteFile(badPath)
        image.Close()
    }
}

AhkTest.Test("Pillow Image.Open and Save PNG reject unsupported inputs", PillowTestImageOpenSavePngRejectsUnsupportedInputs)

PillowTestImageOpenPngPreservesLaAndPaletteModes(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    laPath := PillowTestTempPngPath("open-la")
    pPath := PillowTestTempPngPath("open-p")
    la := 0
    p := 0
    try {
        PillowTestWriteFileBytes(laPath, [
            137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
            0, 0, 0, 2, 0, 0, 0, 2, 8, 4, 0, 0, 0, 216, 191, 197, 175,
            0, 0, 0, 18, 73, 68, 65, 84, 120, 156, 99, 228, 18, 17, 17,
            97, 209, 208, 16, 17, 1, 0, 3, 122, 0, 196, 80, 243, 202, 125,
            0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130
        ])
        PillowTestWriteFileBytes(pPath, [
            137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
            0, 0, 0, 2, 0, 0, 0, 2, 2, 3, 0, 0, 0, 15, 216, 229, 183,
            0, 0, 0, 12, 80, 76, 84, 69, 10, 20, 30, 40, 50, 60, 70,
            80, 90, 100, 110, 120, 198, 72, 119, 223, 0, 0, 0, 12, 73,
            68, 65, 84, 120, 156, 99, 16, 96, 216, 0, 0, 0, 228, 0, 193,
            39, 168, 232, 87, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130
        ])

        la := Pillow.Image.Open(laPath, ["PNG"])
        p := Pillow.Image.Open(pPath, ["PNG"])

        AhkTest.AssertEqual("LA", la.Mode)
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80], PillowTestBufferToArray(la.ToBytes()))
        AhkTest.AssertEqual("P", p.Mode)
        AhkTest.AssertEqual([0, 1, 2, 3], PillowTestBufferToArray(p.ToBytes()))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120], p.GetPalette())
    } finally {
        for image in [p, la] {
            if IsObject(image)
                image.Close()
        }
        for path in [pPath, laPath]
            PillowTestDeleteFile(path)
    }
}

AhkTest.Test("Pillow Image.Open PNG preserves LA and P modes", PillowTestImageOpenPngPreservesLaAndPaletteModes)

PillowTestImageSavePngPreservesLaAndPaletteModes(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    la := Pillow.Image.FromBytes("LA", [2, 2], PillowTestBuffer([10, 20, 30, 40, 50, 60, 70, 80]))
    p := Pillow.Image.FromBytes("P", [2, 2], PillowTestBuffer([0, 1, 2, 3]))
    palette := [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120]
    laPath := PillowTestTempPngPath("save-la")
    pPath := PillowTestTempPngPath("save-p")
    laLoaded := 0
    pLoaded := 0
    try {
        p.PutPalette(palette)
        la.Save(laPath, "PNG")
        p.Save(pPath, "PNG")
        laLoaded := Pillow.Image.Open(laPath)
        pLoaded := Pillow.Image.Open(pPath)

        AhkTest.AssertEqual("LA", laLoaded.Mode)
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80], PillowTestBufferToArray(laLoaded.ToBytes()))
        AhkTest.AssertEqual("P", pLoaded.Mode)
        AhkTest.AssertEqual([0, 1, 2, 3], PillowTestBufferToArray(pLoaded.ToBytes()))
        AhkTest.AssertEqual(palette, pLoaded.GetPalette())
    } finally {
        for image in [pLoaded, laLoaded, p, la] {
            if IsObject(image)
                image.Close()
        }
        for path in [pPath, laPath]
            PillowTestDeleteFile(path)
    }
}

AhkTest.Test("Pillow Image.Save PNG preserves LA and P modes", PillowTestImageSavePngPreservesLaAndPaletteModes)

PillowTestImageSaveAndOpenJpegUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("RGB", [1, 1], PillowTestBuffer([120, 130, 140]))
    savedPath := PillowTestTempJpegPath("save-open")
    oraclePath := PillowTestTempJpegPath("oracle-open")
    loaded := 0
    oracle := 0
    try {
        image.Save(savedPath, "JPEG")
        AhkTest.AssertEqual([255, 216], PillowTestArraySlice(PillowTestReadFileBytes(savedPath), 1, 2))
        loaded := Pillow.Image.Open(savedPath)
        AhkTest.AssertEqual("RGB", loaded.Mode)
        PillowTestAssertArrayNear([120, 130, 140], PillowTestBufferToArray(loaded.ToBytes()), 8)

        PillowTestWriteFileBytes(oraclePath, [
            255, 216, 255, 224, 0, 16, 74, 70, 73, 70, 0, 1, 1, 0, 0, 1,
            0, 1, 0, 0, 255, 219, 0, 67, 0, 8, 6, 6, 7, 6, 5, 8,
            7, 7, 7, 9, 9, 8, 10, 12, 20, 13, 12, 11, 11, 12, 25, 18,
            19, 15, 20, 29, 26, 31, 30, 29, 26, 28, 28, 32, 36, 46, 39, 32,
            34, 44, 35, 28, 28, 40, 55, 41, 44, 48, 49, 52, 52, 52, 31, 39,
            57, 61, 56, 50, 60, 46, 51, 52, 50, 255, 219, 0, 67, 1, 9, 9,
            9, 12, 11, 12, 24, 13, 13, 24, 50, 33, 28, 33, 50, 50, 50, 50,
            50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
            50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
            50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 255, 192,
            0, 17, 8, 0, 1, 0, 1, 3, 1, 34, 0, 2, 17, 1, 3, 17,
            1, 255, 196, 0, 31, 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
            10, 11, 255, 196, 0, 181, 16, 0, 2, 1, 3, 3, 2, 4, 3, 5,
            5, 4, 4, 0, 0, 1, 125, 1, 2, 3, 0, 4, 17, 5, 18, 33,
            49, 65, 6, 19, 81, 97, 7, 34, 113, 20, 50, 129, 145, 161, 8, 35,
            66, 177, 193, 21, 82, 209, 240, 36, 51, 98, 114, 130, 9, 10, 22, 23,
            24, 25, 26, 37, 38, 39, 40, 41, 42, 52, 53, 54, 55, 56, 57, 58,
            67, 68, 69, 70, 71, 72, 73, 74, 83, 84, 85, 86, 87, 88, 89, 90,
            99, 100, 101, 102, 103, 104, 105, 106, 115, 116, 117, 118, 119, 120, 121, 122,
            131, 132, 133, 134, 135, 136, 137, 138, 146, 147, 148, 149, 150, 151, 152, 153,
            154, 162, 163, 164, 165, 166, 167, 168, 169, 170, 178, 179, 180, 181, 182, 183,
            184, 185, 186, 194, 195, 196, 197, 198, 199, 200, 201, 202, 210, 211, 212, 213,
            214, 215, 216, 217, 218, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 241,
            242, 243, 244, 245, 246, 247, 248, 249, 250, 255, 196, 0, 31, 1, 0, 3,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1,
            2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 255, 196, 0, 181, 17, 0,
            2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119, 0,
            1, 2, 3, 17, 4, 5, 33, 49, 6, 18, 65, 81, 7, 97, 113, 19,
            34, 50, 129, 8, 20, 66, 145, 161, 177, 193, 9, 35, 51, 82, 240, 21,
            98, 114, 209, 10, 22, 36, 52, 225, 37, 241, 23, 24, 25, 26, 38, 39,
            40, 41, 42, 53, 54, 55, 56, 57, 58, 67, 68, 69, 70, 71, 72, 73,
            74, 83, 84, 85, 86, 87, 88, 89, 90, 99, 100, 101, 102, 103, 104, 105,
            106, 115, 116, 117, 118, 119, 120, 121, 122, 130, 131, 132, 133, 134, 135, 136,
            137, 138, 146, 147, 148, 149, 150, 151, 152, 153, 154, 162, 163, 164, 165, 166,
            167, 168, 169, 170, 178, 179, 180, 181, 182, 183, 184, 185, 186, 194, 195, 196,
            197, 198, 199, 200, 201, 202, 210, 211, 212, 213, 214, 215, 216, 217, 218, 226,
            227, 228, 229, 230, 231, 232, 233, 234, 242, 243, 244, 245, 246, 247, 248, 249,
            250, 255, 218, 0, 12, 3, 1, 0, 2, 17, 3, 17, 0, 63, 0, 40,
            162, 138, 216, 200, 255, 217
        ])
        oracle := Pillow.Image.Open(oraclePath, ["JPEG"])
        AhkTest.AssertEqual("RGB", oracle.Mode)
        PillowTestAssertArrayNear([120, 130, 140], PillowTestBufferToArray(oracle.ToBytes()), 2)
    } finally {
        for item in [oracle, loaded, image] {
            if IsObject(item)
                item.Close()
        }
        PillowTestDeleteFile(oraclePath)
        PillowTestDeleteFile(savedPath)
    }
}

AhkTest.Test("Pillow Image.Save and Image.Open support JPEG through native path", PillowTestImageSaveAndOpenJpegUsesNativePath)

PillowTestImageOpenSaveJpegRejectsUnsupportedInputs(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("RGBA", [1, 1], PillowTestBuffer([1, 2, 3, 4]))
    badPath := PillowTestTempJpegPath("bad")
    try {
        try {
            image.Save(badPath, "JPEG")
            AhkTest.Fail("Expected Image.Save to reject unsupported RGBA JPEG")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }

        PillowTestWriteFileBytes(badPath, [1, 2, 3, 4])
        try {
            Pillow.Image.Open(badPath, ["JPEG"])
            AhkTest.Fail("Expected Image.Open to reject invalid JPEG")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }
    } finally {
        PillowTestDeleteFile(badPath)
        image.Close()
    }
}

AhkTest.Test("Pillow Image.Open and Save JPEG reject unsupported inputs", PillowTestImageOpenSaveJpegRejectsUnsupportedInputs)

PillowTestImageSaveAndOpenTiffUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [2, 2], PillowTestBuffer([10, 40, 70, 100]))
    rgb := Pillow.Image.FromBytes("RGB", [2, 2], PillowTestBuffer([
        10, 20, 30, 40, 50, 60,
        70, 80, 90, 100, 110, 120
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([10, 20, 30, 40, 50, 60, 70, 80]))
    lPath := PillowTestTempTiffPath("save-l")
    rgbPath := A_Temp "\pillow-ahk-save-rgb-" A_TickCount "-" Random(1, 1000000) ".tif"
    rgbaPath := PillowTestTempTiffPath("save-rgba")
    loadedL := 0
    loadedRgb := 0
    loadedRgba := 0
    try {
        l.Save(lPath, "TIFF")
        rgb.Save(rgbPath, "TIF")
        rgba.Save(rgbaPath)
        for path in [lPath, rgbPath, rgbaPath]
            AhkTest.AssertEqual([73, 73, 42, 0], PillowTestArraySlice(PillowTestReadFileBytes(path), 1, 4))

        loadedL := Pillow.Image.Open(lPath)
        loadedRgb := Pillow.Image.Open(rgbPath)
        loadedRgba := Pillow.Image.Open(rgbaPath, ["TIFF"])
        AhkTest.AssertEqual("L", loadedL.Mode)
        AhkTest.AssertEqual([10, 40, 70, 100], PillowTestBufferToArray(loadedL.ToBytes()))
        AhkTest.AssertEqual("RGB", loadedRgb.Mode)
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120], PillowTestBufferToArray(loadedRgb.ToBytes()))
        AhkTest.AssertEqual("RGBA", loadedRgba.Mode)
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80], PillowTestBufferToArray(loadedRgba.ToBytes()))
    } finally {
        for image in [loadedRgba, loadedRgb, loadedL, rgba, rgb, l] {
            if IsObject(image)
                image.Close()
        }
        for path in [rgbaPath, rgbPath, lPath]
            PillowTestDeleteFile(path)
    }
}

AhkTest.Test("Pillow Image.Save and Image.Open support TIFF through native path", PillowTestImageSaveAndOpenTiffUsesNativePath)

PillowTestImageOpenSaveTiffRejectsUnsupportedInputs(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("CMYK", [1, 1], PillowTestBuffer([1, 2, 3, 4]))
    badPath := PillowTestTempTiffPath("bad")
    try {
        try {
            image.Save(badPath, "TIFF")
            AhkTest.Fail("Expected Image.Save to reject unsupported CMYK TIFF")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }

        PillowTestWriteFileBytes(badPath, [1, 2, 3, 4])
        try {
            Pillow.Image.Open(badPath, ["TIFF"])
            AhkTest.Fail("Expected Image.Open to reject invalid TIFF")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }
    } finally {
        PillowTestDeleteFile(badPath)
        image.Close()
    }
}

AhkTest.Test("Pillow Image.Open and Save TIFF reject unsupported inputs", PillowTestImageOpenSaveTiffRejectsUnsupportedInputs)

PillowTestImageSaveAndOpenGifUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("P", [2, 2], PillowTestBuffer([0, 1, 2, 3]))
    palette := [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120]
    savedPath := PillowTestTempGifPath("save-open")
    oraclePath := PillowTestTempGifPath("oracle-open")
    loaded := 0
    oracle := 0
    try {
        image.PutPalette(palette)
        image.Save(savedPath, "GIF")
        AhkTest.AssertEqual([71, 73, 70], PillowTestArraySlice(PillowTestReadFileBytes(savedPath), 1, 3))
        loaded := Pillow.Image.Open(savedPath)
        AhkTest.AssertEqual("P", loaded.Mode)
        AhkTest.AssertEqual([0, 1, 2, 3], PillowTestBufferToArray(loaded.ToBytes()))
        AhkTest.AssertEqual(palette, loaded.GetPalette())

        PillowTestWriteFileBytes(oraclePath, [
            71, 73, 70, 56, 55, 97, 2, 0, 2, 0, 129, 0, 0, 10, 20, 30,
            40, 50, 60, 70, 80, 90, 100, 110, 120, 44, 0, 0, 0, 0, 2, 0,
            2, 0, 0, 8, 7, 0, 1, 4, 16, 48, 32, 32, 0, 59
        ])
        oracle := Pillow.Image.Open(oraclePath, ["GIF"])
        AhkTest.AssertEqual("P", oracle.Mode)
        AhkTest.AssertEqual([0, 1, 2, 3], PillowTestBufferToArray(oracle.ToBytes()))
        AhkTest.AssertEqual(palette, oracle.GetPalette())
    } finally {
        for item in [oracle, loaded, image] {
            if IsObject(item)
                item.Close()
        }
        PillowTestDeleteFile(oraclePath)
        PillowTestDeleteFile(savedPath)
    }
}

AhkTest.Test("Pillow Image.Save and Image.Open support GIF through native path", PillowTestImageSaveAndOpenGifUsesNativePath)

PillowTestImageOpenSaveGifRejectsUnsupportedInputs(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("RGB", [1, 1], PillowTestBuffer([1, 2, 3]))
    badPath := PillowTestTempGifPath("bad")
    try {
        try {
            image.Save(badPath, "GIF")
            AhkTest.Fail("Expected Image.Save to reject unsupported RGB GIF")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }

        PillowTestWriteFileBytes(badPath, [1, 2, 3, 4])
        try {
            Pillow.Image.Open(badPath, ["GIF"])
            AhkTest.Fail("Expected Image.Open to reject invalid GIF")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }
    } finally {
        PillowTestDeleteFile(badPath)
        image.Close()
    }
}

AhkTest.Test("Pillow Image.Open and Save GIF reject unsupported inputs", PillowTestImageOpenSaveGifRejectsUnsupportedInputs)

PillowTestImageDrawRectangleMutatesImageThroughNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.New("L", [6, 5])
    rgb := Pillow.Image.New("RGB", [4, 3])
    try {
        draw := Pillow.ImageDraw.Draw(l)
        returned := draw.Rectangle([1, 1, 4, 3], 7, 9, 2)
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 9, 9, 9, 9, 0,
            0, 9, 9, 9, 9, 0,
            0, 9, 9, 9, 9, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(l.ToBytes()))

        Pillow.ImageDraw.Draw(rgb).Rectangle([1, 0, 3, 2], [10, 20, 30], [90, 80, 70], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 10, 20, 30, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70,
        ], PillowTestBufferToArray(rgb.ToBytes()))
    } finally {
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Rectangle mutates images through native path", PillowTestImageDrawRectangleMutatesImageThroughNativePath)

PillowTestImageDrawRectangleClipsAndRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("L", [6, 5])
    try {
        draw := Pillow.ImageDraw.Draw(image)
        draw.Rectangle([-1, -1, 2, 2], 7, 9)
        AhkTest.AssertEqual([
            7, 7, 9, 0, 0, 0,
            7, 7, 9, 0, 0, 0,
            9, 9, 9, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(image.ToBytes()))

        before := PillowTestBufferToArray(image.ToBytes())
        draw.Rectangle([0, 0, 5, 4], unset, 3, 0)
        AhkTest.AssertEqual(before, PillowTestBufferToArray(image.ToBytes()))

        try {
            draw.Rectangle([2, 2, 1, 1], unset, 9)
            AhkTest.Fail("Expected ImageDraw.Rectangle to reject reversed coordinates")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Rectangle clips and rejects invalid arguments", PillowTestImageDrawRectangleClipsAndRejectsInvalidArguments)

PillowTestImageDrawEllipseMutatesImagesThroughNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.New("L", [7, 6])
    rgb := Pillow.Image.New("RGB", [5, 4])
    try {
        draw := Pillow.ImageDraw.Draw(l)
        returned := draw.Ellipse([1, 1, 5, 4], 7, 9, 2)
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 9, 9, 9, 9, 9, 0,
            0, 9, 9, 9, 9, 9, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(l.ToBytes()))

        Pillow.ImageDraw.Draw(rgb).Ellipse([0, 0, 4, 3], [10, 20, 30], [90, 80, 70], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
            90, 80, 70, 10, 20, 30, 10, 20, 30, 10, 20, 30, 90, 80, 70,
            90, 80, 70, 10, 20, 30, 10, 20, 30, 10, 20, 30, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
        ], PillowTestBufferToArray(rgb.ToBytes()))
    } finally {
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Ellipse mutates images through native path", PillowTestImageDrawEllipseMutatesImagesThroughNativePath)

PillowTestImageDrawEllipseClipsAndRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("L", [6, 5])
    widthZero := Pillow.Image.New("L", [4, 4])
    try {
        draw := Pillow.ImageDraw.Draw(image)
        draw.Ellipse([-2, -1, 3, 3], 6)
        AhkTest.AssertEqual([
            6, 6, 6, 6, 0, 0,
            6, 6, 6, 6, 0, 0,
            6, 6, 6, 6, 0, 0,
            6, 6, 6, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(image.ToBytes()))

        Pillow.ImageDraw.Draw(widthZero).Ellipse([0, 0, 3, 3], unset, 5, 0)
        AhkTest.AssertEqual([
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ], PillowTestBufferToArray(widthZero.ToBytes()))

        try {
            draw.Ellipse([2, 2, 1, 1], unset, 9)
            AhkTest.Fail("Expected ImageDraw.Ellipse to reject reversed coordinates")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }
    } finally {
        widthZero.Close()
        image.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Ellipse clips and rejects invalid arguments", PillowTestImageDrawEllipseClipsAndRejectsInvalidArguments)

PillowTestImageDrawCircleUsesNativeEllipsePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.New("L", [7, 7])
    clipped := Pillow.Image.New("L", [5, 5])
    rgb := Pillow.Image.New("RGB", [5, 5])
    fractional := Pillow.Image.New("L", [6, 6])
    try {
        draw := Pillow.ImageDraw.Draw(l)
        returned := draw.Circle([3, 3], 2, 7, 9, 2)
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 9, 9, 9, 9, 9, 0,
            0, 9, 9, 7, 9, 9, 0,
            0, 9, 9, 9, 9, 9, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(l.ToBytes()))

        Pillow.ImageDraw.Draw(clipped).Circle([0, 0], 3, 6)
        AhkTest.AssertEqual([
            6, 6, 6, 6, 0,
            6, 6, 6, 6, 0,
            6, 6, 6, 0, 0,
            6, 6, 0, 0, 0,
            0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(clipped.ToBytes()))

        Pillow.ImageDraw.Draw(rgb).Circle([2, 2], 2, [10, 20, 30], [90, 80, 70], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
            90, 80, 70, 10, 20, 30, 10, 20, 30, 10, 20, 30, 90, 80, 70,
            90, 80, 70, 10, 20, 30, 10, 20, 30, 10, 20, 30, 90, 80, 70,
            90, 80, 70, 10, 20, 30, 10, 20, 30, 10, 20, 30, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
        ], PillowTestBufferToArray(rgb.ToBytes()))

        Pillow.ImageDraw.Draw(fractional).Circle([2, 2], 1.5, 4, 8, 1)
        AhkTest.AssertEqual([
            0, 8, 8, 0, 0, 0,
            8, 4, 4, 8, 0, 0,
            8, 4, 4, 8, 0, 0,
            0, 8, 8, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(fractional.ToBytes()))
    } finally {
        fractional.Close()
        rgb.Close()
        clipped.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Circle uses native ellipse path", PillowTestImageDrawCircleUsesNativeEllipsePath)

PillowTestImageDrawCircleRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("L", [5, 5])
    try {
        draw := Pillow.ImageDraw.Draw(image)
        for args in [
            [[2], 1, "center"],
            [[2, "x"], 1, "numeric"],
            [[2, 2], "x", "radius"],
            ["x", 1, "center"],
        ] {
            try {
                draw.Circle(args[1], args[2], 1)
                AhkTest.Fail("Expected ImageDraw.Circle to reject invalid arguments")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, args[3]) > 0)
            }
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Circle rejects invalid arguments", PillowTestImageDrawCircleRejectsInvalidArguments)

PillowTestImageDrawArcMutatesImagesThroughNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.New("L", [7, 7])
    wide := Pillow.Image.New("L", [7, 7])
    rgb := Pillow.Image.New("RGB", [5, 4])
    try {
        draw := Pillow.ImageDraw.Draw(l)
        returned := draw.Arc([1, 1, 5, 5], 0, 90, 7, 1)
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 7, 0,
            0, 0, 0, 0, 0, 7, 0,
            0, 0, 0, 7, 7, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(l.ToBytes()))

        Pillow.ImageDraw.Draw(wide).Arc([1, 1, 5, 5], 0, 270, 8, 2)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 8, 8, 0, 0, 0,
            0, 8, 8, 8, 0, 0, 0,
            0, 8, 8, 0, 8, 8, 0,
            0, 8, 8, 8, 8, 8, 0,
            0, 0, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(wide.ToBytes()))

        Pillow.ImageDraw.Draw(rgb).Arc([0, 0, 4, 3], 0, 180, [10, 20, 30], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            10, 20, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 20, 30,
            0, 0, 0, 10, 20, 30, 10, 20, 30, 10, 20, 30, 0, 0, 0,
        ], PillowTestBufferToArray(rgb.ToBytes()))
    } finally {
        rgb.Close()
        wide.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Arc mutates images through native path", PillowTestImageDrawArcMutatesImagesThroughNativePath)

PillowTestImageDrawArcClipsAndRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("L", [6, 5])
    widthZero := Pillow.Image.New("L", [4, 4])
    try {
        draw := Pillow.ImageDraw.Draw(image)
        draw.Arc([-2, -1, 3, 3], 0, 270, 6, 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 6, 0, 0,
            0, 0, 0, 6, 0, 0,
            6, 6, 6, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(image.ToBytes()))

        Pillow.ImageDraw.Draw(widthZero).Arc([0, 0, 3, 3], 0, 360, 5, 0)
        AhkTest.AssertEqual([
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ], PillowTestBufferToArray(widthZero.ToBytes()))

        try {
            draw.Arc([2, 2, 1, 1], 0, 90, 9)
            AhkTest.Fail("Expected ImageDraw.Arc to reject reversed coordinates")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }

        try {
            draw.Arc([0, 0, 3, 3], "start", 90, 9)
            AhkTest.Fail("Expected ImageDraw.Arc to reject non-numeric angles")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "angles") > 0)
        }
    } finally {
        widthZero.Close()
        image.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Arc clips and rejects invalid arguments", PillowTestImageDrawArcClipsAndRejectsInvalidArguments)

PillowTestImageDrawChordMutatesImagesThroughNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.New("L", [7, 7])
    wide := Pillow.Image.New("L", [7, 7])
    rgb := Pillow.Image.New("RGB", [5, 4])
    try {
        draw := Pillow.ImageDraw.Draw(l)
        returned := draw.Chord([1, 1, 5, 5], 0, 180, 7, 9, 1)
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 9, 9, 9, 9, 9, 0,
            0, 9, 9, 9, 9, 9, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(l.ToBytes()))

        Pillow.ImageDraw.Draw(wide).Chord([1, 1, 5, 5], 0, 180, unset, 8, 2)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 8, 8, 8, 8, 8, 0,
            0, 8, 8, 8, 8, 8, 0,
            0, 0, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(wide.ToBytes()))

        Pillow.ImageDraw.Draw(rgb).Chord([0, 0, 4, 3], 0, 180, [10, 20, 30], [90, 80, 70], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
        ], PillowTestBufferToArray(rgb.ToBytes()))
    } finally {
        rgb.Close()
        wide.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Chord mutates images through native path", PillowTestImageDrawChordMutatesImagesThroughNativePath)

PillowTestImageDrawChordClipsAndRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("L", [6, 5])
    widthZero := Pillow.Image.New("L", [4, 4])
    try {
        draw := Pillow.ImageDraw.Draw(image)
        draw.Chord([-2, -1, 3, 3], 0, 270, 5, 8, 1)
        AhkTest.AssertEqual([
            8, 8, 0, 0, 0, 0,
            5, 8, 8, 8, 0, 0,
            5, 5, 8, 8, 0, 0,
            8, 8, 8, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(image.ToBytes()))

        Pillow.ImageDraw.Draw(widthZero).Chord([0, 0, 3, 3], 0, 180, unset, 5, 0)
        AhkTest.AssertEqual([
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ], PillowTestBufferToArray(widthZero.ToBytes()))

        try {
            draw.Chord([2, 2, 1, 1], 0, 90, 9)
            AhkTest.Fail("Expected ImageDraw.Chord to reject reversed coordinates")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }

        try {
            draw.Chord([0, 0, 3, 3], "start", 90, 9)
            AhkTest.Fail("Expected ImageDraw.Chord to reject non-numeric angles")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "angles") > 0)
        }
    } finally {
        widthZero.Close()
        image.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Chord clips and rejects invalid arguments", PillowTestImageDrawChordClipsAndRejectsInvalidArguments)

PillowTestImageDrawPiesliceMutatesImagesThroughNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.New("L", [7, 7])
    wide := Pillow.Image.New("L", [7, 7])
    rgb := Pillow.Image.New("RGB", [5, 4])
    try {
        draw := Pillow.ImageDraw.Draw(l)
        returned := draw.Pieslice([1, 1, 5, 5], 0, 90, 7, 9, 1)
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 9, 9, 9, 9, 0,
            0, 0, 9, 9, 7, 9, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(l.ToBytes()))

        Pillow.ImageDraw.Draw(wide).Pieslice([1, 1, 5, 5], 0, 180, unset, 8, 2)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 8, 8, 8, 8, 8, 0,
            0, 8, 8, 8, 8, 8, 0,
            0, 8, 8, 8, 8, 8, 0,
            0, 0, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(wide.ToBytes()))

        Pillow.ImageDraw.Draw(rgb).Pieslice([0, 0, 4, 3], 0, 180, [10, 20, 30], [90, 80, 70], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70,
            90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
        ], PillowTestBufferToArray(rgb.ToBytes()))
    } finally {
        rgb.Close()
        wide.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Pieslice mutates images through native path", PillowTestImageDrawPiesliceMutatesImagesThroughNativePath)

PillowTestImageDrawPiesliceClipsAndRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("L", [6, 5])
    widthZero := Pillow.Image.New("L", [4, 4])
    try {
        draw := Pillow.ImageDraw.Draw(image)
        draw.Pieslice([-2, -1, 3, 3], 0, 270, 5, 8, 1)
        AhkTest.AssertEqual([
            8, 8, 0, 0, 0, 0,
            8, 8, 8, 8, 0, 0,
            5, 5, 5, 8, 0, 0,
            8, 8, 8, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(image.ToBytes()))

        Pillow.ImageDraw.Draw(widthZero).Pieslice([0, 0, 3, 3], 0, 180, unset, 5, 0)
        AhkTest.AssertEqual([
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ], PillowTestBufferToArray(widthZero.ToBytes()))

        try {
            draw.Pieslice([2, 2, 1, 1], 0, 90, 9)
            AhkTest.Fail("Expected ImageDraw.Pieslice to reject reversed coordinates")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }

        try {
            draw.Pieslice([0, 0, 3, 3], "start", 90, 9)
            AhkTest.Fail("Expected ImageDraw.Pieslice to reject non-numeric angles")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "angles") > 0)
        }
    } finally {
        widthZero.Close()
        image.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Pieslice clips and rejects invalid arguments", PillowTestImageDrawPiesliceClipsAndRejectsInvalidArguments)

PillowTestImageDrawRoundedRectangleMutatesImagesThroughNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.New("L", [8, 7])
    wide := Pillow.Image.New("L", [8, 7])
    fullY := Pillow.Image.New("L", [9, 6])
    rgb := Pillow.Image.New("RGB", [6, 5])
    try {
        draw := Pillow.ImageDraw.Draw(l)
        returned := draw.RoundedRectangle([1, 1, 6, 5], 2, 7, 9, 1)
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 9, 9, 9, 9, 0, 0,
            0, 9, 7, 7, 7, 7, 9, 0,
            0, 9, 7, 7, 7, 7, 9, 0,
            0, 9, 7, 7, 7, 7, 9, 0,
            0, 0, 9, 9, 9, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(l.ToBytes()))

        Pillow.ImageDraw.Draw(wide).RoundedRectangle([1, 1, 6, 5], 2, unset, 8, 2)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 8, 8, 8, 8, 0, 0,
            0, 8, 8, 8, 8, 8, 8, 0,
            0, 8, 8, 0, 0, 8, 8, 0,
            0, 8, 8, 8, 8, 8, 8, 0,
            0, 0, 8, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(wide.ToBytes()))

        Pillow.ImageDraw.Draw(fullY).RoundedRectangle([1, 1, 7, 4], 2, 5, 8, 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 8, 8, 8, 8, 8, 0, 0,
            0, 8, 5, 5, 5, 5, 5, 8, 0,
            0, 8, 5, 5, 5, 5, 5, 8, 0,
            0, 0, 8, 8, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(fullY.ToBytes()))

        Pillow.ImageDraw.Draw(rgb).RoundedRectangle([0, 0, 5, 4], 2, [10, 20, 30], [90, 80, 70], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
            90, 80, 70, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 90, 80, 70,
            90, 80, 70, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 90, 80, 70,
            90, 80, 70, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
        ], PillowTestBufferToArray(rgb.ToBytes()))
    } finally {
        rgb.Close()
        fullY.Close()
        wide.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.RoundedRectangle mutates images through native path", PillowTestImageDrawRoundedRectangleMutatesImagesThroughNativePath)

PillowTestImageDrawRoundedRectangleSupportsCornersAndFallbacks(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    partial := Pillow.Image.New("L", [8, 7])
    radiusZero := Pillow.Image.New("L", [6, 5])
    fullEllipse := Pillow.Image.New("L", [7, 7])
    try {
        Pillow.ImageDraw.Draw(partial).RoundedRectangle([1, 1, 6, 5], 2, 5, 8, 1, [true, false, true, false])
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 8, 8, 8, 8, 8, 0,
            0, 8, 5, 5, 5, 5, 8, 0,
            0, 8, 5, 5, 5, 5, 8, 0,
            0, 8, 5, 5, 5, 5, 8, 0,
            0, 8, 8, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(partial.ToBytes()))

        Pillow.ImageDraw.Draw(radiusZero).RoundedRectangle([1, 1, 4, 3], 0, 6, 9, 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 9, 9, 9, 9, 0,
            0, 9, 6, 6, 9, 0,
            0, 9, 9, 9, 9, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(radiusZero.ToBytes()))

        Pillow.ImageDraw.Draw(fullEllipse).RoundedRectangle([1, 1, 5, 5], 4, 7, 9, 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 9, 7, 7, 7, 9, 0,
            0, 9, 7, 7, 7, 9, 0,
            0, 9, 7, 7, 7, 9, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(fullEllipse.ToBytes()))
    } finally {
        fullEllipse.Close()
        radiusZero.Close()
        partial.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.RoundedRectangle supports corners and fallbacks", PillowTestImageDrawRoundedRectangleSupportsCornersAndFallbacks)

PillowTestImageDrawRoundedRectangleClipsAndRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("L", [6, 5])
    try {
        draw := Pillow.ImageDraw.Draw(image)
        draw.RoundedRectangle([-2, -1, 4, 3], 2, 5, 8, 1)
        AhkTest.AssertEqual([
            5, 0, 5, 5, 8, 0,
            5, 0, 5, 5, 8, 0,
            5, 0, 5, 5, 8, 0,
            8, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(image.ToBytes()))

        for args in [
            [[2, 2, 1, 1], 2, 9, "invalid argument"],
            [[0, 0, 3, 3], "r", 9, "radius"],
            [[0, 0, 3, 3], 2, 9, "corners"],
        ] {
            try {
                if args[4] = "corners"
                    draw.RoundedRectangle(args[1], args[2], args[3], unset, 1, [true, false])
                else
                    draw.RoundedRectangle(args[1], args[2], args[3])
                AhkTest.Fail("Expected ImageDraw.RoundedRectangle to reject invalid arguments")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, args[4]) > 0)
            }
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.RoundedRectangle clips and rejects invalid arguments", PillowTestImageDrawRoundedRectangleClipsAndRejectsInvalidArguments)

PillowTestImageDrawBitmapMutatesImagesThroughNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    oneTarget := Pillow.Image.New("L", [6, 5])
    oneMask := Pillow.Image.FromBytes("1", [3, 2], PillowTestBuffer([0x40, 0xA0]))
    rgbTarget := Pillow.Image.New("RGB", [5, 4], [100, 100, 100])
    rgbaMask := Pillow.Image.FromBytes("RGBA", [3, 2], PillowTestBuffer([
        0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 255,
        0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 255,
    ]))
    rgbaTarget := Pillow.Image.New("RGBA", [4, 3], [1, 2, 3, 4])
    lMask := Pillow.Image.FromBytes("L", [2, 2], PillowTestBuffer([0, 64, 128, 255]))
    try {
        draw := Pillow.ImageDraw.Draw(oneTarget)
        returned := draw.Bitmap([2, 1], oneMask, 7)
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 7, 0, 0,
            0, 0, 7, 0, 7, 0,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(oneTarget.ToBytes()))

        Pillow.ImageDraw.Draw(rgbTarget).Bitmap([-1, 1], rgbaMask, [10, 20, 30])
        AhkTest.AssertEqual([
            100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
            77, 80, 82, 10, 20, 30, 100, 100, 100, 100, 100, 100, 100, 100, 100,
            100, 100, 100, 10, 20, 30, 100, 100, 100, 100, 100, 100, 100, 100, 100,
            100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
        ], PillowTestBufferToArray(rgbTarget.ToBytes()))

        Pillow.ImageDraw.Draw(rgbaTarget).Bitmap([1, 1], lMask, [10, 20, 30, 200])
        AhkTest.AssertEqual([
            1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4,
            1, 2, 3, 4, 1, 2, 3, 4, 3, 7, 10, 53, 1, 2, 3, 4,
            1, 2, 3, 4, 6, 11, 17, 102, 10, 20, 30, 200, 1, 2, 3, 4,
        ], PillowTestBufferToArray(rgbaTarget.ToBytes()))
    } finally {
        lMask.Close()
        rgbaTarget.Close()
        rgbaMask.Close()
        rgbTarget.Close()
        oneMask.Close()
        oneTarget.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Bitmap mutates images through native path", PillowTestImageDrawBitmapMutatesImagesThroughNativePath)

PillowTestImageDrawBitmapRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    target := Pillow.Image.New("L", [4, 3])
    rgbMask := Pillow.Image.FromBytes("RGB", [2, 2], PillowTestBuffer([
        0, 0, 0, 1, 0, 0,
        0, 0, 1, 255, 255, 255,
    ]))
    try {
        draw := Pillow.ImageDraw.Draw(target)
        for args in [
            ["xy", "xy"],
            ["bitmap", [0, 0]],
            ["mask", [0, 0]],
        ] {
            try {
                if args[1] = "xy"
                    draw.Bitmap(args[2], target, 7)
                else if args[1] = "bitmap"
                    draw.Bitmap(args[2], "not image", 7)
                else
                    draw.Bitmap(args[2], rgbMask, 7)
                AhkTest.Fail("Expected ImageDraw.Bitmap to reject invalid arguments")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, args[1]) > 0 || InStr(err.Message, "invalid argument") > 0)
            }
        }
    } finally {
        rgbMask.Close()
        target.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Bitmap rejects invalid arguments", PillowTestImageDrawBitmapRejectsInvalidArguments)

PillowTestImageDrawFloodfillMutatesImagesThroughNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    noBorder := Pillow.Image.FromBytes("L", [5, 4], PillowTestBuffer([
        1, 1, 9, 9, 9,
        1, 2, 2, 9, 9,
        1, 2, 3, 3, 9,
        1, 1, 3, 9, 9,
    ]))
    threshold := Pillow.Image.FromBytes("L", [5, 3], PillowTestBuffer([
        10, 11, 14, 20, 20,
        9, 12, 13, 21, 20,
        8, 30, 13, 22, 20,
    ]))
    border := Pillow.Image.FromBytes("L", [5, 4], PillowTestBuffer([
        1, 1, 1, 1, 1,
        1, 2, 2, 8, 1,
        1, 2, 3, 8, 1,
        1, 1, 1, 1, 1,
    ]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        10, 20, 30, 11, 21, 31, 100, 0, 0,
        9, 19, 29, 50, 50, 50, 100, 0, 0,
    ]))
    try {
        returned := Pillow.ImageDraw.Floodfill(noBorder, [1, 1], 7)
        AhkTest.AssertEqual(noBorder, returned)
        AhkTest.AssertEqual([
            1, 1, 9, 9, 9,
            1, 7, 7, 9, 9,
            1, 7, 3, 3, 9,
            1, 1, 3, 9, 9,
        ], PillowTestBufferToArray(noBorder.ToBytes()))

        Pillow.ImageDraw.Floodfill(threshold, [0, 0], 99, unset, 3.0)
        AhkTest.AssertEqual([
            99, 99, 14, 20, 20,
            99, 99, 99, 21, 20,
            99, 30, 99, 22, 20,
        ], PillowTestBufferToArray(threshold.ToBytes()))

        Pillow.ImageDraw.Floodfill(border, [1, 1], 7, 1)
        AhkTest.AssertEqual([
            1, 1, 1, 1, 1,
            1, 7, 7, 7, 1,
            1, 7, 7, 7, 1,
            1, 1, 1, 1, 1,
        ], PillowTestBufferToArray(border.ToBytes()))

        Pillow.ImageDraw.Floodfill(rgb, [0, 0], [1, 2, 3], unset, 3.0)
        AhkTest.AssertEqual([
            1, 2, 3, 1, 2, 3, 100, 0, 0,
            1, 2, 3, 50, 50, 50, 100, 0, 0,
        ], PillowTestBufferToArray(rgb.ToBytes()))
    } finally {
        rgb.Close()
        border.Close()
        threshold.Close()
        noBorder.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Floodfill mutates images through native path", PillowTestImageDrawFloodfillMutatesImagesThroughNativePath)

PillowTestImageDrawFloodfillHandlesSeedsAndRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    negativeSeed := Pillow.Image.FromBytes("L", [3, 1], PillowTestBuffer([3, 3, 3]))
    outside := Pillow.Image.FromBytes("L", [3, 1], PillowTestBuffer([1, 2, 3]))
    alreadyFill := Pillow.Image.FromBytes("L", [3, 1], PillowTestBuffer([5, 5, 6]))
    try {
        Pillow.ImageDraw.Floodfill(negativeSeed, [-1, 0], 9)
        AhkTest.AssertEqual([9, 9, 9], PillowTestBufferToArray(negativeSeed.ToBytes()))

        Pillow.ImageDraw.Floodfill(outside, [3, 0], 9)
        AhkTest.AssertEqual([1, 2, 3], PillowTestBufferToArray(outside.ToBytes()))

        Pillow.ImageDraw.Floodfill(alreadyFill, [0, 0], 5)
        AhkTest.AssertEqual([5, 5, 6], PillowTestBufferToArray(alreadyFill.ToBytes()))

        for args in [
            ["image", "not image", [0, 0], 7],
            ["xy", alreadyFill, "bad", 7],
            ["value", alreadyFill, [0, 0], [7, 8]],
        ] {
            try {
                Pillow.ImageDraw.Floodfill(args[2], args[3], args[4])
                AhkTest.Fail("Expected ImageDraw.Floodfill to reject invalid arguments")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, args[1]) > 0 || InStr(err.Message, "color") > 0)
            }
        }
    } finally {
        alreadyFill.Close()
        outside.Close()
        negativeSeed.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Floodfill handles seeds and rejects invalid arguments", PillowTestImageDrawFloodfillHandlesSeedsAndRejectsInvalidArguments)

PillowTestImageDrawLineMutatesImageThroughNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.New("L", [6, 5])
    rgb := Pillow.Image.New("RGB", [4, 4])
    try {
        draw := Pillow.ImageDraw.Draw(l)
        returned := draw.Line([[0, 0], [3, 2], [5, 1]], 7, 1)
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            7, 0, 0, 0, 0, 0,
            0, 7, 7, 0, 7, 7,
            0, 0, 0, 7, 0, 0,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(l.ToBytes()))

        Pillow.ImageDraw.Draw(rgb).Line([0, 0, 3, 2], [10, 20, 30], 1)
        AhkTest.AssertEqual([
            10, 20, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 10, 20, 30, 10, 20, 30, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 20, 30,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(rgb.ToBytes()))
    } finally {
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Line mutates images through native path", PillowTestImageDrawLineMutatesImageThroughNativePath)

PillowTestImageDrawLineClipsAndRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("L", [5, 5])
    try {
        draw := Pillow.ImageDraw.Draw(image)
        draw.Line([[-1, -1], [3, 3]], 7)
        AhkTest.AssertEqual([
            7, 0, 0, 0, 0,
            0, 7, 0, 0, 0,
            0, 0, 7, 0, 0,
            0, 0, 0, 7, 0,
            0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(image.ToBytes()))

        try {
            draw.Line([[0, 0]], 7)
            AhkTest.Fail("Expected ImageDraw.Line to reject a single point")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "expects") > 0)
        }
        try {
            draw.Line([0, 0, 1], 7)
            AhkTest.Fail("Expected ImageDraw.Line to reject odd coordinate counts")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "expects") > 0)
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Line clips and rejects invalid arguments", PillowTestImageDrawLineClipsAndRejectsInvalidArguments)

PillowTestImageDrawLineSupportsWideLines(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    horizontal := Pillow.Image.New("L", [7, 5])
    diagonal := Pillow.Image.New("L", [7, 7])
    rgb := Pillow.Image.New("RGB", [5, 4])
    try {
        Pillow.ImageDraw.Draw(horizontal).Line([[1, 2], [5, 2]], 7, 3)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 7, 7, 7, 7, 7, 0,
            0, 7, 7, 7, 7, 7, 0,
            0, 7, 7, 7, 7, 7, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(horizontal.ToBytes()))

        Pillow.ImageDraw.Draw(diagonal).Line([[1, 1], [5, 4]], 9, 3)
        AhkTest.AssertEqual([
            0, 0, 9, 0, 0, 0, 0,
            0, 9, 9, 9, 0, 0, 0,
            9, 9, 9, 9, 9, 9, 0,
            0, 9, 9, 9, 9, 9, 9,
            0, 0, 0, 9, 9, 9, 0,
            0, 0, 0, 0, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(diagonal.ToBytes()))

        Pillow.ImageDraw.Draw(rgb).Line([0, 1, 4, 1], [10, 20, 30], 3)
        AhkTest.AssertEqual([
            10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30,
            10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30,
            10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(rgb.ToBytes()))
    } finally {
        rgb.Close()
        diagonal.Close()
        horizontal.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Line supports wide lines", PillowTestImageDrawLineSupportsWideLines)

PillowTestImageDrawLineSupportsWidePolylineAndClipping(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    polyline := Pillow.Image.New("L", [7, 6])
    clipped := Pillow.Image.New("L", [5, 4])
    try {
        Pillow.ImageDraw.Draw(polyline).Line([[0, 0], [3, 3], [6, 1]], 6, 3)
        AhkTest.AssertEqual([
            6, 6, 6, 0, 0, 6, 0,
            6, 6, 6, 6, 6, 6, 6,
            6, 6, 6, 6, 6, 6, 6,
            0, 6, 6, 6, 6, 6, 0,
            0, 0, 6, 0, 6, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(polyline.ToBytes()))

        Pillow.ImageDraw.Draw(clipped).Line([[-2, 1], [3, 1]], 5, 3)
        AhkTest.AssertEqual([
            5, 5, 5, 5, 0,
            5, 5, 5, 5, 0,
            5, 5, 5, 5, 0,
            0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(clipped.ToBytes()))
    } finally {
        clipped.Close()
        polyline.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Line supports wide polyline and clipping", PillowTestImageDrawLineSupportsWidePolylineAndClipping)

PillowTestImageDrawLineSupportsCurveJoints(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    curve := Pillow.Image.New("L", [9, 9])
    gap := Pillow.Image.New("L", [12, 10])
    straight := Pillow.Image.New("L", [9, 9])
    clippedGap := Pillow.Image.New("L", [18, 9])
    try {
        draw := Pillow.ImageDraw.Draw(curve)
        returned := draw.Line([[1, 7], [4, 1], [7, 7]], 7, 5, "curve")
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            0, 0, 7, 7, 7, 0, 7, 0, 0,
            0, 0, 7, 7, 7, 7, 7, 0, 0,
            0, 7, 7, 7, 7, 7, 7, 7, 0,
            0, 7, 7, 7, 7, 7, 7, 7, 0,
            7, 7, 7, 7, 7, 7, 7, 7, 7,
            7, 7, 7, 7, 7, 7, 7, 7, 7,
            7, 7, 7, 7, 7, 7, 7, 7, 7,
            0, 7, 7, 7, 0, 7, 7, 7, 0,
            0, 0, 0, 7, 0, 7, 0, 0, 0,
        ], PillowTestBufferToArray(curve.ToBytes()))

        Pillow.ImageDraw.Draw(gap).Line([[1, 8], [5, 1], [10, 8]], 9, 9, "curve")
        AhkTest.AssertEqual([
            0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0,
            0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0,
            0, 0, 0, 9, 9, 9, 9, 9, 9, 0, 0, 0,
        ], PillowTestBufferToArray(gap.ToBytes()))

        Pillow.ImageDraw.Draw(straight).Line([[1, 7], [4, 4], [7, 1]], 5, 7, "curve")
        AhkTest.AssertEqual([
            0, 0, 0, 0, 5, 5, 5, 0, 0,
            0, 0, 0, 5, 5, 5, 5, 5, 0,
            0, 0, 5, 5, 5, 5, 5, 5, 5,
            0, 5, 5, 5, 5, 5, 5, 5, 5,
            5, 5, 5, 5, 5, 5, 5, 5, 5,
            5, 5, 5, 5, 5, 5, 5, 5, 0,
            5, 5, 5, 5, 5, 5, 5, 0, 0,
            0, 5, 5, 5, 5, 5, 0, 0, 0,
            0, 0, 5, 5, 5, 0, 0, 0, 0,
        ], PillowTestBufferToArray(straight.ToBytes()))

        Pillow.ImageDraw.Draw(clippedGap).Line([[-4, 1], [15, 4], [13, 0]], 17, 9, "curve")
        AhkTest.AssertEqual([
            17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
            17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
            17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
            17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
            17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
            17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
            0, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
            0, 0, 0, 0, 0, 0, 0, 0, 17, 17, 17, 17, 17, 17, 17, 17, 17, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0,
        ], PillowTestBufferToArray(clippedGap.ToBytes()))
    } finally {
        clippedGap.Close()
        straight.Close()
        gap.Close()
        curve.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Line supports curve joints", PillowTestImageDrawLineSupportsCurveJoints)

PillowTestImageDrawPointMutatesImagesThroughNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.New("L", [5, 4])
    rgb := Pillow.Image.New("RGB", [3, 3])
    single := Pillow.Image.New("L", [2, 2])
    try {
        draw := Pillow.ImageDraw.Draw(l)
        returned := draw.Point([[0, 0], [2, 1], [4, 3]], 7)
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            7, 0, 0, 0, 0,
            0, 0, 7, 0, 0,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 7,
        ], PillowTestBufferToArray(l.ToBytes()))

        Pillow.ImageDraw.Draw(rgb).Point([0, 0, 2, 1], [10, 20, 30])
        AhkTest.AssertEqual([
            10, 20, 30, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 10, 20, 30,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(rgb.ToBytes()))

        Pillow.ImageDraw.Draw(single).Point([[1, 1]], 5)
        AhkTest.AssertEqual([0, 0, 0, 5], PillowTestBufferToArray(single.ToBytes()))
    } finally {
        single.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Point mutates images through native path", PillowTestImageDrawPointMutatesImagesThroughNativePath)

PillowTestImageDrawPointClipsAllowsEmptyAndRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("L", [5, 4])
    empty := Pillow.Image.New("L", [2, 2])
    try {
        draw := Pillow.ImageDraw.Draw(image)
        draw.Point([-1, -1, 0, 0, 2, 2, 5, 4], 9)
        AhkTest.AssertEqual([
            9, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            0, 0, 9, 0, 0,
            0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(image.ToBytes()))

        Pillow.ImageDraw.Draw(empty).Point([], 7)
        AhkTest.AssertEqual([0, 0, 0, 0], PillowTestBufferToArray(empty.ToBytes()))

        try {
            draw.Point([0, 0, 1], 7)
            AhkTest.Fail("Expected ImageDraw.Point to reject odd coordinate counts")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "expects") > 0)
        }
        try {
            draw.Point([[0, 0, 1]], 7)
            AhkTest.Fail("Expected ImageDraw.Point to reject malformed points")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "point expects") > 0)
        }
    } finally {
        empty.Close()
        image.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Point clips allows empty and rejects invalid arguments", PillowTestImageDrawPointClipsAllowsEmptyAndRejectsInvalidArguments)

PillowTestImageDrawPolygonMutatesImagesThroughNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.New("L", [6, 5])
    rgb := Pillow.Image.New("RGB", [4, 4])
    try {
        draw := Pillow.ImageDraw.Draw(l)
        returned := draw.Polygon([[1, 1], [4, 1], [2, 3]], 7, 9)
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 9, 9, 9, 9, 0,
            0, 9, 7, 9, 0, 0,
            0, 0, 9, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(l.ToBytes()))

        Pillow.ImageDraw.Draw(rgb).Polygon([0, 0, 3, 1, 1, 3], [10, 20, 30], [90, 80, 70])
        AhkTest.AssertEqual([
            90, 80, 70, 90, 80, 70, 0, 0, 0, 0, 0, 0,
            90, 80, 70, 10, 20, 30, 90, 80, 70, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 90, 80, 70, 0, 0, 0,
            0, 0, 0, 90, 80, 70, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(rgb.ToBytes()))
    } finally {
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Polygon mutates images through native path", PillowTestImageDrawPolygonMutatesImagesThroughNativePath)

PillowTestImageDrawPolygonSupportsWideOutlines(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    outline := Pillow.Image.New("L", [8, 7])
    fillOutline := Pillow.Image.New("L", [8, 7])
    rgb := Pillow.Image.New("RGB", [7, 6])
    lineLike := Pillow.Image.New("L", [5, 5])
    try {
        points := [[1, 1], [6, 1], [3, 5]]
        draw := Pillow.ImageDraw.Draw(outline)
        returned := draw.Polygon(points, unset, 9, 3)
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 9, 9, 9, 9, 9, 9, 0,
            0, 0, 9, 9, 9, 9, 0, 0,
            0, 0, 9, 9, 9, 0, 0, 0,
            0, 0, 0, 9, 9, 0, 0, 0,
            0, 0, 0, 9, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(outline.ToBytes()))

        Pillow.ImageDraw.Draw(fillOutline).Polygon(points, 4, 9, 3)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 9, 9, 9, 9, 9, 9, 0,
            0, 0, 9, 9, 9, 9, 0, 0,
            0, 0, 9, 9, 9, 0, 0, 0,
            0, 0, 0, 9, 9, 0, 0, 0,
            0, 0, 0, 9, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(fillOutline.ToBytes()))

        Pillow.ImageDraw.Draw(rgb).Polygon([0, 0, 6, 1, 2, 5], unset, [10, 20, 30], 4)
        AhkTest.AssertEqual([
            10, 20, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30,
            0, 0, 0, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 0, 0, 0,
            0, 0, 0, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 10, 20, 30, 10, 20, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 10, 20, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(rgb.ToBytes()))

        Pillow.ImageDraw.Draw(lineLike).Polygon([[0, 0], [4, 4]], unset, 7, 3)
        AhkTest.AssertEqual([
            7, 0, 0, 0, 0,
            0, 7, 0, 0, 0,
            0, 0, 7, 0, 0,
            0, 0, 0, 7, 0,
            0, 0, 0, 0, 7,
        ], PillowTestBufferToArray(lineLike.ToBytes()))
    } finally {
        lineLike.Close()
        rgb.Close()
        fillOutline.Close()
        outline.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Polygon supports wide outlines", PillowTestImageDrawPolygonSupportsWideOutlines)

PillowTestImageDrawPolygonClipsAndRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("L", [5, 4])
    try {
        draw := Pillow.ImageDraw.Draw(image)
        draw.Polygon([[-1, 1], [2, -1], [5, 3]], 8)
        AhkTest.AssertEqual([
            0, 8, 8, 8, 0,
            8, 8, 8, 8, 0,
            0, 0, 8, 8, 8,
            0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(image.ToBytes()))

        try {
            draw.Polygon([[0, 0]], 7)
            AhkTest.Fail("Expected ImageDraw.Polygon to reject a single point")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "expects") > 0)
        }
        lineLike := Pillow.Image.New("L", [3, 3])
        try {
            Pillow.ImageDraw.Draw(lineLike).Polygon([[0, 0], [1, 1]], unset, 7, 2)
            AhkTest.AssertEqual([
                7, 0, 0,
                0, 7, 0,
                0, 0, 0,
            ], PillowTestBufferToArray(lineLike.ToBytes()))
        } finally {
            lineLike.Close()
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.Polygon clips and rejects invalid arguments", PillowTestImageDrawPolygonClipsAndRejectsInvalidArguments)

PillowTestImageDrawRegularPolygonUsesNativePolygonPath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    triangle := Pillow.Image.New("L", [9, 9])
    square := Pillow.Image.New("L", [9, 9])
    pentagon := Pillow.Image.New("L", [9, 9])
    try {
        draw := Pillow.ImageDraw.Draw(triangle)
        returned := draw.RegularPolygon([4, 4, 3], 3, 0, 7, 9)
        AhkTest.AssertEqual(draw, returned)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 9, 0, 0, 0, 0,
            0, 0, 0, 9, 9, 0, 0, 0, 0,
            0, 0, 9, 7, 7, 9, 0, 0, 0,
            0, 0, 9, 7, 7, 9, 0, 0, 0,
            0, 9, 9, 9, 9, 9, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(triangle.ToBytes()))

        Pillow.ImageDraw.Draw(square).RegularPolygon([[4, 4], 3], 4, 45, 7, 9)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 9, 0, 0, 0, 0,
            0, 0, 0, 9, 7, 9, 0, 0, 0,
            0, 0, 9, 7, 7, 7, 9, 0, 0,
            0, 9, 7, 7, 7, 7, 7, 9, 0,
            0, 0, 9, 7, 7, 7, 9, 0, 0,
            0, 0, 0, 9, 7, 9, 0, 0, 0,
            0, 0, 0, 0, 9, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(square.ToBytes()))

        Pillow.ImageDraw.Draw(pentagon).RegularPolygon([4, 4, 3], 5, 18, 6)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 6, 0, 0, 0, 0, 0,
            0, 0, 6, 6, 6, 6, 6, 0, 0,
            0, 0, 6, 6, 6, 6, 6, 0, 0,
            0, 6, 6, 6, 6, 6, 6, 0, 0,
            0, 0, 6, 6, 6, 6, 6, 0, 0,
            0, 0, 0, 6, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(pentagon.ToBytes()))
    } finally {
        pentagon.Close()
        square.Close()
        triangle.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.RegularPolygon uses native polygon path", PillowTestImageDrawRegularPolygonUsesNativePolygonPath)

PillowTestImageDrawRegularPolygonRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("L", [5, 5])
    try {
        draw := Pillow.ImageDraw.Draw(image)
        for args in [
            [[4, 4, 3], 2, "n_sides"],
            [[4, 4, 0], 3, "radius"],
            ["x", 3, "sequence"],
            [[4, "x", 3], 3, "numeric"],
            [[[4, 4, 4], 3], 3, "2D"],
            [[4, 4, 3], 3.5, "n_sides"],
            [[4, 4, 3], 3, "rotation", "bad"],
        ] {
            try {
                if args.Length = 4
                    draw.RegularPolygon(args[1], args[2], args[4], 1)
                else
                    draw.RegularPolygon(args[1], args[2], 0, 1)
                AhkTest.Fail("Expected RegularPolygon to reject invalid arguments")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, args[3]) > 0)
            }
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageDraw.RegularPolygon rejects invalid arguments", PillowTestImageDrawRegularPolygonRejectsInvalidArguments)

PillowTestImageCmykFoundationUsesNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("CMYK", [2, 1], PillowTestBuffer([0, 10, 20, 30, 255, 128, 64, 0]))
    inverted := 0
    try {
        AhkTest.AssertEqual("CMYK", image.Mode)
        AhkTest.AssertEqual([2, 1], image.Size)
        AhkTest.AssertEqual(4, image.Channels)
        AhkTest.AssertEqual(["C", "M", "Y", "K"], image.GetBands())
        AhkTest.AssertEqual([0, 10, 20, 30, 255, 128, 64, 0], PillowTestBufferToArray(image.ToBytes()))
        AhkTest.AssertEqual([[0, 10, 20, 30], [255, 128, 64, 0]], image.GetData())
        AhkTest.AssertEqual([255, 128, 64, 0], image.GetPixel([1, 0]))

        image.PutData([[9, 8, 7, 6]])
        AhkTest.AssertEqual([9, 8, 7, 6, 255, 128, 64, 0], PillowTestBufferToArray(image.ToBytes()))

        image.PutPixel([0, 0], [1, 2, 3, 4])
        AhkTest.AssertEqual([1, 2, 3, 4, 255, 128, 64, 0], PillowTestBufferToArray(image.ToBytes()))

        inverted := Pillow.ImageChops.Invert(image)
        AhkTest.AssertEqual("CMYK", inverted.Mode)
        AhkTest.AssertEqual([254, 253, 252, 251, 0, 127, 191, 255], PillowTestBufferToArray(inverted.ToBytes()))
    } finally {
        if IsObject(inverted)
            inverted.Close()
        image.Close()
    }
}

AhkTest.Test("Pillow CMYK foundation uses native handles", PillowTestImageCmykFoundationUsesNativeHandles)

PillowTestImageFromBytesDefaultRawAllowsExtraBytes(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [1, 1], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    rgba := Pillow.Image.FromBytes("RGBA", [1, 1], PillowTestBuffer([7, 8, 9, 10, 11, 12, 13, 14]))
    la := Pillow.Image.FromBytes("LA", [1, 1], PillowTestBuffer([15, 16, 17, 18]))
    try {
        AhkTest.AssertEqual([1, 2, 3], PillowTestBufferToArray(rgb.ToBytes()))
        AhkTest.AssertEqual([7, 8, 9, 10], PillowTestBufferToArray(rgba.ToBytes()))
        AhkTest.AssertEqual([15, 16], PillowTestBufferToArray(la.ToBytes()))
    } finally {
        la.Close()
        rgba.Close()
        rgb.Close()
    }
}

AhkTest.Test("Pillow Image.FromBytes default raw decoder allows extra bytes", PillowTestImageFromBytesDefaultRawAllowsExtraBytes)

PillowTestImageFromBytesSupportsRawModesAndStride(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [2, 2], PillowTestBuffer([
        1, 2, 3, 4, 5, 6, 99, 99,
        7, 8, 9, 10, 11, 12, 88, 88,
    ]), "raw", "BGR", 8, -1)
    rgba := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([1, 2, 3, 4, 5, 6, 7, 8]), "raw", "BGRA")
    try {
        AhkTest.AssertEqual([9, 8, 7, 12, 11, 10, 3, 2, 1, 6, 5, 4], PillowTestBufferToArray(rgb.ToBytes()))
        AhkTest.AssertEqual([3, 2, 1, 4, 7, 6, 5, 8], PillowTestBufferToArray(rgba.ToBytes()))
    } finally {
        rgba.Close()
        rgb.Close()
    }
}

AhkTest.Test("Pillow Image.FromBytes supports raw modes and stride", PillowTestImageFromBytesSupportsRawModesAndStride)

PillowTestImageToBytesSupportsRawModes(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([10, 20, 30, 40, 50, 60]))
    rgba := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([10, 20, 30, 40, 50, 60, 70, 80]))
    try {
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60], PillowTestBufferToArray(rgb.ToBytes("raw")))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80], PillowTestBufferToArray(rgba.ToBytes("raw")))
        AhkTest.AssertEqual([30, 20, 10, 60, 50, 40], PillowTestBufferToArray(rgb.ToBytes("raw", "BGR")))
        AhkTest.AssertEqual([10, 20, 30, 255, 40, 50, 60, 255], PillowTestBufferToArray(rgb.ToBytes("raw", "RGBX")))
        AhkTest.AssertEqual([30, 20, 10, 40, 70, 60, 50, 80], PillowTestBufferToArray(rgba.ToBytes("raw", "BGRA")))
        AhkTest.AssertEqual([40, 30, 20, 10, 80, 70, 60, 50], PillowTestBufferToArray(rgba.ToBytes("raw", "ABGR")))
    } finally {
        rgba.Close()
        rgb.Close()
    }
}

AhkTest.Test("Pillow Image.ToBytes supports raw modes", PillowTestImageToBytesSupportsRawModes)

PillowTestModeOneFromAndToBytesUsePillowBitPacking(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("1", [9, 1], PillowTestBuffer([0xAA, 0x80]))
    nibble := Pillow.Image.FromBytes("1", [4, 1], PillowTestBuffer([0x50]))
    try {
        AhkTest.AssertEqual("1", image.Mode)
        AhkTest.AssertEqual(1, image.Channels)
        AhkTest.AssertEqual(9, image.ByteSize)
        AhkTest.AssertEqual(["1"], image.GetBands())
        AhkTest.AssertEqual([0xAA, 0x80], PillowTestBufferToArray(image.ToBytes()))
        AhkTest.AssertEqual([0xAA, 0x80], PillowTestBufferToArray(image.ToBytes("raw", "1")))
        AhkTest.AssertEqual([255, 0, 255, 0, 255, 0, 255, 0, 255], image.GetData())
        AhkTest.AssertEqual([0x50], PillowTestBufferToArray(nibble.ToBytes()))
        AhkTest.AssertEqual([0, 255, 0, 255], nibble.GetData())
    } finally {
        nibble.Close()
        image.Close()
    }
}

AhkTest.Test("Pillow mode 1 FromBytes and ToBytes use Pillow bit packing", PillowTestModeOneFromAndToBytesUsePillowBitPacking)

PillowTestPaletteModeUsesNativePaletteForConversion(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("P", [4, 1], PillowTestBuffer([0, 1, 2, 3]))
    outputs := []
    try {
        image.PutPalette([
            0, 0, 0,
            10, 20, 30,
            200, 100, 50,
            255, 255, 255,
        ])

        AhkTest.AssertEqual("P", image.Mode)
        AhkTest.AssertEqual(["P"], image.GetBands())
        AhkTest.AssertEqual([0, 1, 2, 3], PillowTestBufferToArray(image.ToBytes()))
        AhkTest.AssertEqual([
            0, 0, 0,
            10, 20, 30,
            200, 100, 50,
            255, 255, 255,
        ], image.GetPalette())

        rgb := image.Convert("RGB")
        rgba := image.Convert("RGBA")
        l := image.Convert("L")
        outputs.Push(rgb)
        outputs.Push(rgba)
        outputs.Push(l)

        AhkTest.AssertEqual([0, 0, 0, 10, 20, 30, 200, 100, 50, 255, 255, 255], PillowTestBufferToArray(rgb.ToBytes()))
        AhkTest.AssertEqual([0, 0, 0, 255, 10, 20, 30, 255, 200, 100, 50, 255, 255, 255, 255, 255], PillowTestBufferToArray(rgba.ToBytes()))
        AhkTest.AssertEqual([0, 18, 124, 255], PillowTestBufferToArray(l.ToBytes()))
    } finally {
        for item in outputs {
            if IsObject(item)
                item.Close()
        }
        image.Close()
    }
}

AhkTest.Test("Pillow P mode uses native palette for conversion", PillowTestPaletteModeUsesNativePaletteForConversion)

PillowTestImageRemapPaletteUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    p := Pillow.Image.FromBytes("P", [4, 1], PillowTestBuffer([0, 1, 2, 3]))
    l := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 1, 2, 3]))
    rgb := Pillow.Image.New("RGB", [1, 1])
    outputs := []
    try {
        p.PutPalette([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120])

        remappedP := p.RemapPalette([3, 2, 1, 0])
        duplicateP := p.RemapPalette([1, 1, 2, 3])
        remappedL := l.RemapPalette([3, 2, 1, 0])
        outputs.Push(remappedP)
        outputs.Push(duplicateP)
        outputs.Push(remappedL)

        AhkTest.AssertEqual("P", remappedP.Mode)
        AhkTest.AssertEqual([3, 2, 1, 0], PillowTestBufferToArray(remappedP.ToBytes()))
        AhkTest.AssertEqual([100, 110, 120, 70, 80, 90, 40, 50, 60, 10, 20, 30], remappedP.GetPalette())

        AhkTest.AssertEqual([0, 1, 2, 3], PillowTestBufferToArray(duplicateP.ToBytes()))
        AhkTest.AssertEqual([40, 50, 60, 40, 50, 60, 70, 80, 90, 100, 110, 120], duplicateP.GetPalette())

        AhkTest.AssertEqual("P", remappedL.Mode)
        AhkTest.AssertEqual([3, 2, 1, 0], PillowTestBufferToArray(remappedL.ToBytes()))
        AhkTest.AssertEqual([3, 3, 3, 2, 2, 2, 1, 1, 1, 0, 0, 0], remappedL.GetPalette())

        errorWasRaised := false
        try {
            rgb.RemapPalette([0])
        } catch Error as err {
            errorWasRaised := InStr(err.Message, "invalid argument") > 0 || InStr(err.Message, "illegal image mode") > 0
        }
        AhkTest.AssertTrue(errorWasRaised)
    } finally {
        for item in outputs {
            if IsObject(item)
                item.Close()
        }
        rgb.Close()
        l.Close()
        p.Close()
    }
}

AhkTest.Test("Pillow Image.RemapPalette remaps P and L through native path", PillowTestImageRemapPaletteUsesNativePath)

PillowTestPaletteModePreservesPaletteThroughNativeOperations(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    palette := [
        0, 0, 0,
        10, 20, 30,
        200, 100, 50,
        255, 255, 255,
    ]
    image := Pillow.Image.FromBytes("P", [4, 2], PillowTestBuffer([0, 1, 2, 3, 3, 2, 1, 0]))
    outputs := []
    try {
        image.PutPalette(palette)

        cropped := image.Crop([1, 0, 4, 2])
        transposed := image.Transpose(Pillow.Transpose.ROTATE_90)
        resized := image.Resize([2, 4], Pillow.Resampling.NEAREST)
        offset := Pillow.ImageChops.Offset(image, 1, -1)
        expanded := Pillow.ImageOps.Expand(image, [1, 0, 1, 1], 3)
        outputs.Push(cropped)
        outputs.Push(transposed)
        outputs.Push(resized)
        outputs.Push(offset)
        outputs.Push(expanded)

        AhkTest.AssertEqual("P", cropped.Mode)
        AhkTest.AssertEqual("P", transposed.Mode)
        AhkTest.AssertEqual("P", resized.Mode)
        AhkTest.AssertEqual("P", offset.Mode)
        AhkTest.AssertEqual("P", expanded.Mode)
        AhkTest.AssertEqual(palette, cropped.GetPalette())
        AhkTest.AssertEqual(palette, transposed.GetPalette())
        AhkTest.AssertEqual(palette, resized.GetPalette())
        AhkTest.AssertEqual(palette, offset.GetPalette())
        AhkTest.AssertEqual(palette, expanded.GetPalette())

        croppedRgb := cropped.Convert("RGB")
        transposedRgb := transposed.Convert("RGB")
        resizedRgb := resized.Convert("RGB")
        offsetRgb := offset.Convert("RGB")
        expandedRgb := expanded.Convert("RGB")
        outputs.Push(croppedRgb)
        outputs.Push(transposedRgb)
        outputs.Push(resizedRgb)
        outputs.Push(offsetRgb)
        outputs.Push(expandedRgb)

        AhkTest.AssertEqual([10, 20, 30, 200, 100, 50, 255, 255, 255, 200, 100, 50, 10, 20, 30, 0, 0, 0], PillowTestBufferToArray(croppedRgb.ToBytes()))
        AhkTest.AssertEqual([255, 255, 255, 0, 0, 0, 200, 100, 50, 10, 20, 30, 10, 20, 30, 200, 100, 50, 0, 0, 0, 255, 255, 255], PillowTestBufferToArray(transposedRgb.ToBytes()))
        AhkTest.AssertEqual([10, 20, 30, 255, 255, 255, 10, 20, 30, 255, 255, 255, 200, 100, 50, 0, 0, 0, 200, 100, 50, 0, 0, 0], PillowTestBufferToArray(resizedRgb.ToBytes()))
        AhkTest.AssertEqual([0, 0, 0, 255, 255, 255, 200, 100, 50, 10, 20, 30, 255, 255, 255, 0, 0, 0, 10, 20, 30, 200, 100, 50], PillowTestBufferToArray(offsetRgb.ToBytes()))
        AhkTest.AssertEqual([255, 255, 255, 0, 0, 0, 10, 20, 30, 200, 100, 50, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 200, 100, 50, 10, 20, 30, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255], PillowTestBufferToArray(expandedRgb.ToBytes()))
    } finally {
        for item in outputs {
            if IsObject(item)
                item.Close()
        }
        image.Close()
    }
}

AhkTest.Test("Pillow P mode preserves palette through native operations", PillowTestPaletteModePreservesPaletteThroughNativeOperations)

PillowTestPutPaletteConvertsLToPaletteMode(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [3, 1], PillowTestBuffer([0, 1, 2]))
    rgb := 0
    palette := [
        10, 20, 30,
        40, 50, 60,
        70, 80, 90,
    ]
    try {
        image.PutPalette(palette)
        rgb := image.Convert("RGB")

        AhkTest.AssertEqual("P", image.Mode)
        AhkTest.AssertEqual(["P"], image.GetBands())
        AhkTest.AssertEqual([0, 1, 2], PillowTestBufferToArray(image.ToBytes()))
        AhkTest.AssertEqual(palette, image.GetPalette())
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90], PillowTestBufferToArray(rgb.ToBytes()))
    } finally {
        if IsObject(rgb)
            rgb.Close()
        image.Close()
    }
}

AhkTest.Test("Pillow Image.PutPalette converts L images to P mode", PillowTestPutPaletteConvertsLToPaletteMode)

PillowTestPaletteModePreservesPaletteThroughGeometryOperations(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    palette := [
        0, 0, 0,
        10, 20, 30,
        200, 100, 50,
        255, 255, 255,
    ]
    image := Pillow.Image.FromBytes("P", [2, 2], PillowTestBuffer([0, 1, 2, 3]))
    outputs := []
    try {
        image.PutPalette(palette)

        transformed := image.TransformAffine([2, 2], [1, 0, -1, 0, 1, 0], Pillow.Resampling.NEAREST, 3)
        rotated := image.Rotate(45, Pillow.Resampling.NEAREST, true, , , 3)
        outputs.Push(transformed)
        outputs.Push(rotated)

        AhkTest.AssertEqual("P", transformed.Mode)
        AhkTest.AssertEqual("P", rotated.Mode)
        AhkTest.AssertEqual(palette, transformed.GetPalette())
        AhkTest.AssertEqual(palette, rotated.GetPalette())

        transformedRgb := transformed.Convert("RGB")
        rotatedRgb := rotated.Convert("RGB")
        outputs.Push(transformedRgb)
        outputs.Push(rotatedRgb)

        AhkTest.AssertEqual([255, 255, 255, 0, 0, 0, 255, 255, 255, 200, 100, 50], PillowTestBufferToArray(transformedRgb.ToBytes()))
        AhkTest.AssertEqual([255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 10, 20, 30, 255, 255, 255, 255, 255, 255, 255, 255, 255, 200, 100, 50, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255], PillowTestBufferToArray(rotatedRgb.ToBytes()))
    } finally {
        for item in outputs {
            if IsObject(item)
                item.Close()
        }
        image.Close()
    }
}

AhkTest.Test("Pillow P mode preserves palette through geometry operations", PillowTestPaletteModePreservesPaletteThroughGeometryOperations)

PillowTestPaletteModePreservesPaletteThroughPointOperations(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    palette := [
        0, 0, 0,
        10, 20, 30,
        200, 100, 50,
        255, 255, 255,
    ]
    lut := []
    loop 256
        lut.Push(Mod(A_Index, 4))
    image := Pillow.Image.FromBytes("P", [2, 2], PillowTestBuffer([0, 1, 2, 3]))
    outputs := []
    try {
        image.PutPalette(palette)

        pointed := image.Point(lut)
        outputs.Push(pointed)

        AhkTest.AssertEqual("P", pointed.Mode)
        AhkTest.AssertEqual([1, 2, 3, 0], PillowTestBufferToArray(pointed.ToBytes()))
        AhkTest.AssertEqual(palette, pointed.GetPalette())

        pointedRgb := pointed.Convert("RGB")
        outputs.Push(pointedRgb)
        AhkTest.AssertEqual([10, 20, 30, 200, 100, 50, 255, 255, 255, 0, 0, 0], PillowTestBufferToArray(pointedRgb.ToBytes()))
    } finally {
        for item in outputs {
            if IsObject(item)
                item.Close()
        }
        image.Close()
    }
}

AhkTest.Test("Pillow P mode preserves palette through point operations", PillowTestPaletteModePreservesPaletteThroughPointOperations)

PillowTestImageDataPointerSharesNativeStorage(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("L", [3, 1])
    try {
        data := image.DataPointer()
        AhkTest.AssertTrue(data.Ptr != 0)
        AhkTest.AssertEqual(3, data.Size)
        NumPut("UChar", 7, data.Ptr, 0)
        NumPut("UChar", 8, data.Ptr, 1)
        NumPut("UChar", 9, data.Ptr, 2)
        AhkTest.AssertEqual([7, 8, 9], PillowTestBufferToArray(image.ToBytes()))
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow Image.DataPointer exposes native storage for controlled sharing", PillowTestImageDataPointerSharesNativeStorage)

PillowTestImageGetDataReturnsPillowLikePixelSequence(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [3, 1], PillowTestBuffer([1, 2, 3]))
    rgb := Pillow.Image.FromBytes("RGB", [2, 2], PillowTestBuffer([
        1, 2, 3, 4, 5, 6,
        7, 8, 9, 10, 11, 12,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([1, 2, 3, 4, 5, 6, 7, 8]))
    try {
        AhkTest.AssertEqual([1, 2, 3], l.GetData())
        AhkTest.AssertEqual([[1, 2, 3], [4, 5, 6], [7, 8, 9], [10, 11, 12]], rgb.GetData())
        AhkTest.AssertEqual([1, 4, 7, 10], rgb.GetData(0))
        AhkTest.AssertEqual([3, 6, 9, 12], rgb.GetData(2))
        AhkTest.AssertEqual([[1, 2, 3, 4], [5, 6, 7, 8]], rgba.GetData())
    } finally {
        rgba.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.GetData returns Pillow-like pixel sequences", PillowTestImageGetDataReturnsPillowLikePixelSequence)

PillowTestImagePutDataBulkWritesNativeStorage(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.New("L", [4, 1])
    rgb := Pillow.Image.New("RGB", [3, 1])
    rgba := Pillow.Image.New("RGBA", [3, 1])
    try {
        l.PutData([1.2, 1.5, 255, 300], 2, 10)
        AhkTest.AssertEqual([12, 13, 255, 255], PillowTestBufferToArray(l.ToBytes()))

        l.PutData([9, 8])
        AhkTest.AssertEqual([9, 8, 255, 255], PillowTestBufferToArray(l.ToBytes()))

        rgb.PutData([[1, 2, 3], [4, 5, 6, 7], 0x010203])
        AhkTest.AssertEqual([1, 2, 3, 4, 5, 6, 3, 2, 1], PillowTestBufferToArray(rgb.ToBytes()))

        rgba.PutData([[1, 2, 3], [4, 5, 6, 7], 8])
        AhkTest.AssertEqual([1, 2, 3, 255, 4, 5, 6, 7, 8, 0, 0, 0], PillowTestBufferToArray(rgba.ToBytes()))
    } finally {
        rgba.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.PutData bulk writes native storage", PillowTestImagePutDataBulkWritesNativeStorage)

PillowTestImageFromBytesRejectsInvalidLength(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    try {
        Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 2, 3]))
        AhkTest.Fail("Expected Image.FromBytes to reject invalid byte length")
    } catch Error as err {
        AhkTest.AssertTrue(InStr(err.Message, "invalid length") > 0)
    }
}

AhkTest.Test("Pillow Image.FromBytes raises wrapper errors from native status", PillowTestImageFromBytesRejectsInvalidLength)

PillowTestImageCopyReturnsIndependentNativeHandle(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([10, 20, 30, 40, 50, 60]))
    copied := 0
    try {
        copied := source.Copy()
        data := source.DataPointer()
        NumPut("UChar", 99, data.Ptr, 0)
        AhkTest.AssertEqual("RGB", copied.Mode)
        AhkTest.AssertEqual([2, 1], copied.Size)
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60], PillowTestBufferToArray(copied.ToBytes()))
    } finally {
        if IsObject(copied)
            copied.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Copy returns an independent native image", PillowTestImageCopyReturnsIndependentNativeHandle)

PillowTestImageGetAndPutPixelUseNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [2, 2], PillowTestBuffer([1, 2, 3, 4]))
    rgb := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    rgba := Pillow.Image.FromBytes("RGBA", [1, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6, 7, 8]))
    one := Pillow.Image.FromBytes("1", [4, 1], PillowTestBuffer([0x50]))
    try {
        AhkTest.AssertEqual(1, l.GetPixel([0, 0]))
        AhkTest.AssertEqual(2, l.GetPixel([-1, 0]))
        AhkTest.AssertEqual([4, 5, 6], rgb.GetPixel([1, 0]))
        AhkTest.AssertEqual([1, 2, 3, 4], rgba.GetPixel([-1, 0]))
        AhkTest.AssertEqual(0, one.GetPixel([0, 0]))
        AhkTest.AssertEqual(255, one.GetPixel([1, 0]))

        l.PutPixel([0, 0], 9)
        rgb.PutPixel([0, 0], 9)
        rgba.PutPixel([0, 0], [9, 9, 9, 9])
        one.PutPixel([2, 0], 256)

        AhkTest.AssertEqual([9, 2, 3, 4], PillowTestBufferToArray(l.ToBytes()))
        AhkTest.AssertEqual([9, 0, 0, 4, 5, 6], PillowTestBufferToArray(rgb.ToBytes()))
        AhkTest.AssertEqual([9, 9, 9, 9, 5, 6, 7, 8], PillowTestBufferToArray(rgba.ToBytes()))
        AhkTest.AssertEqual(255, one.GetPixel([2, 0]))
        AhkTest.AssertEqual([0x70], PillowTestBufferToArray(one.ToBytes()))
    } finally {
        one.Close()
        rgba.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.GetPixel and PutPixel use native operations", PillowTestImageGetAndPutPixelUseNativeOperation)

PillowTestImageGetAndPutPixelRejectInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    try {
        try {
            image.GetPixel([99, 0])
            AhkTest.Fail("Expected GetPixel to reject out-of-range coordinates")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0 || InStr(err.Message, "index") > 0)
        }

        try {
            image.PutPixel([0, 0], [1, 2])
            AhkTest.Fail("Expected PutPixel to reject short RGB values")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid length") > 0 || InStr(err.Message, "color") > 0)
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow Image.GetPixel and PutPixel reject invalid arguments", PillowTestImageGetAndPutPixelRejectInvalidArguments)

PillowTestImageCropUsesNativeHandleOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        10, 20, 30, 40, 50, 60, 70, 80, 90,
        100, 110, 120, 130, 140, 150, 160, 170, 180,
    ]))
    cropped := 0
    try {
        cropped := source.Crop([1, 0, 3, 2])
        AhkTest.AssertEqual("RGB", cropped.Mode)
        AhkTest.AssertEqual([2, 2], cropped.Size)
        AhkTest.AssertEqual([40, 50, 60, 70, 80, 90, 130, 140, 150, 160, 170, 180], PillowTestBufferToArray(cropped.ToBytes()))
    } finally {
        if IsObject(cropped)
            cropped.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Crop returns a native cropped image", PillowTestImageCropUsesNativeHandleOperation)

PillowTestImageCropSupportsDefaultAndFloatBoxes(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [5, 3], PillowTestBuffer([
        0, 1, 2, 3, 4,
        5, 6, 7, 8, 9,
        10, 11, 12, 13, 14,
    ]))
    copy := 0
    halfEven := 0
    rounded := 0
    try {
        copy := source.Crop()
        halfEven := source.Crop([0.5, 0.5, 3.5, 2.5])
        rounded := source.Crop([1.2, 0.8, 4.6, 2.2])

        AhkTest.AssertEqual("L", copy.Mode)
        AhkTest.AssertEqual([5, 3], copy.Size)
        AhkTest.AssertEqual(PillowTestBufferToArray(source.ToBytes()), PillowTestBufferToArray(copy.ToBytes()))
        source.PutPixel([0, 0], 99)
        AhkTest.AssertEqual(0, copy.GetPixel([0, 0]))

        AhkTest.AssertEqual([4, 2], halfEven.Size)
        AhkTest.AssertEqual([0, 1, 2, 3, 5, 6, 7, 8], PillowTestBufferToArray(halfEven.ToBytes()))
        AhkTest.AssertEqual([4, 1], rounded.Size)
        AhkTest.AssertEqual([6, 7, 8, 9], PillowTestBufferToArray(rounded.ToBytes()))
    } finally {
        for image in [rounded, halfEven, copy, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Crop supports default and float boxes", PillowTestImageCropSupportsDefaultAndFloatBoxes)

PillowTestImageResizeNearestUsesNativeHandleOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGB", [2, 2], PillowTestBuffer([
        1, 2, 3, 10, 20, 30,
        100, 110, 120, 200, 210, 220,
    ]))
    resized := 0
    try {
        resized := source.Resize([3, 3], Pillow.Resampling.NEAREST)
        AhkTest.AssertEqual("RGB", resized.Mode)
        AhkTest.AssertEqual([3, 3], resized.Size)
        AhkTest.AssertEqual([
            1, 2, 3, 10, 20, 30, 10, 20, 30,
            100, 110, 120, 200, 210, 220, 200, 210, 220,
            100, 110, 120, 200, 210, 220, 200, 210, 220,
        ], PillowTestBufferToArray(resized.ToBytes()))
    } finally {
        if IsObject(resized)
            resized.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Resize NEAREST resizes through native handles", PillowTestImageResizeNearestUsesNativeHandleOperation)

PillowTestImageResizeBilinearUsesNativeHandleOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [2, 2], PillowTestBuffer([0, 64, 128, 255]))
    la := Pillow.Image.FromBytes("LA", [2, 2], PillowTestBuffer([
        10, 0, 200, 128,
        50, 255, 250, 64,
    ]))
    resized := 0
    laResized := 0
    try {
        resized := source.Resize([3, 3], Pillow.Resampling.BILINEAR)
        laResized := la.Resize([3, 3], Pillow.Resampling.BILINEAR)
        AhkTest.AssertEqual("L", resized.Mode)
        AhkTest.AssertEqual([3, 3], resized.Size)
        AhkTest.AssertEqual([
            0, 32, 64,
            64, 112, 160,
            128, 192, 255,
        ], PillowTestBufferToArray(resized.ToBytes()))
        AhkTest.AssertEqual("LA", laResized.Mode)
        AhkTest.AssertEqual([
            0, 0, 199, 64, 199, 128,
            49, 128, 122, 112, 217, 96,
            50, 255, 90, 160, 251, 64,
        ], PillowTestBufferToArray(laResized.ToBytes()))
    } finally {
        if IsObject(laResized)
            laResized.Close()
        if IsObject(resized)
            resized.Close()
        la.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Resize BILINEAR resizes through native handles", PillowTestImageResizeBilinearUsesNativeHandleOperation)

PillowTestImageResizeBicubicUsesNativeHandleOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([0, 30, 80, 120, 180, 255]))
    resized := 0
    try {
        resized := source.Resize([5, 3], Pillow.Resampling.BICUBIC)
        AhkTest.AssertEqual("L", resized.Mode)
        AhkTest.AssertEqual([5, 3], resized.Size)
        AhkTest.AssertEqual([
            0, 0, 20, 53, 73,
            58, 73, 105, 148, 170,
            123, 147, 190, 242, 255,
        ], PillowTestBufferToArray(resized.ToBytes()))
    } finally {
        if IsObject(resized)
            resized.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Resize BICUBIC resizes through native handles", PillowTestImageResizeBicubicUsesNativeHandleOperation)

PillowTestImageResizeDefaultsMatchPillowResamplingRules(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [2, 2], PillowTestBuffer([
        1, 2, 3, 10, 20, 30,
        100, 110, 120, 200, 210, 220,
    ]))
    l := Pillow.Image.FromBytes("L", [2, 2], PillowTestBuffer([1, 10, 100, 200]))
    p := Pillow.Image.FromBytes("P", [2, 2], PillowTestBuffer([0, 1, 2, 3]))
    rgbDefault := 0
    lDefault := 0
    pDefault := 0
    try {
        rgbDefault := rgb.Resize([3, 3])
        lDefault := l.Resize([3, 3])
        pDefault := p.Resize([3, 3])

        AhkTest.AssertEqual([
            0, 0, 0, 0, 1, 7, 0, 8, 19,
            47, 52, 57, 78, 86, 94, 109, 119, 130,
            99, 110, 120, 159, 170, 180, 220, 230, 240,
        ], PillowTestBufferToArray(rgbDefault.ToBytes()))
        AhkTest.AssertEqual([0, 0, 0, 47, 78, 109, 99, 159, 220], PillowTestBufferToArray(lDefault.ToBytes()))
        AhkTest.AssertEqual([0, 1, 1, 2, 3, 3, 2, 3, 3], PillowTestBufferToArray(pDefault.ToBytes()))
    } finally {
        for image in [pDefault, lDefault, rgbDefault, p, l, rgb] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Resize defaults match Pillow resampling rules", PillowTestImageResizeDefaultsMatchPillowResamplingRules)

PillowTestImageResizeBoxUsesNativeSampling(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [3, 3], PillowTestBuffer([
        0, 0, 0, 50, 0, 0, 100, 0, 0,
        0, 50, 0, 50, 50, 0, 100, 50, 0,
        0, 100, 0, 50, 100, 0, 100, 100, 0,
    ]))
    p := Pillow.Image.FromBytes("P", [4, 3], PillowTestBuffer([
        0, 1, 2, 3,
        1, 2, 3, 0,
        2, 3, 0, 1,
    ]))
    palette := [
        0, 0, 0,
        10, 20, 30,
        200, 100, 50,
        255, 255, 255,
    ]
    rgbOut := 0
    pOut := 0
    pIdentity := 0
    try {
        p.PutPalette(palette)

        rgbOut := rgb.Resize([2, 2], Pillow.Resampling.BILINEAR, [0.5, 0.5, 2.5, 2.5])
        pOut := p.Resize([2, 2], Pillow.Resampling.NEAREST, [1.0, 0.0, 4.0, 3.0])
        pIdentity := p.Resize([4, 3], Pillow.Resampling.NEAREST, [0.0, 0.0, 4.0, 3.0])

        AhkTest.AssertEqual("RGB", rgbOut.Mode)
        AhkTest.AssertEqual([2, 2], rgbOut.Size)
        AhkTest.AssertEqual([25, 25, 0, 75, 25, 0, 25, 75, 0, 75, 75, 0], PillowTestBufferToArray(rgbOut.ToBytes()))

        AhkTest.AssertEqual("P", pOut.Mode)
        AhkTest.AssertEqual([2, 2], pOut.Size)
        AhkTest.AssertEqual([1, 3, 3, 1], PillowTestBufferToArray(pOut.ToBytes()))
        AhkTest.AssertEqual(palette, pOut.GetPalette())
        AhkTest.AssertEqual(palette, pIdentity.GetPalette())
    } finally {
        for image in [pIdentity, pOut, rgbOut, p, rgb] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Resize supports native box sampling", PillowTestImageResizeBoxUsesNativeSampling)

PillowTestImageResizeReducingGapUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    lBytes := []
    loop 8 {
        y := A_Index - 1
        loop 8 {
            x := A_Index - 1
            lBytes.Push(y * 20 + x * 3)
        }
    }
    source := Pillow.Image.FromBytes("L", [8, 8], PillowTestBuffer(lBytes))
    resized := 0
    try {
        resized := source.Resize([2, 2], Pillow.Resampling.BILINEAR, , 2.0)
        AhkTest.AssertEqual("L", resized.Mode)
        AhkTest.AssertEqual([2, 2], resized.Size)
        AhkTest.AssertEqual([45, 55, 107, 117], PillowTestBufferToArray(resized.ToBytes()))
    } finally {
        if IsObject(resized)
            resized.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Resize supports native reducing_gap", PillowTestImageResizeReducingGapUsesNativePath)

PillowTestImageThumbnailMutatesThroughNativeResize(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [4, 2], PillowTestBuffer([0, 1, 2, 3, 4, 5, 6, 7]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 5], PillowTestBuffer([
        0, 1, 2, 3, 4, 5, 6, 7, 8,
        9, 10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25, 26,
        27, 28, 29, 30, 31, 32, 33, 34, 35,
        36, 37, 38, 39, 40, 41, 42, 43, 44,
    ]))
    noUpscale := Pillow.Image.FromBytes("L", [2, 2], PillowTestBuffer([1, 2, 3, 4]))
    floorSize := Pillow.Image.FromBytes("L", [7, 3], PillowTestBuffer([
        0, 1, 2, 3, 4, 5, 6,
        7, 8, 9, 10, 11, 12, 13,
        14, 15, 16, 17, 18, 19, 20,
    ]))
    try {
        returned := l.Thumbnail([2, 2], Pillow.Resampling.NEAREST)
        AhkTest.AssertEqual("", returned)
        AhkTest.AssertEqual([2, 1], l.Size)
        AhkTest.AssertEqual([5, 7], PillowTestBufferToArray(l.ToBytes()))

        rgb.Thumbnail([2, 2], Pillow.Resampling.NEAREST)
        AhkTest.AssertEqual([1, 2], rgb.Size)
        AhkTest.AssertEqual([12, 13, 14, 30, 31, 32], PillowTestBufferToArray(rgb.ToBytes()))

        noUpscale.Thumbnail([9, 9], Pillow.Resampling.NEAREST)
        AhkTest.AssertEqual([2, 2], noUpscale.Size)
        AhkTest.AssertEqual([1, 2, 3, 4], PillowTestBufferToArray(noUpscale.ToBytes()))

        floorSize.Thumbnail([4.9, 2.1], Pillow.Resampling.NEAREST, unset)
        AhkTest.AssertEqual([4, 2], floorSize.Size)
        AhkTest.AssertEqual([0, 2, 4, 6, 14, 16, 18, 20], PillowTestBufferToArray(floorSize.ToBytes()))
    } finally {
        floorSize.Close()
        noUpscale.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.Thumbnail mutates images through native resize", PillowTestImageThumbnailMutatesThroughNativeResize)

PillowTestImageThumbnailRejectsInvalidParameters(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.New("L", [3, 2])
    try {
        for args in [
            ["size", "bad", Pillow.Resampling.NEAREST],
            ["size", [0, 1], Pillow.Resampling.NEAREST],
            ["size", [1, 0], Pillow.Resampling.NEAREST],
            ["size", ["2", 2], Pillow.Resampling.NEAREST],
            ["reducingGap", [1, 1], Pillow.Resampling.NEAREST, 0.5],
        ] {
            try {
                if args.Length = 4
                    image.Thumbnail(args[2], args[3], args[4])
                else
                    image.Thumbnail(args[2], args[3])
                AhkTest.Fail("Expected Thumbnail to reject invalid parameters")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, args[1]) > 0 || InStr(err.Message, "width") > 0 || InStr(err.Message, "number") > 0)
            }
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow Image.Thumbnail rejects invalid parameters", PillowTestImageThumbnailRejectsInvalidParameters)

PillowTestImageReduceUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [5, 3], PillowTestBuffer([
        0, 1, 2, 3, 4,
        5, 6, 7, 8, 9,
        10, 11, 12, 13, 14,
    ]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        10, 20, 30, 40, 50, 60, 70, 80, 90,
        100, 110, 120, 130, 140, 150, 160, 170, 180,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [2, 2], PillowTestBuffer([
        10, 20, 30, 0, 100, 110, 120, 128,
        200, 210, 220, 255, 50, 60, 70, 64,
    ]))
    la := Pillow.Image.FromBytes("LA", [3, 2], PillowTestBuffer([
        10, 0, 100, 128, 200, 255,
        50, 64, 30, 32, 240, 200,
    ]))
    cmyk := Pillow.Image.FromBytes("CMYK", [4, 4], PillowTestBuffer([
        0, 0, 0, 0, 20, 40, 60, 80, 100, 50, 25, 125, 255, 128, 64, 32,
        10, 200, 30, 40, 200, 10, 220, 0, 40, 50, 60, 70, 80, 90, 100, 110,
        120, 130, 140, 150, 160, 170, 180, 190, 200, 210, 220, 230, 240, 250, 5, 15,
        25, 35, 45, 55, 65, 75, 85, 95, 105, 115, 125, 135, 145, 155, 165, 175,
    ]))
    lReduced := 0
    lTuple := 0
    lBox := 0
    rgbReduced := 0
    rgbaReduced := 0
    laReduced := 0
    cmykReduced := 0
    cmykTuple := 0
    cmykBox := 0
    try {
        lReduced := l.Reduce(2)
        lTuple := l.Reduce([2, 3])
        lBox := l.Reduce(2, [1, 0, 5, 3])
        rgbReduced := rgb.Reduce(2)
        rgbaReduced := rgba.Reduce(2)
        laReduced := la.Reduce([2, 1])
        cmykReduced := cmyk.Reduce(2)
        cmykTuple := cmyk.Reduce([2, 1])
        cmykBox := cmyk.Reduce(2, [1, 1, 4, 4])

        AhkTest.AssertEqual("L", lReduced.Mode)
        AhkTest.AssertEqual([3, 2], lReduced.Size)
        AhkTest.AssertEqual([3, 5, 7, 11, 13, 14], PillowTestBufferToArray(lReduced.ToBytes()))
        AhkTest.AssertEqual([3, 1], lTuple.Size)
        AhkTest.AssertEqual([5, 7, 9], PillowTestBufferToArray(lTuple.ToBytes()))
        AhkTest.AssertEqual([2, 2], lBox.Size)
        AhkTest.AssertEqual([4, 6, 12, 14], PillowTestBufferToArray(lBox.ToBytes()))
        AhkTest.AssertEqual("RGB", rgbReduced.Mode)
        AhkTest.AssertEqual([2, 1], rgbReduced.Size)
        AhkTest.AssertEqual([70, 80, 90, 115, 125, 135], PillowTestBufferToArray(rgbReduced.ToBytes()))
        AhkTest.AssertEqual("RGBA", rgbaReduced.Mode)
        AhkTest.AssertEqual([1, 1], rgbaReduced.Size)
        AhkTest.AssertEqual([150, 159, 170, 112], PillowTestBufferToArray(rgbaReduced.ToBytes()))
        AhkTest.AssertEqual("LA", laReduced.Mode)
        AhkTest.AssertEqual([2, 2], laReduced.Size)
        AhkTest.AssertEqual([99, 64, 200, 255, 47, 48, 239, 200], PillowTestBufferToArray(laReduced.ToBytes()))
        AhkTest.AssertEqual("CMYK", cmykReduced.Mode)
        AhkTest.AssertEqual([2, 2], cmykReduced.Size)
        AhkTest.AssertEqual([58, 63, 78, 30, 119, 80, 62, 84, 93, 103, 113, 123, 173, 183, 129, 139], PillowTestBufferToArray(cmykReduced.ToBytes()))
        AhkTest.AssertEqual([2, 4], cmykTuple.Size)
        AhkTest.AssertEqual([10, 20, 30, 40, 178, 89, 45, 79, 105, 105, 125, 20, 60, 70, 80, 90, 140, 150, 160, 170, 220, 230, 113, 123, 45, 55, 65, 75, 125, 135, 145, 155], PillowTestBufferToArray(cmykTuple.ToBytes()))
        AhkTest.AssertEqual([2, 2], cmykBox.Size)
        AhkTest.AssertEqual([150, 110, 170, 123, 160, 170, 53, 63, 85, 95, 105, 115, 145, 155, 165, 175], PillowTestBufferToArray(cmykBox.ToBytes()))
    } finally {
        for image in [cmykBox, cmykTuple, cmykReduced, laReduced, rgbaReduced, rgbReduced, lBox, lTuple, lReduced, cmyk, la, rgba, rgb, l] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Reduce uses native integer downsampling", PillowTestImageReduceUsesNativeOperation)

PillowTestImageFilterKernelUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [5, 5], PillowTestBuffer([
        0, 1, 2, 3, 4,
        5, 6, 7, 8, 9,
        10, 11, 12, 13, 14,
        15, 16, 17, 18, 19,
        20, 21, 22, 23, 24,
    ]))
    rgb := Pillow.Image.FromBytes("RGB", [4, 3], PillowTestBuffer([
        1, 2, 3, 10, 20, 30, 40, 50, 60, 70, 80, 90,
        90, 80, 70, 120, 110, 100, 150, 140, 130, 180, 170, 160,
        5, 15, 25, 35, 45, 55, 65, 75, 85, 95, 105, 115,
    ]))
    scaled := 0
    sharpened := 0
    try {
        scaled := source.Filter(Pillow.ImageFilter.Kernel([3, 3], [0, 0, 0, 0, 1, 0, 0, 0, 0], 2.0, 0.5))
        sharpened := rgb.Filter(Pillow.ImageFilter.Kernel([3, 3], [0, -1, 0, -1, 5, -1, 0, -1, 0], 1.0, 0.0))

        AhkTest.AssertEqual("L", scaled.Mode)
        AhkTest.AssertEqual([5, 5], scaled.Size)
        AhkTest.AssertEqual([
            0, 1, 2, 3, 4,
            5, 4, 4, 5, 9,
            10, 6, 7, 7, 14,
            15, 9, 9, 10, 19,
            20, 21, 22, 23, 24,
        ], PillowTestBufferToArray(scaled.ToBytes()))
        AhkTest.AssertEqual([
            1, 2, 3, 10, 20, 30, 40, 50, 60, 70, 80, 90,
            90, 80, 70, 255, 255, 215, 255, 255, 245, 180, 170, 160,
            5, 15, 25, 35, 45, 55, 65, 75, 85, 95, 105, 115,
        ], PillowTestBufferToArray(sharpened.ToBytes()))
    } finally {
        for image in [sharpened, scaled, rgb, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Filter applies ImageFilter.Kernel through native path", PillowTestImageFilterKernelUsesNativePath)

PillowTestImageFilterKernelRejectsInvalidWrapperArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.New("L", [3, 3])
    try {
        try {
            badSize := Pillow.ImageFilter.Kernel([2, 2], [1, 0, 0, 0])
            source.Filter(badSize)
            AhkTest.Fail("Expected Image.Filter to reject unsupported kernel size")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "bad kernel size") > 0)
        }

        try {
            Pillow.ImageFilter.Kernel([3, 3], [1, 0, 0, 0])
            AhkTest.Fail("Expected Kernel to reject coefficient length")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "not enough coefficients") > 0)
        }

        try {
            source.Filter({ Apply: "not callable" })
            AhkTest.Fail("Expected Image.Filter to reject invalid filter")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "filter") > 0)
        }
    } finally {
        source.Close()
    }
}

AhkTest.Test("Pillow ImageFilter.Kernel rejects invalid wrapper arguments", PillowTestImageFilterKernelRejectsInvalidWrapperArguments)

PillowTestImageFilterBuiltinsUseNativeKernelPath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [4, 3], PillowTestBuffer([
        1, 2, 3, 10, 20, 30, 40, 50, 60, 70, 80, 90,
        90, 80, 70, 120, 110, 100, 150, 140, 130, 180, 170, 160,
        5, 15, 25, 35, 45, 55, 65, 75, 85, 95, 105, 115,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [3, 3], PillowTestBuffer([
        1, 2, 3, 4, 10, 20, 30, 40, 50, 60, 70, 80,
        90, 80, 70, 60, 120, 110, 100, 90, 150, 140, 130, 120,
        5, 15, 25, 35, 35, 45, 55, 65, 65, 75, 85, 95,
    ]))
    outputs := []
    try {
        cases := [
            { Filter: Pillow.ImageFilter.CONTOUR(), Name: "Contour", RgbBytes: [255, 255, 255, 255, 255, 255], RgbaCenter: [255, 255, 255, 255] },
            { Filter: Pillow.ImageFilter.DETAIL(), Name: "Detail", RgbBytes: [153, 136, 119, 183, 166, 149], RgbaCenter: [153, 136, 119, 103] },
            { Filter: Pillow.ImageFilter.EDGE_ENHANCE(), Name: "Edge-enhance", RgbBytes: [255, 255, 255, 255, 255, 255], RgbaCenter: [255, 255, 255, 201] },
            { Filter: Pillow.ImageFilter.EDGE_ENHANCE_MORE(), Name: "Edge-enhance More", RgbBytes: [255, 255, 255, 255, 255, 255], RgbaCenter: [255, 255, 255, 255] },
            { Filter: Pillow.ImageFilter.EMBOSS(), Name: "Emboss", RgbBytes: [243, 223, 203, 243, 223, 203], RgbaCenter: [243, 223, 203, 183] },
            { Filter: Pillow.ImageFilter.FIND_EDGES(), Name: "Find Edges", RgbBytes: [255, 255, 255, 255, 255, 255], RgbaCenter: [255, 255, 255, 221] },
            { Filter: Pillow.ImageFilter.SHARPEN(), Name: "Sharpen", RgbBytes: [191, 167, 143, 223, 198, 173], RgbaCenter: [189, 165, 142, 118] },
            { Filter: Pillow.ImageFilter.SMOOTH(), Name: "Smooth", RgbBytes: [77, 75, 74, 105, 104, 103], RgbaCenter: [77, 76, 74, 73] },
        ]

        for item in cases {
            AhkTest.AssertEqual(item.Name, item.Filter.Name)
            rgbOut := rgb.Filter(item.Filter)
            rgbaOut := rgba.Filter(item.Filter)
            outputs.Push(rgbOut)
            outputs.Push(rgbaOut)
            rgbBytes := PillowTestBufferToArray(rgbOut.ToBytes())
            rgbaBytes := PillowTestBufferToArray(rgbaOut.ToBytes())
            AhkTest.AssertEqual(item.RgbBytes, [
                rgbBytes[16], rgbBytes[17], rgbBytes[18],
                rgbBytes[19], rgbBytes[20], rgbBytes[21],
            ])
            AhkTest.AssertEqual(item.RgbaCenter, [
                rgbaBytes[17], rgbaBytes[18], rgbaBytes[19], rgbaBytes[20],
            ])
        }
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        rgba.Close()
        rgb.Close()
    }
}

AhkTest.Test("Pillow ImageFilter builtins use native Kernel path", PillowTestImageFilterBuiltinsUseNativeKernelPath)

PillowTestImageFilterFiveByFiveBuiltinsUseNativeKernelPath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [7, 7], PillowTestBuffer([
        0, 11, 44, 99, 176, 19, 140,
        78, 102, 148, 216, 50, 162, 40,
        156, 193, 252, 77, 180, 49, 196,
        111, 161, 233, 71, 187, 69, 229,
        189, 252, 81, 188, 61, 212, 129,
        11, 87, 185, 49, 191, 99, 29,
        222, 55, 166, 43, 198, 119, 62,
    ]))
    blur := 0
    smoothMore := 0
    try {
        blur := source.Filter(Pillow.ImageFilter.BLUR())
        smoothMore := source.Filter(Pillow.ImageFilter.SMOOTH_MORE())
        blurBytes := PillowTestBufferToArray(blur.ToBytes())
        smoothBytes := PillowTestBufferToArray(smoothMore.ToBytes())

        AhkTest.AssertEqual("Blur", Pillow.ImageFilter.BLUR().Name)
        AhkTest.AssertEqual("Smooth More", Pillow.ImageFilter.SMOOTH_MORE().Name)
        AhkTest.AssertEqual([
            116, 117, 140,
            125, 139, 143,
            143, 138, 139,
        ], [
            blurBytes[17], blurBytes[18], blurBytes[19],
            blurBytes[24], blurBytes[25], blurBytes[26],
            blurBytes[31], blurBytes[32], blurBytes[33],
        ])
        AhkTest.AssertEqual([
            190, 120, 146,
            186, 116, 151,
            120, 158, 102,
        ], [
            smoothBytes[17], smoothBytes[18], smoothBytes[19],
            smoothBytes[24], smoothBytes[25], smoothBytes[26],
            smoothBytes[31], smoothBytes[32], smoothBytes[33],
        ])
    } finally {
        for image in [smoothMore, blur, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow 5x5 ImageFilter builtins use native Kernel path", PillowTestImageFilterFiveByFiveBuiltinsUseNativeKernelPath)

PillowTestImageFilterRankFiltersUseNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [5, 5], PillowTestBuffer([
        0, 200, 2, 180, 4,
        50, 6, 70, 8, 90,
        10, 110, 12, 130, 14,
        150, 16, 170, 18, 190,
        20, 210, 22, 230, 24,
    ]))
    outputs := []
    try {
        cases := [
            { Filter: Pillow.ImageFilter.RankFilter(3, 0), Name: "Rank", Bytes: [
                0, 0, 2, 2, 4,
                0, 0, 2, 2, 4,
                6, 6, 6, 8, 8,
                10, 10, 12, 12, 14,
                16, 16, 16, 18, 18,
            ] },
            { Filter: Pillow.ImageFilter.MinFilter(3), Name: "Min", Bytes: [
                0, 0, 2, 2, 4,
                0, 0, 2, 2, 4,
                6, 6, 6, 8, 8,
                10, 10, 12, 12, 14,
                16, 16, 16, 18, 18,
            ] },
            { Filter: Pillow.ImageFilter.MedianFilter(3), Name: "Median", Bytes: [
                6, 6, 70, 8, 8,
                10, 12, 70, 14, 14,
                50, 50, 18, 70, 90,
                20, 22, 110, 24, 24,
                20, 22, 170, 24, 24,
            ] },
            { Filter: Pillow.ImageFilter.MaxFilter(3), Name: "Max", Bytes: [
                200, 200, 200, 180, 180,
                200, 200, 200, 180, 180,
                150, 170, 170, 190, 190,
                210, 210, 230, 230, 230,
                210, 210, 230, 230, 230,
            ] },
            { Filter: Pillow.ImageFilter.RankFilter(7, 24), Name: "Rank", Bytes: [
                10, 10, 12, 14, 14,
                20, 20, 20, 20, 22,
                20, 20, 22, 24, 24,
                20, 22, 24, 24, 24,
                20, 22, 24, 24, 24,
            ] },
        ]

        for item in cases {
            AhkTest.AssertEqual(item.Name, item.Filter.Name)
            out := source.Filter(item.Filter)
            outputs.Push(out)
            AhkTest.AssertEqual(item.Bytes, PillowTestBufferToArray(out.ToBytes()))
        }
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        source.Close()
    }
}

AhkTest.Test("Pillow ImageFilter rank filters use native path", PillowTestImageFilterRankFiltersUseNativePath)

PillowTestImageFilterRankFiltersSupportRgbRgba(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [4, 3], PillowTestBuffer([
        1, 2, 3, 10, 20, 30, 40, 50, 60, 70, 80, 90,
        90, 80, 70, 120, 110, 100, 150, 140, 130, 180, 170, 160,
        5, 15, 25, 35, 45, 55, 65, 75, 85, 95, 105, 115,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [3, 3], PillowTestBuffer([
        1, 2, 3, 4, 10, 20, 30, 40, 50, 60, 70, 80,
        90, 80, 70, 60, 120, 110, 100, 90, 150, 140, 130, 120,
        5, 15, 25, 35, 35, 45, 55, 65, 65, 75, 85, 95,
    ]))
    rgbOut := 0
    rgbaOut := 0
    try {
        rgbOut := rgb.Filter(Pillow.ImageFilter.MedianFilter(3))
        rgbaOut := rgba.Filter(Pillow.ImageFilter.MedianFilter(3))
        AhkTest.AssertEqual([
            10, 20, 30, 40, 50, 60, 70, 80, 90, 70, 80, 90,
            10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 105, 115,
            35, 45, 55, 65, 75, 70, 95, 105, 100, 95, 105, 115,
        ], PillowTestBufferToArray(rgbOut.ToBytes()))
        AhkTest.AssertEqual([
            10, 20, 30, 40, 50, 60, 70, 60, 50, 60, 70, 80,
            10, 20, 30, 40, 50, 60, 70, 65, 65, 75, 85, 90,
            35, 45, 55, 60, 65, 75, 70, 65, 65, 75, 85, 95,
        ], PillowTestBufferToArray(rgbaOut.ToBytes()))
    } finally {
        for image in [rgbaOut, rgbOut, rgba, rgb] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageFilter rank filters support RGB and RGBA", PillowTestImageFilterRankFiltersSupportRgbRgba)

PillowTestImageFilterRankFiltersRejectInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [3, 3], PillowTestBuffer([0, 1, 2, 3, 4, 5, 6, 7, 8]))
    out := 0
    try {
        out := source.Filter(Pillow.ImageFilter.RankFilter(1, 0))
        AhkTest.AssertEqual([0, 1, 2, 3, 4, 5, 6, 7, 8], PillowTestBufferToArray(out.ToBytes()))

        for item in [
            { Filter: Pillow.ImageFilter.RankFilter(2, 0), Text: "bad filter size" },
            { Filter: Pillow.ImageFilter.RankFilter(3, -1), Text: "bad rank value" },
            { Filter: Pillow.ImageFilter.RankFilter(3, 9), Text: "bad rank value" },
            { Filter: Pillow.ImageFilter.MaxFilter(0), Text: "bad filter size" },
        ] {
            try {
                source.Filter(item.Filter)
                AhkTest.Fail("Expected rank filter failure")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, item.Text) > 0)
            }
        }
    } finally {
        if IsObject(out)
            out.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow ImageFilter rank filters reject invalid arguments", PillowTestImageFilterRankFiltersRejectInvalidArguments)

PillowTestImageFilterModeFilterUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [3, 3], PillowTestBuffer([
        0, 0, 0,
        0, 0, 0,
        1, 1, 1,
    ]))
    outputs := []
    try {
        cases := [
            { Filter: Pillow.ImageFilter.ModeFilter(1), Name: "Mode", Bytes: [
                0, 0, 0,
                0, 0, 0,
                1, 1, 1,
            ] },
            { Filter: Pillow.ImageFilter.ModeFilter(2), Name: "Mode", Bytes: [
                0, 0, 0,
                0, 0, 0,
                1, 0, 1,
            ] },
            { Filter: Pillow.ImageFilter.ModeFilter(4), Name: "Mode", Bytes: [
                0, 0, 0,
                0, 0, 0,
                0, 0, 0,
            ] },
        ]

        for item in cases {
            AhkTest.AssertEqual(item.Name, item.Filter.Name)
            out := source.Filter(item.Filter)
            outputs.Push(out)
            AhkTest.AssertEqual(item.Bytes, PillowTestBufferToArray(out.ToBytes()))
        }
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        source.Close()
    }
}

AhkTest.Test("Pillow ImageFilter ModeFilter uses native path", PillowTestImageFilterModeFilterUsesNativePath)

PillowTestImageFilterModeFilterSupportsRgbRgba(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [3, 3], PillowTestBuffer([
        0, 10, 200, 0, 20, 200, 0, 30, 100,
        0, 40, 200, 9, 50, 100, 9, 60, 100,
        1, 70, 200, 1, 80, 100, 1, 90, 100,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [3, 3], PillowTestBuffer([
        0, 10, 200, 1, 0, 20, 200, 2, 0, 30, 100, 3,
        0, 40, 200, 4, 9, 50, 100, 5, 9, 60, 100, 6,
        1, 70, 200, 7, 1, 80, 100, 8, 1, 90, 100, 9,
    ]))
    rgbOut := 0
    rgbaOut := 0
    try {
        rgbOut := rgb.Filter(Pillow.ImageFilter.ModeFilter(3))
        rgbaOut := rgba.Filter(Pillow.ImageFilter.ModeFilter(3))
        AhkTest.AssertEqual([
            0, 10, 200, 0, 20, 100, 0, 30, 100,
            0, 40, 200, 0, 50, 100, 9, 60, 100,
            1, 70, 200, 1, 80, 100, 1, 90, 100,
        ], PillowTestBufferToArray(rgbOut.ToBytes()))
        AhkTest.AssertEqual([
            0, 10, 200, 1, 0, 20, 100, 2, 0, 30, 100, 3,
            0, 40, 200, 4, 0, 50, 100, 5, 9, 60, 100, 6,
            1, 70, 200, 7, 1, 80, 100, 8, 1, 90, 100, 9,
        ], PillowTestBufferToArray(rgbaOut.ToBytes()))
    } finally {
        for image in [rgbaOut, rgbOut, rgba, rgb] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageFilter ModeFilter supports RGB and RGBA", PillowTestImageFilterModeFilterSupportsRgbRgba)

PillowTestImageFilterModeFilterKeepsSparseModesAndTies(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    threshold := Pillow.Image.FromBytes("L", [3, 3], PillowTestBuffer([
        1, 1, 2,
        2, 3, 4,
        5, 6, 7,
    ]))
    tie := Pillow.Image.FromBytes("L", [3, 3], PillowTestBuffer([
        1, 1, 1,
        2, 2, 2,
        3, 4, 5,
    ]))
    thresholdOut := 0
    tieOut := 0
    try {
        thresholdOut := threshold.Filter(Pillow.ImageFilter.ModeFilter(3))
        tieOut := tie.Filter(Pillow.ImageFilter.ModeFilter(3))

        AhkTest.AssertEqual([
            1, 1, 2,
            2, 3, 4,
            5, 6, 7,
        ], PillowTestBufferToArray(thresholdOut.ToBytes()))
        AhkTest.AssertEqual([
            1, 1, 1,
            2, 1, 2,
            3, 2, 5,
        ], PillowTestBufferToArray(tieOut.ToBytes()))
    } finally {
        for image in [tieOut, thresholdOut, tie, threshold] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageFilter ModeFilter keeps sparse modes and ties", PillowTestImageFilterModeFilterKeepsSparseModesAndTies)

PillowTestImageFilterBoxBlurUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([
        0, 30, 80, 120,
        160, 200, 220, 255,
        10, 40, 90, 140,
    ]))
    outputs := []
    try {
        cases := [
            { Filter: Pillow.ImageFilter.BoxBlur(0), Name: "BoxBlur", Bytes: [
                0, 30, 80, 120,
                160, 200, 220, 255,
                10, 40, 90, 140,
            ] },
            { Filter: Pillow.ImageFilter.BoxBlur(0.5), Name: "BoxBlur", Bytes: [
                49, 75, 115, 144,
                92, 118, 154, 183,
                56, 83, 124, 158,
            ] },
            { Filter: Pillow.ImageFilter.BoxBlur(1), Name: "BoxBlur", Bytes: [
                64, 89, 126, 152,
                68, 92, 131, 158,
                71, 96, 135, 163,
            ] },
            { Filter: Pillow.ImageFilter.BoxBlur([1.5, 0.5]), Name: "BoxBlur", Bytes: [
                58, 82, 110, 134,
                101, 123, 150, 173,
                66, 90, 120, 146,
            ] },
        ]

        for item in cases {
            AhkTest.AssertEqual(item.Name, item.Filter.Name)
            out := source.Filter(item.Filter)
            outputs.Push(out)
            AhkTest.AssertEqual(item.Bytes, PillowTestBufferToArray(out.ToBytes()))
        }
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        source.Close()
    }
}

AhkTest.Test("Pillow ImageFilter BoxBlur uses native path", PillowTestImageFilterBoxBlurUsesNativePath)

PillowTestImageFilterBoxBlurSupportsRgbRgba(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [4, 2], PillowTestBuffer([
        1, 2, 3, 20, 30, 40, 60, 70, 80, 100, 110, 120,
        130, 140, 150, 160, 170, 180, 200, 210, 220, 230, 240, 250,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [3, 2], PillowTestBuffer([
        1, 2, 3, 4, 20, 30, 40, 50, 60, 70, 80, 90,
        100, 110, 120, 130, 140, 150, 160, 170, 200, 210, 220, 230,
    ]))
    rgbOut := 0
    rgbaOut := 0
    try {
        rgbOut := rgb.Filter(Pillow.ImageFilter.BoxBlur([1.25, 0.5]))
        rgbaOut := rgba.Filter(Pillow.ImageFilter.BoxBlur([1.25, 0.5]))
        AhkTest.AssertEqual([
            44, 50, 55, 64, 72, 80, 93, 102, 111, 116, 126, 136,
            111, 119, 128, 132, 141, 151, 160, 170, 180, 183, 193, 203,
        ], PillowTestBufferToArray(rgbOut.ToBytes()))
        AhkTest.AssertEqual([
            38, 44, 49, 55, 58, 65, 73, 80, 77, 87, 96, 106,
            92, 101, 109, 118, 117, 126, 136, 145, 143, 153, 163, 173,
        ], PillowTestBufferToArray(rgbaOut.ToBytes()))
    } finally {
        for image in [rgbaOut, rgbOut, rgba, rgb] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageFilter BoxBlur supports RGB and RGBA", PillowTestImageFilterBoxBlurSupportsRgbRgba)

PillowTestImageFilterBoxBlurRejectsInvalidRadius(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.New("L", [1, 1])
    try {
        for radius in [-1, [0, -0.5], [1], [1, 2, 3]] {
            try {
                source.Filter(Pillow.ImageFilter.BoxBlur(radius))
                AhkTest.Fail("Expected BoxBlur to reject invalid radius")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, "radius") > 0 || InStr(err.Message, "BoxBlur") > 0)
            }
        }
    } finally {
        source.Close()
    }
}

AhkTest.Test("Pillow ImageFilter BoxBlur rejects invalid radius", PillowTestImageFilterBoxBlurRejectsInvalidRadius)

PillowTestImageFilterGaussianBlurUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([
        0, 30, 80, 120,
        160, 200, 220, 255,
        10, 40, 90, 140,
    ]))
    outputs := []
    try {
        cases := [
            { Filter: Pillow.ImageFilter.GaussianBlur(0), Name: "GaussianBlur", Bytes: [
                0, 30, 80, 120,
                160, 200, 220, 255,
                10, 40, 90, 140,
            ] },
            { Filter: Pillow.ImageFilter.GaussianBlur(0.5), Name: "GaussianBlur", Bytes: [
                21, 51, 95, 129,
                131, 163, 191, 224,
                30, 60, 104, 147,
            ] },
            { Filter: Pillow.ImageFilter.GaussianBlur(1), Name: "GaussianBlur", Bytes: [
                62, 84, 118, 143,
                85, 106, 139, 164,
                67, 91, 126, 153,
            ] },
            { Filter: Pillow.ImageFilter.GaussianBlur([1.5, 0.5]), Name: "GaussianBlur", Bytes: [
                46, 62, 86, 103,
                153, 167, 187, 201,
                54, 73, 98, 116,
            ] },
            { Filter: Pillow.ImageFilter.GaussianBlur(), Name: "GaussianBlur", Bytes: [
                82, 94, 109, 121,
                83, 95, 110, 122,
                84, 96, 111, 123,
            ] },
        ]

        for item in cases {
            AhkTest.AssertEqual(item.Name, item.Filter.Name)
            out := source.Filter(item.Filter)
            outputs.Push(out)
            AhkTest.AssertEqual(item.Bytes, PillowTestBufferToArray(out.ToBytes()))
        }
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        source.Close()
    }
}

AhkTest.Test("Pillow ImageFilter GaussianBlur uses native path", PillowTestImageFilterGaussianBlurUsesNativePath)

PillowTestImageFilterGaussianBlurSupportsRgbRgba(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [4, 2], PillowTestBuffer([
        1, 2, 3, 20, 30, 40, 60, 70, 80, 100, 110, 120,
        130, 140, 150, 160, 170, 180, 200, 210, 220, 230, 240, 250,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [3, 2], PillowTestBuffer([
        1, 2, 3, 4, 20, 30, 40, 50, 60, 70, 80, 90,
        100, 110, 120, 130, 140, 150, 160, 170, 200, 210, 220, 230,
    ]))
    rgbOut := 0
    rgbaOut := 0
    try {
        rgbOut := rgb.Filter(Pillow.ImageFilter.GaussianBlur([1.25, 0.5]))
        rgbaOut := rgba.Filter(Pillow.ImageFilter.GaussianBlur([1.25, 0.5]))
        AhkTest.AssertEqual([
            32, 36, 43, 48, 55, 63, 73, 82, 91, 92, 102, 112,
            134, 144, 153, 152, 162, 172, 176, 186, 196, 194, 204, 214,
        ], PillowTestBufferToArray(rgbOut.ToBytes()))
        AhkTest.AssertEqual([
            28, 33, 39, 44, 41, 48, 55, 63, 54, 63, 72, 81,
            113, 123, 132, 142, 132, 142, 152, 161, 152, 162, 172, 182,
        ], PillowTestBufferToArray(rgbaOut.ToBytes()))
    } finally {
        for image in [rgbaOut, rgbOut, rgba, rgb] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageFilter GaussianBlur supports RGB and RGBA", PillowTestImageFilterGaussianBlurSupportsRgbRgba)

PillowTestImageFilterGaussianBlurRejectsInvalidRadiusShape(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.New("L", [1, 1])
    try {
        for radius in [[1], [1, 2, 3], "x"] {
            try {
                source.Filter(Pillow.ImageFilter.GaussianBlur(radius))
                AhkTest.Fail("Expected GaussianBlur to reject invalid radius")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, "radius") > 0)
            }
        }
    } finally {
        source.Close()
    }
}

AhkTest.Test("Pillow ImageFilter GaussianBlur rejects invalid radius shape", PillowTestImageFilterGaussianBlurRejectsInvalidRadiusShape)

PillowTestImageFilterUnsharpMaskUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([
        0, 30, 80, 120,
        160, 200, 220, 255,
        10, 40, 90, 140,
    ]))
    outputs := []
    try {
        cases := [
            { Filter: Pillow.ImageFilter.UnsharpMask(), Name: "UnsharpMask", Bytes: [
                0, 0, 37, 120,
                255, 255, 255, 255,
                0, 0, 59, 165,
            ] },
            { Filter: Pillow.ImageFilter.UnsharpMask(0), Name: "UnsharpMask", Bytes: [
                0, 30, 80, 120,
                160, 200, 220, 255,
                10, 40, 90, 140,
            ] },
            { Filter: Pillow.ImageFilter.UnsharpMask(0.5, 150, 3), Name: "UnsharpMask", Bytes: [
                0, 0, 58, 107,
                203, 255, 255, 255,
                0, 10, 69, 130,
            ] },
            { Filter: Pillow.ImageFilter.UnsharpMask(1, 200, 0), Name: "UnsharpMask", Bytes: [
                0, 0, 4, 74,
                255, 255, 255, 255,
                0, 0, 18, 114,
            ] },
            { Filter: Pillow.ImageFilter.UnsharpMask(1, 50, 20), Name: "UnsharpMask", Bytes: [
                0, 3, 61, 109,
                197, 247, 255, 255,
                0, 15, 72, 140,
            ] },
        ]

        for item in cases {
            AhkTest.AssertEqual(item.Name, item.Filter.Name)
            out := source.Filter(item.Filter)
            outputs.Push(out)
            AhkTest.AssertEqual(item.Bytes, PillowTestBufferToArray(out.ToBytes()))
        }
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        source.Close()
    }
}

AhkTest.Test("Pillow ImageFilter UnsharpMask uses native path", PillowTestImageFilterUnsharpMaskUsesNativePath)

PillowTestImageFilterUnsharpMaskSupportsRgbRgba(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [4, 2], PillowTestBuffer([
        1, 2, 3, 20, 30, 40, 60, 70, 80, 100, 110, 120,
        130, 140, 150, 160, 170, 180, 200, 210, 220, 230, 240, 250,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [3, 2], PillowTestBuffer([
        1, 2, 3, 4, 20, 30, 40, 50, 60, 70, 80, 90,
        100, 110, 120, 130, 140, 150, 160, 170, 200, 210, 220, 230,
    ]))
    rgbOut := 0
    rgbaOut := 0
    try {
        rgbOut := rgb.Filter(Pillow.ImageFilter.UnsharpMask(1.25, 200, 0))
        rgbaOut := rgba.Filter(Pillow.ImageFilter.UnsharpMask(1.25, 200, 0))
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0, 28, 38, 48,
            210, 224, 238, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        ], PillowTestBufferToArray(rgbOut.ToBytes()))
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 22,
            148, 162, 176, 190, 234, 246, 255, 255, 255, 255, 255, 255,
        ], PillowTestBufferToArray(rgbaOut.ToBytes()))
    } finally {
        for image in [rgbaOut, rgbOut, rgba, rgb] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageFilter UnsharpMask supports RGB and RGBA", PillowTestImageFilterUnsharpMaskSupportsRgbRgba)

PillowTestImageFilterLaUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    la := Pillow.Image.FromBytes("LA", [3, 3], PillowTestBuffer([
        10, 0, 40, 64, 80, 128,
        30, 255, 120, 128, 220, 32,
        90, 64, 160, 192, 250, 255,
    ]))
    outputs := []
    try {
        cases := [
            { Out: la.Filter(Pillow.ImageFilter.Kernel([3, 3], [1, 2, 1, 2, 4, 2, 1, 2, 1], 16, 3)), Bytes: [
                10, 0, 40, 64, 80, 128, 30, 255, 116, 131, 220, 32, 90, 64, 160, 192, 250, 255,
            ] },
            { Out: la.Filter(Pillow.ImageFilter.MedianFilter(3)), Bytes: [
                30, 64, 40, 64, 80, 128, 40, 64, 90, 128, 160, 128, 90, 128, 160, 192, 220, 192,
            ] },
            { Out: la.Filter(Pillow.ImageFilter.ModeFilter(3)), Bytes: [
                10, 0, 40, 64, 80, 128, 30, 255, 120, 128, 220, 32, 90, 64, 160, 192, 250, 255,
            ] },
            { Out: la.Filter(Pillow.ImageFilter.BoxBlur(1)), Bytes: [
                33, 85, 70, 89, 107, 93, 64, 114, 111, 124, 158, 135, 95, 142, 152, 159, 209, 177,
            ] },
            { Out: la.Filter(Pillow.ImageFilter.GaussianBlur(1)), Bytes: [
                42, 84, 74, 92, 107, 100, 70, 126, 112, 126, 155, 125, 99, 134, 146, 155, 193, 171,
            ] },
            { Out: la.Filter(Pillow.ImageFilter.UnsharpMask(1, 150, 0)), Bytes: [
                0, 0, 0, 22, 40, 170, 0, 255, 132, 131, 255, 0, 77, 0, 181, 247, 255, 255,
            ] },
        ]

        for item in cases {
            outputs.Push(item.Out)
            AhkTest.AssertEqual("LA", item.Out.Mode)
            AhkTest.AssertEqual([3, 3], item.Out.Size)
            AhkTest.AssertEqual(item.Bytes, PillowTestBufferToArray(item.Out.ToBytes()))
        }
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        la.Close()
    }
}

AhkTest.Test("Pillow ImageFilter supports LA through native path", PillowTestImageFilterLaUsesNativePath)

PillowTestImageFilterCmykUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    cmyk := Pillow.Image.FromBytes("CMYK", [4, 3], PillowTestBuffer([
        1, 2, 3, 4, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130,
        130, 140, 150, 160, 160, 170, 180, 190, 200, 210, 220, 230, 230, 240, 250, 255,
        5, 15, 25, 35, 35, 45, 55, 65, 65, 75, 85, 95, 95, 105, 115, 125,
    ]))
    modeSource := Pillow.Image.FromBytes("CMYK", [3, 3], PillowTestBuffer([
        0, 10, 200, 5, 0, 20, 200, 5, 0, 30, 100, 7,
        0, 40, 200, 5, 9, 50, 100, 5, 9, 60, 100, 7,
        1, 70, 200, 9, 1, 80, 100, 9, 1, 90, 100, 9,
    ]))
    outputs := []
    try {
        cases := [
            { Out: cmyk.Filter(Pillow.ImageFilter.Kernel([3, 3], [0, -1, 0, -1, 5, -1, 0, -1, 0])), Bytes: [
                1, 2, 3, 4, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130,
                130, 140, 150, 160, 255, 255, 255, 255, 255, 255, 255, 255, 230, 240, 250, 255,
                5, 15, 25, 35, 35, 45, 55, 65, 65, 75, 85, 95, 95, 105, 115, 125,
            ] },
            { Out: cmyk.Filter(Pillow.ImageFilter.MedianFilter(3)), Bytes: [
                20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 100, 110, 120, 130,
                20, 30, 40, 50, 60, 70, 80, 90, 95, 105, 115, 125, 100, 110, 120, 130,
                35, 45, 55, 65, 65, 75, 85, 95, 95, 105, 115, 125, 95, 105, 115, 125,
            ] },
            { Out: modeSource.Filter(Pillow.ImageFilter.ModeFilter(3)), Bytes: [
                0, 10, 200, 5, 0, 20, 100, 5, 0, 30, 100, 7,
                0, 40, 200, 5, 0, 50, 100, 5, 9, 60, 100, 7,
                1, 70, 200, 9, 1, 80, 100, 9, 1, 90, 100, 9,
            ] },
            { Out: cmyk.Filter(Pillow.ImageFilter.BoxBlur([1.25, 0.5])), Bytes: [
                44, 50, 55, 61, 64, 72, 80, 87, 93, 102, 111, 121, 116, 126, 136, 146,
                79, 88, 96, 105, 100, 109, 118, 127, 128, 137, 147, 156, 149, 159, 169, 178,
                50, 60, 70, 80, 69, 79, 89, 99, 96, 106, 116, 125, 116, 126, 136, 145,
            ] },
            { Out: cmyk.Filter(Pillow.ImageFilter.GaussianBlur([1.25, 0.5])), Bytes: [
                31, 36, 42, 48, 47, 54, 63, 69, 72, 81, 90, 99, 91, 101, 111, 120,
                122, 131, 140, 150, 139, 148, 158, 167, 163, 172, 182, 191, 180, 190, 200, 208,
                38, 48, 58, 68, 53, 63, 73, 83, 75, 85, 95, 105, 91, 101, 111, 120,
            ] },
            { Out: cmyk.Filter(Pillow.ImageFilter.UnsharpMask(1.25, 200, 0)), Bytes: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 58, 68, 78, 92,
                255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 19, 43, 53, 63, 75,
            ] },
        ]

        for item in cases {
            outputs.Push(item.Out)
            AhkTest.AssertEqual(item.Bytes, PillowTestBufferToArray(item.Out.ToBytes()))
        }
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        modeSource.Close()
        cmyk.Close()
    }
}

AhkTest.Test("Pillow ImageFilter supports CMYK through native path", PillowTestImageFilterCmykUsesNativePath)

PillowTestImageFilterUnsharpMaskRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.New("L", [1, 1])
    try {
        cases := [
            { Radius: [1, 2], Percent: 150, Threshold: 3 },
            { Radius: "x", Percent: 150, Threshold: 3 },
            { Radius: 1, Percent: 1.5, Threshold: 3 },
            { Radius: 1, Percent: 150, Threshold: 3.5 },
        ]
        for item in cases {
            try {
                source.Filter(Pillow.ImageFilter.UnsharpMask(item.Radius, item.Percent, item.Threshold))
                AhkTest.Fail("Expected UnsharpMask to reject invalid arguments")
            } catch Error as err {
                AhkTest.AssertTrue(
                    InStr(err.Message, "radius") > 0 ||
                    InStr(err.Message, "percent") > 0 ||
                    InStr(err.Message, "threshold") > 0
                )
            }
        }
    } finally {
        source.Close()
    }
}

AhkTest.Test("Pillow ImageFilter UnsharpMask rejects invalid arguments", PillowTestImageFilterUnsharpMaskRejectsInvalidArguments)

PillowTestImageTransformAffineUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    la := Pillow.Image.FromBytes("LA", [3, 2], PillowTestBuffer([
        10, 0, 80, 64, 200, 128,
        40, 255, 160, 128, 250, 32,
    ]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        1, 2, 3,
        10, 20, 30,
        40, 50, 60,
        70, 80, 90,
        100, 110, 120,
        130, 140, 150,
    ]))
    nearest := 0
    laBilinear := 0
    bicubic := 0
    try {
        nearest := source.TransformAffine([3, 2], [1.0, 0.0, -1.0, 0.0, 1.0, 0.0], Pillow.Resampling.NEAREST, 8)
        laBilinear := la.TransformAffine([3, 2], [1.0, 0.0, -0.25, 0.0, 1.0, 0.0], Pillow.Resampling.BILINEAR, [0, 0])
        bicubic := rgb.TransformAffine([4, 3], [0.75, 0.0, 0.0, 0.0, 0.75, 0.0], Pillow.Resampling.BICUBIC, [9, 0, 0])

        AhkTest.AssertEqual([8, 1, 2, 8, 4, 5], PillowTestBufferToArray(nearest.ToBytes()))
        AhkTest.AssertEqual([
            0, 0, 79, 48, 182, 112,
            40, 255, 112, 159, 195, 56,
        ], PillowTestBufferToArray(laBilinear.ToBytes()))
        AhkTest.AssertEqual([4, 3], bicubic.Size)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 6, 13, 25, 36, 34, 44, 54,
            42, 48, 54, 53, 62, 71, 80, 91, 101, 99, 109, 119,
            76, 88, 99, 96, 106, 117, 129, 139, 148, 146, 156, 166,
        ], PillowTestBufferToArray(bicubic.ToBytes()))
    } finally {
        for image in [bicubic, laBilinear, nearest, rgb, la, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.TransformAffine uses native affine path", PillowTestImageTransformAffineUsesNativePath)

PillowTestImageTransformAffinePythonLikeEntryPoint(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    out := 0
    try {
        out := source.Transform([3, 2], Pillow.Transform.AFFINE, [1.0, 0.0, -1.0, 0.0, 1.0, 0.0], Pillow.Resampling.NEAREST, 8)
        AhkTest.AssertEqual([8, 1, 2, 8, 4, 5], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        for image in [out, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Transform AFFINE uses Python-like entry point", PillowTestImageTransformAffinePythonLikeEntryPoint)

PillowTestImageTransformExtentUsesPythonLikeEntryPoint(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
    ]))
    nearest := 0
    bilinear := 0
    filled := 0
    try {
        nearest := source.Transform([2, 2], Pillow.Transform.EXTENT, [1.0, 0.0, 4.0, 3.0])
        bilinear := source.Transform([3, 2], Pillow.Transform.EXTENT, [0.5, 0.5, 3.5, 2.5], Pillow.Resampling.BILINEAR, 8)
        filled := source.Transform([4, 3], Pillow.Transform.EXTENT, [-1.0, -1.0, 3.0, 2.0], Pillow.Resampling.NEAREST, 99)

        AhkTest.AssertEqual([2, 4, 10, 12], PillowTestBufferToArray(nearest.ToBytes()))
        AhkTest.AssertEqual([3, 4, 5, 7, 8, 9], PillowTestBufferToArray(bilinear.ToBytes()))
        AhkTest.AssertEqual([
            99, 99, 99, 99,
            99, 1, 2, 3,
            99, 5, 6, 7,
        ], PillowTestBufferToArray(filled.ToBytes()))
    } finally {
        for image in [filled, bilinear, nearest, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Transform EXTENT uses Python-like entry point", PillowTestImageTransformExtentUsesPythonLikeEntryPoint)

PillowTestImageTransformPerspectiveUsesPythonLikeEntryPoint(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
    ]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        1, 2, 3,
        10, 20, 30,
        40, 50, 60,
        70, 80, 90,
        100, 110, 120,
        130, 140, 150,
    ]))
    nearest := 0
    bicubic := 0
    try {
        nearest := source.Transform([3, 2], Pillow.Transform.PERSPECTIVE, [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.1, 0.05], Pillow.Resampling.NEAREST, 9)
        bicubic := rgb.Transform([4, 3], Pillow.Transform.PERSPECTIVE, [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.1, 0.05], Pillow.Resampling.BICUBIC, [9, 0, 0])

        AhkTest.AssertEqual([1, 2, 2, 5, 6, 6], PillowTestBufferToArray(nearest.ToBytes()))
        AhkTest.AssertEqual([
            0, 0, 0, 0, 6, 14, 17, 28, 39, 32, 42, 52,
            57, 65, 74, 64, 73, 82, 81, 92, 102, 88, 98, 108,
            9, 0, 0, 97, 107, 117, 127, 137, 147, 138, 148, 157,
        ], PillowTestBufferToArray(bicubic.ToBytes()))
    } finally {
        for image in [bicubic, nearest, rgb, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Transform PERSPECTIVE uses Python-like entry point", PillowTestImageTransformPerspectiveUsesPythonLikeEntryPoint)

PillowTestImageTransformQuadUsesPythonLikeEntryPoint(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
    ]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        1, 2, 3,
        10, 20, 30,
        40, 50, 60,
        70, 80, 90,
        100, 110, 120,
        130, 140, 150,
    ]))
    nearest := 0
    bicubic := 0
    try {
        nearest := source.Transform([3, 2], Pillow.Transform.QUAD, [0.0, 0.0, 0.0, 3.0, 4.0, 3.0, 4.0, 0.0], Pillow.Resampling.NEAREST, 8)
        bicubic := rgb.Transform([4, 3], Pillow.Transform.QUAD, [0.0, 0.0, 0.25, 1.8, 2.75, 1.7, 2.5, 0.25], Pillow.Resampling.BICUBIC, [9, 0, 0])

        AhkTest.AssertEqual([1, 3, 4, 9, 11, 12], PillowTestBufferToArray(nearest.ToBytes()))
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 3, 7, 18, 29, 29, 40, 50,
            28, 32, 37, 36, 44, 52, 58, 68, 79, 76, 86, 97,
            70, 80, 90, 83, 93, 103, 110, 120, 130, 123, 133, 143,
        ], PillowTestBufferToArray(bicubic.ToBytes()))
    } finally {
        for image in [bicubic, nearest, rgb, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Transform QUAD uses Python-like entry point", PillowTestImageTransformQuadUsesPythonLikeEntryPoint)

PillowTestImageTransformMeshUsesPythonLikeEntryPoint(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
    ]))
    out := 0
    emptyOut := 0
    try {
        mesh := [
            [[0, 0, 3, 2], [0.0, 0.0, 0.0, 3.0, 4.0, 3.0, 4.0, 0.0]],
            [[3, 1, 5, 3], [0.0, 0.0, 0.0, 3.0, 4.0, 3.0, 4.0, 0.0]],
        ]
        out := source.Transform([5, 3], Pillow.Transform.MESH, mesh, Pillow.Resampling.BILINEAR, 99)
        emptyOut := source.Transform([3, 2], Pillow.Transform.MESH, [], Pillow.Resampling.NEAREST, 55)

        AhkTest.AssertEqual([
            2, 3, 4, 99, 99,
            8, 9, 10, 2, 4,
            99, 99, 99, 8, 10,
        ], PillowTestBufferToArray(out.ToBytes()))
        AhkTest.AssertEqual([55, 55, 55, 55, 55, 55], PillowTestBufferToArray(emptyOut.ToBytes()))
    } finally {
        for image in [emptyOut, out, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Transform MESH uses Python-like entry point", PillowTestImageTransformMeshUsesPythonLikeEntryPoint)

class PillowTestHalfShiftDeformer {
    getmesh(image) {
        AhkTest.AssertEqual([4, 3], image.Size)
        return [
            [[0, 0, 4, 3], [0.5, 0.0, 0.5, 3.0, 4.5, 3.0, 4.5, 0.0]],
        ]
    }
}

PillowTestImageOpsDeformUsesNativeMeshTransform(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
    ]))
    defaultOut := 0
    nearestOut := 0
    try {
        deformer := PillowTestHalfShiftDeformer()
        defaultOut := Pillow.ImageOps.Deform(source, deformer)
        nearestOut := Pillow.ImageOps.Deform(source, deformer, Pillow.Resampling.NEAREST)

        AhkTest.AssertEqual([4, 3], defaultOut.Size)
        AhkTest.AssertEqual([1, 2, 3, 0, 5, 6, 7, 0, 9, 10, 11, 0], PillowTestBufferToArray(defaultOut.ToBytes()))
        AhkTest.AssertEqual([2, 3, 4, 0, 6, 7, 8, 0, 10, 11, 12, 0], PillowTestBufferToArray(nearestOut.ToBytes()))
    } finally {
        for image in [nearestOut, defaultOut, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageOps.Deform uses native MESH transform", PillowTestImageOpsDeformUsesNativeMeshTransform)

PillowTestImageTransformRejectsUnsupportedMethods(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    try {
        try {
            source.Transform([3, 2], 99, [], Pillow.Resampling.NEAREST)
            AhkTest.Fail("Expected unknown transform method to throw")
        } catch as err {
            AhkTest.AssertTrue(InStr(err.Message, "unknown transformation method") > 0)
        }
    } finally {
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Transform rejects unsupported methods until native coverage exists", PillowTestImageTransformRejectsUnsupportedMethods)

PillowTestImageRotateNearestUsesNativeAffinePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        1, 2, 3,
        10, 20, 30,
        40, 50, 60,
        70, 80, 90,
        100, 110, 120,
        130, 140, 150,
    ]))
    out := 0
    expanded := 0
    translated := 0
    try {
        out := source.Rotate(45)
        expanded := rgb.Rotate(45, Pillow.Resampling.NEAREST, true, , , [9, 0, 0])
        translated := source.Rotate(30, Pillow.Resampling.NEAREST, false, , [1, -1], 8)

        AhkTest.AssertEqual([3, 2], out.Size)
        AhkTest.AssertEqual([0, 2, 6, 1, 5, 0], PillowTestBufferToArray(out.ToBytes()))
        AhkTest.AssertEqual([5, 4], expanded.Size)
        AhkTest.AssertEqual([
            9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0,
            9, 0, 0, 9, 0, 0, 10, 20, 30, 130, 140, 150, 9, 0, 0,
            9, 0, 0, 1, 2, 3, 100, 110, 120, 9, 0, 0, 9, 0, 0,
            9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0,
        ], PillowTestBufferToArray(expanded.ToBytes()))
        AhkTest.AssertEqual([8, 1, 5, 8, 8, 8], PillowTestBufferToArray(translated.ToBytes()))
    } finally {
        for image in [translated, expanded, out, rgb, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Rotate NEAREST uses native affine path", PillowTestImageRotateNearestUsesNativeAffinePath)

PillowTestImageRotateAndTransformFillColorMatchModeOneSemantics(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("1", [2, 1], PillowTestBuffer([0x80]))
    rotated := 0
    transformed := 0
    try {
        rotated := source.Rotate(45, Pillow.Resampling.NEAREST, true, , , 256)
        transformed := source.TransformAffine([4, 3], [1, 0, -1, 0, 1, -1], Pillow.Resampling.NEAREST, [256])

        AhkTest.AssertEqual("1", rotated.Mode)
        AhkTest.AssertEqual([4, 3], rotated.Size)
        AhkTest.AssertEqual([0xF0, 0xD0, 0xF0], PillowTestBufferToArray(rotated.ToBytes()))
        AhkTest.AssertEqual([0xF0, 0xD0, 0xF0], PillowTestBufferToArray(transformed.ToBytes()))
    } finally {
        for image in [transformed, rotated, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Rotate and Transform fillcolor match mode 1 semantics", PillowTestImageRotateAndTransformFillColorMatchModeOneSemantics)

PillowTestImageRotateBilinearUsesNativeAffinePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        1, 2, 3,
        10, 20, 30,
        40, 50, 60,
        70, 80, 90,
        100, 110, 120,
        130, 140, 150,
    ]))
    out := 0
    expanded := 0
    translated := 0
    try {
        out := source.Rotate(45, Pillow.Resampling.BILINEAR)
        expanded := rgb.Rotate(45, Pillow.Resampling.BILINEAR, true, , , [9, 0, 0])
        translated := source.Rotate(30, Pillow.Resampling.BILINEAR, false, , [1, -1], 8)

        AhkTest.AssertEqual([0, 2, 5, 1, 4, 0], PillowTestBufferToArray(out.ToBytes()))
        AhkTest.AssertEqual([5, 4], expanded.Size)
        AhkTest.AssertEqual([
            9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0,
            9, 0, 0, 9, 0, 0, 33, 43, 53, 116, 126, 136, 9, 0, 0,
            9, 0, 0, 11, 13, 15, 77, 86, 96, 9, 0, 0, 9, 0, 0,
            9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0,
        ], PillowTestBufferToArray(expanded.ToBytes()))
        AhkTest.AssertEqual([8, 2, 4, 8, 8, 8], PillowTestBufferToArray(translated.ToBytes()))
    } finally {
        for image in [translated, expanded, out, rgb, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Rotate BILINEAR uses native affine path", PillowTestImageRotateBilinearUsesNativeAffinePath)

PillowTestImageRotateBicubicUsesNativeAffinePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        1, 2, 3,
        10, 20, 30,
        40, 50, 60,
        70, 80, 90,
        100, 110, 120,
        130, 140, 150,
    ]))
    out := 0
    expanded := 0
    translated := 0
    try {
        out := source.Rotate(45, Pillow.Resampling.BICUBIC)
        expanded := rgb.Rotate(45, Pillow.Resampling.BICUBIC, true, , , [9, 0, 0])
        translated := source.Rotate(30, Pillow.Resampling.BICUBIC, false, , [1, -1], 8)

        AhkTest.AssertEqual([0, 2, 5, 1, 4, 0], PillowTestBufferToArray(out.ToBytes()))
        AhkTest.AssertEqual([5, 4], expanded.Size)
        AhkTest.AssertEqual([
            9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0,
            9, 0, 0, 9, 0, 0, 35, 46, 57, 118, 128, 138, 9, 0, 0,
            9, 0, 0, 10, 12, 14, 72, 82, 91, 9, 0, 0, 9, 0, 0,
            9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0,
        ], PillowTestBufferToArray(expanded.ToBytes()))
        AhkTest.AssertEqual([8, 2, 4, 8, 8, 8], PillowTestBufferToArray(translated.ToBytes()))
    } finally {
        for image in [translated, expanded, out, rgb, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Rotate BICUBIC uses native affine path", PillowTestImageRotateBicubicUsesNativeAffinePath)

PillowTestImageGeometryCmykUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("CMYK", [3, 2], PillowTestBuffer([
        1, 2, 3, 4, 10, 20, 30, 40, 40, 50, 60, 70,
        70, 80, 90, 100, 100, 110, 120, 130, 130, 140, 150, 160,
    ]))
    resized := 0
    lanczos := 0
    affine := 0
    rotated := 0
    try {
        resized := source.Resize([4, 3], Pillow.Resampling.BICUBIC)
        lanczos := source.Resize([2, 4], Pillow.Resampling.LANCZOS)
        affine := source.TransformAffine([4, 3], [0.75, 0.0, 0.0, 0.0, 0.75, 0.0], Pillow.Resampling.BICUBIC, [9, 8, 7, 6])
        rotated := source.Rotate(45, Pillow.Resampling.BILINEAR, true, , , [9, 8, 7, 6])

        AhkTest.AssertEqual("CMYK", resized.Mode)
        AhkTest.AssertEqual([4, 3], resized.Size)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 5, 12, 18, 16, 26, 37, 48, 36, 46, 56, 66,
            35, 40, 45, 50, 46, 54, 63, 71, 68, 78, 88, 99, 87, 97, 107, 117,
            72, 83, 94, 104, 92, 103, 113, 123, 119, 129, 139, 149, 138, 148, 158, 168,
        ], PillowTestBufferToArray(resized.ToBytes()))
        AhkTest.AssertEqual([2, 4], lanczos.Size)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 14, 25, 36, 46,
            20, 25, 30, 36, 51, 62, 73, 83,
            60, 69, 78, 86, 101, 111, 121, 131,
            91, 102, 113, 124, 138, 148, 158, 168,
        ], PillowTestBufferToArray(lanczos.ToBytes()))
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 6, 13, 13, 25, 36, 47, 34, 44, 54, 64,
            42, 48, 54, 60, 53, 62, 71, 79, 80, 91, 101, 112, 99, 109, 119, 129,
            76, 88, 99, 111, 96, 106, 117, 127, 129, 139, 148, 158, 146, 156, 166, 176,
        ], PillowTestBufferToArray(affine.ToBytes()))
        AhkTest.AssertEqual([5, 4], rotated.Size)
        AhkTest.AssertEqual([
            9, 8, 7, 6, 9, 8, 7, 6, 9, 8, 7, 6, 9, 8, 7, 6, 9, 8, 7, 6,
            9, 8, 7, 6, 9, 8, 7, 6, 33, 43, 53, 63, 116, 126, 136, 146, 9, 8, 7, 6,
            9, 8, 7, 6, 11, 13, 15, 18, 77, 86, 96, 105, 9, 8, 7, 6, 9, 8, 7, 6,
            9, 8, 7, 6, 9, 8, 7, 6, 9, 8, 7, 6, 9, 8, 7, 6, 9, 8, 7, 6,
        ], PillowTestBufferToArray(rotated.ToBytes()))
    } finally {
        for image in [rotated, affine, lanczos, resized, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow geometry paths support CMYK through native handles", PillowTestImageGeometryCmykUsesNativePath)

PillowTestImageOpsScaleUsesNativeResizeAndPythonRounding(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [5, 3], PillowTestBuffer([
        0, 1, 2, 3, 4,
        5, 6, 7, 8, 9,
        10, 11, 12, 13, 14,
    ]))
    nearest := 0
    bicubic := 0
    try {
        nearest := Pillow.ImageOps.Scale(source, 0.5, Pillow.Resampling.NEAREST)
        bicubic := Pillow.ImageOps.Scale(source, 0.5)

        AhkTest.AssertEqual([2, 2], nearest.Size)
        AhkTest.AssertEqual([1, 3, 11, 13], PillowTestBufferToArray(nearest.ToBytes()))
        AhkTest.AssertEqual([2, 2], bicubic.Size)
        AhkTest.AssertEqual([3, 5, 9, 11], PillowTestBufferToArray(bicubic.ToBytes()))
    } finally {
        if IsObject(bicubic)
            bicubic.Close()
        if IsObject(nearest)
            nearest.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Scale uses native resize and Python rounding", PillowTestImageOpsScaleUsesNativeResizeAndPythonRounding)

PillowTestImageOpsScaleFactorOneReturnsIndependentCopy(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([0, 1, 2, 3, 4, 5]))
    scaled := 0
    try {
        scaled := Pillow.ImageOps.Scale(source, 1, Pillow.Resampling.NEAREST)
        source.Fill(9)

        AhkTest.AssertEqual([3, 2], scaled.Size)
        AhkTest.AssertEqual([0, 1, 2, 3, 4, 5], PillowTestBufferToArray(scaled.ToBytes()))
    } finally {
        if IsObject(scaled)
            scaled.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Scale factor one returns an independent copy", PillowTestImageOpsScaleFactorOneReturnsIndependentCopy)

PillowTestImageOpsScaleRejectsInvalidFactor(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([0, 1, 2, 3, 4, 5]))
    try {
        try {
            Pillow.ImageOps.Scale(image, 0)
            AhkTest.Fail("Expected ImageOps.Scale to reject zero factor")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "factor") > 0)
        }

        try {
            Pillow.ImageOps.Scale(image, "2")
            AhkTest.Fail("Expected ImageOps.Scale to reject non-numeric factor")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "factor") > 0)
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Scale rejects invalid factors", PillowTestImageOpsScaleRejectsInvalidFactor)

PillowTestImageOpsContainAndCoverUseNativeResizeGeometry(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 2], PillowTestBuffer([0, 40, 80, 120, 160, 200, 220, 255]))
    containTall := 0
    coverSquare := 0
    try {
        containTall := Pillow.ImageOps.Contain(source, [4, 4], Pillow.Resampling.NEAREST)
        coverSquare := Pillow.ImageOps.Cover(source, [4, 4], Pillow.Resampling.NEAREST)

        AhkTest.AssertEqual([4, 2], containTall.Size)
        AhkTest.AssertEqual([0, 40, 80, 120, 160, 200, 220, 255], PillowTestBufferToArray(containTall.ToBytes()))
        AhkTest.AssertEqual([8, 4], coverSquare.Size)
        AhkTest.AssertEqual([
            0, 0, 40, 40, 80, 80, 120, 120,
            0, 0, 40, 40, 80, 80, 120, 120,
            160, 160, 200, 200, 220, 220, 255, 255,
            160, 160, 200, 200, 220, 220, 255, 255,
        ], PillowTestBufferToArray(coverSquare.ToBytes()))
    } finally {
        if IsObject(coverSquare)
            coverSquare.Close()
        if IsObject(containTall)
            containTall.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Contain and Cover use native proportional resize geometry", PillowTestImageOpsContainAndCoverUseNativeResizeGeometry)

PillowTestImageOpsContainAndCoverRejectInvalidSize(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [4, 2], PillowTestBuffer([0, 40, 80, 120, 160, 200, 220, 255]))
    try {
        try {
            Pillow.ImageOps.Contain(image, [1, 1], Pillow.Resampling.NEAREST)
            AhkTest.Fail("Expected Contain to reject zero computed height")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }

        try {
            Pillow.ImageOps.Cover(image, [4, 0], Pillow.Resampling.NEAREST)
            AhkTest.Fail("Expected Cover to reject zero requested height")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Contain and Cover reject invalid requested sizes", PillowTestImageOpsContainAndCoverRejectInvalidSize)

PillowTestImageOpsPadUsesNativeResizeAndFill(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 2], PillowTestBuffer([0, 40, 80, 120, 160, 200, 220, 255]))
    padded := 0
    try {
        padded := Pillow.ImageOps.Pad(source, [4, 4], Pillow.Resampling.NEAREST)

        AhkTest.AssertEqual("L", padded.Mode)
        AhkTest.AssertEqual([4, 4], padded.Size)
        AhkTest.AssertEqual([
            0, 0, 0, 0,
            0, 40, 80, 120,
            160, 200, 220, 255,
            0, 0, 0, 0,
        ], PillowTestBufferToArray(padded.ToBytes()))
    } finally {
        if IsObject(padded)
            padded.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Pad uses native resize and fill", PillowTestImageOpsPadUsesNativeResizeAndFill)

PillowTestImageOpsPadParsesColorAndCenteringLikePillow(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    rgba := Pillow.Image.FromBytes("RGBA", [1, 1], PillowTestBuffer([1, 2, 3, 4]))
    scalarRgb := 0
    tupleRgba := 0
    try {
        scalarRgb := Pillow.ImageOps.Pad(rgb, [4, 4], Pillow.Resampling.NEAREST, 7, [0.5, 0.5])
        tupleRgba := Pillow.ImageOps.Pad(rgba, [3, 5], Pillow.Resampling.NEAREST, [7, 8, 9], [-1, 99])

        AhkTest.AssertEqual([
            7, 0, 0, 7, 0, 0, 7, 0, 0, 7, 0, 0,
            1, 2, 3, 1, 2, 3, 4, 5, 6, 4, 5, 6,
            1, 2, 3, 1, 2, 3, 4, 5, 6, 4, 5, 6,
            7, 0, 0, 7, 0, 0, 7, 0, 0, 7, 0, 0,
        ], PillowTestBufferToArray(scalarRgb.ToBytes()))

        AhkTest.AssertEqual([
            7, 8, 9, 255, 7, 8, 9, 255, 7, 8, 9, 255,
            7, 8, 9, 255, 7, 8, 9, 255, 7, 8, 9, 255,
            1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4,
            1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4,
            1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4,
        ], PillowTestBufferToArray(tupleRgba.ToBytes()))
    } finally {
        if IsObject(tupleRgba)
            tupleRgba.Close()
        if IsObject(scalarRgb)
            scalarRgb.Close()
        rgba.Close()
        rgb.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Pad parses color and centering like Pillow", PillowTestImageOpsPadParsesColorAndCenteringLikePillow)

PillowTestImageOpsPadSupportsLaFillColors(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("LA", [2, 2], PillowTestBuffer([10, 0, 80, 64, 160, 128, 250, 255]))
    scalar := 0
    tuple1 := 0
    tuple2 := 0
    try {
        scalar := Pillow.ImageOps.Pad(source, [4, 2], Pillow.Resampling.NEAREST, 9, [0.5, 0.5])
        tuple1 := Pillow.ImageOps.Pad(source, [4, 2], Pillow.Resampling.NEAREST, [9], [0.5, 0.5])
        tuple2 := Pillow.ImageOps.Pad(source, [4, 2], Pillow.Resampling.NEAREST, [9, 17], [0.5, 0.5])

        AhkTest.AssertEqual("LA", tuple2.Mode)
        AhkTest.AssertEqual([4, 2], tuple2.Size)
        AhkTest.AssertEqual([
            9, 0, 10, 0, 80, 64, 9, 0,
            9, 0, 160, 128, 250, 255, 9, 0,
        ], PillowTestBufferToArray(scalar.ToBytes()))
        AhkTest.AssertEqual(PillowTestBufferToArray(scalar.ToBytes()), PillowTestBufferToArray(tuple1.ToBytes()))
        AhkTest.AssertEqual([
            9, 17, 10, 0, 80, 64, 9, 17,
            9, 17, 160, 128, 250, 255, 9, 17,
        ], PillowTestBufferToArray(tuple2.ToBytes()))

        try {
            Pillow.ImageOps.Pad(source, [4, 2], Pillow.Resampling.NEAREST, [9, 17, 99], [0.5, 0.5])
            AhkTest.Fail("Expected LA pad color to reject a three-item tuple")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "fill") > 0)
        }
    } finally {
        for image in [tuple2, tuple1, scalar] {
            if IsObject(image)
                image.Close()
        }
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Pad supports LA fill colors", PillowTestImageOpsPadSupportsLaFillColors)

PillowTestImageOpsPadFillColorMatchesModeOneSemantics(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("1", [2, 1], PillowTestBuffer([0x80]))
    padded := 0
    try {
        padded := Pillow.ImageOps.Pad(source, [4, 3], Pillow.Resampling.NEAREST, 256)

        AhkTest.AssertEqual("1", padded.Mode)
        AhkTest.AssertEqual([4, 3], padded.Size)
        AhkTest.AssertEqual([0xC0, 0xC0, 0xF0], PillowTestBufferToArray(padded.ToBytes()))
    } finally {
        if IsObject(padded)
            padded.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Pad fill color matches mode 1 semantics", PillowTestImageOpsPadFillColorMatchesModeOneSemantics)

PillowTestImageOpsPadRejectsInvalidParameters(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [4, 2], PillowTestBuffer([0, 40, 80, 120, 160, 200, 220, 255]))
    try {
        try {
            Pillow.ImageOps.Pad(image, [1, 1], Pillow.Resampling.NEAREST)
            AhkTest.Fail("Expected Pad to reject zero computed height")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }

        try {
            Pillow.ImageOps.Pad(image, [4, 4], Pillow.Resampling.NEAREST, 0, [0.5])
            AhkTest.Fail("Expected Pad to reject malformed centering")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "centering") > 0)
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Pad rejects invalid parameters", PillowTestImageOpsPadRejectsInvalidParameters)

PillowTestImageOpsFitUsesNativeCropResize(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 2], PillowTestBuffer([0, 40, 80, 120, 160, 200, 220, 255]))
    fit := 0
    try {
        fit := Pillow.ImageOps.Fit(source, [2, 2], Pillow.Resampling.NEAREST)

        AhkTest.AssertEqual("L", fit.Mode)
        AhkTest.AssertEqual([2, 2], fit.Size)
        AhkTest.AssertEqual([40, 80, 200, 220], PillowTestBufferToArray(fit.ToBytes()))
    } finally {
        if IsObject(fit)
            fit.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Fit uses native crop resize", PillowTestImageOpsFitUsesNativeCropResize)

PillowTestImageOpsFitParsesBleedAndCenteringLikePillow(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 2], PillowTestBuffer([0, 40, 80, 120, 160, 200, 220, 255]))
    fallback := 0
    bleed := 0
    try {
        fallback := Pillow.ImageOps.Fit(source, [2, 2], Pillow.Resampling.NEAREST, -0.1, [-1, 99])
        bleed := Pillow.ImageOps.Fit(source, [2, 1], Pillow.Resampling.NEAREST, 0.25, [0.5, 0.5])

        AhkTest.AssertEqual([40, 80, 200, 220], PillowTestBufferToArray(fallback.ToBytes()))
        AhkTest.AssertEqual([200, 220], PillowTestBufferToArray(bleed.ToBytes()))
    } finally {
        if IsObject(bleed)
            bleed.Close()
        if IsObject(fallback)
            fallback.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Fit parses bleed and centering like Pillow", PillowTestImageOpsFitParsesBleedAndCenteringLikePillow)

PillowTestImageOpsFitRejectsInvalidParameters(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [4, 2], PillowTestBuffer([0, 40, 80, 120, 160, 200, 220, 255]))
    try {
        try {
            Pillow.ImageOps.Fit(image, [0, 1], Pillow.Resampling.NEAREST)
            AhkTest.Fail("Expected Fit to reject invalid output size")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }

        try {
            Pillow.ImageOps.Fit(image, [2, 2], Pillow.Resampling.NEAREST, 0.0, [0.5])
            AhkTest.Fail("Expected Fit to reject malformed centering")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "centering") > 0)
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Fit rejects invalid parameters", PillowTestImageOpsFitRejectsInvalidParameters)

PillowTestImageTransposeUsesPillowConstants(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        10, 20, 30, 40, 50, 60, 70, 80, 90,
        100, 110, 120, 130, 140, 150, 160, 170, 180,
    ]))
    transposed := 0
    try {
        transposed := source.Transpose(Pillow.Transpose.ROTATE_90)
        AhkTest.AssertEqual("RGB", transposed.Mode)
        AhkTest.AssertEqual([2, 3], transposed.Size)
        AhkTest.AssertEqual([
            70, 80, 90, 160, 170, 180,
            40, 50, 60, 130, 140, 150,
            10, 20, 30, 100, 110, 120,
        ], PillowTestBufferToArray(transposed.ToBytes()))
    } finally {
        if IsObject(transposed)
            transposed.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Transpose accepts Pillow transpose constants", PillowTestImageTransposeUsesPillowConstants)

PillowTestImageOpsMirrorAndFlipUseNativeTranspose(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    mirrored := 0
    flipped := 0
    try {
        mirrored := Pillow.ImageOps.Mirror(source)
        flipped := Pillow.ImageOps.Flip(source)

        AhkTest.AssertEqual("L", mirrored.Mode)
        AhkTest.AssertEqual([3, 2], mirrored.Size)
        AhkTest.AssertEqual([3, 2, 1, 6, 5, 4], PillowTestBufferToArray(mirrored.ToBytes()))
        AhkTest.AssertEqual("L", flipped.Mode)
        AhkTest.AssertEqual([3, 2], flipped.Size)
        AhkTest.AssertEqual([4, 5, 6, 1, 2, 3], PillowTestBufferToArray(flipped.ToBytes()))
    } finally {
        for item in [flipped, mirrored, source] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageOps.Mirror and Flip reuse native transpose", PillowTestImageOpsMirrorAndFlipUseNativeTranspose)

PillowTestImageOpsMirrorAndFlipSupportRgba(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGBA", [2, 2], PillowTestBuffer([
        1, 2, 3, 4, 5, 6, 7, 8,
        9, 10, 11, 12, 13, 14, 15, 16,
    ]))
    mirrored := 0
    flipped := 0
    try {
        mirrored := Pillow.ImageOps.Mirror(source)
        flipped := Pillow.ImageOps.Flip(source)

        AhkTest.AssertEqual([5, 6, 7, 8, 1, 2, 3, 4, 13, 14, 15, 16, 9, 10, 11, 12], PillowTestBufferToArray(mirrored.ToBytes()))
        AhkTest.AssertEqual([9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8], PillowTestBufferToArray(flipped.ToBytes()))
    } finally {
        for item in [flipped, mirrored, source] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageOps.Mirror and Flip support RGBA", PillowTestImageOpsMirrorAndFlipSupportRgba)

PillowTestImageConvertRgbToLUsesNativeHandleOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGB", [4, 1], PillowTestBuffer([
        0, 0, 0,
        255, 255, 255,
        255, 0, 0,
        0, 255, 0,
    ]))
    converted := 0
    try {
        converted := source.Convert("L")
        AhkTest.AssertEqual("L", converted.Mode)
        AhkTest.AssertEqual([4, 1], converted.Size)
        AhkTest.AssertEqual([0, 255, 76, 150], PillowTestBufferToArray(converted.ToBytes()))
    } finally {
        if IsObject(converted)
            converted.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Convert converts RGB to L through native handles", PillowTestImageConvertRgbToLUsesNativeHandleOperation)

PillowTestImageConvertCoreModesUseNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [3, 1], PillowTestBuffer([0, 10, 255]))
    rgba := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([10, 20, 30, 40, 200, 100, 50, 128]))
    lToRgba := 0
    rgbaToRgb := 0
    try {
        lToRgba := l.Convert("RGBA")
        rgbaToRgb := rgba.Convert("RGB")
        AhkTest.AssertEqual("RGBA", lToRgba.Mode)
        AhkTest.AssertEqual([0, 0, 0, 255, 10, 10, 10, 255, 255, 255, 255, 255], PillowTestBufferToArray(lToRgba.ToBytes()))
        AhkTest.AssertEqual("RGB", rgbaToRgb.Mode)
        AhkTest.AssertEqual([10, 20, 30, 200, 100, 50], PillowTestBufferToArray(rgbaToRgb.ToBytes()))
    } finally {
        if IsObject(rgbaToRgb)
            rgbaToRgb.Close()
        if IsObject(lToRgba)
            lToRgba.Close()
        rgba.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.Convert covers core L RGB RGBA modes through native handles", PillowTestImageConvertCoreModesUseNativeOperation)

PillowTestImageConvertLaModeUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([10, 80, 130, 200, 20, 250]))
    la := Pillow.Image.FromBytes("LA", [3, 2], PillowTestBuffer([10, 40, 80, 70, 130, 100, 200, 130, 20, 160, 250, 190]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        10, 20, 30, 80, 40, 10, 130, 140, 150,
        200, 190, 180, 20, 120, 220, 250, 240, 10,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [3, 2], PillowTestBuffer([
        10, 20, 30, 40, 80, 40, 10, 70, 130, 140, 150, 100,
        200, 190, 180, 130, 20, 120, 220, 160, 250, 240, 10, 190,
    ]))
    outputs := []
    try {
        cases := [
            { Out: la.Convert("L"), Mode: "L", Bytes: [10, 80, 130, 200, 20, 250] },
            { Out: la.Convert("RGB"), Mode: "RGB", Bytes: [
                10, 10, 10, 80, 80, 80, 130, 130, 130,
                200, 200, 200, 20, 20, 20, 250, 250, 250,
            ] },
            { Out: la.Convert("RGBA"), Mode: "RGBA", Bytes: [
                10, 10, 10, 40, 80, 80, 80, 70, 130, 130, 130, 100,
                200, 200, 200, 130, 20, 20, 20, 160, 250, 250, 250, 190,
            ] },
            { Out: l.Convert("LA"), Mode: "LA", Bytes: [10, 255, 80, 255, 130, 255, 200, 255, 20, 255, 250, 255] },
            { Out: rgb.Convert("LA"), Mode: "LA", Bytes: [18, 255, 49, 255, 138, 255, 192, 255, 102, 255, 217, 255] },
            { Out: rgba.Convert("LA"), Mode: "LA", Bytes: [18, 40, 49, 70, 138, 100, 192, 130, 102, 160, 217, 190] },
        ]
        for item in cases {
            outputs.Push(item.Out)
            AhkTest.AssertEqual(item.Mode, item.Out.Mode)
            AhkTest.AssertEqual(item.Bytes, PillowTestBufferToArray(item.Out.ToBytes()))
        }
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        rgba.Close()
        rgb.Close()
        la.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.Convert supports LA mode through native handles", PillowTestImageConvertLaModeUsesNativeOperation)

PillowTestImageConvertModeOneToCoreModesUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("1", [4, 1], PillowTestBuffer([0x50]))
    outputs := []
    try {
        cases := [
            { Out: source.Convert("L"), Mode: "L", Bytes: [0, 255, 0, 255] },
            { Out: source.Convert("LA"), Mode: "LA", Bytes: [0, 255, 255, 255, 0, 255, 255, 255] },
            { Out: source.Convert("RGB"), Mode: "RGB", Bytes: [
                0, 0, 0, 255, 255, 255, 0, 0, 0, 255, 255, 255,
            ] },
            { Out: source.Convert("RGBA"), Mode: "RGBA", Bytes: [
                0, 0, 0, 255, 255, 255, 255, 255,
                0, 0, 0, 255, 255, 255, 255, 255,
            ] },
        ]
        for item in cases {
            outputs.Push(item.Out)
            AhkTest.AssertEqual(item.Mode, item.Out.Mode)
            AhkTest.AssertEqual(item.Bytes, PillowTestBufferToArray(item.Out.ToBytes()))
        }
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Convert supports mode 1 to core modes", PillowTestImageConvertModeOneToCoreModesUsesNativeOperation)

PillowTestImageConvertCmykUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    cmyk := Pillow.Image.FromBytes("CMYK", [8, 1], PillowTestBuffer([
        0, 0, 0, 0,
        0, 0, 0, 255,
        255, 0, 0, 0,
        0, 255, 0, 0,
        0, 0, 255, 0,
        10, 20, 30, 40,
        200, 100, 50, 128,
        255, 255, 255, 255,
    ]))
    rgb := Pillow.Image.FromBytes("RGB", [4, 1], PillowTestBuffer([0, 0, 0, 255, 255, 255, 10, 20, 30, 200, 100, 50]))
    rgba := Pillow.Image.FromBytes("RGBA", [4, 1], PillowTestBuffer([0, 0, 0, 0, 255, 255, 255, 128, 10, 20, 30, 255, 200, 100, 50, 64]))
    l := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 10, 128, 255]))
    la := Pillow.Image.FromBytes("LA", [4, 1], PillowTestBuffer([0, 0, 10, 64, 128, 128, 255, 255]))
    one := Pillow.Image.FromBytes("1", [4, 1], PillowTestBuffer([0x50]))
    outputs := []
    try {
        cases := [
            { Out: cmyk.Convert("RGB"), Mode: "RGB", Bytes: [
                255, 255, 255, 0, 0, 0, 0, 255, 255, 255, 0, 255,
                255, 255, 0, 207, 198, 190, 27, 77, 102, 0, 0, 0,
            ] },
            { Out: cmyk.Convert("RGBA"), Mode: "RGBA", Bytes: [
                255, 255, 255, 255, 0, 0, 0, 255, 0, 255, 255, 255, 255, 0, 255, 255,
                255, 255, 0, 255, 207, 198, 190, 255, 27, 77, 102, 255, 0, 0, 0, 255,
            ] },
            { Out: cmyk.Convert("L"), Mode: "L", Bytes: [255, 0, 179, 105, 226, 200, 65, 0] },
            { Out: cmyk.Convert("LA"), Mode: "LA", Bytes: [255, 255, 0, 255, 179, 255, 105, 255, 226, 255, 200, 255, 65, 255, 0, 255] },
            { Out: rgb.Convert("CMYK"), Mode: "CMYK", Bytes: [255, 255, 255, 0, 0, 0, 0, 0, 245, 235, 225, 0, 55, 155, 205, 0] },
            { Out: rgba.Convert("CMYK"), Mode: "CMYK", Bytes: [255, 255, 255, 0, 0, 0, 0, 0, 245, 235, 225, 0, 55, 155, 205, 0] },
            { Out: l.Convert("CMYK"), Mode: "CMYK", Bytes: [0, 0, 0, 255, 0, 0, 0, 245, 0, 0, 0, 127, 0, 0, 0, 0] },
            { Out: la.Convert("CMYK"), Mode: "CMYK", Bytes: [0, 0, 0, 255, 0, 0, 0, 245, 0, 0, 0, 127, 0, 0, 0, 0] },
            { Out: one.Convert("CMYK"), Mode: "CMYK", Bytes: [0, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 255, 0, 0, 0, 0] },
        ]
        for item in cases {
            outputs.Push(item.Out)
            AhkTest.AssertEqual(item.Mode, item.Out.Mode)
            AhkTest.AssertEqual(item.Bytes, PillowTestBufferToArray(item.Out.ToBytes()))
        }
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        one.Close()
        la.Close()
        l.Close()
        rgba.Close()
        rgb.Close()
        cmyk.Close()
    }
}

AhkTest.Test("Pillow Image.Convert supports CMYK through native handles", PillowTestImageConvertCmykUsesNativeOperation)

PillowTestImageConvertMatrixUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        0, 0, 0, 10, 20, 30, 100, 110, 120,
        200, 150, 100, 255, 128, 64, 5, 250, 125,
    ]))
    outputs := []
    try {
        l := source.Convert("L", [0.299, 0.587, 0.114, 0])
        lBias := source.Convert("L", [0.5, 0.25, 0.125, 10])
        rgb := source.Convert("RGB", [
            1, 0, 0, 5,
            0, 0.5, 0, 10,
            0, 0, -1, 255,
        ])
        outputs.Push(l)
        outputs.Push(lBias)
        outputs.Push(rgb)

        AhkTest.AssertEqual("L", l.Mode)
        AhkTest.AssertEqual([0, 18, 108, 159, 159, 162], PillowTestBufferToArray(l.ToBytes()))
        AhkTest.AssertEqual([10, 24, 103, 160, 178, 91], PillowTestBufferToArray(lBias.ToBytes()))
        AhkTest.AssertEqual("RGB", rgb.Mode)
        AhkTest.AssertEqual([5, 10, 255, 15, 20, 225, 105, 65, 135, 205, 85, 155, 255, 74, 191, 10, 135, 130], PillowTestBufferToArray(rgb.ToBytes()))
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Convert matrix uses native operation", PillowTestImageConvertMatrixUsesNativeOperation)

PillowTestImageConvertMatrixRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.New("RGB", [1, 1])
    rgba := Pillow.Image.New("RGBA", [1, 1])
    l := Pillow.Image.New("L", [1, 1])
    try {
        cases := [
            (*) => rgba.Convert("L", [0.299, 0.587, 0.114, 0]),
            (*) => l.Convert("L", [0.299, 0.587, 0.114, 0]),
            (*) => rgb.Convert("RGBA", [0.299, 0.587, 0.114, 0]),
            (*) => rgb.Convert("L", [1, 2, 3]),
            (*) => rgb.Convert("RGB", [1, 2, 3, 4]),
        ]
        for action in cases {
            try {
                action.Call()
                AhkTest.Fail("Expected Convert matrix to reject invalid arguments")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, "matrix") > 0 || InStr(err.Message, "illegal conversion") > 0 || InStr(err.Message, "wrong mode") > 0 || InStr(err.Message, "invalid argument") > 0)
            }
        }
    } finally {
        l.Close()
        rgba.Close()
        rgb.Close()
    }
}

AhkTest.Test("Pillow Image.Convert matrix rejects invalid arguments", PillowTestImageConvertMatrixRejectsInvalidArguments)

PillowTestImageConvertToModeOneDitherNoneUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [8, 1], PillowTestBuffer([0, 32, 64, 96, 128, 160, 192, 255]))
    rgb := Pillow.Image.FromBytes("RGB", [4, 2], PillowTestBuffer([
        0, 0, 0, 32, 32, 32, 64, 64, 64, 96, 96, 96,
        128, 128, 128, 160, 160, 160, 192, 192, 192, 224, 224, 224,
    ]))
    rgbThreshold := Pillow.Image.FromBytes("RGB", [4, 2], PillowTestBuffer([
        128, 128, 127, 127, 128, 128, 128, 127, 128, 129, 127, 127,
        127, 129, 127, 127, 127, 129, 255, 255, 255, 0, 0, 0,
    ]))
    la := Pillow.Image.FromBytes("LA", [4, 1], PillowTestBuffer([0, 0, 255, 128, 127, 255, 128, 255]))
    rgbaThreshold := Pillow.Image.FromBytes("RGBA", [4, 2], PillowTestBuffer([
        128, 128, 127, 0, 127, 128, 128, 255, 128, 127, 128, 1, 129, 127, 127, 128,
        127, 129, 127, 255, 127, 127, 129, 0, 255, 255, 255, 64, 0, 0, 0, 255,
    ]))
    cmyk := Pillow.Image.FromBytes("CMYK", [8, 2], PillowTestBuffer([
        0, 0, 0, 255, 0, 0, 0, 224, 0, 0, 0, 192, 0, 0, 0, 160,
        0, 0, 0, 128, 0, 0, 0, 96, 0, 0, 0, 64, 0, 0, 0, 32,
        0, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 80, 0, 0, 0, 120,
        0, 0, 0, 160, 0, 0, 0, 200, 0, 0, 0, 240, 0, 0, 0, 255,
    ]))
    outputs := []
    try {
        cases := [
            { Out: l.Convert("1", Pillow.Dither.NONE), Bytes: [0x0F] },
            { Out: rgb.Convert("1", Pillow.Dither.NONE), Bytes: [0x00, 0xF0] },
            { Out: rgbThreshold.Convert("1", Pillow.Dither.NONE), Bytes: [0x00, 0xA0] },
            { Out: la.Convert("1", Pillow.Dither.NONE), Bytes: [0x50] },
            { Out: rgbaThreshold.Convert("1", Pillow.Dither.NONE), Bytes: [0x00, 0xA0] },
            { Out: cmyk.Convert("1", Pillow.Dither.NONE), Bytes: [0x07, 0xF0] },
        ]
        for item in cases {
            outputs.Push(item.Out)
            AhkTest.AssertEqual("1", item.Out.Mode)
            AhkTest.AssertEqual(item.Bytes, PillowTestBufferToArray(item.Out.ToBytes()))
        }

        try {
            l.Convert("1")
            AhkTest.Fail("Expected default Convert('1') to reject until Floyd-Steinberg is implemented")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "dither") > 0 || InStr(err.Message, "Floyd") > 0)
        }
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        cmyk.Close()
        rgbaThreshold.Close()
        la.Close()
        rgbThreshold.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.Convert to mode 1 supports dither NONE", PillowTestImageConvertToModeOneDitherNoneUsesNativeOperation)

PillowTestImageConvertToModeOneFloydSteinbergUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [8, 1], PillowTestBuffer([0, 32, 64, 96, 128, 160, 192, 255]))
    rgb := Pillow.Image.FromBytes("RGB", [4, 2], PillowTestBuffer([
        128, 128, 127, 127, 128, 128, 128, 127, 128, 129, 127, 127,
        127, 129, 127, 127, 127, 129, 255, 255, 255, 0, 0, 0,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [4, 2], PillowTestBuffer([
        0, 0, 0, 0, 80, 80, 80, 255, 140, 140, 140, 1, 255, 255, 255, 128,
        40, 40, 40, 255, 120, 120, 120, 0, 180, 180, 180, 255, 220, 220, 220, 7,
    ]))
    cmyk := Pillow.Image.FromBytes("CMYK", [8, 2], PillowTestBuffer([
        0, 0, 0, 255, 0, 0, 0, 224, 0, 0, 0, 192, 0, 0, 0, 160,
        0, 0, 0, 128, 0, 0, 0, 96, 0, 0, 0, 64, 0, 0, 0, 32,
        0, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 80, 0, 0, 0, 120,
        0, 0, 0, 160, 0, 0, 0, 200, 0, 0, 0, 240, 0, 0, 0, 255,
    ]))
    outputs := []
    try {
        cases := [
            { Out: l.Convert("1"), Bytes: [0x17] },
            { Out: l.Convert("1", Pillow.Dither.FLOYDSTEINBERG), Bytes: [0x17] },
            { Out: rgb.Convert("1"), Bytes: [0x50, 0xA0] },
            { Out: rgba.Convert("1"), Bytes: [0x30, 0x50] },
            { Out: rgba.Convert("1", Pillow.Dither.FLOYDSTEINBERG), Bytes: [0x30, 0x50] },
            { Out: cmyk.Convert("1"), Bytes: [0x0B, 0xF0] },
            { Out: cmyk.Convert("1", Pillow.Dither.FLOYDSTEINBERG), Bytes: [0x0B, 0xF0] },
        ]
        for item in cases {
            outputs.Push(item.Out)
            AhkTest.AssertEqual("1", item.Out.Mode)
            AhkTest.AssertEqual(item.Bytes, PillowTestBufferToArray(item.Out.ToBytes()))
        }
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        cmyk.Close()
        rgba.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.Convert to mode 1 supports Floyd-Steinberg dither", PillowTestImageConvertToModeOneFloydSteinbergUsesNativeOperation)

PillowTestMakeInvertLut(channelCount) {
    lut := []
    loop channelCount {
        loop 256
            lut.Push(256 - A_Index)
    }
    return lut
}

PillowTestMakeIdentityLut(channelCount) {
    lut := []
    loop channelCount {
        loop 256
            lut.Push(A_Index - 1)
    }
    return lut
}

PillowTestConstantArray(count, value) {
    items := []
    loop count
        items.Push(value)
    return items
}

PillowTestImagePointUsesNativeLutOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([0, 1, 2, 10, 20, 30]))
    one := Pillow.Image.FromBytes("1", [4, 1], PillowTestBuffer([0x50]))
    pointed := 0
    oneInvert := 0
    oneConst := 0
    try {
        pointed := source.Point(PillowTestMakeInvertLut(3))
        oneInvert := one.Point(PillowTestMakeInvertLut(1))
        oneConst := one.Point(PillowTestConstantArray(256, 2))
        AhkTest.AssertEqual("RGB", pointed.Mode)
        AhkTest.AssertEqual([2, 1], pointed.Size)
        AhkTest.AssertEqual([255, 254, 253, 245, 235, 225], PillowTestBufferToArray(pointed.ToBytes()))
        AhkTest.AssertEqual("1", oneInvert.Mode)
        AhkTest.AssertEqual([0xA0], PillowTestBufferToArray(oneInvert.ToBytes()))
        AhkTest.AssertEqual(2, oneConst.GetPixel([0, 0]))
        AhkTest.AssertEqual([0xF0], PillowTestBufferToArray(oneConst.ToBytes()))
    } finally {
        for image in [oneConst, oneInvert, pointed, one] {
            if IsObject(image)
                image.Close()
        }
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Point applies a Pillow-style LUT through native handles", PillowTestImagePointUsesNativeLutOperation)

PillowTestImagePointAcceptsCallableFunction(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [5, 1], PillowTestBuffer([0, 1, 127, 128, 255]))
    rgb := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([0, 1, 2, 10, 20, 30]))
    lOut := 0
    rgbOut := 0
    calls := []
    try {
        lOut := l.Point((value) => value + 0.5)
        rgbOut := rgb.Point((value) => (calls.Push(value), 255 - value))

        AhkTest.AssertEqual("L", lOut.Mode)
        AhkTest.AssertEqual([0, 2, 128, 128, 255], PillowTestBufferToArray(lOut.ToBytes()))
        AhkTest.AssertEqual("RGB", rgbOut.Mode)
        AhkTest.AssertEqual([255, 254, 253, 245, 235, 225], PillowTestBufferToArray(rgbOut.ToBytes()))
        AhkTest.AssertEqual(256, calls.Length)
        AhkTest.AssertEqual(0, calls[1])
        AhkTest.AssertEqual(255, calls[256])
    } finally {
        for image in [rgbOut, lOut, rgb, l] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Point accepts callable functions", PillowTestImagePointAcceptsCallableFunction)

PillowTestImagePointSupportsTargetModeForSingleBandImages(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [6, 1], PillowTestBuffer([0, 1, 127, 128, 129, 255]))
    p := Pillow.Image.FromBytes("P", [6, 1], PillowTestBuffer([0, 1, 2, 3, 4, 5]))
    one := Pillow.Image.FromBytes("1", [4, 1], PillowTestBuffer([0x50]))
    outputs := []
    try {
        p.PutPalette([
            0, 0, 0,
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
            100, 110, 120,
            130, 140, 150,
        ])
        lToOne := l.Point(PillowTestMakeIdentityLut(1), "1")
        pToOne := p.Point(PillowTestMakeInvertLut(1), "1")
        oneToL := one.Point(PillowTestMakeIdentityLut(1), "L")
        lToP := l.Point(PillowTestMakeIdentityLut(1), "P")
        callableToOne := l.Point((value) => value, "1")
        outputs.Push(lToOne)
        outputs.Push(pToOne)
        outputs.Push(oneToL)
        outputs.Push(lToP)
        outputs.Push(callableToOne)

        AhkTest.AssertEqual("1", lToOne.Mode)
        AhkTest.AssertEqual([0x7C], PillowTestBufferToArray(lToOne.ToBytes()))
        AhkTest.AssertEqual("1", pToOne.Mode)
        AhkTest.AssertEqual([0xFC], PillowTestBufferToArray(pToOne.ToBytes()))
        AhkTest.AssertEqual("L", oneToL.Mode)
        AhkTest.AssertEqual([0, 255, 0, 255], PillowTestBufferToArray(oneToL.ToBytes()))
        AhkTest.AssertEqual("P", lToP.Mode)
        AhkTest.AssertEqual([0, 1, 127, 128, 129, 255], PillowTestBufferToArray(lToP.ToBytes()))
        AhkTest.AssertEqual([], lToP.GetPalette())
        AhkTest.AssertEqual([0x7C], PillowTestBufferToArray(callableToOne.ToBytes()))
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        one.Close()
        p.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.Point supports target mode for single-band images", PillowTestImagePointSupportsTargetModeForSingleBandImages)

PillowTestImageEvalBuildsSharedLutForAllBands(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([0, 1, 2, 10, 20, 30]))
    out := 0
    calls := []
    try {
        out := Pillow.Image.Eval(source, (value) => (calls.Push(value), 255 - value))

        AhkTest.AssertEqual("RGB", out.Mode)
        AhkTest.AssertEqual([2, 1], out.Size)
        AhkTest.AssertEqual([255, 254, 253, 245, 235, 225], PillowTestBufferToArray(out.ToBytes()))
        AhkTest.AssertEqual(256, calls.Length)
        AhkTest.AssertEqual(0, calls[1])
        AhkTest.AssertEqual(255, calls[256])
    } finally {
        if IsObject(out)
            out.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Eval builds one LUT and applies it through native handles", PillowTestImageEvalBuildsSharedLutForAllBands)

PillowTestImageEvalRoundsAndClipsLikePillowPoint(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [6, 1], PillowTestBuffer([0, 1, 2, 127, 128, 255]))
    half := 0
    low := 0
    high := 0
    try {
        half := Pillow.Image.Eval(source, (value) => value / 2)
        low := Pillow.Image.Eval(source, (*) => -1)
        high := Pillow.Image.Eval(source, (*) => 300)

        AhkTest.AssertEqual([0, 0, 1, 64, 64, 128], PillowTestBufferToArray(half.ToBytes()))
        AhkTest.AssertEqual([0, 0, 0, 0, 0, 0], PillowTestBufferToArray(low.ToBytes()))
        AhkTest.AssertEqual([255, 255, 255, 255, 255, 255], PillowTestBufferToArray(high.ToBytes()))
    } finally {
        for item in [high, low, half, source] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Eval rounds and clips generated LUT values", PillowTestImageEvalRoundsAndClipsLikePillowPoint)

PillowTestImageEvalUsesPythonHalfEvenRounding(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [8, 1], PillowTestBuffer([0, 1, 2, 3, 4, 5, 6, 7]))
    out := 0
    try {
        out := Pillow.Image.Eval(source, (value) => value + 0.5)

        AhkTest.AssertEqual([0, 2, 2, 4, 4, 6, 6, 8], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        if IsObject(out)
            out.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Eval uses Python half-even rounding", PillowTestImageEvalUsesPythonHalfEvenRounding)

PillowTestImageEvalSupportsRgba(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([0, 1, 2, 3, 10, 20, 30, 40]))
    out := 0
    try {
        out := Pillow.Image.Eval(source, (value) => 255 - value)

        AhkTest.AssertEqual("RGBA", out.Mode)
        AhkTest.AssertEqual([255, 254, 253, 252, 245, 235, 225, 215], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        if IsObject(out)
            out.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Eval supports RGBA through native point LUT", PillowTestImageEvalSupportsRgba)

PillowTestImageEvalSupportsModeOne(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("1", [4, 1], PillowTestBuffer([0x50]))
    inverted := 0
    constOne := 0
    constHigh := 0
    try {
        inverted := Pillow.Image.Eval(source, (value) => 255 - value)
        constOne := Pillow.Image.Eval(source, (*) => 1)
        constHigh := Pillow.Image.Eval(source, (*) => 256)

        AhkTest.AssertEqual("1", inverted.Mode)
        AhkTest.AssertEqual([0xA0], PillowTestBufferToArray(inverted.ToBytes()))
        AhkTest.AssertEqual(1, constOne.GetPixel([0, 0]))
        AhkTest.AssertEqual([0xF0], PillowTestBufferToArray(constOne.ToBytes()))
        AhkTest.AssertEqual(255, constHigh.GetPixel([0, 0]))
        AhkTest.AssertEqual([0xF0], PillowTestBufferToArray(constHigh.ToBytes()))
    } finally {
        for image in [constHigh, constOne, inverted, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Eval supports mode 1 through native point LUT", PillowTestImageEvalSupportsModeOne)

PillowTestImageEvalRejectsInvalidFunctionReturns(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [1, 1], PillowTestBuffer([1]))
    try {
        try {
            Pillow.Image.Eval(source, (*) => "7")
            AhkTest.Fail("Expected Image.Eval to reject non-numeric function returns")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "numeric") > 0 || InStr(err.Message, "Eval") > 0)
        }
    } finally {
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Eval rejects non-numeric function returns", PillowTestImageEvalRejectsInvalidFunctionReturns)

PillowTestImageGetChannelUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([1, 2, 3, 4, 10, 20, 30, 40]))
    alpha := 0
    green := 0
    try {
        alpha := source.GetChannel("A")
        green := source.GetChannel(1)
        AhkTest.AssertEqual("L", alpha.Mode)
        AhkTest.AssertEqual([2, 1], alpha.Size)
        AhkTest.AssertEqual([4, 40], PillowTestBufferToArray(alpha.ToBytes()))
        AhkTest.AssertEqual([2, 20], PillowTestBufferToArray(green.ToBytes()))
    } finally {
        if IsObject(green)
            green.Close()
        if IsObject(alpha)
            alpha.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.GetChannel extracts one channel through native handles", PillowTestImageGetChannelUsesNativeOperation)

PillowTestImageGetChannelSupportsLaMode(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("LA", [3, 1], PillowTestBuffer([10, 20, 30, 40, 50, 60]))
    l := 0
    a := 0
    try {
        l := source.GetChannel("L")
        a := source.GetChannel("A")
        AhkTest.AssertEqual("L", l.Mode)
        AhkTest.AssertEqual([10, 30, 50], PillowTestBufferToArray(l.ToBytes()))
        AhkTest.AssertEqual([20, 40, 60], PillowTestBufferToArray(a.ToBytes()))
    } finally {
        if IsObject(a)
            a.Close()
        if IsObject(l)
            l.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.GetChannel supports LA mode names", PillowTestImageGetChannelSupportsLaMode)

PillowTestImageGetChannelPreservesPaletteMode(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    palette := [0, 0, 0, 10, 20, 30, 200, 100, 50, 255, 255, 255]
    source := Pillow.Image.FromBytes("P", [4, 2], PillowTestBuffer([0, 1, 2, 3, 3, 2, 1, 0]))
    channel := 0
    channelRgb := 0
    try {
        source.PutPalette(palette)

        channel := source.GetChannel("P")
        channelRgb := channel.Convert("RGB")

        AhkTest.AssertEqual("P", channel.Mode)
        AhkTest.AssertEqual([4, 2], channel.Size)
        AhkTest.AssertEqual([0, 1, 2, 3, 3, 2, 1, 0], PillowTestBufferToArray(channel.ToBytes()))
        AhkTest.AssertEqual(palette, channel.GetPalette())
        AhkTest.AssertEqual([0, 0, 0, 10, 20, 30, 200, 100, 50, 255, 255, 255, 255, 255, 255, 200, 100, 50, 10, 20, 30, 0, 0, 0], PillowTestBufferToArray(channelRgb.ToBytes()))
    } finally {
        if IsObject(channelRgb)
            channelRgb.Close()
        if IsObject(channel)
            channel.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.GetChannel preserves P mode and palette", PillowTestImageGetChannelPreservesPaletteMode)

PillowTestImageGetBandsReturnsPillowModeBandNames(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.New("L", [1, 1])
    la := Pillow.Image.New("LA", [1, 1])
    rgb := Pillow.Image.New("RGB", [1, 1])
    rgba := Pillow.Image.New("RGBA", [1, 1])
    cmyk := 0
    try {
        AhkTest.AssertEqual(["L"], l.GetBands())
        AhkTest.AssertEqual(["L", "A"], la.GetBands())
        AhkTest.AssertEqual(["R", "G", "B"], rgb.GetBands())
        AhkTest.AssertEqual(["R", "G", "B", "A"], rgba.GetBands())
        cmyk := Pillow.Image.New("CMYK", [1, 1])
        AhkTest.AssertEqual(["C", "M", "Y", "K"], cmyk.GetBands())

        bands := rgba.GetBands()
        bands[1] := "changed"
        AhkTest.AssertEqual(["R", "G", "B", "A"], rgba.GetBands())
    } finally {
        if IsObject(cmyk)
            cmyk.Close()
        rgba.Close()
        rgb.Close()
        la.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.GetBands returns Pillow mode band names", PillowTestImageGetBandsReturnsPillowModeBandNames)

PillowTestImageHistogramUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGB", [3, 1], PillowTestBuffer([
        0, 10, 20,
        255, 10, 20,
        0, 255, 20,
    ]))
    try {
        hist := source.Histogram()
        AhkTest.AssertEqual(768, hist.Length)
        AhkTest.AssertEqual(2, hist[1])
        AhkTest.AssertEqual(1, hist[256])
        AhkTest.AssertEqual(2, hist[267])
        AhkTest.AssertEqual(1, hist[512])
        AhkTest.AssertEqual(3, hist[533])
    } finally {
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Histogram returns Pillow band histograms through native handles", PillowTestImageHistogramUsesNativeOperation)

PillowTestImageHistogramMatchesPillowLaMode(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("LA", [2, 1], PillowTestBuffer([10, 40, 200, 128]))
    try {
        hist := source.Histogram()
        AhkTest.AssertEqual(512, hist.Length)
        AhkTest.AssertEqual(1, hist[11])
        AhkTest.AssertEqual(1, hist[201])
        AhkTest.AssertEqual(1, hist[267])
        AhkTest.AssertEqual(1, hist[457])
        AhkTest.AssertEqual(4, hist[11] + hist[201] + hist[267] + hist[457])
    } finally {
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Histogram matches Pillow LA mode layout", PillowTestImageHistogramMatchesPillowLaMode)

PillowTestAssertFloatClose(expected, actual, tolerance := 0.0000001) {
    if Abs(expected - actual) > tolerance
        AhkTest.Fail("Expected " expected " +/- " tolerance ", got " actual)
}

PillowTestImageEntropyUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 10, 10, 255]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 1], PillowTestBuffer([
        0, 10, 20,
        255, 10, 20,
        0, 255, 20,
    ]))
    la := Pillow.Image.FromBytes("LA", [2, 1], PillowTestBuffer([10, 40, 200, 128]))
    maskSource := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 10, 20, 30]))
    mask := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 128, 255, 64]))
    empty := 0
    try {
        try {
            PillowTestAssertFloatClose(1.5, l.Entropy())
            PillowTestAssertFloatClose(2.197159723424149, rgb.Entropy())
            PillowTestAssertFloatClose(2.0, la.Entropy())
            PillowTestAssertFloatClose(1.584962500721156, maskSource.Entropy(mask))
            empty := l.Crop([0, 0, 0, 1])
            emptyEntropy := empty.Entropy()
            AhkTest.AssertTrue(emptyEntropy != emptyEntropy)
        } catch Error as err {
            AhkTest.Fail("Expected Image.Entropy to use native entropy: " err.Message)
        }
    } finally {
        if IsObject(empty)
            empty.Close()
        mask.Close()
        maskSource.Close()
        la.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.Entropy uses native histogram entropy", PillowTestImageEntropyUsesNativeOperation)

PillowTestImageStatsAcceptModeOneMasks(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 10, 20, 30]))
    mask := Pillow.Image.FromBytes("1", [4, 1], PillowTestBuffer([0x60]))
    equalized := 0
    autocontrasted := 0
    try {
        hist := source.Histogram(mask)
        stat := Pillow.ImageStat.Stat(source, mask)
        equalized := Pillow.ImageOps.Equalize(source, mask)
        autocontrasted := Pillow.ImageOps.Autocontrast(source, , , mask)

        AhkTest.AssertEqual(1, hist[11])
        AhkTest.AssertEqual(1, hist[21])
        AhkTest.AssertEqual(0, hist[1])
        AhkTest.AssertEqual(0, hist[31])
        PillowTestAssertFloatClose(1.0, source.Entropy(mask))
        AhkTest.AssertEqual([30.0], stat.Sum)
        AhkTest.AssertEqual([[10, 20]], stat.Extrema)
        AhkTest.AssertEqual([0, 10, 20, 30], PillowTestBufferToArray(equalized.ToBytes()))
        AhkTest.AssertEqual([0, 0, 255, 255], PillowTestBufferToArray(autocontrasted.ToBytes()))
    } finally {
        if IsObject(autocontrasted)
            autocontrasted.Close()
        if IsObject(equalized)
            equalized.Close()
        mask.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow histogram statistics accept mode 1 masks", PillowTestImageStatsAcceptModeOneMasks)

PillowTestAssertFloatArrayClose(expected, actual, tolerance := 0.0000001) {
    AhkTest.AssertEqual(expected.Length, actual.Length)
    for index, value in expected
        AhkTest.AssertTrue(Abs(value - actual[index]) <= tolerance)
}

PillowTestImageStatComputesPillowStatisticsFromNativeHistogram(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([0, 10, 255, 20, 30, 40]))
    rgb := Pillow.Image.FromBytes("RGB", [2, 2], PillowTestBuffer([
        10, 20, 30, 40, 50, 60,
        70, 80, 90, 100, 110, 120,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([10, 20, 30, 40, 200, 100, 50, 128]))
    la := Pillow.Image.FromBytes("LA", [2, 1], PillowTestBuffer([10, 40, 200, 128]))
    try {
        lStat := Pillow.ImageStat.Stat(l)
        rgbStat := Pillow.ImageStat.Stat(rgb)
        rgbaStat := Pillow.ImageStat.Stat(rgba)
        laStat := Pillow.ImageStat.Stat(la)

        AhkTest.AssertEqual([[0, 255]], lStat.Extrema)
        AhkTest.AssertEqual([6], lStat.Count)
        AhkTest.AssertEqual([355.0], lStat.Sum)
        AhkTest.AssertEqual([68025.0], lStat.Sum2)
        PillowTestAssertFloatArrayClose([59.166666666666664], lStat.Mean)
        AhkTest.AssertEqual([30], lStat.Median)
        PillowTestAssertFloatArrayClose([106.47769719523427], lStat.Rms)
        PillowTestAssertFloatArrayClose([7836.805555555555], lStat.Var)
        PillowTestAssertFloatArrayClose([88.52573386058742], lStat.StdDev)

        AhkTest.AssertEqual([[10, 100], [20, 110], [30, 120]], rgbStat.Extrema)
        AhkTest.AssertEqual([4, 4, 4], rgbStat.Count)
        AhkTest.AssertEqual([220.0, 260.0, 300.0], rgbStat.Sum)
        AhkTest.AssertEqual([16600.0, 21400.0, 27000.0], rgbStat.Sum2)
        PillowTestAssertFloatArrayClose([55.0, 65.0, 75.0], rgbStat.Mean)
        AhkTest.AssertEqual([70, 80, 90], rgbStat.Median)
        PillowTestAssertFloatArrayClose([64.42049363362563, 73.14369419163897, 82.15838362577492], rgbStat.Rms)
        PillowTestAssertFloatArrayClose([1125.0, 1125.0, 1125.0], rgbStat.Var)
        PillowTestAssertFloatArrayClose([33.54101966249684, 33.54101966249684, 33.54101966249684], rgbStat.StdDev)

        AhkTest.AssertEqual([[10, 200], [20, 100], [30, 50], [40, 128]], rgbaStat.Extrema)
        AhkTest.AssertEqual([2, 2, 2, 2], rgbaStat.Count)
        AhkTest.AssertEqual([210.0, 120.0, 80.0, 168.0], rgbaStat.Sum)
        AhkTest.AssertEqual([40100.0, 10400.0, 3400.0, 17984.0], rgbaStat.Sum2)
        PillowTestAssertFloatArrayClose([105.0, 60.0, 40.0, 84.0], rgbaStat.Mean)
        AhkTest.AssertEqual([200, 100, 50, 128], rgbaStat.Median)
        PillowTestAssertFloatArrayClose([141.59802258506295, 72.11102550927978, 41.23105625617661, 94.82615672903758], rgbaStat.Rms)
        PillowTestAssertFloatArrayClose([9025.0, 1600.0, 100.0, 1936.0], rgbaStat.Var)
        PillowTestAssertFloatArrayClose([95.0, 40.0, 10.0, 44.0], rgbaStat.StdDev)

        AhkTest.AssertEqual([[10, 200], [10, 200]], laStat.Extrema)
        AhkTest.AssertEqual([2, 2], laStat.Count)
        AhkTest.AssertEqual([210.0, 210.0], laStat.Sum)
        AhkTest.AssertEqual([40100.0, 40100.0], laStat.Sum2)
        PillowTestAssertFloatArrayClose([105.0, 105.0], laStat.Mean)
        AhkTest.AssertEqual([200, 200], laStat.Median)
        PillowTestAssertFloatArrayClose([141.59802258506295, 141.59802258506295], laStat.Rms)
        PillowTestAssertFloatArrayClose([9025.0, 9025.0], laStat.Var)
        PillowTestAssertFloatArrayClose([95.0, 95.0], laStat.StdDev)
    } finally {
        la.Close()
        rgba.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageStat.Stat computes Pillow statistics from native histograms", PillowTestImageStatComputesPillowStatisticsFromNativeHistogram)

PillowTestImageStatAcceptsHistogramListAndNativeMask(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [3, 1], PillowTestBuffer([0, 10, 10]))
    l := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 10, 20, 30]))
    lMask := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 128, 255, 64]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 1], PillowTestBuffer([
        10, 20, 30,
        40, 50, 60,
        70, 80, 90,
    ]))
    rgbMask := Pillow.Image.FromBytes("L", [3, 1], PillowTestBuffer([0, 128, 255]))
    la := Pillow.Image.FromBytes("LA", [2, 1], PillowTestBuffer([10, 40, 200, 128]))
    laMask := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([128, 255]))
    rgbaMask := Pillow.Image.FromBytes("RGBA", [4, 1], PillowTestBuffer([
        0, 0, 0, 0,
        0, 0, 0, 255,
        0, 0, 0, 255,
        0, 0, 0, 255,
    ]))
    try {
        stat := Pillow.ImageStat.Stat(source.Histogram())
        AhkTest.AssertEqual([[0, 10]], stat.Extrema)
        AhkTest.AssertEqual([3], stat.Count)
        AhkTest.AssertEqual([20.0], stat.Sum)
        AhkTest.AssertEqual([10], stat.Median)

        lStat := Pillow.ImageStat.Stat(l, lMask)
        rgbStat := Pillow.ImageStat.Stat(rgb, rgbMask)
        laStat := Pillow.ImageStat.Stat(la, laMask)

        AhkTest.AssertEqual([[10, 30]], lStat.Extrema)
        AhkTest.AssertEqual([3], lStat.Count)
        AhkTest.AssertEqual([60.0], lStat.Sum)
        AhkTest.AssertEqual([1400.0], lStat.Sum2)
        PillowTestAssertFloatArrayClose([20.0], lStat.Mean)
        AhkTest.AssertEqual([20], lStat.Median)
        PillowTestAssertFloatArrayClose([21.602468994692867], lStat.Rms)
        PillowTestAssertFloatArrayClose([66.66666666666667], lStat.Var)
        PillowTestAssertFloatArrayClose([8.16496580927726], lStat.StdDev)

        AhkTest.AssertEqual([[40, 70], [50, 80], [60, 90]], rgbStat.Extrema)
        AhkTest.AssertEqual([2, 2, 2], rgbStat.Count)
        AhkTest.AssertEqual([110.0, 130.0, 150.0], rgbStat.Sum)
        AhkTest.AssertEqual([6500.0, 8900.0, 11700.0], rgbStat.Sum2)
        PillowTestAssertFloatArrayClose([55.0, 65.0, 75.0], rgbStat.Mean)
        AhkTest.AssertEqual([70, 80, 90], rgbStat.Median)
        PillowTestAssertFloatArrayClose([57.0087712549569, 66.70832032063167, 76.48529270389177], rgbStat.Rms)
        PillowTestAssertFloatArrayClose([225.0, 225.0, 225.0], rgbStat.Var)
        PillowTestAssertFloatArrayClose([15.0, 15.0, 15.0], rgbStat.StdDev)

        AhkTest.AssertEqual([[10, 200], [10, 200]], laStat.Extrema)
        AhkTest.AssertEqual([2, 2], laStat.Count)
        AhkTest.AssertEqual([210.0, 210.0], laStat.Sum)
        AhkTest.AssertEqual([40100.0, 40100.0], laStat.Sum2)
        PillowTestAssertFloatArrayClose([105.0, 105.0], laStat.Mean)
        AhkTest.AssertEqual([200, 200], laStat.Median)

        try {
            Pillow.ImageStat.Stat(l, rgbaMask)
            AhkTest.Fail("Expected ImageStat.Stat to reject non-L masks")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "mask") > 0 || InStr(err.Message, "mismatch") > 0)
        }
    } finally {
        rgbaMask.Close()
        laMask.Close()
        la.Close()
        rgbMask.Close()
        rgb.Close()
        lMask.Close()
        l.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow ImageStat.Stat accepts histogram lists and native masks", PillowTestImageStatAcceptsHistogramListAndNativeMask)

PillowTestImageGetExtremaUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [3, 1], PillowTestBuffer([
        0, 10, 20,
        255, 10, 20,
        0, 255, 20,
    ]))
    l := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 10, 10, 255]))
    try {
        AhkTest.AssertEqual([[0, 255], [10, 255], [20, 20]], rgb.GetExtrema())
        AhkTest.AssertEqual([0, 255], l.GetExtrema())
    } finally {
        l.Close()
        rgb.Close()
    }
}

AhkTest.Test("Pillow Image.GetExtrema returns Pillow-style extrema through native handles", PillowTestImageGetExtremaUsesNativeOperation)

PillowTestImageGetBboxUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([
        0, 0, 0, 0,
        0, 5, 0, 0,
        0, 0, 7, 0,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [3, 1], PillowTestBuffer([
        0, 0, 0, 0,
        9, 0, 0, 0,
        0, 0, 0, 5,
    ]))
    empty := Pillow.Image.New("L", [3, 2])
    try {
        AhkTest.AssertEqual([1, 1, 3, 3], l.GetBbox())
        AhkTest.AssertEqual([2, 0, 3, 1], rgba.GetBbox())
        AhkTest.AssertEqual([1, 0, 3, 1], rgba.GetBbox(false))
        AhkTest.AssertEqual(0, empty.GetBbox())
    } finally {
        empty.Close()
        rgba.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.GetBbox returns Pillow-style bounding boxes through native handles", PillowTestImageGetBboxUsesNativeOperation)

PillowTestImageGetProjectionUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([
        0, 0, 0, 0,
        0, 5, 0, 0,
        0, 0, 7, 0,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [3, 1], PillowTestBuffer([
        0, 0, 0, 0,
        9, 0, 0, 0,
        0, 0, 0, 5,
    ]))
    empty := Pillow.Image.New("L", [3, 2])
    zeroWidth := 0
    try {
        zeroWidth := l.Crop([0, 0, 0, 2])

        AhkTest.AssertEqual([[0, 1, 1, 0], [0, 1, 1]], l.GetProjection())
        AhkTest.AssertEqual([[0, 1, 1], [1]], rgba.GetProjection())
        AhkTest.AssertEqual([[0, 0, 0], [0, 0]], empty.GetProjection())
        AhkTest.AssertEqual([[], [0, 0]], zeroWidth.GetProjection())
    } finally {
        if IsObject(zeroWidth)
            zeroWidth.Close()
        empty.Close()
        rgba.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.GetProjection returns Pillow-style axis projections through native handles", PillowTestImageGetProjectionUsesNativeOperation)

PillowTestFindColor(colors, expectedColor) {
    for entry in colors {
        color := entry[2]
        if IsObject(expectedColor) {
            if !IsObject(color) || color.Length != expectedColor.Length
                continue
            matched := true
            for index, value in expectedColor {
                if color[index] != value {
                    matched := false
                    break
                }
            }
            if matched
                return entry
        } else if !IsObject(color) && color = expectedColor {
            return entry
        }
    }
    return 0
}

PillowTestImageGetColorsUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [5, 1], PillowTestBuffer([0, 7, 7, 255, 0]))
    rgb := Pillow.Image.FromBytes("RGB", [4, 1], PillowTestBuffer([
        1, 2, 3,
        1, 2, 3,
        4, 5, 6,
        0, 0, 0,
    ]))
    empty := Pillow.Image.New("L", [1, 1])
    emptyCrop := 0
    try {
        emptyCrop := empty.Crop([0, 0, 0, 1])
        lColors := l.GetColors()
        rgbColors := rgb.GetColors()

        AhkTest.AssertEqual(3, lColors.Length)
        AhkTest.AssertEqual([2, 0], PillowTestFindColor(lColors, 0))
        AhkTest.AssertEqual([2, 7], PillowTestFindColor(lColors, 7))
        AhkTest.AssertEqual([1, 255], PillowTestFindColor(lColors, 255))
        AhkTest.AssertEqual(3, rgbColors.Length)
        AhkTest.AssertEqual([2, [1, 2, 3]], PillowTestFindColor(rgbColors, [1, 2, 3]))
        AhkTest.AssertEqual([1, [4, 5, 6]], PillowTestFindColor(rgbColors, [4, 5, 6]))
        AhkTest.AssertEqual([1, [0, 0, 0]], PillowTestFindColor(rgbColors, [0, 0, 0]))
        AhkTest.AssertEqual(0, rgb.GetColors(2))
        AhkTest.AssertEqual([], emptyCrop.GetColors(0))
        AhkTest.AssertEqual(0, emptyCrop.GetColors(-1))
    } finally {
        if IsObject(emptyCrop)
            emptyCrop.Close()
        empty.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.GetColors returns Pillow-style color counts through native handles", PillowTestImageGetColorsUsesNativeOperation)

PillowTestImageOpsGrayscaleConvertsCoreModes(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([10, 20, 30, 200, 100, 50]))
    rgba := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([10, 20, 30, 40, 200, 100, 50, 128]))
    rgbOut := 0
    rgbaOut := 0
    try {
        rgbOut := Pillow.ImageOps.Grayscale(rgb)
        rgbaOut := Pillow.ImageOps.Grayscale(rgba)

        AhkTest.AssertEqual("L", rgbOut.Mode)
        AhkTest.AssertEqual([2, 1], rgbOut.Size)
        AhkTest.AssertEqual([18, 124], PillowTestBufferToArray(rgbOut.ToBytes()))
        AhkTest.AssertEqual("L", rgbaOut.Mode)
        AhkTest.AssertEqual([2, 1], rgbaOut.Size)
        AhkTest.AssertEqual([18, 124], PillowTestBufferToArray(rgbaOut.ToBytes()))
    } finally {
        for item in [rgbaOut, rgbOut, rgba, rgb] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageOps.Grayscale converts RGB and RGBA through native handles", PillowTestImageOpsGrayscaleConvertsCoreModes)

PillowTestImageOpsGrayscaleReturnsIndependentLCopy(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [3, 1], PillowTestBuffer([1, 128, 255]))
    out := 0
    try {
        out := Pillow.ImageOps.Grayscale(source)
        data := source.DataPointer()
        NumPut("UChar", 99, data.Ptr, 0)

        AhkTest.AssertEqual("L", out.Mode)
        AhkTest.AssertEqual([3, 1], out.Size)
        AhkTest.AssertEqual([1, 128, 255], PillowTestBufferToArray(out.ToBytes()))
        AhkTest.AssertEqual([99, 128, 255], PillowTestBufferToArray(source.ToBytes()))
    } finally {
        for item in [out, source] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageOps.Grayscale returns an independent L copy", PillowTestImageOpsGrayscaleReturnsIndependentLCopy)

PillowTestImageOpsAutocontrastUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([10, 20, 30, 40]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 1], PillowTestBuffer([
        10, 50, 100,
        20, 60, 150,
        30, 70, 200,
    ]))
    lOut := 0
    rgbOut := 0
    try {
        lOut := Pillow.ImageOps.Autocontrast(l)
        rgbOut := Pillow.ImageOps.Autocontrast(rgb)
        AhkTest.AssertEqual("L", lOut.Mode)
        AhkTest.AssertEqual("RGB", rgbOut.Mode)
        AhkTest.AssertEqual([0, 85, 170, 255], PillowTestBufferToArray(lOut.ToBytes()))
        AhkTest.AssertEqual([0, 0, 0, 127, 127, 127, 255, 255, 254], PillowTestBufferToArray(rgbOut.ToBytes()))
    } finally {
        if IsObject(rgbOut)
            rgbOut.Close()
        if IsObject(lOut)
            lOut.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Autocontrast maps L and RGB images through native handles", PillowTestImageOpsAutocontrastUsesNativeOperation)

PillowTestImageOpsAutocontrastAppliesCutoffAndIgnore(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    cutoff := Pillow.Image.FromBytes("L", [10, 1], PillowTestBuffer([0, 0, 10, 20, 30, 40, 50, 60, 255, 255]))
    ignored := Pillow.Image.FromBytes("L", [5, 1], PillowTestBuffer([0, 10, 20, 30, 255]))
    cutoffOut := 0
    ignoredOut := 0
    try {
        cutoffOut := Pillow.ImageOps.Autocontrast(cutoff, [20, 20])
        ignoredOut := Pillow.ImageOps.Autocontrast(ignored, 0, [0, 255])
        AhkTest.AssertEqual([0, 0, 0, 51, 102, 153, 203, 255, 255, 255], PillowTestBufferToArray(cutoffOut.ToBytes()))
        AhkTest.AssertEqual([0, 0, 127, 255, 255], PillowTestBufferToArray(ignoredOut.ToBytes()))
    } finally {
        if IsObject(ignoredOut)
            ignoredOut.Close()
        if IsObject(cutoffOut)
            cutoffOut.Close()
        ignored.Close()
        cutoff.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Autocontrast accepts Pillow-style cutoff and ignore options", PillowTestImageOpsAutocontrastAppliesCutoffAndIgnore)

PillowTestImageOpsAutocontrastUsesMaskHistogram(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [5, 1], PillowTestBuffer([0, 10, 20, 30, 255]))
    mask := Pillow.Image.FromBytes("L", [5, 1], PillowTestBuffer([0, 255, 255, 0, 0]))
    rgb := Pillow.Image.FromBytes("RGB", [4, 1], PillowTestBuffer([
        10, 50, 100,
        20, 60, 150,
        30, 70, 200,
        250, 1, 2,
    ]))
    rgbMask := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 255, 255, 0]))
    lOut := 0
    ignoredOut := 0
    rgbOut := 0
    try {
        lOut := Pillow.ImageOps.Autocontrast(l, , , mask)
        ignoredOut := Pillow.ImageOps.Autocontrast(l, 0, 10, mask)
        rgbOut := Pillow.ImageOps.Autocontrast(rgb, , , rgbMask)

        AhkTest.AssertEqual([0, 0, 255, 255, 255], PillowTestBufferToArray(lOut.ToBytes()))
        AhkTest.AssertEqual([0, 10, 20, 30, 255], PillowTestBufferToArray(ignoredOut.ToBytes()))
        AhkTest.AssertEqual([0, 0, 0, 0, 0, 0, 255, 255, 254, 255, 0, 0], PillowTestBufferToArray(rgbOut.ToBytes()))
    } finally {
        for image in [rgbOut, ignoredOut, lOut, rgbMask, rgb, mask, l] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageOps.Autocontrast uses mask histogram", PillowTestImageOpsAutocontrastUsesMaskHistogram)

PillowTestImageOpsAutocontrastPreserveToneUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [4, 1], PillowTestBuffer([
        10, 50, 100,
        20, 60, 150,
        30, 70, 200,
        250, 1, 2,
    ]))
    mask := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 255, 255, 0]))
    out := 0
    maskedOut := 0
    try {
        out := Pillow.ImageOps.Autocontrast(rgb, , , , true)
        maskedOut := Pillow.ImageOps.Autocontrast(rgb, , , mask, true)

        AhkTest.AssertEqual([0, 47, 255, 0, 127, 255, 0, 207, 255, 255, 0, 0], PillowTestBufferToArray(out.ToBytes()))
        AhkTest.AssertEqual([0, 0, 255, 0, 34, 255, 0, 204, 255, 255, 0, 0], PillowTestBufferToArray(maskedOut.ToBytes()))
    } finally {
        for image in [maskedOut, out, mask, rgb] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageOps.Autocontrast preserve_tone uses native handles", PillowTestImageOpsAutocontrastPreserveToneUsesNativeOperation)

PillowTestImageOpsLutTransformsUseNativeOperations(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [5, 1], PillowTestBuffer([0, 1, 127, 128, 255]))
    rgb := Pillow.Image.FromBytes("RGB", [3, 1], PillowTestBuffer([
        0, 1, 2,
        127, 128, 129,
        253, 254, 255,
    ]))
    lInvert := 0
    rgbInvert := 0
    lPosterize := 0
    rgbPosterize := 0
    lSolarize := 0
    rgbSolarize := 0
    try {
        lInvert := Pillow.ImageOps.Invert(l)
        rgbInvert := Pillow.ImageOps.Invert(rgb)
        lPosterize := Pillow.ImageOps.Posterize(l, 4)
        rgbPosterize := Pillow.ImageOps.Posterize(rgb, 4)
        lSolarize := Pillow.ImageOps.Solarize(l, 128)
        rgbSolarize := Pillow.ImageOps.Solarize(rgb)

        AhkTest.AssertEqual([255, 254, 128, 127, 0], PillowTestBufferToArray(lInvert.ToBytes()))
        AhkTest.AssertEqual([255, 254, 253, 128, 127, 126, 2, 1, 0], PillowTestBufferToArray(rgbInvert.ToBytes()))
        AhkTest.AssertEqual([0, 0, 112, 128, 240], PillowTestBufferToArray(lPosterize.ToBytes()))
        AhkTest.AssertEqual([0, 0, 0, 112, 128, 128, 240, 240, 240], PillowTestBufferToArray(rgbPosterize.ToBytes()))
        AhkTest.AssertEqual([0, 1, 127, 127, 0], PillowTestBufferToArray(lSolarize.ToBytes()))
        AhkTest.AssertEqual([0, 1, 2, 127, 127, 126, 2, 1, 0], PillowTestBufferToArray(rgbSolarize.ToBytes()))
    } finally {
        for image in [rgbSolarize, lSolarize, rgbPosterize, lPosterize, rgbInvert, lInvert] {
            if IsObject(image)
                image.Close()
        }
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageOps LUT transforms map L and RGB images through native handles", PillowTestImageOpsLutTransformsUseNativeOperations)

PillowTestImageOpsInvertSupportsModeOne(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("1", [3, 1], PillowTestBuffer([0x40]))
    out := 0
    try {
        out := Pillow.ImageOps.Invert(image)

        AhkTest.AssertEqual("1", out.Mode)
        AhkTest.AssertEqual([0xA0], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        if IsObject(out)
            out.Close()
        image.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Invert supports mode 1 through native handles", PillowTestImageOpsInvertSupportsModeOne)

PillowTestImageOpsLutTransformsRejectBadWrapperParameters(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [1, 1], PillowTestBuffer([128]))
    try {
        try {
            Pillow.ImageOps.Posterize(image, 9)
            AhkTest.Fail("Expected Posterize to reject bits above 8")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "bits") > 0)
        }

        try {
            Pillow.ImageOps.Solarize(image, "128")
            AhkTest.Fail("Expected Solarize to reject a non-numeric threshold")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "threshold") > 0)
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageOps LUT transforms reject invalid wrapper parameters", PillowTestImageOpsLutTransformsRejectBadWrapperParameters)

PillowTestImageOpsColorizeUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [5, 1], PillowTestBuffer([0, 64, 128, 192, 255]))
    out := 0
    midOut := 0
    try {
        out := Pillow.ImageOps.Colorize(image, [10, 20, 30], [110, 120, 130])
        midOut := Pillow.ImageOps.Colorize(image, [0, 0, 0], [255, 255, 255], [255, 0, 0])

        AhkTest.AssertEqual("RGB", out.Mode)
        AhkTest.AssertEqual([5, 1], out.Size)
        AhkTest.AssertEqual([10, 20, 30, 35, 45, 55, 60, 70, 80, 85, 95, 105, 110, 120, 130], PillowTestBufferToArray(out.ToBytes()))
        AhkTest.AssertEqual([0, 0, 0, 128, 0, 0, 255, 1, 1, 255, 129, 129, 255, 255, 255], PillowTestBufferToArray(midOut.ToBytes()))
    } finally {
        if IsObject(midOut)
            midOut.Close()
        if IsObject(out)
            out.Close()
        image.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Colorize maps L images through native handles", PillowTestImageOpsColorizeUsesNativeOperation)

PillowTestImageOpsColorizeAcceptsPointParametersAndRejectsInvalidInputs(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [5, 1], PillowTestBuffer([0, 64, 128, 192, 255]))
    rgb := Pillow.Image.FromBytes("RGB", [1, 1], PillowTestBuffer([1, 2, 3]))
    out := 0
    try {
        out := Pillow.ImageOps.Colorize(image, [0, 0, 0], [255, 255, 255], [0, 255, 0], 32, 224, 128)
        AhkTest.AssertEqual([0, 0, 0, 0, 85, 0, 0, 255, 0, 170, 255, 170, 255, 255, 255], PillowTestBufferToArray(out.ToBytes()))

        try {
            Pillow.ImageOps.Colorize(rgb, [0, 0, 0], [255, 255, 255])
            AhkTest.Fail("Expected Colorize to reject non-L source mode")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "Colorize") > 0)
        }

        try {
            Pillow.ImageOps.Colorize(image, [0, 0], [255, 255, 255])
            AhkTest.Fail("Expected Colorize to reject short RGB color")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "color") > 0)
        }

        try {
            Pillow.ImageOps.Colorize(image, [0, 0, 0], [255, 255, 255], [128, 128, 128], 100, 200, 50)
            AhkTest.Fail("Expected Colorize to reject invalid point ordering")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "point") > 0)
        }
    } finally {
        if IsObject(out)
            out.Close()
        rgb.Close()
        image.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Colorize accepts points and rejects invalid inputs", PillowTestImageOpsColorizeAcceptsPointParametersAndRejectsInvalidInputs)

PillowTestImageOpsEqualizeUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    lValues := []
    rgbValues := []
    loop 400 {
        index := A_Index - 1
        if index < 300 {
            lValues.Push(0)
            r := 0
            g := 10
        } else if index < 350 {
            lValues.Push(128)
            r := 128
            g := 200
        } else {
            lValues.Push(255)
            r := 255
            g := 200
        }

        b := index < 100 ? 0 : index < 200 ? 64 : index < 300 ? 128 : 255
        rgbValues.Push(r)
        rgbValues.Push(g)
        rgbValues.Push(b)
    }
    l := Pillow.Image.FromBytes("L", [400, 1], PillowTestBuffer(lValues))
    rgb := Pillow.Image.FromBytes("RGB", [400, 1], PillowTestBuffer(rgbValues))
    lOut := 0
    rgbOut := 0
    try {
        lOut := Pillow.ImageOps.Equalize(l)
        rgbOut := Pillow.ImageOps.Equalize(rgb)

        lData := PillowTestBufferToArray(lOut.ToBytes())
        rgbData := PillowTestBufferToArray(rgbOut.ToBytes())
        AhkTest.AssertEqual(0, lData[1])
        AhkTest.AssertEqual(0, lData[300])
        AhkTest.AssertEqual(255, lData[301])
        AhkTest.AssertEqual(255, lData[400])
        AhkTest.AssertEqual([0, 0, 0], [rgbData[1], rgbData[2], rgbData[3]])
        AhkTest.AssertEqual([0, 0, 100], [rgbData[301], rgbData[302], rgbData[303]])
        AhkTest.AssertEqual([0, 0, 200], [rgbData[601], rgbData[602], rgbData[603]])
        AhkTest.AssertEqual([255, 255, 255], [rgbData[901], rgbData[902], rgbData[903]])
    } finally {
        if IsObject(rgbOut)
            rgbOut.Close()
        if IsObject(lOut)
            lOut.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Equalize maps L and RGB histograms through native handles", PillowTestImageOpsEqualizeUsesNativeOperation)

PillowFacadeTestEqualizePaletteFixture() {
    values := []
    mask := []
    loop 400 {
        index := A_Index - 1
        values.Push(index < 300 ? 0 : index < 350 ? 1 : 2)
        mask.Push(index < 290 ? 0 : index < 310 ? 255 : index < 350 ? 0 : 255)
    }
    return {
        Values: values,
        Mask: mask,
        Palette: [0, 10, 0, 128, 200, 64, 255, 200, 255],
    }
}

PillowFacadeTestAssertSamples(data, positions, expected) {
    actual := []
    for position in positions
        actual.Push(data[position])
    AhkTest.AssertEqual(expected, actual)
}

PillowTestImageOpsEqualizeConvertsPaletteModeToRgb(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    fixture := PillowFacadeTestEqualizePaletteFixture()
    source := Pillow.Image.FromBytes("P", [400, 1], PillowTestBuffer(fixture.Values))
    out := 0
    try {
        source.PutPalette(fixture.Palette)

        out := Pillow.ImageOps.Equalize(source)

        AhkTest.AssertEqual("RGB", out.Mode)
        AhkTest.AssertEqual([400, 1], out.Size)
        PillowFacadeTestAssertSamples(
            PillowTestBufferToArray(out.ToBytes()),
            [1, 2, 3, 898, 899, 900, 901, 902, 903, 1048, 1049, 1050, 1051, 1052, 1053, 1198, 1199, 1200],
            [0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255])
    } finally {
        if IsObject(out)
            out.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Equalize converts P images to RGB like Pillow", PillowTestImageOpsEqualizeConvertsPaletteModeToRgb)

PillowFacadeTestAppendEqualizeMaskGroup(fixture, count, lValue, r, g, b, maskValue) {
    loop count {
        fixture.L.Push(lValue)
        fixture.RGB.Push(r)
        fixture.RGB.Push(g)
        fixture.RGB.Push(b)
        fixture.Mask.Push(maskValue)
    }
}

PillowFacadeTestEqualizeMaskFixture() {
    fixture := { L: [], RGB: [], Mask: [] }
    PillowFacadeTestAppendEqualizeMaskGroup(fixture, 300, 0, 0, 10, 20, 0)
    PillowFacadeTestAppendEqualizeMaskGroup(fixture, 10, 0, 0, 10, 20, 255)
    PillowFacadeTestAppendEqualizeMaskGroup(fixture, 300, 64, 64, 70, 80, 128)
    PillowFacadeTestAppendEqualizeMaskGroup(fixture, 300, 128, 128, 130, 140, 255)
    PillowFacadeTestAppendEqualizeMaskGroup(fixture, 100, 255, 255, 250, 240, 64)
    return fixture
}

PillowTestImageOpsEqualizeUsesMaskHistogram(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    fixture := PillowFacadeTestEqualizeMaskFixture()
    l := Pillow.Image.FromBytes("L", [1010, 1], PillowTestBuffer(fixture.L))
    rgb := Pillow.Image.FromBytes("RGB", [1010, 1], PillowTestBuffer(fixture.RGB))
    mask := Pillow.Image.FromBytes("L", [1010, 1], PillowTestBuffer(fixture.Mask))
    lOut := 0
    rgbOut := 0
    try {
        try {
            lOut := Pillow.ImageOps.Equalize(l, mask)
            rgbOut := Pillow.ImageOps.Equalize(rgb, mask)
        } catch Error as err {
            AhkTest.Fail("Expected ImageOps.Equalize to accept mask: " err.Message)
        }

        lData := PillowTestBufferToArray(lOut.ToBytes())
        rgbData := PillowTestBufferToArray(rgbOut.ToBytes())

        AhkTest.AssertEqual([0, 0, 5, 155, 255], [lData[1], lData[301], lData[311], lData[611], lData[911]])
        AhkTest.AssertEqual([0, 0, 0], [rgbData[1], rgbData[2], rgbData[3]])
        AhkTest.AssertEqual([5, 5, 5], [rgbData[931], rgbData[932], rgbData[933]])
        AhkTest.AssertEqual([155, 155, 155], [rgbData[1831], rgbData[1832], rgbData[1833]])
        AhkTest.AssertEqual([255, 255, 255], [rgbData[2731], rgbData[2732], rgbData[2733]])
    } finally {
        if IsObject(rgbOut)
            rgbOut.Close()
        if IsObject(lOut)
            lOut.Close()
        mask.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Equalize uses mask histogram", PillowTestImageOpsEqualizeUsesMaskHistogram)

PillowTestImageOpsEqualizeConvertsMaskedPaletteModeToRgb(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    fixture := PillowFacadeTestEqualizePaletteFixture()
    source := Pillow.Image.FromBytes("P", [400, 1], PillowTestBuffer(fixture.Values))
    mask := Pillow.Image.FromBytes("L", [400, 1], PillowTestBuffer(fixture.Mask))
    out := 0
    try {
        source.PutPalette(fixture.Palette)

        out := Pillow.ImageOps.Equalize(source, mask)

        AhkTest.AssertEqual("RGB", out.Mode)
        AhkTest.AssertEqual([400, 1], out.Size)
        PillowFacadeTestAssertSamples(
            PillowTestBufferToArray(out.ToBytes()),
            [1, 2, 3, 871, 872, 873, 898, 899, 900, 901, 902, 903, 1048, 1049, 1050, 1051, 1052, 1053, 1198, 1199, 1200],
            [0, 10, 0, 0, 10, 0, 0, 10, 0, 128, 200, 64, 128, 200, 64, 255, 200, 255, 255, 200, 255])
    } finally {
        if IsObject(out)
            out.Close()
        mask.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Equalize converts masked P images to RGB like Pillow", PillowTestImageOpsEqualizeConvertsMaskedPaletteModeToRgb)

PillowTestImageOpsEqualizeRejectsUnsupportedMode(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgba := Pillow.Image.FromBytes("RGBA", [1, 1], PillowTestBuffer([1, 2, 3, 4]))
    try {
        try {
            Pillow.ImageOps.Equalize(rgba)
            AhkTest.Fail("Expected Equalize to reject RGBA")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
        }
    } finally {
        rgba.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Equalize rejects modes Pillow _lut does not support", PillowTestImageOpsEqualizeRejectsUnsupportedMode)

PillowTestImageOpsCropRemovesScalarAndTupleBorders(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 10, 11,
    ]))
    scalar := 0
    tuple2 := 0
    tuple4 := 0
    try {
        scalar := Pillow.ImageOps.Crop(source, 1)
        tuple2 := Pillow.ImageOps.Crop(source, [1, 0])
        tuple4 := Pillow.ImageOps.Crop(source, [1, 0, 2, 1])

        AhkTest.AssertEqual([2, 1], scalar.Size)
        AhkTest.AssertEqual([5, 6], PillowTestBufferToArray(scalar.ToBytes()))
        AhkTest.AssertEqual([2, 3], tuple2.Size)
        AhkTest.AssertEqual([1, 2, 5, 6, 9, 10], PillowTestBufferToArray(tuple2.ToBytes()))
        AhkTest.AssertEqual([1, 2], tuple4.Size)
        AhkTest.AssertEqual([1, 5], PillowTestBufferToArray(tuple4.ToBytes()))
    } finally {
        for item in [tuple4, tuple2, scalar, source] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageOps.Crop removes scalar and tuple borders through native crop", PillowTestImageOpsCropRemovesScalarAndTupleBorders)

PillowTestImageOpsCropSupportsNegativeBorderExpansion(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 10, 11,
    ]))
    out := 0
    try {
        out := Pillow.ImageOps.Crop(source, -1)

        AhkTest.AssertEqual([6, 5], out.Size)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 0, 1, 2, 3, 0,
            0, 4, 5, 6, 7, 0,
            0, 8, 9, 10, 11, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        for item in [out, source] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageOps.Crop supports negative borders through native crop fill", PillowTestImageOpsCropSupportsNegativeBorderExpansion)

PillowTestImageOpsCropRejectsInvalidBorders(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [4, 3], PillowTestBuffer([
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 10, 11,
    ]))
    try {
        try {
            Pillow.ImageOps.Crop(source, [1, 2, 3])
            AhkTest.Fail("Expected ImageOps.Crop to reject a 3-item border")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "border") > 0 || InStr(err.Message, "Crop") > 0)
        }

        try {
            Pillow.ImageOps.Crop(source, 2)
            AhkTest.Fail("Expected ImageOps.Crop to reject an inverted crop box")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0 || InStr(err.Message, "Coordinate") > 0 || InStr(err.Message, "Crop") > 0)
        }
    } finally {
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Crop rejects invalid borders", PillowTestImageOpsCropRejectsInvalidBorders)

PillowTestImageOpsExpandUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [2, 2], PillowTestBuffer([1, 2, 3, 4]))
    rgb := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    rgba := Pillow.Image.FromBytes("RGBA", [1, 1], PillowTestBuffer([1, 2, 3, 4]))
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        lOut := Pillow.ImageOps.Expand(l, 1, 7)
        rgbOut := Pillow.ImageOps.Expand(rgb, [1, 0, 2, 1], [7, 8, 9])
        rgbaOut := Pillow.ImageOps.Expand(rgba, 1, [7, 8, 9, 10])

        AhkTest.AssertEqual([4, 4], lOut.Size)
        AhkTest.AssertEqual([
            7, 7, 7, 7,
            7, 1, 2, 7,
            7, 3, 4, 7,
            7, 7, 7, 7,
        ], PillowTestBufferToArray(lOut.ToBytes()))

        AhkTest.AssertEqual([5, 2], rgbOut.Size)
        AhkTest.AssertEqual([
            7, 8, 9, 1, 2, 3, 4, 5, 6, 7, 8, 9, 7, 8, 9,
            7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9,
        ], PillowTestBufferToArray(rgbOut.ToBytes()))

        AhkTest.AssertEqual("RGBA", rgbaOut.Mode)
        AhkTest.AssertEqual([
            7, 8, 9, 10, 7, 8, 9, 10, 7, 8, 9, 10,
            7, 8, 9, 10, 1, 2, 3, 4, 7, 8, 9, 10,
            7, 8, 9, 10, 7, 8, 9, 10, 7, 8, 9, 10,
        ], PillowTestBufferToArray(rgbaOut.ToBytes()))
    } finally {
        for image in [rgbaOut, rgbOut, lOut] {
            if IsObject(image)
                image.Close()
        }
        rgba.Close()
        rgb.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Expand adds filled borders through native handles", PillowTestImageOpsExpandUsesNativeOperation)

PillowTestImageOpsExpandSupportsLaFillColors(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("LA", [2, 2], PillowTestBuffer([10, 0, 80, 64, 160, 128, 250, 255]))
    scalar := 0
    tuple1 := 0
    tuple2 := 0
    try {
        scalar := Pillow.ImageOps.Expand(source, 1, 9)
        tuple1 := Pillow.ImageOps.Expand(source, 1, [9])
        tuple2 := Pillow.ImageOps.Expand(source, 1, [9, 17])

        AhkTest.AssertEqual("LA", tuple2.Mode)
        AhkTest.AssertEqual([4, 4], tuple2.Size)
        AhkTest.AssertEqual([
            9, 0, 9, 0, 9, 0, 9, 0,
            9, 0, 10, 0, 80, 64, 9, 0,
            9, 0, 160, 128, 250, 255, 9, 0,
            9, 0, 9, 0, 9, 0, 9, 0,
        ], PillowTestBufferToArray(scalar.ToBytes()))
        AhkTest.AssertEqual(PillowTestBufferToArray(scalar.ToBytes()), PillowTestBufferToArray(tuple1.ToBytes()))
        AhkTest.AssertEqual([
            9, 17, 9, 17, 9, 17, 9, 17,
            9, 17, 10, 0, 80, 64, 9, 17,
            9, 17, 160, 128, 250, 255, 9, 17,
            9, 17, 9, 17, 9, 17, 9, 17,
        ], PillowTestBufferToArray(tuple2.ToBytes()))

        try {
            Pillow.ImageOps.Expand(source, 1, [9, 17, 99])
            AhkTest.Fail("Expected LA fill to reject a three-item tuple")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "fill") > 0)
        }
    } finally {
        for image in [tuple2, tuple1, scalar] {
            if IsObject(image)
                image.Close()
        }
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Expand supports LA fill colors", PillowTestImageOpsExpandSupportsLaFillColors)

PillowTestImageOpsExpandParsesBorderAndFillLikePillow(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGBA", [1, 1], PillowTestBuffer([1, 2, 3, 4]))
    tuple2 := 0
    rgbFill := 0
    scalarFill := 0
    try {
        tuple2 := Pillow.ImageOps.Expand(source, [1, 2], [7, 8, 9])
        rgbFill := Pillow.ImageOps.Expand(source, 1, [7, 8, 9])
        scalarFill := Pillow.ImageOps.Expand(source, 1, 7)

        AhkTest.AssertEqual([3, 5], tuple2.Size)
        AhkTest.AssertEqual([
            7, 8, 9, 255, 7, 8, 9, 255, 7, 8, 9, 255,
            7, 8, 9, 255, 7, 8, 9, 255, 7, 8, 9, 255,
            7, 8, 9, 255, 1, 2, 3, 4, 7, 8, 9, 255,
            7, 8, 9, 255, 7, 8, 9, 255, 7, 8, 9, 255,
            7, 8, 9, 255, 7, 8, 9, 255, 7, 8, 9, 255,
        ], PillowTestBufferToArray(tuple2.ToBytes()))
        AhkTest.AssertEqual(255, NumGet(rgbFill.ToBytes(), 3, "UChar"))
        scalarData := PillowTestBufferToArray(scalarFill.ToBytes())
        AhkTest.AssertEqual([7, 0, 0, 0], [scalarData[1], scalarData[2], scalarData[3], scalarData[4]])
    } finally {
        for image in [scalarFill, rgbFill, tuple2] {
            if IsObject(image)
                image.Close()
        }
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Expand parses border and fill shortcuts", PillowTestImageOpsExpandParsesBorderAndFillLikePillow)

PillowTestImageOpsExpandFillColorMatchesModeOneSemantics(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("1", [2, 1], PillowTestBuffer([0x80]))
    expanded := 0
    try {
        expanded := Pillow.ImageOps.Expand(source, 1, [256])

        AhkTest.AssertEqual("1", expanded.Mode)
        AhkTest.AssertEqual([4, 3], expanded.Size)
        AhkTest.AssertEqual([0xF0, 0xD0, 0xF0], PillowTestBufferToArray(expanded.ToBytes()))
    } finally {
        if IsObject(expanded)
            expanded.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Expand fill color matches mode 1 semantics", PillowTestImageOpsExpandFillColorMatchesModeOneSemantics)

PillowTestImageOpsExpandRejectsInvalidWrapperParameters(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("RGB", [1, 1], PillowTestBuffer([1, 2, 3]))
    try {
        try {
            Pillow.ImageOps.Expand(image, [1, 2, 3], 0)
            AhkTest.Fail("Expected Expand to reject a 3-item border")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "border") > 0)
        }

        try {
            Pillow.ImageOps.Expand(image, 1, [1, 2])
            AhkTest.Fail("Expected Expand to reject a 2-item RGB fill")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "fill") > 0)
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageOps.Expand rejects invalid wrapper parameters", PillowTestImageOpsExpandRejectsInvalidWrapperParameters)

PillowTestImageSplitUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([1, 2, 3, 4, 10, 20, 30, 40]))
    bands := []
    try {
        bands := source.Split()
        AhkTest.AssertEqual(4, bands.Length)
        AhkTest.AssertEqual("L", bands[1].Mode)
        AhkTest.AssertEqual([1, 10], PillowTestBufferToArray(bands[1].ToBytes()))
        AhkTest.AssertEqual([2, 20], PillowTestBufferToArray(bands[2].ToBytes()))
        AhkTest.AssertEqual([3, 30], PillowTestBufferToArray(bands[3].ToBytes()))
        AhkTest.AssertEqual([4, 40], PillowTestBufferToArray(bands[4].ToBytes()))
    } finally {
        for band in bands
            band.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Split returns all L bands through one native call", PillowTestImageSplitUsesNativeOperation)

PillowTestImageSplitPreservesPaletteMode(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    palette := [0, 0, 0, 10, 20, 30, 200, 100, 50, 255, 255, 255]
    source := Pillow.Image.FromBytes("P", [4, 2], PillowTestBuffer([0, 1, 2, 3, 3, 2, 1, 0]))
    bands := []
    bandRgb := 0
    try {
        source.PutPalette(palette)

        bands := source.Split()
        bandRgb := bands[1].Convert("RGB")

        AhkTest.AssertEqual(1, bands.Length)
        AhkTest.AssertEqual("P", bands[1].Mode)
        AhkTest.AssertEqual([0, 1, 2, 3, 3, 2, 1, 0], PillowTestBufferToArray(bands[1].ToBytes()))
        AhkTest.AssertEqual(palette, bands[1].GetPalette())
        AhkTest.AssertEqual([0, 0, 0, 10, 20, 30, 200, 100, 50, 255, 255, 255, 255, 255, 255, 200, 100, 50, 10, 20, 30, 0, 0, 0], PillowTestBufferToArray(bandRgb.ToBytes()))
    } finally {
        if IsObject(bandRgb)
            bandRgb.Close()
        for band in bands
            band.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Split preserves P mode and palette", PillowTestImageSplitPreservesPaletteMode)

PillowTestImageMergeStaticUsesNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    r := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([1, 2]))
    g := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([3, 4]))
    b := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([5, 6]))
    a := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([7, 8]))
    rgb := 0
    rgba := 0
    try {
        rgb := Pillow.Image.Merge("RGB", [r, g, b])
        rgba := Pillow.Image.Merge("RGBA", [r, g, b, a])
        AhkTest.AssertEqual("RGB", rgb.Mode)
        AhkTest.AssertEqual([1, 3, 5, 2, 4, 6], PillowTestBufferToArray(rgb.ToBytes()))
        AhkTest.AssertEqual("RGBA", rgba.Mode)
        AhkTest.AssertEqual([1, 3, 5, 7, 2, 4, 6, 8], PillowTestBufferToArray(rgba.ToBytes()))
    } finally {
        if IsObject(rgba)
            rgba.Close()
        if IsObject(rgb)
            rgb.Close()
        a.Close()
        b.Close()
        g.Close()
        r.Close()
    }
}

AhkTest.Test("Pillow Image.Merge interleaves L bands through native handles", PillowTestImageMergeStaticUsesNativeHandles)

PillowTestImageMergeSupportsLaMode(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([1, 2]))
    a := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([7, 8]))
    la := 0
    try {
        la := Pillow.Image.Merge("LA", [l, a])
        AhkTest.AssertEqual("LA", la.Mode)
        AhkTest.AssertEqual([1, 7, 2, 8], PillowTestBufferToArray(la.ToBytes()))
    } finally {
        if IsObject(la)
            la.Close()
        a.Close()
        l.Close()
    }
}

AhkTest.Test("Pillow Image.Merge supports LA mode", PillowTestImageMergeSupportsLaMode)

PillowTestImagePutAlphaUsesNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 2, 3, 10, 20, 30]))
    alpha := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([50, 60]))
    byValue := 0
    byImage := 0
    try {
        byValue := source.PutAlpha(7)
        byImage := source.PutAlpha(alpha)
        AhkTest.AssertEqual("RGBA", byValue.Mode)
        AhkTest.AssertEqual([1, 2, 3, 7, 10, 20, 30, 7], PillowTestBufferToArray(byValue.ToBytes()))
        AhkTest.AssertEqual([1, 2, 3, 50, 10, 20, 30, 60], PillowTestBufferToArray(byImage.ToBytes()))
    } finally {
        if IsObject(byImage)
            byImage.Close()
        if IsObject(byValue)
            byValue.Close()
        alpha.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.PutAlpha returns an RGBA image through native handles", PillowTestImagePutAlphaUsesNativeOperation)

PillowTestImagePutAlphaReturnsLaForLMode(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([1, 10]))
    existing := Pillow.Image.FromBytes("LA", [2, 1], PillowTestBuffer([1, 2, 10, 20]))
    alpha := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([50, 60]))
    byValue := 0
    byImage := 0
    replaced := 0
    try {
        byValue := source.PutAlpha(7)
        byImage := source.PutAlpha(alpha)
        replaced := existing.PutAlpha(alpha)
        AhkTest.AssertEqual("LA", byValue.Mode)
        AhkTest.AssertEqual([1, 7, 10, 7], PillowTestBufferToArray(byValue.ToBytes()))
        AhkTest.AssertEqual("LA", byImage.Mode)
        AhkTest.AssertEqual([1, 50, 10, 60], PillowTestBufferToArray(byImage.ToBytes()))
        AhkTest.AssertEqual([1, 50, 10, 60], PillowTestBufferToArray(replaced.ToBytes()))
    } finally {
        if IsObject(replaced)
            replaced.Close()
        if IsObject(byImage)
            byImage.Close()
        if IsObject(byValue)
            byValue.Close()
        alpha.Close()
        existing.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.PutAlpha returns LA for L and LA sources", PillowTestImagePutAlphaReturnsLaForLMode)

PillowTestImagePasteMutatesTargetThroughNativeHandleOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    target := Pillow.Image.FromBytes("RGB", [4, 3], PillowTestBuffer([
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    ]))
    source := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        1, 2, 3, 4, 5, 6, 7, 8, 9,
        11, 12, 13, 14, 15, 16, 17, 18, 19,
    ]))
    try {
        target.Paste(source, [1, 1])
        AhkTest.AssertEqual([
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 1, 2, 3, 4, 5, 6, 7, 8, 9,
            10, 10, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        ], PillowTestBufferToArray(target.ToBytes()))
    } finally {
        source.Close()
        target.Close()
    }
}

AhkTest.Test("Pillow Image.Paste mutates target through native handles", PillowTestImagePasteMutatesTargetThroughNativeHandleOperation)

PillowTestImagePasteUsesMaskAndConvertsSourceMode(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    target := Pillow.Image.FromBytes("RGB", [4, 3], PillowTestBuffer([
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    ]))
    source := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([0, 50, 100, 150, 200, 250]))
    mask := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([255, 128, 0, 64, 255, 255]))
    try {
        try {
            target.Paste(source, [1, 1], mask)
        } catch Error as err {
            AhkTest.Fail("Expected Image.Paste to accept mask: " err.Message)
        }
        AhkTest.AssertEqual([
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 0, 0, 0, 30, 30, 30, 10, 10, 10,
            10, 10, 10, 45, 45, 45, 200, 200, 200, 250, 250, 250,
        ], PillowTestBufferToArray(target.ToBytes()))
    } finally {
        mask.Close()
        source.Close()
        target.Close()
    }
}

AhkTest.Test("Pillow Image.Paste uses mask and converts source mode", PillowTestImagePasteUsesMaskAndConvertsSourceMode)

PillowTestImagePasteAcceptsModeOneMask(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    target := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([1, 2, 3, 4]))
    source := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([9, 8]))
    mask := Pillow.Image.FromBytes("1", [2, 1], PillowTestBuffer([0x80]))
    try {
        target.Paste(source, [1, 0], mask)
        AhkTest.AssertEqual([1, 9, 3, 4], PillowTestBufferToArray(target.ToBytes()))
    } finally {
        mask.Close()
        source.Close()
        target.Close()
    }
}

AhkTest.Test("Pillow Image.Paste accepts mode 1 masks", PillowTestImagePasteAcceptsModeOneMask)

PillowTestImagePasteAcceptsColorSources(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    target := Pillow.Image.FromBytes("RGB", [4, 3], PillowTestBuffer([
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    ]))
    mask := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([0, 128]))
    try {
        target.Paste([9, 8, 7], [1, 0, 3, 2])
        target.Paste([1, 2, 3], [1, 2, 3, 3], mask)

        AhkTest.AssertEqual([
            10, 10, 10, 9, 8, 7, 9, 8, 7, 10, 10, 10,
            10, 10, 10, 9, 8, 7, 9, 8, 7, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 5, 6, 6, 10, 10, 10,
        ], PillowTestBufferToArray(target.ToBytes()))
    } finally {
        mask.Close()
        target.Close()
    }
}

AhkTest.Test("Pillow Image.Paste accepts color sources through native path", PillowTestImagePasteAcceptsColorSources)

PillowTestImagePasteColorAcceptsModeOneMask(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    target := Pillow.Image.FromBytes("RGB", [4, 1], PillowTestBuffer([
        1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4,
    ]))
    mask := Pillow.Image.FromBytes("1", [2, 1], PillowTestBuffer([0x80]))
    try {
        target.Paste([9, 8, 7], [1, 0, 3, 1], mask)
        AhkTest.AssertEqual([
            1, 1, 1, 9, 8, 7, 3, 3, 3, 4, 4, 4,
        ], PillowTestBufferToArray(target.ToBytes()))
    } finally {
        mask.Close()
        target.Close()
    }
}

AhkTest.Test("Pillow Image.Paste color source accepts mode 1 masks", PillowTestImagePasteColorAcceptsModeOneMask)

PillowTestImagePasteColorMatchesModeOneScalarSemantics(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    target := Pillow.Image.New("1", [4, 1], 0)
    mask := Pillow.Image.FromBytes("1", [4, 1], PillowTestBuffer([0x50]))
    try {
        target.Paste(256, [0, 0, 4, 1], mask)
        AhkTest.AssertEqual([0x50], PillowTestBufferToArray(target.ToBytes()))
        AhkTest.AssertEqual(255, target.GetPixel([1, 0]))

        target.Paste([256], [0, 0, 4, 1])
        AhkTest.AssertEqual([0xF0], PillowTestBufferToArray(target.ToBytes()))

        target.Paste([-1], [0, 0, 4, 1])
        AhkTest.AssertEqual([0x00], PillowTestBufferToArray(target.ToBytes()))
    } finally {
        mask.Close()
        target.Close()
    }
}

AhkTest.Test("Pillow Image.Paste color source matches mode 1 scalar semantics", PillowTestImagePasteColorMatchesModeOneScalarSemantics)

PillowTestImagePasteColorRejectsTwoItemBox(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    target := Pillow.Image.New("RGB", [2, 2])
    try {
        try {
            target.Paste([9, 8, 7], [0, 0])
            AhkTest.Fail("Expected color Paste to reject two-item box")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "4-item box") > 0)
        }
    } finally {
        target.Close()
    }
}

AhkTest.Test("Pillow Image.Paste color source requires a four-item box", PillowTestImagePasteColorRejectsTwoItemBox)

PillowTestImageBlendStaticUsesNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([10, 20, 30, 100, 110, 120]))
    right := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([250, 240, 230, 1, 2, 3]))
    blended := 0
    try {
        blended := Pillow.Image.Blend(left, right, 0.25)
        AhkTest.AssertEqual("RGB", blended.Mode)
        AhkTest.AssertEqual([2, 1], blended.Size)
        AhkTest.AssertEqual([70, 75, 80, 75, 83, 90], PillowTestBufferToArray(blended.ToBytes()))
    } finally {
        if IsObject(blended)
            blended.Close()
        right.Close()
        left.Close()
    }
}

AhkTest.Test("Pillow Image.Blend blends images through native handles", PillowTestImageBlendStaticUsesNativeHandles)

PillowTestImageCompositeStaticUsesNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("RGB", [3, 1], PillowTestBuffer([10, 20, 30, 40, 50, 60, 70, 80, 90]))
    right := Pillow.Image.FromBytes("RGB", [3, 1], PillowTestBuffer([200, 210, 220, 180, 170, 160, 100, 90, 80]))
    mask := Pillow.Image.FromBytes("L", [3, 1], PillowTestBuffer([0, 128, 255]))
    out := 0
    try {
        out := Pillow.Image.Composite(left, right, mask)

        AhkTest.AssertEqual("RGB", out.Mode)
        AhkTest.AssertEqual([3, 1], out.Size)
        AhkTest.AssertEqual([200, 210, 220, 110, 110, 110, 70, 80, 90], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        for item in [out, mask, right, left] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Composite blends images through native handles", PillowTestImageCompositeStaticUsesNativeHandles)

PillowTestImageCompositeUsesRgbaMaskAndClipsSource(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("RGB", [1, 1], PillowTestBuffer([10, 20, 30]))
    right := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([200, 210, 220, 180, 170, 160]))
    mask := Pillow.Image.FromBytes("RGBA", [1, 1], PillowTestBuffer([1, 2, 3, 128]))
    out := 0
    try {
        out := Pillow.Image.Composite(left, right, mask)

        AhkTest.AssertEqual("RGB", out.Mode)
        AhkTest.AssertEqual([2, 1], out.Size)
        AhkTest.AssertEqual([105, 115, 125, 180, 170, 160], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        for item in [out, mask, right, left] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Composite uses RGBA mask alpha and target size", PillowTestImageCompositeUsesRgbaMaskAndClipsSource)

PillowTestImageCompositeUsesModeOneAndLaMasks(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [3, 1], PillowTestBuffer([10, 20, 30]))
    right := Pillow.Image.FromBytes("L", [3, 1], PillowTestBuffer([100, 110, 120]))
    oneMask := Pillow.Image.FromBytes("1", [3, 1], PillowTestBuffer([0xA0]))
    laMask := Pillow.Image.FromBytes("LA", [3, 1], PillowTestBuffer([9, 255, 9, 0, 9, 128]))
    oneOut := 0
    laOut := 0
    try {
        oneOut := Pillow.Image.Composite(left, right, oneMask)
        laOut := Pillow.Image.Composite(left, right, laMask)

        AhkTest.AssertEqual([10, 110, 30], PillowTestBufferToArray(oneOut.ToBytes()))
        AhkTest.AssertEqual([10, 110, 75], PillowTestBufferToArray(laOut.ToBytes()))
    } finally {
        for item in [laOut, oneOut, laMask, oneMask, right, left] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Composite accepts mode 1 and LA masks", PillowTestImageCompositeUsesModeOneAndLaMasks)

PillowTestImageCompositeConvertsSourceToTargetMode(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([10, 20, 30, 40, 50, 60]))
    target := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([1, 2]))
    mask := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([255, 128]))
    out := 0
    try {
        out := Pillow.Image.Composite(source, target, mask)

        AhkTest.AssertEqual("L", out.Mode)
        AhkTest.AssertEqual([18, 25], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        for item in [out, mask, target, source] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Composite converts source to target mode", PillowTestImageCompositeConvertsSourceToTargetMode)

PillowTestImageCompositeRejectsUnsupportedMask(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([10, 20, 30, 40, 50, 60]))
    right := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([200, 210, 220, 180, 170, 160]))
    mask := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    try {
        try {
            Pillow.Image.Composite(left, right, mask)
            AhkTest.Fail("Expected Image.Composite to reject unsupported mask")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "bad transparency mask") > 0 || InStr(err.Message, "invalid argument") > 0 || InStr(err.Message, "Composite") > 0)
        }
    } finally {
        for item in [mask, right, left] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow Image.Composite rejects unsupported mask modes", PillowTestImageCompositeRejectsUnsupportedMask)

PillowTestImageEnhanceBrightnessContrastColorUseNativeComposition(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        10, 20, 30, 80, 40, 10, 130, 140, 150,
        200, 190, 180, 20, 120, 220, 250, 240, 10,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [3, 2], PillowTestBuffer([
        10, 20, 30, 40, 80, 40, 10, 70, 130, 140, 150, 100,
        200, 190, 180, 130, 20, 120, 220, 160, 250, 240, 10, 190,
    ]))
    cmyk := Pillow.Image.FromBytes("CMYK", [3, 2], PillowTestBuffer([
        0, 0, 0, 0, 20, 40, 60, 80, 100, 50, 25, 125,
        255, 128, 64, 32, 10, 200, 30, 40, 200, 10, 220, 0,
    ]))
    outputs := []
    try {
        cases := [
            { Out: Pillow.ImageEnhance.Brightness(rgb).Enhance(0.5), Bytes: [
                5, 10, 15, 40, 20, 5, 65, 70, 75,
                100, 95, 90, 10, 60, 110, 125, 120, 5,
            ] },
            { Out: Pillow.ImageEnhance.Brightness(rgba).Enhance(2), Bytes: [
                20, 40, 60, 40, 160, 80, 20, 70, 255, 255, 255, 100,
                255, 255, 255, 130, 40, 240, 255, 160, 255, 255, 20, 190,
            ] },
            { Out: Pillow.ImageEnhance.Contrast(rgb).Enhance(0), Bytes: [
                119, 119, 119, 119, 119, 119, 119, 119, 119,
                119, 119, 119, 119, 119, 119, 119, 119, 119,
            ] },
            { Out: Pillow.ImageEnhance.Contrast(rgba).Enhance(2), Bytes: [
                0, 0, 0, 40, 41, 0, 0, 70, 141, 161, 181, 100,
                255, 255, 241, 130, 0, 121, 255, 160, 255, 255, 0, 190,
            ] },
            { Out: Pillow.ImageEnhance.Color(rgb).Enhance(0), Bytes: [
                18, 18, 18, 49, 49, 49, 138, 138, 138,
                192, 192, 192, 102, 102, 102, 217, 217, 217,
            ] },
            { Out: Pillow.ImageEnhance.Color(rgb).Enhance(-0.5), Bytes: [
                22, 17, 12, 33, 53, 68, 142, 137, 132,
                188, 193, 198, 143, 93, 43, 200, 205, 255,
            ] },
            { Out: Pillow.ImageEnhance.Color(rgba).Enhance(0), Bytes: [
                18, 18, 18, 40, 49, 49, 49, 70, 138, 138, 138, 100,
                192, 192, 192, 130, 102, 102, 102, 160, 217, 217, 217, 190,
            ] },
            { Out: Pillow.ImageEnhance.Color(rgba).Enhance(0.5), Bytes: [
                14, 19, 24, 40, 64, 44, 29, 70, 134, 139, 144, 100,
                196, 191, 186, 130, 61, 111, 161, 160, 233, 228, 113, 190,
            ] },
            { Out: Pillow.ImageEnhance.Color(rgba).Enhance(2), Bytes: [
                2, 22, 42, 40, 111, 31, 0, 70, 122, 142, 162, 100,
                208, 188, 168, 130, 0, 138, 255, 160, 255, 255, 0, 190,
            ] },
            { Out: Pillow.ImageEnhance.Color(rgba).Enhance(-0.5), Bytes: [
                22, 17, 12, 40, 33, 53, 68, 70, 142, 137, 132, 100,
                188, 193, 198, 130, 143, 93, 43, 160, 200, 205, 255, 190,
            ] },
        ]
        for item in cases {
            outputs.Push(item.Out)
            AhkTest.AssertEqual(item.Bytes, PillowTestBufferToArray(item.Out.ToBytes()))
        }

        cmykBrightness := Pillow.ImageEnhance.Brightness(cmyk).Enhance(1.2)
        outputs.Push(cmykBrightness)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 24, 48, 72, 96, 120, 60, 30, 150,
            255, 153, 76, 38, 12, 240, 36, 48, 240, 12, 255, 0,
        ], PillowTestBufferToArray(cmykBrightness.ToBytes()))

        cmykContrast := Pillow.ImageEnhance.Contrast(cmyk).Enhance(1.2)
        outputs.Push(cmykContrast)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 24, 48, 72, 73, 120, 60, 30, 127,
            255, 153, 76, 16, 12, 240, 36, 25, 240, 12, 255, 0,
        ], PillowTestBufferToArray(cmykContrast.ToBytes()))

        cmykColor := Pillow.ImageEnhance.Color(cmyk).Enhance(1.2)
        outputs.Push(cmykColor)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 24, 48, 72, 75, 120, 60, 30, 118,
            255, 153, 76, 4, 12, 240, 36, 19, 240, 12, 255, 0,
        ], PillowTestBufferToArray(cmykColor.ToBytes()))
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        cmyk.Close()
        rgba.Close()
        rgb.Close()
    }
}

AhkTest.Test("Pillow ImageEnhance brightness contrast and color use native composition", PillowTestImageEnhanceBrightnessContrastColorUseNativeComposition)

PillowTestImageEnhanceSharpnessUsesNativeFilterComposition(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [5, 5], PillowTestBuffer([
        0, 20, 40, 60, 80,
        30, 80, 120, 160, 200,
        10, 70, 180, 220, 240,
        50, 90, 130, 170, 210,
        20, 60, 100, 140, 180,
    ]))
    smooth := 0
    sharp := 0
    try {
        smooth := Pillow.ImageEnhance.Sharpness(source).Enhance(0)
        sharp := Pillow.ImageEnhance.Sharpness(source).Enhance(2)
        AhkTest.AssertEqual([
            0, 20, 40, 60, 80,
            30, 67, 110, 149, 200,
            10, 80, 149, 193, 240,
            50, 82, 129, 173, 210,
            20, 60, 100, 140, 180,
        ], PillowTestBufferToArray(smooth.ToBytes()))
        AhkTest.AssertEqual([
            0, 20, 40, 60, 80,
            30, 93, 130, 171, 200,
            10, 60, 211, 247, 240,
            50, 98, 131, 167, 210,
            20, 60, 100, 140, 180,
        ], PillowTestBufferToArray(sharp.ToBytes()))
    } finally {
        for image in [sharp, smooth, source] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageEnhance sharpness uses native filter composition", PillowTestImageEnhanceSharpnessUsesNativeFilterComposition)

PillowTestImageEnhanceRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.New("RGB", [1, 1], [10, 20, 30])
    try {
        try {
            Pillow.ImageEnhance.Brightness("x")
            AhkTest.Fail("Expected ImageEnhance constructor to reject non-image")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "Pillow.Image") > 0)
        }
        try {
            Pillow.ImageEnhance.Brightness(source).Enhance("x")
            AhkTest.Fail("Expected ImageEnhance.Enhance to reject non-numeric factor")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "factor") > 0)
        }
    } finally {
        source.Close()
    }
}

AhkTest.Test("Pillow ImageEnhance rejects invalid arguments", PillowTestImageEnhanceRejectsInvalidArguments)

PillowTestImageChopsBlendAndCompositeUseNativeAliases(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([10, 20, 30, 100, 110, 120]))
    right := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([250, 240, 230, 1, 2, 3]))
    mask := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([0, 128]))
    blended := 0
    composited := 0
    try {
        blended := Pillow.ImageChops.Blend(left, right, 0.25)
        composited := Pillow.ImageChops.Composite(left, right, mask)

        AhkTest.AssertEqual([70, 75, 80, 75, 83, 90], PillowTestBufferToArray(blended.ToBytes()))
        AhkTest.AssertEqual([250, 240, 230, 51, 56, 62], PillowTestBufferToArray(composited.ToBytes()))
    } finally {
        for item in [composited, blended, mask, right, left] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops.Blend and Composite alias native image operations", PillowTestImageChopsBlendAndCompositeUseNativeAliases)

PillowTestImageChopsConstantReturnsLImage(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("RGB", [2, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]))
    out := 0
    try {
        out := Pillow.ImageChops.Constant(image, 300)

        AhkTest.AssertEqual("L", out.Mode)
        AhkTest.AssertEqual([2, 2], out.Size)
        AhkTest.AssertEqual([255, 255, 255, 255], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        for item in [out, image] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops.Constant returns an L image through native handles", PillowTestImageChopsConstantReturnsLImage)

PillowTestImageChopsConstantHandlesEmptyImages(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    empty := 0
    out := 0
    try {
        empty := image.Crop([1, 0, 1, 2])
        out := Pillow.ImageChops.Constant(empty, 7)

        AhkTest.AssertEqual("L", out.Mode)
        AhkTest.AssertEqual([0, 2], out.Size)
        AhkTest.AssertEqual([], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        for item in [out, empty, image] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops.Constant handles empty images", PillowTestImageChopsConstantHandlesEmptyImages)

PillowTestImageChopsDuplicateReturnsIndependentCopy(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    duplicate := 0
    try {
        duplicate := Pillow.ImageChops.Duplicate(image)
        data := image.DataPointer()
        NumPut("UChar", 99, data.Ptr, 0)

        AhkTest.AssertEqual("L", duplicate.Mode)
        AhkTest.AssertEqual([3, 2], duplicate.Size)
        AhkTest.AssertEqual([1, 2, 3, 4, 5, 6], PillowTestBufferToArray(duplicate.ToBytes()))
        AhkTest.AssertEqual([99, 2, 3, 4, 5, 6], PillowTestBufferToArray(image.ToBytes()))
    } finally {
        for item in [duplicate, image] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops.Duplicate returns an independent native copy", PillowTestImageChopsDuplicateReturnsIndependentCopy)

PillowTestImageChopsInvertUsesNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([1, 2, 3, 4, 5, 6, 7, 8]))
    out := 0
    try {
        out := Pillow.ImageChops.Invert(image)

        AhkTest.AssertEqual("RGBA", out.Mode)
        AhkTest.AssertEqual([2, 1], out.Size)
        AhkTest.AssertEqual([254, 253, 252, 251, 250, 249, 248, 247], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        for item in [out, image] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops.Invert maps pixels through native handles", PillowTestImageChopsInvertUsesNativeHandles)

PillowTestImageChopsConstantRejectsInvalidValue(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [1, 1], PillowTestBuffer([1]))
    try {
        try {
            Pillow.ImageChops.Constant(image, 1.5)
            AhkTest.Fail("Expected ImageChops.Constant to reject non-integer values")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "value") > 0)
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageChops.Constant rejects invalid value", PillowTestImageChopsConstantRejectsInvalidValue)

PillowTestImageChopsDifferenceUsesNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 50, 200, 255, 0, 80]))
    right := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([4, 20, 100, 15, 200, 90]))
    out := 0
    try {
        out := Pillow.ImageChops.Difference(left, right)

        AhkTest.AssertEqual("RGB", out.Mode)
        AhkTest.AssertEqual([2, 1], out.Size)
        AhkTest.AssertEqual([3, 30, 100, 240, 200, 10], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        if IsObject(out)
            out.Close()
        right.Close()
        left.Close()
    }
}

AhkTest.Test("Pillow ImageChops.Difference computes absolute pixel differences through native handles", PillowTestImageChopsDifferenceUsesNativeHandles)

PillowTestImageChopsDifferenceUsesOverlappingOutputSize(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 40, 200, 255]))
    right := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([255, 10]))
    out := 0
    try {
        out := Pillow.ImageChops.Difference(left, right)

        AhkTest.AssertEqual("L", out.Mode)
        AhkTest.AssertEqual([2, 1], out.Size)
        AhkTest.AssertEqual([255, 30], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        if IsObject(out)
            out.Close()
        right.Close()
        left.Close()
    }
}

AhkTest.Test("Pillow ImageChops.Difference returns the overlapping region size", PillowTestImageChopsDifferenceUsesOverlappingOutputSize)

PillowTestImageChopsDifferenceAllowsEmptyOverlap(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([1, 2]))
    empty := 0
    out := 0
    try {
        empty := source.Crop([1, 0, 1, 1])
        out := Pillow.ImageChops.Difference(empty, source)

        AhkTest.AssertEqual("L", out.Mode)
        AhkTest.AssertEqual([0, 1], out.Size)
        AhkTest.AssertEqual([], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        if IsObject(out)
            out.Close()
        if IsObject(empty)
            empty.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow ImageChops.Difference allows empty overlapping output", PillowTestImageChopsDifferenceAllowsEmptyOverlap)

PillowTestImageChopsDifferenceRejectsModeMismatch(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [1, 1], PillowTestBuffer([1]))
    right := Pillow.Image.FromBytes("RGB", [1, 1], PillowTestBuffer([1, 2, 3]))
    try {
        try {
            Pillow.ImageChops.Difference(left, right)
            AhkTest.Fail("Expected ImageChops.Difference to reject mode mismatch")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "mismatch") > 0)
        }
    } finally {
        right.Close()
        left.Close()
    }
}

AhkTest.Test("Pillow ImageChops.Difference rejects mode mismatch", PillowTestImageChopsDifferenceRejectsModeMismatch)

PillowTestImageChopsMultiplyUsesNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 50, 200, 255, 0, 80]))
    right := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([4, 20, 100, 15, 200, 90]))
    out := 0
    try {
        out := Pillow.ImageChops.Multiply(left, right)

        AhkTest.AssertEqual("RGB", out.Mode)
        AhkTest.AssertEqual([2, 1], out.Size)
        AhkTest.AssertEqual([0, 3, 78, 15, 0, 28], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        if IsObject(out)
            out.Close()
        right.Close()
        left.Close()
    }
}

AhkTest.Test("Pillow ImageChops.Multiply computes pixel products through native handles", PillowTestImageChopsMultiplyUsesNativeHandles)

PillowTestImageChopsMultiplyUsesOverlappingAndEmptyOutputSize(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 40, 200, 255]))
    right := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([255, 10]))
    empty := 0
    overlap := 0
    emptyOut := 0
    try {
        empty := left.Crop([1, 0, 1, 1])
        overlap := Pillow.ImageChops.Multiply(left, right)
        emptyOut := Pillow.ImageChops.Multiply(empty, left)

        AhkTest.AssertEqual([2, 1], overlap.Size)
        AhkTest.AssertEqual([0, 1], PillowTestBufferToArray(overlap.ToBytes()))
        AhkTest.AssertEqual([0, 1], emptyOut.Size)
        AhkTest.AssertEqual([], PillowTestBufferToArray(emptyOut.ToBytes()))
    } finally {
        for image in [emptyOut, overlap, empty, right, left] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops.Multiply returns overlapping and empty output sizes", PillowTestImageChopsMultiplyUsesOverlappingAndEmptyOutputSize)

PillowTestImageChopsMultiplyRejectsModeMismatch(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [1, 1], PillowTestBuffer([1]))
    right := Pillow.Image.FromBytes("RGB", [1, 1], PillowTestBuffer([1, 2, 3]))
    try {
        try {
            Pillow.ImageChops.Multiply(left, right)
            AhkTest.Fail("Expected ImageChops.Multiply to reject mode mismatch")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "mismatch") > 0)
        }
    } finally {
        right.Close()
        left.Close()
    }
}

AhkTest.Test("Pillow ImageChops.Multiply rejects mode mismatch", PillowTestImageChopsMultiplyRejectsModeMismatch)

PillowTestImageChopsScreenUsesNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 50, 200, 255, 0, 80]))
    right := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([4, 20, 100, 15, 200, 90]))
    out := 0
    try {
        out := Pillow.ImageChops.Screen(left, right)

        AhkTest.AssertEqual("RGB", out.Mode)
        AhkTest.AssertEqual([2, 1], out.Size)
        AhkTest.AssertEqual([5, 67, 222, 255, 200, 142], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        if IsObject(out)
            out.Close()
        right.Close()
        left.Close()
    }
}

AhkTest.Test("Pillow ImageChops.Screen computes screened pixels through native handles", PillowTestImageChopsScreenUsesNativeHandles)

PillowTestImageChopsScreenUsesOverlappingAndEmptyOutputSize(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 40, 200, 255]))
    right := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([255, 10]))
    empty := 0
    overlap := 0
    emptyOut := 0
    try {
        empty := left.Crop([1, 0, 1, 1])
        overlap := Pillow.ImageChops.Screen(left, right)
        emptyOut := Pillow.ImageChops.Screen(empty, left)

        AhkTest.AssertEqual([2, 1], overlap.Size)
        AhkTest.AssertEqual([255, 49], PillowTestBufferToArray(overlap.ToBytes()))
        AhkTest.AssertEqual([0, 1], emptyOut.Size)
        AhkTest.AssertEqual([], PillowTestBufferToArray(emptyOut.ToBytes()))
    } finally {
        for image in [emptyOut, overlap, empty, right, left] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops.Screen returns overlapping and empty output sizes", PillowTestImageChopsScreenUsesOverlappingAndEmptyOutputSize)

PillowTestImageChopsScreenRejectsModeMismatch(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [1, 1], PillowTestBuffer([1]))
    right := Pillow.Image.FromBytes("RGB", [1, 1], PillowTestBuffer([1, 2, 3]))
    try {
        try {
            Pillow.ImageChops.Screen(left, right)
            AhkTest.Fail("Expected ImageChops.Screen to reject mode mismatch")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "mismatch") > 0)
        }
    } finally {
        right.Close()
        left.Close()
    }
}

AhkTest.Test("Pillow ImageChops.Screen rejects mode mismatch", PillowTestImageChopsScreenRejectsModeMismatch)

PillowTestImageChopsLighterAndDarkerUseNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 50, 200, 255, 0, 80]))
    right := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([4, 20, 100, 15, 200, 90]))
    lightOut := 0
    darkOut := 0
    try {
        lightOut := Pillow.ImageChops.Lighter(left, right)
        darkOut := Pillow.ImageChops.Darker(left, right)

        AhkTest.AssertEqual("RGB", lightOut.Mode)
        AhkTest.AssertEqual([2, 1], lightOut.Size)
        AhkTest.AssertEqual([4, 50, 200, 255, 200, 90], PillowTestBufferToArray(lightOut.ToBytes()))
        AhkTest.AssertEqual([1, 20, 100, 15, 0, 80], PillowTestBufferToArray(darkOut.ToBytes()))
    } finally {
        for image in [darkOut, lightOut, right, left] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops lighter and darker compute extrema through native handles", PillowTestImageChopsLighterAndDarkerUseNativeHandles)

PillowTestImageChopsLighterAndDarkerUseOverlappingAndEmptyOutputSize(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 40, 200, 255]))
    right := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([255, 10]))
    empty := 0
    lightOverlap := 0
    darkOverlap := 0
    lightEmpty := 0
    darkEmpty := 0
    try {
        empty := left.Crop([1, 0, 1, 1])
        lightOverlap := Pillow.ImageChops.Lighter(left, right)
        darkOverlap := Pillow.ImageChops.Darker(left, right)
        lightEmpty := Pillow.ImageChops.Lighter(empty, left)
        darkEmpty := Pillow.ImageChops.Darker(empty, left)

        AhkTest.AssertEqual([2, 1], lightOverlap.Size)
        AhkTest.AssertEqual([255, 40], PillowTestBufferToArray(lightOverlap.ToBytes()))
        AhkTest.AssertEqual([0, 10], PillowTestBufferToArray(darkOverlap.ToBytes()))
        AhkTest.AssertEqual([0, 1], lightEmpty.Size)
        AhkTest.AssertEqual([], PillowTestBufferToArray(lightEmpty.ToBytes()))
        AhkTest.AssertEqual([], PillowTestBufferToArray(darkEmpty.ToBytes()))
    } finally {
        for image in [darkEmpty, lightEmpty, darkOverlap, lightOverlap, empty, right, left] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops lighter and darker return overlapping and empty output sizes", PillowTestImageChopsLighterAndDarkerUseOverlappingAndEmptyOutputSize)

PillowTestImageChopsLighterAndDarkerRejectModeMismatch(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [1, 1], PillowTestBuffer([1]))
    right := Pillow.Image.FromBytes("RGB", [1, 1], PillowTestBuffer([1, 2, 3]))
    try {
        try {
            Pillow.ImageChops.Lighter(left, right)
            AhkTest.Fail("Expected ImageChops.Lighter to reject mode mismatch")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "mismatch") > 0)
        }

        try {
            Pillow.ImageChops.Darker(left, right)
            AhkTest.Fail("Expected ImageChops.Darker to reject mode mismatch")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "mismatch") > 0)
        }
    } finally {
        right.Close()
        left.Close()
    }
}

AhkTest.Test("Pillow ImageChops lighter and darker reject mode mismatch", PillowTestImageChopsLighterAndDarkerRejectModeMismatch)

PillowTestImageChopsSoftHardOverlayUseNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 50, 200, 255, 0, 80]))
    right := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([4, 20, 100, 15, 200, 90]))
    softOut := 0
    hardOut := 0
    overlayOut := 0
    try {
        softOut := Pillow.ImageChops.SoftLight(left, right)
        hardOut := Pillow.ImageChops.HardLight(left, right)
        overlayOut := Pillow.ImageChops.Overlay(left, right)

        AhkTest.AssertEqual("RGB", softOut.Mode)
        AhkTest.AssertEqual([2, 1], softOut.Size)
        AhkTest.AssertEqual([0, 16, 190, 255, 0, 63], PillowTestBufferToArray(softOut.ToBytes()))
        AhkTest.AssertEqual([0, 7, 157, 30, 145, 56], PillowTestBufferToArray(hardOut.ToBytes()))
        AhkTest.AssertEqual([0, 7, 188, 255, 0, 56], PillowTestBufferToArray(overlayOut.ToBytes()))
    } finally {
        for image in [overlayOut, hardOut, softOut, right, left] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops soft hard overlay compute blend modes through native handles", PillowTestImageChopsSoftHardOverlayUseNativeHandles)

PillowTestImageChopsSoftHardOverlayUseOverlappingAndEmptyOutputSize(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 40, 128, 255]))
    right := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([255, 10]))
    empty := 0
    softOverlap := 0
    hardOverlap := 0
    overlayOverlap := 0
    softEmpty := 0
    hardEmpty := 0
    overlayEmpty := 0
    try {
        empty := left.Crop([1, 0, 1, 1])
        softOverlap := Pillow.ImageChops.SoftLight(left, right)
        hardOverlap := Pillow.ImageChops.HardLight(left, right)
        overlayOverlap := Pillow.ImageChops.Overlay(left, right)
        softEmpty := Pillow.ImageChops.SoftLight(empty, left)
        hardEmpty := Pillow.ImageChops.HardLight(empty, left)
        overlayEmpty := Pillow.ImageChops.Overlay(empty, left)

        AhkTest.AssertEqual([2, 1], softOverlap.Size)
        AhkTest.AssertEqual([0, 8], PillowTestBufferToArray(softOverlap.ToBytes()))
        AhkTest.AssertEqual([255, 3], PillowTestBufferToArray(hardOverlap.ToBytes()))
        AhkTest.AssertEqual([0, 3], PillowTestBufferToArray(overlayOverlap.ToBytes()))
        AhkTest.AssertEqual([0, 1], softEmpty.Size)
        AhkTest.AssertEqual([], PillowTestBufferToArray(softEmpty.ToBytes()))
        AhkTest.AssertEqual([], PillowTestBufferToArray(hardEmpty.ToBytes()))
        AhkTest.AssertEqual([], PillowTestBufferToArray(overlayEmpty.ToBytes()))
    } finally {
        for image in [overlayEmpty, hardEmpty, softEmpty, overlayOverlap, hardOverlap, softOverlap, empty, right, left] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops soft hard overlay return overlapping and empty output sizes", PillowTestImageChopsSoftHardOverlayUseOverlappingAndEmptyOutputSize)

PillowTestImageChopsSoftHardOverlayRejectModeMismatch(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [1, 1], PillowTestBuffer([1]))
    right := Pillow.Image.FromBytes("RGB", [1, 1], PillowTestBuffer([1, 2, 3]))
    try {
        for methodName in ["SoftLight", "HardLight", "Overlay"] {
            try {
                Pillow.ImageChops.%methodName%(left, right)
                AhkTest.Fail("Expected ImageChops." methodName " to reject mode mismatch")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, "mismatch") > 0)
            }
        }
    } finally {
        right.Close()
        left.Close()
    }
}

AhkTest.Test("Pillow ImageChops soft hard overlay reject mode mismatch", PillowTestImageChopsSoftHardOverlayRejectModeMismatch)

PillowTestImageChopsOffsetUsesNativeHandle(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    out := 0
    try {
        out := Pillow.ImageChops.Offset(image, 1)

        AhkTest.AssertEqual("L", out.Mode)
        AhkTest.AssertEqual([3, 2], out.Size)
        AhkTest.AssertEqual([6, 4, 5, 3, 1, 2], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        for item in [out, image] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops.Offset wraps pixels through native handles", PillowTestImageChopsOffsetUsesNativeHandle)

PillowTestImageChopsOffsetAcceptsExplicitYOffsetAndLargeNegativeOffsets(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("RGB", [2, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]))
    out := 0
    try {
        out := Pillow.ImageChops.Offset(image, -4, 3)

        AhkTest.AssertEqual("RGB", out.Mode)
        AhkTest.AssertEqual([2, 2], out.Size)
        AhkTest.AssertEqual([7, 8, 9, 10, 11, 12, 1, 2, 3, 4, 5, 6], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        for item in [out, image] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops.Offset accepts explicit yoffset and large negative offsets", PillowTestImageChopsOffsetAcceptsExplicitYOffsetAndLargeNegativeOffsets)

PillowTestImageChopsOffsetHandlesEmptyImagesSafely(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [3, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    empty := 0
    out := 0
    try {
        empty := image.Crop([1, 0, 1, 2])
        out := Pillow.ImageChops.Offset(empty, 1, -1)

        AhkTest.AssertEqual("L", out.Mode)
        AhkTest.AssertEqual([0, 2], out.Size)
        AhkTest.AssertEqual([], PillowTestBufferToArray(out.ToBytes()))
    } finally {
        for item in [out, empty, image] {
            if IsObject(item)
                item.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops.Offset handles empty images safely", PillowTestImageChopsOffsetHandlesEmptyImagesSafely)

PillowTestImageChopsOffsetRejectsInvalidOffsets(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    image := Pillow.Image.FromBytes("L", [1, 1], PillowTestBuffer([1]))
    try {
        for args in [["x"], [1, "y"]] {
            try {
                if args.Length = 1
                    Pillow.ImageChops.Offset(image, args[1])
                else
                    Pillow.ImageChops.Offset(image, args[1], args[2])
                AhkTest.Fail("Expected ImageChops.Offset to reject invalid offsets")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, "offset") > 0)
            }
        }
    } finally {
        image.Close()
    }
}

AhkTest.Test("Pillow ImageChops.Offset rejects invalid offsets", PillowTestImageChopsOffsetRejectsInvalidOffsets)

PillowTestImageChopsAddAndSubtractUseNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 50, 200, 255, 0, 80]))
    right := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([4, 20, 100, 15, 200, 90]))
    addOut := 0
    subOut := 0
    try {
        addOut := Pillow.ImageChops.Add(left, right)
        subOut := Pillow.ImageChops.Subtract(left, right)

        AhkTest.AssertEqual("RGB", addOut.Mode)
        AhkTest.AssertEqual([2, 1], addOut.Size)
        AhkTest.AssertEqual([5, 70, 255, 255, 200, 170], PillowTestBufferToArray(addOut.ToBytes()))
        AhkTest.AssertEqual([0, 30, 100, 240, 0, 0], PillowTestBufferToArray(subOut.ToBytes()))
    } finally {
        for image in [subOut, addOut, right, left] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops add and subtract compute clipped pixels through native handles", PillowTestImageChopsAddAndSubtractUseNativeHandles)

PillowTestImageChopsAddAndSubtractAcceptScaleAndOffset(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 40, 200, 255]))
    right := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([255, 10, 220, 128]))
    addOut := 0
    subOut := 0
    try {
        addOut := Pillow.ImageChops.Add(left, right, 2.0, 10.0)
        subOut := Pillow.ImageChops.Subtract(left, right, 2.0, 10.0)

        AhkTest.AssertEqual([137, 35, 220, 201], PillowTestBufferToArray(addOut.ToBytes()))
        AhkTest.AssertEqual([0, 25, 0, 73], PillowTestBufferToArray(subOut.ToBytes()))
    } finally {
        for image in [subOut, addOut, right, left] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops add and subtract accept scale and offset", PillowTestImageChopsAddAndSubtractAcceptScaleAndOffset)

PillowTestImageChopsAddAndSubtractUseOverlappingAndEmptyOutputSize(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 40, 200, 255]))
    right := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([255, 10]))
    empty := 0
    addOverlap := 0
    subOverlap := 0
    addEmpty := 0
    subEmpty := 0
    try {
        empty := left.Crop([1, 0, 1, 1])
        addOverlap := Pillow.ImageChops.Add(left, right)
        subOverlap := Pillow.ImageChops.Subtract(left, right)
        addEmpty := Pillow.ImageChops.Add(empty, left)
        subEmpty := Pillow.ImageChops.Subtract(empty, left)

        AhkTest.AssertEqual([2, 1], addOverlap.Size)
        AhkTest.AssertEqual([255, 50], PillowTestBufferToArray(addOverlap.ToBytes()))
        AhkTest.AssertEqual([0, 30], PillowTestBufferToArray(subOverlap.ToBytes()))
        AhkTest.AssertEqual([0, 1], addEmpty.Size)
        AhkTest.AssertEqual([], PillowTestBufferToArray(addEmpty.ToBytes()))
        AhkTest.AssertEqual([], PillowTestBufferToArray(subEmpty.ToBytes()))
    } finally {
        for image in [subEmpty, addEmpty, subOverlap, addOverlap, empty, right, left] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops add and subtract return overlapping and empty output sizes", PillowTestImageChopsAddAndSubtractUseOverlappingAndEmptyOutputSize)

PillowTestImageChopsAddAndSubtractRejectModeMismatch(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [1, 1], PillowTestBuffer([1]))
    right := Pillow.Image.FromBytes("RGB", [1, 1], PillowTestBuffer([1, 2, 3]))
    try {
        try {
            Pillow.ImageChops.Add(left, right)
            AhkTest.Fail("Expected ImageChops.Add to reject mode mismatch")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "mismatch") > 0)
        }

        try {
            Pillow.ImageChops.Subtract(left, right)
            AhkTest.Fail("Expected ImageChops.Subtract to reject mode mismatch")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "mismatch") > 0)
        }
    } finally {
        right.Close()
        left.Close()
    }
}

AhkTest.Test("Pillow ImageChops add and subtract reject mode mismatch", PillowTestImageChopsAddAndSubtractRejectModeMismatch)

PillowTestImageChopsModuloOpsUseNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 50, 200, 255, 0, 80]))
    right := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([4, 20, 100, 15, 200, 90]))
    addOut := 0
    subOut := 0
    try {
        addOut := Pillow.ImageChops.AddModulo(left, right)
        subOut := Pillow.ImageChops.SubtractModulo(left, right)

        AhkTest.AssertEqual("RGB", addOut.Mode)
        AhkTest.AssertEqual([2, 1], addOut.Size)
        AhkTest.AssertEqual([5, 70, 44, 14, 200, 170], PillowTestBufferToArray(addOut.ToBytes()))
        AhkTest.AssertEqual([253, 30, 100, 240, 56, 246], PillowTestBufferToArray(subOut.ToBytes()))
    } finally {
        for image in [subOut, addOut, right, left] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops modulo ops compute wrapped pixels through native handles", PillowTestImageChopsModuloOpsUseNativeHandles)

PillowTestImageChopsModuloOpsUseOverlappingAndEmptyOutputSize(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [4, 1], PillowTestBuffer([0, 40, 200, 255]))
    right := Pillow.Image.FromBytes("L", [2, 1], PillowTestBuffer([255, 10]))
    empty := 0
    addOverlap := 0
    subOverlap := 0
    addEmpty := 0
    subEmpty := 0
    try {
        empty := left.Crop([1, 0, 1, 1])
        addOverlap := Pillow.ImageChops.AddModulo(left, right)
        subOverlap := Pillow.ImageChops.SubtractModulo(left, right)
        addEmpty := Pillow.ImageChops.AddModulo(empty, left)
        subEmpty := Pillow.ImageChops.SubtractModulo(empty, left)

        AhkTest.AssertEqual([2, 1], addOverlap.Size)
        AhkTest.AssertEqual([255, 50], PillowTestBufferToArray(addOverlap.ToBytes()))
        AhkTest.AssertEqual([1, 30], PillowTestBufferToArray(subOverlap.ToBytes()))
        AhkTest.AssertEqual([0, 1], addEmpty.Size)
        AhkTest.AssertEqual([], PillowTestBufferToArray(addEmpty.ToBytes()))
        AhkTest.AssertEqual([], PillowTestBufferToArray(subEmpty.ToBytes()))
    } finally {
        for image in [subEmpty, addEmpty, subOverlap, addOverlap, empty, right, left] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops modulo ops return overlapping and empty output sizes", PillowTestImageChopsModuloOpsUseOverlappingAndEmptyOutputSize)

PillowTestImageChopsModuloOpsRejectModeMismatch(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [1, 1], PillowTestBuffer([1]))
    right := Pillow.Image.FromBytes("RGB", [1, 1], PillowTestBuffer([1, 2, 3]))
    try {
        try {
            Pillow.ImageChops.AddModulo(left, right)
            AhkTest.Fail("Expected ImageChops.AddModulo to reject mode mismatch")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "mismatch") > 0)
        }

        try {
            Pillow.ImageChops.SubtractModulo(left, right)
            AhkTest.Fail("Expected ImageChops.SubtractModulo to reject mode mismatch")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "mismatch") > 0)
        }
    } finally {
        right.Close()
        left.Close()
    }
}

AhkTest.Test("Pillow ImageChops modulo ops reject mode mismatch", PillowTestImageChopsModuloOpsRejectModeMismatch)

PillowTestImageChopsBinaryOpsSupportLaAndCmyk(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    la1 := Pillow.Image.FromBytes("LA", [2, 1], PillowTestBuffer([10, 20, 200, 220]))
    la2 := Pillow.Image.FromBytes("LA", [2, 1], PillowTestBuffer([30, 40, 150, 160]))
    cmyk1 := Pillow.Image.FromBytes("CMYK", [2, 1], PillowTestBuffer([10, 20, 30, 40, 200, 210, 220, 230]))
    cmyk2 := Pillow.Image.FromBytes("CMYK", [2, 1], PillowTestBuffer([30, 40, 50, 60, 150, 160, 170, 180]))
    laDiff := 0
    laMultiply := 0
    laScreen := 0
    laLight := 0
    laDark := 0
    laAdd := 0
    laSub := 0
    laAddModulo := 0
    laSubModulo := 0
    laSoft := 0
    laHard := 0
    laOverlay := 0
    cmykDiff := 0
    cmykMultiply := 0
    cmykScreen := 0
    cmykLight := 0
    cmykDark := 0
    cmykAdd := 0
    cmykSub := 0
    cmykAddModulo := 0
    cmykSubModulo := 0
    cmykSoft := 0
    cmykHard := 0
    cmykOverlay := 0
    try {
        laDiff := Pillow.ImageChops.Difference(la1, la2)
        laMultiply := Pillow.ImageChops.Multiply(la1, la2)
        laScreen := Pillow.ImageChops.Screen(la1, la2)
        laLight := Pillow.ImageChops.Lighter(la1, la2)
        laDark := Pillow.ImageChops.Darker(la1, la2)
        laAdd := Pillow.ImageChops.Add(la1, la2)
        laSub := Pillow.ImageChops.Subtract(la1, la2)
        laAddModulo := Pillow.ImageChops.AddModulo(la1, la2)
        laSubModulo := Pillow.ImageChops.SubtractModulo(la1, la2)
        laSoft := Pillow.ImageChops.SoftLight(la1, la2)
        laHard := Pillow.ImageChops.HardLight(la1, la2)
        laOverlay := Pillow.ImageChops.Overlay(la1, la2)
        cmykDiff := Pillow.ImageChops.Difference(cmyk1, cmyk2)
        cmykMultiply := Pillow.ImageChops.Multiply(cmyk1, cmyk2)
        cmykScreen := Pillow.ImageChops.Screen(cmyk1, cmyk2)
        cmykLight := Pillow.ImageChops.Lighter(cmyk1, cmyk2)
        cmykDark := Pillow.ImageChops.Darker(cmyk1, cmyk2)
        cmykAdd := Pillow.ImageChops.Add(cmyk1, cmyk2)
        cmykSub := Pillow.ImageChops.Subtract(cmyk1, cmyk2)
        cmykAddModulo := Pillow.ImageChops.AddModulo(cmyk1, cmyk2)
        cmykSubModulo := Pillow.ImageChops.SubtractModulo(cmyk1, cmyk2)
        cmykSoft := Pillow.ImageChops.SoftLight(cmyk1, cmyk2)
        cmykHard := Pillow.ImageChops.HardLight(cmyk1, cmyk2)
        cmykOverlay := Pillow.ImageChops.Overlay(cmyk1, cmyk2)

        AhkTest.AssertEqual("LA", laDiff.Mode)
        AhkTest.AssertEqual([20, 20, 50, 60], PillowTestBufferToArray(laDiff.ToBytes()))
        AhkTest.AssertEqual([1, 3, 117, 138], PillowTestBufferToArray(laMultiply.ToBytes()))
        AhkTest.AssertEqual([39, 57, 233, 242], PillowTestBufferToArray(laScreen.ToBytes()))
        AhkTest.AssertEqual([30, 40, 200, 220], PillowTestBufferToArray(laLight.ToBytes()))
        AhkTest.AssertEqual([10, 20, 150, 160], PillowTestBufferToArray(laDark.ToBytes()))
        AhkTest.AssertEqual([40, 60, 255, 255], PillowTestBufferToArray(laAdd.ToBytes()))
        AhkTest.AssertEqual([0, 0, 50, 60], PillowTestBufferToArray(laSub.ToBytes()))
        AhkTest.AssertEqual([40, 60, 94, 124], PillowTestBufferToArray(laAddModulo.ToBytes()))
        AhkTest.AssertEqual([236, 236, 50, 60], PillowTestBufferToArray(laSubModulo.ToBytes()))
        AhkTest.AssertEqual([2, 6, 207, 226], PillowTestBufferToArray(laSoft.ToBytes()))
        AhkTest.AssertEqual([2, 6, 210, 229], PillowTestBufferToArray(laHard.ToBytes()))
        AhkTest.AssertEqual([2, 6, 210, 229], PillowTestBufferToArray(laOverlay.ToBytes()))

        AhkTest.AssertEqual("CMYK", cmykDiff.Mode)
        AhkTest.AssertEqual([20, 20, 20, 20, 50, 50, 50, 50], PillowTestBufferToArray(cmykDiff.ToBytes()))
        AhkTest.AssertEqual([1, 3, 5, 9, 117, 131, 146, 162], PillowTestBufferToArray(cmykMultiply.ToBytes()))
        AhkTest.AssertEqual([39, 57, 75, 91, 233, 239, 244, 248], PillowTestBufferToArray(cmykScreen.ToBytes()))
        AhkTest.AssertEqual([30, 40, 50, 60, 200, 210, 220, 230], PillowTestBufferToArray(cmykLight.ToBytes()))
        AhkTest.AssertEqual([10, 20, 30, 40, 150, 160, 170, 180], PillowTestBufferToArray(cmykDark.ToBytes()))
        AhkTest.AssertEqual([40, 60, 80, 100, 255, 255, 255, 255], PillowTestBufferToArray(cmykAdd.ToBytes()))
        AhkTest.AssertEqual([0, 0, 0, 0, 50, 50, 50, 50], PillowTestBufferToArray(cmykSub.ToBytes()))
        AhkTest.AssertEqual([40, 60, 80, 100, 94, 114, 134, 154], PillowTestBufferToArray(cmykAddModulo.ToBytes()))
        AhkTest.AssertEqual([236, 236, 236, 236, 50, 50, 50, 50], PillowTestBufferToArray(cmykSubModulo.ToBytes()))
        AhkTest.AssertEqual([2, 6, 13, 21, 207, 219, 229, 238], PillowTestBufferToArray(cmykSoft.ToBytes()))
        AhkTest.AssertEqual([2, 6, 11, 18, 210, 222, 232, 241], PillowTestBufferToArray(cmykHard.ToBytes()))
        AhkTest.AssertEqual([2, 6, 11, 18, 210, 222, 232, 241], PillowTestBufferToArray(cmykOverlay.ToBytes()))
    } finally {
        for image in [
            cmykOverlay, cmykHard, cmykSoft, cmykSubModulo, cmykAddModulo, cmykSub,
            cmykAdd, cmykDark, cmykLight, cmykScreen, cmykMultiply, cmykDiff,
            laOverlay, laHard, laSoft, laSubModulo, laAddModulo, laSub, laAdd,
            laDark, laLight, laScreen, laMultiply, laDiff, cmyk2, cmyk1, la2, la1
        ] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops binary ops support LA and CMYK", PillowTestImageChopsBinaryOpsSupportLaAndCmyk)

PillowTestImageChopsLogicalOpsUseModeOneNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("1", [4, 1], PillowTestBuffer([0x60]))
    right := Pillow.Image.FromBytes("1", [4, 1], PillowTestBuffer([0x30]))
    andOut := 0
    orOut := 0
    xorOut := 0
    try {
        andOut := Pillow.ImageChops.LogicalAnd(left, right)
        orOut := Pillow.ImageChops.LogicalOr(left, right)
        xorOut := Pillow.ImageChops.LogicalXor(left, right)

        AhkTest.AssertEqual("1", andOut.Mode)
        AhkTest.AssertEqual([4, 1], andOut.Size)
        AhkTest.AssertEqual([0x20], PillowTestBufferToArray(andOut.ToBytes()))
        AhkTest.AssertEqual([0, 0, 255, 0], andOut.GetData())
        AhkTest.AssertEqual([0x70], PillowTestBufferToArray(orOut.ToBytes()))
        AhkTest.AssertEqual([0, 255, 255, 255], orOut.GetData())
        AhkTest.AssertEqual([0x50], PillowTestBufferToArray(xorOut.ToBytes()))
        AhkTest.AssertEqual([0, 255, 0, 255], xorOut.GetData())
    } finally {
        for image in [xorOut, orOut, andOut, right, left] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops logical ops compute mode 1 pixels through native handles", PillowTestImageChopsLogicalOpsUseModeOneNativeHandles)

PillowTestImageChopsLogicalOpsUseOverlappingAndEmptyOutputSize(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("1", [4, 1], PillowTestBuffer([0x60]))
    right := Pillow.Image.FromBytes("1", [2, 1], PillowTestBuffer([0xC0]))
    empty := 0
    overlap := 0
    emptyOut := 0
    try {
        empty := left.Crop([1, 0, 1, 1])
        overlap := Pillow.ImageChops.LogicalAnd(left, right)
        emptyOut := Pillow.ImageChops.LogicalAnd(empty, left)

        AhkTest.AssertEqual([2, 1], overlap.Size)
        AhkTest.AssertEqual([0x40], PillowTestBufferToArray(overlap.ToBytes()))
        AhkTest.AssertEqual([0, 255], overlap.GetData())
        AhkTest.AssertEqual([0, 1], emptyOut.Size)
        AhkTest.AssertEqual([], PillowTestBufferToArray(emptyOut.ToBytes()))
        AhkTest.AssertEqual([], emptyOut.GetData())
    } finally {
        for image in [emptyOut, overlap, empty, right, left] {
            if IsObject(image)
                image.Close()
        }
    }
}

AhkTest.Test("Pillow ImageChops logical ops return overlapping and empty output sizes", PillowTestImageChopsLogicalOpsUseOverlappingAndEmptyOutputSize)

PillowTestImageChopsLogicalOpsRejectNonModeOne(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    left := Pillow.Image.FromBytes("L", [1, 1], PillowTestBuffer([1]))
    right := Pillow.Image.FromBytes("L", [1, 1], PillowTestBuffer([1]))
    try {
        for methodName in ["LogicalAnd", "LogicalOr", "LogicalXor"] {
            try {
                Pillow.ImageChops.%methodName%(left, right)
                AhkTest.Fail("Expected ImageChops." methodName " to reject non mode 1 images")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0)
            }
        }
    } finally {
        right.Close()
        left.Close()
    }
}

AhkTest.Test("Pillow ImageChops logical ops reject non mode 1 images", PillowTestImageChopsLogicalOpsRejectNonModeOne)

PillowTestImageAlphaCompositeStaticUsesNativeHandles(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    dst := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([
        10, 20, 30, 255,
        100, 110, 120, 128,
    ]))
    src := Pillow.Image.FromBytes("RGBA", [2, 1], PillowTestBuffer([
        200, 210, 220, 255,
        50, 60, 70, 128,
    ]))
    composited := 0
    try {
        composited := Pillow.Image.AlphaComposite(dst, src)
        AhkTest.AssertEqual("RGBA", composited.Mode)
        AhkTest.AssertEqual([2, 1], composited.Size)
        AhkTest.AssertEqual([200, 210, 220, 255, 67, 77, 87, 192], PillowTestBufferToArray(composited.ToBytes()))
    } finally {
        if IsObject(composited)
            composited.Close()
        src.Close()
        dst.Close()
    }
}

AhkTest.Test("Pillow Image.AlphaComposite composites RGBA images through native handles", PillowTestImageAlphaCompositeStaticUsesNativeHandles)

PillowTestImageAlphaCompositeMutatesThroughNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    dst := Pillow.Image.FromBytes("RGBA", [4, 3], PillowTestBuffer([
        10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255,
        10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255,
        10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255,
    ]))
    src := Pillow.Image.FromBytes("RGBA", [3, 2], PillowTestBuffer([
        100, 0, 0, 128, 0, 100, 0, 64, 0, 0, 100, 255,
        200, 50, 0, 128, 0, 200, 50, 128, 50, 0, 200, 0,
    ]))
    try {
        returned := dst.AlphaComposite(src, [1, 1])
        AhkTest.AssertEqual(dst.Handle, returned.Handle)
        AhkTest.AssertEqual([
            10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255,
            10, 20, 30, 255, 55, 10, 15, 255, 7, 40, 22, 255, 0, 0, 100, 255,
            10, 20, 30, 255, 105, 35, 15, 255, 5, 110, 40, 255, 10, 20, 30, 255,
        ], PillowTestBufferToArray(dst.ToBytes()))

        errorWasRaised := false
        try {
            dst.AlphaComposite(src, [0, 0], [-1, 0])
        } catch Error as err {
            errorWasRaised := InStr(err.Message, "invalid argument") > 0 || InStr(err.Message, "Source must be non-negative") > 0
        }
        AhkTest.AssertTrue(errorWasRaised)
    } finally {
        src.Close()
        dst.Close()
    }
}

AhkTest.Test("Pillow Image.AlphaComposite mutates through native path", PillowTestImageAlphaCompositeMutatesThroughNativePath)

PillowTestColor3DLutIdentityTable3() {
    table := []
    loop 2 {
        b := A_Index - 1
        loop 2 {
            g := A_Index - 1
            loop 2 {
                r := A_Index - 1
                table.Push(r)
                table.Push(g)
                table.Push(b)
            }
        }
    }
    return table
}

PillowTestColor3DLutTupleTable4() {
    table := []
    loop 2 {
        b := A_Index - 1
        loop 2 {
            g := A_Index - 1
            loop 2 {
                r := A_Index - 1
                table.Push([1 - r, g * 0.5, b * 0.25, (r + g + b) / 3.0])
            }
        }
    }
    return table
}

PillowTestImageFilterColor3DLUTUsesNativePath(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    rgb := Pillow.Image.FromBytes("RGB", [3, 2], PillowTestBuffer([
        0, 0, 0, 127, 64, 32, 255, 255, 255,
        32, 200, 128, 240, 10, 80, 90, 120, 250,
    ]))
    rgba := Pillow.Image.FromBytes("RGBA", [2, 2], PillowTestBuffer([
        0, 0, 0, 10, 128, 64, 32, 40,
        255, 255, 255, 90, 32, 200, 128, 160,
    ]))
    outputs := []
    try {
        identity := Pillow.ImageFilter.Color3DLUT(2, PillowTestColor3DLutIdentityTable3())
        table4 := Pillow.ImageFilter.Color3DLUT([2, 2, 2], PillowTestColor3DLutTupleTable4(), 4, "RGBA")
        rgb3 := rgb.Filter(identity)
        rgba3 := rgba.Filter(identity)
        rgba4 := rgba.Filter(table4)
        rgbToRgba4 := rgb.Filter(table4)
        rgbaToRgb3 := rgba.Filter(Pillow.ImageFilter.Color3DLUT(2, PillowTestColor3DLutIdentityTable3(), 3, "RGB"))
        outputs.Push(rgb3)
        outputs.Push(rgba3)
        outputs.Push(rgba4)
        outputs.Push(rgbToRgba4)
        outputs.Push(rgbaToRgb3)

        AhkTest.AssertEqual("Color 3D LUT", identity.Name)
        AhkTest.AssertEqual([2, 2, 2], identity.Size)
        AhkTest.AssertEqual(3, identity.Channels)
        AhkTest.AssertEqual("", identity.TargetMode)
        AhkTest.AssertEqual("RGB", rgb3.Mode)
        AhkTest.AssertEqual(PillowTestBufferToArray(rgb.ToBytes()), PillowTestBufferToArray(rgb3.ToBytes()))
        AhkTest.AssertEqual("RGBA", rgba3.Mode)
        AhkTest.AssertEqual(PillowTestBufferToArray(rgba.ToBytes()), PillowTestBufferToArray(rgba3.ToBytes()))
        AhkTest.AssertEqual([255, 0, 0, 0, 127, 32, 8, 75, 0, 127, 64, 255, 223, 100, 32, 120], PillowTestBufferToArray(rgba4.ToBytes()))
        AhkTest.AssertEqual([255, 0, 0, 0, 128, 32, 8, 74, 0, 127, 64, 255, 223, 100, 32, 120, 15, 5, 20, 110, 165, 60, 62, 153], PillowTestBufferToArray(rgbToRgba4.ToBytes()))
        AhkTest.AssertEqual("RGB", rgbaToRgb3.Mode)
        AhkTest.AssertEqual([0, 0, 0, 128, 64, 32, 255, 255, 255, 32, 200, 128], PillowTestBufferToArray(rgbaToRgb3.ToBytes()))
    } finally {
        for image in outputs {
            if IsObject(image)
                image.Close()
        }
        rgba.Close()
        rgb.Close()
    }
}

AhkTest.Test("Pillow ImageFilter.Color3DLUT uses native path", PillowTestImageFilterColor3DLUTUsesNativePath)

PillowTestImageFilterColor3DLUTRejectsInvalidArguments(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.New("L", [1, 1])
    try {
        cases := [
            (*) => Pillow.ImageFilter.Color3DLUT([1], [0, 0, 0], 3),
            (*) => Pillow.ImageFilter.Color3DLUT(2, [0, 0, 0], 1),
            (*) => Pillow.ImageFilter.Color3DLUT(2, [[0, 0]], 3),
            (*) => Pillow.ImageFilter.Color3DLUT(2, [0, 0, 0], 3),
        ]
        for factory in cases {
            try {
                factory.Call()
                AhkTest.Fail("Expected Color3DLUT constructor to reject invalid input")
            } catch Error as err {
                AhkTest.AssertTrue(InStr(err.Message, "Color3DLUT") > 0 || InStr(err.Message, "table") > 0 || InStr(err.Message, "Size") > 0)
            }
        }

        try {
            source.Filter(Pillow.ImageFilter.Color3DLUT(2, PillowTestColor3DLutIdentityTable3()))
            AhkTest.Fail("Expected Color3DLUT to reject L source image")
        } catch Error as err {
            AhkTest.AssertTrue(InStr(err.Message, "invalid argument") > 0 || InStr(err.Message, "wrong mode") > 0)
        }
    } finally {
        source.Close()
    }
}

AhkTest.Test("Pillow ImageFilter.Color3DLUT rejects invalid arguments", PillowTestImageFilterColor3DLUTRejectsInvalidArguments)
