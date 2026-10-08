# Regression report

- Seeds per design: 20, cycles per seed: 6000, directed phases: yes
- Golden design: PASS on all seeds; lint warnings: 0
- Golden functional coverage: 13/13 bins hit; minimum per-seed coverage 100.0%
- Injected bugs detected: 10/10

| Bug | Description | Seeds failing | First failing cycle (seed 1) | Detected |
|---|---|---|---|---|
| BUG_1 | full flag asserts one entry early | 20/20 | 6 | yes |
| BUG_2 | write accepted while full (overflow) | 20/20 | 8 | yes |
| BUG_3 | read accepted while empty (underflow) | 20/20 | 21 | yes |
| BUG_4 | write dropped on a simultaneous read/write | 20/20 | 19 | yes |
| BUG_5 | count updated wrongly on a simultaneous read/write | 20/20 | 12 | yes |
| BUG_6 | read pointer wraps to the wrong slot | 20/20 | 19 | yes |
| BUG_7 | rvalid asserted on a read attempt at empty | 20/20 | 21 | yes |
| BUG_8 | data bit flipped when writing the last slot | 20/20 | 18 | yes |
| BUG_9 | write accepted at full when a read is also requested | 20/20 | 11 | yes |
| BUG_10 | rare corner case: write of 0xFF dropped during read/write at DEPTH-1 entries | 12/20 | - | yes |

## Golden coverage (hits summed over all seeds)

| Bin | Hits |
|---|---|
| empty_state | 19674 |
| full_state | 23275 |
| write_at_full | 16771 |
| read_at_empty | 13051 |
| simul_rw_mid | 27500 |
| simul_rw_at_full | 3770 |
| simul_rw_at_empty | 3107 |
| write_ptr_wrap | 5695 |
| read_ptr_wrap | 5601 |
| back_to_back_writes | 3846 |
| back_to_back_reads | 3752 |
| fill_then_drain | 170 |
| mid_reset_nonempty | 134 |

Total wall time: 64s
