# Match the Inline Hook Invalidate v1.0 config parser. Each target line is an
# exact nice_name, so a package and its private processes are distinct.
{
    sub(/\r+$/, "")
}
NR == 1 {
    split($0, header, ":")
    enabled = header[1] == "1"
    library = header[2] == "" ? "libart.so" : header[2]
    method = header[3] == "pread" ? "pread" : "mmap"
    next
}
$0 != "" {
    selected[$0] = 1
}
END {
    if (NR == 0) {
        print "inline_config=empty"
        exit 1
    }
    if (library ~ /[^A-Za-z0-9_.\/-]/) library = "(custom name omitted)"
    printf "inline_config=%s library=%s method=%s\n", enabled ? "enabled" : "disabled", library, method
    count = split(targets, names, " ")
    for (i = 1; i <= count; i++) {
        process = names[i]
        printf "%s listed=%s configured_remap=%s\n", process,
               process in selected ? "yes" : "no", enabled && process in selected ? "yes" : "no"
    }
}
