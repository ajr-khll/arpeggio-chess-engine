#include "Moves.h"
#include "TextureManager.h"
#include "TranspositionTable.h"
#include "simulateMoves.h"

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>
#include <vector>

using std::vector;

enum colors { WHITE = 0, BLACK = 1 };

static inline bool inBounds(int x, int y) {
  return x >= 0 && x < 8 && y >= 0 && y < 8;
}

int colorOf(int piece) {
  if (piece == EMPTY)
    return -1;
  return (piece - 1) / 6;
}

static inline bool isEnemy(int piece, int other) {
  if (other == EMPTY)
    return false;
  return colorOf(piece) != colorOf(other);
}

bool isEnPassant(const GameState &state, const Move &move) {
  if (state.epSquare == NO_SQUARE || move.to != state.epSquare)
    return false;
  int ownPawn = (state.sideToMove == WHITE) ? W_PAWN : B_PAWN;
  return state.board[move.from.y][move.from.x] == ownPawn;
}

bool canCaptureEnPassant(const int (&board)[8][8], sf::Vector2i epSquare,
                         int capturer) {
  int pawnRank = (capturer == WHITE) ? epSquare.y + 1 : epSquare.y - 1;
  int capturingPawn = (capturer == WHITE) ? W_PAWN : B_PAWN;
  for (int dx : {-1, +1}) {
    int x = epSquare.x + dx;
    if (inBounds(x, pawnRank) && board[pawnRank][x] == capturingPawn)
      return true;
  }
  return false;
}

static inline int scoreMove(GameState &state, Move &move) {
  int victimPiece = isEnPassant(state, move) ? W_PAWN
                                             : state.board[move.to.y][move.to.x];
  int attacker = PIECE_VALUES[state.board[move.from.y][move.from.x]];
  int victim = PIECE_VALUES[victimPiece];
  // Most valuable victim, least valuable attacker.
  float score = (float)(victim * victim) * (1 / (float)attacker);
  return score;
}

static inline bool isFriend(int piece, int other) {
  if (other == EMPTY)
    return false;
  return colorOf(piece) == colorOf(other);
}

void startPos(GameState &state) {
  int board[8][8] = {
      {B_ROOK, B_KNIGHT, B_BISHOP, B_QUEEN, B_KING, B_BISHOP, B_KNIGHT,
       B_ROOK}, // Row 0 (Black Back Rank)
      {B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN,
       B_PAWN}, // Row 1 (Black Pawns)
      {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY}, // Row 2
      {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY}, // Row 3
      {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY}, // Row 4
      {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY}, // Row 5
      {W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN,
       W_PAWN}, // Row 6 (White Pawns)
      {W_ROOK, W_KNIGHT, W_BISHOP, W_QUEEN, W_KING, W_BISHOP, W_KNIGHT,
       W_ROOK} // Row 7 (White Back Rank)
  };

  std::copy(&board[0][0], &board[0][0] + 64,
            &state.board[0][0]); // assign start gamestate board using memory
                                 // copy (what a wonderful programming language
                                 // this is)
  state.sideToMove = WHITE;
  state.kingPos[WHITE] = {4, 7};
  state.kingPos[BLACK] = {4, 0};
  state.castling = W_KINGSIDE | W_QUEENSIDE | B_KINGSIDE | B_QUEENSIDE;
  state.setHash();
}

bool isSquareAttacked(int (&BOARD)[8][8], sf::Vector2i target,
                      int attackerColor);

static void addSlidingMoves(std::vector<Move> &out, int piece, sf::Vector2i pos,
                            int (&BOARD)[8][8],
                            const std::vector<sf::Vector2i> &directions) {
  for (auto d : directions) {
    int x = pos.x + d.x;
    int y = pos.y + d.y;

    while (inBounds(x, y)) {
      int cell = BOARD[y][x];

      if (cell == EMPTY) {
        out.push_back(Move{pos, {x, y}});
      } else {
        if (isEnemy(piece, cell))
          out.push_back(Move{pos, {x, y}});
        break; // blocked
      }

      x += d.x;
      y += d.y;
    }
  }
}

std::vector<Move> removeIllegalMoves(int piece, std::vector<Move> moves,
                                     GameState current) {
  std::vector<Move> validMoves;

  const int mover = current.sideToMove; // side making the move right now

  for (const Move &move : moves) {
    GameState simulated = simulateMove(current, move);

    if (!isInCheck(simulated, mover)) {
      validMoves.push_back(move);
    }
  }
  return validMoves;
}

