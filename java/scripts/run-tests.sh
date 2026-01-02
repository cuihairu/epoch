#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$ROOT_DIR/target/test-classes"
LIST_FILE="$ROOT_DIR/target/sources.txt"
CLASSPATH="$("$ROOT_DIR/scripts/fetch-aeron.sh")"

mkdir -p "$OUT_DIR"

find "$ROOT_DIR/src/main/java" "$ROOT_DIR/src/test/java" -name "*.java" > "$LIST_FILE"

javac --release 17 -cp "$CLASSPATH" -d "$OUT_DIR" @"$LIST_FILE"

java --add-opens java.base/jdk.internal.misc=ALL-UNNAMED -cp "$OUT_DIR:$CLASSPATH" io.epoch.VectorTestMain
