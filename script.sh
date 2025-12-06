#!/usr/bin/env bash
set -euo pipefail

# ==========================================
# CONFIGURATION
# ==========================================
DURATION="3600s"   # Set to 3600s for 1 hour, or 60s for a quick test
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ==========================================
# AGGRESSIVE CLEANUP TRAP (Fixes Ctrl+C)
# ==========================================
cleanup() {
    echo -e "\n\n[!] CAUGHT CTRL+C! KILLING EVERYTHING..."
    
    # 1. Kill direct child processes of this script
    pkill -P $$ || true
    
    # 2. Force kill specific binaries (Nuclear option)
    # This ensures no orphaned fuzzer processes stay alive
    pkill -f "afl-fuzz" || true
    pkill -f "png_fuzz_nosani" || true
    pkill -f "png_fuzz_san" || true
    
    echo "[!] All fuzzers terminated. Exiting."
    exit 1
}

# Trap SIGINT (Ctrl+C) and SIGTERM
trap cleanup SIGINT SIGTERM

echo "[*] Project Root: $ROOT"
echo "[*] Fuzzing Duration: $DURATION"

# 1. INSTALL DEPENDENCIES
echo "[*] Checking/Installing dependencies..."
sudo apt-get update -qq
sudo apt-get install -y -qq build-essential git autoconf automake libtool pkg-config zlib1g-dev clang

# 2. SETUP AFL++
if [ ! -d "$ROOT/AFLplusplus" ]; then
    echo "[*] Cloning AFL++..."
    git clone https://github.com/AFLplusplus/AFLplusplus.git
fi
# Only build if afl-fuzz doesn't exist to save time
if [ ! -f "$ROOT/AFLplusplus/afl-fuzz" ]; then
    echo "[*] Building AFL++..."
    make -C "$ROOT/AFLplusplus" distrib > /dev/null 2>&1
fi

# 3. BUILD FUNCTION (Libpng + Harness)
build_target() {
    local mode="$1"
    local bin_name="$2"
    local use_sanitizer="$3"

    echo "[*] Building Target: $bin_name ($mode)"
    
    # Clean clone of libpng
    rm -rf "$ROOT/libpng"
    git clone -q https://github.com/glennrp/libpng.git "$ROOT/libpng"
    cd "$ROOT/libpng" || exit

    # Configure
    [ -x ./autogen.sh ] && ./autogen.sh > /dev/null
    export CC="$ROOT/AFLplusplus/afl-cc"
    
    if [ "$use_sanitizer" = "yes" ]; then
        export AFL_USE_ASAN=1; export AFL_USE_UBSAN=1
    else
        unset AFL_USE_ASAN; unset AFL_USE_UBSAN
    fi

    ./configure --disable-shared > /dev/null
    make -j"$(nproc)" > /dev/null

    # Link Harness
    PNG_LIB="$(ls .libs/libpng*.a | head -n 1)"
    "$CC" -I. ../harness.c "$PNG_LIB" -lz -lm -o "../$bin_name"
    cd "$ROOT"
}

# 4. COMPILE TARGETS
build_target "Part B (No Sanitizers)" "png_fuzz_nosani" "no"
build_target "Part C (Sanitizers)"    "png_fuzz_san"    "yes"

# 5. COMPILE CUSTOM MUTATOR
echo "[*] Compiling Custom Mutator..."
gcc -shared -Wall -O3 custom_mutator.c -o custom_mutator.so -fPIC

# 6. SETUP INPUTS
mkdir -p "$ROOT/in_empty" "$ROOT/in_png"
[ ! -f "$ROOT/in_empty/seed" ] && echo "X" > "$ROOT/in_empty/seed"

# 7. RUN PARALLEL FUZZERS
export AFL_SKIP_CPUFREQ=1
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
export AFL_NO_UI=1

echo "=========================================================="
echo " Launching 4 Fuzzers in PARALLEL for $DURATION..."
echo " Press Ctrl+C at any time to FORCE KILL all jobs."
echo "=========================================================="

# Helper to run quietly in background
run() {
    # The 'setsid' helps keep the process group distinct, but our cleanup trap handles it anyway
    timeout "$DURATION" bash -c "$3" > /dev/null 2>&1 || true &
    echo "[+] Started: $1 -> Output: $2"
}

# Job 1: No Seeds
run "Part B (No Seeds)" "out_no_seeds" \
    "./AFLplusplus/afl-fuzz -i in_empty -o out_no_seeds -- ./png_fuzz_nosani @@"

# Job 2: With Seeds
run "Part B (Seeds)" "out_with_seeds" \
    "./AFLplusplus/afl-fuzz -i in_png -o out_with_seeds -- ./png_fuzz_nosani @@"

# Job 3: Sanitizers
run "Part C (Sanitizers)" "out_sanitizer_seeds" \
    "./AFLplusplus/afl-fuzz -i in_png -o out_sanitizer_seeds -- ./png_fuzz_san @@"

# Job 4: Custom Mutator
(
    export AFL_CUSTOM_MUTATOR_LIBRARY="$ROOT/custom_mutator.so"
    export AFL_CUSTOM_MUTATOR_ONLY=1
    run "Part D (Mutator)" "out_custom_mutator" \
        "./AFLplusplus/afl-fuzz -i in_png -o out_custom_mutator -- ./png_fuzz_san @@"
)

# Wait for timeout or user interrupt
wait
echo "[+] All fuzzing jobs finished (or timed out)."