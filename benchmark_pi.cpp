#include <atomic>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
std::atomic<bool> g_stop_requested{false};

class BigInt {
public:
    static constexpr std::uint32_t BASE = 1000000000U;

    BigInt() : digits_{0} {}
    BigInt(std::uint64_t value) { *this = value; }

    BigInt& operator=(std::uint64_t value) {
        digits_.clear();
        if (value == 0) {
            digits_.push_back(0);
            return *this;
        }
        while (value > 0) {
            digits_.push_back(static_cast<std::uint32_t>(value % BASE));
            value /= BASE;
        }
        return *this;
    }

    bool is_zero() const {
        return digits_.size() == 1 && digits_[0] == 0;
    }

    int compare(const BigInt& other) const {
        if (digits_.size() != other.digits_.size()) {
            return digits_.size() < other.digits_.size() ? -1 : 1;
        }
        for (std::size_t i = digits_.size(); i-- > 0;) {
            if (digits_[i] != other.digits_[i]) {
                return digits_[i] < other.digits_[i] ? -1 : 1;
            }
        }
        return 0;
    }

    void add(const BigInt& other) {
        const std::size_t max_size = std::max(digits_.size(), other.digits_.size());
        digits_.resize(max_size, 0);

        std::uint64_t carry = 0;
        for (std::size_t i = 0; i < max_size; ++i) {
            std::uint64_t sum = static_cast<std::uint64_t>(digits_[i])
                              + (i < other.digits_.size() ? other.digits_[i] : 0)
                              + carry;
            digits_[i] = static_cast<std::uint32_t>(sum % BASE);
            carry = sum / BASE;
        }
        if (carry > 0) {
            digits_.push_back(static_cast<std::uint32_t>(carry));
        }
    }

    void sub(const BigInt& other) {
        if (compare(other) < 0) {
            throw std::runtime_error("Negative BigInt subtraction is not supported");
        }

        std::int64_t borrow = 0;
        for (std::size_t i = 0; i < digits_.size(); ++i) {
            std::int64_t cur = static_cast<std::int64_t>(digits_[i])
                             - (i < other.digits_.size() ? other.digits_[i] : 0)
                             - borrow;

            if (cur < 0) {
                cur += BASE;
                borrow = 1;
            } else {
                borrow = 0;
            }

            digits_[i] = static_cast<std::uint32_t>(cur);
        }

        normalize();
    }

    void mul_uint(std::uint32_t factor) {
        if (factor == 0 || is_zero()) {
            *this = 0;
            return;
        }
        if (factor == 1) {
            return;
        }

        std::uint64_t carry = 0;
        for (std::size_t i = 0; i < digits_.size(); ++i) {
            std::uint64_t prod = static_cast<std::uint64_t>(digits_[i]) * factor + carry;
            digits_[i] = static_cast<std::uint32_t>(prod % BASE);
            carry = prod / BASE;
        }
        while (carry > 0) {
            digits_.push_back(static_cast<std::uint32_t>(carry % BASE));
            carry /= BASE;
        }
        normalize();
    }

    std::uint32_t div_uint(std::uint32_t divisor) {
        if (divisor == 0) {
            throw std::invalid_argument("Division by zero");
        }

        std::uint64_t remainder = 0;
        for (std::size_t i = digits_.size(); i-- > 0;) {
            std::uint64_t cur = digits_[i] + remainder * BASE;
            digits_[i] = static_cast<std::uint32_t>(cur / divisor);
            remainder = cur % divisor;
        }
        normalize();
        return static_cast<std::uint32_t>(remainder);
    }

    std::string to_string() const {
        if (is_zero()) {
            return "0";
        }
        std::string out = std::to_string(digits_.back());
        for (std::size_t i = digits_.size() - 1; i-- > 0;) {
            std::string part = std::to_string(digits_[i]);
            out += std::string(9 - part.size(), '0');
            out += part;
        }
        return out;
    }

private:
    std::vector<std::uint32_t> digits_;

    void normalize() {
        while (digits_.size() > 1 && digits_.back() == 0) {
            digits_.pop_back();
        }
    }
};

struct Options {
    int digits = -1;
    std::string mode = "Benchmark";
    bool show_pi = false;
    int workers = static_cast<int>(std::thread::hardware_concurrency());
    int print_every = 10;
};

struct BenchmarkResult {
    int digits;
    double elapsed_seconds;
    std::string pi_value;
};

struct BatchResult {
    double wall_seconds;
    double avg_task_seconds;
};

void on_sigint(int) {
    g_stop_requested.store(true);
}

