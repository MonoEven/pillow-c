#Requires AutoHotkey v2.0
head := Buffer(22, 0)
bbox := [0, 0, 100, 100]
for i in [1, 2, 3, 4] {
    value := bbox[i]
    NumPut("Short", value, head, 4 + (i - 1) * 2)
}
out := ""
loop 8
    out .= NumGet(head, 6 + A_Index - 1, "UChar") ","
FileAppend(out "`n", "*")
