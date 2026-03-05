# PiBenchmark

A benchmark tool to measure how long it takes to compute `N` decimal digits of π.

Two implementations are included:
- Python: `benchmark_pi.py`
- C++: `benchmark_pi.cpp`

It supports both `SingleCore` and `MultiCore` execution using multiple processes/workers.

## Requirements

- Python 3.10+ (tested on Python 3.13)
- C++17 compiler (for example `g++`)

## Makefile

Available quick targets:

- `make cpp`: build the C++ binary (`benchmark_pi_cpp`)
- `make python-check`: run Python syntax check (`py_compile`)
- `make test`: run `cpp` + `python-check`
- `make compare`: compare the same benchmark on Python and C++ (default: all CPU cores)
- `make compare-single-core`: compare using exactly 1 worker/core
- `make run-cpp`: run an example C++ benchmark
- `make run-py`: run an example Python benchmark
- `make clean`: remove the C++ binary

Default `make compare` values are: `DIGITS=7000`, `WORKERS=<all logical CPU cores>`, `REPEATS=6`, `WARMUP=2`.

Comparison example:

```bash
make compare DIGITS=10000 WORKERS=4 REPEATS=7 WARMUP=1
```

The comparison uses `compare_benchmarks.py` and prints avg/min/max/standard deviation for both implementations, plus the average C++ vs Python speedup.

## C++ Version

Build:

```bash
g++ -O2 -std=c++17 -pthread benchmark_pi.cpp -o benchmark_pi_cpp
```

MultiCore benchmark example:

```bash
./benchmark_pi_cpp 10000 --mode Benchmark --workers 8
```

MultiCore StressTest example:

```bash
./benchmark_pi_cpp 5000 --mode StressTest --workers 8 --print-every 50
```

CLI options are aligned with the Python version (`--mode`, `--show-pi`, `--workers`, `--print-every`).

## Python Version

From the project folder:

```bash
python3 benchmark_pi.py 10000 --mode Benchmark
```

MultiCore example (uses 8 processes):

```bash
python3 benchmark_pi.py 10000 --mode Benchmark --workers 8
```

### Available Modes

- `Benchmark`: runs a single measurement.
- `StressTest`: repeats benchmark batches until interrupted (`Ctrl+C`), using parallel batches when `--workers > 1`.

When `StressTest` stops, a summary is shown with total batches, total calculations, avg/min/max batch time, total duration, and throughput (`calculations/s`).

During `StressTest`, live throughput (`calculations/s`) is printed every 10 batches (plus the first batch).

StressTest example:

```bash
python3 benchmark_pi.py 5000 --mode StressTest
```

StressTest example printing every 50 batches:

```bash
python3 benchmark_pi.py 5000 --mode StressTest --workers 8 --print-every 50
```

### Options

- `digits` (required): number of decimal digits to compute.
- `--mode`: `Benchmark` or `StressTest` (default: `Benchmark`).
- `--show-pi`: show the computed π value.
- `--workers`: number of processes/workers to use (default: number of logical CPU cores).
- `--print-every`: print progress every N batches in `StressTest` (default: `10`).
