#!/usr/bin/env bash
# Build and run the PC test harnesses with plain gcc.
#
# Usage: tests/run_tests.sh [name ...]     (names: map battle rules trace interp troopflow bake_ai)
#
# Tests that need generated game data (made by tools/stage_data.py from your
# own copy of the game) are SKIPPED when that data is missing, not failed, so
# this works on a fresh clone and in CI. Exit status is non-zero only if a
# test that actually ran failed to build or failed its checks.
set -u
cd "$(dirname "${BASH_SOURCE[0]}")/.."

CC="${CC:-gcc}"
CFLAGS="${CFLAGS:--Wall -Wextra -O2}"
OUT="${TMPDIR:-/tmp}/fh_tests"
mkdir -p "$OUT"

# name | include flags | sources | libs | required generated file ("-" = none)
TESTS=(
  "map|-I runtime|tests/test_map.c runtime/map.c||-"
  "battle|-I runtime|tests/test_battle.c runtime/battle.c|-lm|-"
  "rules|-I runtime|tests/test_battle_rules.c runtime/battle.c|-lm|-"
  "trace|-I runtime|tests/test_trace_buf.c||-"
  "interp|-I runtime -I .|tests/test_interp.c runtime/interp.c runtime/text.c||tests/test_vec.h"
  "troopflow|-I runtime -I psp/gu_demo|tests/test_troopflow.c runtime/battle.c runtime/interp.c runtime/battle_blob.c|-lm|psp/gu_demo/battle_db.h"
)

want=("$@")
selected() {
  [ ${#want[@]} -eq 0 ] && return 0
  local w; for w in "${want[@]}"; do [ "$w" = "$1" ] && return 0; done
  return 1
}

passed=0; failed=0; skipped=0
for spec in "${TESTS[@]}"; do
  IFS='|' read -r name inc src libs need <<<"$spec"
  selected "$name" || continue

  if [ "$need" != "-" ] && [ ! -e "$need" ]; then
    echo "SKIP  $name (needs generated file: $need)"
    skipped=$((skipped + 1)); continue
  fi
  # shellcheck disable=SC2086
  if ! $CC $CFLAGS $inc -o "$OUT/test_$name" $src $libs; then
    echo "FAIL  $name (build error)"; failed=$((failed + 1)); continue
  fi
  if timeout 60 "$OUT/test_$name" >"$OUT/$name.log" 2>&1; then
    echo "PASS  $name"; passed=$((passed + 1))
  else
    echo "FAIL  $name (log: $OUT/$name.log)"; tail -5 "$OUT/$name.log" | sed 's/^/        /'
    failed=$((failed + 1))
  fi
done

# Python tool tests (need only python3)
if selected bake_ai; then
  if python3 tests/test_bake_ai.py >"$OUT/bake_ai.log" 2>&1; then
    echo "PASS  bake_ai"; passed=$((passed + 1))
  else
    echo "FAIL  bake_ai (log: $OUT/bake_ai.log)"; tail -5 "$OUT/bake_ai.log" | sed 's/^/        /'
    failed=$((failed + 1))
  fi
fi

echo "---- $passed passed, $failed failed, $skipped skipped"
[ "$failed" -eq 0 ]
