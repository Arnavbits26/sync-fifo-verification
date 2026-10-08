#!/usr/bin/env python3
"""Regression + fault-injection runner for the sync_fifo verification environment.

For the golden design and each injected bug (BUG_1..BUG_10) it:
  1. builds a Verilator model with the C++ testbench,
  2. runs the testbench over a range of random seeds,
  3. records pass/fail, the first failing cycle, and functional coverage.

A bug counts as "detected" if at least one seed makes the testbench FAIL.
The golden design must pass every seed. Results go to results/ as JSON and Markdown.

Usage:  python3 scripts/run_regression.py [--seeds 20] [--cycles 6000] [--no-directed]
"""
import argparse, json, re, subprocess, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUGS = {
    1: "full flag asserts one entry early",
    2: "write accepted while full (overflow)",
    3: "read accepted while empty (underflow)",
    4: "write dropped on a simultaneous read/write",
    5: "count updated wrongly on a simultaneous read/write",
    6: "read pointer wraps to the wrong slot",
    7: "rvalid asserted on a read attempt at empty",
    8: "data bit flipped when writing the last slot",
    9: "write accepted at full when a read is also requested",
    10: "rare corner case: write of 0xFF dropped during read/write at DEPTH-1 entries",
}

def build(name, define):
    mdir = ROOT / "build" / name
    mdir.mkdir(parents=True, exist_ok=True)
    cmd = ["verilator", "--cc", str(ROOT / "rtl/sync_fifo.v"), "--exe", str(ROOT / "tb/tb_fifo.cpp"),
           "--build", "-j", "4", "-Mdir", str(mdir), "-GADDR_W=3", "-CFLAGS", "-DFIFO_ADDR_W=3",
           "-Wall", "-o", f"tb_{name}"]
    if define:
        cmd.append(f"-D{define}")
    r = subprocess.run(cmd, capture_output=True, text=True)
    warns = [l for l in r.stderr.splitlines() if "%Warning" in l]
    if r.returncode != 0:
        print(r.stderr); sys.exit(f"build failed for {name}")
    return mdir / f"tb_{name}", warns

def run(binary, seed, cycles, directed):
    cmd = [str(binary), "--seed", str(seed), "--cycles", str(cycles)]
    if not directed:
        cmd.append("--no-directed")
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    out = r.stdout
    m = re.search(r"cycle=(\d+)", out) if r.returncode else None
    cov = {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", out.split("COVERAGE")[1].split("\n")[0])} if "COVERAGE" in out else {}
    pct = float(re.search(r"COV_PCT ([\d.]+)", out).group(1)) if "COV_PCT" in out else 0.0
    first_line = out.splitlines()[0] if out else ""
    return {"seed": seed, "passed": r.returncode == 0, "fail_cycle": int(m.group(1)) if m else None,
            "message": first_line, "coverage": cov, "cov_pct": pct}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=20)
    ap.add_argument("--cycles", type=int, default=6000)
    ap.add_argument("--no-directed", action="store_true")
    a = ap.parse_args()
    directed = not a.no_directed
    t0 = time.time()
    results = {}
    variants = [("golden", None)] + [(f"bug{n}", f"BUG_{n}") for n in BUGS]
    for name, define in variants:
        binary, warns = build(name, define)
        runs = [run(binary, s, a.cycles, directed) for s in range(1, a.seeds + 1)]
        results[name] = {"lint_warnings": len(warns), "runs": runs}
        n_fail = sum(not r["passed"] for r in runs)
        print(f"{name:7s}  failing seeds: {n_fail:2d}/{a.seeds}")
    golden = results["golden"]["runs"]
    merged = {}
    for r in golden:
        for k, v in r["coverage"].items():
            merged[k] = merged.get(k, 0) + v
    summary = {
        "seeds": a.seeds, "cycles_per_seed": a.cycles, "directed_phases": directed,
        "golden_all_pass": all(r["passed"] for r in golden),
        "golden_lint_warnings": results["golden"]["lint_warnings"],
        "golden_min_cov_pct": min(r["cov_pct"] for r in golden),
        "golden_bins_hit_any_seed": sum(v > 0 for v in merged.values()), "total_bins": len(merged),
        "bugs_detected": sum(any(not r["passed"] for r in results[f"bug{n}"]["runs"]) for n in BUGS),
        "bugs_total": len(BUGS),
    }
    (ROOT / "results").mkdir(exist_ok=True)
    suffix = "" if directed else "_no_directed"
    (ROOT / f"results/regression{suffix}.json").write_text(json.dumps({"summary": summary, "results": results}, indent=1))

    lines = [f"# Regression report{' (random stimulus only)' if not directed else ''}", "",
             f"- Seeds per design: {a.seeds}, cycles per seed: {a.cycles}, directed phases: {'yes' if directed else 'no'}",
             f"- Golden design: {'PASS on all seeds' if summary['golden_all_pass'] else 'FAILURES'}; "
             f"lint warnings: {summary['golden_lint_warnings']}",
             f"- Golden functional coverage: {summary['golden_bins_hit_any_seed']}/{summary['total_bins']} bins hit; "
             f"minimum per-seed coverage {summary['golden_min_cov_pct']}%",
             f"- Injected bugs detected: {summary['bugs_detected']}/{summary['bugs_total']}", "",
             "| Bug | Description | Seeds failing | First failing cycle (seed 1) | Detected |",
             "|---|---|---|---|---|"]
    for n, d in BUGS.items():
        runs = results[f"bug{n}"]["runs"]
        nf = sum(not r["passed"] for r in runs)
        fc = runs[0]["fail_cycle"]
        lines.append(f"| BUG_{n} | {d} | {nf}/{a.seeds} | {fc if fc is not None else '-'} | {'yes' if nf else 'NO'} |")
    lines += ["", "## Golden coverage (hits summed over all seeds)", "", "| Bin | Hits |", "|---|---|"]
    lines += [f"| {k} | {v} |" for k, v in merged.items()]
    lines += ["", f"Total wall time: {time.time() - t0:.0f}s"]
    (ROOT / f"results/regression_report{suffix}.md").write_text("\n".join(lines) + "\n")
    print("\n" + "\n".join(lines[2:8]))
    sys.exit(0 if summary["golden_all_pass"] and summary["bugs_detected"] == summary["bugs_total"] else 1)

if __name__ == "__main__":
    main()
