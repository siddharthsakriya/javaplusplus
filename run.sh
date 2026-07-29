#!/bin/bash
# Builds the agent (if needed), compiles the sample programs in samples/,
# runs each one with the agent attached, and writes the profiler output
# (text report, JSON, folded-stack, and a flame graph SVG if flamegraph.pl
# is available) to samples/output/<SampleName>/.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

case "$(uname -s)" in
    Darwin) AGENT_LIB="build/javaplusplus.dylib" ;;
    *)      AGENT_LIB="build/javaplusplus.so" ;;
esac

if [ ! -f "$AGENT_LIB" ]; then
    echo "Agent library not found, building..."
    ./build.sh
fi

FLAMEGRAPH_PL=""
if [ -x "./flamegraph.pl" ]; then
    FLAMEGRAPH_PL="./flamegraph.pl"
elif command -v flamegraph.pl >/dev/null 2>&1; then
    FLAMEGRAPH_PL="flamegraph.pl"
fi

SAMPLES_SRC="samples"
CLASSES_DIR="build/samples-classes"
OUTPUT_DIR="samples/output"

mkdir -p "$CLASSES_DIR"
echo "Compiling samples..."
javac -d "$CLASSES_DIR" "$SAMPLES_SRC"/*.java

for CLASS_FILE in "$CLASSES_DIR"/Sample*.class; do
    NAME="$(basename "$CLASS_FILE" .class)"
    DEST="$OUTPUT_DIR/$NAME"
    mkdir -p "$DEST"

    echo "Running $NAME..."
    java "-agentpath:$AGENT_LIB=logpath=$DEST/report.txt,jsonpath=$DEST/profiler.json,flamepath=$DEST/profiler.folded" \
        -cp "$CLASSES_DIR" "$NAME"

    if [ -n "$FLAMEGRAPH_PL" ]; then
        "$FLAMEGRAPH_PL" "$DEST/profiler.folded" > "$DEST/flamegraph.svg" 2>/dev/null
        echo "  -> $DEST/flamegraph.svg"
    else
        echo "  -> flamegraph.pl not found, skipping SVG generation for $NAME"
    fi
done

echo "Done. Output in $OUTPUT_DIR/"
