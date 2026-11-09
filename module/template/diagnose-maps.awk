# Android awk lacks a portable hexadecimal conversion function.
function hex(value,    i, digit, result) {
    result = 0
    for (i = 1; i <= length(value); ++i) {
        digit = index("0123456789abcdef", tolower(substr(value, i, 1))) - 1
        if (digit < 0) return -1
        result = result * 16 + digit
    }
    return result
}

$2 ~ /x/ {
    label = ""
    if (NF < 6) label = "ANONYMOUS"
    else if ($6 == "/memfd:jit-cache" || $6 == "/memfd:jit-zygote-cache") label = "ART_JIT"
    else if ($6 ~ /^\[anon(_shmem)?:/) label = "NAMED_ANONYMOUS"
    else if ($0 ~ /zygisk|magisk|lsposed|vector|\/data\/adb/) label = "NAMED_MODULE"
    else if ($6 ~ /^\/memfd:/) label = "OTHER_MEMFD"
    if (label == "") next
    split($1, range, "-")
    start = hex(range[1])
    end = hex(range[2])
    if (start < 0 || end <= start) next
    counts[label]++
    if (++rows <= 80) printf "%s %s %.0f %s\n", label, $2, end - start, $0
}
END {
    printf "anonymous_executable_regions=%d named_anonymous=%d art_jit=%d named_modules=%d other_memfd=%d\n",
           counts["ANONYMOUS"], counts["NAMED_ANONYMOUS"], counts["ART_JIT"], counts["NAMED_MODULE"], counts["OTHER_MEMFD"]
    if (rows > 80) print "Mapping listing capped at 80 rows."
}
