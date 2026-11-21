#include "TranspositionTable.h"
#include "TextureManager.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

Table seeded;
TranspositionTable zobrist_TT;

// Zobrist Hashing. Credit to https://en.wikipedia.org/wiki/Zobrist_hashing, a
// fantastic guide and pseudocode.

static uint64_t pieceKey(const Table &seeded, int x, int y, int piece) {
  if (piece == EMPTY)
    return 0;
  return seeded.initTable[y * 8 + x][piece - 1];
}

// castling rights
static uint64_t castlingKey(const Table &seeded, int castling) {
  uint64_t key = 0;
  for (int i = 0; i < 4; ++i) {
    if (castling & (1 << i))
      key ^= seeded.castlingRights[i];
  }
  return key;
}

static uint64_t enPassantKey(const Table &seeded, sf::Vector2i epSquare) {
  if (epSquare == NO_SQUARE)
    return 0;
  return seeded.enPassantFile[epSquare.x];
}

Table::Table() {
  std::mt19937_64 rng(42);
  std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);

  for (int s = 0; s < NUM_SQUARES; ++s) {
    for (int p = 0; p < NUM_PIECES; ++p) {
      initTable[s][p] = dist(rng);
    }
  }
  isBlackTurn = dist(rng);
  for (int i = 0; i < 4; ++i) {
    castlingRights[i] = dist(rng);
  }
  for (int file = 0; file < 8; ++file) {
    enPassantFile[file] = dist(rng);
  }
}

uint64_t zobristHash(const GameState &state, const Table &seeded) {
  uint64_t hash = 0;
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      hash ^= pieceKey(seeded, x, y, state.board[y][x]);
    }
  }
  if (state.sideToMove == 1) // BLACK
    hash ^= seeded.isBlackTurn;
  hash ^= castlingKey(seeded, state.castling);
  hash ^= enPassantKey(seeded, state.epSquare);
  return hash;
}

uint64_t zobristHashMove(uint64_t hash, const GameState &before,
                         const Move &move, const GameState &after,
                         const Table &seeded) {
  if (after.sideToMove == before.sideToMove)
    return hash;

  int fx = move.from.x, fy = move.from.y;
  int tx = move.to.x, ty = move.to.y;
  int moved = before.board[fy][fx];
  int captured = before.board[ty][tx];
  int placed = after.board[ty][tx];

  hash ^= pieceKey(seeded, fx, fy, moved);
  hash ^= pieceKey(seeded, tx, ty, captured);
  hash ^= pieceKey(seeded, tx, ty, placed);

  if (isEnPassant(before, move)) {
    int capturedPawn = before.board[fy][tx];
    hash ^= pieceKey(seeded, tx, fy, capturedPawn);
  }

  // castling
  if ((moved == W_KING || moved == B_KING) && std::abs(tx - fx) == 2) {
    int rookFrom = (tx == 6) ? 7 : 0;
    int rookTo = (tx == 6) ? 5 : 3;
    int rook = before.board[fy][rookFrom];
    hash ^= pieceKey(seeded, rookFrom, fy, rook);
    hash ^= pieceKey(seeded, rookTo, fy, rook);
  }

  hash ^= castlingKey(seeded, before.castling ^ after.castling);
  hash ^= enPassantKey(seeded, before.epSquare);
  hash ^= enPassantKey(seeded, after.epSquare);
  hash ^= seeded.isBlackTurn;

  return hash;
}

void TranspositionTable::store(uint64_t hash, int32_t score, uint16_t bestMove,
                               uint8_t depth, uint8_t bound_type) {
  Entry entry = {hash, score, bestMove, depth, bound_type};
  data[hash & (data.size() - 1)] = entry;
}

void TranspositionTable::resize(size_t mb) {
  size_t count = (mb * 1024 * 1024) / sizeof(Entry);
  size_t pow2 = 1;
  while (pow2 * 2 <= count)
    pow2 *= 2;
  data.assign(pow2, Entry{});
}

void TranspositionTable::store(const Entry &entry) {
  data[entry.hash & (data.size() - 1)] = entry;
}

Entry *TranspositionTable::probe(uint64_t hash) {
  Entry &slot = data[hash & (data.size() - 1)];
  if (slot.hash != hash) {
    return nullptr;
  }
  return &slot;
}

void TranspositionTable::clear() {
  std::fill(data.begin(), data.end(), Entry{});
}
