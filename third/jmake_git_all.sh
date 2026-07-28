#!/usr/bin/env bash

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if ! command -v jmake >/dev/null 2>&1; then
    echo "Error: jmake not found in PATH"
    echo "Please run: source ../setup.sh"
    exit 127
fi

declare -a targets=()

if [ "$#" -gt 0 ]; then
    for name in "$@"; do
        if [ -d "$SCRIPT_DIR/$name" ]; then
            targets+=("$name")
        else
            echo "Skip: $name (directory not found)"
        fi
    done
else
    while IFS= read -r dir; do
        targets+=("$(basename "$dir")")
    done < <(find "$SCRIPT_DIR" -mindepth 1 -maxdepth 1 -type d | sort)
fi

if [ "${#targets[@]}" -eq 0 ]; then
    echo "No third-party directories found"
    exit 0
fi

success_list=()
fail_list=()
skip_list=()

for name in "${targets[@]}"; do
    dir="$SCRIPT_DIR/$name"

    if [ ! -f "$dir/build.js" ]; then
        echo "Skip: $name (build.js not found)"
        skip_list+=("$name")
        continue
    fi

    echo "============================================================"
    echo "Entering: $name"
    echo "Command : jmake -git"
    echo "============================================================"

    if (
        cd "$dir" &&
        jmake -git
    ); then
        success_list+=("$name")
        echo "Done: $name"
    else
        fail_list+=("$name")
        echo "Fail: $name"
    fi

    echo
done

echo "================ Summary ================"
echo "Success: ${#success_list[@]}"
for name in "${success_list[@]}"; do
    echo "  - $name"
done

echo "Skipped: ${#skip_list[@]}"
for name in "${skip_list[@]}"; do
    echo "  - $name"
done

echo "Failed : ${#fail_list[@]}"
for name in "${fail_list[@]}"; do
    echo "  - $name"
done

if [ "${#fail_list[@]}" -gt 0 ]; then
    exit 1
fi

exit 0
