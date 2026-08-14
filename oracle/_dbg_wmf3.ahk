#Requires AutoHotkey v2.0
bbox := [0, 0, 100, 100]
out := ""
for i in [1, 2, 3, 4] {
    value := bbox[i]
    out .= value ","
}
FileAppend("values: " out "`n", "*")
out2 := ""
loop 4 {
    value := bbox[A_Index]
    out2 .= value ","
}
FileAppend("loop: " out2 "`n", "*")
