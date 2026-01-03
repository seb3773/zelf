#!/usr/bin/env bash
set -euo pipefail

# Simple interactive make menu for the project.
# Uses whiptail/dialog if available, otherwise falls back to a text menu.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAKE_CMD=${MAKE:-make}
STATIC_MODE=0

run_make() {
  target="$1"
  if [ "$STATIC_MODE" -eq 1 ]; then
    echo "==> Running: $MAKE_CMD STATIC=1 $target"
    if $MAKE_CMD STATIC=1 $target; then
      echo "==> Done: STATIC $target"
    else
      echo "==> Failed: STATIC $target" >&2
      read -p "Press Enter to continue..." _ || true
    fi
  else
    echo "==> Running: $MAKE_CMD $target"
    if $MAKE_CMD $target; then
      echo "==> Done: $target"
    else
      echo "==> Failed: $target" >&2
      read -p "Press Enter to continue..." _ || true
    fi
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
elif command -v dialog >/dev/null 2>&1; then
  DIALOG=dialog
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
    CHOICES=$($DIALOG --clear --title "zELF Build Menu (mode: $MODE_STR)" \
      --menu "Select an action" 24 100 16 \
      1 "Build everything (make all)" \
      2 "Build packer (make packer)" \
      3 "Build stubs (make stubs)" \
      4 "Build tools (make tools)" \
      5 "Clean (make clean)" \
      6 "Build type: $MODE_STR" \
      7 "Create .deb package (make deb)" \
      8 "Create tar.gz package (make tar)" \
      9 "Install build deps (make install_dependencies)" \
      10 "Quick tests (run test_all.sh)" \
      11 "Show Makefile (open in pager)" \
      12 "Exit" 3>&1 1>&2 2>&3)
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
      7) run_make deb ;;
      8) run_make tar ;;
      9) run_make install_dependencies ;;
      10) run_quick_tests ;;
      11) less "$ROOT_DIR/Makefile" ;;
      12) break ;;
    esac
    if [ "${SKIP_PAUSE:-0}" -ne 1 ]; then
      echo
      echo "Press Enter to continue..."
      read -r _ || true
    fi
    SKIP_PAUSE=0
  done
else
  PS3=$'Choose an action (type number): '
  options=("make all" "make packer" "make stubs" "make tools" "make clean" "build type" "make deb (package)" "make tar (package tar.gz)" "install deps" "quick tests" "show Makefile" "exit")
  while true; do
    echo
    echo "zELF Build Menu"
    echo "(Build type affects subsequent make invocations)"
    # update option label to include current mode
    MODE_STR_TEXT=$( [ "$STATIC_MODE" -eq 1 ] && echo static || echo dynamic )
    options[5]="Build type: $MODE_STR_TEXT"
    select opt in "${options[@]}"; do
      case "$REPLY" in
        1) run_make all; break;;
        2) run_make packer; break;;
        3) run_make stubs; break;;
        4) run_make tools; break;;
        5) run_make clean; break;;
  6) STATIC_MODE=$((1 - STATIC_MODE)); echo "Build type now: $( [ "$STATIC_MODE" -eq 1 ] && echo static || echo dynamic )"; break;;
  7) run_make deb; break;;
  8) run_make tar; break;;
  9) run_make install_dependencies; break;;
  10) run_quick_tests; break;;
  11) less "$ROOT_DIR/Makefile"; break;;
  12) echo "Bye"; exit 0;;
        *) echo "Invalid selection"; break;;
      esac
    done
  done
fi

# End
