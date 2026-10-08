# Regression report (random stimulus only)

- Seeds per design: 20, cycles per seed: 6000, directed phases: no
- Golden design: PASS on all seeds; lint warnings: 0
- Golden functional coverage: 13/13 bins hit; minimum per-seed coverage 100.0%
- Injected bugs detected: 10/10

| Bug | Description | Seeds failing | First failing cycle (seed 1) | Detected |
|---|---|---|---|---|
| BUG_1 | full flag asserts one entry early | 20/20 | 11 | yes |
| BUG_2 | write accepted while full (overflow) | 20/20 | 19 | yes |
| BUG_3 | read accepted while empty (underflow) | 20/20 | 0 | yes |
| BUG_4 | write dropped on a simultaneous read/write | 20/20 | 4 | yes |
| BUG_5 | count updated wrongly on a simultaneous read/write | 20/20 | 1 | yes |
| BUG_6 | read pointer wraps to the wrong slot | 20/20 | 36 | yes |
| BUG_7 | rvalid asserted on a read attempt at empty | 20/20 | 0 | yes |
| BUG_8 | data bit flipped when writing the last slot | 20/20 | 32 | yes |
| BUG_9 | write accepted at full when a read is also requested | 20/20 | 32 | yes |
| BUG_10 | rare corner case: write of 0xFF dropped during read/write at DEPTH-1 entries | 7/20 | - | yes |

## Golden coverage (hits summed over all seeds)

| Bin | Hits |
|---|---|
| empty_state | 22549 |
| full_state | 21888 |
| write_at_full | 15440 |
| read_at_empty | 15096 |
| simul_rw_mid | 24738 |
| simul_rw_at_full | 3511 |
| simul_rw_at_empty | 3556 |
| write_ptr_wrap | 5427 |
| read_ptr_wrap | 5330 |
| back_to_back_writes | 3661 |
| back_to_back_reads | 3622 |
| fill_then_drain | 178 |
| mid_reset_nonempty | 138 |

Total wall time: 1s
