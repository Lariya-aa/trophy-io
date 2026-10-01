#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
task_source=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
task_build=$(mktemp -d "${TMPDIR:-/tmp}/trophy-io-build.XXXXXX")
task_image='vitasdk/vitasdk@sha256:7f5eee50ff95b73c8c847dbfef6227aa0035886444b4d0254c21da8369ff1efc'
printf 'Build directory: %s\n' "$task_build"
if docker run --rm --network none -v "$task_source:/workspace:ro" -v "$task_build:/build" \
    -w /workspace "$task_image" sh -c \
    'cmake -S . -B /build -DCMAKE_BUILD_TYPE=Release && cmake --build /build -j4' \
    >"$task_build/build.log" 2>&1; then
    cat "$task_build/build.log"
    printf 'VPK: %s\n' "$task_build"/TrophyBatchManager-*.vpk
else
    cat "$task_build/build.log"
    exit 1
fi
