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
    AhkTest.AssertEqual(1, PillowCModeFromString("L")),
    AhkTest.AssertEqual(3, PillowCModeFromString("RGB")),
    AhkTest.AssertEqual(4, PillowCModeFromString("RGBA"))
))

AhkTest.Test("pillow_c maps native mode ids back to Pillow mode names", (*) => (
    l := PillowCModeName(1),
    rgb := PillowCModeName(3),
    rgba := PillowCModeName(4),
    AhkTest.AssertEqual("L", l.Text),
    AhkTest.AssertEqual(2, l.Required),
    AhkTest.AssertEqual("RGB", rgb.Text),
    AhkTest.AssertEqual(4, rgb.Required),
    AhkTest.AssertEqual("RGBA", rgba.Text),
    AhkTest.AssertEqual(5, rgba.Required)
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
    l := PillowCCreateImageMode(3, 2, 1)
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    try {
        AhkTest.AssertEqual(1, PillowCImageMode(l))
        AhkTest.AssertEqual(1, PillowCImageInt(l, "pillow_c_image_channels"))
        AhkTest.AssertEqual(6, PillowCImageSize(l))
        AhkTest.AssertEqual(3, PillowCImageMode(rgb))
        AhkTest.AssertEqual(3, PillowCImageInt(rgb, "pillow_c_image_channels"))
        AhkTest.AssertEqual(6, PillowCImageSize(rgb))
        AhkTest.AssertEqual(4, PillowCImageMode(rgba))
        AhkTest.AssertEqual(4, PillowCImageInt(rgba, "pillow_c_image_channels"))
        AhkTest.AssertEqual(8, PillowCImageSize(rgba))
    } finally {
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image handle owns Pillow mode metadata", PillowCTestImageHandleOwnsModeMetadata)

PillowCTestImageCreateKeepsLegacyChannelModeMapping(*) {
    l := PillowCCreateImage(3, 2, 1)
    rgb := PillowCCreateImage(2, 1, 3)
    rgba := PillowCCreateImage(2, 1, 4)
    try {
        AhkTest.AssertEqual(1, PillowCImageMode(l))
        AhkTest.AssertEqual(3, PillowCImageMode(rgb))
        AhkTest.AssertEqual(4, PillowCImageMode(rgba))
    } finally {
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
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

PillowCMakeInvertLut(channelCount) {
    lut := []
    loop channelCount {
        loop 256
            lut.Push(256 - A_Index)
    }
    return lut
}

PillowCTestImagePointLutAppliesPerChannelTables(*) {
    l := PillowCCreateImageMode(4, 1, 1)
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    lOut := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l, [0, 1, 2, 255])
        PillowCImageSetBytes(rgb, [0, 1, 2, 10, 20, 30])
        PillowCImageSetBytes(rgba, [0, 1, 2, 3, 10, 20, 30, 40])
        lOut := PillowCImagePointLut(l, PillowCMakeInvertLut(1))
        rgbOut := PillowCImagePointLut(rgb, PillowCMakeInvertLut(3))
        rgbaOut := PillowCImagePointLut(rgba, PillowCMakeInvertLut(4))
        AhkTest.AssertEqual([255, 254, 253, 0], PillowCImageToArray(lOut, 4))
        AhkTest.AssertEqual([255, 254, 253, 245, 235, 225], PillowCImageToArray(rgbOut, 6))
        AhkTest.AssertEqual([255, 254, 253, 252, 245, 235, 225, 215], PillowCImageToArray(rgbaOut, 8))
    } finally {
        if rgbaOut
            PillowCFreeImage(rgbaOut)
        if rgbOut
            PillowCFreeImage(rgbOut)
        if lOut
            PillowCFreeImage(lOut)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image point_lut applies per-channel Pillow LUT tables", PillowCTestImagePointLutAppliesPerChannelTables)

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

PillowCTestImageGetChannelReturnsLImage(*) {
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    r := 0
    g := 0
    a := 0
    try {
        PillowCImageSetBytes(rgb, [1, 2, 3, 10, 20, 30])
        PillowCImageSetBytes(rgba, [1, 2, 3, 4, 10, 20, 30, 40])
        r := PillowCImageGetChannel(rgb, 0)
        g := PillowCImageGetChannel(rgb, 1)
        a := PillowCImageGetChannel(rgba, 3)
        AhkTest.AssertEqual(1, PillowCImageMode(r))
        AhkTest.AssertEqual(1, PillowCImageInt(r, "pillow_c_image_channels"))
        AhkTest.AssertEqual([2, 1], [PillowCImageInt(r, "pillow_c_image_width"), PillowCImageInt(r, "pillow_c_image_height")])
        AhkTest.AssertEqual([1, 10], PillowCImageToArray(r, 2))
        AhkTest.AssertEqual([2, 20], PillowCImageToArray(g, 2))
        AhkTest.AssertEqual([4, 40], PillowCImageToArray(a, 2))
    } finally {
        if a
            PillowCFreeImage(a)
        if g
            PillowCFreeImage(g)
        if r
            PillowCFreeImage(r)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
    }
}

AhkTest.Test("pillow_c image get_channel returns an L image for one channel", PillowCTestImageGetChannelReturnsLImage)

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

PillowCTestImagePutAlphaValueReturnsRgba(*) {
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(rgb, [1, 2, 3, 10, 20, 30])
        PillowCImageSetBytes(rgba, [1, 2, 3, 4, 10, 20, 30, 40])
        rgbOut := PillowCImagePutAlphaValue(rgb, 7)
        rgbaOut := PillowCImagePutAlphaValue(rgba, 8)
        AhkTest.AssertEqual(4, PillowCImageMode(rgbOut))
        AhkTest.AssertEqual([1, 2, 3, 7, 10, 20, 30, 7], PillowCImageToArray(rgbOut, 8))
        AhkTest.AssertEqual([1, 2, 3, 8, 10, 20, 30, 8], PillowCImageToArray(rgbaOut, 8))
    } finally {
        if rgbaOut
            PillowCFreeImage(rgbaOut)
        if rgbOut
            PillowCFreeImage(rgbOut)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
    }
}

AhkTest.Test("pillow_c image put_alpha_value returns an RGBA image", PillowCTestImagePutAlphaValueReturnsRgba)

PillowCTestImagePutAlphaImageReturnsRgba(*) {
    rgb := PillowCCreateImageMode(2, 1, 3)
    alpha := PillowCCreateImageMode(2, 1, 1)
    out := 0
    try {
        PillowCImageSetBytes(rgb, [1, 2, 3, 10, 20, 30])
        PillowCImageSetBytes(alpha, [50, 60])
        out := PillowCImagePutAlphaImage(rgb, alpha)
        AhkTest.AssertEqual(4, PillowCImageMode(out))
        AhkTest.AssertEqual([1, 2, 3, 50, 10, 20, 30, 60], PillowCImageToArray(out, 8))
    } finally {
        if out
            PillowCFreeImage(out)
        PillowCFreeImage(alpha)
        PillowCFreeImage(rgb)
    }
}

AhkTest.Test("pillow_c image put_alpha_image accepts an L alpha image", PillowCTestImagePutAlphaImageReturnsRgba)

PillowCTestImagePutAlphaRejectsUnsupportedSourceMode(*) {
    source := PillowCCreateImageMode(2, 1, 1)
    outHandle := 0
    try {
        status := DllCall(
            PillowCDllPath() "\pillow_c_image_put_alpha_value",
            "Ptr", source,
            "UChar", 7,
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

AhkTest.Test("pillow_c image put_alpha rejects unsupported source modes until LA exists", PillowCTestImagePutAlphaRejectsUnsupportedSourceMode)

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
