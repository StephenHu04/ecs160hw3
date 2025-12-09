# HW3 README


---
### 0. Assumptions
- Directory calles in_png/ , is where the 10 image seeds are stored


### 1. Runnining Program

```
chmod +x script.sh
./script.sh
```


All four programs will run simultaneously in the background for an hour before being terminated

Ctrl + C to manually terminate


### 2. Viewing Results

Inside each output folder (out_no_seeds/, out_png_seeds/, etc), look at:

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


### 3. Custom Mutator Logic
## General Idea:
Mutate the image by only mutating safe data chunks and avoid mutating parts the make the file unreadble. Only mutate, IHDR chunk types for bit depth and color, IDAT, ancillary chunks. The mutation would happen over random chunks of the PNG as opposed to the whole PNG.
## Mutator Logic
The mutator first checks if the input is a png by the 8-byte signature, then parses the PNG chunk by chunk. It selects a random chunk and applies a mutation and recalculates the chunk's CRC for PNG validtity before returning the mutated output.