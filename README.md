# Overview

This was developed as part of the **Computer Organization (OC)** course at IST-UL.
There were 3 deliverables, with main objective of analysing the architecture of a computer and how does the code affect it.

## Lab1 (Cache Characterization and Optimization)

This project analyzes and models the behavior of a computer’s memory hierarchy (L1 and L2 caches) and applies this knowledge to optimize program performance.

The work is divided into two main objectives:
- Cache Characterization
  + Determine the characteristics of L1 and L2 caches
  + Estimate cache size, block size, and associativity
  + Use hardware performance counters via PAPI (Performance API)
- Performance Optimization
  + Analyze cache behavior in a matrix multiplication program
  + Apply optimizations such as:
    + Matrix transposition
    + Blocked (tiled) matrix multiplication
  + Evaluate improvements using cache hit/miss statistics and execution time
 
## Lab2 (TLB Simulator)

The objective was to implement a 2-level TLB system on top of a memory translation simulator.

Both TLB levels:
- Are fully associative
- Use an LRU (Least Recently Used) eviction policy
- Follow a write-back policy

The simulator correctly handle:
- Address translation (VA → PA)
- Page table interactions
- Cache hits and misses
- TLB invalidation

### Compilation

- Compile using the `Makefile`.
- Use `./build/tlbsim` to start the simulator.

### Technologies

- C

## Lab3 (RISC-V Pipeline Optimization)

The goal was to understand how pipelined processor architectures work and how software optimizations can improve execution efficiency.

Using the Ripes simulator, we analysed how instructions are executed in a 5-stage RISC-V pipeline and how hazards (data, structural and control) affect performance.

The main objective was to:
- Observe instruction execution in a pipeline
- Measure CPI (Cycles Per Instruction)
- Identify pipeline hazards
- Apply optimization techniques such as:
  + Data forwarding
  + Instruction reordering
  + Loop unrolling
 
### Technologies

- [Ripes](https://github.com/mortbopet/Ripes)
- RISC-V Assembly
