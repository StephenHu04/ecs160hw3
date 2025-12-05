#!/usr/bin/env bash
set -euo pipefail

if [ ! -d "seeds" ]; then
  echo "Put 10 PNG files in ./seeds/"
  exit 1
fi

mkdir -p out_seeds

AFL_SKIP_CPUFREQ=1 \
AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
"$ROOT/AFLplusplus/afl-fuzz" \
  -i in_empty \
  -o out_no_seeds \
  -- "$ROOT/png_fuzz" @@