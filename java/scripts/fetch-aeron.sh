#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CACHE_DIR="$ROOT_DIR/.cache"

AERON_VERSION="1.49.3"
AGRGONA_VERSION="2.3.2"

CLIENT_JAR="$CACHE_DIR/aeron-client-${AERON_VERSION}.jar"
DRIVER_JAR="$CACHE_DIR/aeron-driver-${AERON_VERSION}.jar"
AGRGONA_JAR="$CACHE_DIR/agrona-${AGRGONA_VERSION}.jar"

mkdir -p "$CACHE_DIR"

if [ ! -f "$CLIENT_JAR" ]; then
  curl -sSL "https://repo1.maven.org/maven2/io/aeron/aeron-client/${AERON_VERSION}/aeron-client-${AERON_VERSION}.jar" -o "$CLIENT_JAR"
fi

if [ ! -f "$DRIVER_JAR" ]; then
  curl -sSL "https://repo1.maven.org/maven2/io/aeron/aeron-driver/${AERON_VERSION}/aeron-driver-${AERON_VERSION}.jar" -o "$DRIVER_JAR"
fi

if [ ! -f "$AGRGONA_JAR" ]; then
  curl -sSL "https://repo1.maven.org/maven2/org/agrona/agrona/${AGRGONA_VERSION}/agrona-${AGRGONA_VERSION}.jar" -o "$AGRGONA_JAR"
fi

echo "${CLIENT_JAR}:${DRIVER_JAR}:${AGRGONA_JAR}"
