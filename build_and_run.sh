#!/bin/zsh
set -euo pipefail

# Build and run audioSample.c with a chosen mode.
# Usage:
#   ./build_and_run.sh <mode>
# Examples:
#   ./build_and_run.sh 0
#   ./build_and_run.sh 1
#   ./build_and_run.sh 2

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
  echo "Usage: $0 {0|1|2}" >&2
  exit 2
fi

echo "[LOG] Building audioSample.c and running with mode=$MODE..."
clang -O2 audioSample.c -o audioSample
./audioSample "$MODE"


