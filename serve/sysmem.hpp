#pragma once

// Host-only facts about the machine's memory that the GPU shares - no HIP, so it builds and tests anywhere.
//
// On an APU (Radeon 890M, Strix Halo) the GPU's allocations come out of system RAM through the GTT aperture, whose
// size is a kernel limit (amdgpu.gttsize + ttm.pages_limit). The default is about half of RAM, which cannot hold the
// 65.9 GiB weights on a 96 GB machine. Without a check, the failure is a hipMalloc error in the middle of loading;
// the startup preflight (tools/strix_server.cpp) turns it into one message naming the kernel parameters to set.

#include <cstdint>
#include <string>

namespace strix {

struct GttInfo {
    bool found = false;   // some card under the drm root has mem_info_gtt_total
    uint64_t total = 0;   // bytes: the largest GTT limit found (the iGPU's; a discrete AMD card's is smaller or equal)
    std::string path;     // the file it was read from
};

// The largest mem_info_gtt_total under `drm_root`/card*/device/. Cards without the file (NVIDIA, virtio) are skipped.
GttInfo read_gtt(const std::string &drm_root = "/sys/class/drm");

// /proc/meminfo's MemTotal in bytes; 0 if it can't be read.
uint64_t read_mem_total(const std::string &meminfo_path = "/proc/meminfo");

// Kernel parameters that give the GPU `gtt_gib` GiB of GTT: amdgpu.gttsize is in MiB, ttm.pages_limit in 4 KiB pages.
struct GttParams {
    uint64_t gttsize_mib = 0;
    uint64_t pages_limit = 0;
};
GttParams gtt_kernel_params(uint64_t gtt_gib);

// The GTT size to recommend for a machine with `mem_total` bytes of RAM: all of it but ~6 GiB for the operating system
// and the host-side caches, rounded down to a whole GiB, at least `weights_bytes` + 2 GiB.
uint64_t recommended_gtt_gib(uint64_t mem_total, uint64_t weights_bytes);

// "" when the GTT limit can hold the weights plus a 2 GiB floor for the session's buffers (or when gtt_total is 0:
// unknown, so nothing is claimed); else the refusal message: what was found, what is needed, and the parameters to set.
std::string check_gtt_for_weights(uint64_t gtt_total, uint64_t weights_bytes, uint64_t mem_total);

}  // namespace strix
