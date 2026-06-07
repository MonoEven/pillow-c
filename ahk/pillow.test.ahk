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

PillowTestImageGetAndPutPixelUseNativeOperation(*) {
    Pillow.Configure({ DllPath: PillowTestDllPath() })
    l := Pillow.Image.FromBytes("L", [2, 2], PillowTestBuffer([1, 2, 3, 4]))
    rgb := Pillow.Image.FromBytes("RGB", [2, 1], PillowTestBuffer([1, 2, 3, 4, 5, 6]))
    rgba := Pillow.Image.FromBytes("RGBA", [1, 2], PillowTestBuffer([1, 2, 3, 4, 5, 6, 7, 8]))
    try {
        AhkTest.AssertEqual(1, l.GetPixel([0, 0]))
        AhkTest.AssertEqual(2, l.GetPixel([-1, 0]))
        AhkTest.AssertEqual([4, 5, 6], rgb.GetPixel([1, 0]))
        AhkTest.AssertEqual([1, 2, 3, 4], rgba.GetPixel([-1, 0]))

        l.PutPixel([0, 0], 9)
        rgb.PutPixel([0, 0], 9)
        rgba.PutPixel([0, 0], [9, 9, 9, 9])

        AhkTest.AssertEqual([9, 2, 3, 4], PillowTestBufferToArray(l.ToBytes()))
        AhkTest.AssertEqual([9, 0, 0, 4, 5, 6], PillowTestBufferToArray(rgb.ToBytes()))
        AhkTest.AssertEqual([9, 9, 9, 9, 5, 6, 7, 8], PillowTestBufferToArray(rgba.ToBytes()))
    } finally {
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