BigInt pow10_u(unsigned int exponent) {
    BigInt result = 1;
    for (unsigned int i = 0; i < exponent; ++i) {
        result.mul_uint(10);
    }
    return result;
}

BigInt arctan_inv(unsigned int inv_x, const BigInt& scale) {
    const std::uint32_t x2 = inv_x * inv_x;

    BigInt term = scale;
    term.div_uint(inv_x);
    BigInt sum = term;

    for (std::uint64_t n = 1; !term.is_zero(); ++n) {
        term.div_uint(x2);
        if (term.is_zero()) {
            break;
        }

        const std::uint64_t odd = 2 * n + 1;
        if (odd > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Overflow in arctan series divisor");
        }

        BigInt frac = term;
        frac.div_uint(static_cast<std::uint32_t>(odd));
        if (frac.is_zero()) {
            break;
        }

        if (n % 2 == 0) {
            sum.add(frac);
        } else {
            sum.sub(frac);
        }
    }

    return sum;
}

std::string format_scaled_pi(const BigInt& scaled_pi, int digits) {
    std::string raw = scaled_pi.to_string();

    if (static_cast<int>(raw.size()) < digits + 1) {
        raw = std::string(digits + 1 - raw.size(), '0') + raw;
    }

    std::string whole = raw.substr(0, raw.size() - digits);
    std::string fractional = raw.substr(raw.size() - digits);
    return whole + "." + fractional;
}

std::string calculate_pi_digits(int digits) {
    if (digits < 1) {
        throw std::invalid_argument("The number of digits must be >= 1");
    }

    const int guard_digits = 10;
    const BigInt scale = pow10_u(static_cast<unsigned int>(digits + guard_digits));

    BigInt a = arctan_inv(5, scale);
    a.mul_uint(16);
    BigInt b = arctan_inv(239, scale);
    b.mul_uint(4);

    if (a.compare(b) < 0) {
        throw std::runtime_error("Numeric error: unexpected negative result");
    }

    a.sub(b);
    for (int i = 0; i < guard_digits; ++i) {
        a.div_uint(10);
    }

    return format_scaled_pi(a, digits);
}

double compute_single_elapsed(int digits) {
    auto start = std::chrono::steady_clock::now();
    (void)calculate_pi_digits(digits);
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(end - start).count();
}

BatchResult run_parallel_batch(int digits, int workers) {
    auto wall_start = std::chrono::steady_clock::now();

    std::vector<std::future<double>> futures;
    futures.reserve(static_cast<std::size_t>(workers));

    for (int i = 0; i < workers; ++i) {
        futures.emplace_back(std::async(std::launch::async, [digits]() {
            return compute_single_elapsed(digits);
        }));
    }

    double sum_task_time = 0.0;
    for (auto& future : futures) {
        sum_task_time += future.get();
    }

    auto wall_end = std::chrono::steady_clock::now();
    double wall = std::chrono::duration<double>(wall_end - wall_start).count();

    BatchResult result;
    result.wall_seconds = wall;
    result.avg_task_seconds = sum_task_time / static_cast<double>(workers);
    return result;
}

BenchmarkResult run_single_benchmark(int digits, bool show_pi, int workers) {
    if (workers < 1) {
        throw std::invalid_argument("workers must be >= 1");
    }

    if (workers == 1) {
        auto start = std::chrono::steady_clock::now();
        std::string pi_value = calculate_pi_digits(digits);
        auto end = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();

        if (show_pi) {
            std::cout << "π (" << digits << " digits):\n" << pi_value << "\n";
        }

        BenchmarkResult result;
        result.digits = digits;
        result.elapsed_seconds = elapsed;
        result.pi_value = std::move(pi_value);
        return result;
    }

    if (show_pi) {
        std::cout << "π (" << digits << " digits):\n" << calculate_pi_digits(digits) << "\n";
    }

    BatchResult batch = run_parallel_batch(digits, workers);
    std::cout << "MultiCore Benchmark (" << workers << " workers): "
              << "batch in " << std::fixed << std::setprecision(6) << batch.wall_seconds << " s"
              << " | average task time: " << batch.avg_task_seconds << " s\n";

    BenchmarkResult result;
    result.digits = digits;
    result.elapsed_seconds = batch.wall_seconds;
    result.pi_value = "";
    return result;
}

