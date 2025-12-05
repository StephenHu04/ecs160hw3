# HW3 README


---
### 0. Assumptions

> - Cloned the libraries already
> - Use Ubuntu & download tools
> - Directory called in_png where the 10 images are stored

Update and download tools if needed
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

Run build_all.sh 

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

### 3. Running AFL++

Without seeds, copy and paste the following cmd

```
bash scripts/run_afl_no_seeds.sh
```


With seeds, copy and paste the following cmd
*Make sure there are 10 images in the directory called in_png/
```
bash scripts/run_afl_with_seeds.sh
```

With seeds + sanitizers, copy and paste the following cmd
```
bash scripts/run_afl_sanitized_with_seeds.sh
```
Ctrl + C to stop




### 4. Viewing Results

Inside each output folder (out_no_seeds/, out_png_seeds/), look at:

```
out_XXX/fuzzer_stats
```
execs_per_sec — throughput
bitmap_cvg — coverage
unique_crashes — number of crashes

```
out_XXX/crashes/
```
Each file here is a crashing input, just count them


