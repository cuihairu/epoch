#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
MAIN_OUT="$ROOT_DIR/target/classes"
OUT_DIR="$ROOT_DIR/target/test-classes"
MAIN_LIST="$ROOT_DIR/target/main-sources.txt"
LIST_FILE="$ROOT_DIR/target/sources.txt"
JACOCO_VERSION="0.8.12"
CACHE_DIR="$ROOT_DIR/.cache"
AGENT_JAR="$CACHE_DIR/jacocoagent.jar"
CLI_JAR="$CACHE_DIR/jacococli.jar"
EXEC_FILE="$ROOT_DIR/target/jacoco.exec"
XML_FILE="$ROOT_DIR/target/jacoco.xml"
CLASSPATH="$("$ROOT_DIR/scripts/fetch-aeron.sh")"

mkdir -p "$OUT_DIR" "$MAIN_OUT" "$CACHE_DIR"

if [ ! -f "$AGENT_JAR" ]; then
  curl -sSL "https://repo1.maven.org/maven2/org/jacoco/org.jacoco.agent/${JACOCO_VERSION}/org.jacoco.agent-${JACOCO_VERSION}-runtime.jar" -o "$AGENT_JAR"
fi

if [ ! -f "$CLI_JAR" ]; then
  curl -sSL "https://repo1.maven.org/maven2/org/jacoco/org.jacoco.cli/${JACOCO_VERSION}/org.jacoco.cli-${JACOCO_VERSION}-nodeps.jar" -o "$CLI_JAR"
fi

find "$ROOT_DIR/src/main/java" -name "*.java" > "$MAIN_LIST"
find "$ROOT_DIR/src/main/java" "$ROOT_DIR/src/test/java" -name "*.java" > "$LIST_FILE"

# Compile main classes first so the coverage report can scope to production
# sources only; test sources then compile against them.
javac --release 17 -cp "$CLASSPATH" -d "$MAIN_OUT" @"$MAIN_LIST"
javac --release 17 -cp "$MAIN_OUT:$CLASSPATH" -d "$OUT_DIR" @"$LIST_FILE"

java --add-opens java.base/jdk.internal.misc=ALL-UNNAMED -javaagent:"$AGENT_JAR"=destfile="$EXEC_FILE" -cp "$OUT_DIR:$MAIN_OUT:$CLASSPATH" io.epoch.VectorTestMain

java -jar "$CLI_JAR" report "$EXEC_FILE" \
  --classfiles "$MAIN_OUT" \
  --sourcefiles "$ROOT_DIR/src/main/java" \
  --xml "$XML_FILE"
