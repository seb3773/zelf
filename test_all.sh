#!/bin/bash
TITLE_EXTRA=""
EXTRA=""
if [[ "$1" == "-big" ]]; then
EXTRA="-big"
TITLE_EXTRA=" {BIG}"
fi
sep="#---------------------------------------------------------------------#"
echo -e "\e[48;5;21m\e[37m===================== Quick all codecs test$TITLE_EXTRA =====================\e[0m"
runtest() {
local name="$1"
local flag="$2"
echo -e "\n$sep"
echo "➤ $name"
./build_and_test.sh "$flag" -nobuild $EXTRA | awk '
/Ratio:/ {
match($0, /Ratio:[[:space:]]*[0-9.]+%/, r)
last_ratio = r[0]
next
}
/OK|FAILED/ {
if ($0 ~ /UNPACKED/) {
# Pas de ratio pour les tests UNPACKED
print
} else if (last_ratio != "") {
sub(/OK|FAILED/, "(" last_ratio ") &")
print
} else {
print
}
}
'
}

runtest "lz4"       -lz4
runtest "nanozip"   -nz1
runtest "lzfse"     -lzfse
runtest "csc"       -csc
runtest "rnc"       -rnc
runtest "density"   -dsty
runtest "lzham"     -lzham
runtest "apultra"   -apultra
runtest "zx7b"      -zx7b
runtest "blz"       -blz
runtest "exo"       -exo
runtest "pp"        -pp
runtest "snappy"    -snappy
runtest "doboz"     -doboz
runtest "qlz"       -qlz
runtest "lzav"      -lzav
runtest "lzma"      -lzma
runtest "zstd"      -zstd
runtest "shrinkler" -shr
runtest "stcr"      -stcr
runtest "lzsa2"     -lzsa
runtest "zx0"       -zx0
echo -e "\n$sep"
echo -e "\e[48;5;21m\e[37m====================== Completed. ==========================\e[0m"