void run_stress_test(int digits, bool show_pi, int workers, int print_every) {
    if (workers < 1) {
        throw std::invalid_argument("workers must be >= 1");
    }
    if (print_every < 1) {
        throw std::invalid_argument("print_every must be >= 1");
    }

    std::cout << "StressTest mode started (workers: " << workers
              << "). Press Ctrl+C to stop.\n";

    if (show_pi) {
        std::cout << "π (" << digits << " digits):\n" << calculate_pi_digits(digits) << "\n";
    }

    auto stress_start = std::chrono::steady_clock::now();
    int batch_iteration = 0;
    double total_batch_time = 0.0;
    double min_batch_time = std::numeric_limits<double>::infinity();
    double max_batch_time = 0.0;

    while (!g_stop_requested.load()) {
        ++batch_iteration;
        BatchResult batch = run_parallel_batch(digits, workers);

        total_batch_time += batch.wall_seconds;
        min_batch_time = std::min(min_batch_time, batch.wall_seconds);
        max_batch_time = std::max(max_batch_time, batch.wall_seconds);

        double avg_batch = total_batch_time / static_cast<double>(batch_iteration);
        double elapsed_total = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - stress_start)
                                   .count();
        long long total_calculations = static_cast<long long>(batch_iteration) * workers;
        double live_throughput = elapsed_total > 0.0 ? total_calculations / elapsed_total : 0.0;

        if (batch_iteration == 1 || batch_iteration % print_every == 0) {
            std::cout << "Batch " << batch_iteration << ": "
                      << std::fixed << std::setprecision(6) << batch.wall_seconds << " s "
                      << "(batch avg: " << avg_batch << " s, "
                      << "task avg: " << batch.avg_task_seconds << " s, "
                      << "throughput: " << std::setprecision(2) << live_throughput
                      << " calculations/s)\n";
        }
    }

    std::cout << "\nStressTest interrupted by user.\n";
    if (batch_iteration > 0) {
        double total_elapsed = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - stress_start)
                                   .count();
        long long total_calculations = static_cast<long long>(batch_iteration) * workers;
        double throughput = total_elapsed > 0.0 ? total_calculations / total_elapsed : 0.0;

        std::cout << "Total batches: " << batch_iteration
                  << " | Total calculations: " << total_calculations
                  << " | Average batch time: " << std::fixed << std::setprecision(6)
                  << (total_batch_time / static_cast<double>(batch_iteration)) << " s"
                  << " | Min batch: " << min_batch_time << " s"
                  << " | Max batch: " << max_batch_time << " s\n";

        std::cout << "Total duration: " << std::fixed << std::setprecision(3)
                  << total_elapsed << " s"
                  << " | Throughput: " << std::setprecision(2) << throughput
                  << " calculations/s\n";
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage:\n"
              << "  " << program_name
              << " <digits> [--mode Benchmark|StressTest] [--show-pi]"
              << " [--workers N] [--print-every N]\n";
}

Options parse_args(int argc, char** argv) {
    Options opts;

    if (opts.workers <= 0) {
        opts.workers = 1;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }

        if (arg == "--show-pi") {
            opts.show_pi = true;
            continue;
        }

        if (arg == "--mode") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --mode");
            }
            opts.mode = argv[++i];
            continue;
        }

        if (arg == "--workers") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --workers");
            }
            opts.workers = std::stoi(argv[++i]);
            continue;
        }

        if (arg == "--print-every") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --print-every");
            }
            opts.print_every = std::stoi(argv[++i]);
            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            throw std::invalid_argument("Unknown argument: " + arg);
        }

        if (opts.digits != -1) {
            throw std::invalid_argument("Specify only one value for <digits>");
        }
        opts.digits = std::stoi(arg);
    }

    if (opts.digits < 1) {
        throw std::invalid_argument("digits must be >= 1");
    }
    if (opts.mode != "Benchmark" && opts.mode != "StressTest") {
        throw std::invalid_argument("--mode must be Benchmark or StressTest");
    }
    if (opts.workers < 1) {
        throw std::invalid_argument("--workers must be >= 1");
    }
    if (opts.print_every < 1) {
        throw std::invalid_argument("--print-every must be >= 1");
    }

    return opts;
}
}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, on_sigint);

    try {
        Options args = parse_args(argc, argv);

        if (args.mode == "StressTest") {
            run_stress_test(args.digits, args.show_pi, args.workers, args.print_every);
            return 0;
        }

        BenchmarkResult result = run_single_benchmark(args.digits, args.show_pi, args.workers);
        std::cout << "Benchmark completed: " << result.digits << " digits in "
                  << std::fixed << std::setprecision(6) << result.elapsed_seconds
                  << " seconds (workers: " << args.workers << ")\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        print_usage(argv[0]);
        return 1;
    }
}
