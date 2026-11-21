#include "minimax.h"
#include "Moves.h"
#include "TextureManager.h"
#include "TranspositionTable.h"
#include "simulateMoves.h"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>

// Array for values of pieces
// 0: Empty
// 1-6:  W_PAWN, W_ROOK, W_KNIGHT, W_BISHOP, W_QUEEN, W_KING
// 7-12: B_PAWN, B_ROOK, B_KNIGHT, B_BISHOP, B_QUEEN, B_KING
// (must match the PieceIDs order in TextureManager.h)

enum colors { WHITE = 0, BLACK = 1 };

// --------------------------------------------------------------------------
// PIECE-SQUARE TABLES (White's Perspective)
// --------------------------------------------------------------------------
// These are flat arrays of 64 integers.
// Access them using: table[y * 8 + x]

// PAWNS: Encourage moving forward and controlling the center (d4/e4).
static const int mvv_luv[64] = {
    0,   0,   0,   0,
    0,   0,   0,   0, // Rank 8 (Promoted - usually irrelevant here)
    50,  50,  50,  50,
    50,  50,  50,  50, // Rank 7 (Almost promoted!)
    10,  10,  20,  30,
    30,  20,  10,  10, // Rank 6
    5,   5,   10,  25,
    25,  10,  5,   5, // Rank 5
    0,   0,   0,   20,
    20,  0,   0,   0, // Rank 4
    5,   -5,  -10, 0,
    0,   -10, -5,  5, // Rank 3
    5,   10,  10,  -20,
    -20, 10,  10,  5, // Rank 2 (Don't move f/g/h pawns too early)
    0,   0,   0,   0,
    0,   0,   0,   0 // Rank 1 (Base)
};

// KNIGHTS: Strong in the center, terrible at the edges/corners.
static const int knight_pst[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,   0,   0,
    0,   -20, -40, -30, 0,   10,  15,  15,  10,  0,   -30, -30, 5,
    15,  20,  20,  15,  5,   -30, -30, 0,   15,  20,  20,  15,  0,
    -30, -30, 5,   10,  15,  15,  10,  5,   -30, -40, -20, 0,   5,
    5,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50};

// BISHOPS: Good on long diagonals, better in center than corners.
static const int bishop_pst[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20, -10, 0,   0,   0,   0,
    0,   0,   -10, -10, 0,   5,   10,  10,  5,   0,   -10, -10, 5,
    5,   10,  10,  5,   5,   -10, -10, 0,   10,  10,  10,  10,  0,
    -10, -10, 10,  10,  10,  10,  10,  10,  -10, -10, 5,   0,   0,
    0,   0,   5,   -10, -20, -10, -10, -10, -10, -10, -10, -20};

// ROOKS: Bonus for 7th rank (attacking enemy pawns) and centering.
static const int rook_pst[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 5, 10, 10, 10, 10, 10, 10, 5, // 7th Rank (Pig
                                                          // on the 7th)
    -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0,
    -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 5, 5, 0, 0,
    0 // Castling squares
      // slightly better
};

// QUEENS: Generally kept simple. Avoid corners, stay somewhat central.
static const int queen_pst[64] = {
    -20, -10, -10, -5, -5, -10, -10, -20, -10, 0,   0,   0,  0,  0,   0,   -10,
    -10, 0,   5,   5,  5,  5,   0,   -10, -5,  0,   5,   5,  5,  5,   0,   -5,
    0,   0,   5,   5,  5,  5,   0,   -5,  -10, 5,   5,   5,  5,  5,   0,   -10,
    -10, 0,   5,   0,  0,  0,   0,   -10, -20, -10, -10, -5, -5, -10, -10, -20};

// KINGS (MIDDLE GAME): Highly penalized for being in the center or open files.
// Encourages castling into the corners (g1/b1).
static const int king_pst[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40,
    -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40,
    -40, -30, -20, -30, -30, -40, -40, -30, -30, -20, -10, -20, -20, -20, -20,
    -20, -20, -10, 20,  20,  0,   0,   0,   0,   20,  20, // Pawn shield rank
    20,  30,  10,  0,   0,   10,  30,  20 // Back rank (Corners are safest)
};

