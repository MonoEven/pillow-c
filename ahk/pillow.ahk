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

    class Transform {
        static AFFINE := 0
        static EXTENT := 1
        static PERSPECTIVE := 2
        static QUAD := 3
        static MESH := 4
    }

    class Resampling {
        static NEAREST := 0
        static BOX := 4
        static BILINEAR := 2
        static HAMMING := 5
        static BICUBIC := 3
        static LANCZOS := 1
    }

    class Dither {
        static NONE := 0
        static FLOYDSTEINBERG := 3
    }

    class ImageOps {
        static Invert(image) {
            return Pillow.ImageOps.NativeUnaryImageOp(image, "pillow_c_image_invert")
        }

        static Grayscale(image) {
            Pillow.ImageOps.RequireImageHandle(image, "Grayscale")
            return image.Convert("L")
        }

        static Mirror(image) {
            Pillow.ImageOps.RequireImageHandle(image, "Mirror")
            return image.Transpose(Pillow.Transpose.FLIP_LEFT_RIGHT)
        }

        static Flip(image) {
            Pillow.ImageOps.RequireImageHandle(image, "Flip")
            return image.Transpose(Pillow.Transpose.FLIP_TOP_BOTTOM)
        }

        static Deform(image, deformer, resample := unset) {
            Pillow.ImageOps.RequireImageHandle(image, "Deform")
            if !IsObject(deformer) || !HasMethod(deformer, "getmesh")
                throw Error("Pillow.ImageOps.Deform expects a deformer with getmesh(image)", -1)
            return image.Transform(
                image.Size,
                Pillow.Transform.MESH,
                deformer.getmesh(image),
                IsSet(resample) ? resample : Pillow.Resampling.BILINEAR)
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

        static Colorize(image, black, white, mid := unset, blackpoint := 0, whitepoint := 255, midpoint := 127) {
            Pillow.ImageOps.RequireImageHandle(image, "Colorize")
            if image.Mode != "L"
                throw Error("Pillow.ImageOps.Colorize expects an L image", -1)
            blackColor := Pillow.ImageOps.RgbColorBuffer(black, "Colorize")
            whiteColor := Pillow.ImageOps.RgbColorBuffer(white, "Colorize")
            hasMid := IsSet(mid)
            midColor := hasMid ? Pillow.ImageOps.RgbColorBuffer(mid, "Colorize") : 0
            if !(blackpoint is Integer) || !(whitepoint is Integer) || !(midpoint is Integer)
                throw Error("Pillow.ImageOps.Colorize points must be integers", -1)
            if hasMid {
                if !(0 <= blackpoint && blackpoint <= midpoint && midpoint <= whitepoint && whitepoint <= 255)
                    throw Error("Pillow.ImageOps.Colorize points must satisfy blackpoint <= midpoint <= whitepoint", -1)
            } else if !(0 <= blackpoint && blackpoint <= whitepoint && whitepoint <= 255) {
                throw Error("Pillow.ImageOps.Colorize points must satisfy blackpoint <= whitepoint", -1)
            }

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_colorize",
                "Ptr", image.RequireHandle(),
                "Ptr", blackColor,
                "Ptr", whiteColor,
                "Int", hasMid,
                "Ptr", hasMid ? midColor.Ptr : 0,
                "Int", blackpoint,
                "Int", whitepoint,
                "Int", midpoint,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Equalize(image, mask := unset) {
            if IsSet(mask) {
                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_equalize_masked",
                    "Ptr", Pillow.ImageOps.RequireImageHandle(image, "Equalize"),
                    "Ptr", Pillow.ImageOps.RequireImageHandle(mask, "Equalize mask"),
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return Pillow.WrapImageHandle(outHandle)
            }
            return Pillow.ImageOps.NativeUnaryImageOp(image, "pillow_c_image_equalize")
        }

        static Crop(image, border := 0) {
            Pillow.ImageOps.RequireImageHandle(image, "Crop")
            borders := Pillow.ImageOps.BorderBox(border, "Crop")
            return image.Crop([
                borders[1],
                borders[2],
                image.Width - borders[3],
                image.Height - borders[4],
            ])
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

        static Scale(image, factor, resample := unset) {
            Pillow.ImageOps.RequireImageHandle(image, "Scale")
            if !(factor is Number)
                throw Error("Pillow.ImageOps.Scale factor must be numeric", -1)
            if factor = 1
                return image.Copy()
            if factor <= 0
                throw Error("Pillow.ImageOps.Scale factor must be greater than 0", -1)

            return image.Resize([
                Pillow.Image.RoundHalfEven(factor * image.Width),
                Pillow.Image.RoundHalfEven(factor * image.Height),
            ], IsSet(resample) ? resample : Pillow.Resampling.BICUBIC)
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

        static Autocontrast(image, cutoff := 0, ignore := unset, mask := unset, preserveTone := false) {
            cuts := Pillow.ImageOps.CutoffPair(cutoff)
            ignorePtr := 0
            ignoreCount := 0
            ignoreBuffer := 0
            if IsSet(ignore) {
                ignoreBuffer := Pillow.ImageOps.IgnoreBuffer(ignore)
                ignorePtr := ignoreBuffer.Ptr
                ignoreCount := ignoreBuffer.Size
            }
            maskHandle := 0
            if IsSet(mask)
                maskHandle := Pillow.ImageOps.RequireImageHandle(mask, "Autocontrast mask")

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_autocontrast",
                "Ptr", Pillow.ImageOps.RequireImageHandle(image, "Autocontrast"),
                "Double", cuts[1],
                "Double", cuts[2],
                "Ptr", ignorePtr,
                "UPtr", ignoreCount,
                "Ptr", maskHandle,
                "Int", preserveTone,
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

        static BorderBox(border, operationName := "Expand") {
            if IsObject(border) {
                if border.Length = 2
                    return [border[1], border[2], border[1], border[2]]
                if border.Length = 4
                    return [border[1], border[2], border[3], border[4]]
                throw Error("Pillow.ImageOps." operationName " border expects a number, [x, y], or [left, top, right, bottom]", -1)
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
                    NumPut("UChar", image.ModeAwareU8(fill[1]), buf, 0)
                    return buf
                }
                if channels = 2 {
                    if fill.Length != 1 && fill.Length != 2
                        throw Error("Pillow.ImageOps." operationName " fill must match image mode", -1)
                    NumPut("UChar", fill[1], buf, 0)
                    NumPut("UChar", fill.Length = 2 ? fill[2] : 0, buf, 1)
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

            NumPut("UChar", image.ModeAwareU8(fill), buf, 0)
            return buf
        }

        static RgbColorBuffer(color, operationName) {
            if !IsObject(color) || color.Length != 3
                throw Error("Pillow.ImageOps." operationName " color expects [r, g, b]", -1)
            buf := Buffer(3, 0)
            loop 3
                NumPut("UChar", color[A_Index], buf, A_Index - 1)
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
        static Blend(left, right, alpha) {
            return Pillow.Image.Blend(left, right, alpha)
        }

        static Composite(image1, image2, mask) {
            return Pillow.Image.Composite(image1, image2, mask)
        }

        static Constant(image, value) {
            if !(value is Integer)
                throw Error("Pillow.ImageChops.Constant value must be an integer", -1)

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_constant",
                "Ptr", Pillow.ImageChops.RequireImageHandle(image, "Constant"),
                "Int", value,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Duplicate(image) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_copy",
                "Ptr", Pillow.ImageChops.RequireImageHandle(image, "Duplicate"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Invert(image) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_chops_invert",
                "Ptr", Pillow.ImageChops.RequireImageHandle(image, "Invert"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

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

        static Screen(left, right) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_screen",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "Screen"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "Screen"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Lighter(left, right) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_lighter",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "Lighter"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "Lighter"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Darker(left, right) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_darker",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "Darker"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "Darker"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static SoftLight(left, right) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_soft_light",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "SoftLight"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "SoftLight"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static HardLight(left, right) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_hard_light",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "HardLight"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "HardLight"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Overlay(left, right) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_overlay",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "Overlay"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "Overlay"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static LogicalAnd(left, right) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_logical_and",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "LogicalAnd"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "LogicalAnd"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static LogicalOr(left, right) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_logical_or",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "LogicalOr"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "LogicalOr"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static LogicalXor(left, right) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_logical_xor",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "LogicalXor"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "LogicalXor"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Offset(image, xoffset, yoffset := unset) {
            if !(xoffset is Integer)
                throw Error("Pillow.ImageChops.Offset xoffset must be an integer", -1)
            if IsSet(yoffset) {
                if !(yoffset is Integer)
                    throw Error("Pillow.ImageChops.Offset yoffset must be an integer", -1)
            } else {
                yoffset := xoffset
            }

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_offset",
                "Ptr", Pillow.ImageChops.RequireImageHandle(image, "Offset"),
                "Int", xoffset,
                "Int", yoffset,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Add(left, right, scale := 1.0, offset := 0) {
            if !(scale is Number)
                throw Error("Pillow.ImageChops.Add scale must be numeric", -1)
            if !(offset is Number)
                throw Error("Pillow.ImageChops.Add offset must be numeric", -1)

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_add",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "Add"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "Add"),
                "Double", scale,
                "Double", offset,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static Subtract(left, right, scale := 1.0, offset := 0) {
            if !(scale is Number)
                throw Error("Pillow.ImageChops.Subtract scale must be numeric", -1)
            if !(offset is Number)
                throw Error("Pillow.ImageChops.Subtract offset must be numeric", -1)

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_subtract",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "Subtract"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "Subtract"),
                "Double", scale,
                "Double", offset,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static AddModulo(left, right) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_add_modulo",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "AddModulo"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "AddModulo"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static SubtractModulo(left, right) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_subtract_modulo",
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, "SubtractModulo"),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, "SubtractModulo"),
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

    class ImageFilter {
        class Kernel {
            __New(size, kernel, scale := unset, offset := 0) {
                if !IsObject(size) || size.Length != 2
                    throw Error("Pillow.ImageFilter.Kernel expects size [width, height]", -1)
                if !(size[1] is Integer) || !(size[2] is Integer)
                    throw Error("Pillow.ImageFilter.Kernel size values must be integers", -1)
                if !IsObject(kernel)
                    throw Error("Pillow.ImageFilter.Kernel expects an array of coefficients", -1)
                expected := size[1] * size[2]
                if kernel.Length != expected
                    throw Error("not enough coefficients in kernel", -1)
                if !(offset is Number)
                    throw Error("Pillow.ImageFilter.Kernel offset must be numeric", -1)

                this.Size := [size[1], size[2]]
                this.Kernel := kernel.Clone()
                if IsSet(scale) {
                    if !(scale is Number)
                        throw Error("Pillow.ImageFilter.Kernel scale must be numeric", -1)
                    this.Scale := scale
                } else {
                    this.Scale := Pillow.ImageFilter.SumCoefficients(kernel)
                }
                this.Offset := offset
            }

            Apply(image) {
                if !(IsObject(image) && image is Pillow.Image)
                    throw Error("Pillow.ImageFilter.Kernel expects a Pillow.Image", -1)
                if !((this.Size[1] = 3 && this.Size[2] = 3) || (this.Size[1] = 5 && this.Size[2] = 5))
                    throw Error("bad kernel size", -1)

                kernelBuffer := Buffer(this.Kernel.Length * 8, 0)
                for index, value in this.Kernel {
                    if !(value is Number)
                        throw Error("Pillow.ImageFilter.Kernel coefficients must be numeric", -1)
                    NumPut("Double", value, kernelBuffer, (index - 1) * 8)
                }

                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_filter_kernel",
                    "Ptr", image.RequireHandle(),
                    "Int", this.Size[1],
                    "Int", this.Size[2],
                    "Ptr", kernelBuffer,
                    "UPtr", this.Kernel.Length,
                    "Double", this.Scale,
                    "Double", this.Offset,
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return Pillow.WrapImageHandle(outHandle)
            }
        }

        class BuiltinKernel extends Pillow.ImageFilter.Kernel {
            __New(name, size, scale, offset, kernel) {
                this.Name := name
                super.__New(size, kernel, scale, offset)
            }
        }

        class RankFilter {
            __New(size, rank, name := "Rank") {
                if !(size is Integer)
                    throw Error("Pillow.ImageFilter.RankFilter size must be an integer", -1)
                if !(rank is Integer)
                    throw Error("Pillow.ImageFilter.RankFilter rank must be an integer", -1)
                this.Name := name
                this.Size := size
                this.Rank := rank
            }

            Apply(image) {
                if !(IsObject(image) && image is Pillow.Image)
                    throw Error("Pillow.ImageFilter.RankFilter expects a Pillow.Image", -1)
                if this.Size <= 0 || Mod(this.Size, 2) = 0
                    throw Error("bad filter size", -1)
                maxRank := this.Size * this.Size - 1
                if this.Rank < 0 || this.Rank > maxRank
                    throw Error("bad rank value", -1)

                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_filter_rank",
                    "Ptr", image.RequireHandle(),
                    "Int", this.Size,
                    "Int", this.Rank,
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return Pillow.WrapImageHandle(outHandle)
            }
        }

        static MinFilter(size := 3) {
            return Pillow.ImageFilter.RankFilter(size, 0, "Min")
        }

        static MedianFilter(size := 3) {
            return Pillow.ImageFilter.RankFilter(size, size * size // 2, "Median")
        }

        static MaxFilter(size := 3) {
            return Pillow.ImageFilter.RankFilter(size, size * size - 1, "Max")
        }

        class ModeFilter {
            __New(size := 3) {
                if !(size is Integer)
                    throw Error("Pillow.ImageFilter.ModeFilter size must be an integer", -1)
                this.Name := "Mode"
                this.Size := size
            }

            Apply(image) {
                if !(IsObject(image) && image is Pillow.Image)
                    throw Error("Pillow.ImageFilter.ModeFilter expects a Pillow.Image", -1)

                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_filter_mode",
                    "Ptr", image.RequireHandle(),
                    "Int", this.Size,
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return Pillow.WrapImageHandle(outHandle)
            }
        }

        class BoxBlur {
            __New(radius) {
                xy := Pillow.ImageFilter.RadiusPair(radius, "BoxBlur")
                if xy[1] < 0 || xy[2] < 0
                    throw Error("radius must be >= 0", -1)
                this.Name := "BoxBlur"
                this.Radius := radius
                this.XRadius := xy[1]
                this.YRadius := xy[2]
            }

            Apply(image) {
                if !(IsObject(image) && image is Pillow.Image)
                    throw Error("Pillow.ImageFilter.BoxBlur expects a Pillow.Image", -1)

                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_filter_box_blur",
                    "Ptr", image.RequireHandle(),
                    "Double", this.XRadius,
                    "Double", this.YRadius,
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return Pillow.WrapImageHandle(outHandle)
            }
        }

        class GaussianBlur {
            __New(radius := 2) {
                xy := Pillow.ImageFilter.RadiusPair(radius, "GaussianBlur")
                this.Name := "GaussianBlur"
                this.Radius := radius
                this.XRadius := xy[1]
                this.YRadius := xy[2]
            }

            Apply(image) {
                if !(IsObject(image) && image is Pillow.Image)
                    throw Error("Pillow.ImageFilter.GaussianBlur expects a Pillow.Image", -1)

                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_filter_gaussian_blur",
                    "Ptr", image.RequireHandle(),
                    "Double", this.XRadius,
                    "Double", this.YRadius,
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return Pillow.WrapImageHandle(outHandle)
            }
        }

        class UnsharpMask {
            __New(radius := 2, percent := 150, threshold := 3) {
                if !(radius is Number)
                    throw Error("Pillow.ImageFilter.UnsharpMask radius must be numeric", -1)
                if !(percent is Integer)
                    throw Error("Pillow.ImageFilter.UnsharpMask percent must be an integer", -1)
                if !(threshold is Integer)
                    throw Error("Pillow.ImageFilter.UnsharpMask threshold must be an integer", -1)
                this.Name := "UnsharpMask"
                this.Radius := radius
                this.Percent := percent
                this.Threshold := threshold
            }

            Apply(image) {
                if !(IsObject(image) && image is Pillow.Image)
                    throw Error("Pillow.ImageFilter.UnsharpMask expects a Pillow.Image", -1)

                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_filter_unsharp_mask",
                    "Ptr", image.RequireHandle(),
                    "Double", this.Radius,
                    "Int", this.Percent,
                    "Int", this.Threshold,
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return Pillow.WrapImageHandle(outHandle)
            }
        }

        class Color3DLUT {
            __New(size, table, channels := 3, targetMode := "") {
                if !(channels is Integer) || !(channels = 3 || channels = 4)
                    throw Error("Pillow.ImageFilter.Color3DLUT supports only 3 or 4 output channels", -1)
                this.Name := "Color 3D LUT"
                this.Size := Pillow.ImageFilter.Color3DLUT.CheckSize(size)
                this.Channels := channels
                this.TargetMode := targetMode
                this.Table := Pillow.ImageFilter.Color3DLUT.FlattenTable(table, channels)
                expected := channels * this.Size[1] * this.Size[2] * this.Size[3]
                if this.Table.Length != expected
                    throw Error("Pillow.ImageFilter.Color3DLUT table has wrong length", -1)
            }

            Apply(image) {
                if !(IsObject(image) && image is Pillow.Image)
                    throw Error("Pillow.ImageFilter.Color3DLUT expects a Pillow.Image", -1)

                targetMode := this.TargetMode = "" ? image.Mode : this.TargetMode
                tableBuffer := Buffer(this.Table.Length * 8, 0)
                for index, value in this.Table {
                    if !(value is Number)
                        throw Error("Pillow.ImageFilter.Color3DLUT table values must be numeric", -1)
                    NumPut("Double", value, tableBuffer, (index - 1) * 8)
                }

                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_filter_color_3d_lut",
                    "Ptr", image.RequireHandle(),
                    "Int", Pillow.ModeId(targetMode),
                    "Int", this.Channels,
                    "Int", this.Size[1],
                    "Int", this.Size[2],
                    "Int", this.Size[3],
                    "Ptr", tableBuffer,
                    "UPtr", this.Table.Length,
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return Pillow.WrapImageHandle(outHandle)
            }

            static CheckSize(size) {
                if IsObject(size) {
                    if size.Length != 3
                        throw Error("Pillow.ImageFilter.Color3DLUT Size should be an integer or [x, y, z]", -1)
                    checked := [size[1], size[2], size[3]]
                } else {
                    checked := [size, size, size]
                }
                for value in checked {
                    if !(value is Integer)
                        throw Error("Pillow.ImageFilter.Color3DLUT size values must be integers", -1)
                    if value < 2 || value > 65
                        throw Error("Pillow.ImageFilter.Color3DLUT size should be in [2, 65] range", -1)
                }
                return checked
            }

            static FlattenTable(table, channels) {
                if !IsObject(table)
                    throw Error("Pillow.ImageFilter.Color3DLUT expects a table array", -1)
                flat := []
                if table.Length > 0 && IsObject(table[1]) {
                    for pixel in table {
                        if !IsObject(pixel) || pixel.Length != channels
                            throw Error("Pillow.ImageFilter.Color3DLUT table tuple length must match channels", -1)
                        for value in pixel
                            flat.Push(value)
                    }
                    return flat
                }
                return table.Clone()
            }
        }

        static BLUR() {
            return Pillow.ImageFilter.BuiltinKernel("Blur", [5, 5], 16, 0, [
                1, 1, 1, 1, 1,
                1, 0, 0, 0, 1,
                1, 0, 0, 0, 1,
                1, 0, 0, 0, 1,
                1, 1, 1, 1, 1,
            ])
        }

        static CONTOUR() {
            return Pillow.ImageFilter.BuiltinKernel("Contour", [3, 3], 1, 255, [
                -1, -1, -1,
                -1, 8, -1,
                -1, -1, -1,
            ])
        }

        static DETAIL() {
            return Pillow.ImageFilter.BuiltinKernel("Detail", [3, 3], 6, 0, [
                0, -1, 0,
                -1, 10, -1,
                0, -1, 0,
            ])
        }

        static EDGE_ENHANCE() {
            return Pillow.ImageFilter.BuiltinKernel("Edge-enhance", [3, 3], 2, 0, [
                -1, -1, -1,
                -1, 10, -1,
                -1, -1, -1,
            ])
        }

        static EDGE_ENHANCE_MORE() {
            return Pillow.ImageFilter.BuiltinKernel("Edge-enhance More", [3, 3], 1, 0, [
                -1, -1, -1,
                -1, 9, -1,
                -1, -1, -1,
            ])
        }

        static EMBOSS() {
            return Pillow.ImageFilter.BuiltinKernel("Emboss", [3, 3], 1, 128, [
                -1, 0, 0,
                0, 1, 0,
                0, 0, 0,
            ])
        }

        static FIND_EDGES() {
            return Pillow.ImageFilter.BuiltinKernel("Find Edges", [3, 3], 1, 0, [
                -1, -1, -1,
                -1, 8, -1,
                -1, -1, -1,
            ])
        }

        static SHARPEN() {
            return Pillow.ImageFilter.BuiltinKernel("Sharpen", [3, 3], 16, 0, [
                -2, -2, -2,
                -2, 32, -2,
                -2, -2, -2,
            ])
        }

        static SMOOTH() {
            return Pillow.ImageFilter.BuiltinKernel("Smooth", [3, 3], 13, 0, [
                1, 1, 1,
                1, 5, 1,
                1, 1, 1,
            ])
        }

        static SMOOTH_MORE() {
            return Pillow.ImageFilter.BuiltinKernel("Smooth More", [5, 5], 100, 0, [
                1, 1, 1, 1, 1,
                1, 5, 5, 5, 1,
                1, 5, 44, 5, 1,
                1, 5, 5, 5, 1,
                1, 1, 1, 1, 1,
            ])
        }

        static SumCoefficients(values) {
            total := 0.0
            for value in values {
                if !(value is Number)
                    throw Error("Pillow.ImageFilter.Kernel coefficients must be numeric", -1)
                total += value
            }
            return total
        }

        static RadiusPair(radius, operationName) {
            if IsObject(radius) {
                if radius.Length != 2
                    throw Error("Pillow.ImageFilter." operationName " radius expects a number or [x, y]", -1)
                if !(radius[1] is Number) || !(radius[2] is Number)
                    throw Error("Pillow.ImageFilter." operationName " radius values must be numeric", -1)
                return [radius[1], radius[2]]
            }
            if !(radius is Number)
                throw Error("Pillow.ImageFilter." operationName " radius must be numeric", -1)
            return [radius, radius]
        }
    }

    class ImageEnhance {
        class _Enhance {
            Enhance(factor) {
                if !(factor is Number)
                    throw Error("Pillow.ImageEnhance factor must be numeric", -1)
                return Pillow.Image.Blend(this.Degenerate, this.Image, factor)
            }
        }

        class Color extends Pillow.ImageEnhance._Enhance {
            __New(image) {
                Pillow.ImageEnhance.RequireImage(image, "Color")
                this.Image := image
                if image.Mode = "L" {
                    this.Degenerate := image.Copy()
                } else if image.Mode = "LA" {
                    this.Degenerate := image.Copy()
                } else if image.Mode = "RGB" {
                    this.Degenerate := image.Convert("L").Convert("RGB")
                } else if image.Mode = "RGBA" {
                    this.Degenerate := image.Convert("LA").Convert("RGBA")
                } else if image.Mode = "CMYK" {
                    this.Degenerate := image.Convert("L").Convert("CMYK")
                } else {
                    throw Error("Pillow.ImageEnhance.Color currently supports L LA RGB RGBA and CMYK", -1)
                }
            }
        }

        class Contrast extends Pillow.ImageEnhance._Enhance {
            __New(image) {
                Pillow.ImageEnhance.RequireImage(image, "Contrast")
                this.Image := image
                gray := image.Mode = "L" ? image.Copy() : image.Convert("L")
                try {
                    mean := Pillow.ImageEnhance.GrayscaleMeanRounded(gray)
                    degenerate := Pillow.Image.New("L", image.Size, mean)
                    if image.Mode != "L" {
                        converted := degenerate.Convert(image.Mode)
                        degenerate.Close()
                        degenerate := converted
                    }
                    if image.Mode = "RGBA" {
                        alpha := image.GetChannel("A")
                        try {
                            withAlpha := degenerate.PutAlpha(alpha)
                            degenerate.Close()
                            degenerate := withAlpha
                        } finally {
                            alpha.Close()
                        }
                    }
                    this.Degenerate := degenerate
                } finally {
                    gray.Close()
                }
            }
        }

        class Brightness extends Pillow.ImageEnhance._Enhance {
            __New(image) {
                Pillow.ImageEnhance.RequireImage(image, "Brightness")
                this.Image := image
                if image.Mode = "RGBA" {
                    base := Pillow.Image.New("RGB", image.Size, [0, 0, 0])
                    try {
                        alpha := image.GetChannel("A")
                        try {
                            this.Degenerate := base.PutAlpha(alpha)
                        } finally {
                            alpha.Close()
                        }
                    } finally {
                        base.Close()
                    }
                } else {
                    this.Degenerate := Pillow.Image.New(image.Mode, image.Size, Pillow.ImageEnhance.ZeroColor(image))
                }
            }
        }

        class Sharpness extends Pillow.ImageEnhance._Enhance {
            __New(image) {
                Pillow.ImageEnhance.RequireImage(image, "Sharpness")
                this.Image := image
                degenerate := image.Filter(Pillow.ImageFilter.SMOOTH())
                if image.Mode = "RGBA" {
                    alpha := image.GetChannel("A")
                    try {
                        withAlpha := degenerate.PutAlpha(alpha)
                        degenerate.Close()
                        degenerate := withAlpha
                    } finally {
                        alpha.Close()
                    }
                }
                this.Degenerate := degenerate
            }
        }

        static RequireImage(image, operationName) {
            if !(IsObject(image) && image is Pillow.Image)
                throw Error("Pillow.ImageEnhance." operationName " expects a Pillow.Image", -1)
        }

        static GrayscaleMeanRounded(image) {
            histogram := image.Histogram()
            total := 0
            weighted := 0
            for index, count in histogram {
                value := index - 1
                total += count
                weighted += value * count
            }
            if total = 0
                return 0
            return Floor(weighted / total + 0.5)
        }

        static ZeroColor(image) {
            if image.Channels = 1
                return 0
            values := []
            loop image.Channels
                values.Push(0)
            return values
        }
    }

    class ImageDraw {
        static Draw(image) {
            return Pillow.ImageDraw.DrawHandle(image)
        }

        static Floodfill(image, xy, value, border := unset, thresh := 0.0) {
            if !(IsObject(image) && image is Pillow.Image)
                throw Error("Pillow.ImageDraw.Floodfill image expects a Pillow.Image", -1)
            if !IsObject(xy) || xy.Length != 2
                throw Error("Pillow.ImageDraw.Floodfill xy expects [x, y]", -1)
            if !(xy[1] is Number) || !(xy[2] is Number)
                throw Error("Pillow.ImageDraw.Floodfill xy coordinates must be numeric", -1)
            if !(thresh is Number)
                throw Error("Pillow.ImageDraw.Floodfill thresh must be numeric", -1)

            valueBuffer := image.PasteColorBuffer(value)
            borderPtr := 0
            borderSize := 0
            borderBuffer := 0
            if IsSet(border) {
                borderBuffer := image.PasteColorBuffer(border)
                borderPtr := borderBuffer.Ptr
                borderSize := borderBuffer.Size
            }

            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_draw_floodfill",
                "Ptr", image.RequireHandle(),
                "Int", xy[1],
                "Int", xy[2],
                "Ptr", valueBuffer,
                "UPtr", valueBuffer.Size,
                "Ptr", borderPtr,
                "UPtr", borderSize,
                "Double", thresh,
                "Int"
            ))
            return image
        }

        class DrawHandle {
            __New(image) {
                if !(IsObject(image) && image is Pillow.Image)
                    throw Error("Pillow.ImageDraw.Draw expects a Pillow.Image", -1)
                this.Image := image
            }

            Rectangle(box, fill := unset, outline := unset, width := 1) {
                if !IsObject(box) || box.Length != 4
                    throw Error("Pillow.ImageDraw.Rectangle expects box [left, top, right, bottom]", -1)

                fillPtr := 0
                fillSize := 0
                fillBuffer := 0
                if IsSet(fill) {
                    fillBuffer := this.Image.PasteColorBuffer(fill)
                    fillPtr := fillBuffer.Ptr
                    fillSize := fillBuffer.Size
                }

                outlinePtr := 0
                outlineSize := 0
                outlineBuffer := 0
                if IsSet(outline) {
                    outlineBuffer := this.Image.PasteColorBuffer(outline)
                    outlinePtr := outlineBuffer.Ptr
                    outlineSize := outlineBuffer.Size
                }

                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_draw_rectangle",
                    "Ptr", this.Image.RequireHandle(),
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
                ))
                return this
            }

            Ellipse(box, fill := unset, outline := unset, width := 1) {
                if !IsObject(box) || box.Length != 4
                    throw Error("Pillow.ImageDraw.Ellipse expects box [left, top, right, bottom]", -1)

                fillPtr := 0
                fillSize := 0
                fillBuffer := 0
                if IsSet(fill) {
                    fillBuffer := this.Image.PasteColorBuffer(fill)
                    fillPtr := fillBuffer.Ptr
                    fillSize := fillBuffer.Size
                }

                outlinePtr := 0
                outlineSize := 0
                outlineBuffer := 0
                if IsSet(outline) {
                    outlineBuffer := this.Image.PasteColorBuffer(outline)
                    outlinePtr := outlineBuffer.Ptr
                    outlineSize := outlineBuffer.Size
                }

                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_draw_ellipse",
                    "Ptr", this.Image.RequireHandle(),
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
                ))
                return this
            }

            Circle(xy, radius, fill := unset, outline := unset, width := 1) {
                if !IsObject(xy) || xy.Length != 2
                    throw Error("Pillow.ImageDraw.Circle center expects [x, y]", -1)
                if !(xy[1] is Number) || !(xy[2] is Number)
                    throw Error("Pillow.ImageDraw.Circle center coordinates must be numeric", -1)
                if !(radius is Number)
                    throw Error("Pillow.ImageDraw.Circle radius must be numeric", -1)

                return this.Ellipse([
                    xy[1] - radius,
                    xy[2] - radius,
                    xy[1] + radius,
                    xy[2] + radius,
                ], IsSet(fill) ? fill : unset, IsSet(outline) ? outline : unset, width)
            }

            Arc(box, start, end, fill := unset, width := 1) {
                if !IsObject(box) || box.Length != 4
                    throw Error("Pillow.ImageDraw.Arc expects box [left, top, right, bottom]", -1)
                if !(start is Number) || !(end is Number)
                    throw Error("Pillow.ImageDraw.Arc angles must be numeric", -1)

                color := this.Image.PasteColorBuffer(IsSet(fill) ? fill : 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_draw_arc",
                    "Ptr", this.Image.RequireHandle(),
                    "Int", box[1],
                    "Int", box[2],
                    "Int", box[3],
                    "Int", box[4],
                    "Double", start,
                    "Double", end,
                    "Ptr", color,
                    "UPtr", color.Size,
                    "Int", width,
                    "Int"
                ))
                return this
            }

            Chord(box, start, end, fill := unset, outline := unset, width := 1) {
                if !IsObject(box) || box.Length != 4
                    throw Error("Pillow.ImageDraw.Chord expects box [left, top, right, bottom]", -1)
                if !(start is Number) || !(end is Number)
                    throw Error("Pillow.ImageDraw.Chord angles must be numeric", -1)

                fillPtr := 0
                fillSize := 0
                fillBuffer := 0
                if IsSet(fill) {
                    fillBuffer := this.Image.PasteColorBuffer(fill)
                    fillPtr := fillBuffer.Ptr
                    fillSize := fillBuffer.Size
                }

                outlinePtr := 0
                outlineSize := 0
                outlineBuffer := 0
                if IsSet(outline) {
                    outlineBuffer := this.Image.PasteColorBuffer(outline)
                    outlinePtr := outlineBuffer.Ptr
                    outlineSize := outlineBuffer.Size
                }

                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_draw_chord",
                    "Ptr", this.Image.RequireHandle(),
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
                ))
                return this
            }

            Pieslice(box, start, end, fill := unset, outline := unset, width := 1) {
                if !IsObject(box) || box.Length != 4
                    throw Error("Pillow.ImageDraw.Pieslice expects box [left, top, right, bottom]", -1)
                if !(start is Number) || !(end is Number)
                    throw Error("Pillow.ImageDraw.Pieslice angles must be numeric", -1)

                fillPtr := 0
                fillSize := 0
                fillBuffer := 0
                if IsSet(fill) {
                    fillBuffer := this.Image.PasteColorBuffer(fill)
                    fillPtr := fillBuffer.Ptr
                    fillSize := fillBuffer.Size
                }

                outlinePtr := 0
                outlineSize := 0
                outlineBuffer := 0
                if IsSet(outline) {
                    outlineBuffer := this.Image.PasteColorBuffer(outline)
                    outlinePtr := outlineBuffer.Ptr
                    outlineSize := outlineBuffer.Size
                }

                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_draw_pieslice",
                    "Ptr", this.Image.RequireHandle(),
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
                ))
                return this
            }

            RoundedRectangle(box, radius := 0, fill := unset, outline := unset, width := 1, corners := unset) {
                if !IsObject(box) || box.Length != 4
                    throw Error("Pillow.ImageDraw.RoundedRectangle expects box [left, top, right, bottom]", -1)
                if !(radius is Number)
                    throw Error("Pillow.ImageDraw.RoundedRectangle radius must be numeric", -1)

                cornersMask := 15
                if IsSet(corners) {
                    if !IsObject(corners) || corners.Length != 4
                        throw Error("Pillow.ImageDraw.RoundedRectangle corners must be a four-item array", -1)
                    cornersMask := 0
                    loop 4 {
                        if corners[A_Index]
                            cornersMask |= 1 << (A_Index - 1)
                    }
                }

                fillPtr := 0
                fillSize := 0
                fillBuffer := 0
                if IsSet(fill) {
                    fillBuffer := this.Image.PasteColorBuffer(fill)
                    fillPtr := fillBuffer.Ptr
                    fillSize := fillBuffer.Size
                }

                outlinePtr := 0
                outlineSize := 0
                outlineBuffer := 0
                if IsSet(outline) {
                    outlineBuffer := this.Image.PasteColorBuffer(outline)
                    outlinePtr := outlineBuffer.Ptr
                    outlineSize := outlineBuffer.Size
                }

                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_draw_rounded_rectangle",
                    "Ptr", this.Image.RequireHandle(),
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
                ))
                return this
            }

            Bitmap(xy, bitmap, fill := unset) {
                if !IsObject(xy) || xy.Length != 2
                    throw Error("Pillow.ImageDraw.Bitmap xy expects [x, y]", -1)
                if !(xy[1] is Number) || !(xy[2] is Number)
                    throw Error("Pillow.ImageDraw.Bitmap xy coordinates must be numeric", -1)
                if !(IsObject(bitmap) && bitmap is Pillow.Image)
                    throw Error("Pillow.ImageDraw.Bitmap bitmap expects a Pillow.Image", -1)
                if !IsSet(fill)
                    return this

                color := this.Image.PasteColorBuffer(fill)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_draw_bitmap",
                    "Ptr", this.Image.RequireHandle(),
                    "Int", xy[1],
                    "Int", xy[2],
                    "Ptr", bitmap.RequireHandle(),
                    "Ptr", color,
                    "UPtr", color.Size,
                    "Int"
                ))
                return this
            }

            Line(xy, fill := unset, width := 0, joint := unset) {
                points := Pillow.ImageDraw.FlattenPoints(xy, "Line")
                color := this.Image.PasteColorBuffer(IsSet(fill) ? fill : 0)
                jointCurve := IsSet(joint) && joint = "curve"
                if jointCurve {
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_draw_line_joint",
                        "Ptr", this.Image.RequireHandle(),
                        "Ptr", points,
                        "UPtr", points.Size // 8,
                        "Ptr", color,
                        "UPtr", color.Size,
                        "Int", width,
                        "Int", 1,
                        "Int"
                    ))
                } else {
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_draw_line",
                        "Ptr", this.Image.RequireHandle(),
                        "Ptr", points,
                        "UPtr", points.Size // 8,
                        "Ptr", color,
                        "UPtr", color.Size,
                        "Int", width,
                        "Int"
                    ))
                }
                return this
            }

            Point(xy, fill := unset) {
                points := Pillow.ImageDraw.FlattenPoints(xy, "Point", 0)
                color := this.Image.PasteColorBuffer(IsSet(fill) ? fill : 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_draw_points",
                    "Ptr", this.Image.RequireHandle(),
                    "Ptr", points.Size ? points : 0,
                    "UPtr", points.Size // 8,
                    "Ptr", color,
                    "UPtr", color.Size,
                    "Int"
                ))
                return this
            }

            RegularPolygon(boundingCircle, nSides, rotation := 0, fill := unset, outline := unset, width := 1) {
                points := Pillow.ImageDraw.RegularPolygonVertices(boundingCircle, nSides, rotation)
                return this.Polygon(points, IsSet(fill) ? fill : unset, IsSet(outline) ? outline : unset, width)
            }

            Polygon(xy, fill := unset, outline := unset, width := 1) {
                points := Pillow.ImageDraw.FlattenPoints(xy, "Polygon")

                fillPtr := 0
                fillSize := 0
                fillBuffer := 0
                if IsSet(fill) {
                    fillBuffer := this.Image.PasteColorBuffer(fill)
                    fillPtr := fillBuffer.Ptr
                    fillSize := fillBuffer.Size
                }

                outlinePtr := 0
                outlineSize := 0
                outlineBuffer := 0
                if IsSet(outline) {
                    outlineBuffer := this.Image.PasteColorBuffer(outline)
                    outlinePtr := outlineBuffer.Ptr
                    outlineSize := outlineBuffer.Size
                }

                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_draw_polygon",
                    "Ptr", this.Image.RequireHandle(),
                    "Ptr", points,
                    "UPtr", points.Size // 8,
                    "Ptr", fillPtr,
                    "UPtr", fillSize,
                    "Ptr", outlinePtr,
                    "UPtr", outlineSize,
                    "Int", width,
                    "Int"
                ))
                return this
            }
        }

        static RegularPolygonVertices(boundingCircle, nSides, rotation) {
            if !(nSides is Integer)
                throw Error("Pillow.ImageDraw.RegularPolygon n_sides should be an int", -1)
            if nSides < 3
                throw Error("Pillow.ImageDraw.RegularPolygon n_sides should be an int > 2", -1)

            if !IsObject(boundingCircle)
                throw Error("Pillow.ImageDraw.RegularPolygon bounding_circle should be a sequence", -1)

            if boundingCircle.Length = 3 {
                for value in boundingCircle {
                    if !(value is Number)
                        throw Error("Pillow.ImageDraw.RegularPolygon bounding_circle should only contain numeric data", -1)
                }
                centroidX := boundingCircle[1]
                centroidY := boundingCircle[2]
                radius := boundingCircle[3]
            } else if boundingCircle.Length = 2 && IsObject(boundingCircle[1]) {
                center := boundingCircle[1]
                for value in center {
                    if !(value is Number)
                        throw Error("Pillow.ImageDraw.RegularPolygon bounding_circle should only contain numeric data", -1)
                }
                if !(boundingCircle[2] is Number)
                    throw Error("Pillow.ImageDraw.RegularPolygon bounding_circle should only contain numeric data", -1)
                if center.Length != 2
                    throw Error("Pillow.ImageDraw.RegularPolygon bounding_circle centre should contain 2D coordinates (e.g. (x, y))", -1)
                centroidX := center[1]
                centroidY := center[2]
                radius := boundingCircle[2]
            } else {
                throw Error("Pillow.ImageDraw.RegularPolygon bounding_circle should contain 2D coordinates and a radius (e.g. (x, y, r) or ((x, y), r) )", -1)
            }

            if radius <= 0
                throw Error("Pillow.ImageDraw.RegularPolygon bounding_circle radius should be > 0", -1)
            if !(rotation is Number)
                throw Error("Pillow.ImageDraw.RegularPolygon rotation should be an int or float", -1)

            points := []
            degrees := 360 / nSides
            angle := (270 - 0.5 * degrees) + rotation
            loop nSides {
                points.Push(Pillow.ImageDraw.RegularPolygonVertex(centroidX, centroidY, radius, angle))
                angle += degrees
                if angle > 360
                    angle -= 360
            }
            return points
        }

        static RegularPolygonVertex(centroidX, centroidY, radius, angle) {
            radians := (360 - angle) * 3.141592653589793 / 180
            return [
                Round(radius * Cos(radians) + centroidX, 2) + 0.0,
                Round(radius * Sin(radians) + centroidY, 2) + 0.0,
            ]
        }

        static FlattenPoints(xy, operationName, minPoints := 2) {
            if !IsObject(xy)
                throw Error("Pillow.ImageDraw." operationName " expects a coordinate array", -1)

            values := []
            for item in xy {
                if IsObject(item) {
                    if item.Length != 2
                        throw Error("Pillow.ImageDraw." operationName " point expects [x, y]", -1)
                    values.Push(item[1])
                    values.Push(item[2])
                } else {
                    values.Push(item)
                }
            }

            if Mod(values.Length, 2) != 0 || values.Length < minPoints * 2
                throw Error("Pillow.ImageDraw." operationName " expects at least " minPoints " point" (minPoints = 1 ? "" : "s"), -1)
            buf := Buffer(values.Length * 4, 0)
            for index, value in values {
                if !(value is Number)
                    throw Error("Pillow.ImageDraw." operationName " coordinates must be numeric", -1)
                NumPut("Int", Integer(value), buf, (index - 1) * 4)
            }
            return buf
        }
    }

    class ImageStat {
        class Stat {
            __New(imageOrList, mask := unset) {
                if IsObject(imageOrList) && imageOrList is Pillow.Image {
                    this.Histogram := IsSet(mask) ? imageOrList.Histogram(mask) : imageOrList.Histogram()
                } else if IsObject(imageOrList) {
                    if IsSet(mask)
                        throw Error("Pillow.ImageStat.Stat mask requires image input", -1)
                    this.Histogram := imageOrList
                } else {
                    throw TypeError("first argument must be image or list", -1)
                }
                if Mod(this.Histogram.Length, 256) != 0
                    throw Error("Pillow.ImageStat.Stat histogram length must be a multiple of 256", -1)
                this.Bands := this.Histogram.Length // 256
            }

            Extrema {
                get {
                    if !this.HasOwnProp("_Extrema") {
                        values := []
                        loop this.Bands {
                            base := (A_Index - 1) * 256
                            low := 255
                            high := 0
                            loop 256 {
                                value := A_Index - 1
                                if this.Histogram[base + A_Index] {
                                    low := value
                                    break
                                }
                            }
                            loop 256 {
                                value := 256 - A_Index
                                if this.Histogram[base + value + 1] {
                                    high := value
                                    break
                                }
                            }
                            values.Push([low, high])
                        }
                        this._Extrema := values
                    }
                    return this._Extrema
                }
            }

            Count {
                get {
                    if !this.HasOwnProp("_Count") {
                        values := []
                        loop this.Bands {
                            base := (A_Index - 1) * 256
                            total := 0
                            loop 256
                                total += this.Histogram[base + A_Index]
                            values.Push(total)
                        }
                        this._Count := values
                    }
                    return this._Count
                }
            }

            Sum {
                get {
                    if !this.HasOwnProp("_Sum") {
                        values := []
                        loop this.Bands {
                            base := (A_Index - 1) * 256
                            total := 0.0
                            loop 256 {
                                level := A_Index - 1
                                total += level * this.Histogram[base + A_Index]
                            }
                            values.Push(total)
                        }
                        this._Sum := values
                    }
                    return this._Sum
                }
            }

            Sum2 {
                get {
                    if !this.HasOwnProp("_Sum2") {
                        values := []
                        loop this.Bands {
                            base := (A_Index - 1) * 256
                            total := 0.0
                            loop 256 {
                                level := A_Index - 1
                                total += level * level * this.Histogram[base + A_Index]
                            }
                            values.Push(total)
                        }
                        this._Sum2 := values
                    }
                    return this._Sum2
                }
            }

            Mean {
                get {
                    if !this.HasOwnProp("_Mean") {
                        values := []
                        loop this.Bands
                            values.Push(this.Sum[A_Index] / this.Count[A_Index])
                        this._Mean := values
                    }
                    return this._Mean
                }
            }

            Median {
                get {
                    if !this.HasOwnProp("_Median") {
                        values := []
                        loop this.Bands {
                            band := A_Index
                            base := (band - 1) * 256
                            total := 0
                            half := this.Count[band] // 2
                            median := 255
                            loop 256 {
                                value := A_Index - 1
                                total += this.Histogram[base + A_Index]
                                if total > half {
                                    median := value
                                    break
                                }
                            }
                            values.Push(median)
                        }
                        this._Median := values
                    }
                    return this._Median
                }
            }

            Rms {
                get {
                    if !this.HasOwnProp("_Rms") {
                        values := []
                        loop this.Bands
                            values.Push(Sqrt(this.Sum2[A_Index] / this.Count[A_Index]))
                        this._Rms := values
                    }
                    return this._Rms
                }
            }

            Var {
                get {
                    if !this.HasOwnProp("_Var") {
                        values := []
                        loop this.Bands {
                            count := this.Count[A_Index]
                            values.Push((this.Sum2[A_Index] - (this.Sum[A_Index] ** 2.0) / count) / count)
                        }
                        this._Var := values
                    }
                    return this._Var
                }
            }

            StdDev {
                get {
                    if !this.HasOwnProp("_StdDev") {
                        values := []
                        loop this.Bands
                            values.Push(Sqrt(this.Var[A_Index]))
                        this._StdDev := values
                    }
                    return this._StdDev
                }
            }
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
        class PixelAccess {
            __New(image) {
                this.Image := image
            }

            __Item[x, y] {
                get => this.Image.GetPixel([x, y])
                set => this.Image.PutPixel([x, y], value)
            }
        }

        __New(handle) {
            this.Handle := handle
        }

        __Delete() {
            this.Close()
        }

        static LinearGradient(modeName) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_linear_gradient",
                "Int", Pillow.ModeId(modeName),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static RadialGradient(modeName) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_radial_gradient",
                "Int", Pillow.ModeId(modeName),
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static EffectMandelbrot(size, extent, quality) {
            if !IsObject(size) || size.Length != 2
                throw Error("Pillow.Image.EffectMandelbrot expects size [width, height]", -1)
            if !IsObject(extent) || extent.Length != 4
                throw Error("Pillow.Image.EffectMandelbrot expects extent [x0, y0, x1, y1]", -1)
            if !(quality is Integer)
                throw Error("Pillow.Image.EffectMandelbrot quality must be an integer", -1)

            extentBuffer := Buffer(4 * 8, 0)
            for index, value in extent {
                if !(value is Number)
                    throw Error("Pillow.Image.EffectMandelbrot extent values must be numeric", -1)
                NumPut("Double", value, extentBuffer, (index - 1) * 8)
            }

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_effect_mandelbrot",
                "Int", size[1],
                "Int", size[2],
                "Ptr", extentBuffer,
                "Int", quality,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static EffectNoise(size, sigma) {
            if !IsObject(size) || size.Length != 2
                throw Error("Pillow.Image.EffectNoise expects size [width, height]", -1)
            if !(sigma is Number)
                throw Error("Pillow.Image.EffectNoise sigma must be numeric", -1)

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_effect_noise",
                "Int", size[1],
                "Int", size[2],
                "Double", sigma,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        static EffectSpread(image, distance) {
            if !(image is Pillow.Image)
                throw Error("Pillow.Image.EffectSpread expects a Pillow.Image", -1)
            return image.EffectSpread(distance)
        }

        static Open(path, formats := unset) {
            if !(path is String)
                throw Error("Pillow.Image.Open expects a file path", -1)
            format := Pillow.Image.ResolveOpenFormat(path, IsSet(formats) ? formats : unset)

            pathBytes := Pillow.Image.Utf8Buffer(path)
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_open_" StrLower(format),
                "Ptr", pathBytes,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
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

        static FromBytes(modeName, size, bytes, decoder := unset, rawmode := unset, stride := 0, orientation := 1) {
            image := Pillow.Image.New(modeName, size)
            try {
                if IsSet(decoder) {
                    if decoder != "raw"
                        throw Error("Pillow.Image.FromBytes currently supports only the raw decoder", -1)
                    if !IsSet(rawmode)
                        rawmode := modeName
                    rawModeBytes := Pillow.Image.RawModeBuffer(rawmode)
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_set_raw_bytes",
                        "Ptr", image.Handle,
                        "Ptr", bytes,
                        "UPtr", bytes.Size,
                        "Ptr", rawModeBytes,
                        "Int", stride,
                        "Int", orientation,
                        "Int"
                    ))
                } else {
                    rawModeBytes := Pillow.Image.RawModeBuffer(modeName)
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_set_raw_bytes",
                        "Ptr", image.Handle,
                        "Ptr", bytes,
                        "UPtr", bytes.Size,
                        "Ptr", rawModeBytes,
                        "Int", 0,
                        "Int", 1,
                        "Int"
                    ))
                }
                return image
            } catch {
                image.Close()
                throw
            }
        }

        static Eval(image, fn) {
            if !(IsObject(image) && image is Pillow.Image)
                throw Error("Pillow.Image.Eval expects a Pillow.Image", -1)
            if !(fn is Func)
                throw Error("Pillow.Image.Eval expects a callable function", -1)

            return image.Point(fn)
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

        static Composite(image1, image2, mask) {
            if !(IsObject(mask) && mask is Pillow.Image)
                throw Error("bad transparency mask", -1)
            if !(mask.Mode = "1" || mask.Mode = "L" || mask.Mode = "LA" || mask.Mode = "RGBA")
                throw Error("bad transparency mask", -1)

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_composite",
                "Ptr", image1.RequireHandle(),
                "Ptr", image2.RequireHandle(),
                "Ptr", mask.RequireHandle(),
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

        static RoundClipU8(value) {
            if !(value is Number)
                throw Error("Pillow.Image.Eval function must return numeric values", -1)
            rounded := Pillow.Image.RoundHalfEven(value)
            if rounded <= 0
                return 0
            if rounded >= 255
                return 255
            return rounded
        }

        static RoundHalfEven(value) {
            floorValue := Floor(value)
            fraction := value - floorValue
            if fraction < 0.5
                return floorValue
            if fraction > 0.5
                return floorValue + 1
            return Mod(floorValue, 2) = 0 ? floorValue : floorValue + 1
        }

        static TruncateClipU8(value) {
            if !(value is Number)
                throw Error("Pillow.Image.PutData expects numeric pixel values", -1)
            if value <= 0
                return 0
            if value >= 255
                return 255
            return Floor(value)
        }

        static ClipTupleU8(value) {
            if !(value is Integer)
                throw Error("Pillow.Image.PutData tuple values must be integers", -1)
            if value <= 0
                return 0
            if value >= 255
                return 255
            return value
        }

        static ByteBuffer(values, operationName) {
            if !IsObject(values)
                throw Error(operationName " expects an array of byte values", -1)
            buf := Buffer(values.Length, 0)
            for index, value in values {
                if !(value is Integer) || value < 0 || value > 255
                    throw Error(operationName " byte values must be integers in range 0..255", -1)
                NumPut("UChar", value, buf, index - 1)
            }
            return buf
        }

        static IntBuffer(values, operationName) {
            if !IsObject(values)
                throw Error(operationName " expects an array of integers", -1)
            buf := Buffer(values.Length * 4, 0)
            for index, value in values {
                if !(value is Integer)
                    throw Error(operationName " values must be integers", -1)
                NumPut("Int", value, buf, (index - 1) * 4)
            }
            return buf
        }

        static RawModeBuffer(rawmode) {
            buf := Buffer(StrPut(rawmode, "UTF-8"), 0)
            StrPut(rawmode, buf, "UTF-8")
            return buf
        }

        static Utf8Buffer(value) {
            buf := Buffer(StrPut(value, "UTF-8"), 0)
            StrPut(value, buf, "UTF-8")
            return buf
        }

        static ResolveOpenFormat(path, formats := unset) {
            if IsSet(formats) {
                if !IsObject(formats) || formats.Length != 1
                    throw Error("Pillow.Image.Open currently supports one explicit format", -1)
                return Pillow.Image.NormalizeFileFormat(formats[1])
            }
            return Pillow.Image.FormatFromPath(path)
        }

        static ResolveSaveFormat(path, format := unset) {
            if IsSet(format)
                return Pillow.Image.NormalizeFileFormat(format)
            return Pillow.Image.FormatFromPath(path)
        }

        static FormatFromPath(path) {
            if RegExMatch(path, "i)\.bmp$")
                return "BMP"
            if RegExMatch(path, "i)\.png$")
                return "PNG"
            if RegExMatch(path, "i)\.jpe?g$")
                return "JPEG"
            if RegExMatch(path, "i)\.tiff?$")
                return "TIFF"
            if RegExMatch(path, "i)\.gif$")
                return "GIF"
            throw Error("Pillow image file format is unsupported", -1)
        }

        static NormalizeFileFormat(format) {
            name := StrUpper(format)
            if name = "JPG"
                return "JPEG"
            if name = "TIF"
                return "TIFF"
            if name = "BMP" || name = "PNG" || name = "JPEG" || name = "TIFF" || name = "GIF"
                return name
            throw Error("Pillow image file format is unsupported", -1)
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

        Load() {
            this.RequireHandle()
            return Pillow.Image.PixelAccess(this)
        }

        Tell() {
            this.RequireHandle()
            return 0
        }

        Seek(frame) {
            this.RequireHandle()
            if frame != 0
                throw Error("no more images in file", -1)
        }

        Verify() {
            this.RequireHandle()
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

        ToBytes(encoder := unset, rawmode := unset) {
            if IsSet(encoder) {
                if encoder != "raw"
                    throw Error("Pillow.Image.ToBytes currently supports only the raw encoder", -1)
                if !IsSet(rawmode)
                    rawmode := this.Mode
                rawModeBytes := Pillow.Image.RawModeBuffer(rawmode)
                required := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_get_raw_bytes",
                    "Ptr", this.RequireHandle(),
                    "Ptr", rawModeBytes,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                ))
                out := Buffer(required, 0)
                if required = 0
                    return out
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_get_raw_bytes",
                    "Ptr", this.RequireHandle(),
                    "Ptr", rawModeBytes,
                    "Ptr", out,
                    "UPtr", out.Size,
                    "UPtr*", &required,
                    "Int"
                ))
                return out
            }

            if this.Mode = "1"
                return this.ToBytes("raw", "1")

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

        Save(path, format := unset) {
            if !(path is String)
                throw Error("Pillow.Image.Save expects a file path", -1)
            resolvedFormat := Pillow.Image.ResolveSaveFormat(path, IsSet(format) ? format : unset)

            pathBytes := Pillow.Image.Utf8Buffer(path)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_save_" StrLower(resolvedFormat),
                "Ptr", this.RequireHandle(),
                "Ptr", pathBytes,
                "Int"
            ))
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

        GetData(band := unset) {
            bytes := this.Mode = "1" ? this.InternalBytes() : this.ToBytes()
            channels := this.Channels
            pixelCount := this.Width * this.Height
            values := []
            if IsSet(band) {
                if !(band is Integer)
                    throw Error("sequence index must be integer", -1)
                if band < 0 || band >= channels
                    throw Error("band index out of range", -1)
                loop pixelCount
                    values.Push(NumGet(bytes, (A_Index - 1) * channels + band, "UChar"))
                return values
            }

            if channels = 1 {
                loop bytes.Size
                    values.Push(NumGet(bytes, A_Index - 1, "UChar"))
                return values
            }

            loop pixelCount {
                offset := (A_Index - 1) * channels
                pixel := []
                loop channels
                    pixel.Push(NumGet(bytes, offset + A_Index - 1, "UChar"))
                values.Push(pixel)
            }
            return values
        }

        InternalBytes() {
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

        PutPalette(data, rawmode := "RGB") {
            if !(this.Mode = "P" || this.Mode = "L")
                throw Error("illegal image mode", -1)
            if rawmode != "RGB"
                throw Error("Pillow.Image.PutPalette currently supports only RGB palettes", -1)
            palette := Pillow.Image.ByteBuffer(data, "Pillow.Image.PutPalette")
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_put_palette_rgb",
                "Ptr", this.RequireHandle(),
                "Ptr", palette,
                "UPtr", palette.Size,
                "Int"
            ))
            return this
        }

        GetPalette(rawmode := "RGB") {
            if this.Mode != "P"
                throw Error("illegal image mode", -1)
            if rawmode != "RGB"
                throw Error("Pillow.Image.GetPalette currently supports only RGB palettes", -1)
            required := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_get_palette_rgb",
                "Ptr", this.RequireHandle(),
                "Ptr", 0,
                "UPtr", 0,
                "UPtr*", &required,
                "Int"
            ))
            if required = 0
                return []
            out := Buffer(required, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_get_palette_rgb",
                "Ptr", this.RequireHandle(),
                "Ptr", out,
                "UPtr", out.Size,
                "UPtr*", &required,
                "Int"
            ))
            values := []
            loop out.Size
                values.Push(NumGet(out, A_Index - 1, "UChar"))
            return values
        }

        RemapPalette(destMap, sourcePalette := unset) {
            map := Pillow.Image.IntBuffer(destMap, "Pillow.Image.RemapPalette")
            palette := IsSet(sourcePalette) ? Pillow.Image.ByteBuffer(sourcePalette, "Pillow.Image.RemapPalette source_palette") : 0
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_remap_palette",
                "Ptr", this.RequireHandle(),
                "Ptr", map,
                "UPtr", destMap.Length,
                "Ptr", IsObject(palette) ? palette.Ptr : 0,
                "UPtr", IsObject(palette) ? palette.Size : 0,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        PutData(data, scale := 1.0, offset := 0.0) {
            if !IsObject(data)
                throw Error("Pillow.Image.PutData expects an array of pixel values", -1)
            if !(scale is Number) || !(offset is Number)
                throw Error("Pillow.Image.PutData scale and offset must be numeric", -1)

            pixelCount := this.Width * this.Height
            if data.Length > pixelCount
                throw Error("too many data entries", -1)

            channels := this.Channels
            packed := Buffer(data.Length * channels, 0)
            for index, value in data
                this.WritePutDataPixel(packed, index - 1, value, scale, offset)

            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_put_data",
                "Ptr", this.RequireHandle(),
                "Ptr", packed,
                "UPtr", packed.Size,
                "UPtr", data.Length,
                "Int"
            ))
            return this
        }

        WritePutDataPixel(buf, pixelIndex, value, scale, offset) {
            channels := this.Channels
            base := pixelIndex * channels
            if channels = 1 {
                if IsObject(value)
                    throw Error("sequence must be flattened", -1)
                NumPut("UChar", Pillow.Image.TruncateClipU8(value * scale + offset), buf, base)
                return
            }

            if IsObject(value) {
                this.WritePutDataTuple(buf, base, value)
                return
            }

            if !(value is Integer)
                throw Error("color must be int or tuple", -1)
            loop channels
                NumPut("UChar", (value >> ((A_Index - 1) * 8)) & 0xFF, buf, base + A_Index - 1)
        }

        WritePutDataTuple(buf, base, value) {
            channels := this.Channels
            length := value.Length
            if channels = 2 {
                if length != 1 && length != 2
                    throw Error("color must be int, or tuple of one or two elements", -1)
                NumPut("UChar", Pillow.Image.ClipTupleU8(value[1]), buf, base)
                NumPut("UChar", length = 2 ? Pillow.Image.ClipTupleU8(value[2]) : 0, buf, base + 1)
                return
            }

            if channels = 3 {
                if length != 1 && length != 3 && length != 4
                    throw Error("color must be int, or tuple of one, three or four elements", -1)
                NumPut("UChar", Pillow.Image.ClipTupleU8(value[1]), buf, base)
                NumPut("UChar", length >= 3 ? Pillow.Image.ClipTupleU8(value[2]) : 0, buf, base + 1)
                NumPut("UChar", length >= 3 ? Pillow.Image.ClipTupleU8(value[3]) : 0, buf, base + 2)
                return
            }

            if channels = 4 {
                if length != 1 && length != 3 && length != 4
                    throw Error("color must be int, or tuple of one, three or four elements", -1)
                NumPut("UChar", Pillow.Image.ClipTupleU8(value[1]), buf, base)
                NumPut("UChar", length >= 3 ? Pillow.Image.ClipTupleU8(value[2]) : 0, buf, base + 1)
                NumPut("UChar", length >= 3 ? Pillow.Image.ClipTupleU8(value[3]) : 0, buf, base + 2)
                NumPut("UChar", length = 4 ? Pillow.Image.ClipTupleU8(value[4]) : (length = 3 ? 255 : 0), buf, base + 3)
                return
            }

            throw Error("Pillow.Image.PutData unsupported image mode", -1)
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

        GetPixel(xy) {
            if !IsObject(xy) || xy.Length != 2
                throw Error("Pillow.Image.GetPixel expects xy [x, y]", -1)
            out := Buffer(this.Channels, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_getpixel",
                "Ptr", this.RequireHandle(),
                "Int", xy[1],
                "Int", xy[2],
                "Ptr", out,
                "UPtr", out.Size,
                "Int"
            ))
            return this.PixelBufferToValue(out)
        }

        PutPixel(xy, value) {
            if !IsObject(xy) || xy.Length != 2
                throw Error("Pillow.Image.PutPixel expects xy [x, y]", -1)
            color := this.PixelValueBuffer(value)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_putpixel",
                "Ptr", this.RequireHandle(),
                "Int", xy[1],
                "Int", xy[2],
                "Ptr", color,
                "UPtr", color.Size,
                "Int"
            ))
        }

        PixelBufferToValue(buf) {
            if this.Channels = 1
                return NumGet(buf, 0, "UChar")
            values := []
            loop buf.Size
                values.Push(NumGet(buf, A_Index - 1, "UChar"))
            return values
        }

        PixelValueBuffer(value) {
            if IsObject(value) {
                if this.Channels = 1 {
                    if value.Length != 1
                        throw Error("Pillow.Image.PutPixel color must be int or single-element array", -1)
                    buf := Buffer(1, 0)
                    NumPut("UChar", this.ModeAwareU8(value[1]), buf, 0)
                    return buf
                }
                if value.Length != this.Channels
                    throw Error("Pillow.Image.PutPixel color must match image mode", -1)
                buf := Buffer(this.Channels, 0)
                for index, item in value
                    NumPut("UChar", item, buf, index - 1)
                return buf
            }

            buf := Buffer(1, 0)
            NumPut("UChar", this.ModeAwareU8(value), buf, 0)
            return buf
        }

        ModeAwareU8(value) {
            return this.Mode = "1" ? Pillow.Image.ModeOnePixelValue(value) : value
        }

        static ModeOnePixelValue(value) {
            return value <= 0 ? 0 : value > 255 ? 255 : value
        }

        ColorBuffer(color) {
            channels := this.Channels
            if IsObject(color) {
                if color.Length != channels
                    throw Error("Pillow color length must match image channels", -1)
                buf := Buffer(channels, 0)
                for index, value in color
                    NumPut("UChar", this.ModeAwareU8(value), buf, index - 1)
                return buf
            }

            if channels != 1
                throw Error("Scalar color is only valid for single-channel images", -1)
            buf := Buffer(1, 0)
            NumPut("UChar", this.ModeAwareU8(color), buf, 0)
            return buf
        }

        PasteColorBuffer(color) {
            channels := this.Channels
            if IsObject(color) {
                length := color.Length
                if channels = 1 {
                    if length != 1
                        throw Error("Pillow color must be int or single-element array", -1)
                    buf := Buffer(1, 0)
                    NumPut("UChar", this.ModeAwareU8(color[1]), buf, 0)
                    return buf
                }
                if channels = 2 {
                    if length != 1 && length != 2
                        throw Error("Pillow color must be int, or array of one or two elements", -1)
                    buf := Buffer(2, 0)
                    NumPut("UChar", color[1], buf, 0)
                    NumPut("UChar", length = 2 ? color[2] : 0, buf, 1)
                    return buf
                }
                if channels = 3 {
                    if length != 1 && length != 3 && length != 4
                        throw Error("Pillow color must be int, or array of one, three or four elements", -1)
                    buf := Buffer(3, 0)
                    NumPut("UChar", color[1], buf, 0)
                    NumPut("UChar", length >= 3 ? color[2] : 0, buf, 1)
                    NumPut("UChar", length >= 3 ? color[3] : 0, buf, 2)
                    return buf
                }
                if channels = 4 {
                    if length != 1 && length != 3 && length != 4
                        throw Error("Pillow color must be int, or array of one, three or four elements", -1)
                    buf := Buffer(4, 0)
                    NumPut("UChar", color[1], buf, 0)
                    NumPut("UChar", length >= 3 ? color[2] : 0, buf, 1)
                    NumPut("UChar", length >= 3 ? color[3] : 0, buf, 2)
                    NumPut("UChar", length = 4 ? color[4] : (length = 3 ? 255 : 0), buf, 3)
                    return buf
                }
                throw Error("Pillow color is unsupported for this image mode", -1)
            }

            buf := Buffer(channels, 0)
            NumPut("UChar", this.ModeAwareU8(color), buf, 0)
            return buf
        }

        TransformFillBuffer(color) {
            channels := this.Channels
            if IsObject(color) {
                if channels = 1 {
                    if color.Length != 1
                        throw Error("Pillow transform fill must match image mode", -1)
                    buf := Buffer(1, 0)
                    NumPut("UChar", this.ModeAwareU8(color[1]), buf, 0)
                    return buf
                }
                if channels = 2 {
                    if color.Length != 1 && color.Length != 2
                        throw Error("Pillow transform fill must match image mode", -1)
                    buf := Buffer(color.Length, 0)
                    loop color.Length
                        NumPut("UChar", color[A_Index], buf, A_Index - 1)
                    return buf
                }
                if channels = 3 {
                    if color.Length != 1 && color.Length != 3 && color.Length != 4
                        throw Error("Pillow transform fill must match image mode", -1)
                    buf := Buffer(color.Length = 1 ? 1 : color.Length, 0)
                    loop color.Length
                        NumPut("UChar", color[A_Index], buf, A_Index - 1)
                    return buf
                }
                if channels = 4 {
                    if color.Length != 1 && color.Length != 3 && color.Length != 4
                        throw Error("Pillow transform fill must match image mode", -1)
                    buf := Buffer(color.Length = 1 ? 1 : color.Length, 0)
                    loop color.Length
                        NumPut("UChar", color[A_Index], buf, A_Index - 1)
                    return buf
                }
                throw Error("Pillow transform fill is unsupported for this image mode", -1)
            }

            buf := Buffer(1, 0)
            NumPut("UChar", this.ModeAwareU8(color), buf, 0)
            return buf
        }

        Point(lut, modeName := unset) {
            if lut is Func
                lut := this.CallablePointLut(lut)
            lutBytes := this.LutBuffer(lut)
            outHandle := 0
            if IsSet(modeName) {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_point_lut_mode",
                    "Ptr", this.RequireHandle(),
                    "Ptr", lutBytes,
                    "UPtr", lutBytes.Size,
                    "Int", Pillow.ModeId(modeName),
                    "Ptr*", &outHandle,
                    "Int"
                ))
            } else {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_point_lut",
                    "Ptr", this.RequireHandle(),
                    "Ptr", lutBytes,
                    "UPtr", lutBytes.Size,
                    "Ptr*", &outHandle,
                    "Int"
                ))
            }
            return Pillow.WrapImageHandle(outHandle)
        }

        CallablePointLut(fn) {
            baseLut := []
            loop 256
                baseLut.Push(Pillow.Image.RoundClipU8(fn.Call(A_Index - 1)))

            lut := []
            loop this.Channels {
                for value in baseLut
                    lut.Push(value)
            }
            return lut
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

        Histogram(mask := unset) {
            count := this.Channels * 256
            out := Buffer(count * 8, 0)
            if IsSet(mask) {
                if !(IsObject(mask) && mask is Pillow.Image)
                    throw Error("Pillow.Image.Histogram mask expects a Pillow.Image", -1)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_histogram_masked",
                    "Ptr", this.RequireHandle(),
                    "Ptr", mask.RequireHandle(),
                    "Ptr", out,
                    "UPtr", count,
                    "Int"
                ))
            } else {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_histogram",
                    "Ptr", this.RequireHandle(),
                    "Ptr", out,
                    "UPtr", count,
                    "Int"
                ))
            }
            values := []
            loop count
                values.Push(NumGet(out, (A_Index - 1) * 8, "Int64"))
            return values
        }

        Entropy(mask := unset) {
            value := 0.0
            maskHandle := 0
            if IsSet(mask) {
                if !(IsObject(mask) && mask is Pillow.Image)
                    throw Error("Pillow.Image.Entropy mask expects a Pillow.Image", -1)
                maskHandle := mask.RequireHandle()
            }
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_entropy",
                "Ptr", this.RequireHandle(),
                "Ptr", maskHandle,
                "Double*", &value,
                "Int"
            ))
            return value
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

        GetBbox(alphaOnly := true) {
            left := 0
            top := 0
            right := 0
            bottom := 0
            hasBbox := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_getbbox",
                "Ptr", this.RequireHandle(),
                "Int", alphaOnly ? 1 : 0,
                "Int*", &left,
                "Int*", &top,
                "Int*", &right,
                "Int*", &bottom,
                "Int*", &hasBbox,
                "Int"
            ))
            return hasBbox ? [left, top, right, bottom] : 0
        }

        GetProjection() {
            xProjection := Buffer(this.Width, 0)
            yProjection := Buffer(this.Height, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_getprojection",
                "Ptr", this.RequireHandle(),
                "Ptr", xProjection,
                "UPtr", xProjection.Size,
                "Ptr", yProjection,
                "UPtr", yProjection.Size,
                "Int"
            ))
            return [Pillow.Image.ProjectionBufferToArray(xProjection), Pillow.Image.ProjectionBufferToArray(yProjection)]
        }

        static ProjectionBufferToArray(buf) {
            values := []
            loop buf.Size
                values.Push(NumGet(buf, A_Index - 1, "UChar"))
            return values
        }

        GetColors(maxcolors := 256) {
            count := 0
            exceeded := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_getcolors",
                "Ptr", this.RequireHandle(),
                "Int", maxcolors,
                "Ptr", 0,
                "Ptr", 0,
                "UPtr", 0,
                "UPtr*", &count,
                "Int*", &exceeded,
                "Int"
            ))
            if exceeded
                return 0

            counts := Buffer(count * 8, 0)
            colors := Buffer(count * this.Channels, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_getcolors",
                "Ptr", this.RequireHandle(),
                "Int", maxcolors,
                "Ptr", counts,
                "Ptr", colors,
                "UPtr", count,
                "UPtr*", &count,
                "Int*", &exceeded,
                "Int"
            ))
            if exceeded
                return 0
            return this.ColorCountsToArray(counts, colors, count)
        }

        ColorCountsToArray(counts, colors, count) {
            out := []
            channels := this.Channels
            loop count {
                itemIndex := A_Index - 1
                pixel := 0
                if channels = 1 {
                    pixel := NumGet(colors, itemIndex, "UChar")
                } else {
                    pixel := []
                    loop channels
                        pixel.Push(NumGet(colors, itemIndex * channels + A_Index - 1, "UChar"))
                }
                out.Push([NumGet(counts, itemIndex * 8, "Int64"), pixel])
            }
            return out
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

        GetBands() {
            names := this.BandNames()
            return names.Clone()
        }

        BandNames() {
            mode := this.Mode
            if mode = "1"
                return ["1"]
            if mode = "P"
                return ["P"]
            if mode = "L"
                return ["L"]
            if mode = "LA"
                return ["L", "A"]
            if mode = "RGB"
                return ["R", "G", "B"]
            if mode = "RGBA"
                return ["R", "G", "B", "A"]
            if mode = "CMYK"
                return ["C", "M", "Y", "K"]
            return []
        }

        ChannelIndex(channel) {
            if !IsObject(channel) && channel is Integer {
                if channel < 0 || channel >= this.Channels
                    throw Error("band index out of range", -1)
                return channel
            }

            name := channel ""
            names := this.BandNames()
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

        Crop(box := unset) {
            if !IsSet(box)
                return this.Copy()
            if !IsObject(box) || box.Length != 4
                throw Error("Pillow.Image.Crop expects box [left, top, right, bottom]", -1)
            cropBox := []
            for value in box {
                if !(value is Number)
                    throw Error("Pillow.Image.Crop box coordinates must be numeric", -1)
                cropBox.Push(Pillow.Image.RoundHalfEven(value))
            }

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_crop",
                "Ptr", this.RequireHandle(),
                "Int", cropBox[1],
                "Int", cropBox[2],
                "Int", cropBox[3],
                "Int", cropBox[4],
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        Resize(size, resample := unset, box := unset, reducingGap := unset) {
            if size.Length != 2
                throw Error("Pillow.Image.Resize expects size [width, height]", -1)
            if !IsSet(resample)
                resample := (this.Mode = "1" || this.Mode = "P") ? Pillow.Resampling.NEAREST : Pillow.Resampling.BICUBIC

            outHandle := 0
            if IsSet(reducingGap) {
                if !(reducingGap is Number) || reducingGap < 1.0
                    throw Error("Pillow.Image.Resize reducingGap must be 1.0 or greater", -1)
                if IsSet(box) {
                    if !IsObject(box) || box.Length != 4
                        throw Error("Pillow.Image.Resize box expects [left, top, right, bottom]", -1)
                    for value in box {
                        if !(value is Number)
                            throw Error("Pillow.Image.Resize box coordinates must be numeric", -1)
                    }
                } else {
                    box := [0.0, 0.0, this.Width + 0.0, this.Height + 0.0]
                }
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_resize_reducing_gap",
                    "Ptr", this.RequireHandle(),
                    "Int", size[1],
                    "Int", size[2],
                    "Int", resample,
                    "Double", box[1],
                    "Double", box[2],
                    "Double", box[3],
                    "Double", box[4],
                    "Double", reducingGap,
                    "Ptr*", &outHandle,
                    "Int"
                ))
            } else if IsSet(box) {
                if !IsObject(box) || box.Length != 4
                    throw Error("Pillow.Image.Resize box expects [left, top, right, bottom]", -1)
                for value in box {
                    if !(value is Number)
                        throw Error("Pillow.Image.Resize box coordinates must be numeric", -1)
                }
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_resize_box",
                    "Ptr", this.RequireHandle(),
                    "Int", size[1],
                    "Int", size[2],
                    "Int", resample,
                    "Double", box[1],
                    "Double", box[2],
                    "Double", box[3],
                    "Double", box[4],
                    "Ptr*", &outHandle,
                    "Int"
                ))
            } else {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_resize",
                    "Ptr", this.RequireHandle(),
                    "Int", size[1],
                    "Int", size[2],
                    "Int", resample,
                    "Ptr*", &outHandle,
                    "Int"
                ))
            }
            return Pillow.WrapImageHandle(outHandle)
        }

        Thumbnail(size, resample := unset, reducingGap := 2.0) {
            if !IsObject(size) || size.Length != 2
                throw Error("Pillow.Image.Thumbnail size expects [width, height]", -1)
            if !(size[1] is Number) || !(size[2] is Number)
                throw Error("Pillow.Image.Thumbnail size values must be numbers", -1)
            if !IsSet(resample)
                resample := Pillow.Resampling.BICUBIC
            if IsSet(reducingGap) && (!(reducingGap is Number) || reducingGap <= 1.0)
                throw Error("Pillow.Image.Thumbnail reducingGap must be greater than 1.0", -1)

            requestedWidth := Floor(size[1])
            requestedHeight := Floor(size[2])
            if requestedWidth <= 0 || requestedHeight <= 0
                throw Error("Pillow.Image.Thumbnail height and width must be > 0", -1)

            sourceWidth := this.Width
            sourceHeight := this.Height
            if requestedWidth >= sourceWidth && requestedHeight >= sourceHeight
                return

            aspect := sourceWidth / sourceHeight
            if requestedWidth / requestedHeight >= aspect {
                finalWidth := Pillow.Image.ThumbnailRoundAspect(requestedHeight * aspect, (candidate) => Abs(aspect - candidate / requestedHeight))
                finalHeight := requestedHeight
            } else {
                finalWidth := requestedWidth
                finalHeight := Pillow.Image.ThumbnailRoundAspect(requestedWidth / aspect, (candidate) => candidate = 0 ? 0 : Abs(aspect - requestedWidth / candidate))
            }

            resized := IsSet(reducingGap)
                ? this.Resize([finalWidth, finalHeight], resample, unset, reducingGap)
                : this.Resize([finalWidth, finalHeight], resample)
            oldHandle := this.Handle
            this.Handle := resized.RequireHandle()
            resized.Handle := 0
            Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", oldHandle, "Int"))
            return
        }

        static ThumbnailRoundAspect(number, key) {
            floorValue := Floor(number)
            ceilValue := Ceil(number)
            floorKey := key(floorValue)
            ceilKey := key(ceilValue)
            result := floorKey <= ceilKey ? floorValue : ceilValue
            return result < 1 ? 1 : result
        }

        Reduce(factor, box := unset) {
            scale := this.ReduceFactor(factor)
            cropBox := IsSet(box) ? this.ReduceBox(box) : [0, 0, this.Width, this.Height]
            if scale[1] = 1 && scale[2] = 1 &&
                cropBox[1] = 0 && cropBox[2] = 0 && cropBox[3] = this.Width && cropBox[4] = this.Height
                return this.Copy()
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_reduce",
                "Ptr", this.RequireHandle(),
                "Int", scale[1],
                "Int", scale[2],
                "Int", cropBox[1],
                "Int", cropBox[2],
                "Int", cropBox[3],
                "Int", cropBox[4],
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        ReduceFactor(factor) {
            if IsObject(factor) {
                if factor.Length != 2
                    throw Error("Pillow.Image.Reduce factor must be an integer or [x, y]", -1)
                return [factor[1], factor[2]]
            }
            return [factor, factor]
        }

        ReduceBox(box) {
            if !IsObject(box) || box.Length != 4
                throw Error("Pillow.Image.Reduce box expects [left, top, right, bottom]", -1)
            return box
        }

        Filter(filter) {
            if IsObject(filter) && HasMethod(filter, "Apply")
                return filter.Apply(this)
            throw Error("Pillow.Image.Filter expects an ImageFilter object", -1)
        }

        Transform(size, method, data, resample := unset, fillcolor := unset) {
            if method == Pillow.Transform.AFFINE
                return this.TransformAffine(size, data, IsSet(resample) ? resample : unset, IsSet(fillcolor) ? fillcolor : unset)
            if method == Pillow.Transform.EXTENT
                return this.TransformExtent(size, data, IsSet(resample) ? resample : unset, IsSet(fillcolor) ? fillcolor : unset)
            if method == Pillow.Transform.PERSPECTIVE
                return this.TransformPerspective(size, data, IsSet(resample) ? resample : unset, IsSet(fillcolor) ? fillcolor : unset)
            if method == Pillow.Transform.QUAD
                return this.TransformQuad(size, data, IsSet(resample) ? resample : unset, IsSet(fillcolor) ? fillcolor : unset)
            if method == Pillow.Transform.MESH
                return this.TransformMesh(size, data, IsSet(resample) ? resample : unset, IsSet(fillcolor) ? fillcolor : unset)
            throw Error("unknown transformation method", -1)
        }

        TransformExtent(size, extent, resample := unset, fillcolor := unset) {
            if size.Length != 2
                throw Error("Pillow.Image.Transform EXTENT expects size [width, height]", -1)
            if extent.Length != 4
                throw Error("Pillow.Image.Transform EXTENT expects a 4-value extent", -1)
            if !IsSet(resample)
                resample := Pillow.Resampling.NEAREST
            for index, value in extent {
                if !(value is Number)
                    throw Error("Pillow.Image.Transform EXTENT values must be numeric", -1)
            }

            matrix := [
                (extent[3] - extent[1]) / size[1], 0.0, extent[1],
                0.0, (extent[4] - extent[2]) / size[2], extent[2],
            ]
            return this.TransformAffine(size, matrix, resample, IsSet(fillcolor) ? fillcolor : unset)
        }

        TransformPerspective(size, coefficients, resample := unset, fillcolor := unset) {
            if size.Length != 2
                throw Error("Pillow.Image.Transform PERSPECTIVE expects size [width, height]", -1)
            if coefficients.Length < 8
                throw Error("Pillow.Image.Transform PERSPECTIVE expects at least 8 coefficients", -1)
            if !IsSet(resample)
                resample := Pillow.Resampling.NEAREST
            coefficientBuffer := Buffer(8 * 8, 0)
            loop 8 {
                value := coefficients[A_Index]
                if !(value is Number)
                    throw Error("Pillow.Image.Transform PERSPECTIVE coefficients must be numeric", -1)
                NumPut("Double", value, coefficientBuffer, (A_Index - 1) * 8)
            }
            fill := IsSet(fillcolor) ? this.TransformFillBuffer(fillcolor) : 0

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_transform_perspective",
                "Ptr", this.RequireHandle(),
                "Int", size[1],
                "Int", size[2],
                "Ptr", coefficientBuffer,
                "Int", resample,
                "Ptr", IsObject(fill) ? fill.Ptr : 0,
                "UPtr", IsObject(fill) ? fill.Size : 0,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        TransformQuad(size, corners, resample := unset, fillcolor := unset) {
            if size.Length != 2
                throw Error("Pillow.Image.Transform QUAD expects size [width, height]", -1)
            if corners.Length < 8
                throw Error("Pillow.Image.Transform QUAD expects at least 8 corner values", -1)
            if !IsSet(resample)
                resample := Pillow.Resampling.NEAREST
            cornerBuffer := Buffer(8 * 8, 0)
            loop 8 {
                value := corners[A_Index]
                if !(value is Number)
                    throw Error("Pillow.Image.Transform QUAD corner values must be numeric", -1)
                NumPut("Double", value, cornerBuffer, (A_Index - 1) * 8)
            }
            fill := IsSet(fillcolor) ? this.TransformFillBuffer(fillcolor) : 0

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_transform_quad",
                "Ptr", this.RequireHandle(),
                "Int", size[1],
                "Int", size[2],
                "Ptr", cornerBuffer,
                "Int", resample,
                "Ptr", IsObject(fill) ? fill.Ptr : 0,
                "UPtr", IsObject(fill) ? fill.Size : 0,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        TransformMesh(size, mesh, resample := unset, fillcolor := unset) {
            if size.Length != 2
                throw Error("Pillow.Image.Transform MESH expects size [width, height]", -1)
            if !IsObject(mesh)
                throw Error("Pillow.Image.Transform MESH expects an array of [box, quad] entries", -1)
            if !IsSet(resample)
                resample := Pillow.Resampling.NEAREST

            meshCount := mesh.Length
            boxes := Buffer(meshCount * 4 * 4, 0)
            quads := Buffer(meshCount * 8 * 8, 0)
            for meshIndex, entry in mesh {
                if !IsObject(entry) || entry.Length != 2
                    throw Error("Pillow.Image.Transform MESH entries must be [box, quad]", -1)
                box := entry[1]
                quad := entry[2]
                if !IsObject(box) || box.Length != 4
                    throw Error("Pillow.Image.Transform MESH box must have 4 values", -1)
                if !IsObject(quad) || quad.Length < 8
                    throw Error("Pillow.Image.Transform MESH quad must have at least 8 values", -1)
                loop 4 {
                    value := box[A_Index]
                    if !(value is Integer)
                        throw Error("Pillow.Image.Transform MESH box values must be integers", -1)
                    NumPut("Int", value, boxes, ((meshIndex - 1) * 4 + A_Index - 1) * 4)
                }
                loop 8 {
                    value := quad[A_Index]
                    if !(value is Number)
                        throw Error("Pillow.Image.Transform MESH quad values must be numeric", -1)
                    NumPut("Double", value, quads, ((meshIndex - 1) * 8 + A_Index - 1) * 8)
                }
            }
            fill := IsSet(fillcolor) ? this.TransformFillBuffer(fillcolor) : 0

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_transform_mesh",
                "Ptr", this.RequireHandle(),
                "Int", size[1],
                "Int", size[2],
                "Ptr", boxes,
                "Ptr", quads,
                "UPtr", meshCount,
                "Int", resample,
                "Ptr", IsObject(fill) ? fill.Ptr : 0,
                "UPtr", IsObject(fill) ? fill.Size : 0,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        TransformAffine(size, matrix, resample := unset, fillcolor := unset) {
            if size.Length != 2
                throw Error("Pillow.Image.TransformAffine expects size [width, height]", -1)
            if matrix.Length != 6
                throw Error("Pillow.Image.TransformAffine expects a 6-value affine matrix", -1)
            if !IsSet(resample)
                resample := Pillow.Resampling.NEAREST
            matrixBuffer := Buffer(6 * 8, 0)
            for index, value in matrix {
                if !(value is Number)
                    throw Error("Pillow.Image.TransformAffine matrix values must be numeric", -1)
                NumPut("Double", value, matrixBuffer, (index - 1) * 8)
            }
            fill := IsSet(fillcolor) ? this.TransformFillBuffer(fillcolor) : 0

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_transform_affine",
                "Ptr", this.RequireHandle(),
                "Int", size[1],
                "Int", size[2],
                "Ptr", matrixBuffer,
                "Int", resample,
                "Ptr", IsObject(fill) ? fill.Ptr : 0,
                "UPtr", IsObject(fill) ? fill.Size : 0,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        Rotate(angle, resample := unset, expand := false, center := unset, translate := unset, fillcolor := unset) {
            if !(angle is Number)
                throw Error("Pillow.Image.Rotate angle must be numeric", -1)
            if !IsSet(resample)
                resample := Pillow.Resampling.NEAREST
            hasCenter := IsSet(center)
            centerX := 0.0
            centerY := 0.0
            if hasCenter {
                if !IsObject(center) || center.Length != 2
                    throw Error("Pillow.Image.Rotate center expects [x, y]", -1)
                centerX := center[1]
                centerY := center[2]
            }
            hasTranslate := IsSet(translate)
            translateX := 0.0
            translateY := 0.0
            if hasTranslate {
                if !IsObject(translate) || translate.Length != 2
                    throw Error("Pillow.Image.Rotate translate expects [x, y]", -1)
                translateX := translate[1]
                translateY := translate[2]
            }
            fill := IsSet(fillcolor) ? this.TransformFillBuffer(fillcolor) : 0

            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_rotate",
                "Ptr", this.RequireHandle(),
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
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        AlphaComposite(image, dest := unset, source := unset) {
            if !(IsObject(image) && image is Pillow.Image)
                throw Error("Pillow.Image.AlphaComposite expects a Pillow.Image", -1)
            targetDest := IsSet(dest) ? dest : [0, 0]
            targetSource := IsSet(source) ? source : [0, 0]
            if !IsObject(targetDest) || targetDest.Length != 2
                throw Error("Destination must be a sequence of length 2", -1)
            if !IsObject(targetSource) || !(targetSource.Length = 2 || targetSource.Length = 4)
                throw Error("Source must be a sequence of length 2 or 4", -1)

            sourceLeft := targetSource[1]
            sourceTop := targetSource[2]
            if targetSource.Length = 4 {
                sourceRight := targetSource[3]
                sourceBottom := targetSource[4]
            } else {
                sourceRight := image.Width
                sourceBottom := image.Height
            }
            if sourceLeft < 0 || sourceTop < 0 || sourceRight < 0 || sourceBottom < 0
                throw Error("Source must be non-negative", -1)

            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_alpha_composite_rgba_in_place",
                "Ptr", this.RequireHandle(),
                "Ptr", image.RequireHandle(),
                "Int", targetDest[1],
                "Int", targetDest[2],
                "Int", sourceLeft,
                "Int", sourceTop,
                "Int", sourceRight,
                "Int", sourceBottom,
                "Int"
            ))
            return this
        }

        Paste(source, box := unset, mask := unset) {
            if !IsSet(box)
                box := [0, 0]
            if IsObject(box) && box is Pillow.Image {
                if IsSet(mask)
                    throw Error("If using second argument as mask, third argument must be None", -1)
                mask := box
                box := [0, 0]
            }
            if !IsObject(box) || box.Length < 2
                throw Error("Pillow.Image.Paste expects box [left, top]", -1)

            if !(IsObject(source) && source is Pillow.Image) {
                if box.Length != 4
                    throw Error("Pillow.Image.Paste cannot determine region size; use 4-item box", -1)
                if IsSet(mask) && !(IsObject(mask) && mask is Pillow.Image)
                    throw Error("Pillow.Image.Paste mask expects a Pillow.Image", -1)
                color := this.PasteColorBuffer(source)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_paste_color",
                    "Ptr", this.RequireHandle(),
                    "Ptr", color,
                    "UPtr", color.Size,
                    "Int", box[1],
                    "Int", box[2],
                    "Int", box[3],
                    "Int", box[4],
                    "Ptr", IsSet(mask) ? mask.RequireHandle() : 0,
                    "Int"
                ))
                return this
            }

            if IsSet(mask) {
                if !(IsObject(mask) && mask is Pillow.Image)
                    throw Error("Pillow.Image.Paste mask expects a Pillow.Image", -1)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_paste_masked",
                    "Ptr", this.RequireHandle(),
                    "Ptr", source.RequireHandle(),
                    "Int", box[1],
                    "Int", box[2],
                    "Ptr", mask.RequireHandle(),
                    "Int"
                ))
            } else {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_paste",
                    "Ptr", this.RequireHandle(),
                    "Ptr", source.RequireHandle(),
                    "Int", box[1],
                    "Int", box[2],
                    "Int"
                ))
            }
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

        EffectSpread(distance) {
            if !(distance is Integer)
                throw Error("Pillow.Image.EffectSpread distance must be an integer", -1)
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_effect_spread",
                "Ptr", this.RequireHandle(),
                "Int", distance,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }

        Convert(modeName, matrixOrDither := unset, dither := unset) {
            targetMode := Pillow.ModeId(modeName)
            if IsSet(matrixOrDither) && IsObject(matrixOrDither) {
                if !(targetMode = 1 || targetMode = 3)
                    throw Error("Pillow.Image.Convert matrix illegal conversion", -1)
                expected := targetMode = 1 ? 4 : 12
                if matrixOrDither.Length != expected
                    throw Error("Pillow.Image.Convert matrix length must be " expected, -1)
                matrixBuffer := Buffer(expected * 8, 0)
                for index, value in matrixOrDither {
                    if !(value is Number)
                        throw Error("Pillow.Image.Convert matrix values must be numeric", -1)
                    NumPut("Double", value, matrixBuffer, (index - 1) * 8)
                }
                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_convert_matrix",
                    "Ptr", this.RequireHandle(),
                    "Int", targetMode,
                    "Ptr", matrixBuffer,
                    "UPtr", expected,
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return Pillow.WrapImageHandle(outHandle)
            }
            if targetMode = 5 {
                resolvedDither := IsSet(matrixOrDither) ? matrixOrDither : (IsSet(dither) ? dither : Pillow.Dither.FLOYDSTEINBERG)
                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_convert_mode_dither",
                    "Ptr", this.RequireHandle(),
                    "Int", targetMode,
                    "Int", resolvedDither,
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return Pillow.WrapImageHandle(outHandle)
            }
            if IsSet(matrixOrDither) || IsSet(dither)
                throw Error("Pillow.Image.Convert dither is currently supported only for mode 1", -1)
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_convert_mode",
                "Ptr", this.RequireHandle(),
                "Int", targetMode,
                "Ptr*", &outHandle,
                "Int"
            ))
            return Pillow.WrapImageHandle(outHandle)
        }
    }
}
