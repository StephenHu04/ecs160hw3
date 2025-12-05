# HW3 README


---
### 0. Assumptions

> - Cloned the libraries already
> - Use Ubuntu & download tools
```
sudo apt update
sudo apt install -y build-essential autoconf automake libtool pkg-config zlib1g-dev clang git
```
---
---

### 1. Structure

How our project is structured

ecs160hw3/
│
├── harness.c          ← Test program that reads PNG files using libpng
├── png_fuzz           ← Fuzzing target (created after build)
│
├── libpng/            ← LibPNG source (cloned from GitHub)
│   └── .libs/libpngXX.a  ← Static library created after building
│
├── AFLplusplus/       ← AFL++ fuzzer (cloned + compiled)
│
├── in_empty/          ← Input directory for fuzzing without seeds
├── in_png/            ← Directory for seed PNG files
│
├── out_no_seeds/      ← AFL output for the no-seed fuzzing run
├── out_png_seeds/     ← AFL output for seeded fuzzing run
│
└── README.md


### 2. AFL++ & libpng

Run our build_afl.sh 

or


From your project root:
```
git clone https://github.com/AFLplusplus/AFLplusplus.git
cd AFLplusplus
make distrib
cd ..
```

You should have these files
```
AFLplusplus/afl-fuzz
AFLplusplus/afl-cc
```


LIBPNG
```
rm -rf libpng
git clone https://github.com/glennrp/libpng.git
cd libpng

# Generate configure script if missing
[ ! -f configure ] && ./autogen.sh

# Build with AFL compiler
CC=../AFLplusplus/afl-cc ./configure --disable-shared
make -j"$(nproc)"

cd ..
```
After building verify with
```
ls libpng/.libs/
```
should see libpng18.a

### 3. Building png_fuzz target


```
./AFLplusplus/afl-cc \
  -Ilibpng \
  harness.c \
  libpng/.libs/libpng18.a \
  -lz -lm \
  -o png_fuzz
```
Vreify with 

```
ls -l png_fuzz
```

### 4. Running AFL++

Without seeds, copy and paste the following cmd

```
AFL_SKIP_CPUFREQ=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
./AFLplusplus/afl-fuzz \
  -i in_empty \
  -o out_no_seeds \
  -- ./png_fuzz @@
```


With seeds, copy and paste the following cmd
*Make sure there are 10 images in the directory called in_png/
```
AFL_SKIP_CPUFREQ=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
./AFLplusplus/afl-fuzz \
  -i in_png \
  -o out_png_seeds \
  -- ./png_fuzz @@
```
Ctrl + C to stop




### 5. Viewing Results

Inside each output folder (out_no_seeds/, out_png_seeds/), look at:

```
out_XXX/fuzzer_stats
```
execs_per_sec — throughput
bitmap_cvg — coverage
unique_crashes — number of crashes
paths_total — number of paths explored

```
out_XXX/crashes/
```
Each file here is a crashing input, just count them

### 6. Running with Sanitizers (ASAN + UBSAN)
Rebuild libpng and png_fuzz using:
```
export AFL_USE_ASAN=1
export AFL_USE_UBSAN=1
```
Then run fuzzing again using the PNG seed directory.

1. Remove old build
```
rm -rf libpng
rm png_fuzz
```
2. Clone libpng again
```
git clone https://github.com/glennrp/libpng.git
cd libpng
./autogen.sh
cd ..
```
3. Enable sanitizers
```
export AFL_USE_ASAN=1
export AFL_USE_UBSAN=1
```
4. Rebuild libpng with sanitizers
```
cd libpng
CC=../AFLplusplus/afl-cc ./configure --disable-shared
make -j"$(nproc)"
cd ..
```
5. Rebuild png_fuzz
```./AFLplusplus/afl-cc \
  -Ilibpng \
  harness.c \
  libpng/.libs/libpng18.a \
  -lz -lm \
  -o png_fuzz
```

Now you're using sanitizers.



