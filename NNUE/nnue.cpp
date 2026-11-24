#include "nnue.h"
#include "../Moves.h"
#include <algorithm>
#include <cstdint>

// Maps this engine's piece IDs (Moves.h / TextureManager.h order) to the
// trainer's conventions. Index directly with the piece value.

// trainer piece order: P=0, N=1, B=2, R=3, Q=4, K=5
static constexpr int PIECE_TYPE[13] = {
    -1,                // EMPTY (never valid)
    0,  3, 1, 2, 4, 5, // W_PAWN, W_ROOK, W_KNIGHT, W_BISHOP, W_QUEEN, W_KING
    0,  3, 1, 2, 4, 5  // B_PAWN, B_ROOK, B_KNIGHT, B_BISHOP, B_QUEEN, B_KING
};

// WHITE = 0, BLACK = 1
static constexpr int PIECE_COLOUR[13] = {
    -1,                // EMPTY (never valid)
    0,  0, 0, 0, 0, 0, // white pieces
    1,  1, 1, 1, 1, 1  // black pieces
};

Network network;

int featureIndex(int piece, int x, int y, int perspective) {
  int square = (7 - y) * 8 + x;
  int type = PIECE_TYPE[piece];
  int colour = PIECE_COLOUR[piece];

  if (perspective == 0) {
    // White perspective
    return colour * 384 + type * 64 + square;
  } else {
    return (colour ^ 1) * 384 + type * 64 + (square ^ 56);
  }
}

void refreshAccumulator(const GameState &state, int perspective,
                        Accumulator &acc) {
  for (int i = 0; i < HL_SIZE; ++i) {
    acc.values[i] = network.accumulator_biases[i];
  } // copy biases over

  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      if (state.board[y][x] != EMPTY) {
        int index = featureIndex(state.board[y][x], x, y, perspective);
        for (int h = 0; h < HL_SIZE; h++) {
          acc.values[h] += network.accumulator_weights[index][h];
        }
      }
    }
  }
}

int32_t screlu(int16_t x) {
  int32_t clamped_x = std::clamp((int32_t)x, 0, QA);
  return clamped_x * clamped_x;
}

int evaluate(const Accumulator &us, const Accumulator &them) {
  int64_t sum = 0;
  for (int i = 0; i < HL_SIZE; i++) {
    sum += screlu(us.values[i]) * network.output_weights[i];
    sum += screlu(them.values[i]) * network.output_weights[HL_SIZE + i];
  }
  sum /= QA;
  sum += network.output_bias;
  return sum * SCALE / (QA * QB);
}

int nnueEval(const GameState &state) {
  Accumulator white;
  Accumulator black;
  refreshAccumulator(state, 0, white);
  refreshAccumulator(state, 1, black);

  if (state.sideToMove == 0) { // white
    return evaluate(white, black);
  } else {
    return evaluate(black, white);
  }
}

void addFeature(AccumulatorPair &ap, int piece, int x, int y) {
  int index0 = featureIndex(piece, x, y, 0);
  int index1 = featureIndex(piece, x, y, 1);
  for (int h = 0; h < HL_SIZE; h++) {
    ap.white.values[h] += network.accumulator_weights[index0][h];
    ap.black.values[h] += network.accumulator_weights[index1][h];
  }
}

void subFeature(AccumulatorPair &ap, int piece, int x, int y) {
  int index0 = featureIndex(piece, x, y, 0);
  int index1 = featureIndex(piece, x, y, 1);
  for (int h = 0; h < HL_SIZE; h++) {
    ap.white.values[h] -= network.accumulator_weights[index0][h];
    ap.black.values[h] -= network.accumulator_weights[index1][h];
  }
}

void updateAccumulator(const GameState &before, const GameState &after,
                       const AccumulatorPair &in, AccumulatorPair &out) {
  out = in;
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      if (before.board[y][x] != after.board[y][x]) {
        if (before.board[y][x] != EMPTY) {
          subFeature(out, before.board[y][x], x, y);
        };
        if (after.board[y][x] != EMPTY) {
          addFeature(out, after.board[y][x], x, y);
        };
      }
    }
  }
}

bool loadNetwork(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == nullptr) {
    return false;
  }
  size_t itemsRead = fread(&network, sizeof(network), 1, file);
  fclose(file);
  return itemsRead == 1;
}
