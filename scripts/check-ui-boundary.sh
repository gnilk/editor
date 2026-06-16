#!/usr/bin/env bash
# AI-2 (docs/ui-refactor.md §8): the generic UI toolkit under src/Core/UI/ must never include an
# app-level header. Mirrors the existing src/Core/Graphics -> Core/Session|Editor.h invariant
# (CLAUDE.md "Graphics layer refactor"), scoped to the full toolkit folder.
#
# Exit 0 + silent  -> boundary clean.
# Exit 1 + a report -> the leak set; each line is a (file, forbidden header) pair to fix via
# AI-3 (RuntimeConfig/Editor.h -> UIHost), AI-4 (kAction), AI-5 (Core/Session -> ILayoutSink),
# AI-6 (Core/Config -> injected theme/colors).
set -euo pipefail

cd "$(dirname "$0")/.."

UI_DIR="src/Core/UI"

if [ ! -d "$UI_DIR" ]; then
    echo "check-ui-boundary: $UI_DIR not found" >&2
    exit 1
fi

# Pattern -> which break (AI-#) resolves it, for the report.
declare -a FORBIDDEN=(
    'Core/Editor\.h|AI-3 (UIHost)'
    '"Editor\.h"|AI-3 (UIHost)'
    'Core/RuntimeConfig\.h|AI-3 (UIHost)'
    '"RuntimeConfig\.h"|AI-3 (UIHost)'
    'Core/Document\.h|AI-4/§6 (editor model leak)'
    '"Document\.h"|AI-4/§6 (editor model leak)'
    'Core/TextBuffer\.h|AI-4/§6 (editor model leak)'
    '"TextBuffer\.h"|AI-4/§6 (editor model leak)'
    'Core/Workspace\.h|AI-4/§6 (editor model leak)'
    '"Workspace\.h"|AI-4/§6 (editor model leak)'
    'Core/Session/|AI-5 (ILayoutSink)'
    'Core/Plugins/|AI-6 (leaf cleanup)'
    'Core/Config/|AI-6 (inject theme/colors)'
)

failures=0

while IFS= read -r -d '' f; do
    for entry in "${FORBIDDEN[@]}"; do
        pattern="${entry%%|*}"
        reason="${entry#*|}"
        if grep -nE "#include[[:space:]]*\"${pattern}" "$f" > /tmp/check-ui-boundary.hit 2>/dev/null; then
            while IFS= read -r hit; do
                echo "${f}:${hit#*:}  -> ${reason}"
                failures=$((failures + 1))
            done < /tmp/check-ui-boundary.hit
        fi
    done
done < <(find "$UI_DIR" \( -name '*.h' -o -name '*.cpp' \) -print0)

rm -f /tmp/check-ui-boundary.hit

if [ "$failures" -gt 0 ]; then
    echo ""
    echo "check-ui-boundary: ${failures} app-level include(s) found under ${UI_DIR}/ (see docs/ui-refactor.md §8 AI-3/4/5/6)"
    exit 1
fi

echo "check-ui-boundary: clean — no app-level includes under ${UI_DIR}/"
exit 0
