# Codec Compression Speed Notes

Why some codecs in zELF may appear very (very very) slow during compression ?


## LZHAM

### Why slow compression is expected
LZHAM is inherently a **ratio-oriented** codec. It combines LZ-style matching with additional modeling/entropy coding and can enable very expensive parsing modes. As a result, it is expected to be significantly slower than “speed-first” codecs such as LZ4/Snappy, and also slower than many small 8/16-bit era compressors.

In other words: slow compression is not a bug : it is the cost of pursuing maximum ratio.

### Why it is especially slow in zELF
In the current zELF configuration, LZHAM is effectively forced into a “maximum ratio / worst speed” setup.

In `src/packer/compression.c` the packer calls:

- `lzhamc_compress_memory(29, ...)`
- with the comment `// Use dict_size_log2=29 (512MB)`

A `dict_size_log2` of **29** means a **512MB dictionary** (the maximum allowed on x86_64). This can drastically increase CPU work and memory footprint (larger internal structures, more expensive search/parsing behavior, more cache misses).

In addition, the LZHAM wrapper defaults in `src/compressors/lzham/lzham_compressor.c` hardcode aggressive settings:

- `p->m_level = LZHAM_COMP_LEVEL_UBER;`
- compression flags force:
  - `LZHAM_COMP_FLAG_EXTREME_PARSING`
  - `LZHAM_COMP_FLAG_TRADEOFF_DECOMPRESSION_RATE_FOR_COMP_RATIO`
- `p->m_extreme_parsing_max_best_arrivals = 8` (maximum)
- `p->m_fast_bytes = 258` (maximum)

These settings intentionally push the compressor toward the highest ratio, and therefore can make compression extremely slow.

### Summary
- LZHAM being slow versus LZ4 is expected.
- In zELF it is expected to be **very** slow, because the integration explicitly selects:
  - **UBER** level
  - **Extreme parsing**
  - **512MB dictionary**
  - other internal parameters set to maximum-like values


## BriefLZ (blz)

### Why slow compression is expected
BriefLZ supports multiple compression levels, and level 10 is explicitly documented as optimal but very slow.

In zELF, the wrapper hardcodes the highest/slowest mode.

In `src/compressors/brieflz/blz_compress_wrapper.c`:

- `const int level = 10; /* optimal/slow */`

And the upstream header `src/compressors/brieflz/brieflz.h` states:

- “Level 10 is optimal but very slow.”

### What level 10 means internally
In `src/compressors/brieflz/brieflz.c`, level 10 maps to:

- `blz_pack_btparse(..., ULONG_MAX, ULONG_MAX)`

This implies:

- a dynamic-programming (optimal parse) approach using a binary-tree based match structure,
- effectively “unbounded” match exploration parameters (`max_depth` and `accept_len` are set to `ULONG_MAX`),
- highly data-dependent runtime (some data leads to far more candidate matches being explored).

This is consistent with a maximum-ratio preset and can become very slow on large inputs.

### Why it can be worse than expected (without being a bug)
Even when the integration is correct and compiled with optimizations, level 10 can become extremely slow for realistic reasons:

#### 1) Very large work memory at level 10 (O(n))
The btparse workmem size is computed in `src/compressors/brieflz/brieflz_btparse.h`:

- `blz_btparse_workmem_size(src_size)` returns:
  - `(5 * src_size + 3 + LOOKUP_SIZE) * sizeof(blz_word)`

With:

- `LOOKUP_SIZE = 1 << BLZ_HASH_BITS`
- `BLZ_HASH_BITS` defaults to **17** in `src/compressors/brieflz/brieflz.c`
- `blz_word` is `uint32_t`

So memory usage includes:

- a term proportional to input size (roughly `~ 20 * src_size` bytes), plus
- a hash lookup table of about `131072 * 4 = 512KB`

For large inputs, the workmem can become huge (e.g., tens of MB input may lead to hundreds of MB to >1GB of temporary memory). This alone can explain “super slow” behavior due to cache misses and memory pressure/paging.

#### 2) Lookup table initialization per compression
The btparse path initializes `lookup[]` to `NO_MATCH_POS` each run:

- roughly `LOOKUP_SIZE` stores (~131k at default settings)

This is not the dominant cost versus optimal parsing, but it adds overhead.

#### 3) Data-dependent explosion of candidate matches
The optimal parse explores much more when:

- the input contains many ambiguous repetitions,
- patterns create many match candidates with similar costs.

### Integration/optimization notes
In the zELF integration of BriefLZ:

- `#pragma GCC optimize("O3")` is present in both:
  - `src/compressors/brieflz/blz_compress_wrapper.c`
  - `src/compressors/brieflz/brieflz.c`

So even if the global build uses `-O2`, these translation units are forced to `O3`.

The wrapper itself is minimal (one output allocation, one workmem allocation, one call to `blz_pack_level`, then a `realloc` shrink), so the observed slowness is dominated by the codec’s level-10 parsing behavior.


## ZX0

