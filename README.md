# FIFO Verification Environment (C++ / Verilator) with Fault Injection

A small but complete verification environment for a parameterised synchronous FIFO written in Verilog.
The design is checked by a **C++ testbench** (driven by Verilator) that contains a reference model,
a cycle-by-cycle scoreboard, constrained-random stimulus and functional coverage. A Python
**regression runner** then measures how good the testbench is by injecting ten deliberate RTL bugs
and checking that each one is caught.

## What it demonstrates

- C++ testbench development: reference model, scoreboard, randomised stimulus, coverage bins
- Verilog RTL and the cycle-level semantics of a FIFO (full/empty/count flags, simultaneous read and write, pointer wrap)
- Fault injection (mutation analysis) as a way of evaluating a testbench, not just the design
- Regression automation in Python: multi-seed runs, pass/fail, first-failure cycle, coverage merge, report generation
- Debugging methodology: every failure prints the seed and cycle number so it can be reproduced exactly

## Layout

```
rtl/sync_fifo.v           FIFO RTL; -DBUG_1 ... -DBUG_10 inject one deliberate bug each
tb/tb_fifo.cpp            C++ testbench: model + scoreboard + stimulus + functional coverage
scripts/run_regression.py builds golden + 10 mutants, runs N seeds each, writes reports
results/                  regression reports (Markdown) and raw data (JSON)
```

## Quick start

Requires Verilator (5.x), a C++ compiler and Python 3.

```bash
sudo apt install verilator g++ python3          # Ubuntu / Debian
python3 scripts/run_regression.py               # 20 seeds x 6000 cycles for each of 11 designs
python3 scripts/run_regression.py --no-directed # random stimulus only
```

Run one design by hand (replay a failure from its seed):

```bash
verilator --cc rtl/sync_fifo.v --exe tb/tb_fifo.cpp --build -Mdir build/g -GADDR_W=3 -CFLAGS -DFIFO_ADDR_W=3 -o tb_g
./build/g/tb_g --seed 7 --cycles 6000
```

## How it works

**DUT semantics** (documented at the top of `rtl/sync_fifo.v`): a write is accepted when `wr_en && !full`,
a read when `rd_en && !empty`, both decided before the clock edge. A simultaneous read and write is allowed
when the FIFO is neither full nor empty; at full only the read is accepted, at empty only the write.
`rdata`/`rvalid` are registered.

**Reference model:** a `std::deque` that applies the same rules. After every clock edge the scoreboard
compares `rvalid`, `rdata`, `full`, `empty` and `count` against the model and stops at the first mismatch.

**Stimulus:** directed phases (fill and overflow, simultaneous access at full, drain and underflow,
simultaneous access at empty), then constrained-random phases whose write/read probabilities change every
250 cycles, plus occasional mid-operation resets.

**Functional coverage:** 13 hand-written bins: `empty_state`, `full_state`, `write_at_full`, `read_at_empty`,
`simul_rw_mid`, `simul_rw_at_full`, `simul_rw_at_empty`, `write_ptr_wrap`, `read_ptr_wrap`,
`back_to_back_writes`, `back_to_back_reads`, `fill_then_drain`, `mid_reset_nonempty`.

**Fault injection:** each `BUG_n` changes one thing in the RTL (early full flag, overflow, underflow, dropped
write on simultaneous access, wrong count, bad pointer wrap, spurious `rvalid`, corrupted last slot, write
accepted at full, and one rare corner case). A bug counts as *detected* if at least one seed makes the testbench fail.

## Results (20 seeds x 6000 cycles per design)

| Check | Result |
|---|---|
| Golden design | passes all 20 seeds, 0 Verilator lint warnings |
| Functional coverage (golden) | 13/13 bins hit, 100% on every seed |
| Injected bugs detected | 10/10 |
| Bugs 1-9 | detected on every seed; failing cycle between 6 and 21 (median 18) |
| Bug 10 (rare corner case) | detected on 12/20 seeds; first failing cycle between 545 and 5153 |

Full tables: `results/regression_report.md` (directed + random) and `results/regression_report_no_directed.md`
(random only: bugs 1-9 still detected on every seed, bug 10 on 7/20).

**Reading the results honestly**

- Bugs 1-9 are easy for a FIFO this small, so catching them says the checking works, not that the bugs were subtle.
- Bug 10 shows the more useful point: one seed would miss it about 40% of the time, so a single run is not
  enough and regressions should use many seeds.
- With only 20 seeds, the difference between 12/20 (directed) and 7/20 (random only) is within noise,
  so no conclusion is drawn about the directed phases.

## Limitations

- The bugs are injected by the author, so this measures detection of known faults. It is not bug-finding in third-party RTL.
- Coverage is hand-written C++ counters, not simulator code or toggle coverage and not SystemVerilog covergroups.
- The DUT is a single small FIFO (8 entries, 8-bit data). No SystemVerilog assertions, UVM or SystemC are used.
