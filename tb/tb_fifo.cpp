// C++ verification environment for sync_fifo (Verilator).
//
//  * Reference model  : std::deque mirroring the documented FIFO semantics
//  * Stimulus         : directed phases (fill, overflow, drain, underflow, simultaneous
//                       read/write at the extremes) followed by constrained-random
//                       phases whose read/write probabilities change every 250 cycles,
//                       with occasional mid-operation resets
//  * Scoreboard       : every cycle checks rvalid, rdata, full, empty and count
//  * Functional cover : named bins (see COVERAGE_BINS) reported at the end of the run
//
// Usage: ./Vsync_fifo [--seed N] [--cycles N] [--no-directed]
// Exit code: 0 = PASS, 1 = FAIL (first mismatch is printed with its cycle number)

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "Vsync_fifo.h"
#include "verilated.h"

#ifndef FIFO_ADDR_W
#define FIFO_ADDR_W 3
#endif
static const int DEPTH = 1 << FIFO_ADDR_W;

static const std::vector<std::string> COVERAGE_BINS = {
    "empty_state",          "full_state",
    "write_at_full",        "read_at_empty",
    "simul_rw_mid",         "simul_rw_at_full",   "simul_rw_at_empty",
    "write_ptr_wrap",       "read_ptr_wrap",
    "back_to_back_writes",  "back_to_back_reads",
    "fill_then_drain",      "mid_reset_nonempty",
};

static std::map<std::string, uint64_t> cov;
static void hit(const char *name) { cov[name]++; }

struct Fail {
    bool failed = false;
    char msg[160] = {0};
};

