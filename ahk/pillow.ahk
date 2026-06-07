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

        static New(modeName, size) {
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
            return Pillow.Image(handle)
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
            if this.Mode = modeName
                return this.Copy()
            if this.Mode = "RGB" && modeName = "L" {
                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_rgb_to_l",
                    "Ptr", this.RequireHandle(),
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return Pillow.WrapImageHandle(outHandle)
            }
            throw Error("Pillow.Image.Convert does not support " this.Mode " to " modeName " yet", -1)
        }
    }
}
