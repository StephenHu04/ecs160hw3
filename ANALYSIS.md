# SPECS
- CPU: Intel Core i5-12600KF (10 cores, 16 threads, 3.7 Ghz)
- Memory: 32GB Corsair Vengeance DDR4 (2x16 GB, 3600 Mhz)
- Storage: 1Tb NVMe SDD
- OS: Windows 11 Pro
- Notes: WSL2 is used to run experiment

# WSL2 Enviroment:
- Distro: Ubuntu 24.04
- Kernel: Linux 6.6.87.2-microsoft-standard-WSL2
- Compiler: clang 18.1.3, gcc 13.3.0
- AFL++: built from source

# Stats
## No seed
- Number of crashes:0
- Coverage: 0.09%
- Execution Throughput:99.57
- Edge counts:7

## With seed
- Number of crashes:0
- Coverage: 8.77%
- Execution Throughput:57.34
- Edge counts:655

## With seed + ASAN & UBSAN
- Number of crashes:0
- Coverage:5.83%
- Execution Throughput:55.11
- Edge counts:1677

## With Custom Mutator
- Number of crashes:0
- Coverage:6.32%
- Execution Throughput:46.44
- Edge counts:1818

# Custom Mutator Logic

## General Idea:
Mutate the image by only mutating safe data chunks and avoid mutating parts the make the file unreadble. Only mutate, IHDR chunk types for bit depth and color, IDAT, ancillary chunks. The mutation would happen over random chunks of the PNG as opposed to the whole PNG.
## Mutator Implementation
The mutator first checks if the input is a png by the 8-byte signature, then parses the PNG chunk by chunk. It selects a random chunk and applies a mutation and recalculates the chunk's CRC for PNG validtity before returning the mutated output.

# Analysis
- In part B, the fuzzing with no seed provided the highest throughput and detected the least amount of edge cases as it failed very early on due to invalid PNG inputs from mutating. This lead to an immediate reject. Adding seed inputs in, hugely, increased coverage, indicating that valid inputs are essential for exploring the code path. However the amount of edges found was still on the lower end. 

- For Part C, enabling ASAN and UBSAN introduced runtime checks for memory errors and undefined behavior, leading to an increased number of edges found. However, this significantly lowered throughput increasing overhead.

- For Part D, the custom mutator's coverage improved by generating valid, structure aware mutations that targeted specific PNG chunks, allowing fuzzer to reach deeper paths in the parser exploring more edges. However this came at a cost of slower execution due to  extra calculations to maintain PNGs validity after mutation. Overall, these results highlights the tradeoff between execution speed and input quality: random muations are fast but shallow and inefficient at finding edges. Seeds can help impove coverage by providing some valid inputs, and using ASAN helps detect UB errors and memory errors at a cost of additional overhead.