std::vector<Move> calculatePossibleMoves(int piece, sf::Vector2i position,
                                         GameState current, bool capturesOnly,
                                         bool includeChecks) {
  std::vector<Move> moves;

  if (piece == EMPTY)
    return moves;
  if (!inBounds(position.x, position.y))
    return moves;

  const int myColor = colorOf(piece);
  const bool isWhite = (myColor == 0);
  const bool isBlack = (myColor == 1);

  // ---------------- Pawns ----------------
  if (piece == W_PAWN || piece == B_PAWN) {
    const int dir = isWhite ? -1 : +1;
    const int startRank = isWhite ? 6 : 1;

    const int x = position.x;
    const int y = position.y;

    // reaching the last rank generates one move per promotion piece
    const int promoRank = isWhite ? 0 : 7;
    const int promoPieces[4] = {
        isWhite ? W_QUEEN : B_QUEEN, isWhite ? W_ROOK : B_ROOK,
        isWhite ? W_BISHOP : B_BISHOP, isWhite ? W_KNIGHT : B_KNIGHT};
    auto addPawnMove = [&](sf::Vector2i to) {
      if (to.y == promoRank) {
        for (int p : promoPieces) {
          moves.push_back(Move{{x, y}, to, p});
        }
      } else {
        moves.push_back(Move{{x, y}, to});
      }
    };

    // 1 step forward
    int y1 = y + dir;
    if (inBounds(x, y1) && current.board[y1][x] == EMPTY) {
      addPawnMove({x, y1});

      // 2 steps forward
      int y2 = y + 2 * dir;
      if (y == startRank && inBounds(x, y2) && current.board[y2][x] == EMPTY) {
        moves.push_back(Move{{x, y}, {x, y2}});
      }
    }

    for (int dx : {-1, +1}) {
      int cx = x + dx;
      int cy = y + dir;
      if (!inBounds(cx, cy))
        continue;

      bool capturesEnemy = isEnemy(piece, current.board[cy][cx]);
      bool capturesEnPassant = isEnPassant(current, Move{{x, y}, {cx, cy}});
      if (capturesEnemy || capturesEnPassant) {
        addPawnMove({cx, cy});
      }
    }
  }

  // ---------------- Knights ----------------
  if (piece == W_KNIGHT || piece == B_KNIGHT) {
    static const std::array<sf::Vector2i, 8> jumps = {{{+1, +2},
                                                       {+2, +1},
                                                       {+2, -1},
                                                       {+1, -2},
                                                       {-1, -2},
                                                       {-2, -1},
                                                       {-2, +1},
                                                       {-1, +2}}};

    for (auto j : jumps) {
      int nx = position.x + j.x;
      int ny = position.y + j.y;
      if (!inBounds(nx, ny))
        continue;

      int cell = current.board[ny][nx];
      if (cell == EMPTY || isEnemy(piece, cell)) {
        moves.push_back(Move{position, {nx, ny}});
      }
    }
  }

  // ---------------- King ----------------
  if (piece == W_KING || piece == B_KING) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0)
          continue;
        int nx = position.x + dx;
        int ny = position.y + dy;
        if (!inBounds(nx, ny))
          continue;

        int cell = current.board[ny][nx];
        if (cell == EMPTY || isEnemy(piece, cell)) {
          moves.push_back(Move{position, {nx, ny}});
        }
      }
    }

    // castling: king can't be in check or pass through an attacked square.
    // landing in check is filtered out by removeIllegalMoves.
    const int homeRank = isWhite ? 7 : 0;
    const int enemy = myColor ^ 1;
    const int ownRook = isWhite ? W_ROOK : B_ROOK;
    const int kingSide = isWhite ? W_KINGSIDE : B_KINGSIDE;
    const int queenSide = isWhite ? W_QUEENSIDE : B_QUEENSIDE;
    int (&b)[8][8] = current.board;

    if (position.x == 4 && position.y == homeRank &&
        !isSquareAttacked(b, position, enemy)) {
      if ((current.castling & kingSide) && b[homeRank][7] == ownRook &&
          b[homeRank][5] == EMPTY && b[homeRank][6] == EMPTY &&
          !isSquareAttacked(b, {5, homeRank}, enemy)) {
        moves.push_back(Move{position, {6, homeRank}});
      }
      if ((current.castling & queenSide) && b[homeRank][0] == ownRook &&
          b[homeRank][1] == EMPTY && b[homeRank][2] == EMPTY &&
          b[homeRank][3] == EMPTY &&
          !isSquareAttacked(b, {3, homeRank}, enemy)) {
        moves.push_back(Move{position, {2, homeRank}});
      }
    }
  }

  static const std::vector<sf::Vector2i> rookDirs = {
      {+1, 0}, {-1, 0}, {0, +1}, {0, -1}};
  static const std::vector<sf::Vector2i> bishopDirs = {
      {+1, +1}, {+1, -1}, {-1, +1}, {-1, -1}};

  if (piece == W_ROOK || piece == B_ROOK) {
    addSlidingMoves(moves, piece, position, current.board, rookDirs);
  }

  if (piece == W_BISHOP || piece == B_BISHOP) {
    addSlidingMoves(moves, piece, position, current.board, bishopDirs);
  }

  if (piece == W_QUEEN || piece == B_QUEEN) {
    std::vector<sf::Vector2i> queenDirs = rookDirs;
    queenDirs.insert(queenDirs.end(), bishopDirs.begin(), bishopDirs.end());
    addSlidingMoves(moves, piece, position, current.board, queenDirs);
  }

  if (capturesOnly) {
    std::vector<Move> captures;

    for (const Move &move : moves) {
      int target = current.board[move.to.y][move.to.x];
      bool isCapture = (target != EMPTY) || isEnPassant(current, move);

      bool givesCheck = false;
      if (includeChecks && !isCapture) {
        GameState after = makeMove(current, move);
        int opponent = colorOf(piece) ^ 1;
        givesCheck = isInCheck(after, opponent);
      }

      if (isCapture || givesCheck) {
        captures.push_back(move);
      }
    }

    moves = captures;
  }
  std::vector<Move> validMoves = removeIllegalMoves(piece, moves, current);

  for (Move &move : validMoves) {
    move.ranking = scoreMove(current, move);
  }

  return validMoves;
}