int positionalScore(int piece, int x, int y) {
  if (piece == EMPTY)
    return 0;

  const int *table = nullptr;
  if (piece == W_PAWN || piece == B_PAWN)
    table = mvv_luv;
  else if (piece == W_KNIGHT || piece == B_KNIGHT)
    table = knight_pst;
  else if (piece == W_BISHOP || piece == B_BISHOP)
    table = bishop_pst;
  else if (piece == W_ROOK || piece == B_ROOK)
    table = rook_pst;
  else if (piece == W_QUEEN || piece == B_QUEEN)
    table = queen_pst;
  else if (piece == W_KING || piece == B_KING)
    table = king_pst;

  if (!table)
    return 0;

  if (colorOf(piece) == WHITE) {
    return table[y * 8 + x];
  } else {
    return table[(7 - y) * 8 + x];
  }
}

static const int king_edge_bonus[64] = {
    60, 50, 40, 30, 30, 40, 50, 60, 50, 40, 30, 20, 20, 30, 40, 50,
    40, 30, 20, 10, 10, 20, 30, 40, 30, 20, 10, 0,  0,  10, 20, 30,
    30, 20, 10, 0,  0,  10, 20, 30, 40, 30, 20, 10, 10, 20, 30, 40,
    50, 40, 30, 20, 20, 30, 40, 50, 60, 50, 40, 30, 30, 40, 50, 60};

static const int STARTING_MATERIAL = 8000;

static double endgameWeight(const GameState &state) {
  int material = 0;
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      int piece = state.board[y][x];
      if (piece != EMPTY && piece != W_KING && piece != B_KING)
        material += PIECE_VALUES[piece];
    }
  }
  double weight = 1.0 - static_cast<double>(material) / STARTING_MATERIAL;
  return std::clamp(weight, 0.0, 1.0);
}

static int kingSquareScore(int piece, int x, int y, double endgame) {
  int middlegame = positionalScore(piece, x, y);
  int edge = king_edge_bonus[y * 8 + x];
  return static_cast<int>((1.0 - endgame) * middlegame - endgame * edge);
}

static int kingDistance(const GameState &state) {
  sf::Vector2i white = state.kingPos[WHITE];
  sf::Vector2i black = state.kingPos[BLACK];
  return std::abs(white.x - black.x) + std::abs(white.y - black.y);
}

int evaluateScore(GameState state, int color) {
  // Given a game state, return an integer value for the score of the game
  double endgame = endgameWeight(state);
  int score = 0;
  int materialLead = 0;
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      int piece = state.board[y][x];
      if (piece == EMPTY)
        continue;

      int value = PIECE_VALUES[piece];
      if (piece == W_KING || piece == B_KING)
        value += kingSquareScore(piece, x, y, endgame);
      else
        value += positionalScore(piece, x, y);

      if (colorOf(piece) == color) {
        score += value;
        materialLead += PIECE_VALUES[piece];
      } else {
        score -= value;
        materialLead -= PIECE_VALUES[piece];
      }
    }
  }

  if (materialLead != 0) {
    int closeness = 14 - kingDistance(state);
    int bonus = static_cast<int>(endgame * closeness * 4);
    score += (materialLead > 0) ? bonus : -bonus;
  }
  return score * (200 - state.halfmoveClock) / 200;
}

static void selectNextMove(std::vector<Move> &moves, int position) {
  // swap best ranked move into position
  int bestRankPos = position;
  for (size_t i = position; i < moves.size(); i++) {
    if (moves[i].ranking >= moves[bestRankPos].ranking) {
      bestRankPos = i;
    }
  }
  std::swap(moves[position], moves[bestRankPos]);
}

// Minimax algorithm, recursive
const int CAP = 5; // think no more than 5 moves ahead

