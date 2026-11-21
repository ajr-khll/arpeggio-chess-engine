#pragma once
#include <SFML/System/Vector2.hpp>
#include <cstdint>
#include <optional>
#include <vector>

static const int PIECE_VALUES[13] = {
    0,     // EMPTY
    100,   // W_PAWN
    500,   // W_ROOK
    320,   // W_KNIGHT
    330,   // W_BISHOP
    900,   // W_QUEEN
    20000, // W_KING (Arbitrarily high so the engine never sacrifices it)
    100,   // B_PAWN
    500,   // B_ROOK
    320,   // B_KNIGHT
    330,   // B_BISHOP
    900,   // B_QUEEN
    20000  // B_KING
};

struct Move {
  sf::Vector2i from;
  sf::Vector2i to;
  int promotion = 0; // piece to promote to (e.g. W_QUEEN), 0 if none
  int ranking = 0;   // move-ordering score, not part of move identity

  // promotion stays 3rd so existing {from, to, promotion} inits keep meaning
  Move(sf::Vector2i from = {}, sf::Vector2i to = {}, int promotion = 0,
       int ranking = 0)
      : from(from), to(to), promotion(promotion), ranking(ranking) {}
};

inline bool operator==(const Move &a, const Move &b) {
  return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
}

// castling rights, stored as bit flags in GameState::castling
enum CastlingRights {
  W_KINGSIDE = 1,
  W_QUEENSIDE = 2,
  B_KINGSIDE = 4,
  B_QUEENSIDE = 8
};

struct Undo {
  Move move;
  int movedPiece;
  int capturedPiece;
  sf::Vector2i kingPosBefore[2];
};

inline const sf::Vector2i NO_SQUARE{-1, -1};

struct GameState {
  int board[8][8];
  int sideToMove;
  sf::Vector2i kingPos[2];
  int castling = 0;
  sf::Vector2i epSquare = NO_SQUARE;
  int halfmoveClock = 0;
  uint64_t hash = 0;
  void setHash();
};

void startPos(GameState &state);

std::vector<Move> calculatePossibleMoves(int piece, sf::Vector2i position,
                                         GameState game,
                                         bool capturesOnly = false,
                                         bool includeChecks = false);

std::vector<Move> gatherMoves(GameState game, int side,
                              bool capturesOnly = false,
                              bool includeChecks = false);

bool isInCheck(GameState current, int color);

bool isCheckmated(GameState state, int side);

int colorOf(int piece);

bool isEnPassant(const GameState &state, const Move &move);

bool canCaptureEnPassant(const int (&board)[8][8], sf::Vector2i epSquare,
                         int capturer);

GameState
makeMove(const GameState &current, const Move &move,
         const std::optional<std::vector<Move>> &moves = std::nullopt);
