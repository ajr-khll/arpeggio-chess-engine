#include "nnue.h"
#include <iostream>
#include <random>

void randomNetwork(unsigned int seed) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<> dist(-50, 50);
  for (int f = 0; f < INPUT_SIZE; f++) {
    for (int h = 0; h < HL_SIZE; h++) {
      network.accumulator_weights[f][h] = dist(gen);
      if (f == 0)
        network.accumulator_biases[h] = dist(gen);
      if (f == 0 || f == 1) {
        network.output_weights[h] = dist(gen);
      }
    }
  }
  network.output_bias = dist(gen);
}

int swapColour(int piece) {
  if (piece == EMPTY)
    return EMPTY;
  return piece <= 6 ? piece + 6 : piece - 6;
}

GameState mirror(const GameState &state) {
  GameState out;
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      out.board[7 - y][x] = swapColour(state.board[y][x]);
    }
  }
  out.sideToMove = state.sideToMove ^ 1;
  return out;
}

int main() {
  randomNetwork(42);
  std::cout << featureIndex(W_KNIGHT, 6, 7, 0) << std::endl;
  std::cout << featureIndex(W_KNIGHT, 6, 7, 1) << std::endl;
  GameState state;
  startPos(state);
  std::cout << "nnueEval: " << nnueEval(state) << std::endl;
  state.sideToMove = 1;
  std::cout << "nnueEval flipped: " << nnueEval(state) << std::endl;

  state.board[7][1] = EMPTY;
  std::cout << "nnueEval sans knight" << nnueEval(state) << std::endl;

  int a = nnueEval(state), b = nnueEval(mirror(state));
  std::cout << "mirror test: " << a << " vs " << b
            << (a == b ? "  PASS" : "  FAIL") << std::endl;
}
