#include "serve/sysmem.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace strix {

namespace {
constexpr uint64_t kGiB = 1ull << 30;
constexpr uint64_t kSessionFloor = 2 * kGiB;  // the least the session's buffers need next to the weights
constexpr uint64_t kHostReserve = 6 * kGiB;   // what the OS and the host-side caches keep of the machine's RAM

std::string gib_text(uint64_t bytes) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", (double)bytes / (double)kGiB);
    return buf;
}
}  // namespace

GttInfo read_gtt(const std::string &drm_root) {
    GttInfo best;
    std::error_code ec;
    std::filesystem::directory_iterator it(drm_root, ec), end;
    if (ec) return best;
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        const std::string name = it->path().filename().string();
        // card0, card1, ... (not card0-DP-1 connectors, not renderD*)
        if (name.rfind("card", 0) != 0 || name.find('-') != std::string::npos) continue;
        const std::filesystem::path file = it->path() / "device" / "mem_info_gtt_total";
        std::ifstream f(file);
        unsigned long long v = 0;
        if (!f.good() || !(f >> v) || v == 0) continue;
        if (!best.found || v > best.total) {
            best.found = true;
            best.total = v;
            best.path = file.string();
        }
    }
    return best;
}

uint64_t read_mem_total(const std::string &meminfo_path) {
    std::ifstream f(meminfo_path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("MemTotal:", 0) != 0) continue;
        unsigned long long kb = 0;
        std::istringstream(line.substr(9)) >> kb;
        return (uint64_t)kb * 1024ull;
    }
    return 0;
}

GttParams gtt_kernel_params(uint64_t gtt_gib) {
    GttParams p;
    p.gttsize_mib = gtt_gib * 1024ull;
    p.pages_limit = gtt_gib * (kGiB / 4096ull);
    return p;
}

uint64_t recommended_gtt_gib(uint64_t mem_total, uint64_t weights_bytes) {
    const uint64_t floor_gib = (weights_bytes + kSessionFloor + kGiB - 1) / kGiB;
    const uint64_t ram_gib = mem_total / kGiB;
    const uint64_t from_ram = ram_gib > kHostReserve / kGiB ? ram_gib - kHostReserve / kGiB : 0;
    return std::max(floor_gib, from_ram);
}

std::string check_gtt_for_weights(uint64_t gtt_total, uint64_t weights_bytes, uint64_t mem_total) {
    if (gtt_total == 0) return "";
    const uint64_t need = weights_bytes + kSessionFloor;
    if (gtt_total >= need) return "";
    std::ostringstream m;
    m << "the GPU's GTT limit is " << gib_text(gtt_total) << " GiB but the weights alone are " << gib_text(weights_bytes)
      << " GiB (plus at least " << gib_text(kSessionFloor) << " GiB of session buffers = " << gib_text(need)
      << " GiB). The GPU allocates from system RAM through the GTT, and its default size is typically about half of RAM.";
    if (mem_total > 0 && mem_total < need + kHostReserve)
        m << " This machine's MemTotal is " << gib_text(mem_total) << " GiB, which is short of the weights + "
          << gib_text(kHostReserve) << " GiB for the system: if the BIOS reserves a large dedicated GPU memory "
          << "(UMA frame buffer), set it to the minimum so that RAM returns to the system.";
    const GttParams p = gtt_kernel_params(recommended_gtt_gib(mem_total, weights_bytes));
    m << " Add these kernel parameters and reboot: amdgpu.gttsize=" << p.gttsize_mib
      << " ttm.pages_limit=" << p.pages_limit << " amd_iommu=off (then mem_info_gtt_total reports ~"
      << gib_text(p.gttsize_mib << 20) << " GiB). To skip this check: gtt-check = off.";
    return m.str();
}

}  // namespace strix
