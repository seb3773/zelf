#!/bin/bash
export LC_ALL=C
export LANG=C
packing="-lz4"
FULL=0
NOBUILD=0
SPEEDTEST=0
show_help() {
    echo "Usage: $0 <Packer> [options]

Packer (mandatory):
  -pp           Use PowerPacker2.0 Packing
  -snappy       Use Snappy Packing
  -lz4          Use LZ4 Packing
  -apultra      Use Apultra Packing
  -exo          Use Exomizer Packing
  -zx0          Use zx7b Packing
  -doboz        Use DoBoz Packing
  -qlz          Use QuickLZ Packing
  -lzav         Use LZAV Packing
  -lzma         Use LZMA Packing
  -zstd         Use Zstandard Packing
  -shr    Use Shrinkler Packing
  -zx0          Use ZX0 Packing
  -blz          Use BLZ Packing
  -stcr         Use StoneCracker Packing
  -lzsa         Use LZSA2 Packing

Options (optionnal):
  -full         Build fully (no tail)
  -nobuild      Skip 'make clean' and 'make'
  -speedtest    Enable speedtest mode


  -help, --help Display this help message

Examples:
  $0  -lz4 -full
  $0 -zstd -nobuild -speedtest
"
}

echo "
  -= Zelf packer shell script tester =-
  -------------------------------------
"

# ----------------------------
#  ARGUMENT PARSING
# ----------------------------

# No arguments → show help
if [ $# -eq 0 ]; then
    show_help
    exit 0
fi

# Explicit help request
for a in "$@"; do
    case "$a" in
        -help|--help)
            show_help
            exit 0
            ;;
    esac
done

# Parse arguments
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
        -pp|-snappy|-lz4|-apultra|-exo|-doboz|-qlz|-lzav|-lzma|-zstd|-shr|-zx0|-blz|-stcr|-lzsa|-zx7b)
            packing="$arg"
            ;;
        *)
            echo -e "\e[48;5;88m\e[97m⚠ Unknown argument: '$arg' \e[0m"
            show_help
            exit 1
            ;;
    esac
done



# ----------------------------
#  BUILD PHASE (optional)
# ----------------------------
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


#############################################
#             SPEEDTEST MODE
#############################################

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

        # Speeds pour ce binaire
        comp_speed=$(echo "scale=6; ($size_orig/1048576)/$comp_time" | bc -l)
        decomp_speed=$(echo "scale=6; ($size_orig/1048576)/$decomp_time" | bc -l)

        # Stockage
        comp_times+=( "$comp_time" )
        decomp_times+=( "$decomp_time" )
        ratios+=( "$ratio" )
        comp_speeds+=( "$comp_speed" )
        decomp_speeds+=( "$decomp_speed" )

        # Affichage par binaire
        echo -e "Original size:           $size_orig bytes"
        echo -e "Packed size:             $size_pack bytes"
        echo -e "Compression time:        $comp_time sec"
        echo -e "Decompression time(net): $decomp_time sec"
        echo -e "Compression ratio:       $ratio %"
        echo -e "Compression speed:       $comp_speed MiB/s"
        echo -e "Decompression speed:     $decomp_speed MiB/s"

        rm -f ./packed_binary
    done

    #############################################
    # Moyennes
    #############################################

    avg_comp=$(printf "%0.6f\n" $(echo "${comp_times[*]}" | awk '{sum=0; for(i=1;i<=NF;i++) sum+=$i; print sum/NF}'))
    avg_decomp=$(printf "%0.6f\n" $(echo "${decomp_times[*]}" | awk '{sum=0; for(i=1;i<=NF;i++) sum+=$i; print sum/NF}'))
    avg_ratio=$(printf "%0.2f\n" $(echo "${ratios[*]}" | awk '{sum=0; for(i=1;i<=NF;i++) sum+=$i; print sum/NF}'))
    avg_comp_speed=$(printf "%0.3f\n" $(echo "${comp_speeds[*]}" | awk '{sum=0; for(i=1;i<=NF;i++) sum+=$i; print sum/NF}'))
    avg_decomp_speed=$(printf "%0.3f\n" $(echo "${decomp_speeds[*]}" | awk '{sum=0; for(i=1;i<=NF;i++) sum+=$i; print sum/NF}'))

#############################################
# FINAL SUMMARY
#############################################

echo
echo -e "\e[48;5;28m\e[97m=== SPEEDTEST SUMMARY ===\e[0m"
echo -e "Compressor:                  \e[48;5;60m\e[97m  $packing  \e[0m"

# -------------------
# Average compression time
# -------------------
if (( $(echo "$avg_comp < 1" | bc -l) )); then
    comp_ms=$(echo "$avg_comp * 1000" | bc -l)
    # Conversion correcte MiB/s → KB/ms
    comp_speed_display=$(echo "$avg_comp_speed * 1024 / 1000" | bc -l)
    printf "Average compression time:  %0.3f ms (%.3f KB/ms)\n" "$comp_ms" "$comp_speed_display"