evaluatedMove Minimax(GameState state, int side, int alpha, int beta,
                      Move lastMove, int depth) {
  // base case
  if (depth == CAP) {
    int score = evaluateScore(state, BLACK);
    if (score > alpha) {
      alpha = score;
    }
    return {lastMove, score};
  }
  Move nullMove = Move({-1, -1}, {-1, -1}, 0);
  evaluatedMove bestMove{nullMove};
  if (side == BLACK)
    bestMove.score = -999999;
  else
    bestMove.score = 999999;
  // recursive case. keep growing tree until depth == 4
  // first, get every possible move
  std::vector<Move> moves = gatherMoves(state, side);
  if (moves.empty()) {
    if (isInCheck(state, side)) {
      // If White is mated, return a very high number (but slightly lower if it
      // takes longer) If Black is mated, return a very low number (but
      // slightly higher if it takes longer)
      return {{{-1, -1}, {-1, -1}},
              (side == WHITE) ? 1000000 + depth : -1000000 - depth};
    } else {
      return {{{-1, -1}, {-1, -1}}, 0};
    }
  }
  // Now moves is a list of all possible moves we can make in our current
  // state.
  int enemy = !side;
  for (Move move : moves) {
    evaluatedMove result =
        Minimax(simulateMove(state, move), enemy, alpha, beta, move, depth + 1);
    if (side == BLACK) {
      if (result.score > alpha) {
        alpha = result.score;
      }
      if (result.score > bestMove.score) {
        bestMove = {move, result.score};
      }
      if (alpha >= beta) {
        break;
      }
    } else {
      if (result.score < beta) {
        beta = result.score;
      }
      if (result.score < bestMove.score) {
        bestMove = {move, result.score};
      }
      if (beta <= alpha) {
        break;
      }
    }
  }

  return bestMove;
}

std::atomic_bool stop_search{false};
std::chrono::steady_clock::time_point deadline;
std::vector<uint64_t> positionHistory;
static uint64_t nodeCount = 0;

static bool isRepetition(uint64_t hash, int halfmoveClock) {
  int last = static_cast<int>(positionHistory.size()) - 1;
  int oldest = std::max(0, last - halfmoveClock);
  for (int i = last - 2; i >= oldest; i -= 2) {
    if (positionHistory[i] == hash)
      return true;
  }
  return false;
}

// allow us to check if time is up every 2048 nodes.
static inline bool isTimeUp() {
  if ((nodeCount & 2047) == 0 && std::chrono::steady_clock::now() > deadline)
    stop_search = true;
  return stop_search;
}

// follow all capture-to-capture paths on the board to find any potential
// blunders.
int quiescenceSearch(GameState state, int alpha, int beta, int qDepth = 0) {
  ++nodeCount;
  bool black = state.sideToMove == BLACK;
  bool inCheck = isInCheck(state, state.sideToMove);

  int best;
  std::vector<Move> moves;
  if (isTimeUp()) {
    return 0;
  }
  if (inCheck) {
    moves = gatherMoves(state, state.sideToMove);
    if (moves.empty()) {
      return black ? -1000000 : 1000000;
    }
    best = black ? -10000000 : 10000000;
  } else {
    int standPat = evaluateScore(state, BLACK);

    if (black) {
      if (standPat >= beta)
        return standPat;
      if (standPat > alpha)
        alpha = standPat;
    } else {
      if (standPat <= alpha)
        return standPat;
      if (standPat < beta)
        beta = standPat;
    }

    best = standPat;
    bool includeChecks = (qDepth == 0);
    moves = gatherMoves(state, state.sideToMove, true, includeChecks);
  }

  for (size_t i = 0; i < moves.size(); i++) {
    selectNextMove(moves, i);
    Move move = moves[i];

    int score =
        quiescenceSearch(simulateMove(state, move), alpha, beta, qDepth + 1);
    if (isTimeUp())
      return 0;
    if (black) {
      if (score > best)
        best = score;
      if (score > alpha)
        alpha = score;
    } else {
      if (score < best)
        best = score;
      if (score < beta)
        beta = score;
    }
    if (alpha >= beta)
      break;
  }
  return best;
}

static uint16_t encodeMove(const Move &m) {
  int from = m.from.y * 8 + m.from.x;
  int to = m.to.y * 8 + m.to.x;
  return static_cast<uint16_t>(from | (to << 6) | (m.promotion << 12));
}

static Move decodeMove(uint16_t code) {
  int from = code & 63;
  int to = (code >> 6) & 63;
  return {{from % 8, from / 8}, {to % 8, to / 8}, code >> 12};
}

