#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

ARCH="${1:-rts3917}"
if [ "$#" -gt 0 ]; then
    shift
fi

if [ "$#" -gt 0 ]; then
    SEARCH_ROOTS=("$@")
else
    SEARCH_ROOTS=("third" "cam" "chip")
fi

SOURCE_EXTENSIONS=(
    -name '*.c'
    -o -name '*.cc'
    -o -name '*.cpp'
    -o -name '*.cxx'
    -o -name '*.s'
    -o -name '*.S'
)

created_dirs=0
processed_targets=0
processed_modules=0

extract_targets() {
    local build_js="$1"

    sed -nE "s/.*createTarget\\([[:space:]]*['\"]([^'\"]+)['\"].*/\\1/p" "$build_js" | sort -u
}

collect_source_dirs() {
    local module_dir="$1"

    (
        cd "$module_dir"
        find . \
            -path './build' -prune -o \
            -path './build/*' -prune -o \
            -path './.git' -prune -o \
            -path './.git/*' -prune -o \
            -type f \( "${SOURCE_EXTENSIONS[@]}" \) -print |
            sed 's#^\./##' |
            xargs -r -n1 dirname |
            sort -u
    )
}

ensure_dir() {
    local dir="$1"
    if [ ! -d "$dir" ]; then
        mkdir -p "$dir"
        created_dirs=$((created_dirs + 1))
    fi
}

echo "Preparing jmake output directories"
echo "Project root : $ROOT_DIR"
echo "Target arch  : $ARCH"
echo "Search roots : ${SEARCH_ROOTS[*]}"
echo

for search_root in "${SEARCH_ROOTS[@]}"; do
    abs_root="$ROOT_DIR/$search_root"
    if [ ! -d "$abs_root" ]; then
        echo "Skip root: $search_root (not found)"
        continue
    fi

    while IFS= read -r build_js; do
        module_dir="$(dirname "$build_js")"
        rel_module_dir="${module_dir#$ROOT_DIR/}"

        mapfile -t targets < <(extract_targets "$build_js")
        if [ "${#targets[@]}" -eq 0 ]; then
            echo "Skip module: $rel_module_dir (no literal createTarget found)"
            continue
        fi

        mapfile -t source_dirs < <(collect_source_dirs "$module_dir")
        if [ "${#source_dirs[@]}" -eq 0 ]; then
            echo "Skip module: $rel_module_dir (no source directories found)"
            continue
        fi

        processed_modules=$((processed_modules + 1))
        echo "Module: $rel_module_dir"

        for target in "${targets[@]}"; do
            target_dir="$module_dir/build/$ARCH/t_$target"
            processed_targets=$((processed_targets + 1))

            ensure_dir "$target_dir/.deps"
            ensure_dir "$target_dir/objs"

            for source_dir in "${source_dirs[@]}"; do
                ensure_dir "$target_dir/.deps/$source_dir"
                ensure_dir "$target_dir/objs/$source_dir"
            done

            echo "  prepared: ${target_dir#$ROOT_DIR/}"
        done
    done < <(find "$abs_root" -name build.js | sort)
done

echo
echo "Summary:"
echo "  modules : $processed_modules"
echo "  targets : $processed_targets"
echo "  new dirs: $created_dirs"
