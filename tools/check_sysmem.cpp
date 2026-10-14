// Host-only check of serve/sysmem (no model, no GPU): the GTT probe against a fake sysfs tree, the MemTotal parser,
// the kernel-parameter arithmetic and the preflight's refusal text.
//   check_sysmem            exits 0 when every case passes

#include "serve/sysmem.hpp"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace strix;

namespace {
int g_failed = 0;
void expect(bool ok, const char *what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failed;
    }
}
void write(const fs::path &p, const std::string &text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p) << text;
}
constexpr uint64_t kGiB = 1ull << 30;
}  // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / ("check_sysmem." + std::to_string(::getpid()));
    fs::remove_all(root);

    // An NVIDIA card (no GTT file), the iGPU (48 GiB default) and a connector entry that must be ignored.
    write(root / "drm/card0/device/vendor", "0x10de\n");
    write(root / "drm/card1/device/mem_info_gtt_total", "51539607552\n");
    write(root / "drm/card1-DP-1/device/mem_info_gtt_total", "999999999999\n");
    GttInfo g = read_gtt((root / "drm").string());
    expect(g.found && g.total == 48 * kGiB, "iGPU GTT read; NVIDIA card and connector skipped");

    // Two AMD cards: the larger limit wins.
    write(root / "drm/card2/device/mem_info_gtt_total", std::to_string(88 * kGiB) + "\n");
    g = read_gtt((root / "drm").string());
    expect(g.found && g.total == 88 * kGiB, "the largest GTT limit wins");

    expect(!read_gtt((root / "missing").string()).found, "no drm root: not found");

    write(root / "meminfo", "MemTotal:       98000000 kB\nMemFree:        1 kB\n");
    expect(read_mem_total((root / "meminfo").string()) == 98000000ull * 1024, "MemTotal parsed");
    expect(read_mem_total((root / "nope").string()) == 0, "missing meminfo: 0");

    const GttParams p = gtt_kernel_params(88);
    expect(p.gttsize_mib == 90112 && p.pages_limit == 23068672, "88 GiB -> gttsize 90112, pages_limit 23068672");
    // upstream's Strix Halo numbers: 124 GiB
    const GttParams h = gtt_kernel_params(124);
    expect(h.gttsize_mib == 126976 && h.pages_limit == 32505856, "124 GiB -> upstream's 126976 / 32505856");

    const uint64_t weights = (uint64_t)(65.9 * (double)kGiB);
    expect(recommended_gtt_gib(93 * kGiB, weights) == 87, "93 GiB MemTotal -> 87 GiB GTT");
    expect(recommended_gtt_gib(60 * kGiB, weights) == 68, "small machine: the weights + 2 GiB floor rules");

    expect(check_gtt_for_weights(0, weights, 93 * kGiB).empty(), "unknown GTT: no claim");
    expect(check_gtt_for_weights(88 * kGiB, weights, 93 * kGiB).empty(), "88 GiB GTT passes");
    expect(check_gtt_for_weights(68 * kGiB, weights, 93 * kGiB).empty(), "weights + 2 GiB exactly-ish passes");
    const std::string bad = check_gtt_for_weights(46 * kGiB, weights, 93 * kGiB);
    expect(!bad.empty() && bad.find("amdgpu.gttsize=89088") != std::string::npos &&
               bad.find("ttm.pages_limit=22806528") != std::string::npos,
           "46 GiB GTT refused with the parameters for 87 GiB");
    const std::string shrunk = check_gtt_for_weights(46 * kGiB, weights, 60 * kGiB);
    expect(shrunk.find("dedicated GPU memory") != std::string::npos, "low MemTotal mentions the UMA frame buffer");

    fs::remove_all(root);
    if (g_failed) return 1;
    std::puts("check_sysmem: all cases pass");
    return 0;
}