static const int MATE_THRESHOLD = 900000;

static int scoreToTT(int score, int maxDepth) {
  if (score >= MATE_THRESHOLD)
    return score - maxDepth;
  if (score <= -MATE_THRESHOLD)
    return score + maxDepth;
  return score;
}

static int scoreFromTT(int score, int maxDepth) {
  if (score >= MATE_THRESHOLD)
    return score + maxDepth;
  if (score <= -MATE_THRESHOLD)
    return score - maxDepth;
  return score;
}

static void moveToFront(std::vector<Move> &moves, const Move &target) {
  for (size_t i = 0; i < moves.size(); ++i) {
    if (moves[i] == target) {
      std::swap(moves[0], moves[i]);
      return;
    }
  }
}

int minimaxScore(GameState state, int maxDepth, int currDepth,
                 int alpha = -1000000, int beta = 1000000) {
  ++nodeCount;
  // check if this position is in our TranspositionTable
  if (isRepetition(state.hash, state.halfmoveClock)) {
    return 0;
  }

  if (state.halfmoveClock >= 100 && !isCheckmated(state, state.sideToMove)) {
    return 0;
  }
  int remainingDepth = maxDepth - currDepth;
  Entry *probe_result = zobrist_TT.probe(state.hash);
  if (probe_result && probe_result->depth >= remainingDepth) {
    int ttScore = scoreFromTT(probe_result->score, remainingDepth);
    // Check bound-type
    if (probe_result->bound_type == UPPER) {
      if (ttScore < beta) {
        beta = ttScore;
      }
    } else if (probe_result->bound_type == LOWER) {
      if (ttScore > alpha) {
        alpha = ttScore;
      }
    } else if (probe_result->bound_type == EXACT) {
      return ttScore;
    }
    if (alpha >= beta) {
      return ttScore;
    }
  }

  if (remainingDepth == 0) {
    // base case
    int score = evaluateScore(state, BLACK);
    // consider if any standing moves are blunders
    score = quiescenceSearch(state, alpha, beta);
    return score;
  }
  std::vector<Move> moves = gatherMoves(state, state.sideToMove);

  if (probe_result != nullptr) {
    Move ttMove = decodeMove(probe_result->bestMove);
    for (Move &move : moves) {
      if (move == ttMove) {
        move.ranking = INT_MAX;
      }
    }
  }
  if (moves.empty()) {
    if (isInCheck(state, state.sideToMove)) {
      // If White is mated, return a very high number (but slightly lower if
      // it takes longer) If Black is mated, return a very low number (but
      // slightly higher if it takes longer)
      return (state.sideToMove == WHITE) ? 1000000 + remainingDepth
                                         : -1000000 - remainingDepth;
    } else {
      return 0;
    }
  }
  int score = (state.sideToMove == BLACK) ? -10000000 : 10000000;
  Entry entry;
  Move bestMove;
  int alphaOriginal = alpha;
  int betaOriginal = beta;
  for (size_t i = 0; i < moves.size(); i++) {
    selectNextMove(moves, i);
    Move move = moves[i];
    if (std::chrono::steady_clock::now() > deadline) {
      stop_search = true;
    }
    if (stop_search) {
      return -100000;
    }
    GameState simulated = simulateMove(state, move);
    positionHistory.push_back(simulated.hash);
    int moveScore =
        minimaxScore(simulated, maxDepth, currDepth + 1, alpha, beta);
    positionHistory.pop_back();
    if (stop_search) {
      return -100000;
    }
    if (state.sideToMove == BLACK) {
      // Black maximizes
      if (moveScore > alpha) {
        alpha = moveScore;
      }
      if (moveScore > score) {
        score = moveScore;
        bestMove = move;
      }
      if (alpha >= beta) {
        break;
      }
    } else if (state.sideToMove == WHITE) {
      // White minimizes
      if (moveScore < beta) {
        beta = moveScore;
      }
      if (moveScore < score) {
        score = moveScore;
        bestMove = move;
      }
      if (beta <= alpha) {
        break;
      }
    }
  }
  if (score <= alphaOriginal) {
    entry.bound_type = UPPER;
  } else if (score >= betaOriginal) {
    entry.bound_type = LOWER;
  } else {
    entry.bound_type = EXACT;
  }
  entry.depth = remainingDepth;
  entry.score = scoreToTT(score, remainingDepth);
  entry.hash = state.hash;
  entry.bestMove = encodeMove(bestMove);
  zobrist_TT.store(entry);
  return score; // score of this move
}

