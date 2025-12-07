# HW3 README


---
### 0. Assumptions

> - Cloned the libraries already
> - Use Ubuntu & download tools
> - Directory called in_png where the 10 images are stored



### 1. AFL++ & libpng

Run script.sh
```
bash script.sh
```


### 2. Running AFL++
All four programs will run simultaneously for an hour before being terminated


Ctrl + C to stop




### 3. Viewing Results

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



