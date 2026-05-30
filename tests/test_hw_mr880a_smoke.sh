#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
module_root="$(cd "$script_dir/.." && pwd)"
artifact_dir="${SROBOTIS_TEST_ARTIFACT_DIR:-${SROBOTIS_OUTPUT_ROOT:-$PWD/output}/test-artifacts/components/peripherals/5g/${SROBOTIS_TEST_NAME:-5g-mr880a-hardware-smoke}}"
log_dir="$artifact_dir/logs"
build_dir="$artifact_dir/build"
log_file="$log_dir/5g_mr880a_hardware_smoke.log"

dev_path="${MODEM_5G_UART_DEV:-auto}"
baud="${MODEM_5G_UART_BAUD:-9600}"
timeout_s="${MODEM_5G_HW_SMOKE_TIMEOUT_S:-45}"

mkdir -p "$log_dir" "$build_dir"

{
    echo "[info] module_root=$module_root"
    echo "[info] build_dir=$build_dir"
    echo "[info] dev_path=$dev_path"
    echo "[info] baud=$baud"
    echo "[info] timeout_s=$timeout_s"

    cmake -S "$module_root" -B "$build_dir" -DBUILD_TESTS=ON
    cmake --build "$build_dir" --target test_5g_mr880a -j"$(nproc)"
    LD_LIBRARY_PATH="$build_dir:${LD_LIBRARY_PATH:-}" \
        timeout "$timeout_s" "$build_dir/test_5g_mr880a" -d "$dev_path" -b "$baud"
} 2>&1 | tee "$log_file"

grep -Eq "Manufacturer:|Model:|SIM state:|Reg state:|RSSI:" "$log_file"
