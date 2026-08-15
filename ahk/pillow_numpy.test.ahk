#Requires AutoHotkey v2.0
#Include <stdlib\ahktest>
#Include "pillow.ahk"
#Include "..\..\2026-07-19-cnumpy-foundation\ahk\numpy.ahk"

; =============================================================================
; Pillow + cnumpy interop tests (BEHAV-NUMPY-001 / API-NUMPY-001).
; Oracle: F:\Python\Python310\python.exe, Pillow 11.3.0 + NumPy 1.25.0,
; pins recorded in oracle/probe_numpy_interop.py and
; oracle/numpy_interop_pins.txt.
; cnumpy: https://github.com/MonoEven/cnumpy (v1.21.0-cnumpy).
; =============================================================================

PillowTestNumpyDllPath() {
    SplitPath A_LineFile, , &ahkDir
    return ahkDir "\..\build\x64\Release\pillow_c.dll"
}

PillowTestNumpyBuf(values) {
    buf := Buffer(values.Length, 0)
    for index, value in values
        NumPut("UChar", value, buf, index - 1)
    return buf
}

PillowTestNumpyBufHex(buf) {
    hex := ""
    loop buf.Size
        hex .= Format("{:02x}", NumGet(buf, A_Index - 1, "UChar"))
    return hex
}

PillowTestNumpyArrayBytesHex(arr) {
    return PillowTestNumpyBufHex(Numpy.ToBytes(arr))
}

PillowTestNumpyFormatCaptureError(callable) {
    try {
        callable.Call()
    } catch Error as err {
        return err.Message
    }
    return ""
}

; ---------------------------------------------------------------------------
; numpy.asarray(im) analogue: Pillow.Image.AsArray(image) / image.AsArray()
; ---------------------------------------------------------------------------

PillowTestNumpyAsArrayModes() {
    Pillow.Configure({ DllPath: PillowTestNumpyDllPath() })
    cases := Map(
        "L",    [[3, 2], [1, 2, 3, 4, 5, 6],                                            3, [2, 3]],
        "P",    [[3, 2], [1, 2, 3, 4, 5, 6],                                            3, [2, 3]],
        "RGB",  [[3, 2], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18], 3, [2, 3, 3]],
        "RGBA", [[3, 2], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24], 3, [2, 3, 4]],
        "CMYK", [[3, 2], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24], 3, [2, 3, 4]],
        "YCbCr",[[3, 2], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18], 3, [2, 3, 3]],
        "LAB",  [[3, 2], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18], 3, [2, 3, 3]],
        "HSV",  [[3, 2], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18], 3, [2, 3, 3]],
        "LA",   [[3, 2], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],                       3, [2, 3, 2]],
        "I",    [[2, 2], [0xFE, 0xFF, 0xFF, 0xFF, 3, 0, 0, 0, 4, 0, 0, 0, 0xFB, 0xFF, 0xFF, 0xFF], 6, [2, 2]],
        "F",    [[2, 2], [0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0xA0, 0xBF, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x60, 0x40], 12, [2, 2]],
        "I;16", [[2, 2], [1, 0, 2, 0, 3, 0, 4, 0],                                        5, [2, 2]],
        "1",    [[3, 2], [0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00],                            1, [2, 3]]
    )
    for mode, spec in cases {
        size := spec[1]
        bytes := spec[2]
        expectedDtype := spec[3]
        expectedShape := spec[4]
        image := Pillow.Image.FromBuffer(mode, size, PillowTestNumpyBuf(bytes), "raw", mode = "1" ? "1;8" : mode)
        try {
            arr := image.AsArray()
            AhkTest.AssertEqual(expectedDtype, arr.Dtype, mode " dtype")
            AhkTest.AssertEqual(expectedShape, arr.Shape, mode " shape")
            AhkTest.AssertEqual(PillowTestNumpyBufHex(image.InternalBytes()),
                PillowTestNumpyArrayBytesHex(arr), mode " bytes")
            arr2 := Pillow.Image.AsArray(image)
            AhkTest.AssertEqual(expectedShape, arr2.Shape, mode " static shape")
        } finally {
            image.Close()
        }
    }
}

PillowTestNumpyAsArrayPIndicesOnly() {
    Pillow.Configure({ DllPath: PillowTestNumpyDllPath() })
    image := Pillow.Image.FromBuffer("P", [2, 2], PillowTestNumpyBuf([1, 2, 3, 4]), "raw", "P")
    try {
        image.PutPalette([255, 0, 0])
        arr := image.AsArray()
        AhkTest.AssertEqual(3, arr.Dtype)
        AhkTest.AssertEqual([2, 2], arr.Shape)
        AhkTest.AssertEqual("01020304", PillowTestNumpyArrayBytesHex(arr))
    } finally {
        image.Close()
    }
}

