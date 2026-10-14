#!/usr/bin/env python3
"""Display *physical* memory, swap and AMD GTT; intentionally never counts swap as GPU RAM."""
from pathlib import Path
import sys

GIB = 1024 ** 3

def show(byte_count):
    return f"{byte_count / GIB:.2f} GiB"

def meminfo():
    values = {}
    for line in Path('/proc/meminfo').read_text().splitlines():
        field, sep, rest = line.partition(':')
        if sep:
            parts = rest.split()
            if parts and parts[0].isdigit():
                values[field] = int(parts[0]) * (1024 if len(parts)>1 and parts[1]=='kB' else 1)
    return values

def main():
    m = meminfo()
    total = m.get('MemTotal', 0)
    avail = m.get('MemAvailable', 0)
    swap = m.get('SwapTotal', 0)
    print(f"Physical RAM: {show(total)} (MemAvailable: {show(avail)})")
    print(f"Configured swap: {show(swap)} — NOT counted as available AMD HIP device RAM")
    if total < 80*GIB:
        print('WARNING: physical RAM is below 80 GiB; this 66 GiB-weights workload is unlikely to fit.')
    if avail < 76*GIB:
        print('WARNING: MemAvailable is below 76 GiB before model load. Free RAM or reduce GPU workload.')
    amd = 0
    for card in sorted(Path('/sys/class/drm').glob('card[0-9]*')):
        vendor_path = card / 'device/vendor'
        try:
            vendor = vendor_path.read_text().strip().lower()
        except OSError:
            continue
        if vendor != '0x1002':  # AMD; avoid accidentally reading the RTX 2070
            continue
        amd += 1
        gtt = card / 'device/mem_info_gtt_total'
        try:
            value = int(gtt.read_text().strip())
            print(f"{card.name} AMD GTT reported total: {show(value)}")
            if value < 72 * GIB:
                print('WARNING: AMD GTT under 72 GiB. Verify kernel GTT/TTM limits before launching.')
        except (OSError, ValueError):
            print(f"{card.name} AMD GTT total: unavailable (check driver, KFD and memory settings)")
    if amd == 0:
        print('NOTE: no AMD DRM card visible here. Run on the 890M Linux host.')
    print('Read docs/890m-96gb.md for safer configuration; these checks are not a GPU allocation test.')

if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError) as e:
        print(f"Unable to read memory metrics: {e}", file=sys.stderr)
        sys.exit(1)
