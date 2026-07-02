#!/bin/sh

SRC="./common/language.c"
OUT="${1:?usage: $0 <output json path>}"

[ -f "$SRC" ] || {
	echo "gen_lang.sh: $SRC not found (run from the frontend repo root)" >&2
	exit 1
}

# I hate awk at the best of times...
awk '
    /^static const lang_field lang_fields\[\] = \{/ { intable = 1; next }
    intable && /^\/\/ clang-format on/ { intable = 0 }
    !intable { next }
    !/^    \{/ { next }
    {
        split($0, f, "\"")
        module = f[2]
        type = f[3]
        str = f[4]

        sub(/^, LANG_OFF\([^)]*\), /, "", type)
        sub(/,[ \t]*$/, "", type)

        if (type == "lang_system") next
        if (seen[module SUBSEP str]++) next

        keys[module] = keys[module] str "\n"
        if (!(module in modseen)) { modorder[++nmod] = module; modseen[module] = 1 }
    }
    END {
        for (i = 1; i <= nmod; i++) msort[i] = modorder[i]
        for (i = 2; i <= nmod; i++) {
            v = msort[i]; j = i - 1
            while (j >= 1 && msort[j] > v) { msort[j + 1] = msort[j]; j-- }
            msort[j + 1] = v
        }

        printf "{\n"
        for (i = 1; i <= nmod; i++) {
            m = msort[i]

            delete sortk
            nk = split(keys[m], arr, "\n")
            realn = 0
            for (k = 1; k <= nk; k++) if (arr[k] != "") sortk[++realn] = arr[k]
            for (a = 2; a <= realn; a++) {
                v = sortk[a]; b = a - 1
                while (b >= 1 && sortk[b] > v) { sortk[b + 1] = sortk[b]; b-- }
                sortk[b + 1] = v
            }

            printf "  \"%s\": {\n", m
            for (k = 1; k <= realn; k++) {
                printf "    \"%s\": \"%s\"%s\n", sortk[k], sortk[k], (k < realn ? "," : "")
            }
            printf "  }%s\n", (i < nmod ? "," : "")

            delete arr
        }
        printf "}\n"
    }
' "$SRC" >"$OUT"