PillowTestNumpyAsArrayI16BBoundary() {
    Pillow.Configure({ DllPath: PillowTestNumpyDllPath() })
    image := Pillow.Image.FromBuffer("I;16B", [2, 2], PillowTestNumpyBuf([0, 1, 0, 2, 0, 3, 0, 4]), "raw", "I;16B")
    try {
        message := PillowTestNumpyFormatCaptureError(() => image.AsArray())
        AhkTest.AssertEqual("Pillow.Image.AsArray: I;16B big-endian dtype (numpy '>u2') is not representable in cnumpy (documented boundary)", message)
    } finally {
        image.Close()
    }
}

PillowTestNumpyAsArraySurvivesImageClose() {
    Pillow.Configure({ DllPath: PillowTestNumpyDllPath() })
    image := Pillow.Image.FromBuffer("L", [2, 2], PillowTestNumpyBuf([10, 20, 30, 40]), "raw", "L")
    arr := image.AsArray()
    image.Close()
    AhkTest.AssertEqual([2, 2], arr.Shape)
    AhkTest.AssertEqual(20.0, arr.GetItem(1))
    AhkTest.AssertEqual("0a141e28", PillowTestNumpyArrayBytesHex(arr))
}

; ---------------------------------------------------------------------------
; Image.fromarray analogue: Pillow.Image.FromArray(obj, mode)
; ---------------------------------------------------------------------------

PillowTestNumpyFromArrayDtypeMapping() {
    Pillow.Configure({ DllPath: PillowTestNumpyDllPath() })
    b := Numpy.Array([1, 0, 0, 1], [2, 2], 1)
    im := Pillow.Image.FromArray(b)
    AhkTest.AssertEqual("1", im.Mode)
    AhkTest.AssertEqual([2, 2], im.Size)
    AhkTest.AssertEqual("8040", PillowTestNumpyBufHex(im.ToBytes()))
    im.Close()
    i1 := Numpy.Array([1, 2, 3, 4, 5, 6], [2, 3], 2)
    im := Pillow.Image.FromArray(i1)
    AhkTest.AssertEqual("I", im.Mode)
    AhkTest.AssertEqual("010000000200000003000000040000000500000006000000", PillowTestNumpyBufHex(im.ToBytes()))
    im.Close()
    u1 := Numpy.Array([1, 2, 3, 4, 5, 6], [2, 3], 3)
    im := Pillow.Image.FromArray(u1)
    AhkTest.AssertEqual("L", im.Mode)
    AhkTest.AssertEqual("010203040506", PillowTestNumpyBufHex(im.ToBytes()))
    im.Close()
    la := Numpy.Zeros([2, 3, 2], 3)
    im := Pillow.Image.FromArray(la)
    AhkTest.AssertEqual("LA", im.Mode)
    im.Close()
    rgb := Numpy.Zeros([2, 3, 3], 3)
    im := Pillow.Image.FromArray(rgb)
    AhkTest.AssertEqual("RGB", im.Mode)
    AhkTest.AssertEqual([3, 2], im.Size)
    im.Close()
    rgba := Numpy.Zeros([2, 3, 4], 3)
    im := Pillow.Image.FromArray(rgba)
    AhkTest.AssertEqual("RGBA", im.Mode)
    im.Close()
    i2 := Numpy.Array([-2, 300, 4, -5], [2, 2], 4)
    im := Pillow.Image.FromArray(i2)
    AhkTest.AssertEqual("I", im.Mode)
    AhkTest.AssertEqual("feffffff2c01000004000000fbffffff", PillowTestNumpyBufHex(im.ToBytes()))
    im.Close()
    u2 := Numpy.Array([1, 2, 3, 4], [2, 2], 5)
    im := Pillow.Image.FromArray(u2)
    AhkTest.AssertEqual("I;16", im.Mode)
    AhkTest.AssertEqual("0100020003000400", PillowTestNumpyBufHex(im.ToBytes()))
    im.Close()
    i4 := Numpy.Array([-2, 3, 4, -5], [2, 2], 6)
    im := Pillow.Image.FromArray(i4)
    AhkTest.AssertEqual("I", im.Mode)
    AhkTest.AssertEqual("feffffff0300000004000000fbffffff", PillowTestNumpyBufHex(im.ToBytes()))
    im.Close()
    u4 := Numpy.Array([0, 1, 2, 3, 4, 5], [2, 3], 7)
    im := Pillow.Image.FromArray(u4)
    AhkTest.AssertEqual("I", im.Mode)
    im.Close()
    f4 := Numpy.Array([0.5, -1.25, 2.0, 3.5], [2, 2], 12)
    im := Pillow.Image.FromArray(f4)
    AhkTest.AssertEqual("F", im.Mode)
    AhkTest.AssertEqual("0000003f0000a0bf0000004000006040", PillowTestNumpyBufHex(im.ToBytes()))
    im.Close()
    f8 := Numpy.Array([0.5, -1.25, 2.0, 3.5], [2, 2], 13)
    im := Pillow.Image.FromArray(f8)
    AhkTest.AssertEqual("F", im.Mode)
    AhkTest.AssertEqual("0000003f0000a0bf0000004000006040", PillowTestNumpyBufHex(im.ToBytes()))
    im.Close()
}

