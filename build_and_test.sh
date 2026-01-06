#!/bin/bash

#~~ Binaries for tests ~~~~~~~~~~~~~~~~~~~~~
normal_dyn="./tools/binaries_sample/ls"
normal_static="./tools/binaries_sample/simple_static"
dynexec_test_bin="./tools/binaries_sample/dynexec_probe"
big_dyn="./tools/binaries_sample/qemu-system-aarch64"
big_static="./tools/binaries_sample/rclone"
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


export LC_ALL=C
export LANG=C
packing="-lz4"
FULL=0
NOBUILD=0
SPEEDTEST=0
BIG=0
SUMMARY=0
dyn_bin="$normal_dyn"
stat_bin="$normal_static"
big_tag() {
if [ $BIG -eq 1 ]; then
echo -n " {BIG}"
fi
}
dynamic_none_packing=""
dynamic_none_unpacking=""
dynamic_bcj_packing=""
dynamic_bcj_unpacking=""
dynamic_kexe_packing=""
dynamic_kexe_unpacking=""
static_none_packing=""
static_none_unpacking=""
static_bcj_packing=""
static_bcj_unpacking=""
static_kexe_packing=""
static_kexe_unpacking=""
dynexec_none_packing=""
dynexec_none_unpacking=""
dynexec_bcj_packing=""
dynexec_bcj_unpacking=""
dynexec_kexe_packing=""
dynexec_kexe_unpacking=""
archive_packing=""
archive_unpacking=""
show_help() {
echo -e "│\n│ Usage: $0 <Packer> [options]
│
│ Packer (mandatory):
│   -pp / -snappy / -lz4 / -apultra / -exo
│   -zx0 / -doboz / -qlz / -lzav / -lzma
│   -zstd / -shr / -zx0 / -blz / -stcr
│   -lzsa / -dsty / -lzham / -rnc / -lzfse
│   -csc
│   -nz1
│ 
│  Options (optionnal):
│   -full         Build fully (no tail)
│   -nobuild      Skip 'make clean' and 'make'
│   -speedtest    Speedtest mode
│   -big          Use bigger binaries (qemu-system-aarch64 and rclone) 
│                 than default ones
│   -summary      Display a summary table of test results at the end
│   -clean        Remove all generated files during tests and exit
│   -help, --help Display this help message
│ 
│ Examples:
│   $0 -lz4 -full
│   $0 -zstd -nobuild -speedtest
│   $0 -lzfse -big -nobuild -full
│ "
echo -e "\e[48;5;94m\e[97m ----------------------------------------------------------------------------------------------------- \e[0m\n"
}
clean_all() {
echo -e "\n\033[37;44m❯ Nettoyage...\033[0m\n"
rm -f ./packed_binary_bcj
rm -f ./packed_binary_bcj_unpacked
rm -f ./packed_binary_kexe
rm -f ./packed_binary_kexe_unpacked
rm -f ./packed_binary_none
rm -f ./packed_binary_none_unpacked
rm -f ./packed_dynexec_binary_none
rm -f ./packed_dynexec_binary_none_unpacked
rm -f ./packed_dynexec_binary_bcj
rm -f ./packed_dynexec_binary_bcj_unpacked
rm -f ./packed_dynexec_binary_kexe
rm -f ./packed_dynexec_binary_kexe_unpacked
rm -f ./trace_dyn.txt
rm -f ./trace_static.txt
echo -e "\n\033[37;43m✔ Nettoyage complet\033[0m\n"
}

echo -e "\n\e[48;5;33m\e[97m                -= Zelf packer shell script tester =-              \e[0m"
echo -e "\e[48;5;94m\e[97m ----------------------------------------------------------------------------------------------------- \e[0m"

if [ $# -eq 0 ]; then
show_help
exit 0
fi

for a in "$@"; do
case "$a" in
-help|--help)
show_help
exit 0
;;
-clean)
clean_all
exit 0
;;
esac
done

for arg in "$@"; do
case "$arg" in
-full)
	FULL=1
	;;
-nobuild)
	NOBUILD=1
	;;
-speedtest)
	SPEEDTEST=1
	;;
-big)
	BIG=1
	;;
-summary)
	SUMMARY=1
	;;
-pp|-snappy|-lz4|-apultra|-exo|-doboz|-qlz|-lzav|-lzma|-zstd|-shr|-zx0|-blz|-stcr|-lzsa|-zx7b|-dsty|-lzham|-rnc|-lzfse|-csc|-nz1)
	packing="$arg"
	;;
*)
	echo -e "\e[48;5;88m\e[97m⚠ Unknown argument: '$arg' \e[0m"
	show_help
	exit 1
	;;
esac
done
if [ $BIG -eq 1 ]; then
dyn_bin="$big_dyn"
stat_bin="$big_static"
else
dyn_bin="$normal_dyn"
stat_bin="$normal_static"
fi


#  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ BUILD

