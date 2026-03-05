# PiBenchmark

Benchmark per misurare il tempo necessario a calcolare `N` cifre decimali di π.

Sono incluse due implementazioni:
- Python: `benchmark_pi.py`
- C++: `benchmark_pi.cpp`

Supporta sia esecuzione `SingleCore` sia `MultiCore` tramite processi multipli.

## Requisiti

- Python 3.10+ (testato su Python 3.13)
- Compilatore C++17 (es. `g++`)

## Makefile

Sono disponibili target rapidi:

- `make cpp`: compila il binario C++ (`benchmark_pi_cpp`)
- `make python-check`: verifica sintassi Python (`py_compile`)
- `make test`: esegue `cpp` + `python-check`
- `make compare`: confronta lo stesso benchmark su Python e C++
- `make run-cpp`: esegue un benchmark C++ di esempio
- `make run-py`: esegue un benchmark Python di esempio
- `make clean`: rimuove il binario C++

Esempio confronto:

```bash
make compare DIGITS=10000 WORKERS=4 REPEATS=7 WARMUP=1
```

Il confronto usa `compare_benchmarks.py` e stampa media/min/max/deviazione standard per entrambe le implementazioni, più speedup medio C++ vs Python.

## Versione C++

Compilazione:

```bash
g++ -O2 -std=c++17 -pthread benchmark_pi.cpp -o benchmark_pi_cpp
```

Esempio Benchmark MultiCore:

```bash
./benchmark_pi_cpp 10000 --mode Benchmark --workers 8
```

Esempio StressTest MultiCore:

```bash
./benchmark_pi_cpp 5000 --mode StressTest --workers 8 --print-every 50
```

Le opzioni CLI sono allineate con la versione Python (`--mode`, `--show-pi`, `--workers`, `--print-every`).

## Versione Python

Dalla cartella del progetto:

```bash
python3 benchmark_pi.py 10000 --mode Benchmark
```

Esempio MultiCore (usa 8 processi):

```bash
python3 benchmark_pi.py 10000 --mode Benchmark --workers 8
```

### Modalità disponibili

- `Benchmark`: esegue una sola misurazione.
- `StressTest`: ripete il benchmark in loop finché non viene interrotto (`Ctrl+C`), con batch paralleli se `--workers > 1`.

Alla chiusura dello `StressTest` viene mostrato un riepilogo con numero batch, totale calcoli, tempo medio/min/max batch, durata totale e throughput (`calcoli/s`).

Durante l'esecuzione dello `StressTest` viene mostrato anche il throughput live (`calcoli/s`) ogni 10 batch (oltre al primo).

Esempio StressTest:

```bash
python3 benchmark_pi.py 5000 --mode StressTest
```

Esempio StressTest con stampa ogni 50 batch:

```bash
python3 benchmark_pi.py 5000 --mode StressTest --workers 8 --print-every 50
```

### Opzioni

- `digits` (obbligatorio): numero di cifre decimali da calcolare.
- `--mode`: `Benchmark` oppure `StressTest` (default: `Benchmark`).
- `--show-pi`: mostra il valore di π calcolato.
- `--workers`: numero di processi da usare (default: numero di core logici).
- `--print-every`: ogni quanti batch stampare il progresso nello `StressTest` (default: `10`).
