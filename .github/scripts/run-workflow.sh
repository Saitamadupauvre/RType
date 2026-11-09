#!/usr/bin/env bash
set -euo pipefail

preset=$1
attempts=3

for attempt in $(seq "$attempts"); do
  if cmake --preset "$preset" --fresh; then
    break
  fi
  if [ "$attempt" -eq "$attempts" ]; then
    exit 1
  fi
  echo "::warning::Configuring $preset failed (attempt $attempt of $attempts), retrying"
  sleep $((attempt * 30))
done

cmake --workflow --preset "$preset"