bool isSquareAttacked(int (&BOARD)[8][8], sf::Vector2i target,
                      int attackerColor) {
  const int tx = target.x;
  const int ty = target.y;
  if (!inBounds(tx, ty))
    return false;
  // pawns!!
  if (attackerColor == WHITE) {
    int py = ty + 1;
    if (inBounds(tx - 1, py) && BOARD[py][tx - 1] == W_PAWN) {
      return true;
    }
    if (inBounds(tx + 1, py) && BOARD[py][tx + 1] == W_PAWN) {
      return true;
    }
  } else if (attackerColor == BLACK) {
    int py = ty - 1;
    if (inBounds(tx - 1, py) && BOARD[py][tx - 1] == B_PAWN) {
      return true;
    }
    if (inBounds(tx + 1, py) && BOARD[py][tx + 1] == B_PAWN) {
      return true;
    }
  }

  // knights!!
  static const std::array<sf::Vector2i, 8> knightD = {{{+1, +2},
                                                       {+2, +1},
                                                       {+2, -1},
                                                       {+1, -2},
                                                       {-1, -2},
                                                       {-2, -1},
                                                       {-2, +1},
                                                       {-1, +2}}};
  const int attackerKnight =
      (attackerColor == WHITE)
          ? W_KNIGHT
          : B_KNIGHT; // pick out which knight we care about
  for (auto d : knightD) {
    if (inBounds(tx + d.x, ty + d.y) &&
        BOARD[ty + d.y][tx + d.x] == attackerKnight) {
      return true;
    }
  }

  const int attackerKing = (attackerColor == WHITE) ? W_KING : B_KING;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0)
        continue;
      int x = tx + dx, y = ty + dy;
      if (inBounds(x, y) && BOARD[y][x] == attackerKing) {
        return true;
      }
    }
  }

  static const std::array<sf::Vector2i, 4> rookDirs = {
      {{+1, 0}, {-1, 0}, {0, +1}, {0, -1}}};
  static const std::array<sf::Vector2i, 4> bishopDirs = {
      {{+1, +1}, {+1, -1}, {-1, +1}, {-1, -1}}};

  const int attackerRook = (attackerColor == WHITE) ? W_ROOK : B_ROOK;
  const int attackerBish = (attackerColor == WHITE) ? W_BISHOP : B_BISHOP;
  const int attackerQueen = (attackerColor == WHITE) ? W_QUEEN : B_QUEEN;

  for (auto d : rookDirs) {
    int x = tx + d.x, y = ty + d.y;
    while (inBounds(x, y)) {
      int p = BOARD[y][x];
      if (p != EMPTY) {
        if (p == attackerRook || p == attackerQueen) {
          return true;
        }
        break; // blocked by something else
      }
      x += d.x;
      y += d.y;
    }
  }

  for (auto d : bishopDirs) {
    int x = tx + d.x, y = ty + d.y;
    while (inBounds(x, y)) {
      int p = BOARD[y][x];
      if (p != EMPTY) {
        if (p == attackerBish || p == attackerQueen) {
          return true;
        }
        break;
      }
      x += d.x;
      y += d.y;
    }
  }

  return false;
}

vector<Move> gatherMoves(GameState state, int side, bool capturesOnly,
                         bool includeChecks) {
  std::vector<Move> moves;
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      int piece = state.board[y][x];
      if (colorOf(piece) == side) {
        std::vector<Move> pieceMoves = calculatePossibleMoves(
            piece, {x, y}, state, capturesOnly, includeChecks);
        moves.insert(moves.end(), pieceMoves.begin(), pieceMoves.end());
      }
    }
  }
  return moves;
}

