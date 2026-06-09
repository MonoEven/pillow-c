#Requires AutoHotkey v2.0
#Include <stdlib\ahktest>

PillowCDllPath() {
    SplitPath A_LineFile, , &ahkDir
    return ahkDir "\..\build\x64\Release\pillow_c.dll"
}

PillowCBuffer(values) {
    buf := Buffer(values.Length, 0)
    for index, value in values
        NumPut("UChar", value, buf, index - 1)
    return buf
}

PillowCBufferToArray(buf) {
    values := []
    loop buf.Size
        values.Push(NumGet(buf, A_Index - 1, "UChar"))
    return values
}

PillowCArraySlice(values, firstIndex, lastIndex) {
    out := []
    loop lastIndex - firstIndex + 1
        out.Push(values[firstIndex + A_Index - 1])
    return out
}

PillowCAssertArrayNear(expected, actual, tolerance) {
    AhkTest.AssertEqual(expected.Length, actual.Length)
    for index, expectedValue in expected
        AhkTest.AssertTrue(Abs(expectedValue - actual[index]) <= tolerance)
}

PillowCWriteFileBytes(path, values) {
    buf := PillowCBuffer(values)
    file := FileOpen(path, "w")
    try {
        file.RawWrite(buf, buf.Size)
    } finally {
        file.Close()
    }
}

PillowCReadFileBytes(path) {
    file := FileOpen(path, "r")
    try {
        buf := Buffer(file.Length, 0)
        file.RawRead(buf, buf.Size)
        return PillowCBufferToArray(buf)
    } finally {
        file.Close()
    }
}

PillowCReadBe32(values, offset) {
    return (values[offset] << 24)
        | (values[offset + 1] << 16)
        | (values[offset + 2] << 8)
        | values[offset + 3]
}

PillowCReadBe16(values, offset) {
    return (values[offset] << 8) | values[offset + 1]
}

PillowCArrayEquals(left, right) {
    if left.Length != right.Length
        return false
    for index, value in left {
        if value != right[index]
            return false
    }
    return true
}

PillowCFindPngChunk(values, typeBytes) {
    pos := 9
    while pos + 11 <= values.Length {
        length := PillowCReadBe32(values, pos)
        typeStart := pos + 4
        if typeStart + 3 <= values.Length
            && PillowCArrayEquals(PillowCArraySlice(values, typeStart, typeStart + 3), typeBytes) {
            return { Found: true, DataOffset: pos + 8, Length: length }
        }
        pos += 12 + length
    }
    return { Found: false, DataOffset: 0, Length: 0 }
}

PillowCAssertPngPhys(path, expectedX, expectedY) {
    bytes := PillowCReadFileBytes(path)
    chunk := PillowCFindPngChunk(bytes, [0x70, 0x48, 0x59, 0x73])
    AhkTest.AssertTrue(chunk.Found)
    AhkTest.AssertEqual(9, chunk.Length)
    AhkTest.AssertEqual(expectedX, PillowCReadBe32(bytes, chunk.DataOffset))
    AhkTest.AssertEqual(expectedY, PillowCReadBe32(bytes, chunk.DataOffset + 4))
    AhkTest.AssertEqual(1, bytes[chunk.DataOffset + 8])
}

PillowCReadJpegJfif(path) {
    bytes := PillowCReadFileBytes(path)
    AhkTest.AssertTrue(bytes.Length >= 4)
    AhkTest.AssertEqual([0xFF, 0xD8], PillowCArraySlice(bytes, 1, 2))
    pos := 3
    while pos + 3 <= bytes.Length {
        AhkTest.AssertEqual(0xFF, bytes[pos])
        marker := bytes[pos + 1]
        pos += 2
        if marker = 0xD9 || (marker >= 0xD0 && marker <= 0xD7)
            continue
        length := PillowCReadBe16(bytes, pos)
        payloadStart := pos + 2
        if marker = 0xE0
            && length >= 16
            && PillowCArrayEquals(PillowCArraySlice(bytes, payloadStart, payloadStart + 4), [0x4A, 0x46, 0x49, 0x46, 0]) {
            return {
                Unit: bytes[payloadStart + 7],
                XDensity: PillowCReadBe16(bytes, payloadStart + 8),
                YDensity: PillowCReadBe16(bytes, payloadStart + 10)
            }
        }
        pos += length
    }
    AhkTest.Fail("Expected JPEG JFIF metadata")
}

PillowCExifOrientationSegment(orientation) {
    return [
        0xFF, 0xE1, 0x00, 0x22,
        0x45, 0x78, 0x69, 0x66, 0x00, 0x00,
        0x49, 0x49, 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00,
        0x01, 0x00,
        0x12, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00,
        orientation, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    ]
}

PillowCWriteJpegWithExifOrientation(sourcePath, targetPath, orientation) {
    bytes := PillowCReadFileBytes(sourcePath)
    AhkTest.AssertTrue(bytes.Length >= 2)
    AhkTest.AssertEqual([0xFF, 0xD8], PillowCArraySlice(bytes, 1, 2))

    out := [bytes[1], bytes[2]]
    for value in PillowCExifOrientationSegment(orientation)
        out.Push(value)
    loop bytes.Length - 2
        out.Push(bytes[A_Index + 2])
    PillowCWriteFileBytes(targetPath, out)
}

PillowCTempBmpPath(name) {
    return A_Temp "\pillow-c-" name "-" A_TickCount "-" Random(1, 1000000) ".bmp"
}

PillowCTempPngPath(name) {
    return A_Temp "\pillow-c-" name "-" A_TickCount "-" Random(1, 1000000) ".png"
}

PillowCTempJpegPath(name) {
    return A_Temp "\pillow-c-" name "-" A_TickCount "-" Random(1, 1000000) ".jpg"
}

PillowCTempTiffPath(name) {
    return A_Temp "\pillow-c-" name "-" A_TickCount "-" Random(1, 1000000) ".tiff"
}

PillowCTempGifPath(name) {
    return A_Temp "\pillow-c-" name "-" A_TickCount "-" Random(1, 1000000) ".gif"
}

PillowCDeleteFile(path) {
    try FileDelete path
}

PillowCDoubleBuffer(values) {
    buf := Buffer(values.Length * 8, 0)
    for index, value in values
        NumPut("Double", value, buf, (index - 1) * 8)
    return buf
}

PillowCIntBuffer(values) {
    buf := Buffer(values.Length * 4, 0)
    for index, value in values
        NumPut("Int", value, buf, (index - 1) * 4)
    return buf
}

SumArray(values) {
    total := 0
    for value in values
        total += value
    return total
}

PillowCAssertStatus(status) {
    AhkTest.AssertEqual(0, status)
}

PillowCAbiVersion() {
    major := 0
    minor := 0
    patch := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_abi_version",
        "Int*", &major,
        "Int*", &minor,
        "Int*", &patch,
        "Int"
    )
    PillowCAssertStatus(status)
    return [major, minor, patch]
}

PillowCStatusMessage(statusCode) {
    buf := Buffer(64, 0)
    required := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_status_message",
        "Int", statusCode,
        "Ptr", buf,
        "UPtr", buf.Size,
        "UPtr*", &required,
        "Int"
    )
    PillowCAssertStatus(status)
    return { Text: StrGet(buf.Ptr, required - 1, "UTF-8"), Required: required }
}

PillowCModeFromString(modeName) {
    mode := -1
    data := Buffer(StrPut(modeName, "UTF-8"), 0)
    StrPut(modeName, data, "UTF-8")
    status := DllCall(
        PillowCDllPath() "\pillow_c_mode_from_string",
        "Ptr", data,
        "Int*", &mode,
        "Int"
    )
    PillowCAssertStatus(status)
    return mode
}

PillowCModeName(mode) {
    buf := Buffer(16, 0)
    required := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_mode_name",
        "Int", mode,
        "Ptr", buf,
        "UPtr", buf.Size,
        "UPtr*", &required,
        "Int"
    )
    PillowCAssertStatus(status)
    return { Text: StrGet(buf.Ptr, required - 1, "UTF-8"), Required: required }
}

PillowCRawModeBuffer(rawMode) {
    buf := Buffer(StrPut(rawMode, "UTF-8"), 0)
    StrPut(rawMode, buf, "UTF-8")
    return buf
}

PillowCUtf8Buffer(value) {
    buf := Buffer(StrPut(value, "UTF-8"), 0)
    StrPut(value, buf, "UTF-8")
    return buf
}

PillowCBlend(leftValues, rightValues, alpha) {
    left := PillowCBuffer(leftValues)
    right := PillowCBuffer(rightValues)
    out := Buffer(leftValues.Length, 0)
    status := DllCall(
        PillowCDllPath() "\pillow_c_blend_u8",
        "Ptr", left,
        "Ptr", right,
        "Ptr", out,
        "UPtr", leftValues.Length,
        "Double", alpha,
        "Int"
    )
    PillowCAssertStatus(status)
    return PillowCBufferToArray(out)
}

PillowCRgbToL(rgbValues) {
    rgb := PillowCBuffer(rgbValues)
    out := Buffer(rgbValues.Length // 3, 0)
    status := DllCall(
        PillowCDllPath() "\pillow_c_rgb_to_l",
        "Ptr", rgb,
        "Ptr", out,
        "UPtr", rgbValues.Length // 3,
        "Int"
    )
    PillowCAssertStatus(status)
    return PillowCBufferToArray(out)
}

PillowCAlphaCompositeRgba(dstValues, srcValues) {
    dst := PillowCBuffer(dstValues)
    src := PillowCBuffer(srcValues)
    out := Buffer(dstValues.Length, 0)
    status := DllCall(
        PillowCDllPath() "\pillow_c_alpha_composite_rgba",
        "Ptr", dst,
        "Ptr", src,
        "Ptr", out,
        "UPtr", dstValues.Length // 4,
        "Int"
    )
    PillowCAssertStatus(status)
    return PillowCBufferToArray(out)
}

PillowCCreateImage(width, height, channels) {
    handle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_create",
        "Int", width,
        "Int", height,
        "Int", channels,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCCreateImageMode(width, height, mode) {
    handle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_create_mode",
        "Int", width,
        "Int", height,
        "Int", mode,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCImageLinearGradient(mode) {
    handle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_linear_gradient",
        "Int", mode,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCImageRadialGradient(mode) {
    handle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_radial_gradient",
        "Int", mode,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCImageEffectMandelbrot(width, height, extent, quality) {
    handle := 0
    extentBuffer := PillowCDoubleBuffer(extent)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_effect_mandelbrot",
        "Int", width,
        "Int", height,
        "Ptr", extentBuffer,
        "Int", quality,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCImageEffectNoise(width, height, sigma) {
    handle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_effect_noise",
        "Int", width,
        "Int", height,
        "Double", sigma,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCTestSrand(seed := 1) {
    DllCall("ucrtbase\srand", "UInt", seed, "Cdecl")
}

PillowCImageEffectSpread(source, distance) {
    handle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_effect_spread",
        "Ptr", source,
        "Int", distance,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCImageOpenBmp(path) {
    handle := 0
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_open_bmp",
        "Ptr", pathBytes,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCImageSaveBmp(handle, path) {
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_save_bmp",
        "Ptr", handle,
        "Ptr", pathBytes,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageOpenPng(path) {
    handle := 0
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_open_png",
        "Ptr", pathBytes,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCImageSavePng(handle, path) {
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_save_png",
        "Ptr", handle,
        "Ptr", pathBytes,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageSavePngCompressLevel(handle, path, compressLevel) {
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_save_png_compress_level",
        "Ptr", handle,
        "Ptr", pathBytes,
        "Int", compressLevel,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageOpenJpeg(path) {
    handle := 0
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_open_jpeg",
        "Ptr", pathBytes,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCImageSaveJpeg(handle, path) {
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_save_jpeg",
        "Ptr", handle,
        "Ptr", pathBytes,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageSaveJpegQuality(handle, path, quality) {
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_save_jpeg_quality",
        "Ptr", handle,
        "Ptr", pathBytes,
        "Int", quality,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageOpenTiff(path) {
    handle := 0
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_open_tiff",
        "Ptr", pathBytes,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCImageOpenTiffFrame(path, frame) {
    handle := 0
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_open_tiff_frame",
        "Ptr", pathBytes,
        "Int", frame,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCImageFrameCountTiff(path) {
    count := 0
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_frame_count_tiff",
        "Ptr", pathBytes,
        "Int*", &count,
        "Int"
    )
    PillowCAssertStatus(status)
    return count
}

PillowCImageSaveTiff(handle, path) {
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_save_tiff",
        "Ptr", handle,
        "Ptr", pathBytes,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageOpenGif(path) {
    handle := 0
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_open_gif",
        "Ptr", pathBytes,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCImageOpenGifFrame(path, frame) {
    handle := 0
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_open_gif_frame",
        "Ptr", pathBytes,
        "Int", frame,
        "Ptr*", &handle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(handle != 0)
    return handle
}

PillowCImageFrameCountGif(path) {
    count := 0
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_frame_count_gif",
        "Ptr", pathBytes,
        "Int*", &count,
        "Int"
    )
    PillowCAssertStatus(status)
    return count
}

PillowCImageGifMetadata(path, frame) {
    duration := -1
    loopCount := -1
    disposal := -1
    background := -1
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_gif_metadata",
        "Ptr", pathBytes,
        "Int", frame,
        "Int*", &duration,
        "Int*", &loopCount,
        "Int*", &disposal,
        "Int*", &background,
        "Int"
    )
    PillowCAssertStatus(status)
    return { Duration: duration, Loop: loopCount, Disposal: disposal, Background: background }
}

PillowCImageSaveGif(handle, path) {
    pathBytes := PillowCUtf8Buffer(path)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_save_gif",
        "Ptr", handle,
        "Ptr", pathBytes,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageSaveGifAnimation(handles, path, durations := unset, loopCount := -1, disposals := unset) {
    pathBytes := PillowCUtf8Buffer(path)
    handleArray := PillowCImageHandleArray(handles)
    durationBuffer := 0
    durationCount := 0
    disposalBuffer := 0
    disposalCount := 0
    if IsSet(durations) {
        durationBuffer := PillowCIntBuffer(durations)
        durationCount := durations.Length
    }
    if IsSet(disposals) {
        disposalBuffer := PillowCIntBuffer(disposals)
        disposalCount := disposals.Length
    }
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_save_gif_animation",
        "Ptr", handleArray,
        "UPtr", handles.Length,
        "Ptr", pathBytes,
        "Ptr", durationBuffer,
        "UPtr", durationCount,
        "Int", loopCount,
        "Ptr", disposalBuffer,
        "UPtr", disposalCount,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageDrawRectangle(handle, box, fill := unset, outline := unset, width := 1) {
    fillPtr := 0
    fillSize := 0
    fillBuffer := 0
    if IsSet(fill) {
        fillBuffer := PillowCBuffer(fill)
        fillPtr := fillBuffer.Ptr
        fillSize := fillBuffer.Size
    }
    outlinePtr := 0
    outlineSize := 0
    outlineBuffer := 0
    if IsSet(outline) {
        outlineBuffer := PillowCBuffer(outline)
        outlinePtr := outlineBuffer.Ptr
        outlineSize := outlineBuffer.Size
    }
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_draw_rectangle",
        "Ptr", handle,
        "Int", box[1],
        "Int", box[2],
        "Int", box[3],
        "Int", box[4],
        "Ptr", fillPtr,
        "UPtr", fillSize,
        "Ptr", outlinePtr,
        "UPtr", outlineSize,
        "Int", width,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageDrawEllipse(handle, box, fill := unset, outline := unset, width := 1) {
    fillPtr := 0
    fillSize := 0
    fillBuffer := 0
    if IsSet(fill) {
        fillBuffer := PillowCBuffer(fill)
        fillPtr := fillBuffer.Ptr
        fillSize := fillBuffer.Size
    }
    outlinePtr := 0
    outlineSize := 0
    outlineBuffer := 0
    if IsSet(outline) {
        outlineBuffer := PillowCBuffer(outline)
        outlinePtr := outlineBuffer.Ptr
        outlineSize := outlineBuffer.Size
    }
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_draw_ellipse",
        "Ptr", handle,
        "Int", box[1],
        "Int", box[2],
        "Int", box[3],
        "Int", box[4],
        "Ptr", fillPtr,
        "UPtr", fillSize,
        "Ptr", outlinePtr,
        "UPtr", outlineSize,
        "Int", width,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageDrawArc(handle, box, start, end, fill, width := 1) {
    fillBuffer := PillowCBuffer(fill)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_draw_arc",
        "Ptr", handle,
        "Int", box[1],
        "Int", box[2],
        "Int", box[3],
        "Int", box[4],
        "Double", start,
        "Double", end,
        "Ptr", fillBuffer,
        "UPtr", fillBuffer.Size,
        "Int", width,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageDrawChord(handle, box, start, end, fill := unset, outline := unset, width := 1) {
    fillPtr := 0
    fillSize := 0
    fillBuffer := 0
    if IsSet(fill) {
        fillBuffer := PillowCBuffer(fill)
        fillPtr := fillBuffer.Ptr
        fillSize := fillBuffer.Size
    }
    outlinePtr := 0
    outlineSize := 0
    outlineBuffer := 0
    if IsSet(outline) {
        outlineBuffer := PillowCBuffer(outline)
        outlinePtr := outlineBuffer.Ptr
        outlineSize := outlineBuffer.Size
    }
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_draw_chord",
        "Ptr", handle,
        "Int", box[1],
        "Int", box[2],
        "Int", box[3],
        "Int", box[4],
        "Double", start,
        "Double", end,
        "Ptr", fillPtr,
        "UPtr", fillSize,
        "Ptr", outlinePtr,
        "UPtr", outlineSize,
        "Int", width,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageDrawPieslice(handle, box, start, end, fill := unset, outline := unset, width := 1) {
    fillPtr := 0
    fillSize := 0
    fillBuffer := 0
    if IsSet(fill) {
        fillBuffer := PillowCBuffer(fill)
        fillPtr := fillBuffer.Ptr
        fillSize := fillBuffer.Size
    }
    outlinePtr := 0
    outlineSize := 0
    outlineBuffer := 0
    if IsSet(outline) {
        outlineBuffer := PillowCBuffer(outline)
        outlinePtr := outlineBuffer.Ptr
        outlineSize := outlineBuffer.Size
    }
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_draw_pieslice",
        "Ptr", handle,
        "Int", box[1],
        "Int", box[2],
        "Int", box[3],
        "Int", box[4],
        "Double", start,
        "Double", end,
        "Ptr", fillPtr,
        "UPtr", fillSize,
        "Ptr", outlinePtr,
        "UPtr", outlineSize,
        "Int", width,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageDrawRoundedRectangle(handle, box, radius := 0, fill := unset, outline := unset, width := 1, cornersMask := 15) {
    fillPtr := 0
    fillSize := 0
    fillBuffer := 0
    if IsSet(fill) {
        fillBuffer := PillowCBuffer(fill)
        fillPtr := fillBuffer.Ptr
        fillSize := fillBuffer.Size
    }
    outlinePtr := 0
    outlineSize := 0
    outlineBuffer := 0
    if IsSet(outline) {
        outlineBuffer := PillowCBuffer(outline)
        outlinePtr := outlineBuffer.Ptr
        outlineSize := outlineBuffer.Size
    }
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_draw_rounded_rectangle",
        "Ptr", handle,
        "Int", box[1],
        "Int", box[2],
        "Int", box[3],
        "Int", box[4],
        "Double", radius,
        "Ptr", fillPtr,
        "UPtr", fillSize,
        "Ptr", outlinePtr,
        "UPtr", outlineSize,
        "Int", width,
        "Int", cornersMask,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageDrawBitmap(handle, xy, mask, fill) {
    fillBuffer := PillowCBuffer(fill)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_draw_bitmap",
        "Ptr", handle,
        "Int", xy[1],
        "Int", xy[2],
        "Ptr", mask,
        "Ptr", fillBuffer,
        "UPtr", fillBuffer.Size,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageDrawFloodfill(handle, xy, value, border := unset, thresh := 0.0) {
    valueBuffer := PillowCBuffer(value)
    borderPtr := 0
    borderSize := 0
    borderBuffer := 0
    if IsSet(border) {
        borderBuffer := PillowCBuffer(border)
        borderPtr := borderBuffer.Ptr
        borderSize := borderBuffer.Size
    }
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_draw_floodfill",
        "Ptr", handle,
        "Int", xy[1],
        "Int", xy[2],
        "Ptr", valueBuffer,
        "UPtr", valueBuffer.Size,
        "Ptr", borderPtr,
        "UPtr", borderSize,
        "Double", thresh,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageDrawLine(handle, points, fill, width := 0) {
    pointBuffer := PillowCIntBuffer(points)
    fillBuffer := PillowCBuffer(fill)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_draw_line",
        "Ptr", handle,
        "Ptr", pointBuffer,
        "UPtr", points.Length // 2,
        "Ptr", fillBuffer,
        "UPtr", fillBuffer.Size,
        "Int", width,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageDrawLineJoint(handle, points, fill, width := 0, jointCurve := false) {
    pointBuffer := PillowCIntBuffer(points)
    fillBuffer := PillowCBuffer(fill)
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_line_joint",
            "Ptr", handle,
            "Ptr", pointBuffer,
            "UPtr", points.Length // 2,
            "Ptr", fillBuffer,
            "UPtr", fillBuffer.Size,
            "Int", width,
            "Int", jointCurve ? 1 : 0,
            "Int"
        )
    } catch Error as err {
        AhkTest.Fail("Expected pillow_c_image_draw_line_joint export: " err.Message)
    }
    PillowCAssertStatus(status)
}

PillowCImageDrawPoints(handle, points, fill) {
    pointBuffer := points.Length ? PillowCIntBuffer(points) : 0
    fillBuffer := PillowCBuffer(fill)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_draw_points",
        "Ptr", handle,
        "Ptr", pointBuffer,
        "UPtr", points.Length // 2,
        "Ptr", fillBuffer,
        "UPtr", fillBuffer.Size,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageDrawPolygon(handle, points, fill := unset, outline := unset, width := 1) {
    pointBuffer := PillowCIntBuffer(points)
    fillPtr := 0
    fillSize := 0
    fillBuffer := 0
    if IsSet(fill) {
        fillBuffer := PillowCBuffer(fill)
        fillPtr := fillBuffer.Ptr
        fillSize := fillBuffer.Size
    }
    outlinePtr := 0
    outlineSize := 0
    outlineBuffer := 0
    if IsSet(outline) {
        outlineBuffer := PillowCBuffer(outline)
        outlinePtr := outlineBuffer.Ptr
        outlineSize := outlineBuffer.Size
    }
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_draw_polygon",
        "Ptr", handle,
        "Ptr", pointBuffer,
        "UPtr", points.Length // 2,
        "Ptr", fillPtr,
        "UPtr", fillSize,
        "Ptr", outlinePtr,
        "UPtr", outlineSize,
        "Int", width,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCFreeImage(handle) {
    return DllCall(PillowCDllPath() "\pillow_c_image_free", "Ptr", handle, "Int")
}

PillowCImageSetBytes(handle, values) {
    data := PillowCBuffer(values)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_set_bytes",
        "Ptr", handle,
        "Ptr", data,
        "UPtr", values.Length,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageSetRawBytes(handle, values, rawMode, stride := 0, orientation := 1) {
    data := PillowCBuffer(values)
    modeBytes := PillowCRawModeBuffer(rawMode)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_set_raw_bytes",
        "Ptr", handle,
        "Ptr", data,
        "UPtr", data.Size,
        "Ptr", modeBytes,
        "Int", stride,
        "Int", orientation,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageGetRawBytes(handle, rawMode) {
    modeBytes := PillowCRawModeBuffer(rawMode)
    required := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_get_raw_bytes",
        "Ptr", handle,
        "Ptr", modeBytes,
        "Ptr", 0,
        "UPtr", 0,
        "UPtr*", &required,
        "Int"
    )
    PillowCAssertStatus(status)
    out := Buffer(required, 0)
    if required = 0
        return []
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_get_raw_bytes",
        "Ptr", handle,
        "Ptr", modeBytes,
        "Ptr", out,
        "UPtr", out.Size,
        "UPtr*", &required,
        "Int"
    )
    PillowCAssertStatus(status)
    return PillowCBufferToArray(out)
}

PillowCImagePutData(handle, values, pixelCount) {
    data := PillowCBuffer(values)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_put_data",
        "Ptr", handle,
        "Ptr", data,
        "UPtr", data.Size,
        "UPtr", pixelCount,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageFill(handle, values) {
    color := PillowCBuffer(values)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_fill",
        "Ptr", handle,
        "Ptr", color,
        "UPtr", values.Length,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageGetPixel(handle, x, y, channels) {
    out := Buffer(channels, 0)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_getpixel",
        "Ptr", handle,
        "Int", x,
        "Int", y,
        "Ptr", out,
        "UPtr", out.Size,
        "Int"
    )
    PillowCAssertStatus(status)
    return PillowCBufferToArray(out)
}

PillowCImagePutPixel(handle, x, y, values) {
    color := PillowCBuffer(values)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_putpixel",
        "Ptr", handle,
        "Int", x,
        "Int", y,
        "Ptr", color,
        "UPtr", color.Size,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageToArray(handle, expectedSize) {
    if expectedSize = 0
        return []
    out := Buffer(expectedSize, 0)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_get_bytes",
        "Ptr", handle,
        "Ptr", out,
        "UPtr", expectedSize,
        "Int"
    )
    PillowCAssertStatus(status)
    return PillowCBufferToArray(out)
}

PillowCImageInt(handle, exportName) {
    value := 0
    status := DllCall(PillowCDllPath() "\" exportName, "Ptr", handle, "Int*", &value, "Int")
    PillowCAssertStatus(status)
    return value
}

PillowCImageMode(handle) {
    mode := -1
    status := DllCall(PillowCDllPath() "\pillow_c_image_mode", "Ptr", handle, "Int*", &mode, "Int")
    PillowCAssertStatus(status)
    return mode
}

PillowCImageExifOrientation(handle) {
    orientation := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_exif_orientation",
        "Ptr", handle,
        "Int*", &orientation,
        "Int"
    )
    PillowCAssertStatus(status)
    return orientation
}

PillowCImageMetadataResolution(handle) {
    hasDpi := 0
    dpiX := 0.0
    dpiY := 0.0
    jfif := 0
    jfifMajor := 0
    jfifMinor := 0
    jfifUnit := -1
    jfifDensityX := 0
    jfifDensityY := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_metadata_resolution",
        "Ptr", handle,
        "Int*", &hasDpi,
        "Double*", &dpiX,
        "Double*", &dpiY,
        "Int*", &jfif,
        "Int*", &jfifMajor,
        "Int*", &jfifMinor,
        "Int*", &jfifUnit,
        "Int*", &jfifDensityX,
        "Int*", &jfifDensityY,
        "Int"
    )
    PillowCAssertStatus(status)
    return {
        HasDpi: hasDpi,
        DpiX: dpiX,
        DpiY: dpiY,
        Jfif: jfif,
        JfifVersion: [jfifMajor, jfifMinor],
        JfifUnit: jfifUnit,
        JfifDensity: [jfifDensityX, jfifDensityY],
    }
}

PillowCImageHistogram(handle, expectedCount) {
    out := Buffer(expectedCount * 8, 0)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_histogram",
        "Ptr", handle,
        "Ptr", out,
        "UPtr", expectedCount,
        "Int"
    )
    PillowCAssertStatus(status)
    values := []
    loop expectedCount
        values.Push(NumGet(out, (A_Index - 1) * 8, "Int64"))
    return values
}

PillowCImageHistogramMasked(handle, maskHandle, expectedCount) {
    out := Buffer(expectedCount * 8, 0)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_histogram_masked",
        "Ptr", handle,
        "Ptr", maskHandle,
        "Ptr", out,
        "UPtr", expectedCount,
        "Int"
    )
    PillowCAssertStatus(status)
    values := []
    loop expectedCount
        values.Push(NumGet(out, (A_Index - 1) * 8, "Int64"))
    return values
}

PillowCImageEntropy(handle, maskHandle := 0) {
    value := 0.0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_entropy",
            "Ptr", handle,
            "Ptr", maskHandle,
            "Double*", &value,
            "Int"
        )
    } catch Error as err {
        AhkTest.Fail("Expected pillow_c_image_entropy export: " err.Message)
    }
    PillowCAssertStatus(status)
    return value
}

PillowCAssertFloatClose(expected, actual, tolerance := 0.0000001) {
    if Abs(expected - actual) > tolerance
        AhkTest.Fail("Expected " expected " +/- " tolerance ", got " actual)
}

PillowCImageExtrema(handle, bandCount) {
    minBuf := Buffer(bandCount, 0)
    maxBuf := Buffer(bandCount, 0)
    hasBuf := Buffer(bandCount, 0)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_get_extrema",
        "Ptr", handle,
        "Ptr", minBuf,
        "Ptr", maxBuf,
        "Ptr", hasBuf,
        "UPtr", bandCount,
        "Int"
    )
    PillowCAssertStatus(status)
    values := []
    loop bandCount {
        if NumGet(hasBuf, A_Index - 1, "UChar")
            values.Push([NumGet(minBuf, A_Index - 1, "UChar"), NumGet(maxBuf, A_Index - 1, "UChar")])
        else
            values.Push(0)
    }
    return values
}

PillowCImageGetBbox(handle, alphaOnly := true) {
    left := 0
    top := 0
    right := 0
    bottom := 0
    hasBbox := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_getbbox",
        "Ptr", handle,
        "Int", alphaOnly ? 1 : 0,
        "Int*", &left,
        "Int*", &top,
        "Int*", &right,
        "Int*", &bottom,
        "Int*", &hasBbox,
        "Int"
    )
    PillowCAssertStatus(status)
    return hasBbox ? [left, top, right, bottom] : 0
}

PillowCProjectionArray(buf) {
    values := []
    loop buf.Size
        values.Push(NumGet(buf, A_Index - 1, "UChar"))
    return values
}

PillowCImageGetProjection(handle, width, height) {
    xProjection := Buffer(width, 0)
    yProjection := Buffer(height, 0)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_getprojection",
        "Ptr", handle,
        "Ptr", xProjection,
        "UPtr", xProjection.Size,
        "Ptr", yProjection,
        "UPtr", yProjection.Size,
        "Int"
    )
    PillowCAssertStatus(status)
    return [PillowCProjectionArray(xProjection), PillowCProjectionArray(yProjection)]
}

PillowCImageGetColors(handle, maxcolors, channels) {
    count := 0
    exceeded := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_getcolors",
        "Ptr", handle,
        "Int", maxcolors,
        "Ptr", 0,
        "Ptr", 0,
        "UPtr", 0,
        "UPtr*", &count,
        "Int*", &exceeded,
        "Int"
    )
    PillowCAssertStatus(status)
    if exceeded
        return 0

    counts := Buffer(count * 8, 0)
    colors := Buffer(count * channels, 0)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_getcolors",
        "Ptr", handle,
        "Int", maxcolors,
        "Ptr", counts,
        "Ptr", colors,
        "UPtr", count,
        "UPtr*", &count,
        "Int*", &exceeded,
        "Int"
    )
    PillowCAssertStatus(status)
    if exceeded
        return 0

    out := []
    loop count {
        itemIndex := A_Index - 1
        color := []
        loop channels
            color.Push(NumGet(colors, itemIndex * channels + A_Index - 1, "UChar"))
        out.Push([NumGet(counts, itemIndex * 8, "Int64"), color])
    }
    return out
}

PillowCFindColor(colors, expectedColor) {
    for entry in colors {
        if entry[2].Length != expectedColor.Length
            continue
        matched := true
        for index, value in expectedColor {
            if entry[2][index] != value {
                matched := false
                break
            }
        }
        if matched
            return entry
    }
    return 0
}

PillowCImageSize(handle) {
    value := 0
    status := DllCall(PillowCDllPath() "\pillow_c_image_size", "Ptr", handle, "UPtr*", &value, "Int")
    PillowCAssertStatus(status)
    return value
}

PillowCImageData(handle) {
    dataPtr := 0
    size := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_data",
        "Ptr", handle,
        "Ptr*", &dataPtr,
        "UPtr*", &size,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(dataPtr != 0)
    return { Ptr: dataPtr, Size: size }
}

PillowCImageConstant(sourceHandle, value) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_constant",
        "Ptr", sourceHandle,
        "Int", value,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageBlend(leftHandle, rightHandle, alpha) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_blend",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Double", alpha,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageComposite(sourceHandle, targetHandle, maskHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_composite",
        "Ptr", sourceHandle,
        "Ptr", targetHandle,
        "Ptr", maskHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageDifference(leftHandle, rightHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_difference",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageMultiply(leftHandle, rightHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_multiply",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageScreen(leftHandle, rightHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_screen",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageLighter(leftHandle, rightHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_lighter",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageDarker(leftHandle, rightHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_darker",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageSoftLight(leftHandle, rightHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_soft_light",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageHardLight(leftHandle, rightHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_hard_light",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageOverlay(leftHandle, rightHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_overlay",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageOffset(sourceHandle, xOffset, yOffset) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_offset",
        "Ptr", sourceHandle,
        "Int", xOffset,
        "Int", yOffset,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageAdd(leftHandle, rightHandle, scale := 1.0, offset := 0.0) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_add",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Double", scale,
        "Double", offset,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageSubtract(leftHandle, rightHandle, scale := 1.0, offset := 0.0) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_subtract",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Double", scale,
        "Double", offset,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageAddModulo(leftHandle, rightHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_add_modulo",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageSubtractModulo(leftHandle, rightHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_subtract_modulo",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageLogicalAnd(leftHandle, rightHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_logical_and",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageLogicalOr(leftHandle, rightHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_logical_or",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageLogicalXor(leftHandle, rightHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_logical_xor",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageCopyInto(sourceHandle, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_copy_into",
        "Ptr", sourceHandle,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageBlendInto(leftHandle, rightHandle, alpha, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_blend_into",
        "Ptr", leftHandle,
        "Ptr", rightHandle,
        "Double", alpha,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageCompositeInto(sourceHandle, targetHandle, maskHandle, outHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_composite_into",
        "Ptr", sourceHandle,
        "Ptr", targetHandle,
        "Ptr", maskHandle,
        "Ptr", outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageRgbToL(sourceHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_rgb_to_l",
        "Ptr", sourceHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImagePointLut(sourceHandle, lutValues) {
    lut := PillowCBuffer(lutValues)
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_point_lut",
        "Ptr", sourceHandle,
        "Ptr", lut,
        "UPtr", lut.Size,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImagePointLutMode(sourceHandle, lutValues, targetMode) {
    lut := PillowCBuffer(lutValues)
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_point_lut_mode",
        "Ptr", sourceHandle,
        "Ptr", lut,
        "UPtr", lut.Size,
        "Int", targetMode,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImagePointLutInto(sourceHandle, lutValues, targetHandle) {
    lut := PillowCBuffer(lutValues)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_point_lut_into",
        "Ptr", sourceHandle,
        "Ptr", lut,
        "UPtr", lut.Size,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImagePointLutModeInto(sourceHandle, lutValues, targetMode, targetHandle) {
    lut := PillowCBuffer(lutValues)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_point_lut_mode_into",
        "Ptr", sourceHandle,
        "Ptr", lut,
        "UPtr", lut.Size,
        "Int", targetMode,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageInvert(sourceHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_invert",
        "Ptr", sourceHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageInvertInto(sourceHandle, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_invert_into",
        "Ptr", sourceHandle,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageChopsInvert(sourceHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_chops_invert",
        "Ptr", sourceHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageChopsInvertInto(sourceHandle, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_chops_invert_into",
        "Ptr", sourceHandle,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImagePosterize(sourceHandle, bits) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_posterize",
        "Ptr", sourceHandle,
        "Int", bits,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImagePosterizeInto(sourceHandle, bits, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_posterize_into",
        "Ptr", sourceHandle,
        "Int", bits,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageSolarize(sourceHandle, threshold := 128.0) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_solarize",
        "Ptr", sourceHandle,
        "Double", threshold,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageSolarizeInto(sourceHandle, threshold, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_solarize_into",
        "Ptr", sourceHandle,
        "Double", threshold,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageColorize(sourceHandle, black, white, hasMid := false, mid := unset, blackpoint := 0, whitepoint := 255, midpoint := 127) {
    blackColor := PillowCBuffer(black)
    whiteColor := PillowCBuffer(white)
    midColor := hasMid ? PillowCBuffer(mid) : 0
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_colorize",
        "Ptr", sourceHandle,
        "Ptr", blackColor,
        "Ptr", whiteColor,
        "Int", hasMid,
        "Ptr", hasMid ? midColor.Ptr : 0,
        "Int", blackpoint,
        "Int", whitepoint,
        "Int", midpoint,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageColorizeInto(sourceHandle, black, white, targetHandle, hasMid := false, mid := unset, blackpoint := 0, whitepoint := 255, midpoint := 127) {
    blackColor := PillowCBuffer(black)
    whiteColor := PillowCBuffer(white)
    midColor := hasMid ? PillowCBuffer(mid) : 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_colorize_into",
        "Ptr", sourceHandle,
        "Ptr", blackColor,
        "Ptr", whiteColor,
        "Int", hasMid,
        "Ptr", hasMid ? midColor.Ptr : 0,
        "Int", blackpoint,
        "Int", whitepoint,
        "Int", midpoint,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageEqualize(sourceHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_equalize",
        "Ptr", sourceHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageEqualizeInto(sourceHandle, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_equalize_into",
        "Ptr", sourceHandle,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageEqualizeMasked(sourceHandle, maskHandle) {
    outHandle := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_equalize_masked",
            "Ptr", sourceHandle,
            "Ptr", maskHandle,
            "Ptr*", &outHandle,
            "Int"
        )
    } catch Error as err {
        AhkTest.Fail("Expected pillow_c_image_equalize_masked export: " err.Message)
    }
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageEqualizeMaskedInto(sourceHandle, maskHandle, targetHandle) {
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_equalize_masked_into",
            "Ptr", sourceHandle,
            "Ptr", maskHandle,
            "Ptr", targetHandle,
            "Int"
        )
    } catch Error as err {
        AhkTest.Fail("Expected pillow_c_image_equalize_masked_into export: " err.Message)
    }
    PillowCAssertStatus(status)
}

PillowCImageGetChannel(sourceHandle, channelIndex) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_get_channel",
        "Ptr", sourceHandle,
        "Int", channelIndex,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageGetChannelInto(sourceHandle, channelIndex, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_get_channel_into",
        "Ptr", sourceHandle,
        "Int", channelIndex,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImagePutAlphaValue(sourceHandle, alpha) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_put_alpha_value",
        "Ptr", sourceHandle,
        "UChar", alpha,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImagePutAlphaImage(sourceHandle, alphaHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_put_alpha_image",
        "Ptr", sourceHandle,
        "Ptr", alphaHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageConvertMode(sourceHandle, mode) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_convert_mode",
        "Ptr", sourceHandle,
        "Int", mode,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageConvertModeDither(sourceHandle, mode, dither) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_convert_mode_dither",
        "Ptr", sourceHandle,
        "Int", mode,
        "Int", dither,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageConvertMatrix(sourceHandle, mode, matrixValues) {
    matrix := PillowCDoubleBuffer(matrixValues)
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_convert_matrix",
        "Ptr", sourceHandle,
        "Int", mode,
        "Ptr", matrix,
        "UPtr", matrixValues.Length,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageConvertMatrixInto(sourceHandle, mode, matrixValues, targetHandle) {
    matrix := PillowCDoubleBuffer(matrixValues)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_convert_matrix_into",
        "Ptr", sourceHandle,
        "Int", mode,
        "Ptr", matrix,
        "UPtr", matrixValues.Length,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageConvertModeInto(sourceHandle, mode, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_convert_mode_into",
        "Ptr", sourceHandle,
        "Int", mode,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageQuantizePalette(sourceHandle, paletteHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_quantize_palette",
        "Ptr", sourceHandle,
        "Ptr", paletteHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageQuantize(sourceHandle, colors) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_quantize",
        "Ptr", sourceHandle,
        "Int", colors,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageRemapPalette(sourceHandle, destMap, sourcePalette := unset) {
    map := PillowCIntBuffer(destMap)
    palette := IsSet(sourcePalette) ? PillowCBuffer(sourcePalette) : 0
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_remap_palette",
        "Ptr", sourceHandle,
        "Ptr", map,
        "UPtr", destMap.Length,
        "Ptr", IsObject(palette) ? palette.Ptr : 0,
        "UPtr", IsObject(palette) ? palette.Size : 0,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageRemapPaletteInto(sourceHandle, destMap, targetHandle, sourcePalette := unset) {
    map := PillowCIntBuffer(destMap)
    palette := IsSet(sourcePalette) ? PillowCBuffer(sourcePalette) : 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_remap_palette_into",
        "Ptr", sourceHandle,
        "Ptr", map,
        "UPtr", destMap.Length,
        "Ptr", IsObject(palette) ? palette.Ptr : 0,
        "UPtr", IsObject(palette) ? palette.Size : 0,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImagePutPaletteRgb(sourceHandle, values) {
    data := PillowCBuffer(values)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_put_palette_rgb",
        "Ptr", sourceHandle,
        "Ptr", data,
        "UPtr", data.Size,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageGetPaletteRgb(sourceHandle) {
    required := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_get_palette_rgb",
        "Ptr", sourceHandle,
        "Ptr", 0,
        "UPtr", 0,
        "UPtr*", &required,
        "Int"
    )
    PillowCAssertStatus(status)
    out := Buffer(required, 0)
    if required = 0
        return []
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_get_palette_rgb",
        "Ptr", sourceHandle,
        "Ptr", out,
        "UPtr", out.Size,
        "UPtr*", &required,
        "Int"
    )
    PillowCAssertStatus(status)
    return PillowCBufferToArray(out)
}

PillowCImagePutPaletteRgba(sourceHandle, values, alphaMode := 1) {
    data := PillowCBuffer(values)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_put_palette_rgba",
        "Ptr", sourceHandle,
        "Ptr", data,
        "UPtr", data.Size,
        "Int", alphaMode,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImagePaletteAlphaMode(sourceHandle) {
    mode := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_palette_alpha_mode",
        "Ptr", sourceHandle,
        "Int*", &mode,
        "Int"
    )
    PillowCAssertStatus(status)
    return mode
}

PillowCImageGetPaletteRgba(sourceHandle) {
    required := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_get_palette_rgba",
        "Ptr", sourceHandle,
        "Ptr", 0,
        "UPtr", 0,
        "UPtr*", &required,
        "Int"
    )
    PillowCAssertStatus(status)
    out := Buffer(required, 0)
    if required = 0
        return []
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_get_palette_rgba",
        "Ptr", sourceHandle,
        "Ptr", out,
        "UPtr", out.Size,
        "UPtr*", &required,
        "Int"
    )
    PillowCAssertStatus(status)
    return PillowCBufferToArray(out)
}

PillowCImageHandleArray(handles) {
    buf := Buffer(handles.Length * A_PtrSize, 0)
    for index, handle in handles
        NumPut("Ptr", handle, buf, (index - 1) * A_PtrSize)
    return buf
}

PillowCImageMergeBands(mode, bandHandles) {
    bands := PillowCImageHandleArray(bandHandles)
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_merge_bands",
        "Int", mode,
        "Ptr", bands,
        "UPtr", bandHandles.Length,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageMergeBandsInto(mode, bandHandles, targetHandle) {
    bands := PillowCImageHandleArray(bandHandles)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_merge_bands_into",
        "Int", mode,
        "Ptr", bands,
        "UPtr", bandHandles.Length,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageSplitBands(sourceHandle, bandCount) {
    outHandles := Buffer(bandCount * A_PtrSize, 0)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_split_bands",
        "Ptr", sourceHandle,
        "Ptr", outHandles,
        "UPtr", bandCount,
        "Int"
    )
    PillowCAssertStatus(status)
    handles := []
    loop bandCount {
        handle := NumGet(outHandles, (A_Index - 1) * A_PtrSize, "Ptr")
        AhkTest.AssertTrue(handle != 0)
        handles.Push(handle)
    }
    return handles
}

PillowCImageRgbToLInto(sourceHandle, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_rgb_to_l_into",
        "Ptr", sourceHandle,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageAlphaCompositeRgba(dstHandle, srcHandle) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_alpha_composite_rgba",
        "Ptr", dstHandle,
        "Ptr", srcHandle,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageAlphaCompositeRgbaInto(dstHandle, srcHandle, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_alpha_composite_rgba_into",
        "Ptr", dstHandle,
        "Ptr", srcHandle,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageAlphaCompositeRgbaInPlace(dstHandle, srcHandle, destX, destY, sourceLeft, sourceTop, sourceRight, sourceBottom) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_alpha_composite_rgba_in_place",
        "Ptr", dstHandle,
        "Ptr", srcHandle,
        "Int", destX,
        "Int", destY,
        "Int", sourceLeft,
        "Int", sourceTop,
        "Int", sourceRight,
        "Int", sourceBottom,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageCrop(sourceHandle, left, top, right, bottom) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_crop",
        "Ptr", sourceHandle,
        "Int", left,
        "Int", top,
        "Int", right,
        "Int", bottom,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageCropInto(sourceHandle, left, top, right, bottom, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_crop_into",
        "Ptr", sourceHandle,
        "Int", left,
        "Int", top,
        "Int", right,
        "Int", bottom,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageExpand(sourceHandle, left, top, right, bottom, colorValues) {
    color := PillowCBuffer(colorValues)
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_expand",
        "Ptr", sourceHandle,
        "Int", left,
        "Int", top,
        "Int", right,
        "Int", bottom,
        "Ptr", color,
        "UPtr", color.Size,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageExpandInto(sourceHandle, left, top, right, bottom, colorValues, targetHandle) {
    color := PillowCBuffer(colorValues)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_expand_into",
        "Ptr", sourceHandle,
        "Int", left,
        "Int", top,
        "Int", right,
        "Int", bottom,
        "Ptr", color,
        "UPtr", color.Size,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageResize(sourceHandle, width, height, resample) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_resize",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Int", resample,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageResizeInto(sourceHandle, width, height, resample, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_resize_into",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Int", resample,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageResizeBox(sourceHandle, width, height, resample, box) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_resize_box",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Int", resample,
        "Double", box[1],
        "Double", box[2],
        "Double", box[3],
        "Double", box[4],
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageResizeReducingGap(sourceHandle, width, height, resample, box, reducingGap) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_resize_reducing_gap",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Int", resample,
        "Double", box[1],
        "Double", box[2],
        "Double", box[3],
        "Double", box[4],
        "Double", reducingGap,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageResizeBoxInto(sourceHandle, width, height, resample, box, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_resize_box_into",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Int", resample,
        "Double", box[1],
        "Double", box[2],
        "Double", box[3],
        "Double", box[4],
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageReduce(sourceHandle, xscale, yscale, left, top, right, bottom) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_reduce",
        "Ptr", sourceHandle,
        "Int", xscale,
        "Int", yscale,
        "Int", left,
        "Int", top,
        "Int", right,
        "Int", bottom,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageReduceInto(sourceHandle, xscale, yscale, left, top, right, bottom, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_reduce_into",
        "Ptr", sourceHandle,
        "Int", xscale,
        "Int", yscale,
        "Int", left,
        "Int", top,
        "Int", right,
        "Int", bottom,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCKernelWeights(values) {
    buf := Buffer(values.Length * 8, 0)
    for index, value in values
        NumPut("Double", value, buf, (index - 1) * 8)
    return buf
}

PillowCImageFilterKernel(sourceHandle, kernelWidth, kernelHeight, kernelValues, scale := 1.0, offset := 0.0) {
    kernel := PillowCKernelWeights(kernelValues)
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_kernel",
        "Ptr", sourceHandle,
        "Int", kernelWidth,
        "Int", kernelHeight,
        "Ptr", kernel,
        "UPtr", kernelValues.Length,
        "Double", scale,
        "Double", offset,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageFilterKernelInto(sourceHandle, kernelWidth, kernelHeight, kernelValues, scale, offset, targetHandle) {
    kernel := PillowCKernelWeights(kernelValues)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_kernel_into",
        "Ptr", sourceHandle,
        "Int", kernelWidth,
        "Int", kernelHeight,
        "Ptr", kernel,
        "UPtr", kernelValues.Length,
        "Double", scale,
        "Double", offset,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageFilterRank(sourceHandle, size, rank) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_rank",
        "Ptr", sourceHandle,
        "Int", size,
        "Int", rank,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageFilterRankInto(sourceHandle, size, rank, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_rank_into",
        "Ptr", sourceHandle,
        "Int", size,
        "Int", rank,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageFilterMode(sourceHandle, size := 3) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_mode",
        "Ptr", sourceHandle,
        "Int", size,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageFilterModeInto(sourceHandle, size, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_mode_into",
        "Ptr", sourceHandle,
        "Int", size,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageFilterBoxBlur(sourceHandle, xRadius, yRadius) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_box_blur",
        "Ptr", sourceHandle,
        "Double", xRadius,
        "Double", yRadius,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageFilterBoxBlurInto(sourceHandle, xRadius, yRadius, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_box_blur_into",
        "Ptr", sourceHandle,
        "Double", xRadius,
        "Double", yRadius,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageFilterGaussianBlur(sourceHandle, xRadius, yRadius) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_gaussian_blur",
        "Ptr", sourceHandle,
        "Double", xRadius,
        "Double", yRadius,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageFilterGaussianBlurInto(sourceHandle, xRadius, yRadius, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_gaussian_blur_into",
        "Ptr", sourceHandle,
        "Double", xRadius,
        "Double", yRadius,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageFilterUnsharpMask(sourceHandle, radius, percent, threshold) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_unsharp_mask",
        "Ptr", sourceHandle,
        "Double", radius,
        "Int", percent,
        "Int", threshold,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageFilterUnsharpMaskInto(sourceHandle, radius, percent, threshold, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_unsharp_mask_into",
        "Ptr", sourceHandle,
        "Double", radius,
        "Int", percent,
        "Int", threshold,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageFilterColor3DLut(sourceHandle, targetMode, tableChannels, size, tableValues) {
    table := PillowCDoubleBuffer(tableValues)
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_color_3d_lut",
        "Ptr", sourceHandle,
        "Int", targetMode,
        "Int", tableChannels,
        "Int", size[1],
        "Int", size[2],
        "Int", size[3],
        "Ptr", table,
        "UPtr", tableValues.Length,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageFilterColor3DLutInto(sourceHandle, targetMode, tableChannels, size, tableValues, targetHandle) {
    table := PillowCDoubleBuffer(tableValues)
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_filter_color_3d_lut_into",
        "Ptr", sourceHandle,
        "Int", targetMode,
        "Int", tableChannels,
        "Int", size[1],
        "Int", size[2],
        "Int", size[3],
        "Ptr", table,
        "UPtr", tableValues.Length,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCAffineMatrix(values) {
    buf := Buffer(6 * 8, 0)
    for index, value in values
        NumPut("Double", value, buf, (index - 1) * 8)
    return buf
}

PillowCPerspectiveCoefficients(values) {
    buf := Buffer(8 * 8, 0)
    for index, value in values
        NumPut("Double", value, buf, (index - 1) * 8)
    return buf
}

PillowCQuadCorners(values) {
    buf := Buffer(8 * 8, 0)
    for index, value in values
        NumPut("Double", value, buf, (index - 1) * 8)
    return buf
}

PillowCMeshBoxes(values) {
    buf := Buffer(values.Length * 4, 0)
    for index, value in values
        NumPut("Int", value, buf, (index - 1) * 4)
    return buf
}

PillowCMeshQuads(values) {
    buf := Buffer(values.Length * 8, 0)
    for index, value in values
        NumPut("Double", value, buf, (index - 1) * 8)
    return buf
}

PillowCImageTransformAffine(sourceHandle, width, height, matrixValues, resample := 0, fillValues := 0) {
    matrix := PillowCAffineMatrix(matrixValues)
    fill := IsObject(fillValues) ? PillowCBuffer(fillValues) : 0
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_transform_affine",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Ptr", matrix,
        "Int", resample,
        "Ptr", IsObject(fill) ? fill.Ptr : 0,
        "UPtr", IsObject(fill) ? fill.Size : 0,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageTransformPerspective(sourceHandle, width, height, coefficientValues, resample := 0, fillValues := 0) {
    coefficients := PillowCPerspectiveCoefficients(coefficientValues)
    fill := IsObject(fillValues) ? PillowCBuffer(fillValues) : 0
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_transform_perspective",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Ptr", coefficients,
        "Int", resample,
        "Ptr", IsObject(fill) ? fill.Ptr : 0,
        "UPtr", IsObject(fill) ? fill.Size : 0,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageTransformQuad(sourceHandle, width, height, cornerValues, resample := 0, fillValues := 0) {
    corners := PillowCQuadCorners(cornerValues)
    fill := IsObject(fillValues) ? PillowCBuffer(fillValues) : 0
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_transform_quad",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Ptr", corners,
        "Int", resample,
        "Ptr", IsObject(fill) ? fill.Ptr : 0,
        "UPtr", IsObject(fill) ? fill.Size : 0,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageTransformMesh(sourceHandle, width, height, boxValues, quadValues, resample := 0, fillValues := 0) {
    boxes := PillowCMeshBoxes(boxValues)
    quads := PillowCMeshQuads(quadValues)
    fill := IsObject(fillValues) ? PillowCBuffer(fillValues) : 0
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_transform_mesh",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Ptr", boxes,
        "Ptr", quads,
        "UPtr", boxValues.Length // 4,
        "Int", resample,
        "Ptr", IsObject(fill) ? fill.Ptr : 0,
        "UPtr", IsObject(fill) ? fill.Size : 0,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageTransformAffineInto(sourceHandle, width, height, matrixValues, targetHandle, resample := 0, fillValues := 0) {
    matrix := PillowCAffineMatrix(matrixValues)
    fill := IsObject(fillValues) ? PillowCBuffer(fillValues) : 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_transform_affine_into",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Ptr", matrix,
        "Int", resample,
        "Ptr", IsObject(fill) ? fill.Ptr : 0,
        "UPtr", IsObject(fill) ? fill.Size : 0,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageTransformPerspectiveInto(sourceHandle, width, height, coefficientValues, targetHandle, resample := 0, fillValues := 0) {
    coefficients := PillowCPerspectiveCoefficients(coefficientValues)
    fill := IsObject(fillValues) ? PillowCBuffer(fillValues) : 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_transform_perspective_into",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Ptr", coefficients,
        "Int", resample,
        "Ptr", IsObject(fill) ? fill.Ptr : 0,
        "UPtr", IsObject(fill) ? fill.Size : 0,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageTransformQuadInto(sourceHandle, width, height, cornerValues, targetHandle, resample := 0, fillValues := 0) {
    corners := PillowCQuadCorners(cornerValues)
    fill := IsObject(fillValues) ? PillowCBuffer(fillValues) : 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_transform_quad_into",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Ptr", corners,
        "Int", resample,
        "Ptr", IsObject(fill) ? fill.Ptr : 0,
        "UPtr", IsObject(fill) ? fill.Size : 0,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageTransformMeshInto(sourceHandle, width, height, boxValues, quadValues, targetHandle, resample := 0, fillValues := 0) {
    boxes := PillowCMeshBoxes(boxValues)
    quads := PillowCMeshQuads(quadValues)
    fill := IsObject(fillValues) ? PillowCBuffer(fillValues) : 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_transform_mesh_into",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Ptr", boxes,
        "Ptr", quads,
        "UPtr", boxValues.Length // 4,
        "Int", resample,
        "Ptr", IsObject(fill) ? fill.Ptr : 0,
        "UPtr", IsObject(fill) ? fill.Size : 0,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageRotate(sourceHandle, angle, resample := 0, expand := false, centerX := 0.0, centerY := 0.0, hasCenter := false, translateX := 0.0, translateY := 0.0, hasTranslate := false, fillValues := 0) {
    fill := IsObject(fillValues) ? PillowCBuffer(fillValues) : 0
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_rotate",
        "Ptr", sourceHandle,
        "Double", angle,
        "Int", resample,
        "Int", expand,
        "Double", centerX,
        "Double", centerY,
        "Int", hasCenter,
        "Double", translateX,
        "Double", translateY,
        "Int", hasTranslate,
        "Ptr", IsObject(fill) ? fill.Ptr : 0,
        "UPtr", IsObject(fill) ? fill.Size : 0,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageRotateInto(sourceHandle, angle, targetHandle, resample := 0, expand := false, centerX := 0.0, centerY := 0.0, hasCenter := false, translateX := 0.0, translateY := 0.0, hasTranslate := false, fillValues := 0) {
    fill := IsObject(fillValues) ? PillowCBuffer(fillValues) : 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_rotate_into",
        "Ptr", sourceHandle,
        "Double", angle,
        "Int", resample,
        "Int", expand,
        "Double", centerX,
        "Double", centerY,
        "Int", hasCenter,
        "Double", translateX,
        "Double", translateY,
        "Int", hasTranslate,
        "Ptr", IsObject(fill) ? fill.Ptr : 0,
        "UPtr", IsObject(fill) ? fill.Size : 0,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImageContain(sourceHandle, width, height, resample) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_contain",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Int", resample,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageCover(sourceHandle, width, height, resample) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_cover",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Int", resample,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImagePad(sourceHandle, width, height, resample, colorValues, centerX := 0.5, centerY := 0.5) {
    color := PillowCBuffer(colorValues)
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_pad",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Int", resample,
        "Ptr", color,
        "UPtr", color.Size,
        "Double", centerX,
        "Double", centerY,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageFit(sourceHandle, width, height, resample, bleed := 0.0, centerX := 0.5, centerY := 0.5) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_fit",
        "Ptr", sourceHandle,
        "Int", width,
        "Int", height,
        "Int", resample,
        "Double", bleed,
        "Double", centerX,
        "Double", centerY,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageAutocontrast(sourceHandle, lowCutoff := 0.0, highCutoff := 0.0, ignoreCount := 0, ignoreValues := 0, maskHandle := 0, preserveTone := false) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_autocontrast",
        "Ptr", sourceHandle,
        "Double", lowCutoff,
        "Double", highCutoff,
        "Ptr", ignoreValues,
        "UPtr", ignoreCount,
        "Ptr", maskHandle,
        "Int", preserveTone,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageAutocontrastInto(sourceHandle, lowCutoff, highCutoff, targetHandle, ignoreCount := 0, ignoreValues := 0, maskHandle := 0, preserveTone := false) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_autocontrast_into",
        "Ptr", sourceHandle,
        "Double", lowCutoff,
        "Double", highCutoff,
        "Ptr", ignoreValues,
        "UPtr", ignoreCount,
        "Ptr", maskHandle,
        "Int", preserveTone,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImagePaste(targetHandle, sourceHandle, left, top) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_paste",
        "Ptr", targetHandle,
        "Ptr", sourceHandle,
        "Int", left,
        "Int", top,
        "Int"
    )
    PillowCAssertStatus(status)
}

PillowCImagePasteMasked(targetHandle, sourceHandle, left, top, maskHandle) {
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_paste_masked",
            "Ptr", targetHandle,
            "Ptr", sourceHandle,
            "Int", left,
            "Int", top,
            "Ptr", maskHandle,
            "Int"
        )
    } catch Error as err {
        AhkTest.Fail("Expected pillow_c_image_paste_masked export: " err.Message)
    }
    PillowCAssertStatus(status)
}

PillowCImagePasteColor(targetHandle, values, left, top, right, bottom, maskHandle := 0) {
    color := PillowCBuffer(values)
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_paste_color",
            "Ptr", targetHandle,
            "Ptr", color,
            "UPtr", color.Size,
            "Int", left,
            "Int", top,
            "Int", right,
            "Int", bottom,
            "Ptr", maskHandle,
            "Int"
        )
    } catch Error as err {
        AhkTest.Fail("Expected pillow_c_image_paste_color export: " err.Message)
    }
    PillowCAssertStatus(status)
}

PillowCImageTranspose(sourceHandle, method) {
    outHandle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_transpose",
        "Ptr", sourceHandle,
        "Int", method,
        "Ptr*", &outHandle,
        "Int"
    )
    PillowCAssertStatus(status)
    AhkTest.AssertTrue(outHandle != 0)
    return outHandle
}

PillowCImageTransposeInto(sourceHandle, method, targetHandle) {
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_transpose_into",
        "Ptr", sourceHandle,
        "Int", method,
        "Ptr", targetHandle,
        "Int"
    )
    PillowCAssertStatus(status)
}

AhkTest.Test("pillow_c exposes ABI version for AHK wrapper compatibility", (*) =>
    AhkTest.AssertEqual([0, 1, 0], PillowCAbiVersion()))

AhkTest.Test("pillow_c exposes stable status messages for wrapper exceptions", (*) => (
    msg := PillowCStatusMessage(-3),
    AhkTest.AssertEqual("invalid argument", msg.Text),
    AhkTest.AssertEqual(StrLen("invalid argument") + 1, msg.Required)
))

AhkTest.Test("pillow_c maps core Pillow mode names to native mode ids", (*) => (
    AhkTest.AssertEqual(5, PillowCModeFromString("1")),
    AhkTest.AssertEqual(1, PillowCModeFromString("L")),
    AhkTest.AssertEqual(2, PillowCModeFromString("LA")),
    AhkTest.AssertEqual(3, PillowCModeFromString("RGB")),
    AhkTest.AssertEqual(4, PillowCModeFromString("RGBA")),
    AhkTest.AssertEqual(6, PillowCModeFromString("P")),
    AhkTest.AssertEqual(7, PillowCModeFromString("CMYK"))
))

AhkTest.Test("pillow_c maps native mode ids back to Pillow mode names", (*) => (
    one := PillowCModeName(5),
    l := PillowCModeName(1),
    la := PillowCModeName(2),
    rgb := PillowCModeName(3),
    rgba := PillowCModeName(4),
    p := PillowCModeName(6),
    cmyk := PillowCModeName(7),
    AhkTest.AssertEqual("1", one.Text),
    AhkTest.AssertEqual(2, one.Required),
    AhkTest.AssertEqual("L", l.Text),
    AhkTest.AssertEqual(2, l.Required),
    AhkTest.AssertEqual("LA", la.Text),
    AhkTest.AssertEqual(3, la.Required),
    AhkTest.AssertEqual("RGB", rgb.Text),
    AhkTest.AssertEqual(4, rgb.Required),
    AhkTest.AssertEqual("RGBA", rgba.Text),
    AhkTest.AssertEqual(5, rgba.Required),
    AhkTest.AssertEqual("P", p.Text),
    AhkTest.AssertEqual(2, p.Required),
    AhkTest.AssertEqual("CMYK", cmyk.Text),
    AhkTest.AssertEqual(5, cmyk.Required)
))

AhkTest.Test("pillow_c_blend_u8 matches Pillow L alpha 0.25", (*) =>
    AhkTest.AssertEqual([63, 32, 112, 187], PillowCBlend([0, 10, 128, 250], [255, 100, 64, 0], 0.25)))

AhkTest.Test("pillow_c_blend_u8 matches Pillow L alpha 0.5", (*) =>
    AhkTest.AssertEqual([127, 55, 96, 125], PillowCBlend([0, 10, 128, 250], [255, 100, 64, 0], 0.5)))

AhkTest.Test("pillow_c_blend_u8 matches Pillow RGB alpha 0.25", (*) =>
    AhkTest.AssertEqual([70, 75, 80, 75, 83, 90], PillowCBlend([10, 20, 30, 100, 110, 120], [250, 240, 230, 1, 2, 3], 0.25)))

AhkTest.Test("pillow_c_blend_u8 clips Pillow extrapolation above one", (*) =>
    AhkTest.AssertEqual([255, 255, 255, 0, 0, 0], PillowCBlend([10, 20, 30, 100, 110, 120], [250, 240, 230, 1, 2, 3], 1.25)))

AhkTest.Test("pillow_c_blend_u8 clips Pillow extrapolation below zero", (*) =>
    AhkTest.AssertEqual([0, 0, 0], PillowCBlend([10, 20, 30], [250, 240, 230], -0.25)))

AhkTest.Test("pillow_c_rgb_to_l matches Pillow ITU-R 601-2 luma", (*) =>
    AhkTest.AssertEqual([0, 255, 76, 150, 29, 18, 71, 150], PillowCRgbToL([
        0, 0, 0,
        255, 255, 255,
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        10, 20, 30,
        123, 45, 67,
        250, 128, 1,
    ])))

AhkTest.Test("pillow_c_alpha_composite_rgba matches Pillow source-over", (*) =>
    AhkTest.AssertEqual([
        0, 0, 0, 0,
        200, 210, 220, 255,
        67, 77, 87, 192,
        24, 222, 30, 208,
        141, 135, 130, 9,
    ], PillowCAlphaCompositeRgba([
        0, 0, 0, 0,
        10, 20, 30, 255,
        100, 110, 120, 128,
        200, 10, 30, 64,
        1, 2, 3, 4,
    ], [
        255, 0, 0, 0,
        200, 210, 220, 255,
        50, 60, 70, 128,
        10, 240, 30, 192,
        250, 240, 230, 5,
    ])))

PillowCTestImageHandleOwnsRgbBytesAndMetadata(*) {
    handle := PillowCCreateImage(2, 1, 3)
    try {
        PillowCImageSetBytes(handle, [10, 20, 30, 100, 110, 120])
        AhkTest.AssertEqual(2, PillowCImageInt(handle, "pillow_c_image_width"))
        AhkTest.AssertEqual(1, PillowCImageInt(handle, "pillow_c_image_height"))
        AhkTest.AssertEqual(3, PillowCImageInt(handle, "pillow_c_image_channels"))
        AhkTest.AssertEqual(6, PillowCImageInt(handle, "pillow_c_image_stride"))
        AhkTest.AssertEqual(6, PillowCImageSize(handle))
        AhkTest.AssertEqual([10, 20, 30, 100, 110, 120], PillowCImageToArray(handle, 6))
    } finally {
        PillowCFreeImage(handle)
    }
}

AhkTest.Test("pillow_c image handle owns RGB bytes and metadata", PillowCTestImageHandleOwnsRgbBytesAndMetadata)

PillowCTestImageHandleOwnsModeMetadata(*) {
    one := PillowCCreateImageMode(9, 2, 5)
    l := PillowCCreateImageMode(3, 2, 1)
    la := PillowCCreateImageMode(3, 2, 2)
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    try {
        AhkTest.AssertEqual(5, PillowCImageMode(one))
        AhkTest.AssertEqual(1, PillowCImageInt(one, "pillow_c_image_channels"))
        AhkTest.AssertEqual(9, PillowCImageInt(one, "pillow_c_image_stride"))
        AhkTest.AssertEqual(18, PillowCImageSize(one))
        AhkTest.AssertEqual(1, PillowCImageMode(l))
        AhkTest.AssertEqual(1, PillowCImageInt(l, "pillow_c_image_channels"))
        AhkTest.AssertEqual(6, PillowCImageSize(l))
        AhkTest.AssertEqual(2, PillowCImageMode(la))
        AhkTest.AssertEqual(2, PillowCImageInt(la, "pillow_c_image_channels"))
        AhkTest.AssertEqual(12, PillowCImageSize(la))
        AhkTest.AssertEqual(3, PillowCImageMode(rgb))
        AhkTest.AssertEqual(3, PillowCImageInt(rgb, "pillow_c_image_channels"))
        AhkTest.AssertEqual(6, PillowCImageSize(rgb))
        AhkTest.AssertEqual(4, PillowCImageMode(rgba))
        AhkTest.AssertEqual(4, PillowCImageInt(rgba, "pillow_c_image_channels"))
        AhkTest.AssertEqual(8, PillowCImageSize(rgba))
    } finally {
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(la)
        PillowCFreeImage(l)
        PillowCFreeImage(one)
    }
}

AhkTest.Test("pillow_c image handle owns Pillow mode metadata", PillowCTestImageHandleOwnsModeMetadata)

PillowCTestImageHandleOwnsCmykBytesAndOperations(*) {
    cmyk := PillowCCreateImageMode(2, 1, 7)
    inverted := 0
    try {
        AhkTest.AssertEqual(7, PillowCImageMode(cmyk))
        AhkTest.AssertEqual(4, PillowCImageInt(cmyk, "pillow_c_image_channels"))
        AhkTest.AssertEqual(8, PillowCImageInt(cmyk, "pillow_c_image_stride"))
        AhkTest.AssertEqual(8, PillowCImageSize(cmyk))

        PillowCImageSetRawBytes(cmyk, [0, 10, 20, 30, 255, 128, 64, 0], "CMYK")
        AhkTest.AssertEqual([0, 10, 20, 30, 255, 128, 64, 0], PillowCImageToArray(cmyk, 8))
        AhkTest.AssertEqual([0, 10, 20, 30, 255, 128, 64, 0], PillowCImageGetRawBytes(cmyk, "CMYK"))
        AhkTest.AssertEqual([255, 128, 64, 0], PillowCImageGetPixel(cmyk, 1, 0, 4))

        PillowCImagePutData(cmyk, [9, 8, 7, 6], 1)
        AhkTest.AssertEqual([9, 8, 7, 6, 255, 128, 64, 0], PillowCImageToArray(cmyk, 8))

        PillowCImagePutPixel(cmyk, 0, 0, [1, 2, 3, 4])
        AhkTest.AssertEqual([1, 2, 3, 4, 255, 128, 64, 0], PillowCImageToArray(cmyk, 8))

        inverted := PillowCImageChopsInvert(cmyk)
        AhkTest.AssertEqual(7, PillowCImageMode(inverted))
        AhkTest.AssertEqual([254, 253, 252, 251, 0, 127, 191, 255], PillowCImageToArray(inverted, 8))
    } finally {
        if inverted
            PillowCFreeImage(inverted)
        PillowCFreeImage(cmyk)
    }
}

AhkTest.Test("pillow_c image handle owns CMYK bytes and operations", PillowCTestImageHandleOwnsCmykBytesAndOperations)

PillowCTestImageCreateKeepsLegacyChannelModeMapping(*) {
    l := PillowCCreateImage(3, 2, 1)
    la := PillowCCreateImage(3, 2, 2)
    rgb := PillowCCreateImage(2, 1, 3)
    rgba := PillowCCreateImage(2, 1, 4)
    try {
        AhkTest.AssertEqual(1, PillowCImageMode(l))
        AhkTest.AssertEqual(2, PillowCImageMode(la))
        AhkTest.AssertEqual(3, PillowCImageMode(rgb))
        AhkTest.AssertEqual(4, PillowCImageMode(rgba))
    } finally {
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(la)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c legacy channel create maps to core Pillow modes", PillowCTestImageCreateKeepsLegacyChannelModeMapping)

PillowCTestImageCreateModeRejectsUnsupportedMode(*) {
    handle := 0
    status := DllCall(
        PillowCDllPath() "\pillow_c_image_create_mode",
        "Int", 1,
        "Int", 1,
        "Int", 999,
        "Ptr*", &handle,
        "Int"
    )
    AhkTest.AssertEqual(-3, status)
    AhkTest.AssertEqual(0, handle)
}

AhkTest.Test("pillow_c image create_mode rejects unsupported mode ids", PillowCTestImageCreateModeRejectsUnsupportedMode)

PillowCTestImageLinearGradientMatchesPillowCoreModes(*) {
    one := 0
    l := 0
    p := 0
    target := PillowCCreateImageMode(256, 256, 1)
    wrongTarget := PillowCCreateImageMode(256, 256, 3)
    outHandle := 0
    try {
        one := PillowCImageLinearGradient(5)
        l := PillowCImageLinearGradient(1)
        p := PillowCImageLinearGradient(6)

        for image in [one, l, p] {
            AhkTest.AssertEqual(256, PillowCImageInt(image, "pillow_c_image_width"))
            AhkTest.AssertEqual(256, PillowCImageInt(image, "pillow_c_image_height"))
            AhkTest.AssertEqual(1, PillowCImageInt(image, "pillow_c_image_channels"))
            for y in [0, 1, 127, 128, 255] {
                AhkTest.AssertEqual([y], PillowCImageGetPixel(image, 0, y, 1))
                AhkTest.AssertEqual([y], PillowCImageGetPixel(image, 255, y, 1))
            }
        }
        AhkTest.AssertEqual(5, PillowCImageMode(one))
        AhkTest.AssertEqual(1, PillowCImageMode(l))
        AhkTest.AssertEqual(6, PillowCImageMode(p))

        status := DllCall(PillowCDllPath() "\pillow_c_image_linear_gradient_into", "Int", 1, "Ptr", target, "Int")
        PillowCAssertStatus(status)
        AhkTest.AssertEqual([0], PillowCImageGetPixel(target, 0, 0, 1))
        AhkTest.AssertEqual([255], PillowCImageGetPixel(target, 128, 255, 1))

        status := DllCall(PillowCDllPath() "\pillow_c_image_linear_gradient", "Int", 3, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(PillowCDllPath() "\pillow_c_image_linear_gradient_into", "Int", 1, "Ptr", wrongTarget, "Int")
        AhkTest.AssertEqual(-5, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        for image in [wrongTarget, target, p, l, one] {
            if image
                PillowCFreeImage(image)
        }
    }
}

AhkTest.Test("pillow_c image linear_gradient matches Pillow core modes", PillowCTestImageLinearGradientMatchesPillowCoreModes)

PillowCTestImageRadialGradientMatchesPillowCoreModes(*) {
    one := 0
    l := 0
    p := 0
    target := PillowCCreateImageMode(256, 256, 1)
    wrongTarget := PillowCCreateImageMode(256, 256, 3)
    outHandle := 0
    try {
        one := PillowCImageRadialGradient(5)
        l := PillowCImageRadialGradient(1)
        p := PillowCImageRadialGradient(6)

        for image in [one, l, p] {
            AhkTest.AssertEqual(256, PillowCImageInt(image, "pillow_c_image_width"))
            AhkTest.AssertEqual(256, PillowCImageInt(image, "pillow_c_image_height"))
            AhkTest.AssertEqual(1, PillowCImageInt(image, "pillow_c_image_channels"))
            AhkTest.AssertEqual([0], PillowCImageGetPixel(image, 128, 128, 1))
            AhkTest.AssertEqual([1], PillowCImageGetPixel(image, 129, 128, 1))
            AhkTest.AssertEqual([2], PillowCImageGetPixel(image, 127, 127, 1))
            AhkTest.AssertEqual([90], PillowCImageGetPixel(image, 64, 128, 1))
            AhkTest.AssertEqual([179], PillowCImageGetPixel(image, 255, 128, 1))
            AhkTest.AssertEqual([254], PillowCImageGetPixel(image, 255, 255, 1))
            AhkTest.AssertEqual([255], PillowCImageGetPixel(image, 0, 0, 1))
        }
        AhkTest.AssertEqual(5, PillowCImageMode(one))
        AhkTest.AssertEqual(1, PillowCImageMode(l))
        AhkTest.AssertEqual(6, PillowCImageMode(p))

        status := DllCall(PillowCDllPath() "\pillow_c_image_radial_gradient_into", "Int", 1, "Ptr", target, "Int")
        PillowCAssertStatus(status)
        AhkTest.AssertEqual([0], PillowCImageGetPixel(target, 128, 128, 1))
        AhkTest.AssertEqual([255], PillowCImageGetPixel(target, 0, 0, 1))

        status := DllCall(PillowCDllPath() "\pillow_c_image_radial_gradient", "Int", 3, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(PillowCDllPath() "\pillow_c_image_radial_gradient_into", "Int", 1, "Ptr", wrongTarget, "Int")
        AhkTest.AssertEqual(-5, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        for image in [wrongTarget, target, p, l, one] {
            if image
                PillowCFreeImage(image)
        }
    }
}

AhkTest.Test("pillow_c image radial_gradient matches Pillow core modes", PillowCTestImageRadialGradientMatchesPillowCoreModes)

PillowCTestImageEffectMandelbrotMatchesPillow(*) {
    first := 0
    second := 0
    empty := 0
    outHandle := 0
    try {
        first := PillowCImageEffectMandelbrot(4, 3, [-2.0, -1.0, 1.0, 1.0], 10)
        second := PillowCImageEffectMandelbrot(5, 4, [-2.0, -1.25, 0.75, 1.25], 20)
        empty := PillowCImageEffectMandelbrot(0, 1, [-2.0, -1.0, 1.0, 1.0], 10)

        AhkTest.AssertEqual(1, PillowCImageMode(first))
        AhkTest.AssertEqual(4, PillowCImageInt(first, "pillow_c_image_width"))
        AhkTest.AssertEqual(3, PillowCImageInt(first, "pillow_c_image_height"))
        AhkTest.AssertEqual([76, 102, 0, 102, 0, 0, 0, 102, 76, 102, 0, 102], PillowCImageToArray(first, 12))
        AhkTest.AssertEqual([38, 51, 51, 63, 51, 51, 102, 0, 0, 63, 51, 102, 0, 0, 63, 38, 51, 51, 63, 51], PillowCImageToArray(second, 20))
        AhkTest.AssertEqual(0, PillowCImageInt(empty, "pillow_c_image_width"))
        AhkTest.AssertEqual(1, PillowCImageInt(empty, "pillow_c_image_height"))
        AhkTest.AssertEqual([], PillowCImageToArray(empty, 0))

        badExtent := PillowCDoubleBuffer([1.0, 0.0, -1.0, 1.0])
        status := DllCall(PillowCDllPath() "\pillow_c_image_effect_mandelbrot", "Int", 2, "Int", 2, "Ptr", badExtent, "Int", 10, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        goodExtent := PillowCDoubleBuffer([-1.0, 0.0, 1.0, 1.0])
        status := DllCall(PillowCDllPath() "\pillow_c_image_effect_mandelbrot", "Int", 2, "Int", 2, "Ptr", goodExtent, "Int", 1, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        for image in [empty, second, first] {
            if image
                PillowCFreeImage(image)
        }
    }
}

AhkTest.Test("pillow_c image effect_mandelbrot matches Pillow", PillowCTestImageEffectMandelbrotMatchesPillow)

PillowCTestImageEffectNoiseMatchesPillow(*) {
    first := 0
    zeroSigma := 0
    empty := 0
    outHandle := 0
    try {
        PillowCTestSrand(1)
        first := PillowCImageEffectNoise(4, 3, 12.5)
        zeroSigma := PillowCImageEffectNoise(3, 1, 0.0)
        empty := PillowCImageEffectNoise(0, 1, 12.5)

        AhkTest.AssertEqual(1, PillowCImageMode(first))
        AhkTest.AssertEqual(4, PillowCImageInt(first, "pillow_c_image_width"))
        AhkTest.AssertEqual(3, PillowCImageInt(first, "pillow_c_image_height"))
        AhkTest.AssertEqual([121, 160, 124, 137, 125, 151, 118, 124, 131, 141, 147, 115], PillowCImageToArray(first, 12))
        AhkTest.AssertEqual([128, 128, 128], PillowCImageToArray(zeroSigma, 3))
        AhkTest.AssertEqual(0, PillowCImageInt(empty, "pillow_c_image_width"))
        AhkTest.AssertEqual(1, PillowCImageInt(empty, "pillow_c_image_height"))
        AhkTest.AssertEqual([], PillowCImageToArray(empty, 0))

        status := DllCall(PillowCDllPath() "\pillow_c_image_effect_noise", "Int", -1, "Int", 2, "Double", 12.5, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        for image in [empty, zeroSigma, first] {
            if image
                PillowCFreeImage(image)
        }
    }
}

AhkTest.Test("pillow_c image effect_noise matches Pillow seeded rand stream", PillowCTestImageEffectNoiseMatchesPillow)

PillowCTestImageEffectSpreadMatchesPillowCore(*) {
    l := PillowCCreateImageMode(4, 3, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    p := PillowCCreateImageMode(3, 2, 6)
    lOut := 0
    rgbOut := 0
    pOut := 0
    identity := 0
    outHandle := 0
    try {
        PillowCImageSetBytes(l, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11])
        PillowCImageSetBytes(rgb, [
            0, 1, 2, 3, 4, 5, 6, 7, 8,
            9, 10, 11, 12, 13, 14, 15, 16, 17,
        ])
        PillowCImageSetBytes(p, [0, 1, 2, 3, 4, 5])
        PillowCImagePutPaletteRgb(p, [10, 20, 30, 40, 50, 60, 70, 80, 90])

        PillowCTestSrand(1)
        lOut := PillowCImageEffectSpread(l, 2)
        PillowCTestSrand(1)
        rgbOut := PillowCImageEffectSpread(rgb, 2)
        PillowCTestSrand(1)
        pOut := PillowCImageEffectSpread(p, 2)
        identity := PillowCImageEffectSpread(l, 0)

        AhkTest.AssertEqual(1, PillowCImageMode(lOut))
        AhkTest.AssertEqual([0, 1, 2, 3, 8, 9, 10, 7, 4, 5, 11, 10], PillowCImageToArray(lOut, 12))
        AhkTest.AssertEqual(3, PillowCImageMode(rgbOut))
        AhkTest.AssertEqual([
            12, 13, 14, 3, 4, 5, 6, 7, 8,
            9, 10, 11, 0, 1, 2, 15, 16, 17,
        ], PillowCImageToArray(rgbOut, 18))
        AhkTest.AssertEqual(6, PillowCImageMode(pOut))
        AhkTest.AssertEqual([4, 1, 2, 3, 0, 5], PillowCImageToArray(pOut, 6))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90], PillowCImageGetPaletteRgb(pOut))
        AhkTest.AssertEqual([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11], PillowCImageToArray(identity, 12))

        status := DllCall(PillowCDllPath() "\pillow_c_image_effect_spread", "Ptr", l, "Int", -1, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        for image in [identity, pOut, rgbOut, lOut, p, rgb, l] {
            if image
                PillowCFreeImage(image)
        }
    }
}

AhkTest.Test("pillow_c image effect_spread matches Pillow core", PillowCTestImageEffectSpreadMatchesPillowCore)

PillowCTestImageSaveBmpMatchesPillowRgb(*) {
    image := PillowCCreateImageMode(2, 2, 3)
    path := PillowCTempBmpPath("save-rgb")
    try {
        PillowCImageSetBytes(image, [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120])
        PillowCImageSaveBmp(image, path)

        AhkTest.AssertEqual([
            66, 77, 70, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,
            40, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 1, 0, 24, 0,
            0, 0, 0, 0, 16, 0, 0, 0, 196, 14, 0, 0, 196, 14, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            90, 80, 70, 120, 110, 100, 0, 0,
            30, 20, 10, 60, 50, 40, 0, 0
        ], PillowCReadFileBytes(path))
    } finally {
        PillowCDeleteFile(path)
        PillowCFreeImage(image)
    }
}

AhkTest.Test("pillow_c image save_bmp matches Pillow RGB bytes", PillowCTestImageSaveBmpMatchesPillowRgb)

PillowCTestImageOpenBmpReadsPillowRgb(*) {
    path := PillowCTempBmpPath("open-rgb")
    image := 0
    try {
        PillowCWriteFileBytes(path, [
            66, 77, 70, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,
            40, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 1, 0, 24, 0,
            0, 0, 0, 0, 16, 0, 0, 0, 196, 14, 0, 0, 196, 14, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            90, 80, 70, 120, 110, 100, 0, 0,
            30, 20, 10, 60, 50, 40, 0, 0
        ])

        image := PillowCImageOpenBmp(path)

        AhkTest.AssertEqual(3, PillowCImageMode(image))
        AhkTest.AssertEqual([2, 2], [PillowCImageInt(image, "pillow_c_image_width"), PillowCImageInt(image, "pillow_c_image_height")])
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120], PillowCImageToArray(image, 12))
    } finally {
        if image
            PillowCFreeImage(image)
        PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image open_bmp reads Pillow RGB BMP", PillowCTestImageOpenBmpReadsPillowRgb)

PillowCTestImageSaveAndOpenBmpMatchPillowLAndRgba(*) {
    l := PillowCCreateImageMode(2, 2, 1)
    rgba := PillowCCreateImageMode(2, 1, 4)
    lPath := PillowCTempBmpPath("save-l")
    rgbaPath := PillowCTempBmpPath("save-rgba")
    lLoaded := 0
    rgbaLoaded := 0
    try {
        PillowCImageSetBytes(l, [10, 40, 70, 100])
        PillowCImageSetBytes(rgba, [10, 20, 30, 40, 50, 60, 70, 80])

        PillowCImageSaveBmp(l, lPath)
        PillowCImageSaveBmp(rgba, rgbaPath)
        lBytes := PillowCReadFileBytes(lPath)
        rgbaBytes := PillowCReadFileBytes(rgbaPath)

        AhkTest.AssertEqual(1086, lBytes.Length)
        AhkTest.AssertEqual([
            66, 77, 62, 4, 0, 0, 0, 0, 0, 0, 54, 4, 0, 0,
            40, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 1, 0, 8, 0
        ], PillowCArraySlice(lBytes, 1, 30))
        AhkTest.AssertEqual([70, 100, 0, 0, 10, 40, 0, 0], PillowCArraySlice(lBytes, 1079, 1086))
        AhkTest.AssertEqual([
            66, 77, 62, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,
            40, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 32, 0,
            0, 0, 0, 0, 8, 0, 0, 0, 196, 14, 0, 0, 196, 14, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            30, 20, 10, 40, 70, 60, 50, 80
        ], rgbaBytes)

        lLoaded := PillowCImageOpenBmp(lPath)
        rgbaLoaded := PillowCImageOpenBmp(rgbaPath)
        AhkTest.AssertEqual(1, PillowCImageMode(lLoaded))
        AhkTest.AssertEqual([10, 40, 70, 100], PillowCImageToArray(lLoaded, 4))
        AhkTest.AssertEqual(3, PillowCImageMode(rgbaLoaded))
        AhkTest.AssertEqual([10, 20, 30, 50, 60, 70], PillowCImageToArray(rgbaLoaded, 6))
    } finally {
        for handle in [rgbaLoaded, lLoaded, rgba, l] {
            if handle
                PillowCFreeImage(handle)
        }
        PillowCDeleteFile(rgbaPath)
        PillowCDeleteFile(lPath)
    }
}

AhkTest.Test("pillow_c image save_bmp and open_bmp match Pillow L and RGBA behavior", PillowCTestImageSaveAndOpenBmpMatchPillowLAndRgba)

PillowCTestImageBmpRejectsUnsupportedModesAndInvalidFiles(*) {
    cmyk := PillowCCreateImageMode(1, 1, 7)
    badPath := PillowCTempBmpPath("bad")
    missingPath := PillowCTempBmpPath("missing")
    outHandle := 0
    try {
        PillowCImageSetBytes(cmyk, [1, 2, 3, 4])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_save_bmp",
            "Ptr", cmyk,
            "Ptr", PillowCUtf8Buffer(badPath),
            "Int"
        )
        AhkTest.AssertEqual(-3, status)

        PillowCWriteFileBytes(badPath, [1, 2, 3, 4])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_open_bmp",
            "Ptr", PillowCUtf8Buffer(badPath),
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_open_bmp",
            "Ptr", PillowCUtf8Buffer(missingPath),
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCDeleteFile(badPath)
        PillowCFreeImage(cmyk)
    }
}

AhkTest.Test("pillow_c BMP IO rejects unsupported modes and invalid files", PillowCTestImageBmpRejectsUnsupportedModesAndInvalidFiles)

PillowCTestImageOpenPngReadsPillowCoreModes(*) {
    rgbPath := PillowCTempPngPath("open-rgb")
    lPath := PillowCTempPngPath("open-l")
    rgbaPath := PillowCTempPngPath("open-rgba")
    rgb := 0
    l := 0
    rgba := 0
    try {
        PillowCWriteFileBytes(rgbPath, [
            137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
            0, 0, 0, 2, 0, 0, 0, 2, 8, 2, 0, 0, 0, 253, 212, 154, 115,
            0, 0, 0, 22, 73, 68, 65, 84, 120, 156, 99, 228, 18, 145, 147,
            147, 147, 99, 177, 177, 177, 145, 147, 147, 3, 0, 10, 86, 1,
            170, 1, 74, 101, 56, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130
        ])
        PillowCWriteFileBytes(lPath, [
            137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
            0, 0, 0, 2, 0, 0, 0, 2, 8, 0, 0, 0, 0, 87, 221, 82, 248,
            0, 0, 0, 14, 73, 68, 65, 84, 120, 156, 99, 228, 146, 99, 177,
            145, 3, 0, 1, 88, 0, 136, 92, 94, 213, 140, 0, 0, 0, 0,
            73, 69, 78, 68, 174, 66, 96, 130
        ])
        PillowCWriteFileBytes(rgbaPath, [
            137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
            0, 0, 0, 2, 0, 0, 0, 1, 8, 6, 0, 0, 0, 244, 34, 127, 138,
            0, 0, 0, 14, 73, 68, 65, 84, 120, 156, 99, 228, 18, 145, 211,
            0, 1, 0, 3, 250, 1, 6, 247, 12, 146, 55, 0, 0, 0, 0,
            73, 69, 78, 68, 174, 66, 96, 130
        ])

        rgb := PillowCImageOpenPng(rgbPath)
        l := PillowCImageOpenPng(lPath)
        rgba := PillowCImageOpenPng(rgbaPath)

        AhkTest.AssertEqual(3, PillowCImageMode(rgb))
        AhkTest.AssertEqual([2, 2], [PillowCImageInt(rgb, "pillow_c_image_width"), PillowCImageInt(rgb, "pillow_c_image_height")])
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120], PillowCImageToArray(rgb, 12))
        AhkTest.AssertEqual(1, PillowCImageMode(l))
        AhkTest.AssertEqual([10, 40, 70, 100], PillowCImageToArray(l, 4))
        AhkTest.AssertEqual(4, PillowCImageMode(rgba))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80], PillowCImageToArray(rgba, 8))
    } finally {
        for handle in [rgba, l, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
        for path in [rgbaPath, lPath, rgbPath]
            PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image open_png reads Pillow core modes", PillowCTestImageOpenPngReadsPillowCoreModes)

PillowCTestImageSavePngRoundTripsCoreModes(*) {
    rgb := PillowCCreateImageMode(2, 2, 3)
    l := PillowCCreateImageMode(2, 2, 1)
    rgba := PillowCCreateImageMode(2, 1, 4)
    rgbPath := PillowCTempPngPath("save-rgb")
    lPath := PillowCTempPngPath("save-l")
    rgbaPath := PillowCTempPngPath("save-rgba")
    rgbLoaded := 0
    lLoaded := 0
    rgbaLoaded := 0
    try {
        PillowCImageSetBytes(rgb, [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120])
        PillowCImageSetBytes(l, [10, 40, 70, 100])
        PillowCImageSetBytes(rgba, [10, 20, 30, 40, 50, 60, 70, 80])

        PillowCImageSavePng(rgb, rgbPath)
        PillowCImageSavePng(l, lPath)
        PillowCImageSavePng(rgba, rgbaPath)
        for path in [rgbPath, lPath, rgbaPath]
            AhkTest.AssertEqual([137, 80, 78, 71, 13, 10, 26, 10], PillowCArraySlice(PillowCReadFileBytes(path), 1, 8))

        rgbLoaded := PillowCImageOpenPng(rgbPath)
        lLoaded := PillowCImageOpenPng(lPath)
        rgbaLoaded := PillowCImageOpenPng(rgbaPath)
        AhkTest.AssertEqual(3, PillowCImageMode(rgbLoaded))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120], PillowCImageToArray(rgbLoaded, 12))
        AhkTest.AssertEqual(1, PillowCImageMode(lLoaded))
        AhkTest.AssertEqual([10, 40, 70, 100], PillowCImageToArray(lLoaded, 4))
        AhkTest.AssertEqual(4, PillowCImageMode(rgbaLoaded))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80], PillowCImageToArray(rgbaLoaded, 8))
    } finally {
        for handle in [rgbaLoaded, lLoaded, rgbLoaded, rgba, l, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
        for path in [rgbaPath, lPath, rgbPath]
            PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image save_png round-trips core modes", PillowCTestImageSavePngRoundTripsCoreModes)

PillowCTestImageOpenPngReadsPillowLaAndPaletteModes(*) {
    laPath := PillowCTempPngPath("open-la")
    pPath := PillowCTempPngPath("open-p")
    la := 0
    p := 0
    try {
        PillowCWriteFileBytes(laPath, [
            137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
            0, 0, 0, 2, 0, 0, 0, 2, 8, 4, 0, 0, 0, 216, 191, 197, 175,
            0, 0, 0, 18, 73, 68, 65, 84, 120, 156, 99, 228, 18, 17, 17,
            97, 209, 208, 16, 17, 1, 0, 3, 122, 0, 196, 80, 243, 202, 125,
            0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130
        ])
        PillowCWriteFileBytes(pPath, [
            137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
            0, 0, 0, 2, 0, 0, 0, 2, 2, 3, 0, 0, 0, 15, 216, 229, 183,
            0, 0, 0, 12, 80, 76, 84, 69, 10, 20, 30, 40, 50, 60, 70,
            80, 90, 100, 110, 120, 198, 72, 119, 223, 0, 0, 0, 12, 73,
            68, 65, 84, 120, 156, 99, 16, 96, 216, 0, 0, 0, 228, 0, 193,
            39, 168, 232, 87, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130
        ])

        la := PillowCImageOpenPng(laPath)
        p := PillowCImageOpenPng(pPath)

        AhkTest.AssertEqual(2, PillowCImageMode(la))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80], PillowCImageToArray(la, 8))
        AhkTest.AssertEqual(6, PillowCImageMode(p))
        AhkTest.AssertEqual([0, 1, 2, 3], PillowCImageToArray(p, 4))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120], PillowCImageGetPaletteRgb(p))
    } finally {
        for handle in [p, la] {
            if handle
                PillowCFreeImage(handle)
        }
        for path in [pPath, laPath]
            PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image open_png reads Pillow LA and P modes", PillowCTestImageOpenPngReadsPillowLaAndPaletteModes)

PillowCTestImageSavePngRoundTripsLaAndPaletteModes(*) {
    la := PillowCCreateImageMode(2, 2, 2)
    p := PillowCCreateImageMode(2, 2, 6)
    laPath := PillowCTempPngPath("save-la")
    pPath := PillowCTempPngPath("save-p")
    laLoaded := 0
    pLoaded := 0
    try {
        PillowCImageSetBytes(la, [10, 20, 30, 40, 50, 60, 70, 80])
        PillowCImageSetBytes(p, [0, 1, 2, 3])
        PillowCImagePutPaletteRgb(p, [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120])

        PillowCImageSavePng(la, laPath)
        PillowCImageSavePng(p, pPath)
        laLoaded := PillowCImageOpenPng(laPath)
        pLoaded := PillowCImageOpenPng(pPath)

        AhkTest.AssertEqual(2, PillowCImageMode(laLoaded))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80], PillowCImageToArray(laLoaded, 8))
        AhkTest.AssertEqual(6, PillowCImageMode(pLoaded))
        AhkTest.AssertEqual([0, 1, 2, 3], PillowCImageToArray(pLoaded, 4))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120], PillowCImageGetPaletteRgb(pLoaded))
    } finally {
        for handle in [pLoaded, laLoaded, p, la] {
            if handle
                PillowCFreeImage(handle)
        }
        for path in [pPath, laPath]
            PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image save_png round-trips LA and P modes", PillowCTestImageSavePngRoundTripsLaAndPaletteModes)

PillowCTestImageSavePngCompressLevelZeroWritesStoredPng(*) {
    rgb := PillowCCreateImageMode(32, 32, 3)
    l := PillowCCreateImageMode(4, 2, 1)
    rgba := PillowCCreateImageMode(2, 2, 4)
    defaultPath := PillowCTempPngPath("compress-default")
    storedPath := PillowCTempPngPath("compress-stored")
    lPath := PillowCTempPngPath("compress-l")
    rgbaPath := PillowCTempPngPath("compress-rgba")
    rgbLoaded := 0
    lLoaded := 0
    rgbaLoaded := 0
    try {
        rgbValues := []
        loop 32 {
            y := A_Index - 1
            loop 32 {
                x := A_Index - 1
                rgbValues.Push(Mod(x * 19 + y * 7, 256))
                rgbValues.Push(Mod(x * 3 + y * 29, 256))
                rgbValues.Push(Mod(x * 41 + y * 11, 256))
            }
        }
        lValues := [0, 32, 64, 96, 128, 160, 192, 255]
        rgbaValues := [
            255, 0, 0, 0,
            0, 255, 0, 64,
            0, 0, 255, 128,
            10, 20, 30, 255,
        ]
        PillowCImageSetBytes(rgb, rgbValues)
        PillowCImageSetBytes(l, lValues)
        PillowCImageSetBytes(rgba, rgbaValues)

        PillowCImageSavePng(rgb, defaultPath)
        PillowCImageSavePngCompressLevel(rgb, storedPath, 0)
        defaultBytes := PillowCReadFileBytes(defaultPath)
        storedBytes := PillowCReadFileBytes(storedPath)
        AhkTest.AssertEqual([137, 80, 78, 71, 13, 10, 26, 10], PillowCArraySlice(storedBytes, 1, 8))
        AhkTest.AssertTrue(storedBytes.Length > defaultBytes.Length)
        rgbLoaded := PillowCImageOpenPng(storedPath)
        AhkTest.AssertEqual(3, PillowCImageMode(rgbLoaded))
        AhkTest.AssertEqual(rgbValues, PillowCImageToArray(rgbLoaded, rgbValues.Length))

        PillowCImageSavePngCompressLevel(l, lPath, 0)
        lLoaded := PillowCImageOpenPng(lPath)
        AhkTest.AssertEqual(1, PillowCImageMode(lLoaded))
        AhkTest.AssertEqual(lValues, PillowCImageToArray(lLoaded, lValues.Length))

        PillowCImageSavePngCompressLevel(rgba, rgbaPath, 0)
        rgbaLoaded := PillowCImageOpenPng(rgbaPath)
        AhkTest.AssertEqual(4, PillowCImageMode(rgbaLoaded))
        AhkTest.AssertEqual(rgbaValues, PillowCImageToArray(rgbaLoaded, rgbaValues.Length))
    } finally {
        for handle in [rgbaLoaded, lLoaded, rgbLoaded, rgba, l, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
        for path in [rgbaPath, lPath, storedPath, defaultPath]
            PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image save_png_compress_level zero writes stored PNG", PillowCTestImageSavePngCompressLevelZeroWritesStoredPng)

PillowCTestImageSavePngCompressLevelRejectsInvalidLevel(*) {
    rgb := PillowCCreateImageMode(1, 1, 3)
    badPath := PillowCTempPngPath("bad-compress-level")
    try {
        PillowCImageSetBytes(rgb, [1, 2, 3])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_save_png_compress_level",
            "Ptr", rgb,
            "Ptr", PillowCUtf8Buffer(badPath),
            "Int", 10,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
    } finally {
        PillowCDeleteFile(badPath)
        PillowCFreeImage(rgb)
    }
}

AhkTest.Test("pillow_c image save_png_compress_level rejects invalid levels", PillowCTestImageSavePngCompressLevelRejectsInvalidLevel)

PillowCTestImageSavePngOptionsWritesDpiChunk(*) {
    rgb := PillowCCreateImageMode(2, 1, 3)
    p := PillowCCreateImageMode(2, 1, 6)
    rgbPath := PillowCTempPngPath("dpi-rgb")
    pPath := PillowCTempPngPath("dpi-p")
    rgbLoaded := 0
    pLoaded := 0
    try {
        PillowCImageSetBytes(rgb, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(p, [0, 1])
        PillowCImagePutPaletteRgb(p, [10, 20, 30, 40, 50, 60])

        try {
            status := DllCall(
                PillowCDllPath() "\pillow_c_image_save_png_options",
                "Ptr", rgb,
                "Ptr", PillowCUtf8Buffer(rgbPath),
                "Int", -1,
                "Double", 300.0,
                "Double", 150.0,
                "Int"
            )
        } catch Error as err {
            AhkTest.Fail("Expected pillow_c_image_save_png_options export")
            return
        }
        PillowCAssertStatus(status)
        PillowCAssertPngPhys(rgbPath, 11811, 5906)
        rgbLoaded := PillowCImageOpenPng(rgbPath)
        AhkTest.AssertEqual([1, 2, 3, 4, 5, 6], PillowCImageToArray(rgbLoaded, 6))
        metadata := PillowCImageMetadataResolution(rgbLoaded)
        AhkTest.AssertEqual(1, metadata.HasDpi)
        PillowCAssertFloatClose(299.9994, metadata.DpiX, 0.0001)
        PillowCAssertFloatClose(150.0124, metadata.DpiY, 0.0001)
        AhkTest.AssertEqual(0, metadata.Jfif)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_save_png_options",
            "Ptr", p,
            "Ptr", PillowCUtf8Buffer(pPath),
            "Int", 0,
            "Double", 96.0,
            "Double", 96.0,
            "Int"
        )
        PillowCAssertStatus(status)
        PillowCAssertPngPhys(pPath, 3780, 3780)
        pLoaded := PillowCImageOpenPng(pPath)
        AhkTest.AssertEqual(6, PillowCImageMode(pLoaded))
        AhkTest.AssertEqual([0, 1], PillowCImageToArray(pLoaded, 2))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60], PillowCImageGetPaletteRgb(pLoaded))
        metadata := PillowCImageMetadataResolution(pLoaded)
        AhkTest.AssertEqual(1, metadata.HasDpi)
        PillowCAssertFloatClose(96.012, metadata.DpiX, 0.0001)
        PillowCAssertFloatClose(96.012, metadata.DpiY, 0.0001)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_save_png_options",
            "Ptr", rgb,
            "Ptr", PillowCUtf8Buffer(rgbPath),
            "Int", -1,
            "Double", -1.0,
            "Double", 96.0,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
    } finally {
        for handle in [pLoaded, rgbLoaded, p, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
        for path in [pPath, rgbPath]
            PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image save_png_options writes dpi metadata", PillowCTestImageSavePngOptionsWritesDpiChunk)

PillowCTestImagePngRejectsUnsupportedModesAndInvalidFiles(*) {
    cmyk := PillowCCreateImageMode(1, 1, 7)
    badPath := PillowCTempPngPath("bad")
    missingPath := PillowCTempPngPath("missing")
    outHandle := 0
    try {
        PillowCImageSetBytes(cmyk, [1, 2, 3, 4])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_save_png",
            "Ptr", cmyk,
            "Ptr", PillowCUtf8Buffer(badPath),
            "Int"
        )
        AhkTest.AssertEqual(-3, status)

        PillowCWriteFileBytes(badPath, [1, 2, 3, 4])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_open_png",
            "Ptr", PillowCUtf8Buffer(badPath),
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_open_png",
            "Ptr", PillowCUtf8Buffer(missingPath),
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCDeleteFile(badPath)
        PillowCFreeImage(cmyk)
    }
}

AhkTest.Test("pillow_c PNG IO rejects unsupported modes and invalid files", PillowCTestImagePngRejectsUnsupportedModesAndInvalidFiles)

PillowCTestImageOpenJpegReadsPillowCoreModes(*) {
    rgbPath := PillowCTempJpegPath("open-rgb")
    lPath := PillowCTempJpegPath("open-l")
    rgb := 0
    l := 0
    try {
        PillowCWriteFileBytes(rgbPath, [
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
        PillowCWriteFileBytes(lPath, [
            255, 216, 255, 224, 0, 16, 74, 70, 73, 70, 0, 1, 1, 0, 0, 1,
            0, 1, 0, 0, 255, 219, 0, 67, 0, 8, 6, 6, 7, 6, 5, 8,
            7, 7, 7, 9, 9, 8, 10, 12, 20, 13, 12, 11, 11, 12, 25, 18,
            19, 15, 20, 29, 26, 31, 30, 29, 26, 28, 28, 32, 36, 46, 39, 32,
            34, 44, 35, 28, 28, 40, 55, 41, 44, 48, 49, 52, 52, 52, 31, 39,
            57, 61, 56, 50, 60, 46, 51, 52, 50, 255, 192, 0, 11, 8, 0, 1,
            0, 1, 1, 1, 17, 0, 255, 196, 0, 31, 0, 0, 1, 5, 1, 1,
            1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4,
            5, 6, 7, 8, 9, 10, 11, 255, 196, 0, 181, 16, 0, 2, 1, 3,
            3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125, 1, 2, 3, 0,
            4, 17, 5, 18, 33, 49, 65, 6, 19, 81, 97, 7, 34, 113, 20, 50,
            129, 145, 161, 8, 35, 66, 177, 193, 21, 82, 209, 240, 36, 51, 98, 114,
            130, 9, 10, 22, 23, 24, 25, 26, 37, 38, 39, 40, 41, 42, 52, 53,
            54, 55, 56, 57, 58, 67, 68, 69, 70, 71, 72, 73, 74, 83, 84, 85,
            86, 87, 88, 89, 90, 99, 100, 101, 102, 103, 104, 105, 106, 115, 116, 117,
            118, 119, 120, 121, 122, 131, 132, 133, 134, 135, 136, 137, 138, 146, 147, 148,
            149, 150, 151, 152, 153, 154, 162, 163, 164, 165, 166, 167, 168, 169, 170, 178,
            179, 180, 181, 182, 183, 184, 185, 186, 194, 195, 196, 197, 198, 199, 200, 201,
            202, 210, 211, 212, 213, 214, 215, 216, 217, 218, 225, 226, 227, 228, 229, 230,
            231, 232, 233, 234, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 255, 218,
            0, 8, 1, 1, 0, 0, 63, 0, 138, 191, 255, 217
        ])

        rgb := PillowCImageOpenJpeg(rgbPath)
        l := PillowCImageOpenJpeg(lPath)
        AhkTest.AssertEqual(3, PillowCImageMode(rgb))
        AhkTest.AssertEqual([1, 1], [PillowCImageInt(rgb, "pillow_c_image_width"), PillowCImageInt(rgb, "pillow_c_image_height")])
        PillowCAssertArrayNear([120, 130, 140], PillowCImageToArray(rgb, 3), 2)
        AhkTest.AssertEqual(1, PillowCImageMode(l))
        PillowCAssertArrayNear([123], PillowCImageToArray(l, 1), 2)
    } finally {
        for handle in [l, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
        for path in [lPath, rgbPath]
            PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image open_jpeg reads Pillow L and RGB JPEGs", PillowCTestImageOpenJpegReadsPillowCoreModes)

PillowCTestImageSaveJpegRoundTripsCoreModes(*) {
    rgb := PillowCCreateImageMode(1, 1, 3)
    l := PillowCCreateImageMode(1, 1, 1)
    rgbPath := PillowCTempJpegPath("save-rgb")
    lPath := PillowCTempJpegPath("save-l")
    rgbLoaded := 0
    lLoaded := 0
    try {
        PillowCImageSetBytes(rgb, [120, 130, 140])
        PillowCImageSetBytes(l, [123])
        PillowCImageSaveJpeg(rgb, rgbPath)
        PillowCImageSaveJpeg(l, lPath)
        AhkTest.AssertEqual([255, 216], PillowCArraySlice(PillowCReadFileBytes(rgbPath), 1, 2))
        AhkTest.AssertEqual([255, 216], PillowCArraySlice(PillowCReadFileBytes(lPath), 1, 2))

        rgbLoaded := PillowCImageOpenJpeg(rgbPath)
        lLoaded := PillowCImageOpenJpeg(lPath)
        AhkTest.AssertEqual(3, PillowCImageMode(rgbLoaded))
        PillowCAssertArrayNear([120, 130, 140], PillowCImageToArray(rgbLoaded, 3), 8)
        AhkTest.AssertEqual(1, PillowCImageMode(lLoaded))
        PillowCAssertArrayNear([123], PillowCImageToArray(lLoaded, 1), 8)
    } finally {
        for handle in [lLoaded, rgbLoaded, l, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
        for path in [lPath, rgbPath]
            PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image save_jpeg round-trips L and RGB modes", PillowCTestImageSaveJpegRoundTripsCoreModes)

PillowCTestImageSaveJpegQualityChangesEncodedSize(*) {
    rgb := PillowCCreateImageMode(32, 32, 3)
    lowPath := PillowCTempJpegPath("quality-low")
    highPath := PillowCTempJpegPath("quality-high")
    lowLoaded := 0
    highLoaded := 0
    try {
        values := []
        loop 32 {
            y := A_Index - 1
            loop 32 {
                x := A_Index - 1
                values.Push(Mod(x * 19 + y * 7, 256))
                values.Push(Mod(x * 3 + y * 29, 256))
                values.Push(Mod(x * 41 + y * 11, 256))
            }
        }
        PillowCImageSetBytes(rgb, values)

        PillowCImageSaveJpegQuality(rgb, lowPath, 20)
        PillowCImageSaveJpegQuality(rgb, highPath, 95)
        lowBytes := PillowCReadFileBytes(lowPath)
        highBytes := PillowCReadFileBytes(highPath)
        AhkTest.AssertEqual([255, 216], PillowCArraySlice(lowBytes, 1, 2))
        AhkTest.AssertEqual([255, 216], PillowCArraySlice(highBytes, 1, 2))
        AhkTest.AssertTrue(highBytes.Length > lowBytes.Length)

        lowLoaded := PillowCImageOpenJpeg(lowPath)
        highLoaded := PillowCImageOpenJpeg(highPath)
        AhkTest.AssertEqual([32, 32], [PillowCImageInt(lowLoaded, "pillow_c_image_width"), PillowCImageInt(lowLoaded, "pillow_c_image_height")])
        AhkTest.AssertEqual([32, 32], [PillowCImageInt(highLoaded, "pillow_c_image_width"), PillowCImageInt(highLoaded, "pillow_c_image_height")])
        AhkTest.AssertEqual(3, PillowCImageMode(lowLoaded))
        AhkTest.AssertEqual(3, PillowCImageMode(highLoaded))
    } finally {
        for handle in [highLoaded, lowLoaded, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
        for path in [highPath, lowPath]
            PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image save_jpeg_quality controls encoded quality", PillowCTestImageSaveJpegQualityChangesEncodedSize)

PillowCTestImageSaveJpegOptionsWritesDpiMetadata(*) {
    rgb := PillowCCreateImageMode(2, 1, 3)
    dpiPath := PillowCTempJpegPath("dpi")
    defaultUnitPath := PillowCTempJpegPath("dpi-default-unit")
    dpiLoaded := 0
    defaultUnitLoaded := 0
    try {
        PillowCImageSetBytes(rgb, [10, 20, 30, 40, 50, 60])

        try {
            status := DllCall(
                PillowCDllPath() "\pillow_c_image_save_jpeg_options",
                "Ptr", rgb,
                "Ptr", PillowCUtf8Buffer(dpiPath),
                "Int", 95,
                "Int", 1,
                "Double", 300.0,
                "Double", 150.0,
                "Int"
            )
        } catch Error as err {
            AhkTest.Fail("Expected pillow_c_image_save_jpeg_options export")
            return
        }
        PillowCAssertStatus(status)
        jfif := PillowCReadJpegJfif(dpiPath)
        AhkTest.AssertEqual(1, jfif.Unit)
        AhkTest.AssertEqual(300, jfif.XDensity)
        AhkTest.AssertEqual(150, jfif.YDensity)
        dpiLoaded := PillowCImageOpenJpeg(dpiPath)
        metadata := PillowCImageMetadataResolution(dpiLoaded)
        AhkTest.AssertEqual(1, metadata.HasDpi)
        PillowCAssertFloatClose(300.0, metadata.DpiX)
        PillowCAssertFloatClose(150.0, metadata.DpiY)
        AhkTest.AssertEqual(257, metadata.Jfif)
        AhkTest.AssertEqual([1, 1], metadata.JfifVersion)
        AhkTest.AssertEqual(1, metadata.JfifUnit)
        AhkTest.AssertEqual([300, 150], metadata.JfifDensity)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_save_jpeg_options",
            "Ptr", rgb,
            "Ptr", PillowCUtf8Buffer(defaultUnitPath),
            "Int", -1,
            "Int", 1,
            "Double", 0.4,
            "Double", 96.0,
            "Int"
        )
        PillowCAssertStatus(status)
        jfif := PillowCReadJpegJfif(defaultUnitPath)
        AhkTest.AssertEqual(0, jfif.Unit)
        AhkTest.AssertEqual(1, jfif.XDensity)
        AhkTest.AssertEqual(1, jfif.YDensity)
        defaultUnitLoaded := PillowCImageOpenJpeg(defaultUnitPath)
        metadata := PillowCImageMetadataResolution(defaultUnitLoaded)
        AhkTest.AssertEqual(0, metadata.HasDpi)
        AhkTest.AssertEqual(257, metadata.Jfif)
        AhkTest.AssertEqual([1, 1], metadata.JfifVersion)
        AhkTest.AssertEqual(0, metadata.JfifUnit)
        AhkTest.AssertEqual([1, 1], metadata.JfifDensity)
    } finally {
        for handle in [defaultUnitLoaded, dpiLoaded] {
            if handle
                PillowCFreeImage(handle)
        }
        PillowCDeleteFile(defaultUnitPath)
        PillowCDeleteFile(dpiPath)
        PillowCFreeImage(rgb)
    }
}

AhkTest.Test("pillow_c image save_jpeg_options writes dpi metadata", PillowCTestImageSaveJpegOptionsWritesDpiMetadata)

PillowCTestImageOpenJpegReadsExifOrientationMetadata(*) {
    rgb := PillowCCreateImageMode(2, 3, 3)
    plainPath := PillowCTempJpegPath("exif-source")
    exifPath := PillowCTempJpegPath("exif-orientation")
    plainLoaded := 0
    exifLoaded := 0
    try {
        PillowCImageSetBytes(rgb, [
            10, 20, 30, 40, 50, 60,
            70, 80, 90, 100, 110, 120,
            130, 140, 150, 160, 170, 180,
        ])
        PillowCImageSaveJpeg(rgb, plainPath)
        PillowCWriteJpegWithExifOrientation(plainPath, exifPath, 6)

        plainLoaded := PillowCImageOpenJpeg(plainPath)
        exifLoaded := PillowCImageOpenJpeg(exifPath)
        AhkTest.AssertEqual(0, PillowCImageExifOrientation(plainLoaded))
        AhkTest.AssertEqual(6, PillowCImageExifOrientation(exifLoaded))
    } finally {
        for handle in [exifLoaded, plainLoaded, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
        for path in [exifPath, plainPath]
            PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image open_jpeg reads EXIF orientation metadata", PillowCTestImageOpenJpegReadsExifOrientationMetadata)

PillowCTestImageJpegRejectsUnsupportedModesAndInvalidFiles(*) {
    rgba := PillowCCreateImageMode(1, 1, 4)
    badPath := PillowCTempJpegPath("bad")
    missingPath := PillowCTempJpegPath("missing")
    outHandle := 0
    try {
        PillowCImageSetBytes(rgba, [1, 2, 3, 4])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_save_jpeg",
            "Ptr", rgba,
            "Ptr", PillowCUtf8Buffer(badPath),
            "Int"
        )
        AhkTest.AssertEqual(-3, status)

        PillowCWriteFileBytes(badPath, [1, 2, 3, 4])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_open_jpeg",
            "Ptr", PillowCUtf8Buffer(badPath),
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_open_jpeg",
            "Ptr", PillowCUtf8Buffer(missingPath),
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCDeleteFile(badPath)
        PillowCFreeImage(rgba)
    }
}

AhkTest.Test("pillow_c JPEG IO rejects unsupported modes and invalid files", PillowCTestImageJpegRejectsUnsupportedModesAndInvalidFiles)

PillowCTestImageOpenTiffReadsPillowCoreModes(*) {
    lPath := PillowCTempTiffPath("open-l")
    rgbPath := PillowCTempTiffPath("open-rgb")
    rgbaPath := PillowCTempTiffPath("open-rgba")
    l := 0
    rgb := 0
    rgba := 0
    try {
        PillowCWriteFileBytes(lPath, [
            73, 73, 42, 0, 8, 0, 0, 0, 9, 0, 0, 1, 4, 0, 1, 0,
            0, 0, 2, 0, 0, 0, 1, 1, 4, 0, 1, 0, 0, 0, 2, 0,
            0, 0, 2, 1, 3, 0, 1, 0, 0, 0, 8, 0, 0, 0, 3, 1,
            3, 0, 1, 0, 0, 0, 1, 0, 0, 0, 6, 1, 3, 0, 1, 0,
            0, 0, 1, 0, 0, 0, 17, 1, 4, 0, 1, 0, 0, 0, 122, 0,
            0, 0, 22, 1, 4, 0, 1, 0, 0, 0, 2, 0, 0, 0, 23, 1,
            4, 0, 1, 0, 0, 0, 4, 0, 0, 0, 28, 1, 3, 0, 1, 0,
            0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 10, 40, 70, 100
        ])
        PillowCWriteFileBytes(rgbPath, [
            73, 73, 42, 0, 8, 0, 0, 0, 10, 0, 0, 1, 4, 0, 1, 0,
            0, 0, 2, 0, 0, 0, 1, 1, 4, 0, 1, 0, 0, 0, 2, 0,
            0, 0, 2, 1, 3, 0, 3, 0, 0, 0, 134, 0, 0, 0, 3, 1,
            3, 0, 1, 0, 0, 0, 1, 0, 0, 0, 6, 1, 3, 0, 1, 0,
            0, 0, 2, 0, 0, 0, 17, 1, 4, 0, 1, 0, 0, 0, 140, 0,
            0, 0, 21, 1, 3, 0, 1, 0, 0, 0, 3, 0, 0, 0, 22, 1,
            4, 0, 1, 0, 0, 0, 2, 0, 0, 0, 23, 1, 4, 0, 1, 0,
            0, 0, 12, 0, 0, 0, 28, 1, 3, 0, 1, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 0, 8, 0, 8, 0, 8, 0, 10, 20, 30, 40,
            50, 60, 70, 80, 90, 100, 110, 120
        ])
        PillowCWriteFileBytes(rgbaPath, [
            73, 73, 42, 0, 8, 0, 0, 0, 11, 0, 0, 1, 4, 0, 1, 0,
            0, 0, 2, 0, 0, 0, 1, 1, 4, 0, 1, 0, 0, 0, 1, 0,
            0, 0, 2, 1, 3, 0, 4, 0, 0, 0, 146, 0, 0, 0, 3, 1,
            3, 0, 1, 0, 0, 0, 1, 0, 0, 0, 6, 1, 3, 0, 1, 0,
            0, 0, 2, 0, 0, 0, 17, 1, 4, 0, 1, 0, 0, 0, 154, 0,
            0, 0, 21, 1, 3, 0, 1, 0, 0, 0, 4, 0, 0, 0, 22, 1,
            4, 0, 1, 0, 0, 0, 1, 0, 0, 0, 23, 1, 4, 0, 1, 0,
            0, 0, 8, 0, 0, 0, 28, 1, 3, 0, 1, 0, 0, 0, 1, 0,
            0, 0, 82, 1, 3, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0,
            0, 0, 8, 0, 8, 0, 8, 0, 8, 0, 10, 20, 30, 40, 50, 60,
            70, 80
        ])

        l := PillowCImageOpenTiff(lPath)
        rgb := PillowCImageOpenTiff(rgbPath)
        rgba := PillowCImageOpenTiff(rgbaPath)
        AhkTest.AssertEqual(1, PillowCImageMode(l))
        AhkTest.AssertEqual([10, 40, 70, 100], PillowCImageToArray(l, 4))
        AhkTest.AssertEqual(3, PillowCImageMode(rgb))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120], PillowCImageToArray(rgb, 12))
        AhkTest.AssertEqual(4, PillowCImageMode(rgba))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80], PillowCImageToArray(rgba, 8))
    } finally {
        for handle in [rgba, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
        for path in [rgbaPath, rgbPath, lPath]
            PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image open_tiff reads Pillow L RGB and RGBA TIFFs", PillowCTestImageOpenTiffReadsPillowCoreModes)

PillowCTestImageOpenTiffFramesReadsMultiframeImages(*) {
    path := PillowCTempTiffPath("multi-l")
    first := 0
    second := 0
    outHandle := 0
    try {
        PillowCWriteFileBytes(path, [
            73, 73, 42, 0, 8, 0, 0, 0, 9, 0, 0, 1, 4, 0, 1, 0,
            0, 0, 2, 0, 0, 0, 1, 1, 4, 0, 1, 0, 0, 0, 1, 0,
            0, 0, 2, 1, 3, 0, 1, 0, 0, 0, 8, 0, 0, 0, 3, 1,
            3, 0, 1, 0, 0, 0, 1, 0, 0, 0, 6, 1, 3, 0, 1, 0,
            0, 0, 1, 0, 0, 0, 17, 1, 4, 0, 1, 0, 0, 0, 122, 0,
            0, 0, 22, 1, 4, 0, 1, 0, 0, 0, 1, 0, 0, 0, 23, 1,
            4, 0, 1, 0, 0, 0, 2, 0, 0, 0, 28, 1, 3, 0, 1, 0,
            0, 0, 1, 0, 0, 0, 136, 0, 0, 0, 10, 20, 0, 0, 0, 0,
            73, 73, 42, 0, 8, 0, 0, 0, 9, 0, 0, 1, 4, 0, 1, 0,
            0, 0, 2, 0, 0, 0, 1, 1, 4, 0, 1, 0, 0, 0, 1, 0,
            0, 0, 2, 1, 3, 0, 1, 0, 0, 0, 8, 0, 0, 0, 3, 1,
            3, 0, 1, 0, 0, 0, 1, 0, 0, 0, 6, 1, 3, 0, 1, 0,
            0, 0, 1, 0, 0, 0, 17, 1, 4, 0, 1, 0, 0, 0, 250, 0,
            0, 0, 22, 1, 4, 0, 1, 0, 0, 0, 1, 0, 0, 0, 23, 1,
            4, 0, 1, 0, 0, 0, 2, 0, 0, 0, 28, 1, 3, 0, 1, 0,
            0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 30, 40, 0, 0, 0, 0
        ])

        AhkTest.AssertEqual(2, PillowCImageFrameCountTiff(path))
        first := PillowCImageOpenTiffFrame(path, 0)
        second := PillowCImageOpenTiffFrame(path, 1)
        AhkTest.AssertEqual(1, PillowCImageMode(first))
        AhkTest.AssertEqual([10, 20], PillowCImageToArray(first, 2))
        AhkTest.AssertEqual(1, PillowCImageMode(second))
        AhkTest.AssertEqual([30, 40], PillowCImageToArray(second, 2))

        status := DllCall(PillowCDllPath() "\pillow_c_image_open_tiff_frame", "Ptr", PillowCUtf8Buffer(path), "Int", 2, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        for handle in [second, first] {
            if handle
                PillowCFreeImage(handle)
        }
        PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image open_tiff_frame reads multiframe TIFF images", PillowCTestImageOpenTiffFramesReadsMultiframeImages)

PillowCTestImageSaveTiffRoundTripsCoreModes(*) {
    l := PillowCCreateImageMode(2, 2, 1)
    rgb := PillowCCreateImageMode(2, 2, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    lPath := PillowCTempTiffPath("save-l")
    rgbPath := PillowCTempTiffPath("save-rgb")
    rgbaPath := PillowCTempTiffPath("save-rgba")
    lLoaded := 0
    rgbLoaded := 0
    rgbaLoaded := 0
    try {
        PillowCImageSetBytes(l, [10, 40, 70, 100])
        PillowCImageSetBytes(rgb, [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120])
        PillowCImageSetBytes(rgba, [10, 20, 30, 40, 50, 60, 70, 80])
        PillowCImageSaveTiff(l, lPath)
        PillowCImageSaveTiff(rgb, rgbPath)
        PillowCImageSaveTiff(rgba, rgbaPath)
        for path in [lPath, rgbPath, rgbaPath]
            AhkTest.AssertEqual([73, 73, 42, 0], PillowCArraySlice(PillowCReadFileBytes(path), 1, 4))

        lLoaded := PillowCImageOpenTiff(lPath)
        rgbLoaded := PillowCImageOpenTiff(rgbPath)
        rgbaLoaded := PillowCImageOpenTiff(rgbaPath)
        AhkTest.AssertEqual(1, PillowCImageMode(lLoaded))
        AhkTest.AssertEqual([10, 40, 70, 100], PillowCImageToArray(lLoaded, 4))
        AhkTest.AssertEqual(3, PillowCImageMode(rgbLoaded))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120], PillowCImageToArray(rgbLoaded, 12))
        AhkTest.AssertEqual(4, PillowCImageMode(rgbaLoaded))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80], PillowCImageToArray(rgbaLoaded, 8))
    } finally {
        for handle in [rgbaLoaded, rgbLoaded, lLoaded, rgba, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
        for path in [rgbaPath, rgbPath, lPath]
            PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image save_tiff round-trips L RGB and RGBA modes", PillowCTestImageSaveTiffRoundTripsCoreModes)

PillowCTestImageTiffRejectsUnsupportedModesAndInvalidFiles(*) {
    cmyk := PillowCCreateImageMode(1, 1, 7)
    badPath := PillowCTempTiffPath("bad")
    missingPath := PillowCTempTiffPath("missing")
    outHandle := 0
    try {
        PillowCImageSetBytes(cmyk, [1, 2, 3, 4])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_save_tiff",
            "Ptr", cmyk,
            "Ptr", PillowCUtf8Buffer(badPath),
            "Int"
        )
        AhkTest.AssertEqual(-3, status)

        PillowCWriteFileBytes(badPath, [1, 2, 3, 4])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_open_tiff",
            "Ptr", PillowCUtf8Buffer(badPath),
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_open_tiff",
            "Ptr", PillowCUtf8Buffer(missingPath),
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCDeleteFile(badPath)
        PillowCFreeImage(cmyk)
    }
}

AhkTest.Test("pillow_c TIFF IO rejects unsupported modes and invalid files", PillowCTestImageTiffRejectsUnsupportedModesAndInvalidFiles)

PillowCTestImageOpenGifReadsPillowPaletteMode(*) {
    path := PillowCTempGifPath("open-p")
    image := 0
    try {
        PillowCWriteFileBytes(path, [
            71, 73, 70, 56, 55, 97, 2, 0, 2, 0, 129, 0, 0, 10, 20, 30,
            40, 50, 60, 70, 80, 90, 100, 110, 120, 44, 0, 0, 0, 0, 2, 0,
            2, 0, 0, 8, 7, 0, 1, 4, 16, 48, 32, 32, 0, 59
        ])
        image := PillowCImageOpenGif(path)
        AhkTest.AssertEqual(6, PillowCImageMode(image))
        AhkTest.AssertEqual([2, 2], [PillowCImageInt(image, "pillow_c_image_width"), PillowCImageInt(image, "pillow_c_image_height")])
        AhkTest.AssertEqual([0, 1, 2, 3], PillowCImageToArray(image, 4))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120], PillowCImageGetPaletteRgb(image))
    } finally {
        if image
            PillowCFreeImage(image)
        PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image open_gif reads Pillow P mode GIFs", PillowCTestImageOpenGifReadsPillowPaletteMode)

PillowCTestImageOpenGifFramesReadsMultiframeImages(*) {
    path := PillowCTempGifPath("multi-p")
    first := 0
    second := 0
    outHandle := 0
    try {
        PillowCWriteFileBytes(path, [
            71, 73, 70, 56, 57, 97, 2, 0, 1, 0, 129, 0, 0, 0, 0, 0,
            10, 20, 30, 0, 0, 0, 0, 0, 0, 33, 255, 11, 78, 69, 84, 83,
            67, 65, 80, 69, 50, 46, 48, 3, 1, 0, 0, 0, 33, 249, 4, 0,
            1, 0, 0, 0, 44, 0, 0, 0, 0, 2, 0, 1, 0, 0, 8, 5,
            0, 1, 4, 8, 8, 0, 33, 249, 4, 1, 2, 0, 2, 0, 44, 0,
            0, 0, 0, 2, 0, 1, 0, 129, 0, 0, 0, 10, 20, 30, 0, 0,
            0, 0, 0, 0, 8, 5, 0, 3, 0, 8, 8, 0, 59
        ])

        AhkTest.AssertEqual(2, PillowCImageFrameCountGif(path))
        first := PillowCImageOpenGifFrame(path, 0)
        second := PillowCImageOpenGifFrame(path, 1)
        AhkTest.AssertEqual(6, PillowCImageMode(first))
        AhkTest.AssertEqual([0, 1], PillowCImageToArray(first, 2))
        AhkTest.AssertEqual([0, 0, 0, 10, 20, 30], PillowCArraySlice(PillowCImageGetPaletteRgb(first), 1, 6))
        AhkTest.AssertEqual(3, PillowCImageMode(second))
        AhkTest.AssertEqual([10, 20, 30, 0, 0, 0], PillowCImageToArray(second, 6))

        status := DllCall(PillowCDllPath() "\pillow_c_image_open_gif_frame", "Ptr", PillowCUtf8Buffer(path), "Int", 2, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        for handle in [second, first] {
            if handle
                PillowCFreeImage(handle)
        }
        PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image open_gif_frame reads multiframe GIF images", PillowCTestImageOpenGifFramesReadsMultiframeImages)

PillowCTestImageGifMetadataReadsLoopDurationAndDisposal(*) {
    path := PillowCTempGifPath("metadata")
    try {
        PillowCWriteFileBytes(path, [
            71, 73, 70, 56, 57, 97, 2, 0, 1, 0, 129, 0, 0, 0, 0, 0,
            10, 20, 30, 0, 0, 0, 0, 0, 0, 33, 255, 11, 78, 69, 84, 83,
            67, 65, 80, 69, 50, 46, 48, 3, 1, 3, 0, 0, 33, 249, 4, 0,
            1, 0, 0, 0, 44, 0, 0, 0, 0, 2, 0, 1, 0, 0, 8, 5,
            0, 1, 4, 8, 8, 0, 33, 249, 4, 9, 2, 0, 2, 0, 44, 0,
            0, 0, 0, 2, 0, 1, 0, 129, 0, 0, 0, 10, 20, 30, 0, 0,
            0, 0, 0, 0, 8, 5, 0, 3, 0, 8, 8, 0, 59
        ])

        first := PillowCImageGifMetadata(path, 0)
        second := PillowCImageGifMetadata(path, 1)
        AhkTest.AssertEqual(10, first.Duration)
        AhkTest.AssertEqual(20, second.Duration)
        AhkTest.AssertEqual(3, first.Loop)
        AhkTest.AssertEqual(3, second.Loop)
        AhkTest.AssertEqual(0, first.Disposal)
        AhkTest.AssertEqual(2, second.Disposal)
        AhkTest.AssertEqual(0, first.Background)
        AhkTest.AssertEqual(0, second.Background)

        outDuration := 0
        outLoop := 0
        outDisposal := 0
        outBackground := 0
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_gif_metadata",
            "Ptr", PillowCUtf8Buffer(path),
            "Int", 2,
            "Int*", &outDuration,
            "Int*", &outLoop,
            "Int*", &outDisposal,
            "Int*", &outBackground,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
    } finally {
        PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image gif metadata reads loop duration and disposal", PillowCTestImageGifMetadataReadsLoopDurationAndDisposal)

PillowCTestImageSaveGifRoundTripsPaletteMode(*) {
    image := PillowCCreateImageMode(2, 2, 6)
    path := PillowCTempGifPath("save-p")
    loaded := 0
    try {
        PillowCImageSetBytes(image, [0, 1, 2, 3])
        PillowCImagePutPaletteRgb(image, [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120])
        PillowCImageSaveGif(image, path)
        AhkTest.AssertEqual([71, 73, 70], PillowCArraySlice(PillowCReadFileBytes(path), 1, 3))

        loaded := PillowCImageOpenGif(path)
        AhkTest.AssertEqual(6, PillowCImageMode(loaded))
        AhkTest.AssertEqual([0, 1, 2, 3], PillowCImageToArray(loaded, 4))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120], PillowCImageGetPaletteRgb(loaded))
    } finally {
        if loaded
            PillowCFreeImage(loaded)
        PillowCFreeImage(image)
        PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image save_gif round-trips P mode images", PillowCTestImageSaveGifRoundTripsPaletteMode)

PillowCTestImageSaveGifAnimationWritesFramesAndMetadata(*) {
    first := PillowCCreateImageMode(2, 1, 6)
    second := PillowCCreateImageMode(2, 1, 6)
    path := PillowCTempGifPath("save-animation")
    loadedFirst := 0
    loadedSecond := 0
    try {
        palette := [0, 0, 0, 10, 20, 30]
        PillowCImageSetBytes(first, [0, 1])
        PillowCImageSetBytes(second, [1, 0])
        PillowCImagePutPaletteRgb(first, palette)
        PillowCImagePutPaletteRgb(second, palette)

        PillowCImageSaveGifAnimation([first, second], path, [10, 20], 3, [0, 2])
        AhkTest.AssertEqual([71, 73, 70, 56, 57, 97], PillowCArraySlice(PillowCReadFileBytes(path), 1, 6))
        AhkTest.AssertEqual(2, PillowCImageFrameCountGif(path))

        firstMetadata := PillowCImageGifMetadata(path, 0)
        secondMetadata := PillowCImageGifMetadata(path, 1)
        AhkTest.AssertEqual(10, firstMetadata.Duration)
        AhkTest.AssertEqual(20, secondMetadata.Duration)
        AhkTest.AssertEqual(3, firstMetadata.Loop)
        AhkTest.AssertEqual(3, secondMetadata.Loop)
        AhkTest.AssertEqual(0, firstMetadata.Disposal)
        AhkTest.AssertEqual(2, secondMetadata.Disposal)

        loadedFirst := PillowCImageOpenGifFrame(path, 0)
        loadedSecond := PillowCImageOpenGifFrame(path, 1)
        AhkTest.AssertEqual(6, PillowCImageMode(loadedFirst))
        AhkTest.AssertEqual([0, 1], PillowCImageToArray(loadedFirst, 2))
        AhkTest.AssertEqual(palette, PillowCArraySlice(PillowCImageGetPaletteRgb(loadedFirst), 1, 6))
        AhkTest.AssertEqual(3, PillowCImageMode(loadedSecond))
        AhkTest.AssertEqual([10, 20, 30, 0, 0, 0], PillowCImageToArray(loadedSecond, 6))
    } finally {
        for handle in [loadedSecond, loadedFirst, second, first] {
            if handle
                PillowCFreeImage(handle)
        }
        PillowCDeleteFile(path)
    }
}

AhkTest.Test("pillow_c image save_gif_animation writes frames and metadata", PillowCTestImageSaveGifAnimationWritesFramesAndMetadata)

PillowCTestImageGifRejectsUnsupportedModesAndInvalidFiles(*) {
    rgb := PillowCCreateImageMode(1, 1, 3)
    badPath := PillowCTempGifPath("bad")
    missingPath := PillowCTempGifPath("missing")
    outHandle := 0
    try {
        PillowCImageSetBytes(rgb, [1, 2, 3])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_save_gif",
            "Ptr", rgb,
            "Ptr", PillowCUtf8Buffer(badPath),
            "Int"
        )
        AhkTest.AssertEqual(-3, status)

        PillowCWriteFileBytes(badPath, [1, 2, 3, 4])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_open_gif",
            "Ptr", PillowCUtf8Buffer(badPath),
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_open_gif",
            "Ptr", PillowCUtf8Buffer(missingPath),
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCDeleteFile(badPath)
        PillowCFreeImage(rgb)
    }
}

AhkTest.Test("pillow_c GIF IO rejects unsupported modes and invalid files", PillowCTestImageGifRejectsUnsupportedModesAndInvalidFiles)

PillowCTestImageCopyIsIndependent(*) {
    source := PillowCCreateImage(2, 1, 3)
    copied := 0
    try {
        PillowCImageSetBytes(source, [10, 20, 30, 100, 110, 120])
        status := DllCall(PillowCDllPath() "\pillow_c_image_copy", "Ptr", source, "Ptr*", &copied, "Int")
        PillowCAssertStatus(status)
        PillowCImageSetBytes(source, [1, 2, 3, 4, 5, 6])
        AhkTest.AssertEqual([10, 20, 30, 100, 110, 120], PillowCImageToArray(copied, 6))
    } finally {
        if copied
            PillowCFreeImage(copied)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image copy is independent", PillowCTestImageCopyIsIndependent)

PillowCTestImageCopyPreservesPalette(*) {
    source := PillowCCreateImageMode(2, 1, 6)
    copied := 0
    try {
        PillowCImageSetBytes(source, [1, 0])
        PillowCImagePutPaletteRgb(source, [0, 0, 0, 10, 20, 30])
        status := DllCall(PillowCDllPath() "\pillow_c_image_copy", "Ptr", source, "Ptr*", &copied, "Int")
        PillowCAssertStatus(status)
        PillowCImagePutPaletteRgb(source, [0, 0, 0, 40, 50, 60])
        AhkTest.AssertEqual([0, 0, 0, 10, 20, 30], PillowCImageGetPaletteRgb(copied))
        rgb := PillowCImageConvertMode(copied, 3)
        try {
            AhkTest.AssertEqual([10, 20, 30, 0, 0, 0], PillowCImageToArray(rgb, 6))
        } finally {
            PillowCFreeImage(rgb)
        }
    } finally {
        if copied
            PillowCFreeImage(copied)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image copy preserves P palette", PillowCTestImageCopyPreservesPalette)

PillowCTestImageCopyIntoReusesTargetHandle(*) {
    source := PillowCCreateImage(2, 1, 3)
    target := PillowCCreateImage(2, 1, 3)
    try {
        PillowCImageSetBytes(source, [10, 20, 30, 100, 110, 120])
        PillowCImageSetBytes(target, [1, 2, 3, 4, 5, 6])
        before := PillowCImageData(target).Ptr
        PillowCImageCopyInto(source, target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([10, 20, 30, 100, 110, 120], PillowCImageToArray(target, 6))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image copy_into reuses target handle storage", PillowCTestImageCopyIntoReusesTargetHandle)

PillowCTestImageDrawRectangleMatchesPillowFillOutlineWidth(*) {
    l := PillowCCreateImageMode(6, 5, 1)
    rgb := PillowCCreateImageMode(4, 3, 3)
    rgba := PillowCCreateImageMode(4, 3, 4)
    try {
        PillowCImageDrawRectangle(l, [1, 1, 4, 3], [7], [9], 2)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 9, 9, 9, 9, 0,
            0, 9, 9, 9, 9, 0,
            0, 9, 9, 9, 9, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(l, 30))

        PillowCImageDrawRectangle(rgb, [1, 0, 3, 2], [10, 20, 30], [90, 80, 70], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 10, 20, 30, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70,
        ], PillowCImageToArray(rgb, 36))

        PillowCImageDrawRectangle(rgba, [1, 0, 3, 2], [10, 20, 30, 40], [90, 80, 70, 128], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 90, 80, 70, 128, 90, 80, 70, 128, 90, 80, 70, 128,
            0, 0, 0, 0, 90, 80, 70, 128, 10, 20, 30, 40, 90, 80, 70, 128,
            0, 0, 0, 0, 90, 80, 70, 128, 90, 80, 70, 128, 90, 80, 70, 128,
        ], PillowCImageToArray(rgba, 48))
    } finally {
        for handle in [rgba, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_rectangle matches Pillow fill outline and width", PillowCTestImageDrawRectangleMatchesPillowFillOutlineWidth)

PillowCTestImageDrawRectangleClipsAndRejectsInvalidArguments(*) {
    clipped := PillowCCreateImageMode(6, 5, 1)
    reversed := PillowCCreateImageMode(3, 3, 1)
    try {
        PillowCImageDrawRectangle(clipped, [-1, -1, 2, 2], [7], [9], 1)
        AhkTest.AssertEqual([
            7, 7, 9, 0, 0, 0,
            7, 7, 9, 0, 0, 0,
            9, 9, 9, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(clipped, 30))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_rectangle",
            "Ptr", reversed,
            "Int", 2,
            "Int", 2,
            "Int", 1,
            "Int", 1,
            "Ptr", 0,
            "UPtr", 0,
            "Ptr", PillowCBuffer([9]),
            "UPtr", 1,
            "Int", 1,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)

        PillowCImageSetBytes(reversed, [1, 2, 3, 4, 5, 6, 7, 8, 9])
        PillowCImageDrawRectangle(reversed, [0, 0, 2, 2], unset, [7], 0)
        AhkTest.AssertEqual([1, 2, 3, 4, 5, 6, 7, 8, 9], PillowCImageToArray(reversed, 9))
    } finally {
        for handle in [reversed, clipped] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_rectangle clips and rejects invalid arguments", PillowCTestImageDrawRectangleClipsAndRejectsInvalidArguments)

PillowCTestImageDrawEllipseMatchesPillowFillOutlineWidth(*) {
    l := PillowCCreateImageMode(7, 6, 1)
    outline := PillowCCreateImageMode(7, 6, 1)
    rgb := PillowCCreateImageMode(5, 4, 3)
    try {
        PillowCImageDrawEllipse(l, [1, 1, 5, 4], [7], [9], 2)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 9, 9, 9, 9, 9, 0,
            0, 9, 9, 9, 9, 9, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(l, 42))

        PillowCImageDrawEllipse(outline, [1, 1, 5, 4], unset, [9], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 9, 0, 0, 0, 9, 0,
            0, 9, 0, 0, 0, 9, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(outline, 42))

        PillowCImageDrawEllipse(rgb, [0, 0, 4, 3], [10, 20, 30], [90, 80, 70], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
            90, 80, 70, 10, 20, 30, 10, 20, 30, 10, 20, 30, 90, 80, 70,
            90, 80, 70, 10, 20, 30, 10, 20, 30, 10, 20, 30, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
        ], PillowCImageToArray(rgb, 60))
    } finally {
        for handle in [rgb, outline, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_ellipse matches Pillow fill outline and width", PillowCTestImageDrawEllipseMatchesPillowFillOutlineWidth)

PillowCTestImageDrawEllipseClipsAndRejectsInvalidArguments(*) {
    clipped := PillowCCreateImageMode(6, 5, 1)
    widthZero := PillowCCreateImageMode(4, 4, 1)
    reversed := PillowCCreateImageMode(3, 3, 1)
    try {
        PillowCImageDrawEllipse(clipped, [-2, -1, 3, 3], [6])
        AhkTest.AssertEqual([
            6, 6, 6, 6, 0, 0,
            6, 6, 6, 6, 0, 0,
            6, 6, 6, 6, 0, 0,
            6, 6, 6, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(clipped, 30))

        PillowCImageDrawEllipse(widthZero, [0, 0, 3, 3], unset, [5], 0)
        AhkTest.AssertEqual([
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ], PillowCImageToArray(widthZero, 16))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_ellipse",
            "Ptr", reversed,
            "Int", 2,
            "Int", 2,
            "Int", 1,
            "Int", 1,
            "Ptr", 0,
            "UPtr", 0,
            "Ptr", PillowCBuffer([9]),
            "UPtr", 1,
            "Int", 1,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
    } finally {
        for handle in [reversed, widthZero, clipped] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_ellipse clips and rejects invalid arguments", PillowCTestImageDrawEllipseClipsAndRejectsInvalidArguments)

PillowCTestImageDrawArcMatchesPillowAnglesWidthAndRgb(*) {
    quarter := PillowCCreateImageMode(7, 7, 1)
    wide := PillowCCreateImageMode(7, 7, 1)
    wrap := PillowCCreateImageMode(7, 7, 1)
    rgb := PillowCCreateImageMode(5, 4, 3)
    try {
        PillowCImageDrawArc(quarter, [1, 1, 5, 5], 0, 90, [7], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 7, 0,
            0, 0, 0, 0, 0, 7, 0,
            0, 0, 0, 7, 7, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(quarter, 49))

        PillowCImageDrawArc(wide, [1, 1, 5, 5], 0, 270, [8], 2)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 8, 8, 0, 0, 0,
            0, 8, 8, 8, 0, 0, 0,
            0, 8, 8, 0, 8, 8, 0,
            0, 8, 8, 8, 8, 8, 0,
            0, 0, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(wide, 49))

        PillowCImageDrawArc(wrap, [1, 1, 5, 5], 300, 60, [9], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 9, 0, 0,
            0, 0, 0, 0, 0, 9, 0,
            0, 0, 0, 0, 0, 9, 0,
            0, 0, 0, 0, 0, 9, 0,
            0, 0, 0, 0, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(wrap, 49))

        PillowCImageDrawArc(rgb, [0, 0, 4, 3], 0, 180, [10, 20, 30], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            10, 20, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 20, 30,
            0, 0, 0, 10, 20, 30, 10, 20, 30, 10, 20, 30, 0, 0, 0,
        ], PillowCImageToArray(rgb, 60))
    } finally {
        for handle in [rgb, wrap, wide, quarter] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_arc matches Pillow angles width and RGB", PillowCTestImageDrawArcMatchesPillowAnglesWidthAndRgb)

PillowCTestImageDrawArcClipsAndRejectsInvalidArguments(*) {
    clipped := PillowCCreateImageMode(6, 5, 1)
    widthZero := PillowCCreateImageMode(4, 4, 1)
    reversed := PillowCCreateImageMode(3, 3, 1)
    try {
        PillowCImageDrawArc(clipped, [-2, -1, 3, 3], 0, 270, [6], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 6, 0, 0,
            0, 0, 0, 6, 0, 0,
            6, 6, 6, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(clipped, 30))

        PillowCImageDrawArc(widthZero, [0, 0, 3, 3], 0, 360, [5], 0)
        AhkTest.AssertEqual([
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ], PillowCImageToArray(widthZero, 16))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_arc",
            "Ptr", reversed,
            "Int", 2,
            "Int", 2,
            "Int", 1,
            "Int", 1,
            "Double", 0,
            "Double", 90,
            "Ptr", PillowCBuffer([9]),
            "UPtr", 1,
            "Int", 1,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
    } finally {
        for handle in [reversed, widthZero, clipped] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_arc clips and rejects invalid arguments", PillowCTestImageDrawArcClipsAndRejectsInvalidArguments)

PillowCTestImageDrawChordMatchesPillowFillOutlineWidthAndRgb(*) {
    fillOutline := PillowCCreateImageMode(7, 7, 1)
    outlineWide := PillowCCreateImageMode(7, 7, 1)
    wrapFill := PillowCCreateImageMode(7, 7, 1)
    rgb := PillowCCreateImageMode(5, 4, 3)
    try {
        PillowCImageDrawChord(fillOutline, [1, 1, 5, 5], 0, 180, [7], [9], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 9, 9, 9, 9, 9, 0,
            0, 9, 9, 9, 9, 9, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(fillOutline, 49))

        PillowCImageDrawChord(outlineWide, [1, 1, 5, 5], 0, 180, unset, [8], 2)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 8, 8, 8, 8, 8, 0,
            0, 8, 8, 8, 8, 8, 0,
            0, 0, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(outlineWide, 49))

        PillowCImageDrawChord(wrapFill, [1, 1, 5, 5], 300, 60, [6])
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 6, 0, 0,
            0, 0, 0, 0, 6, 6, 0,
            0, 0, 0, 0, 6, 6, 0,
            0, 0, 0, 0, 6, 6, 0,
            0, 0, 0, 0, 6, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(wrapFill, 49))

        PillowCImageDrawChord(rgb, [0, 0, 4, 3], 0, 180, [10, 20, 30], [90, 80, 70], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
        ], PillowCImageToArray(rgb, 60))
    } finally {
        for handle in [rgb, wrapFill, outlineWide, fillOutline] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_chord matches Pillow fill outline width and RGB", PillowCTestImageDrawChordMatchesPillowFillOutlineWidthAndRgb)

PillowCTestImageDrawChordClipsAndRejectsInvalidArguments(*) {
    clipped := PillowCCreateImageMode(6, 5, 1)
    widthZero := PillowCCreateImageMode(4, 4, 1)
    reversed := PillowCCreateImageMode(3, 3, 1)
    try {
        PillowCImageDrawChord(clipped, [-2, -1, 3, 3], 0, 270, [5], [8], 1)
        AhkTest.AssertEqual([
            8, 8, 0, 0, 0, 0,
            5, 8, 8, 8, 0, 0,
            5, 5, 8, 8, 0, 0,
            8, 8, 8, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(clipped, 30))

        PillowCImageDrawChord(widthZero, [0, 0, 3, 3], 0, 180, unset, [5], 0)
        AhkTest.AssertEqual([
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ], PillowCImageToArray(widthZero, 16))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_chord",
            "Ptr", reversed,
            "Int", 2,
            "Int", 2,
            "Int", 1,
            "Int", 1,
            "Double", 0,
            "Double", 90,
            "Ptr", PillowCBuffer([7]),
            "UPtr", 1,
            "Ptr", 0,
            "UPtr", 0,
            "Int", 1,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
    } finally {
        for handle in [reversed, widthZero, clipped] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_chord clips and rejects invalid arguments", PillowCTestImageDrawChordClipsAndRejectsInvalidArguments)

PillowCTestImageDrawPiesliceMatchesPillowFillOutlineWidthAndRgb(*) {
    fillOutline := PillowCCreateImageMode(7, 7, 1)
    outlineWide := PillowCCreateImageMode(7, 7, 1)
    wrapFill := PillowCCreateImageMode(7, 7, 1)
    rgb := PillowCCreateImageMode(5, 4, 3)
    try {
        PillowCImageDrawPieslice(fillOutline, [1, 1, 5, 5], 0, 90, [7], [9], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 9, 9, 9, 9, 0,
            0, 0, 9, 9, 7, 9, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(fillOutline, 49))

        PillowCImageDrawPieslice(outlineWide, [1, 1, 5, 5], 0, 180, unset, [8], 2)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            0, 8, 8, 8, 8, 8, 0,
            0, 8, 8, 8, 8, 8, 0,
            0, 8, 8, 8, 8, 8, 0,
            0, 0, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(outlineWide, 49))

        PillowCImageDrawPieslice(wrapFill, [1, 1, 5, 5], 300, 60, [6])
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 6, 0, 0,
            0, 0, 0, 0, 6, 6, 0,
            0, 0, 0, 6, 6, 6, 0,
            0, 0, 0, 0, 6, 6, 0,
            0, 0, 0, 0, 6, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(wrapFill, 49))

        PillowCImageDrawPieslice(rgb, [0, 0, 4, 3], 0, 180, [10, 20, 30], [90, 80, 70], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70,
            90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
        ], PillowCImageToArray(rgb, 60))
    } finally {
        for handle in [rgb, wrapFill, outlineWide, fillOutline] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_pieslice matches Pillow fill outline width and RGB", PillowCTestImageDrawPiesliceMatchesPillowFillOutlineWidthAndRgb)

PillowCTestImageDrawPiesliceClipsAndRejectsInvalidArguments(*) {
    clipped := PillowCCreateImageMode(6, 5, 1)
    widthZero := PillowCCreateImageMode(4, 4, 1)
    reversed := PillowCCreateImageMode(3, 3, 1)
    try {
        PillowCImageDrawPieslice(clipped, [-2, -1, 3, 3], 0, 270, [5], [8], 1)
        AhkTest.AssertEqual([
            8, 8, 0, 0, 0, 0,
            8, 8, 8, 8, 0, 0,
            5, 5, 5, 8, 0, 0,
            8, 8, 8, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(clipped, 30))

        PillowCImageDrawPieslice(widthZero, [0, 0, 3, 3], 0, 180, unset, [5], 0)
        AhkTest.AssertEqual([
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        ], PillowCImageToArray(widthZero, 16))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_pieslice",
            "Ptr", reversed,
            "Int", 2,
            "Int", 2,
            "Int", 1,
            "Int", 1,
            "Double", 0,
            "Double", 90,
            "Ptr", PillowCBuffer([7]),
            "UPtr", 1,
            "Ptr", 0,
            "UPtr", 0,
            "Int", 1,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
    } finally {
        for handle in [reversed, widthZero, clipped] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_pieslice clips and rejects invalid arguments", PillowCTestImageDrawPiesliceClipsAndRejectsInvalidArguments)

PillowCTestImageDrawRoundedRectangleMatchesPillowFillOutlineAndCorners(*) {
    fillOutline := PillowCCreateImageMode(8, 7, 1)
    outlineWide := PillowCCreateImageMode(8, 7, 1)
    partial := PillowCCreateImageMode(8, 7, 1)
    fullY := PillowCCreateImageMode(9, 6, 1)
    rgb := PillowCCreateImageMode(6, 5, 3)
    try {
        PillowCImageDrawRoundedRectangle(fillOutline, [1, 1, 6, 5], 2, [7], [9], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 9, 9, 9, 9, 0, 0,
            0, 9, 7, 7, 7, 7, 9, 0,
            0, 9, 7, 7, 7, 7, 9, 0,
            0, 9, 7, 7, 7, 7, 9, 0,
            0, 0, 9, 9, 9, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(fillOutline, 56))

        PillowCImageDrawRoundedRectangle(outlineWide, [1, 1, 6, 5], 2, unset, [8], 2)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 8, 8, 8, 8, 0, 0,
            0, 8, 8, 8, 8, 8, 8, 0,
            0, 8, 8, 0, 0, 8, 8, 0,
            0, 8, 8, 8, 8, 8, 8, 0,
            0, 0, 8, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(outlineWide, 56))

        PillowCImageDrawRoundedRectangle(partial, [1, 1, 6, 5], 2, [5], [8], 1, 5)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 8, 8, 8, 8, 8, 0,
            0, 8, 5, 5, 5, 5, 8, 0,
            0, 8, 5, 5, 5, 5, 8, 0,
            0, 8, 5, 5, 5, 5, 8, 0,
            0, 8, 8, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(partial, 56))

        PillowCImageDrawRoundedRectangle(fullY, [1, 1, 7, 4], 2, [5], [8], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 8, 8, 8, 8, 8, 0, 0,
            0, 8, 5, 5, 5, 5, 5, 8, 0,
            0, 8, 5, 5, 5, 5, 5, 8, 0,
            0, 0, 8, 8, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(fullY, 54))

        PillowCImageDrawRoundedRectangle(rgb, [0, 0, 5, 4], 2, [10, 20, 30], [90, 80, 70], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
            90, 80, 70, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 90, 80, 70,
            90, 80, 70, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 90, 80, 70,
            90, 80, 70, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 90, 80, 70, 90, 80, 70, 90, 80, 70, 0, 0, 0,
        ], PillowCImageToArray(rgb, 90))
    } finally {
        for handle in [rgb, fullY, partial, outlineWide, fillOutline] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_rounded_rectangle matches Pillow fill outline and corners", PillowCTestImageDrawRoundedRectangleMatchesPillowFillOutlineAndCorners)

PillowCTestImageDrawRoundedRectangleFallbacksClippingAndRejectsInvalidArguments(*) {
    radiusZero := PillowCCreateImageMode(6, 5, 1)
    fullEllipse := PillowCCreateImageMode(7, 7, 1)
    clipped := PillowCCreateImageMode(6, 5, 1)
    reversed := PillowCCreateImageMode(3, 3, 1)
    try {
        PillowCImageDrawRoundedRectangle(radiusZero, [1, 1, 4, 3], 0, [6], [9], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 9, 9, 9, 9, 0,
            0, 9, 6, 6, 9, 0,
            0, 9, 9, 9, 9, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(radiusZero, 30))

        PillowCImageDrawRoundedRectangle(fullEllipse, [1, 1, 5, 5], 4, [7], [9], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 9, 7, 7, 7, 9, 0,
            0, 9, 7, 7, 7, 9, 0,
            0, 9, 7, 7, 7, 9, 0,
            0, 0, 9, 9, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(fullEllipse, 49))

        PillowCImageDrawRoundedRectangle(clipped, [-2, -1, 4, 3], 2, [5], [8], 1)
        AhkTest.AssertEqual([
            5, 0, 5, 5, 8, 0,
            5, 0, 5, 5, 8, 0,
            5, 0, 5, 5, 8, 0,
            8, 8, 8, 8, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(clipped, 30))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_rounded_rectangle",
            "Ptr", reversed,
            "Int", 2,
            "Int", 2,
            "Int", 1,
            "Int", 1,
            "Double", 2,
            "Ptr", PillowCBuffer([7]),
            "UPtr", 1,
            "Ptr", 0,
            "UPtr", 0,
            "Int", 1,
            "Int", 15,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
    } finally {
        for handle in [reversed, clipped, fullEllipse, radiusZero] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_rounded_rectangle fallbacks clipping and rejects invalid arguments", PillowCTestImageDrawRoundedRectangleFallbacksClippingAndRejectsInvalidArguments)

PillowCTestImageDrawBitmapMatchesPillowMaskModesAndClipping(*) {
    oneTarget := PillowCCreateImageMode(6, 5, 1)
    oneMask := PillowCCreateImageMode(3, 2, 5)
    rgbTarget := PillowCCreateImageMode(5, 4, 3)
    rgbaMask := PillowCCreateImageMode(3, 2, 4)
    rgbaTarget := PillowCCreateImageMode(4, 3, 4)
    lMask := PillowCCreateImageMode(2, 2, 1)
    try {
        PillowCImageSetBytes(oneMask, [0, 255, 0, 255, 0, 255])
        PillowCImageDrawBitmap(oneTarget, [2, 1], oneMask, [7])
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 7, 0, 0,
            0, 0, 7, 0, 7, 0,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(oneTarget, 30))

        PillowCImageFill(rgbTarget, [100, 100, 100])
        PillowCImageSetBytes(rgbaMask, [
            0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 255,
            0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 255,
        ])
        PillowCImageDrawBitmap(rgbTarget, [-1, 1], rgbaMask, [10, 20, 30])
        AhkTest.AssertEqual([
            100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
            77, 80, 82, 10, 20, 30, 100, 100, 100, 100, 100, 100, 100, 100, 100,
            100, 100, 100, 10, 20, 30, 100, 100, 100, 100, 100, 100, 100, 100, 100,
            100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
        ], PillowCImageToArray(rgbTarget, 60))

        PillowCImageFill(rgbaTarget, [1, 2, 3, 4])
        PillowCImageSetBytes(lMask, [0, 64, 128, 255])
        PillowCImageDrawBitmap(rgbaTarget, [1, 1], lMask, [10, 20, 30, 200])
        AhkTest.AssertEqual([
            1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4,
            1, 2, 3, 4, 1, 2, 3, 4, 3, 7, 10, 53, 1, 2, 3, 4,
            1, 2, 3, 4, 6, 11, 17, 102, 10, 20, 30, 200, 1, 2, 3, 4,
        ], PillowCImageToArray(rgbaTarget, 48))
    } finally {
        for handle in [lMask, rgbaTarget, rgbaMask, rgbTarget, oneMask, oneTarget] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_bitmap matches Pillow mask modes and clipping", PillowCTestImageDrawBitmapMatchesPillowMaskModesAndClipping)

PillowCTestImageDrawBitmapRejectsInvalidArguments(*) {
    target := PillowCCreateImageMode(4, 3, 1)
    rgbMask := PillowCCreateImageMode(2, 2, 3)
    wrongTarget := PillowCCreateImageMode(4, 3, 3)
    mask := PillowCCreateImageMode(2, 2, 1)
    try {
        PillowCImageSetBytes(rgbMask, [0, 0, 0, 1, 0, 0, 0, 0, 1, 255, 255, 255])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_bitmap",
            "Ptr", target,
            "Int", 1,
            "Int", 1,
            "Ptr", rgbMask,
            "Ptr", PillowCBuffer([7]),
            "UPtr", 1,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_bitmap",
            "Ptr", wrongTarget,
            "Int", 1,
            "Int", 1,
            "Ptr", mask,
            "Ptr", PillowCBuffer([7]),
            "UPtr", 1,
            "Int"
        )
        AhkTest.AssertEqual(-2, status)
    } finally {
        for handle in [mask, wrongTarget, rgbMask, target] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_bitmap rejects invalid arguments", PillowCTestImageDrawBitmapRejectsInvalidArguments)

PillowCTestImageDrawFloodfillMatchesPillowFillModes(*) {
    noBorder := PillowCCreateImageMode(5, 4, 1)
    threshold := PillowCCreateImageMode(5, 3, 1)
    border := PillowCCreateImageMode(5, 4, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    try {
        PillowCImageSetBytes(noBorder, [
            1, 1, 9, 9, 9,
            1, 2, 2, 9, 9,
            1, 2, 3, 3, 9,
            1, 1, 3, 9, 9,
        ])
        PillowCImageDrawFloodfill(noBorder, [1, 1], [7])
        AhkTest.AssertEqual([
            1, 1, 9, 9, 9,
            1, 7, 7, 9, 9,
            1, 7, 3, 3, 9,
            1, 1, 3, 9, 9,
        ], PillowCImageToArray(noBorder, 20))

        PillowCImageSetBytes(threshold, [
            10, 11, 14, 20, 20,
            9, 12, 13, 21, 20,
            8, 30, 13, 22, 20,
        ])
        PillowCImageDrawFloodfill(threshold, [0, 0], [99], unset, 3.0)
        AhkTest.AssertEqual([
            99, 99, 14, 20, 20,
            99, 99, 99, 21, 20,
            99, 30, 99, 22, 20,
        ], PillowCImageToArray(threshold, 15))

        PillowCImageSetBytes(border, [
            1, 1, 1, 1, 1,
            1, 2, 2, 8, 1,
            1, 2, 3, 8, 1,
            1, 1, 1, 1, 1,
        ])
        PillowCImageDrawFloodfill(border, [1, 1], [7], [1])
        AhkTest.AssertEqual([
            1, 1, 1, 1, 1,
            1, 7, 7, 7, 1,
            1, 7, 7, 7, 1,
            1, 1, 1, 1, 1,
        ], PillowCImageToArray(border, 20))

        PillowCImageSetBytes(rgb, [
            10, 20, 30, 11, 21, 31, 100, 0, 0,
            9, 19, 29, 50, 50, 50, 100, 0, 0,
        ])
        PillowCImageDrawFloodfill(rgb, [0, 0], [1, 2, 3], unset, 3.0)
        AhkTest.AssertEqual([
            1, 2, 3, 1, 2, 3, 100, 0, 0,
            1, 2, 3, 50, 50, 50, 100, 0, 0,
        ], PillowCImageToArray(rgb, 18))
    } finally {
        for handle in [rgb, border, threshold, noBorder] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_floodfill matches Pillow fill modes", PillowCTestImageDrawFloodfillMatchesPillowFillModes)

PillowCTestImageDrawFloodfillHandlesSeedAndRejectsInvalidArguments(*) {
    negativeSeed := PillowCCreateImageMode(3, 1, 1)
    outside := PillowCCreateImageMode(3, 1, 1)
    alreadyFill := PillowCCreateImageMode(3, 1, 1)
    target := PillowCCreateImageMode(3, 1, 1)
    try {
        PillowCImageSetBytes(negativeSeed, [3, 3, 3])
        PillowCImageDrawFloodfill(negativeSeed, [-1, 0], [9])
        AhkTest.AssertEqual([9, 9, 9], PillowCImageToArray(negativeSeed, 3))

        PillowCImageSetBytes(outside, [1, 2, 3])
        PillowCImageDrawFloodfill(outside, [3, 0], [9])
        AhkTest.AssertEqual([1, 2, 3], PillowCImageToArray(outside, 3))

        PillowCImageSetBytes(alreadyFill, [5, 5, 6])
        PillowCImageDrawFloodfill(alreadyFill, [0, 0], [5])
        AhkTest.AssertEqual([5, 5, 6], PillowCImageToArray(alreadyFill, 3))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_floodfill",
            "Ptr", target,
            "Int", 0,
            "Int", 0,
            "Ptr", PillowCBuffer([7, 8]),
            "UPtr", 2,
            "Ptr", 0,
            "UPtr", 0,
            "Double", 0.0,
            "Int"
        )
        AhkTest.AssertEqual(-2, status)
    } finally {
        for handle in [target, alreadyFill, outside, negativeSeed] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_floodfill handles seeds and rejects invalid arguments", PillowCTestImageDrawFloodfillHandlesSeedAndRejectsInvalidArguments)

PillowCTestImageDrawLineMatchesPillowWidthZeroAndOne(*) {
    horizontal := PillowCCreateImageMode(6, 4, 1)
    diagonal := PillowCCreateImageMode(6, 6, 1)
    rgb := PillowCCreateImageMode(4, 4, 3)
    try {
        PillowCImageDrawLine(horizontal, [1, 1, 4, 1], [7], 0)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 7, 7, 7, 7, 0,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(horizontal, 24))

        PillowCImageDrawLine(diagonal, [0, 0, 5, 3], [7], 1)
        AhkTest.AssertEqual([
            7, 0, 0, 0, 0, 0,
            0, 7, 7, 0, 0, 0,
            0, 0, 0, 7, 7, 0,
            0, 0, 0, 0, 0, 7,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(diagonal, 36))

        PillowCImageDrawLine(rgb, [0, 0, 3, 2], [10, 20, 30], 1)
        AhkTest.AssertEqual([
            10, 20, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 10, 20, 30, 10, 20, 30, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 20, 30,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(rgb, 48))
    } finally {
        for handle in [rgb, diagonal, horizontal] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_line matches Pillow width zero and one", PillowCTestImageDrawLineMatchesPillowWidthZeroAndOne)

PillowCTestImageDrawLineSupportsPolylinesClippingAndRejectsInvalidArguments(*) {
    polyline := PillowCCreateImageMode(6, 5, 1)
    clipped := PillowCCreateImageMode(6, 4, 1)
    offdiag := PillowCCreateImageMode(5, 5, 1)
    try {
        PillowCImageDrawLine(polyline, [0, 0, 3, 2, 5, 1], [7], 1)
        AhkTest.AssertEqual([
            7, 0, 0, 0, 0, 0,
            0, 7, 7, 0, 7, 7,
            0, 0, 0, 7, 0, 0,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(polyline, 30))

        PillowCImageDrawLine(clipped, [-2, 1, 3, 1], [7], 1)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            7, 7, 7, 7, 0, 0,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(clipped, 24))

        PillowCImageDrawLine(offdiag, [-1, -1, 3, 3], [7], 1)
        AhkTest.AssertEqual([
            7, 0, 0, 0, 0,
            0, 7, 0, 0, 0,
            0, 0, 7, 0, 0,
            0, 0, 0, 7, 0,
            0, 0, 0, 0, 0,
        ], PillowCImageToArray(offdiag, 25))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_line",
            "Ptr", polyline,
            "Ptr", PillowCIntBuffer([0, 0]),
            "UPtr", 1,
            "Ptr", PillowCBuffer([7]),
            "UPtr", 1,
            "Int", 1,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_line",
            "Ptr", polyline,
            "Ptr", PillowCIntBuffer([0, 0, 1, 1]),
            "UPtr", 2,
            "Ptr", PillowCBuffer([7]),
            "UPtr", 1,
            "Int", 0,
            "Int"
        )
        AhkTest.AssertEqual(0, status)
    } finally {
        for handle in [offdiag, clipped, polyline] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_line supports polylines clipping and rejects invalid arguments", PillowCTestImageDrawLineSupportsPolylinesClippingAndRejectsInvalidArguments)

PillowCTestImageDrawLineSupportsWideLines(*) {
    horizontal := PillowCCreateImageMode(7, 5, 1)
    vertical := PillowCCreateImageMode(6, 7, 1)
    diagonal := PillowCCreateImageMode(7, 7, 1)
    rgb := PillowCCreateImageMode(5, 4, 3)
    try {
        PillowCImageDrawLine(horizontal, [1, 2, 5, 2], [7], 3)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0,
            0, 7, 7, 7, 7, 7, 0,
            0, 7, 7, 7, 7, 7, 0,
            0, 7, 7, 7, 7, 7, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(horizontal, 35))

        PillowCImageDrawLine(vertical, [2, 1, 2, 5], [8], 4)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 8, 8, 8, 8, 0,
            0, 8, 8, 8, 8, 0,
            0, 8, 8, 8, 8, 0,
            0, 8, 8, 8, 8, 0,
            0, 8, 8, 8, 8, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(vertical, 42))

        PillowCImageDrawLine(diagonal, [1, 1, 5, 4], [9], 3)
        AhkTest.AssertEqual([
            0, 0, 9, 0, 0, 0, 0,
            0, 9, 9, 9, 0, 0, 0,
            9, 9, 9, 9, 9, 9, 0,
            0, 9, 9, 9, 9, 9, 9,
            0, 0, 0, 9, 9, 9, 0,
            0, 0, 0, 0, 9, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(diagonal, 49))

        PillowCImageDrawLine(rgb, [0, 1, 4, 1], [10, 20, 30], 3)
        AhkTest.AssertEqual([
            10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30,
            10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30,
            10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(rgb, 60))
    } finally {
        for handle in [rgb, diagonal, vertical, horizontal] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_line supports wide lines", PillowCTestImageDrawLineSupportsWideLines)

PillowCTestImageDrawLineSupportsWidePolylineAndClipping(*) {
    polyline := PillowCCreateImageMode(7, 6, 1)
    clipped := PillowCCreateImageMode(5, 4, 1)
    try {
        PillowCImageDrawLine(polyline, [0, 0, 3, 3, 6, 1], [6], 3)
        AhkTest.AssertEqual([
            6, 6, 6, 0, 0, 6, 0,
            6, 6, 6, 6, 6, 6, 6,
            6, 6, 6, 6, 6, 6, 6,
            0, 6, 6, 6, 6, 6, 0,
            0, 0, 6, 0, 6, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(polyline, 42))

        PillowCImageDrawLine(clipped, [-2, 1, 3, 1], [5], 3)
        AhkTest.AssertEqual([
            5, 5, 5, 5, 0,
            5, 5, 5, 5, 0,
            5, 5, 5, 5, 0,
            0, 0, 0, 0, 0,
        ], PillowCImageToArray(clipped, 20))
    } finally {
        for handle in [clipped, polyline] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_line supports wide polyline and clipping", PillowCTestImageDrawLineSupportsWidePolylineAndClipping)

PillowCTestImageDrawLineSupportsCurveJoints(*) {
    curve := PillowCCreateImageMode(9, 9, 1)
    gap := PillowCCreateImageMode(12, 10, 1)
    straight := PillowCCreateImageMode(9, 9, 1)
    clippedGap := PillowCCreateImageMode(18, 9, 1)
    try {
        PillowCImageDrawLineJoint(curve, [1, 7, 4, 1, 7, 7], [7], 5, true)
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
        ], PillowCImageToArray(curve, 81))

        PillowCImageDrawLineJoint(gap, [1, 8, 5, 1, 10, 8], [9], 9, true)
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
        ], PillowCImageToArray(gap, 120))

        PillowCImageDrawLineJoint(straight, [1, 7, 4, 4, 7, 1], [5], 7, true)
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
        ], PillowCImageToArray(straight, 81))

        PillowCImageDrawLineJoint(clippedGap, [-4, 1, 15, 4, 13, 0], [17], 9, true)
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
        ], PillowCImageToArray(clippedGap, 162))
    } finally {
        for handle in [clippedGap, straight, gap, curve] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_line supports curve joints", PillowCTestImageDrawLineSupportsCurveJoints)

PillowCTestImageDrawPointsMatchesPillowSingleMultipleAndRgb(*) {
    l := PillowCCreateImageMode(5, 4, 1)
    rgb := PillowCCreateImageMode(3, 3, 3)
    single := PillowCCreateImageMode(2, 2, 1)
    try {
        PillowCImageDrawPoints(l, [0, 0, 2, 1, 4, 3], [7])
        AhkTest.AssertEqual([
            7, 0, 0, 0, 0,
            0, 0, 7, 0, 0,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 7,
        ], PillowCImageToArray(l, 20))

        PillowCImageDrawPoints(rgb, [0, 0, 2, 1], [10, 20, 30])
        AhkTest.AssertEqual([
            10, 20, 30, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 10, 20, 30,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(rgb, 27))

        PillowCImageDrawPoints(single, [1, 1], [5])
        AhkTest.AssertEqual([0, 0, 0, 5], PillowCImageToArray(single, 4))
    } finally {
        for handle in [single, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_points matches Pillow single multiple and RGB", PillowCTestImageDrawPointsMatchesPillowSingleMultipleAndRgb)

PillowCTestImageDrawPointsClipsAllowsEmptyAndRejectsInvalidArguments(*) {
    clipped := PillowCCreateImageMode(5, 4, 1)
    empty := PillowCCreateImageMode(2, 2, 1)
    try {
        PillowCImageDrawPoints(clipped, [-1, -1, 0, 0, 2, 2, 5, 4], [9])
        AhkTest.AssertEqual([
            9, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            0, 0, 9, 0, 0,
            0, 0, 0, 0, 0,
        ], PillowCImageToArray(clipped, 20))

        fillBuffer := PillowCBuffer([7])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_points",
            "Ptr", empty,
            "Ptr", 0,
            "UPtr", 0,
            "Ptr", fillBuffer,
            "UPtr", fillBuffer.Size,
            "Int"
        )
        AhkTest.AssertEqual(0, status)
        AhkTest.AssertEqual([0, 0, 0, 0], PillowCImageToArray(empty, 4))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_points",
            "Ptr", clipped,
            "Ptr", PillowCIntBuffer([0, 0]),
            "UPtr", 1,
            "Ptr", PillowCBuffer([1, 2]),
            "UPtr", 2,
            "Int"
        )
        AhkTest.AssertEqual(-2, status)
    } finally {
        for handle in [empty, clipped] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_points clips allows empty and rejects invalid arguments", PillowCTestImageDrawPointsClipsAllowsEmptyAndRejectsInvalidArguments)

PillowCTestImageDrawPolygonMatchesPillowFillOutlineAndRgb(*) {
    fill := PillowCCreateImageMode(6, 5, 1)
    outline := PillowCCreateImageMode(6, 5, 1)
    fillOutline := PillowCCreateImageMode(6, 5, 1)
    rgb := PillowCCreateImageMode(4, 4, 3)
    try {
        points := [1, 1, 4, 1, 2, 3]
        PillowCImageDrawPolygon(fill, points, [7])
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 7, 7, 7, 7, 0,
            0, 0, 7, 7, 0, 0,
            0, 0, 7, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(fill, 30))

        PillowCImageDrawPolygon(outline, points, unset, [9])
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 9, 9, 9, 9, 0,
            0, 9, 0, 9, 0, 0,
            0, 0, 9, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(outline, 30))

        PillowCImageDrawPolygon(fillOutline, points, [7], [9])
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0,
            0, 9, 9, 9, 9, 0,
            0, 9, 7, 9, 0, 0,
            0, 0, 9, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(fillOutline, 30))

        PillowCImageDrawPolygon(rgb, [0, 0, 3, 1, 1, 3], [10, 20, 30], [90, 80, 70])
        AhkTest.AssertEqual([
            90, 80, 70, 90, 80, 70, 0, 0, 0, 0, 0, 0,
            90, 80, 70, 10, 20, 30, 90, 80, 70, 90, 80, 70,
            0, 0, 0, 90, 80, 70, 90, 80, 70, 0, 0, 0,
            0, 0, 0, 90, 80, 70, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(rgb, 48))
    } finally {
        for handle in [rgb, fillOutline, outline, fill] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_polygon matches Pillow fill outline and RGB", PillowCTestImageDrawPolygonMatchesPillowFillOutlineAndRgb)

PillowCTestImageDrawPolygonSupportsWideOutlines(*) {
    outline := PillowCCreateImageMode(8, 7, 1)
    fillOutline := PillowCCreateImageMode(8, 7, 1)
    rgb := PillowCCreateImageMode(7, 6, 3)
    lineLike := PillowCCreateImageMode(5, 5, 1)
    try {
        points := [1, 1, 6, 1, 3, 5]
        PillowCImageDrawPolygon(outline, points, unset, [9], 3)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 9, 9, 9, 9, 9, 9, 0,
            0, 0, 9, 9, 9, 9, 0, 0,
            0, 0, 9, 9, 9, 0, 0, 0,
            0, 0, 0, 9, 9, 0, 0, 0,
            0, 0, 0, 9, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(outline, 56))

        PillowCImageDrawPolygon(fillOutline, points, [4], [9], 3)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 9, 9, 9, 9, 9, 9, 0,
            0, 0, 9, 9, 9, 9, 0, 0,
            0, 0, 9, 9, 9, 0, 0, 0,
            0, 0, 0, 9, 9, 0, 0, 0,
            0, 0, 0, 9, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(fillOutline, 56))

        PillowCImageDrawPolygon(rgb, [0, 0, 6, 1, 2, 5], unset, [10, 20, 30], 4)
        AhkTest.AssertEqual([
            10, 20, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30,
            0, 0, 0, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 0, 0, 0,
            0, 0, 0, 10, 20, 30, 10, 20, 30, 10, 20, 30, 10, 20, 30, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 10, 20, 30, 10, 20, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 10, 20, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(rgb, 126))

        PillowCImageDrawPolygon(lineLike, [0, 0, 4, 4], unset, [7], 3)
        AhkTest.AssertEqual([
            7, 0, 0, 0, 0,
            0, 7, 0, 0, 0,
            0, 0, 7, 0, 0,
            0, 0, 0, 7, 0,
            0, 0, 0, 0, 7,
        ], PillowCImageToArray(lineLike, 25))
    } finally {
        for handle in [lineLike, rgb, fillOutline, outline] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_polygon supports wide outlines", PillowCTestImageDrawPolygonSupportsWideOutlines)

PillowCTestImageDrawPolygonClipsAndRejectsInvalidArguments(*) {
    clipped := PillowCCreateImageMode(5, 4, 1)
    lineLike := PillowCCreateImageMode(3, 3, 1)
    try {
        PillowCImageDrawPolygon(clipped, [-1, 1, 2, -1, 5, 3], [8])
        AhkTest.AssertEqual([
            0, 8, 8, 8, 0,
            8, 8, 8, 8, 0,
            0, 0, 8, 8, 8,
            0, 0, 0, 0, 0,
        ], PillowCImageToArray(clipped, 20))

        PillowCImageDrawPolygon(lineLike, [0, 0, 1, 1], [1])
        AhkTest.AssertEqual([
            1, 0, 0,
            0, 1, 0,
            0, 0, 0,
        ], PillowCImageToArray(lineLike, 9))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_draw_polygon",
            "Ptr", clipped,
            "Ptr", PillowCIntBuffer([0, 0]),
            "UPtr", 1,
            "Ptr", PillowCBuffer([7]),
            "UPtr", 1,
            "Ptr", 0,
            "UPtr", 0,
            "Int", 1,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)

        PillowCImageDrawPolygon(lineLike, [0, 0, 1, 1], unset, [7], 2)
        AhkTest.AssertEqual([
            7, 0, 0,
            0, 7, 0,
            0, 0, 0,
        ], PillowCImageToArray(lineLike, 9))
    } finally {
        for handle in [lineLike, clipped] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image draw_polygon clips and rejects invalid arguments", PillowCTestImageDrawPolygonClipsAndRejectsInvalidArguments)

PillowCTestImageBlendOperatesOnNativeHandles(*) {
    left := PillowCCreateImage(2, 1, 3)
    right := PillowCCreateImage(2, 1, 3)
    blended := 0
    try {
        PillowCImageSetBytes(left, [10, 20, 30, 100, 110, 120])
        PillowCImageSetBytes(right, [250, 240, 230, 1, 2, 3])
        blended := PillowCImageBlend(left, right, 0.25)
        AhkTest.AssertEqual(2, PillowCImageInt(blended, "pillow_c_image_width"))
        AhkTest.AssertEqual(1, PillowCImageInt(blended, "pillow_c_image_height"))
        AhkTest.AssertEqual(3, PillowCImageInt(blended, "pillow_c_image_channels"))
        AhkTest.AssertEqual([70, 75, 80, 75, 83, 90], PillowCImageToArray(blended, 6))
    } finally {
        if blended
            PillowCFreeImage(blended)
        PillowCFreeImage(right)
        PillowCFreeImage(left)
    }
}

AhkTest.Test("pillow_c image blend operates on native handles", PillowCTestImageBlendOperatesOnNativeHandles)

PillowCTestImageBlendIntoReusesTargetHandle(*) {
    left := PillowCCreateImage(2, 1, 3)
    right := PillowCCreateImage(2, 1, 3)
    target := PillowCCreateImage(2, 1, 3)
    try {
        PillowCImageSetBytes(left, [10, 20, 30, 100, 110, 120])
        PillowCImageSetBytes(right, [250, 240, 230, 1, 2, 3])
        before := PillowCImageData(target).Ptr
        PillowCImageBlendInto(left, right, 0.25, target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([70, 75, 80, 75, 83, 90], PillowCImageToArray(target, 6))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(right)
        PillowCFreeImage(left)
    }
}

AhkTest.Test("pillow_c image blend_into reuses target handle storage", PillowCTestImageBlendIntoReusesTargetHandle)

PillowCTestImageCompositeMatchesPillowMaskBlending(*) {
    left := PillowCCreateImageMode(3, 1, 3)
    right := PillowCCreateImageMode(3, 1, 3)
    mask := PillowCCreateImageMode(3, 1, 1)
    out := 0
    try {
        PillowCImageSetBytes(left, [10, 20, 30, 40, 50, 60, 70, 80, 90])
        PillowCImageSetBytes(right, [200, 210, 220, 180, 170, 160, 100, 90, 80])
        PillowCImageSetBytes(mask, [0, 128, 255])

        out := PillowCImageComposite(left, right, mask)

        AhkTest.AssertEqual(3, PillowCImageMode(out))
        AhkTest.AssertEqual([3, 1], [PillowCImageInt(out, "pillow_c_image_width"), PillowCImageInt(out, "pillow_c_image_height")])
        AhkTest.AssertEqual([200, 210, 220, 110, 110, 110, 70, 80, 90], PillowCImageToArray(out, 9))
    } finally {
        for handle in [out, mask, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image composite matches Pillow L mask blending", PillowCTestImageCompositeMatchesPillowMaskBlending)

PillowCTestImageCompositeUsesRgbaMaskAlpha(*) {
    left := PillowCCreateImageMode(2, 1, 3)
    right := PillowCCreateImageMode(2, 1, 3)
    mask := PillowCCreateImageMode(2, 1, 4)
    out := 0
    try {
        PillowCImageSetBytes(left, [10, 20, 30, 40, 50, 60])
        PillowCImageSetBytes(right, [200, 210, 220, 180, 170, 160])
        PillowCImageSetBytes(mask, [1, 2, 3, 128, 4, 5, 6, 255])

        out := PillowCImageComposite(left, right, mask)

        AhkTest.AssertEqual([105, 115, 125, 40, 50, 60], PillowCImageToArray(out, 6))
    } finally {
        for handle in [out, mask, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image composite uses RGBA mask alpha", PillowCTestImageCompositeUsesRgbaMaskAlpha)

PillowCTestImageCompositeUsesLaMaskAlpha(*) {
    left := PillowCCreateImageMode(2, 1, 3)
    right := PillowCCreateImageMode(2, 1, 3)
    mask := PillowCCreateImageMode(2, 1, 2)
    out := 0
    try {
        PillowCImageSetBytes(left, [10, 20, 30, 40, 50, 60])
        PillowCImageSetBytes(right, [200, 210, 220, 180, 170, 160])
        PillowCImageSetBytes(mask, [9, 128, 9, 255])

        out := PillowCImageComposite(left, right, mask)

        AhkTest.AssertEqual([105, 115, 125, 40, 50, 60], PillowCImageToArray(out, 6))
    } finally {
        for handle in [out, mask, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image composite uses LA mask alpha", PillowCTestImageCompositeUsesLaMaskAlpha)

PillowCTestImageCompositeUsesModeOneMaskAlpha(*) {
    left := PillowCCreateImageMode(3, 1, 1)
    right := PillowCCreateImageMode(3, 1, 1)
    mask := PillowCCreateImageMode(3, 1, 5)
    out := 0
    try {
        PillowCImageSetBytes(left, [10, 20, 30])
        PillowCImageSetBytes(right, [100, 110, 120])
        PillowCImageSetRawBytes(mask, [0xA0], "1")

        out := PillowCImageComposite(left, right, mask)

        AhkTest.AssertEqual(1, PillowCImageMode(out))
        AhkTest.AssertEqual([10, 110, 30], PillowCImageToArray(out, 3))
    } finally {
        for handle in [out, mask, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image composite uses mode 1 mask alpha", PillowCTestImageCompositeUsesModeOneMaskAlpha)

PillowCTestImageCompositeUsesTargetSizeAndClipsSource(*) {
    left := PillowCCreateImageMode(1, 1, 3)
    right := PillowCCreateImageMode(2, 1, 3)
    mask := PillowCCreateImageMode(1, 1, 1)
    out := 0
    try {
        PillowCImageSetBytes(left, [10, 20, 30])
        PillowCImageSetBytes(right, [200, 210, 220, 180, 170, 160])
        PillowCImageSetBytes(mask, [128])

        out := PillowCImageComposite(left, right, mask)

        AhkTest.AssertEqual([2, 1], [PillowCImageInt(out, "pillow_c_image_width"), PillowCImageInt(out, "pillow_c_image_height")])
        AhkTest.AssertEqual([105, 115, 125, 180, 170, 160], PillowCImageToArray(out, 6))
    } finally {
        for handle in [out, mask, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image composite uses target size and clips source", PillowCTestImageCompositeUsesTargetSizeAndClipsSource)

PillowCTestImageCompositeConvertsSourceToTargetMode(*) {
    rgbSource := PillowCCreateImageMode(2, 1, 3)
    lTarget := PillowCCreateImageMode(2, 1, 1)
    lMask := PillowCCreateImageMode(2, 1, 1)
    lOut := 0
    lSource := PillowCCreateImageMode(2, 1, 1)
    rgbTarget := PillowCCreateImageMode(2, 1, 3)
    rgbMask := PillowCCreateImageMode(2, 1, 1)
    rgbOut := 0
    rgbaTarget := PillowCCreateImageMode(2, 1, 4)
    rgbaOut := 0
    try {
        PillowCImageSetBytes(rgbSource, [10, 20, 30, 40, 50, 60])
        PillowCImageSetBytes(lTarget, [1, 2])
        PillowCImageSetBytes(lMask, [255, 128])
        lOut := PillowCImageComposite(rgbSource, lTarget, lMask)
        AhkTest.AssertEqual(1, PillowCImageMode(lOut))
        AhkTest.AssertEqual([18, 25], PillowCImageToArray(lOut, 2))

        PillowCImageSetBytes(lSource, [10, 40])
        PillowCImageSetBytes(rgbTarget, [200, 210, 220, 180, 170, 160])
        PillowCImageSetBytes(rgbMask, [255, 128])
        rgbOut := PillowCImageComposite(lSource, rgbTarget, rgbMask)
        AhkTest.AssertEqual(3, PillowCImageMode(rgbOut))
        AhkTest.AssertEqual([10, 10, 10, 110, 105, 100], PillowCImageToArray(rgbOut, 6))

        PillowCImageSetBytes(rgbaTarget, [200, 210, 220, 230, 1, 2, 3, 4])
        rgbaOut := PillowCImageComposite(rgbSource, rgbaTarget, rgbMask)
        AhkTest.AssertEqual(4, PillowCImageMode(rgbaOut))
        AhkTest.AssertEqual([10, 20, 30, 255, 21, 26, 32, 130], PillowCImageToArray(rgbaOut, 8))
    } finally {
        for handle in [rgbaOut, rgbaTarget, rgbOut, rgbMask, rgbTarget, lSource, lOut, lMask, lTarget, rgbSource] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image composite converts source to target mode like Pillow paste", PillowCTestImageCompositeConvertsSourceToTargetMode)

PillowCTestImageCompositeIntoReusesTargetStorage(*) {
    left := PillowCCreateImageMode(2, 1, 4)
    right := PillowCCreateImageMode(2, 1, 4)
    mask := PillowCCreateImageMode(2, 1, 1)
    target := PillowCCreateImageMode(2, 1, 4)
    try {
        PillowCImageSetBytes(left, [10, 20, 30, 40, 100, 110, 120, 130])
        PillowCImageSetBytes(right, [200, 210, 220, 230, 1, 2, 3, 4])
        PillowCImageSetBytes(mask, [128, 255])

        before := PillowCImageData(target).Ptr
        PillowCImageCompositeInto(left, right, mask, target)

        AhkTest.AssertEqual(before, PillowCImageData(target).Ptr)
        AhkTest.AssertEqual([105, 115, 125, 135, 100, 110, 120, 130], PillowCImageToArray(target, 8))
    } finally {
        for handle in [target, mask, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image composite_into reuses target storage", PillowCTestImageCompositeIntoReusesTargetStorage)

PillowCTestImageCompositeRejectsUnsupportedMaskOrTargetShape(*) {
    left := PillowCCreateImageMode(2, 1, 3)
    right := PillowCCreateImageMode(2, 1, 3)
    rgbMask := PillowCCreateImageMode(2, 1, 3)
    smallMask := PillowCCreateImageMode(1, 1, 1)
    wrongOut := PillowCCreateImageMode(2, 1, 1)
    try {
        outHandle := 0
        status := DllCall(PillowCDllPath() "\pillow_c_image_composite", "Ptr", left, "Ptr", right, "Ptr", rgbMask, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        outHandle := 0
        status := DllCall(PillowCDllPath() "\pillow_c_image_composite", "Ptr", left, "Ptr", right, "Ptr", smallMask, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(PillowCDllPath() "\pillow_c_image_composite_into", "Ptr", left, "Ptr", right, "Ptr", smallMask, "Ptr", wrongOut, "Int")
        AhkTest.AssertEqual(-5, status)
    } finally {
        for handle in [wrongOut, smallMask, rgbMask, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image composite rejects unsupported mask or target shape", PillowCTestImageCompositeRejectsUnsupportedMaskOrTargetShape)

PillowCTestImageConstantReturnsLImageLikePillow(*) {
    l := PillowCCreateImageMode(3, 2, 1)
    rgb := PillowCCreateImageMode(2, 2, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        lOut := PillowCImageConstant(l, 7)
        rgbOut := PillowCImageConstant(rgb, 300)
        rgbaOut := PillowCImageConstant(rgba, -1)

        AhkTest.AssertEqual(1, PillowCImageMode(lOut))
        AhkTest.AssertEqual([3, 2], [PillowCImageInt(lOut, "pillow_c_image_width"), PillowCImageInt(lOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([7, 7, 7, 7, 7, 7], PillowCImageToArray(lOut, 6))
        AhkTest.AssertEqual(1, PillowCImageMode(rgbOut))
        AhkTest.AssertEqual([255, 255, 255, 255], PillowCImageToArray(rgbOut, 4))
        AhkTest.AssertEqual([0, 0], PillowCImageToArray(rgbaOut, 2))
    } finally {
        for handle in [rgbaOut, rgbOut, lOut, rgba, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image constant returns an L image like Pillow", PillowCTestImageConstantReturnsLImageLikePillow)

PillowCTestImageConstantHandlesEmptyImages(*) {
    source := PillowCCreateImageMode(3, 2, 1)
    empty := 0
    out := 0
    try {
        empty := PillowCImageCrop(source, 1, 0, 1, 2)
        out := PillowCImageConstant(empty, 7)

        AhkTest.AssertEqual(1, PillowCImageMode(out))
        AhkTest.AssertEqual([0, 2], [PillowCImageInt(out, "pillow_c_image_width"), PillowCImageInt(out, "pillow_c_image_height")])
        AhkTest.AssertEqual([], PillowCImageToArray(out, 0))
    } finally {
        for handle in [out, empty, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image constant handles empty images", PillowCTestImageConstantHandlesEmptyImages)

PillowCTestImageConstantIntoReusesTargetStorage(*) {
    source := PillowCCreateImageMode(2, 2, 3)
    target := PillowCCreateImageMode(2, 2, 1)
    try {
        before := PillowCImageData(target).Ptr
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_constant_into",
            "Ptr", source,
            "Int", 7,
            "Ptr", target,
            "Int"
        )
        PillowCAssertStatus(status)

        AhkTest.AssertEqual(before, PillowCImageData(target).Ptr)
        AhkTest.AssertEqual([7, 7, 7, 7], PillowCImageToArray(target, 4))
    } finally {
        for handle in [target, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image constant_into reuses target storage", PillowCTestImageConstantIntoReusesTargetStorage)

PillowCTestImageConstantIntoRejectsTargetShapeMismatch(*) {
    source := PillowCCreateImageMode(2, 2, 3)
    wrongMode := PillowCCreateImageMode(2, 2, 3)
    wrongSize := PillowCCreateImageMode(3, 2, 1)
    try {
        status := DllCall(PillowCDllPath() "\pillow_c_image_constant_into", "Ptr", source, "Int", 7, "Ptr", wrongMode, "Int")
        AhkTest.AssertEqual(-5, status)
        status := DllCall(PillowCDllPath() "\pillow_c_image_constant_into", "Ptr", source, "Int", 7, "Ptr", wrongSize, "Int")
        AhkTest.AssertEqual(-5, status)
    } finally {
        for handle in [wrongSize, wrongMode, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image constant_into rejects target shape mismatch", PillowCTestImageConstantIntoRejectsTargetShapeMismatch)

PillowCTestImageChopsInvertMatchesPillowModes(*) {
    l := PillowCCreateImageMode(3, 2, 1)
    rgb := PillowCCreateImageMode(2, 2, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(rgb, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12])
        PillowCImageSetBytes(rgba, [1, 2, 3, 4, 5, 6, 7, 8])

        lOut := PillowCImageChopsInvert(l)
        rgbOut := PillowCImageChopsInvert(rgb)
        rgbaOut := PillowCImageChopsInvert(rgba)

        AhkTest.AssertEqual(1, PillowCImageMode(lOut))
        AhkTest.AssertEqual([254, 253, 252, 251, 250, 249], PillowCImageToArray(lOut, 6))
        AhkTest.AssertEqual(3, PillowCImageMode(rgbOut))
        AhkTest.AssertEqual([254, 253, 252, 251, 250, 249, 248, 247, 246, 245, 244, 243], PillowCImageToArray(rgbOut, 12))
        AhkTest.AssertEqual(4, PillowCImageMode(rgbaOut))
        AhkTest.AssertEqual([254, 253, 252, 251, 250, 249, 248, 247], PillowCImageToArray(rgbaOut, 8))
    } finally {
        for handle in [rgbaOut, rgbOut, lOut, rgba, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image chops_invert matches Pillow L RGB RGBA modes", PillowCTestImageChopsInvertMatchesPillowModes)

PillowCTestImageChopsInvertIntoReusesTargetStorage(*) {
    source := PillowCCreateImageMode(2, 1, 4)
    target := PillowCCreateImageMode(2, 1, 4)
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4, 5, 6, 7, 8])
        before := PillowCImageData(target).Ptr
        PillowCImageChopsInvertInto(source, target)

        AhkTest.AssertEqual(before, PillowCImageData(target).Ptr)
        AhkTest.AssertEqual([254, 253, 252, 251, 250, 249, 248, 247], PillowCImageToArray(target, 8))
    } finally {
        for handle in [target, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image chops_invert_into reuses target storage", PillowCTestImageChopsInvertIntoReusesTargetStorage)

PillowCTestImageChopsInvertIntoRejectsTargetShapeMismatch(*) {
    source := PillowCCreateImageMode(2, 1, 4)
    wrongMode := PillowCCreateImageMode(2, 1, 3)
    wrongSize := PillowCCreateImageMode(1, 1, 4)
    try {
        status := DllCall(PillowCDllPath() "\pillow_c_image_chops_invert_into", "Ptr", source, "Ptr", wrongMode, "Int")
        AhkTest.AssertEqual(-5, status)
        status := DllCall(PillowCDllPath() "\pillow_c_image_chops_invert_into", "Ptr", source, "Ptr", wrongSize, "Int")
        AhkTest.AssertEqual(-5, status)
    } finally {
        for handle in [wrongSize, wrongMode, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image chops_invert_into rejects target shape mismatch", PillowCTestImageChopsInvertIntoRejectsTargetShapeMismatch)

PillowCTestImageDifferenceMatchesPillowModes(*) {
    l1 := PillowCCreateImageMode(4, 1, 1)
    l2 := PillowCCreateImageMode(4, 1, 1)
    rgb1 := PillowCCreateImageMode(2, 1, 3)
    rgb2 := PillowCCreateImageMode(2, 1, 3)
    rgba1 := PillowCCreateImageMode(2, 1, 4)
    rgba2 := PillowCCreateImageMode(2, 1, 4)
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l1, [0, 40, 200, 255])
        PillowCImageSetBytes(l2, [255, 10, 220, 0])
        PillowCImageSetBytes(rgb1, [1, 50, 200, 255, 0, 80])
        PillowCImageSetBytes(rgb2, [4, 20, 100, 15, 200, 90])
        PillowCImageSetBytes(rgba1, [1, 50, 200, 255, 20, 30, 40, 0])
        PillowCImageSetBytes(rgba2, [4, 20, 100, 15, 200, 90, 50, 255])

        lOut := PillowCImageDifference(l1, l2)
        rgbOut := PillowCImageDifference(rgb1, rgb2)
        rgbaOut := PillowCImageDifference(rgba1, rgba2)

        AhkTest.AssertEqual([4, 1], [PillowCImageInt(lOut, "pillow_c_image_width"), PillowCImageInt(lOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([255, 30, 20, 255], PillowCImageToArray(lOut, 4))
        AhkTest.AssertEqual([3, 30, 100, 240, 200, 10], PillowCImageToArray(rgbOut, 6))
        AhkTest.AssertEqual([3, 30, 100, 240, 180, 60, 10, 255], PillowCImageToArray(rgbaOut, 8))
    } finally {
        for handle in [rgbaOut, rgbOut, lOut, rgba2, rgba1, rgb2, rgb1, l2, l1] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image difference matches Pillow L RGB RGBA modes", PillowCTestImageDifferenceMatchesPillowModes)

PillowCTestImageDifferenceUsesOverlappingOutputSize(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    out := 0
    try {
        PillowCImageSetBytes(left, [0, 40, 200, 255])
        PillowCImageSetBytes(right, [255, 10])
        out := PillowCImageDifference(left, right)

        AhkTest.AssertEqual([2, 1], [PillowCImageInt(out, "pillow_c_image_width"), PillowCImageInt(out, "pillow_c_image_height")])
        AhkTest.AssertEqual([255, 30], PillowCImageToArray(out, 2))
    } finally {
        for handle in [out, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image difference uses overlapping output size like Pillow", PillowCTestImageDifferenceUsesOverlappingOutputSize)

PillowCTestImageDifferenceAllowsEmptyOverlap(*) {
    source := PillowCCreateImageMode(2, 1, 1)
    empty := 0
    out := 0
    try {
        PillowCImageSetBytes(source, [1, 2])
        empty := PillowCImageCrop(source, 1, 0, 1, 1)
        out := PillowCImageDifference(empty, source)

        AhkTest.AssertEqual([0, 1], [PillowCImageInt(out, "pillow_c_image_width"), PillowCImageInt(out, "pillow_c_image_height")])
        AhkTest.AssertEqual(0, PillowCImageSize(out))
        AhkTest.AssertEqual([], PillowCImageToArray(out, 0))
    } finally {
        for handle in [out, empty, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image difference allows empty overlapping output like Pillow", PillowCTestImageDifferenceAllowsEmptyOverlap)

PillowCTestImageDifferenceIntoReusesTargetHandle(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    target := PillowCCreateImageMode(2, 1, 1)
    try {
        PillowCImageSetBytes(left, [0, 40, 200, 255])
        PillowCImageSetBytes(right, [255, 10])
        PillowCImageSetBytes(target, [1, 2])
        before := PillowCImageData(target).Ptr
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_difference_into",
            "Ptr", left,
            "Ptr", right,
            "Ptr", target,
            "Int"
        )
        PillowCAssertStatus(status)
        after := PillowCImageData(target).Ptr

        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([255, 30], PillowCImageToArray(target, 2))
    } finally {
        for handle in [target, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image difference_into reuses overlapping target storage", PillowCTestImageDifferenceIntoReusesTargetHandle)

PillowCTestImageDifferenceRejectsModeMismatch(*) {
    left := PillowCCreateImageMode(1, 1, 1)
    right := PillowCCreateImageMode(1, 1, 3)
    outHandle := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_difference",
            "Ptr", left,
            "Ptr", right,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(right)
        PillowCFreeImage(left)
    }
}

AhkTest.Test("pillow_c image difference rejects mode mismatch", PillowCTestImageDifferenceRejectsModeMismatch)

PillowCTestImageMultiplyMatchesPillowModes(*) {
    l1 := PillowCCreateImageMode(4, 1, 1)
    l2 := PillowCCreateImageMode(4, 1, 1)
    rgb1 := PillowCCreateImageMode(2, 1, 3)
    rgb2 := PillowCCreateImageMode(2, 1, 3)
    rgba1 := PillowCCreateImageMode(2, 1, 4)
    rgba2 := PillowCCreateImageMode(2, 1, 4)
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l1, [0, 40, 200, 255])
        PillowCImageSetBytes(l2, [255, 10, 220, 128])
        PillowCImageSetBytes(rgb1, [1, 50, 200, 255, 0, 80])
        PillowCImageSetBytes(rgb2, [4, 20, 100, 15, 200, 90])
        PillowCImageSetBytes(rgba1, [1, 50, 200, 255, 20, 30, 40, 0])
        PillowCImageSetBytes(rgba2, [4, 20, 100, 15, 200, 90, 50, 255])

        lOut := PillowCImageMultiply(l1, l2)
        rgbOut := PillowCImageMultiply(rgb1, rgb2)
        rgbaOut := PillowCImageMultiply(rgba1, rgba2)

        AhkTest.AssertEqual([0, 1, 172, 128], PillowCImageToArray(lOut, 4))
        AhkTest.AssertEqual([0, 3, 78, 15, 0, 28], PillowCImageToArray(rgbOut, 6))
        AhkTest.AssertEqual([0, 3, 78, 15, 15, 10, 7, 0], PillowCImageToArray(rgbaOut, 8))
    } finally {
        for handle in [rgbaOut, rgbOut, lOut, rgba2, rgba1, rgb2, rgb1, l2, l1] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image multiply matches Pillow L RGB RGBA modes", PillowCTestImageMultiplyMatchesPillowModes)

PillowCTestImageMultiplyUsesOverlappingAndEmptyOutputSize(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    empty := 0
    overlap := 0
    emptyOut := 0
    try {
        PillowCImageSetBytes(left, [0, 40, 200, 255])
        PillowCImageSetBytes(right, [255, 10])
        empty := PillowCImageCrop(left, 1, 0, 1, 1)
        overlap := PillowCImageMultiply(left, right)
        emptyOut := PillowCImageMultiply(empty, left)

        AhkTest.AssertEqual([2, 1], [PillowCImageInt(overlap, "pillow_c_image_width"), PillowCImageInt(overlap, "pillow_c_image_height")])
        AhkTest.AssertEqual([0, 1], PillowCImageToArray(overlap, 2))
        AhkTest.AssertEqual([0, 1], [PillowCImageInt(emptyOut, "pillow_c_image_width"), PillowCImageInt(emptyOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([], PillowCImageToArray(emptyOut, 0))
    } finally {
        for handle in [emptyOut, overlap, empty, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image multiply uses overlapping and empty output size like Pillow", PillowCTestImageMultiplyUsesOverlappingAndEmptyOutputSize)

PillowCTestImageMultiplyIntoReusesTargetHandle(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    target := PillowCCreateImageMode(2, 1, 1)
    try {
        PillowCImageSetBytes(left, [0, 40, 200, 255])
        PillowCImageSetBytes(right, [255, 10])
        before := PillowCImageData(target).Ptr
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_multiply_into",
            "Ptr", left,
            "Ptr", right,
            "Ptr", target,
            "Int"
        )
        PillowCAssertStatus(status)
        after := PillowCImageData(target).Ptr

        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([0, 1], PillowCImageToArray(target, 2))
    } finally {
        for handle in [target, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image multiply_into reuses overlapping target storage", PillowCTestImageMultiplyIntoReusesTargetHandle)

PillowCTestImageMultiplyRejectsModeMismatch(*) {
    left := PillowCCreateImageMode(1, 1, 1)
    right := PillowCCreateImageMode(1, 1, 3)
    outHandle := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_multiply",
            "Ptr", left,
            "Ptr", right,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(right)
        PillowCFreeImage(left)
    }
}

AhkTest.Test("pillow_c image multiply rejects mode mismatch", PillowCTestImageMultiplyRejectsModeMismatch)

PillowCTestImageScreenMatchesPillowModes(*) {
    l1 := PillowCCreateImageMode(4, 1, 1)
    l2 := PillowCCreateImageMode(4, 1, 1)
    rgb1 := PillowCCreateImageMode(2, 1, 3)
    rgb2 := PillowCCreateImageMode(2, 1, 3)
    rgba1 := PillowCCreateImageMode(2, 1, 4)
    rgba2 := PillowCCreateImageMode(2, 1, 4)
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l1, [0, 40, 200, 255])
        PillowCImageSetBytes(l2, [255, 10, 220, 128])
        PillowCImageSetBytes(rgb1, [1, 50, 200, 255, 0, 80])
        PillowCImageSetBytes(rgb2, [4, 20, 100, 15, 200, 90])
        PillowCImageSetBytes(rgba1, [1, 50, 200, 255, 20, 30, 40, 0])
        PillowCImageSetBytes(rgba2, [4, 20, 100, 15, 200, 90, 50, 255])

        lOut := PillowCImageScreen(l1, l2)
        rgbOut := PillowCImageScreen(rgb1, rgb2)
        rgbaOut := PillowCImageScreen(rgba1, rgba2)

        AhkTest.AssertEqual([255, 49, 248, 255], PillowCImageToArray(lOut, 4))
        AhkTest.AssertEqual([5, 67, 222, 255, 200, 142], PillowCImageToArray(rgbOut, 6))
        AhkTest.AssertEqual([5, 67, 222, 255, 205, 110, 83, 255], PillowCImageToArray(rgbaOut, 8))
    } finally {
        for handle in [rgbaOut, rgbOut, lOut, rgba2, rgba1, rgb2, rgb1, l2, l1] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image screen matches Pillow L RGB RGBA modes", PillowCTestImageScreenMatchesPillowModes)

PillowCTestImageScreenUsesOverlappingAndEmptyOutputSize(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    empty := 0
    overlap := 0
    emptyOut := 0
    try {
        PillowCImageSetBytes(left, [0, 40, 200, 255])
        PillowCImageSetBytes(right, [255, 10])
        empty := PillowCImageCrop(left, 1, 0, 1, 1)
        overlap := PillowCImageScreen(left, right)
        emptyOut := PillowCImageScreen(empty, left)

        AhkTest.AssertEqual([2, 1], [PillowCImageInt(overlap, "pillow_c_image_width"), PillowCImageInt(overlap, "pillow_c_image_height")])
        AhkTest.AssertEqual([255, 49], PillowCImageToArray(overlap, 2))
        AhkTest.AssertEqual([0, 1], [PillowCImageInt(emptyOut, "pillow_c_image_width"), PillowCImageInt(emptyOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([], PillowCImageToArray(emptyOut, 0))
    } finally {
        for handle in [emptyOut, overlap, empty, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image screen uses overlapping and empty output size like Pillow", PillowCTestImageScreenUsesOverlappingAndEmptyOutputSize)

PillowCTestImageScreenIntoReusesTargetHandle(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    target := PillowCCreateImageMode(2, 1, 1)
    try {
        PillowCImageSetBytes(left, [0, 40, 200, 255])
        PillowCImageSetBytes(right, [255, 10])
        before := PillowCImageData(target).Ptr
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_screen_into",
            "Ptr", left,
            "Ptr", right,
            "Ptr", target,
            "Int"
        )
        PillowCAssertStatus(status)
        after := PillowCImageData(target).Ptr

        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([255, 49], PillowCImageToArray(target, 2))
    } finally {
        for handle in [target, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image screen_into reuses overlapping target storage", PillowCTestImageScreenIntoReusesTargetHandle)

PillowCTestImageScreenRejectsModeMismatch(*) {
    left := PillowCCreateImageMode(1, 1, 1)
    right := PillowCCreateImageMode(1, 1, 3)
    outHandle := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_screen",
            "Ptr", left,
            "Ptr", right,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(right)
        PillowCFreeImage(left)
    }
}

AhkTest.Test("pillow_c image screen rejects mode mismatch", PillowCTestImageScreenRejectsModeMismatch)

PillowCTestImageLighterAndDarkerMatchPillowModes(*) {
    l1 := PillowCCreateImageMode(4, 1, 1)
    l2 := PillowCCreateImageMode(4, 1, 1)
    rgb1 := PillowCCreateImageMode(2, 1, 3)
    rgb2 := PillowCCreateImageMode(2, 1, 3)
    rgba1 := PillowCCreateImageMode(2, 1, 4)
    rgba2 := PillowCCreateImageMode(2, 1, 4)
    lLight := 0
    rgbLight := 0
    rgbaLight := 0
    lDark := 0
    rgbDark := 0
    rgbaDark := 0
    try {
        PillowCImageSetBytes(l1, [0, 40, 200, 255])
        PillowCImageSetBytes(l2, [255, 10, 220, 128])
        PillowCImageSetBytes(rgb1, [1, 50, 200, 255, 0, 80])
        PillowCImageSetBytes(rgb2, [4, 20, 100, 15, 200, 90])
        PillowCImageSetBytes(rgba1, [1, 50, 200, 255, 20, 30, 40, 0])
        PillowCImageSetBytes(rgba2, [4, 20, 100, 15, 200, 90, 50, 255])

        lLight := PillowCImageLighter(l1, l2)
        rgbLight := PillowCImageLighter(rgb1, rgb2)
        rgbaLight := PillowCImageLighter(rgba1, rgba2)
        lDark := PillowCImageDarker(l1, l2)
        rgbDark := PillowCImageDarker(rgb1, rgb2)
        rgbaDark := PillowCImageDarker(rgba1, rgba2)

        AhkTest.AssertEqual([255, 40, 220, 255], PillowCImageToArray(lLight, 4))
        AhkTest.AssertEqual([4, 50, 200, 255, 200, 90], PillowCImageToArray(rgbLight, 6))
        AhkTest.AssertEqual([4, 50, 200, 255, 200, 90, 50, 255], PillowCImageToArray(rgbaLight, 8))
        AhkTest.AssertEqual([0, 10, 200, 128], PillowCImageToArray(lDark, 4))
        AhkTest.AssertEqual([1, 20, 100, 15, 0, 80], PillowCImageToArray(rgbDark, 6))
        AhkTest.AssertEqual([1, 20, 100, 15, 20, 30, 40, 0], PillowCImageToArray(rgbaDark, 8))
    } finally {
        for handle in [rgbaDark, rgbDark, lDark, rgbaLight, rgbLight, lLight, rgba2, rgba1, rgb2, rgb1, l2, l1] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image lighter and darker match Pillow modes", PillowCTestImageLighterAndDarkerMatchPillowModes)

PillowCTestImageLighterAndDarkerUseOverlappingAndEmptyOutputSize(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    empty := 0
    lightOverlap := 0
    darkOverlap := 0
    lightEmpty := 0
    darkEmpty := 0
    try {
        PillowCImageSetBytes(left, [0, 40, 200, 255])
        PillowCImageSetBytes(right, [255, 10])
        empty := PillowCImageCrop(left, 1, 0, 1, 1)
        lightOverlap := PillowCImageLighter(left, right)
        darkOverlap := PillowCImageDarker(left, right)
        lightEmpty := PillowCImageLighter(empty, left)
        darkEmpty := PillowCImageDarker(empty, left)

        AhkTest.AssertEqual([2, 1], [PillowCImageInt(lightOverlap, "pillow_c_image_width"), PillowCImageInt(lightOverlap, "pillow_c_image_height")])
        AhkTest.AssertEqual([255, 40], PillowCImageToArray(lightOverlap, 2))
        AhkTest.AssertEqual([0, 10], PillowCImageToArray(darkOverlap, 2))
        AhkTest.AssertEqual([0, 1], [PillowCImageInt(lightEmpty, "pillow_c_image_width"), PillowCImageInt(lightEmpty, "pillow_c_image_height")])
        AhkTest.AssertEqual([], PillowCImageToArray(lightEmpty, 0))
        AhkTest.AssertEqual([], PillowCImageToArray(darkEmpty, 0))
    } finally {
        for handle in [darkEmpty, lightEmpty, darkOverlap, lightOverlap, empty, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image lighter and darker use overlapping and empty output size like Pillow", PillowCTestImageLighterAndDarkerUseOverlappingAndEmptyOutputSize)

PillowCTestImageLighterAndDarkerIntoReuseTargetHandle(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    lightTarget := PillowCCreateImageMode(2, 1, 1)
    darkTarget := PillowCCreateImageMode(2, 1, 1)
    try {
        PillowCImageSetBytes(left, [0, 40, 200, 255])
        PillowCImageSetBytes(right, [255, 10])
        lightBefore := PillowCImageData(lightTarget).Ptr
        darkBefore := PillowCImageData(darkTarget).Ptr

        lightStatus := DllCall(
            PillowCDllPath() "\pillow_c_image_lighter_into",
            "Ptr", left,
            "Ptr", right,
            "Ptr", lightTarget,
            "Int"
        )
        darkStatus := DllCall(
            PillowCDllPath() "\pillow_c_image_darker_into",
            "Ptr", left,
            "Ptr", right,
            "Ptr", darkTarget,
            "Int"
        )
        PillowCAssertStatus(lightStatus)
        PillowCAssertStatus(darkStatus)

        AhkTest.AssertEqual(lightBefore, PillowCImageData(lightTarget).Ptr)
        AhkTest.AssertEqual(darkBefore, PillowCImageData(darkTarget).Ptr)
        AhkTest.AssertEqual([255, 40], PillowCImageToArray(lightTarget, 2))
        AhkTest.AssertEqual([0, 10], PillowCImageToArray(darkTarget, 2))
    } finally {
        for handle in [darkTarget, lightTarget, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image lighter and darker _into reuse overlapping target storage", PillowCTestImageLighterAndDarkerIntoReuseTargetHandle)

PillowCTestImageLighterAndDarkerRejectModeMismatch(*) {
    left := PillowCCreateImageMode(1, 1, 1)
    right := PillowCCreateImageMode(1, 1, 3)
    outHandle := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_lighter",
            "Ptr", left,
            "Ptr", right,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_darker",
            "Ptr", left,
            "Ptr", right,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(right)
        PillowCFreeImage(left)
    }
}

AhkTest.Test("pillow_c image lighter and darker reject mode mismatch", PillowCTestImageLighterAndDarkerRejectModeMismatch)

PillowCTestImageSoftHardOverlayMatchPillowModes(*) {
    l1 := PillowCCreateImageMode(4, 1, 1)
    l2 := PillowCCreateImageMode(4, 1, 1)
    rgb1 := PillowCCreateImageMode(2, 1, 3)
    rgb2 := PillowCCreateImageMode(2, 1, 3)
    rgba1 := PillowCCreateImageMode(2, 1, 4)
    rgba2 := PillowCCreateImageMode(2, 1, 4)
    lSoft := 0
    rgbSoft := 0
    rgbaSoft := 0
    lHard := 0
    rgbHard := 0
    rgbaHard := 0
    lOverlay := 0
    rgbOverlay := 0
    rgbaOverlay := 0
    try {
        PillowCImageSetBytes(l1, [0, 40, 128, 255])
        PillowCImageSetBytes(l2, [255, 10, 128, 64])
        PillowCImageSetBytes(rgb1, [1, 50, 200, 255, 0, 80])
        PillowCImageSetBytes(rgb2, [4, 20, 100, 15, 200, 90])
        PillowCImageSetBytes(rgba1, [1, 50, 200, 255, 20, 30, 40, 0])
        PillowCImageSetBytes(rgba2, [4, 20, 100, 15, 200, 90, 50, 255])

        lSoft := PillowCImageSoftLight(l1, l2)
        rgbSoft := PillowCImageSoftLight(rgb1, rgb2)
        rgbaSoft := PillowCImageSoftLight(rgba1, rgba2)
        lHard := PillowCImageHardLight(l1, l2)
        rgbHard := PillowCImageHardLight(rgb1, rgb2)
        rgbaHard := PillowCImageHardLight(rgba1, rgba2)
        lOverlay := PillowCImageOverlay(l1, l2)
        rgbOverlay := PillowCImageOverlay(rgb1, rgb2)
        rgbaOverlay := PillowCImageOverlay(rgba1, rgba2)

        AhkTest.AssertEqual([0, 8, 127, 255], PillowCImageToArray(lSoft, 4))
        AhkTest.AssertEqual([0, 16, 190, 255, 0, 63], PillowCImageToArray(rgbSoft, 6))
        AhkTest.AssertEqual([0, 16, 190, 255, 30, 21, 19, 0], PillowCImageToArray(rgbaSoft, 8))
        AhkTest.AssertEqual([255, 3, 128, 128], PillowCImageToArray(lHard, 4))
        AhkTest.AssertEqual([0, 7, 157, 30, 145, 56], PillowCImageToArray(rgbHard, 6))
        AhkTest.AssertEqual([0, 7, 157, 30, 154, 21, 15, 255], PillowCImageToArray(rgbaHard, 8))
        AhkTest.AssertEqual([0, 3, 128, 255], PillowCImageToArray(lOverlay, 4))
        AhkTest.AssertEqual([0, 7, 188, 255, 0, 56], PillowCImageToArray(rgbOverlay, 6))
        AhkTest.AssertEqual([0, 7, 188, 255, 31, 21, 15, 0], PillowCImageToArray(rgbaOverlay, 8))
    } finally {
        for handle in [rgbaOverlay, rgbOverlay, lOverlay, rgbaHard, rgbHard, lHard, rgbaSoft, rgbSoft, lSoft, rgba2, rgba1, rgb2, rgb1, l2, l1] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image soft hard overlay match Pillow modes", PillowCTestImageSoftHardOverlayMatchPillowModes)

PillowCTestImageSoftHardOverlayUseOverlappingAndEmptyOutputSize(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    empty := 0
    softOverlap := 0
    hardOverlap := 0
    overlayOverlap := 0
    softEmpty := 0
    hardEmpty := 0
    overlayEmpty := 0
    try {
        PillowCImageSetBytes(left, [0, 40, 128, 255])
        PillowCImageSetBytes(right, [255, 10])
        empty := PillowCImageCrop(left, 1, 0, 1, 1)
        softOverlap := PillowCImageSoftLight(left, right)
        hardOverlap := PillowCImageHardLight(left, right)
        overlayOverlap := PillowCImageOverlay(left, right)
        softEmpty := PillowCImageSoftLight(empty, left)
        hardEmpty := PillowCImageHardLight(empty, left)
        overlayEmpty := PillowCImageOverlay(empty, left)

        AhkTest.AssertEqual([2, 1], [PillowCImageInt(softOverlap, "pillow_c_image_width"), PillowCImageInt(softOverlap, "pillow_c_image_height")])
        AhkTest.AssertEqual([0, 8], PillowCImageToArray(softOverlap, 2))
        AhkTest.AssertEqual([255, 3], PillowCImageToArray(hardOverlap, 2))
        AhkTest.AssertEqual([0, 3], PillowCImageToArray(overlayOverlap, 2))
        AhkTest.AssertEqual([0, 1], [PillowCImageInt(softEmpty, "pillow_c_image_width"), PillowCImageInt(softEmpty, "pillow_c_image_height")])
        AhkTest.AssertEqual([], PillowCImageToArray(softEmpty, 0))
        AhkTest.AssertEqual([], PillowCImageToArray(hardEmpty, 0))
        AhkTest.AssertEqual([], PillowCImageToArray(overlayEmpty, 0))
    } finally {
        for handle in [overlayEmpty, hardEmpty, softEmpty, overlayOverlap, hardOverlap, softOverlap, empty, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image soft hard overlay use overlapping and empty output size like Pillow", PillowCTestImageSoftHardOverlayUseOverlappingAndEmptyOutputSize)

PillowCTestImageSoftHardOverlayIntoReuseTargetHandle(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    softTarget := PillowCCreateImageMode(2, 1, 1)
    hardTarget := PillowCCreateImageMode(2, 1, 1)
    overlayTarget := PillowCCreateImageMode(2, 1, 1)
    try {
        PillowCImageSetBytes(left, [0, 40, 128, 255])
        PillowCImageSetBytes(right, [255, 10])
        softBefore := PillowCImageData(softTarget).Ptr
        hardBefore := PillowCImageData(hardTarget).Ptr
        overlayBefore := PillowCImageData(overlayTarget).Ptr

        softStatus := DllCall(PillowCDllPath() "\pillow_c_image_soft_light_into", "Ptr", left, "Ptr", right, "Ptr", softTarget, "Int")
        hardStatus := DllCall(PillowCDllPath() "\pillow_c_image_hard_light_into", "Ptr", left, "Ptr", right, "Ptr", hardTarget, "Int")
        overlayStatus := DllCall(PillowCDllPath() "\pillow_c_image_overlay_into", "Ptr", left, "Ptr", right, "Ptr", overlayTarget, "Int")
        PillowCAssertStatus(softStatus)
        PillowCAssertStatus(hardStatus)
        PillowCAssertStatus(overlayStatus)

        AhkTest.AssertEqual(softBefore, PillowCImageData(softTarget).Ptr)
        AhkTest.AssertEqual(hardBefore, PillowCImageData(hardTarget).Ptr)
        AhkTest.AssertEqual(overlayBefore, PillowCImageData(overlayTarget).Ptr)
        AhkTest.AssertEqual([0, 8], PillowCImageToArray(softTarget, 2))
        AhkTest.AssertEqual([255, 3], PillowCImageToArray(hardTarget, 2))
        AhkTest.AssertEqual([0, 3], PillowCImageToArray(overlayTarget, 2))
    } finally {
        for handle in [overlayTarget, hardTarget, softTarget, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image soft hard overlay _into reuse overlapping target storage", PillowCTestImageSoftHardOverlayIntoReuseTargetHandle)

PillowCTestImageSoftHardOverlayRejectModeMismatch(*) {
    left := PillowCCreateImageMode(1, 1, 1)
    right := PillowCCreateImageMode(1, 1, 3)
    outHandle := 0
    try {
        status := DllCall(PillowCDllPath() "\pillow_c_image_soft_light", "Ptr", left, "Ptr", right, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(PillowCDllPath() "\pillow_c_image_hard_light", "Ptr", left, "Ptr", right, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(PillowCDllPath() "\pillow_c_image_overlay", "Ptr", left, "Ptr", right, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(right)
        PillowCFreeImage(left)
    }
}

AhkTest.Test("pillow_c image soft hard overlay reject mode mismatch", PillowCTestImageSoftHardOverlayRejectModeMismatch)

PillowCTestImageOffsetWrapsPixelsLikePillowModes(*) {
    l := PillowCCreateImageMode(3, 2, 1)
    rgb := PillowCCreateImageMode(2, 2, 3)
    rgba := PillowCCreateImageMode(2, 2, 4)
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(rgb, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12])
        PillowCImageSetBytes(rgba, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16])

        lOut := PillowCImageOffset(l, 1, 1)
        rgbOut := PillowCImageOffset(rgb, 1, -1)
        rgbaOut := PillowCImageOffset(rgba, -4, 3)

        AhkTest.AssertEqual([6, 4, 5, 3, 1, 2], PillowCImageToArray(lOut, 6))
        AhkTest.AssertEqual([10, 11, 12, 7, 8, 9, 4, 5, 6, 1, 2, 3], PillowCImageToArray(rgbOut, 12))
        AhkTest.AssertEqual([9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8], PillowCImageToArray(rgbaOut, 16))
    } finally {
        for handle in [rgbaOut, rgbOut, lOut, rgba, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image offset wraps pixels like Pillow modes", PillowCTestImageOffsetWrapsPixelsLikePillowModes)

PillowCTestImageOffsetIntoReusesTargetHandle(*) {
    source := PillowCCreateImageMode(3, 2, 1)
    target := PillowCCreateImageMode(3, 2, 1)
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4, 5, 6])
        before := PillowCImageData(target).Ptr
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_offset_into",
            "Ptr", source,
            "Int", -4,
            "Int", 3,
            "Ptr", target,
            "Int"
        )
        PillowCAssertStatus(status)

        AhkTest.AssertEqual(before, PillowCImageData(target).Ptr)
        AhkTest.AssertEqual([5, 6, 4, 2, 3, 1], PillowCImageToArray(target, 6))
    } finally {
        for handle in [target, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image offset_into reuses target storage", PillowCTestImageOffsetIntoReusesTargetHandle)

PillowCTestImageOffsetHandlesEmptyImagesSafely(*) {
    source := PillowCCreateImageMode(3, 2, 1)
    emptyWidth := 0
    emptyHeight := 0
    outWidth := 0
    outHeight := 0
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4, 5, 6])
        emptyWidth := PillowCImageCrop(source, 1, 0, 1, 2)
        emptyHeight := PillowCImageCrop(source, 0, 1, 3, 1)
        outWidth := PillowCImageOffset(emptyWidth, 1, -1)
        outHeight := PillowCImageOffset(emptyHeight, 1, -1)

        AhkTest.AssertEqual([0, 2], [PillowCImageInt(outWidth, "pillow_c_image_width"), PillowCImageInt(outWidth, "pillow_c_image_height")])
        AhkTest.AssertEqual([3, 0], [PillowCImageInt(outHeight, "pillow_c_image_width"), PillowCImageInt(outHeight, "pillow_c_image_height")])
        AhkTest.AssertEqual([], PillowCImageToArray(outWidth, 0))
        AhkTest.AssertEqual([], PillowCImageToArray(outHeight, 0))
    } finally {
        for handle in [outHeight, outWidth, emptyHeight, emptyWidth, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image offset handles empty images safely", PillowCTestImageOffsetHandlesEmptyImagesSafely)

PillowCTestImageOffsetIntoRejectsTargetShapeMismatch(*) {
    source := PillowCCreateImageMode(3, 2, 1)
    target := PillowCCreateImageMode(2, 2, 1)
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_offset_into",
            "Ptr", source,
            "Int", 1,
            "Int", 1,
            "Ptr", target,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image offset_into rejects target shape mismatch", PillowCTestImageOffsetIntoRejectsTargetShapeMismatch)

PillowCTestImageAddAndSubtractClampLikePillowModes(*) {
    l1 := PillowCCreateImageMode(4, 1, 1)
    l2 := PillowCCreateImageMode(4, 1, 1)
    rgb1 := PillowCCreateImageMode(2, 1, 3)
    rgb2 := PillowCCreateImageMode(2, 1, 3)
    rgba1 := PillowCCreateImageMode(2, 1, 4)
    rgba2 := PillowCCreateImageMode(2, 1, 4)
    lAdd := 0
    rgbAdd := 0
    rgbaAdd := 0
    lSub := 0
    rgbSub := 0
    rgbaSub := 0
    try {
        PillowCImageSetBytes(l1, [0, 40, 200, 255])
        PillowCImageSetBytes(l2, [255, 10, 220, 128])
        PillowCImageSetBytes(rgb1, [1, 50, 200, 255, 0, 80])
        PillowCImageSetBytes(rgb2, [4, 20, 100, 15, 200, 90])
        PillowCImageSetBytes(rgba1, [1, 50, 200, 255, 20, 30, 40, 0])
        PillowCImageSetBytes(rgba2, [4, 20, 100, 15, 200, 90, 50, 255])

        lAdd := PillowCImageAdd(l1, l2)
        rgbAdd := PillowCImageAdd(rgb1, rgb2)
        rgbaAdd := PillowCImageAdd(rgba1, rgba2)
        lSub := PillowCImageSubtract(l1, l2)
        rgbSub := PillowCImageSubtract(rgb1, rgb2)
        rgbaSub := PillowCImageSubtract(rgba1, rgba2)

        AhkTest.AssertEqual([255, 50, 255, 255], PillowCImageToArray(lAdd, 4))
        AhkTest.AssertEqual([5, 70, 255, 255, 200, 170], PillowCImageToArray(rgbAdd, 6))
        AhkTest.AssertEqual([5, 70, 255, 255, 220, 120, 90, 255], PillowCImageToArray(rgbaAdd, 8))
        AhkTest.AssertEqual([0, 30, 0, 127], PillowCImageToArray(lSub, 4))
        AhkTest.AssertEqual([0, 30, 100, 240, 0, 0], PillowCImageToArray(rgbSub, 6))
        AhkTest.AssertEqual([0, 30, 100, 240, 0, 0, 0, 0], PillowCImageToArray(rgbaSub, 8))
    } finally {
        for handle in [rgbaSub, rgbSub, lSub, rgbaAdd, rgbAdd, lAdd, rgba2, rgba1, rgb2, rgb1, l2, l1] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image add and subtract clamp like Pillow modes", PillowCTestImageAddAndSubtractClampLikePillowModes)

PillowCTestImageAddAndSubtractApplyScaleAndOffset(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(4, 1, 1)
    addOut := 0
    subOut := 0
    addZeroScale := 0
    addNegativeOffset := 0
    subPositiveOffset := 0
    try {
        PillowCImageSetBytes(left, [0, 40, 200, 255])
        PillowCImageSetBytes(right, [255, 10, 220, 128])

        addOut := PillowCImageAdd(left, right, 2.0, 10.0)
        subOut := PillowCImageSubtract(left, right, 2.0, 10.0)
        addZeroScale := PillowCImageAdd(left, left, 0.0, 0.0)
        addNegativeOffset := PillowCImageAdd(left, left, 1.0, -100.0)
        subPositiveOffset := PillowCImageSubtract(left, left, 1.0, 300.0)

        AhkTest.AssertEqual([137, 35, 220, 201], PillowCImageToArray(addOut, 4))
        AhkTest.AssertEqual([0, 25, 0, 73], PillowCImageToArray(subOut, 4))
        AhkTest.AssertEqual([0, 0, 0, 0], PillowCImageToArray(addZeroScale, 4))
        AhkTest.AssertEqual([0, 0, 255, 255], PillowCImageToArray(addNegativeOffset, 4))
        AhkTest.AssertEqual([255, 255, 255, 255], PillowCImageToArray(subPositiveOffset, 4))
    } finally {
        for handle in [subPositiveOffset, addNegativeOffset, addZeroScale, subOut, addOut, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image add and subtract apply scale and offset", PillowCTestImageAddAndSubtractApplyScaleAndOffset)

PillowCTestImageAddAndSubtractUseOverlappingAndEmptyOutputSize(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    empty := 0
    addOverlap := 0
    subOverlap := 0
    addEmpty := 0
    subEmpty := 0
    try {
        PillowCImageSetBytes(left, [0, 40, 200, 255])
        PillowCImageSetBytes(right, [255, 10])
        empty := PillowCImageCrop(left, 1, 0, 1, 1)
        addOverlap := PillowCImageAdd(left, right)
        subOverlap := PillowCImageSubtract(left, right)
        addEmpty := PillowCImageAdd(empty, left)
        subEmpty := PillowCImageSubtract(empty, left)

        AhkTest.AssertEqual([2, 1], [PillowCImageInt(addOverlap, "pillow_c_image_width"), PillowCImageInt(addOverlap, "pillow_c_image_height")])
        AhkTest.AssertEqual([255, 50], PillowCImageToArray(addOverlap, 2))
        AhkTest.AssertEqual([0, 30], PillowCImageToArray(subOverlap, 2))
        AhkTest.AssertEqual([0, 1], [PillowCImageInt(addEmpty, "pillow_c_image_width"), PillowCImageInt(addEmpty, "pillow_c_image_height")])
        AhkTest.AssertEqual([], PillowCImageToArray(addEmpty, 0))
        AhkTest.AssertEqual([], PillowCImageToArray(subEmpty, 0))
    } finally {
        for handle in [subEmpty, addEmpty, subOverlap, addOverlap, empty, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image add and subtract use overlapping and empty output size like Pillow", PillowCTestImageAddAndSubtractUseOverlappingAndEmptyOutputSize)

PillowCTestImageAddAndSubtractIntoReuseTargetHandle(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    addTarget := PillowCCreateImageMode(2, 1, 1)
    subTarget := PillowCCreateImageMode(2, 1, 1)
    try {
        PillowCImageSetBytes(left, [0, 40, 200, 255])
        PillowCImageSetBytes(right, [255, 10])
        addBefore := PillowCImageData(addTarget).Ptr
        subBefore := PillowCImageData(subTarget).Ptr

        addStatus := DllCall(
            PillowCDllPath() "\pillow_c_image_add_into",
            "Ptr", left,
            "Ptr", right,
            "Double", 1.0,
            "Double", 0.0,
            "Ptr", addTarget,
            "Int"
        )
        subStatus := DllCall(
            PillowCDllPath() "\pillow_c_image_subtract_into",
            "Ptr", left,
            "Ptr", right,
            "Double", 1.0,
            "Double", 0.0,
            "Ptr", subTarget,
            "Int"
        )
        PillowCAssertStatus(addStatus)
        PillowCAssertStatus(subStatus)

        AhkTest.AssertEqual(addBefore, PillowCImageData(addTarget).Ptr)
        AhkTest.AssertEqual(subBefore, PillowCImageData(subTarget).Ptr)
        AhkTest.AssertEqual([255, 50], PillowCImageToArray(addTarget, 2))
        AhkTest.AssertEqual([0, 30], PillowCImageToArray(subTarget, 2))
    } finally {
        for handle in [subTarget, addTarget, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image add and subtract _into reuse overlapping target storage", PillowCTestImageAddAndSubtractIntoReuseTargetHandle)

PillowCTestImageAddAndSubtractRejectModeMismatch(*) {
    left := PillowCCreateImageMode(1, 1, 1)
    right := PillowCCreateImageMode(1, 1, 3)
    outHandle := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_add",
            "Ptr", left,
            "Ptr", right,
            "Double", 1.0,
            "Double", 0.0,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_subtract",
            "Ptr", left,
            "Ptr", right,
            "Double", 1.0,
            "Double", 0.0,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(right)
        PillowCFreeImage(left)
    }
}

AhkTest.Test("pillow_c image add and subtract reject mode mismatch", PillowCTestImageAddAndSubtractRejectModeMismatch)

PillowCTestImageAddAndSubtractModuloMatchPillowModes(*) {
    l1 := PillowCCreateImageMode(4, 1, 1)
    l2 := PillowCCreateImageMode(4, 1, 1)
    rgb1 := PillowCCreateImageMode(2, 1, 3)
    rgb2 := PillowCCreateImageMode(2, 1, 3)
    rgba1 := PillowCCreateImageMode(2, 1, 4)
    rgba2 := PillowCCreateImageMode(2, 1, 4)
    lAdd := 0
    rgbAdd := 0
    rgbaAdd := 0
    lSub := 0
    rgbSub := 0
    rgbaSub := 0
    try {
        PillowCImageSetBytes(l1, [0, 40, 200, 255])
        PillowCImageSetBytes(l2, [255, 10, 220, 128])
        PillowCImageSetBytes(rgb1, [1, 50, 200, 255, 0, 80])
        PillowCImageSetBytes(rgb2, [4, 20, 100, 15, 200, 90])
        PillowCImageSetBytes(rgba1, [1, 50, 200, 255, 20, 30, 40, 0])
        PillowCImageSetBytes(rgba2, [4, 20, 100, 15, 200, 90, 50, 255])

        lAdd := PillowCImageAddModulo(l1, l2)
        rgbAdd := PillowCImageAddModulo(rgb1, rgb2)
        rgbaAdd := PillowCImageAddModulo(rgba1, rgba2)
        lSub := PillowCImageSubtractModulo(l1, l2)
        rgbSub := PillowCImageSubtractModulo(rgb1, rgb2)
        rgbaSub := PillowCImageSubtractModulo(rgba1, rgba2)

        AhkTest.AssertEqual([255, 50, 164, 127], PillowCImageToArray(lAdd, 4))
        AhkTest.AssertEqual([5, 70, 44, 14, 200, 170], PillowCImageToArray(rgbAdd, 6))
        AhkTest.AssertEqual([5, 70, 44, 14, 220, 120, 90, 255], PillowCImageToArray(rgbaAdd, 8))
        AhkTest.AssertEqual([1, 30, 236, 127], PillowCImageToArray(lSub, 4))
        AhkTest.AssertEqual([253, 30, 100, 240, 56, 246], PillowCImageToArray(rgbSub, 6))
        AhkTest.AssertEqual([253, 30, 100, 240, 76, 196, 246, 1], PillowCImageToArray(rgbaSub, 8))
    } finally {
        for handle in [rgbaSub, rgbSub, lSub, rgbaAdd, rgbAdd, lAdd, rgba2, rgba1, rgb2, rgb1, l2, l1] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image add_modulo and subtract_modulo match Pillow modes", PillowCTestImageAddAndSubtractModuloMatchPillowModes)

PillowCTestImageModuloOpsUseOverlappingAndEmptyOutputSize(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    empty := 0
    addOverlap := 0
    subOverlap := 0
    addEmpty := 0
    subEmpty := 0
    try {
        PillowCImageSetBytes(left, [0, 40, 200, 255])
        PillowCImageSetBytes(right, [255, 10])
        empty := PillowCImageCrop(left, 1, 0, 1, 1)
        addOverlap := PillowCImageAddModulo(left, right)
        subOverlap := PillowCImageSubtractModulo(left, right)
        addEmpty := PillowCImageAddModulo(empty, left)
        subEmpty := PillowCImageSubtractModulo(empty, left)

        AhkTest.AssertEqual([2, 1], [PillowCImageInt(addOverlap, "pillow_c_image_width"), PillowCImageInt(addOverlap, "pillow_c_image_height")])
        AhkTest.AssertEqual([255, 50], PillowCImageToArray(addOverlap, 2))
        AhkTest.AssertEqual([1, 30], PillowCImageToArray(subOverlap, 2))
        AhkTest.AssertEqual([0, 1], [PillowCImageInt(addEmpty, "pillow_c_image_width"), PillowCImageInt(addEmpty, "pillow_c_image_height")])
        AhkTest.AssertEqual([], PillowCImageToArray(addEmpty, 0))
        AhkTest.AssertEqual([], PillowCImageToArray(subEmpty, 0))
    } finally {
        for handle in [subEmpty, addEmpty, subOverlap, addOverlap, empty, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image modulo ops use overlapping and empty output size like Pillow", PillowCTestImageModuloOpsUseOverlappingAndEmptyOutputSize)

PillowCTestImageModuloOpsIntoReuseTargetHandle(*) {
    left := PillowCCreateImageMode(4, 1, 1)
    right := PillowCCreateImageMode(2, 1, 1)
    addTarget := PillowCCreateImageMode(2, 1, 1)
    subTarget := PillowCCreateImageMode(2, 1, 1)
    try {
        PillowCImageSetBytes(left, [0, 40, 200, 255])
        PillowCImageSetBytes(right, [255, 10])
        addBefore := PillowCImageData(addTarget).Ptr
        subBefore := PillowCImageData(subTarget).Ptr

        addStatus := DllCall(
            PillowCDllPath() "\pillow_c_image_add_modulo_into",
            "Ptr", left,
            "Ptr", right,
            "Ptr", addTarget,
            "Int"
        )
        subStatus := DllCall(
            PillowCDllPath() "\pillow_c_image_subtract_modulo_into",
            "Ptr", left,
            "Ptr", right,
            "Ptr", subTarget,
            "Int"
        )
        PillowCAssertStatus(addStatus)
        PillowCAssertStatus(subStatus)

        AhkTest.AssertEqual(addBefore, PillowCImageData(addTarget).Ptr)
        AhkTest.AssertEqual(subBefore, PillowCImageData(subTarget).Ptr)
        AhkTest.AssertEqual([255, 50], PillowCImageToArray(addTarget, 2))
        AhkTest.AssertEqual([1, 30], PillowCImageToArray(subTarget, 2))
    } finally {
        for handle in [subTarget, addTarget, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image modulo ops _into reuse overlapping target storage", PillowCTestImageModuloOpsIntoReuseTargetHandle)

PillowCTestImageModuloOpsRejectModeMismatch(*) {
    left := PillowCCreateImageMode(1, 1, 1)
    right := PillowCCreateImageMode(1, 1, 3)
    outHandle := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_add_modulo",
            "Ptr", left,
            "Ptr", right,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_subtract_modulo",
            "Ptr", left,
            "Ptr", right,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(right)
        PillowCFreeImage(left)
    }
}

AhkTest.Test("pillow_c image modulo ops reject mode mismatch", PillowCTestImageModuloOpsRejectModeMismatch)

PillowCTestImageChopsBinaryOpsMatchPillowLaAndCmyk(*) {
    la1 := PillowCCreateImageMode(2, 1, 2)
    la2 := PillowCCreateImageMode(2, 1, 2)
    cmyk1 := PillowCCreateImageMode(2, 1, 7)
    cmyk2 := PillowCCreateImageMode(2, 1, 7)
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
        PillowCImageSetBytes(la1, [10, 20, 200, 220])
        PillowCImageSetBytes(la2, [30, 40, 150, 160])
        PillowCImageSetBytes(cmyk1, [10, 20, 30, 40, 200, 210, 220, 230])
        PillowCImageSetBytes(cmyk2, [30, 40, 50, 60, 150, 160, 170, 180])

        laDiff := PillowCImageDifference(la1, la2)
        laMultiply := PillowCImageMultiply(la1, la2)
        laScreen := PillowCImageScreen(la1, la2)
        laLight := PillowCImageLighter(la1, la2)
        laDark := PillowCImageDarker(la1, la2)
        laAdd := PillowCImageAdd(la1, la2)
        laSub := PillowCImageSubtract(la1, la2)
        laAddModulo := PillowCImageAddModulo(la1, la2)
        laSubModulo := PillowCImageSubtractModulo(la1, la2)
        laSoft := PillowCImageSoftLight(la1, la2)
        laHard := PillowCImageHardLight(la1, la2)
        laOverlay := PillowCImageOverlay(la1, la2)
        cmykDiff := PillowCImageDifference(cmyk1, cmyk2)
        cmykMultiply := PillowCImageMultiply(cmyk1, cmyk2)
        cmykScreen := PillowCImageScreen(cmyk1, cmyk2)
        cmykLight := PillowCImageLighter(cmyk1, cmyk2)
        cmykDark := PillowCImageDarker(cmyk1, cmyk2)
        cmykAdd := PillowCImageAdd(cmyk1, cmyk2)
        cmykSub := PillowCImageSubtract(cmyk1, cmyk2)
        cmykAddModulo := PillowCImageAddModulo(cmyk1, cmyk2)
        cmykSubModulo := PillowCImageSubtractModulo(cmyk1, cmyk2)
        cmykSoft := PillowCImageSoftLight(cmyk1, cmyk2)
        cmykHard := PillowCImageHardLight(cmyk1, cmyk2)
        cmykOverlay := PillowCImageOverlay(cmyk1, cmyk2)

        AhkTest.AssertEqual(2, PillowCImageMode(laDiff))
        AhkTest.AssertEqual([20, 20, 50, 60], PillowCImageToArray(laDiff, 4))
        AhkTest.AssertEqual([1, 3, 117, 138], PillowCImageToArray(laMultiply, 4))
        AhkTest.AssertEqual([39, 57, 233, 242], PillowCImageToArray(laScreen, 4))
        AhkTest.AssertEqual([30, 40, 200, 220], PillowCImageToArray(laLight, 4))
        AhkTest.AssertEqual([10, 20, 150, 160], PillowCImageToArray(laDark, 4))
        AhkTest.AssertEqual([40, 60, 255, 255], PillowCImageToArray(laAdd, 4))
        AhkTest.AssertEqual([0, 0, 50, 60], PillowCImageToArray(laSub, 4))
        AhkTest.AssertEqual([40, 60, 94, 124], PillowCImageToArray(laAddModulo, 4))
        AhkTest.AssertEqual([236, 236, 50, 60], PillowCImageToArray(laSubModulo, 4))
        AhkTest.AssertEqual([2, 6, 207, 226], PillowCImageToArray(laSoft, 4))
        AhkTest.AssertEqual([2, 6, 210, 229], PillowCImageToArray(laHard, 4))
        AhkTest.AssertEqual([2, 6, 210, 229], PillowCImageToArray(laOverlay, 4))

        AhkTest.AssertEqual(7, PillowCImageMode(cmykDiff))
        AhkTest.AssertEqual([20, 20, 20, 20, 50, 50, 50, 50], PillowCImageToArray(cmykDiff, 8))
        AhkTest.AssertEqual([1, 3, 5, 9, 117, 131, 146, 162], PillowCImageToArray(cmykMultiply, 8))
        AhkTest.AssertEqual([39, 57, 75, 91, 233, 239, 244, 248], PillowCImageToArray(cmykScreen, 8))
        AhkTest.AssertEqual([30, 40, 50, 60, 200, 210, 220, 230], PillowCImageToArray(cmykLight, 8))
        AhkTest.AssertEqual([10, 20, 30, 40, 150, 160, 170, 180], PillowCImageToArray(cmykDark, 8))
        AhkTest.AssertEqual([40, 60, 80, 100, 255, 255, 255, 255], PillowCImageToArray(cmykAdd, 8))
        AhkTest.AssertEqual([0, 0, 0, 0, 50, 50, 50, 50], PillowCImageToArray(cmykSub, 8))
        AhkTest.AssertEqual([40, 60, 80, 100, 94, 114, 134, 154], PillowCImageToArray(cmykAddModulo, 8))
        AhkTest.AssertEqual([236, 236, 236, 236, 50, 50, 50, 50], PillowCImageToArray(cmykSubModulo, 8))
        AhkTest.AssertEqual([2, 6, 13, 21, 207, 219, 229, 238], PillowCImageToArray(cmykSoft, 8))
        AhkTest.AssertEqual([2, 6, 11, 18, 210, 222, 232, 241], PillowCImageToArray(cmykHard, 8))
        AhkTest.AssertEqual([2, 6, 11, 18, 210, 222, 232, 241], PillowCImageToArray(cmykOverlay, 8))
    } finally {
        for handle in [
            cmykOverlay, cmykHard, cmykSoft, cmykSubModulo, cmykAddModulo, cmykSub,
            cmykAdd, cmykDark, cmykLight, cmykScreen, cmykMultiply, cmykDiff,
            laOverlay, laHard, laSoft, laSubModulo, laAddModulo, laSub, laAdd,
            laDark, laLight, laScreen, laMultiply, laDiff, cmyk2, cmyk1, la2, la1
        ] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c ImageChops binary ops match Pillow LA and CMYK", PillowCTestImageChopsBinaryOpsMatchPillowLaAndCmyk)

PillowCTestImageLogicalOpsMatchPillowModeOne(*) {
    left := PillowCCreateImageMode(4, 1, 5)
    right := PillowCCreateImageMode(4, 1, 5)
    andOut := 0
    orOut := 0
    xorOut := 0
    try {
        PillowCImageSetRawBytes(left, [0x60], "1")
        PillowCImageSetRawBytes(right, [0x30], "1")

        andOut := PillowCImageLogicalAnd(left, right)
        orOut := PillowCImageLogicalOr(left, right)
        xorOut := PillowCImageLogicalXor(left, right)

        AhkTest.AssertEqual(5, PillowCImageMode(andOut))
        AhkTest.AssertEqual([0, 0, 255, 0], PillowCImageToArray(andOut, 4))
        AhkTest.AssertEqual([0x20], PillowCImageGetRawBytes(andOut, "1"))
        AhkTest.AssertEqual([0, 255, 255, 255], PillowCImageToArray(orOut, 4))
        AhkTest.AssertEqual([0x70], PillowCImageGetRawBytes(orOut, "1"))
        AhkTest.AssertEqual([0, 255, 0, 255], PillowCImageToArray(xorOut, 4))
        AhkTest.AssertEqual([0x50], PillowCImageGetRawBytes(xorOut, "1"))
    } finally {
        for handle in [xorOut, orOut, andOut, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image logical ops match Pillow mode 1", PillowCTestImageLogicalOpsMatchPillowModeOne)

PillowCTestImageLogicalOpsUseOverlappingAndEmptyOutputSize(*) {
    left := PillowCCreateImageMode(4, 1, 5)
    right := PillowCCreateImageMode(2, 1, 5)
    empty := 0
    overlap := 0
    emptyOut := 0
    try {
        PillowCImageSetRawBytes(left, [0x60], "1")
        PillowCImageSetRawBytes(right, [0xC0], "1")
        empty := PillowCImageCrop(left, 1, 0, 1, 1)
        overlap := PillowCImageLogicalAnd(left, right)
        emptyOut := PillowCImageLogicalAnd(empty, left)

        AhkTest.AssertEqual([2, 1], [PillowCImageInt(overlap, "pillow_c_image_width"), PillowCImageInt(overlap, "pillow_c_image_height")])
        AhkTest.AssertEqual([0, 255], PillowCImageToArray(overlap, 2))
        AhkTest.AssertEqual([0x40], PillowCImageGetRawBytes(overlap, "1"))
        AhkTest.AssertEqual([0, 1], [PillowCImageInt(emptyOut, "pillow_c_image_width"), PillowCImageInt(emptyOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([], PillowCImageToArray(emptyOut, 0))
        AhkTest.AssertEqual([], PillowCImageGetRawBytes(emptyOut, "1"))
    } finally {
        for handle in [emptyOut, overlap, empty, right, left] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image logical ops use overlapping and empty output size like Pillow", PillowCTestImageLogicalOpsUseOverlappingAndEmptyOutputSize)

PillowCTestImageLogicalOpsRejectNonModeOne(*) {
    left := PillowCCreateImageMode(1, 1, 1)
    right := PillowCCreateImageMode(1, 1, 1)
    target := PillowCCreateImageMode(1, 1, 1)
    outHandle := 0
    try {
        status := DllCall(PillowCDllPath() "\pillow_c_image_logical_and", "Ptr", left, "Ptr", right, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(PillowCDllPath() "\pillow_c_image_logical_or", "Ptr", left, "Ptr", right, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(PillowCDllPath() "\pillow_c_image_logical_xor_into", "Ptr", left, "Ptr", right, "Ptr", target, "Int")
        AhkTest.AssertEqual(-3, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(target)
        PillowCFreeImage(right)
        PillowCFreeImage(left)
    }
}

AhkTest.Test("pillow_c image logical ops reject non mode 1 inputs", PillowCTestImageLogicalOpsRejectNonModeOne)

PillowCTestImageDataPointerSharesMemoryWithAhk(*) {
    image := PillowCCreateImage(2, 1, 3)
    try {
        data := PillowCImageData(image)
        AhkTest.AssertEqual(6, data.Size)
        for index, value in [9, 8, 7, 6, 5, 4]
            NumPut("UChar", value, data.Ptr, index - 1)
        AhkTest.AssertEqual([9, 8, 7, 6, 5, 4], PillowCImageToArray(image, 6))
    } finally {
        PillowCFreeImage(image)
    }
}

AhkTest.Test("pillow_c image data pointer shares handle memory with AHK", PillowCTestImageDataPointerSharesMemoryWithAhk)

PillowCTestImageRawBytesDecodeStrideAndOrientation(*) {
    rgb := PillowCCreateImageMode(2, 2, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    try {
        PillowCImageSetRawBytes(rgb, [
            1, 2, 3, 4, 5, 6, 99, 99,
            7, 8, 9, 10, 11, 12, 88, 88,
        ], "BGR", 8, -1)
        AhkTest.AssertEqual([9, 8, 7, 12, 11, 10, 3, 2, 1, 6, 5, 4], PillowCImageToArray(rgb, 12))

        PillowCImageSetRawBytes(rgba, [1, 2, 3, 4, 5, 6, 7, 8], "BGRA")
        AhkTest.AssertEqual([3, 2, 1, 4, 7, 6, 5, 8], PillowCImageToArray(rgba, 8))

        PillowCImageSetRawBytes(rgba, [1, 2, 3, 4, 5, 6], "BGR")
        AhkTest.AssertEqual([3, 2, 1, 255, 6, 5, 4, 255], PillowCImageToArray(rgba, 8))
    } finally {
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
    }
}

AhkTest.Test("pillow_c image raw bytes decode modes stride and orientation", PillowCTestImageRawBytesDecodeStrideAndOrientation)

PillowCTestImageRawBytesEncodeModes(*) {
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    try {
        PillowCImageSetBytes(rgb, [10, 20, 30, 40, 50, 60])
        PillowCImageSetBytes(rgba, [10, 20, 30, 40, 50, 60, 70, 80])

        AhkTest.AssertEqual([30, 20, 10, 60, 50, 40], PillowCImageGetRawBytes(rgb, "BGR"))
        AhkTest.AssertEqual([10, 20, 30, 255, 40, 50, 60, 255], PillowCImageGetRawBytes(rgb, "RGBX"))
        AhkTest.AssertEqual([30, 20, 10, 0, 60, 50, 40, 0], PillowCImageGetRawBytes(rgb, "BGRX"))
        AhkTest.AssertEqual([30, 20, 10, 40, 70, 60, 50, 80], PillowCImageGetRawBytes(rgba, "BGRA"))
        AhkTest.AssertEqual([40, 30, 20, 10, 80, 70, 60, 50], PillowCImageGetRawBytes(rgba, "ABGR"))
        AhkTest.AssertEqual([30, 20, 10, 70, 60, 50], PillowCImageGetRawBytes(rgba, "BGR"))
    } finally {
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
    }
}

AhkTest.Test("pillow_c image raw bytes encode modes", PillowCTestImageRawBytesEncodeModes)

PillowCTestImageModeOneRawBytesAreBitPacked(*) {
    wide := PillowCCreateImageMode(9, 1, 5)
    nibble := PillowCCreateImageMode(4, 1, 5)
    try {
        PillowCImageSetRawBytes(wide, [0xAA, 0x80], "1")
        AhkTest.AssertEqual([255, 0, 255, 0, 255, 0, 255, 0, 255], PillowCImageToArray(wide, 9))
        AhkTest.AssertEqual([0xAA, 0x80], PillowCImageGetRawBytes(wide, "1"))

        PillowCImageSetRawBytes(nibble, [0x50], "1")
        AhkTest.AssertEqual([0, 255, 0, 255], PillowCImageToArray(nibble, 4))
        AhkTest.AssertEqual([0x50], PillowCImageGetRawBytes(nibble, "1"))
    } finally {
        PillowCFreeImage(nibble)
        PillowCFreeImage(wide)
    }
}

AhkTest.Test("pillow_c image mode 1 raw bytes use Pillow bit packing", PillowCTestImageModeOneRawBytesAreBitPacked)

PillowCTestImageModeOneRawBytesRejectShortPackedInput(*) {
    image := PillowCCreateImageMode(9, 1, 5)
    data := PillowCBuffer([0xAA])
    mode := PillowCRawModeBuffer("1")
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_set_raw_bytes",
            "Ptr", image,
            "Ptr", data,
            "UPtr", data.Size,
            "Ptr", mode,
            "Int", 0,
            "Int", 1,
            "Int"
        )
        AhkTest.AssertEqual(-2, status)
    } finally {
        PillowCFreeImage(image)
    }
}

AhkTest.Test("pillow_c image mode 1 raw bytes reject short packed input", PillowCTestImageModeOneRawBytesRejectShortPackedInput)

PillowCTestImageRawBytesRejectInvalidArguments(*) {
    rgb := PillowCCreateImageMode(2, 1, 3)
    data := PillowCBuffer([1, 2, 3])
    badMode := PillowCRawModeBuffer("BGRA")
    goodMode := PillowCRawModeBuffer("BGR")
    required := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_set_raw_bytes",
            "Ptr", rgb,
            "Ptr", data,
            "UPtr", data.Size,
            "Ptr", badMode,
            "Int", 0,
            "Int", 1,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_set_raw_bytes",
            "Ptr", rgb,
            "Ptr", data,
            "UPtr", data.Size,
            "Ptr", goodMode,
            "Int", 0,
            "Int", 1,
            "Int"
        )
        AhkTest.AssertEqual(-2, status)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_get_raw_bytes",
            "Ptr", rgb,
            "Ptr", badMode,
            "Ptr", 0,
            "UPtr", 0,
            "UPtr*", &required,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
    } finally {
        PillowCFreeImage(rgb)
    }
}

AhkTest.Test("pillow_c image raw bytes reject invalid arguments", PillowCTestImageRawBytesRejectInvalidArguments)

PillowCTestImagePutDataWritesPixelPrefix(*) {
    image := PillowCCreateImageMode(3, 1, 3)
    try {
        PillowCImageSetBytes(image, [1, 2, 3, 4, 5, 6, 7, 8, 9])
        PillowCImagePutData(image, [10, 20, 30, 40, 50, 60], 2)
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 7, 8, 9], PillowCImageToArray(image, 9))

        PillowCImagePutData(image, [], 0)
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 7, 8, 9], PillowCImageToArray(image, 9))
    } finally {
        PillowCFreeImage(image)
    }
}

AhkTest.Test("pillow_c image put_data writes a packed pixel prefix", PillowCTestImagePutDataWritesPixelPrefix)

PillowCTestImagePutDataRejectsInvalidLengths(*) {
    image := PillowCCreateImageMode(2, 1, 3)
    data := PillowCBuffer([1, 2, 3])
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_put_data",
            "Ptr", image,
            "Ptr", data,
            "UPtr", data.Size,
            "UPtr", 2,
            "Int"
        )
        AhkTest.AssertEqual(-2, status)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_put_data",
            "Ptr", image,
            "Ptr", data,
            "UPtr", data.Size,
            "UPtr", 3,
            "Int"
        )
        AhkTest.AssertEqual(-2, status)
    } finally {
        PillowCFreeImage(image)
    }
}

AhkTest.Test("pillow_c image put_data rejects invalid lengths", PillowCTestImagePutDataRejectsInvalidLengths)

PillowCTestImageFillRepeatsModeSizedColor(*) {
    l := PillowCCreateImageMode(4, 1, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    rgba := PillowCCreateImageMode(2, 2, 4)
    try {
        PillowCImageFill(l, [7])
        AhkTest.AssertEqual([7, 7, 7, 7], PillowCImageToArray(l, 4))

        PillowCImageFill(rgb, [10, 20, 30])
        AhkTest.AssertEqual([
            10, 20, 30, 10, 20, 30, 10, 20, 30,
            10, 20, 30, 10, 20, 30, 10, 20, 30,
        ], PillowCImageToArray(rgb, 18))

        PillowCImageFill(rgba, [1, 2, 3, 4])
        AhkTest.AssertEqual([
            1, 2, 3, 4, 1, 2, 3, 4,
            1, 2, 3, 4, 1, 2, 3, 4,
        ], PillowCImageToArray(rgba, 16))
    } finally {
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image fill repeats one mode-sized color across native storage", PillowCTestImageFillRepeatsModeSizedColor)

PillowCTestImageFillRejectsWrongColorLength(*) {
    image := PillowCCreateImageMode(2, 1, 3)
    try {
        color := PillowCBuffer([10, 20])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_fill",
            "Ptr", image,
            "Ptr", color,
            "UPtr", color.Size,
            "Int"
        )
        AhkTest.AssertEqual(-2, status)
    } finally {
        PillowCFreeImage(image)
    }
}

AhkTest.Test("pillow_c image fill rejects colors that do not match channel count", PillowCTestImageFillRejectsWrongColorLength)

PillowCTestImageGetAndPutPixelMatchPillowCoreModes(*) {
    l := PillowCCreateImageMode(2, 2, 1)
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(1, 2, 4)
    one := PillowCCreateImageMode(4, 1, 5)
    try {
        PillowCImageSetBytes(l, [1, 2, 3, 4])
        PillowCImageSetBytes(rgb, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(rgba, [1, 2, 3, 4, 5, 6, 7, 8])
        PillowCImageSetRawBytes(one, [0x50], "1")

        AhkTest.AssertEqual([1], PillowCImageGetPixel(l, 0, 0, 1))
        AhkTest.AssertEqual([2], PillowCImageGetPixel(l, -1, 0, 1))
        AhkTest.AssertEqual([4, 5, 6], PillowCImageGetPixel(rgb, 1, 0, 3))
        AhkTest.AssertEqual([1, 2, 3, 4], PillowCImageGetPixel(rgba, -1, 0, 4))
        AhkTest.AssertEqual([0], PillowCImageGetPixel(one, 0, 0, 1))
        AhkTest.AssertEqual([255], PillowCImageGetPixel(one, 1, 0, 1))

        PillowCImagePutPixel(l, 0, 0, [9])
        PillowCImagePutPixel(rgb, 0, 0, [9, 0, 0])
        PillowCImagePutPixel(rgba, 0, 0, [9, 9, 9, 9])
        PillowCImagePutPixel(one, 2, 0, [1])

        AhkTest.AssertEqual([9, 2, 3, 4], PillowCImageToArray(l, 4))
        AhkTest.AssertEqual([9, 0, 0, 4, 5, 6], PillowCImageToArray(rgb, 6))
        AhkTest.AssertEqual([9, 9, 9, 9, 5, 6, 7, 8], PillowCImageToArray(rgba, 8))
        AhkTest.AssertEqual([0, 255, 1, 255], PillowCImageToArray(one, 4))
    } finally {
        PillowCFreeImage(one)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image getpixel and putpixel match Pillow core modes", PillowCTestImageGetAndPutPixelMatchPillowCoreModes)

PillowCTestImageGetAndPutPixelRejectInvalidArguments(*) {
    image := PillowCCreateImageMode(2, 1, 3)
    try {
        PillowCImageSetBytes(image, [1, 2, 3, 4, 5, 6])

        out := Buffer(3, 0)
        status := DllCall(PillowCDllPath() "\pillow_c_image_getpixel", "Ptr", image, "Int", 99, "Int", 0, "Ptr", out, "UPtr", out.Size, "Int")
        AhkTest.AssertEqual(-3, status)

        color := PillowCBuffer([1, 2])
        status := DllCall(PillowCDllPath() "\pillow_c_image_putpixel", "Ptr", image, "Int", 0, "Int", 0, "Ptr", color, "UPtr", color.Size, "Int")
        AhkTest.AssertEqual(-2, status)
    } finally {
        PillowCFreeImage(image)
    }
}

AhkTest.Test("pillow_c image getpixel and putpixel reject invalid arguments", PillowCTestImageGetAndPutPixelRejectInvalidArguments)

PillowCMakeInvertLut(channelCount) {
    lut := []
    loop channelCount {
        loop 256
            lut.Push(256 - A_Index)
    }
    return lut
}

PillowCMakeIdentityLut(channelCount) {
    lut := []
    loop channelCount {
        loop 256
            lut.Push(A_Index - 1)
    }
    return lut
}

PillowCConstantArray(count, value) {
    items := []
    loop count
        items.Push(value)
    return items
}

PillowCTestImagePointLutAppliesPerChannelTables(*) {
    l := PillowCCreateImageMode(4, 1, 1)
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    one := PillowCCreateImageMode(4, 1, 5)
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    oneInvert := 0
    oneConst := 0
    try {
        PillowCImageSetBytes(l, [0, 1, 2, 255])
        PillowCImageSetBytes(rgb, [0, 1, 2, 10, 20, 30])
        PillowCImageSetBytes(rgba, [0, 1, 2, 3, 10, 20, 30, 40])
        PillowCImageSetRawBytes(one, [0x50], "1")
        lOut := PillowCImagePointLut(l, PillowCMakeInvertLut(1))
        rgbOut := PillowCImagePointLut(rgb, PillowCMakeInvertLut(3))
        rgbaOut := PillowCImagePointLut(rgba, PillowCMakeInvertLut(4))
        oneInvert := PillowCImagePointLut(one, PillowCMakeInvertLut(1))
        oneConst := PillowCImagePointLut(one, PillowCConstantArray(256, 2))
        AhkTest.AssertEqual([255, 254, 253, 0], PillowCImageToArray(lOut, 4))
        AhkTest.AssertEqual([255, 254, 253, 245, 235, 225], PillowCImageToArray(rgbOut, 6))
        AhkTest.AssertEqual([255, 254, 253, 252, 245, 235, 225, 215], PillowCImageToArray(rgbaOut, 8))
        AhkTest.AssertEqual([255, 0, 255, 0], PillowCImageToArray(oneInvert, 4))
        AhkTest.AssertEqual([2, 2, 2, 2], PillowCImageToArray(oneConst, 4))
    } finally {
        if oneConst
            PillowCFreeImage(oneConst)
        if oneInvert
            PillowCFreeImage(oneInvert)
        if rgbaOut
            PillowCFreeImage(rgbaOut)
        if rgbOut
            PillowCFreeImage(rgbOut)
        if lOut
            PillowCFreeImage(lOut)
        PillowCFreeImage(one)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image point_lut applies per-channel Pillow LUT tables", PillowCTestImagePointLutAppliesPerChannelTables)

PillowCTestImagePointLutModeConvertsSingleBandModes(*) {
    l := PillowCCreateImageMode(6, 1, 1)
    p := PillowCCreateImageMode(6, 1, 6)
    one := PillowCCreateImageMode(4, 1, 5)
    lOut := 0
    pOut := 0
    oneOut := 0
    lToP := 0
    lTarget := PillowCCreateImageMode(6, 1, 5)
    try {
        PillowCImageSetBytes(l, [0, 1, 127, 128, 129, 255])
        PillowCImageSetBytes(p, [0, 1, 2, 3, 4, 5])
        PillowCImageSetRawBytes(one, [0x50], "1")
        lOut := PillowCImagePointLutMode(l, PillowCMakeIdentityLut(1), 5)
        pOut := PillowCImagePointLutMode(p, PillowCMakeInvertLut(1), 5)
        oneOut := PillowCImagePointLutMode(one, PillowCMakeIdentityLut(1), 1)
        lToP := PillowCImagePointLutMode(l, PillowCMakeIdentityLut(1), 6)
        before := PillowCImageData(lTarget).Ptr
        PillowCImagePointLutModeInto(l, PillowCMakeIdentityLut(1), 5, lTarget)

        AhkTest.AssertEqual(5, PillowCImageMode(lOut))
        AhkTest.AssertEqual([0, 1, 127, 128, 129, 255], PillowCImageToArray(lOut, 6))
        AhkTest.AssertEqual([0x7C], PillowCImageGetRawBytes(lOut, "1"))
        AhkTest.AssertEqual(5, PillowCImageMode(pOut))
        AhkTest.AssertEqual([255, 254, 253, 252, 251, 250], PillowCImageToArray(pOut, 6))
        AhkTest.AssertEqual([0xFC], PillowCImageGetRawBytes(pOut, "1"))
        AhkTest.AssertEqual(1, PillowCImageMode(oneOut))
        AhkTest.AssertEqual([0, 255, 0, 255], PillowCImageToArray(oneOut, 4))
        AhkTest.AssertEqual(6, PillowCImageMode(lToP))
        AhkTest.AssertEqual([0, 1, 127, 128, 129, 255], PillowCImageToArray(lToP, 6))
        AhkTest.AssertEqual([], PillowCImageGetPaletteRgb(lToP))
        AhkTest.AssertEqual(before, PillowCImageData(lTarget).Ptr)
        AhkTest.AssertEqual(PillowCImageToArray(lOut, 6), PillowCImageToArray(lTarget, 6))
    } finally {
        for handle in [lTarget, lToP, oneOut, pOut, lOut, one, p, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image point_lut_mode converts single-band modes", PillowCTestImagePointLutModeConvertsSingleBandModes)

PillowCTestImagePointLutIntoReusesTargetHandle(*) {
    source := PillowCCreateImageMode(2, 1, 3)
    target := PillowCCreateImageMode(2, 1, 3)
    try {
        PillowCImageSetBytes(source, [0, 1, 2, 10, 20, 30])
        before := PillowCImageData(target).Ptr
        PillowCImagePointLutInto(source, PillowCMakeInvertLut(3), target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([255, 254, 253, 245, 235, 225], PillowCImageToArray(target, 6))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image point_lut_into reuses target handle storage", PillowCTestImagePointLutIntoReusesTargetHandle)

PillowCTestImagePointLutRejectsWrongLength(*) {
    source := PillowCCreateImageMode(2, 1, 3)
    outHandle := 0
    try {
        lut := PillowCBuffer(PillowCMakeInvertLut(1))
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_point_lut",
            "Ptr", source,
            "Ptr", lut,
            "UPtr", lut.Size,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-2, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image point_lut rejects LUT lengths that do not match Pillow mode", PillowCTestImagePointLutRejectsWrongLength)

PillowCTestImageOpsLutTransformsMatchPillowLAndRgb(*) {
    l := PillowCCreateImageMode(5, 1, 1)
    rgb := PillowCCreateImageMode(3, 1, 3)
    lInvert := 0
    rgbInvert := 0
    lPosterize := 0
    rgbPosterize := 0
    lSolarize := 0
    rgbSolarize := 0
    try {
        PillowCImageSetBytes(l, [0, 1, 127, 128, 255])
        PillowCImageSetBytes(rgb, [
            0, 1, 2,
            127, 128, 129,
            253, 254, 255,
        ])

        lInvert := PillowCImageInvert(l)
        rgbInvert := PillowCImageInvert(rgb)
        lPosterize := PillowCImagePosterize(l, 4)
        rgbPosterize := PillowCImagePosterize(rgb, 4)
        lSolarize := PillowCImageSolarize(l, 128.0)
        rgbSolarize := PillowCImageSolarize(rgb, 128.0)

        AhkTest.AssertEqual([255, 254, 128, 127, 0], PillowCImageToArray(lInvert, 5))
        AhkTest.AssertEqual([255, 254, 253, 128, 127, 126, 2, 1, 0], PillowCImageToArray(rgbInvert, 9))
        AhkTest.AssertEqual([0, 0, 112, 128, 240], PillowCImageToArray(lPosterize, 5))
        AhkTest.AssertEqual([0, 0, 0, 112, 128, 128, 240, 240, 240], PillowCImageToArray(rgbPosterize, 9))
        AhkTest.AssertEqual([0, 1, 127, 127, 0], PillowCImageToArray(lSolarize, 5))
        AhkTest.AssertEqual([0, 1, 2, 127, 127, 126, 2, 1, 0], PillowCImageToArray(rgbSolarize, 9))
    } finally {
        for handle in [rgbSolarize, lSolarize, rgbPosterize, lPosterize, rgbInvert, lInvert] {
            if handle
                PillowCFreeImage(handle)
        }
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image ImageOps LUT transforms match Pillow for L and RGB", PillowCTestImageOpsLutTransformsMatchPillowLAndRgb)

PillowCTestImageOpsInvertMatchesPillowModeOne(*) {
    source := PillowCCreateImageMode(3, 1, 5)
    out := 0
    try {
        PillowCImageSetRawBytes(source, [0x40], "1")

        out := PillowCImageInvert(source)

        AhkTest.AssertEqual(5, PillowCImageMode(out))
        AhkTest.AssertEqual([255, 0, 255], PillowCImageToArray(out, 3))
    } finally {
        if out
            PillowCFreeImage(out)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image ImageOps invert matches Pillow mode 1", PillowCTestImageOpsInvertMatchesPillowModeOne)

PillowCTestImageOpsLutTransformsIntoReuseTargetHandleStorage(*) {
    source := PillowCCreateImageMode(5, 1, 1)
    invertTarget := PillowCCreateImageMode(5, 1, 1)
    posterizeTarget := PillowCCreateImageMode(5, 1, 1)
    solarizeTarget := PillowCCreateImageMode(5, 1, 1)
    try {
        PillowCImageSetBytes(source, [0, 1, 127, 128, 255])

        invertBefore := PillowCImageData(invertTarget).Ptr
        PillowCImageInvertInto(source, invertTarget)
        AhkTest.AssertEqual(invertBefore, PillowCImageData(invertTarget).Ptr)
        AhkTest.AssertEqual([255, 254, 128, 127, 0], PillowCImageToArray(invertTarget, 5))

        posterizeBefore := PillowCImageData(posterizeTarget).Ptr
        PillowCImagePosterizeInto(source, 4, posterizeTarget)
        AhkTest.AssertEqual(posterizeBefore, PillowCImageData(posterizeTarget).Ptr)
        AhkTest.AssertEqual([0, 0, 112, 128, 240], PillowCImageToArray(posterizeTarget, 5))

        solarizeBefore := PillowCImageData(solarizeTarget).Ptr
        PillowCImageSolarizeInto(source, 128.0, solarizeTarget)
        AhkTest.AssertEqual(solarizeBefore, PillowCImageData(solarizeTarget).Ptr)
        AhkTest.AssertEqual([0, 1, 127, 127, 0], PillowCImageToArray(solarizeTarget, 5))
    } finally {
        PillowCFreeImage(solarizeTarget)
        PillowCFreeImage(posterizeTarget)
        PillowCFreeImage(invertTarget)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image ImageOps LUT transform _into calls reuse target handle storage", PillowCTestImageOpsLutTransformsIntoReuseTargetHandleStorage)

PillowCTestImageOpsLutTransformsHandlePosterizeAndSolarizeEdges(*) {
    source := PillowCCreateImageMode(5, 1, 1)
    posterizeZero := 0
    posterizeEight := 0
    solarizeLow := 0
    solarizeHigh := 0
    try {
        PillowCImageSetBytes(source, [0, 1, 127, 128, 255])
        posterizeZero := PillowCImagePosterize(source, 0)
        posterizeEight := PillowCImagePosterize(source, 8)
        solarizeLow := PillowCImageSolarize(source, 0.0)
        solarizeHigh := PillowCImageSolarize(source, 256.0)

        AhkTest.AssertEqual([0, 0, 0, 0, 0], PillowCImageToArray(posterizeZero, 5))
        AhkTest.AssertEqual([0, 1, 127, 128, 255], PillowCImageToArray(posterizeEight, 5))
        AhkTest.AssertEqual([255, 254, 128, 127, 0], PillowCImageToArray(solarizeLow, 5))
        AhkTest.AssertEqual([0, 1, 127, 128, 255], PillowCImageToArray(solarizeHigh, 5))
    } finally {
        for handle in [solarizeHigh, solarizeLow, posterizeEight, posterizeZero] {
            if handle
                PillowCFreeImage(handle)
        }
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image posterize and solarize cover Pillow edge parameters", PillowCTestImageOpsLutTransformsHandlePosterizeAndSolarizeEdges)

PillowCTestImageColorizeMatchesPillowLMapping(*) {
    source := PillowCCreateImageMode(5, 1, 1)
    out := 0
    midOut := 0
    pointsOut := 0
    try {
        PillowCImageSetBytes(source, [0, 64, 128, 192, 255])
        out := PillowCImageColorize(source, [10, 20, 30], [110, 120, 130])
        midOut := PillowCImageColorize(source, [0, 0, 0], [255, 255, 255], true, [255, 0, 0])
        pointsOut := PillowCImageColorize(source, [0, 0, 0], [255, 255, 255], true, [0, 255, 0], 32, 224, 128)

        AhkTest.AssertEqual(3, PillowCImageMode(out))
        AhkTest.AssertEqual([10, 20, 30, 35, 45, 55, 60, 70, 80, 85, 95, 105, 110, 120, 130], PillowCImageToArray(out, 15))
        AhkTest.AssertEqual([0, 0, 0, 128, 0, 0, 255, 1, 1, 255, 129, 129, 255, 255, 255], PillowCImageToArray(midOut, 15))
        AhkTest.AssertEqual([0, 0, 0, 0, 85, 0, 0, 255, 0, 170, 255, 170, 255, 255, 255], PillowCImageToArray(pointsOut, 15))
    } finally {
        for handle in [pointsOut, midOut, out, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image colorize maps L pixels through Pillow color wedges", PillowCTestImageColorizeMatchesPillowLMapping)

PillowCTestImageColorizeIntoReusesTargetStorageAndHandlesEmpty(*) {
    source := PillowCCreateImageMode(3, 1, 1)
    target := PillowCCreateImageMode(3, 1, 3)
    emptySource := PillowCCreateImageMode(1, 2, 1)
    emptyTargetSource := PillowCCreateImageMode(1, 2, 3)
    empty := 0
    emptyTarget := 0
    try {
        PillowCImageSetBytes(source, [0, 128, 255])
        before := PillowCImageData(target).Ptr
        PillowCImageColorizeInto(source, [1, 2, 3], [17, 18, 19], target)
        after := PillowCImageData(target).Ptr

        empty := PillowCImageCrop(emptySource, 0, 0, 0, 2)
        emptyTarget := PillowCImageCrop(emptyTargetSource, 0, 0, 0, 2)
        PillowCImageColorizeInto(empty, [0, 0, 0], [255, 255, 255], emptyTarget)

        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([1, 2, 3, 9, 10, 11, 17, 18, 19], PillowCImageToArray(target, 9))
        AhkTest.AssertEqual([], PillowCImageToArray(emptyTarget, 0))
    } finally {
        PillowCFreeImage(emptyTarget)
        PillowCFreeImage(empty)
        PillowCFreeImage(emptyTargetSource)
        PillowCFreeImage(emptySource)
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image colorize_into reuses target storage and handles empty images", PillowCTestImageColorizeIntoReusesTargetStorageAndHandlesEmpty)

PillowCTestImageColorizeRejectsInvalidArguments(*) {
    l := PillowCCreateImageMode(1, 1, 1)
    rgb := PillowCCreateImageMode(1, 1, 3)
    targetL := PillowCCreateImageMode(1, 1, 1)
    outHandle := 0
    try {
        black := PillowCBuffer([0, 0, 0])
        white := PillowCBuffer([255, 255, 255])
        mid := PillowCBuffer([128, 128, 128])

        status := DllCall(PillowCDllPath() "\pillow_c_image_colorize", "Ptr", rgb, "Ptr", black, "Ptr", white, "Int", false, "Ptr", 0, "Int", 0, "Int", 255, "Int", 127, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(PillowCDllPath() "\pillow_c_image_colorize", "Ptr", l, "Ptr", black, "Ptr", white, "Int", true, "Ptr", mid, "Int", 100, "Int", 200, "Int", 50, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(PillowCDllPath() "\pillow_c_image_colorize_into", "Ptr", l, "Ptr", black, "Ptr", white, "Int", false, "Ptr", 0, "Int", 0, "Int", 255, "Int", 127, "Ptr", targetL, "Int")
        AhkTest.AssertEqual(-5, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(targetL)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image colorize rejects unsupported modes points and target shapes", PillowCTestImageColorizeRejectsInvalidArguments)

PillowCTestImageOpsLutTransformsRejectUnsupportedModes(*) {
    rgba := PillowCCreateImageMode(1, 1, 4)
    outHandle := 0
    try {
        PillowCImageSetBytes(rgba, [1, 2, 3, 4])
        for exportName in ["pillow_c_image_invert", "pillow_c_image_posterize", "pillow_c_image_solarize"] {
            outHandle := 0
            if exportName = "pillow_c_image_posterize" {
                status := DllCall(
                    PillowCDllPath() "\" exportName,
                    "Ptr", rgba,
                    "Int", 4,
                    "Ptr*", &outHandle,
                    "Int"
                )
            } else if exportName = "pillow_c_image_solarize" {
                status := DllCall(
                    PillowCDllPath() "\" exportName,
                    "Ptr", rgba,
                    "Double", 128.0,
                    "Ptr*", &outHandle,
                    "Int"
                )
            } else {
                status := DllCall(
                    PillowCDllPath() "\" exportName,
                    "Ptr", rgba,
                    "Ptr*", &outHandle,
                    "Int"
                )
            }
            AhkTest.AssertEqual(-3, status)
            AhkTest.AssertEqual(0, outHandle)
        }
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(rgba)
    }
}

AhkTest.Test("pillow_c image ImageOps LUT transforms reject modes Pillow _lut does not support", PillowCTestImageOpsLutTransformsRejectUnsupportedModes)

PillowCTestImageEqualizeMatchesPillowLAndRgb(*) {
    l := PillowCCreateImageMode(400, 1, 1)
    rgb := PillowCCreateImageMode(400, 1, 3)
    lOut := 0
    rgbOut := 0
    try {
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
        PillowCImageSetBytes(l, lValues)
        PillowCImageSetBytes(rgb, rgbValues)

        lOut := PillowCImageEqualize(l)
        rgbOut := PillowCImageEqualize(rgb)

        AhkTest.AssertEqual(0, PillowCImageToArray(lOut, 400)[1])
        AhkTest.AssertEqual(0, PillowCImageToArray(lOut, 400)[300])
        AhkTest.AssertEqual(255, PillowCImageToArray(lOut, 400)[301])
        AhkTest.AssertEqual(255, PillowCImageToArray(lOut, 400)[400])

        rgbData := PillowCImageToArray(rgbOut, 1200)
        AhkTest.AssertEqual([0, 0, 0], [rgbData[1], rgbData[2], rgbData[3]])
        AhkTest.AssertEqual([0, 0, 100], [rgbData[301], rgbData[302], rgbData[303]])
        AhkTest.AssertEqual([0, 0, 200], [rgbData[601], rgbData[602], rgbData[603]])
        AhkTest.AssertEqual([255, 255, 255], [rgbData[901], rgbData[902], rgbData[903]])
        AhkTest.AssertEqual([255, 255, 255], [rgbData[1198], rgbData[1199], rgbData[1200]])
    } finally {
        if rgbOut
            PillowCFreeImage(rgbOut)
        if lOut
            PillowCFreeImage(lOut)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image equalize matches Pillow for L and RGB histograms", PillowCTestImageEqualizeMatchesPillowLAndRgb)

PillowCTestEqualizePaletteFixture() {
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

PillowCTestAssertSamples(data, positions, expected) {
    actual := []
    for position in positions
        actual.Push(data[position])
    AhkTest.AssertEqual(expected, actual)
}

PillowCTestImageEqualizeConvertsPaletteModeToRgb(*) {
    fixture := PillowCTestEqualizePaletteFixture()
    source := PillowCCreateImageMode(400, 1, 6)
    target := PillowCCreateImageMode(400, 1, 3)
    out := 0
    try {
        PillowCImageSetBytes(source, fixture.Values)
        PillowCImagePutPaletteRgb(source, fixture.Palette)

        out := PillowCImageEqualize(source)
        before := PillowCImageData(target).Ptr
        PillowCImageEqualizeInto(source, target)
        after := PillowCImageData(target).Ptr

        AhkTest.AssertEqual(3, PillowCImageMode(out))
        AhkTest.AssertEqual([400, 1], [PillowCImageInt(out, "pillow_c_image_width"), PillowCImageInt(out, "pillow_c_image_height")])
        AhkTest.AssertEqual(before, after)
        PillowCTestAssertSamples(
            PillowCImageToArray(out, 1200),
            [1, 2, 3, 898, 899, 900, 901, 902, 903, 1048, 1049, 1050, 1051, 1052, 1053, 1198, 1199, 1200],
            [0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255])
        PillowCTestAssertSamples(
            PillowCImageToArray(target, 1200),
            [1, 2, 3, 898, 899, 900, 901, 902, 903, 1048, 1049, 1050, 1051, 1052, 1053, 1198, 1199, 1200],
            [0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255])
    } finally {
        if out
            PillowCFreeImage(out)
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image equalize converts P images to RGB like Pillow", PillowCTestImageEqualizeConvertsPaletteModeToRgb)

PillowCTestAppendEqualizeMaskGroup(fixture, count, lValue, r, g, b, maskValue) {
    loop count {
        fixture.L.Push(lValue)
        fixture.RGB.Push(r)
        fixture.RGB.Push(g)
        fixture.RGB.Push(b)
        fixture.Mask.Push(maskValue)
    }
}

PillowCTestEqualizeMaskFixture() {
    fixture := { L: [], RGB: [], Mask: [] }
    PillowCTestAppendEqualizeMaskGroup(fixture, 300, 0, 0, 10, 20, 0)
    PillowCTestAppendEqualizeMaskGroup(fixture, 10, 0, 0, 10, 20, 255)
    PillowCTestAppendEqualizeMaskGroup(fixture, 300, 64, 64, 70, 80, 128)
    PillowCTestAppendEqualizeMaskGroup(fixture, 300, 128, 128, 130, 140, 255)
    PillowCTestAppendEqualizeMaskGroup(fixture, 100, 255, 255, 250, 240, 64)
    return fixture
}

PillowCTestImageEqualizeUsesMaskHistogram(*) {
    fixture := PillowCTestEqualizeMaskFixture()
    l := PillowCCreateImageMode(1010, 1, 1)
    rgb := PillowCCreateImageMode(1010, 1, 3)
    mask := PillowCCreateImageMode(1010, 1, 1)
    lTarget := PillowCCreateImageMode(1010, 1, 1)
    lOut := 0
    rgbOut := 0
    try {
        PillowCImageSetBytes(l, fixture.L)
        PillowCImageSetBytes(rgb, fixture.RGB)
        PillowCImageSetBytes(mask, fixture.Mask)

        lOut := PillowCImageEqualizeMasked(l, mask)
        rgbOut := PillowCImageEqualizeMasked(rgb, mask)
        before := PillowCImageData(lTarget).Ptr
        PillowCImageEqualizeMaskedInto(l, mask, lTarget)
        after := PillowCImageData(lTarget).Ptr

        lData := PillowCImageToArray(lOut, 1010)
        rgbData := PillowCImageToArray(rgbOut, 3030)
        targetData := PillowCImageToArray(lTarget, 1010)

        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([0, 0, 5, 155, 255], [lData[1], lData[301], lData[311], lData[611], lData[911]])
        AhkTest.AssertEqual([0, 0, 5, 155, 255], [targetData[1], targetData[301], targetData[311], targetData[611], targetData[911]])
        AhkTest.AssertEqual([0, 0, 0], [rgbData[1], rgbData[2], rgbData[3]])
        AhkTest.AssertEqual([5, 5, 5], [rgbData[931], rgbData[932], rgbData[933]])
        AhkTest.AssertEqual([155, 155, 155], [rgbData[1831], rgbData[1832], rgbData[1833]])
        AhkTest.AssertEqual([255, 255, 255], [rgbData[2731], rgbData[2732], rgbData[2733]])
    } finally {
        if rgbOut
            PillowCFreeImage(rgbOut)
        if lOut
            PillowCFreeImage(lOut)
        PillowCFreeImage(lTarget)
        PillowCFreeImage(mask)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image equalize uses mask histogram", PillowCTestImageEqualizeUsesMaskHistogram)

PillowCTestImageEqualizeConvertsMaskedPaletteModeToRgb(*) {
    fixture := PillowCTestEqualizePaletteFixture()
    source := PillowCCreateImageMode(400, 1, 6)
    mask := PillowCCreateImageMode(400, 1, 1)
    target := PillowCCreateImageMode(400, 1, 3)
    out := 0
    try {
        PillowCImageSetBytes(source, fixture.Values)
        PillowCImageSetBytes(mask, fixture.Mask)
        PillowCImagePutPaletteRgb(source, fixture.Palette)

        out := PillowCImageEqualizeMasked(source, mask)
        before := PillowCImageData(target).Ptr
        PillowCImageEqualizeMaskedInto(source, mask, target)
        after := PillowCImageData(target).Ptr

        AhkTest.AssertEqual(3, PillowCImageMode(out))
        AhkTest.AssertEqual([400, 1], [PillowCImageInt(out, "pillow_c_image_width"), PillowCImageInt(out, "pillow_c_image_height")])
        AhkTest.AssertEqual(before, after)
        PillowCTestAssertSamples(
            PillowCImageToArray(out, 1200),
            [1, 2, 3, 871, 872, 873, 898, 899, 900, 901, 902, 903, 1048, 1049, 1050, 1051, 1052, 1053, 1198, 1199, 1200],
            [0, 10, 0, 0, 10, 0, 0, 10, 0, 128, 200, 64, 128, 200, 64, 255, 200, 255, 255, 200, 255])
        PillowCTestAssertSamples(
            PillowCImageToArray(target, 1200),
            [1, 2, 3, 871, 872, 873, 898, 899, 900, 901, 902, 903, 1048, 1049, 1050, 1051, 1052, 1053, 1198, 1199, 1200],
            [0, 10, 0, 0, 10, 0, 0, 10, 0, 128, 200, 64, 128, 200, 64, 255, 200, 255, 255, 200, 255])
    } finally {
        if out
            PillowCFreeImage(out)
        PillowCFreeImage(target)
        PillowCFreeImage(mask)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image equalize converts masked P images to RGB like Pillow", PillowCTestImageEqualizeConvertsMaskedPaletteModeToRgb)

PillowCTestImageEqualizeIntoReusesTargetHandleStorage(*) {
    source := PillowCCreateImageMode(400, 1, 1)
    target := PillowCCreateImageMode(400, 1, 1)
    try {
        values := []
        loop 400 {
            index := A_Index - 1
            values.Push(index < 300 ? 10 : 200)
        }
        PillowCImageSetBytes(source, values)

        before := PillowCImageData(target).Ptr
        PillowCImageEqualizeInto(source, target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)

        data := PillowCImageToArray(target, 400)
        AhkTest.AssertEqual(0, data[1])
        AhkTest.AssertEqual(0, data[300])
        AhkTest.AssertEqual(255, data[301])
        AhkTest.AssertEqual(255, data[400])
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image equalize_into reuses target handle storage", PillowCTestImageEqualizeIntoReusesTargetHandleStorage)

PillowCTestImageEqualizeKeepsFlatAndEmptyImagesIdentity(*) {
    flat := PillowCCreateImageMode(4, 1, 1)
    empty := 0
    flatOut := 0
    emptyOut := 0
    try {
        PillowCImageSetBytes(flat, [10, 10, 10, 10])
        empty := PillowCImageCrop(flat, 0, 0, 0, 1)
        flatOut := PillowCImageEqualize(flat)
        emptyOut := PillowCImageEqualize(empty)

        AhkTest.AssertEqual([10, 10, 10, 10], PillowCImageToArray(flatOut, 4))
        AhkTest.AssertEqual([], PillowCImageToArray(emptyOut, 0))
        AhkTest.AssertEqual([0, 1], [PillowCImageInt(emptyOut, "pillow_c_image_width"), PillowCImageInt(emptyOut, "pillow_c_image_height")])
    } finally {
        if emptyOut
            PillowCFreeImage(emptyOut)
        if flatOut
            PillowCFreeImage(flatOut)
        if empty
            PillowCFreeImage(empty)
        PillowCFreeImage(flat)
    }
}

AhkTest.Test("pillow_c image equalize keeps flat and empty images as Pillow identity", PillowCTestImageEqualizeKeepsFlatAndEmptyImagesIdentity)

PillowCTestImageEqualizeRejectsUnsupportedModes(*) {
    rgba := PillowCCreateImageMode(1, 1, 4)
    outHandle := 0
    try {
        PillowCImageSetBytes(rgba, [1, 2, 3, 4])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_equalize",
            "Ptr", rgba,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(rgba)
    }
}

AhkTest.Test("pillow_c image equalize rejects modes Pillow _lut does not support", PillowCTestImageEqualizeRejectsUnsupportedModes)

PillowCTestImageHistogramMatchesPillowBands(*) {
    l := PillowCCreateImageMode(4, 1, 1)
    rgb := PillowCCreateImageMode(3, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    empty := 0
    try {
        PillowCImageSetBytes(l, [0, 10, 10, 255])
        PillowCImageSetBytes(rgb, [
            0, 10, 20,
            255, 10, 20,
            0, 255, 20,
        ])
        PillowCImageSetBytes(rgba, [
            0, 10, 20, 30,
            255, 10, 20, 128,
        ])
        empty := PillowCImageCrop(l, 0, 0, 0, 1)

        lHist := PillowCImageHistogram(l, 256)
        rgbHist := PillowCImageHistogram(rgb, 768)
        rgbaHist := PillowCImageHistogram(rgba, 1024)
        emptyHist := PillowCImageHistogram(empty, 256)

        AhkTest.AssertEqual(1, lHist[1])
        AhkTest.AssertEqual(2, lHist[11])
        AhkTest.AssertEqual(1, lHist[256])
        AhkTest.AssertEqual(4, SumArray(lHist))

        AhkTest.AssertEqual(2, rgbHist[1])
        AhkTest.AssertEqual(1, rgbHist[256])
        AhkTest.AssertEqual(2, rgbHist[267])
        AhkTest.AssertEqual(1, rgbHist[512])
        AhkTest.AssertEqual(3, rgbHist[533])
        AhkTest.AssertEqual(9, SumArray(rgbHist))

        AhkTest.AssertEqual(1, rgbaHist[1])
        AhkTest.AssertEqual(1, rgbaHist[256])
        AhkTest.AssertEqual(2, rgbaHist[267])
        AhkTest.AssertEqual(2, rgbaHist[533])
        AhkTest.AssertEqual(1, rgbaHist[799])
        AhkTest.AssertEqual(1, rgbaHist[897])
        AhkTest.AssertEqual(8, SumArray(rgbaHist))

        AhkTest.AssertEqual(256, emptyHist.Length)
        AhkTest.AssertEqual(0, SumArray(emptyHist))
    } finally {
        if empty
            PillowCFreeImage(empty)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image histogram matches Pillow band layout", PillowCTestImageHistogramMatchesPillowBands)

PillowCTestImageHistogramMaskedMatchesPillowMaskRules(*) {
    l := PillowCCreateImageMode(4, 1, 1)
    rgb := PillowCCreateImageMode(3, 1, 3)
    la := PillowCCreateImageMode(2, 1, 2)
    lMask := PillowCCreateImageMode(4, 1, 1)
    oneMask := PillowCCreateImageMode(4, 1, 5)
    rgbMask := PillowCCreateImageMode(3, 1, 1)
    laMask := PillowCCreateImageMode(2, 1, 1)
    try {
        PillowCImageSetBytes(l, [0, 10, 20, 30])
        PillowCImageSetBytes(lMask, [0, 128, 255, 64])
        PillowCImageSetRawBytes(oneMask, [0x60], "1")
        PillowCImageSetBytes(rgb, [
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
        ])
        PillowCImageSetBytes(rgbMask, [0, 128, 255])
        PillowCImageSetBytes(la, [10, 40, 200, 128])
        PillowCImageSetBytes(laMask, [128, 255])

        lHist := PillowCImageHistogramMasked(l, lMask, 256)
        oneHist := PillowCImageHistogramMasked(l, oneMask, 256)
        rgbHist := PillowCImageHistogramMasked(rgb, rgbMask, 768)
        laHist := PillowCImageHistogramMasked(la, laMask, 512)

        AhkTest.AssertEqual(1, lHist[11])
        AhkTest.AssertEqual(1, lHist[21])
        AhkTest.AssertEqual(1, lHist[31])
        AhkTest.AssertEqual(3, SumArray(lHist))

        AhkTest.AssertEqual(1, oneHist[11])
        AhkTest.AssertEqual(1, oneHist[21])
        AhkTest.AssertEqual(2, SumArray(oneHist))

        AhkTest.AssertEqual(1, rgbHist[41])
        AhkTest.AssertEqual(1, rgbHist[71])
        AhkTest.AssertEqual(1, rgbHist[307])
        AhkTest.AssertEqual(1, rgbHist[337])
        AhkTest.AssertEqual(1, rgbHist[573])
        AhkTest.AssertEqual(1, rgbHist[603])
        AhkTest.AssertEqual(6, SumArray(rgbHist))

        AhkTest.AssertEqual(1, laHist[11])
        AhkTest.AssertEqual(1, laHist[201])
        AhkTest.AssertEqual(1, laHist[267])
        AhkTest.AssertEqual(1, laHist[457])
        AhkTest.AssertEqual(4, SumArray(laHist))
    } finally {
        PillowCFreeImage(laMask)
        PillowCFreeImage(rgbMask)
        PillowCFreeImage(oneMask)
        PillowCFreeImage(lMask)
        PillowCFreeImage(la)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image histogram_masked matches Pillow L mask rules", PillowCTestImageHistogramMaskedMatchesPillowMaskRules)

PillowCTestImageEntropyMatchesPillowCoreModes(*) {
    l := PillowCCreateImageMode(4, 1, 1)
    rgb := PillowCCreateImageMode(3, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    la := PillowCCreateImageMode(2, 1, 2)
    empty := 0
    try {
        PillowCImageSetBytes(l, [0, 10, 10, 255])
        PillowCImageSetBytes(rgb, [
            0, 10, 20,
            255, 10, 20,
            0, 255, 20,
        ])
        PillowCImageSetBytes(rgba, [
            0, 10, 20, 30,
            255, 10, 20, 128,
        ])
        PillowCImageSetBytes(la, [10, 40, 200, 128])
        empty := PillowCImageCrop(l, 0, 0, 0, 1)

        PillowCAssertFloatClose(1.5, PillowCImageEntropy(l))
        PillowCAssertFloatClose(2.197159723424149, PillowCImageEntropy(rgb))
        PillowCAssertFloatClose(2.5, PillowCImageEntropy(rgba))
        PillowCAssertFloatClose(2.0, PillowCImageEntropy(la))
        emptyEntropy := PillowCImageEntropy(empty)
        AhkTest.AssertTrue(emptyEntropy != emptyEntropy)
    } finally {
        if empty
            PillowCFreeImage(empty)
        PillowCFreeImage(la)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image entropy matches Pillow core modes", PillowCTestImageEntropyMatchesPillowCoreModes)

PillowCTestImageEntropyUsesLMask(*) {
    l := PillowCCreateImageMode(4, 1, 1)
    rgb := PillowCCreateImageMode(3, 1, 3)
    mask := PillowCCreateImageMode(4, 1, 1)
    oneMask := PillowCCreateImageMode(4, 1, 5)
    rgbMask := PillowCCreateImageMode(3, 1, 1)
    try {
        PillowCImageSetBytes(l, [0, 10, 20, 30])
        PillowCImageSetBytes(mask, [0, 128, 255, 64])
        PillowCImageSetRawBytes(oneMask, [0x60], "1")
        PillowCImageSetBytes(rgb, [
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
        ])
        PillowCImageSetBytes(rgbMask, [0, 128, 255])

        PillowCAssertFloatClose(1.584962500721156, PillowCImageEntropy(l, mask))
        PillowCAssertFloatClose(1.0, PillowCImageEntropy(l, oneMask))
        PillowCAssertFloatClose(2.584962500721156, PillowCImageEntropy(rgb, rgbMask))
    } finally {
        PillowCFreeImage(rgbMask)
        PillowCFreeImage(oneMask)
        PillowCFreeImage(mask)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image entropy uses L mask", PillowCTestImageEntropyUsesLMask)

PillowCTestImageGetExtremaMatchesPillowBands(*) {
    l := PillowCCreateImageMode(4, 1, 1)
    rgb := PillowCCreateImageMode(3, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    empty := 0
    try {
        PillowCImageSetBytes(l, [0, 10, 10, 255])
        PillowCImageSetBytes(rgb, [
            0, 10, 20,
            255, 10, 20,
            0, 255, 20,
        ])
        PillowCImageSetBytes(rgba, [
            0, 10, 20, 30,
            255, 10, 20, 128,
        ])
        empty := PillowCImageCrop(l, 0, 0, 0, 1)

        AhkTest.AssertEqual([[0, 255]], PillowCImageExtrema(l, 1))
        AhkTest.AssertEqual([[0, 255], [10, 255], [20, 20]], PillowCImageExtrema(rgb, 3))
        AhkTest.AssertEqual([[0, 255], [10, 10], [20, 20], [30, 128]], PillowCImageExtrema(rgba, 4))
        AhkTest.AssertEqual([0], PillowCImageExtrema(empty, 1))
    } finally {
        if empty
            PillowCFreeImage(empty)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image get_extrema matches Pillow band extrema", PillowCTestImageGetExtremaMatchesPillowBands)

PillowCTestImageGetBboxMatchesPillowModes(*) {
    l := PillowCCreateImageMode(4, 3, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    rgba := PillowCCreateImageMode(3, 1, 4)
    empty := PillowCCreateImageMode(3, 2, 1)
    zeroWidth := 0
    try {
        PillowCImageSetBytes(l, [
            0, 0, 0, 0,
            0, 5, 0, 0,
            0, 0, 7, 0,
        ])
        PillowCImageSetBytes(rgb, [
            0, 0, 0, 1, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 2, 0, 0, 0, 3,
        ])
        PillowCImageSetBytes(rgba, [
            0, 0, 0, 0,
            9, 0, 0, 0,
            0, 0, 0, 5,
        ])
        PillowCImageSetBytes(empty, [0, 0, 0, 0, 0, 0])
        zeroWidth := PillowCImageCrop(l, 0, 0, 0, 1)

        AhkTest.AssertEqual([1, 1, 3, 3], PillowCImageGetBbox(l))
        AhkTest.AssertEqual([1, 0, 3, 2], PillowCImageGetBbox(rgb))
        AhkTest.AssertEqual([2, 0, 3, 1], PillowCImageGetBbox(rgba))
        AhkTest.AssertEqual([1, 0, 3, 1], PillowCImageGetBbox(rgba, false))
        AhkTest.AssertEqual(0, PillowCImageGetBbox(empty))
        AhkTest.AssertEqual(0, PillowCImageGetBbox(zeroWidth))
    } finally {
        if zeroWidth
            PillowCFreeImage(zeroWidth)
        PillowCFreeImage(empty)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image getbbox matches Pillow modes and alpha_only", PillowCTestImageGetBboxMatchesPillowModes)

PillowCTestImageGetProjectionMatchesPillowModes(*) {
    l := PillowCCreateImageMode(4, 3, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    rgba := PillowCCreateImageMode(3, 1, 4)
    empty := PillowCCreateImageMode(3, 2, 1)
    zeroWidth := 0
    try {
        PillowCImageSetBytes(l, [
            0, 0, 0, 0,
            0, 5, 0, 0,
            0, 0, 7, 0,
        ])
        PillowCImageSetBytes(rgb, [
            0, 0, 0, 1, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 2, 0, 0, 0, 3,
        ])
        PillowCImageSetBytes(rgba, [
            0, 0, 0, 0,
            9, 0, 0, 0,
            0, 0, 0, 5,
        ])
        PillowCImageSetBytes(empty, [0, 0, 0, 0, 0, 0])
        zeroWidth := PillowCImageCrop(l, 0, 0, 0, 2)

        AhkTest.AssertEqual([[0, 1, 1, 0], [0, 1, 1]], PillowCImageGetProjection(l, 4, 3))
        AhkTest.AssertEqual([[0, 1, 1], [1, 1]], PillowCImageGetProjection(rgb, 3, 2))
        AhkTest.AssertEqual([[0, 1, 1], [1]], PillowCImageGetProjection(rgba, 3, 1))
        AhkTest.AssertEqual([[0, 0, 0], [0, 0]], PillowCImageGetProjection(empty, 3, 2))
        AhkTest.AssertEqual([[], [0, 0]], PillowCImageGetProjection(zeroWidth, 0, 2))
    } finally {
        if zeroWidth
            PillowCFreeImage(zeroWidth)
        PillowCFreeImage(empty)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image getprojection matches Pillow modes", PillowCTestImageGetProjectionMatchesPillowModes)

PillowCTestImageGetColorsMatchesPillowModes(*) {
    l := PillowCCreateImageMode(5, 1, 1)
    rgb := PillowCCreateImageMode(4, 1, 3)
    rgba := PillowCCreateImageMode(4, 1, 4)
    empty := PillowCCreateImageMode(1, 1, 1)
    emptyCrop := 0
    try {
        PillowCImageSetBytes(l, [0, 7, 7, 255, 0])
        PillowCImageSetBytes(rgb, [
            1, 2, 3,
            1, 2, 3,
            4, 5, 6,
            0, 0, 0,
        ])
        PillowCImageSetBytes(rgba, [
            1, 2, 3, 4,
            1, 2, 3, 4,
            1, 2, 3, 0,
            0, 0, 0, 0,
        ])
        emptyCrop := PillowCImageCrop(empty, 0, 0, 0, 1)

        lColors := PillowCImageGetColors(l, 256, 1)
        rgbColors := PillowCImageGetColors(rgb, 256, 3)
        rgbaColors := PillowCImageGetColors(rgba, 256, 4)

        AhkTest.AssertEqual(3, lColors.Length)
        AhkTest.AssertEqual([2, [0]], PillowCFindColor(lColors, [0]))
        AhkTest.AssertEqual([2, [7]], PillowCFindColor(lColors, [7]))
        AhkTest.AssertEqual([1, [255]], PillowCFindColor(lColors, [255]))
        AhkTest.AssertEqual(3, rgbColors.Length)
        AhkTest.AssertEqual([2, [1, 2, 3]], PillowCFindColor(rgbColors, [1, 2, 3]))
        AhkTest.AssertEqual([1, [4, 5, 6]], PillowCFindColor(rgbColors, [4, 5, 6]))
        AhkTest.AssertEqual([1, [0, 0, 0]], PillowCFindColor(rgbColors, [0, 0, 0]))
        AhkTest.AssertEqual(3, rgbaColors.Length)
        AhkTest.AssertEqual([2, [1, 2, 3, 4]], PillowCFindColor(rgbaColors, [1, 2, 3, 4]))
        AhkTest.AssertEqual([1, [1, 2, 3, 0]], PillowCFindColor(rgbaColors, [1, 2, 3, 0]))
        AhkTest.AssertEqual([1, [0, 0, 0, 0]], PillowCFindColor(rgbaColors, [0, 0, 0, 0]))
        AhkTest.AssertEqual(0, PillowCImageGetColors(rgb, 2, 3))
        AhkTest.AssertEqual([], PillowCImageGetColors(emptyCrop, 0, 1))
        AhkTest.AssertEqual(0, PillowCImageGetColors(emptyCrop, -1, 1))
    } finally {
        if emptyCrop
            PillowCFreeImage(emptyCrop)
        PillowCFreeImage(empty)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image getcolors matches Pillow modes and maxcolors", PillowCTestImageGetColorsMatchesPillowModes)

PillowCTestImageAutocontrastMatchesPillowLAndRgb(*) {
    l := PillowCCreateImageMode(4, 1, 1)
    rgb := PillowCCreateImageMode(3, 1, 3)
    lOut := 0
    rgbOut := 0
    try {
        PillowCImageSetBytes(l, [10, 20, 30, 40])
        PillowCImageSetBytes(rgb, [
            10, 50, 100,
            20, 60, 150,
            30, 70, 200,
        ])
        lOut := PillowCImageAutocontrast(l)
        rgbOut := PillowCImageAutocontrast(rgb)

        AhkTest.AssertEqual(1, PillowCImageMode(lOut))
        AhkTest.AssertEqual(3, PillowCImageMode(rgbOut))
        AhkTest.AssertEqual([0, 85, 170, 255], PillowCImageToArray(lOut, 4))
        AhkTest.AssertEqual([0, 0, 0, 127, 127, 127, 255, 255, 254], PillowCImageToArray(rgbOut, 9))
    } finally {
        if rgbOut
            PillowCFreeImage(rgbOut)
        if lOut
            PillowCFreeImage(lOut)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image autocontrast matches Pillow for L and RGB", PillowCTestImageAutocontrastMatchesPillowLAndRgb)

PillowCTestImageAutocontrastAppliesCutoffAndIgnore(*) {
    cutoff := PillowCCreateImageMode(10, 1, 1)
    ignoreScalar := PillowCCreateImageMode(5, 1, 1)
    ignoreList := PillowCCreateImageMode(5, 1, 1)
    cutoffOut := 0
    scalarOut := 0
    listOut := 0
    try {
        PillowCImageSetBytes(cutoff, [0, 0, 10, 20, 30, 40, 50, 60, 255, 255])
        PillowCImageSetBytes(ignoreScalar, [0, 10, 20, 30, 255])
        PillowCImageSetBytes(ignoreList, [0, 10, 20, 30, 255])

        ignoreZero := PillowCBuffer([0])
        ignoreEdges := PillowCBuffer([0, 255])
        cutoffOut := PillowCImageAutocontrast(cutoff, 20.0, 20.0)
        scalarOut := PillowCImageAutocontrast(ignoreScalar, 0.0, 0.0, 1, ignoreZero)
        listOut := PillowCImageAutocontrast(ignoreList, 0.0, 0.0, 2, ignoreEdges)

        AhkTest.AssertEqual([0, 0, 0, 51, 102, 153, 203, 255, 255, 255], PillowCImageToArray(cutoffOut, 10))
        AhkTest.AssertEqual([0, 0, 10, 20, 255], PillowCImageToArray(scalarOut, 5))
        AhkTest.AssertEqual([0, 0, 127, 255, 255], PillowCImageToArray(listOut, 5))
    } finally {
        if listOut
            PillowCFreeImage(listOut)
        if scalarOut
            PillowCFreeImage(scalarOut)
        if cutoffOut
            PillowCFreeImage(cutoffOut)
        PillowCFreeImage(ignoreList)
        PillowCFreeImage(ignoreScalar)
        PillowCFreeImage(cutoff)
    }
}

AhkTest.Test("pillow_c image autocontrast applies Pillow cutoff and ignore rules", PillowCTestImageAutocontrastAppliesCutoffAndIgnore)

PillowCTestImageAutocontrastUsesMaskHistogram(*) {
    l := PillowCCreateImageMode(5, 1, 1)
    lMask := PillowCCreateImageMode(5, 1, 1)
    oneMask := PillowCCreateImageMode(5, 1, 5)
    rgb := PillowCCreateImageMode(4, 1, 3)
    rgbMask := PillowCCreateImageMode(4, 1, 1)
    lOut := 0
    lIgnoreOut := 0
    oneOut := 0
    rgbOut := 0
    try {
        PillowCImageSetBytes(l, [0, 10, 20, 30, 255])
        PillowCImageSetBytes(lMask, [0, 255, 255, 0, 0])
        PillowCImageSetRawBytes(oneMask, [0x60], "1")
        PillowCImageSetBytes(rgb, [
            10, 50, 100,
            20, 60, 150,
            30, 70, 200,
            250, 1, 2,
        ])
        PillowCImageSetBytes(rgbMask, [0, 255, 255, 0])

        ignoreTen := PillowCBuffer([10])
        lOut := PillowCImageAutocontrast(l, 0.0, 0.0, 0, 0, lMask)
        lIgnoreOut := PillowCImageAutocontrast(l, 0.0, 0.0, 1, ignoreTen, lMask)
        oneOut := PillowCImageAutocontrast(l, 0.0, 0.0, 0, 0, oneMask)
        rgbOut := PillowCImageAutocontrast(rgb, 0.0, 0.0, 0, 0, rgbMask)

        AhkTest.AssertEqual([0, 0, 255, 255, 255], PillowCImageToArray(lOut, 5))
        AhkTest.AssertEqual([0, 10, 20, 30, 255], PillowCImageToArray(lIgnoreOut, 5))
        AhkTest.AssertEqual([0, 0, 255, 255, 255], PillowCImageToArray(oneOut, 5))
        AhkTest.AssertEqual([0, 0, 0, 0, 0, 0, 255, 255, 254, 255, 0, 0], PillowCImageToArray(rgbOut, 12))
    } finally {
        for handle in [rgbOut, oneOut, lIgnoreOut, lOut, rgbMask, rgb, oneMask, lMask, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image autocontrast uses mask histogram", PillowCTestImageAutocontrastUsesMaskHistogram)

PillowCTestImageAutocontrastMaskIntoReusesTargetStorage(*) {
    source := PillowCCreateImageMode(5, 1, 1)
    mask := PillowCCreateImageMode(5, 1, 1)
    target := PillowCCreateImageMode(5, 1, 1)
    try {
        PillowCImageSetBytes(source, [0, 10, 20, 30, 255])
        PillowCImageSetBytes(mask, [0, 255, 255, 0, 0])
        before := PillowCImageData(target).Ptr
        PillowCImageAutocontrastInto(source, 0.0, 0.0, target, 0, 0, mask)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([0, 0, 255, 255, 255], PillowCImageToArray(target, 5))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(mask)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image autocontrast mask_into reuses target storage", PillowCTestImageAutocontrastMaskIntoReusesTargetStorage)

PillowCTestImageAutocontrastPreserveToneMatchesPillowRgbAndL(*) {
    l := PillowCCreateImageMode(5, 1, 1)
    rgb := PillowCCreateImageMode(4, 1, 3)
    lOut := 0
    rgbOut := 0
    try {
        PillowCImageSetBytes(l, [0, 10, 20, 30, 255])
        PillowCImageSetBytes(rgb, [
            10, 50, 100,
            20, 60, 150,
            30, 70, 200,
            250, 1, 2,
        ])
        lOut := PillowCImageAutocontrast(l, 0.0, 0.0, 0, 0, 0, true)
        rgbOut := PillowCImageAutocontrast(rgb, 0.0, 0.0, 0, 0, 0, true)

        AhkTest.AssertEqual([0, 10, 20, 30, 255], PillowCImageToArray(lOut, 5))
        AhkTest.AssertEqual([0, 47, 255, 0, 127, 255, 0, 207, 255, 255, 0, 0], PillowCImageToArray(rgbOut, 12))
    } finally {
        for handle in [rgbOut, lOut, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image autocontrast preserve_tone matches Pillow RGB and L", PillowCTestImageAutocontrastPreserveToneMatchesPillowRgbAndL)

PillowCTestImageAutocontrastPreserveToneUsesMaskHistogram(*) {
    rgb := PillowCCreateImageMode(4, 1, 3)
    mask := PillowCCreateImageMode(4, 1, 1)
    out := 0
    try {
        PillowCImageSetBytes(rgb, [
            10, 50, 100,
            20, 60, 150,
            30, 70, 200,
            250, 1, 2,
        ])
        PillowCImageSetBytes(mask, [0, 255, 255, 0])
        out := PillowCImageAutocontrast(rgb, 0.0, 0.0, 0, 0, mask, true)

        AhkTest.AssertEqual([0, 0, 255, 0, 34, 255, 0, 204, 255, 255, 0, 0], PillowCImageToArray(out, 12))
    } finally {
        for handle in [out, mask, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image autocontrast preserve_tone uses mask histogram", PillowCTestImageAutocontrastPreserveToneUsesMaskHistogram)

PillowCTestImageAutocontrastPreserveToneRejectsUnsupportedModes(*) {
    rgba := PillowCCreateImageMode(1, 1, 4)
    outHandle := 0
    try {
        PillowCImageSetBytes(rgba, [10, 20, 30, 40])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_autocontrast",
            "Ptr", rgba,
            "Double", 0.0,
            "Double", 0.0,
            "Ptr", 0,
            "UPtr", 0,
            "Ptr", 0,
            "Int", true,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(rgba)
    }
}

AhkTest.Test("pillow_c image autocontrast preserve_tone rejects unsupported modes", PillowCTestImageAutocontrastPreserveToneRejectsUnsupportedModes)

PillowCTestImageAutocontrastIntoReusesTargetHandleStorage(*) {
    source := PillowCCreateImageMode(4, 1, 1)
    target := PillowCCreateImageMode(4, 1, 1)
    try {
        PillowCImageSetBytes(source, [10, 20, 30, 40])
        before := PillowCImageData(target).Ptr
        PillowCImageAutocontrastInto(source, 0.0, 0.0, target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([0, 85, 170, 255], PillowCImageToArray(target, 4))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image autocontrast_into reuses target handle storage", PillowCTestImageAutocontrastIntoReusesTargetHandleStorage)

PillowCTestImageAutocontrastRejectsUnsupportedModes(*) {
    rgba := PillowCCreateImageMode(1, 1, 4)
    outHandle := 0
    try {
        PillowCImageSetBytes(rgba, [10, 20, 30, 40])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_autocontrast",
            "Ptr", rgba,
            "Double", 0.0,
            "Double", 0.0,
            "Ptr", 0,
            "UPtr", 0,
            "Ptr", 0,
            "Int", false,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(rgba)
    }
}

AhkTest.Test("pillow_c image autocontrast rejects modes Pillow ImageOps._lut does not support", PillowCTestImageAutocontrastRejectsUnsupportedModes)

PillowCTestImageGetChannelReturnsLImage(*) {
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    la := PillowCCreateImageMode(2, 1, 2)
    r := 0
    g := 0
    a := 0
    laL := 0
    laA := 0
    try {
        PillowCImageSetBytes(rgb, [1, 2, 3, 10, 20, 30])
        PillowCImageSetBytes(rgba, [1, 2, 3, 4, 10, 20, 30, 40])
        PillowCImageSetBytes(la, [11, 12, 21, 22])
        r := PillowCImageGetChannel(rgb, 0)
        g := PillowCImageGetChannel(rgb, 1)
        a := PillowCImageGetChannel(rgba, 3)
        laL := PillowCImageGetChannel(la, 0)
        laA := PillowCImageGetChannel(la, 1)
        AhkTest.AssertEqual(1, PillowCImageMode(r))
        AhkTest.AssertEqual(1, PillowCImageInt(r, "pillow_c_image_channels"))
        AhkTest.AssertEqual([2, 1], [PillowCImageInt(r, "pillow_c_image_width"), PillowCImageInt(r, "pillow_c_image_height")])
        AhkTest.AssertEqual([1, 10], PillowCImageToArray(r, 2))
        AhkTest.AssertEqual([2, 20], PillowCImageToArray(g, 2))
        AhkTest.AssertEqual([4, 40], PillowCImageToArray(a, 2))
        AhkTest.AssertEqual([11, 21], PillowCImageToArray(laL, 2))
        AhkTest.AssertEqual([12, 22], PillowCImageToArray(laA, 2))
    } finally {
        if laA
            PillowCFreeImage(laA)
        if laL
            PillowCFreeImage(laL)
        if a
            PillowCFreeImage(a)
        if g
            PillowCFreeImage(g)
        if r
            PillowCFreeImage(r)
        PillowCFreeImage(la)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
    }
}

AhkTest.Test("pillow_c image get_channel returns an L image for one channel", PillowCTestImageGetChannelReturnsLImage)

PillowCTestImageGetChannelPreservesPaletteMode(*) {
    palette := [0, 0, 0, 10, 20, 30, 200, 100, 50, 255, 255, 255]
    source := PillowCCreateImageMode(4, 2, 6)
    target := PillowCCreateImageMode(4, 2, 6)
    channel := 0
    channelRgb := 0
    targetRgb := 0
    try {
        PillowCImageSetBytes(source, [0, 1, 2, 3, 3, 2, 1, 0])
        PillowCImagePutPaletteRgb(source, palette)

        channel := PillowCImageGetChannel(source, 0)
        before := PillowCImageData(target).Ptr
        PillowCImageGetChannelInto(source, 0, target)
        after := PillowCImageData(target).Ptr

        AhkTest.AssertEqual(6, PillowCImageMode(channel))
        AhkTest.AssertEqual(1, PillowCImageInt(channel, "pillow_c_image_channels"))
        AhkTest.AssertEqual([4, 2], [PillowCImageInt(channel, "pillow_c_image_width"), PillowCImageInt(channel, "pillow_c_image_height")])
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([0, 1, 2, 3, 3, 2, 1, 0], PillowCImageToArray(channel, 8))
        AhkTest.AssertEqual([0, 1, 2, 3, 3, 2, 1, 0], PillowCImageToArray(target, 8))
        AhkTest.AssertEqual(palette, PillowCImageGetPaletteRgb(channel))
        AhkTest.AssertEqual(palette, PillowCImageGetPaletteRgb(target))

        channelRgb := PillowCImageConvertMode(channel, 3)
        targetRgb := PillowCImageConvertMode(target, 3)
        AhkTest.AssertEqual([0, 0, 0, 10, 20, 30, 200, 100, 50, 255, 255, 255, 255, 255, 255, 200, 100, 50, 10, 20, 30, 0, 0, 0], PillowCImageToArray(channelRgb, 24))
        AhkTest.AssertEqual([0, 0, 0, 10, 20, 30, 200, 100, 50, 255, 255, 255, 255, 255, 255, 200, 100, 50, 10, 20, 30, 0, 0, 0], PillowCImageToArray(targetRgb, 24))
    } finally {
        if targetRgb
            PillowCFreeImage(targetRgb)
        if channelRgb
            PillowCFreeImage(channelRgb)
        if channel
            PillowCFreeImage(channel)
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image get_channel preserves P mode and palette", PillowCTestImageGetChannelPreservesPaletteMode)

PillowCTestImageGetChannelIntoReusesTargetHandle(*) {
    source := PillowCCreateImageMode(2, 1, 4)
    target := PillowCCreateImageMode(2, 1, 1)
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4, 10, 20, 30, 40])
        before := PillowCImageData(target).Ptr
        PillowCImageGetChannelInto(source, 3, target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([4, 40], PillowCImageToArray(target, 2))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image get_channel_into reuses target handle storage", PillowCTestImageGetChannelIntoReusesTargetHandle)

PillowCTestImageGetChannelRejectsOutOfRangeIndex(*) {
    source := PillowCCreateImageMode(2, 1, 3)
    outHandle := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_get_channel",
            "Ptr", source,
            "Int", 3,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image get_channel rejects out-of-range channel indexes", PillowCTestImageGetChannelRejectsOutOfRangeIndex)

PillowCTestImageSplitBandsReturnsAllLBandsInOneCall(*) {
    l := PillowCCreateImageMode(3, 1, 1)
    la := PillowCCreateImageMode(2, 1, 2)
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    lBands := []
    laBands := []
    rgbBands := []
    rgbaBands := []
    try {
        PillowCImageSetBytes(l, [1, 2, 3])
        PillowCImageSetBytes(la, [1, 2, 10, 20])
        PillowCImageSetBytes(rgb, [1, 2, 3, 10, 20, 30])
        PillowCImageSetBytes(rgba, [1, 2, 3, 4, 10, 20, 30, 40])
        lBands := PillowCImageSplitBands(l, 1)
        laBands := PillowCImageSplitBands(la, 2)
        rgbBands := PillowCImageSplitBands(rgb, 3)
        rgbaBands := PillowCImageSplitBands(rgba, 4)

        AhkTest.AssertEqual(1, PillowCImageMode(lBands[1]))
        AhkTest.AssertEqual([1, 2, 3], PillowCImageToArray(lBands[1], 3))
        AhkTest.AssertEqual([1, 10], PillowCImageToArray(laBands[1], 2))
        AhkTest.AssertEqual([2, 20], PillowCImageToArray(laBands[2], 2))
        AhkTest.AssertEqual([1, 10], PillowCImageToArray(rgbBands[1], 2))
        AhkTest.AssertEqual([2, 20], PillowCImageToArray(rgbBands[2], 2))
        AhkTest.AssertEqual([3, 30], PillowCImageToArray(rgbBands[3], 2))
        AhkTest.AssertEqual([1, 10], PillowCImageToArray(rgbaBands[1], 2))
        AhkTest.AssertEqual([2, 20], PillowCImageToArray(rgbaBands[2], 2))
        AhkTest.AssertEqual([3, 30], PillowCImageToArray(rgbaBands[3], 2))
        AhkTest.AssertEqual([4, 40], PillowCImageToArray(rgbaBands[4], 2))
    } finally {
        for handle in rgbaBands
            PillowCFreeImage(handle)
        for handle in rgbBands
            PillowCFreeImage(handle)
        for handle in laBands
            PillowCFreeImage(handle)
        for handle in lBands
            PillowCFreeImage(handle)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(la)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image split_bands returns all L bands in one call", PillowCTestImageSplitBandsReturnsAllLBandsInOneCall)

PillowCTestImageSplitBandsPreservesPaletteMode(*) {
    palette := [0, 0, 0, 10, 20, 30, 200, 100, 50, 255, 255, 255]
    source := PillowCCreateImageMode(4, 2, 6)
    bands := []
    bandRgb := 0
    try {
        PillowCImageSetBytes(source, [0, 1, 2, 3, 3, 2, 1, 0])
        PillowCImagePutPaletteRgb(source, palette)

        bands := PillowCImageSplitBands(source, 1)
        AhkTest.AssertEqual(1, bands.Length)
        AhkTest.AssertEqual(6, PillowCImageMode(bands[1]))
        AhkTest.AssertEqual([0, 1, 2, 3, 3, 2, 1, 0], PillowCImageToArray(bands[1], 8))
        AhkTest.AssertEqual(palette, PillowCImageGetPaletteRgb(bands[1]))

        bandRgb := PillowCImageConvertMode(bands[1], 3)
        AhkTest.AssertEqual([0, 0, 0, 10, 20, 30, 200, 100, 50, 255, 255, 255, 255, 255, 255, 200, 100, 50, 10, 20, 30, 0, 0, 0], PillowCImageToArray(bandRgb, 24))
    } finally {
        if bandRgb
            PillowCFreeImage(bandRgb)
        for handle in bands
            PillowCFreeImage(handle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image split_bands preserves P mode and palette", PillowCTestImageSplitBandsPreservesPaletteMode)

PillowCTestImageSplitBandsRejectsWrongOutputCount(*) {
    source := PillowCCreateImageMode(2, 1, 3)
    outHandles := Buffer(2 * A_PtrSize, 0)
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_split_bands",
            "Ptr", source,
            "Ptr", outHandles,
            "UPtr", 2,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, NumGet(outHandles, 0, "Ptr"))
        AhkTest.AssertEqual(0, NumGet(outHandles, A_PtrSize, "Ptr"))
    } finally {
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image split_bands rejects wrong output count", PillowCTestImageSplitBandsRejectsWrongOutputCount)

PillowCTestImageMergeBandsInterleavesLBandsIntoTargetMode(*) {
    r := PillowCCreateImageMode(2, 1, 1)
    g := PillowCCreateImageMode(2, 1, 1)
    b := PillowCCreateImageMode(2, 1, 1)
    a := PillowCCreateImageMode(2, 1, 1)
    la := 0
    rgb := 0
    rgba := 0
    try {
        PillowCImageSetBytes(r, [1, 2])
        PillowCImageSetBytes(g, [3, 4])
        PillowCImageSetBytes(b, [5, 6])
        PillowCImageSetBytes(a, [7, 8])
        la := PillowCImageMergeBands(2, [r, a])
        rgb := PillowCImageMergeBands(3, [r, g, b])
        rgba := PillowCImageMergeBands(4, [r, g, b, a])
        AhkTest.AssertEqual(2, PillowCImageMode(la))
        AhkTest.AssertEqual([1, 7, 2, 8], PillowCImageToArray(la, 4))
        AhkTest.AssertEqual(3, PillowCImageMode(rgb))
        AhkTest.AssertEqual([2, 1], [PillowCImageInt(rgb, "pillow_c_image_width"), PillowCImageInt(rgb, "pillow_c_image_height")])
        AhkTest.AssertEqual([1, 3, 5, 2, 4, 6], PillowCImageToArray(rgb, 6))
        AhkTest.AssertEqual(4, PillowCImageMode(rgba))
        AhkTest.AssertEqual([1, 3, 5, 7, 2, 4, 6, 8], PillowCImageToArray(rgba, 8))
    } finally {
        if rgba
            PillowCFreeImage(rgba)
        if rgb
            PillowCFreeImage(rgb)
        if la
            PillowCFreeImage(la)
        PillowCFreeImage(a)
        PillowCFreeImage(b)
        PillowCFreeImage(g)
        PillowCFreeImage(r)
    }
}

AhkTest.Test("pillow_c image merge_bands interleaves L bands into Pillow modes", PillowCTestImageMergeBandsInterleavesLBandsIntoTargetMode)

PillowCTestImageMergeBandsIntoReusesTargetHandle(*) {
    r := PillowCCreateImageMode(2, 1, 1)
    g := PillowCCreateImageMode(2, 1, 1)
    b := PillowCCreateImageMode(2, 1, 1)
    target := PillowCCreateImageMode(2, 1, 3)
    try {
        PillowCImageSetBytes(r, [10, 20])
        PillowCImageSetBytes(g, [30, 40])
        PillowCImageSetBytes(b, [50, 60])
        before := PillowCImageData(target).Ptr
        PillowCImageMergeBandsInto(3, [r, g, b], target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([10, 30, 50, 20, 40, 60], PillowCImageToArray(target, 6))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(b)
        PillowCFreeImage(g)
        PillowCFreeImage(r)
    }
}

AhkTest.Test("pillow_c image merge_bands_into reuses target handle storage", PillowCTestImageMergeBandsIntoReusesTargetHandle)

PillowCTestImageMergeBandsRejectsWrongBandShapeOrMode(*) {
    r := PillowCCreateImageMode(2, 1, 1)
    g := PillowCCreateImageMode(2, 1, 1)
    rgbBand := PillowCCreateImageMode(2, 1, 3)
    wrongSize := PillowCCreateImageMode(3, 1, 1)
    outHandle := 0
    try {
        bands := PillowCImageHandleArray([r, g])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_merge_bands",
            "Int", 3,
            "Ptr", bands,
            "UPtr", 2,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        bands := PillowCImageHandleArray([rgbBand, g, r])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_merge_bands",
            "Int", 3,
            "Ptr", bands,
            "UPtr", 3,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        bands := PillowCImageHandleArray([r, g, wrongSize])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_merge_bands",
            "Int", 3,
            "Ptr", bands,
            "UPtr", 3,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(wrongSize)
        PillowCFreeImage(rgbBand)
        PillowCFreeImage(g)
        PillowCFreeImage(r)
    }
}

AhkTest.Test("pillow_c image merge_bands rejects wrong band count mode or size", PillowCTestImageMergeBandsRejectsWrongBandShapeOrMode)

PillowCTestImagePutAlphaValueReturnsRgba(*) {
    l := PillowCCreateImageMode(2, 1, 1)
    la := PillowCCreateImageMode(2, 1, 2)
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    lOut := 0
    laOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l, [1, 10])
        PillowCImageSetBytes(la, [1, 2, 10, 20])
        PillowCImageSetBytes(rgb, [1, 2, 3, 10, 20, 30])
        PillowCImageSetBytes(rgba, [1, 2, 3, 4, 10, 20, 30, 40])
        lOut := PillowCImagePutAlphaValue(l, 6)
        laOut := PillowCImagePutAlphaValue(la, 7)
        rgbOut := PillowCImagePutAlphaValue(rgb, 7)
        rgbaOut := PillowCImagePutAlphaValue(rgba, 8)
        AhkTest.AssertEqual(2, PillowCImageMode(lOut))
        AhkTest.AssertEqual([1, 6, 10, 6], PillowCImageToArray(lOut, 4))
        AhkTest.AssertEqual(2, PillowCImageMode(laOut))
        AhkTest.AssertEqual([1, 7, 10, 7], PillowCImageToArray(laOut, 4))
        AhkTest.AssertEqual(4, PillowCImageMode(rgbOut))
        AhkTest.AssertEqual([1, 2, 3, 7, 10, 20, 30, 7], PillowCImageToArray(rgbOut, 8))
        AhkTest.AssertEqual([1, 2, 3, 8, 10, 20, 30, 8], PillowCImageToArray(rgbaOut, 8))
    } finally {
        if rgbaOut
            PillowCFreeImage(rgbaOut)
        if rgbOut
            PillowCFreeImage(rgbOut)
        if laOut
            PillowCFreeImage(laOut)
        if lOut
            PillowCFreeImage(lOut)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(la)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image put_alpha_value returns LA or RGBA by source mode", PillowCTestImagePutAlphaValueReturnsRgba)

PillowCTestImagePutAlphaImageReturnsRgba(*) {
    l := PillowCCreateImageMode(2, 1, 1)
    la := PillowCCreateImageMode(2, 1, 2)
    rgb := PillowCCreateImageMode(2, 1, 3)
    alpha := PillowCCreateImageMode(2, 1, 1)
    lOut := 0
    laOut := 0
    out := 0
    try {
        PillowCImageSetBytes(l, [1, 10])
        PillowCImageSetBytes(la, [1, 2, 10, 20])
        PillowCImageSetBytes(rgb, [1, 2, 3, 10, 20, 30])
        PillowCImageSetBytes(alpha, [50, 60])
        lOut := PillowCImagePutAlphaImage(l, alpha)
        laOut := PillowCImagePutAlphaImage(la, alpha)
        out := PillowCImagePutAlphaImage(rgb, alpha)
        AhkTest.AssertEqual(2, PillowCImageMode(lOut))
        AhkTest.AssertEqual([1, 50, 10, 60], PillowCImageToArray(lOut, 4))
        AhkTest.AssertEqual(2, PillowCImageMode(laOut))
        AhkTest.AssertEqual([1, 50, 10, 60], PillowCImageToArray(laOut, 4))
        AhkTest.AssertEqual(4, PillowCImageMode(out))
        AhkTest.AssertEqual([1, 2, 3, 50, 10, 20, 30, 60], PillowCImageToArray(out, 8))
    } finally {
        if out
            PillowCFreeImage(out)
        if laOut
            PillowCFreeImage(laOut)
        if lOut
            PillowCFreeImage(lOut)
        PillowCFreeImage(alpha)
        PillowCFreeImage(rgb)
        PillowCFreeImage(la)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image put_alpha_image accepts an L alpha image", PillowCTestImagePutAlphaImageReturnsRgba)

PillowCTestImageConvertModeCoversCorePillowModes(*) {
    l := PillowCCreateImageMode(3, 1, 1)
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    lToRgb := 0
    lToRgba := 0
    rgbaToRgb := 0
    rgbaToL := 0
    try {
        PillowCImageSetBytes(l, [0, 10, 255])
        PillowCImageSetBytes(rgb, [10, 20, 30, 200, 100, 50])
        PillowCImageSetBytes(rgba, [10, 20, 30, 40, 200, 100, 50, 128])
        lToRgb := PillowCImageConvertMode(l, 3)
        lToRgba := PillowCImageConvertMode(l, 4)
        rgbaToRgb := PillowCImageConvertMode(rgba, 3)
        rgbaToL := PillowCImageConvertMode(rgba, 1)
        AhkTest.AssertEqual([0, 0, 0, 10, 10, 10, 255, 255, 255], PillowCImageToArray(lToRgb, 9))
        AhkTest.AssertEqual([0, 0, 0, 255, 10, 10, 10, 255, 255, 255, 255, 255], PillowCImageToArray(lToRgba, 12))
        AhkTest.AssertEqual([10, 20, 30, 200, 100, 50], PillowCImageToArray(rgbaToRgb, 6))
        AhkTest.AssertEqual([18, 124], PillowCImageToArray(rgbaToL, 2))
    } finally {
        if rgbaToL
            PillowCFreeImage(rgbaToL)
        if rgbaToRgb
            PillowCFreeImage(rgbaToRgb)
        if lToRgba
            PillowCFreeImage(lToRgba)
        if lToRgb
            PillowCFreeImage(lToRgb)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image convert_mode covers core Pillow modes", PillowCTestImageConvertModeCoversCorePillowModes)

PillowCTestImageConvertModeCoversLAMode(*) {
    l := PillowCCreateImageMode(3, 2, 1)
    la := PillowCCreateImageMode(3, 2, 2)
    rgb := PillowCCreateImageMode(3, 2, 3)
    rgba := PillowCCreateImageMode(3, 2, 4)
    outputs := []
    try {
        PillowCImageSetBytes(l, [10, 80, 130, 200, 20, 250])
        PillowCImageSetBytes(la, [10, 40, 80, 70, 130, 100, 200, 130, 20, 160, 250, 190])
        PillowCImageSetBytes(rgb, [
            10, 20, 30, 80, 40, 10, 130, 140, 150,
            200, 190, 180, 20, 120, 220, 250, 240, 10,
        ])
        PillowCImageSetBytes(rgba, [
            10, 20, 30, 40, 80, 40, 10, 70, 130, 140, 150, 100,
            200, 190, 180, 130, 20, 120, 220, 160, 250, 240, 10, 190,
        ])

        cases := [
            { Source: la, Mode: 1, Size: 6, Bytes: [10, 80, 130, 200, 20, 250] },
            { Source: la, Mode: 2, Size: 12, Bytes: [10, 40, 80, 70, 130, 100, 200, 130, 20, 160, 250, 190] },
            { Source: la, Mode: 3, Size: 18, Bytes: [
                10, 10, 10, 80, 80, 80, 130, 130, 130,
                200, 200, 200, 20, 20, 20, 250, 250, 250,
            ] },
            { Source: la, Mode: 4, Size: 24, Bytes: [
                10, 10, 10, 40, 80, 80, 80, 70, 130, 130, 130, 100,
                200, 200, 200, 130, 20, 20, 20, 160, 250, 250, 250, 190,
            ] },
            { Source: l, Mode: 2, Size: 12, Bytes: [10, 255, 80, 255, 130, 255, 200, 255, 20, 255, 250, 255] },
            { Source: rgb, Mode: 2, Size: 12, Bytes: [18, 255, 49, 255, 138, 255, 192, 255, 102, 255, 217, 255] },
            { Source: rgba, Mode: 2, Size: 12, Bytes: [18, 40, 49, 70, 138, 100, 192, 130, 102, 160, 217, 190] },
        ]
        for item in cases {
            out := PillowCImageConvertMode(item.Source, item.Mode)
            outputs.Push(out)
            AhkTest.AssertEqual(item.Mode, PillowCImageMode(out))
            AhkTest.AssertEqual(item.Bytes, PillowCImageToArray(out, item.Size))
        }
    } finally {
        for handle in outputs
            PillowCFreeImage(handle)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(la)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image convert_mode covers Pillow LA mode", PillowCTestImageConvertModeCoversLAMode)

PillowCTestImageConvertModeCoversModeOneSource(*) {
    source := PillowCCreateImageMode(4, 1, 5)
    outputs := []
    try {
        PillowCImageSetRawBytes(source, [0x50], "1")
        cases := [
            { Mode: 1, Size: 4, Bytes: [0, 255, 0, 255] },
            { Mode: 2, Size: 8, Bytes: [0, 255, 255, 255, 0, 255, 255, 255] },
            { Mode: 3, Size: 12, Bytes: [
                0, 0, 0, 255, 255, 255, 0, 0, 0, 255, 255, 255,
            ] },
            { Mode: 4, Size: 16, Bytes: [
                0, 0, 0, 255, 255, 255, 255, 255,
                0, 0, 0, 255, 255, 255, 255, 255,
            ] },
        ]
        for item in cases {
            out := PillowCImageConvertMode(source, item.Mode)
            outputs.Push(out)
            AhkTest.AssertEqual(item.Mode, PillowCImageMode(out))
            AhkTest.AssertEqual(item.Bytes, PillowCImageToArray(out, item.Size))
        }
    } finally {
        for handle in outputs
            PillowCFreeImage(handle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image convert_mode covers mode 1 source", PillowCTestImageConvertModeCoversModeOneSource)

PillowCTestImageConvertModeCoversCmykMode(*) {
    cmyk := PillowCCreateImageMode(8, 1, 7)
    rgb := PillowCCreateImageMode(4, 1, 3)
    rgba := PillowCCreateImageMode(4, 1, 4)
    l := PillowCCreateImageMode(4, 1, 1)
    la := PillowCCreateImageMode(4, 1, 2)
    one := PillowCCreateImageMode(4, 1, 5)
    p := PillowCCreateImageMode(4, 1, 6)
    outputs := []
    try {
        PillowCImageSetBytes(cmyk, [
            0, 0, 0, 0,
            0, 0, 0, 255,
            255, 0, 0, 0,
            0, 255, 0, 0,
            0, 0, 255, 0,
            10, 20, 30, 40,
            200, 100, 50, 128,
            255, 255, 255, 255,
        ])
        PillowCImageSetBytes(rgb, [0, 0, 0, 255, 255, 255, 10, 20, 30, 200, 100, 50])
        PillowCImageSetBytes(rgba, [0, 0, 0, 0, 255, 255, 255, 128, 10, 20, 30, 255, 200, 100, 50, 64])
        PillowCImageSetBytes(l, [0, 10, 128, 255])
        PillowCImageSetBytes(la, [0, 0, 10, 64, 128, 128, 255, 255])
        PillowCImageSetRawBytes(one, [0x50], "1")
        PillowCImageSetBytes(p, [0, 1, 2, 3])
        PillowCImagePutPaletteRgb(p, [
            0, 0, 0,
            10, 20, 30,
            200, 100, 50,
            255, 255, 255,
        ])

        cases := [
            { Source: cmyk, Mode: 3, Size: 24, Bytes: [
                255, 255, 255, 0, 0, 0, 0, 255, 255, 255, 0, 255,
                255, 255, 0, 207, 198, 190, 27, 77, 102, 0, 0, 0,
            ] },
            { Source: cmyk, Mode: 4, Size: 32, Bytes: [
                255, 255, 255, 255, 0, 0, 0, 255, 0, 255, 255, 255, 255, 0, 255, 255,
                255, 255, 0, 255, 207, 198, 190, 255, 27, 77, 102, 255, 0, 0, 0, 255,
            ] },
            { Source: cmyk, Mode: 1, Size: 8, Bytes: [255, 0, 179, 105, 226, 200, 65, 0] },
            { Source: cmyk, Mode: 2, Size: 16, Bytes: [255, 255, 0, 255, 179, 255, 105, 255, 226, 255, 200, 255, 65, 255, 0, 255] },
            { Source: rgb, Mode: 7, Size: 16, Bytes: [255, 255, 255, 0, 0, 0, 0, 0, 245, 235, 225, 0, 55, 155, 205, 0] },
            { Source: rgba, Mode: 7, Size: 16, Bytes: [255, 255, 255, 0, 0, 0, 0, 0, 245, 235, 225, 0, 55, 155, 205, 0] },
            { Source: l, Mode: 7, Size: 16, Bytes: [0, 0, 0, 255, 0, 0, 0, 245, 0, 0, 0, 127, 0, 0, 0, 0] },
            { Source: la, Mode: 7, Size: 16, Bytes: [0, 0, 0, 255, 0, 0, 0, 245, 0, 0, 0, 127, 0, 0, 0, 0] },
            { Source: one, Mode: 7, Size: 16, Bytes: [0, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 255, 0, 0, 0, 0] },
            { Source: p, Mode: 7, Size: 16, Bytes: [255, 255, 255, 0, 245, 235, 225, 0, 55, 155, 205, 0, 0, 0, 0, 0] },
        ]
        for item in cases {
            out := PillowCImageConvertMode(item.Source, item.Mode)
            outputs.Push(out)
            AhkTest.AssertEqual(item.Mode, PillowCImageMode(out))
            AhkTest.AssertEqual(item.Bytes, PillowCImageToArray(out, item.Size))
        }
    } finally {
        for handle in outputs
            PillowCFreeImage(handle)
        PillowCFreeImage(p)
        PillowCFreeImage(one)
        PillowCFreeImage(la)
        PillowCFreeImage(l)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(cmyk)
    }
}

AhkTest.Test("pillow_c image convert_mode covers CMYK mode", PillowCTestImageConvertModeCoversCmykMode)

PillowCTestImageConvertModeDitherNoneCoversModeOneTarget(*) {
    l := PillowCCreateImageMode(8, 1, 1)
    rgb := PillowCCreateImageMode(4, 2, 3)
    rgbThreshold := PillowCCreateImageMode(4, 2, 3)
    la := PillowCCreateImageMode(4, 1, 2)
    rgbaThreshold := PillowCCreateImageMode(4, 2, 4)
    cmyk := PillowCCreateImageMode(8, 2, 7)
    outputs := []
    try {
        PillowCImageSetBytes(l, [0, 32, 64, 96, 128, 160, 192, 255])
        PillowCImageSetBytes(rgb, [
            0, 0, 0, 32, 32, 32, 64, 64, 64, 96, 96, 96,
            128, 128, 128, 160, 160, 160, 192, 192, 192, 224, 224, 224,
        ])
        PillowCImageSetBytes(rgbThreshold, [
            128, 128, 127, 127, 128, 128, 128, 127, 128, 129, 127, 127,
            127, 129, 127, 127, 127, 129, 255, 255, 255, 0, 0, 0,
        ])
        PillowCImageSetBytes(la, [0, 0, 255, 128, 127, 255, 128, 255])
        PillowCImageSetBytes(rgbaThreshold, [
            128, 128, 127, 0, 127, 128, 128, 255, 128, 127, 128, 1, 129, 127, 127, 128,
            127, 129, 127, 255, 127, 127, 129, 0, 255, 255, 255, 64, 0, 0, 0, 255,
        ])
        PillowCImageSetBytes(cmyk, [
            0, 0, 0, 255, 0, 0, 0, 224, 0, 0, 0, 192, 0, 0, 0, 160,
            0, 0, 0, 128, 0, 0, 0, 96, 0, 0, 0, 64, 0, 0, 0, 32,
            0, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 80, 0, 0, 0, 120,
            0, 0, 0, 160, 0, 0, 0, 200, 0, 0, 0, 240, 0, 0, 0, 255,
        ])

        cases := [
            { Source: l, Size: 8, Bytes: [0, 0, 0, 0, 255, 255, 255, 255] },
            { Source: rgb, Size: 8, Bytes: [0, 0, 0, 0, 255, 255, 255, 255] },
            { Source: rgbThreshold, Size: 8, Bytes: [0, 0, 0, 0, 255, 0, 255, 0] },
            { Source: la, Size: 4, Bytes: [0, 255, 0, 255] },
            { Source: rgbaThreshold, Size: 8, Bytes: [0, 0, 0, 0, 255, 0, 255, 0] },
            { Source: cmyk, Size: 16, Bytes: [0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0] },
        ]
        for item in cases {
            out := PillowCImageConvertModeDither(item.Source, 5, 0)
            outputs.Push(out)
            AhkTest.AssertEqual(5, PillowCImageMode(out))
            AhkTest.AssertEqual(item.Bytes, PillowCImageToArray(out, item.Size))
        }
    } finally {
        for handle in outputs
            PillowCFreeImage(handle)
        PillowCFreeImage(cmyk)
        PillowCFreeImage(rgbaThreshold)
        PillowCFreeImage(la)
        PillowCFreeImage(rgbThreshold)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image convert_mode_dither NONE covers mode 1 target", PillowCTestImageConvertModeDitherNoneCoversModeOneTarget)

PillowCTestImageConvertModeDitherFloydSteinbergCoversModeOneTarget(*) {
    l := PillowCCreateImageMode(8, 1, 1)
    rgb := PillowCCreateImageMode(4, 2, 3)
    rgba := PillowCCreateImageMode(4, 2, 4)
    cmyk := PillowCCreateImageMode(8, 2, 7)
    outputs := []
    try {
        PillowCImageSetBytes(l, [0, 32, 64, 96, 128, 160, 192, 255])
        PillowCImageSetBytes(rgb, [
            128, 128, 127, 127, 128, 128, 128, 127, 128, 129, 127, 127,
            127, 129, 127, 127, 127, 129, 255, 255, 255, 0, 0, 0,
        ])
        PillowCImageSetBytes(rgba, [
            0, 0, 0, 0, 80, 80, 80, 255, 140, 140, 140, 1, 255, 255, 255, 128,
            40, 40, 40, 255, 120, 120, 120, 0, 180, 180, 180, 255, 220, 220, 220, 7,
        ])
        PillowCImageSetBytes(cmyk, [
            0, 0, 0, 255, 0, 0, 0, 224, 0, 0, 0, 192, 0, 0, 0, 160,
            0, 0, 0, 128, 0, 0, 0, 96, 0, 0, 0, 64, 0, 0, 0, 32,
            0, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 80, 0, 0, 0, 120,
            0, 0, 0, 160, 0, 0, 0, 200, 0, 0, 0, 240, 0, 0, 0, 255,
        ])

        cases := [
            { Source: l, Size: 8, Bytes: [0, 0, 0, 255, 0, 255, 255, 255] },
            { Source: rgb, Size: 8, Bytes: [0, 255, 0, 255, 255, 0, 255, 0] },
            { Source: rgba, Size: 8, Bytes: [0, 0, 255, 255, 0, 255, 0, 255] },
            { Source: cmyk, Size: 16, Bytes: [0, 0, 0, 0, 255, 0, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0] },
        ]
        for item in cases {
            out := PillowCImageConvertModeDither(item.Source, 5, 3)
            outputs.Push(out)
            AhkTest.AssertEqual(5, PillowCImageMode(out))
            AhkTest.AssertEqual(item.Bytes, PillowCImageToArray(out, item.Size))
        }
    } finally {
        for handle in outputs
            PillowCFreeImage(handle)
        PillowCFreeImage(cmyk)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image convert_mode_dither Floyd-Steinberg covers mode 1 target", PillowCTestImageConvertModeDitherFloydSteinbergCoversModeOneTarget)

PillowCTestImageConvertMatrixMatchesPillowRgbInput(*) {
    source := PillowCCreateImageMode(3, 2, 3)
    lTarget := PillowCCreateImageMode(3, 2, 1)
    rgbTarget := PillowCCreateImageMode(3, 2, 3)
    lOut := 0
    lBiasOut := 0
    rgbOut := 0
    try {
        PillowCImageSetBytes(source, [
            0, 0, 0, 10, 20, 30, 100, 110, 120,
            200, 150, 100, 255, 128, 64, 5, 250, 125,
        ])
        PillowCImageSetBytes(lTarget, [99, 99, 99, 99, 99, 99])
        PillowCImageSetBytes(rgbTarget, [
            9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9,
        ])

        lMatrix := [0.299, 0.587, 0.114, 0]
        lBiasMatrix := [0.5, 0.25, 0.125, 10]
        rgbMatrix := [
            1, 0, 0, 5,
            0, 0.5, 0, 10,
            0, 0, -1, 255,
        ]
        lOut := PillowCImageConvertMatrix(source, 1, lMatrix)
        lBiasOut := PillowCImageConvertMatrix(source, 1, lBiasMatrix)
        rgbOut := PillowCImageConvertMatrix(source, 3, rgbMatrix)
        lBefore := PillowCImageData(lTarget).Ptr
        rgbBefore := PillowCImageData(rgbTarget).Ptr
        PillowCImageConvertMatrixInto(source, 1, lMatrix, lTarget)
        PillowCImageConvertMatrixInto(source, 3, rgbMatrix, rgbTarget)

        AhkTest.AssertEqual(1, PillowCImageMode(lOut))
        AhkTest.AssertEqual([0, 18, 108, 159, 159, 162], PillowCImageToArray(lOut, 6))
        AhkTest.AssertEqual([10, 24, 103, 160, 178, 91], PillowCImageToArray(lBiasOut, 6))
        AhkTest.AssertEqual(3, PillowCImageMode(rgbOut))
        AhkTest.AssertEqual([5, 10, 255, 15, 20, 225, 105, 65, 135, 205, 85, 155, 255, 74, 191, 10, 135, 130], PillowCImageToArray(rgbOut, 18))
        AhkTest.AssertEqual(lBefore, PillowCImageData(lTarget).Ptr)
        AhkTest.AssertEqual([0, 18, 108, 159, 159, 162], PillowCImageToArray(lTarget, 6))
        AhkTest.AssertEqual(rgbBefore, PillowCImageData(rgbTarget).Ptr)
        AhkTest.AssertEqual(PillowCImageToArray(rgbOut, 18), PillowCImageToArray(rgbTarget, 18))
    } finally {
        for handle in [rgbOut, lBiasOut, lOut, rgbTarget, lTarget, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image convert_matrix matches Pillow RGB input", PillowCTestImageConvertMatrixMatchesPillowRgbInput)

PillowCTestImageConvertMatrixRejectsInvalidArguments(*) {
    rgb := PillowCCreateImageMode(1, 1, 3)
    rgba := PillowCCreateImageMode(1, 1, 4)
    l := PillowCCreateImageMode(1, 1, 1)
    wrongTarget := PillowCCreateImageMode(1, 1, 4)
    outHandle := 0
    try {
        matrix4 := PillowCDoubleBuffer([0.299, 0.587, 0.114, 0])
        matrix12 := PillowCDoubleBuffer([
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
        ])
        matrix3 := PillowCDoubleBuffer([1, 2, 3])

        cases := [
            { Source: rgba, Mode: 1, Matrix: matrix4, Count: 4, Status: -3 },
            { Source: l, Mode: 1, Matrix: matrix4, Count: 4, Status: -3 },
            { Source: rgb, Mode: 4, Matrix: matrix4, Count: 4, Status: -3 },
            { Source: rgb, Mode: 1, Matrix: matrix3, Count: 3, Status: -2 },
            { Source: rgb, Mode: 1, Matrix: matrix12, Count: 12, Status: -2 },
            { Source: rgb, Mode: 3, Matrix: matrix4, Count: 4, Status: -2 },
        ]
        for item in cases {
            status := DllCall(
                PillowCDllPath() "\pillow_c_image_convert_matrix",
                "Ptr", item.Source,
                "Int", item.Mode,
                "Ptr", item.Matrix,
                "UPtr", item.Count,
                "Ptr*", &outHandle,
                "Int"
            )
            AhkTest.AssertEqual(item.Status, status)
            AhkTest.AssertEqual(0, outHandle)
        }

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_convert_matrix_into",
            "Ptr", rgb,
            "Int", 3,
            "Ptr", matrix12,
            "UPtr", 12,
            "Ptr", wrongTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(wrongTarget)
        PillowCFreeImage(l)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
    }
}

AhkTest.Test("pillow_c image convert_matrix rejects invalid arguments", PillowCTestImageConvertMatrixRejectsInvalidArguments)

PillowCTestPaletteModeConvertsThroughNativePalette(*) {
    source := PillowCCreateImageMode(4, 1, 6)
    outputs := []
    try {
        PillowCImageSetBytes(source, [0, 1, 2, 3])
        PillowCImagePutPaletteRgb(source, [
            0, 0, 0,
            10, 20, 30,
            200, 100, 50,
            255, 255, 255,
        ])
        AhkTest.AssertEqual([0, 1, 2, 3], PillowCImageToArray(source, 4))
        AhkTest.AssertEqual([
            0, 0, 0,
            10, 20, 30,
            200, 100, 50,
            255, 255, 255,
        ], PillowCImageGetPaletteRgb(source))

        rgb := PillowCImageConvertMode(source, 3)
        rgba := PillowCImageConvertMode(source, 4)
        l := PillowCImageConvertMode(source, 1)
        outputs.Push(rgb)
        outputs.Push(rgba)
        outputs.Push(l)

        AhkTest.AssertEqual([0, 0, 0, 10, 20, 30, 200, 100, 50, 255, 255, 255], PillowCImageToArray(rgb, 12))
        AhkTest.AssertEqual([0, 0, 0, 255, 10, 20, 30, 255, 200, 100, 50, 255, 255, 255, 255, 255], PillowCImageToArray(rgba, 16))
        AhkTest.AssertEqual([0, 18, 124, 255], PillowCImageToArray(l, 4))
    } finally {
        for handle in outputs
            PillowCFreeImage(handle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c P mode converts through native RGB palette", PillowCTestPaletteModeConvertsThroughNativePalette)

PillowCTestImageQuantizePaletteMatchesPillowReferencePalette(*) {
    palette := PillowCCreateImageMode(1, 1, 6)
    rgb := PillowCCreateImageMode(4, 1, 3)
    l := PillowCCreateImageMode(4, 1, 1)
    rgbOut := 0
    lOut := 0
    referencePalette := [
        0, 0, 0,
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
    ]
    try {
        PillowCImagePutPaletteRgb(palette, referencePalette)
        PillowCImageSetBytes(rgb, [
            250, 10, 10,
            10, 240, 20,
            1, 2, 3,
            0, 0, 250,
        ])
        PillowCImageSetBytes(l, [0, 100, 200, 255])

        rgbOut := PillowCImageQuantizePalette(rgb, palette)
        lOut := PillowCImageQuantizePalette(l, palette)

        AhkTest.AssertEqual(6, PillowCImageMode(rgbOut))
        AhkTest.AssertEqual([1, 2, 0, 3], PillowCImageToArray(rgbOut, 4))
        AhkTest.AssertEqual(referencePalette, PillowCImageGetPaletteRgb(rgbOut))
        AhkTest.AssertEqual(6, PillowCImageMode(lOut))
        AhkTest.AssertEqual([0, 100, 200, 255], PillowCImageToArray(lOut, 4))
        AhkTest.AssertEqual(referencePalette, PillowCImageGetPaletteRgb(lOut))
    } finally {
        for handle in [lOut, rgbOut, l, rgb, palette] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image quantize_palette matches Pillow reference palette", PillowCTestImageQuantizePaletteMatchesPillowReferencePalette)

PillowCTestImageQuantizeExactColorsMatchesPillow(*) {
    rgb := PillowCCreateImageMode(4, 1, 3)
    l := PillowCCreateImageMode(4, 1, 1)
    rgbOut := 0
    lOut := 0
    try {
        PillowCImageSetBytes(rgb, [
            255, 0, 0,
            0, 255, 0,
            0, 0, 255,
            0, 0, 0,
        ])
        PillowCImageSetBytes(l, [0, 100, 200, 255])

        rgbOut := PillowCImageQuantize(rgb, 4)
        lOut := PillowCImageQuantize(l, 4)

        AhkTest.AssertEqual(6, PillowCImageMode(rgbOut))
        AhkTest.AssertEqual([1, 0, 2, 3], PillowCImageToArray(rgbOut, 4))
        AhkTest.AssertEqual([
            0, 255, 0,
            255, 0, 0,
            0, 0, 255,
            0, 0, 0,
        ], PillowCImageGetPaletteRgb(rgbOut))
        AhkTest.AssertEqual(6, PillowCImageMode(lOut))
        AhkTest.AssertEqual([3, 2, 1, 0], PillowCImageToArray(lOut, 4))
        AhkTest.AssertEqual([
            255, 255, 255,
            200, 200, 200,
            100, 100, 100,
            0, 0, 0,
        ], PillowCImageGetPaletteRgb(lOut))
    } finally {
        for handle in [lOut, rgbOut, l, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image quantize covers exact unique colors", PillowCTestImageQuantizeExactColorsMatchesPillow)

PillowCTestImageRemapPaletteMatchesPillowPAndL(*) {
    p := PillowCCreateImageMode(4, 1, 6)
    l := PillowCCreateImageMode(4, 1, 1)
    rgb := PillowCCreateImageMode(1, 1, 3)
    remappedP := 0
    duplicateP := 0
    remappedL := 0
    target := PillowCCreateImageMode(4, 1, 6)
    wrongTarget := PillowCCreateImageMode(4, 1, 1)
    outHandle := 0
    try {
        PillowCImageSetBytes(p, [0, 1, 2, 3])
        PillowCImagePutPaletteRgb(p, [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120])
        PillowCImageSetBytes(l, [0, 1, 2, 3])

        remappedP := PillowCImageRemapPalette(p, [3, 2, 1, 0])
        AhkTest.AssertEqual(6, PillowCImageMode(remappedP))
        AhkTest.AssertEqual([3, 2, 1, 0], PillowCImageToArray(remappedP, 4))
        AhkTest.AssertEqual([100, 110, 120, 70, 80, 90, 40, 50, 60, 10, 20, 30], PillowCImageGetPaletteRgb(remappedP))

        duplicateP := PillowCImageRemapPalette(p, [1, 1, 2, 3])
        AhkTest.AssertEqual([0, 1, 2, 3], PillowCImageToArray(duplicateP, 4))
        AhkTest.AssertEqual([40, 50, 60, 40, 50, 60, 70, 80, 90, 100, 110, 120], PillowCImageGetPaletteRgb(duplicateP))

        remappedL := PillowCImageRemapPalette(l, [3, 2, 1, 0])
        AhkTest.AssertEqual(6, PillowCImageMode(remappedL))
        AhkTest.AssertEqual([3, 2, 1, 0], PillowCImageToArray(remappedL, 4))
        AhkTest.AssertEqual([3, 3, 3, 2, 2, 2, 1, 1, 1, 0, 0, 0], PillowCImageGetPaletteRgb(remappedL))

        PillowCImageRemapPaletteInto(p, [0, 1], target)
        AhkTest.AssertEqual([0, 1, 0, 0], PillowCImageToArray(target, 4))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60], PillowCImageGetPaletteRgb(target))

        map := PillowCIntBuffer([0])
        status := DllCall(PillowCDllPath() "\pillow_c_image_remap_palette", "Ptr", rgb, "Ptr", map, "UPtr", 1, "Ptr", 0, "UPtr", 0, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(PillowCDllPath() "\pillow_c_image_remap_palette_into", "Ptr", p, "Ptr", map, "UPtr", 1, "Ptr", 0, "UPtr", 0, "Ptr", wrongTarget, "Int")
        AhkTest.AssertEqual(-5, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        for image in [wrongTarget, target, remappedL, duplicateP, remappedP, rgb, l, p] {
            if image
                PillowCFreeImage(image)
        }
    }
}

AhkTest.Test("pillow_c image remap_palette matches Pillow P and L", PillowCTestImageRemapPaletteMatchesPillowPAndL)

PillowCTestPaletteModePreservesPaletteThroughNativeOperations(*) {
    source := PillowCCreateImageMode(4, 2, 6)
    outputs := []
    palette := [
        0, 0, 0,
        10, 20, 30,
        200, 100, 50,
        255, 255, 255,
    ]
    try {
        PillowCImageSetBytes(source, [0, 1, 2, 3, 3, 2, 1, 0])
        PillowCImagePutPaletteRgb(source, palette)

        cropped := PillowCImageCrop(source, 1, 0, 4, 2)
        transposed := PillowCImageTranspose(source, 2)
        resized := PillowCImageResize(source, 2, 4, 0)
        offset := PillowCImageOffset(source, 1, -1)
        expanded := PillowCImageExpand(source, 1, 0, 1, 1, [3])
        outputs.Push(cropped)
        outputs.Push(transposed)
        outputs.Push(resized)
        outputs.Push(offset)
        outputs.Push(expanded)

        AhkTest.AssertEqual(6, PillowCImageInt(cropped, "pillow_c_image_mode"))
        AhkTest.AssertEqual(6, PillowCImageInt(transposed, "pillow_c_image_mode"))
        AhkTest.AssertEqual(6, PillowCImageInt(resized, "pillow_c_image_mode"))
        AhkTest.AssertEqual(6, PillowCImageInt(offset, "pillow_c_image_mode"))
        AhkTest.AssertEqual(6, PillowCImageInt(expanded, "pillow_c_image_mode"))
        AhkTest.AssertEqual(palette, PillowCImageGetPaletteRgb(cropped))
        AhkTest.AssertEqual(palette, PillowCImageGetPaletteRgb(transposed))
        AhkTest.AssertEqual(palette, PillowCImageGetPaletteRgb(resized))
        AhkTest.AssertEqual(palette, PillowCImageGetPaletteRgb(offset))
        AhkTest.AssertEqual(palette, PillowCImageGetPaletteRgb(expanded))

        croppedRgb := PillowCImageConvertMode(cropped, 3)
        transposedRgb := PillowCImageConvertMode(transposed, 3)
        resizedRgb := PillowCImageConvertMode(resized, 3)
        offsetRgb := PillowCImageConvertMode(offset, 3)
        expandedRgb := PillowCImageConvertMode(expanded, 3)
        outputs.Push(croppedRgb)
        outputs.Push(transposedRgb)
        outputs.Push(resizedRgb)
        outputs.Push(offsetRgb)
        outputs.Push(expandedRgb)

        AhkTest.AssertEqual([10, 20, 30, 200, 100, 50, 255, 255, 255, 200, 100, 50, 10, 20, 30, 0, 0, 0], PillowCImageToArray(croppedRgb, 18))
        AhkTest.AssertEqual([255, 255, 255, 0, 0, 0, 200, 100, 50, 10, 20, 30, 10, 20, 30, 200, 100, 50, 0, 0, 0, 255, 255, 255], PillowCImageToArray(transposedRgb, 24))
        AhkTest.AssertEqual([10, 20, 30, 255, 255, 255, 10, 20, 30, 255, 255, 255, 200, 100, 50, 0, 0, 0, 200, 100, 50, 0, 0, 0], PillowCImageToArray(resizedRgb, 24))
        AhkTest.AssertEqual([0, 0, 0, 255, 255, 255, 200, 100, 50, 10, 20, 30, 255, 255, 255, 0, 0, 0, 10, 20, 30, 200, 100, 50], PillowCImageToArray(offsetRgb, 24))
        AhkTest.AssertEqual([255, 255, 255, 0, 0, 0, 10, 20, 30, 200, 100, 50, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 200, 100, 50, 10, 20, 30, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255], PillowCImageToArray(expandedRgb, 54))
    } finally {
        for handle in outputs
            PillowCFreeImage(handle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c P mode preserves palette through native operations", PillowCTestPaletteModePreservesPaletteThroughNativeOperations)

PillowCTestPutPaletteConvertsLToPaletteMode(*) {
    source := PillowCCreateImageMode(3, 1, 1)
    rgb := 0
    palette := [
        10, 20, 30,
        40, 50, 60,
        70, 80, 90,
    ]
    try {
        PillowCImageSetBytes(source, [0, 1, 2])
        PillowCImagePutPaletteRgb(source, palette)
        rgb := PillowCImageConvertMode(source, 3)

        AhkTest.AssertEqual(6, PillowCImageMode(source))
        AhkTest.AssertEqual([0, 1, 2], PillowCImageToArray(source, 3))
        AhkTest.AssertEqual(palette, PillowCImageGetPaletteRgb(source))
        AhkTest.AssertEqual([10, 20, 30, 40, 50, 60, 70, 80, 90], PillowCImageToArray(rgb, 9))
    } finally {
        if rgb
            PillowCFreeImage(rgb)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image put_palette converts L images to P mode", PillowCTestPutPaletteConvertsLToPaletteMode)

PillowCTestPutPaletteRgbaPreservesAlphaMetadata(*) {
    source := PillowCCreateImageMode(2, 1, 1)
    rgb := 0
    paletteRgba := [
        10, 20, 30, 40,
        50, 60, 70, 80,
    ]
    try {
        PillowCImageSetBytes(source, [0, 1])
        PillowCImagePutPaletteRgba(source, paletteRgba)
        rgb := PillowCImageConvertMode(source, 3)

        AhkTest.AssertEqual(6, PillowCImageMode(source))
        AhkTest.AssertEqual([0, 1], PillowCImageToArray(source, 2))
        AhkTest.AssertEqual([10, 20, 30, 50, 60, 70], PillowCImageGetPaletteRgb(source))
        AhkTest.AssertEqual(paletteRgba, PillowCImageGetPaletteRgba(source))
        AhkTest.AssertEqual(1, PillowCImagePaletteAlphaMode(source))
        AhkTest.AssertEqual([10, 20, 30, 50, 60, 70], PillowCImageToArray(rgb, 6))
    } finally {
        if rgb
            PillowCFreeImage(rgb)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image put_palette_rgba preserves alpha metadata", PillowCTestPutPaletteRgbaPreservesAlphaMetadata)

PillowCTestPutPaletteRgbaTracksAlphaMode(*) {
    source := PillowCCreateImageMode(1, 1, 6)
    try {
        PillowCImagePutPaletteRgba(source, [1, 2, 3, 4], 2)
        AhkTest.AssertEqual(2, PillowCImagePaletteAlphaMode(source))
        AhkTest.AssertEqual([1, 2, 3, 4], PillowCImageGetPaletteRgba(source))

        PillowCImagePutPaletteRgba(source, [5, 6, 7, 255], 0)
        AhkTest.AssertEqual(0, PillowCImagePaletteAlphaMode(source))
        AhkTest.AssertEqual([5, 6, 7, 255], PillowCImageGetPaletteRgba(source))

        PillowCImagePutPaletteRgb(source, [8, 9, 10])
        AhkTest.AssertEqual(0, PillowCImagePaletteAlphaMode(source))
        AhkTest.AssertEqual([8, 9, 10, 255], PillowCImageGetPaletteRgba(source))
    } finally {
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image put_palette_rgba tracks alpha mode metadata", PillowCTestPutPaletteRgbaTracksAlphaMode)

PillowCTestPaletteModePreservesPaletteThroughGeometryOperations(*) {
    source := PillowCCreateImageMode(2, 2, 6)
    outputs := []
    palette := [
        0, 0, 0,
        10, 20, 30,
        200, 100, 50,
        255, 255, 255,
    ]
    try {
        PillowCImageSetBytes(source, [0, 1, 2, 3])
        PillowCImagePutPaletteRgb(source, palette)

        transformed := PillowCImageTransformAffine(source, 2, 2, [1, 0, -1, 0, 1, 0], 0, [3])
        rotated := PillowCImageRotate(source, 45, 0, true, 0.0, 0.0, false, 0.0, 0.0, false, [3])
        outputs.Push(transformed)
        outputs.Push(rotated)

        AhkTest.AssertEqual(6, PillowCImageInt(transformed, "pillow_c_image_mode"))
        AhkTest.AssertEqual(6, PillowCImageInt(rotated, "pillow_c_image_mode"))
        AhkTest.AssertEqual(palette, PillowCImageGetPaletteRgb(transformed))
        AhkTest.AssertEqual(palette, PillowCImageGetPaletteRgb(rotated))

        transformedRgb := PillowCImageConvertMode(transformed, 3)
        rotatedRgb := PillowCImageConvertMode(rotated, 3)
        outputs.Push(transformedRgb)
        outputs.Push(rotatedRgb)

        AhkTest.AssertEqual([255, 255, 255, 0, 0, 0, 255, 255, 255, 200, 100, 50], PillowCImageToArray(transformedRgb, 12))
        AhkTest.AssertEqual([255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 10, 20, 30, 255, 255, 255, 255, 255, 255, 255, 255, 255, 200, 100, 50, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255], PillowCImageToArray(rotatedRgb, 48))
    } finally {
        for handle in outputs
            PillowCFreeImage(handle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c P mode preserves palette through geometry operations", PillowCTestPaletteModePreservesPaletteThroughGeometryOperations)

PillowCTestPaletteModePreservesPaletteThroughPointOperations(*) {
    source := PillowCCreateImageMode(2, 2, 6)
    target := PillowCCreateImageMode(2, 2, 6)
    outputs := []
    palette := [
        0, 0, 0,
        10, 20, 30,
        200, 100, 50,
        255, 255, 255,
    ]
    lut := []
    loop 256
        lut.Push(Mod(A_Index, 4))
    try {
        PillowCImageSetBytes(source, [0, 1, 2, 3])
        PillowCImagePutPaletteRgb(source, palette)

        pointed := PillowCImagePointLut(source, lut)
        outputs.Push(pointed)
        before := PillowCImageData(target).Ptr
        PillowCImagePointLutInto(source, lut, target)
        after := PillowCImageData(target).Ptr

        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual(6, PillowCImageInt(pointed, "pillow_c_image_mode"))
        AhkTest.AssertEqual(6, PillowCImageInt(target, "pillow_c_image_mode"))
        AhkTest.AssertEqual([1, 2, 3, 0], PillowCImageToArray(pointed, 4))
        AhkTest.AssertEqual([1, 2, 3, 0], PillowCImageToArray(target, 4))
        AhkTest.AssertEqual(palette, PillowCImageGetPaletteRgb(pointed))
        AhkTest.AssertEqual(palette, PillowCImageGetPaletteRgb(target))

        pointedRgb := PillowCImageConvertMode(pointed, 3)
        targetRgb := PillowCImageConvertMode(target, 3)
        outputs.Push(pointedRgb)
        outputs.Push(targetRgb)
        AhkTest.AssertEqual([10, 20, 30, 200, 100, 50, 255, 255, 255, 0, 0, 0], PillowCImageToArray(pointedRgb, 12))
        AhkTest.AssertEqual([10, 20, 30, 200, 100, 50, 255, 255, 255, 0, 0, 0], PillowCImageToArray(targetRgb, 12))
    } finally {
        for handle in outputs
            PillowCFreeImage(handle)
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c P mode preserves palette through point operations", PillowCTestPaletteModePreservesPaletteThroughPointOperations)

PillowCTestImageConvertModeIntoReusesTargetHandle(*) {
    source := PillowCCreateImageMode(2, 1, 3)
    target := PillowCCreateImageMode(2, 1, 4)
    try {
        PillowCImageSetBytes(source, [10, 20, 30, 200, 100, 50])
        before := PillowCImageData(target).Ptr
        PillowCImageConvertModeInto(source, 4, target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([10, 20, 30, 255, 200, 100, 50, 255], PillowCImageToArray(target, 8))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image convert_mode_into reuses target handle storage", PillowCTestImageConvertModeIntoReusesTargetHandle)

PillowCTestImageRgbToLHandleOperation(*) {
    source := PillowCCreateImage(8, 1, 3)
    converted := 0
    try {
        PillowCImageSetBytes(source, [
            0, 0, 0,
            255, 255, 255,
            255, 0, 0,
            0, 255, 0,
            0, 0, 255,
            10, 20, 30,
            123, 45, 67,
            250, 128, 1,
        ])
        converted := PillowCImageRgbToL(source)
        AhkTest.AssertEqual(8, PillowCImageInt(converted, "pillow_c_image_width"))
        AhkTest.AssertEqual(1, PillowCImageInt(converted, "pillow_c_image_height"))
        AhkTest.AssertEqual(1, PillowCImageInt(converted, "pillow_c_image_channels"))
        AhkTest.AssertEqual([0, 255, 76, 150, 29, 18, 71, 150], PillowCImageToArray(converted, 8))
    } finally {
        if converted
            PillowCFreeImage(converted)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image rgb_to_l operates on native handles", PillowCTestImageRgbToLHandleOperation)

PillowCTestImageRgbToLIntoReusesTargetHandle(*) {
    source := PillowCCreateImage(8, 1, 3)
    target := PillowCCreateImage(8, 1, 1)
    try {
        PillowCImageSetBytes(source, [
            0, 0, 0,
            255, 255, 255,
            255, 0, 0,
            0, 255, 0,
            0, 0, 255,
            10, 20, 30,
            123, 45, 67,
            250, 128, 1,
        ])
        before := PillowCImageData(target).Ptr
        PillowCImageRgbToLInto(source, target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([0, 255, 76, 150, 29, 18, 71, 150], PillowCImageToArray(target, 8))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image rgb_to_l_into reuses target handle storage", PillowCTestImageRgbToLIntoReusesTargetHandle)

PillowCTestImageAlphaCompositeRgbaHandleOperation(*) {
    dst := PillowCCreateImage(5, 1, 4)
    src := PillowCCreateImage(5, 1, 4)
    composited := 0
    try {
        PillowCImageSetBytes(dst, [
            0, 0, 0, 0,
            10, 20, 30, 255,
            100, 110, 120, 128,
            200, 10, 30, 64,
            1, 2, 3, 4,
        ])
        PillowCImageSetBytes(src, [
            255, 0, 0, 0,
            200, 210, 220, 255,
            50, 60, 70, 128,
            10, 240, 30, 192,
            250, 240, 230, 5,
        ])
        composited := PillowCImageAlphaCompositeRgba(dst, src)
        AhkTest.AssertEqual(5, PillowCImageInt(composited, "pillow_c_image_width"))
        AhkTest.AssertEqual(1, PillowCImageInt(composited, "pillow_c_image_height"))
        AhkTest.AssertEqual(4, PillowCImageInt(composited, "pillow_c_image_channels"))
        AhkTest.AssertEqual([
            0, 0, 0, 0,
            200, 210, 220, 255,
            67, 77, 87, 192,
            24, 222, 30, 208,
            141, 135, 130, 9,
        ], PillowCImageToArray(composited, 20))
    } finally {
        if composited
            PillowCFreeImage(composited)
        PillowCFreeImage(src)
        PillowCFreeImage(dst)
    }
}

AhkTest.Test("pillow_c image alpha_composite_rgba operates on native handles", PillowCTestImageAlphaCompositeRgbaHandleOperation)

PillowCTestImageAlphaCompositeRgbaIntoReusesTargetHandle(*) {
    dst := PillowCCreateImage(5, 1, 4)
    src := PillowCCreateImage(5, 1, 4)
    target := PillowCCreateImage(5, 1, 4)
    try {
        PillowCImageSetBytes(dst, [
            0, 0, 0, 0,
            10, 20, 30, 255,
            100, 110, 120, 128,
            200, 10, 30, 64,
            1, 2, 3, 4,
        ])
        PillowCImageSetBytes(src, [
            255, 0, 0, 0,
            200, 210, 220, 255,
            50, 60, 70, 128,
            10, 240, 30, 192,
            250, 240, 230, 5,
        ])
        before := PillowCImageData(target).Ptr
        PillowCImageAlphaCompositeRgbaInto(dst, src, target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([
            0, 0, 0, 0,
            200, 210, 220, 255,
            67, 77, 87, 192,
            24, 222, 30, 208,
            141, 135, 130, 9,
        ], PillowCImageToArray(target, 20))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(src)
        PillowCFreeImage(dst)
    }
}

AhkTest.Test("pillow_c image alpha_composite_rgba_into reuses target handle storage", PillowCTestImageAlphaCompositeRgbaIntoReusesTargetHandle)

PillowCAlphaCompositeInPlaceSourceBytes() {
    return [
        100, 0, 0, 128, 0, 100, 0, 64, 0, 0, 100, 255,
        200, 50, 0, 128, 0, 200, 50, 128, 50, 0, 200, 0,
    ]
}

PillowCAlphaCompositeInPlaceNewDst() {
    dst := PillowCCreateImage(4, 3, 4)
    PillowCImageSetBytes(dst, [
        10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255,
        10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255,
        10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255,
    ])
    return dst
}

PillowCTestImageAlphaCompositeRgbaInPlaceMatchesPillowGeometry(*) {
    src := PillowCCreateImage(3, 2, 4)
    destOffset := 0
    negativeDest := 0
    sourceBox := 0
    rgb := PillowCCreateImageMode(1, 1, 3)
    try {
        PillowCImageSetBytes(src, PillowCAlphaCompositeInPlaceSourceBytes())

        destOffset := PillowCAlphaCompositeInPlaceNewDst()
        PillowCImageAlphaCompositeRgbaInPlace(destOffset, src, 1, 1, 0, 0, 3, 2)
        AhkTest.AssertEqual([
            10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255,
            10, 20, 30, 255, 55, 10, 15, 255, 7, 40, 22, 255, 0, 0, 100, 255,
            10, 20, 30, 255, 105, 35, 15, 255, 5, 110, 40, 255, 10, 20, 30, 255,
        ], PillowCImageToArray(destOffset, 48))

        negativeDest := PillowCAlphaCompositeInPlaceNewDst()
        PillowCImageAlphaCompositeRgbaInPlace(negativeDest, src, -1, 0, 0, 0, 3, 2)
        AhkTest.AssertEqual([
            7, 40, 22, 255, 0, 0, 100, 255, 10, 20, 30, 255, 10, 20, 30, 255,
            5, 110, 40, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255,
            10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255,
        ], PillowCImageToArray(negativeDest, 48))

        sourceBox := PillowCAlphaCompositeInPlaceNewDst()
        PillowCImageAlphaCompositeRgbaInPlace(sourceBox, src, 1, 0, 1, 0, 3, 2)
        AhkTest.AssertEqual([
            10, 20, 30, 255, 7, 40, 22, 255, 0, 0, 100, 255, 10, 20, 30, 255,
            10, 20, 30, 255, 5, 110, 40, 255, 10, 20, 30, 255, 10, 20, 30, 255,
            10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255,
        ], PillowCImageToArray(sourceBox, 48))

        status := DllCall(PillowCDllPath() "\pillow_c_image_alpha_composite_rgba_in_place", "Ptr", rgb, "Ptr", src, "Int", 0, "Int", 0, "Int", 0, "Int", 0, "Int", 1, "Int", 1, "Int")
        AhkTest.AssertEqual(-3, status)

        status := DllCall(PillowCDllPath() "\pillow_c_image_alpha_composite_rgba_in_place", "Ptr", destOffset, "Ptr", src, "Int", 0, "Int", 0, "Int", -1, "Int", 0, "Int", 1, "Int", 1, "Int")
        AhkTest.AssertEqual(-3, status)
    } finally {
        PillowCFreeImage(rgb)
        for image in [sourceBox, negativeDest, destOffset, src] {
            if image
                PillowCFreeImage(image)
        }
    }
}

AhkTest.Test("pillow_c image alpha_composite_rgba_in_place matches Pillow geometry", PillowCTestImageAlphaCompositeRgbaInPlaceMatchesPillowGeometry)

PillowCTestImageCropInsideBox(*) {
    source := PillowCCreateImage(3, 2, 3)
    cropped := 0
    try {
        PillowCImageSetBytes(source, [
            10, 20, 30, 40, 50, 60, 70, 80, 90,
            100, 110, 120, 130, 140, 150, 160, 170, 180,
        ])
        cropped := PillowCImageCrop(source, 1, 0, 3, 2)
        AhkTest.AssertEqual(2, PillowCImageInt(cropped, "pillow_c_image_width"))
        AhkTest.AssertEqual(2, PillowCImageInt(cropped, "pillow_c_image_height"))
        AhkTest.AssertEqual(3, PillowCImageInt(cropped, "pillow_c_image_channels"))
        AhkTest.AssertEqual([40, 50, 60, 70, 80, 90, 130, 140, 150, 160, 170, 180], PillowCImageToArray(cropped, 12))
    } finally {
        if cropped
            PillowCFreeImage(cropped)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image crop copies an inside box", PillowCTestImageCropInsideBox)

PillowCTestImageCropIntoReusesTargetHandle(*) {
    source := PillowCCreateImage(3, 2, 3)
    target := PillowCCreateImage(2, 2, 3)
    try {
        PillowCImageSetBytes(source, [
            10, 20, 30, 40, 50, 60, 70, 80, 90,
            100, 110, 120, 130, 140, 150, 160, 170, 180,
        ])
        before := PillowCImageData(target).Ptr
        PillowCImageCropInto(source, 1, 0, 3, 2, target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([40, 50, 60, 70, 80, 90, 130, 140, 150, 160, 170, 180], PillowCImageToArray(target, 12))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image crop_into reuses target handle storage", PillowCTestImageCropIntoReusesTargetHandle)

PillowCTestImageCropOutsideBoxZeroFills(*) {
    source := PillowCCreateImage(3, 2, 3)
    cropped := 0
    try {
        PillowCImageSetBytes(source, [
            10, 20, 30, 40, 50, 60, 70, 80, 90,
            100, 110, 120, 130, 140, 150, 160, 170, 180,
        ])
        cropped := PillowCImageCrop(source, -1, -1, 2, 2)
        AhkTest.AssertEqual(3, PillowCImageInt(cropped, "pillow_c_image_width"))
        AhkTest.AssertEqual(3, PillowCImageInt(cropped, "pillow_c_image_height"))
        AhkTest.AssertEqual(3, PillowCImageInt(cropped, "pillow_c_image_channels"))
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 10, 20, 30, 40, 50, 60,
            0, 0, 0, 100, 110, 120, 130, 140, 150,
        ], PillowCImageToArray(cropped, 27))
    } finally {
        if cropped
            PillowCFreeImage(cropped)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image crop zero-fills outside the source", PillowCTestImageCropOutsideBoxZeroFills)

PillowCTestImageCropAllowsZeroWidth(*) {
    source := PillowCCreateImage(3, 2, 3)
    cropped := 0
    try {
        PillowCImageSetBytes(source, [
            10, 20, 30, 40, 50, 60, 70, 80, 90,
            100, 110, 120, 130, 140, 150, 160, 170, 180,
        ])
        cropped := PillowCImageCrop(source, 1, 1, 1, 2)
        AhkTest.AssertEqual(0, PillowCImageInt(cropped, "pillow_c_image_width"))
        AhkTest.AssertEqual(1, PillowCImageInt(cropped, "pillow_c_image_height"))
        AhkTest.AssertEqual(3, PillowCImageInt(cropped, "pillow_c_image_channels"))
        AhkTest.AssertEqual(0, PillowCImageSize(cropped))
        AhkTest.AssertEqual([], PillowCImageToArray(cropped, 0))
    } finally {
        if cropped
            PillowCFreeImage(cropped)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image crop allows zero width like Pillow", PillowCTestImageCropAllowsZeroWidth)

PillowCTestImageCropRejectsInvertedBox(*) {
    source := PillowCCreateImage(3, 2, 3)
    cropped := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_crop",
            "Ptr", source,
            "Int", 2,
            "Int", 0,
            "Int", 1,
            "Int", 1,
            "Ptr*", &cropped,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, cropped)
    } finally {
        if cropped
            PillowCFreeImage(cropped)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image crop rejects right less than left", PillowCTestImageCropRejectsInvertedBox)

PillowCTestImageExpandAddsFilledBorderAroundLImage(*) {
    source := PillowCCreateImageMode(2, 2, 1)
    out := 0
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4])
        out := PillowCImageExpand(source, 1, 1, 1, 1, [7])
        AhkTest.AssertEqual(1, PillowCImageMode(out))
        AhkTest.AssertEqual([4, 4], [PillowCImageInt(out, "pillow_c_image_width"), PillowCImageInt(out, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            7, 7, 7, 7,
            7, 1, 2, 7,
            7, 3, 4, 7,
            7, 7, 7, 7,
        ], PillowCImageToArray(out, 16))
    } finally {
        if out
            PillowCFreeImage(out)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image expand adds a filled border around an L image", PillowCTestImageExpandAddsFilledBorderAroundLImage)

PillowCTestImageExpandSupportsRgbAndRgbaFillColors(*) {
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(1, 1, 4)
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(rgb, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(rgba, [1, 2, 3, 4])
        rgbOut := PillowCImageExpand(rgb, 1, 0, 2, 1, [7, 8, 9])
        rgbaOut := PillowCImageExpand(rgba, 1, 1, 1, 1, [7, 8, 9, 10])

        AhkTest.AssertEqual([5, 2], [PillowCImageInt(rgbOut, "pillow_c_image_width"), PillowCImageInt(rgbOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            7, 8, 9, 1, 2, 3, 4, 5, 6, 7, 8, 9, 7, 8, 9,
            7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9,
        ], PillowCImageToArray(rgbOut, 30))

        AhkTest.AssertEqual(4, PillowCImageMode(rgbaOut))
        AhkTest.AssertEqual([
            7, 8, 9, 10, 7, 8, 9, 10, 7, 8, 9, 10,
            7, 8, 9, 10, 1, 2, 3, 4, 7, 8, 9, 10,
            7, 8, 9, 10, 7, 8, 9, 10, 7, 8, 9, 10,
        ], PillowCImageToArray(rgbaOut, 36))
    } finally {
        if rgbaOut
            PillowCFreeImage(rgbaOut)
        if rgbOut
            PillowCFreeImage(rgbOut)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
    }
}

AhkTest.Test("pillow_c image expand supports RGB and RGBA fill colors", PillowCTestImageExpandSupportsRgbAndRgbaFillColors)

PillowCTestImageExpandClipsNegativeBordersLikePillowPaste(*) {
    source := PillowCCreateImageMode(2, 2, 1)
    out := 0
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4])
        out := PillowCImageExpand(source, -1, -1, 0, 0, [9])
        AhkTest.AssertEqual([1, 1], [PillowCImageInt(out, "pillow_c_image_width"), PillowCImageInt(out, "pillow_c_image_height")])
        AhkTest.AssertEqual([4], PillowCImageToArray(out, 1))
    } finally {
        if out
            PillowCFreeImage(out)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image expand clips negative borders like Pillow paste", PillowCTestImageExpandClipsNegativeBordersLikePillowPaste)

PillowCTestImageExpandHandlesEmptySourcesAndReusesTargetStorage(*) {
    source := PillowCCreateImageMode(2, 2, 1)
    empty := 0
    out := 0
    target := PillowCCreateImageMode(3, 2, 1)
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4])
        empty := PillowCImageCrop(source, 0, 0, 0, 1)
        out := PillowCImageExpand(empty, 1, 1, 2, 0, [5])
        AhkTest.AssertEqual([3, 2], [PillowCImageInt(out, "pillow_c_image_width"), PillowCImageInt(out, "pillow_c_image_height")])
        AhkTest.AssertEqual([5, 5, 5, 5, 5, 5], PillowCImageToArray(out, 6))

        before := PillowCImageData(target).Ptr
        PillowCImageExpandInto(empty, 1, 1, 2, 0, [6], target)
        AhkTest.AssertEqual(before, PillowCImageData(target).Ptr)
        AhkTest.AssertEqual([6, 6, 6, 6, 6, 6], PillowCImageToArray(target, 6))
    } finally {
        PillowCFreeImage(target)
        if out
            PillowCFreeImage(out)
        if empty
            PillowCFreeImage(empty)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image expand handles empty sources and expand_into reuses target storage", PillowCTestImageExpandHandlesEmptySourcesAndReusesTargetStorage)

PillowCTestImageExpandRejectsInvalidShapeOrFillLength(*) {
    source := PillowCCreateImageMode(2, 1, 3)
    out := 0
    try {
        color := PillowCBuffer([1, 2])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_expand",
            "Ptr", source,
            "Int", 1,
            "Int", 1,
            "Int", 1,
            "Int", 1,
            "Ptr", color,
            "UPtr", color.Size,
            "Ptr*", &out,
            "Int"
        )
        AhkTest.AssertEqual(-2, status)
        AhkTest.AssertEqual(0, out)

        color := PillowCBuffer([0, 0, 0])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_expand",
            "Ptr", source,
            "Int", 0,
            "Int", -2,
            "Int", 0,
            "Int", 0,
            "Ptr", color,
            "UPtr", color.Size,
            "Ptr*", &out,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, out)
    } finally {
        if out
            PillowCFreeImage(out)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image expand rejects invalid output shapes or fill lengths", PillowCTestImageExpandRejectsInvalidShapeOrFillLength)

PillowCTestImageResizeNearestMatchesPillowCenterSampling(*) {
    l := PillowCCreateImageMode(2, 2, 1)
    rgb := PillowCCreateImageMode(2, 2, 3)
    lBig := 0
    lSmall := 0
    lSmallSource := 0
    lTall := 0
    lTallSource := 0
    rgbOut := 0
    try {
        PillowCImageSetBytes(l, [1, 2, 3, 4])
        PillowCImageSetBytes(rgb, [
            1, 2, 3, 10, 20, 30,
            100, 110, 120, 200, 210, 220,
        ])
        lBig := PillowCImageResize(l, 4, 4, 0)
        lSmallSource := PillowCCreateImageMode(4, 1, 1)
        try {
            PillowCImageSetBytes(lSmallSource, [10, 20, 30, 40])
            lSmall := PillowCImageResize(lSmallSource, 2, 1, 0)
        } finally {
            if lSmallSource
                PillowCFreeImage(lSmallSource)
        }
        lTallSource := PillowCCreateImageMode(1, 4, 1)
        try {
            PillowCImageSetBytes(lTallSource, [215, 194, 144, 49])
            lTall := PillowCImageResize(lTallSource, 1, 6, 0)
        } finally {
            if lTallSource
                PillowCFreeImage(lTallSource)
        }
        rgbOut := PillowCImageResize(rgb, 3, 3, 0)
        AhkTest.AssertEqual([4, 4], [PillowCImageInt(lBig, "pillow_c_image_width"), PillowCImageInt(lBig, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            1, 1, 2, 2,
            1, 1, 2, 2,
            3, 3, 4, 4,
            3, 3, 4, 4,
        ], PillowCImageToArray(lBig, 16))
        AhkTest.AssertEqual([20, 40], PillowCImageToArray(lSmall, 2))
        AhkTest.AssertEqual([215, 194, 194, 144, 144, 49], PillowCImageToArray(lTall, 6))
        AhkTest.AssertEqual([
            1, 2, 3, 10, 20, 30, 10, 20, 30,
            100, 110, 120, 200, 210, 220, 200, 210, 220,
            100, 110, 120, 200, 210, 220, 200, 210, 220,
        ], PillowCImageToArray(rgbOut, 27))
    } finally {
        if rgbOut
            PillowCFreeImage(rgbOut)
        if lTall
            PillowCFreeImage(lTall)
        if lSmall
            PillowCFreeImage(lSmall)
        if lBig
            PillowCFreeImage(lBig)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image resize NEAREST matches Pillow center sampling", PillowCTestImageResizeNearestMatchesPillowCenterSampling)

PillowCTestImageResizeBilinearMatchesPillowFilterSampling(*) {
    l := PillowCCreateImageMode(2, 2, 1)
    la := PillowCCreateImageMode(2, 2, 2)
    rgb := PillowCCreateImageMode(2, 2, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    lOut := 0
    laOut := 0
    lSmall := 0
    lSmallSource := 0
    lTall := 0
    lTallSource := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l, [0, 64, 128, 255])
        PillowCImageSetBytes(la, [
            10, 0, 200, 128,
            50, 255, 250, 64,
        ])
        PillowCImageSetBytes(rgb, [
            0, 0, 0, 255, 0, 0,
            0, 255, 0, 0, 0, 255,
        ])
        PillowCImageSetBytes(rgba, [
            0, 0, 0, 0,
            255, 100, 50, 255,
        ])
        lOut := PillowCImageResize(l, 3, 3, 2)
        laOut := PillowCImageResize(la, 3, 3, 2)
        lSmallSource := PillowCCreateImageMode(4, 1, 1)
        try {
            PillowCImageSetBytes(lSmallSource, [10, 20, 30, 40])
            lSmall := PillowCImageResize(lSmallSource, 2, 1, 2)
        } finally {
            if lSmallSource
                PillowCFreeImage(lSmallSource)
        }
        lTallSource := PillowCCreateImageMode(1, 2, 1)
        try {
            PillowCImageSetBytes(lTallSource, [113, 18])
            lTall := PillowCImageResize(lTallSource, 1, 5, 2)
        } finally {
            if lTallSource
                PillowCFreeImage(lTallSource)
        }
        rgbOut := PillowCImageResize(rgb, 3, 3, 2)
        rgbaOut := PillowCImageResize(rgba, 4, 1, 2)

        AhkTest.AssertEqual([
            0, 32, 64,
            64, 112, 160,
            128, 192, 255,
        ], PillowCImageToArray(lOut, 9))
        AhkTest.AssertEqual([
            0, 0, 199, 64, 199, 128,
            49, 128, 122, 112, 217, 96,
            50, 255, 90, 160, 251, 64,
        ], PillowCImageToArray(laOut, 18))
        AhkTest.AssertEqual([17, 33], PillowCImageToArray(lSmall, 2))
        AhkTest.AssertEqual([113, 104, 66, 27, 18], PillowCImageToArray(lTall, 5))
        AhkTest.AssertEqual([
            0, 0, 0, 128, 0, 0, 255, 0, 0,
            0, 128, 0, 64, 64, 64, 128, 0, 128,
            0, 255, 0, 0, 128, 128, 0, 0, 255,
        ], PillowCImageToArray(rgbOut, 27))
        AhkTest.AssertEqual([
            0, 0, 0, 0,
            255, 99, 51, 64,
            255, 100, 50, 191,
            255, 100, 50, 255,
        ], PillowCImageToArray(rgbaOut, 16))
    } finally {
        if rgbaOut
            PillowCFreeImage(rgbaOut)
        if rgbOut
            PillowCFreeImage(rgbOut)
        if lTall
            PillowCFreeImage(lTall)
        if lSmall
            PillowCFreeImage(lSmall)
        if laOut
            PillowCFreeImage(laOut)
        if lOut
            PillowCFreeImage(lOut)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(la)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image resize BILINEAR matches Pillow filter sampling", PillowCTestImageResizeBilinearMatchesPillowFilterSampling)

PillowCTestImageResizeBoxMatchesPillowSampling(*) {
    l := PillowCCreateImageMode(4, 3, 1)
    la := PillowCCreateImageMode(4, 3, 2)
    rgb := PillowCCreateImageMode(3, 3, 3)
    lOut := 0
    laOut := 0
    rgbOut := 0
    target := PillowCCreateImageMode(2, 2, 3)
    wrongTarget := PillowCCreateImageMode(2, 2, 1)
    outHandle := 0
    try {
        PillowCImageSetBytes(l, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11])
        PillowCImageSetBytes(la, [
            0, 0, 100, 64, 200, 128, 255, 255,
            30, 255, 60, 128, 90, 64, 120, 0,
            255, 32, 180, 96, 90, 160, 0, 224,
        ])
        PillowCImageSetBytes(rgb, [
            0, 0, 0, 50, 0, 0, 100, 0, 0,
            0, 50, 0, 50, 50, 0, 100, 50, 0,
            0, 100, 0, 50, 100, 0, 100, 100, 0,
        ])

        lOut := PillowCImageResizeBox(l, 2, 2, 0, [1.0, 0.0, 4.0, 3.0])
        AhkTest.AssertEqual(1, PillowCImageMode(lOut))
        AhkTest.AssertEqual([1, 3, 9, 11], PillowCImageToArray(lOut, 4))

        laOut := PillowCImageResizeBox(la, 2, 2, 2, [0.5, 0.0, 3.5, 3.0])
        AhkTest.AssertEqual(2, PillowCImageMode(laOut))
        AhkTest.AssertEqual([74, 92, 200, 121, 102, 112, 59, 128], PillowCImageToArray(laOut, 8))

        rgbOut := PillowCImageResizeBox(rgb, 2, 2, 2, [0.5, 0.5, 2.5, 2.5])
        AhkTest.AssertEqual(3, PillowCImageMode(rgbOut))
        AhkTest.AssertEqual([25, 25, 0, 75, 25, 0, 25, 75, 0, 75, 75, 0], PillowCImageToArray(rgbOut, 12))

        before := PillowCImageData(target).Ptr
        PillowCImageResizeBoxInto(rgb, 2, 2, 2, [0.5, 0.5, 2.5, 2.5], target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([25, 25, 0, 75, 25, 0, 25, 75, 0, 75, 75, 0], PillowCImageToArray(target, 12))

        status := DllCall(PillowCDllPath() "\pillow_c_image_resize_box", "Ptr", rgb, "Int", 2, "Int", 2, "Int", 2, "Double", -1.0, "Double", 0.0, "Double", 2.0, "Double", 2.0, "Ptr*", &outHandle, "Int")
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(PillowCDllPath() "\pillow_c_image_resize_box_into", "Ptr", rgb, "Int", 2, "Int", 2, "Int", 2, "Double", 0.5, "Double", 0.5, "Double", 2.5, "Double", 2.5, "Ptr", wrongTarget, "Int")
        AhkTest.AssertEqual(-5, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        for image in [wrongTarget, target, rgbOut, laOut, lOut, rgb, la, l] {
            if image
                PillowCFreeImage(image)
        }
    }
}

AhkTest.Test("pillow_c image resize_box matches Pillow sampling", PillowCTestImageResizeBoxMatchesPillowSampling)

PillowCTestImageResizeReducingGapMatchesPillowTwoStepSampling(*) {
    l := PillowCCreateImageMode(8, 8, 1)
    rgb := PillowCCreateImageMode(8, 6, 3)
    p := PillowCCreateImageMode(8, 4, 6)
    lOut := 0
    rgbOut := 0
    pOut := 0
    outHandle := 0
    try {
        lBytes := []
        loop 8 {
            y := A_Index - 1
            loop 8 {
                x := A_Index - 1
                lBytes.Push(y * 20 + x * 3)
            }
        }
        PillowCImageSetBytes(l, lBytes)

        rgbBytes := []
        loop 6 {
            y := A_Index - 1
            loop 8 {
                x := A_Index - 1
                rgbBytes.Push(x * 30)
                rgbBytes.Push(y * 35)
                rgbBytes.Push(Mod(x * 11 + y * 17, 256))
            }
        }
        PillowCImageSetBytes(rgb, rgbBytes)

        pBytes := []
        loop 4 {
            y := A_Index - 1
            loop 8 {
                x := A_Index - 1
                pBytes.Push(Mod(x + y, 4))
            }
        }
        PillowCImageSetBytes(p, pBytes)
        PillowCImagePutPaletteRgb(p, [
            0, 0, 0,
            10, 20, 30,
            200, 100, 50,
            255, 255, 255,
        ])

        lOut := PillowCImageResizeReducingGap(l, 2, 2, 2, [0.0, 0.0, 8.0, 8.0], 2.0)
        AhkTest.AssertEqual([45, 55, 107, 117], PillowCImageToArray(lOut, 4))

        rgbOut := PillowCImageResizeReducingGap(rgb, 2, 2, 3, [1.0, 0.0, 7.0, 6.0], 1.5)
        AhkTest.AssertEqual([
            54, 40, 39, 156, 40, 77,
            54, 136, 85, 156, 136, 123,
        ], PillowCImageToArray(rgbOut, 12))

        pOut := PillowCImageResizeReducingGap(p, 2, 2, 3, [0.0, 0.0, 8.0, 4.0], 1.1)
        AhkTest.AssertEqual([3, 3, 1, 1], PillowCImageToArray(pOut, 4))
        AhkTest.AssertEqual([
            0, 0, 0,
            10, 20, 30,
            200, 100, 50,
            255, 255, 255,
        ], PillowCImageGetPaletteRgb(pOut))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_resize_reducing_gap",
            "Ptr", l,
            "Int", 2,
            "Int", 2,
            "Int", 2,
            "Double", 0.0,
            "Double", 0.0,
            "Double", 8.0,
            "Double", 8.0,
            "Double", 0.9,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        for image in [pOut, rgbOut, lOut, p, rgb, l] {
            if image
                PillowCFreeImage(image)
        }
    }
}

AhkTest.Test("pillow_c image resize reducing_gap matches Pillow two-step sampling", PillowCTestImageResizeReducingGapMatchesPillowTwoStepSampling)

PillowCTestImageResizeAdvancedFiltersMatchPillowSampling(*) {
    l := PillowCCreateImageMode(3, 2, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    rgba := PillowCCreateImageMode(3, 1, 4)
    rgbaIdentity := PillowCCreateImageMode(1, 1, 4)
    outputs := []
    try {
        PillowCImageSetBytes(l, [0, 30, 80, 120, 180, 255])
        PillowCImageSetBytes(rgb, [
            0, 0, 0, 255, 0, 0, 0, 255, 0,
            0, 0, 255, 255, 255, 0, 255, 0, 255,
        ])
        PillowCImageSetBytes(rgba, [
            0, 0, 0, 0,
            255, 100, 50, 128,
            0, 200, 255, 255,
        ])
        PillowCImageSetBytes(rgbaIdentity, [141, 169, 37, 98])

        cases := [
            { Name: "BOX", Filter: 4,
              L: [0, 0, 30, 80, 80, 120, 120, 180, 255, 255, 120, 120, 180, 255, 255],
              RGB: [128, 0, 0, 0, 255, 0, 128, 0, 0, 0, 255, 0, 128, 128, 128, 255, 0, 255, 128, 128, 128, 255, 0, 255],
              RGBA: [0, 0, 0, 0, 0, 0, 0, 0, 255, 99, 49, 128, 0, 200, 255, 255, 0, 200, 255, 255] },
            { Name: "HAMMING", Filter: 5,
              L: [0, 8, 30, 66, 80, 60, 73, 105, 150, 168, 120, 137, 180, 234, 255],
              RGB: [71, 0, 0, 71, 184, 0, 71, 5, 14, 85, 175, 14, 71, 66, 170, 241, 80, 170, 71, 71, 184, 255, 71, 184],
              RGBA: [0, 0, 0, 0, 255, 99, 49, 36, 255, 99, 49, 128, 41, 183, 222, 219, 0, 200, 255, 255] },
            { Name: "BICUBIC", Filter: 3,
              L: [0, 0, 20, 53, 73, 58, 73, 105, 148, 170, 123, 147, 190, 242, 255],
              RGB: [100, 0, 0, 85, 172, 0, 97, 21, 32, 131, 152, 32, 91, 78, 124, 223, 113, 124, 88, 108, 170, 255, 93, 170],
              RGBA: [0, 0, 0, 0, 255, 71, 0, 39, 255, 99, 49, 128, 60, 177, 206, 216, 0, 211, 255, 255] },
            { Name: "LANCZOS", Filter: 1,
              L: [0, 0, 11, 46, 63, 58, 69, 105, 152, 170, 130, 148, 199, 255, 255],
              RGB: [110, 0, 0, 81, 178, 0, 103, 25, 34, 141, 153, 34, 92, 82, 114, 221, 121, 114, 85, 126, 174, 255, 96, 174],
              RGBA: [0, 0, 5, 0, 255, 8, 0, 29, 255, 99, 49, 128, 62, 176, 205, 226, 0, 215, 255, 255] },
        ]

        for item in cases {
            lOut := PillowCImageResize(l, 5, 3, item.Filter)
            rgbOut := PillowCImageResize(rgb, 2, 4, item.Filter)
            rgbaOut := PillowCImageResize(rgba, 5, 1, item.Filter)
            outputs.Push(lOut, rgbOut, rgbaOut)
            AhkTest.AssertEqual(item.L, PillowCImageToArray(lOut, 15), item.Name " L")
            AhkTest.AssertEqual(item.RGB, PillowCImageToArray(rgbOut, 24), item.Name " RGB")
            AhkTest.AssertEqual(item.RGBA, PillowCImageToArray(rgbaOut, 20), item.Name " RGBA")
        }

        identityOut := PillowCImageResize(rgbaIdentity, 1, 1, 1)
        outputs.Push(identityOut)
        AhkTest.AssertEqual([141, 169, 37, 98], PillowCImageToArray(identityOut, 4), "RGBA identity")
    } finally {
        for handle in outputs
            PillowCFreeImage(handle)
        PillowCFreeImage(rgbaIdentity)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image resize advanced filters match Pillow sampling", PillowCTestImageResizeAdvancedFiltersMatchPillowSampling)

PillowCTestImageResizeNearestIntoReusesTargetHandle(*) {
    source := PillowCCreateImageMode(2, 2, 1)
    target := PillowCCreateImageMode(4, 4, 1)
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4])
        before := PillowCImageData(target).Ptr
        PillowCImageResizeInto(source, 4, 4, 0, target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([
            1, 1, 2, 2,
            1, 1, 2, 2,
            3, 3, 4, 4,
            3, 3, 4, 4,
        ], PillowCImageToArray(target, 16))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image resize_into NEAREST reuses target handle storage", PillowCTestImageResizeNearestIntoReusesTargetHandle)

PillowCTestImageResizeBilinearIntoReusesTargetHandle(*) {
    source := PillowCCreateImageMode(2, 2, 1)
    target := PillowCCreateImageMode(3, 3, 1)
    try {
        PillowCImageSetBytes(source, [0, 64, 128, 255])
        before := PillowCImageData(target).Ptr
        PillowCImageResizeInto(source, 3, 3, 2, target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([
            0, 32, 64,
            64, 112, 160,
            128, 192, 255,
        ], PillowCImageToArray(target, 9))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image resize_into BILINEAR reuses target handle storage", PillowCTestImageResizeBilinearIntoReusesTargetHandle)

PillowCTestImageResizeRejectsUnsupportedResampleAndInvalidSize(*) {
    source := PillowCCreateImageMode(2, 2, 1)
    outHandle := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_resize",
            "Ptr", source,
            "Int", 4,
            "Int", 4,
            "Int", 99,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_resize",
            "Ptr", source,
            "Int", 0,
            "Int", 1,
            "Int", 0,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image resize rejects unsupported resample and invalid output size", PillowCTestImageResizeRejectsUnsupportedResampleAndInvalidSize)

PillowCTestImageReduceMatchesPillowLAndRgb(*) {
    l := PillowCCreateImageMode(5, 3, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    lOut := 0
    rgbOut := 0
    try {
        PillowCImageSetBytes(l, [
            0, 1, 2, 3, 4,
            5, 6, 7, 8, 9,
            10, 11, 12, 13, 14,
        ])
        PillowCImageSetBytes(rgb, [
            10, 20, 30, 40, 50, 60, 70, 80, 90,
            100, 110, 120, 130, 140, 150, 160, 170, 180,
        ])

        lOut := PillowCImageReduce(l, 2, 2, 0, 0, 5, 3)
        rgbOut := PillowCImageReduce(rgb, 2, 2, 0, 0, 3, 2)

        AhkTest.AssertEqual([3, 2], [PillowCImageInt(lOut, "pillow_c_image_width"), PillowCImageInt(lOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([3, 5, 7, 11, 13, 14], PillowCImageToArray(lOut, 6))
        AhkTest.AssertEqual([2, 1], [PillowCImageInt(rgbOut, "pillow_c_image_width"), PillowCImageInt(rgbOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([70, 80, 90, 115, 125, 135], PillowCImageToArray(rgbOut, 6))
    } finally {
        if rgbOut
            PillowCFreeImage(rgbOut)
        if lOut
            PillowCFreeImage(lOut)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image reduce matches Pillow L and RGB", PillowCTestImageReduceMatchesPillowLAndRgb)

PillowCTestImageReduceSupportsBoxFactorTupleAndInto(*) {
    l := PillowCCreateImageMode(5, 3, 1)
    target := PillowCCreateImageMode(3, 1, 1)
    boxed := 0
    try {
        PillowCImageSetBytes(l, [
            0, 1, 2, 3, 4,
            5, 6, 7, 8, 9,
            10, 11, 12, 13, 14,
        ])

        boxed := PillowCImageReduce(l, 2, 2, 1, 0, 5, 3)
        AhkTest.AssertEqual([2, 2], [PillowCImageInt(boxed, "pillow_c_image_width"), PillowCImageInt(boxed, "pillow_c_image_height")])
        AhkTest.AssertEqual([4, 6, 12, 14], PillowCImageToArray(boxed, 4))

        before := PillowCImageData(target).Ptr
        PillowCImageReduceInto(l, 2, 3, 0, 0, 5, 3, target)
        AhkTest.AssertEqual(before, PillowCImageData(target).Ptr)
        AhkTest.AssertEqual([5, 7, 9], PillowCImageToArray(target, 3))
    } finally {
        if boxed
            PillowCFreeImage(boxed)
        PillowCFreeImage(target)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image reduce supports box factor tuple and _into", PillowCTestImageReduceSupportsBoxFactorTupleAndInto)

PillowCTestImageReduceMatchesPillowAlphaModes(*) {
    rgba := PillowCCreateImageMode(2, 2, 4)
    rgbaWide := PillowCCreateImageMode(3, 2, 4)
    la := PillowCCreateImageMode(3, 2, 2)
    rgbaOut := 0
    wideOut := 0
    boxOut := 0
    laOut := 0
    try {
        PillowCImageSetBytes(rgba, [
            10, 20, 30, 0, 100, 110, 120, 128,
            200, 210, 220, 255, 50, 60, 70, 64,
        ])
        PillowCImageSetBytes(rgbaWide, [
            10, 20, 30, 0, 100, 110, 120, 128, 200, 210, 220, 255,
            50, 60, 70, 64, 30, 40, 50, 32, 240, 230, 220, 200,
        ])
        PillowCImageSetBytes(la, [
            10, 0, 100, 128, 200, 255,
            50, 64, 30, 32, 240, 200,
        ])

        rgbaOut := PillowCImageReduce(rgba, 2, 2, 0, 0, 2, 2)
        wideOut := PillowCImageReduce(rgbaWide, 2, 2, 0, 0, 3, 2)
        boxOut := PillowCImageReduce(rgbaWide, 2, 2, 1, 0, 3, 2)
        laOut := PillowCImageReduce(la, 2, 1, 0, 0, 3, 2)

        AhkTest.AssertEqual([150, 159, 170, 112], PillowCImageToArray(rgbaOut, 4))
        AhkTest.AssertEqual([77, 86, 95, 56, 216, 218, 220, 228], PillowCImageToArray(wideOut, 8))
        AhkTest.AssertEqual([183, 187, 190, 154], PillowCImageToArray(boxOut, 4))
        AhkTest.AssertEqual([99, 64, 200, 255, 47, 48, 239, 200], PillowCImageToArray(laOut, 8))
    } finally {
        if laOut
            PillowCFreeImage(laOut)
        if boxOut
            PillowCFreeImage(boxOut)
        if wideOut
            PillowCFreeImage(wideOut)
        if rgbaOut
            PillowCFreeImage(rgbaOut)
        PillowCFreeImage(la)
        PillowCFreeImage(rgbaWide)
        PillowCFreeImage(rgba)
    }
}

AhkTest.Test("pillow_c image reduce matches Pillow alpha modes", PillowCTestImageReduceMatchesPillowAlphaModes)

PillowCTestImageReduceMatchesPillowCmyk(*) {
    cmyk := PillowCCreateImageMode(4, 4, 7)
    target := PillowCCreateImageMode(2, 2, 7)
    factor2 := 0
    factorTuple := 0
    boxFactor := 0
    try {
        PillowCImageSetBytes(cmyk, [
            0, 0, 0, 0, 20, 40, 60, 80, 100, 50, 25, 125, 255, 128, 64, 32,
            10, 200, 30, 40, 200, 10, 220, 0, 40, 50, 60, 70, 80, 90, 100, 110,
            120, 130, 140, 150, 160, 170, 180, 190, 200, 210, 220, 230, 240, 250, 5, 15,
            25, 35, 45, 55, 65, 75, 85, 95, 105, 115, 125, 135, 145, 155, 165, 175,
        ])

        factor2 := PillowCImageReduce(cmyk, 2, 2, 0, 0, 4, 4)
        factorTuple := PillowCImageReduce(cmyk, 2, 1, 0, 0, 4, 4)
        boxFactor := PillowCImageReduce(cmyk, 2, 2, 1, 1, 4, 4)
        before := PillowCImageData(target).Ptr
        PillowCImageReduceInto(cmyk, 2, 2, 0, 0, 4, 4, target)

        AhkTest.AssertEqual(7, PillowCImageMode(factor2))
        AhkTest.AssertEqual([2, 2], [PillowCImageInt(factor2, "pillow_c_image_width"), PillowCImageInt(factor2, "pillow_c_image_height")])
        AhkTest.AssertEqual([58, 63, 78, 30, 119, 80, 62, 84, 93, 103, 113, 123, 173, 183, 129, 139], PillowCImageToArray(factor2, 16))
        AhkTest.AssertEqual([10, 20, 30, 40, 178, 89, 45, 79, 105, 105, 125, 20, 60, 70, 80, 90, 140, 150, 160, 170, 220, 230, 113, 123, 45, 55, 65, 75, 125, 135, 145, 155], PillowCImageToArray(factorTuple, 32))
        AhkTest.AssertEqual([150, 110, 170, 123, 160, 170, 53, 63, 85, 95, 105, 115, 145, 155, 165, 175], PillowCImageToArray(boxFactor, 16))
        AhkTest.AssertEqual(before, PillowCImageData(target).Ptr)
        AhkTest.AssertEqual([58, 63, 78, 30, 119, 80, 62, 84, 93, 103, 113, 123, 173, 183, 129, 139], PillowCImageToArray(target, 16))
    } finally {
        if boxFactor
            PillowCFreeImage(boxFactor)
        if factorTuple
            PillowCFreeImage(factorTuple)
        if factor2
            PillowCFreeImage(factor2)
        PillowCFreeImage(target)
        PillowCFreeImage(cmyk)
    }
}

AhkTest.Test("pillow_c image reduce matches Pillow CMYK", PillowCTestImageReduceMatchesPillowCmyk)

PillowCTestImageReduceRejectsInvalidArguments(*) {
    source := PillowCCreateImageMode(2, 2, 1)
    target := PillowCCreateImageMode(2, 1, 1)
    out := 0
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4])
        cases := [
            { Args: [0, 1, 0, 0, 2, 2], Status: -3 },
            { Args: [1, 0, 0, 0, 2, 2], Status: -3 },
            { Args: [2, 2, -1, 0, 2, 2], Status: -3 },
            { Args: [2, 2, 0, 0, 3, 2], Status: -3 },
            { Args: [2, 2, 1, 1, 1, 2], Status: -3 },
        ]
        for item in cases {
            status := DllCall(
                PillowCDllPath() "\pillow_c_image_reduce",
                "Ptr", source,
                "Int", item.Args[1],
                "Int", item.Args[2],
                "Int", item.Args[3],
                "Int", item.Args[4],
                "Int", item.Args[5],
                "Int", item.Args[6],
                "Ptr*", &out,
                "Int"
            )
            AhkTest.AssertEqual(item.Status, status)
            AhkTest.AssertEqual(0, out)
        }

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_reduce_into",
            "Ptr", source,
            "Int", 2,
            "Int", 2,
            "Int", 0,
            "Int", 0,
            "Int", 2,
            "Int", 2,
            "Ptr", target,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        if out
            PillowCFreeImage(out)
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image reduce rejects invalid arguments", PillowCTestImageReduceRejectsInvalidArguments)

PillowCTestImageFilterKernelLMatchesPillowEdgesRoundingAndOrientation(*) {
    source := PillowCCreateImageMode(5, 5, 1)
    scaled := 0
    offset := 0
    topLeft := 0
    bottomLeft := 0
    try {
        PillowCImageSetBytes(source, [
            0, 1, 2, 3, 4,
            5, 6, 7, 8, 9,
            10, 11, 12, 13, 14,
            15, 16, 17, 18, 19,
            20, 21, 22, 23, 24,
        ])

        scaled := PillowCImageFilterKernel(source, 3, 3, [0, 0, 0, 0, 1, 0, 0, 0, 0], 2.0, 0.0)
        offset := PillowCImageFilterKernel(source, 3, 3, [0, 0, 0, 0, 1, 0, 0, 0, 0], 2.0, 0.5)
        topLeft := PillowCImageFilterKernel(source, 3, 3, [1, 0, 0, 0, 0, 0, 0, 0, 0], 1.0, 0.0)
        bottomLeft := PillowCImageFilterKernel(source, 3, 3, [0, 0, 0, 0, 0, 0, 1, 0, 0], 1.0, 0.0)

        AhkTest.AssertEqual([
            0, 1, 2, 3, 4,
            5, 3, 4, 4, 9,
            10, 6, 6, 7, 14,
            15, 8, 9, 9, 19,
            20, 21, 22, 23, 24,
        ], PillowCImageToArray(scaled, 25))
        AhkTest.AssertEqual([
            0, 1, 2, 3, 4,
            5, 4, 4, 5, 9,
            10, 6, 7, 7, 14,
            15, 9, 9, 10, 19,
            20, 21, 22, 23, 24,
        ], PillowCImageToArray(offset, 25))
        AhkTest.AssertEqual([
            0, 1, 2, 3, 4,
            5, 10, 11, 12, 9,
            10, 15, 16, 17, 14,
            15, 20, 21, 22, 19,
            20, 21, 22, 23, 24,
        ], PillowCImageToArray(topLeft, 25))
        AhkTest.AssertEqual([
            0, 1, 2, 3, 4,
            5, 0, 1, 2, 9,
            10, 5, 6, 7, 14,
            15, 10, 11, 12, 19,
            20, 21, 22, 23, 24,
        ], PillowCImageToArray(bottomLeft, 25))
    } finally {
        for handle in [bottomLeft, topLeft, offset, scaled, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter kernel matches Pillow L edges rounding and vertical flip", PillowCTestImageFilterKernelLMatchesPillowEdgesRoundingAndOrientation)

PillowCTestImageFilterKernelRgbRgbaAndFiveByFiveMatchPillow(*) {
    rgb := PillowCCreateImageMode(4, 3, 3)
    rgba := PillowCCreateImageMode(3, 3, 4)
    l7 := PillowCCreateImageMode(7, 7, 1)
    rgbOut := 0
    rgbaOut := 0
    l7Out := 0
    try {
        PillowCImageSetBytes(rgb, [
            1, 2, 3, 10, 20, 30, 40, 50, 60, 70, 80, 90,
            90, 80, 70, 120, 110, 100, 150, 140, 130, 180, 170, 160,
            5, 15, 25, 35, 45, 55, 65, 75, 85, 95, 105, 115,
        ])
        PillowCImageSetBytes(rgba, [
            1, 2, 3, 4, 10, 20, 30, 40, 50, 60, 70, 80,
            90, 80, 70, 60, 120, 110, 100, 90, 150, 140, 130, 120,
            5, 15, 25, 35, 35, 45, 55, 65, 65, 75, 85, 95,
        ])
        PillowCImageSetBytes(l7, [
            0, 11, 44, 99, 176, 19, 140,
            78, 102, 148, 216, 50, 162, 40,
            156, 193, 252, 77, 180, 49, 196,
            111, 161, 233, 71, 187, 69, 229,
            189, 252, 81, 188, 61, 212, 129,
            11, 87, 185, 49, 191, 99, 29,
            222, 55, 166, 43, 198, 119, 62,
        ])

        rgbOut := PillowCImageFilterKernel(rgb, 3, 3, [0, -1, 0, -1, 5, -1, 0, -1, 0], 1.0, 0.0)
        rgbaOut := PillowCImageFilterKernel(rgba, 3, 3, [0, -1, 0, -1, 5, -1, 0, -1, 0], 1.0, 0.0)
        l7Out := PillowCImageFilterKernel(l7, 5, 5, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], 2.0, 0.5)

        AhkTest.AssertEqual([
            1, 2, 3, 10, 20, 30, 40, 50, 60, 70, 80, 90,
            90, 80, 70, 255, 255, 215, 255, 255, 245, 180, 170, 160,
            5, 15, 25, 35, 45, 55, 65, 75, 85, 95, 105, 115,
        ], PillowCImageToArray(rgbOut, 36))
        AhkTest.AssertEqual([
            1, 2, 3, 4, 10, 20, 30, 40, 50, 60, 70, 80,
            90, 80, 70, 60, 255, 255, 215, 165, 150, 140, 130, 120,
            5, 15, 25, 35, 35, 45, 55, 65, 65, 75, 85, 95,
        ], PillowCImageToArray(rgbaOut, 36))
        AhkTest.AssertEqual([
            0, 11, 44, 99, 176, 19, 140,
            78, 102, 148, 216, 50, 162, 40,
            156, 193, 127, 39, 91, 49, 196,
            111, 161, 117, 36, 94, 69, 229,
            189, 252, 41, 95, 31, 212, 129,
            11, 87, 185, 49, 191, 99, 29,
            222, 55, 166, 43, 198, 119, 62,
        ], PillowCImageToArray(l7Out, 49))
    } finally {
        for handle in [l7Out, rgbaOut, rgbOut, l7, rgba, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter kernel matches Pillow RGB RGBA and 5x5 output", PillowCTestImageFilterKernelRgbRgbaAndFiveByFiveMatchPillow)

PillowCTestImageFilterKernelIntoReuseAndRejectInvalidArguments(*) {
    source := PillowCCreateImageMode(5, 4, 1)
    target := PillowCCreateImageMode(5, 4, 1)
    wrongTarget := PillowCCreateImageMode(4, 4, 1)
    wrongModeTarget := PillowCCreateImageMode(5, 4, 3)
    outHandle := 0
    try {
        PillowCImageSetBytes(source, [
            0, 1, 2, 3, 4,
            5, 6, 7, 8, 9,
            10, 11, 12, 13, 14,
            15, 16, 17, 18, 19,
        ])
        before := PillowCImageData(target).Ptr
        PillowCImageFilterKernelInto(source, 3, 3, [-1, -1, -1, -1, 8, -1, -1, -1, -1], 1.0, 128.0, target)
        AhkTest.AssertEqual(before, PillowCImageData(target).Ptr)
        AhkTest.AssertEqual([
            0, 1, 2, 3, 4,
            5, 128, 128, 128, 9,
            10, 128, 128, 128, 14,
            15, 16, 17, 18, 19,
        ], PillowCImageToArray(target, 20))

        kernel4 := PillowCKernelWeights([1, 0, 0, 0])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_filter_kernel",
            "Ptr", source,
            "Int", 2,
            "Int", 2,
            "Ptr", kernel4,
            "UPtr", 4,
            "Double", 1.0,
            "Double", 0.0,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        kernelShort := PillowCKernelWeights([1, 0, 0, 0, 1, 0, 0, 0])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_filter_kernel",
            "Ptr", source,
            "Int", 3,
            "Int", 3,
            "Ptr", kernelShort,
            "UPtr", 8,
            "Double", 1.0,
            "Double", 0.0,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-2, status)
        AhkTest.AssertEqual(0, outHandle)

        kernel := PillowCKernelWeights([0, 0, 0, 0, 1, 0, 0, 0, 0])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_filter_kernel_into",
            "Ptr", source,
            "Int", 3,
            "Int", 3,
            "Ptr", kernel,
            "UPtr", 9,
            "Double", 1.0,
            "Double", 0.0,
            "Ptr", wrongTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_filter_kernel_into",
            "Ptr", source,
            "Int", 3,
            "Int", 3,
            "Ptr", kernel,
            "UPtr", 9,
            "Double", 1.0,
            "Double", 0.0,
            "Ptr", wrongModeTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        for handle in [wrongModeTarget, wrongTarget, target, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter kernel_into reuses storage and rejects invalid arguments", PillowCTestImageFilterKernelIntoReuseAndRejectInvalidArguments)

PillowCTestImageFilterRankMatchesPillowLAndClampsEdges(*) {
    source := PillowCCreateImageMode(5, 5, 1)
    minOut := 0
    medianOut := 0
    maxOut := 0
    rank7Out := 0
    try {
        PillowCImageSetBytes(source, [
            0, 200, 2, 180, 4,
            50, 6, 70, 8, 90,
            10, 110, 12, 130, 14,
            150, 16, 170, 18, 190,
            20, 210, 22, 230, 24,
        ])

        minOut := PillowCImageFilterRank(source, 3, 0)
        medianOut := PillowCImageFilterRank(source, 3, 4)
        maxOut := PillowCImageFilterRank(source, 3, 8)
        rank7Out := PillowCImageFilterRank(source, 7, 24)

        AhkTest.AssertEqual([
            0, 0, 2, 2, 4,
            0, 0, 2, 2, 4,
            6, 6, 6, 8, 8,
            10, 10, 12, 12, 14,
            16, 16, 16, 18, 18,
        ], PillowCImageToArray(minOut, 25))
        AhkTest.AssertEqual([
            6, 6, 70, 8, 8,
            10, 12, 70, 14, 14,
            50, 50, 18, 70, 90,
            20, 22, 110, 24, 24,
            20, 22, 170, 24, 24,
        ], PillowCImageToArray(medianOut, 25))
        AhkTest.AssertEqual([
            200, 200, 200, 180, 180,
            200, 200, 200, 180, 180,
            150, 170, 170, 190, 190,
            210, 210, 230, 230, 230,
            210, 210, 230, 230, 230,
        ], PillowCImageToArray(maxOut, 25))
        AhkTest.AssertEqual([
            10, 10, 12, 14, 14,
            20, 20, 20, 20, 22,
            20, 20, 22, 24, 24,
            20, 22, 24, 24, 24,
            20, 22, 24, 24, 24,
        ], PillowCImageToArray(rank7Out, 25))
    } finally {
        for handle in [rank7Out, maxOut, medianOut, minOut, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter rank matches Pillow L and clamps edges", PillowCTestImageFilterRankMatchesPillowLAndClampsEdges)

PillowCTestImageFilterRankMatchesPillowRgbRgbaAndInto(*) {
    rgb := PillowCCreateImageMode(4, 3, 3)
    rgba := PillowCCreateImageMode(3, 3, 4)
    rgbTarget := PillowCCreateImageMode(4, 3, 3)
    rgbMedian := 0
    rgbaMedian := 0
    try {
        PillowCImageSetBytes(rgb, [
            1, 2, 3, 10, 20, 30, 40, 50, 60, 70, 80, 90,
            90, 80, 70, 120, 110, 100, 150, 140, 130, 180, 170, 160,
            5, 15, 25, 35, 45, 55, 65, 75, 85, 95, 105, 115,
        ])
        PillowCImageSetBytes(rgba, [
            1, 2, 3, 4, 10, 20, 30, 40, 50, 60, 70, 80,
            90, 80, 70, 60, 120, 110, 100, 90, 150, 140, 130, 120,
            5, 15, 25, 35, 35, 45, 55, 65, 65, 75, 85, 95,
        ])
        before := PillowCImageData(rgbTarget).Ptr

        rgbMedian := PillowCImageFilterRank(rgb, 3, 4)
        rgbaMedian := PillowCImageFilterRank(rgba, 3, 4)
        PillowCImageFilterRankInto(rgb, 3, 4, rgbTarget)

        AhkTest.AssertEqual(before, PillowCImageData(rgbTarget).Ptr)
        AhkTest.AssertEqual([
            10, 20, 30, 40, 50, 60, 70, 80, 90, 70, 80, 90,
            10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 105, 115,
            35, 45, 55, 65, 75, 70, 95, 105, 100, 95, 105, 115,
        ], PillowCImageToArray(rgbMedian, 36))
        AhkTest.AssertEqual(PillowCImageToArray(rgbMedian, 36), PillowCImageToArray(rgbTarget, 36))
        AhkTest.AssertEqual([
            10, 20, 30, 40, 50, 60, 70, 60, 50, 60, 70, 80,
            10, 20, 30, 40, 50, 60, 70, 65, 65, 75, 85, 90,
            35, 45, 55, 60, 65, 75, 70, 65, 65, 75, 85, 95,
        ], PillowCImageToArray(rgbaMedian, 36))
    } finally {
        for handle in [rgbaMedian, rgbMedian, rgbTarget, rgba, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter rank matches Pillow RGB RGBA and _into", PillowCTestImageFilterRankMatchesPillowRgbRgbaAndInto)

PillowCTestImageFilterRankRejectsInvalidArguments(*) {
    source := PillowCCreateImageMode(3, 3, 1)
    target := PillowCCreateImageMode(3, 3, 1)
    wrongTarget := PillowCCreateImageMode(3, 3, 3)
    outHandle := 0
    try {
        PillowCImageSetBytes(source, [0, 1, 2, 3, 4, 5, 6, 7, 8])

        for item in [
            { Size: 0, Rank: -1 },
            { Size: 2, Rank: 0 },
            { Size: 3, Rank: -1 },
            { Size: 3, Rank: 9 },
        ] {
            status := DllCall(
                PillowCDllPath() "\pillow_c_image_filter_rank",
                "Ptr", source,
                "Int", item.Size,
                "Int", item.Rank,
                "Ptr*", &outHandle,
                "Int"
            )
            AhkTest.AssertEqual(-3, status)
            AhkTest.AssertEqual(0, outHandle)
        }

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_filter_rank_into",
            "Ptr", source,
            "Int", 3,
            "Int", 4,
            "Ptr", wrongTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)

        PillowCImageFilterRankInto(source, 1, 0, target)
        AhkTest.AssertEqual([0, 1, 2, 3, 4, 5, 6, 7, 8], PillowCImageToArray(target, 9))
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        for handle in [wrongTarget, target, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter rank rejects invalid arguments", PillowCTestImageFilterRankRejectsInvalidArguments)

PillowCTestImageFilterModeMatchesPillowLWindowRules(*) {
    edge := PillowCCreateImageMode(3, 3, 1)
    tie := PillowCCreateImageMode(3, 3, 1)
    threshold := PillowCCreateImageMode(3, 3, 1)
    identity := 0
    negative := 0
    size2 := 0
    size4 := 0
    tieOut := 0
    thresholdOut := 0
    try {
        PillowCImageSetBytes(edge, [
            0, 0, 0,
            0, 0, 0,
            1, 1, 1,
        ])
        PillowCImageSetBytes(tie, [
            1, 1, 1,
            2, 2, 2,
            3, 4, 5,
        ])
        PillowCImageSetBytes(threshold, [
            1, 1, 2,
            2, 3, 4,
            5, 6, 7,
        ])

        identity := PillowCImageFilterMode(edge, 1)
        negative := PillowCImageFilterMode(edge, -3)
        size2 := PillowCImageFilterMode(edge, 2)
        size4 := PillowCImageFilterMode(edge, 4)
        tieOut := PillowCImageFilterMode(tie, 3)
        thresholdOut := PillowCImageFilterMode(threshold, 3)

        AhkTest.AssertEqual([
            0, 0, 0,
            0, 0, 0,
            1, 1, 1,
        ], PillowCImageToArray(identity, 9))
        AhkTest.AssertEqual(PillowCImageToArray(identity, 9), PillowCImageToArray(negative, 9))
        AhkTest.AssertEqual([
            0, 0, 0,
            0, 0, 0,
            1, 0, 1,
        ], PillowCImageToArray(size2, 9))
        AhkTest.AssertEqual([
            0, 0, 0,
            0, 0, 0,
            0, 0, 0,
        ], PillowCImageToArray(size4, 9))
        AhkTest.AssertEqual([
            1, 1, 1,
            2, 1, 2,
            3, 2, 5,
        ], PillowCImageToArray(tieOut, 9))
        AhkTest.AssertEqual([
            1, 1, 2,
            2, 3, 4,
            5, 6, 7,
        ], PillowCImageToArray(thresholdOut, 9))
    } finally {
        for handle in [thresholdOut, tieOut, size4, size2, negative, identity, threshold, tie, edge] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter mode matches Pillow L window rules", PillowCTestImageFilterModeMatchesPillowLWindowRules)

PillowCTestImageFilterModeMatchesPillowRgbRgbaAndInto(*) {
    rgb := PillowCCreateImageMode(3, 3, 3)
    rgba := PillowCCreateImageMode(3, 3, 4)
    rgbTarget := PillowCCreateImageMode(3, 3, 3)
    wrongTarget := PillowCCreateImageMode(3, 3, 1)
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(rgb, [
            0, 10, 200, 0, 20, 200, 0, 30, 100,
            0, 40, 200, 9, 50, 100, 9, 60, 100,
            1, 70, 200, 1, 80, 100, 1, 90, 100,
        ])
        PillowCImageSetBytes(rgba, [
            0, 10, 200, 1, 0, 20, 200, 2, 0, 30, 100, 3,
            0, 40, 200, 4, 9, 50, 100, 5, 9, 60, 100, 6,
            1, 70, 200, 7, 1, 80, 100, 8, 1, 90, 100, 9,
        ])
        before := PillowCImageData(rgbTarget).Ptr

        rgbOut := PillowCImageFilterMode(rgb, 3)
        rgbaOut := PillowCImageFilterMode(rgba, 3)
        PillowCImageFilterModeInto(rgb, 3, rgbTarget)

        AhkTest.AssertEqual(before, PillowCImageData(rgbTarget).Ptr)
        AhkTest.AssertEqual([
            0, 10, 200, 0, 20, 100, 0, 30, 100,
            0, 40, 200, 0, 50, 100, 9, 60, 100,
            1, 70, 200, 1, 80, 100, 1, 90, 100,
        ], PillowCImageToArray(rgbOut, 27))
        AhkTest.AssertEqual(PillowCImageToArray(rgbOut, 27), PillowCImageToArray(rgbTarget, 27))
        AhkTest.AssertEqual([
            0, 10, 200, 1, 0, 20, 100, 2, 0, 30, 100, 3,
            0, 40, 200, 4, 0, 50, 100, 5, 9, 60, 100, 6,
            1, 70, 200, 7, 1, 80, 100, 8, 1, 90, 100, 9,
        ], PillowCImageToArray(rgbaOut, 36))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_filter_mode_into",
            "Ptr", rgb,
            "Int", 3,
            "Ptr", wrongTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        for handle in [rgbaOut, rgbOut, wrongTarget, rgbTarget, rgba, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter mode matches Pillow RGB RGBA and _into", PillowCTestImageFilterModeMatchesPillowRgbRgbaAndInto)

PillowCTestImageFilterBoxBlurMatchesPillowLRadiusVariants(*) {
    source := PillowCCreateImageMode(4, 3, 1)
    radius0 := 0
    r05 := 0
    r1 := 0
    r125 := 0
    horizontal := 0
    vertical := 0
    mixed := 0
    try {
        PillowCImageSetBytes(source, [
            0, 30, 80, 120,
            160, 200, 220, 255,
            10, 40, 90, 140,
        ])

        radius0 := PillowCImageFilterBoxBlur(source, 0, 0)
        r05 := PillowCImageFilterBoxBlur(source, 0.5, 0.5)
        r1 := PillowCImageFilterBoxBlur(source, 1, 1)
        r125 := PillowCImageFilterBoxBlur(source, 1.25, 1.25)
        horizontal := PillowCImageFilterBoxBlur(source, 1, 0)
        vertical := PillowCImageFilterBoxBlur(source, 0, 1)
        mixed := PillowCImageFilterBoxBlur(source, 1.5, 0.5)

        AhkTest.AssertEqual([
            0, 30, 80, 120,
            160, 200, 220, 255,
            10, 40, 90, 140,
        ], PillowCImageToArray(radius0, 12))
        AhkTest.AssertEqual([49, 75, 115, 144, 92, 118, 154, 183, 56, 83, 124, 158], PillowCImageToArray(r05, 12))
        AhkTest.AssertEqual([64, 89, 126, 152, 68, 92, 131, 158, 71, 96, 135, 163], PillowCImageToArray(r1, 12))
        AhkTest.AssertEqual([61, 85, 117, 143, 64, 88, 121, 148, 67, 91, 125, 153], PillowCImageToArray(r125, 12))
        AhkTest.AssertEqual([10, 37, 77, 107, 173, 193, 225, 243, 20, 47, 90, 123], PillowCImageToArray(horizontal, 12))
        AhkTest.AssertEqual([53, 87, 127, 165, 57, 90, 130, 172, 60, 93, 133, 178], PillowCImageToArray(vertical, 12))
        AhkTest.AssertEqual([58, 82, 110, 134, 101, 123, 150, 173, 66, 90, 120, 146], PillowCImageToArray(mixed, 12))
    } finally {
        for handle in [mixed, vertical, horizontal, r125, r1, r05, radius0, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter box blur matches Pillow L radius variants", PillowCTestImageFilterBoxBlurMatchesPillowLRadiusVariants)

PillowCTestImageFilterBoxBlurMatchesPillowRgbRgbaAndInto(*) {
    rgb := PillowCCreateImageMode(4, 2, 3)
    rgba := PillowCCreateImageMode(3, 2, 4)
    rgbTarget := PillowCCreateImageMode(4, 2, 3)
    wrongTarget := PillowCCreateImageMode(4, 2, 1)
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(rgb, [
            1, 2, 3, 20, 30, 40, 60, 70, 80, 100, 110, 120,
            130, 140, 150, 160, 170, 180, 200, 210, 220, 230, 240, 250,
        ])
        PillowCImageSetBytes(rgba, [
            1, 2, 3, 4, 20, 30, 40, 50, 60, 70, 80, 90,
            100, 110, 120, 130, 140, 150, 160, 170, 200, 210, 220, 230,
        ])
        before := PillowCImageData(rgbTarget).Ptr

        rgbOut := PillowCImageFilterBoxBlur(rgb, 1.25, 0.5)
        rgbaOut := PillowCImageFilterBoxBlur(rgba, 1.25, 0.5)
        PillowCImageFilterBoxBlurInto(rgb, 1.25, 0.5, rgbTarget)

        AhkTest.AssertEqual(before, PillowCImageData(rgbTarget).Ptr)
        AhkTest.AssertEqual([
            44, 50, 55, 64, 72, 80, 93, 102, 111, 116, 126, 136,
            111, 119, 128, 132, 141, 151, 160, 170, 180, 183, 193, 203,
        ], PillowCImageToArray(rgbOut, 24))
        AhkTest.AssertEqual(PillowCImageToArray(rgbOut, 24), PillowCImageToArray(rgbTarget, 24))
        AhkTest.AssertEqual([
            38, 44, 49, 55, 58, 65, 73, 80, 77, 87, 96, 106,
            92, 101, 109, 118, 117, 126, 136, 145, 143, 153, 163, 173,
        ], PillowCImageToArray(rgbaOut, 24))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_filter_box_blur_into",
            "Ptr", rgb,
            "Double", 1.0,
            "Double", 1.0,
            "Ptr", wrongTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        for handle in [rgbaOut, rgbOut, wrongTarget, rgbTarget, rgba, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter box blur matches Pillow RGB RGBA and _into", PillowCTestImageFilterBoxBlurMatchesPillowRgbRgbaAndInto)

PillowCTestImageFilterBoxBlurRejectsInvalidRadius(*) {
    source := PillowCCreateImageMode(1, 1, 1)
    outHandle := 0
    try {
        PillowCImageSetBytes(source, [5])
        for radius in [-1.0, -0.5] {
            status := DllCall(
                PillowCDllPath() "\pillow_c_image_filter_box_blur",
                "Ptr", source,
                "Double", radius,
                "Double", 0.0,
                "Ptr*", &outHandle,
                "Int"
            )
            AhkTest.AssertEqual(-3, status)
            AhkTest.AssertEqual(0, outHandle)
        }
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image filter box blur rejects invalid radius", PillowCTestImageFilterBoxBlurRejectsInvalidRadius)

PillowCTestImageFilterGaussianBlurMatchesPillowLRadiusVariants(*) {
    source := PillowCCreateImageMode(4, 3, 1)
    radius0 := 0
    r05 := 0
    r1 := 0
    r125 := 0
    horizontal := 0
    vertical := 0
    mixed := 0
    r2 := 0
    try {
        PillowCImageSetBytes(source, [
            0, 30, 80, 120,
            160, 200, 220, 255,
            10, 40, 90, 140,
        ])

        radius0 := PillowCImageFilterGaussianBlur(source, 0, 0)
        r05 := PillowCImageFilterGaussianBlur(source, 0.5, 0.5)
        r1 := PillowCImageFilterGaussianBlur(source, 1, 1)
        r125 := PillowCImageFilterGaussianBlur(source, 1.25, 1.25)
        horizontal := PillowCImageFilterGaussianBlur(source, 1, 0)
        vertical := PillowCImageFilterGaussianBlur(source, 0, 1)
        mixed := PillowCImageFilterGaussianBlur(source, 1.5, 0.5)
        r2 := PillowCImageFilterGaussianBlur(source, 2, 2)

        AhkTest.AssertEqual([
            0, 30, 80, 120,
            160, 200, 220, 255,
            10, 40, 90, 140,
        ], PillowCImageToArray(radius0, 12))
        AhkTest.AssertEqual([21, 51, 95, 129, 131, 163, 191, 224, 30, 60, 104, 147], PillowCImageToArray(r05, 12))
        AhkTest.AssertEqual([62, 84, 118, 143, 85, 106, 139, 164, 67, 91, 126, 153], PillowCImageToArray(r1, 12))
        AhkTest.AssertEqual([76, 96, 123, 143, 79, 99, 127, 147, 80, 100, 129, 149], PillowCImageToArray(r125, 12))
        AhkTest.AssertEqual([14, 38, 74, 102, 176, 196, 221, 240, 24, 49, 88, 119], PillowCImageToArray(horizontal, 12))
        AhkTest.AssertEqual([47, 79, 121, 160, 69, 103, 141, 182, 53, 86, 127, 172], PillowCImageToArray(vertical, 12))
        AhkTest.AssertEqual([46, 62, 86, 103, 153, 167, 187, 201, 54, 73, 98, 116], PillowCImageToArray(mixed, 12))
        AhkTest.AssertEqual([82, 94, 109, 121, 83, 95, 110, 122, 84, 96, 111, 123], PillowCImageToArray(r2, 12))
    } finally {
        for handle in [r2, mixed, vertical, horizontal, r125, r1, r05, radius0, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter GaussianBlur matches Pillow L radius variants", PillowCTestImageFilterGaussianBlurMatchesPillowLRadiusVariants)

PillowCTestImageFilterGaussianBlurMatchesPillowRgbRgbaAndInto(*) {
    rgb := PillowCCreateImageMode(4, 2, 3)
    rgba := PillowCCreateImageMode(3, 2, 4)
    rgbTarget := PillowCCreateImageMode(4, 2, 3)
    wrongTarget := PillowCCreateImageMode(4, 2, 1)
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(rgb, [
            1, 2, 3, 20, 30, 40, 60, 70, 80, 100, 110, 120,
            130, 140, 150, 160, 170, 180, 200, 210, 220, 230, 240, 250,
        ])
        PillowCImageSetBytes(rgba, [
            1, 2, 3, 4, 20, 30, 40, 50, 60, 70, 80, 90,
            100, 110, 120, 130, 140, 150, 160, 170, 200, 210, 220, 230,
        ])
        before := PillowCImageData(rgbTarget).Ptr

        rgbOut := PillowCImageFilterGaussianBlur(rgb, 1.25, 0.5)
        rgbaOut := PillowCImageFilterGaussianBlur(rgba, 1.25, 0.5)
        PillowCImageFilterGaussianBlurInto(rgb, 1.25, 0.5, rgbTarget)

        AhkTest.AssertEqual(before, PillowCImageData(rgbTarget).Ptr)
        AhkTest.AssertEqual([
            32, 36, 43, 48, 55, 63, 73, 82, 91, 92, 102, 112,
            134, 144, 153, 152, 162, 172, 176, 186, 196, 194, 204, 214,
        ], PillowCImageToArray(rgbOut, 24))
        AhkTest.AssertEqual(PillowCImageToArray(rgbOut, 24), PillowCImageToArray(rgbTarget, 24))
        AhkTest.AssertEqual([
            28, 33, 39, 44, 41, 48, 55, 63, 54, 63, 72, 81,
            113, 123, 132, 142, 132, 142, 152, 161, 152, 162, 172, 182,
        ], PillowCImageToArray(rgbaOut, 24))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_filter_gaussian_blur_into",
            "Ptr", rgb,
            "Double", 1.0,
            "Double", 1.0,
            "Ptr", wrongTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        for handle in [rgbaOut, rgbOut, wrongTarget, rgbTarget, rgba, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter GaussianBlur matches Pillow RGB RGBA and _into", PillowCTestImageFilterGaussianBlurMatchesPillowRgbRgbaAndInto)

PillowCTestImageFilterGaussianBlurRejectsOutOfRangeRadius(*) {
    source := PillowCCreateImageMode(1, 1, 1)
    outHandle := 0
    try {
        PillowCImageSetBytes(source, [5])
        for radius in [1.0e300, -1.0e300] {
            status := DllCall(
                PillowCDllPath() "\pillow_c_image_filter_gaussian_blur",
                "Ptr", source,
                "Double", radius,
                "Double", 0.0,
                "Ptr*", &outHandle,
                "Int"
            )
            AhkTest.AssertEqual(-3, status)
            AhkTest.AssertEqual(0, outHandle)
        }
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image filter GaussianBlur rejects out-of-range radius", PillowCTestImageFilterGaussianBlurRejectsOutOfRangeRadius)

PillowCTestImageFilterUnsharpMaskMatchesPillowLVariants(*) {
    source := PillowCCreateImageMode(4, 3, 1)
    defaultOut := 0
    radius0 := 0
    r05 := 0
    r1 := 0
    strong := 0
    thresholded := 0
    negativePercent := 0
    try {
        PillowCImageSetBytes(source, [
            0, 30, 80, 120,
            160, 200, 220, 255,
            10, 40, 90, 140,
        ])

        defaultOut := PillowCImageFilterUnsharpMask(source, 2, 150, 3)
        radius0 := PillowCImageFilterUnsharpMask(source, 0, 150, 3)
        r05 := PillowCImageFilterUnsharpMask(source, 0.5, 150, 3)
        r1 := PillowCImageFilterUnsharpMask(source, 1, 150, 3)
        strong := PillowCImageFilterUnsharpMask(source, 1, 200, 0)
        thresholded := PillowCImageFilterUnsharpMask(source, 1, 50, 20)
        negativePercent := PillowCImageFilterUnsharpMask(source, 1, -100, 0)

        AhkTest.AssertEqual([0, 0, 37, 120, 255, 255, 255, 255, 0, 0, 59, 165], PillowCImageToArray(defaultOut, 12))
        AhkTest.AssertEqual([
            0, 30, 80, 120,
            160, 200, 220, 255,
            10, 40, 90, 140,
        ], PillowCImageToArray(radius0, 12))
        AhkTest.AssertEqual([0, 0, 58, 107, 203, 255, 255, 255, 0, 10, 69, 130], PillowCImageToArray(r05, 12))
        AhkTest.AssertEqual([0, 0, 23, 86, 255, 255, 255, 255, 0, 0, 36, 121], PillowCImageToArray(r1, 12))
        AhkTest.AssertEqual([0, 0, 4, 74, 255, 255, 255, 255, 0, 0, 18, 114], PillowCImageToArray(strong, 12))
        AhkTest.AssertEqual([0, 3, 61, 109, 197, 247, 255, 255, 0, 15, 72, 140], PillowCImageToArray(thresholded, 12))
        AhkTest.AssertEqual([62, 84, 118, 143, 85, 106, 139, 164, 67, 91, 126, 153], PillowCImageToArray(negativePercent, 12))
    } finally {
        for handle in [negativePercent, thresholded, strong, r1, r05, radius0, defaultOut, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter UnsharpMask matches Pillow L variants", PillowCTestImageFilterUnsharpMaskMatchesPillowLVariants)

PillowCTestImageFilterUnsharpMaskMatchesPillowRgbRgbaAndInto(*) {
    rgb := PillowCCreateImageMode(4, 2, 3)
    rgba := PillowCCreateImageMode(3, 2, 4)
    rgbTarget := PillowCCreateImageMode(4, 2, 3)
    wrongTarget := PillowCCreateImageMode(4, 2, 1)
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(rgb, [
            1, 2, 3, 20, 30, 40, 60, 70, 80, 100, 110, 120,
            130, 140, 150, 160, 170, 180, 200, 210, 220, 230, 240, 250,
        ])
        PillowCImageSetBytes(rgba, [
            1, 2, 3, 4, 20, 30, 40, 50, 60, 70, 80, 90,
            100, 110, 120, 130, 140, 150, 160, 170, 200, 210, 220, 230,
        ])
        before := PillowCImageData(rgbTarget).Ptr

        rgbOut := PillowCImageFilterUnsharpMask(rgb, 1.25, 200, 0)
        rgbaOut := PillowCImageFilterUnsharpMask(rgba, 1.25, 200, 0)
        PillowCImageFilterUnsharpMaskInto(rgb, 1.25, 200, 0, rgbTarget)

        AhkTest.AssertEqual(before, PillowCImageData(rgbTarget).Ptr)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0, 28, 38, 48,
            210, 224, 238, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        ], PillowCImageToArray(rgbOut, 24))
        AhkTest.AssertEqual(PillowCImageToArray(rgbOut, 24), PillowCImageToArray(rgbTarget, 24))
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 22,
            148, 162, 176, 190, 234, 246, 255, 255, 255, 255, 255, 255,
        ], PillowCImageToArray(rgbaOut, 24))

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_filter_unsharp_mask_into",
            "Ptr", rgb,
            "Double", 1.0,
            "Int", 150,
            "Int", 3,
            "Ptr", wrongTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        for handle in [rgbaOut, rgbOut, wrongTarget, rgbTarget, rgba, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter UnsharpMask matches Pillow RGB RGBA and _into", PillowCTestImageFilterUnsharpMaskMatchesPillowRgbRgbaAndInto)

PillowCTestImageFilterUnsharpMaskRejectsOutOfRangeRadius(*) {
    source := PillowCCreateImageMode(1, 1, 1)
    outHandle := 0
    try {
        PillowCImageSetBytes(source, [5])
        for radius in [1.0e300, -1.0e300] {
            status := DllCall(
                PillowCDllPath() "\pillow_c_image_filter_unsharp_mask",
                "Ptr", source,
                "Double", radius,
                "Int", 150,
                "Int", 3,
                "Ptr*", &outHandle,
                "Int"
            )
            AhkTest.AssertEqual(-3, status)
            AhkTest.AssertEqual(0, outHandle)
        }
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image filter UnsharpMask rejects out-of-range radius", PillowCTestImageFilterUnsharpMaskRejectsOutOfRangeRadius)

PillowCTestImageFilterLaMatchesPillow(*) {
    la := PillowCCreateImageMode(3, 3, 2)
    kernelOut := 0
    minOut := 0
    medianOut := 0
    maxOut := 0
    modeOut := 0
    boxOut := 0
    gaussianOut := 0
    unsharpOut := 0
    boxTarget := PillowCCreateImageMode(3, 3, 2)
    try {
        PillowCImageSetBytes(la, [
            10, 0, 40, 64, 80, 128,
            30, 255, 120, 128, 220, 32,
            90, 64, 160, 192, 250, 255,
        ])
        before := PillowCImageData(boxTarget).Ptr

        kernelOut := PillowCImageFilterKernel(la, 3, 3, [1, 2, 1, 2, 4, 2, 1, 2, 1], 16.0, 3.0)
        minOut := PillowCImageFilterRank(la, 3, 0)
        medianOut := PillowCImageFilterRank(la, 3, 4)
        maxOut := PillowCImageFilterRank(la, 3, 8)
        modeOut := PillowCImageFilterMode(la, 3)
        boxOut := PillowCImageFilterBoxBlur(la, 1, 1)
        gaussianOut := PillowCImageFilterGaussianBlur(la, 1, 1)
        unsharpOut := PillowCImageFilterUnsharpMask(la, 1, 150, 0)
        PillowCImageFilterBoxBlurInto(la, 1, 1, boxTarget)

        AhkTest.AssertEqual(before, PillowCImageData(boxTarget).Ptr)
        AhkTest.AssertEqual([10, 0, 40, 64, 80, 128, 30, 255, 116, 131, 220, 32, 90, 64, 160, 192, 250, 255], PillowCImageToArray(kernelOut, 18))
        AhkTest.AssertEqual([10, 0, 10, 0, 40, 32, 10, 0, 10, 0, 40, 32, 30, 64, 30, 32, 120, 32], PillowCImageToArray(minOut, 18))
        AhkTest.AssertEqual([30, 64, 40, 64, 80, 128, 40, 64, 90, 128, 160, 128, 90, 128, 160, 192, 220, 192], PillowCImageToArray(medianOut, 18))
        AhkTest.AssertEqual([120, 255, 220, 255, 220, 128, 160, 255, 250, 255, 250, 255, 160, 255, 250, 255, 250, 255], PillowCImageToArray(maxOut, 18))
        AhkTest.AssertEqual([10, 0, 40, 64, 80, 128, 30, 255, 120, 128, 220, 32, 90, 64, 160, 192, 250, 255], PillowCImageToArray(modeOut, 18))
        AhkTest.AssertEqual([33, 85, 70, 89, 107, 93, 64, 114, 111, 124, 158, 135, 95, 142, 152, 159, 209, 177], PillowCImageToArray(boxOut, 18))
        AhkTest.AssertEqual(PillowCImageToArray(boxOut, 18), PillowCImageToArray(boxTarget, 18))
        AhkTest.AssertEqual([42, 84, 74, 92, 107, 100, 70, 126, 112, 126, 155, 125, 99, 134, 146, 155, 193, 171], PillowCImageToArray(gaussianOut, 18))
        AhkTest.AssertEqual([0, 0, 0, 22, 40, 170, 0, 255, 132, 131, 255, 0, 77, 0, 181, 247, 255, 255], PillowCImageToArray(unsharpOut, 18))
    } finally {
        for handle in [boxTarget, unsharpOut, gaussianOut, boxOut, modeOut, maxOut, medianOut, minOut, kernelOut, la] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filters match Pillow LA", PillowCTestImageFilterLaMatchesPillow)

PillowCTestImageFilterCmykMatchesPillow(*) {
    cmyk := PillowCCreateImageMode(4, 3, 7)
    modeSource := PillowCCreateImageMode(3, 3, 7)
    boxTarget := PillowCCreateImageMode(4, 3, 7)
    kernelOut := 0
    medianOut := 0
    modeOut := 0
    boxOut := 0
    gaussianOut := 0
    unsharpOut := 0
    try {
        PillowCImageSetBytes(cmyk, [
            1, 2, 3, 4, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130,
            130, 140, 150, 160, 160, 170, 180, 190, 200, 210, 220, 230, 230, 240, 250, 255,
            5, 15, 25, 35, 35, 45, 55, 65, 65, 75, 85, 95, 95, 105, 115, 125,
        ])
        PillowCImageSetBytes(modeSource, [
            0, 10, 200, 5, 0, 20, 200, 5, 0, 30, 100, 7,
            0, 40, 200, 5, 9, 50, 100, 5, 9, 60, 100, 7,
            1, 70, 200, 9, 1, 80, 100, 9, 1, 90, 100, 9,
        ])
        before := PillowCImageData(boxTarget).Ptr

        kernelOut := PillowCImageFilterKernel(cmyk, 3, 3, [0, -1, 0, -1, 5, -1, 0, -1, 0], 1.0, 0.0)
        medianOut := PillowCImageFilterRank(cmyk, 3, 4)
        modeOut := PillowCImageFilterMode(modeSource, 3)
        boxOut := PillowCImageFilterBoxBlur(cmyk, 1.25, 0.5)
        gaussianOut := PillowCImageFilterGaussianBlur(cmyk, 1.25, 0.5)
        unsharpOut := PillowCImageFilterUnsharpMask(cmyk, 1.25, 200, 0)
        PillowCImageFilterBoxBlurInto(cmyk, 1.25, 0.5, boxTarget)

        AhkTest.AssertEqual(before, PillowCImageData(boxTarget).Ptr)
        AhkTest.AssertEqual([
            1, 2, 3, 4, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130,
            130, 140, 150, 160, 255, 255, 255, 255, 255, 255, 255, 255, 230, 240, 250, 255,
            5, 15, 25, 35, 35, 45, 55, 65, 65, 75, 85, 95, 95, 105, 115, 125,
        ], PillowCImageToArray(kernelOut, 48))
        AhkTest.AssertEqual([
            20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 100, 110, 120, 130,
            20, 30, 40, 50, 60, 70, 80, 90, 95, 105, 115, 125, 100, 110, 120, 130,
            35, 45, 55, 65, 65, 75, 85, 95, 95, 105, 115, 125, 95, 105, 115, 125,
        ], PillowCImageToArray(medianOut, 48))
        AhkTest.AssertEqual([
            0, 10, 200, 5, 0, 20, 100, 5, 0, 30, 100, 7,
            0, 40, 200, 5, 0, 50, 100, 5, 9, 60, 100, 7,
            1, 70, 200, 9, 1, 80, 100, 9, 1, 90, 100, 9,
        ], PillowCImageToArray(modeOut, 36))
        AhkTest.AssertEqual([
            44, 50, 55, 61, 64, 72, 80, 87, 93, 102, 111, 121, 116, 126, 136, 146,
            79, 88, 96, 105, 100, 109, 118, 127, 128, 137, 147, 156, 149, 159, 169, 178,
            50, 60, 70, 80, 69, 79, 89, 99, 96, 106, 116, 125, 116, 126, 136, 145,
        ], PillowCImageToArray(boxOut, 48))
        AhkTest.AssertEqual(PillowCImageToArray(boxOut, 48), PillowCImageToArray(boxTarget, 48))
        AhkTest.AssertEqual([
            31, 36, 42, 48, 47, 54, 63, 69, 72, 81, 90, 99, 91, 101, 111, 120,
            122, 131, 140, 150, 139, 148, 158, 167, 163, 172, 182, 191, 180, 190, 200, 208,
            38, 48, 58, 68, 53, 63, 73, 83, 75, 85, 95, 105, 91, 101, 111, 120,
        ], PillowCImageToArray(gaussianOut, 48))
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 58, 68, 78, 92,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 19, 43, 53, 63, 75,
        ], PillowCImageToArray(unsharpOut, 48))
    } finally {
        for handle in [unsharpOut, gaussianOut, boxOut, modeOut, medianOut, kernelOut, boxTarget, modeSource, cmyk] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filters match Pillow CMYK", PillowCTestImageFilterCmykMatchesPillow)

PillowCTestImageTransformAffineNearestMatchesPillow(*) {
    source := PillowCCreateImageMode(3, 2, 1)
    out := 0
    target := PillowCCreateImageMode(3, 2, 1)
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(target, [99, 99, 99, 99, 99, 99])

        out := PillowCImageTransformAffine(source, 3, 2, [1.0, 0.0, -1.0, 0.0, 1.0, 0.0], 0, [8])
        PillowCImageTransformAffineInto(source, 3, 2, [1.0, 0.0, -1.0, 0.0, 1.0, 0.0], target, 0, [8])

        AhkTest.AssertEqual([8, 1, 2, 8, 4, 5], PillowCImageToArray(out, 6))
        AhkTest.AssertEqual([8, 1, 2, 8, 4, 5], PillowCImageToArray(target, 6))
    } finally {
        for handle in [target, out, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image transform AFFINE NEAREST matches Pillow", PillowCTestImageTransformAffineNearestMatchesPillow)

PillowCTestImageTransformAffineFilteredResamplersMatchPillow(*) {
    l := PillowCCreateImageMode(3, 2, 1)
    la := PillowCCreateImageMode(3, 2, 2)
    rgb := PillowCCreateImageMode(3, 2, 3)
    rgba := PillowCCreateImageMode(2, 2, 4)
    lOut := 0
    laOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(la, [
            10, 0, 80, 64, 200, 128,
            40, 255, 160, 128, 250, 32,
        ])
        PillowCImageSetBytes(rgb, [
            1, 2, 3,
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
            100, 110, 120,
            130, 140, 150,
        ])
        PillowCImageSetBytes(rgba, [
            1, 2, 3, 4,
            10, 20, 30, 40,
            50, 60, 70, 80,
            90, 100, 110, 120,
        ])

        lOut := PillowCImageTransformAffine(l, 3, 2, [1.0, 0.0, -0.5, 0.0, 1.0, 0.0], 2, [8])
        laOut := PillowCImageTransformAffine(la, 3, 2, [1.0, 0.0, -0.25, 0.0, 1.0, 0.0], 2, [0, 0])
        rgbOut := PillowCImageTransformAffine(rgb, 4, 3, [0.75, 0.0, 0.0, 0.0, 0.75, 0.0], 3, [9, 0, 0])
        rgbaOut := PillowCImageTransformAffine(rgba, 4, 4, [1.0, 0.0, -1.0, 0.0, 1.0, -1.0], 2, [9, 0, 0, 128])

        AhkTest.AssertEqual([1, 1, 2, 4, 4, 5], PillowCImageToArray(lOut, 6))
        AhkTest.AssertEqual([
            0, 0, 79, 48, 182, 112,
            40, 255, 112, 159, 195, 56,
        ], PillowCImageToArray(laOut, 12))
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 6, 13, 25, 36, 34, 44, 54,
            42, 48, 54, 53, 62, 71, 80, 91, 101, 99, 109, 119,
            76, 88, 99, 96, 106, 117, 129, 139, 148, 146, 156, 166,
        ], PillowCImageToArray(rgbOut, 36))
        AhkTest.AssertEqual([
            17, 0, 0, 128, 17, 0, 0, 128, 17, 0, 0, 128, 17, 0, 0, 128,
            17, 0, 0, 128, 0, 0, 0, 4, 12, 19, 31, 40, 17, 0, 0, 128,
            17, 0, 0, 128, 51, 60, 70, 80, 89, 99, 110, 120, 17, 0, 0, 128,
            17, 0, 0, 128, 17, 0, 0, 128, 17, 0, 0, 128, 17, 0, 0, 128,
        ], PillowCImageToArray(rgbaOut, 64))
    } finally {
        for handle in [rgbaOut, rgbOut, laOut, lOut, rgba, rgb, la, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image transform AFFINE filtered resamplers match Pillow", PillowCTestImageTransformAffineFilteredResamplersMatchPillow)

PillowCTestImageTransformAffineRejectsUnsupportedResampleAndTargetShape(*) {
    source := PillowCCreateImageMode(3, 2, 1)
    wrongTarget := PillowCCreateImageMode(2, 2, 1)
    matrix := PillowCAffineMatrix([1.0, 0.0, 0.0, 0.0, 1.0, 0.0])
    fill := PillowCBuffer([0])
    outHandle := 0
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4, 5, 6])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_transform_affine",
            "Ptr", source,
            "Int", 3,
            "Int", 2,
            "Ptr", matrix,
            "Int", 4,
            "Ptr", fill,
            "UPtr", fill.Size,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_transform_affine_into",
            "Ptr", source,
            "Int", 3,
            "Int", 2,
            "Ptr", matrix,
            "Int", 0,
            "Ptr", fill,
            "UPtr", fill.Size,
            "Ptr", wrongTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(wrongTarget)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image transform AFFINE rejects unsupported resample and target shape", PillowCTestImageTransformAffineRejectsUnsupportedResampleAndTargetShape)

PillowCTestImageTransformPerspectiveMatchesPillow(*) {
    l := PillowCCreateImageMode(4, 3, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    nearest := 0
    bilinear := 0
    bicubic := 0
    target := PillowCCreateImageMode(3, 2, 1)
    try {
        PillowCImageSetBytes(l, [
            1, 2, 3, 4,
            5, 6, 7, 8,
            9, 10, 11, 12,
        ])
        PillowCImageSetBytes(rgb, [
            1, 2, 3,
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
            100, 110, 120,
            130, 140, 150,
        ])
        PillowCImageSetBytes(target, [99, 99, 99, 99, 99, 99])

        nearest := PillowCImageTransformPerspective(l, 3, 2, [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.1, 0.05], 0, [9])
        bilinear := PillowCImageTransformPerspective(l, 3, 2, [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.1, 0.05], 2, [9])
        bicubic := PillowCImageTransformPerspective(rgb, 4, 3, [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.1, 0.05], 3, [9, 0, 0])
        PillowCImageTransformPerspectiveInto(l, 3, 2, [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.1, 0.05], target, 2, [9])

        AhkTest.AssertEqual([1, 2, 2, 5, 6, 6], PillowCImageToArray(nearest, 6))
        AhkTest.AssertEqual([1, 1, 2, 4, 4, 4], PillowCImageToArray(bilinear, 6))
        AhkTest.AssertEqual([1, 1, 2, 4, 4, 4], PillowCImageToArray(target, 6))
        AhkTest.AssertEqual([
            0, 0, 0, 0, 6, 14, 17, 28, 39, 32, 42, 52,
            57, 65, 74, 64, 73, 82, 81, 92, 102, 88, 98, 108,
            9, 0, 0, 97, 107, 117, 127, 137, 147, 138, 148, 157,
        ], PillowCImageToArray(bicubic, 36))
    } finally {
        for handle in [target, bicubic, bilinear, nearest, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image transform PERSPECTIVE matches Pillow", PillowCTestImageTransformPerspectiveMatchesPillow)

PillowCTestImageTransformPerspectiveRejectsUnsupportedResampleAndTargetShape(*) {
    source := PillowCCreateImageMode(3, 2, 1)
    wrongTarget := PillowCCreateImageMode(2, 2, 1)
    coefficients := PillowCPerspectiveCoefficients([1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0])
    fill := PillowCBuffer([0])
    outHandle := 0
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4, 5, 6])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_transform_perspective",
            "Ptr", source,
            "Int", 3,
            "Int", 2,
            "Ptr", coefficients,
            "Int", 4,
            "Ptr", fill,
            "UPtr", fill.Size,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_transform_perspective_into",
            "Ptr", source,
            "Int", 3,
            "Int", 2,
            "Ptr", coefficients,
            "Int", 0,
            "Ptr", fill,
            "UPtr", fill.Size,
            "Ptr", wrongTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(wrongTarget)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image transform PERSPECTIVE rejects unsupported resample and target shape", PillowCTestImageTransformPerspectiveRejectsUnsupportedResampleAndTargetShape)

PillowCTestImageTransformQuadMatchesPillow(*) {
    l := PillowCCreateImageMode(4, 3, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    nearest := 0
    bilinear := 0
    bicubic := 0
    target := PillowCCreateImageMode(3, 2, 1)
    try {
        PillowCImageSetBytes(l, [
            1, 2, 3, 4,
            5, 6, 7, 8,
            9, 10, 11, 12,
        ])
        PillowCImageSetBytes(rgb, [
            1, 2, 3,
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
            100, 110, 120,
            130, 140, 150,
        ])
        PillowCImageSetBytes(target, [99, 99, 99, 99, 99, 99])

        nearest := PillowCImageTransformQuad(l, 3, 2, [0.0, 0.0, 0.0, 3.0, 4.0, 3.0, 4.0, 0.0], 0, [8])
        bilinear := PillowCImageTransformQuad(l, 3, 2, [0.0, 0.0, 0.5, 2.5, 3.5, 2.0, 3.0, 0.5], 2, [9])
        bicubic := PillowCImageTransformQuad(rgb, 4, 3, [0.0, 0.0, 0.25, 1.8, 2.75, 1.7, 2.5, 0.25], 3, [9, 0, 0])
        PillowCImageTransformQuadInto(l, 3, 2, [0.0, 0.0, 0.5, 2.5, 3.5, 2.0, 3.0, 0.5], target, 2, [9])

        AhkTest.AssertEqual([1, 3, 4, 9, 11, 12], PillowCImageToArray(nearest, 6))
        AhkTest.AssertEqual([1, 3, 4, 6, 7, 8], PillowCImageToArray(bilinear, 6))
        AhkTest.AssertEqual([1, 3, 4, 6, 7, 8], PillowCImageToArray(target, 6))
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 3, 7, 18, 29, 29, 40, 50,
            28, 32, 37, 36, 44, 52, 58, 68, 79, 76, 86, 97,
            70, 80, 90, 83, 93, 103, 110, 120, 130, 123, 133, 143,
        ], PillowCImageToArray(bicubic, 36))
    } finally {
        for handle in [target, bicubic, bilinear, nearest, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image transform QUAD matches Pillow", PillowCTestImageTransformQuadMatchesPillow)

PillowCTestImageTransformQuadRejectsUnsupportedResampleAndTargetShape(*) {
    source := PillowCCreateImageMode(3, 2, 1)
    wrongTarget := PillowCCreateImageMode(2, 2, 1)
    corners := PillowCQuadCorners([0.0, 0.0, 0.0, 2.0, 3.0, 2.0, 3.0, 0.0])
    fill := PillowCBuffer([0])
    outHandle := 0
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4, 5, 6])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_transform_quad",
            "Ptr", source,
            "Int", 3,
            "Int", 2,
            "Ptr", corners,
            "Int", 4,
            "Ptr", fill,
            "UPtr", fill.Size,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_transform_quad_into",
            "Ptr", source,
            "Int", 3,
            "Int", 2,
            "Ptr", corners,
            "Int", 0,
            "Ptr", fill,
            "UPtr", fill.Size,
            "Ptr", wrongTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(wrongTarget)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image transform QUAD rejects unsupported resample and target shape", PillowCTestImageTransformQuadRejectsUnsupportedResampleAndTargetShape)

PillowCTestImageTransformMeshMatchesPillow(*) {
    l := PillowCCreateImageMode(4, 3, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    nearest := 0
    bilinear := 0
    emptyOut := 0
    rgbOut := 0
    target := PillowCCreateImageMode(5, 3, 1)
    try {
        PillowCImageSetBytes(l, [
            1, 2, 3, 4,
            5, 6, 7, 8,
            9, 10, 11, 12,
        ])
        PillowCImageSetBytes(rgb, [
            1, 2, 3,
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
            100, 110, 120,
            130, 140, 150,
        ])
        PillowCImageSetBytes(target, [
            1, 1, 1, 1, 1,
            1, 1, 1, 1, 1,
            1, 1, 1, 1, 1,
        ])

        boxes := [0, 0, 3, 2, 3, 1, 5, 3]
        quads := [
            0.0, 0.0, 0.0, 3.0, 4.0, 3.0, 4.0, 0.0,
            0.0, 0.0, 0.0, 3.0, 4.0, 3.0, 4.0, 0.0,
        ]
        nearest := PillowCImageTransformMesh(l, 5, 3, boxes, quads, 0, [99])
        bilinear := PillowCImageTransformMesh(l, 5, 3, boxes, quads, 2, [99])
        PillowCImageTransformMeshInto(l, 5, 3, boxes, quads, target, 2, [99])
        emptyOut := PillowCImageTransformMesh(l, 3, 2, [], [], 0, [55])
        rgbOut := PillowCImageTransformMesh(
            rgb,
            4,
            3,
            [0, 0, 2, 2, 2, 1, 4, 3],
            [
                0.0, 0.0, 0.0, 2.0, 3.0, 2.0, 3.0, 0.0,
                0.0, 0.0, 0.0, 2.0, 3.0, 2.0, 3.0, 0.0,
            ],
            2,
            [9, 0, 0])

        AhkTest.AssertEqual([
            1, 3, 4, 99, 99,
            9, 11, 12, 2, 4,
            99, 99, 99, 10, 12,
        ], PillowCImageToArray(nearest, 15))
        AhkTest.AssertEqual([
            2, 3, 4, 99, 99,
            8, 9, 10, 2, 4,
            99, 99, 99, 8, 10,
        ], PillowCImageToArray(bilinear, 15))
        AhkTest.AssertEqual([
            2, 3, 4, 99, 99,
            8, 9, 10, 2, 4,
            99, 99, 99, 8, 10,
        ], PillowCImageToArray(target, 15))
        AhkTest.AssertEqual([55, 55, 55, 55, 55, 55], PillowCImageToArray(emptyOut, 6))
        AhkTest.AssertEqual([
            3, 6, 9, 32, 42, 52, 9, 0, 0, 9, 0, 0,
            77, 87, 97, 122, 132, 142, 3, 6, 9, 32, 42, 52,
            9, 0, 0, 9, 0, 0, 77, 87, 97, 122, 132, 142,
        ], PillowCImageToArray(rgbOut, 36))
    } finally {
        for handle in [target, rgbOut, emptyOut, bilinear, nearest, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image transform MESH matches Pillow", PillowCTestImageTransformMeshMatchesPillow)

PillowCTestImageTransformMeshRejectsUnsupportedResampleAndTargetShape(*) {
    source := PillowCCreateImageMode(3, 2, 1)
    wrongTarget := PillowCCreateImageMode(2, 2, 1)
    boxes := PillowCMeshBoxes([0, 0, 3, 2])
    quads := PillowCMeshQuads([0.0, 0.0, 0.0, 2.0, 3.0, 2.0, 3.0, 0.0])
    fill := PillowCBuffer([0])
    outHandle := 0
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4, 5, 6])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_transform_mesh",
            "Ptr", source,
            "Int", 3,
            "Int", 2,
            "Ptr", boxes,
            "Ptr", quads,
            "UPtr", 1,
            "Int", 4,
            "Ptr", fill,
            "UPtr", fill.Size,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_transform_mesh_into",
            "Ptr", source,
            "Int", 3,
            "Int", 2,
            "Ptr", boxes,
            "Ptr", quads,
            "UPtr", 1,
            "Int", 0,
            "Ptr", fill,
            "UPtr", fill.Size,
            "Ptr", wrongTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(wrongTarget)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image transform MESH rejects unsupported resample and target shape", PillowCTestImageTransformMeshRejectsUnsupportedResampleAndTargetShape)

PillowCTestImageRotateNearestMatchesPillowGeometry(*) {
    l := PillowCCreateImageMode(3, 2, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    rgba := PillowCCreateImageMode(2, 2, 4)
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(rgb, [
            1, 2, 3,
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
            100, 110, 120,
            130, 140, 150,
        ])
        PillowCImageSetBytes(rgba, [
            1, 2, 3, 4,
            10, 20, 30, 40,
            50, 60, 70, 80,
            90, 100, 110, 120,
        ])

        lOut := PillowCImageRotate(l, 45)
        rgbOut := PillowCImageRotate(rgb, 45)
        rgbaOut := PillowCImageRotate(rgba, 45)

        AhkTest.AssertEqual(3, PillowCImageInt(lOut, "pillow_c_image_width"))
        AhkTest.AssertEqual(2, PillowCImageInt(lOut, "pillow_c_image_height"))
        AhkTest.AssertEqual([0, 2, 6, 1, 5, 0], PillowCImageToArray(lOut, 6))
        AhkTest.AssertEqual([
            0, 0, 0,
            10, 20, 30,
            130, 140, 150,
            1, 2, 3,
            100, 110, 120,
            0, 0, 0,
        ], PillowCImageToArray(rgbOut, 18))
        AhkTest.AssertEqual([
            10, 20, 30, 40,
            90, 100, 110, 120,
            50, 60, 70, 80,
            90, 100, 110, 120,
        ], PillowCImageToArray(rgbaOut, 16))
    } finally {
        for handle in [rgbaOut, rgbOut, lOut, rgba, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image rotate NEAREST matches Pillow geometry", PillowCTestImageRotateNearestMatchesPillowGeometry)

PillowCTestImageRotateNearestExpandAndFillMatchesPillow(*) {
    l := PillowCCreateImageMode(3, 2, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    rgba := PillowCCreateImageMode(2, 2, 4)
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(rgb, [
            1, 2, 3,
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
            100, 110, 120,
            130, 140, 150,
        ])
        PillowCImageSetBytes(rgba, [
            1, 2, 3, 4,
            10, 20, 30, 40,
            50, 60, 70, 80,
            90, 100, 110, 120,
        ])

        lOut := PillowCImageRotate(l, 45, 0, true, 0.0, 0.0, false, 0.0, 0.0, false, [9])
        rgbOut := PillowCImageRotate(rgb, 45, 0, true, 0.0, 0.0, false, 0.0, 0.0, false, [9, 0, 0])
        rgbaOut := PillowCImageRotate(rgba, 45, 0, true, 0.0, 0.0, false, 0.0, 0.0, false, [9, 0, 0, 0])

        AhkTest.AssertEqual([5, 4], [PillowCImageInt(lOut, "pillow_c_image_width"), PillowCImageInt(lOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            9, 9, 9, 9, 9,
            9, 9, 2, 6, 9,
            9, 1, 5, 9, 9,
            9, 9, 9, 9, 9,
        ], PillowCImageToArray(lOut, 20))
        AhkTest.AssertEqual([
            9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0,
            9, 0, 0, 9, 0, 0, 10, 20, 30, 130, 140, 150, 9, 0, 0,
            9, 0, 0, 1, 2, 3, 100, 110, 120, 9, 0, 0, 9, 0, 0,
            9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0,
        ], PillowCImageToArray(rgbOut, 60))
        AhkTest.AssertEqual([4, 4], [PillowCImageInt(rgbaOut, "pillow_c_image_width"), PillowCImageInt(rgbaOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0,
            9, 0, 0, 0, 10, 20, 30, 40, 90, 100, 110, 120, 9, 0, 0, 0,
            9, 0, 0, 0, 50, 60, 70, 80, 90, 100, 110, 120, 9, 0, 0, 0,
            9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0,
        ], PillowCImageToArray(rgbaOut, 64))
    } finally {
        for handle in [rgbaOut, rgbOut, lOut, rgba, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image rotate NEAREST expand and fill match Pillow", PillowCTestImageRotateNearestExpandAndFillMatchesPillow)

PillowCTestImageRotateBilinearMatchesPillowGeometry(*) {
    l := PillowCCreateImageMode(3, 2, 1)
    la := PillowCCreateImageMode(3, 2, 2)
    rgb := PillowCCreateImageMode(3, 2, 3)
    rgba := PillowCCreateImageMode(2, 2, 4)
    lOut := 0
    laOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(la, [
            10, 0, 80, 64, 200, 128,
            40, 255, 160, 128, 250, 32,
        ])
        PillowCImageSetBytes(rgb, [
            1, 2, 3,
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
            100, 110, 120,
            130, 140, 150,
        ])
        PillowCImageSetBytes(rgba, [
            1, 2, 3, 4,
            10, 20, 30, 40,
            50, 60, 70, 80,
            90, 100, 110, 120,
        ])

        lOut := PillowCImageRotate(l, 45, 2)
        laOut := PillowCImageRotate(la, 30, 2, true)
        rgbOut := PillowCImageRotate(rgb, 45, 2)
        rgbaOut := PillowCImageRotate(rgba, 45, 2)

        AhkTest.AssertEqual([0, 2, 5, 1, 4, 0], PillowCImageToArray(lOut, 6))
        AhkTest.AssertEqual([5, 4], [PillowCImageInt(laOut, "pillow_c_image_width"), PillowCImageInt(laOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 74, 24, 129, 81, 209, 73, 0, 0,
            0, 0, 39, 110, 110, 152, 183, 68, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(laOut, 40))
        AhkTest.AssertEqual([
            0, 0, 0,
            33, 43, 53,
            116, 126, 136,
            11, 13, 15,
            77, 86, 96,
            0, 0, 0,
        ], PillowCImageToArray(rgbOut, 18))
        AhkTest.AssertEqual([
            11, 11, 23, 22,
            70, 79, 89, 80,
            48, 54, 66, 42,
            73, 84, 94, 100,
        ], PillowCImageToArray(rgbaOut, 16))
    } finally {
        for handle in [rgbaOut, rgbOut, laOut, lOut, rgba, rgb, la, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image rotate BILINEAR matches Pillow geometry", PillowCTestImageRotateBilinearMatchesPillowGeometry)

PillowCTestImageRotateBilinearExpandFillTranslateAndInto(*) {
    l := PillowCCreateImageMode(3, 2, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    rgba := PillowCCreateImageMode(2, 2, 4)
    target := PillowCCreateImageMode(3, 2, 1)
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(rgb, [
            1, 2, 3,
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
            100, 110, 120,
            130, 140, 150,
        ])
        PillowCImageSetBytes(rgba, [
            1, 2, 3, 4,
            10, 20, 30, 40,
            50, 60, 70, 80,
            90, 100, 110, 120,
        ])
        PillowCImageSetBytes(target, [99, 99, 99, 99, 99, 99])

        lOut := PillowCImageRotate(l, 30, 2, false, 0.0, 0.0, false, 1.0, -1.0, true, [8])
        rgbOut := PillowCImageRotate(rgb, 45, 2, true, 0.0, 0.0, false, 0.0, 0.0, false, [9, 0, 0])
        rgbaOut := PillowCImageRotate(rgba, 45, 2, true, 0.0, 0.0, false, 0.0, 0.0, false, [9, 0, 0, 128])
        PillowCImageRotateInto(l, 30, target, 2, false, 0.0, 0.0, false, 1.0, -1.0, true, [8])

        AhkTest.AssertEqual([8, 2, 4, 8, 8, 8], PillowCImageToArray(lOut, 6))
        AhkTest.AssertEqual([8, 2, 4, 8, 8, 8], PillowCImageToArray(target, 6))
        AhkTest.AssertEqual([5, 4], [PillowCImageInt(rgbOut, "pillow_c_image_width"), PillowCImageInt(rgbOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0,
            9, 0, 0, 9, 0, 0, 33, 43, 53, 116, 126, 136, 9, 0, 0,
            9, 0, 0, 11, 13, 15, 77, 86, 96, 9, 0, 0, 9, 0, 0,
            9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0,
        ], PillowCImageToArray(rgbOut, 60))
        AhkTest.AssertEqual([
            17, 0, 0, 128, 17, 0, 0, 128, 17, 0, 0, 128, 17, 0, 0, 128,
            17, 0, 0, 128, 11, 11, 23, 22, 70, 79, 89, 80, 17, 0, 0, 128,
            17, 0, 0, 128, 48, 54, 66, 42, 73, 84, 94, 100, 17, 0, 0, 128,
            17, 0, 0, 128, 17, 0, 0, 128, 17, 0, 0, 128, 17, 0, 0, 128,
        ], PillowCImageToArray(rgbaOut, 64))
    } finally {
        for handle in [rgbaOut, rgbOut, lOut, target, rgba, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image rotate BILINEAR expand fill translate and _into match Pillow", PillowCTestImageRotateBilinearExpandFillTranslateAndInto)

PillowCTestImageRotateBicubicMatchesPillowGeometry(*) {
    l := PillowCCreateImageMode(3, 2, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    rgba := PillowCCreateImageMode(2, 2, 4)
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(rgb, [
            1, 2, 3,
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
            100, 110, 120,
            130, 140, 150,
        ])
        PillowCImageSetBytes(rgba, [
            1, 2, 3, 4,
            10, 20, 30, 40,
            50, 60, 70, 80,
            90, 100, 110, 120,
        ])

        lOut := PillowCImageRotate(l, 45, 3)
        rgbOut := PillowCImageRotate(rgb, 45, 3)
        rgbaOut := PillowCImageRotate(rgba, 45, 3)

        AhkTest.AssertEqual([0, 2, 5, 1, 4, 0], PillowCImageToArray(lOut, 6))
        AhkTest.AssertEqual([
            0, 0, 0,
            35, 46, 57,
            118, 128, 138,
            10, 12, 14,
            72, 82, 91,
            0, 0, 0,
        ], PillowCImageToArray(rgbOut, 18))
        AhkTest.AssertEqual([
            0, 0, 0, 11,
            69, 81, 91, 84,
            41, 48, 55, 37,
            74, 85, 95, 110,
        ], PillowCImageToArray(rgbaOut, 16))
    } finally {
        for handle in [rgbaOut, rgbOut, lOut, rgba, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image rotate BICUBIC matches Pillow geometry", PillowCTestImageRotateBicubicMatchesPillowGeometry)

PillowCTestImageRotateBicubicExpandFillTranslateAndInto(*) {
    l := PillowCCreateImageMode(3, 2, 1)
    rgb := PillowCCreateImageMode(3, 2, 3)
    rgba := PillowCCreateImageMode(2, 2, 4)
    target := PillowCCreateImageMode(3, 2, 1)
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(rgb, [
            1, 2, 3,
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
            100, 110, 120,
            130, 140, 150,
        ])
        PillowCImageSetBytes(rgba, [
            1, 2, 3, 4,
            10, 20, 30, 40,
            50, 60, 70, 80,
            90, 100, 110, 120,
        ])
        PillowCImageSetBytes(target, [99, 99, 99, 99, 99, 99])

        lOut := PillowCImageRotate(l, 30, 3, false, 0.0, 0.0, false, 1.0, -1.0, true, [8])
        rgbOut := PillowCImageRotate(rgb, 45, 3, true, 0.0, 0.0, false, 0.0, 0.0, false, [9, 0, 0])
        rgbaOut := PillowCImageRotate(rgba, 45, 3, true, 0.0, 0.0, false, 0.0, 0.0, false, [9, 0, 0, 128])
        PillowCImageRotateInto(l, 30, target, 3, false, 0.0, 0.0, false, 1.0, -1.0, true, [8])

        AhkTest.AssertEqual([8, 2, 4, 8, 8, 8], PillowCImageToArray(lOut, 6))
        AhkTest.AssertEqual([8, 2, 4, 8, 8, 8], PillowCImageToArray(target, 6))
        AhkTest.AssertEqual([5, 4], [PillowCImageInt(rgbOut, "pillow_c_image_width"), PillowCImageInt(rgbOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0,
            9, 0, 0, 9, 0, 0, 35, 46, 57, 118, 128, 138, 9, 0, 0,
            9, 0, 0, 10, 12, 14, 72, 82, 91, 9, 0, 0, 9, 0, 0,
            9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0, 9, 0, 0,
        ], PillowCImageToArray(rgbOut, 60))
        AhkTest.AssertEqual([
            17, 0, 0, 128, 17, 0, 0, 128, 17, 0, 0, 128, 17, 0, 0, 128,
            17, 0, 0, 128, 0, 0, 0, 11, 69, 81, 91, 84, 17, 0, 0, 128,
            17, 0, 0, 128, 41, 48, 55, 37, 74, 85, 95, 110, 17, 0, 0, 128,
            17, 0, 0, 128, 17, 0, 0, 128, 17, 0, 0, 128, 17, 0, 0, 128,
        ], PillowCImageToArray(rgbaOut, 64))
    } finally {
        for handle in [rgbaOut, rgbOut, lOut, target, rgba, rgb, l] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image rotate BICUBIC expand fill translate and _into match Pillow", PillowCTestImageRotateBicubicExpandFillTranslateAndInto)

PillowCTestImageRotateNearestCenterTranslateAndInto(*) {
    source := PillowCCreateImageMode(3, 2, 1)
    centered := 0
    translated := 0
    target := PillowCCreateImageMode(3, 2, 1)
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(target, [99, 99, 99, 99, 99, 99])

        centered := PillowCImageRotate(source, 30, 0, false, 0.0, 0.0, true, 0.0, 0.0, false, [7])
        translated := PillowCImageRotate(source, 30, 0, false, 0.0, 0.0, false, 1.0, -1.0, true, [8])
        PillowCImageRotateInto(source, 30, target, 0, false, 0.0, 0.0, false, 1.0, -1.0, true, [8])

        AhkTest.AssertEqual([1, 5, 5, 7, 7, 7], PillowCImageToArray(centered, 6))
        AhkTest.AssertEqual([8, 1, 5, 8, 8, 8], PillowCImageToArray(translated, 6))
        AhkTest.AssertEqual([8, 1, 5, 8, 8, 8], PillowCImageToArray(target, 6))
    } finally {
        for handle in [target, translated, centered, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image rotate NEAREST center translate and _into match Pillow", PillowCTestImageRotateNearestCenterTranslateAndInto)

PillowCTestImageRotateRejectsUnsupportedResampleAndTargetShape(*) {
    source := PillowCCreateImageMode(3, 2, 1)
    wrongTarget := PillowCCreateImageMode(3, 2, 1)
    outHandle := 0
    try {
        PillowCImageSetBytes(source, [1, 2, 3, 4, 5, 6])
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_rotate",
            "Ptr", source,
            "Double", 45.0,
            "Int", 4,
            "Int", false,
            "Double", 0.0,
            "Double", 0.0,
            "Int", false,
            "Double", 0.0,
            "Double", 0.0,
            "Int", false,
            "Ptr", 0,
            "UPtr", 0,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_rotate_into",
            "Ptr", source,
            "Double", 45.0,
            "Int", 0,
            "Int", true,
            "Double", 0.0,
            "Double", 0.0,
            "Int", false,
            "Double", 0.0,
            "Double", 0.0,
            "Int", false,
            "Ptr", 0,
            "UPtr", 0,
            "Ptr", wrongTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(wrongTarget)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image rotate rejects unsupported resample and target shape", PillowCTestImageRotateRejectsUnsupportedResampleAndTargetShape)

PillowCTestImageGeometryCmykMatchesPillow(*) {
    source := PillowCCreateImageMode(3, 2, 7)
    resizeTarget := PillowCCreateImageMode(4, 3, 7)
    resized := 0
    lanczos := 0
    affine := 0
    rotated := 0
    try {
        PillowCImageSetBytes(source, [
            1, 2, 3, 4, 10, 20, 30, 40, 40, 50, 60, 70,
            70, 80, 90, 100, 100, 110, 120, 130, 130, 140, 150, 160,
        ])
        before := PillowCImageData(resizeTarget).Ptr

        resized := PillowCImageResize(source, 4, 3, 3)
        lanczos := PillowCImageResize(source, 2, 4, 1)
        affine := PillowCImageTransformAffine(source, 4, 3, [0.75, 0.0, 0.0, 0.0, 0.75, 0.0], 3, [9, 8, 7, 6])
        rotated := PillowCImageRotate(source, 45, 2, true, 0.0, 0.0, false, 0.0, 0.0, false, [9, 8, 7, 6])
        PillowCImageResizeInto(source, 4, 3, 3, resizeTarget)

        AhkTest.AssertEqual(before, PillowCImageData(resizeTarget).Ptr)
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 5, 12, 18, 16, 26, 37, 48, 36, 46, 56, 66,
            35, 40, 45, 50, 46, 54, 63, 71, 68, 78, 88, 99, 87, 97, 107, 117,
            72, 83, 94, 104, 92, 103, 113, 123, 119, 129, 139, 149, 138, 148, 158, 168,
        ], PillowCImageToArray(resized, 48))
        AhkTest.AssertEqual(PillowCImageToArray(resized, 48), PillowCImageToArray(resizeTarget, 48))
        AhkTest.AssertEqual([
            0, 0, 0, 0, 14, 25, 36, 46,
            20, 25, 30, 36, 51, 62, 73, 83,
            60, 69, 78, 86, 101, 111, 121, 131,
            91, 102, 113, 124, 138, 148, 158, 168,
        ], PillowCImageToArray(lanczos, 32))
        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 6, 13, 13, 25, 36, 47, 34, 44, 54, 64,
            42, 48, 54, 60, 53, 62, 71, 79, 80, 91, 101, 112, 99, 109, 119, 129,
            76, 88, 99, 111, 96, 106, 117, 127, 129, 139, 148, 158, 146, 156, 166, 176,
        ], PillowCImageToArray(affine, 48))
        AhkTest.AssertEqual([5, 4], [PillowCImageInt(rotated, "pillow_c_image_width"), PillowCImageInt(rotated, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            9, 8, 7, 6, 9, 8, 7, 6, 9, 8, 7, 6, 9, 8, 7, 6, 9, 8, 7, 6,
            9, 8, 7, 6, 9, 8, 7, 6, 33, 43, 53, 63, 116, 126, 136, 146, 9, 8, 7, 6,
            9, 8, 7, 6, 11, 13, 15, 18, 77, 86, 96, 105, 9, 8, 7, 6, 9, 8, 7, 6,
            9, 8, 7, 6, 9, 8, 7, 6, 9, 8, 7, 6, 9, 8, 7, 6, 9, 8, 7, 6,
        ], PillowCImageToArray(rotated, 80))
    } finally {
        for handle in [rotated, affine, lanczos, resized, resizeTarget, source] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image geometry paths match Pillow CMYK", PillowCTestImageGeometryCmykMatchesPillow)

PillowCTestImageContainAndCoverNearestMatchPillowGeometry(*) {
    source := PillowCCreateImageMode(4, 2, 1)
    containTall := 0
    containWide := 0
    coverSquare := 0
    coverWide := 0
    try {
        PillowCImageSetBytes(source, [0, 40, 80, 120, 160, 200, 220, 255])
        containTall := PillowCImageContain(source, 4, 4, 0)
        containWide := PillowCImageContain(source, 2, 4, 0)
        coverSquare := PillowCImageCover(source, 4, 4, 0)
        coverWide := PillowCImageCover(source, 6, 2, 0)

        AhkTest.AssertEqual([4, 2], [PillowCImageInt(containTall, "pillow_c_image_width"), PillowCImageInt(containTall, "pillow_c_image_height")])
        AhkTest.AssertEqual([0, 40, 80, 120, 160, 200, 220, 255], PillowCImageToArray(containTall, 8))
        AhkTest.AssertEqual([2, 1], [PillowCImageInt(containWide, "pillow_c_image_width"), PillowCImageInt(containWide, "pillow_c_image_height")])
        AhkTest.AssertEqual([200, 255], PillowCImageToArray(containWide, 2))

        AhkTest.AssertEqual([8, 4], [PillowCImageInt(coverSquare, "pillow_c_image_width"), PillowCImageInt(coverSquare, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            0, 0, 40, 40, 80, 80, 120, 120,
            0, 0, 40, 40, 80, 80, 120, 120,
            160, 160, 200, 200, 220, 220, 255, 255,
            160, 160, 200, 200, 220, 220, 255, 255,
        ], PillowCImageToArray(coverSquare, 32))

        AhkTest.AssertEqual([6, 3], [PillowCImageInt(coverWide, "pillow_c_image_width"), PillowCImageInt(coverWide, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            0, 40, 40, 80, 80, 120,
            160, 200, 200, 220, 220, 255,
            160, 200, 200, 220, 220, 255,
        ], PillowCImageToArray(coverWide, 18))
    } finally {
        for handle in [coverWide, coverSquare, containWide, containTall] {
            if handle
                PillowCFreeImage(handle)
        }
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image contain and cover NEAREST match Pillow geometry", PillowCTestImageContainAndCoverNearestMatchPillowGeometry)

PillowCTestImageContainAndCoverBilinearMatchPillowResizeResults(*) {
    source := PillowCCreateImageMode(3, 2, 3)
    containSquare := 0
    coverSquare := 0
    coverWide := 0
    try {
        PillowCImageSetBytes(source, [
            1, 2, 3, 20, 30, 40, 60, 70, 80,
            100, 110, 120, 150, 160, 170, 200, 210, 220,
        ])
        containSquare := PillowCImageContain(source, 4, 4, 2)
        coverSquare := PillowCImageCover(source, 4, 4, 2)
        coverWide := PillowCImageCover(source, 5, 2, 2)

        AhkTest.AssertEqual([4, 3], [PillowCImageInt(containSquare, "pillow_c_image_width"), PillowCImageInt(containSquare, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            1, 2, 3, 13, 20, 26, 35, 45, 55, 60, 70, 80,
            51, 56, 62, 72, 81, 89, 102, 112, 122, 130, 140, 150,
            100, 110, 120, 131, 141, 151, 169, 179, 189, 200, 210, 220,
        ], PillowCImageToArray(containSquare, 36))

        AhkTest.AssertEqual([6, 4], [PillowCImageInt(coverSquare, "pillow_c_image_width"), PillowCImageInt(coverSquare, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            1, 2, 3, 6, 9, 12, 15, 23, 31, 30, 40, 50, 50, 60, 70, 60, 70, 80,
            26, 29, 32, 33, 38, 42, 46, 54, 63, 63, 73, 83, 85, 95, 105, 95, 105, 115,
            75, 83, 91, 86, 95, 103, 107, 117, 126, 130, 140, 150, 154, 164, 174, 165, 175, 185,
            100, 110, 120, 113, 123, 133, 138, 148, 158, 163, 173, 183, 188, 198, 208, 200, 210, 220,
        ], PillowCImageToArray(coverSquare, 72))

        AhkTest.AssertEqual([5, 3], [PillowCImageInt(coverWide, "pillow_c_image_width"), PillowCImageInt(coverWide, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            1, 2, 3, 9, 13, 18, 20, 30, 40, 44, 54, 64, 60, 70, 80,
            51, 56, 62, 65, 72, 79, 85, 95, 105, 112, 122, 132, 130, 140, 150,
            100, 110, 120, 120, 130, 140, 150, 160, 170, 180, 190, 200, 200, 210, 220,
        ], PillowCImageToArray(coverWide, 45))
    } finally {
        for handle in [coverWide, coverSquare, containSquare] {
            if handle
                PillowCFreeImage(handle)
        }
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image contain and cover BILINEAR match Pillow resize results", PillowCTestImageContainAndCoverBilinearMatchPillowResizeResults)

PillowCTestImageContainAndCoverRejectInvalidRequestedSize(*) {
    source := PillowCCreateImageMode(4, 2, 1)
    outHandle := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_contain",
            "Ptr", source,
            "Int", 1,
            "Int", 1,
            "Int", 0,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_cover",
            "Ptr", source,
            "Int", 4,
            "Int", 0,
            "Int", 0,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image contain and cover reject invalid requested sizes", PillowCTestImageContainAndCoverRejectInvalidRequestedSize)

PillowCTestImagePadNearestMatchesPillowLFillAndCentering(*) {
    source := PillowCCreateImageMode(4, 2, 1)
    centered := 0
    top := 0
    bottom := 0
    narrow := 0
    same := 0
    try {
        PillowCImageSetBytes(source, [0, 40, 80, 120, 160, 200, 220, 255])
        centered := PillowCImagePad(source, 4, 4, 0, [0], 0.5, 0.5)
        top := PillowCImagePad(source, 4, 4, 0, [7], 0, 0)
        bottom := PillowCImagePad(source, 4, 4, 0, [7], 1, 1)
        narrow := PillowCImagePad(source, 2, 4, 0, [9], 0.5, 0.5)
        same := PillowCImagePad(source, 4, 2, 0, [99], 0.5, 0.5)

        AhkTest.AssertEqual([4, 4], [PillowCImageInt(centered, "pillow_c_image_width"), PillowCImageInt(centered, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            0, 0, 0, 0,
            0, 40, 80, 120,
            160, 200, 220, 255,
            0, 0, 0, 0,
        ], PillowCImageToArray(centered, 16))

        AhkTest.AssertEqual([
            0, 40, 80, 120,
            160, 200, 220, 255,
            7, 7, 7, 7,
            7, 7, 7, 7,
        ], PillowCImageToArray(top, 16))

        AhkTest.AssertEqual([
            7, 7, 7, 7,
            7, 7, 7, 7,
            0, 40, 80, 120,
            160, 200, 220, 255,
        ], PillowCImageToArray(bottom, 16))

        AhkTest.AssertEqual([2, 4], [PillowCImageInt(narrow, "pillow_c_image_width"), PillowCImageInt(narrow, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            9, 9,
            9, 9,
            200, 255,
            9, 9,
        ], PillowCImageToArray(narrow, 8))

        AhkTest.AssertEqual([4, 2], [PillowCImageInt(same, "pillow_c_image_width"), PillowCImageInt(same, "pillow_c_image_height")])
        AhkTest.AssertEqual([0, 40, 80, 120, 160, 200, 220, 255], PillowCImageToArray(same, 8))
    } finally {
        for handle in [same, narrow, bottom, top, centered] {
            if handle
                PillowCFreeImage(handle)
        }
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image Pad NEAREST matches Pillow L fill and centering", PillowCTestImagePadNearestMatchesPillowLFillAndCentering)

PillowCTestImagePadHandlesRgbRgbaFillAndCenteringClamp(*) {
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(1, 1, 4)
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(rgb, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(rgba, [1, 2, 3, 4])
        rgbOut := PillowCImagePad(rgb, 4, 4, 0, [7, 8, 9], 0.5, 0.5)
        rgbaOut := PillowCImagePad(rgba, 3, 5, 0, [7, 8, 9, 255], -1, 99)

        AhkTest.AssertEqual([4, 4], [PillowCImageInt(rgbOut, "pillow_c_image_width"), PillowCImageInt(rgbOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9,
            1, 2, 3, 1, 2, 3, 4, 5, 6, 4, 5, 6,
            1, 2, 3, 1, 2, 3, 4, 5, 6, 4, 5, 6,
            7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9,
        ], PillowCImageToArray(rgbOut, 48))

        AhkTest.AssertEqual([3, 5], [PillowCImageInt(rgbaOut, "pillow_c_image_width"), PillowCImageInt(rgbaOut, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            7, 8, 9, 255, 7, 8, 9, 255, 7, 8, 9, 255,
            7, 8, 9, 255, 7, 8, 9, 255, 7, 8, 9, 255,
            1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4,
            1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4,
            1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4,
        ], PillowCImageToArray(rgbaOut, 60))
    } finally {
        for handle in [rgbaOut, rgbOut, rgba, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image Pad handles RGB RGBA fill and centering clamp", PillowCTestImagePadHandlesRgbRgbaFillAndCenteringClamp)

PillowCTestImagePadHandlesLaFillAndCentering(*) {
    la := PillowCCreateImageMode(2, 2, 2)
    wide := 0
    tall := 0
    try {
        PillowCImageSetBytes(la, [10, 0, 80, 64, 160, 128, 250, 255])
        wide := PillowCImagePad(la, 4, 2, 0, [9, 17], 0.5, 0.5)
        tall := PillowCImagePad(la, 2, 4, 0, [9, 17], 0.5, 0.5)

        AhkTest.AssertEqual([4, 2], [PillowCImageInt(wide, "pillow_c_image_width"), PillowCImageInt(wide, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            9, 17, 10, 0, 80, 64, 9, 17,
            9, 17, 160, 128, 250, 255, 9, 17,
        ], PillowCImageToArray(wide, 16))

        AhkTest.AssertEqual([2, 4], [PillowCImageInt(tall, "pillow_c_image_width"), PillowCImageInt(tall, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            9, 17, 9, 17,
            10, 0, 80, 64,
            160, 128, 250, 255,
            9, 17, 9, 17,
        ], PillowCImageToArray(tall, 16))
    } finally {
        for handle in [tall, wide, la] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image Pad handles LA fill and centering", PillowCTestImagePadHandlesLaFillAndCentering)

PillowCTestImagePadRejectsInvalidRequestedSize(*) {
    source := PillowCCreateImageMode(4, 2, 1)
    color := PillowCBuffer([0])
    outHandle := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_pad",
            "Ptr", source,
            "Int", 1,
            "Int", 1,
            "Int", 0,
            "Ptr", color,
            "UPtr", color.Size,
            "Double", 0.5,
            "Double", 0.5,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_pad",
            "Ptr", source,
            "Int", 4,
            "Int", 0,
            "Int", 0,
            "Ptr", color,
            "UPtr", color.Size,
            "Double", 0.5,
            "Double", 0.5,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image Pad rejects invalid requested sizes", PillowCTestImagePadRejectsInvalidRequestedSize)

PillowCTestImageFitNearestMatchesPillowCropGeometry(*) {
    source := PillowCCreateImageMode(4, 2, 1)
    center := 0
    left := 0
    right := 0
    fallback := 0
    wide := 0
    bleed := 0
    try {
        PillowCImageSetBytes(source, [0, 40, 80, 120, 160, 200, 220, 255])
        center := PillowCImageFit(source, 2, 2, 0, 0.0, 0.5, 0.5)
        left := PillowCImageFit(source, 2, 2, 0, 0.0, 0.0, 0.0)
        right := PillowCImageFit(source, 2, 2, 0, 0.0, 1.0, 1.0)
        fallback := PillowCImageFit(source, 2, 2, 0, 0.0, -1.0, 99.0)
        wide := PillowCImageFit(source, 2, 1, 0, 0.0, 0.5, 0.5)
        bleed := PillowCImageFit(source, 2, 1, 0, 0.25, 0.5, 0.5)

        AhkTest.AssertEqual([2, 2], [PillowCImageInt(center, "pillow_c_image_width"), PillowCImageInt(center, "pillow_c_image_height")])
        AhkTest.AssertEqual([40, 80, 200, 220], PillowCImageToArray(center, 4))
        AhkTest.AssertEqual([0, 40, 160, 200], PillowCImageToArray(left, 4))
        AhkTest.AssertEqual([80, 120, 220, 255], PillowCImageToArray(right, 4))
        AhkTest.AssertEqual([40, 80, 200, 220], PillowCImageToArray(fallback, 4))
        AhkTest.AssertEqual([200, 255], PillowCImageToArray(wide, 2))
        AhkTest.AssertEqual([200, 220], PillowCImageToArray(bleed, 2))
    } finally {
        for handle in [bleed, wide, fallback, right, left, center] {
            if handle
                PillowCFreeImage(handle)
        }
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image Fit NEAREST matches Pillow crop geometry", PillowCTestImageFitNearestMatchesPillowCropGeometry)

PillowCTestImageFitBilinearMatchesPillowFloatBoxResize(*) {
    source := PillowCCreateImageMode(3, 2, 3)
    out := 0
    try {
        PillowCImageSetBytes(source, [
            1, 2, 3, 20, 30, 40, 60, 70, 80,
            100, 110, 120, 150, 160, 170, 200, 210, 220,
        ])
        out := PillowCImageFit(source, 4, 4, 2, 0.0, 0.5, 0.5)

        AhkTest.AssertEqual([4, 4], [PillowCImageInt(out, "pillow_c_image_width"), PillowCImageInt(out, "pillow_c_image_height")])
        AhkTest.AssertEqual([
            6, 9, 12, 15, 23, 31, 30, 40, 50, 50, 60, 70,
            33, 38, 42, 46, 54, 63, 63, 73, 83, 85, 95, 105,
            86, 95, 103, 107, 117, 126, 130, 140, 150, 154, 164, 174,
            113, 123, 133, 138, 148, 158, 163, 173, 183, 188, 198, 208,
        ], PillowCImageToArray(out, 48))
    } finally {
        if out
            PillowCFreeImage(out)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image Fit BILINEAR matches Pillow float box resize", PillowCTestImageFitBilinearMatchesPillowFloatBoxResize)

PillowCTestImageFitRejectsInvalidOutputSize(*) {
    source := PillowCCreateImageMode(4, 2, 1)
    outHandle := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_fit",
            "Ptr", source,
            "Int", 0,
            "Int", 1,
            "Int", 0,
            "Double", 0.0,
            "Double", 0.5,
            "Double", 0.5,
            "Ptr*", &outHandle,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, outHandle)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image Fit rejects invalid output size", PillowCTestImageFitRejectsInvalidOutputSize)

PillowCTestImagePasteInsidePoint(*) {
    target := PillowCCreateImage(4, 3, 3)
    source := PillowCCreateImage(3, 2, 3)
    try {
        PillowCImageSetBytes(target, [
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        ])
        PillowCImageSetBytes(source, [
            1, 2, 3, 4, 5, 6, 7, 8, 9,
            11, 12, 13, 14, 15, 16, 17, 18, 19,
        ])
        PillowCImagePaste(target, source, 1, 1)
        AhkTest.AssertEqual([
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 1, 2, 3, 4, 5, 6, 7, 8, 9,
            10, 10, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        ], PillowCImageToArray(target, 36))
    } finally {
        PillowCFreeImage(source)
        PillowCFreeImage(target)
    }
}

AhkTest.Test("pillow_c image paste copies an inside point region", PillowCTestImagePasteInsidePoint)

PillowCTestImagePasteNegativePointClips(*) {
    target := PillowCCreateImage(4, 3, 3)
    source := PillowCCreateImage(3, 2, 3)
    try {
        PillowCImageSetBytes(target, [
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        ])
        PillowCImageSetBytes(source, [
            1, 2, 3, 4, 5, 6, 7, 8, 9,
            11, 12, 13, 14, 15, 16, 17, 18, 19,
        ])
        PillowCImagePaste(target, source, -1, -1)
        AhkTest.AssertEqual([
            14, 15, 16, 17, 18, 19, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        ], PillowCImageToArray(target, 36))
    } finally {
        PillowCFreeImage(source)
        PillowCFreeImage(target)
    }
}

AhkTest.Test("pillow_c image paste clips a negative point region", PillowCTestImagePasteNegativePointClips)

PillowCTestImagePasteLowerRightPointClips(*) {
    target := PillowCCreateImage(4, 3, 3)
    source := PillowCCreateImage(3, 2, 3)
    try {
        PillowCImageSetBytes(target, [
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        ])
        PillowCImageSetBytes(source, [
            1, 2, 3, 4, 5, 6, 7, 8, 9,
            11, 12, 13, 14, 15, 16, 17, 18, 19,
        ])
        PillowCImagePaste(target, source, 3, 2)
        AhkTest.AssertEqual([
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 1, 2, 3,
        ], PillowCImageToArray(target, 36))
    } finally {
        PillowCFreeImage(source)
        PillowCFreeImage(target)
    }
}

AhkTest.Test("pillow_c image paste clips a lower-right point region", PillowCTestImagePasteLowerRightPointClips)

PillowCTestImagePasteMaskedBlendsAndClipsLikePillow(*) {
    expected := [
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 5, 55, 5, 0, 0, 100,
        10, 10, 10, 58, 10, 10, 10, 200, 10, 10, 10, 10,
    ]
    target := PillowCCreateImageMode(4, 3, 3)
    rgbaTarget := PillowCCreateImageMode(4, 3, 3)
    laTarget := PillowCCreateImageMode(4, 3, 3)
    clippedTarget := PillowCCreateImageMode(4, 3, 3)
    source := PillowCCreateImageMode(3, 2, 3)
    lMask := PillowCCreateImageMode(3, 2, 1)
    laMask := PillowCCreateImageMode(3, 2, 2)
    rgbaMask := PillowCCreateImageMode(3, 2, 4)
    try {
        base := [
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        ]
        PillowCImageSetBytes(target, base)
        PillowCImageSetBytes(rgbaTarget, base)
        PillowCImageSetBytes(laTarget, base)
        PillowCImageSetBytes(clippedTarget, base)
        PillowCImageSetBytes(source, [
            100, 0, 0, 0, 100, 0, 0, 0, 100,
            200, 10, 10, 10, 200, 10, 10, 10, 200,
        ])
        PillowCImageSetBytes(lMask, [0, 128, 255, 64, 255, 0])
        PillowCImageSetBytes(laMask, [9, 0, 9, 128, 9, 255, 9, 64, 9, 255, 9, 0])
        PillowCImageSetBytes(rgbaMask, [
            1, 2, 3, 0,
            4, 5, 6, 128,
            7, 8, 9, 255,
            10, 11, 12, 64,
            13, 14, 15, 255,
            16, 17, 18, 0,
        ])

        PillowCImagePasteMasked(target, source, 1, 1, lMask)
        PillowCImagePasteMasked(rgbaTarget, source, 1, 1, rgbaMask)
        PillowCImagePasteMasked(laTarget, source, 1, 1, laMask)
        PillowCImagePasteMasked(clippedTarget, source, -1, -1, lMask)

        AhkTest.AssertEqual(expected, PillowCImageToArray(target, 36))
        AhkTest.AssertEqual(expected, PillowCImageToArray(rgbaTarget, 36))
        AhkTest.AssertEqual(expected, PillowCImageToArray(laTarget, 36))
        AhkTest.AssertEqual([
            10, 200, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        ], PillowCImageToArray(clippedTarget, 36))
    } finally {
        PillowCFreeImage(rgbaMask)
        PillowCFreeImage(laMask)
        PillowCFreeImage(lMask)
        PillowCFreeImage(source)
        PillowCFreeImage(clippedTarget)
        PillowCFreeImage(laTarget)
        PillowCFreeImage(rgbaTarget)
        PillowCFreeImage(target)
    }
}

AhkTest.Test("pillow_c image paste_masked blends and clips like Pillow", PillowCTestImagePasteMaskedBlendsAndClipsLikePillow)

PillowCTestImagePasteMaskedConvertsSourceToTargetMode(*) {
    target := PillowCCreateImageMode(4, 3, 3)
    source := PillowCCreateImageMode(3, 2, 1)
    mask := PillowCCreateImageMode(3, 2, 1)
    try {
        PillowCImageSetBytes(target, [
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        ])
        PillowCImageSetBytes(source, [0, 50, 100, 150, 200, 250])
        PillowCImageSetBytes(mask, [255, 128, 0, 64, 255, 255])

        PillowCImagePasteMasked(target, source, 1, 1, mask)

        AhkTest.AssertEqual([
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 0, 0, 0, 30, 30, 30, 10, 10, 10,
            10, 10, 10, 45, 45, 45, 200, 200, 200, 250, 250, 250,
        ], PillowCImageToArray(target, 36))
    } finally {
        PillowCFreeImage(mask)
        PillowCFreeImage(source)
        PillowCFreeImage(target)
    }
}

AhkTest.Test("pillow_c image paste_masked converts source to target mode", PillowCTestImagePasteMaskedConvertsSourceToTargetMode)

PillowCTestImagePasteMaskedAcceptsModeOneMask(*) {
    target := PillowCCreateImageMode(4, 1, 1)
    source := PillowCCreateImageMode(2, 1, 1)
    mask := PillowCCreateImageMode(2, 1, 5)
    try {
        PillowCImageSetBytes(target, [1, 2, 3, 4])
        PillowCImageSetBytes(source, [9, 8])
        PillowCImageSetRawBytes(mask, [0x80], "1")

        PillowCImagePasteMasked(target, source, 1, 0, mask)

        AhkTest.AssertEqual([1, 9, 3, 4], PillowCImageToArray(target, 4))
    } finally {
        PillowCFreeImage(mask)
        PillowCFreeImage(source)
        PillowCFreeImage(target)
    }
}

AhkTest.Test("pillow_c image paste_masked accepts mode 1 masks", PillowCTestImagePasteMaskedAcceptsModeOneMask)

PillowCTestImagePasteColorFillsRectangleAndClipsLikePillow(*) {
    target := PillowCCreateImageMode(4, 3, 3)
    clippedTarget := PillowCCreateImageMode(4, 3, 3)
    try {
        base := [
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        ]
        PillowCImageSetBytes(target, base)
        PillowCImageSetBytes(clippedTarget, base)

        PillowCImagePasteColor(target, [9, 8, 7], 1, 0, 3, 2)
        PillowCImagePasteColor(clippedTarget, [9, 8, 7], -1, -1, 2, 1)

        AhkTest.AssertEqual([
            10, 10, 10, 9, 8, 7, 9, 8, 7, 10, 10, 10,
            10, 10, 10, 9, 8, 7, 9, 8, 7, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        ], PillowCImageToArray(target, 36))
        AhkTest.AssertEqual([
            9, 8, 7, 9, 8, 7, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        ], PillowCImageToArray(clippedTarget, 36))
    } finally {
        PillowCFreeImage(clippedTarget)
        PillowCFreeImage(target)
    }
}

AhkTest.Test("pillow_c image paste_color fills rectangles and clips like Pillow", PillowCTestImagePasteColorFillsRectangleAndClipsLikePillow)

PillowCTestImagePasteColorUsesMaskAlpha(*) {
    target := PillowCCreateImageMode(3, 2, 3)
    mask := PillowCCreateImageMode(2, 1, 1)
    try {
        PillowCImageSetBytes(target, [
            0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
        ])
        PillowCImageSetBytes(mask, [0, 128])

        PillowCImagePasteColor(target, [9, 8, 7], 1, 0, 3, 1, mask)

        AhkTest.AssertEqual([
            0, 0, 0, 0, 0, 0, 5, 4, 4,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
        ], PillowCImageToArray(target, 18))
    } finally {
        PillowCFreeImage(mask)
        PillowCFreeImage(target)
    }
}

AhkTest.Test("pillow_c image paste_color blends through masks", PillowCTestImagePasteColorUsesMaskAlpha)

PillowCTestImagePasteColorAcceptsModeOneMask(*) {
    target := PillowCCreateImageMode(4, 1, 3)
    mask := PillowCCreateImageMode(2, 1, 5)
    try {
        PillowCImageSetBytes(target, [
            1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4,
        ])
        PillowCImageSetRawBytes(mask, [0x80], "1")

        PillowCImagePasteColor(target, [9, 8, 7], 1, 0, 3, 1, mask)

        AhkTest.AssertEqual([
            1, 1, 1, 9, 8, 7, 3, 3, 3, 4, 4, 4,
        ], PillowCImageToArray(target, 12))
    } finally {
        PillowCFreeImage(mask)
        PillowCFreeImage(target)
    }
}

AhkTest.Test("pillow_c image paste_color accepts mode 1 masks", PillowCTestImagePasteColorAcceptsModeOneMask)

PillowCTestImagePasteRejectsChannelMismatch(*) {
    target := PillowCCreateImage(2, 2, 3)
    source := PillowCCreateImage(1, 1, 1)
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_paste",
            "Ptr", target,
            "Ptr", source,
            "Int", 1,
            "Int", 1,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        PillowCFreeImage(source)
        PillowCFreeImage(target)
    }
}

AhkTest.Test("pillow_c image paste rejects channel mismatch for the raw fast path", PillowCTestImagePasteRejectsChannelMismatch)

PillowCAssertTranspose(method, expectedWidth, expectedHeight, expectedBytes) {
    source := PillowCCreateImage(3, 2, 3)
    transposed := 0
    try {
        PillowCImageSetBytes(source, [
            10, 20, 30, 40, 50, 60, 70, 80, 90,
            100, 110, 120, 130, 140, 150, 160, 170, 180,
        ])
        transposed := PillowCImageTranspose(source, method)
        AhkTest.AssertEqual(expectedWidth, PillowCImageInt(transposed, "pillow_c_image_width"))
        AhkTest.AssertEqual(expectedHeight, PillowCImageInt(transposed, "pillow_c_image_height"))
        AhkTest.AssertEqual(3, PillowCImageInt(transposed, "pillow_c_image_channels"))
        AhkTest.AssertEqual(expectedBytes, PillowCImageToArray(transposed, expectedBytes.Length))
    } finally {
        if transposed
            PillowCFreeImage(transposed)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image transpose flips left-right", (*) =>
    PillowCAssertTranspose(0, 3, 2, [
        70, 80, 90, 40, 50, 60, 10, 20, 30,
        160, 170, 180, 130, 140, 150, 100, 110, 120,
    ]))

PillowCTestImageTransposeIntoReusesTargetHandle(*) {
    source := PillowCCreateImage(3, 2, 3)
    target := PillowCCreateImage(2, 3, 3)
    try {
        PillowCImageSetBytes(source, [
            10, 20, 30, 40, 50, 60, 70, 80, 90,
            100, 110, 120, 130, 140, 150, 160, 170, 180,
        ])
        before := PillowCImageData(target).Ptr
        PillowCImageTransposeInto(source, 2, target)
        after := PillowCImageData(target).Ptr
        AhkTest.AssertEqual(before, after)
        AhkTest.AssertEqual([
            70, 80, 90, 160, 170, 180,
            40, 50, 60, 130, 140, 150,
            10, 20, 30, 100, 110, 120,
        ], PillowCImageToArray(target, 18))
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image transpose_into reuses target handle storage", PillowCTestImageTransposeIntoReusesTargetHandle)

AhkTest.Test("pillow_c image transpose flips top-bottom", (*) =>
    PillowCAssertTranspose(1, 3, 2, [
        100, 110, 120, 130, 140, 150, 160, 170, 180,
        10, 20, 30, 40, 50, 60, 70, 80, 90,
    ]))

AhkTest.Test("pillow_c image transpose rotates 90", (*) =>
    PillowCAssertTranspose(2, 2, 3, [
        70, 80, 90, 160, 170, 180,
        40, 50, 60, 130, 140, 150,
        10, 20, 30, 100, 110, 120,
    ]))

AhkTest.Test("pillow_c image transpose rotates 180", (*) =>
    PillowCAssertTranspose(3, 3, 2, [
        160, 170, 180, 130, 140, 150, 100, 110, 120,
        70, 80, 90, 40, 50, 60, 10, 20, 30,
    ]))

AhkTest.Test("pillow_c image transpose rotates 270", (*) =>
    PillowCAssertTranspose(4, 2, 3, [
        100, 110, 120, 10, 20, 30,
        130, 140, 150, 40, 50, 60,
        160, 170, 180, 70, 80, 90,
    ]))

AhkTest.Test("pillow_c image transpose swaps across main diagonal", (*) =>
    PillowCAssertTranspose(5, 2, 3, [
        10, 20, 30, 100, 110, 120,
        40, 50, 60, 130, 140, 150,
        70, 80, 90, 160, 170, 180,
    ]))

AhkTest.Test("pillow_c image transpose swaps across anti-diagonal", (*) =>
    PillowCAssertTranspose(6, 2, 3, [
        160, 170, 180, 70, 80, 90,
        130, 140, 150, 40, 50, 60,
        100, 110, 120, 10, 20, 30,
    ]))

PillowCTestImageTransposeRejectsInvalidMethod(*) {
    source := PillowCCreateImage(3, 2, 3)
    transposed := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_transpose",
            "Ptr", source,
            "Int", 999,
            "Ptr*", &transposed,
            "Int"
        )
        AhkTest.AssertEqual(-3, status)
        AhkTest.AssertEqual(0, transposed)
    } finally {
        if transposed
            PillowCFreeImage(transposed)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image transpose rejects an invalid Pillow method", PillowCTestImageTransposeRejectsInvalidMethod)

PillowCTestImageTransposeRotatesZeroWidthMetadata(*) {
    source := PillowCCreateImage(3, 2, 3)
    zeroWidth := 0
    transposed := 0
    try {
        PillowCImageSetBytes(source, [
            10, 20, 30, 40, 50, 60, 70, 80, 90,
            100, 110, 120, 130, 140, 150, 160, 170, 180,
        ])
        zeroWidth := PillowCImageCrop(source, 1, 1, 1, 2)
        transposed := PillowCImageTranspose(zeroWidth, 2)
        AhkTest.AssertEqual(1, PillowCImageInt(transposed, "pillow_c_image_width"))
        AhkTest.AssertEqual(0, PillowCImageInt(transposed, "pillow_c_image_height"))
        AhkTest.AssertEqual(3, PillowCImageInt(transposed, "pillow_c_image_channels"))
        AhkTest.AssertEqual(0, PillowCImageSize(transposed))
        AhkTest.AssertEqual([], PillowCImageToArray(transposed, 0))
    } finally {
        if transposed
            PillowCFreeImage(transposed)
        if zeroWidth
            PillowCFreeImage(zeroWidth)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image transpose rotates zero-width metadata", PillowCTestImageTransposeRotatesZeroWidthMetadata)

PillowCTestImageIntoRejectsShapeMismatch(*) {
    source := PillowCCreateImage(3, 2, 3)
    target := PillowCCreateImage(3, 2, 3)
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_transpose_into",
            "Ptr", source,
            "Int", 2,
            "Ptr", target,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        PillowCFreeImage(target)
        PillowCFreeImage(source)
    }
}

AhkTest.Test("pillow_c image into operations reject target shape mismatch", PillowCTestImageIntoRejectsShapeMismatch)

PillowCColor3DLutIdentityTable3() {
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

PillowCColor3DLutTable4() {
    table := []
    loop 2 {
        b := A_Index - 1
        loop 2 {
            g := A_Index - 1
            loop 2 {
                r := A_Index - 1
                table.Push(1 - r)
                table.Push(g * 0.5)
                table.Push(b * 0.25)
                table.Push((r + g + b) / 3.0)
            }
        }
    }
    return table
}

PillowCTestImageFilterColor3DLutMatchesPillowRgbRgbaAndCmyk(*) {
    rgb := PillowCCreateImageMode(3, 2, 3)
    rgba := PillowCCreateImageMode(2, 2, 4)
    cmyk := PillowCCreateImageMode(2, 1, 7)
    rgbaTarget := PillowCCreateImageMode(2, 2, 4)
    rgb3 := 0
    rgba3 := 0
    rgba4 := 0
    rgbToRgba4 := 0
    rgbaToRgb3 := 0
    cmyk3 := 0
    try {
        PillowCImageSetBytes(rgb, [
            0, 0, 0, 127, 64, 32, 255, 255, 255,
            32, 200, 128, 240, 10, 80, 90, 120, 250,
        ])
        PillowCImageSetBytes(rgba, [
            0, 0, 0, 10, 128, 64, 32, 40,
            255, 255, 255, 90, 32, 200, 128, 160,
        ])
        PillowCImageSetBytes(cmyk, [
            255, 127, 0, 33,
            55, 215, 175, 77,
        ])
        PillowCImageSetBytes(rgbaTarget, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16])

        table3 := PillowCColor3DLutIdentityTable3()
        table4 := PillowCColor3DLutTable4()
        rgb3 := PillowCImageFilterColor3DLut(rgb, 3, 3, [2, 2, 2], table3)
        rgba3 := PillowCImageFilterColor3DLut(rgba, 4, 3, [2, 2, 2], table3)
        rgba4 := PillowCImageFilterColor3DLut(rgba, 4, 4, [2, 2, 2], table4)
        rgbToRgba4 := PillowCImageFilterColor3DLut(rgb, 4, 4, [2, 2, 2], table4)
        rgbaToRgb3 := PillowCImageFilterColor3DLut(rgba, 3, 3, [2, 2, 2], table3)
        cmyk3 := PillowCImageFilterColor3DLut(cmyk, 7, 3, [2, 2, 2], table3)
        before := PillowCImageData(rgbaTarget).Ptr
        PillowCImageFilterColor3DLutInto(rgba, 4, 4, [2, 2, 2], table4, rgbaTarget)

        AhkTest.AssertEqual(3, PillowCImageMode(rgb3))
        AhkTest.AssertEqual([0, 0, 0, 127, 64, 32, 255, 255, 255, 32, 200, 128, 240, 10, 80, 90, 120, 250], PillowCImageToArray(rgb3, 18))
        AhkTest.AssertEqual(4, PillowCImageMode(rgba3))
        AhkTest.AssertEqual([0, 0, 0, 10, 128, 64, 32, 40, 255, 255, 255, 90, 32, 200, 128, 160], PillowCImageToArray(rgba3, 16))
        AhkTest.AssertEqual([255, 0, 0, 0, 127, 32, 8, 75, 0, 127, 64, 255, 223, 100, 32, 120], PillowCImageToArray(rgba4, 16))
        AhkTest.AssertEqual([255, 0, 0, 0, 128, 32, 8, 74, 0, 127, 64, 255, 223, 100, 32, 120, 15, 5, 20, 110, 165, 60, 62, 153], PillowCImageToArray(rgbToRgba4, 24))
        AhkTest.AssertEqual(3, PillowCImageMode(rgbaToRgb3))
        AhkTest.AssertEqual([0, 0, 0, 128, 64, 32, 255, 255, 255, 32, 200, 128], PillowCImageToArray(rgbaToRgb3, 12))
        AhkTest.AssertEqual(7, PillowCImageMode(cmyk3))
        AhkTest.AssertEqual([255, 127, 0, 33, 55, 215, 175, 77], PillowCImageToArray(cmyk3, 8))
        AhkTest.AssertEqual(before, PillowCImageData(rgbaTarget).Ptr)
        AhkTest.AssertEqual(PillowCImageToArray(rgba4, 16), PillowCImageToArray(rgbaTarget, 16))
    } finally {
        for handle in [cmyk3, rgbaToRgb3, rgbToRgba4, rgba4, rgba3, rgb3, rgbaTarget, cmyk, rgba, rgb] {
            if handle
                PillowCFreeImage(handle)
        }
    }
}

AhkTest.Test("pillow_c image filter Color3DLUT matches Pillow RGB RGBA and CMYK", PillowCTestImageFilterColor3DLutMatchesPillowRgbRgbaAndCmyk)

PillowCTestImageFilterColor3DLutRejectsInvalidArguments(*) {
    rgb := PillowCCreateImageMode(1, 1, 3)
    l := PillowCCreateImageMode(1, 1, 1)
    wrongTarget := PillowCCreateImageMode(1, 1, 3)
    outHandle := 0
    try {
        PillowCImageSetBytes(rgb, [1, 2, 3])
        table3 := PillowCColor3DLutIdentityTable3()
        cases := [
            { Source: rgb, TargetMode: 3, Channels: 2, Size: [2, 2, 2], Table: table3, Status: -3 },
            { Source: rgb, TargetMode: 3, Channels: 3, Size: [1, 2, 2], Table: table3, Status: -3 },
            { Source: rgb, TargetMode: 3, Channels: 3, Size: [2, 2, 2], Table: [0, 0, 0], Status: -2 },
            { Source: rgb, TargetMode: 4, Channels: 3, Size: [2, 2, 2], Table: table3, Status: -3 },
            { Source: l, TargetMode: 1, Channels: 3, Size: [2, 2, 2], Table: table3, Status: -3 },
        ]
        for item in cases {
            table := PillowCDoubleBuffer(item.Table)
            status := DllCall(
                PillowCDllPath() "\pillow_c_image_filter_color_3d_lut",
                "Ptr", item.Source,
                "Int", item.TargetMode,
                "Int", item.Channels,
                "Int", item.Size[1],
                "Int", item.Size[2],
                "Int", item.Size[3],
                "Ptr", table,
                "UPtr", item.Table.Length,
                "Ptr*", &outHandle,
                "Int"
            )
            AhkTest.AssertEqual(item.Status, status)
            AhkTest.AssertEqual(0, outHandle)
        }

        status := DllCall(
            PillowCDllPath() "\pillow_c_image_filter_color_3d_lut_into",
            "Ptr", rgb,
            "Int", 4,
            "Int", 4,
            "Int", 2,
            "Int", 2,
            "Int", 2,
            "Ptr", PillowCDoubleBuffer(PillowCColor3DLutTable4()),
            "UPtr", 32,
            "Ptr", wrongTarget,
            "Int"
        )
        AhkTest.AssertEqual(-5, status)
    } finally {
        if outHandle
            PillowCFreeImage(outHandle)
        PillowCFreeImage(wrongTarget)
        PillowCFreeImage(l)
        PillowCFreeImage(rgb)
    }
}

AhkTest.Test("pillow_c image filter Color3DLUT rejects invalid arguments", PillowCTestImageFilterColor3DLutRejectsInvalidArguments)
