#Requires AutoHotkey v2.0

class Pillow {
    static DllPath := ""

    class Transpose {
        static FLIP_LEFT_RIGHT := 0
        static FLIP_TOP_BOTTOM := 1
        static ROTATE_90 := 2
        static ROTATE_180 := 3
        static ROTATE_270 := 4
        static TRANSPOSE := 5
        static TRANSVERSE := 6
    }

    class Resampling {
        static NEAREST := 0
        static BOX := 4
        static BILINEAR := 2
        static HAMMING := 5
        static BICUBIC := 3
        static LANCZOS := 1
    }

    class ImageOps {
        static Invert(image) {
            return Pillow.ImageOps.NativeUnaryImageOp(image, "pillow_c_image_invert")
        }

        static Posterize(image, bits) {
            if !(bits is Integer)
                throw Error("Pillow.ImageOps.Posterize bits must be an integer", -1)
            if bits > 8
                throw Error("Pillow.ImageOps.Posterize bits must be 8 or less", -1)
            if bits < -31
                throw Error("Pillow.ImageOps.Posterize bits is out of native range", -1)

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_posterize",
                "Ptr", Pillow.ImageOps.RequireImageHandle(image, "Posterize"),
                "Int", bits,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Solarize(image, threshold := 128) {
            if !(threshold is Number)
                throw Error("Pillow.ImageOps.Solarize threshold must be numeric", -1)

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_solarize",
                "Ptr", Pillow.ImageOps.RequireImageHandle(image, "Solarize"),
                "Double", threshold,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Equalize(image) {
            return Pillow.ImageOps.NativeUnaryImageOp(image, "pillow_c_image_equalize")
        }

        static Expand(image, border := 0, fill := 0) {
            handle := Pillow.ImageOps.RequireImageHandle(image, "Expand")
            borders := Pillow.ImageOps.BorderBox(border)
            color := Pillow.ImageOps.FillBuffer(image, fill)
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_expand",
                "Ptr", handle,
                "Int", borders[1],
                "Int", borders[2],
                "Int", borders[3],
                "Int", borders[4],
                "Ptr", color,
                "UPtr", color.Size,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Contain(image, size, method := unset) {
            return Pillow.ImageOps.NativeProportionalResize(image, size, IsSet(method) ? method : Pillow.Resampling.BICUBIC, "pillow_c_image_contain")
        }

        static Cover(image, size, method := unset) {
            return Pillow.ImageOps.NativeProportionalResize(image, size, IsSet(method) ? method : Pillow.Resampling.BICUBIC, "pillow_c_image_cover")
        }

        static Fit(image, size, method := unset, bleed := 0.0, centering := unset) {
            if !IsObject(size) || size.Length != 2
                throw Error("Pillow.ImageOps.Fit expects size [width, height]", -1)
            center := Pillow.ImageOps.CenteringPair(IsSet(centering) ? centering : [0.5, 0.5], "Fit")
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_fit",
                "Ptr", Pillow.ImageOps.RequireImageHandle(image, "Fit"),
                "Int", size[1],
                "Int", size[2],
                "Int", IsSet(method) ? method : Pillow.Resampling.BICUBIC,
                "Double", bleed,
                "Double", center[1],
                "Double", center[2],
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Pad(image, size, method := unset, color := unset, centering := unset) {
            if !IsObject(size) || size.Length != 2
                throw Error("Pillow.ImageOps.Pad expects size [width, height]", -1)
            handle := Pillow.ImageOps.RequireImageHandle(image, "Pad")
            fill := Pillow.ImageOps.FillBuffer(image, IsSet(color) ? color : 0, "Pad")
            center := Pillow.ImageOps.CenteringPair(IsSet(centering) ? centering : [0.5, 0.5], "Pad")
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_pad",
                "Ptr", handle,
                "Int", size[1],
                "Int", size[2],
                "Int", IsSet(method) ? method : Pillow.Resampling.BICUBIC,
                "Ptr", fill,
                "UPtr", fill.Size,
                "Double", center[1],
                "Double", center[2],
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Autocontrast(image, cutoff := 0, ignore := unset) {
            cuts := Pillow.ImageOps.CutoffPair(cutoff)
            ignorePtr := 0
            ignoreCount := 0
            ignoreBuffer := 0
            if IsSet(ignore) {
                ignoreBuffer := Pillow.ImageOps.IgnoreBuffer(ignore)
                ignorePtr := ignoreBuffer.Ptr
                ignoreCount := ignoreBuffer.Size
            }

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_autocontrast",
                "Ptr", Pillow.ImageOps.RequireImageHandle(image, "Autocontrast"),
                "Double", cuts[1],
                "Double", cuts[2],
                "Ptr", ignorePtr,
                "UPtr", ignoreCount,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static NativeProportionalResize(image, size, method, exportName) {
            if !IsObject(size) || size.Length != 2
                throw Error("Pillow.ImageOps proportional resize expects size [width, height]", -1)
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\" exportName,
                "Ptr", Pillow.ImageOps.RequireImageHandle(image, exportName),
                "Int", size[1],
                "Int", size[2],
                "Int", method,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static NativeUnaryImageOp(image, exportName) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\" exportName,
                "Ptr", Pillow.ImageOps.RequireImageHandle(image, exportName),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static RequireImageHandle(image, operationName) {
            if !(IsObject(image) && image is Pillow.Image)
                throw Error("Pillow.ImageOps." operationName " expects a Pillow.Image", -1)
            return image.RequireHandle()
        }

        static BorderBox(border) {
            if IsObject(border) {
                if border.Length = 2
                    return [border[1], border[2], border[1], border[2]]
                if border.Length = 4
                    return [border[1], border[2], border[3], border[4]]
                throw Error("Pillow.ImageOps.Expand border expects a number, [x, y], or [left, top, right, bottom]", -1)
            }
            return [border, border, border, border]
        }

        static FillBuffer(image, fill, operationName := "Expand") {
            channels := image.Channels
            buf := Buffer(channels, 0)
            if IsObject(fill) {
                if channels = 1 {
                    if fill.Length != 1
                        throw Error("Pillow.ImageOps." operationName " fill must match image mode", -1)
                    NumPut("UChar", fill[1], buf, 0)
                    return buf
                }
                if channels = 3 {
                    if fill.Length != 3 && fill.Length != 4
                        throw Error("Pillow.ImageOps." operationName " fill must match image mode", -1)
                    loop 3
                        NumPut("UChar", fill[A_Index], buf, A_Index - 1)
                    return buf
                }
                if channels = 4 {
                    if fill.Length != 3 && fill.Length != 4
                        throw Error("Pillow.ImageOps." operationName " fill must match image mode", -1)
                    NumPut("UChar", fill[1], buf, 0)
                    NumPut("UChar", fill[2], buf, 1)
                    NumPut("UChar", fill[3], buf, 2)
                    NumPut("UChar", fill.Length = 4 ? fill[4] : 255, buf, 3)
                    return buf
                }
                throw Error("Pillow.ImageOps." operationName " fill is unsupported for this image mode", -1)
            }

            NumPut("UChar", fill, buf, 0)
            return buf
        }

        static CenteringPair(centering, operationName := "Pad") {
            if !IsObject(centering) || centering.Length != 2
                throw Error("Pillow.ImageOps." operationName " centering expects [x, y]", -1)
            return [centering[1], centering[2]]
        }

        static CutoffPair(cutoff) {
            if IsObject(cutoff) {
                if cutoff.Length != 2
                    throw Error("Pillow.ImageOps.Autocontrast cutoff expects a number or [low, high]", -1)
                return [cutoff[1], cutoff[2]]
            }
            return [cutoff, cutoff]
        }

        static IgnoreBuffer(ignore) {
            if IsObject(ignore) {
                buf := Buffer(ignore.Length, 0)
                for index, value in ignore
                    NumPut("UChar", Pillow.ImageOps.NormalizeIgnoreValue(value), buf, index - 1)
                return buf
            }

            buf := Buffer(1, 0)
            NumPut("UChar", Pillow.ImageOps.NormalizeIgnoreValue(ignore), buf, 0)
            return buf
        }

        static NormalizeIgnoreValue(value) {
            if !(value is Integer)
                throw Error("Pillow.ImageOps.Autocontrast ignore values must be integers", -1)
            if value < -256 || value > 255
                throw Error("Pillow.ImageOps.Autocontrast ignore index out of range", -1)
            return value < 0 ? value + 256 : value
        }
    }

    class ImageChops {
        static Difference(left, right) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_difference",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "Difference"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "Difference"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Multiply(left, right) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_multiply",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "Multiply"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "Multiply"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static RequireImageHandle(image, operationName) {
            if !(IsObject(image) && image is Pillow.Image)
                throw Error("Pillow.ImageChops." operationName " expects a Pillow.Image", -1)
            return image.RequireHandle()
        }
    }

    static Configure(options := unset) {
        if IsSet(options) && options.HasOwnProp("DllPath")
            Pillow.DllPath := options.DllPath
    }

    static RequireDllPath() {
        if Pillow.DllPath = "" {
            SplitPath A_LineFile, , &ahkDir
            Pillow.DllPath := ahkDir "\..\build\x64\Release\pillow_c.dll"
        }
        return Pillow.DllPath
    }

    static AbiVersion() {
        major := 0
        minor := 0
        patch := 0
        Pillow.CheckStatus(DllCall(
            Pillow.RequireDllPath() "\pillow_c_abi_version",
            "Int*", &major,
            "Int*", &minor,
            "Int*", &patch,
            "Int"
        ))
        return [major, minor, patch]
    }

    static CheckStatus(status) {
        if status = 0
            return
        throw Error("pillow_c: " Pillow.StatusMessage(status), -2)
    }

    static StatusMessage(statusCode) {
        required := 0
        status := DllCall(
            Pillow.RequireDllPath() "\pillow_c_status_message",
            "Int", statusCode,
            "Ptr", 0,
            "UPtr", 0,
            "UPtr*", &required,
            "Int"
        )
        if status != -1 || required <= 0
            return "status " statusCode

        buf := Buffer(required, 0)
        status := DllCall(
            Pillow.RequireDllPath() "\pillow_c_status_message",
            "Int", statusCode,
            "Ptr", buf,
            "UPtr", buf.Size,
            "UPtr*", &required,
            "Int"
        )
        if status != 0
            return "status " statusCode
        return StrGet(buf.Ptr, required - 1, "UTF-8")
    }

    static ModeId(modeName) {
        mode := -1
        data := Buffer(StrPut(modeName, "UTF-8"), 0)
        StrPut(modeName, data, "UTF-8")
        Pillow.CheckStatus(DllCall(
            Pillow.RequireDllPath() "\pillow_c_mode_from_string",
            "Ptr", data,
            "Int*", &mode,
            "Int"
        ))
        return mode
    }

    static ModeName(mode) {
        required := 0
        status := DllCall(
            Pillow.RequireDllPath() "\pillow_c_mode_name",
            "Int", mode,
            "Ptr", 0,
            "UPtr", 0,
            "UPtr*", &required,
            "Int"
        )
        if status != -1 || required <= 0
            Pillow.CheckStatus(status)

        buf := Buffer(required, 0)
        Pillow.CheckStatus(DllCall(
            Pillow.RequireDllPath() "\pillow_c_mode_name",
            "Int", mode,
            "Ptr", buf,
            "UPtr", buf.Size,
            "UPtr*", &required,
            "Int"
        ))
        return StrGet(buf.Ptr, required - 1, "UTF-8")
    }

    static WrapImageHandle(handle) {
        if handle = 0
            throw Error("pillow_c returned a null image handle", -2)
        return Pillow.Image(handle)
    }

    class Image {
        __New(handle) {
            this.Handle := handle
        }

        __Delete() {
            this.Close()
        }

        static New(modeName, size, color := unset) {
            if size.Length != 2
                throw Error("Pillow.Image.New expects size [width, height]", -1)

            handle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_create_mode",
                "Int", size[1],
                "Int", size[2],
                "Int", Pillow.ModeId(modeName),
                "Ptr*", &handle,
                "Int"
            ))
            image := Pillow.Image(handle)
            if IsSet(color) {
                try {
                    image.Fill(color)
                } catch {
                    image.Close()
                    throw
                }
            }
            return image
        }

        static FromBytes(modeName, size, bytes) {
            image := Pillow.Image.New(modeName, size)
            try {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_set_bytes",
                    "Ptr", image.Handle,
                    "Ptr", bytes,
                    "UPtr", bytes.Size,
                    "Int"
                ))
                return image
            } catch {
                image.Close()
                throw
            }
        }

        static Blend(left, right, alpha) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_blend",
                "Ptr", left.RequireHandle(),
                "Ptr", right.RequireHandle(),
                "Double", alpha,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static AlphaComposite(dst, src) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_alpha_composite_rgba",
                "Ptr", dst.RequireHandle(),
                "Ptr", src.RequireHandle(),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Merge(modeName, bands) {
            if !IsObject(bands)
                throw Error("Pillow.Image.Merge expects an array of band images", -1)
            bandHandles := Pillow.Image.HandleArray(bands)
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_merge_bands",
                "Int", Pillow.ModeId(modeName),
                "Ptr", bandHandles,
                "UPtr", bands.Length,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static HandleArray(images) {
            buf := Buffer(images.Length * A_PtrSize, 0)
            for index, image in images {
                if !(IsObject(image) && image is Pillow.Image)
                    throw Error("Pillow.Image.Merge bands must be Pillow.Image objects", -1)
                NumPut("Ptr", image.RequireHandle(), buf, (index - 1) * A_PtrSize)
            }
            return buf
        }

        Close() {
            if !this.HasOwnProp("Handle") || this.Handle = 0
                return
            handle := this.Handle
            this.Handle := 0
            Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", handle, "Int"))
        }

        RequireHandle() {
            if !this.HasOwnProp("Handle") || this.Handle = 0
                throw Error("Pillow.Image handle is closed", -1)
            return this.Handle
        }

        Mode {
            get {
                mode := 0
                Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_mode", "Ptr", this.RequireHandle(), "Int*", &mode, "Int"))
                return Pillow.ModeName(mode)
            }
        }

        Width {
            get => this.GetInt("pillow_c_image_width")
        }

        Height {
            get => this.GetInt("pillow_c_image_height")
        }

        Size {
            get => [this.Width, this.Height]
        }

        Channels {
            get => this.GetInt("pillow_c_image_channels")
        }

        Stride {
            get => this.GetInt("pillow_c_image_stride")
        }

        ByteSize {
            get {
                value := 0
                Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_size", "Ptr", this.RequireHandle(), "UPtr*", &value, "Int"))
                return value
            }
        }

        GetInt(exportName) {
            value := 0
            Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\" exportName, "Ptr", this.RequireHandle(), "Int*", &value, "Int"))
            return value
        }

        ToBytes() {
            size := this.ByteSize
            out := Buffer(size, 0)
            if size = 0
                return out
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_get_bytes",
                "Ptr", this.RequireHandle(),
                "Ptr", out,
                "UPtr", out.Size,
                "Int"
            ))
            return out
        }

        DataPointer() {
            dataPtr := 0
            size := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_data",
                "Ptr", this.RequireHandle(),
                "Ptr*", &dataPtr,
                "UPtr*", &size,
                "Int"
            ))
            return { Ptr: dataPtr, Size: size }
        }

