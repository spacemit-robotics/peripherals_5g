#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
module_root="$(cd "$script_dir/.." && pwd)"
artifact_dir="${SROBOTIS_TEST_ARTIFACT_DIR:-${SROBOTIS_OUTPUT_ROOT:-$PWD/output}/test-artifacts/components/peripherals/5g/${SROBOTIS_TEST_NAME:-5g-api-contract}}"
log_dir="$artifact_dir/logs"
build_dir="$artifact_dir/build"
mode="${1:-all}"
case "$mode" in
    all|functional|error-paths) ;;
    *) echo "usage: $0 [all|functional|error-paths]" >&2; exit 2 ;;
esac

log_file="$log_dir/5g_api_${mode//-/_}.log"
cc="${CC:-cc}"

mkdir -p "$log_dir" "$build_dir"

{
    echo "[info] module_root=$module_root"
    echo "[info] build_dir=$build_dir"
    echo "[info] cc=$cc"
    echo "[info] mode=$mode"

    "$cc" -std=c99 -Wall -Wextra -Werror \
        -I"$module_root/include" \
        -I"$module_root/src" \
        "$module_root/src/modem_5g_core.c" \
        "$module_root/tests/test_5g_api_contract.c" \
        -o "$build_dir/test_5g_api_contract"

    "$build_dir/test_5g_api_contract" "$mode"
} | tee "$log_file"

case "$mode" in
    all) grep -q "5g api contract test PASSED" "$log_file" ;;
    functional) grep -q "5g api functional test PASSED" "$log_file" ;;
    error-paths) grep -q "5g api error paths test PASSED" "$log_file" ;;
esac
