#pragma once
#include "../Moves.h"
#include "../TextureManager.h"
#include <cstdint>

constexpr int INPUT_SIZE = 768;
constexpr int HL_SIZE = 128;
constexpr int QA = 255;
constexpr int QB = 64;
constexpr int SCALE = 400;

struct Network {
  int16_t accumulator_weights[INPUT_SIZE][HL_SIZE];
  int16_t accumulator_biases[HL_SIZE];
  int16_t output_weights[2 * HL_SIZE];
  int16_t output_bias;
};

extern Network network;

struct Accumulator {
  int16_t values[HL_SIZE];
};

struct AccumulatorPair {
  Accumulator white, black;
};

int featureIndex(int piece, int x, int y, int perspective);

void refreshAccumulator(const GameState &state, int perspective,
                        Accumulator &acc);

void updateAccumulator(const GameState &before, const GameState &after,
                       const AccumulatorPair &in, AccumulatorPair &out);

int evaluate(const Accumulator &us, const Accumulator &them);

// full eval of a position from side-to-move's POV
int nnueEval(const GameState &state);

bool loadNetwork(const char *path);
