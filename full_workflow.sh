#!/usr/bin/env bash
set -euo pipefail

###############################################
# 0. SETUP & DEPENDENCIES
###############################################

# Resolve project root to the directory where this script lives
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

echo "[*] Project root: $ROOT"

echo "[*] Installing required packages (build-essential, git, autoconf, libtool, zlib1g-dev, clang)..."
sudo apt update
sudo apt install -y build-essential git autoconf automake libtool pkg-config zlib1g-dev clang

###############################################
# 1. CLONE + BUILD AFL++
###############################################

if [ ! -d "$ROOT/AFLplusplus" ]; then
  echo "[*] Cloning AFL++..."
  git clone https://github.com/AFLplusplus/AFLplusplus.git
fi

echo "[*] Building AFL++ (this may take a bit)..."
cd "$ROOT/AFLplusplus"
make distrib
cd "$ROOT"

###############################################
# 2. FUNCTION: BUILD libpng + harness
###############################################
# build_libpng <mode-name> <output-binary> <sanitize: yes|no>
###############################################

build_libpng() {
  local mode="$1"
  local output_bin="$2"
  local sanitize="$3"

  echo
  echo "===================================="
  echo " Building libpng ($mode) -> $output_bin"
  echo "===================================="

  # Always start from a clean libpng clone to avoid mixing configs
  rm -rf "$ROOT/libpng"
  git clone https://github.com/glennrp/libpng.git "$ROOT/libpng"
  cd "$ROOT/libpng"

  # Generate configure if needed
  if [ ! -f ./configure ]; then
    if [ -x ./autogen.sh ]; then
      echo "[*] Running autogen.sh..."
      ./autogen.sh
    else
      echo "[!] No configure or autogen.sh found - this would be unusual for libpng."
    fi
  fi

  # Use AFL++ compiler
  export CC="$ROOT/AFLplusplus/afl-cc"

  if [ "$sanitize" = "yes" ]; then
    echo "[*] Enabling sanitizers (AFL_USE_ASAN + AFL_USE_UBSAN)..."
    export AFL_USE_ASAN=1
    export AFL_USE_UBSAN=1
  else
    unset AFL_USE_ASAN || true
    unset AFL_USE_UBSAN || true
  fi

  echo "[*] Configuring libpng..."
  ./configure --disable-shared

  echo "[*] Building libpng..."
  make -j"$(nproc)"

  # We are still inside libpng here.
  # Find the static library: it might be libpng16.a, libpng18.a, etc.
  PNG_AR="$(ls .libs/libpng*.a | head -n 1 || true)"
  if [ -z "$PNG_AR" ]; then
    echo "[-] ERROR: Could not find .libs/libpng*.a after build."
    exit 1
  fi
  echo "[*] Using libpng static archive: $PNG_AR"

  echo "[*] Building harness -> $output_bin"
  # We are in $ROOT/libpng:
  #  - headers are in current dir (png.h etc.), so use -I.
  #  - harness.c is one level up.
  #  - output binary goes one level up in $ROOT.
  "$ROOT/AFLplusplus/afl-cc" \
    -I. \
    ../harness.c \
    "$PNG_AR" \
    -lz -lm \
    -o "../$output_bin"

  cd "$ROOT"

  if [ ! -x "$ROOT/$output_bin" ]; then
    echo "[-] ERROR: Failed to build $output_bin"
    exit 1
  fi

  echo "[+] Built $output_bin successfully."
}

###############################################
# 3. BUILD BINARIES FOR PART B & C
###############################################

# Part B: non-sanitized binary
build_libpng "no-sanitizers (Part B)" "png_fuzz_nosani" "no"

# Part C: sanitized binary (ASAN + UBSAN)
build_libpng "ASAN+UBSAN (Part C)" "png_fuzz_san" "yes"

echo
echo "===================================="
echo " Built fuzzing targets:"
echo "   - $ROOT/png_fuzz_nosani  (Part B)"
echo "   - $ROOT/png_fuzz_san     (Part C & D)"
echo "===================================="

###############################################
# 4. PREPARE INPUT DIRS (EMPTY + SEEDS)
###############################################

# For Part B (no seeds)
mkdir -p "$ROOT/in_empty"
if [ ! -f "$ROOT/in_empty/seed" ]; then
  echo "X" > "$ROOT/in_empty/seed"
fi

# Seed directory (you must put PNGs here yourself)
mkdir -p "$ROOT/in_png"

echo
echo "===================================="
echo " Input directories prepared:"
echo "   - in_empty/  (dummy seed for no-seed fuzzing)"
echo "   - in_png/    (PUT ~10 PNG FILES HERE for seeded fuzzing)"
echo "===================================="