int main(int argc, char **argv) {
    uint32_t seed = 1;
    int cycles = 6000;
    bool directed = true;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint32_t)strtoul(argv[++i], nullptr, 10);
        else if (!strcmp(argv[i], "--cycles") && i + 1 < argc) cycles = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--no-directed")) directed = false;
    }
    for (const auto &b : COVERAGE_BINS) cov[b] = 0;

    VerilatedContext ctx;
    Vsync_fifo *dut = new Vsync_fifo{&ctx};
    std::mt19937 rng(seed);
    std::deque<uint8_t> q;           // reference model
    int mw = 0, mr = 0;              // model write/read pointer (mod DEPTH)
    int run_w = 0, run_r = 0;        // consecutive accepted writes/reads
    bool seen_full = false;
    Fail f;

    auto tick = [&]() { dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval(); };

    auto fail = [&](int cycle, const char *fmt, int a, int b) {
        f.failed = true;
        char body[120];
        snprintf(body, sizeof body, fmt, a, b);
        snprintf(f.msg, sizeof f.msg, "FAIL seed=%u cycle=%d: %s", seed, cycle, body);
    };

    // ---- reset ----
    dut->rst_n = 0; dut->wr_en = 0; dut->rd_en = 0; dut->wdata = 0;
    tick(); tick();
    dut->rst_n = 1; dut->eval();
    if (!dut->empty || dut->full || dut->count != 0 || dut->rvalid) {
        printf("FAIL seed=%u cycle=-1: bad state after reset\n", seed);
        return 1;
    }

    // ---- directed schedule: {wr_en, rd_en, cycles} ----
    struct Step { int wr, rd, n; };
    std::vector<Step> sched;
    if (directed) {
        sched = {{1, 0, DEPTH + 3},   // fill, then attempt writes at full
                 {1, 1, 3},           // simultaneous read/write at full
                 {0, 1, DEPTH + 3},   // drain, then attempt reads at empty
                 {1, 1, 3}};          // simultaneous read/write at empty
    }
    std::vector<std::pair<double, double>> mixes = {
        {0.8, 0.2}, {0.2, 0.8}, {0.5, 0.5}, {0.9, 0.9}, {0.1, 0.1}};
    double pw = 0.5, pr = 0.5;
    std::uniform_real_distribution<double> uni(0.0, 1.0);

    size_t si = 0; int sleft = 0;
    for (int cycle = 0; cycle < cycles && !f.failed; cycle++) {
        int wr, rd;
        bool in_directed = false;
        if (si < sched.size()) {
            if (sleft == 0) sleft = sched[si].n;
            wr = sched[si].wr; rd = sched[si].rd;
            in_directed = true;
            if (--sleft == 0) si++;
        } else {
            if ((cycle % 250) == 0) { auto m = mixes[rng() % mixes.size()]; pw = m.first; pr = m.second; }
            wr = uni(rng) < pw; rd = uni(rng) < pr;
            // occasional mid-operation reset (random phase only)
            if ((rng() % 700) == 0) {
                if (!q.empty()) hit("mid_reset_nonempty");
                dut->rst_n = 0; dut->eval(); tick(); dut->rst_n = 1; dut->eval();
                q.clear(); mw = mr = 0; run_w = run_r = 0; seen_full = false;
                if (!dut->empty || dut->full || dut->count != 0 || dut->rvalid) { fail(cycle, "bad state after mid-run reset%d%d", 0, 0); break; }
            }
        }
        (void)in_directed;
        uint8_t wd = (uint8_t)(rng() & 0xFF);

        // ---- reference model: pre-edge decisions ----
        bool m_full = (int)q.size() == DEPTH;
        bool m_empty = q.empty();
        bool do_wr = wr && !m_full;
        bool do_rd = rd && !m_empty;
        uint8_t expected = do_rd ? q.front() : 0;

        // ---- functional coverage (pre-edge events) ----
        if (wr && m_full) hit("write_at_full");
        if (rd && m_empty) hit("read_at_empty");
        if (wr && rd) { if (m_empty) hit("simul_rw_at_empty"); else if (m_full) hit("simul_rw_at_full"); else hit("simul_rw_mid"); }
        if (do_wr && mw == DEPTH - 1) hit("write_ptr_wrap");
        if (do_rd && mr == DEPTH - 1) hit("read_ptr_wrap");
        run_w = do_wr ? run_w + 1 : 0; if (run_w == 3) hit("back_to_back_writes");
        run_r = do_rd ? run_r + 1 : 0; if (run_r == 3) hit("back_to_back_reads");

        // ---- drive DUT, clock it ----
        dut->wr_en = wr; dut->rd_en = rd; dut->wdata = wd;
        tick();

        // ---- update model ----
        if (do_wr) { q.push_back(wd); mw = (mw + 1) % DEPTH; }
        if (do_rd) { q.pop_front(); mr = (mr + 1) % DEPTH; }

        // ---- scoreboard ----
        if ((bool)dut->rvalid != do_rd) { fail(cycle, "rvalid=%d expected %d", dut->rvalid, do_rd); break; }
        if (do_rd && dut->rdata != expected) { fail(cycle, "rdata=0x%02x expected 0x%02x", dut->rdata, expected); break; }
        if ((bool)dut->full != ((int)q.size() == DEPTH)) { fail(cycle, "full=%d expected %d", dut->full, (int)q.size() == DEPTH); break; }
        if ((bool)dut->empty != q.empty()) { fail(cycle, "empty=%d expected %d", dut->empty, (int)q.empty()); break; }
        if ((int)dut->count != (int)q.size()) { fail(cycle, "count=%d expected %d", dut->count, (int)q.size()); break; }

        // ---- state coverage (post-edge) ----
        if (q.empty()) { hit("empty_state"); if (seen_full) { hit("fill_then_drain"); seen_full = false; } }
        if ((int)q.size() == DEPTH) { hit("full_state"); seen_full = true; }
    }

    if (f.failed) {
        printf("%s\n", f.msg);
    } else {
        printf("PASS seed=%u cycles=%d\n", seed, cycles);
    }
    int hit_bins = 0;
    printf("COVERAGE");
    for (const auto &b : COVERAGE_BINS) { printf(" %s=%llu", b.c_str(), (unsigned long long)cov[b]); if (cov[b]) hit_bins++; }
    printf("\nCOV_PCT %.1f\n", 100.0 * hit_bins / COVERAGE_BINS.size());
    delete dut;
    return f.failed ? 1 : 0;
}
