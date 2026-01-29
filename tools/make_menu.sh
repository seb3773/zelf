#!/usr/bin/env bash
set -euo pipefail

# Simple interactive make menu for the project.
# Uses whiptail/dialog if available, otherwise falls back to a text menu.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAKE_CMD=${MAKE:-make}
STATIC_MODE=0
LINKER_MODE=auto
C_RESET=$(tput sgr0)
C_TITLE=$(tput bold; tput setaf 2)     # green
C_TEXT=$(tput setaf 7)                  # light gray
C_DIM=$(tput setaf 8)                   # dark gray
C_HIGHLIGHT=$(tput setaf 2)             # green
C_ERROR=$(tput setaf 1)                 # red
DIALOG_OPTS=""
C_TITLE=""
C_ITEM=""
C_STATE=""
C_RESET=""
run_make() {
  target="$1"
  make_args=()
  if [ "$STATIC_MODE" -eq 1 ]; then
    make_args+=(STATIC=1)
  fi
  if [ "$LINKER_MODE" != "auto" ]; then
    make_args+=(FUSE_LD="$LINKER_MODE")
  fi

  if [ ${#make_args[@]} -ne 0 ]; then
    echo "==> Running: $MAKE_CMD ${make_args[*]} $target"
  else
    echo "==> Running: $MAKE_CMD $target"
  fi

  if [ "$STATIC_MODE" -eq 1 ]; then
    if $MAKE_CMD "${make_args[@]}" $target; then
      echo "==> Done: STATIC $target"
    else
      echo "==> Failed: STATIC $target" >&2
      read -p "Press Enter to continue..." _ || true
    fi
  else
    if $MAKE_CMD "${make_args[@]}" $target; then
      echo "==> Done: $target"
    else
      echo "==> Failed: $target" >&2
      read -p "Press Enter to continue..." _ || true
    fi
  fi
}

select_linker() {
  if [ -n "${DIALOG:-}" ]; then
    choice=$($DIALOG --clear --title "Linker selection" \
      --menu "Select linker (FUSE_LD)" 18 80 10 \
      1 "auto (lld -> bfd -> default)" \
      2 "lld" \
      3 "bfd" 3>&1 1>&2 2>&3) || return 0
    case "$choice" in
      1) LINKER_MODE=auto ;;
      2) LINKER_MODE=lld ;;
      3) LINKER_MODE=bfd ;;
    esac
  else
    case "$LINKER_MODE" in
      auto) LINKER_MODE=lld ;;
      lld) LINKER_MODE=bfd ;;
      bfd) LINKER_MODE=auto ;;
      *) LINKER_MODE=auto ;;
    esac
    echo "Linker now: $LINKER_MODE"
  fi
}

run_quick_tests() {
  echo "==> Running quick tests: $ROOT_DIR/test_all.sh"
  # Ensure nested script is executable when invoked by test_all.sh
  chmod +x "$ROOT_DIR/build_and_test.sh" 2>/dev/null || true
  if (cd "$ROOT_DIR" && bash test_all.sh); then
    echo "==> Quick tests completed successfully"
  else
    echo "==> Quick tests failed" >&2
    read -p "Press Enter to continue..." _ || true
  fi
}

if command -v whiptail >/dev/null 2>&1; then
  DIALOG=whiptail
export NEWT_COLORS='
    root=black,black
    border=white,black
    window=white,black
    shadow=black,black
    title=green,black
    button=black,green
    actbutton=black,lightgreen
    compactbutton=black,green
    checkbox=white,black
    actcheckbox=black,green
    entry=white,black
    label=white,black
    listbox=white,black
    actlistbox=black,green
    textbox=white,black
    helpline=white,black
    roottext=lightgray,black
  '
elif command -v dialog >/dev/null 2>&1; then
  DIALOG=dialog
DIALOG_OPTS="--colors"
  C_TITLE='\Zb\Z7'   # bold white
  C_ITEM='\Zn\Z7'    # white
  C_STATE='\Zb\Z2'   # bold green
  C_RESET='\Zn'
else
  DIALOG=""
fi

menu_actions=( \
  "all:Build everything (stubs/tools/packer)" \
  "packer:Build packer only" \
  "stubs:Build stubs" \
  "tools:Build tools" \
  "clean:Clean build artifacts" \
  "help:Show Makefile help" \
  "exit:Exit menu" \
)

