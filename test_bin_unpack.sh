#!/usr/bin/env bash

# --- Couleurs ---
OK_BG="\e[42m\e[97m"
FAIL_BG="\e[41m\e[97m"
BLUE_BG="\e[48;5;19m\e[97m"
RESET="\e[0m"

# --- Fonction d'affichage ---
check_status() {
    local status=$1
    if [ "$status" -eq 0 ]; then
        echo -e "${OK_BG} OK ${RESET}"
    else
        echo -e "${FAIL_BG} FAILED ${RESET}"
    fi
}

# --- Fonction qui retourne "OK" ou "FAILED" sans couleur (pour résumé global) ---
status_word() {
    local status=$1
    if [ "$status" -eq 0 ]; then
        echo "OK"
    else
        echo "FAILED"
    fi
}

# --- Liste des codecs ---
codecs=(
    -lz4 -pp -snappy -doboz -qlz -lzav -lzma -zstd  -stcr -lzsa2 -zx7b -blz -apultra -exo  -zx0 -shrinkler
)

# --- Binaires à tester ---
bins=("/usr/bin/ls" "/usr/bin/rclone")

# --- Résultats globaux ---
declare -A GLOBAL

for codec in "${codecs[@]}"; do
    echo
    echo "==============================="
    echo "Testing codec: $codec"
    echo "==============================="

    # --- Résultats init ---
    pack_ls=1; pack_rclone=1
    exec_ls_packed=1; exec_rclone_packed=1
    unpack_ls=1; unpack_rclone=1
    exec_ls_unpacked=1; exec_rclone_unpacked=1

    # --- PACKING ---
    for bin in "${bins[@]}"; do
        base=$(basename "$bin")
        packed="${base}_packed"

        echo "Packing $base ..."
        ./build/zelf "$codec" "$bin" --output "./$packed" >/dev/null 2>&1
        status=$?
        check_status $status

        if [ "$base" = "ls" ]; then pack_ls=$status; else pack_rclone=$status; fi

        echo -n "Packed $base execution test: "
        if [ "$base" = "ls" ]; then
            ./"$packed" --version >/dev/null 2>&1
            exec_ls_packed=$?
        else
            ./"$packed" version >/dev/null 2>&1
            exec_rclone_packed=$?
        fi
        check_status $?
    done

    # --- UNPACKING ---
    echo
    echo "Unpacking test"

    for bin in "${bins[@]}"; do
        base=$(basename "$bin")
        packed="${base}_packed"

        echo "--------------------------------"
        echo "Unpack $packed:"
        echo "--------------------------------"

        ./build/zelf --unpack "./$packed"
        status=$?
        echo -n "Unpack status: "
        check_status $status

        if [ "$base" = "ls" ]; then unpack_ls=$status; else unpack_rclone=$status; fi

        # Skip execution if unpack failed
        if [ "$status" -ne 0 ]; then
            echo "Skipping execution test for $packed (unpack failed)"
            continue
        fi

        chmod +x "./$packed"

        echo -n "Execution after unpack ($base): "
        if [ "$base" = "ls" ]; then
            ./"$packed" --version >/dev/null 2>&1
            exec_ls_unpacked=$?
        else
            ./"$packed" version >/dev/null 2>&1
            exec_rclone_unpacked=$?
        fi
        check_status $?
    done

    # --- Résumé final par codec ---
    echo
echo -e "${BLUE_BG}========== Résumé codec $codec ==========${RESET}"
echo -e "${BLUE_BG}$(printf "%-25s %s" "Pack LS:" "$(check_status $pack_ls)")${RESET}"
echo -e "${BLUE_BG}$(printf "%-25s %s" "Pack Rclone:" "$(check_status $pack_rclone)")${RESET}"
echo -e "${BLUE_BG}$(printf "%-25s %s" "Exec LS (packed):" "$(check_status $exec_ls_packed)")${RESET}"
echo -e "${BLUE_BG}$(printf "%-25s %s" "Exec Rclone (packed):" "$(check_status $exec_rclone_packed)")${RESET}"
echo -e "${BLUE_BG}$(printf "%-25s %s" "Unpack LS:" "$(check_status $unpack_ls)")${RESET}"
echo -e "${BLUE_BG}$(printf "%-25s %s" "Unpack Rclone:" "$(check_status $unpack_rclone)")${RESET}"
echo -e "${BLUE_BG}$(printf "%-25s %s" "Exec LS (unpacked):" "$(check_status $exec_ls_unpacked)")${RESET}"
echo -e "${BLUE_BG}$(printf "%-25s %s" "Exec Rclone (unpacked):" "$(check_status $exec_rclone_unpacked)")${RESET}"
echo -e "${BLUE_BG}==========================================${RESET}"


    # --- Stockage global ---
    GLOBAL["$codec,pack_ls"]=$(status_word $pack_ls)
    GLOBAL["$codec,pack_rclone"]=$(status_word $pack_rclone)
    GLOBAL["$codec,exec_ls_packed"]=$(status_word $exec_ls_packed)
    GLOBAL["$codec,exec_rclone_packed"]=$(status_word $exec_rclone_packed)
    GLOBAL["$codec,unpack_ls"]=$(status_word $unpack_ls)
    GLOBAL["$codec,unpack_rclone"]=$(status_word $unpack_rclone)
    GLOBAL["$codec,exec_ls_unpacked"]=$(status_word $exec_ls_unpacked)
    GLOBAL["$codec,exec_rclone_unpacked"]=$(status_word $exec_rclone_unpacked)

    # --- Cleanup ---
    rm -f ./ls_packed ./rclone_packed
done

# --- Résumé global final ---
echo
echo "==============================================="
echo "============== RÉSUMÉ GLOBAL =================="
echo "==============================================="

printf "%-12s %-10s %-12s %-15s %-15s %-12s %-15s %-18s %-20s\n" \
    "Codec" "PackLS" "PackRclone" "ExecLS(packed)" "ExecRclone(packed)" \
    "UnpackLS" "UnpackRclone" "ExecLS(unpacked)" "ExecRclone(unpacked)"

for codec in "${codecs[@]}"; do
    printf "%-12s %-10s %-12s %-15s %-15s %-12s %-15s %-18s %-20s\n" \
        "$codec" \
        "${GLOBAL["$codec,pack_ls"]}" \
        "${GLOBAL["$codec,pack_rclone"]}" \
        "${GLOBAL["$codec,exec_ls_packed"]}" \
        "${GLOBAL["$codec,exec_rclone_packed"]}" \
        "${GLOBAL["$codec,unpack_ls"]}" \
        "${GLOBAL["$codec,unpack_rclone"]}" \
        "${GLOBAL["$codec,exec_ls_unpacked"]}" \
        "${GLOBAL["$codec,exec_rclone_unpacked"]}"
done

echo "==============================================="
