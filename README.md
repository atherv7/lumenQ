# lumenQ

An ultra-low latency, lock-free Inter-Process Communication (IPC) ring buffer pipeline written in C, Designed for performance-critical architecture. `lumenQ` achieves sub-30 nanosecond transit times by operating entirely within the CPU cache hierarchy.

```
==================================================
   LUMENQ LOCK-FREE INTER-PROCESS LATENCY SHEET
==================================================
 Total Processed Packets : 10000000
 CPU Speed Estimate      : 3.31 GHz
--------------------------------------------------
 Average Latency         :   21.24 ns
 p50 (Median)            :   21.00 ns
 p95                     :   22.00 ns
 p99                     :   23.00 ns
 p99.99 (Tail Latency)   :   24.00 ns
==================================================
```

## Key Architectural Features

* **Lock-Free Atomic Synchronization:** Utilizes fine-grained C11 atomic primitives (`memory_order_release` / `memory_order_acquire`) to pass frame frame indexes between boundaries without using blocking kernel primitives (mutexes, semaphores).
* **Zero-Copy Cache-Line Alignment:** Enforces rigid structural padding (`__attribute__((aligned(CACHE_LINE_SIZE))`) on transmission frames to map exactly to the underlying CPU architecture, eliminating false sharing and cache-line invalidation thrashing.
* **Deterministic "Drop & Log" Safety Invariant:** When the ring buffer saturates, the producer maintains its strict execution budget by instantly bypassing the transaction and atomically tracking data loss via an out-of-band `overflow_count` state block rather than stalling.
* **OS Jitter Eradication:** Integrates OS kernel tuning routines to pin memory allocations directly to physicals RAM page tables (`mlockall`) and isolate processing threads onto dedicated hardware cores (`pthread_setaffinity_np`).

---

## Getting Started & Building

### Prerequisites

* A Linux environment (Arch Linux or Ubuntu preferred)
* CMake v3.15+
* GCC or Clang compiler supporting C11 atomics

### Makefile Commands

The project includes a top-level `Makefile` wrapper that automatically configures an optimized Release build configuration (`-O3`) and executes tests across all available CPU cores. The `Makefile` also allows benchmarking the current software, and formatting the codebase.

```bash
# Build the production library and all verification tools
make build

# Clean up build artifacts
make clean

# Benchmark software
make benchmark

# Test functionality
make test

# Format codebase
make format
```
