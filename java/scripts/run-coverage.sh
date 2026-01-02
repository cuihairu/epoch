#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$ROOT_DIR/target/test-classes"
LIST_FILE="$ROOT_DIR/target/sources.txt"
JACOCO_VERSION="0.8.12"
CACHE_DIR="$ROOT_DIR/.cache"
AGENT_JAR="$CACHE_DIR/jacocoagent.jar"
CLI_JAR="$CACHE_DIR/jacococli.jar"
EXEC_FILE="$ROOT_DIR/target/jacoco.exec"
XML_FILE="$ROOT_DIR/target/jacoco.xml"

mkdir -p "$OUT_DIR" "$CACHE_DIR"

if [ ! -f "$AGENT_JAR" ]; then
  curl -sSL "https://repo1.maven.org/maven2/org/jacoco/org.jacoco.agent/${JACOCO_VERSION}/org.jacoco.agent-${JACOCO_VERSION}-runtime.jar" -o "$AGENT_JAR"
fi

if [ ! -f "$CLI_JAR" ]; then
  curl -sSL "https://repo1.maven.org/maven2/org/jacoco/org.jacoco.cli/${JACOCO_VERSION}/org.jacoco.cli-${JACOCO_VERSION}-nodeps.jar" -o "$CLI_JAR"
fi

find "$ROOT_DIR/src/main/java" "$ROOT_DIR/src/test/java" -name "*.java" > "$LIST_FILE"

javac --release 17 -d "$OUT_DIR" @"$LIST_FILE"

java -javaagent:"$AGENT_JAR"=destfile="$EXEC_FILE" -cp "$OUT_DIR" io.epoch.VectorTestMain

java -jar "$CLI_JAR" report "$EXEC_FILE" \
  --classfiles "$OUT_DIR" \
  --sourcefiles "$ROOT_DIR/src/main/java" \
  --xml "$XML_FILE"