if [ -n "$DIALOG" ]; then
  while true; do
    # Display current static/dynamic mode in the title
    MODE_STR="dynamic"
    [ "$STATIC_MODE" -eq 1 ] && MODE_STR="static"
    CHOICES=$($DIALOG --clear $DIALOG_OPTS \
  --title "${C_TITLE}zELF Build Menu (mode: $MODE_STR, linker: $LINKER_MODE)${C_RESET}" \
  --menu "${C_ITEM}Select an action${C_RESET}" 24 100 16 \
  1 "${C_ITEM}Build everything (make all)${C_RESET}" \
  2 "${C_ITEM}Build packer (make packer)${C_RESET}" \
  3 "${C_ITEM}Build stubs (make stubs)${C_RESET}" \
  4 "${C_ITEM}Build tools (make tools)${C_RESET}" \
  5 "${C_ITEM}Clean (make clean)${C_RESET}" \
  6 "${C_STATE}Build type: $MODE_STR${C_RESET}" \
  7 "${C_STATE}Linker: $LINKER_MODE${C_RESET}" \
  8 "${C_ITEM}Create .deb package (make deb)${C_RESET}" \
  9 "${C_ITEM}Create tar.gz package (make tar)${C_RESET}" \
  10 "${C_ITEM}Install build deps (make install_dependencies)${C_RESET}" \
  11 "${C_ITEM}Quick tests (run test_all.sh)${C_RESET}" \
  12 "${C_ITEM}Show Makefile (open in pager)${C_RESET}" \
  13 "${C_ITEM}Exit${C_RESET}" \
  3>&1 1>&2 2>&3)
    rc=$?
    [ $rc -ne 0 ] && break
    case "$CHOICES" in
      1) run_make all ;;
      2) run_make packer ;;
      3) run_make stubs ;;
      4) run_make tools ;;
      5) run_make clean ;;
      6) # toggle
         STATIC_MODE=$((1 - STATIC_MODE)); SKIP_PAUSE=1 ;;
      7) select_linker; SKIP_PAUSE=1 ;;
      8) run_make deb ;;
      9) run_make tar ;;
      10) run_make install_dependencies ;;
      11) run_quick_tests ;;
      12) less "$ROOT_DIR/Makefile" ;;
      13) break ;;
    esac
    if [ "${SKIP_PAUSE:-0}" -ne 1 ]; then
      echo
      echo "Press Enter to continue..."
      read -r _ || true
    fi
    SKIP_PAUSE=0
  done
else
  PS3="${C_HIGHLIGHT}Choose an action (type number): ${C_RESET}"
  options=("make all" "make packer" "make stubs" "make tools" "make clean" "build type" "linker" "make deb (package)" "make tar (package tar.gz)" "install deps" "quick tests" "show Makefile" "exit")

  while true; do
    echo
    echo "${C_TITLE}zELF Build Menu${C_RESET}"
    echo "${C_DIM}(Build type affects subsequent make invocations)${C_RESET}"

    MODE_STR_TEXT=$( [ "$STATIC_MODE" -eq 1 ] && echo static || echo dynamic )
    options[5]="Build type: ${MODE_STR_TEXT}"
    options[6]="Linker: ${LINKER_MODE}"

    select opt in "${options[@]}"; do
      case "$REPLY" in
        1) run_make all; break;;
        2) run_make packer; break;;
        3) run_make stubs; break;;
        4) run_make tools; break;;
        5) run_make clean; break;;
        6)
          STATIC_MODE=$((1 - STATIC_MODE))
          echo "${C_HIGHLIGHT}Build type now:${C_RESET} $( [ "$STATIC_MODE" -eq 1 ] && echo static || echo dynamic )"
          break
          ;;
        7) select_linker; break;;
        8) run_make deb; break;;
        9) run_make tar; break;;
        10) run_make install_dependencies; break;;
        11) run_quick_tests; break;;
        12) less "$ROOT_DIR/Makefile"; break;;
        13)
          echo "${C_DIM}Bye${C_RESET}"
          exit 0
          ;;
        *)
          echo "${C_ERROR}Invalid selection${C_RESET}"
          break
          ;;
      esac
    done
  done
fi


# End
