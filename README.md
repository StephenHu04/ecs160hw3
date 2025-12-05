# HW3 README


---
### 0. Assumptions

> - Cloned the libraries already
> - Use Ubuntu & download tools
> - Directory called in_png where the 10 images are stored



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
export AFL_SKIP_CPUFREQ=1
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1

./AFLplusplus/afl-fuzz
  -i in_png
  -o out_with_seeds
  -- ./png_fuzz_nosani @@

```
[PART B – WITH PNG SEEDS]
  (put ~10 PNGs into in_png/ FIRST)
```
export AFL_SKIP_CPUFREQ=1
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1

./AFLplusplus/afl-fuzz \
  -i in_png \
  -o out_with_seeds \
  -- ./png_fuzz_nosani @@
```
[PART C – ASAN + UBSAN + PNG SEEDS]
  (uses sanitized binary)
```
export AFL_SKIP_CPUFREQ=1
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1

./AFLplusplus/afl-fuzz \
  -i in_png \
  -o out_sanitizer_seeds \
  -- ./png_fuzz_san @@
```
[PART D – CUSTOM MUTATOR + SANITIZED + PNG SEEDS]
```
gcc -shared -Wall -O3 custom_mutator.c -o custom_mutator.so -fPIC
```
```
export AFL_CUSTOM_MUTATOR_LIBRARY=$(pwd)/custom_mutator.so
```
```
export AFL_CUSTOM_MUTATOR_LIBRARY=./custom_mutators/libpng_mutator.so
export AFL_CUSTOM_MUTATOR_ONLY=1
export AFL_SKIP_CPUFREQ=1
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1

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


