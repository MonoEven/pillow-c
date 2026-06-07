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
    try {
        AhkTest.AssertEqual([
            10, 20, 30, 10, 20, 30, 10, 20, 30,
            10, 20, 30, 10, 20, 30, 10, 20, 30,
        ], PillowTestBufferToArray(rgb.ToBytes()))
        AhkTest.AssertEqual([7, 7, 7, 7], PillowTestBufferToArray(l.ToBytes()))
    } finally {
        l.Close()
        rgb.Close()
    }
}

AhkTest.Test("Pillow Image.New fills a solid color through the native DLL", PillowTestImageNewWithColorUsesNativeFill)

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
    resized := 0
    try {
        resized := source.Resize([3, 3], Pillow.Resampling.BILINEAR)
        AhkTest.AssertEqual("L", resized.Mode)
        AhkTest.AssertEqual([3, 3], resized.Size)
        AhkTest.AssertEqual([
            0, 32, 64,
            64, 112, 160,
            128, 192, 255,
        ], PillowTestBufferToArray(resized.ToBytes()))
    } finally {
        if IsObject(resized)
            resized.Close()
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

PillowTestMakeInvertLut(channelCount) {
    lut := []
    loop channelCount {
        loop 256
            lut.Push(256 - A_Index)
    }
    return lut
}

PillowTestImagePointUsesNativeLutOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    source := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([0, 1, 2, 10, 20, 30]))
    pointed := 0
    try {
        pointed := source.Point(PillowTestMakeInvertLut(3))
        AhkTest.AssertEqual("RGB", pointed.Mode)
        AhkTest.AssertEqual([2, 1], pointed.Size)
        AhkTest.AssertEqual([255, 254, 253, 245, 235, 225], PillowTestBufferToArray(pointed.ToBytes()))
    } finally {
        if IsObject(pointed)
            pointed.Close()
        source.Close()
    }
}

AhkTest.Test("Pillow Image.Point applies a Pillow-style LUT through native handles", PillowTestImagePointUsesNativeLutOperation)

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
