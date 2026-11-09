# Version 1.0 uses exact target lines. The fork added one-shot mode in code 2
# and per-process mode directives in code 3.
function valid_method(value) {
    return value == "mmap" || value == "mmap-once" || value == "pread"
}
{
    sub(/\r+$/, "")
}
NR == 1 {
    fields = split($0, header, ":")
    enabled = header[1] == "1"
    library = header[2] == "" ? "libart.so" : header[2]
    method = header[3] == "pread" ? "pread" : "mmap"
    if (version_code >= 2) {
        if (fields > 3 || (header[1] != "0" && header[1] != "1") ||
            (header[3] != "" && !valid_method(header[3]))) invalid = 1
        if (header[3] == "mmap-once") method = "mmap-once"
    }
    next
}
version_code >= 3 && /^@mode / {
    directive = substr($0, 7)
    sub(/^[[:space:]]+/, "", directive)
    sub(/[[:space:]]+$/, "", directive)
    if (split(directive, parts, /[[:space:]]+/) != 2 ||
        !valid_method(parts[2]) || parts[1] in overrides) invalid = 1
    else overrides[parts[1]] = parts[2]
    next
}
$0 != "" {
    selected[$0] = 1
}
END {
    if (NR == 0 || invalid) {
        print "inline_config=" (NR == 0 ? "empty" : "invalid")
        exit 1
    }
    if (library ~ /[^A-Za-z0-9_.\/-]/) library = "(custom name omitted)"
    printf "inline_config=%s library=%s method=%s\n", enabled ? "enabled" : "disabled", library, method
    count = split(targets, names, " ")
    for (i = 1; i <= count; i++) {
        process = names[i]
        listed = process in selected
        process_method = process in overrides ? overrides[process] : method
        printf "%s listed=%s configured_remap=%s method=%s\n", process,
               listed ? "yes" : "no", enabled && listed ? "yes" : "no", listed ? process_method : "none"
    }
}
