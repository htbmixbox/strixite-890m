#!/usr/bin/env bash
# Portable config smoke test, no ROCm/GPU required. Run from any cwd.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
python3 - <<'PY'
import json
with open('CMakePresets.json') as f: p=json.load(f)
conf={x['name']:x for x in p['configurePresets']}
assert conf['890m']['cacheVariables']['STRIX_GPU_ARCH']=='gfx1150'
assert conf['strix']['cacheVariables']['STRIX_GPU_ARCH']=='gfx1151'
assert conf['890m-debug']['inherits']=='890m'
assert conf['890m']['binaryDir']=='${sourceDir}/build/890m'
assert {'890m','strix'} <= {x['name'] for x in p['buildPresets']}
cmake=open('CMakeLists.txt').read()
assert '--offload-arch=${STRIX_GPU_ARCH}' in cmake
assert 'STRIX_TARGET_GPU_ARCH' in cmake
print('PASS: 890M / Halo CMake presets and HIP ISA flags')
PY
bin=$(mktemp "${TMPDIR:-/tmp}/strix-890m-smoke.XXXXXX")
trap 'rm -f "$bin"' EXIT
"${CXX:-g++}" -std=c++20 -O0 -I. serve/server_config.cpp tests/890m_profile.cpp -o "$bin"
"$bin"
echo 'PASS: host-only 890M smoke suite (no HIP compilation or GPU run)'