evaluatedMove rootSearch(GameState state, int maxDepth,
                         Move lastBest = {{-1, -1}, {-1, -1}}) {
  std::vector<Move> moves = gatherMoves(state, state.sideToMove);
  moveToFront(moves, lastBest);
  evaluatedMove bestMove{{{-1, -1}, {-1, -1}},
                         (state.sideToMove == BLACK) ? -10000000 : 10000000};
  int alpha = -10000000;
  int beta = 10000000;
  for (Move move : moves) {
    GameState simulated = simulateMove(state, move);
    positionHistory.push_back(simulated.hash);
    int score = minimaxScore(simulated, maxDepth, 1, alpha, beta);
    positionHistory.pop_back();
    if (std::chrono::steady_clock::now() > deadline) {
      stop_search = true;
    }
    if (stop_search) {
      return bestMove; // Technically we shouldn't trust this branch but we
                       // can't return -1,-1
    }
    if (score > bestMove.score && state.sideToMove == BLACK) {
      bestMove = {move, score};
    } else if (score < bestMove.score && state.sideToMove == WHITE) {
      bestMove = {move, score};
    }
    if (state.sideToMove == BLACK && score > alpha) {
      alpha = score;
    } else if (state.sideToMove == WHITE && score < beta) {
      beta = score;
    }
  }
  return bestMove;
};

// rewrite of Minimax algorithm with iterative deepening
static std::string moveToUci(const Move &move) {
  std::string text;
  text += static_cast<char>('a' + move.from.x);
  text += static_cast<char>('8' - move.from.y);
  text += static_cast<char>('a' + move.to.x);
  text += static_cast<char>('8' - move.to.y);

  switch (move.promotion) {
  case W_QUEEN:
  case B_QUEEN:
    text += 'q';
    break;
  case W_ROOK:
  case B_ROOK:
    text += 'r';
    break;
  case W_BISHOP:
  case B_BISHOP:
    text += 'b';
    break;
  case W_KNIGHT:
  case B_KNIGHT:
    text += 'n';
    break;
  }
  return text;
}

static std::string uciScore(int blackScore, int sideToMove, int depth) {
  int score = (sideToMove == BLACK) ? blackScore : -blackScore;

  if (std::abs(score) >= MATE_THRESHOLD) {
    int remainingDepth = std::abs(score) - 1000000;
    int pliesToMate = depth - remainingDepth;
    int movesToMate = (pliesToMate + 1) / 2;
    if (score < 0)
      movesToMate = -movesToMate;
    return "mate " + std::to_string(movesToMate);
  }

  return "cp " + std::to_string(score);
}

evaluatedMove newMinimax(GameState state) {
  evaluatedMove bestMove{{{-1, -1}, {-1, -1}}};
  nodeCount = 0;
  auto start = std::chrono::steady_clock::now();

  for (int d = 1; d < 64; d++) {
    if (stop_search) {
      return bestMove;
    }
    evaluatedMove algoMove = rootSearch(state, d, bestMove);
    if (!stop_search) {
      bestMove = algoMove;

      auto elapsed = std::chrono::steady_clock::now() - start;
      long long ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
              .count();
      long long nps =
          (ms > 0) ? static_cast<long long>(nodeCount) * 1000 / ms : 0;

      std::cout << "info depth " << d << " score "
                << uciScore(bestMove.score, state.sideToMove, d) << " nodes "
                << nodeCount << " nps " << nps << " time " << ms << " pv "
                << moveToUci(bestMove) << std::endl;
    } else if (algoMove.from.x != -1) {
      // not a garbage move
      bestMove = algoMove;
    } else if (bestMove.from.x == -1)
      bestMove = {gatherMoves(state, state.sideToMove)[0],
                  0}; // just return the first legal move if we didn't even have
                      // time to return rootSearch()
  }

  return bestMove;
}
