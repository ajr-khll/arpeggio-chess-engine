#pragma once
#include <array>
#include <cstdint>
#include <random>
#include <vector>

#include "Moves.h"

constexpr int NUM_SQUARES = 64;
constexpr int NUM_PIECES = 12; // W_PAWN..B_KING; EMPTY is not hashed

enum BOUND {
  EXACT = 0,
  LOWER = 1,
  UPPER = 2,
};

struct Entry {
  uint64_t hash;
  int32_t score;
  uint16_t bestMove;
  uint8_t depth;
  uint8_t bound_type;
};

struct TranspositionTable {
  std::vector<Entry> data;

  TranspositionTable() { resize(64); }
  void resize(size_t mb);
  void clear();
  void store(uint64_t hash, int32_t score, uint16_t bestMove, uint8_t depth,
             uint8_t bound_type);
  void store(const Entry &entry);
  Entry *probe(uint64_t hash);
};

extern TranspositionTable zobrist_TT;

struct Table {
  std::array<std::array<uint64_t, NUM_PIECES>, NUM_SQUARES> initTable;
  uint64_t isBlackTurn;
  std::array<uint64_t, 4> castlingRights;
  std::array<uint64_t, 8> enPassantFile;
  Table();
};

extern Table seeded;

// Full hash of a position, computed from scratch.
uint64_t zobristHash(const GameState &state, const Table &seeded);

// Gives the same result as zobristHash(after, seeded).
uint64_t zobristHashMove(uint64_t hash, const GameState &before,
                         const Move &move, const GameState &after,
                         const Table &seeded);
