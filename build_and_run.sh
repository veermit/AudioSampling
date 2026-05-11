#!/bin/zsh
set -euo pipefail

# Build and run audioSample.c with a chosen mode.
# Usage:
#   ./build_and_run.sh <mode>
# Examples:
#   ./build_and_run.sh 0
#   ./build_and_run.sh 1
#   ./build_and_run.sh 2
# Cleanup:
#   ./build_and_run.sh clean

OUT_ROOT="outputs"

cleanup() {
  rm -rf "$OUT_ROOT/mode_0" "$OUT_ROOT/mode_1" "$OUT_ROOT/mode_2" "$OUT_ROOT" || true
}

if [[ ${1:-} == "clean" ]]; then
  echo "[LOG] Cleaning output directories..."
  cleanup
  exit 0
fi

if [[ ${1:-} == "" ]]; then
  echo "Select DSP mode:"
  echo "  0 = no filtering (naive decimation + sample-hold)"
  echo "  1 = FIR anti-alias down + FIR reconstruction (group-delay compensated)"
  echo "  2 = FIR anti-alias down + FIR reconstruction (no group-delay compensation)"
  echo -n "Enter 0/1/2: "
  read -r MODE
else
  MODE="$1"
fi

if [[ "$MODE" != "0" && "$MODE" != "1" && "$MODE" != "2" ]]; then
  echo "Usage: $0 {0|1|2} | clean" >&2
  exit 2
fi

OUT_DIR="$OUT_ROOT/mode_${MODE}"
mkdir -p "$OUT_DIR"

echo "[LOG] Building audioSample.c and running with mode=$MODE..."
clang -O2 audioSample.c -o audioSample
./audioSample "$MODE" "$OUT_DIR"



