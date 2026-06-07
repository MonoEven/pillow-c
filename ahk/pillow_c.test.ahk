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

PillowCTestImageGetAndPutPixelMatchPillowCoreModes(*) {
    l := PillowCCreateImageMode(2, 2, 1)
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(1, 2, 4)
    try {
        PillowCImageSetBytes(l, [1, 2, 3, 4])
        PillowCImageSetBytes(rgb, [1, 2, 3, 4, 5, 6])
        PillowCImageSetBytes(rgba, [1, 2, 3, 4, 5, 6, 7, 8])

        AhkTest.AssertEqual([1], PillowCImageGetPixel(l, 0, 0, 1))
        AhkTest.AssertEqual([2], PillowCImageGetPixel(l, -1, 0, 1))
        AhkTest.AssertEqual([4, 5, 6], PillowCImageGetPixel(rgb, 1, 0, 3))
        AhkTest.AssertEqual([1, 2, 3, 4], PillowCImageGetPixel(rgba, -1, 0, 4))

        PillowCImagePutPixel(l, 0, 0, [9])
        PillowCImagePutPixel(rgb, 0, 0, [9, 0, 0])
        PillowCImagePutPixel(rgba, 0, 0, [9, 9, 9, 9])

        AhkTest.AssertEqual([9, 2, 3, 4], PillowCImageToArray(l, 4))
        AhkTest.AssertEqual([9, 0, 0, 4, 5, 6], PillowCImageToArray(rgb, 6))
        AhkTest.AssertEqual([9, 9, 9, 9, 5, 6, 7, 8], PillowCImageToArray(rgba, 8))
    } finally {
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
    rgb := PillowCCreateImageMode(4, 1, 3)
    rgbMask := PillowCCreateImageMode(4, 1, 1)
    lOut := 0
    lIgnoreOut := 0
    rgbOut := 0
    try {
        PillowCImageSetBytes(l, [0, 10, 20, 30, 255])
        PillowCImageSetBytes(lMask, [0, 255, 255, 0, 0])
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
        rgbOut := PillowCImageAutocontrast(rgb, 0.0, 0.0, 0, 0, rgbMask)

        AhkTest.AssertEqual([0, 0, 255, 255, 255], PillowCImageToArray(lOut, 5))
        AhkTest.AssertEqual([0, 10, 20, 30, 255], PillowCImageToArray(lIgnoreOut, 5))
        AhkTest.AssertEqual([0, 0, 0, 0, 0, 0, 255, 255, 254, 255, 0, 0], PillowCImageToArray(rgbOut, 12))
    } finally {
        for handle in [rgbOut, lIgnoreOut, lOut, rgbMask, rgb, lMask, l] {
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

PillowCTestImageSplitBandsReturnsAllLBandsInOneCall(*) {
    l := PillowCCreateImageMode(3, 1, 1)
    rgb := PillowCCreateImageMode(2, 1, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    lBands := []
    rgbBands := []
    rgbaBands := []
    try {
        PillowCImageSetBytes(l, [1, 2, 3])
        PillowCImageSetBytes(rgb, [1, 2, 3, 10, 20, 30])
        PillowCImageSetBytes(rgba, [1, 2, 3, 4, 10, 20, 30, 40])
        lBands := PillowCImageSplitBands(l, 1)
        rgbBands := PillowCImageSplitBands(rgb, 3)
        rgbaBands := PillowCImageSplitBands(rgba, 4)

        AhkTest.AssertEqual(1, PillowCImageMode(lBands[1]))
        AhkTest.AssertEqual([1, 2, 3], PillowCImageToArray(lBands[1], 3))
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
        for handle in lBands
            PillowCFreeImage(handle)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image split_bands returns all L bands in one call", PillowCTestImageSplitBandsReturnsAllLBandsInOneCall)

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
    rgb := 0
    rgba := 0
    try {
        PillowCImageSetBytes(r, [1, 2])
        PillowCImageSetBytes(g, [3, 4])
        PillowCImageSetBytes(b, [5, 6])
        PillowCImageSetBytes(a, [7, 8])
        rgb := PillowCImageMergeBands(3, [r, g, b])
        rgba := PillowCImageMergeBands(4, [r, g, b, a])
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
    rgb := PillowCCreateImageMode(2, 2, 3)
    rgba := PillowCCreateImageMode(2, 1, 4)
    lOut := 0
    lSmall := 0
    lSmallSource := 0
    lTall := 0
    lTallSource := 0
    rgbOut := 0
    rgbaOut := 0
    try {
        PillowCImageSetBytes(l, [0, 64, 128, 255])
        PillowCImageSetBytes(rgb, [
            0, 0, 0, 255, 0, 0,
            0, 255, 0, 0, 0, 255,
        ])
        PillowCImageSetBytes(rgba, [
            0, 0, 0, 0,
            255, 100, 50, 255,
        ])
        lOut := PillowCImageResize(l, 3, 3, 2)
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
        if lOut
            PillowCFreeImage(lOut)
        PillowCFreeImage(rgba)
        PillowCFreeImage(rgb)
        PillowCFreeImage(l)
    }
}

AhkTest.Test("pillow_c image resize BILINEAR matches Pillow filter sampling", PillowCTestImageResizeBilinearMatchesPillowFilterSampling)

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

        lOut := PillowCImageTransformAffine(l, 3, 2, [1.0, 0.0, -0.5, 0.0, 1.0, 0.0], 2, [8])
        rgbOut := PillowCImageTransformAffine(rgb, 4, 3, [0.75, 0.0, 0.0, 0.0, 0.75, 0.0], 3, [9, 0, 0])
        rgbaOut := PillowCImageTransformAffine(rgba, 4, 4, [1.0, 0.0, -1.0, 0.0, 1.0, -1.0], 2, [9, 0, 0, 128])

        AhkTest.AssertEqual([1, 1, 2, 4, 4, 5], PillowCImageToArray(lOut, 6))
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
        for handle in [rgbaOut, rgbOut, lOut, rgba, rgb, l] {
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

        lOut := PillowCImageRotate(l, 45, 2)
        rgbOut := PillowCImageRotate(rgb, 45, 2)
        rgbaOut := PillowCImageRotate(rgba, 45, 2)

        AhkTest.AssertEqual([0, 2, 5, 1, 4, 0], PillowCImageToArray(lOut, 6))
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
        for handle in [rgbaOut, rgbOut, lOut, rgba, rgb, l] {
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
