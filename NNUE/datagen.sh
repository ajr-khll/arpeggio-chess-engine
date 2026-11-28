#!/usr/bin/env bash
set -e

cd "$(dirname "$0")/.."

if [ "$(uname -s)" = "Darwin" ]; then
  CPU_COUNT=$(sysctl -n hw.ncpu)
  SFML_INCLUDE="-I$(brew --prefix)/include"
else
  CPU_COUNT=$(nproc)
  SFML_INCLUDE=$(pkg-config --cflags sfml-graphics)
fi

WORKERS=${1:-$CPU_COUNT}
POSITIONS_PER_WORKER=${2:-500000}
DEPTH=${3:-3}
NETWORK=${4:-NNUE/nets/arpeggio-gen1.bin}

OUTPUT_DIRECTORY=NNUE/data
mkdir -p "$OUTPUT_DIRECTORY"

g++ -std=c++17 -O2 $SFML_INCLUDE \
  NNUE/datagen.cpp NNUE/nnue.cpp minimax.cpp Moves.cpp TranspositionTable.cpp simulateMoves.cpp \
  -o NNUE/datagen

echo "running $WORKERS workers, $POSITIONS_PER_WORKER positions each, depth $DEPTH, network $NETWORK"

SEED_BASE=$(date +%s)
for WORKER in $(seq 1 "$WORKERS"); do
  SEED=$((SEED_BASE + WORKER))
  NNUE/datagen "$SEED" "$POSITIONS_PER_WORKER" "$DEPTH" \
    "$OUTPUT_DIRECTORY/datagen-$SEED.txt" "$NETWORK" 2> "$OUTPUT_DIRECTORY/datagen-$SEED.log" &
done

wait

cat "$OUTPUT_DIRECTORY"/datagen-*.txt > "$OUTPUT_DIRECTORY/combined.txt"
echo "done: $(wc -l < "$OUTPUT_DIRECTORY/combined.txt") positions in $OUTPUT_DIRECTORY/combined.txt"
