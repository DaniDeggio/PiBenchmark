CXX := g++
CXXFLAGS := -O2 -std=c++17 -pthread
PYTHON ?= python3

DIGITS ?= 7000
WORKERS ?= 2
REPEATS ?= 6
WARMUP ?= 2

CPP_SRC := benchmark_pi.cpp
CPP_BIN := benchmark_pi_cpp
PY_SRC := benchmark_pi.py
COMPARE_SRC := compare_benchmarks.py

.PHONY: all cpp python-check test compare clean run-cpp run-py

all: cpp

cpp: $(CPP_BIN)

$(CPP_BIN): $(CPP_SRC)
	$(CXX) $(CXXFLAGS) $(CPP_SRC) -o $(CPP_BIN)

python-check:
	$(PYTHON) -m py_compile $(PY_SRC)

test: cpp python-check

compare: cpp python-check
	$(PYTHON) $(COMPARE_SRC) $(DIGITS) --workers $(WORKERS) --repeats $(REPEATS) --warmup $(WARMUP) --python-exe $(PYTHON) --python-script $(PY_SRC) --cpp-bin ./$(CPP_BIN)

run-cpp: cpp
	./$(CPP_BIN) 5000 --mode Benchmark --workers 2

run-py:
	$(PYTHON) $(PY_SRC) 5000 --mode Benchmark --workers 2

clean:
	rm -f $(CPP_BIN)
