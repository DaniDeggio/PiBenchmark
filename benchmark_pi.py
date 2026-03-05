#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import math
import time
from concurrent.futures import ProcessPoolExecutor
from dataclasses import dataclass
from decimal import Decimal, getcontext


@dataclass
class BenchmarkResult:
    digits: int
    elapsed_seconds: float
    pi_value: str


def calculate_pi_digits(digits: int) -> str:
    if digits < 1:
        raise ValueError("The number of digits must be >= 1")

    extra_precision = 10
    getcontext().prec = digits + extra_precision

    c = 426880 * Decimal(10005).sqrt()
    k = 0
    m = 1
    l = 13591409
    x = 1
    s = Decimal(l)

    # Each term adds about 14 correct decimal digits
    terms = max(1, math.ceil((digits + extra_precision) / 14))

    for i in range(1, terms):
        k += 12
        m = (m * (k**3 - 16 * k)) // (i**3)
        l += 545140134
        x *= -262537412640768000
        s += Decimal(m * l) / Decimal(x)

    pi = c / s

    # Returns exactly "3." + N decimal digits
    pi_str = format(pi, f".{digits}f")
    return pi_str


def _compute_elapsed(digits: int) -> float:
    start = time.perf_counter()
    calculate_pi_digits(digits)
    return time.perf_counter() - start


def run_single_benchmark(digits: int, show_pi: bool, workers: int) -> BenchmarkResult:
    if workers < 1:
        raise ValueError("workers must be >= 1")

    if workers == 1:
        start = time.perf_counter()
        pi_value = calculate_pi_digits(digits)
        elapsed = time.perf_counter() - start

        if show_pi:
            print(f"π ({digits} digits):")
            print(pi_value)

        return BenchmarkResult(digits=digits, elapsed_seconds=elapsed, pi_value=pi_value)

    if show_pi:
        print(f"π ({digits} digits):")
        print(calculate_pi_digits(digits))

    start = time.perf_counter()
    with ProcessPoolExecutor(max_workers=workers) as executor:
        futures = [executor.submit(_compute_elapsed, digits) for _ in range(workers)]
        task_times = [future.result() for future in futures]
    wall_elapsed = time.perf_counter() - start

    print(
        f"MultiCore Benchmark ({workers} workers): "
        f"batch in {wall_elapsed:.6f} s | "
        f"average task time: {sum(task_times) / len(task_times):.6f} s"
    )

    return BenchmarkResult(digits=digits, elapsed_seconds=wall_elapsed, pi_value="")


def run_stress_test(digits: int, show_pi: bool, workers: int, print_every: int) -> None:
    if workers < 1:
        raise ValueError("workers must be >= 1")
    if print_every < 1:
        raise ValueError("print_every must be >= 1")

    print(
        f"StressTest mode started (workers: {workers}). "
        "Press Ctrl+C to stop."
    )

    if show_pi:
        print(f"π ({digits} digits):")
        print(calculate_pi_digits(digits))

    stress_start = time.perf_counter()
    batch_iteration = 0
    total_batch_time = 0.0
    min_batch_time = float("inf")
    max_batch_time = 0.0

    with ProcessPoolExecutor(max_workers=workers) as executor:
        try:
            while True:
                batch_iteration += 1
                batch_start = time.perf_counter()
                futures = [executor.submit(_compute_elapsed, digits) for _ in range(workers)]
                task_times = [future.result() for future in futures]
                batch_elapsed = time.perf_counter() - batch_start

                total_batch_time += batch_elapsed
                min_batch_time = min(min_batch_time, batch_elapsed)
                max_batch_time = max(max_batch_time, batch_elapsed)
                avg_batch_time = total_batch_time / batch_iteration

                total_elapsed = time.perf_counter() - stress_start
                total_calculations = batch_iteration * workers
                live_throughput = (
                    total_calculations / total_elapsed if total_elapsed > 0 else 0.0
                )

                if batch_iteration == 1 or batch_iteration % print_every == 0:
                    print(
                        f"Batch {batch_iteration}: {batch_elapsed:.6f} s "
                        f"(batch avg: {avg_batch_time:.6f} s, "
                        f"task avg: {sum(task_times) / len(task_times):.6f} s, "
                        f"throughput: {live_throughput:.2f} calculations/s)"
                    )
        except KeyboardInterrupt:
            print("\nStressTest interrupted by user.")
            if batch_iteration > 0:
                total_elapsed = time.perf_counter() - stress_start
                total_calculations = batch_iteration * workers
                throughput = (
                    total_calculations / total_elapsed if total_elapsed > 0 else 0.0
                )

                print(
                    f"Total batches: {batch_iteration} | "
                    f"Total calculations: {total_calculations} | "
                    f"Average batch time: {total_batch_time / batch_iteration:.6f} s | "
                    f"Min batch: {min_batch_time:.6f} s | "
                    f"Max batch: {max_batch_time:.6f} s"
                )
                print(
                    f"Total duration: {total_elapsed:.3f} s | "
                    f"Throughput: {throughput:.2f} calculations/s"
                )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Benchmark for computing decimal digits of Pi"
    )
    parser.add_argument(
        "digits",
        type=int,
        help="Number of Pi decimal digits to compute",
    )
    parser.add_argument(
        "--mode",
        choices=["Benchmark", "StressTest"],
        default="Benchmark",
        help="Execution mode",
    )
    parser.add_argument(
        "--show-pi",
        action="store_true",
        help="Show the computed Pi value",
    )
    parser.add_argument(
        "--workers",
        type=int,
        default=os.cpu_count() or 1,
        help="Number of processes to use (default: logical CPU cores)",
    )
    parser.add_argument(
        "--print-every",
        type=int,
        default=10,
        help="Progress print frequency in StressTest, in number of batches (default: 10)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    if args.workers < 1:
        raise ValueError("--workers must be >= 1")
    if args.print_every < 1:
        raise ValueError("--print-every must be >= 1")

    if args.mode == "StressTest":
        run_stress_test(
            digits=args.digits,
            show_pi=args.show_pi,
            workers=args.workers,
            print_every=args.print_every,
        )
        return

    result = run_single_benchmark(
        digits=args.digits,
        show_pi=args.show_pi,
        workers=args.workers,
    )
    print(
        f"Benchmark completed: {result.digits} digits in "
        f"{result.elapsed_seconds:.6f} seconds (workers: {args.workers})"
    )


if __name__ == "__main__":
    main()