vector<Move> gatherCaptures(GameState state) {
  std::vector<Move>
      moves; // incomplete. not sure how to implement yet, to use MVV-LUA }
};
bool isInCheck(GameState state, int color) {
  int enemyColor = color ^ 1;
  return isSquareAttacked(state.board, state.kingPos[color], enemyColor);
}

bool isCheckmated(GameState state, int color) {
  if (gatherMoves(state, color).empty() && isInCheck(state, color)) {
    return true;
  } else {
    return false;
  }
}

GameState makeMove(const GameState &current, const Move &move,
                   const std::optional<std::vector<Move>> &moves) {
  GameState before = current;
  sf::Vector2i SELECTED = move.from;
  sf::Vector2i coordinate = move.to;

  auto pred = [&](const Move &m) {
    return m.from == SELECTED && m.to == coordinate;
  };

  bool ok = true;
  if (moves) {
    ok = (std::find_if(moves->begin(), moves->end(), pred) != moves->end());
  }

  int BOARD[8][8];
  std::copy(&current.board[0][0], &current.board[0][0] + 64, &BOARD[0][0]);

  sf::Vector2i whiteKingPos = current.kingPos[0];
  sf::Vector2i blackKingPos = current.kingPos[1];
  int turn = current.sideToMove;
  int castling = current.castling;
  int halfmoveClock = current.halfmoveClock;
  sf::Vector2i epSquare = current.epSquare;

  if (ok) {
    int piece = BOARD[SELECTED.y][SELECTED.x];

    bool pawnMove = (piece == W_PAWN || piece == B_PAWN);
    bool capture = (BOARD[coordinate.y][coordinate.x] != EMPTY);
    halfmoveClock = (pawnMove || capture) ? 0 : halfmoveClock + 1;

    if (piece == B_KING)
      blackKingPos = {coordinate.x, coordinate.y};
    if (piece == W_KING)
      whiteKingPos = {coordinate.x, coordinate.y};

    // promotion: use the requested piece, default to queen (e.g. GUI clicks)
    if ((piece == W_PAWN && coordinate.y == 0) ||
        (piece == B_PAWN && coordinate.y == 7)) {
      if (move.promotion != EMPTY)
        piece = move.promotion;
      else
        piece = (piece == W_PAWN) ? W_QUEEN : B_QUEEN;
    }

    if (isEnPassant(current, move))
      BOARD[SELECTED.y][coordinate.x] = EMPTY;

    BOARD[SELECTED.y][SELECTED.x] = EMPTY;
    BOARD[coordinate.y][coordinate.x] = piece;

    // castling: king moving two squares also moves the rook
    if ((piece == W_KING || piece == B_KING) &&
        std::abs(coordinate.x - SELECTED.x) == 2) {
      int rank = SELECTED.y;
      if (coordinate.x == 6) {
        BOARD[rank][5] = BOARD[rank][7];
        BOARD[rank][7] = EMPTY;
      } else {
        BOARD[rank][3] = BOARD[rank][0];
        BOARD[rank][0] = EMPTY;
      }
    }

    // update castling rights: king moved, or a rook moved / was captured
    if (piece == W_KING)
      castling &= ~(W_KINGSIDE | W_QUEENSIDE);
    if (piece == B_KING)
      castling &= ~(B_KINGSIDE | B_QUEENSIDE);
    for (sf::Vector2i sq : {SELECTED, coordinate}) {
      if (sq == sf::Vector2i{7, 7})
        castling &= ~W_KINGSIDE;
      if (sq == sf::Vector2i{0, 7})
        castling &= ~W_QUEENSIDE;
      if (sq == sf::Vector2i{7, 0})
        castling &= ~B_KINGSIDE;
      if (sq == sf::Vector2i{0, 0})
        castling &= ~B_QUEENSIDE;
    }

    turn ^= 1;

    epSquare = NO_SQUARE;
    bool doublePush = pawnMove && std::abs(coordinate.y - SELECTED.y) == 2;
    if (doublePush) {
      sf::Vector2i skipped = {SELECTED.x, (SELECTED.y + coordinate.y) / 2};
      if (canCaptureEnPassant(BOARD, skipped, turn))
        epSquare = skipped;
    }
  }

  GameState newState;
  std::copy(&BOARD[0][0], &BOARD[0][0] + 64, &newState.board[0][0]);
  newState.sideToMove = turn;
  newState.kingPos[WHITE] = whiteKingPos;
  newState.kingPos[BLACK] = blackKingPos;
  newState.castling = castling;
  newState.epSquare = epSquare;
  newState.halfmoveClock = halfmoveClock;

  newState.hash = zobristHashMove(before.hash, before, move, newState, seeded);

  return newState;
}

void GameState::setHash() { hash = zobristHash(*this, seeded); }
