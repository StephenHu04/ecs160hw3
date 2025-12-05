#!/usr/bin/env bash
set -euo pipefail

# PROJECT ROOT = parent directory of this script
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$ROOT"

echo "[*] Project root is: $ROOT"

# === 1. Clone AFL++ in $ROOT/AFLplusplus ===
if [ ! -d "AFLplusplus" ]; then
  git clone https://github.com/AFLplusplus/AFLplusplus.git
fi

cd AFLplusplus
make distrib
cd "$ROOT"

# === 2. Clone libpng in $ROOT/libpng ===
if [ ! -d "libpng" ]; then
  git clone https://github.com/glennrp/libpng.git
fi

export CC="$ROOT/AFLplusplus/afl-cc"
export CXX="$ROOT/AFLplusplus/afl-c++"

cd libpng

# Generate ./configure if missing (git clone case)
if [ ! -f configure ]; then
  echo "[*] Running autogen.sh to generate configure..."
  ./autogen.sh
fi

./configure --disable-shared
make clean
make -j"$(nproc)"
cd "$ROOT"

# === 3. Compile harness.c into $ROOT/png_fuzz ===
$CC \
  -Ilibpng \
  -Ilibpng/include \
  -Ilibpng/include/libpng16 \
  harness.c \
  libpng/.libs/libpng16.a \
  -lz -o png_fuzz

echo "[+] Built png_fuzz successfully at: $ROOT/png_fuzz"
