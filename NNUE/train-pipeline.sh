#!/usr/bin/env bash
set -euo pipefail

TOTAL_POSITIONS=${1:-20000000}
DEPTH=${2:-3}
NET_NAME=${3:-arpeggio-gen1}
DATAGEN_NETWORK=${4:-NNUE/nets/arpeggio-gen1.bin}

CHESS_DIRECTORY="$HOME/Projects/chess-ai"
BULLET_DIRECTORY="$HOME/Projects/bullet"
DATA_DIRECTORY="$CHESS_DIRECTORY/NNUE/data"
NETS_DIRECTORY="$CHESS_DIRECTORY/NNUE/nets"
BULLET_UTILS="$BULLET_DIRECTORY/target/release/bullet-utils"

if [ "$(uname -s)" = "Darwin" ]; then
  WORKERS=$(sysctl -n hw.ncpu)
  GPU_FEATURE=metal
else
  WORKERS=$(nproc)
  GPU_FEATURE=cuda
fi
POSITIONS_PER_WORKER=$(( (TOTAL_POSITIONS + WORKERS - 1) / WORKERS ))

announce() {
  echo
  echo "=== [$(date '+%Y-%m-%d %H:%M:%S')] $1 ==="
}

positions_so_far() {
  local total=0
  local log_file
  for log_file in "$DATA_DIRECTORY"/datagen-*.log; do
    if [ -s "$log_file" ]; then
      local count
      count=$(tail -n 1 "$log_file" | sed -E 's/.*positions ([0-9]+) .*/\1/')
      if [[ "$count" =~ ^[0-9]+$ ]]; then
        total=$((total + count))
      fi
    fi
  done
  echo "$total"
}

cd "$CHESS_DIRECTORY"
mkdir -p "$DATA_DIRECTORY" "$NETS_DIRECTORY"

announce "datagen: $TOTAL_POSITIONS positions, depth $DEPTH, $WORKERS workers, network $DATAGEN_NETWORK"
START_SECONDS=$(date +%s)
NNUE/datagen.sh "$WORKERS" "$POSITIONS_PER_WORKER" "$DEPTH" "$DATAGEN_NETWORK" &
DATAGEN_PROCESS=$!

while kill -0 "$DATAGEN_PROCESS" 2> /dev/null; do
  sleep 30
  DONE=$(positions_so_far)
  ELAPSED=$(( $(date +%s) - START_SECONDS ))
  RATE=0
  REMAINING_MINUTES="?"
  if [ "$ELAPSED" -gt 0 ]; then
    RATE=$((DONE / ELAPSED))
  fi
  if [ "$RATE" -gt 0 ]; then
    REMAINING_MINUTES=$(( (TOTAL_POSITIONS - DONE) / RATE / 60 ))
  fi
  PERCENT=$((DONE * 100 / TOTAL_POSITIONS))
  echo "[$(date '+%H:%M:%S')] $DONE / $TOTAL_POSITIONS positions ($PERCENT%)  $RATE positions/s  about $REMAINING_MINUTES min left"
done
wait "$DATAGEN_PROCESS"

announce "convert to bulletformat"
"$BULLET_UTILS" convert --from text --input "$DATA_DIRECTORY/combined.txt" --output "$DATA_DIRECTORY/combined.data"

announce "shuffle"
"$BULLET_UTILS" shuffle --input "$DATA_DIRECTORY/combined.data" --output "$DATA_DIRECTORY/train.data" --mem-used-mb 4096
"$BULLET_UTILS" validate --input "$DATA_DIRECTORY/train.data"

announce "train ($GPU_FEATURE)"
cd "$BULLET_DIRECTORY"
cargo run --release --example arpeggio --features "$GPU_FEATURE"

FINAL_CHECKPOINT=$(ls -d checkpoints/arpeggio-* | sort -t- -k2 -n | tail -n 1)
cp "$FINAL_CHECKPOINT/quantised.bin" "$NETS_DIRECTORY/$NET_NAME.bin"

announce "done: $NETS_DIRECTORY/$NET_NAME.bin ($(wc -c < "$NETS_DIRECTORY/$NET_NAME.bin") bytes)"