        Fill(color) {
            colorBytes := this.ColorBuffer(color)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_fill",
                "Ptr", this.RequireHandle(),
                "Ptr", colorBytes,
                "UPtr", colorBytes.Size,
                "Int"
            ))
            return this
        }

        ColorBuffer(color) {
            channels := this.Channels
            if IsObject(color) {
                if color.Length != channels
                    throw Error("Pillow color length must match image channels", -1)
                buf := Buffer(channels, 0)
                for index, value in color
                    NumPut("UChar", value, buf, index - 1)
                return buf
            }

            if channels != 1
                throw Error("Scalar color is only valid for single-channel images", -1)
            buf := Buffer(1, 0)
            NumPut("UChar", color, buf, 0)
            return buf
        }

        Point(lut) {
            lutBytes := this.LutBuffer(lut)
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_point_lut",
                "Ptr", this.RequireHandle(),
                "Ptr", lutBytes,
                "UPtr", lutBytes.Size,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        LutBuffer(lut) {
            if !IsObject(lut)
                throw Error("Pillow.Image.Point expects an array LUT", -1)
            expected := this.Channels * 256
            if lut.Length != expected
                throw Error("Pillow.Image.Point LUT length must be " expected, -1)
            buf := Buffer(expected, 0)
            for index, value in lut
                NumPut("UChar", value, buf, index - 1)
            return buf
        }

        GetChannel(channel) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_get_channel",
                "Ptr", this.RequireHandle(),
                "Int", this.ChannelIndex(channel),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        Histogram() {
            count := this.Channels * 256
            out := Buffer(count * 8, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_histogram",
                "Ptr", this.RequireHandle(),
                "Ptr", out,
                "UPtr", count,
                "Int"
            ))
            values := []
            loop count
                values.Push(NumGet(out, (A_Index - 1) * 8, "Int64"))
            return values
        }

        GetExtrema() {
            bandCount := this.Channels
            minBuf := Buffer(bandCount, 0)
            maxBuf := Buffer(bandCount, 0)
            hasBuf := Buffer(bandCount, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_get_extrema",
                "Ptr", this.RequireHandle(),
                "Ptr", minBuf,
                "Ptr", maxBuf,
                "Ptr", hasBuf,
                "UPtr", bandCount,
                "Int"
            ))

            extrema := []
            loop bandCount {
                if NumGet(hasBuf, A_Index - 1, "UChar")
                    extrema.Push([NumGet(minBuf, A_Index - 1, "UChar"), NumGet(maxBuf, A_Index - 1, "UChar")])
                else
                    extrema.Push(0)
            }
            return bandCount = 1 ? extrema[1] : extrema
        }

        Split() {
            bandCount := this.Channels
            outHandles := Buffer(bandCount * A_PtrSize, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_split_bands",
                "Ptr", this.RequireHandle(),
                "Ptr", outHandles,
                "UPtr", bandCount,
                "Int"
            ))
            bands := []
            loop bandCount
                bands.Push(Pillow.WrapImageHandle(NumGet(outHandles, (A_Index - 1) * A_PtrSize, "Ptr")))
            return bands
        }

        ChannelIndex(channel) {
            if !IsObject(channel) && channel is Integer {
                if channel < 0 || channel >= this.Channels
                    throw Error("band index out of range", -1)
                return channel
            }

            name := channel ""
            mode := this.Mode
            names := mode = "L" ? ["L"] : mode = "RGB" ? ["R", "G", "B"] : mode = "RGBA" ? ["R", "G", "B", "A"] : []
            for index, item in names {
                if item = name
                    return index - 1
            }
            throw Error('The image has no channel "' name '"', -1)
        }

        PutAlpha(alpha) {
            outHandle := 0
            if IsObject(alpha) && alpha is Pillow.Image {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_put_alpha_image",
                    "Ptr", this.RequireHandle(),
                    "Ptr", alpha.RequireHandle(),
                    "Ptr*", &outHandle,
                    "Int"
                ))
            } else {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_put_alpha_value",
                    "Ptr", this.RequireHandle(),
                    "UChar", alpha,
                    "Ptr*", &outHandle,
                    "Int"
                ))
            }
            return Pillow.WrapImageHandle(outHandle)
        }

        Copy() {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_copy",
                "Ptr", this.RequireHandle(),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        Crop(box) {
            if box.Length != 4
                throw Error("Pillow.Image.Crop expects box [left, top, right, bottom]", -1)

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_crop",
                "Ptr", this.RequireHandle(),
                "Int", box[1],
                "Int", box[2],
                "Int", box[3],
                "Int", box[4],
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        Resize(size, resample := unset) {
            if size.Length != 2
                throw Error("Pillow.Image.Resize expects size [width, height]", -1)
            if !IsSet(resample)
                resample := Pillow.Resampling.NEAREST

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_resize",
                "Ptr", this.RequireHandle(),
                "Int", size[1],
                "Int", size[2],
                "Int", resample,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        Paste(source, box) {
            if box.Length < 2
                throw Error("Pillow.Image.Paste expects box [left, top]", -1)

            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_paste",
                "Ptr", this.RequireHandle(),
                "Ptr", source.RequireHandle(),
                "Int", box[1],
                "Int", box[2],
                "Int"
            ))
            return this
        }

        Transpose(method) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_transpose",
                "Ptr", this.RequireHandle(),
                "Int", method,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        Convert(modeName) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_convert_mode",
                "Ptr", this.RequireHandle(),
                "Int", Pillow.ModeId(modeName),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }
    }
}