### Why slow compression is expected
ZX0 is a ratio-oriented “retro/demoscene” compressor. Its core is an optimal parsing strategy (dynamic programming) that explores a large state space in order to minimize the final number of output bits.

As a consequence:

- compression can be extremely slow (by design)
- decompression remains simple and fast

### Why it is especially slow in zELF
In the current zELF integration, ZX0 is called in a configuration that is intrinsically expensive.

In `src/compressors/zx0/zx0_compress_wrapper.c`, the wrapper calls:

- `zx0_optimize(data, (int)in_sz, 0, MAX_OFFSET_ZX0)`

Where:

- `skip = 0`
- `MAX_OFFSET_ZX0 = 32640`

In `src/compressors/zx0/optimize.c`, `zx0_optimize()` contains a massive double loop:

- `index` iterates over all input bytes
- for each `index`, `offset` iterates from `1` to `max_offset` (up to 32640)
- for each `(index, offset)`, it updates the DP state and bit-cost estimates (Elias gamma cost, best chain selection)

This yields a runtime behavior close to:

- `O(n * offset_limit)` iterations

And `offset_limit` quickly reaches 32640 for most of the input. Therefore, even with a fast inner loop, compression time can “explode” as soon as the input size becomes large (multi-MB).

### Why input size makes it impractical fast
Unlike codecs where runtime is mostly data-dependent (candidate match explosion only on certain patterns), ZX0 still scans offsets across a large window for each position.

Even on inputs with poor matches, the `offset = 1..max_offset` loop is still executed, so the runtime grows quickly with file size.

### Practical guidance
ZX0 is best reserved for relatively small binaries or cases where the best possible compression ratio is worth paying a high compression cost.

For large binaries, it is expected that ZX0 may become very slow to the point of being impractical.

### Integration/optimization notes
Even when forcing aggressive compilation (`#pragma GCC optimize("O3")`), compression can remain slow because the dominant cost is algorithmic.

Recent zELF-side micro-optimizations (no algorithm/bitstream change) were applied mainly in:

- `src/compressors/zx0/memory.c` (`zx0_assign()`/`zx0_allocate()` refcount and recycling hot paths)
- `src/compressors/zx0/optimize.c` (reduced redundant checks and cheaper bit-cost operations)

Profiling still shows most time in `zx0_assign`, `zx0_optimize`, and `zx0_allocate`, which is consistent with an optimal-parse DP manipulating many small state nodes.

### Possible future improvements (likely algorithm-level tradeoffs)
Further speedups without changing the output are limited, because the runtime is dominated by the optimal parse complexity.

If speed becomes a priority, potential directions include (but would change behavior/ratio):

- reducing `MAX_OFFSET_ZX0` for large inputs
- adding heuristics/pruning (limit explored offsets, early-exit on low potential)
- switching to a non-optimal parse mode on large inputs (near-optimal / greedy fallback)


## Shrinkler

### Why slow compression is expected
Shrinkler is historically oriented toward “smallest possible output” (demoscene / cracktro use cases), not throughput. It relies on expensive parsing/search plus range coding and refinement passes. Compared to speed-first codecs (LZ4/Snappy) or even general-purpose modern codecs, Shrinkler compression being slow is expected.

### Why it is slow in zELF (by design)
In `src/packer/compression.c`, the Shrinkler path calls `shrinkler_compress_memory(...)` with:

- `shrinkler_comp_params opt = {0};`

So the wrapper defaults are used.

In `src/compressors/shrinkler/shrinkler_comp.cpp`, the defaults imply multiple heavy passes:

- `iterations = 3` (multiple full parsing/estimation cycles)
- `length_margin = 3`
- `skip_length = 3000`
- `effort` maps to `match_patience = 300`
- `same_length` maps to `max_same_length = 30`
- the reference edge pool is fixed via `RefEdgeFactory edge_factory(400000)` (similar to Shrinkler’s `-r 400000` parameter)

The key point is the iterative optimization loop:

- for each iteration:
  - reset match-finder state
  - run `parser.parse(...)` (the dominant, expensive step)
  - encode into a “dummy” `RangeCoder` to estimate the size
  - update symbol frequencies via a counting pass to refine the next iteration

Even before writing the final bitstream, several full input passes are performed.

### Final verification pass adds another full decode
After encoding the best result, the wrapper performs a verification pass similar to the reference implementation:

- `RangeDecoder` + `LZDecoder` + `LZVerifier`

This is an additional full decode over the produced stream to validate correctness. It is a robustness feature, but it adds noticeable CPU cost.

### Data-dependent worst cases
Shrinkler runtime can become impractical when:

- the input is large (multiple MBs and above)
- the data contains many ambiguous repetitive patterns (many match candidates)
- the heuristics encourage exploring more candidates (`match_patience`, parsing graph growth)

This is similar in spirit to optimal/near-optimal parses (e.g. BriefLZ level 10): runtime is highly data-dependent.

### Compiler optimization note
Even when Shrinkler’s C++ translation unit is compiled with aggressive optimization (e.g. `-O3`), compression can remain slow because the dominant cost is algorithmic (multi-pass parsing + verification), not just instruction-level efficiency.
