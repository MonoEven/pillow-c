#Requires AutoHotkey v2.0

class Pillow {
    static DllPath := ""
    static DllHandle := 0

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

    class Palette {
        static WEB := 0
        static ADAPTIVE := 1
    }

    class Quantize {
        static MEDIANCUT := 0
        static MAXCOVERAGE := 1
        static FASTOCTREE := 2
        static LIBIMAGEQUANT := 3
    }

    class ImageMath {
        ; AHK identifiers are case-insensitive, so Eval()/UnsafeEval()
        ; also serve Pillow's eval()/unsafe_eval() aliases. Variables are
        ; passed as a Map of name -> Image or Number (Pillow kwargs).
        static UnsafeEval(expression, variables := unset) => Pillow.ImageMath.Eval(expression, variables)

        static Eval(expression, variables := unset) {
            if !(expression is String)
                throw Error("Pillow.ImageMath expects a string expression", -1)
            variableTable := Pillow.ImageMath.NormalizeVariables(IsSet(variables) ? variables : unset)
            tokens := Pillow.ImageMath.Tokenize(expression)
            compiled := Pillow.ImageMath.Compile(tokens, variableTable)
            if compiled.ImageSlots.Length = 0 {
                value := Pillow.ImageMath.EvalTreeScalar(compiled.Root, compiled.Constants)
                return value.IsFloat ? value.Value + 0.0 : Integer(value.Value)
            }
            handles := Buffer(compiled.SlotCount * A_PtrSize, 0)
            for slot, image in compiled.Images {
                if image.Width != compiled.Width || image.Height != compiled.Height
                    throw Error("images do not match", -1)
                NumPut("Ptr", image.RequireHandle(), handles, (slot - 1) * A_PtrSize)
            }
            constants := Buffer(compiled.SlotCount * 8, 0)
            for slot, value in compiled.Constants
                NumPut("Double", value, constants, (slot - 1) * 8)
            constantFloats := Buffer(compiled.SlotCount, 0)
            for slot, value in compiled.ConstantFloats
                NumPut("UChar", value ? 1 : 0, constantFloats, slot - 1)
            kinds := Buffer(compiled.SlotCount, 0)
            for slot, value in compiled.Kinds
                NumPut("UChar", value, kinds, slot - 1)
            program := Pillow.ImageMath.Linearize(compiled.Root, [])
            programBuf := Pillow.ImageMath.ProgramBuffer(program)
            outHandle := 0
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_math_rpn",
                "Ptr", handles,
                "Ptr", constants,
                "Ptr", constantFloats,
                "Ptr", kinds,
                "UPtr", compiled.SlotCount,
                "Ptr", programBuf,
                "UPtr", programBuf.Size,
                "Ptr*", &outHandle,
                "Int"
            )
            if status = -3 && compiled.FloatBitwiseOperator
                throw TypeError("bad operand type for '" compiled.FloatBitwiseOperator "'", -1)
            Pillow.CheckStatus(status)
            return compiled.FirstImage.WrapDerivedHandle(outHandle)
        }

        static Tokenize(expression) {
            tokens := []
            pos := 1
            length := StrLen(expression)
            while pos <= length {
                char := SubStr(expression, pos, 1)
                if char = " " || char = "`t" || char = "`r" || char = "`n" {
                    pos++
                    continue
                }
                if RegExMatch(SubStr(expression, pos), "^(\d+\.\d*|\.\d+|\d+)", &m) {
                    value := m[1]
                    if RegExMatch(SubStr(expression, pos + StrLen(m[1])), "^\w") || RegExMatch(SubStr(expression, pos + StrLen(m[1])), "^\.\d")
                        throw Error("SyntaxError: invalid syntax", -1)
                    tokens.Push({ Kind: "Number", Value: value + 0.0, Int: !InStr(value, ".") })
                    pos += StrLen(m[1])
                    continue
                }
                if RegExMatch(SubStr(expression, pos), "^[A-Za-z_]\w*", &m) {
                    tokens.Push({ Kind: "Name", Value: m[0] })
                    pos += StrLen(m[0])
                    continue
                }
                two := SubStr(expression, pos, 2)
                if two = "<<" || two = ">>" || two = "==" || two = "!=" || two = "<=" || two = ">=" {
                    tokens.Push({ Kind: "Operator", Value: two })
                    pos += 2
                    continue
                }
                if InStr("+-*/%&|^~<>()", char) || char = "," {
                    tokens.Push({ Kind: char = "," ? "Comma" : (char = "(" || char = ")") ? "Paren" : "Operator", Value: char })
                    pos++
                    continue
                }
                if char = "'" {
                    end := InStr(expression, "'", false, pos + 1)
                    if !end
                        throw Error("unbalanced parentheses", -1)
                    tokens.Push({ Kind: "String", Value: SubStr(expression, pos + 1, end - pos - 1) })
                    pos := end + 1
                    continue
                }
                throw Error("SyntaxError: invalid syntax", -1)
            }
            return tokens
        }

        static NormalizeVariables(variables := unset) {
            if !IsSet(variables)
                return Map()
            if variables is Map
                return variables
            throw Error("Pillow.ImageMath variables must be a Map of name to Image or Number", -1)
        }

        static Compile(tokens, variables) {
            imageSlots := []
            images := Map()
            constants := Map()
            kinds := Map()
            constantFloats := Map()
            slotCount := 0
            firstImage := 0
            width := 0
            height := 0
            floatBitwiseOperator := ""

            precedence := Map(
                "==", 1, "!=", 1, "<", 1, "<=", 1, ">", 1, ">=", 1,
                "|", 2, "^", 3, "&", 4, "<<", 5, ">>", 5,
                "+", 6, "-", 6, "*", 7, "/", 7, "%", 7)
            binaryOpcodes := Map("+", 2, "-", 3, "*", 4, "/", 5, "%", 6, "&", 7, "|", 8, "^", 9,
                "<<", 10, ">>", 11, "==", 12, "!=", 13, "<", 14, "<=", 15, ">", 16, ">=", 17)

            output := []
            ops := []
            for index, token in tokens {
                if token.Kind = "Number" {
                    output.Push({ Kind: "Leaf", Float: !token.Int, Slot: Pillow.ImageMath.AddConstant(token.Value, !token.Int, &constants, &kinds, &slotCount, &constantFloats) })
                    continue
                }
                if token.Kind = "String" {
                    output.Push({ Kind: "ModeStr", Value: token.Value })
                    continue
                }
                if token.Kind = "Name" {
                    if token.Value = "min" || token.Value = "max" || token.Value = "abs"
                        || token.Value = "float" || token.Value = "int" || token.Value = "convert" {
                        ops.Push({ Kind: "Function", Name: token.Value })
                        continue
                    }
                    if !variables.Has(token.Value)
                        throw Error("'" token.Value "' not allowed", -1)
                    value := variables[token.Value]
                    if IsObject(value) && value is Pillow.Image {
                        if !(value.Mode = "L" || value.Mode = "I" || value.Mode = "F")
                            throw Error("unsupported mode: " value.Mode, -1)
                        slot := Pillow.ImageMath.AddImage(value, &images, &kinds, &slotCount, &imageSlots, &firstImage, &width, &height)
                        output.Push({ Kind: "Leaf", Float: value.Mode = "F", Slot: slot })
                        continue
                    }
                    if value is Number {
                        output.Push({ Kind: "Leaf", Float: value is Float, Slot: Pillow.ImageMath.AddConstant(value + 0.0, value is Float, &constants, &kinds, &slotCount, &constantFloats) })
                        continue
                    }
                    throw Error("'" token.Value "' not allowed", -1)
                }
                if token.Kind = "Paren" && token.Value = "(" {
                    ops.Push({ Kind: "Open" })
                    continue
                }
                if token.Kind = "Paren" && token.Value = ")" {
                    while ops.Length > 0 && (ops[ops.Length].Kind = "Binary" || ops[ops.Length].Kind = "Unary") {
                        top := ops.Pop()
                        if top.Kind = "Binary" {
                            if !Pillow.ImageMath.EmitBinary(top.Name, output, &floatBitwiseOperator)
                                throw TypeError("bad operand type for '" Pillow.ImageMath.OperatorName(top.Name) "'", -1)
                        } else {
                            Pillow.ImageMath.EmitUnary(top.Name, output)
                        }
                    }
                    if ops.Length = 0 || ops[ops.Length].Kind != "Open"
                        throw Error("unbalanced parentheses", -1)
                    ops.Pop()
                    if ops.Length = 0 || ops[ops.Length].Kind != "Function"
                        throw Error("unbalanced parentheses", -1)
                    last := ops.Pop()
                    Pillow.ImageMath.EmitCall(last.Name, output)
                    continue
                }
                if token.Kind = "Comma" {
                    while ops.Length > 0 && (ops[ops.Length].Kind = "Binary" || ops[ops.Length].Kind = "Unary") {
                        top := ops.Pop()
                        if top.Kind = "Binary" {
                            if !Pillow.ImageMath.EmitBinary(top.Name, output, &floatBitwiseOperator)
                                throw TypeError("bad operand type for '" Pillow.ImageMath.OperatorName(top.Name) "'", -1)
                        } else {
                            Pillow.ImageMath.EmitUnary(top.Name, output)
                        }
                    }
                    continue
                }
                if token.Kind = "Operator" {
                    op := token.Value
                    if op = "-" && (index = 1
                        || tokens[index - 1].Kind = "Operator"
                        || (tokens[index - 1].Kind = "Paren" && tokens[index - 1].Value = "(")
                        || tokens[index - 1].Kind = "Comma") {
                        ops.Push({ Kind: "Unary", Name: "u-" })
                        continue
                    }
                    if op = "~" {
                        ops.Push({ Kind: "Unary", Name: "~" })
                        continue
                    }
                    if !precedence.Has(op)
                        throw Error("SyntaxError: invalid syntax", -1)
                    current := precedence[op]
                    while ops.Length > 0 && ops[ops.Length].Kind = "Unary" {
                        Pillow.ImageMath.EmitUnary(ops.Pop().Name, output)
                    }
                    while ops.Length > 0 && ops[ops.Length].Kind = "Binary"
                        && precedence[ops[ops.Length].Name] >= current {
                        top := ops.Pop()
                        if !Pillow.ImageMath.EmitBinary(top.Name, output, &floatBitwiseOperator)
                            throw TypeError("bad operand type for '" Pillow.ImageMath.OperatorName(top.Name) "'", -1)
                    }
                    ops.Push({ Kind: "Binary", Name: op })
                    continue
                }
                throw Error("SyntaxError: invalid syntax", -1)
            }
            while ops.Length > 0 {
                last := ops.Pop()
                if last.Kind = "Binary" {
                    if !Pillow.ImageMath.EmitBinary(last.Name, output, &floatBitwiseOperator)
                        throw TypeError("bad operand type for '" Pillow.ImageMath.OperatorName(last.Name) "'", -1)
                } else if last.Kind = "Unary" {
                    Pillow.ImageMath.EmitUnary(last.Name, output)
                } else if last.Kind = "Function" {
                    Pillow.ImageMath.EmitCall(last.Name, output)
                } else {
                    throw Error("unbalanced parentheses", -1)
                }
            }
            if output.Length != 1
                throw Error("SyntaxError: invalid syntax", -1)
            root := output[1]
            if !IsObject(root)
                throw Error("SyntaxError: invalid syntax", -1)
            return {
                Root: root,
                Images: images,
                Constants: constants,
                ConstantFloats: constantFloats,
                Kinds: kinds,
                SlotCount: slotCount,
                ImageSlots: imageSlots,
                FirstImage: firstImage,
                Width: width,
                Height: height,
                FloatBitwiseOperator: floatBitwiseOperator,
            }
        }

        static AddConstant(value, isFloat, &constants, &kinds, &slotCount, &constantFloats) {
            slotCount++
            kinds[slotCount] := 1
            constants[slotCount] := value
            constantFloats[slotCount] := isFloat ? 1 : 0
            return slotCount
        }

        static AddImage(image, &images, &kinds, &slotCount, &imageSlots, &firstImage, &width, &height) {
            slotCount++
            kinds[slotCount] := 0
            images[slotCount] := image
            imageSlots.Push(slotCount)
            if !firstImage {
                firstImage := image
                width := image.Width
                height := image.Height
            }
            return slotCount
        }

        static OperatorName(op) {
            names := Map("&", "and", "|", "or", "^", "xor", "<<", "lshift", ">>", "rshift")
            return names.Has(op) ? names[op] : op
        }

        static EmitBinary(name, output, &floatBitwiseOperator) {
            if output.Length < 2
                throw Error("SyntaxError: invalid syntax", -1)
            right := output.Pop()
            left := output.Pop()
            if left.Kind = "ModeStr" || right.Kind = "ModeStr"
                throw Error("SyntaxError: invalid syntax", -1)
            floatResult := left.Float || right.Float
            if InStr("&|^<<>>", name) && floatResult {
                floatBitwiseOperator := Pillow.ImageMath.OperatorName(name)
                return false
            }
            opcodes := Map("+", 2, "-", 3, "*", 4, "/", 5, "%", 6, "&", 7, "|", 8, "^", 9,
                "<<", 10, ">>", 11, "==", 12, "!=", 13, "<", 14, "<=", 15, ">", 16, ">=", 17)
            output.Push({ Kind: "Binary", Opcode: opcodes[name], Lhs: left, Rhs: right, Float: floatResult })
            return true
        }

        static EmitUnary(name, output) {
            if output.Length < 1
                throw Error("SyntaxError: invalid syntax", -1)
            arg := output.Pop()
            if arg.Kind = "ModeStr"
                throw Error("SyntaxError: invalid syntax", -1)
            if name = "u-" {
                output.Push({ Kind: "Unary", Opcode: 18, Child: arg, Float: arg.Float })
            } else {
                if arg.Float
                    throw TypeError("bad operand type for 'invert'", -1)
                output.Push({ Kind: "Unary", Opcode: 19, Child: arg, Float: false })
            }
        }

        static EmitCall(name, output) {
            if name = "abs" || name = "float" || name = "int" {
                if output.Length < 1
                    throw Error("unbalanced parentheses", -1)
                arg := output.Pop()
                if arg.Kind = "ModeStr"
                    throw Error("SyntaxError: invalid syntax", -1)
                if name = "abs" {
                    output.Push({ Kind: "Unary", Opcode: 20, Child: arg, Float: arg.Float })
                } else if name = "float" {
                    output.Push({ Kind: "Unary", Opcode: 23, Child: arg, Float: true })
                } else {
                    output.Push({ Kind: "Unary", Opcode: 24, Child: arg, Float: false })
                }
                return
            }
            if name = "min" || name = "max" {
                if output.Length < 2
                    throw Error("unbalanced parentheses", -1)
                right := output.Pop()
                left := output.Pop()
                if left.Kind = "ModeStr" || right.Kind = "ModeStr"
                    throw Error("SyntaxError: invalid syntax", -1)
                output.Push({
                    Kind: "Binary",
                    Opcode: name = "min" ? 21 : 22,
                    Lhs: left,
                    Rhs: right,
                    Float: left.Float || right.Float,
                })
                return
            }
            if name = "convert" {
                if output.Length < 2
                    throw Error("unbalanced parentheses", -1)
                modeNode := output.Pop()
                arg := output.Pop()
                if modeNode.Kind != "ModeStr"
                    throw Error("SyntaxError: invalid syntax", -1)
                target := StrUpper(modeNode.Value)
                if target = "F" {
                    output.Push({ Kind: "Convert", Mode: 9, Child: arg, Float: true })
                } else if target = "I" {
                    output.Push({ Kind: "Convert", Mode: 8, Child: arg, Float: false })
                } else if target = "L" {
                    output.Push({ Kind: "Convert", Mode: 1, Child: arg, Float: false })
                } else {
                    throw Error("unsupported mode: " target, -1)
                }
                return
            }
            throw Error("'" name "' not allowed", -1)
        }

        static Linearize(node, program) {
            switch node.Kind {
                case "Leaf":
                    program.Push(1, node.Slot)
                case "Unary":
                    Pillow.ImageMath.Linearize(node.Child, program)
                    program.Push(node.Opcode)
                case "Binary":
                    Pillow.ImageMath.Linearize(node.Lhs, program)
                    Pillow.ImageMath.Linearize(node.Rhs, program)
                    program.Push(node.Opcode)
                case "Convert":
                    Pillow.ImageMath.Linearize(node.Child, program)
                    program.Push(25, node.Mode)
                default:
                    throw Error("SyntaxError: invalid syntax", -1)
            }
            return program
        }

        static ProgramBuffer(program) {
            buf := Buffer(program.Length, 0)
            for index, value in program
                NumPut("UChar", value, buf, index - 1)
            return buf
        }

        static EvalTreeScalar(node, constants) {
            switch node.Kind {
                case "Leaf":
                    return { IsFloat: node.Float, Value: constants[node.Slot] }
                case "Unary":
                    arg := Pillow.ImageMath.EvalTreeScalar(node.Child, constants)
                    if node.Opcode = 18
                        return { IsFloat: arg.IsFloat, Value: -arg.Value }
                    if node.Opcode = 19
                        return { IsFloat: false, Value: ~Integer(arg.Value) }
                    if node.Opcode = 20
                        return { IsFloat: arg.IsFloat, Value: Abs(arg.Value) }
                    if node.Opcode = 23
                        return { IsFloat: true, Value: arg.Value + 0.0 }
                    return { IsFloat: false, Value: Integer(arg.Value) }
                case "Binary":
                    right := Pillow.ImageMath.EvalTreeScalar(node.Rhs, constants)
                    left := Pillow.ImageMath.EvalTreeScalar(node.Lhs, constants)
                    return Pillow.ImageMath.ScalarBinary(node.Opcode, left, right)
                case "Convert":
                    arg := Pillow.ImageMath.EvalTreeScalar(node.Child, constants)
                    if node.Mode = 9
                        return { IsFloat: true, Value: arg.Value + 0.0 }
                    return { IsFloat: false, Value: Integer(arg.Value) }
                default:
                    throw Error("SyntaxError: invalid syntax", -1)
            }
        }

        static ScalarBinary(op, left, right) {
            if left.IsFloat || right.IsFloat {
                a := left.Value + 0.0
                b := right.Value + 0.0
                switch op {
                    case 2: return { IsFloat: true, Value: a + b }
                    case 3: return { IsFloat: true, Value: a - b }
                    case 4: return { IsFloat: true, Value: a * b }
                    case 5: return { IsFloat: true, Value: a / b }
                    case 21: return { IsFloat: true, Value: Min(a, b) }
                    case 22: return { IsFloat: true, Value: Max(a, b) }
                }
                return { IsFloat: true, Value: 0.0 }
            }
            a := Integer(left.Value)
            b := Integer(right.Value)
            switch op {
                case 2: return { IsFloat: false, Value: a + b }
                case 3: return { IsFloat: false, Value: a - b }
                case 4: return { IsFloat: false, Value: a * b }
                case 5: return { IsFloat: false, Value: a // b }
                case 6: return { IsFloat: false, Value: Mod(a, b) }
                case 7: return { IsFloat: false, Value: a & b }
                case 8: return { IsFloat: false, Value: a | b }
                case 9: return { IsFloat: false, Value: a ^ b }
                case 10: return { IsFloat: false, Value: a << b }
                case 11: return { IsFloat: false, Value: a >> b }
                case 12: return { IsFloat: false, Value: a = b ? 1 : 0 }
                case 13: return { IsFloat: false, Value: a != b ? 1 : 0 }
                case 14: return { IsFloat: false, Value: a < b ? 1 : 0 }
                case 15: return { IsFloat: false, Value: a <= b ? 1 : 0 }
                case 16: return { IsFloat: false, Value: a > b ? 1 : 0 }
                case 17: return { IsFloat: false, Value: a >= b ? 1 : 0 }
                case 21: return { IsFloat: false, Value: Min(a, b) }
                case 22: return { IsFloat: false, Value: Max(a, b) }
            }
            return { IsFloat: false, Value: 0 }
        }
    }
    class ImageQt {
        ; API-QTTK-001: explicit documented boundary — the AHK runtime
        ; ships no Qt binding, so Pillow 11.3.0's ImageQt module surface
        ; (ImageQt/fromqimage/toqimage/toqpixmap) fails loudly with the
        ; classic Pillow message.
        static ImageQt(im) => Pillow.ImageQt.RequireQt()
        static ToQImage(im) => Pillow.ImageQt.RequireQt()
        static ToQPixmap(im) => Pillow.ImageQt.RequireQt()
        static FromQImage(qim) => Pillow.ImageQt.RequireQt()

        static RequireQt() {
            throw Error("Qt bindings are not installed", -1)
        }
    }

    class ImageTk {
        ; API-QTTK-001: explicit documented boundary — the AHK runtime
        ; ships no Tk interpreter, so Pillow 11.3.0's ImageTk module
        ; surface (PhotoImage/BitmapImage) fails loudly with Pillow's
        ; no-root RuntimeError.
        static PhotoImage(image := unset, args*) => Pillow.ImageTk.RequireTk()
        static BitmapImage(image := unset, args*) => Pillow.ImageTk.RequireTk()

        static RequireTk() {
            throw Error("Too early to create image: no default root window", -1)
        }
    }

    class ImageFile {
        ; API-FILE-001: Pillow 11.3.0's PIL.ImageFile module surface. The
        ; native ABI decodes/encodes whole files, so the incremental
        ; feed/plugin protocol (ImageFile/Parser/StubImageFile/StubHandler/
        ; PyCodec/PyDecoder/PyEncoder) is an explicit documented boundary;
        ; MAXBLOCK/SAFEBLOCK/ERRORS, the LOAD_TRUNCATED_IMAGES default, and
        ; the plain PyCodecState object are covered exactly.
        static MAXBLOCK := 65536
        static SAFEBLOCK := 1048576

        static ERRORS := Map(
            -1, "image buffer overrun error",
            -2, "decoding error",
            -3, "unknown error",
            -8, "bad configuration",
            -9, "out of memory error"
        )

        static LoadTruncatedImages {
            get => Pillow.ImageFile._LoadTruncated
            set {
                if value
                    throw Error("Pillow.ImageFile.LOAD_TRUNCATED_IMAGES truncated-load tolerance is not supported: native decoders require complete files (the Pillow default is False)", -1)
                Pillow.ImageFile._LoadTruncated := false
            }
        }
        static _LoadTruncated := false

        ; Pillow's ImageFile.ImageFile(fp, name) base object and the Parser
        ; incremental feed are documented boundaries: the AHK runtime has no
        ; file-object/incremental protocol, and the native ABI decodes whole
        ; files. Construction fails loudly instead of building inert shells.
        __New(args*) => Pillow.ImageFile.RequireIncremental()

        class ImageFile {
            __New(args*) => Pillow.ImageFile.RequireIncremental()
        }

        class Parser {
            ; BEHAV-PARSER-001: Pillow 11.3.0's ImageFile.Parser feed/close
            ; consumer. The facade accumulates the fed buffers and reopens
            ; the whole stream through the eager Open at close() — Pillow's
            ; own fallback path for non-incremental formats (the
            ; decoder-based incremental decode stays a documented child;
            ; the observable feed/close/error surface matches).
            __New() {
                this.Data := unset
                this.Image := unset
                this.Finished := false
            }

            Reset() {
                ; Pillow asserts the parser has not collected data yet.
                if this.HasOwnProp("Data")
                    throw Error("cannot reuse parsers", -1)
            }

            Feed(data) {
                if this.Finished
                    return
                if !this.HasOwnProp("Data")
                    this.Data := data
                else
                    this.Data := Pillow.ImageFile.ParserConcat(this.Data, data)
                if !this.HasOwnProp("Image") {
                    ; Pillow attempts an open on every feed and swallows the
                    ; OSError-family failures (UnidentifiedImageError is an
                    ; OSError subclass) until the data suffices
                    path := Pillow.ImageFile.ParserWriteTemp(this.Data)
                    try {
                        this.Image := Pillow.Image.Open(path)
                    } catch {
                        ; not enough data / not yet identifiable
                    } finally {
                        Pillow.ImageFile.ParserDeleteTemp(path)
                    }
                }
            }

            Close() {
                if !this.HasOwnProp("Image")
                    throw Error("cannot parse this image", -1)
                if this.HasOwnProp("Data") {
                    ; Pillow reopens the accumulated stream after a
                    ; successful earlier open; identification/decode
                    ; failures surface
                    path := Pillow.ImageFile.ParserWriteTemp(this.Data)
                    try {
                        this.Image := Pillow.Image.Open(path)
                    } finally {
                        Pillow.ImageFile.ParserDeleteTemp(path)
                    }
                }
                this.Data := unset
                this.Finished := true
                return this.Image
            }

            __Enter() {
                return this
            }

            __Exit(args*) {
                this.Close()
            }
        }

        static ParserConcat(a, b) {
            out := Buffer(a.Size + b.Size, 0)
            DllCall("msvcrt\memcpy", "Ptr", out, "Ptr", a, "UPtr", a.Size, "CDecl Ptr")
            DllCall("msvcrt\memcpy", "Ptr", out.Ptr + a.Size, "Ptr", b, "UPtr", b.Size, "CDecl Ptr")
            return out
        }

        static ParserWriteTemp(data) {
            ext := Pillow.ImageFile.ParserProbeExtension(data)
            path := A_Temp "\pillow-c-parser-" DllCall("GetTickCount64", "Int64") "-" Random(100000, 999999) ext
            file := FileOpen(path, "w")
            try {
                if data.Size > 0
                    file.RawWrite(data, data.Size)
            } finally {
                file.Close()
            }
            return path
        }

        static ParserProbeExtension(data) {
            ; Pillow's Parser feeds a BytesIO to Image.open (content-sniffed);
            ; the facade routes by extension, so the temp file gets the
            ; extension matching the magic bytes. Unrecognized data keeps a
            ; benign extension — the open fails and the close raises Pillow's
            ; "cannot parse this image" either way.
            if data.Size >= 8 {
                b0 := NumGet(data, 0, "UChar")
                b1 := NumGet(data, 1, "UChar")
                b2 := NumGet(data, 2, "UChar")
                b3 := NumGet(data, 3, "UChar")
                if b0 = 0x89 && b1 = 0x50 && b2 = 0x4E && b3 = 0x47
                    return ".png"
                if b0 = 0xFF && b1 = 0xD8
                    return ".jpg"
                if b0 = 0x47 && b1 = 0x49 && b2 = 0x46 && b3 = 0x38
                    return ".gif"
                if b0 = 0x49 && b1 = 0x49 && b2 = 0x2A
                    return ".tif"
                if b0 = 0x4D && b1 = 0x4D && b2 = 0x00 && b3 = 0x2A
                    return ".tif"
                if b0 = 0x42 && b1 = 0x4D
                    return ".bmp"
                if b0 = 0x00 && b1 = 0x00 && b2 = 0x01 && b3 = 0x00
                    return ".ico"
                if b0 = 0x00 && b1 = 0x00 && b2 = 0x02 && b3 = 0x00
                    return ".cur"
                if b0 = 0x38 && b1 = 0x42 && b2 = 0x50 && b3 = 0x53
                    return ".psd"
                if b0 = 0x44 && b1 = 0x44 && b2 = 0x53 && b3 = 0x20
                    return ".dds"
                if b0 = 0x69 && b1 = 0x63 && b2 = 0x6E && b3 = 0x73
                    return ".icns"
                if b0 = 0x01 && b1 = 0xDA
                    return ".sgi"
                if b0 = 0x46 && b1 = 0x54 && b2 = 0x45 && b3 = 0x58
                    return ".ftc"
                if b0 = 0x59 && b1 = 0xA6 && b2 = 0x6A && b3 = 0x95
                    return ".ras"
                if b0 = 0x53 && b1 = 0x49 && b2 = 0x4D && b3 = 0x50
                    return ".fit"
                if b0 = 0x2F && b1 = 0x2A && b2 = 0x20
                    return ".xpm"
                if b0 = 0xD0 && b1 = 0xCF && b2 = 0x11 && b3 = 0xE0
                    return ".mic"
                if b0 = 0xD7 && b1 = 0xCD && b2 = 0xC6 && b3 = 0x9A
                    return ".wmf"
                if b0 = 0x00 && b1 = 0x00 && b2 = 0x01 && b3 = 0xB3
                    return ".mpg"
                if b0 = 0x71 && b1 = 0x6F && b2 = 0x69 && b3 = 0x66
                    return ".qoi"
                if b0 = 0x0A && b1 <= 5
                    return ".pcx"
                if b0 = 0x50 && b1 >= 0x31 && b1 <= 0x36
                    return ".ppm"
                if NumGet(data, 4, "UChar") = 0x11 && NumGet(data, 5, "UChar") = 0xAF
                    return ".fli"
                if NumGet(data, 4, "UChar") = 0x12 && NumGet(data, 5, "UChar") = 0xAF
                    return ".flc"
                if b0 = 0x25 && b1 = 0x21
                    return ".eps"
                if b0 = 0xC5 && b1 = 0xD0 && b2 = 0xD3 && b3 = 0xC6
                    return ".eps"
            }
            return ".png"
        }

        static ParserDeleteTemp(path) {
            FileDelete(path)
        }

        class StubImageFile {
            ; Pillow 11.3.0 raises this exact TypeError (abstract _load/_open).
            __New(args*) {
                throw Error("Can't instantiate abstract class StubImageFile with abstract methods _load, _open", -1)
            }
        }

        class StubHandler {
            ; Pillow 11.3.0 raises this exact TypeError (abstract load).
            __New(args*) {
                throw Error("Can't instantiate abstract class StubHandler with abstract method load", -1)
            }
        }

        class PyCodec {
            ; Pillow 11.3.0: PyCodec.__init__() requires 'mode'.
            __New(args*) => Pillow.ImageFile.RequirePyCodecMode()
        }

        class PyDecoder {
            __New(args*) => Pillow.ImageFile.RequirePyCodecMode()
        }

        class PyEncoder {
            __New(args*) => Pillow.ImageFile.RequirePyCodecMode()
        }

        class PyCodecState {
            ; Pillow 11.3.0's plain codec state object; AHK case-insensitivity
            ; serves xsize/ysize/xoff/yoff.
            Xsize := 0
            Ysize := 0
            Xoff := 0
            Yoff := 0
        }

        static ReportOSError(code) {
            ; Serves Pillow 11.3.0's raise_oserror, which Pillow deprecates
            ; (removal in Pillow 12); it only translates codec decode()
            ; return codes, which the native ABI surfaces directly. AHK
            ; identifiers beginning with "Raise" lex as the raise keyword at
            ; call sites, and a parameter named "error" would shadow the
            ; Error class (AHK names are case-insensitive), so the facade
            ; member is ReportOSError(code).
            throw Error("raise_oserror is a deprecated Pillow helper; codec errors surface directly from the native ABI", -1)
        }

        static RequireIncremental() {
            throw Error("Pillow.ImageFile incremental decoding is not supported: the native ABI decodes whole files", -1)
        }

        static RequirePyCodecMode() {
            throw Error("PyCodec.__init__() missing 1 required positional argument: 'mode'", -1)
        }
    }

    class ImagePalette {
        ; API-PALETTE-001: Pillow 11.3.0's PIL.ImagePalette module surface.
        ; AHK case-insensitivity serves ImagePalette/raw/negative/random/
        ; sepia/wedge/load/make_linear_lut/make_gamma_lut. The palette
        ; sequence is stored as an AHK Array (Pillow accepts any int
        ; sequence); Colors uses comma-joined string keys because AHK Map
        ; keys are identity-compared (no tuple keys). load() and the
        ; GimpPaletteFile/GimpGradientFile/PaletteFile parser classes are
        ; documented boundaries (fail-loud), and random() shares Pillow's
        ; shape but not its Mersenne-Twister stream (documented boundary).
        Mode := "RGB"
        Rawmode := ""
        Dirty := 0

        __New(mode := "RGB", palette := unset) {
            this.Mode := mode
            this.Rawmode := ""
            this._Palette := IsSet(palette) && IsObject(palette) && palette.Length > 0 ? palette : []
            this._Colors := 0
            this.Dirty := 0
        }

        ; Pillow's ImagePalette.ImagePalette class name is served by this
        ; class itself (Pillow.ImagePalette(...) constructs an instance).
        Palette {
            get => this._Palette
            set {
                this._Colors := 0
                this._Palette := value
            }
        }

        Colors {
            get {
                if !IsObject(this._Colors) {
                    modeLen := StrLen(this.Mode)
                    colorMap := Map()
                    loop this._Palette.Length // modeLen {
                        colorIndex := A_Index - 1
                        key := ""
                        loop modeLen
                            key .= (A_Index > 1 ? "," : "") this._Palette[colorIndex * modeLen + A_Index]
                        if !colorMap.Has(key)
                            colorMap[key] := colorIndex
                    }
                    this._Colors := colorMap
                }
                return this._Colors
            }
            set {
                this._Colors := value
            }
        }

        Copy() {
            new := Pillow.ImagePalette()
            new.Mode := this.Mode
            new.Rawmode := this.Rawmode
            new._Palette := this._Palette.Clone()
            new._Colors := 0
            new.Dirty := this.Dirty
            return new
        }

        GetData() {
            if this.Rawmode
                return [this.Rawmode, this._Palette]
            return [this.Mode, this.ToBytes()]
        }

        ToBytes() {
            if this.Rawmode
                throw Error("palette contains raw palette data", -1)
            out := Buffer(this._Palette.Length, 0)
            for index, value in this._Palette
                NumPut("UChar", value, out, index - 1)
            return out
        }

        Tostring() => this.ToBytes()

        GetColor(color, image := unset) {
            if this.Rawmode
                throw Error("palette contains raw palette data", -1)
            if !IsObject(color) {
                spec := color is String ? "'" color "'" : color
                throw Error("unknown color specifier: " spec, -1)
            }
            if this.Mode = "RGB" {
                if color.Length = 4 {
                    if color[4] != 255
                        throw Error("cannot add non-opaque RGBA color to RGB palette", -1)
                    color := [color[1], color[2], color[3]]
                }
            } else if this.Mode = "RGBA" {
                if color.Length = 3
                    color := [color[1], color[2], color[3], 255]
            }
            key := Pillow.ImagePalette.ColorKey(color)
            colorMap := this.Colors
            if colorMap.Has(key)
                return colorMap[key]
            ; allocate a new color slot (Pillow hard-codes //3 regardless
            ; of mode — mirrored exactly)
            index := this._Palette.Length // 3
            specials := []
            if IsSet(image) && IsObject(image) {
                if image.Info.Has("background")
                    specials.Push(image.Info["background"])
                if image.Info.Has("transparency")
                    specials.Push(image.Info["transparency"])
            }
            loop {
                isSpecial := false
                for specialIndex, special in specials {
                    if index = special {
                        isSpecial := true
                        break
                    }
                }
                if !isSpecial
                    break
                index += 1
            }
            if index >= 256 {
                if IsSet(image) && IsObject(image) {
                    histogram := image.Histogram()
                    loop histogram.Length {
                        i := histogram.Length - A_Index
                        if histogram[i + 1] = 0 {
                            taken := false
                            for specialIndex, special in specials {
                                if i = special {
                                    taken := true
                                    break
                                }
                            }
                            if !taken {
                                index := i
                                break
                            }
                        }
                    }
                }
                if index >= 256
                    throw Error("cannot allocate more than 256 colors", -1)
            }
            colorMap[key] := index
            if index * 3 < this._Palette.Length {
                ; replace the slot in place
                loop color.Length
                    this._Palette[index * 3 + A_Index] := color[A_Index]
            } else {
                ; append trailing channels
                for channelIndex, channelValue in color
                    this._Palette.Push(channelValue)
            }
            this.Dirty := 1
            return index
        }

        Save(fp) {
            if this.Rawmode
                throw Error("palette contains raw palette data", -1)
            if !(fp is String)
                throw Error("Pillow.ImagePalette.Save expects a file path string", -1)
            ; Pillow writes "# Palette\n", "# Mode: ...\n" and 256 indexed
            ; lines; Windows text mode turns "\n" into CRLF.
            text := "# Palette`r`n# Mode: " this.Mode "`r`n"
            loop 256 {
                i := A_Index - 1
                text .= i
                loop StrLen(this.Mode) {
                    valueIndex := i * StrLen(this.Mode) + A_Index
                    text .= " " (valueIndex <= this._Palette.Length ? this._Palette[valueIndex] : 0)
                }
                text .= "`r`n"
            }
            FileAppend text, fp, "UTF-8"
        }

        static ColorKey(color) {
            key := ""
            for index, value in color
                key .= (A_Index > 1 ? "," : "") value
            return key
        }

        static Raw(rawmode, data) {
            palette := Pillow.ImagePalette()
            palette.Rawmode := rawmode
            palette.Palette := data
            palette.Dirty := 1
            return palette
        }

        static Negative(mode := "RGB") {
            count := 256 * StrLen(mode)
            palette := []
            loop count {
                v := count - A_Index
                palette.Push(v // StrLen(mode))
            }
            return Pillow.ImagePalette(mode, palette)
        }

        static Random(mode := "RGB") {
            ; Pillow 11.3.0 uses Python's Mersenne-Twister random stream;
            ; this runtime uses AHK's own RNG with the same range/shape
            ; (documented boundary).
            count := 256 * StrLen(mode)
            palette := []
            loop count
                palette.Push(Random(0, 255))
            return Pillow.ImagePalette(mode, palette)
        }

        static Sepia(white := "#fff0c0") {
            rgb := Pillow.ImageColor.GetRgb(white)
            bands := [
                Pillow.ImagePalette.MakeLinearLut(0, rgb[1]),
                Pillow.ImagePalette.MakeLinearLut(0, rgb[2]),
                Pillow.ImagePalette.MakeLinearLut(0, rgb[3]),
            ]
            palette := []
            loop 768 {
                i := A_Index - 1
                palette.Push(bands[Mod(i, 3) + 1][i // 3 + 1])
            }
            return Pillow.ImagePalette("RGB", palette)
        }

        static Wedge(mode := "RGB") {
            count := 256 * StrLen(mode)
            palette := []
            loop count
                palette.Push((A_Index - 1) // StrLen(mode))
            return Pillow.ImagePalette(mode, palette)
        }

        static Load(filename) {
            ; BEHAV-PALETTE-002: Pillow 11.3.0 ImagePalette.load tries
            ; GimpPaletteFile, GimpGradientFile, then PaletteFile; each
            ; parser's SyntaxError/ValueError falls through to the next,
            ; and a fully failed chain raises "cannot load palette". The
            ; gradient's non-RGB colour space OSError and its short-line
            ; IndexError escape load unwrapped.
            if !FileExist(filename)
                throw Error("[Errno 2] No such file or directory: '" filename "'", -1)
            content := FileRead(filename)
            try {
                return Pillow.ImagePalette.GimpPaletteParse(content)
            } catch Error as err {
                if !Pillow.ImagePalette.PaletteLoadCaught(err.Message)
                    throw
            }
            try {
                return Pillow.ImagePalette.GimpGradientParse(content)
            } catch Error as err {
                if !Pillow.ImagePalette.PaletteLoadCaught(err.Message)
                    throw
            }
            try {
                return Pillow.ImagePalette.PaletteParse(content)
            } catch Error as err {
                if !Pillow.ImagePalette.PaletteLoadCaught(err.Message)
                    throw
            }
            throw Error("cannot load palette", -1)
        }

        static PaletteLoadCaught(message) {
            return message = "not a GIMP palette file" || message = "bad palette file" || message = "bad palette entry"
                || message = "not a GIMP gradient file" || message = "Invalid value."
        }

        static PaletteTokens(line) {
            trimmed := Trim(line)
            collapsed := RegExReplace(trimmed, "\s+", " ")
            if collapsed = ""
                return []
            return StrSplit(collapsed, " ")
        }

        static GimpPaletteParse(content) {
            ; GimpPaletteFile._read with limit=True
            lines := StrSplit(content, "`n", "`r")
            if lines.Length = 0 || !(SubStr(lines[1], 1, 13) = "GIMP Palette")
                throw Error("not a GIMP palette file", -1)
            values := []
            i := 0
            for line in lines {
                if A_Index = 1
                    continue
                if i = 259
                    break
                i += 1
                if line = "" {
                    if A_Index < lines.Length
                        throw Error("bad palette entry", -1)
                    break
                }
                if RegExMatch(line, "^\w+:|^#")
                    continue
                if StrLen(line) > 100
                    throw Error("bad palette file", -1)
                tokens := Pillow.ImagePalette.PaletteTokens(line)
                if tokens.Length < 3
                    throw Error("bad palette entry", -1)
                values.Push(Pillow.ImagePalette.PaletteInt(tokens[1]))
                values.Push(Pillow.ImagePalette.PaletteInt(tokens[2]))
                values.Push(Pillow.ImagePalette.PaletteInt(tokens[3]))
                if values.Length = 768
                    break
            }
            out := Buffer(values.Length, 0)
            for index, value in values
                NumPut("UChar", value, out, index - 1)
            return [out, "RGB"]
        }

        static PaletteInt(text) {
            try {
                return Integer(text)
            } catch {
                throw Error("Invalid value.", -1)
            }
        }

        static GradientSegment(id, middle, pos) {
            ; GimpGradientFile segment functions
            if id = 0 {
                if pos <= middle {
                    if middle < 0.0000000001
                        return 0.0
                    return 0.5 * pos / middle
                }
                pos2 := pos - middle
                middle2 := 1.0 - middle
                if middle2 < 0.0000000001
                    return 1.0
                return 0.5 + 0.5 * pos2 / middle2
            }
            if id = 1
                return pos ** (Log(0.5) / Log(Max(middle, 0.0000000001)))
            linear := Pillow.ImagePalette.GradientSegment(0, middle, pos)
            if id = 2
                return (Sin(-1.5707963267948966 + 3.141592653589793 * linear) + 1.0) / 2.0
            if id = 3
                return Sqrt(1.0 - (linear - 1.0) ** 2)
            return 1.0 - Sqrt(1.0 - linear ** 2)
        }

        static GimpGradientParse(content) {
            ; GimpGradientFile + GradientFile.getpalette(256)
            lines := StrSplit(content, "`n", "`r")
            if lines.Length = 0 || !(SubStr(lines[1], 1, 14) = "GIMP Gradient")
                throw Error("not a GIMP gradient file", -1)
            line := lines[2]
            if SubStr(line, 1, 6) = "Name: "
                line := Trim(lines[3])
            count := Pillow.ImagePalette.PaletteInt(Trim(line))
            segments := []
            loop count {
                tokens := Pillow.ImagePalette.PaletteTokens(lines[3 + A_Index])
                if tokens.Length < 13
                    throw Error("list index out of range", -1)
                w := []
                loop 11
                    w.Push(Pillow.ImagePalette.PaletteFloat(tokens[A_Index]))
                segmentId := Pillow.ImagePalette.PaletteInt(tokens[12])
                cspace := Pillow.ImagePalette.PaletteInt(tokens[13])
                if cspace != 0
                    throw Error("cannot handle HSV colour space", -1)
                segments.Push([w[1], w[3], w[2], [w[4], w[5], w[6], w[7]], [w[8], w[9], w[10], w[11]], segmentId])
            }
            out := Buffer(1024, 0)
            ix := 1
            loop 256 {
                i := A_Index - 1
                x := i / 255
                while segments[ix][2] < x
                    ix += 1
                seg := segments[ix]
                x0 := seg[1]
                x1 := seg[2]
                xm := seg[3]
                rgb0 := seg[4]
                rgb1 := seg[5]
                segment := seg[6]
                w := x1 - x0
                scale := 0.0
                if w < 0.0000000001
                    scale := Pillow.ImagePalette.GradientSegment(segment, 0.5, 0.5)
                else
                    scale := Pillow.ImagePalette.GradientSegment(segment, (xm - x0) / w, (x - x0) / w)
                for band in [1, 2, 3, 4] {
                    value := Integer(255 * ((rgb1[band] - rgb0[band]) * scale + rgb0[band]) + 0.5)
                    NumPut("UChar", value, out, i * 4 + band - 1)
                }
            }
            return [out, "RGBA"]
        }

        static PaletteFloat(text) {
            try {
                return Float(text)
            } catch {
                throw Error("Invalid value.", -1)
            }
        }

        static PaletteParse(content) {
            ; PaletteFile (Teragon-style): 256 grayscale entries overridden
            ; by "index r g b" or "index value" lines
            out := Buffer(768, 0)
            loop 256 {
                base := (A_Index - 1) * 3
                NumPut("UChar", A_Index - 1, out, base)
                NumPut("UChar", A_Index - 1, out, base + 1)
                NumPut("UChar", A_Index - 1, out, base + 2)
            }
            paletteLines := StrSplit(content, "`n", "`r")
            for line in paletteLines {
                if line = "" {
                    if A_Index < paletteLines.Length
                        throw Error("Invalid value.", -1)
                    break
                }
                if SubStr(line, 1, 1) = "#"
                    continue
                if StrLen(line) > 100
                    throw Error("bad palette file", -1)
                tokens := Pillow.ImagePalette.PaletteTokens(line)
                i := 0
                r := 0
                g := 0
                b := 0
                if tokens.Length = 4 {
                    i := Pillow.ImagePalette.PaletteInt(tokens[1])
                    r := Pillow.ImagePalette.PaletteInt(tokens[2])
                    g := Pillow.ImagePalette.PaletteInt(tokens[3])
                    b := Pillow.ImagePalette.PaletteInt(tokens[4])
                } else if tokens.Length = 2 {
                    i := Pillow.ImagePalette.PaletteInt(tokens[1])
                    r := Pillow.ImagePalette.PaletteInt(tokens[2])
                    g := r
                    b := r
                } else
                    throw Error("Invalid value.", -1)
                if i >= 0 && i <= 255 {
                    if r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255
                        throw Error("int too big to convert", -1)
                    NumPut("UChar", r, out, i * 3)
                    NumPut("UChar", g, out, i * 3 + 1)
                    NumPut("UChar", b, out, i * 3 + 2)
                }
            }
            return [out, "RGB"]
        }

        static MakeLinearLut(black, white) {
            if black != 0
                throw Error("unavailable when black is non-zero", -1)
            lut := []
            loop 256
                lut.Push(Integer(Floor(white * (A_Index - 1) / 255)))
            return lut
        }

        static MakeGammaLut(exp) {
            lut := []
            loop 256 {
                i := A_Index - 1
                lut.Push(Integer(((i / 255.0) ** exp) * 255.0 + 0.5))
            }
            return lut
        }
    }

    class ImageTransform {
        ; API-TRANSFORMCLS-001: Pillow 11.3.0's PIL.ImageTransform module
        ; class objects. Each subclass only carries a method constant; the
        ; base Transform class stores data and routes transform() through
        ; the facade's Image.Transform seam. Pillow's module itself is not
        ; callable, so construction on the module class fails loudly.
        __New(args*) {
            throw Error("Pillow ImageTransform module is not callable; construct Pillow.ImageTransform.Transform subclasses", -1)
        }

        class Transform {
            __New(data) {
                this.Data := data
            }

            GetData() {
                ; Pillow's base Transform carries only a type annotation for
                ; method, so getdata() raises AttributeError; the subclasses
                ; set Method (AHK surfaces the same shape as a missing
                ; property on the base).
                return [this.Method, this.Data]
            }

            Transform(size, image, resample := unset, fillcolor := unset) {
                if IsSet(resample) && IsSet(fillcolor)
                    return image.Transform(size, this.Method, this.Data, resample, fillcolor)
                if IsSet(resample)
                    return image.Transform(size, this.Method, this.Data, resample)
                return image.Transform(size, this.Method, this.Data)
            }
        }

        class AffineTransform extends Pillow.ImageTransform.Transform {
            Method := Pillow.Transform.AFFINE
        }

        class ExtentTransform extends Pillow.ImageTransform.Transform {
            Method := Pillow.Transform.EXTENT
        }

        class PerspectiveTransform extends Pillow.ImageTransform.Transform {
            Method := Pillow.Transform.PERSPECTIVE
        }

        class QuadTransform extends Pillow.ImageTransform.Transform {
            Method := Pillow.Transform.QUAD
        }

        class MeshTransform extends Pillow.ImageTransform.Transform {
            Method := Pillow.Transform.MESH
        }
    }

    class ImagePath {
        ; AHK case-insensitivity serves Pillow's ImagePath.Path module.
        class Path {
            Points := []

            __New(xy) {
                if !IsObject(xy)
                    throw Error("Path expects a sequence of coordinates", -1)
                if xy.Length > 0 && IsObject(xy[1]) {
                    for index, pair in xy {
                        if !IsObject(pair) || pair.Length != 2
                            throw Error("Path coordinate pairs must be [x, y]", -1)
                        this.Points.Push(pair[1] + 0.0, pair[2] + 0.0)
                    }
                } else {
                    for index, value in xy {
                        if !(value is Number)
                            throw Error("Path coordinates must be numeric", -1)
                        this.Points.Push(value + 0.0)
                    }
                }
                if Mod(this.Points.Length, 2) != 0
                    throw Error("Path coordinate sequence must have an even length", -1)
            }

            Tolist(flat := true) {
                out := []
                loop this.Points.Length // 2
                    out.Push([this.Points[A_Index * 2 - 1], this.Points[A_Index * 2]])
                return out
            }

            GetBbox() {
                if this.Points.Length = 0
                    return [0.0, 0.0, 0.0, 0.0]
                minX := this.Points[1]
                minY := this.Points[2]
                maxX := minX
                maxY := minY
                loop this.Points.Length // 2 {
                    x := this.Points[A_Index * 2 - 1]
                    y := this.Points[A_Index * 2]
                    if x < minX
                        minX := x
                    if y < minY
                        minY := y
                    if x > maxX
                        maxX := x
                    if y > maxY
                        maxY := y
                }
                return [minX, minY, maxX, maxY]
            }

            Compact(distance) {
                ; Pillow 11.3.0's core.path only compacts curve segments;
                ; the simplified path object holds lines only, so compact
                ; is an in-place no-op that returns the converted count (0).
                return 0
            }

            Transform(matrix) {
                if !IsObject(matrix) || matrix.Length != 6
                    throw Error("transform() argument 1 must be sequence of length 6, not " (IsObject(matrix) ? matrix.Length : 0), -1)
                for index, value in matrix {
                    if !(value is Number)
                        throw Error("transform() matrix values must be numeric", -1)
                }
                loop this.Points.Length // 2 {
                    x := this.Points[A_Index * 2 - 1]
                    y := this.Points[A_Index * 2]
                    this.Points[A_Index * 2 - 1] := matrix[1] * x + matrix[2] * y + matrix[3]
                    this.Points[A_Index * 2] := matrix[4] * x + matrix[5] * y + matrix[6]
                }
            }

            Map(fn) {
                ; Pillow 11.3.0's path.map with a callable returns None
                ; without mutating (the ImagingTransformHandler form stays
                ; a documented boundary); 0 is this runtime's None analogue.
                return 0
            }
        }
    }

    class ImageGrab {
        ; AHK case-insensitivity serves grab()/grabclipboard().
        static GrabClipboard() {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_grab_clipboard",
                "Ptr*", &outHandle,
                "Int"
            ))
            return outHandle ? Pillow.WrapImageHandle(outHandle) : 0
        }

        static Grab(bbox := unset, includeLayeredWindows := false, allScreens := false) {
            left := 0
            top := 0
            right := 0
            bottom := 0
            if IsSet(bbox) {
                if !IsObject(bbox) || bbox.Length != 4
                    throw Error("Pillow.ImageGrab.grab bbox expects a 4-value rectangle", -1)
                left := bbox[1]
                top := bbox[2]
                right := bbox[3]
                bottom := bbox[4]
                if right < left
                    throw Error("Coordinate 'right' is less than 'left'", -1)
                if bottom < top
                    throw Error("Coordinate 'lower' is less than 'upper'", -1)
            } else if allScreens {
                left := DllCall("user32\GetSystemMetrics", "Int", 76, "Int")
                top := DllCall("user32\GetSystemMetrics", "Int", 77, "Int")
                right := left + DllCall("user32\GetSystemMetrics", "Int", 78, "Int")
                bottom := top + DllCall("user32\GetSystemMetrics", "Int", 79, "Int")
            } else {
                right := DllCall("user32\GetSystemMetrics", "Int", 0, "Int")
                bottom := DllCall("user32\GetSystemMetrics", "Int", 1, "Int")
            }
            outHandle := 0
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_grab",
                "Int", left,
                "Int", top,
                "Int", right,
                "Int", bottom,
                "Int", allScreens ? 1 : 0,
                "Int", includeLayeredWindows ? 1 : 0,
                "Ptr*", &outHandle,
                "Int"
            )
            if status = -3
                ; Pillow 11.3.0 raises OSError("screen grab failed") when
                ; the GDI capture fails (e.g. an off-screen bbox).
                throw Error("screen grab failed", -1)
            Pillow.CheckStatus(status)
            return Pillow.WrapImageHandle(outHandle)
        }
    }

    class ImageSequence {
        static AllFrames(im, fn := unset) {
            if im is Array {
                sequences := im
            } else {
                sequences := [im]
            }
            if IsSet(fn) && !(fn is Func)
                throw Error("Pillow.ImageSequence.AllFrames expects a callable function", -1)

            frames := []
            for image in sequences {
                if !(IsObject(image) && image is Pillow.Image)
                    throw Error("Pillow.ImageSequence.AllFrames expects Pillow.Image inputs", -1)
                current := image.Tell()
                try {
                    for frame in Pillow.ImageSequence.Iterator(image)
                        frames.Push(frame.Copy())
                } finally {
                    image.Seek(current)
                }
            }
            if !IsSet(fn)
                return frames

            mapped := []
            for frame in frames
                mapped.Push(fn.Call(frame))
            return mapped
        }

        static all_frames(im, fn := unset) {
            return IsSet(fn)
                ? Pillow.ImageSequence.AllFrames(im, fn)
                : Pillow.ImageSequence.AllFrames(im)
        }

        class Iterator {
            __New(image) {
                if !(IsObject(image) && image is Pillow.Image)
                    throw Error("im must have seek method", -1)
                this.Image := image
                this.Position := 0
            }

            __Item[index] {
                get {
                    return this.GetFrame(index)
                }
            }

            __Enum(varCount) {
                if varCount = 1 {
                    iterator := this
                    EnumerateOne(&frame) {
                        index := iterator.Position
                        if !iterator.SeekForIteration(index)
                            return false
                        iterator.Position := index + 1
                        frame := iterator.Image
                        return true
                    }
                    return EnumerateOne
                }
                if varCount = 2 {
                    iterator := this
                    EnumerateTwo(&index, &frame) {
                        frameIndex := iterator.Position
                        if !iterator.SeekForIteration(frameIndex)
                            return false
                        iterator.Position := frameIndex + 1
                        index := frameIndex
                        frame := iterator.Image
                        return true
                    }
                    return EnumerateTwo
                }
                throw Error("Pillow.ImageSequence.Iterator supports one or two loop variables", -1)
            }

            GetFrame(index) {
                if !(index is Integer)
                    throw Error("Pillow.ImageSequence.Iterator index must be an integer", -1)
                try {
                    this.Image.Seek(index)
                } catch Error as err {
                    if Pillow.ImageSequence.Iterator.IsEndError(err)
                        throw Error("end of sequence", -1)
                    throw
                }
                return this.Image
            }

            SeekForIteration(index) {
                try {
                    this.Image.Seek(index)
                    return true
                } catch Error as err {
                    if Pillow.ImageSequence.Iterator.IsEndError(err)
                        return false
                    throw
                }
            }

            static IsEndError(err) {
                return InStr(err.Message, "attempt to seek outside sequence") > 0
                    || InStr(err.Message, "no more images in file") > 0
            }
        }
    }

    class PngImagePlugin {
        class PngInfo {
            __New() {
                this.TextEntries := []
                this.ChunkEntries := []
            }

            AddText(key, value, zip := false) {
                return this.add_text(key, value, zip)
            }

            add_text(key, value, zip := false) {
                if !(key is String)
                    throw Error("Pillow.PngInfo.add_text key expects a string", -1)
                if value is String {
                    this.TextEntries.Push({ Key: key, Value: value, Zip: zip ? true : false, Kind: 0, RawLatin1: false })
                    return
                }
                if IsObject(value) && Type(value) = "Buffer" {
                    valueCopy := Buffer(value.Size, 0)
                    hasEmbeddedNul := false
                    loop value.Size {
                        byte := NumGet(value, A_Index - 1, "UChar")
                        if byte = 0
                            hasEmbeddedNul := true
                        NumPut("UChar", byte, valueCopy, A_Index - 1)
                    }
                    this.TextEntries.Push({ Key: key, Value: valueCopy, Zip: zip ? true : false, Kind: 0, RawLatin1: true, HasEmbeddedNul: hasEmbeddedNul })
                    return
                }
                throw Error("Pillow.PngInfo.add_text value expects a string or Buffer", -1)
            }

            AddIText(key, value, lang := "", tkey := "", zip := false) {
                return this.add_itxt(key, value, lang, tkey, zip)
            }

            add_itxt(key, value, lang := "", tkey := "", zip := false) {
                if !(key is String)
                    throw Error("Pillow.PngInfo.add_itxt key expects a string", -1)
                if !(value is String)
                    throw Error("Pillow.PngInfo.add_itxt value expects a string", -1)
                if !(lang is String)
                    throw Error("Pillow.PngInfo.add_itxt lang expects a string", -1)
                if !(tkey is String)
                    throw Error("Pillow.PngInfo.add_itxt tkey expects a string", -1)
                this.TextEntries.Push({ Key: key, Value: value, Zip: zip ? true : false, Kind: 1, Lang: lang, TKey: tkey })
            }

            add(cid, data, after_idat := false) {
                if cid is String {
                    dataBuffer := Pillow.Image.BinaryBuffer(data, "Pillow.PngInfo.add data")
                    dataCopy := Buffer(dataBuffer.Size, 0)
                    loop dataBuffer.Size
                        NumPut("UChar", NumGet(dataBuffer, A_Index - 1, "UChar"), dataCopy, A_Index - 1)
                    this.ChunkEntries.Push({ Type: cid, Data: dataCopy, AfterIdat: after_idat ? true : false })
                    return
                }
                typeBuffer := Pillow.Image.BinaryBuffer(cid, "Pillow.PngInfo.add cid")
                dataBuffer := Pillow.Image.BinaryBuffer(data, "Pillow.PngInfo.add data")
                typeCopy := Buffer(typeBuffer.Size, 0)
                dataCopy := Buffer(dataBuffer.Size, 0)
                loop typeBuffer.Size
                    NumPut("UChar", NumGet(typeBuffer, A_Index - 1, "UChar"), typeCopy, A_Index - 1)
                loop dataBuffer.Size
                    NumPut("UChar", NumGet(dataBuffer, A_Index - 1, "UChar"), dataCopy, A_Index - 1)
                this.ChunkEntries.Push({ Type: typeCopy, Data: dataCopy, AfterIdat: after_idat ? true : false })
            }
        }
    }

    class ImageColor {
        static GetRgb(color) {
            if !(color is String)
                throw Error("Pillow.ImageColor.GetRgb expects a color string", -1)
            if StrLen(color) > 100
                throw Error("color specifier is too long", -1)
            color := StrLower(color)

            colors := Pillow.ImageColor.ColorMap()
            if colors.Has(color)
                return Pillow.ImageColor.GetRgb(colors[color])

            if RegExMatch(color, "^#[a-f0-9]{3}$") {
                return [
                    Pillow.ImageColor.HexByte(SubStr(color, 2, 1) SubStr(color, 2, 1)),
                    Pillow.ImageColor.HexByte(SubStr(color, 3, 1) SubStr(color, 3, 1)),
                    Pillow.ImageColor.HexByte(SubStr(color, 4, 1) SubStr(color, 4, 1)),
                ]
            }
            if RegExMatch(color, "^#[a-f0-9]{4}$") {
                return [
                    Pillow.ImageColor.HexByte(SubStr(color, 2, 1) SubStr(color, 2, 1)),
                    Pillow.ImageColor.HexByte(SubStr(color, 3, 1) SubStr(color, 3, 1)),
                    Pillow.ImageColor.HexByte(SubStr(color, 4, 1) SubStr(color, 4, 1)),
                    Pillow.ImageColor.HexByte(SubStr(color, 5, 1) SubStr(color, 5, 1)),
                ]
            }
            if RegExMatch(color, "^#[a-f0-9]{6}$") {
                return [
                    Pillow.ImageColor.HexByte(SubStr(color, 2, 2)),
                    Pillow.ImageColor.HexByte(SubStr(color, 4, 2)),
                    Pillow.ImageColor.HexByte(SubStr(color, 6, 2)),
                ]
            }
            if RegExMatch(color, "^#[a-f0-9]{8}$") {
                return [
                    Pillow.ImageColor.HexByte(SubStr(color, 2, 2)),
                    Pillow.ImageColor.HexByte(SubStr(color, 4, 2)),
                    Pillow.ImageColor.HexByte(SubStr(color, 6, 2)),
                    Pillow.ImageColor.HexByte(SubStr(color, 8, 2)),
                ]
            }

            if RegExMatch(color, "^rgb\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)$", &m)
                return [Integer(m[1]), Integer(m[2]), Integer(m[3])]
            if RegExMatch(color, "^rgb\(\s*(\d+)%\s*,\s*(\d+)%\s*,\s*(\d+)%\s*\)$", &m) {
                return [
                    Floor((Integer(m[1]) * 255) / 100.0 + 0.5),
                    Floor((Integer(m[2]) * 255) / 100.0 + 0.5),
                    Floor((Integer(m[3]) * 255) / 100.0 + 0.5),
                ]
            }
            if RegExMatch(color, "^hsl\(\s*(\d+\.?\d*)\s*,\s*(\d+\.?\d*)%\s*,\s*(\d+\.?\d*)%\s*\)$", &m)
                return Pillow.ImageColor.HlsToRgb((m[1] + 0.0) / 360.0, (m[3] + 0.0) / 100.0, (m[2] + 0.0) / 100.0)
            if RegExMatch(color, "^hs[bv]\(\s*(\d+\.?\d*)\s*,\s*(\d+\.?\d*)%\s*,\s*(\d+\.?\d*)%\s*\)$", &m)
                return Pillow.ImageColor.HsvToRgb((m[1] + 0.0) / 360.0, (m[2] + 0.0) / 100.0, (m[3] + 0.0) / 100.0)
            if RegExMatch(color, "^rgba\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)$", &m)
                return [Integer(m[1]), Integer(m[2]), Integer(m[3]), Integer(m[4])]

            throw Error("unknown color specifier: '" color "'", -1)
        }

        static GetColor(color, modeName) {
            rgb := Pillow.ImageColor.GetRgb(color)
            alpha := 255
            if rgb.Length = 4 {
                alpha := rgb[4]
                rgb := [rgb[1], rgb[2], rgb[3]]
            }

            if modeName = "HSV"
                return Pillow.ImageColor.RgbToHsv(rgb)
            if Pillow.Image.GetModeBase(modeName) = "L" {
                gray := (rgb[1] * 19595 + rgb[2] * 38470 + rgb[3] * 7471 + 0x8000) >> 16
                if SubStr(modeName, StrLen(modeName), 1) = "A"
                    return [gray, alpha]
                return gray
            }
            if SubStr(modeName, StrLen(modeName), 1) = "A"
                return [rgb[1], rgb[2], rgb[3], alpha]
            return rgb
        }

        static HexByte(hex) {
            return Integer("0x" hex)
        }

        static HsvToRgb(h, s, v) {
            if s = 0
                return [Floor(v * 255 + 0.5), Floor(v * 255 + 0.5), Floor(v * 255 + 0.5)]
            h6 := h * 6.0
            i := Floor(h6)
            f := h6 - i
            i := Mod(i, 6)
            p := v * (1.0 - s)
            q := v * (1.0 - s * f)
            t := v * (1.0 - s * (1.0 - f))
            if i = 0
                values := [v, t, p]
            else if i = 1
                values := [q, v, p]
            else if i = 2
                values := [p, v, t]
            else if i = 3
                values := [p, q, v]
            else if i = 4
                values := [t, p, v]
            else
                values := [v, p, q]
            return [
                Floor(values[1] * 255 + 0.5),
                Floor(values[2] * 255 + 0.5),
                Floor(values[3] * 255 + 0.5),
            ]
        }

        static HlsToRgb(h, l, s) {
            if s = 0
                return [Floor(l * 255 + 0.5), Floor(l * 255 + 0.5), Floor(l * 255 + 0.5)]
            m2 := l <= 0.5 ? l * (1.0 + s) : l + s - l * s
            m1 := 2.0 * l - m2
            return [
                Floor(Pillow.ImageColor.HueToRgb(m1, m2, h + 1.0 / 3.0) * 255 + 0.5),
                Floor(Pillow.ImageColor.HueToRgb(m1, m2, h) * 255 + 0.5),
                Floor(Pillow.ImageColor.HueToRgb(m1, m2, h - 1.0 / 3.0) * 255 + 0.5),
            ]
        }

        static HueToRgb(m1, m2, h) {
            while h < 0
                h += 1.0
            while h > 1
                h -= 1.0
            if h < 1.0 / 6.0
                return m1 + (m2 - m1) * h * 6.0
            if h < 0.5
                return m2
            if h < 2.0 / 3.0
                return m1 + (m2 - m1) * (2.0 / 3.0 - h) * 6.0
            return m1
        }

        static RgbToHsv(rgb) {
            r := rgb[1] / 255.0
            g := rgb[2] / 255.0
            b := rgb[3] / 255.0
            maxValue := Max(r, g, b)
            minValue := Min(r, g, b)
            v := maxValue
            if minValue = maxValue
                return [0, 0, Floor(v * 255)]
            diff := maxValue - minValue
            s := diff / maxValue
            if r = maxValue {
                h := (g - b) / diff
                if g < b
                    h += 6.0
            } else if g = maxValue {
                h := (b - r) / diff + 2.0
            } else {
                h := (r - g) / diff + 4.0
            }
            h /= 6.0
            return [Floor(h * 255), Floor(s * 255), Floor(v * 255)]
        }

        static ColorMap() {
            static colors := Map(
                "aliceblue", "#f0f8ff", "antiquewhite", "#faebd7", "aqua", "#00ffff",
                "aquamarine", "#7fffd4", "azure", "#f0ffff", "beige", "#f5f5dc",
                "bisque", "#ffe4c4", "black", "#000000", "blanchedalmond", "#ffebcd",
                "blue", "#0000ff", "blueviolet", "#8a2be2", "brown", "#a52a2a",
                "burlywood", "#deb887", "cadetblue", "#5f9ea0", "chartreuse", "#7fff00",
                "chocolate", "#d2691e", "coral", "#ff7f50", "cornflowerblue", "#6495ed",
                "cornsilk", "#fff8dc", "crimson", "#dc143c", "cyan", "#00ffff",
                "darkblue", "#00008b", "darkcyan", "#008b8b", "darkgoldenrod", "#b8860b",
                "darkgray", "#a9a9a9", "darkgreen", "#006400", "darkgrey", "#a9a9a9",
                "darkkhaki", "#bdb76b", "darkmagenta", "#8b008b", "darkolivegreen", "#556b2f",
                "darkorange", "#ff8c00", "darkorchid", "#9932cc", "darkred", "#8b0000",
                "darksalmon", "#e9967a", "darkseagreen", "#8fbc8f", "darkslateblue", "#483d8b",
                "darkslategray", "#2f4f4f", "darkslategrey", "#2f4f4f", "darkturquoise", "#00ced1",
                "darkviolet", "#9400d3", "deeppink", "#ff1493", "deepskyblue", "#00bfff",
                "dimgray", "#696969", "dimgrey", "#696969", "dodgerblue", "#1e90ff",
                "firebrick", "#b22222", "floralwhite", "#fffaf0", "forestgreen", "#228b22",
                "fuchsia", "#ff00ff", "gainsboro", "#dcdcdc", "ghostwhite", "#f8f8ff",
                "gold", "#ffd700", "goldenrod", "#daa520", "gray", "#808080",
                "green", "#008000", "greenyellow", "#adff2f", "grey", "#808080",
                "honeydew", "#f0fff0", "hotpink", "#ff69b4", "indianred", "#cd5c5c",
                "indigo", "#4b0082", "ivory", "#fffff0", "khaki", "#f0e68c",
                "lavender", "#e6e6fa", "lavenderblush", "#fff0f5", "lawngreen", "#7cfc00",
                "lemonchiffon", "#fffacd", "lightblue", "#add8e6", "lightcoral", "#f08080",
                "lightcyan", "#e0ffff", "lightgoldenrodyellow", "#fafad2", "lightgray", "#d3d3d3",
                "lightgreen", "#90ee90", "lightgrey", "#d3d3d3", "lightpink", "#ffb6c1",
                "lightsalmon", "#ffa07a", "lightseagreen", "#20b2aa", "lightskyblue", "#87cefa",
                "lightslategray", "#778899", "lightslategrey", "#778899", "lightsteelblue", "#b0c4de",
                "lightyellow", "#ffffe0", "lime", "#00ff00", "limegreen", "#32cd32",
                "linen", "#faf0e6", "magenta", "#ff00ff", "maroon", "#800000",
                "mediumaquamarine", "#66cdaa", "mediumblue", "#0000cd", "mediumorchid", "#ba55d3",
                "mediumpurple", "#9370db", "mediumseagreen", "#3cb371", "mediumslateblue", "#7b68ee",
                "mediumspringgreen", "#00fa9a", "mediumturquoise", "#48d1cc", "mediumvioletred", "#c71585",
                "midnightblue", "#191970", "mintcream", "#f5fffa", "mistyrose", "#ffe4e1",
                "moccasin", "#ffe4b5", "navajowhite", "#ffdead", "navy", "#000080",
                "oldlace", "#fdf5e6", "olive", "#808000", "olivedrab", "#6b8e23",
                "orange", "#ffa500", "orangered", "#ff4500", "orchid", "#da70d6",
                "palegoldenrod", "#eee8aa", "palegreen", "#98fb98", "paleturquoise", "#afeeee",
                "palevioletred", "#db7093", "papayawhip", "#ffefd5", "peachpuff", "#ffdab9",
                "peru", "#cd853f", "pink", "#ffc0cb", "plum", "#dda0dd",
                "powderblue", "#b0e0e6", "purple", "#800080", "rebeccapurple", "#663399",
                "red", "#ff0000", "rosybrown", "#bc8f8f", "royalblue", "#4169e1",
                "saddlebrown", "#8b4513", "salmon", "#fa8072", "sandybrown", "#f4a460",
                "seagreen", "#2e8b57", "seashell", "#fff5ee", "sienna", "#a0522d",
                "silver", "#c0c0c0", "skyblue", "#87ceeb", "slateblue", "#6a5acd",
                "slategray", "#708090", "slategrey", "#708090", "snow", "#fffafa",
                "springgreen", "#00ff7f", "steelblue", "#4682b4", "tan", "#d2b48c",
                "teal", "#008080", "thistle", "#d8bfd8", "tomato", "#ff6347",
                "turquoise", "#40e0d0", "violet", "#ee82ee", "wheat", "#f5deb3",
                "white", "#ffffff", "whitesmoke", "#f5f5f5", "yellow", "#ffff00",
                "yellowgreen", "#9acd32"
            )
            return colors
        }
    }

    class ImageOps {
        static Invert(image) {
            outHandle := 0
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_invert",
                "Ptr", Pillow.ImageOps.RequireImageHandle(image, "Invert"),
                "Ptr*", &outHandle,
                "Int"
            )
            Pillow.ImageOps.CheckLutTransformStatus(status, image, true)
            return image.WrapDerivedHandle(outHandle)
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

        static ExifTranspose(image, in_place := false) {
            Pillow.ImageOps.RequireImageHandle(image, "ExifTranspose")
            orientation := image.ExifOrientation()
            if orientation <= 1 {
                if in_place
                    return
                return image.Copy()
            }

            method := Pillow.ImageOps.ExifOrientationTransposeMethod(orientation)
            if in_place {
                transposed := image.Transpose(method)
                oldHandle := image.RequireHandle()
                image.Handle := transposed.RequireHandle()
                transposed.Handle := 0
                Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", oldHandle, "Int"))
                return
            }
            return image.Transpose(method)
        }

        static exif_transpose(image, in_place := false) {
            return Pillow.ImageOps.ExifTranspose(image, in_place)
        }

        static ExifOrientationTransposeMethod(orientation) {
            switch orientation {
                case 2:
                    return Pillow.Transpose.FLIP_LEFT_RIGHT
                case 3:
                    return Pillow.Transpose.ROTATE_180
                case 4:
                    return Pillow.Transpose.FLIP_TOP_BOTTOM
                case 5:
                    return Pillow.Transpose.TRANSPOSE
                case 6:
                    return Pillow.Transpose.ROTATE_270
                case 7:
                    return Pillow.Transpose.TRANSVERSE
                case 8:
                    return Pillow.Transpose.ROTATE_90
            }
            throw Error("Pillow.ImageOps.ExifTranspose unsupported EXIF orientation", -1)
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
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_posterize",
                "Ptr", Pillow.ImageOps.RequireImageHandle(image, "Posterize"),
                "Int", bits,
                "Ptr*", &outHandle,
                "Int"
            )
            Pillow.ImageOps.CheckLutTransformStatus(status, image)
            return image.WrapDerivedHandle(outHandle)
        }

        static Solarize(image, threshold := 128) {
            if !(threshold is Number)
                throw Error("Pillow.ImageOps.Solarize threshold must be numeric", -1)

            outHandle := 0
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_solarize",
                "Ptr", Pillow.ImageOps.RequireImageHandle(image, "Solarize"),
                "Double", threshold,
                "Ptr*", &outHandle,
                "Int"
            )
            Pillow.ImageOps.CheckLutTransformStatus(status, image)
            return image.WrapDerivedHandle(outHandle)
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
            return image.WrapDerivedHandle(outHandle)
        }

        static Equalize(image, mask := unset) {
            if IsSet(mask) {
                outHandle := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_equalize_masked",
                    "Ptr", Pillow.ImageOps.RequireImageHandle(image, "Equalize"),
                    "Ptr", Pillow.ImageOps.RequireImageHandle(mask, "Equalize mask"),
                    "Ptr*", &outHandle,
                    "Int"
                )
                Pillow.ImageOps.CheckHistogramTransformStatus(status, image, true)
                return image.WrapDerivedHandle(outHandle)
            }
            outHandle := 0
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_equalize",
                "Ptr", Pillow.ImageOps.RequireImageHandle(image, "Equalize"),
                "Ptr*", &outHandle,
                "Int"
            )
            Pillow.ImageOps.CheckHistogramTransformStatus(status, image)
            return image.WrapDerivedHandle(outHandle)
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
            return image.WrapDerivedHandle(outHandle)
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
            status := DllCall(
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
            )
            Pillow.ImageOps.CheckHistogramTransformStatus(status, image, IsSet(mask), preserveTone)
            return image.WrapDerivedHandle(outHandle)
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
            return image.WrapDerivedHandle(outHandle)
        }

        static NativeUnaryImageOp(image, exportName) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\" exportName,
                "Ptr", Pillow.ImageOps.RequireImageHandle(image, exportName),
                "Ptr*", &outHandle,
                "Int"
            ))
            return image.WrapDerivedHandle(outHandle)
        }

        static CheckLutTransformStatus(status, image, allowModeOne := false) {
            if status = -3 && !Pillow.ImageOps.SupportsLutTransformMode(image.Mode, allowModeOne)
                throw Error("not supported for mode " image.Mode, -1)
            Pillow.CheckStatus(status)
        }

        static CheckHistogramTransformStatus(status, image, masked := false, preserveTone := false) {
            if status = -3 && (image.Mode = "I" || image.Mode = "F") {
                if masked && !preserveTone
                    throw Error("image has wrong mode", -1)
                throw Error("not supported for mode " image.Mode, -1)
            }
            Pillow.CheckStatus(status)
        }

        static SupportsLutTransformMode(mode, allowModeOne := false) {
            return mode = "L" || mode = "RGB" || (allowModeOne && mode = "1")
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
            if fill is String
                fill := Pillow.ImageColor.GetColor(fill, image.Mode)
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
            if color is String
                color := Pillow.ImageColor.GetColor(color, "RGB")
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

        static CheckBinaryStatus(status, left, right) {
            if status != 0 && left.Mode = right.Mode && (left.Mode = "I" || left.Mode = "F")
                throw Error("image has wrong mode", -1)
            Pillow.CheckStatus(status)
        }

        static BinaryOperation(exportName, left, right, operationName) {
            outHandle := 0
            status := DllCall(
                Pillow.RequireDllPath() "\" exportName,
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, operationName),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, operationName),
                "Ptr*", &outHandle,
                "Int"
            )
            Pillow.ImageChops.CheckBinaryStatus(status, left, right)
            return left.WrapDerivedHandle(outHandle)
        }

        static ScaledBinaryOperation(exportName, left, right, scale, offset, operationName) {
            outHandle := 0
            status := DllCall(
                Pillow.RequireDllPath() "\" exportName,
                "Ptr", Pillow.ImageChops.RequireImageHandle(left, operationName),
                "Ptr", Pillow.ImageChops.RequireImageHandle(right, operationName),
                "Double", scale,
                "Double", offset,
                "Ptr*", &outHandle,
                "Int"
            )
            Pillow.ImageChops.CheckBinaryStatus(status, left, right)
            return left.WrapDerivedHandle(outHandle)
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
            return image.WrapDerivedHandle(outHandle)
        }

        static Invert(image) {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_chops_invert",
                "Ptr", Pillow.ImageChops.RequireImageHandle(image, "Invert"),
                "Ptr*", &outHandle,
                "Int"
            ))
            return image.WrapDerivedHandle(outHandle)
        }

        static Difference(left, right) {
            return Pillow.ImageChops.BinaryOperation("pillow_c_image_difference", left, right, "Difference")
        }

        static Multiply(left, right) {
            return Pillow.ImageChops.BinaryOperation("pillow_c_image_multiply", left, right, "Multiply")
        }

        static Screen(left, right) {
            return Pillow.ImageChops.BinaryOperation("pillow_c_image_screen", left, right, "Screen")
        }

        static Lighter(left, right) {
            return Pillow.ImageChops.BinaryOperation("pillow_c_image_lighter", left, right, "Lighter")
        }

        static Darker(left, right) {
            return Pillow.ImageChops.BinaryOperation("pillow_c_image_darker", left, right, "Darker")
        }

        static SoftLight(left, right) {
            return Pillow.ImageChops.BinaryOperation("pillow_c_image_soft_light", left, right, "SoftLight")
        }

        static HardLight(left, right) {
            return Pillow.ImageChops.BinaryOperation("pillow_c_image_hard_light", left, right, "HardLight")
        }

        static Overlay(left, right) {
            return Pillow.ImageChops.BinaryOperation("pillow_c_image_overlay", left, right, "Overlay")
        }

        static LogicalAnd(left, right) {
            return Pillow.ImageChops.BinaryOperation("pillow_c_image_logical_and", left, right, "LogicalAnd")
        }

        static LogicalOr(left, right) {
            return Pillow.ImageChops.BinaryOperation("pillow_c_image_logical_or", left, right, "LogicalOr")
        }

        static LogicalXor(left, right) {
            return Pillow.ImageChops.BinaryOperation("pillow_c_image_logical_xor", left, right, "LogicalXor")
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
            return image.WrapDerivedHandle(outHandle)
        }

        static Add(left, right, scale := 1.0, offset := 0) {
            if !(scale is Number)
                throw Error("Pillow.ImageChops.Add scale must be numeric", -1)
            if !(offset is Number)
                throw Error("Pillow.ImageChops.Add offset must be numeric", -1)

            return Pillow.ImageChops.ScaledBinaryOperation("pillow_c_image_add", left, right, scale, offset, "Add")
        }

        static Subtract(left, right, scale := 1.0, offset := 0) {
            if !(scale is Number)
                throw Error("Pillow.ImageChops.Subtract scale must be numeric", -1)
            if !(offset is Number)
                throw Error("Pillow.ImageChops.Subtract offset must be numeric", -1)

            return Pillow.ImageChops.ScaledBinaryOperation("pillow_c_image_subtract", left, right, scale, offset, "Subtract")
        }

        static AddModulo(left, right) {
            return Pillow.ImageChops.BinaryOperation("pillow_c_image_add_modulo", left, right, "AddModulo")
        }

        static SubtractModulo(left, right) {
            return Pillow.ImageChops.BinaryOperation("pillow_c_image_subtract_modulo", left, right, "SubtractModulo")
        }

        static add_modulo(left, right) {
            return Pillow.ImageChops.AddModulo(left, right)
        }

        static hard_light(left, right) {
            return Pillow.ImageChops.HardLight(left, right)
        }

        static logical_and(left, right) {
            return Pillow.ImageChops.LogicalAnd(left, right)
        }

        static logical_or(left, right) {
            return Pillow.ImageChops.LogicalOr(left, right)
        }

        static logical_xor(left, right) {
            return Pillow.ImageChops.LogicalXor(left, right)
        }

        static soft_light(left, right) {
            return Pillow.ImageChops.SoftLight(left, right)
        }

        static subtract_modulo(left, right) {
            return Pillow.ImageChops.SubtractModulo(left, right)
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
                if image.Mode = "F"
                    throw Error("image has wrong mode", -1)

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
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_filter_mode",
                    "Ptr", image.RequireHandle(),
                    "Int", this.Size,
                    "Ptr*", &outHandle,
                    "Int"
                )
                if status != 0 && (image.Mode = "I" || image.Mode = "F")
                    throw Error("image has wrong mode", -1)
                Pillow.CheckStatus(status)
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
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_filter_box_blur",
                    "Ptr", image.RequireHandle(),
                    "Double", this.XRadius,
                    "Double", this.YRadius,
                    "Ptr*", &outHandle,
                    "Int"
                )
                if status != 0 && (image.Mode = "I" || image.Mode = "F")
                    throw Error("image has wrong mode", -1)
                Pillow.CheckStatus(status)
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
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_filter_gaussian_blur",
                    "Ptr", image.RequireHandle(),
                    "Double", this.XRadius,
                    "Double", this.YRadius,
                    "Ptr*", &outHandle,
                    "Int"
                )
                if status != 0 && (image.Mode = "I" || image.Mode = "F")
                    throw Error("image has wrong mode", -1)
                Pillow.CheckStatus(status)
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
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_filter_unsharp_mask",
                    "Ptr", image.RequireHandle(),
                    "Double", this.Radius,
                    "Int", this.Percent,
                    "Int", this.Threshold,
                    "Ptr*", &outHandle,
                    "Int"
                )
                if status != 0 && (image.Mode = "I" || image.Mode = "F")
                    throw Error("image has wrong mode", -1)
                Pillow.CheckStatus(status)
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

    class ImageCms {
        static CreateProfile(colorSpace, colorTemp := 0) {
            if !(colorSpace is String) || !(colorSpace == "sRGB" || colorSpace == "LAB" || colorSpace == "XYZ")
                throw Error("Color space not supported for on-the-fly profile creation (" colorSpace ")", -1)

            handle := 0
            if colorSpace == "LAB" {
                try {
                    temperature := colorTemp is Number ? colorTemp : Float(colorTemp)
                } catch Error {
                    throw Error("Color temperature must be numeric, `"" colorTemp "`" not valid", -1)
                }
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_create_lab",
                    "Double", temperature,
                    "Ptr*", &handle,
                    "Int"
                ))
            } else if colorSpace == "XYZ" {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_create_xyz",
                    "Ptr*", &handle,
                    "Int"
                ))
            } else {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_create_srgb",
                    "Ptr*", &handle,
                    "Int"
                ))
            }
            return Pillow.ImageCms.CmsProfile(handle)
        }

        static GetProfileName(profile) {
            if !(IsObject(profile) && profile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            return profile.GetName()
        }

        static GetProfileDescription(profile) {
            return Pillow.ImageCms.GetProfileName(profile)
        }

        static GetProfileInfo(profile) {
            if !(IsObject(profile) && profile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            required := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_profile_info",
                "Ptr", profile.RequireHandle(),
                "Ptr", 0,
                "UPtr", 0,
                "UPtr*", &required,
                "Int"
            ))
            infoBytes := Buffer(required, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_profile_info",
                "Ptr", profile.RequireHandle(),
                "Ptr", infoBytes,
                "UPtr", infoBytes.Size,
                "UPtr*", &required,
                "Int"
            ))
            return StrGet(infoBytes.Ptr, required - 1, "UTF-8")
        }

        static GetProfileCopyright(profile) {
            if !(IsObject(profile) && profile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            required := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_profile_copyright",
                "Ptr", profile.RequireHandle(),
                "Ptr", 0,
                "UPtr", 0,
                "UPtr*", &required,
                "Int"
            ))
            copyrightBytes := Buffer(required, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_profile_copyright",
                "Ptr", profile.RequireHandle(),
                "Ptr", copyrightBytes,
                "UPtr", copyrightBytes.Size,
                "UPtr*", &required,
                "Int"
            ))
            return StrGet(copyrightBytes.Ptr, required - 1, "UTF-8")
        }

        static GetProfileManufacturer(profile) {
            if !(IsObject(profile) && profile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            required := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_profile_manufacturer",
                "Ptr", profile.RequireHandle(),
                "Ptr", 0,
                "UPtr", 0,
                "UPtr*", &required,
                "Int"
            ))
            manufacturerBytes := Buffer(required, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_profile_manufacturer",
                "Ptr", profile.RequireHandle(),
                "Ptr", manufacturerBytes,
                "UPtr", manufacturerBytes.Size,
                "UPtr*", &required,
                "Int"
            ))
            return StrGet(manufacturerBytes.Ptr, required - 1, "UTF-8")
        }

        static GetProfileModel(profile) {
            if !(IsObject(profile) && profile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            required := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_profile_model",
                "Ptr", profile.RequireHandle(),
                "Ptr", 0,
                "UPtr", 0,
                "UPtr*", &required,
                "Int"
            ))
            modelBytes := Buffer(required, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_profile_model",
                "Ptr", profile.RequireHandle(),
                "Ptr", modelBytes,
                "UPtr", modelBytes.Size,
                "UPtr*", &required,
                "Int"
            ))
            return StrGet(modelBytes.Ptr, required - 1, "UTF-8")
        }

        static GetDefaultIntent(profile) {
            if !(IsObject(profile) && profile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            defaultIntent := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_profile_default_intent",
                "Ptr", profile.RequireHandle(),
                "Int*", &defaultIntent,
                "Int"
            ))
            return defaultIntent
        }

        static IsIntentSupported(profile, intent, direction) {
            if !(IsObject(profile) && profile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            supported := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_profile_intent_supported",
                "Ptr", profile.RequireHandle(),
                "Int", intent,
                "Int", direction,
                "Int*", &supported,
                "Int"
            ))
            return supported ? 1 : -1
        }

        static GetOpenProfile(profile) {
            if IsObject(profile) && profile is Pillow.ImageCms.CmsProfile {
                handle := profile.RequireHandle()
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_retain",
                    "Ptr", handle,
                    "Int"
                ))
                try {
                    openedProfile := Pillow.ImageCms.ImageCmsProfile(handle)
                    openedProfile.Profile := profile
                    openedProfile.Filename := 0
                    return openedProfile
                } catch {
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_cms_profile_free",
                        "Ptr", handle,
                        "Int"
                    ))
                    throw
                }
            }
            if profile is String {
                handle := 0
                nonAsciiPath := RegExMatch(profile, "[^\x00-\x7F]")
                if nonAsciiPath {
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_cms_profile_open_file_wide",
                        "WStr", profile,
                        "Ptr*", &handle,
                        "Int"
                    ))
                } else {
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_cms_profile_open_file",
                        "AStr", profile,
                        "Ptr*", &handle,
                        "Int"
                    ))
                }
                openedProfile := Pillow.ImageCms.ImageCmsProfile(handle)
                openedProfile.Filename := nonAsciiPath ? 0 : profile
                return openedProfile
            }
            if profile is File {
                profileBytes := Buffer(profile.Length - profile.Pos, 0)
                profile.RawRead(profileBytes, profileBytes.Size)
                openedProfile := Pillow.ImageCms.GetOpenProfile(profileBytes)
                openedProfile.Filename := 0
                return openedProfile
            }
            if !(profile is Buffer)
                throw Error("Invalid type for Profile", -1)
            handle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_profile_open_memory",
                "Ptr", profile,
                "UPtr", profile.Size,
                "Ptr*", &handle,
                "Int"
            ))
            return Pillow.ImageCms.ImageCmsProfile(handle)
        }

        static BuildTransform(
            inputProfile,
            outputProfile,
            inputMode,
            outputMode,
            renderingIntent := 0,
            flags := 0
        ) {
            if !(IsObject(inputProfile) && inputProfile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            if !(IsObject(outputProfile) && outputProfile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            validPair := (inputMode == "RGB" && outputMode == "LAB")
                || (inputMode == "LAB" && outputMode == "RGB")
                || (inputMode == "RGB" && outputMode == "RGB")
                || (inputMode == "LAB" && outputMode == "LAB")
            if !validPair
                throw Error("cannot build transform", -1)
            validIntent := renderingIntent == 0 || renderingIntent == 1
                || (renderingIntent == 2
                    && ((inputMode == "RGB" && outputMode == "LAB")
                        || (inputMode == "LAB" && outputMode == "RGB")
                        || (inputMode == "RGB" && outputMode == "RGB")
                        || (inputMode == "LAB" && outputMode == "LAB")))
                || (renderingIntent == 3
                    && ((inputMode == "RGB" && outputMode == "LAB")
                        || (inputMode == "LAB" && outputMode == "RGB")
                        || (inputMode == "RGB" && outputMode == "RGB")
                        || (inputMode == "LAB" && outputMode == "LAB")))
            validFlags := flags == 0
                || (flags == 0x2000
                    && (renderingIntent == 0
                        || (((inputMode == "RGB" && outputMode == "LAB")
                                || (inputMode == "LAB" && outputMode == "RGB")
                                || (inputMode == "RGB" && outputMode == "RGB")
                                || (inputMode == "LAB" && outputMode == "LAB"))
                            && renderingIntent == 1)
                        || (((inputMode == "RGB" && outputMode == "LAB")
                                || (inputMode == "LAB" && outputMode == "RGB")
                                || (inputMode == "RGB" && outputMode == "RGB")
                                || (inputMode == "LAB" && outputMode == "LAB"))
                            && renderingIntent == 2)
                        || (((inputMode == "RGB" && outputMode == "LAB")
                                || (inputMode == "LAB" && outputMode == "RGB")
                                || (inputMode == "RGB" && outputMode == "RGB")
                                || (inputMode == "LAB" && outputMode == "LAB"))
                            && renderingIntent == 3)))
            if !validIntent || !validFlags
                throw Error("cannot build transform", -1)

            handle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_transform_build",
                "Ptr", inputProfile.RequireHandle(),
                "Ptr", outputProfile.RequireHandle(),
                "Int", Pillow.ModeId(inputMode),
                "Int", Pillow.ModeId(outputMode),
                "Int", renderingIntent,
                "UInt", flags,
                "Ptr*", &handle,
                "Int"
            ))
            return Pillow.ImageCms.ImageCmsTransform(handle, inputMode, outputMode)
        }

        static BuildTransformFromOpenProfiles(
            inputProfile,
            outputProfile,
            inputMode,
            outputMode,
            renderingIntent := 0,
            flags := 0
        ) {
            return Pillow.ImageCms.BuildTransform(
                inputProfile,
                outputProfile,
                inputMode,
                outputMode,
                renderingIntent,
                flags
            )
        }

        static BuildProofTransform(
            inputProfile,
            outputProfile,
            proofProfile,
            inputMode,
            outputMode,
            renderingIntent := 0,
            proofRenderingIntent := 3,
            flags := 0x4000
        ) {
            if !(IsObject(inputProfile) && inputProfile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            if !(IsObject(outputProfile) && outputProfile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            if !(IsObject(proofProfile) && proofProfile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            supportedModePair :=
                (inputMode == "RGB" && (outputMode == "RGB" || outputMode == "LAB"))
                || (inputMode == "LAB" && (outputMode == "RGB" || outputMode == "LAB"))
            supportedRenderingIntent :=
                renderingIntent == 0
                || (supportedModePair && renderingIntent == 1)
                || (supportedModePair && renderingIntent == 2)
                || (supportedModePair && renderingIntent == 3)
            supportedFlags :=
                flags == 0x4000
                || (inputMode == "LAB" && (outputMode == "RGB" || outputMode == "LAB")
                    && renderingIntent == 0 && flags == 0x5000)
                || (inputMode == "LAB" && outputMode == "RGB"
                    && renderingIntent == 1 && flags == 0x5000)
                || (inputMode == "LAB" && outputMode == "LAB"
                    && renderingIntent == 1 && flags == 0x5000)
                || (inputMode == "LAB" && outputMode == "RGB"
                    && renderingIntent == 2 && flags == 0x5000)
                || (inputMode == "LAB" && outputMode == "LAB"
                    && renderingIntent == 2 && flags == 0x5000)
                || (inputMode == "LAB" && outputMode == "RGB"
                    && renderingIntent == 3 && flags == 0x5000)
                || (inputMode == "LAB" && outputMode == "LAB"
                    && renderingIntent == 3 && flags == 0x5000)
                || (inputMode == "RGB" && outputMode == "RGB"
                    && renderingIntent == 0 && flags == 0x5000)
                || (inputMode == "RGB" && outputMode == "RGB"
                    && renderingIntent == 1 && flags == 0x5000)
                || (inputMode == "RGB" && outputMode == "RGB"
                    && renderingIntent == 2 && flags == 0x5000)
                || (inputMode == "RGB" && outputMode == "RGB"
                    && renderingIntent == 3 && flags == 0x5000)
                || (inputMode == "RGB" && outputMode == "LAB"
                    && renderingIntent == 0 && flags == 0x5000)
                || (inputMode == "RGB" && outputMode == "LAB"
                    && renderingIntent == 1 && flags == 0x5000)
                || (inputMode == "RGB" && outputMode == "LAB"
                    && renderingIntent == 2 && flags == 0x5000)
                || (inputMode == "RGB" && outputMode == "LAB"
                    && renderingIntent == 3 && flags == 0x5000)
            if !supportedModePair
                || !supportedRenderingIntent || proofRenderingIntent != 3
                || !supportedFlags
                throw Error("cannot build proof transform", -1)

            handle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_proof_transform_build",
                "Ptr", inputProfile.RequireHandle(),
                "Ptr", outputProfile.RequireHandle(),
                "Ptr", proofProfile.RequireHandle(),
                "Int", Pillow.ModeId(inputMode),
                "Int", Pillow.ModeId(outputMode),
                "Int", renderingIntent,
                "Int", proofRenderingIntent,
                "UInt", flags,
                "Ptr*", &handle,
                "Int"
            ))
            return Pillow.ImageCms.ImageCmsTransform(handle, inputMode, outputMode)
        }

        static ApplyTransform(image, transform, inPlace := false) {
            if !(IsObject(image) && image is Pillow.Image)
                throw Error("Invalid image type", -1)
            if !(IsObject(transform) && transform is Pillow.ImageCms.ImageCmsTransform)
                throw Error("Invalid transform type", -1)
            if inPlace
                return transform.ApplyInPlace(image)
            return transform.Apply(image)
        }

        static ProfileToProfile(
            image,
            inputProfile,
            outputProfile,
            renderingIntent := 0,
            outputMode := unset,
            inPlace := false,
            flags := 0
        ) {
            if !(IsObject(image) && image is Pillow.Image)
                throw Error("Invalid image type", -1)
            if !(IsObject(inputProfile) && inputProfile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            if !(IsObject(outputProfile) && outputProfile is Pillow.ImageCms.CmsProfile)
                throw Error("Invalid type for Profile", -1)
            if !IsSet(outputMode)
                throw Error("cannot build transform", -1)
            validPair := (image.Mode == "RGB" && outputMode == "LAB")
                || (image.Mode == "LAB" && outputMode == "RGB")
                || (image.Mode == "RGB" && outputMode == "RGB")
                || (image.Mode == "LAB" && outputMode == "LAB")
            validInPlace := !inPlace
                || (image.Mode == "RGB" && outputMode == "RGB")
                || (image.Mode == "LAB" && outputMode == "LAB")
            validIntent := renderingIntent == 0 || renderingIntent == 1
                || (!inPlace && renderingIntent == 2
                    && ((image.Mode == "RGB" && outputMode == "LAB")
                        || (image.Mode == "LAB" && outputMode == "RGB")
                        || (image.Mode == "RGB" && outputMode == "RGB")
                        || (image.Mode == "LAB" && outputMode == "LAB")))
                || (inPlace && renderingIntent == 2
                    && ((image.Mode == "RGB" && outputMode == "RGB")
                        || (image.Mode == "LAB" && outputMode == "LAB")))
                || (inPlace && renderingIntent == 3
                    && ((image.Mode == "RGB" && outputMode == "RGB")
                        || (image.Mode == "LAB" && outputMode == "LAB")))
                || (!inPlace && renderingIntent == 3
                    && ((image.Mode == "RGB" && outputMode == "LAB")
                        || (image.Mode == "LAB" && outputMode == "RGB")
                        || (image.Mode == "RGB" && outputMode == "RGB")
                        || (image.Mode == "LAB" && outputMode == "LAB")))
            validFlags := flags == 0
                || (!inPlace
                    && flags == 0x2000
                    && ((image.Mode == "RGB" && outputMode == "LAB")
                        || (image.Mode == "LAB" && outputMode == "RGB")
                        || (image.Mode == "RGB" && outputMode == "RGB")
                        || (image.Mode == "LAB" && outputMode == "LAB"))
                    && (renderingIntent == 0
                        || (((image.Mode == "RGB" && outputMode == "LAB")
                                || (image.Mode == "LAB" && outputMode == "RGB")
                                || (image.Mode == "RGB" && outputMode == "RGB")
                                || (image.Mode == "LAB" && outputMode == "LAB"))
                            && renderingIntent == 1)
                        || (((image.Mode == "RGB" && outputMode == "LAB")
                                || (image.Mode == "LAB" && outputMode == "RGB")
                                || (image.Mode == "RGB" && outputMode == "RGB")
                                || (image.Mode == "LAB" && outputMode == "LAB"))
                            && renderingIntent == 2)
                        || (((image.Mode == "RGB" && outputMode == "LAB")
                                || (image.Mode == "LAB" && outputMode == "RGB")
                                || (image.Mode == "RGB" && outputMode == "RGB")
                                || (image.Mode == "LAB" && outputMode == "LAB"))
                            && renderingIntent == 3)))
                || (inPlace
                    && flags == 0x2000
                    && ((((image.Mode == "RGB" && outputMode == "RGB")
                            || (image.Mode == "LAB" && outputMode == "LAB"))
                        && renderingIntent == 0)
                        || (((image.Mode == "RGB" && outputMode == "RGB")
                                || (image.Mode == "LAB" && outputMode == "LAB"))
                            && renderingIntent == 1)
                        || (((image.Mode == "RGB" && outputMode == "RGB")
                                || (image.Mode == "LAB" && outputMode == "LAB"))
                            && renderingIntent == 2)
                        || (((image.Mode == "RGB" && outputMode == "RGB")
                                || (image.Mode == "LAB" && outputMode == "LAB"))
                            && renderingIntent == 3)))
            if !validPair || !validIntent || !validFlags
                || !validInPlace
                throw Error("cannot build transform", -1)

            if inPlace {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_to_profile_image_in_place",
                    "Ptr", image.RequireHandle(),
                    "Ptr", inputProfile.RequireHandle(),
                    "Ptr", outputProfile.RequireHandle(),
                    "Int", Pillow.ModeId(outputMode),
                    "Int", renderingIntent,
                    "UInt", flags,
                    "Int"
                ))
                image.Info["icc_profile"] := outputProfile.Serialize()
                return ""
            }

            resultHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_cms_profile_to_profile_image",
                "Ptr", image.RequireHandle(),
                "Ptr", inputProfile.RequireHandle(),
                "Ptr", outputProfile.RequireHandle(),
                "Int", Pillow.ModeId(outputMode),
                "Int", renderingIntent,
                "UInt", flags,
                "Ptr*", &resultHandle,
                "Int"
            ))
            result := Pillow.WrapImageHandle(resultHandle)
            try {
                result.Info["icc_profile"] := outputProfile.Serialize()
                return result
            } catch {
                result.Close()
                throw
            }
        }

        class CmsProfile {
            __New(handle) {
                if handle = 0
                    throw Error("pillow_c returned a null CMS profile handle", -2)
                this.Handle := handle
            }

            __Delete() {
                this.Close()
            }

            Close() {
                handle := this.HasOwnProp("Handle") ? this.Handle : 0
                if handle {
                    this.Handle := 0
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_cms_profile_free",
                        "Ptr", handle,
                        "Int"
                    ))
                }
            }

            RequireHandle() {
                if !this.HasOwnProp("Handle") || !this.Handle
                    throw Error("Pillow.ImageCms profile is closed", -1)
                return this.Handle
            }

            profile_description {
                get => RTrim(Pillow.ImageCms.GetProfileDescription(this), "`r`n")
            }

            copyright {
                get => RTrim(Pillow.ImageCms.GetProfileCopyright(this), "`r`n")
            }

            manufacturer {
                get {
                    value := RTrim(Pillow.ImageCms.GetProfileManufacturer(this), "`r`n")
                    return value == "" ? 0 : value
                }
            }

            model {
                get {
                    value := RTrim(Pillow.ImageCms.GetProfileModel(this), "`r`n")
                    return value == "" ? 0 : value
                }
            }

            rendering_intent {
                get => Pillow.ImageCms.GetDefaultIntent(this)
            }

            creation_date {
                get => this.GetHeaderIdentityRemainder().creation_date
            }

            header_flags {
                get => this.GetHeaderIdentityRemainder().header_flags
            }

            header_manufacturer {
                get => this.GetHeaderIdentityRemainder().header_manufacturer
            }

            header_model {
                get => this.GetHeaderIdentityRemainder().header_model
            }

            profile_id {
                get => this.GetHeaderIdentityRemainder().profile_id
            }

            perceptual_rendering_intent_gamut {
                get => this.GetOptionalSignatures()[1]
            }

            saturation_rendering_intent_gamut {
                get => this.GetOptionalSignatures()[2]
            }

            technology {
                get => this.GetOptionalSignatures()[3]
            }

            screening_description {
                get => this.GetOptionalTextTags()[1]
            }

            target {
                get => this.GetOptionalTextTags()[2]
            }

            icc_measurement_condition {
                get => this.GetConditionTags()[1]
            }

            icc_viewing_condition {
                get => this.GetConditionTags()[2]
            }

            viewing_condition {
                get => this.GetConditionTags()[3]
            }

            attributes {
                get => this.GetAttributesAndColorimetricIntent().attributes
            }

            colorimetric_intent {
                get => this.GetAttributesAndColorimetricIntent().colorimetric_intent
            }

            colorant_table {
                get => this.GetColorantTables()[1]
            }

            colorant_table_out {
                get => this.GetColorantTables()[2]
            }

            clut {
                get => this.GetClutState()
            }

            intent_supported {
                get => this.GetIntentSupport()
            }

            is_intent_supported(intent, direction) {
                return Pillow.ImageCms.IsIntentSupported(this, intent, direction)
            }

            connection_space {
                get => this.GetHeaderIdentity().connection_space
            }

            device_class {
                get => this.GetHeaderIdentity().device_class
            }

            xcolor_space {
                get => this.GetHeaderIdentity().xcolor_space
            }

            is_matrix_shaper {
                get => this.GetHeaderIdentity().is_matrix_shaper
            }

            version {
                get => this.GetHeaderIdentity().version
            }

            icc_version {
                get => this.GetHeaderIdentity().icc_version
            }

            media_white_point {
                get {
                    this.RequireHandle()
                    if this.HasOwnProp("MediaWhitePointValue")
                        return this.MediaWhitePointValue

                    present := 0
                    xyzX := 0.0
                    xyzY := 0.0
                    xyzZ := 0.0
                    xyyX := 0.0
                    xyyY := 0.0
                    xyyLuminance := 0.0
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_cms_profile_media_white_point",
                        "Ptr", this.RequireHandle(),
                        "Int*", &present,
                        "Double*", &xyzX,
                        "Double*", &xyzY,
                        "Double*", &xyzZ,
                        "Double*", &xyyX,
                        "Double*", &xyyY,
                        "Double*", &xyyLuminance,
                        "Int"
                    ))
                    this.MediaWhitePointValue := present
                        ? [[xyzX, xyzY, xyzZ], [xyyX, xyyY, xyyLuminance]]
                        : 0
                    return this.MediaWhitePointValue
                }
            }

            media_black_point {
                get => this.GetOptionalXyzTags()[1]
            }

            luminance {
                get => this.GetOptionalXyzTags()[2]
            }

            chromaticity {
                get {
                    this.RequireHandle()
                    if this.HasOwnProp("ChromaticityValue")
                        return this.ChromaticityValue

                    present := 0
                    values := Buffer(9 * 8, 0)
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_cms_profile_chromaticity",
                        "Ptr", this.RequireHandle(),
                        "Int*", &present,
                        "Ptr", values,
                        "UPtr", 9,
                        "Int"
                    ))
                    this.ChromaticityValue := present ? [
                        [NumGet(values, 0, "Double"), NumGet(values, 8, "Double"), NumGet(values, 16, "Double")],
                        [NumGet(values, 24, "Double"), NumGet(values, 32, "Double"), NumGet(values, 40, "Double")],
                        [NumGet(values, 48, "Double"), NumGet(values, 56, "Double"), NumGet(values, 64, "Double")]
                    ] : 0
                    return this.ChromaticityValue
                }
            }

            media_white_point_temperature {
                get {
                    this.RequireHandle()
                    if this.HasOwnProp("MediaWhitePointTemperatureValue")
                        return this.MediaWhitePointTemperatureValue

                    present := 0
                    temperature := 0.0
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_cms_profile_media_white_point_temperature",
                        "Ptr", this.RequireHandle(),
                        "Int*", &present,
                        "Double*", &temperature,
                        "Int"
                    ))
                    this.MediaWhitePointTemperatureValue := present ? temperature : 0
                    return this.MediaWhitePointTemperatureValue
                }
            }

            GetOptionalXyzTags() {
                this.RequireHandle()
                if this.HasOwnProp("OptionalXyzTags")
                    return this.OptionalXyzTags

                present := Buffer(2 * 4, 0)
                values := Buffer(12 * 8, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_optional_xyz_tags",
                    "Ptr", this.RequireHandle(),
                    "Ptr", present,
                    "UPtr", 2,
                    "Ptr", values,
                    "UPtr", 12,
                    "Int"
                ))
                tags := []
                Loop 2 {
                    index := A_Index - 1
                    offset := index * 6 * 8
                    tags.Push(NumGet(present, index * 4, "Int") ? [
                        [
                            NumGet(values, offset, "Double"),
                            NumGet(values, offset + 8, "Double"),
                            NumGet(values, offset + 16, "Double")
                        ],
                        [
                            NumGet(values, offset + 24, "Double"),
                            NumGet(values, offset + 32, "Double"),
                            NumGet(values, offset + 40, "Double")
                        ]
                    ] : 0)
                }
                this.OptionalXyzTags := tags
                return this.OptionalXyzTags
            }

            red_colorant {
                get => this.GetRgbColorants()[1]
            }

            green_colorant {
                get => this.GetRgbColorants()[2]
            }

            blue_colorant {
                get => this.GetRgbColorants()[3]
            }

            red_primary {
                get => this.GetRgbPrimaries()[1]
            }

            green_primary {
                get => this.GetRgbPrimaries()[2]
            }

            blue_primary {
                get => this.GetRgbPrimaries()[3]
            }

            chromatic_adaptation {
                get {
                    this.RequireHandle()
                    if this.HasOwnProp("ChromaticAdaptationValue")
                        return this.ChromaticAdaptationValue

                    present := 0
                    values := Buffer(18 * 8, 0)
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_cms_profile_chromatic_adaptation",
                        "Ptr", this.RequireHandle(),
                        "Int*", &present,
                        "Ptr", values,
                        "UPtr", 18,
                        "Int"
                    ))
                    this.ChromaticAdaptationValue := present ? [
                        [
                            [NumGet(values, 0, "Double"), NumGet(values, 8, "Double"), NumGet(values, 16, "Double")],
                            [NumGet(values, 24, "Double"), NumGet(values, 32, "Double"), NumGet(values, 40, "Double")],
                            [NumGet(values, 48, "Double"), NumGet(values, 56, "Double"), NumGet(values, 64, "Double")]
                        ],
                        [
                            [NumGet(values, 72, "Double"), NumGet(values, 80, "Double"), NumGet(values, 88, "Double")],
                            [NumGet(values, 96, "Double"), NumGet(values, 104, "Double"), NumGet(values, 112, "Double")],
                            [NumGet(values, 120, "Double"), NumGet(values, 128, "Double"), NumGet(values, 136, "Double")]
                        ]
                    ] : 0
                    return this.ChromaticAdaptationValue
                }
            }

            GetRgbColorants() {
                this.RequireHandle()
                if this.HasOwnProp("RgbColorants")
                    return this.RgbColorants

                present := Buffer(3 * 4, 0)
                values := Buffer(18 * 8, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_rgb_colorants",
                    "Ptr", this.RequireHandle(),
                    "Ptr", present,
                    "UPtr", 3,
                    "Ptr", values,
                    "UPtr", 18,
                    "Int"
                ))
                colorants := []
                Loop 3 {
                    index := A_Index - 1
                    offset := index * 6 * 8
                    colorants.Push(NumGet(present, index * 4, "Int") ? [
                        [
                            NumGet(values, offset, "Double"),
                            NumGet(values, offset + 8, "Double"),
                            NumGet(values, offset + 16, "Double")
                        ],
                        [
                            NumGet(values, offset + 24, "Double"),
                            NumGet(values, offset + 32, "Double"),
                            NumGet(values, offset + 40, "Double")
                        ]
                    ] : 0)
                }
                this.RgbColorants := colorants
                return this.RgbColorants
            }

            GetRgbPrimaries() {
                this.RequireHandle()
                if this.HasOwnProp("RgbPrimaries")
                    return this.RgbPrimaries

                present := Buffer(3 * 4, 0)
                values := Buffer(18 * 8, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_rgb_primaries",
                    "Ptr", this.RequireHandle(),
                    "Ptr", present,
                    "UPtr", 3,
                    "Ptr", values,
                    "UPtr", 18,
                    "Int"
                ))
                primaries := []
                Loop 3 {
                    index := A_Index - 1
                    offset := index * 6 * 8
                    primaries.Push(NumGet(present, index * 4, "Int") ? [
                        [
                            NumGet(values, offset, "Double"),
                            NumGet(values, offset + 8, "Double"),
                            NumGet(values, offset + 16, "Double")
                        ],
                        [
                            NumGet(values, offset + 24, "Double"),
                            NumGet(values, offset + 32, "Double"),
                            NumGet(values, offset + 40, "Double")
                        ]
                    ] : 0)
                }
                this.RgbPrimaries := primaries
                return this.RgbPrimaries
            }

            GetHeaderIdentityRemainder() {
                this.RequireHandle()
                if this.HasOwnProp("HeaderIdentityRemainder")
                    return this.HeaderIdentityRemainder

                year := 0
                month := 0
                day := 0
                hour := 0
                minute := 0
                second := 0
                flags := 0
                manufacturer := 0
                model := 0
                profileId := Buffer(16, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_header_identity",
                    "Ptr", this.RequireHandle(),
                    "Int*", &year,
                    "Int*", &month,
                    "Int*", &day,
                    "Int*", &hour,
                    "Int*", &minute,
                    "Int*", &second,
                    "UInt*", &flags,
                    "UInt*", &manufacturer,
                    "UInt*", &model,
                    "Ptr", profileId,
                    "UPtr", profileId.Size,
                    "Int"
                ))
                this.HeaderIdentityRemainder := {
                    creation_date: {
                        year: year,
                        month: month,
                        day: day,
                        hour: hour,
                        minute: minute,
                        second: second
                    },
                    header_flags: flags,
                    header_manufacturer: Pillow.ImageCms.CmsProfile.SignatureText(manufacturer),
                    header_model: Pillow.ImageCms.CmsProfile.SignatureText(model),
                    profile_id: profileId
                }
                return this.HeaderIdentityRemainder
            }

            GetOptionalSignatures() {
                this.RequireHandle()
                if this.HasOwnProp("OptionalSignatures")
                    return this.OptionalSignatures

                present := Buffer(3 * 4, 0)
                values := Buffer(3 * 4, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_optional_signatures",
                    "Ptr", this.RequireHandle(),
                    "Ptr", present,
                    "UPtr", 3,
                    "Ptr", values,
                    "UPtr", 3,
                    "Int"
                ))
                signatures := []
                Loop 3 {
                    index := A_Index - 1
                    signatures.Push(NumGet(present, index * 4, "Int")
                        ? Pillow.ImageCms.CmsProfile.SignatureText(NumGet(values, index * 4, "UInt"))
                        : 0)
                }
                this.OptionalSignatures := signatures
                return this.OptionalSignatures
            }

            GetOptionalTextTags() {
                this.RequireHandle()
                if this.HasOwnProp("OptionalTextTags")
                    return this.OptionalTextTags

                present := Buffer(2 * 4, 0)
                screeningRequired := 0
                targetRequired := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_optional_text_tags",
                    "Ptr", this.RequireHandle(),
                    "Ptr", present,
                    "UPtr", 2,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &screeningRequired,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &targetRequired,
                    "Int"
                ))
                screeningBytes := screeningRequired ? Buffer(screeningRequired, 0) : 0
                targetBytes := targetRequired ? Buffer(targetRequired, 0) : 0
                if screeningRequired || targetRequired {
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_cms_profile_optional_text_tags",
                        "Ptr", this.RequireHandle(),
                        "Ptr", present,
                        "UPtr", 2,
                        "Ptr", screeningBytes,
                        "UPtr", screeningRequired,
                        "UPtr*", &screeningRequired,
                        "Ptr", targetBytes,
                        "UPtr", targetRequired,
                        "UPtr*", &targetRequired,
                        "Int"
                    ))
                }
                this.OptionalTextTags := [
                    NumGet(present, 0, "Int")
                        ? StrGet(screeningBytes.Ptr, screeningRequired - 1, "UTF-8")
                        : 0,
                    NumGet(present, 4, "Int")
                        ? StrGet(targetBytes.Ptr, targetRequired - 1, "UTF-8")
                        : 0
                ]
                return this.OptionalTextTags
            }

            GetConditionTags() {
                this.RequireHandle()
                if this.HasOwnProp("ConditionTags")
                    return this.ConditionTags

                present := Buffer(3 * 4, 0)
                codes := Buffer(4 * 4, 0)
                values := Buffer(10 * 8, 0)
                viewingDescriptionRequired := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_condition_tags",
                    "Ptr", this.RequireHandle(),
                    "Ptr", present,
                    "UPtr", 3,
                    "Ptr", codes,
                    "UPtr", 4,
                    "Ptr", values,
                    "UPtr", 10,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &viewingDescriptionRequired,
                    "Int"
                ))
                viewingDescription := viewingDescriptionRequired
                    ? Buffer(viewingDescriptionRequired, 0)
                    : 0
                if viewingDescriptionRequired {
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_cms_profile_condition_tags",
                        "Ptr", this.RequireHandle(),
                        "Ptr", present,
                        "UPtr", 3,
                        "Ptr", codes,
                        "UPtr", 4,
                        "Ptr", values,
                        "UPtr", 10,
                        "Ptr", viewingDescription,
                        "UPtr", viewingDescriptionRequired,
                        "UPtr*", &viewingDescriptionRequired,
                        "Int"
                    ))
                }
                measurement := NumGet(present, 0, "Int") ? {
                    observer: NumGet(codes, 0, "UInt"),
                    backing: [
                        NumGet(values, 0, "Double"),
                        NumGet(values, 8, "Double"),
                        NumGet(values, 16, "Double")
                    ],
                    geo: Pillow.ImageCms.CmsProfile.ConditionGeometry(NumGet(codes, 4, "UInt")),
                    flare: NumGet(values, 24, "Double"),
                    illuminant_type: Pillow.ImageCms.CmsProfile.ConditionIlluminantType(NumGet(codes, 8, "UInt"))
                } : 0
                viewing := NumGet(present, 4, "Int") ? {
                    illuminant: [
                        NumGet(values, 32, "Double"),
                        NumGet(values, 40, "Double"),
                        NumGet(values, 48, "Double")
                    ],
                    surround: [
                        NumGet(values, 56, "Double"),
                        NumGet(values, 64, "Double"),
                        NumGet(values, 72, "Double")
                    ],
                    illuminant_type: Pillow.ImageCms.CmsProfile.ConditionIlluminantType(NumGet(codes, 12, "UInt"))
                } : 0
                description := NumGet(present, 8, "Int")
                    ? StrGet(viewingDescription.Ptr, viewingDescriptionRequired - 1, "UTF-8")
                    : 0
                this.ConditionTags := [measurement, viewing, description]
                return this.ConditionTags
            }

            GetAttributesAndColorimetricIntent() {
                this.RequireHandle()
                if this.HasOwnProp("AttributesAndColorimetricIntent")
                    return this.AttributesAndColorimetricIntent

                attributes := 0
                colorimetricIntentPresent := 0
                colorimetricIntent := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_attributes_and_colorimetric_intent",
                    "Ptr", this.RequireHandle(),
                    "Int64*", &attributes,
                    "Int*", &colorimetricIntentPresent,
                    "UInt*", &colorimetricIntent,
                    "Int"
                ))
                this.AttributesAndColorimetricIntent := {
                    attributes: attributes,
                    colorimetric_intent: colorimetricIntentPresent
                        ? Pillow.ImageCms.CmsProfile.SignatureText(colorimetricIntent)
                        : 0
                }
                return this.AttributesAndColorimetricIntent
            }

            GetColorantTables() {
                this.RequireHandle()
                if this.HasOwnProp("ColorantTables")
                    return this.ColorantTables

                present := Buffer(2 * 4, 0)
                counts := Buffer(2 * 4, 0)
                tableRequired := 0
                tableOutRequired := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_colorant_tables",
                    "Ptr", this.RequireHandle(),
                    "Ptr", present,
                    "UPtr", 2,
                    "Ptr", counts,
                    "UPtr", 2,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &tableRequired,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &tableOutRequired,
                    "Int"
                ))
                tableBytes := tableRequired ? Buffer(tableRequired, 0) : 0
                tableOutBytes := tableOutRequired ? Buffer(tableOutRequired, 0) : 0
                if tableRequired || tableOutRequired {
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_cms_profile_colorant_tables",
                        "Ptr", this.RequireHandle(),
                        "Ptr", present,
                        "UPtr", 2,
                        "Ptr", counts,
                        "UPtr", 2,
                        "Ptr", tableBytes,
                        "UPtr", tableRequired,
                        "UPtr*", &tableRequired,
                        "Ptr", tableOutBytes,
                        "UPtr", tableOutRequired,
                        "UPtr*", &tableOutRequired,
                        "Int"
                    ))
                }

                tables := []
                buffers := [tableBytes, tableOutBytes]
                Loop 2 {
                    index := A_Index - 1
                    if !NumGet(present, index * 4, "Int") {
                        tables.Push(0)
                        continue
                    }
                    names := []
                    count := NumGet(counts, index * 4, "UInt")
                    Loop count
                        names.Push(StrGet(buffers[index + 1].Ptr + (A_Index - 1) * 256, "UTF-8"))
                    tables.Push(names)
                }
                this.ColorantTables := tables
                return this.ColorantTables
            }

            GetClutState() {
                this.RequireHandle()
                if this.HasOwnProp("ClutState")
                    return this.ClutState

                values := Buffer(12 * 4, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_clut",
                    "Ptr", this.RequireHandle(),
                    "Ptr", values,
                    "UPtr", 12,
                    "Int"
                ))
                clut := Map()
                Loop 4 {
                    intent := A_Index - 1
                    offset := intent * 3
                    clut[intent] := [
                        NumGet(values, offset * 4, "Int") ? true : false,
                        NumGet(values, (offset + 1) * 4, "Int") ? true : false,
                        NumGet(values, (offset + 2) * 4, "Int") ? true : false
                    ]
                }
                this.ClutState := clut
                return this.ClutState
            }

            GetIntentSupport() {
                this.RequireHandle()
                if this.HasOwnProp("IntentSupport")
                    return this.IntentSupport

                values := Buffer(12 * 4, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_intent_support",
                    "Ptr", this.RequireHandle(),
                    "Ptr", values,
                    "UPtr", 12,
                    "Int"
                ))
                supported := Map()
                Loop 4 {
                    intent := A_Index - 1
                    offset := intent * 3
                    supported[intent] := [
                        NumGet(values, offset * 4, "Int") ? true : false,
                        NumGet(values, (offset + 1) * 4, "Int") ? true : false,
                        NumGet(values, (offset + 2) * 4, "Int") ? true : false
                    ]
                }
                this.IntentSupport := supported
                return this.IntentSupport
            }

            GetHeaderIdentity() {
                this.RequireHandle()
                if this.HasOwnProp("HeaderIdentity")
                    return this.HeaderIdentity

                deviceClass := 0
                colorSpace := 0
                connectionSpace := 0
                encodedVersion := 0
                version := 0.0
                isMatrixShaper := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_header",
                    "Ptr", this.RequireHandle(),
                    "UInt*", &deviceClass,
                    "UInt*", &colorSpace,
                    "UInt*", &connectionSpace,
                    "UInt*", &encodedVersion,
                    "Double*", &version,
                    "Int*", &isMatrixShaper,
                    "Int"
                ))
                this.HeaderIdentity := {
                    connection_space: Pillow.ImageCms.CmsProfile.SignatureText(connectionSpace),
                    device_class: Pillow.ImageCms.CmsProfile.SignatureText(deviceClass),
                    xcolor_space: Pillow.ImageCms.CmsProfile.SignatureText(colorSpace),
                    is_matrix_shaper: isMatrixShaper ? true : false,
                    version: version,
                    icc_version: encodedVersion
                }
                return this.HeaderIdentity
            }

            static SignatureText(signature) {
                return Chr((signature >> 24) & 0xFF) Chr((signature >> 16) & 0xFF) Chr((signature >> 8) & 0xFF) Chr(signature & 0xFF)
            }

            static ConditionGeometry(code) {
                return code = 1 ? "45/0, 0/45" : code = 2 ? "0d, d/0" : "unknown"
            }

            static ConditionIlluminantType(code) {
                return code = 1 ? "D50"
                    : code = 2 ? "D65"
                    : code = 3 ? "D93"
                    : code = 5 ? "D55"
                    : "unknown"
            }

            GetName() {
                required := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_name",
                    "Ptr", this.RequireHandle(),
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                ))
                nameBytes := Buffer(required, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_name",
                    "Ptr", this.RequireHandle(),
                    "Ptr", nameBytes,
                    "UPtr", nameBytes.Size,
                    "UPtr*", &required,
                    "Int"
                ))
                return StrGet(nameBytes.Ptr, required - 1, "UTF-8")
            }

            Serialize() {
                required := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_bytes",
                    "Ptr", this.RequireHandle(),
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                ))
                profileBytes := Buffer(required, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_profile_bytes",
                    "Ptr", this.RequireHandle(),
                    "Ptr", profileBytes,
                    "UPtr", profileBytes.Size,
                    "UPtr*", &required,
                    "Int"
                ))
                return profileBytes
            }
        }

        class ImageCmsProfile extends Pillow.ImageCms.CmsProfile {
            Tobytes() {
                return this.Serialize()
            }
        }

        class ImageCmsTransform {
            __New(handle, inputMode, outputMode) {
                if handle = 0
                    throw Error("pillow_c returned a null CMS transform handle", -2)
                this.Handle := handle
                this.InputMode := inputMode
                this.OutputMode := outputMode
            }

            __Delete() {
                this.Close()
            }

            input_mode {
                get => this.InputMode
            }

            output_mode {
                get => this.OutputMode
            }

            Close() {
                handle := this.HasOwnProp("Handle") ? this.Handle : 0
                if handle {
                    this.Handle := 0
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_cms_transform_free",
                        "Ptr", handle,
                        "Int"
                    ))
                }
            }

            RequireHandle() {
                if !this.HasOwnProp("Handle") || !this.Handle
                    throw Error("Pillow.ImageCms transform is closed", -1)
                return this.Handle
            }

            Apply(image) {
                if !(IsObject(image) && image is Pillow.Image)
                    throw Error("Invalid image type", -1)
                if !(image.Mode == this.InputMode)
                    throw Error("mode mismatch", -1)
                resultHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_transform_apply",
                    "Ptr", this.RequireHandle(),
                    "Ptr", image.RequireHandle(),
                    "Ptr*", &resultHandle,
                    "Int"
                ))
                result := Pillow.WrapImageHandle(resultHandle)
                try {
                    result.Info["icc_profile"] := this.OutputProfileBytes()
                    return result
                } catch {
                    result.Close()
                    throw
                }
            }

            ApplyInPlace(image) {
                if !(IsObject(image) && image is Pillow.Image)
                    throw Error("Invalid image type", -1)
                if !(image.Mode == this.InputMode) || !(this.InputMode == this.OutputMode)
                    throw Error("mode mismatch", -1)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_transform_apply_in_place",
                    "Ptr", this.RequireHandle(),
                    "Ptr", image.RequireHandle(),
                    "Int"
                ))
                image.Info["icc_profile"] := this.OutputProfileBytes()
                return ""
            }

            OutputProfileBytes() {
                required := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_transform_output_profile_bytes",
                    "Ptr", this.RequireHandle(),
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                ))
                profileBytes := Buffer(required, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_cms_transform_output_profile_bytes",
                    "Ptr", this.RequireHandle(),
                    "Ptr", profileBytes,
                    "UPtr", profileBytes.Size,
                    "UPtr*", &required,
                    "Int"
                ))
                return profileBytes
            }
        }
    }

    class ImageFont {
        static MAX_STRING_LENGTH := 1000000

        static CheckStringLength(text) {
            ; Pillow's _string_length_check: more than 1,000,000 characters
            ; raises ValueError("too many characters in string").
            if Pillow.ImageFont.MAX_STRING_LENGTH != "" && StrLen(text) > Pillow.ImageFont.MAX_STRING_LENGTH
                throw Error("too many characters in string", -1)
        }

        static LoadDefault() {
            handle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_font_load_default",
                "Ptr*", &handle,
                "Int"
            ))
            return Pillow.ImageFont.FreeTypeFont(handle)
        }

        static load_default() {
            return Pillow.ImageFont.LoadDefault()
        }

        ; BEHAV-FONTFILE-001: ImageFont.truetype / load / load_path.
        ; truetype loads a TrueType/OpenType face from a path or an in-memory
        ; Buffer and returns a FreeTypeFont; the native layer parses the sfnt
        ; tables (exact hmtx/kern/name/hhea arithmetic, pinned against Pillow
        ; 11.3.0) and renders glyphs through GDI. Pillow's Windows-fallback
        ; search of %WINDIR%\fonts is reproduced here (basename match with an
        ; extension, stem match preferring .ttf otherwise); the facade maps
        ; the native statuses to Pillow's exact error messages.
        static Truetype(font, size := 10, index := 0, encoding := "", layoutEngine := unset) {
            if !(size is Number)
                throw Error("Pillow.ImageFont.Truetype size expects a number", -1)
            if size <= 0
                throw Error("font size must be greater than 0, not " size, -1)
            if encoding != "" && encoding != "unic"
                throw Error("invalid argument", -1)
            engine := (IsSet(layoutEngine) && layoutEngine = 0) ? 0 : 1
            encodingBytes := Pillow.Image.Utf8Buffer(encoding)

            handle := 0
            if font is String {
                pathBytes := Pillow.Image.Utf8Buffer(font)
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_font_load_file",
                    "Ptr", pathBytes,
                    "Double", size,
                    "Int", index,
                    "Ptr", encodingBytes,
                    "Int", engine,
                    "Ptr*", &handle,
                    "Int"
                )
                if status = -60 {
                    ; Pillow's truetype() retries in the Windows font
                    ; repository when the direct open raises OSError.
                    resolved := Pillow.ImageFont.ResolveWindowsFont(font)
                    if resolved != "" {
                        resolvedBytes := Pillow.Image.Utf8Buffer(resolved)
                        status := DllCall(
                            Pillow.RequireDllPath() "\pillow_c_font_load_file",
                            "Ptr", resolvedBytes,
                            "Double", size,
                            "Int", index,
                            "Ptr", encodingBytes,
                            "Int", engine,
                            "Ptr*", &handle,
                            "Int"
                        )
                    }
                }
            } else if font is Buffer {
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_font_load_bytes",
                    "Ptr", font,
                    "UPtr", font.Size,
                    "Double", size,
                    "Int", index,
                    "Ptr", encodingBytes,
                    "Int", engine,
                    "Ptr*", &handle,
                    "Int"
                )
            } else {
                throw Error("Pillow.ImageFont.Truetype font expects a path or Buffer", -1)
            }
            if status = -60
                throw Error("cannot open resource", -1)
            if status = -61
                throw Error("unknown file format", -1)
            if status = -62
                throw Error("invalid argument", -1)
            if status = -63
                throw Error("invalid argument", -1)
            Pillow.CheckStatus(status)
            return Pillow.ImageFont.FreeTypeFont(handle, {
                Path: font,
                Size: size,
                Index: index,
                Encoding: encoding,
                LayoutEngine: engine,
            })
        }
        ; AHK method names are case-insensitive, so Pillow's snake_case
        ; truetype() resolves to Truetype above without a duplicate
        ; declaration.

        ; Pillow's Windows fallback: with an extension only exact basenames
        ; are tried; without one the stem is matched and a .ttf match wins.
        static ResolveWindowsFont(fontPath) {
            SplitPath(fontPath, &base)
            if base = ""
                return ""
            ext := ""
            stem := base
            dot := InStr(base, ".")
            if dot {
                stem := SubStr(base, 1, dot - 1)
                ext := SubStr(base, dot)
            }
            fontDir := EnvGet("WINDIR") "\fonts"
            if !DirExist(fontDir)
                return ""
            firstOther := ""
            matches := []
            Loop Files, fontDir "\*", "R" {
                if !(A_LoopFileName ~= "i)\.(ttf|otf)$")
                    continue
                if ext != "" {
                    if A_LoopFileName = base
                        matches.Push(A_LoopFileFullPath)
                } else {
                    if SubStr(A_LoopFileName, 1, StrLen(stem)) = stem && SubStr(A_LoopFileName, StrLen(stem) + 1, 1) = "." {
                        if A_LoopFileExt = "ttf"
                            matches.Push(A_LoopFileFullPath)
                        else if firstOther = ""
                            firstOther := A_LoopFileFullPath
                    }
                }
            }
            if matches.Length > 0
                return matches[1]
            return firstOther
        }

        ; BEHAV-FONTFILE-002: ImageFont.load, the PIL bitmap font loader.
        ; The glyph-image search (.png/.gif/.pbm in Pillow's order, first
        ; mode-1/L image kept), the PILfont header walk (descriptor line
        ; discarded, info lines collected, DATA marker, 256*20 bytes of
        ; big-endian int16 metrics), and the exact error shapes are
        ; Pillow-11.3.0-pinned; the native layer owns the glyph blitting and
        ; the mask surface (see pillow_c_codec_font.cpp).
        static Load(filename) {
            if !(filename is String)
                throw Error("Pillow.ImageFont.Load filename expects a string", -1)
            if !FileExist(filename)
                throw Error("[Errno 2] No such file or directory: '" filename "'", -1)
            root := filename
            dot := InStr(root, ".")
            if dot
                root := SubStr(root, 1, dot - 1)
            glyphImage := ""
            glyphPath := ""
            for ext in [".png", ".gif", ".pbm"] {
                candidate := root ext
                if !FileExist(candidate)
                    continue
                try {
                    opened := Pillow.Image.Open(candidate)
                    if opened.Mode = "1" || opened.Mode = "L" {
                        glyphImage := opened
                        glyphPath := candidate
                        break
                    }
                    opened.Close()
                } catch {
                }
            }
            if glyphImage = ""
                throw Error("cannot find glyph data file " root ".{gif|pbm|png}", -1)
            file := FileOpen(filename, "r")
            firstLine := ""
            infoLines := []
            metrics := Buffer(0, 0)
            try {
                firstLine := file.ReadLine()
                if firstLine != "PILfont"
                    throw Error("Not a PILfont file", -1)
                file.ReadLine() ; the fontdescriptor line is split and ignored
                while !file.AtEOF {
                    line := file.ReadLine()
                    if line = "DATA"
                        break
                    infoLines.Push(line "`n") ; Pillow keeps the raw bytes with LF
                }
                metrics := Buffer(5120, 0)
                file.RawRead(metrics, 5120)
            } finally {
                file.Close()
            }
            handle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_font_load_pil",
                "Ptr", metrics,
                "UPtr", metrics.Size,
                "Ptr", glyphImage.RequireHandle(),
                "Ptr*", &handle,
                "Int"
            ))
            glyphImage.Close()
            return Pillow.ImageFont.ImageFont(handle, {
                File: glyphPath,
                Info: infoLines,
            })
        }
        ; Pillow's load() resolves to Load above (AHK names are
        ; case-insensitive).

        ; ImageFont.load_path: searches for the bitmap font along sys.path.
        ; The AHK runtime has no sys.path, so the script directory and the
        ; working directory stand in (a documented approximation); a miss
        ; keeps Pillow's exact OSError shape with the did-you-mean hint when
        ; the file exists relative to the working directory.
        static LoadPath(filename) {
            name := filename is String ? filename : StrGet(filename.Ptr, "UTF-8")
            for dir in [A_ScriptDir, A_WorkingDir] {
                candidate := dir "\" name
                if !FileExist(candidate)
                    continue
                try
                    return Pillow.ImageFont.Load(candidate)
                catch {
                }
            }
            if FileExist(name)
                throw Error('cannot find font file "' name '" in sys.path, did you mean ImageFont.load("' name '") instead?', -1)
            throw Error('cannot find font file "' name '" in sys.path', -1)
        }

        static load_path(filename) {
            return Pillow.ImageFont.LoadPath(filename)
        }

        ; ImageFont.load_default_imagefont: Pillow's bundled courB08 bitmap
        ; font. The same metrics + glyph PNG that Pillow embeds ship as
        ; ahk/fonts/courB08.pil and ahk/fonts/courB08.png; the load flow is
        ; identical to load() (Pillow's embedded variant skips setting `file`
        ; — a recorded micro-divergence). AHK case-insensitivity serves the
        ; snake_case name too.
        static LoadDefaultImagefont() {
            SplitPath A_LineFile, , &pilAhkDir
            pilPath := pilAhkDir "\fonts\courB08.pil"
            if !FileExist(pilPath)
                throw Error("[Errno 2] No such file or directory: '" pilPath "'", -1)
            return Pillow.ImageFont.Load(pilPath)
        }

        ; BEHAV-FONTFILE-002: Pillow's ImageFont.ImageFont bitmap class. The
        ; surface is getmask/getbbox/getlength plus the file/info attributes;
        ; masks mirror the glyph-image mode (mode 1 stays a packed-mode-1
        ; image, L stays L) and the src/dst box-size mismatch keeps Pillow's
        ; exact SystemError message.
        class ImageFont {
            __New(handle, meta := unset) {
                if handle = 0
                    throw Error("pillow_c returned a null font handle", -2)
                this.Handle := handle
                if IsSet(meta) {
                    this.File := meta.File
                    this.Info := meta.Info
                }
            }

            __Delete() {
                this.Close()
            }

            Close() {
                handle := this.HasOwnProp("Handle") ? this.Handle : 0
                if handle {
                    this.Handle := 0
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_font_free",
                        "Ptr", handle,
                        "Int"
                    ))
                }
            }

            RequireHandle() {
                if !this.Handle
                    throw Error("Pillow.ImageFont font is closed", -1)
                return this.Handle
            }

            GetMask(text, mode := "") {
                if !(text is String)
                    throw Error("Pillow.ImageFont.GetMask text expects a string", -1)
                Pillow.ImageFont.CheckStringLength(text)
                textBytes := Pillow.Image.Utf8Buffer(text)
                modeBytes := Pillow.Image.Utf8Buffer(mode)
                outHandle := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_font_getmask",
                    "Ptr", this.RequireHandle(),
                    "Ptr", textBytes,
                    "Ptr", modeBytes,
                    "Int", 0,
                    "Ptr*", &outHandle,
                    "Int"
                )
                if status = -64
                    throw Error("<method 'getmask' of 'ImagingFont' objects> returned a result with an exception set", -1)
                Pillow.CheckStatus(status)
                return Pillow.WrapImageHandle(outHandle)
            }

            GetBbox(text, args*) {
                if !(text is String)
                    throw Error("Pillow.ImageFont.GetBbox text expects a string", -1)
                Pillow.ImageFont.CheckStringLength(text)
                textBytes := Pillow.Image.Utf8Buffer(text)
                left := 0
                top := 0
                right := 0
                bottom := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_font_getbbox",
                    "Ptr", this.RequireHandle(),
                    "Ptr", textBytes,
                    "Int*", &left,
                    "Int*", &top,
                    "Int*", &right,
                    "Int*", &bottom,
                    "Int"
                ))
                return [left, top, right, bottom]
            }

            GetLength(text, args*) {
                if !(text is String)
                    throw Error("Pillow.ImageFont.GetLength text expects a string", -1)
                Pillow.ImageFont.CheckStringLength(text)
                textBytes := Pillow.Image.Utf8Buffer(text)
                length := 0.0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_font_getlength",
                    "Ptr", this.RequireHandle(),
                    "Ptr", textBytes,
                    "Double*", &length,
                    "Int"
                ))
                return length
            }
        }

        class FreeTypeFont {
            __New(handle, meta := unset) {
                if handle = 0
                    throw Error("pillow_c returned a null font handle", -2)
                this.Handle := handle
                if IsSet(meta) {
                    ; BEHAV-FONTFILE-001: the truetype source metadata for
                    ; FontVariant re-loads (Pillow keeps path/size/index/
                    ; encoding/layout_engine on the FreeTypeFont object).
                    this.Path := meta.Path
                    this.Size := meta.Size
                    this.Index := meta.Index
                    this.Encoding := meta.Encoding
                    this.LayoutEngine := meta.LayoutEngine
                }
            }

            __Delete() {
                this.Close()
            }

            Close() {
                handle := this.HasOwnProp("Handle") ? this.Handle : 0
                if handle {
                    this.Handle := 0
                    Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_font_free", "Ptr", handle, "Int"))
                }
            }

            RequireHandle() {
                if !this.Handle
                    throw Error("Pillow.ImageFont font is closed", -1)
                return this.Handle
            }

            GetLength(text, mode := "", direction := unset, features := unset, language := unset) {
                ; mode/direction/features/language require libraqm; this
                ; runtime implements the default ltr path and accepts them
                ; (a documented boundary, like a raqm-less Pillow build).
                if !(text is String)
                    throw Error("Pillow.ImageFont.GetLength text expects a string", -1)
                Pillow.ImageFont.CheckStringLength(text)
                textBytes := Pillow.Image.Utf8Buffer(text)
                length := 0.0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_font_getlength",
                    "Ptr", this.RequireHandle(),
                    "Ptr", textBytes,
                    "Double*", &length,
                    "Int"
                ))
                return length
            }

            GetMask(text, mode := "", ink := 0, direction := unset, features := unset, language := unset, strokeWidth := 0, anchor := unset, start := unset) {
                ; BEHAV-FONT-002: the default-font glyph coverage mask; and
                ; BEHAV-FONTFILE-001: the GDI-rendered truetype grayscale
                ; mask. For truetype fonts Pillow renders mode ""/"L"/"1"/any
                ; unknown mode as an L image (mode "1" with mono-rounded
                ; metrics) and "RGBA" as an RGBA image whose alpha scales
                ; with ink; the GDI pixel values are the documented
                ; rasterizer divergence. strokeWidth/anchor/start are
                ; accepted-and-ignored (libraqm/FreeType-stroke boundaries).
                if !(text is String)
                    throw Error("Pillow.ImageFont.GetMask text expects a string", -1)
                Pillow.ImageFont.CheckStringLength(text)
                textBytes := Pillow.Image.Utf8Buffer(text)
                modeBytes := Pillow.Image.Utf8Buffer(mode)
                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_font_getmask",
                    "Ptr", this.RequireHandle(),
                    "Ptr", textBytes,
                    "Ptr", modeBytes,
                    "Int", ink,
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return Pillow.WrapImageHandle(outHandle)
            }

            GetMask2(text, mode := "", ink := 0, args*) {
                ; Pillow's getmask2 returns (mask, offset) with the offset
                ; from font.getsize (the bbox top-left corner).
                mask := this.GetMask(text, mode, ink)
                bbox := this.GetBbox(text)
                return [mask, [bbox[1], bbox[2]]]
            }

            GetBbox(text, anchor := unset, mode := "", direction := unset, features := unset, language := unset, strokeWidth := 0) {
                ; The anchor is Pillow's second-to-last parameter but the
                ; facade keeps it positional-second for compatibility with
                ; the pre-truetype callers. mode/direction/features/language
                ; are accepted-and-ignored; strokeWidth expands the box like
                ; Pillow's getbbox.
                if !(text is String)
                    throw Error("Pillow.ImageFont.GetBbox text expects a string", -1)
                Pillow.ImageFont.CheckStringLength(text)
                textBytes := Pillow.Image.Utf8Buffer(text)
                anchorBytes := 0
                if IsSet(anchor) {
                    if !(anchor is String)
                        throw Error("Pillow.ImageFont.GetBbox anchor expects a string", -1)
                    anchorBytes := Pillow.Image.Utf8Buffer(anchor)
                }
                left := 0
                top := 0
                right := 0
                bottom := 0
                if IsSet(anchor) {
                    status := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_font_getbbox_anchor",
                        "Ptr", this.RequireHandle(),
                        "Ptr", textBytes,
                        "Ptr", anchorBytes,
                        "Int*", &left,
                        "Int*", &top,
                        "Int*", &right,
                        "Int*", &bottom,
                        "Int"
                    )
                    if status = -3
                        throw Error("bad anchor specified: " anchor, -1)
                    Pillow.CheckStatus(status)
                } else {
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_font_getbbox",
                        "Ptr", this.RequireHandle(),
                        "Ptr", textBytes,
                        "Int*", &left,
                        "Int*", &top,
                        "Int*", &right,
                        "Int*", &bottom,
                        "Int"
                    ))
                }
                if strokeWidth > 0
                    return [left - strokeWidth, top - strokeWidth, right + strokeWidth, bottom + strokeWidth]
                return [left, top, right, bottom]
            }

            GetMetrics() {
                ascent := 0
                descent := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_font_getmetrics",
                    "Ptr", this.RequireHandle(),
                    "Int*", &ascent,
                    "Int*", &descent,
                    "Int"
                ))
                return [ascent, descent]
            }

            GetName() {
                familyRequired := 0
                styleRequired := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_font_getname",
                    "Ptr", this.RequireHandle(),
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &familyRequired,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &styleRequired,
                    "Int"
                )
                if status != -1 || familyRequired <= 0 || styleRequired <= 0
                    Pillow.CheckStatus(status)

                family := Buffer(familyRequired, 0)
                style := Buffer(styleRequired, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_font_getname",
                    "Ptr", this.RequireHandle(),
                    "Ptr", family,
                    "UPtr", family.Size,
                    "UPtr*", &familyRequired,
                    "Ptr", style,
                    "UPtr", style.Size,
                    "UPtr*", &styleRequired,
                    "Int"
                ))
                return [
                    StrGet(family.Ptr, familyRequired - 1, "UTF-8"),
                    StrGet(style.Ptr, styleRequired - 1, "UTF-8"),
                ]
            }

            FontVariant(font := unset, size := unset, index := unset, encoding := unset, layoutEngine := unset) {
                ; BEHAV-FONTFILE-001: Pillow's font_variant re-opens the
                ; source with the overrides (layout_engine falls back to the
                ; current engine when 0/unset, mirroring `layout_engine or
                ; self.layout_engine`). The default bitmap font keeps the
                ; native handle duplication.
                if !this.HasOwnProp("Path")
                    return this.DuplicateHandle()
                newFont := IsSet(font) ? font : this.Path
                newSize := IsSet(size) ? size : this.Size
                newIndex := IsSet(index) ? index : this.Index
                newEncoding := IsSet(encoding) ? encoding : this.Encoding
                newEngine := (IsSet(layoutEngine) && layoutEngine != 0) ? layoutEngine : this.LayoutEngine
                return Pillow.ImageFont.Truetype(newFont, newSize, newIndex, newEncoding, newEngine)
            }

            DuplicateHandle() {
                handle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_font_variant",
                    "Ptr", this.RequireHandle(),
                    "Ptr*", &handle,
                    "Int"
                ))
                return Pillow.ImageFont.FreeTypeFont(handle)
            }

            font_variant(font := unset, size := unset, index := unset, encoding := unset, layoutEngine := unset) {
                if IsSet(font) || IsSet(size) || IsSet(index) || IsSet(encoding) || IsSet(layoutEngine)
                    return this.FontVariant(font, size, index, encoding, layoutEngine)
                return this.FontVariant()
            }

            IsVariable() {
                variable := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_font_is_variable",
                    "Ptr", this.RequireHandle(),
                    "Int*", &variable,
                    "Int"
                ))
                return variable != 0
            }

            ; BEHAV-FONTFILE-001: the variation surface. Non-variable fonts
            ; keep Pillow's exact OSError("invalid argument"); variable-font
            ; axes are the documented boundary (Pillow works there through
            ; FreeType).
            GetVariationAxes() {
                if !this.IsVariable()
                    throw Error("invalid argument", -1)
                throw Error("variable font axes are not supported by this runtime", -1)
            }

            get_variation_axes() {
                return this.GetVariationAxes()
            }

            GetVariationNames() {
                if !this.IsVariable()
                    throw Error("invalid argument", -1)
                throw Error("variable font names are not supported by this runtime", -1)
            }

            get_variation_names() {
                return this.GetVariationNames()
            }

            SetVariationByAxes(axes) {
                if !this.IsVariable()
                    throw Error("invalid argument", -1)
                throw Error("variable font axes are not supported by this runtime", -1)
            }

            set_variation_by_axes(axes) {
                this.SetVariationByAxes(axes)
            }

            SetVariationByName(name) {
                if !this.IsVariable()
                    throw Error("invalid argument", -1)
                throw Error("variable font names are not supported by this runtime", -1)
            }

            set_variation_by_name(name) {
                this.SetVariationByName(name)
            }
        }

        class TransposedFont {
            ; API-FONTVAR-001: Pillow 11.3.0's ImageFont.TransposedFont
            ; wrapper for rotated/mirrored text. The orientation default
            ; uses "" as the None analogue. GetMask stays a documented
            ; boundary (the AHK runtime rasterizes text through the native
            ; draw seam and exposes no mask objects); Pillow's Axis is a
            ; type-only TypedDict and is recorded as a boundary name.
            __New(font, orientation := unset) {
                this.Font := font
                this.Orientation := IsSet(orientation) ? orientation : ""
            }

            GetMask(text, mode := "", args*) {
                ; Pillow returns the wrapped font's mask, transposed by the
                ; orientation when one is set.
                mask := this.Font.GetMask(text, mode, args*)
                if this.Orientation != ""
                    return mask.Transpose(this.Orientation)
                return mask
            }

            GetBbox(text, anchor := unset) {
                bbox := IsSet(anchor) ? this.Font.GetBbox(text, anchor) : this.Font.GetBbox(text)
                width := bbox[3] - bbox[1]
                height := bbox[4] - bbox[2]
                if this.Orientation = Pillow.Transpose.ROTATE_90 || this.Orientation = Pillow.Transpose.ROTATE_270
                    return [0, 0, height, width]
                return [0, 0, width, height]
            }

            GetLength(text) {
                if this.Orientation = Pillow.Transpose.ROTATE_90 || this.Orientation = Pillow.Transpose.ROTATE_270
                    throw Error("text length is undefined for text rotated by 90 or 270 degrees", -1)
                return this.Font.GetLength(text)
            }
        }

        class Layout {
            static BASIC := 0
            static RAQM := 1
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
            emptyNumericValue := (
                IsObject(value)
                && value is Array
                && value.Length = 0
                && (image.Mode = "I" || image.Mode = "F")
            )
            if !emptyNumericValue && !(thresh is Number)
                throw Error("Pillow.ImageDraw.Floodfill thresh must be numeric", -1)
            nativeThresh := emptyNumericValue ? 0.0 : thresh
            if emptyNumericValue {
                valueBuffer := Buffer(1, 0)
                valueSize := 0
            } else {
                if IsObject(value) && value.Length != 1 {
                    if image.Mode = "I"
                        throw Error("color must be int or single-element tuple", -1)
                    if image.Mode = "F"
                        throw Error("must be real number, not tuple", -1)
                }
                valueBuffer := image.PasteColorBuffer(value)
                valueSize := valueBuffer.Size
            }
            borderPtr := 0
            borderSize := 0
            borderBuffer := 0
            if IsSet(border) && !emptyNumericValue {
                if IsObject(border) && border is Array && (image.Mode = "I" || image.Mode = "F") {
                    borderBuffer := Buffer(1, 0)
                    borderPtr := borderBuffer.Ptr
                } else {
                    borderBuffer := image.PasteColorBuffer(border)
                    borderPtr := borderBuffer.Ptr
                    borderSize := borderBuffer.Size
                }
            }

            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_draw_floodfill",
                "Ptr", image.RequireHandle(),
                "Int", xy[1],
                "Int", xy[2],
                "Ptr", valueBuffer,
                "UPtr", valueSize,
                "Ptr", borderPtr,
                "UPtr", borderSize,
                "Double", nativeThresh,
                "Int"
            ))
            return image
        }

        static TextAlignId(align, operationName) {
            if align = "left"
                return 0
            if align = "center"
                return 1
            if align = "right"
                return 2
            if align = "justify"
                return 3
            throw Error("Pillow.ImageDraw." operationName ' align must be "left", "center", "right" or "justify"', -1)
        }

        static ValidateMultilineAnchor(anchor) {
            if StrLen(anchor) != 2
                throw Error("anchor must be a 2 character string", -1)
            horizontal := SubStr(anchor, 1, 1)
            vertical := SubStr(anchor, 2, 1)
            if !InStr("lmr", horizontal) || !InStr("atmbds", vertical)
                throw Error("bad anchor specified: " anchor, -1)
            if vertical = "t" || vertical = "b"
                throw Error("anchor not supported for multiline text", -1)
        }

        static StrokeWidth(strokeWidth, operationName) {
            if !(strokeWidth is Integer)
                throw Error("Pillow.ImageDraw." operationName " strokeWidth must be an integer", -1)
            if strokeWidth < 0
                throw Error("Pillow.ImageDraw." operationName " strokeWidth must be >= 0", -1)
            return strokeWidth
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
                    if IsObject(fill) && fill.Length != 1 {
                        if this.Image.Mode = "I"
                            throw Error("color must be int or single-element tuple", -1)
                        if this.Image.Mode = "F"
                            throw Error("must be real number, not tuple", -1)
                    }
                    fillBuffer := this.Image.PasteColorBuffer(fill)
                    fillPtr := fillBuffer.Ptr
                    fillSize := fillBuffer.Size
                }

                outlinePtr := 0
                outlineSize := 0
                outlineBuffer := 0
                if IsSet(outline) {
                    if IsObject(outline) && outline.Length != 1 {
                        if this.Image.Mode = "I"
                            throw Error("color must be int or single-element tuple", -1)
                        if this.Image.Mode = "F"
                            throw Error("must be real number, not tuple", -1)
                    }
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
                    if IsObject(fill) && fill.Length != 1 {
                        if this.Image.Mode = "I"
                            throw Error("color must be int or single-element tuple", -1)
                        if this.Image.Mode = "F"
                            throw Error("must be real number, not tuple", -1)
                    }
                    fillBuffer := this.Image.PasteColorBuffer(fill)
                    fillPtr := fillBuffer.Ptr
                    fillSize := fillBuffer.Size
                }

                outlinePtr := 0
                outlineSize := 0
                outlineBuffer := 0
                if IsSet(outline) {
                    if IsObject(outline) && outline.Length != 1 {
                        if this.Image.Mode = "I"
                            throw Error("color must be int or single-element tuple", -1)
                        if this.Image.Mode = "F"
                            throw Error("must be real number, not tuple", -1)
                    }
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

                colorValue := IsSet(fill) ? fill : 0
                if IsObject(colorValue) && colorValue.Length != 1 {
                    if this.Image.Mode = "I"
                        throw Error("color must be int or single-element tuple", -1)
                    if this.Image.Mode = "F"
                        throw Error("must be real number, not tuple", -1)
                }
                color := this.Image.PasteColorBuffer(colorValue)
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
                    if IsObject(fill) && fill.Length != 1 {
                        if this.Image.Mode = "I"
                            throw Error("color must be int or single-element tuple", -1)
                        if this.Image.Mode = "F"
                            throw Error("must be real number, not tuple", -1)
                    }
                    fillBuffer := this.Image.PasteColorBuffer(fill)
                    fillPtr := fillBuffer.Ptr
                    fillSize := fillBuffer.Size
                }

                outlinePtr := 0
                outlineSize := 0
                outlineBuffer := 0
                if IsSet(outline) {
                    if IsObject(outline) && outline.Length != 1 {
                        if this.Image.Mode = "I"
                            throw Error("color must be int or single-element tuple", -1)
                        if this.Image.Mode = "F"
                            throw Error("must be real number, not tuple", -1)
                    }
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
                    if IsObject(fill) && fill.Length != 1 {
                        if this.Image.Mode = "I"
                            throw Error("color must be int or single-element tuple", -1)
                        if this.Image.Mode = "F"
                            throw Error("must be real number, not tuple", -1)
                    }
                    fillBuffer := this.Image.PasteColorBuffer(fill)
                    fillPtr := fillBuffer.Ptr
                    fillSize := fillBuffer.Size
                }

                outlinePtr := 0
                outlineSize := 0
                outlineBuffer := 0
                if IsSet(outline) {
                    if IsObject(outline) && outline.Length != 1 {
                        if this.Image.Mode = "I"
                            throw Error("color must be int or single-element tuple", -1)
                        if this.Image.Mode = "F"
                            throw Error("must be real number, not tuple", -1)
                    }
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
                    if IsObject(fill) && fill.Length != 1 {
                        if this.Image.Mode = "I"
                            throw Error("color must be int or single-element tuple", -1)
                        if this.Image.Mode = "F"
                            throw Error("must be real number, not tuple", -1)
                    }
                    fillBuffer := this.Image.PasteColorBuffer(fill)
                    fillPtr := fillBuffer.Ptr
                    fillSize := fillBuffer.Size
                }

                outlinePtr := 0
                outlineSize := 0
                outlineBuffer := 0
                if IsSet(outline) {
                    if IsObject(outline) && outline.Length != 1 {
                        if this.Image.Mode = "I"
                            throw Error("color must be int or single-element tuple", -1)
                        if this.Image.Mode = "F"
                            throw Error("must be real number, not tuple", -1)
                    }
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

            rounded_rectangle(box, radius := 0, fill := unset, outline := unset, width := 1, corners := unset) {
                if IsSet(corners)
                    return this.RoundedRectangle(box, radius, IsSet(fill) ? fill : unset, IsSet(outline) ? outline : unset, width, corners)
                return this.RoundedRectangle(box, radius, IsSet(fill) ? fill : unset, IsSet(outline) ? outline : unset, width)
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

                if IsObject(fill) && fill.Length != 1 {
                    if this.Image.Mode = "I"
                        throw Error("color must be int or single-element tuple", -1)
                    if this.Image.Mode = "F"
                        throw Error("must be real number, not tuple", -1)
                }
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

            Text(xy, text, fill := unset, font := unset, anchor := unset, strokeWidth := 0, strokeFill := unset) {
                if !IsObject(xy) || xy.Length != 2
                    throw Error("Pillow.ImageDraw.Text xy expects [x, y]", -1)
                if !(xy[1] is Number) || !(xy[2] is Number)
                    throw Error("Pillow.ImageDraw.Text xy coordinates must be numeric", -1)
                if !(text is String)
                    throw Error("Pillow.ImageDraw.Text text expects a string", -1)
                strokeWidth := Pillow.ImageDraw.StrokeWidth(strokeWidth, "Text")

                textBytes := Pillow.Image.Utf8Buffer(text)
                anchorBytes := 0
                if IsSet(anchor) {
                    if !(anchor is String)
                        throw Error("Pillow.ImageDraw.Text anchor expects a string", -1)
                    anchorBytes := Pillow.Image.Utf8Buffer(anchor)
                }
                color := this.Image.PasteColorBuffer(IsSet(fill) ? fill : 0)
                strokeColor := 0
                if strokeWidth > 0
                    strokeColor := this.Image.PasteColorBuffer(IsSet(strokeFill) ? strokeFill : (IsSet(fill) ? fill : 0))
                if IsSet(font) {
                    if !(IsObject(font) && font is Pillow.ImageFont.FreeTypeFont)
                        throw Error("Pillow.ImageDraw.Text currently supports only Pillow.ImageFont fonts", -1)
                    if strokeWidth > 0 && IsSet(anchor) {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_text_font_anchor_stroke",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Ptr", anchorBytes,
                            "Int", strokeWidth,
                            "Ptr", strokeColor,
                            "UPtr", strokeColor.Size,
                            "Int"
                        ))
                    } else if strokeWidth > 0 {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_text_font_stroke",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int", strokeWidth,
                            "Ptr", strokeColor,
                            "UPtr", strokeColor.Size,
                            "Int"
                        ))
                    } else if IsSet(anchor) {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_text_font_anchor",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Ptr", anchorBytes,
                            "Int"
                        ))
                    } else {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_text_font",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int"
                        ))
                    }
                } else {
                    if strokeWidth > 0 && IsSet(anchor) {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_text_anchor_stroke",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Ptr", anchorBytes,
                            "Int", strokeWidth,
                            "Ptr", strokeColor,
                            "UPtr", strokeColor.Size,
                            "Int"
                        ))
                    } else if strokeWidth > 0 {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_text_stroke",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int", strokeWidth,
                            "Ptr", strokeColor,
                            "UPtr", strokeColor.Size,
                            "Int"
                        ))
                    } else if IsSet(anchor) {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_text_anchor",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Ptr", anchorBytes,
                            "Int"
                        ))
                    } else {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_text",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int"
                        ))
                    }
                }
                return this
            }

            TextLength(text, font := unset) {
                if !(text is String)
                    throw Error("Pillow.ImageDraw.TextLength text expects a string", -1)
                if IsSet(font) {
                    if !(IsObject(font) && font is Pillow.ImageFont.FreeTypeFont)
                        throw Error("Pillow.ImageDraw.TextLength currently supports only Pillow.ImageFont fonts", -1)
                    return font.GetLength(text)
                }

                textBytes := Pillow.Image.Utf8Buffer(text)
                length := 0.0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_textlength",
                    "Ptr", textBytes,
                    "Double*", &length,
                    "Int"
                ))
                return length
            }

            TextBbox(xy, text, font := unset, anchor := unset, strokeWidth := 0) {
                if !IsObject(xy) || xy.Length != 2
                    throw Error("Pillow.ImageDraw.TextBbox xy expects [x, y]", -1)
                if !(xy[1] is Number) || !(xy[2] is Number)
                    throw Error("Pillow.ImageDraw.TextBbox xy coordinates must be numeric", -1)
                if !(text is String)
                    throw Error("Pillow.ImageDraw.TextBbox text expects a string", -1)
                strokeWidth := Pillow.ImageDraw.StrokeWidth(strokeWidth, "TextBbox")

                textBytes := Pillow.Image.Utf8Buffer(text)
                anchorBytes := 0
                if IsSet(anchor) {
                    if !(anchor is String)
                        throw Error("Pillow.ImageDraw.TextBbox anchor expects a string", -1)
                    anchorBytes := Pillow.Image.Utf8Buffer(anchor)
                }
                left := 0
                top := 0
                right := 0
                bottom := 0
                if IsSet(font) {
                    if !(IsObject(font) && font is Pillow.ImageFont.FreeTypeFont)
                        throw Error("Pillow.ImageDraw.TextBbox currently supports only Pillow.ImageFont fonts", -1)
                    if strokeWidth > 0 && IsSet(anchor) {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_textbbox_font_anchor_stroke",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Ptr", anchorBytes,
                            "Int", strokeWidth,
                            "Int*", &left,
                            "Int*", &top,
                            "Int*", &right,
                            "Int*", &bottom,
                            "Int"
                        ))
                        return [left, top, right, bottom]
                    }
                    if strokeWidth > 0 {
                        bbox := font.GetBbox(text)
                        return [
                            xy[1] + bbox[1] - strokeWidth,
                            xy[2] + bbox[2] - strokeWidth,
                            xy[1] + bbox[3] + strokeWidth,
                            xy[2] + bbox[4] + strokeWidth
                        ]
                    }
                    if IsSet(anchor) {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_textbbox_font_anchor",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Ptr", anchorBytes,
                            "Int*", &left,
                            "Int*", &top,
                            "Int*", &right,
                            "Int*", &bottom,
                            "Int"
                        ))
                        return [left, top, right, bottom]
                    }
                    bbox := font.GetBbox(text)
                    return [xy[1] + bbox[1], xy[2] + bbox[2], xy[1] + bbox[3], xy[2] + bbox[4]]
                } else {
                    if strokeWidth > 0 && IsSet(anchor) {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_textbbox_anchor_stroke",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", anchorBytes,
                            "Int", strokeWidth,
                            "Int*", &left,
                            "Int*", &top,
                            "Int*", &right,
                            "Int*", &bottom,
                            "Int"
                        ))
                    } else if strokeWidth > 0 {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_textbbox_stroke",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Int", strokeWidth,
                            "Int*", &left,
                            "Int*", &top,
                            "Int*", &right,
                            "Int*", &bottom,
                            "Int"
                        ))
                    } else if IsSet(anchor) {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_textbbox_anchor",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", anchorBytes,
                            "Int*", &left,
                            "Int*", &top,
                            "Int*", &right,
                            "Int*", &bottom,
                            "Int"
                        ))
                    } else {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_textbbox",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Int*", &left,
                            "Int*", &top,
                            "Int*", &right,
                            "Int*", &bottom,
                            "Int"
                        ))
                    }
                    return [left, top, right, bottom]
                }
            }

            MultilineText(xy, text, fill := unset, font := unset, spacing := 4, align := "left", anchor := unset, strokeWidth := 0, strokeFill := unset) {
                if !IsObject(xy) || xy.Length != 2
                    throw Error("Pillow.ImageDraw.MultilineText xy expects [x, y]", -1)
                if !(xy[1] is Number) || !(xy[2] is Number)
                    throw Error("Pillow.ImageDraw.MultilineText xy coordinates must be numeric", -1)
                if !(text is String)
                    throw Error("Pillow.ImageDraw.MultilineText text expects a string", -1)
                if !(spacing is Integer)
                    throw Error("Pillow.ImageDraw.MultilineText spacing must be an integer", -1)
                strokeWidth := Pillow.ImageDraw.StrokeWidth(strokeWidth, "MultilineText")

                alignId := Pillow.ImageDraw.TextAlignId(align, "MultilineText")
                textBytes := Pillow.Image.Utf8Buffer(text)
                anchorBytes := 0
                if IsSet(anchor) {
                    if !(anchor is String)
                        throw Error("Pillow.ImageDraw.MultilineText anchor expects a string", -1)
                    Pillow.ImageDraw.ValidateMultilineAnchor(anchor)
                    anchorBytes := Pillow.Image.Utf8Buffer(anchor)
                }
                color := this.Image.PasteColorBuffer(IsSet(fill) ? fill : 0)
                strokeColor := 0
                if strokeWidth > 0
                    strokeColor := this.Image.PasteColorBuffer(IsSet(strokeFill) ? strokeFill : (IsSet(fill) ? fill : 0))
                if IsSet(font) {
                    if !(IsObject(font) && font is Pillow.ImageFont.FreeTypeFont)
                        throw Error("Pillow.ImageDraw.MultilineText currently supports only Pillow.ImageFont fonts", -1)
                    if strokeWidth > 0 && IsSet(anchor) {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_multiline_text_font_anchor_stroke",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int", spacing,
                            "Int", alignId,
                            "Ptr", anchorBytes,
                            "Int", strokeWidth,
                            "Ptr", strokeColor,
                            "UPtr", strokeColor.Size,
                            "Int"
                        ))
                    } else if strokeWidth > 0 {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_multiline_text_font_align_stroke",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int", spacing,
                            "Int", alignId,
                            "Int", strokeWidth,
                            "Ptr", strokeColor,
                            "UPtr", strokeColor.Size,
                            "Int"
                        ))
                    } else if IsSet(anchor) {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_multiline_text_font_anchor",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int", spacing,
                            "Int", alignId,
                            "Ptr", anchorBytes,
                            "Int"
                        ))
                    } else if alignId = 0 {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_multiline_text_font",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int", spacing,
                            "Int"
                        ))
                    } else {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_multiline_text_font_align",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int", spacing,
                            "Int", alignId,
                            "Int"
                        ))
                    }
                } else {
                    if strokeWidth > 0 && IsSet(anchor) {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_multiline_text_anchor_stroke",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int", spacing,
                            "Int", alignId,
                            "Ptr", anchorBytes,
                            "Int", strokeWidth,
                            "Ptr", strokeColor,
                            "UPtr", strokeColor.Size,
                            "Int"
                        ))
                    } else if strokeWidth > 0 {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_multiline_text_align_stroke",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int", spacing,
                            "Int", alignId,
                            "Int", strokeWidth,
                            "Ptr", strokeColor,
                            "UPtr", strokeColor.Size,
                            "Int"
                        ))
                    } else if IsSet(anchor) {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_multiline_text_anchor",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int", spacing,
                            "Int", alignId,
                            "Ptr", anchorBytes,
                            "Int"
                        ))
                    } else if alignId = 0 {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_multiline_text",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int", spacing,
                            "Int"
                        ))
                    } else {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_draw_multiline_text_align",
                            "Ptr", this.Image.RequireHandle(),
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", color,
                            "UPtr", color.Size,
                            "Int", spacing,
                            "Int", alignId,
                            "Int"
                        ))
                    }
                }
                return this
            }

            multiline_text(xy, text, fill := unset, font := unset, spacing := 4, align := "left", anchor := unset, strokeWidth := 0, strokeFill := unset) {
                return this.MultilineText(
                    xy,
                    text,
                    IsSet(fill) ? fill : unset,
                    IsSet(font) ? font : unset,
                    spacing,
                    align,
                    IsSet(anchor) ? anchor : unset,
                    strokeWidth,
                    IsSet(strokeFill) ? strokeFill : unset)
            }

            MultilineTextBbox(xy, text, font := unset, spacing := 4, align := "left", anchor := unset, strokeWidth := 0) {
                if !IsObject(xy) || xy.Length != 2
                    throw Error("Pillow.ImageDraw.MultilineTextBbox xy expects [x, y]", -1)
                if !(xy[1] is Number) || !(xy[2] is Number)
                    throw Error("Pillow.ImageDraw.MultilineTextBbox xy coordinates must be numeric", -1)
                if !(text is String)
                    throw Error("Pillow.ImageDraw.MultilineTextBbox text expects a string", -1)
                if !(spacing is Integer)
                    throw Error("Pillow.ImageDraw.MultilineTextBbox spacing must be an integer", -1)
                strokeWidth := Pillow.ImageDraw.StrokeWidth(strokeWidth, "MultilineTextBbox")

                alignId := Pillow.ImageDraw.TextAlignId(align, "MultilineTextBbox")
                textBytes := Pillow.Image.Utf8Buffer(text)
                anchorBytes := 0
                if IsSet(anchor) {
                    if !(anchor is String)
                        throw Error("Pillow.ImageDraw.MultilineTextBbox anchor expects a string", -1)
                    Pillow.ImageDraw.ValidateMultilineAnchor(anchor)
                    anchorBytes := Pillow.Image.Utf8Buffer(anchor)
                }
                left := 0
                top := 0
                right := 0
                bottom := 0
                if IsSet(font) {
                    if !(IsObject(font) && font is Pillow.ImageFont.FreeTypeFont)
                        throw Error("Pillow.ImageDraw.MultilineTextBbox currently supports only Pillow.ImageFont fonts", -1)
                    if strokeWidth > 0 && IsSet(anchor) {
                        left := 0.0
                        top := 0.0
                        right := 0.0
                        bottom := 0.0
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_multiline_textbbox_font_anchor_stroke_f64",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Int", spacing,
                            "Int", alignId,
                            "Ptr", anchorBytes,
                            "Int", strokeWidth,
                            "Double*", &left,
                            "Double*", &top,
                            "Double*", &right,
                            "Double*", &bottom,
                            "Int"
                        ))
                    } else if strokeWidth > 0 && alignId = 0 {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_multiline_textbbox_font_align_stroke",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Int", spacing,
                            "Int", alignId,
                            "Int", strokeWidth,
                            "Int*", &left,
                            "Int*", &top,
                            "Int*", &right,
                            "Int*", &bottom,
                            "Int"
                        ))
                    } else if strokeWidth > 0 {
                        left := 0.0
                        top := 0.0
                        right := 0.0
                        bottom := 0.0
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_multiline_textbbox_font_align_stroke_f64",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Int", spacing,
                            "Int", alignId,
                            "Int", strokeWidth,
                            "Double*", &left,
                            "Double*", &top,
                            "Double*", &right,
                            "Double*", &bottom,
                            "Int"
                        ))
                    } else if IsSet(anchor) {
                        left := 0.0
                        top := 0.0
                        right := 0.0
                        bottom := 0.0
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_multiline_textbbox_font_anchor_f64",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Int", spacing,
                            "Int", alignId,
                            "Ptr", anchorBytes,
                            "Double*", &left,
                            "Double*", &top,
                            "Double*", &right,
                            "Double*", &bottom,
                            "Int"
                        ))
                    } else if alignId = 0 {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_multiline_textbbox_font",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Int", spacing,
                            "Int*", &left,
                            "Int*", &top,
                            "Int*", &right,
                            "Int*", &bottom,
                            "Int"
                        ))
                    } else {
                        left := 0.0
                        top := 0.0
                        right := 0.0
                        bottom := 0.0
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_multiline_textbbox_font_align_f64",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Ptr", font.RequireHandle(),
                            "Int", spacing,
                            "Int", alignId,
                            "Double*", &left,
                            "Double*", &top,
                            "Double*", &right,
                            "Double*", &bottom,
                            "Int"
                        ))
                    }
                } else {
                    if strokeWidth > 0 && IsSet(anchor) {
                        left := 0.0
                        top := 0.0
                        right := 0.0
                        bottom := 0.0
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_multiline_textbbox_anchor_stroke_f64",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Int", spacing,
                            "Int", alignId,
                            "Ptr", anchorBytes,
                            "Int", strokeWidth,
                            "Double*", &left,
                            "Double*", &top,
                            "Double*", &right,
                            "Double*", &bottom,
                            "Int"
                        ))
                    } else if strokeWidth > 0 && alignId = 0 {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_multiline_textbbox_align_stroke",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Int", spacing,
                            "Int", alignId,
                            "Int", strokeWidth,
                            "Int*", &left,
                            "Int*", &top,
                            "Int*", &right,
                            "Int*", &bottom,
                            "Int"
                        ))
                    } else if strokeWidth > 0 {
                        left := 0.0
                        top := 0.0
                        right := 0.0
                        bottom := 0.0
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_multiline_textbbox_align_stroke_f64",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Int", spacing,
                            "Int", alignId,
                            "Int", strokeWidth,
                            "Double*", &left,
                            "Double*", &top,
                            "Double*", &right,
                            "Double*", &bottom,
                            "Int"
                        ))
                    } else if IsSet(anchor) {
                        left := 0.0
                        top := 0.0
                        right := 0.0
                        bottom := 0.0
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_multiline_textbbox_anchor_f64",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Int", spacing,
                            "Int", alignId,
                            "Ptr", anchorBytes,
                            "Double*", &left,
                            "Double*", &top,
                            "Double*", &right,
                            "Double*", &bottom,
                            "Int"
                        ))
                    } else if alignId = 0 {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_multiline_textbbox",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Int", spacing,
                            "Int*", &left,
                            "Int*", &top,
                            "Int*", &right,
                            "Int*", &bottom,
                            "Int"
                        ))
                    } else {
                        left := 0.0
                        top := 0.0
                        right := 0.0
                        bottom := 0.0
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_multiline_textbbox_align_f64",
                            "Int", xy[1],
                            "Int", xy[2],
                            "Ptr", textBytes,
                            "Int", spacing,
                            "Int", alignId,
                            "Double*", &left,
                            "Double*", &top,
                            "Double*", &right,
                            "Double*", &bottom,
                            "Int"
                        ))
                    }
                }
                return [left, top, right, bottom]
            }

            multiline_textbbox(xy, text, font := unset, spacing := 4, align := "left", anchor := unset, strokeWidth := 0) {
                return this.MultilineTextBbox(
                    xy,
                    text,
                    IsSet(font) ? font : unset,
                    spacing,
                    align,
                    IsSet(anchor) ? anchor : unset,
                    strokeWidth)
            }

            Line(xy, fill := unset, width := 0, joint := unset) {
                points := Pillow.ImageDraw.FlattenPoints(xy, "Line")
                colorValue := IsSet(fill) ? fill : 0
                if IsObject(colorValue) && colorValue.Length != 1 {
                    if this.Image.Mode = "I"
                        throw Error("color must be int or single-element tuple", -1)
                    if this.Image.Mode = "F"
                        throw Error("must be real number, not tuple", -1)
                }
                color := this.Image.PasteColorBuffer(colorValue)
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
                colorValue := IsSet(fill) ? fill : 0
                if IsObject(colorValue) && colorValue.Length != 1 {
                    if this.Image.Mode = "I"
                        throw Error("color must be int or single-element tuple", -1)
                    if this.Image.Mode = "F"
                        throw Error("must be real number, not tuple", -1)
                }
                color := this.Image.PasteColorBuffer(colorValue)
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

            regular_polygon(boundingCircle, n_sides, rotation := 0, fill := unset, outline := unset, width := 1) {
                return this.RegularPolygon(boundingCircle, n_sides, rotation, IsSet(fill) ? fill : unset, IsSet(outline) ? outline : unset, width)
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
                    size := imageOrList.Size
                    if !IsSet(mask) && (imageOrList.Mode = "I" || imageOrList.Mode = "F") && (size[1] = 0 || size[2] = 0)
                        throw Error("min/max not given", -1)
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
        if IsSet(options) && options.HasOwnProp("DllPath") {
            path := options.DllPath
            if Pillow.DllHandle && Pillow.DllPath != "" && path != Pillow.DllPath
                throw Error("Pillow.Configure cannot change DllPath after pillow_c.dll is loaded", -1)
            Pillow.DllPath := path
            Pillow.EnsureDllLoaded()
        }
    }

    static RequireDllPath() {
        if Pillow.DllPath = "" {
            SplitPath A_LineFile, , &ahkDir
            Pillow.DllPath := ahkDir "\..\build\x64\Release\pillow_c.dll"
        }
        Pillow.EnsureDllLoaded()
        return Pillow.DllPath
    }

    static EnsureDllLoaded() {
        if Pillow.DllHandle
            return Pillow.DllHandle
        handle := DllCall("kernel32\LoadLibraryW", "Str", Pillow.DllPath, "Ptr")
        if !handle
            throw Error("pillow_c: failed to load DLL " Pillow.DllPath " (GetLastError " A_LastError ")", -1)
        Pillow.DllHandle := handle
        return handle
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
        static ModeInfo(modeName) {
            modeName := modeName ""
            switch modeName {
                case "1":
                    return { BaseMode: "L", BaseType: "L", Bands: ["1"] }
                case "L":
                    return { BaseMode: "L", BaseType: "L", Bands: ["L"] }
                case "LA":
                    return { BaseMode: "L", BaseType: "L", Bands: ["L", "A"] }
                case "P":
                    return { BaseMode: "P", BaseType: "L", Bands: ["P"] }
                case "PA":
                    return { BaseMode: "RGB", BaseType: "L", Bands: ["P", "A"] }
                case "RGB":
                    return { BaseMode: "RGB", BaseType: "L", Bands: ["R", "G", "B"] }
                case "RGBA":
                    return { BaseMode: "RGB", BaseType: "L", Bands: ["R", "G", "B", "A"] }
                case "RGBX":
                    return { BaseMode: "RGB", BaseType: "L", Bands: ["R", "G", "B", "X"] }
                case "CMYK":
                    return { BaseMode: "RGB", BaseType: "L", Bands: ["C", "M", "Y", "K"] }
                case "YCbCr":
                    return { BaseMode: "RGB", BaseType: "L", Bands: ["Y", "Cb", "Cr"] }
                case "LAB":
                    return { BaseMode: "RGB", BaseType: "L", Bands: ["L", "A", "B"] }
                case "HSV":
                    return { BaseMode: "RGB", BaseType: "L", Bands: ["H", "S", "V"] }
                case "I":
                    return { BaseMode: "L", BaseType: "I", Bands: ["I"] }
                case "F":
                    return { BaseMode: "L", BaseType: "F", Bands: ["F"] }
                case "I;16", "I;16L", "I;16B", "I;16N":
                    return { BaseMode: "L", BaseType: "L", Bands: ["I"] }
                case "BGR;15", "BGR;16", "BGR;24":
                    return { BaseMode: "RGB", BaseType: "L", Bands: ["B", "G", "R"] }
            }
            throw Error("'" modeName "'", -1)
        }

        static GetModeBands(modeName) {
            return Pillow.Image.ModeInfo(modeName).Bands.Length
        }

        static GetModeBandNames(modeName) {
            return Pillow.Image.ModeInfo(modeName).Bands.Clone()
        }

        static GetModeBase(modeName) {
            return Pillow.Image.ModeInfo(modeName).BaseMode
        }

        static GetModeType(modeName) {
            return Pillow.Image.ModeInfo(modeName).BaseType
        }

        class PixelAccess {
            __New(image) {
                this.Image := image
            }

            __Item[x, y] {
                get => this.Image.GetPixel([x, y])
                set => this.Image.PutPixel([x, y], value)
            }
        }

        class Exif {
            __New(orientation := unset, hasOrientation := false, asciiTags := unset, intTags := unset, rationalTags := unset, shortArrayTags := unset, byteArrayTags := unset, signedRationalTags := unset, undefinedTags := unset, rationalArrayTags := unset) {
                this.HasOrientation := hasOrientation ? true : false
                this.Orientation := this.HasOrientation ? Pillow.Image.Exif.NormalizeOrientation(orientation) : 0
                this.AsciiTags := Map()
                if IsSet(asciiTags) {
                    for tag, value in asciiTags
                        this.AsciiTags[Pillow.Image.Exif.NormalizeAsciiTag(tag)] := Pillow.Image.Exif.NormalizeAsciiValue(value)
                }
                this.IntTags := Map()
                if IsSet(intTags) {
                    for tag, value in intTags
                        this.IntTags[Pillow.Image.Exif.NormalizeUintTag(tag)] := Pillow.Image.Exif.NormalizeUintValue(value, Pillow.Image.Exif.UintTagType(tag))
                }
                this.RationalTags := Map()
                if IsSet(rationalTags) {
                    for tag, value in rationalTags
                        this.RationalTags[Pillow.Image.Exif.NormalizeRationalTag(tag)] := Pillow.Image.Exif.NormalizeRationalValue(value)
                }
                this.RationalArrayTags := Map()
                if IsSet(rationalArrayTags) {
                    for tag, value in rationalArrayTags
                        this.RationalArrayTags[Pillow.Image.Exif.NormalizeTag(tag)] := value
                }
                this.UintArrayTags := Map()
                this.DoubleArrayTags := Map()
                this.FloatArrayTags := Map()
                this.ShortArrayTags := Map()
                if IsSet(shortArrayTags) {
                    for tag, value in shortArrayTags
                        this.ShortArrayTags[Pillow.Image.Exif.NormalizeShortArrayTag(tag)] := Pillow.Image.Exif.NormalizeShortArrayValue(tag, value)
                }
                this.ByteArrayTags := Map()
                if IsSet(byteArrayTags) {
                    for tag, value in byteArrayTags
                        this.ByteArrayTags[Pillow.Image.Exif.NormalizeByteArrayTag(tag)] := Pillow.Image.Exif.NormalizeByteArrayValue(value)
                }
                this.SignedRationalTags := Map()
                if IsSet(signedRationalTags) {
                    for tag, value in signedRationalTags
                        this.SignedRationalTags[Pillow.Image.Exif.NormalizeSignedRationalTag(tag)] := Pillow.Image.Exif.NormalizeSignedRationalValue(value)
                }
                this.UndefinedTags := Map()
                if IsSet(undefinedTags) {
                    for tag, value in undefinedTags
                        this.UndefinedTags[Pillow.Image.Exif.NormalizeUndefinedTag(tag)] := Pillow.Image.Exif.NormalizeUndefinedValue(value)
                }
            }

            static FromImage(image) {
                orientation := image.ExifOrientation()
                exif := Pillow.Image.Exif(orientation, orientation != 0)
                exifBlob := 0
                if image.Info.Has("exif") {
                    exifBlob := image.Info["exif"]
                } else if image.Format = "TIFF" {
                    exifBlob := Pillow.Image.NativeMetadataBlob(image.RequireHandle(), "pillow_c_image_metadata_tiff_exif")
                }
                if IsObject(exifBlob) {
                    for tag in [1, 3, 9, 10, 12, 14, 16, 18, 19, 23, 25, 27, 28, 269, 270, 271, 272, 285, 305, 306, 315, 316, 333, 337, 33432, 34737, 34852, 36867, 36868, 36880, 36881, 36882, 37394, 37395, 37520, 37521, 37522, 40964, 42016, 42032, 42033, 42035, 42036, 42037, 42112, 42113, 50708, 50735, 50827, 50931, 50932, 50934, 50936, 50942, 50966, 50967, 50968, 50971, 51081, 51092, 51182, 52526, 52528] {
                        parsed := Pillow.Image.Exif.ReadAsciiTag(exifBlob, tag)
                        if parsed.Has
                            exif.AsciiTags[tag] := parsed.Value
                    }
                    for tag in [5, 7, 11, 29, 30, 31, 254, 255, 256, 257, 258, 259, 262, 263, 264, 265, 266, 273, 277, 278, 279, 280, 281, 284, 288, 289, 290, 292, 293, 296, 317, 322, 323, 324, 325, 326, 327, 328, 332, 334, 338, 339, 340, 341, 531, 34665, 34853, 34855, 34864, 34865, 34866, 34867, 34868, 34869, 40961, 40962, 40963, 41488, 41495, 41985, 41986, 41987, 41989, 41990, 41991, 41992, 41993, 41994, 41996, 42080, 50711, 50717, 50741, 50778, 50779, 50879, 50941, 50970, 50974, 50975, 51107, 51108, 51110, 51177, 51180, 51181, 52529] {
                        parsed := Pillow.Image.Exif.ReadUintTag(exifBlob, tag)
                        if parsed.Has
                            exif.IntTags[tag] := parsed.Value
                    }
                    for tag in [6, 13, 15, 17, 21, 24, 26, 282, 283, 286, 287, 33434, 33437, 37122, 37378, 37381, 37382, 37386, 41483, 41486, 41487, 41493, 41988, 42240, 50731, 50732, 50734, 50737, 50738, 50780, 50935, 51058, 51112, 51178, 51179] {
                        parsed := Pillow.Image.Exif.ReadRationalTag(exifBlob, tag)
                        if parsed.Has
                            exif.RationalTags[tag] := parsed.Value
                    }
                    for tag in [2, 4, 20, 22, 318, 319, 529, 532, 42034, 42082, 50714, 50718, 50719, 50720, 50727, 50728, 50729, 50736, 51091, 51125] {
                        parsed := Pillow.Image.Exif.ReadRationalArrayTag(exifBlob, tag)
                        if parsed.Has
                            exif.RationalArrayTags[tag] := parsed.Value
                    }
                    for tag in [50715, 50721, 50722, 50723, 50724, 50725, 50726, 50832, 50834, 50964, 50965, 52530, 52531, 52532] {
                        parsed := Pillow.Image.Exif.ReadSignedRationalArrayTag(exifBlob, tag)
                        if parsed.Has
                            exif.RationalArrayTags[tag] := parsed.Value
                    }
                    for tag in [273, 279, 324, 325, 50719, 50720, 50829, 50830, 50937, 50981, 51089, 51090, 51091, 52536] {
                        parsed := Pillow.Image.Exif.ReadUintArrayTag(exifBlob, tag)
                        if parsed.Has && parsed.Value.Length > 1
                            exif.UintArrayTags[tag] := parsed.Value
                    }
                    for tag in [33550, 33922, 34264, 34736, 50844, 51041] {
                        parsed := Pillow.Image.Exif.ReadDoubleArrayTag(exifBlob, tag)
                        if parsed.Has
                            exif.DoubleArrayTags[tag] := parsed.Value
                    }
                    for tag in [50938, 50939, 50940, 50982] {
                        parsed := Pillow.Image.Exif.ReadFloatArrayTag(exifBlob, tag)
                        if parsed.Has
                            exif.FloatArrayTags[tag] := parsed.Value
                    }
                    for tag in [258, 291, 297, 301, 320, 321, 336, 342, 530, 34735, 37396, 41492, 42081, 50712, 50713, 50719, 50720, 50829] {
                        parsed := Pillow.Image.Exif.ReadUshortArrayTag(exifBlob, tag)
                        if parsed.Has && (tag != 258 || parsed.Value.Length > 1)
                            exif.ShortArrayTags[tag] := parsed.Value
                    }
                    for tag in [700, 34377, 34856, 37121, 37500, 37510, 40091, 40092, 40093, 40094, 40095, 41484, 41728, 41729, 41995, 50706, 50707, 50709, 50710, 50781, 50828, 50831, 50833, 50969, 50972, 50973, 51008, 51009, 51022, 51043, 51111, 52525, 52533, 52534, 52535] {
                        parsed := Pillow.Image.Exif.ReadByteArrayTag(exifBlob, tag)
                        if parsed.Has
                            exif.ByteArrayTags[tag] := parsed.Value
                    }
                    for tag in [347, 700, 33723, 34856, 36864, 37121, 37500, 37510, 37724, 40960, 41484, 41728, 41729, 41730, 41995, 34675, 50828, 50969, 51008, 51009, 51022, 52525, 52533, 52534, 52535] {
                        parsed := Pillow.Image.Exif.ReadUndefinedTag(exifBlob, tag)
                        if parsed.Has
                            exif.UndefinedTags[tag] := parsed.Value
                    }
                    for tag in [37377, 37379, 37380, 50716, 50730, 50739, 51044, 51109] {
                        parsed := Pillow.Image.Exif.ReadSignedRationalTag(exifBlob, tag)
                        if parsed.Has
                            exif.SignedRationalTags[tag] := parsed.Value
                    }
                }
                return exif
            }

            static NormalizeTag(tag) {
                if !(tag is Integer)
                    throw Error("Pillow.Image.Exif tag must be an integer", -1)
                if tag < 1 || tag > 65535
                    throw Error("Pillow.Image.Exif tag must be an integer in range 1..65535", -1)
                return tag
            }

            static NormalizeAsciiTag(tag) {
                Pillow.Image.Exif.NormalizeTag(tag)
                if tag = 274
                    throw Error("Pillow.Image.Exif orientation tag must use integer orientation values", -1)
                return tag
            }

            static NormalizeAsciiValue(value) {
                if !(value is String)
                    throw Error("Pillow.Image.Exif ASCII tag value must be a string", -1)
                return value
            }

            static NormalizeUintTag(tag) {
                Pillow.Image.Exif.NormalizeTag(tag)
                if !Pillow.Image.Exif.UintTagType(tag)
                    throw Error("Pillow.Image.Exif integer tag is not covered by this native route", -1)
                return tag
            }

            static NormalizeUintValue(value, type) {
                if !(value is Integer)
                    throw Error("Pillow.Image.Exif integer tag value must be an integer", -1)
                limit := type = 3 ? 65535 : 4294967295
                if value < 0 || value > limit
                    throw Error("Pillow.Image.Exif integer tag value out of range", -1)
                return value
            }

            static UintTagType(tag) {
                switch tag {
                    case 256, 257:
                        return 4
                    case 296, 531:
                        return 3
                    default:
                        return 0
                }
            }

            static NormalizeRationalTag(tag) {
                Pillow.Image.Exif.NormalizeTag(tag)
                if !Pillow.Image.Exif.RationalTagType(tag)
                    throw Error("Pillow.Image.Exif rational tag is not covered by this native route", -1)
                return tag
            }

            static NormalizeRationalValue(value) {
                if !IsObject(value) || !(value is Array) || value.Length != 2
                    throw Error("Pillow.Image.Exif rational tag value must be [numerator, denominator]", -1)
                numerator := value[1]
                denominator := value[2]
                if !(numerator is Integer) || !(denominator is Integer)
                    throw Error("Pillow.Image.Exif rational tag values must be integers", -1)
                if numerator < 0 || numerator > 4294967295 || denominator < 1 || denominator > 4294967295
                    throw Error("Pillow.Image.Exif rational tag value out of range", -1)
                return [numerator, denominator]
            }

            static RationalTagType(tag) {
                switch tag {
                    case 282, 283:
                        return 5
                    default:
                        return 0
                }
            }

            static NormalizeSignedRationalTag(tag) {
                Pillow.Image.Exif.NormalizeTag(tag)
                if !Pillow.Image.Exif.SignedRationalTagType(tag)
                    throw Error("Pillow.Image.Exif signed rational tag is not covered by this native route", -1)
                return tag
            }

            static NormalizeSignedRationalValue(value) {
                if !IsObject(value) || !(value is Array) || value.Length != 2
                    throw Error("Pillow.Image.Exif signed rational tag value must be [numerator, denominator]", -1)
                numerator := value[1]
                denominator := value[2]
                if !(numerator is Integer) || !(denominator is Integer)
                    throw Error("Pillow.Image.Exif signed rational tag values must be integers", -1)
                if numerator < -2147483648 || numerator > 2147483647 || denominator < 1 || denominator > 2147483647
                    throw Error("Pillow.Image.Exif signed rational tag value out of range", -1)
                return [numerator, denominator]
            }

            static SignedRationalTagType(tag) {
                switch tag {
                    case 37380:
                        return 10
                    default:
                        return 0
                }
            }

            static NormalizeShortArrayTag(tag) {
                Pillow.Image.Exif.NormalizeTag(tag)
                if !Pillow.Image.Exif.ShortArrayTagLength(tag)
                    throw Error("Pillow.Image.Exif SHORT array tag is not covered by this native route", -1)
                return tag
            }

            static NormalizeShortArrayValue(tag, value) {
                expectedLength := Pillow.Image.Exif.ShortArrayTagLength(tag)
                if !IsObject(value) || !(value is Array) || value.Length != expectedLength
                    throw Error("Pillow.Image.Exif SHORT array tag value length is not covered by this native route", -1)
                result := []
                for item in value {
                    if !(item is Integer)
                        throw Error("Pillow.Image.Exif SHORT array tag values must be integers", -1)
                    if item < 0 || item > 65535
                        throw Error("Pillow.Image.Exif SHORT array tag value out of range", -1)
                    result.Push(item)
                }
                return result
            }

            static ShortArrayTagLength(tag) {
                switch tag {
                    case 530:
                        return 2
                    default:
                        return 0
                }
            }

            static NormalizeByteArrayTag(tag) {
                Pillow.Image.Exif.NormalizeTag(tag)
                if !Pillow.Image.Exif.ByteArrayTagCovered(tag)
                    throw Error("Pillow.Image.Exif BYTE array tag is not covered by this native route", -1)
                return tag
            }

            static NormalizeByteArrayValue(value) {
                if !IsObject(value) || Type(value) != "Buffer"
                    throw Error("Pillow.Image.Exif BYTE array tag value must be a Buffer", -1)
                if value.Size <= 0
                    throw Error("Pillow.Image.Exif BYTE array tag value length is not covered by this native route", -1)
                result := Buffer(value.Size, 0)
                loop value.Size
                    NumPut("UChar", NumGet(value, A_Index - 1, "UChar"), result, A_Index - 1)
                return result
            }

            static ByteArrayTagCovered(tag) {
                switch tag {
                    case 34377, 37510, 40091, 40092, 40093, 40094, 40095, 50706, 50707, 50709, 50710, 50781, 50831, 50833, 50969, 50972, 50973, 51008, 51009, 51022:
                        return true
                    case 700, 34856, 37121, 37500, 41484, 41728, 41729, 41995, 50828, 51043, 51111, 52525, 52533, 52534, 52535:
                        return true
                    default:
                        return false
                }
            }

            static NormalizeUndefinedTag(tag) {
                Pillow.Image.Exif.NormalizeTag(tag)
                if !Pillow.Image.Exif.UndefinedTagCovered(tag)
                    throw Error("Pillow.Image.Exif UNDEFINED tag is not covered by this native route", -1)
                return tag
            }

            static NormalizeUndefinedValue(value) {
                if !IsObject(value) || Type(value) != "Buffer"
                    throw Error("Pillow.Image.Exif UNDEFINED tag value must be a Buffer", -1)
                if value.Size <= 0
                    throw Error("Pillow.Image.Exif UNDEFINED tag value length is not covered by this native route", -1)
                result := Buffer(value.Size, 0)
                loop value.Size
                    NumPut("UChar", NumGet(value, A_Index - 1, "UChar"), result, A_Index - 1)
                return result
            }

            static UndefinedTagCovered(tag) {
                switch tag {
                    case 347, 33723, 34675, 36864, 37724, 40960, 41730:
                        return true
                    default:
                        return false
                }
            }

            static NormalizeOrientation(value) {
                if !(value is Integer) || value < 1 || value > 65535
                    throw Error("Pillow.Image.Exif orientation must be an integer in range 1..65535", -1)
                return value
            }

            static ReadAsciiTag(exif, tag) {
                if !IsObject(exif) || Type(exif) != "Buffer"
                    return { Has: false, Value: "" }
                hasTag := 0
                required := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_ascii_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                )
                if status != -1
                    Pillow.CheckStatus(status)
                if !hasTag
                    return { Has: false, Value: "" }
                value := Buffer(required, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_ascii_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", value,
                    "UPtr", value.Size,
                    "UPtr*", &required,
                    "Int"
                ))
                return { Has: hasTag != 0, Value: StrGet(value, "UTF-8") }
            }

            static ReadUintTag(exif, tag) {
                if !IsObject(exif) || Type(exif) != "Buffer"
                    return { Has: false, Value: 0 }
                hasTag := 0
                value := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_uint_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "UInt*", &value,
                    "Int"
                ))
                return { Has: hasTag != 0, Value: value }
            }

            static ReadRationalTag(exif, tag) {
                if !IsObject(exif) || Type(exif) != "Buffer"
                    return { Has: false, Value: [] }
                hasTag := 0
                numerator := 0
                denominator := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_rational_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "UInt*", &numerator,
                    "UInt*", &denominator,
                    "Int"
                ))
                return { Has: hasTag != 0, Value: hasTag ? [numerator, denominator] : [] }
            }

            static ReadRationalArrayTag(exif, tag) {
                if !IsObject(exif) || Type(exif) != "Buffer"
                    return { Has: false, Value: [] }
                hasTag := 0
                required := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_rational_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", 0,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                )
                if status != -1
                    Pillow.CheckStatus(status)
                if !hasTag
                    return { Has: false, Value: [] }
                numerators := Buffer(required * 4, 0)
                denominators := Buffer(required * 4, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_rational_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", numerators,
                    "Ptr", denominators,
                    "UPtr", required,
                    "UPtr*", &required,
                    "Int"
                ))
                result := []
                Loop required
                    result.Push([NumGet(numerators, (A_Index - 1) * 4, "UInt"), NumGet(denominators, (A_Index - 1) * 4, "UInt")])
                return { Has: hasTag != 0, Value: result }
            }

            static ReadSignedRationalArrayTag(exif, tag) {
                if !IsObject(exif) || Type(exif) != "Buffer"
                    return { Has: false, Value: [] }
                hasTag := 0
                required := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_signed_rational_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", 0,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                )
                if status != -1
                    Pillow.CheckStatus(status)
                if !hasTag
                    return { Has: false, Value: [] }
                numerators := Buffer(required * 4, 0)
                denominators := Buffer(required * 4, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_signed_rational_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", numerators,
                    "Ptr", denominators,
                    "UPtr", required,
                    "UPtr*", &required,
                    "Int"
                ))
                result := []
                Loop required
                    result.Push([NumGet(numerators, (A_Index - 1) * 4, "Int"), NumGet(denominators, (A_Index - 1) * 4, "Int")])
                return { Has: hasTag != 0, Value: result }
            }

            static ReadSignedRationalTag(exif, tag) {
                if !IsObject(exif) || Type(exif) != "Buffer"
                    return { Has: false, Value: [] }
                hasTag := 0
                numerator := 0
                denominator := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_signed_rational_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Int*", &numerator,
                    "Int*", &denominator,
                    "Int"
                ))
                return { Has: hasTag != 0, Value: hasTag ? [numerator, denominator] : [] }
            }

            static ReadUshortArrayTag(exif, tag) {
                if !IsObject(exif) || Type(exif) != "Buffer"
                    return { Has: false, Value: [] }
                hasTag := 0
                required := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_ushort_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                )
                if status != -1
                    Pillow.CheckStatus(status)
                if !hasTag
                    return { Has: false, Value: [] }
                values := Buffer(required * 4, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_ushort_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", values,
                    "UPtr", required,
                    "UPtr*", &required,
                    "Int"
                ))
                result := []
                Loop required
                    result.Push(NumGet(values, (A_Index - 1) * 4, "UInt"))
                return { Has: hasTag != 0, Value: result }
            }

            static ReadUintArrayTag(exif, tag) {
                if !IsObject(exif) || Type(exif) != "Buffer"
                    return { Has: false, Value: [] }
                hasTag := 0
                required := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_uint_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                )
                if status != -1
                    Pillow.CheckStatus(status)
                if !hasTag
                    return { Has: false, Value: [] }
                values := Buffer(required * 4, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_uint_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", values,
                    "UPtr", required,
                    "UPtr*", &required,
                    "Int"
                ))
                result := []
                Loop required
                    result.Push(NumGet(values, (A_Index - 1) * 4, "UInt"))
                return { Has: hasTag != 0, Value: result }
            }

            static ReadByteArrayTag(exif, tag) {
                if !IsObject(exif) || Type(exif) != "Buffer"
                    return { Has: false, Value: Buffer(0, 0) }
                hasTag := 0
                required := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_byte_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                )
                if status != -1
                    Pillow.CheckStatus(status)
                if !hasTag
                    return { Has: false, Value: Buffer(0, 0) }
                value := Buffer(required, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_byte_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", value,
                    "UPtr", value.Size,
                    "UPtr*", &required,
                    "Int"
                ))
                return { Has: hasTag != 0, Value: value }
            }

            static ReadDoubleArrayTag(exif, tag) {
                if !IsObject(exif) || Type(exif) != "Buffer"
                    return { Has: false, Value: [] }
                hasTag := 0
                required := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_double_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                )
                if status != -1
                    Pillow.CheckStatus(status)
                if !hasTag
                    return { Has: false, Value: [] }
                values := Buffer(required * 8, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_double_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", values,
                    "UPtr", required,
                    "UPtr*", &required,
                    "Int"
                ))
                result := []
                Loop required
                    result.Push(NumGet(values, (A_Index - 1) * 8, "Double"))
                return { Has: hasTag != 0, Value: result }
            }

            static ReadFloatArrayTag(exif, tag) {
                if !IsObject(exif) || Type(exif) != "Buffer"
                    return { Has: false, Value: [] }
                hasTag := 0
                required := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_float_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                )
                if status != -1
                    Pillow.CheckStatus(status)
                if !hasTag
                    return { Has: false, Value: [] }
                values := Buffer(required * 4, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_float_array_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", values,
                    "UPtr", required,
                    "UPtr*", &required,
                    "Int"
                ))
                result := []
                Loop required
                    result.Push(NumGet(values, (A_Index - 1) * 4, "Float"))
                return { Has: hasTag != 0, Value: result }
            }

            static ReadUndefinedTag(exif, tag) {
                if !IsObject(exif) || Type(exif) != "Buffer"
                    return { Has: false, Value: Buffer(0, 0) }
                hasTag := 0
                required := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_undefined_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                )
                if status != -1
                    Pillow.CheckStatus(status)
                if !hasTag
                    return { Has: false, Value: Buffer(0, 0) }
                value := Buffer(required, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_undefined_tag",
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int", tag,
                    "Int*", &hasTag,
                    "Ptr", value,
                    "UPtr", value.Size,
                    "UPtr*", &required,
                    "Int"
                ))
                return { Has: hasTag != 0, Value: value }
            }

            Length {
                get => (this.HasOrientation ? 1 : 0) + this.AsciiTags.Count + this.IntTags.Count + this.RationalTags.Count + this.RationalArrayTags.Count + this.UintArrayTags.Count + this.DoubleArrayTags.Count + this.FloatArrayTags.Count + this.ShortArrayTags.Count + this.ByteArrayTags.Count + this.SignedRationalTags.Count + this.UndefinedTags.Count
            }

            Has(tag) {
                Pillow.Image.Exif.NormalizeTag(tag)
                if tag = 274
                    return this.HasOrientation
                return this.AsciiTags.Has(tag) || this.IntTags.Has(tag) || this.RationalTags.Has(tag) || this.RationalArrayTags.Has(tag) || this.UintArrayTags.Has(tag) || this.DoubleArrayTags.Has(tag) || this.FloatArrayTags.Has(tag) || this.ShortArrayTags.Has(tag) || this.ByteArrayTags.Has(tag) || this.SignedRationalTags.Has(tag) || this.UndefinedTags.Has(tag)
            }

            Get(tag, defaultValue := unset) {
                Pillow.Image.Exif.NormalizeTag(tag)
                if tag = 274 && this.HasOrientation
                    return this.Orientation
                if tag != 274 && this.AsciiTags.Has(tag)
                    return this.AsciiTags[tag]
                if tag != 274 && this.IntTags.Has(tag)
                    return this.IntTags[tag]
                if tag != 274 && this.RationalTags.Has(tag)
                    return this.RationalTags[tag]
                if tag != 274 && this.RationalArrayTags.Has(tag)
                    return this.RationalArrayTags[tag]
                if tag != 274 && this.UintArrayTags.Has(tag)
                    return this.UintArrayTags[tag]
                if tag != 274 && this.DoubleArrayTags.Has(tag)
                    return this.DoubleArrayTags[tag]
                if tag != 274 && this.FloatArrayTags.Has(tag)
                    return this.FloatArrayTags[tag]
                if tag != 274 && this.ShortArrayTags.Has(tag)
                    return this.ShortArrayTags[tag]
                if tag != 274 && this.ByteArrayTags.Has(tag)
                    return this.ByteArrayTags[tag]
                if tag != 274 && this.SignedRationalTags.Has(tag)
                    return this.SignedRationalTags[tag]
                if tag != 274 && this.UndefinedTags.Has(tag)
                    return this.UndefinedTags[tag]
                return IsSet(defaultValue) ? defaultValue : ""
            }

            Delete(tag) {
                Pillow.Image.Exif.NormalizeTag(tag)
                if tag = 274 {
                    this.HasOrientation := false
                    this.Orientation := 0
                } else if this.AsciiTags.Has(tag) {
                    this.AsciiTags.Delete(tag)
                } else if this.IntTags.Has(tag) {
                    this.IntTags.Delete(tag)
                } else if this.RationalTags.Has(tag) {
                    this.RationalTags.Delete(tag)
                } else if this.RationalArrayTags.Has(tag) {
                    this.RationalArrayTags.Delete(tag)
                } else if this.UintArrayTags.Has(tag) {
                    this.UintArrayTags.Delete(tag)
                } else if this.DoubleArrayTags.Has(tag) {
                    this.DoubleArrayTags.Delete(tag)
                } else if this.FloatArrayTags.Has(tag) {
                    this.FloatArrayTags.Delete(tag)
                } else if this.ShortArrayTags.Has(tag) {
                    this.ShortArrayTags.Delete(tag)
                } else if this.ByteArrayTags.Has(tag) {
                    this.ByteArrayTags.Delete(tag)
                } else if this.SignedRationalTags.Has(tag) {
                    this.SignedRationalTags.Delete(tag)
                } else if this.UndefinedTags.Has(tag) {
                    this.UndefinedTags.Delete(tag)
                }
            }

            __Item[tag] {
                get {
                    Pillow.Image.Exif.NormalizeTag(tag)
                    if tag = 274 {
                        if !this.HasOrientation
                            throw Error("Pillow.Image.Exif tag not found", -1)
                        return this.Orientation
                    }
                    if !this.AsciiTags.Has(tag)
                        if !this.IntTags.Has(tag)
                            if !this.RationalTags.Has(tag)
                                if !this.RationalArrayTags.Has(tag)
                                    if !this.UintArrayTags.Has(tag)
                                        if !this.DoubleArrayTags.Has(tag)
                                            if !this.FloatArrayTags.Has(tag)
                                                if !this.ShortArrayTags.Has(tag)
                                                    if !this.ByteArrayTags.Has(tag)
                                                        if !this.SignedRationalTags.Has(tag)
                                                            if !this.UndefinedTags.Has(tag)
                                                                throw Error("Pillow.Image.Exif tag not found", -1)
                    if this.AsciiTags.Has(tag)
                        return this.AsciiTags[tag]
                    if this.IntTags.Has(tag)
                        return this.IntTags[tag]
                    if this.RationalTags.Has(tag)
                        return this.RationalTags[tag]
                    if this.RationalArrayTags.Has(tag)
                        return this.RationalArrayTags[tag]
                    if this.UintArrayTags.Has(tag)
                        return this.UintArrayTags[tag]
                    if this.DoubleArrayTags.Has(tag)
                        return this.DoubleArrayTags[tag]
                    if this.FloatArrayTags.Has(tag)
                        return this.FloatArrayTags[tag]
                    if this.ShortArrayTags.Has(tag)
                        return this.ShortArrayTags[tag]
                    if this.ByteArrayTags.Has(tag)
                        return this.ByteArrayTags[tag]
                    if this.SignedRationalTags.Has(tag)
                        return this.SignedRationalTags[tag]
                    return this.UndefinedTags[tag]
                }
                set {
                    Pillow.Image.Exif.NormalizeTag(tag)
                    if this.DoubleArrayTags.Has(tag)
                        this.DoubleArrayTags.Delete(tag)
                    if this.FloatArrayTags.Has(tag)
                        this.FloatArrayTags.Delete(tag)
                    if tag = 274 {
                        this.Orientation := Pillow.Image.Exif.NormalizeOrientation(value)
                        this.HasOrientation := true
                    } else if IsObject(value) && Type(value) = "Buffer" {
                        isUndefinedTag := Pillow.Image.Exif.UndefinedTagCovered(tag)
                        if isUndefinedTag
                            this.UndefinedTags[Pillow.Image.Exif.NormalizeUndefinedTag(tag)] := Pillow.Image.Exif.NormalizeUndefinedValue(value)
                        else
                            this.ByteArrayTags[Pillow.Image.Exif.NormalizeByteArrayTag(tag)] := Pillow.Image.Exif.NormalizeByteArrayValue(value)
                        if this.AsciiTags.Has(tag)
                            this.AsciiTags.Delete(tag)
                        if this.IntTags.Has(tag)
                            this.IntTags.Delete(tag)
                        if this.RationalTags.Has(tag)
                            this.RationalTags.Delete(tag)
                        if this.RationalArrayTags.Has(tag)
                            this.RationalArrayTags.Delete(tag)
                        if this.ShortArrayTags.Has(tag)
                            this.ShortArrayTags.Delete(tag)
                        if isUndefinedTag && this.ByteArrayTags.Has(tag)
                            this.ByteArrayTags.Delete(tag)
                        if !isUndefinedTag && this.UndefinedTags.Has(tag)
                            this.UndefinedTags.Delete(tag)
                        if this.SignedRationalTags.Has(tag)
                            this.SignedRationalTags.Delete(tag)
                    } else if IsObject(value) && value is Array && Pillow.Image.Exif.SignedRationalTagType(tag) {
                        this.SignedRationalTags[tag] := Pillow.Image.Exif.NormalizeSignedRationalValue(value)
                        if this.AsciiTags.Has(tag)
                            this.AsciiTags.Delete(tag)
                        if this.IntTags.Has(tag)
                            this.IntTags.Delete(tag)
                        if this.RationalTags.Has(tag)
                            this.RationalTags.Delete(tag)
                        if this.RationalArrayTags.Has(tag)
                            this.RationalArrayTags.Delete(tag)
                        if this.ShortArrayTags.Has(tag)
                            this.ShortArrayTags.Delete(tag)
                        if this.ByteArrayTags.Has(tag)
                            this.ByteArrayTags.Delete(tag)
                        if this.UndefinedTags.Has(tag)
                            this.UndefinedTags.Delete(tag)
                    } else if IsObject(value) && value is Array && Pillow.Image.Exif.ShortArrayTagLength(tag) {
                        this.ShortArrayTags[tag] := Pillow.Image.Exif.NormalizeShortArrayValue(tag, value)
                        if this.AsciiTags.Has(tag)
                            this.AsciiTags.Delete(tag)
                        if this.IntTags.Has(tag)
                            this.IntTags.Delete(tag)
                        if this.RationalTags.Has(tag)
                            this.RationalTags.Delete(tag)
                        if this.RationalArrayTags.Has(tag)
                            this.RationalArrayTags.Delete(tag)
                        if this.ByteArrayTags.Has(tag)
                            this.ByteArrayTags.Delete(tag)
                        if this.SignedRationalTags.Has(tag)
                            this.SignedRationalTags.Delete(tag)
                        if this.UndefinedTags.Has(tag)
                            this.UndefinedTags.Delete(tag)
                    } else if IsObject(value) && value is Array && Pillow.Image.Exif.RationalTagType(tag) {
                        this.RationalTags[tag] := Pillow.Image.Exif.NormalizeRationalValue(value)
                        if this.AsciiTags.Has(tag)
                            this.AsciiTags.Delete(tag)
                        if this.IntTags.Has(tag)
                            this.IntTags.Delete(tag)
                        if this.RationalArrayTags.Has(tag)
                            this.RationalArrayTags.Delete(tag)
                        if this.ShortArrayTags.Has(tag)
                            this.ShortArrayTags.Delete(tag)
                        if this.ByteArrayTags.Has(tag)
                            this.ByteArrayTags.Delete(tag)
                        if this.SignedRationalTags.Has(tag)
                            this.SignedRationalTags.Delete(tag)
                        if this.UndefinedTags.Has(tag)
                            this.UndefinedTags.Delete(tag)
                    } else if value is Integer && Pillow.Image.Exif.UintTagType(tag) {
                        uintType := Pillow.Image.Exif.UintTagType(tag)
                        this.IntTags[tag] := Pillow.Image.Exif.NormalizeUintValue(value, uintType)
                        if this.AsciiTags.Has(tag)
                            this.AsciiTags.Delete(tag)
                        if this.RationalTags.Has(tag)
                            this.RationalTags.Delete(tag)
                        if this.RationalArrayTags.Has(tag)
                            this.RationalArrayTags.Delete(tag)
                        if this.ShortArrayTags.Has(tag)
                            this.ShortArrayTags.Delete(tag)
                        if this.ByteArrayTags.Has(tag)
                            this.ByteArrayTags.Delete(tag)
                        if this.SignedRationalTags.Has(tag)
                            this.SignedRationalTags.Delete(tag)
                        if this.UndefinedTags.Has(tag)
                            this.UndefinedTags.Delete(tag)
                    } else {
                        this.AsciiTags[tag] := Pillow.Image.Exif.NormalizeAsciiValue(value)
                        if this.IntTags.Has(tag)
                            this.IntTags.Delete(tag)
                        if this.RationalTags.Has(tag)
                            this.RationalTags.Delete(tag)
                        if this.RationalArrayTags.Has(tag)
                            this.RationalArrayTags.Delete(tag)
                        if this.ShortArrayTags.Has(tag)
                            this.ShortArrayTags.Delete(tag)
                        if this.ByteArrayTags.Has(tag)
                            this.ByteArrayTags.Delete(tag)
                        if this.SignedRationalTags.Has(tag)
                            this.SignedRationalTags.Delete(tag)
                        if this.UndefinedTags.Has(tag)
                            this.UndefinedTags.Delete(tag)
                    }
                    if tag != 274 && this.UintArrayTags.Has(tag)
                        this.UintArrayTags.Delete(tag)
                }
            }

            ToBytes() {
                orientation := this.HasOrientation ? this.Orientation : 0
                asciiCount := this.AsciiTags.Count
                intCount := this.IntTags.Count
                rationalCount := this.RationalTags.Count
                rationalArrayCount := this.RationalArrayTags.Count
                if rationalArrayCount
                    throw Error("Pillow.Image.Exif rational-array serialization is not covered by this native route", -1)
                uintArrayCount := this.UintArrayTags.Count
                if uintArrayCount
                    throw Error("Pillow.Image.Exif LONG-array serialization is not covered by this native route", -1)
                shortArrayCount := this.ShortArrayTags.Count
                byteArrayCount := this.ByteArrayTags.Count
                signedRationalCount := this.SignedRationalTags.Count
                undefinedCount := this.UndefinedTags.Count
                tags := asciiCount ? Buffer(asciiCount * 4, 0) : 0
                valuePtrs := asciiCount ? Buffer(asciiCount * A_PtrSize, 0) : 0
                valueBuffers := []
                index := 0
                for tag, value in this.AsciiTags {
                    index += 1
                    valueBuffers.Push(Pillow.Image.Utf8Buffer(value))
                    NumPut("Int", tag, tags, (index - 1) * 4)
                    NumPut("Ptr", valueBuffers[index].Ptr, valuePtrs, (index - 1) * A_PtrSize)
                }
                intTags := intCount ? Buffer(intCount * 4, 0) : 0
                intValues := intCount ? Buffer(intCount * 4, 0) : 0
                intTypes := intCount ? Buffer(intCount * 4, 0) : 0
                index := 0
                for tag, value in this.IntTags {
                    index += 1
                    type := Pillow.Image.Exif.UintTagType(tag)
                    NumPut("Int", tag, intTags, (index - 1) * 4)
                    NumPut("UInt", value, intValues, (index - 1) * 4)
                    NumPut("Int", type, intTypes, (index - 1) * 4)
                }
                rationalTags := rationalCount ? Buffer(rationalCount * 4, 0) : 0
                rationalNumerators := rationalCount ? Buffer(rationalCount * 4, 0) : 0
                rationalDenominators := rationalCount ? Buffer(rationalCount * 4, 0) : 0
                index := 0
                for tag, value in this.RationalTags {
                    index += 1
                    NumPut("Int", tag, rationalTags, (index - 1) * 4)
                    NumPut("UInt", value[1], rationalNumerators, (index - 1) * 4)
                    NumPut("UInt", value[2], rationalDenominators, (index - 1) * 4)
                }
                shortArrayTags := shortArrayCount ? Buffer(shortArrayCount * 4, 0) : 0
                shortArrayOffsets := shortArrayCount ? Buffer(shortArrayCount * A_PtrSize, 0) : 0
                shortArrayCounts := shortArrayCount ? Buffer(shortArrayCount * A_PtrSize, 0) : 0
                flatShortArrayValues := []
                index := 0
                for tag, value in this.ShortArrayTags {
                    index += 1
                    NumPut("Int", tag, shortArrayTags, (index - 1) * 4)
                    NumPut("UPtr", flatShortArrayValues.Length, shortArrayOffsets, (index - 1) * A_PtrSize)
                    NumPut("UPtr", value.Length, shortArrayCounts, (index - 1) * A_PtrSize)
                    for item in value
                        flatShortArrayValues.Push(item)
                }
                shortArrayValues := flatShortArrayValues.Length ? Buffer(flatShortArrayValues.Length * 4, 0) : 0
                for index, value in flatShortArrayValues
                    NumPut("UInt", value, shortArrayValues, (index - 1) * 4)
                byteArrayTags := byteArrayCount ? Buffer(byteArrayCount * 4, 0) : 0
                byteArrayOffsets := byteArrayCount ? Buffer(byteArrayCount * A_PtrSize, 0) : 0
                byteArrayCounts := byteArrayCount ? Buffer(byteArrayCount * A_PtrSize, 0) : 0
                flatByteArrayValues := []
                index := 0
                for tag, value in this.ByteArrayTags {
                    index += 1
                    NumPut("Int", tag, byteArrayTags, (index - 1) * 4)
                    NumPut("UPtr", flatByteArrayValues.Length, byteArrayOffsets, (index - 1) * A_PtrSize)
                    NumPut("UPtr", value.Size, byteArrayCounts, (index - 1) * A_PtrSize)
                    loop value.Size
                        flatByteArrayValues.Push(NumGet(value, A_Index - 1, "UChar"))
                }
                byteArrayValues := flatByteArrayValues.Length ? Buffer(flatByteArrayValues.Length, 0) : 0
                for index, value in flatByteArrayValues
                    NumPut("UChar", value, byteArrayValues, index - 1)
                signedRationalTags := signedRationalCount ? Buffer(signedRationalCount * 4, 0) : 0
                signedRationalNumerators := signedRationalCount ? Buffer(signedRationalCount * 4, 0) : 0
                signedRationalDenominators := signedRationalCount ? Buffer(signedRationalCount * 4, 0) : 0
                index := 0
                for tag, value in this.SignedRationalTags {
                    index += 1
                    NumPut("Int", tag, signedRationalTags, (index - 1) * 4)
                    NumPut("Int", value[1], signedRationalNumerators, (index - 1) * 4)
                    NumPut("Int", value[2], signedRationalDenominators, (index - 1) * 4)
                }
                undefinedTags := undefinedCount ? Buffer(undefinedCount * 4, 0) : 0
                undefinedOffsets := undefinedCount ? Buffer(undefinedCount * A_PtrSize, 0) : 0
                undefinedCounts := undefinedCount ? Buffer(undefinedCount * A_PtrSize, 0) : 0
                flatUndefinedValues := []
                index := 0
                for tag, value in this.UndefinedTags {
                    index += 1
                    NumPut("Int", tag, undefinedTags, (index - 1) * 4)
                    NumPut("UPtr", flatUndefinedValues.Length, undefinedOffsets, (index - 1) * A_PtrSize)
                    NumPut("UPtr", value.Size, undefinedCounts, (index - 1) * A_PtrSize)
                    loop value.Size
                        flatUndefinedValues.Push(NumGet(value, A_Index - 1, "UChar"))
                }
                undefinedValues := flatUndefinedValues.Length ? Buffer(flatUndefinedValues.Length, 0) : 0
                for index, value in flatUndefinedValues
                    NumPut("UChar", value, undefinedValues, index - 1)
                required := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_entries_undefined_bytes",
                    "Int", orientation,
                    "Ptr", tags,
                    "Ptr", valuePtrs,
                    "UPtr", asciiCount,
                    "Ptr", intTags,
                    "Ptr", intValues,
                    "Ptr", intTypes,
                    "UPtr", intCount,
                    "Ptr", rationalTags,
                    "Ptr", rationalNumerators,
                    "Ptr", rationalDenominators,
                    "UPtr", rationalCount,
                    "Ptr", shortArrayTags,
                    "Ptr", shortArrayValues,
                    "UPtr", flatShortArrayValues.Length,
                    "Ptr", shortArrayOffsets,
                    "Ptr", shortArrayCounts,
                    "UPtr", shortArrayCount,
                    "Ptr", byteArrayTags,
                    "Ptr", byteArrayValues,
                    "UPtr", flatByteArrayValues.Length,
                    "Ptr", byteArrayOffsets,
                    "Ptr", byteArrayCounts,
                    "UPtr", byteArrayCount,
                    "Ptr", signedRationalTags,
                    "Ptr", signedRationalNumerators,
                    "Ptr", signedRationalDenominators,
                    "UPtr", signedRationalCount,
                    "Ptr", undefinedTags,
                    "Ptr", undefinedValues,
                    "UPtr", flatUndefinedValues.Length,
                    "Ptr", undefinedOffsets,
                    "Ptr", undefinedCounts,
                    "UPtr", undefinedCount,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                )
                if status != -1
                    Pillow.CheckStatus(status)
                bytes := Buffer(required, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_exif_entries_undefined_bytes",
                    "Int", orientation,
                    "Ptr", tags,
                    "Ptr", valuePtrs,
                    "UPtr", asciiCount,
                    "Ptr", intTags,
                    "Ptr", intValues,
                    "Ptr", intTypes,
                    "UPtr", intCount,
                    "Ptr", rationalTags,
                    "Ptr", rationalNumerators,
                    "Ptr", rationalDenominators,
                    "UPtr", rationalCount,
                    "Ptr", shortArrayTags,
                    "Ptr", shortArrayValues,
                    "UPtr", flatShortArrayValues.Length,
                    "Ptr", shortArrayOffsets,
                    "Ptr", shortArrayCounts,
                    "UPtr", shortArrayCount,
                    "Ptr", byteArrayTags,
                    "Ptr", byteArrayValues,
                    "UPtr", flatByteArrayValues.Length,
                    "Ptr", byteArrayOffsets,
                    "Ptr", byteArrayCounts,
                    "UPtr", byteArrayCount,
                    "Ptr", signedRationalTags,
                    "Ptr", signedRationalNumerators,
                    "Ptr", signedRationalDenominators,
                    "UPtr", signedRationalCount,
                    "Ptr", undefinedTags,
                    "Ptr", undefinedValues,
                    "UPtr", flatUndefinedValues.Length,
                    "Ptr", undefinedOffsets,
                    "Ptr", undefinedCounts,
                    "UPtr", undefinedCount,
                    "Ptr", bytes,
                    "UPtr", bytes.Size,
                    "UPtr*", &required,
                    "Int"
                ))
                return bytes
            }

        }

        class IcoFile {
            __New(image) {
                if !(image is Pillow.Image) || image.Format != "ICO" || image.FramePath = ""
                    throw Error("Pillow.Image.ico is only available on opened ICO images", -1)
                this.Image := image
                this.Path := image.FramePath
            }

            sizes() {
                return Pillow.Image.IcoSizes(this.Path)
            }

            getimage(size) {
                sizes := this.sizes()
                width := 0
                height := 0
                useRequestedSize := false
                if IsObject(size) && size.Length = 2 && size[1] is Integer && size[2] is Integer {
                    width := size[1]
                    height := size[2]
                    for available in sizes {
                        if available[1] = width && available[2] = height {
                            useRequestedSize := true
                            break
                        }
                    }
                }

                outHandle := 0
                pathBytes := Pillow.Image.Utf8Buffer(this.Path)
                if useRequestedSize {
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_ico_size",
                        "Ptr", pathBytes,
                        "Int", width,
                        "Int", height,
                        "Ptr*", &outHandle,
                        "Int"
                    ))
                } else {
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_ico",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    ))
                }
                result := Pillow.WrapImageHandle(outHandle)
                result.Format := Pillow.Image.IcoPayloadFormat(this.Path, width, height, useRequestedSize)
                Pillow.Image.ApplyIcoPayloadDibMetadata(result, this.Path, width, height, useRequestedSize)
                return result
            }
        }

        static IcoSizes(path) {
            pathBytes := Pillow.Image.Utf8Buffer(path)
            required := 0
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_ico_sizes",
                "Ptr", pathBytes,
                "Ptr", 0,
                "UPtr", 0,
                "UPtr*", &required,
                "Int"
            )
            if status != -1
                Pillow.CheckStatus(status)
            pairs := Buffer(required * 2 * 4, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_ico_sizes",
                "Ptr", pathBytes,
                "Ptr", pairs,
                "UPtr", required,
                "UPtr*", &required,
                "Int"
            ))
            sizes := []
            loop required
                sizes.Push([NumGet(pairs, (A_Index - 1) * 8, "Int"), NumGet(pairs, (A_Index - 1) * 8 + 4, "Int")])
            return sizes
        }

        static IcoPayloadFormat(path, width, height, requireRequestedSize) {
            pathBytes := Pillow.Image.Utf8Buffer(path)
            required := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_ico_payload_format",
                "Ptr", pathBytes,
                "Int", width,
                "Int", height,
                "Int", requireRequestedSize ? 1 : 0,
                "Ptr", 0,
                "UPtr", 0,
                "UPtr*", &required,
                "Int"
            ))
            if required = 0
                return ""
            formatBytes := Buffer(required, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_ico_payload_format",
                "Ptr", pathBytes,
                "Int", width,
                "Int", height,
                "Int", requireRequestedSize ? 1 : 0,
                "Ptr", formatBytes,
                "UPtr", formatBytes.Size,
                "UPtr*", &required,
                "Int"
            ))
            return StrGet(formatBytes.Ptr, required - 1, "UTF-8")
        }

        static ApplyIcoPayloadDibMetadata(image, path, width, height, requireRequestedSize) {
            pathBytes := Pillow.Image.Utf8Buffer(path)
            hasDib := 0
            hasDpi := 0
            dpiX := 0.0
            dpiY := 0.0
            compression := -1
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_ico_payload_dib_metadata",
                "Ptr", pathBytes,
                "Int", width,
                "Int", height,
                "Int", requireRequestedSize ? 1 : 0,
                "Int*", &hasDib,
                "Int*", &hasDpi,
                "Double*", &dpiX,
                "Double*", &dpiY,
                "Int*", &compression,
                "Int"
            ))
            if hasDib {
                if hasDpi
                    image.Info["dpi"] := [dpiX, dpiY]
                image.Info["compression"] := compression
            }
        }

        __New(handle) {
            this.Handle := handle
            this.Format := ""
            this.Info := Map()
            this.FramePath := ""
            this.FrameFormat := ""
            this.FrameIndex := 0
            this.FrameCount := 1
            this.DisposalMethod := 0
            this.BufferViewSource := 0
            this.JpegDraftApplied := false
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

        static linear_gradient(modeName) {
            return Pillow.Image.LinearGradient(modeName)
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

        static radial_gradient(modeName) {
            return Pillow.Image.RadialGradient(modeName)
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

        static effect_mandelbrot(size, extent, quality) {
            return Pillow.Image.EffectMandelbrot(size, extent, quality)
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

        static effect_noise(size, sigma) {
            return Pillow.Image.EffectNoise(size, sigma)
        }

        static EffectSpread(image, distance) {
            if !(image is Pillow.Image)
                throw Error("Pillow.Image.EffectSpread expects a Pillow.Image", -1)
            return image.EffectSpread(distance)
        }

        static effect_spread(image, distance) {
            return Pillow.Image.EffectSpread(image, distance)
        }

        static Open(path, formats := unset) {
            if !(path is String)
                throw Error("Pillow.Image.Open expects a file path", -1)
            openFormats := Pillow.Image.ResolveOpenFormats(path, IsSet(formats) ? formats : unset)

            pathBytes := Pillow.Image.Utf8Buffer(path)
            lastStatus := -3
            for format in openFormats {
                outHandle := 0
                if format = "DIB" {
                    ; BEHAV-DIB-001: Pillow's DIB is byte-identical to its
                    ; BMP minus the 14-byte BITMAPFILEHEADER; open rebuilds
                    ; that header from the BITMAPINFOHEADER and reuses the
                    ; native BMP decoder.
                    lastStatus := Pillow.Image.OpenDibHandle(path, &outHandle)
                } else if format = "IM" {
                    ; BEHAV-IM-001: parse the ASCII header, then feed the
                    ; raw payload (and P-mode LUT) into native storage.
                    lastStatus := Pillow.Image.OpenImHandle(path, &outHandle)
                } else if format = "PALM" {
                    ; BEHAV-PALM-001: Pillow registers no Palm OPEN, so
                    ; identification fails with the Pillow-shaped message.
                    throw Error("cannot identify image file <" path ">", -1)
                } else if format = "SPIDER" {
                    ; BEHAV-SPIDER-001: parse the float header records and
                    ; feed the native float32 samples into an F image.
                    lastStatus := Pillow.Image.OpenSpiderHandle(path, &outHandle)
                } else if format = "SGI" {
                    ; BEHAV-SGI-001: the native SGI decoder returns local
                    ; status codes for Pillow's distinct error shapes
                    ; (bad magic, unsupported (bpc, dimension, zsize)
                    ; keys, RLE table overruns, verbatim truncation with
                    ; Pillow's per-tile leftover-byte count, short 16-bit
                    ; bands, and the tile-less compression values).
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_sgi",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -5
                        throw Error("Unsupported SGI image mode", -1)
                    if lastStatus = -2
                        throw Error("buffer overrun when reading image file", -1)
                    if lastStatus = -7
                        throw Error("not enough image data", -1)
                    if lastStatus = -8
                        throw Error("cannot load this image", -1)
                    if lastStatus = -6
                        throw Error("image file is truncated (" Pillow.Image.SgiTruncatedCount(path) " bytes not processed)", -1)
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "DDS" {
                    ; BEHAV-DDS-001: the native DDS decoder returns local
                    ; status codes for Pillow's distinct error shapes;
                    ; rebuild the exact messages from the file header.
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_dds",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus < -8
                        throw Error(Pillow.Image.DdsOpenError(lastStatus, path), -1)
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "ICNS" {
                    ; BEHAV-ICNS-001: the native ICNS decoder returns local
                    ; status codes for Pillow's load-time error shapes;
                    ; container-level failures collapse to Pillow's
                    ; identification error (Image.open wraps the plugin's
                    ; SyntaxError in UnidentifiedImageError).
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_icns",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -20
                        throw Error("Unsupported icon subimage format (rebuild PIL with JPEG 2000 support to fix this)", -1)
                    if lastStatus = -21
                        throw Error("Unsupported icon subimage format", -1)
                    if lastStatus = -22
                        throw Error("Unknown signature, expecting 0x00000000", -1)
                    if lastStatus = -23
                        throw Error("Error reading channel [" Pillow.Image.IcnsRleLeftover(path) " left]", -1)
                    if lastStatus = -24
                        throw Error("Pillow.Image.Open ICNS PNG payload uses an unsupported bit depth or color type", -1)
                    if lastStatus = -25
                        throw Error("image file is truncated", -1)
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "EPS" {
                    ; BEHAV-EPS-001: Pillow's EpsImageFile parses the DSC
                    ; header at open (its SyntaxErrors collapse to the
                    ; identification error; "cannot determine EPS bounding
                    ; box" and the bad-header OSError propagate), and only
                    ; load() needs Ghostscript. This runtime ships no
                    ; Ghostscript, so a VALID header surfaces Pillow's
                    ; exact load error at Open (the eager facade's
                    ; open+load analogue); header parsing matches Pillow's
                    ; open-time error shapes exactly.
                    epsError := Pillow.Image.EpsOpenFailure(path)
                    if epsError = ""
                        throw Error("Unable to locate Ghostscript on paths", -1)
                    if epsError = 'EPS header missing "%!PS-Adobe" comment' || epsError = 'EPS header missing "%%BoundingBox" comment'
                        ; Pillow's Image.open wraps the plugin's
                        ; SyntaxError into UnidentifiedImageError.
                        throw Error("cannot identify image file <" path ">", -1)
                    throw Error(epsError, -1)
                } else if format = "PDF" {
                    ; BEHAV-PDF-001: Pillow 11.3.0 registers no PDF open
                    ; (PdfImagePlugin is save-only), so identification
                    ; fails exactly like an unknown file.
                    throw Error("cannot identify image file <" path ">", -1)
                } else if format = "PIXAR" {
                    ; BEHAV-OPEN-001: Pillow's PixarImageFile reads the
                    ; 512-byte header (size at 418/416, mode 14,2 = RGB)
                    ; and a raw dump at offset 1024; short dumps raise
                    ; "image file is truncated (X bytes not processed)"
                    ; with X = leftover payload mod the row width.
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_pixar",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -29
                        throw Error("image file is truncated (" Pillow.Image.OpenSimpleTruncatedCount(path, Pillow.Image.PixarWidth(path) * 3, 1024) " bytes not processed)", -1)
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "XVTHUMB" {
                    ; BEHAV-OPEN-001: Pillow's XVThumbImageFile parses
                    ; "P7 332", skips "#" comments, reads "W H", and
                    ; decodes raw indices with the RGB332 palette; every
                    ; header shape failure collapses to the
                    ; identification error and short indices raise the
                    ; truncated message with the row-modulo count.
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_xvthumb",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -29
                        throw Error("image file is truncated (" Pillow.Image.XvThumbTruncatedCount(path, Pillow.Image.XvThumbHeader(path)) " bytes not processed)", -1)
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "DCX" {
                    ; BEHAV-OPEN-001: Pillow's DcxImageFile is the PCX
                    ; multi-page container (LE32 directory, frames at
                    ; each offset); the eager facade decodes frame 0
                    ; through the native PCX decoder and exposes
                    ; FrameCount (n_frames/seek stay a documented child).
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_dcx",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "FTEX" {
                    ; BEHAV-OPEN-002: FtexImageFile — "FTEX" magic, one
                    ; format entry (else Pillow's AssertionError), format
                    ; 1 = raw RGB at the directory offset, format 0 =
                    ; DXT1/BC1 RGBA; other ids raise Pillow's exact
                    ; ValueError and short payloads the truncated shapes.
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_ftex",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -30
                        throw Error("", -1)
                    if lastStatus = -38
                        throw Error("Invalid texture compression format: " Pillow.Image.FtexFormatId(path), -1)
                    if lastStatus = -32
                        throw Error("image file is truncated (0 bytes not processed)", -1)
                    if lastStatus = -29
                        throw Error("image file is truncated (" Pillow.Image.FtexTruncatedCount(path) " bytes not processed)", -1)
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "SUN" {
                    ; BEHAV-OPEN-002: SunImageFile — big-endian 32-byte
                    ; header, depths 1/4/8/24/32 with the 16-bit-padded
                    ; stride, optional RGB;L palette (P mode), raw or the
                    ; 0x80-escape RLE (file type 2); every other shape
                    ; collapses to the identification error.
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_sun",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -32
                        throw Error("image file is truncated (0 bytes not processed)", -1)
                    if lastStatus = -29
                        throw Error("image file is truncated (" Pillow.Image.OpenSimpleTruncatedCount(path, Pillow.Image.SunStride(path), 0) " bytes not processed)", -1)
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "GBR" {
                    ; BEHAV-OPEN-002: GbrImageFile — big-endian header
                    ; (version 1/2, depth 1 = L or 4 = RGBA), the GIMP
                    ; magic for version 2, comment/spacing info, and the
                    ; raw data block; short data raises Pillow's exact
                    ; "not enough image data" ValueError.
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_gbr",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -31
                        throw Error("not enough image data", -1)
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "FITS" {
                    ; BEHAV-OPEN-002: FitsImageFile — 80-byte card walk,
                    ; 2880-boundary data offset with Pillow's tell()-80
                    ; arithmetic, BITPIX 8/16/32/-32/-64 as L/I;16/I/F
                    ; with verbatim big-endian samples and bottom-up rows.
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_fits",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -33
                        throw Error("Truncated FITS file", -1)
                    if lastStatus = -34
                        throw Error("No image data", -1)
                    if lastStatus = -29
                        throw Error("image file is truncated (" Pillow.Image.FitsTruncatedCount(path) " bytes not processed)", -1)
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "XPM" {
                    ; BEHAV-OPEN-002: XpmImageFile — "/* XPM */" magic,
                    ; the quoted header, palette lines with "c" colors or
                    ; "None" transparency keys, P (<= 256) or RGB mode,
                    ; and the quote-joined pixel rows; header failures
                    ; collapse to the identification error while the
                    ; color/key/data errors keep Pillow's ValueError
                    ; messages (unwrapped in 11.3.0).
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_xpm",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -31
                        throw Error("not enough image data", -1)
                    if lastStatus = -35
                        throw Error("cannot read this XPM file", -1)
                    if lastStatus = -36
                        throw Error("tuple.index(x): x not in tuple", -1)
                    if lastStatus = -39
                        throw Error("b'" Pillow.Image.XpmRgbUnknownKey(path) "'", -1)
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "IPTC" {
                    ; BEHAV-OPEN-003: IptcImageFile — facade-only field
                    ; walker; raw payloads become L images and JPEG
                    ; payloads decode through the native JPEG route.
                    image := Pillow.Image.OpenIptc(path)
                    image.Format := "IPTC"
                    image.FramePath := path
                    image.FrameFormat := "IPTC"
                    return image
                } else if format = "MCIDAS" {
                    ; BEHAV-OPEN-003: McIdasImageFile — facade-only
                    ; big-endian directory parse with the stride rows
                    ; fed through the raw decoder.
                    image := Pillow.Image.OpenMcIdas(path)
                    image.Format := "MCIDAS"
                    image.FramePath := path
                    image.FrameFormat := "MCIDAS"
                    return image
                } else if format = "PSD" {
                    ; BEHAV-OPEN-004: PsdImageFile — the native opener
                    ; decodes the base image (raw or PackBits channels,
                    ; CMYK inverted, RGB;L palette, mode 1 packed bits);
                    ; layers/seek stay a documented child.
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_psd",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -47
                        throw Error("not enough channels", -1)
                    if lastStatus = -29 {
                        psdInfo := Pillow.Image.PsdHeaderInfo(path)
                        payload := 0
                        file := FileOpen(path, "r")
                        try {
                            payload := file.Length - psdInfo["DataStart"]
                        } finally {
                            file.Close()
                        }
                        if psdInfo["Width"] > 0 && payload > 0
                            throw Error("image file is truncated (" Mod(payload, psdInfo["Width"]) " bytes not processed)", -1)
                        throw Error("image file is truncated (0 bytes not processed)", -1)
                    }
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "FLI" {
                    ; BEHAV-OPEN-005: FliImageFile — the native opener
                    ; parses the 128-byte header (magic, zero field,
                    ; duration), walks the F100 prefix + frame-0 COLOR
                    ; subchunks for the palette, and decodes frame 0
                    ; (BLACK/COPY/BRUN/LC/SS2 with Pillow's exact
                    ; out-of-bounds accounting). Frame seeking stays a
                    ; documented child; n_frames/is_animated come from
                    ; FrameCountForOpen.
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_fli",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -48
                        throw Error("buffer overrun when reading image file", -1)
                    if lastStatus = -49
                        throw Error("unrecognized data stream contents when reading image file", -1)
                    if lastStatus = -50
                        throw Error("broken data stream when reading image file", -1)
                    if lastStatus = -51 {
                        fliUnprocessed := 0
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_fli_truncation_count",
                            "Ptr", pathBytes,
                            "Int64*", &fliUnprocessed,
                            "Int"
                        ))
                        throw Error("image file is truncated (" fliUnprocessed " bytes not processed)", -1)
                    }
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "MIC" {
                    ; BEHAV-OPEN-006: MicImageFile — the native opener
                    ; parses the OLE2/CFB container (v3: 512-byte
                    ; sectors, mini FAT and mini stream), finds the
                    ; first *.ACI/Image stream (case-sensitive
                    ; endswith/name matching, olefile's sorted order),
                    ; and decodes it through the native TIFF route.
                    ; Pillow's seek(0) resets n_frames to the TIFF IFD
                    ; count, so multi-ACI files report n_frames 1 and
                    ; seek(1) raises the sequence EOFError — the eager
                    ; facade pins FrameCount 1/is_animated false.
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_mic",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -52
                        throw Error("bytes length not a multiple of item size", -1)
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "PCD" {
                    ; BEHAV-OPEN-007: PcdImageFile — the native opener
                    ; decodes the 768x512 base image at sector 96 with
                    ; the PhotoYCC lookup tables. Orientations 1/3 crash
                    ; Pillow's load_end (ImagingCore has no rotate), so
                    ; the eager facade surfaces that AttributeError at
                    ; Open; truncated data uses Pillow's mod-2304
                    ; "bytes not processed" count.
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_pcd",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -53
                        throw Error("'ImagingCore' object has no attribute 'rotate'", -1)
                    if lastStatus = -54 {
                        file := FileOpen(path, "r")
                        pcdLength := 0
                        try {
                            pcdLength := file.Length
                        } finally {
                            file.Close()
                        }
                        pcdUnprocessed := Mod(Max(0, pcdLength - 196608), 2304)
                        throw Error("image file is truncated (" pcdUnprocessed " bytes not processed)", -1)
                    }
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "MPEG" {
                    ; BEHAV-OPEN-008: MpegImageFile parses the
                    ; 0x000001B3 sequence-header bits into an RGB
                    ; (w,h) image with no tile, so Pillow's load
                    ; raises "cannot load this image" — the eager
                    ; facade surfaces that at Open. Bad magic and
                    ; short streams collapse to the identification
                    ; error; n_frames/is_animated are not exposed.
                    if !Pillow.Image.MpegAccepts(path)
                        throw Error("cannot identify image file <" path ">", -1)
                    throw Error("cannot load this image", -1)
                } else if format = "WMF" {
                    ; BEHAV-OPEN-009: WmfImageFile — the native opener
                    ; parses the 44-byte placeable/EMF header (size and
                    ; dpi math) and renders the metafile through GDI
                    ; (SetWinMetaFileBits/SetEnhMetaFileBits -> a white
                    ; 24-bit DIB -> EnumEnhMetaFile), mirroring Pillow's
                    ; display.c drawwmf. The dpi-override load(dpi)
                    ; stays a documented child (eager 72-dpi decode).
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_wmf",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = -55
                        throw Error("Invalid inch", -1)
                    if lastStatus = -56
                        throw Error("cannot load metafile", -1)
                    if lastStatus = -57
                        throw Error("cannot create bitmap", -1)
                    if lastStatus = -58
                        throw Error("cannot select bitmap", -1)
                    if lastStatus = -3
                        throw Error("cannot identify image file <" path ">", -1)
                } else if format = "HDF5" || format = "BUFR" || format = "GRIB" {
                    ; BEHAV-OPEN-001: the HDF5/BUFR/GRIB stub plugins
                    ; accept the magic and open an F(1,1) image whose
                    ; load raises "cannot find loader for this X file" —
                    ; the eager facade surfaces that load error at Open
                    ; (no handler is ever registered in this runtime).
                    if !Pillow.Image.StubAccepts(path, format)
                        throw Error("cannot identify image file <" path ">", -1)
                    throw Error("cannot find loader for this " format " file", -1)
                } else if format = "IMT" {
                    ; BEHAV-OPEN-001: Pillow 11.3.0 registers no IMT
                    ; format at all (the IM plugin only maps ".im"), so
                    ; identification fails.
                    throw Error("cannot identify image file <" path ">", -1)
                } else if format = "MPO" {
                    ; BEHAV-MPO-001: Pillow opens MPO through the JPEG
                    ; factory — a file WITH the MPF index reports format
                    ; MPO and a plain JPEG in an .mpo reports JPEG; the
                    ; eager facade decodes frame 0 through the native
                    ; JPEG open (n_frames/seek stay a documented child).
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_jpeg",
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                    if lastStatus = 0
                        format := Pillow.Image.MpoHasIndex(path) ? "MPO" : "JPEG"
                } else {
                    lastStatus := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_open_" StrLower(format),
                        "Ptr", pathBytes,
                        "Ptr*", &outHandle,
                        "Int"
                    )
                }
                if lastStatus = 0 {
                    image := Pillow.WrapImageHandle(outHandle)
                    try {
                        image.Format := format
                        image.FramePath := path
                        image.FrameFormat := format
                        image.FrameIndex := 0
                        image.FrameCount := Pillow.Image.FrameCountForOpen(pathBytes, format)
                        image.ApplyNativeMetadata()
                        image.ApplyFrameMetadata()
                        if format = "ICNS" {
                            ; BEHAV-ICNS-001: expose info["sizes"] like
                            ; Pillow's IcnsImageFile, and remember the
                            ; pre-load rawmode quirk: Pillow's tobytes()
                            ; snapshots self.mode (RGBA) BEFORE load(), so
                            ; the first tobytes() on a non-RGBA best icon
                            ; raises "No packer found from {mode} to RGBA"
                            ; while RGB packs to RGBA. The eager facade
                            ; replays that on the next ToBytes() call; any
                            ; other handle-touching access clears it.
                            image.Info["sizes"] := Pillow.Image.IcnsSizes(path)
                            if image.Mode != "RGBA"
                                image.IcnsQuirkPending := true
                        }
                        if format = "GBR" {
                            ; BEHAV-OPEN-002: expose info["comment"] and
                            ; info["spacing"] like Pillow's GbrImageFile.
                            gbrInfo := Pillow.Image.GbrInfo(path)
                            if gbrInfo.Has("comment")
                                image.Info["comment"] := gbrInfo["comment"]
                            if gbrInfo.Has("spacing")
                                image.Info["spacing"] := gbrInfo["spacing"]
                        }
                        if format = "XPM" {
                            ; BEHAV-OPEN-002: Pillow's XpmImageFile stores
                            ; info["transparency"] as the "c None" key.
                            transparencyKey := Pillow.Image.XpmTransparencyKey(path)
                            if transparencyKey != ""
                                image.Info["transparency"] := transparencyKey
                        }
                        if format = "PSD" {
                            ; BEHAV-OPEN-004: Pillow exposes
                            ; info["icc_profile"] from resource 1039.
                            icc := Pillow.Image.PsdIcc(path)
                            if icc
                                image.Info["icc_profile"] := icc
                        }
                        if format = "FLI" {
                            ; BEHAV-OPEN-005: Pillow exposes
                            ; info["duration"] (AF11 speed-jiffies
                            ; scaled *1000//70, AF12 raw milliseconds).
                            image.Info["duration"] := Pillow.Image.FliDuration(path)
                        }
                    } catch {
                        image.Close()
                        throw
                    }
                    return image
                }
                if outHandle
                    Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", outHandle, "Int"))
            }
            Pillow.CheckStatus(lastStatus)
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

        static FromBuffer(modeName, size, data, decoder := "raw", rawmode := unset, stride := 0, orientation := 1) {
            if size.Length != 2
                throw Error("Pillow.Image.FromBuffer expects size [width, height]", -1)
            if decoder != "raw"
                throw Error("Pillow.Image.FromBuffer currently supports only the raw decoder", -1)
            if !IsSet(rawmode)
                rawmode := modeName
            if !(modeName = "L" || modeName = "RGB" || modeName = "RGBA" || modeName = "RGBX")
                throw Error("Pillow.Image.FromBuffer currently supports L, RGB, RGBA, and RGBX", -1)

            dataBuffer := Pillow.Image.BinaryBuffer(data, "Pillow.Image.FromBuffer")
            rawModeBytes := Pillow.Image.RawModeBuffer(rawmode)
            aliasSource := (rawmode = "L" || rawmode = "RGBA" || rawmode = "RGBX") ? 1 : 0
            handle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_frombuffer_raw",
                "Int", size[1],
                "Int", size[2],
                "Int", Pillow.ModeId(modeName),
                "Ptr", dataBuffer,
                "UPtr", dataBuffer.Size,
                "Ptr", rawModeBytes,
                "Int", stride,
                "Int", orientation,
                "Int", aliasSource,
                "Ptr*", &handle,
                "Int"
            ))
            image := Pillow.WrapImageHandle(handle)
            image.BufferViewSource := aliasSource ? dataBuffer : 0
            return image
        }

        FromBytes(bytes, decoder := unset, rawmode := unset, stride := 0, orientation := 1) {
            this.DetachBufferView()
            if IsSet(decoder) {
                if decoder != "raw"
                    throw Error("Pillow.Image.FromBytes currently supports only the raw decoder", -1)
                if !IsSet(rawmode)
                    rawmode := this.Mode
                rawModeBytes := Pillow.Image.RawModeBuffer(rawmode)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_set_raw_bytes",
                    "Ptr", this.RequireHandle(),
                    "Ptr", bytes,
                    "UPtr", bytes.Size,
                    "Ptr", rawModeBytes,
                    "Int", stride,
                    "Int", orientation,
                    "Int"
                ))
            } else {
                rawModeBytes := Pillow.Image.RawModeBuffer(this.Mode)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_set_raw_bytes",
                    "Ptr", this.RequireHandle(),
                    "Ptr", bytes,
                    "UPtr", bytes.Size,
                    "Ptr", rawModeBytes,
                    "Int", 0,
                    "Int", 1,
                    "Int"
                ))
            }
            return this
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
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_blend",
                "Ptr", left.RequireHandle(),
                "Ptr", right.RequireHandle(),
                "Double", alpha,
                "Ptr*", &outHandle,
                "Int"
            )
            if status != 0 && left.Mode = right.Mode && (left.Mode = "I" || left.Mode = "F")
                throw Error("image has wrong mode", -1)
            Pillow.CheckStatus(status)
            return left.WrapDerivedHandle(outHandle)
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
            return image2.WrapDerivedHandle(outHandle)
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
            return dst.WrapDerivedHandle(outHandle)
        }

        static alpha_composite(dst, src) {
            return Pillow.Image.AlphaComposite(dst, src)
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
            return bands[1].WrapDerivedHandle(outHandle)
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

        static ReadI32(buf, offset) {
            return NumGet(buf, offset, "Int")
        }

        static WriteI32(buf, offset, value, operationName := "Pillow.Image") {
            if !(value is Number)
                throw Error(operationName " expects numeric I-mode pixel values", -1)
            NumPut("UInt", Integer(value) & 0xFFFFFFFF, buf, offset)
        }

        static ReadF32(buf, offset) {
            return NumGet(buf, offset, "Float")
        }

        static WriteF32(buf, offset, value, operationName := "Pillow.Image") {
            if !(value is Number)
                throw Error(operationName " expects numeric F-mode pixel values", -1)
            NumPut("Float", value + 0.0, buf, offset)
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

        static BinaryBuffer(value, operationName) {
            if IsObject(value) {
                if Type(value) = "Buffer"
                    return value
                if Type(value) = "Pillow.Image.Exif"
                    return value.ToBytes()
            }
            return Pillow.Image.ByteBuffer(value, operationName)
        }

        static NativeMetadataBlob(handle, exportName) {
            hasBlob := 0
            required := 0
            status := DllCall(
                Pillow.RequireDllPath() "\" exportName,
                "Ptr", handle,
                "Int*", &hasBlob,
                "Ptr", 0,
                "UPtr", 0,
                "UPtr*", &required,
                "Int"
            )
            if status != -1
                Pillow.CheckStatus(status)
            if !hasBlob
                return 0
            blob := Buffer(required, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\" exportName,
                "Ptr", handle,
                "Int*", &hasBlob,
                "Ptr", blob,
                "UPtr", blob.Size,
                "UPtr*", &required,
                "Int"
            ))
            return blob
        }

        static XmpNodeLocalName(node) {
            name := node.baseName
            if name != ""
                return name
            name := node.nodeName
            colon := InStr(name, ":")
            return colon ? SubStr(name, colon + 1) : name
        }

        static XmpElementToMap(node) {
            result := Map()
            attrs := node.attributes
            loop attrs.length {
                attr := attrs.item(A_Index - 1)
                attrName := attr.nodeName
                if attrName = "xmlns" || SubStr(attrName, 1, 6) = "xmlns:"
                    continue
                result[Pillow.Image.XmpNodeLocalName(attr)] := attr.text
            }

            textValue := ""
            children := node.childNodes
            loop children.length {
                child := children.item(A_Index - 1)
                if child.nodeType = 1 {
                    childName := Pillow.Image.XmpNodeLocalName(child)
                    childValue := Pillow.Image.XmpElementToMap(child)
                    if result.Has(childName) {
                        existingValue := result[childName]
                        if !(existingValue is Array)
                            result[childName] := [existingValue]
                        result[childName].Push(childValue)
                    } else {
                        result[childName] := childValue
                    }
                } else if child.nodeType = 3 || child.nodeType = 4 {
                    textValue .= child.nodeValue
                }
            }
            textValue := Trim(textValue, " `t`r`n")
            if textValue != ""
                result["text"] := textValue
            if result.Count = 1 && result.Has("text")
                return result["text"]
            return result
        }

        static ParseXmpBuffer(xmpBuffer) {
            doc := ComObject("MSXML2.DOMDocument.6.0")
            doc.async := false
            doc.validateOnParse := false
            doc.resolveExternals := false
            xmpText := StrGet(xmpBuffer.Ptr, xmpBuffer.Size, "UTF-8")
            if !doc.loadXML(xmpText)
                throw Error("Pillow.Image.getxmp could not parse XMP packet: " doc.parseError.reason, -1)
            root := doc.documentElement
            if !IsObject(root)
                throw Error("Pillow.Image.getxmp could not parse XMP packet", -1)
            parsed := Map()
            parsed[Pillow.Image.XmpNodeLocalName(root)] := Pillow.Image.XmpElementToMap(root)
            return parsed
        }

        static NativeJpegQTables(handle) {
            count := 0
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_metadata_jpeg_qtable_count",
                "Ptr", handle,
                "UPtr*", &count,
                "Int"
            )
            Pillow.CheckStatus(status)
            if count < 1
                throw Error("Cannot use 'keep' when original image is not a JPEG", -1)
            if count > 2
                throw Error("Pillow.Image.Save JPEG keep currently supports one or two quantization tables", -1)
            buf := Buffer(count * 64 * 4, 0)
            loop count {
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_metadata_jpeg_qtable",
                    "Ptr", handle,
                    "UPtr", A_Index - 1,
                    "Ptr", buf.Ptr + ((A_Index - 1) * 64 * 4),
                    "UPtr", 64,
                    "Int"
                )
                Pillow.CheckStatus(status)
            }
            return { Buffer: buf, Count: count }
        }

        static NativeJpegSubsampling(handle) {
            subsampling := -1
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_metadata_jpeg_subsampling",
                "Ptr", handle,
                "Int*", &subsampling,
                "Int"
            )
            Pillow.CheckStatus(status)
            return subsampling
        }

        static SavePdfInfoString(options, names*) {
            ; BEHAV-PDF-001: Pillow's PDF info entries only accept strings
            ; (pdf_repr(str) -> UTF-16BE with BOM); an empty string is
            ; falsy in the plugin's "if v:" guard and is omitted, and an
            ; unset option returns 0 (nullptr -> the native default).
            option := Pillow.Image.SaveOption(options, names*)
            if !option.Set
                return 0
            if !(option.Value is String)
                throw Error("Pillow.Image.Save PDF info values must be strings", -1)
            if StrLen(option.Value) = 0
                return 0
            return Pillow.Image.Utf8Buffer(option.Value)
        }

        static SaveJpegCommentBuffer(commentOption, handle, useOpenedComment) {
            if commentOption.Set {
                if commentOption.Value is String {
                    comment := Pillow.Image.Utf8Buffer(commentOption.Value)
                    return { Buffer: comment, Size: comment.Size - 1 }
                }
                comment := Pillow.Image.BinaryBuffer(commentOption.Value, "Pillow.Image.Save comment")
                return { Buffer: comment, Size: comment.Size }
            }
            if useOpenedComment {
                comment := Pillow.Image.NativeMetadataBlob(handle, "pillow_c_image_metadata_jpeg_comment")
                if comment
                    return { Buffer: comment, Size: comment.Size }
            }
            return { Buffer: 0, Size: 0 }
        }

        static SaveGifCommentBuffer(commentOption) {
            if !commentOption.Set
                return { Buffer: 0, Size: 0 }
            if commentOption.Value is String {
                comment := Pillow.Image.Utf8Buffer(commentOption.Value)
                return { Buffer: comment, Size: comment.Size - 1 }
            }
            comment := Pillow.Image.BinaryBuffer(commentOption.Value, "Pillow.Image.Save comment")
            return { Buffer: comment, Size: comment.Size }
        }

        static NativeGifComment(path, frameIndex) {
            hasComment := 0
            required := 0
            pathBytes := Pillow.Image.Utf8Buffer(path)
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_gif_comment",
                "Ptr", pathBytes,
                "Int", frameIndex,
                "Int*", &hasComment,
                "Ptr", 0,
                "UPtr", 0,
                "UPtr*", &required,
                "Int"
            )
            if status != -1
                Pillow.CheckStatus(status)
            if !hasComment
                return 0
            comment := Buffer(required, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_gif_comment",
                "Ptr", pathBytes,
                "Int", frameIndex,
                "Int*", &hasComment,
                "Ptr", comment,
                "UPtr", comment.Size,
                "UPtr*", &required,
                "Int"
            ))
            return comment
        }

        static NormalizePaletteRawmode(rawmode, operationName, allowExpanded := false) {
            if !(rawmode is String)
                throw Error(operationName " rawmode expects a string", -1)
            name := StrUpper(rawmode)
            if name = "RGB" || name = "BGR"
                return name
            if allowExpanded && (name = "RGBA" || name = "RGBX" || name = "BGRX")
                return name
            throw Error(operationName " currently supports RGB BGR RGBA RGBX and BGRX palettes", -1)
        }

        static PaletteBuffer(values, rawmode, operationName) {
            rawmode := Pillow.Image.NormalizePaletteRawmode(rawmode, operationName)
            buf := Pillow.Image.ByteBuffer(values, operationName)
            if Mod(buf.Size, 3) != 0
                throw Error(operationName " palette length must be a multiple of 3", -1)
            if rawmode = "RGB"
                return buf

            out := Buffer(buf.Size, 0)
            loop buf.Size // 3 {
                offset := (A_Index - 1) * 3
                NumPut("UChar", NumGet(buf, offset + 2, "UChar"), out, offset)
                NumPut("UChar", NumGet(buf, offset + 1, "UChar"), out, offset + 1)
                NumPut("UChar", NumGet(buf, offset, "UChar"), out, offset + 2)
            }
            return out
        }

        static PaletteRgbaBuffer(values, rawmode, operationName) {
            rawmode := Pillow.Image.NormalizePaletteRawmode(rawmode, operationName, true)
            buf := Pillow.Image.ByteBuffer(values, operationName)
            if Mod(buf.Size, 4) != 0
                throw Error(operationName " palette length must be a multiple of 4", -1)
            if !(rawmode = "RGBA" || rawmode = "RGBX" || rawmode = "BGRX")
                throw Error(operationName " rawmode does not contain four palette bytes", -1)

            out := Buffer(buf.Size, 0)
            loop buf.Size // 4 {
                offset := (A_Index - 1) * 4
                if rawmode = "BGRX" {
                    NumPut("UChar", NumGet(buf, offset + 2, "UChar"), out, offset)
                    NumPut("UChar", NumGet(buf, offset + 1, "UChar"), out, offset + 1)
                    NumPut("UChar", NumGet(buf, offset, "UChar"), out, offset + 2)
                    NumPut("UChar", 255, out, offset + 3)
                } else {
                    loop 4
                        NumPut("UChar", NumGet(buf, offset + A_Index - 1, "UChar"), out, offset + A_Index - 1)
                }
            }
            return out
        }

        static PaletteAlphaModeForPut(rawmode) {
            if rawmode = "RGBA"
                return 1
            if rawmode = "RGBX"
                return 2
            return 0
        }

        static ConvertRgbPaletteValues(values, rawmode, operationName) {
            rawmode := Pillow.Image.NormalizePaletteRawmode(rawmode, operationName, true)
            if Mod(values.Length, 3) != 0
                throw Error(operationName " palette length must be a multiple of 3", -1)
            if rawmode = "RGB"
                return values

            out := []
            loop values.Length // 3 {
                index := (A_Index - 1) * 3 + 1
                if rawmode = "BGR" {
                    out.Push(values[index + 2])
                    out.Push(values[index + 1])
                    out.Push(values[index])
                } else if rawmode = "BGRX" {
                    out.Push(values[index + 2])
                    out.Push(values[index + 1])
                    out.Push(values[index])
                    out.Push(0)
                } else {
                    out.Push(values[index])
                    out.Push(values[index + 1])
                    out.Push(values[index + 2])
                    out.Push(255)
                }
            }
            return out
        }

        static ConvertRgbaPaletteValues(values, rawmode, operationName) {
            rawmode := Pillow.Image.NormalizePaletteRawmode(rawmode, operationName, true)
            if Mod(values.Length, 4) != 0
                throw Error(operationName " palette length must be a multiple of 4", -1)
            if rawmode = "RGBA" || rawmode = "RGBX"
                return values

            out := []
            loop values.Length // 4 {
                index := (A_Index - 1) * 4 + 1
                if rawmode = "RGB" {
                    out.Push(values[index])
                    out.Push(values[index + 1])
                    out.Push(values[index + 2])
                } else if rawmode = "BGR" {
                    out.Push(values[index + 2])
                    out.Push(values[index + 1])
                    out.Push(values[index])
                } else {
                    out.Push(values[index + 2])
                    out.Push(values[index + 1])
                    out.Push(values[index])
                    out.Push(0)
                }
            }
            return out
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

        static Utf8StringFromBytes(buf, byteCount, operationName) {
            text := ""
            offset := 0
            while offset < byteCount {
                b1 := NumGet(buf, offset, "UChar")
                if b1 < 0x80 {
                    codepoint := b1
                    offset += 1
                } else if b1 >= 0xC2 && b1 <= 0xDF {
                    if offset + 1 >= byteCount
                        throw Error(operationName " contains truncated UTF-8", -1)
                    b2 := NumGet(buf, offset + 1, "UChar")
                    if (b2 & 0xC0) != 0x80
                        throw Error(operationName " contains invalid UTF-8", -1)
                    codepoint := ((b1 & 0x1F) << 6) | (b2 & 0x3F)
                    offset += 2
                } else if b1 >= 0xE0 && b1 <= 0xEF {
                    if offset + 2 >= byteCount
                        throw Error(operationName " contains truncated UTF-8", -1)
                    b2 := NumGet(buf, offset + 1, "UChar")
                    b3 := NumGet(buf, offset + 2, "UChar")
                    if (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80
                        throw Error(operationName " contains invalid UTF-8", -1)
                    codepoint := ((b1 & 0x0F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F)
                    if codepoint < 0x800 || (codepoint >= 0xD800 && codepoint <= 0xDFFF)
                        throw Error(operationName " contains invalid UTF-8", -1)
                    offset += 3
                } else if b1 >= 0xF0 && b1 <= 0xF4 {
                    if offset + 3 >= byteCount
                        throw Error(operationName " contains truncated UTF-8", -1)
                    b2 := NumGet(buf, offset + 1, "UChar")
                    b3 := NumGet(buf, offset + 2, "UChar")
                    b4 := NumGet(buf, offset + 3, "UChar")
                    if (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80 || (b4 & 0xC0) != 0x80
                        throw Error(operationName " contains invalid UTF-8", -1)
                    codepoint := ((b1 & 0x07) << 18) | ((b2 & 0x3F) << 12) | ((b3 & 0x3F) << 6) | (b4 & 0x3F)
                    if codepoint < 0x10000 || codepoint > 0x10FFFF
                        throw Error(operationName " contains invalid UTF-8", -1)
                    offset += 4
                } else {
                    throw Error(operationName " contains invalid UTF-8", -1)
                }
                text .= Chr(codepoint)
            }
            return text
        }

        static Latin1Buffer(value, operationName) {
            length := StrLen(value)
            buf := Buffer(length + 1, 0)
            loop length {
                codepoint := Ord(SubStr(value, A_Index, 1))
                if codepoint > 255
                    throw Error(operationName " contains characters outside Latin-1", -1)
                NumPut("UChar", codepoint, buf, A_Index - 1)
            }
            return buf
        }

        static NulTerminatedByteBuffer(value, operationName) {
            if !(IsObject(value) && Type(value) = "Buffer")
                throw Error(operationName " expects a Buffer", -1)
            buf := Buffer(value.Size + 1, 0)
            loop value.Size {
                byte := NumGet(value, A_Index - 1, "UChar")
                if byte = 0
                    throw Error(operationName " bytes with embedded NUL are not supported", -1)
                NumPut("UChar", byte, buf, A_Index - 1)
            }
            return buf
        }

        static OpenImHandle(path, &outHandle) {
            ; BEHAV-IM-001: parse the ASCII header (lines end at the first
            ; NUL or ^Z), rebuild the raw payload into native storage, and
            ; reattach the P-mode LUT as the image palette.
            source := FileOpen(path, "r")
            if !source
                throw Error("Pillow.Image.Open IM failed to open the source file", -1)
            size := source.Length
            if size < 40 {
                source.Close()
                outHandle := 0
                return -3
            }
            payload := Buffer(size, 0)
            source.RawRead(payload, size)
            source.Close()
            foundLf := false
            loop (size > 100 ? 100 : size) {
                if NumGet(payload, A_Index - 1, "UChar") = 10 {
                    foundLf := true
                    break
                }
            }
            if !foundLf {
                outHandle := 0
                return -3
            }
            textLen := 0
            headerEnd := 0
            loop payload.Size {
                byte := NumGet(payload, A_Index - 1, "UChar")
                if byte = 0 || byte = 0x1A {
                    if !textLen
                        textLen := A_Index - 1
                    if byte = 0x1A {
                        headerEnd := A_Index - 1
                        break
                    }
                }
            }
            if !headerEnd {
                outHandle := 0
                return -3
            }
            header := StrGet(payload.Ptr, textLen, "UTF-8")
            types := Map(
                "L", ["L", "L"],
                "Greyscale", ["L", "L"],
                "Grayscale", ["L", "L"],
                "RGB", ["RGB", "RGB;L"],
                "RGBA", ["RGBA", "RGBA;L"],
                "0 1", ["1", "1"],
                "L 1", ["1", "1"],
                "B1", ["1", "1"],
                "P", ["P", "P"],
                "LA", ["LA", "LA;L"],
                "CMYK", ["CMYK", "CMYK;L"],
                "L 32S", ["I", "I;32S"],
                "L 16", ["I;16", "I;16"],
                "L 16B", ["I;16B", "I;16B"],
                "L 32F", ["F", "F;32F"]
            )
            mode := "L"
            rawmode := "L"
            width := 512
            height := 512
            hasLut := false
            for line in StrSplit(header, "`r`n") {
                parts := StrSplit(line, ": ", , 2)
                if parts.Length < 2
                    continue
                key := parts[1]
                value := parts[2]
                if key = "Image type" {
                    if RegExMatch(value, "^(.*) image$", &typeMatch) && types.Has(typeMatch[1]) {
                        mode := types[typeMatch[1]][1]
                        rawmode := types[typeMatch[1]][2]
                    } else {
                        outHandle := 0
                        return -3
                    }
                } else if key = "Image size (x*y)" {
                    if RegExMatch(value, "^(\d+)\*(\d+)$", &sizeMatch) {
                        width := Integer(sizeMatch[1])
                        height := Integer(sizeMatch[2])
                    } else {
                        outHandle := 0
                        return -3
                    }
                } else if key = "Lut" {
                    hasLut := value = "1"
                }
            }
            if hasLut && (mode = "L" || mode = "P") {
                ; Pillow 11.3.0 promotes to P only for a NON-greyscale LUT;
                ; a greyscale LUT keeps mode L (Pillow may attach a lut,
                ; recorded as a boundary note for the display LUT).
                greyscale := true
                loop 256 {
                    r := NumGet(payload, headerEnd + 1 + A_Index - 1, "UChar")
                    g := NumGet(payload, headerEnd + 1 + 256 + A_Index - 1, "UChar")
                    b := NumGet(payload, headerEnd + 1 + 512 + A_Index - 1, "UChar")
                    if r != g || g != b {
                        greyscale := false
                        break
                    }
                }
                if !greyscale {
                    mode := "P"
                    rawmode := "P"
                }
            }
            if width <= 0 || height <= 0 {
                outHandle := 0
                return -3
            }
            dataOffset := headerEnd + 1
            if hasLut
                dataOffset += 768
            bytesPerPixel := 0
            switch mode {
                case "L", "P": bytesPerPixel := 1
                case "LA": bytesPerPixel := 2
                case "RGB": bytesPerPixel := 3
                case "RGBA", "CMYK", "I", "F": bytesPerPixel := 4
                case "I;16", "I;16B": bytesPerPixel := 2
            }
            pixelCount := width * height * bytesPerPixel
            if mode = "1"
                pixelCount := ((width + 7) // 8) * height
            if dataOffset + pixelCount > size {
                outHandle := 0
                return -3
            }
            handle := 0
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_create_mode",
                "Int", width,
                "Int", height,
                "Int", Pillow.ModeId(mode),
                "Ptr*", &handle,
                "Int"
            )
            if status != 0 {
                outHandle := 0
                return status
            }
            try {
                rawModeBytes := Pillow.Image.RawModeBuffer(rawmode)
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_set_raw_bytes",
                    "Ptr", handle,
                    "Ptr", payload.Ptr + dataOffset,
                    "UPtr", pixelCount,
                    "Ptr", rawModeBytes,
                    "Int", 0,
                    "Int", -1,
                    "Int"
                )
                if status != 0
                    throw Error("pillow_c: " Pillow.StatusMessage(status), status)
                if hasLut && mode = "P" {
                    lutOffset := headerEnd + 1
                    rgb := []
                    loop 256 {
                        rgb.Push(NumGet(payload, lutOffset + A_Index - 1, "UChar"))
                        rgb.Push(NumGet(payload, lutOffset + 256 + A_Index - 1, "UChar"))
                        rgb.Push(NumGet(payload, lutOffset + 512 + A_Index - 1, "UChar"))
                    }
                    paletteBytes := Pillow.Image.PaletteBuffer(rgb, "RGB", "Pillow.Image.Open IM")
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_put_palette_rgb",
                        "Ptr", handle,
                        "Ptr", paletteBytes,
                        "UPtr", paletteBytes.Size,
                        "Int"
                    ))
                }
                outHandle := handle
                return 0
            } catch Error as err {
                DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", handle, "Int")
                outHandle := 0
                throw
            } catch {
                DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", handle, "Int")
                outHandle := 0
                return -3
            }
        }

        static ResolveOpenFormat(path, formats := unset) {
            return Pillow.Image.ResolveOpenFormats(path, IsSet(formats) ? formats : unset)[1]
        }

        static OpenSpiderHandle(path, &outHandle) {
            ; BEHAV-SPIDER-001: the header carries labbyt at written float
            ; 21 (byte 84) and the size at floats 11/1 (bytes 44/4); the
            ; samples are native little-endian float32.
            source := FileOpen(path, "r")
            if !source
                throw Error("Pillow.Image.Open SPIDER failed to open the source file", -1)
            size := source.Length
            if size < 92 {
                source.Close()
                outHandle := 0
                return -3
            }
            payload := Buffer(size, 0)
            source.RawRead(payload, size)
            source.Close()
            labbyt := Integer(NumGet(payload, 84, "Float"))
            nsam := Integer(NumGet(payload, 44, "Float"))
            nrow := Integer(NumGet(payload, 4, "Float"))
            if labbyt < 92 || nsam <= 0 || nrow <= 0 || labbyt + nsam * nrow * 4 > size {
                outHandle := 0
                return -3
            }
            handle := 0
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_create_mode",
                "Int", nsam,
                "Int", nrow,
                "Int", Pillow.ModeId("F"),
                "Ptr*", &handle,
                "Int"
            )
            if status != 0 {
                outHandle := 0
                return status
            }
            try {
                rawModeBytes := Pillow.Image.RawModeBuffer("F;32F")
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_set_raw_bytes",
                    "Ptr", handle,
                    "Ptr", payload.Ptr + labbyt,
                    "UPtr", nsam * nrow * 4,
                    "Ptr", rawModeBytes,
                    "Int", 0,
                    "Int", 1,
                    "Int"
                )
                if status != 0
                    throw Error("pillow_c: " Pillow.StatusMessage(status), status)
                outHandle := handle
                return 0
            } catch Error as err {
                DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", handle, "Int")
                outHandle := 0
                throw
            } catch {
                DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", handle, "Int")
                outHandle := 0
                return -3
            }
        }

        static ResolveOpenFormats(path, formats := unset) {
            if IsSet(formats) {
                if !IsObject(formats)
                    throw Error("Pillow.Image.Open formats expects an array", -1)
                if formats.Length < 1
                    throw Error("Pillow.Image.Open formats must not be empty", -1)
                normalized := []
                for format in formats
                    normalized.Push(Pillow.Image.NormalizeFileFormat(format))
                return normalized
            }
            return [Pillow.Image.FormatFromPath(path)]
        }

        static ResolveSaveFormat(path, format := unset) {
            if IsSet(format)
                return Pillow.Image.NormalizeFileFormat(format)
            return Pillow.Image.FormatFromPath(path)
        }

        static SgiTruncatedCount(path) {
            ; BEHAV-SGI-001: Pillow's raw decoder reports the leftover
            ; bytes of the failing band tile modulo the row width
            ; (probed: remaining % xsize for the first short band).
            file := FileOpen(path, "r")
            if !file
                return 0
            try {
                header := Buffer(12, 0)
                file.RawRead(header, 12)
                xsize := (NumGet(header, 6, "UChar") << 8) | NumGet(header, 7, "UChar")
                ysize := (NumGet(header, 8, "UChar") << 8) | NumGet(header, 9, "UChar")
                zsize := (NumGet(header, 10, "UChar") << 8) | NumGet(header, 11, "UChar")
                payload := file.Length - 512
                pagesize := xsize * ysize
                tile := 0
                while tile < zsize {
                    remaining := payload - tile * pagesize
                    if remaining < pagesize
                        return Mod(remaining, xsize)
                    tile += 1
                }
                return 0
            } finally {
                file.Close()
            }
        }

        static DdsOpenError(status, path) {
            ; BEHAV-DDS-001: rebuild Pillow's exact DDS open error
            ; messages from the file header (the native decoder returns
            ; local status codes for each shape).
            file := FileOpen(path, "r")
            if !file
                return "pillow_c: status " status
            try {
                header := Buffer(148, 0)
                file.RawRead(header, Min(file.Length, 148))
                total := file.Length
                pfflags := NumGet(header, 80, "UInt")
                fourcc := NumGet(header, 84, "UInt")
                bitcount := NumGet(header, 88, "UInt")
                if status = -9
                    return "Unsupported header size " NumGet(header, 4, "UInt")
                if status = -10
                    return "Incomplete header: " (total - 8) " bytes"
                if status = -11
                    return "Unknown pixel format flags " pfflags
                if status = -12
                    return "Unimplemented pixel format " fourcc
                if status = -13
                    return "Unsupported bitcount " bitcount " for " pfflags
                if status = -14
                    return "Unimplemented DXGI format " NumGet(header, 128, "UInt")
                if status = -15 || status = -18 {
                    blocksize := status = -15 ? 8 : 16
                    offset := fourcc = 0x30315844 ? 148 : 128
                    return "image file is truncated (" Mod(total - offset, blocksize) " bytes not processed)"
                }
                if status = -16 {
                    xsize := NumGet(header, 16, "UInt")
                    channels := 1
                    if (pfflags & 0x1) && bitcount = 16
                        channels := 2
                    if (pfflags & 0x4) && fourcc = 0x30315844
                        channels := 4
                    return "image file is truncated (" Mod(total - 128, xsize * channels) " bytes not processed)"
                }
                if status = -17
                    return "division by zero"
                return "pillow_c: status " status
            } finally {
                file.Close()
            }
        }

        static OpenSimpleTruncatedCount(path, rowBytes, headerSize) {
            ; BEHAV-OPEN-001: Pillow's raw decoder reports the leftover
            ; bytes of the short payload modulo the row width
            ; (probed: remaining % row bytes for PIXAR/XVTHUMB).
            file := FileOpen(path, "r")
            if !file
                return 0
            try {
                payload := file.Length - headerSize
                if payload <= 0
                    return 0
                if rowBytes <= 0
                    return 0
                return Mod(payload, rowBytes)
            } finally {
                file.Close()
            }
        }

        static PixarWidth(path) {
            file := FileOpen(path, "r")
            if !file
                return 0
            try {
                header := Buffer(512, 0)
                file.RawRead(header, Min(file.Length, 512))
                return NumGet(header, 418, "UShort")
            } finally {
                file.Close()
            }
        }

        static XvThumbHeader(path) {
            ; BEHAV-OPEN-001: rescan the XV thumbnail header ("P7 332",
            ; then "#" comments, then "W H") to recover W and the payload
            ; offset for the truncated-count message.
            file := FileOpen(path, "r")
            if !file
                return { Width: 0, PayloadOffset: 0 }
            try {
                line := file.ReadLine()
                if SubStr(line, 1, 6) != "P7 332"
                    return { Width: 0, PayloadOffset: 0 }
                loop {
                    line := file.ReadLine()
                    if line = ""
                        return { Width: 0, PayloadOffset: 0 }
                    if SubStr(line, 1, 1) = "#"
                        continue
                    parts := StrSplit(line, [" ", A_Tab])
                    width := 0
                    for part in parts {
                        if part != "" {
                            width := Integer(part)
                            break
                        }
                    }
                    return { Width: width > 0 ? width : 0, PayloadOffset: file.Pos }
                }
            } finally {
                file.Close()
            }
        }

        static XvThumbTruncatedCount(path, header) {
            file := FileOpen(path, "r")
            if !file
                return 0
            try {
                payload := file.Length - header.PayloadOffset
                if payload <= 0 || header.Width <= 0
                    return 0
                return Mod(payload, header.Width)
            } finally {
                file.Close()
            }
        }

        static DcxFrameCount(path) {
            ; BEHAV-OPEN-001: the DCX component directory is up to 1024
            ; LE32 offsets terminated by 0; n_frames = the entry count.
            file := FileOpen(path, "r")
            if !file
                return 0
            try {
                header := Buffer(4100, 0)
                file.RawRead(header, Min(file.Length, 4100))
                if file.Length < 8 || NumGet(header, 0, "UInt") != 0x3ADE68B1
                    return 0
                count := 0
                loop 1024 {
                    offset := NumGet(header, 4 + (A_Index - 1) * 4, "UInt")
                    if offset = 0
                        break
                    count += 1
                }
                return count
            } finally {
                file.Close()
            }
        }

        static FtexWidth(path) {
            file := FileOpen(path, "r")
            if !file
                return 0
            try {
                head := Buffer(24, 0)
                file.RawRead(head, Min(file.Length, 24))
                return NumGet(head, 8, "Int")
            } finally {
                file.Close()
            }
        }

        static FtexDataStart(path) {
            file := FileOpen(path, "r")
            if !file
                return 0
            try {
                head := Buffer(32, 0)
                file.RawRead(head, Min(file.Length, 32))
                return NumGet(head, 28, "UInt") + 4
            } finally {
                file.Close()
            }
        }

        static FtexTruncatedCount(path) {
            file := FileOpen(path, "r")
            if !file
                return 0
            try {
                payload := file.Length - Pillow.Image.FtexDataStart(path)
                rowBytes := Pillow.Image.FtexWidth(path) * 3
                if payload <= 0 || rowBytes <= 0
                    return 0
                return Mod(payload, rowBytes)
            } finally {
                file.Close()
            }
        }

        static FtexFormatId(path) {
            file := FileOpen(path, "r")
            if !file
                return -1
            try {
                head := Buffer(32, 0)
                file.RawRead(head, Min(file.Length, 32))
                return NumGet(head, 24, "Int")
            } finally {
                file.Close()
            }
        }

        static SunStride(path) {
            file := FileOpen(path, "r")
            if !file
                return 0
            try {
                head := Buffer(32, 0)
                file.RawRead(head, Min(file.Length, 32))
                width := (NumGet(head, 4, "UChar") << 24) | (NumGet(head, 5, "UChar") << 16) | (NumGet(head, 6, "UChar") << 8) | NumGet(head, 7, "UChar")
                depth := (NumGet(head, 12, "UChar") << 24) | (NumGet(head, 13, "UChar") << 16) | (NumGet(head, 14, "UChar") << 8) | NumGet(head, 15, "UChar")
                if width <= 0 || depth <= 0
                    return 0
                return ((width * depth + 15) // 16) * 2
            } finally {
                file.Close()
            }
        }

        static GbrInfo(path) {
            ; BEHAV-OPEN-002: Pillow exposes info["comment"] (and
            ; info["spacing"] for version 2) from the GBR header.
            file := FileOpen(path, "r")
            if !file
                return Map()
            try {
                head := Buffer(28, 0)
                file.RawRead(head, Min(file.Length, 28))
                headerSize := (NumGet(head, 0, "UChar") << 24) | (NumGet(head, 1, "UChar") << 16) | (NumGet(head, 2, "UChar") << 8) | NumGet(head, 3, "UChar")
                version := (NumGet(head, 4, "UChar") << 24) | (NumGet(head, 5, "UChar") << 16) | (NumGet(head, 6, "UChar") << 8) | NumGet(head, 7, "UChar")
                if headerSize < 20 || !(version = 1 || version = 2)
                    return Map()
                commentLength := headerSize - (version = 1 ? 20 : 28)
                if commentLength <= 0
                    return Map()
                file.Pos := version = 1 ? 20 : 28
                comment := file.Read(commentLength)
                if SubStr(comment, -1) = "`n"
                    comment := SubStr(comment, 1, -1)
                info := Map()
                info["comment"] := comment
                if version = 2
                    info["spacing"] := (NumGet(head, 24, "UChar") << 24) | (NumGet(head, 25, "UChar") << 16) | (NumGet(head, 26, "UChar") << 8) | NumGet(head, 27, "UChar")
                return info
            } finally {
                file.Close()
            }
        }

        static FliHeader(path) {
            ; BEHAV-OPEN-005: the FLI/FLC 128-byte header fields used by
            ; FliImagePlugin._open (magic, n_frames, duration).
            file := FileOpen(path, "r")
            if !file
                return Map("Magic", 0, "NFrames", 1, "Duration", 0)
            try {
                head := Buffer(24, 0)
                file.RawRead(head, Min(file.Length, 24))
                return Map(
                    "Magic", NumGet(head, 4, "UShort"),
                    "NFrames", NumGet(head, 6, "UShort"),
                    "Duration", NumGet(head, 16, "Int")
                )
            } finally {
                file.Close()
            }
        }

        static FliDuration(path) {
            ; BEHAV-OPEN-005: info["duration"] — AF11 speed-jiffies become
            ; duration * 1000 // 70 (floor division), AF12 is raw ms.
            header := Pillow.Image.FliHeader(path)
            if header["Magic"] = 0xAF11
                return Floor(header["Duration"] * 1000 / 70)
            return header["Duration"]
        }

        static FliFrameCount(path) {
            ; BEHAV-OPEN-005: Pillow's n_frames is the raw header count;
            ; is_animated is n_frames > 1 (NFrames > 1 in the facade).
            header := Pillow.Image.FliHeader(path)
            return Max(header["NFrames"], 1)
        }

        static MpegAccepts(path) {
            ; BEHAV-OPEN-008: the 0x000001B3 sequence header plus the
            ; 12+12 size bits (7 bytes total) — shorter streams raise
            ; the identification error through the i8 IndexError wrap.
            file := FileOpen(path, "r")
            if !file
                return false
            try {
                if file.Length < 7
                    return false
                head := Buffer(7, 0)
                file.RawRead(head, 7)
                return NumGet(head, 0, "UChar") = 0x00
                    && NumGet(head, 1, "UChar") = 0x00
                    && NumGet(head, 2, "UChar") = 0x01
                    && NumGet(head, 3, "UChar") = 0xB3
            } finally {
                file.Close()
            }
        }

        static FitsRowBytes(path) {
            ; BEHAV-OPEN-002: NAXIS1 * bytes-per-sample plus the data
            ; offset for the truncation row-modulo count.
            file := FileOpen(path, "r")
            if !file
                return Map("RowBytes", 0, "DataStart", 0)
            try {
                bitpix := 0
                naxis1 := 0
                dataStart := 0
                sawEnd := false
                loop {
                    recordStart := file.Pos
                    record := file.Read(80)
                    if StrLen(record) = 0
                        return Map("RowBytes", 0, "DataStart", 0)
                    recordLen := StrLen(record)
                    keyword := StrReplace(SubStr(record, 1, 8), " ", "")
                    isUnitStart := keyword = "SIMPLE" || keyword = "XTENSION"
                    if keyword = "END" {
                        file.Pos := Ceil(file.Pos / 2880) * 2880
                        sawEnd := true
                        continue
                    }
                    if sawEnd && !isUnitStart {
                        ; the first non-unit record after END breaks the
                        ; walk; Pillow's offset = tell() - 80
                        dataStart := recordStart + recordLen - 80
                        break
                    }
                    if keyword = "BITPIX" {
                        value := StrReplace(SubStr(record, 11), "/", "")
                        value := Trim(SubStr(value, InStr(value, "=") + 1))
                        negative := SubStr(value, 1, 1) = "-"
                        if negative
                            value := SubStr(value, 2)
                        parsed := 0
                        for char in StrSplit(value) {
                            if char = ""
                                continue
                            parsed := parsed * 10 + Integer(char)
                        }
                        bitpix := negative ? -parsed : parsed
                    }
                    if keyword = "NAXIS1" {
                        value := StrReplace(SubStr(record, 11), "/", "")
                        value := Trim(SubStr(value, InStr(value, "=") + 1))
                        parsed := 0
                        for char in StrSplit(value) {
                            if char = ""
                                continue
                            parsed := parsed * 10 + Integer(char)
                        }
                        naxis1 := parsed
                    }
                }
                sampleBytes := bitpix = 8 ? 1 : (bitpix = 16 ? 2 : (bitpix = 32 ? 4 : (bitpix = -32 ? 4 : 8)))
                return Map("RowBytes", naxis1 > 0 ? naxis1 * sampleBytes : 0, "DataStart", dataStart)
            } finally {
                file.Close()
            }
        }

        static FitsTruncatedCount(path) {
            file := FileOpen(path, "r")
            if !file
                return 0
            try {
                info := Pillow.Image.FitsRowBytes(path)
                payload := file.Length - info["DataStart"]
                if payload <= 0 || info["RowBytes"] <= 0
                    return 0
                return Mod(payload, info["RowBytes"])
            } finally {
                file.Close()
            }
        }

        static XpmRgbUnknownKey(path) {
            ; BEHAV-OPEN-002: Pillow's RGB-mode pixel decode looks the
            ; key up in a dict — an unknown key raises KeyError whose
            ; message is the key's bytes repr. Rescan the file to find
            ; the first unknown key.
            file := FileOpen(path, "r")
            if !file
                return ""
            try {
                file.ReadLine()
                loop {
                    line := file.ReadLine()
                    if line = ""
                        return ""
                    if SubStr(line, 1, 1) = '"' && RegExMatch(SubStr(line, 2), "^\d+ \d+ \d+ \d+")
                        break
                }
                bpp := 0
                parts := StrSplit(Trim(StrReplace(file.ReadLine(), '"', "")), " ")
                if parts.Length >= 4
                    bpp := Integer(parts[4])
                keys := Map()
                loop {
                    line := file.ReadLine()
                    if line = ""
                        return ""
                    if SubStr(line, 1, 1) != '"'
                        break
                    keys[SubStr(line, 2, bpp)] := true
                }
                loop {
                    line := file.ReadLine()
                    if line = ""
                        return ""
                    if Trim(line, " `t`r`n") = "/* pixels */"
                        continue
                    joined := ""
                    for segment in StrSplit(line, '"')
                        joined .= segment
                    index := 1
                    while index + bpp - 1 <= StrLen(joined) {
                        key := SubStr(joined, index, bpp)
                        if !keys.Has(key)
                            return key
                        index += bpp
                    }
                }
            } finally {
                file.Close()
            }
        }

        static XpmTransparencyKey(path) {
            ; BEHAV-OPEN-002: Pillow stores info["transparency"] as the
            ; key of the first "c None" palette entry.
            file := FileOpen(path, "r")
            if !file
                return ""
            try {
                file.ReadLine()
                loop {
                    line := file.ReadLine()
                    if line = ""
                        return ""
                    if SubStr(line, 1, 1) = '"' && RegExMatch(SubStr(line, 2), "^\d+ \d+ \d+ \d+")
                        break
                }
                loop {
                    line := file.ReadLine()
                    if line = ""
                        return ""
                    if SubStr(line, 1, 1) != '"'
                        return ""
                    nonePos := InStr(line, " c None")
                    if nonePos
                        return SubStr(line, 2, nonePos - 2)
                }
            } finally {
                file.Close()
            }
        }

        static Be32At(buf, offset) {
            return (NumGet(buf, offset, "UChar") << 24) | (NumGet(buf, offset + 1, "UChar") << 16) | (NumGet(buf, offset + 2, "UChar") << 8) | NumGet(buf, offset + 3, "UChar")
        }

        static PsdHeaderInfo(path) {
            ; BEHAV-OPEN-004: the width and the image-descriptor data
            ; offset for the truncation count.
            file := FileOpen(path, "r")
            if !file
                return Map("Width", 0, "DataStart", 0)
            try {
                head := Buffer(26, 0)
                file.RawRead(head, Min(file.Length, 26))
                width := Pillow.Image.Be32At(head, 18)
                pos := 26
                block := Buffer(4, 0)
                file.Pos := pos
                file.RawRead(block, 4)
                colorSize := Pillow.Image.Be32At(block, 0)
                pos += 4 + colorSize
                file.Pos := pos
                file.RawRead(block, 4)
                resourceSize := Pillow.Image.Be32At(block, 0)
                pos += 4 + resourceSize
                file.Pos := pos
                file.RawRead(block, 4)
                layerSize := Pillow.Image.Be32At(block, 0)
                pos += 4 + layerSize
                return Map("Width", width, "DataStart", pos + 2)
            } finally {
                file.Close()
            }
        }

        static PsdIcc(path) {
            ; BEHAV-OPEN-004: Pillow exposes info["icc_profile"] from
            ; the image-resource block id 1039.
            file := FileOpen(path, "r")
            if !file
                return 0
            try {
                head := Buffer(26, 0)
                file.RawRead(head, Min(file.Length, 26))
                pos := 26
                block := Buffer(4, 0)
                file.Pos := pos
                file.RawRead(block, 4)
                pos += 4 + Pillow.Image.Be32At(block, 0)
                file.Pos := pos
                file.RawRead(block, 4)
                resourceSize := Pillow.Image.Be32At(block, 0)
                pos += 4
                end := pos + resourceSize
                while pos + 4 <= end {
                    file.Pos := pos + 4
                    small := Buffer(3, 0)
                    file.RawRead(small, 3)
                    id := (NumGet(small, 0, "UChar") << 8) | NumGet(small, 1, "UChar")
                    nameLen := NumGet(small, 2, "UChar")
                    pad := nameLen & 1 ? 0 : 1
                    lenField := Buffer(4, 0)
                    file.Pos := pos + 7 + nameLen + pad
                    file.RawRead(lenField, 4)
                    dataLen := Pillow.Image.Be32At(lenField, 0)
                    dataStart := pos + 11 + nameLen + pad
                    if id = 1039 {
                        icc := Buffer(dataLen, 0)
                        file.Pos := dataStart
                        file.RawRead(icc, dataLen)
                        return icc
                    }
                    pos := dataStart + dataLen + (dataLen & 1 ? 1 : 0)
                }
                return 0
            } finally {
                file.Close()
            }
        }

        static OpenMcIdas(path) {
            ; BEHAV-OPEN-003: McIdasImageFile — the 256-byte big-endian
            ; directory (magic 00..00 04), bytes-per-sample 1/2/4 as
            ; L/I;16B/I, offset w[34]+w[15], and rows of stride
            ; w[15]+w[10]*bpp*w[14] with only the first w*bpp bytes used.
            data := Pillow.Image.ReadAllBytes(path)
            if data.Size < 256
                throw Error("cannot identify image file <" path ">", -1)
            magicOk := true
            loop 7 {
                if NumGet(data, A_Index - 1, "UChar") != 0
                    magicOk := false
            }
            if !magicOk || NumGet(data, 7, "UChar") != 4
                throw Error("cannot identify image file <" path ">", -1)
            bpp := Pillow.Image.Be32At(data, 40)
            width := Pillow.Image.Be32At(data, 36)
            height := Pillow.Image.Be32At(data, 32)
            aux := Pillow.Image.Be32At(data, 52)
            prefixLen := Pillow.Image.Be32At(data, 56)
            offsetPrefix := Pillow.Image.Be32At(data, 132)
            modeName := ""
            rawMode := ""
            if bpp = 1 {
                modeName := "L"
                rawMode := "L"
            } else if bpp = 2 {
                modeName := "I;16B"
                rawMode := "I;16B"
            } else if bpp = 4 {
                modeName := "I"
                rawMode := "I;32B"
            } else {
                throw Error("cannot identify image file <" path ">", -1)
            }
            if width <= 0 || height <= 0
                throw Error("cannot identify image file <" path ">", -1)
            offset := offsetPrefix + prefixLen
            stride := prefixLen + width * bpp * aux
            if stride <= 0 || offset > data.Size
                throw Error("cannot identify image file <" path ">", -1)
            present := data.Size - offset
            needed := (height - 1) * stride + width * bpp
            if present < needed
                throw Error("image file is truncated (" Mod(present, stride) " bytes not processed)", -1)
            tight := Buffer(width * height * bpp, 0)
            rowBytes := width * bpp
            loop height {
                DllCall("msvcrt\memcpy", "Ptr", tight.Ptr + (A_Index - 1) * rowBytes, "Ptr", data.Ptr + offset + (A_Index - 1) * stride, "UPtr", rowBytes, "CDecl Ptr")
            }
            image := Pillow.Image.New(modeName, [width, height])
            try {
                image.FromBytes(tight, "raw", rawMode)
            } catch {
                image.Close()
                throw
            }
            return image
        }

        static OpenIptc(path) {
            ; BEHAV-OPEN-003: IptcImageFile — the 5-byte field headers
            ; (record 1-9/240, size byte with the 128-extended form),
            ; the (3,60)/(3,65)/(3,20)/(3,30)/(3,120) descriptive fields,
            ; and the (8,10) data fields decoded as a raw PGM (L) or a
            ; plain JPEG.
            data := Pillow.Image.ReadAllBytes(path)
            pos := 0
            layers := 0
            component := 0
            id := 0
            hasId := false
            width := 0
            height := 0
            compression := 0
            payloadStart := 0
            payloadEnd := 0
            sawData := false
            loop {
                if pos + 5 > data.Size
                    break
                allZero := true
                loop 5 {
                    if NumGet(data, pos + A_Index - 1, "UChar") != 0
                        allZero := false
                }
                if allZero
                    break
                if NumGet(data, pos, "UChar") != 0x1C
                    throw Error("cannot identify image file <" path ">", -1)
                record := NumGet(data, pos + 1, "UChar")
                tagNum := NumGet(data, pos + 2, "UChar")
                if !(record = 1 || record = 2 || record = 3 || record = 4 || record = 5 || record = 6 || record = 7 || record = 8 || record = 9 || record = 240)
                    throw Error("cannot identify image file <" path ">", -1)
                sizeByte := NumGet(data, pos + 3, "UChar")
                size := 0
                headerLen := 5
                if sizeByte > 132
                    throw Error("illegal field length in IPTC/NAA file", -1)
                else if sizeByte = 128 {
                    size := 0
                } else if sizeByte > 128 {
                    extBytes := sizeByte - 128
                    if pos + 5 + extBytes > data.Size
                        throw Error("cannot identify image file <" path ">", -1)
                    size := 0
                    loop extBytes
                        size := size * 256 + NumGet(data, pos + 4 + A_Index, "UChar")
                    headerLen := 5 + extBytes
                } else {
                    size := (NumGet(data, pos + 3, "UChar") << 8) | NumGet(data, pos + 4, "UChar")
                }
                fieldData := pos + headerLen
                if fieldData + size > data.Size
                    throw Error("cannot identify image file <" path ">", -1)
                if record = 8 && tagNum = 10 {
                    sawData := true
                    if payloadStart = 0
                        payloadStart := fieldData
                    payloadEnd := fieldData + size
                } else if sawData {
                    ; a non-(8,10) field after the data stops the copy
                    break
                } else {
                    if record = 3 && tagNum = 60 && size >= 2 {
                        layers := NumGet(data, fieldData, "UChar")
                        component := NumGet(data, fieldData + 1, "UChar")
                    }
                    if record = 3 && tagNum = 65 && size >= 1 {
                        hasId := true
                        id := Integer(NumGet(data, fieldData, "UChar")) - 1
                    }
                    if record = 3 && tagNum = 20 && size >= 2 {
                        width := (NumGet(data, fieldData, "UChar") << 8) | NumGet(data, fieldData + 1, "UChar")
                    }
                    if record = 3 && tagNum = 30 && size >= 2 {
                        height := (NumGet(data, fieldData, "UChar") << 8) | NumGet(data, fieldData + 1, "UChar")
                    }
                    if record = 3 && tagNum = 120 && size >= 2 {
                        compression := (NumGet(data, fieldData, "UChar") << 8) | NumGet(data, fieldData + 1, "UChar")
                    }
                }
                pos := fieldData + size
            }
            if !hasId
                id := 0
            if layers = 1 && !component {
                ; L mode
            } else if layers = 3 && component {
                ; Pillow indexes "RGB"[id] with Python semantics —
                ; negative ids wrap from the end and out-of-range ids
                ; raise IndexError (the identification error).
                if id < -3 || id > 2
                    throw Error("cannot identify image file <" path ">", -1)
                if id < 0
                    id := id + 3
                modeChar := SubStr("RGB", id + 1, 1)
                throw Error("No packer found from " modeChar " to " modeChar, -1)
            } else if layers = 4 && component {
                if id < -4 || id > 3
                    throw Error("cannot identify image file <" path ">", -1)
                if id < 0
                    id := id + 4
                modeChar := SubStr("CMYK", id + 1, 1)
                throw Error("No packer found from " modeChar " to " modeChar, -1)
            } else {
                throw Error("cannot identify image file <" path ">", -1)
            }
            if !sawData || payloadStart = 0 || payloadEnd <= payloadStart
                throw Error("cannot load this image", -1)
            if !(compression = 1 || compression = 5)
                throw Error("Unknown IPTC image compression", -1)
            payload := Buffer(payloadEnd - payloadStart, 0)
            DllCall("msvcrt\memcpy", "Ptr", payload, "Ptr", data.Ptr + payloadStart, "UPtr", payload.Size, "CDecl Ptr")
            if compression = 1 {
                if width <= 0 || height <= 0
                    throw Error("cannot load this image", -1)
                if payload.Size < width * height
                    throw Error("image file is truncated (" Mod(payload.Size, width) " bytes not processed)", -1)
                image := Pillow.Image.New("L", [width, height])
                try {
                    image.FromBytes(payload)
                } catch {
                    image.Close()
                    throw
                }
                return image
            }
            ; JPEG payload: decode via the native JPEG route (Pillow
            ; re-opens the extracted bytes as a JPEG).
            tempPath := A_Temp "\pillow-ahk-iptc-" A_TickCount "-" Random(1, 1000000) ".jpg"
            try {
                file := FileOpen(tempPath, "w")
                try
                    file.RawWrite(payload, payload.Size)
                finally
                    file.Close()
                image := Pillow.Image.Open(tempPath)
                try {
                    FileDelete(tempPath)
                } catch {
                }
                image.Format := "IPTC"
                return image
            } catch {
                try {
                    FileDelete(tempPath)
                } catch {
                }
                throw Error("cannot identify image file <" path ">", -1)
            }
        }

        static StubAccepts(path, format) {
            ; BEHAV-OPEN-001: HDF5/BUFR/GRIB stub plugins accept only the
            ; exact magic (HDF5 8 bytes; BUFR "BUFR"/"ZCZC"; GRIB "GRIB"
            ; with byte 7 == 1).
            file := FileOpen(path, "r")
            if !file
                return false
            try {
                head := Buffer(8, 0)
                file.RawRead(head, Min(file.Length, 8))
                if format = "HDF5"
                    return NumGet(head, 0, "UInt64") = 0x0A1A0A0D46444889
                if format = "BUFR"
                    return NumGet(head, 0, "UInt") = 0x52465542 || NumGet(head, 0, "UInt") = 0x435A435A
                if format = "GRIB"
                    return NumGet(head, 0, "UInt") = 0x42495247 && NumGet(head, 7, "UChar") = 1
                return false
            } finally {
                file.Close()
            }
        }

        static IcnsSizes(path) {
            ; BEHAV-ICNS-001: Pillow's info["sizes"] is the (w, h, scale)
            ; list for every present icon slot in the plugin's SIZES order.
            sizes := Buffer(96, 0)
            count := 0
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_icns_sizes",
                "Ptr", Pillow.Image.Utf8Buffer(String(path)),
                "Ptr", sizes,
                "Int", 32,
                "Int*", &count,
                "Int"
            )
            if status != 0 || count <= 0
                return []
            out := []
            index := 0
            loop count / 3 {
                out.Push([NumGet(sizes, index * 4, "Int"), NumGet(sizes, (index + 1) * 4, "Int"), NumGet(sizes, (index + 2) * 4, "Int")])
                index += 3
            }
            return out
        }

        static IcnsRleLeftover(path) {
            ; BEHAV-ICNS-001: Pillow's read_32 reports the remaining pixel
            ; count of the first short RLE band as "Error reading channel
            ; [N left]". Re-derive N by replaying the best slot's RGB32
            ; chunk decode (uncompressed payloads never hit this error).
            file := FileOpen(path, "r")
            if !file
                return 0
            try {
                size := file.Length
                header := Buffer(8, 0)
                file.RawRead(header, 8)
                if size < 8 || StrGet(header, 4, "UTF-8") != "icns"
                    return 0
                fileLength := (NumGet(header, 4, "UChar") << 24) | (NumGet(header, 5, "UChar") << 16) | (NumGet(header, 6, "UChar") << 8) | NumGet(header, 7, "UChar")
                slots := [
                    [512, 512, 2, ["ic10"], ""],
                    [512, 512, 1, ["ic09"], ""],
                    [256, 256, 2, ["ic14"], ""],
                    [256, 256, 1, ["ic08"], ""],
                    [128, 128, 2, ["ic13"], ""],
                    [128, 128, 1, ["ic07", "it32", "t8mk"], "it32"],
                    [64, 64, 1, ["icp6"], ""],
                    [32, 32, 2, ["ic12"], ""],
                    [48, 48, 1, ["ih32", "h8mk"], "ih32"],
                    [32, 32, 1, ["icp5", "il32", "l8mk"], "il32"],
                    [16, 16, 2, ["ic11"], ""],
                    [16, 16, 1, ["icp4", "is32", "s8mk"], "is32"],
                ]
                chunks := Map()
                offset := 8
                while offset < fileLength {
                    chunkHeader := Buffer(8, 0)
                    file.RawRead(chunkHeader, 8)
                    if file.Pos < offset + 8
                        break
                    blockSize := (NumGet(chunkHeader, 4, "UChar") << 24) | (NumGet(chunkHeader, 5, "UChar") << 16) | (NumGet(chunkHeader, 6, "UChar") << 8) | NumGet(chunkHeader, 7, "UChar")
                    if blockSize < 8
                        break
                    chunks[StrGet(chunkHeader, 4, "UTF-8")] := offset
                    offset += 8 + blockSize - 8
                    file.Seek(offset)
                }
                best := unset
                for s in slots {
                    present := false
                    for key in s[4]
                        present := present || chunks.Has(key)
                    if !present
                        continue
                    if !IsSet(best) || s[1] > best[1] || (s[1] = best[1] && s[2] > best[2]) || (s[1] = best[1] && s[2] = best[2] && s[3] > best[3])
                        best := s
                }
                if !IsSet(best)
                    return 0
                rgbKey := best[5]
                if rgbKey = "" || !chunks.Has(rgbKey)
                    return 0
                file.Seek(chunks[rgbKey])
                chunkHeader := Buffer(8, 0)
                file.RawRead(chunkHeader, 8)
                payloadSize := ((NumGet(chunkHeader, 4, "UChar") << 24) | (NumGet(chunkHeader, 5, "UChar") << 16) | (NumGet(chunkHeader, 6, "UChar") << 8) | NumGet(chunkHeader, 7, "UChar")) - 8
                if rgbKey = "it32" {
                    skip := Buffer(4, 0)
                    file.RawRead(skip, 4)
                    payloadSize -= 4
                }
                sizesq := best[1] * best[2]
                if payloadSize = sizesq * 3
                    return 0
                data := Buffer(payloadSize, 0)
                file.RawRead(data, payloadSize)
                pos := 0
                loop 3 {
                    bytesLeft := sizesq
                    while bytesLeft > 0 {
                        if pos >= payloadSize
                            break
                        byteValue := NumGet(data, pos, "UChar")
                        pos += 1
                        if byteValue & 0x80 {
                            blockSize := byteValue - 125
                            if pos < payloadSize
                                pos += 1
                            bytesLeft -= blockSize
                        } else {
                            blockSize := byteValue + 1
                            pos += Min(blockSize, payloadSize - pos)
                            bytesLeft -= blockSize
                        }
                        if bytesLeft <= 0
                            break
                    }
                    if bytesLeft != 0
                        return bytesLeft
                }
                return 0
            } finally {
                file.Close()
            }
        }

        static EpsOpenFailure(path) {
            ; BEHAV-EPS-001: replay Pillow's EpsImageFile._open header
            ; scan. Returns "" for a valid header (the caller raises the
            ; Ghostscript error) or Pillow's exact open-time message.
            file := FileOpen(path, "r")
            if !file
                return "cannot identify image file <" path ">"
            try {
                if file.Length < 4
                    return "cannot identify image file <" path ">"
                head := Buffer(4, 0)
                file.RawRead(head, 4)
                if StrGet(head, 4, "UTF-8") = "%!PS" {
                    file.Seek(0)
                } else if NumGet(head, 0, "UInt") = 0xC6D3D0C5 {
                    ; binary preview: skip to the %!PS offset (i32le)
                    ext := Buffer(8, 0)
                    file.RawRead(ext, 8)
                    file.Seek(NumGet(ext, 0, "Int"))
                } else {
                    return "cannot identify image file <" path ">"
                }

                info := Map()
                boundingBox := unset
                imageDataSize := unset
                readingHeader := true
                readingTrailer := false
                trailerReached := false
                stopLoop := false
                while !stopLoop && !file.AtEOF {
                    line := file.ReadLine()
                    if line = "" && file.AtEOF
                        break
                    if StrLen(line) > 255 && SubStr(line, 1, 1) = "%"
                        return "cannot identify image file <" path ">"

                    if readingHeader {
                        if SubStr(line, 1, 1) != "%" || SubStr(line, 1, 13) = "%%EndComments" {
                            if !info.Has("PS-Adobe")
                                return 'EPS header missing "%!PS-Adobe" comment'
                            if !info.Has("BoundingBox")
                                return 'EPS header missing "%%BoundingBox" comment'
                            readingHeader := false
                            continue
                        }
                        handled := false
                        ; split: ^%%([^:]*):[ \t]*(.*)[ \t]*$
                        if SubStr(line, 1, 2) = "%%" && InStr(line, ":") {
                            colonPos := InStr(line, ":")
                            key := SubStr(line, 3, colonPos - 3)
                            value := Trim(SubStr(line, colonPos + 1), " `t")
                            info[key] := value
                            handled := true
                            if key = "BoundingBox" {
                                if value = "(atend)" {
                                    readingTrailer := true
                                } else if !IsSet(boundingBox) || (trailerReached && readingTrailer) {
                                    parts := StrSplit(value, " ")
                                    parsed := []
                                    valid := true
                                    for part in parts {
                                        try
                                            parsed.Push(Integer(Float(part)))
                                        catch {
                                            valid := false
                                            break
                                        }
                                    }
                                    if valid
                                        boundingBox := parsed
                                }
                            }
                        } else if SubStr(line, 1, 1) = "%" {
                            ; field: ^%[%!\w]([^:]*)[ \t]*$
                            second := SubStr(line, 2, 1)
                            if second = "%" || second = "!" || RegExMatch(second, "\w") {
                                key := Trim(SubStr(line, 3), " `t")
                                if SubStr(key, 1, 8) = "PS-Adobe"
                                    info["PS-Adobe"] := SubStr(key, 10)
                                else
                                    info[key] := ""
                                handled := true
                            }
                        }
                        if !handled {
                            if SubStr(line, 1, 1) = "%"
                                continue
                            return "bad EPS header"
                        }
                        continue
                    }

                    if SubStr(line, 1, 11) = "%ImageData:" {
                        if IsSet(imageDataSize)
                            continue
                        fields := StrSplit(Trim(SubStr(line, 12)), " ")
                        if fields.Length >= 4 {
                            try {
                                columns := Integer(fields[1])
                                rows := Integer(fields[2])
                                bitDepth := Integer(fields[3])
                                modeId := Integer(fields[4])
                                if bitDepth = 1 {
                                    imageDataSize := [columns, rows]
                                } else if bitDepth = 8 {
                                    if modeId = 1 || modeId = 2 || modeId = 3 || modeId = 4
                                        imageDataSize := [columns, rows]
                                    else
                                        stopLoop := true
                                } else {
                                    stopLoop := true
                                }
                            }
                        }
                        continue
                    }
                    if SubStr(line, 1, 5) = "%%EOF"
                        break
                    if trailerReached && readingTrailer {
                        ; last BoundingBox wins in the trailer
                        if SubStr(line, 1, 2) = "%%" && InStr(line, ":") {
                            colonPos := InStr(line, ":")
                            key := SubStr(line, 3, colonPos - 3)
                            value := Trim(SubStr(line, colonPos + 1), " `t")
                            info[key] := value
                            if key = "BoundingBox" && value != "(atend)" {
                                parts := StrSplit(value, " ")
                                parsed := []
                                valid := true
                                for part in parts {
                                    try
                                        parsed.Push(Integer(Float(part)))
                                    catch {
                                        valid := false
                                        break
                                    }
                                }
                                if valid
                                    boundingBox := parsed
                            }
                        }
                        continue
                    }
                    if SubStr(line, 1, 9) = "%%Trailer"
                        trailerReached := true
                }
                if !IsSet(boundingBox)
                    return "cannot determine EPS bounding box"
                return ""
            } finally {
                file.Close()
            }
        }

        static ReadAllBytes(path) {
            file := FileOpen(path, "r")
            if !file
                throw Error("Pillow.Image.ReadAllBytes failed to open " path, -1)
            try {
                out := Buffer(file.Length, 0)
                file.RawRead(out, out.Size)
                return out
            } finally {
                file.Close()
            }
        }

        static MpoHasIndex(path) {
            ; BEHAV-MPO-001: Pillow's JPEG factory reports format MPO
            ; only when the APP2 "MPF\0" index marker is present; a
            ; plain JPEG saved with an .mpo extension reports JPEG.
            file := FileOpen(path, "r")
            if !file
                return false
            try {
                if file.Length < 32
                    return false
                data := Buffer(Min(file.Length, 65536), 0)
                file.RawRead(data, data.Size)
                loop data.Size - 3 {
                    if NumGet(data, A_Index - 1, "UChar") = 0x4D
                        && NumGet(data, A_Index, "UChar") = 0x50
                        && NumGet(data, A_Index + 1, "UChar") = 0x46
                        && NumGet(data, A_Index + 2, "UChar") = 0
                        return true
                }
                return false
            } finally {
                file.Close()
            }
        }

        static OpenDibHandle(path, &outHandle) {
            ; BEHAV-DIB-001: DIB open = synthetic BITMAPFILEHEADER + native
            ; BMP decoder. Pillow's DIB has no BITMAPFILEHEADER, so the
            ; palette size (and the pixel offset) derives from biBitCount
            ; and biClrUsed in the BITMAPINFOHEADER at offset 0.
            source := FileOpen(path, "r")
            if !source
                throw Error("Pillow.Image.Open DIB failed to open the source file", -1)
            size := source.Length
            if size < 40 {
                source.Close()
                outHandle := 0
                return -3
            }
            payload := Buffer(size, 0)
            source.RawRead(payload, size)
            source.Close()
            bitCount := NumGet(payload, 14, "UShort")
            clrUsed := NumGet(payload, 32, "UInt")
            paletteEntries := clrUsed
            if paletteEntries = 0 && bitCount <= 8
                paletteEntries := 1 << bitCount
            bfh := Buffer(14, 0)
            NumPut("UShort", 0x4D42, bfh, 0)
            NumPut("UInt", size + 14, bfh, 2)
            NumPut("UInt", 54 + paletteEntries * 4, bfh, 10)
            tempPath := A_Temp "\pillow_c_dib_open_" Random(1, 2147483647) ".bmp"
            try {
                temp := FileOpen(tempPath, "w")
                if !temp
                    throw Error("Pillow.Image.Open DIB failed to create the temporary BMP", -1)
                temp.RawWrite(bfh.Ptr, 14)
                temp.RawWrite(payload.Ptr, size)
                temp.Close()
                tempBytes := Pillow.Image.Utf8Buffer(tempPath)
                return DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_open_bmp",
                    "Ptr", tempBytes,
                    "Ptr*", &outHandle,
                    "Int"
                )
            } finally {
                FileDelete(tempPath)
            }
        }

        static FrameCountForOpen(pathBytes, format) {
            if format = "DCX"
                return Pillow.Image.DcxFrameCount(StrGet(pathBytes, "UTF-8"))
            if format = "FLI"
                return Pillow.Image.FliFrameCount(StrGet(pathBytes, "UTF-8"))
            if !(format = "TIFF" || format = "GIF")
                return 1
            count := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_frame_count_" StrLower(format),
                "Ptr", pathBytes,
                "Int*", &count,
                "Int"
            ))
            return count > 0 ? count : 1
        }

        static FormatFromPath(path) {
            if RegExMatch(path, "i)\.bmp$")
                return "BMP"
            if RegExMatch(path, "i)\.dib$")
                return "DIB"
            if RegExMatch(path, "i)\.im$")
                return "IM"
            if RegExMatch(path, "i)\.msp$")
                return "MSP"
            if RegExMatch(path, "i)\.palm$")
                return "PALM"
            if RegExMatch(path, "i)\.blp$")
                return "BLP"
            if RegExMatch(path, "i)\.spider$")
                return "SPIDER"
            if RegExMatch(path, "i)\.pcx$")
                return "PCX"
            if RegExMatch(path, "i)\.(sgi|bw|rgb|rgba)$")
                return "SGI"
            if RegExMatch(path, "i)\.dds$")
                return "DDS"
            if RegExMatch(path, "i)\.icns$")
                return "ICNS"
            if RegExMatch(path, "i)\.(eps|ps)$")
                return "EPS"
            if RegExMatch(path, "i)\.mpo$")
                return "MPO"
            if RegExMatch(path, "i)\.pdf$")
                return "PDF"
            if RegExMatch(path, "i)\.dcx$")
                return "DCX"
            if RegExMatch(path, "i)\.pxr$")
                return "PIXAR"
            if RegExMatch(path, "i)\.(h5|hdf)$")
                return "HDF5"
            if RegExMatch(path, "i)\.bufr$")
                return "BUFR"
            if RegExMatch(path, "i)\.grib$")
                return "GRIB"
            if RegExMatch(path, "i)\.(ftc|ftu)$")
                return "FTEX"
            if RegExMatch(path, "i)\.ras$")
                return "SUN"
            if RegExMatch(path, "i)\.gbr$")
                return "GBR"
            if RegExMatch(path, "i)\.fits?$")
                return "FITS"
            if RegExMatch(path, "i)\.xpm$")
                return "XPM"
            if RegExMatch(path, "i)\.iim$")
                return "IPTC"
            if RegExMatch(path, "i)\.psd$")
                return "PSD"
            if RegExMatch(path, "i)\.(fli|flc)$")
                return "FLI"
            if RegExMatch(path, "i)\.mic$")
                return "MIC"
            if RegExMatch(path, "i)\.pcd$")
                return "PCD"
            if RegExMatch(path, "i)\.(mpg|mpeg)$")
                return "MPEG"
            if RegExMatch(path, "i)\.(wmf|emf)$")
                return "WMF"
            if RegExMatch(path, "i)\.(pbm|pgm|ppm|pnm)$")
                return "PPM"
            if RegExMatch(path, "i)\.qoi$")
                return "QOI"
            if RegExMatch(path, "i)\.tga$")
                return "TGA"
            if RegExMatch(path, "i)\.xbm$")
                return "XBM"
            if RegExMatch(path, "i)\.ico$")
                return "ICO"
            if RegExMatch(path, "i)\.cur$")
                return "CUR"
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
            if name = "BMP" || name = "DIB" || name = "IM" || name = "MSP" || name = "PALM" || name = "BLP" || name = "SPIDER" || name = "PCX" || name = "SGI" || name = "DDS" || name = "ICNS" || name = "EPS" || name = "MPO" || name = "PDF" || name = "DCX" || name = "PIXAR" || name = "XVTHUMB" || name = "IMT" || name = "HDF5" || name = "BUFR" || name = "GRIB" || name = "FTEX" || name = "SUN" || name = "GBR" || name = "FITS" || name = "XPM" || name = "IPTC" || name = "MCIDAS" || name = "PSD" || name = "FLI" || name = "MIC" || name = "PCD" || name = "MPEG" || name = "WMF" || name = "PNG" || name = "JPEG" || name = "TIFF" || name = "GIF" || name = "PPM" || name = "QOI" || name = "TGA" || name = "XBM" || name = "ICO" || name = "CUR"
                return name
            throw Error("Pillow image file format is unsupported", -1)
        }

        static FormatDescription(format) {
            descriptions := Map(
                "BMP", "Windows Bitmap",
                "CUR", "Windows Cursor",
                "DIB", "DIB",
                "IM", "IM",
                "MSP", "Windows Paint",
                "PALM", "Palm pixmap",
                "BLP", "Blizzard Mipmap Format",
                "SPIDER", "The Spider image format",
                "PCX", "PCX",
                "SGI", "SGI Image File Format",
                "DDS", "DirectDraw Surface",
                "ICNS", "Mac OS icns resource",
                "EPS", "Encapsulated Postscript",
                "MPO", "MPO (CIPA DC-007)",
                "DCX", "Intel DCX",
                "PIXAR", "PIXAR raster image",
                "XVTHUMB", "XV thumbnail image",
                "HDF5", "HDF5",
                "BUFR", "BUFR",
                "GRIB", "GRIB",
                "FTEX", "Texture File Format (IW2:EOC)",
                "SUN", "Sun Raster File",
                "GBR", "GIMP brush file",
                "FITS", "FITS",
                "XPM", "X11 Pixel Map",
                "IPTC", "IPTC/NAA",
                "MCIDAS", "McIdas area file",
                "PSD", "Adobe Photoshop",
                "FLI", "Autodesk FLI/FLC Animation",
                "MIC", "Microsoft Image Composer",
                "PCD", "Kodak PhotoCD",
                "MPEG", "MPEG",
                "WMF", "Windows Metafile",
                "GIF", "Compuserve GIF",
                "ICO", "Windows Icon",
                "JPEG", "JPEG (ISO 10918)",
                "PNG", "Portable network graphics",
                "PPM", "Pbmplus image",
                "QOI", "Quite OK Image",
                "TGA", "Targa",
                "TIFF", "Adobe TIFF",
                "XBM", "X11 Bitmap"
            )
            normalized := StrUpper(format)
            if normalized = "JPG"
                normalized := "JPEG"
            if normalized = "TIF"
                normalized := "TIFF"
            return descriptions.Has(normalized) ? descriptions[normalized] : ""
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

        static SaveOption(options, names*) {
            if !IsObject(options)
                return { Set: false }
            for name in names {
                if options is Map {
                    if options.Has(name)
                        return { Set: true, Value: options[name] }
                } else if options.HasOwnProp(name) {
                    return { Set: true, Value: options.%name% }
                }
            }
            return { Set: false }
        }

        static SaveOptionBool(options, defaultValue, names*) {
            option := Pillow.Image.SaveOption(options, names*)
            return option.Set ? !!option.Value : defaultValue
        }

        static SaveIntSequence(value, imageCount, optionName) {
            values := []
            if IsObject(value) {
                for item in value {
                    if !(item is Integer)
                        throw Error("Pillow.Image.Save " optionName " values must be integers", -1)
                    values.Push(item)
                }
            } else {
                if !(value is Integer)
                    throw Error("Pillow.Image.Save " optionName " must be an integer or integer array", -1)
                values.Push(value)
            }
            if !(values.Length = 1 || values.Length = imageCount)
                throw Error("Pillow.Image.Save " optionName " length must be 1 or match frame count", -1)
            return { Buffer: Pillow.Image.IntBuffer(values, "Pillow.Image.Save " optionName), Count: values.Length }
        }

        static SaveDpiPair(value, requirePositive := true) {
            if !IsObject(value)
                throw Error("Pillow.Image.Save dpi expects [x, y]", -1)
            values := []
            for item in value {
                if !(item is Number)
                    throw Error("Pillow.Image.Save dpi values must be numeric", -1)
                values.Push(item + 0.0)
            }
            if values.Length != 2
                throw Error("Pillow.Image.Save dpi expects [x, y]", -1)
            if requirePositive && (values[1] <= 0 || values[2] <= 0)
                throw Error("Pillow.Image.Save dpi values must be greater than 0", -1)
            return values
        }

        static SaveTiffCompression(value) {
            if value is Integer {
                if value = 1 || value = 32773
                    return value
                throw Error("Pillow.Image.Save TIFF compression is not supported", -1)
            }
            text := StrLower(String(value))
            if text = "raw" || text = "none"
                return 1
            if text = "tiff_lzw" || text = "lzw"
                return 5
            if text = "tiff_adobe_deflate" || text = "tiff_deflate"
                return 8
            if text = "packbits"
                return 32773
            if text = "jpeg" || text = "tiff_jpeg" || text = "group3" || text = "group4" || text = "tiff_ccitt"
                throw Error("Pillow.Image.Save TIFF compression is not supported", -1)
            ; any other unknown compression string is ignored by Pillow and
            ; the image falls back to the raw writer
            return 1
        }

        static SaveTiffAsciiNamedValue(value) {
            ; Pillow 11.3.0 TiffImagePlugin.write_string: a sequence
            ; truncates to its first entry first, bytes pass through with a
            ; trailing NUL, int becomes str(value), str is encoded
            ; ascii-with-replace, and a float hits the exact AttributeError.
            if IsObject(value) && Type(value) != "Buffer" {
                if value.Length = 0
                    throw Error("Pillow.Image.Save tiff named tag sequence must not be empty", -1)
                value := value[1]
            }
            if value is Buffer {
                out := Buffer(value.Size + 1, 0)
                offset := 0
                while offset < value.Size {
                    NumPut("UChar", NumGet(value, offset, "UChar"), out, offset)
                    offset += 1
                }
                return out
            }
            if value is String
                return Pillow.Image.Utf8Buffer(RegExReplace(value, "[^\x00-\x7F]", "?"))
            if value is Integer
                return Pillow.Image.Utf8Buffer(String(value))
            throw Error("'float' object has no attribute 'encode'", -1)
        }

        static SaveTiffInfoEntries(tiffInfo) {
            ; Pillow 11.3.0's tiffinfo arbitrary tags follow
            ; ImageFileDirectory_v2.__setitem__: registered tags keep their
            ; type (282/283 RATIONAL via the exact float conversion, the
            ; named ASCII tags via write_string, 296 SHORT), unknown tags
            ; infer SHORT/LONG/SIGNED_SHORT/SIGNED_LONG/DOUBLE/ASCII/BYTE by
            ; value shape, sequences reuse the same rules, the empty
            ; sequence writes a RATIONAL count-0 entry, and mixed sequences
            ; hit the exact write_string/write_undefined TypeErrors.
            records := []
            valuesSize := 0
            for tag, value in tiffInfo {
                if !(tag is Integer)
                    throw Error("Pillow.Image.Save tiffinfo keys must be integers", -1)
                if tag < 0 || tag > 65535
                    throw Error("ushort format requires 0 <= number <= 0xffff", -1)
                recordType := 0
                recordCount := 1
                recordBytes := 0
                if tag = 282 || tag = 283 {
                    first := IsObject(value) ? value[1] : value
                    num := 0
                    den := 1
                    if first is Integer {
                        if first < 0
                            throw Error("argument out of range", -1)
                        num := first
                    } else if first is Float {
                        if first < 0
                            throw Error("argument out of range", -1)
                        numBuf := Buffer(8, 0)
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_tiff_rational_from_double",
                            "Double", first + 0.0,
                            "Ptr", numBuf,
                            "Ptr", numBuf.Ptr + 4,
                            "Int"
                        ))
                        num := NumGet(numBuf, 0, "UInt")
                        den := NumGet(numBuf, 4, "UInt")
                    } else {
                        throw Error("bad operand type for abs(): 'str'", -1)
                    }
                    recordType := 5
                    recordBytes := Buffer(8, 0)
                    NumPut("UInt", num, recordBytes, 0)
                    NumPut("UInt", den, recordBytes, 4)
                } else if tag = 270 || tag = 305 || tag = 306 || tag = 315 || tag = 33432 {
                    recordType := 2
                    recordBytes := Pillow.Image.SaveTiffAsciiNamedValue(value)
                    recordCount := recordBytes.Size
                } else if tag = 296 {
                    if !(value is Integer)
                        throw Error("required argument is not an integer", -1)
                    if value < 0 || value > 65535
                        throw Error("ushort format requires 0 <= number <= 0xffff", -1)
                    recordType := 3
                    recordBytes := Buffer(2, 0)
                    NumPut("UShort", value, recordBytes, 0)
                } else {
                    seq := IsObject(value) && Type(value) != "Buffer" ? value : 0
                    if IsObject(seq) {
                        if seq.Length = 0 {
                            ; Pillow writes the empty sequence as RATIONAL count 0
                            recordType := 5
                            recordCount := 0
                            recordBytes := Buffer(0)
                        } else {
                            allInt := true
                            allFloat := true
                            allStr := true
                            allBytes := true
                            for item in seq {
                                if !(item is Integer)
                                    allInt := false
                                if !(item is Float)
                                    allFloat := false
                                if !(item is String)
                                    allStr := false
                                if Type(item) != "Buffer"
                                    allBytes := false
                            }
                            if allStr
                                throw Error("ImageFileDirectory_v2.write_string() takes 2 positional arguments but " seq.Length + 1 " were given", -1)
                            if allBytes {
                                ; Pillow truncates a bytes sequence to its first value
                                value := seq[1]
                                seq := 0
                            } else if !allInt && !allFloat {
                                throw Error("ImageFileDirectory_v2.write_undefined() takes 2 positional arguments but " seq.Length + 1 " were given", -1)
                            }
                        }
                    }
                    if recordType = 0 && IsObject(seq) {
                        if allFloat && !allInt {
                            ; float sequence -> DOUBLE
                            recordType := 12
                            recordCount := seq.Length
                            recordBytes := Buffer(seq.Length * 8, 0)
                            idx := 0
                            for item in seq {
                                NumPut("Double", item + 0.0, recordBytes, idx * 8)
                                idx += 1
                            }
                        } else {
                            ; homogeneous numeric sequence
                            shortOk := true
                            signedShortOk := true
                            longOk := true
                            for item in seq {
                                if !(item is Integer) || item < -2147483648 || item > 4294967295
                                    throw Error("argument out of range", -1)
                                if item < 0 || item >= 65536
                                    shortOk := false
                                if item <= -32768 || item >= 32768
                                    signedShortOk := false
                                if item < 0
                                    longOk := false
                            }
                            if shortOk
                                recordType := 3
                            else if signedShortOk
                                recordType := 8
                            else if longOk
                                recordType := 4
                            else
                                recordType := 9
                            recordCount := seq.Length
                            recordBytes := Buffer(seq.Length * (recordType = 3 || recordType = 8 ? 2 : 4), 0)
                            idx := 0
                            for item in seq {
                                if recordType = 3
                                    NumPut("UShort", item, recordBytes, idx * 2)
                                else if recordType = 8
                                    NumPut("Short", item, recordBytes, idx * 2)
                                else if recordType = 4
                                    NumPut("UInt", item, recordBytes, idx * 4)
                                else
                                    NumPut("Int", item, recordBytes, idx * 4)
                                idx += 1
                            }
                        }
                    }
                    if recordType = 0 {
                        ; scalar
                        if value is Integer {
                            shortOk := true
                            signedShortOk := true
                            longOk := true
                            if value < -2147483648 || value > 4294967295
                                throw Error("argument out of range", -1)
                            if value < 0 || value >= 65536
                                shortOk := false
                            if value <= -32768 || value >= 32768
                                signedShortOk := false
                            if value < 0
                                longOk := false
                            if shortOk {
                                recordType := 3
                                recordBytes := Buffer(2, 0)
                                NumPut("UShort", value, recordBytes, 0)
                            } else if signedShortOk {
                                recordType := 8
                                recordBytes := Buffer(2, 0)
                                NumPut("Short", value, recordBytes, 0)
                            } else {
                                recordType := longOk ? 4 : 9
                                recordBytes := Buffer(4, 0)
                                if longOk
                                    NumPut("UInt", value, recordBytes, 0)
                                else
                                    NumPut("Int", value, recordBytes, 0)
                            }
                        } else if value is Float {
                            recordType := 12
                            recordBytes := Buffer(8, 0)
                            NumPut("Double", value + 0.0, recordBytes, 0)
                        } else if value is String {
                            recordType := 2
                            recordBytes := Pillow.Image.Utf8Buffer(RegExReplace(value, "[^\x00-\x7F]", "?"))
                            recordCount := recordBytes.Size
                        } else if value is Buffer {
                            recordType := 1
                            recordCount := value.Size
                            recordBytes := Buffer(value.Size, 0)
                            offset := 0
                            while offset < value.Size {
                                NumPut("UChar", NumGet(value, offset, "UChar"), recordBytes, offset)
                                offset += 1
                            }
                        } else {
                            throw Error("Pillow.Image.Save tiffinfo value type is not supported", -1)
                        }
                    }
                }
                records.Push({ Tag: tag, Type: recordType, Count: recordCount, Bytes: recordBytes })
                valuesSize += recordBytes.Size
            }
            tagsBuf := Buffer(records.Length * 4, 0)
            typesBuf := Buffer(records.Length * 4, 0)
            countsBuf := Buffer(records.Length * 4, 0)
            offsetsBuf := Buffer(records.Length * A_PtrSize, 0)
            valuesBuf := Buffer(valuesSize, 0)
            cursor := 0
            idx := 0
            for record in records {
                NumPut("Int", record.Tag, tagsBuf, idx * 4)
                NumPut("Int", record.Type, typesBuf, idx * 4)
                NumPut("UInt", record.Count, countsBuf, idx * 4)
                NumPut("UPtr", cursor, offsetsBuf, idx * A_PtrSize)
                offset := 0
                while offset < record.Bytes.Size {
                    NumPut("UChar", NumGet(record.Bytes, offset, "UChar"), valuesBuf, cursor + offset)
                    offset += 1
                }
                cursor += record.Bytes.Size
                idx += 1
            }
            return {
                Tags: tagsBuf,
                Types: typesBuf,
                Counts: countsBuf,
                Offsets: offsetsBuf,
                Values: valuesBuf,
                ValuesSize: valuesSize,
                EntryCount: records.Length,
            }
        }

        static SaveJpegSubsampling(value) {
            if value is Integer {
                ; Pillow passes any integer to the encoder: values below 0
                ; keep the default, 3 selects 4:1:1 (2x1), and 4+ falls to
                ; the all-1x1 (4:4:4) default of the C switch.
                if value < 0
                    return -1
                if value = 3
                    return 1
                if value >= 4
                    return 0
                return value
            }
            text := StrLower(String(value))
            if text = "4:4:4" || text = "web_high" || text = "web_very_high" || text = "web_maximum" || text = "high" || text = "maximum"
                return 0
            if text = "4:2:2"
                return 1
            if text = "4:2:0" || text = "4:1:1" || text = "web_low" || text = "web_medium" || text = "low" || text = "medium"
                return 2
            if text = "keep"
                return -2
            ; any other non-integer reaches the C int parse in Pillow
            throw Error("'str' object cannot be interpreted as an integer", -1)
        }

        static SaveJpegQTables(value) {
            if !IsObject(value) || Type(value) = "Buffer"
                throw Error("Pillow.Image.Save qtables expects one or two 64-entry integer arrays", -1)
            tables := []
            for table in value {
                if !IsObject(table) || Type(table) = "Buffer"
                    throw Error("Pillow.Image.Save qtables expects one or two 64-entry integer arrays", -1)
                entries := []
                for item in table {
                    if !(item is Integer) || item < 1 || item > 255
                        throw Error("Pillow.Image.Save qtables values must be integers in range 1..255", -1)
                    entries.Push(item)
                }
                if entries.Length != 64
                    throw Error("Pillow.Image.Save qtables entries must contain exactly 64 values", -1)
                tables.Push(entries)
            }
            if !(tables.Length = 1 || tables.Length = 2)
                throw Error("Pillow.Image.Save qtables expects one or two tables", -1)

            buf := Buffer(tables.Length * 64 * 4, 0)
            for tableIndex, table in tables {
                for valueIndex, item in table
                    NumPut("Int", item, buf, ((tableIndex - 1) * 64 + valueIndex - 1) * 4)
            }
            return { Buffer: buf, Count: tables.Length }
        }

        static SaveJpegQualityPreset(value) {
            text := StrLower(String(value))
            if text = "web_low"
                return Pillow.Image.SaveJpegQualityPresetInfo(2,
                    [20, 16, 25, 39, 50, 46, 62, 68, 16, 18, 23, 38, 38, 53, 65, 68, 25, 23, 31, 38, 53, 65, 68, 68, 39, 38, 38, 53, 65, 68, 68, 68, 50, 38, 53, 65, 68, 68, 68, 68, 46, 53, 65, 68, 68, 68, 68, 68, 62, 65, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68],
                    [21, 25, 32, 38, 54, 68, 68, 68, 25, 28, 24, 38, 54, 68, 68, 68, 32, 24, 32, 43, 66, 68, 68, 68, 38, 38, 43, 53, 68, 68, 68, 68, 54, 54, 66, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68],
                )
            if text = "web_medium"
                return Pillow.Image.SaveJpegQualityPresetInfo(2,
                    [16, 11, 11, 16, 23, 27, 31, 30, 11, 12, 12, 15, 20, 23, 23, 30, 11, 12, 13, 16, 23, 26, 35, 47, 16, 15, 16, 23, 26, 37, 47, 64, 23, 20, 23, 26, 39, 51, 64, 64, 27, 23, 26, 37, 51, 64, 64, 64, 31, 23, 35, 47, 64, 64, 64, 64, 30, 30, 47, 64, 64, 64, 64, 64],
                    [17, 15, 17, 21, 20, 26, 38, 48, 15, 19, 18, 17, 20, 26, 35, 43, 17, 18, 20, 22, 26, 30, 46, 53, 21, 17, 22, 28, 30, 39, 53, 64, 20, 20, 26, 30, 39, 48, 64, 64, 26, 26, 30, 39, 48, 63, 64, 64, 38, 35, 46, 53, 64, 64, 64, 64, 48, 43, 53, 64, 64, 64, 64, 64],
                )
            if text = "web_high"
                return Pillow.Image.SaveJpegQualityPresetInfo(0,
                    [6, 4, 4, 6, 9, 11, 12, 16, 4, 5, 5, 6, 8, 10, 12, 12, 4, 5, 5, 6, 10, 12, 14, 19, 6, 6, 6, 11, 12, 15, 19, 28, 9, 8, 10, 12, 16, 20, 27, 31, 11, 10, 12, 15, 20, 27, 31, 31, 12, 12, 14, 19, 27, 31, 31, 31, 16, 12, 19, 28, 31, 31, 31, 31],
                    [7, 7, 13, 24, 26, 31, 31, 31, 7, 12, 16, 21, 31, 31, 31, 31, 13, 16, 17, 31, 31, 31, 31, 31, 24, 21, 31, 31, 31, 31, 31, 31, 26, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31],
                )
            if text = "web_very_high"
                return Pillow.Image.SaveJpegQualityPresetInfo(0,
                    [2, 2, 2, 2, 3, 4, 5, 6, 2, 2, 2, 2, 3, 4, 5, 6, 2, 2, 2, 2, 4, 5, 7, 9, 2, 2, 2, 4, 5, 7, 9, 12, 3, 3, 4, 5, 8, 10, 12, 12, 4, 4, 5, 7, 10, 12, 12, 12, 5, 5, 7, 9, 12, 12, 12, 12, 6, 6, 9, 12, 12, 12, 12, 12],
                    [3, 3, 5, 9, 13, 15, 15, 15, 3, 4, 6, 11, 14, 12, 12, 12, 5, 6, 9, 14, 12, 12, 12, 12, 9, 11, 14, 12, 12, 12, 12, 12, 13, 14, 12, 12, 12, 12, 12, 12, 15, 12, 12, 12, 12, 12, 12, 12, 15, 12, 12, 12, 12, 12, 12, 12, 15, 12, 12, 12, 12, 12, 12, 12],
                )
            if text = "web_maximum"
                return Pillow.Image.SaveJpegQualityPresetInfo(0,
                    [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 2, 2, 3, 1, 1, 1, 1, 2, 2, 3, 3, 1, 1, 1, 2, 2, 3, 3, 3, 1, 1, 2, 2, 3, 3, 3, 3],
                    [1, 1, 1, 2, 2, 3, 3, 3, 1, 1, 1, 2, 3, 3, 3, 3, 1, 1, 1, 3, 3, 3, 3, 3, 2, 2, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3],
                )
            if text = "low"
                return Pillow.Image.SaveJpegQualityPresetInfo(2,
                    [18, 14, 14, 21, 30, 35, 34, 17, 14, 16, 16, 19, 26, 23, 12, 12, 14, 16, 17, 21, 23, 12, 12, 12, 21, 19, 21, 23, 12, 12, 12, 12, 30, 26, 23, 12, 12, 12, 12, 12, 35, 23, 12, 12, 12, 12, 12, 12, 34, 12, 12, 12, 12, 12, 12, 12, 17, 12, 12, 12, 12, 12, 12, 12],
                    [20, 19, 22, 27, 20, 20, 17, 17, 19, 25, 23, 14, 14, 12, 12, 12, 22, 23, 14, 14, 12, 12, 12, 12, 27, 14, 14, 12, 12, 12, 12, 12, 20, 14, 12, 12, 12, 12, 12, 12, 20, 12, 12, 12, 12, 12, 12, 12, 17, 12, 12, 12, 12, 12, 12, 12, 17, 12, 12, 12, 12, 12, 12, 12],
                )
            if text = "medium"
                return Pillow.Image.SaveJpegQualityPresetInfo(2,
                    [12, 8, 8, 12, 17, 21, 24, 17, 8, 9, 9, 11, 15, 19, 12, 12, 8, 9, 10, 12, 19, 12, 12, 12, 12, 11, 12, 21, 12, 12, 12, 12, 17, 15, 19, 12, 12, 12, 12, 12, 21, 19, 12, 12, 12, 12, 12, 12, 24, 12, 12, 12, 12, 12, 12, 12, 17, 12, 12, 12, 12, 12, 12, 12],
                    [13, 11, 13, 16, 20, 20, 17, 17, 11, 14, 14, 14, 14, 12, 12, 12, 13, 14, 14, 14, 12, 12, 12, 12, 16, 14, 14, 12, 12, 12, 12, 12, 20, 14, 12, 12, 12, 12, 12, 12, 20, 12, 12, 12, 12, 12, 12, 12, 17, 12, 12, 12, 12, 12, 12, 12, 17, 12, 12, 12, 12, 12, 12, 12],
                )
            if text = "high"
                return Pillow.Image.SaveJpegQualityPresetInfo(0,
                    [6, 4, 4, 6, 9, 11, 12, 16, 4, 5, 5, 6, 8, 10, 12, 12, 4, 5, 5, 6, 10, 12, 12, 12, 6, 6, 6, 11, 12, 12, 12, 12, 9, 8, 10, 12, 12, 12, 12, 12, 11, 10, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 16, 12, 12, 12, 12, 12, 12, 12],
                    [7, 7, 13, 24, 20, 20, 17, 17, 7, 12, 16, 14, 14, 12, 12, 12, 13, 16, 14, 14, 12, 12, 12, 12, 24, 14, 14, 12, 12, 12, 12, 12, 20, 14, 12, 12, 12, 12, 12, 12, 20, 12, 12, 12, 12, 12, 12, 12, 17, 12, 12, 12, 12, 12, 12, 12, 17, 12, 12, 12, 12, 12, 12, 12],
                )
            if text = "maximum"
                return Pillow.Image.SaveJpegQualityPresetInfo(0,
                    [2, 2, 2, 2, 3, 4, 5, 6, 2, 2, 2, 2, 3, 4, 5, 6, 2, 2, 2, 2, 4, 5, 7, 9, 2, 2, 2, 4, 5, 7, 9, 12, 3, 3, 4, 5, 8, 10, 12, 12, 4, 4, 5, 7, 10, 12, 12, 12, 5, 5, 7, 9, 12, 12, 12, 12, 6, 6, 9, 12, 12, 12, 12, 12],
                    [3, 3, 5, 9, 13, 15, 15, 15, 3, 4, 6, 10, 14, 12, 12, 12, 5, 6, 9, 14, 12, 12, 12, 12, 9, 10, 14, 12, 12, 12, 12, 12, 13, 14, 12, 12, 12, 12, 12, 12, 15, 12, 12, 12, 12, 12, 12, 12, 15, 12, 12, 12, 12, 12, 12, 12, 15, 12, 12, 12, 12, 12, 12, 12],
                )
            throw Error("Pillow.Image.Save quality must be an integer, 'keep', or a Pillow JPEG quality preset", -1)
        }

        static SaveJpegQualityPresetInfo(subsampling, luma, chroma) {
            return { Subsampling: subsampling, QTables: Pillow.Image.SaveJpegQTables([luma, chroma]) }
        }

        static SaveTransparencyRgbTuple(value) {
            if !IsObject(value)
                throw Error("Pillow.Image.Save transparency expects an integer or [r, g, b]", -1)
            if Type(value) = "Buffer"
                throw Error("Pillow.Image.Save transparency expects [r, g, b]", -1)
            values := []
            for item in value {
                if !(item is Integer)
                    throw Error("Pillow.Image.Save transparency values must be integers", -1)
                if item < 0 || item > 255
                    throw Error("Pillow.Image.Save transparency values must be in 0..255", -1)
                values.Push(item)
            }
            if values.Length != 3
                throw Error("Pillow.Image.Save transparency expects [r, g, b]", -1)
            return values
        }

        static SaveTransparencyByteTable(value) {
            buf := Pillow.Image.BinaryBuffer(value, "Pillow.Image.Save transparency")
            if buf.Size > 256
                throw Error("Pillow.Image.Save transparency byte table must contain at most 256 values", -1)
            return buf
        }

        static IsPngPrivateChunkType(typeBuffer) {
            return typeBuffer.Size = 4
                && NumGet(typeBuffer, 1, "UChar") >= 0x61
                && NumGet(typeBuffer, 1, "UChar") <= 0x7A
        }

        static IsPngPrivateChunkName(typeName) {
            return (typeName is String)
                && StrLen(typeName) >= 2
                && Ord(SubStr(typeName, 2, 1)) >= 0x61
                && Ord(SubStr(typeName, 2, 1)) <= 0x7A
        }

        static SavePngTextEntries(value) {
            if !(IsObject(value) && value is Pillow.PngImagePlugin.PngInfo)
                throw Error("Pillow.Image.Save pnginfo expects Pillow.PngImagePlugin.PngInfo", -1)
            hasGama := false
            gamaRaw := 0
            customChunk := 0
            customChunkKind := ""
            customChunks := []
            for index, entry in value.ChunkEntries {
                if entry.Type is String {
                    if Pillow.Image.IsPngPrivateChunkName(entry.Type)
                        throw Error("can't concat str to bytes", -1)
                    continue
                }
                if entry.AfterIdat && !Pillow.Image.IsPngPrivateChunkType(entry.Type)
                    throw Error("Pillow.Image.Save pnginfo after_idat chunk is not supported", -1)
                if !entry.AfterIdat
                    && entry.Type.Size = 4
                    && NumGet(entry.Type, 0, "UChar") = 0x67
                    && NumGet(entry.Type, 1, "UChar") = 0x41
                    && NumGet(entry.Type, 2, "UChar") = 0x4D
                    && NumGet(entry.Type, 3, "UChar") = 0x41
                    && entry.Data.Size = 4
                    && !hasGama {
                    hasGama := true
                    gamaRaw := (NumGet(entry.Data, 0, "UChar") << 24)
                        | (NumGet(entry.Data, 1, "UChar") << 16)
                        | (NumGet(entry.Data, 2, "UChar") << 8)
                        | NumGet(entry.Data, 3, "UChar")
                } else if entry.Type.Size = 4
                    && NumGet(entry.Type, 0, "UChar") = 0x63
                    && NumGet(entry.Type, 1, "UChar") = 0x48
                    && NumGet(entry.Type, 2, "UChar") = 0x52
                    && NumGet(entry.Type, 3, "UChar") = 0x4D
                    && entry.Data.Size = 32
                    && !IsObject(customChunk)
                    && customChunks.Length = 0 {
                    customChunk := entry
                    customChunkKind := "cHRM"
                } else if !entry.AfterIdat
                    && entry.Type.Size = 4
                    && NumGet(entry.Type, 0, "UChar") = 0x63
                    && NumGet(entry.Type, 1, "UChar") = 0x49
                    && NumGet(entry.Type, 2, "UChar") = 0x43
                    && NumGet(entry.Type, 3, "UChar") = 0x50
                    && entry.Data.Size = 4
                    && !IsObject(customChunk)
                    && customChunks.Length = 0 {
                    customChunk := entry
                    customChunkKind := "cICP"
                } else if !entry.AfterIdat
                    && entry.Type.Size = 4
                    && NumGet(entry.Type, 0, "UChar") = 0x73
                    && NumGet(entry.Type, 1, "UChar") = 0x52
                    && NumGet(entry.Type, 2, "UChar") = 0x47
                    && NumGet(entry.Type, 3, "UChar") = 0x42
                    && entry.Data.Size = 1
                    && !IsObject(customChunk)
                    && customChunks.Length = 0 {
                    customChunk := entry
                    customChunkKind := "sRGB"
                } else if !entry.AfterIdat
                    && entry.Type.Size = 4
                    && NumGet(entry.Type, 0, "UChar") = 0x73
                    && NumGet(entry.Type, 1, "UChar") = 0x42
                    && NumGet(entry.Type, 2, "UChar") = 0x49
                    && NumGet(entry.Type, 3, "UChar") = 0x54
                    && entry.Data.Size = 3
                    && !IsObject(customChunk)
                    && customChunks.Length = 0 {
                    customChunk := entry
                    customChunkKind := "sBIT"
                } else if !entry.AfterIdat
                    && entry.Type.Size = 4
                    && NumGet(entry.Type, 0, "UChar") = 0x73
                    && NumGet(entry.Type, 1, "UChar") = 0x50
                    && NumGet(entry.Type, 2, "UChar") = 0x4C
                    && NumGet(entry.Type, 3, "UChar") = 0x54
                    && !IsObject(customChunk)
                    && customChunks.Length = 0 {
                    customChunk := entry
                    customChunkKind := "sPLT"
                } else if !entry.AfterIdat
                    && entry.Type.Size = 4
                    && NumGet(entry.Type, 0, "UChar") = 0x62
                    && NumGet(entry.Type, 1, "UChar") = 0x4B
                    && NumGet(entry.Type, 2, "UChar") = 0x47
                    && NumGet(entry.Type, 3, "UChar") = 0x44
                    && (entry.Data.Size = 1 || entry.Data.Size = 6)
                    && !IsObject(customChunk)
                    && customChunks.Length = 0 {
                    customChunk := entry
                    customChunkKind := "bKGD"
                } else if !entry.AfterIdat
                    && entry.Type.Size = 4
                    && NumGet(entry.Type, 0, "UChar") = 0x68
                    && NumGet(entry.Type, 1, "UChar") = 0x49
                    && NumGet(entry.Type, 2, "UChar") = 0x53
                    && NumGet(entry.Type, 3, "UChar") = 0x54
                    && entry.Data.Size >= 2
                    && entry.Data.Size <= 512
                    && Mod(entry.Data.Size, 2) = 0
                    && !IsObject(customChunk)
                    && customChunks.Length = 0 {
                    customChunk := entry
                    customChunkKind := "hIST"
                } else if !entry.AfterIdat
                    && entry.Type.Size = 4
                    && NumGet(entry.Type, 0, "UChar") = 0x74
                    && NumGet(entry.Type, 1, "UChar") = 0x49
                    && NumGet(entry.Type, 2, "UChar") = 0x4D
                    && NumGet(entry.Type, 3, "UChar") = 0x45
                    && entry.Data.Size = 7
                    && !IsObject(customChunk)
                    && customChunks.Length = 0 {
                    customChunk := entry
                    customChunkKind := "tIME"
                } else if Pillow.Image.IsPngPrivateChunkType(entry.Type) {
                    if IsObject(customChunk) && customChunks.Length = 0
                        throw Error("Pillow.Image.Save pnginfo multiple chunks are not supported", -1)
                    customChunks.Push(entry)
                    if !IsObject(customChunk) {
                        customChunk := entry
                        customChunkKind := "private"
                    }
                } else {
                    throw Error("Pillow.Image.Save pnginfo chunk is not supported", -1)
                }
            }
            if hasGama && value.TextEntries.Length > 0
                throw Error("Pillow.Image.Save pnginfo gAMA with text is not supported", -1)
            if hasGama && IsObject(customChunk)
                throw Error("Pillow.Image.Save pnginfo multiple chunks are not supported", -1)
            if value.TextEntries.Length = 0 {
                if hasGama {
                    return {
                        Set: true,
                        Count: 0,
                        HasITextFields: false,
                        HasGama: true,
                        GamaRaw: gamaRaw,
                        HasCustomChunk: false,
                        CustomChunkAfterIdat: false
                    }
                }
                if IsObject(customChunk) {
                    if customChunks.Length > 1 {
                        typeBytes := Buffer(customChunks.Length * 4, 0)
                        dataBuffers := []
                        dataPtrs := Buffer(customChunks.Length * A_PtrSize, 0)
                        dataSizes := Buffer(customChunks.Length * A_PtrSize, 0)
                        afterIdat := Buffer(customChunks.Length * 4, 0)
                        for index, chunk in customChunks {
                            dataBuffers.Push(chunk.Data)
                            loop 4
                                NumPut("UChar", NumGet(chunk.Type, A_Index - 1, "UChar"), typeBytes, (index - 1) * 4 + A_Index - 1)
                            NumPut("Ptr", chunk.Data.Ptr, dataPtrs, (index - 1) * A_PtrSize)
                            NumPut("UPtr", chunk.Data.Size, dataSizes, (index - 1) * A_PtrSize)
                            NumPut("Int", chunk.AfterIdat ? 1 : 0, afterIdat, (index - 1) * 4)
                        }
                        return {
                            Set: true,
                            Count: 0,
                            HasITextFields: false,
                            HasGama: false,
                            GamaRaw: 0,
                            HasCustomChunk: false,
                            HasCustomChunks: true,
                            CustomChunkCount: customChunks.Length,
                            CustomChunkTypes: typeBytes,
                            CustomChunkDataBuffers: dataBuffers,
                            CustomChunkDataPtrs: dataPtrs,
                            CustomChunkDataSizes: dataSizes,
                            CustomChunkAfterIdat: afterIdat
                        }
                    }
                    return {
                        Set: true,
                        Count: 0,
                        HasITextFields: false,
                        HasGama: false,
                        GamaRaw: 0,
                        HasCustomChunk: true,
                        HasCustomChunks: false,
                        CustomChunkKind: customChunkKind,
                        CustomChunkType: customChunk.Type,
                        CustomChunkData: customChunk.Data,
                        CustomChunkAfterIdat: customChunk.AfterIdat ? true : false
                    }
                }
                return { Set: false, Count: 0 }
            }
            keyBuffers := []
            valueBuffers := []
            keyPtrs := Buffer(value.TextEntries.Length * A_PtrSize, 0)
            valuePtrs := Buffer(value.TextEntries.Length * A_PtrSize, 0)
            valueSizes := Buffer(value.TextEntries.Length * A_PtrSize, 0)
            langBuffers := []
            tkeyBuffers := []
            langPtrs := Buffer(value.TextEntries.Length * A_PtrSize, 0)
            tkeyPtrs := Buffer(value.TextEntries.Length * A_PtrSize, 0)
            kinds := Buffer(value.TextEntries.Length * 4, 0)
            compressed := Buffer(value.TextEntries.Length * 4, 0)
            hasITextFields := false
            hasITextText := false
            hasCompressedText := false
            hasValueSizes := false
            for index, entry in value.TextEntries {
                kind := entry.HasOwnProp("Kind") ? entry.Kind : 0
                keyBuffers.Push(Pillow.Image.Utf8Buffer(entry.Key))
                valueSize := 0
                if kind = 0 && entry.HasOwnProp("RawLatin1") && entry.RawLatin1 {
                    if entry.HasOwnProp("HasEmbeddedNul") && entry.HasEmbeddedNul {
                        valueBuffers.Push(entry.Value)
                        valueSize := entry.Value.Size
                        hasValueSizes := true
                    } else {
                        valueBuffer := Pillow.Image.NulTerminatedByteBuffer(entry.Value, "Pillow.PngInfo.add_text")
                        valueBuffers.Push(valueBuffer)
                        valueSize := valueBuffer.Size - 1
                    }
                } else if kind = 0 {
                    valueBuffer := Pillow.Image.Latin1Buffer(entry.Value, "Pillow.PngInfo.add_text value")
                    valueBuffers.Push(valueBuffer)
                    valueSize := valueBuffer.Size - 1
                } else {
                    valueBuffer := Pillow.Image.Utf8Buffer(entry.Value)
                    valueBuffers.Push(valueBuffer)
                    valueSize := valueBuffer.Size - 1
                }
                lang := entry.HasOwnProp("Lang") ? entry.Lang : ""
                tkey := entry.HasOwnProp("TKey") ? entry.TKey : ""
                langBuffers.Push(Pillow.Image.Utf8Buffer(lang))
                tkeyBuffers.Push(Pillow.Image.Utf8Buffer(tkey))
                NumPut("Ptr", keyBuffers[index].Ptr, keyPtrs, (index - 1) * A_PtrSize)
                NumPut("Ptr", valueBuffers[index].Ptr, valuePtrs, (index - 1) * A_PtrSize)
                NumPut("UPtr", valueSize, valueSizes, (index - 1) * A_PtrSize)
                NumPut("Ptr", langBuffers[index].Ptr, langPtrs, (index - 1) * A_PtrSize)
                NumPut("Ptr", tkeyBuffers[index].Ptr, tkeyPtrs, (index - 1) * A_PtrSize)
                NumPut("Int", kind, kinds, (index - 1) * 4)
                NumPut("Int", entry.Zip ? 1 : 0, compressed, (index - 1) * 4)
                if kind = 1
                    hasITextText := true
                if entry.Zip
                    hasCompressedText := true
                if kind = 1 && (lang != "" || tkey != "")
                    hasITextFields := true
            }
            hasCustomChunks := customChunks.Length > 1
            if hasCustomChunks {
                typeBytes := Buffer(customChunks.Length * 4, 0)
                dataBuffers := []
                dataPtrs := Buffer(customChunks.Length * A_PtrSize, 0)
                dataSizes := Buffer(customChunks.Length * A_PtrSize, 0)
                afterIdat := Buffer(customChunks.Length * 4, 0)
                for index, chunk in customChunks {
                    dataBuffers.Push(chunk.Data)
                    loop 4
                        NumPut("UChar", NumGet(chunk.Type, A_Index - 1, "UChar"), typeBytes, (index - 1) * 4 + A_Index - 1)
                    NumPut("Ptr", chunk.Data.Ptr, dataPtrs, (index - 1) * A_PtrSize)
                    NumPut("UPtr", chunk.Data.Size, dataSizes, (index - 1) * A_PtrSize)
                    NumPut("Int", chunk.AfterIdat ? 1 : 0, afterIdat, (index - 1) * 4)
                }
            }
            return {
                Set: true,
                Count: value.TextEntries.Length,
                HasITextFields: hasITextFields,
                HasITextText: hasITextText,
                HasCompressedText: hasCompressedText,
                HasValueSizes: hasValueSizes,
                HasGama: false,
                GamaRaw: 0,
                HasCustomChunk: IsObject(customChunk) && !hasCustomChunks,
                HasCustomChunks: hasCustomChunks,
                CustomChunkKind: customChunkKind,
                CustomChunkType: IsObject(customChunk) && !hasCustomChunks ? customChunk.Type : 0,
                CustomChunkData: IsObject(customChunk) && !hasCustomChunks ? customChunk.Data : 0,
                CustomChunkAfterIdat: IsObject(customChunk) && !hasCustomChunks && customChunk.AfterIdat ? true : false,
                CustomChunkCount: hasCustomChunks ? customChunks.Length : 0,
                CustomChunkTypes: hasCustomChunks ? typeBytes : 0,
                CustomChunkDataBuffers: hasCustomChunks ? dataBuffers : [],
                CustomChunkDataPtrs: hasCustomChunks ? dataPtrs : 0,
                CustomChunkDataSizes: hasCustomChunks ? dataSizes : 0,
                CustomChunkAfterIdatArray: hasCustomChunks ? afterIdat : 0,
                KeyBuffers: keyBuffers,
                ValueBuffers: valueBuffers,
                ValueSizes: valueSizes,
                LangBuffers: langBuffers,
                TKeyBuffers: tkeyBuffers,
                KeyPtrs: keyPtrs,
                ValuePtrs: valuePtrs,
                LangPtrs: langPtrs,
                TKeyPtrs: tkeyPtrs,
                Kinds: kinds,
                Compressed: compressed
            }
        }

        static SaveHotspotPair(value) {
            if !IsObject(value)
                throw Error("Pillow.Image.Save hotspot expects [x, y]", -1)
            values := []
            for item in value {
                if !(item is Integer)
                    throw Error("Pillow.Image.Save hotspot values must be integers", -1)
                if item < 0
                    throw Error("Pillow.Image.Save hotspot values must be non-negative", -1)
                values.Push(item)
            }
            if values.Length != 2
                throw Error("Pillow.Image.Save hotspot expects [x, y]", -1)
            return values
        }

        static SaveIcoSizePairs(value) {
            if value is String
                ; Pillow iterates the value and compares the characters
                ; against integer sizes, raising this exact TypeError.
                throw Error("'>' not supported between instances of 'str' and 'int'", -1)
            if !IsObject(value)
                throw Error("Pillow.Image.Save sizes expects an array of [width, height] pairs", -1)
            flat := []
            for size in value {
                if !IsObject(size)
                    throw Error("Pillow.Image.Save sizes expects [width, height] pairs", -1)
                pair := []
                for item in size {
                    if !(item is Integer)
                        throw Error("Pillow.Image.Save sizes values must be integers", -1)
                    if item <= 0
                        throw Error("Pillow.Image.Save sizes values must be greater than 0", -1)
                    pair.Push(item)
                }
                if pair.Length != 2
                    throw Error("Pillow.Image.Save sizes expects [width, height] pairs", -1)
                flat.Push(pair[1], pair[2])
            }
            return {
                Buffer: flat.Length ? Pillow.Image.IntBuffer(flat, "Pillow.Image.Save sizes") : Buffer(0),
                Count: flat.Length // 2,
            }
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

        GetIm() {
            ; Pillow 11.3.0's getim() returns the low-level "Pillow Imaging"
            ; capsule for open images and raises "Operation on closed image"
            ; once closed; the AHK analogue is the native handle pointer.
            ; (AHK identifiers are case-insensitive, so GetIm() also serves
            ; as the getim() alias.)
            if !this.HasOwnProp("Handle") || this.Handle = 0
                throw Error("Operation on closed image", -1)
            return this.Handle
        }

        Im {
            get {
                ; Pillow 11.3.0's `im` attribute is the per-image ImagingCore
                ; C object (bands/size/histogram/getpixel/transform/...); this
                ; runtime's analogue is the native image handle, and the
                ; ImagingCore method surface is covered by the pillow_c_* ABI
                ; exports behind the facade. AHK case-insensitivity makes Im
                ; serve `im`; the handle is the explicit documented boundary.
                return this.GetIm()
            }
        }

        ToQImage() {
            ; Pillow 11.3.0 raises ImportError("Qt bindings are not
            ; installed") without PyQt6/PySide6; this runtime ships no Qt
            ; binding, so the method is an explicit documented boundary
            ; (AHK case-insensitivity makes toqimage() the same method).
            this.RequireHandle()
            throw Error("Qt bindings are not installed", -1)
        }

        ToQPixmap() {
            this.RequireHandle()
            throw Error("Qt bindings are not installed", -1)
        }

        Show(title := unset) {
            ; Pillow 11.3.0 dispatches to a registered system viewer; this
            ; runtime ships no viewer registry, so show() is an explicit
            ; documented boundary with the Pillow-shaped error.
            this.RequireHandle()
            throw Error("no viewers found", -1)
        }

        RefreshBufferView() {
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_refresh_buffer",
                "Ptr", this.RequireHandle(),
                "Int"
            ))
        }

        DetachBufferView() {
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_detach_buffer",
                "Ptr", this.RequireHandle(),
                "Int"
            ))
            this.BufferViewSource := 0
        }

        Load() {
            ; BEHAV-ICNS-001: pixel access resolves the pre-load rawmode
            ; quirk state like Pillow's load().
            this.IcnsQuirkPending := false
            this.RequireHandle()
            return Pillow.Image.PixelAccess(this)
        }

        Tell() {
            this.RequireHandle()
            return this.FrameIndex
        }

        Seek(frame) {
            this.RequireHandle()
            if !(frame is Integer)
                throw Error("Pillow.Image.Seek frame must be an integer", -1)
            if this.FrameCount <= 1 {
                if frame != 0
                    throw Error("no more images in file", -1)
                return
            }
            if frame < 0 || frame >= this.FrameCount
                throw Error("attempt to seek outside sequence", -1)
            if frame = this.FrameIndex
                return

            outHandle := 0
            pathBytes := Pillow.Image.Utf8Buffer(this.FramePath)
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_open_" StrLower(this.FrameFormat) "_frame",
                "Ptr", pathBytes,
                "Int", frame,
                "Ptr*", &outHandle,
                "Int"
            )
            if status != 0 {
                if outHandle
                    Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", outHandle, "Int"))
                Pillow.CheckStatus(status)
            }
            oldHandle := this.Handle
            oldFrameIndex := this.FrameIndex
            this.Handle := outHandle
            this.FrameIndex := frame
            try {
                this.ApplyNativeMetadata()
                this.ApplyFrameMetadata()
            } catch {
                this.Handle := oldHandle
                this.FrameIndex := oldFrameIndex
                Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", outHandle, "Int"))
                throw
            }
            Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", oldHandle, "Int"))
        }

        ApplyNativeMetadata() {
            this.RequireHandle()
            hasDpi := 0
            dpiX := 0.0
            dpiY := 0.0
            jfif := 0
            jfifMajor := 0
            jfifMinor := 0
            jfifUnit := -1
            jfifDensityX := 0
            jfifDensityY := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_metadata_resolution",
                "Ptr", this.RequireHandle(),
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
            ))
            if hasDpi
                this.Info["dpi"] := [dpiX, dpiY]
            if jfif {
                this.Info["jfif"] := jfif
                this.Info["jfif_version"] := [jfifMajor, jfifMinor]
                this.Info["jfif_unit"] := jfifUnit
                this.Info["jfif_density"] := [jfifDensityX, jfifDensityY]
            }

            hasHotspot := 0
            hotspotX := 0
            hotspotY := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_metadata_hotspot",
                "Ptr", this.RequireHandle(),
                "Int*", &hasHotspot,
                "Int*", &hotspotX,
                "Int*", &hotspotY,
                "Int"
            ))
            if hasHotspot
                this.Info["hotspot"] := [hotspotX, hotspotY]
            else if this.Info.Has("hotspot")
                this.Info.Delete("hotspot")

            hasDibCompression := 0
            dibCompression := -1
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_metadata_dib_compression",
                "Ptr", this.RequireHandle(),
                "Int*", &hasDibCompression,
                "Int*", &dibCompression,
                "Int"
            ))
            if hasDibCompression
                this.Info["compression"] := dibCompression
            else if this.Info.Has("compression") && this.Format = "CUR"
                this.Info.Delete("compression")

            hasGamma := 0
            gamma := 0.0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_metadata_png_gamma",
                "Ptr", this.RequireHandle(),
                "Int*", &hasGamma,
                "Double*", &gamma,
                "Int"
            ))
            if hasGamma
                this.Info["gamma"] := gamma
            else if this.Info.Has("gamma")
                this.Info.Delete("gamma")

            hasSrgb := 0
            srgb := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_metadata_png_srgb",
                "Ptr", this.RequireHandle(),
                "Int*", &hasSrgb,
                "Int*", &srgb,
                "Int"
            ))
            if hasSrgb
                this.Info["srgb"] := srgb
            else if this.Info.Has("srgb") && this.Format = "PNG"
                this.Info.Delete("srgb")

            hasChromaticity := 0
            chromaticityBuffer := Buffer(8 * 8, 0)
            chromaticityStatus := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_metadata_png_chromaticity",
                "Ptr", this.RequireHandle(),
                "Int*", &hasChromaticity,
                "Ptr", chromaticityBuffer,
                "UPtr", 8,
                "Int"
            )
            Pillow.CheckStatus(chromaticityStatus)
            if hasChromaticity {
                chromaticity := []
                loop 8
                    chromaticity.Push(NumGet(chromaticityBuffer, (A_Index - 1) * 8, "Double"))
                this.Info["chromaticity"] := chromaticity
            } else if this.Info.Has("chromaticity") && this.Format = "PNG" {
                this.Info.Delete("chromaticity")
            }

            textCount := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_metadata_png_text_count",
                "Ptr", this.RequireHandle(),
                "UPtr*", &textCount,
                "Int"
            ))
            if this.Format = "PNG"
                this.Text := Map()
            loop textCount {
                keyRequired := 0
                valueRequired := 0
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_metadata_png_text",
                    "Ptr", this.RequireHandle(),
                    "UPtr", A_Index - 1,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &keyRequired,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &valueRequired,
                    "Int"
                )
                if status != -1
                    Pillow.CheckStatus(status)
                key := Buffer(keyRequired, 0)
                value := Buffer(valueRequired, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_metadata_png_text",
                    "Ptr", this.RequireHandle(),
                    "UPtr", A_Index - 1,
                    "Ptr", key,
                    "UPtr", key.Size,
                    "UPtr*", &keyRequired,
                    "Ptr", value,
                    "UPtr", value.Size,
                    "UPtr*", &valueRequired,
                    "Int"
                ))
                textKey := Pillow.Image.Utf8StringFromBytes(key, keyRequired - 1, "Pillow.Image PNG text key")
                textValue := Pillow.Image.Utf8StringFromBytes(value, valueRequired - 1, "Pillow.Image PNG text value")
                this.Info[textKey] := textValue
                if this.Format = "PNG"
                    this.Text[textKey] := textValue
            }

            xmp := Pillow.Image.NativeMetadataBlob(this.RequireHandle(), "pillow_c_image_metadata_xmp")
            if IsObject(xmp)
                this.Info["xmp"] := xmp
            else if this.Info.Has("xmp") && (this.Format = "PNG" || this.Format = "JPEG" || this.Format = "TIFF")
                this.Info.Delete("xmp")

            hasIccProfile := 0
            iccRequired := 0
            iccStatus := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_metadata_png_icc_profile",
                "Ptr", this.RequireHandle(),
                "Int*", &hasIccProfile,
                "Ptr", 0,
                "UPtr", 0,
                "UPtr*", &iccRequired,
                "Int"
            )
            if iccStatus != -1
                Pillow.CheckStatus(iccStatus)
            if hasIccProfile {
                iccProfile := Buffer(iccRequired, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_metadata_png_icc_profile",
                    "Ptr", this.RequireHandle(),
                    "Int*", &hasIccProfile,
                    "Ptr", iccProfile,
                    "UPtr", iccProfile.Size,
                    "UPtr*", &iccRequired,
                    "Int"
                ))
                this.Info["icc_profile"] := iccProfile
            } else if this.Info.Has("icc_profile") && this.Format = "PNG" {
                this.Info.Delete("icc_profile")
            }

            hasExif := 0
            exifRequired := 0
            exifStatus := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_metadata_png_exif",
                "Ptr", this.RequireHandle(),
                "Int*", &hasExif,
                "Ptr", 0,
                "UPtr", 0,
                "UPtr*", &exifRequired,
                "Int"
            )
            if exifStatus != -1
                Pillow.CheckStatus(exifStatus)
            if hasExif {
                exif := Buffer(exifRequired, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_metadata_png_exif",
                    "Ptr", this.RequireHandle(),
                    "Int*", &hasExif,
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "UPtr*", &exifRequired,
                    "Int"
                ))
                this.Info["exif"] := exif
            } else if this.Info.Has("exif") && this.Format = "PNG" {
                this.Info.Delete("exif")
            }

            if this.Format = "JPEG" {
                jpegComment := Pillow.Image.NativeMetadataBlob(this.RequireHandle(), "pillow_c_image_metadata_jpeg_comment")
                if IsObject(jpegComment)
                    this.Info["comment"] := jpegComment
                else if this.Info.Has("comment")
                    this.Info.Delete("comment")

                jpegIccState := -1
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_metadata_jpeg_icc_profile_state",
                    "Ptr", this.RequireHandle(),
                    "Int*", &jpegIccState,
                    "Int"
                ))
                switch jpegIccState {
                    case 0:
                        if this.Info.Has("icc_profile")
                            this.Info.Delete("icc_profile")
                    case 1:
                        jpegIccProfile := Pillow.Image.NativeMetadataBlob(
                            this.RequireHandle(), "pillow_c_image_metadata_jpeg_icc_profile")
                        if !IsObject(jpegIccProfile)
                            throw Error("Pillow.Image JPEG ICC state/profile mismatch", -1)
                        this.Info["icc_profile"] := jpegIccProfile
                    case 2:
                        this.Info["icc_profile"] := ""
                    default:
                        throw Error("Pillow.Image JPEG ICC metadata state is invalid", -1)
                }

                jpegExif := Pillow.Image.NativeMetadataBlob(this.RequireHandle(), "pillow_c_image_metadata_jpeg_exif")
                if IsObject(jpegExif)
                    this.Info["exif"] := jpegExif
                else if this.Info.Has("exif")
                    this.Info.Delete("exif")

                photoshopResourceCount := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_metadata_jpeg_photoshop_resource_count",
                    "Ptr", this.RequireHandle(),
                    "UPtr*", &photoshopResourceCount,
                    "Int"
                ))
                photoshop := Map()
                loop photoshopResourceCount {
                    resourceCode := 0
                    resourceRequired := 0
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_metadata_jpeg_photoshop_resource",
                        "Ptr", this.RequireHandle(),
                        "UPtr", A_Index - 1,
                        "Int*", &resourceCode,
                        "Ptr", 0,
                        "UPtr", 0,
                        "UPtr*", &resourceRequired,
                        "Int"
                    ))
                    resourceValue := Buffer(resourceRequired, 0)
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_metadata_jpeg_photoshop_resource",
                        "Ptr", this.RequireHandle(),
                        "UPtr", A_Index - 1,
                        "Int*", &resourceCode,
                        "Ptr", resourceValue,
                        "UPtr", resourceValue.Size,
                        "UPtr*", &resourceRequired,
                        "Int"
                    ))
                    photoshop[resourceCode] := resourceValue
                }

                hasResolutionInfo := 0
                xResolution := 0.0
                displayedUnitsX := 0
                yResolution := 0.0
                displayedUnitsY := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_metadata_jpeg_photoshop_resolution_info",
                    "Ptr", this.RequireHandle(),
                    "Int*", &hasResolutionInfo,
                    "Double*", &xResolution,
                    "Int*", &displayedUnitsX,
                    "Double*", &yResolution,
                    "Int*", &displayedUnitsY,
                    "Int"
                ))
                if hasResolutionInfo {
                    photoshop[0x03ED] := Map(
                        "XResolution", xResolution,
                        "DisplayedUnitsX", displayedUnitsX,
                        "YResolution", yResolution,
                        "DisplayedUnitsY", displayedUnitsY)
                }

                if photoshop.Count > 0
                    this.Info["photoshop"] := photoshop
                else if this.Info.Has("photoshop")
                    this.Info.Delete("photoshop")
            }

            if this.Format = "TIFF" {
                tiffIccProfile := Pillow.Image.NativeMetadataBlob(this.RequireHandle(), "pillow_c_image_metadata_tiff_icc_profile")
                if IsObject(tiffIccProfile)
                    this.Info["icc_profile"] := tiffIccProfile
                else if this.Info.Has("icc_profile")
                    this.Info.Delete("icc_profile")
            }

            hasPngTransparency := 0
            pngTransparency := -1
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_metadata_png_transparency",
                "Ptr", this.RequireHandle(),
                "Int*", &hasPngTransparency,
                "Int*", &pngTransparency,
                "Int"
            ))
            if hasPngTransparency && (this.Mode = "P" || this.Mode = "L")
                this.Info["transparency"] := pngTransparency
            else if this.Info.Has("transparency") && this.Format = "PNG"
                this.Info.Delete("transparency")

            pngTransparencyTable := Pillow.Image.NativeMetadataBlob(this.RequireHandle(), "pillow_c_image_metadata_png_transparency_table")
            if IsObject(pngTransparencyTable) && this.Mode = "P"
                this.Info["transparency"] := pngTransparencyTable

            hasPngRgbTransparency := 0
            pngTransparencyR := -1
            pngTransparencyG := -1
            pngTransparencyB := -1
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_metadata_png_rgb_transparency",
                "Ptr", this.RequireHandle(),
                "Int*", &hasPngRgbTransparency,
                "Int*", &pngTransparencyR,
                "Int*", &pngTransparencyG,
                "Int*", &pngTransparencyB,
                "Int"
            ))
            if hasPngRgbTransparency && this.Mode = "RGB"
                this.Info["transparency"] := [pngTransparencyR, pngTransparencyG, pngTransparencyB]
            else if this.Info.Has("transparency") && this.Format = "PNG" && this.Mode != "P" && this.Mode != "L"
                this.Info.Delete("transparency")

            if this.Format = "ICO" && this.FramePath != ""
                this.Info["sizes"] := Pillow.Image.IcoSizes(this.FramePath)
            else if this.Info.Has("sizes") && this.Format != "ICO"
                this.Info.Delete("sizes")
        }

        ApplyFrameMetadata() {
            this.RequireHandle()
            if !(this.FrameFormat = "GIF" && this.FramePath != "")
                return

            duration := -1
            loopCount := -1
            disposal := -1
            background := -1
            transparency := -1
            pathBytes := Pillow.Image.Utf8Buffer(this.FramePath)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_gif_metadata_ex",
                "Ptr", pathBytes,
                "Int", this.FrameIndex,
                "Int*", &duration,
                "Int*", &loopCount,
                "Int*", &disposal,
                "Int*", &background,
                "Int*", &transparency,
                "Int"
            ))
            Pillow.Image.SetOptionalInfo(this.Info, "duration", duration)
            Pillow.Image.SetOptionalInfo(this.Info, "loop", loopCount)
            Pillow.Image.SetOptionalInfo(this.Info, "background", background)
            gifComment := Pillow.Image.NativeGifComment(this.FramePath, this.FrameIndex)
            if IsObject(gifComment) {
                this.Info["comment"] := gifComment
            } else if this.Info.Has("comment") {
                this.Info.Delete("comment")
            }
            if this.Mode = "P" {
                Pillow.Image.SetOptionalInfo(this.Info, "transparency", transparency)
            } else if this.Info.Has("transparency") {
                this.Info.Delete("transparency")
            }
            this.DisposalMethod := disposal >= 0 ? disposal : 0
        }

        static SetOptionalInfo(info, key, value) {
            if value >= 0 {
                info[key] := value
            } else if info.Has(key) {
                info.Delete(key)
            }
        }

        NFrames {
            get {
                this.RequireHandle()
                return this.FrameCount
            }
        }

        n_frames {
            get => this.NFrames
        }

        IsAnimated {
            get => this.NFrames > 1
        }

        is_animated {
            get => this.IsAnimated
        }

        ico {
            get => Pillow.Image.IcoFile(this)
        }

        FormatDescription {
            get => this.Format = "" ? "" : Pillow.Image.FormatDescription(this.Format)
        }

        format_description {
            get => this.FormatDescription
        }

        HasTransparencyData {
            get {
                mode := this.Mode
                return mode = "RGBA" || mode = "LA" || mode = "PA" || this.Info.Has("transparency")
            }
        }

        has_transparency_data {
            get => this.HasTransparencyData
        }

        GetChildImages() {
            this.RequireHandle()
            return []
        }

        get_child_images() {
            return this.GetChildImages()
        }

        disposal_method {
            get => this.DisposalMethod
        }

        Verify() {
            this.RequireHandle()
        }

        Draft(mode, size) {
            this.RequireHandle()
            if this.JpegDraftApplied
                return
            currentMode := this.Mode
            sameModeDraft := currentMode = mode
                && (mode = "CMYK" || mode = "RGB")
            requestedModeDraft := currentMode = "RGB"
                && (mode = "L" || mode = "YCbCr")
            if !(this.Format = "JPEG" && this.FramePath != ""
                && (sameModeDraft || requestedModeDraft))
                return
            if !IsObject(size) || size.Length != 2
                throw Error("Pillow.Image.Draft size must be [width, height]", -1)
            if !(size[1] is Integer) || !(size[2] is Integer)
                throw Error("Pillow.Image.Draft size values must be integers", -1)
            if size[1] <= 0 || size[2] <= 0
                throw Error("Pillow.Image.Draft size values must be positive", -1)

            originalSize := this.Size
            pathBytes := Pillow.Image.Utf8Buffer(this.FramePath)
            outHandle := 0
            scale := 0
            if requestedModeDraft {
                status := DllCall(
                    Pillow.RequireDllPath()
                        "\pillow_c_image_open_jpeg_draft_mode",
                    "Ptr", pathBytes,
                    "Int", Pillow.ModeId(mode),
                    "Int", size[1],
                    "Int", size[2],
                    "Ptr*", &outHandle,
                    "Int*", &scale,
                    "Int"
                )
            } else {
                status := DllCall(
                    Pillow.RequireDllPath()
                        "\pillow_c_image_open_jpeg_draft",
                    "Ptr", pathBytes,
                    "Int", size[1],
                    "Int", size[2],
                    "Ptr*", &outHandle,
                    "Int*", &scale,
                    "Int"
                )
            }
            if status != 0 {
                if outHandle
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_free",
                        "Ptr", outHandle,
                        "Int"
                    ))
                Pillow.CheckStatus(status)
            }

            oldHandle := this.Handle
            oldInfo := this.Info
            this.Handle := outHandle
            this.Info := Map()
            try {
                this.ApplyNativeMetadata()
                this.ApplyFrameMetadata()
            } catch {
                this.Handle := oldHandle
                this.Info := oldInfo
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_free",
                    "Ptr", outHandle,
                    "Int"
                ))
                throw
            }
            this.JpegDraftApplied := true
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_free",
                "Ptr", oldHandle,
                "Int"
            ))
            return [this.Mode, [
                0,
                0,
                originalSize[1] / scale,
                originalSize[2] / scale
            ]]
        }

        Mode {
            get {
                mode := 0
                Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_mode", "Ptr", this.RequireHandle(), "Int*", &mode, "Int"))
                return Pillow.ModeName(mode)
            }
        }

        _ReadOnly := 0

        ; BEHAV-ICNS-001: True while an ICNS-opened image has not had its
        ; first pixel access; the first no-args ToBytes() then replays
        ; Pillow's pre-load rawmode quirk (RGB packs to RGBA, other
        ; non-RGBA modes raise "No packer found from {mode} to RGBA").
        IcnsQuirkPending := false

        ReadOnly {
            get {
                ; Pillow 11.3.0: (self._im and self._im.readonly) or
                ; self._readonly — the core alias flag wins, then the
                ; facade flag set through the readonly setter.
                readonly := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_readonly",
                    "Ptr", this.RequireHandle(),
                    "Int*", &readonly,
                    "Int"
                ))
                if readonly
                    return readonly
                return this._ReadOnly
            }
            set {
                ; Pillow 11.3.0's setter stores _readonly directly.
                this._ReadOnly := value
            }
        }

        ExifOrientation() {
            orientation := 0
            Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_exif_orientation", "Ptr", this.RequireHandle(), "Int*", &orientation, "Int"))
            return orientation
        }

        GetExif() {
            this.RequireHandle()
            return Pillow.Image.Exif.FromImage(this)
        }

        GetXmp() {
            this.RequireHandle()
            if !this.Info.Has("xmp")
                return Map()
            return Pillow.Image.ParseXmpBuffer(this.Info["xmp"])
        }

        Width {
            get => this.GetInt("pillow_c_image_width")
        }

        Height {
            get => this.GetInt("pillow_c_image_height")
        }

        Size {
            get => [this.Width, this.Height]
            set {
                this.RequireHandle()
                if this.Format != "ICO" || this.FramePath = ""
                    throw Error("Pillow.Image.Size setter currently supports ICO images", -1)
                if !IsObject(value) || value.Length != 2
                    throw Error("Pillow.Image.Size expects [width, height]", -1)
                if !(value[1] is Integer) || !(value[2] is Integer)
                    throw Error("Pillow.Image.Size values must be integers", -1)

                outHandle := 0
                pathBytes := Pillow.Image.Utf8Buffer(this.FramePath)
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_open_ico_size",
                    "Ptr", pathBytes,
                    "Int", value[1],
                    "Int", value[2],
                    "Ptr*", &outHandle,
                    "Int"
                )
                if status != 0 {
                    if outHandle
                        Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", outHandle, "Int"))
                    Pillow.CheckStatus(status)
                }

                oldHandle := this.Handle
                this.Handle := outHandle
                try {
                    this.ApplyNativeMetadata()
                    this.ApplyFrameMetadata()
                } catch {
                    this.Handle := oldHandle
                    Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", outHandle, "Int"))
                    throw
                }
                Pillow.CheckStatus(DllCall(Pillow.RequireDllPath() "\pillow_c_image_free", "Ptr", oldHandle, "Int"))
            }
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

        ToBytes(encoder := unset, rawmode := unset, orientation := 1) {
            if this.IcnsQuirkPending && !IsSet(encoder) && !IsSet(rawmode) {
                ; BEHAV-ICNS-001: Pillow's tobytes() snapshots self.mode
                ; ("RGBA", the plugin's pre-load value) BEFORE load(), so
                ; the first no-args tobytes() on an ICNS whose best icon
                ; loads as RGB packs RGBA (opaque alpha), and any other
                ; non-RGBA mode raises "No packer found from {mode} to
                ; RGBA". Explicit rawmode/encoder calls skip the quirk.
                this.IcnsQuirkPending := false
                if this.Mode = "RGB" {
                    converted := this.Convert("RGBA")
                    try
                        return converted.ToBytes()
                    finally
                        converted.Close()
                }
                throw Error("No packer found from " this.Mode " to RGBA", -1)
            }
            this.RefreshBufferView()
            if IsSet(encoder) {
                if encoder != "raw"
                    throw Error("Pillow.Image.ToBytes currently supports only the raw encoder", -1)
                if !IsSet(rawmode)
                    rawmode := this.Mode
                rawModeBytes := Pillow.Image.RawModeBuffer(rawmode)
                required := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_get_raw_bytes_oriented",
                    "Ptr", this.RequireHandle(),
                    "Ptr", rawModeBytes,
                    "Int", orientation,
                    "Ptr", 0,
                    "UPtr", 0,
                    "UPtr*", &required,
                    "Int"
                ))
                out := Buffer(required, 0)
                if required = 0
                    return out
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_get_raw_bytes_oriented",
                    "Ptr", this.RequireHandle(),
                    "Ptr", rawModeBytes,
                    "Int", orientation,
                    "Ptr", out,
                    "UPtr", out.Size,
                    "UPtr*", &required,
                    "Int"
                ))
                return out
            }

            if this.Mode = "1" || this.Mode = "LAB"
                return this.ToBytes("raw", this.Mode)

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

        ToBitmap(name := "image") {
            nameBytes := Pillow.Image.Utf8Buffer(String(name))
            required := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_tobitmap",
                "Ptr", this.RequireHandle(),
                "Ptr", nameBytes,
                "Ptr", 0,
                "UPtr", 0,
                "UPtr*", &required,
                "Int"
            ))
            out := Buffer(required, 0)
            if required = 0
                return out
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_tobitmap",
                "Ptr", this.RequireHandle(),
                "Ptr", nameBytes,
                "Ptr", out,
                "UPtr", out.Size,
                "UPtr*", &required,
                "Int"
            ))
            return out
        }

        Save(path, format := unset, options := unset) {
            if !(path is String)
                throw Error("Pillow.Image.Save expects a file path", -1)

            saveFormat := unset
            saveOptions := unset
            if IsSet(format) {
                if IsObject(format) && !IsSet(options) {
                    saveOptions := format
                    formatOption := Pillow.Image.SaveOption(saveOptions, "Format", "format")
                    if formatOption.Set
                        saveFormat := formatOption.Value
                } else {
                    saveFormat := format
                }
            }
            if IsSet(options)
                saveOptions := options

            resolvedFormat := Pillow.Image.ResolveSaveFormat(path, IsSet(saveFormat) ? saveFormat : unset)
            if resolvedFormat = "DIB" {
                ; BEHAV-DIB-001: Pillow's DIB is byte-identical to its BMP
                ; minus the 14-byte BITMAPFILEHEADER; save reuses the
                ; byte-matched native BMP encoder and strips that header.
                this.SaveDib(path)
                return
            }
            if resolvedFormat = "IM" {
                ; BEHAV-IM-001: Pillow's IM is a 512-byte ASCII header
                ; (with an optional 768-byte palette LUT for P) plus raw
                ; pixel bytes; save composes it over the raw encoder.
                this.SaveIm(path)
                return
            }
            if resolvedFormat = "MSP" {
                ; BEHAV-MSP-001: Pillow's MSP is mode-1 only ("DanM"
                ; uncompressed); the native codec writes the exact header.
                if this.Mode != "1"
                    throw Error("cannot write mode " this.Mode " as MSP", -1)
                pathBytes := Pillow.Image.Utf8Buffer(path)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_msp",
                    "Ptr", this.RequireHandle(),
                    "Ptr", pathBytes,
                    "Int"
                ))
                return
            }
            if resolvedFormat = "PALM" {
                ; BEHAV-PALM-001: Pillow's Palm pixmap is save-only; the
                ; bounded slice covers the 8-bit P path (the L-with-bpp and
                ; mode-1 inverted slices stay separate children).
                this.SavePalm(path)
                return
            }
            if resolvedFormat = "BLP" {
                ; BEHAV-BLP-001: Pillow's BLP is P-mode only; the native
                ; codec writes the exact BLP1/BLP2 header, the 128-byte
                ; preamble, the quirky linear palette, and the indices
                ; (RGB-palette slice; RGBA-palette alpha stays a child).
                if this.Mode != "P"
                    throw Error("Unsupported BLP image mode", -1)
                blp1 := 0
                if IsSet(saveOptions) {
                    versionOption := Pillow.Image.SaveOption(saveOptions, "BlpVersion", "blp_version")
                    if versionOption.Set && versionOption.Value = "BLP1"
                        blp1 := 1
                }
                pathBytes := Pillow.Image.Utf8Buffer(path)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_blp",
                    "Ptr", this.RequireHandle(),
                    "Ptr", pathBytes,
                    "Int", blp1,
                    "Int"
                ))
                return
            }
            if resolvedFormat = "SPIDER" {
                ; BEHAV-SPIDER-001: Pillow's SPIDER is the float header
                ; records plus raw native float32 samples; save composes
                ; the exact header over the F;32NF raw encoder.
                this.SaveSpider(path)
                return
            }
            if resolvedFormat = "PCX" {
                ; BEHAV-PCX-001: Pillow's PCX is mode 1/L/P/RGB with the
                ; RLE rows and the L/P palette trailers; the native codec
                ; writes the exact bytes (other modes raise Pillow's
                ; ValueError message).
                if !(this.Mode = "1" || this.Mode = "L" || this.Mode = "P" || this.Mode = "RGB")
                    throw Error("Cannot save " this.Mode " images as PCX", -1)
                pathBytes := Pillow.Image.Utf8Buffer(path)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_pcx",
                    "Ptr", this.RequireHandle(),
                    "Ptr", pathBytes,
                    "Int"
                ))
                return
            }
            if resolvedFormat = "SGI" {
                ; BEHAV-SGI-001: Pillow's SGI save covers L/RGB/RGBA with
                ; the bpc option (1/2); the native codec writes the exact
                ; 512-byte header (path-basename name field) and the
                ; band-major bottom-up payload (v<<8 samples for bpc 2).
                ; Other modes and bpc values raise Pillow's exact
                ; ValueError messages.
                if !(this.Mode = "L" || this.Mode = "RGB" || this.Mode = "RGBA")
                    throw Error("Unsupported SGI image mode", -1)
                bpc := 1
                if IsSet(saveOptions) {
                    option := Pillow.Image.SaveOption(saveOptions, "bpc", "Bpc")
                    if option.Set
                        bpc := option.Value
                }
                if !(bpc = 1 || bpc = 2)
                    throw Error("Unsupported number of bytes per pixel", -1)
                pathBytes := Pillow.Image.Utf8Buffer(path)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_sgi",
                    "Ptr", this.RequireHandle(),
                    "Ptr", pathBytes,
                    "Int", bpc,
                    "Int"
                ))
                return
            }
            if resolvedFormat = "DDS" {
                ; BEHAV-DDS-001: Pillow's DDS covers L/LA/RGB/RGBA raw
                ; writes and DXT1/3/5 plus BC2/BC3/BC5 BCN writes; other
                ; modes and pixel formats raise the exact OSError
                ; messages (the native codec writes the exact header and
                ; BCN blocks).
                if !(this.Mode = "L" || this.Mode = "LA" || this.Mode = "RGB" || this.Mode = "RGBA")
                    throw Error("cannot write mode " this.Mode " as DDS", -1)
                pixelFormat := ""
                if IsSet(saveOptions) {
                    option := Pillow.Image.SaveOption(saveOptions, "pixel_format", "PixelFormat")
                    if option.Set
                        pixelFormat := option.Value
                }
                if pixelFormat != "" && !(pixelFormat = "DXT1" || pixelFormat = "DXT3" || pixelFormat = "DXT5" || pixelFormat = "BC2" || pixelFormat = "BC3" || pixelFormat = "BC5")
                    throw Error("cannot write pixel format " pixelFormat, -1)
                if pixelFormat = "BC5" && this.Mode != "RGB"
                    throw Error("only RGB mode can be written as BC5", -1)
                pathBytes := Pillow.Image.Utf8Buffer(path)
                fmtBytes := pixelFormat = "" ? 0 : Pillow.Image.Utf8Buffer(pixelFormat)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_dds",
                    "Ptr", this.RequireHandle(),
                    "Ptr", pathBytes,
                    "Ptr", fmtBytes,
                    "Int"
                ))
                return
            }
            if resolvedFormat = "ICNS" {
                ; BEHAV-ICNS-001: Pillow writes the 8-byte "icns" header,
                ; a TOC chunk, and eight PNG-backed icon entries
                ; (ic07..ic14); per-size payloads come from width-matched
                ; append_images entries or a default-resample resize of
                ; the base image (the native codec writes the container
                ; and PNG payloads in the DLL).
                images := [this]
                if IsSet(saveOptions) {
                    appendOption := Pillow.Image.SaveOption(saveOptions, "AppendImages", "append_images")
                    if appendOption.Set {
                        appendImages := appendOption.Value
                        if IsObject(appendImages) && appendImages is Pillow.Image {
                            images.Push(appendImages)
                        } else if IsObject(appendImages) {
                            for image in appendImages {
                                if !(IsObject(image) && image is Pillow.Image)
                                    throw Error("Pillow.Image.Save append_images expects Pillow.Image values", -1)
                                images.Push(image)
                            }
                        } else {
                            throw Error("Pillow.Image.Save append_images expects an image or image array", -1)
                        }
                    }
                }
                pathBytes := Pillow.Image.Utf8Buffer(path)
                status := 0
                if images.Length = 1 {
                    status := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_icns",
                        "Ptr", this.RequireHandle(),
                        "Ptr", pathBytes,
                        "Int"
                    )
                } else {
                    handles := Pillow.Image.HandleArray(images)
                    status := DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_icns_frames",
                        "Ptr", handles,
                        "Int", images.Length,
                        "Ptr", pathBytes,
                        "Int"
                    )
                }
                if status = -26 {
                    ; Pillow's PNG encoder rejects F with this exact
                    ; message; the remaining unsupported modes (1/CMYK/
                    ; I/I;16) are documented boundary children because
                    ; our PNG payload encoder covers L/P/RGB/LA/RGBA.
                    if this.Mode = "F"
                        throw Error("cannot write mode F as PNG", -1)
                    throw Error("Pillow.Image.Save ICNS mode " this.Mode " is not supported", -1)
                }
                Pillow.CheckStatus(status)
                return
            }
            if resolvedFormat = "EPS" {
                ; BEHAV-EPS-001: Pillow's pure-Python EPS writer covers
                ; L/RGB/CMYK with the exact DSC header and 39-byte hex
                ; lines; every other mode raises "image mode is not
                ; supported" (the native codec writes the file).
                pathBytes := Pillow.Image.Utf8Buffer(path)
                status := DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_eps",
                    "Ptr", this.RequireHandle(),
                    "Ptr", pathBytes,
                    "Int"
                )
                if status = -27
                    throw Error("image mode is not supported", -1)
                Pillow.CheckStatus(status)
                return
            }
            if resolvedFormat = "MPO" {
                ; BEHAV-MPO-001: Pillow's MpoImagePlugin writes a plain
                ; JPEG for a single image; with append_images it writes a
                ; first JPEG whose APP2 "MPF\0" marker carries a
                ; placeholder MP Index IFD (overwritten in place at file
                ; offset 28) plus the appended frames' JPEGs. L/RGB/CMYK
                ; save (mode 1 is JPEG-encoded as grayscale via Pillow's
                ; RAWMODE map) and every other mode raises
                ; "cannot write mode X as JPEG". The facade composes the
                ; container from native JPEG payloads (no pixel loops).
                images := [this]
                if IsSet(saveOptions) {
                    appendOption := Pillow.Image.SaveOption(saveOptions, "AppendImages", "append_images")
                    if appendOption.Set {
                        appendImages := appendOption.Value
                        if IsObject(appendImages) && appendImages is Pillow.Image {
                            images.Push(appendImages)
                        } else if IsObject(appendImages) {
                            for image in appendImages {
                                if !(IsObject(image) && image is Pillow.Image)
                                    throw Error("Pillow.Image.Save append_images expects Pillow.Image values", -1)
                                images.Push(image)
                            }
                        } else {
                            throw Error("Pillow.Image.Save append_images expects an image or image array", -1)
                        }
                    }
                }
                converted := []
                owned := []
                tempPaths := []
                try {
                    for image in images {
                        if image.Mode = "1" {
                            gray := image.Convert("L")
                            owned.Push(gray)
                            converted.Push(gray)
                        } else if image.Mode = "L" || image.Mode = "RGB" || image.Mode = "CMYK" {
                            converted.Push(image)
                        } else {
                            throw Error("cannot write mode " image.Mode " as JPEG", -1)
                        }
                    }
                    pathBytes := Pillow.Image.Utf8Buffer(path)
                    if converted.Length = 1 {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_save_jpeg",
                            "Ptr", converted[1].RequireHandle(),
                            "Ptr", pathBytes,
                            "Int"
                        ))
                        return
                    }
                    total := converted.Length
                    ifdLength := 66 + 16 * total
                    extra := Buffer(8 + ifdLength, 0)
                    NumPut("UChar", 0xFF, extra, 0)
                    NumPut("UChar", 0xE2, extra, 1)
                    NumPut("UChar", ((6 + ifdLength) >> 8) & 0xFF, extra, 2)
                    NumPut("UChar", (6 + ifdLength) & 0xFF, extra, 3)
                    NumPut("UChar", Ord("M"), extra, 4)
                    NumPut("UChar", Ord("P"), extra, 5)
                    NumPut("UChar", Ord("F"), extra, 6)
                    NumPut("UChar", 0, extra, 7)
                    loop ifdLength
                        NumPut("UChar", 32, extra, 7 + A_Index)

                    frameBytes := []
                    frame1Path := A_Temp "\pillow-ahk-mpo-" A_TickCount "-" Random(1, 1000000) "-0.jpg"
                    tempPaths.Push(frame1Path)
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_extra_options",
                        "Ptr", converted[1].RequireHandle(),
                        "Ptr", Pillow.Image.Utf8Buffer(frame1Path),
                        "Int", 75,
                        "Int", 0,
                        "Double", 0.0,
                        "Double", 0.0,
                        "Int", -1,
                        "Int", -1,
                        "Int", -1,
                        "Ptr", extra,
                        "UPtr", extra.Size,
                        "Int"
                    ))
                    frameBytes.Push(Pillow.Image.ReadAllBytes(frame1Path))
                    loop converted.Length - 1 {
                        framePath := A_Temp "\pillow-ahk-mpo-" A_TickCount "-" Random(1, 1000000) "-" A_Index ".jpg"
                        tempPaths.Push(framePath)
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_save_jpeg",
                            "Ptr", converted[A_Index + 1].RequireHandle(),
                            "Ptr", Pillow.Image.Utf8Buffer(framePath),
                            "Int"
                        ))
                        frameBytes.Push(Pillow.Image.ReadAllBytes(framePath))
                    }

                    ; MP Index IFD (ImageFileDirectory_v2.tobytes(8) layout)
                    mpentries := Buffer(16 * total, 0)
                    dataOffset := 0
                    loop total {
                        i := A_Index - 1
                        NumPut("UInt", i = 0 ? 0x030000 : 0, mpentries, i * 16)
                        NumPut("UInt", frameBytes[A_Index].Size, mpentries, i * 16 + 4)
                        NumPut("UInt", dataOffset, mpentries, i * 16 + 8)
                        NumPut("UShort", 0, mpentries, i * 16 + 12)
                        NumPut("UShort", 0, mpentries, i * 16 + 14)
                        if i = 0
                            dataOffset -= 28
                        dataOffset += frameBytes[A_Index].Size
                    }
                    ifd := Buffer(2 + 3 * 12 + 4 + mpentries.Size, 0)
                    offset := 0
                    NumPut("UShort", 3, ifd, offset)
                    offset += 2
                    NumPut("UShort", 0xB000, ifd, offset)
                    NumPut("UShort", 4, ifd, offset + 2)
                    NumPut("UInt", 1, ifd, offset + 4)
                    NumPut("UChar", Ord("0"), ifd, offset + 8)
                    NumPut("UChar", Ord("1"), ifd, offset + 9)
                    NumPut("UChar", Ord("0"), ifd, offset + 10)
                    NumPut("UChar", Ord("0"), ifd, offset + 11)
                    offset += 12
                    NumPut("UShort", 0xB001, ifd, offset)
                    NumPut("UShort", 4, ifd, offset + 2)
                    NumPut("UInt", 1, ifd, offset + 4)
                    NumPut("UInt", total, ifd, offset + 8)
                    offset += 12
                    NumPut("UShort", 0xB002, ifd, offset)
                    NumPut("UShort", 7, ifd, offset + 2)
                    NumPut("UInt", mpentries.Size, ifd, offset + 4)
                    NumPut("UInt", 50, ifd, offset + 8)
                    offset += 12
                    NumPut("UInt", 0, ifd, offset)
                    offset += 4
                    DllCall("msvcrt\memcpy", "Ptr", ifd.Ptr + offset, "Ptr", mpentries.Ptr, "UPtr", mpentries.Size, "CDecl Ptr")

                    totalSize := 0
                    for frame in frameBytes
                        totalSize += frame.Size
                    out := Buffer(totalSize, 0)
                    offset := 0
                    for frame in frameBytes {
                        DllCall("msvcrt\memcpy", "Ptr", out.Ptr + offset, "Ptr", frame.Ptr, "UPtr", frame.Size, "CDecl Ptr")
                        offset += frame.Size
                    }
                    ; patch the MP Index IFD over the placeholder
                    NumPut("UChar", 0x49, out, 28)
                    NumPut("UChar", 0x49, out, 29)
                    NumPut("UChar", 0x2A, out, 30)
                    NumPut("UChar", 0, out, 31)
                    NumPut("UInt", 8, out, 32)
                    DllCall("msvcrt\memcpy", "Ptr", out.Ptr + 36, "Ptr", ifd.Ptr, "UPtr", ifd.Size, "CDecl Ptr")
                    file := FileOpen(path, "w")
                    try
                        file.RawWrite(out, out.Size)
                    finally
                        file.Close()
                } finally {
                    for image in owned
                        image.Close()
                    for tempPath in tempPaths
                        FileDelete(tempPath)
                }
                return
            }
            if resolvedFormat = "HDF5" || resolvedFormat = "BUFR" || resolvedFormat = "GRIB" || resolvedFormat = "WMF" {
                ; BEHAV-OPEN-001/009: the stub plugins register a save
                ; whose handler is never installed — Pillow raises the
                ; exact OSError before writing anything.
                throw Error(resolvedFormat " save handler not installed", -1)
            }
            if resolvedFormat = "DCX" || resolvedFormat = "PIXAR" || resolvedFormat = "XVTHUMB" || resolvedFormat = "IMT" || resolvedFormat = "FTEX" || resolvedFormat = "SUN" || resolvedFormat = "GBR" || resolvedFormat = "FITS" || resolvedFormat = "XPM" || resolvedFormat = "IPTC" || resolvedFormat = "MCIDAS" || resolvedFormat = "PSD" || resolvedFormat = "FLI" || resolvedFormat = "MIC" || resolvedFormat = "PCD" || resolvedFormat = "MPEG" {
                ; BEHAV-OPEN-001/002/003/004/005/006/007/008: these plugins
                ; register no save handler at all — Pillow raises
                ; KeyError with the bare name.
                throw Error("'" resolvedFormat "'", -1)
            }
            if resolvedFormat = "PDF" {
                ; BEHAV-PDF-001: Pillow's PdfImagePlugin is save-only —
                ; a P image writes a byte-exact ASCIIHexDecode stream
                ; with the Indexed DeviceRGB palette, L/RGB/CMYK pages
                ; embed a DCTDecode JPEG payload (structure-exact here;
                ; WIC JPEG vs Pillow's libjpeg), and the UTC timestamps
                ; plus the path-stem UTF-16BE title go into the Info
                ; object. LA/RGBA (and P with a transparency entry)
                ; require JPEG2000 and mode 1 requires CCITT Group 4 —
                ; Pillow performs both locally, so those are documented
                ; runtime boundaries (no Pillow error exists to match).
                ; Every other mode raises Pillow's exact ValueError.
                images := [this]
                if IsSet(saveOptions) {
                    saveAllOption := Pillow.Image.SaveOption(saveOptions, "SaveAll", "save_all")
                    appendOption := Pillow.Image.SaveOption(saveOptions, "AppendImages", "append_images")
                    if saveAllOption.Set && saveAllOption.Value && appendOption.Set {
                        ; Pillow only appends when save_all is truthy;
                        ; each appended image contributes one page (the
                        ; n_frames>1 first-image expansion stays a
                        ; documented child).
                        appendImages := appendOption.Value
                        if IsObject(appendImages) && appendImages is Pillow.Image {
                            images.Push(appendImages)
                        } else if IsObject(appendImages) {
                            for image in appendImages {
                                if !(IsObject(image) && image is Pillow.Image)
                                    throw Error("Pillow.Image.Save append_images expects Pillow.Image values", -1)
                                images.Push(image)
                            }
                        } else {
                            throw Error("Pillow.Image.Save append_images expects an image or image array", -1)
                        }
                    }
                }
                for image in images {
                    if image.Mode = "LA" || image.Mode = "RGBA" || (image.Mode = "P" && image.Info.Has("transparency"))
                        throw Error("PDF save of mode " image.Mode " requires JPEG2000 support, which this runtime does not ship", -1)
                    if image.Mode = "1"
                        throw Error("PDF save of mode 1 requires CCITT Group 4 compression, which this runtime does not ship", -1)
                    if !(image.Mode = "P" || image.Mode = "L" || image.Mode = "RGB" || image.Mode = "CMYK")
                        throw Error("cannot save mode " image.Mode, -1)
                }
                xResolution := 72.0
                yResolution := 72.0
                if IsSet(saveOptions) {
                    dpiOption := Pillow.Image.SaveOption(saveOptions, "Dpi", "dpi")
                    dpiEmpty := dpiOption.Set && ((dpiOption.Value is String && StrLen(dpiOption.Value) = 0) || (dpiOption.Value is Array && dpiOption.Value.Length = 0))
                    if dpiOption.Set && !dpiEmpty {
                        dpiPair := Pillow.Image.SaveDpiPair(dpiOption.Value, false)
                        xResolution := dpiPair[1]
                        yResolution := dpiPair[2]
                    } else if !dpiOption.Set {
                        resolutionOption := Pillow.Image.SaveOption(saveOptions, "Resolution", "resolution")
                        if resolutionOption.Set {
                            if !(resolutionOption.Value is Number)
                                throw Error("Pillow.Image.Save resolution expects a number", -1)
                            xResolution := resolutionOption.Value + 0.0
                            yResolution := xResolution
                        }
                    }
                }
                if xResolution = 0 || yResolution = 0
                    throw Error("float division by zero", -1)
                pathBytes := Pillow.Image.Utf8Buffer(path)
                infoOptions := IsSet(saveOptions) ? saveOptions : 0
                titleBuffer := Pillow.Image.SavePdfInfoString(infoOptions, "Title", "title")
                authorBuffer := Pillow.Image.SavePdfInfoString(infoOptions, "Author", "author")
                subjectBuffer := Pillow.Image.SavePdfInfoString(infoOptions, "Subject", "subject")
                keywordsBuffer := Pillow.Image.SavePdfInfoString(infoOptions, "Keywords", "keywords")
                creatorBuffer := Pillow.Image.SavePdfInfoString(infoOptions, "Creator", "creator")
                producerBuffer := Pillow.Image.SavePdfInfoString(infoOptions, "Producer", "producer")
                creationDateBuffer := Pillow.Image.SavePdfInfoString(infoOptions, "CreationDate", "creationDate")
                modDateBuffer := Pillow.Image.SavePdfInfoString(infoOptions, "ModDate", "modDate")
                if images.Length = 1 {
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_pdf",
                        "Ptr", images[1].RequireHandle(),
                        "Ptr", pathBytes,
                        "Double", xResolution,
                        "Double", yResolution,
                        "Ptr", titleBuffer,
                        "Ptr", authorBuffer,
                        "Ptr", subjectBuffer,
                        "Ptr", keywordsBuffer,
                        "Ptr", creatorBuffer,
                        "Ptr", producerBuffer,
                        "Ptr", creationDateBuffer,
                        "Ptr", modDateBuffer,
                        "Int"
                    ))
                } else {
                    handles := Pillow.Image.HandleArray(images)
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_pdf_frames",
                        "Ptr", handles,
                        "Int", images.Length,
                        "Ptr", pathBytes,
                        "Double", xResolution,
                        "Double", yResolution,
                        "Ptr", titleBuffer,
                        "Ptr", authorBuffer,
                        "Ptr", subjectBuffer,
                        "Ptr", keywordsBuffer,
                        "Ptr", creatorBuffer,
                        "Ptr", producerBuffer,
                        "Ptr", creationDateBuffer,
                        "Ptr", modDateBuffer,
                        "Int"
                    ))
                }
                return
            }
            if IsSet(saveOptions) && resolvedFormat = "CUR" {
                ; Pillow 11.3.0 registers no CUR save; this is a standards
                ; extension using the ICO container with hotspot fields.
                pathBytes := Pillow.Image.Utf8Buffer(path)
                hotspotOption := Pillow.Image.SaveOption(saveOptions, "Hotspot", "hotspot")
                hasHotspot := 0
                hotspotX := 0
                hotspotY := 0
                if hotspotOption.Set {
                    hotspot := Pillow.Image.SaveHotspotPair(hotspotOption.Value)
                    hasHotspot := 1
                    hotspotX := hotspot[1]
                    hotspotY := hotspot[2]
                }
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_cur_options",
                    "Ptr", this.RequireHandle(),
                    "Ptr", pathBytes,
                    "Int", hasHotspot,
                    "Int", hotspotX,
                    "Int", hotspotY,
                    "Int"
                ))
                return
            }
            if IsSet(saveOptions) && resolvedFormat = "TIFF" {
                tiffInfoOption := Pillow.Image.SaveOption(saveOptions, "TiffInfo", "tiffinfo")
                iccProfileOption := Pillow.Image.SaveOption(saveOptions, "IccProfile", "icc_profile")
                dpiOption := Pillow.Image.SaveOption(saveOptions, "Dpi", "dpi")
                exifOption := Pillow.Image.SaveOption(saveOptions, "Exif", "exif")
                hasXmpTiffInfo := tiffInfoOption.Set
                    && tiffInfoOption.Value is Map
                    && tiffInfoOption.Value.Has(700)
                hasDescriptionTiffInfo := tiffInfoOption.Set
                    && tiffInfoOption.Value is Map
                    && tiffInfoOption.Value.Has(270)
                hasArtistTiffInfo := tiffInfoOption.Set
                    && tiffInfoOption.Value is Map
                    && tiffInfoOption.Value.Has(315)
                if (iccProfileOption.Set && (tiffInfoOption.Set || dpiOption.Set))
                    || (dpiOption.Set && (hasXmpTiffInfo || hasDescriptionTiffInfo || hasArtistTiffInfo))
                    || exifOption.Set
                    || Pillow.Image.SaveOptionBool(saveOptions, false, "BigTiff", "big_tiff") {
                    this.SaveTiffFrames(path, saveOptions)
                    return
                }
            }
            if IsSet(saveOptions) && Pillow.Image.SaveOptionBool(saveOptions, false, "SaveAll", "save_all") {
                if resolvedFormat = "GIF" {
                    this.SaveGifAnimation(path, saveOptions)
                } else if resolvedFormat = "TIFF" {
                    this.SaveTiffFrames(path, saveOptions)
                } else {
                    throw Error("Pillow.Image.Save save_all currently supports GIF and TIFF only", -1)
                }
                return
            }

            pathBytes := Pillow.Image.Utf8Buffer(path)
            if IsSet(saveOptions) && resolvedFormat = "GIF" {
                transparencyOption := Pillow.Image.SaveOption(saveOptions, "Transparency", "transparency")
                commentOption := Pillow.Image.SaveOption(saveOptions, "Comment", "comment")
                interlaceOption := Pillow.Image.SaveOption(saveOptions, "Interlace", "interlace")
                paletteOption := Pillow.Image.SaveOption(saveOptions, "Palette", "palette")
                if interlaceOption.Set || paletteOption.Set {
                    ; Pillow 11.3.0's interlace flag (0x40 + four-pass rows)
                    ; and the palette global-color-table override (the pixel
                    ; indices keep their meaning).
                    hasTransparency := 0
                    transparency := 0
                    if transparencyOption.Set {
                        if !(transparencyOption.Value is Integer)
                            throw Error("Pillow.Image.Save transparency must be an integer", -1)
                        hasTransparency := 1
                        transparency := transparencyOption.Value
                    }
                    interlace := 1
                    if interlaceOption.Set
                        interlace := interlaceOption.Value ? 1 : 0
                    ; Pillow's @PIL153 workaround disables interlace whenever
                    ; either side is shorter than 16 pixels (even explicit).
                    if Min(this.Width, this.Height) < 16
                        interlace := 0
                    paletteBytes := 0
                    paletteSize := 0
                    if paletteOption.Set {
                        paletteBytes := Pillow.Image.SaveTransparencyByteTable(paletteOption.Value)
                        paletteSize := paletteBytes.Size
                    }
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_gif_interlace_palette_options",
                        "Ptr", this.RequireHandle(),
                        "Ptr", pathBytes,
                        "Int", hasTransparency,
                        "Int", transparency,
                        "Int", interlace,
                        "Ptr", paletteBytes,
                        "UPtr", paletteSize,
                        "Int"
                    ))
                    return
                }
                if commentOption.Set {
                    commentState := Pillow.Image.SaveGifCommentBuffer(commentOption)
                    if transparencyOption.Set {
                        if !(transparencyOption.Value is Integer)
                            throw Error("Pillow.Image.Save transparency must be an integer", -1)
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_save_gif_comment_options",
                            "Ptr", this.RequireHandle(),
                            "Ptr", pathBytes,
                            "Int", 1,
                            "Int", transparencyOption.Value,
                            "Ptr", commentState.Buffer,
                            "UPtr", commentState.Size,
                            "Int"
                        ))
                        return
                    }
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_gif_comment",
                        "Ptr", this.RequireHandle(),
                        "Ptr", pathBytes,
                        "Ptr", commentState.Buffer,
                        "UPtr", commentState.Size,
                        "Int"
                    ))
                    return
                }
                if transparencyOption.Set {
                    if !(transparencyOption.Value is Integer)
                        throw Error("Pillow.Image.Save transparency must be an integer", -1)
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_gif_options",
                        "Ptr", this.RequireHandle(),
                        "Ptr", pathBytes,
                        "Int", 1,
                        "Int", transparencyOption.Value,
                        "Int"
                    ))
                    return
                }
            }

            if IsSet(saveOptions) && resolvedFormat = "TIFF" {
                dpiOption := Pillow.Image.SaveOption(saveOptions, "Dpi", "dpi")
                compressionOption := Pillow.Image.SaveOption(saveOptions, "Compression", "compression")
                resolutionOption := Pillow.Image.SaveOption(saveOptions, "Resolution", "resolution")
                resolutionUnitOption := Pillow.Image.SaveOption(saveOptions, "ResolutionUnit", "resolution_unit")
                stripSizeOption := Pillow.Image.SaveOption(saveOptions, "StripSize", "strip_size")
                descriptionOption := Pillow.Image.SaveOption(saveOptions, "Description", "description")
                softwareOption := Pillow.Image.SaveOption(saveOptions, "Software", "software")
                artistOption := Pillow.Image.SaveOption(saveOptions, "Artist", "artist")
                copyrightOption := Pillow.Image.SaveOption(saveOptions, "Copyright", "copyright")
                dateTimeOption := Pillow.Image.SaveOption(saveOptions, "DateTime", "date_time")
                xResolutionOption := Pillow.Image.SaveOption(saveOptions, "XResolution", "x_resolution")
                yResolutionOption := Pillow.Image.SaveOption(saveOptions, "YResolution", "y_resolution")
                iccProfileOption := Pillow.Image.SaveOption(saveOptions, "IccProfile", "icc_profile")
                exifOption := Pillow.Image.SaveOption(saveOptions, "Exif", "exif")
                tiffInfoOption := Pillow.Image.SaveOption(saveOptions, "TiffInfo", "tiffinfo")
                if tiffInfoOption.Set && tiffInfoOption.Value is Map {
                    ; BEHAV-SAVEOPTS-006: Pillow's tiffinfo arbitrary tags.
                    ; Tags outside the legacy 700 surface (including the
                    ; 270/315 ASCII tags) are classified with
                    ; ImageFileDirectory_v2's type inference and patched
                    ; into IFD0 after the plain save.  Bounded:
                    ; composition with icc/dpi/exif/big_tiff/resolution/
                    ; named kwargs still routes through the older paths,
                    ; which ignore the arbitrary tags, and a 700-only map
                    ; without icc/dpi keeps the legacy route.
                    hasNonXmpTags := false
                    for tag in tiffInfoOption.Value {
                        if tag != 700
                            hasNonXmpTags := true
                    }
                    if hasNonXmpTags
                        && !iccProfileOption.Set && !dpiOption.Set && !exifOption.Set
                        && !compressionOption.Set && !resolutionOption.Set && !resolutionUnitOption.Set
                        && !descriptionOption.Set && !softwareOption.Set && !artistOption.Set
                        && !copyrightOption.Set && !dateTimeOption.Set
                        && !xResolutionOption.Set && !yResolutionOption.Set
                        && !Pillow.Image.SaveOptionBool(saveOptions, false, "BigTiff", "big_tiff") {
                        entries := Pillow.Image.SaveTiffInfoEntries(tiffInfoOption.Value)
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_save_tiff",
                            "Ptr", this.RequireHandle(),
                            "Ptr", pathBytes,
                            "Int"
                        ))
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_patch_tiff_raw_entries",
                            "Ptr", pathBytes,
                            "Ptr", entries.Tags,
                            "Ptr", entries.Types,
                            "Ptr", entries.Counts,
                            "Ptr", entries.Offsets,
                            "Ptr", entries.Values,
                            "UPtr", entries.ValuesSize,
                            "UPtr", entries.EntryCount,
                            "Int"
                        ))
                        return
                    }
                }
                if descriptionOption.Set || softwareOption.Set || artistOption.Set || copyrightOption.Set
                    || dateTimeOption.Set || xResolutionOption.Set || yResolutionOption.Set {
                    ; Pillow 11.3.0 TiffImagePlugin._save: the named ASCII
                    ; kwargs (description -> 270, software -> 305, date_time
                    ; -> 306, artist -> 315, copyright -> 33432) compose with
                    ; the resolution surface. resolution sets BOTH axes,
                    ; x_resolution/y_resolution overwrite their own axis, and
                    ; a truthy dpi pair overwrites all of it plus forces
                    ; ResolutionUnit=2. strip_size stays accepted-and-ignored.
                    hasX := 0
                    hasY := 0
                    resX := 0.0
                    resY := 0.0
                    if resolutionOption.Set {
                        first := IsObject(resolutionOption.Value) ? resolutionOption.Value[1] : resolutionOption.Value
                        if !(first is Number)
                            throw Error("bad operand type for abs(): 'str'", -1)
                        if first < 0
                            throw Error("argument out of range", -1)
                        hasX := 1
                        hasY := 1
                        resX := first
                        resY := first
                    }
                    if xResolutionOption.Set {
                        xFirst := IsObject(xResolutionOption.Value) ? xResolutionOption.Value[1] : xResolutionOption.Value
                        if !(xFirst is Number)
                            throw Error("bad operand type for abs(): 'str'", -1)
                        if xFirst < 0
                            throw Error("argument out of range", -1)
                        hasX := 1
                        resX := xFirst
                    }
                    if yResolutionOption.Set {
                        yFirst := IsObject(yResolutionOption.Value) ? yResolutionOption.Value[1] : yResolutionOption.Value
                        if !(yFirst is Number)
                            throw Error("bad operand type for abs(): 'str'", -1)
                        if yFirst < 0
                            throw Error("argument out of range", -1)
                        hasY := 1
                        resY := yFirst
                    }
                    hasUnit := 0
                    resolutionUnit := 0
                    if resolutionUnitOption.Set {
                        if !(resolutionUnitOption.Value is Integer)
                            throw Error("required argument is not an integer", -1)
                        if resolutionUnitOption.Value < 0 || resolutionUnitOption.Value > 65535
                            throw Error("ushort format requires 0 <= number <= 0xffff", -1)
                        hasUnit := 1
                        resolutionUnit := resolutionUnitOption.Value
                    }
                    if dpiOption.Set {
                        dpi := Pillow.Image.SaveDpiPair(dpiOption.Value, false)
                        if dpi[1] != 0 || dpi[2] != 0 {
                            ; Pillow: "if dpi:" — a truthy pair overwrites the
                            ; resolution surface and forces ResolutionUnit=2;
                            ; a negative value hits the exact struct.error.
                            if dpi[1] < 0 || dpi[2] < 0
                                throw Error("argument out of range", -1)
                            hasX := 1
                            hasY := 1
                            resX := dpi[1]
                            resY := dpi[2]
                            hasUnit := 1
                            resolutionUnit := 2
                        }
                    }
                    compression := 1
                    if compressionOption.Set {
                        ; Pillow validates quality only on the jpeg
                        ; compression route (the exact ValueError); with any
                        ; other compression the option is ignored.
                        qualityOption := Pillow.Image.SaveOption(saveOptions, "Quality", "quality")
                        if qualityOption.Set {
                            compressionText := StrLower(String(compressionOption.Value))
                            if compressionText = "jpeg" || compressionText = "tiff_jpeg" {
                                if !(qualityOption.Value is Integer) || qualityOption.Value < 0 || qualityOption.Value > 100
                                    throw Error("Invalid quality setting", -1)
                            }
                        }
                        compression := Pillow.Image.SaveTiffCompression(compressionOption.Value)
                    }
                    iccProfile := 0
                    iccProfileOption := Pillow.Image.SaveOption(saveOptions, "IccProfile", "icc_profile")
                    if iccProfileOption.Set {
                        iccProfile := Pillow.Image.BinaryBuffer(
                            iccProfileOption.Value,
                            "Pillow.Image.Save icc_profile"
                        )
                        if iccProfile.Size = 0
                            throw Error("Pillow.Image.Save icc_profile must not be empty", -1)
                    }
                    asciiTags := []
                    asciiValues := []
                    if descriptionOption.Set {
                        asciiTags.Push(270)
                        asciiValues.Push(Pillow.Image.SaveTiffAsciiNamedValue(descriptionOption.Value))
                    }
                    if softwareOption.Set {
                        asciiTags.Push(305)
                        asciiValues.Push(Pillow.Image.SaveTiffAsciiNamedValue(softwareOption.Value))
                    }
                    if dateTimeOption.Set {
                        asciiTags.Push(306)
                        asciiValues.Push(Pillow.Image.SaveTiffAsciiNamedValue(dateTimeOption.Value))
                    }
                    if artistOption.Set {
                        asciiTags.Push(315)
                        asciiValues.Push(Pillow.Image.SaveTiffAsciiNamedValue(artistOption.Value))
                    }
                    if copyrightOption.Set {
                        asciiTags.Push(33432)
                        asciiValues.Push(Pillow.Image.SaveTiffAsciiNamedValue(copyrightOption.Value))
                    }
                    asciiTagBuffer := Buffer(asciiTags.Length * 4, 0)
                    asciiValuePointers := Buffer(asciiTags.Length * A_PtrSize, 0)
                    asciiValueSizes := Buffer(asciiTags.Length * A_PtrSize, 0)
                    for index, tag in asciiTags {
                        NumPut("Int", tag, asciiTagBuffer, (index - 1) * 4)
                        NumPut("Ptr", asciiValues[index].Ptr, asciiValuePointers, (index - 1) * A_PtrSize)
                        NumPut("UPtr", asciiValues[index].Size, asciiValueSizes, (index - 1) * A_PtrSize)
                    }
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_tiff_named_options",
                        "Ptr", this.RequireHandle(),
                        "Ptr", pathBytes,
                        "Int", hasX,
                        "Double", resX,
                        "Int", hasY,
                        "Double", resY,
                        "Int", hasUnit,
                        "Int", resolutionUnit,
                        "Int", compression,
                        "Ptr", IsObject(iccProfile) ? iccProfile : 0,
                        "UPtr", IsObject(iccProfile) ? iccProfile.Size : 0,
                        "Ptr", asciiTagBuffer,
                        "Ptr", asciiValuePointers,
                        "Ptr", asciiValueSizes,
                        "UPtr", asciiTags.Length,
                        "Int"
                    ))
                    return
                }
                if resolutionOption.Set || resolutionUnitOption.Set {
                    ; Pillow 11.3.0's resolution/resolution_unit: the scalar
                    ; (a pair truncates to its first value for BOTH axes,
                    ; matching the "too many entries" libtiff warning) writes
                    ; X/YResolution with NO ResolutionUnit tag unless
                    ; resolution_unit is given; resolution_unit alone writes
                    ; tag 296. strip_size is accepted and ignored by Pillow
                    ; (the strip writer always packs the full image).
                    hasResolution := 0
                    resolutionX := 0.0
                    if resolutionOption.Set {
                        first := IsObject(resolutionOption.Value) ? resolutionOption.Value[1] : resolutionOption.Value
                        if !(first is Number)
                            throw Error("bad operand type for abs(): 'str'", -1)
                        if first < 0
                            throw Error("argument out of range", -1)
                        hasResolution := 1
                        resolutionX := first
                    }
                    hasUnit := 0
                    resolutionUnit := 0
                    if resolutionUnitOption.Set {
                        if !(resolutionUnitOption.Value is Integer)
                            throw Error("required argument is not an integer", -1)
                        if resolutionUnitOption.Value < 0 || resolutionUnitOption.Value > 65535
                            throw Error("ushort format requires 0 <= number <= 0xffff", -1)
                        hasUnit := 1
                        resolutionUnit := resolutionUnitOption.Value
                    }
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_tiff_resolution_options",
                        "Ptr", this.RequireHandle(),
                        "Ptr", pathBytes,
                        "Int", hasResolution,
                        "Double", resolutionX,
                        "Double", resolutionX,
                        "Int", hasUnit,
                        "Int", resolutionUnit,
                        "Int"
                    ))
                    return
                }
                if dpiOption.Set || compressionOption.Set {
                    hasDpi := 0
                    dpiX := 0.0
                    dpiY := 0.0
                    if dpiOption.Set {
                        dpi := Pillow.Image.SaveDpiPair(dpiOption.Value)
                        hasDpi := 1
                        dpiX := dpi[1]
                        dpiY := dpi[2]
                    }
                    if compressionOption.Set {
                        ; Pillow validates quality only on the jpeg
                        ; compression route (the exact ValueError); with any
                        ; other compression the option is ignored.
                        qualityOption := Pillow.Image.SaveOption(saveOptions, "Quality", "quality")
                        if qualityOption.Set {
                            compressionText := StrLower(String(compressionOption.Value))
                            if compressionText = "jpeg" || compressionText = "tiff_jpeg" {
                                if !(qualityOption.Value is Integer) || qualityOption.Value < 0 || qualityOption.Value > 100
                                    throw Error("Invalid quality setting", -1)
                            }
                        }
                        compression := Pillow.Image.SaveTiffCompression(compressionOption.Value)
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_save_tiff_compression_options",
                            "Ptr", this.RequireHandle(),
                            "Ptr", pathBytes,
                            "Int", hasDpi,
                            "Double", dpiX,
                            "Double", dpiY,
                            "Int", compression,
                            "Int"
                        ))
                        return
                    }
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_tiff_options",
                        "Ptr", this.RequireHandle(),
                        "Ptr", pathBytes,
                        "Int", hasDpi,
                        "Double", dpiX,
                        "Double", dpiY,
                        "Int"
                    ))
                    return
                }
            }

            if IsSet(saveOptions) && resolvedFormat = "PNG" {
                compressLevelOption := Pillow.Image.SaveOption(saveOptions, "CompressLevel", "compress_level")
                compressTypeOption := Pillow.Image.SaveOption(saveOptions, "CompressType", "compress_type")
                bitsOption := Pillow.Image.SaveOption(saveOptions, "Bits", "bits")
                dpiOption := Pillow.Image.SaveOption(saveOptions, "Dpi", "dpi")
                transparencyOption := Pillow.Image.SaveOption(saveOptions, "Transparency", "transparency")
                pngInfoOption := Pillow.Image.SaveOption(saveOptions, "PngInfo", "pnginfo")
                iccProfileOption := Pillow.Image.SaveOption(saveOptions, "IccProfile", "icc_profile")
                exifOption := Pillow.Image.SaveOption(saveOptions, "Exif", "exif")
                interlaceOption := Pillow.Image.SaveOption(saveOptions, "Interlace", "interlace")
                gammaOption := Pillow.Image.SaveOption(saveOptions, "Gamma", "gamma")
                optimizeOption := Pillow.Image.SaveOption(saveOptions, "Optimize", "optimize")
                dictionaryOption := Pillow.Image.SaveOption(saveOptions, "Dictionary", "dictionary")
                if compressLevelOption.Set || compressTypeOption.Set || bitsOption.Set || dpiOption.Set || transparencyOption.Set || pngInfoOption.Set || iccProfileOption.Set || exifOption.Set || interlaceOption.Set || gammaOption.Set || optimizeOption.Set || dictionaryOption.Set {
                    ; Pillow's C encoder parses compress_type the same way
                    ; (the exact TypeError) and only the 0-4 range is
                    ; accepted-and-ignored; 5+ fails with the zlib codec
                    ; configuration OSError.
                    if compressTypeOption.Set {
                        if !(compressTypeOption.Value is Integer)
                            throw Error("'str' object cannot be interpreted as an integer", -1)
                        if compressTypeOption.Value < 0 || compressTypeOption.Value > 4
                            throw Error("codec configuration error when writing image file", -1)
                    }
                    if dictionaryOption.Set {
                        ; Pillow's zlib preset dictionary takes bytes-like
                        ; objects (the exact TypeError for str); the DLL
                        ; writes stored deflate blocks, so a dictionary is
                        ; accepted-and-ignored with no observable payload
                        ; effect (the documented no-compressor boundary).
                        if dictionaryOption.Value is String
                            throw Error("a bytes-like object is required, not 'str'", -1)
                    }
                    ; Pillow's bits override uses colors = min(1 << bits, 256)
                    ; with the exact shift errors, then picks 1/2/4/8 depth.
                    if bitsOption.Set {
                        if !(bitsOption.Value is Integer)
                            throw Error("unsupported operand type(s) for <<: 'int' and 'str'", -1)
                        if bitsOption.Value < 0
                            throw Error("negative shift count", -1)
                        if this.Mode = "P" {
                            pathBytes := Pillow.Image.Utf8Buffer(path)
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_png_bits",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", bitsOption.Value,
                                "Int"
                            ))
                            return
                        }
                    }
                    compressLevel := -1
                    if compressLevelOption.Set {
                        ; Pillow parses the value as a C int (the exact
                        ; TypeError for non-integers) and out-of-range
                        ; levels fail in zlib with the codec-configuration
                        ; OSError.
                        if !(compressLevelOption.Value is Integer)
                            throw Error("'str' object cannot be interpreted as an integer", -1)
                        if compressLevelOption.Value < 0 || compressLevelOption.Value > 9
                            throw Error("codec configuration error when writing image file", -1)
                        compressLevel := compressLevelOption.Value
                    }
                    dpiX := 0.0
                    dpiY := 0.0
                    if dpiOption.Set {
                        dpi := Pillow.Image.SaveDpiPair(dpiOption.Value)
                        dpiX := dpi[1]
                        dpiY := dpi[2]
                    }
                    hasTransparency := 0
                    transparency := 0
                    hasTransparencyTable := 0
                    transparencyTable := 0
                    hasRgbTransparency := 0
                    rgbTransparency := [0, 0, 0]
                    hasRgbTransparencyBytes := 0
                    rgbTransparencyBytes := 0
                    if transparencyOption.Set {
                        if transparencyOption.Value is Integer {
                            hasTransparency := 1
                            transparency := transparencyOption.Value
                        } else if IsObject(transparencyOption.Value) && Type(transparencyOption.Value) = "Buffer" {
                            if this.Mode = "P" {
                                hasTransparencyTable := 1
                                transparencyTable := Pillow.Image.SaveTransparencyByteTable(transparencyOption.Value)
                            } else if this.Mode = "RGB" && transparencyOption.Value.Size = 3 {
                                hasRgbTransparencyBytes := 1
                                rgbTransparencyBytes := Pillow.Image.SaveTransparencyByteTable(transparencyOption.Value)
                            } else {
                                throw Error("Pillow.Image.Save PNG byte transparency expects P mode or RGB with 3 bytes", -1)
                            }
                        } else {
                            hasRgbTransparency := 1
                            rgbTransparency := Pillow.Image.SaveTransparencyRgbTuple(transparencyOption.Value)
                        }
                    }
                    pngText := { Set: false }
                    pngTextCanUseIccExifRgbTransparency := false
                    if pngInfoOption.Set {
                        pngText := Pillow.Image.SavePngTextEntries(pngInfoOption.Value)
                        pngTextHasCustomChunk := pngText.HasOwnProp("HasCustomChunk") && pngText.HasCustomChunk
                        pngTextHasMultipleCustomChunks := pngText.HasOwnProp("HasCustomChunks") && pngText.HasCustomChunks
                        pngTextHasAdvancedText := pngText.HasOwnProp("HasCompressedText") && (pngText.HasCompressedText || pngText.HasITextText)
                        customChunkHasAdvancedText := pngTextHasCustomChunk && pngTextHasAdvancedText
                        pngTextCanUseRgbTransparency := (((pngText.Count > 0 && !pngTextHasCustomChunk) || (pngTextHasCustomChunk && pngText.Count > 0)) && hasRgbTransparency)
                        if pngText.Count > 0
                            && hasRgbTransparencyBytes
                            pngTextCanUseRgbTransparency := true
                        pngTextCanUseMultipleCustomChunksIccExifRgbTransparency := pngTextHasMultipleCustomChunks
                            && pngText.Count > 0
                            && pngTextHasAdvancedText
                            && iccProfileOption.Set
                            && exifOption.Set
                            && (hasRgbTransparency || hasRgbTransparencyBytes)
                            && (optimizeOption.Set && optimizeOption.Value)
                            && !interlaceOption.Set
                            && !gammaOption.Set
                        pngTextCanUseIccExifRgbTransparency := (pngTextHasCustomChunk
                                && pngText.Count > 0
                                && pngTextHasAdvancedText
                                && (hasRgbTransparency || hasRgbTransparencyBytes))
                            || (!pngTextHasCustomChunk
                                && !pngTextHasMultipleCustomChunks
                                && pngText.Count > 0
                                && iccProfileOption.Set
                                && exifOption.Set
                                && (hasRgbTransparency || hasRgbTransparencyBytes))
                            || pngTextCanUseMultipleCustomChunksIccExifRgbTransparency
                        if pngText.Set
                            && transparencyOption.Set
                            && !pngTextCanUseRgbTransparency
                            && !(pngText.HasCustomChunk && pngText.Count = 0 && (hasRgbTransparency || hasRgbTransparencyBytes))
                            throw Error("Pillow.Image.Save pnginfo with transparency is not supported", -1)
                    }
                    iccProfile := 0
                    if iccProfileOption.Set {
                        iccProfile := Pillow.Image.BinaryBuffer(iccProfileOption.Value, "Pillow.Image.Save icc_profile")
                        if iccProfile.Size = 0
                            throw Error("Pillow.Image.Save icc_profile must not be empty", -1)
                        if transparencyOption.Set {
                            if !(hasRgbTransparency || hasRgbTransparencyBytes)
                                throw Error("Pillow.Image.Save icc_profile with transparency is not supported", -1)
                            if pngText.Set && !pngTextCanUseIccExifRgbTransparency
                                throw Error("Pillow.Image.Save pnginfo with icc_profile and transparency is not supported", -1)
                        }
                    }
                    exif := 0
                    if exifOption.Set {
                        exif := Pillow.Image.BinaryBuffer(exifOption.Value, "Pillow.Image.Save exif")
                        if exif.Size = 0
                            throw Error("Pillow.Image.Save exif must not be empty", -1)
                        if transparencyOption.Set {
                            if !(hasRgbTransparency || hasRgbTransparencyBytes)
                                throw Error("Pillow.Image.Save exif with transparency is not supported", -1)
                            if pngText.Set && !pngTextCanUseIccExifRgbTransparency
                                throw Error("Pillow.Image.Save pnginfo with exif and transparency is not supported", -1)
                        }
                    }
                    if optimizeOption.Set && optimizeOption.Value {
                        optimizeAllowsIccExifRgbTransparency := pngTextCanUseIccExifRgbTransparency
                            && (iccProfileOption.Set || exifOption.Set)
                        if (transparencyOption.Set && !optimizeAllowsIccExifRgbTransparency) || interlaceOption.Set || gammaOption.Set
                            throw Error("Pillow.Image.Save optimize with this PNG option combination is not supported", -1)
                        if pngText.Set {
                            optimizeAllowsCustomTextKind := (pngText.HasCustomChunk
                                    && pngText.Count > 0
                                    && pngTextHasAdvancedText)
                                || (!pngTextHasCustomChunk
                                    && !pngTextHasMultipleCustomChunks
                                    && pngText.Count > 0
                                    && pngTextHasAdvancedText
                                    && pngTextCanUseIccExifRgbTransparency)
                                || pngTextCanUseMultipleCustomChunksIccExifRgbTransparency
                            if pngText.HasGama || (pngText.HasCustomChunk && !optimizeAllowsCustomTextKind)
                                throw Error("Pillow.Image.Save optimize with pnginfo chunks is not supported", -1)
                            if pngText.Count > 0 && (pngText.HasCompressedText || pngText.HasITextText) && !optimizeAllowsCustomTextKind
                                throw Error("Pillow.Image.Save optimize with compressed or iTXt pnginfo is not supported", -1)
                        }
                    }
                    if hasRgbTransparencyBytes {
                        rgbTransparency := [
                            NumGet(rgbTransparencyBytes, 0, "UChar"),
                            NumGet(rgbTransparencyBytes, 1, "UChar"),
                            NumGet(rgbTransparencyBytes, 2, "UChar"),
                        ]
                    }
                    if pngText.Set && pngText.HasGama {
                        if iccProfileOption.Set || exifOption.Set || transparencyOption.Set || interlaceOption.Set || gammaOption.Set
                            throw Error("Pillow.Image.Save pnginfo gAMA with other metadata or transparency options is not supported", -1)
                    }
                    if pngText.Set
                        && pngText.HasOwnProp("CustomChunkKind")
                        && (pngText.CustomChunkKind = "sRGB" || pngText.CustomChunkKind = "sBIT" || pngText.CustomChunkKind = "sPLT" || pngText.CustomChunkKind = "cICP" || pngText.CustomChunkKind = "bKGD" || pngText.CustomChunkKind = "hIST" || pngText.CustomChunkKind = "tIME") {
                        if pngText.Count > 0 || iccProfileOption.Set || exifOption.Set || transparencyOption.Set || interlaceOption.Set || gammaOption.Set || compressLevelOption.Set || dpiOption.Set || (optimizeOption.Set && optimizeOption.Value)
                            throw Error("Pillow.Image.Save pnginfo public chunk with other metadata or options is not supported", -1)
                    }
                    if pngText.Set && pngText.HasCustomChunk && pngText.Count = 0 {
                        if (transparencyOption.Set && !(hasRgbTransparency || hasRgbTransparencyBytes)) || interlaceOption.Set || gammaOption.Set
                            throw Error("Pillow.Image.Save pnginfo custom chunk with other metadata or transparency options is not supported", -1)
                        if iccProfileOption.Set && exifOption.Set
                            throw Error("Pillow.Image.Save pnginfo custom chunk with icc_profile and exif is not supported", -1)
                    }
                    if pngText.Set && pngText.Count > 0 {
                        if pngText.HasCustomChunk {
                            if interlaceOption.Set || gammaOption.Set || (transparencyOption.Set && !(hasRgbTransparency || hasRgbTransparencyBytes))
                                throw Error("Pillow.Image.Save pnginfo custom chunk with other metadata or transparency options is not supported", -1)
                            if pngTextHasAdvancedText && (transparencyOption.Set && !(hasRgbTransparency || hasRgbTransparencyBytes))
                                throw Error("Pillow.Image.Save pnginfo custom chunk with advanced text and other metadata is not supported", -1)
                        }
                    }
                    if pngText.Set && pngText.HasOwnProp("HasValueSizes") && pngText.HasValueSizes {
                        if pngText.HasITextText
                            throw Error("Pillow.Image.Save pnginfo bytes with embedded NUL currently supports only tEXt/zTXt entries", -1)
                        if pngText.HasCustomChunk || pngText.HasCustomChunks || iccProfileOption.Set || exifOption.Set || transparencyOption.Set || interlaceOption.Set || gammaOption.Set || (optimizeOption.Set && optimizeOption.Value)
                            throw Error("Pillow.Image.Save pnginfo bytes with embedded NUL and other PNG options is not supported", -1)
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_save_png_text_entries_value_sizes_options",
                            "Ptr", this.RequireHandle(),
                            "Ptr", pathBytes,
                            "Int", compressLevel,
                            "Double", dpiX,
                            "Double", dpiY,
                            "Ptr", pngText.KeyPtrs,
                            "Ptr", pngText.ValuePtrs,
                            "Ptr", pngText.ValueSizes,
                            "Ptr", pngText.Compressed,
                            "UPtr", pngText.Count,
                            "Int"
                        ))
                        return
                    }
                    if pngText.Set && pngText.HasOwnProp("HasCustomChunks") && pngText.HasCustomChunks {
                        pngTextMultipleCustomChunksSupportedText := !pngTextHasAdvancedText
                            || (iccProfileOption.Set && exifOption.Set)
                        pngTextCanUseMetadataCustomChunks := pngText.Count > 0
                            && pngTextMultipleCustomChunksSupportedText
                            && (iccProfileOption.Set || exifOption.Set)
                            && (!transparencyOption.Set || pngTextCanUseMultipleCustomChunksIccExifRgbTransparency)
                            && !interlaceOption.Set
                            && !gammaOption.Set
                            && (!(optimizeOption.Set && optimizeOption.Value) || pngTextCanUseMultipleCustomChunksIccExifRgbTransparency)
                        if (iccProfileOption.Set || exifOption.Set || transparencyOption.Set || interlaceOption.Set || gammaOption.Set || (optimizeOption.Set && optimizeOption.Value)) && !pngTextCanUseMetadataCustomChunks
                            throw Error("Pillow.Image.Save pnginfo multiple custom chunks with other metadata or options is not supported", -1)
                        if pngTextCanUseMetadataCustomChunks {
                            pngMetadataCustomChunkFlags := 0
                            if exifOption.Set
                                pngMetadataCustomChunkFlags |= 0x04
                            if optimizeOption.Set && optimizeOption.Value
                                pngMetadataCustomChunkFlags |= 0x08
                            if hasTransparency
                                pngMetadataCustomChunkFlags |= 0x10
                            if hasRgbTransparency || hasRgbTransparencyBytes
                                pngMetadataCustomChunkFlags |= 0x20
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_png_metadata_custom_chunks_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", compressLevel,
                                "Double", dpiX,
                                "Double", dpiY,
                                "Ptr", pngText.KeyPtrs,
                                "Ptr", pngText.ValuePtrs,
                                "Ptr", pngText.Kinds,
                                "Ptr", pngText.Compressed,
                                "Ptr", pngText.LangPtrs,
                                "Ptr", pngText.TKeyPtrs,
                                "UPtr", pngText.Count,
                                "Ptr", iccProfileOption.Set ? iccProfile : 0,
                                "UPtr", iccProfileOption.Set ? iccProfile.Size : 0,
                                "Ptr", exifOption.Set ? exif : 0,
                                "UPtr", exifOption.Set ? exif.Size : 0,
                                "Ptr", pngText.CustomChunkTypes,
                                "Ptr", pngText.CustomChunkDataPtrs,
                                "Ptr", pngText.CustomChunkDataSizes,
                                "Ptr", pngText.CustomChunkAfterIdatArray,
                                "UPtr", pngText.CustomChunkCount,
                                "UInt", pngMetadataCustomChunkFlags,
                                "UInt", 0,
                                "Int", transparency,
                                "Ptr", hasTransparencyTable ? transparencyTable : 0,
                                "UPtr", hasTransparencyTable ? transparencyTable.Size : 0,
                                "Int", rgbTransparency[1],
                                "Int", rgbTransparency[2],
                                "Int", rgbTransparency[3],
                                "Int"
                            ))
                            return
                        }
                        if pngText.Count > 0 {
                            if pngText.HasCompressedText || pngText.HasITextText {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_png_text_entries_custom_chunks_kind_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", compressLevel,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", pngText.KeyPtrs,
                                    "Ptr", pngText.ValuePtrs,
                                    "Ptr", pngText.Kinds,
                                    "Ptr", pngText.Compressed,
                                    "Ptr", pngText.LangPtrs,
                                    "Ptr", pngText.TKeyPtrs,
                                    "UPtr", pngText.Count,
                                    "Ptr", pngText.CustomChunkTypes,
                                    "Ptr", pngText.CustomChunkDataPtrs,
                                    "Ptr", pngText.CustomChunkDataSizes,
                                    "Ptr", pngText.CustomChunkAfterIdatArray,
                                    "UPtr", pngText.CustomChunkCount,
                                    "Int"
                                ))
                            } else {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_png_text_entries_custom_chunks_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", compressLevel,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", pngText.KeyPtrs,
                                    "Ptr", pngText.ValuePtrs,
                                    "UPtr", pngText.Count,
                                    "Ptr", pngText.CustomChunkTypes,
                                    "Ptr", pngText.CustomChunkDataPtrs,
                                    "Ptr", pngText.CustomChunkDataSizes,
                                    "Ptr", pngText.CustomChunkAfterIdatArray,
                                    "UPtr", pngText.CustomChunkCount,
                                    "Int"
                                ))
                            }
                            return
                        }
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_save_png_custom_chunks_options",
                            "Ptr", this.RequireHandle(),
                            "Ptr", pathBytes,
                            "Int", compressLevel,
                            "Double", dpiX,
                            "Double", dpiY,
                            "Ptr", pngText.CustomChunkTypes,
                            "Ptr", pngText.CustomChunkDataPtrs,
                            "Ptr", pngText.CustomChunkDataSizes,
                            "Ptr", pngText.CustomChunkAfterIdat,
                            "UPtr", pngText.CustomChunkCount,
                            "Int"
                        ))
                        return
                    }
                    if pngText.Set || iccProfileOption.Set || exifOption.Set || transparencyOption.Set || interlaceOption.Set || gammaOption.Set || (optimizeOption.Set && optimizeOption.Value) {
                        pngMetadataFlags := 0
                        if pngText.Set && pngText.HasGama
                            pngMetadataFlags |= 0x01
                        if pngText.Set && pngText.HasCustomChunk && iccProfileOption.Set
                            pngMetadataFlags |= 0x02
                        if exifOption.Set && pngText.Set && pngText.Count > 0
                            pngMetadataFlags |= 0x04
                        if optimizeOption.Set && optimizeOption.Value
                            pngMetadataFlags |= 0x08
                        if hasTransparency
                            pngMetadataFlags |= 0x10
                        if hasRgbTransparency || hasRgbTransparencyBytes
                            pngMetadataFlags |= 0x20
                        if pngText.Set && pngText.HasCustomChunk && pngText.CustomChunkAfterIdat
                            pngMetadataFlags |= 0x40
                        textCount := pngText.Set ? pngText.Count : 0
                        customChunkDataSize := (pngText.Set && pngText.HasCustomChunk) ? pngText.CustomChunkData.Size : 0
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_save_png_metadata_options",
                            "Ptr", this.RequireHandle(),
                            "Ptr", pathBytes,
                            "Int", compressLevel,
                            "Double", dpiX,
                            "Double", dpiY,
                            "Ptr", textCount ? pngText.KeyPtrs : 0,
                            "Ptr", textCount ? pngText.ValuePtrs : 0,
                            "Ptr", textCount ? pngText.Kinds : 0,
                            "Ptr", textCount ? pngText.Compressed : 0,
                            "Ptr", textCount ? pngText.LangPtrs : 0,
                            "Ptr", textCount ? pngText.TKeyPtrs : 0,
                            "UPtr", textCount,
                            "Ptr", iccProfileOption.Set ? iccProfile : 0,
                            "UPtr", iccProfileOption.Set ? iccProfile.Size : 0,
                            "Ptr", exifOption.Set ? exif : 0,
                            "UPtr", exifOption.Set ? exif.Size : 0,
                            "Ptr", (pngText.Set && pngText.HasCustomChunk) ? pngText.CustomChunkType : 0,
                            "Ptr", (pngText.Set && pngText.HasCustomChunk) ? pngText.CustomChunkData : 0,
                            "UPtr", customChunkDataSize,
                            "UInt", pngMetadataFlags,
                            "UInt", (pngText.Set && pngText.HasGama) ? pngText.GamaRaw : 0,
                            "Int", transparency,
                            "Ptr", hasTransparencyTable ? transparencyTable : 0,
                            "UPtr", hasTransparencyTable ? transparencyTable.Size : 0,
                            "Int", rgbTransparency[1],
                            "Int", rgbTransparency[2],
                            "Int", rgbTransparency[3],
                            "Int"
                        ))
                        return
                    }
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_png_options",
                        "Ptr", this.RequireHandle(),
                        "Ptr", pathBytes,
                        "Int", compressLevel,
                        "Double", dpiX,
                        "Double", dpiY,
                        "Int"
                    ))
                    return
                }
            }

            if IsSet(saveOptions) && resolvedFormat = "JPEG" {
                qualityOption := Pillow.Image.SaveOption(saveOptions, "Quality", "quality")
                dpiOption := Pillow.Image.SaveOption(saveOptions, "Dpi", "dpi")
                commentOption := Pillow.Image.SaveOption(saveOptions, "Comment", "comment")
                iccProfileOption := Pillow.Image.SaveOption(saveOptions, "IccProfile", "icc_profile")
                exifOption := Pillow.Image.SaveOption(saveOptions, "Exif", "exif")
                xmpOption := Pillow.Image.SaveOption(saveOptions, "Xmp", "xmp")
                extraOption := Pillow.Image.SaveOption(saveOptions, "Extra", "extra")
                subsamplingOption := Pillow.Image.SaveOption(saveOptions, "Subsampling", "subsampling")
                progressiveOption := Pillow.Image.SaveOption(saveOptions, "Progressive", "progressive")
                progressionOption := Pillow.Image.SaveOption(saveOptions, "Progression", "progression")
                optimizeOption := Pillow.Image.SaveOption(saveOptions, "Optimize", "optimize")
                keepRgbOption := Pillow.Image.SaveOption(saveOptions, "KeepRgb", "keep_rgb")
                qtablesOption := Pillow.Image.SaveOption(saveOptions, "QTables", "qtables")
                restartMarkerBlocksOption := Pillow.Image.SaveOption(saveOptions, "RestartMarkerBlocks", "restart_marker_blocks")
                restartMarkerRowsOption := Pillow.Image.SaveOption(saveOptions, "RestartMarkerRows", "restart_marker_rows")
                smoothOption := Pillow.Image.SaveOption(saveOptions, "Smooth", "smooth")
                streamTypeOption := Pillow.Image.SaveOption(saveOptions, "StreamType", "streamtype")
                if qualityOption.Set || dpiOption.Set || commentOption.Set || iccProfileOption.Set || exifOption.Set || xmpOption.Set || extraOption.Set || subsamplingOption.Set || progressiveOption.Set || progressionOption.Set || optimizeOption.Set || keepRgbOption.Set || qtablesOption.Set || restartMarkerBlocksOption.Set || restartMarkerRowsOption.Set || smoothOption.Set || streamTypeOption.Set {
                    quality := -1
                    qualityKeep := false
                    qualityPreset := 0
                    qualityPresetSet := false
                    if qualityOption.Set {
                        if qualityOption.Value is String {
                            qualityText := StrLower(qualityOption.Value)
                            if qualityText = "keep" {
                                qualityKeep := true
                                quality := -1
                            } else if qualityText = "web_low" || qualityText = "web_medium" || qualityText = "web_high" {
                                qualityPreset := Pillow.Image.SaveJpegQualityPreset(qualityText)
                                qualityPresetSet := true
                                quality := -1
                            } else {
                                throw Error("Invalid quality setting", -1)
                            }
                        } else {
                            if !(qualityOption.Value is Integer)
                                throw Error("Invalid quality setting", -1)
                            quality := qualityOption.Value
                        }
                    }
                    hasDpi := 0
                    dpiX := 0.0
                    dpiY := 0.0
                    if dpiOption.Set {
                        dpi := Pillow.Image.SaveDpiPair(dpiOption.Value, false)
                        hasDpi := 1
                        dpiX := dpi[1]
                        dpiY := dpi[2]
                    }
                    subsampling := -1
                    subsamplingKeep := false
                    if subsamplingOption.Set && !qualityKeep && !qualityPresetSet
                        subsampling := Pillow.Image.SaveJpegSubsampling(subsamplingOption.Value)
                    if subsampling = -2
                        subsamplingKeep := true
                    progressive := -1
                    if progressiveOption.Set || progressionOption.Set {
                        progressive := 0
                        if (progressiveOption.Set && progressiveOption.Value) || (progressionOption.Set && progressionOption.Value)
                            progressive := 1
                    }
                    optimize := -1
                    if optimizeOption.Set {
                        optimize := optimizeOption.Value ? 1 : 0
                    }
                    keepRgb := -1
                    if keepRgbOption.Set {
                        keepRgb := keepRgbOption.Value ? 1 : 0
                    }
                    qtables := 0
                    hasJpegQTables := false
                    qtablesKeep := false
                    qtablesPresetSet := false
                    if qtablesOption.Set && !qualityKeep && !qualityPresetSet {
                        if qtablesOption.Value is String {
                            qtablesText := StrLower(qtablesOption.Value)
                            if qtablesText = "keep" {
                                qtablesKeep := true
                            } else {
                                qtables := Pillow.Image.SaveJpegQualityPreset(qtablesText).QTables
                                hasJpegQTables := true
                                qtablesPresetSet := true
                            }
                        } else {
                            qtables := Pillow.Image.SaveJpegQTables(qtablesOption.Value)
                            hasJpegQTables := true
                        }
                    }
                    restartMarkerBlocks := 0
                    if restartMarkerBlocksOption.Set {
                        if !(restartMarkerBlocksOption.Value is Integer)
                            throw Error("Pillow.Image.Save restart_marker_blocks must be an integer", -1)
                        restartMarkerBlocks := restartMarkerBlocksOption.Value
                    }
                    restartMarkerRows := 0
                    if restartMarkerRowsOption.Set {
                        if !(restartMarkerRowsOption.Value is Integer)
                            throw Error("Pillow.Image.Save restart_marker_rows must be an integer", -1)
                        restartMarkerRows := restartMarkerRowsOption.Value
                    }
                    if qualityPresetSet {
                        qtables := qualityPreset.QTables
                        hasJpegQTables := true
                        qtablesKeep := false
                        qtablesPresetSet := false
                        if this.Mode = "L"
                            subsampling := -1
                        else
                            subsampling := qualityPreset.Subsampling
                        subsamplingKeep := false
                    }
                    if qualityKeep || qtablesKeep {
                        if !(this.Format = "JPEG" && (this.Mode = "CMYK" || this.Mode = "RGB" || this.Mode = "L"))
                            throw Error("Pillow.Image.Save JPEG keep currently supports opened L, RGB, or CMYK JPEG images", -1)
                    }
                    if qualityKeep {
                        qtables := Pillow.Image.NativeJpegQTables(this.RequireHandle())
                        hasJpegQTables := true
                        qtablesKeep := false
                        qtablesPresetSet := false
                        subsamplingKeep := false
                        subsampling := this.Mode = "CMYK"
                            ? -1
                            : Pillow.Image.NativeJpegSubsampling(this.RequireHandle())
                    } else if qtablesKeep {
                        if !hasJpegQTables {
                            qtables := Pillow.Image.NativeJpegQTables(this.RequireHandle())
                            hasJpegQTables := true
                        }
                        if !subsamplingOption.Set {
                            subsampling := -1
                        }
                    }
                    if subsamplingKeep {
                        if this.Format != "JPEG"
                            throw Error("Cannot use subsampling='keep' when original image is not a JPEG", -1)
                        nativeSubsampling := Pillow.Image.NativeJpegSubsampling(this.RequireHandle())
                        if nativeSubsampling < 0 && this.Mode != "CMYK"
                            throw Error("Pillow.Image.Save JPEG subsampling='keep' requires opened JPEG subsampling metadata", -1)
                        subsampling := this.Mode = "CMYK" ? -1 : nativeSubsampling
                    }
                    smoothOption := Pillow.Image.SaveOption(saveOptions, "Smooth", "smooth")
                    streamTypeOption := Pillow.Image.SaveOption(saveOptions, "StreamType", "streamtype")
                    if smoothOption.Set || streamTypeOption.Set {
                        ; Pillow 11.3.0: smooth feeds libjpeg's
                        ; smoothing_factor and streamtype selects the
                        ; abbreviated stream mode (1 = tables-only, 2 =
                        ; image-only).  The exact int-parse TypeErrors match
                        ; PyArg_ParseTuple("i"); any integer value is accepted
                        ; (no range validation), like the C encoder.
                        smooth := 0
                        streamType := 0
                        if smoothOption.Set {
                            if smoothOption.Value is String
                                throw Error("'str' object cannot be interpreted as an integer", -1)
                            if !(smoothOption.Value is Integer)
                                throw Error("'float' object cannot be interpreted as an integer", -1)
                            smooth := smoothOption.Value
                        }
                        if streamTypeOption.Set {
                            if streamTypeOption.Value is String
                                throw Error("'str' object cannot be interpreted as an integer", -1)
                            if !(streamTypeOption.Value is Integer)
                                throw Error("'float' object cannot be interpreted as an integer", -1)
                            streamType := streamTypeOption.Value
                        }
                        if !(qualityKeep || qualityPresetSet || qtablesKeep || qtablesPresetSet
                            || keepRgb = 1 || hasJpegQTables
                            || commentOption.Set || iccProfileOption.Set || exifOption.Set
                            || xmpOption.Set || extraOption.Set
                            || restartMarkerBlocksOption.Set || restartMarkerRowsOption.Set) {
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_smooth_streamtype_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", quality,
                                "Int", hasDpi,
                                "Double", dpiX,
                                "Double", dpiY,
                                "Int", subsampling,
                                "Int", progressive,
                                "Int", optimize,
                                "Int", smooth,
                                "Int", streamType,
                                "Int"
                            ))
                            return
                        }
                        ; otherwise fall through: the older keep/qtables/
                        ; metadata routes ignore smooth/streamtype (bounded
                        ; divergence, documented in the gap ledger).
                    }
                    if extraOption.Set {
                        if qualityKeep || qualityPresetSet || qtablesKeep || qtablesPresetSet
                            throw Error("Pillow.Image.Save JPEG extra currently supports explicit qtables and/or restart markers, but not quality='keep', JPEG quality presets, qtables='keep', or qtable presets", -1)
                        if !(this.Mode = "L" || this.Mode = "RGB" || this.Mode = "CMYK")
                            throw Error("Pillow.Image.Save JPEG extra currently supports mode L, RGB, or CMYK", -1)
                        extra := Pillow.Image.BinaryBuffer(extraOption.Value, "Pillow.Image.Save extra")
                        if keepRgb = 1 {
                            commentState := Pillow.Image.SaveJpegCommentBuffer(commentOption, this.RequireHandle(), false)
                            comment := commentState.Buffer
                            commentSize := commentState.Size
                            iccProfile := 0
                            iccProfileSize := 0
                            if iccProfileOption.Set {
                                iccProfile := Pillow.Image.BinaryBuffer(iccProfileOption.Value, "Pillow.Image.Save icc_profile")
                                iccProfileSize := iccProfile.Size
                            }
                            exif := 0
                            exifSize := 0
                            if exifOption.Set {
                                exif := Pillow.Image.BinaryBuffer(exifOption.Value, "Pillow.Image.Save exif")
                                exifSize := exif.Size
                            }
                            xmp := 0
                            xmpSize := 0
                            if xmpOption.Set {
                                xmp := Pillow.Image.BinaryBuffer(xmpOption.Value, "Pillow.Image.Save xmp")
                                xmpSize := xmp.Size
                            }
                            if restartMarkerBlocksOption.Set || restartMarkerRowsOption.Set {
                                if restartMarkerBlocksOption.Set && restartMarkerRowsOption.Set
                                    throw Error("Pillow.Image.Save JPEG cannot combine restart_marker_blocks and restart_marker_rows", -1)
                                if dpiOption.Set || subsamplingOption.Set
                                    throw Error("Pillow.Image.Save JPEG keep_rgb restart markers plus extra currently support default subsampling and no dpi", -1)
                                restartQTablesBuffer := hasJpegQTables ? qtables.Buffer : 0
                                restartQTableCount := hasJpegQTables ? qtables.Count : 0
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_keep_rgb_restart_marker_extra_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", xmp,
                                    "UPtr", xmpSize,
                                    "Ptr", restartQTablesBuffer,
                                    "UPtr", restartQTableCount,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Int", restartMarkerBlocks,
                                    "Int", restartMarkerRows,
                                    "Ptr", extra,
                                    "UPtr", extra.Size,
                                    "Int"
                                ))
                                return
                            }
                            if hasJpegQTables {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_extra_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", xmp,
                                    "UPtr", xmpSize,
                                    "Ptr", qtables.Buffer,
                                    "UPtr", qtables.Count,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Int", keepRgb,
                                    "Ptr", extra,
                                    "UPtr", extra.Size,
                                    "Int"
                                ))
                            } else {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_keep_rgb_extra_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", xmp,
                                    "UPtr", xmpSize,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Int", keepRgb,
                                    "Ptr", extra,
                                    "UPtr", extra.Size,
                                    "Int"
                                ))
                            }
                            return
                        }
                        if restartMarkerBlocksOption.Set || restartMarkerRowsOption.Set {
                            if restartMarkerBlocksOption.Set && restartMarkerRowsOption.Set
                                throw Error("Pillow.Image.Save JPEG cannot combine restart_marker_blocks and restart_marker_rows", -1)
                            commentState := Pillow.Image.SaveJpegCommentBuffer(commentOption, this.RequireHandle(), false)
                            comment := commentState.Buffer
                            commentSize := commentState.Size
                            iccProfile := 0
                            iccProfileSize := 0
                            if iccProfileOption.Set {
                                iccProfile := Pillow.Image.BinaryBuffer(iccProfileOption.Value, "Pillow.Image.Save icc_profile")
                                iccProfileSize := iccProfile.Size
                            }
                            exif := 0
                            exifSize := 0
                            if exifOption.Set {
                                exif := Pillow.Image.BinaryBuffer(exifOption.Value, "Pillow.Image.Save exif")
                                exifSize := exif.Size
                            }
                            xmp := 0
                            xmpSize := 0
                            if xmpOption.Set {
                                xmp := Pillow.Image.BinaryBuffer(xmpOption.Value, "Pillow.Image.Save xmp")
                                xmpSize := xmp.Size
                            }
                            restartQTablesBuffer := hasJpegQTables ? qtables.Buffer : 0
                            restartQTableCount := hasJpegQTables ? qtables.Count : 0
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_restart_marker_extra_encode_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", quality,
                                "Int", hasDpi,
                                "Double", dpiX,
                                "Double", dpiY,
                                "Ptr", comment,
                                "UPtr", commentSize,
                                "Ptr", iccProfile,
                                "UPtr", iccProfileSize,
                                "Ptr", exif,
                                "UPtr", exifSize,
                                "Ptr", xmp,
                                "UPtr", xmpSize,
                                "Ptr", restartQTablesBuffer,
                                "UPtr", restartQTableCount,
                                "Int", subsampling,
                                "Int", progressive,
                                "Int", optimize,
                                "Int", restartMarkerBlocks,
                                "Int", restartMarkerRows,
                                "Ptr", extra,
                                "UPtr", extra.Size,
                                "Int"
                            ))
                            return
                        }
                        if commentOption.Set || iccProfileOption.Set || exifOption.Set || xmpOption.Set {
                            commentState := Pillow.Image.SaveJpegCommentBuffer(commentOption, this.RequireHandle(), false)
                            comment := commentState.Buffer
                            commentSize := commentState.Size
                            iccProfile := 0
                            iccProfileSize := 0
                            if iccProfileOption.Set {
                                iccProfile := Pillow.Image.BinaryBuffer(iccProfileOption.Value, "Pillow.Image.Save icc_profile")
                                iccProfileSize := iccProfile.Size
                            }
                            exif := 0
                            exifSize := 0
                            if exifOption.Set {
                                exif := Pillow.Image.BinaryBuffer(exifOption.Value, "Pillow.Image.Save exif")
                                exifSize := exif.Size
                            }
                            xmp := 0
                            xmpSize := 0
                            if xmpOption.Set {
                                xmp := Pillow.Image.BinaryBuffer(xmpOption.Value, "Pillow.Image.Save xmp")
                                xmpSize := xmp.Size
                            }
                            if hasJpegQTables {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_metadata_extra_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", xmp,
                                    "UPtr", xmpSize,
                                    "Ptr", qtables.Buffer,
                                    "UPtr", qtables.Count,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Ptr", extra,
                                    "UPtr", extra.Size,
                                    "Int"
                                ))
                            } else {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_extra_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", xmp,
                                    "UPtr", xmpSize,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Ptr", extra,
                                    "UPtr", extra.Size,
                                    "Int"
                                ))
                            }
                        } else {
                            if hasJpegQTables {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_metadata_extra_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", 0,
                                    "UPtr", 0,
                                    "Ptr", 0,
                                    "UPtr", 0,
                                    "Ptr", 0,
                                    "UPtr", 0,
                                    "Ptr", 0,
                                    "UPtr", 0,
                                    "Ptr", qtables.Buffer,
                                    "UPtr", qtables.Count,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Ptr", extra,
                                    "UPtr", extra.Size,
                                    "Int"
                                ))
                            } else {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_extra_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Ptr", extra,
                                    "UPtr", extra.Size,
                                    "Int"
                                ))
                            }
                        }
                        return
                    }
                    if restartMarkerBlocksOption.Set || restartMarkerRowsOption.Set {
                        if restartMarkerBlocksOption.Set && restartMarkerRowsOption.Set
                            throw Error("Pillow.Image.Save JPEG cannot combine restart_marker_blocks and restart_marker_rows", -1)
                        hasRestartMetadata := commentOption.Set || iccProfileOption.Set || exifOption.Set || xmpOption.Set
                        hasOpenedRgbQTablesKeepRestart := qtablesKeep
                            && this.Mode = "RGB"
                            && !qualityOption.Set
                            && keepRgb != 1
                        hasOpenedCmykQTablesKeepRestart := qtablesKeep
                            && this.Mode = "CMYK"
                            && !qualityOption.Set
                            && keepRgb != 1
                        hasCmykRestartPreset := this.Mode = "CMYK"
                            && (qualityPresetSet || qtablesPresetSet)
                        hasRestartQTables := hasJpegQTables
                            && (!qualityPresetSet || hasCmykRestartPreset)
                            && (!qtablesKeep || hasOpenedRgbQTablesKeepRestart || hasOpenedCmykQTablesKeepRestart)
                        hasCmykBaselineRestartMetadataRealSubsampling := this.Mode = "CMYK"
                            && hasRestartMetadata
                            && hasRestartQTables
                            && subsamplingOption.Set
                            && !subsamplingKeep
                            && (subsampling = 1 || subsampling = 2)
                        hasCmykBaselineRestartCoreMetadataSentinel := this.Mode = "CMYK"
                            && (commentOption.Set || iccProfileOption.Set || exifOption.Set)
                            && hasRestartQTables
                            && (qualityKeep || qualityPresetSet || qtablesKeep || qtablesPresetSet)
                            && optimize != 1
                            && progressive != 1
                        hasUnsupportedRestartOptions := !(this.Mode = "L" || this.Mode = "RGB" || this.Mode = "CMYK")
                            || (qualityKeep && !(this.Mode = "RGB" || this.Mode = "CMYK"))
                            || (qualityPresetSet && this.Mode != "CMYK")
                            || (qtablesPresetSet && this.Mode != "CMYK")
                            || (dpiOption.Set && !(this.Mode = "CMYK" && hasRestartQTables))
                            || (subsamplingOption.Set && (!hasRestartQTables || this.Mode = "L"))
                            || (qtablesOption.Set && !hasRestartQTables)
                            || (keepRgb = 1 && (this.Mode != "RGB" || subsamplingOption.Set))
                            || (this.Mode = "CMYK" && (keepRgb = 1 || (qtablesOption.Set && !hasRestartQTables) || (hasRestartMetadata && optimize != 1 && progressive != 1 && !hasCmykBaselineRestartMetadataRealSubsampling && !hasCmykBaselineRestartCoreMetadataSentinel)))
                        if hasUnsupportedRestartOptions
                            throw Error("Pillow.Image.Save JPEG restart markers currently support mode L, RGB, or bounded CMYK baseline/optimized/progressive output with resolved qtables and optional real subsampling", -1)
                        if hasRestartMetadata || optimize = 1 || progressive = 1 || hasRestartQTables || keepRgb = 1 {
                            commentState := Pillow.Image.SaveJpegCommentBuffer(
                                commentOption,
                                this.RequireHandle(),
                                qualityKeep || qtablesKeep || subsamplingKeep
                                    || qualityPresetSet || qtablesPresetSet)
                            comment := commentState.Buffer
                            commentSize := commentState.Size
                            iccProfile := 0
                            iccProfileSize := 0
                            if iccProfileOption.Set {
                                iccProfile := Pillow.Image.BinaryBuffer(iccProfileOption.Value, "Pillow.Image.Save icc_profile")
                                iccProfileSize := iccProfile.Size
                            }
                            exif := 0
                            exifSize := 0
                            if exifOption.Set {
                                exif := Pillow.Image.BinaryBuffer(exifOption.Value, "Pillow.Image.Save exif")
                                exifSize := exif.Size
                            }
                            xmp := 0
                            xmpSize := 0
                            if xmpOption.Set {
                                xmp := Pillow.Image.BinaryBuffer(xmpOption.Value, "Pillow.Image.Save xmp")
                                xmpSize := xmp.Size
                            }
                            if keepRgb = 1 {
                                restartQTablesBuffer := hasRestartQTables ? qtables.Buffer : 0
                                restartQTableCount := hasRestartQTables ? qtables.Count : 0
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_keep_rgb_restart_marker_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", xmp,
                                    "UPtr", xmpSize,
                                    "Ptr", restartQTablesBuffer,
                                    "UPtr", restartQTableCount,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Int", restartMarkerBlocks,
                                    "Int", restartMarkerRows,
                                    "Int"
                                ))
                            } else if hasRestartQTables {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", xmp,
                                    "UPtr", xmpSize,
                                    "Ptr", qtables.Buffer,
                                    "UPtr", qtables.Count,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Int", restartMarkerBlocks,
                                    "Int", restartMarkerRows,
                                    "Int"
                                ))
                            } else if progressive = 1 {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_restart_marker_progressive_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", xmp,
                                    "UPtr", xmpSize,
                                    "Int", restartMarkerBlocks,
                                    "Int", restartMarkerRows,
                                    "Int"
                                ))
                            } else if optimize = 1 {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_restart_marker_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", xmp,
                                    "UPtr", xmpSize,
                                    "Int", restartMarkerBlocks,
                                    "Int", restartMarkerRows,
                                    "Int", optimize,
                                    "Int"
                                ))
                            } else {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_restart_marker_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", xmp,
                                    "UPtr", xmpSize,
                                    "Int", restartMarkerBlocks,
                                    "Int", restartMarkerRows,
                                    "Int"
                                ))
                            }
                        } else if restartMarkerBlocksOption.Set {
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_restart_marker_blocks_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", quality,
                                "Int", restartMarkerBlocks,
                                "Int"
                            ))
                        } else {
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_restart_marker_rows_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", quality,
                                "Int", restartMarkerRows,
                                "Int"
                            ))
                        }
                        return
                    }
                    if this.Mode = "CMYK" {
                        hasJpegMetadata := commentOption.Set || iccProfileOption.Set || exifOption.Set || xmpOption.Set
                        hasCmykProgressive := progressive = 1
                        hasCmykQTablesProgressiveSubsampling := hasJpegQTables && hasCmykProgressive
                        hasCmykQTablesKeepRgbSubsampling := hasJpegQTables && keepRgb = 1 && (!hasCmykProgressive || !hasDpi)
                        hasCmykQTablesBaselineMetadataSubsampling := hasJpegQTables && hasJpegMetadata && keepRgb != 1 && !hasCmykProgressive && !(optimizeOption.Set && optimizeOption.Value)
                        hasCmykQTablesOptimizedMetadataSubsampling := hasJpegQTables && hasJpegMetadata && keepRgb != 1 && !hasCmykProgressive && (optimizeOption.Set && optimizeOption.Value)
                        hasCmykQTablesMetadataSubsampling := hasJpegQTables && hasJpegMetadata && keepRgb != 1 && (hasCmykQTablesBaselineMetadataSubsampling || hasCmykQTablesOptimizedMetadataSubsampling || hasCmykQTablesProgressiveSubsampling)
                        hasCmykMetadataSubsamplingKeep := hasJpegMetadata && subsamplingKeep && keepRgb != 1 && !hasJpegQTables && !hasCmykProgressive && !(optimizeOption.Set && optimizeOption.Value)
                        if subsamplingOption.Set && !subsamplingKeep && !qualityKeep && !qualityPresetSet && ((hasJpegMetadata && keepRgb != 1 && !hasCmykMetadataSubsamplingKeep && !hasCmykQTablesMetadataSubsampling) || (hasJpegQTables && !hasCmykQTablesKeepRgbSubsampling && !hasCmykQTablesMetadataSubsampling && !hasCmykQTablesProgressiveSubsampling) || (hasCmykProgressive && !hasCmykQTablesKeepRgbSubsampling && !hasCmykQTablesProgressiveSubsampling) || (optimizeOption.Set && optimizeOption.Value && !hasCmykQTablesKeepRgbSubsampling && !hasCmykQTablesMetadataSubsampling && !hasCmykQTablesProgressiveSubsampling))
                            throw Error("Pillow.Image.Save JPEG CMYK subsampling currently supports only baseline quality and dpi saves", -1)
                        if hasJpegMetadata {
                            commentState := Pillow.Image.SaveJpegCommentBuffer(commentOption, this.RequireHandle(), qualityKeep || qtablesKeep || subsamplingKeep)
                            comment := commentState.Buffer
                            commentSize := commentState.Size
                            iccProfile := 0
                            iccProfileSize := 0
                            if iccProfileOption.Set {
                                iccProfile := Pillow.Image.BinaryBuffer(iccProfileOption.Value, "Pillow.Image.Save icc_profile")
                                iccProfileSize := iccProfile.Size
                            }
                            exif := 0
                            exifSize := 0
                            if exifOption.Set {
                                exif := Pillow.Image.BinaryBuffer(exifOption.Value, "Pillow.Image.Save exif")
                                exifSize := exif.Size
                            }
                            xmp := 0
                            xmpSize := 0
                            if xmpOption.Set {
                                xmp := Pillow.Image.BinaryBuffer(xmpOption.Value, "Pillow.Image.Save xmp")
                                xmpSize := xmp.Size
                            }
                            if hasJpegQTables {
                                if keepRgb = 1 {
                                    if xmpOption.Set {
                                        Pillow.CheckStatus(DllCall(
                                            Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_xmp_encode_options",
                                            "Ptr", this.RequireHandle(),
                                            "Ptr", pathBytes,
                                            "Int", quality,
                                            "Int", hasDpi,
                                            "Double", dpiX,
                                            "Double", dpiY,
                                            "Ptr", comment,
                                            "UPtr", commentSize,
                                            "Ptr", iccProfile,
                                            "UPtr", iccProfileSize,
                                            "Ptr", exif,
                                            "UPtr", exifSize,
                                            "Ptr", xmp,
                                            "UPtr", xmpSize,
                                            "Ptr", qtables.Buffer,
                                            "UPtr", qtables.Count,
                                            "Int", subsampling,
                                            "Int", progressive,
                                            "Int", optimize,
                                            "Int", keepRgb,
                                            "Int"
                                        ))
                                    } else {
                                        Pillow.CheckStatus(DllCall(
                                            Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options",
                                            "Ptr", this.RequireHandle(),
                                            "Ptr", pathBytes,
                                            "Int", quality,
                                            "Int", hasDpi,
                                            "Double", dpiX,
                                            "Double", dpiY,
                                            "Ptr", comment,
                                            "UPtr", commentSize,
                                            "Ptr", iccProfile,
                                            "UPtr", iccProfileSize,
                                            "Ptr", exif,
                                            "UPtr", exifSize,
                                            "Ptr", qtables.Buffer,
                                            "UPtr", qtables.Count,
                                            "Int", subsampling,
                                            "Int", progressive,
                                            "Int", optimize,
                                            "Int", keepRgb,
                                            "Int"
                                        ))
                                    }
                                } else if xmpOption.Set {
                                    Pillow.CheckStatus(DllCall(
                                        Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_metadata_xmp_encode_options",
                                        "Ptr", this.RequireHandle(),
                                        "Ptr", pathBytes,
                                        "Int", quality,
                                        "Int", hasDpi,
                                        "Double", dpiX,
                                        "Double", dpiY,
                                        "Ptr", comment,
                                        "UPtr", commentSize,
                                        "Ptr", iccProfile,
                                        "UPtr", iccProfileSize,
                                        "Ptr", exif,
                                        "UPtr", exifSize,
                                        "Ptr", xmp,
                                        "UPtr", xmpSize,
                                        "Ptr", qtables.Buffer,
                                        "UPtr", qtables.Count,
                                        "Int", subsampling,
                                        "Int", progressive,
                                        "Int", optimize,
                                        "Int"
                                    ))
                                } else {
                                    Pillow.CheckStatus(DllCall(
                                        Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_metadata_encode_options",
                                        "Ptr", this.RequireHandle(),
                                        "Ptr", pathBytes,
                                        "Int", quality,
                                        "Int", hasDpi,
                                        "Double", dpiX,
                                        "Double", dpiY,
                                        "Ptr", comment,
                                        "UPtr", commentSize,
                                        "Ptr", iccProfile,
                                        "UPtr", iccProfileSize,
                                        "Ptr", exif,
                                        "UPtr", exifSize,
                                        "Ptr", qtables.Buffer,
                                        "UPtr", qtables.Count,
                                        "Int", subsampling,
                                        "Int", progressive,
                                        "Int", optimize,
                                        "Int"
                                    ))
                                }
                            } else if keepRgb = 1 {
                                if xmpOption.Set {
                                    Pillow.CheckStatus(DllCall(
                                        Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_keep_rgb_xmp_encode_options",
                                        "Ptr", this.RequireHandle(),
                                        "Ptr", pathBytes,
                                        "Int", quality,
                                        "Int", hasDpi,
                                        "Double", dpiX,
                                        "Double", dpiY,
                                        "Ptr", comment,
                                        "UPtr", commentSize,
                                        "Ptr", iccProfile,
                                        "UPtr", iccProfileSize,
                                        "Ptr", exif,
                                        "UPtr", exifSize,
                                        "Ptr", xmp,
                                        "UPtr", xmpSize,
                                        "Int", subsampling,
                                        "Int", progressive,
                                        "Int", optimize,
                                        "Int", keepRgb,
                                        "Int"
                                    ))
                                } else {
                                    Pillow.CheckStatus(DllCall(
                                        Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_keep_rgb_encode_options",
                                        "Ptr", this.RequireHandle(),
                                        "Ptr", pathBytes,
                                        "Int", quality,
                                        "Int", hasDpi,
                                        "Double", dpiX,
                                        "Double", dpiY,
                                        "Ptr", comment,
                                        "UPtr", commentSize,
                                        "Ptr", iccProfile,
                                        "UPtr", iccProfileSize,
                                        "Ptr", exif,
                                        "UPtr", exifSize,
                                        "Int", subsampling,
                                        "Int", progressive,
                                        "Int", optimize,
                                        "Int", keepRgb,
                                        "Int"
                                    ))
                                }
                            } else if xmpOption.Set {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_xmp_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", xmp,
                                    "UPtr", xmpSize,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Int"
                                ))
                            } else if hasCmykProgressive || (optimizeOption.Set && optimizeOption.Value) || hasCmykMetadataSubsamplingKeep {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Int"
                                ))
                            } else {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Int"
                                ))
                            }
                        } else if hasJpegQTables {
                            if keepRgb = 1 {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", qtables.Buffer,
                                    "UPtr", qtables.Count,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Int", keepRgb,
                                    "Int"
                                ))
                            } else {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", qtables.Buffer,
                                    "UPtr", qtables.Count,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Int"
                                ))
                            }
                        } else if keepRgb = 1 {
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_encode_keep_rgb_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", quality,
                                "Int", hasDpi,
                                "Double", dpiX,
                                "Double", dpiY,
                                "Int", subsampling,
                                "Int", progressive,
                                "Int", optimize,
                                "Int", keepRgb,
                                "Int"
                            ))
                        } else if subsamplingOption.Set || progressiveOption.Set || progressionOption.Set || optimizeOption.Set {
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_encode_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", quality,
                                "Int", hasDpi,
                                "Double", dpiX,
                                "Double", dpiY,
                                "Int", subsampling,
                                "Int", progressive,
                                "Int", optimize,
                                "Int"
                            ))
                        } else {
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", quality,
                                "Int", hasDpi,
                                "Double", dpiX,
                                "Double", dpiY,
                                "Int"
                            ))
                        }
                        return
                    }
                    if commentOption.Set || iccProfileOption.Set || exifOption.Set || xmpOption.Set {
                        commentState := Pillow.Image.SaveJpegCommentBuffer(commentOption, this.RequireHandle(), qualityKeep || qtablesKeep || subsamplingKeep)
                        comment := commentState.Buffer
                        commentSize := commentState.Size
                        iccProfile := 0
                        iccProfileSize := 0
                        if iccProfileOption.Set {
                            iccProfile := Pillow.Image.BinaryBuffer(iccProfileOption.Value, "Pillow.Image.Save icc_profile")
                            iccProfileSize := iccProfile.Size
                        }
                        exif := 0
                        exifSize := 0
                        if exifOption.Set {
                            exif := Pillow.Image.BinaryBuffer(exifOption.Value, "Pillow.Image.Save exif")
                            exifSize := exif.Size
                        }
                        xmp := 0
                        xmpSize := 0
                        if xmpOption.Set {
                            xmp := Pillow.Image.BinaryBuffer(xmpOption.Value, "Pillow.Image.Save xmp")
                            xmpSize := xmp.Size
                        }
                        if keepRgb = 1 {
                            if hasJpegQTables {
                                if xmpOption.Set {
                                    Pillow.CheckStatus(DllCall(
                                        Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_xmp_encode_options",
                                        "Ptr", this.RequireHandle(),
                                        "Ptr", pathBytes,
                                        "Int", quality,
                                        "Int", hasDpi,
                                        "Double", dpiX,
                                        "Double", dpiY,
                                        "Ptr", comment,
                                        "UPtr", commentSize,
                                        "Ptr", iccProfile,
                                        "UPtr", iccProfileSize,
                                        "Ptr", exif,
                                        "UPtr", exifSize,
                                        "Ptr", xmp,
                                        "UPtr", xmpSize,
                                        "Ptr", qtables.Buffer,
                                        "UPtr", qtables.Count,
                                        "Int", subsampling,
                                        "Int", progressive,
                                        "Int", optimize,
                                        "Int", keepRgb,
                                        "Int"
                                    ))
                                } else {
                                    Pillow.CheckStatus(DllCall(
                                        Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options",
                                        "Ptr", this.RequireHandle(),
                                        "Ptr", pathBytes,
                                        "Int", quality,
                                        "Int", hasDpi,
                                        "Double", dpiX,
                                        "Double", dpiY,
                                        "Ptr", comment,
                                        "UPtr", commentSize,
                                        "Ptr", iccProfile,
                                        "UPtr", iccProfileSize,
                                        "Ptr", exif,
                                        "UPtr", exifSize,
                                        "Ptr", qtables.Buffer,
                                        "UPtr", qtables.Count,
                                        "Int", subsampling,
                                        "Int", progressive,
                                        "Int", optimize,
                                        "Int", keepRgb,
                                        "Int"
                                    ))
                                }
                            } else if xmpOption.Set {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_keep_rgb_xmp_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", xmp,
                                    "UPtr", xmpSize,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Int", keepRgb,
                                    "Int"
                                ))
                            } else {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_keep_rgb_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Int", keepRgb,
                                    "Int"
                                ))
                            }
                        } else if hasJpegQTables {
                            if xmpOption.Set {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_metadata_xmp_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", xmp,
                                    "UPtr", xmpSize,
                                    "Ptr", qtables.Buffer,
                                    "UPtr", qtables.Count,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Int"
                                ))
                            } else {
                                Pillow.CheckStatus(DllCall(
                                    Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_metadata_encode_options",
                                    "Ptr", this.RequireHandle(),
                                    "Ptr", pathBytes,
                                    "Int", quality,
                                    "Int", hasDpi,
                                    "Double", dpiX,
                                    "Double", dpiY,
                                    "Ptr", comment,
                                    "UPtr", commentSize,
                                    "Ptr", iccProfile,
                                    "UPtr", iccProfileSize,
                                    "Ptr", exif,
                                    "UPtr", exifSize,
                                    "Ptr", qtables.Buffer,
                                    "UPtr", qtables.Count,
                                    "Int", subsampling,
                                    "Int", progressive,
                                    "Int", optimize,
                                    "Int"
                                ))
                            }
                        } else if xmpOption.Set {
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_xmp_encode_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", quality,
                                "Int", hasDpi,
                                "Double", dpiX,
                                "Double", dpiY,
                                "Ptr", comment,
                                "UPtr", commentSize,
                                "Ptr", iccProfile,
                                "UPtr", iccProfileSize,
                                "Ptr", exif,
                                "UPtr", exifSize,
                                "Ptr", xmp,
                                "UPtr", xmpSize,
                                "Int", subsampling,
                                "Int", progressive,
                                "Int", optimize,
                                "Int"
                            ))
                        } else if progressiveOption.Set || progressionOption.Set || optimizeOption.Set {
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_encode_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", quality,
                                "Int", hasDpi,
                                "Double", dpiX,
                                "Double", dpiY,
                                "Ptr", comment,
                                "UPtr", commentSize,
                                "Ptr", iccProfile,
                                "UPtr", iccProfileSize,
                                "Ptr", exif,
                                "UPtr", exifSize,
                                "Int", subsampling,
                                "Int", progressive,
                                "Int", optimize,
                                "Int"
                            ))
                        } else if subsamplingOption.Set {
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_subsampling_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", quality,
                                "Int", hasDpi,
                                "Double", dpiX,
                                "Double", dpiY,
                                "Ptr", comment,
                                "UPtr", commentSize,
                                "Ptr", iccProfile,
                                "UPtr", iccProfileSize,
                                "Ptr", exif,
                                "UPtr", exifSize,
                                "Int", subsampling,
                                "Int"
                            ))
                        } else {
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_metadata_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", quality,
                                "Int", hasDpi,
                                "Double", dpiX,
                                "Double", dpiY,
                                "Ptr", comment,
                                "UPtr", commentSize,
                                "Ptr", iccProfile,
                                "UPtr", iccProfileSize,
                                "Ptr", exif,
                                "UPtr", exifSize,
                                "Int"
                            ))
                        }
                    } else if keepRgb = 1 {
                        if hasJpegQTables {
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", quality,
                                "Int", hasDpi,
                                "Double", dpiX,
                                "Double", dpiY,
                                "Ptr", qtables.Buffer,
                                "UPtr", qtables.Count,
                                "Int", subsampling,
                                "Int", progressive,
                                "Int", optimize,
                                "Int", keepRgb,
                                "Int"
                            ))
                        } else {
                            Pillow.CheckStatus(DllCall(
                                Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_encode_keep_rgb_options",
                                "Ptr", this.RequireHandle(),
                                "Ptr", pathBytes,
                                "Int", quality,
                                "Int", hasDpi,
                                "Double", dpiX,
                                "Double", dpiY,
                                "Int", subsampling,
                                "Int", progressive,
                                "Int", optimize,
                                "Int", keepRgb,
                                "Int"
                            ))
                        }
                    } else if hasJpegQTables {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_qtables_encode_options",
                            "Ptr", this.RequireHandle(),
                            "Ptr", pathBytes,
                            "Int", quality,
                            "Int", hasDpi,
                            "Double", dpiX,
                            "Double", dpiY,
                            "Ptr", qtables.Buffer,
                            "UPtr", qtables.Count,
                            "Int", subsampling,
                            "Int", progressive,
                            "Int", optimize,
                            "Int"
                        ))
                    } else if subsamplingOption.Set || progressiveOption.Set || progressionOption.Set || optimizeOption.Set {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_encode_options",
                            "Ptr", this.RequireHandle(),
                            "Ptr", pathBytes,
                            "Int", quality,
                            "Int", hasDpi,
                            "Double", dpiX,
                            "Double", dpiY,
                            "Int", subsampling,
                            "Int", progressive,
                            "Int", optimize,
                            "Int"
                        ))
                    } else {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_save_jpeg_options",
                            "Ptr", this.RequireHandle(),
                            "Ptr", pathBytes,
                            "Int", quality,
                            "Int", hasDpi,
                            "Double", dpiX,
                            "Double", dpiY,
                            "Int"
                        ))
                    }
                    return
                }
            }

            if IsSet(saveOptions) && resolvedFormat = "TGA" {
                rleOption := Pillow.Image.SaveOption(saveOptions, "Rle", "rle")
                compressionOption := Pillow.Image.SaveOption(saveOptions, "Compression", "compression")
                idOption := Pillow.Image.SaveOption(saveOptions, "IdSection", "id_section")
                orientationOption := Pillow.Image.SaveOption(saveOptions, "Orientation", "orientation")
                if rleOption.Set || compressionOption.Set || idOption.Set || orientationOption.Set {
                    rle := 0
                    if rleOption.Set {
                        rle := !!rleOption.Value
                    } else if compressionOption.Set {
                        rle := StrLower(String(compressionOption.Value)) = "tga_rle"
                    }
                    idBytes := 0
                    idSize := 0
                    if idOption.Set {
                        idBytes := Pillow.Image.BinaryBuffer(idOption.Value, "Pillow.Image.Save id_section")
                        idSize := idBytes.Size
                    }
                    orientation := -1
                    if orientationOption.Set
                        orientation := orientationOption.Value
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_tga_full_options",
                        "Ptr", this.RequireHandle(),
                        "Ptr", pathBytes,
                        "Int", rle,
                        "Ptr", idBytes,
                        "UPtr", idSize,
                        "Int", orientation,
                        "Int"
                    ))
                    return
                }
            }

            if IsSet(saveOptions) && resolvedFormat = "QOI" {
                colorspaceOption := Pillow.Image.SaveOption(saveOptions, "Colorspace", "colorspace")
                if colorspaceOption.Set {
                    ; Pillow writes 0 only when colorspace == "sRGB"; every
                    ; other value (including the default) writes 1 (linear).
                    colorspace := colorspaceOption.Value = "sRGB" ? 0 : 1
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_qoi_options",
                        "Ptr", this.RequireHandle(),
                        "Ptr", pathBytes,
                        "Int", colorspace,
                        "Int"
                    ))
                    return
                }
            }

            if IsSet(saveOptions) && resolvedFormat = "XBM" {
                hotspotOption := Pillow.Image.SaveOption(saveOptions, "Hotspot", "hotspot")
                if hotspotOption.Set {
                    hotspot := Pillow.Image.SaveHotspotPair(hotspotOption.Value)
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_xbm_options",
                        "Ptr", this.RequireHandle(),
                        "Ptr", pathBytes,
                        "Int", 1,
                        "Int", hotspot[1],
                        "Int", hotspot[2],
                        "Int"
                    ))
                    return
                }
            }

            if IsSet(saveOptions) && resolvedFormat = "ICO" {
                sizesOption := Pillow.Image.SaveOption(saveOptions, "Sizes", "sizes")
                bitmapFormatOption := Pillow.Image.SaveOption(saveOptions, "BitmapFormat", "bitmap_format")
                appendOption := Pillow.Image.SaveOption(saveOptions, "AppendImages", "append_images")
                if sizesOption.Set || bitmapFormatOption.Set || appendOption.Set {
                    if sizesOption.Set {
                        sizes := Pillow.Image.SaveIcoSizePairs(sizesOption.Value)
                        sizePtr := sizes.Count ? sizes.Buffer : 0
                        sizeCount := sizes.Count
                        hasSizes := 1
                    } else {
                        sizePtr := 0
                        sizeCount := 0
                        hasSizes := 0
                    }
                    bitmapFormat := ""
                    if bitmapFormatOption.Set && bitmapFormatOption.Value is String
                        bitmapFormat := bitmapFormatOption.Value
                    bitmapFormatBytes := Pillow.Image.Utf8Buffer(bitmapFormat)
                    if appendOption.Set {
                        images := [this]
                        appendImages := appendOption.Value
                        if IsObject(appendImages) && appendImages is Pillow.Image {
                            images.Push(appendImages)
                        } else if IsObject(appendImages) {
                            for image in appendImages {
                                if !(IsObject(image) && image is Pillow.Image)
                                    throw Error("Pillow.Image.Save append_images expects Pillow.Image values", -1)
                                images.Push(image)
                            }
                        } else {
                            throw Error("Pillow.Image.Save append_images expects an image or image array", -1)
                        }
                        handles := Pillow.Image.HandleArray(images)
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_save_ico_frames_format_options",
                            "Ptr", handles,
                            "UPtr", images.Length,
                            "Ptr", pathBytes,
                            "Ptr", sizePtr,
                            "UPtr", sizeCount,
                            "Int", hasSizes,
                            "Ptr", bitmapFormatBytes,
                            "Int"
                        ))
                        return
                    }
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_ico_format_options",
                        "Ptr", this.RequireHandle(),
                        "Ptr", pathBytes,
                        "Ptr", sizePtr,
                        "UPtr", sizeCount,
                        "Int", hasSizes,
                        "Ptr", bitmapFormatBytes,
                        "Int"
                    ))
                    return
                }
            }

            if resolvedFormat = "GIF" {
                ; BEHAV-SAVEOPTS-001: Pillow's GIF save is interlaced by
                ; default (interlace=1) except images with a side shorter
                ; than 16 pixels (the @PIL153 workaround), so the plain save
                ; routes through the native writer with that default.
                gifInterlace := Min(this.Width, this.Height) < 16 ? 0 : 1
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_gif_interlace_palette_options",
                    "Ptr", this.RequireHandle(),
                    "Ptr", pathBytes,
                    "Int", 0,
                    "Int", 0,
                    "Int", gifInterlace,
                    "Ptr", 0,
                    "UPtr", 0,
                    "Int"
                ))
                return
            }

            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_save_" StrLower(resolvedFormat),
                "Ptr", this.RequireHandle(),
                "Ptr", pathBytes,
                "Int"
            ))
        }

        SaveDib(path) {
            tempPath := A_Temp "\pillow_c_dib_save_" Random(1, 2147483647) ".bmp"
            try {
                tempBytes := Pillow.Image.Utf8Buffer(tempPath)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_bmp",
                    "Ptr", this.RequireHandle(),
                    "Ptr", tempBytes,
                    "Int"
                ))
                source := FileOpen(tempPath, "r")
                if !source
                    throw Error("Pillow.Image.Save DIB failed to reopen the temporary BMP", -1)
                size := source.Length
                if size < 14 {
                    source.Close()
                    throw Error("Pillow.Image.Save DIB temporary BMP is truncated", -1)
                }
                source.Pos := 14
                dib := Buffer(size - 14, 0)
                source.RawRead(dib, dib.Size)
                source.Close()
                target := FileOpen(path, "w")
                if !target
                    throw Error("Pillow.Image.Save DIB failed to open the target file", -1)
                target.RawWrite(dib.Ptr, dib.Size)
                target.Close()
            } finally {
                try {
                    FileDelete(tempPath)
                } catch {
                }
            }
        }

        SaveIm(path) {
            ; BEHAV-IM-001: Pillow 11.3.0's IM format is a 512-byte ASCII
            ; header (NUL padding plus a ^Z marker, an optional 768-byte
            ; palette LUT for P) followed by raw pixel bytes written with
            ; orientation -1 (bottom-up rows) and Pillow's ;L per-row
            ; planar raw modes for multi-channel modes.
            typeName := ""
            rawmode := ""
            switch this.Mode {
                case "1": typeName := "0 1", rawmode := "1"
                case "L": typeName := "Greyscale", rawmode := "L"
                case "LA": typeName := "LA", rawmode := "LA;L"
                case "P": typeName := "Greyscale", rawmode := "P"
                case "I": typeName := "L 32S", rawmode := "I;32S"
                case "I;16": typeName := "L 16", rawmode := "I;16"
                case "I;16B": typeName := "L 16B", rawmode := "I;16B"
                case "F": typeName := "L 32F", rawmode := "F;32F"
                case "RGB": typeName := "RGB", rawmode := "RGB;L"
                case "RGBA": typeName := "RGBA", rawmode := "RGBA;L"
                case "RGBX": typeName := "RGBX", rawmode := "RGBX;L"
                case "CMYK": typeName := "CMYK", rawmode := "CMYK;L"
                default:
                    throw Error("Cannot save " this.Mode " images as IM", -1)
            }
            base := ""
            if RegExMatch(path, "i)([^\\/]+)$", &baseMatch) {
                base := baseMatch[1]
                if RegExMatch(base, "i)(.+)(\.[^.]+)$", &nameMatch) {
                    ext := nameMatch[2]
                    limit := 92 - StrLen(ext)
                    base := (StrLen(nameMatch[1]) > limit ? SubStr(nameMatch[1], 1, limit) : nameMatch[1]) ext
                }
            }
            header := "Image type: " typeName " image`r`n"
            if base != ""
                header .= "Name: " base "`r`n"
            header .= "Image size (x*y): " this.Size[1] "*" this.Size[2] "`r`n"
            header .= "File size (no of images): 1`r`n"
            if this.Mode = "P"
                header .= "Lut: 1`r`n"
            padCount := 511 - StrLen(header)
            if padCount < 0
                throw Error("Cannot save " this.Mode " images as IM", -1)
            target := FileOpen(path, "w")
            if !target
                throw Error("Pillow.Image.Save IM failed to open the target file", -1)
            try {
                target.Write(header)
                pad := Buffer(padCount + 1, 0)
                NumPut("UChar", 0x1A, pad, padCount)
                target.RawWrite(pad.Ptr, pad.Size)
                if this.Mode = "P" {
                    ; the LUT holds the R plane, then G, then B (256 each).
                    palette := this.GetPalette("RGB")
                    while palette.Length < 768
                        palette.Push(0)
                    lut := Buffer(768, 0)
                    loop 256 {
                        NumPut("UChar", palette[(A_Index - 1) * 3 + 1], lut, A_Index - 1)
                        NumPut("UChar", palette[(A_Index - 1) * 3 + 2], lut, 256 + A_Index - 1)
                        NumPut("UChar", palette[(A_Index - 1) * 3 + 3], lut, 512 + A_Index - 1)
                    }
                    target.RawWrite(lut.Ptr, lut.Size)
                }
                raw := this.ToBytes("raw", rawmode, -1)
                target.RawWrite(raw.Ptr, raw.Size)
            } finally {
                target.Close()
            }
        }

        SavePalm(path) {
            ; BEHAV-PALM-001: Pillow 11.3.0's Palm pixmap save is output-only.
            ; The bounded slice covers the 8-bit P path: a 16-byte
            ; big-endian header, the 1026-byte colormap, and row-padded
            ; index bytes. L-with-bpp and mode-1 inverted slices stay
            ; separate children (Pillow raises the same mode error here).
            if this.Mode != "P"
                throw Error("cannot write mode " this.Mode " as Palm", -1)
            palette := this.GetPalette("RGB")
            while palette.Length < 768
                palette.Push(0)
            cols := this.Size[1]
            rows := this.Size[2]
            rowbytes := ((cols + 1) // 2) * 2
            header := Buffer(16, 0)
            NumPut("UChar", cols >> 8, header, 0)
            NumPut("UChar", cols & 0xFF, header, 1)
            NumPut("UChar", rows >> 8, header, 2)
            NumPut("UChar", rows & 0xFF, header, 3)
            NumPut("UChar", rowbytes >> 8, header, 4)
            NumPut("UChar", rowbytes & 0xFF, header, 5)
            NumPut("UChar", 0x40, header, 6)
            NumPut("UChar", 0x00, header, 7)
            NumPut("UChar", 8, header, 8)
            NumPut("UChar", 1, header, 9)
            NumPut("UChar", 0, header, 10)
            NumPut("UChar", 0, header, 11)
            NumPut("UChar", 0, header, 12)
            NumPut("UChar", 0xFF, header, 13)
            NumPut("UChar", 0, header, 14)
            NumPut("UChar", 0, header, 15)
            colormap := Buffer(2 + 256 * 4, 0)
            NumPut("UChar", 0x01, colormap, 0)
            NumPut("UChar", 0x00, colormap, 1)
            ; Pillow quirk: getpalette() returns the planar "RGB;L" storage
            ; and the writer slices it LINEARLY, so entry i's RGB is the
            ; three blob bytes at offset 3*i (walking across the planes).
            blob := Buffer(768, 0)
            loop 256 {
                blobPos := A_Index - 1
                NumPut("UChar", palette[blobPos * 3 + 1], blob, blobPos)
                NumPut("UChar", palette[blobPos * 3 + 2], blob, 256 + blobPos)
                NumPut("UChar", palette[blobPos * 3 + 3], blob, 512 + blobPos)
            }
            loop 256 {
                index := A_Index - 1
                NumPut("UChar", index, colormap, 2 + index * 4)
                NumPut("UChar", NumGet(blob, index * 3, "UChar"), colormap, 2 + index * 4 + 1)
                NumPut("UChar", NumGet(blob, index * 3 + 1, "UChar"), colormap, 2 + index * 4 + 2)
                NumPut("UChar", NumGet(blob, index * 3 + 2, "UChar"), colormap, 2 + index * 4 + 3)
            }
            tight := this.ToBytes("raw", "P")
            padded := Buffer(rowbytes * rows, 0)
            loop rows {
                if cols > 0
                    DllCall("ntdll\RtlMoveMemory", "Ptr", padded.Ptr + (A_Index - 1) * rowbytes, "Ptr", tight.Ptr + (A_Index - 1) * cols, "UPtr", cols)
            }
            target := FileOpen(path, "w")
            if !target
                throw Error("Pillow.Image.Save PALM failed to open the target file", -1)
            try {
                target.RawWrite(header.Ptr, header.Size)
                target.RawWrite(colormap.Ptr, colormap.Size)
                target.RawWrite(padded.Ptr, padded.Size)
            } finally {
                target.Close()
            }
        }

        SaveSpider(path) {
            ; BEHAV-SPIDER-001: Pillow 11.3.0's SPIDER is the float header
            ; records (nvalues floats, Fortran 1-based fields shifted to a
            ; 0-based written array) plus raw native float32 samples; save
            ; composes the exact header over the F;32NF raw encoder.
            w := this.Size[1]
            h := this.Size[2]
            lenbyt := w * 4
            if lenbyt = 0
                throw Error("cannot write zero-width image as SPIDER", -1)
            labrec := (1024 + lenbyt - 1) // lenbyt
            labbyt := labrec * lenbyt
            header := Buffer(labbyt, 0)
            NumPut("Float", 1.0, header, 0)
            NumPut("Float", h, header, 4)
            NumPut("Float", h, header, 8)
            NumPut("Float", 1.0, header, 16)
            NumPut("Float", w, header, 44)
            NumPut("Float", labrec, header, 48)
            NumPut("Float", labbyt, header, 84)
            NumPut("Float", lenbyt, header, 88)
            f := this.Mode = "F" ? this : this.Convert("F")
            try {
                data := f.ToBytes("raw", "F;32NF")
                target := FileOpen(path, "w")
                if !target
                    throw Error("Pillow.Image.Save SPIDER failed to open the target file", -1)
                try {
                    target.RawWrite(header.Ptr, header.Size)
                    target.RawWrite(data.Ptr, data.Size)
                } finally {
                    target.Close()
                }
            } finally {
                if f != this
                    f.Close()
            }
        }

        static ExifFamilyBuffers(exif) {
            asciiCount := exif.AsciiTags.Count
            intCount := exif.IntTags.Count
            rationalCount := exif.RationalTags.Count
            rationalArrayCount := exif.RationalArrayTags.Count
            shortArrayCount := exif.ShortArrayTags.Count
            byteArrayCount := exif.ByteArrayTags.Count
            uintArrayCount := exif.UintArrayTags.Count
            signedRationalCount := exif.SignedRationalTags.Count
            undefinedCount := exif.UndefinedTags.Count
            tags := asciiCount ? Buffer(asciiCount * 4, 0) : 0
            valuePtrs := asciiCount ? Buffer(asciiCount * A_PtrSize, 0) : 0
            valueSizes := asciiCount ? Buffer(asciiCount * A_PtrSize, 0) : 0
            valueBuffers := []
            index := 0
            for tag, value in exif.AsciiTags {
                index += 1
                valueBuffers.Push(Pillow.Image.Utf8Buffer(value))
                NumPut("Int", tag, tags, (index - 1) * 4)
                NumPut("Ptr", valueBuffers[index].Ptr, valuePtrs, (index - 1) * A_PtrSize)
                NumPut("UPtr", valueBuffers[index].Size, valueSizes, (index - 1) * A_PtrSize)
            }
            intTags := intCount ? Buffer(intCount * 4, 0) : 0
            intValues := intCount ? Buffer(intCount * 4, 0) : 0
            intTypes := intCount ? Buffer(intCount * 4, 0) : 0
            index := 0
            for tag, value in exif.IntTags {
                index += 1
                NumPut("Int", tag, intTags, (index - 1) * 4)
                NumPut("UInt", value, intValues, (index - 1) * 4)
                NumPut("Int", Pillow.Image.Exif.UintTagType(tag), intTypes, (index - 1) * 4)
            }
            rationalTags := rationalCount ? Buffer(rationalCount * 4, 0) : 0
            rationalNumerators := rationalCount ? Buffer(rationalCount * 4, 0) : 0
            rationalDenominators := rationalCount ? Buffer(rationalCount * 4, 0) : 0
            index := 0
            for tag, value in exif.RationalTags {
                index += 1
                NumPut("Int", tag, rationalTags, (index - 1) * 4)
                NumPut("UInt", value[1], rationalNumerators, (index - 1) * 4)
                NumPut("UInt", value[2], rationalDenominators, (index - 1) * 4)
            }
            rationalArrayTags := rationalArrayCount ? Buffer(rationalArrayCount * 4, 0) : 0
            rationalArrayOffsets := rationalArrayCount ? Buffer(rationalArrayCount * A_PtrSize, 0) : 0
            rationalArrayCounts := rationalArrayCount ? Buffer(rationalArrayCount * A_PtrSize, 0) : 0
            flatRationalArrayNumerators := []
            flatRationalArrayDenominators := []
            index := 0
            for tag, value in exif.RationalArrayTags {
                index += 1
                NumPut("Int", tag, rationalArrayTags, (index - 1) * 4)
                NumPut("UPtr", flatRationalArrayNumerators.Length, rationalArrayOffsets, (index - 1) * A_PtrSize)
                NumPut("UPtr", value.Length, rationalArrayCounts, (index - 1) * A_PtrSize)
                for pair in value {
                    flatRationalArrayNumerators.Push(pair[1])
                    flatRationalArrayDenominators.Push(pair[2])
                }
            }
            rationalArrayNumerators := flatRationalArrayNumerators.Length ? Buffer(flatRationalArrayNumerators.Length * 4, 0) : 0
            rationalArrayDenominators := flatRationalArrayDenominators.Length ? Buffer(flatRationalArrayDenominators.Length * 4, 0) : 0
            for index, value in flatRationalArrayNumerators
                NumPut("UInt", value, rationalArrayNumerators, (index - 1) * 4)
            for index, value in flatRationalArrayDenominators
                NumPut("UInt", value, rationalArrayDenominators, (index - 1) * 4)
            shortArrayTags := shortArrayCount ? Buffer(shortArrayCount * 4, 0) : 0
            shortArrayOffsets := shortArrayCount ? Buffer(shortArrayCount * A_PtrSize, 0) : 0
            shortArrayCounts := shortArrayCount ? Buffer(shortArrayCount * A_PtrSize, 0) : 0
            flatShortArrayValues := []
            index := 0
            for tag, value in exif.ShortArrayTags {
                index += 1
                NumPut("Int", tag, shortArrayTags, (index - 1) * 4)
                NumPut("UPtr", flatShortArrayValues.Length, shortArrayOffsets, (index - 1) * A_PtrSize)
                NumPut("UPtr", value.Length, shortArrayCounts, (index - 1) * A_PtrSize)
                for item in value
                    flatShortArrayValues.Push(item)
            }
            shortArrayValues := flatShortArrayValues.Length ? Buffer(flatShortArrayValues.Length * 4, 0) : 0
            for index, value in flatShortArrayValues
                NumPut("UInt", value, shortArrayValues, (index - 1) * 4)
            byteArrayTags := byteArrayCount ? Buffer(byteArrayCount * 4, 0) : 0
            byteArrayOffsets := byteArrayCount ? Buffer(byteArrayCount * A_PtrSize, 0) : 0
            byteArrayCounts := byteArrayCount ? Buffer(byteArrayCount * A_PtrSize, 0) : 0
            flatByteArrayValues := []
            index := 0
            for tag, value in exif.ByteArrayTags {
                index += 1
                NumPut("Int", tag, byteArrayTags, (index - 1) * 4)
                NumPut("UPtr", flatByteArrayValues.Length, byteArrayOffsets, (index - 1) * A_PtrSize)
                NumPut("UPtr", value.Size, byteArrayCounts, (index - 1) * A_PtrSize)
                loop value.Size
                    flatByteArrayValues.Push(NumGet(value, A_Index - 1, "UChar"))
            }
            byteArrayValues := flatByteArrayValues.Length ? Buffer(flatByteArrayValues.Length, 0) : 0
            for index, value in flatByteArrayValues
                NumPut("UChar", value, byteArrayValues, index - 1)
            uintArrayTags := uintArrayCount ? Buffer(uintArrayCount * 4, 0) : 0
            uintArrayOffsets := uintArrayCount ? Buffer(uintArrayCount * A_PtrSize, 0) : 0
            uintArrayCounts := uintArrayCount ? Buffer(uintArrayCount * A_PtrSize, 0) : 0
            flatUintArrayValues := []
            index := 0
            for tag, value in exif.UintArrayTags {
                index += 1
                NumPut("Int", tag, uintArrayTags, (index - 1) * 4)
                NumPut("UPtr", flatUintArrayValues.Length, uintArrayOffsets, (index - 1) * A_PtrSize)
                NumPut("UPtr", value.Length, uintArrayCounts, (index - 1) * A_PtrSize)
                for item in value
                    flatUintArrayValues.Push(item)
            }
            uintArrayValues := flatUintArrayValues.Length ? Buffer(flatUintArrayValues.Length * 4, 0) : 0
            for index, value in flatUintArrayValues
                NumPut("UInt", value, uintArrayValues, (index - 1) * 4)
            signedRationalTags := signedRationalCount ? Buffer(signedRationalCount * 4, 0) : 0
            signedRationalNumerators := signedRationalCount ? Buffer(signedRationalCount * 4, 0) : 0
            signedRationalDenominators := signedRationalCount ? Buffer(signedRationalCount * 4, 0) : 0
            index := 0
            for tag, value in exif.SignedRationalTags {
                index += 1
                NumPut("Int", tag, signedRationalTags, (index - 1) * 4)
                NumPut("Int", value[1], signedRationalNumerators, (index - 1) * 4)
                NumPut("Int", value[2], signedRationalDenominators, (index - 1) * 4)
            }
            undefinedTags := undefinedCount ? Buffer(undefinedCount * 4, 0) : 0
            undefinedOffsets := undefinedCount ? Buffer(undefinedCount * A_PtrSize, 0) : 0
            undefinedCounts := undefinedCount ? Buffer(undefinedCount * A_PtrSize, 0) : 0
            flatUndefinedValues := []
            index := 0
            for tag, value in exif.UndefinedTags {
                index += 1
                NumPut("Int", tag, undefinedTags, (index - 1) * 4)
                NumPut("UPtr", flatUndefinedValues.Length, undefinedOffsets, (index - 1) * A_PtrSize)
                NumPut("UPtr", value.Size, undefinedCounts, (index - 1) * A_PtrSize)
                loop value.Size
                    flatUndefinedValues.Push(NumGet(value, A_Index - 1, "UChar"))
            }
            undefinedValues := flatUndefinedValues.Length ? Buffer(flatUndefinedValues.Length, 0) : 0
            for index, value in flatUndefinedValues
                NumPut("UChar", value, undefinedValues, index - 1)
            return {
                Tags: tags,
                ValuePtrs: valuePtrs,
                ValueSizes: valueSizes,
                AsciiValueBuffers: valueBuffers,
                AsciiCount: asciiCount,
                IntTags: intTags,
                IntValues: intValues,
                IntTypes: intTypes,
                IntCount: intCount,
                RationalTags: rationalTags,
                RationalNumerators: rationalNumerators,
                RationalDenominators: rationalDenominators,
                RationalCount: rationalCount,
                RationalArrayTags: rationalArrayTags,
                RationalArrayNumerators: rationalArrayNumerators,
                RationalArrayDenominators: rationalArrayDenominators,
                RationalArrayValueCount: flatRationalArrayNumerators.Length,
                RationalArrayOffsets: rationalArrayOffsets,
                RationalArrayCounts: rationalArrayCounts,
                RationalArrayCount: rationalArrayCount,
                ShortArrayTags: shortArrayTags,
                ShortArrayValues: shortArrayValues,
                ShortArrayValueCount: flatShortArrayValues.Length,
                ShortArrayOffsets: shortArrayOffsets,
                ShortArrayCounts: shortArrayCounts,
                ShortArrayCount: shortArrayCount,
                ByteArrayTags: byteArrayTags,
                ByteArrayValues: byteArrayValues,
                ByteArrayValueCount: flatByteArrayValues.Length,
                ByteArrayOffsets: byteArrayOffsets,
                ByteArrayCounts: byteArrayCounts,
                ByteArrayCount: byteArrayCount,
                UintArrayTags: uintArrayTags,
                UintArrayValues: uintArrayValues,
                UintArrayValueCount: flatUintArrayValues.Length,
                UintArrayOffsets: uintArrayOffsets,
                UintArrayCounts: uintArrayCounts,
                UintArrayCount: uintArrayCount,
                SignedRationalTags: signedRationalTags,
                SignedRationalNumerators: signedRationalNumerators,
                SignedRationalDenominators: signedRationalDenominators,
                SignedRationalCount: signedRationalCount,
                UndefinedTags: undefinedTags,
                UndefinedValues: undefinedValues,
                UndefinedValueCount: flatUndefinedValues.Length,
                UndefinedOffsets: undefinedOffsets,
                UndefinedCounts: undefinedCounts,
                UndefinedCount: undefinedCount,
            }
        }

        static PatchTiffExifEntries(pathBytes, exif) {
            if IsObject(exif) && Type(exif) = "Buffer" {
                if exif.Size = 0
                    throw Error("Pillow.Image.Save TIFF exif Buffer must not be empty", -1)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_patch_tiff_exif_bytes",
                    "Ptr", pathBytes,
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int"
                ))
                return
            }
            families := Pillow.Image.ExifFamilyBuffers(exif)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_patch_tiff_exif_entries",
                "Ptr", pathBytes,
                "Ptr", families.Tags,
                "Ptr", families.ValuePtrs,
                "Ptr", families.ValueSizes,
                "UPtr", families.AsciiCount,
                "Ptr", families.IntTags,
                "Ptr", families.IntValues,
                "Ptr", families.IntTypes,
                "UPtr", families.IntCount,
                "Ptr", families.RationalTags,
                "Ptr", families.RationalNumerators,
                "Ptr", families.RationalDenominators,
                "UPtr", families.RationalCount,
                "Ptr", families.RationalArrayTags,
                "Ptr", families.RationalArrayNumerators,
                "Ptr", families.RationalArrayDenominators,
                "UPtr", families.RationalArrayValueCount,
                "Ptr", families.RationalArrayOffsets,
                "Ptr", families.RationalArrayCounts,
                "UPtr", families.RationalArrayCount,
                "Ptr", families.ShortArrayTags,
                "Ptr", families.ShortArrayValues,
                "UPtr", families.ShortArrayValueCount,
                "Ptr", families.ShortArrayOffsets,
                "Ptr", families.ShortArrayCounts,
                "UPtr", families.ShortArrayCount,
                "Ptr", families.ByteArrayTags,
                "Ptr", families.ByteArrayValues,
                "UPtr", families.ByteArrayValueCount,
                "Ptr", families.ByteArrayOffsets,
                "Ptr", families.ByteArrayCounts,
                "UPtr", families.ByteArrayCount,
                "Ptr", families.UintArrayTags,
                "Ptr", families.UintArrayValues,
                "UPtr", families.UintArrayValueCount,
                "Ptr", families.UintArrayOffsets,
                "Ptr", families.UintArrayCounts,
                "UPtr", families.UintArrayCount,
                "Ptr", families.SignedRationalTags,
                "Ptr", families.SignedRationalNumerators,
                "Ptr", families.SignedRationalDenominators,
                "UPtr", families.SignedRationalCount,
                "Ptr", families.UndefinedTags,
                "Ptr", families.UndefinedValues,
                "UPtr", families.UndefinedValueCount,
                "Ptr", families.UndefinedOffsets,
                "Ptr", families.UndefinedCounts,
                "UPtr", families.UndefinedCount,
                "Int"
            ))
        }

        static PatchTiffBigTiffExifEntries(pathBytes, exif) {
            if IsObject(exif) && Type(exif) = "Buffer" {
                if exif.Size = 0
                    throw Error("Pillow.Image.Save TIFF exif Buffer must not be empty", -1)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_patch_tiff_bigtiff_exif_bytes",
                    "Ptr", pathBytes,
                    "Ptr", exif,
                    "UPtr", exif.Size,
                    "Int"
                ))
                return
            }
            families := Pillow.Image.ExifFamilyBuffers(exif)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_patch_tiff_bigtiff_exif_entries",
                "Ptr", pathBytes,
                "Ptr", families.Tags,
                "Ptr", families.ValuePtrs,
                "Ptr", families.ValueSizes,
                "UPtr", families.AsciiCount,
                "Ptr", families.IntTags,
                "Ptr", families.IntValues,
                "Ptr", families.IntTypes,
                "UPtr", families.IntCount,
                "Ptr", families.RationalTags,
                "Ptr", families.RationalNumerators,
                "Ptr", families.RationalDenominators,
                "UPtr", families.RationalCount,
                "Ptr", families.RationalArrayTags,
                "Ptr", families.RationalArrayNumerators,
                "Ptr", families.RationalArrayDenominators,
                "UPtr", families.RationalArrayValueCount,
                "Ptr", families.RationalArrayOffsets,
                "Ptr", families.RationalArrayCounts,
                "UPtr", families.RationalArrayCount,
                "Ptr", families.ShortArrayTags,
                "Ptr", families.ShortArrayValues,
                "UPtr", families.ShortArrayValueCount,
                "Ptr", families.ShortArrayOffsets,
                "Ptr", families.ShortArrayCounts,
                "UPtr", families.ShortArrayCount,
                "Ptr", families.ByteArrayTags,
                "Ptr", families.ByteArrayValues,
                "UPtr", families.ByteArrayValueCount,
                "Ptr", families.ByteArrayOffsets,
                "Ptr", families.ByteArrayCounts,
                "UPtr", families.ByteArrayCount,
                "Ptr", families.UintArrayTags,
                "Ptr", families.UintArrayValues,
                "UPtr", families.UintArrayValueCount,
                "Ptr", families.UintArrayOffsets,
                "Ptr", families.UintArrayCounts,
                "UPtr", families.UintArrayCount,
                "Ptr", families.SignedRationalTags,
                "Ptr", families.SignedRationalNumerators,
                "Ptr", families.SignedRationalDenominators,
                "UPtr", families.SignedRationalCount,
                "Ptr", families.UndefinedTags,
                "Ptr", families.UndefinedValues,
                "UPtr", families.UndefinedValueCount,
                "Ptr", families.UndefinedOffsets,
                "Ptr", families.UndefinedCounts,
                "UPtr", families.UndefinedCount,
                "Int"
            ))
        }

        SaveTiffFrames(path, options) {
            appendOption := Pillow.Image.SaveOption(options, "AppendImages", "append_images")
            exifOption := Pillow.Image.SaveOption(options, "Exif", "exif")
            if exifOption.Set && !((IsObject(exifOption.Value) && exifOption.Value is Pillow.Image.Exif)
                || (IsObject(exifOption.Value) && Type(exifOption.Value) = "Buffer"))
                throw Error("Pillow.Image.Save TIFF exif expects a Pillow.Image.Exif value or Buffer", -1)
            images := [this]
            if appendOption.Set {
                appendImages := appendOption.Value
                if IsObject(appendImages) && appendImages is Pillow.Image {
                    images.Push(appendImages)
                } else if IsObject(appendImages) {
                    for image in appendImages {
                        if !(IsObject(image) && image is Pillow.Image)
                            throw Error("Pillow.Image.Save append_images expects Pillow.Image values", -1)
                        images.Push(image)
                    }
                } else {
                    throw Error("Pillow.Image.Save append_images expects an image or image array", -1)
                }
            }

            pathBytes := Pillow.Image.Utf8Buffer(path)
            handles := Pillow.Image.HandleArray(images)
            if exifOption.Set && images.Length > 1
                throw Error("Pillow.Image.Save TIFF exif currently supports single-frame saves", -1)
            dpiOption := Pillow.Image.SaveOption(options, "Dpi", "dpi")
            compressionOption := Pillow.Image.SaveOption(options, "Compression", "compression")
            iccProfileOption := Pillow.Image.SaveOption(options, "IccProfile", "icc_profile")
            tiffInfoOption := Pillow.Image.SaveOption(options, "TiffInfo", "tiffinfo")
            if tiffInfoOption.Set && exifOption.Set
                exifOption := { Set: false }
            bigTiffOption := Pillow.Image.SaveOption(options, "BigTiff", "big_tiff")
            if bigTiffOption.Set && bigTiffOption.Value {
                if this.Mode != "L" && this.Mode != "RGB" && this.Mode != "RGBA" && this.Mode != "LA"
                    && this.Mode != "CMYK" && this.Mode != "I;16" && this.Mode != "I;16B"
                    && this.Mode != "I" && this.Mode != "F" && this.Mode != "P" && this.Mode != "1"
                    throw Error("Pillow.Image.Save big_tiff currently supports L, RGB, RGBA, LA, CMYK, I;16, I, F, P, and 1 modes", -1)
                compression := compressionOption.Set
                    ? Pillow.Image.SaveTiffCompression(compressionOption.Value)
                    : 1
                numericMode := this.Mode = "CMYK" || this.Mode = "I;16" || this.Mode = "I;16B"
                    || this.Mode = "I" || this.Mode = "F"
                hasMetadata := iccProfileOption.Set || tiffInfoOption.Set || dpiOption.Set || exifOption.Set
                if !hasMetadata {
                    if !numericMode || compression = 1 {
                        Pillow.CheckStatus(DllCall(
                            Pillow.RequireDllPath() "\pillow_c_image_save_tiff_bigtiff_frames_compression_options",
                            "Ptr", handles,
                            "UPtr", images.Length,
                            "Ptr", pathBytes,
                            "Int", compression,
                            "Int"
                        ))
                        return
                    }
                } else if compression = 1 {
                    if tiffInfoOption.Set {
                        if !(tiffInfoOption.Value is Map)
                            throw Error("Pillow.Image.Save tiffinfo expects a Map", -1)
                        if !tiffInfoOption.Value.Has(270)
                            && !tiffInfoOption.Value.Has(315)
                            && !tiffInfoOption.Value.Has(700)
                            throw Error("Pillow.Image.Save tiffinfo currently supports tags 270, 315, and 700", -1)
                        for tag, value in tiffInfoOption.Value {
                            if tag != 270 && tag != 315 && tag != 700
                                throw Error("Pillow.Image.Save tiffinfo currently supports tags 270, 315, and 700", -1)
                        }
                    }
                    xmp := 0
                    if tiffInfoOption.Set && tiffInfoOption.Value.Has(700) {
                        xmp := Pillow.Image.BinaryBuffer(
                            tiffInfoOption.Value[700],
                            "Pillow.Image.Save tiffinfo tag 700"
                        )
                        if xmp.Size = 0
                            throw Error("Pillow.Image.Save tiffinfo tag 700 must not be empty", -1)
                    }
                    asciiTags := []
                    asciiValues := []
                    if tiffInfoOption.Set {
                        for tag in [270, 315] {
                            if !tiffInfoOption.Value.Has(tag)
                                continue
                            if !(tiffInfoOption.Value[tag] is String)
                                throw Error("Pillow.Image.Save tiffinfo tag " tag " expects a string", -1)
                            asciiTags.Push(tag)
                            asciiValues.Push(Pillow.Image.Utf8Buffer(tiffInfoOption.Value[tag]))
                        }
                    }
                    iccProfile := 0
                    if iccProfileOption.Set {
                        iccProfile := Pillow.Image.BinaryBuffer(
                            iccProfileOption.Value,
                            "Pillow.Image.Save icc_profile"
                        )
                        if iccProfile.Size = 0
                            throw Error("Pillow.Image.Save icc_profile must not be empty", -1)
                    }
                    hasDpi := 0
                    dpiX := 0.0
                    dpiY := 0.0
                    if dpiOption.Set {
                        dpi := Pillow.Image.SaveDpiPair(dpiOption.Value)
                        hasDpi := 1
                        dpiX := dpi[1]
                        dpiY := dpi[2]
                    }
                    asciiTagBuffer := Buffer(asciiTags.Length * 4, 0)
                    asciiValuePointers := Buffer(asciiTags.Length * A_PtrSize, 0)
                    asciiValueSizes := Buffer(asciiTags.Length * A_PtrSize, 0)
                    for index, tag in asciiTags {
                        NumPut("Int", tag, asciiTagBuffer, (index - 1) * 4)
                        NumPut("Ptr", asciiValues[index].Ptr, asciiValuePointers, (index - 1) * A_PtrSize)
                        NumPut("UPtr", asciiValues[index].Size, asciiValueSizes, (index - 1) * A_PtrSize)
                    }
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_tiff_bigtiff_frames_metadata_ascii_entries_options",
                        "Ptr", handles,
                        "UPtr", images.Length,
                        "Ptr", pathBytes,
                        "Int", hasDpi,
                        "Double", dpiX,
                        "Double", dpiY,
                        "Int", compression,
                        "Ptr", IsObject(iccProfile) ? iccProfile : 0,
                        "UPtr", IsObject(iccProfile) ? iccProfile.Size : 0,
                        "Ptr", IsObject(xmp) ? xmp : 0,
                        "UPtr", IsObject(xmp) ? xmp.Size : 0,
                        "Ptr", asciiTagBuffer,
                        "Ptr", asciiValuePointers,
                        "Ptr", asciiValueSizes,
                        "UPtr", asciiTags.Length,
                        "Int"
                    ))
                    if exifOption.Set
                        Pillow.Image.PatchTiffBigTiffExifEntries(pathBytes, exifOption.Value)
                    return
                }
                ; Pillow falls back to classic TIFF when big_tiff combines with
                ; compression (libtiff ignores big_tiff), so numeric or
                ; metadata saves with compression reuse the classic writer
                ; below.
            }
            if tiffInfoOption.Set {
                if !(tiffInfoOption.Value is Map)
                    throw Error("Pillow.Image.Save tiffinfo expects a Map", -1)
                if !tiffInfoOption.Value.Has(270)
                    && !tiffInfoOption.Value.Has(315)
                    && !tiffInfoOption.Value.Has(700)
                    throw Error("Pillow.Image.Save tiffinfo currently supports tags 270, 315, and 700", -1)
                for tag, value in tiffInfoOption.Value {
                    if tag != 270 && tag != 315 && tag != 700
                        throw Error("Pillow.Image.Save tiffinfo currently supports tags 270, 315, and 700", -1)
                }
                xmp := 0
                if tiffInfoOption.Value.Has(700) {
                    xmp := Pillow.Image.BinaryBuffer(
                        tiffInfoOption.Value[700],
                        "Pillow.Image.Save tiffinfo tag 700"
                    )
                    if xmp.Size = 0
                        throw Error("Pillow.Image.Save tiffinfo tag 700 must not be empty", -1)
                }
                asciiTags := []
                asciiValues := []
                for tag in [270, 315] {
                    if !tiffInfoOption.Value.Has(tag)
                        continue
                    if !(tiffInfoOption.Value[tag] is String)
                        throw Error("Pillow.Image.Save tiffinfo tag " tag " expects a string", -1)
                    asciiTags.Push(tag)
                    asciiValues.Push(Pillow.Image.Utf8Buffer(tiffInfoOption.Value[tag]))
                }
                iccProfile := 0
                if iccProfileOption.Set {
                    iccProfile := Pillow.Image.BinaryBuffer(
                        iccProfileOption.Value,
                        "Pillow.Image.Save icc_profile"
                    )
                    if iccProfile.Size = 0
                        throw Error("Pillow.Image.Save icc_profile must not be empty", -1)
                }
                hasDpi := 0
                dpiX := 0.0
                dpiY := 0.0
                if dpiOption.Set {
                    dpi := Pillow.Image.SaveDpiPair(dpiOption.Value)
                    hasDpi := 1
                    dpiX := dpi[1]
                    dpiY := dpi[2]
                }
                compression := compressionOption.Set
                    ? Pillow.Image.SaveTiffCompression(compressionOption.Value)
                    : 1
                if asciiTags.Length > 1 {
                    asciiTagBuffer := Buffer(asciiTags.Length * 4, 0)
                    asciiValuePointers := Buffer(asciiTags.Length * A_PtrSize, 0)
                    asciiValueSizes := Buffer(asciiTags.Length * A_PtrSize, 0)
                    for index, tag in asciiTags {
                        NumPut("Int", tag, asciiTagBuffer, (index - 1) * 4)
                        NumPut("Ptr", asciiValues[index].Ptr, asciiValuePointers, (index - 1) * A_PtrSize)
                        NumPut("UPtr", asciiValues[index].Size, asciiValueSizes, (index - 1) * A_PtrSize)
                    }
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_tiff_frames_metadata_ascii_entries_options",
                        "Ptr", handles,
                        "UPtr", images.Length,
                        "Ptr", pathBytes,
                        "Int", hasDpi,
                        "Double", dpiX,
                        "Double", dpiY,
                        "Int", compression,
                        "Ptr", IsObject(iccProfile) ? iccProfile : 0,
                        "UPtr", IsObject(iccProfile) ? iccProfile.Size : 0,
                        "Ptr", IsObject(xmp) ? xmp : 0,
                        "UPtr", IsObject(xmp) ? xmp.Size : 0,
                        "Ptr", asciiTagBuffer,
                        "Ptr", asciiValuePointers,
                        "Ptr", asciiValueSizes,
                        "UPtr", asciiTags.Length,
                        "Int"
                    ))
                    if exifOption.Set
                        Pillow.Image.PatchTiffExifEntries(pathBytes, exifOption.Value)
                    return
                }
                if asciiTags.Length = 1 {
                    Pillow.CheckStatus(DllCall(
                        Pillow.RequireDllPath() "\pillow_c_image_save_tiff_frames_metadata_ascii_options",
                        "Ptr", handles,
                        "UPtr", images.Length,
                        "Ptr", pathBytes,
                        "Int", hasDpi,
                        "Double", dpiX,
                        "Double", dpiY,
                        "Int", compression,
                        "Ptr", IsObject(iccProfile) ? iccProfile : 0,
                        "UPtr", IsObject(iccProfile) ? iccProfile.Size : 0,
                        "Ptr", IsObject(xmp) ? xmp : 0,
                        "UPtr", IsObject(xmp) ? xmp.Size : 0,
                        "Int", asciiTags[1],
                        "Ptr", asciiValues[1],
                        "UPtr", asciiValues[1].Size,
                        "Int"
                    ))
                    if exifOption.Set
                        Pillow.Image.PatchTiffExifEntries(pathBytes, exifOption.Value)
                    return
                }
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_tiff_frames_metadata_ex_options",
                    "Ptr", handles,
                    "UPtr", images.Length,
                    "Ptr", pathBytes,
                    "Int", hasDpi,
                    "Double", dpiX,
                    "Double", dpiY,
                    "Int", compression,
                    "Ptr", IsObject(iccProfile) ? iccProfile : 0,
                    "UPtr", IsObject(iccProfile) ? iccProfile.Size : 0,
                    "Ptr", xmp,
                    "UPtr", xmp.Size,
                    "Int"
                ))
                if exifOption.Set
                    Pillow.Image.PatchTiffExifEntries(pathBytes, exifOption.Value)
                return
            }
            if iccProfileOption.Set {
                iccProfile := Pillow.Image.BinaryBuffer(
                    iccProfileOption.Value,
                    "Pillow.Image.Save icc_profile"
                )
                if iccProfile.Size = 0
                    throw Error("Pillow.Image.Save icc_profile must not be empty", -1)
                hasDpi := 0
                dpiX := 0.0
                dpiY := 0.0
                if dpiOption.Set {
                    dpi := Pillow.Image.SaveDpiPair(dpiOption.Value)
                    hasDpi := 1
                    dpiX := dpi[1]
                    dpiY := dpi[2]
                }
                compression := compressionOption.Set
                    ? Pillow.Image.SaveTiffCompression(compressionOption.Value)
                    : 1
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_tiff_frames_metadata_options",
                    "Ptr", handles,
                    "UPtr", images.Length,
                    "Ptr", pathBytes,
                    "Int", hasDpi,
                    "Double", dpiX,
                    "Double", dpiY,
                    "Int", compression,
                    "Ptr", iccProfile,
                    "UPtr", iccProfile.Size,
                    "Int"
                ))
                if exifOption.Set
                    Pillow.Image.PatchTiffExifEntries(pathBytes, exifOption.Value)
                return
            }
            if dpiOption.Set {
                dpi := Pillow.Image.SaveDpiPair(dpiOption.Value)
                compression := compressionOption.Set
                    ? Pillow.Image.SaveTiffCompression(compressionOption.Value)
                    : 1
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_tiff_frames_options",
                    "Ptr", handles,
                    "UPtr", images.Length,
                    "Ptr", pathBytes,
                    "Int", 1,
                    "Double", dpi[1],
                    "Double", dpi[2],
                    "Int", compression,
                    "Int"
                ))
                if exifOption.Set
                    Pillow.Image.PatchTiffExifEntries(pathBytes, exifOption.Value)
                return
            }
            if compressionOption.Set {
                compression := Pillow.Image.SaveTiffCompression(compressionOption.Value)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_tiff_frames_compression_options",
                    "Ptr", handles,
                    "UPtr", images.Length,
                    "Ptr", pathBytes,
                    "Int", compression,
                    "Int"
                ))
                if exifOption.Set
                    Pillow.Image.PatchTiffExifEntries(pathBytes, exifOption.Value)
                return
            }
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_save_tiff_frames",
                "Ptr", handles,
                "UPtr", images.Length,
                "Ptr", pathBytes,
                "Int"
            ))
            if exifOption.Set
                Pillow.Image.PatchTiffExifEntries(pathBytes, exifOption.Value)
        }

        SaveGifAnimation(path, options) {
            appendOption := Pillow.Image.SaveOption(options, "AppendImages", "append_images")
            images := [this]
            if appendOption.Set {
                appendImages := appendOption.Value
                if IsObject(appendImages) && appendImages is Pillow.Image {
                    images.Push(appendImages)
                } else if IsObject(appendImages) {
                    for image in appendImages {
                        if !(IsObject(image) && image is Pillow.Image)
                            throw Error("Pillow.Image.Save append_images expects Pillow.Image values", -1)
                        images.Push(image)
                    }
                } else {
                    throw Error("Pillow.Image.Save append_images expects an image or image array", -1)
                }
            }

            durationPtr := 0
            durationCount := 0
            durationOption := Pillow.Image.SaveOption(options, "Duration", "duration")
            if durationOption.Set {
                duration := Pillow.Image.SaveIntSequence(durationOption.Value, images.Length, "duration")
                durationPtr := duration.Buffer.Ptr
                durationCount := duration.Count
            }

            disposalPtr := 0
            disposalCount := 0
            disposalOption := Pillow.Image.SaveOption(options, "Disposal", "disposal")
            if disposalOption.Set {
                disposal := Pillow.Image.SaveIntSequence(disposalOption.Value, images.Length, "disposal")
                disposalPtr := disposal.Buffer.Ptr
                disposalCount := disposal.Count
            }

            loopOption := Pillow.Image.SaveOption(options, "Loop", "loop")
            loopCount := loopOption.Set ? loopOption.Value : -1
            if !(loopCount is Integer)
                throw Error("Pillow.Image.Save loop must be an integer", -1)

            includeColorTableOption := Pillow.Image.SaveOption(options, "IncludeColorTable", "include_color_table")
            includeColorTable := -1
            if includeColorTableOption.Set
                includeColorTable := !!includeColorTableOption.Value ? 1 : 0

            optimizeOption := Pillow.Image.SaveOption(options, "Optimize", "optimize")
            optimize := -1
            if optimizeOption.Set
                optimize := !!optimizeOption.Value ? 1 : 0

            transparencyOption := Pillow.Image.SaveOption(options, "Transparency", "transparency")
            hasTransparency := 0
            transparency := 0
            if transparencyOption.Set {
                if !(transparencyOption.Value is Integer)
                    throw Error("Pillow.Image.Save transparency must be an integer", -1)
                hasTransparency := 1
                transparency := transparencyOption.Value
            }

            backgroundOption := Pillow.Image.SaveOption(options, "Background", "background")
            hasBackground := 0
            background := 0
            if backgroundOption.Set {
                if !(backgroundOption.Value is Integer)
                    throw Error("Pillow.Image.Save background must be an integer", -1)
                if backgroundOption.Value < 0 || backgroundOption.Value > 255
                    throw Error("Pillow.Image.Save background must be in 0..255", -1)
                hasBackground := 1
                background := backgroundOption.Value
            }

            commentOption := Pillow.Image.SaveOption(options, "Comment", "comment")
            commentState := Pillow.Image.SaveGifCommentBuffer(commentOption)

            pathBytes := Pillow.Image.Utf8Buffer(path)
            handles := Pillow.Image.HandleArray(images)
            if commentOption.Set && (backgroundOption.Set || includeColorTableOption.Set || optimizeOption.Set) {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_gif_animation_comment_background_options",
                    "Ptr", handles,
                    "UPtr", images.Length,
                    "Ptr", pathBytes,
                    "Ptr", durationPtr,
                    "UPtr", durationCount,
                    "Int", loopCount,
                    "Ptr", disposalPtr,
                    "UPtr", disposalCount,
                    "Int", includeColorTable,
                    "Int", optimize,
                    "Int", hasTransparency,
                    "Int", transparency,
                    "Int", hasBackground,
                    "Int", background,
                    "Ptr", commentState.Buffer,
                    "UPtr", commentState.Size,
                    "Int"
                ))
            } else if backgroundOption.Set {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_gif_animation_background_options",
                    "Ptr", handles,
                    "UPtr", images.Length,
                    "Ptr", pathBytes,
                    "Ptr", durationPtr,
                    "UPtr", durationCount,
                    "Int", loopCount,
                    "Ptr", disposalPtr,
                    "UPtr", disposalCount,
                    "Int", includeColorTable,
                    "Int", optimize,
                    "Int", hasTransparency,
                    "Int", transparency,
                    "Int", hasBackground,
                    "Int", background,
                    "Int"
                ))
            } else if commentOption.Set && transparencyOption.Set {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_gif_animation_comment_metadata_options",
                    "Ptr", handles,
                    "UPtr", images.Length,
                    "Ptr", pathBytes,
                    "Ptr", durationPtr,
                    "UPtr", durationCount,
                    "Int", loopCount,
                    "Ptr", disposalPtr,
                    "UPtr", disposalCount,
                    "Int", includeColorTable,
                    "Int", optimize,
                    "Int", hasTransparency,
                    "Int", transparency,
                    "Ptr", commentState.Buffer,
                    "UPtr", commentState.Size,
                    "Int"
                ))
            } else if transparencyOption.Set {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_gif_animation_metadata_options",
                    "Ptr", handles,
                    "UPtr", images.Length,
                    "Ptr", pathBytes,
                    "Ptr", durationPtr,
                    "UPtr", durationCount,
                    "Int", loopCount,
                    "Ptr", disposalPtr,
                    "UPtr", disposalCount,
                    "Int", includeColorTable,
                    "Int", optimize,
                    "Int", hasTransparency,
                    "Int", transparency,
                    "Int"
                ))
            } else if commentOption.Set {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_gif_animation_comment",
                    "Ptr", handles,
                    "UPtr", images.Length,
                    "Ptr", pathBytes,
                    "Ptr", durationPtr,
                    "UPtr", durationCount,
                    "Int", loopCount,
                    "Ptr", disposalPtr,
                    "UPtr", disposalCount,
                    "Ptr", commentState.Buffer,
                    "UPtr", commentState.Size,
                    "Int"
                ))
            } else if includeColorTableOption.Set || optimizeOption.Set {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_gif_animation_options",
                    "Ptr", handles,
                    "UPtr", images.Length,
                    "Ptr", pathBytes,
                    "Ptr", durationPtr,
                    "UPtr", durationCount,
                    "Int", loopCount,
                    "Ptr", disposalPtr,
                    "UPtr", disposalCount,
                    "Int", includeColorTable,
                    "Int", optimize,
                    "Int"
                ))
            } else {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_save_gif_animation",
                    "Ptr", handles,
                    "UPtr", images.Length,
                    "Ptr", pathBytes,
                    "Ptr", durationPtr,
                    "UPtr", durationCount,
                    "Int", loopCount,
                    "Ptr", disposalPtr,
                    "UPtr", disposalCount,
                    "Int"
                ))
            }
        }

        DataPointer() {
            this.RefreshBufferView()
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
            this.IcnsQuirkPending := false
            if this.Mode = "I" || this.Mode = "F" {
                if IsSet(band)
                    throw Error("image has wrong mode", -1)
                bytes := this.InternalBytes()
                pixelCount := this.Width * this.Height
                values := []
                loop pixelCount {
                    offset := (A_Index - 1) * 4
                    values.Push(this.Mode = "F"
                        ? Pillow.Image.ReadF32(bytes, offset)
                        : Pillow.Image.ReadI32(bytes, offset))
                }
                return values
            }

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
            if !(this.Mode = "P" || this.Mode = "PA" || this.Mode = "L")
                throw Error("illegal image mode", -1)
            rawmode := Pillow.Image.NormalizePaletteRawmode(rawmode, "Pillow.Image.PutPalette", true)
            if rawmode = "RGBA" || rawmode = "RGBX" || rawmode = "BGRX" {
                palette := Pillow.Image.PaletteRgbaBuffer(data, rawmode, "Pillow.Image.PutPalette")
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_put_palette_rgba",
                    "Ptr", this.RequireHandle(),
                    "Ptr", palette,
                    "UPtr", palette.Size,
                    "Int", Pillow.Image.PaletteAlphaModeForPut(rawmode),
                    "Int"
                ))
            } else {
                palette := Pillow.Image.PaletteBuffer(data, rawmode, "Pillow.Image.PutPalette")
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_put_palette_rgb",
                    "Ptr", this.RequireHandle(),
                    "Ptr", palette,
                    "UPtr", palette.Size,
                    "Int"
                ))
            }
            return this
        }

        GetPalette(rawmode := "RGB") {
            if !(this.Mode = "P" || this.Mode = "PA")
                throw Error("illegal image mode", -1)
            rawmode := Pillow.Image.NormalizePaletteRawmode(rawmode, "Pillow.Image.GetPalette", true)
            exportName := "pillow_c_image_get_palette_rgb"
            if rawmode = "RGBX" || rawmode = "BGRX" {
                alphaMode := this.PaletteAlphaMode()
                if alphaMode = 1
                    throw Error("unrecognized raw mode", -1)
            }
            if rawmode = "RGBA" || rawmode = "RGBX"
                exportName := "pillow_c_image_get_palette_rgba"
            required := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\" exportName,
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
                Pillow.RequireDllPath() "\" exportName,
                "Ptr", this.RequireHandle(),
                "Ptr", out,
                "UPtr", out.Size,
                "UPtr*", &required,
                "Int"
            ))
            values := []
            loop out.Size
                values.Push(NumGet(out, A_Index - 1, "UChar"))
            if exportName = "pillow_c_image_get_palette_rgba"
                return Pillow.Image.ConvertRgbaPaletteValues(values, rawmode, "Pillow.Image.GetPalette")
            return Pillow.Image.ConvertRgbPaletteValues(values, rawmode, "Pillow.Image.GetPalette")
        }

        ApplyTransparency() {
            this.RequireHandle()
            if this.Mode != "P" || !this.Info.Has("transparency")
                return

            palette := this.GetPalette("RGBA")
            transparency := this.Info["transparency"]
            if transparency is Integer {
                palette[transparency * 4 + 4] := 0
            } else {
                loop transparency.Size
                    palette[(A_Index - 1) * 4 + 4] := NumGet(transparency, A_Index - 1, "UChar")
            }
            this.PutPalette(palette, "RGBA")
            this.Info.Delete("transparency")
        }

        apply_transparency() {
            return this.ApplyTransparency()
        }

        PaletteAlphaMode() {
            mode := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_palette_alpha_mode",
                "Ptr", this.RequireHandle(),
                "Int*", &mode,
                "Int"
            ))
            return mode
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
            return this.WrapDerivedHandle(outHandle)
        }

        remap_palette(destMap, sourcePalette := unset) {
            if IsSet(sourcePalette)
                return this.RemapPalette(destMap, sourcePalette)
            return this.RemapPalette(destMap)
        }

        Quantize(colors := 256, method := unset, kmeans := 0, palette := unset, dither := unset) {
            resolvedMethod := IsSet(method) ? method : (this.Mode = "RGBA" ? Pillow.Quantize.FASTOCTREE : Pillow.Quantize.MEDIANCUT)
            if this.Mode = "LAB" && !IsSet(palette) && Type(kmeans) = "Float" {
                if kmeans < 0
                    throw Error("kmeans must not be negative", -1)
                throw Error("'float' object cannot be interpreted as an integer", -1)
            }
            if this.Mode = "LAB" && !IsSet(palette) && Type(kmeans) = "String"
                throw Error("'<' not supported between instances of 'str' and 'int'", -1)
            if this.Mode = "LAB" && !IsSet(palette) && kmeans is Integer && kmeans < 0
                throw Error("kmeans must not be negative", -1)
            if this.Mode = "LAB" && (resolvedMethod = Pillow.Quantize.MEDIANCUT || resolvedMethod = Pillow.Quantize.MAXCOVERAGE || resolvedMethod = Pillow.Quantize.FASTOCTREE || resolvedMethod = Pillow.Quantize.LIBIMAGEQUANT) && kmeans is Integer && !IsSet(palette) {
                if this.Width != 0 && this.Height != 0 {
                    if !(colors is Integer) || colors < 1 || colors > 256
                        throw Error("bad number of colors", -1)
                    throw Error("image has wrong mode", -1)
                }
                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_quantize",
                    "Ptr", this.RequireHandle(),
                    "Int", colors,
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return this.WrapDerivedHandle(outHandle)
            }
            if this.Mode = "RGBA" && !(resolvedMethod = Pillow.Quantize.FASTOCTREE || resolvedMethod = Pillow.Quantize.LIBIMAGEQUANT)
                throw Error("Fast Octree (method == 2) and libimagequant (method == 3) are the only valid methods for quantizing RGBA images", -1)
            if IsSet(palette) {
                if !(palette is Pillow.Image)
                    throw Error("bad mode for palette image", -1)
                if palette.Mode != "P"
                    throw Error("bad mode for palette image", -1)
                if !(this.Mode = "RGB" || this.Mode = "L")
                    throw Error("only RGB or L mode images can be quantized to a palette", -1)

                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_quantize_palette",
                    "Ptr", this.RequireHandle(),
                    "Ptr", palette.RequireHandle(),
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return this.WrapDerivedHandle(outHandle)
            }
            if !(colors is Integer) || colors < 1 || colors > 256
                throw Error("bad number of colors", -1)
            if !(kmeans is Integer) || kmeans < 0
                throw Error("kmeans must not be negative", -1)
            if !(resolvedMethod = Pillow.Quantize.MEDIANCUT
                || resolvedMethod = Pillow.Quantize.MAXCOVERAGE
                || resolvedMethod = Pillow.Quantize.FASTOCTREE
                || resolvedMethod = Pillow.Quantize.LIBIMAGEQUANT)
                throw Error("quantization error", -1)
            if resolvedMethod = Pillow.Quantize.LIBIMAGEQUANT
                throw Error("dependency required by this method was not enabled at compile time", -1)
            if !(this.Mode = "RGB" || this.Mode = "L" || this.Mode = "P" || this.Mode = "RGBA")
                throw Error("Pillow.Image.Quantize currently supports L/P/RGB/RGBA images", -1)

            outHandle := 0
            if resolvedMethod = Pillow.Quantize.MEDIANCUT && kmeans = 0 &&
                (this.Mode = "RGB" || this.Mode = "L") {
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_quantize",
                    "Ptr", this.RequireHandle(),
                    "Int", colors,
                    "Ptr*", &outHandle,
                    "Int"
                ))
            } else {
                ditherValue := IsSet(dither) ? dither : Pillow.Dither.FLOYDSTEINBERG
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_quantize_options",
                    "Ptr", this.RequireHandle(),
                    "Int", colors,
                    "Int", resolvedMethod,
                    "Int", kmeans,
                    "Int", ditherValue,
                    "Ptr*", &outHandle,
                    "Int"
                ))
            }
            return this.WrapDerivedHandle(outHandle)
        }

        PutData(data, scale := 1.0, offset := 0.0) {
            this.DetachBufferView()
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
            if this.Mode = "I" {
                if IsObject(value)
                    throw Error("sequence must be flattened", -1)
                Pillow.Image.WriteI32(buf, base, value * scale + offset, "Pillow.Image.PutData")
                return
            }
            if this.Mode = "F" {
                if IsObject(value)
                    throw Error("sequence must be flattened", -1)
                Pillow.Image.WriteF32(buf, base, value * scale + offset, "Pillow.Image.PutData")
                return
            }

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
            this.DetachBufferView()
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
            ; Pillow 11.3.0's C getpixel parses the coordinates as C ints
            ; (strings raise "an integer is required", floats truncate),
            ; wraps negative coordinates once, and raises the exact
            ; IndexError for out-of-range pixels.
            x := xy[1]
            y := xy[2]
            if x is String || y is String
                throw Error("an integer is required", -1)
            if !(x is Number) || !(y is Number)
                throw Error("an integer is required", -1)
            x := Integer(x)
            y := Integer(y)
            if x < 0
                x += this.Width
            if y < 0
                y += this.Height
            if x < 0 || x >= this.Width || y < 0 || y >= this.Height
                throw Error("image index out of range", -1)
            this.IcnsQuirkPending := false
            out := Buffer(this.Channels, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_getpixel",
                "Ptr", this.RequireHandle(),
                "Int", x,
                "Int", y,
                "Ptr", out,
                "UPtr", out.Size,
                "Int"
            ))
            return this.PixelBufferToValue(out)
        }

        PutPixel(xy, value) {
            this.DetachBufferView()
            this.IcnsQuirkPending := false
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
            if this.Mode = "I"
                return Pillow.Image.ReadI32(buf, 0)
            if this.Mode = "F"
                return Pillow.Image.ReadF32(buf, 0)
            if this.Channels = 1
                return NumGet(buf, 0, "UChar")
            values := []
            loop buf.Size
                values.Push(NumGet(buf, A_Index - 1, "UChar"))
            return values
        }

        PixelValueBuffer(value) {
            if value is String
                value := Pillow.ImageColor.GetColor(value, this.Mode)
            if this.Mode = "I" {
                if IsObject(value) {
                    if value.Length != 1
                        throw Error("Pillow.Image.PutPixel color must be int or single-element array", -1)
                    value := value[1]
                }
                buf := Buffer(4, 0)
                Pillow.Image.WriteI32(buf, 0, value, "Pillow.Image.PutPixel")
                return buf
            }
            if this.Mode = "F" {
                if IsObject(value) {
                    if value.Length != 1
                        throw Error("Pillow.Image.PutPixel color must be float or single-element array", -1)
                    value := value[1]
                }
                buf := Buffer(4, 0)
                Pillow.Image.WriteF32(buf, 0, value, "Pillow.Image.PutPixel")
                return buf
            }
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
            if color is String
                color := Pillow.ImageColor.GetColor(color, this.Mode)
            if this.Mode = "I" {
                if IsObject(color) {
                    if color.Length != 1
                        throw Error("Pillow color length must match image channels", -1)
                    color := color[1]
                }
                buf := Buffer(4, 0)
                Pillow.Image.WriteI32(buf, 0, color, "Pillow color")
                return buf
            }
            if this.Mode = "F" {
                if IsObject(color) {
                    if color.Length != 1
                        throw Error("Pillow color length must match image channels", -1)
                    color := color[1]
                }
                buf := Buffer(4, 0)
                Pillow.Image.WriteF32(buf, 0, color, "Pillow color")
                return buf
            }
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
            if color is String
                color := Pillow.ImageColor.GetColor(color, this.Mode)
            if this.Mode = "I" || this.Mode = "F"
                return this.ColorBuffer(color)
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

        ThrowUnsupportedTransformResample(resample) {
            ; Pillow 11.3.0 interpolates I;16 transform storage bytes as
            ; byte channels for BILINEAR/BICUBIC (endian-bug garbage on
            ; I;16B), so those are explicit documented boundaries here.
            if (this.Mode = "I;16" || this.Mode = "I;16B") {
                if resample = Pillow.Resampling.BILINEAR || resample = Pillow.Resampling.BICUBIC
                    throw Error("Pillow.Image.Transform BILINEAR/BICUBIC is not supported for mode " this.Mode, -1)
            }
        }

        TransformFillBuffer(color) {
            if this.Mode = "I;16" || this.Mode = "I;16B" {
                ; Pillow 11.3.0 packs I;16/I;16B transform fill colors as
                ; one uint16 sample (int scalars and single-element tuples
                ; wrap modulo 65536; color names resolve to grayscale).
                if IsObject(color) {
                    if !(color is Array) || color.Length != 1
                        throw Error("color must be int or single-element tuple", -1)
                    color := color[1]
                }
                if color is String {
                    rgb := Pillow.ImageColor.GetRgb(color)
                    if rgb.Length = 4
                        rgb := [rgb[1], rgb[2], rgb[3]]
                    color := (rgb[1] * 19595 + rgb[2] * 38470 + rgb[3] * 7471 + 0x8000) >> 16
                }
                if !(color is Integer)
                    throw Error("color must be int or single-element tuple", -1)
                buf := Buffer(2, 0)
                if this.Mode = "I;16B" {
                    NumPut("UChar", color >> 8, buf, 0)
                    NumPut("UChar", color & 0xFF, buf, 1)
                } else {
                    NumPut("UShort", color, buf, 0)
                }
                return buf
            }
            if this.Mode = "I" || this.Mode = "F" {
                ; Pillow 11.3.0 packs numeric transform fill colors as one
                ; int32/float32 sample: scalars, single-element sequences,
                ; and strings through the grayscale color map.
                if IsObject(color) {
                    if !(color is Array) || color.Length != 1
                        throw Error(this.Mode = "I" ? "color must be int or single-element tuple" : "must be real number, not tuple", -1)
                    color := color[1]
                }
                if color is String {
                    rgb := Pillow.ImageColor.GetRgb(color)
                    if rgb.Length = 4
                        rgb := [rgb[1], rgb[2], rgb[3]]
                    color := (rgb[1] * 19595 + rgb[2] * 38470 + rgb[3] * 7471 + 0x8000) >> 16
                }
                if !(color is Number)
                    throw Error(this.Mode = "I" ? "color must be int or single-element tuple" : "must be real number, not tuple", -1)
                buf := Buffer(4, 0)
                if this.Mode = "I" {
                    if !(color is Integer)
                        throw Error("color must be int or single-element tuple", -1)
                    NumPut("Int", Integer(color), buf, 0)
                } else {
                    NumPut("Float", Float(color), buf, 0)
                }
                return buf
            }
            if color is String
                color := Pillow.ImageColor.GetColor(color, this.Mode)
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
            if this.Mode = "I" || this.Mode = "F" {
                ; Pillow 11.3.0 rejects list tables on numeric modes with
                ; "point operation not supported for this mode" and routes
                ; LINEAR callables through point_transform(scale, offset).
                if !(lut is Func)
                    throw Error("point operation not supported for this mode", -1)
                if IsSet(modeName)
                    throw Error("point operation not supported for this mode", -1)
                v0 := lut.Call(0)
                v1 := lut.Call(1)
                v2 := lut.Call(2)
                if v1 - v0 != v2 - v1
                    throw Error("point operation not supported for this mode", -1)
                scale := v1 - v0
                offset := v0
                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_point_transform",
                    "Ptr", this.RequireHandle(),
                    "Double", scale,
                    "Double", offset,
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return this.WrapDerivedHandle(outHandle)
            }
            if this.Mode = "I;16"
                throw Error("point operation not supported for this mode", -1)
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
            return this.WrapDerivedHandle(outHandle)
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
            if !IsObject(lut) {
                if lut is String
                    ; Pillow's C point tries to round a non-sequence LUT
                    throw Error("type str doesn't define __round__ method", -1)
                throw Error("Pillow.Image.Point expects an array LUT", -1)
            }
            expected := this.Channels * 256
            if lut.Length != expected
                throw Error("wrong number of lut entries", -1)
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
            return this.WrapDerivedHandle(outHandle)
        }

        Histogram(mask := unset) {
            if this.Mode = "I;16" || this.Mode = "I;16B"
                ; Pillow 11.3.0's C histogram reads 2-byte I;16 storage
                ; through byte/word misreads, producing layout-dependent
                ; bins; this runtime fails loudly instead (documented
                ; boundary). getextrema/convert cover the useful paths.
                throw Error("Pillow.Image.Histogram is not supported for mode " this.Mode, -1)
            count := Pillow.Image.GetModeBands(this.Mode) * 256
            out := Buffer(count * 8, 0)
            if IsSet(mask) {
                if !(IsObject(mask) && mask is Pillow.Image)
                    throw Error("Pillow.Image.Histogram mask expects a Pillow.Image", -1)
                if !(mask.Mode = "1" || mask.Mode = "L")
                    throw Error("bad transparency mask", -1)
                if mask.Width != this.Width || mask.Height != this.Height
                    throw Error("images do not match", -1)
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
            if this.Mode = "I;16" || this.Mode = "I;16B"
                ; Pillow 11.3.0's C entropy reads 2-byte I;16 storage
                ; through byte misreads (layout-dependent log2 values);
                ; this runtime fails loudly instead (documented boundary).
                throw Error("Pillow.Image.Entropy is not supported for mode " this.Mode, -1)
            value := 0.0
            maskHandle := 0
            if IsSet(mask) {
                if !(IsObject(mask) && mask is Pillow.Image)
                    throw Error("Pillow.Image.Entropy mask expects a Pillow.Image", -1)
                if !(mask.Mode = "1" || mask.Mode = "L")
                    throw Error("bad transparency mask", -1)
                if mask.Width != this.Width || mask.Height != this.Height
                    throw Error("images do not match", -1)
                maskHandle := mask.RequireHandle()
            }
            status := DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_entropy",
                "Ptr", this.RequireHandle(),
                "Ptr", maskHandle,
                "Double*", &value,
                "Int"
            )
            if status = -3 && IsSet(mask) && (this.Mode = "I" || this.Mode = "F")
                throw Error("image has wrong mode", -1)
            Pillow.CheckStatus(status)
            return value
        }

        GetExtrema() {
            if this.Mode = "I;16B"
                ; Pillow 11.3.0 raises "image has wrong mode" for I;16B
                ; getextrema() (documented boundary).
                throw Error("image has wrong mode", -1)
            if this.Mode = "I" || this.Mode = "F" || this.Mode = "I;16" {
                bandCount := Pillow.Image.GetModeBands(this.Mode)
                minBuf := Buffer(bandCount * 8, 0)
                maxBuf := Buffer(bandCount * 8, 0)
                hasBuf := Buffer(bandCount, 0)
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_get_extrema_numeric",
                    "Ptr", this.RequireHandle(),
                    "Ptr", minBuf,
                    "Ptr", maxBuf,
                    "Ptr", hasBuf,
                    "UPtr", bandCount,
                    "Int"
                ))
                if !NumGet(hasBuf, 0, "UChar")
                    return 0
                if this.Mode = "F"
                    return [
                        NumGet(minBuf, 0, "Double"),
                        NumGet(maxBuf, 0, "Double"),
                    ]
                return [
                    Integer(NumGet(minBuf, 0, "Double")),
                    Integer(NumGet(maxBuf, 0, "Double")),
                ]
            }

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
            if this.Mode = "I;16" || this.Mode = "I;16B"
                ; Pillow 11.3.0 raises "image has wrong mode" for I;16
                ; getcolors() (documented boundary parity).
                throw Error("image has wrong mode", -1)
            if this.Mode = "I" || this.Mode = "F"
                return this.GetColorsNumeric(maxcolors)

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

        GetColorsNumeric(maxcolors) {
            count := 0
            exceeded := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_getcolors_numeric",
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
            values := Buffer(count * 8, 0)
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_getcolors_numeric",
                "Ptr", this.RequireHandle(),
                "Int", maxcolors,
                "Ptr", counts,
                "Ptr", values,
                "UPtr", count,
                "UPtr*", &count,
                "Int*", &exceeded,
                "Int"
            ))
            if exceeded
                return 0
            return this.NumericColorCountsToArray(counts, values, count)
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

        NumericColorCountsToArray(counts, values, count) {
            out := []
            loop count {
                itemIndex := A_Index - 1
                value := NumGet(values, itemIndex * 8, "Double")
                if this.Mode = "I"
                    value := Round(value)
                out.Push([NumGet(counts, itemIndex * 8, "Int64"), value])
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
                bands.Push(this.WrapDerivedHandle(NumGet(outHandles, (A_Index - 1) * A_PtrSize, "Ptr")))
            return bands
        }

        GetBands() {
            names := this.BandNames()
            return names.Clone()
        }

        BandNames() {
            return Pillow.Image.GetModeBandNames(this.Mode)
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
            return this.WrapDerivedHandle(outHandle)
        }

        Copy() {
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_copy",
                "Ptr", this.RequireHandle(),
                "Ptr*", &outHandle,
                "Int"
            ))
            return this.WrapDerivedHandle(outHandle)
        }

        WrapDerivedHandle(handle) {
            image := Pillow.WrapImageHandle(handle)
            image.Info := Pillow.Image.CopyInfo(this.Info)
            return image
        }

        static CopyInfo(info) {
            copied := Map()
            for key, value in info
                copied[key] := value
            return copied
        }

        Crop(box := unset) {
            if !IsSet(box)
                return this.Copy()
            this.IcnsQuirkPending := false
            if !IsObject(box) || box.Length != 4
                throw Error("Pillow.Image.Crop expects box [left, top, right, bottom]", -1)
            cropBox := []
            for value in box {
                if !(value is Number)
                    throw Error("Pillow.Image.Crop box coordinates must be numeric", -1)
                cropBox.Push(Pillow.Image.RoundHalfEven(value))
            }
            ; Pillow 11.3.0 validates the box before the C crop (the exact
            ; ValueErrors; right == left is the 'right' message).
            if cropBox[3] < cropBox[1]
                throw Error("Coordinate 'right' is less than 'left'", -1)
            if cropBox[4] < cropBox[2]
                throw Error("Coordinate 'lower' is less than 'upper'", -1)

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
            return this.WrapDerivedHandle(outHandle)
        }

        Resize(size, resample := unset, box := unset, reducingGap := unset) {
            if !IsObject(size) || size.Length != 2
                throw Error("Pillow.Image.Resize expects size [width, height]", -1)
            ; Pillow 11.3.0's C resize parses each size element as a C int
            ; (the exact TypeErrors) and rejects unknown resample filters
            ; with the exact ValueError naming the value.
            for value in size {
                if value is String
                    throw Error("'str' object cannot be interpreted as an integer", -1)
                if !(value is Integer)
                    throw Error("'float' object cannot be interpreted as an integer", -1)
            }
            this.IcnsQuirkPending := false
            if !IsSet(resample)
                ; Pillow 11.3.0: NEAREST only for BGR;-prefixed modes,
                ; BICUBIC otherwise; the native layer forces NEAREST for
                ; mode 1/P and applies the RGBA/LA premultiply roundtrip.
                resample := Pillow.Resampling.BICUBIC
            if !(resample is Integer) || resample < -1 || resample > 5 {
                resampleText := resample is String ? resample : String(resample)
                throw Error("Unknown resampling filter (" resampleText "). Use Image.Resampling.NEAREST (0), Image.Resampling.LANCZOS (1), Image.Resampling.BILINEAR (2), Image.Resampling.BICUBIC (3), Image.Resampling.BOX (4) or Image.Resampling.HAMMING (5)", -1)
            }
            if this.Mode = "I;16B" && resample != Pillow.Resampling.NEAREST
                ; Pillow 11.3.0 accepts I;16B non-NEAREST resize but garbles
                ; every sample through its endian handling; replicating that
                ; corruption is declined, so this is a documented boundary.
                throw Error("Pillow.Image.Resize BILINEAR/BICUBIC is not supported for mode I;16B", -1)

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
                if (this.Mode = "I;16" || this.Mode = "I;16B") && resample != Pillow.Resampling.NEAREST {
                    ; Pillow 11.3.0's reduce() rejects special modes with
                    ; "image has wrong mode" whenever a reducing step is
                    ; actually needed.
                    factorX := Max(1, Integer(((box[3] - box[1]) / size[1]) / reducingGap))
                    factorY := Max(1, Integer(((box[4] - box[2]) / size[2]) / reducingGap))
                    if factorX > 1 || factorY > 1
                        throw Error("image has wrong mode", -1)
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
            return this.WrapDerivedHandle(outHandle)
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
            return this.WrapDerivedHandle(outHandle)
        }

        ReduceFactor(factor) {
            if IsObject(factor) {
                if factor.Length != 2
                    throw Error("Pillow.Image.Reduce factor must be an integer or [x, y]", -1)
                for value in factor {
                    if value is String
                        throw Error("'str' object cannot be interpreted as an integer", -1)
                    if !(value is Integer)
                        throw Error("'float' object cannot be interpreted as an integer", -1)
                    if value <= 0
                        throw Error("scale must be > 0", -1)
                }
                return [factor[1], factor[2]]
            }
            if factor is String
                throw Error("'str' object cannot be interpreted as an integer", -1)
            if !(factor is Integer)
                throw Error("'float' object cannot be interpreted as an integer", -1)
            if factor <= 0
                throw Error("scale must be > 0", -1)
            return [factor, factor]
        }

        ReduceBox(box) {
            if !IsObject(box) || box.Length != 4
                throw Error("Pillow.Image.Reduce box expects [left, top, right, bottom]", -1)
            return box
        }

        Filter(filter) {
            if IsObject(filter) && HasMethod(filter, "Apply") {
                image := filter.Apply(this)
                if IsObject(image) && image is Pillow.Image
                    image.Info := Pillow.Image.CopyInfo(this.Info)
                return image
            }
            throw Error("filter argument should be ImageFilter.Filter instance or class", -1)
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
            this.ThrowUnsupportedTransformResample(resample)
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
            return this.WrapDerivedHandle(outHandle)
        }

        TransformQuad(size, corners, resample := unset, fillcolor := unset) {
            if size.Length != 2
                throw Error("Pillow.Image.Transform QUAD expects size [width, height]", -1)
            if corners.Length < 8
                throw Error("Pillow.Image.Transform QUAD expects at least 8 corner values", -1)
            if !IsSet(resample)
                resample := Pillow.Resampling.NEAREST
            this.ThrowUnsupportedTransformResample(resample)
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
            return this.WrapDerivedHandle(outHandle)
        }

        TransformMesh(size, mesh, resample := unset, fillcolor := unset) {
            if size.Length != 2
                throw Error("Pillow.Image.Transform MESH expects size [width, height]", -1)
            if !IsObject(mesh)
                throw Error("Pillow.Image.Transform MESH expects an array of [box, quad] entries", -1)
            if !IsSet(resample)
                resample := Pillow.Resampling.NEAREST
            this.ThrowUnsupportedTransformResample(resample)

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
            return this.WrapDerivedHandle(outHandle)
        }

        TransformAffine(size, matrix, resample := unset, fillcolor := unset) {
            if size.Length != 2
                throw Error("Pillow.Image.TransformAffine expects size [width, height]", -1)
            if matrix.Length != 6
                throw Error("Pillow.Image.TransformAffine expects a 6-value affine matrix", -1)
            if !IsSet(resample)
                resample := Pillow.Resampling.NEAREST
            this.ThrowUnsupportedTransformResample(resample)
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
            return this.WrapDerivedHandle(outHandle)
        }

        Rotate(angle, resample := unset, expand := false, center := unset, translate := unset, fillcolor := unset) {
            if !(angle is Number)
                throw Error("Pillow.Image.Rotate angle must be numeric", -1)
            if !IsSet(resample)
                resample := Pillow.Resampling.NEAREST
            this.ThrowUnsupportedTransformResample(resample)
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
            return this.WrapDerivedHandle(outHandle)
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

        alpha_composite(image, dest := unset, source := unset) {
            if IsSet(dest) {
                if IsSet(source)
                    return this.AlphaComposite(image, dest, source)
                return this.AlphaComposite(image, dest)
            }
            return this.AlphaComposite(image)
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
                if IsObject(source) && source.Length != 1 {
                    if this.Mode = "I"
                        throw Error("color must be int or single-element tuple", -1)
                    if this.Mode = "F"
                        throw Error("must be real number, not tuple", -1)
                }
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
            ; Pillow 11.3.0's transpose parses the method as a C int (the
            ; exact TypeErrors) and rejects unknown operations with the
            ; exact ValueError.
            if method is String
                throw Error("'str' object cannot be interpreted as an integer", -1)
            if !(method is Integer)
                throw Error("'float' object cannot be interpreted as an integer", -1)
            if method < 0 || method > 6
                throw Error("No such transpose operation", -1)
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_transpose",
                "Ptr", this.RequireHandle(),
                "Int", method,
                "Ptr*", &outHandle,
                "Int"
            ))
            return this.WrapDerivedHandle(outHandle)
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
            return this.WrapDerivedHandle(outHandle)
        }

        effect_spread(distance) {
            return this.EffectSpread(distance)
        }

        Convert(modeName, matrixOrDither := unset, dither := unset, palette := unset, colors := unset) {
            this.IcnsQuirkPending := false
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
                return this.WrapDerivedHandle(outHandle)
            }
            if this.Mode = "LAB" && (targetMode = 5 || targetMode = 16)
                throw Error("conversion from LAB to RGB not supported", -1)
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
                return this.WrapDerivedHandle(outHandle)
            }
            if this.Mode = "LAB" && targetMode = 6 {
                resolvedPalette := IsSet(palette) ? palette : Pillow.Palette.WEB
                resolvedColors := IsSet(colors) ? colors : 256
                if resolvedPalette != Pillow.Palette.ADAPTIVE
                    throw Error("conversion from LAB to RGB not supported", -1)
                if this.Width != 0 && this.Height != 0 {
                    if !(resolvedColors is Integer) || resolvedColors < 1 || resolvedColors > 256
                        throw Error("bad number of colors", -1)
                    throw Error("image has wrong mode", -1)
                }
                outHandle := 0
                Pillow.CheckStatus(DllCall(
                    Pillow.RequireDllPath() "\pillow_c_image_quantize",
                    "Ptr", this.RequireHandle(),
                    "Int", resolvedColors,
                    "Ptr*", &outHandle,
                    "Int"
                ))
                return this.WrapDerivedHandle(outHandle)
            }
            if IsSet(matrixOrDither) || IsSet(dither)
                throw Error("Pillow.Image.Convert dither is currently supported only for mode 1", -1)
            if this.Mode = "LAB" && (targetMode = 1 || targetMode = 2 || targetMode = 6 || targetMode = 7 || targetMode = 8 || targetMode = 9 || targetMode = 13 || targetMode = 14)
                throw Error("conversion from LAB to RGB not supported", -1)
            outHandle := 0
            Pillow.CheckStatus(DllCall(
                Pillow.RequireDllPath() "\pillow_c_image_convert_mode",
                "Ptr", this.RequireHandle(),
                "Int", targetMode,
                "Ptr*", &outHandle,
                "Int"
            ))
            return this.WrapDerivedHandle(outHandle)
        }
    }
}