else
    comp_speed_display=$(echo "$avg_comp_speed" | bc -l)
    printf "Average compression time:  %0.3f sec (%.3f MiB/s)\n" "$avg_comp" "$comp_speed_display"
fi

# -------------------
# Average decompression time
# -------------------
if (( $(echo "$avg_decomp < 1" | bc -l) )); then
    decomp_ms=$(echo "$avg_decomp * 1000" | bc -l)
    decomp_speed_display=$(echo "$avg_decomp_speed * 1024 / 1000" | bc -l)
    printf "Average decompression time: %0.3f ms (%.3f KB/ms)\n" "$decomp_ms" "$decomp_speed_display"
else
    decomp_speed_display=$(echo "$avg_decomp_speed" | bc -l)
    printf "Average decompression time: %0.3f sec (%.3f MiB/s)\n" "$avg_decomp" "$decomp_speed_display"
fi

# -------------------
# Average compression ratio
# -------------------
printf "Average \e[48;5;32m\e[97mcompression ratio:\e[0m   \e[48;5;32m\e[97m %0.2f %% \e[0m\n" "$avg_ratio"

# -------------------
# Average compression speed (unchanged)
# -------------------
if (( $(echo "$avg_comp_speed < 1" | bc -l) )); then
    comp_speed_kib=$(echo "$avg_comp_speed * 1024" | bc -l)
    printf "Average \e[48;5;28m\e[97mcompression speed:\e[0m   \e[48;5;28m\e[97m %0.3f KiB/s\e[0m\n" "$comp_speed_kib"
else
    printf "Average \e[48;5;28m\e[97mcompression speed:\e[0m   \e[48;5;28m\e[97m %0.3f MiB/s\e[0m\n" "$avg_comp_speed"
fi

# -------------------
# Average decompression speed (unchanged)
# -------------------
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
#                 NORMAL TESTS (unchanged)
############################################################

echo; echo -e "--------------------------------------------------------------------------------------------------------------------\e[0m"
echo -e "\e[48;5;235m\e[97m                         || test dynamic binary '/usr/bin/ls'  \e[48;5;236m\e[97m"
rm -f ./packed_binary
echo -e "## packing dynamic binary... \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose $packing /usr/bin/ls  --output"./packed_binary"
else
    echo "[...] last 7 lines:"
    ./build/zelf $packing /usr/bin/ls  --output"./packed_binary" | tail -n 7
fi

echo -e "\e[0m"; echo -e "\e[48;5;117m\e[30m## testing dynamic execution:\e[0m"
output=$(./packed_binary --version)
status=$?

if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 8 lines:"
    echo "$output" | tail -n 8
fi

if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m DYNAMIC BINARY TEST OK \e[0m"
else
    echo -e "\e[48;5;88m\e[97m DYNAMIC BINARY TEST FAILED       ✖✖✖✖✖    \e[0m"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_dyn.txt ./packed_binary --version || true && tail -n 200 trace_dyn.txt
fi

rm -f ./packed_binary
echo "--------------------------------------------------------------------------------------"
echo -e "\e[48;5;235m\e[97m              || test static binary '../tools/simple_static'  \e[48;5;236m\e[97m"
echo -e "## packing static binary... \e[48;5;229m\e[30m($packing)\e[0m"
if [ $FULL -eq 1 ]; then
    ./build/zelf -verbose $packing ./tools/simple_static  --output"./packed_binary"
else
    echo "[...] last 7 lines:"
    ./build/zelf $packing ./tools/simple_static  --output"./packed_binary" | tail -n 7
fi

echo -e "\e[0m"; echo -e "\e[48;5;117m\e[30m## testing static execution:\e[0m"
output=$(./packed_binary)
status=$?

if [ $FULL -eq 1 ]; then
    echo "$output"
else
    echo "[...] last 6 lines:"
    echo "$output" | tail -n 6
fi

if [ $status -eq 0 ]; then
    echo -e "\e[48;5;28m\e[97m STATIC BINARY TEST OK \e[0m"
else
    echo -e "\e[48;5;88m\e[97m STATIC BINARY TEST FAILED       ✖✖✖✖✖   \e[0m"
    if [ $FULL -eq 0 ]; then
        echo -e "Full output:\n"
        echo "$output"
    fi
   echo -e "\e[48;5;88m\e[97mlast 200 lines strace:\e[0m\n"
   strace -f -o trace_static.txt ./packed_binary || true && tail -n 200 trace_static.txt
fi

echo "--------------------------------------------------------------------------------------"
rm -f ./packed_binary
echo
