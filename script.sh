#!/usr/bin/env bash
set -euo pipefail

# config
DURATION="3600s"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# check required files
for file in harness.c custom_mutator.c; do
    if [ ! -f "$ROOT/$file" ]; then
        echo "[!] Error: Missing required file '$file' in $ROOT"
        exit 1
    fi
done

# check input directory for seeds
if [ ! -d "$ROOT/in_png" ] || [ -z "$(ls -A "$ROOT/in_png")" ]; then
   echo "[!] Error: 'in_png' directory is empty or missing!"
   exit 1
fi

# clean previous runs info
echo "[*] Cleaning previous run data..."
rm -rf "$ROOT/out_no_seeds" "$ROOT/out_with_seeds" "$ROOT/out_sanitizer_seeds" "$ROOT/out_custom_mutator"


cleanup() {
    echo -e "\n\n[!] Terminate EVERYTHING..."
    pkill -P $$ || true
    pkill -f "afl-fuzz" || true
    pkill -f "png_fuzz_nosani" || true
    pkill -f "png_fuzz_san" || true
    echo "[!] All fuzzers terminated. Exiting."
    exit 1
}
trap cleanup SIGINT SIGTERM

echo "=========================================================="
echo "[*] Project Root: $ROOT"
echo "[*] Fuzzing Duration: $DURATION"
echo "=========================================================="

# check/install dependencies
echo "[*] Checking/Installing dependencies..."
sudo apt-get update -qq
sudo apt-get install -y -qq build-essential git autoconf automake libtool pkg-config zlib1g-dev clang

# setup AFL++
if [ ! -d "$ROOT/AFLplusplus" ]; then
    echo "[*] Cloning AFL++..."
    git clone https://github.com/AFLplusplus/AFLplusplus.git
fi
if [ ! -f "$ROOT/AFLplusplus/afl-fuzz" ]; then
    echo "[*] Building AFL++..."
    make -C "$ROOT/AFLplusplus" distrib > /dev/null 2>&1
fi

# build target function
build_target() {
    local mode="$1"
    local bin_name="$2"
    local use_sanitizer="$3"

    echo "[*] Building Target: $bin_name ($mode)"
    
    # Don't re-clone if it exists, just clean it
    if [ ! -d "$ROOT/libpng" ]; then
        git clone -q https://github.com/glennrp/libpng.git "$ROOT/libpng"
    fi
    
    cd "$ROOT/libpng" || exit
    # Reset git to ensure clean build state
    git clean -fdx > /dev/null
    
    [ -x ./autogen.sh ] && ./autogen.sh > /dev/null
    export CC="$ROOT/AFLplusplus/afl-cc"
    
    if [ "$use_sanitizer" = "yes" ]; then
        export AFL_USE_ASAN=1; export AFL_USE_UBSAN=1
    else
        unset AFL_USE_ASAN; unset AFL_USE_UBSAN
    fi

    ./configure --disable-shared > /dev/null
    make -j"$(nproc)" > /dev/null

    PNG_LIB="$(ls .libs/libpng*.a | head -n 1)"
    "$CC" -I. ../harness.c "$PNG_LIB" -lz -lm -o "../$bin_name"
    cd "$ROOT"
}

# build targets
build_target "Part B (No Sanitizers)" "png_fuzz_nosani" "no"
build_target "Part C (Sanitizers)"    "png_fuzz_san"    "yes"

# compile custom mutator
echo "[*] Compiling Custom Mutator..."
gcc -shared -Wall -O3 custom_mutator.c -o custom_mutator.so -fPIC

# prepare directories
mkdir -p "$ROOT/in_empty"
[ ! -f "$ROOT/in_empty/seed" ] && echo "X" > "$ROOT/in_empty/seed"

# run fuzzers in parallel
export AFL_SKIP_CPUFREQ=1
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
export AFL_NO_UI=1

echo "=========================================================="
echo " Launching 4 all Fuzzers in running in background in parrallel for $DURATION..."
echo " Press Ctrl+C at any time to terminate all jobs."
echo "=========================================================="

run() {
    timeout "$DURATION" bash -c "$3" > /dev/null 2>&1 || true &
    echo "[+] Started: $1"
}

# part B without seeds
run "Part B (No Seeds)" "out_no_seeds" \
    "./AFLplusplus/afl-fuzz -i in_empty -o out_no_seeds -- ./png_fuzz_nosani @@"

#part B with seeds
run "Part B (Seeds)" "out_with_seeds" \
    "./AFLplusplus/afl-fuzz -i in_png -o out_with_seeds -- ./png_fuzz_nosani @@"

#  part C
run "Part C (Sanitizers)" "out_sanitizer_seeds" \
    "./AFLplusplus/afl-fuzz -i in_png -o out_sanitizer_seeds -- ./png_fuzz_san @@"

#  part D
(
    export AFL_CUSTOM_MUTATOR_LIBRARY="$ROOT/custom_mutator.so"
    export AFL_CUSTOM_MUTATOR_ONLY=1
    run "Part D (Mutator)" "out_custom_mutator" \
        "./AFLplusplus/afl-fuzz -i in_png -o out_custom_mutator -- ./png_fuzz_san @@"
)

# wait for all background jobs to finish
wait
echo "=========================================================="
echo "[+] All fuzzing jobs finished (or timed out)."
echo "[*] Check the 'out_*' directories for result status."
echo "=========================================================="

