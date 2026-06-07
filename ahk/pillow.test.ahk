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