PillowTestNumpyFromArrayDtypeErrors() {
    Pillow.Configure({ DllPath: PillowTestNumpyDllPath() })
    i8 := Numpy.Zeros([2, 3], 8)
    AhkTest.AssertEqual("Cannot handle this data type: (1, 1), <i8", PillowTestNumpyFormatCaptureError(() => Pillow.Image.FromArray(i8)))
    u8 := Numpy.Zeros([2, 3], 9)
    AhkTest.AssertEqual("Cannot handle this data type: (1, 1), <u8", PillowTestNumpyFormatCaptureError(() => Pillow.Image.FromArray(u8)))
    five := Numpy.Zeros([2, 3, 5], 3)
    AhkTest.AssertEqual("Cannot handle this data type: (1, 1, 5), |u1", PillowTestNumpyFormatCaptureError(() => Pillow.Image.FromArray(five)))
    four := Numpy.Zeros([2, 2, 2, 2], 3)
    AhkTest.AssertEqual("Cannot handle this data type: (1, 1, 2, 2), |u1", PillowTestNumpyFormatCaptureError(() => Pillow.Image.FromArray(four)))
    f8d := Numpy.Zeros([2, 3, 4], 13)
    AhkTest.AssertEqual("Cannot handle this data type: (1, 1, 4), <f8", PillowTestNumpyFormatCaptureError(() => Pillow.Image.FromArray(f8d)))
    i2d := Numpy.Zeros([2, 3, 3], 4)
    AhkTest.AssertEqual("Cannot handle this data type: (1, 1, 3), <i2", PillowTestNumpyFormatCaptureError(() => Pillow.Image.FromArray(i2d)))
}

PillowTestNumpyFromArrayDimensionRules() {
    Pillow.Configure({ DllPath: PillowTestNumpyDllPath() })
    f1 := Numpy.Array([1, 2], unset, 12)
    im := Pillow.Image.FromArray(f1)
    AhkTest.AssertEqual("F", im.Mode)
    AhkTest.AssertEqual([1, 2], im.Size)
    AhkTest.AssertEqual("0000803f00000040", PillowTestNumpyBufHex(im.ToBytes()))
    im.Close()
    four := Numpy.Zeros([2, 2, 2, 2], 3)
    AhkTest.AssertEqual("Too many dimensions: 4 > 3.", PillowTestNumpyFormatCaptureError(() => Pillow.Image.FromArray(four, "RGB")))
    three := Numpy.Zeros([2, 3, 3], 3)
    AhkTest.AssertEqual("Too many dimensions: 3 > 2.", PillowTestNumpyFormatCaptureError(() => Pillow.Image.FromArray(three, "L")))
}

PillowTestNumpyFromArrayModeParam() {
    Pillow.Configure({ DllPath: PillowTestNumpyDllPath() })
    u2 := Numpy.Array([300, 0], [1, 2], 5)
    im := Pillow.Image.FromArray(u2, "L")
    AhkTest.AssertEqual("L", im.Mode)
    AhkTest.AssertEqual(44, im.GetPixel([0, 0]))
    AhkTest.AssertEqual("2c01", PillowTestNumpyBufHex(im.ToBytes()))
    im.Close()
    rgbShort := Numpy.Array([300, 0, 0], [1, 3], 5)
    AhkTest.AssertEqual("not enough image data", PillowTestNumpyFormatCaptureError(() => Pillow.Image.FromArray(rgbShort, "RGB")))
    p := Numpy.Array([1, 2, 3, 4], [2, 2], 3)
    im := Pillow.Image.FromArray(p, "P")
    AhkTest.AssertEqual("P", im.Mode)
    AhkTest.AssertEqual("01020304", PillowTestNumpyBufHex(im.ToBytes()))
    im.Close()
}

PillowTestNumpyFromArrayRejectsNonNdArray() {
    Pillow.Configure({ DllPath: PillowTestNumpyDllPath() })
    AhkTest.AssertEqual("Pillow.Image.FromArray expects a cnumpy Numpy.NdArray", PillowTestNumpyFormatCaptureError(() => Pillow.Image.FromArray("bogus")))
}

