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

### 1. AFL++ & libpng

Run build_all.sh 
```
bash build_all.sh
```

After building verify with
```
ls libpng/.libs/
```
should see libpng18.a

### 2. Running AFL++

[PART B – NO SEEDS]
  (fuzzing png_fuzz_nosani with a dummy seed file)
```
AFL_SKIP_CPUFREQ=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
./AFLplusplus/afl-fuzz \
  -i in_empty \
  -o out_no_seeds_fresh \
  -- ./png_fuzz_nosani @@
```
[PART B – WITH PNG SEEDS]
  (put ~10 PNGs into in_png/ FIRST)
```
AFL_SKIP_CPUFREQ=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
./AFLplusplus/afl-fuzz \
  -i in_png \
  -o out_with_seeds \
  -- ./png_fuzz_nosani @@
```
[PART C – ASAN + UBSAN + PNG SEEDS]
  (uses sanitized binary)
```
AFL_SKIP_CPUFREQ=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
./AFLplusplus/afl-fuzz \
  -i in_png \
  -o out_sanitizer_seeds \
  -- ./png_fuzz_san @@
```
[PART D – CUSTOM MUTATOR + SANITIZED + PNG SEEDS]
  (assumes your friend’s mutator is at:
     $ROOT/custom_mutators/libpng_mutator.so )
```
AFL_CUSTOM_MUTATOR_LIBRARY=./custom_mutators/libpng_mutator.so \
AFL_CUSTOM_MUTATOR_ONLY=1 \
AFL_SKIP_CPUFREQ=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
./AFLplusplus/afl-fuzz \
  -i in_png \
  -o out_custom_mutator \
  -- ./png_fuzz_san @@
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


