#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import statistics
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


ELAPSED_RE = re.compile(r"in\s+([0-9]+(?:\.[0-9]+)?)\s+seconds")


@dataclass
class RunStats:
    label: str
    samples: list[float]

    @property
    def avg(self) -> float:
        return statistics.fmean(self.samples)

    @property
    def min(self) -> float:
        return min(self.samples)

    @property
    def max(self) -> float:
        return max(self.samples)

    @property
    def stdev(self) -> float:
        if len(self.samples) < 2:
            return 0.0
        return statistics.stdev(self.samples)


def parse_elapsed(output: str) -> float:
    match = ELAPSED_RE.search(output)
    if not match:
        raise ValueError("Unable to extract elapsed time from command output")
    return float(match.group(1))


def run_and_measure(command: list[str], cwd: Path) -> float:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            "Command failed:\n"
            f"{' '.join(command)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )

    combined_output = (result.stdout or "") + "\n" + (result.stderr or "")
    return parse_elapsed(combined_output)


def benchmark_many(
    label: str,
    command: list[str],
    cwd: Path,
    repeats: int,
    warmup: int,
) -> RunStats:
    for _ in range(warmup):
        run_and_measure(command, cwd)

    samples: list[float] = []
    for i in range(repeats):
        elapsed = run_and_measure(command, cwd)
        samples.append(elapsed)
        print(f"{label} run {i + 1}/{repeats}: {elapsed:.6f} s")

    return RunStats(label=label, samples=samples)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compare the same benchmark between Python and C++ implementations"
    )
    parser.add_argument("digits", type=int, help="Number of decimal digits of Pi")
    parser.add_argument("--workers", type=int, default=1, help="Number of workers")
    parser.add_argument("--repeats", type=int, default=5, help="Number of measured runs")
    parser.add_argument("--warmup", type=int, default=1, help="Number of warmup runs")
    parser.add_argument(
        "--python-exe",
        default=sys.executable,
        help="Python interpreter used to run benchmark_pi.py",
    )
    parser.add_argument(
        "--python-script",
        default="benchmark_pi.py",
        help="Path to the Python benchmark script",
    )
    parser.add_argument(
        "--cpp-bin",
        default="./benchmark_pi_cpp",
        help="Path to the C++ benchmark binary",
    )
    parser.add_argument(
        "--cwd",
        default=".",
        help="Working directory used to execute commands",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    if args.digits < 1:
        raise ValueError("digits must be >= 1")
    if args.workers < 1:
        raise ValueError("workers must be >= 1")
    if args.repeats < 1:
        raise ValueError("repeats must be >= 1")
    if args.warmup < 0:
        raise ValueError("warmup must be >= 0")

    cwd = Path(args.cwd).resolve()
    python_script = (cwd / Path(args.python_script)).resolve()
    cpp_bin = (cwd / Path(args.cpp_bin)).resolve()

    python_cmd = [
        args.python_exe,
        str(python_script),
        str(args.digits),
        "--mode",
        "Benchmark",
        "--workers",
        str(args.workers),
    ]

    cpp_cmd = [
        str(cpp_bin),
        str(args.digits),
        "--mode",
        "Benchmark",
        "--workers",
        str(args.workers),
    ]

    print("Running benchmark comparison...")
    print(f"- digits: {args.digits}")
    print(f"- workers: {args.workers}")
    print(f"- repeats: {args.repeats}")
    print(f"- warmup: {args.warmup}")

    python_stats = benchmark_many(
        label="Python",
        command=python_cmd,
        cwd=cwd,
        repeats=args.repeats,
        warmup=args.warmup,
    )

    cpp_stats = benchmark_many(
        label="C++",
        command=cpp_cmd,
        cwd=cwd,
        repeats=args.repeats,
        warmup=args.warmup,
    )

    speedup = python_stats.avg / cpp_stats.avg if cpp_stats.avg > 0 else float("inf")

    print("\nResults (seconds):")
    print("Impl    Avg        Min        Max        StdDev")
    print(
        f"Python  {python_stats.avg:9.6f}  {python_stats.min:9.6f}  "
        f"{python_stats.max:9.6f}  {python_stats.stdev:9.6f}"
    )
    print(
        f"C++     {cpp_stats.avg:9.6f}  {cpp_stats.min:9.6f}  "
        f"{cpp_stats.max:9.6f}  {cpp_stats.stdev:9.6f}"
    )
    print(f"\nAverage C++ vs Python speedup: {speedup:.2f}x")


if __name__ == "__main__":
    main()