PillowTestNumpyRoundTrips() {
    Pillow.Configure({ DllPath: PillowTestNumpyDllPath() })
    cases := Map(
        "L",    [[3, 2], [1, 2, 3, 4, 5, 6]],
        "RGB",  [[3, 2], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18]],
        "RGBA", [[3, 2], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24]],
        "LA",   [[3, 2], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]],
        "I;16", [[2, 2], [1, 0, 2, 0, 3, 0, 4, 0]]
    )
    for mode, spec in cases {
        image := Pillow.Image.FromBuffer(mode, spec[1], PillowTestNumpyBuf(spec[2]), "raw", mode)
        arr := image.AsArray()
        back := Pillow.Image.FromArray(arr)
        AhkTest.AssertEqual(mode, back.Mode, mode " roundtrip mode")
        AhkTest.AssertEqual(spec[1], back.Size, mode " roundtrip size")
        AhkTest.AssertEqual(PillowTestNumpyBufHex(image.ToBytes()), PillowTestNumpyBufHex(back.ToBytes()), mode " roundtrip bytes")
        back.Close()
        image.Close()
    }
    one := Pillow.Image.FromBuffer("1", [2, 2], PillowTestNumpyBuf([0x80, 0x40]), "raw", "1")
    arr := one.AsArray()
    AhkTest.AssertEqual("ff0000ff", PillowTestNumpyArrayBytesHex(arr))
    back := Pillow.Image.FromArray(arr)
    AhkTest.AssertEqual("1", back.Mode)
    AhkTest.AssertEqual("8040", PillowTestNumpyBufHex(back.ToBytes()))
    back.Close()
    one.Close()
    i := Pillow.Image.FromBuffer("I", [2, 2], PillowTestNumpyBuf([0xFE, 0xFF, 0xFF, 0xFF, 3, 0, 0, 0, 4, 0, 0, 0, 0xFB, 0xFF, 0xFF, 0xFF]), "raw", "I")
    arr := i.AsArray()
    AhkTest.AssertEqual(6, arr.Dtype)
    back := Pillow.Image.FromArray(arr)
    AhkTest.AssertEqual("I", back.Mode)
    AhkTest.AssertEqual("feffffff0300000004000000fbffffff", PillowTestNumpyBufHex(back.ToBytes()))
    back.Close()
    i.Close()
    f := Pillow.Image.FromBuffer("F", [2, 2], PillowTestNumpyBuf([0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0xA0, 0xBF, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x60, 0x40]), "raw", "F")
    arr := f.AsArray()
    AhkTest.AssertEqual(12, arr.Dtype)
    back := Pillow.Image.FromArray(arr)
    AhkTest.AssertEqual("F", back.Mode)
    AhkTest.AssertEqual("0000003f0000a0bf0000004000006040", PillowTestNumpyBufHex(back.ToBytes()))
    back.Close()
    f.Close()
}

PillowTestNumpyStridedFromArray() {
    Pillow.Configure({ DllPath: PillowTestNumpyDllPath() })
    values := []
    loop 36
        values.Push(A_Index - 1)
    full := Numpy.Array(values, [6, 6], 3)
    strided := full.Slice(0, 6, 2, 0).Slice(0, 6, 2, 1)
    im := Pillow.Image.FromArray(strided)
    AhkTest.AssertEqual("L", im.Mode)
    AhkTest.AssertEqual([3, 3], im.Size)
    AhkTest.AssertEqual("0002040c0e10181a1c", PillowTestNumpyBufHex(im.ToBytes()))
    im.Close()
}

AhkTest.Test("Pillow numpy interop AsArray dtype shape and bytes per mode", PillowTestNumpyAsArrayModes)
AhkTest.Test("Pillow numpy interop AsArray P mode keeps palette indices only", PillowTestNumpyAsArrayPIndicesOnly)
AhkTest.Test("Pillow numpy interop AsArray I;16B big-endian boundary", PillowTestNumpyAsArrayI16BBoundary)
AhkTest.Test("Pillow numpy interop AsArray array survives image close", PillowTestNumpyAsArraySurvivesImageClose)
AhkTest.Test("Pillow numpy interop FromArray dtype mapping", PillowTestNumpyFromArrayDtypeMapping)
AhkTest.Test("Pillow numpy interop FromArray dtype error shapes", PillowTestNumpyFromArrayDtypeErrors)
AhkTest.Test("Pillow numpy interop FromArray dimension rules", PillowTestNumpyFromArrayDimensionRules)
AhkTest.Test("Pillow numpy interop FromArray mode parameter", PillowTestNumpyFromArrayModeParam)
AhkTest.Test("Pillow numpy interop FromArray rejects non-NdArray", PillowTestNumpyFromArrayRejectsNonNdArray)
AhkTest.Test("Pillow numpy interop round trips", PillowTestNumpyRoundTrips)
AhkTest.Test("Pillow numpy interop strided FromArray serialization", PillowTestNumpyStridedFromArray)
