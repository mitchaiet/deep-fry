#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build build --config Release --parallel "${DEEPFRY_JOBS:-4}"
ctest --test-dir build -C Release --output-on-failure
