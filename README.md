# strixite — Radeon 890M / `gfx1150` (96 GB port)

A source-compatible port of [strixite](https://github.com/shawnshekari/strixite)'s Qwen3.8-Flash-Next inference engine to AMD Radeon 890M (`gfx1150`, RDNA 3.5), for a Linux computer with **96 GB unified physical memory**. The original project is optimized and benchmarked on the Radeon 8060S (`gfx1151`) with 128 GB. **This 890M variant has not been GPU-tested**, so performance and successful large-model allocation are not guaranteed.

The original source, model formats, GPU kernels, HTTP server and host tools remain included. See the [original README](UPSTREAM-README.md) for the model, weights and feature descriptions; its *Strix Halo* installation instructions and measured speeds are not appropriate for the 890M. See [890M memory/setup details](docs/890m-96gb.md) for important device and swap limitations.

## Quick start — Linux, gfx1150

1. Obtain a [Linux TheRock ROCm toolchain compiled for `gfx1150`](https://rocm.nightlies.amd.com/tarball-multi-arch/) and extract it under `~/tools/therock-tarball/install`, or set `STRIX_ROCM_ROOT` to the extract directory. You need CMake 3.28+, Ninja, g++ and working AMDGPU/KFD drivers for the 890M.
2. Run `python3 tools/check_890m_memory.py` to check **physical** RAM and the AMD GTT aperture (not RTX swap size). Adjust driver memory limits if necessary; see the [kernel configuration notes](docs/890m-96gb.md#unified-memory-gtt-configuration).
3. Configure and build:

```bash
cmake --preset 890m -DSTRIX_ROCM_ROOT="$HOME/tools/therock-tarball/install"
cmake --build --preset 890m -j "$(nproc)"
```

4. Put the original project's converted weights and separate n-gram table in `~/models/strix-infer` (see [weights and conversion](docs/tools.md) or download with `hf download wemoh/Qwen3.8-Flash-Next-strixw --local-dir ~/models/strix-infer`). The files take approximately 115 GiB on disk; they are *not* embedded in this repository.
5. Launch the OpenAI-compatible server:

```bash
export STRIX_MODELS_DIR="$HOME/models/strix-infer"
export HSA_DISABLE_COREDUMP_ON_EXCEPTION=1
build/890m/strix_server --config deploy/890m-96gb.conf
```

The served port defaults to `5300`. To check settings without GPU allocation, use `build/890m/strix_server --print-config`. First-boot fallback if GPU memory is tight:

```bash
build/890m/strix_server --config deploy/890m-96gb.conf \
  --capacity 32768 --chunk 1024 --mtp off
```

### Memory profile

| Setting | 890M fork | Upstream 128 GB Halo |
|---|---:|---:|
| HIP architecture | `gfx1150` | `gfx1151` |
| Context capacity | 65,536 | 524,288 |
| Prefill chunk | 2,048 | 16,384 |
| In-RAM n-gram rows | 262,144 | 8,388,608 |
| Host RAM safety margin | 6 GiB | 4 GiB |
| Prompt cache | 128 GiB maximum **on disk** | 128 GiB |

~66 GiB of quantized model weights still need **GPU-visible resident memory**, plus context, activation and workspace buffers. A machine advertised as 96 GB has ~89.4 GiB physical RAM if GB is decimal; the operating system and GTT must share it. **The RTX 2070 is not transparently available as extra AMD HIP memory**: a fast NVIDIA-backed swap tier may help CPU-pageable caches, but does not make `hipMalloc` GPU allocations spill into the 2070. Details: [890M setup and limitations](docs/890m-96gb.md#your-rtx-2070-as-a-fast-swapcache-tier).

The original Strix Halo build is still accessible through `cmake --preset strix` (requires a gfx1151-capable toolchain) and `deploy/strix-server.conf`.

## Tests and scope

`bash tests/check_890m.sh` tests the configuration parser, 890M defaults, explicit overrides and project presets with a host compiler. HIP/ROCm compilation, device execution, numerical accuracy, memory fit and performance **require testing on a real 890M system**; this package does not claim to have performed them. See [upstream license](LICENSE), [third-party notices](THIRD_PARTY.md), and the [upstream project](https://github.com/shawnshekari/strixite).
