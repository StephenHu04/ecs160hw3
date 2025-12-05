#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

mkdir -p in_empty
echo "X" > in_empty/seed

mkdir -p out_no_seeds

AFL_SKIP_CPUFREQ=1 \
AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
"$ROOT/AFLplusplus/afl-fuzz" \
  -i in_empty \
  -o out_no_seeds \
  -- "$ROOT/png_fuzz" @@