if [ $NOBUILD -eq 0 ]; then
    echo -e "\e[0m"; echo -e "\e[44m                                      || Cleaning before make  "
    rm -f ./build/*
    make clean
    echo -e "--------------------------------------------------------------------------------------------------------------------\e[0m"
    echo; echo -e "\e[48;5;22m                                        || make...  "
    if [ $FULL -eq 1 ]; then
        make
    else
        make | tail -n 10
    fi
    echo; echo -e "--------------------------------------------------------------------------------------------------------------------\e[0m"
else
    echo -e "\e[48;5;94m\e[97m=== SKIPPING BUILD (make clean + make) BECAUSE -nobuild WAS SPECIFIED ===\e[0m"
fi


#  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ SPEEDTEST MODE

if [ $SPEEDTEST -eq 1 ]; then
    echo -e "\e[48;5;33m\e[97m=== SPEEDTEST MODE ENABLED ===\e[0m"
    TEST_BINARIES=(
        "/usr/bin/ls"
        "/usr/bin/putty"
        "/usr/bin/ls"
        "/usr/bin/lsusb"
        "/usr/bin/nano"
        "/usr/bin/strace"
        "/usr/bin/scalar"
        "/usr/bin/lsusb"
        "/usr/bin/nasm"
        "/usr/bin/rsync"
        "/usr/bin/dmesg"
        "/usr/bin/cat"
        "/usr/bin/fio"
        "/usr/bin/dmesg"
        "/usr/bin/scalar"
        "/usr/bin/htop"
        "/usr/bin/nano"
        "/usr/bin/w3m"
        "/usr/bin/fio"
        "/usr/bin/bash"
        "/usr/bin/rsync"
        "/usr/bin/grep"
        "/usr/bin/putty"
        "/usr/bin/grep"
        "/usr/bin/wget"
        "/usr/bin/cat"
        "/usr/bin/wget"
        "/usr/bin/scalar"
        "/usr/bin/lshw"
        "/usr/bin/strace"
        "/usr/bin/w3m"
        "/usr/bin/htop"
        "/usr/bin/lshw"
        "/usr/bin/nasm"
        "/usr/bin/bash"
    )
    comp_times=()
    decomp_times=()
    ratios=()
    comp_speeds=()
    decomp_speeds=()
    for bin in "${TEST_BINARIES[@]}"; do
        echo
        echo -e "\e[48;5;60m\e[97mTesting binary ($packing): $bin\e[0m"
        size_orig=$(stat -c%s "$bin")
        # -------------------------
        # Compression timing
        # -------------------------
        start=$(date +%s.%N)
        ./build/zelf $packing "$bin" --output"./packed_binary" > /dev/null
        end=$(date +%s.%N)
        comp_time=$(echo "$end - $start" | bc -l)
        size_pack=$(stat -c%s "./packed_binary")
        ratio=$(echo "scale=2; ($size_pack / $size_orig) * 100" | bc -l)
sleep 0.5
        # -------------------------
        # Decompression timing
        # -------------------------
        start=$(date +%s.%N)
        ./packed_binary --version > /dev/null 2>&1
        end=$(date +%s.%N)
        packed_time=$(echo "$end - $start" | bc -l)
sleep 0.5
        # -------------------------
        # Original execution timing
        # -------------------------
        start=$(date +%s.%N)
        "$bin" --version > /dev/null 2>&1 || true
        end=$(date +%s.%N)
        orig_time=$(echo "$end - $start" | bc -l)
        # Net decompression = packed - original
        decomp_time=$(echo "$packed_time - $orig_time" | bc -l)
        if (( $(echo "$decomp_time < 0" | bc -l) )); then decomp_time=0; fi
        # Speeds
        comp_speed=$(echo "scale=6; ($size_orig/1048576)/$comp_time" | bc -l)
        decomp_speed=$(echo "scale=6; ($size_orig/1048576)/$decomp_time" | bc -l)
        comp_times+=( "$comp_time" )
        decomp_times+=( "$decomp_time" )
        ratios+=( "$ratio" )
        comp_speeds+=( "$comp_speed" )
        decomp_speeds+=( "$decomp_speed" )
        echo -e "Original size:           $size_orig bytes"
        echo -e "Packed size:             $size_pack bytes"
        echo -e "Compression time:        $comp_time sec"
        echo -e "Decompression time(net): $decomp_time sec"
        echo -e "Compression ratio:       $ratio %"
        echo -e "Compression speed:       $comp_speed MiB/s"
        echo -e "Decompression speed:     $decomp_speed MiB/s"
        rm -f ./packed_binary
    done
    avg_comp=$(printf "%0.6f\n" $(echo "${comp_times[*]}" | awk '{sum=0; for(i=1;i<=NF;i++) sum+=$i; print sum/NF}'))
    avg_decomp=$(printf "%0.6f\n" $(echo "${decomp_times[*]}" | awk '{sum=0; for(i=1;i<=NF;i++) sum+=$i; print sum/NF}'))
    avg_ratio=$(printf "%0.2f\n" $(echo "${ratios[*]}" | awk '{sum=0; for(i=1;i<=NF;i++) sum+=$i; print sum/NF}'))
    avg_comp_speed=$(printf "%0.3f\n" $(echo "${comp_speeds[*]}" | awk '{sum=0; for(i=1;i<=NF;i++) sum+=$i; print sum/NF}'))
    avg_decomp_speed=$(printf "%0.3f\n" $(echo "${decomp_speeds[*]}" | awk '{sum=0; for(i=1;i<=NF;i++) sum+=$i; print sum/NF}'))
echo
echo -e "\e[48;5;28m\e[97m=== SPEEDTEST SUMMARY ===\e[0m"
echo -e "Compressor:                  \e[48;5;60m\e[97m  $packing  \e[0m"

if (( $(echo "$avg_comp < 1" | bc -l) )); then
    comp_ms=$(echo "$avg_comp * 1000" | bc -l)
    comp_speed_display=$(echo "$avg_comp_speed * 1024 / 1000" | bc -l)
    printf "Average compression time:  %0.3f ms (%.3f KB/ms)\n" "$comp_ms" "$comp_speed_display"
else
    comp_speed_display=$(echo "$avg_comp_speed" | bc -l)
    printf "Average compression time:  %0.3f sec (%.3f MiB/s)\n" "$avg_comp" "$comp_speed_display"
fi

if (( $(echo "$avg_decomp < 1" | bc -l) )); then
    decomp_ms=$(echo "$avg_decomp * 1000" | bc -l)
    decomp_speed_display=$(echo "$avg_decomp_speed * 1024 / 1000" | bc -l)
    printf "Average decompression time: %0.3f ms (%.3f KB/ms)\n" "$decomp_ms" "$decomp_speed_display"
else
    decomp_speed_display=$(echo "$avg_decomp_speed" | bc -l)
    printf "Average decompression time: %0.3f sec (%.3f MiB/s)\n" "$avg_decomp" "$decomp_speed_display"
fi

printf "Average \e[48;5;32m\e[97mcompression ratio:\e[0m   \e[48;5;32m\e[97m %0.2f %% \e[0m\n" "$avg_ratio"

if (( $(echo "$avg_comp_speed < 1" | bc -l) )); then
    comp_speed_kib=$(echo "$avg_comp_speed * 1024" | bc -l)
    printf "Average \e[48;5;28m\e[97mcompression speed:\e[0m   \e[48;5;28m\e[97m %0.3f KiB/s\e[0m\n" "$comp_speed_kib"
else
    printf "Average \e[48;5;28m\e[97mcompression speed:\e[0m   \e[48;5;28m\e[97m %0.3f MiB/s\e[0m\n" "$avg_comp_speed"
fi

if (( $(echo "$avg_decomp_speed < 1" | bc -l) )); then
    decomp_speed_kib=$(echo "$avg_decomp_speed * 1024" | bc -l)
    printf "Average \e[48;5;37m\e[97mdecompression speed:\e[0m \e[48;5;37m\e[97m %0.3f KiB/s \e[0m\n" "$decomp_speed_kib"
else
    printf "Average \e[48;5;37m\e[97mdecompression speed:\e[0m \e[48;5;37m\e[97m %0.3f MiB/s \e[0m\n" "$avg_decomp_speed"
fi

echo -e "\e[48;5;28m\e[97m=== DONE ===\e[0m"
exit 0
fi

############################################################

# TESTS
rm -f ./packed_binary_none
rm -f ./packed_binary_bcj
rm -f ./packed_binary_kexe
rm -f ./packed_static_binary_none
rm -f ./packed_static_binary_bck
rm -f ./packed_static_binary_kexe
rm -f ./packed_binary_kexe_unpacked
rm -f ./packed_binary_none_unpacked
rm -f ./packed_binary_bcj_unpacked
rm -f ./packed_static_binary_none_unpacked
rm -f ./packed_static_binary_bcj_unpacked
rm -f ./packed_static_binary_kexe_unpacked
rm -f ./packed_dynexec_binary_none
rm -f ./packed_dynexec_binary_bcj
rm -f ./packed_dynexec_binary_kexe
rm -f ./packed_dynexec_binary_none_unpacked
rm -f ./packed_dynexec_binary_bcj_unpacked
rm -f ./packed_dynexec_binary_kexe_unpacked
echo; echo -e "--------------------------------------------------------------------------------------------------------------------\e[0m"
echo -e "\e[48;5;235m\e[97m                         || test dynamic binary '$dyn_bin'  \e[48;5;236m\e[97m"
echo "--------------------------------------------------------------------------------------------------------------"
echo -e "######-----       packing dynamic binary...[filter:none] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose $packing --exe-filter=none "$dyn_bin"  --output"./packed_binary_none"
else
    echo "[...] last 7 lines:"
    ./build/zelf $packing "$dyn_bin"  --exe-filter=none --output"./packed_binary_none" | tail -n 7
fi
echo -e "\e[0m"; echo -e "\e[48;5;117m\e[30m######-----       testing dynamic execution$(big_tag) [filter: none]:\e[0m"
output=$(./packed_binary_none --version)
status=$?

if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ➤ DYNAMIC BINARY TEST$(big_tag) [NONE] OK \e[0m"
    dynamic_none_packing="ok"
else
    echo -e "\e[48;5;88m\e[97m ➤ DYNAMIC BINARY TEST$(big_tag) [NONE] FAILED       ✖✖✖✖✖    \e[0m"
    dynamic_none_packing="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_binary_none --version || true && tail -n 200 trace_dyn.txt
fi


echo -e "######-----       UNpacking dynamic binary...[filter:none] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose --unpack  ./packed_binary_none  --output"./packed_binary_none_unpacked"
else
    echo "[...] last 7 lines:"
    ./build/zelf --unpack ./packed_binary_none  --output"./packed_binary_none_unpacked" | tail -n 7
fi
output=$(./packed_binary_none_unpacked --version)
status=$?

if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ➤ UNPACKED DYNAMIC BINARY TEST$(big_tag) [NONE] OK \e[0m"
    dynamic_none_unpacking="ok"
else
    echo -e "\e[48;5;88m\e[97m ➤ UNPACKED DYNAMIC BINARY TEST$(big_tag) [NONE] FAILED       ✖✖✖✖✖    \e[0m"
    dynamic_none_unpacking="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_binary_none_unpacked --version || true && tail -n 200 trace_dyn.txt
fi





echo -e "######-----       packing dynamic binary...[filter:bcj] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose $packing --exe-filter=bcj "$dyn_bin"  --output"./packed_binary_bcj"
else
    echo "[...] last 7 lines:"
    ./build/zelf $packing "$dyn_bin"  --exe-filter=bcj --output"./packed_binary_bcj" | tail -n 7
fi
echo -e "\e[0m"; echo -e "\e[48;5;117m\e[30m######-----       testing dynamic execution$(big_tag) [filter: bcj]:\e[0m"
output=$(./packed_binary_bcj --version)
status=$?

if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ➤ DYNAMIC BINARY TEST$(big_tag) [BCJ] OK \e[0m"
    dynamic_bcj_packing="ok"
else
    echo -e "\e[48;5;88m\e[97m ➤ DYNAMIC BINARY TEST$(big_tag) [BCJ] FAILED       ✖✖✖✖✖    \e[0m"
    dynamic_bcj_packing="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_binary_bcj --version || true && tail -n 200 trace_dyn.txt
fi





echo -e "######-----       UNpacking dynamic binary...[filter:bcj] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose --unpack  ./packed_binary_bcj  --output"./packed_binary_bcj_unpacked"
else
    echo "[...] last 7 lines:"
    ./build/zelf --unpack ./packed_binary_bcj  --output"./packed_binary_bcj_unpacked" | tail -n 7
fi
output=$(./packed_binary_bcj_unpacked --version)
status=$?

if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ➤ UNPACKED DYNAMIC BINARY TEST$(big_tag) [BCJ] OK \e[0m"
    dynamic_bcj_unpacking="ok"
else
    echo -e "\e[48;5;88m\e[97m ➤ UNPACKED DYNAMIC BINARY TEST$(big_tag) [BCJ] FAILED       ✖✖✖✖✖    \e[0m"
    dynamic_bcj_unpacking="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_binary_bcj_unpacked --version || true && tail -n 200 trace_dyn.txt
fi




echo -e "######-----       packing dynamic binary...[filter:kexe] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose $packing --exe-filter=kanziexe "$dyn_bin"  --output"./packed_binary_kexe"
else
    echo "[...] last 7 lines:"
    ./build/zelf $packing "$dyn_bin"  --exe-filter=kanziexe --output"./packed_binary_kexe" | tail -n 7
fi
echo -e "\e[0m"; echo -e "\e[48;5;117m\e[30m######-----       testing dynamic execution$(big_tag) [filter: kexe]:\e[0m"
output=$(./packed_binary_kexe --version)
status=$?

if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ➤ DYNAMIC BINARY TEST$(big_tag) [KEXE] OK \e[0m"
    dynamic_kexe_packing="ok"
else
    echo -e "\e[48;5;88m\e[97m ➤ DYNAMIC BINARY TEST$(big_tag) [KEXE] FAILED       ✖✖✖✖✖    \e[0m"
    dynamic_kexe_packing="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_binary_kexe --version || true && tail -n 200 trace_dyn.txt
fi





echo -e "######-----       UNpacking dynamic binary...[filter:kexe] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose --unpack  ./packed_binary_kexe  --output"./packed_binary_kexe_unpacked"
else
    echo "[...] last 7 lines:"
    ./build/zelf --unpack ./packed_binary_kexe  --output"./packed_binary_kexe_unpacked" | tail -n 7
fi
output=$(./packed_binary_kexe_unpacked --version)
status=$?
if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi

if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ➤ UNPACKED DYNAMIC BINARY TEST$(big_tag) [KEXE] OK \e[0m"
    dynamic_kexe_unpacking="ok"
else
    echo -e "\e[48;5;88m\e[97m ➤ UNPACKED DYNAMIC BINARY TEST$(big_tag) [KEXE] FAILED       ✖✖✖✖✖    \e[0m"
    dynamic_kexe_unpacking="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_binary_kexe_unpacked --version || true && tail -n 200 trace_dyn.txt
fi

rm -f ./packed_static_binary_none
rm -f ./packed_static_binary_bcj
rm -f ./packed_static_binary_kexe
echo
echo "--------------------------------------------------------------------------------------------------------------"
echo -e "\e[48;5;235m\e[97m              || test static binary '$stat_bin'  \e[48;5;236m\e[97m"
echo "--------------------------------------------------------------------------------------------------------------"
echo -e "######-----       packing static binary [filter:none]... \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose $packing --exe-filter=none "$stat_bin"  --output"./packed_static_binary_none"
else
    echo "[...] last 7 lines:"
    ./build/zelf $packing --exe-filter=none "$stat_bin"  --output"./packed_static_binary_none" | tail -n 7
fi
echo -e "\e[0m"; echo -e "\e[48;5;117m\e[30m######-----       testing static execution$(big_tag) [filter: none]:\e[0m"
output=$(./packed_static_binary_none --version)
status=$?

if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 6 lines:"
    echo "$output" | tail -n 6
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ▪ STATIC BINARY TEST$(big_tag) [NONE] OK \e[0m"
    static_none_packing="ok"
else
    echo -e "\e[48;5;88m\e[97m ▪ STATIC BINARY TEST$(big_tag) [NONE] FAILED       ✖✖✖✖✖   \e[0m"
    static_none_packing="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_static.txt ./packed_static_binary_none || true && tail -n 200 trace_static.txt
fi





echo -e "######-----       UNpacking static binary...[filter:none] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose --unpack  ./packed_static_binary_none  --output"./packed_static_binary_none_unpacked"
else
    echo "[...] last 7 lines:"
    ./build/zelf --unpack ./packed_static_binary_none  --output"./packed_static_binary_none_unpacked" | tail -n 7
fi
output=$(./packed_static_binary_none_unpacked --version)
status=$?
if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi

if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ▪ UNPACKED STATIC BINARY TEST$(big_tag) [NONE] OK \e[0m"
    static_none_unpacking="ok"
else
    echo -e "\e[48;5;88m\e[97m ▪ UNPACKED STATIC BINARY TEST$(big_tag) [NONE] FAILED       ✖✖✖✖✖    \e[0m"
    static_none_unpacking="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_static_binary_none_unpacked --version || true && tail -n 200 trace_dyn.txt
fi




echo -e "######-----       packing static binary [filter:bcj]... \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose $packing --exe-filter=bcj "$stat_bin"  --output"./packed_static_binary_bcj"
else
    echo "[...] last 7 lines:"
    ./build/zelf $packing --exe-filter=bcj "$stat_bin"  --output"./packed_static_binary_bcj" | tail -n 7
fi
echo -e "\e[0m"; echo -e "\e[48;5;117m\e[30m######-----       testing static execution$(big_tag) [filter: bcj]:\e[0m"
output=$(./packed_static_binary_bcj --version)
status=$?

if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 6 lines:"
    echo "$output" | tail -n 6
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ▪ STATIC BINARY TEST$(big_tag) [BCJ] OK \e[0m"
    static_bcj_packing="ok"
else
    echo -e "\e[48;5;88m\e[97m ▪ STATIC BINARY TEST$(big_tag) [BCJ] FAILED       ✖✖✖✖✖   \e[0m"
    static_bcj_packing="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_static.txt ./packed_static_binary_bcj || true && tail -n 200 trace_static.txt
fi



echo -e "######-----       UNpacking static binary...[filter:bcj] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose --unpack  ./packed_static_binary_bcj  --output"./packed_static_binary_bcj_unpacked"
else
    echo "[...] last 7 lines:"
    ./build/zelf --unpack ./packed_static_binary_bcj  --output"./packed_static_binary_bcj_unpacked" | tail -n 7
fi
output=$(./packed_static_binary_bcj_unpacked --version)
status=$?
if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi

if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ▪ UNPACKED STATIC BINARY TEST$(big_tag) [BCJ] OK \e[0m"
    static_bcj_unpacking="ok"
else
    echo -e "\e[48;5;88m\e[97m ▪ UNPACKED STATIC BINARY TEST$(big_tag) [BCJ] FAILED       ✖✖✖✖✖    \e[0m"
    static_bcj_unpacking="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_static_binary_bcj_unpacked --version || true && tail -n 200 trace_dyn.txt
fi





echo -e "######-----       packing static binary [filter:kexe]... \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose $packing --exe-filter=kanziexe "$stat_bin"  --output"./packed_static_binary_kexe"
else
    echo "[...] last 7 lines:"
    ./build/zelf $packing --exe-filter=kanziexe "$stat_bin"  --output"./packed_static_binary_kexe" | tail -n 7
fi
echo -e "\e[0m"; echo -e "\e[48;5;117m\e[30m######-----       testing static execution$(big_tag) [filter: kexe]:\e[0m"
output=$(./packed_static_binary_kexe --version)
status=$?

if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 6 lines:"
    echo "$output" | tail -n 6
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ▪ STATIC BINARY TEST$(big_tag) [KEXE] OK \e[0m"
    static_kexe_packing="ok"
else
    echo -e "\e[48;5;88m\e[97m ▪ STATIC BINARY TEST$(big_tag) [KEXE] FAILED       ✖✖✖✖✖   \e[0m"
    static_kexe_packing="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_static.txt ./packed_static_binary_kexe || true && tail -n 200 trace_static.txt
fi






echo -e "######-----       UNpacking static binary...[filter:kexe] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose --unpack  ./packed_static_binary_kexe  --output"./packed_static_binary_kexe_unpacked"
else
    echo "[...] last 7 lines:"
    ./build/zelf --unpack ./packed_static_binary_kexe  --output"./packed_static_binary_kexe_unpacked" | tail -n 7
fi
output=$(./packed_static_binary_kexe_unpacked --version)
status=$?
if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi

if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ▪ UNPACKED STATIC BINARY TEST$(big_tag) [KEXE] OK \e[0m"
    static_kexe_unpacking="ok"
else
    echo -e "\e[48;5;88m\e[97m ▪ UNPACKED STATIC BINARY TEST$(big_tag) [KEXE] FAILED       ✖✖✖✖✖    \e[0m"
    static_kexe_unpacking="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_static_binary_kexe_unpacked --version || true && tail -n 200 trace_dyn.txt
fi



echo
echo "--------------------------------------------------------------------------------------------------------------"
echo -e "\e[48;5;235m\e[97m              || test dynexec binary '$dynexec_test_bin'  \e[48;5;236m\e[97m"
echo "--------------------------------------------------------------------------------------------------------------"
echo -e "######-----       packing dynexec binary...[filter:none] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose $packing --exe-filter=none "$dynexec_test_bin"  --output"./packed_dynexec_binary_none"
else
    echo "[...] last 7 lines:"
    ./build/zelf $packing "$dynexec_test_bin"  --exe-filter=none --output"./packed_dynexec_binary_none" | tail -n 7
fi
echo -e "\e[0m"; echo -e "\e[48;5;117m\e[30m######-----       testing dynexec execution [filter: none]:\e[0m"
output=$(./packed_dynexec_binary_none --selftest)
status=$?
if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ➢ DYNEXEC BINARY TEST [NONE] OK \e[0m"
    dynexec_none_packing="ok"
else
    echo -e "\e[48;5;88m\e[97m ➢ DYNEXEC BINARY TEST [NONE] FAILED       ✖✖✖✖✖    \e[0m"
    dynexec_none_packing="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_dynexec_binary_none --selftest || true && tail -n 200 trace_dyn.txt
fi

echo -e "######-----       UNpacking dynexec binary...[filter:none] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose --unpack  ./packed_dynexec_binary_none  --output"./packed_dynexec_binary_none_unpacked"
else
    echo "[...] last 7 lines:"
    ./build/zelf --unpack ./packed_dynexec_binary_none  --output"./packed_dynexec_binary_none_unpacked" | tail -n 7
fi
output=$(./packed_dynexec_binary_none_unpacked --selftest)
status=$?
if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ➢ UNPACKED DYNEXEC BINARY TEST [NONE] OK \e[0m"
    dynexec_none_unpacking="ok"
else
    echo -e "\e[48;5;88m\e[97m ➢ UNPACKED DYNEXEC BINARY TEST [NONE] FAILED       ✖✖✖✖✖    \e[0m"
    dynexec_none_unpacking="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_dynexec_binary_none_unpacked --selftest || true && tail -n 200 trace_dyn.txt
fi

echo -e "######-----       packing dynexec binary...[filter:bcj] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose $packing --exe-filter=bcj "$dynexec_test_bin"  --output"./packed_dynexec_binary_bcj"
else
    echo "[...] last 7 lines:"
    ./build/zelf $packing "$dynexec_test_bin"  --exe-filter=bcj --output"./packed_dynexec_binary_bcj" | tail -n 7
fi
echo -e "\e[0m"; echo -e "\e[48;5;117m\e[30m######-----       testing dynexec execution [filter: bcj]:\e[0m"
output=$(./packed_dynexec_binary_bcj --selftest)
status=$?
if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ➢ DYNEXEC BINARY TEST [BCJ] OK \e[0m"
    dynexec_bcj_packing="ok"
else
    echo -e "\e[48;5;88m\e[97m ➢ DYNEXEC BINARY TEST [BCJ] FAILED       ✖✖✖✖✖    \e[0m"
    dynexec_bcj_packing="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_dynexec_binary_bcj --selftest || true && tail -n 200 trace_dyn.txt
fi

echo -e "######-----       UNpacking dynexec binary...[filter:bcj] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose --unpack  ./packed_dynexec_binary_bcj  --output"./packed_dynexec_binary_bcj_unpacked"
else
    echo "[...] last 7 lines:"
    ./build/zelf --unpack ./packed_dynexec_binary_bcj  --output"./packed_dynexec_binary_bcj_unpacked" | tail -n 7
fi
output=$(./packed_dynexec_binary_bcj_unpacked --selftest)
status=$?
if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ➢ UNPACKED DYNEXEC BINARY TEST [BCJ] OK \e[0m"
    dynexec_bcj_unpacking="ok"
else
    echo -e "\e[48;5;88m\e[97m ➢ UNPACKED DYNEXEC BINARY TEST [BCJ] FAILED       ✖✖✖✖✖    \e[0m"
    dynexec_bcj_unpacking="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_dynexec_binary_bcj_unpacked --selftest || true && tail -n 200 trace_dyn.txt
fi

echo -e "######-----       packing dynexec binary...[filter:kexe] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose $packing --exe-filter=kanziexe "$dynexec_test_bin"  --output"./packed_dynexec_binary_kexe"
else
    echo "[...] last 7 lines:"
    ./build/zelf $packing "$dynexec_test_bin"  --exe-filter=kanziexe --output"./packed_dynexec_binary_kexe" | tail -n 7
fi
echo -e "\e[0m"; echo -e "\e[48;5;117m\e[30m######-----       testing dynexec execution [filter: kexe]:\e[0m"
output=$(./packed_dynexec_binary_kexe --selftest)
status=$?
if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ➢ DYNEXEC BINARY TEST [KEXE] OK \e[0m"
    dynexec_kexe_packing="ok"
else
    echo -e "\e[48;5;88m\e[97m ➢ DYNEXEC BINARY TEST [KEXE] FAILED       ✖✖✖✖✖    \e[0m"
    dynexec_kexe_packing="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_dynexec_binary_kexe --selftest || true && tail -n 200 trace_dyn.txt
fi

echo -e "######-----       UNpacking dynexec binary...[filter:kexe] \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose --unpack  ./packed_dynexec_binary_kexe  --output"./packed_dynexec_binary_kexe_unpacked"
else
    echo "[...] last 7 lines:"
    ./build/zelf --unpack ./packed_dynexec_binary_kexe  --output"./packed_dynexec_binary_kexe_unpacked" | tail -n 7
fi
output=$(./packed_dynexec_binary_kexe_unpacked --selftest)
status=$?
if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi
if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m ➢ UNPACKED DYNEXEC BINARY TEST [KEXE] OK \e[0m"
    dynexec_kexe_unpacking="ok"
else
    echo -e "\e[48;5;88m\e[97m ➢ UNPACKED DYNEXEC BINARY TEST [KEXE] FAILED       ✖✖✖✖✖    \e[0m"
    dynexec_kexe_unpacking="failed"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_dynexec_binary_kexe_unpacked --selftest || true && tail -n 200 trace_dyn.txt
fi


echo
echo "--------------------------------------------------------------------------------------------------------------"
echo -e "\e[48;5;235m\e[97m              || test archive mode on file '$stat_bin'  \e[48;5;236m\e[97m"
echo "--------------------------------------------------------------------------------------------------------------"
echo -e "######-----       Testing archive mode (packing)... \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    if ./build/zelf --archive --verbose $packing "$stat_bin"  --output"./simple_static_${packing}.zlf"; then
        archive_packing="ok"
    else
        archive_packing="failed"
    fi
else
    echo "[...] last 7 lines:"
    if ./build/zelf $packing --archive "$stat_bin"  --output"./simple_static_${packing}.zlf" | tail -n 7; then
        archive_packing="ok"
    else
        archive_packing="failed"
    fi
fi
echo -e "######-----       Testing archive mode (unpacking)... \e[48;5;229m\e[30m($packing)\e[0m"
 ./build/zelf --unpack --verbose  ./simple_static_${packing}.zlf  --output"./simple_static_${packing}_unpacked"
if cmp -s "$stat_bin" "./simple_static_${packing}_unpacked"; then
    echo -e "\e[48;5;28m\e[97m • ARCHIVE MODE OK\e[0m"
    archive_unpacking="ok"
else
    echo -e "\e[48;5;88m\e[97m • ARCHIVE MODE FAILED                     ✖✖✖✖✖   \e[0m"
    archive_unpacking="failed"
fi
rm -f "./simple_static_${packing}.zlf"
rm -f "./simple_static_${packing}_unpacked"


# Summary
if [ $SUMMARY -eq 1 ]; then
    echo
       echo "┌───────────────────────────────────────────────────────────────────────────────────────────────┐"
    if [ $BIG -eq 1 ]; then
        summary_text="SUMMARY: ${packing#-} {BIG}"
    else
        summary_text="SUMMARY: ${packing#-}"
    fi
    total_content_width=107
    text_length=${#summary_text}
    total_padding=$((total_content_width - text_length))
    left_padding=$((total_padding / 2))
    right_padding=$((total_padding - left_padding))
    printf -v padded_line "│\e[48;5;235m\e[97m%*s%s%*s\e[0m\e[48;5;236m\e[97m\e[0m│" $left_padding "" "$summary_text" $right_padding ""
    echo "$padded_line"
       echo "├───────────────┬──────────────────────┬──────────────────────┬──────────────────────┬──────────────────────┤"
     printf "│ %-13s │ %-20s │ %-20s │ %-20s │ %-20s │\n" "Filter" "Dynamic" "Static" "Dynexec" "Archive"
     printf "├───────────────┼──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤\n"
    dynamic_none_packing_line="packing: ${dynamic_none_packing:-unknown}"
    dynamic_none_unpacking_line="unpacking: ${dynamic_none_unpacking:-unknown}"
    static_none_packing_line="packing: ${static_none_packing:-unknown}"
    static_none_unpacking_line="unpacking: ${static_none_unpacking:-unknown}"
    dynexec_none_packing_line="packing: ${dynexec_none_packing:-unknown}"
    dynexec_none_unpacking_line="unpacking: ${dynexec_none_unpacking:-unknown}"
    archive_packing_line="packing: ${archive_packing:-unknown}"
    archive_unpacking_line="unpacking: ${archive_unpacking:-unknown}"
    printf "│ %-13s │ %-20s │ %-20s │ %-20s │ %-20s │\n" "none" "$dynamic_none_packing_line" "$static_none_packing_line" "$dynexec_none_packing_line" "$archive_packing_line"
    printf "│ %-13s │ %-20s │ %-20s │ %-20s │ %-20s │\n" "" "$dynamic_none_unpacking_line" "$static_none_unpacking_line" "$dynexec_none_unpacking_line" "$archive_unpacking_line"
    printf "├───────────────┼──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤\n"
    dynamic_bcj_packing_line="packing: ${dynamic_bcj_packing:-unknown}"
    dynamic_bcj_unpacking_line="unpacking: ${dynamic_bcj_unpacking:-unknown}"
    static_bcj_packing_line="packing: ${static_bcj_packing:-unknown}"
    static_bcj_unpacking_line="unpacking: ${static_bcj_unpacking:-unknown}"
    dynexec_bcj_packing_line="packing: ${dynexec_bcj_packing:-unknown}"
    dynexec_bcj_unpacking_line="unpacking: ${dynexec_bcj_unpacking:-unknown}"
    printf "│ %-13s │ %-20s │ %-20s │ %-20s │ %-20s │\n" "bcj" "$dynamic_bcj_packing_line" "$static_bcj_packing_line" "$dynexec_bcj_packing_line" ""
    printf "│ %-13s │ %-20s │ %-20s │ %-20s │ %-20s │\n" "" "$dynamic_bcj_unpacking_line" "$static_bcj_unpacking_line" "$dynexec_bcj_unpacking_line" ""
    printf "├───────────────┼──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤\n"
    dynamic_kexe_packing_line="packing: ${dynamic_kexe_packing:-unknown}"
    dynamic_kexe_unpacking_line="unpacking: ${dynamic_kexe_unpacking:-unknown}"
    static_kexe_packing_line="packing: ${static_kexe_packing:-unknown}"
    static_kexe_unpacking_line="unpacking: ${static_kexe_unpacking:-unknown}"
    dynexec_kexe_packing_line="packing: ${dynexec_kexe_packing:-unknown}"
    dynexec_kexe_unpacking_line="unpacking: ${dynexec_kexe_unpacking:-unknown}"
    printf "│ %-13s │ %-20s │ %-20s │ %-20s │ %-20s │\n" "kanziexe" "$dynamic_kexe_packing_line" "$static_kexe_packing_line" "$dynexec_kexe_packing_line" ""
    printf "│ %-13s │ %-20s │ %-20s │ %-20s │ %-20s │\n" "" "$dynamic_kexe_unpacking_line" "$static_kexe_unpacking_line" "$dynexec_kexe_unpacking_line" ""
    echo "└───────────────┴──────────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘"
fi
echo -e "\n\e[48;5;94m\e[97m ----------------------------------------------------------------------------------------------------- \e[0m\n│"
echo
